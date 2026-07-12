// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

// EIP-7610 / EIP-158 interaction: a pre-existing account whose only on-chain
// footprint is storage (nonce=0, balance=0, code=empty, non-empty storage)
// must survive a transaction that merely warms it via EIP-2929 cold→warm
// transition. Warming is not an EIP-158 "touch"; only a genuine touch
// (real balance/nonce/code change) should make an empty account eligible
// for end-of-tx deletion.
//
// These tests document evmone's behavior; the scenarios are only reachable
// on pre-Paris forks because EIP-7523 ("Empty Accounts Deprecation") forbids
// an empty-per-EIP-158 account in the pre-state from Paris onward (the
// `validate_state` helper enforces this at PARIS+). Mainnet trie state may
// still contain legacy storage-only accounts predating Spurious Dragon, so
// the behavior these tests pin down would matter for any client running on
// pre-Paris history (e.g. a re-execution of pre-Merge blocks). On Amsterdam
// and other v7.1.x-relevant forks the precondition can't legally exist in a
// spec-compliant block, so this bug class is unreachable through any
// post-EIP-7523 fixture.
//
// Two non-CREATE-collision paths and one rolled-back genuine-touch path are
// covered here; each one used to delete the storage-only account from the
// post-state before the placeholder-aware rollback below.

#include "../utils/bytecode.hpp"
#include "state_transition.hpp"

using namespace evmc::literals;
using namespace evmone::test;

namespace
{
constexpr auto STORAGE_ONLY = 0x5a_address;
}  // namespace

TEST_F(state_transition, eip7610_access_list_no_touch_preserves_storage_only_account)
{
    // EIP-2930 access list warms STORAGE_ONLY at tx start, but the tx does not
    // otherwise touch it. The account must survive into the post-state with
    // its storage intact.
    //
    // NOTE: this scenario alone does NOT reproduce the deletion bug,
    // because access-list-only entries stay `loaded=false` and `build_diff`
    // filters them out via the early `!m.loaded || !m.exists_in_state` check.
    // The bug requires both the buggy placeholder flag AND a subsequent
    // lazy-load that flips `loaded=true` (see the BALANCE test below). This
    // test guards against a regression where access-list warming starts
    // eagerly loading the account leaf, which would re-expose the bug.
    rev = EVMC_LONDON;  // Pre-PARIS so the pre-state validator accepts an
                        // empty-with-storage account; Berlin+ access lists are
                        // already active.

    tx.to = To;
    tx.access_list = {{STORAGE_ONLY, {}}};

    pre[*tx.to] = {.code = bytecode{OP_STOP}};
    pre[STORAGE_ONLY] = {.storage = {{0x01_bytes32, 0x01_bytes32}}};

    expect.post[*tx.to].exists = true;
    expect.post[STORAGE_ONLY].exists = true;
    expect.post[STORAGE_ONLY].storage[0x01_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, eip7610_balance_opcode_preserves_storage_only_account)
{
    // BALANCE(STORAGE_ONLY) goes through Host::access_account (cold) plus a
    // genuine state-view fetch for the balance. The fetch lazy-loads the
    // placeholder, which historically inherited erase_if_empty=true; with that
    // flag set, build_diff would emit STORAGE_ONLY into deleted_accounts at
    // end-of-tx even though semantically nothing was touched.
    rev = EVMC_LONDON;

    tx.to = To;
    pre[*tx.to] = {.code = bytecode{} + push(STORAGE_ONLY) + OP_BALANCE + OP_POP + OP_STOP};
    pre[STORAGE_ONLY] = {.storage = {{0x01_bytes32, 0x01_bytes32}}};

    expect.post[*tx.to].exists = true;
    expect.post[STORAGE_ONLY].exists = true;
    expect.post[STORAGE_ONLY].storage[0x01_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, eip7610_touch_then_revert_preserves_storage_only_account)
{
    // A genuine touch (value transfer) is performed against STORAGE_ONLY and
    // then the outer frame REVERTs. The journal must roll back both the
    // balance change AND the touched flag, so STORAGE_ONLY survives with the
    // original storage. This isolates the bug from EIP-7610 path because the
    // failure mode here is purely about the access-warming placeholder, not
    // about the EIP-6780/-7610 CREATE-collision branch.
    rev = EVMC_LONDON;
    block.base_fee = 0;  // simplify gas accounting

    tx.to = To;
    pre[*tx.to] = {
        .balance = 10, .code = call(STORAGE_ONLY).value(1).gas(0xffff) + revert(0, 0)};
    pre[STORAGE_ONLY] = {.storage = {{0x01_bytes32, 0x01_bytes32}}};

    expect.status = EVMC_REVERT;
    expect.post[*tx.to].exists = true;
    expect.post[*tx.to].balance = 10;  // revert undid the value transfer
    expect.post[STORAGE_ONLY].exists = true;
    expect.post[STORAGE_ONLY].balance = 0;  // revert undid the credit
    expect.post[STORAGE_ONLY].storage[0x01_bytes32] = 0x01_bytes32;
}

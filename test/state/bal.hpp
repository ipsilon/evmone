// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "hash_utils.hpp"
#include "state_diff.hpp"
#include "state_view.hpp"
#include <intx/intx.hpp>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace evmone::state
{
using intx::uint256;

/// EIP-7928 BAL `BlockAccessIndex` conventions:
/// - 0:     pre-block accesses (system_call_block_start)
/// - i + 1: per-tx (1-based — i is the 0-based tx position in the block)
/// - N + 1: post-block accesses (system_call_block_end and finalize both
///          share this index; their per-slot writes are deduped at build)
namespace bal_tx_index
{
constexpr uint32_t PRE_BLOCK = 0;
constexpr uint32_t tx(size_t position) noexcept
{
    return static_cast<uint32_t>(position + 1);
}
constexpr uint32_t post_block(size_t num_txs) noexcept
{
    return static_cast<uint32_t>(num_txs + 1);
}
}  // namespace bal_tx_index

/// EIP-7928 Block-Level Access List.
///
/// Aggregates per-block state accesses: storage writes (per tx), storage reads
/// (block-aggregate), balance/nonce/code changes per tx. Encoded as RLP and
/// committed via keccak256 to the block header's blockAccessListHash.
struct BlockAccessList
{
    /// Per-item cost used for the gas-limit check (EIP-7928).
    static constexpr uint64_t GAS_PER_ITEM = 2000;

    struct StorageWrite
    {
        uint32_t tx_index;
        bytes32 value_after;
    };

    struct SlotWrites
    {
        bytes32 slot;
        std::vector<StorageWrite> accesses;  ///< sorted by tx_index, unique
    };

    struct BalanceChange
    {
        uint32_t tx_index;
        uint256 balance;
    };

    struct NonceChange
    {
        uint32_t tx_index;
        uint64_t nonce;
    };

    struct CodeChange
    {
        uint32_t tx_index;
        bytes code;
    };

    struct AccountAccess
    {
        address addr;
        std::vector<SlotWrites> storage_changes;  ///< sorted by slot key
        std::vector<bytes32> storage_reads;       ///< sorted, disjoint from writes
        std::vector<BalanceChange> balance_changes;
        std::vector<NonceChange> nonce_changes;
        std::vector<CodeChange> code_changes;
    };

    std::vector<AccountAccess> accounts;  ///< sorted by addr, unique

    /// RLP-encode the BAL per EIP-7928.
    [[nodiscard]] bytes rlp_encode() const;

    /// keccak256(rlp_encode()).
    [[nodiscard]] hash256 hash() const;

#ifndef NDEBUG
    /// Debug dump: human-readable representation of the BAL. Available in
    /// debug builds only.
    [[nodiscard]] std::string dump() const;
#endif

    /// Returns true iff item count exceeds block_gas_limit / GAS_PER_ITEM.
    /// Items = number of accounts + sum over accounts of unique slots (reads ∪ writes).
    [[nodiscard]] bool exceeds_gas_limit(uint64_t block_gas_limit) const noexcept;
};

/// Aggregates per-tx state diffs and access callbacks into a BlockAccessList.
class BalBuilder
{
    struct AccountData
    {
        std::unordered_map<bytes32, std::vector<BlockAccessList::StorageWrite>> storage_writes;
        std::unordered_set<bytes32> storage_reads;
        std::vector<BlockAccessList::BalanceChange> balance_changes;
        std::vector<BlockAccessList::NonceChange> nonce_changes;
        std::vector<BlockAccessList::CodeChange> code_changes;
    };
    std::unordered_map<address, AccountData> m_accounts;

public:
    /// Record a cold account access (called by the StateView decorator).
    /// Ensures the account appears in the final BAL even if it has no reads/writes.
    void on_account_read(const address& addr);

    /// Record a cold storage slot access (called by the StateView decorator).
    /// Thanks to caching inside the execution State, each (addr, key) fires at
    /// most once per tx; dedup across txs happens here via unordered_set.
    void on_storage_read(const address& addr, const bytes32& key);

    /// Record the state changes produced by a tx (or system call) at @p tx_index.
    /// @p pre_state must reflect the state BEFORE @p diff is applied.
    void record_diff(uint32_t tx_index, const StateDiff& diff, const StateView& pre_state);

    /// Assemble the final BlockAccessList with all required sort/dedup
    /// invariants. Consumes the builder's per-tx vectors; do not re-use
    /// the builder after calling.
    [[nodiscard]] BlockAccessList build();
};

/// Transparent StateView decorator that forwards to an inner StateView while
/// reporting every cold account/storage-slot access to the given BalBuilder.
class BalStateView final : public StateView
{
    const StateView& m_inner;
    BalBuilder& m_builder;

public:
    BalStateView(const StateView& inner, BalBuilder& builder) noexcept
      : m_inner{inner}, m_builder{builder}
    {}

    std::optional<Account> get_account(const address& addr) const noexcept override
    {
        m_builder.on_account_read(addr);
        return m_inner.get_account(addr);
    }

    /// Intentionally non-recording. Two access patterns exist:
    ///
    /// 1. EVM code-read paths (EXTCODECOPY/EXTCODESIZE/CALL-to-contract,
    ///    delegation lookup): always preceded by `State::find` →
    ///    `m_initial.get_account()` (state.cpp:348,362), so the address is
    ///    already recorded by the time code is fetched.
    ///
    /// 2. System-contract dispatch (system_contracts.cpp:106, 139):
    ///    deliberately probes code FIRST and only records the address
    ///    (via explicit `state_view.get_account(addr)`) if the system
    ///    contract is non-empty. Recording here would surface empty
    ///    system contracts that don't actually execute.
    ///
    /// Adding `on_account_read` here would over-include in case 2.
    bytes get_account_code(const address& addr) const noexcept override
    {
        return m_inner.get_account_code(addr);
    }

    bytes32 get_storage(const address& addr, const bytes32& key) const noexcept override
    {
        m_builder.on_storage_read(addr, key);
        return m_inner.get_storage(addr, key);
    }
};
}  // namespace evmone::state

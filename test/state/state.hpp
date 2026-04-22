// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2022 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "account.hpp"
#include "block.hpp"
#include "bloom_filter.hpp"
#include "errors.hpp"
#include "hash_utils.hpp"
#include "state_diff.hpp"
#include "state_view.hpp"
#include "transaction.hpp"
#include <variant>

namespace evmone::state
{
/// The Ethereum State: the collection of accounts mapped by their addresses.
class State
{
    struct JournalBase
    {
        address addr;
    };

    struct JournalBalanceChange : JournalBase
    {
        intx::uint256 prev_balance;
    };

    struct JournalTouched : JournalBase
    {};

    struct JournalStorageChange : JournalBase
    {
        bytes32 key;
        bytes32 prev_value;
        evmc_access_status prev_access_status;
    };

    struct JournalTransientStorageChange : JournalBase
    {
        bytes32 key;
        bytes32 prev_value;
    };

    struct JournalNonceBump : JournalBase
    {};

    struct JournalCreate : JournalBase
    {
        bool existed;
    };

    struct JournalDestruct : JournalBase
    {};

    struct JournalAccessAccount : JournalBase
    {};

    using JournalEntry =
        std::variant<JournalBalanceChange, JournalTouched, JournalStorageChange, JournalNonceBump,
            JournalCreate, JournalTransientStorageChange, JournalDestruct, JournalAccessAccount>;

    /// The read-only view of the initial (cold) state.
    const StateView& m_initial;

    /// The accounts loaded from the initial state and potentially modified.
    std::unordered_map<address, Account> m_modified;

    /// The state journal: the list of changes made to the state
    /// with information how to revert them.
    std::vector<JournalEntry> m_journal;

public:
    explicit State(const StateView& state_view) noexcept : m_initial{state_view} {}
    State(const State&) = delete;
    State(State&&) = delete;
    State& operator=(State&&) = delete;

    /// Inserts the new account at the address.
    /// There must not exist any account under this address before.
    Account& insert(const address& addr, Account account = {});

    /// Returns the pointer to the account at the address if the account exists. Null otherwise.
    Account* find(const address& addr) noexcept;

    /// Gets the account at the address (the account must exist).
    Account& get(const address& addr) noexcept;

    /// Gets an existing account or inserts new account.
    Account& get_or_insert(const address& addr, Account account = {});

    bytes_view get_code(const address& addr);

    StorageValue& get_storage(const address& addr, const bytes32& key);

    StateDiff build_diff(evmc_revision rev) const;

    /// Returns the state journal checkpoint. It can be later used to in rollback()
    /// to revert changes newer than the checkpoint.
    [[nodiscard]] size_t checkpoint() const noexcept { return m_journal.size(); }

    /// Reverts state changes made after the checkpoint.
    void rollback(size_t checkpoint);

    /// Methods performing changes to the state which can be reverted by rollback().
    /// @{

    /// Touches (as in EIP-161) an existing account or inserts new erasable account.
    Account& touch(const address& addr);

    void journal_balance_change(const address& addr, const intx::uint256& prev_balance);

    void journal_storage_change(const address& addr, const bytes32& key, const StorageValue& value);

    void journal_transient_storage_change(
        const address& addr, const bytes32& key, const bytes32& value);

    void journal_bump_nonce(const address& addr);

    void journal_create(const address& addr, bool existed);

    void journal_destruct(const address& addr);

    void journal_access_account(const address& addr);

    /// @}
};

/// Finalize state after applying a "block" of transactions.
///
/// Applies block reward to coinbase, withdrawals (post Shanghai) and deletes empty touched accounts
/// (post Spurious Dragon).
[[nodiscard]] StateDiff finalize(const StateView& state_view, evmc_revision rev,
    const address& coinbase, std::optional<uint64_t> block_reward, std::span<const Ommer> ommers,
    std::span<const Withdrawal> withdrawals);

/// Executes a valid transaction.
///
/// @return Transaction receipt with state diff.
TransactionReceipt transition(const StateView& state, const BlockInfo& block,
    const BlockHashes& block_hashes, const Transaction& tx, evmc_revision rev, evmc::VM& vm,
    const TransactionProperties& tx_props);

/// Validate a transaction.
///
/// @return Computed execution gas limit or validation error.
[[nodiscard]] std::variant<TransactionProperties, std::error_code> validate_transaction(
    const StateView& state_view, const BlockInfo& block, const Transaction& tx, evmc_revision rev,
    int64_t block_gas_left, int64_t blob_gas_left) noexcept;

/// Compute the transaction signing hash, dispatching on tx.type per EIP-2718.
[[nodiscard]] evmc::bytes32 compute_tx_signing_hash(const Transaction& tx) noexcept;

/// Compute the EIP-7702 authorization signing hash: keccak256(0x05 || rlp(chain_id, addr, nonce)).
[[nodiscard]] evmc::bytes32 compute_authorization_signing_hash(const Authorization& auth) noexcept;

/// Recover the sender address from a signed transaction.
/// Performs full signature validation: r/s in (0, secp256k1n), s <= secp256k1n/2 (EIP-2),
/// and y_parity within the range valid for tx.type. Returns std::nullopt if the signature
/// is malformed or ecrecover yields the point at infinity.
[[nodiscard]] std::optional<evmc::address> recover_sender(const Transaction& tx) noexcept;

/// Recover the signer address from a signed EIP-7702 authorization tuple. Performs the same
/// validation as recover_sender and additionally enforces y_parity in {0, 1}.
[[nodiscard]] std::optional<evmc::address> recover_authorization_signer(
    const Authorization& auth) noexcept;
}  // namespace evmone::state

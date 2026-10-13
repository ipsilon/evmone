// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "hash_utils.hpp"
#include "state_diff.hpp"
#include "state_view.hpp"

namespace evmone::state
{
/// The block-level access list (EIP-7928): the storage writes and the balance, nonce and code
/// changes per block access index (0 for the block-start system calls, i+1 for the transaction i,
/// n+1 for the withdrawals and the block-end system calls) and the storage reads of the block.
/// The keccak256 hash of its RLP encoding is the header's blockAccessListHash.
struct BlockAccessList
{
    /// The gas the block gas limit must cover per item (EIP-7928).
    static constexpr uint64_t GAS_PER_ITEM = 2000;

    /// A value set at a block access index.
    template <typename T>
    struct Change
    {
        uint32_t index = 0;
        T value;
    };

    struct SlotChanges
    {
        uint256 slot;
        std::vector<Change<uint256>> changes;  ///< sorted by index, unique
    };

    struct AccountChanges
    {
        address addr;
        std::vector<SlotChanges> storage_changes;  ///< sorted by slot
        std::vector<uint256> storage_reads;        ///< sorted, disjoint from the writes
        std::vector<Change<uint256>> balance_changes;
        std::vector<Change<uint64_t>> nonce_changes;
        std::vector<Change<bytes>> code_changes;
    };

    std::vector<AccountChanges> accounts;  ///< sorted by addr, unique

    [[nodiscard]] bytes rlp_encode() const;

    [[nodiscard]] hash256 hash() const;

    /// Whether the items (the accounts and their unique storage slots) exceed
    /// block_gas_limit / GAS_PER_ITEM.
    [[nodiscard]] bool exceeds_gas_limit(uint64_t block_gas_limit) const noexcept;
};

/// Builds the BlockAccessList from the state diffs of a block.
class BalBuilder
{
    struct AccountData
    {
        std::unordered_map<bytes32, std::vector<BlockAccessList::Change<uint256>>> storage_writes;
        std::vector<bytes32> storage_reads;  ///< With duplicates; build() sorts and deduplicates.
        std::vector<BlockAccessList::Change<uint256>> balance_changes;
        std::vector<BlockAccessList::Change<uint64_t>> nonce_changes;
        std::vector<BlockAccessList::Change<bytes>> code_changes;

        /// The account before the block, fetched the first time a diff lists it.
        std::optional<StateView::Account> initial;
    };
    std::unordered_map<address, AccountData> m_accounts;

public:
    /// Records the reads and the state changes of a transaction or a system call at @p index.
    /// A change is against the one recorded last or, if none, against @p pre_block_state,
    /// so every diff applied to the block state must be recorded, in order.
    void record_diff(uint32_t index, const StateDiff& diff, const StateView& pre_block_state);

    /// Sorts the records into the BlockAccessList, leaving the builder empty.
    [[nodiscard]] BlockAccessList build();
};
}  // namespace evmone::state

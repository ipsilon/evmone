// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "bal.hpp"
#include "../utils/rlp.hpp"
#include "account.hpp"

namespace evmone::state
{
// The RLP encoders of the BlockAccessList parts. They are static, not in an unnamed namespace,
// because rlp::encode() finds them through ADL.

template <typename T>
static bytes rlp_encode(const BlockAccessList::Change<T>& c)
{
    return rlp::encode_tuple(uint64_t{c.index}, c.value);
}

static bytes rlp_encode(const BlockAccessList::SlotChanges& s)
{
    return rlp::encode_tuple(s.slot, s.changes);
}

static bytes rlp_encode(const BlockAccessList::AccountChanges& a)
{
    return rlp::encode_tuple(a.addr, a.storage_changes, a.storage_reads, a.balance_changes,
        a.nonce_changes, a.code_changes);
}

bytes BlockAccessList::rlp_encode() const
{
    // TODO: rlp::encode() asserts lengths below 16 MiB (3 length bytes). The storage reads exceed
    //   it from a block gas limit of about 1.1G (one item per GAS_PER_ITEM gas).
    return rlp::encode(accounts);
}

hash256 BlockAccessList::hash() const
{
    return keccak256(rlp_encode());
}

bool BlockAccessList::exceeds_gas_limit(uint64_t block_gas_limit) const noexcept
{
    uint64_t items = 0;
    for (const auto& acc : accounts)  // The reads are disjoint from the writes.
        items += 1 + acc.storage_changes.size() + acc.storage_reads.size();
    return items > block_gas_limit / GAS_PER_ITEM;
}

namespace
{
/// Records the value at the index unless it is the one the index started with: the last recorded
/// or the initial one. The last index gets two diffs (the withdrawals and the block-end system
/// calls), so a change already recorded at the index is replaced.
template <typename T>
void record(std::vector<BlockAccessList::Change<T>>& changes, uint32_t index, const T& value,
    const T& initial)
{
    if (!changes.empty() && changes.back().index == index)
        changes.pop_back();
    assert(changes.empty() || changes.back().index < index);
    if (value != (changes.empty() ? initial : changes.back().value))
        changes.push_back({index, value});
}
}  // namespace

void BalBuilder::record_diff(
    uint32_t index, const StateDiff& diff, const StateView& pre_block_state)
{
    for (const auto& [addr, keys] : diff.reads)
    {
        // A read account is in the BAL even without changes.
        auto& reads = m_accounts[addr].storage_reads;
        reads.insert(reads.end(), keys.begin(), keys.end());
    }

    const auto listed = [&](const address& addr) -> AccountData& {
        auto& ad = m_accounts[addr];
        if (!ad.initial.has_value())  // A missing account reads as an empty one.
        {
            ad.initial = pre_block_state.get_account(addr).value_or(
                StateView::Account{.code_hash = Account::EMPTY_CODE_HASH});
        }
        return ad;
    };

    for (const auto& entry : diff.modified_accounts)
    {
        auto& ad = listed(entry.addr);
        record(ad.nonce_changes, index, entry.nonce, ad.initial->nonce);
        record(ad.balance_changes, index, entry.balance, ad.initial->balance);
        // The code may be set to the same bytes, e.g. by a pair of self-cancelling
        // authorizations in a transaction (EIP-7702).
        if (entry.code.has_value())
        {
            record(
                ad.code_changes, index, *entry.code, pre_block_state.get_account_code(entry.addr));
        }
        for (const auto& [slot, value] : entry.modified_storage)
        {
            auto& writes = ad.storage_writes[slot];  // A diff lists only the changed slots.
            assert(writes.empty() || writes.back().index < index);
            writes.push_back({index, intx::be::load<uint256>(value)});
        }
    }

    for (const auto& addr : diff.deleted_accounts)
    {
        auto& ad = listed(addr);
        // Only a created or an empty account is deleted (EIP-161, EIP-6780).
        assert(ad.initial->nonce == 0 && ad.initial->code_hash == Account::EMPTY_CODE_HASH);
        record(ad.balance_changes, index, uint256{0}, ad.initial->balance);
    }
}

BlockAccessList BalBuilder::build()
{
    BlockAccessList bal;
    bal.accounts.reserve(m_accounts.size());
    for (auto& [addr, data] : m_accounts)
    {
        auto& acc = bal.accounts.emplace_back();
        acc.addr = addr;

        acc.storage_changes.reserve(data.storage_writes.size());
        for (auto& [slot, writes] : data.storage_writes)
            acc.storage_changes.push_back({intx::be::load<uint256>(slot), std::move(writes)});
        std::ranges::sort(acc.storage_changes, {}, &BlockAccessList::SlotChanges::slot);

        for (const auto& slot : data.storage_reads)
        {
            if (!data.storage_writes.contains(slot))  // A written slot is not a read.
                acc.storage_reads.push_back(intx::be::load<uint256>(slot));
        }
        std::ranges::sort(acc.storage_reads);
        acc.storage_reads.erase(
            std::ranges::unique(acc.storage_reads).begin(), acc.storage_reads.end());

        acc.balance_changes = std::move(data.balance_changes);
        acc.nonce_changes = std::move(data.nonce_changes);
        acc.code_changes = std::move(data.code_changes);
    }
    std::ranges::sort(bal.accounts, {}, &BlockAccessList::AccountChanges::addr);
    m_accounts.clear();
    return bal;
}
}  // namespace evmone::state

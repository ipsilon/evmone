// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "bal.hpp"
#include "account.hpp"
#include <evmc/hex.hpp>
#include <test/utils/rlp.hpp>
#include <algorithm>
#include <sstream>

namespace evmone::state
{
// ADL-discoverable RLP encoders for BAL sub-structs.
// `rlp::encode(std::vector<T>)` finds these in the `evmone::state` namespace
// (where the types live) via argument-dependent lookup.

inline bytes rlp_encode(const BlockAccessList::StorageWrite& w)
{
    return rlp::encode_tuple(uint64_t{w.tx_index}, intx::be::load<uint256>(w.value_after));
}

inline bytes rlp_encode(const BlockAccessList::SlotWrites& sw)
{
    return rlp::encode_tuple(intx::be::load<uint256>(sw.slot), sw.accesses);
}

inline bytes rlp_encode(const BlockAccessList::BalanceChange& b)
{
    return rlp::encode_tuple(uint64_t{b.tx_index}, b.balance);
}

inline bytes rlp_encode(const BlockAccessList::NonceChange& n)
{
    return rlp::encode_tuple(uint64_t{n.tx_index}, n.nonce);
}

inline bytes rlp_encode(const BlockAccessList::CodeChange& c)
{
    return rlp::encode_tuple(uint64_t{c.tx_index}, bytes_view{c.code.data(), c.code.size()});
}

inline bytes rlp_encode(const BlockAccessList::AccountAccess& a)
{
    // Storage reads encode as a list of trimmed uint256 (per geth's
    // EncodedStorage), not fixed-width bytes32. Materialize the list as
    // a vector<uint256> so encode_tuple's encode(vector<T>) finds the
    // right encoder via rlp::encode(intx::uint256) (which trims).
    std::vector<uint256> reads;
    reads.reserve(a.storage_reads.size());
    for (const auto& r : a.storage_reads)
        reads.push_back(intx::be::load<uint256>(r));

    return rlp::encode_tuple(bytes_view{a.addr.bytes, sizeof(a.addr.bytes)},
        a.storage_changes, reads, a.balance_changes, a.nonce_changes, a.code_changes);
}

bytes BlockAccessList::rlp_encode() const
{
    return rlp::encode(accounts);
}

hash256 BlockAccessList::hash() const
{
    const auto enc = rlp_encode();
    return keccak256(bytes_view{enc.data(), enc.size()});
}

#ifndef NDEBUG
std::string BlockAccessList::dump() const
{
    std::ostringstream os;
    os << "[\n";
    for (const auto& a : accounts)
    {
        os << "  addr=" << evmc::hex({a.addr.bytes, sizeof(a.addr.bytes)}) << "\n";
        if (!a.storage_changes.empty())
        {
            os << "    storage_changes:\n";
            for (const auto& sw : a.storage_changes)
            {
                os << "      slot=" << evmc::hex({sw.slot.bytes, sizeof(sw.slot.bytes)})
                   << " writes:";
                for (const auto& w : sw.accesses)
                    os << " [tx=" << w.tx_index
                       << " val=" << evmc::hex({w.value_after.bytes, sizeof(w.value_after.bytes)})
                       << "]";
                os << "\n";
            }
        }
        if (!a.storage_reads.empty())
        {
            os << "    storage_reads:";
            for (const auto& r : a.storage_reads)
                os << " " << evmc::hex({r.bytes, sizeof(r.bytes)});
            os << "\n";
        }
        if (!a.balance_changes.empty())
        {
            os << "    balance:";
            for (const auto& b : a.balance_changes)
                os << " [tx=" << b.tx_index << " " << intx::hex(b.balance) << "]";
            os << "\n";
        }
        if (!a.nonce_changes.empty())
        {
            os << "    nonce:";
            for (const auto& n : a.nonce_changes)
                os << " [tx=" << n.tx_index << " " << n.nonce << "]";
            os << "\n";
        }
        if (!a.code_changes.empty())
        {
            os << "    code:";
            for (const auto& c : a.code_changes)
                os << " [tx=" << c.tx_index << " " << evmc::hex({c.code.data(), c.code.size()})
                   << "]";
            os << "\n";
        }
    }
    os << "]";
    return os.str();
}
#endif  // NDEBUG

bool BlockAccessList::exceeds_gas_limit(uint64_t block_gas_limit) const noexcept
{
    uint64_t items = 0;
    for (const auto& acc : accounts)
    {
        // BalBuilder::build() guarantees storage_changes and storage_reads
        // are disjoint, so they can be summed directly with no dedup.
        items += 1 + acc.storage_changes.size() + acc.storage_reads.size();
    }
    return items > block_gas_limit / GAS_PER_ITEM;
}

void BalBuilder::on_account_read(const address& addr)
{
    m_accounts[addr];  // Ensure the account appears in the final BAL.
}

void BalBuilder::on_storage_read(const address& addr, const bytes32& key)
{
    m_accounts[addr].storage_reads.insert(key);
}

/// Append a change entry. EIP-7928 requires each BlockAccessIndex to appear at
/// most once per change list. `record_diff` is called twice at `tx_idx_post`
/// (system_call_block_end + finalize), but current Amsterdam system contracts
/// (EIP-7002, EIP-7251) only modify their own storage and never `CALL` or
/// transfer value, while finalize only modifies withdrawal recipient balances —
/// disjoint `(account, list)` pairs, so no collision occurs. The assertion
/// catches any future contract that breaks this invariant.
template <typename V, typename T>
static void append_unique(V& list, uint32_t tx_index, T&& value)
{
    assert(list.empty() || list.back().tx_index != tx_index);
    list.push_back({tx_index, std::forward<T>(value)});
}

void BalBuilder::record_diff(
    uint32_t tx_index, const StateDiff& diff, const StateView& pre_state)
{
    for (const auto& entry : diff.modified_accounts)
    {
        auto& ad = m_accounts[entry.addr];
        const auto pre = pre_state.get_account(entry.addr);
        const auto pre_nonce = pre ? pre->nonce : uint64_t{0};
        const auto pre_balance = pre ? pre->balance : uint256{0};

        if (entry.nonce != pre_nonce)
            append_unique(ad.nonce_changes, tx_index, entry.nonce);
        if (entry.balance != pre_balance)
            append_unique(ad.balance_changes, tx_index, entry.balance);
        if (entry.code.has_value())
        {
            // Record only an actual code change: EIP-7702 authorizations can
            // leave code_changed==true with the same final bytes (e.g. a pair
            // of self-canceling auths in the same tx).
            const auto pre_code = pre_state.get_account_code(entry.addr);
            if (*entry.code != pre_code)
                append_unique(ad.code_changes, tx_index, *entry.code);
        }

        for (const auto& [slot, new_val] : entry.modified_storage)
            append_unique(ad.storage_writes[slot], tx_index, new_val);
    }

    for (const auto& addr : diff.deleted_accounts)
    {
        auto& ad = m_accounts[addr];
        const auto pre = pre_state.get_account(addr);
        if (!pre)
            continue;  // Nothing to record if the account didn't exist pre-diff.
        // The nonce/code comparisons are structurally dead at Amsterdam
        // (post-EIP-6780 only same-tx-created accounts self-destruct, and their
        // pre-state is absent; EIP-158-cleared accounts are empty by definition;
        // EIP-8246 keeps destructed-with-balance accounts as modified). Kept for
        // shape parity with EELS, which carries the same dead branches.
        if (pre->nonce != 0)
            append_unique(ad.nonce_changes, tx_index, uint64_t{0});
        if (pre->balance != 0)
            append_unique(ad.balance_changes, tx_index, uint256{0});
        if (pre->code_hash != Account::EMPTY_CODE_HASH)
            append_unique(ad.code_changes, tx_index, bytes{});
    }
}

BlockAccessList BalBuilder::build()
{
    BlockAccessList bal;
    bal.accounts.reserve(m_accounts.size());

    for (auto& [addr, data] : m_accounts)
    {
        BlockAccessList::AccountAccess acc;
        acc.addr = addr;

        // Storage writes: flatten the slot map and sort by slot.
        acc.storage_changes.reserve(data.storage_writes.size());
        for (auto& [slot, writes] : data.storage_writes)
            acc.storage_changes.push_back({slot, std::move(writes)});
        std::sort(acc.storage_changes.begin(), acc.storage_changes.end(),
            [](const auto& a, const auto& b) { return a.slot < b.slot; });

        // Storage reads: drop any slot also present in writes, then sort.
        acc.storage_reads.reserve(data.storage_reads.size());
        for (const auto& slot : data.storage_reads)
        {
            if (data.storage_writes.find(slot) == data.storage_writes.end())
                acc.storage_reads.push_back(slot);
        }
        std::sort(acc.storage_reads.begin(), acc.storage_reads.end());

        // Per-tx changes are already appended in ascending tx order.
        acc.balance_changes = std::move(data.balance_changes);
        acc.nonce_changes = std::move(data.nonce_changes);
        acc.code_changes = std::move(data.code_changes);

        bal.accounts.push_back(std::move(acc));
    }

    std::sort(bal.accounts.begin(), bal.accounts.end(),
        [](const auto& a, const auto& b) { return a.addr < b.addr; });

    return bal;
}
}  // namespace evmone::state

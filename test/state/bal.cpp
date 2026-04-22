// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "bal.hpp"
#include "account.hpp"
#include <evmc/hex.hpp>
#include <test/utils/rlp.hpp>
#include <algorithm>
#include <sstream>
#include <unordered_set>

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
    // Storage reads must be encoded as a list of trimmed uint256 (per geth's EncodedStorage),
    // not as fixed-width bytes32, so we build that list manually.
    bytes reads;
    for (const auto& r : a.storage_reads)
        reads += rlp::encode(intx::be::load<uint256>(r));

    bytes content;
    content += rlp::encode(bytes_view{a.addr.bytes, sizeof(a.addr.bytes)});
    content += rlp::encode(a.storage_changes);
    content += rlp::internal::wrap_list(reads);
    content += rlp::encode(a.balance_changes);
    content += rlp::encode(a.nonce_changes);
    content += rlp::encode(a.code_changes);
    return rlp::internal::wrap_list(content);
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

bool BlockAccessList::exceeds_gas_limit(uint64_t block_gas_limit) const noexcept
{
    uint64_t items = 0;
    for (const auto& acc : accounts)
    {
        ++items;  // account itself counts as one item
        std::unordered_set<bytes32> unique_slots;
        for (const auto& sc : acc.storage_changes)
            unique_slots.insert(sc.slot);
        for (const auto& sr : acc.storage_reads)
            unique_slots.insert(sr);
        items += unique_slots.size();
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

void BalBuilder::record_diff(
    uint16_t tx_index, const StateDiff& diff, const StateView& pre_state)
{
    for (const auto& entry : diff.modified_accounts)
    {
        auto& ad = m_accounts[entry.addr];
        const auto pre = pre_state.get_account(entry.addr);
        const auto pre_nonce = pre ? pre->nonce : uint64_t{0};
        const auto pre_balance = pre ? pre->balance : uint256{0};

        if (entry.nonce != pre_nonce)
            ad.nonce_changes.push_back({tx_index, entry.nonce});
        if (entry.balance != pre_balance)
            ad.balance_changes.push_back({tx_index, entry.balance});
        if (entry.code.has_value())
            ad.code_changes.push_back({tx_index, *entry.code});

        for (const auto& [slot, new_val] : entry.modified_storage)
            ad.storage_writes[slot].push_back({tx_index, new_val});
    }

    for (const auto& addr : diff.deleted_accounts)
    {
        auto& ad = m_accounts[addr];
        const auto pre = pre_state.get_account(addr);
        if (!pre)
            continue;  // Nothing to record if the account didn't exist pre-diff.
        if (pre->nonce != 0)
            ad.nonce_changes.push_back({tx_index, 0});
        if (pre->balance != 0)
            ad.balance_changes.push_back({tx_index, 0});
        if (pre->code_hash != Account::EMPTY_CODE_HASH)
            ad.code_changes.push_back({tx_index, bytes{}});
    }
}

BlockAccessList BalBuilder::build() const
{
    BlockAccessList bal;
    bal.accounts.reserve(m_accounts.size());

    for (const auto& [addr, data] : m_accounts)
    {
        BlockAccessList::AccountAccess acc;
        acc.addr = addr;

        // Storage writes: flatten the slot map and sort by slot.
        acc.storage_changes.reserve(data.storage_writes.size());
        for (const auto& [slot, writes] : data.storage_writes)
            acc.storage_changes.push_back({slot, writes});
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
        acc.balance_changes = data.balance_changes;
        acc.nonce_changes = data.nonce_changes;
        acc.code_changes = data.code_changes;

        bal.accounts.push_back(std::move(acc));
    }

    std::sort(bal.accounts.begin(), bal.accounts.end(),
        [](const auto& a, const auto& b) { return a.addr < b.addr; });

    return bal;
}
}  // namespace evmone::state

// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2022 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "state.hpp"
#include "../utils/stdx/utility.hpp"
#include "host.hpp"
#include "precompiles.hpp"
#include "state_view.hpp"
#include <evmone/constants.hpp>
#include <evmone/delegation.hpp>
#include <evmone/instructions_traits.hpp>
#include <evmone/state_gas.hpp>
#include <algorithm>
#include <ranges>

using namespace intx;

namespace evmone::state
{
namespace
{
/// EIP-7702: The cost of authorization that sets delegation to an account that didn't exist before.
constexpr auto AUTHORIZATION_EMPTY_ACCOUNT_COST = 25000;
/// EIP-7702: The cost of authorization that sets delegation to an account that already exists.
constexpr auto AUTHORIZATION_BASE_COST = 12500;

constexpr int64_t num_words(size_t size_in_bytes) noexcept
{
    return static_cast<int64_t>((size_in_bytes + 31) / 32);
}

size_t compute_tx_data_tokens(evmc_revision rev, bytes_view data) noexcept
{
    const auto num_zero_bytes = static_cast<size_t>(std::ranges::count(data, 0));
    const auto num_nonzero_bytes = data.size() - num_zero_bytes;

    const size_t nonzero_byte_multiplier = rev >= EVMC_ISTANBUL ? 4 : 17;
    return (nonzero_byte_multiplier * num_nonzero_bytes) + num_zero_bytes;
}

struct AccessListCounts
{
    size_t num_addresses = 0;
    size_t num_storage_keys = 0;
};

AccessListCounts count_access_list(const AccessList& access_list) noexcept
{
    size_t num_storage_keys = 0;
    for (const auto& keys : access_list | std::views::values)
        num_storage_keys += keys.size();
    return {access_list.size(), num_storage_keys};
}

struct TransactionCost
{
    int64_t intrinsic = 0;
    int64_t min = 0;
};

/// Compute the transaction intrinsic gas 𝑔₀ (Yellow Paper, 6.2) and minimal gas (floor cost).
TransactionCost compute_tx_intrinsic_cost(evmc_revision rev, const Transaction& tx) noexcept
{
    static constexpr auto TX_BASE_COST = 21000;
    static constexpr auto TX_BASE_COST_AMSTERDAM = 12000;
    static constexpr auto TX_VALUE_COST = 6000;
    static constexpr auto TX_CREATE_COST = 32000;
    static constexpr auto ACCESS_LIST_ADDRESS_COST = 2400;
    static constexpr auto ACCESS_LIST_STORAGE_KEY_COST = 1900;
    static constexpr auto ACCESS_LIST_ADDRESS_COST_AMSTERDAM =
        instr::additional_cold_account_access(EVMC_AMSTERDAM);
    static constexpr auto ACCESS_LIST_STORAGE_KEY_COST_AMSTERDAM =
        instr::ADDITIONAL_COLD_STORAGE_ACCESS;
    static constexpr auto ACCESS_LIST_ADDRESS_BYTES = 20;
    static constexpr auto ACCESS_LIST_STORAGE_KEY_BYTES = 32;
    static constexpr auto DATA_TOKEN_COST = 4;
    static constexpr auto INITCODE_WORD_COST = 2;
    static constexpr auto TOTAL_COST_FLOOR_PER_TOKEN = 10;
    static constexpr auto TOTAL_COST_FLOOR_PER_BYTE = 16 * 4;

    // EXECUTION_PER_AUTH_BASE_COST, the execution cost of processing a single authorization:
    // the calldata cost of the 101-byte authorization tuple, the signature recovery, the cold
    // authority access and two warm writes (EIP-8037).
    static constexpr auto AUTHORIZATION_EXECUTION_COST =
        101 * 16 + 3000 + instr::COLD_ACCOUNT_ACCESS_AMSTERDAM + 2 * instr::WARM_ACCESS;

    const auto is_create = !tx.to.has_value();

    const auto recipient_cost = [&] {
        if (rev < EVMC_AMSTERDAM)  // EIP-2780.
            return 0;
        if (is_create)
            return instr::CREATE_ACCESS;       // Cold account access + account write.
        if (*tx.to == tx.sender) [[unlikely]]  // Transaction to self.
            return 0;
        return instr::COLD_ACCOUNT_ACCESS_AMSTERDAM + (tx.value != 0 ? TX_VALUE_COST : 0);
    }();
    const auto base_cost =
        ((rev >= EVMC_AMSTERDAM) ? TX_BASE_COST_AMSTERDAM : TX_BASE_COST) + recipient_cost;

    const auto create_cost =
        (is_create && rev >= EVMC_HOMESTEAD && rev < EVMC_AMSTERDAM) ? TX_CREATE_COST : 0;

    const auto num_tokens = static_cast<int64_t>(compute_tx_data_tokens(rev, tx.data));
    const auto data_cost = num_tokens * DATA_TOKEN_COST;

    const auto [num_addresses, num_storage_keys] = count_access_list(tx.access_list);
    const auto access_list_num_bytes =
        static_cast<int64_t>(num_addresses * ACCESS_LIST_ADDRESS_BYTES +
                             num_storage_keys * ACCESS_LIST_STORAGE_KEY_BYTES);
    const auto address_cost =
        (rev >= EVMC_AMSTERDAM) ? ACCESS_LIST_ADDRESS_COST_AMSTERDAM : ACCESS_LIST_ADDRESS_COST;
    const auto storage_key_cost = (rev >= EVMC_AMSTERDAM) ? ACCESS_LIST_STORAGE_KEY_COST_AMSTERDAM :
                                                            ACCESS_LIST_STORAGE_KEY_COST;
    const auto access_list_cost = static_cast<int64_t>(num_addresses) * address_cost +
                                  static_cast<int64_t>(num_storage_keys) * storage_key_cost;

    const auto auth_cost =
        (rev >= EVMC_AMSTERDAM) ? AUTHORIZATION_EXECUTION_COST : AUTHORIZATION_EMPTY_ACCOUNT_COST;
    const auto auth_list_cost = static_cast<int64_t>(tx.authorization_list.size()) * auth_cost;

    const auto initcode_cost =
        (is_create && rev >= EVMC_SHANGHAI) ? INITCODE_WORD_COST * num_words(tx.data.size()) : 0;

    // Charge a flat cost per access-list byte (EIP-7981).
    const auto access_list_data_cost =
        (rev >= EVMC_AMSTERDAM) ? access_list_num_bytes * TOTAL_COST_FLOOR_PER_BYTE : 0;

    const auto intrinsic_cost = base_cost + create_cost + data_cost + access_list_data_cost +
                                access_list_cost + auth_list_cost + initcode_cost;

    int64_t data_min_cost = 0;
    if (rev >= EVMC_AMSTERDAM)  // Unified cost per byte (EIP-7976).
        data_min_cost = TOTAL_COST_FLOOR_PER_BYTE * static_cast<int64_t>(tx.data.size());
    else if (rev >= EVMC_PRAGUE)  // Cost per token capturing num of zero-nonzero bytes (EIP-7623).
        data_min_cost = TOTAL_COST_FLOOR_PER_TOKEN * num_tokens;

    // Compute the calldata floor cost (EIP-7623), including the access list data (EIP-7981).
    const auto min_cost =
        (rev >= EVMC_PRAGUE) ? base_cost + data_min_cost + access_list_data_cost : 0;

    return {intrinsic_cost, min_cost};
}

/// A revertible applied authorization (EIP-2780).
struct AppliedAuthorization
{
    Account* authority = nullptr;
    std::optional<bytes32> initial_code_hash;
};

/// Applies the authorization list (EIP-7702) and returns the delegation refund. From Amsterdam,
/// charges the state-dependent costs instead of the refund and records the applied authorizations
/// to revert them on halt; returns nullopt when out of gas (EIP-2780).
[[nodiscard]] std::optional<int64_t> process_authorization_list(State& state, const Transaction& tx,
    evmc_revision rev, int64_t& gas_left, StateGas& state_gas,
    std::vector<AppliedAuthorization>& applied)
{
    int64_t delegation_refund = 0;
    for (const auto& auth : tx.authorization_list)
    {
        // 1. Verify the chain id is either 0 or the chain’s current ID.
        if (auth.chain_id != 0 && auth.chain_id != tx.chain_id)
            continue;

        // 2. Verify the nonce is less than 2**64 - 1.
        if (auth.nonce == MAX_NONCE)
            continue;

        // 3. Verify if the authority has been successfully recovered from the signature.
        //    authority = ecrecover(...)
        const auto authority_addr = recover_authority(auth);
        if (!authority_addr.has_value())
            continue;

        // Get or create the authority account.
        // It is still empty at this point until nonce bump following successful authorization.
        auto& authority = state.get_or_insert(*authority_addr);

        // 4. Add authority to accessed_addresses (as defined in EIP-2929.)
        authority.access_status = EVMC_ACCESS_WARM;

        // 5. Verify the code of authority is either empty or already delegated.
        if (authority.code_hash != Account::EMPTY_CODE_HASH &&
            !is_code_delegated(state.get_code(*authority_addr)))
            continue;

        // 6. Verify the nonce of authority is equal to nonce.
        // In case authority does not exist in the trie, verify that nonce is equal to 0.
        if (auth.nonce != authority.nonce)
            continue;

        if (rev >= EVMC_AMSTERDAM)  // The state-dependent costs replace the refund (EIP-2780).
        {
            // TODO: Extract the 23 to global constant. Also use to check delegation (fuzzing).
            static constexpr auto AUTHORIZATION_STATE_GAS = 23 * COST_PER_STATE_BYTE;
            // TODO: Replace the linear search with a "written" account flag
            //   or by processing the authorizations sorted by authority.
            const auto already_applied = std::ranges::find(applied, &authority,
                                             &AppliedAuthorization::authority) != applied.end();
            // The first write to the authority: it is not the sender, not the value recipient
            // (paid by TX_VALUE_COST) and not an already applied authority.
            if (authority_addr != tx.sender && (tx.value == 0 || tx.to != authority_addr) &&
                !already_applied)
            {
                if ((gas_left -= instr::ACCOUNT_WRITE) < 0)
                    return std::nullopt;
            }
            // A new account and a net-new delegation indicator: no code at the transaction start
            // and none set since. An applied authority is not empty because of its nonce bump.
            assert(!authority.is_empty() || !authority.code_changed);
            if (authority.code_hash == Account::EMPTY_CODE_HASH && !authority.code_changed)
            {
                int64_t state_gas_cost = 0;
                // TODO: State knows the nonexistent → existent transition. Record it as a flag.
                if (authority.is_empty())
                    state_gas_cost += NEW_ACCOUNT_STATE_GAS;
                if (!is_zero(auth.addr))
                    state_gas_cost += AUTHORIZATION_STATE_GAS;
                if (!state_gas.charge(gas_left, state_gas_cost))
                    return std::nullopt;
            }
            assert(!authority.code_changed || already_applied);
            applied.emplace_back(&authority,
                authority.code_changed ? std::nullopt : std::optional{authority.code_hash});
        }
        // 7. Add PER_EMPTY_ACCOUNT_COST - PER_AUTH_BASE_COST gas to the global refund counter
        // if authority exists in the trie.
        // Successful authorization validation makes an account non-empty.
        // We apply the refund only if the account has existed before.
        // We detect "exists in the trie" by inspecting _empty_ property (EIP-161) because _empty_
        // implies an account doesn't exist in the state (EIP-7523).
        else if (!authority.is_empty())
        {
            static constexpr auto EXISTING_AUTHORITY_REFUND =
                AUTHORIZATION_EMPTY_ACCOUNT_COST - AUTHORIZATION_BASE_COST;
            delegation_refund += EXISTING_AUTHORITY_REFUND;
        }

        // 9. Increase the nonce of authority by one.
        ++authority.nonce;

        // As a special case, if address is 0 do not write the designation.
        // Clear the account’s code and reset the account’s code hash to the empty hash.
        if (is_zero(auth.addr))
        {
            if (authority.code_hash != Account::EMPTY_CODE_HASH)
            {
                authority.code_changed = true;
                authority.code.clear();
                authority.code_hash = Account::EMPTY_CODE_HASH;
            }
        }
        // 8. Set the code of authority to be 0xef0100 || address. This is a delegation designation.
        else
        {
            uint8_t designation_buf[std::size(DELEGATION_MAGIC) + sizeof(auth.addr)];
            const auto it = std::ranges::copy(DELEGATION_MAGIC, std::begin(designation_buf)).out;
            std::ranges::copy(auth.addr.bytes, it);
            const bytes_view designation{designation_buf, std::size(designation_buf)};
            if (authority.code != designation)
            {
                // We are doing this only if the code is different to make the state diff precise.
                authority.code_changed = true;
                authority.code = designation;
                authority.code_hash = keccak256(designation);
            }
        }
    }
    return delegation_refund;
}

evmc_message build_message(const Transaction& tx, const TransactionProperties& tx_props) noexcept
{
    const auto recipient = tx.to.has_value() ? *tx.to : compute_create_address(tx.sender, tx.nonce);

    return {
        .kind = tx.to.has_value() ? EVMC_CALL : EVMC_CREATE,
        .flags = 0,
        .depth = 0,
        .gas = tx_props.execution_gas_limit,
        .state_gas = tx_props.state_gas_limit,
        .recipient = recipient,
        .sender = tx.sender,
        .input_data = tx.data.data(),
        .input_size = tx.data.size(),
        .value = intx::be::store<evmc::uint256be>(tx.value),
        .code_address = recipient,
        .code = nullptr,
        .code_size = 0,
    };
}

/// Applies the authorizations (EIP-7702), resolves the delegation and calls the top-level message.
[[nodiscard]] evmc::Result process_top_level(
    State& state, Host& host, evmc_revision rev, const Transaction& tx, evmc_message msg)
{
    const auto state_gas_limit = msg.state_gas;
    StateGas state_gas{{.left = state_gas_limit}};

    std::vector<AppliedAuthorization> applied;
    const auto delegation_refund =
        process_authorization_list(state, tx, rev, msg.gas, state_gas, applied);

    // The authorizations' state-gas (left and spilled) stays consumed even if the call fails
    // (EIP-2780).
    const auto auth_state_gas = state_gas;

    // Resolves the delegation and, from Amsterdam, charges the recipient creation or the
    // delegation target access. These are exclusive because a delegated recipient exists.
    const auto prepare_call = [&] {
        // Creating the recipient account costs state-gas, refilled if the call fails (EIP-8037).
        if (!tx.to.has_value())
            return rev < EVMC_AMSTERDAM || host.account_exists(msg.recipient) ||
                   state_gas.charge(msg.gas, NEW_ACCOUNT_STATE_GAS);

        const auto delegate = get_delegate_address(host, *tx.to);
        if (!delegate.has_value())
            return rev < EVMC_AMSTERDAM || tx.value == 0 || host.account_exists(*tx.to) ||
                   state_gas.charge(msg.gas, NEW_ACCOUNT_STATE_GAS);

        assert(host.account_exists(*tx.to));
        msg.code_address = *delegate;
        msg.flags |= EVMC_DELEGATED;
        const auto warm = host.access_account(msg.code_address) == EVMC_ACCESS_WARM;
        if (rev < EVMC_AMSTERDAM)
            return true;
        // The delegation target access (EIP-2780).
        msg.gas -= warm ? instr::WARM_ACCESS : instr::COLD_ACCOUNT_ACCESS_AMSTERDAM;
        return msg.gas >= 0;
    };

    if (!delegation_refund.has_value() || !prepare_call())
    {
        // Out of gas before the call: the execution-gas is consumed, the state-gas is returned
        // and the authorizations are reverted (EIP-2780).
        assert(rev >= EVMC_AMSTERDAM);
        for (const auto& [authority, initial_code_hash] : applied)
        {
            --authority->nonce;
            if (initial_code_hash.has_value())
            {
                authority->code_changed = false;

                // Restore initial code hash. TODO: This is only needed for .is_empty() to work.
                authority->code_hash = *initial_code_hash;
            }
        }
        assert(
            std::ranges::none_of(applied, [](const auto& a) { return a.authority->code_changed; }));
        return evmc::Result{EVMC_OUT_OF_GAS, {.left = state_gas_limit}};
    }

    msg.state_gas = state_gas.left;
    auto result = host.call(msg);
    if (result.status_code == EVMC_SUCCESS)
    {
        result.state_gas.spilled += state_gas.spilled;
    }
    else
    {
        // Rollback state-gas costs.
        assert(result.state_gas.left == msg.state_gas);
        assert(result.state_gas.spilled == 0);
        if (result.status_code == EVMC_REVERT)
            result.gas_left += state_gas.spilled - auth_state_gas.spilled;
        result.state_gas = auth_state_gas;
    }
    result.gas_refund += *delegation_refund;  // Kept even if the call fails (EIP-7702).
    return result;
}
}  // namespace

StateDiff State::build_diff(evmc_revision rev) const
{
    StateDiff diff;
    diff.modified_accounts.reserve(m_modified.size());
    for (const auto& [addr, m] : m_modified)
    {
        if (m.nonexistent)
        {
            // A nonexistent account holds no change: anything else would be dropped silently.
            assert(m.is_empty() && !m.destructed && !m.erase_if_empty && !m.just_created &&
                   !m.code_changed);
            assert(std::ranges::all_of(m.storage | std::views::values,
                [](const auto& v) { return v.current == v.original; }));
            continue;
        }
        if (m.destructed)
        {
            if (rev >= EVMC_AMSTERDAM && m.balance != 0)
            {
                // Preserve the balance of the self-destructed account, no burn (EIP-8246).
                diff.modified_accounts.emplace_back(StateDiff::Entry{addr, 0, m.balance});
            }
            else
            {
                // Delete account. This must be done also for pre-funded just_created account.
                diff.deleted_accounts.emplace_back(addr);
            }
            continue;
        }
        if (m.erase_if_empty && rev >= EVMC_SPURIOUS_DRAGON && m.is_empty())
        {
            if (!m.just_created)  // Don't report just created accounts
                diff.deleted_accounts.emplace_back(addr);
            continue;
        }

        // Unconditionally report nonce and balance as modified.
        // TODO: We don't have information if the balance/nonce has actually changed.
        //   One option is to just keep the original values. This may be handy for RPC.
        // TODO(clang): In AppleClang 15 emplace_back without StateDiff::Entry doesn't compile.
        //   NOLINTNEXTLINE(modernize-use-emplace)
        auto& a = diff.modified_accounts.emplace_back(StateDiff::Entry{addr, m.nonce, m.balance});

        // Output only the new code.
        // TODO: Output also the code hash. It will be needed for DB update and MPT hash.
        if (m.code_changed)
            a.code = m.code;

        for (const auto& [k, v] : m.storage)
        {
            if (v.current != v.original)
                a.modified_storage.emplace_back(k, v.current);
        }
    }
    return diff;
}

std::pair<Account&, bool> State::get_or_create(const address& addr)
{
    auto& acc = get(addr);
    if (!acc.nonexistent)
        return {acc, false};
    journal_account_flags(addr, acc);
    // The account is created in place: a nonexistent account holds nothing but the warm status
    // and the warm storage slots, which must survive (EIP-2929).
    acc.nonexistent = false;
    return {acc, true};
}

Account& State::get(const address& addr) noexcept
{
    const auto [it, inserted] = m_modified.try_emplace(addr);
    auto& acc = it->second;
    if (inserted)
    {
        // Load the account from the initial state. A miss is cached as a nonexistent account,
        // so the initial state is queried once per address rather than once per access.
        if (const auto cacc = m_initial.get_account(addr); cacc)
        {
            acc.nonce = cacc->nonce;
            acc.balance = cacc->balance;
            acc.code_hash = cacc->code_hash;
            acc.has_initial_storage = cacc->has_storage;
        }
        else
            acc.nonexistent = true;
    }
    // A nonexistent account is readable: its zero fields are what a missing account holds.
    assert(!acc.nonexistent ||
           (acc.is_empty() && !acc.erase_if_empty && !acc.just_created && !acc.code_changed));
    return acc;
}

Account& State::get_or_insert(const address& addr)
{
    auto& acc = get(addr);
    if (acc.nonexistent)
    {
        acc.nonexistent = false;
        acc.erase_if_empty = true;
    }
    return acc;
}

bytes_view State::get_code(const address& addr)
{
    // A nonexistent account has the empty code hash, so it needs no separate check.
    auto& a = get(addr);
    if (a.code_hash == Account::EMPTY_CODE_HASH)
        return {};
    if (a.code.empty())
        a.code = m_initial.get_account_code(addr);
    return a.code;
}

Account& State::touch(const address& addr)
{
    auto& acc = get(addr);
    if (!acc.erase_if_empty && acc.is_empty())  // Also true for a nonexistent account.
    {
        journal_account_flags(addr, acc);
        acc.nonexistent = false;
        acc.erase_if_empty = true;
    }
    return acc;
}

StorageValue& State::get_storage(const address& addr, const bytes32& key)
{
    // TODO: Avoid account lookup by giving the reference to the account's storage to Host.
    auto& acc = get(addr);
    const auto [it, missing] = acc.storage.try_emplace(key);
    if (missing)
    {
        const auto initial_value = m_initial.get_storage(addr, key);
        it->second = {initial_value, initial_value};
    }
    return it->second;
}

void State::journal_balance_change(const address& addr, const intx::uint256& prev_balance)
{
    m_journal.emplace_back(JournalBalanceChange{{addr}, prev_balance});
}

void State::journal_storage_change(StorageValue& slot)
{
    m_journal.emplace_back(JournalStorageChange{&slot, slot.current, slot.access_status});
}

void State::journal_transient_storage_change(bytes32& slot)
{
    m_journal.emplace_back(JournalTransientStorageChange{&slot, slot});
}

void State::journal_bump_nonce(const address& addr)
{
    m_journal.emplace_back(JournalNonceBump{addr});
}

void State::journal_create(const address& addr)
{
    m_journal.emplace_back(JournalCreate{{addr}});
}

void State::journal_account_flags(const address& addr, const Account& acc)
{
    m_journal.emplace_back(JournalAccountFlags{
        {addr}, acc.access_status, acc.nonexistent, acc.destructed, acc.erase_if_empty});
}

namespace
{
/// Resets the account value fields set by a create. The balance and the storage are restored
/// by their own journal entries, which are always replayed first.
void clear_value(Account& a) noexcept
{
    a.nonce = 0;
    a.code_hash = Account::EMPTY_CODE_HASH;
    a.code.clear();
}
}  // namespace

void State::rollback(size_t checkpoint)
{
    while (m_journal.size() != checkpoint)
    {
        std::visit(
            [this](const auto& e) {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, JournalNonceBump>)
                {
                    auto& a = get(e.addr);
                    assert(!a.nonexistent);  // Replayed before the flags entry un-creating it.
                    a.nonce -= 1;
                }
                else if constexpr (std::is_same_v<T, JournalAccountFlags>)
                {
                    auto& a = get(e.addr);
                    a.access_status = e.access_status;
                    a.nonexistent = e.nonexistent;
                    a.destructed = e.destructed;
                    a.erase_if_empty = e.erase_if_empty;
                    if (e.nonexistent)
                    {
                        // An account which did not exist holds nothing.
                        assert(a.balance == 0);  // Restored by the balance entries replayed first.
                        clear_value(a);
                        a.just_created = false;
                        a.code_changed = false;
                    }
                }
                else if constexpr (std::is_same_v<T, JournalCreate>)
                {
                    // Revert a create over a pre-existing account.
                    // TODO: Why this account is not always "touched"?
                    auto& a = get(e.addr);
                    assert(!a.nonexistent);
                    clear_value(a);
                }
                else if constexpr (std::is_same_v<T, JournalStorageChange>)
                {
                    e.slot->current = e.prev_value;
                    e.slot->access_status = e.prev_access_status;
                }
                else if constexpr (std::is_same_v<T, JournalTransientStorageChange>)
                {
                    *e.slot = e.prev_value;
                }
                else if constexpr (std::is_same_v<T, JournalBalanceChange>)
                {
                    auto& a = get(e.addr);
                    assert(!a.nonexistent);  // Replayed before the flags entry un-creating it.
                    a.balance = e.prev_balance;
                }
                else
                {
                    // TODO(C++23): Change condition to `false` once CWG2518 is in.
                    static_assert(std::is_void_v<T>, "unhandled journal entry type");
                }
            },
            m_journal.back());
        m_journal.pop_back();
    }
}

/// Validates transaction and computes the gas limits it provides to the EVM.
/// @return  The transaction's computed gas properties or a validation error.
std::variant<TransactionProperties, std::error_code> validate_transaction(
    const StateView& state_view, const BlockInfo& block, const Transaction& tx, evmc_revision rev,
    int64_t block_gas_left, int64_t block_state_gas_left, int64_t blob_gas_left) noexcept
{
    if (tx.chain_id_protected() && tx.chain_id != block.chain_id)
        return make_error_code(INVALID_CHAIN_ID);

    switch (tx.type)  // Validate "special" transaction types.
    {
    case Transaction::Type::blob:
        if (rev < EVMC_CANCUN)
            return make_error_code(TYPE_NOT_SUPPORTED);
        if (!tx.to.has_value())
            return make_error_code(CREATE_BLOB_TX);
        if (tx.blob_hashes.empty())
            return make_error_code(EMPTY_BLOB_HASHES_LIST);
        if (rev >= EVMC_OSAKA && tx.blob_hashes.size() > MAX_TX_BLOB_COUNT)
            return make_error_code(BLOB_GAS_LIMIT_EXCEEDED);

        assert(block.blob_base_fee.has_value());
        if (tx.max_blob_gas_price < *block.blob_base_fee)
            return make_error_code(INSUFFICIENT_MAX_FEE_PER_BLOB_GAS);

        if (std::ranges::any_of(tx.blob_hashes, [](const auto& h) { return h.bytes[0] != 0x01; }))
            return make_error_code(INVALID_BLOB_HASH_VERSION);
        if (std::cmp_greater(tx.blob_gas_used(), blob_gas_left))
            return make_error_code(BLOB_GAS_LIMIT_EXCEEDED);
        break;

    case Transaction::Type::set_code:
        if (rev < EVMC_PRAGUE)
            return make_error_code(TYPE_NOT_SUPPORTED);
        if (!tx.to.has_value())
            return make_error_code(CREATE_SET_CODE_TX);
        if (tx.authorization_list.empty())
            return make_error_code(EMPTY_AUTHORIZATION_LIST);
        break;

    default:;
    }

    switch (tx.type)  // Validate the "regular" transaction type hierarchy.
    {
    case Transaction::Type::set_code:
    case Transaction::Type::blob:
    case Transaction::Type::eip1559:
        if (rev < EVMC_LONDON)
            return make_error_code(TYPE_NOT_SUPPORTED);

        if (tx.max_priority_gas_price > tx.max_gas_price)
            return make_error_code(PRIORITY_GREATER_THAN_MAX_FEE_PER_GAS);
        [[fallthrough]];

    case Transaction::Type::access_list:
        if (rev < EVMC_BERLIN)
            return make_error_code(TYPE_NOT_SUPPORTED);
        [[fallthrough]];

    case Transaction::Type::legacy:;
    }

    assert(tx.max_priority_gas_price <= tx.max_gas_price);

    if (rev == EVMC_OSAKA && tx.gas_limit > MAX_TX_GAS_LIMIT)
        return make_error_code(GAS_LIMIT_EXCEEDS_MAXIMUM);

    if (rev >= EVMC_AMSTERDAM && tx.gas_limit > int64_t{MAX_TX_TOTAL_GAS_LIMIT})
        return make_error_code(GAS_LIMIT_EXCEEDS_MAXIMUM);

    if (rev < EVMC_AMSTERDAM)
    {
        if (tx.gas_limit > block_gas_left)
            return make_error_code(GAS_ALLOWANCE_EXCEEDED);
    }
    else
    {
        // Check gas limits in both dimensions.
        if (std::min(tx.gas_limit, int64_t{MAX_TX_GAS_LIMIT}) > block_gas_left)
            return make_error_code(GAS_ALLOWANCE_EXCEEDED);
        if (tx.gas_limit > block_state_gas_left)
            return make_error_code(GAS_ALLOWANCE_EXCEEDED);
    }

    if (tx.max_gas_price < block.base_fee)
        return make_error_code(INSUFFICIENT_MAX_FEE_PER_GAS);

    // We need some information about the sender so lookup the account in the state.
    // TODO: During transaction execution this account will be also needed, so we may pass it along.
    const auto sender_acc = state_view.get_account(tx.sender).value_or(
        StateView::Account{.code_hash = Account::EMPTY_CODE_HASH});

    if (sender_acc.code_hash != Account::EMPTY_CODE_HASH &&
        !is_code_delegated(state_view.get_account_code(tx.sender)))
        return make_error_code(SENDER_NOT_EOA);  // Origin must not be a contract (EIP-3607).

    if (sender_acc.nonce == MAX_NONCE)  // Nonce value limit (EIP-2681).
        return make_error_code(NONCE_IS_MAX);

    if (sender_acc.nonce < tx.nonce)
        return make_error_code(NONCE_TOO_HIGH);

    if (sender_acc.nonce > tx.nonce)
        return make_error_code(NONCE_TOO_LOW);

    // Initcode size is limited by EIP-3860, raised for Amsterdam by EIP-7954.
    const size_t max_initcode_size =
        rev >= EVMC_AMSTERDAM ? MAX_INITCODE_SIZE_AMSTERDAM : MAX_INITCODE_SIZE;
    if (rev >= EVMC_SHANGHAI && !tx.to.has_value() && tx.data.size() > max_initcode_size)
        return make_error_code(INITCODE_SIZE_EXCEEDED);

    // Compute and check if sender has enough balance for the theoretical maximum transaction cost.
    // Note this is different from tx_max_cost computed with effective gas price later.
    // The computation cannot overflow if done with 512-bit precision.
    auto max_total_fee = umul(uint256{tx.gas_limit}, tx.max_gas_price);
    max_total_fee += tx.value;

    if (tx.type == Transaction::Type::blob)
        max_total_fee += umul(uint256{tx.blob_gas_used()}, tx.max_blob_gas_price);
    if (sender_acc.balance < max_total_fee)
        return make_error_code(INSUFFICIENT_ACCOUNT_FUNDS);

    const auto [intrinsic_cost, min_cost] = compute_tx_intrinsic_cost(rev, tx);

    // The transaction state-gas limit is all above the cap constant (EIP-8037).
    const auto state_gas_limit =
        rev >= EVMC_AMSTERDAM ? std::max(tx.gas_limit - MAX_TX_GAS_LIMIT, int64_t{0}) : 0;

    // Transaction gas limit with state-gas limit excluded must cover intrinsic and min cost.
    if (tx.gas_limit - state_gas_limit < std::max(intrinsic_cost, min_cost))
        return make_error_code(INTRINSIC_GAS_TOO_LOW);

    const auto execution_gas_limit = tx.gas_limit - intrinsic_cost - state_gas_limit;
    return TransactionProperties{execution_gas_limit, state_gas_limit, min_cost};
}

StateDiff finalize(const StateView& state_view, evmc_revision rev, const address& coinbase,
    std::optional<uint64_t> block_reward, std::span<const Ommer> ommers,
    std::span<const Withdrawal> withdrawals)
{
    State state{state_view};
    // TODO: The block reward can be represented as a withdrawal.
    if (block_reward.has_value())
    {
        const auto reward = *block_reward;
        assert(reward % 32 == 0);  // Assume block reward is divisible by 32.
        const auto reward_by_32 = reward / 32;
        const auto reward_by_8 = reward / 8;

        state.touch(coinbase).balance += reward + reward_by_32 * ommers.size();
        for (const auto& ommer : ommers)
        {
            assert(ommer.delta > 0 && ommer.delta < 8);
            state.touch(ommer.beneficiary).balance += reward_by_8 * (8 - ommer.delta);
        }
    }

    for (const auto& withdrawal : withdrawals)
        state.touch(withdrawal.recipient).balance += withdrawal.get_amount();

    return state.build_diff(rev);
}

TransactionReceipt transition(const StateView& state_view, const BlockInfo& block,
    const BlockHashes& block_hashes, const Transaction& tx, evmc_revision rev, evmc::VM& vm,
    const TransactionProperties& tx_props)
{
    State state{state_view};

    auto& sender_acc = state.get_or_insert(tx.sender);
    assert(sender_acc.nonce < MAX_NONCE);  // Required for valid tx.
    ++sender_acc.nonce;                    // Bump sender nonce.

    const auto base_fee = (rev >= EVMC_LONDON) ? block.base_fee : 0;
    assert(tx.max_gas_price >= base_fee);                   // Required for valid tx.
    assert(tx.max_gas_price >= tx.max_priority_gas_price);  // Required for valid tx.
    const auto priority_gas_price =
        std::min(tx.max_priority_gas_price, tx.max_gas_price - base_fee);
    const auto effective_gas_price = base_fee + priority_gas_price;

    assert(effective_gas_price <= tx.max_gas_price);  // Required for valid tx.
    const auto tx_max_cost = tx.gas_limit * effective_gas_price;

    sender_acc.balance -= tx_max_cost;  // Modify sender balance after all checks.

    if (tx.type == Transaction::Type::blob)
    {
        // This uint64 * uint256 cannot overflow, because tx.blob_gas_used has limits enforced
        // before this stage.
        assert(block.blob_base_fee.has_value());
        const auto blob_fee = intx::umul(intx::uint256(tx.blob_gas_used()), *block.blob_base_fee);
        assert(blob_fee <= std::numeric_limits<intx::uint256>::max());
        assert(sender_acc.balance >= blob_fee);  // Required for valid tx.
        sender_acc.balance -= intx::uint256(blob_fee);
    }

    Host host{rev, vm, state, block, block_hashes, tx};

    const auto message = build_message(tx, tx_props);

    sender_acc.access_status = EVMC_ACCESS_WARM;  // Sender is always warm.
    host.access_account(message.recipient);  // Recipient (incl. create address) is always warm.
    for (const auto& [a, storage_keys] : tx.access_list)
    {
        host.access_account(a);
        if (is_precompile(rev, a))  // Precompile storage is never accessed.
            continue;
        for (const auto& key : storage_keys)
            state.get_storage(a, key).access_status = EVMC_ACCESS_WARM;
    }
    // EIP-3651: Warm COINBASE.
    if (rev >= EVMC_SHANGHAI)
        host.access_account(block.coinbase);

    const auto result = process_top_level(state, host, rev, tx, message);

    const auto gas_used_b4_refund = tx.gas_limit - result.gas_left - result.state_gas.left;

    const auto refund_limit = rev >= EVMC_LONDON ? gas_used_b4_refund / 5 : gas_used_b4_refund / 2;
    const auto refund = std::min(result.gas_refund, refund_limit);
    auto gas_used = gas_used_b4_refund - refund;
    assert(gas_used > 0);  // The refund is capped at a fraction of the gas used.

    // The gas used by the transaction must be at least the min_gas_cost (EIP-7623).
    gas_used = std::max(gas_used, tx_props.min_gas_cost);

    const auto state_gas_used =
        tx_props.state_gas_limit - result.state_gas.left + result.state_gas.spilled;
    assert(state_gas_used >= 0);

    // For block gas accounting, exclude refunds and enforce the min gas cost, raised by the
    // state gas so state-gas spending cannot discount it (EIP-7778, EIP-8037).
    const auto block_gas_used =
        (rev >= EVMC_AMSTERDAM) ?
            std::max(gas_used_b4_refund, tx_props.min_gas_cost + state_gas_used) :
            gas_used;

    sender_acc.balance += tx_max_cost - gas_used * effective_gas_price;
    state.touch(block.coinbase).balance += gas_used * priority_gas_price;

    // Cumulative gas used is unknown in this scope.
    TransactionReceipt receipt{
        .type = tx.type,
        .status = result.status_code,
        .gas_used = gas_used,
        .block_gas_used = block_gas_used,
        .state_gas_used = state_gas_used,
        .logs = host.take_logs(),
        .state_diff = state.build_diff(rev),
    };

    // Cannot put it into constructor call because logs are std::moved from host instance.
    receipt.logs_bloom_filter = compute_bloom_filter(receipt.logs);

    return receipt;
}
}  // namespace evmone::state

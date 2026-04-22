// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2022 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "state.hpp"
#include "../utils/stdx/utility.hpp"
#include "host.hpp"
#include "state_view.hpp"
#include "system_contracts.hpp"
#include <evmone/constants.hpp>
#include <evmone/delegation.hpp>
#include <evmone_precompiles/secp256k1.hpp>
#include <algorithm>
#include <unordered_set>

using namespace intx;

namespace evmone::state
{
namespace
{
/// Secp256k1's N/2 is the upper bound of the signature's s value.
constexpr auto SECP256K1N_OVER_2 = evmmax::secp256k1::Curve::ORDER / 2;
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

/// EIP-7976 floor token count in Amsterdam: every calldata byte counts as 4 tokens.
size_t compute_tx_data_floor_tokens(evmc_revision rev, bytes_view data) noexcept
{
    if (rev >= EVMC_AMSTERDAM)
        return 4 * data.size();
    return compute_tx_data_tokens(rev, data);
}

int64_t compute_access_list_cost(const AccessList& access_list) noexcept
{
    static constexpr auto ADDRESS_COST = 2400;
    static constexpr auto STORAGE_KEY_COST = 1900;

    int64_t cost = 0;
    for (const auto& [_, keys] : access_list)
        cost += ADDRESS_COST + static_cast<int64_t>(keys.size()) * STORAGE_KEY_COST;
    return cost;
}

/// EIP-7981: Number of calldata-floor tokens contributed by access-list data.
/// Each address = 20 bytes, each storage key = 32 bytes, 4 tokens per byte.
size_t compute_access_list_tokens(const AccessList& access_list) noexcept
{
    size_t bytes = 0;
    for (const auto& [_, keys] : access_list)
        bytes += 20 + keys.size() * 32;
    return 4 * bytes;
}

struct TransactionCost
{
    int64_t intrinsic = 0;        ///< Regular intrinsic gas.
    int64_t intrinsic_state = 0;  ///< EIP-8037: State gas component of intrinsic.
    int64_t min = 0;
};

/// Compute the transaction intrinsic gas 𝑔₀ (Yellow Paper, 6.2) and minimal gas (EIP-7623).
TransactionCost compute_tx_intrinsic_cost(
    evmc_revision rev, const Transaction& tx, int64_t block_gas_limit) noexcept
{
    // EIP-8037: cost per state byte is dynamic based on block gas limit.
    const auto cpsb = (rev >= EVMC_AMSTERDAM) ? evmone::compute_cpsb(block_gas_limit) : int64_t{0};
    static constexpr auto TX_BASE_COST = 21000;
    static constexpr auto TX_CREATE_COST = 32000;
    static constexpr auto DATA_TOKEN_COST = 4;
    static constexpr auto INITCODE_WORD_COST = 2;
    // EIP-7976: Amsterdam raises the calldata floor per-token cost from 10 to 16.
    const auto total_cost_floor_per_token = (rev >= EVMC_AMSTERDAM) ? int64_t{16} : int64_t{10};

    const auto is_create = !tx.to.has_value();

    // EIP-8037: Amsterdam splits CREATE cost: regular 9000, state 112*CPSB.
    const auto tx_create_regular = (rev >= EVMC_AMSTERDAM) ? int64_t{9000} : TX_CREATE_COST;
    const auto create_cost = (is_create && rev >= EVMC_HOMESTEAD) ? tx_create_regular : 0;
    const auto create_state_cost =
        (rev >= EVMC_AMSTERDAM && is_create) ? int64_t{112 * cpsb} : 0;

    const auto num_tokens = static_cast<int64_t>(compute_tx_data_tokens(rev, tx.data));
    const auto data_cost = num_tokens * DATA_TOKEN_COST;

    // EIP-7981: In Amsterdam+ access-list data bytes contribute an extra cost
    // (TOTAL_COST_FLOOR_PER_TOKEN * floor_tokens_in_access_list) to the regular intrinsic,
    // and the same tokens are counted in the floor.
    const auto access_list_tokens = (rev >= EVMC_AMSTERDAM) ?
        static_cast<int64_t>(compute_access_list_tokens(tx.access_list)) : int64_t{0};
    const auto access_list_cost =
        compute_access_list_cost(tx.access_list) + access_list_tokens * total_cost_floor_per_token;

    // EIP-8037: Amsterdam splits auth cost: regular 7500, state (112+23)*CPSB per tuple.
    const auto per_auth_regular = (rev >= EVMC_AMSTERDAM) ?
        int64_t{7500} : AUTHORIZATION_EMPTY_ACCOUNT_COST;
    const auto auth_list_cost =
        static_cast<int64_t>(tx.authorization_list.size()) * per_auth_regular;
    const auto auth_state_cost = (rev >= EVMC_AMSTERDAM) ?
        static_cast<int64_t>(tx.authorization_list.size()) * (112 + 23) * cpsb : int64_t{0};

    const auto initcode_cost =
        (is_create && rev >= EVMC_SHANGHAI) ? INITCODE_WORD_COST * num_words(tx.data.size()) : 0;

    const auto intrinsic_cost =
        TX_BASE_COST + create_cost + data_cost + access_list_cost + auth_list_cost + initcode_cost;

    // EIP-7623: Compute the minimum cost for the transaction by. If disabled, just use 0.
    // EIP-7976: Amsterdam counts every calldata byte as 4 tokens in the floor.
    // EIP-7981: Amsterdam includes access-list tokens in the floor.
    const auto floor_tokens = static_cast<int64_t>(compute_tx_data_floor_tokens(rev, tx.data))
                              + access_list_tokens;
    const auto min_cost =
        rev >= EVMC_PRAGUE ? TX_BASE_COST + floor_tokens * total_cost_floor_per_token : 0;

    const auto intrinsic_state_cost = create_state_cost + auth_state_cost;

    return {intrinsic_cost, intrinsic_state_cost, min_cost};
}

int64_t process_authorization_list(
    State& state, uint64_t chain_id, const AuthorizationList& authorization_list,
    evmc_revision rev = EVMC_PRAGUE, int64_t cpsb = 0)
{
    int64_t delegation_refund = 0;
    for (const auto& auth : authorization_list)
    {
        // 1. Verify the chain id is either 0 or the chain’s current ID.
        if (auth.chain_id != 0 && auth.chain_id != chain_id)
            continue;

        // 2. Verify the nonce is less than 2**64 - 1.
        if (auth.nonce == Account::NonceMax)
            continue;

        // 3. Verify if the signer has been successfully recovered from the signature.
        //    authority = ecrecover(...)
        // y_parity must be 0 or 1 for EIP-7702/2930 signatures.
        if (auth.v > 1)
            continue;
        // TODO: We actually only do "partial" verification by assuming the signature is valid
        //   when the test has the signer specified.
        if (!auth.signer.has_value())
            continue;

        // s value must be less than or equal to secp256k1n/2, as specified in EIP-2.
        if (auth.s > SECP256K1N_OVER_2)
            continue;

        // Get or create the authority account.
        // It is still empty at this point until nonce bump following successful authorization.
        auto& authority = state.get_or_insert(*auth.signer, {.erase_if_empty = true});

        // 4. Add authority to accessed_addresses (as defined in EIP-2929.)
        authority.access_status = EVMC_ACCESS_WARM;

        // 5. Verify the code of authority is either empty or already delegated.
        if (authority.code_hash != Account::EMPTY_CODE_HASH &&
            !is_code_delegated(state.get_code(*auth.signer)))
            continue;

        // 6. Verify the nonce of authority is equal to nonce.
        // In case authority does not exist in the trie, verify that nonce is equal to 0.
        if (auth.nonce != authority.nonce)
            continue;

        // 7. Add PER_EMPTY_ACCOUNT_COST - PER_AUTH_BASE_COST gas to the global refund counter
        // if authority exists in the trie.
        // Successful authorization validation makes an account non-empty.
        // We apply the refund only if the account has existed before.
        // We detect "exists in the trie" by inspecting _empty_ property (EIP-161) because _empty_
        // implies an account doesn't exist in the state (EIP-7523).
        if (!authority.is_empty())
        {
            // EIP-8037: For Amsterdam, refund the account creation portion (112 * CPSB).
            // Pre-Amsterdam: refund EMPTY - BASE = 25000 - 12500 = 12500.
            const auto existing_auth_refund = (rev >= EVMC_AMSTERDAM) ?
                int64_t{112 * cpsb} : int64_t{AUTHORIZATION_EMPTY_ACCOUNT_COST - AUTHORIZATION_BASE_COST};
            delegation_refund += existing_auth_refund;
        }

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
            auto new_code = bytes(DELEGATION_MAGIC) + bytes(auth.addr);
            if (authority.code != new_code)
            {
                // We are doing this only if the code is different to make the state diff precise.
                authority.code_changed = true;
                authority.code = std::move(new_code);
                authority.code_hash = keccak256(authority.code);
            }
        }

        // 9. Increase the nonce of authority by one.
        ++authority.nonce;
    }
    return delegation_refund;
}

evmc_message build_message(const Transaction& tx, int64_t execution_gas_limit) noexcept
{
    const auto recipient = tx.to.has_value() ? *tx.to : evmc::address{};

    return {
        .kind = tx.to.has_value() ? EVMC_CALL : EVMC_CREATE,
        .flags = 0,
        .depth = 0,
        .gas = execution_gas_limit,
        .recipient = recipient,
        .sender = tx.sender,
        .input_data = tx.data.data(),
        .input_size = tx.data.size(),
        .value = intx::be::store<evmc::uint256be>(tx.value),
        .create2_salt = {},
        .code_address = recipient,
        .code = nullptr,
        .code_size = 0,
    };
}
}  // namespace

StateDiff State::build_diff(evmc_revision rev) const
{
    StateDiff diff;
    for (const auto& [addr, m] : m_modified)
    {
        // Skip placeholders created only for access-list warming. Their data
        // was either never fetched (loaded==false) or confirmed absent
        // (exists_in_state==false with no subsequent modifications).
        if (!m.loaded || !m.exists_in_state)
            continue;

        if (m.destructed)
        {
            // TODO: This must be done even for just_created
            //   because destructed may pre-date just_created. Add test to evmone (EEST has it).
            diff.deleted_accounts.emplace_back(addr);
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

Account& State::insert(const address& addr, Account account)
{
    auto [it, inserted] = m_modified.try_emplace(addr);
    if (inserted)
    {
        it->second = std::move(account);
        return it->second;
    }

    // The entry already exists — it must be a "warm-only" placeholder (either
    // still unloaded, or loaded and confirmed absent from the StateView).
    // Overwriting a real loaded account here would silently corrupt state.
    assert(!it->second.loaded || !it->second.exists_in_state);
    const auto was_warm = (it->second.access_status == EVMC_ACCESS_WARM);
    const auto was_touched = it->second.erase_if_empty;
    it->second = std::move(account);
    if (was_warm)
        it->second.access_status = EVMC_ACCESS_WARM;
    it->second.erase_if_empty = it->second.erase_if_empty || was_touched;
    it->second.loaded = true;
    it->second.exists_in_state = true;
    return it->second;
}

Account* State::find(const address& addr) noexcept
{
    // TODO: Avoid double lookup (find+insert) and not cached initial state lookup for non-existent
    //   accounts. If we want to cache non-existent account we need a proper flag for it.
    if (const auto it = m_modified.find(addr); it != m_modified.end())
    {
        if (it->second.loaded)
            return it->second.exists_in_state ? &it->second : nullptr;

        // Access-list placeholder: lazy-load from StateView now.
        const auto cacc = m_initial.get_account(addr);
        it->second.loaded = true;
        if (!cacc)
        {
            it->second.exists_in_state = false;
            return nullptr;  // Account doesn't exist; the entry stays warm-only.
        }
        it->second.nonce = cacc->nonce;
        it->second.balance = cacc->balance;
        it->second.code_hash = cacc->code_hash;
        it->second.has_initial_storage = cacc->has_storage;
        it->second.exists_in_state = true;
        return &it->second;
    }
    if (const auto cacc = m_initial.get_account(addr); cacc)
        return &insert(addr, {.nonce = cacc->nonce,
                                 .balance = cacc->balance,
                                 .code_hash = cacc->code_hash,
                                 .has_initial_storage = cacc->has_storage});
    return nullptr;
}

Account& State::get(const address& addr) noexcept
{
    auto acc = find(addr);
    assert(acc != nullptr);
    return *acc;
}

Account& State::get_or_insert(const address& addr, Account account)
{
    if (const auto acc = find(addr); acc != nullptr)
        return *acc;
    // find() may return null while a warm-only placeholder is still in m_modified
    // (for an address not present in the underlying StateView). Our insert()
    // promotes the placeholder in that case, preserving its warm/touched flags.
    return insert(addr, std::move(account));
}

Account& State::get_or_insert_for_access(const address& addr)
{
    // Fast path: entry already present (loaded or warm-only placeholder).
    if (const auto it = m_modified.find(addr); it != m_modified.end())
        return it->second;

    // Insert a lightweight placeholder without querying the cold StateView.
    // The real balance/nonce/code will be fetched on-demand by find().
    Account placeholder{};
    placeholder.erase_if_empty = true;
    placeholder.loaded = false;
    return insert(addr, std::move(placeholder));
}

bytes_view State::get_code(const address& addr)
{
    auto* a = find(addr);
    if (a == nullptr)
        return {};
    if (a->code_hash == Account::EMPTY_CODE_HASH)
        return {};
    if (a->code.empty())
        a->code = m_initial.get_account_code(addr);
    return a->code;
}

Account& State::touch(const address& addr)
{
    auto& acc = get_or_insert(addr, {.erase_if_empty = true});
    if (!acc.erase_if_empty && acc.is_empty())
    {
        acc.erase_if_empty = true;
        m_journal.emplace_back(JournalTouched{addr});
    }
    return acc;
}

StorageValue& State::get_storage(const address& addr, const bytes32& key)
{
    // TODO: Avoid account lookup by giving the reference to the account's storage to Host.
    // Work on any entry already in m_modified (including access-only placeholders)
    // without triggering a StateView account fetch; fall back to lazy-load or
    // create an access-only placeholder if nothing is there.
    Account* acc = find_modified(addr);
    if (acc == nullptr)
    {
        acc = find(addr);
        if (acc == nullptr)
            acc = &get_or_insert_for_access(addr);
    }
    const auto [it, _] = acc->storage.try_emplace(key);
    if (!it->second.loaded)
    {
        // The slot may have been created by access_storage() without a fetch.
        // Load the underlying value now; preserve access_status set earlier.
        const auto initial_value = m_initial.get_storage(addr, key);
        it->second.current = initial_value;
        it->second.original = initial_value;
        it->second.loaded = true;
    }
    return it->second;
}

void State::journal_balance_change(const address& addr, const intx::uint256& prev_balance)
{
    m_journal.emplace_back(JournalBalanceChange{{addr}, prev_balance});
}

void State::journal_storage_change(
    const address& addr, const bytes32& key, const StorageValue& value)
{
    m_journal.emplace_back(JournalStorageChange{{addr}, key, value.current, value.access_status});
}

void State::journal_storage_access(
    const address& addr, const bytes32& key, evmc_access_status prev_status, bool was_fresh)
{
    m_journal.emplace_back(JournalStorageAccess{{addr}, key, prev_status, was_fresh});
}

void State::journal_transient_storage_change(
    const address& addr, const bytes32& key, const bytes32& value)
{
    m_journal.emplace_back(JournalTransientStorageChange{{addr}, key, value});
}

void State::journal_bump_nonce(const address& addr)
{
    m_journal.emplace_back(JournalNonceBump{addr});
}

void State::journal_create(const address& addr, bool existed)
{
    m_journal.emplace_back(JournalCreate{{addr}, existed});
}

void State::journal_destruct(const address& addr)
{
    m_journal.emplace_back(JournalDestruct{addr});
}

void State::journal_access_account(const address& addr)
{
    m_journal.emplace_back(JournalAccessAccount{addr});
}

void State::rollback(size_t checkpoint)
{
    // Rollback lookups never go through the StateView: every journal entry is
    // paired with an in-m_modified account, so find_modified() is safe and
    // also keeps cold-read hooks quiet.
    const auto modified_get = [this](const address& addr) -> Account& {
        auto* const a = find_modified(addr);
        assert(a != nullptr);
        return *a;
    };
    while (m_journal.size() != checkpoint)
    {
        std::visit(
            [&](const auto& e) {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, JournalNonceBump>)
                {
                    modified_get(e.addr).nonce -= 1;
                }
                else if constexpr (std::is_same_v<T, JournalTouched>)
                {
                    modified_get(e.addr).erase_if_empty = false;
                }
                else if constexpr (std::is_same_v<T, JournalDestruct>)
                {
                    modified_get(e.addr).destructed = false;
                }
                else if constexpr (std::is_same_v<T, JournalAccessAccount>)
                {
                    // The entry may have already been erased by a JournalCreate
                    // rollback (access_account-created placeholder followed by a
                    // failed CREATE). Skip silently in that case.
                    if (auto* a = find_modified(e.addr); a != nullptr)
                        a->access_status = EVMC_ACCESS_COLD;
                }
                else if constexpr (std::is_same_v<T, JournalCreate>)
                {
                    if (e.existed)
                    {
                        // This account is not always "touched". TODO: Why?
                        auto& a = modified_get(e.addr);
                        a.nonce = 0;
                        a.code_hash = Account::EMPTY_CODE_HASH;
                        a.code.clear();
                    }
                    else
                    {
                        // TODO: Before Spurious Dragon we don't clear empty accounts ("erasable")
                        //       so we need to delete them here explicitly.
                        //       This should be changed by tuning "erasable" flag
                        //       and clear in all revisions.
                        m_modified.erase(e.addr);
                    }
                }
                else if constexpr (std::is_same_v<T, JournalStorageChange>)
                {
                    auto& s = modified_get(e.addr).storage.find(e.key)->second;
                    s.current = e.prev_value;
                    s.access_status = e.prev_access_status;
                }
                else if constexpr (std::is_same_v<T, JournalStorageAccess>)
                {
                    auto& storage = modified_get(e.addr).storage;
                    if (e.was_fresh)
                        storage.erase(e.key);
                    else
                        storage.find(e.key)->second.access_status = e.prev_access_status;
                }
                else if constexpr (std::is_same_v<T, JournalTransientStorageChange>)
                {
                    auto& s = modified_get(e.addr).transient_storage.find(e.key)->second;
                    s = e.prev_value;
                }
                else if constexpr (std::is_same_v<T, JournalBalanceChange>)
                {
                    modified_get(e.addr).balance = e.prev_balance;
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

/// Validates transaction and computes its execution gas limit (the amount of gas provided to EVM).
/// @return  Execution gas limit or transaction validation error.
std::variant<TransactionProperties, std::error_code> validate_transaction(
    const StateView& state_view, const BlockInfo& block, const Transaction& tx, evmc_revision rev,
    int64_t block_gas_left, int64_t blob_gas_left) noexcept
{
    switch (tx.type)  // Validate "special" transaction types.
    {
    case Transaction::Type::blob:
        if (rev < EVMC_CANCUN)
            return make_error_code(TX_TYPE_NOT_SUPPORTED);
        if (!tx.to.has_value())
            return make_error_code(CREATE_BLOB_TX);
        if (tx.blob_hashes.empty())
            return make_error_code(EMPTY_BLOB_HASHES_LIST);
        if (rev >= EVMC_OSAKA && tx.blob_hashes.size() > MAX_TX_BLOB_COUNT)
            return make_error_code(BLOB_GAS_LIMIT_EXCEEDED);

        assert(block.blob_base_fee.has_value());
        if (tx.max_blob_gas_price < *block.blob_base_fee)
            return make_error_code(BLOB_FEE_CAP_LESS_THAN_BLOCKS);

        if (std::ranges::any_of(tx.blob_hashes, [](const auto& h) { return h.bytes[0] != 0x01; }))
            return make_error_code(INVALID_BLOB_HASH_VERSION);
        if (std::cmp_greater(tx.blob_gas_used(), blob_gas_left))
            return make_error_code(BLOB_GAS_LIMIT_EXCEEDED);
        break;

    case Transaction::Type::set_code:
        if (rev < EVMC_PRAGUE)
            return make_error_code(TX_TYPE_NOT_SUPPORTED);
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
            return make_error_code(TX_TYPE_NOT_SUPPORTED);

        if (tx.max_priority_gas_price > tx.max_gas_price)
            return make_error_code(TIP_GT_FEE_CAP);  // Priority gas price is too high.
        [[fallthrough]];

    case Transaction::Type::access_list:
        if (rev < EVMC_BERLIN)
            return make_error_code(TX_TYPE_NOT_SUPPORTED);
        [[fallthrough]];

    case Transaction::Type::legacy:;
    }

    assert(tx.max_priority_gas_price <= tx.max_gas_price);

    if (rev >= EVMC_OSAKA && rev < EVMC_AMSTERDAM && tx.gas_limit > MAX_TX_GAS_LIMIT)
        return make_error_code(MAX_GAS_LIMIT_EXCEEDED);

    // EIP-8037: For Amsterdam, per-tx cumulative gas check is relaxed.
    // But tx.gas_limit must still not exceed the block's total gas limit.
    if (tx.gas_limit > block_gas_left)
    {
        if (rev < EVMC_AMSTERDAM || tx.gas_limit > block.gas_limit)
            return make_error_code(GAS_LIMIT_REACHED);
    }

    if (tx.max_gas_price < block.base_fee)
        return make_error_code(FEE_CAP_LESS_THAN_BLOCKS);

    // We need some information about the sender so lookup the account in the state.
    // TODO: During transaction execution this account will be also needed, so we may pass it along.
    const auto sender_acc = state_view.get_account(tx.sender).value_or(
        StateView::Account{.code_hash = Account::EMPTY_CODE_HASH});

    if (sender_acc.code_hash != Account::EMPTY_CODE_HASH &&
        !is_code_delegated(state_view.get_account_code(tx.sender)))
        return make_error_code(SENDER_NOT_EOA);  // Origin must not be a contract (EIP-3607).

    if (sender_acc.nonce == Account::NonceMax)  // Nonce value limit (EIP-2681).
        return make_error_code(NONCE_HAS_MAX_VALUE);

    if (sender_acc.nonce < tx.nonce)
        return make_error_code(NONCE_TOO_HIGH);

    if (sender_acc.nonce > tx.nonce)
        return make_error_code(NONCE_TOO_LOW);

    // initcode size is limited by EIP-3860 / EIP-7954 (Amsterdam increases limit).
    const auto max_initcode_size =
        rev >= EVMC_AMSTERDAM ? MAX_INITCODE_SIZE_AMSTERDAM : MAX_INITCODE_SIZE;
    if (rev >= EVMC_SHANGHAI && !tx.to.has_value() &&
        tx.data.size() > static_cast<size_t>(max_initcode_size))
        return make_error_code(INIT_CODE_SIZE_LIMIT_EXCEEDED);

    // Compute and check if sender has enough balance for the theoretical maximum transaction cost.
    // Note this is different from tx_max_cost computed with effective gas price later.
    // The computation cannot overflow if done with 512-bit precision.
    auto max_total_fee = umul(uint256{tx.gas_limit}, tx.max_gas_price);
    max_total_fee += tx.value;

    if (tx.type == Transaction::Type::blob)
    {
        const auto total_blob_gas = tx.blob_gas_used();
        // FIXME: Can overflow uint256.
        max_total_fee += total_blob_gas * tx.max_blob_gas_price;
    }
    if (sender_acc.balance < max_total_fee)
        return make_error_code(INSUFFICIENT_FUNDS);

    const auto [intrinsic_cost, intrinsic_state, min_cost] =
        compute_tx_intrinsic_cost(rev, tx, block.gas_limit);
    // EIP-8037: total intrinsic includes both regular and state components.
    const auto total_intrinsic = intrinsic_cost + intrinsic_state;
    if (tx.gas_limit < std::max(total_intrinsic, min_cost))
        return make_error_code(INTRINSIC_GAS_TOO_LOW);

    const auto execution_gas_limit = tx.gas_limit - total_intrinsic;
    return TransactionProperties{execution_gas_limit, intrinsic_state, intrinsic_cost, min_cost};
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
    assert(sender_acc.nonce < Account::NonceMax);  // Required for valid tx.
    ++sender_acc.nonce;                            // Bump sender nonce.

    const auto delegation_refund =
        process_authorization_list(state, tx.chain_id, tx.authorization_list, rev,
            rev >= EVMC_AMSTERDAM ? evmone::compute_cpsb(block.gas_limit) : int64_t{0});

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

    sender_acc.access_status = EVMC_ACCESS_WARM;  // Tx sender is always warm.
    if (tx.to.has_value())
        host.access_account(*tx.to);
    for (const auto& [a, storage_keys] : tx.access_list)
    {
        host.access_account(a);
        for (const auto& key : storage_keys)
            state.get_storage(a, key).access_status = EVMC_ACCESS_WARM;
    }
    // EIP-3651: Warm COINBASE.
    // This may create an empty coinbase account. The account cannot be created unconditionally
    // because this breaks old revisions.
    if (rev >= EVMC_SHANGHAI)
        host.access_account(block.coinbase);

    auto message = build_message(tx, tx_props.execution_gas_limit);
    if (tx.to.has_value())
    {
        if (const auto delegate = get_delegate_address(host, *tx.to))
        {
            message.code_address = *delegate;
            message.flags |= EVMC_DELEGATED;
            host.access_account(message.code_address);
        }
    }

    // EIP-8037: Always split execution gas into regular + state reservoir.
    // execution_gas already excludes both regular AND state intrinsic costs.
    //   regular = min(MAX_TX_GAS_LIMIT - intrinsic_regular, exec_gas)
    //   reservoir = exec_gas - regular
    // No further deduction — intrinsic state gas is already consumed from gas_limit.
    if (rev >= EVMC_AMSTERDAM)
    {
        const auto exec_gas = tx_props.execution_gas_limit;
        const auto regular_cap =
            std::max(int64_t{0}, static_cast<int64_t>(MAX_TX_GAS_LIMIT) -
                                     tx_props.intrinsic_regular_gas);
        const auto regular_exec = std::min(exec_gas, regular_cap);
        message.gas = regular_exec;
        message.state_gas = exec_gas - regular_exec;
        // EIP-8037: Add delegation refund to reservoir (geth: st.gasRemaining.StateGas += refund).
        // This returns the auth account-creation state gas for existing authorities.
        message.state_gas += delegation_refund;
    }

    auto result = host.call(message);

    // EIP-8037: On top-level failure (revert or exceptional halt), refund all state gas
    // consumed by EVM execution back to the reservoir, since nothing was created.
    // Exception: depth-0 CREATE collision uses state_gas_used = -1 as a sentinel;
    // the Host already preserved state_gas_left in that case.
    if (rev >= EVMC_AMSTERDAM && result.status_code != EVMC_SUCCESS &&
        result.state_gas_used > 0)
    {
        result.raw().state_gas_left += result.state_gas_used;
        result.raw().state_gas_used = 0;
    }

    // EIP-8037: Refund state gas for accounts created AND destroyed in the same
    // transaction (EIP-6780). Covers the account-creation charge, each created
    // storage slot, and the code-deposit state gas. Capped at state_gas_used.
    if (rev >= EVMC_AMSTERDAM && result.status_code == EVMC_SUCCESS)
    {
        const auto cpsb = evmone::compute_cpsb(block.gas_limit);
        int64_t selfdestruct_refund = 0;
        // m_destructed is append-only: if an inner frame reverts and an outer
        // frame later selfdestructs the same account, the address appears
        // twice. Dedupe here.
        std::unordered_set<address, decltype([](const address& a) {
            return std::hash<std::string_view>{}(
                std::string_view(reinterpret_cast<const char*>(a.bytes), sizeof(a.bytes)));
        })> seen;
        for (const auto& addr : host.get_destructed())
        {
            if (!seen.insert(addr).second)
                continue;
            const auto* acc = state.find(addr);
            if (acc == nullptr || !acc->destructed || !acc->just_created)
                continue;
            // Account creation state gas.
            selfdestruct_refund += 112 * cpsb;
            // Storage creation state gas: each slot written to a non-zero
            // value during this account's lifetime.
            for (const auto& [k, v] : acc->storage)
            {
                if (v.current != bytes32{})
                    selfdestruct_refund += 32 * cpsb;
            }
            // Code deposit state gas: 1 byte per CPSB.
            selfdestruct_refund += static_cast<int64_t>(acc->code.size()) * cpsb;
        }
        selfdestruct_refund = std::min(selfdestruct_refund, result.state_gas_used);
        result.raw().state_gas_left += selfdestruct_refund;
        result.raw().state_gas_used -= selfdestruct_refund;
    }

    // EIP-8037: actual gas consumed = gas_limit - regular_unspent - reservoir_unspent.
    auto gas_used = tx.gas_limit - result.gas_left - result.state_gas_left;

    // EIP-7778: Block-level gas components for Amsterdam.
    int64_t amsterdam_regular_gas = 0;
    int64_t amsterdam_state_gas = 0;

    if (rev >= EVMC_AMSTERDAM)
    {
        const auto total_consumed = gas_used;

        // EIP-7778: block gas_used = max(sum_regular, sum_state) at block level.
        // State gas = EVM-tracked + intrinsic state gas (CREATE + auth, minus refund).
        // Regular gas = total consumed - state gas - refunds discarded at ancestor
        // reverts (EIP-8037: spilled charges whose refunds were dropped at the
        // revert boundary don't represent state growth and must not inflate the
        // regular component).
        const auto exec_state_gas = std::max(int64_t{0}, result.state_gas_used);
        const auto intrinsic_state = tx_props.intrinsic_state_gas;
        // EIP-7778: tx_state for block metering uses GROSS intrinsic state gas
        // (EELS: `tx_state_gas = intrinsic_state_gas + exec_state_gas_used`).
        // Delegation refund reduces sender's payment (returned via reservoir)
        // but does not reduce the block-level state component.
        const auto state_gas_used = exec_state_gas + intrinsic_state;
        const auto refund_discarded = std::max(int64_t{0}, result.state_gas_refund_discarded);
        // Regular gas = total consumed + delegation_refund - gross intrinsic state
        // - exec state - refund_discarded.
        //
        // Special case: on collision/early-failure (gas_left=0, state_gas_used=0,
        // state_gas_left preserved), no EVM execution happened. The burned gas is
        // not categorized as regular or state. Regular = just intrinsic.
        // Detect collision/early-failure: EVMC_FAILURE with no state gas used and
        // gas burned. On collision, Host::create returns EVMC_FAILURE (not OOG/REVERT)
        // with gas_left=0, state_gas_used=0, reservoir preserved.
        // Detect depth-0 collision: marked by Host with state_gas_used = -1.
        const auto is_collision = (result.state_gas_used == -1);
        const auto regular_gas = is_collision ?
            tx_props.intrinsic_regular_gas :
            std::max(int64_t{0}, total_consumed + delegation_refund - intrinsic_state -
                                     exec_state_gas - refund_discarded);
        const auto block_gas = std::max(regular_gas, state_gas_used);

        // Store components for block-level aggregation.
        amsterdam_regular_gas = std::max(regular_gas, tx_props.min_gas_cost);
        amsterdam_state_gas = state_gas_used;

        // Refund: based on total consumed (auth gas is included in refund base).
        const auto refund_limit = total_consumed / 5;
        const auto refund = std::min(result.gas_refund, refund_limit);

        // EIP-7623: per-tx block gas = max(max(regular, state), min_gas_cost).
        // Used for block_gas_left. Block header uses max(sum_regular, sum_state) in runner.
        gas_used = std::max(block_gas, tx_props.min_gas_cost);

        // Sender pays: total consumed minus gas refund.
        // Delegation refund is already in the reservoir (returned as state_gas_left),
        // reducing total_consumed. No separate subtraction needed.
        const auto sender_total = std::max(total_consumed, tx_props.min_gas_cost);
        const auto sender_gas_cost = std::max(sender_total - refund, tx_props.min_gas_cost);
        sender_acc.balance += tx_max_cost - sender_gas_cost * effective_gas_price;
        state.touch(block.coinbase).balance += sender_gas_cost * priority_gas_price;
    }
    else
    {
        const auto max_refund_quotient = rev >= EVMC_LONDON ? 5 : 2;
        const auto refund_limit = gas_used / max_refund_quotient;
        const auto refund = std::min(delegation_refund + result.gas_refund, refund_limit);
        gas_used -= refund;
        assert(gas_used > 0);

        // EIP-7623: The gas used by the transaction must be at least the min_gas_cost.
        gas_used = std::max(gas_used, tx_props.min_gas_cost);

        sender_acc.balance += tx_max_cost - gas_used * effective_gas_price;
        state.touch(block.coinbase).balance += gas_used * priority_gas_price;
    }

    // Cumulative gas used is unknown in this scope.
    TransactionReceipt receipt{};
    receipt.type = tx.type;
    receipt.status = result.status_code;
    if (rev >= EVMC_AMSTERDAM)
    {
        // Receipt gas_used = sender gas cost including calldata floor.
        const auto tc = tx.gas_limit - result.gas_left - result.state_gas_left;
        const auto regular_refund_limit = tc / 5;
        const auto regular_refund = std::min(result.gas_refund, regular_refund_limit);
        const auto sender_total = std::max(tc, tx_props.min_gas_cost);
        receipt.gas_used = std::max(sender_total - regular_refund, tx_props.min_gas_cost);
    }
    else
    {
        receipt.gas_used = gas_used;
    }
    receipt.block_gas_used = gas_used;  // For block header gas accounting (per-tx max).
    receipt.regular_block_gas = amsterdam_regular_gas;
    receipt.state_block_gas = amsterdam_state_gas;
    receipt.logs = host.take_logs();

    // EIP-7708: Emit burn logs for destructed accounts with remaining balance,
    // in lexicographical order of account address.
    if (rev >= EVMC_AMSTERDAM)
    {
        std::vector<std::pair<address, intx::uint256>> burn_entries;
        for (const auto& addr : host.get_destructed())
        {
            const auto* acc = state.find(addr);
            if (acc != nullptr && acc->destructed && acc->balance != 0)
                burn_entries.emplace_back(addr, acc->balance);
        }
        std::ranges::sort(burn_entries, {}, &std::pair<address, intx::uint256>::first);
        for (const auto& [addr, balance] : burn_entries)
            emit_burn_log(receipt.logs, addr, balance);
    }
    receipt.logs_bloom_filter = compute_bloom_filter(receipt.logs);
    receipt.state_diff = state.build_diff(rev);

    return receipt;
}
}  // namespace evmone::state

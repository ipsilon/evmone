// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2022 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "host.hpp"
#include "precompiles.hpp"
#include "system_contracts.hpp"
#include <evmone/constants.hpp>
#include <evmone/state_gas.hpp>

namespace evmone::state
{
namespace
{
/// EIP-8037: set the state-gas fields on a returned Result. `used` is not stored;
/// the caller derives it as `initial - left + spilled`.
void set_state_gas(evmc::Result& r, int64_t left, int64_t spilled) noexcept
{
    r.state_gas_left = left;
    r.state_gas_spilled = spilled;
}
}  // namespace

bool Host::account_exists(const address& addr) const noexcept
{
    const auto* const acc = m_state.find(addr);
    return acc != nullptr && (m_rev < EVMC_SPURIOUS_DRAGON || !acc->is_empty());
}

bytes32 Host::get_storage(const address& addr, const bytes32& key) const noexcept
{
    return m_state.get_storage(addr, key).current;
}

evmc_storage_status Host::set_storage(
    const address& addr, const bytes32& key, const bytes32& value) noexcept
{
    // Follow EVMC documentation https://evmc.ethereum.org/storagestatus.html#autotoc_md3
    // and EIP-2200 specification https://eips.ethereum.org/EIPS/eip-2200.

    auto& storage_slot = m_state.get_storage(addr, key);
    const auto& [current, original, _] = storage_slot;

    const auto dirty = original != current;
    const auto restored = original == value;
    const auto current_is_zero = is_zero(current);
    const auto value_is_zero = is_zero(value);

    auto status = EVMC_STORAGE_ASSIGNED;  // All other cases.
    if (!dirty && !restored)
    {
        if (current_is_zero)
            status = EVMC_STORAGE_ADDED;  // 0 → 0 → Z
        else if (value_is_zero)
            status = EVMC_STORAGE_DELETED;  // X → X → 0
        else
            status = EVMC_STORAGE_MODIFIED;  // X → X → Z
    }
    else if (dirty && !restored)
    {
        if (current_is_zero && !value_is_zero)
            status = EVMC_STORAGE_DELETED_ADDED;  // X → 0 → Z
        else if (!current_is_zero && value_is_zero)
            status = EVMC_STORAGE_MODIFIED_DELETED;  // X → Y → 0
    }
    else if (dirty)
    {
        assert(restored);  // Always true.
        if (current_is_zero)
            status = EVMC_STORAGE_DELETED_RESTORED;  // X → 0 → X
        else if (value_is_zero)
            status = EVMC_STORAGE_ADDED_DELETED;  // 0 → Y → 0
        else
            status = EVMC_STORAGE_MODIFIED_RESTORED;  // X → Y → X
    }

    // In Berlin this is handled in access_storage().
    if (m_rev < EVMC_BERLIN)
        m_state.journal_storage_change(addr, key, storage_slot);
    storage_slot.current = value;  // Update current value.
    return status;
}

uint256be Host::get_balance(const address& addr) const noexcept
{
    const auto* const acc = m_state.find(addr);
    return (acc != nullptr) ? intx::be::store<uint256be>(acc->balance) : uint256be{};
}

uint64_t Host::get_nonce(const address& addr) const noexcept
{
    const auto* const acc = m_state.find(addr);
    return (acc != nullptr) ? acc->nonce : 0;
}

namespace
{
/// Check if an existing account is the "create collision"
/// as defined in the [EIP-7610](https://eips.ethereum.org/EIPS/eip-7610).
[[nodiscard]] bool is_create_collision(const Account& acc) noexcept
{
    // TODO: This requires much more testing:
    // - what if an account had storage but is destructed?
    // - what if an account had cold storage but it was emptied?
    // - what if an account without cold storage gain one?
    if (acc.nonce != 0)
        return true;
    if (acc.code_hash != Account::EMPTY_CODE_HASH)
        return true;
    if (acc.has_initial_storage)
        return true;

    // The hot storage is ignored because it can contain elements from access list.
    // TODO: Is this correct for destructed accounts?
    assert(!acc.destructed && "untested");
    return false;
}
}  // namespace

size_t Host::get_code_size(const address& addr) const noexcept
{
    const auto raw_code = m_state.get_code(addr);
    return raw_code.size();
}

bytes32 Host::get_code_hash(const address& addr) const noexcept
{
    const auto* const acc = m_state.find(addr);
    if (acc == nullptr || acc->is_empty())
        return {};

    return acc->code_hash;
}

size_t Host::copy_code(const address& addr, size_t code_offset, uint8_t* buffer_data,
    size_t buffer_size) const noexcept
{
    const auto code = m_state.get_code(addr);
    const auto code_slice = code.substr(std::min(code_offset, code.size()));
    const auto num_bytes = std::min(buffer_size, code_slice.size());
    std::copy_n(code_slice.begin(), num_bytes, buffer_data);
    return num_bytes;
}

bool Host::selfdestruct(const address& addr, const address& beneficiary) noexcept
{
    if (m_state.find(beneficiary) == nullptr)
        m_state.journal_create(beneficiary, false);
    auto& acc = m_state.get(addr);
    const auto balance = acc.balance;
    auto& beneficiary_acc = m_state.touch(beneficiary);

    m_state.journal_balance_change(beneficiary, beneficiary_acc.balance);
    m_state.journal_balance_change(addr, balance);

    if (m_rev >= EVMC_CANCUN && !acc.just_created)
    {
        // EIP-6780:
        // "SELFDESTRUCT is executed in a transaction that is not the same
        // as the contract invoking SELFDESTRUCT was created"
        acc.balance = 0;
        beneficiary_acc.balance += balance;  // Keep balance if acc is the beneficiary.

        if (m_rev >= EVMC_AMSTERDAM)
            emit_transfer_log(m_logs, addr, beneficiary, balance);

        // Return "selfdestruct not registered".
        // In practice this affects only refunds before Cancun.
        return false;
    }

    if (m_rev < EVMC_AMSTERDAM || beneficiary != addr)
    {
        // Transfer may happen multiple times per single account as account's balance
        // can be increased with a call following previous selfdestruct.
        beneficiary_acc.balance += balance;
        acc.balance = 0;  // Zero balance if acc is the beneficiary (before EIP-8246)
    }

    if (m_rev >= EVMC_AMSTERDAM)
        emit_transfer_log(m_logs, addr, beneficiary, balance);

    // Mark the destruction if not done already.
    if (!acc.destructed)
    {
        m_state.journal_destruct(addr);
        acc.destructed = true;
        return true;
    }
    return false;
}

evmc::Result Host::create(const evmc_message& msg) noexcept
{
    assert(msg.kind == EVMC_CREATE || msg.kind == EVMC_CREATE2);
    // The VM computes the created account's address (and warms it, EIP-2929) and
    // performs the EIP-2681 nonce-overflow check before entering the create frame.
    assert(msg.recipient != address{});

    auto* new_acc = m_state.find(msg.recipient);
    const bool new_acc_exists = new_acc != nullptr;
    // EIP-8037 (EELS #3126): the created account's NEW_ACCOUNT state gas is charged at this access
    // when the deployment address's leaf is empty (per EIP-161). Captured before any mutation.
    const bool target_empty = !new_acc_exists || new_acc->is_empty();
    if (!new_acc_exists)
        new_acc = &m_state.insert(msg.recipient);
    else if (is_create_collision(*new_acc))
    {
        auto r = evmc::Result{EVMC_FAILURE};
        // Preserve reservoir so the parent (or transition() at depth 0) can refund
        // any unused state gas. No execution happened, so the derived state gas
        // used is 0.
        r.state_gas_left = msg.state_gas;
        return r;
    }
    m_state.journal_create(msg.recipient, new_acc_exists);

    assert(new_acc != nullptr);
    assert(new_acc->nonce == 0);

    if (m_rev >= EVMC_SPURIOUS_DRAGON)
        new_acc->nonce = 1;  // No need to journal: create revert will 0 the nonce.

    new_acc->just_created = true;

    auto& sender_acc = m_state.get(msg.sender);  // TODO: Duplicated account lookup.
    const auto value = intx::be::load<intx::uint256>(msg.value);
    assert(sender_acc.balance >= value && "EVM must guarantee balance");
    m_state.journal_balance_change(msg.sender, sender_acc.balance);
    m_state.journal_balance_change(msg.recipient, new_acc->balance);
    sender_acc.balance -= value;
    new_acc->balance += value;  // The new account may be prefunded.

    if (m_rev >= EVMC_AMSTERDAM)
        emit_transfer_log(m_logs, msg.sender, msg.recipient, value);

    auto create_msg = msg;
    create_msg.input_data = nullptr;
    create_msg.input_size = 0;

    // EIP-8037 charge-at-access (EELS #3126): the depth-0 tx-level create charges the created
    // account's NEW_ACCOUNT state gas here (the opcode CREATE charges it in create_impl). Draw from
    // the reservoir, spilling into the frame's regular gas; refunded below if no account persists.
    bool new_account_charged = false;
    int64_t new_account_spilled = 0;    // Portion drawn from the frame's regular gas.
    int64_t new_account_reservoir = 0;  // Portion drawn from the state-gas reservoir.
    if (m_rev >= EVMC_AMSTERDAM && msg.depth == 0 && target_empty)
    {
        StateGas sg{.left = create_msg.state_gas};
        if (!sg.charge(create_msg.gas, NEW_ACCOUNT_STATE_GAS))
        {
            auto r = evmc::Result{EVMC_OUT_OF_GAS};
            r.state_gas_left = msg.state_gas;  // no account created: refill the entry reservoir
            return r;
        }
        new_account_charged = true;
        new_account_spilled = sg.spilled;
        new_account_reservoir = create_msg.state_gas - sg.left;
        create_msg.state_gas = sg.left;
    }

    const bytes_view initcode{msg.input_data, msg.input_size};
    auto result = m_vm.execute(*this, m_rev, create_msg, initcode.data(), initcode.size());
    if (result.status_code != EVMC_SUCCESS)
    {
        // No account created: refund the NEW_ACCOUNT charge. The reservoir portion is always
        // restored (no state persisted); the spilled portion returns to gas only on a revert — an
        // exceptional halt consumes it as regular gas (matches EELS refill_frame_state_gas then
        // gas_left = 0). The initcode frame already refilled its own state gas at its boundary.
        if (new_account_charged)
        {
            result.state_gas_left += new_account_reservoir;
            if (result.status_code == EVMC_REVERT)
                result.gas_left += new_account_spilled;
        }
        return result;
    }

    auto gas_left = result.gas_left;
    assert(gas_left >= 0);

    const bytes_view code{result.output_data, result.output_size};

    // EIP-3541: Reject new contract code starting with the 0xEF byte.
    // Checked before the size check and code deposit gas (per EELS) to avoid
    // charging for rejected code.
    if (m_rev >= EVMC_LONDON && code.starts_with(0xEF))
    {
        auto r = evmc::Result{EVMC_CONTRACT_VALIDATION_FAILURE};
        r.state_gas_left = msg.state_gas;  // refill the full reservoir (nothing persists)
        return r;
    }

    // EIP-7954: Amsterdam increases max code size.
    // Checked before code deposit gas (per geth) to avoid inflating state gas.
    const auto max_code_size = m_rev >= EVMC_AMSTERDAM ? MAX_CODE_SIZE_AMSTERDAM : MAX_CODE_SIZE;
    if (m_rev >= EVMC_SPURIOUS_DRAGON && code.size() > static_cast<size_t>(max_code_size))
    {
        auto r = evmc::Result{EVMC_FAILURE};
        r.state_gas_left = msg.state_gas;  // refill the full reservoir (nothing persists)
        return r;
    }

    // Code deployment cost. Continue the init frame's state gas (left + spill), carrying the
    // NEW_ACCOUNT charge's spill so the created account's state gas is reported on success.
    StateGas state_gas{.left = result.state_gas_left,
        .spilled = new_account_spilled + result.state_gas_spilled};
    if (m_rev >= EVMC_AMSTERDAM)
    {
        // EIP-8037: split code deposit into regular and state components.
        const auto regular_cost = 6 * ((std::ssize(code) + 31) / 32);
        const auto state_cost = std::ssize(code) * COST_PER_STATE_BYTE;
        gas_left -= regular_cost;
        if (gas_left < 0 || !state_gas.charge(gas_left, state_cost))
        {
            auto r = evmc::Result{EVMC_FAILURE};
            r.state_gas_left = msg.state_gas;  // refill the full reservoir (nothing persists)
            return r;
        }
    }
    else
    {
        const auto cost = std::ssize(code) * 200;
        gas_left -= cost;
        if (gas_left < 0)
        {
            if (m_rev == EVMC_FRONTIER)
                return evmc::Result{EVMC_SUCCESS, result.gas_left, result.gas_refund};
            auto r = evmc::Result{EVMC_FAILURE};
            r.state_gas_left = msg.state_gas;  // refill on failure
            return r;
        }
    }

    if (!code.empty())
    {
        new_acc->code_hash = keccak256(code);
        new_acc->code = code;
        new_acc->code_changed = true;
    }

    auto r = evmc::Result{result.status_code, gas_left, result.gas_refund};
    set_state_gas(r, state_gas.left, state_gas.spilled);
    return r;
}

evmc::Result Host::execute_message(const evmc_message& msg_in) noexcept
{
    if (msg_in.kind == EVMC_CREATE || msg_in.kind == EVMC_CREATE2)
        return create(msg_in);

    auto msg = msg_in;  // Mutable copy for EIP-2780 top-level gas adjustments.

    // EIP-2780: top-level (depth 0) execution charges, applied after EIP-7702 authorizations and
    // before the value transfer or any opcode, evaluated against the pre-transfer recipient state.
    // Charged here, not in the interpreter, because a value transfer to a new account runs no code
    // and so never enters the VM. An OOG returns failure; the value transfer performed below is
    // then rolled back at Host::call's revert boundary.
    // EIP-2780: `msg.state_gas` stays the entry reservoir; `top_level_sg` holds the post-charge
    // pools (left/spilled) so a consuming path can commit the NEW_ACCOUNT charge on success or
    // refund it on failure. `used` is always derived as `msg.state_gas - left + spilled`.
    StateGas top_level_sg{.left = msg.state_gas};
    // Builds the failure result for a pre-execution charge OOG: all regular gas
    // is consumed and the entry reservoir is returned intact.
    const auto out_of_gas_result = [&msg] {
        evmc::Result r{EVMC_OUT_OF_GAS, 0};
        r.state_gas_left = msg.state_gas;
        return r;
    };
    if (m_rev >= EVMC_AMSTERDAM && msg.depth == 0)
    {
        const auto* const recipient_acc = m_state.find(msg.recipient);
        const auto recipient_alive = recipient_acc != nullptr && !recipient_acc->is_empty();
        if (!evmc::is_zero(msg.value) && !recipient_alive)
        {
            // A new account is materialized by the value transfer: pay NEW_ACCOUNT state gas.
            // This includes a previously-zero-balance precompile (EIP-2780/EIP-161): funding it
            // creates a state account just like any other recipient.
            if (!top_level_sg.charge(msg.gas, NEW_ACCOUNT_STATE_GAS))
                return out_of_gas_result();  // Reservoir untouched (atomic charge failure).
        }
    }

    if (msg.kind == EVMC_CALL)
    {
        const auto exists = m_state.find(msg.recipient) != nullptr;
        if (!exists)
            m_state.journal_create(msg.recipient, exists);

        if (evmc::is_zero(msg.value))
            m_state.touch(msg.recipient);
        else
        {
            // We skip touching if we send value, because account cannot end up empty.
            // It will either have value, or code that transfers this value out, or will be
            // selfdestructed anyway.
            auto& dst_acc = m_state.get_or_insert(msg.recipient);

            // Transfer value: sender → recipient.
            // The sender's balance is already checked therefore the sender account must exist.
            const auto value = intx::be::load<intx::uint256>(msg.value);
            assert(m_state.get(msg.sender).balance >= value);
            m_state.journal_balance_change(msg.sender, m_state.get(msg.sender).balance);
            m_state.journal_balance_change(msg.recipient, dst_acc.balance);
            m_state.get(msg.sender).balance -= value;
            dst_acc.balance += value;

            if (m_rev >= EVMC_AMSTERDAM)
                emit_transfer_log(m_logs, msg.sender, msg.recipient, value);
        }
    }

    // Calls to precompile address via EIP-7702 delegation execute empty code instead of precompile.
    if ((msg.flags & EVMC_DELEGATED) == 0 && is_precompile(m_rev, msg.code_address))
    {
        auto r = call_precompile(m_rev, msg);
        // EIP-8037/2780: precompiles consume no execution state gas, but a value transfer funding a
        // zero-balance precompile paid NEW_ACCOUNT state gas above (top_level_sg). On success the
        // account persists, so commit the charge (it lands in the block state dimension). On an
        // exceptional-halt failure nothing persists, so refund it by restoring the entry reservoir,
        // exactly as a normal frame does on halt — the derived net state used is then 0. Any
        // spilled portion was taken from msg.gas and is burned with the failed call's gas.
        if (r.status_code == EVMC_SUCCESS)
            set_state_gas(r, top_level_sg.left, top_level_sg.spilled);
        else
            r.state_gas_left = msg.state_gas;
        return r;
    }

    // TODO: get_code() performs the account lookup. Add a way to get an account with code?
    const auto code = m_state.get_code(msg.code_address);
    if (code.empty())
    {
        auto r = evmc::Result{EVMC_SUCCESS, msg.gas};  // Skip trivial execution.
        // EIP-8037: empty-code call consumes no execution state gas. EIP-2780: a depth-0 value
        // transfer to a new account runs no code but paid its NEW_ACCOUNT state gas above — commit
        // those pools (a no-op when nothing was charged); the caller derives the net used.
        set_state_gas(r, top_level_sg.left, top_level_sg.spilled);
        return r;
    }

    return m_vm.execute(*this, m_rev, msg, code.data(), code.size());
}

evmc::Result Host::call(const evmc_message& msg) noexcept
{
    // Bump the creator's nonce outside the creation's rollback scope: per the
    // Yellow Paper the increment survives any failure of the creation itself
    // (collision or initcode failure). The VM checked EIP-2681 and computed the
    // create address from the pre-bump value; at depth 0 the transaction
    // processing has already bumped the sender.
    if (msg.depth != 0 && (msg.kind == EVMC_CREATE || msg.kind == EVMC_CREATE2))
    {
        auto& sender_acc = m_state.get(msg.sender);
        assert(sender_acc.nonce != Account::NonceMax);
        m_state.journal_bump_nonce(msg.sender);
        ++sender_acc.nonce;
    }

    const auto logs_checkpoint = m_logs.size();
    const auto state_checkpoint = m_state.checkpoint();

    auto result = execute_message(msg);

    if (result.status_code != EVMC_SUCCESS)
    {
        static constexpr auto addr_03 = 0x03_address;
        auto* const acc_03 = m_state.find(addr_03);
        const auto is_03_touched = acc_03 != nullptr && acc_03->erase_if_empty;

        // Revert.
        m_state.rollback(state_checkpoint);
        m_logs.resize(logs_checkpoint);

        // The 0x03 quirk: the touch on this address is never reverted.
        if (is_03_touched && m_rev >= EVMC_SPURIOUS_DRAGON)
            m_state.touch(addr_03);
    }
    return result;
}

evmc_tx_context Host::get_tx_context() const noexcept
{
    // TODO: The effective gas price is already computed in transaction validation.
    // TODO: The effective gas price calculation is broken for system calls (gas price 0).
    assert(m_tx.max_gas_price >= m_block.base_fee || m_tx.max_gas_price == 0);
    const auto priority_gas_price =
        std::min(m_tx.max_priority_gas_price, m_tx.max_gas_price - m_block.base_fee);
    const auto effective_gas_price = m_block.base_fee + priority_gas_price;

    return evmc_tx_context{
        intx::be::store<uint256be>(effective_gas_price),  // By EIP-1559.
        m_tx.sender,
        m_block.coinbase,
        m_block.number,
        m_block.timestamp,
        m_block.gas_limit,
        m_block.prev_randao,
        0x01_bytes32,  // Chain ID is expected to be 1.
        uint256be{m_block.base_fee},
        intx::be::store<uint256be>(m_block.blob_base_fee.value_or(0)),
        m_tx.blob_hashes.data(),
        m_tx.blob_hashes.size(),
        m_block.slot_number,
    };
}

bytes32 Host::get_block_hash(int64_t block_number) const noexcept
{
    return m_block_hashes.get_block_hash(block_number);
}

void Host::emit_log(const address& addr, const uint8_t* data, size_t data_size,
    const bytes32 topics[], size_t topics_count) noexcept
{
    m_logs.push_back({addr, {data, data_size}, {topics, topics + topics_count}});
}

evmc_access_status Host::access_account(const address& addr) noexcept
{
    if (m_rev < EVMC_BERLIN)
        return EVMC_ACCESS_COLD;  // Ignore before Berlin.

    auto& acc = m_state.get_or_insert(addr, {.erase_if_empty = true});

    if (acc.access_status == EVMC_ACCESS_WARM || is_precompile(m_rev, addr))
        return EVMC_ACCESS_WARM;

    m_state.journal_access_account(addr);
    acc.access_status = EVMC_ACCESS_WARM;
    return EVMC_ACCESS_COLD;
}

evmc_access_status Host::access_storage(const address& addr, const bytes32& key) noexcept
{
    auto& storage_slot = m_state.get_storage(addr, key);
    m_state.journal_storage_change(addr, key, storage_slot);
    return std::exchange(storage_slot.access_status, EVMC_ACCESS_WARM);
}


evmc::bytes32 Host::get_transient_storage(const address& addr, const bytes32& key) const noexcept
{
    const auto& acc = m_state.get(addr);
    const auto it = acc.transient_storage.find(key);
    return it != acc.transient_storage.end() ? it->second : bytes32{};
}

void Host::set_transient_storage(
    const address& addr, const bytes32& key, const bytes32& value) noexcept
{
    auto& slot = m_state.get(addr).transient_storage[key];
    m_state.journal_transient_storage_change(addr, key, slot);
    slot = value;
}
}  // namespace evmone::state

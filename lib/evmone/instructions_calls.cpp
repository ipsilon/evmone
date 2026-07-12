// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "constants.hpp"
#include "create_address.hpp"
#include "delegation.hpp"
#include "instructions.hpp"
#include <limits>
#include <variant>

constexpr int64_t CALL_VALUE_COST = 9000;
constexpr int64_t ACCOUNT_CREATION_COST = 25000;

namespace evmone::instr::core
{
namespace
{
/// Get target address of a code executing instruction.
///
/// Returns EIP-7702 delegate address if addr is delegated, or addr itself otherwise.
/// Applies gas charge for accessing delegate account and may fail with out of gas.
inline std::variant<evmc::address, Result> get_target_address(
    const evmc::address& addr, int64_t& gas_left, ExecutionState& state) noexcept
{
    if (state.rev < EVMC_PRAGUE)
        return addr;

    const auto delegate_addr = get_delegate_address(state.host, addr);
    if (!delegate_addr)
        return addr;

    const auto delegate_account_access_cost =
        (state.host.access_account(*delegate_addr) == EVMC_ACCESS_COLD ?
                instr::cold_account_access_cost :
                instr::warm_storage_read_cost);

    if ((gas_left -= delegate_account_access_cost) < 0)
        return Result{EVMC_OUT_OF_GAS, gas_left};

    return *delegate_addr;
}
}  // namespace

/// Converts an opcode to matching EVMC call kind.
/// NOLINTNEXTLINE(misc-use-internal-linkage) fixed in clang-tidy 20.
consteval evmc_call_kind to_call_kind(Opcode op) noexcept
{
    switch (op)
    {
    case OP_CALL:
    case OP_STATICCALL:
        return EVMC_CALL;
    case OP_CALLCODE:
        return EVMC_CALLCODE;
    case OP_DELEGATECALL:
        return EVMC_DELEGATECALL;
    case OP_CREATE:
        return EVMC_CREATE;
    case OP_CREATE2:
        return EVMC_CREATE2;
    default:
        intx::unreachable();
    }
}

template <Opcode Op>
Result call_impl(StackTop stack, int64_t gas_left, ExecutionState& state) noexcept
{
    static_assert(
        Op == OP_CALL || Op == OP_CALLCODE || Op == OP_DELEGATECALL || Op == OP_STATICCALL);

    static constexpr bool HAS_VALUE_ARG = Op == OP_CALL || Op == OP_CALLCODE;

    const auto gas = stack.pop();
    const auto dst = intx::be::trunc<evmc::address>(stack.pop());
    const auto value = (!HAS_VALUE_ARG) ? 0 : stack.pop();
    const auto has_value = value != 0;
    const auto input_offset_u256 = stack.pop();
    const auto input_size_u256 = stack.pop();
    const auto output_offset_u256 = stack.pop();
    const auto output_size_u256 = stack.pop();

    stack.push(0);  // Assume failure.
    state.return_data.clear();

    if (state.rev >= EVMC_BERLIN && state.host.access_account(dst) == EVMC_ACCESS_COLD)
    {
        if ((gas_left -= instr::additional_cold_account_access_cost) < 0)
            return {EVMC_OUT_OF_GAS, gas_left};
    }

    const auto target_addr_or_result = get_target_address(dst, gas_left, state);
    if (const auto* result = std::get_if<Result>(&target_addr_or_result))
        return *result;

    const auto& code_addr = std::get<evmc::address>(target_addr_or_result);

    if (!check_memory(gas_left, state.memory, input_offset_u256, input_size_u256))
        return {EVMC_OUT_OF_GAS, gas_left};

    if (!check_memory(gas_left, state.memory, output_offset_u256, output_size_u256))
        return {EVMC_OUT_OF_GAS, gas_left};

    const auto input_offset = static_cast<size_t>(input_offset_u256);
    const auto input_size = static_cast<size_t>(input_size_u256);
    const auto output_offset = static_cast<size_t>(output_offset_u256);
    const auto output_size = static_cast<size_t>(output_size_u256);

    evmc_message msg{.kind = to_call_kind(Op)};
    msg.flags = (Op == OP_STATICCALL) ? uint32_t{EVMC_STATIC} : state.msg->flags;
    if (dst != code_addr)
        msg.flags |= EVMC_DELEGATED;
    else
        msg.flags &= ~std::underlying_type_t<evmc_flags>{EVMC_DELEGATED};
    msg.depth = state.msg->depth + 1;
    msg.recipient = (Op == OP_CALL || Op == OP_STATICCALL) ? dst : state.msg->recipient;
    msg.code_address = code_addr;
    msg.sender = (Op == OP_DELEGATECALL) ? state.msg->sender : state.msg->recipient;
    msg.value =
        (Op == OP_DELEGATECALL) ? state.msg->value : intx::be::store<evmc::uint256be>(value);

    if (input_size > 0)
    {
        // input_offset may be garbage if input_size == 0.
        msg.input_data = &state.memory[input_offset];
        msg.input_size = input_size;
    }

    // EIP-8037: state gas for creating the called account (value-CALL to a nonexistent account).
    // Tracked at function scope so it can be refunded on every non-success exit below — a light
    // failure or child revert/halt undoes the account creation (matches EELS generic_call).
    int64_t new_account_state_gas = 0;
    const auto refund_new_account_state_gas = [&]() noexcept {
        if (new_account_state_gas != 0)
            credit_state_gas_refund(gas_left, state, new_account_state_gas);
    };

    if constexpr (HAS_VALUE_ARG)
    {
        auto cost = has_value ? CALL_VALUE_COST : 0;

        if constexpr (Op == OP_CALL)
        {
            if (has_value && state.in_static_mode())
                return {EVMC_STATIC_MODE_VIOLATION, gas_left};

            if ((has_value || state.rev < EVMC_SPURIOUS_DRAGON) && !state.host.account_exists(dst))
            {
                if (state.rev >= EVMC_AMSTERDAM)
                    new_account_state_gas = NEW_ACCOUNT_STATE_GAS;  // EIP-8037: charged below.
                else
                    cost += ACCOUNT_CREATION_COST;
            }
        }

        if ((gas_left -= cost) < 0)
            return {EVMC_OUT_OF_GAS, gas_left};

        // EIP-8037: the state-gas account-creation charge must come AFTER the regular cost is
        // committed (reservoir model), so a regular OOG can't leave committed state growth behind.
        if (new_account_state_gas != 0 && !charge_state_gas(gas_left, state, new_account_state_gas))
            return {EVMC_OUT_OF_GAS, gas_left};
    }

    msg.gas = std::numeric_limits<int64_t>::max();
    if (gas < msg.gas)
        msg.gas = static_cast<int64_t>(gas);

    if constexpr (Op == OP_STATICCALL)
    {
        msg.gas = std::min(msg.gas, gas_left - gas_left / 64);
    }
    else
    {
        if (state.rev >= EVMC_TANGERINE_WHISTLE)  // Always true for STATICCALL.
            msg.gas = std::min(msg.gas, gas_left - gas_left / 64);
        else if (msg.gas > gas_left)
            return {EVMC_OUT_OF_GAS, gas_left};
    }

    if constexpr (HAS_VALUE_ARG)
    {
        if (has_value)
        {
            msg.gas += 2300;  // Add stipend.
            gas_left += 2300;
            if (intx::be::load<uint256>(state.host.get_balance(state.msg->recipient)) < value)
            {
                refund_new_account_state_gas();   // No transfer, so no account created.
                return {EVMC_SUCCESS, gas_left};  // "Light" failure.
            }
        }
    }

    if (state.msg->depth >= 1024)
    {
        refund_new_account_state_gas();   // Child never runs, so no account created.
        return {EVMC_SUCCESS, gas_left};  // "Light" failure.
    }

    // EIP-8037: pass state gas to child execution.
    msg.state_gas = state.state_gas.left;

    const auto result = state.host.call(msg);
    state.return_data.assign(result.output_data, result.output_size);
    stack.top() = result.status_code == EVMC_SUCCESS;

    if (const auto copy_size = std::min(output_size, result.output_size); copy_size > 0)
        std::memcpy(&state.memory[output_offset], result.output_data, copy_size);

    const auto gas_used = msg.gas - result.gas_left;
    gas_left -= gas_used;
    state.gas_refund += result.gas_refund;
    // EIP-8037: thread the child's state gas back (leftover reservoir + accumulated spill); on
    // child failure the created account is rolled back, so refund the NEW_ACCOUNT charge.
    accumulate_child_state_gas(state, result);
    if (result.status_code != EVMC_SUCCESS)
        refund_new_account_state_gas();
    return {EVMC_SUCCESS, gas_left};
}

template Result call_impl<OP_CALL>(
    StackTop stack, int64_t gas_left, ExecutionState& state) noexcept;
template Result call_impl<OP_STATICCALL>(
    StackTop stack, int64_t gas_left, ExecutionState& state) noexcept;
template Result call_impl<OP_DELEGATECALL>(
    StackTop stack, int64_t gas_left, ExecutionState& state) noexcept;
template Result call_impl<OP_CALLCODE>(
    StackTop stack, int64_t gas_left, ExecutionState& state) noexcept;

template <Opcode Op>
Result create_impl(StackTop stack, int64_t gas_left, ExecutionState& state) noexcept
{
    static_assert(Op == OP_CREATE || Op == OP_CREATE2);

    if (state.in_static_mode())
        return {EVMC_STATIC_MODE_VIOLATION, gas_left};

    const auto endowment = stack.pop();
    const auto init_code_offset_u256 = stack.pop();
    const auto init_code_size_u256 = stack.pop();
    const auto salt = (Op == OP_CREATE2) ? stack.pop() : uint256{};

    stack.push(0);  // Assume failure.
    state.return_data.clear();

    if (!check_memory(gas_left, state.memory, init_code_offset_u256, init_code_size_u256))
        return {EVMC_OUT_OF_GAS, gas_left};

    const auto init_code_offset = static_cast<size_t>(init_code_offset_u256);
    const auto init_code_size = static_cast<size_t>(init_code_size_u256);

    // EIP-3860 initcode size limit, raised by EIP-7954 for Amsterdam.
    const auto max_initcode_size =
        state.rev >= EVMC_AMSTERDAM ? MAX_INITCODE_SIZE_AMSTERDAM : MAX_INITCODE_SIZE;
    if (state.rev >= EVMC_SHANGHAI && init_code_size > static_cast<size_t>(max_initcode_size))
        return {EVMC_OUT_OF_GAS, gas_left};

    const auto init_code_word_cost = 6 * (Op == OP_CREATE2) + 2 * (state.rev >= EVMC_SHANGHAI);
    const auto init_code_cost = num_words(init_code_size) * init_code_word_cost;
    if ((gas_left -= init_code_cost) < 0)
        return {EVMC_OUT_OF_GAS, gas_left};

    if (state.msg->depth >= 1024)
        return {EVMC_SUCCESS, gas_left};  // "Light" failure.

    if (endowment != 0 &&
        intx::be::load<uint256>(state.host.get_balance(state.msg->recipient)) < endowment)
        return {EVMC_SUCCESS, gas_left};  // "Light" failure.

    const auto& sender = state.msg->recipient;

    // EIP-2681: creation fails when the sender's nonce is at its maximum,
    // without bumping it and without consuming the forwarded gas.
    const auto sender_nonce = state.host.get_nonce(sender);
    if (sender_nonce == std::numeric_limits<uint64_t>::max())
        return {EVMC_SUCCESS, gas_left};  // "Light" failure.

    // Compute the address of the account to be created. The Host bumps the sender's
    // nonce on create-frame entry, so CREATE uses the pre-bump value read above.
    const auto salt32 = intx::be::store<evmc::bytes32>(salt);
    const auto init_code = bytes_view{
        init_code_size > 0 ? &state.memory[init_code_offset] : nullptr, init_code_size};
    const auto create_addr = (Op == OP_CREATE) ?
                                 compute_create_address(sender, sender_nonce) :
                                 compute_create2_address(sender, salt32, init_code);

    // Per EIP-2929, access to the newly created address is never reverted:
    // warm it in the creating frame, outside the create's rollback scope.
    // Gated on Berlin like the other access_account sites (2929 is Berlin onward).
    if (state.rev >= EVMC_BERLIN)
        (void)state.host.access_account(create_addr);

    // EIP-8037 charge-at-access: charge NEW_ACCOUNT for a deployment onto a non-existent
    // (per EIP-161, not-alive) address. Decided by existence alone, after warming and before
    // the 63/64 split so a reservoir spill correctly lowers the gas forwarded to the child.
    // Refilled below when no account is created (address collision or a rolled-back initcode).
    int64_t create_state_gas_charged = 0;
    if (state.rev >= EVMC_AMSTERDAM)
    {
        const auto target_alive = state.host.get_nonce(create_addr) != 0 ||
                                  intx::be::load<uint256>(state.host.get_balance(create_addr)) != 0 ||
                                  state.host.get_code_size(create_addr) != 0;
        if (!target_alive)
        {
            create_state_gas_charged = NEW_ACCOUNT_STATE_GAS;
            if (!charge_state_gas(gas_left, state, create_state_gas_charged))
                return {EVMC_OUT_OF_GAS, gas_left};
        }
    }

    evmc_message msg{.kind = to_call_kind(Op)};
    msg.recipient = create_addr;
    msg.gas = gas_left;
    if (state.rev >= EVMC_TANGERINE_WHISTLE)
        msg.gas = msg.gas - msg.gas / 64;

    if (init_code_size > 0)
    {
        // init_code_offset may be garbage if init_code_size == 0.
        msg.input_data = &state.memory[init_code_offset];
        msg.input_size = init_code_size;
    }
    msg.sender = sender;
    msg.depth = state.msg->depth + 1;
    msg.create2_salt = salt32;
    msg.value = intx::be::store<evmc::uint256be>(endowment);

    // EIP-8037: pass state gas to child execution.
    msg.state_gas = state.state_gas.left;

    const auto result = state.host.call(msg);
    gas_left -= msg.gas - result.gas_left;
    state.gas_refund += result.gas_refund;
    // EIP-8037: take the child's leftover reservoir + accumulated spill. When no account is
    // created (rolled-back initcode or address collision — any non-success), refund the
    // NEW_ACCOUNT charge (LIFO); a create onto an already-alive account was never charged.
    accumulate_child_state_gas(state, result);
    if (create_state_gas_charged != 0 && result.status_code != EVMC_SUCCESS)
        credit_state_gas_refund(gas_left, state, create_state_gas_charged);

    state.return_data.assign(result.output_data, result.output_size);
    if (result.status_code == EVMC_SUCCESS)
        stack.top() = intx::be::load<uint256>(create_addr);

    return {EVMC_SUCCESS, gas_left};
}

template Result create_impl<OP_CREATE>(
    StackTop stack, int64_t gas_left, ExecutionState& state) noexcept;
template Result create_impl<OP_CREATE2>(
    StackTop stack, int64_t gas_left, ExecutionState& state) noexcept;
}  // namespace evmone::instr::core

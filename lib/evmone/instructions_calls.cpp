// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "constants.hpp"
#include "delegation.hpp"
#include "instructions.hpp"
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

    // EIP-7928: once the access cost is committed (no OOG), the delegate
    // address must appear in the block access list even if the CALL itself
    // light-fails (e.g. insufficient funds) without doing any other state
    // touch on it. Force the lazy-load now so the BAL StateView decorator
    // observes the read.
    (void)state.host.account_exists(*delegate_addr);

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

    // EIP-7702/EIP-7928: in a static context, a value-transferring CALL must
    // fail before touching the target's code (which the delegation lookup does
    // via copy_code), otherwise the target would be wrongly recorded in the
    // block access list.
    if constexpr (Op == OP_CALL)
    {
        if (value != 0 && state.in_static_mode())
            return {EVMC_STATIC_MODE_VIOLATION, gas_left};
    }

    if (state.rev >= EVMC_BERLIN && state.host.access_account(dst) == EVMC_ACCESS_COLD)
    {
        if ((gas_left -= instr::additional_cold_account_access_cost) < 0)
            return {EVMC_OUT_OF_GAS, gas_left};
    }

    if (!check_memory(gas_left, state.memory, input_offset_u256, input_size_u256))
        return {EVMC_OUT_OF_GAS, gas_left};

    if (!check_memory(gas_left, state.memory, output_offset_u256, output_size_u256))
        return {EVMC_OUT_OF_GAS, gas_left};

    const auto input_offset = static_cast<size_t>(input_offset_u256);
    const auto input_size = static_cast<size_t>(input_size_u256);
    const auto output_offset = static_cast<size_t>(output_offset_u256);
    const auto output_size = static_cast<size_t>(output_size_u256);

    // EIP-7928: charge the value-transfer intrinsic before any stateful lookup
    // so an OOG here does not leak the target to the BAL tracker via delegation
    // resolution or account-existence checks.
    if constexpr (HAS_VALUE_ARG)
    {
        if (has_value)
        {
            if ((gas_left -= CALL_VALUE_COST) < 0)
                return {EVMC_OUT_OF_GAS, gas_left};
        }
    }

    const auto target_addr_or_result = get_target_address(dst, gas_left, state);
    if (const auto* result = std::get_if<Result>(&target_addr_or_result))
        return *result;

    const auto& code_addr = std::get<evmc::address>(target_addr_or_result);

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

    if constexpr (HAS_VALUE_ARG)
    {
        if constexpr (Op == OP_CALL)
        {
            if ((has_value || state.rev < EVMC_SPURIOUS_DRAGON) && !state.host.account_exists(dst))
            {
                if (state.rev >= EVMC_AMSTERDAM)
                {
                    // EIP-8037: charge state gas instead of regular ACCOUNT_CREATION_COST.
                    if (!state.state_gas.charge(gas_left, NEW_ACCOUNT_STATE_GAS))
                        return {EVMC_OUT_OF_GAS, gas_left};
                }
                else if ((gas_left -= ACCOUNT_CREATION_COST) < 0)
                {
                    return {EVMC_OUT_OF_GAS, gas_left};
                }
            }
        }
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
                return {EVMC_SUCCESS, gas_left};  // "Light" failure.
        }
    }

    if (state.msg->depth >= 1024)
        return {EVMC_SUCCESS, gas_left};  // "Light" failure.

    // EIP-8037: pass state gas to child execution.
    msg.state_gas = state.state_gas.reservoir;

    const auto result = state.host.call(msg);
    state.return_data.assign(result.output_data, result.output_size);
    stack.top() = result.status_code == EVMC_SUCCESS;

    if (const auto copy_size = std::min(output_size, result.output_size); copy_size > 0)
        std::memcpy(&state.memory[output_offset], result.output_data, copy_size);

    const auto gas_used = msg.gas - result.gas_left;
    gas_left -= gas_used;
    state.gas_refund += result.gas_refund;
    // EIP-8037: take the child's leftover reservoir and accumulate its state gas
    // used. A reverted child already had its used folded back into the reservoir
    // (and zeroed) at the `Host::call` revert boundary, so success and failure
    // are handled the same way here.
    // EIP-8037 invariant: a child returns at least the reservoir it was given; the
    // surplus is exactly the regular gas it spilled into state-gas charges (S >= 0):
    //   S = result.state_gas_left + result.state_gas_used - msg.state_gas.
    assert(result.state_gas_left + result.state_gas_used >= msg.state_gas);
    state.state_gas.reservoir = result.state_gas_left;
    state.state_gas.used += result.state_gas_used;
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

    // EIP-7954: Amsterdam increases initcode size limit.
    if (state.rev >= EVMC_AMSTERDAM && init_code_size > MAX_INITCODE_SIZE_AMSTERDAM)
        return {EVMC_OUT_OF_GAS, gas_left};
    else if (state.rev >= EVMC_SHANGHAI && state.rev < EVMC_AMSTERDAM && init_code_size > 0xC000)
        return {EVMC_OUT_OF_GAS, gas_left};

    // EIP-3860/7954: regular init-code word cost. Charged BEFORE the EIP-8037
    // state-gas charge per EIP-8037 line 126 ("regular gas MUST be applied
    // first"); otherwise a state charge that succeeds via spill can leave a
    // committed `state_gas_used` even when the following regular check OOGs,
    // producing wrong block_gas_used = max(regular_sum, state_sum).
    const auto init_code_word_cost = 6 * (Op == OP_CREATE2) + 2 * (state.rev >= EVMC_SHANGHAI);
    const auto init_code_cost = num_words(init_code_size) * init_code_word_cost;
    if ((gas_left -= init_code_cost) < 0)
        return {EVMC_OUT_OF_GAS, gas_left};

    if (state.msg->depth >= 1024)
    {
        return {EVMC_SUCCESS, gas_left};  // "Light" failure.
    }

    if (endowment != 0 &&
        intx::be::load<uint256>(state.host.get_balance(state.msg->recipient)) < endowment)
    {
        return {EVMC_SUCCESS, gas_left};  // "Light" failure.
    }

    // EIP-8037: charge state gas for account creation.
    if (state.rev >= EVMC_AMSTERDAM)
    {
        if (!state.state_gas.charge(gas_left, NEW_ACCOUNT_STATE_GAS))
            return {EVMC_OUT_OF_GAS, gas_left};
    }

    evmc_message msg{.kind = to_call_kind(Op)};
    msg.gas = gas_left;
    if (state.rev >= EVMC_TANGERINE_WHISTLE)
        msg.gas = msg.gas - msg.gas / 64;

    if (init_code_size > 0)
    {
        // init_code_offset may be garbage if init_code_size == 0.
        msg.input_data = &state.memory[init_code_offset];
        msg.input_size = init_code_size;
    }
    msg.sender = state.msg->recipient;
    msg.depth = state.msg->depth + 1;
    msg.create2_salt = intx::be::store<evmc::bytes32>(salt);
    msg.value = intx::be::store<evmc::uint256be>(endowment);

    // EIP-8037: pass state gas to child execution.
    msg.state_gas = state.state_gas.reservoir;

    const auto result = state.host.call(msg);
    gas_left -= msg.gas - result.gas_left;
    state.gas_refund += result.gas_refund;
    // EIP-8037: take the child's leftover reservoir and accumulate its state gas
    // used (uniform across status — a reverted child already had its used folded
    // into the reservoir at the `Host::call` revert boundary). On failure also
    // refill this frame's own NEW_ACCOUNT pre-charge, since no account was created.
    // EIP-8037 invariant: a child returns at least the reservoir it was given; the
    // surplus is exactly the regular gas it spilled into state-gas charges (S >= 0):
    //   S = result.state_gas_left + result.state_gas_used - msg.state_gas.
    assert(result.state_gas_left + result.state_gas_used >= msg.state_gas);
    state.state_gas.reservoir = result.state_gas_left;
    state.state_gas.used += result.state_gas_used;
    if (state.rev >= EVMC_AMSTERDAM && result.status_code != EVMC_SUCCESS)
        state.state_gas.refill(NEW_ACCOUNT_STATE_GAS);

    state.return_data.assign(result.output_data, result.output_size);
    if (result.status_code == EVMC_SUCCESS)
        stack.top() = intx::be::load<uint256>(result.create_address);

    return {EVMC_SUCCESS, gas_left};
}

template Result create_impl<OP_CREATE>(
    StackTop stack, int64_t gas_left, ExecutionState& state) noexcept;
template Result create_impl<OP_CREATE2>(
    StackTop stack, int64_t gas_left, ExecutionState& state) noexcept;
}  // namespace evmone::instr::core

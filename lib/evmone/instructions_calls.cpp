// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "constants.hpp"
#include "create_address.hpp"
#include "delegation.hpp"
#include "instructions.hpp"
#include <variant>

constexpr int64_t CALL_VALUE_COST = 9000;
/// EIP-8038 redefines CALL_VALUE as ACCOUNT_WRITE + CALL_STIPEND.
constexpr int64_t CALL_VALUE_COST_AMSTERDAM =
    evmone::instr::account_write_cost_amsterdam + evmone::CALL_STIPEND;
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
                instr::cold_account_access(state.rev) :
                int64_t{instr::warm_storage_read_cost});

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
    const auto value = HAS_VALUE_ARG ? stack.pop() : 0;
    const auto has_value = value != 0;
    const auto input_offset_u256 = stack.pop();
    const auto input_size_u256 = stack.pop();
    const auto output_offset_u256 = stack.pop();
    const auto output_size_u256 = stack.pop();

    stack.push(0);  // Assume failure.
    state.return_data.clear();

    if constexpr (Op == OP_CALL)
    {
        // TODO: gas_left is used as no-op and ignored by caller. Refactor this.
        if (has_value && state.in_static_mode())
            return {EVMC_STATIC_MODE_VIOLATION, gas_left};
    }

    if (!check_memory(gas_left, state.memory, input_offset_u256, input_size_u256))
        return {EVMC_OUT_OF_GAS, gas_left};

    if (!check_memory(gas_left, state.memory, output_offset_u256, output_size_u256))
        return {EVMC_OUT_OF_GAS, gas_left};

    const auto input_offset = static_cast<size_t>(input_offset_u256);
    const auto input_size = static_cast<size_t>(input_size_u256);
    const auto output_offset = static_cast<size_t>(output_offset_u256);
    const auto output_size = static_cast<size_t>(output_size_u256);

    if constexpr (HAS_VALUE_ARG)
    {
        // EIP-8038: the value-transfer cost becomes ACCOUNT_WRITE + CALL_STIPEND.
        const auto call_value_cost =
            state.rev >= EVMC_AMSTERDAM ? CALL_VALUE_COST_AMSTERDAM : CALL_VALUE_COST;
        if (has_value && (gas_left -= call_value_cost) < 0)
            return {EVMC_OUT_OF_GAS, gas_left};
    }

    if (state.rev >= EVMC_BERLIN && state.host.access_account(dst) == EVMC_ACCESS_COLD)
    {
        if ((gas_left -= instr::additional_cold_account_access_cost(state.rev)) < 0)
            return {EVMC_OUT_OF_GAS, gas_left};
    }

    const auto target_addr_or_result = get_target_address(dst, gas_left, state);
    if (const auto* result = std::get_if<Result>(&target_addr_or_result))
        return *result;

    const auto& code_addr = std::get<evmc::address>(target_addr_or_result);

    // State gas for creating the called account, i.e. a value-CALL to a nonexistent one. Tracked
    // at function scope so every non-success exit below can refill it: a light failure or a child
    // revert/halt undoes the account creation (EIP-8037).
    int64_t new_account_state_gas = 0;
    const auto refund_new_account_state_gas = [&]() noexcept {
        if (new_account_state_gas != 0)
            state.state_gas.refill(gas_left, new_account_state_gas);
    };

    if constexpr (Op == OP_CALL)
    {
        if ((has_value || state.rev < EVMC_SPURIOUS_DRAGON) && !state.host.account_exists(dst))
        {
            if (state.rev >= EVMC_AMSTERDAM)
            {
                // The state charge comes after every regular cost of this instruction is
                // committed (reservoir model), so a regular OOG cannot leave committed
                // state growth behind.
                new_account_state_gas = NEW_ACCOUNT_STATE_GAS;
                if (!state.state_gas.charge(gas_left, new_account_state_gas))
                    return {EVMC_OUT_OF_GAS, gas_left};
            }
            else if ((gas_left -= ACCOUNT_CREATION_COST) < 0)
                return {EVMC_OUT_OF_GAS, gas_left};
        }
    }

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
            msg.gas += CALL_STIPEND;
            gas_left += CALL_STIPEND;
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

    // The reservoir passes to the child in full; the 63/64 rule applies to gas_left only
    // (EIP-8037).
    msg.state_gas = state.state_gas.left;

    const auto result = state.host.call(msg);
    state.return_data.assign(result.output_data, result.output_size);
    stack.top() = result.status_code == EVMC_SUCCESS;

    if (const auto copy_size = std::min(output_size, result.output_size); copy_size > 0)
        std::memcpy(&state.memory[output_offset], result.output_data, copy_size);

    const auto gas_used = msg.gas - result.gas_left;
    gas_left -= gas_used;
    state.gas_refund += result.gas_refund;
    // Thread the child's state gas back. A failed child rolls the created account back, so its
    // NEW_ACCOUNT charge is refilled (EIP-8037).
    accumulate_child_state_gas(gas_left, state, result);
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
    const auto salt = (Op == OP_CREATE2) ? intx::be::store<bytes32>(stack.pop()) : bytes32{};

    stack.push(0);  // Assume failure.
    state.return_data.clear();

    if (!check_memory(gas_left, state.memory, init_code_offset_u256, init_code_size_u256))
        return {EVMC_OUT_OF_GAS, gas_left};

    const auto init_code_offset = static_cast<size_t>(init_code_offset_u256);
    const auto init_code_size = static_cast<size_t>(init_code_size_u256);

    const size_t max_init_code_size =
        state.rev >= EVMC_AMSTERDAM ? MAX_INITCODE_SIZE_AMSTERDAM : MAX_INITCODE_SIZE;
    if (state.rev >= EVMC_SHANGHAI && init_code_size > max_init_code_size)
        return {EVMC_OUT_OF_GAS, gas_left};

    // EIP-3860/7954: regular init-code word cost. Charged BEFORE the EIP-8037 state-gas
    // charge (reservoir model): regular gas is committed against gas_left first, so a
    // state charge that succeeds via spill cannot leave committed state growth behind a
    // subsequent regular OOG, which would otherwise inflate the block's state component
    // in max(regular, state).
    const auto init_code_word_cost = 6 * (Op == OP_CREATE2) + 2 * (state.rev >= EVMC_SHANGHAI);
    const auto init_code_cost = num_words(init_code_size) * init_code_word_cost;
    if ((gas_left -= init_code_cost) < 0)
        return {EVMC_OUT_OF_GAS, gas_left};

    // EIP-8037 charge-at-access (execution-specs #3116): the NEW_ACCOUNT state gas is not
    // charged up front. The pre-access light failures below therefore never charge it; the
    // charge is applied later, conditionally, at the deployment-address access.

    if (state.msg->depth >= 1024)
        return {EVMC_SUCCESS, gas_left};  // "Light" failure.

    if (endowment != 0 &&
        intx::be::load<uint256>(state.host.get_balance(state.msg->recipient)) < endowment)
        return {EVMC_SUCCESS, gas_left};  // "Light" failure.

    const auto& sender = state.msg->recipient;
    const auto sender_nonce = state.host.get_nonce(sender);  // Pre-bump sender nonce.

    // Creation fails when the sender's nonce is at maximum (EIP-2681).
    if (sender_nonce == MAX_NONCE)
        return {EVMC_SUCCESS, gas_left};  // "Light" failure.

    const auto init_code =
        bytes_view{init_code_size > 0 ? &state.memory[init_code_offset] : nullptr, init_code_size};

    // Compute the address of the account to be created. The Host bumps the sender's
    // nonce on create-frame entry, so CREATE uses the pre-bump value read above.
    const auto create_addr = (Op == OP_CREATE) ? compute_create_address(sender, sender_nonce) :
                                                 compute_create2_address(sender, salt, init_code);

    // Access to the new address is warmed and never reverted (EIP-2929).
    if (state.rev >= EVMC_BERLIN)
        state.host.access_account(create_addr);

    // Charge NEW_ACCOUNT for a deployment onto a not-alive address (EIP-161), after warming and
    // before the 63/64 split so a reservoir spill correctly lowers the gas forwarded to the
    // child. Refilled below when no account is created (EIP-8037).
    int64_t create_state_gas_charged = 0;
    if (state.rev >= EVMC_AMSTERDAM)
    {
        // EIP-161 aliveness. account_exists() is the same predicate: its pre-Spurious-Dragon
        // arm is unreachable under the Amsterdam gate, leaving `acc != nullptr && !is_empty()`.
        if (!state.host.account_exists(create_addr))
        {
            create_state_gas_charged = NEW_ACCOUNT_STATE_GAS;
            if (!state.state_gas.charge(gas_left, create_state_gas_charged))
                return {EVMC_OUT_OF_GAS, gas_left};
        }
    }

    evmc_message msg{.kind = to_call_kind(Op)};
    msg.recipient = create_addr;

    msg.gas = gas_left;
    if (state.rev >= EVMC_TANGERINE_WHISTLE)
        msg.gas -= msg.gas / 64;

    msg.input_data = init_code.data();
    msg.input_size = init_code.size();
    msg.sender = sender;
    msg.depth = state.msg->depth + 1;
    msg.value = intx::be::store<evmc::uint256be>(endowment);

    // The reservoir passes to the child in full; the 63/64 rule applies to gas_left only
    // (EIP-8037).
    msg.state_gas = state.state_gas.left;

    const auto result = state.host.call(msg);
    gas_left -= msg.gas - result.gas_left;
    state.gas_refund += result.gas_refund;
    // Thread the child's state gas back. A non-success result — a rolled-back initcode or an
    // address collision — creates no account, so its NEW_ACCOUNT charge is refilled; a create
    // onto an already-alive account was never charged (EIP-8037).
    accumulate_child_state_gas(gas_left, state, result);
    if (create_state_gas_charged != 0 && result.status_code != EVMC_SUCCESS)
        state.state_gas.refill(gas_left, create_state_gas_charged);

    state.return_data.assign(result.output_data, result.output_size);
    if (result.status_code == EVMC_SUCCESS)
        stack.top() = intx::be::load<uint256>(msg.recipient);

    return {EVMC_SUCCESS, gas_left};
}

template Result create_impl<OP_CREATE>(
    StackTop stack, int64_t gas_left, ExecutionState& state) noexcept;
template Result create_impl<OP_CREATE2>(
    StackTop stack, int64_t gas_left, ExecutionState& state) noexcept;
}  // namespace evmone::instr::core

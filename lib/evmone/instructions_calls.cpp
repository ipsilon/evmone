// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "delegation.hpp"
#include "instructions.hpp"
#include <variant>

constexpr int64_t MIN_RETAINED_GAS = 5000;
constexpr int64_t MIN_CALLEE_GAS = 2300;
constexpr int64_t CALL_VALUE_COST = 9000;
constexpr int64_t ACCOUNT_CREATION_COST = 25000;

constexpr auto EXTCALL_SUCCESS = 0;
constexpr auto EXTCALL_REVERT = 1;
constexpr auto EXTCALL_ABORT = 2;

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

    if constexpr (HAS_VALUE_ARG)
    {
        auto cost = has_value ? CALL_VALUE_COST : 0;

        if constexpr (Op == OP_CALL)
        {
            if (has_value && state.in_static_mode())
                return {EVMC_STATIC_MODE_VIOLATION, gas_left};

            if ((has_value || state.rev < EVMC_SPURIOUS_DRAGON) && !state.host.account_exists(dst))
                cost += ACCOUNT_CREATION_COST;
        }

        if ((gas_left -= cost) < 0)
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
                return {EVMC_SUCCESS, gas_left};  // "Light" failure.
        }
    }

    if (state.msg->depth >= 1024)
        return {EVMC_SUCCESS, gas_left};  // "Light" failure.

    const auto result = state.host.call(msg);
    state.return_data.assign(result.output_data, result.output_size);
    stack.top() = result.status_code == EVMC_SUCCESS;

    if (const auto copy_size = std::min(output_size, result.output_size); copy_size > 0)
        std::memcpy(&state.memory[output_offset], result.output_data, copy_size);

    const auto gas_used = msg.gas - result.gas_left;
    gas_left -= gas_used;
    state.gas_refund += result.gas_refund;
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
Result extcall_impl(StackTop stack, int64_t gas_left, ExecutionState& state) noexcept
{
    static_assert(Op == OP_EXTCALL || Op == OP_EXTDELEGATECALL || Op == OP_EXTSTATICCALL);

    const auto dst_u256 = stack.pop();
    const auto input_offset_u256 = stack.pop();
    const auto input_size_u256 = stack.pop();
    const auto value = (Op == OP_EXTSTATICCALL || Op == OP_EXTDELEGATECALL) ? 0 : stack.pop();
    const auto has_value = value != 0;

    stack.push(EXTCALL_ABORT);  // Assume (hard) failure.
    state.return_data.clear();

    // Address space expansion ready check.
    static constexpr auto ADDRESS_MAX = (uint256{1} << 160) - 1;
    if (dst_u256 > ADDRESS_MAX)
        return {EVMC_ARGUMENT_OUT_OF_RANGE, gas_left};

    const auto dst = intx::be::trunc<evmc::address>(dst_u256);

    if (state.host.access_account(dst) == EVMC_ACCESS_COLD)
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

    const auto input_offset = static_cast<size_t>(input_offset_u256);
    const auto input_size = static_cast<size_t>(input_size_u256);

    evmc_message msg{.kind = to_call_kind(Op)};
    msg.flags = (Op == OP_EXTSTATICCALL) ? uint32_t{EVMC_STATIC} : state.msg->flags;
    if (dst != code_addr)
        msg.flags |= EVMC_DELEGATED;
    else
        msg.flags &= ~std::underlying_type_t<evmc_flags>{EVMC_DELEGATED};
    msg.depth = state.msg->depth + 1;
    msg.recipient = (Op != OP_EXTDELEGATECALL) ? dst : state.msg->recipient;
    msg.code_address = code_addr;
    msg.sender = (Op == OP_EXTDELEGATECALL) ? state.msg->sender : state.msg->recipient;
    msg.value =
        (Op == OP_EXTDELEGATECALL) ? state.msg->value : intx::be::store<evmc::uint256be>(value);

    if (input_size > 0)
    {
        // input_offset may be garbage if input_size == 0.
        msg.input_data = &state.memory[input_offset];
        msg.input_size = input_size;
    }

    auto cost = has_value ? CALL_VALUE_COST : 0;

    if constexpr (Op == OP_EXTCALL)
    {
        if (has_value && state.in_static_mode())
            return {EVMC_STATIC_MODE_VIOLATION, gas_left};

        if (has_value && !state.host.account_exists(dst))
            cost += ACCOUNT_CREATION_COST;
    }

    if ((gas_left -= cost) < 0)
        return {EVMC_OUT_OF_GAS, gas_left};

    msg.gas = gas_left - std::max(gas_left / 64, MIN_RETAINED_GAS);

    if (msg.gas < MIN_CALLEE_GAS || state.msg->depth >= 1024 ||
        (has_value &&
            intx::be::load<uint256>(state.host.get_balance(state.msg->recipient)) < value))
    {
        stack.top() = EXTCALL_REVERT;
        return {EVMC_SUCCESS, gas_left};  // "Light" failure.
    }

    if constexpr (Op == OP_EXTDELEGATECALL)
    {
        // The code targeted by EXTDELEGATECALL must also be an EOF.
        // This restriction has been added to EIP-3540 in
        // https://github.com/ethereum/EIPs/pull/7131
        uint8_t target_code_prefix[2];
        const auto s = state.host.copy_code(
            msg.code_address, 0, target_code_prefix, std::size(target_code_prefix));
        if (!is_eof_container({target_code_prefix, s}))
        {
            stack.top() = EXTCALL_REVERT;
            return {EVMC_SUCCESS, gas_left};  // "Light" failure.
        }
    }

    const auto result = state.host.call(msg);
    state.return_data.assign(result.output_data, result.output_size);
    if (result.status_code == EVMC_SUCCESS)
        stack.top() = EXTCALL_SUCCESS;
    else if (result.status_code == EVMC_REVERT)
        stack.top() = EXTCALL_REVERT;
    else
        stack.top() = EXTCALL_ABORT;

    const auto gas_used = msg.gas - result.gas_left;
    gas_left -= gas_used;
    state.gas_refund += result.gas_refund;
    return {EVMC_SUCCESS, gas_left};
}

template Result extcall_impl<OP_EXTCALL>(
    StackTop stack, int64_t gas_left, ExecutionState& state) noexcept;
template Result extcall_impl<OP_EXTSTATICCALL>(
    StackTop stack, int64_t gas_left, ExecutionState& state) noexcept;
template Result extcall_impl<OP_EXTDELEGATECALL>(
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

    if (state.rev >= EVMC_SHANGHAI && init_code_size > 0xC000)
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

    evmc_message msg{.kind = to_call_kind(Op)};
    msg.gas = gas_left;
    if (state.rev >= EVMC_TANGERINE_WHISTLE)
        msg.gas = msg.gas - msg.gas / 64;

    if (init_code_size > 0)
    {
        // init_code_offset may be garbage if init_code_size == 0.
        msg.input_data = &state.memory[init_code_offset];
        msg.input_size = init_code_size;

        if (state.rev >= EVMC_EXPERIMENTAL)
        {
            // EOF initcode is not allowed for legacy creation
            if (is_eof_container({msg.input_data, msg.input_size}))
                return {EVMC_SUCCESS, gas_left};  // "Light" failure.
        }
    }
    msg.sender = state.msg->recipient;
    msg.depth = state.msg->depth + 1;
    msg.create2_salt = intx::be::store<evmc::bytes32>(salt);
    msg.value = intx::be::store<evmc::uint256be>(endowment);

    const auto result = state.host.call(msg);
    gas_left -= msg.gas - result.gas_left;
    state.gas_refund += result.gas_refund;

    state.return_data.assign(result.output_data, result.output_size);
    if (result.status_code == EVMC_SUCCESS)
        stack.top() = intx::be::load<uint256>(result.create_address);

    return {EVMC_SUCCESS, gas_left};
}

template <Opcode Op>
Result create_eof_impl(
    StackTop stack, int64_t gas_left, ExecutionState& state, code_iterator& pos) noexcept
{
    static_assert(Op == OP_EOFCREATE || Op == OP_TXCREATE);

    if (state.in_static_mode())
        return {EVMC_STATIC_MODE_VIOLATION, gas_left};

    const auto initcode_hash =
        (Op == OP_TXCREATE) ? intx::be::store<evmc::bytes32>(stack.pop()) : evmc::bytes32{};
    const auto salt = stack.pop();
    const auto input_offset_u256 = stack.pop();
    const auto input_size_u256 = stack.pop();
    const auto endowment = stack.pop();

    stack.push(0);  // Assume failure.
    state.return_data.clear();

    if (!check_memory(gas_left, state.memory, input_offset_u256, input_size_u256))
        return {EVMC_OUT_OF_GAS, gas_left};

    constexpr auto pos_advance = (Op == OP_EOFCREATE ? 2 : 1);
    pos += pos_advance;

    if (state.msg->depth >= 1024)
        return {EVMC_SUCCESS, gas_left};  // "Light" failure.

    if (endowment != 0 &&
        intx::be::load<uint256>(state.host.get_balance(state.msg->recipient)) < endowment)
        return {EVMC_SUCCESS, gas_left};  // "Light" failure.

    bytes_view initcontainer;
    if constexpr (Op == OP_EOFCREATE)
    {
        const auto initcontainer_index = pos[-1];
        const auto& container = state.original_code;
        const auto& eof_header = state.analysis.baseline->eof_header();
        initcontainer = eof_header.get_container(container, initcontainer_index);
    }
    else
    {
        auto* tx_initcode = state.get_tx_initcode_by_hash(initcode_hash);
        // In case initcode was not found, nullptr was returned.
        if (tx_initcode == nullptr)
            return {EVMC_SUCCESS, gas_left};  // "Light" failure
        initcontainer = tx_initcode->code;

        if (!tx_initcode->is_valid.has_value())
        {
            const auto error_subcont =
                validate_eof(state.rev, ContainerKind::initcode, initcontainer);
            tx_initcode->is_valid = (error_subcont == EOFValidationError::success);
        }

        if (!*tx_initcode->is_valid)
            return {EVMC_SUCCESS, gas_left};  // "Light" failure.
    }

    const auto input_offset = static_cast<size_t>(input_offset_u256);
    const auto input_size = static_cast<size_t>(input_size_u256);

    evmc_message msg{.kind = to_call_kind(Op)};
    msg.gas = gas_left - gas_left / 64;
    if (input_size > 0)
    {
        // input_data may be garbage if init_code_size == 0.
        msg.input_data = &state.memory[input_offset];
        msg.input_size = input_size;
    }

    msg.sender = state.msg->recipient;
    msg.depth = state.msg->depth + 1;
    msg.create2_salt = intx::be::store<evmc::bytes32>(salt);
    msg.value = intx::be::store<evmc::uint256be>(endowment);
    // init_code is guaranteed to be non-empty by validation of container sections
    msg.code = initcontainer.data();
    msg.code_size = initcontainer.size();

    const auto result = state.host.call(msg);
    gas_left -= msg.gas - result.gas_left;
    state.gas_refund += result.gas_refund;

    state.return_data.assign(result.output_data, result.output_size);
    if (result.status_code == EVMC_SUCCESS)
        stack.top() = intx::be::load<uint256>(result.create_address);

    return {EVMC_SUCCESS, gas_left};
}

template Result create_impl<OP_CREATE>(
    StackTop stack, int64_t gas_left, ExecutionState& state) noexcept;
template Result create_impl<OP_CREATE2>(
    StackTop stack, int64_t gas_left, ExecutionState& state) noexcept;
template Result create_eof_impl<OP_EOFCREATE>(
    StackTop stack, int64_t gas_left, ExecutionState& state, code_iterator& pos) noexcept;
template Result create_eof_impl<OP_TXCREATE>(
    StackTop stack, int64_t gas_left, ExecutionState& state, code_iterator& pos) noexcept;
}  // namespace evmone::instr::core

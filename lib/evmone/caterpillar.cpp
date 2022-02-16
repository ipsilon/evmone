// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2021 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "caterpillar.hpp"
#include "execution_state.hpp"
#include "instructions.hpp"
#include "vm.hpp"
#include <memory>

namespace evmone::caterpillar
{
namespace
{
[[gnu::always_inline]] inline code_iterator invoke(void (*instr_fn)(StackTop) noexcept,
    uint256* stack_top, code_iterator code_it, int64_t& /*gas*/, ExecutionState& /*state*/) noexcept
{
    instr_fn(stack_top);
    return code_it + 1;
}

[[gnu::always_inline]] inline code_iterator invoke(
    Result (*instr_fn)(StackTop, int64_t, ExecutionState&) noexcept, uint256* stack_top,
    code_iterator code_it, int64_t& gas, ExecutionState& state) noexcept
{
    const auto o = instr_fn(stack_top, gas, state);
    gas = o.gas_left;
    if (o.status != EVMC_SUCCESS)
    {
        state.status = o.status;
        return nullptr;
    }
    return code_it + 1;
}

[[gnu::always_inline]] inline code_iterator invoke(
    void (*instr_fn)(StackTop, ExecutionState&) noexcept, uint256* stack_top, code_iterator code_it,
    int64_t& /*gas*/, ExecutionState& state) noexcept
{
    instr_fn(stack_top, state);
    return code_it + 1;
}

[[gnu::always_inline]] inline code_iterator invoke(
    code_iterator (*instr_fn)(StackTop, ExecutionState&, code_iterator) noexcept,
    uint256* stack_top, code_iterator code_it, int64_t& /*gas*/, ExecutionState& state) noexcept
{
    return instr_fn(stack_top, state, code_it);
}

[[gnu::always_inline]] inline code_iterator invoke(
    TermResult (*instr_fn)(StackTop, int64_t, ExecutionState&) noexcept, uint256* stack_top,
    code_iterator /*code_it*/, int64_t& gas, ExecutionState& state) noexcept
{
    const auto result = instr_fn(stack_top, gas, state);
    gas = result.gas_left;
    state.status = result.status;
    return nullptr;
}
/// @}

template <Opcode Op>
inline evmc_status_code check_defined(evmc_revision rev) noexcept
{
    static_assert(
        !instr::has_const_gas_cost(Op) || instr::gas_costs[EVMC_FRONTIER][Op] != instr::undefined,
        "undefined instructions must not be handled by check_requirements()");

    if constexpr (!instr::has_const_gas_cost(Op))
    {
        if (INTX_UNLIKELY(instr::gas_costs[rev][Op] < 0))
            return EVMC_UNDEFINED_INSTRUCTION;
    }
    return EVMC_SUCCESS;
}

template <Opcode Op>
inline evmc_status_code check_stack(const uint256* stack_top, const uint256* stack_bottom) noexcept
{
    if constexpr (instr::traits[Op].stack_height_change > 0)
    {
        static_assert(instr::traits[Op].stack_height_change == 1,
            "unexpected instruction with multiple results");
        if (INTX_UNLIKELY(stack_top == stack_bottom + StackSpace::limit))
            return EVMC_STACK_OVERFLOW;
    }
    if constexpr (instr::traits[Op].stack_height_required > 0)
    {
        // Check stack underflow using pointer comparison <= (better optimization).
        static constexpr auto min_offset = instr::traits[Op].stack_height_required - 1;
        if (INTX_UNLIKELY(stack_top <= stack_bottom + min_offset))
            return EVMC_STACK_UNDERFLOW;
    }
    return EVMC_SUCCESS;
}

template <Opcode Op>
inline int64_t check_gas(int64_t& gas_left, evmc_revision rev) noexcept
{
    auto gas_cost = instr::gas_costs[EVMC_FRONTIER][Op];  // Init assuming const cost.
    if constexpr (!instr::has_const_gas_cost(Op))
        gas_cost = instr::gas_costs[rev][Op];  // If not, load the cost from the table.
    return gas_left - gas_cost;
}

template <Opcode Op>
evmc_status_code invoke(const uint256* stack_bottom, uint256* stack_top, code_iterator code_it,
    int64_t& gas, void*, ExecutionState& state) noexcept;

evmc_status_code cat_undefined(const uint256* /*stack_bottom*/, uint256* /*stack_top*/,
    code_iterator /*code_it*/, int64_t& /*gas*/, void*, ExecutionState& /*state*/) noexcept
{
    return EVMC_UNDEFINED_INSTRUCTION;
}

using InstrFn = evmc_status_code (*)(const uint256* stack_bottom, uint256* stack_top,
    code_iterator code_it, int64_t& gas, void*, ExecutionState& state) noexcept;

constexpr auto instr_table = []() noexcept {
#define ON_OPCODE(OPCODE) invoke<OPCODE>,
#undef ON_OPCODE_UNDEFINED
#define ON_OPCODE_UNDEFINED(_) cat_undefined,
    std::array<InstrFn, 256> table{MAP_OPCODES};
    return table;
#undef ON_OPCODE
#undef ON_OPCODE_UNDEFINED
#define ON_OPCODE_UNDEFINED ON_OPCODE_UNDEFINED_DEFAULT
}();
static_assert(std::size(instr_table) == 256);
static_assert(instr_table[OP_PUSH2] == invoke<OP_PUSH2>);

/// A helper to invoke the instruction implementation of the given opcode Op.
template <Opcode Op>
evmc_status_code invoke(const uint256* stack_bottom, uint256* stack_top, code_iterator code_it,
    int64_t& gas, void* tbl, ExecutionState& state) noexcept
{
    [[maybe_unused]] auto op = Op;

    const auto rev = state.rev;
    if (const auto status = check_defined<Op>(rev); status != EVMC_SUCCESS)
        return status;

    if (const auto status = check_stack<Op>(stack_top, stack_bottom);
        INTX_UNLIKELY(status != EVMC_SUCCESS))
        return status;

    if (gas = check_gas<Op>(gas, rev); INTX_UNLIKELY(gas < 0))
        return EVMC_OUT_OF_GAS;

    code_it = invoke(instr::core::impl<Op>, stack_top, code_it, gas, state);
    if (!code_it)
        return state.status;

    stack_top += instr::traits[Op].stack_height_change;
    auto tbl2 = (InstrFn*)tbl;
    [[clang::musttail]] return tbl2[*code_it](stack_bottom, stack_top, code_it, gas, tbl, state);
}

}  // namespace

evmc_result execute(const VM& /*vm*/, int64_t gas, ExecutionState& state,
    const baseline::CodeAnalysis& analysis) noexcept
{
    state.analysis.baseline = &analysis;  // Assign code analysis for instruction implementations.

    const auto code = analysis.executable_code();

    const auto code_it = code.data();
    const auto first_fn = instr_table[*code_it];
    const auto stack_bottom = state.stack_space.bottom();
    auto stack_top = stack_bottom;
    const auto status =
        first_fn(stack_bottom, stack_top, code_it, gas, (void*)instr_table.data(), state);
    state.status = status;

    const auto gas_left = (status == EVMC_SUCCESS || status == EVMC_REVERT) ? gas : 0;
    const auto gas_refund = (status == EVMC_SUCCESS) ? state.gas_refund : 0;

    assert(state.output_size != 0 || state.output_offset == 0);
    const auto result = evmc::make_result(state.status, gas_left, gas_refund,
        state.output_size != 0 ? &state.memory[state.output_offset] : nullptr, state.output_size);
    return result;
}

evmc_result execute(evmc_vm* c_vm, const evmc_host_interface* host, evmc_host_context* ctx,
    evmc_revision rev, const evmc_message* msg, const uint8_t* code, size_t code_size) noexcept
{
    auto vm = static_cast<VM*>(c_vm);
    const auto jumpdest_map = baseline::analyze({code, code_size});
    auto state =
        std::make_unique<ExecutionState>(*msg, rev, *host, ctx, bytes_view{code, code_size});
    return caterpillar::execute(*vm, msg->gas, *state, jumpdest_map);
}

}  // namespace evmone::caterpillar

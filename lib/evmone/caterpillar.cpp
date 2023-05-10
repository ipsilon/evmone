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
        state.gas_left = gas;
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
    state.gas_left = result.gas_left;
    state.status = result.status;
    return nullptr;
}
/// @}

template <Opcode Op>
inline bool check_stack(const uint256* stack_top, const uint256* stack_bottom) noexcept
{
    if constexpr (instr::traits[Op].stack_height_change > 0)
    {
        static_assert(instr::traits[Op].stack_height_change == 1,
            "unexpected instruction with multiple results");
        if (INTX_UNLIKELY(stack_top == stack_bottom + StackSpace::limit))
            return false;
    }
    if constexpr (instr::traits[Op].stack_height_required > 0)
    {
        // Check stack underflow using pointer comparison <= (better optimization).
        static constexpr auto min_offset = instr::traits[Op].stack_height_required - 1;
        if (INTX_UNLIKELY(stack_top <= stack_bottom + min_offset))
            return false;
    }
    return true;
}

inline constexpr bool has_const_gas_cost_since_defined(Opcode op) noexcept
{
    const size_t first_rev = *instr::traits[op].since;
    const auto g = instr::gas_costs[first_rev][op];
    for (size_t r = first_rev + 1; r <= EVMC_MAX_REVISION; ++r)
    {
        if (instr::gas_costs[r][op] != g)
            return false;
    }
    return true;
}
static_assert(has_const_gas_cost_since_defined(OP_STOP));
static_assert(has_const_gas_cost_since_defined(OP_ADD));
static_assert(has_const_gas_cost_since_defined(OP_PUSH1));
static_assert(has_const_gas_cost_since_defined(OP_SHL));
static_assert(has_const_gas_cost_since_defined(OP_SELFBALANCE));
static_assert(!has_const_gas_cost_since_defined(OP_BALANCE));
static_assert(!has_const_gas_cost_since_defined(OP_SLOAD));

template <Opcode Op>
inline int64_t check_gas(int64_t gas_left, evmc_revision rev) noexcept
{
    auto gas_cost = instr::gas_costs[*instr::traits[Op].since][Op];  // Init assuming const cost.
    if constexpr (!has_const_gas_cost_since_defined(Op))
        gas_cost = instr::gas_costs[rev][Op];  // If not, load the cost from the table.
    return gas_left - gas_cost;
}

template <Opcode Op>
evmc_status_code invoke(
    uint256* stack_top, code_iterator code_it, int64_t& gas, ExecutionState& state) noexcept;

evmc_status_code cat_undefined(uint256* /*stack_top*/, code_iterator /*code_it*/, int64_t& /*gas*/,
    ExecutionState& /*state*/) noexcept
{
    return EVMC_UNDEFINED_INSTRUCTION;
}

using InstrFn = evmc_status_code (*)(
    uint256* stack_top, code_iterator code_it, int64_t& gas, ExecutionState& state) noexcept;

#define ON_OPCODE(OPCODE)                                                                      \
    extern "C" evmc_status_code evmone_##OPCODE(                                                                \
        uint256* stack_top, code_iterator code_it, int64_t& g, ExecutionState& state) noexcept \
    {                                                                                          \
        /*TODO: The [[musttail]] is needed although invoke<> is [[always_inline]]*/ \
        [[clang::musttail]] return invoke<OPCODE>(stack_top, code_it, g, state);             \
    }

MAP_OPCODES
#undef ON_OPCODE

using InstrTable = std::array<InstrFn, 256>;
template <evmc_revision Rev>
constexpr InstrTable build_instr_table() noexcept
{
#define ON_OPCODE(OPCODE) (instr::traits[OPCODE].since <= Rev ? evmone_##OPCODE : cat_undefined),
#undef ON_OPCODE_UNDEFINED
#define ON_OPCODE_UNDEFINED(_) cat_undefined,
    return {MAP_OPCODES};
#undef ON_OPCODE
#undef ON_OPCODE_UNDEFINED
#define ON_OPCODE_UNDEFINED ON_OPCODE_UNDEFINED_DEFAULT
}
constexpr InstrTable instr_table[] = {
    build_instr_table<EVMC_FRONTIER>(),
    build_instr_table<EVMC_HOMESTEAD>(),
    build_instr_table<EVMC_TANGERINE_WHISTLE>(),
    build_instr_table<EVMC_SPURIOUS_DRAGON>(),
    build_instr_table<EVMC_BYZANTIUM>(),
    build_instr_table<EVMC_CONSTANTINOPLE>(),
    build_instr_table<EVMC_PETERSBURG>(),
    build_instr_table<EVMC_ISTANBUL>(),
    build_instr_table<EVMC_BERLIN>(),
    build_instr_table<EVMC_LONDON>(),
    build_instr_table<EVMC_PARIS>(),
    build_instr_table<EVMC_SHANGHAI>(),
    build_instr_table<EVMC_CANCUN>(),
    build_instr_table<EVMC_PRAGUE>(),
    build_instr_table<EVMC_OSAKA>(),
    build_instr_table<EVMC_EXPERIMENTAL>(),
};
static_assert(std::size(instr_table) == EVMC_MAX_REVISION + 1);
static_assert(std::size(instr_table[0]) == 256);
static_assert(instr_table[0][OP_PUSH2] == evmone_OP_PUSH2);

/// A helper to invoke the instruction implementation of the given opcode Op.
template <Opcode Op>
evmc_status_code invoke(
    uint256* stack_top, code_iterator code_it, int64_t& gas, ExecutionState& state) noexcept
{
    [[maybe_unused]] auto op = Op;

    if (INTX_UNLIKELY(!check_stack<Op>(stack_top, state.stack_bottom)))
        return stack_top - state.stack_bottom < StackSpace::limit ? EVMC_STACK_UNDERFLOW :
                                                                    EVMC_STACK_OVERFLOW;

    if (gas = check_gas<Op>(gas, state.rev); INTX_UNLIKELY(gas < 0))
        return EVMC_OUT_OF_GAS;

    code_it = invoke(instr::core::impl<Op>, stack_top, code_it, gas, state);
    if (!code_it)
        return state.status;

    stack_top += instr::traits[Op].stack_height_change;
    const auto tbl = static_cast<const InstrFn*>(state.tbl);
    [[clang::musttail]] return tbl[*code_it](stack_top, code_it, gas, state);
}

}  // namespace

evmc_result execute(const VM& /*vm*/, int64_t gas, ExecutionState& state,
    const baseline::CodeAnalysis& analysis) noexcept
{
    state.analysis.baseline = &analysis;  // Assign code analysis for instruction implementations.

    const auto code = analysis.executable_code();

    const auto& tbl = instr_table[state.rev];
    state.tbl = tbl.data();

    state.tbl = instr_table[state.rev].data();

    const auto code_it = code.data();
    const auto first_fn = instr_table[state.rev][*code_it];
    state.stack_bottom = state.stack_space.bottom();
    auto stack_top = state.stack_space.bottom();
    const auto status = first_fn(stack_top, code_it, gas, state);
    state.status = status;

    const auto gas_left = (status == EVMC_SUCCESS || status == EVMC_REVERT) ? state.gas_left : 0;
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

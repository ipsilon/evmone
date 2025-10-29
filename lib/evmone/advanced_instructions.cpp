// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "advanced_analysis.hpp"
#include "instructions.hpp"
#include "instructions_traits.hpp"

// TODO: This warning suppression may be lifted, but the situation in this file is complicated.
// NOLINTBEGIN(misc-use-internal-linkage)

namespace evmone::advanced
{
namespace instr
{
using namespace evmone::instr;

/// Instruction implementations - "core" instruction + stack height adjustment.
/// @{
template <Opcode Op, void CoreFn(StackTop) noexcept = core::impl<Op>>
inline void impl(AdvancedExecutionState& state) noexcept
{
    CoreFn(state.stack);
    state.adjust_stack_size(instr::traits[Op].stack_height_change);
}

template <Opcode Op, void CoreFn(StackTop, ExecutionState&) noexcept = core::impl<Op>>
inline void impl(AdvancedExecutionState& state) noexcept
{
    CoreFn(state.stack, state);
    state.adjust_stack_size(instr::traits[Op].stack_height_change);
}

template <Opcode Op, evmc_status_code CoreFn(StackTop, ExecutionState&) noexcept = core::impl<Op>>
inline evmc_status_code impl(AdvancedExecutionState& state) noexcept
{
    const auto status = CoreFn(state.stack, state);
    state.adjust_stack_size(instr::traits[Op].stack_height_change);
    return status;
}

template <Opcode Op,
    evmc_status_code CoreFn(StackTop, int64_t&, ExecutionState&) noexcept = core::impl<Op>>
inline evmc_status_code impl(AdvancedExecutionState& state) noexcept
{
    const auto status = CoreFn(state.stack, state.gas_left, state);
    state.adjust_stack_size(instr::traits[Op].stack_height_change);
    return status;
}

template <Opcode Op, Result CoreFn(StackTop, int64_t, ExecutionState&) noexcept = core::impl<Op>>
inline evmc_status_code impl(AdvancedExecutionState& state) noexcept
{
    const auto status = CoreFn(state.stack, state.gas_left, state);
    state.gas_left = status.gas_left;
    state.adjust_stack_size(instr::traits[Op].stack_height_change);
    return status.status;
}

template <Opcode Op,
    TermResult CoreFn(StackTop, int64_t, ExecutionState&) noexcept = core::impl<Op>>
inline TermResult impl(AdvancedExecutionState& state) noexcept
{
    // Stack height adjustment may be omitted.
    return CoreFn(state.stack, state.gas_left, state);
}

template <Opcode Op,
    code_iterator CoreFn(StackTop, ExecutionState&, code_iterator) noexcept = core::impl<Op>>
inline code_iterator impl(AdvancedExecutionState& state, code_iterator pos) noexcept
{
    const auto new_pos = CoreFn(state.stack, state, pos);
    state.adjust_stack_size(instr::traits[Op].stack_height_change);
    return new_pos;
}
/// @}
}  // namespace instr

/// Fake wrap for generic instruction implementations accessing current code location.
/// This is to make any op<...> compile, but pointers must be replaced with Advanced-specific
/// implementation. Definition not provided.
template <code_iterator InstrFn(AdvancedExecutionState&, code_iterator)>
const Instruction* op(const Instruction* /*instr*/, AdvancedExecutionState& state) noexcept;

/// Wraps the generic instruction implementation to advanced instruction function signature.
template <Result InstrFn(AdvancedExecutionState&, code_iterator) noexcept>
const Instruction* op(const Instruction* instr, AdvancedExecutionState& state) noexcept;

/// Wraps the generic instruction implementation to advanced instruction function signature.
template <TermResult InstrFn(AdvancedExecutionState&, code_iterator) noexcept>
const Instruction* op(const Instruction* instr, AdvancedExecutionState& state) noexcept;

namespace
{
using advanced::op;

/// Wraps the generic instruction implementation to advanced instruction function signature.
template <void InstrFn(AdvancedExecutionState&) noexcept>
const Instruction* op(const Instruction* instr, AdvancedExecutionState& state) noexcept
{
    InstrFn(state);
    return ++instr;
}

/// Wraps the generic instruction implementation to advanced instruction function signature.
template <evmc_status_code InstrFn(AdvancedExecutionState&) noexcept>
const Instruction* op(const Instruction* instr, AdvancedExecutionState& state) noexcept
{
    if (const auto status_code = InstrFn(state); status_code != EVMC_SUCCESS)
        return state.exit(status_code);
    return ++instr;
}

/// Wraps the generic instruction implementation to advanced instruction function signature.
template <TermResult InstrFn(AdvancedExecutionState&) noexcept>
const Instruction* op(const Instruction* /*instr*/, AdvancedExecutionState& state) noexcept
{
    const auto result = InstrFn(state);
    state.gas_left = result.gas_left;
    return state.exit(result.status);
}

const Instruction* op_sstore(const Instruction* instr, AdvancedExecutionState& state) noexcept
{
    const auto gas_left_correction = state.current_block_cost - instr->arg.number;
    state.gas_left += gas_left_correction;

    const auto status = instr::impl<OP_SSTORE>(state);
    if (status != EVMC_SUCCESS)
        return state.exit(status);

    if ((state.gas_left -= gas_left_correction) < 0)
        return state.exit(EVMC_OUT_OF_GAS);

    return ++instr;
}

const Instruction* opx_beginblock(const Instruction* instr, AdvancedExecutionState& state) noexcept
{
    auto& block = instr->arg.block;

    if ((state.gas_left -= block.gas_cost) < 0)
        return state.exit(EVMC_OUT_OF_GAS);

    if (const auto stack_size = state.stack_size(); stack_size < block.stack_req)
        return state.exit(EVMC_STACK_UNDERFLOW);
    else if (stack_size + block.stack_max_growth > StackSpace::limit)
        return state.exit(EVMC_STACK_OVERFLOW);

    state.current_block_cost = block.gas_cost;
    return ++instr;
}

const Instruction* op_jump(const Instruction*, AdvancedExecutionState& state) noexcept
{
    const auto dst = state.stack.pop();
    auto pc = -1;
    if (std::numeric_limits<int>::max() < dst ||
        (pc = find_jumpdest(*state.analysis.advanced, static_cast<int>(dst))) < 0)
        return state.exit(EVMC_BAD_JUMP_DESTINATION);

    return &state.analysis.advanced->instrs[static_cast<size_t>(pc)];
}

const Instruction* op_jumpi(const Instruction* instr, AdvancedExecutionState& state) noexcept
{
    if (state.stack[1] != 0)
    {
        instr = op_jump(instr, state);  // target
        (void)state.stack.pop();        // condition
    }
    else
    {
        (void)state.stack.pop();               // target
        (void)state.stack.pop();               // condition
        instr = opx_beginblock(instr, state);  // follow-by block
    }
    return instr;
}

const Instruction* op_pc(const Instruction* instr, AdvancedExecutionState& state) noexcept
{
    state.stack.push(instr->arg.number);
    return ++instr;
}

const Instruction* op_gas(const Instruction* instr, AdvancedExecutionState& state) noexcept
{
    const auto correction = state.current_block_cost - instr->arg.number;
    const auto gas = static_cast<uint64_t>(state.gas_left + correction);
    state.stack.push(gas);
    return ++instr;
}

const Instruction* op_push_small(const Instruction* instr, AdvancedExecutionState& state) noexcept
{
    state.stack.push(instr->arg.small_push_value);
    return ++instr;
}

const Instruction* op_push_full(const Instruction* instr, AdvancedExecutionState& state) noexcept
{
    state.stack.push(*instr->arg.push_value);
    return ++instr;
}

template <Opcode Op>
const Instruction* op_call(const Instruction* instr, AdvancedExecutionState& state) noexcept
{
    const auto gas_left_correction = state.current_block_cost - instr->arg.number;
    state.gas_left += gas_left_correction;

    const auto status = instr::impl<Op>(state);
    if (status != EVMC_SUCCESS)
        return state.exit(status);

    if ((state.gas_left -= gas_left_correction) < 0)
        return state.exit(EVMC_OUT_OF_GAS);

    return ++instr;
}

template <Opcode Op>
const Instruction* op_create(const Instruction* instr, AdvancedExecutionState& state) noexcept
{
    const auto gas_left_correction = state.current_block_cost - instr->arg.number;
    state.gas_left += gas_left_correction;

    const auto status = instr::impl<Op>(state);
    if (status != EVMC_SUCCESS)
        return state.exit(status);

    if ((state.gas_left -= gas_left_correction) < 0)
        return state.exit(EVMC_OUT_OF_GAS);

    return ++instr;
}

const Instruction* op_undefined(const Instruction*, AdvancedExecutionState& state) noexcept
{
    return state.exit(EVMC_UNDEFINED_INSTRUCTION);
}


constexpr std::array<instruction_exec_fn, 256> instruction_implementations = []() noexcept {
    std::array<instruction_exec_fn, 256> table{};

    // Init table with wrapped generic implementations.
#define ON_OPCODE(OPCODE) table[OPCODE] = op<instr::impl<(OPCODE)>>;
    MAP_OPCODES
#undef ON_OPCODE

    // Overwrite with Advanced-specific implementations.
    table[OP_SSTORE] = op_sstore;
    table[OP_JUMP] = op_jump;
    table[OP_JUMPI] = op_jumpi;
    table[OP_PC] = op_pc;
    table[OP_GAS] = op_gas;
    table[OP_JUMPDEST] = opx_beginblock;

    for (auto op = size_t{OP_PUSH1}; op <= OP_PUSH8; ++op)
        table[op] = op_push_small;
    for (auto op = size_t{OP_PUSH9}; op <= OP_PUSH32; ++op)
        table[op] = op_push_full;

    table[OP_CREATE] = op_create<OP_CREATE>;
    table[OP_CALL] = op_call<OP_CALL>;
    table[OP_CALLCODE] = op_call<OP_CALLCODE>;
    table[OP_DELEGATECALL] = op_call<OP_DELEGATECALL>;
    table[OP_CREATE2] = op_create<OP_CREATE2>;
    table[OP_STATICCALL] = op_call<OP_STATICCALL>;

    return table;
}();
}  // namespace

EVMC_EXPORT const OpTable& get_op_table(evmc_revision rev) noexcept
{
    static constexpr auto op_tables = []() noexcept {
        std::array<OpTable, EVMC_MAX_REVISION + 1> tables{};
        for (size_t r = EVMC_FRONTIER; r <= EVMC_MAX_REVISION; ++r)
        {
            auto& table = tables[r];
            for (size_t i = 0; i < table.size(); ++i)
            {
                auto& t = table[i];
                const auto gas_cost = instr::gas_costs[r][i];
                if (gas_cost == instr::undefined)
                {
                    t.fn = op_undefined;
                    t.gas_cost = 0;
                }
                else
                {
                    t.fn = instruction_implementations[i];
                    t.gas_cost = gas_cost;
                    t.stack_req = instr::traits[i].stack_height_required;
                    t.stack_change = instr::traits[i].stack_height_change;
                }
            }
        }
        return tables;
    }();

    return op_tables[rev];
}
}  // namespace evmone::advanced

// NOLINTEND(misc-use-internal-linkage)

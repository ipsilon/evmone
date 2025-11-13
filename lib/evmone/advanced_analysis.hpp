// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "execution_state.hpp"
#include "instructions.hpp"
#include "instructions_opcodes.hpp"
#include <evmc/evmc.hpp>
#include <evmc/utils.h>
#include <intx/intx.hpp>
#include <array>
#include <cstdint>
#include <vector>

namespace evmone::advanced
{
struct Instruction;

/// Compressed information about instruction basic block.
struct BlockInfo
{
    /// The total base gas cost of all instructions in the block.
    uint32_t gas_cost = 0;

    /// The stack height required to execute the block.
    int16_t stack_req = 0;

    /// The maximum stack height growth relative to the stack height at block start.
    int16_t stack_max_growth = 0;
};
static_assert(sizeof(BlockInfo) == 8);


/// The execution state specialized for the Advanced interpreter.
struct AdvancedExecutionState : ExecutionState
{
    int64_t gas_left = 0;

    /// Pointer to the stack top.
    StackTop stack = stack_space.bottom();

    /// The gas cost of the current block.
    ///
    /// This is only needed to correctly calculate the "current gas left" value.
    uint32_t current_block_cost = 0;

    AdvancedExecutionState() noexcept = default;

    AdvancedExecutionState(const evmc_message& message, evmc_revision revision,
        const evmc_host_interface& host_interface, evmc_host_context* host_ctx,
        bytes_view _code) noexcept
      : ExecutionState{message, revision, host_interface, host_ctx, _code}, gas_left{message.gas}
    {}

    /// Computes the current EVM stack height.
    [[nodiscard]] int stack_size() noexcept
    {
        return static_cast<int>(stack.end() - stack_space.bottom());
    }

    /// Adjust the EVM stack height by given change.
    void adjust_stack_size(int change) noexcept { stack = stack.end() + change; }

    /// Terminates the execution with the given status code.
    const Instruction* exit(evmc_status_code status_code) noexcept
    {
        status = status_code;
        return nullptr;
    }

    /// Resets the contents of the execution_state so that it could be reused.
    void reset(const evmc_message& message, evmc_revision revision,
        const evmc_host_interface& host_interface, evmc_host_context* host_ctx,
        bytes_view _code) noexcept
    {
        ExecutionState::reset(message, revision, host_interface, host_ctx, _code);
        gas_left = message.gas;
        stack = stack_space.bottom();
        analysis.advanced = nullptr;  // For consistency with previous behavior.
        current_block_cost = 0;
    }
};

union InstructionArgument
{
    int64_t number;
    const intx::uint256* push_value;
    uint64_t small_push_value;
    BlockInfo block{};
};
static_assert(
    sizeof(InstructionArgument) == sizeof(uint64_t), "Incorrect size of instruction_argument");

/// The pointer to function implementing an instruction execution.
using instruction_exec_fn = const Instruction* (*)(const Instruction*, AdvancedExecutionState&);

/// The evmone intrinsic opcodes.
///
/// These intrinsic instructions may be injected to the code in the analysis phase.
/// They contain additional and required logic to be executed by the interpreter.
enum intrinsic_opcodes : uint8_t  // NOLINT(*-use-enum-class)
{
    /// The BEGINBLOCK instruction.
    ///
    /// This instruction is defined as alias for JUMPDEST and replaces all JUMPDEST instructions.
    /// It is also injected at beginning of basic blocks not being the valid jump destination.
    /// It checks basic block execution requirements and terminates execution if they are not met.
    OPX_BEGINBLOCK = OP_JUMPDEST
};

struct OpTableEntry
{
    instruction_exec_fn fn;
    int16_t gas_cost;
    uint8_t stack_req;
    int8_t stack_change;
};

using OpTable = std::array<OpTableEntry, 256>;

struct Instruction
{
    instruction_exec_fn fn = nullptr;
    InstructionArgument arg;

    explicit constexpr Instruction(instruction_exec_fn f) noexcept : fn{f}, arg{} {}
};

struct AdvancedCodeAnalysis
{
    std::vector<Instruction> instrs;

    /// Storage for large push values.
    std::vector<intx::uint256> push_values;

    /// The offsets of JUMPDESTs in the original code.
    /// These are values that JUMP/JUMPI receives as an argument.
    /// The elements are sorted.
    std::vector<int32_t> jumpdest_offsets;

    /// The indexes of the instructions in the generated instruction table
    /// matching the elements from jumdest_offsets.
    /// This is value to which the next instruction pointer must be set in JUMP/JUMPI.
    std::vector<int32_t> jumpdest_targets;
};

inline int find_jumpdest(const AdvancedCodeAnalysis& analysis, int offset) noexcept
{
    const auto begin = std::begin(analysis.jumpdest_offsets);
    const auto end = std::end(analysis.jumpdest_offsets);
    const auto it = std::lower_bound(begin, end, offset);
    return (it != end && *it == offset) ?
               analysis.jumpdest_targets[static_cast<size_t>(it - begin)] :
               -1;
}

EVMC_EXPORT AdvancedCodeAnalysis analyze(evmc_revision rev, bytes_view code) noexcept;

EVMC_EXPORT const OpTable& get_op_table(evmc_revision rev) noexcept;

}  // namespace evmone::advanced

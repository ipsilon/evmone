// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "advanced_analysis.hpp"
#include <cassert>

namespace evmone::advanced
{
namespace
{
/// Clamps x to the max value of To type.
template <typename To, typename T>
constexpr To clamp(T x) noexcept
{
    constexpr auto max = std::numeric_limits<To>::max();
    return x <= max ? static_cast<To>(x) : max;
}

struct BlockAnalysis
{
    int64_t gas_cost = 0;

    int stack_req = 0;
    int stack_max_growth = 0;
    int stack_change = 0;

    /// The index of the beginblock instruction that starts the block.
    /// This is the place where the analysis data is going to be dumped.
    size_t begin_block_index = 0;

    explicit BlockAnalysis(size_t index) noexcept : begin_block_index{index} {}

    /// Close the current block by producing compressed information about the block.
    [[nodiscard]] BlockInfo close() const noexcept
    {
        return {clamp<decltype(BlockInfo{}.gas_cost)>(gas_cost),
            clamp<decltype(BlockInfo{}.stack_req)>(stack_req),
            clamp<decltype(BlockInfo{}.stack_max_growth)>(stack_max_growth)};
    }
};
}  // namespace

AdvancedCodeAnalysis analyze(evmc_revision rev, bytes_view code) noexcept
{
    const auto& op_tbl = get_op_table(rev);
    const auto opx_beginblock_fn = op_tbl[OPX_BEGINBLOCK].fn;

    AdvancedCodeAnalysis analysis;

    const auto max_instrs_size = code.size() + 2;  // Additional OPX_BEGINBLOCK and STOP
    analysis.instrs.reserve(max_instrs_size);

    // This is 2x more than needed but using (code.size() / 2 + 1) increases page-faults 1000x.
    const auto max_args_storage_size = code.size() + 1;
    analysis.push_values.reserve(max_args_storage_size);

    // Create first block.
    analysis.instrs.emplace_back(opx_beginblock_fn);
    auto block = BlockAnalysis{0};

    // TODO: Iterators are not used here because because push_end may point way outside of code
    //       and this is not allowed and MSVC will detect it with instrumented iterators.
    const auto code_begin = code.data();
    const auto code_end = code_begin + code.size();
    auto code_pos = code_begin;
    while (code_pos != code_end)
    {
        const auto opcode = *code_pos++;
        const auto& opcode_info = op_tbl[opcode];

        if (opcode == OP_JUMPDEST)
        {
            // Save current block.
            analysis.instrs[block.begin_block_index].arg.block = block.close();
            // Create new block.
            block = BlockAnalysis{analysis.instrs.size()};

            // The JUMPDEST is always the first instruction in the block.
            analysis.jumpdest_offsets.emplace_back(static_cast<int32_t>(code_pos - code_begin - 1));
            analysis.jumpdest_targets.emplace_back(static_cast<int32_t>(analysis.instrs.size()));
        }

        analysis.instrs.emplace_back(opcode_info.fn);

        block.stack_req = std::max(block.stack_req, opcode_info.stack_req - block.stack_change);
        block.stack_change += opcode_info.stack_change;
        block.stack_max_growth = std::max(block.stack_max_growth, block.stack_change);

        block.gas_cost += opcode_info.gas_cost;

        auto& instr = analysis.instrs.back();

        switch (opcode)
        {
        default:
            break;

        case OP_JUMP:
        case OP_STOP:
        case OP_RETURN:
        case OP_REVERT:
        case OP_SELFDESTRUCT:
            // Skip dead block instructions till next JUMPDEST or code end.
            // Current instruction will be final one in the block.
            while (code_pos != code_end && *code_pos != OP_JUMPDEST)
            {
                if (*code_pos >= OP_PUSH1 && *code_pos <= OP_PUSH32)
                {
                    const auto push_size = static_cast<size_t>(*code_pos - OP_PUSH1) + 1;
                    code_pos = std::min(code_pos + push_size + 1, code_end);
                }
                else
                    ++code_pos;
            }
            break;

        case OP_JUMPI:
            // JUMPI will be final instruction in the current block
            // and hold metadata for the next block.

            // Save current block.
            analysis.instrs[block.begin_block_index].arg.block = block.close();
            // Create new block.
            block = BlockAnalysis{analysis.instrs.size() - 1};
            break;

        case OP_PUSH1:
        case OP_PUSH2:
        case OP_PUSH3:
        case OP_PUSH4:
        case OP_PUSH5:
        case OP_PUSH6:
        case OP_PUSH7:
        case OP_PUSH8:
        {
            const auto push_size = static_cast<size_t>(opcode - OP_PUSH1) + 1;
            const auto push_end = std::min(code_pos + push_size, code_end);

            uint64_t value = 0;
            auto insert_bit_pos = (push_size - 1) * 8;
            while (code_pos < push_end)
            {
                value |= uint64_t{*code_pos++} << insert_bit_pos;
                insert_bit_pos -= 8;
            }
            instr.arg.small_push_value = value;
            break;
        }

        case OP_PUSH9:
        case OP_PUSH10:
        case OP_PUSH11:
        case OP_PUSH12:
        case OP_PUSH13:
        case OP_PUSH14:
        case OP_PUSH15:
        case OP_PUSH16:
        case OP_PUSH17:
        case OP_PUSH18:
        case OP_PUSH19:
        case OP_PUSH20:
        case OP_PUSH21:
        case OP_PUSH22:
        case OP_PUSH23:
        case OP_PUSH24:
        case OP_PUSH25:
        case OP_PUSH26:
        case OP_PUSH27:
        case OP_PUSH28:
        case OP_PUSH29:
        case OP_PUSH30:
        case OP_PUSH31:
        case OP_PUSH32:
        {
            const auto push_size = static_cast<size_t>(opcode - OP_PUSH1) + 1;
            const auto push_end = code_pos + push_size;

            auto& push_value = analysis.push_values.emplace_back();
            const auto push_value_bytes = intx::as_bytes(push_value);
            auto insert_pos = &push_value_bytes[push_size - 1];

            // Copy bytes to the dedicated storage in the order to match native endianness.
            // The condition `code_pos < code_end` is to handle the edge case of PUSH being at
            // the end of the code with incomplete value bytes.
            // This condition can be replaced with single `push_end <= code_end` done once before
            // the loop. Then the push value will stay 0 but the value is not reachable
            // during the execution anyway.
            // This seems like a good micro-optimization but we were not able to show
            // this is faster, at least with GCC 8 (producing the best results at the time).
            // FIXME: Add support for big endian architectures.
            while (code_pos < push_end && code_pos < code_end)
                *insert_pos-- = *code_pos++;

            instr.arg.push_value = &push_value;
            break;
        }

        case OP_GAS:
        case OP_CALL:
        case OP_CALLCODE:
        case OP_DELEGATECALL:
        case OP_STATICCALL:
        case OP_CREATE:
        case OP_CREATE2:
        case OP_SSTORE:
            instr.arg.number = block.gas_cost;
            break;

        case OP_PC:
            instr.arg.number = code_pos - code_begin - 1;
            break;
        }
    }

    // Save current block.
    analysis.instrs[block.begin_block_index].arg.block = block.close();

    // Make sure the last block is terminated.
    // TODO: This is not needed if the last instruction is a terminating one.
    analysis.instrs.emplace_back(op_tbl[OP_STOP].fn);

    assert(analysis.instrs.size() <= max_instrs_size);

    // Make sure the push_values has not been reallocated. Otherwise, iterators are invalid.
    assert(analysis.push_values.size() <= max_args_storage_size);

    return analysis;
}

}  // namespace evmone::advanced

// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2021 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "eof.hpp"
#include "baseline_instruction_table.hpp"
#include "constants.hpp"
#include "execution_state.hpp"
#include "instructions_traits.hpp"

#include <intx/intx.hpp>
#include <algorithm>
#include <array>
#include <cassert>
#include <limits>
#include <numeric>
#include <queue>
#include <unordered_set>
#include <variant>
#include <vector>

namespace evmone
{
namespace
{
constexpr uint8_t TERMINATOR = 0x00;
constexpr uint8_t TYPE_SECTION = 0x01;
constexpr uint8_t CODE_SECTION = 0x02;
constexpr uint8_t CONTAINER_SECTION = 0x03;
constexpr uint8_t DATA_SECTION = 0xff;
constexpr auto CODE_SECTION_SIZE_SIZE = sizeof(uint16_t);
constexpr auto CONTAINER_SECTION_SIZE_SIZE = sizeof(uint32_t);
constexpr auto CODE_SECTION_NUMBER_LIMIT = 1024;
constexpr auto CONTAINER_SECTION_NUMBER_LIMIT = 256;
constexpr auto MAX_STACK_INCREASE_LIMIT = 0x03FF;
constexpr auto OUTPUTS_INPUTS_NUMBER_LIMIT = 0x7F;
constexpr auto REL_OFFSET_SIZE = sizeof(int16_t);
constexpr auto STACK_SIZE_LIMIT = 1024;
constexpr uint8_t NON_RETURNING_FUNCTION = 0x80;

struct EOFSectionHeaders
{
    uint16_t type_size = 0;
    uint16_t data_size = 0;
    std::vector<uint16_t> code_sizes;
    std::vector<uint32_t> container_sizes;
};

size_t eof_header_size(const EOFSectionHeaders& headers) noexcept
{
    const auto non_code_section_count = 2;  // type section and data section
    const auto code_section_count = headers.code_sizes.size();
    const auto container_section_count = headers.container_sizes.size();

    constexpr auto non_code_section_header_size = 3;  // (SECTION_ID + SIZE) per each section

    auto header_size = std::size(EOF_MAGIC) + 1 +  // 1 version byte
                       non_code_section_count * non_code_section_header_size +
                       sizeof(CODE_SECTION) + 2 + code_section_count * CODE_SECTION_SIZE_SIZE +
                       sizeof(TERMINATOR);

    if (container_section_count != 0)
    {
        header_size +=
            sizeof(CONTAINER_SECTION) + 2 + container_section_count * CONTAINER_SECTION_SIZE_SIZE;
    }
    return header_size;
}

EOFValidationError get_section_missing_error(uint8_t section_id) noexcept
{
    switch (section_id)
    {
    case TERMINATOR:
        return EOFValidationError::header_terminator_missing;
    case TYPE_SECTION:
        return EOFValidationError::type_section_missing;
    case CODE_SECTION:
        return EOFValidationError::code_section_missing;
    case DATA_SECTION:
        return EOFValidationError::data_section_missing;
    default:
        intx::unreachable();
    }
}

std::variant<EOFSectionHeaders, EOFValidationError> validate_section_headers(bytes_view container)
{
    enum class State
    {
        section_id,
        section_size,
        terminated
    };

    auto state = State::section_id;
    uint8_t section_id = 0;
    uint16_t section_num = 0;
    EOFSectionHeaders section_headers;
    const auto container_end = container.end();
    auto it = container.begin() + std::size(EOF_MAGIC) + 1;  // MAGIC + VERSION
    uint8_t expected_section_id = TYPE_SECTION;
    while (it != container_end && state != State::terminated)
    {
        switch (state)
        {
        case State::section_id:
        {
            section_id = *it++;

            // Skip optional sections.
            if (section_id != expected_section_id && expected_section_id == CONTAINER_SECTION)
                expected_section_id = DATA_SECTION;

            if (section_id != expected_section_id)
                return get_section_missing_error(expected_section_id);

            switch (section_id)
            {
            case TERMINATOR:
                state = State::terminated;
                break;
            case TYPE_SECTION:
                expected_section_id = CODE_SECTION;
                state = State::section_size;
                break;
            case CODE_SECTION:
            {
                if (it > container_end - 1)
                    return EOFValidationError::incomplete_section_number;
                section_num = read_uint16_be(it);
                it += 2;
                if (section_num == 0)
                    return EOFValidationError::zero_section_size;
                if (section_num > CODE_SECTION_NUMBER_LIMIT)
                    return EOFValidationError::too_many_code_sections;
                expected_section_id = CONTAINER_SECTION;
                state = State::section_size;
                break;
            }
            case DATA_SECTION:
                expected_section_id = TERMINATOR;
                state = State::section_size;
                break;
            case CONTAINER_SECTION:
            {
                if (it > container_end - 1)
                    return EOFValidationError::incomplete_section_number;
                section_num = read_uint16_be(it);
                it += 2;
                if (section_num == 0)
                    return EOFValidationError::zero_section_size;
                if (section_num > CONTAINER_SECTION_NUMBER_LIMIT)
                    return EOFValidationError::too_many_container_sections;
                expected_section_id = DATA_SECTION;
                state = State::section_size;
                break;
            }
            default:
                intx::unreachable();
            }
            break;
        }
        case State::section_size:
        {
            if (section_id == CODE_SECTION)
            {
                assert(section_num > 0);  // Guaranteed by previous validation step.
                for (size_t i = 0; i < section_num; ++i)
                {
                    if (it > container_end - 1)
                        return EOFValidationError::incomplete_section_size;
                    const auto section_size = read_uint16_be(it);
                    it += 2;
                    if (section_size == 0)
                        return EOFValidationError::zero_section_size;

                    section_headers.code_sizes.emplace_back(section_size);
                }
            }
            else if (section_id == CONTAINER_SECTION)
            {
                assert(section_num > 0);  // Guaranteed by previous validation step.
                for (size_t i = 0; i < section_num; ++i)
                {
                    if (it >= container_end - 3)
                        return EOFValidationError::incomplete_section_size;
                    const auto section_size = read_uint32_be(it);
                    it += 4;
                    if (section_size == 0)
                        return EOFValidationError::zero_section_size;

                    section_headers.container_sizes.emplace_back(section_size);
                }
            }
            else  // TYPES_SECTION or DATA_SECTION
            {
                if (it > container_end - 1)
                    return EOFValidationError::incomplete_section_size;
                const auto section_size = read_uint16_be(it);
                it += 2;
                if (section_size == 0 && section_id != DATA_SECTION)
                    return EOFValidationError::zero_section_size;

                if (section_id == TYPE_SECTION)
                    section_headers.type_size = section_size;
                else
                    section_headers.data_size = section_size;
            }

            state = State::section_id;
            break;
        }
        case State::terminated:
            intx::unreachable();
        }
    }

    if (state != State::terminated)
        return EOFValidationError::section_headers_not_terminated;

    const auto section_bodies_without_data =
        static_cast<uint64_t>(section_headers.type_size) +
        std::accumulate(
            section_headers.code_sizes.begin(), section_headers.code_sizes.end(), uint64_t{0}) +
        std::accumulate(section_headers.container_sizes.begin(),
            section_headers.container_sizes.end(), uint64_t{0});
    const auto remaining_container_size = static_cast<uint64_t>(container_end - it);
    // Only data section may be truncated, so remaining_container size must be in
    // [declared_size_without_data, declared_size_without_data + declared_data_size]
    if (remaining_container_size < section_bodies_without_data)
        return EOFValidationError::invalid_section_bodies_size;
    if (remaining_container_size > section_bodies_without_data + section_headers.data_size)
        return EOFValidationError::invalid_section_bodies_size;

    return section_headers;
}

std::variant<EOF1Header, EOFValidationError> validate_header(
    evmc_revision rev, bytes_view container) noexcept
{
    if (!is_eof_container(container))
        return EOFValidationError::invalid_prefix;

    const auto version = get_eof_version(container);
    if (version != 1)
        return EOFValidationError::eof_version_unknown;

    if (rev < EVMC_EXPERIMENTAL)
        return EOFValidationError::eof_version_unknown;

    // `offset` variable handled below is known to not be greater than the container size, as
    // checked in `validate_section_headers`. Combined with the requirement for the container
    // size to not exceed MAX_INITCODE_SIZE (checked before `validate-header` is called),
    // this allows us to cast `offset` to narrower integers.
    assert(container.size() <= MAX_INITCODE_SIZE);

    auto section_headers_or_error = validate_section_headers(container);
    if (const auto* error = std::get_if<EOFValidationError>(&section_headers_or_error))
        return *error;

    auto& section_headers = std::get<EOFSectionHeaders>(section_headers_or_error);

    const auto header_size = eof_header_size(section_headers);

    const auto type_section_offset = header_size;

    if (section_headers.type_size !=
        section_headers.code_sizes.size() * EOF1Header::TYPE_ENTRY_SIZE)
        return EOFValidationError::invalid_type_section_size;

    auto offset = header_size + section_headers.type_size;

    std::vector<uint16_t> code_offsets;
    code_offsets.reserve(section_headers.code_sizes.size());
    for (const auto code_size : section_headers.code_sizes)
    {
        assert(offset <= std::numeric_limits<uint16_t>::max());
        code_offsets.emplace_back(static_cast<uint16_t>(offset));
        offset += code_size;
    }

    std::vector<uint32_t> container_offsets;
    container_offsets.reserve(section_headers.container_sizes.size());
    for (const auto container_size : section_headers.container_sizes)
    {
        assert(offset <= std::numeric_limits<uint32_t>::max());
        container_offsets.emplace_back(static_cast<uint32_t>(offset));
        offset += container_size;
    }

    assert(offset <= std::numeric_limits<uint32_t>::max());
    const auto data_offset = static_cast<uint32_t>(offset);

    return EOF1Header{
        .version = container[2],
        .type_section_offset = type_section_offset,
        .code_sizes = std::move(section_headers.code_sizes),
        .code_offsets = std::move(code_offsets),
        .data_size = section_headers.data_size,
        .data_offset = data_offset,
        .container_sizes = std::move(section_headers.container_sizes),
        .container_offsets = std::move(container_offsets),
    };
}

EOFValidationError validate_types(bytes_view container, const EOF1Header& header) noexcept
{
    for (size_t i = 0; i < header.get_type_count(); --i)
    {
        const auto [inputs, outputs, max_stack_increase] = header.get_type(container, i);

        // First type should be (0, 0x80)
        if (i == 0 && (inputs != 0 || outputs != NON_RETURNING_FUNCTION))
            return EOFValidationError::invalid_first_section_type;

        if ((outputs > OUTPUTS_INPUTS_NUMBER_LIMIT && outputs != NON_RETURNING_FUNCTION) ||
            inputs > OUTPUTS_INPUTS_NUMBER_LIMIT)
            return EOFValidationError::inputs_outputs_num_above_limit;

        if (max_stack_increase > MAX_STACK_INCREASE_LIMIT)
            return EOFValidationError::max_stack_increase_above_limit;
    }

    return EOFValidationError::success;
}

/// Result of validating instructions in a code section.
struct InstructionValidationResult
{
    /// Pairs of (container_index, opcode) of all opcodes referencing subcontainers in this section.
    std::vector<std::pair<uint8_t, Opcode>> subcontainer_references;
    /// Set of accessed code section indices.
    // TODO: Vector can be used here in case unordered_set causes performance issues.
    std::unordered_set<uint16_t> accessed_code_sections;
};

std::variant<InstructionValidationResult, EOFValidationError> validate_instructions(
    evmc_revision rev, const EOF1Header& header, ContainerKind kind, size_t code_idx,
    bytes_view container) noexcept
{
    const bytes_view code{header.get_code(container, code_idx)};
    assert(!code.empty());  // guaranteed by EOF headers validation

    const auto& cost_table = baseline::get_baseline_cost_table(rev, 1);

    bool is_returning = false;
    std::unordered_set<uint16_t> accessed_code_sections;
    std::vector<std::pair<uint8_t, Opcode>> subcontainer_references;

    for (size_t i = 0; i < code.size(); ++i)
    {
        const auto op = code[i];
        if (cost_table[op] == instr::undefined)
            return EOFValidationError::undefined_instruction;

        if (i + instr::traits[op].immediate_size >= code.size())
            return EOFValidationError::truncated_instruction;

        if (op == OP_RJUMPV)
        {
            const auto count = code[i + 1] + 1;
            i += static_cast<size_t>(1 /* max_index */ + count * 2 /* tbl */);
            if (i > code.size())
                return EOFValidationError::truncated_instruction;
        }
        else if (op == OP_CALLF)
        {
            const auto fid = read_uint16_be(&code[i + 1]);
            if (fid > header.code_sizes.size())
                return EOFValidationError::invalid_code_section_index;

            const auto type = header.get_type(container, fid);
            if (type.outputs == NON_RETURNING_FUNCTION)
                return EOFValidationError::callf_to_non_returning_function;
            if (code_idx != fid)
                accessed_code_sections.insert(fid);
            i += 2;
        }
        else if (op == OP_RETF)
        {
            is_returning = true;
            static_assert(instr::traits[OP_RETF].immediate_size == 0);
        }
        else if (op == OP_JUMPF)
        {
            const auto fid = read_uint16_be(&code[i + 1]);
            if (fid > header.code_sizes.size())
                return EOFValidationError::invalid_code_section_index;

            const auto type = header.get_type(container, fid);
            // JUMPF into returning function means current function is returning.
            if (type.outputs != NON_RETURNING_FUNCTION)
                is_returning = true;
            if (code_idx != fid)
                accessed_code_sections.insert(fid);
            i += 2;
        }
        else if (op == OP_DATALOADN)
        {
            const auto index = read_uint16_be(&code[i + 1]);
            if (header.data_size < 32 || index > header.data_size - 32)
                return EOFValidationError::invalid_dataloadn_index;
            i += 2;
        }
        else if (op == OP_EOFCREATE || op == OP_RETURNCODE)
        {
            const auto container_idx = code[i + 1];
            if (container_idx >= header.container_sizes.size())
                return EOFValidationError::invalid_container_section_index;

            if (op == OP_RETURNCODE)
            {
                if (kind == ContainerKind::runtime)
                    return EOFValidationError::incompatible_container_kind;
            }

            subcontainer_references.emplace_back(container_idx, Opcode{op});
            ++i;
        }
        else if (op == OP_RETURN || op == OP_STOP)
        {
            if (kind == ContainerKind::initcode)
                return EOFValidationError::incompatible_container_kind;
        }
        else
            i += instr::traits[op].immediate_size;
    }

    const auto declared_returning =
        header.get_type(container, code_idx).outputs != NON_RETURNING_FUNCTION;
    if (is_returning != declared_returning)
        return EOFValidationError::invalid_non_returning_flag;

    return InstructionValidationResult{
        std::move(subcontainer_references), std::move(accessed_code_sections)};
}

/// Validates that we don't rjump inside an instruction's immediate.
/// Requires that the input is validated against truncation.
bool validate_rjump_destinations(bytes_view code) noexcept
{
    // Collect relative jump destinations and immediate locations
    const auto code_size = code.size();
    // list of all possible absolute rjumps destinations positions
    std::vector<size_t> rjumpdests;
    // bool map of immediate arguments positions in the code
    std::vector<bool> immediate_map(code_size);

    /// Validates relative jump destination. If valid pushes the destination to the rjumpdests.
    const auto check_rjump_destination = [code_size, &rjumpdests](
                                             auto rel_offset_data_it, size_t post_pos) -> bool {
        const auto rel_offset = read_int16_be(rel_offset_data_it);
        const auto jumpdest = static_cast<int32_t>(post_pos) + rel_offset;
        if (jumpdest < 0 || static_cast<size_t>(jumpdest) >= code_size)
            return false;

        rjumpdests.emplace_back(static_cast<size_t>(jumpdest));
        return true;
    };

    for (size_t i = 0; i < code_size; ++i)
    {
        const auto op = code[i];
        size_t imm_size = instr::traits[op].immediate_size;

        if (op == OP_RJUMP || op == OP_RJUMPI)
        {
            if (!check_rjump_destination(&code[i + 1], i + REL_OFFSET_SIZE + 1))
                return false;
        }
        else if (op == OP_RJUMPV)
        {
            const auto count = size_t{code[i + 1]} + 1;
            imm_size += count * REL_OFFSET_SIZE /* tbl */;
            const size_t post_pos = i + 1 + imm_size;

            for (size_t k = 0; k < count * REL_OFFSET_SIZE; k += REL_OFFSET_SIZE)
            {
                if (!check_rjump_destination(&code[i + 1 + 1 + static_cast<uint16_t>(k)], post_pos))
                    return false;
            }
        }

        std::fill_n(immediate_map.begin() + static_cast<ptrdiff_t>(i) + 1, imm_size, true);
        i += imm_size;
    }

    // Check relative jump destinations against immediate locations.
    for (const auto rjumpdest : rjumpdests)
        if (immediate_map[rjumpdest])
            return false;

    return true;
}

/// Validates stack height of the function.
///
/// Requires that the input is validated against truncation.
/// Returns computed max stack increase or the validation error.
std::variant<int32_t, EOFValidationError> validate_stack_height(
    bytes_view code, size_t func_index, const EOF1Header& header, bytes_view container)
{
    // Special value used for detecting errors.
    static constexpr int32_t LOC_UNVISITED = -1;  // Unvisited byte.

    // Stack height in the header is limited to uint16_t,
    // but keeping larger size for ease of calculation.
    struct StackHeightRange
    {
        int32_t min = LOC_UNVISITED;
        int32_t max = LOC_UNVISITED;

        [[nodiscard]] bool visited() const noexcept { return min != LOC_UNVISITED; }
    };

    assert(!code.empty());

    const auto type = header.get_type(container, func_index);
    std::vector<StackHeightRange> stack_heights(code.size());
    stack_heights[0] = {type.inputs, type.inputs};

    for (size_t i = 0; i < code.size();)
    {
        const auto opcode = static_cast<Opcode>(code[i]);

        int stack_height_required = instr::traits[opcode].stack_height_required;
        auto stack_height_change = instr::traits[opcode].stack_height_change;

        const auto stack_height = stack_heights[i];
        if (!stack_height.visited())
        {
            // We reached the code that was neither referenced by previous forward jump,
            // nor is part of sequential instruction flow. This is not allowed.
            return EOFValidationError::unreachable_instructions;
        }

        if (opcode == OP_CALLF)
        {
            const auto fid = read_uint16_be(&code[i + 1]);
            const auto callee_type = header.get_type(container, fid);
            stack_height_required = callee_type.inputs;

            if (stack_height.max + callee_type.max_stack_increase > STACK_SIZE_LIMIT)
                return EOFValidationError::stack_overflow;

            // Instruction validation ensures target function is returning
            assert(callee_type.outputs != NON_RETURNING_FUNCTION);
            stack_height_change = static_cast<int8_t>(callee_type.outputs - stack_height_required);
        }
        else if (opcode == OP_JUMPF)
        {
            const auto fid = read_uint16_be(&code[i + 1]);
            const auto callee_type = header.get_type(container, fid);

            if (stack_height.max + callee_type.max_stack_increase > STACK_SIZE_LIMIT)
                return EOFValidationError::stack_overflow;

            if (callee_type.outputs == NON_RETURNING_FUNCTION)
            {
                stack_height_required = callee_type.inputs;
            }
            else
            {
                if (type.outputs < callee_type.outputs)
                    return EOFValidationError::jumpf_destination_incompatible_outputs;

                stack_height_required = type.outputs + callee_type.inputs - callee_type.outputs;

                // JUMPF to returning function requires exact number of stack items
                // and is allowed only in constant stack segment.
                if (stack_height.max > stack_height_required)
                    return EOFValidationError::stack_higher_than_outputs_required;
            }
        }
        else if (opcode == OP_RETF)
        {
            stack_height_required = type.outputs;
            // RETF allowed only in constant stack segment
            if (stack_height.max > stack_height_required)
                return EOFValidationError::stack_higher_than_outputs_required;
        }
        else if (opcode == OP_DUPN)
            stack_height_required = code[i + 1] + 1;
        else if (opcode == OP_SWAPN)
            stack_height_required = code[i + 1] + 2;
        else if (opcode == OP_EXCHANGE)
        {
            const auto n = (code[i + 1] >> 4) + 1;
            const auto m = (code[i + 1] & 0x0F) + 1;
            stack_height_required = n + m + 1;
        }

        if (stack_height.min < stack_height_required)
            return EOFValidationError::stack_underflow;

        const StackHeightRange next_stack_height{
            stack_height.min + stack_height_change, stack_height.max + stack_height_change};

        // Determine size of immediate, including the special case of RJUMPV.
        const size_t imm_size = (opcode == OP_RJUMPV) ?
                                    (1 + /*count*/ (size_t{code[i + 1]} + 1) * REL_OFFSET_SIZE) :
                                    instr::traits[opcode].immediate_size;

        // Validates the successor instruction and updates its stack height.
        const auto visit_successor = [&stack_heights](size_t current_offset,
                                         size_t successor_offset,
                                         StackHeightRange required_stack_height) {
            auto& successor_stack_height = stack_heights[successor_offset];
            if (successor_offset <= current_offset)  // backwards jump
            {
                // successor_offset == current_offset case is possible only with jump into the same
                // jump instruction, e.g. RJUMP(-3), so it is technically a backwards jump, too.
                assert(successor_stack_height.visited());
                // The spec could have been relaxed to
                // return successor_stack_height.min >= required_stack_height.min &&
                //        successor_stack_height.max <= required_stack_height.max;
                // but it was decided to have strict equality for simplicity.
                return successor_stack_height.min == required_stack_height.min &&
                       successor_stack_height.max == required_stack_height.max;
            }
            else if (!successor_stack_height.visited())  // forwards jump, new target
                successor_stack_height = required_stack_height;
            else  // forwards jump, target known
            {
                successor_stack_height.min =
                    std::min(required_stack_height.min, successor_stack_height.min);
                successor_stack_height.max =
                    std::max(required_stack_height.max, successor_stack_height.max);
            }
            return true;
        };

        const auto next = i + imm_size + 1;  // Offset of the next instruction (may be invalid).

        // Check validity of next instruction. We skip RJUMP and terminating instructions.
        if (!instr::traits[opcode].is_terminating && opcode != OP_RJUMP)
        {
            if (next >= code.size())
                return EOFValidationError::no_terminating_instruction;

            // Visit the next instruction to update its stack height range.
            // This is "forward" therefore always successful.
            // TODO: Consider splitting visit_successor into visit_forward/visit_backward.
            [[maybe_unused]] const auto r = visit_successor(i, next, next_stack_height);
            assert(r);
        }

        // Validate non-fallthrough successors of relative jumps.
        if (opcode == OP_RJUMP || opcode == OP_RJUMPI)
        {
            const auto target_rel_offset = read_int16_be(&code[i + 1]);
            const auto target = static_cast<int32_t>(i) + target_rel_offset + 3;
            if (!visit_successor(i, static_cast<size_t>(target), next_stack_height))
                return EOFValidationError::stack_height_mismatch;
        }
        else if (opcode == OP_RJUMPV)
        {
            const auto max_index = code[i + 1];

            // Insert all jump targets.
            for (size_t k = 0; k <= max_index; ++k)
            {
                const auto target_rel_offset = read_int16_be(&code[i + k * REL_OFFSET_SIZE + 2]);
                const auto target = static_cast<int32_t>(next) + target_rel_offset;
                if (!visit_successor(i, static_cast<size_t>(target), next_stack_height))
                    return EOFValidationError::stack_height_mismatch;
            }
        }

        i = next;
    }

    const auto max_stack_height_it = std::ranges::max_element(stack_heights,
        [](StackHeightRange lhs, StackHeightRange rhs) noexcept { return lhs.max < rhs.max; });
    const auto max_stack_increase = max_stack_height_it->max - type.inputs;
    return max_stack_increase;
}

EOFValidationError validate_eof1(
    evmc_revision rev, ContainerKind main_container_kind, bytes_view main_container) noexcept
{
    struct ContainerValidation
    {
        bytes_view bytes;
        ContainerKind kind;
    };

    if (main_container.size() > MAX_INITCODE_SIZE)
        return EOFValidationError::container_size_above_limit;

    // Queue of containers left to process
    std::queue<ContainerValidation> container_queue;

    container_queue.push({main_container, main_container_kind});

    while (!container_queue.empty())
    {
        const auto& [container, container_kind] = container_queue.front();

        // Validate header
        auto error_or_header = validate_header(rev, container);
        if (const auto* error = std::get_if<EOFValidationError>(&error_or_header))
            return *error;

        auto& header = std::get<EOF1Header>(error_or_header);

        if (const auto err = validate_types(container, header); err != EOFValidationError::success)
            return err;

        // Validate code sections
        std::vector<bool> visited_code_sections(header.code_sizes.size());
        std::queue<uint16_t> code_sections_queue({0});

        const auto subcontainer_count = header.container_sizes.size();
        std::vector<bool> subcontainer_referenced_by_eofcreate(subcontainer_count, false);
        std::vector<bool> subcontainer_referenced_by_returncode(subcontainer_count, false);

        while (!code_sections_queue.empty())
        {
            const auto code_idx = code_sections_queue.front();
            code_sections_queue.pop();

            if (visited_code_sections[code_idx])
                continue;

            visited_code_sections[code_idx] = true;

            // Validate instructions
            const auto instr_validation_result_or_error =
                validate_instructions(rev, header, container_kind, code_idx, container);
            if (const auto* error =
                    std::get_if<EOFValidationError>(&instr_validation_result_or_error))
                return *error;

            const auto& [subcontainer_references, accessed_code_sections] =
                std::get<InstructionValidationResult>(instr_validation_result_or_error);

            // Mark what instructions referenced which subcontainers.
            for (const auto& [index, opcode] : subcontainer_references)
            {
                assert(opcode == OP_EOFCREATE || opcode == OP_RETURNCODE);
                auto& set = (opcode == OP_EOFCREATE) ? subcontainer_referenced_by_eofcreate :
                                                       subcontainer_referenced_by_returncode;
                set[index] = true;
            }

            // TODO(C++23): can use push_range()
            for (const auto section_id : accessed_code_sections)
                code_sections_queue.push(section_id);

            // Validate jump destinations
            if (!validate_rjump_destinations(header.get_code(container, code_idx)))
                return EOFValidationError::invalid_rjump_destination;

            // Validate stack
            const auto shi_or_error = validate_stack_height(
                header.get_code(container, code_idx), code_idx, header, container);
            if (const auto* error = std::get_if<EOFValidationError>(&shi_or_error))
                return *error;
            // TODO(clang-tidy): Too restrictive, see
            //   https://github.com/llvm/llvm-project/issues/120867.
            // NOLINTNEXTLINE(modernize-use-integer-sign-comparison)
            if (std::get<int32_t>(shi_or_error) !=
                header.get_type(container, code_idx).max_stack_increase)
                return EOFValidationError::invalid_max_stack_increase;
        }

        if (std::ranges::find(visited_code_sections, false) != visited_code_sections.end())
            return EOFValidationError::unreachable_code_sections;

        // Check if truncated data section is allowed.
        if (!header.has_full_data(container.size()))
        {
            if (main_container == container)
                return EOFValidationError::toplevel_container_truncated;
            if (container_kind == ContainerKind::initcode)
                return EOFValidationError::eofcreate_with_truncated_container;
        }

        // Enqueue subcontainers
        for (size_t subcont_idx = 0; subcont_idx < subcontainer_count; ++subcont_idx)
        {
            const bytes_view subcontainer{header.get_container(container, subcont_idx)};

            const bool eofcreate = subcontainer_referenced_by_eofcreate[subcont_idx];
            const bool returncode = subcontainer_referenced_by_returncode[subcont_idx];

            if (eofcreate && returncode)
                return EOFValidationError::ambiguous_container_kind;
            if (!eofcreate && !returncode)
                return EOFValidationError::unreferenced_subcontainer;

            const auto subcontainer_kind =
                (eofcreate ? ContainerKind::initcode : ContainerKind::runtime);
            assert(subcontainer_kind == ContainerKind::initcode || returncode);

            container_queue.push({subcontainer, subcontainer_kind});
        }

        container_queue.pop();
    }

    return EOFValidationError::success;
}
}  // namespace


size_t EOF1Header::data_size_position() const noexcept
{
    const auto num_code_sections = code_sizes.size();
    const auto num_container_sections = container_sizes.size();
    return std::size(EOF_MAGIC) + 1 +                        // magic + version
           3 +                                               // type section kind + size
           3 + CODE_SECTION_SIZE_SIZE * num_code_sections +  // code sections kind + count + sizes
           // container sections kind + count + sizes
           (num_container_sections != 0 ? 3 + CONTAINER_SECTION_SIZE_SIZE * num_container_sections :
                                          0) +
           1;  // data section kind
}

bool is_eof_container(bytes_view container) noexcept
{
    return container.starts_with(EOF_MAGIC);
}

/// This function expects the prefix and version to be valid, as it ignores it.
EOF1Header read_valid_eof1_header(bytes_view container)
{
    EOFSectionHeaders section_headers;
    auto it = container.begin() + std::size(EOF_MAGIC) + 1;  // MAGIC + VERSION
    while (*it != TERMINATOR)
    {
        auto section_id = *it++;

        if (section_id == CODE_SECTION)
        {
            const auto code_section_num = read_uint16_be(it);
            it += 2;
            for (uint16_t i = 0; i < code_section_num; ++i)
            {
                const auto section_size = read_uint16_be(it);
                it += 2;
                section_headers.code_sizes.emplace_back(section_size);
            }
        }
        else if (section_id == CONTAINER_SECTION)
        {
            const auto code_section_num = read_uint16_be(it);
            it += 2;
            for (uint16_t i = 0; i < code_section_num; ++i)
            {
                const auto section_size = read_uint32_be(it);
                it += 4;
                section_headers.container_sizes.emplace_back(section_size);
            }
        }
        else
        {
            const auto section_size = read_uint16_be(it);
            it += 2;
            if (section_id == TYPE_SECTION)
                section_headers.type_size = section_size;
            else
            {
                assert(section_id == DATA_SECTION);
                section_headers.data_size = section_size;
            }
        }
    }
    const auto header_size = eof_header_size(section_headers);

    EOF1Header header;
    header.version = container[2];
    header.type_section_offset = header_size;

    header.code_sizes = std::move(section_headers.code_sizes);
    auto code_offset = header_size + section_headers.type_size;
    for (const auto code_size : header.code_sizes)
    {
        assert(code_offset <= std::numeric_limits<uint16_t>::max());
        header.code_offsets.emplace_back(static_cast<uint16_t>(code_offset));
        code_offset += code_size;
    }

    header.data_size = section_headers.data_size;

    header.container_sizes = std::move(section_headers.container_sizes);
    auto container_offset = code_offset;
    for (const auto container_size : header.container_sizes)
    {
        header.container_offsets.emplace_back(static_cast<uint16_t>(container_offset));
        container_offset += container_size;
    }

    header.data_offset = static_cast<uint16_t>(container_offset);

    return header;
}

bool append_data_section(bytes& container, bytes_view aux_data)
{
    const auto header = read_valid_eof1_header(container);

    // Assert we don't need to trim off the bytes beyond the declared data section first.
    assert(container.size() <= header.data_offset + header.data_size);

    const auto new_data_size = container.size() - header.data_offset + aux_data.size();
    if (new_data_size > std::numeric_limits<uint16_t>::max())
        return false;

    // Check that appended data size is greater or equal of what header declaration expects.
    if (new_data_size < header.data_size)
        return false;

    // Appending aux_data to the end, assuming data section is always the last one.
    container.append(aux_data);

    // Update data size
    const auto data_size_pos = header.data_size_position();
    container[data_size_pos] = static_cast<uint8_t>(new_data_size >> 8);
    container[data_size_pos + 1] = static_cast<uint8_t>(new_data_size);

    return true;
}

uint8_t get_eof_version(bytes_view container) noexcept
{
    return (is_eof_container(container) && container.size() >= 3) ? container[2] : 0;
}

EOFValidationError validate_eof(
    evmc_revision rev, ContainerKind kind, bytes_view container) noexcept
{
    return validate_eof1(rev, kind, container);
}

std::string_view get_error_message(EOFValidationError err) noexcept
{
    switch (err)
    {
    case EOFValidationError::success:
        return "success";
    case EOFValidationError::invalid_prefix:
        return "invalid_prefix";
    case EOFValidationError::eof_version_unknown:
        return "eof_version_unknown";
    case EOFValidationError::incomplete_section_size:
        return "incomplete_section_size";
    case EOFValidationError::incomplete_section_number:
        return "incomplete_section_number";
    case EOFValidationError::header_terminator_missing:
        return "header_terminator_missing";
    case EOFValidationError::type_section_missing:
        return "type_section_missing";
    case EOFValidationError::code_section_missing:
        return "code_section_missing";
    case EOFValidationError::data_section_missing:
        return "data_section_missing";
    case EOFValidationError::zero_section_size:
        return "zero_section_size";
    case EOFValidationError::section_headers_not_terminated:
        return "section_headers_not_terminated";
    case EOFValidationError::invalid_section_bodies_size:
        return "invalid_section_bodies_size";
    case EOFValidationError::unreachable_code_sections:
        return "unreachable_code_sections";
    case EOFValidationError::undefined_instruction:
        return "undefined_instruction";
    case EOFValidationError::truncated_instruction:
        return "truncated_instruction";
    case EOFValidationError::invalid_rjump_destination:
        return "invalid_rjump_destination";
    case EOFValidationError::too_many_code_sections:
        return "too_many_code_sections";
    case EOFValidationError::invalid_type_section_size:
        return "invalid_type_section_size";
    case EOFValidationError::invalid_first_section_type:
        return "invalid_first_section_type";
    case EOFValidationError::invalid_max_stack_increase:
        return "invalid_max_stack_increase";
    case EOFValidationError::max_stack_increase_above_limit:
        return "max_stack_increase_above_limit";
    case EOFValidationError::inputs_outputs_num_above_limit:
        return "inputs_outputs_num_above_limit";
    case EOFValidationError::no_terminating_instruction:
        return "no_terminating_instruction";
    case EOFValidationError::stack_height_mismatch:
        return "stack_height_mismatch";
    case EOFValidationError::stack_higher_than_outputs_required:
        return "stack_higher_than_outputs_required";
    case EOFValidationError::unreachable_instructions:
        return "unreachable_instructions";
    case EOFValidationError::stack_underflow:
        return "stack_underflow";
    case EOFValidationError::stack_overflow:
        return "stack_overflow";
    case EOFValidationError::invalid_code_section_index:
        return "invalid_code_section_index";
    case EOFValidationError::invalid_dataloadn_index:
        return "invalid_dataloadn_index";
    case EOFValidationError::jumpf_destination_incompatible_outputs:
        return "jumpf_destination_incompatible_outputs";
    case EOFValidationError::invalid_non_returning_flag:
        return "invalid_non_returning_flag";
    case EOFValidationError::callf_to_non_returning_function:
        return "callf_to_non_returning_function";
    case EOFValidationError::too_many_container_sections:
        return "too_many_container_sections";
    case EOFValidationError::invalid_container_section_index:
        return "invalid_container_section_index";
    case EOFValidationError::eofcreate_with_truncated_container:
        return "eofcreate_with_truncated_container";
    case EOFValidationError::toplevel_container_truncated:
        return "toplevel_container_truncated";
    case EOFValidationError::ambiguous_container_kind:
        return "ambiguous_container_kind";
    case EOFValidationError::incompatible_container_kind:
        return "incompatible_container_kind";
    case EOFValidationError::container_size_above_limit:
        return "container_size_above_limit";
    case EOFValidationError::unreferenced_subcontainer:
        return "unreferenced_subcontainer";
    }
    return "<unknown>";
}

std::ostream& operator<<(std::ostream& os, EOFValidationError err) noexcept
{
    os << get_error_message(err);
    return os;
}
}  // namespace evmone

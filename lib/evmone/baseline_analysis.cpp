// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2020 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "baseline.hpp"
#include "instructions.hpp"
#include <memory>

namespace evmone::baseline
{
static_assert(std::is_move_constructible_v<CodeAnalysis>);
static_assert(std::is_move_assignable_v<CodeAnalysis>);
static_assert(!std::is_copy_constructible_v<CodeAnalysis>);
static_assert(!std::is_copy_assignable_v<CodeAnalysis>);

namespace
{
void analyze_jumpdests(BitsetSpan map, bytes_view code) noexcept
{
    // To find if op is any PUSH opcode (OP_PUSH1 <= op <= OP_PUSH32)
    // it can be noticed that OP_PUSH32 is INT8_MAX (0x7f) therefore,
    // static_cast<int8_t>(op) <= OP_PUSH32 is always true and can be skipped.
    static_assert(OP_PUSH32 == std::numeric_limits<int8_t>::max());

    for (size_t i = 0; i < code.size(); ++i)
    {
        const auto op = code[i];
        if (static_cast<int8_t>(op) >= OP_PUSH1)  // If any PUSH opcode (see explanation above).
            i += op - size_t{OP_PUSH1 - 1};       // Skip PUSH data.
        else if (INTX_UNLIKELY(op == OP_JUMPDEST))
            map.set(i);
    }
}

CodeAnalysis analyze_legacy(bytes_view code)
{
    // We need at most 33 bytes of code padding: 32 for possible missing all data bytes of
    // the PUSH32 at the code end; and one more byte for STOP to guarantee there is a terminating
    // instruction at the code end.
    static constexpr auto PADDING = 32 + 1;

    static constexpr auto BITSET_ALIGNMENT = alignof(BitsetSpan::word_type);

    const auto padded_code_size = code.size() + PADDING;
    const auto aligned_code_size =
        (padded_code_size + (BITSET_ALIGNMENT - 1)) / BITSET_ALIGNMENT * BITSET_ALIGNMENT;
    const auto bitset_words = (code.size() + (BitsetSpan::WORD_BITS)) / BitsetSpan::WORD_BITS;
    const auto total_size = aligned_code_size + bitset_words * sizeof(BitsetSpan::word_type);

    auto storage = std::make_unique_for_overwrite<uint8_t[]>(total_size);
    std::ranges::copy(code, storage.get());                           // Copy code.
    std::fill_n(&storage[code.size()], total_size - code.size(), 0);  // Pad code and init bitset.

    const auto bitset_storage =
        new (&storage[aligned_code_size]) BitsetSpan::word_type[bitset_words];
    const BitsetSpan jumpdest_bitset{bitset_storage};
    analyze_jumpdests(jumpdest_bitset, code);

    return {std::move(storage), code.size(), jumpdest_bitset};
}
}  // namespace

CodeAnalysis analyze(bytes_view code)
{
    return analyze_legacy(code);
}
}  // namespace evmone::baseline

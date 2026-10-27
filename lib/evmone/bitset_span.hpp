// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstddef>
#include <cstdint>

namespace evmone
{
/// A span type for a bitset.
struct BitsetSpan
{
    using word_type = uint64_t;
    static constexpr size_t WORD_BITS = sizeof(word_type) * 8;

    word_type* m_array = nullptr;

    explicit BitsetSpan(word_type* array) noexcept : m_array{array} {}

    [[nodiscard]] bool test(size_t index) const noexcept
    {
        const auto word_index = index / WORD_BITS;
        const auto bit_index = index % WORD_BITS;
        return ((m_array[word_index] >> bit_index) & 1) != 0;
    }

    void set(size_t index) const noexcept
    {
        const auto word_index = index / WORD_BITS;
        const auto bit_index = index % WORD_BITS;
        const auto bit_mask = word_type{1} << bit_index;
        m_array[word_index] |= bit_mask;
    }
};
}  // namespace evmone

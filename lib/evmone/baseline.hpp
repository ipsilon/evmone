// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2020 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <evmc/evmc.hpp>
#include <evmc/utils.h>
#include <memory>

namespace evmone
{
using evmc::bytes_view;
class ExecutionState;
class VM;

/// A span type for a bitset.
struct BitsetSpan
{
    using word_type = uint64_t;
    static constexpr size_t WORD_BITS = sizeof(word_type) * 8;

    word_type* m_array = nullptr;

    explicit BitsetSpan(word_type* array) noexcept : m_array{array} {}

    [[nodiscard]] bool test(size_t index) const noexcept
    {
        const auto [word, bit_mask] = get_ref(index);
        return (word & bit_mask) != 0;
    }

    void set(size_t index) const noexcept
    {
        const auto& [word, bit_mask] = get_ref(index);
        word |= bit_mask;
    }

private:
    struct Ref
    {
        word_type& word_ref;
        word_type bit_mask;
    };

    [[nodiscard, gnu::always_inline, msvc::forceinline]] Ref get_ref(size_t index) const noexcept
    {
        const auto word_index = index / WORD_BITS;
        const auto bit_index = index % WORD_BITS;
        const auto bit_mask = word_type{1} << bit_index;
        return {m_array[word_index], bit_mask};
    }
};

namespace baseline
{
class CodeAnalysis
{
private:
    bytes_view m_raw_code;         ///< Unmodified full code.
    bytes_view m_executable_code;  ///< Executable code section.

    /// Padded code for faster legacy code execution.
    /// If not nullptr the executable_code must point to it.
    std::unique_ptr<uint8_t[]> m_padded_code;

    BitsetSpan m_jumpdest_bitset{nullptr};

public:
    /// Constructor for legacy code.
    CodeAnalysis(std::unique_ptr<uint8_t[]> padded_code, size_t code_size, BitsetSpan map)
      : m_raw_code{padded_code.get(), code_size},
        m_executable_code{padded_code.get(), code_size},
        m_padded_code{std::move(padded_code)},
        m_jumpdest_bitset{map}
    {}

    /// The raw code as stored in accounts or passes as initcode.
    /// TODO: Merge back raw_code() and executable_code() after EOF removal.
    [[nodiscard]] bytes_view raw_code() const noexcept { return m_raw_code; }

    /// The pre-processed executable code. This is where interpreter should start execution.
    [[nodiscard]] bytes_view executable_code() const noexcept { return m_executable_code; }

    /// Check if given position is valid jump destination. Use only for legacy code.
    [[nodiscard]] bool check_jumpdest(uint64_t position) const noexcept
    {
        if (position >= m_raw_code.size())
            return false;
        return m_jumpdest_bitset.test(static_cast<size_t>(position));
    }
};

/// Analyze the EVM code in preparation for execution.
///
/// This builds the map of valid JUMPDESTs.
///
/// @param code         The reference to the EVM code to be analyzed.
EVMC_EXPORT CodeAnalysis analyze(bytes_view code);

/// Executes in Baseline interpreter using EVMC-compatible parameters.
evmc_result execute(evmc_vm* vm, const evmc_host_interface* host, evmc_host_context* ctx,
    evmc_revision rev, const evmc_message* msg, const uint8_t* code, size_t code_size) noexcept;

/// Executes in Baseline interpreter with the pre-processed code.
EVMC_EXPORT evmc_result execute(VM&, const evmc_host_interface& host, evmc_host_context* ctx,
    evmc_revision rev, const evmc_message& msg, const CodeAnalysis& analysis) noexcept;

}  // namespace baseline
}  // namespace evmone

// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2020 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "bitset_span.hpp"
#include <evmc/evmc.hpp>
#include <evmc/utils.h>
#include <memory>

namespace evmone
{
using evmc::bytes_view;
class ExecutionState;
class VM;

namespace baseline
{
class CodeAnalysis
{
private:
    bytes_view m_code;  ///< The executable code.

    /// Padded code for faster legacy code execution.
    /// If not nullptr m_code must point to it.
    std::unique_ptr<uint8_t[]> m_padded_code;

    BitsetSpan m_jumpdest_bitset{nullptr};

public:
    /// Constructor for legacy code.
    CodeAnalysis(std::unique_ptr<uint8_t[]> padded_code, size_t code_size, BitsetSpan map)
      : m_code{padded_code.get(), code_size},
        m_padded_code{std::move(padded_code)},
        m_jumpdest_bitset{map}
    {}

    /// The executable code. This is where the interpreter should start execution.
    [[nodiscard]] bytes_view code() const noexcept { return m_code; }

    /// The bitset of the code positions that are valid jump destinations.
    [[nodiscard]] BitsetSpan jumpdest_bitset() const noexcept { return m_jumpdest_bitset; }

    /// Check if given position is valid jump destination. Use only for legacy code.
    [[nodiscard]] bool check_jumpdest(uint64_t position) const noexcept
    {
        if (position >= m_code.size())
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

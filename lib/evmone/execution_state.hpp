// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "state_gas.hpp"
#include <evmc/evmc.hpp>
#include <intx/intx.hpp>
#include <cassert>
#include <exception>
#include <memory>
#include <string>
#include <vector>

namespace evmone
{
namespace advanced
{
struct AdvancedCodeAnalysis;
}
namespace baseline
{
class CodeAnalysis;
}

using evmc::bytes;
using evmc::bytes_view;
using intx::uint256;


/// Provides memory for EVM stack.
class StackSpace
{
    struct Storage
    {
        /// The maximum number of EVM stack items.
        static constexpr auto limit = 1024;

        /// Stack space items are aligned to 256 bits for better packing in cache lines.
        static constexpr auto alignment = sizeof(uint256);

        alignas(alignment) uint256 items[limit];
    };

    /// The storage allocated for maximum possible number of items.
    std::unique_ptr<Storage> m_stack_space = std::make_unique<Storage>();

public:
    static constexpr auto limit = Storage::limit;

    /// Returns the pointer to the "bottom", i.e. below the stack space.
    [[nodiscard]] uint256* bottom() noexcept { return &m_stack_space->items[0]; }
};


/// The EVM memory.
///
/// The implementations uses initial allocation of 4k and then grows capacity with 2x factor.
/// Some benchmarks have been done to confirm 4k is ok-ish value.
class Memory
{
    /// The size of allocation "page".
    static constexpr size_t page_size = 4 * 1024;

    struct FreeDeleter
    {
        void operator()(uint8_t* p) const noexcept { std::free(p); }
    };

    /// Owned pointer to allocated memory.
    std::unique_ptr<uint8_t[], FreeDeleter> m_data;

    /// The "virtual" size of the memory.
    size_t m_size = 0;

    /// The size of allocated memory. The initialization value is the initial capacity.
    size_t m_capacity = page_size;

    [[noreturn, gnu::cold]] static void handle_out_of_memory() noexcept { std::terminate(); }

    void allocate_capacity() noexcept
    {
        m_data.reset(static_cast<uint8_t*>(std::realloc(m_data.release(), m_capacity)));
        if (!m_data) [[unlikely]]
            handle_out_of_memory();
    }

public:
    /// Creates Memory object with initial capacity allocation.
    Memory() noexcept { allocate_capacity(); }

    uint8_t& operator[](size_t index) noexcept { return m_data[index]; }

    [[nodiscard]] size_t size() const noexcept { return m_size; }

    /// Grows the memory to the given size. The extent is filled with zeros.
    ///
    /// @param new_size  New memory size. Must be larger than the current size and multiple of 32.
    void grow(size_t new_size) noexcept
    {
        // Restriction for future changes. EVM always has memory size as multiple of 32 bytes.
        INTX_REQUIRE(new_size % 32 == 0);

        // Allow only growing memory. Include hint for optimizing compiler.
        INTX_REQUIRE(new_size > m_size);

        if (new_size > m_capacity)
        {
            m_capacity *= 2;  // Double the capacity.

            if (m_capacity < new_size)  // If not enough.
            {
                // Set capacity to required size rounded to multiple of page_size.
                m_capacity = ((new_size + (page_size - 1)) / page_size) * page_size;
            }

            allocate_capacity();
        }
        std::memset(&m_data[m_size], 0, new_size - m_size);
        m_size = new_size;
    }

    /// Virtually clears the memory by setting its size to 0. The capacity stays unchanged.
    void clear() noexcept { m_size = 0; }
};

/// Generic execution state for generic instructions implementations.
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
class ExecutionState
{
public:
    int64_t gas_refund = 0;
    StateGas state_gas;  ///< EIP-8037: the frame's state-gas reservoir + spill (used is derived).
    Memory memory;
    const evmc_message* msg = nullptr;
    evmc::HostContext host;
    evmc_revision rev = {};
    bytes return_data;

    /// Reference to original EVM code.
    bytes_view original_code;

    evmc_status_code status = EVMC_SUCCESS;
    size_t output_offset = 0;
    size_t output_size = 0;

private:
    evmc_tx_context m_tx = {};

public:
    /// Pointer to code analysis.
    /// This should be set and used internally by execute() function of a particular interpreter.
    union
    {
        const baseline::CodeAnalysis* baseline = nullptr;
        const advanced::AdvancedCodeAnalysis* advanced;
    } analysis{};

    /// Stack space allocation.
    ///
    /// This is the last field to make other fields' offsets of reasonable values.
    StackSpace stack_space;

    ExecutionState() noexcept = default;

    ExecutionState(const evmc_message& message, evmc_revision revision,
        const evmc_host_interface& host_interface, evmc_host_context* host_ctx,
        bytes_view _code) noexcept
      : state_gas{.left = message.state_gas},
        msg{&message},
        host{host_interface, host_ctx},
        rev{revision},
        original_code{_code}
    {}

    /// Resets the contents of the ExecutionState so that it could be reused.
    void reset(const evmc_message& message, evmc_revision revision,
        const evmc_host_interface& host_interface, evmc_host_context* host_ctx,
        bytes_view _code) noexcept
    {
        gas_refund = 0;
        // EIP-8037: initialize the state-gas reservoir from the message budget.
        state_gas = {.left = message.state_gas};
        memory.clear();
        msg = &message;
        host = {host_interface, host_ctx};
        rev = revision;
        return_data.clear();
        original_code = _code;
        status = EVMC_SUCCESS;
        output_offset = 0;
        output_size = 0;
        m_tx = {};
    }

    [[nodiscard]] bool in_static_mode() const { return (msg->flags & EVMC_STATIC) != 0; }

    const evmc_tx_context& get_tx_context() noexcept
    {
        if (INTX_UNLIKELY(m_tx.block_timestamp == 0))
            m_tx = host.get_tx_context();
        return m_tx;
    }
};

/// Builds the execution result for a finished frame from its final @p state and @p gas_left.
///
/// Applies the frame-exit rules shared by the baseline and advanced interpreters: an exceptional
/// halt consumes all gas (only a success or revert keeps it), the gas refund counts only on
/// success, and the output is the memory range recorded in the state.
inline evmc_result make_execution_result(ExecutionState& state, int64_t gas_left) noexcept
{
    // EIP-8037: on revert or exceptional halt, roll back this frame's state gas (LIFO). The
    // spilled portion returns to `gas_left` (kept on revert; discarded by the halt's gas_left = 0
    // below, consuming it — matching EELS interpreter.py), and the reservoir is restored to the
    // frame's budget, leaving the net state gas used at zero.
    if (state.rev >= EVMC_AMSTERDAM && state.status != EVMC_SUCCESS)
    {
        gas_left += state.state_gas.spilled;
        state.state_gas.left = state.msg->state_gas;
        state.state_gas.spilled = 0;
    }

    // An exceptional halt consumes all gas; only a success or revert keeps gas_left.
    if (state.status != EVMC_SUCCESS && state.status != EVMC_REVERT)
        gas_left = 0;
    const auto gas_refund = (state.status == EVMC_SUCCESS) ? state.gas_refund : 0;

    assert(state.output_size != 0 || state.output_offset == 0);
    auto result = evmc::make_result(state.status, gas_left, gas_refund,
        state.output_size != 0 ? &state.memory[state.output_offset] : nullptr, state.output_size);

    // EIP-8037: return the leftover reservoir and spill; the net used is derived by the caller
    // as `initial - state_gas_left + state_gas_spilled`. The reservoir stays non-negative:
    // charge/refill preserve this from a non-negative message budget.
    assert(state.state_gas.left >= 0);
    result.state_gas_left = state.state_gas.left;
    result.state_gas_spilled = state.state_gas.spilled;
    return result;
}
}  // namespace evmone

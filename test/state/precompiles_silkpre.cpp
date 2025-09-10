// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2022 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "precompiles_silkpre.hpp"
#include "../utils/stdx/utility.hpp"
#include "precompiles.hpp"
#include <silkpre/precompile.h>
#include <cassert>
#include <cstring>

namespace evmone::state
{
namespace
{
ExecutionResult execute(const uint8_t* input, size_t input_size, uint8_t* output_buf,
    [[maybe_unused]] size_t max_output_size, PrecompileId id) noexcept
{
    const auto index = stdx::to_underlying(id);
    const auto [output, output_size] = kSilkpreContracts[index].run(input, input_size);
    if (output == nullptr)
        return {EVMC_PRECOMPILE_FAILURE, 0};

    // Check if max_output_size computed by analysis match the computed result.
    assert(output_size <= max_output_size);

    const auto trimmed_output_size = std::min(output_size, max_output_size);
    std::memcpy(output_buf, output, trimmed_output_size);
    std::free(output);  // Free output allocation (required by silkpre API).
    return {EVMC_SUCCESS, trimmed_output_size};
}
}  // namespace

ExecutionResult silkpre_ecrecover_execute(
    const uint8_t* input, size_t input_size, uint8_t* output_buf, size_t max_output_size) noexcept
{
    return execute(input, input_size, output_buf, max_output_size, PrecompileId::ecrecover);
}

ExecutionResult silkpre_sha256_execute(
    const uint8_t* input, size_t input_size, uint8_t* output_buf, size_t max_output_size) noexcept
{
    return execute(input, input_size, output_buf, max_output_size, PrecompileId::sha256);
}

ExecutionResult silkpre_ripemd160_execute(
    const uint8_t* input, size_t input_size, uint8_t* output_buf, size_t max_output_size) noexcept
{
    return execute(input, input_size, output_buf, max_output_size, PrecompileId::ripemd160);
}

ExecutionResult silkpre_expmod_execute(
    const uint8_t* input, size_t input_size, uint8_t* output_buf, size_t max_output_size) noexcept
{
    return execute(input, input_size, output_buf, max_output_size, PrecompileId::expmod);
}

ExecutionResult silkpre_ecadd_execute(
    const uint8_t* input, size_t input_size, uint8_t* output_buf, size_t max_output_size) noexcept
{
    return execute(input, input_size, output_buf, max_output_size, PrecompileId::ecadd);
}

ExecutionResult silkpre_ecmul_execute(
    const uint8_t* input, size_t input_size, uint8_t* output_buf, size_t max_output_size) noexcept
{
    return execute(input, input_size, output_buf, max_output_size, PrecompileId::ecmul);
}


ExecutionResult silkpre_ecpairing_execute(
    const uint8_t* input, size_t input_size, uint8_t* output_buf, size_t max_output_size) noexcept
{
    return execute(input, input_size, output_buf, max_output_size, PrecompileId::ecpairing);
}

ExecutionResult silkpre_blake2bf_execute(
    const uint8_t* input, size_t input_size, uint8_t* output_buf, size_t max_output_size) noexcept
{
    return execute(input, input_size, output_buf, max_output_size, PrecompileId::blake2bf);
}
}  // namespace evmone::state

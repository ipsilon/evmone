// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "precompiles_openssl.hpp"
#include <intx/intx.hpp>
#include <openssl/bn.h>
#include <memory>

namespace evmone::state
{
ExecutionResult openssl_expmod_execute(const uint8_t* input, size_t input_size, uint8_t* output_buf,
    [[maybe_unused]] size_t max_output_size) noexcept
{
    assert(input_size >= 3 * sizeof(intx::uint256));
    const auto base_len = static_cast<size_t>(intx::be::unsafe::load<intx::uint256>(input));
    const auto exp_len = static_cast<size_t>(intx::be::unsafe::load<intx::uint256>(input + 32));
    const auto mod_len = static_cast<size_t>(intx::be::unsafe::load<intx::uint256>(input + 64));
    assert(mod_len == max_output_size);

    // FIXME: Don't copy full input, just modulus should be enough.
    const auto input_padded = std::make_unique<uint8_t[]>(base_len + exp_len + mod_len);
    std::copy_n(input + 96, input_size - 96, input_padded.get());

    const auto base = BN_bin2bn(&input_padded[0], static_cast<int>(base_len), nullptr);
    const auto exp = BN_bin2bn(&input_padded[base_len], static_cast<int>(exp_len), nullptr);
    const auto mod =
        BN_bin2bn(&input_padded[base_len + exp_len], static_cast<int>(mod_len), nullptr);

    if (BN_is_zero(mod) == 0)
    {
        const auto ctx = BN_CTX_new();
        [[maybe_unused]] const auto status = BN_mod_exp(mod, base, exp, mod, ctx);
        assert(status == 1);
        BN_CTX_free(ctx);
    }

    BN_bn2binpad(mod, output_buf, static_cast<int>(mod_len));

    BN_free(base);
    BN_free(exp);
    BN_free(mod);
    return {EVMC_SUCCESS, mod_len};
}
}  // namespace evmone::state

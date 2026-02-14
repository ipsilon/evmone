// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "precompiles_libsecp256k1.hpp"
#include <secp256k1_recovery.h>
#include <cassert>

namespace evmone::state
{
bool ecrecover_libsecp256k1(std::span<uint8_t, 64> pubkey, std::span<const uint8_t, 32> hash,
    std::span<const uint8_t, 64> sig_bytes, bool parity) noexcept
{
    secp256k1_ecdsa_recoverable_signature sig;
    if (secp256k1_ecdsa_recoverable_signature_parse_compact(
            secp256k1_context_static, &sig, sig_bytes.data(), parity ? 1 : 0) != 1)
        return false;

    secp256k1_pubkey pk;
    if (secp256k1_ecdsa_recover(secp256k1_context_static, &pk, &sig, hash.data()) != 1)
        return false;

    uint8_t pubkey_prefixed[65];
    auto output_length = sizeof(pubkey_prefixed);
    [[maybe_unused]] const auto serialized_ok = secp256k1_ec_pubkey_serialize(
        secp256k1_context_static, pubkey_prefixed, &output_length, &pk, SECP256K1_EC_UNCOMPRESSED);
    assert(serialized_ok == 1);
    assert(output_length == sizeof(pubkey_prefixed));
    std::copy_n(&pubkey_prefixed[1], pubkey.size(), pubkey.data());
    return true;
}
}  // namespace evmone::state

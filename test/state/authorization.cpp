// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "authorization.hpp"
#include "hash_utils.hpp"
// TODO: Move the RLP encoder down into the state library, next to rlp_common.hpp. It lives in
//   evmone.testutils, which links against evmone::state, so this include points the wrong way.
//   It works only because the encoder is header-only.
#include "../utils/rlp.hpp"
#include <evmone_precompiles/secp256k1.hpp>

namespace evmone::state
{
namespace
{
/// Computes the hash the authorization tuple is signed over (EIP-7702):
/// keccak256(0x05 || rlp([chain_id, address, nonce])).
bytes32 compute_authorization_signing_hash(const Authorization& auth) noexcept
{
    static constexpr uint8_t MAGIC = 0x05;

    // TODO: The preimage is at most 66 bytes, so it can be encoded in a local buffer.
    //   Find helpers in compute_create_address() and process_authorization_list().
    return keccak256(bytes{MAGIC} + rlp::encode_tuple(auth.chain_id, auth.addr, auth.nonce));
}
}  // namespace

std::optional<address> recover_authority(const Authorization& auth) noexcept
{
    if (auth.y_parity > 1)
        return std::nullopt;

    const auto h = compute_authorization_signing_hash(auth);
    const auto r = intx::be::store<bytes32>(auth.r);
    const auto s = intx::be::store<bytes32>(auth.s);
    return crypto::secp256k1::ecrecover(
        h.bytes, r.bytes, s.bytes, auth.y_parity == 1, crypto::secp256k1::RecoveryMode::strict);
}
}  // namespace evmone::state

// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "create_address.hpp"
#include <evmone_precompiles/keccak.hpp>
#include <intx/intx.hpp>
#include <algorithm>
#include <bit>

namespace evmone
{
address compute_create_address(const address& sender, uint64_t sender_nonce) noexcept
{
    static constexpr auto RLP_STR_BASE = 0x80;
    static constexpr auto RLP_LIST_BASE = 0xc0;
    static constexpr auto ADDRESS_SIZE = sizeof(sender);
    static constexpr std::ptrdiff_t MAX_NONCE_SIZE = sizeof(sender_nonce);

    uint8_t buffer[ADDRESS_SIZE + MAX_NONCE_SIZE + 3];  // 3 for RLP prefix bytes.
    auto p = &buffer[1];                                // Skip RLP list prefix for now.
    *p++ = RLP_STR_BASE + ADDRESS_SIZE;                 // Set RLP string prefix for address.
    p = std::copy_n(sender.bytes, ADDRESS_SIZE, p);

    if (sender_nonce < RLP_STR_BASE)  // Short integer encoding including 0 as empty string (0x80).
    {
        *p++ = sender_nonce != 0 ? static_cast<uint8_t>(sender_nonce) : RLP_STR_BASE;
    }
    else  // Prefixed integer encoding.
    {
        const auto num_nonzero_bytes = (std::bit_width(sender_nonce) + 7) / 8;
        *p++ = static_cast<uint8_t>(RLP_STR_BASE + num_nonzero_bytes);
        intx::be::unsafe::store(p, sender_nonce);
        p = std::shift_left(p, p + MAX_NONCE_SIZE, MAX_NONCE_SIZE - num_nonzero_bytes);
    }

    const auto total_size = static_cast<size_t>(p - buffer);
    buffer[0] = static_cast<uint8_t>(RLP_LIST_BASE + (total_size - 1));  // Set the RLP list prefix.

    const auto base_hash = ethash::keccak256(buffer, total_size);
    address addr;
    std::copy_n(&base_hash.bytes[sizeof(base_hash) - ADDRESS_SIZE], ADDRESS_SIZE, addr.bytes);
    return addr;
}

address compute_create2_address(
    const address& sender, const bytes32& salt, bytes_view init_code) noexcept
{
    const auto init_code_hash = ethash::keccak256(init_code.data(), init_code.size());
    uint8_t buffer[1 + sizeof(sender) + sizeof(salt) + sizeof(init_code_hash)];
    static_assert(std::size(buffer) == 85);
    auto it = std::begin(buffer);
    *it++ = 0xff;
    it = std::copy_n(sender.bytes, sizeof(sender), it);
    it = std::copy_n(salt.bytes, sizeof(salt), it);
    std::copy_n(init_code_hash.bytes, sizeof(init_code_hash), it);
    const auto base_hash = ethash::keccak256(buffer, std::size(buffer));
    address addr;
    std::copy_n(&base_hash.bytes[sizeof(base_hash) - sizeof(addr)], sizeof(addr), addr.bytes);
    return addr;
}
}  // namespace evmone

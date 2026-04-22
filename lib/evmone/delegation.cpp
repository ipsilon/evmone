// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#include "delegation.hpp"
#include <cassert>

namespace evmone
{
std::optional<evmc::address> get_delegate_address(
    const evmc::HostInterface& host, const evmc::address& addr) noexcept
{
    // A valid EIP-7702 delegation designator is exactly 23 bytes. Reject longer
    // 0xef01...-prefixed code because is_code_delegated sees only copy_code's
    // truncated prefix and can't distinguish a 23-byte designator from a longer blob.
    if (host.get_code_size(addr) != DELEGATION_DESIGNATOR_SIZE)
        return {};

    uint8_t designation_buffer[DELEGATION_DESIGNATOR_SIZE];
    const auto size = host.copy_code(addr, 0, designation_buffer, std::size(designation_buffer));
    const bytes_view designation{designation_buffer, size};

    if (!is_code_delegated(designation))
        return {};

    evmc::address delegate_address;
    std::ranges::copy(designation.substr(std::size(DELEGATION_MAGIC)), delegate_address.bytes);
    return delegate_address;
}
}  // namespace evmone

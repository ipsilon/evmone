// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#include "delegation.hpp"

namespace evmone
{
std::optional<evmc::address> get_delegate_address(
    const evmc::HostInterface& host, const evmc::address& addr) noexcept
{
    // Load the code prefix one byte past the delegation designator size.
    // The HostInterface::copy_code() copies up to the addr's code size
    // and returns the number of bytes copied, so code longer than the designator
    // fills the extra byte and is rejected as ordinary code.
    uint8_t designation_buffer[DELEGATION_DESIGNATOR_SIZE + 1];
    const auto size = host.copy_code(addr, 0, designation_buffer, std::size(designation_buffer));
    const bytes_view designation{designation_buffer, size};

    if (!is_code_delegated(designation))
        return {};

    // Copy the delegate address from the designation buffer.
    evmc::address delegate_address;
    std::ranges::copy(designation.substr(std::size(DELEGATION_MAGIC)), delegate_address.bytes);
    return delegate_address;
}
}  // namespace evmone

// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#include "delegation.hpp"

namespace evmone
{
std::optional<evmc::address> get_delegate_address(
    const evmc::HostInterface& host, const evmc::address& addr) noexcept
{
    // Load the code prefix up to the delegation designation size plus one byte
    // to tell the designation apart from longer code with the same prefix.
    // The HostInterface::copy_code() copies up to the addr's code size
    // and returns the number of bytes copied.
    uint8_t code_prefix_buffer[DELEGATION_DESIGNATION_SIZE + 1];
    const auto size = host.copy_code(addr, 0, code_prefix_buffer, std::size(code_prefix_buffer));
    const bytes_view code_prefix{code_prefix_buffer, size};

    if (!is_code_delegated(code_prefix))
        return {};

    // Copy the delegate address from the designation.
    evmc::address delegate_address;
    std::ranges::copy(code_prefix.substr(std::size(DELEGATION_MAGIC)), delegate_address.bytes);
    return delegate_address;
}
}  // namespace evmone

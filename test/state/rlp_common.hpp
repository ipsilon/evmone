// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstddef>
#include <cstdint>

namespace evmone::rlp
{
/// RLP prefix constants (Yellow Paper Appendix B), shared by the encoder (test/utils/rlp.hpp)
/// and the decoder (rlp_decode). The long-form offset is the short-form offset plus the maximum
/// length still representable in the short form.
constexpr size_t MAX_SHORT_LEN = 55;
constexpr uint8_t SHORT_STRING_OFFSET = 0x80;                               // 128
constexpr uint8_t LONG_STRING_OFFSET = SHORT_STRING_OFFSET + MAX_SHORT_LEN;  // 0xb7 (183)
constexpr uint8_t SHORT_LIST_OFFSET = 0xc0;                                 // 192
constexpr uint8_t LONG_LIST_OFFSET = SHORT_LIST_OFFSET + MAX_SHORT_LEN;      // 0xf7 (247)
}  // namespace evmone::rlp

// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <string_view>
#include <system_error>

namespace evmone::test
{
/// Rewrites a fixture's `expectException` value to the execution-spec-tests names evmone reports,
/// so both test runners compare one vocabulary. Covers the retesteth vocabulary of ethereum/tests
/// (TR_NoFunds, InvalidGasLimit2, ...) and the few block-level spec names evmone does not tell
/// apart. Anything else is returned unchanged.
[[nodiscard]] std::string map_legacy_exception(std::string_view expected);

/// Whether any of the `|`-separated @p names is one of the `|`-separated @p expected, a fixture's
/// `expectException` value listing the exceptions it accepts. Names are compared whole: several
/// are a prefix of another (BlockException.UNKNOWN_PARENT and BlockException.UNKNOWN_PARENT_ZERO),
/// so a substring search would accept a rejection for a different rule.
///
/// TODO(C++23): both sides become std::views::split ranges. In C++20 that view is the lazy one:
///   it yields forward ranges, not the contiguous ones std::string_view can be built from.
[[nodiscard]] bool contains_any(std::string_view expected, std::string_view names) noexcept;

/// Whether the transaction validation error @p ec is one of the exceptions @p expected, the
/// fixture's `expectException` value. The canonical name is the error's own message; where the
/// specs name more exceptions for the same rule, those are accepted too.
[[nodiscard]] bool is_expected_tx_exception(
    const std::error_code& ec, std::string_view expected) noexcept;

/// The same for a block validation error.
[[nodiscard]] bool is_expected_block_exception(
    const std::error_code& ec, std::string_view expected) noexcept;
}  // namespace evmone::test

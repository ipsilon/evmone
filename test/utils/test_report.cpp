// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "test_report.hpp"
#include <ostream>

namespace evmone::test
{
std::ostream& operator<<(std::ostream& out, const Failure& failure)
{
    // A failure nests: the test, the place in it, the check, the values. The test name is a line
    // of its own because fixture names run to ~300 characters. The columns are fixed, so a check
    // with no place to report at leaves the second rung empty rather than shifting.
    out << failure.test << ":\n";
    if (!failure.where.empty())
        out << "  " << failure.where << ":\n";
    out << "    " << failure.what;

    if (failure.detail.empty())
        return out;

    out << ':';
    const std::string_view detail{failure.detail};
    for (size_t pos = 0;;)
    {
        const auto end = detail.find('\n', pos);
        const auto line = detail.substr(pos, end - pos);
        out << '\n';
        if (!line.empty())  // A blank line separating blocks stays blank, not six spaces.
            out << "      " << line;
        if (end == std::string_view::npos)
            return out;
        pos = end + 1;
    }
}
}  // namespace evmone::test

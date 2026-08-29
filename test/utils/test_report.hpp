// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "utils.hpp"
#include <evmc/bytes.hpp>
#include <intx/intx.hpp>
#include <cassert>
#include <concepts>
#include <functional>
#include <iosfwd>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace evmone::test
{
/// A test cannot be run at all. Skipped rather than failed.
struct UnsupportedTestFeature : std::runtime_error
{
    using runtime_error::runtime_error;
};

namespace detail
{
/// Writes @p value the way a report shows it: byte sequences as hex, error codes as their
/// message, everything else the way it streams itself.
template <typename T>
void stream(std::ostream& out, const T& value)
{
    // A byte buffer must arrive as a bytes_view: both bytes_view and the stream operators
    // take their length from a terminating zero, so a raw pointer or array either reports the
    // wrong length or reads past the end. Text is the one pointer worth streaming as it is.
    using Pointee = std::remove_cv_t<std::remove_pointer_t<std::decay_t<T>>>;
    static_assert(!std::is_pointer_v<std::decay_t<T>> || std::same_as<Pointee, char>,
        "pass bytes_view{...} rather than a raw pointer or array");

    if constexpr (std::convertible_to<const T&, bytes_view>)
        out << hex0x(bytes_view{value});
    else if constexpr (std::same_as<T, intx::uint256>)
        out << hex0x(value);
    else if constexpr (std::same_as<T, std::error_code>)
        out << value.message();  // Not the "<category>:<value>" it streams as.
    else if constexpr (std::integral<T> && sizeof(T) == 1 && !std::same_as<T, char>)
        out << +value;  // A byte is a number in a report, not a character.
    else
        out << value;
}
}  // namespace detail

/// Joins @p parts into a string the way a report shows them.
template <typename... Ts>
[[nodiscard]] std::string concat(const Ts&... parts)
{
    std::ostringstream out;
    (detail::stream(out, parts), ...);
    return std::move(out).str();
}

/// A check that did not hold.
struct Failure
{
    /// The test case it fired in: the fixture's top-level name.
    std::string test;
    /// Where in that case, as built by TestReport::at(), e.g. "Prague/0".
    std::string where;
    /// What was checked, named in the terms the fixture uses.
    std::string what;
    /// The values that differ and anything further worth saying.
    std::string detail;
};

/// Records what did not hold in one test case. Each failure goes to the sink as it happens.
/// Holds no global state, so reports are independent of each other.
class TestReport
{
public:
    /// Called with each failure as it is recorded, and the only place a failure goes.
    using Sink = std::function<void(const Failure&)>;

    explicit TestReport(Sink sink) : m_sink{std::move(sink)}
    {
        assert(m_sink && "a report without a sink discards every failure");
    }

    /// Restores the place its at() extended.
    class [[nodiscard]] Scope
    {
        TestReport& m_report;
        size_t m_restore_length;

        friend class TestReport;
        Scope(TestReport& report, size_t restore_length) noexcept
          : m_report{report}, m_restore_length{restore_length}
        {}

    public:
        Scope(Scope&&) = delete;
        ~Scope() { m_report.m_where.resize(m_restore_length); }
    };

    /// Names the test case the failures that follow belong to, until the next call.
    void start_case(std::string name) { m_test = std::move(name); }

    /// Extends the place within the test case with @p position, until the scope ends.
    /// Nested calls read as a path: at(1) inside at("Prague") is "Prague/1".
    [[nodiscard]] Scope at(const auto&... position)
    {
        const auto restore_length = m_where.size();
        // The suffix is built whole before m_where is touched, so a place that renders empty
        // adds no separator and a throw part-way through leaves nothing behind.
        auto suffix = concat(position...);
        if (!m_where.empty() && !suffix.empty())
            suffix.insert(suffix.begin(), '/');
        m_where += suffix;
        return Scope{*this, restore_length};  // A prvalue: Scope is not movable.
    }

    /// Records that @p what did not hold.
    void fail(std::string_view what, std::string detail = {})
    {
        m_sink(Failure{m_test, m_where, std::string{what}, std::move(detail)});
    }

    /// Records a failure with @p detail, unless @p ok holds. Returns @p ok. For a check that
    /// has nothing to put in two columns, such as a structure that did not decode.
    ///
    /// @p detail is an ordinary argument, so a caller whose detail costs anything to build
    /// needs the explicit `if` and fail() instead.
    bool check(bool ok, std::string_view what, std::string detail = {})
    {
        if (ok)
            return true;

        fail(what, std::move(detail));
        return false;
    }

    /// Records a failure showing what was produced against what the fixture expects, unless
    /// @p ok holds. Returns @p ok, so a caller can skip what only makes sense once it does not.
    ///
    /// @p more, if given, is invoked only when the check fails, for detail too expensive to
    /// format otherwise. It runs before the failure is recorded, so the failure is complete the
    /// first time anything sees it.
    bool check(bool ok, std::string_view what, const auto& actual, const auto& expected,
        const std::invocable auto&... more)
    {
        if (ok)
            return true;

        auto detail = concat("actual   ", actual, "\nexpected ", expected);
        (detail.append("\n").append(more()), ...);
        fail(what, std::move(detail));
        return false;
    }

    /// The same, for the common case of the two having to be equal.
    bool check_eq(std::string_view what, const auto& actual, const auto& expected,
        const std::invocable auto&... more)
    {
        return check(actual == expected, what, actual, expected, more...);
    }

private:
    Sink m_sink;
    std::string m_test;
    std::string m_where;
};

/// Writes @p failure the way pytest reports one: the test it fired in and where, then what was
/// checked and its detail. The C++ location of the check is deliberately absent — the name of the
/// check identifies it, and the fixture is what the reader is debugging.
std::ostream& operator<<(std::ostream& out, const Failure& failure);
}  // namespace evmone::test

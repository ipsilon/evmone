// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

/// Tests of the failure report the fixture runners write into: what a check records, and how a
/// recorded failure is rendered. The rendering is the runners' entire output, and a green fixture
/// run never reaches it.

#include <evmc/evmc.hpp>
#include <gtest/gtest.h>
#include <test/utils/test_report.hpp>
#include <sstream>
#include <system_error>
#include <vector>

using namespace evmone;
using namespace evmone::test;
using namespace evmc::literals;
using evmc::bytes;

namespace
{
/// A report whose sink keeps every failure, so a test can look at what was reported.
class Recorded
{
    std::vector<Failure> m_failures;

public:
    TestReport report{[this](const Failure& failure) { m_failures.push_back(failure); }};

    Recorded() = default;
    Recorded(Recorded&&) = delete;  // The sink holds `this`.

    [[nodiscard]] const std::vector<Failure>& failures() const noexcept { return m_failures; }

    /// The failures as the runners print them.
    [[nodiscard]] std::string render() const
    {
        std::ostringstream out;
        for (const auto& failure : m_failures)
            out << failure << '\n';
        return std::move(out).str();
    }
};
}  // namespace

TEST(test_report, no_failures)
{
    Recorded recorded;
    recorded.report.start_case("t");
    EXPECT_TRUE(recorded.report.check_eq("state root", 1, 1));
    EXPECT_TRUE(recorded.report.check(true, "rejection reason", "a", "b"));
    EXPECT_TRUE(recorded.failures().empty());
}

TEST(test_report, check_eq_reports_both_values)
{
    Recorded recorded;
    recorded.report.start_case("t");
    const auto in_case = recorded.report.at("Prague", '/', 0);

    EXPECT_FALSE(recorded.report.check_eq("state root", 0x01_bytes32, 0x02_bytes32));

    ASSERT_EQ(recorded.failures().size(), 1u);
    EXPECT_EQ(recorded.render(),
        "t:\n"
        "  Prague/0:\n"
        "    state root:\n"
        "      actual   0x0000000000000000000000000000000000000000000000000000000000000001\n"
        "      expected 0x0000000000000000000000000000000000000000000000000000000000000002\n");
}

TEST(test_report, check_reports_both_values_without_equality)
{
    // The rejection-reason checks compare through is_expected_tx_exception(), not ==.
    Recorded recorded;
    recorded.report.start_case("t");
    EXPECT_FALSE(recorded.report.check(false, "rejection reason", "GOT", "WANTED"));
    EXPECT_EQ(recorded.render(),
        "t:\n"
        "    rejection reason:\n"
        "      actual   GOT\n"
        "      expected WANTED\n");
}

TEST(test_report, what_stays_at_the_same_level_without_a_place)
{
    Recorded recorded;
    recorded.report.start_case("t");
    recorded.report.fail("block validity", "expected the block to be invalid");
    EXPECT_EQ(recorded.render(),
        "t:\n"
        "    block validity:\n"
        "      expected the block to be invalid\n");
}

TEST(test_report, at_scopes_nest_and_restore)
{
    Recorded recorded;
    recorded.report.start_case("t");
    {
        const auto outer = recorded.report.at("Prague");
        {
            const auto inner = recorded.report.at(3);
            recorded.report.fail("inner", "d");
        }
        recorded.report.fail("outer", "d");
    }
    recorded.report.fail("case", "d");

    ASSERT_EQ(recorded.failures().size(), 3u);
    EXPECT_EQ(recorded.failures()[0].where, "Prague/3");
    EXPECT_EQ(recorded.failures()[1].where, "Prague");
    EXPECT_EQ(recorded.failures()[2].where, "");
}

TEST(test_report, extra_detail_extends_the_failure)
{
    Recorded recorded;
    recorded.report.start_case("t");
    ASSERT_FALSE(recorded.report.check_eq(
        "post state root", 1, 2, [] { return std::string{"Result state:\nan account"}; }));

    EXPECT_EQ(recorded.render(),
        "t:\n"
        "    post state root:\n"
        "      actual   1\n"
        "      expected 2\n"
        "      Result state:\n"
        "      an account\n");
}

TEST(test_report, byte_sequences_render_as_hex)
{
    Recorded recorded;
    recorded.report.start_case("t");
    recorded.report.check_eq("bytes", bytes{0xb0, 0xb1}, bytes{0xb2});
    recorded.report.check_eq("address", 0xaa_address, 0xbb_address);

    ASSERT_EQ(recorded.failures().size(), 2u);
    EXPECT_EQ(recorded.failures()[0].detail, "actual   0xb0b1\nexpected 0xb2");
    EXPECT_EQ(recorded.failures()[1].detail,
        "actual   0x00000000000000000000000000000000000000aa\n"
        "expected 0x00000000000000000000000000000000000000bb");
}

TEST(test_report, concat_renders_mixed_values)
{
    EXPECT_EQ(concat("n=", 42, ' ', 0x01_bytes32),
        "n=42 0x0000000000000000000000000000000000000000000000000000000000000001");
    EXPECT_EQ(concat(intx::uint256{255}), "0xff");
}

TEST(test_report, error_codes_render_as_their_message)
{
    const auto ec = std::make_error_code(std::errc::invalid_argument);
    EXPECT_EQ(concat(ec), ec.message());
    EXPECT_NE(ec.message(), "generic:22");  // What streaming it directly would produce.
}

TEST(test_report, bytes_render_as_numbers_and_chars_as_text)
{
    EXPECT_EQ(concat(uint8_t{65}), "65");
    EXPECT_EQ(concat('/'), "/");
}

TEST(test_report, blank_detail_lines_carry_no_indent)
{
    Recorded recorded;
    recorded.report.start_case("t");
    ASSERT_FALSE(recorded.report.check_eq("post state root", 1, 2,
        [] { return std::string{"Result state:\naccount\n\nExpected state:\naccount\n"}; }));

    EXPECT_EQ(recorded.render(),
        "t:\n"
        "    post state root:\n"
        "      actual   1\n"
        "      expected 2\n"
        "      Result state:\n"
        "      account\n"
        "\n"
        "      Expected state:\n"
        "      account\n"
        "\n");
}

TEST(test_report, a_place_that_renders_empty_adds_no_separator)
{
    Recorded recorded;
    recorded.report.start_case("t");
    const auto outer = recorded.report.at("Prague");
    {
        const auto inner = recorded.report.at(std::string{});
        recorded.report.fail("what");
    }
    recorded.report.fail("after");

    ASSERT_EQ(recorded.failures().size(), 2u);
    EXPECT_EQ(recorded.failures()[0].where, "Prague");
    EXPECT_EQ(recorded.failures()[1].where, "Prague");
}

TEST(test_report, extra_detail_is_not_formatted_when_the_check_holds)
{
    Recorded recorded;
    recorded.report.start_case("t");
    int calls = 0;
    EXPECT_TRUE(recorded.report.check_eq("root", 1, 1, [&] {
        ++calls;
        return std::string{"expensive"};
    }));
    EXPECT_EQ(calls, 0);
}

TEST(test_report, the_sink_sees_each_failure_complete_as_it_is_recorded)
{
    Recorded recorded;
    recorded.report.start_case("t");

    recorded.report.check_eq("root", 1, 2, [] { return std::string{"dump"}; });
    // Delivered during the run rather than at the end of it, and already carrying its detail.
    ASSERT_EQ(recorded.failures().size(), 1u);
    EXPECT_EQ(recorded.failures()[0].detail, "actual   1\nexpected 2\ndump");

    recorded.report.fail("other");
    ASSERT_EQ(recorded.failures().size(), 2u);
    EXPECT_EQ(recorded.failures()[1].what, "other");
}

TEST(test_report, check_with_only_a_detail_reports_no_columns)
{
    // A structural check has nothing to put in an actual/expected pair.
    Recorded recorded;
    recorded.report.start_case("t");
    EXPECT_TRUE(recorded.report.check(true, "block RLP", "not a list"));
    EXPECT_FALSE(recorded.report.check(false, "block RLP", "not a list"));

    EXPECT_EQ(recorded.render(),
        "t:\n"
        "    block RLP:\n"
        "      not a list\n");
}

TEST(test_report, a_failure_without_detail_ends_after_what)
{
    Recorded recorded;
    recorded.report.start_case("t");
    recorded.report.fail("block RLP");

    EXPECT_EQ(recorded.render(),
        "t:\n"
        "    block RLP\n");
}

// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <test/utils/test_driver.hpp>

using namespace evmone::test;

namespace
{
/// The exit code and the report of one run.
struct Run
{
    int exit_code = -1;
    std::string output;
};

Run run(std::span<const TestCase> cases, const RunOptions& options = {})
{
    std::ostringstream out;
    const auto exit_code = run_tests(cases, out, options);
    return {exit_code, std::move(out).str()};
}

/// A test holding one case, which is what a fixture file with one fixture in it comes to.
TestCase one(std::string name, std::function<void(TestReport&)> run)
{
    return {name, [name, run = std::move(run)] { return std::vector{run_one(name, run)}; }};
}

/// A test holding cases which only report the outcome given, running nothing.
TestCase holding(std::string name, std::initializer_list<Outcome> outcomes)
{
    std::vector<Result> results;
    for (const auto outcome : outcomes)
        results.push_back({name + "::case", outcome, "the reason", {}});
    return {std::move(name), [results = std::move(results)] { return results; }};
}
}  // namespace

TEST(test_driver, nothing_collected)
{
    const auto [exit_code, output] = run({});
    EXPECT_EQ(NOTHING_VERIFIED, 5);  // pytest's value, not just whatever we declared.
    EXPECT_EQ(exit_code, NOTHING_VERIFIED);
    EXPECT_NE(output.find("collected 0 files"), std::string::npos);
}

TEST(test_driver, collect_only_lists_without_running)
{
    bool ran = false;
    const std::vector<TestCase> cases{one("a name", [&ran](TestReport&) { ran = true; })};

    const auto [exit_code, output] = run(cases, {.collect_only = true});
    EXPECT_FALSE(ran);
    EXPECT_EQ(exit_code, SUCCESS);
    EXPECT_EQ(output, "a name\n");
}

TEST(test_driver, collect_only_nothing_collected)
{
    const auto [exit_code, output] = run({}, {.collect_only = true});
    EXPECT_EQ(exit_code, NOTHING_VERIFIED);
    EXPECT_EQ(output, "");
}

TEST(test_driver, exception_fails_only_its_own_test)
{
    bool last_ran = false;
    const std::vector<TestCase> cases{
        one("ok", [](TestReport&) {}),
        one("throws", [](TestReport&) { throw std::runtime_error{"the reason"}; }),
        one("unknown", [](TestReport&) { throw 42; }),  // NOLINT(hicpp-exception-baseclass)
        one("last", [&last_ran](TestReport&) { last_ran = true; }),
    };

    const auto [exit_code, output] = run(cases);
    EXPECT_TRUE(last_ran);
    EXPECT_EQ(exit_code, 1);
    EXPECT_NE(output.find("2 failed, 2 passed"), std::string::npos);
    // The reason a test threw belongs in the summary, not only in the failure block.
    EXPECT_NE(output.find("FAILED  throws - exception: the reason"), std::string::npos);
}

TEST(test_driver, unsupported_feature_skips)
{
    const std::vector<TestCase> cases{
        one("ok", [](TestReport&) {}),
        one("skipped", [](TestReport&) { throw UnsupportedTestFeature{"no support for it"}; }),
    };

    const auto [exit_code, output] = run(cases);
    EXPECT_EQ(exit_code, 0);  // A skip does not fail the run.
    EXPECT_NE(output.find("1 passed, 1 skipped"), std::string::npos);
    EXPECT_NE(output.find("SKIPPED skipped - no support for it"), std::string::npos);
}

TEST(test_driver, everything_skipped_verifies_nothing)
{
    const std::vector<TestCase> cases{
        one("skipped", [](TestReport&) { throw UnsupportedTestFeature{"no support for it"}; })};

    const auto [exit_code, output] = run(cases);
    EXPECT_EQ(exit_code, NOTHING_VERIFIED);
    EXPECT_NE(output.find("0 passed, 1 skipped"), std::string::npos);
}

TEST(test_driver, summary_names_the_check_which_failed)
{
    const std::vector<TestCase> cases{
        one("mismatch", [](TestReport& report) { report.check_eq("a value", 1, 2); })};

    const auto [exit_code, output] = run(cases);
    EXPECT_EQ(exit_code, 1);
    EXPECT_NE(output.find("FAILED  mismatch - a value"), std::string::npos);
}

TEST(test_driver, failure_outranks_a_later_exception)
{
    const std::vector<TestCase> cases{one("both", [](TestReport& report) {
        report.check_eq("a value", 1, 2);
        throw std::runtime_error{"gave up afterwards"};
    })};

    const auto [exit_code, output] = run(cases);
    EXPECT_EQ(exit_code, TESTS_FAILED);
    // The summary names the check which failed, not the exception which ended the test.
    EXPECT_NE(output.find("FAILED  both - a value"), std::string::npos);
}

TEST(test_driver, failure_outranks_a_later_skip)
{
    const std::vector<TestCase> cases{one("both", [](TestReport& report) {
        report.check_eq("a value", 1, 2);
        throw UnsupportedTestFeature{"gave up afterwards"};
    })};

    const auto [exit_code, output] = run(cases);
    EXPECT_EQ(exit_code, 1);
    EXPECT_NE(output.find("1 failed"), std::string::npos);
    // The summary names the check which failed, not what the test then gave up on.
    EXPECT_NE(output.find("FAILED  both - a value"), std::string::npos);
}

TEST(test_driver, a_file_counts_once_however_many_fixtures_it_holds)
{
    const std::vector<TestCase> cases{
        holding("a file", {Outcome::passed, Outcome::passed, Outcome::passed})};

    const auto [exit_code, output] = run(cases);
    EXPECT_EQ(exit_code, SUCCESS);
    EXPECT_NE(output.find("collected 1 file"), std::string::npos);
    EXPECT_NE(output.find("1 passed"), std::string::npos);
}

TEST(test_driver, a_declined_fixture_is_named_though_its_file_passed)
{
    const std::vector<TestCase> cases{holding("a file", {Outcome::passed, Outcome::skipped})};

    const auto [exit_code, output] = run(cases);
    EXPECT_EQ(exit_code, SUCCESS);
    // The file's own verdict says nothing about what it declined, so the fixture is named.
    EXPECT_NE(output.find("1 passed in"), std::string::npos);
    EXPECT_NE(output.find("SKIPPED a file::case - the reason"), std::string::npos);
}

TEST(test_driver, one_failed_fixture_fails_its_file)
{
    const std::vector<TestCase> cases{
        holding("a file", {Outcome::passed, Outcome::failed, Outcome::skipped})};

    const auto [exit_code, output] = run(cases);
    EXPECT_EQ(exit_code, TESTS_FAILED);
    EXPECT_NE(output.find("1 failed, 0 passed"), std::string::npos);
}

TEST(test_driver, a_file_is_skipped_only_when_nothing_in_it_ran)
{
    const std::vector<TestCase> cases{holding("a file", {Outcome::skipped, Outcome::skipped})};

    const auto [exit_code, output] = run(cases);
    EXPECT_EQ(exit_code, NOTHING_VERIFIED);
    EXPECT_NE(output.find("0 passed, 1 skipped"), std::string::npos);
}

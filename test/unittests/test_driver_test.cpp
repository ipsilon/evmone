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
}  // namespace

TEST(test_driver, nothing_collected)
{
    // pytest's exit code for a run which selected nothing, which is rarely what was meant.
    const auto [exit_code, output] = run({});
    EXPECT_EQ(NO_TESTS_COLLECTED, 5);  // The value pytest uses, not just whatever we declared.
    EXPECT_EQ(exit_code, NO_TESTS_COLLECTED);
    EXPECT_NE(output.find("collected 0 tests"), std::string::npos);
}

TEST(test_driver, collect_only_lists_without_running)
{
    bool ran = false;
    const std::vector<TestCase> cases{{"a name", [&ran](TestReport&) { ran = true; }}};

    const auto [exit_code, output] = run(cases, {.collect_only = true});
    EXPECT_FALSE(ran);
    EXPECT_EQ(exit_code, SUCCESS);
    EXPECT_EQ(output, "a name\n");
}

TEST(test_driver, collect_only_nothing_collected)
{
    const auto [exit_code, output] = run({}, {.collect_only = true});
    EXPECT_EQ(exit_code, NO_TESTS_COLLECTED);
    EXPECT_EQ(output, "");
}

TEST(test_driver, exception_fails_only_its_own_test)
{
    bool last_ran = false;
    const std::vector<TestCase> cases{
        {"ok", [](TestReport&) {}},
        {"throws", [](TestReport&) { throw std::runtime_error{"the reason"}; }},
        {"unknown", [](TestReport&) { throw 42; }},  // NOLINT(hicpp-exception-baseclass)
        {"last", [&last_ran](TestReport&) { last_ran = true; }},
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
        {"ok", [](TestReport&) {}},
        {"skipped", [](TestReport&) { throw UnsupportedTestFeature{"no support for it"}; }},
    };

    const auto [exit_code, output] = run(cases);
    EXPECT_EQ(exit_code, 0);  // A skip does not fail the run.
    EXPECT_NE(output.find("1 passed, 1 skipped"), std::string::npos);
    EXPECT_NE(output.find("SKIPPED skipped - no support for it"), std::string::npos);
}

TEST(test_driver, summary_names_the_check_which_failed)
{
    const std::vector<TestCase> cases{
        {"mismatch", [](TestReport& report) { report.check_eq("a value", 1, 2); }}};

    const auto [exit_code, output] = run(cases);
    EXPECT_EQ(exit_code, 1);
    EXPECT_NE(output.find("FAILED  mismatch - a value"), std::string::npos);
}

TEST(test_driver, failure_outranks_a_later_exception)
{
    const std::vector<TestCase> cases{{"both", [](TestReport& report) {
                                           report.check_eq("a value", 1, 2);
                                           throw std::runtime_error{"gave up afterwards"};
                                       }}};

    const auto [exit_code, output] = run(cases);
    EXPECT_EQ(exit_code, TESTS_FAILED);
    // The summary names the check which failed, not the exception which ended the test.
    EXPECT_NE(output.find("FAILED  both - a value"), std::string::npos);
}

TEST(test_driver, failure_outranks_a_later_skip)
{
    const std::vector<TestCase> cases{{"both", [](TestReport& report) {
                                           report.check_eq("a value", 1, 2);
                                           throw UnsupportedTestFeature{"gave up afterwards"};
                                       }}};

    const auto [exit_code, output] = run(cases);
    EXPECT_EQ(exit_code, 1);
    EXPECT_NE(output.find("1 failed"), std::string::npos);
    // The summary names the check which failed, not what the test then gave up on.
    EXPECT_NE(output.find("FAILED  both - a value"), std::string::npos);
}

// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <evmc/evmc.hpp>
#include <nlohmann/json.hpp>
#include <test/utils/test_report.hpp>
#include <filesystem>
#include <optional>

namespace evmone::test
{
namespace json = nlohmann;

/// Nothing failed and something passed.
constexpr int SUCCESS = 0;

/// At least one test failed.
constexpr int TESTS_FAILED = 1;

/// No test passed, whether none was collected or every one was skipped. pytest returns this
/// value for the first of those; a test skipped has verified no more than a missing one.
constexpr int NOTHING_VERIFIED = 5;

/// A single test: its name and how to run it.
struct TestCase
{
    std::string name;

    /// Executes the test, recording what did not hold in the report.
    std::function<void(TestReport&)> run;
};

/// How to run and what to report.
struct RunOptions
{
    /// List the tests instead of running them.
    bool collect_only = false;

    /// Mark each test with a progress character rather than report its name. A progress line
    /// has no terminating newline, so anything a test prints itself would continue it.
    bool progress = true;
};

/// Runs @p cases, reports to @p out and returns the process exit code.
///
/// The failures are printed once the run ends, so a run killed part-way reports only how far it
/// got.
[[nodiscard]] int run_tests(
    std::span<const TestCase> cases, std::ostream& out, const RunOptions& options = {});

/// What the tests are run with.
struct TestSettings
{
    /// Run only the fixtures whose name contains this.
    std::optional<std::string> name_filter;

    /// Paths, relative to a test directory, not to collect tests from.
    std::vector<std::filesystem::path> ignored;

    /// Report each test's execution summary on the trace stream.
    bool trace_summary = false;

    /// Whether the name filter, if any, keeps the fixture called @p name.
    [[nodiscard]] bool selects(const std::string& name) const noexcept
    {
        return !name_filter.has_value() || name.find(*name_filter) != std::string::npos;
    }
};

/// Runs every selected fixture of one fixture file, which together are one test. Throws
/// UnsupportedTestFeature for a file this tool has nothing to run in.
void run_fixture_file(const std::filesystem::path& path, const TestSettings& settings, evmc::VM& vm,
    TestReport& report);

}  // namespace evmone::test

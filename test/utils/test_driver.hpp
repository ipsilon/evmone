// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <evmc/evmc.hpp>
#include <test/utils/test_report.hpp>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace evmone::test
{
/// Nothing failed and something passed.
constexpr int SUCCESS = 0;

/// At least one test failed.
constexpr int TESTS_FAILED = 1;

/// No test passed, whether none was collected or every one was skipped. pytest returns this
/// value for the first of those; a test skipped has verified no more than a missing one.
constexpr int NOTHING_VERIFIED = 5;

/// The verdict on a fixture or on a file, spelled as the file's progress character.
enum class Outcome : char
{
    passed = '.',
    failed = 'F',
    skipped = 's',
};

/// What running one fixture produced.
struct Result
{
    /// The fixture, as "<file>::<name>", or the file alone when it never got as far as one.
    std::string name;

    Outcome outcome = Outcome::passed;

    /// Why it did not pass. Empty when it did.
    std::string reason;

    std::vector<Failure> failures;
};

/// A single test: its name and how to run it.
struct TestCase
{
    std::string name;

    /// Executes the test, returning what each of its fixtures produced. A test which never got
    /// as far as a fixture returns the one result which says so, rather than throwing.
    std::function<std::vector<Result>()> run;
};

/// Runs @p run under a report of its own and says what it produced. What the run recorded
/// outranks how it ended: an exception is the reason only when nothing failed before it threw.
[[nodiscard]] Result run_one(std::string name, const std::function<void(TestReport&)>& run);

/// What to collect, what to run of it, and what to report.
struct RunOptions
{
    /// List the tests instead of running them.
    bool collect_only = false;

    /// Mark each test with a progress character. A progress line has no terminating newline,
    /// so anything a test prints itself would continue it.
    bool progress = true;

    /// Run only the fixtures whose name contains this.
    std::optional<std::string> name_filter;

    /// Paths, relative to a test directory, not to collect tests from.
    std::vector<std::filesystem::path> ignored;

    /// Report each state test's execution summary on the trace stream. Blockchain tests
    /// have no summary to report.
    bool trace_summary = false;

    /// Whether the name filter, if any, keeps the fixture called @p name.
    [[nodiscard]] bool selects(const std::string& name) const noexcept
    {
        return !name_filter.has_value() || name.find(*name_filter) != std::string::npos;
    }
};

/// Runs @p cases, reports to @p out and returns the process exit code.
///
/// The failures are printed once the run ends, so a run killed part-way reports only how far it
/// got.
[[nodiscard]] int run_tests(
    std::span<const TestCase> cases, std::ostream& out, const RunOptions& options = {});

/// Runs every selected fixture of one fixture file, which together are one test, and returns
/// what each produced. A file which holds no fixture, or does not parse, is the one result.
[[nodiscard]] std::vector<Result> run_fixture_file(
    const std::filesystem::path& path, const RunOptions& options, evmc::VM& vm);

}  // namespace evmone::test

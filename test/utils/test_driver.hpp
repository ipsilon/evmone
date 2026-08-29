// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <test/utils/test_report.hpp>

namespace evmone::test
{
/// The process exit codes, following pytest.
constexpr int SUCCESS = 0;
constexpr int TESTS_FAILED = 1;
constexpr int NO_TESTS_COLLECTED = 5;

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
}  // namespace evmone::test

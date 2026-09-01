// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <CLI/CLI.hpp>
#include <evmone/evmone.h>
#include <evmone/version.h>
#include <test/utils/blockchaintest.hpp>
#include <test/utils/test_driver.hpp>
#include <test/utils/test_files.hpp>
#include <iostream>

namespace fs = std::filesystem;
using evmone::test::TestCase;

namespace
{
/// Adds to @p cases every test under @p root: one per file for a directory, one per test case in
/// the file when the file itself is named. Returns whether every test was collected.
bool collect_tests(std::vector<TestCase>& cases, const fs::path& root,
    std::span<const fs::path> ignored, evmc::VM& vm)
{
    if (is_directory(root))
    {
        auto files = evmone::test::collect_test_files(root);
        evmone::test::ignore_test_files(files, ignored);
        cases.reserve(cases.size() + files.size());
        for (const auto& file : files)
        {
            // Loaded when the test runs: loading a whole tree up front costs far more. A
            // load which throws over an unsupported fixture reaches the driver, which skips.
            cases.push_back(
                {file.path.string(), [path = file.path, &vm](evmone::test::TestReport& report) {
                     std::ifstream f{path};
                     for (const auto& test : evmone::test::load_blockchain_tests(f))
                         evmone::test::run_blockchain_test(test, vm, report);
                 }});
        }
    }
    else  // Treat as a file.
    {
        // Naming a file loads it now, to name the test cases in it. One which cannot be
        // loaded becomes a single test the driver skips or fails.
        std::vector<evmone::test::BlockchainTest> tests;
        try
        {
            std::ifstream f{root};
            tests = evmone::test::load_blockchain_tests(f);
        }
        catch (const evmone::test::UnsupportedTestFeature&)
        {
            // An unsupported fixture is a skip, not a broken collection.
            cases.push_back({root.string(),
                [error = std::current_exception()](auto&) { std::rethrow_exception(error); }});
            return true;
        }
        catch (const std::exception& ex)
        {
            // Also reported here: --collect-only never runs the test.
            std::cerr << root.string() << ": " << ex.what() << '\n';
            cases.push_back({root.string(),
                [error = std::current_exception()](auto&) { std::rethrow_exception(error); }});
            return false;
        }

        for (const auto& test : tests)
        {
            cases.push_back(
                {root.string() + "::" + test.name, [test, &vm](evmone::test::TestReport& report) {
                     evmone::test::run_blockchain_test(test, vm, report);
                 }});
        }
    }
    return true;
}
}  // namespace


int main(int argc, char* argv[])
{
    try
    {
        CLI::App app{"evmone blockchain test runner"};

        app.set_version_flag("--version", "evmone-blockchaintest " EVMONE_VERSION);

        std::vector<std::string> paths;
        app.add_option("path", paths,
               "Path to test file or directory. For a directory, all .json "
               "files (except index.json) are considered test files, and each file is treated as a "
               "separate test. For a file, all tests in the file are treated as separate tests.")
            ->required()
            ->check(CLI::ExistingPath);

        std::vector<fs::path> ignored;
        app.add_option("--ignore", ignored,
               "Path, relative to a test directory, not to collect tests from. May be given more "
               "than once. Whole path components are matched, so --ignore bc4895 keeps "
               "bc4895-withdrawals.")
            // Without this the option is variadic and swallows the positional paths after it.
            ->allow_extra_args(false);

        bool collect_only = false;
        app.add_flag("--collect-only", collect_only,
            "List the path of each collected test, one per line, and exit.");

        bool trace_flag = false;
        app.add_flag("--trace", trace_flag, "Enable EVM tracing");

        CLI11_PARSE(app, argc, argv);

        evmc::VM vm{evmc_create_evmone()};

        if (trace_flag)
            vm.set_option("trace", "1");

        std::vector<TestCase> cases;
        bool all_collected = true;
        for (const auto& p : paths)
            all_collected &= collect_tests(cases, p, ignored, vm);

        const evmone::test::RunOptions options{
            .collect_only = collect_only, .progress = !trace_flag};
        const auto exit_code = evmone::test::run_tests(cases, std::cout, options);
        // A file which could not be loaded fails the listing too, not only a run of it.
        return all_collected ? exit_code : evmone::test::TESTS_FAILED;
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << "\n";
        return -1;
    }
}

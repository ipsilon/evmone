// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2022 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <CLI/CLI.hpp>
#include <evmone/evmone.h>
#include <evmone/version.h>
#include <test/utils/statetest.hpp>
#include <test/utils/test_collector.hpp>
#include <test/utils/test_driver.hpp>
#include <iostream>

namespace fs = std::filesystem;
using evmone::test::TestCase;

namespace
{
/// A test which is one case: the driver takes an array of results, and this is the one.
TestCase one_case(std::string name, std::function<void(evmone::test::TestReport&)> run)
{
    return {name,
        [name, run = std::move(run)] { return std::vector{evmone::test::run_one(name, run)}; }};
}

/// Adds to @p cases one test per fixture file under @p root, which is that file itself when it
/// is not a directory.
void collect_tests(std::vector<TestCase>& cases, const fs::path& root,
    const std::optional<std::string>& filter, std::span<const fs::path> ignored, evmc::VM& vm,
    bool trace)
{
    // Which cases -k keeps. It selects within the file's test, because naming the cases up front
    // would mean loading the whole tree.
    const auto selected = [&filter](const evmone::test::StateTransitionTest& test) {
        return !filter.has_value() || test.name.find(*filter) != std::string::npos;
    };

    // A file is one test, whether it was named or found under a directory.
    std::vector<evmone::test::TestFile> files;
    if (is_directory(root))
    {
        files = evmone::test::collect_test_files(root);
        evmone::test::ignore_test_files(files, ignored);
    }
    else
        files.push_back({root, {}});

    cases.reserve(cases.size() + files.size());
    for (const auto& file : files)
    {
        // Loaded when the test runs: loading a whole tree up front costs far more.
        cases.push_back(one_case(file.path.string(),
            [path = file.path, selected, &vm, trace](evmone::test::TestReport& report) {
            std::ifstream f{path};
            for (const auto& test : evmone::test::load_state_tests(f))
            {
                if (selected(test))
                    evmone::test::run_state_test(test, vm, trace, report);
            }
            }
    }));
}
}
}  // namespace


int main(int argc, char* argv[])
{
    try
    {
        CLI::App app{"evmone state test runner"};

        app.set_version_flag("--version", "evmone-statetest " EVMONE_VERSION);

        std::vector<std::string> paths;
        app.add_option("path", paths,
               "Path to test file or directory. For a directory, all .json "
               "files (except index.json) are considered test files, and each file is treated as a "
               "separate test. For a file, all tests in the file are treated as separate tests.")
            ->required()
            ->check(CLI::ExistingPath);

        std::optional<std::string> filter;
        app.add_option("-k", filter,
            "Test name filter. Run only tests with names containing the specified string.");

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

        bool trace = false;
        bool trace_summary = false;
        const auto trace_opt = app.add_flag("--trace", trace, "Enable EVM tracing");
        app.add_flag("--trace-summary", trace_summary, "Output trace summary only")
            ->excludes(trace_opt);

        CLI11_PARSE(app, argc, argv);

        evmc::VM vm{evmc_create_evmone(), {{"O", "0"}}};

        if (trace)
        {
            std::ios::sync_with_stdio(false);
            vm.set_option("trace", "1");
        }

        std::vector<TestCase> cases;
        for (const auto& p : paths)
            collect_tests(cases, p, filter, ignored, vm, trace || trace_summary);

        const evmone::test::RunOptions options{
            .collect_only = collect_only, .progress = !(trace || trace_summary)};
        return evmone::test::run_tests(cases, std::cout, options);
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << "\n";
        return -1;
    }
}

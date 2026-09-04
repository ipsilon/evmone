// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <CLI/CLI.hpp>
#include <evmone/evmone.h>
#include <evmone/version.h>
#include <test/utils/blockchaintest.hpp>
#include <test/utils/test_collector.hpp>
#include <test/utils/test_driver.hpp>
#include <iostream>

namespace fs = std::filesystem;
using evmone::test::TestCase;

namespace
{
/// Adds to @p cases one test per fixture file under @p root, which is that file itself when it
/// is not a directory.
void collect_tests(std::vector<TestCase>& cases, const fs::path& root,
    std::span<const fs::path> ignored, evmc::VM& vm)
{
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
        for (const auto& p : paths)
            collect_tests(cases, p, ignored, vm);

        const evmone::test::RunOptions options{
            .collect_only = collect_only, .progress = !trace_flag};
        return evmone::test::run_tests(cases, std::cout, options);
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << "\n";
        return -1;
    }
}

// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <CLI/CLI.hpp>
#include <evmone/evmone.h>
#include <evmone/version.h>
#include <gtest/gtest.h>
#include <test/utils/blockchaintest.hpp>
#include <test/utils/test_files.hpp>
#include <test/utils/test_report.hpp>
#include <iostream>

namespace fs = std::filesystem;

namespace
{
/// Reports each failure to gtest the moment the runner records it, so a run that dies part-way
/// still shows what it found.
///
/// TODO: Bridge for as long as gtest drives these tests. Once the test driver replaces it the
///   report is the verdict directly and this goes away.
evmone::test::TestReport make_report()
{
    return evmone::test::TestReport{
        [](const evmone::test::Failure& failure) { ADD_FAILURE() << failure; }};
}

/// Implementation of a gtest Test which runs all blockchain tests from a given file.
class BlockchainGTestFile : public testing::Test
{
    fs::path m_json_test_file;
    evmc::VM& m_vm;

public:
    explicit BlockchainGTestFile(fs::path json_test_file, evmc::VM& vm) noexcept
      : m_json_test_file{std::move(json_test_file)}, m_vm{vm}
    {}

    void TestBody() final
    {
        auto report = make_report();
        std::ifstream f{m_json_test_file};

        try
        {
            evmone::test::run_blockchain_tests(
                evmone::test::load_blockchain_tests(f), m_vm, report);
        }
        catch (const evmone::test::UnsupportedTestFeature& ex)
        {
            GTEST_SKIP() << ex.what();
        }
    }

    static void register_one(const std::string& suite_name, const fs::path& file, evmc::VM& vm)
    {
        testing::RegisterTest(suite_name.c_str(), file.stem().string().c_str(), nullptr, nullptr,
            file.string().c_str(), 0,
            [file, &vm]() -> testing::Test* { return new BlockchainGTestFile(file, vm); });
    }
};

/// Implementation of a gtest Test which runs a single blockchain test.
class BlockchainGTest : public testing::Test
{
    const evmone::test::BlockchainTest m_blockchain_test;
    evmc::VM& m_vm;

public:
    explicit BlockchainGTest(evmone::test::BlockchainTest blockchain_test, evmc::VM& vm) noexcept
      : m_blockchain_test{std::move(blockchain_test)}, m_vm{vm}
    {}

    void TestBody() final
    {
        auto report = make_report();
        evmone::test::run_blockchain_tests(std::array{m_blockchain_test}, m_vm, report);
    }

    static void register_one(const evmone::test::BlockchainTest& test,
        const std::string& suite_name, const std::string& test_name, const fs::path& file,
        evmc::VM& vm)
    {
        testing::RegisterTest(suite_name.c_str(), test_name.c_str(), nullptr, nullptr,
            file.string().c_str(), 0,
            [test, &vm]() -> testing::Test* { return new BlockchainGTest(test, vm); });
    }
};

/// Registers every test under @p root, or prints its path if @p collect_only.
void register_test_files(
    const fs::path& root, std::span<const fs::path> ignored, bool collect_only, evmc::VM& vm)
{
    if (is_directory(root))
    {
        auto files = evmone::test::collect_test_files(root);
        evmone::test::ignore_test_files(files, ignored);
        for (const auto& [path, suite_name] : files)
        {
            if (collect_only)
                std::cout << path.string() << '\n';
            else
                BlockchainGTestFile::register_one(suite_name, path, vm);
        }
    }
    else  // Treat as a file.
    {
        std::ifstream f{root};
        try
        {
            const auto tests = evmone::test::load_blockchain_tests(f);
            for (const auto& test : tests)
            {
                if (collect_only)
                    std::cout << root.string() << "::" << test.name << '\n';
                else
                    BlockchainGTest::register_one(test, root.string(), test.name, root, vm);
            }
        }
        catch (const evmone::test::UnsupportedTestFeature& ex)
        {
            std::cerr << ex.what() << ": " << root.string() << '\n';
        }
    }
}
}  // namespace


int main(int argc, char* argv[])
{
    try
    {
        testing::InitGoogleTest(&argc, argv);  // Process GoogleTest flags.

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

        for (const auto& p : paths)
            register_test_files(p, ignored, collect_only, vm);

        return collect_only ? 0 : RUN_ALL_TESTS();
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << "\n";
        return -1;
    }
}

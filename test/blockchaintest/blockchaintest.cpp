// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "blockchaintest.hpp"
#include <CLI/CLI.hpp>
#include <evmone/evmone.h>
#include <evmone/version.h>
#include <gtest/gtest.h>
#include <iostream>

namespace fs = std::filesystem;

namespace
{
class BlockchainGTest : public testing::Test
{
    fs::path m_json_test_file;
    evmc::VM& m_vm;
    bool m_time;

public:
    explicit BlockchainGTest(fs::path json_test_file, evmc::VM& vm, bool time) noexcept
      : m_json_test_file{std::move(json_test_file)}, m_vm{vm}, m_time{time}
    {}

    void TestBody() final
    {
        std::ifstream f{m_json_test_file};

        try
        {
            evmone::test::run_blockchain_tests(
                evmone::test::load_blockchain_tests(f), m_vm, m_time);
        }
        catch (const evmone::test::UnsupportedTestFeature& ex)
        {
            GTEST_SKIP() << ex.what();
        }
    }
};

void register_test(const std::string& suite_name, const fs::path& file, evmc::VM& vm, bool time)
{
    testing::RegisterTest(suite_name.c_str(), file.stem().string().c_str(), nullptr, nullptr,
        file.string().c_str(), 0,
        [file, &vm, time]() -> testing::Test* { return new BlockchainGTest(file, vm, time); });
}

void register_test_files(const fs::path& root, evmc::VM& vm, bool time)
{
    if (is_directory(root))
    {
        std::vector<fs::path> test_files;
        std::copy_if(fs::recursive_directory_iterator{root}, fs::recursive_directory_iterator{},
            std::back_inserter(test_files), [](const fs::directory_entry& entry) {
                return entry.is_regular_file() && entry.path().extension() == ".json";
            });
        std::ranges::sort(test_files);

        for (const auto& p : test_files)
            register_test(fs::relative(p, root).parent_path().string(), p, vm, time);
    }
    else  // Treat as a file.
    {
        register_test(root.parent_path().string(), root, vm, time);
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
        app.add_option("path", paths, "Path to test file or directory")
            ->required()
            ->check(CLI::ExistingPath);

        bool trace_flag = false;
        app.add_flag("--trace", trace_flag, "Enable EVM tracing");

        bool time_flag = false;
        app.add_flag("--time", time_flag, "Measure last block in test execution");

        CLI11_PARSE(app, argc, argv);

        evmc::VM vm{evmc_create_evmone()};

        if (trace_flag)
            vm.set_option("trace", "1");

        for (const auto& p : paths)
            register_test_files(p, vm, time_flag);

        return RUN_ALL_TESTS();
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << "\n";
        return -1;
    }
}

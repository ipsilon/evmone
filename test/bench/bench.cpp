// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "helpers.hpp"
#include "synthetic_benchmarks.hpp"
#include <benchmark/benchmark.h>
#include <evmc/evmc.hpp>
#include <evmone/evmone.h>
#include <gtest/gtest.h>
#include <test/statetest/statetest.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>

namespace fs = std::filesystem;

using namespace benchmark;

namespace evmone::test
{
std::map<std::string_view, evmc::VM> registered_vms;

namespace
{
struct BenchmarkCase
{
    StateTransitionTest state_test;
    std::string name;
};

/// Loads a benchmark case from a file at `path` and all its inputs from the matching inputs file.
std::vector<BenchmarkCase> load_benchmark(const fs::path& path, const std::string& name_prefix)
{
    std::ifstream f{path};
    std::vector<BenchmarkCase> result;

    auto state_tests = load_state_tests(f);
    result.reserve(state_tests.size());
    for (const auto& state_test : state_tests)
    {
        // If a file has a single test, skip its name.
        auto name = name_prefix + path.stem().string() +
                    (state_tests.size() > 1 ? "/" + state_test.name : "");
        result.emplace_back(BenchmarkCase{state_test, std::move(name)});
    }

    return result;
}

/// Loads all benchmark cases from the given directory and all its subdirectories.
std::vector<BenchmarkCase> load_benchmarks_from_dir(  // NOLINT(misc-no-recursion)
    const fs::path& path, const std::string& name_prefix = {})
{
    std::vector<fs::path> subdirs;
    std::vector<fs::path> files;

    for (auto& e : fs::directory_iterator{path})
    {
        if (e.is_directory())
            subdirs.emplace_back(e);
        else if (e.path().extension() == ".json")
            files.emplace_back(e);
    }

    std::ranges::sort(subdirs);
    std::ranges::sort(files);

    std::vector<BenchmarkCase> benchmark_cases;

    for (const auto& f : files)
    {
        auto t = load_benchmark(f, name_prefix);
        benchmark_cases.insert(benchmark_cases.end(), std::make_move_iterator(t.begin()),
            std::make_move_iterator(t.end()));
    }

    for (const auto& d : subdirs)
    {
        auto t = load_benchmarks_from_dir(d, name_prefix + d.filename().string() + '/');
        benchmark_cases.insert(benchmark_cases.end(), std::make_move_iterator(t.begin()),
            std::make_move_iterator(t.end()));
    }

    return benchmark_cases;
}

void register_benchmarks(std::span<const BenchmarkCase> benchmark_cases)
{
    evmc::VM* advanced_vm = nullptr;
    evmc::VM* baseline_vm = nullptr;
    if (const auto it = registered_vms.find("advanced"); it != registered_vms.end())
        advanced_vm = &it->second;
    if (const auto it = registered_vms.find("baseline"); it != registered_vms.end())
        baseline_vm = &it->second;

    evmc::VM check_test_vm = evmc::VM{evmc_create_evmone()};

    for (const auto& b : benchmark_cases)
    {
        run_state_test(b.state_test, check_test_vm, false);

        if (::testing::Test::HasFailure())
            throw std::invalid_argument{"State test you want to bench failed."};

        const auto code = b.state_test.pre_state.get_account_code(b.state_test.multi_tx.to.value());
        for (const auto& [rev, cases, block_info] : b.state_test.cases)
        {
            if (advanced_vm != nullptr)
            {
                RegisterBenchmark("advanced/analyse/" + b.name, [code, &rev](State& state) {
                    bench_analyse<advanced::AdvancedCodeAnalysis, advanced_analyse>(
                        state, rev, code);
                })->Unit(kMicrosecond);
            }

            if (baseline_vm != nullptr)
            {
                RegisterBenchmark("baseline/analyse/" + b.name, [code, &rev](State& state) {
                    bench_analyse<baseline::CodeAnalysis, baseline_analyse>(state, rev, code);
                })->Unit(kMicrosecond);
            }

            for (size_t case_index = 0; case_index != cases.size(); ++case_index)
            {
                const auto& expected = cases[case_index];
                const auto tx = b.state_test.multi_tx.get(expected.indexes);

                std::string case_name;
                if (const auto it = b.state_test.input_labels.find(case_index);
                    it != b.state_test.input_labels.end())
                {
                    case_name = it->second;
                }
                else
                    case_name = std::to_string(case_index);


                for (auto& [vm_name, vm] : registered_vms)
                {
                    const auto name = std::string{vm_name} + "/execute/" + b.name + '/' + case_name;

                    const auto tx_props_or_error = state::validate_transaction(
                        b.state_test.pre_state, block_info, tx, rev, block_info.gas_limit,
                        static_cast<int64_t>(state::max_blob_gas_per_block(rev)));
                    if (const auto err = get_if<std::error_code>(&tx_props_or_error))
                    {
                        throw std::invalid_argument{
                            "Transaction validation failure: " + err->message()};
                    }

                    const auto tx_props = get<state::TransactionProperties>(tx_props_or_error);

                    // `tx` and `tx_props` are temporary.
                    RegisterBenchmark(name, [tx, tx_props, &vm, &b, &block_info, &rev](
                                                State& state) {
                        bench_transition(state, vm, tx, tx_props, b.state_test.pre_state,
                            block_info, b.state_test.block_hashes, rev);
                    })->Unit(kMicrosecond);
                }
            }
        }
    }
}


/// The error code for CLI arguments parsing error in evmone-bench.
/// The number tries to be different from EVMC loading error codes.
constexpr auto cli_parsing_error = -3;

/// Parses evmone-bench CLI arguments and registers benchmark cases.
///
/// The following variants of number arguments are supported (including argv[0]):
///
/// 1: evmone-bench
///    Uses evmone VMs, only synthetic benchmarks are available.
/// 2: evmone-bench benchmarks_path
///    Uses evmone VMs, loads all benchmarks from benchmarks_path. If benchmarks_path is a `json`
///    file, single test is run.
std::variant<int, std::vector<BenchmarkCase>> parseargs(int argc, char** argv)
{
    // Argument's placeholder:
    std::string benchmarks_path;

    switch (argc)
    {
    case 1:
        // Run with built-in synthetic benchmarks only.
        break;
    case 2:
        benchmarks_path = argv[1];
        break;
    default:
        return cli_parsing_error;
    }

    if (!benchmarks_path.empty())
    {
        if (fs::is_directory(benchmarks_path))
            return load_benchmarks_from_dir(benchmarks_path);
        else
            return load_benchmark(benchmarks_path, {});
    }

    return std::vector<BenchmarkCase>{};
}
}  // namespace
}  // namespace evmone::test

int main(int argc, char** argv)
{
    MaybeReenterWithoutASLR(argc, argv);

    using namespace evmone::test;
    try
    {
        Initialize(&argc, argv);  // Consumes --benchmark_ options.
        const auto ec_or_benchmark_cases = parseargs(argc, argv);
        if (std::holds_alternative<int>(ec_or_benchmark_cases))
        {
            const auto ec = std::get<int>(ec_or_benchmark_cases);
            if (ec == cli_parsing_error && ReportUnrecognizedArguments(argc, argv))
                return ec;

            if (ec != 0)
                return ec;
        }
        else
        {
            const auto benchmark_cases =
                std::get<std::vector<BenchmarkCase>>(ec_or_benchmark_cases);
            registered_vms["advanced"] = evmc::VM{evmc_create_evmone(), {{"advanced", ""}}};
            registered_vms["baseline"] = evmc::VM{evmc_create_evmone()};
            registered_vms["bnocgoto"] = evmc::VM{evmc_create_evmone(), {{"cgoto", "no"}}};
            register_benchmarks(benchmark_cases);
            register_synthetic_benchmarks();
            RunSpecifiedBenchmarks();
            return 0;
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << "\n";
        return -1;
    }
}

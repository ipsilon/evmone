// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "../statetest/statetest.hpp"
#include "helpers.hpp"
#include "synthetic_benchmarks.hpp"
#include <benchmark/benchmark.h>
#include <evmc/evmc.hpp>
#include <evmc/loader.h>
#include <evmone/evmone.h>
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
    struct Input
    {
        std::string name;
        bytes input;
        bytes expected_output;

        Input(std::string _name, bytes _input, bytes _expected_output = {}) noexcept
          : name{std::move(_name)},
            input{std::move(_input)},
            expected_output{std::move(_expected_output)}
        {}
    };

    std::string name;
    bytes code;
    std::vector<Input> inputs;
};

/// Loads the benchmark case's inputs from the inputs file at the given path.
std::vector<BenchmarkCase::Input> load_inputs(const StateTransitionTest& state_test)
{
    std::vector<BenchmarkCase::Input> inputs;
    inputs.reserve(state_test.multi_tx.inputs.size());
    for (size_t i = 0; i < state_test.multi_tx.inputs.size(); ++i)
        inputs.emplace_back(state_test.input_labels.at(i), state_test.multi_tx.inputs[i]);
    return inputs;
}

/// Loads a benchmark case from a file at `path` and all its inputs from the matching inputs file.
BenchmarkCase load_benchmark(const fs::path& path, const std::string& name_prefix)
{
    std::ifstream f{path};
    auto state_test = std::move(load_state_tests(f).at(0));

    const auto name = name_prefix + path.stem().string();
    const auto code = state_test.pre_state[state_test.multi_tx.to.value()].code;
    const auto inputs = load_inputs(state_test);

    return BenchmarkCase{name, code, inputs};
}

/// Loads all benchmark cases from the given directory and all its subdirectories.
std::vector<BenchmarkCase> load_benchmarks_from_dir(  // NOLINT(misc-no-recursion)
    const fs::path& path, const std::string& name_prefix = {})
{
    std::vector<fs::path> subdirs;
    std::vector<fs::path> code_files;

    for (auto& e : fs::directory_iterator{path})
    {
        if (e.is_directory())
            subdirs.emplace_back(e);
        else if (e.path().extension() == ".json")
            code_files.emplace_back(e);
    }

    std::ranges::sort(subdirs);
    std::ranges::sort(code_files);

    std::vector<BenchmarkCase> benchmark_cases;

    benchmark_cases.reserve(std::size(code_files));
    for (const auto& f : code_files)
        benchmark_cases.emplace_back(load_benchmark(f, name_prefix));

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
    evmc::VM* basel_cg_vm = nullptr;
    if (const auto it = registered_vms.find("advanced"); it != registered_vms.end())
        advanced_vm = &it->second;
    if (const auto it = registered_vms.find("baseline"); it != registered_vms.end())
        baseline_vm = &it->second;
    if (const auto it = registered_vms.find("bnocgoto"); it != registered_vms.end())
        basel_cg_vm = &it->second;

    for (const auto& b : benchmark_cases)
    {
        if (advanced_vm != nullptr)
        {
            RegisterBenchmark("advanced/analyse/" + b.name, [&b](State& state) {
                bench_analyse<advanced::AdvancedCodeAnalysis, advanced_analyse>(
                    state, default_revision, b.code);
            })->Unit(kMicrosecond);
        }

        if (baseline_vm != nullptr)
        {
            RegisterBenchmark("baseline/analyse/" + b.name, [&b](State& state) {
                bench_analyse<baseline::CodeAnalysis, baseline_analyse>(
                    state, default_revision, b.code);
            })->Unit(kMicrosecond);
        }

        for (const auto& input : b.inputs)
        {
            const auto case_name = b.name + (!input.name.empty() ? '/' + input.name : "");

            if (advanced_vm != nullptr)
            {
                const auto name = "advanced/execute/" + case_name;
                RegisterBenchmark(name, [&vm = *advanced_vm, &b, &input](State& state) {
                    bench_advanced_execute(state, vm, b.code, input.input, input.expected_output);
                })->Unit(kMicrosecond);
            }

            if (baseline_vm != nullptr)
            {
                const auto name = "baseline/execute/" + case_name;
                RegisterBenchmark(name, [&vm = *baseline_vm, &b, &input](State& state) {
                    bench_baseline_execute(state, vm, b.code, input.input, input.expected_output);
                })->Unit(kMicrosecond);
            }

            if (basel_cg_vm != nullptr)
            {
                const auto name = "bnocgoto/execute/" + case_name;
                RegisterBenchmark(name, [&vm = *basel_cg_vm, &b, &input](State& state) {
                    bench_baseline_execute(state, vm, b.code, input.input, input.expected_output);
                })->Unit(kMicrosecond);
            }

            for (auto& [vm_name, vm] : registered_vms)
            {
                const auto name = std::string{vm_name} + "/total/" + case_name;
                RegisterBenchmark(name, [&vm, &b, &input](State& state) {
                    bench_evmc_execute(state, vm, b.code, input.input, input.expected_output);
                })->Unit(kMicrosecond);
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
/// 2: evmone-bench benchmarks_dir
///    Uses evmone VMs, loads all benchmarks from benchmarks_dir.
/// 3: evmone-bench evmc_config benchmarks_dir
///    The same as (2) but loads additional custom EVMC VM.
/// 4: evmone-bench code_hex_file input_hex expected_output_hex.
///    Uses evmone VMs, registers custom benchmark with the code from the given file,
///    and the given input. The benchmark will compare the output with the provided
///    expected one.
std::tuple<int, std::vector<BenchmarkCase>> parseargs(int argc, char** argv)
{
    // Arguments' placeholders:
    std::string evmc_config;
    std::string benchmarks_dir;
    std::string code_hex_file;
    std::string input_hex;
    std::string expected_output_hex;

    switch (argc)
    {
    case 1:
        // Run with built-in synthetic benchmarks only.
        break;
    case 2:
        benchmarks_dir = argv[1];
        break;
    case 3:
        evmc_config = argv[1];
        benchmarks_dir = argv[2];
        break;
    case 4:
        code_hex_file = argv[1];
        input_hex = argv[2];
        expected_output_hex = argv[3];
        break;
    default:
        std::cerr << "Too many arguments\n";
        return {cli_parsing_error, {}};
    }

    if (!evmc_config.empty())
    {
        auto ec = evmc_loader_error_code{};
        registered_vms["external"] = evmc::VM{evmc_load_and_configure(evmc_config.c_str(), &ec)};

        if (ec != EVMC_LOADER_SUCCESS)
        {
            if (const auto error = evmc_last_error_msg())
                std::cerr << "EVMC loading error: " << error << "\n";
            else
                std::cerr << "EVMC loading error " << ec << "\n";
            return {static_cast<int>(ec), {}};
        }

        std::cout << "External VM: " << evmc_config << "\n";
    }

    if (!benchmarks_dir.empty())
    {
        return {0, load_benchmarks_from_dir(benchmarks_dir)};
    }

    if (!code_hex_file.empty())
    {
        std::ifstream file{code_hex_file};
        return {0, {BenchmarkCase{code_hex_file,
                       from_spaced_hex(
                           std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{})
                           .value(),
                       {BenchmarkCase::Input{"", from_hex(input_hex).value(),
                           from_hex(expected_output_hex).value()}}}}};
    }

    return {0, {}};
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
        const auto [ec, benchmark_cases] = parseargs(argc, argv);
        if (ec == cli_parsing_error && ReportUnrecognizedArguments(argc, argv))
            return ec;

        if (ec != 0)
            return ec;

        registered_vms["advanced"] = evmc::VM{evmc_create_evmone(), {{"advanced", ""}}};
        registered_vms["baseline"] = evmc::VM{evmc_create_evmone()};
        registered_vms["bnocgoto"] = evmc::VM{evmc_create_evmone(), {{"cgoto", "no"}}};
        register_benchmarks(benchmark_cases);
        register_synthetic_benchmarks();
        RunSpecifiedBenchmarks();
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << "\n";
        return -1;
    }
}

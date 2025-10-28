// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "test/utils/utils.hpp"
#include <benchmark/benchmark.h>
#include <evmc/evmc.hpp>
#include <evmc/mocked_host.hpp>
#include <evmone/advanced_analysis.hpp>
#include <evmone/advanced_execution.hpp>
#include <evmone/baseline.hpp>
#include <evmone/vm.hpp>

namespace evmone::test
{
extern std::map<std::string_view, evmc::VM> registered_vms;

constexpr auto default_revision = EVMC_ISTANBUL;
constexpr auto default_gas_limit = std::numeric_limits<int64_t>::max();


template <typename ExecutionStateT, typename AnalysisT>
using ExecuteFn = evmc::Result(evmc::VM& vm, ExecutionStateT& exec_state, const AnalysisT&,
    const evmc_message&, evmc_revision, evmc::Host&, bytes_view);

template <typename AnalysisT>
using AnalyseFn = AnalysisT(evmc_revision, bytes_view);


struct FakeExecutionState
{};

struct FakeCodeAnalysis
{};

inline advanced::AdvancedCodeAnalysis advanced_analyse(evmc_revision rev, bytes_view code)
{
    return advanced::analyze(rev, code);
}

inline baseline::CodeAnalysis baseline_analyse(evmc_revision /*rev*/, bytes_view code)
{
    return baseline::analyze(code);
}

inline FakeCodeAnalysis evmc_analyse(evmc_revision /*rev*/, bytes_view /*code*/)
{
    return {};
}


inline evmc::Result advanced_execute(evmc::VM& /*vm*/, advanced::AdvancedExecutionState& exec_state,
    const advanced::AdvancedCodeAnalysis& analysis, const evmc_message& msg, evmc_revision rev,
    evmc::Host& host, bytes_view code)
{
    exec_state.reset(msg, rev, host.get_interface(), host.to_context(), code);
    return evmc::Result{execute(exec_state, analysis)};
}

inline evmc::Result baseline_execute(evmc::VM& c_vm, [[maybe_unused]] ExecutionState& exec_state,
    const baseline::CodeAnalysis& analysis, const evmc_message& msg, evmc_revision rev,
    evmc::Host& host, [[maybe_unused]] bytes_view code)
{
    auto& vm = *static_cast<evmone::VM*>(c_vm.get_raw_pointer());
    return evmc::Result{
        baseline::execute(vm, host.get_interface(), host.to_context(), rev, msg, analysis)};
}

inline evmc::Result evmc_execute(evmc::VM& vm, FakeExecutionState& /*exec_state*/,
    const FakeCodeAnalysis& /*analysis*/, const evmc_message& msg, evmc_revision rev,
    evmc::Host& host, bytes_view code) noexcept
{
    return vm.execute(host, rev, msg, code.data(), code.size());
}


template <typename AnalysisT, AnalyseFn<AnalysisT> analyse_fn>
inline void bench_analyse(benchmark::State& state, evmc_revision rev, bytes_view code) noexcept
{
    auto bytes_analysed = uint64_t{0};
    for (auto _ : state)
    {
        auto r = analyse_fn(rev, code);
        benchmark::DoNotOptimize(&r);
        bytes_analysed += code.size();
    }

    using benchmark::Counter;
    state.counters["size"] = Counter(static_cast<double>(code.size()));
    state.counters["rate"] = Counter(static_cast<double>(bytes_analysed), Counter::kIsRate);
}


template <typename ExecutionStateT, typename AnalysisT,
    ExecuteFn<ExecutionStateT, AnalysisT> execute_fn, AnalyseFn<AnalysisT> analyse_fn>
inline void bench_execute(benchmark::State& state, evmc::VM& vm, bytes_view code, bytes_view input,
    bytes_view expected_output) noexcept
{
    constexpr auto rev = default_revision;
    constexpr auto gas_limit = default_gas_limit;

    const auto analysis = analyse_fn(rev, code);
    evmc::MockedHost host;
    ExecutionStateT exec_state;
    evmc_message msg{};
    msg.kind = EVMC_CALL;
    msg.gas = gas_limit;
    msg.input_data = input.data();
    msg.input_size = input.size();


    {  // Test run.
        const auto r = execute_fn(vm, exec_state, analysis, msg, rev, host, code);
        if (r.status_code != EVMC_SUCCESS)
        {
            state.SkipWithError(("failure: " + std::to_string(r.status_code)).c_str());
            return;
        }

        if (!expected_output.empty())
        {
            const auto output = bytes_view{r.output_data, r.output_size};
            if (output != expected_output)
            {
                state.SkipWithError(
                    ("got: " + hex(output) + "  expected: " + hex(expected_output)).c_str());
                return;
            }
        }
    }

    auto total_gas_used = int64_t{0};
    auto iteration_gas_used = int64_t{0};
    for (auto _ : state)
    {
        const auto r = execute_fn(vm, exec_state, analysis, msg, rev, host, code);
        iteration_gas_used = gas_limit - r.gas_left;
        total_gas_used += iteration_gas_used;
    }

    using benchmark::Counter;
    state.counters["gas_used"] = Counter(static_cast<double>(iteration_gas_used));
    state.counters["gas_rate"] = Counter(static_cast<double>(total_gas_used), Counter::kIsRate);
}


constexpr auto bench_advanced_execute = bench_execute<advanced::AdvancedExecutionState,
    advanced::AdvancedCodeAnalysis, advanced_execute, advanced_analyse>;

constexpr auto bench_baseline_execute =
    bench_execute<ExecutionState, baseline::CodeAnalysis, baseline_execute, baseline_analyse>;

inline void bench_evmc_execute(benchmark::State& state, evmc::VM& vm, bytes_view code,
    bytes_view input = {}, bytes_view expected_output = {})
{
    bench_execute<FakeExecutionState, FakeCodeAnalysis, evmc_execute, evmc_analyse>(
        state, vm, code, input, expected_output);
}

}  // namespace evmone::test

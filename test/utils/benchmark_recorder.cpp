// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "benchmark_recorder.hpp"

#ifdef EVMONE_CODSPEED
#include <codspeed.h>
#include <core.h>
#include <unistd.h>
#endif

namespace evmone::test
{
#ifdef EVMONE_CODSPEED
BenchmarkRecorder::BenchmarkRecorder() noexcept
{
    auto* const hooks = instrument_hooks_init();
    if (hooks == nullptr)
        return;

    // A build with the hooks in it still runs as the plain test tool when no runner is
    // attached, and then has nothing to report to.
    if (!instrument_hooks_is_instrumented(hooks))
    {
        instrument_hooks_deinit(hooks);
        return;
    }

    // The integration this names is the one whose library measures here, the same the C++
    // benchmarks use. CodSpeed generates the flame graphs only for an integration it knows,
    // and withholds them from a name of our own. The version is the fork's, pinned a
    // directory up, and is not derived from it because that directory is configured later.
    instrument_hooks_set_integration(hooks, "codspeed-cpp", "2.4.0");
    m_hooks = hooks;
}

BenchmarkRecorder::~BenchmarkRecorder()
{
    if (m_hooks != nullptr)
        instrument_hooks_deinit(m_hooks);
}

void BenchmarkRecorder::__codspeed_root_frame__measure(
    const char* file, const std::string& name, const std::function<void()>& run) const
{
    instrument_hooks_start_benchmark_inline(m_hooks);
    run();
    instrument_hooks_stop_benchmark_inline(m_hooks);

    // The name a fixture carries is the file it was filled from in another repository, which
    // CodSpeed cannot find, so the file registering it goes in front. Built once the stop has
    // been counted, the work of it being no part of the benchmark.
    const auto uri = codspeed::get_path_relative_to_workspace(file) + "::" + name;
    instrument_hooks_set_executed_benchmark(m_hooks, getpid(), uri.c_str());
}

#else

BenchmarkRecorder::BenchmarkRecorder() noexcept = default;
BenchmarkRecorder::~BenchmarkRecorder() = default;

// Never reached, the recorder being inactive without the hooks, but a call to it is still
// compiled.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void BenchmarkRecorder::__codspeed_root_frame__measure(
    const char* /*file*/, const std::string& /*name*/, const std::function<void()>& run) const
{
    run();
}

#endif
}  // namespace evmone::test

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
namespace
{
/// The name of a benchmark begins with the file registering it, relative to the repository,
/// which is how CodSpeed finds it in the source. A fixture names the file it was filled from
/// in another repository, which is no help, so this stands in front of it.
const std::string& uri_prefix()
{
    static const auto prefix = codspeed::get_path_relative_to_workspace(__FILE__) + "::";
    return prefix;
}
}  // namespace

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

    // PROBE: claims to be the google-benchmark integration, which this is not, to find out
    // whether the flame graphs are withheld from a harness CodSpeed does not recognize.
    instrument_hooks_set_integration(hooks, "codspeed-cpp", "2.4.0");
    m_hooks = hooks;
}

BenchmarkRecorder::~BenchmarkRecorder()
{
    if (m_hooks != nullptr)
        instrument_hooks_deinit(m_hooks);
}

void BenchmarkRecorder::__codspeed_root_frame__measure(
    const std::string& uri, const std::function<void()>& run) const
{
    instrument_hooks_start_benchmark_inline(m_hooks);
    run();
    instrument_hooks_stop_benchmark_inline(m_hooks);
    instrument_hooks_set_executed_benchmark(m_hooks, getpid(), (uri_prefix() + uri).c_str());
}

#else

BenchmarkRecorder::BenchmarkRecorder() noexcept = default;
BenchmarkRecorder::~BenchmarkRecorder() = default;

// Never reached, the recorder being inactive without the hooks, but a call to it is still
// compiled.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void BenchmarkRecorder::__codspeed_root_frame__measure(
    const std::string& /*uri*/, const std::function<void()>& run) const
{
    run();
}

#endif
}  // namespace evmone::test

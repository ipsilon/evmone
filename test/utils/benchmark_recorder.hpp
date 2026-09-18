// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <functional>
#include <optional>
#include <source_location>
#include <string>
#include <utility>

/// The handle of the CodSpeed runner, an opaque type of its C library, declared where that
/// library declares it.
struct InstrumentHooks;

namespace evmone::test
{
/// Reports the tests it runs to the CodSpeed runner as benchmarks.
///
/// It is inactive unless the tool was built with EVMONE_CODSPEED and runs under the runner,
/// so the one binary serves as both the test runner and the benchmark harness.
class BenchmarkRecorder
{
    /// The runner's handle, null while nothing is being measured.
    InstrumentHooks* m_hooks = nullptr;

public:
    BenchmarkRecorder() noexcept;
    ~BenchmarkRecorder();

    BenchmarkRecorder(const BenchmarkRecorder&) = delete;
    BenchmarkRecorder& operator=(const BenchmarkRecorder&) = delete;

    /// Whether the runner is attached and the measurements reach it.
    [[nodiscard]] bool active() const noexcept { return m_hooks != nullptr; }

    /// Runs @p run and reports what it cost as the benchmark called @p name.
    ///
    /// The benchmark is identified by the file it is registered from, which @p loc names, and
    /// so defaults to the caller's, followed by @p name.
    template <typename F>
    auto measure(const std::string& name, F&& run,
        const std::source_location loc = std::source_location::current()) const
    {
        if (!active())
            return run();

        // The measured call goes through the function CodSpeed roots the flame graph at, which
        // is why it takes an erased callable: the name of a template instantiated on a lambda
        // is not one the runner can be expected to recognize.
        std::optional<decltype(run())> result;
        __codspeed_root_frame__measure(loc.file_name(), name, [&] { result.emplace(run()); });
        return *std::move(result);
    }

private:
    /// Runs @p run between the start and the stop of the benchmark named after @p file and
    /// @p name.
    ///
    /// CodSpeed roots the flame graph at the frame whose name begins this way, so the name
    /// matters as much as the body. Every instruction between the start and the stop is
    /// counted, which is why nothing but the call stands between them.
    // NOLINTNEXTLINE(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
    void __codspeed_root_frame__measure(
        const char* file, const std::string& name, const std::function<void()>& run) const;
};
}  // namespace evmone::test

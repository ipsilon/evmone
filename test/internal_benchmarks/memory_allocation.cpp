// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <benchmark/benchmark.h>
#include <cstdlib>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/mman.h>
#endif

namespace
{
void malloc_(size_t size) noexcept
{
    auto m = std::malloc(size);
    benchmark::DoNotOptimize(m);
    std::free(m);
}

void calloc_(size_t size) noexcept
{
    auto m = std::calloc(1, size);
    benchmark::DoNotOptimize(m);
    std::free(m);
}

void os_specific(size_t size) noexcept
{
#if defined(__unix__) || defined(__APPLE__)
    auto m = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (m == MAP_FAILED)
        __builtin_trap();
    munmap(m, size);
#else
    (void)size;
#endif
}


template <void (*F)(size_t) noexcept>
void allocate(benchmark::State& state)
{
    const auto size = static_cast<size_t>(state.range(0)) * 1024;

    for (auto _ : state)
        F(size);
}

#define ARGS ->RangeMultiplier(2)->Range(1, 128 * 1024)

BENCHMARK_TEMPLATE(allocate, malloc_) ARGS;
BENCHMARK_TEMPLATE(allocate, calloc_) ARGS;
BENCHMARK_TEMPLATE(allocate, os_specific) ARGS;


#if defined(__unix__) || defined(__APPLE__)
void bench_mprotect(benchmark::State& state)
{
    const auto page_size = static_cast<size_t>(getpagesize());
    const auto size = static_cast<size_t>(state.range(0)) * page_size;
    const auto idx = static_cast<size_t>(state.range(1));

    auto prot = PROT_READ | PROT_WRITE;
    const auto m = mmap(nullptr, size, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (m == MAP_FAILED)
        state.SkipWithError("mmap failed");

    const auto p = &static_cast<char*>(m)[idx * page_size];

    for (auto _ : state)
    {
        prot = (prot == PROT_NONE) ? (PROT_READ | PROT_WRITE) : PROT_NONE;
        const auto res = mprotect(p, page_size, prot);
        if (res != 0) [[unlikely]]
            state.SkipWithError("mprotect failed");
    }

    munmap(m, size);
}
BENCHMARK(bench_mprotect)->Args({1, 0})->Args({8 * 1024, 13 * 9});
#endif

}  // namespace

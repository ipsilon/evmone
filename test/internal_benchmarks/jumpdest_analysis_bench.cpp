// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

// Benchmarks of JUMPDEST analysis kernels: the current evmone loop and the data-independent
// SIMD variants. Each benchmark checks its result against the reference before timing.

#include "test_bytecodes.hpp"
#include <benchmark/benchmark.h>
#include <evmc/hex.hpp>
#include <evmone/constants.hpp>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <random>
#include <string>
#include <vector>

#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
#define JDA_X86 1
#include <immintrin.h>
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
#define JDA_NEON 1
#include <arm_neon.h>
#endif

namespace
{
using u8 = uint8_t;
using u64 = uint64_t;

/// Writes the JUMPDEST bitset of the code of the given size. The code is 64-byte aligned and
/// followed by at least 64 zero bytes; the bitset has (size + 64) / 64 zeroed words.
using AnalyzeFn = void (*)(const u8* code, size_t size, u64* bits);

/// The current evmone loop (lib/evmone/baseline_analysis.cpp).
void reference(const u8* code, size_t size, u64* bits)
{
    for (size_t i = 0; i < size; ++i)
    {
        const auto op = code[i];
        if (static_cast<int8_t>(op) >= 0x60)
            i += op - size_t{0x5f};
        else if (__builtin_expect(op == 0x5b, 0))
            bits[i / 64] |= u64{1} << (i % 64);
    }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wunused-function"
#define MCA_BEGIN(name) ((void)0)
#define MCA_END(name) ((void)0)
#if JDA_X86
#include "jumpdest/v4.hpp"
#include "jumpdest/g8.hpp"
#include "jumpdest/g9.hpp"
#include "jumpdest/pipe.hpp"
#endif
#if JDA_NEON
#include "jumpdest/neon.hpp"
#endif
#undef MCA_BEGIN
#undef MCA_END
#pragma GCC diagnostic pop

struct Variant
{
    const char* name;
    AnalyzeFn fn;
    bool (*supported)();
};

bool always()
{
    return true;
}

#if JDA_X86
bool has_sse41()
{
    return __builtin_cpu_supports("ssse3") && __builtin_cpu_supports("sse4.1");
}
bool has_avx2()
{
    return __builtin_cpu_supports("avx2");
}
#endif

const Variant variants[] = {
    {"reference", reference, always},
#if JDA_X86
    {"v4_sse", g16v4_sse, has_sse41},
    {"g8_sse", g8_sse, has_sse41},
    {"v4_avx2", g16v4_avx2, has_avx2},
    {"g8_avx2", g8_avx2, has_avx2},
    {"g9np_avx2", g9np_avx2, has_avx2},
    {"g9_avx2", g9_avx2, has_avx2},
    {"pipe_avx2", gfinal_avx2, has_avx2},
    {"pipe_bytes_avx2", gfinal_bytes_avx2, has_avx2},
#endif
#if JDA_NEON
    {"v4_neon", g16v4_neon, always},
#endif
};

constexpr size_t PADDING = 64;

/// A code buffer as the analysis gets it: aligned and followed by zero padding.
struct Input
{
    std::string name;
    size_t size = 0;
    std::unique_ptr<u8[], decltype(&std::free)> storage{nullptr, &std::free};
    std::vector<u64> expected;

    Input(std::string n, const std::vector<u8>& code) : name{std::move(n)}, size{code.size()}
    {
        const auto alloc_size = (size + PADDING + 63) / 64 * 64;
        storage.reset(static_cast<u8*>(std::aligned_alloc(64, alloc_size)));
        std::memset(storage.get(), 0, alloc_size);
        std::copy(code.begin(), code.end(), storage.get());
        expected.resize(num_words());
        reference(storage.get(), size, expected.data());
    }

    [[nodiscard]] size_t num_words() const noexcept { return (size + 64) / 64; }
};

std::vector<u8> random_code(size_t size, std::vector<u8> alphabet, std::vector<int> weights)
{
    std::mt19937_64 rng{0};
    std::discrete_distribution<size_t> dist(weights.begin(), weights.end());
    std::vector<u8> code(size);
    for (auto& b : code)
        b = alphabet[dist(rng)];
    return code;
}

std::vector<u8> tiled(size_t size, std::vector<u8> pattern)
{
    std::vector<u8> code(size);
    for (size_t i = 0; i < size; ++i)
        code[i] = pattern[i % pattern.size()];
    return code;
}

std::vector<Input> make_inputs()
{
    std::vector<Input> inputs;
    for (size_t i = 0; i < evmone::test::test_bytecodes.size(); ++i)
    {
        const auto code = evmc::from_hex(evmone::test::test_bytecodes[i]).value();
        inputs.emplace_back(
            "real" + std::to_string(i) + "_" + std::to_string(code.size()),
            std::vector<u8>(code.begin(), code.end()));
    }

    // The maximum initcode size (EIP-7954); the random alphabets are from the execution-specs
    // jumpdest analysis benchmark.
    constexpr size_t N = evmone::MAX_INITCODE_SIZE_AMSTERDAM;
    std::vector<u8> all(256);
    for (size_t i = 0; i < all.size(); ++i)
        all[i] = static_cast<u8>(i);
    inputs.emplace_back("stop", tiled(N, {0x00}));
    inputs.emplace_back("jumpdest", tiled(N, {0x5b}));
    inputs.emplace_back("push1_jumpdest", tiled(N, {0x60, 0x5b}));
    inputs.emplace_back("push1", tiled(N, {0x60}));
    inputs.emplace_back("push32", tiled(N, {0x7f}));
    inputs.emplace_back("random_all", random_code(N, all, std::vector<int>(256, 1)));
    inputs.emplace_back("random_stop_jumpdest", random_code(N, {0x00, 0x5b}, {1, 1}));
    inputs.emplace_back(
        "random_stop_jumpdest_push1", random_code(N, {0x00, 0x5b, 0x60}, {1, 1, 1}));
    inputs.emplace_back("random_jumpdest_push1", random_code(N, {0x5b, 0x60}, {1, 1}));
    inputs.emplace_back(
        "random_stop_jumpdest_2push1", random_code(N, {0x00, 0x5b, 0x60}, {1, 1, 2}));
    return inputs;
}

void jumpdest_analysis(benchmark::State& state, const Variant& v, const Input& in)
{
    std::vector<u64> bits(in.num_words() + 1);  // One guard word.
    const auto* code = in.storage.get();

    v.fn(code, in.size, bits.data());
    if (!std::equal(in.expected.begin(), in.expected.end(), bits.begin()) || bits.back() != 0)
        return state.SkipWithError("wrong result");

    for ([[maybe_unused]] auto _ : state)
    {
        std::fill(bits.begin(), bits.end(), 0);
        v.fn(code, in.size, bits.data());
        benchmark::DoNotOptimize(bits.data());
        benchmark::ClobberMemory();
    }

    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(in.size));
    // The initcode cost that pays for the analysis (EIP-3860).
    const auto gas = 2 * ((in.size + 31) / 32);
    state.counters["gas_rate"] = benchmark::Counter(
        static_cast<double>(gas), benchmark::Counter::kIsIterationInvariantRate);
}

[[maybe_unused]] const auto registered = [] {
    static const auto inputs = make_inputs();
    for (const auto& in : inputs)
    {
        for (const auto& v : variants)
        {
            if (!v.supported())
                continue;
            benchmark::RegisterBenchmark("jumpdest_analysis/" + in.name + "/" + v.name,
                [&v, &in](benchmark::State& state) { jumpdest_analysis(state, v, in); })
                ->ComputeStatistics("min", [](const std::vector<double>& x) {
                    return *std::min_element(x.begin(), x.end());
                });
        }
    }
    return true;
}();
}  // namespace

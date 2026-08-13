// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <benchmark/benchmark.h>
#include <evmmax/evmmax.hpp>

using namespace intx;

namespace
{
constexpr auto bn254 = 0x30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd47_u256;
constexpr auto secp256k1 = 0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f_u256;

template <typename UintT, const UintT& Mod>
void evmmax_add(benchmark::State& state)
{
    const evmmax::ModArith<UintT> m{Mod};
    auto a = Mod / 2;
    auto b = Mod / 3;

    for ([[maybe_unused]] auto _ : state)
    {
        a = m.add(a, b);
        b = m.add(b, a);
    }
    state.counters["avg_time_per_item"] =
        benchmark::Counter(2.0 * static_cast<double>(state.iterations()),
            benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}

template <typename UintT, const UintT& Mod>
void evmmax_sub(benchmark::State& state)
{
    const evmmax::ModArith<UintT> m{Mod};
    auto a = Mod / 2;
    auto b = Mod / 3;

    for ([[maybe_unused]] auto _ : state)
    {
        a = m.sub(a, b);
        b = m.sub(b, a);
    }
    state.counters["avg_time_per_item"] =
        benchmark::Counter(2.0 * static_cast<double>(state.iterations()),
            benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}

template <typename UintT, const UintT& Mod>
void evmmax_mul(benchmark::State& state)
{
    const evmmax::ModArith<UintT> m{Mod};
    auto a = m.to_mont(Mod / 2);
    auto b = m.to_mont(Mod / 3);

    for ([[maybe_unused]] auto _ : state)
    {
        a = m.mul(a, b);
        b = m.mul(b, a);
    }
    state.counters["avg_time_per_item"] =
        benchmark::Counter(2.0 * static_cast<double>(state.iterations()),
            benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
}  // namespace

BENCHMARK_TEMPLATE(evmmax_add, uint256, bn254);
BENCHMARK_TEMPLATE(evmmax_add, uint256, secp256k1);
BENCHMARK_TEMPLATE(evmmax_sub, uint256, bn254);
BENCHMARK_TEMPLATE(evmmax_sub, uint256, secp256k1);
BENCHMARK_TEMPLATE(evmmax_mul, uint256, bn254);
BENCHMARK_TEMPLATE(evmmax_mul, uint256, secp256k1);

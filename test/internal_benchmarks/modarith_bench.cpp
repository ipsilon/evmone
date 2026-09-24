// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <benchmark/benchmark.h>
#include <evmone_precompiles/modarith.hpp>

using namespace intx;
using evmone::crypto::MontgomeryArith;

namespace
{
constexpr auto bn254 = 0x30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd47_u256;
constexpr auto secp256k1 = 0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f_u256;

// Each pair of operations forms a single dependency chain, so the reported time is the latency.

template <const auto& Mod>
void modarith_add(benchmark::State& state)
{
    using M = MontgomeryArith<Mod>;
    auto a = Mod / 2;
    auto b = Mod / 3;

    while (state.KeepRunningBatch(2))
    {
        a = M::add(a, b);
        b = M::add(b, a);
    }
    benchmark::DoNotOptimize(a);
    benchmark::DoNotOptimize(b);
}

template <const auto& Mod>
void modarith_sub(benchmark::State& state)
{
    using M = MontgomeryArith<Mod>;
    auto a = Mod / 2;
    auto b = Mod / 3;

    while (state.KeepRunningBatch(2))
    {
        a = M::sub(a, b);
        b = M::sub(b, a);
    }
    benchmark::DoNotOptimize(a);
    benchmark::DoNotOptimize(b);
}

template <const auto& Mod>
void modarith_mul(benchmark::State& state)
{
    using M = MontgomeryArith<Mod>;
    auto a = M::to_internal(Mod / 2);
    auto b = M::to_internal(Mod / 3);

    while (state.KeepRunningBatch(2))
    {
        a = M::mul(a, b);
        b = M::mul(b, a);
    }
    benchmark::DoNotOptimize(a);
    benchmark::DoNotOptimize(b);
}
}  // namespace

BENCHMARK_TEMPLATE(modarith_add, bn254);
BENCHMARK_TEMPLATE(modarith_add, secp256k1);
BENCHMARK_TEMPLATE(modarith_sub, bn254);
BENCHMARK_TEMPLATE(modarith_sub, secp256k1);
BENCHMARK_TEMPLATE(modarith_mul, bn254);
BENCHMARK_TEMPLATE(modarith_mul, secp256k1);

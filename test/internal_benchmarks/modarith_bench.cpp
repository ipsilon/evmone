// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <benchmark/benchmark.h>
#include <evmone_precompiles/modarith.hpp>

using namespace intx;

namespace
{
constexpr auto bn254 = 0x30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd47_u256;
constexpr auto secp256k1 = 0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f_u256;

template <typename UintT, const UintT& Mod>
void modarith_add(benchmark::State& state)
{
    const evmone::crypto::ModArith<UintT> m{Mod};
    auto a = Mod / 2;
    auto b = Mod / 3;

    while (state.KeepRunningBatch(2))
    {
        a = m.add(a, b);
        b = m.add(b, a);
    }
}

template <typename UintT, const UintT& Mod>
void modarith_sub(benchmark::State& state)
{
    const evmone::crypto::ModArith<UintT> m{Mod};
    auto a = Mod / 2;
    auto b = Mod / 3;

    while (state.KeepRunningBatch(2))
    {
        a = m.sub(a, b);
        b = m.sub(b, a);
    }
}

template <typename UintT, const UintT& Mod>
void modarith_mul(benchmark::State& state)
{
    const evmone::crypto::ModArith<UintT> m{Mod};
    auto a = m.to_mont(Mod / 2);
    auto b = m.to_mont(Mod / 3);

    while (state.KeepRunningBatch(2))
    {
        a = m.mul(a, b);
        b = m.mul(b, a);
    }
}
}  // namespace

BENCHMARK_TEMPLATE(modarith_add, uint256, bn254);
BENCHMARK_TEMPLATE(modarith_add, uint256, secp256k1);
BENCHMARK_TEMPLATE(modarith_sub, uint256, bn254);
BENCHMARK_TEMPLATE(modarith_sub, uint256, secp256k1);
BENCHMARK_TEMPLATE(modarith_mul, uint256, bn254);
BENCHMARK_TEMPLATE(modarith_mul, uint256, secp256k1);

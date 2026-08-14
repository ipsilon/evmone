// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2024 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "../state/hash_utils.hpp"
#include <benchmark/benchmark.h>
#include <evmone/lru_cache.hpp>
#include <memory>

using evmone::hash256;

namespace
{
template <typename Key, typename Value>
void lru_cache_not_found(benchmark::State& state)
{
    const auto capacity = static_cast<size_t>(state.range(0));
    evmone::LRUCache<Key, Value> cache(capacity);

    std::vector<Key> keys(capacity + 1, Key{});
    for (size_t i = 0; i < keys.size(); ++i)
        keys[i] = static_cast<Key>(i);
    benchmark::ClobberMemory();

    for (size_t i = 0; i < capacity; ++i)
        cache.put(keys[i], {});

    const volatile auto key = &keys[capacity];

    for ([[maybe_unused]] auto _ : state)
    {
        auto v = cache.get(*key);
        benchmark::DoNotOptimize(v);
        if (v.has_value()) [[unlikely]]
            state.SkipWithError("found");
    }
}
BENCHMARK(lru_cache_not_found<int, int>)->Arg(5000);
BENCHMARK(lru_cache_not_found<hash256, std::shared_ptr<char>>)->Arg(5000);


template <typename Key, typename Value>
void lru_cache_get_same(benchmark::State& state)
{
    const auto capacity = static_cast<size_t>(state.range(0));
    evmone::LRUCache<Key, Value> cache(capacity);

    std::vector<Key> keys(capacity, Key{});
    for (size_t i = 0; i < keys.size(); ++i)
        keys[i] = static_cast<Key>(i);
    benchmark::ClobberMemory();

    for (const auto key : keys)
        cache.put(key, {});

    const volatile auto key = &keys[capacity / 2];

    for ([[maybe_unused]] auto _ : state)
    {
        auto v = cache.get(*key);
        benchmark::DoNotOptimize(v);
        if (!v.has_value()) [[unlikely]]
            state.SkipWithError("not found");
    }
}
BENCHMARK(lru_cache_get_same<int, int>)->Arg(5000);
BENCHMARK(lru_cache_get_same<hash256, std::shared_ptr<char>>)->Arg(5000);


template <typename Key, typename Value>
void lru_cache_get(benchmark::State& state)
{
    const auto capacity = static_cast<size_t>(state.range(0));
    evmone::LRUCache<Key, Value> cache(capacity);

    std::vector<Key> data(capacity, Key{});
    for (size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<Key>(i);
    benchmark::ClobberMemory();

    for (const auto& key : data)
        cache.put(key, {});

    auto key_it = data.begin();
    for ([[maybe_unused]] auto _ : state)
    {
        auto v = cache.get(*key_it++);
        benchmark::DoNotOptimize(v);
        if (!v.has_value()) [[unlikely]]
            state.SkipWithError("not found");

        if (key_it == data.end())
            key_it = data.begin();
    }
}
BENCHMARK(lru_cache_get<int, int>)->Arg(5000);
BENCHMARK(lru_cache_get<hash256, std::shared_ptr<char>>)->Arg(5000);


template <typename Key, typename Value>
void lru_cache_put_empty(benchmark::State& state)
{
    const auto capacity = static_cast<size_t>(state.range(0));
    evmone::LRUCache<Key, Value> cache(capacity);

    std::vector<Key> data(capacity, Key{});
    for (size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<Key>(i);
    benchmark::ClobberMemory();

    while (state.KeepRunningBatch(static_cast<benchmark::IterationCount>(capacity)))
    {
        for (const auto& key : data)
        {
            cache.put(key, {});
        }
        state.PauseTiming();
        cache.clear();
        state.ResumeTiming();
    }
}
BENCHMARK(lru_cache_put_empty<int, int>)->Arg(5000);
BENCHMARK(lru_cache_put_empty<hash256, std::shared_ptr<char>>)->Arg(5000);


template <typename Key, typename Value>
void lru_cache_put_full(benchmark::State& state)
{
    const auto capacity = static_cast<size_t>(state.range(0));
    evmone::LRUCache<Key, Value> cache(capacity);

    std::vector<Key> keys(capacity, Key{});
    for (size_t i = 0; i < keys.size(); ++i)
        keys[i] = static_cast<Key>(i);
    benchmark::ClobberMemory();

    for (const auto& key : keys)
        cache.put(key, {});

    auto key_index = keys.size();
    for ([[maybe_unused]] auto _ : state)
    {
        cache.put(static_cast<Key>(key_index), {});
        ++key_index;
    }
}
BENCHMARK(lru_cache_put_full<int, int>)->Arg(5000);
BENCHMARK(lru_cache_put_full<hash256, std::shared_ptr<char>>)->Arg(5000);

}  // namespace

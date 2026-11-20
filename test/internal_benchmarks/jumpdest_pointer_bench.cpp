// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

// Benchmarks of the JUMPDEST analysis loop of lib/evmone/baseline_analysis.cpp: the index loop
// (index), the pointer walk of the ZisK Ethereum guest program, ziskethone patch 04 (zisk), the
// index loop with the index replaced by a pointer (ptr), and the zisk loop with signed byte loads
// (szisk). Each variant is compiled 4 times, with its code shifted by 0, 16, 32 and 48 bytes from
// a 64-byte boundary, so that no result depends on one alignment of the loop. Each benchmark
// checks its result against the index loop.
//
// Name: jumpdest_pointer/<input>/<variant>/s<shift>.

#include "test_bytecodes.hpp"
#include <benchmark/benchmark.h>
#include <evmc/bytes.hpp>
#include <evmc/hex.hpp>
#include <evmone/bitset_span.hpp>
#include <evmone/instructions_opcodes.hpp>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <vector>

#define JDP_UNLIKELY(EXPR) __builtin_expect(bool{EXPR}, false)

#if defined(__x86_64__)
#define JDP_JUMP "jmp"
#elif defined(__aarch64__)
#define JDP_JUMP "b"
#elif defined(__riscv)
#define JDP_JUMP "j"
#else
#error "unsupported architecture"
#endif

/// Jumps over padding which places the code that follows K bytes after a 64-byte boundary.
#define JDP_SHIFT(K) asm volatile(JDP_JUMP " 1f\n\t.p2align 6\n\t.skip " #K "\n1:" :::)

namespace
{
using evmc::bytes_view;
using evmone::BitsetSpan;
using evmone::OP_JUMPDEST;
using evmone::OP_PUSH1;
using evmone::OP_PUSH32;

static_assert(OP_PUSH32 == std::numeric_limits<int8_t>::max());

[[gnu::always_inline]] inline void index_loop(BitsetSpan map, bytes_view code) noexcept
{
    for (size_t i = 0; i < code.size(); ++i)
    {
        const auto op = code[i];
        if (static_cast<int8_t>(op) >= OP_PUSH1)
            i += op - size_t{OP_PUSH1 - 1};
        else if (JDP_UNLIKELY(op == OP_JUMPDEST))
            map.set(i);
    }
}

[[gnu::always_inline]] inline void zisk_loop(BitsetSpan map, bytes_view code) noexcept
{
    const auto* const begin = code.data();
    const auto* const end = begin + code.size();
    for (const auto* p = begin; p < end;)
    {
        const auto op = *p;
        if (static_cast<int8_t>(op) >= OP_PUSH1)
        {
            p += op - size_t{OP_PUSH1 - 2};
        }
        else
        {
            if (JDP_UNLIKELY(op == OP_JUMPDEST))
                map.set(static_cast<size_t>(p - begin));
            ++p;
        }
    }
}

[[gnu::always_inline]] inline void ptr_loop(BitsetSpan map, bytes_view code) noexcept
{
    const auto* const begin = code.data();
    const auto* const end = begin + code.size();
    for (const auto* p = begin; p < end; ++p)
    {
        const auto op = *p;
        if (static_cast<int8_t>(op) >= OP_PUSH1)
            p += op - size_t{OP_PUSH1 - 1};
        else if (JDP_UNLIKELY(op == OP_JUMPDEST))
            map.set(static_cast<size_t>(p - begin));
    }
}

[[gnu::always_inline]] inline void szisk_loop(BitsetSpan map, bytes_view code) noexcept
{
    const auto* const begin = reinterpret_cast<const int8_t*>(code.data());
    const auto* const end = begin + code.size();
    for (const auto* p = begin; p < end;)
    {
        const auto op = *p;
        if (op >= OP_PUSH1)
            p += static_cast<uint8_t>(op) - size_t{OP_PUSH1 - 2};
        else
        {
            if (JDP_UNLIKELY(op == OP_JUMPDEST))
                map.set(static_cast<size_t>(p - begin));
            ++p;
        }
    }
}

#define JDP_VARIANT(NAME, K)                                                          \
    [[gnu::noinline]] void jdp_##NAME##_##K(BitsetSpan map, bytes_view code) noexcept \
    {                                                                                 \
        JDP_SHIFT(K);                                                                 \
        NAME##_loop(map, code);                                                       \
    }
#define JDP_VARIANTS(NAME) \
    JDP_VARIANT(NAME, 0)   \
    JDP_VARIANT(NAME, 16)  \
    JDP_VARIANT(NAME, 32)  \
    JDP_VARIANT(NAME, 48)

JDP_VARIANTS(index)
JDP_VARIANTS(zisk)
JDP_VARIANTS(ptr)
JDP_VARIANTS(szisk)

using AnalyzeFn = void (*)(BitsetSpan, bytes_view) noexcept;

struct Variant
{
    const char* name;
    unsigned shift;
    AnalyzeFn fn;
};

#define JDP_ENTRY(NAME, K)         \
    Variant                        \
    {                              \
        #NAME, K, jdp_##NAME##_##K \
    }
#define JDP_ENTRIES(NAME) \
    JDP_ENTRY(NAME, 0), JDP_ENTRY(NAME, 16), JDP_ENTRY(NAME, 32), JDP_ENTRY(NAME, 48)
const Variant variants[] = {
    JDP_ENTRIES(index), JDP_ENTRIES(zisk), JDP_ENTRIES(ptr), JDP_ENTRIES(szisk)};

/// The padding of the code in evmone (32 bytes of the PUSH32 data and a STOP), rounded up.
constexpr size_t PADDING = 64;

struct Input
{
    std::string name;
    std::vector<uint8_t> padded_code;
    size_t size = 0;
    std::vector<BitsetSpan::word_type> expected;

    Input(std::string n, std::vector<uint8_t> code) : name{std::move(n)}, size{code.size()}
    {
        padded_code = std::move(code);
        padded_code.resize(size + PADDING);
        expected.resize(num_words());
        index_loop(BitsetSpan{expected.data()}, this->code());
    }

    [[nodiscard]] bytes_view code() const noexcept { return {padded_code.data(), size}; }
    [[nodiscard]] size_t num_words() const noexcept
    {
        return (size + BitsetSpan::WORD_BITS) / BitsetSpan::WORD_BITS;
    }
};

std::vector<uint8_t> tiled(size_t size, std::vector<uint8_t> pattern)
{
    std::vector<uint8_t> code(size);
    for (size_t i = 0; i < size; ++i)
        code[i] = pattern[i % pattern.size()];
    return code;
}

std::vector<uint8_t> random_bytes(size_t size)
{
    std::mt19937_64 rng{0};
    std::uniform_int_distribution<unsigned> byte{0, 255};
    std::vector<uint8_t> code(size);
    for (auto& b : code)
        b = static_cast<uint8_t>(byte(rng));
    return code;
}

/// A random instruction stream: PUSH1-PUSH32 with data of JUMPDEST, PUSH and STOP bytes,
/// JUMPDEST and STOP, with the weights 2:1:1.
std::vector<uint8_t> random_instructions(size_t size)
{
    const uint8_t data[] = {OP_JUMPDEST, OP_PUSH1, OP_PUSH32, 0x00};
    std::mt19937_64 rng{0};
    std::discrete_distribution<int> kind{2, 1, 1};
    std::uniform_int_distribution<unsigned> width{1, 32};
    std::uniform_int_distribution<size_t> data_byte{0, std::size(data) - 1};
    std::vector<uint8_t> code;
    while (code.size() < size)
    {
        switch (kind(rng))
        {
        case 0:
        {
            const auto n = width(rng);
            code.push_back(static_cast<uint8_t>(OP_PUSH1 - 1 + n));
            for (unsigned j = 0; j < n; ++j)
                code.push_back(data[data_byte(rng)]);
            break;
        }
        case 1:
            code.push_back(OP_JUMPDEST);
            break;
        default:
            code.push_back(0x00);
            break;
        }
    }
    code.resize(size);
    return code;
}

std::vector<Input> make_inputs()
{
    std::vector<Input> inputs;
    std::vector<uint8_t> all;
    for (size_t i = 0; i < evmone::test::test_bytecodes.size(); ++i)
    {
        const auto code = evmc::from_hex(evmone::test::test_bytecodes[i]).value();
        all.insert(all.end(), code.begin(), code.end());
        inputs.emplace_back("real" + std::to_string(i) + "_" + std::to_string(code.size()),
            std::vector<uint8_t>(code.begin(), code.end()));
    }
    // All the contracts in one code: too long for the branch predictor to learn.
    inputs.emplace_back("real_all_" + std::to_string(all.size()), all);

    constexpr size_t N = 128 * 1024;  // The maximum initcode size (EIP-7954).
    inputs.emplace_back("stop", tiled(N, {0x00}));
    inputs.emplace_back("jumpdest", tiled(N, {OP_JUMPDEST}));
    inputs.emplace_back("push1", tiled(N, {OP_PUSH1}));
    inputs.emplace_back("push32", tiled(N, {OP_PUSH32}));
    inputs.emplace_back("rand_bytes", random_bytes(N));
    inputs.emplace_back("rand_push1to32", random_instructions(N));
    return inputs;
}

void jumpdest_pointer(benchmark::State& state, const Variant& v, const Input& in)
{
    std::vector<BitsetSpan::word_type> bits(in.num_words() + 1);  // One guard word.

    v.fn(BitsetSpan{bits.data()}, in.code());
    if (!std::equal(in.expected.begin(), in.expected.end(), bits.begin()) || bits.back() != 0)
        return state.SkipWithError("wrong result");

    for ([[maybe_unused]] auto _ : state)
    {
        std::fill(bits.begin(), bits.end(), 0);
        v.fn(BitsetSpan{bits.data()}, in.code());
        benchmark::DoNotOptimize(bits.data());
        benchmark::ClobberMemory();
    }
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(in.size));
}

[[maybe_unused]] const auto registered = [] {
    static const auto inputs = make_inputs();
    for (const auto& in : inputs)
    {
        for (const auto& v : variants)
        {
            const auto name =
                "jumpdest_pointer/" + in.name + "/" + v.name + "/s" + std::to_string(v.shift);
            benchmark::RegisterBenchmark(
                name, [&v, &in](benchmark::State& state) { jumpdest_pointer(state, v, in); });
        }
    }
    return true;
}();
}  // namespace

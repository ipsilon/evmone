// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

// Benchmarks of the JUMPDEST analysis loop of lib/evmone/baseline_analysis.cpp: the index loop
// (index), the pointer walk of the ZisK Ethereum guest program, ziskethone patch 04 (zisk), the
// index loop with the index replaced by a pointer (ptr), and the zisk loop with signed byte loads
// (szisk). Each variant is compiled 4 times, with its code shifted by 0, 16, 32 and 48 bytes from
// a 64-byte boundary, so that no result depends on one alignment of the loop. Each benchmark
// checks its result against the index loop. All of that is compiled once per code layout mode
// (jumpdest_pointer_mode_*.cpp): the default, loops aligned to 64 bytes (+al), and that plus the
// assembler's mitigation of the Skylake JCC erratum (+alj). A mode the compiler cannot build is
// skipped; the jdp_modes context of the results lists the flags of each.
//
// Name: jumpdest_pointer/<input>/<variant><mode>/s<shift>.

#include "jumpdest_pointer_loops.hpp"
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

namespace jdp
{
namespace def
{
extern const Mode mode;
}
namespace al
{
extern const Mode mode;
}
namespace alj
{
extern const Mode mode;
}
}  // namespace jdp

namespace
{
using namespace jdp;

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
    const Mode* const modes[] = {&jdp::def::mode, &jdp::al::mode, &jdp::alj::mode};
    std::string modes_info;
    for (const auto* m : modes)
    {
        modes_info += std::string{m->name[0] != '\0' ? m->name : "def"} + "=" +
                      (m->flags != nullptr ? m->flags : "unsupported") + "; ";
    }
    benchmark::AddCustomContext("jdp_modes", modes_info);
    for (const auto& in : inputs)
    {
        for (const auto* m : modes)
        {
            if (m->flags == nullptr)
                continue;
            for (size_t i = 0; i < NUM_VARIANTS; ++i)
            {
                const auto& v = m->variants[i];
                const auto name = "jumpdest_pointer/" + in.name + "/" + v.name + m->name + "/s" +
                                  std::to_string(v.shift);
                benchmark::RegisterBenchmark(
                    name, [&v, &in](benchmark::State& state) { jumpdest_pointer(state, v, in); });
            }
        }
    }
    return true;
}();
}  // namespace

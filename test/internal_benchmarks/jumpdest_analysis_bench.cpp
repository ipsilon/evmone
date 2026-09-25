// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

// Benchmarks of JUMPDEST analysis of padded code: the default evmone loop, the speculative
// variant and the data-independent SIMD kernels. Each benchmark checks its result against the
// default loop before timing.
//
// Name: jumpdest_analysis/<input>/<variant>/<alignment>. The alignment is the offset of the code
// from a 64-byte boundary (a32: 0, a16: 16, a1: 1) and, for the SIMD kernels, the load
// instruction (_load: aligned loads, _loadu: unaligned loads).

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

/// Writes the JUMPDEST bitset of the code of the given size. The code is followed by at least
/// PADDING zero bytes; the bitset has (size + 64) / 64 zeroed words.
using AnalyzeFn = void (*)(const u8* code, size_t size, u64* bits);

/// The default evmone loop (lib/evmone/baseline_analysis.cpp).
void default_loop(const u8* code, size_t size, u64* bits)
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

/// speculate_push_data_size() from test/experimental/jumpdest_analysis.cpp.
void speculate_push_data_size(const u8* code, size_t size, u64* bits)
{
    for (size_t i = 0; i < size; ++i)
    {
        const auto op = code[i];
        const auto potential_push_data_len = op - size_t{0x5f};
        if (potential_push_data_len <= 32)
            i += potential_push_data_len;
        else if (__builtin_expect(op == 0x5b, 0))
            bits[i / 64] |= u64{1} << (i % 64);
    }
}
}  // namespace

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#define MCA_BEGIN(name) ((void)0)
#define MCA_END(name) ((void)0)
#if JDA_X86
namespace aligned_load
{
using u8 = uint8_t;
using u64 = uint64_t;
#define JDA_LOAD128(p) _mm_load_si128(reinterpret_cast<const __m128i*>(p))
#define JDA_LOAD256(p) _mm256_load_si256(reinterpret_cast<const __m256i*>(p))
#include "jumpdest/g8.hpp"

#include "jumpdest/vt.hpp"

#undef JDA_LOAD128
#undef JDA_LOAD256
}  // namespace aligned_load
namespace unaligned_load
{
using u8 = uint8_t;
using u64 = uint64_t;
#define JDA_LOAD128(p) _mm_loadu_si128(reinterpret_cast<const __m128i*>(p))
#define JDA_LOAD256(p) _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p))
#include "jumpdest/g8.hpp"

#undef JDA_LOAD128
#undef JDA_LOAD256
}  // namespace unaligned_load
#endif
#if JDA_NEON
namespace neon
{
#include "jumpdest/neon.hpp"
}  // namespace neon
#endif
#undef MCA_BEGIN
#undef MCA_END
#pragma GCC diagnostic pop

#if JDA_FROZEN_ASM
// Frozen clang-21 builds (jumpdest/frozen_asm.S): the same code for every compiler.
extern "C" void jda_asm_g8_sse(const uint8_t*, size_t, uint64_t*);
void vt2_sse(const uint8_t*, size_t, uint64_t*);
void vt2_avx2(const uint8_t*, size_t, uint64_t*);
extern "C" void jda_asm_g8_avx2(const uint8_t*, size_t, uint64_t*);
extern "C" void jda_asm_g9np_avx2_ofix(const uint8_t*, size_t, uint64_t*);
extern "C" void jda_asm_c8_avx2_fnew1(const uint8_t*, size_t, uint64_t*);
extern "C" void jda_asm_g9np_avx2_o41(const uint8_t*, size_t, uint64_t*);
#endif

namespace
{
enum class Kind
{
    scalar,       ///< Runs at every code offset.
    simd_load,    ///< Aligned loads: the aligned code only.
    simd_loadu,   ///< Unaligned loads: every code offset.
    simd_native,  ///< Loads with no alignment variants (NEON): every code offset.
};

struct Variant
{
    const char* name;
    AnalyzeFn fn;
    Kind kind;
    bool (*supported)();
};

bool always()
{
    return true;
}

#if JDA_X86
bool has_sse41()
{
    __builtin_cpu_init();  // Called from a static initializer: may run before the runtime's one.
    return __builtin_cpu_supports("ssse3") && __builtin_cpu_supports("sse4.1");
}
bool has_avx2()
{
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx2");
}
bool has_avx2_bmi2()
{
    return has_avx2() && __builtin_cpu_supports("bmi") && __builtin_cpu_supports("bmi2");
}
#endif

const Variant variants[] = {
    {"default", default_loop, Kind::scalar, always},
    {"speculate", speculate_push_data_size, Kind::scalar, always},
#if JDA_X86
// clang-format off
#define JDA_SIMD(name, fn, supported) \
    {name, aligned_load::fn, Kind::simd_load, supported}, \
    {name, unaligned_load::fn, Kind::simd_loadu, supported}
    // clang-format on
    JDA_SIMD("g8_sse", g8_sse, has_sse41),
    JDA_SIMD("g8_avx2", g8_avx2, has_avx2),
    {"vt_g8_sse", aligned_load::vt_g8_sse, Kind::simd_load, has_sse41},
    {"vt_g9np_avx2", aligned_load::vt_g9np_avx2, Kind::simd_load, has_avx2},
#undef JDA_SIMD
#endif
#if JDA_FROZEN_ASM
    {"asm_g8_sse", jda_asm_g8_sse, Kind::simd_load, has_sse41},
    {"vt2_sse", vt2_sse, Kind::simd_load, has_sse41},
    {"vt2_avx2", vt2_avx2, Kind::simd_load, has_avx2},
    {"asm_g8_avx2", jda_asm_g8_avx2, Kind::simd_load, has_avx2},
    {"asm_g9np_avx2_ofix", jda_asm_g9np_avx2_ofix, Kind::simd_load, has_avx2},
    {"asm_c8_avx2_fnew1", jda_asm_c8_avx2_fnew1, Kind::simd_load, has_avx2_bmi2},
    {"asm_g9np_avx2_o41", jda_asm_g9np_avx2_o41, Kind::simd_load, has_avx2},
#endif
#if JDA_NEON
    {"v4_neon", neon::g16v4_neon, Kind::simd_native, always},
#endif
};

constexpr size_t PADDING = 64;
constexpr size_t OFFSETS[] = {0, 16, 1};

/// The code copied at the given offset from a 64-byte boundary, followed by zero padding.
struct Buffer
{
    std::unique_ptr<u8[], decltype(&std::free)> storage{nullptr, &std::free};
    const u8* code = nullptr;

    Buffer(const std::vector<u8>& c, size_t offset)
    {
        const auto alloc_size = (offset + c.size() + PADDING + 63) / 64 * 64;
        storage.reset(static_cast<u8*>(std::aligned_alloc(64, alloc_size)));
        std::memset(storage.get(), 0, alloc_size);
        std::copy(c.begin(), c.end(), storage.get() + offset);
        code = storage.get() + offset;
    }
};

struct Input
{
    std::string name;
    size_t size = 0;
    std::vector<Buffer> buffers;  // One per OFFSETS entry.
    std::vector<u64> expected;

    Input(std::string n, const std::vector<u8>& code) : name{std::move(n)}, size{code.size()}
    {
        for (const auto offset : OFFSETS)
            buffers.emplace_back(code, offset);
        expected.resize(num_words());
        default_loop(buffers[0].code, size, expected.data());
    }

    [[nodiscard]] size_t num_words() const noexcept { return (size + 64) / 64; }
};

/// Random bytes over the alphabet with the given weights.
std::vector<u8> random_bytes(size_t size, std::vector<u8> alphabet, std::vector<int> weights)
{
    std::mt19937_64 rng{0};
    std::discrete_distribution<size_t> dist(weights.begin(), weights.end());
    std::vector<u8> code(size);
    for (auto& b : code)
        b = alphabet[dist(rng)];
    return code;
}

/// A random instruction stream: PUSHn with n in [min_width, max_width] whose data bytes are
/// drawn from `data`, JUMPDEST and STOP, with the weights push:jumpdest:stop.
std::vector<u8> random_instructions(size_t size, unsigned min_width, unsigned max_width,
    std::vector<u8> data, std::vector<int> weights)
{
    std::mt19937_64 rng{0};
    std::discrete_distribution<int> kind(weights.begin(), weights.end());
    std::uniform_int_distribution<unsigned> width(min_width, max_width);
    std::uniform_int_distribution<size_t> data_byte(0, data.size() - 1);
    std::vector<u8> code;
    code.reserve(size + 32);
    while (code.size() < size)
    {
        switch (kind(rng))
        {
        case 0:
        {
            const auto n = width(rng);
            code.push_back(static_cast<u8>(0x5f + n));
            for (unsigned j = 0; j < n; ++j)
                code.push_back(data[data_byte(rng)]);
            break;
        }
        case 1:
            code.push_back(0x5b);
            break;
        default:
            code.push_back(0x00);
            break;
        }
    }
    code.resize(size);
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
        inputs.emplace_back("real" + std::to_string(i) + "_" + std::to_string(code.size()),
            std::vector<u8>(code.begin(), code.end()));
    }

    // Synthetic inputs of the maximum initcode size (EIP-7954).
    constexpr size_t N = evmone::MAX_INITCODE_SIZE_AMSTERDAM;
    std::vector<u8> all(256);
    for (size_t i = 0; i < all.size(); ++i)
        all[i] = static_cast<u8>(i);

    // Periodic patterns.
    inputs.emplace_back("stop", tiled(N, {0x00}));
    inputs.emplace_back("jumpdest", tiled(N, {0x5b}));
    inputs.emplace_back("push1_jumpdest", tiled(N, {0x60, 0x5b}));
    inputs.emplace_back("jumpdest_push1", tiled(N, {0x5b, 0x60, 0x5b}));
    inputs.emplace_back("push1", tiled(N, {0x60}));
    inputs.emplace_back("push32", tiled(N, {0x7f}));

    // Random bytes: uniform, and the alphabets of the execution-specs jumpdest analysis benchmark.
    inputs.emplace_back("rand_bytes", random_bytes(N, all, std::vector<int>(256, 1)));
    inputs.emplace_back("rand_stop_jumpdest", random_bytes(N, {0x00, 0x5b}, {1, 1}));
    inputs.emplace_back("rand_stop_jumpdest_push1", random_bytes(N, {0x00, 0x5b, 0x60}, {1, 1, 1}));
    inputs.emplace_back("rand_jumpdest_push1", random_bytes(N, {0x5b, 0x60}, {1, 1}));
    inputs.emplace_back(
        "rand_stop_jumpdest_2push1", random_bytes(N, {0x00, 0x5b, 0x60}, {1, 1, 2}));

    // Random instruction streams with JUMPDEST and PUSH opcodes hidden in the push data.
    const std::vector<u8> tricky{0x5b, 0x60, 0x7f, 0x00};
    inputs.emplace_back("rand_push1to32", random_instructions(N, 1, 32, tricky, {2, 1, 1}));
    inputs.emplace_back("rand_push1to4", random_instructions(N, 1, 4, tricky, {2, 1, 1}));
    inputs.emplace_back("rand_push1_hidden", random_instructions(N, 1, 1, {0x5b, 0x60}, {1, 1, 0}));
    inputs.emplace_back("rand_push32_jumpdest", random_instructions(N, 32, 32, tricky, {1, 1, 0}));
    return inputs;
}

void set_counters(benchmark::State& state, size_t size)
{
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(size));
    // The initcode cost that pays for the analysis (EIP-3860).
    const auto gas = 2 * ((size + 31) / 32);
    state.counters["gas_rate"] =
        benchmark::Counter(static_cast<double>(gas), benchmark::Counter::kIsIterationInvariantRate);
}

void jumpdest_analysis(benchmark::State& state, const Variant& v, const Input& in, size_t buf)
{
    std::vector<u64> bits(in.num_words() + 1);  // One guard word.
    const auto* code = in.buffers[buf].code;

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
    set_counters(state, in.size);
}

[[maybe_unused]] const auto registered = [] {
    static const auto inputs = make_inputs();
    const auto min_statistic = [](const std::vector<double>& x) {
        return *std::min_element(x.begin(), x.end());
    };
    for (const auto& in : inputs)
    {
        for (const auto& v : variants)
        {
            if (!v.supported())
                continue;
            for (size_t buf = 0; buf < std::size(OFFSETS); ++buf)
            {
                const auto offset = OFFSETS[buf];
                auto alignment = offset == 0 ? std::string{"a32"} : "a" + std::to_string(offset);
                if (v.kind == Kind::simd_load)
                {
                    if (offset != 0)
                        continue;
                    alignment += "_load";
                }
                else if (v.kind == Kind::simd_loadu)
                    alignment += "_loadu";

                const auto name = "jumpdest_analysis/" + in.name + "/" + v.name + "/" + alignment;
                benchmark::RegisterBenchmark(name, [&v, &in, buf](benchmark::State& state) {
                    jumpdest_analysis(state, v, in, buf);
                })->ComputeStatistics("min", min_statistic);
            }
        }
    }
    return true;
}();
}  // namespace

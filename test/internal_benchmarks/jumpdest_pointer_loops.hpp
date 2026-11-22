// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

// The JUMPDEST analysis loops of jumpdest_pointer_bench.cpp.
#pragma once

#include <evmc/bytes.hpp>
#include <evmone/bitset_span.hpp>
#include <evmone/instructions_opcodes.hpp>
#include <cstddef>
#include <cstdint>
#include <limits>

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

namespace jdp
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

/// The type the tszisk and tsptr loops load the opcodes as. x86 extends a loaded byte either way at no
/// extra cost, but its sign-extending load is a byte longer and, on Zen 3/4 and Granite Rapids, a
/// cycle slower in the chain of PUSH skips. GCC for RISC-V and AArch64 folds the sign extension into
/// the load (lb, ldrsb) only when the byte is loaded as signed. Not plain char: it is unsigned on
/// AArch64 Linux and RISC-V. Accessing the unsigned char code as signed char is allowed: it is the
/// signed type corresponding to the type of the object.
#if defined(__x86_64__) || defined(_M_X64)
using OpcodeByte = unsigned char;
#else
using OpcodeByte = signed char;
#endif

/// szisk with the opcode type picked per target: one cast in the loop, a no-op for signed char.
[[gnu::always_inline]] inline void tszisk_loop(BitsetSpan map, bytes_view code) noexcept
{
    const auto* const begin = reinterpret_cast<const OpcodeByte*>(code.data());
    const auto* const end = begin + code.size();
    for (const auto* p = begin; p < end;)
    {
        const auto op = *p;
        if (static_cast<signed char>(op) >= OP_PUSH1)
            p += op - std::ptrdiff_t{OP_PUSH1 - 2};
        else
        {
            if (JDP_UNLIKELY(op == OP_JUMPDEST))
                map.set(static_cast<size_t>(p - begin));
            ++p;
        }
    }
}

/// The ptr loop (the index loop's shape) with the opcode type picked per target.
[[gnu::always_inline]] inline void tsptr_loop(BitsetSpan map, bytes_view code) noexcept
{
    const auto* const begin = reinterpret_cast<const OpcodeByte*>(code.data());
    const auto* const end = begin + code.size();
    for (const auto* p = begin; p < end; ++p)
    {
        const auto op = *p;
        if (static_cast<signed char>(op) >= OP_PUSH1)
            p += op - std::ptrdiff_t{OP_PUSH1 - 1};
        else if (JDP_UNLIKELY(op == OP_JUMPDEST))
            map.set(static_cast<size_t>(p - begin));
    }
}

using AnalyzeFn = void (*)(BitsetSpan, bytes_view) noexcept;

struct Variant
{
    const char* name;
    unsigned shift;
    AnalyzeFn fn;
};

/// 6 loops x 4 shifts.
constexpr size_t NUM_VARIANTS = 24;

/// The loops compiled with one set of code layout flags (one jumpdest_pointer_mode_*.cpp).
struct Mode
{
    const char* name;   ///< The suffix of the benchmark variant names, empty for the default.
    const char* flags;  ///< The compiler flags, or nullptr when the compiler does not support them.
    const Variant* variants;
};
}  // namespace jdp

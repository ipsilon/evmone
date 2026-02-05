// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2024 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "ripemd160.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <type_traits>
#include <utility>

#if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION < 180000
// libc++ before version 18 has incorrect std::rotl signature
// https://github.com/llvm/llvm-project/commit/45500fa08acdf3849de9de470cdee5f4c8ee2f32
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif

namespace evmone::crypto
{
namespace
{
// TODO(C++23): Use std::byteswap.
template <std::integral T>
constexpr T byteswap(T value) noexcept
{
    static_assert(std::has_unique_object_representations_v<T>, "T may not have padding bits");
    auto value_representation = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
    std::ranges::reverse(value_representation);
    return std::bit_cast<T>(value_representation);
}

/// @file
/// The implementation of the RIPEMD-160 hash function
/// based on the "RIPEMD-160: A Strengthened Version of RIPEMD"
/// https://homes.esat.kuleuven.be/~bosselae/ripemd160/pdf/AB-9601/AB-9601.pdf

constexpr size_t L = 2;      ///< Number of lines.
constexpr size_t R = 5;      ///< Number of rounds.
constexpr size_t B = 16;     ///< Number of steps per round and words in a block.
constexpr size_t N = R * B;  ///< Number of steps.

using State = std::array<uint32_t, RIPEMD160_HASH_SIZE / sizeof(uint32_t)>;

using BinaryFunction = uint32_t (*)(uint32_t, uint32_t, uint32_t) noexcept;

// TODO: Functions from the array of function pointers are not inlined by GCC:
//       https://gcc.gnu.org/bugzilla/show_bug.cgi?id=114452
// TODO(C++23): Mark these as [[always_inline]]
constexpr BinaryFunction binary_functions[R] = {
    // f₁(x, y, z) = x ⊕ y ⊕ z
    [](uint32_t x, uint32_t y, uint32_t z) noexcept { return x ^ y ^ z; },

    // f₂(x, y, z) = (x ∧ y) ∨ (¬x ∧ z) ⇔ ((y ⊕ z) ∧ x) ⊕ z
    [](uint32_t x, uint32_t y, uint32_t z) noexcept { return ((y ^ z) & x) ^ z; },

    // f₃(x, y, z) = (x ∨ ¬y) ⊕ z
    [](uint32_t x, uint32_t y, uint32_t z) noexcept { return (x | ~y) ^ z; },

    // f₄(x, y, z) = (x ∧ z) ∨ (y ∧ ¬z) ⇔ ((x ⊕ y) ∧ z) ⊕ y
    [](uint32_t x, uint32_t y, uint32_t z) noexcept { return ((x ^ y) & z) ^ y; },

    // f₅(x, y, z) = x ⊕ (y ∨ ¬z)
    [](uint32_t x, uint32_t y, uint32_t z) noexcept { return x ^ (y | ~z); },
};

/// Added constants.
constexpr uint32_t constants[L][R] = {
    {
        0,
        0x5a827999,
        0x6ed9eba1,
        0x8f1bbcdc,
        0xa953fd4e,
    },
    {
        0x50a28be6,
        0x5c4dd124,
        0x6d703ef3,
        0x7a6d76e9,
        0,
    },
};


/// Selection of message word.
constexpr size_t word_index[L][N] = {
    {
        /*r ( 0..15) = */ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,  //
        /*r (16..31) = */ 7, 4, 13, 1, 10, 6, 15, 3, 12, 0, 9, 5, 2, 14, 11, 8,  //
        /*r (32..47) = */ 3, 10, 14, 4, 9, 15, 8, 1, 2, 7, 0, 6, 13, 11, 5, 12,  //
        /*r (48..63) = */ 1, 9, 11, 10, 0, 8, 12, 4, 13, 3, 7, 15, 14, 5, 6, 2,  //
        /*r (64..79) = */ 4, 0, 5, 9, 7, 12, 2, 10, 14, 1, 3, 8, 11, 6, 15, 13   //
    },
    {
        /*r′( 0..15) = */ 5, 14, 7, 0, 9, 2, 11, 4, 13, 6, 15, 8, 1, 10, 3, 12,  //
        /*r′(16..31) = */ 6, 11, 3, 7, 0, 13, 5, 10, 14, 15, 8, 12, 4, 9, 1, 2,  //
        /*r′(32..47) = */ 15, 5, 1, 3, 7, 14, 6, 9, 11, 8, 12, 2, 10, 0, 4, 13,  //
        /*r′(48..63) = */ 8, 6, 4, 1, 3, 11, 15, 0, 5, 12, 2, 13, 9, 7, 10, 14,  //
        /*r′(64..79) = */ 12, 15, 10, 4, 1, 5, 8, 7, 6, 2, 13, 14, 0, 3, 9, 11   //
    },
};

/// Amount for rotate left.
constexpr int rotate_amount[L][N] = {
    {
        /* s ( 0..15) = */ 11, 14, 15, 12, 5, 8, 7, 9, 11, 13, 14, 15, 6, 7, 9, 8,  //
        /* s (16..31) = */ 7, 6, 8, 13, 11, 9, 7, 15, 7, 12, 15, 9, 11, 7, 13, 12,  //
        /* s (32..47) = */ 11, 13, 6, 7, 14, 9, 13, 15, 14, 8, 13, 6, 5, 12, 7, 5,  //
        /* s (48..63) = */ 11, 12, 14, 15, 14, 15, 9, 8, 9, 14, 5, 6, 8, 6, 5, 12,  //
        /* s (64..79) = */ 9, 15, 5, 11, 6, 8, 13, 12, 5, 12, 13, 14, 11, 8, 5, 6   //
    },
    {
        /* s′( 0..15) = */ 8, 9, 9, 11, 13, 15, 15, 5, 7, 7, 8, 11, 14, 14, 12, 6,  //
        /* s′(16..31) = */ 9, 13, 15, 7, 12, 8, 9, 11, 7, 7, 12, 7, 6, 15, 13, 11,  //
        /* s′(32..47) = */ 9, 7, 15, 11, 8, 6, 6, 14, 12, 13, 5, 14, 13, 13, 7, 5,  //
        /* s′(48..63) = */ 15, 5, 8, 11, 14, 14, 6, 14, 6, 9, 12, 9, 12, 5, 15, 8,  //
        /* s′(64..79) = */ 8, 5, 12, 9, 12, 5, 14, 6, 8, 13, 6, 5, 15, 13, 11, 11   //
    },
};

/// Converts native representation to/from little-endian.
inline auto to_le(std::integral auto x) noexcept
{
    if constexpr (std::endian::native == std::endian::big)
        return byteswap(x);
    return x;
}

template <typename T>
inline T load_le(const std::byte* data) noexcept
{
    std::array<std::byte, sizeof(T)> bytes{};
    std::copy_n(data, sizeof(T), bytes.begin());
    return to_le(std::bit_cast<T>(bytes));
}

inline std::byte* store_le(std::byte* out, std::integral auto x) noexcept
{
    return std::ranges::copy(std::bit_cast<std::array<std::byte, sizeof(x)>>(to_le(x)), out).out;
}

template <size_t J>
inline void step(State z[L], const std::byte* block) noexcept
{
    static constexpr auto I = J / B;  // round index
    static constexpr BinaryFunction fs[]{binary_functions[I], binary_functions[R - 1 - I]};

    for (size_t i = 0; i < L; ++i)
    {
        const auto f = fs[i];
        const auto w = load_le<uint32_t>(&block[sizeof(uint32_t) * word_index[i][J]]);
        const auto k = constants[i][I];
        const auto s = rotate_amount[i][J];

        const auto a = z[i][0];
        const auto b = z[i][1];
        const auto c = z[i][2];
        const auto d = z[i][3];
        const auto e = z[i][4];

        z[i][0] = e;
        z[i][1] = std::rotl(a + f(b, c, d) + w + k, s) + e;
        z[i][2] = b;
        z[i][3] = std::rotl(c, 10);
        z[i][4] = d;
    }
}


// TODO(C++23): This could be a lambda, but [[always_inline]] does not work.
// TODO: Try arguments instead of capture.
template <std::size_t... J>
[[gnu::always_inline]] inline void steps(
    State z[L], const std::byte* block, std::integer_sequence<std::size_t, J...>) noexcept
{
    (step<J>(z, block), ...);
}

void compress(State& h, const std::byte* block) noexcept
{
    State z[L]{h, h};
    steps(z, block, std::make_index_sequence<N>{});

    State t;
    for (size_t i = 0, M = t.size(); i < M; ++i)
        t[i] = h[(i + 1) % M] + z[0][(i + 2) % M] + z[1][(i + 3) % M];
    h = t;
}
}  // namespace

void ripemd160(std::byte hash[RIPEMD160_HASH_SIZE], const std::byte* data, size_t size) noexcept
{
    static constexpr size_t BLOCK_SIZE = B * sizeof(uint32_t);
    State h{0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0};

    const auto tail_size = size % BLOCK_SIZE;
    for (const auto tail_begin = &data[size - tail_size]; data != tail_begin; data += BLOCK_SIZE)
        compress(h, data);

    {
        std::array<std::byte, BLOCK_SIZE> padding_block{};
        const auto padded_tail_end = std::copy_n(data, tail_size, padding_block.data());
        *padded_tail_end = std::byte{0x80};  // The padding bit placed just after the input bytes.

        // Store the input length in bits in the last two words of the padded block.
        const auto length_in_bits = uint64_t{size} * 8;
        const auto length_begin = &padding_block[BLOCK_SIZE - sizeof(length_in_bits)];
        if (padded_tail_end >= length_begin)  // If not enough space, create one more block.
        {
            compress(h, padding_block.data());
            padding_block = {};
        }
        store_le(length_begin, length_in_bits);
        compress(h, padding_block.data());
    }

    for (const auto e : h)
        hash = store_le(hash, e);
}
}  // namespace evmone::crypto

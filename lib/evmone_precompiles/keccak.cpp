// ethash: C/C++ implementation of Ethash, the Ethereum Proof of Work algorithm.
// Copyright 2018 Pawel Bylica.
// SPDX-License-Identifier: Apache-2.0

#include "keccak.h"

// Provide __has_builtin macro if not defined.
#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#ifdef _MSC_VER
#define ALWAYS_INLINE __forceinline
#elif __has_cpp_attribute(gnu::always_inline)
#define ALWAYS_INLINE [[gnu::always_inline]]
#else
#define ALWAYS_INLINE
#endif

#if !__has_builtin(__builtin_memcpy) && !defined(__GNUC__)
#include <string.h>
#define __builtin_memcpy memcpy
#endif

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define to_le64(X) __builtin_bswap64(X)
#else
#define to_le64(X) X
#endif

namespace
{
/// Loads 64-bit integer from given memory location as little-endian number.
ALWAYS_INLINE inline uint64_t load_le(const uint8_t* data)
{
    /* memcpy is the best way of expressing the intention. Every compiler will
       optimize is to single load instruction if the target architecture
       supports unaligned memory access (GCC and clang even in O0).
       This is great trick because we are violating C/C++ memory alignment
       restrictions with no performance penalty. */
    uint64_t word = 0;
    __builtin_memcpy(&word, data, sizeof(word));
    return to_le64(word);
}

/// Rotates the bits of x left by the count value specified by s.
/// The s must be in range <0, 64> exclusively, otherwise the result is undefined.
inline uint64_t rol(uint64_t x, unsigned s)
{
    return (x << s) | (x >> (64 - s));
}

const uint64_t ROUND_CONSTANTS[24] = {  //
    0x0000000000000001, 0x0000000000008082, 0x800000000000808a, 0x8000000080008000,
    0x000000000000808b, 0x0000000080000001, 0x8000000080008081, 0x8000000000008009,
    0x000000000000008a, 0x0000000000000088, 0x0000000080008009, 0x000000008000000a,
    0x000000008000808b, 0x800000000000008b, 0x8000000000008089, 0x8000000000008003,
    0x8000000000008002, 0x8000000000000080, 0x000000000000800a, 0x800000008000000a,
    0x8000000080008081, 0x8000000000008080, 0x0000000080000001, 0x8000000080008008};


/// The Keccak-f[1600] function.
///
/// The implementation of the Keccak-f function with 1600-bit width of the permutation (b).
/// The size of the state is also 1600 bit what gives 25 64-bit words.
///
/// @param state  The state of 25 64-bit words on which the permutation is to be performed.
///
/// The implementation based on:
/// - "simple" implementation by Ronny Van Keer, included in "Reference and optimized code in C",
///   https://keccak.team/archives.html, CC0-1.0 / Public Domain.
ALWAYS_INLINE inline void keccakf1600_implementation(uint64_t state[25])
{
    auto Aba = state[0];
    auto Abe = state[1];
    auto Abi = state[2];
    auto Abo = state[3];
    auto Abu = state[4];
    auto Aga = state[5];
    auto Age = state[6];
    auto Agi = state[7];
    auto Ago = state[8];
    auto Agu = state[9];
    auto Aka = state[10];
    auto Ake = state[11];
    auto Aki = state[12];
    auto Ako = state[13];
    auto Aku = state[14];
    auto Ama = state[15];
    auto Ame = state[16];
    auto Ami = state[17];
    auto Amo = state[18];
    auto Amu = state[19];
    auto Asa = state[20];
    auto Ase = state[21];
    auto Asi = state[22];
    auto Aso = state[23];
    auto Asu = state[24];

    for (size_t n = 0; n < 24; n += 2)
    {
        // Round (n + 0): Axx -> Exx

        auto Ba = Aba ^ Aga ^ Aka ^ Ama ^ Asa;
        auto Be = Abe ^ Age ^ Ake ^ Ame ^ Ase;
        auto Bi = Abi ^ Agi ^ Aki ^ Ami ^ Asi;
        auto Bo = Abo ^ Ago ^ Ako ^ Amo ^ Aso;
        auto Bu = Abu ^ Agu ^ Aku ^ Amu ^ Asu;

        auto Da = Bu ^ rol(Be, 1);
        auto De = Ba ^ rol(Bi, 1);
        auto Di = Be ^ rol(Bo, 1);
        auto Do = Bi ^ rol(Bu, 1);
        auto Du = Bo ^ rol(Ba, 1);

        Ba = Aba ^ Da;
        Be = rol(Age ^ De, 44);
        Bi = rol(Aki ^ Di, 43);
        Bo = rol(Amo ^ Do, 21);
        Bu = rol(Asu ^ Du, 14);
        const auto Eba = Ba ^ (~Be & Bi) ^ ROUND_CONSTANTS[n];
        const auto Ebe = Be ^ (~Bi & Bo);
        const auto Ebi = Bi ^ (~Bo & Bu);
        const auto Ebo = Bo ^ (~Bu & Ba);
        const auto Ebu = Bu ^ (~Ba & Be);

        Ba = rol(Abo ^ Do, 28);
        Be = rol(Agu ^ Du, 20);
        Bi = rol(Aka ^ Da, 3);
        Bo = rol(Ame ^ De, 45);
        Bu = rol(Asi ^ Di, 61);
        const auto Ega = Ba ^ (~Be & Bi);
        const auto Ege = Be ^ (~Bi & Bo);
        const auto Egi = Bi ^ (~Bo & Bu);
        const auto Ego = Bo ^ (~Bu & Ba);
        const auto Egu = Bu ^ (~Ba & Be);

        Ba = rol(Abe ^ De, 1);
        Be = rol(Agi ^ Di, 6);
        Bi = rol(Ako ^ Do, 25);
        Bo = rol(Amu ^ Du, 8);
        Bu = rol(Asa ^ Da, 18);
        const auto Eka = Ba ^ (~Be & Bi);
        const auto Eke = Be ^ (~Bi & Bo);
        const auto Eki = Bi ^ (~Bo & Bu);
        const auto Eko = Bo ^ (~Bu & Ba);
        const auto Eku = Bu ^ (~Ba & Be);

        Ba = rol(Abu ^ Du, 27);
        Be = rol(Aga ^ Da, 36);
        Bi = rol(Ake ^ De, 10);
        Bo = rol(Ami ^ Di, 15);
        Bu = rol(Aso ^ Do, 56);
        const auto Ema = Ba ^ (~Be & Bi);
        const auto Eme = Be ^ (~Bi & Bo);
        const auto Emi = Bi ^ (~Bo & Bu);
        const auto Emo = Bo ^ (~Bu & Ba);
        const auto Emu = Bu ^ (~Ba & Be);

        Ba = rol(Abi ^ Di, 62);
        Be = rol(Ago ^ Do, 55);
        Bi = rol(Aku ^ Du, 39);
        Bo = rol(Ama ^ Da, 41);
        Bu = rol(Ase ^ De, 2);
        const auto Esa = Ba ^ (~Be & Bi);
        const auto Ese = Be ^ (~Bi & Bo);
        const auto Esi = Bi ^ (~Bo & Bu);
        const auto Eso = Bo ^ (~Bu & Ba);
        const auto Esu = Bu ^ (~Ba & Be);


        // Round (n + 1): Exx -> Axx

        Ba = Eba ^ Ega ^ Eka ^ Ema ^ Esa;
        Be = Ebe ^ Ege ^ Eke ^ Eme ^ Ese;
        Bi = Ebi ^ Egi ^ Eki ^ Emi ^ Esi;
        Bo = Ebo ^ Ego ^ Eko ^ Emo ^ Eso;
        Bu = Ebu ^ Egu ^ Eku ^ Emu ^ Esu;

        Da = Bu ^ rol(Be, 1);
        De = Ba ^ rol(Bi, 1);
        Di = Be ^ rol(Bo, 1);
        Do = Bi ^ rol(Bu, 1);
        Du = Bo ^ rol(Ba, 1);

        Ba = Eba ^ Da;
        Be = rol(Ege ^ De, 44);
        Bi = rol(Eki ^ Di, 43);
        Bo = rol(Emo ^ Do, 21);
        Bu = rol(Esu ^ Du, 14);
        Aba = Ba ^ (~Be & Bi) ^ ROUND_CONSTANTS[n + 1];
        Abe = Be ^ (~Bi & Bo);
        Abi = Bi ^ (~Bo & Bu);
        Abo = Bo ^ (~Bu & Ba);
        Abu = Bu ^ (~Ba & Be);

        Ba = rol(Ebo ^ Do, 28);
        Be = rol(Egu ^ Du, 20);
        Bi = rol(Eka ^ Da, 3);
        Bo = rol(Eme ^ De, 45);
        Bu = rol(Esi ^ Di, 61);
        Aga = Ba ^ (~Be & Bi);
        Age = Be ^ (~Bi & Bo);
        Agi = Bi ^ (~Bo & Bu);
        Ago = Bo ^ (~Bu & Ba);
        Agu = Bu ^ (~Ba & Be);

        Ba = rol(Ebe ^ De, 1);
        Be = rol(Egi ^ Di, 6);
        Bi = rol(Eko ^ Do, 25);
        Bo = rol(Emu ^ Du, 8);
        Bu = rol(Esa ^ Da, 18);
        Aka = Ba ^ (~Be & Bi);
        Ake = Be ^ (~Bi & Bo);
        Aki = Bi ^ (~Bo & Bu);
        Ako = Bo ^ (~Bu & Ba);
        Aku = Bu ^ (~Ba & Be);

        Ba = rol(Ebu ^ Du, 27);
        Be = rol(Ega ^ Da, 36);
        Bi = rol(Eke ^ De, 10);
        Bo = rol(Emi ^ Di, 15);
        Bu = rol(Eso ^ Do, 56);
        Ama = Ba ^ (~Be & Bi);
        Ame = Be ^ (~Bi & Bo);
        Ami = Bi ^ (~Bo & Bu);
        Amo = Bo ^ (~Bu & Ba);
        Amu = Bu ^ (~Ba & Be);

        Ba = rol(Ebi ^ Di, 62);
        Be = rol(Ego ^ Do, 55);
        Bi = rol(Eku ^ Du, 39);
        Bo = rol(Ema ^ Da, 41);
        Bu = rol(Ese ^ De, 2);
        Asa = Ba ^ (~Be & Bi);
        Ase = Be ^ (~Bi & Bo);
        Asi = Bi ^ (~Bo & Bu);
        Aso = Bo ^ (~Bu & Ba);
        Asu = Bu ^ (~Ba & Be);
    }

    state[0] = Aba;
    state[1] = Abe;
    state[2] = Abi;
    state[3] = Abo;
    state[4] = Abu;
    state[5] = Aga;
    state[6] = Age;
    state[7] = Agi;
    state[8] = Ago;
    state[9] = Agu;
    state[10] = Aka;
    state[11] = Ake;
    state[12] = Aki;
    state[13] = Ako;
    state[14] = Aku;
    state[15] = Ama;
    state[16] = Ame;
    state[17] = Ami;
    state[18] = Amo;
    state[19] = Amu;
    state[20] = Asa;
    state[21] = Ase;
    state[22] = Asi;
    state[23] = Aso;
    state[24] = Asu;
}

void keccakf1600_generic(uint64_t state[25])
{
    keccakf1600_implementation(state);
}

/// The pointer to the best Keccak-f[1600] function implementation,
/// selected during runtime initialization.
auto keccakf1600_best = keccakf1600_generic;


#if !defined(_MSC_VER) && defined(__x86_64__) && __has_cpp_attribute(gnu::target)
[[gnu::target("bmi,bmi2")]] void keccakf1600_bmi(uint64_t state[25])
{
    keccakf1600_implementation(state);
}

[[gnu::constructor]] void select_keccakf1600_implementation()
{
    // Init CPU information.
    // This is needed on macOS because of the bug: https://bugs.llvm.org/show_bug.cgi?id=48459.
    __builtin_cpu_init();

    // Check if both BMI and BMI2 are supported. Some CPUs like Intel E5-2697 v2 incorrectly
    // report BMI2 but not BMI being available.
    if (__builtin_cpu_supports("bmi") && __builtin_cpu_supports("bmi2"))
        keccakf1600_best = keccakf1600_bmi;
}
#endif


ALWAYS_INLINE inline void keccak(uint64_t* out, size_t bits, const uint8_t* data, size_t size)
{
    constexpr auto WORD_SIZE = sizeof(uint64_t);
    const auto hash_size = bits / 8;
    const auto block_size = (1600 - bits * 2) / 8;

    uint64_t state[25] = {};

    while (size >= block_size)
    {
        for (size_t i = 0; i < (block_size / WORD_SIZE); ++i)
        {
            state[i] ^= load_le(data);
            data += WORD_SIZE;
        }

        keccakf1600_best(state);

        size -= block_size;
    }

    auto* state_iter = state;

    while (size >= WORD_SIZE)
    {
        *state_iter ^= load_le(data);
        ++state_iter;
        data += WORD_SIZE;
        size -= WORD_SIZE;
    }

    // Absorb last 0–7 bytes of input + the padding byte.
    auto last_word = uint64_t{0x01} << (size * 8);
    for (size_t i = 0; i < size; ++i)
        last_word |= uint64_t{data[i]} << (i * 8);
    *state_iter ^= last_word;

    state[(block_size / WORD_SIZE) - 1] ^= 0x8000000000000000;  // Last block bit flip.

    keccakf1600_best(state);

    for (size_t i = 0; i < (hash_size / WORD_SIZE); ++i)
        out[i] = to_le64(state[i]);
}
}  // namespace

ethash_hash256 ethash_keccak256(const uint8_t* data, size_t size) noexcept
{
    ethash_hash256 hash = {};
    keccak(hash.word64s, 256, data, size);
    return hash;
}

// ethash: C/C++ implementation of Ethash, the Ethereum Proof of Work algorithm.
// Copyright 2018 Pawel Bylica.
// SPDX-License-Identifier: Apache-2.0

#include "keccak.h"
#include <bit>

// Provide __has_builtin macro if not defined.
#ifndef __has_builtin
#define __has_builtin(x) 0
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
[[gnu::always_inline]] inline uint64_t load_le(const uint8_t* data)
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

constexpr uint64_t ROUND_CONSTANTS[24] = {  //
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
[[gnu::always_inline]] inline void keccakf1600_implementation(uint64_t state[25])
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

    auto Ca = Aba ^ Aga ^ Aka ^ Ama ^ Asa;
    auto Ce = Abe ^ Age ^ Ake ^ Ame ^ Ase;
    auto Ci = Abi ^ Agi ^ Aki ^ Ami ^ Asi;
    auto Co = Abo ^ Ago ^ Ako ^ Amo ^ Aso;
    auto Cu = Abu ^ Agu ^ Aku ^ Amu ^ Asu;

    for (size_t n = 0; n < 24; n += 2)
    {
        // Round (n + 0): Axx -> Exx

        auto Da = Cu ^ std::rotl(Ce, 1);
        auto De = Ca ^ std::rotl(Ci, 1);
        auto Di = Ce ^ std::rotl(Co, 1);
        auto Do = Ci ^ std::rotl(Cu, 1);
        auto Du = Co ^ std::rotl(Ca, 1);

        auto Ba = Aba ^ Da;
        auto Be = std::rotl(Age ^ De, 44);
        auto Bi = std::rotl(Aki ^ Di, 43);
        auto Bo = std::rotl(Amo ^ Do, 21);
        auto Bu = std::rotl(Asu ^ Du, 14);
        const auto Eba = Ba ^ (~Be & Bi) ^ ROUND_CONSTANTS[n];
        Ca = Eba;
        const auto Ebe = Be ^ (~Bi & Bo);
        Ce = Ebe;
        const auto Ebi = Bi ^ (~Bo & Bu);
        Ci = Ebi;
        const auto Ebo = Bo ^ (~Bu & Ba);
        Co = Ebo;
        const auto Ebu = Bu ^ (~Ba & Be);
        Cu = Ebu;

        Ba = std::rotl(Abo ^ Do, 28);
        Be = std::rotl(Agu ^ Du, 20);
        Bi = std::rotl(Aka ^ Da, 3);
        Bo = std::rotl(Ame ^ De, 45);
        Bu = std::rotl(Asi ^ Di, 61);
        const auto Ega = Ba ^ (~Be & Bi);
        Ca ^= Ega;
        const auto Ege = Be ^ (~Bi & Bo);
        Ce ^= Ege;
        const auto Egi = Bi ^ (~Bo & Bu);
        Ci ^= Egi;
        const auto Ego = Bo ^ (~Bu & Ba);
        Co ^= Ego;
        const auto Egu = Bu ^ (~Ba & Be);
        Cu ^= Egu;

        Ba = std::rotl(Abe ^ De, 1);
        Be = std::rotl(Agi ^ Di, 6);
        Bi = std::rotl(Ako ^ Do, 25);
        Bo = std::rotl(Amu ^ Du, 8);
        Bu = std::rotl(Asa ^ Da, 18);
        const auto Eka = Ba ^ (~Be & Bi);
        Ca ^= Eka;
        const auto Eke = Be ^ (~Bi & Bo);
        Ce ^= Eke;
        const auto Eki = Bi ^ (~Bo & Bu);
        Ci ^= Eki;
        const auto Eko = Bo ^ (~Bu & Ba);
        Co ^= Eko;
        const auto Eku = Bu ^ (~Ba & Be);
        Cu ^= Eku;

        Ba = std::rotl(Abu ^ Du, 27);
        Be = std::rotl(Aga ^ Da, 36);
        Bi = std::rotl(Ake ^ De, 10);
        Bo = std::rotl(Ami ^ Di, 15);
        Bu = std::rotl(Aso ^ Do, 56);
        const auto Ema = Ba ^ (~Be & Bi);
        Ca ^= Ema;
        const auto Eme = Be ^ (~Bi & Bo);
        Ce ^= Eme;
        const auto Emi = Bi ^ (~Bo & Bu);
        Ci ^= Emi;
        const auto Emo = Bo ^ (~Bu & Ba);
        Co ^= Emo;
        const auto Emu = Bu ^ (~Ba & Be);
        Cu ^= Emu;

        Ba = std::rotl(Abi ^ Di, 62);
        Be = std::rotl(Ago ^ Do, 55);
        Bi = std::rotl(Aku ^ Du, 39);
        Bo = std::rotl(Ama ^ Da, 41);
        Bu = std::rotl(Ase ^ De, 2);
        const auto Esa = Ba ^ (~Be & Bi);
        Ca ^= Esa;
        const auto Ese = Be ^ (~Bi & Bo);
        Ce ^= Ese;
        const auto Esi = Bi ^ (~Bo & Bu);
        Ci ^= Esi;
        const auto Eso = Bo ^ (~Bu & Ba);
        Co ^= Eso;
        const auto Esu = Bu ^ (~Ba & Be);
        Cu ^= Esu;


        // Round (n + 1): Exx -> Axx

        Da = Cu ^ std::rotl(Ce, 1);
        De = Ca ^ std::rotl(Ci, 1);
        Di = Ce ^ std::rotl(Co, 1);
        Do = Ci ^ std::rotl(Cu, 1);
        Du = Co ^ std::rotl(Ca, 1);

        Ba = Eba ^ Da;
        Be = std::rotl(Ege ^ De, 44);
        Bi = std::rotl(Eki ^ Di, 43);
        Bo = std::rotl(Emo ^ Do, 21);
        Bu = std::rotl(Esu ^ Du, 14);
        Aba = Ba ^ (~Be & Bi) ^ ROUND_CONSTANTS[n + 1];
        Ca = Aba;
        Abe = Be ^ (~Bi & Bo);
        Ce = Abe;
        Abi = Bi ^ (~Bo & Bu);
        Ci = Abi;
        Abo = Bo ^ (~Bu & Ba);
        Co = Abo;
        Abu = Bu ^ (~Ba & Be);
        Cu = Abu;

        Ba = std::rotl(Ebo ^ Do, 28);
        Be = std::rotl(Egu ^ Du, 20);
        Bi = std::rotl(Eka ^ Da, 3);
        Bo = std::rotl(Eme ^ De, 45);
        Bu = std::rotl(Esi ^ Di, 61);
        Aga = Ba ^ (~Be & Bi);
        Ca ^= Aga;
        Age = Be ^ (~Bi & Bo);
        Ce ^= Age;
        Agi = Bi ^ (~Bo & Bu);
        Ci ^= Agi;
        Ago = Bo ^ (~Bu & Ba);
        Co ^= Ago;
        Agu = Bu ^ (~Ba & Be);
        Cu ^= Agu;

        Ba = std::rotl(Ebe ^ De, 1);
        Be = std::rotl(Egi ^ Di, 6);
        Bi = std::rotl(Eko ^ Do, 25);
        Bo = std::rotl(Emu ^ Du, 8);
        Bu = std::rotl(Esa ^ Da, 18);
        Aka = Ba ^ (~Be & Bi);
        Ca ^= Aka;
        Ake = Be ^ (~Bi & Bo);
        Ce ^= Ake;
        Aki = Bi ^ (~Bo & Bu);
        Ci ^= Aki;
        Ako = Bo ^ (~Bu & Ba);
        Co ^= Ako;
        Aku = Bu ^ (~Ba & Be);
        Cu ^= Aku;

        Ba = std::rotl(Ebu ^ Du, 27);
        Be = std::rotl(Ega ^ Da, 36);
        Bi = std::rotl(Eke ^ De, 10);
        Bo = std::rotl(Emi ^ Di, 15);
        Bu = std::rotl(Eso ^ Do, 56);
        Ama = Ba ^ (~Be & Bi);
        Ca ^= Ama;
        Ame = Be ^ (~Bi & Bo);
        Ce ^= Ame;
        Ami = Bi ^ (~Bo & Bu);
        Ci ^= Ami;
        Amo = Bo ^ (~Bu & Ba);
        Co ^= Amo;
        Amu = Bu ^ (~Ba & Be);
        Cu ^= Amu;

        Ba = std::rotl(Ebi ^ Di, 62);
        Be = std::rotl(Ego ^ Do, 55);
        Bi = std::rotl(Eku ^ Du, 39);
        Bo = std::rotl(Ema ^ Da, 41);
        Bu = std::rotl(Ese ^ De, 2);
        Asa = Ba ^ (~Be & Bi);
        Ca ^= Asa;
        Ase = Be ^ (~Bi & Bo);
        Ce ^= Ase;
        Asi = Bi ^ (~Bo & Bu);
        Ci ^= Asi;
        Aso = Bo ^ (~Bu & Ba);
        Co ^= Aso;
        Asu = Bu ^ (~Ba & Be);
        Cu ^= Asu;
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


[[gnu::always_inline]] inline void keccak(
    uint64_t* out, size_t bits, const uint8_t* data, size_t size)
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

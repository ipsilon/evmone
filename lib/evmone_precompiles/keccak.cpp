// ethash: C/C++ implementation of Ethash, the Ethereum Proof of Work algorithm.
// Copyright 2018 Pawel Bylica.
// SPDX-License-Identifier: Apache-2.0

#include "keccak.h"
#include "int_bytes.hpp"
#include <bit>

#if defined(__APPLE__) && defined(__ARM_FEATURE_SHA3)
#include <arm_neon.h>
#endif

namespace
{
using evmone::crypto::load_le;
using evmone::crypto::to_le;

constexpr uint64_t ROUND_CONSTANTS[24] = {  //
    0x0000000000000001, 0x0000000000008082, 0x800000000000808a, 0x8000000080008000,
    0x000000000000808b, 0x0000000080000001, 0x8000000080008081, 0x8000000000008009,
    0x000000000000008a, 0x0000000000000088, 0x0000000080008009, 0x000000008000000a,
    0x000000008000808b, 0x800000000000008b, 0x8000000000008089, 0x8000000000008003,
    0x8000000000008002, 0x8000000000000080, 0x000000000000800a, 0x800000008000000a,
    0x8000000080008081, 0x8000000000008080, 0x0000000080000001, 0x8000000080008008};


#if defined(__APPLE__) && defined(__ARM_FEATURE_SHA3)
/// The Keccak-f[1600] implementation using the Armv8.2 SHA3 instructions,
/// enabled by default by Clang for arm64 macOS.
/// Neoverse N2, V1 and V2 execute the SHA3 instructions in a single pipeline,
/// where this is ~2x slower than the scalar implementation; newer cores are
/// at best on par.
[[gnu::noinline]] void keccakf1600_sha3(uint64_t state[25])
{
    // Each lane occupies the low half of a vector register. Theta uses EOR3 and RAX1,
    // rho and pi are combined into XAR, chi is BCAX. The statement order follows
    // the XKCP implementation by Hanno Becker and Matthias Kannwischer, so that
    // the compiler needs no register copies. Iota is applied after chi and Aba goes
    // to the second EOR3 of theta to keep iota off the critical path.
    uint64x2_t Aba = {state[0]};
    uint64x2_t Abe = {state[1]};
    uint64x2_t Abi = {state[2]};
    uint64x2_t Abo = {state[3]};
    uint64x2_t Abu = {state[4]};
    uint64x2_t Aga = {state[5]};
    uint64x2_t Age = {state[6]};
    uint64x2_t Agi = {state[7]};
    uint64x2_t Ago = {state[8]};
    uint64x2_t Agu = {state[9]};
    uint64x2_t Aka = {state[10]};
    uint64x2_t Ake = {state[11]};
    uint64x2_t Aki = {state[12]};
    uint64x2_t Ako = {state[13]};
    uint64x2_t Aku = {state[14]};
    uint64x2_t Ama = {state[15]};
    uint64x2_t Ame = {state[16]};
    uint64x2_t Ami = {state[17]};
    uint64x2_t Amo = {state[18]};
    uint64x2_t Amu = {state[19]};
    uint64x2_t Asa = {state[20]};
    uint64x2_t Ase = {state[21]};
    uint64x2_t Asi = {state[22]};
    uint64x2_t Aso = {state[23]};
    uint64x2_t Asu = {state[24]};

    for (const auto rc : ROUND_CONSTANTS)
    {
        const auto Ca = veor3q_u64(veor3q_u64(Aga, Aka, Ama), Asa, Aba);
        const auto Ce = veor3q_u64(veor3q_u64(Abe, Age, Ake), Ame, Ase);
        const auto Ci = veor3q_u64(veor3q_u64(Abi, Agi, Aki), Ami, Asi);
        const auto Co = veor3q_u64(veor3q_u64(Abo, Ago, Ako), Amo, Aso);
        const auto Cu = veor3q_u64(veor3q_u64(Abu, Agu, Aku), Amu, Asu);
        const auto Da = vrax1q_u64(Cu, Ce);
        const auto De = vrax1q_u64(Ca, Ci);
        const auto Di = vrax1q_u64(Ce, Co);
        const auto Do = vrax1q_u64(Ci, Cu);
        const auto Du = vrax1q_u64(Co, Ca);
        const auto Bba = veorq_u64(Aba, Da);
        const auto Bsa = vxarq_u64(Abi, Di, 2);
        const auto Bbi = vxarq_u64(Aki, Di, 21);
        const auto Bki = vxarq_u64(Ako, Do, 39);
        const auto Bko = vxarq_u64(Amu, Du, 56);
        const auto Bmu = vxarq_u64(Aso, Do, 8);
        const auto Bso = vxarq_u64(Ama, Da, 23);
        const auto Bka = vxarq_u64(Abe, De, 63);
        const auto Bse = vxarq_u64(Ago, Do, 9);
        const auto Bgo = vxarq_u64(Ame, De, 19);
        const auto Bke = vxarq_u64(Agi, Di, 58);
        const auto Bgi = vxarq_u64(Aka, Da, 61);
        const auto Bga = vxarq_u64(Abo, Do, 36);
        const auto Bbo = vxarq_u64(Amo, Do, 43);
        const auto Bmo = vxarq_u64(Ami, Di, 49);
        const auto Bmi = vxarq_u64(Ake, De, 54);
        const auto Bge = vxarq_u64(Agu, Du, 44);
        const auto Bgu = vxarq_u64(Asi, Di, 3);
        const auto Bsi = vxarq_u64(Aku, Du, 25);
        const auto Bku = vxarq_u64(Asa, Da, 46);
        const auto Bma = vxarq_u64(Abu, Du, 37);
        const auto Bbu = vxarq_u64(Asu, Du, 50);
        const auto Bsu = vxarq_u64(Ase, De, 62);
        const auto Bme = vxarq_u64(Aga, Da, 28);
        const auto Bbe = vxarq_u64(Age, De, 20);
        Aba = vbcaxq_u64(Bba, Bbi, Bbe);
        Abe = vbcaxq_u64(Bbe, Bbo, Bbi);
        Abi = vbcaxq_u64(Bbi, Bbu, Bbo);
        Abo = vbcaxq_u64(Bbo, Bba, Bbu);
        Abu = vbcaxq_u64(Bbu, Bbe, Bba);
        Aga = vbcaxq_u64(Bga, Bgi, Bge);
        Age = vbcaxq_u64(Bge, Bgo, Bgi);
        Agi = vbcaxq_u64(Bgi, Bgu, Bgo);
        Ago = vbcaxq_u64(Bgo, Bga, Bgu);
        Agu = vbcaxq_u64(Bgu, Bge, Bga);
        Aka = vbcaxq_u64(Bka, Bki, Bke);
        Ake = vbcaxq_u64(Bke, Bko, Bki);
        Aki = vbcaxq_u64(Bki, Bku, Bko);
        Ako = vbcaxq_u64(Bko, Bka, Bku);
        Aku = vbcaxq_u64(Bku, Bke, Bka);
        Ama = vbcaxq_u64(Bma, Bmi, Bme);
        Ame = vbcaxq_u64(Bme, Bmo, Bmi);
        Ami = vbcaxq_u64(Bmi, Bmu, Bmo);
        Amo = vbcaxq_u64(Bmo, Bma, Bmu);
        Amu = vbcaxq_u64(Bmu, Bme, Bma);
        Asa = vbcaxq_u64(Bsa, Bsi, Bse);
        Ase = vbcaxq_u64(Bse, Bso, Bsi);
        Asi = vbcaxq_u64(Bsi, Bsu, Bso);
        Aso = vbcaxq_u64(Bso, Bsa, Bsu);
        Asu = vbcaxq_u64(Bsu, Bse, Bsa);
        Aba = veorq_u64(Aba, uint64x2_t{rc});
    }

    state[0] = vgetq_lane_u64(Aba, 0);
    state[1] = vgetq_lane_u64(Abe, 0);
    state[2] = vgetq_lane_u64(Abi, 0);
    state[3] = vgetq_lane_u64(Abo, 0);
    state[4] = vgetq_lane_u64(Abu, 0);
    state[5] = vgetq_lane_u64(Aga, 0);
    state[6] = vgetq_lane_u64(Age, 0);
    state[7] = vgetq_lane_u64(Agi, 0);
    state[8] = vgetq_lane_u64(Ago, 0);
    state[9] = vgetq_lane_u64(Agu, 0);
    state[10] = vgetq_lane_u64(Aka, 0);
    state[11] = vgetq_lane_u64(Ake, 0);
    state[12] = vgetq_lane_u64(Aki, 0);
    state[13] = vgetq_lane_u64(Ako, 0);
    state[14] = vgetq_lane_u64(Aku, 0);
    state[15] = vgetq_lane_u64(Ama, 0);
    state[16] = vgetq_lane_u64(Ame, 0);
    state[17] = vgetq_lane_u64(Ami, 0);
    state[18] = vgetq_lane_u64(Amo, 0);
    state[19] = vgetq_lane_u64(Amu, 0);
    state[20] = vgetq_lane_u64(Asa, 0);
    state[21] = vgetq_lane_u64(Ase, 0);
    state[22] = vgetq_lane_u64(Asi, 0);
    state[23] = vgetq_lane_u64(Aso, 0);
    state[24] = vgetq_lane_u64(Asu, 0);
}

/// The pointer to the best Keccak-f[1600] function implementation.
constexpr auto keccakf1600_best = keccakf1600_sha3;
#else
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


#if !defined(_MSC_VER) && defined(__x86_64__) && __has_cpp_attribute(gnu::target)
/// The pointer to the best Keccak-f[1600] function implementation,
/// selected during runtime initialization.
auto keccakf1600_best = keccakf1600_generic;

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
#else
/// The pointer to the Keccak-f[1600] function implementation.
constexpr auto keccakf1600_best = keccakf1600_generic;
#endif
#endif


[[gnu::always_inline]] inline void keccak(
    uint64_t* out, size_t bits, const std::byte* data, size_t size)
{
    constexpr auto WORD_SIZE = sizeof(uint64_t);
    const auto hash_size = bits / 8;
    const auto block_size = (1600 - bits * 2) / 8;

    uint64_t state[25] = {};

    while (size >= block_size)
    {
        for (size_t i = 0; i < (block_size / WORD_SIZE); ++i)
        {
            state[i] ^= load_le<uint64_t>(data);
            data += WORD_SIZE;
        }

        keccakf1600_best(state);

        size -= block_size;
    }

    auto* state_iter = state;

    while (size >= WORD_SIZE)
    {
        *state_iter ^= load_le<uint64_t>(data);
        ++state_iter;
        data += WORD_SIZE;
        size -= WORD_SIZE;
    }

    // Absorb last 0–7 bytes of input + the padding byte.
    auto last_word = uint64_t{0x01} << (size * 8);
    for (size_t i = 0; i < size; ++i)
        last_word |= std::to_integer<uint64_t>(data[i]) << (i * 8);
    *state_iter ^= last_word;

    state[(block_size / WORD_SIZE) - 1] ^= 0x8000000000000000;  // Last block bit flip.

    keccakf1600_best(state);

    for (size_t i = 0; i < (hash_size / WORD_SIZE); ++i)
        out[i] = to_le(state[i]);
}
}  // namespace

ethash_hash256 ethash_keccak256(const uint8_t* data, size_t size) noexcept
{
    ethash_hash256 hash = {};
    keccak(hash.word64s, 256, reinterpret_cast<const std::byte*>(data), size);
    return hash;
}

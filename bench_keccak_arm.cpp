// Standalone Keccak-f[1600] AArch64 benchmark: scalar vs ARMv8.2 SHA3 variants.
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <arm_neon.h>
#if defined(__APPLE__)
#include <pthread.h>
#include <sys/sysctl.h>
#else
#include <asm/hwcap.h>
#include <sys/auxv.h>
#endif

#if defined(__ARM_FEATURE_SHA3)
#define SHA3_TARGET
#else
#define SHA3_TARGET [[gnu::target("+sha3")]]
#endif

namespace {
inline uint64_t rol(uint64_t x, unsigned s) { return (x << s) | (x >> (64 - s)); }
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

        auto Da = Cu ^ rol(Ce, 1);
        auto De = Ca ^ rol(Ci, 1);
        auto Di = Ce ^ rol(Co, 1);
        auto Do = Ci ^ rol(Cu, 1);
        auto Du = Co ^ rol(Ca, 1);

        auto Ba = Aba ^ Da;
        auto Be = rol(Age ^ De, 44);
        auto Bi = rol(Aki ^ Di, 43);
        auto Bo = rol(Amo ^ Do, 21);
        auto Bu = rol(Asu ^ Du, 14);
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

        Ba = rol(Abo ^ Do, 28);
        Be = rol(Agu ^ Du, 20);
        Bi = rol(Aka ^ Da, 3);
        Bo = rol(Ame ^ De, 45);
        Bu = rol(Asi ^ Di, 61);
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

        Ba = rol(Abe ^ De, 1);
        Be = rol(Agi ^ Di, 6);
        Bi = rol(Ako ^ Do, 25);
        Bo = rol(Amu ^ Du, 8);
        Bu = rol(Asa ^ Da, 18);
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

        Ba = rol(Abu ^ Du, 27);
        Be = rol(Aga ^ Da, 36);
        Bi = rol(Ake ^ De, 10);
        Bo = rol(Ami ^ Di, 15);
        Bu = rol(Aso ^ Do, 56);
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

        Ba = rol(Abi ^ Di, 62);
        Be = rol(Ago ^ Do, 55);
        Bi = rol(Aku ^ Du, 39);
        Bo = rol(Ama ^ Da, 41);
        Bu = rol(Ase ^ De, 2);
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

        Da = Cu ^ rol(Ce, 1);
        De = Ca ^ rol(Ci, 1);
        Di = Ce ^ rol(Co, 1);
        Do = Ci ^ rol(Cu, 1);
        Du = Co ^ rol(Ca, 1);

        Ba = Eba ^ Da;
        Be = rol(Ege ^ De, 44);
        Bi = rol(Eki ^ Di, 43);
        Bo = rol(Emo ^ Do, 21);
        Bu = rol(Esu ^ Du, 14);
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

        Ba = rol(Ebo ^ Do, 28);
        Be = rol(Egu ^ Du, 20);
        Bi = rol(Eka ^ Da, 3);
        Bo = rol(Eme ^ De, 45);
        Bu = rol(Esi ^ Di, 61);
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

        Ba = rol(Ebe ^ De, 1);
        Be = rol(Egi ^ Di, 6);
        Bi = rol(Eko ^ Do, 25);
        Bo = rol(Emu ^ Du, 8);
        Bu = rol(Esa ^ Da, 18);
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

        Ba = rol(Ebu ^ Du, 27);
        Be = rol(Ega ^ Da, 36);
        Bi = rol(Eke ^ De, 10);
        Bo = rol(Emi ^ Di, 15);
        Bu = rol(Eso ^ Do, 56);
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

        Ba = rol(Ebi ^ Di, 62);
        Be = rol(Ego ^ Do, 55);
        Bi = rol(Eku ^ Du, 39);
        Bo = rol(Ema ^ Da, 41);
        Bu = rol(Ese ^ De, 2);
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


}  // namespace

[[gnu::noinline]] void perm_scalar(uint64_t state[25]) { keccakf1600_implementation(state); }

SHA3_TARGET void perm_sha3_u1(uint64_t state[25])
{
    auto Aba = vld1q_dup_u64(&state[0]); auto Abe = vld1q_dup_u64(&state[1]); auto Abi = vld1q_dup_u64(&state[2]); auto Abo = vld1q_dup_u64(&state[3]); auto Abu = vld1q_dup_u64(&state[4]); auto Aga = vld1q_dup_u64(&state[5]); auto Age = vld1q_dup_u64(&state[6]); auto Agi = vld1q_dup_u64(&state[7]); auto Ago = vld1q_dup_u64(&state[8]); auto Agu = vld1q_dup_u64(&state[9]); auto Aka = vld1q_dup_u64(&state[10]); auto Ake = vld1q_dup_u64(&state[11]); auto Aki = vld1q_dup_u64(&state[12]); auto Ako = vld1q_dup_u64(&state[13]); auto Aku = vld1q_dup_u64(&state[14]); auto Ama = vld1q_dup_u64(&state[15]); auto Ame = vld1q_dup_u64(&state[16]); auto Ami = vld1q_dup_u64(&state[17]); auto Amo = vld1q_dup_u64(&state[18]); auto Amu = vld1q_dup_u64(&state[19]); auto Asa = vld1q_dup_u64(&state[20]); auto Ase = vld1q_dup_u64(&state[21]); auto Asi = vld1q_dup_u64(&state[22]); auto Aso = vld1q_dup_u64(&state[23]); auto Asu = vld1q_dup_u64(&state[24]);
    
    for (size_t n = 0; n < 24; ++n)
    {
        const auto Ca = veor3q_u64(veor3q_u64(Aba, Aga, Aka), Ama, Asa);
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
        const auto Bba_rc = veor3q_u64(Aba, Da, vld1q_dup_u64(&ROUND_CONSTANTS[n]));
        const auto Bbe = vxarq_u64(Age, De, 20);
        const auto Bbi = vxarq_u64(Aki, Di, 21);
        const auto Bbo = vxarq_u64(Amo, Do, 43);
        const auto Bbu = vxarq_u64(Asu, Du, 50);
        const auto Bga = vxarq_u64(Abo, Do, 36);
        const auto Bge = vxarq_u64(Agu, Du, 44);
        const auto Bgi = vxarq_u64(Aka, Da, 61);
        const auto Bgo = vxarq_u64(Ame, De, 19);
        const auto Bgu = vxarq_u64(Asi, Di, 3);
        const auto Bka = vxarq_u64(Abe, De, 63);
        const auto Bke = vxarq_u64(Agi, Di, 58);
        const auto Bki = vxarq_u64(Ako, Do, 39);
        const auto Bko = vxarq_u64(Amu, Du, 56);
        const auto Bku = vxarq_u64(Asa, Da, 46);
        const auto Bma = vxarq_u64(Abu, Du, 37);
        const auto Bme = vxarq_u64(Aga, Da, 28);
        const auto Bmi = vxarq_u64(Ake, De, 54);
        const auto Bmo = vxarq_u64(Ami, Di, 49);
        const auto Bmu = vxarq_u64(Aso, Do, 8);
        const auto Bsa = vxarq_u64(Abi, Di, 2);
        const auto Bse = vxarq_u64(Ago, Do, 9);
        const auto Bsi = vxarq_u64(Aku, Du, 25);
        const auto Bso = vxarq_u64(Ama, Da, 23);
        const auto Bsu = vxarq_u64(Ase, De, 62);
        Aba = vbcaxq_u64(Bba_rc, Bbi, Bbe);
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
    }
    vst1q_lane_u64(&state[0], Aba, 0); vst1q_lane_u64(&state[1], Abe, 0); vst1q_lane_u64(&state[2], Abi, 0); vst1q_lane_u64(&state[3], Abo, 0); vst1q_lane_u64(&state[4], Abu, 0); vst1q_lane_u64(&state[5], Aga, 0); vst1q_lane_u64(&state[6], Age, 0); vst1q_lane_u64(&state[7], Agi, 0); vst1q_lane_u64(&state[8], Ago, 0); vst1q_lane_u64(&state[9], Agu, 0); vst1q_lane_u64(&state[10], Aka, 0); vst1q_lane_u64(&state[11], Ake, 0); vst1q_lane_u64(&state[12], Aki, 0); vst1q_lane_u64(&state[13], Ako, 0); vst1q_lane_u64(&state[14], Aku, 0); vst1q_lane_u64(&state[15], Ama, 0); vst1q_lane_u64(&state[16], Ame, 0); vst1q_lane_u64(&state[17], Ami, 0); vst1q_lane_u64(&state[18], Amo, 0); vst1q_lane_u64(&state[19], Amu, 0); vst1q_lane_u64(&state[20], Asa, 0); vst1q_lane_u64(&state[21], Ase, 0); vst1q_lane_u64(&state[22], Asi, 0); vst1q_lane_u64(&state[23], Aso, 0); vst1q_lane_u64(&state[24], Asu, 0);
}

SHA3_TARGET void perm_sha3_x1(uint64_t state[25])
{
    auto Aba = vld1q_dup_u64(&state[0]); auto Abe = vld1q_dup_u64(&state[1]); auto Abi = vld1q_dup_u64(&state[2]); auto Abo = vld1q_dup_u64(&state[3]); auto Abu = vld1q_dup_u64(&state[4]); auto Aga = vld1q_dup_u64(&state[5]); auto Age = vld1q_dup_u64(&state[6]); auto Agi = vld1q_dup_u64(&state[7]); auto Ago = vld1q_dup_u64(&state[8]); auto Agu = vld1q_dup_u64(&state[9]); auto Aka = vld1q_dup_u64(&state[10]); auto Ake = vld1q_dup_u64(&state[11]); auto Aki = vld1q_dup_u64(&state[12]); auto Ako = vld1q_dup_u64(&state[13]); auto Aku = vld1q_dup_u64(&state[14]); auto Ama = vld1q_dup_u64(&state[15]); auto Ame = vld1q_dup_u64(&state[16]); auto Ami = vld1q_dup_u64(&state[17]); auto Amo = vld1q_dup_u64(&state[18]); auto Amu = vld1q_dup_u64(&state[19]); auto Asa = vld1q_dup_u64(&state[20]); auto Ase = vld1q_dup_u64(&state[21]); auto Asi = vld1q_dup_u64(&state[22]); auto Aso = vld1q_dup_u64(&state[23]); auto Asu = vld1q_dup_u64(&state[24]);
    
    for (size_t n = 0; n < 24; ++n)
    {
        const auto Ca = veor3q_u64(veor3q_u64(Aba, Aga, Aka), Ama, Asa);
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
        const auto Bba_rc = veorq_u64(Bba, vld1q_dup_u64(&ROUND_CONSTANTS[n]));
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
        Aba = vbcaxq_u64(Bba_rc, Bbi, Bbe);
        Abe = vbcaxq_u64(Bbe, Bbo, Bbi);
        Abi = vbcaxq_u64(Bbi, Bbu, Bbo);
        Abo = vbcaxq_u64(Bbo, Bba, Bbu);
        Abu = vbcaxq_u64(Bbu, Bbe, Bba);
    }
    vst1q_lane_u64(&state[0], Aba, 0); vst1q_lane_u64(&state[1], Abe, 0); vst1q_lane_u64(&state[2], Abi, 0); vst1q_lane_u64(&state[3], Abo, 0); vst1q_lane_u64(&state[4], Abu, 0); vst1q_lane_u64(&state[5], Aga, 0); vst1q_lane_u64(&state[6], Age, 0); vst1q_lane_u64(&state[7], Agi, 0); vst1q_lane_u64(&state[8], Ago, 0); vst1q_lane_u64(&state[9], Agu, 0); vst1q_lane_u64(&state[10], Aka, 0); vst1q_lane_u64(&state[11], Ake, 0); vst1q_lane_u64(&state[12], Aki, 0); vst1q_lane_u64(&state[13], Ako, 0); vst1q_lane_u64(&state[14], Aku, 0); vst1q_lane_u64(&state[15], Ama, 0); vst1q_lane_u64(&state[16], Ame, 0); vst1q_lane_u64(&state[17], Ami, 0); vst1q_lane_u64(&state[18], Amo, 0); vst1q_lane_u64(&state[19], Amu, 0); vst1q_lane_u64(&state[20], Asa, 0); vst1q_lane_u64(&state[21], Ase, 0); vst1q_lane_u64(&state[22], Asi, 0); vst1q_lane_u64(&state[23], Aso, 0); vst1q_lane_u64(&state[24], Asu, 0);
}

SHA3_TARGET void perm_sha3_u2(uint64_t state[25])
{
    auto Aba = vld1q_dup_u64(&state[0]); auto Abe = vld1q_dup_u64(&state[1]); auto Abi = vld1q_dup_u64(&state[2]); auto Abo = vld1q_dup_u64(&state[3]); auto Abu = vld1q_dup_u64(&state[4]); auto Aga = vld1q_dup_u64(&state[5]); auto Age = vld1q_dup_u64(&state[6]); auto Agi = vld1q_dup_u64(&state[7]); auto Ago = vld1q_dup_u64(&state[8]); auto Agu = vld1q_dup_u64(&state[9]); auto Aka = vld1q_dup_u64(&state[10]); auto Ake = vld1q_dup_u64(&state[11]); auto Aki = vld1q_dup_u64(&state[12]); auto Ako = vld1q_dup_u64(&state[13]); auto Aku = vld1q_dup_u64(&state[14]); auto Ama = vld1q_dup_u64(&state[15]); auto Ame = vld1q_dup_u64(&state[16]); auto Ami = vld1q_dup_u64(&state[17]); auto Amo = vld1q_dup_u64(&state[18]); auto Amu = vld1q_dup_u64(&state[19]); auto Asa = vld1q_dup_u64(&state[20]); auto Ase = vld1q_dup_u64(&state[21]); auto Asi = vld1q_dup_u64(&state[22]); auto Aso = vld1q_dup_u64(&state[23]); auto Asu = vld1q_dup_u64(&state[24]);
    _Pragma("GCC unroll 2")
    for (size_t n = 0; n < 24; ++n)
    {
        const auto Ca = veor3q_u64(veor3q_u64(Aba, Aga, Aka), Ama, Asa);
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
        const auto Bba_rc = veor3q_u64(Aba, Da, vld1q_dup_u64(&ROUND_CONSTANTS[n]));
        const auto Bbe = vxarq_u64(Age, De, 20);
        const auto Bbi = vxarq_u64(Aki, Di, 21);
        const auto Bbo = vxarq_u64(Amo, Do, 43);
        const auto Bbu = vxarq_u64(Asu, Du, 50);
        const auto Bga = vxarq_u64(Abo, Do, 36);
        const auto Bge = vxarq_u64(Agu, Du, 44);
        const auto Bgi = vxarq_u64(Aka, Da, 61);
        const auto Bgo = vxarq_u64(Ame, De, 19);
        const auto Bgu = vxarq_u64(Asi, Di, 3);
        const auto Bka = vxarq_u64(Abe, De, 63);
        const auto Bke = vxarq_u64(Agi, Di, 58);
        const auto Bki = vxarq_u64(Ako, Do, 39);
        const auto Bko = vxarq_u64(Amu, Du, 56);
        const auto Bku = vxarq_u64(Asa, Da, 46);
        const auto Bma = vxarq_u64(Abu, Du, 37);
        const auto Bme = vxarq_u64(Aga, Da, 28);
        const auto Bmi = vxarq_u64(Ake, De, 54);
        const auto Bmo = vxarq_u64(Ami, Di, 49);
        const auto Bmu = vxarq_u64(Aso, Do, 8);
        const auto Bsa = vxarq_u64(Abi, Di, 2);
        const auto Bse = vxarq_u64(Ago, Do, 9);
        const auto Bsi = vxarq_u64(Aku, Du, 25);
        const auto Bso = vxarq_u64(Ama, Da, 23);
        const auto Bsu = vxarq_u64(Ase, De, 62);
        Aba = vbcaxq_u64(Bba_rc, Bbi, Bbe);
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
    }
    vst1q_lane_u64(&state[0], Aba, 0); vst1q_lane_u64(&state[1], Abe, 0); vst1q_lane_u64(&state[2], Abi, 0); vst1q_lane_u64(&state[3], Abo, 0); vst1q_lane_u64(&state[4], Abu, 0); vst1q_lane_u64(&state[5], Aga, 0); vst1q_lane_u64(&state[6], Age, 0); vst1q_lane_u64(&state[7], Agi, 0); vst1q_lane_u64(&state[8], Ago, 0); vst1q_lane_u64(&state[9], Agu, 0); vst1q_lane_u64(&state[10], Aka, 0); vst1q_lane_u64(&state[11], Ake, 0); vst1q_lane_u64(&state[12], Aki, 0); vst1q_lane_u64(&state[13], Ako, 0); vst1q_lane_u64(&state[14], Aku, 0); vst1q_lane_u64(&state[15], Ama, 0); vst1q_lane_u64(&state[16], Ame, 0); vst1q_lane_u64(&state[17], Ami, 0); vst1q_lane_u64(&state[18], Amo, 0); vst1q_lane_u64(&state[19], Amu, 0); vst1q_lane_u64(&state[20], Asa, 0); vst1q_lane_u64(&state[21], Ase, 0); vst1q_lane_u64(&state[22], Asi, 0); vst1q_lane_u64(&state[23], Aso, 0); vst1q_lane_u64(&state[24], Asu, 0);
}

SHA3_TARGET void perm_sha3_u4(uint64_t state[25])
{
    auto Aba = vld1q_dup_u64(&state[0]); auto Abe = vld1q_dup_u64(&state[1]); auto Abi = vld1q_dup_u64(&state[2]); auto Abo = vld1q_dup_u64(&state[3]); auto Abu = vld1q_dup_u64(&state[4]); auto Aga = vld1q_dup_u64(&state[5]); auto Age = vld1q_dup_u64(&state[6]); auto Agi = vld1q_dup_u64(&state[7]); auto Ago = vld1q_dup_u64(&state[8]); auto Agu = vld1q_dup_u64(&state[9]); auto Aka = vld1q_dup_u64(&state[10]); auto Ake = vld1q_dup_u64(&state[11]); auto Aki = vld1q_dup_u64(&state[12]); auto Ako = vld1q_dup_u64(&state[13]); auto Aku = vld1q_dup_u64(&state[14]); auto Ama = vld1q_dup_u64(&state[15]); auto Ame = vld1q_dup_u64(&state[16]); auto Ami = vld1q_dup_u64(&state[17]); auto Amo = vld1q_dup_u64(&state[18]); auto Amu = vld1q_dup_u64(&state[19]); auto Asa = vld1q_dup_u64(&state[20]); auto Ase = vld1q_dup_u64(&state[21]); auto Asi = vld1q_dup_u64(&state[22]); auto Aso = vld1q_dup_u64(&state[23]); auto Asu = vld1q_dup_u64(&state[24]);
    _Pragma("GCC unroll 4")
    for (size_t n = 0; n < 24; ++n)
    {
        const auto Ca = veor3q_u64(veor3q_u64(Aba, Aga, Aka), Ama, Asa);
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
        const auto Bba_rc = veor3q_u64(Aba, Da, vld1q_dup_u64(&ROUND_CONSTANTS[n]));
        const auto Bbe = vxarq_u64(Age, De, 20);
        const auto Bbi = vxarq_u64(Aki, Di, 21);
        const auto Bbo = vxarq_u64(Amo, Do, 43);
        const auto Bbu = vxarq_u64(Asu, Du, 50);
        const auto Bga = vxarq_u64(Abo, Do, 36);
        const auto Bge = vxarq_u64(Agu, Du, 44);
        const auto Bgi = vxarq_u64(Aka, Da, 61);
        const auto Bgo = vxarq_u64(Ame, De, 19);
        const auto Bgu = vxarq_u64(Asi, Di, 3);
        const auto Bka = vxarq_u64(Abe, De, 63);
        const auto Bke = vxarq_u64(Agi, Di, 58);
        const auto Bki = vxarq_u64(Ako, Do, 39);
        const auto Bko = vxarq_u64(Amu, Du, 56);
        const auto Bku = vxarq_u64(Asa, Da, 46);
        const auto Bma = vxarq_u64(Abu, Du, 37);
        const auto Bme = vxarq_u64(Aga, Da, 28);
        const auto Bmi = vxarq_u64(Ake, De, 54);
        const auto Bmo = vxarq_u64(Ami, Di, 49);
        const auto Bmu = vxarq_u64(Aso, Do, 8);
        const auto Bsa = vxarq_u64(Abi, Di, 2);
        const auto Bse = vxarq_u64(Ago, Do, 9);
        const auto Bsi = vxarq_u64(Aku, Du, 25);
        const auto Bso = vxarq_u64(Ama, Da, 23);
        const auto Bsu = vxarq_u64(Ase, De, 62);
        Aba = vbcaxq_u64(Bba_rc, Bbi, Bbe);
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
    }
    vst1q_lane_u64(&state[0], Aba, 0); vst1q_lane_u64(&state[1], Abe, 0); vst1q_lane_u64(&state[2], Abi, 0); vst1q_lane_u64(&state[3], Abo, 0); vst1q_lane_u64(&state[4], Abu, 0); vst1q_lane_u64(&state[5], Aga, 0); vst1q_lane_u64(&state[6], Age, 0); vst1q_lane_u64(&state[7], Agi, 0); vst1q_lane_u64(&state[8], Ago, 0); vst1q_lane_u64(&state[9], Agu, 0); vst1q_lane_u64(&state[10], Aka, 0); vst1q_lane_u64(&state[11], Ake, 0); vst1q_lane_u64(&state[12], Aki, 0); vst1q_lane_u64(&state[13], Ako, 0); vst1q_lane_u64(&state[14], Aku, 0); vst1q_lane_u64(&state[15], Ama, 0); vst1q_lane_u64(&state[16], Ame, 0); vst1q_lane_u64(&state[17], Ami, 0); vst1q_lane_u64(&state[18], Amo, 0); vst1q_lane_u64(&state[19], Amu, 0); vst1q_lane_u64(&state[20], Asa, 0); vst1q_lane_u64(&state[21], Ase, 0); vst1q_lane_u64(&state[22], Asi, 0); vst1q_lane_u64(&state[23], Aso, 0); vst1q_lane_u64(&state[24], Asu, 0);
}

SHA3_TARGET void perm_sha3_u24(uint64_t state[25])
{
    auto Aba = vld1q_dup_u64(&state[0]); auto Abe = vld1q_dup_u64(&state[1]); auto Abi = vld1q_dup_u64(&state[2]); auto Abo = vld1q_dup_u64(&state[3]); auto Abu = vld1q_dup_u64(&state[4]); auto Aga = vld1q_dup_u64(&state[5]); auto Age = vld1q_dup_u64(&state[6]); auto Agi = vld1q_dup_u64(&state[7]); auto Ago = vld1q_dup_u64(&state[8]); auto Agu = vld1q_dup_u64(&state[9]); auto Aka = vld1q_dup_u64(&state[10]); auto Ake = vld1q_dup_u64(&state[11]); auto Aki = vld1q_dup_u64(&state[12]); auto Ako = vld1q_dup_u64(&state[13]); auto Aku = vld1q_dup_u64(&state[14]); auto Ama = vld1q_dup_u64(&state[15]); auto Ame = vld1q_dup_u64(&state[16]); auto Ami = vld1q_dup_u64(&state[17]); auto Amo = vld1q_dup_u64(&state[18]); auto Amu = vld1q_dup_u64(&state[19]); auto Asa = vld1q_dup_u64(&state[20]); auto Ase = vld1q_dup_u64(&state[21]); auto Asi = vld1q_dup_u64(&state[22]); auto Aso = vld1q_dup_u64(&state[23]); auto Asu = vld1q_dup_u64(&state[24]);
    _Pragma("GCC unroll 24")
    for (size_t n = 0; n < 24; ++n)
    {
        const auto Ca = veor3q_u64(veor3q_u64(Aba, Aga, Aka), Ama, Asa);
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
        const auto Bba_rc = veor3q_u64(Aba, Da, vld1q_dup_u64(&ROUND_CONSTANTS[n]));
        const auto Bbe = vxarq_u64(Age, De, 20);
        const auto Bbi = vxarq_u64(Aki, Di, 21);
        const auto Bbo = vxarq_u64(Amo, Do, 43);
        const auto Bbu = vxarq_u64(Asu, Du, 50);
        const auto Bga = vxarq_u64(Abo, Do, 36);
        const auto Bge = vxarq_u64(Agu, Du, 44);
        const auto Bgi = vxarq_u64(Aka, Da, 61);
        const auto Bgo = vxarq_u64(Ame, De, 19);
        const auto Bgu = vxarq_u64(Asi, Di, 3);
        const auto Bka = vxarq_u64(Abe, De, 63);
        const auto Bke = vxarq_u64(Agi, Di, 58);
        const auto Bki = vxarq_u64(Ako, Do, 39);
        const auto Bko = vxarq_u64(Amu, Du, 56);
        const auto Bku = vxarq_u64(Asa, Da, 46);
        const auto Bma = vxarq_u64(Abu, Du, 37);
        const auto Bme = vxarq_u64(Aga, Da, 28);
        const auto Bmi = vxarq_u64(Ake, De, 54);
        const auto Bmo = vxarq_u64(Ami, Di, 49);
        const auto Bmu = vxarq_u64(Aso, Do, 8);
        const auto Bsa = vxarq_u64(Abi, Di, 2);
        const auto Bse = vxarq_u64(Ago, Do, 9);
        const auto Bsi = vxarq_u64(Aku, Du, 25);
        const auto Bso = vxarq_u64(Ama, Da, 23);
        const auto Bsu = vxarq_u64(Ase, De, 62);
        Aba = vbcaxq_u64(Bba_rc, Bbi, Bbe);
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
    }
    vst1q_lane_u64(&state[0], Aba, 0); vst1q_lane_u64(&state[1], Abe, 0); vst1q_lane_u64(&state[2], Abi, 0); vst1q_lane_u64(&state[3], Abo, 0); vst1q_lane_u64(&state[4], Abu, 0); vst1q_lane_u64(&state[5], Aga, 0); vst1q_lane_u64(&state[6], Age, 0); vst1q_lane_u64(&state[7], Agi, 0); vst1q_lane_u64(&state[8], Ago, 0); vst1q_lane_u64(&state[9], Agu, 0); vst1q_lane_u64(&state[10], Aka, 0); vst1q_lane_u64(&state[11], Ake, 0); vst1q_lane_u64(&state[12], Aki, 0); vst1q_lane_u64(&state[13], Ako, 0); vst1q_lane_u64(&state[14], Aku, 0); vst1q_lane_u64(&state[15], Ama, 0); vst1q_lane_u64(&state[16], Ame, 0); vst1q_lane_u64(&state[17], Ami, 0); vst1q_lane_u64(&state[18], Amo, 0); vst1q_lane_u64(&state[19], Amu, 0); vst1q_lane_u64(&state[20], Asa, 0); vst1q_lane_u64(&state[21], Ase, 0); vst1q_lane_u64(&state[22], Asi, 0); vst1q_lane_u64(&state[23], Aso, 0); vst1q_lane_u64(&state[24], Asu, 0);
}

static bool has_sha3()
{
#if defined(__APPLE__)
    int64_t v = 0; size_t sz = sizeof(v);
    return sysctlbyname("hw.optional.armv8_2_sha3", &v, &sz, nullptr, 0) == 0 && v == 1;
#else
    return (getauxval(AT_HWCAP) & HWCAP_SHA3) != 0;
#endif
}

int main(int argc, char** argv)
{
#if defined(__APPLE__)
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
    char brand[128] = {}; size_t bl = sizeof(brand);
    sysctlbyname("machdep.cpu.brand_string", brand, &bl, nullptr, 0);
    std::printf("cpu: %s\n", brand);
#else
    std::ifstream ci("/proc/cpuinfo"); std::string line;
    while (std::getline(ci, line))
    {
        if (line.rfind("processor", 0) == 0 && line.find(": 0") == std::string::npos) break;
        if (line.rfind("CPU part", 0) == 0 || line.rfind("CPU implementer", 0) == 0 || line.rfind("CPU variant", 0) == 0) std::cout << line << "\n";
    }
#endif
    const bool sha3 = has_sha3();
    std::printf("sha3: %d\n", sha3);
    struct V { const char* name; void (*f)(uint64_t*); bool needs_sha3; };
    const V vs[] = {{"scalar", perm_scalar, false}, {"sha3_u1", perm_sha3_u1, true}, {"sha3_x1", perm_sha3_x1, true}, {"sha3_u2", perm_sha3_u2, true}, {"sha3_u4", perm_sha3_u4, true}, {"sha3_u24", perm_sha3_u24, true}};
    uint64_t ref[25], st0[25];
    for (int i = 0; i < 25; ++i) st0[i] = i * 0x9e3779b97f4a7c15ull;
    std::memcpy(ref, st0, sizeof ref); for (int k = 0; k < 3; ++k) perm_scalar(ref);
    const long n = argc > 1 ? std::atol(argv[1]) : 2000000;
    const int reps = argc > 2 ? std::atoi(argv[2]) : 9;
    double best[8]; std::fill(best, best + 8, 1e30);
    for (int r = 0; r < reps; ++r)
        for (size_t j = 0; j < std::size(vs); ++j)
        {
            if (vs[j].needs_sha3 && !sha3) continue;
            uint64_t st[25]; std::memcpy(st, st0, sizeof st);
            if (r == 0) { for (int k = 0; k < 3; ++k) vs[j].f(st); if (std::memcmp(st, ref, sizeof st)) { std::printf("%s: MISMATCH\n", vs[j].name); return 1; } }
            const auto t0 = std::chrono::steady_clock::now();
            for (long i = 0; i < n; ++i) vs[j].f(st);
            const auto t1 = std::chrono::steady_clock::now();
            asm volatile("" : : "r"(st) : "memory");
            best[j] = std::min(best[j], std::chrono::duration<double, std::nano>(t1 - t0).count() / double(n));
        }
    for (size_t j = 0; j < std::size(vs); ++j)
        if (!vs[j].needs_sha3 || sha3) std::printf("%-9s %7.1f ns/perm  (%.2fx vs scalar)\n", vs[j].name, best[j], best[0] / best[j]);
}

// JUMPDEST analysis fable_a for AArch64 NEON: the g8 half-group scheme with the block entry
// chain kept in vector registers.
//
// g8_neon carries the block entry through memory: vector stores of the chain tables followed by
// a byte load at a data-dependent offset. On Neoverse N2 it only reaches its no-chain bound when
// that offset is always 0, so the load is not forwarded from the vector store in general and the
// iterations serialize on the store commit. Here the entry is a broadcast byte vector and one
// step of the chain is a TBX: the table of a 16-byte group is indexed by the entry and entries
// >= 16 fall through unchanged (TBX keeps the destination for out-of-range indexes), which is
// exactly the tail "entry - 16" once the table values are in the raw frame 16 + entry.
//
// fable_a_1 still gathers the JUMPDEST bits with scalar byte loads from stored tables, but these
// loads are not loop-carried: a forwarding miss costs latency that overlaps with the vector work.
// fable_a_2 gathers in vector registers and touches no memory but the output.
#pragma once

namespace
{
struct FableAGroup
{
    uint8x16_t ex;  // Half exit per entry: the next start in the group frame (A: 8.., B: 16..).
    uint8x16_t xm;  // Group exit per entry, raw: 16 + entry into the next group (16..48).
    uint8x16_t v;   // JUMPDEST bits visited from each entry within its half.
};

struct FableAConsts
{
    uint8x16_t iota;  // lane + 1 - 0x5f: max_s8(c, 0x5f) + iota is the next instruction start.
    uint8x16_t hend;  // End of the half: 8 for lanes 0..7, 16 for lanes 8..15.
    uint8x16_t lane;
    uint8x16_t bit;
    int8x16_t k5f;
    uint8x16_t k5b;
    uint8x16_t k16;
    uint8x16_t k2;
    uint8x16_t c01;  // 0, 1, 0, 1, ...
};

inline FableAConsts fable_a_consts()
{
    static const uint8_t iota_d[16] = {0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab,
        0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1};
    static const uint8_t hend_d[16] = {8, 8, 8, 8, 8, 8, 8, 8, 16, 16, 16, 16, 16, 16, 16, 16};
    static const uint8_t lane_d[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    static const uint8_t bit_d[16] = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
    static const uint8_t c01_d[16] = {0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1};
    return {vld1q_u8(iota_d), vld1q_u8(hend_d), vld1q_u8(lane_d), vld1q_u8(bit_d),
        vdupq_n_s8(0x5f), vdupq_n_u8(0x5b), vdupq_n_u8(16), vdupq_n_u8(2), vld1q_u8(c01_d)};
}

// 18 vector ops per 16 bytes (g8_group: 19).
inline FableAGroup fable_a_group(uint8x16_t c, const FableAConsts& k)
{
    const auto n = vaddq_u8(vreinterpretq_u8_s8(vmaxq_s8(vreinterpretq_s8_u8(c), k.k5f)), k.iota);
    auto x = vbslq_u8(vcltq_u8(n, k.hend), n, k.lane);  // Next start in the half, or a sink.
    auto v = vandq_u8(vceqq_u8(c, k.k5b), k.bit);
    for (int i = 0; i < 3; ++i)
    {
        v = vorrq_u8(v, vqtbl1q_u8(v, x));
        x = vqtbl1q_u8(x, x);
    }
    const auto ex = vqtbl1q_u8(n, x);
    return {ex, vmaxq_u8(ex, vqtbl1q_u8(ex, ex)), v};
}

// Per-block tables. Only the first 16 bytes of each are written; the rest stays zero.
struct FableATables
{
    alignas(64) uint8_t T0[48];  // Half A exit (raw group frame) by entry 0..7; read for 0..32.
    alignas(64) uint8_t T2[48];
    alignas(64) uint8_t VA[64];  // JUMPDEST bits of group A by entry 0..15; read for 0..48.
    alignas(64) uint8_t VC[64];
};

// Lane 0 as a scalar. The empty asm keeps the compiler from rematerializing the UMOV at every
// use, which would cost vector pipe slots.
inline unsigned fable_a_lane0(uint8x16_t v)
{
    unsigned s = vgetq_lane_u8(v, 0);
    asm("" : "+r"(s));
    return s;
}

// The JUMPDEST bits of the 32-byte block whose tables are in t, entered at s (0..32) with s2
// (0..32) the entry into its 2nd group. Branch-free: the loads are unconditional.
inline uint32_t fable_a_bits(const FableATables& t, unsigned s, unsigned s2)
{
    const unsigned t0 = t.T0[s], t2 = t.T2[s2];
    const unsigned e1 = s < 8 ? t0 : s;
    const unsigned e3 = s2 < 8 ? t2 : s2;
    const uint32_t v0 = t.VA[s], v2 = t.VC[s2];
    const uint32_t b0 = s < 8 ? v0 : 0;
    const uint32_t b2 = s2 < 8 ? v2 : 0;
    return b0 | uint32_t{t.VA[e1]} << 8 | b2 << 16 | uint32_t{t.VC[e3]} << 24;
}
}  // namespace

// bits: bit i is set iff position i is a valid JUMPDEST. Reads up to 31 bytes past the end.
// The chain does one TBX per 16-byte group: 2 dependent steps (about 8 cycles) per block.
void fable_a_1(const uint8_t* code, size_t size, uint64_t* bits)
{
    const auto k = fable_a_consts();
    FableATables t{};
    auto* const out = reinterpret_cast<uint32_t*>(bits);
    auto e = vdupq_n_u8(0);
    const size_t num_blocks = (size + 31) / 32;
    for (size_t q = 0; q < num_blocks; ++q)
    {
        const auto a = fable_a_group(vld1q_u8(code + 32 * q), k);
        const auto c = fable_a_group(vld1q_u8(code + 32 * q + 16), k);
        vst1q_u8(t.T0, a.ex);
        vst1q_u8(t.T2, c.ex);
        vst1q_u8(t.VA, a.v);
        vst1q_u8(t.VC, c.v);
        const auto e2 = vsubq_u8(vqtbx1q_u8(e, a.xm, e), k.k16);
        const unsigned s = fable_a_lane0(e);
        const unsigned s2 = fable_a_lane0(e2);
        e = vsubq_u8(vqtbx1q_u8(e2, c.xm, e2), k.k16);
        out[q] = fable_a_bits(t, s, s2);
    }
}

namespace
{
// The JUMPDEST bits of a 16-byte group as a 16-bit word per entry 0..15 (as in g8_avx2): the
// low byte is the half A bits (0 for entries in half B), the high byte the half B bits reached.
struct FableAWords
{
    uint8x16_t lo, hi;  // Entries 0..7 and 8..15.
};

inline FableAWords fable_a_words(const FableAGroup& g)
{
    const auto h = vqtbl1q_u8(g.v, g.ex);  // Half B bits from the half A exit; 0 for B lanes.
    return {vzip1q_u8(g.v, h), vzip2q_u8(vdupq_n_u8(0), g.v)};
}

// The word of the entry e (broadcast, 0..32) in lanes 0..1: one 2-register TBL at 2e + {0, 1},
// out of range (0) for e >= 16.
inline uint8x16_t fable_a_word(const FableAWords& w, uint8x16_t e, const FableAConsts& k)
{
    const auto idx = vmlaq_u8(k.c01, e, k.k2);
    return vqtbl2q_u8(uint8x16x2_t{{w.lo, w.hi}}, idx);
}
}  // namespace

// No memory inside the loop: the JUMPDEST bits are gathered from the entry vectors with
// 2-register TBLs (a single uop on Neoverse N2) and packed with SLI into one 32-bit store per
// block. Per block 11 more vector ALU ops but 3 fewer vector stores than fable_a_1, and no
// store-to-load traffic at all.
void fable_a_2(const uint8_t* code, size_t size, uint64_t* bits)
{
    const auto k = fable_a_consts();
    auto* const out = reinterpret_cast<uint32_t*>(bits);
    auto e = vdupq_n_u8(0);
    const size_t num_blocks = (size + 31) / 32;
    for (size_t q = 0; q < num_blocks; ++q)
    {
        const auto a = fable_a_group(vld1q_u8(code + 32 * q), k);
        const auto c = fable_a_group(vld1q_u8(code + 32 * q + 16), k);
        const auto e2 = vsubq_u8(vqtbx1q_u8(e, a.xm, e), k.k16);
        const auto wa = fable_a_word(fable_a_words(a), e, k);
        const auto wc = fable_a_word(fable_a_words(c), e2, k);
        e = vsubq_u8(vqtbx1q_u8(e2, c.xm, e2), k.k16);
        const auto w = vsliq_n_u32(vreinterpretq_u32_u8(wa), vreinterpretq_u32_u8(wc), 16);
        vst1q_lane_u32(out + q, w, 0);
    }
}

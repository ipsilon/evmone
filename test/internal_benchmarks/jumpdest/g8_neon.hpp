// JUMPDEST analysis g8 for AArch64 NEON: the g8_sse scheme with raw lane pointers.
//
// A 16-byte register holds two 8-byte halves. Pointers are lane indexes 0..15; a position whose
// next instruction start leaves its half points to itself (a sink), so a pointer-jumping step is
// one TBL. After 3 rounds x holds the sink of every entry; a gather from N (the next start in the
// 16-byte group frame) gives the half exit, and one merge step joins the halves.
#pragma once

namespace
{
struct G8Group
{
    uint8x16_t ex;  // Half exit per entry: the next start in the group frame (A: 8.., B: 16..).
    uint8x16_t xm;  // Group exit per entry: 16..48.
    uint8x16_t v;   // JUMPDEST bits visited from each entry within its half.
};

inline G8Group g8_group(uint8x16_t c)
{
    static const uint8_t lane_d[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    static const uint8_t lane1_d[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    static const uint8_t hend_d[16] = {8, 8, 8, 8, 8, 8, 8, 8, 16, 16, 16, 16, 16, 16, 16, 16};
    static const uint8_t bit_d[16] = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
    const auto s = vreinterpretq_s8_u8(c);
    const auto pl = vreinterpretq_u8_s8(vsubq_s8(vmaxq_s8(s, vdupq_n_s8(0x5f)), vdupq_n_s8(0x5f)));
    const auto n = vaddq_u8(vld1q_u8(lane1_d), pl);
    auto x = vbslq_u8(vcltq_u8(n, vld1q_u8(hend_d)), n, vld1q_u8(lane_d));
    auto v = vandq_u8(vceqq_u8(c, vdupq_n_u8(0x5b)), vld1q_u8(bit_d));
    for (int k = 0; k < 3; ++k)
    {
        v = vorrq_u8(v, vqtbl1q_u8(v, x));
        x = vqtbl1q_u8(x, x);
    }
    const auto ex = vqtbl1q_u8(n, x);
    return {ex, vmaxq_u8(ex, vqtbl1q_u8(ex, ex)), v};
}

struct G8NeonTables
{
    alignas(64) uint8_t TA[48];  // Entry into the 2nd group, by block entry.
    alignas(64) uint8_t PT[48];  // Entry into the next block, by block entry.
    alignas(64) uint8_t T0[48];  // Entry into half 1 (raw, group frame), by entry into half 0.
    alignas(64) uint8_t T2[48];
    alignas(64) uint8_t V0[48];  // JUMPDEST bits of half 0, by entry.
    alignas(64) uint8_t V1[48];  // JUMPDEST bits of half 1, by raw entry 8..15.
    alignas(64) uint8_t V2[48];
    alignas(64) uint8_t V3[48];
    G8NeonTables()
    {
        for (unsigned i = 0; i < 48; ++i)
        {
            TA[i] = static_cast<uint8_t>(i - 16);
            PT[i] = static_cast<uint8_t>(i - 32);
            T0[i] = T2[i] = static_cast<uint8_t>(i);
            V0[i] = V1[i] = V2[i] = V3[i] = 0;
        }
    }
};
}  // namespace

template <bool Chain>
void g8_neon_impl(const uint8_t* code, size_t size, uint64_t* bits)
{
    G8NeonTables t;
    auto* const out = reinterpret_cast<uint8_t*>(bits);
    const auto k16 = vdupq_n_u8(16);
    size_t s = 0;
    const size_t num_blocks = (size + 31) / 32;
    for (size_t q = 0; q < num_blocks; ++q)
    {
        const auto a = g8_group(vld1q_u8(code + 32 * q));
        const auto c = g8_group(vld1q_u8(code + 32 * q + 16));
        const auto ta = vsubq_u8(a.xm, k16);
        const auto cx = vsubq_u8(c.xm, k16);
        vst1q_u8(t.TA, ta);
        vst1q_u8(t.PT, vorrq_u8(vqtbl1q_u8(cx, ta), vsubq_u8(vmaxq_u8(ta, k16), k16)));
        vst1q_u8(t.PT + 16, cx);
        vst1_u8(t.T0, vget_low_u8(a.ex));
        vst1_u8(t.T2, vget_low_u8(c.ex));
        vst1_u8(t.V0, vget_low_u8(a.v));
        vst1_u8(t.V2, vget_low_u8(c.v));
        vst1q_u8(t.V1, a.v);
        vst1q_u8(t.V3, c.v);

        const unsigned e2 = t.TA[s];
        out[4 * q + 0] = t.V0[s];
        out[4 * q + 1] = t.V1[t.T0[s]];
        out[4 * q + 2] = t.V2[e2];
        out[4 * q + 3] = t.V3[t.T2[e2]];
        s = Chain ? t.PT[s] : 0;
    }
}

// bits: bit i is set iff position i is a valid JUMPDEST. Reads up to 31 bytes past the end.
void g8_neon(const uint8_t* code, size_t size, uint64_t* bits)
{
    g8_neon_impl<true>(code, size, bits);
}

// Experiment: no loop-carried entry chain (wrong result), the throughput bound.
void x_g8_nochain(const uint8_t* code, size_t size, uint64_t* bits)
{
    g8_neon_impl<false>(code, size, bits);
}

// The entry chain in the vector domain: the entry is broadcast in a register and the chain
// tables are TBL lookups, so nothing goes through memory.
void g8v_neon(const uint8_t* code, size_t size, uint64_t* bits)
{
    static const uint8_t i0_d[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    static const uint8_t lo8_d[16] = {255, 255, 255, 255, 255, 255, 255, 255};
    static const uint8_t pick_d[16] = {0, 16, 32, 48};
    const auto i0 = vld1q_u8(i0_d);
    const auto k16 = vdupq_n_u8(16);
    const auto i16 = vaddq_u8(i0, k16);
    const auto i32 = vaddq_u8(i16, k16);
    const auto lo8 = vld1q_u8(lo8_d);
    const auto pick = vld1q_u8(pick_d);
    auto* const out = reinterpret_cast<uint32_t*>(bits);
    auto e = vdupq_n_u8(0);
    const size_t num_blocks = (size + 31) / 32;
    for (size_t q = 0; q < num_blocks; ++q)
    {
        const auto a = g8_group(vld1q_u8(code + 32 * q));
        const auto c = g8_group(vld1q_u8(code + 32 * q + 16));
        const auto ta = vsubq_u8(a.xm, k16);
        const auto cx = vsubq_u8(c.xm, k16);
        const auto pab = vorrq_u8(vqtbl1q_u8(cx, ta), vsubq_u8(vmaxq_u8(ta, k16), k16));
        const auto e2 = vqtbl3q_u8(uint8x16x3_t{{ta, i0, i16}}, e);
        const auto e1 = vqtbl3q_u8(uint8x16x3_t{{vbslq_u8(lo8, a.ex, i0), i16, i32}}, e);
        const auto e3 = vqtbl3q_u8(uint8x16x3_t{{vbslq_u8(lo8, c.ex, i0), i16, i32}}, e2);
        const auto b0 = vqtbl1q_u8(vandq_u8(a.v, lo8), e);
        const auto b1 = vqtbl1q_u8(a.v, e1);
        const auto b2 = vqtbl1q_u8(vandq_u8(c.v, lo8), e2);
        const auto b3 = vqtbl1q_u8(c.v, e3);
        const auto w = vqtbl4q_u8(uint8x16x4_t{{b0, b1, b2, b3}}, pick);
        vst1q_lane_u32(out + q, vreinterpretq_u32_u8(w), 0);
        e = vqtbl3q_u8(uint8x16x3_t{{pab, cx, i0}}, e);
    }
}

namespace
{
struct G8Block
{
    uint8x16_t ta, pab, cx;  // TA and PT for the entries 0..15 (PT for 16..31 is cx).
    G8Group a, c;
};

inline G8Block g8_block(const uint8_t* p)
{
    const auto k16 = vdupq_n_u8(16);
    const auto a = g8_group(vld1q_u8(p));
    const auto c = g8_group(vld1q_u8(p + 16));
    const auto ta = vsubq_u8(a.xm, k16);
    const auto cx = vsubq_u8(c.xm, k16);
    return {ta, vorrq_u8(vqtbl1q_u8(cx, ta), vsubq_u8(vmaxq_u8(ta, k16), k16)), cx, a, c};
}

inline void g8_store(G8NeonTables& t, const G8Block& b)
{
    vst1q_u8(t.TA, b.ta);
    vst1q_u8(t.PT, b.pab);
    vst1q_u8(t.PT + 16, b.cx);
    vst1_u8(t.T0, vget_low_u8(b.a.ex));
    vst1_u8(t.T2, vget_low_u8(b.c.ex));
    vst1_u8(t.V0, vget_low_u8(b.a.v));
    vst1_u8(t.V2, vget_low_u8(b.c.v));
    vst1q_u8(t.V1, b.a.v);
    vst1q_u8(t.V3, b.c.v);
}

inline uint32_t g8_bits(const G8NeonTables& t, unsigned s)
{
    const unsigned e2 = t.TA[s];
    return uint32_t{t.V0[s]} | uint32_t{t.V1[t.T0[s]]} << 8 | uint32_t{t.V2[e2]} << 16 |
           uint32_t{t.V3[t.T2[e2]]} << 24;
}
}  // namespace

// Two 32-byte blocks per step of the entry chain: the next entry comes from the composed table
// PT2[PT1[s]], built in SIMD off the chain.
void g8x2_neon(const uint8_t* code, size_t size, uint64_t* bits)
{
    G8NeonTables t1, t2;
    alignas(64) uint8_t PT64[48];
    static const uint8_t i0_d[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    const auto i0 = vld1q_u8(i0_d);
    auto* const out = reinterpret_cast<uint32_t*>(bits);
    unsigned s = 0;
    const size_t num_blocks = (size + 31) / 32;
    size_t q = 0;
    for (; q + 2 <= num_blocks; q += 2)
    {
        const auto b1 = g8_block(code + 32 * q);
        const auto b2 = g8_block(code + 32 * q + 32);
        g8_store(t1, b1);
        g8_store(t2, b2);
        const uint8x16x3_t pt2{{b2.pab, b2.cx, i0}};
        vst1q_u8(PT64, vqtbl3q_u8(pt2, b1.pab));
        vst1q_u8(PT64 + 16, vqtbl3q_u8(pt2, b1.cx));
        vst1q_u8(PT64 + 32, vqtbl3q_u8(pt2, i0));
        const unsigned s1 = t1.PT[s];
        out[q] = g8_bits(t1, s);
        out[q + 1] = g8_bits(t2, s1);
        s = PT64[s];
    }
    if (q < num_blocks)
    {
        g8_store(t1, g8_block(code + 32 * q));
        out[q] = g8_bits(t1, s);
    }
}

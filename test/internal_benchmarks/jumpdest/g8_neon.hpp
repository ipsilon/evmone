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

// bits: bit i is set iff position i is a valid JUMPDEST. Reads up to 31 bytes past the end.
void g8_neon(const uint8_t* code, size_t size, uint64_t* bits)
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
        s = t.PT[s];
    }
}

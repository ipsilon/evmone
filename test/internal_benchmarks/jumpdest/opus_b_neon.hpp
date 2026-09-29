// JUMPDEST analysis opus_b for AArch64 NEON: the g8 scheme, strip-mined in two passes.
//
// Pass 1 runs the g8 group analysis for a strip of blocks and stores the lookup tables of each
// block to its own slot, so it has no loop-carried dependency. Pass 2 walks the entry chain over
// the strip with scalar loads only: the tables were stored long before, so the loads neither wait
// for the SIMD code of the same block nor get replayed by store-to-load forwarding.
//
// All tables are indexed by the entry offset plus 16 (the "u" frame), so a group exit xm
// (16..48) is directly the index into the next group's tables and no SIMD op is spent on
// rebasing. opus_b_1 composes the block transition in pass 2 (two chain loads per block);
// opus_b_2 composes it with SIMD (one chain load per block, 3 more SIMD ops).
#pragma once

namespace
{
struct opus_b_Group
{
    uint8x16_t ex;  // Half exit per entry: the next start in the group frame (A: 8.., B: 16..).
    uint8x16_t xm;  // Group exit per entry: 16..48.
    uint8x16_t v;   // JUMPDEST bits visited from each entry within its half.
};

inline opus_b_Group opus_b_group(uint8x16_t c)
{
    // lane + 1 - 0x5f, so that max_s8(c, 0x5f) + it is the next instruction start.
    static const uint8_t lane1m_d[16] = {0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa,
        0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1};
    static const uint8_t lane_d[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    static const uint8_t hend_d[16] = {8, 8, 8, 8, 8, 8, 8, 8, 16, 16, 16, 16, 16, 16, 16, 16};
    static const uint8_t bit_d[16] = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
    const auto m = vreinterpretq_u8_s8(vmaxq_s8(vreinterpretq_s8_u8(c), vdupq_n_s8(0x5f)));
    const auto n = vaddq_u8(m, vld1q_u8(lane1m_d));
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

constexpr size_t opus_b_strip = 8;

// The tables of a strip, field-major so that pass 2 addresses every table as base + 64 j + index.
// u is the block entry offset (0..32) + 16, e2 is the entry offset into the 2nd group + 16,
// r is a raw half-1 entry in the group frame.
struct opus_b_Strip
{
    uint8_t TA[opus_b_strip][64];  // e2 by u.
    uint8_t PC[opus_b_strip][64];  // Next block's u by e2 (opus_b_1).
    uint8_t PT[opus_b_strip][64];  // Next block's u by u (opus_b_2).
    uint8_t T0[opus_b_strip][64];  // r by u.
    uint8_t V0[opus_b_strip][64];  // Half 0 bits by u.
    uint8_t V1[opus_b_strip][64];  // Half 1 bits by r.
    uint8_t T2[opus_b_strip][64];  // r by e2.
    uint8_t V2[opus_b_strip][64];  // Half 2 bits by e2.
    uint8_t V3[opus_b_strip][64];  // Half 3 bits by r.
    opus_b_Strip()
    {
        for (size_t j = 0; j < opus_b_strip; ++j)
        {
            for (unsigned i = 0; i < 64; ++i)
            {
                const auto r = static_cast<uint8_t>(i - 16);
                TA[j][i] = PC[j][i] = T0[j][i] = T2[j][i] = r;
                PT[j][i] = static_cast<uint8_t>(i - 32);
                V0[j][i] = V1[j][i] = V2[j][i] = V3[j][i] = 0;
            }
        }
    }
};

template <bool SimdPT>
inline void opus_b_pass1(opus_b_Strip& t, size_t j, const uint8_t* p)
{
    const auto a = opus_b_group(vld1q_u8(p));
    const auto c = opus_b_group(vld1q_u8(p + 16));
    vst1q_u8(&t.TA[j][16], a.xm);
    if constexpr (SimdPT)
    {
        const auto ta = vsubq_u8(a.xm, vdupq_n_u8(16));
        vst1q_u8(&t.PT[j][16], vmaxq_u8(vqtbl1q_u8(c.xm, ta), ta));
        vst1q_u8(&t.PT[j][32], c.xm);
    }
    else
        vst1q_u8(&t.PC[j][16], c.xm);
    vst1_u8(&t.T0[j][16], vget_low_u8(a.ex));
    vst1_u8(&t.V0[j][16], vget_low_u8(a.v));
    vst1q_u8(&t.V1[j][0], a.v);
    vst1_u8(&t.T2[j][16], vget_low_u8(c.ex));
    vst1_u8(&t.V2[j][16], vget_low_u8(c.v));
    vst1q_u8(&t.V3[j][0], c.v);
}

template <bool SimdPT>
void opus_b_impl(const uint8_t* code, size_t size, uint64_t* bits)
{
    opus_b_Strip t;
    const uint8_t* const TA = &t.TA[0][0];
    const uint8_t* const PC = &t.PC[0][0];
    const uint8_t* const PT = &t.PT[0][0];
    const uint8_t* const T0 = &t.T0[0][0];
    const uint8_t* const V0 = &t.V0[0][0];
    const uint8_t* const V1 = &t.V1[0][0];
    const uint8_t* const T2 = &t.T2[0][0];
    const uint8_t* const V2 = &t.V2[0][0];
    const uint8_t* const V3 = &t.V3[0][0];
    auto* const out = reinterpret_cast<uint32_t*>(bits);
    size_t u = 16;
    const size_t num_blocks = (size + 31) / 32;
    for (size_t q0 = 0; q0 < num_blocks; q0 += opus_b_strip)
    {
        const size_t nb = num_blocks - q0 < opus_b_strip ? num_blocks - q0 : opus_b_strip;
        for (size_t j = 0; j < nb; ++j)
            opus_b_pass1<SimdPT>(t, j, code + 32 * (q0 + j));
#ifdef __clang__
#pragma clang loop unroll(disable)
#endif
        for (size_t o = 0; o < 64 * nb; o += 64)
        {
            const size_t iu = o + u;
            const size_t ie = o + TA[iu];
            out[q0 + o / 64] = uint32_t{V0[iu]} | uint32_t{V1[o + T0[iu]]} << 8 |
                               uint32_t{V2[ie]} << 16 | uint32_t{V3[o + T2[ie]]} << 24;
            u = SimdPT ? PT[iu] : PC[ie];
        }
    }
}
}  // namespace

// bits: bit i is set iff position i is a valid JUMPDEST. Reads up to 31 bytes past the end.
void opus_b_1(const uint8_t* code, size_t size, uint64_t* bits)
{
    opus_b_impl<false>(code, size, bits);
}

void opus_b_2(const uint8_t* code, size_t size, uint64_t* bits)
{
    opus_b_impl<true>(code, size, bits);
}

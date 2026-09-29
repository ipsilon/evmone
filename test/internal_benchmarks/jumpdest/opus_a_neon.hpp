// JUMPDEST analysis for AArch64 NEON: the g8 scheme with a cheaper block stage.
//
// The group stage is g8_group with the lane offset folded into one constant (n = max(c, 0x5f) + K).
// The block entry chain skips the composed PT table: it is two table lookups, A (group A exit by
// block entry) and C (next block entry by group C entry), both with identity tails. The half-0
// tables are not stored separately: the entry < 8 test is done in scalar code.
// opus_a_1 keeps A and C in memory, opus_a_2 moves them to GPRs so the chain never touches memory.
#pragma once

namespace
{
struct opus_a_Group
{
    uint8x16_t ex;  // Half exit per entry: the next start in the group frame.
    uint8x16_t xm;  // Group exit per entry: 16..48.
    uint8x16_t v;   // JUMPDEST bits visited from each entry within its half.
};

inline opus_a_Group opus_a_group(uint8x16_t c)
{
    // K[i] = i + 1 - 0x5f (mod 256).
    static const uint8_t k_d[16] = {0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab,
        0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1};
    static const uint8_t lane_d[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    static const uint8_t hend_d[16] = {8, 8, 8, 8, 8, 8, 8, 8, 16, 16, 16, 16, 16, 16, 16, 16};
    static const uint8_t bit_d[16] = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
    const auto m = vreinterpretq_u8_s8(vmaxq_s8(vreinterpretq_s8_u8(c), vdupq_n_s8(0x5f)));
    const auto n = vaddq_u8(m, vld1q_u8(k_d));
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

// Tables of 48 bytes; each iteration overwrites the first 16 bytes only.
struct opus_a_Tables
{
    alignas(64) uint8_t A[48];   // Block frame entry into group C (16..48), by block entry.
    alignas(64) uint8_t C[48];   // Next block entry + 16, by group C entry.
    alignas(64) uint8_t Xa[48];  // Half exits of group A (only 0..7 used).
    alignas(64) uint8_t Xc[48];
    alignas(64) uint8_t Va[48];  // JUMPDEST bits of group A by entry, zero tail.
    alignas(64) uint8_t Vc[48];
    opus_a_Tables()
    {
        for (unsigned i = 0; i < 48; ++i)
        {
            A[i] = C[i] = static_cast<uint8_t>(i);
            Xa[i] = Xc[i] = Va[i] = Vc[i] = 0;
        }
    }
};

// The 16 JUMPDEST bits of a group entered at e (0..32).
// All ones if e < k (e, k <= 64), else zero. Masks keep the code free of data-dependent branches.
inline unsigned opus_a_lt(unsigned e, unsigned k)
{
    return static_cast<unsigned>(static_cast<int>(e - k) >> 31);
}

inline uint32_t opus_a_group_bits(const uint8_t* X, const uint8_t* V, unsigned e)
{
    const auto m = opus_a_lt(e, 8);
    const unsigned t = (X[e] & m) | (e & ~m);
    return (V[e] & m & 0xff) | uint32_t{V[t]} << 8;
}

// Byte i (0..15) of the 128-bit value {lo, hi}.
inline unsigned opus_a_pick(uint64_t lo, uint64_t hi, unsigned i)
{
    const auto w = lo ^ ((lo ^ hi) & (0 - uint64_t{(i >> 3) & 1}));
    return static_cast<unsigned>(w >> ((i * 8) & 63)) & 0xff;
}
}  // namespace

// bits: bit i is set iff position i is a valid JUMPDEST. Reads up to 31 bytes past the end.
void opus_a_1(const uint8_t* code, size_t size, uint64_t* bits)
{
    opus_a_Tables t;
    auto* const out = reinterpret_cast<uint32_t*>(bits);
    unsigned r = 16;  // Block entry + 16.
    const size_t num_blocks = (size + 31) / 32;
    for (size_t q = 0; q < num_blocks; ++q)
    {
        const auto a = opus_a_group(vld1q_u8(code + 32 * q));
        const auto c = opus_a_group(vld1q_u8(code + 32 * q + 16));
        vst1q_u8(t.A, a.xm);
        vst1q_u8(t.C, c.xm);
        vst1q_u8(t.Xa, a.ex);
        vst1q_u8(t.Xc, c.ex);
        vst1q_u8(t.Va, a.v);
        vst1q_u8(t.Vc, c.v);

        const unsigned s = r - 16;
        const unsigned e2 = t.A[s];
        r = t.C[e2 - 16];
        out[q] = opus_a_group_bits(t.Xa, t.Va, s) | opus_a_group_bits(t.Xc, t.Vc, e2 - 16) << 16;
    }
}

// opus_a_1 with the entry chain in GPRs: A and C are moved out of the vector registers as two
// 64-bit words each and indexed with shifts.
void opus_a_2(const uint8_t* code, size_t size, uint64_t* bits)
{
    opus_a_Tables t;
    auto* const out = reinterpret_cast<uint32_t*>(bits);
    unsigned s = 0;  // Block entry: 0..32.
    const size_t num_blocks = (size + 31) / 32;
    for (size_t q = 0; q < num_blocks; ++q)
    {
        const auto a = opus_a_group(vld1q_u8(code + 32 * q));
        const auto c = opus_a_group(vld1q_u8(code + 32 * q + 16));
        const auto xa = vreinterpretq_u64_u8(a.xm);
        const auto xc = vreinterpretq_u64_u8(c.xm);
        const auto xa0 = vgetq_lane_u64(xa, 0);
        const auto xa1 = vgetq_lane_u64(xa, 1);
        const auto xc0 = vgetq_lane_u64(xc, 0);
        const auto xc1 = vgetq_lane_u64(xc, 1);
        vst1q_u8(t.Xa, a.ex);
        vst1q_u8(t.Xc, c.ex);
        vst1q_u8(t.Va, a.v);
        vst1q_u8(t.Vc, c.v);

        const auto ma = opus_a_lt(s, 16);
        const unsigned ec = ((opus_a_pick(xa0, xa1, s) & ma) | (s & ~ma)) - 16;
        const auto mc = opus_a_lt(ec, 16);
        const unsigned sn = ((opus_a_pick(xc0, xc1, ec) & mc) | (ec & ~mc)) - 16;
        out[q] = opus_a_group_bits(t.Xa, t.Va, s) | opus_a_group_bits(t.Xc, t.Vc, ec) << 16;
        s = sn;
    }
}

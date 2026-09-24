// JUMPDEST analysis v4 for AArch64 NEON.
#pragma once

namespace
{
struct Group
{
    uint8x16_t t;  // The next group's entry offset (0..32) for each entry offset 0..15.
    uint8x16_t v;  // Valid JUMPDESTs on the walk from each entry offset: positions 0..7.
    uint8x16_t w;  // Valid JUMPDESTs on the walk from each entry offset: positions 8..15.
};

// Positions are kept raw (1..48). TBL gives 0 for an index >= 16, so a walk that left the group
// stops by itself: x = max(x, x[x]) keeps its exit and V[x] adds nothing.
inline Group analyze_group(uint8x16_t c)
{
    static const uint8_t iota1[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    static const uint8_t bit_lo[16] = {1, 2, 4, 8, 16, 32, 64, 128, 0, 0, 0, 0, 0, 0, 0, 0};
    static const uint8_t bit_hi[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 4, 8, 16, 32, 64, 128};

    // PUSH1..PUSH32 (0x60..0x7f) -> 1..32, everything else -> 0.
    const auto push_len = vreinterpretq_u8_s8(
        vmaxq_s8(vqsubq_s8(vreinterpretq_s8_u8(c), vdupq_n_s8(0x5f)), vdupq_n_s8(0)));
    auto x = vaddq_u8(vld1q_u8(iota1), push_len);  // The next instruction start.
    const auto jumpdest = vceqq_u8(c, vdupq_n_u8(0x5b));
    auto v = vandq_u8(vld1q_u8(bit_lo), jumpdest);
    auto w = vandq_u8(vld1q_u8(bit_hi), jumpdest);
    for (int k = 0; k < 4; ++k)  // 16 steps always leave the group.
    {
        v = vorrq_u8(v, vqtbl1q_u8(v, x));
        w = vorrq_u8(w, vqtbl1q_u8(w, x));
        x = vmaxq_u8(x, vqtbl1q_u8(x, x));
    }
    return {vsubq_u8(x, vdupq_n_u8(16)), v, w};
}
}  // namespace

// bits: bit i is set iff position i is a valid JUMPDEST. Reads up to 31 bytes past the end.
void g16v4_neon(const uint8_t* code, size_t size, uint64_t* bits)
{
    // Tails for entry offsets >= 16: TA, PT continue with the offset minus the skipped bytes,
    // the JUMPDEST masks are empty.
    alignas(64) uint8_t TA[48];
    alignas(64) uint8_t PT[48];
    alignas(64) uint16_t VA[48];
    alignas(64) uint16_t VB[48];
    for (unsigned i = 16; i < 48; ++i)
    {
        TA[i] = static_cast<uint8_t>(i - 16);
        VA[i] = VB[i] = 0;
    }
    for (unsigned i = 32; i < 48; ++i)
        PT[i] = static_cast<uint8_t>(i - 32);

    auto* out = reinterpret_cast<uint32_t*>(bits);
    unsigned s = 0;
    const size_t num_pairs = (size + 31) / 32;
    for (size_t q = 0; q < num_pairs; ++q)
    {
        const auto a = analyze_group(vld1q_u8(code + 32 * q));
        const auto b = analyze_group(vld1q_u8(code + 32 * q + 16));
        // The next pair's entry for s < 16: TB[TA[s]] or, if B is skipped, TA[s] - 16.
        const auto pab = vorrq_u8(vqtbl1q_u8(b.t, a.t), vqsubq_u8(a.t, vdupq_n_u8(16)));
        vst1q_u8(TA, a.t);
        vst1q_u8(PT, pab);
        vst1q_u8(PT + 16, b.t);
        vst1q_u8(reinterpret_cast<uint8_t*>(VA), vzip1q_u8(a.v, a.w));
        vst1q_u8(reinterpret_cast<uint8_t*>(VA + 8), vzip2q_u8(a.v, a.w));
        vst1q_u8(reinterpret_cast<uint8_t*>(VB), vzip1q_u8(b.v, b.w));
        vst1q_u8(reinterpret_cast<uint8_t*>(VB + 8), vzip2q_u8(b.v, b.w));

        const unsigned sb = TA[s];
        out[q] = uint32_t{VA[s]} | (uint32_t{VB[sb]} << 16);
        s = PT[s];
    }
}

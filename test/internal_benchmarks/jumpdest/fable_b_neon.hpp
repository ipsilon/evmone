// JUMPDEST analysis fable_b for AArch64 NEON: the g8 half/group scheme with a lagged scalar chain.
//
// SIMD does only the per-16-byte group work (half doubling, half merge) and stores three raw
// tables per group into a ring buffer. The block entry chain is scalar and walks the tables of a
// block stored many iterations earlier, so the chain loads never wait on in-flight vector stores.
// Group composition is not done in SIMD: the chain takes two dependent loads per 32 bytes, well
// under the SIMD throughput budget. Table tails (entries past the 16 stored ones) are constants
// filled once per call, so the chain has no compare/select.
#pragma once

namespace
{
struct fable_b_group_t
{
    uint8x16_t ex;  // Half exit per entry (group frame): A lanes 8..40, B lanes 16..48.
    uint8x16_t xm;  // Group exit per entry: 16..48 = 16 + entry into the next group.
    uint8x16_t v;   // JUMPDEST bits visited from each entry within its half.
};

inline fable_b_group_t fable_b_group(uint8x16_t c)
{
    // lane + 1 - 0x5f: y = max_s8(c, 0x5f) + iota is the next instruction start (1..48).
    static const uint8_t iota_d[16] = {0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab,
        0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1};
    static const uint8_t lane_d[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    static const uint8_t hend_d[16] = {8, 8, 8, 8, 8, 8, 8, 8, 16, 16, 16, 16, 16, 16, 16, 16};
    static const uint8_t bit_d[16] = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
    const auto y = vaddq_u8(
        vreinterpretq_u8_s8(vmaxq_s8(vreinterpretq_s8_u8(c), vdupq_n_s8(0x5f))), vld1q_u8(iota_d));
    auto x = vbslq_u8(vcltq_u8(y, vld1q_u8(hend_d)), y, vld1q_u8(lane_d));
    auto v = vandq_u8(vceqq_u8(c, vdupq_n_u8(0x5b)), vld1q_u8(bit_d));
    for (int k = 0; k < 3; ++k)
    {
        v = vorrq_u8(v, vqtbl1q_u8(v, x));
        x = vqtbl1q_u8(x, x);
    }
    const auto ex = vqtbl1q_u8(y, x);
    return {ex, vmaxq_u8(ex, vqtbl1q_u8(ex, ex)), v};
}

// Group record, indexed by the biased entry j = 16 + entry (entry 0..32):
//   V[0..15]  = v (stored), V[16..48] = 0.
//   X[0..7]   = ex A lanes (stored), X[8..15] = 8..15, X[16..31] = xm (stored), X[32..48] = 16..32.
// X[j] is the biased entry into the next group; X[j - 16] indexes V for the B-half bits.
// P (even records only, fable_b_2): P[16..31] = block exit by entry 0..15, P[32..47] = xm_c, P[48] = 16.
constexpr size_t fable_b_rec_v = 0;
constexpr size_t fable_b_rec_x = 64;
constexpr size_t fable_b_rec_p = 128;
constexpr size_t fable_b_rec_size = 192;
constexpr size_t fable_b_ring_groups = 16;
constexpr size_t fable_b_ring_size = fable_b_ring_groups * fable_b_rec_size;
constexpr size_t fable_b_lag_blocks = 7;  // Blocks between a record store and its chain loads.

inline void fable_b_init_ring(uint8_t* ring)
{
    static const uint8_t i8_d[16] = {8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};
    static const uint8_t i16_d[16] = {16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
    const auto zero = vdupq_n_u8(0);
    for (size_t g = 0; g < fable_b_ring_groups; ++g)
    {
        auto* const rec = ring + g * fable_b_rec_size;
        vst1q_u8(rec + fable_b_rec_v + 16, zero);
        vst1q_u8(rec + fable_b_rec_v + 32, zero);
        vst1q_u8(rec + fable_b_rec_v + 48, zero);
        vst1_u8(rec + fable_b_rec_x + 8, vld1_u8(i8_d));
        vst1q_u8(rec + fable_b_rec_x + 32, vld1q_u8(i16_d));
        rec[fable_b_rec_x + 48] = 32;
        rec[fable_b_rec_p + 48] = 16;
    }
}

inline void fable_b_store_group(uint8_t* rec, const fable_b_group_t& g)
{
    vst1q_u8(rec + fable_b_rec_v, g.v);
    vst1_u8(rec + fable_b_rec_x, vget_low_u8(g.ex));
    vst1q_u8(rec + fable_b_rec_x + 16, g.xm);
}

inline uint8_t* fable_b_rec(uint8_t* ring, size_t block)
{
    return ring + (2 * block & (fable_b_ring_groups - 1)) * fable_b_rec_size;
}

// A-half bits index: the entry itself for entries 0..7, else a zero byte.
inline size_t fable_b_z0(size_t j)
{
    static const uint8_t z0_d[49] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4,
        5, 6, 7, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16};
    return z0_d[j];
}

// Walks one group record from the biased entry j; returns the 2 output bytes and the next entry.
inline uint32_t fable_b_walk(const uint8_t* rec, size_t& j)
{
    const auto* const v = rec + fable_b_rec_v;
    const auto* const x = rec + fable_b_rec_x;
    const uint32_t lo = v[fable_b_z0(j)];
    const uint32_t hi = v[x[j - 16]];
    j = x[j];
    return lo | hi << 8;
}

template <bool Compose>
inline void fable_b_chain(uint8_t* ring, size_t block, size_t& j, uint32_t* out)
{
    auto* const rec = fable_b_rec(ring, block);
    const size_t j0 = j;
    const auto w0 = fable_b_walk(rec, j);
    const auto w1 = fable_b_walk(rec + fable_b_rec_size, j);
    if (Compose)
        j = rec[fable_b_rec_p + j0];
    out[block] = w0 | w1 << 16;
}

// P[j] = xm_c[xm_a[j] - 16] if xm_a[j] < 32, else xm_a[j] - 16: the block exit, biased.
inline uint8x16_t fable_b_compose(uint8x16_t xm_a, uint8x16_t xm_c)
{
    const auto ta = vsubq_u8(xm_a, vdupq_n_u8(16));
    return vmaxq_u8(vqtbl1q_u8(xm_c, ta), ta);
}

template <bool Compose>
inline void fable_b_simd(const uint8_t* code, uint8_t* ring, size_t block)
{
    auto* const rec = fable_b_rec(ring, block);
    const auto a = fable_b_group(vld1q_u8(code + 32 * block));
    const auto c = fable_b_group(vld1q_u8(code + 32 * block + 16));
    fable_b_store_group(rec, a);
    fable_b_store_group(rec + fable_b_rec_size, c);
    if (Compose)
    {
        vst1q_u8(rec + fable_b_rec_p + 16, fable_b_compose(a.xm, c.xm));
        vst1q_u8(rec + fable_b_rec_p + 32, c.xm);
    }
}

template <bool Compose>
void fable_b_impl(const uint8_t* code, size_t size, uint64_t* bits)
{
    alignas(64) uint8_t ring[fable_b_ring_size];
    fable_b_init_ring(ring);
    auto* const out = reinterpret_cast<uint32_t*>(bits);
    const size_t num_blocks = (size + 31) / 32;
    size_t j = 16;
    size_t q = 0;
    for (; q < num_blocks && q < fable_b_lag_blocks; ++q)
        fable_b_simd<Compose>(code, ring, q);
    for (; q < num_blocks; ++q)
    {
        fable_b_simd<Compose>(code, ring, q);
        fable_b_chain<Compose>(ring, q - fable_b_lag_blocks, j, out);
    }
    for (size_t r = q > fable_b_lag_blocks ? q - fable_b_lag_blocks : 0; r < num_blocks; ++r)
        fable_b_chain<Compose>(ring, r, j, out);
}
}  // namespace

// bits: bit i is set iff position i is a valid JUMPDEST. Reads up to 31 bytes past the end.
// The chain takes two dependent loads per block (no block composition in SIMD).
void fable_b_1(const uint8_t* code, size_t size, uint64_t* bits)
{
    fable_b_impl<false>(code, size, bits);
}

// Block composition in SIMD (+3 ops per block): one dependent load per block on the chain.
void fable_b_2(const uint8_t* code, size_t size, uint64_t* bits)
{
    fable_b_impl<true>(code, size, bits);
}

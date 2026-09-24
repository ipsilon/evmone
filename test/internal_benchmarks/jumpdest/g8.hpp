#pragma once
// ---------------------------------------------------------------------------------------------
// g8_sse: final SSE (x86-64-v2) kernel.
//
// A 16-byte register holds two 8-byte half-groups: A (lanes 0..7) and B (lanes 8..15).
// Pointer doubling runs inside each half, so 3 rounds cover all 8 positions and the visited
// mask of a half fits in one byte (1 pshufb per round instead of 2 byte planes).
// A position whose next instruction start leaves its half points to itself (a sink), so a
// doubling step for the pointers is one in-place pshufb. After 3 rounds x holds the sink (last
// instruction start in the half) of every entry; one more gather from N (next start, 16-byte
// group frame, encoded 0x70 + n) gives the exit, and one merge step joins A and B.
// The 32-byte block entry s is carried in the encoded form 0x80 + s; table pointers absorb the
// bias. Needs 33 zero bytes of padding after the code (over-reads at most 31 bytes).
// ---------------------------------------------------------------------------------------------

struct G8Half
{
    __m128i xm;  // 16-byte group exit per entry: 0x80 + entry into the next 16-byte group.
    __m128i xa;  // A lanes: 0x78 + entry into B (0..32); B lanes unused.
    __m128i v;   // JUMPDEST bits visited from each entry within its half (A: bits 0..7 = pos 0..7).
};

__attribute__((target("ssse3,sse4.1"), always_inline)) inline G8Half g8_half(__m128i c)
{
    // 0x79 + lane within the half - 0x5f.
    const auto iota = _mm_setr_epi8(0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x1a, 0x1b,
        0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21);
    const auto k8 = _mm_setr_epi8(8, 8, 8, 8, 8, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0);
    const auto lane = _mm_setr_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    const auto bit = _mm_setr_epi8(1, 2, 4, 8, 16, 32, 64, -128, 1, 2, 4, 8, 16, 32, 64, -128);

    // max_epi8(c, 0x5f) - 0x5f is the push data length (bytes >= 0x80 are negative).
    // y = 0x78 + next position relative to the half; negative iff it leaves the half.
    const auto y = _mm_add_epi8(_mm_max_epi8(c, _mm_set1_epi8(0x5f)), iota);
    auto n = _mm_sub_epi8(y, k8);  // 0x70 + next position in the 16-byte group.
    // Pointer: A lanes flip bit 3 so the nibble indexes A's lanes; sinks point to themselves.
    auto x = _mm_max_epi8(_mm_xor_si128(y, k8), lane);
    auto v = _mm_and_si128(_mm_cmpeq_epi8(c, _mm_set1_epi8(0x5b)), bit);
#pragma GCC unroll 3
    for (int k = 0; k < 3; ++k)
    {
        v = _mm_or_si128(v, _mm_shuffle_epi8(v, x));
        x = _mm_shuffle_epi8(x, x);
    }
    n = _mm_shuffle_epi8(n, x);  // Exit of the half: A 0x78..0x7f if in B, else >= 0x80.
    // Merge: A entries whose exit is in B take B's exit.
    return {_mm_max_epu8(n, _mm_shuffle_epi8(n, n)), n, v};
}

// Chain tables, indexed by an entry offset 0..32. Half-group tables have 8 data entries.
struct G8Tables
{
    static constexpr size_t TA = 0;    // 0x80 + entry into the 2nd 16-byte group.
    static constexpr size_t PT = 48;   // 0x80 + entry into the next 32-byte block.
    static constexpr size_t T0 = 96;   // 0x78 + entry into half 1, by entry into half 0.
    static constexpr size_t T2 = 144;  // 0x78 + entry into half 3, by entry into half 2.
    static constexpr size_t V0 = 192;  // JUMPDEST bytes of halves 0..3 (0 for entries >= 8).
    static constexpr size_t V2 = 240;
    static constexpr size_t V1 = 296;  // Written by a 16-byte store at V1 - 8.
    static constexpr size_t V3 = 344;
    alignas(64) u8 b[384];
    G8Tables()
    {
        // Only the tails (entries past the per-block data) need initialization.
        for (unsigned i = 8; i < 40; ++i)
        {
            b[T0 + i] = b[T2 + i] = static_cast<u8>(0x78 + i - 8);
            b[V0 + i] = b[V1 + i] = b[V2 + i] = b[V3 + i] = 0;
        }
        for (unsigned i = 16; i < 40; ++i)
            b[TA + i] = static_cast<u8>(0x80 + i - 16);
        for (unsigned i = 32; i < 40; ++i)
            b[PT + i] = static_cast<u8>(0x80 + i - 32);
    }
};

__attribute__((target("ssse3,sse4.1"))) void g8_sse(const u8* code, size_t size, u64* bits)
{
    using T = G8Tables;
    T t;
    u8* const b = t.b;
    auto* const out = reinterpret_cast<u8*>(bits);
    size_t e = 0x80;
    const size_t num_blocks = (size + 31) / 32;
    for (size_t q = 0; q < num_blocks; ++q)
    {
        MCA_BEGIN("g8_sse");
        const auto a = g8_half(_mm_loadu_si128(reinterpret_cast<const __m128i*>(code + 32 * q)));
        const auto c = g8_half(_mm_loadu_si128(reinterpret_cast<const __m128i*>(code + 32 * q + 16)));
        _mm_store_si128(reinterpret_cast<__m128i*>(b + T::TA), a.xm);
        _mm_store_si128(reinterpret_cast<__m128i*>(b + T::PT + 16), c.xm);
        // Block composition is one more doubling step: PT[s] = c.xm[a.xm[s] - 16] or a.xm[s] - 16.
        const auto ia = _mm_sub_epi8(a.xm, _mm_set1_epi8(0x10));
        _mm_store_si128(reinterpret_cast<__m128i*>(b + T::PT),
            _mm_max_epu8(_mm_shuffle_epi8(c.xm, ia), ia));
        _mm_storel_epi64(reinterpret_cast<__m128i*>(b + T::T0), a.xa);
        _mm_storel_epi64(reinterpret_cast<__m128i*>(b + T::T2), c.xa);
        _mm_storel_epi64(reinterpret_cast<__m128i*>(b + T::V0), a.v);
        _mm_storel_epi64(reinterpret_cast<__m128i*>(b + T::V2), c.v);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(b + T::V1 - 8), a.v);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(b + T::V3 - 8), c.v);

        // Entries are encoded as 0x80 + s (0x78 + s for T0/T2 values), so indexes never wrap.
        const size_t e1 = b[e - 0x80 + T::T0];
        const size_t e2 = b[e - 0x80 + T::TA];
        const size_t e3 = b[e2 - 0x80 + T::T2];
        out[4 * q + 0] = b[e - 0x80 + T::V0];
        out[4 * q + 1] = b[e1 - 0x78 + T::V1];
        out[4 * q + 2] = b[e2 - 0x80 + T::V2];
        out[4 * q + 3] = b[e3 - 0x78 + T::V3];
        e = b[e - 0x80 + T::PT];  // The only loop-carried dependency.
        MCA_END("g8_sse");
    }
}

// Experiment: no block composition; the chain takes 2 dependent loads per 32 bytes.
__attribute__((target("ssse3,sse4.1"))) void g8nc_sse(const u8* code, size_t size, u64* bits)
{
    using T = G8Tables;
    T t;
    u8* const b = t.b;
    // PT is used as the 2nd group's exit table: entry into the 2nd group -> next block.
    for (unsigned i = 16; i < 40; ++i)
        b[T::PT + i] = static_cast<u8>(0x80 + i - 16);
    auto* const out = reinterpret_cast<u8*>(bits);
    size_t e = 0x80;
    const size_t num_blocks = (size + 31) / 32;
    for (size_t q = 0; q < num_blocks; ++q)
    {
        MCA_BEGIN("g8nc_sse");
        const auto a = g8_half(_mm_loadu_si128(reinterpret_cast<const __m128i*>(code + 32 * q)));
        const auto c = g8_half(_mm_loadu_si128(reinterpret_cast<const __m128i*>(code + 32 * q + 16)));
        _mm_store_si128(reinterpret_cast<__m128i*>(b + T::TA), a.xm);
        _mm_store_si128(reinterpret_cast<__m128i*>(b + T::PT), c.xm);
        _mm_storel_epi64(reinterpret_cast<__m128i*>(b + T::T0), a.xa);
        _mm_storel_epi64(reinterpret_cast<__m128i*>(b + T::T2), c.xa);
        _mm_storel_epi64(reinterpret_cast<__m128i*>(b + T::V0), a.v);
        _mm_storel_epi64(reinterpret_cast<__m128i*>(b + T::V2), c.v);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(b + T::V1 - 8), a.v);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(b + T::V3 - 8), c.v);
        const size_t e1 = b[e - 0x80 + T::T0];
        const size_t e2 = b[e - 0x80 + T::TA];
        const size_t e3 = b[e2 - 0x80 + T::T2];
        out[4 * q + 0] = b[e - 0x80 + T::V0];
        out[4 * q + 1] = b[e1 - 0x78 + T::V1];
        out[4 * q + 2] = b[e2 - 0x80 + T::V2];
        out[4 * q + 3] = b[e3 - 0x78 + T::V3];
        e = b[e2 - 0x80 + T::PT];
        MCA_END("g8nc_sse");
    }
}

// ---------------------------------------------------------------------------------------------
// g8_avx2: the same scheme for x86-64-v3; the 32-byte block is one ymm. Without SSE's register
// copies the visited bytes are cheaper to merge in SIMD into 16-bit tables (as in v4).
// ---------------------------------------------------------------------------------------------
struct G8TablesAvx2
{
    alignas(64) u8 TA[48];        // 0x80 + entry into the 2nd 16-byte group.
    alignas(64) u8 PT[48];        // 0x80 + entry into the next 32-byte block.
    alignas(64) uint16_t VA[48];  // JUMPDEST bits of the 1st 16-byte group (0 for entries >= 16).
    alignas(64) uint16_t VB[48];  // JUMPDEST bits of the 2nd 16-byte group (0 for entries >= 16).
    G8TablesAvx2()
    {
        for (unsigned i = 16; i < 40; ++i)
        {
            TA[i] = static_cast<u8>(0x80 + i - 16);
            VA[i] = VB[i] = 0;
        }
        for (unsigned i = 32; i < 40; ++i)
            PT[i] = static_cast<u8>(0x80 + i - 32);
    }
};

__attribute__((target("avx2"))) void g8_avx2(const u8* code, size_t size, u64* bits)
{
    const auto iota = _mm256_broadcastsi128_si256(_mm_setr_epi8(
        0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21));
    const auto k8 = _mm256_broadcastsi128_si256(_mm_setr_epi8(8, 8, 8, 8, 8, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0));
    const auto lane = _mm256_broadcastsi128_si256(
        _mm_setr_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15));
    const auto bit = _mm256_broadcastsi128_si256(
        _mm_setr_epi8(1, 2, 4, 8, 16, 32, 64, -128, 1, 2, 4, 8, 16, 32, 64, -128));
    G8TablesAvx2 t;
    auto* const out = reinterpret_cast<uint16_t*>(bits);
    size_t e = 0x80;
    const size_t num_blocks = (size + 31) / 32;
    for (size_t q = 0; q < num_blocks; ++q)
    {
        MCA_BEGIN("g8_avx2");
        const auto c = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(code + 32 * q));
        const auto y = _mm256_add_epi8(_mm256_max_epi8(c, _mm256_set1_epi8(0x5f)), iota);
        auto n = _mm256_sub_epi8(y, k8);
        auto x = _mm256_max_epi8(_mm256_xor_si256(y, k8), lane);
        auto v = _mm256_and_si256(_mm256_cmpeq_epi8(c, _mm256_set1_epi8(0x5b)), bit);
        for (int k = 0; k < 3; ++k)
        {
            v = _mm256_or_si256(v, _mm256_shuffle_epi8(v, x));
            x = _mm256_shuffle_epi8(x, x);
        }
        n = _mm256_shuffle_epi8(n, x);
        const auto xm = _mm256_max_epu8(n, _mm256_shuffle_epi8(n, n));  // [TA | TB]
        const auto h = _mm256_shuffle_epi8(v, n);  // A lanes: B's JUMPDEST bits if the exit is in B.
        const auto vl = _mm256_unpacklo_epi8(v, h);                       // [A 0..7 | B 0..7]
        const auto vh = _mm256_unpackhi_epi8(_mm256_setzero_si256(), v);  // [A 8..15 | B 8..15]
        const auto ia = _mm256_sub_epi8(xm, _mm256_set1_epi8(0x10));
        const auto pab = _mm256_max_epu8(_mm256_shuffle_epi8(_mm256_permute4x64_epi64(xm, 0xEE), ia), ia);
        _mm_store_si128(reinterpret_cast<__m128i*>(t.TA), _mm256_castsi256_si128(xm));
        _mm256_store_si256(reinterpret_cast<__m256i*>(t.PT), _mm256_blend_epi32(xm, pab, 0x0F));
        _mm256_store_si256(reinterpret_cast<__m256i*>(t.VA), _mm256_permute2x128_si256(vl, vh, 0x20));
        _mm256_store_si256(reinterpret_cast<__m256i*>(t.VB), _mm256_permute2x128_si256(vl, vh, 0x31));

        const size_t e2 = t.TA[e - 0x80];
        out[2 * q] = t.VA[e - 0x80];
        out[2 * q + 1] = t.VB[e2 - 0x80];
        e = t.PT[e - 0x80];
        MCA_END("g8_avx2");
    }
}

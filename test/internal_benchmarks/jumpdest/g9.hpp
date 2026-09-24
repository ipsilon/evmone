#pragma once
// g9_avx2: JUMPDEST analysis, x86-64-v3. Combines the other agent's o8_avx2 front-end and scalar
// chain with a 2-stage ring-buffer pipeline and permute-free table stores.
//
// Front-end (per 32-byte pair, one ymm; lane = 16-byte group, two 8-byte halves per lane):
//   y = max_epi8(c, 0x5f) + iota = 0x78 + next start relative to the half (negative if it leaves).
//   x = pointer to the next start inside the half; one that leaves points to itself (a sink).
//   3 rounds of x = x[x], v |= v[x] give the last start in the half and the JUMPDEST bits visited.
//   n[x] is the exit of the half; one more step joins the halves: xm = 16-byte group exits.
// Back-end: v4-style tables (PT, TA, VA, VB) indexed by the entry offset (encoded 0x80 + s), and a
// scalar chain with one dependent byte load per 32 bytes. The tables of pair q+2 are built while
// the chain reads the tables of pair q, so no chain load depends on a store of the same iteration
// (only the TB reload in store() does, off the loop-carried path).
// Requires: code 32-byte aligned, at least 33 zero bytes after the code (reads at most 31 past
// the end). Writes one uint32 per 32 bytes: (size + 64) / 64 words suffice.
namespace g9
{
// Slot layout (256 bytes), tables indexed by the entry offset s = 0..32:
constexpr size_t PT = 0;    // u8: 0x80 + next pair entry; [16,32) = TB; tail s >= 32: 0x80 + s - 32
constexpr size_t TA = 48;   // u8: 0x80 + entry into group B; tail s >= 16: 0x80 + s - 16
constexpr size_t VA = 96;   // u16: JUMPDEST bits of group A (0 for s >= 16)
constexpr size_t VB = 176;  // u16: JUMPDEST bits of group B (0 for s >= 16)
constexpr size_t SLOT = 256;

inline void init_tails(u8* s)
{
    for (unsigned i = 16; i < 48; ++i)
        s[TA + i] = static_cast<u8>(0x80 + i - 16);
    for (unsigned i = 32; i < 48; ++i)
        s[PT + i] = static_cast<u8>(0x80 + i - 32);
    std::memset(s + VA + 32, 0, 48);
    std::memset(s + VB + 32, 0, 48);
}

struct Tables
{
    __m256i xm, vl, vh;
};

__attribute__((target("avx2"), always_inline)) inline Tables compute(const u8* p)
{
    // 0x79 + position in the half - 0x5f.
    const auto iota = _mm256_setr_epi8(0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x1a, 0x1b,
        0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x1a,
        0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21);
    const auto k8 = _mm256_setr_epi8(8, 8, 8, 8, 8, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 8, 8, 8,
        8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0);
    const auto lane = _mm256_setr_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1,
        2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    const auto bit = _mm256_setr_epi8(1, 2, 4, 8, 16, 32, 64, -128, 1, 2, 4, 8, 16, 32, 64, -128,
        1, 2, 4, 8, 16, 32, 64, -128, 1, 2, 4, 8, 16, 32, 64, -128);

    const auto c = _mm256_load_si256(reinterpret_cast<const __m256i*>(p));
    const auto y = _mm256_add_epi8(_mm256_max_epi8(c, _mm256_set1_epi8(0x5f)), iota);
    auto n = _mm256_sub_epi8(y, k8);                          // 0x70 + next start in the group
    auto x = _mm256_max_epi8(_mm256_xor_si256(y, k8), lane);  // in-half pointer, or a sink
    auto v = _mm256_and_si256(_mm256_cmpeq_epi8(c, _mm256_set1_epi8(0x5b)), bit);
    for (int i = 0; i < 3; ++i)
    {
        v = _mm256_or_si256(v, _mm256_shuffle_epi8(v, x));
        x = _mm256_shuffle_epi8(x, x);
    }
    n = _mm256_shuffle_epi8(n, x);  // Exit of the half (1st half: < 0x80 if it lands in the 2nd).
    const auto xm = _mm256_max_epu8(n, _mm256_shuffle_epi8(n, n));  // [TA | TB] = 0x80 + entry
    const auto h = _mm256_shuffle_epi8(v, n);  // 2nd-half bits for 1st-half entries (else 0)
    return {xm, _mm256_unpacklo_epi8(v, h), _mm256_unpackhi_epi8(h, v)};
}

__attribute__((target("avx2"), always_inline)) inline void store(u8* s, const Tables& t)
{
    _mm_store_si128(reinterpret_cast<__m128i*>(s + TA), _mm256_castsi256_si128(t.xm));
    // PT = [TB[TA[s]] or TA[s] - 16 | TB]; TB is read back from memory (no lane-crossing op).
    _mm256_store_si256(reinterpret_cast<__m256i*>(s + PT), t.xm);
    asm("" : "+m"(*reinterpret_cast<u8(*)[32]>(s + PT)));
    const auto tb = _mm_load_si128(reinterpret_cast<const __m128i*>(s + PT + 16));
    const auto ia = _mm_sub_epi8(_mm256_castsi256_si128(t.xm), _mm_set1_epi8(0x10));
    _mm_store_si128(reinterpret_cast<__m128i*>(s + PT), _mm_max_epu8(_mm_shuffle_epi8(tb, ia), ia));
    // VA = [lo(vl) | lo(vh)], VB = [hi(vl) | hi(vh)] as 16-byte stores (no vperm2i128).
    _mm_store_si128(reinterpret_cast<__m128i*>(s + VA), _mm256_castsi256_si128(t.vl));
    asm("" : "+m"(*reinterpret_cast<u8(*)[16]>(s + VA)));
    _mm_store_si128(reinterpret_cast<__m128i*>(s + VA + 16), _mm256_castsi256_si128(t.vh));
    _mm_store_si128(reinterpret_cast<__m128i*>(s + VB), _mm256_extracti128_si256(t.vl, 1));
    asm("" : "+m"(*reinterpret_cast<u8(*)[16]>(s + VB)));
    _mm_store_si128(reinterpret_cast<__m128i*>(s + VB + 16), _mm256_extracti128_si256(t.vh, 1));
}

// Output for the pair whose tables are in slot s, entered at 0x80 + entry e; returns the next e.
inline size_t chain(const u8* s, size_t e, uint16_t* out)
{
    const size_t e2 = s[TA + e - 0x80];
    uint16_t lo, hi;
    std::memcpy(&lo, s + VA + 2 * (e - 0x80), 2);
    std::memcpy(&hi, s + VB + 2 * (e2 - 0x80), 2);
    out[0] = lo;
    out[1] = hi;
    return s[PT + e - 0x80];
}
}  // namespace g9

__attribute__((target("avx2"))) void g9_avx2(const u8* code, size_t size, u64* bits)
{
    const size_t n = (size + 31) / 32;
    if (n == 0)
        return;
    alignas(2 * g9::SLOT) u8 buf[2 * g9::SLOT];  // Two slots; slot ^ SLOT flips between them.
    g9::init_tails(buf);
    g9::init_tails(buf + g9::SLOT);
    for (size_t i = 0; i < n && i < 2; ++i)
        g9::store(buf + g9::SLOT * i, g9::compute(code + 32 * i));

    auto* const out = reinterpret_cast<uint16_t*>(bits);
    u8* slot = buf;
    size_t e = 0x80;  // 0x80 + entry offset into the current pair
    size_t q = 0;
    for (; q + 2 < n; ++q)
    {
        const auto t = g9::compute(code + 32 * (q + 2));
        e = g9::chain(slot, e, out + 2 * q);
        g9::store(slot, t);
        slot = reinterpret_cast<u8*>(reinterpret_cast<uintptr_t>(slot) ^ g9::SLOT);
    }
    for (; q < n; ++q)
    {
        e = g9::chain(slot, e, out + 2 * q);
        slot = reinterpret_cast<u8*>(reinterpret_cast<uintptr_t>(slot) ^ g9::SLOT);
    }
}

// g9np_avx2: g9_avx2 without the pipeline (tables built and read in the same iteration).
__attribute__((target("avx2"))) void g9np_avx2(const u8* code, size_t size, u64* bits)
{
    const size_t n = (size + 31) / 32;
    alignas(64) u8 slot[g9::SLOT];
    g9::init_tails(slot);
    auto* const out = reinterpret_cast<uint16_t*>(bits);
    size_t e = 0x80;
    for (size_t q = 0; q < n; ++q)
    {
        g9::store(slot, g9::compute(code + 32 * q));
        e = g9::chain(slot, e, out + 2 * q);
    }
}

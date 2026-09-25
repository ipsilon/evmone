// c8: the g8 doubling core with gapped 16-bit word tables written by plain ymm stores (no lane
// permutes), one block per iteration, with the next block's code loaded one iteration ahead.
namespace r2
{
#define R2_BARRIER(p, n) asm("" : "+m"(*reinterpret_cast<u8(*)[n]>(p)))
#define R2_I __attribute__((target("avx2,bmi,bmi2"), always_inline)) inline

struct Core
{
    __m256i xm, lo, hi;
};

// Doubling core (g8/g9): xm = [TA | TB] (0x80 + entry into the next group), lo/hi = 16-bit
// JUMPDEST words of entries 0..7 / 8..15 of both groups.
R2_I Core core_v(__m256i c);
R2_I __m256i ld(const u8* p) { return JDA_LOAD256(p); }
R2_I Core core(const u8* p) { return core_v(ld(p)); }
R2_I Core core_v(__m256i c)
{
    const auto iota = _mm256_broadcastsi128_si256(_mm_setr_epi8(
        0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21));
    const auto k8 = _mm256_broadcastsi128_si256(_mm_setr_epi8(8, 8, 8, 8, 8, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0));
    const auto lane = _mm256_broadcastsi128_si256(_mm_setr_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15));
    const auto bit = _mm256_broadcastsi128_si256(_mm_setr_epi8(1, 2, 4, 8, 16, 32, 64, -128, 1, 2, 4, 8, 16, 32, 64, -128));
    const auto y = _mm256_add_epi8(_mm256_max_epi8(c, _mm256_set1_epi8(0x5f)), iota);
    auto n = _mm256_sub_epi8(y, k8);
    auto x = _mm256_max_epi8(_mm256_xor_si256(y, k8), lane);
    auto v = _mm256_and_si256(_mm256_cmpeq_epi8(c, _mm256_set1_epi8(0x5b)), bit);
#pragma GCC unroll 3
    for (int k = 0; k < 3; ++k)
    {
        v = _mm256_or_si256(v, _mm256_shuffle_epi8(v, x));
        x = _mm256_shuffle_epi8(x, x);
    }
    n = _mm256_shuffle_epi8(n, x);
    const auto xm = _mm256_max_epu8(n, _mm256_shuffle_epi8(n, n));
    const auto h = _mm256_shuffle_epi8(v, n);
    return {xm, _mm256_unpacklo_epi8(v, h), _mm256_unpackhi_epi8(h, v)};
}

constexpr size_t TA = 0, PT = 64;
constexpr size_t VG = 128;  // [VG, VG + 64) data, [VG + 64, VG + 160) zeros.

// C8: gapped word tables: vl at VG, vh at VG + 32 (2 ymm stores); the word index of entry s is
// s + (s & 0x38) from a static byte table (entries 8..15 -> 16..23, >= 16 -> zeros).
constexpr size_t X = 288;   // Scratch [TA | TB] (CLEAN_PT only).
constexpr size_t GT = 336;  // GT[e - 0x80] = 0x80 + gap(e - 0x80), e - 0x80 in [0, 48).
struct alignas(64) Tables8
{
    u8 b[384];
    Tables8()
    {
        std::memset(b, 0, sizeof(b));
        for (unsigned i = 16; i < 40; ++i)
            b[TA + i] = static_cast<u8>(0x80 + i - 16);
        for (unsigned i = 32; i < 40; ++i)
            b[PT + i] = static_cast<u8>(0x80 + i - 32);
        for (unsigned i = 0; i < 48; ++i)
            b[GT + i] = static_cast<u8>(0x80 + i + (i & 0x38));
    }
};
template <bool CLEAN_PT>
R2_I void finish_c8(Core g, u8* b, size_t& e, uint16_t* out)
{
    _mm_store_si128(reinterpret_cast<__m128i*>(b + TA), _mm256_castsi256_si128(g.xm));
    if constexpr (CLEAN_PT)
    {
        // [TA | TB] goes to a scratch slot, so PT is written by two non-overlapping stores.
        _mm256_store_si256(reinterpret_cast<__m256i*>(b + X), g.xm);
        R2_BARRIER(b + X, 32);
        const auto tb = _mm_load_si128(reinterpret_cast<const __m128i*>(b + X + 16));
        const auto ia = _mm_sub_epi8(_mm256_castsi256_si128(g.xm), _mm_set1_epi8(0x10));
        _mm_store_si128(reinterpret_cast<__m128i*>(b + PT), _mm_max_epu8(_mm_shuffle_epi8(tb, ia), ia));
        _mm_store_si128(reinterpret_cast<__m128i*>(b + PT + 16), tb);
    }
    else
    {
    _mm256_store_si256(reinterpret_cast<__m256i*>(b + PT), g.xm);
    R2_BARRIER(b + PT, 32);
    const auto tb = _mm_load_si128(reinterpret_cast<const __m128i*>(b + PT + 16));
    const auto ia = _mm_sub_epi8(_mm256_castsi256_si128(g.xm), _mm_set1_epi8(0x10));
    _mm_store_si128(reinterpret_cast<__m128i*>(b + PT), _mm_max_epu8(_mm_shuffle_epi8(tb, ia), ia));
    }
    _mm256_store_si256(reinterpret_cast<__m256i*>(b + VG), g.lo);
    _mm256_store_si256(reinterpret_cast<__m256i*>(b + VG + 32), g.hi);
    const size_t e2 = b[e - 0x80 + TA];
    const size_t ga = b[GT + e - 0x80], gb = b[GT + e2 - 0x80];
    out[0] = *reinterpret_cast<const uint16_t*>(b + VG + 2 * ga - 0x100);
    out[1] = *reinterpret_cast<const uint16_t*>(b + VG + 16 + 2 * gb - 0x100);
    e = b[e - 0x80 + PT];
}
}  // namespace r2

// The load-ahead reads up to 63 bytes past the end.
#define R2_DRIVER(NAME, CORE_V, FINISH, TABLES)                                             \
    __attribute__((target("avx2,bmi,bmi2"))) void NAME(const u8* code, size_t size, u64* bits) \
    {                                                                                          \
        TABLES t;                                                                              \
        size_t e = 0x80;                                                                       \
        const size_t nb = (size + 31) / 32;                                                    \
        auto c = r2::ld(code);                                                                 \
        _Pragma("GCC unroll 1") for (size_t q = 0; q < nb; ++q)                                \
        {                                                                                      \
            const auto a = CORE_V(c);                                                          \
            c = r2::ld(code + 32 * q + 32);                                                    \
            FINISH(a, t.b, e, reinterpret_cast<uint16_t*>(bits) + 2 * q);                      \
        }                                                                                      \
    }
R2_DRIVER(c8_avx2, r2::core_v, r2::finish_c8<false>, r2::Tables8)
R2_DRIVER(c8f_avx2, r2::core_v, r2::finish_c8<true>, r2::Tables8)

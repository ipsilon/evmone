// Round 2 AVX2 kernels (Opus): c8 (x2 loop), c9 (x4 loop), c10 (x4 loop, paired block
// composition) and d6 (byte-plane tables with the c10 composition). See ROUND2.md.
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

constexpr size_t TA = 0, PT = 64, VA = 128, VB = 256;
constexpr size_t VG = 128;  // [VG, VG + 64) data, [VG + 64, VG + 160) zeros.

// D: byte planes instead of 16-bit words (no h/unpack: 10 shuffles). hi = pshufb(v, max(n, lb))
// = [h_A v_A1 | h_B v_B1], lo = v with the second half of each group cleared. Planes are stored
// as ymm and indexed through a static table (entries >= 16 hit zeros). A word is a 16-bit load
// of the hi plane at g - 1 with the low byte replaced.
struct CoreD
{
    __m256i xm, lo, hi;
};
R2_I CoreD core_dv(__m256i c);
R2_I CoreD core_d(const u8* p) { return core_dv(ld(p)); }
R2_I CoreD core_dv(__m256i c)
{
    const auto iota = _mm256_broadcastsi128_si256(_mm_setr_epi8(
        0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21));
    const auto k8 = _mm256_broadcastsi128_si256(_mm_setr_epi8(8, 8, 8, 8, 8, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0));
    const auto lane = _mm256_broadcastsi128_si256(_mm_setr_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15));
    const auto bit = _mm256_broadcastsi128_si256(_mm_setr_epi8(1, 2, 4, 8, 16, 32, 64, -128, 1, 2, 4, 8, 16, 32, 64, -128));
    const auto lb = _mm256_broadcastsi128_si256(_mm_setr_epi8(-128, -128, -128, -128, -128, -128, -128, -128, 8, 9, 10, 11, 12, 13, 14, 15));
    const auto m8 = _mm256_broadcastsi128_si256(_mm_setr_epi8(-1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0));
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
    return {xm, _mm256_and_si256(v, m8), _mm256_shuffle_epi8(v, _mm256_max_epi8(n, lb))};
}
}  // namespace r2

namespace r2
{
// C8: gapped word tables: vl at VG, vh at VG + 32 (2 ymm stores); the word index of entry s is
// s + (s & 0x38) from a static byte table (entries 8..15 -> 16..23, >= 16 -> zeros).
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
R2_I void finish_c8(Core g, u8* b, size_t& e, uint16_t* out)
{
    _mm_store_si128(reinterpret_cast<__m128i*>(b + TA), _mm256_castsi256_si128(g.xm));
    _mm256_store_si256(reinterpret_cast<__m256i*>(b + PT), g.xm);
    R2_BARRIER(b + PT, 32);
    const auto tb = _mm_load_si128(reinterpret_cast<const __m128i*>(b + PT + 16));
    const auto ia = _mm_sub_epi8(_mm256_castsi256_si128(g.xm), _mm_set1_epi8(0x10));
    _mm_store_si128(reinterpret_cast<__m128i*>(b + PT), _mm_max_epu8(_mm_shuffle_epi8(tb, ia), ia));
    _mm256_store_si256(reinterpret_cast<__m256i*>(b + VG), g.lo);
    _mm256_store_si256(reinterpret_cast<__m256i*>(b + VG + 32), g.hi);
    const size_t e2 = b[e - 0x80 + TA];
    const size_t ga = b[GT + e - 0x80], gb = b[GT + e2 - 0x80];
    out[0] = *reinterpret_cast<const uint16_t*>(b + VG + 2 * ga - 0x100);
    out[1] = *reinterpret_cast<const uint16_t*>(b + VG + 16 + 2 * gb - 0x100);
    e = b[e - 0x80 + PT];
}
}  // namespace r2
namespace r2
{
// C10: two table slots; the block compositions of a block pair share one ymm pshufb.
// Slot layout (320 B): TA [0,40), PT [64,104) ([pab | TB | tails]), VG [128, 274); GT after
// both slots. The lane-1 pab is stored with a ymm store at PT - 16 of slot 1 (dead space).
constexpr size_t SLOT = 320, GT2 = 2 * SLOT;
struct alignas(64) Tables10
{
    u8 b[2 * SLOT + 64];
    Tables10()
    {
        std::memset(b, 0, sizeof(b));
        for (size_t s = 0; s < 2 * SLOT; s += SLOT)
        {
            for (unsigned i = 16; i < 40; ++i)
                b[s + TA + i] = static_cast<u8>(0x80 + i - 16);
            for (unsigned i = 32; i < 40; ++i)
                b[s + PT + i] = static_cast<u8>(0x80 + i - 32);
        }
        for (unsigned i = 0; i < 48; ++i)
            b[GT2 + i] = static_cast<u8>(0x80 + i + (i & 0x38));
    }
};
R2_I void store10(Core g, u8* s)
{
    _mm_store_si128(reinterpret_cast<__m128i*>(s + TA), _mm256_castsi256_si128(g.xm));
    _mm256_store_si256(reinterpret_cast<__m256i*>(s + PT), g.xm);
    _mm256_store_si256(reinterpret_cast<__m256i*>(s + VG), g.lo);
    _mm256_store_si256(reinterpret_cast<__m256i*>(s + VG + 32), g.hi);
}
R2_I void compose10(Core g0, u8* b)
{
    R2_BARRIER(b, 2 * SLOT);
    const auto tb = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_load_si128(reinterpret_cast<const __m128i*>(b + PT + 16))),
        _mm_load_si128(reinterpret_cast<const __m128i*>(b + SLOT + PT + 16)), 1);
    const auto ta = _mm256_inserti128_si256(
        g0.xm, _mm_load_si128(reinterpret_cast<const __m128i*>(b + SLOT + TA)), 1);
    const auto ia = _mm256_sub_epi8(ta, _mm256_set1_epi8(0x10));
    const auto pab = _mm256_max_epu8(_mm256_shuffle_epi8(tb, ia), ia);
    _mm_store_si128(reinterpret_cast<__m128i*>(b + PT), _mm256_castsi256_si128(pab));
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(b + SLOT + PT - 16), pab);
}
R2_I void chain10(const u8* s, const u8* b, size_t& e, uint16_t* out)
{
    const size_t e2 = s[e - 0x80 + TA];
    const size_t ga = b[GT2 + e - 0x80], gb = b[GT2 + e2 - 0x80];
    out[0] = *reinterpret_cast<const uint16_t*>(s + VG + 2 * ga - 0x100);
    out[1] = *reinterpret_cast<const uint16_t*>(s + VG + 16 + 2 * gb - 0x100);
    e = s[e - 0x80 + PT];
}
}  // namespace r2

// x4 (two pairs) with load-ahead; see R2_DRIVER_NP4.
__attribute__((target("avx2,bmi,bmi2"))) void c10_avx2(const u8* code, size_t size, u64* bits)
{
    r2::Tables10 t;
    u8* const b = t.b;
    size_t e = 0x80;
    const size_t nb = (size + 31) / 32;
    const ptrdiff_t K = nb < 2 ? 0 : static_cast<ptrdiff_t>((nb - 2) / 4);
    const u8* const cb = code + 128 * K;
    u8* const ob = reinterpret_cast<u8*>(bits) + 16 * K;
    if (K != 0)
    {
        auto c0 = r2::ld(code), c1 = r2::ld(code + 32);
        for (ptrdiff_t i = -16 * K; i != 0; i += 16)
        {
            const auto a0 = r2::core_v(c0);
            c0 = r2::ld(cb + 8 * i + 64);
            r2::store10(a0, b);
            const auto a1 = r2::core_v(c1);
            c1 = r2::ld(cb + 8 * i + 96);
            r2::store10(a1, b + r2::SLOT);
            r2::compose10(a0, b);
            r2::chain10(b, b, e, reinterpret_cast<uint16_t*>(ob + i));
            r2::chain10(b + r2::SLOT, b, e, reinterpret_cast<uint16_t*>(ob + i + 4));
            const auto a2 = r2::core_v(c0);
            c0 = r2::ld(cb + 8 * i + 128);
            r2::store10(a2, b);
            const auto a3 = r2::core_v(c1);
            c1 = r2::ld(cb + 8 * i + 160);
            r2::store10(a3, b + r2::SLOT);
            r2::compose10(a2, b);
            r2::chain10(b, b, e, reinterpret_cast<uint16_t*>(ob + i + 8));
            r2::chain10(b + r2::SLOT, b, e, reinterpret_cast<uint16_t*>(ob + i + 12));
        }
    }
    const u8* p = cb;
    uint16_t* o = reinterpret_cast<uint16_t*>(ob);
    for (size_t r = nb - 4 * static_cast<size_t>(K); r != 0; --r, p += 32, o += 2)
    {
        const auto a = r2::core_v(r2::ld(p));
        r2::store10(a, b);
        r2::store10(a, b + r2::SLOT);
        r2::compose10(a, b);
        r2::chain10(b, b, e, o);
    }
}
namespace r2
{
// D6: D-style byte planes + C10's paired composition. Slot: TA [0,40), PT [64,104),
// PL [128, 224), PH [224, 320) (32 data + zeros up to +81); GT16 after both slots.
constexpr size_t SLOT6 = 320, PL6 = 128, PH6 = 224, GT6 = 2 * SLOT6;
struct alignas(64) Tables6
{
    u8 b[2 * SLOT6 + 64];
    Tables6()
    {
        std::memset(b, 0, sizeof(b));
        for (size_t s = 0; s < 2 * SLOT6; s += SLOT6)
        {
            for (unsigned i = 16; i < 40; ++i)
                b[s + TA + i] = static_cast<u8>(0x80 + i - 16);
            for (unsigned i = 32; i < 40; ++i)
                b[s + PT + i] = static_cast<u8>(0x80 + i - 32);
        }
        for (unsigned i = 0; i < 48; ++i)
            b[GT6 + i] = static_cast<u8>(i + (i & 0x30));
    }
};
R2_I void store6(CoreD g, u8* s)
{
    _mm_store_si128(reinterpret_cast<__m128i*>(s + TA), _mm256_castsi256_si128(g.xm));
    _mm256_store_si256(reinterpret_cast<__m256i*>(s + PT), g.xm);
    _mm256_store_si256(reinterpret_cast<__m256i*>(s + PL6), g.lo);
    _mm256_store_si256(reinterpret_cast<__m256i*>(s + PH6), g.hi);
}
R2_I void compose6(CoreD g0, u8* b)
{
    R2_BARRIER(b, 2 * SLOT6);
    const auto tb = _mm256_inserti128_si256(
        _mm256_castsi128_si256(_mm_load_si128(reinterpret_cast<const __m128i*>(b + PT + 16))),
        _mm_load_si128(reinterpret_cast<const __m128i*>(b + SLOT6 + PT + 16)), 1);
    const auto ta = _mm256_inserti128_si256(
        g0.xm, _mm_load_si128(reinterpret_cast<const __m128i*>(b + SLOT6 + TA)), 1);
    const auto ia = _mm256_sub_epi8(ta, _mm256_set1_epi8(0x10));
    const auto pab = _mm256_max_epu8(_mm256_shuffle_epi8(tb, ia), ia);
    _mm_store_si128(reinterpret_cast<__m128i*>(b + PT), _mm256_castsi256_si128(pab));
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(b + SLOT6 + PT - 16), pab);
}
R2_I uint16_t word6(const u8* s, size_t g)
{
    uint16_t w;
    std::memcpy(&w, s + PH6 - 1 + g, 2);
    asm("movb %1, %b0" : "+r"(w) : "m"(s[PL6 + g]));
    return w;
}
R2_I void chain6(const u8* s, const u8* b, size_t& e, uint16_t* out)
{
    const size_t e2 = s[e - 0x80 + TA];
    const size_t ga = b[GT6 + e - 0x80], gb = b[GT6 + e2 - 0x80] + 16;
    out[0] = word6(s, ga);
    out[1] = word6(s, gb);
    e = s[e - 0x80 + PT];
}
}  // namespace r2

__attribute__((target("avx2,bmi,bmi2"))) void d6_avx2(const u8* code, size_t size, u64* bits)
{
    r2::Tables6 t;
    u8* const b = t.b;
    size_t e = 0x80;
    const size_t nb = (size + 31) / 32;
    const ptrdiff_t K = nb < 2 ? 0 : static_cast<ptrdiff_t>((nb - 2) / 4);
    const u8* const cb = code + 128 * K;
    u8* const ob = reinterpret_cast<u8*>(bits) + 16 * K;
    if (K != 0)
    {
        auto c0 = r2::ld(code), c1 = r2::ld(code + 32);
        for (ptrdiff_t i = -16 * K; i != 0; i += 16)
        {
            const auto a0 = r2::core_dv(c0);
            c0 = r2::ld(cb + 8 * i + 64);
            r2::store6(a0, b);
            const auto a1 = r2::core_dv(c1);
            c1 = r2::ld(cb + 8 * i + 96);
            r2::store6(a1, b + r2::SLOT6);
            r2::compose6(a0, b);
            r2::chain6(b, b, e, reinterpret_cast<uint16_t*>(ob + i));
            r2::chain6(b + r2::SLOT6, b, e, reinterpret_cast<uint16_t*>(ob + i + 4));
            const auto a2 = r2::core_dv(c0);
            c0 = r2::ld(cb + 8 * i + 128);
            r2::store6(a2, b);
            const auto a3 = r2::core_dv(c1);
            c1 = r2::ld(cb + 8 * i + 160);
            r2::store6(a3, b + r2::SLOT6);
            r2::compose6(a2, b);
            r2::chain6(b, b, e, reinterpret_cast<uint16_t*>(ob + i + 8));
            r2::chain6(b + r2::SLOT6, b, e, reinterpret_cast<uint16_t*>(ob + i + 12));
        }
    }
    const u8* p = cb;
    uint16_t* o = reinterpret_cast<uint16_t*>(ob);
    for (size_t r = nb - 4 * static_cast<size_t>(K); r != 0; --r, p += 32, o += 2)
    {
        const auto a = r2::core_dv(r2::ld(p));
        r2::store6(a, b);
        r2::store6(a, b + r2::SLOT6);
        r2::compose6(a, b);
        r2::chain6(b, b, e, o);
    }
}
// Non-pipelined x2 with load-ahead: core(q) and finish(q) in the same step.
#define R2_DRIVER_NP(NAME, CORE_V, FINISH, TABLES)                                              \
    __attribute__((target("avx2,bmi,bmi2"))) void NAME(const u8* code, size_t size, u64* bits) \
    {                                                                                          \
        TABLES t;                                                                              \
        size_t e = 0x80;                                                                       \
        const size_t nb = (size + 31) / 32;                                                    \
        const ptrdiff_t K = static_cast<ptrdiff_t>(nb / 2);                                    \
        const u8* const cb = code + 64 * K;                                                    \
        u8* const ob = reinterpret_cast<u8*>(bits) + 8 * K;                                    \
        auto c0 = r2::ld(code), c1 = r2::ld(code + 32);                                        \
        for (ptrdiff_t i = -8 * K; i != 0; i += 8)                                             \
        {                                                                                      \
            const auto a = CORE_V(c0);                                                         \
            c0 = r2::ld(cb + 8 * i + 64);                                                      \
            FINISH(a, t.b, e, reinterpret_cast<uint16_t*>(ob + i));                            \
            const auto bq = CORE_V(c1);                                                        \
            c1 = r2::ld(cb + 8 * i + 96);                                                      \
            FINISH(bq, t.b, e, reinterpret_cast<uint16_t*>(ob + i + 4));                       \
        }                                                                                      \
        if (nb & 1)                                                                            \
            FINISH(CORE_V(c0), t.b, e, reinterpret_cast<uint16_t*>(ob));                       \
    }
R2_DRIVER_NP(c8_avx2, r2::core_v, r2::finish_c8, r2::Tables8)
// Non-pipelined x4 with load-ahead (the code of the next 2 blocks is loaded one step early).
// The main loop stops while 2 blocks remain, so no read goes past block nb - 1 (+31 bytes).
#define R2_DRIVER_NP4(NAME, CORE_V, FINISH, TABLES)                                             \
    __attribute__((target("avx2,bmi,bmi2"))) void NAME(const u8* code, size_t size, u64* bits) \
    {                                                                                          \
        TABLES t;                                                                              \
        size_t e = 0x80;                                                                       \
        const size_t nb = (size + 31) / 32;                                                    \
        const ptrdiff_t K = nb < 2 ? 0 : static_cast<ptrdiff_t>((nb - 2) / 4);                 \
        const u8* const cb = code + 128 * K;                                                   \
        u8* const ob = reinterpret_cast<u8*>(bits) + 16 * K;                                   \
        if (K != 0)                                                                            \
        {                                                                                      \
            auto c0 = r2::ld(code), c1 = r2::ld(code + 32);                                    \
            for (ptrdiff_t i = -16 * K; i != 0; i += 16)                                       \
            {                                                                                  \
                const auto a0 = CORE_V(c0);                                                    \
                c0 = r2::ld(cb + 8 * i + 64);                                                  \
                FINISH(a0, t.b, e, reinterpret_cast<uint16_t*>(ob + i));                       \
                const auto a1 = CORE_V(c1);                                                    \
                c1 = r2::ld(cb + 8 * i + 96);                                                  \
                FINISH(a1, t.b, e, reinterpret_cast<uint16_t*>(ob + i + 4));                   \
                const auto a2 = CORE_V(c0);                                                    \
                c0 = r2::ld(cb + 8 * i + 128);                                                 \
                FINISH(a2, t.b, e, reinterpret_cast<uint16_t*>(ob + i + 8));                   \
                const auto a3 = CORE_V(c1);                                                    \
                c1 = r2::ld(cb + 8 * i + 160);                                                 \
                FINISH(a3, t.b, e, reinterpret_cast<uint16_t*>(ob + i + 12));                  \
            }                                                                                  \
        }                                                                                      \
        const u8* p = cb;                                                                      \
        uint16_t* o = reinterpret_cast<uint16_t*>(ob);                                         \
        for (size_t r = nb - 4 * static_cast<size_t>(K); r != 0; --r, p += 32, o += 2)          \
            FINISH(CORE_V(r2::ld(p)), t.b, e, o);                                              \
    }
R2_DRIVER_NP4(c9_avx2, r2::core_v, r2::finish_c8, r2::Tables8)

// One block per iteration with load-ahead (c8 and c9 without the unroll).
#define R2_DRIVER_NP1(NAME, CORE_V, FINISH, TABLES)                                             \
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
R2_DRIVER_NP1(c8u1_avx2, r2::core_v, r2::finish_c8, r2::Tables8)

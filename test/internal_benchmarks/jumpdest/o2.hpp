// Round 2 SSE kernels (Opus): g8 with byte-insert output words, a negative loop counter and
// the setup copies replaced by loads. o2l: x2 loop, o2x: x4 loop, o2m: x2 loop with the half
// merge as a pure gather. The code loads are inline asm (movdqu), so the aligned and unaligned
// builds are the same.
namespace o2
{
// Tables: like G8Tables; V1/V3 are read with a 16-bit load at index - 1 (low byte = garbage).
struct Tables
{
    static constexpr size_t TA = 0;
    static constexpr size_t PT = 48;
    static constexpr size_t T0 = 96;
    static constexpr size_t T2 = 144;
    static constexpr size_t V0 = 192;
    static constexpr size_t V2 = 240;
    static constexpr size_t V1 = 296;
    static constexpr size_t V3 = 344;
    alignas(64) u8 b[384];
    Tables()
    {
        std::memset(b, 0, sizeof(b));
        for (unsigned i = 8; i < 40; ++i)
            b[T0 + i] = b[T2 + i] = static_cast<u8>(0x78 + i - 8);
        for (unsigned i = 16; i < 40; ++i)
            b[TA + i] = static_cast<u8>(0x80 + i - 16);
        for (unsigned i = 32; i < 40; ++i)
            b[PT + i] = static_cast<u8>(0x80 + i - 32);
    }
};

// Low byte of w := *p (a byte insert; the upper bits of w are kept).
__attribute__((always_inline)) inline uint32_t insert_lo(uint32_t w, const u8* p)
{
#ifdef R2_NO_ASM
    return (w & ~0xffu) | *p;
#else
    asm("movb %1, %b0" : "+r"(w) : "m"(*p));
    return w;
#endif
}
__attribute__((always_inline)) inline uint32_t load16(const u8* p)
{
    uint16_t v;
    std::memcpy(&v, p, 2);
    return v;
}
}  // namespace o2

namespace o2
{
alignas(16) inline constexpr u8 IOTA[16] = {0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x1a,
    0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21};

// g8_half with the two register copies of the setup replaced by loads (the code is loaded twice,
// the iota constant is loaded into the destination of the add), which moves them off the vector ALUs.
__attribute__((target("ssse3,sse4.1"), always_inline)) inline G8Half half_ld(const u8* p)
{
    const auto k8 = _mm_setr_epi8(8, 8, 8, 8, 8, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0);
    const auto lane = _mm_setr_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    const auto bit = _mm_setr_epi8(1, 2, 4, 8, 16, 32, 64, -128, 1, 2, 4, 8, 16, 32, 64, -128);
    const auto iota_n = _mm_setr_epi8(0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b,
        0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21);  // iota - k8
    __m128i c, c2, y;
    asm volatile("movdqu %1, %0" : "=x"(c) : "m"(*reinterpret_cast<const u8(*)[16]>(p)));
    asm volatile("movdqu %1, %0" : "=x"(c2) : "m"(*reinterpret_cast<const u8(*)[16]>(p)));
    asm volatile("movdqa %1, %0" : "=x"(y) : "m"(IOTA));
    const auto m = _mm_max_epi8(c2, _mm_set1_epi8(0x5f));
    y = _mm_add_epi8(y, m);
    auto n = _mm_add_epi8(m, iota_n);
    auto x = _mm_max_epi8(_mm_xor_si128(y, k8), lane);
    auto v = _mm_and_si128(_mm_cmpeq_epi8(c, _mm_set1_epi8(0x5b)), bit);
#pragma GCC unroll 3
    for (int k = 0; k < 3; ++k)
    {
        v = _mm_or_si128(v, _mm_shuffle_epi8(v, x));
        x = _mm_shuffle_epi8(x, x);
    }
    n = _mm_shuffle_epi8(n, x);
    return {_mm_max_epu8(n, _mm_shuffle_epi8(n, n)), n, v};
}

__attribute__((target("ssse3,sse4.1"), always_inline)) inline size_t block_ld(
    const u8* p, u8* b, size_t e, u8* o)
{
    using T = Tables;
    const auto a = half_ld(p);
    const auto c = half_ld(p + 16);
    _mm_store_si128(reinterpret_cast<__m128i*>(b + T::TA), a.xm);
    _mm_store_si128(reinterpret_cast<__m128i*>(b + T::PT + 16), c.xm);
    const auto ia = _mm_sub_epi8(a.xm, _mm_set1_epi8(0x10));
    _mm_store_si128(
        reinterpret_cast<__m128i*>(b + T::PT), _mm_max_epu8(_mm_shuffle_epi8(c.xm, ia), ia));
    _mm_storel_epi64(reinterpret_cast<__m128i*>(b + T::T0), a.xa);
    _mm_storel_epi64(reinterpret_cast<__m128i*>(b + T::T2), c.xa);
    _mm_storel_epi64(reinterpret_cast<__m128i*>(b + T::V0), a.v);
    _mm_storel_epi64(reinterpret_cast<__m128i*>(b + T::V2), c.v);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(b + T::V1 - 8), a.v);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(b + T::V3 - 8), c.v);
    const size_t e1 = b[e - 0x80 + T::T0];
    const size_t e2 = b[e - 0x80 + T::TA];
    const size_t e3 = b[e2 - 0x80 + T::T2];
    const auto w0 = insert_lo(load16(b + e1 - 0x78 + T::V1 - 1), b + e - 0x80 + T::V0);
    const auto w1 = insert_lo(load16(b + e3 - 0x78 + T::V3 - 1), b + e2 - 0x80 + T::V2);
    const auto w0s = static_cast<uint16_t>(w0), w1s = static_cast<uint16_t>(w1);
    std::memcpy(o, &w0s, 2);
    std::memcpy(o + 2, &w1s, 2);
    return b[e - 0x80 + T::PT];
}
}  // namespace o2

__attribute__((target("ssse3,sse4.1"))) void o2l_sse(const u8* code, size_t size, u64* bits)
{
    o2::Tables t;
    u8* const b = t.b;
    const ptrdiff_t nb = static_cast<ptrdiff_t>((size + 31) / 32);
    auto* out = reinterpret_cast<u8*>(bits);
    size_t e = 0x80;
    if (nb & 1)
    {
        e = o2::block_ld(code, b, e, out);
        code += 32;
        out += 4;
    }
    const ptrdiff_t n2 = nb & ~ptrdiff_t{1};
    if (n2 == 0)
        return;
    const u8* const cend = code + 32 * n2;
    u8* const oend = out + 4 * n2;
    ptrdiff_t i = -4 * n2;
    do
    {
        e = o2::block_ld(cend + 8 * i, b, e, oend + i);
        asm volatile("" ::: "memory");
        e = o2::block_ld(cend + 8 * i + 32, b, e, oend + i + 4);
        asm("" : "+r"(i));
    } while ((i += 8) != 0);
}

namespace o2
{
alignas(16) inline constexpr u8 LANE[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

struct Half2
{
    __m128i xm, v;
};

// half_ld with the merge as a pure gather: xm = n[max(n, lane)] (A lanes whose exit is in B point
// to the B entry, all others to themselves). The half exits n are stored to tn (8 bytes) first.
__attribute__((target("ssse3,sse4.1"), always_inline)) inline Half2 half_m(const u8* p, u8* tn)
{
    const auto k8 = _mm_setr_epi8(8, 8, 8, 8, 8, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0);
    const auto lane = _mm_setr_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    const auto bit = _mm_setr_epi8(1, 2, 4, 8, 16, 32, 64, -128, 1, 2, 4, 8, 16, 32, 64, -128);
    const auto iota_n = _mm_setr_epi8(0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b,
        0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21);
    __m128i c, c2, y, l;
    asm volatile("movdqu %1, %0" : "=x"(c) : "m"(*reinterpret_cast<const u8(*)[16]>(p)));
    asm volatile("movdqu %1, %0" : "=x"(c2) : "m"(*reinterpret_cast<const u8(*)[16]>(p)));
    asm volatile("movdqa %1, %0" : "=x"(y) : "m"(IOTA));
    const auto m = _mm_max_epi8(c2, _mm_set1_epi8(0x5f));
    y = _mm_add_epi8(y, m);
    auto n = _mm_add_epi8(m, iota_n);
    auto x = _mm_max_epi8(_mm_xor_si128(y, k8), lane);
    auto v = _mm_and_si128(_mm_cmpeq_epi8(c, _mm_set1_epi8(0x5b)), bit);
#pragma GCC unroll 3
    for (int k = 0; k < 3; ++k)
    {
        v = _mm_or_si128(v, _mm_shuffle_epi8(v, x));
        x = _mm_shuffle_epi8(x, x);
    }
    n = _mm_shuffle_epi8(n, x);
    _mm_storel_epi64(reinterpret_cast<__m128i*>(tn), n);
    asm volatile("movdqa %1, %0" : "=x"(l) : "m"(LANE));
    l = _mm_max_epi8(l, n);
    return {_mm_shuffle_epi8(n, l), v};
}

__attribute__((target("ssse3,sse4.1"), always_inline)) inline size_t block_m(
    const u8* p, u8* b, size_t e, u8* o)
{
    using T = Tables;
    const auto a = half_m(p, b + T::T0);
    const auto c = half_m(p + 16, b + T::T2);
    _mm_store_si128(reinterpret_cast<__m128i*>(b + T::TA), a.xm);
    _mm_store_si128(reinterpret_cast<__m128i*>(b + T::PT + 16), c.xm);
    const auto ia = _mm_sub_epi8(a.xm, _mm_set1_epi8(0x10));
    _mm_store_si128(
        reinterpret_cast<__m128i*>(b + T::PT), _mm_max_epu8(_mm_shuffle_epi8(c.xm, ia), ia));
    _mm_storel_epi64(reinterpret_cast<__m128i*>(b + T::V0), a.v);
    _mm_storel_epi64(reinterpret_cast<__m128i*>(b + T::V2), c.v);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(b + T::V1 - 8), a.v);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(b + T::V3 - 8), c.v);
    const size_t e1 = b[e - 0x80 + T::T0];
    const size_t e2 = b[e - 0x80 + T::TA];
    const size_t e3 = b[e2 - 0x80 + T::T2];
    const auto w0 = insert_lo(load16(b + e1 - 0x78 + T::V1 - 1), b + e - 0x80 + T::V0);
    const auto w1 = insert_lo(load16(b + e3 - 0x78 + T::V3 - 1), b + e2 - 0x80 + T::V2);
    const auto w0s = static_cast<uint16_t>(w0), w1s = static_cast<uint16_t>(w1);
    std::memcpy(o, &w0s, 2);
    std::memcpy(o + 2, &w1s, 2);
    return b[e - 0x80 + T::PT];
}
}  // namespace o2

__attribute__((target("ssse3,sse4.1"))) void o2m_sse(const u8* code, size_t size, u64* bits)
{
    o2::Tables t;
    u8* const b = t.b;
    const ptrdiff_t nb = static_cast<ptrdiff_t>((size + 31) / 32);
    auto* out = reinterpret_cast<u8*>(bits);
    size_t e = 0x80;
    if (nb & 1)
    {
        e = o2::block_m(code, b, e, out);
        code += 32;
        out += 4;
    }
    const ptrdiff_t n2 = nb & ~ptrdiff_t{1};
    if (n2 == 0)
        return;
    const u8* const cend = code + 32 * n2;
    u8* const oend = out + 4 * n2;
    ptrdiff_t i = -4 * n2;
    do
    {
        e = o2::block_m(cend + 8 * i, b, e, oend + i);
        asm volatile("" ::: "memory");
        e = o2::block_m(cend + 8 * i + 32, b, e, oend + i + 4);
        asm("" : "+r"(i));
    } while ((i += 8) != 0);
}

// o2l unrolled x4.
__attribute__((target("ssse3,sse4.1"))) void o2x_sse(const u8* code, size_t size, u64* bits)
{
    o2::Tables t;
    u8* const b = t.b;
    const ptrdiff_t nb = static_cast<ptrdiff_t>((size + 31) / 32);
    auto* out = reinterpret_cast<u8*>(bits);
    size_t e = 0x80;
    for (ptrdiff_t k = 0; k < (nb & 3); ++k)
    {
        e = o2::block_ld(code, b, e, out);
        code += 32;
        out += 4;
    }
    const ptrdiff_t n4 = nb & ~ptrdiff_t{3};
    if (n4 == 0)
        return;
    const u8* const cend = code + 32 * n4;
    u8* const oend = out + 4 * n4;
    ptrdiff_t i = -4 * n4;
    do
    {
        e = o2::block_ld(cend + 8 * i, b, e, oend + i);
        asm volatile("" ::: "memory");
        e = o2::block_ld(cend + 8 * i + 32, b, e, oend + i + 4);
        asm volatile("" ::: "memory");
        e = o2::block_ld(cend + 8 * i + 64, b, e, oend + i + 8);
        asm volatile("" ::: "memory");
        e = o2::block_ld(cend + 8 * i + 96, b, e, oend + i + 12);
        asm("" : "+r"(i));
    } while ((i += 16) != 0);
}

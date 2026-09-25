// g9np_ofix written with GCC/clang vector types. The only architecture-specific operation is
// shuffle(): a byte table lookup within 16-byte lanes that returns 0 for indexes with bit 7 set.
namespace vt
{
using u8x16 = uint8_t __attribute__((vector_size(16)));
using u8x32 = uint8_t __attribute__((vector_size(32)));
using i8x32 = int8_t __attribute__((vector_size(32)));

#define VT_I __attribute__((target("avx2"), always_inline)) inline

VT_I u8x32 shuffle(u8x32 t, u8x32 i)
{
    return reinterpret_cast<u8x32>(
        _mm256_shuffle_epi8(reinterpret_cast<__m256i>(t), reinterpret_cast<__m256i>(i)));
}
VT_I u8x16 shuffle(u8x16 t, u8x16 i)
{
    return reinterpret_cast<u8x16>(
        _mm_shuffle_epi8(reinterpret_cast<__m128i>(t), reinterpret_cast<__m128i>(i)));
}

VT_I u8x32 max_s(u8x32 a, u8x32 b)
{
    const auto sa = reinterpret_cast<i8x32>(a), sb = reinterpret_cast<i8x32>(b);
    return reinterpret_cast<u8x32>(sa > sb ? sa : sb);
}
template <typename V>
VT_I V max_u(V a, V b)
{
    return a > b ? a : b;
}
VT_I u8x16 lo(u8x32 v)
{
    return __builtin_shufflevector(v, v, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
}
VT_I u8x16 hi(u8x32 v)
{
    return __builtin_shufflevector(
        v, v, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31);
}
/// Interleaves the low (L = 0) or high (L = 8) 8 bytes of each 16-byte lane of a and b.
template <int L>
VT_I u8x32 unpack(u8x32 a, u8x32 b)
{
    return __builtin_shufflevector(a, b, L + 0, 32 + L + 0, L + 1, 32 + L + 1, L + 2, 32 + L + 2,
        L + 3, 32 + L + 3, L + 4, 32 + L + 4, L + 5, 32 + L + 5, L + 6, 32 + L + 6, L + 7,
        32 + L + 7, 16 + L + 0, 48 + L + 0, 16 + L + 1, 48 + L + 1, 16 + L + 2, 48 + L + 2,
        16 + L + 3, 48 + L + 3, 16 + L + 4, 48 + L + 4, 16 + L + 5, 48 + L + 5, 16 + L + 6,
        48 + L + 6, 16 + L + 7, 48 + L + 7);
}
/// Stores v at p. The empty asm keeps the compiler from merging adjacent stores into a wider one
/// assembled with a lane insert.
template <typename V>
VT_I void store(u8* p, V v)
{
    __builtin_memcpy(p, &v, sizeof(v));
    asm("" : "+m"(*reinterpret_cast<u8(*)[sizeof(V)]>(p)));
}

constexpr size_t PT = 0;    // u8: 0x80 + next block entry; [16,32) = TB; tail s >= 32.
constexpr size_t TA = 48;   // u8: 0x80 + entry into group B; tail s >= 16.
constexpr size_t VA = 96;   // u16: JUMPDEST bits of group A (0 for s >= 16).
constexpr size_t VB = 176;  // u16: JUMPDEST bits of group B (0 for s >= 16).
constexpr size_t SLOT = 256;

VT_I void block(const u8* p, u8* s)
{
    constexpr u8x32 iota = {0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x1a, 0x1b, 0x1c, 0x1d,
        0x1e, 0x1f, 0x20, 0x21, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x1a, 0x1b, 0x1c,
        0x1d, 0x1e, 0x1f, 0x20, 0x21};
    constexpr u8x32 k8 = {8, 8, 8, 8, 8, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 8, 8, 8, 8, 8, 8, 0,
        0, 0, 0, 0, 0, 0, 0};
    constexpr u8x32 lane = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5,
        6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    constexpr u8x32 bit = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16,
        32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};

    const u8x32 c = *static_cast<const u8x32*>(__builtin_assume_aligned(p, 32));
    const u8x32 y = max_s(c, u8x32{} + 0x5f) + iota;
    u8x32 n = y - k8;               // 0x70 + next start in the group.
    u8x32 x = max_s(y ^ k8, lane);  // In-half pointer, or a sink.
    u8x32 v = (c == 0x5b) & bit;
#pragma GCC unroll 3
    for (int i = 0; i < 3; ++i)
    {
        v |= shuffle(v, x);
        x = shuffle(x, x);
    }
    n = shuffle(n, x);                         // Exit of the half.
    const u8x32 xm = max_u(n, shuffle(n, n));  // [TA | TB] = 0x80 + entry.
    const u8x32 h = shuffle(v, n);             // 2nd-half bits for 1st-half entries.
    const u8x32 vl = unpack<0>(v, h), vh = unpack<8>(h, v);

    const u8x16 ta = lo(xm), tb = hi(xm);
    const u8x16 ia = ta - 0x10;
    store(s + TA, ta);
    store(s + PT, max_u(shuffle(tb, ia), ia));
    store(s + PT + 16, tb);
    store(s + VA, lo(vl));
    store(s + VA + 16, lo(vh));
    store(s + VB, hi(vl));
    store(s + VB + 16, hi(vh));
}
}  // namespace vt

__attribute__((target("avx2"))) void vt_g9np_avx2(const u8* code, size_t size, u64* bits)
{
    const size_t n = (size + 31) / 32;
    alignas(64) u8 s[vt::SLOT] = {};
    for (unsigned i = 16; i < 48; ++i)
        s[vt::TA + i] = static_cast<u8>(0x80 + i - 16);
    for (unsigned i = 32; i < 48; ++i)
        s[vt::PT + i] = static_cast<u8>(0x80 + i - 32);
    auto* const out = reinterpret_cast<uint16_t*>(bits);
    size_t e = 0x80;
    for (size_t q = 0; q < n; ++q)
    {
        vt::block(code + 32 * q, s);
        const size_t e2 = s[vt::TA + e - 0x80];
        uint16_t w0, w1;
        __builtin_memcpy(&w0, s + vt::VA + 2 * (e - 0x80), 2);
        __builtin_memcpy(&w1, s + vt::VB + 2 * (e2 - 0x80), 2);
        out[2 * q] = w0;
        out[2 * q + 1] = w1;
        e = s[vt::PT + e - 0x80];
    }
}

namespace vt
{
using i8x16 = int8_t __attribute__((vector_size(16)));

#define VT_S __attribute__((target("ssse3,sse4.1"), always_inline)) inline

VT_S u8x16 shuffle_sse(u8x16 t, u8x16 i)
{
    return reinterpret_cast<u8x16>(
        _mm_shuffle_epi8(reinterpret_cast<__m128i>(t), reinterpret_cast<__m128i>(i)));
}
VT_S u8x16 max_s(u8x16 a, u8x16 b)
{
    const auto sa = reinterpret_cast<i8x16>(a), sb = reinterpret_cast<i8x16>(b);
    return reinterpret_cast<u8x16>(sa > sb ? sa : sb);
}
VT_S u8x16 max_u_sse(u8x16 a, u8x16 b)
{
    return a > b ? a : b;
}
VT_S void store_sse(u8* p, const void* v, size_t n)
{
    __builtin_memcpy(p, v, n);
}

struct Half
{
    u8x16 xm, xa, v;
};

VT_S Half half(const u8* p)
{
    constexpr u8x16 iota = {0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x1a, 0x1b, 0x1c, 0x1d,
        0x1e, 0x1f, 0x20, 0x21};
    constexpr u8x16 k8 = {8, 8, 8, 8, 8, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0};
    constexpr u8x16 lane = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    constexpr u8x16 bit = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
    const u8x16 c = *static_cast<const u8x16*>(__builtin_assume_aligned(p, 16));
    const u8x16 y = max_s(c, u8x16{} + 0x5f) + iota;
    u8x16 n = y - k8;
    u8x16 x = max_s(y ^ k8, lane);
    u8x16 v = (c == 0x5b) & bit;
#pragma GCC unroll 3
    for (int i = 0; i < 3; ++i)
    {
        v |= shuffle_sse(v, x);
        x = shuffle_sse(x, x);
    }
    n = shuffle_sse(n, x);
    return {max_u_sse(n, shuffle_sse(n, n)), n, v};
}

// g8_sse tables (see G8Tables).
constexpr size_t S_TA = 0, S_PT = 48, S_T0 = 96, S_T2 = 144, S_V0 = 192, S_V2 = 240, S_V1 = 296,
                 S_V3 = 344, S_SIZE = 384;
}  // namespace vt

__attribute__((target("ssse3,sse4.1"))) void vt_g8_sse(const u8* code, size_t size, u64* bits)
{
    using namespace vt;
    alignas(64) u8 b[S_SIZE] = {};
    for (unsigned i = 8; i < 40; ++i)
        b[S_T0 + i] = b[S_T2 + i] = static_cast<u8>(0x78 + i - 8);
    for (unsigned i = 16; i < 40; ++i)
        b[S_TA + i] = static_cast<u8>(0x80 + i - 16);
    for (unsigned i = 32; i < 40; ++i)
        b[S_PT + i] = static_cast<u8>(0x80 + i - 32);
    auto* const out = reinterpret_cast<u8*>(bits);
    size_t e = 0x80;
    const size_t n = (size + 31) / 32;
    for (size_t q = 0; q < n; ++q)
    {
        const auto a = half(code + 32 * q);
        const auto c = half(code + 32 * q + 16);
        const u8x16 ia = a.xm - 0x10;
        const u8x16 pt = max_u_sse(shuffle_sse(c.xm, ia), ia);
        store_sse(b + S_TA, &a.xm, 16);
        store_sse(b + S_PT + 16, &c.xm, 16);
        store_sse(b + S_PT, &pt, 16);
        store_sse(b + S_T0, &a.xa, 8);
        store_sse(b + S_T2, &c.xa, 8);
        store_sse(b + S_V0, &a.v, 8);
        store_sse(b + S_V2, &c.v, 8);
        store_sse(b + S_V1 - 8, &a.v, 16);
        store_sse(b + S_V3 - 8, &c.v, 16);
        const size_t e1 = b[e - 0x80 + S_T0];
        const size_t e2 = b[e - 0x80 + S_TA];
        const size_t e3 = b[e2 - 0x80 + S_T2];
        // Separate byte stores: GCC would merge them into one 32-bit store built with shifts.
        out[4 * q + 0] = b[e - 0x80 + S_V0];
        asm("" : "+m"(out[4 * q]));
        out[4 * q + 1] = b[e1 - 0x78 + S_V1];
        asm("" : "+m"(out[4 * q + 1]));
        out[4 * q + 2] = b[e2 - 0x80 + S_V2];
        asm("" : "+m"(out[4 * q + 2]));
        out[4 * q + 3] = b[e3 - 0x78 + S_V3];
        e = b[e - 0x80 + S_PT];
    }
}

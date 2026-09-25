// One g8 source for SSE4 and AVX2 (the g9np table stores), differing only in the vector type. A
// 32-byte block is one u8x32 (AVX2: the two 16-byte groups are the two lanes) or two u8x16 (SSE4:
// one group each). All operations work within 16-byte lanes; shuffle() is the only intrinsic.
// Include it in a source file compiled for the ISA (-mssse3 -msse4.1 or -mavx2) and call run<V>().
namespace vt2
{
using u8x16 = uint8_t __attribute__((vector_size(16)));
using i8x16 = int8_t __attribute__((vector_size(16)));
using u8x32 = uint8_t __attribute__((vector_size(32)));
using i8x32 = int8_t __attribute__((vector_size(32)));

template <typename V>
struct Traits;
template <>
struct Traits<u8x16>
{
    using S = i8x16;
    static constexpr int N = 2;  // Vectors per block.
    [[gnu::always_inline]] static u8x16 shuffle(u8x16 t, u8x16 i)
    {
        return reinterpret_cast<u8x16>(
            _mm_shuffle_epi8(reinterpret_cast<__m128i>(t), reinterpret_cast<__m128i>(i)));
    }
};
#ifdef __AVX2__
template <>
struct Traits<u8x32>
{
    using S = i8x32;
    static constexpr int N = 1;
    [[gnu::always_inline]] static u8x32 shuffle(u8x32 t, u8x32 i)
    {
        return reinterpret_cast<u8x32>(
            _mm256_shuffle_epi8(reinterpret_cast<__m256i>(t), reinterpret_cast<__m256i>(i)));
    }
    [[gnu::always_inline]] static u8x16 shuffle(u8x16 t, u8x16 i)
    {
        return reinterpret_cast<u8x16>(
            _mm_shuffle_epi8(reinterpret_cast<__m128i>(t), reinterpret_cast<__m128i>(i)));
    }
};
#endif

/// A vector of the 16-byte pattern repeated in every lane.
template <typename V>
[[gnu::always_inline]] inline V rep(u8x16 p)
{
    if constexpr (sizeof(V) == 16)
        return p;
    else
        return __builtin_shufflevector(p, p, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
}
template <typename V>
[[gnu::always_inline]] inline V max_s(V a, V b)
{
    using S = typename Traits<V>::S;
    const auto sa = reinterpret_cast<S>(a), sb = reinterpret_cast<S>(b);
    return reinterpret_cast<V>(sa > sb ? sa : sb);
}
template <typename V>
[[gnu::always_inline]] inline V max_u(V a, V b)
{
    return a > b ? a : b;
}
/// Group g (0: bytes 0..15 of the block, 1: bytes 16..31) of the k-th vector of the block.
[[gnu::always_inline]] inline u8x16 group(u8x16 v, int)
{
    return v;
}
[[gnu::always_inline]] inline u8x16 group(u8x32 v, int g)
{
    return g == 0 ?
               __builtin_shufflevector(v, v, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15) :
               __builtin_shufflevector(
                   v, v, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31);
}
/// Interleaves the low (L = 0) or high (L = 8) 8 bytes of each 16-byte lane of a and b.
template <int L>
[[gnu::always_inline]] inline u8x16 unpack(u8x16 a, u8x16 b)
{
    return __builtin_shufflevector(a, b, L, 16 + L, L + 1, 17 + L, L + 2, 18 + L, L + 3, 19 + L,
        L + 4, 20 + L, L + 5, 21 + L, L + 6, 22 + L, L + 7, 23 + L);
}
template <int L>
[[gnu::always_inline]] inline u8x32 unpack(u8x32 a, u8x32 b)
{
    return __builtin_shufflevector(a, b, L, 32 + L, L + 1, 33 + L, L + 2, 34 + L, L + 3, 35 + L,
        L + 4, 36 + L, L + 5, 37 + L, L + 6, 38 + L, L + 7, 39 + L, 16 + L, 48 + L, 17 + L, 49 + L,
        18 + L, 50 + L, 19 + L, 51 + L, 20 + L, 52 + L, 21 + L, 53 + L, 22 + L, 54 + L, 23 + L,
        55 + L);
}
/// Stores v at p. The empty asm keeps compilers from merging adjacent stores.
[[gnu::always_inline]] inline void store(u8* p, u8x16 v)
{
    __builtin_memcpy(p, &v, sizeof(v));
    asm("" : "+m"(*reinterpret_cast<u8(*)[16]>(p)));
}

constexpr size_t PT = 0;    // u8: 0x80 + next block entry; [16,32) = TB; tail s >= 32.
constexpr size_t TA = 48;   // u8: 0x80 + entry into group B; tail s >= 16.
constexpr size_t VA = 96;   // u16: JUMPDEST bits of group A (0 for s >= 16).
constexpr size_t VB = 176;  // u16: JUMPDEST bits of group B (0 for s >= 16).
constexpr size_t SLOT = 256;

template <typename V>
[[gnu::always_inline]] inline void block(const u8* p, u8* s)
{
    using T = Traits<V>;
    constexpr u8x16 IOTA = {0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x1a, 0x1b, 0x1c, 0x1d,
        0x1e, 0x1f, 0x20, 0x21};
    constexpr u8x16 K8 = {8, 8, 8, 8, 8, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0};
    constexpr u8x16 LANE = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    constexpr u8x16 BIT = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
    const auto iota = rep<V>(IOTA), k8 = rep<V>(K8), lane = rep<V>(LANE), bit = rep<V>(BIT);

    u8x16 xm_g[2], vl_g[2], vh_g[2];  // Per 16-byte group: [TA or TB], word halves.
    for (int k = 0; k < T::N; ++k)
    {
        const V c = *static_cast<const V*>(__builtin_assume_aligned(p + k * sizeof(V), sizeof(V)));
        const V y = max_s(c, V{} + 0x5f) + iota;
        V n = y - k8;               // 0x70 + next start in the group.
        V x = max_s(y ^ k8, lane);  // In-half pointer, or a sink.
        V v = (c == 0x5b) & bit;
#pragma GCC unroll 3
        for (int i = 0; i < 3; ++i)
        {
            v |= T::shuffle(v, x);
            x = T::shuffle(x, x);
        }
        n = T::shuffle(n, x);                     // Exit of the half.
        const V xm = max_u(n, T::shuffle(n, n));  // 0x80 + entry into the next group.
        const V h = T::shuffle(v, n);             // 2nd-half bits for 1st-half entries.
        const V vl = unpack<0>(v, h), vh = unpack<8>(h, v);
        for (int g = 0; g < 2 / T::N; ++g)
        {
            xm_g[k + g] = group(xm, g);
            vl_g[k + g] = group(vl, g);
            vh_g[k + g] = group(vh, g);
        }
    }
    const u8x16 ia = xm_g[0] - 0x10;
    store(s + TA, xm_g[0]);
    store(s + PT, max_u(T::shuffle(xm_g[1], ia), ia));
    store(s + PT + 16, xm_g[1]);
    store(s + VA, vl_g[0]);
    store(s + VA + 16, vh_g[0]);
    store(s + VB, vl_g[1]);
    store(s + VB + 16, vh_g[1]);
}

template <typename V>
[[gnu::always_inline]] inline void run(const u8* code, size_t size, u64* bits)
{
    const size_t n = (size + 31) / 32;
    alignas(64) u8 s[SLOT] = {};
    for (unsigned i = 16; i < 48; ++i)
        s[TA + i] = static_cast<u8>(0x80 + i - 16);
    for (unsigned i = 32; i < 48; ++i)
        s[PT + i] = static_cast<u8>(0x80 + i - 32);
    auto* const out = reinterpret_cast<uint16_t*>(bits);
    size_t e = 0x80;
    for (size_t q = 0; q < n; ++q)
    {
        block<V>(code + 32 * q, s);
        const size_t e2 = s[TA + e - 0x80];
        uint16_t w0, w1;
        __builtin_memcpy(&w0, s + VA + 2 * (e - 0x80), 2);
        __builtin_memcpy(&w1, s + VB + 2 * (e2 - 0x80), 2);
        out[2 * q] = w0;
        out[2 * q + 1] = w1;
        e = s[PT + e - 0x80];
    }
}
}  // namespace vt2

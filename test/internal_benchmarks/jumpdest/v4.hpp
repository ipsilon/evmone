// JUMPDEST analysis v4: 16-byte groups, pshufb pointer doubling, pair composition.

// v4: positions are encoded as 0x70 + pos, so pos >= 16 (the group exit) has bit 7 set and
// pshufb returns 0 for it; the setup is 3 ops. Tables have tails so the chain needs no compares:
// TA[s] (next group entry), PT[s] (next pair entry), VA/VB[s] (visited starts, 0 for s >= 16).
struct G16x
{
    __m128i x;  // Encoded exit (0x80 | (exit - 16)) for each entry offset 0..15.
    __m128i v;  // Visited starts 0..7 per entry offset (low byte plane).
    __m128i w;  // Visited starts 8..15 per entry offset (high byte plane).
};

__attribute__((target("ssse3,sse4.1"), always_inline)) inline G16x g16x(__m128i c)
{
    const auto iota = _mm_setr_epi8(0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a,
        0x7b, 0x7c, 0x7d, 0x7e, 0x7f, -128);
    const auto p = _mm_max_epi8(_mm_subs_epi8(c, _mm_set1_epi8(0x5f)), _mm_setzero_si128());
    auto x = _mm_add_epi8(iota, p);
    auto v = _mm_setr_epi8(1, 2, 4, 8, 16, 32, 64, -128, 0, 0, 0, 0, 0, 0, 0, 0);
    auto w = _mm_setr_epi8(0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 4, 8, 16, 32, 64, -128);
    for (int k = 0; k < 4; ++k)
    {
        v = _mm_or_si128(v, _mm_shuffle_epi8(v, x));
        w = _mm_or_si128(w, _mm_shuffle_epi8(w, x));
        x = _mm_max_epu8(x, _mm_shuffle_epi8(x, x));
    }
    return {x, v, w};
}

struct Chain
{
    alignas(64) u8 TA[48];
    alignas(64) u8 PT[48];
    alignas(64) uint16_t VA[48];
    alignas(64) uint16_t VB[48];
    Chain()
    {
        for (unsigned i = 16; i < 48; ++i)
            TA[i] = static_cast<u8>(i - 16);
        for (unsigned i = 32; i < 48; ++i)
            PT[i] = static_cast<u8>(i - 32);
        for (unsigned i = 16; i < 48; ++i)
            VA[i] = VB[i] = 0;
    }
};

__attribute__((target("ssse3,sse4.1"))) void g16v4_sse(const u8* code, size_t size, u64* bits)
{
    const auto c5b = _mm_set1_epi8(0x5b);
    const auto c7f = _mm_set1_epi8(0x7f);
    const auto c10 = _mm_set1_epi8(0x10);
    const auto c16 = _mm_set1_epi8(16);
    Chain t;
    unsigned s = 0;
    const size_t num_pairs = (size + 31) / 32;
    for (size_t q = 0; q < num_pairs; ++q)
    {
        MCA_BEGIN("g16v4_sse");
        const auto ca = JDA_LOAD128(code + 32 * q);
        const auto cb = JDA_LOAD128(code + 32 * q + 16);
        const auto a = g16x(ca);
        const auto b = g16x(cb);
        const auto ta = _mm_and_si128(a.x, c7f);  // Entry offset into B: 0..32.
        const auto tb = _mm_and_si128(b.x, c7f);
        // Next pair entry for the pair entry s < 16: TB[ta] if ta < 16, ta - 16 otherwise.
        const auto pab = _mm_or_si128(
            _mm_shuffle_epi8(tb, _mm_sub_epi8(a.x, c10)), _mm_subs_epu8(ta, c16));
        const auto jd = static_cast<uint32_t>(_mm_movemask_epi8(_mm_cmpeq_epi8(ca, c5b))) |
                        (static_cast<uint32_t>(_mm_movemask_epi8(_mm_cmpeq_epi8(cb, c5b))) << 16);
        _mm_store_si128(reinterpret_cast<__m128i*>(t.TA), ta);
        _mm_store_si128(reinterpret_cast<__m128i*>(t.PT), pab);
        _mm_store_si128(reinterpret_cast<__m128i*>(t.PT + 16), tb);
        _mm_store_si128(reinterpret_cast<__m128i*>(t.VA), _mm_unpacklo_epi8(a.v, a.w));
        _mm_store_si128(reinterpret_cast<__m128i*>(t.VA + 8), _mm_unpackhi_epi8(a.v, a.w));
        _mm_store_si128(reinterpret_cast<__m128i*>(t.VB), _mm_unpacklo_epi8(b.v, b.w));
        _mm_store_si128(reinterpret_cast<__m128i*>(t.VB + 8), _mm_unpackhi_epi8(b.v, b.w));

        const auto sb = t.TA[s];
        const auto m = uint32_t{t.VA[s]} | (uint32_t{t.VB[sb]} << 16);
        reinterpret_cast<uint32_t*>(bits)[q] = m & jd;
        s = t.PT[s];
        MCA_END("g16v4_sse");
    }
}

__attribute__((target("avx2"))) void g16v4_avx2(const u8* code, size_t size, u64* bits)
{
    const auto iota = _mm256_setr_epi8(0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a,
        0x7b, 0x7c, 0x7d, 0x7e, 0x7f, -128, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
        0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, -128);
    const auto c5f = _mm256_set1_epi8(0x5f);
    const auto c5b = _mm256_set1_epi8(0x5b);
    const auto c7f = _mm256_set1_epi8(0x7f);
    const auto c10 = _mm256_set1_epi8(0x10);
    const auto c16 = _mm256_set1_epi8(16);
    const auto v0 = _mm256_setr_epi8(1, 2, 4, 8, 16, 32, 64, -128, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 4,
        8, 16, 32, 64, -128, 0, 0, 0, 0, 0, 0, 0, 0);
    const auto w0 = _mm256_setr_epi8(0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 4, 8, 16, 32, 64, -128, 0, 0, 0,
        0, 0, 0, 0, 0, 1, 2, 4, 8, 16, 32, 64, -128);
    Chain t;
    unsigned s = 0;
    const size_t num_pairs = (size + 31) / 32;
    for (size_t q = 0; q < num_pairs; ++q)
    {
        MCA_BEGIN("g16v4_avx2");
        const auto c = JDA_LOAD256(code + 32 * q);
        const auto p = _mm256_max_epi8(_mm256_subs_epi8(c, c5f), _mm256_setzero_si256());
        auto x = _mm256_add_epi8(iota, p);
        auto v = v0;
        auto w = w0;
        for (int k = 0; k < 4; ++k)
        {
            v = _mm256_or_si256(v, _mm256_shuffle_epi8(v, x));
            w = _mm256_or_si256(w, _mm256_shuffle_epi8(w, x));
            x = _mm256_max_epu8(x, _mm256_shuffle_epi8(x, x));
        }
        const auto t2 = _mm256_and_si256(x, c7f);            // [TA | TB]
        const auto tb = _mm256_permute4x64_epi64(t2, 0xEE);  // [TB | TB]
        const auto pab =
            _mm256_or_si256(_mm256_shuffle_epi8(tb, _mm256_sub_epi8(x, c10)), _mm256_subs_epu8(t2, c16));
        const auto pt = _mm256_blend_epi32(t2, pab, 0x0F);  // [TB∘TA | TB]
        const auto jd = static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(c, c5b)));
        const auto vl = _mm256_unpacklo_epi8(v, w);  // [A0..7 | B0..7]
        const auto vh = _mm256_unpackhi_epi8(v, w);  // [A8..15 | B8..15]
        _mm_store_si128(reinterpret_cast<__m128i*>(t.TA), _mm256_castsi256_si128(t2));
        _mm256_store_si256(reinterpret_cast<__m256i*>(t.PT), pt);
        _mm256_store_si256(
            reinterpret_cast<__m256i*>(t.VA), _mm256_permute2x128_si256(vl, vh, 0x20));
        _mm256_store_si256(
            reinterpret_cast<__m256i*>(t.VB), _mm256_permute2x128_si256(vl, vh, 0x31));

        const auto sb = t.TA[s];
        const auto m = uint32_t{t.VA[s]} | (uint32_t{t.VB[sb]} << 16);
        reinterpret_cast<uint32_t*>(bits)[q] = m & jd;
        s = t.PT[s];
        MCA_END("g16v4_avx2");
    }
}

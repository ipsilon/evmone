// JUMPDEST analysis, AVX2 (x86-64-v3), final kernel.
//
// Core (per 32-byte block = 4 half-groups of 8 bytes, as in g8): for every entry offset of every
// half, pointer doubling with self-loop sinks (3 rounds) gives the last instruction start of the
// half and the JUMPDEST bits visited; one gather gives the half exit and one max/gather merges
// the two halves of a 16-byte group into [TA | TB] (0x80 + entry into the next group).
//
// What is new here:
//  - The table formatting uses no lane-crossing shuffles: h = pshufb(v, n') fetches the second
//    half's JUMPDEST byte through the half exit; vpunpck{l,h}bw interleave (lo, hi) bytes into
//    16-bit entries (0 for entries >= 8 in the low byte via unpackhi(zero, v)). Optionally
//    (BYTE_TABLES) the bytes are stored unshuffled and the scalar side assembles the words.
//  - Lean stores: [PT | TB] is one vpblendd + one ymm store; the group-1 tables are written by
//    ymm stores whose low lane lands in dead space (1 uop instead of a 2-uop vextracti128 store).
//  - The 32-byte block composition is done on xmm (vextracti128 + pshufb + max).
//  - Software pipelining: iteration q runs the doubling core of block q and the composition,
//    table stores and scalar chain of block q-1, whose inputs are already final; that empties
//    the FP scheduler (Zen 3's 64-entry FP queue was the limiter) and hides the load latency.
//    The loop is unrolled 2x so the two in-flight blocks alternate registers without copies.
//
// Input: code followed by >= 31 zero bytes. Output: zero-initialized bitset with
// (size + 64) / 64 words, written as 16-bit words; bit i of word i/64 is set iff i is a valid
// JUMPDEST. Data-independent: no branches or loops depend on the code content.

struct JdaCore
{
    __m256i xm;  // [TA | TB]: 0x80 + entry into the next 16-byte group, per entry 0..15.
    __m256i lo;  // Table part 1 (see finish).
    __m256i hi;  // Table part 2.
};

struct JdaTables
{
    static constexpr size_t TA = 0;    // 0x80 + entry into group 1, by block entry (tail: e - 16).
    static constexpr size_t PT = 64;   // 0x80 + entry into the next block; [16, 32) = TB.
    static constexpr size_t VA = 128;  // Group 0 JUMPDEST words/bytes by block entry.
    static constexpr size_t VB = 256;  // Group 1, by the entry into group 1.
    static constexpr size_t HA = 192;  // BYTE_TABLES only: second bytes.
    static constexpr size_t HB = 320;
    alignas(64) u8 b[384];
    JdaTables()
    {
        std::memset(b, 0, sizeof(b));
        for (unsigned i = 16; i < 40; ++i)
            b[TA + i] = static_cast<u8>(0x80 + i - 16);
        for (unsigned i = 32; i < 40; ++i)
            b[PT + i] = static_cast<u8>(0x80 + i - 32);
    }
};

template <bool BYTE_TABLES>
__attribute__((target("avx2"), always_inline)) inline JdaCore jda_core(const u8* code)
{
    // 0x79 + lane within the half - 0x5f, so that max_epi8(c, 0x5f) + iota = 0x78 + next
    // position relative to the half (negative iff the next start leaves the half).
    const auto iota = _mm256_broadcastsi128_si256(_mm_setr_epi8(0x1a, 0x1b, 0x1c, 0x1d, 0x1e,
        0x1f, 0x20, 0x21, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21));
    const auto k8 = _mm256_broadcastsi128_si256(
        _mm_setr_epi8(8, 8, 8, 8, 8, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0));
    const auto lane = _mm256_broadcastsi128_si256(
        _mm_setr_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15));
    const auto bit = _mm256_broadcastsi128_si256(
        _mm_setr_epi8(1, 2, 4, 8, 16, 32, 64, -128, 1, 2, 4, 8, 16, 32, 64, -128));

    const auto c = JDA_LOAD256(code);
    const auto y = _mm256_add_epi8(_mm256_max_epi8(c, _mm256_set1_epi8(0x5f)), iota);
    auto n = _mm256_sub_epi8(y, k8);                          // 0x70 + next pos in the group.
    auto x = _mm256_max_epi8(_mm256_xor_si256(y, k8), lane);  // Pointer with self-loop sinks.
    auto v = _mm256_and_si256(_mm256_cmpeq_epi8(c, _mm256_set1_epi8(0x5b)), bit);
    for (int k = 0; k < 3; ++k)
    {
        v = _mm256_or_si256(v, _mm256_shuffle_epi8(v, x));
        x = _mm256_shuffle_epi8(x, x);
    }
    n = _mm256_shuffle_epi8(n, x);                                  // Half exits.
    const auto xm = _mm256_max_epu8(n, _mm256_shuffle_epi8(n, n));  // [TA | TB]
    if constexpr (BYTE_TABLES)
    {
        // A lanes: -128 keeps the gathered half exit; B lanes: the lane itself.
        const auto lb = _mm256_broadcastsi128_si256(_mm_setr_epi8(-128, -128, -128, -128, -128,
            -128, -128, -128, 8, 9, 10, 11, 12, 13, 14, 15));
        return {xm, v, _mm256_shuffle_epi8(v, _mm256_max_epi8(n, lb))};  // [v_A v_B | h_A v_B]
    }
    else
    {
        const auto h = _mm256_shuffle_epi8(v, n);  // A lanes: JUMPDEST byte of the second half.
        return {xm, _mm256_unpacklo_epi8(v, h),    // 16-bit entries 0..7 of both groups.
            _mm256_unpackhi_epi8(_mm256_setzero_si256(), v)};  // Entries 8..15.
    }
}

#define JDA_STORE_BARRIER() __asm volatile("" ::: "memory")  // Keeps clang from fusing stores.

template <bool BYTE_TABLES>
__attribute__((target("avx2"), always_inline)) inline void jda_finish(
    const JdaCore& g, u8* b, size_t& e, uint16_t* out)
{
    using T = JdaTables;
    _mm_store_si128(reinterpret_cast<__m128i*>(b + T::TA), _mm256_castsi256_si128(g.xm));
    // Block composition: PT[s] = TB[TA[s] - 16] for TA[s] < 16, TA[s] - 16 otherwise.
    const auto tb = _mm256_extracti128_si256(g.xm, 1);
    const auto ia = _mm_sub_epi8(_mm256_castsi256_si128(g.xm), _mm_set1_epi8(0x10));
    const auto pab = _mm_max_epu8(_mm_shuffle_epi8(tb, ia), ia);
    _mm256_store_si256(reinterpret_cast<__m256i*>(b + T::PT),
        _mm256_blend_epi32(g.xm, _mm256_castsi128_si256(pab), 0x0F));
    if constexpr (BYTE_TABLES)
    {
        _mm_storel_epi64(reinterpret_cast<__m128i*>(b + T::VA), _mm256_castsi256_si128(g.lo));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(b + T::VB - 16), g.lo);
        _mm_storel_epi64(reinterpret_cast<__m128i*>(b + T::VB + 8), _mm_setzero_si128());
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(b + T::HB - 16), g.hi);
        _mm_store_si128(reinterpret_cast<__m128i*>(b + T::HA), _mm256_castsi256_si128(g.hi));
        const size_t e2 = b[e - 0x80 + T::TA];
        out[0] = static_cast<uint16_t>(b[e - 0x80 + T::VA] | (b[e - 0x80 + T::HA] << 8));
        out[1] = static_cast<uint16_t>(b[e2 - 0x80 + T::VB] | (b[e2 - 0x80 + T::HB] << 8));
    }
    else
    {
        // VB: entries 8..15 first (the low lane is junk at VB), then entries 0..7 over it.
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(b + T::VB), g.hi);
        JDA_STORE_BARRIER();
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(b + T::VB - 16), g.lo);
        _mm_store_si128(reinterpret_cast<__m128i*>(b + T::VA), _mm256_castsi256_si128(g.lo));
        JDA_STORE_BARRIER();
        _mm_store_si128(reinterpret_cast<__m128i*>(b + T::VA + 16), _mm256_castsi256_si128(g.hi));
        const size_t e2 = b[e - 0x80 + T::TA];
        out[0] = *reinterpret_cast<const uint16_t*>(b + T::VA + 2 * (e - 0x80));
        out[1] = *reinterpret_cast<const uint16_t*>(b + T::VB + 2 * (e2 - 0x80));
    }
    e = b[e - 0x80 + T::PT];  // The only loop-carried dependency.
}

template <bool BYTE_TABLES>
__attribute__((target("avx2"))) void jda_avx2(const u8* code, size_t size, u64* bits)
{
    JdaTables t;
    auto* const out = reinterpret_cast<uint16_t*>(bits);
    size_t e = 0x80;  // Block entry, encoded 0x80 + offset.
    const size_t num_blocks = (size + 31) / 32;
    if (num_blocks == 0)
        return;
    auto a = jda_core<BYTE_TABLES>(code);
    size_t q = 1;
    for (; q + 1 < num_blocks; q += 2)
    {
        if constexpr (BYTE_TABLES)
            MCA_BEGIN("jda_bytes_avx2");
        else
            MCA_BEGIN("jda_avx2");
        const auto b = jda_core<BYTE_TABLES>(code + 32 * q);
        jda_finish<BYTE_TABLES>(a, t.b, e, out + 2 * (q - 1));
        a = jda_core<BYTE_TABLES>(code + 32 * (q + 1));
        jda_finish<BYTE_TABLES>(b, t.b, e, out + 2 * q);
        if constexpr (BYTE_TABLES)
            MCA_END("jda_bytes_avx2");
        else
            MCA_END("jda_avx2");
    }
    if (q < num_blocks)
    {
        const auto b = jda_core<BYTE_TABLES>(code + 32 * q);
        jda_finish<BYTE_TABLES>(a, t.b, e, out + 2 * (q - 1));
        jda_finish<BYTE_TABLES>(b, t.b, e, out + 2 * q);
    }
    else
        jda_finish<BYTE_TABLES>(a, t.b, e, out + 2 * (q - 1));
}

void gfinal_avx2(const u8* code, size_t size, u64* bits) { jda_avx2<false>(code, size, bits); }
void gfinal_bytes_avx2(const u8* code, size_t size, u64* bits) { jda_avx2<true>(code, size, bits); }

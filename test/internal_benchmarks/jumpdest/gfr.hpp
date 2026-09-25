// gfr_avx2: round-2 AVX2 (x86-64-v3) JUMPDEST-analysis kernel, clean version of gfa_avx2.
//
// Core (per 32-byte block = 2 lanes = 4 half-groups of 8 bytes; as in g8/gfinal): pointer doubling
// with self-loop sinks (3 rounds) gives, for every entry of every half, the last start of the
// half and the JUMPDEST bits visited; n[x] gives the half exit, max(n, n[n]) merges the halves
// into [TA | TB] (0x80 + entry into the next 16-byte group); h = v[n] plus two unpacks build the
// 16-bit JUMPDEST words of both groups.
// Glue: TA is stored as xmm; xm = [TA | TB] is stored whole into the PT slot, TB is reloaded from
// it (a load instead of a shuffle-port vextracti128) and the composed pab = TB[TA - 16] overwrites
// the low half, so PT = [pab | TB] without a blend. VB gets the high lanes via two staggered ymm
// stores whose low lanes land in dead space; VA gets the low lanes via two xmm stores
// (ZEN_STORES: one vinserti128 + one ymm store instead, better on Zen 3, worse on Intel).
// Loop: software-pipelined x2 (core of block q with the finish of block q-1) with a single exact
// induction variable (add + cmp + jne).
// Requires: code 32-byte aligned with >= 31 readable (zero) bytes after it. Output: the
// zero-initialized (size + 64) / 64 words, written as 16-bit words. No data-dependent control flow.
namespace gfr
{
constexpr size_t PT = 0;    // u8: 0x80 + next block entry; [16,32) = TB; tail s >= 32: 0x80+s-32.
constexpr size_t TA = 48;   // u8: 0x80 + entry into group B; tail s >= 16: 0x80 + s - 16.
constexpr size_t VA = 96;   // u16: group-A JUMPDEST words by block entry; zero tail (s >= 16).
constexpr size_t VB = 192;  // u16: group-B words by the entry into B; zero tail. VB-16 is dead.
constexpr size_t SLOT = 320;

inline void init(u8* b)
{
    std::memset(b, 0, SLOT);
    for (unsigned i = 32; i < 40; ++i)
        b[PT + i] = static_cast<u8>(0x80 + i - 32);
    for (unsigned i = 16; i < 40; ++i)
        b[TA + i] = static_cast<u8>(0x80 + i - 16);
}

struct Core
{
    __m256i xm, vl, vh;
};

__attribute__((target("avx2"), always_inline)) inline Core core(const u8* code)
{
    // 0x79 + lane within the half - 0x5f: max_epi8(c, 0x5f) + iota = 0x78 + next position
    // relative to the half (negative iff the next start leaves the half).
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
    auto x = _mm256_max_epi8(_mm256_xor_si256(y, k8), lane);  // In-half pointer, or a sink.
    auto v = _mm256_and_si256(_mm256_cmpeq_epi8(c, _mm256_set1_epi8(0x5b)), bit);
    for (int k = 0; k < 3; ++k)
    {
        v = _mm256_or_si256(v, _mm256_shuffle_epi8(v, x));
        x = _mm256_shuffle_epi8(x, x);
    }
    n = _mm256_shuffle_epi8(n, x);                                  // Half exits.
    const auto xm = _mm256_max_epu8(n, _mm256_shuffle_epi8(n, n));  // [TA | TB]
    const auto h = _mm256_shuffle_epi8(v, n);  // 1st-half entries: 2nd-half JUMPDEST byte.
    return {xm, _mm256_unpacklo_epi8(v, h), _mm256_unpackhi_epi8(_mm256_setzero_si256(), v)};
}

#define GFR_BARRIER(p, n) asm("" : "+m"(*reinterpret_cast<u8(*)[n]>(p)))  // No store fusion.

template <bool ZEN_STORES>
__attribute__((target("avx2"), always_inline)) inline void finish(
    const Core& g, u8* b, size_t& e, uint16_t* out)
{
    _mm_store_si128(reinterpret_cast<__m128i*>(b + TA), _mm256_castsi256_si128(g.xm));
    _mm256_store_si256(reinterpret_cast<__m256i*>(b + PT), g.xm);
    GFR_BARRIER(b + PT, 32);
    const auto tb = _mm_load_si128(reinterpret_cast<const __m128i*>(b + PT + 16));
    const auto ia = _mm_sub_epi8(_mm256_castsi256_si128(g.xm), _mm_set1_epi8(0x10));
    _mm_store_si128(
        reinterpret_cast<__m128i*>(b + PT), _mm_max_epu8(_mm_shuffle_epi8(tb, ia), ia));
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(b + VB), g.vh);
    GFR_BARRIER(b + VB, 32);
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(b + VB - 16), g.vl);
    if constexpr (ZEN_STORES)
    {
        _mm256_store_si256(reinterpret_cast<__m256i*>(b + VA),
            _mm256_inserti128_si256(g.vl, _mm256_castsi256_si128(g.vh), 1));
    }
    else
    {
        _mm_store_si128(reinterpret_cast<__m128i*>(b + VA), _mm256_castsi256_si128(g.vl));
        GFR_BARRIER(b + VA, 16);
        _mm_store_si128(reinterpret_cast<__m128i*>(b + VA + 16), _mm256_castsi256_si128(g.vh));
    }
    const size_t e2 = b[TA + e - 0x80];
    out[0] = *reinterpret_cast<const uint16_t*>(b + VA + 2 * (e - 0x80));
    out[1] = *reinterpret_cast<const uint16_t*>(b + VB + 2 * (e2 - 0x80));
    e = b[PT + e - 0x80];  // The only loop-carried dependency.
}

template <bool ZEN_STORES>
__attribute__((target("avx2"), always_inline)) inline void run(
    const u8* code, size_t size, u64* bits)
{
    const size_t n = (size + 31) / 32;
    if (n == 0)
        return;
    alignas(64) u8 b[SLOT];
    init(b);
    size_t e = 0x80;  // 0x80 + entry offset into the current block.
    // Iteration k handles the cores of blocks 2i+1, 2i+2 and the finishes of 2i, 2i+1 for
    // i = 0..m-1 (k = 8 * (i - m) is the output byte offset relative to out_end).
    const size_t m = (n - 1) / 2;
    auto* const out_end = reinterpret_cast<u8*>(bits) + 8 * m;
    const u8* const code_end = code + 64 * m + 32;
    auto a = core(code);
    for (ptrdiff_t k = -8 * static_cast<ptrdiff_t>(m); k != 0; k += 8)
    {
        auto* const o = reinterpret_cast<uint16_t*>(out_end + k);
        const auto c = core(code_end + 8 * k);
        finish<ZEN_STORES>(a, b, e, o);
        a = core(code_end + 8 * k + 32);
        finish<ZEN_STORES>(c, b, e, o + 2);
    }
    auto* const o = reinterpret_cast<uint16_t*>(out_end);
    if (2 * m + 1 < n)  // One block left after the pipelined pairs.
    {
        const auto c = core(code_end);
        finish<ZEN_STORES>(a, b, e, o);
        finish<ZEN_STORES>(c, b, e, o + 2);
    }
    else
        finish<ZEN_STORES>(a, b, e, o);
}
}  // namespace gfr

__attribute__((target("avx2"))) void gfr_avx2(const u8* code, size_t size, u64* bits)
{
    gfr::run<false>(code, size, bits);
}
__attribute__((target("avx2"))) void gfrz_avx2(const u8* code, size_t size, u64* bits)
{
    gfr::run<true>(code, size, bits);
}

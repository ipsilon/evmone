// The single-source g8 kernel instantiated for AVX2; see vt2.hpp.
#include <immintrin.h>
#include <cstddef>
#include <cstdint>

using u8 = uint8_t;
using u64 = uint64_t;
#include "vt2.hpp"

void vt2_avx2_words(const u8* code, size_t size, u64* bits);
void vt2_avx2_bytes(const u8* code, size_t size, u64* bits);

void vt2_avx2_words(const u8* code, size_t size, u64* bits)
{
    vt2::run<vt2::u8x32, vt2::Layout::words>(code, size, bits);
}
void vt2_avx2_bytes(const u8* code, size_t size, u64* bits)
{
    vt2::run<vt2::u8x32, vt2::Layout::bytes>(code, size, bits);
}

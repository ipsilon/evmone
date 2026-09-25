// The single-source g9np kernel instantiated for AVX2; see vt2.hpp.
#include <immintrin.h>
#include <cstddef>
#include <cstdint>

using u8 = uint8_t;
using u64 = uint64_t;
#include "vt2.hpp"

void vt2_avx2(const u8* code, size_t size, u64* bits)
{
    vt2::run<vt2::u8x32>(code, size, bits);
}

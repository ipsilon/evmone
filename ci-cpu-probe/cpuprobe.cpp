// Prints the CPU brand and whether AVX2, BMI2 and ADX are available.
#include <cstdio>
#include <cstring>
#ifdef _MSC_VER
#include <immintrin.h>
#include <intrin.h>
static void cpuid(unsigned leaf, unsigned sub, unsigned r[4])
{
    __cpuidex(reinterpret_cast<int*>(r), static_cast<int>(leaf), static_cast<int>(sub));
}
static unsigned long long xgetbv0()
{
    return _xgetbv(0);
}
#else
#include <cpuid.h>
static void cpuid(unsigned leaf, unsigned sub, unsigned r[4])
{
    __cpuid_count(leaf, sub, r[0], r[1], r[2], r[3]);
}
static unsigned long long xgetbv0()
{
    unsigned a = 0;
    unsigned d = 0;
    __asm__ volatile("xgetbv" : "=a"(a), "=d"(d) : "c"(0));
    return (static_cast<unsigned long long>(d) << 32) | a;
}
#endif

int main()
{
    unsigned r[4]{};
    char brand[49]{};
    cpuid(0x80000000, 0, r);
    if (r[0] >= 0x80000004)
    {
        for (unsigned i = 0; i < 3; ++i)
        {
            cpuid(0x80000002 + i, 0, r);
            std::memcpy(brand + 16 * i, r, 16);
        }
    }

    cpuid(0, 0, r);
    const unsigned max_leaf = r[0];

    cpuid(1, 0, r);
    const bool osxsave = (r[2] >> 27) & 1;
    const bool os_ymm = osxsave && (xgetbv0() & 6) == 6;

    bool avx2 = false;
    bool bmi2 = false;
    bool adx = false;
    if (max_leaf >= 7)
    {
        cpuid(7, 0, r);
        avx2 = (r[1] >> 5) & 1;
        bmi2 = (r[1] >> 8) & 1;
        adx = (r[1] >> 19) & 1;
    }

    std::printf("PROBE cpu=\"%s\" avx2=%d os_ymm=%d bmi2=%d adx=%d\n", brand, avx2, os_ymm, bmi2, adx);
    return 0;
}

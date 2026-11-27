
#define STRINGIFY_HELPER(X) #X
#define STRINGIFY(X) STRINGIFY_HELPER(X)

#if defined(__GNUC__) && __GNUC__ >= 12
#define CPU_FEATURE "x86-64-v" STRINGIFY(EVMONE_X86_64_ARCH_LEVEL)
#else
// TODO(clang-18): x86 architecture levels are supported in __builtin_cpu_supports()
//   since GCC 11 and Clang 18. Use approximations as fallback.
#if EVMONE_X86_64_ARCH_LEVEL == 2
#define CPU_FEATURE "sse4.2"
#elif EVMONE_X86_64_ARCH_LEVEL == 3
#define CPU_FEATURE "avx2"
#endif
#endif

#ifndef CPU_FEATURE
#error "EVMONE_X86_64_ARCH_LEVEL: Unsupported x86-64 architecture level"
#endif

#include <cstdio>
#include <cstdlib>

static bool cpu_check = [] noexcept {
    if (!__builtin_cpu_supports(CPU_FEATURE))
    {
        (void)std::fputs("CPU does not support " CPU_FEATURE "\n", stderr);
        std::abort();
    }
    return false;
}();

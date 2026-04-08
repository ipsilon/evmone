// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "mulmod.hpp"

#ifdef __x86_64__

namespace evmone::crypto
{
namespace
{
/// A 256-bit value for precise inline asm memory constraints.
/// Using "m" on this struct (instead of a blanket "memory" clobber) tells the compiler
/// exactly which memory the asm block accesses, preventing unnecessary register spills.
struct mem256
{
    uint64_t w[4];
};

/// Computes a[0..4] = x[0..3] * y_word.
/// Uses mulx (BMI2) with a plain add/adc carry chain.
inline void init_mul(uint64_t& a0, uint64_t& a1, uint64_t& a2, uint64_t& a3, uint64_t& a4,
    const mem256& x, uint64_t y_word) noexcept
{
    uint64_t t;
    asm("mulxq (%[x]), %[a0], %[a1]\n\t"
        "mulxq 8(%[x]), %[t], %[a2]\n\t"
        "addq %[t], %[a1]\n\t"
        "mulxq 16(%[x]), %[t], %[a3]\n\t"
        "adcq %[t], %[a2]\n\t"
        "mulxq 24(%[x]), %[t], %[a4]\n\t"
        "adcq %[t], %[a3]\n\t"
        "adcq $0, %[a4]"
        : [a0] "=&r"(a0), [a1] "=&r"(a1), [a2] "=&r"(a2),
          [a3] "=&r"(a3), [a4] "=&r"(a4), [t] "=&r"(t)
        : [x] "r"(&x), "d"(y_word), "m"(x));
}

/// Multiply-accumulate: a[0..3] += x[0..3] * y_word (in rdx), producing carry c.
/// Uses adox (OF chain) for low products, adcx (CF chain) for high products.
/// The residual CF from the adcx chain is combined with prev_carry via a final adc.
/// r_carry: on input, the carry from the previous reduce. On output, overflow flag.
inline void mul_add(uint64_t& a0, uint64_t& a1, uint64_t& a2, uint64_t& a3,
    uint64_t& c, uint64_t& r_carry, const mem256& x, uint64_t y_word) noexcept
{
    uint64_t t1, t2;
    asm("xorq %[c], %[c]\n\t"
        "mulxq (%[x]), %[t1], %[t2]\n\t"
        "adoxq %[t1], %[a0]\n\t"
        "adcxq %[t2], %[a1]\n\t"
        "mulxq 8(%[x]), %[t1], %[t2]\n\t"
        "adoxq %[t1], %[a1]\n\t"
        "adcxq %[t2], %[a2]\n\t"
        "mulxq 16(%[x]), %[t1], %[t2]\n\t"
        "adoxq %[t1], %[a2]\n\t"
        "adcxq %[t2], %[a3]\n\t"
        "mulxq 24(%[x]), %[t1], %[t2]\n\t"
        "adoxq %[t1], %[a3]\n\t"
        "adcxq %[c], %[t2]\n\t"
        "adoxq %[t2], %[c]\n\t"
        "movq $0, %[t1]\n\t"
        "adoxq %[t1], %[c]\n\t"
        "adcq %[prev], %[c]\n\t"
        "setb %b[prev]"
        : [a0] "+r"(a0), [a1] "+r"(a1), [a2] "+r"(a2), [a3] "+r"(a3),
          [c] "=&r"(c), [prev] "+q"(r_carry), [t1] "=&r"(t1), [t2] "=&r"(t2)
        : [x] "r"(&x), "d"(y_word), "m"(x)
        : "cc");
}

/// Montgomery reduce: adds mod[0..3] * m (in rdx) to the accumulator, consuming a0.
/// Propagates through a1..a4 into carry c. Uses 3-instruction tail.
inline void reduce(uint64_t a0, uint64_t& a1, uint64_t& a2, uint64_t& a3, uint64_t& a4,
    uint64_t& c, const mem256& mod, uint64_t m) noexcept
{
    uint64_t z, lo, hi;
    asm("xorq %[z], %[z]\n\t"
        "mulxq (%[mod]), %[lo], %[hi]\n\t"
        "adcxq %[a0], %[lo]\n\t"
        "adoxq %[hi], %[a1]\n\t"
        "mulxq 8(%[mod]), %[lo], %[hi]\n\t"
        "adcxq %[lo], %[a1]\n\t"
        "adoxq %[hi], %[a2]\n\t"
        "mulxq 16(%[mod]), %[lo], %[hi]\n\t"
        "adcxq %[lo], %[a2]\n\t"
        "adoxq %[hi], %[a3]\n\t"
        "mulxq 24(%[mod]), %[lo], %[hi]\n\t"
        "adcxq %[lo], %[a3]\n\t"
        "adoxq %[hi], %[a4]\n\t"
        "adcxq %[z], %[a4]\n\t"
        "adoxq %[z], %[c]\n\t"
        "adcq $0, %[c]"
        : [a1] "+r"(a1), [a2] "+r"(a2), [a3] "+r"(a3), [a4] "+r"(a4),
          [c] "+r"(c), [z] "=&r"(z), [lo] "=&r"(lo), [hi] "=&r"(hi)
        : [a0] "r"(a0), [mod] "r"(&mod), "d"(m), "m"(mod)
        : "cc");
}

}  // namespace


/// Almost Montgomery Multiplication (AMM) for 256-bit operands.
/// Computes r = x * y * R^{-1} mod m using the CIOS method with inline x86-64 assembly.
/// Uses mulx (BMI2) and adcx/adox (ADX) for dual carry chain interleaving.
///
/// The no-tree-vectorize attribute prevents GCC from replacing the four scalar stores
/// at the end with vmovdqa, which causes a store-forwarding stall.
[[gnu::target("bmi2,adx"), gnu::optimize("no-tree-vectorize")]]
#ifdef __clang__
[[clang::preserve_none]]
#endif
void mont_amm_256(mem256& r, const mem256& x, const mem256& y, const mem256& mod,
    uint64_t mod_inv) noexcept
{
    uint64_t t0, t1, t2, t3, t4;

    // Iteration 0: initial multiply x[] * y[0].
    init_mul(t0, t1, t2, t3, t4, x, y.w[0]);

    // Iteration 0: reduce.
    {
        const auto m = t0 * mod_inv;
        uint64_t c = 0;
        reduce(t0, t1, t2, t3, t4, c, mod, m);
        // Shift: t0 consumed. New accumulator: t1, t2, t3, t4, c.
        t0 = t1;
        t1 = t2;
        t2 = t3;
        t3 = t4;
        t4 = c;
    }

    // Iterations 1-3: mul_add + reduce.
#pragma GCC unroll 3
    for (int i = 1; i < 4; ++i)
    {
        // Multiply-accumulate: t[0..3] += x[] * y[i].
        // The mul_add combines its carry with the previous reduce carry (t4)
        // via adc, producing the combined carry in mc and overflow in t4.
        uint64_t mc;
        mul_add(t0, t1, t2, t3, mc, t4, x, y.w[i]);
        // After: mc = mul_carry + t4 + CF. t4 = overflow (0 or 1).

        // Reduce.
        const auto m = t0 * mod_inv;
        reduce(t0, t1, t2, t3, mc, t4, mod, m);

        // Shift: t0 consumed. New accumulator: t1, t2, t3, mc, t4.
        t0 = t1;
        t1 = t2;
        t2 = t3;
        t3 = mc;
    }

    // Conditional subtraction: if carry is set, subtract mod.
    if (t4 != 0)
    {
        bool b = false;
        std::tie(t0, b) = subc(t0, mod.w[0]);
        std::tie(t1, b) = subc(t1, mod.w[1], b);
        std::tie(t2, b) = subc(t2, mod.w[2], b);
        std::tie(t3, b) = subc(t3, mod.w[3], b);
    }

    // Store result.
    r.w[0] = t0;
    r.w[1] = t1;
    r.w[2] = t2;
    r.w[3] = t3;
}

}  // namespace evmone::crypto

#endif  // __x86_64__

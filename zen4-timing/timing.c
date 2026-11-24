// Latency and throughput of the instructions of llvm/llvm-project#244230, timed with
// rdtscp where no performance counters are available (VMs). Core cycles come from a
// chain of dependent 1-cycle adds timed right before each test.
//
// Latency: each instruction depends on the previous one through its destination.
// Throughput: 8 (or 6 for mask destinations) independent destinations.
// Usage: timing [avx512]   (without the argument only the SSE/AVX2 tests run)
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <x86intrin.h>

#define S(x) #x
#define REP(n, body) ".rept " S(n) "\n\t" body "\n\t.endr\n\t"
#define CLOBBERS2                                                                         \
  "memory", "cc", "rax", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", \
      "xmm9"
#define CLOBBERS512 CLOBBERS2, "xmm16", "xmm17", "xmm18", "xmm25", "k1", "k2", "k3", "k4", "k5", "k6", "k7"
#define AVX512_TARGET __attribute__((target("avx512f,avx512bw,avx512vl,avx512bitalg")))

// Zero all vector registers used, k1 = all ones, xmm1/xmm2/xmm17/xmm18 = 1.0f (or 1.0).
#define SETUP_F                                                                       \
  "vpxord %%zmm0, %%zmm0, %%zmm0\n\t"                                                \
  "vmovdqa64 %%zmm0, %%zmm3\n\tvmovdqa64 %%zmm0, %%zmm4\n\tvmovdqa64 %%zmm0, %%zmm5\n\t" \
  "vmovdqa64 %%zmm0, %%zmm6\n\tvmovdqa64 %%zmm0, %%zmm7\n\tvmovdqa64 %%zmm0, %%zmm8\n\t" \
  "vmovdqa64 %%zmm0, %%zmm9\n\tvmovdqa64 %%zmm0, %%zmm16\n\tvmovdqa64 %%zmm0, %%zmm25\n\t" \
  "kxnorq %%k0, %%k0, %%k1\n\t"                                                     \
  "movl $0x3f800000, %%eax\n\tvmovd %%eax, %%xmm1\n\tvmovaps %%xmm1, %%xmm2\n\t"      \
  "vmovaps %%xmm1, %%xmm17\n\tvmovaps %%xmm1, %%xmm18\n\t"
#define SETUP_D                                                                       \
  SETUP_F "movabsq $0x3ff0000000000000, %%rax\n\tvmovq %%rax, %%xmm1\n\tvmovaps %%xmm1, %%xmm2\n\t"
// The same without AVX-512 instructions.
#define SETUP_AVX2                                                                     \
  "vpxor %%xmm0, %%xmm0, %%xmm0\n\t"                                                 \
  "vmovdqa %%ymm0, %%ymm3\n\tvmovdqa %%ymm0, %%ymm4\n\tvmovdqa %%ymm0, %%ymm5\n\t"     \
  "vmovdqa %%ymm0, %%ymm6\n\tvmovdqa %%ymm0, %%ymm7\n\tvmovdqa %%ymm0, %%ymm8\n\t"    \
  "vmovdqa %%ymm0, %%ymm9\n\t"                                                       \
  "movl $0x3f800000, %%eax\n\tvmovd %%eax, %%xmm1\n\tvmovaps %%xmm1, %%xmm2\n\t"

#define BENCH_(ATTR, CLOB, NAME, SETUP, BODY, NBODY)                                    \
  ATTR static void NAME(uint64_t n) {                                                     \
    __asm__ volatile(SETUP "1:\n\t" REP(25, BODY) "dec %0\n\tjnz 1b\n\tvzeroupper\n\t"   \
                     : "+r"(n)                                                            \
                     :                                                                    \
                     : CLOB);                                                             \
  }                                                                                       \
  enum { NAME##_per_iter = 25 * (NBODY) };
#define BENCH(NAME, SETUP, BODY, NBODY) BENCH_(, CLOBBERS2, NAME, SETUP, BODY, NBODY)
#define BENCH512(NAME, SETUP, BODY, NBODY) BENCH_(AVX512_TARGET, CLOBBERS512, NAME, SETUP, BODY, NBODY)

// Calibration: 25 x 4 dependent adds per iteration.
static void add_chain(uint64_t n) {
  uint64_t x = 0;
  __asm__ volatile("1:\n\t" REP(100, "add $1, %1") "dec %0\n\tjnz 1b" : "+r"(n), "+r"(x) :: "cc");
}

#define T8(i) i "%%xmm0\n\t" i "%%xmm1\n\t" i "%%xmm2\n\t" i "%%xmm3\n\t" i "%%xmm4\n\t" i "%%xmm5\n\t" i "%%xmm6\n\t" i "%%xmm7"
#define T8Y(i) i "%%ymm0\n\t" i "%%ymm1\n\t" i "%%ymm2\n\t" i "%%ymm3\n\t" i "%%ymm4\n\t" i "%%ymm5\n\t" i "%%ymm6\n\t" i "%%ymm7"
#define T8Z(i) i "%%zmm0\n\t" i "%%zmm1\n\t" i "%%zmm2\n\t" i "%%zmm3\n\t" i "%%zmm4\n\t" i "%%zmm5\n\t" i "%%zmm6\n\t" i "%%zmm7"
#define T6K(i) i "%%k2\n\t" i "%%k3\n\t" i "%%k4\n\t" i "%%k5\n\t" i "%%k6\n\t" i "%%k7"

// --- SSE / AVX2 (also run on Zen 3 to check the method) ---
BENCH(pshufb_lat, SETUP_AVX2, REP(4, "pshufb %%xmm9, %%xmm0"), 4)
BENCH(pshufb_tp, SETUP_AVX2, "pshufb %%xmm9, %%xmm0\n\tpshufb %%xmm9, %%xmm3\n\tpshufb %%xmm9, %%xmm4\n\tpshufb %%xmm9, %%xmm5\n\tpshufb %%xmm9, %%xmm6\n\tpshufb %%xmm9, %%xmm7\n\tpshufb %%xmm9, %%xmm8\n\tpshufb %%xmm9, %%xmm1", 8)
BENCH(vpshufb_x_lat, SETUP_AVX2, REP(4, "vpshufb %%xmm9, %%xmm0, %%xmm0"), 4)
BENCH(vpshufb_x_ctl_lat, SETUP_AVX2, REP(4, "vpshufb %%xmm0, %%xmm9, %%xmm0"), 4)
BENCH(vpshufb_x_tp, SETUP_AVX2, T8("vpshufb %%xmm9, %%xmm8, "), 8)
BENCH(vpshufb_y_lat, SETUP_AVX2, REP(4, "vpshufb %%ymm9, %%ymm0, %%ymm0"), 4)
BENCH(vpshufb_y_tp, SETUP_AVX2, T8Y("vpshufb %%ymm9, %%ymm8, "), 8)
BENCH(vfmsub231ss_vex_lat, SETUP_AVX2, REP(4, "vfmsub231ss %%xmm2, %%xmm1, %%xmm0"), 4)
BENCH(vfmsub231ss_vex_tp, SETUP_AVX2, "vfmsub231ss %%xmm2, %%xmm1, %%xmm0\n\tvfmsub231ss %%xmm2, %%xmm1, %%xmm3\n\tvfmsub231ss %%xmm2, %%xmm1, %%xmm4\n\tvfmsub231ss %%xmm2, %%xmm1, %%xmm5\n\tvfmsub231ss %%xmm2, %%xmm1, %%xmm6\n\tvfmsub231ss %%xmm2, %%xmm1, %%xmm7\n\tvfmsub231ss %%xmm2, %%xmm1, %%xmm8\n\tvfmsub231ss %%xmm2, %%xmm1, %%xmm9", 8)

// --- AVX-512 ---
BENCH512(vpshufb_x_evex_lat, SETUP_F, REP(4, "vpshufb %%xmm25, %%xmm16, %%xmm16"), 4)
BENCH512(vpshufb_y_evex_lat, SETUP_F, REP(4, "vpshufb %%ymm25, %%ymm16, %%ymm16"), 4)
BENCH512(vpshufb_z_lat, SETUP_F, REP(4, "vpshufb %%zmm9, %%zmm0, %%zmm0"), 4)
BENCH512(vpshufb_z_tp, SETUP_F, T8Z("vpshufb %%zmm9, %%zmm8, "), 8)
BENCH512(vpshufb_x_kz_lat, SETUP_F, REP(4, "vpshufb %%xmm9, %%xmm0, %%xmm0 %{%%k1%} %{z%}"), 4)
BENCH512(vpshufb_x_kz_tp, SETUP_F, "vpshufb %%xmm9, %%xmm8, %%xmm0 %{%%k1%} %{z%}\n\tvpshufb %%xmm9, %%xmm8, %%xmm1 %{%%k1%} %{z%}\n\tvpshufb %%xmm9, %%xmm8, %%xmm2 %{%%k1%} %{z%}\n\tvpshufb %%xmm9, %%xmm8, %%xmm3 %{%%k1%} %{z%}\n\tvpshufb %%xmm9, %%xmm8, %%xmm4 %{%%k1%} %{z%}\n\tvpshufb %%xmm9, %%xmm8, %%xmm5 %{%%k1%} %{z%}\n\tvpshufb %%xmm9, %%xmm8, %%xmm6 %{%%k1%} %{z%}\n\tvpshufb %%xmm9, %%xmm8, %%xmm7 %{%%k1%} %{z%}", 8)
BENCH512(vpshufb_y_kz_tp, SETUP_F, "vpshufb %%ymm9, %%ymm8, %%ymm0 %{%%k1%} %{z%}\n\tvpshufb %%ymm9, %%ymm8, %%ymm1 %{%%k1%} %{z%}\n\tvpshufb %%ymm9, %%ymm8, %%ymm2 %{%%k1%} %{z%}\n\tvpshufb %%ymm9, %%ymm8, %%ymm3 %{%%k1%} %{z%}\n\tvpshufb %%ymm9, %%ymm8, %%ymm4 %{%%k1%} %{z%}\n\tvpshufb %%ymm9, %%ymm8, %%ymm5 %{%%k1%} %{z%}\n\tvpshufb %%ymm9, %%ymm8, %%ymm6 %{%%k1%} %{z%}\n\tvpshufb %%ymm9, %%ymm8, %%ymm7 %{%%k1%} %{z%}", 8)
// VPSHUFBITQMB writes a mask; the latency chain goes back to xmm0 through vpmovm2b,
// so it is the latency of the pair. The same pair with vpcmpeqb is the reference.
BENCH512(vpshufbitqmb_x_pair_lat, SETUP_F, REP(4, "vpshufbitqmb %%xmm9, %%xmm0, %%k2\n\tvpmovm2b %%k2, %%xmm0"), 4)
BENCH512(vpshufbitqmb_y_pair_lat, SETUP_F, REP(4, "vpshufbitqmb %%ymm9, %%ymm0, %%k2\n\tvpmovm2b %%k2, %%ymm0"), 4)
BENCH512(vpshufbitqmb_z_pair_lat, SETUP_F, REP(4, "vpshufbitqmb %%zmm9, %%zmm0, %%k2\n\tvpmovm2b %%k2, %%zmm0"), 4)
BENCH512(vpcmpeqb_x_pair_lat, SETUP_F, REP(4, "vpcmpeqb %%xmm9, %%xmm0, %%k2\n\tvpmovm2b %%k2, %%xmm0"), 4)
BENCH512(vpshufbitqmb_x_tp, SETUP_F, T6K("vpshufbitqmb %%xmm9, %%xmm8, "), 6)
BENCH512(vpshufbitqmb_y_tp, SETUP_F, T6K("vpshufbitqmb %%ymm9, %%ymm8, "), 6)
BENCH512(vpshufbitqmb_z_tp, SETUP_F, T6K("vpshufbitqmb %%zmm9, %%zmm8, "), 6)
BENCH512(vfmsub231ss_kz_lat, SETUP_F, REP(4, "vfmsub231ss %%xmm2, %%xmm1, %%xmm0 %{%%k1%} %{z%}"), 4)
BENCH512(vfmsub231ss_k_lat, SETUP_F, REP(4, "vfmsub231ss %%xmm2, %%xmm1, %%xmm0 %{%%k1%}"), 4)
BENCH512(vfmsub231ss_evex_lat, SETUP_F, REP(4, "vfmsub231ss %%xmm18, %%xmm17, %%xmm16"), 4)
BENCH512(vfmadd231ss_kz_lat, SETUP_F, REP(4, "vfmadd231ss %%xmm2, %%xmm1, %%xmm0 %{%%k1%} %{z%}"), 4)
BENCH512(vfmsub231sd_kz_lat, SETUP_D, REP(4, "vfmsub231sd %%xmm2, %%xmm1, %%xmm0 %{%%k1%} %{z%}"), 4)
BENCH512(vfmsub231ss_kz_tp, SETUP_F, "vfmsub231ss %%xmm2, %%xmm1, %%xmm0 %{%%k1%} %{z%}\n\tvfmsub231ss %%xmm2, %%xmm1, %%xmm3 %{%%k1%} %{z%}\n\tvfmsub231ss %%xmm2, %%xmm1, %%xmm4 %{%%k1%} %{z%}\n\tvfmsub231ss %%xmm2, %%xmm1, %%xmm5 %{%%k1%} %{z%}\n\tvfmsub231ss %%xmm2, %%xmm1, %%xmm6 %{%%k1%} %{z%}\n\tvfmsub231ss %%xmm2, %%xmm1, %%xmm7 %{%%k1%} %{z%}\n\tvfmsub231ss %%xmm2, %%xmm1, %%xmm8 %{%%k1%} %{z%}\n\tvfmsub231ss %%xmm2, %%xmm1, %%xmm9 %{%%k1%} %{z%}", 8)

typedef struct {
  const char* name;
  void (*f)(uint64_t);
  int per_iter;
  int avx512;
} test;

#define T(NAME, AVX512) {#NAME, NAME, NAME##_per_iter, AVX512}
static const test tests[] = {
    T(pshufb_lat, 0), T(pshufb_tp, 0), T(vpshufb_x_lat, 0), T(vpshufb_x_ctl_lat, 0), T(vpshufb_x_tp, 0),
    T(vpshufb_y_lat, 0), T(vpshufb_y_tp, 0), T(vfmsub231ss_vex_lat, 0), T(vfmsub231ss_vex_tp, 0),
    T(vpshufb_x_evex_lat, 1), T(vpshufb_y_evex_lat, 1), T(vpshufb_z_lat, 1), T(vpshufb_z_tp, 1),
    T(vpshufb_x_kz_lat, 1), T(vpshufb_x_kz_tp, 1), T(vpshufb_y_kz_tp, 1),
    T(vpshufbitqmb_x_pair_lat, 1), T(vpshufbitqmb_y_pair_lat, 1), T(vpshufbitqmb_z_pair_lat, 1),
    T(vpcmpeqb_x_pair_lat, 1), T(vpshufbitqmb_x_tp, 1), T(vpshufbitqmb_y_tp, 1), T(vpshufbitqmb_z_tp, 1),
    T(vfmsub231ss_kz_lat, 1), T(vfmsub231ss_k_lat, 1), T(vfmsub231ss_evex_lat, 1), T(vfmadd231ss_kz_lat, 1),
    T(vfmsub231sd_kz_lat, 1), T(vfmsub231ss_kz_tp, 1),
};

static uint64_t ticks(void (*f)(uint64_t), uint64_t n) {
  unsigned aux;
  uint64_t t0 = __rdtscp(&aux);
  f(n);
  uint64_t t1 = __rdtscp(&aux);
  return t1 - t0;
}

int main(int argc, char** argv) {
  const int avx512 = argc > 1 && strcmp(argv[1], "avx512") == 0;
  const uint64_t n = 4000;  // iterations; 100..200 instructions each
  for (unsigned k = 0; k < sizeof tests / sizeof tests[0]; ++k) {
    if (tests[k].avx512 && !avx512) continue;
    // The fastest run of the test against the fastest run of the add chain, timed in turn.
    uint64_t best_add = UINT64_MAX, best_test = UINT64_MAX;
    for (int r = 0; r < 41; ++r) {
      const uint64_t a = ticks(add_chain, n), t = ticks(tests[k].f, n);
      if (a < best_add) best_add = a;
      if (t < best_test) best_test = t;
    }
    const double cycle = best_add / (100.0 * n);  // TSC ticks per 1-cycle add
    printf("%-26s %6.2f\n", tests[k].name, best_test / ((double)tests[k].per_iter * n) / cycle);
  }
  return 0;
}

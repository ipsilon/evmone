# BN254 pairing review — findings

Reviewed 2026-08-10. Scope: `lib/evmone_precompiles/pairing/**`, `bn254.{hpp,cpp}`, `ecc.hpp`.

**Line numbers re-anchored to `master` @ 98881638.** They moved once already: #1635 removed 27
lines from `utils.hpp` and #1633 removed 13 from `ecc.hpp`, so anything quoted from an earlier
version of this file is off by that much. Re-grep before trusting a number.

## Measurement harness

Release build, GCC, `build/bin/evmone-precompiles-bench`. gbench `--benchmark_perf_counters`
is unavailable on this box, and wall-clock is useless here (load average ~76), so all numbers
below are **instruction counts via two-point subtraction**:

```
perf stat -e instructions ./build/bin/evmone-precompiles-bench \
    --benchmark_filter=ecpairing --benchmark_min_time={20x,120x}
instructions/iter = (I_120 - I_20) / 100
```

### WARNING: instruction-only numbers below are unreliable — corrected 2026-08-11

Everything in this file measured by **instruction count alone** can be wrong in sign.
Fq2 Karatsuba (#1644) measured −5.8% instructions and turned out to be **+20% cycles /
+19% wall time** (IPC 2.17 → 1.70), because it trades four independent multiplications
for a serialized chain. It was merged and had to be reverted.

Headline **cycles**, confirm with wall time, watch IPC. Instructions remain useful as a
low-noise screen and for provably-equivalent changes (better: `cmp` the binary).
Re-verified with cycles so far: #1544 is a genuine win (cycles −12.8%, time −13.7%, IPC
flat). Not yet re-verified with cycles: #1637 (−0.14% instr), #1643 (−0.023% instr),
and the ξ estimate (#1). Use `scratchpad/ab3.py BASE_BIN CHANGE_BIN` for all three
metrics at once.

### Measurement method — corrected 2026-08-10

**Single-shot two-point subtraction is not reliable below ~0.1%.** Totals move ~0.06%
run-to-run (gbench appears to do load-dependent calibration work before the measured run), which
the /100 division does not remove. A single shot put finding #6 at −0.059%; the paired method below
put it at −0.142%.

**Use paired, interleaved A/B instead** (`scratchpad/ab.sh`): build both variants, keep *both
binaries*, then alternate base/change runs within each rep so drift cancels, and pair the 20x/120x
points by rep. 4 reps resolves ~0.02% and reports a sign-consistency check across reps.

Consequence: everything in the table under ~0.4% was single-shot and carries ±0.06%. The two big
wins (−10.5%, −5.6%) are far above that and unaffected. Re-measure small items with `ab.sh` before
quoting them.

Baselines: **24,351,463** @ 22c6d373, **24,337,284** @ ce6ef36f, **24,253,277** @ 98881638 (paired).

| Variant | instr/iter | vs master |
|---|---|---|
| master baseline | 24,351,463 | — |
| `mul_by_ksi` (13 sites) | 21,800,097 | **−10.5%** |
| Fq2 Karatsuba | 22,988,564 | **−5.6%** |
| both stacked | 20,774,527 | **−14.7%** |
| `add()` reuse `V` | 24,261,147 | −0.32% (single-shot) → **MERGED** #1636 |
| `bn254::dbl` → `ecc::dbl` | 24,277,659 | −0.30% (single-shot) → **MERGED** #1635 |
| `multiply_by_lin_func_value` by-ref | 24,338,222 | −0.05% single-shot, i.e. **within uncertainty — re-measure with ab.sh** |
| Frobenius powers direct (#6) | 24,218,781 | **−0.142%** (paired, 4 reps, sign-consistent) → PR #1637 |

Patches saved (scratchpad, session-local — **re-derive if lost**):
`mul_by_ksi.patch`, `ksi_plus_karatsuba.patch`.

All variants above passed 1172/1172 unit tests.

---

## Findings

Ranked by value. CONFIRMED = verified by me (measurement, compile test, or numeric identity
check). PLAUSIBLE = credible but not verified to the point of quoting a number.

### 1. ξ multiplication uses a general Fq2 mul at all 13 sites — CONFIRMED, −10.5%

`fields.hpp:74,75,92,130,131,134,150`; `utils.hpp:368,372,388,414`; `pairing.cpp:23,25`

ξ = 9 + u and Fq² = Fq[u]/(u²+1), so `(a₀+a₁u)·ξ = (9a₀−a₁) + (a₀+9a₁)u` — **zero** base-field
muls (9x = 3 doublings + 1 add). Current generic `multiply(Fq2,Fq2)` costs 4 Fq muls per site.
~550 ξ-muls per pair in the Miller loop alone (6 per `square(Fq12)`, 2 per sparse line mul),
plus 4 per `cyclotomic_square` in the final exponentiation.

Identity verified over 2000 random inputs mod p. Every `ModArith` add fully reduces, so the
addition chain is safe. This is the standard `mul_by_nonresidue` every production BN254 library has;
no such helper exists anywhere in this repo.

### 2. Fq2 multiply is schoolbook (4M) where Karatsuba needs 3M — CONFIRMED, −5.6%

`fields.hpp:45`

`t0=a0*b0; t1=a1*b1; c0=t0-t1; c1=(a0+a1)*(b0+b1)-t0-t1`. Trades one Montgomery mul for three
modular adds; pays off because Fq mul ≈ 4–6× an add here. **Every other level of the tower is
already Karatsuba** (`multiply(Fq6)` at :63, `multiply(Fq12)` at :82) and `sqr(Fq2)` at :53 is
already optimal — Fq2 mul was the one left behind.

Composes with #1 to −14.7% (not purely additive: `mul_by_ksi` deletes muls Karatsuba would
otherwise have cheapened).

### 3. Sparse line multiply: 20 Fq2 muls where 13 suffice — PLAUSIBLE, ~6–8% claimed

`pairing.cpp:15-37` (`multiply_by_lin_func_value`)

Line value has shape `[(A,0,0),(B,C,0)]`. Karatsuba-structured dense×sparse:
`c0 = f0·v0 + γ(f1·v1)`, `c1 = (f0+f1)(v0+v1) − f0·v0 − f1·v1`, inner sparse Fq6 products done
Karatsuba-style → 13 Fq2 muls + 3 ξ-muls. Matches the published BN dense×sparse count
(Aranha / Grewal et al.). Runs ~87×/pair.

**Not implemented or measured.** Most intricate item on the list — the six row formulas encode the
exact Fq12 layout, so it needs an implementation plus a differential test against the current
function before the number means anything.

### 4. `bn254::dbl` duplicates `ecc::dbl` — CONFIRMED → **MERGED** PR #1635 (`98881638`)

`utils.hpp:196-220` vs `ecc.hpp:387-416` (a=0 branch)

Same point, same Jacobian representative: S≡d=4xy², M≡e=3x², Xp≡x3, Yp≡y3, Zp=2yz. Compile-tested
`ecc::dbl<E2>` against `bn254::dbl` on a real G2 point: identical x, y, z, and still identical
after two successive doublings (no Jacobian-scaling divergence).

The two are **not** textually identical: x and y are the same formula with different temp names,
but z genuinely differs — the local copy reaches 2yz via `(y+z)²−y²−z²`, needing a `z²` that has no
other use there, where `ecc::dbl` takes `2·y·z` directly.

Careful with the cost claim (I got this wrong first time round): **both cost 18 Fq muls**, because
an Fq2 sqr is 2 Fq muls against 4 for an Fq2 mul, so the z coordinate is 4 Fq muls either way.

| | Fq2 sqr | Fq2 mul | Fq muls | Fq2 add/sub |
|---|---|---|---|---|
| `bn254::dbl` | 7 | 1 | 18 | 18 |
| `ecc::dbl` | 5 | 2 | 18 | 14 |

The win is in **additions**, not multiplications: ~59 vs ~47 Fq add/sub per doubling counting the
adds inside `sqr`/`multiply`, and a G2 subgroup check does 63 doublings per pair (57 through the
`n_dbl<k>` chain, 5 explicit in `mul_by_X`, 1 for `_2px`).

**The trick is justified in `lin_func_and_dbl` (:267)**, where `z_squared` is genuinely needed for
`t[0]`/`t[1]` — that copy stays and becomes the only one.

Needs `constexpr` on `ecc::dbl`, since `n_dbl`/`mul_by_X`/`g2_subgroup_check` are constexpr.

Turned out to be a small perf win too, not just a dedup: **−0.30%** measured. Branch
`crypto/dedup-g2-doubling` @ `d4bdf7b6`, 1 insertion / 28 deletions, 1172/1172 unit tests. **Why `add` is not symmetric with `dbl`** (verified by compile test, not assumed): `ecc::add<E2>`
does not compile. Its infinity/doubling guards — `ecc.hpp:194` `p == 0`, `:300`
`assert(r != 0 || h == 0)`, `:301` `if (h == 0 && r == 0)` — all compare a field element against
zero, and **Fq2 has no zero comparison**: `ExtFieldElem` has only the defaulted
`ExtFieldElem == ExtFieldElem`, and `0` cannot convert to it (consteval ctor wants DEGREE==2
coefficients). The a=0 `dbl` branch is entirely branch-free, which is exactly why *it* substituted.

One line would fix the compile error (`operator==(const ExtFieldElem&, zero_t)`), but it should not:
`bn254::add`'s documented contract is that inputs are never infinity, equal, or negations, and
`mul_by_X`'s addchain guarantees it — unifying would import an assert plus three dead branches onto
a path taken ~20×/pair. Unqualified `add(...)` in bn254 resolves to `bn254::add` regardless, since a
non-template beats a template in overload resolution.

### 5. `add()` recomputes `U1 * H_squared` — **MERGED** PR #1636 (`155eb662`)

`utils.hpp:189` — recomputes what :186 bound to `V`. Sibling `lin_func_and_add` (:326) gets it
right with `(V - X3)`.

**Both reviewers predicted GCC would CSE this** (one "confirmed" it by compiling `add()` in
isolation: 572 vs 573 instructions). It does not, in the real build: −0.37%, well clear of the
0.006% noise floor. ~20 `add()` calls per pair via `mul_by_X`/`g2_subgroup_check`.

### 6. Frobenius powers spelled three ways — **PR #1637 open**, −0.142%

`utils.hpp:256` — 4 Fq2 muls where 2 suffice.

Verified numerically: `FROBENIUS_COEFFS[2][i] == FROBENIUS_COEFFS[1][i] * FROBENIUS_COEFFS[0][i]`
for all i, and the conjugation composes because all `FROBENIUS_COEFFS[1]` entries have zero
imaginary part (so `conj()` is a no-op on them); z is `conj(z)` in both forms.

`:255` `endomorphism<1>(e_px)` → `endomorphism<2>(px)` was taken too (verified
`norm(C1[i]) == C2[i]`). I had written it off as "reads worse" — wrong: doing all three powers
directly is what makes the block uniform, and it drops 3 conjugations.

> A reviewer proposed instead *deleting* the point overload's `P==3` branch as unused generality.
> That is backwards: it is unused **because** this call site open-codes it. Using it removes the
> dead generality and saves the muls.

### 7. `msm` inverts for `add_affine` even when a scalar is zero — CONFIRMED

`ecc.hpp:484`

`h` is reachable only via `points[3]`, i.e. `idx == 3`, i.e. both scalars have a set bit. Verified
with the real GLV constants that `ecc::decompose<Curve>` returns `k2 == 0` for 1, 2, 3, 1000,
2¹⁰⁰, 2¹²⁶ (nonzero from 2¹²⁷ up). So **every ECMUL with a scalar below 2¹²⁷ pays a wasted
binary-GCD inversion** — for a tiny scalar that is comparable to the whole useful MSM loop.
Also affects ecrecover / p256verify paths.

Guard `if (u != 0 && v != 0)` measured −0.14% on the ecmul/ecrecover bench (i.e. free) and passes
all tests. **The current bench cannot show the real win**: all ten `ecmul` inputs in
`precompiles_bench.cpp` use full-width ~250-bit scalars. Quantifying it needs a small-scalar input.

### 8. Each pair pays its own 64 Fq12 squarings — CONFIRMED (= open PR #1544)

`pairing.cpp:47-90`, `137-152`

Shared accumulator across pairs needs the squaring 64× total, not 64× per pair (~4,608 Fq muls
per extra pair). Algebra is sound (Fq12 commutative, all pairs share the NAF schedule).

**Verified on cycles, not just instructions**: instructions −13.4%, cycles −13.7%, IPC unchanged
at 1.80, wall time −13% in the rounds measured before the box got loaded. Work is removed rather
than restructured, which is why all metrics agree — see the Karatsuba warning at the top.
The PR's original "within noise (σ≈7%)" claim was wall-clock at load ~76.

**Follow-up (TODO in the code)**: eliminate the `std::vector`. The pair count is capped at
**492** by the transaction gas limit — `MAX_TX_GAS_LIMIT` = 2²⁴ (EIP-7825) against
`45000 + 34000*k`, so 492 costs 16,773,000 and 493 would cost 16,807,000. A flat buffer of 492
`MillerPairState` is 185 KB (384 B each after dropping the cached negations), likely too much to
place on the stack, so the cheaper shapes are: store only `T` per pair (192 B → 92 KB) and read
`Q`/`P` from the input, or keep a small inline buffer sized for real inputs (mainnet carries 2–4
pairs) with a heap tail. Note the
attainable maximum is slightly lower than 492 (~490) once the 21,000 intrinsic and the memory or
calldata cost of 94 KB of input are counted; 492 is the formula ceiling and the safe bound.

Scaling (Fable reviewer, standalone harness, N copies of one valid pair, cycles tracking
instructions throughout): N=1 −0.2%, N=2 −9%, N=4 −17%, N=8 −24%, N=64 −28%, N=880 (the ~30M gas
ceiling) −28%. Monotone, no input size where it loses. Two side effects found there and worth
keeping in mind: a single pair **breaks even** rather than regressing, because the old code paid a
full Fq12 multiply by one to combine the lone per-pair result (~72 Fq muls, far more than the
malloc); and deferring all Miller work until every pair is validated cuts an invalid-input call by
~86% instructions (4 valid pairs + 1 invalid G1), a DoS-profile improvement.

### 9. Miller iteration 0 squares and multiplies f == 1 — CONFIRMED (size unmeasured)

`pairing.cpp:56-60` — `square(one)` (~72 Fq muls) + full sparse mul against one (~84) to produce
the line value, whose only real cost (`t0y`, `t1x` = 4 Fq muls) is already inside the sparse mul.
Peel to `f = [(t0y,0,0),(t1x,t2,0)]`, ~150 Fq muls/pair. The NAF low digit is 1 so an add step also
runs at i=0, but it acts on the already non-trivial f.

### 10. `endomorphism<2>` uses full Fq2 muls for real constants — CONFIRMED

`utils.hpp:79-87`, `106-113`, `137-153`. All `FROBENIUS_COEFFS[1][*]` (:26-30) are `{c, 0}`, and
`(a₀+a₁u)(c+0u) = (a₀c, a₁c)` — 2 Fq muls via the existing `operator*(ExtFieldElem, Base)` instead
of 4+2A. ~50 Fq muls/check. Small, zero-risk, states the sparsity that is currently implicit.

### 11. First accumulator multiply is by `Fq12::one()` — CONFIRMED

`pairing.cpp:135,151` — one wasted full Fq12 mul (~96 Fq muls) per call; `operator*` only
special-cases squaring. Initialise from the first Miller result. **Subsumed by #8** if that lands.

### 12. `validate()` computes the curve equation before the free zero test — CONFIRMED

`bn254.cpp:16` — `on_curve || pt == 0` spends 3 Fq muls before consulting an 8-word compare.
Infinity is an ordinary ECPAIRING/ECADD input (padding pairs, identity operands). Exactly
equivalent reordered, because (0,0) is not on the curve (0 ≠ 3 == `Curve::B`). The compiler cannot
do this itself — both operands of `||` are evaluated for infinity inputs.

### 13. Dead declarations — CONFIRMED → **LANDED** `a1a87ff2`

`ecc.hpp:132-140` `ecc::Point` (even carried the *same* doc comment as `AffinePoint`),
`ecc.hpp:220-221` `InvFn`, `field_template.hpp:53` `ExtFieldElem::zero()`. Verified unused
repo-wide including tests and tools.

### 14. `lin_func` duplicates `lin_func_and_add`'s line coefficients — CONFIRMED

`utils.hpp:339-358` vs `:329-331`. Algebraically identical: `(x0-U2)*z0_cubed` == `-H*z0_cubed`
since H = U2-x0; `t[1]`, `t[2]` match term for term. `lin_func` is `lin_func_and_add` minus the
point update → shared helper taking `(P0, P1, z0_squared, z0_cubed, U2, S2)`, zero arithmetic cost.

Its doc comment (`:336-338`) is also **actually wrong**, not merely redundant: claims it "Computes
points P0 and P1 addition" and refers to a `P2` parameter that exists in neither function. Fix as
part of this change, not as a standalone comment PR.

### 15. `mul_by_X` and `cyclotomic_pow_to_X` repeat one addchain — CONFIRMED, judgment call

`utils.hpp:208-242` vs `440-474`. Same 30-step seed-X addchain, identical temp names and
`n_dbl<k>`/`n_cyclotomic_square<k>` schedule, once over points and once over Fq12. A helper
parameterised by square/multiply collapses both, plus the parallel `n_dbl`/`n_cyclotomic_square`.
**Trades 60 duplicated lines for a genericity layer** — against this project's simplest-impl
preference, so reasonable to decline.

### 16. `final_exp` runs even when the accumulator is one — CONFIRMED

`pairing.cpp:155`. Input where every pair has p==0 or q==0 (validation still runs, no Miller loop)
pays an Fq12 inversion + three `cyclotomic_pow_to_X` chains to compare one with one.
`if (f == Fq12::one()) return true;` is exact for all inputs since `final_exp(1) == 1`, and can
never short-circuit to false, so the non-injectivity of the final exponentiation is not bypassed.
Ranked last: narrow input class, and it adds a branch.

---

## Discarded — real but below the bar

| Item | Why |
|---|---|
| `multiply_by_lin_func_value` takes `std::array<Fq2,3>` **by value** (`pairing.cpp:16`) | Confirmed out-of-line with array by value in the mangled name — but **measured −0.05%**. Consistency fix only (every sibling takes it by reference). |
| Negated ternaries `!k2.sign ? pt.y : -pt.y` (`bn254.cpp:31-32`) | Real, cosmetic. Invert to `k2.sign ? -pt.y : pt.y`. |
| `_3_ksi_inv` as two opaque hex literals (`fields.hpp:25`) | Derivable as `Fq2{3,0} * inverse(ksi)` with `inverse(Fq2)` made constexpr. Declined: adds constexpr machinery to replace a working constant. `ksi` could also use the `Fq2{9,1}` literal form the ctor advertises. |
| `n_dbl<N>`/`n_cyclotomic_square<N>` loop asymmetry; missing `noexcept` on two `constexpr bool` helpers | Style noise. |

## Rejected — not findings

| Claim | Verdict |
|---|---|
| Restrict point `endomorphism` to `requires(P == 1)` | **Backwards** — see #6. |
| Missing `sqr(Fq6)`; `square(Fq12)` not named `sqr` so `f*f` won't dispatch | Only helps the two Fq6 squarings in the one-shot `inverse(Fq12)`: ~0.07%. No live `f*f` site on Fq12. |
| Karabina compressed squaring in `cyclotomic_pow_to_X` | Squaring runs are only 6–10 long; each re-entry needs a decompression containing an Fq inversion, exceeding the (34→~12 Fq M)×run saving. Granger–Scott is correct here. |
| `&e1 == &e2` address trick dispatching `sqr` (`field_template.hpp:84`) | Odd-looking but sound: `always_inline`, and all real `x * x` sites pass the same lvalue. |
| Redundant re-validation; per-call recomputed constants; coordinate churn; missing `reserve` | Checked, absent. Constants already `constexpr`; one `to_affine` per ECMUL; `pairs.reserve` present. |
| `square(Fq12)` structure, Fq6 6-mul Karatsuba, `sqr(Fq2)`, Jacobian formulas, X addchains | Already at standard optimal operation counts. |

---

## Landing order

1. ~~Dead declarations (#13)~~ — **MERGED** PR #1633 (`ce6ef36f`)
2. ~~`bn254::dbl` → `ecc::dbl` (#4)~~ — **MERGED** PR #1635 (`98881638`), −0.36%
3. `endomorphism<3>` (#6) — simplification + 2 Fq2 muls
4. `add()` reuse `V` (#5) — one line, −0.37%
5. `lin_func` dedup + wrong comment (#14)
6. `validate()` zero-check first (#12)
7. `mul_by_ksi` (#1) — **−10.5%**, own commit
8. Fq2 Karatsuba (#2) — **−5.6%**, own commit
9. Re-measure PR #1544 under the instruction harness (#8)
10. Sparse line mul 13-mul restructure (#3) — needs implementation + differential test

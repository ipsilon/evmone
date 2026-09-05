# Modular arithmetic backends — design

Target state for `evmmax`. The current PR (#1649, branch `crypto/field-arith-specialization`)
is the first step: one specialization wired in by hand. This document is where it is going.

## Two abstractions

1. **A backend family** — one class per reduction strategy, unified by a concept.
2. **`ecc::FieldElement`** — the element type: operators, serialization, nothing about
   representation.

The backend family is *one* abstraction with several implementations, not a layer per
implementation. `FieldElement` delegates and contains no `if constexpr`:

```cpp
template <FieldSpec Spec>
class FieldElement
{
    using Arith = ArithFor<Spec::ORDER>;
    uint_type value_;
public:
    constexpr explicit FieldElement(uint_type v) : value_{Arith::to_internal(v)} {}
    constexpr uint_type value() const noexcept { return Arith::from_internal(value_); }
    // operators delegate to Arith::mul / sqr / add / sub / inv
};
```

This is simpler than the interim shape on the PR, which puts the dispatch *inside*
`FieldElement`. That works for two strategies and degrades at four: 7 operations × N
strategies, with the arms silently correlated. "Plain representation" simultaneously implies
`to_internal` is the identity, `inv` seeds u=1, and `add` cannot skip its carry — three facts
that must agree, spread across three functions, checked by nothing. One class per strategy
makes each internally coherent and obviously complete.

## The contract

The concept carries the operation list; its doc comment carries the invariants, which no
concept can express.

```cpp
/// The modular arithmetic backend of a prime field.
///
/// An implementation keeps values in an internal representation of its choice. Every operation
/// takes and returns fully reduced values, i.e. less than the modulus. to_internal() also
/// requires its argument reduced, because a plain representation cannot reduce it.
/// inv() returns 0 for non-invertible input, including 0.
template <typename T>
concept FieldArith = requires(const typename T::uint_type& x, const typename T::uint_type& y) {
    { T::to_internal(x) } -> std::same_as<typename T::uint_type>;
    { T::from_internal(x) } -> std::same_as<typename T::uint_type>;
    { T::mul(x, y) } -> std::same_as<typename T::uint_type>;
    { T::sqr(x) } -> std::same_as<typename T::uint_type>;
    { T::add(x, y) } -> std::same_as<typename T::uint_type>;
    { T::sub(x, y) } -> std::same_as<typename T::uint_type>;
    { T::inv(x) } -> std::same_as<typename T::uint_type>;
};
```

`sqr` is **mandatory**, with `mul(x, x)` as the trivial fallback, so that call sites can be
unconditional. See "Squaring" below.

The invariants are enforced by extending the existing typed suite in `evmmax_test.cpp`, which
already runs add/sub/mul/inv/to_from_mont against `udivrem` references over five moduli. One
entry per backend gives the syntax check (won't compile) and the semantic check (won't pass).
The concept alone only proves `mul` compiles.

## The strategies

Three classes. Not four — see the refinements note.

| class | representation | selected by | needs from the modulus |
|---|---|---|---|
| `MontArith<Mod>` | Montgomery | fallback, any odd modulus | mod, mod_inv, R² — all derived |
| `PMArith<Mod>` | plain | 2ⁿ−mod fits one word | c = 2ⁿ−mod |
| `SolinasArith<Mod>` | plain | identity: a known prime | bespoke per-prime code |

Applied to every modulus in the tree:

| modulus | top limb all-ones | mod_inv | strategy |
|---|---|---|---|
| secp256k1 p | yes | `0xd838…` | `PMArith`, c = `0x1000003d1` |
| secp256k1 n | yes | `0x4b0d…` | `MontArith` (plain, no refinement) |
| bn254 q | no | `0x87d2…` | `MontArith` sparse |
| bn254 n | no | `0xc2e1…` | `MontArith` sparse |
| secp256r1 n | no | `0xccd1…` | `MontArith` sparse |
| secp256r1 p | no | `0x1` | `MontArith` sparse, or `SolinasArith` |

`MontArith` is chosen by property, `PMArith` by property, `SolinasArith` by **identity**. Since
an identity match necessarily also matches some property, the classifier resolves
**most specific first**: identity, then pseudo-Mersenne, then Montgomery. (The disjointness
proof below covers only `PMArith` vs sparse; it does not extend to identity-based backends,
which overlap by construction.)

BLS12-381 contributes nothing: `bls.cpp` delegates to blst throughout, and
`BLS_FIELD_MODULUS` exists only for input range validation. The uint384 modulus in
`evmmax_test.cpp` merely exercises genericity.

### One refinement, not two, and not peer classes

**sparse** — the most significant limb is not all-ones, so the extra carry word is provably
zero. blst's wording: *"'Sparse' … refers to most significant limb of the modulus. Though
'sparse' is a bit of misnomer, because limitation is just not-all-ones."* This resolves the
three existing `// TODO: Carry is 0 for sparse modulus` comments, and it is the **only**
refinement worth writing, because it is the only one the compiler cannot already do: proving
the extra carry zero needs value-range reasoning across the loop, not constant folding.

A **unit-mod-inv** refinement (mod ≡ −1 mod 2⁶⁴ makes `mod_inv == 1`, collapsing
`m = t[0] * mod_inv`) would be dead code. Because `mod_inv_` is a constexpr member of a
constexpr `ModArith`, gcc already folds it: secp256r1 p emits **zero** `imulq` against bn254's
four. See "Room for modulus-specific procedures".

Refinements stay `if constexpr` inside `MontArith` rather than becoming peer classes: they are
independent of each other, so peers would form a product of names (and secp256r1 p would need
the both-set corner). Over a shared loop:

```cpp
template <bool Sparse, bool UnitModInv, typename UintT>
constexpr UintT mul_mont_cios(
    const UintT& x, const UintT& y, const UintT& mod, uint64_t mod_inv) noexcept;
```

`MontArith<Mod>` derives both flags; `ModArith` (runtime modulus) instantiates
`<false, false>`. One CIOS body, no duplicated loop.

**`PMArith` and `MontArith`-sparse are provably disjoint**, so the classifier needs no
priority reasoning: a modulus 2ⁿ−c with single-word c has limbs `[2⁶⁴−c, ~0, ~0, ~0]`, so its
top limb is always all-ones, which is exactly what sparse forbids.

## Compile-time vs runtime constants

Only `SolinasArith` genuinely requires a compile-time modulus. For the others, compile-time
knowledge buys **selection**, not speed:

- `PMArith`: c as a runtime word is free. Measured: the same reduction with mod and c passed as
  runtime arguments is **255 instructions vs 257**, identical 21 multiplies. The constant has to
  be materialized into a register either way, and the multiplies by c are not strength-reduced.
- `MontArith`: all constants derive from mod at runtime — that is `ModArith` today. The
  *refinements* need compile time, not for a value but so `if constexpr` can **structurally**
  delete the carry tracking. A runtime `if` cannot remove a variable from a loop.
- `SolinasArith`: the reduction is a hand-derived pattern of word shuffles specific to one
  prime. Not parameterizable by a runtime value at all.

`intx::uint` is not a structural type (`words_` is private), so the modulus reaches a template
either through a Spec struct (current) or as a `const auto&` reference NTTP. Both work and
produce readable symbols; the Spec stays for now.

## Room for modulus-specific procedures

Yes, mechanically: a procedure hand-written for one specific prime is just another type
satisfying `FieldArith`, selected by identity. `SolinasArith` already is one. That is what the
concept plus classifier buys — no new machinery, only the priority rule above.

But there is **little room for hand-written per-modulus C++**, because the modulus is already a
constexpr member and gcc specializes CIOS to it without help:

| modulus | `mulq` | `imulq` | total | exploited |
|---|---|---|---|---|
| bn254 q | 32 | 4 | 36 | nothing — dense words |
| secp256k1 p | 24 | 4 | 28 | 3 all-ones limbs → shift/subtract |
| secp256r1 p | 28 | **0** | 28 | a zero limb, and `mod_inv == 1` |

The algorithmic count is 36 (9 per round × 4). secp256k1 loses 8 multiplies automatically
because `m⋅(2⁶⁴−1)` becomes a shift and a subtract; secp256r1 loses 4 to a zero limb and its
four `imulq` entirely to `mod_inv == 1`. Anything expressible as constant folding or strength
reduction over the modulus words is therefore already done.

What is left for identity-specific code:

1. **Reductions the compiler cannot derive** — Solinas is a different algorithm, not a strength
   reduction, so no amount of constant propagation reaches it.
2. **Asm-level register allocation and carry chains** — real (a 3-4× gap), but generic to the
   algorithm, not per-modulus.
3. **A different limb layout** — see below.

### Boundary: redundant limb layouts are out of scope, deliberately

libsecp256k1's 5×52 representation — 52-bit limbs in 64-bit words so partial sums accumulate
with no carry propagation — does not fit this contract, on two counts:

- every operation is typed on `T::uint_type`, the canonical width; 5×52 needs 5 words;
- "every operation returns a fully reduced value" is incompatible with redundant limbs, whose
  entire benefit is non-unique, lazily-carried values.

**Decision: keep the contract as it is.** Do not add a `repr_type` alias "just in case". The
alias is the trivial half of the change; the invariant is the hard half, and a typedef cannot
pre-pay it. Relaxing "always reduced" to normalize-at-boundary means `operator==`, `value()`,
`operator bool` and every `== 0` comparison in `ecc.hpp` normalize first — the same work
whenever it happens, and it is not reduced by preparing the signatures now. With three backends
the eventual signature change is contained.

So deferring is free, and the contract stays simple. If it is ever taken up, note that
libsecp256k1 tracks a `magnitude` invariant in debug builds for exactly this reason.

## Implementation form: header vs asm kernel

Backends differ in *how* they are implemented, and the concept hides it. **Not every backend
wants asm.**

- **`PMArith` stays generic C++ in the header.** The reduction is short — a fold pass, a
  two-word fold, two never-taken branches — and its value depends on inlining into the EC
  formulas. It also pays no canonicalization tax: `mul_pm` emits **0 cmov** against
  `mul_mont`'s 4, because its final subtraction is a `[[unlikely]]` branch taken at ~2⁻²²³
  while Montgomery's is a coin flip that gcc makes branchless. There is nothing here for an
  asm kernel to win, and a call would cost the inlining.
- **`MontArith` gets an asm kernel later.** This is where the C++ is far off: ~300 instructions
  and ~118 cycles latency / ~79 throughput per 256-bit Montgomery mul, against ~40-50
  instructions and ~20-25 cycles for blst-class asm — a 3-4× gap. Kernels live in a `.cpp`;
  nothing is lost by that, since a `target(...)` function cannot be inlined into a
  non-attributed caller anyway.

### Runtime ISA dispatch

ADX is in **no** x86-64 architecture level — not v2, v3, or v4; it is Broadwell-era and
orthogonal to the level scheme. The default `EVMONE_X86_64_ARCH_LEVEL` is 2. So any
unconditional call to a `target("bmi2,adx")` routine on x86-64 executes illegal instructions on
pre-2014 CPUs and on Atom-derived parts still shipping. **This is already a live defect on
`crypto/modexp_asm`**, whose `mul_amm_256` is guarded only by `#ifdef __x86_64__`. Dispatch is a
prerequisite there, not a future nicety.

The tree already has the pattern twice — `keccak.c` and `sha256.cpp` both use
`__attribute__((constructor))` plus a function pointer, with `target(...)` on the
implementations. Two gotchas documented in `keccak.c` are worth inheriting: `__builtin_cpu_init()`
must be called explicitly (LLVM macOS bug 48459), and the Intel E5-2697 v2 errata reports BMI2
without BMI, so check both.

Placement of the check differs by consumer, because the cost does:

- **MODEXP**: pointer resolved once by the constructor, indirect call per multiply. Amortized
  over a variable-width operation; fine.
- **Field path**: an indirect call per 4-limb mul is unaffordable. Hoist the check to the
  precompile entry point and monomorphize below it — `MontArithAsm<Mod>` is simply another type
  satisfying `FieldArith`, chosen by a runtime ISA check at the top of `ecrecover` /
  `pairing_check` rather than by the modulus. One branch per precompile call, none in the inner
  loop.

So the concept is the seam for two independent axes: modulus structure *and* ISA.

## Sharing with MODEXP

Share the **kernels**, not the dispatch. MODEXP cannot use any modulus specialization — its
modulus is user calldata, so never sparse, never pseudo-Mersenne, never Solinas, never
unit-mod-inv. The only overlap is the general Montgomery kernel, and the fixed-width 256-bit
case is exactly `mul_amm_256`.

Two mismatches to reconcile:

- **AMM vs canonical.** MODEXP wants Almost Montgomery — the conditional subtraction fires only
  on the extra carry word, output in [0, 2ⁿ), normalized once at the end. Fields need canonical
  output per operation, because `FieldElement::operator==` is defaulted and memberwise and the
  EC formulas compare constantly (`dx == 0`, `h == 0 && r == 0`, `p.z == 0`). So: one CIOS core,
  two tails. AMM has nothing to offer `PMArith`, whose tail is already free.
- **Width.** MODEXP is span-based and variable-limb; fields are fixed-width `intx::uint<N>`.
  Only the fixed-width kernels are shareable.

AMM remains usable *inside* a bounded field routine — `field_sqrt` could run 253 squarings in
AMM form and normalize once — but that is a local optimization, not the type's invariant, and
sizing puts it in low single digits.

## Squaring

Mandatory in the concept, because that makes the call sites unconditional. Put the aliasing
check in `FieldElement::operator*` — the pattern `ExtFieldElem::operator*` already uses, minus
the `requires` probe, since the concept guarantees `sqr`:

```cpp
if (&a == &b)
    return wrap(Arith::sqr(a.value_));
return wrap(Arith::mul(a.value_, b.value_));
```

Operands in these formulas are named locals, so `&a == &b` constant-folds at every site. That
picks up, with no formula edited:

| site | squarings / multiplies |
|---|---|
| `secp256k1::field_sqrt` | **253 / 266** |
| `ecc::dbl` (a=0) | 5 / 7 |
| `ecc::dbl` (a=p−3) | 5 / 8 |
| `ecc::add` (add-1998-cmo-2) | 4 / 16 |
| `ProjPoint::operator==` | 2 / 8 |
| `bn254::inverse(Fq2)` | 2 / 2 |

`field_sqrt`'s addchain comment already says *"253 squares 13 multiplies"* and then writes
`t0 = t0 * t0`. Note `Fq2::sqr` deliberately avoids base-field squarings, so the pairing's Fq2
layer does not benefit.

Cost model: 10 partial products instead of 16, plus doubling the cross terms.
`PMArith` 15 vs 21 (−29%); `MontArith` ~30 vs 36 (−17%), since only the product half improves.

## File layout

One header, `include/evmmax/evmmax.hpp`, for everything except asm kernels. Not a matter of
tidiness:

- backends are templates on the modulus and **must** inline into the EC formulas;
- the binary GCD inversion (~50 lines) is shared by all backends, seeded with R² for Montgomery
  and 1 for plain representations — already `inv_scaled`;
- `add`/`sub` are representation-independent;
- the classifier belongs beside what it classifies.

`.cpp` only for asm kernels and their constructor-based resolver.

Free functions stay free underneath the classes — `mul_mont_cios<Sparse, UnitModInv>`,
`pseudo_mersenne_reduce<Mod>`. Classes are the interface and the unit of coherence; the
functions are the shared, independently testable internals. `pseudo_mersenne_reduce` **must**
remain reachable directly: that is how the unit test drives the two carry foldings no product of
canonical operands can reach (~2⁻¹⁹⁰ and ~2⁻²²³), and that coverage is mutation-tested. Buried
as a private member, it is lost.

## Measurement discipline

Instruction counts have twice predicted the wrong sign in this codebase:

- mul-free `mul_by_ksi`: −9.3% instructions, **+5.2% cycles** (IPC 1.78 → 1.54)
- Fq2 Karatsuba: −5.7% instructions, **+18% cycles**, +21% wall time
- pseudo-Mersenne on ecrecover: −33.4% instructions, **cycles unchanged**

The cause is the same each time: fewer operations, longer critical path. ecrecover measures
IPC 1.51 and ecpairing 1.80 — both latency-leaning, not the throughput regime where the same
pseudo-Mersenne mul was −48.5% cycles. Anything that restructures a dependency graph — dedicated
squaring included — is gated on a cycles measurement, not an instruction count.

## Sequencing

1. **#1649** — four commits: `inv_scaled`, the reduction primitive, the concept plus `MontArith`
   plus the classifier (no functional change), then `PMArith` wired in.

   Scope deliberately stops at two backends. An interface is validated by implementations that
   differ along its axes, and `PMArith` vs `MontArith` differ on **representation** — the axis
   that produced every constraint found so far (`inv_scaled`, the `to_internal` precondition, the
   disjointness question). secp256r1 comes along free as another `MontArith` user; it adds a
   third *user* but not a third *shape*. With only two backends the classifier needs no
   most-specific-first machinery, so none is written.
2. Rename `ModArith` into the family as the runtime-modulus Montgomery backend, collapsing the
   transitional duplication with `MontArith`.
3. `sqr` in the concept with the trivial fallback, plus the aliasing check in `operator*`.
   Deliberately not in #1649: with no real implementation it is dead code. Real implementations
   behind measurements.
4. The sparse refinement inside `MontArith` (unit-mod-inv is already free). This is the change
   that requires extracting `mul_mont_cios` from `ModArith`, touching the multiply every curve
   depends on — kept out of #1649 on risk grounds.
5. Asm kernels with constructor-based cpuid dispatch — which also fixes the live defect on
   `crypto/modexp_asm`. Highest expected value of anything here, given the 3-4× gap. Exercises
   the implementation-form axis, which #1649 designs but does not test.
6. `SolinasArith` for secp256r1 p, if it beats sparse. Exercises the identity-selection axis, and
   is when the classifier's priority rule gets written.

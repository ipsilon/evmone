# modexp follow-up work

Everything deferred out of the fixed-window exponentiation change
(PR [#1618](https://github.com/ipsilon/evmone/pull/1618), reference branch `crypto/modexp-windowing`).

All figures were measured on an AMD Ryzen 9 5950X, gcc 15.2, Release, using interleaved
runs (build one binary per variant, run them round-robin, take the minimum per case).
Read [Measurement notes](#measurement-notes) before trusting or re-running any of them.

---

## 1. Sliding window instead of all-powers fixed window — DONE (PR #1631)

**This is the one to do next.** Not primarily for the speed — for matching what every
reference implementation does.

The current code is an all-powers ("m-ary") fixed window: it precomputes *every* power
`b^1 .. b^(2^w - 1)`. That layout's natural home is **constant-time** modexp, where
sliding windows and odd-only tables are unusable because their access patterns leak. It is
what OpenSSL's `BN_mod_exp_mont_consttime` does, "to limit data-dependency to a minimum to
protect secret exponents".

EVM modexp exponents are public calldata, and this code is not constant-time anyway — the
`if (v != 0)` multiply skip and the width-from-`exp_bits` choice are both data-dependent.
So we pay for the all-powers table without buying the property it exists to provide. For
public exponents the references converge on sliding window with an **odd-powers** table:

- GMP `mpn_powm` — *"Compute table U^1, U^3, U^5... of E-dependent size"*, *"Precompute odd powers of b"*
- OpenSSL `BN_mod_exp_mont` (the variable-time sibling) — sliding window

**Thresholds are free.** GMP's `win_size()` uses `{7, 25, 81, 241, 673, 1793, 4609, 11521, 28161}`.
Those are exactly the analytic sliding-window break-evens `2^(w-1) / (1/(w+1) - 1/(w+2))`
= 6, 24, 80, 240, 672, 1792, 4608, 11520, 28160 — GMP tests `>=` where the derivation uses `>`.
All nine match. Use GMP's numbers; do not re-tune by hand.

| | fixed (now) | sliding |
|---|--:|--:|
| table entries at width `w` | `2^w - 1` | `2^(w-1)` |
| multiplies, random exponent | `~L/w · (1-2^-w)` | `~L/(w+1)` |
| multiplies, dense exponent | `~L/w` | `~L/w` (identical) |

Measured **3–7%** faster at comparable memory. Half the table for a given width also buys
one extra width at the same footprint, which is where most of the gain comes from.

Cost: `modexp_odd` goes from 79 to ~92 code-only lines (+16%) — noticeably less than the
usual assumption. Sliding *removes* the fixed version's grid-alignment arithmetic
(`top_width = (exp_bits-1) % w + 1` and the "decrements to exactly 0" invariant) and its
zero-window check; it adds `b^2`, a trailing-zero trim loop, and a data-dependent squaring
run. Roughly a wash on complexity.

A working implementation and its diff against the current code are in the session
scratchpad as `modexp-sliding2.cpp` / `sliding-vs-fixed.diff` (passes the 7696-case
differential test, clang-format clean). Treat it as a sketch, not a submission.

**Note this subsumes item 3** — sliding window aligns from the top set bit by construction.

---

## 1b. Window width vs real mainnet traffic — measured, no change warranted

Tuned against the 16 real inputs in `precompiles_bench` rather than synthetic ones.
**Conclusion: the shipped heuristic is already right for mainnet. Leave it alone.**

All 16 inputs are 32/32/32. Fourteen are 252–256-bit exponents; seven distinct values, every
one a curve constant (`bn254 Fp-2`, `secp256k1 n-2`, `P-256 p-2`, `secp256k1 (p+1)/4`,
`bn254 Fr-2`) — i.e. Fermat inversion and modular square root for SNARK verification. This
matches the published usage split: *99.99% of modexp usage came from 256-bit moduli for SNARK
verification*, with RSA the rare remainder.

Forced-width sweep over the whole set (hardware counters, instructions per input):

| w | 1 | 2 | 3 | **4** | 5 | 6 |
|---|--:|--:|--:|--:|--:|--:|
| instructions | 151,460 | 133,673 | 126,232 | **123,779** | 126,923 | 137,134 |
| vs best | 1.224x | 1.080x | 1.020x | **1.000x** | 1.025x | 1.108x |

Per input, w=4 is optimal for **every** one of the fourteen large exponents, across densities
from 43% to 97% — so the choice is robust to exponent shape at that size. Weighted over all
16 inputs the heuristic lands **0.07% above per-input optimal**.

The only mis-picks are the two tiny exponents, and they pull in opposite directions, which is
exactly what a bit-length-only rule cannot see:

| exponent | bits | density | best w | heuristic | loss |
|---|--:|--:|--:|--:|--:|
| `2^24` | 25 | 4% | 1 | 2 | 8.3% |
| `2^24 - 1` | 24 | 100% | 3 | 2 | 2.9% |
| RSA `e = 65537` (not in set) | 17 | 12% | 1 | 2 | 11.8% |
| RSA `e = 3` (not in set) | 2 | 100% | 1 | 1 | 0% |

**The one case worth considering** is RSA with `e = 65537`, an explicit target use case of
EIP-2565. It is 17 `mul_amm` calls optimally vs 19 as shipped, and on a 4096-bit modulus each
of those is ~8k multiply instructions, so the 11.8% is real work even though the operation is
rare. Fixing it needs a density-aware width, since multiplies can never exceed the exponent's
popcount: minimise `L + min(ceil(L/w), popcount) - 1 + 2^w - 2`. Popcount is cheap — the
exponent bytes are already scanned for `bit_width`. This changes nothing for the 99.99%.

**Tension with item 2 below:** worst-case tuning pushes short exponents to *wider* windows,
which is the wrong direction for real sparse ones. The closed form in item 2 gives w=3 for
`e = 65537`, which is 35% worse than the shipped w=2 on that input. The two objectives
genuinely conflict below ~50 bits; they agree everywhere above it.

## 2. Tune the window width for the worst case, not the average

Gas (EIP-2565/7883) is charged on exponent **bit length** and ignores Hamming weight, so
the costliest input at a given charge is the *densest* exponent. Optimising for it:

```
cost(w) = L (squarings) + L/w (multiplies) + 2^w - 2 (table)
cost(w) == cost(w+1)  ->  L = 2^w · w · (w+1)  =  4, 24, 96, 320, 960, 2688, 7168
```

versus the 16, 48, 140 currently shipped (which are the *random*-exponent break-evens).

Those dense crossovers have bit widths 3, 5, 7, 9 — linear in `w` — so the width collapses
to a closed form. (No closed form exists for the random thresholds: the `(1-2^-w)` sparsity
terms distort exactly the small-`w` end that matters.)

```cpp
const auto w = std::min<unsigned>(
    MAX_WINDOW_WIDTH, (static_cast<unsigned>(std::bit_width(exp_bits)) + 1) / 2);
```

Worst-case time per gas over the whole modexp benchmark matrix:

| | worst ns/gas | vs master |
|---|--:|--:|
| master (binary square-and-multiply) | 8.20 | — |
| PR as shipped (16/48/144) | 6.41 | +21.8% headroom |
| **closed form** | **6.02** | **+26.5% headroom** |

The closed form also beats a hand-tuned triple (4, 28, 96), which gave 6.05 — it widens
earlier (bands 4/16/64), and that is the correct direction for dense exponents. It picks
the optimal width at every gas-binding input measured. Per-case wins: −13% to −18% at
`exp_bits` 4..11, −11% at 33. Sparse exponents pay up to 9.5%, which is free under this
objective since they are charged the same gas as dense and therefore have slack.

**The 144 threshold is measurably too high** (instruction counts, mod_len=32, forced widths).
w=4 already beats w=3 from 128 bits upward — below both the shipped 144 and the model's
139.6 — for a dense *and* a random exponent:

| exp_bits | 139 | 140 | 141 | 142 | 143 | 144 |
|---|--:|--:|--:|--:|--:|--:|
| all-ones, w=4 vs w=3 | 0.970x | 0.970x | 0.995x | 0.975x | 0.975x | 0.975x |
| random, w=4 vs w=3 | 0.990x | 0.990x | 1.010x | 0.995x | 0.990x | 0.990x |

So w=4 wins 6/6 sizes on all-ones and 5/6 on random across 139..144, by 0.5-3%. The model
underestimates how early widening pays — real per-operation overhead favours fewer, larger
operations more than an op count suggests. Lowering the threshold to ~128 is worth 1-3% in
that band. No mainnet traffic lives there (the gap runs 26-251 bits), so this only matters
under the worst-case objective.

**Requires regenerating the test vectors.** Widths shift (`exp_bits=16` moves from w=1 to
w=3) and the current 12 vectors would collapse onto w=3/w=4 only, losing the w=1/w=2
coverage that mutation testing justified. New bands: `exp_bits <= 3` → w=1, 4..15 → w=2,
16..63 → w=3, `>= 64` → w=4. Generator and mutation harness are in the scratchpad
(`gen_cases.py`, `mutants.py`).

---

## 3. Tile windows from the top, not the bottom

`top_width = (exp_bits - 1) % w + 1` places the ragged window at the MSB end, so the loop
does `exp_bits - top_width` squarings. A full-width top window with a ragged *final* window
does `exp_bits - w` squarings for the same multiply count, saving `w - top_width`.

Worth **0% whenever `exp_bits % w == 0`** — so nothing at all for 256/512/2048/8192-bit
exponents. Peaks at 4.0% (`exp_bits=17`), 2.9% (49), 2.0% (33, the gas-binding size),
1.6% (145). Costs a special-cased final iteration.

Note the current layout is the *standard* one for a fixed window — blst's `ec_mult.h`
computes the same `window = bits % SZ` ("top excess bits modulo target window size"). So
this is a deviation from common practice, not a correction. **Item 1 makes it moot.**

---

## 4. Batched window bit extraction — DONE (folded into PR #1631)

Landed as `Exponent::window(lo, hi)`: one two-byte access per window instead of one
`operator[]` per bit, with `countr_zero` trimming to odd, which also removed the scan for
the lowest set bit. Measured **-1.7% instructions / -2.1% cycles** weighted over the
mainnet corpus, **+0.06%** (neutral) on the all-ones matrix, whose exponents have no zero
runs for the scan to skip. Branch misses halve but are immaterial: 12.5k against 1.59G
instructions. Costs +5 net code lines (-6 in `modexp_odd`, +10 in `Exponent`).

An earlier attempt measured *slower* (+0.47%) because it kept the per-bit read loop and
only added `countr_zero` on top — the batched load is the whole win. Original estimate
below was 1-3%; it came in at the top of that range.

Rejected in the same pass, both measured: `operator[]` delegating to `window(index, index)`
(-6 lines but +0.43% geomean, +2.25% on the already-regressing case), and skipping zero
runs with `countl_zero` instead of a per-bit test (sub-1%, judged not worth it).

### Original write-up


`window()` reads `w` individual bits via `Exponent::operator[]`. A `w <= 4` bit field spans
at most two adjacent bytes, so it could be one two-byte load, a shift and a mask.

There is exactly **one** `operator[]` call per exponent bit — the same count as the binary
loop this replaced — so windowing added no extraction work per bit; only the lambda's
shift/or accumulation is new. The apparent "division per bit" is division by the constant 8,
which compiles to shifts; the only true `div` is `% w`, once per call.

Estimated at ≤6% of *instructions* in the worst corner (4-word modulus, 8192-bit exponent)
and 1–3% of *cycles* there, since the scalar work largely hides under the `mul_amm` carry
chain. Noise everywhere else. **Do not do this without a measurement showing a win.**

---

## 5. Shrink the modexp stack frame — REJECTED 2026-08-10

**Do not do this.** evmone targets embedded use, where allocation must be minimised; a
large stack buffer that keeps `modexp` off the heap is the point, not a cost. The frame
size is deliberate. Kept below only so the measurement is not re-derived.



`modexp()`'s `stack_buf` went from 11,296 to 25,632 bytes; the whole frame is ~26 KB.
Deliberately accepted, recorded here only so the option isn't lost.

`std::pmr::monotonic_buffer_resource` **falls back to the heap** when its buffer is
exhausted, so `STACK_CAPACITY` is a fast-path size, not a correctness bound. Sizing it for
a 128-byte modulus instead of 1024:

- frame 27,016 → **3,720 bytes**
- identical results (same 7696/7696 differential pass; large moduli exercise the heap path)
- no measurable slowdown (0.975–1.068x across six cases, i.e. inside the noise floor) —
  one `malloc` is nothing against a multi-millisecond big-modulus modexp

---

## 9. Window width ignores the modulus size — NEW, causes the only regression in #1631

`window_width()` takes only `exp_bits`. The table costs `2^(w-1)` mul_amm calls whatever
the modulus is, but each call gets cheaper as the modulus shrinks, so for a small odd part
the table stops being repaid while the bit-length rule still widens the window.

This is the whole of the regression #1631 documents. Instructions, min of 3 rounds:

| case | fixed window | sliding | sliding + window read |
|---|--:|--:|--:|
| `mod_len=32 / mod_tz=254 / exp_bits=256` | 131,652 | 133,135 (+1.13%) | 133,231 (+1.20%) |

The odd part there is 2 bits — one word — and the fixed window used w=4 where sliding
picks w=5. Same shape, smaller, in `m504 tz4000 e255` and `m1024 tz8190 e2048`.

Fix is to bound `w` by the modulus word count as well, e.g. cap the table cost against the
work the loop actually saves. Low churn on tests: every current vector uses a 4- or 5-word
modulus, so their widths would not move; a 1-word-modulus vector would need adding.

## 6. Window the even-modulus path

`modexp_pow2()` still uses plain binary square-and-multiply with truncated `mul`.
Windowing applies there too, and is cheaper than in the odd path (no Montgomery form).

Currently these cases get ~1.00x from the windowing change: `mod_len=1024/mod_tz=8190/exp_bits=2048`
and `mod_len=504/mod_tz=4000/exp_bits=255` are dominated by the pow2 half and are untouched.

---

## 7. The real lever for worst-case gas: fixed per-call cost

The gas-binding input is `mod_len=32 / exp_bits=33` at 512 gas — just above the EIP-7883
gas floor, where the exponentiation loop is short and **fixed per-call work dominates**:
the to-Montgomery conversion (`rem()`, a full division), `modinv_pow2()`, and the CRT
combine.

This is why the whole window-width question only moves total headroom by ~10% (item 2)
while individual sizes move 20–30%. Large exponents (>512 bits) are the *least* constrained
bucket at ~2.5 ns/gas — roughly 2.6x more headroom than the binding case. Anything that
cuts the constant overhead is worth more than further loop tuning.

---

## 8. Comment cleanup outside the windowing code

Flagged during review but deliberately left out of PR #1618 to avoid unrelated churn in an
external contributor's PR. Roughly 30 lines, independent of the windowing work:

- `mul_amm` doc — says "Almost Montgomery Multiplication" and the formula twice; the y=1
  derivation is a one-line proof of a one-line claim
- `modexp()` dispatcher — six comments restating well-named code; extracting
  `const bool mod_is_one = ...` removes two more
- `load_mod` — the overflow justification's gas arithmetic is overwritten
- `store()` — re-explains its own `static_assert`
- `modinv_pow2` — the "avoids the negation helper" justification
- `mul_amm<4>` doc — restates the signature
- `mul` / `rem` / `Exponent` class — one-line restatements

---

## Measurement notes

Things that produced wrong answers during this work. Worth reading before re-measuring.

- **`perf_event_paranoid` is 4 on this box**, so hardware counters are unavailable and
  `--benchmark_perf_counters=INSTRUCTIONS` silently returns 0. Use wall-clock with
  interleaved variants, or count operations analytically.
- **Noise floor is ±10% on the slow benchmarks.** Cases whose window width does *not*
  change between two builds still swung 8–14%. Always include such cases as a control: if
  they move, your signal is smaller than your noise.
- **Single runs put the ns/gas peak in the wrong place entirely.** One cold run reported
  `mod_len=72/exp_bits=4` at 34 ns/gas; across three interleaved rounds it is 5.9, and the
  real peak is `mod_len=32/exp_bits=33`. Never conclude from one run.
- **`precompiles_bench` generates all-ones exponents** (base and exponent filled with
  `0xff`). For a *gas* objective this is correct — gas ignores Hamming weight, so the dense
  exponent is the worst case an attacker can buy. But it is the best case for windowing, so
  it overstates typical throughput: the fixed-window change measures 1.53–1.62x on all-ones
  and 1.18–1.26x with a random exponent. Quote whichever you mean, and say which.
- **Width changes are semantics-preserving**, so no correctness test can validate the
  heuristic — width mutants are equivalent mutants by construction. Only benchmarks can.
  What *is* testable is the leading-partial-window alignment.
- To A/B a refactor that should be free, **diff the disassembly** rather than benchmark it
  (`objdump -d`, normalise addresses). That is how the `window_width()` extraction was
  confirmed at byte-identical codegen, with no benchmark noise to argue about.

---

## Unrelated: CI on external PRs

Three jobs (`clang-tidy`, `clang-latest-sanitizers`, `fuzzing`) cannot run on fork PRs —
they use `executor: linux-clang-selfhosted` (`resource_class: ipsilon/ha9x`), and CircleCI
blocks self-hosted runners for public projects with "Build forked pull requests" enabled.
There is no approval-gated exception; `type: approval` gates *when* a job runs, not the
pipeline's trust level. GitHub Actions has the equivalent feature, CircleCI does not.

Current workaround, which works and doubles as the approval gate: push the PR head to a
branch in `ipsilon/evmone` and let CI run there.

**Untested idea** — trigger a pipeline on the PR head via API v2:

```bash
curl -u "$CIRCLE_TOKEN:" -X POST --header "Content-Type: application/json" \
  -d '{"branch":"pull/1618/head"}' \
  https://circleci.com/api/v2/project/gh/ipsilon/evmone/pipeline
```

Note the `/head` suffix (the auto-assigned branch is `pull/1618`, but the API needs
`pull/1618/head`). **Whether this grants main-project privileges — self-hosted resource
classes included — is undocumented and unverified.** One command to find out.

Either way: whatever raises a fork PR to full privileges also lets that PR's `circle.yml`
run as trusted, so reviewing the config diff matters as much as reviewing the code.

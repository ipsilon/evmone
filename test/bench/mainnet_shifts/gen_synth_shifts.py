#!/usr/bin/env python3
"""Experiment: synthetic shift-amount sequences vs the mainnet window.

Models are fitted on the full mainnet sequence per opcode (amounts <= 255):
  iid - independent draws from the empirical histogram
  m1  - first-order Markov chain (empirical transitions)
  m2  - second-order Markov chain (falls back to m1 for unseen contexts)
Emits a header with <op>_iid, <op>_m1, <op>_m2 arrays of the same length as the
mainnet window, and prints sequence statistics for comparison.
"""
import glob
import math
import os
import random
import sys
from collections import Counter, defaultdict

PARTS, OUT_HPP = sys.argv[1], sys.argv[2]
N = 21819
OPS = {0x1B: "shl", 0x1C: "shr", 0x1D: "sar"}
WINDOW_START = {"shl": 2093520, "shr": 330330, "sar": 85921}

seq = {name: [] for name in OPS.values()}
for p in sorted(glob.glob(os.path.join(PARTS, "*.bin"))):
    d = open(p, "rb").read()
    for i in range(0, len(d) - 64, 65):
        if not any(d[i + 2 : i + 33]):
            seq[OPS[d[i]]].append(d[i + 1])


def H(c):
    n = sum(c.values())
    return -sum(v / n * math.log2(v / n) for v in c.values())


def stats(s):
    h0 = H(Counter(s))
    h1 = H(Counter(zip(s, s[1:]))) - H(Counter(s[:-1]))
    rep = sum(a == b for a, b in zip(s, s[1:])) / (len(s) - 1)
    return f"H0={h0:.2f} H1={h1:.2f} P(repeat)={rep:.2f}"


def sampler(counter):
    vals, w = zip(*counter.items())
    return lambda rng: rng.choices(vals, w)[0]


out = {}
for name, s in seq.items():
    rng = random.Random(0x5EED + len(name))
    hist = Counter(s)
    t1 = defaultdict(Counter)
    t2 = defaultdict(Counter)
    for a, b in zip(s, s[1:]):
        t1[a][b] += 1
    for a, b, c in zip(s, s[1:], s[2:]):
        t2[(a, b)][c] += 1
    draw0 = sampler(hist)
    draw1 = {k: sampler(v) for k, v in t1.items()}
    draw2 = {k: sampler(v) for k, v in t2.items()}

    iid = [draw0(rng) for _ in range(N)]

    m1 = [draw0(rng)]
    while len(m1) < N:
        prev = m1[-1]
        m1.append(draw1[prev](rng) if prev in draw1 else draw0(rng))

    m2 = [draw0(rng)]
    m2.append(draw1[m2[0]](rng) if m2[0] in draw1 else draw0(rng))
    while len(m2) < N:
        ctx = (m2[-2], m2[-1])
        if ctx in draw2:
            m2.append(draw2[ctx](rng))
        elif m2[-1] in draw1:
            m2.append(draw1[m2[-1]](rng))
        else:
            m2.append(draw0(rng))

    w = WINDOW_START[name]
    print(f"{name.upper()}  full    : {stats(s)}")
    print(f"{name.upper()}  window  : {stats(s[w:w + N])}")
    for label, g in (("iid", iid), ("m1", m1), ("m2", m2)):
        print(f"{name.upper()}  {label:8s}: {stats(g)}")
        out[f"{name}_{label}"] = g

with open(OUT_HPP, "w") as f:
    f.write("""// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>

/// Synthetic shift amount sequences generated from models fitted on the whole mainnet trace
/// (see mainnet_shift_amounts.hpp and mainnet_shifts/gen_synth_shifts.py):
/// - iid: independent draws from the shift amount histogram,
/// - m1: first-order Markov chain (next amount depends on the previous one),
/// - m2: second-order Markov chain (next amount depends on the previous two).
/// Each sequence has the same length as the mainnet window.
namespace evmone::test::synth_shift_amounts
{
// clang-format off
""")
    for key, vals in out.items():
        f.write(f"inline constexpr uint8_t {key}[] = {{\n")
        for i in range(0, len(vals), 16):
            f.write("    " + ", ".join(map(str, vals[i:i + 16])) + ",\n")
        f.write("};\n")
    f.write("// clang-format on\n}  // namespace evmone::test::synth_shift_amounts\n")
print("wrote", OUT_HPP)

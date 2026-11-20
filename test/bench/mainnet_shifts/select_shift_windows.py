#!/usr/bin/env python3
"""Pick, per shift opcode, the contiguous window of mainnet shift amounts that
fits the 64 KiB benchmark body and whose amount histogram is closest (minimum
total-variation distance) to the opcode's global histogram. Emits a C++ header.

Input: per-block trace parts <N>.bin with 65-byte records
[opcode:1][shift:32 LE][value:32 LE]. Amounts > 255 are dropped (outliers).
"""
import bisect
import glob
import os
import sys

PARTS, OUT_HPP = sys.argv[1], sys.argv[2]
BODY_BUDGET = 65536 - 77  # must match the code layout in mainnet_shifts.cpp
OPS = {0x1B: "shl", 0x1C: "shr", 0x1D: "sar"}

seq = {op: [] for op in OPS}
blk = {op: [] for op in OPS}  # block number of each element
blocks = []
for path in sorted(glob.glob(os.path.join(PARTS, "*.bin"))):
    n = int(os.path.basename(path)[:-4])
    blocks.append(n)
    data = open(path, "rb").read()
    for i in range(0, len(data) - 64, 65):
        op = data[i]
        if any(data[i + 2 : i + 33]):  # amount > 255
            continue
        seq[op].append(data[i + 1])
        blk[op].append(n)


def tvd(h, g, n):
    return 0.5 * sum(abs(h[a] / n - g[a]) for a in range(256))


chosen = {}
for op, name in OPS.items():
    s_all = seq[op]
    total = len(s_all)
    g = [0] * 256
    for a in s_all:
        g[a] += 1
    g = [c / total for c in g]

    prefix = [0]
    for a in s_all:
        prefix.append(prefix[-1] + 3)  # PUSH1 a + OP (no PUSH0 in Istanbul)

    stride = max(1, total // 4000)
    best = None
    for s in range(0, total, stride):
        e = bisect.bisect_right(prefix, prefix[s] + BODY_BUDGET) - 1
        if e >= total and prefix[total] - prefix[s] < BODY_BUDGET - 3:
            break  # window would run off the end
        h = [0] * 256
        for a in s_all[s:e]:
            h[a] += 1
        d = tvd(h, g, e - s)
        if best is None or d < best[0]:
            best = (d, s, e)
    d, s, e = best
    chosen[name] = (s, e, d, blk[op][s], blk[op][e - 1], total)
    print(f"{name}: total={total:,} window=[{s:,}, {e:,}) len={e - s:,} "
          f"bytes={prefix[e] - prefix[s]} tvd={d:.4f} blocks {blk[op][s]}..{blk[op][e - 1]}")

with open(OUT_HPP, "w") as f:
    f.write("""// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>

/// Shift amounts of SHL, SHR and SAR instructions in the order of execution on Ethereum mainnet.
///
/// Collected by tracing all shift instructions while executing 200 mainnet blocks
/// from the range %d..%d. Shift amounts greater than 255 (rare outliers) are skipped.
/// For each instruction, the contiguous window that fits the 64 KiB benchmark code and has
/// the shift amount distribution closest to the instruction's overall distribution is selected.
namespace evmone::test::mainnet_shift_amounts
{
// clang-format off
""" % (blocks[0], blocks[-1]))
    for op, name in OPS.items():
        s, e, d, b0, b1, total = chosen[name]
        vals = seq[op][s:e]
        f.write(f"\n/// {name.upper()}: {e - s} of {total} amounts, blocks {b0}..{b1}, "
                f"distribution TVD {d:.3f}.\n")
        f.write(f"inline constexpr uint8_t {name}[] = {{\n")
        for i in range(0, len(vals), 16):
            f.write("    " + ", ".join(str(v) for v in vals[i:i + 16]) + ",\n")
        f.write("};\n")
    f.write("// clang-format on\n}  // namespace evmone::test::mainnet_shift_amounts\n")
print("wrote", OUT_HPP)

#!/bin/bash
# Build each Keccak variant with each compiler and run the benchmark.
cd "$(dirname "$0")"
echo "### host: $(uname -sm)"
if [ "$(uname -s)" = Darwin ]; then
  sysctl -n machdep.cpu.brand_string; sysctl hw.optional.arm.FEAT_SHA3 hw.perflevel0.physicalcpu 2>/dev/null || true
  ARCH=
else
  (lscpu | grep -E "Model name|Vendor ID|^CPU\(s\)") || true
  grep -m1 -o -w sha3 /proc/cpuinfo || echo "no sha3 hwcap"
  ARCH=-march=armv8.2-a+sha3
fi
VARIANTS="scalar committed dload iota_post iota_post_eor3 natchi"
for cxx in clang++ g++; do
  command -v $cxx >/dev/null || continue
  $cxx --version 2>&1 | grep -qi "apple clang" && [ $cxx = g++ ] && continue
  echo "### $cxx: $($cxx --version | head -1)"
  for v in $VARIANTS; do
    $cxx -std=c++20 -O3 $ARCH -I. bench.cpp v_$v.cpp -o b_${cxx}_$v || { echo "$v: build failed"; continue; }
  done
  for round in 1 2; do
    for v in $VARIANTS; do
      [ -x b_${cxx}_$v ] || continue
      echo "--- $v (round $round)"; ./b_${cxx}_$v 15
    done
  done
done

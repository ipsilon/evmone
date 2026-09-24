#!/usr/bin/env bash
# Builds evmone-bench-internal and runs the JUMPDEST analysis benchmarks.
# Usage: jumpdest_bench_ci.sh <label> [<results dir>]
# Writes <results dir>/<label>.json (Google Benchmark) and <label>-hw.txt (the machine).
set -eo pipefail

label=$1
out=${2:-results}
build=${BUILD_DIR:-build-bench}
jobs=${JOBS:-2}
mkdir -p "$out"

generator=()
if command -v ninja > /dev/null; then generator=(-G Ninja); fi
cmake -S . -B "$build" "${generator[@]}" -DCMAKE_BUILD_TYPE=Release -DEVMONE_TESTING=ON
cmake --build "$build" --target evmone-bench-internal --parallel "$jobs"

{
    echo "label: $label"
    uname -a
    command -v nproc > /dev/null && echo "nproc: $(nproc)"
    if command -v lscpu > /dev/null; then lscpu; fi
    if [ -f /proc/cpuinfo ]; then
        grep -m1 -E 'model name|CPU part' /proc/cpuinfo || true
        grep -m1 -E '^(flags|Features)' /proc/cpuinfo || true
    fi
    if [ -f /sys/fs/cgroup/cpu.max ]; then echo "cgroup cpu.max: $(cat /sys/fs/cgroup/cpu.max)"; fi
    if [ "$(uname)" = Darwin ]; then
        sysctl machdep.cpu.brand_string hw.ncpu hw.perflevel0.physicalcpu hw.perflevel1.physicalcpu \
            hw.memsize 2> /dev/null || true
    fi
    "${CXX:-c++}" --version || true
    cat /proc/loadavg 2> /dev/null || uptime
} > "$out/$label-hw.txt" 2>&1

"$build/bin/evmone-bench-internal" --benchmark_filter='^jumpdest_analysis/' \
    --benchmark_repetitions=30 --benchmark_enable_random_interleaving=true \
    --benchmark_min_time=0.015s --benchmark_display_aggregates_only=true \
    --benchmark_context=label="$label" \
    --benchmark_out="$out/$label.json" --benchmark_out_format=json > "$out/$label.txt"
tail -n 3 "$out/$label.txt"

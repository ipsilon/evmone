#!/usr/bin/env bash
# Builds evmone-bench-jumpdest-pointer and runs it.
# Usage: jumpdest_pointer_bench_ci.sh <label> [<results dir>]
# Writes to <results dir>: <label>.json and <label>.csv (Google Benchmark, every repetition),
# <label>-hw.txt (the machine) and <label>-disasm.txt (the benchmarked functions).
# The CSV and the disassembly are also printed to the log, gzipped and base64-encoded.
set -eo pipefail

label=$1
out=${2:-results}
build=${BUILD_DIR:-build-bench}
jobs=${JOBS:-2}
mkdir -p "$out"

generator=()
if command -v ninja > /dev/null; then generator=(-G Ninja); fi
cmake -S . -B "$build" "${generator[@]}" -DCMAKE_BUILD_TYPE=Release -DEVMONE_TESTING=ON
cmake --build "$build" --target evmone-bench-jumpdest-pointer --parallel "$jobs"
bin=$(find "$build" -name evmone-bench-jumpdest-pointer -type f | head -n 1)

{
    echo "label: $label"
    uname -a
    command -v nproc > /dev/null && echo "nproc: $(nproc)"
    if command -v lscpu > /dev/null; then lscpu; fi
    if [ -f /proc/cpuinfo ]; then
        grep -m1 -E 'model name|CPU part' /proc/cpuinfo || true
        grep -m1 -E '^(uarch|isa)' /proc/cpuinfo || true
    fi
    if [ "$(uname)" = Darwin ]; then
        sysctl machdep.cpu.brand_string hw.ncpu hw.perflevel0.physicalcpu hw.perflevel1.physicalcpu \
            hw.memsize 2> /dev/null || true
    fi
    "${CXX:-c++}" --version || true
    cat /proc/loadavg 2> /dev/null || uptime
} > "$out/$label-hw.txt" 2>&1
cat "$out/$label-hw.txt"

objdump=objdump
command -v objdump > /dev/null || objdump=llvm-objdump
"$objdump" -d --no-show-raw-insn -C "$bin" |
    awk '/^[0-9a-f]+ <.*jdp_[a-z]+_[0-9]+.*>:$/{p=1} p{print} /^$/{p=0}' > "$out/$label-disasm.txt" || true

"$bin" --benchmark_repetitions="${REPS:-30}" --benchmark_enable_random_interleaving=true \
    --benchmark_min_time=0.012s --benchmark_format=csv --benchmark_context=label="$label" \
    --benchmark_out="$out/$label.json" --benchmark_out_format=json > "$out/$label.csv"

for f in "$label.csv" "$label-disasm.txt"; do
    echo "BEGIN $f.gz.b64"
    gzip -9c "$out/$f" | base64
    echo "END $f.gz.b64"
done

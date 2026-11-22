#!/usr/bin/env bash
# Throwaway: builds the baseline_execution.cpp alignment variants and times them.
# Usage: ci.sh <label> <results dir> [<x86-64 levels, or - for the default>...]
# Writes <label>-v<level>.json per level and <label>-hw.txt; the JSON is also printed to the log, gzipped and
# base64-encoded. Uses $CC/$CXX (default: gcc/g++, else clang/clang++).
set -eo pipefail

here=$(cd "$(dirname "$0")" && pwd)
label=$1
out=$(mkdir -p "$2" && cd "$2" && pwd)
shift 2
levels=("$@")
[ ${#levels[@]} -gt 0 ] || levels=(2 3)

if [ -z "$CXX" ]; then
    if command -v g++ > /dev/null; then export CC=gcc CXX=g++; else export CC=clang CXX=clang++; fi
fi

{
    echo "label: $label"
    uname -a
    echo "nproc: $(nproc)"
    lscpu || true
    "$CXX" --version || true
    cat /proc/loadavg
} > "$out/$label-hw.txt" 2>&1
cat "$out/$label-hw.txt"

for level in "${levels[@]}"; do
    tag=v$level
    [ "$level" != - ] || tag=default
    python3 "$here/variants.py" build "$level" "build-align-$tag" "variants-$tag"
    python3 "$here/variants.py" run "$level" "variants-$tag" "${ROUNDS:-15}" "$out/$label-$tag.json" "$label"
    echo "BEGIN $label-$tag.json.gz.b64"
    gzip -9c "$out/$label-$tag.json" | base64
    echo "END $label-$tag.json.gz.b64"
done

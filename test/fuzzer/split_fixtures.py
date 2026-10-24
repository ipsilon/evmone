#!/usr/bin/env python3
# evmone-fuzzer: LibFuzzer based testing tool for evmone.
# Copyright 2026 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0
"""Splits the state test files under SRC into single-fixture files in DST, the fuzzer's seeds.

Drops what evmone does not read, as the fuzzer's mutator does. Files are named by the hash of
their contents, as libFuzzer names its corpus."""

import hashlib
import json
import pathlib
import sys

src, dst = map(pathlib.Path, sys.argv[1:])
dst.mkdir(parents=True, exist_ok=True)
for path in src.rglob("*.json"):
    for name, test in json.loads(path.read_bytes()).items():
        test.get("_info", {}).pop("hash", None)
        for expectations in test.get("post", {}).values():
            for e in expectations:
                e.pop("state", None)
                e.pop("receipt", None)
        out = json.dumps({name: test}, separators=(",", ":")).encode()
        (dst / hashlib.sha1(out).hexdigest()).write_bytes(out)

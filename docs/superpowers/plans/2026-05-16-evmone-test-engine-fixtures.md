# `evmone test` — Engine-format fixture runner — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a `test` subcommand to the `evmone` CLI that runs a JSON fixture in EEST's `blockchain_test_engine` format against evmone's state machinery.

**Architecture:** Two new sources in `test/utils/` — a JSON loader (no VM) and a runner (no I/O, no gtest) — plus a thin CLI glue. Loader keeps transactions as raw RLP bytes; runner decodes them at execution time using fuzzing/main's RLP decoder + signer recovery, which are cherry-picked first.

**Tech Stack:** C++23, nlohmann_json, gtest/gmock for unit tests, CLI11 for the CLI binary, evmone's existing `state` + `testutils` libraries.

**Branch:** all work lands on `evmone-test` (already created off `master`, contains the design spec at `30bc3ab6`).

**Reference spec:** `docs/superpowers/specs/2026-05-16-evmone-test-engine-fixtures-design.md`.

---

## Task 1: Cherry-pick fuzzing/main RLP-decode + signer-recovery commits

Bring in the 15 commits required for engine-payload transaction processing. They are landed first as their own logical unit so the engine-test code on top is strictly additive.

**Files:**
- Modify (via cherry-pick): `test/utils/rlp.hpp`, `test/utils/rlp.cpp` (new), `test/utils/rlp_decode.hpp` (new), `test/utils/rlp_decode.cpp` (new), `test/utils/CMakeLists.txt`, `test/state/state.hpp`, `test/state/state.cpp`, `test/state/host.cpp`, `test/state/block.hpp`, `test/state/errors.hpp`, `lib/evmone/delegation.hpp`, `lib/evmone/delegation.cpp`

- [ ] **Step 1: Confirm we are on the `evmone-test` branch**

Run:
```
git rev-parse --abbrev-ref HEAD
```
Expected output: `evmone-test`.

- [ ] **Step 2: Cherry-pick all 15 commits in one batch**

Each cherry-pick becomes its own commit. Order is the chronological order from `origin/fuzzing/main`:

```
git cherry-pick \
  43e0162e \
  c433b411 \
  92afadeb \
  6293b0a0 \
  38687cf0 \
  8b7a30b9 \
  78310649 \
  f11aaeab \
  e1c886c6 \
  b9a83f94 \
  c107406e \
  724f2cea \
  43f595c7 \
  fe0b9640 \
  b845a22e
```

(This is the actual chronological order on `origin/fuzzing/main`, verified via `git log --reverse --not master`. `a86ace9d` is skipped — it is a pure TODO comment.)

Expected: 15 new commits on top of `30bc3ab6`. If a cherry-pick conflicts, resolve in-line preserving the original commit message, then `git cherry-pick --continue`.

- [ ] **Step 3: Verify history**

Run:
```
git log --oneline -17
```
Expected: the spec commit (`30bc3ab6 docs: Add design spec ...`) is the 16th-from-top entry, with 15 cherry-pick commits above it, ending at `b845a22e state: don't reject depth==0 tx call when sender.nonce == NonceMax` (the latest fuzzing/main commit).

- [ ] **Step 4: Build with EVMONE_TESTING=ON**

Run:
```
cmake --build build/debug -j
```
Expected: clean build, no errors. `build/debug/bin/evmone-unittests` and `build/debug/bin/evmone-blockchaintest` are rebuilt.

- [ ] **Step 5: Run existing unit tests**

Run:
```
build/debug/bin/evmone-unittests
```
Expected: all tests pass. (The cherry-picks include their own test changes; existing tests should continue to pass.)

- [ ] **Step 6: No commit step**

The cherry-picks already committed themselves. Nothing more to commit.

---

## Task 2: Define `EngineTest` types and skeleton loader

Create the public header and an empty-bodied loader so subsequent TDD steps have a target to make fail.

**Files:**
- Create: `test/utils/engine_test.hpp`
- Create: `test/utils/engine_test_loader.cpp`
- Modify: `test/utils/CMakeLists.txt`

- [ ] **Step 1: Write the header `test/utils/engine_test.hpp`**

```cpp
// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <evmc/evmc.hpp>
#include <test/state/block.hpp>
#include <test/state/bloom_filter.hpp>
#include <test/utils/blob_schedule.hpp>
#include <test/utils/blockchaintest.hpp>  // BlockHeader, UnsupportedTestFeature
#include <test/utils/test_state.hpp>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace evmone::test
{

/// One execution payload (Engine API `newPayloadV4`-shaped) plus its expected outputs.
struct EnginePayload
{
    /// Block context built from `params[0]` + `params[2]` (parent beacon block root).
    state::BlockInfo block_info;

    /// Raw RLP-encoded transactions, as found in the payload (hex-decoded to bytes).
    /// Decoded by the runner at execution time via state::rlp_decode.
    std::vector<bytes> transactions_rlp;

    /// Payload's declared `blockHash`. Verified through parent-hash chaining.
    hash256 expected_block_hash;

    /// Expected outputs declared by the payload, used for verification:
    hash256 expected_state_root;
    hash256 expected_receipts_root;
    state::BloomFilter expected_logs_bloom;
    int64_t expected_gas_used = 0;

    /// EEST `validationError`. Present ⇒ the payload is expected to be rejected.
    std::optional<std::string> validation_error;
};

/// One fixture entry in a `blockchain_test_engine` JSON file.
struct EngineTest
{
    std::string name;
    std::string network;
    RevisionSchedule rev;
    BlobSchedule blob_schedule;
    TestState pre_state;
    BlockHeader genesis;
    std::vector<EnginePayload> payloads;
    hash256 last_block_hash;
    TestState post_state;
};

struct TestResult
{
    bool passed;
    std::string error;
};

/// Parse a `blockchain_test_engine` JSON document. Throws UnsupportedTestFeature
/// if any test entry's `_info.fixture-format` is not `blockchain_test_engine`.
std::vector<EngineTest> load_engine_tests(std::string_view json);

/// Run one engine test. Returns {true, ""} on PASS, {false, "..."} on FAIL.
/// Does not throw on test failures; throws only on programmer errors.
TestResult run_engine_test(const EngineTest& t, evmc::VM& vm);

/// Top-level CLI glue: parse `json`, run every test, write PASS/FAIL lines to `out`,
/// return failure count (clamped to 255) as an exit code.
int run_engine_tests_json(std::string_view json, evmc::VM& vm, std::ostream& out);

}  // namespace evmone::test
```

- [ ] **Step 2: Write a stub `test/utils/engine_test_loader.cpp`**

```cpp
// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "engine_test.hpp"
#include "statetest.hpp"
#include "utils.hpp"

namespace evmone::test
{

std::vector<EngineTest> load_engine_tests(std::string_view /*json*/)
{
    throw UnsupportedTestFeature{"load_engine_tests not implemented"};
}

}  // namespace evmone::test
```

- [ ] **Step 3: Add the new sources to `evmone::testutils`**

Modify `test/utils/CMakeLists.txt`. Find the `target_sources(evmone.testutils PRIVATE ...)` block and add the two new files (alphabetically near `blockchaintest_*`):

```cmake
target_sources(
    evmone.testutils
    PRIVATE
    stdx/utility.hpp
    blob_schedule.hpp
    blob_schedule.cpp
    blockchaintest.hpp
    blockchaintest_loader.cpp
    bytecode.hpp
    engine_test.hpp
    engine_test_loader.cpp
    mpt.hpp
    ...
)
```

(Preserve any other lines already there; only add the two new entries.)

- [ ] **Step 4: Build**

Run:
```
cmake --build build/debug -j
```
Expected: clean build. `engine_test_loader.cpp` compiles as part of `evmone.testutils`.

- [ ] **Step 5: Commit**

```
git add test/utils/engine_test.hpp test/utils/engine_test_loader.cpp test/utils/CMakeLists.txt
git commit -m "test: Add EngineTest types and skeleton loader

Introduce engine_test.hpp with EnginePayload/EngineTest/TestResult and a
stub load_engine_tests() that throws UnsupportedTestFeature. Subsequent
commits will implement parsing and runner logic on top.

Part of the evmone test subcommand for blockchain_test_engine fixtures
(see docs/superpowers/specs/2026-05-16-evmone-test-engine-fixtures-design.md)."
```

---

## Task 3: Loader — minimal-fixture parsing (TDD)

Implement loader parsing for a minimal `blockchain_test_engine` fixture: top-level entries, name, network, blob_schedule, pre/post state, genesis header, `lastblockhash`, and an empty `engineNewPayloads`.

**Files:**
- Modify: `test/utils/engine_test_loader.cpp`
- Create: `test/unittests/engine_test_loader_test.cpp`
- Modify: `test/unittests/CMakeLists.txt`

- [ ] **Step 1: Write the failing test in `test/unittests/engine_test_loader_test.cpp`**

```cpp
// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <test/utils/engine_test.hpp>
#include <test/utils/utils.hpp>

using namespace evmone;
using namespace evmone::test;
using namespace testing;

// A minimal-but-valid blockchain_test_engine fixture with zero payloads.
constexpr auto MINIMAL_ENGINE_JSON = R"({
    "minimal_test": {
        "network": "Osaka",
        "lastblockhash": "0x0000000000000000000000000000000000000000000000000000000000000001",
        "config": {
            "network": "Osaka",
            "chainid": "0x01",
            "blobSchedule": {
                "Osaka": {
                    "target": "0x06",
                    "max": "0x09",
                    "baseFeeUpdateFraction": "0x4c6964"
                }
            }
        },
        "pre": {},
        "genesisBlockHeader": {
            "parentHash":  "0x0000000000000000000000000000000000000000000000000000000000000000",
            "uncleHash":   "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347",
            "coinbase":    "0x0000000000000000000000000000000000000000",
            "stateRoot":   "0x0000000000000000000000000000000000000000000000000000000000000002",
            "transactionsTrie": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
            "receiptTrie": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
            "bloom": "0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
            "difficulty": "0x00",
            "number": "0x00",
            "gasLimit": "0x07270e00",
            "gasUsed": "0x00",
            "timestamp": "0x00",
            "extraData": "0x00",
            "mixHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
            "nonce": "0x0000000000000000",
            "baseFeePerGas": "0x07",
            "withdrawalsRoot": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
            "blobGasUsed": "0x00",
            "excessBlobGas": "0x00",
            "parentBeaconBlockRoot": "0x0000000000000000000000000000000000000000000000000000000000000000",
            "requestsHash": "0xe3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
            "hash": "0x0000000000000000000000000000000000000000000000000000000000000003"
        },
        "engineNewPayloads": [],
        "postState": {},
        "_info": {
            "hash": "0xaa",
            "fixture-format": "blockchain_test_engine"
        }
    }
})";

TEST(engine_test_loader, minimal_fixture)
{
    const auto tests = load_engine_tests(MINIMAL_ENGINE_JSON);
    ASSERT_EQ(tests.size(), 1u);
    EXPECT_EQ(tests[0].name, "minimal_test");
    EXPECT_EQ(tests[0].network, "Osaka");
    EXPECT_TRUE(tests[0].pre_state.empty());
    EXPECT_TRUE(tests[0].post_state.empty());
    EXPECT_EQ(tests[0].payloads.size(), 0u);
    EXPECT_EQ(tests[0].last_block_hash,
        0x0000000000000000000000000000000000000000000000000000000000000001_bytes32);
    EXPECT_EQ(tests[0].genesis.block_number, 0);
    EXPECT_EQ(tests[0].genesis.hash,
        0x0000000000000000000000000000000000000000000000000000000000000003_bytes32);
}
```

- [ ] **Step 2: Add the test file to `evmone-unittests`**

Modify `test/unittests/CMakeLists.txt`. Find the `target_sources(evmone-unittests PRIVATE ...)` block (lines around 8–80) and add the new test source near the existing `blockchaintest_loader_test.cpp` entry:

```cmake
    blockchaintest_loader_test.cpp
    bytecode_test.cpp
    engine_test_loader_test.cpp
```

- [ ] **Step 3: Build and run the new test — expect a failure**

```
cmake --build build/debug -j evmone-unittests
build/debug/bin/evmone-unittests --gtest_filter='engine_test_loader.*'
```
Expected: the test runs and **fails** with `UnsupportedTestFeature: load_engine_tests not implemented` (because the stub still throws). This confirms the test wiring is correct.

- [ ] **Step 4: Implement minimal loader in `test/utils/engine_test_loader.cpp`**

Replace the file's body with the real implementation:

```cpp
// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "engine_test.hpp"
#include "statetest.hpp"
#include "utils.hpp"

namespace evmone::test
{

namespace
{
EngineTest load_engine_test_case(const std::string& name, const json::json& j)
{
    using namespace state;

    EngineTest et;
    et.name = name;
    et.network = j.at("network").get<std::string>();
    et.rev = to_rev_schedule(et.network);

    if (const auto config_it = j.find("config"); config_it != j.end())
    {
        if (const auto bs_it = config_it->find("blobSchedule"); bs_it != config_it->end())
            et.blob_schedule = from_json<BlobSchedule>(*bs_it);
    }

    et.pre_state = from_json<TestState>(j.at("pre"));
    et.post_state = from_json<TestState>(j.at("postState"));
    et.genesis = from_json<BlockHeader>(j.at("genesisBlockHeader"));
    et.last_block_hash = from_json<hash256>(j.at("lastblockhash"));

    // engineNewPayloads parsing comes in the next task. For now: require the
    // field to exist (per the format) but only accept an empty array.
    const auto& payloads_json = j.at("engineNewPayloads");
    if (!payloads_json.empty())
        throw UnsupportedTestFeature{"engineNewPayloads parsing not implemented yet"};

    return et;
}
}  // namespace

std::vector<EngineTest> load_engine_tests(std::string_view json_str)
{
    const auto j = json::json::parse(json_str);
    std::vector<EngineTest> tests;
    for (const auto& [name, entry] : j.items())
    {
        // Format detection comes in the next task.
        load_engine_test_case(name, entry);
        tests.emplace_back(load_engine_test_case(name, entry));
    }
    return tests;
}

}  // namespace evmone::test
```

- [ ] **Step 5: Build and re-run the test — expect PASS**

```
cmake --build build/debug -j evmone-unittests
build/debug/bin/evmone-unittests --gtest_filter='engine_test_loader.minimal_fixture'
```
Expected: PASS.

- [ ] **Step 6: Commit**

```
git add test/utils/engine_test_loader.cpp test/unittests/engine_test_loader_test.cpp test/unittests/CMakeLists.txt
git commit -m "test: Implement minimal blockchain_test_engine loader

Parse top-level entries: name, network, blob_schedule, pre/post state,
genesisBlockHeader, lastblockhash. engineNewPayloads must be present but
is currently restricted to the empty list; payload parsing lands next."
```

---

## Task 4: Loader — fixture-format detection

Reject any fixture whose `_info.fixture-format` is not `blockchain_test_engine`.

**Files:**
- Modify: `test/utils/engine_test_loader.cpp`
- Modify: `test/unittests/engine_test_loader_test.cpp`

- [ ] **Step 1: Add failing tests for format detection**

Append to `test/unittests/engine_test_loader_test.cpp`:

```cpp
TEST(engine_test_loader, rejects_blockchain_test_format)
{
    constexpr auto json = R"({
        "t": { "_info": { "fixture-format": "blockchain_test" } }
    })";
    EXPECT_THROW(load_engine_tests(json), UnsupportedTestFeature);
}

TEST(engine_test_loader, rejects_state_test_format)
{
    constexpr auto json = R"({
        "t": { "_info": { "fixture-format": "state_test" } }
    })";
    EXPECT_THROW(load_engine_tests(json), UnsupportedTestFeature);
}

TEST(engine_test_loader, rejects_blockchain_test_engine_x_format)
{
    constexpr auto json = R"({
        "t": { "_info": { "fixture-format": "blockchain_test_engine_x" } }
    })";
    EXPECT_THROW(load_engine_tests(json), UnsupportedTestFeature);
}

TEST(engine_test_loader, rejects_missing_info)
{
    constexpr auto json = R"({ "t": { "network": "Osaka" } })";
    EXPECT_THROW(load_engine_tests(json), UnsupportedTestFeature);
}
```

- [ ] **Step 2: Run them — expect failures**

```
build/debug/bin/evmone-unittests --gtest_filter='engine_test_loader.rejects_*'
```
Expected: these tests fail (they don't throw, because the loader currently has no format check — the existing impl will throw on `j.at("network")` etc. for these malformed JSONs; that's the wrong exception type).

- [ ] **Step 3: Add the format check in `engine_test_loader.cpp`**

Replace the body of `load_engine_tests` with:

```cpp
std::vector<EngineTest> load_engine_tests(std::string_view json_str)
{
    const auto j = json::json::parse(json_str);
    std::vector<EngineTest> tests;
    for (const auto& [name, entry] : j.items())
    {
        const auto info_it = entry.find("_info");
        if (info_it == entry.end())
            throw UnsupportedTestFeature{"unsupported fixture format: <missing _info>"};
        const auto fmt_it = info_it->find("fixture-format");
        if (fmt_it == info_it->end())
            throw UnsupportedTestFeature{
                "unsupported fixture format: <missing fixture-format>"};
        const auto fmt = fmt_it->get<std::string>();
        if (fmt != "blockchain_test_engine")
            throw UnsupportedTestFeature{"unsupported fixture format: " + fmt};

        tests.emplace_back(load_engine_test_case(name, entry));
    }
    return tests;
}
```

(Remove the duplicate `load_engine_test_case(name, entry);` line that was previously above the `emplace_back` — that was a placeholder to keep the function signature exercised.)

- [ ] **Step 4: Run the new tests + the previous minimal test**

```
cmake --build build/debug -j evmone-unittests
build/debug/bin/evmone-unittests --gtest_filter='engine_test_loader.*'
```
Expected: all five tests pass.

- [ ] **Step 5: Commit**

```
git add test/utils/engine_test_loader.cpp test/unittests/engine_test_loader_test.cpp
git commit -m "test: Reject non-engine fixture formats in load_engine_tests

Check _info.fixture-format on every test entry; throw
UnsupportedTestFeature for blockchain_test, state_test,
blockchain_test_engine_x, or any entry missing _info."
```

---

## Task 5: Loader — payload mapping (transactions, params[2], withdrawals, validationError)

Parse `engineNewPayloads[]` into `EnginePayload` records.

**Files:**
- Modify: `test/utils/engine_test_loader.cpp`
- Modify: `test/unittests/engine_test_loader_test.cpp`

- [ ] **Step 1: Add failing tests covering payload mapping**

Append to `test/unittests/engine_test_loader_test.cpp`:

```cpp
// A blockchain_test_engine fixture with one transaction-free payload that
// also carries a validationError. The payload has minimal but valid fields.
constexpr auto SINGLE_PAYLOAD_JSON = R"({
    "single_payload": {
        "network": "Osaka",
        "lastblockhash": "0x0000000000000000000000000000000000000000000000000000000000000005",
        "config": {
            "network": "Osaka",
            "chainid": "0x01",
            "blobSchedule": {
                "Osaka": {
                    "target": "0x06",
                    "max": "0x09",
                    "baseFeeUpdateFraction": "0x4c6964"
                }
            }
        },
        "pre": {},
        "postState": {},
        "genesisBlockHeader": {
            "parentHash":  "0x0000000000000000000000000000000000000000000000000000000000000000",
            "uncleHash":   "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347",
            "coinbase":    "0x0000000000000000000000000000000000000000",
            "stateRoot":   "0x0000000000000000000000000000000000000000000000000000000000000002",
            "transactionsTrie": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
            "receiptTrie": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
            "bloom": "0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
            "difficulty": "0x00",
            "number": "0x00",
            "gasLimit": "0x07270e00",
            "gasUsed": "0x00",
            "timestamp": "0x00",
            "extraData": "0x00",
            "mixHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
            "nonce": "0x0000000000000000",
            "baseFeePerGas": "0x07",
            "withdrawalsRoot": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
            "blobGasUsed": "0x00",
            "excessBlobGas": "0x00",
            "parentBeaconBlockRoot": "0x0000000000000000000000000000000000000000000000000000000000000000",
            "requestsHash": "0xe3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
            "hash": "0x0000000000000000000000000000000000000000000000000000000000000003"
        },
        "engineNewPayloads": [
            {
                "params": [
                    {
                        "parentHash":     "0x0000000000000000000000000000000000000000000000000000000000000003",
                        "feeRecipient":   "0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba",
                        "stateRoot":      "0x0000000000000000000000000000000000000000000000000000000000000020",
                        "receiptsRoot":   "0x0000000000000000000000000000000000000000000000000000000000000021",
                        "logsBloom":      "0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                        "blockNumber":    "0x1",
                        "gasLimit":       "0x7270e00",
                        "gasUsed":        "0x534a",
                        "timestamp":      "0x3e8",
                        "extraData":      "0x00",
                        "prevRandao":     "0x0000000000000000000000000000000000000000000000000000000000000000",
                        "baseFeePerGas":  "0x7",
                        "blobGasUsed":    "0x0",
                        "excessBlobGas":  "0x0",
                        "blockHash":      "0x0000000000000000000000000000000000000000000000000000000000000099",
                        "transactions": [
                            "0xdeadbeef"
                        ],
                        "withdrawals": []
                    },
                    [],
                    "0x0000000000000000000000000000000000000000000000000000000000000010",
                    []
                ],
                "newPayloadVersion": "4",
                "forkchoiceUpdatedVersion": "3",
                "validationError": "BlockException.INVALID_BASEFEE_PER_GAS"
            }
        ],
        "_info": {
            "hash": "0xbb",
            "fixture-format": "blockchain_test_engine"
        }
    }
})";

TEST(engine_test_loader, single_payload_with_validation_error)
{
    const auto tests = load_engine_tests(SINGLE_PAYLOAD_JSON);
    ASSERT_EQ(tests.size(), 1u);
    const auto& t = tests[0];
    ASSERT_EQ(t.payloads.size(), 1u);

    const auto& p = t.payloads[0];

    // BlockInfo mapping
    EXPECT_EQ(p.block_info.number, 1);
    EXPECT_EQ(p.block_info.gas_limit, 0x7270e00);
    EXPECT_EQ(p.block_info.gas_used, 0x534a);
    EXPECT_EQ(p.block_info.timestamp, 0x3e8);
    EXPECT_EQ(p.block_info.base_fee, 0x7u);
    EXPECT_EQ(p.block_info.coinbase,
        0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba_address);
    EXPECT_EQ(p.block_info.parent_hash,
        0x0000000000000000000000000000000000000000000000000000000000000003_bytes32);
    EXPECT_EQ(p.block_info.parent_beacon_block_root,
        0x0000000000000000000000000000000000000000000000000000000000000010_bytes32);
    EXPECT_TRUE(p.block_info.blob_gas_used.has_value());
    EXPECT_EQ(*p.block_info.blob_gas_used, 0u);
    EXPECT_TRUE(p.block_info.excess_blob_gas.has_value());
    EXPECT_EQ(*p.block_info.excess_blob_gas, 0u);
    EXPECT_TRUE(p.block_info.withdrawals.empty());

    // Transactions stored as raw RLP bytes, no decode.
    ASSERT_EQ(p.transactions_rlp.size(), 1u);
    EXPECT_EQ(p.transactions_rlp[0], evmc::from_hex("0xdeadbeef").value());

    // Expected outputs
    EXPECT_EQ(p.expected_block_hash,
        0x0000000000000000000000000000000000000000000000000000000000000099_bytes32);
    EXPECT_EQ(p.expected_state_root,
        0x0000000000000000000000000000000000000000000000000000000000000020_bytes32);
    EXPECT_EQ(p.expected_receipts_root,
        0x0000000000000000000000000000000000000000000000000000000000000021_bytes32);
    EXPECT_EQ(p.expected_gas_used, 0x534a);

    // validation_error propagated.
    ASSERT_TRUE(p.validation_error.has_value());
    EXPECT_EQ(*p.validation_error, "BlockException.INVALID_BASEFEE_PER_GAS");
}
```

- [ ] **Step 2: Run — expect failure**

```
build/debug/bin/evmone-unittests --gtest_filter='engine_test_loader.single_payload_with_validation_error'
```
Expected: FAIL (current loader throws `UnsupportedTestFeature{"engineNewPayloads parsing not implemented yet"}`).

- [ ] **Step 3: Implement payload mapping in `engine_test_loader.cpp`**

Replace the file with the full implementation:

```cpp
// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "engine_test.hpp"
#include "statetest.hpp"
#include "utils.hpp"

namespace evmone::test
{

namespace
{
template <typename T>
T load_if_exists(const json::json& j, std::string_view key)
{
    if (const auto it = j.find(key); it != j.end())
        return from_json<T>(*it);
    return {};
}

template <typename T>
std::optional<T> load_optional(const json::json& j, std::string_view key)
{
    if (const auto it = j.find(key); it != j.end())
        return from_json<T>(*it);
    return std::nullopt;
}

EnginePayload load_engine_payload(
    const json::json& j, const std::string& network, const BlobSchedule& blob_schedule)
{
    using namespace state;
    EnginePayload p;

    const auto& params = j.at("params");
    if (!params.is_array() || params.size() < 3)
        throw std::runtime_error{"engine payload params: expected an array of length >= 3"};

    const auto& exec_payload = params[0];
    // params[1] (blob versioned hashes) is not stored; covered indirectly by state root.
    const auto& parent_beacon_root_j = params[2];
    // params[3] (execution requests) is not stored; verified implicitly via state root.

    p.block_info.parent_hash = from_json<hash256>(exec_payload.at("parentHash"));
    p.block_info.coinbase = from_json<address>(exec_payload.at("feeRecipient"));
    p.block_info.prev_randao = from_json<bytes32>(exec_payload.at("prevRandao"));
    p.block_info.number = from_json<int64_t>(exec_payload.at("blockNumber"));
    p.block_info.gas_limit = from_json<int64_t>(exec_payload.at("gasLimit"));
    p.block_info.gas_used = from_json<int64_t>(exec_payload.at("gasUsed"));
    p.block_info.timestamp = from_json<int64_t>(exec_payload.at("timestamp"));
    p.block_info.extra_data = from_json<bytes>(exec_payload.at("extraData"));
    p.block_info.base_fee = from_json<uint64_t>(exec_payload.at("baseFeePerGas"));
    p.block_info.blob_gas_used = load_optional<uint64_t>(exec_payload, "blobGasUsed");
    p.block_info.excess_blob_gas = load_optional<uint64_t>(exec_payload, "excessBlobGas");
    p.block_info.parent_beacon_block_root = from_json<hash256>(parent_beacon_root_j);

    // Withdrawals
    if (const auto wit = exec_payload.find("withdrawals"); wit != exec_payload.end())
    {
        for (const auto& w : *wit)
            p.block_info.withdrawals.push_back(from_json<Withdrawal>(w));
    }

    // blob_base_fee, same computation as blockchaintest_loader.cpp
    if (p.block_info.excess_blob_gas.has_value())
    {
        const auto blob_params = get_blob_params(network, blob_schedule, p.block_info.timestamp);
        p.block_info.blob_base_fee =
            compute_blob_gas_price(blob_params, *p.block_info.excess_blob_gas);
    }

    // Raw RLP transactions, no decoding here.
    for (const auto& tx_hex : exec_payload.at("transactions"))
        p.transactions_rlp.push_back(from_json<bytes>(tx_hex));

    p.expected_block_hash = from_json<hash256>(exec_payload.at("blockHash"));
    p.expected_state_root = from_json<hash256>(exec_payload.at("stateRoot"));
    p.expected_receipts_root = from_json<hash256>(exec_payload.at("receiptsRoot"));
    p.expected_logs_bloom = bloom_filter_from_bytes(from_json<bytes>(exec_payload.at("logsBloom")));
    p.expected_gas_used = p.block_info.gas_used;

    if (const auto ve_it = j.find("validationError"); ve_it != j.end())
        p.validation_error = ve_it->get<std::string>();

    return p;
}

EngineTest load_engine_test_case(const std::string& name, const json::json& j)
{
    using namespace state;

    EngineTest et;
    et.name = name;
    et.network = j.at("network").get<std::string>();
    et.rev = to_rev_schedule(et.network);

    if (const auto config_it = j.find("config"); config_it != j.end())
    {
        if (const auto bs_it = config_it->find("blobSchedule"); bs_it != config_it->end())
            et.blob_schedule = from_json<BlobSchedule>(*bs_it);
    }

    et.pre_state = from_json<TestState>(j.at("pre"));
    et.post_state = from_json<TestState>(j.at("postState"));
    et.genesis = from_json<BlockHeader>(j.at("genesisBlockHeader"));
    et.last_block_hash = from_json<hash256>(j.at("lastblockhash"));

    for (const auto& payload_j : j.at("engineNewPayloads"))
        et.payloads.emplace_back(load_engine_payload(payload_j, et.network, et.blob_schedule));

    return et;
}
}  // namespace

std::vector<EngineTest> load_engine_tests(std::string_view json_str)
{
    const auto j = json::json::parse(json_str);
    std::vector<EngineTest> tests;
    for (const auto& [name, entry] : j.items())
    {
        const auto info_it = entry.find("_info");
        if (info_it == entry.end())
            throw UnsupportedTestFeature{"unsupported fixture format: <missing _info>"};
        const auto fmt_it = info_it->find("fixture-format");
        if (fmt_it == info_it->end())
            throw UnsupportedTestFeature{"unsupported fixture format: <missing fixture-format>"};
        const auto fmt = fmt_it->get<std::string>();
        if (fmt != "blockchain_test_engine")
            throw UnsupportedTestFeature{"unsupported fixture format: " + fmt};

        tests.emplace_back(load_engine_test_case(name, entry));
    }
    return tests;
}

}  // namespace evmone::test
```

- [ ] **Step 4: Run all loader tests — expect PASS**

```
cmake --build build/debug -j evmone-unittests
build/debug/bin/evmone-unittests --gtest_filter='engine_test_loader.*'
```
Expected: all six tests pass.

- [ ] **Step 5: Commit**

```
git add test/utils/engine_test_loader.cpp test/unittests/engine_test_loader_test.cpp
git commit -m "test: Parse engineNewPayloads into EnginePayload records

Map params[0] (ExecutionPayload) to state::BlockInfo, hex-decode the
transactions array into raw RLP bytes (decoded later by the runner),
take params[2] as parent_beacon_block_root, and capture validationError
when present. params[1] (blob hashes) and params[3] (execution requests)
are not stored; verification covers them implicitly via the state root."
```

---

## Task 6: Runner — skeleton + empty-payload happy path

Add an `engine_test_runner.cpp` with an internal `apply_block` helper. First runner test exercises an `EngineTest` that has zero payloads (post-state must equal pre-state, last block hash must equal genesis hash).

**Files:**
- Create: `test/utils/engine_test_runner.cpp`
- Modify: `test/utils/CMakeLists.txt`
- Create: `test/unittests/engine_test_runner_test.cpp`
- Modify: `test/unittests/CMakeLists.txt`

- [ ] **Step 1: Write the failing test in `test/unittests/engine_test_runner_test.cpp`**

```cpp
// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmc/evmc.hpp>
#include <evmone/evmone.h>
#include <gmock/gmock.h>
#include <test/utils/engine_test.hpp>
#include <test/utils/utils.hpp>

using namespace evmone;
using namespace evmone::test;
using namespace testing;

namespace
{
EngineTest make_empty_engine_test()
{
    EngineTest t;
    t.name = "empty";
    t.network = "Osaka";
    t.rev = to_rev_schedule(t.network);
    t.genesis.hash =
        0x0000000000000000000000000000000000000000000000000000000000000003_bytes32;
    t.last_block_hash = t.genesis.hash;  // no payloads → head stays at genesis
    return t;
}
}  // namespace

TEST(engine_test_runner, empty_payloads_pass)
{
    evmc::VM vm{evmc_create_evmone()};
    const auto result = run_engine_test(make_empty_engine_test(), vm);
    EXPECT_TRUE(result.passed) << result.error;
    EXPECT_EQ(result.error, "");
}
```

- [ ] **Step 2: Wire the test into `evmone-unittests`**

Modify `test/unittests/CMakeLists.txt`, add `engine_test_runner_test.cpp` next to `engine_test_loader_test.cpp`:

```cmake
    engine_test_loader_test.cpp
    engine_test_runner_test.cpp
```

- [ ] **Step 3: Run — expect link error (run_engine_test not yet defined)**

```
cmake --build build/debug -j evmone-unittests
```
Expected: link error: `undefined reference to evmone::test::run_engine_test`.

- [ ] **Step 4: Create `test/utils/engine_test_runner.cpp` with minimal happy-path implementation**

```cpp
// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "engine_test.hpp"
#include "mpt_hash.hpp"
#include "rlp_encode.hpp"
#include <test/state/requests.hpp>
#include <test/state/state.hpp>

namespace evmone::test
{

namespace
{
struct RejectedTransaction
{
    hash256 hash;
    size_t index;
    std::string message;
};

struct BlockResult
{
    std::vector<state::TransactionReceipt> receipts;
    std::vector<RejectedTransaction> rejected;
    std::optional<std::vector<state::Requests>> requests;
    int64_t gas_used;
    state::BloomFilter bloom;
    int64_t blob_gas_left;
    TestState block_state;
};

// Modeled on blockchaintest_runner.cpp:apply_block. Returns a BlockResult.
// Transactions are pre-decoded (by the caller) into state::Transaction values.
[[maybe_unused]] BlockResult apply_block(
    const TestState& state, evmc::VM& vm, const state::BlockInfo& block,
    const state::BlockHashes& block_hashes,
    const std::vector<state::Transaction>& txs, evmc_revision rev)
{
    TestState block_state(state);
    system_call_block_start(block_state, block, block_hashes, rev, vm);

    int64_t block_gas_left = block.gas_limit;
    auto blob_gas_left = static_cast<int64_t>(block.blob_gas_used.value_or(0));

    std::vector<RejectedTransaction> rejected_txs;
    std::vector<state::TransactionReceipt> receipts;
    int64_t cumulative_gas_used = 0;
    int64_t block_gas_used = 0;

    for (size_t i = 0; i < txs.size(); ++i)
    {
        const auto& tx = txs[i];
        const auto tx_hash = keccak256(rlp::encode(tx));
        auto res = transition(
            block_state, block, block_hashes, tx, rev, vm, block_gas_left, blob_gas_left);
        if (std::holds_alternative<std::error_code>(res))
        {
            rejected_txs.push_back({tx_hash, i, std::get<std::error_code>(res).message()});
        }
        else
        {
            auto& receipt = std::get<state::TransactionReceipt>(res);
            cumulative_gas_used += receipt.gas_used;
            receipt.cumulative_gas_used = cumulative_gas_used;
            if (rev < EVMC_BYZANTIUM)
                receipt.post_state = state::mpt_hash(block_state);

            const auto block_tx_gas = (rev >= EVMC_AMSTERDAM) ?
                                          receipt.gas_used + receipt.gas_refund :
                                          receipt.gas_used;
            block_gas_used += block_tx_gas;
            block_gas_left -= block_tx_gas;
            blob_gas_left -= static_cast<int64_t>(tx.blob_gas_used());
            receipts.emplace_back(std::move(receipt));
        }
    }

    auto requests = [&]() -> std::optional<std::vector<state::Requests>> {
        std::vector<state::Requests> collected;
        if (rev >= EVMC_PRAGUE)
        {
            auto opt_deposits = collect_deposit_requests(receipts);
            if (!opt_deposits.has_value())
                return std::nullopt;
            collected.emplace_back(std::move(*opt_deposits));
        }
        auto sysreq = system_call_block_end(block_state, block, block_hashes, rev, vm);
        if (!sysreq.has_value())
            return std::nullopt;
        std::ranges::move(*sysreq, std::back_inserter(collected));
        return collected;
    }();

    finalize(block_state, rev, block.coinbase, std::nullopt, block.ommers, block.withdrawals);

    return {std::move(receipts), std::move(rejected_txs), std::move(requests), block_gas_used,
        compute_bloom_filter(receipts), blob_gas_left, std::move(block_state)};
}
}  // namespace

TestResult run_engine_test(const EngineTest& t, evmc::VM& vm)
{
    (void)vm;

    // Empty-payload path: chain head must equal genesis hash, post == pre.
    if (t.payloads.empty())
    {
        if (t.last_block_hash != t.genesis.hash)
            return {false, "final: lastblockhash differs from genesis hash with no payloads"};
        if (state::mpt_hash(t.pre_state) != state::mpt_hash(t.post_state))
            return {false, "final: post state differs from pre state with no payloads"};
        return {true, ""};
    }

    return {false, "engine runner: payload execution not yet implemented"};
}

}  // namespace evmone::test
```

- [ ] **Step 5: Add the runner source to `evmone::testutils`**

Modify `test/utils/CMakeLists.txt` — add `engine_test_runner.cpp` next to `engine_test_loader.cpp`:

```cmake
    engine_test.hpp
    engine_test_loader.cpp
    engine_test_runner.cpp
```

- [ ] **Step 6: Build and run — expect PASS**

```
cmake --build build/debug -j evmone-unittests
build/debug/bin/evmone-unittests --gtest_filter='engine_test_runner.empty_payloads_pass'
```
Expected: PASS.

- [ ] **Step 7: Commit**

```
git add test/utils/engine_test_runner.cpp test/utils/CMakeLists.txt test/unittests/engine_test_runner_test.cpp test/unittests/CMakeLists.txt
git commit -m "test: Add engine test runner skeleton with apply_block helper

Implement the no-payload happy path. apply_block is modeled on
blockchaintest_runner.cpp but is gtest-free and returns a BlockResult.
Payload execution and verification land in subsequent commits."
```

---

## Task 7: Runner — payload execution, decode, sender recovery, verification

Wire `apply_block` into the per-payload loop. Decode each `transactions_rlp` entry to `state::Transaction`, recover senders, apply the block, and verify expected outputs.

**Files:**
- Modify: `test/utils/engine_test_runner.cpp`
- Modify: `test/unittests/engine_test_runner_test.cpp`

- [ ] **Step 1: Write the failing test — payload with mismatched state root must fail with a descriptive error**

Append to `test/unittests/engine_test_runner_test.cpp`:

```cpp
namespace
{
EnginePayload make_empty_payload_with_bad_state_root()
{
    EnginePayload p;
    p.block_info.number = 1;
    p.block_info.gas_limit = 0x7270e00;
    p.block_info.gas_used = 0;
    p.block_info.timestamp = 0x3e8;
    p.block_info.base_fee = 7;
    p.block_info.coinbase = 0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba_address;
    p.block_info.parent_hash =
        0x0000000000000000000000000000000000000000000000000000000000000003_bytes32;
    p.block_info.blob_gas_used = 0u;
    p.block_info.excess_blob_gas = 0u;
    p.block_info.parent_beacon_block_root =
        0x0000000000000000000000000000000000000000000000000000000000000000_bytes32;
    p.expected_block_hash =
        0x0000000000000000000000000000000000000000000000000000000000000099_bytes32;
    // Deliberately wrong: an empty block produces the empty MPT root for state,
    // not 0xdead...
    p.expected_state_root =
        0xdeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddead_bytes32;
    p.expected_receipts_root =
        0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421_bytes32;
    p.expected_gas_used = 0;
    return p;
}

EngineTest make_test_with_bad_state_root()
{
    EngineTest t;
    t.name = "bad_state_root";
    t.network = "Osaka";
    t.rev = to_rev_schedule(t.network);
    t.genesis.hash =
        0x0000000000000000000000000000000000000000000000000000000000000003_bytes32;
    t.payloads.push_back(make_empty_payload_with_bad_state_root());
    t.last_block_hash = t.payloads[0].expected_block_hash;
    return t;
}
}  // namespace

TEST(engine_test_runner, state_root_mismatch_fails)
{
    evmc::VM vm{evmc_create_evmone()};
    const auto result = run_engine_test(make_test_with_bad_state_root(), vm);
    EXPECT_FALSE(result.passed);
    EXPECT_THAT(result.error, HasSubstr("state root mismatch"));
}
```

- [ ] **Step 2: Run — expect FAIL because the runner still returns the stub error**

```
build/debug/bin/evmone-unittests --gtest_filter='engine_test_runner.state_root_mismatch_fails'
```
Expected: assertion fails — the error message is "engine runner: payload execution not yet implemented", which does not contain "state root mismatch".

- [ ] **Step 3: Implement the per-payload loop in `engine_test_runner.cpp`**

Replace `run_engine_test` (keep the existing namespace + `apply_block` helper, just replace this function and add the necessary includes):

Add to the includes at the top (alongside the existing includes from Task 6):

```cpp
#include "rlp.hpp"
#include <test/utils/rlp_decode.hpp>
#include <sstream>
```

Then replace `run_engine_test`:

```cpp
TestResult run_engine_test(const EngineTest& t, evmc::VM& vm)
{
    using namespace state;

    TestState current_state = t.pre_state;
    BlockHashes block_hashes{{0, t.genesis.hash}};

    for (size_t pi = 0; pi < t.payloads.size(); ++pi)
    {
        const auto& p = t.payloads[pi];
        const auto rev = t.rev.get_revision(p.block_info.timestamp);

        const auto fail = [&](std::string_view what) -> TestResult {
            std::ostringstream os;
            os << "payload " << pi << ": " << what;
            return {false, os.str()};
        };
        const auto expected_invalid = p.validation_error.has_value();

        // 1. Decode transactions.
        std::vector<Transaction> txs;
        txs.reserve(p.transactions_rlp.size());
        for (size_t ti = 0; ti < p.transactions_rlp.size(); ++ti)
        {
            try
            {
                Transaction tx;
                bytes_view view{p.transactions_rlp[ti]};
                rlp_decode(view, tx);
                txs.emplace_back(std::move(tx));
            }
            catch (const std::exception& ex)
            {
                if (expected_invalid)
                    goto next_payload;
                std::ostringstream os;
                os << "tx " << ti << " RLP decode error: " << ex.what();
                return fail(os.str());
            }
        }

        // 2. Recover senders.
        for (size_t ti = 0; ti < txs.size(); ++ti)
        {
            auto sender = recover_sender(txs[ti]);
            if (!sender.has_value())
            {
                if (expected_invalid)
                    goto next_payload;
                std::ostringstream os;
                os << "tx " << ti << " sender recovery failed";
                return fail(os.str());
            }
            txs[ti].sender = *sender;
        }

        {
            // 3. Apply the block.
            auto res = apply_block(current_state, vm, p.block_info, block_hashes, txs, rev);

            // 4. Verify expected outputs.
            const auto verify_failed = [&]() -> std::optional<std::string> {
                if (!res.requests.has_value())
                    return "system requests collection failed";
                if (!res.rejected.empty())
                    return "transaction rejected: " + res.rejected[0].message;
                if (res.blob_gas_left != 0)
                    return "blob gas mismatch (left=" + std::to_string(res.blob_gas_left) + ")";
                if (mpt_hash(res.block_state) != p.expected_state_root)
                    return "state root mismatch";
                if (mpt_hash(res.receipts) != p.expected_receipts_root)
                    return "receipts root mismatch";
                if (bytes_view{res.bloom} != bytes_view{p.expected_logs_bloom})
                    return "logs bloom mismatch";
                if (res.gas_used != p.expected_gas_used)
                    return "gas used mismatch (got " + std::to_string(res.gas_used) +
                           ", expected " + std::to_string(p.expected_gas_used) + ")";
                return std::nullopt;
            }();

            if (verify_failed.has_value())
            {
                if (expected_invalid)
                    goto next_payload;
                return fail(*verify_failed);
            }

            if (expected_invalid)
                return fail("expected validation error '" + *p.validation_error +
                            "' but block was accepted");

            // Advance chain head.
            current_state = std::move(res.block_state);
            block_hashes[p.block_info.number] = p.expected_block_hash;
        }

    next_payload:
        continue;
    }

    // Final checks.
    if (state::mpt_hash(current_state) != state::mpt_hash(t.post_state))
        return {false, "final: post state mismatch"};

    if (!t.payloads.empty())
    {
        const auto& last_p = t.payloads.back();
        if (!last_p.validation_error.has_value() && last_p.expected_block_hash != t.last_block_hash)
            return {false, "final: last applied block hash differs from lastblockhash"};
    }
    else if (t.last_block_hash != t.genesis.hash)
    {
        return {false, "final: lastblockhash differs from genesis hash with no payloads"};
    }

    return {true, ""};
}
```

- [ ] **Step 4: Build and run state-root-mismatch test — expect PASS**

```
cmake --build build/debug -j evmone-unittests
build/debug/bin/evmone-unittests --gtest_filter='engine_test_runner.state_root_mismatch_fails'
```
Expected: PASS.

- [ ] **Step 5: Re-run the empty-payloads test — must still pass**

```
build/debug/bin/evmone-unittests --gtest_filter='engine_test_runner.*'
```
Expected: both runner tests pass.

- [ ] **Step 6: Commit**

```
git add test/utils/engine_test_runner.cpp test/unittests/engine_test_runner_test.cpp
git commit -m "test: Apply engine payload, verify state/receipts/bloom/gas

Per payload: RLP-decode transactions, recover senders, apply_block, then
verify state root, receipts root, logs bloom, and gas used against the
payload-declared values. Advance the chain head on success. Detailed
verification failures are reported as 'payload N: <reason>'."
```

---

## Task 8: Runner — `validation_error` semantics

Cover the `validation_error` flow: an actually-invalid block flagged
`validation_error` must produce PASS. The inverse direction (valid block
flagged `validation_error` → FAIL) is exercised indirectly via the e2e
fixture run in Task 11, since constructing a hermetic "valid block"
without pre-computed expected hashes is not worth the test scaffolding.

**Files:**
- Modify: `test/unittests/engine_test_runner_test.cpp`

- [ ] **Step 1: Add a test for the validation-error PASS path**

Append to `test/unittests/engine_test_runner_test.cpp`:

```cpp
TEST(engine_test_runner, validation_error_with_invalid_block_passes)
{
    auto t = make_test_with_bad_state_root();
    t.payloads[0].validation_error = "BlockException.INVALID_STATE_ROOT";
    // With validation_error set, the state-root mismatch is the expected
    // outcome → result is PASS.
    evmc::VM vm{evmc_create_evmone()};
    const auto result = run_engine_test(t, vm);
    EXPECT_TRUE(result.passed) << result.error;
}

TEST(engine_test_runner, validation_error_with_bad_rlp_passes)
{
    auto t = make_test_with_bad_state_root();
    // Replace the (currently empty) RLP list with garbage that will fail
    // decoding before apply_block runs. With validation_error set, the
    // decode failure is the expected outcome → PASS.
    t.payloads[0].transactions_rlp.push_back(*evmc::from_hex("0xff"));
    t.payloads[0].validation_error = "BlockException.INVALID_TRANSACTION";
    evmc::VM vm{evmc_create_evmone()};
    const auto result = run_engine_test(t, vm);
    EXPECT_TRUE(result.passed) << result.error;
}
```

- [ ] **Step 2: Build and run — expect PASS on both**

```
cmake --build build/debug -j evmone-unittests
build/debug/bin/evmone-unittests --gtest_filter='engine_test_runner.validation_error_*'
```
Expected: both pass.

- [ ] **Step 3: Commit**

```
git add test/unittests/engine_test_runner_test.cpp
git commit -m "test: Cover validation_error PASS paths in engine runner

A payload flagged validation_error PASSes when verification (or RLP
decode / sender recovery) fails as expected. The inverse direction
(valid block flagged validation_error → FAIL) is exercised by the e2e
fixture run, where genuinely valid blocks come pre-computed."
```

---

## Task 9: Top-level glue — `run_engine_tests_json`

Implement the CLI-facing `int run_engine_tests_json(json, vm, out)`. Prints PASS/FAIL lines and returns failure count.

**Files:**
- Modify: `test/utils/engine_test_runner.cpp`
- Modify: `test/unittests/engine_test_runner_test.cpp`

- [ ] **Step 1: Add a failing integration test**

Append to `test/unittests/engine_test_runner_test.cpp`:

```cpp
TEST(engine_test_runner, run_engine_tests_json_prints_pass_for_empty_fixture)
{
    constexpr auto json = R"({
        "minimal_test": {
            "network": "Osaka",
            "lastblockhash": "0x0000000000000000000000000000000000000000000000000000000000000003",
            "config": {
                "network": "Osaka",
                "chainid": "0x01",
                "blobSchedule": {
                    "Osaka": {
                        "target": "0x06",
                        "max": "0x09",
                        "baseFeeUpdateFraction": "0x4c6964"
                    }
                }
            },
            "pre": {},
            "postState": {},
            "genesisBlockHeader": {
                "parentHash":  "0x0000000000000000000000000000000000000000000000000000000000000000",
                "uncleHash":   "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347",
                "coinbase":    "0x0000000000000000000000000000000000000000",
                "stateRoot":   "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
                "transactionsTrie": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
                "receiptTrie": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
                "bloom": "0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                "difficulty": "0x00",
                "number": "0x00",
                "gasLimit": "0x07270e00",
                "gasUsed": "0x00",
                "timestamp": "0x00",
                "extraData": "0x00",
                "mixHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
                "nonce": "0x0000000000000000",
                "baseFeePerGas": "0x07",
                "withdrawalsRoot": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
                "blobGasUsed": "0x00",
                "excessBlobGas": "0x00",
                "parentBeaconBlockRoot": "0x0000000000000000000000000000000000000000000000000000000000000000",
                "requestsHash": "0xe3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                "hash": "0x0000000000000000000000000000000000000000000000000000000000000003"
            },
            "engineNewPayloads": [],
            "_info": { "fixture-format": "blockchain_test_engine" }
        }
    })";

    evmc::VM vm{evmc_create_evmone()};
    std::ostringstream out;
    const int rc = run_engine_tests_json(json, vm, out);
    EXPECT_EQ(rc, 0);
    const auto s = out.str();
    EXPECT_THAT(s, HasSubstr("PASS minimal_test"));
    EXPECT_THAT(s, HasSubstr("1/1 passed"));
}
```

- [ ] **Step 2: Run — expect link error**

```
cmake --build build/debug -j evmone-unittests
```
Expected: `undefined reference to evmone::test::run_engine_tests_json`.

- [ ] **Step 3: Implement `run_engine_tests_json` in `engine_test_runner.cpp`**

Append to the file (inside `namespace evmone::test`):

```cpp
int run_engine_tests_json(std::string_view json, evmc::VM& vm, std::ostream& out)
{
    std::vector<EngineTest> tests;
    try
    {
        tests = load_engine_tests(json);
    }
    catch (const std::exception& ex)
    {
        out << "ERROR: " << ex.what() << "\n";
        return 1;
    }

    size_t failures = 0;
    for (const auto& t : tests)
    {
        const auto r = run_engine_test(t, vm);
        if (r.passed)
        {
            out << "PASS " << t.name << "\n";
        }
        else
        {
            ++failures;
            out << "FAIL " << t.name << "\n  " << r.error << "\n";
        }
    }
    out << (tests.size() - failures) << "/" << tests.size() << " passed\n";

    return static_cast<int>(std::min<size_t>(failures, 255));
}
```

- [ ] **Step 4: Build and run — expect PASS**

```
cmake --build build/debug -j evmone-unittests
build/debug/bin/evmone-unittests --gtest_filter='engine_test_runner.run_engine_tests_json_*'
```
Expected: PASS.

- [ ] **Step 5: Run all runner + loader tests as a regression check**

```
build/debug/bin/evmone-unittests --gtest_filter='engine_test_*'
```
Expected: all engine tests pass.

- [ ] **Step 6: Commit**

```
git add test/utils/engine_test_runner.cpp test/unittests/engine_test_runner_test.cpp
git commit -m "test: Add run_engine_tests_json CLI glue

Top-level entry that loads, runs, and reports test outcomes via a
plain-text PASS/FAIL stream + summary, returning the failure count
(clamped to 255) as an exit code."
```

---

## Task 10: CLI — add the `test` subcommand to `evmone`

Wire `run_engine_tests_json` into `tools/evmone/main.cpp`. Conditional on `EVMONE_TESTING=ON`.

**Files:**
- Modify: `tools/evmone/main.cpp`
- Modify: `tools/evmone/CMakeLists.txt`

- [ ] **Step 1: Update `tools/evmone/CMakeLists.txt`**

Replace the file with:

```cmake
# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2025 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

hunter_add_package(CLI11)
find_package(CLI11 CONFIG REQUIRED)

add_executable(evmone-cli main.cpp)
set_target_properties(evmone-cli PROPERTIES OUTPUT_NAME evmone)
target_link_libraries(evmone-cli PRIVATE evmone evmc::tooling CLI11::CLI11)

if(EVMONE_TESTING)
    target_link_libraries(evmone-cli PRIVATE evmone::testutils)
    target_compile_definitions(evmone-cli PRIVATE EVMONE_HAS_TEST_SUBCOMMAND=1)
endif()
```

- [ ] **Step 2: Patch `tools/evmone/main.cpp`**

Add this include at the top (alongside the existing includes):

```cpp
#ifdef EVMONE_HAS_TEST_SUBCOMMAND
#include <test/utils/engine_test.hpp>
#endif
```

Inside `main()`, after the existing `run_cmd` registration block and before `app.parse`, add:

```cpp
#ifdef EVMONE_HAS_TEST_SUBCOMMAND
        std::string test_path;
        auto& test_cmd = *app.add_subcommand("test",
            "Run an Engine-format blockchain test fixture")->fallthrough();
        test_cmd.add_option("path", test_path, "Path to JSON fixture file")
            ->required()->check(CLI::ExistingFile);
#endif
```

After the existing `if (run_cmd) { ... }` block (and before the final `return 0;`), add:

```cpp
#ifdef EVMONE_HAS_TEST_SUBCOMMAND
            if (test_cmd)
            {
                std::ifstream f{test_path};
                const std::string json{
                    std::istreambuf_iterator<char>{f}, std::istreambuf_iterator<char>{}};
                return evmone::test::run_engine_tests_json(json, vm, std::cout);
            }
#endif
```

- [ ] **Step 3: Build**

```
cmake --build build/debug -j evmone
```
Expected: `build/debug/bin/evmone` builds cleanly.

- [ ] **Step 4: Smoke-test `evmone test --help`**

```
build/debug/bin/evmone test --help
```
Expected: help text shows `path` as a required positional argument.

- [ ] **Step 5: Smoke-test on a real bal-711 fixture**

```
build/debug/bin/evmone test \
  ~/proj/fixtures/bal-711/blockchain_tests_engine/for_osaka/istanbul/eip152_blake2/blake2_delegatecall/blake2_precompile_delegatecall.json
```
Expected: a single `PASS …` line followed by `1/1 passed`, exit code 0.

If this fails, the failure message indicates which verification step diverged — that is the place to debug.

- [ ] **Step 6: Smoke-test format rejection on a non-engine fixture**

```
build/debug/bin/evmone test \
  ~/proj/fixtures/bal-711/blockchain_tests/for_osaka/cancun/eip4844_blobs/blob_txs/invalid_blob_tx_contract_creation.json
```
Expected: an `ERROR: unsupported fixture format: blockchain_test` line, non-zero exit code.

- [ ] **Step 7: Verify the `clang-local` build (`EVMONE_TESTING=OFF`) still works**

```
cmake --build build/clang-local -j evmone
build/clang-local/bin/evmone test --help
```
Expected: build succeeds; `evmone test --help` prints an error like `"Subcommand 'test' is not supported"` (because the subcommand is gated by `EVMONE_HAS_TEST_SUBCOMMAND`).

- [ ] **Step 8: Commit**

```
git add tools/evmone/main.cpp tools/evmone/CMakeLists.txt
git commit -m "cli: Add evmone test subcommand for engine fixtures

evmone test <file.json> runs a blockchain_test_engine fixture against
the EVM via run_engine_tests_json. Available only when EVMONE_TESTING
is on; the slim release build (EVMONE_TESTING=OFF) is unaffected."
```

---

## Task 11: End-to-end fixture smoke test (env-var gated)

Add a single env-var-gated test in `engine_test_runner_test.cpp` that loads a bal-711 fixture from disk and runs it through `run_engine_tests_json`. CI without the fixture set on disk continues to pass.

**Files:**
- Modify: `test/unittests/engine_test_runner_test.cpp`

- [ ] **Step 1: Append the gated test**

```cpp
TEST(engine_test_runner, e2e_bal711_blake2_delegatecall_passes)
{
    const auto* dir = std::getenv("EVMONE_FIXTURES_DIR");
    if (dir == nullptr)
        GTEST_SKIP() << "EVMONE_FIXTURES_DIR not set; skipping on-disk fixture test";

    const std::filesystem::path path = std::filesystem::path{dir} /
        "blockchain_tests_engine/for_osaka/istanbul/eip152_blake2/blake2_delegatecall/"
        "blake2_precompile_delegatecall.json";
    if (!std::filesystem::exists(path))
        GTEST_SKIP() << "fixture not found: " << path;

    std::ifstream f{path};
    const std::string json{
        std::istreambuf_iterator<char>{f}, std::istreambuf_iterator<char>{}};

    evmc::VM vm{evmc_create_evmone()};
    std::ostringstream out;
    const int rc = run_engine_tests_json(json, vm, out);
    EXPECT_EQ(rc, 0) << out.str();
}
```

Add include at the top of the file (if not already present):

```cpp
#include <cstdlib>
#include <filesystem>
#include <fstream>
```

- [ ] **Step 2: Run with `EVMONE_FIXTURES_DIR` unset — must SKIP**

```
build/debug/bin/evmone-unittests --gtest_filter='engine_test_runner.e2e_*'
```
Expected: 1 skipped, 0 failed.

- [ ] **Step 3: Run with `EVMONE_FIXTURES_DIR` set — must PASS**

```
EVMONE_FIXTURES_DIR=$HOME/proj/fixtures/bal-711 \
  build/debug/bin/evmone-unittests --gtest_filter='engine_test_runner.e2e_*'
```
Expected: PASS.

- [ ] **Step 4: Final sweep — all engine tests**

```
build/debug/bin/evmone-unittests --gtest_filter='engine_test_*'
```
Expected: all pass (with `e2e_*` skipped unless the env var is set).

- [ ] **Step 5: Commit**

```
git add test/unittests/engine_test_runner_test.cpp
git commit -m "test: Add env-var-gated e2e fixture run for engine tests

Loads a real bal-711 blockchain_test_engine fixture from
\$EVMONE_FIXTURES_DIR through run_engine_tests_json. Skipped when the
env var is unset so CI without the fixture set on disk keeps passing."
```

---

## Done

Final branch state on `evmone-test`:

```
docs: Add design spec ...                      (30bc3ab6)
[15 fuzzing/main cherry-picks]
test: Add EngineTest types and skeleton loader
test: Implement minimal blockchain_test_engine loader
test: Reject non-engine fixture formats ...
test: Parse engineNewPayloads ...
test: Add engine test runner skeleton ...
test: Apply engine payload, verify ...
test: Cover validation_error paths ...
test: Add run_engine_tests_json CLI glue
cli: Add evmone test subcommand ...
test: Add env-var-gated e2e fixture run ...
```

26 commits, every one of them landed atomically per the auto-memory's "commit each landed change separately" guidance for feature series.

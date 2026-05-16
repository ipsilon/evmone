# `evmone test` — Engine-format fixture runner

**Status:** design accepted, ready for implementation plan
**Date:** 2026-05-16

## Goal

Add a `test` subcommand to the existing `evmone` CLI that consumes a single
JSON fixture file in EEST's `blockchain_test_engine` format and runs it
against evmone's state machinery. The first iteration targets the Osaka
revision and the fixtures in `~/proj/fixtures/bal-711`.

The design favors the simplest implementation that fits the user's
long-term direction: a fixture representation that keeps transactions as
raw RLP bytes (rather than verbose decoded structs), which is the natural
seed for a future migration of `BlockchainTest` to the engine shape.

## Non-goals

- `blockchain_test_engine_x` (requires external pre-allocation lookup).
- `blockchain_test`, `state_test`, `transaction_test` formats.
- Hive integration / Engine API over JSON-RPC. The CLI consumes the
  fixture directly; it does not speak the Engine API.
- Reusing or refactoring `run_blockchain_tests` from
  `test/blockchaintest/blockchaintest_runner.cpp`. The engine runner is
  a separate, self-contained implementation. Consolidation can happen
  later when `BlockchainTest` migrates to `EngineTest`.

## Format choice

`blockchain_test_engine` is the format present in `bal-711` and the only
engine format the CLI supports.

- Self-contained: full `pre` state, `genesisBlockHeader`,
  `engineNewPayloads`, `postState`, `lastblockhash`.
- Each `engineNewPayloads[].params[0]` is a `newPayloadV4`-shaped
  `ExecutionPayload`; `params[1]` is blob versioned hashes; `params[2]`
  is `parentBeaconBlockRoot`; `params[3]` is execution requests.
  `validationError` flags a payload that the EL is expected to reject.
- The `blockchain_test_engine_x` variant (only in `bal-560`) replaces
  inline `pre` with a `preHash` referencing a hive pre-alloc group. That
  is an optimization for hive simulators that share pre-allocations
  across tests; it is not in scope.

Any other `_info.fixture-format` value (or missing `_info`) is rejected
by the loader with `UnsupportedTestFeature("unsupported fixture format:
X")`. There is no silent skipping.

## Architecture

```
test/utils/
  engine_test.hpp            ← public types + entry points
  engine_test_loader.cpp     ← JSON → EngineTest, format detection
  engine_test_runner.cpp     ← EngineTest → TestResult (no gtest, no I/O)

tools/evmone/
  main.cpp                   ← `test` subcommand (~12 lines added)
  CMakeLists.txt             ← link evmone::testutils when EVMONE_TESTING=ON

test/unittests/
  engine_test_loader_test.cpp  ← inline-JSON loader tests
  engine_test_runner_test.cpp  ← runner tests on programmatic structs
```

The new files become part of the existing `evmone::testutils` static
library, which already depends on `evmone::state` + nlohmann_json. No new
external dependencies.

### Layer separation

- **Loader** is pure JSON-to-struct. No VM, no state machinery. Testable
  from string literals.
- **Runner** takes a parsed `EngineTest` + `evmc::VM&` and returns
  `TestResult{ bool passed; std::string error; }`. No gtest, no I/O.
  Testable from programmatically constructed structs.
- **CLI glue** (`run_engine_tests_json`) reads JSON, calls loader and
  runner, prints PASS/FAIL lines, returns an exit code. The CLI binary's
  `main.cpp` only invokes this glue.

### Build wiring

- `tools/evmone/CMakeLists.txt` conditionally links `evmone::testutils`
  and defines `EVMONE_HAS_TEST_SUBCOMMAND` when `EVMONE_TESTING=ON`.
- `main.cpp` wraps the `test` subcommand registration in
  `#ifdef EVMONE_HAS_TEST_SUBCOMMAND`. Under
  `build/clang-local` (`EVMONE_TESTING=OFF`) the subcommand is absent
  and the binary stays slim. Under `build/debug` (`EVMONE_TESTING=ON`)
  it is present.
- `test/utils/CMakeLists.txt` lists the three new sources under
  `evmone.testutils`.

## Data model

```cpp
namespace evmone::test {

struct EnginePayload {
    state::BlockInfo block_info;          // built from params[0] + params[2]
    std::vector<bytes> transactions_rlp;  // raw RLP bytes, hex-decoded from JSON
    hash256 expected_block_hash;          // payload.blockHash, verified by runner

    // Expected outputs decoded from the payload for verification:
    hash256 expected_state_root;
    hash256 expected_receipts_root;
    state::BloomFilter expected_logs_bloom;
    int64_t expected_gas_used;

    // Engine-specific:
    std::optional<std::string> validation_error;     // EEST `validationError`
};

struct EngineTest {
    std::string name;
    std::string network;            // e.g. "Osaka"
    RevisionSchedule rev;
    BlobSchedule blob_schedule;

    TestState pre_state;            // from `pre`
    BlockHeader genesis;            // from `genesisBlockHeader`

    std::vector<EnginePayload> payloads;

    hash256 last_block_hash;        // expected canonical head
    TestState post_state;           // from `postState`
};

struct TestResult { bool passed; std::string error; };

std::vector<EngineTest> load_engine_tests(std::string_view json);  // throws

TestResult run_engine_test(const EngineTest& t, evmc::VM& vm);

// Top-level CLI glue. Reads JSON, runs every test, prints lines, returns
// the failure count (clamped to 255) as an exit code.
int run_engine_tests_json(std::string_view json, evmc::VM& vm, std::ostream& out);

}  // namespace evmone::test
```

### Notes on shape

- `transactions_rlp` is the **canonical on-disk representation**.
  Transactions are not RLP-decoded at load time; the runner decodes
  them at execution time using the cherry-picked
  `state::rlp_decode(bytes_view&, Transaction&)`. This both reflects the
  JSON faithfully and supports the long-term migration target.
- `block_info` is the existing `state::BlockInfo`. The state-transition
  primitives (`state::transition`, `state::system_call_block_start`, …)
  consume that, so the loader does the field mapping once.
- `newPayloadVersion`, `forkchoiceUpdatedVersion`, `_info` (besides
  `fixture-format`) are intentionally absent — they are Engine-API
  protocol metadata, not EL semantics.

## Loader

`engine_test_loader.cpp` parses JSON via nlohmann_json and reuses the
existing `from_json` helpers in `test/utils/`.

### Algorithm

1. Parse JSON top-level (a map of `"<test_id>": {...}`).
2. For each entry, read `_info.fixture-format`:
   - `"blockchain_test_engine"` → proceed.
   - Anything else (`"blockchain_test"`, `"state_test"`,
     `"blockchain_test_engine_x"`, missing, …) → throw
     `UnsupportedTestFeature("unsupported fixture format: <X>")`.
3. For each accepted entry, build an `EngineTest`:
   - `pre`, `postState` → existing `from_json<TestState>`.
   - `genesisBlockHeader` → existing `from_json<BlockHeader>`.
   - `network`, `config.blobSchedule` → existing `to_rev_schedule`,
     `from_json<BlobSchedule>`.
   - `lastblockhash` → `last_block_hash`.
   - `engineNewPayloads[]` → vector of `EnginePayload` via the mapping
     below.

### `params[0]` → `state::BlockInfo`

```
parentHash       → block_info.parent_hash
feeRecipient     → block_info.coinbase
prevRandao       → block_info.prev_randao
blockNumber      → block_info.number
gasLimit         → block_info.gas_limit
gasUsed          → block_info.gas_used
timestamp        → block_info.timestamp
extraData        → block_info.extra_data
baseFeePerGas    → block_info.base_fee
blobGasUsed      → block_info.blob_gas_used
excessBlobGas    → block_info.excess_blob_gas
withdrawals      → block_info.withdrawals (via existing from_json<Withdrawal>)
params[2]        → block_info.parent_beacon_block_root
```

`blob_base_fee` is computed from `excess_blob_gas` and the blob
schedule, exactly as `blockchaintest_loader.cpp` does.

`stateRoot`, `receiptsRoot`, `logsBloom`, `gasUsed`, `blockHash`
populate the `expected_*` fields on `EnginePayload`. The execution
requests in `params[3]` are not loaded as an expected value in this
first iteration — see the runner section for the rationale.

`transactions[]` are hex-decoded via `from_hex` into
`std::vector<bytes>`. **No RLP decode at load time.**

### Format detection semantics

Format is checked per top-level test entry. If any entry has a
non-engine format, the load fails for the whole file — this matches the
"error out" decision. Mixed-format files are not expected from EEST in
practice, but if we ever need a "skip non-engine entries" mode it can be
added behind a flag later.

## Runner

`engine_test_runner.cpp` provides
`TestResult run_engine_test(const EngineTest&, evmc::VM&)`. No gtest, no
I/O, no JSON.

### Per-test flow

1. Initialize: `current_state = pre_state`, `block_hashes = {{0,
   genesis.hash}}`.
2. For each payload `p` (in order):
   1. **Decode** each `transactions_rlp[i]` via
      `state::rlp_decode(bytes_view&, Transaction&)`.
   2. **Recover sender** for each decoded tx via
      `state::recover_sender`.
      - If any (1) or (2) fails:
        - With `p.validation_error.has_value()` → expected outcome, skip
          to next payload without advancing the chain.
        - Without it → `TestResult{false, "payload N: tx i RLP decode
          error: …"}` (or sender-recovery message).
   3. **Apply block** via a local `apply_block` modeled on
      `blockchaintest_runner.cpp:44-121`:
      `system_call_block_start` → loop `state::transition` per tx,
      accumulating gas/receipts/logs/blob-gas → if Prague+,
      `collect_deposit_requests` + `system_call_block_end` → `finalize`.
      Yields `{receipts, rejected, requests, gas_used, bloom,
      blob_gas_left, new_state}`.
   4. **Verify** (mirrors the assertions in
      `blockchaintest_runner.cpp:344-370`):
      - `mpt_hash(new_state) == p.expected_state_root`
      - `mpt_hash(receipts) == p.expected_receipts_root`
      - `compute_bloom_filter(receipts) == p.expected_logs_bloom`
      - `gas_used == p.expected_gas_used`
      - `rejected.empty() && blob_gas_left == 0`

      Engine payloads do not expose `transactionsTrie`, `requestsHash`,
      or the block-hash preimage as independent fields, so none of those
      are verified directly. Coverage:
      - The transactions trie is implicitly validated by the
        `parent_hash` chain (next payload's `parentHash` must match this
        payload's `blockHash`, which is derived from a header that
        embeds `transactionsTrie`).
      - Execution requests (Prague+) are implicitly validated through
        the state root — the system contracts for EIP-7002/7251/6110
        write to state, so divergent requests produce a divergent state
        root.
      - `p.expected_block_hash` is still consumed: it is the key the
        runner stores in `block_hashes` and the `parent_hash` the next
        payload must match. Recomputing the EL block hash from the
        assembled header is a follow-up, not part of this first
        iteration.
   5. **Resolve expectation**:
      - `validation_error` absent: any verification failure →
        `TestResult{false, "<one-line message>"}`. On success: advance
        `current_state = new_state`,
        `block_hashes[p.block_info.number] = p.expected_block_hash`.
      - `validation_error` present: a verification or RLP/sender failure
        is the expected outcome — continue without advancing. If
        everything verifies (block was accepted) → fail:
        `"payload N: expected validation error '<X>' but block was
        accepted"`.
3. **Final checks**:
   - `mpt_hash(current_state) == mpt_hash(post_state)`.
   - Last successfully-applied payload's hash equals `last_block_hash`.

### `apply_block` placement

A free function in `engine_test_runner.cpp`'s anonymous namespace. It is
a near-copy of the existing function. That duplication is accepted for
"simplest first" and is expected to disappear when `BlockchainTest` is
later migrated to the engine shape. No refactor of
`blockchaintest_runner.cpp` is in scope for this work.

### Error message convention

One short line, prefixed with `payload <N>:` or `final:`. The CLI prints
it indented under the failing test name.

### On pre-execution header validation

The existing `blockchaintest_runner.cpp` does pre-execution header
checks via `validate_block` (gas-limit deltas, base-fee formula, blob-gas
formula, …). The engine runner intentionally **does not** replicate
those in this first iteration. Invalid headers manifest as
verification mismatches (state root, gas used, …) after `apply_block`,
which is sufficient for the EEST `validationError` cases in `bal-711`.
Adding explicit pre-checks is a follow-up if a fixture is found that
relies on them.

## CLI integration

`tools/evmone/main.cpp` gains a single subcommand block:

```cpp
#ifdef EVMONE_HAS_TEST_SUBCOMMAND
std::string test_path;
auto& test_cmd = *app.add_subcommand("test",
    "Run an Engine-format blockchain test fixture")->fallthrough();
test_cmd.add_option("path", test_path, "Path to JSON fixture file")
    ->required()->check(CLI::ExistingFile);
#endif

// … after app.parse() and VM creation:
#ifdef EVMONE_HAS_TEST_SUBCOMMAND
if (test_cmd) {
    std::ifstream f{test_path};
    const std::string json((std::istreambuf_iterator<char>{f}), {});
    return evmone::test::run_engine_tests_json(json, vm, std::cout);
}
#endif
```

No other changes to `main.cpp`. Exceptions (from the loader or runner)
bubble up to the existing `catch (const std::exception&)` handler, which
prints `Error: <what>` and exits non-zero.

### Output format

```
$ evmone test path/to/blake2_precompile_delegatecall.json
PASS tests/istanbul/eip152_blake2/test_blake2_delegatecall.py::test_blake2_precompile_delegatecall[fork_Osaka-blockchain_test_engine_from_state_test]
1/1 passed
```

```
$ evmone test path/to/broken.json
FAIL tests/foo/bar.py::test_thing[...]
  payload 0: state root mismatch (got 0xabc..., expected 0xdef...)
0/1 passed
```

Exit code is the failure count (clamped to 255). 0 iff all PASS.

## fuzzing/main cherry-picks

The new code depends on RLP transaction decoding and signer recovery
from `origin/fuzzing/main`. The following 15 commits are cherry-picked
onto a fresh branch off `master`, in chronological order, **each as its
own commit** (matching the memory note that perf-opt series should not
batch commits — same principle for landed-feature series):

```
43e0162e rlp: add RLP decoding and Transaction RLP decode             ← base
92afadeb rlp: validate list boundary and reject trailing data in tx decode
6293b0a0 rlp: check list payload length does not exceed available data
38687cf0 rlp: validate payload bounds in pair and vector decoders
8b7a30b9 rlp: reject non-canonical integer encoding with leading zeros
43f595c7 rlp: enforce canonical length headers in decode_header
c433b411 rlp: reject EIP-1559/EIP-2930 tx with invalid y_parity
fe0b9640 rlp_decode: distinguish empty 'to' (CREATE) from 20-byte address
724f2cea rlp_decode: handle pre-EIP-155 legacy v ∈ {27, 28}
e1c886c6 rlp_decode: accept blob (type 3) and set_code (type 4) tx
f11aaeab state: add proper transaction and authorization signer recovery
c107406e state: recover_sender handles decoded legacy y_parity tx.v
b9a83f94 state: enforce tx.chain_id matches BlockInfo::chain_id
78310649 delegation: enforce exact 23-byte length for EIP-7702 designator
b845a22e state: don't reject depth==0 tx call when sender.nonce == NonceMax
```

Skipped: `a86ace9d` (pure TODO comment, no functional change).

These commits land **first**, on their own branch/PR, before the
engine-test code. The engine-test code is strictly additive on top.

Conflicts are resolved in-line with the original commit message
preserved. A squash-with-citation-list is acceptable only if a fix is
non-trivially intertwined with later commits; the default is a straight
cherry-pick.

## Testability

Three layers, each independently testable.

### Loader tests (`engine_test_loader_test.cpp`, gtest)

Inline JSON literals fed to `load_engine_tests`. Cases:

- Minimal valid `blockchain_test_engine` — verify all fields parsed
  (name, network, pre/post state, genesis header, one payload with txs
  as raw bytes, withdrawals, `validation_error` absent).
- Payload with `validationError` present — captured into
  `EnginePayload::validation_error`.
- Format rejection: `"blockchain_test"`, `"state_test"`,
  `"blockchain_test_engine_x"`, missing `_info` — each throws
  `UnsupportedTestFeature` with the format name in the message.
- Multiple tests in one file — both loaded.
- Malformed JSON / missing required fields — throws.

### Runner tests (`engine_test_runner_test.cpp`, gtest)

`EngineTest` constructed programmatically (no JSON) to isolate runner
logic. Cases:

- Simple value-transfer payload — passes.
- Payload with corrupted `expected_state_root` —
  `TestResult.passed == false`, error mentions "state root mismatch".
- Payload with `validation_error` set + an actually-invalid block —
  passes.
- Payload with `validation_error` set + a valid block — fails with
  "expected validation error '…' but block was accepted".
- Bad RLP in `transactions_rlp` without `validation_error` — fails with
  RLP decode error.
- Same, with `validation_error` set — passes.

### End-to-end test

One or two cases in `engine_test_runner_test.cpp` that read a small
fixture from `~/proj/fixtures/bal-711/blockchain_tests_engine/for_osaka/…`
through `load_engine_tests` → `run_engine_test`, expecting PASS. Gated
behind an env var (e.g. `EVMONE_FIXTURES_DIR`) so CI does not require
the fixture set on disk.

### Why this composes well

- Loader has no `evmc::VM`, no state machinery — tests run in
  milliseconds.
- Runner takes a struct + VM — no I/O, no JSON parsing in the test hot
  path.
- The CLI is a one-liner over `run_engine_tests_json`; `main.cpp` has
  effectively nothing worth unit-testing.

## Implementation order

1. PR 1 — cherry-pick the 15 fuzzing/main commits onto master.
2. PR 2 — add `engine_test.hpp` + `engine_test_loader.cpp` +
   `engine_test_loader_test.cpp`.
3. PR 3 — add `engine_test_runner.cpp` +
   `engine_test_runner_test.cpp`.
4. PR 4 — add `test` subcommand to `tools/evmone/main.cpp` and update
   `tools/evmone/CMakeLists.txt`.

Splitting steps 2–4 keeps each PR small and reviewable. Steps 2 and 3
both land library code with isolated tests; step 4 is purely
CLI/build wiring.

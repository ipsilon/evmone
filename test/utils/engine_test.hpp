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
#include <filesystem>
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

/// Run a single fixture file, or every `.json` (except `index.json`) under a directory.
/// Prints per-file headers and a final summary. Returns the failure count clamped to 255.
int run_engine_tests_path(
    const std::filesystem::path& path, evmc::VM& vm, std::ostream& out);

}  // namespace evmone::test

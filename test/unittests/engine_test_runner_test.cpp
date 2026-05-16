// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmc/evmc.hpp>
#include <evmone/evmone.h>
#include <gmock/gmock.h>
#include <test/utils/engine_test.hpp>
#include <test/utils/mpt_hash.hpp>
#include <test/utils/utils.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

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
    // Use Cancun (pre-Prague) to avoid the requirement that
    // Prague+ system contracts (withdrawal/consolidation) be pre-deployed.
    t.network = "Cancun";
    t.rev = to_rev_schedule(t.network);
    t.genesis.hash =
        0x0000000000000000000000000000000000000000000000000000000000000003_bytes32;
    t.genesis.gas_limit = 0x7270e00;  // match child so gas_limit window check passes
    t.genesis.gas_used = 0;
    t.genesis.base_fee_per_gas = 7;   // match child so base_fee check passes
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

TEST(engine_test_runner, validation_error_with_invalid_block_passes)
{
    auto t = make_test_with_bad_state_root();
    t.payloads[0].validation_error = "BlockException.INVALID_STATE_ROOT";
    // With validation_error set, the state-root mismatch is the expected
    // outcome → result is PASS. The accepted chain head stays at genesis,
    // so lastblockhash must equal genesis.hash.
    t.last_block_hash = t.genesis.hash;
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
    t.last_block_hash = t.genesis.hash;
    evmc::VM vm{evmc_create_evmone()};
    const auto result = run_engine_test(t, vm);
    EXPECT_TRUE(result.passed) << result.error;
}

TEST(engine_test_runner, lastblockhash_mismatch_caught_when_trailing_payload_invalid)
{
    auto t = make_test_with_bad_state_root();
    // Mark the last (and only) payload as expected-invalid → it does not
    // advance the chain head. The accepted chain head is therefore the genesis.
    t.payloads[0].validation_error = "BlockException.INVALID_STATE_ROOT";
    // ...but the test declares lastblockhash to be the rejected payload's
    // declared block hash. That should be caught as a mismatch.
    t.last_block_hash = t.payloads[0].expected_block_hash;

    evmc::VM vm{evmc_create_evmone()};
    const auto result = run_engine_test(t, vm);
    EXPECT_FALSE(result.passed);
    EXPECT_THAT(result.error, HasSubstr("lastblockhash"));
}

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

TEST(engine_test_runner, run_engine_tests_path_file_mode)
{
    // File-mode dispatch: a single fixture file should behave exactly like
    // run_engine_tests_json. Use the existing env-var-gated path so CI without
    // the fixture set stays green.
    const auto* dir = std::getenv("EVMONE_FIXTURES_DIR");
    if (dir == nullptr)
        GTEST_SKIP() << "EVMONE_FIXTURES_DIR not set";

    const std::filesystem::path path = std::filesystem::path{dir} /
        "blockchain_tests_engine/for_osaka/istanbul/eip152_blake2/blake2_delegatecall/"
        "blake2_precompile_delegatecall.json";
    if (!std::filesystem::exists(path))
        GTEST_SKIP() << "fixture not found: " << path;

    evmc::VM vm{evmc_create_evmone()};
    std::ostringstream out;
    const int rc = run_engine_tests_path(path, vm, out);
    EXPECT_EQ(rc, 0) << out.str();
    EXPECT_THAT(out.str(), HasSubstr("PASS "));
}

TEST(engine_test_runner, run_engine_tests_path_directory_mode)
{
    const auto* dir = std::getenv("EVMONE_FIXTURES_DIR");
    if (dir == nullptr)
        GTEST_SKIP() << "EVMONE_FIXTURES_DIR not set";

    // Use one of the smallest existing subdirectories so the test is reasonably fast.
    const std::filesystem::path subdir = std::filesystem::path{dir} /
        "blockchain_tests_engine/for_osaka/istanbul/eip152_blake2/blake2_delegatecall";
    if (!std::filesystem::is_directory(subdir))
        GTEST_SKIP() << "directory not found: " << subdir;

    evmc::VM vm{evmc_create_evmone()};
    std::ostringstream out;
    const int rc = run_engine_tests_path(subdir, vm, out);
    EXPECT_EQ(rc, 0) << out.str();
    // Output should include the SUMMARY line.
    EXPECT_THAT(out.str(), HasSubstr("SUMMARY:"));
    EXPECT_THAT(out.str(), HasSubstr("files passed"));
}

namespace
{
EngineTest make_test_with_bad_excess_blob_gas()
{
    EngineTest t;
    t.name = "bad_excess_blob_gas";
    t.network = "Osaka";
    t.rev = to_rev_schedule(t.network);
    t.genesis.hash =
        0x0000000000000000000000000000000000000000000000000000000000000003_bytes32;
    t.genesis.gas_limit = 0x7270e00;  // match child so gas_limit window check passes
    t.genesis.gas_used = 0;
    t.genesis.blob_gas_used = 0u;
    t.genesis.excess_blob_gas = 0u;
    t.genesis.base_fee_per_gas = 7;

    // Build an Osaka blob schedule so calc_excess_blob_gas resolves correctly.
    t.blob_schedule["Osaka"] = state::BlobParams{0x06, 0x09, 0x4c6964};

    EnginePayload p;
    p.block_info.number = 1;
    p.block_info.gas_limit = 0x7270e00;
    p.block_info.gas_used = 0;
    p.block_info.timestamp = 0x3e8;
    p.block_info.base_fee = 7;
    p.block_info.coinbase = 0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba_address;
    p.block_info.parent_hash = t.genesis.hash;
    p.block_info.blob_gas_used = 0u;
    p.block_info.excess_blob_gas = 0xdeadbeefu;  // bogus
    p.block_info.parent_beacon_block_root = bytes32{};
    p.expected_block_hash =
        0x0000000000000000000000000000000000000000000000000000000000000099_bytes32;
    p.expected_state_root = state::EMPTY_MPT_HASH;
    p.expected_receipts_root = state::EMPTY_MPT_HASH;
    p.expected_gas_used = 0;

    t.payloads.push_back(p);
    t.last_block_hash = t.genesis.hash;  // payload rejected → head stays at genesis
    return t;
}
}  // namespace

TEST(engine_test_runner, excess_blob_gas_mismatch_caught_as_header_error)
{
    // Without validation_error, this must FAIL with a descriptive header error.
    evmc::VM vm{evmc_create_evmone()};
    const auto result = run_engine_test(make_test_with_bad_excess_blob_gas(), vm);
    EXPECT_FALSE(result.passed);
    EXPECT_THAT(result.error, HasSubstr("excess_blob_gas mismatch"));
}

TEST(engine_test_runner, excess_blob_gas_mismatch_satisfies_validation_error)
{
    // Same payload as above, but with validation_error set → must PASS.
    auto t = make_test_with_bad_excess_blob_gas();
    t.payloads[0].validation_error = "BlockException.INCORRECT_EXCESS_BLOB_GAS";
    evmc::VM vm{evmc_create_evmone()};
    const auto result = run_engine_test(t, vm);
    EXPECT_TRUE(result.passed) << result.error;
}

namespace
{
EngineTest make_test_with_cancun_genesis()
{
    EngineTest t;
    t.network = "Cancun";
    t.rev = to_rev_schedule(t.network);
    t.genesis.hash =
        0x0000000000000000000000000000000000000000000000000000000000000003_bytes32;
    t.genesis.block_number = 0;
    t.genesis.gas_limit = 0x1c9c380;  // 30M
    t.genesis.gas_used = 0;
    t.genesis.base_fee_per_gas = 0x7;
    t.genesis.blob_gas_used = 0u;
    t.genesis.excess_blob_gas = 0u;
    t.blob_schedule["Cancun"] = state::BlobParams{0x03, 0x06, 0x32f0ed};
    t.last_block_hash = t.genesis.hash;
    return t;
}

EnginePayload make_minimal_child_payload(const BlockHeader& genesis)
{
    EnginePayload p;
    p.block_info.number = 1;
    // Match parent gas_limit exactly: in-window.
    p.block_info.gas_limit = genesis.gas_limit;
    p.block_info.gas_used = 0;
    p.block_info.timestamp = 0x3e8;
    p.block_info.base_fee = state::calc_base_fee(
        genesis.gas_limit, genesis.gas_used, genesis.base_fee_per_gas);
    p.block_info.coinbase = 0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba_address;
    p.block_info.parent_hash = genesis.hash;
    p.block_info.blob_gas_used = 0u;
    p.block_info.excess_blob_gas = 0u;
    p.block_info.parent_beacon_block_root = bytes32{};
    p.expected_block_hash =
        0x0000000000000000000000000000000000000000000000000000000000000099_bytes32;
    p.expected_state_root = state::EMPTY_MPT_HASH;
    p.expected_receipts_root = state::EMPTY_MPT_HASH;
    p.expected_gas_used = 0;
    return p;
}
}  // namespace

TEST(engine_test_runner, gas_limit_below_floor_caught)
{
    auto t = make_test_with_cancun_genesis();
    auto p = make_minimal_child_payload(t.genesis);
    p.block_info.gas_limit = 4999;  // below 5000 floor
    t.payloads.push_back(p);

    evmc::VM vm{evmc_create_evmone()};
    const auto result = run_engine_test(t, vm);
    EXPECT_FALSE(result.passed);
    EXPECT_THAT(result.error, HasSubstr("gas_limit below 5000 floor"));
}

TEST(engine_test_runner, gas_limit_out_of_window_caught)
{
    auto t = make_test_with_cancun_genesis();
    auto p = make_minimal_child_payload(t.genesis);
    // Parent gas_limit = 0x1c9c380 = 30,000,000. Window = ±29297.
    // Push way past the window:
    p.block_info.gas_limit = static_cast<int64_t>(
        static_cast<uint64_t>(t.genesis.gas_limit) + 1'000'000);
    // base_fee was computed from the original gas_limit; keep it consistent.
    t.payloads.push_back(p);

    evmc::VM vm{evmc_create_evmone()};
    const auto result = run_engine_test(t, vm);
    EXPECT_FALSE(result.passed);
    EXPECT_THAT(result.error, HasSubstr("gas_limit"));
}

TEST(engine_test_runner, base_fee_mismatch_caught)
{
    auto t = make_test_with_cancun_genesis();
    auto p = make_minimal_child_payload(t.genesis);
    p.block_info.base_fee = 0xdeadbeefu;  // wrong
    t.payloads.push_back(p);

    evmc::VM vm{evmc_create_evmone()};
    const auto result = run_engine_test(t, vm);
    EXPECT_FALSE(result.passed);
    EXPECT_THAT(result.error, HasSubstr("base_fee_per_gas mismatch"));
}

TEST(engine_test_runner, blob_gas_used_over_cap_caught)
{
    auto t = make_test_with_cancun_genesis();
    auto p = make_minimal_child_payload(t.genesis);
    // Cancun: target=3, max=6. max_blob_gas_per_block = max * 131072 = 786432.
    // Set blob_gas_used way past the cap:
    p.block_info.blob_gas_used = 0xffffffffu;
    t.payloads.push_back(p);

    evmc::VM vm{evmc_create_evmone()};
    const auto result = run_engine_test(t, vm);
    EXPECT_FALSE(result.passed);
    EXPECT_THAT(result.error, HasSubstr("blob_gas_used exceeds max_blob_gas_per_block"));
}

namespace
{
// Load a Prague fixture that exercises the requests pipeline end-to-end
// (system contracts pre-deployed, requests collection succeeds). Returns
// empty optional if the fixture set is unavailable.
std::optional<std::vector<EngineTest>> load_prague_e2e_fixture()
{
    const auto* dir = std::getenv("EVMONE_FIXTURES_DIR");
    if (dir == nullptr)
        return std::nullopt;
    const std::filesystem::path path = std::filesystem::path{dir} /
        "blockchain_tests_engine/for_osaka/prague/eip2537_bls_12_381_precompiles/"
        "bls12_variable_length_input_contracts/valid_gas_pairing.json";
    if (!std::filesystem::exists(path))
        return std::nullopt;
    std::ifstream f{path};
    const std::string json{
        std::istreambuf_iterator<char>{f}, std::istreambuf_iterator<char>{}};
    return load_engine_tests(json);
}
}  // namespace

TEST(engine_test_runner, requests_count_mismatch_caught)
{
    // Use a real Prague fixture so the Prague system contracts (withdrawal,
    // consolidation) are pre-deployed in pre_state and apply_block actually
    // produces a `requests` result. Tamper with expected_requests to force a
    // count mismatch. Without validation_error → FAIL with "requests count
    // mismatch".
    auto opt_tests = load_prague_e2e_fixture();
    if (!opt_tests.has_value())
        GTEST_SKIP() << "EVMONE_FIXTURES_DIR not set or fixture missing";
    ASSERT_FALSE(opt_tests->empty());
    auto& t = (*opt_tests)[0];
    ASSERT_FALSE(t.payloads.empty());
    // Deliberate mismatch: claim one expected request, but the block
    // produces zero (this fixture has empty params[3]).
    t.payloads[0].expected_requests.push_back(*evmc::from_hex("0x00deadbeef"));

    evmc::VM vm{evmc_create_evmone()};
    const auto result = run_engine_test(t, vm);
    EXPECT_FALSE(result.passed);
    EXPECT_THAT(result.error, HasSubstr("requests count mismatch"));
}

TEST(engine_test_runner, requests_mismatch_satisfies_validation_error)
{
    // Same tampering as above, but flag the payload as expected-invalid →
    // the mismatch becomes the expected outcome and the test must PASS.
    auto opt_tests = load_prague_e2e_fixture();
    if (!opt_tests.has_value())
        GTEST_SKIP() << "EVMONE_FIXTURES_DIR not set or fixture missing";
    ASSERT_FALSE(opt_tests->empty());
    auto& t = (*opt_tests)[0];
    ASSERT_FALSE(t.payloads.empty());
    t.payloads[0].expected_requests.push_back(*evmc::from_hex("0x00deadbeef"));
    t.payloads[0].validation_error = "BlockException.INVALID_REQUESTS";
    // The (now-rejected) payload no longer advances the chain head, so the
    // accepted chain head is the genesis and the final state equals pre_state.
    t.last_block_hash = t.genesis.hash;
    t.post_state = t.pre_state;

    evmc::VM vm{evmc_create_evmone()};
    const auto result = run_engine_test(t, vm);
    EXPECT_TRUE(result.passed) << result.error;
}

TEST(engine_test_runner, block_hash_mismatch_caught_when_payload_lies)
{
    // Use the e2e fixture but corrupt the expected_block_hash so the
    // recomputed hash won't match. The runner must surface a "block hash
    // mismatch" error (i.e. the canonical EL header check catches that
    // the declared blockHash diverges from the hash implied by the
    // reconstructed header).
    const auto* dir = std::getenv("EVMONE_FIXTURES_DIR");
    if (dir == nullptr)
        GTEST_SKIP() << "EVMONE_FIXTURES_DIR not set";

    const std::filesystem::path path = std::filesystem::path{dir} /
        "blockchain_tests_engine/for_osaka/istanbul/eip152_blake2/blake2_delegatecall/"
        "blake2_precompile_delegatecall.json";
    if (!std::filesystem::exists(path))
        GTEST_SKIP() << "fixture not found: " << path;

    std::ifstream f{path};
    const std::string json{
        std::istreambuf_iterator<char>{f}, std::istreambuf_iterator<char>{}};

    auto tests = load_engine_tests(json);
    ASSERT_EQ(tests.size(), 1u);
    ASSERT_FALSE(tests[0].payloads.empty());
    tests[0].payloads[0].expected_block_hash = bytes32{};  // corrupt

    evmc::VM vm{evmc_create_evmone()};
    const auto result = run_engine_test(tests[0], vm);
    EXPECT_FALSE(result.passed);
    EXPECT_THAT(result.error, HasSubstr("block hash mismatch"));
}

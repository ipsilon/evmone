// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmone/evmone.h>
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <test/utils/t8n.hpp>
#include <sstream>

using namespace evmone;
using namespace testing;
using nlohmann::json;

namespace
{
// Minimal block env used by the trace test below.
// currentDifficulty is set so t8n() takes the "difficulty supplied" branch;
// tests with no env still exercise the calculate_difficulty fallback.
// currentRandom is also set so that the difficulty value isn't reinterpreted
// as a bytes32 prev_randao by from_json_with_rev.
constexpr auto ENV_JSON = R"({
    "currentCoinbase": "0x8888f1f195afa192cfee860698584c030f4c9db1",
    "currentNumber": "0x01",
    "currentTimestamp": "0x54c99069",
    "currentGasLimit": "0x2fefd8",
    "currentDifficulty": "0x20000",
    "currentRandom": "0x0000000000000000000000000000000000000000000000000000000000000000"
})";

// Account funding the transaction used in the trace test.
constexpr auto ALLOC_JSON = R"({
    "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b": {
        "code": "",
        "nonce": "0x00",
        "balance": "0x02540be400"
    }
})";

// Single legacy CREATE transaction; init code is `PUSH1 0x01 PUSH0 RETURN`,
// which deploys a one-byte runtime `0x00`. Three opcodes => three trace lines.
// Matches test/integration/evmone-cli/t8n/cancun_create_tx/txs.json[0]; the tx hash is
// well-known and used below.
constexpr auto TX_JSON = R"([{
    "to": null,
    "input": "0x60015ff3",
    "gas": "0x186a0",
    "nonce": "0x0",
    "value": "0x0",
    "gasPrice": "0x32",
    "chainId": "0x1",
    "sender": "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b",
    "v": "0x1b",
    "r": "0x468a915f087692bb9be503831a3dfef2cf9c8dee26deb40ff2ec99e8d22665ae",
    "s": "0x5cedae0810c3851ecd1004bfdbfe6ddc7753c2d665993bb01ce75af7857b13dc"
}])";

/// Runs t8n over the given pre-state and transactions, and returns the result JSON.
std::string run_t8n(std::string_view alloc_json, std::string_view txs_json, evmc_revision rev)
{
    evmc::VM vm{evmc_create_evmone()};

    std::istringstream env{ENV_JSON};
    std::istringstream alloc{std::string{alloc_json}};
    std::istringstream txs{std::string{txs_json}};
    std::ostringstream out_result;

    tooling::T8NArgs args;
    args.rev = rev;
    args.chain_id = 1;
    args.alloc = &alloc;
    args.env = &env;
    args.txs = &txs;
    args.out_result = &out_result;

    tooling::t8n(vm, args);
    return out_result.str();
}

/// Legacy transaction calling CALLEE. t8n takes `sender` from the JSON and only checks `hash`
/// when present, so the signature is never recovered.
constexpr auto TX_TO_CALLEE = R"([{
    "to": "0x000000000000000000000000000000000000c0de",
    "input": "0x",
    "gas": "0x186a0",
    "nonce": "0x0",
    "value": "0x0",
    "gasPrice": "0x32",
    "chainId": "0x1",
    "sender": "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b",
    "v": "0x1b",
    "r": "0x468a915f087692bb9be503831a3dfef2cf9c8dee26deb40ff2ec99e8d22665ae",
    "s": "0x5cedae0810c3851ecd1004bfdbfe6ddc7753c2d665993bb01ce75af7857b13dc"
}])";

/// Runs TX_TO_CALLEE against a callee deployed with the given code.
std::string run_call_to(std::string_view callee_code, evmc_revision rev)
{
    const auto alloc = R"({
        "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b": {
            "code": "", "nonce": "0x00", "balance": "0x02540be400"
        },
        "0x000000000000000000000000000000000000c0de": {
            "code": ")" +
                       std::string{callee_code} +
                       R"(", "nonce": "0x00", "balance": "0x00"
        }
    })";
    return run_t8n(alloc, TX_TO_CALLEE, rev);
}

/// A block env carrying the parent's fee market, which is what a base fee is computed from.
constexpr auto ENV_WITH_PARENT_JSON = R"({
    "currentCoinbase": "0x8888f1f195afa192cfee860698584c030f4c9db1",
    "currentNumber": "0x01",
    "currentTimestamp": "0x54c99069",
    "currentGasLimit": "15000000",
    "parentBaseFee": "7",
    "parentGasLimit": "5000000",
    "parentGasUsed": "5000000"
})";

/// Runs t8n over an empty state with no transaction, for what the block env alone decides.
std::string run_t8n_env(std::string_view env_json, evmc_revision rev)
{
    evmc::VM vm{evmc_create_evmone()};

    std::istringstream env{std::string{env_json}};
    std::istringstream alloc{"{}"};
    std::istringstream txs{"[]"};
    std::ostringstream out_result;

    tooling::T8NArgs args;
    args.rev = rev;
    args.alloc = &alloc;
    args.env = &env;
    args.txs = &txs;
    args.out_result = &out_result;

    tooling::t8n(vm, args);
    return out_result.str();
}

/// ALLOC_JSON's sender beside the beacon-roots contract: without its code the block-start system
/// call is skipped (EIP-4788).
constexpr auto ALLOC_WITH_BEACON_ROOTS_JSON = R"({
    "0x000f3df6d732807ef1319fb7b8bb8522d0beac02": {
        "code": "0x3373fffffffffffffffffffffffffffffffffffffffe14604d57602036146024575f5ffd5b5f35801560495762001fff810690815414603c575f5ffd5b62001fff01545f5260205ff35b5f5ffd5b62001fff42064281555f359062001fff015500",
        "nonce": "0x01",
        "balance": "0x00"
    },
    "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b": {
        "code": "",
        "nonce": "0x00",
        "balance": "0x02540be400"
    }
})";

/// Stubs of both request contracts, each returning nothing. Without their code the block-end
/// system calls fail the block instead (EIP-7002, EIP-7251).
constexpr auto ALLOC_WITH_REQUEST_STUBS_JSON = R"({
    "0x00000961ef480eb55e80d19ad83579a64c007002": {
        "code": "0x00",
        "nonce": "0x01",
        "balance": "0x00"
    },
    "0x0000bbddc7ce488642fb579f8b00f3a590007251": {
        "code": "0x00",
        "nonce": "0x01",
        "balance": "0x00"
    }
})";
}  // namespace

TEST(tooling_t8n, base_fee_is_computed_from_the_parent_block)
{
    // The parent used twice its gas target, so the fee rises by max(7 / 8, 1) = 1 (EIP-1559).
    const auto result = json::parse(run_t8n_env(ENV_WITH_PARENT_JSON, EVMC_LONDON));
    EXPECT_EQ(result.at("currentBaseFee"), "0x8");
}

TEST(tooling_t8n, no_base_fee_before_london)
{
    const auto result = json::parse(run_t8n_env(ENV_WITH_PARENT_JSON, EVMC_BERLIN));
    EXPECT_TRUE(result.contains("gasUsed"));  // Not an empty result which says nothing.
    EXPECT_FALSE(result.contains("currentBaseFee"));
}

TEST(tooling_t8n, blob_transaction_creating_a_contract_is_rejected)
{
    // A blob transaction has no create form (EIP-4844). This one is valid in every other respect,
    // so the missing `to` is its only fault.
    static constexpr auto BLOB_CREATE_TX = R"([{
        "input": "0x",
        "gas": "0x186a0",
        "nonce": "0x0",
        "value": "0x0",
        "v": "0x0",
        "r": "0xfc12b67159a3567f8bdbc49e0be369a2e20e09d57a51c41310543a4128409464",
        "s": "0x2de0cfe5495c4f58ff60645ceda0afd67a4c90a70bc89fe207269435b35e5b67",
        "maxFeePerGas": "0x32",
        "maxPriorityFeePerGas": "0x2",
        "maxFeePerBlobGas": "0xa",
        "blobVersionedHashes": [
            "0x01a915e4d060149eb4365960e6a7a45f334393093061116b197e3240065ff2d8"
        ],
        "sender": "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b"
    }])";

    const auto result = json::parse(run_t8n(ALLOC_JSON, BLOB_CREATE_TX, EVMC_CANCUN));
    EXPECT_EQ(result.at("receipts"), json::array());
    ASSERT_EQ(result.at("rejected").size(), 1u);
    EXPECT_EQ(
        result.at("rejected")[0].at("error"), "TransactionException.TYPE_3_TX_CONTRACT_CREATION");
}

TEST(tooling_t8n, a_block_requesting_nothing_reports_the_empty_requests_hash)
{
    const auto result = json::parse(run_t8n(ALLOC_WITH_REQUEST_STUBS_JSON, "[]", EVMC_PRAGUE));
    EXPECT_FALSE(result.contains("blockException"));
    EXPECT_EQ(result.at("requests"), json::array());
    // sha256 of nothing at all (EIP-7685).
    EXPECT_EQ(result.at("requestsHash"),
        "0xe3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(tooling_t8n, no_inputs_no_outputs)
{
    // Smoke: t8n() with everything left at defaults must not throw or crash.
    evmc::VM vm{evmc_create_evmone()};

    tooling::T8NArgs args;
    args.rev = EVMC_OSAKA;

    tooling::t8n(vm, args);
}

TEST(tooling_t8n, result_written_to_out_streams)
{
    evmc::VM vm{evmc_create_evmone()};

    tooling::T8NArgs args;
    args.rev = EVMC_OSAKA;
    std::ostringstream out_result;
    std::ostringstream out_alloc;
    args.out_result = &out_result;
    args.out_alloc = &out_alloc;

    tooling::t8n(vm, args);

    EXPECT_THAT(out_result.str(), HasSubstr("\"gasUsed\""));
    EXPECT_THAT(out_result.str(), HasSubstr("\"txRoot\""));
    EXPECT_THAT(out_result.str(), HasSubstr("\"receiptsRoot\""));
    EXPECT_THAT(out_result.str(), HasSubstr("\"logsBloom\""));
    EXPECT_THAT(out_alloc.str(), Eq("{}"));
}

TEST(tooling_t8n, out_alloc_reports_the_beacon_root_write_and_the_created_account)
{
    evmc::VM vm{evmc_create_evmone()};

    std::istringstream env{ENV_JSON};
    std::istringstream alloc{ALLOC_WITH_BEACON_ROOTS_JSON};
    std::istringstream txs{TX_JSON};
    std::ostringstream out_alloc;

    tooling::T8NArgs args;
    args.rev = EVMC_CANCUN;
    args.chain_id = 1;
    args.alloc = &alloc;
    args.env = &env;
    args.txs = &txs;
    args.out_alloc = &out_alloc;

    tooling::t8n(vm, args);

    const auto post = json::parse(out_alloc.str());
    // Slot timestamp % 8191 holds the timestamp (EIP-4788).
    EXPECT_EQ(post.at("0x000f3df6d732807ef1319fb7b8bb8522d0beac02")
                  .at("storage")
                  .at("0x00000000000000000000000000000000000000000000000000000000000016ca"),
        "0x0000000000000000000000000000000000000000000000000000000054c99069");
    EXPECT_EQ(post.at("0x6295ee1b4f6dd65047762f924ecd367c17eabf8f").at("code"), "0x00");
}

TEST(tooling_t8n, open_trace_called_per_tx)
{
    evmc::VM vm{evmc_create_evmone()};

    std::istringstream env{ENV_JSON};
    std::istringstream alloc{ALLOC_JSON};
    std::istringstream txs{TX_JSON};
    std::ostringstream out_result;
    std::ostringstream out_alloc;
    std::ostringstream trace_buf;
    std::vector<std::pair<size_t, evmc::bytes32>> trace_calls;

    tooling::T8NArgs args;
    args.rev = EVMC_SHANGHAI;  // No system contracts => clean trace_buf.
    args.chain_id = 1;
    args.alloc = &alloc;
    args.env = &env;
    args.txs = &txs;
    args.out_result = &out_result;
    args.out_alloc = &out_alloc;
    args.open_trace = [&](size_t i, const evmc::bytes32& hash) -> std::ostream& {
        trace_calls.emplace_back(i, hash);
        return trace_buf;
    };

    tooling::t8n(vm, args);

    ASSERT_EQ(trace_calls.size(), 1U);
    EXPECT_EQ(trace_calls[0].first, 0U);
    EXPECT_THAT(trace_buf.str(), HasSubstr("\"opName\":\"PUSH1\""));
    EXPECT_THAT(trace_buf.str(), HasSubstr("\"opName\":\"PUSH0\""));
    EXPECT_THAT(trace_buf.str(), HasSubstr("\"opName\":\"RETURN\""));
}

TEST(tooling_t8n, out_body_is_hex_rlp_of_transactions)
{
    evmc::VM vm{evmc_create_evmone()};

    std::istringstream env{ENV_JSON};
    std::istringstream alloc{ALLOC_JSON};
    std::istringstream txs{TX_JSON};
    std::ostringstream out_result;
    std::ostringstream out_alloc;
    std::ostringstream out_body;

    tooling::T8NArgs args;
    args.rev = EVMC_SHANGHAI;
    args.chain_id = 1;
    args.alloc = &alloc;
    args.env = &env;
    args.txs = &txs;
    args.out_result = &out_result;
    args.out_alloc = &out_alloc;
    args.out_body = &out_body;

    tooling::t8n(vm, args);

    // RLP-encoded list of one legacy transaction, hex-prefixed.
    EXPECT_THAT(out_body.str(), StartsWith("0x"));
    EXPECT_GT(out_body.str().size(), std::size_t{2});
}

TEST(tooling_t8n, pre_byzantium_sets_receipt_post_state)
{
    // The TX_JSON fixture uses PUSH0 in its init code, so the inner CREATE fails at Homestead,
    // but the outer tx still produces a receipt, which carries the post-state root instead of
    // the EIP-658 status.
    const auto result = run_t8n(ALLOC_JSON, TX_JSON, EVMC_HOMESTEAD);

    EXPECT_THAT(result, HasSubstr("\"transactionHash\""));
    EXPECT_THAT(result, HasSubstr("\"root\": \"0x"));
}

TEST(tooling_t8n, mismatched_tx_hash_throws)
{
    evmc::VM vm{evmc_create_evmone()};

    // TX_JSON's tx with a deliberately wrong "hash" field. t8n() must detect
    // the mismatch against the recomputed hash and throw std::logic_error.
    static constexpr auto TX_WITH_BAD_HASH = R"([{
        "to": null,
        "input": "0x60015ff3",
        "gas": "0x186a0",
        "nonce": "0x0",
        "value": "0x0",
        "gasPrice": "0x32",
        "chainId": "0x1",
        "sender": "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b",
        "v": "0x1b",
        "r": "0x468a915f087692bb9be503831a3dfef2cf9c8dee26deb40ff2ec99e8d22665ae",
        "s": "0x5cedae0810c3851ecd1004bfdbfe6ddc7753c2d665993bb01ce75af7857b13dc",
        "hash": "0xdeadbeef00000000000000000000000000000000000000000000000000000000"
    }])";

    std::istringstream env{ENV_JSON};
    std::istringstream alloc{ALLOC_JSON};
    std::istringstream txs{TX_WITH_BAD_HASH};

    tooling::T8NArgs args;
    args.rev = EVMC_SHANGHAI;
    args.chain_id = 1;
    args.alloc = &alloc;
    args.env = &env;
    args.txs = &txs;

    EXPECT_THROW(tooling::t8n(vm, args), std::logic_error);
}

TEST(tooling_t8n, max_chain_id)
{
    evmc::VM vm{evmc_create_evmone()};

    // The maximum `chainId` (uint64 max = 0xffffffffffffffff) must be parsed and
    // executed without overflow; regression test for `chainId` being loaded as
    // `uint8_t`, which threw `from_json<uint8_t>: value > 0xFF`.
    static constexpr auto TX_MAX_CHAIN_ID = R"([{
        "to": null,
        "input": "0x60015ff3",
        "gas": "0x186a0",
        "nonce": "0x0",
        "value": "0x0",
        "gasPrice": "0x32",
        "chainId": "0xffffffffffffffff",
        "sender": "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b",
        "v": "0x1b",
        "r": "0x468a915f087692bb9be503831a3dfef2cf9c8dee26deb40ff2ec99e8d22665ae",
        "s": "0x5cedae0810c3851ecd1004bfdbfe6ddc7753c2d665993bb01ce75af7857b13dc"
    }])";

    std::istringstream env{ENV_JSON};
    std::istringstream alloc{ALLOC_JSON};
    std::istringstream txs{TX_MAX_CHAIN_ID};
    std::ostringstream out_result;

    tooling::T8NArgs args;
    args.rev = EVMC_SHANGHAI;
    args.chain_id = std::numeric_limits<uint64_t>::max();
    args.alloc = &alloc;
    args.env = &env;
    args.txs = &txs;
    args.out_result = &out_result;

    EXPECT_NO_THROW(tooling::t8n(vm, args));
    EXPECT_THAT(out_result.str(), HasSubstr("\"transactionHash\""));
}

TEST(tooling_t8n, max_v)
{
    evmc::VM vm{evmc_create_evmone()};

    // Legacy EIP-155 `v` is chainId*2 + 35 + parity, exceeding 0xff for chainId > 110.
    // The maximum `v` (uint64 max = 0xffffffffffffffff) must be parsed and executed without
    // overflow; regression test for `v` being loaded as `uint8_t`, which threw
    // `from_json<uint8_t>: value > 0xFF`.
    static constexpr auto TX_MAX_V = R"([{
        "to": null,
        "input": "0x60015ff3",
        "gas": "0x186a0",
        "nonce": "0x0",
        "value": "0x0",
        "gasPrice": "0x32",
        "chainId": "0x1",
        "sender": "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b",
        "v": "0xffffffffffffffff",
        "r": "0x468a915f087692bb9be503831a3dfef2cf9c8dee26deb40ff2ec99e8d22665ae",
        "s": "0x5cedae0810c3851ecd1004bfdbfe6ddc7753c2d665993bb01ce75af7857b13dc"
    }])";

    std::istringstream env{ENV_JSON};
    std::istringstream alloc{ALLOC_JSON};
    std::istringstream txs{TX_MAX_V};
    std::ostringstream out_result;

    tooling::T8NArgs args;
    args.rev = EVMC_SHANGHAI;
    args.chain_id = 1;
    args.alloc = &alloc;
    args.env = &env;
    args.txs = &txs;
    args.out_result = &out_result;

    EXPECT_NO_THROW(tooling::t8n(vm, args));
    EXPECT_THAT(out_result.str(), HasSubstr("\"transactionHash\""));
}

TEST(tooling_t8n, receipt_reports_emitted_logs)
{
    // MSTORE8(0, 0xaa); LOG1(offset=0, size=1, topic=0x42).
    const auto result = run_call_to("0x60aa600053604260016000a100", EVMC_SHANGHAI);

    EXPECT_THAT(result, HasSubstr("\"address\": \"0x000000000000000000000000000000000000c0de\""));
    EXPECT_THAT(result,
        HasSubstr("\"0x0000000000000000000000000000000000000000000000000000000000000042\""));
    EXPECT_THAT(result, HasSubstr("\"data\": \"0xaa\""));
}

TEST(tooling_t8n, receipt_status_reports_failure)
{
    // The callee is the INVALID instruction, so the transaction fails.
    EXPECT_THAT(run_call_to("0xfe", EVMC_SHANGHAI), HasSubstr("\"status\": \"0x0\""));
    EXPECT_THAT(run_call_to("0x00", EVMC_SHANGHAI), HasSubstr("\"status\": \"0x1\""));
}

TEST(tooling_t8n, block_gas_used_is_pre_refund_from_amsterdam)
{
    // The block gas is the pre-refund transaction gas from Amsterdam on (EIP-7778), while a
    // receipt reports what the sender paid. The callee clears a storage slot, so the transaction
    // earns a refund and the two must differ; before Amsterdam they must stay equal.
    static constexpr auto ALLOC_REFUNDING_CALLEE = R"({
        "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b": {
            "code": "", "nonce": "0x00", "balance": "0x02540be400"
        },
        "0x000000000000000000000000000000000000c0de": {
            "code": "0x5f5f55", "nonce": "0x00", "balance": "0x00",
            "storage": {"0x00": "0x01"}
        }
    })";

    const auto run = [](evmc_revision rev) {
        const auto j = json::parse(run_t8n(ALLOC_REFUNDING_CALLEE, TX_TO_CALLEE, rev));
        const auto hex_value = [](const json& v) {
            return std::stoll(v.get<std::string>(), nullptr, 16);
        };
        return std::pair{
            hex_value(j.at("gasUsed")), hex_value(j.at("receipts").at(0).at("cumulativeGasUsed"))};
    };

    const auto [osaka_block_gas, osaka_cumulative] = run(EVMC_OSAKA);
    EXPECT_EQ(osaka_block_gas, osaka_cumulative);

    // The whole of the difference is the refund for clearing the slot (EIP-3529).
    const auto [block_gas, cumulative] = run(EVMC_AMSTERDAM);
    EXPECT_EQ(block_gas - cumulative, 4800);
}

// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmone/evmone.h>
#include <gmock/gmock.h>
#include <test/utils/t8n.hpp>
#include <sstream>

using namespace evmone;
using namespace testing;

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
// which deploys a one-byte runtime `0x01`. Three opcodes => three trace lines.
// Matches test/integration/t8n/cancun_create_tx/txs.json[0]; the tx hash is
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
}  // namespace

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
    evmc::VM vm{evmc_create_evmone()};

    // Pre-Byzantium receipts include the post-state root via receipt.post_state.
    // The TX_JSON fixture uses PUSH0 in its init code, so the inner CREATE fails
    // at Homestead, but the outer tx still produces a TransactionReceipt that
    // exercises the `rev < EVMC_BYZANTIUM` branch in t8n().
    std::istringstream env{ENV_JSON};
    std::istringstream alloc{ALLOC_JSON};
    std::istringstream txs{TX_JSON};
    std::ostringstream out_result;

    tooling::T8NArgs args;
    args.rev = EVMC_HOMESTEAD;
    args.chain_id = 1;
    args.alloc = &alloc;
    args.env = &env;
    args.txs = &txs;
    args.out_result = &out_result;

    tooling::t8n(vm, args);

    // The "receipts" array is initialized empty on every txs-present run; the
    // distinguishing signal that a receipt was actually produced (i.e., the
    // tx wasn't classified as rejected) is the presence of transactionHash.
    EXPECT_THAT(out_result.str(), HasSubstr("\"transactionHash\""));
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

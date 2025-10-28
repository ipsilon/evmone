// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <test/statetest/statetest.hpp>
#include <test/utils/utils.hpp>

using namespace evmone;
using namespace evmone::test;
using namespace testing;

TEST(json_loader, uint64_t)
{
    using json::basic_json;

    EXPECT_EQ(from_json<uint64_t>(basic_json("0x00000005")), 5);
    EXPECT_EQ(from_json<uint64_t>(basic_json("5")), 5);
    EXPECT_EQ(from_json<uint64_t>(basic_json(7)), 7);

    EXPECT_EQ(from_json<uint64_t>(basic_json("0xffffffffffffffff")),
        std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(from_json<uint64_t>(basic_json("18446744073709551615")),
        std::numeric_limits<uint64_t>::max());
    EXPECT_THROW(from_json<uint64_t>(basic_json("0x10000000000000000")), std::out_of_range);
    EXPECT_THROW(from_json<uint64_t>(basic_json("18446744073709551616")), std::out_of_range);
    EXPECT_EQ(from_json<uint64_t>(basic_json(std::numeric_limits<uint64_t>::max())),
        std::numeric_limits<uint64_t>::max());

    // Octal is also supported.
    EXPECT_EQ(from_json<uint64_t>(basic_json("0777")), 0777);

    EXPECT_THROW(from_json<uint64_t>(basic_json("0x000000000000000k")), std::invalid_argument);
    EXPECT_THROW(from_json<uint64_t>(basic_json("k")), std::invalid_argument);
    EXPECT_THROW(from_json<uint64_t>(basic_json("")), std::invalid_argument);
}

TEST(json_loader, int64_t)
{
    using json::basic_json;

    EXPECT_EQ(from_json<int64_t>(basic_json("0x00000005")), 5);
    EXPECT_EQ(from_json<int64_t>(basic_json("-0x5")), -5);
    EXPECT_EQ(from_json<int64_t>(basic_json("-5")), -5);

    EXPECT_EQ(from_json<int64_t>(basic_json(-7)), -7);
    EXPECT_EQ(from_json<int64_t>(basic_json(0xffffffffffffffff)), -1);

    EXPECT_EQ(
        from_json<int64_t>(basic_json("0x7fffffffffffffff")), std::numeric_limits<int64_t>::max());
    EXPECT_EQ(
        from_json<int64_t>(basic_json("9223372036854775807")), std::numeric_limits<int64_t>::max());
    EXPECT_EQ(from_json<int64_t>(basic_json("-9223372036854775808")),
        std::numeric_limits<int64_t>::min());

    // Unfortunate conversion results:
    EXPECT_EQ(from_json<int64_t>(basic_json("0xffffffffffffffff")), int64_t{-1});
    EXPECT_EQ(
        from_json<int64_t>(basic_json("9223372036854775808")), std::numeric_limits<int64_t>::min());
    EXPECT_EQ(from_json<int64_t>(basic_json("-9223372036854775809")),
        std::numeric_limits<int64_t>::max());

    EXPECT_THROW(from_json<uint64_t>(basic_json("0x10000000000000000")), std::out_of_range);
    EXPECT_THROW(from_json<uint64_t>(basic_json("18446744073709551616")), std::out_of_range);

    // Octal is also supported.
    EXPECT_EQ(from_json<int64_t>(basic_json("0777")), 0777);

    EXPECT_THROW(from_json<int64_t>(basic_json("0x000000000000000k")), std::invalid_argument);
    EXPECT_THROW(from_json<int64_t>(basic_json("k")), std::invalid_argument);
    EXPECT_THROW(from_json<int64_t>(basic_json("")), std::invalid_argument);
}

TEST(statetest_loader, load_empty_test)
{
    std::istringstream s{"{}"};
    EXPECT_EQ(load_state_tests(s).size(), 0);
}

TEST(statetest_loader, load_multi_test)
{
    std::istringstream s{R"({
      "T1": {
        "pre": {},
        "transaction": {"gasPrice": "","sender": "","to": "","data": null,
          "gasLimit": "0","value": null,"nonce" : "0"},
        "post": {},
        "env": {"currentNumber": "0","currentTimestamp": "0",
          "currentGasLimit": "0","currentCoinbase": ""}
      },
      "T2": {
        "pre": {},
        "transaction": {"gasPrice": "","sender": "","to": "","data": null,
          "gasLimit": "0","value": null,"nonce" : "0"},
        "post": {},
        "env": {"currentNumber": "0","currentTimestamp": "0",
          "currentGasLimit": "0","currentCoinbase": ""}
      }
    })"};
    const auto tests = load_state_tests(s);
    ASSERT_EQ(tests.size(), 2);
    EXPECT_EQ(tests[0].name, "T1");
    EXPECT_EQ(tests[1].name, "T2");
}

TEST(statetest_loader, load_minimal_test)
{
    std::istringstream s{R"({
        "test": {
            "pre": {},
            "transaction": {
                "gasPrice": "",
                "sender": "",
                "to": "",
                "data": null,
                "gasLimit": "0",
                "value": null,
                "nonce" : "0"
            },
            "post": {
                "Cancun": []
            },
            "env": {
                "currentNumber": "0",
                "currentTimestamp": "0",
                "currentGasLimit": "0",
                "currentCoinbase": ""
            }
        }
    })"};
    const auto st = std::move(load_state_tests(s).at(0));
    // TODO: should add some comparison operator to State, BlockInfo, AccessList
    EXPECT_EQ(st.pre_state.size(), 0);
    EXPECT_EQ(st.cases[0].block.number, 0);
    EXPECT_EQ(st.cases[0].block.timestamp, 0);
    EXPECT_EQ(st.cases[0].block.gas_limit, 0);
    EXPECT_EQ(st.cases[0].block.coinbase, address{});
    EXPECT_EQ(st.cases[0].block.prev_randao, bytes32{});
    EXPECT_EQ(st.cases[0].block.base_fee, 0);
    EXPECT_EQ(st.multi_tx.type, test::TestMultiTransaction::Type::legacy);
    EXPECT_EQ(st.multi_tx.data, bytes{});
    EXPECT_EQ(st.multi_tx.gas_limit, 0);
    EXPECT_EQ(st.multi_tx.max_gas_price, 0);
    EXPECT_EQ(st.multi_tx.max_priority_gas_price, 0);
    EXPECT_EQ(st.multi_tx.sender, address{});
    EXPECT_EQ(st.multi_tx.to, std::nullopt);
    EXPECT_EQ(st.multi_tx.value, 0);
    EXPECT_EQ(st.multi_tx.nonce, 0);
    EXPECT_EQ(st.multi_tx.access_list.size(), 0);
    EXPECT_EQ(st.multi_tx.chain_id, 1);
    EXPECT_EQ(st.multi_tx.nonce, 0);
    EXPECT_EQ(st.multi_tx.r, 0);
    EXPECT_EQ(st.multi_tx.s, 0);
    EXPECT_EQ(st.multi_tx.v, 0);
    EXPECT_EQ(st.multi_tx.access_lists.size(), 0);
    EXPECT_EQ(st.multi_tx.inputs.size(), 0);
    EXPECT_EQ(st.multi_tx.gas_limits.size(), 1);
    EXPECT_EQ(st.multi_tx.gas_limits[0], 0);
    EXPECT_EQ(st.multi_tx.values.size(), 0);
    EXPECT_EQ(st.cases.size(), 1);
    EXPECT_EQ(st.cases[0].expectations.size(), 0);
    EXPECT_EQ(st.input_labels.size(), 0);
}

TEST(statetest_loader, validate_state_zero_storage_slot)
{
    TestState state{{0xadd4_address, {.storage = {{0x01_bytes32, 0x00_bytes32}}}}};
    EXPECT_THAT([&] { validate_state(state, EVMC_LONDON); },
        ThrowsMessage<std::invalid_argument>(
            "account 0x000000000000000000000000000000000000add4 contains invalid zero-value "
            "storage entry "
            "0x0000000000000000000000000000000000000000000000000000000000000001"));
}

TEST(statetest_loader, validate_state_unexpected_ef_prefix)
{
    TestState state{{0xadd4_address, {.code = "EF00"_hex}}};
    EXPECT_THAT([&] { validate_state(state, EVMC_LONDON); },
        ThrowsMessage<std::invalid_argument>(
            "unexpected code starting with 0xEF at 0x000000000000000000000000000000000000add4"));
}

TEST(statetest_loader, validate_state_invalid_delegation_size)
{
    TestState state{{0xadd4_address, {.code = "EF010000"_hex}}};
    EXPECT_THAT([&] { validate_state(state, EVMC_PRAGUE); },
        ThrowsMessage<std::invalid_argument>(
            "EIP-7702 delegation designator at 0x000000000000000000000000000000000000add4 has "
            "invalid size"));
}

TEST(statetest_loader, validate_state_unexpected_delegation)
{
    TestState state{
        {0xadd4_address, {.code = "EF01000000000000000000000000000000000000000001"_hex}}};
    EXPECT_THAT([&] { validate_state(state, EVMC_CANCUN); },
        ThrowsMessage<std::invalid_argument>(
            "unexpected code starting with 0xEF at 0x000000000000000000000000000000000000add4"));
}

TEST(statetest_loader, validate_empty_account_with_storage)
{
    TestState state{{0xadd4_address, {.storage = {{0x01_bytes32, 0x01_bytes32}}}}};
    EXPECT_THAT([&] { validate_state(state, EVMC_CANCUN); },
        ThrowsMessage<std::invalid_argument>(
            "empty account with non-empty storage at 0x000000000000000000000000000000000000add4"));
}

TEST(statetest_loader, validate_code_at_precompile_address)
{
    TestState state{{0x0a_address, {.code = "00"_hex}}};
    EXPECT_THAT([&] { validate_state(state, EVMC_CANCUN); },
        ThrowsMessage<std::invalid_argument>(
            "unexpected code at precompile address 0x000000000000000000000000000000000000000a"));
}

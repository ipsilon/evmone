// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <test/utils/mpt_hash.hpp>
#include <test/utils/statetest.hpp>

using namespace evmone;
using namespace evmone::state;
using namespace evmone::test;

TEST(statetest_withdrawals, withdrawals_root_hash)
{
    // Input taken from https://etherscan.io/block/17826409
    constexpr std::string_view input =
        R"([{"address":"0xb9d7934878b5fb9610b3fe8a5e441e8fad7e293f","amount":"0xe162d9","index":"0xc13ad8","validatorIndex":"0xa2f00"},{"address":"0xb9d7934878b5fb9610b3fe8a5e441e8fad7e293f","amount":"0xe1c5c2","index":"0xc13ad9","validatorIndex":"0xa2f01"},{"address":"0xb9d7934878b5fb9610b3fe8a5e441e8fad7e293f","amount":"0xe14f28","index":"0xc13ada","validatorIndex":"0xa2f02"},{"address":"0xb9d7934878b5fb9610b3fe8a5e441e8fad7e293f","amount":"0xe190f2","index":"0xc13adb","validatorIndex":"0xa2f03"},{"address":"0xb9d7934878b5fb9610b3fe8a5e441e8fad7e293f","amount":"0xe1e59c","index":"0xc13adc","validatorIndex":"0xa2f04"},{"address":"0xb9d7934878b5fb9610b3fe8a5e441e8fad7e293f","amount":"0xe1bbfe","index":"0xc13add","validatorIndex":"0xa2f05"},{"address":"0xb9d7934878b5fb9610b3fe8a5e441e8fad7e293f","amount":"0xe20974","index":"0xc13ade","validatorIndex":"0xa2f06"},{"address":"0xb9d7934878b5fb9610b3fe8a5e441e8fad7e293f","amount":"0xe145b7","index":"0xc13adf","validatorIndex":"0xa2f07"},{"address":"0xb9d7934878b5fb9610b3fe8a5e441e8fad7e293f","amount":"0xe11e5d","index":"0xc13ae0","validatorIndex":"0xa2f08"},{"address":"0xb9d7934878b5fb9610b3fe8a5e441e8fad7e293f","amount":"0xe221e0","index":"0xc13ae1","validatorIndex":"0xa2f09"},{"address":"0xb9d7934878b5fb9610b3fe8a5e441e8fad7e293f","amount":"0xe2061a","index":"0xc13ae2","validatorIndex":"0xa2f0a"},{"address":"0xb9d7934878b5fb9610b3fe8a5e441e8fad7e293f","amount":"0xe23d22","index":"0xc13ae3","validatorIndex":"0xa2f0b"},{"address":"0xb9d7934878b5fb9610b3fe8a5e441e8fad7e293f","amount":"0xe1ab3a","index":"0xc13ae4","validatorIndex":"0xa2f0c"},{"address":"0xb9d7934878b5fb9610b3fe8a5e441e8fad7e293f","amount":"0xe19535","index":"0xc13ae5","validatorIndex":"0xa2f0d"},{"address":"0xb9d7934878b5fb9610b3fe8a5e441e8fad7e293f","amount":"0x317c537","index":"0xc13ae6","validatorIndex":"0xa2f0e"},{"address":"0xb9d7934878b5fb9610b3fe8a5e441e8fad7e293f","amount":"0xe1965f","index":"0xc13ae7","validatorIndex":"0xa2f0f"}])";

    const auto j = json::json::parse(input);

    std::vector<Withdrawal> withdrawals;
    for (const auto& withdrawal : j)
        withdrawals.push_back(from_json<Withdrawal>(withdrawal));

    EXPECT_EQ(mpt_hash(withdrawals),
        0x38cd9ae992a22b94a1582e7d0691dbef56a90cdb36bf7b11d98373f80b102c8f_bytes32);
}

TEST(statetest_withdrawals, withdrawals_warmup_test_case)
{
    // Input taken from
    // https://github.com/ethereum/tests/blob/develop/BlockchainTests/InvalidBlocks/bc4895-withdrawals/warmup.json
    constexpr std::string_view input =
        R"([
{
    "index" : "0x0",
    "validatorIndex" : "0x0",
    "amount" : "0x186a0",
    "address" : "0xc000000000000000000000000000000000000001"
},
{
    "index" : "0x1",
    "validatorIndex" : "0x0",
    "amount" : "0x186a0",
    "address" : "0xc000000000000000000000000000000000000002"
},
{
    "index" : "0x2",
    "validatorIndex" : "0x0",
    "amount" : "0x186a0",
    "address" : "0xc000000000000000000000000000000000000003"
},
{
    "index" : "0x3",
    "validatorIndex" : "0x0",
    "amount" : "0x186a0",
    "address" : "0xc000000000000000000000000000000000000004"
},
{
    "index" : "0x4",
    "validatorIndex" : "0x0",
    "amount" : "0x186a0",
    "address" : "0xc000000000000000000000000000000000000005"
},
{
    "index" : "0x5",
    "validatorIndex" : "0x0",
    "amount" : "0x186a0",
    "address" : "0xc000000000000000000000000000000000000006"
},
{
    "index" : "0x6",
    "validatorIndex" : "0x0",
    "amount" : "0x186a0",
    "address" : "0xc000000000000000000000000000000000000007"
},
{
    "index" : "0x7",
    "validatorIndex" : "0x0",
    "amount" : "0x186a0",
    "address" : "0xc000000000000000000000000000000000000008"
},
{
    "index" : "0x8",
    "validatorIndex" : "0x0",
    "amount" : "0x186a0",
    "address" : "0xc000000000000000000000000000000000000009"
},
{
    "index" : "0x9",
    "validatorIndex" : "0x0",
    "amount" : "0x186a0",
    "address" : "0xc000000000000000000000000000000000000010"
}
])";
    const auto j = json::json::parse(input);

    std::vector<Withdrawal> withdrawals;
    for (const auto& withdrawal : j)
        withdrawals.push_back(from_json<Withdrawal>(withdrawal));

    EXPECT_EQ(mpt_hash(withdrawals),
        0xaa45c53e9f7d6a8362f80876029915da00b1441ef39eb9bbb74f98465ff433ad_bytes32);
}

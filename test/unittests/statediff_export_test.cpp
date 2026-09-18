// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <test/utils/statetest.hpp>

using namespace evmone;
using namespace evmone::state;
using namespace evmc::literals;

TEST(statediff_export, state_diff_empty)
{
    const StateDiff diff;
    EXPECT_EQ(test::to_json(diff).dump(), R"({"deletedAccounts":[],"modifiedAccounts":{}})");
}

TEST(statediff_export, state_diff_modified_account_with_code_and_storage)
{
    StateDiff diff;
    diff.modified_accounts.push_back({.addr = 0x01_address,
        .nonce = 1,
        .balance = 2,
        .code = bytes{0xfe},
        .modified_storage = {{0x03_bytes32, 0x04_bytes32}}});

    const auto j = test::to_json(diff);
    const auto& j_acc = j["modifiedAccounts"]["0x0000000000000000000000000000000000000001"];
    EXPECT_EQ(j_acc["nonce"], "0x1");
    EXPECT_EQ(j_acc["balance"], "0x2");
    EXPECT_EQ(j_acc["code"], "0xfe");
    EXPECT_EQ(j_acc["modifiedStorage"]
                   ["0x0000000000000000000000000000000000000000000000000000000000000003"],
        "0x0000000000000000000000000000000000000000000000000000000000000004");
    EXPECT_EQ(j["deletedAccounts"], json::json::array());
}

TEST(statediff_export, state_diff_modified_account_without_code)
{
    StateDiff diff;
    diff.modified_accounts.push_back({.addr = 0x02_address, .nonce = 0, .balance = 0});

    const auto j = test::to_json(diff);
    EXPECT_FALSE(
        j["modifiedAccounts"]["0x0000000000000000000000000000000000000002"].contains("code"));
}

TEST(statediff_export, state_diff_deleted_account)
{
    StateDiff diff;
    diff.deleted_accounts.push_back(0x03_address);

    const auto j = test::to_json(diff);
    EXPECT_EQ(j["modifiedAccounts"], json::json::object());
    ASSERT_EQ(j["deletedAccounts"].size(), 1);
    EXPECT_EQ(j["deletedAccounts"][0], "0x0000000000000000000000000000000000000003");
}

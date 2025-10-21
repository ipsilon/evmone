// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <test/state/requests.hpp>

using namespace evmc::literals;
using namespace evmone::state;

TEST(state_deposit_requests, collect_invalid_deposit_requests)
{
    std::array receipts{
        TransactionReceipt{.logs = {Log{.addr = DEPOSIT_CONTRACT_ADDRESS,
                               .topics = {DEPOSIT_EVENT_SIGNATURE_HASH}}}},
    };

    auto& log_data = receipts[0].logs[0].data;
    log_data = bytes(576, 0xfe);  // fill expected length with 0xfe
    ASSERT_EQ(collect_deposit_requests(receipts), std::nullopt);
}
TEST(state_deposit_requests, collect_deposit_requests)
{
    std::array receipts{
        TransactionReceipt{.logs = {Log{.addr = DEPOSIT_CONTRACT_ADDRESS,
                               .topics = {DEPOSIT_EVENT_SIGNATURE_HASH}}}},
    };

    auto& log_data = receipts[0].logs[0].data;
    log_data = bytes(576, 0x00);  // fill expected length with 0x00
    // Encode offsets (bytes) as 5 consecutive 32-byte big-endian words starting at word index 0.
    // log_data was filled with zeros above, so only set the non-zero trailing bytes.
    // 160  = 0x00...00A0
    log_data.replace(0 * 32 + 31, 1, 1, char8_t(0xA0));
    // 256  = 0x00...0100
    log_data.replace(1 * 32 + 30, 1, 1, char8_t(0x01));
    log_data.replace(1 * 32 + 31, 1, 1, char8_t(0x00));
    // 320  = 0x00...0140
    log_data.replace(2 * 32 + 30, 1, 1, char8_t(0x01));
    log_data.replace(2 * 32 + 31, 1, 1, char8_t(0x40));
    // 384  = 0x00...0180
    log_data.replace(3 * 32 + 30, 1, 1, char8_t(0x01));
    log_data.replace(3 * 32 + 31, 1, 1, char8_t(0x80));
    // 512  = 0x00...0200
    log_data.replace(4 * 32 + 30, 1, 1, char8_t(0x02));
    log_data.replace(4 * 32 + 31, 1, 1, char8_t(0x00));

    // Encode lengths and data for each field at the expected offsets.
    // Pubkey (48 bytes) at offset 160
    log_data.replace(5 * 32 + 31, 1, 1, char8_t(0x30));  // length = 48
    // Withdrawal credentials (32 bytes) at offset 256
    log_data.replace(8 * 32 + 31, 1, 1, char8_t(0x20));  // length = 32
    // Amount (8 bytes) at offset 320
    log_data.replace(10 * 32 + 31, 1, 1, char8_t(0x08));  // length = 8
    // Signature (96 bytes) at offset 384
    log_data.replace(12 * 32 + 31, 1, 1, char8_t(0x60));  // length = 96
    // Index (8 bytes) at offset 512
    log_data.replace(16 * 32 + 31, 1, 1, char8_t(0x08));  // length = 8

    log_data.replace(6 * 32, 48, 48, 0x01);   // pubkey
    log_data.replace(9 * 32, 32, 32, 0x02);   // withdrawal_credentials
    log_data.replace(11 * 32, 8, 8, 0x03);    // amount
    log_data.replace(13 * 32, 96, 96, 0x04);  // signature
    log_data.replace(17 * 32, 8, 8, 0x05);    // index

    const auto requests = collect_deposit_requests(receipts).value();
    EXPECT_EQ(requests.type(), Requests::Type::deposit);
    EXPECT_EQ(requests.data(),
        bytes(48, 0x01) + bytes(32, 0x02) + bytes(8, 0x03) + bytes(96, 0x04) + bytes(8, 0x05));
}

TEST(state_deposit_requests, collect_deposit_requests_skips_wrong_topic)
{
    constexpr auto DUMMPY_EVENT_SIGNATURE_HASH = 0xdeadbeef_bytes32;
    const std::array receipts{
        TransactionReceipt{.logs = {Log{.addr = DEPOSIT_CONTRACT_ADDRESS,
                               .data = {0x01, 0x02, 0x03},
                               .topics = {DUMMPY_EVENT_SIGNATURE_HASH}}}},
    };

    const auto requests = collect_deposit_requests(receipts).value();
    EXPECT_EQ(requests.type(), Requests::Type::deposit);
    EXPECT_TRUE(requests.data().empty());
}

// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

/// This file contains EVM unit tests that access or modify information
/// about accounts, without storage.

#include "evm_fixture.hpp"
#include <evmone/instructions_traits.hpp>

using namespace evmc::literals;
using namespace evmone::test;

TEST_P(evm, code)
{
    // CODESIZE 2 0 CODECOPY RETURN(0,9)
    const auto s = bytecode{"38600260003960096000f3"};
    execute(s);
    EXPECT_EQ(gas_used, 23);
    ASSERT_EQ(result.output_size, 9);
    EXPECT_EQ(bytes_view(&result.output_data[0], 9), bytes_view(&s[2], 9));
}

TEST_P(evm, codecopy_combinations)
{
    // The CODECOPY arguments are provided in calldata: first byte is index, second byte is size.
    // The whole copied code is returned.
    const auto code = dup1(byte(calldataload(0), 1)) + byte(calldataload(0), 0) + push(0) +
                      OP_CODECOPY + ret(0, {});
    EXPECT_EQ(code.size(), 0x13);

    execute(code, "0013"_hex);
    EXPECT_EQ(output, code);

    execute(code, "0012"_hex);
    EXPECT_EQ(output, code.substr(0, 0x12));

    execute(code, "0014"_hex);
    EXPECT_EQ(output, code + "00");

    execute(code, "1300"_hex);
    EXPECT_EQ(output, bytes_view{});

    execute(code, "1400"_hex);
    EXPECT_EQ(output, bytes_view{});

    execute(code, "1200"_hex);
    EXPECT_EQ(output, bytes_view{});

    execute(code, "1301"_hex);
    EXPECT_EQ(output, "00"_hex);

    execute(code, "1401"_hex);
    EXPECT_EQ(output, "00"_hex);

    execute(code, "1201"_hex);
    EXPECT_EQ(output, code.substr(0x12, 1));
}

TEST_P(evm, tx_context)
{
    rev = EVMC_ISTANBUL;

    host.tx_context.block_timestamp = 0xdd;
    host.tx_context.block_number = 0x1100;
    host.tx_context.block_gas_limit = 0x990000;
    host.tx_context.chain_id.bytes[28] = 0xaa;
    host.tx_context.block_coinbase.bytes[1] = 0xcc;
    host.tx_context.tx_origin.bytes[2] = 0x55;
    host.tx_context.block_prev_randao.bytes[1] = 0xdd;
    host.tx_context.tx_gas_price.bytes[2] = 0x66;

    auto const code = bytecode{} + OP_TIMESTAMP + OP_COINBASE + OP_OR + OP_GASPRICE + OP_OR +
                      OP_NUMBER + OP_OR + OP_PREVRANDAO + OP_OR + OP_GASLIMIT + OP_OR + OP_ORIGIN +
                      OP_OR + OP_CHAINID + OP_OR + ret_top();
    execute(52, code);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    EXPECT_EQ(result.gas_left, 0);
    ASSERT_EQ(result.output_size, 32);
    EXPECT_EQ(result.output_data[31], 0xdd);
    EXPECT_EQ(result.output_data[30], 0x11);
    EXPECT_EQ(result.output_data[29], 0x99);
    EXPECT_EQ(result.output_data[28], 0xaa);
    EXPECT_EQ(result.output_data[14], 0x55);
    EXPECT_EQ(result.output_data[13], 0xcc);
    EXPECT_EQ(result.output_data[2], 0x66);
    EXPECT_EQ(result.output_data[1], 0xdd);
}

TEST_P(evm, balance)
{
    host.accounts[msg.recipient].set_balance(0x0504030201);
    auto code = bytecode{} + OP_ADDRESS + OP_BALANCE + mstore(0) + ret(32 - 6, 6);
    execute(417, code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 417);
    ASSERT_EQ(result.output_size, 6);
    EXPECT_EQ(result.output_data[0], 0);
    EXPECT_EQ(result.output_data[1], 0x05);
    EXPECT_EQ(result.output_data[2], 0x04);
    EXPECT_EQ(result.output_data[3], 0x03);
    EXPECT_EQ(result.output_data[4], 0x02);
    EXPECT_EQ(result.output_data[5], 0x01);
}

TEST_P(evm, account_info_homestead)
{
    rev = EVMC_HOMESTEAD;
    host.accounts[msg.recipient].set_balance(1);
    host.accounts[msg.recipient].code = bytes{1};

    execute(bytecode{} + OP_ADDRESS + OP_BALANCE + ret_top());
    EXPECT_GAS_USED(EVMC_SUCCESS, 37);
    EXPECT_OUTPUT_INT(1);

    execute(bytecode{} + OP_ADDRESS + OP_EXTCODESIZE + ret_top());
    EXPECT_GAS_USED(EVMC_SUCCESS, 37);
    EXPECT_OUTPUT_INT(1);

    execute(bytecode{} + push(1) + push(0) + push(0) + OP_ADDRESS + OP_EXTCODECOPY + ret(0, 1));
    EXPECT_GAS_USED(EVMC_SUCCESS, 43);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 1);
}

TEST_P(evm, selfbalance)
{
    host.accounts[msg.recipient].set_balance(0x0504030201);
    // NOTE: adding push here to balance out the stack pre-Istanbul (needed to get undefined
    // instruction as a result)
    auto code = bytecode{} + push(1) + OP_SELFBALANCE + mstore(0) + ret(32 - 6, 6);

    rev = EVMC_PETERSBURG;
    execute(code);
    EXPECT_EQ(result.status_code, EVMC_UNDEFINED_INSTRUCTION);

    rev = EVMC_ISTANBUL;
    execute(code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 23);
    ASSERT_EQ(result.output_size, 6);
    EXPECT_EQ(result.output_data[0], 0);
    EXPECT_EQ(result.output_data[1], 0x05);
    EXPECT_EQ(result.output_data[2], 0x04);
    EXPECT_EQ(result.output_data[3], 0x03);
    EXPECT_EQ(result.output_data[4], 0x02);
    EXPECT_EQ(result.output_data[5], 0x01);
}

TEST_P(evm, log)
{
    for (auto op : {OP_LOG0, OP_LOG1, OP_LOG2, OP_LOG3, OP_LOG4})
    {
        const uint8_t n = op - OP_LOG0;
        const auto code =
            push(1) + push(2) + push(3) + push(4) + mstore8(2, 0x77) + push(2) + push(2) + op;
        host.recorded_logs.clear();
        execute(code);
        EXPECT_GAS_USED(EVMC_SUCCESS, 421 + n * 375);
        ASSERT_EQ(host.recorded_logs.size(), 1);
        const auto& last_log = host.recorded_logs.back();
        ASSERT_EQ(last_log.data.size(), 2);
        EXPECT_EQ(last_log.data[0], 0x77);
        EXPECT_EQ(last_log.data[1], 0);
        ASSERT_EQ(last_log.topics.size(), n);
        for (size_t i = 0; i < n; ++i)
        {
            EXPECT_EQ(last_log.topics[i].bytes[31], 4 - i);
        }
    }
}

TEST_P(evm, log0_empty)
{
    auto code = push(0) + OP_DUP1 + OP_LOG0;
    execute(code);
    ASSERT_EQ(host.recorded_logs.size(), 1);
    const auto& last_log = host.recorded_logs.back();
    EXPECT_EQ(last_log.topics.size(), 0);
    EXPECT_EQ(last_log.data.size(), 0);
}

TEST_P(evm, log_data_cost)
{
    for (auto op : {OP_LOG0, OP_LOG1, OP_LOG2, OP_LOG3, OP_LOG4})
    {
        auto num_topics = op - OP_LOG0;
        auto code = push(0) + (4 * OP_DUP1) + push(1) + push(0) + op;
        auto cost = 407 + num_topics * 375;
        EXPECT_EQ(host.recorded_logs.size(), 0);
        execute(cost, code);
        EXPECT_EQ(result.status_code, EVMC_SUCCESS);
        EXPECT_EQ(host.recorded_logs.size(), 1);
        host.recorded_logs.clear();

        EXPECT_EQ(host.recorded_logs.size(), 0);
        execute(cost - 1, code);
        EXPECT_EQ(result.status_code, EVMC_OUT_OF_GAS);
        EXPECT_EQ(host.recorded_logs.size(), 0) << evmone::instr::traits[op].name;
        host.recorded_logs.clear();
    }
}

TEST_P(evm, selfdestruct)
{
    msg.recipient = 0x01_address;
    const auto& selfdestructs = host.recorded_selfdestructs[msg.recipient];

    rev = EVMC_SPURIOUS_DRAGON;
    execute(selfdestruct(0x09));
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    EXPECT_EQ(gas_used, 5003);
    ASSERT_EQ(selfdestructs.size(), 1);
    EXPECT_EQ(selfdestructs.back(), 0x09_address);

    rev = EVMC_HOMESTEAD;
    execute(selfdestruct(0x07));
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    EXPECT_EQ(gas_used, 3);
    ASSERT_EQ(selfdestructs.size(), 2);
    EXPECT_EQ(selfdestructs.back(), 0x07_address);

    rev = EVMC_TANGERINE_WHISTLE;
    execute(selfdestruct(0x08));
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    EXPECT_EQ(gas_used, 30003);
    ASSERT_EQ(selfdestructs.size(), 3);
    EXPECT_EQ(selfdestructs.back(), 0x08_address);
}

TEST_P(evm, selfdestruct_with_balance)
{
    constexpr auto beneficiary = 0xbe_address;
    const auto code = selfdestruct(beneficiary);
    msg.recipient = 0x5e_address;


    host.accounts[msg.recipient].set_balance(0);

    rev = EVMC_HOMESTEAD;
    execute(3, code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 3);
    EXPECT_EQ(result.gas_left, 0);
    ASSERT_EQ(host.recorded_account_accesses.size(), 1);
    EXPECT_EQ(host.recorded_account_accesses[0], msg.recipient);  // Selfdestruct.
    host.recorded_account_accesses.clear();

    rev = EVMC_TANGERINE_WHISTLE;
    execute(30003, code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 30003);
    ASSERT_EQ(host.recorded_account_accesses.size(), 2);
    EXPECT_EQ(host.recorded_account_accesses[0], beneficiary);    // Exists?
    EXPECT_EQ(host.recorded_account_accesses[1], msg.recipient);  // Selfdestruct.
    host.recorded_account_accesses.clear();

    execute(30002, code);
    EXPECT_GAS_USED(EVMC_OUT_OF_GAS, 30002);
    EXPECT_EQ(result.gas_left, 0);
    ASSERT_EQ(host.recorded_account_accesses.size(), 1);
    EXPECT_EQ(host.recorded_account_accesses[0], beneficiary);  // Exists?
    host.recorded_account_accesses.clear();

    rev = EVMC_SPURIOUS_DRAGON;
    execute(5003, code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 5003);
    ASSERT_EQ(host.recorded_account_accesses.size(), 2);
    EXPECT_EQ(host.recorded_account_accesses[0], msg.recipient);  // Balance.
    EXPECT_EQ(host.recorded_account_accesses[1], msg.recipient);  // Selfdestruct.
    host.recorded_account_accesses.clear();

    execute(5002, code);
    EXPECT_GAS_USED(EVMC_OUT_OF_GAS, 5002);
    EXPECT_EQ(result.gas_left, 0);
    EXPECT_EQ(host.recorded_account_accesses.size(), 0);
    host.recorded_account_accesses.clear();


    host.accounts[msg.recipient].set_balance(1);

    rev = EVMC_HOMESTEAD;
    execute(3, code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 3);
    EXPECT_EQ(result.gas_left, 0);
    ASSERT_EQ(host.recorded_account_accesses.size(), 1);
    EXPECT_EQ(host.recorded_account_accesses[0], msg.recipient);  // Selfdestruct.
    host.recorded_account_accesses.clear();

    rev = EVMC_TANGERINE_WHISTLE;
    execute(30003, code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 30003);
    ASSERT_EQ(host.recorded_account_accesses.size(), 2);
    EXPECT_EQ(host.recorded_account_accesses[0], beneficiary);    // Exists?
    EXPECT_EQ(host.recorded_account_accesses[1], msg.recipient);  // Selfdestruct.
    host.recorded_account_accesses.clear();

    execute(30002, code);
    EXPECT_GAS_USED(EVMC_OUT_OF_GAS, 30002);
    EXPECT_EQ(result.gas_left, 0);
    ASSERT_EQ(host.recorded_account_accesses.size(), 1);
    EXPECT_EQ(host.recorded_account_accesses[0], beneficiary);  // Exists?
    host.recorded_account_accesses.clear();

    rev = EVMC_SPURIOUS_DRAGON;
    execute(30003, code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 30003);
    ASSERT_EQ(host.recorded_account_accesses.size(), 3);
    EXPECT_EQ(host.recorded_account_accesses[0], msg.recipient);  // Balance.
    EXPECT_EQ(host.recorded_account_accesses[1], beneficiary);    // Exists?
    EXPECT_EQ(host.recorded_account_accesses[2], msg.recipient);  // Selfdestruct.
    host.recorded_account_accesses.clear();

    execute(30002, code);
    EXPECT_GAS_USED(EVMC_OUT_OF_GAS, 30002);
    EXPECT_EQ(result.gas_left, 0);
    ASSERT_EQ(host.recorded_account_accesses.size(), 2);
    EXPECT_EQ(host.recorded_account_accesses[0], msg.recipient);  // Balance.
    EXPECT_EQ(host.recorded_account_accesses[1], beneficiary);    // Exists?
    host.recorded_account_accesses.clear();


    host.accounts[beneficiary] = {};  // Beneficiary exists.


    host.accounts[msg.recipient].set_balance(0);

    rev = EVMC_HOMESTEAD;
    execute(3, code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 3);
    EXPECT_EQ(result.gas_left, 0);
    ASSERT_EQ(host.recorded_account_accesses.size(), 1);
    EXPECT_EQ(host.recorded_account_accesses[0], msg.recipient);  // Selfdestruct.
    host.recorded_account_accesses.clear();

    rev = EVMC_TANGERINE_WHISTLE;
    execute(5003, code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 5003);
    ASSERT_EQ(host.recorded_account_accesses.size(), 2);
    EXPECT_EQ(host.recorded_account_accesses[0], beneficiary);    // Exists?
    EXPECT_EQ(host.recorded_account_accesses[1], msg.recipient);  // Selfdestruct.
    host.recorded_account_accesses.clear();

    execute(5002, code);
    EXPECT_GAS_USED(EVMC_OUT_OF_GAS, 5002);
    EXPECT_EQ(result.gas_left, 0);
    EXPECT_EQ(host.recorded_account_accesses.size(), 0);
    host.recorded_account_accesses.clear();

    rev = EVMC_SPURIOUS_DRAGON;
    execute(5003, code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 5003);
    ASSERT_EQ(host.recorded_account_accesses.size(), 2);
    EXPECT_EQ(host.recorded_account_accesses[0], msg.recipient);  // Balance.
    EXPECT_EQ(host.recorded_account_accesses[1], msg.recipient);  // Selfdestruct.
    host.recorded_account_accesses.clear();

    execute(5002, code);
    EXPECT_GAS_USED(EVMC_OUT_OF_GAS, 5002);
    EXPECT_EQ(result.gas_left, 0);
    EXPECT_EQ(host.recorded_account_accesses.size(), 0);
    host.recorded_account_accesses.clear();


    host.accounts[msg.recipient].set_balance(1);

    rev = EVMC_HOMESTEAD;
    execute(3, code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 3);
    EXPECT_EQ(result.gas_left, 0);
    ASSERT_EQ(host.recorded_account_accesses.size(), 1);
    EXPECT_EQ(host.recorded_account_accesses[0], msg.recipient);  // Selfdestruct.
    host.recorded_account_accesses.clear();

    rev = EVMC_TANGERINE_WHISTLE;
    execute(5003, code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 5003);
    ASSERT_EQ(host.recorded_account_accesses.size(), 2);
    EXPECT_EQ(host.recorded_account_accesses[0], beneficiary);    // Exists?
    EXPECT_EQ(host.recorded_account_accesses[1], msg.recipient);  // Selfdestruct.
    host.recorded_account_accesses.clear();

    execute(5002, code);
    EXPECT_GAS_USED(EVMC_OUT_OF_GAS, 5002);
    EXPECT_EQ(result.gas_left, 0);
    EXPECT_EQ(host.recorded_account_accesses.size(), 0);
    host.recorded_account_accesses.clear();

    rev = EVMC_SPURIOUS_DRAGON;
    execute(5003, code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 5003);
    ASSERT_EQ(host.recorded_account_accesses.size(), 3);
    EXPECT_EQ(host.recorded_account_accesses[0], msg.recipient);  // Balance.
    EXPECT_EQ(host.recorded_account_accesses[1], beneficiary);    // Exists?
    EXPECT_EQ(host.recorded_account_accesses[2], msg.recipient);  // Selfdestruct.
    host.recorded_account_accesses.clear();

    execute(5002, code);
    EXPECT_GAS_USED(EVMC_OUT_OF_GAS, 5002);
    EXPECT_EQ(result.gas_left, 0);
    EXPECT_EQ(host.recorded_account_accesses.size(), 0);
    host.recorded_account_accesses.clear();
}

TEST_P(evm, selfdestruct_gas_refund)
{
    rev = EVMC_BERLIN;  // The last revision with gas refund.
    const auto code = selfdestruct(0xbe);
    execute(code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 7603);  // Cold access to 0xbe.
    EXPECT_EQ(result.gas_refund, 24000);

    // Second selfdestruct of the same account.
    execute(code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 5003);  // Warm access to 0xbe.
    EXPECT_EQ(result.gas_refund, 0);      // No refund.

    // Third selfdestruct - from different account.
    msg.recipient = 0x01_address;
    execute(code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 5003);  // Warm access to 0xbe.
    EXPECT_EQ(result.gas_refund, 24000);
}

TEST_P(evm, selfdestruct_no_gas_refund)
{
    rev = EVMC_LONDON;  // Since London there is no gas refund.
    execute(selfdestruct(0xbe));
    EXPECT_GAS_USED(EVMC_SUCCESS, 7603);
    EXPECT_EQ(result.gas_refund, 0);
}


TEST_P(evm, blockhash)
{
    host.block_hash.bytes[13] = 0x13;

    host.tx_context.block_number = 0;
    const auto code = blockhash(0) + ret_top();
    execute(code);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    EXPECT_EQ(gas_used, 38);
    ASSERT_EQ(result.output_size, 32);
    EXPECT_EQ(result.output_data[13], 0);
    EXPECT_EQ(host.recorded_blockhashes.size(), 0);

    host.tx_context.block_number = 257;
    execute(code);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    EXPECT_EQ(gas_used, 38);
    ASSERT_EQ(result.output_size, 32);
    EXPECT_EQ(result.output_data[13], 0);
    EXPECT_EQ(host.recorded_blockhashes.size(), 0);

    host.tx_context.block_number = 256;
    execute(code);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    EXPECT_EQ(gas_used, 38);
    ASSERT_EQ(result.output_size, 32);
    EXPECT_EQ(result.output_data[13], 0x13);
    ASSERT_EQ(host.recorded_blockhashes.size(), 1);
    EXPECT_EQ(host.recorded_blockhashes.back(), 0);
}

TEST_P(evm, extcode)
{
    constexpr auto addr = 0xfffffffffffffffffffffffffffffffffffffffe_address;
    host.accounts[addr].code = {'a', 'b', 'c', 'd'};

    bytecode code;
    code += "6002600003803b60019003";  // S = EXTCODESIZE(-2) - 1
    code += "90600080913c";            // EXTCODECOPY(-2, 0, 0, S)
    code += "60046000f3";              // RETURN(0, 4)

    execute(code);
    EXPECT_EQ(gas_used, 1445);
    ASSERT_EQ(result.output_size, 4);
    EXPECT_EQ(bytes_view(result.output_data, 3), bytes_view(host.accounts[addr].code.data(), 3));
    EXPECT_EQ(result.output_data[3], 0);
    ASSERT_EQ(host.recorded_account_accesses.size(), 2);
    EXPECT_EQ(host.recorded_account_accesses[0], addr);
    EXPECT_EQ(host.recorded_account_accesses[1], addr);
}

TEST_P(evm, extcodesize)
{
    constexpr auto addr = 0x0000000000000000000000000000000000000002_address;
    host.accounts[addr].code = {'\0'};
    execute(push(2) + OP_EXTCODESIZE + ret_top());
    EXPECT_OUTPUT_INT(1);
}

TEST_P(evm, extcodecopy_big_index)
{
    constexpr auto index = uint64_t{std::numeric_limits<uint32_t>::max()} + 1;
    const auto code = dup1(1) + push(index) + dup1(0) + OP_EXTCODECOPY + ret(0, {});
    execute(code);
    EXPECT_EQ(output, "00"_hex);
}

TEST_P(evm, extcodehash)
{
    static constexpr auto HASH =
        0xe10000a00000000b0000000000c00000000d00000e0000f0000000000000001e_bytes32;

    host.accounts[{}].codehash = HASH;
    const auto code = push(0) + OP_EXTCODEHASH + ret_top();

    rev = EVMC_BYZANTIUM;
    execute(code);
    EXPECT_EQ(result.status_code, EVMC_UNDEFINED_INSTRUCTION);

    rev = EVMC_PETERSBURG;
    execute(code);
    EXPECT_EQ(gas_used, 418);
    ASSERT_EQ(result.output_size, 32);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size), HASH);
}

TEST_P(evm, codecopy_empty)
{
    execute(push(0) + 2 * OP_DUP1 + OP_CODECOPY + OP_MSIZE + ret_top());
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    EXPECT_EQ(std::count(result.output_data, result.output_data + result.output_size, 0), 32);
}

TEST_P(evm, extcodecopy_empty)
{
    execute(push(0) + 3 * OP_DUP1 + OP_EXTCODECOPY + OP_MSIZE + ret_top());
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    EXPECT_EQ(std::count(result.output_data, result.output_data + result.output_size, 0), 32);
}

TEST_P(evm, codecopy_memory_cost)
{
    auto code = push(1) + push(0) + push(0) + OP_CODECOPY;
    execute(18, code);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    execute(17, code);
    EXPECT_EQ(result.status_code, EVMC_OUT_OF_GAS);
}

TEST_P(evm, extcodecopy_memory_cost)
{
    auto code = push(1) + push(0) + 2 * OP_DUP1 + OP_EXTCODECOPY;
    execute(718, code);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    execute(717, code);
    EXPECT_EQ(result.status_code, EVMC_OUT_OF_GAS);
}

TEST_P(evm, extcodecopy_nonzero_index)
{
    constexpr auto addr = 0x000000000000000000000000000000000000000a_address;
    constexpr auto index = 15;

    auto& extcode = host.accounts[addr].code;
    extcode.assign(16, 0x00);
    extcode[index] = 0xc0;
    auto code = push(2) + push(index) + push(0) + push(0xa) + OP_EXTCODECOPY + ret(0, 2);
    EXPECT_EQ(code.length() + 1, index);
    execute(code);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 2);
    EXPECT_EQ(result.output_data[0], 0xc0);
    EXPECT_EQ(result.output_data[1], 0);
    ASSERT_EQ(host.recorded_account_accesses.size(), 1);
    EXPECT_EQ(host.recorded_account_accesses.back().bytes[19], 0xa);
}

TEST_P(evm, extcodecopy_fill_tail)
{
    auto addr = evmc_address{};
    addr.bytes[19] = 0xa;

    auto& extcode = host.accounts[addr].code;
    extcode = {0xff, 0xfe};
    extcode.resize(1);
    auto code = push(2) + push(0) + push(0) + push(0xa) + OP_EXTCODECOPY + ret(0, 2);
    execute(code);
    ASSERT_EQ(host.recorded_account_accesses.size(), 1);
    EXPECT_EQ(host.recorded_account_accesses.back().bytes[19], 0xa);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 2);
    EXPECT_EQ(result.output_data[0], 0xff);
    EXPECT_EQ(result.output_data[1], 0);
}

TEST_P(evm, extcodecopy_buffer_overflow)
{
    const auto code = bytecode{} + OP_NUMBER + OP_TIMESTAMP + calldatasize() + OP_ADDRESS +
                      OP_EXTCODECOPY + ret(calldatasize(), OP_NUMBER);

    host.accounts[msg.recipient].code = code;

    const auto s = static_cast<int>(code.size());
    const auto values = {0, 1, s - 1, s, s + 1, 5000};
    for (auto offset : values)
    {
        for (auto size : values)
        {
            host.tx_context.block_timestamp = offset;
            host.tx_context.block_number = size;

            execute(code);
            EXPECT_STATUS(EVMC_SUCCESS);
            EXPECT_EQ(result.output_size, size);
        }
    }
}

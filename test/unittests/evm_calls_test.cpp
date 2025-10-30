// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019-2020 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

/// This file contains EVM unit tests that perform any kind of calls.

#include "evm_fixture.hpp"

using namespace evmc::literals;
using namespace evmone::test;

inline constexpr auto max_uint256 =
    0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff_bytes32;

TEST_P(evm, delegatecall)
{
    auto code = bytecode{};
    code += "6001600003600052";              // m[0] = 0xffffff...
    code += "600560046003600260016103e8f4";  // DELEGATECALL(1000, 0x01, ...)
    code += "60086000f3";

    auto call_output = bytes{0xa, 0xb, 0xc};
    host.call_result.output_data = call_output.data();
    host.call_result.output_size = call_output.size();
    host.call_result.gas_left = 1;

    msg.value.bytes[17] = 0xfe;

    execute(1700, code);

    EXPECT_EQ(gas_used, 1690);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);

    auto gas_left = 1700 - 736;
    ASSERT_EQ(host.recorded_calls.size(), 1);
    const auto& call_msg = host.recorded_calls.back();
    EXPECT_EQ(call_msg.gas, gas_left - gas_left / 64);
    EXPECT_EQ(call_msg.input_size, 3);
    EXPECT_EQ(call_msg.value.bytes[17], 0xfe);

    ASSERT_EQ(result.output_size, 8);
    EXPECT_EQ(output, (bytes{0xff, 0xff, 0xff, 0xff, 0xa, 0xb, 0xc, 0xff}));
}

TEST_P(evm, delegatecall_static)
{
    // Checks if DELEGATECALL forwards the "static" flag.
    msg.flags = EVMC_STATIC;
    execute(bytecode{} + delegatecall(0).gas(1));
    ASSERT_EQ(host.recorded_calls.size(), 1);
    const auto& call_msg = host.recorded_calls.back();
    EXPECT_EQ(call_msg.gas, 1);
    EXPECT_EQ(call_msg.flags, uint32_t{EVMC_STATIC});
    EXPECT_GAS_USED(EVMC_SUCCESS, 719);
}

TEST_P(evm, delegatecall_oog_depth_limit)
{
    rev = EVMC_HOMESTEAD;
    msg.depth = 1024;
    const auto code = bytecode{} + delegatecall(0).gas(16) + ret_top();

    execute(code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 73);
    EXPECT_OUTPUT_INT(0);

    execute(73, code);
    EXPECT_STATUS(EVMC_OUT_OF_GAS);
}

TEST_P(evm, create)
{
    auto& account = host.accounts[msg.recipient];
    account.set_balance(1);

    auto call_output = bytes{0xa, 0xb, 0xc};
    host.call_result.output_data = call_output.data();
    host.call_result.output_size = call_output.size();
    host.call_result.create_address = 0xcc010203040506070809010203040506070809ce_address;
    host.call_result.gas_left = 200000;
    execute(300000, sstore(1, create().value(1).input(0, 0x20)));

    EXPECT_GAS_USED(EVMC_SUCCESS, 115816);

    EXPECT_EQ(account.storage[0x01_bytes32].current,
        0x000000000000000000000000cc010203040506070809010203040506070809ce_bytes32);

    ASSERT_EQ(host.recorded_calls.size(), 1);
    const auto& call_msg = host.recorded_calls.back();
    EXPECT_EQ(call_msg.kind, EVMC_CREATE);
    EXPECT_EQ(call_msg.gas, 263801);
    EXPECT_EQ(call_msg.value, 0x01_bytes32);
    EXPECT_EQ(call_msg.input_size, 0x20);
    EXPECT_EQ(call_msg.create2_salt, 0x00_bytes32);
}

TEST_P(evm, create_gas)
{
    size_t c = 0;
    for (auto r : {EVMC_HOMESTEAD, EVMC_TANGERINE_WHISTLE})
    {
        ++c;
        rev = r;
        execute(50000, create());
        EXPECT_EQ(result.status_code, EVMC_SUCCESS);
        EXPECT_EQ(gas_used, rev == EVMC_HOMESTEAD ? 50000 : 49719) << rev;
        ASSERT_EQ(host.recorded_calls.size(), c);
        EXPECT_EQ(host.recorded_calls.back().gas, rev == EVMC_HOMESTEAD ? 17991 : 17710) << rev;
    }
}

TEST_P(evm, create2)
{
    rev = EVMC_CONSTANTINOPLE;
    auto& account = host.accounts[msg.recipient];
    account.set_balance(1);

    const bytes call_output{0xa, 0xb, 0xc};
    host.call_result.output_data = call_output.data();
    host.call_result.output_size = call_output.size();
    host.call_result.create_address = 0xc2010203040506070809010203040506070809ce_address;
    host.call_result.gas_left = 200000;
    execute(300000, sstore(1, create2().value(1).input(0, 0x41).salt(0x5a)));
    EXPECT_GAS_USED(EVMC_SUCCESS, 115817);

    ASSERT_EQ(host.recorded_calls.size(), 1);
    const auto& call_msg = host.recorded_calls.back();
    EXPECT_EQ(call_msg.kind, EVMC_CREATE2);
    EXPECT_EQ(call_msg.gas, 263775);
    EXPECT_EQ(call_msg.value, 0x01_bytes32);
    EXPECT_EQ(call_msg.input_size, 0x41);
    EXPECT_EQ(call_msg.create2_salt, 0x5a_bytes32);

    EXPECT_EQ(account.storage[0x01_bytes32].current,
        0x000000000000000000000000c2010203040506070809010203040506070809ce_bytes32);
}

TEST_P(evm, create2_salt_cost)
{
    rev = EVMC_CONSTANTINOPLE;
    const auto code = create2().input(0, 0x20);

    execute(32021, code);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    EXPECT_EQ(result.gas_left, 0);
    ASSERT_EQ(host.recorded_calls.size(), 1);
    EXPECT_EQ(host.recorded_calls.back().kind, EVMC_CREATE2);
    EXPECT_EQ(host.recorded_calls.back().depth, 1);

    execute(32021 - 1, code);
    EXPECT_EQ(result.status_code, EVMC_OUT_OF_GAS);
    EXPECT_EQ(result.gas_left, 0);
    EXPECT_EQ(host.recorded_calls.size(), 1);  // No more CREATE2 recorded.
}

TEST_P(evm, create_balance_too_low)
{
    rev = EVMC_CONSTANTINOPLE;
    host.accounts[msg.recipient].set_balance(1);
    for (auto op : {OP_CREATE, OP_CREATE2})
    {
        execute(push(2) + (3 * OP_DUP1) + hex(op) + ret_top());
        EXPECT_EQ(result.status_code, EVMC_SUCCESS);
        EXPECT_EQ(std::count(result.output_data, result.output_data + result.output_size, 0), 32);
        EXPECT_EQ(host.recorded_calls.size(), 0);
    }
}

TEST_P(evm, create_failure)
{
    host.call_result.create_address = 0x00000000000000000000000000000000000000ce_address;
    const auto create_address =
        bytes_view{host.call_result.create_address.bytes, sizeof(host.call_result.create_address)};
    rev = EVMC_CONSTANTINOPLE;
    for (auto op : {OP_CREATE, OP_CREATE2})
    {
        const auto code = push(0) + (3 * OP_DUP1) + op + ret_top();

        host.call_result.status_code = EVMC_SUCCESS;
        execute(code);
        EXPECT_EQ(result.status_code, EVMC_SUCCESS);
        ASSERT_EQ(result.output_size, 32);
        EXPECT_EQ((bytes_view{result.output_data + 12, 20}), create_address);
        ASSERT_EQ(host.recorded_calls.size(), 1);
        EXPECT_EQ(host.recorded_calls.back().kind, op == OP_CREATE ? EVMC_CREATE : EVMC_CREATE2);
        host.recorded_calls.clear();

        host.call_result.status_code = EVMC_REVERT;
        execute(code);
        EXPECT_EQ(result.status_code, EVMC_SUCCESS);
        EXPECT_OUTPUT_INT(0);
        ASSERT_EQ(host.recorded_calls.size(), 1);
        EXPECT_EQ(host.recorded_calls.back().kind, op == OP_CREATE ? EVMC_CREATE : EVMC_CREATE2);
        host.recorded_calls.clear();

        host.call_result.status_code = EVMC_FAILURE;
        execute(code);
        EXPECT_EQ(result.status_code, EVMC_SUCCESS);
        EXPECT_OUTPUT_INT(0);
        ASSERT_EQ(host.recorded_calls.size(), 1);
        EXPECT_EQ(host.recorded_calls.back().kind, op == OP_CREATE ? EVMC_CREATE : EVMC_CREATE2);
        host.recorded_calls.clear();
    }
}

TEST_P(evm, call_failing_with_value)
{
    host.accounts[0x00000000000000000000000000000000000000aa_address] = {};
    for (auto op : {OP_CALL, OP_CALLCODE})
    {
        const auto code = push(0xff) + push(0) + OP_DUP2 + OP_DUP2 + push(1) + push(0xaa) +
                          push(0x8000) + op + OP_POP;

        // Fails on balance check.
        execute(12000, code);
        EXPECT_GAS_USED(EVMC_SUCCESS, 7447);
        EXPECT_EQ(host.recorded_calls.size(), 0);  // There was no call().

        // Fails on value transfer additional cost - minimum gas limit that triggers this condition.
        execute(747, code);
        EXPECT_STATUS(EVMC_OUT_OF_GAS);
        EXPECT_EQ(host.recorded_calls.size(), 0);  // There was no call().

        // Fails on value transfer additional cost - maximum gas limit that triggers this condition.
        execute(744 + 9000, code);
        EXPECT_STATUS(EVMC_OUT_OF_GAS);
        EXPECT_EQ(host.recorded_calls.size(), 0);  // There was no call().
    }
}

TEST_P(evm, call_with_value)
{
    constexpr auto code = "60ff600060ff6000600160aa618000f150";

    constexpr auto call_sender = 0x5e4d00000000000000000000000000000000d4e5_address;
    constexpr auto call_dst = 0x00000000000000000000000000000000000000aa_address;

    msg.recipient = call_sender;
    host.accounts[msg.recipient].set_balance(1);
    host.accounts[call_dst] = {};
    host.call_result.gas_left = 1;

    execute(40000, code);
    EXPECT_EQ(gas_used, 7447 + 32082);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    ASSERT_EQ(host.recorded_calls.size(), 1);
    const auto& call_msg = host.recorded_calls.back();
    EXPECT_EQ(call_msg.kind, EVMC_CALL);
    EXPECT_EQ(call_msg.depth, 1);
    EXPECT_EQ(call_msg.gas, 32083);
    EXPECT_EQ(call_msg.recipient, call_dst);
    EXPECT_EQ(call_msg.sender, call_sender);
}

TEST_P(evm, call_with_value_depth_limit)
{
    auto call_dst = evmc_address{};
    call_dst.bytes[19] = 0xaa;
    host.accounts[call_dst] = {};

    msg.depth = 1024;
    execute(bytecode{"60ff600060ff6000600160aa618000f150"});
    EXPECT_EQ(gas_used, 7447);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    EXPECT_EQ(host.recorded_calls.size(), 0);
}

TEST_P(evm, call_depth_limit)
{
    rev = EVMC_CONSTANTINOPLE;
    msg.depth = 1024;

    for (auto op : {OP_CALL, OP_CALLCODE, OP_DELEGATECALL, OP_STATICCALL, OP_CREATE, OP_CREATE2})
    {
        const auto code = push(0) + 6 * OP_DUP1 + op + ret_top() + OP_INVALID;
        execute(code);
        EXPECT_EQ(result.status_code, EVMC_SUCCESS);
        EXPECT_EQ(host.recorded_calls.size(), 0);
        EXPECT_OUTPUT_INT(0);
    }
}

TEST_P(evm, call_output)
{
    static bool result_is_correct = false;
    static uint8_t call_output[] = {0xa, 0xb};

    host.accounts[{}].set_balance(1);
    host.call_result.output_data = call_output;
    host.call_result.output_size = sizeof(call_output);
    host.call_result.release = [](const evmc_result* r) {
        result_is_correct = r->output_size == sizeof(call_output) && r->output_data == call_output;
    };

    auto code_prefix_output_1 = push(1) + 6 * OP_DUP1 + push("7fffffffffffffff");
    auto code_prefix_output_0 = push(0) + 6 * OP_DUP1 + push("7fffffffffffffff");
    auto code_suffix = ret(0, 3);

    for (auto op : {OP_CALL, OP_CALLCODE, OP_DELEGATECALL, OP_STATICCALL})
    {
        result_is_correct = false;
        execute(code_prefix_output_1 + hex(op) + code_suffix);
        EXPECT_TRUE(result_is_correct);
        EXPECT_EQ(result.status_code, EVMC_SUCCESS);
        ASSERT_EQ(result.output_size, 3);
        EXPECT_EQ(result.output_data[0], 0);
        EXPECT_EQ(result.output_data[1], 0xa);
        EXPECT_EQ(result.output_data[2], 0);


        result_is_correct = false;
        execute(code_prefix_output_0 + hex(op) + code_suffix);
        EXPECT_TRUE(result_is_correct);
        EXPECT_EQ(result.status_code, EVMC_SUCCESS);
        ASSERT_EQ(result.output_size, 3);
        EXPECT_EQ(result.output_data[0], 0);
        EXPECT_EQ(result.output_data[1], 0);
        EXPECT_EQ(result.output_data[2], 0);
    }
}

TEST_P(evm, call_high_gas)
{
    rev = EVMC_HOMESTEAD;
    host.accounts[0xaa_address] = {};

    for (auto call_opcode : {OP_CALL, OP_CALLCODE, OP_DELEGATECALL})
    {
        execute(5000, 5 * push(0) + push(0xaa) + push(0x134c) + call_opcode);
        EXPECT_EQ(result.status_code, EVMC_OUT_OF_GAS);
    }
}

TEST_P(evm, call_value_zero_to_nonexistent_account)
{
    constexpr auto call_gas = 6000;
    host.call_result.gas_left = 1000;

    const auto code = push(0x40) + push(0) + push(0x40) + push(0) + push(0) + push(0xaa) +
                      push(call_gas) + OP_CALL + OP_POP;

    execute(9000, code);
    EXPECT_EQ(gas_used, 729 + (call_gas - host.call_result.gas_left));
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    ASSERT_EQ(host.recorded_calls.size(), 1);
    const auto& call_msg = host.recorded_calls.back();
    EXPECT_EQ(call_msg.kind, EVMC_CALL);
    EXPECT_EQ(call_msg.depth, 1);
    EXPECT_EQ(call_msg.gas, 6000);
    EXPECT_EQ(call_msg.input_size, 64);
    EXPECT_EQ(call_msg.recipient, 0x00000000000000000000000000000000000000aa_address);
    EXPECT_EQ(call_msg.value.bytes[31], 0);
}

TEST_P(evm, call_new_account_creation_cost)
{
    constexpr auto call_dst = 0x00000000000000000000000000000000000000ad_address;
    constexpr auto msg_dst = 0x0000000000000000000000000000000000000003_address;
    const auto code =
        4 * push(0) + calldataload(0) + push(call_dst) + push(0) + OP_CALL + ret_top();
    msg.recipient = msg_dst;


    rev = EVMC_TANGERINE_WHISTLE;
    host.accounts[msg.recipient].set_balance(0);
    execute(code, "00"_hex);
    EXPECT_GAS_USED(EVMC_SUCCESS, 25000 + 739);
    EXPECT_OUTPUT_INT(1);
    ASSERT_EQ(host.recorded_calls.size(), 1);
    EXPECT_EQ(host.recorded_calls.back().recipient, call_dst);
    EXPECT_EQ(host.recorded_calls.back().gas, 0);
    ASSERT_EQ(host.recorded_account_accesses.size(), 2);
    EXPECT_EQ(host.recorded_account_accesses[0], call_dst);  // Account exist?
    EXPECT_EQ(host.recorded_account_accesses[1], call_dst);  // Call.
    host.recorded_account_accesses.clear();
    host.recorded_calls.clear();

    rev = EVMC_TANGERINE_WHISTLE;
    host.accounts[msg.recipient].set_balance(1);
    execute(code, "0000000000000000000000000000000000000000000000000000000000000001"_hex);
    EXPECT_GAS_USED(EVMC_SUCCESS, 25000 + 9000 + 739);
    EXPECT_OUTPUT_INT(1);
    ASSERT_EQ(host.recorded_calls.size(), 1);
    EXPECT_EQ(host.recorded_calls.back().recipient, call_dst);
    EXPECT_EQ(host.recorded_calls.back().gas, 2300);
    EXPECT_EQ(host.recorded_calls.back().sender, msg_dst);
    EXPECT_EQ(host.recorded_calls.back().value.bytes[31], 1);
    EXPECT_EQ(host.recorded_calls.back().input_size, 0);
    ASSERT_EQ(host.recorded_account_accesses.size(), 3);
    EXPECT_EQ(host.recorded_account_accesses[0], call_dst);       // Account exist?
    EXPECT_EQ(host.recorded_account_accesses[1], msg.recipient);  // Balance.
    EXPECT_EQ(host.recorded_account_accesses[2], call_dst);       // Call.
    host.recorded_account_accesses.clear();
    host.recorded_calls.clear();

    rev = EVMC_SPURIOUS_DRAGON;
    host.accounts[msg.recipient].set_balance(0);
    execute(code, "00"_hex);
    EXPECT_GAS_USED(EVMC_SUCCESS, 739);
    EXPECT_OUTPUT_INT(1);
    ASSERT_EQ(host.recorded_calls.size(), 1);
    EXPECT_EQ(host.recorded_calls.back().recipient, call_dst);
    EXPECT_EQ(host.recorded_calls.back().gas, 0);
    EXPECT_EQ(host.recorded_calls.back().sender, msg_dst);
    EXPECT_EQ(host.recorded_calls.back().value.bytes[31], 0);
    EXPECT_EQ(host.recorded_calls.back().input_size, 0);
    ASSERT_EQ(host.recorded_account_accesses.size(), 1);
    EXPECT_EQ(host.recorded_account_accesses[0], call_dst);  // Call.
    host.recorded_account_accesses.clear();
    host.recorded_calls.clear();

    rev = EVMC_SPURIOUS_DRAGON;
    host.accounts[msg.recipient].set_balance(1);
    execute(code, "0000000000000000000000000000000000000000000000000000000000000001"_hex);
    EXPECT_GAS_USED(EVMC_SUCCESS, 25000 + 9000 + 739);
    EXPECT_OUTPUT_INT(1);
    ASSERT_EQ(host.recorded_calls.size(), 1);
    EXPECT_EQ(host.recorded_calls.back().recipient, call_dst);
    EXPECT_EQ(host.recorded_calls.back().gas, 2300);
    EXPECT_EQ(host.recorded_calls.back().sender, msg_dst);
    EXPECT_EQ(host.recorded_calls.back().value.bytes[31], 1);
    EXPECT_EQ(host.recorded_calls.back().input_size, 0);
    ASSERT_EQ(host.recorded_account_accesses.size(), 3);
    EXPECT_EQ(host.recorded_account_accesses[0], call_dst);       // Account exist?
    EXPECT_EQ(host.recorded_account_accesses[1], msg.recipient);  // Balance.
    EXPECT_EQ(host.recorded_account_accesses[2], call_dst);       // Call.
    host.recorded_account_accesses.clear();
    host.recorded_calls.clear();
}

TEST_P(evm, callcode_new_account_create)
{
    constexpr auto code = "60008080806001600061c350f250";
    constexpr auto call_sender = 0x5e4d00000000000000000000000000000000d4e5_address;

    msg.recipient = call_sender;
    host.accounts[msg.recipient].set_balance(1);
    host.call_result.gas_left = 1;
    execute(100000, code);
    EXPECT_EQ(gas_used, 59722);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    ASSERT_EQ(host.recorded_calls.size(), 1);
    const auto& call_msg = host.recorded_calls.back();
    EXPECT_EQ(call_msg.kind, EVMC_CALLCODE);
    EXPECT_EQ(call_msg.depth, 1);
    EXPECT_EQ(call_msg.gas, 52300);
    EXPECT_EQ(call_msg.sender, call_sender);
    EXPECT_EQ(call_msg.value.bytes[31], 1);
}

TEST_P(evm, call_then_oog)
{
    // Performs a CALL then OOG in the same code block.
    auto call_dst = evmc_address{};
    call_dst.bytes[19] = 0xaa;
    host.accounts[call_dst] = {};
    host.call_result.status_code = EVMC_FAILURE;
    host.call_result.gas_left = 0;

    const auto code =
        call(0xaa).gas(254).value(0).input(0, 0x40).output(0, 0x40) + 4 * add(OP_DUP1) + OP_POP;

    execute(1000, code);
    EXPECT_EQ(gas_used, 1000);
    ASSERT_EQ(host.recorded_calls.size(), 1);
    const auto& call_msg = host.recorded_calls.back();
    EXPECT_EQ(call_msg.gas, 254);
    EXPECT_EQ(result.gas_left, 0);
    EXPECT_EQ(result.status_code, EVMC_OUT_OF_GAS);
}

TEST_P(evm, callcode_then_oog)
{
    // Performs a CALLCODE then OOG in the same code block.
    host.call_result.status_code = EVMC_FAILURE;
    host.call_result.gas_left = 0;

    const auto code =
        callcode(0xaa).gas(100).value(0).input(0, 3).output(3, 9) + 4 * add(OP_DUP1) + OP_POP;

    execute(825, code);
    EXPECT_STATUS(EVMC_OUT_OF_GAS);
    ASSERT_EQ(host.recorded_calls.size(), 1);
    const auto& call_msg = host.recorded_calls.back();
    EXPECT_EQ(call_msg.gas, 100);
}

TEST_P(evm, delegatecall_then_oog)
{
    // Performs a CALL then OOG in the same code block.
    auto call_dst = evmc_address{};
    call_dst.bytes[19] = 0xaa;
    host.accounts[call_dst] = {};
    host.call_result.status_code = EVMC_FAILURE;
    host.call_result.gas_left = 0;

    const auto code =
        delegatecall(0xaa).gas(254).input(0, 0x40).output(0, 0x40) + 4 * add(OP_DUP1) + OP_POP;

    execute(1000, code);
    EXPECT_EQ(gas_used, 1000);
    ASSERT_EQ(host.recorded_calls.size(), 1);
    const auto& call_msg = host.recorded_calls.back();
    EXPECT_EQ(call_msg.gas, 254);
    EXPECT_EQ(result.gas_left, 0);
    EXPECT_EQ(result.status_code, EVMC_OUT_OF_GAS);
}

TEST_P(evm, staticcall_then_oog)
{
    // Performs a STATICCALL then OOG in the same code block.
    auto call_dst = evmc_address{};
    call_dst.bytes[19] = 0xaa;
    host.accounts[call_dst] = {};
    host.call_result.status_code = EVMC_FAILURE;
    host.call_result.gas_left = 0;

    const auto code =
        staticcall(0xaa).gas(254).input(0, 0x40).output(0, 0x40) + 4 * add(OP_DUP1) + OP_POP;

    execute(1000, code);
    EXPECT_EQ(gas_used, 1000);
    ASSERT_EQ(host.recorded_calls.size(), 1);
    const auto& call_msg = host.recorded_calls.back();
    EXPECT_EQ(call_msg.gas, 254);
    EXPECT_EQ(result.gas_left, 0);
    EXPECT_EQ(result.status_code, EVMC_OUT_OF_GAS);
}

TEST_P(evm, staticcall_input)
{
    const auto code = mstore(3, 0x010203) + staticcall(0).gas(0xee).input(32, 3);
    execute(code);
    ASSERT_EQ(host.recorded_calls.size(), 1);
    const auto& call_msg = host.recorded_calls.back();
    EXPECT_EQ(call_msg.gas, 0xee);
    EXPECT_EQ(call_msg.input_size, 3);
    EXPECT_EQ(hex(bytes_view(call_msg.input_data, call_msg.input_size)), "010203");
}

TEST_P(evm, call_with_value_low_gas)
{
    // Create the call destination account.
    host.accounts[0x0000000000000000000000000000000000000000_address] = {};
    for (auto call_op : {OP_CALL, OP_CALLCODE})
    {
        auto code = 4 * push(0) + push(1) + 2 * push(0) + call_op + OP_POP;
        execute(9721, code);
        EXPECT_EQ(result.status_code, EVMC_SUCCESS);
        EXPECT_EQ(result.gas_left, 2300 - 2);
    }
}

TEST_P(evm, call_oog_after_balance_check)
{
    // Create the call destination account.
    host.accounts[0x0000000000000000000000000000000000000000_address] = {};
    for (auto op : {OP_CALL, OP_CALLCODE})
    {
        auto code = 4 * push(0) + push(1) + 2 * push(0) + op + OP_SELFDESTRUCT;
        execute(12420, code);
        EXPECT_EQ(result.status_code, EVMC_OUT_OF_GAS);
    }
}

TEST_P(evm, call_oog_after_depth_check)
{
    // Create the call recipient account.
    host.accounts[0x0000000000000000000000000000000000000000_address] = {};
    msg.depth = 1024;

    for (auto op : {OP_CALL, OP_CALLCODE})
    {
        const auto code = 4 * push(0) + push(1) + 2 * push(0) + op + OP_SELFDESTRUCT;
        execute(12420, code);
        EXPECT_EQ(result.status_code, EVMC_OUT_OF_GAS);
    }

    rev = EVMC_TANGERINE_WHISTLE;
    const auto code = 7 * push(0) + OP_CALL + OP_SELFDESTRUCT;
    execute(721, code);
    EXPECT_EQ(result.status_code, EVMC_OUT_OF_GAS);

    execute(721 + 5000 - 1, code);
    EXPECT_EQ(result.status_code, EVMC_OUT_OF_GAS);
}

TEST_P(evm, call_recipient_and_code_address)
{
    constexpr auto origin = 0x9900000000000000000000000000000000000099_address;
    constexpr auto executor = 0xee000000000000000000000000000000000000ee_address;
    constexpr auto recipient = 0x4400000000000000000000000000000000000044_address;

    msg.sender = origin;
    msg.recipient = executor;

    for (auto op : {OP_CALL, OP_CALLCODE, OP_DELEGATECALL, OP_STATICCALL})
    {
        const auto code = 5 * push(0) + push(recipient) + push(0) + op;
        execute(100000, code);
        EXPECT_GAS_USED(EVMC_SUCCESS, 721);
        ASSERT_EQ(host.recorded_calls.size(), 1);
        const auto& call = host.recorded_calls[0];
        EXPECT_EQ(call.recipient, (op == OP_CALL || op == OP_STATICCALL) ? recipient : executor);
        EXPECT_EQ(call.code_address, recipient);
        EXPECT_EQ(call.sender, (op == OP_DELEGATECALL) ? origin : executor);
        host.recorded_calls.clear();
    }
}

TEST_P(evm, call_value)
{
    constexpr auto origin = 0x9900000000000000000000000000000000000099_address;
    constexpr auto executor = 0xee000000000000000000000000000000000000ee_address;
    constexpr auto recipient = 0x4400000000000000000000000000000000000044_address;

    constexpr auto passed_value = 3;
    constexpr auto origin_value = 8;

    msg.sender = origin;
    msg.recipient = executor;
    msg.value.bytes[31] = origin_value;
    host.accounts[executor].set_balance(passed_value);
    host.accounts[recipient] = {};  // Create the call recipient account.

    for (auto op : {OP_CALL, OP_CALLCODE, OP_DELEGATECALL, OP_STATICCALL})
    {
        const auto has_value_arg = (op == OP_CALL || op == OP_CALLCODE);
        const auto value_cost = has_value_arg ? 9000 : 0;
        const auto expected_value = [=] {
            if (has_value_arg)
                return passed_value;
            else if (op == OP_DELEGATECALL)
                return origin_value;
            else
                return 0;
        }();

        const auto code =
            4 * push(0) + push(has_value_arg ? passed_value : 0) + push(recipient) + push(0) + op;
        execute(100000, code);
        EXPECT_GAS_USED(EVMC_SUCCESS, 721 + value_cost);
        ASSERT_EQ(host.recorded_calls.size(), 1);
        const auto& call = host.recorded_calls[0];
        EXPECT_EQ(call.value.bytes[31], expected_value) << op;
        host.recorded_calls.clear();
    }
}

TEST_P(evm, create_oog_after)
{
    rev = EVMC_CONSTANTINOPLE;
    for (auto op : {OP_CREATE, OP_CREATE2})
    {
        auto code = 4 * push(0) + op + OP_SELFDESTRUCT;
        execute(39000, code);
        EXPECT_STATUS(EVMC_OUT_OF_GAS);
    }
}

TEST_P(evm, returndatasize_before_call)
{
    execute(returndatasize() + ret_top());
    EXPECT_GAS_USED(EVMC_SUCCESS, 17);
    EXPECT_OUTPUT_INT(0);
}

TEST_P(evm, returndatasize)
{
    const uint8_t call_output[13]{};
    host.call_result.output_data = std::data(call_output);

    const auto code = delegatecall(0) + returndatasize() + ret_top();

    host.call_result.status_code = EVMC_SUCCESS;
    host.call_result.output_size = std::size(call_output);
    execute(code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 735);
    EXPECT_OUTPUT_INT(std::size(call_output));

    host.call_result.status_code = EVMC_FAILURE;
    host.call_result.output_size = 1;
    execute(code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 735);
    EXPECT_OUTPUT_INT(1);

    host.call_result.status_code = EVMC_INTERNAL_ERROR;
    host.call_result.output_size = 0;
    execute(code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 735);
    EXPECT_OUTPUT_INT(0);
}

TEST_P(evm, returndatacopy)
{
    const auto call_output =
        0x497f3c9f61479c1cfa53f0373d39d2bf4e5f73f71411da62f1d6b85c03a60735_bytes32;
    host.call_result.output_data = std::data(call_output.bytes);
    host.call_result.output_size = std::size(call_output.bytes);

    const auto code = delegatecall(0) + returndatacopy(0, 0, 32) + ret(0, 32);
    execute(code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 742);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size), call_output);
}

TEST_P(evm, returndatacopy_empty)
{
    execute(delegatecall(0) + returndatacopy(0, 0, 0) + ret(0, 32));
    EXPECT_GAS_USED(EVMC_SUCCESS, 739);
    EXPECT_OUTPUT_INT(0);
}

TEST_P(evm, returndatacopy_cost)
{
    const uint8_t call_output[1]{};
    host.call_result.output_data = std::data(call_output);
    host.call_result.output_size = std::size(call_output);

    const auto code = staticcall(0) + returndatacopy(0, 0, 1);
    execute(736, code);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    execute(735, code);
    EXPECT_EQ(result.status_code, EVMC_OUT_OF_GAS);
}

TEST_P(evm, returndatacopy_outofrange)
{
    const uint8_t call_output[2]{};
    host.call_result.output_data = std::data(call_output);
    host.call_result.output_size = std::size(call_output);

    execute(735, staticcall(0) + returndatacopy(0, 0, 3));
    EXPECT_EQ(result.status_code, EVMC_INVALID_MEMORY_ACCESS);

    execute(735, staticcall(0) + returndatacopy(0, 1, 2));
    EXPECT_EQ(result.status_code, EVMC_INVALID_MEMORY_ACCESS);

    execute(735, staticcall(0) + returndatacopy(0, 2, 1));
    EXPECT_EQ(result.status_code, EVMC_INVALID_MEMORY_ACCESS);

    execute(735, staticcall(0) + returndatacopy(0, 3, 0));
    EXPECT_EQ(result.status_code, EVMC_INVALID_MEMORY_ACCESS);

    execute(735, staticcall(0) + returndatacopy(0, 1, 0));
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);

    execute(735, staticcall(0) + returndatacopy(0, 2, 0));
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
}

TEST_P(evm, returndatacopy_outofrange_highbits)
{
    const uint8_t call_output[2]{};
    host.call_result.output_data = std::data(call_output);
    host.call_result.output_size = std::size(call_output);

    // Covers an incorrect cast of RETURNDATACOPY arg to `size_t` ignoring the high bits.
    const auto highbits =
        0x1000000000000000000000000000000000000000000000000000000000000000_bytes32;
    execute(735, staticcall(0) + returndatacopy(0, highbits, 0));
    EXPECT_EQ(result.status_code, EVMC_INVALID_MEMORY_ACCESS);
}

TEST_P(evm, call_gas_refund_propagation)
{
    rev = EVMC_LONDON;
    host.accounts[msg.recipient].set_balance(1);
    host.call_result.status_code = EVMC_SUCCESS;
    host.call_result.gas_refund = 1;

    const auto code_prolog = 7 * push(1);
    for (const auto op :
        {OP_CALL, OP_CALLCODE, OP_DELEGATECALL, OP_STATICCALL, OP_CREATE, OP_CREATE2})
    {
        execute(code_prolog + op);
        EXPECT_STATUS(EVMC_SUCCESS);
        EXPECT_EQ(result.gas_refund, 1);
    }
}

TEST_P(evm, call_gas_refund_aggregation_different_calls)
{
    rev = EVMC_LONDON;
    host.accounts[msg.recipient].set_balance(1);
    host.call_result.status_code = EVMC_SUCCESS;
    host.call_result.gas_refund = 1;

    const auto a = 0xaa_address;
    const auto code =
        call(a) + callcode(a) + delegatecall(a) + staticcall(a) + create() + create2();
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(result.gas_refund, 6);
}

TEST_P(evm, call_gas_refund_aggregation_same_calls)
{
    rev = EVMC_LONDON;
    host.accounts[msg.recipient].set_balance(2);
    host.call_result.status_code = EVMC_SUCCESS;
    host.call_result.gas_refund = 1;

    const auto code_prolog = 14 * push(1);
    for (const auto op :
        {OP_CALL, OP_CALLCODE, OP_DELEGATECALL, OP_STATICCALL, OP_CREATE, OP_CREATE2})
    {
        execute(code_prolog + 2 * op);
        EXPECT_STATUS(EVMC_SUCCESS);
        EXPECT_EQ(result.gas_refund, 2);
    }
}

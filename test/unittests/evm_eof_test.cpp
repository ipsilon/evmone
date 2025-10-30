// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2021 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "evm_fixture.hpp"
#include <evmone/eof.hpp>

using namespace evmc::literals;
using namespace evmone::test;

TEST_P(evm, eof1_execution)
{
    const auto code = eof_bytecode(OP_STOP);

    rev = EVMC_CANCUN;
    execute(code);
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);

    rev = EVMC_EXPERIMENTAL;
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
}

TEST_P(evm, eof1_execution_with_data_section)
{
    rev = EVMC_EXPERIMENTAL;
    // data section contains ret(0, 1)
    const auto code = eof_bytecode(mstore8(0, 1) + OP_STOP, 2).data(ret(0, 1));

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(result.output_size, 0);
}

TEST_P(evm, eof_data_only_contract)
{
    rev = EVMC_EXPERIMENTAL;
    auto code = "EF0001 010004 020001 0001 FFdaaa 00 00800000 FE"_hex;
    const auto data_size_ptr = &code[code.find(0xda)];

    intx::be::unsafe::store(data_size_ptr, uint16_t{0});
    execute(code);
    EXPECT_STATUS(EVMC_INVALID_INSTRUCTION);

    intx::be::unsafe::store(data_size_ptr, uint16_t{1});
    execute(code + "aa"_hex);
    EXPECT_STATUS(EVMC_INVALID_INSTRUCTION);

    intx::be::unsafe::store(data_size_ptr, uint16_t{256});
    execute(code + bytes(256, 0x01));
    EXPECT_STATUS(EVMC_INVALID_INSTRUCTION);
}

TEST_P(evm, eof1_dataload)
{
    // Data instructions are not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    // data is 64 bytes long
    const auto data = bytes(8, 0x0) + bytes(8, 0x11) + bytes(8, 0x22) + bytes(8, 0x33) +
                      bytes(8, 0xaa) + bytes(8, 0xbb) + bytes(8, 0xcc) + bytes(8, 0xdd);
    const auto code = eof_bytecode(calldataload(0) + OP_DATALOAD + ret_top(), 2).data(data);

    // DATALOAD(0)
    execute(code, "00"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "0000000000000000111111111111111122222222222222223333333333333333"_hex);

    // DATALOAD(1)
    execute(code, "0000000000000000000000000000000000000000000000000000000000000001"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "00000000000000111111111111111122222222222222223333333333333333aa"_hex);

    // DATALOAD(2)
    execute(code, "0000000000000000000000000000000000000000000000000000000000000020"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "aaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbccccccccccccccccdddddddddddddddd"_hex);

    // DATALOAD(33) - truncated word
    execute(code, "0000000000000000000000000000000000000000000000000000000000000021"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "aaaaaaaaaaaaaabbbbbbbbbbbbbbbbccccccccccccccccdddddddddddddddd00"_hex);

    // DATALOAD(64) - out of data bounds
    execute(code, "0000000000000000000000000000000000000000000000000000000000000040"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "0000000000000000000000000000000000000000000000000000000000000000"_hex);

    // DATALOAD(u256_max) - out of data bounds
    execute(code, "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "0000000000000000000000000000000000000000000000000000000000000000"_hex);
}

TEST_P(evm, eof1_dataloadn)
{
    // Data instructions are not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    // data is 64 bytes long
    const auto data = bytes(8, 0x0) + bytes(8, 0x11) + bytes(8, 0x22) + bytes(8, 0x33) +
                      bytes(8, 0xaa) + bytes(8, 0xbb) + bytes(8, 0xcc) + bytes(8, 0xdd);

    // DATALOADN{0}
    auto code = eof_bytecode(bytecode(OP_DATALOADN) + "0000" + ret_top(), 2).data(data);
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "0000000000000000111111111111111122222222222222223333333333333333"_hex);

    // DATALOADN{1}
    code = eof_bytecode(bytecode(OP_DATALOADN) + "0001" + ret_top(), 2).data(data);
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "00000000000000111111111111111122222222222222223333333333333333aa"_hex);

    // DATALOADN{32}
    code = eof_bytecode(bytecode(OP_DATALOADN) + "0020" + ret_top(), 2).data(data);
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "aaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbccccccccccccccccdddddddddddddddd"_hex);
}

TEST_P(evm, eof1_datasize)
{
    // Data instructions are not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;

    // no data section
    auto code = eof_bytecode(bytecode(OP_DATASIZE) + ret_top(), 2);
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "0000000000000000000000000000000000000000000000000000000000000000"_hex);

    code = eof_bytecode(bytecode(OP_DATASIZE) + ret_top(), 2).data(bytes{0x0});
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "0000000000000000000000000000000000000000000000000000000000000001"_hex);

    code = eof_bytecode(bytecode(OP_DATASIZE) + ret_top(), 2).data(bytes(32, 0x0));
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "0000000000000000000000000000000000000000000000000000000000000020"_hex);

    code = eof_bytecode(bytecode(OP_DATASIZE) + ret_top(), 2).data(bytes(64, 0x0));
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "0000000000000000000000000000000000000000000000000000000000000040"_hex);

    code = eof_bytecode(bytecode(OP_DATASIZE) + ret_top(), 2).data(bytes(80, 0x0));
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "0000000000000000000000000000000000000000000000000000000000000050"_hex);
}

TEST_P(evm, eof1_datacopy)
{
    // Data instructions are not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    // data is 64 bytes long
    const auto data = bytes(8, 0x0) + bytes(8, 0x11) + bytes(8, 0x22) + bytes(8, 0x33) +
                      bytes(8, 0xaa) + bytes(8, 0xbb) + bytes(8, 0xcc) + bytes(8, 0xdd);

    auto code = eof_bytecode(bytecode(1) + 0 + 0 + OP_DATACOPY + ret(0, 32), 3).data(data);
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "0000000000000000000000000000000000000000000000000000000000000000"_hex);

    code = eof_bytecode(bytecode(1) + 8 + 0 + OP_DATACOPY + ret(0, 32), 3).data(data);
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "1100000000000000000000000000000000000000000000000000000000000000"_hex);

    code = eof_bytecode(bytecode(1) + 63 + 0 + OP_DATACOPY + ret(0, 32), 3).data(data);
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "dd00000000000000000000000000000000000000000000000000000000000000"_hex);

    code = eof_bytecode(bytecode(0) + 64 + 0 + OP_DATACOPY + ret(0, 32), 3).data(data);
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "0000000000000000000000000000000000000000000000000000000000000000"_hex);

    code = eof_bytecode(bytecode(16) + 8 + 0 + OP_DATACOPY + ret(0, 32), 3).data(data);
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "1111111111111111222222222222222200000000000000000000000000000000"_hex);

    code = eof_bytecode(bytecode(32) + 8 + 0 + OP_DATACOPY + ret(0, 32), 3).data(data);
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "111111111111111122222222222222223333333333333333aaaaaaaaaaaaaaaa"_hex);

    code = eof_bytecode(bytecode(8) + 63 + 0 + OP_DATACOPY + ret(0, 32), 3).data(data);
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "dd00000000000000000000000000000000000000000000000000000000000000"_hex);

    code = eof_bytecode(bytecode(0) + 65 + 0 + OP_DATACOPY + ret(0, 32), 3).data(data);
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "0000000000000000000000000000000000000000000000000000000000000000"_hex);
}

TEST_P(evm, datacopy_memory_cost)
{
    // Data instructions are not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    const auto data = bytes{0};
    const auto code = eof_bytecode(bytecode(1) + 0 + 0 + OP_DATACOPY + OP_STOP, 3).data(data);
    execute(18, code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 18);

    execute(17, code);
    EXPECT_EQ(result.status_code, EVMC_OUT_OF_GAS);
}

TEST_P(evm, eof_eofcreate)
{
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = "abcdef"_hex;
    const auto aux_data = "aabbccddeeff"_hex;
    const auto deploy_data_size = static_cast<uint16_t>(deploy_data.size() + aux_data.size());
    const bytecode deploy_container =
        eof_bytecode(bytecode(OP_INVALID)).data(deploy_data, deploy_data_size);

    const auto init_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) + OP_CALLDATASIZE + 0 + OP_RETURNCODE + Opcode{0};
    const auto init_container = eof_bytecode(init_code, 3).container(deploy_container);

    const auto create_code = calldatacopy(0, 0, OP_CALLDATASIZE) +
                             eofcreate().input(0, OP_CALLDATASIZE).salt(0xff) + ret_top();
    const auto container = eof_bytecode(create_code, 4).container(init_container);

    // test executing create code mocking EOFCREATE call
    host.call_result.output_data = deploy_container.data();
    host.call_result.output_size = deploy_container.size();
    host.call_result.create_address = 0xcc010203040506070809010203040506070809ce_address;

    execute(container, aux_data);
    EXPECT_STATUS(EVMC_SUCCESS);

    ASSERT_EQ(host.recorded_calls.size(), 1);
    const auto& call_msg = host.recorded_calls.back();

    EXPECT_EQ(call_msg.input_size, aux_data.size());

    ASSERT_EQ(result.output_size, 32);
    EXPECT_EQ(output, "000000000000000000000000cc010203040506070809010203040506070809ce"_hex);
}

TEST_P(evm, eofcreate_staticmode)
{
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    msg.flags |= EVMC_STATIC;
    const auto code = eof_bytecode(4 * push0() + OP_EOFCREATE + "00" + OP_STOP, 4)
                          .container(eof_bytecode(push0() + push0() + OP_REVERT, 2));
    execute(code);
    EXPECT_EQ(result.status_code, EVMC_STATIC_MODE_VIOLATION);
    EXPECT_EQ(result.gas_left, 0);
}

// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2024 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "../utils/bytecode.hpp"
#include "state_transition.hpp"

using namespace evmc::literals;
using namespace evmone::test;

namespace
{
constexpr bytes32 Salt{0xff};
}

TEST_F(state_transition, txcreate_empty_auxdata)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = "abcdef"_hex;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID)).data(deploy_data);

    const auto init_code = returncode(0, 0, 0);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        txcreate().initcode(keccak256(init_container)).input(0, 0).salt(Salt) + ret_top();
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[create_address].code = deploy_container;
    expect.post[create_address].nonce = 1;
}

TEST_F(state_transition, txcreate_auxdata_equal_to_declared)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = "abcdef"_hex;
    const auto aux_data = "aabbccddeeff"_hex;
    const auto deploy_data_size = static_cast<uint16_t>(deploy_data.size() + aux_data.size());
    const auto deploy_container =
        eof_bytecode(bytecode(OP_INVALID)).data(deploy_data, deploy_data_size);

    const auto init_code = calldatacopy(0, 0, OP_CALLDATASIZE) + returncode(0, 0, OP_CALLDATASIZE);
    const bytecode init_container = eof_bytecode(init_code, 3).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt) +
        ret_top();
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    tx.data = aux_data;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    const auto expected_container = eof_bytecode(bytecode(OP_INVALID)).data(deploy_data + aux_data);

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[create_address].code = expected_container;
    expect.post[create_address].nonce = 1;
}

TEST_F(state_transition, txcreate_auxdata_longer_than_declared)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = "abcdef"_hex;
    const auto aux_data1 = "aabbccdd"_hex;
    const auto aux_data2 = "eeff"_hex;
    const auto deploy_data_size = static_cast<uint16_t>(deploy_data.size() + aux_data1.size());
    const auto deploy_container =
        eof_bytecode(bytecode(OP_INVALID)).data(deploy_data, deploy_data_size);

    const auto init_code = calldatacopy(0, 0, OP_CALLDATASIZE) + returncode(0, 0, OP_CALLDATASIZE);
    const bytecode init_container = eof_bytecode(init_code, 3).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt) +
        ret_top();
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    tx.data = aux_data1 + aux_data2;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    const auto expected_container =
        eof_bytecode(bytecode(OP_INVALID)).data(deploy_data + aux_data1 + aux_data2);

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[create_address].code = expected_container;
    expect.post[create_address].nonce = 1;
}

TEST_F(state_transition, txcreate_auxdata_shorter_than_declared)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = "abcdef"_hex;
    const auto aux_data = "aabbccddeeff"_hex;
    const auto deploy_data_size = static_cast<uint16_t>(deploy_data.size() + aux_data.size() + 1);
    const auto deploy_container =
        eof_bytecode(bytecode(OP_INVALID)).data(deploy_data, deploy_data_size);

    const auto init_code = calldatacopy(0, 0, OP_CALLDATASIZE) + returncode(0, 0, OP_CALLDATASIZE);
    const bytecode init_container = eof_bytecode(init_code, 3).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt) +
        ret_top();
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    tx.data = aux_data;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
}

TEST_F(state_transition, txcreate_stray_data_initcontainer)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID));

    const auto init_code = returncode(0, 0, 0);
    const auto stray_data = "abcdef"_hex;
    const bytecode init_container =
        eof_bytecode(init_code, 2).container(deploy_container) + stray_data;

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt) +
        OP_DUP1 + push(1) + OP_SSTORE + ret_top();
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[tx.sender].nonce = pre.get(tx.sender).nonce + 1;
    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;  // CREATE caller's nonce must not be bumped
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;  // CREATE must fail
}

TEST_F(state_transition, txcreate_dataloadn_referring_to_auxdata)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = bytes(64, 0);
    const auto aux_data = bytes(32, 0);
    const auto deploy_data_size = static_cast<uint16_t>(deploy_data.size() + aux_data.size());
    // DATALOADN{64} - referring to data that will be appended as aux_data
    const auto deploy_code = bytecode(OP_DATALOADN) + "0040" + ret_top();
    const auto deploy_container = eof_bytecode(deploy_code, 2).data(deploy_data, deploy_data_size);

    const auto init_code = returncode(0, 0, 32);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        sstore(0, txcreate().initcode(keccak256(init_container)).input(0, 0).salt(Salt)) +
        sstore(1, 1) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    const auto expected_container = eof_bytecode(deploy_code, 2).data(deploy_data + aux_data);

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[*tx.to].storage[0x00_bytes32] = to_bytes32(create_address);
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
    expect.post[create_address].code = expected_container;
    expect.post[create_address].nonce = 1;
}

TEST_F(state_transition, txcreate_revert_empty_returndata)
{
    rev = EVMC_EXPERIMENTAL;
    const auto init_code = revert(0, 0);
    const bytecode init_container = eof_bytecode(init_code, 2);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0,
            txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt)) +
        sstore(1, OP_RETURNDATASIZE) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, txcreate_revert_non_empty_returndata)
{
    rev = EVMC_EXPERIMENTAL;
    const auto init_code = mstore8(0, 0xaa) + revert(0, 1);
    const bytecode init_container = eof_bytecode(init_code, 2);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0,
            txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt)) +
        sstore(1, OP_RETURNDATASIZE) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, txcreate_initcontainer_aborts)
{
    rev = EVMC_EXPERIMENTAL;
    const auto init_code = bytecode{Opcode{OP_INVALID}};
    const bytecode init_container = eof_bytecode(init_code, 0);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0,
            txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt)) +
        OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, txcreate_initcontainer_return)
{
    rev = EVMC_EXPERIMENTAL;
    const auto init_code = bytecode{0xaa + ret_top()};
    const bytecode init_container = eof_bytecode(init_code, 2);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0,
            txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt)) +
        OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, txcreate_initcontainer_stop)
{
    rev = EVMC_EXPERIMENTAL;
    const auto init_code = bytecode{Opcode{OP_STOP}};
    const bytecode init_container = eof_bytecode(init_code, 0);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0,
            txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt)) +
        OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, txcreate_initcontainer_max_size)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre.get(tx.sender).balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID));

    const auto init_code = returncode(0, 0, 0);
    const bytecode init_container_no_data = eof_bytecode(init_code, 2).container(deploy_container);
    const auto data_size = 0xc000 - init_container_no_data.size();
    const bytecode init_container =
        eof_bytecode(init_code, 2).container(deploy_container).data(bytes(data_size, 0));
    EXPECT_EQ(init_container.size(), 0xc000);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        txcreate().initcode(keccak256(init_container)).input(0, 0).salt(Salt) + ret_top();
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[create_address].code = deploy_container;
    expect.post[create_address].nonce = 1;
}

TEST_F(state_transition, txcreate_initcontainer_empty)
{
    rev = EVMC_EXPERIMENTAL;

    const bytecode empty_init_container{};

    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID));

    const auto init_code = returncode(0, 0, 0);
    const bytes init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);
    tx.initcodes.push_back(empty_init_container);

    const auto factory_code = txcreate().initcode(keccak256(init_container)) + ret_top();
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.tx_error = INIT_CODE_EMPTY;
    expect.post[*tx.to].exists = true;
}

TEST_F(state_transition, txcreate_no_initcontainer)
{
    rev = EVMC_EXPERIMENTAL;

    tx.type = Transaction::Type::initcodes;

    const auto factory_code = txcreate().initcode(keccak256(bytecode())) + ret_top();
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.tx_error = INIT_CODE_COUNT_ZERO;
    expect.post[*tx.to].exists = true;
}

TEST_F(state_transition, txcreate_initcontainer_too_large)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre.get(tx.sender).balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID));

    const auto init_code = returncode(0, 0, 0);
    const bytecode init_container_no_data = eof_bytecode(init_code, 2).container(deploy_container);
    const auto data_size = 0xc001 - init_container_no_data.size();
    const bytecode init_container =
        eof_bytecode(init_code, 2).container(deploy_container).data(bytes(data_size, 0));
    EXPECT_EQ(init_container.size(), 0xc001);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        txcreate().initcode(keccak256(init_container)).input(0, 0).salt(Salt) + ret_top();
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.tx_error = INIT_CODE_SIZE_LIMIT_EXCEEDED;
    expect.post[*tx.to].exists = true;
}

TEST_F(state_transition, txcreate_too_many_initcontainers)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre.get(tx.sender).balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID));

    const auto init_code = returncode(0, 0, 0);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.assign(257, init_container);

    const auto factory_code =
        txcreate().initcode(keccak256(init_container)).input(0, 0).salt(Salt) + ret_top();
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.tx_error = INIT_CODE_COUNT_LIMIT_EXCEEDED;
    expect.post[*tx.to].exists = true;
}

TEST_F(state_transition, initcode_transaction_before_prague)
{
    rev = EVMC_CANCUN;

    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID));

    const auto init_code = returncode(0, 0, 0);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.assign(257, init_container);

    tx.to = To;

    expect.tx_error = TX_TYPE_NOT_SUPPORTED;
}

TEST_F(state_transition, txcreate_deploy_container_max_size)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre.get(tx.sender).balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const auto eof_header_size =
        static_cast<int>(bytecode{eof_bytecode(Opcode{OP_INVALID})}.size() - 1);
    const auto deploy_code = (0x5fff - eof_header_size) * bytecode{Opcode{OP_JUMPDEST}} + OP_STOP;
    const bytecode deploy_container = eof_bytecode(deploy_code);
    EXPECT_EQ(deploy_container.size(), 0x6000);

    // no aux data
    const auto init_code = returncode(0, 0, 0);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0,
            txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt)) +
        OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[*tx.to].storage[0x00_bytes32] = to_bytes32(create_address);
    expect.post[create_address].code = deploy_container;
}

TEST_F(state_transition, txcreate_deploy_container_too_large)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre.get(tx.sender).balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const auto eof_header_size =
        static_cast<int>(bytecode{eof_bytecode(Opcode{OP_INVALID})}.size() - 1);
    const auto deploy_code = (0x6000 - eof_header_size) * bytecode{Opcode{OP_JUMPDEST}} + OP_STOP;
    const bytecode deploy_container = eof_bytecode(deploy_code);
    EXPECT_EQ(deploy_container.size(), 0x6001);

    // no aux data
    const auto init_code = returncode(0, 0, 0);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0,
            txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt)) +
        OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, txcreate_appended_data_size_larger_than_64K)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre.get(tx.sender).balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const auto aux_data = bytes(std::numeric_limits<uint16_t>::max(), 0);
    const auto deploy_data = "aa"_hex;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID)).data(deploy_data);

    const auto init_code = calldatacopy(0, 0, OP_CALLDATASIZE) + returncode(0, 0, OP_CALLDATASIZE);
    const bytecode init_container = eof_bytecode(init_code, 3).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    static constexpr bytes32 salt2{0xfe};
    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        // with aux data, final data size = 2**16
        sstore(0,
            txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt)) +
        // no aux_data - final data size = 1
        sstore(1, txcreate().initcode(keccak256(init_container)).salt(salt2)) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    tx.data = aux_data;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 2;  // 1 successful creation + 1 hard fail
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
    const auto create_address = compute_eofcreate_address(*tx.to, salt2);
    expect.post[*tx.to].storage[0x01_bytes32] = to_bytes32(create_address);
    expect.post[create_address].code = deploy_container;
    expect.post[create_address].nonce = 1;
}

TEST_F(state_transition, txcreate_deploy_container_with_aux_data_too_large)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre.get(tx.sender).balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const auto eof_header_size =
        static_cast<int>(bytecode{eof_bytecode(Opcode{OP_INVALID})}.size() - 1);
    const auto deploy_code = (0x5fff - eof_header_size) * bytecode{Opcode{OP_JUMPDEST}} + OP_STOP;
    const bytecode deploy_container = eof_bytecode(deploy_code);
    EXPECT_EQ(deploy_container.size(), 0x6000);

    // 1 byte aux data
    const auto init_code = returncode(0, 0, 1);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0,
            txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt)) +
        OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, txcreate_nested_txcreate)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = "abcdef"_hex;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID)).data(deploy_data);

    const auto deploy_data_nested = "ffffff"_hex;
    const auto deploy_container_nested =
        eof_bytecode(bytecode(OP_INVALID)).data(deploy_data_nested);

    const auto init_code_nested = returncode(0, 0, 0);
    const bytecode init_container_nested =
        eof_bytecode(init_code_nested, 2).container(deploy_container_nested);

    const auto init_code =
        sstore(0, txcreate().initcode(keccak256(init_container_nested)).salt(Salt)) +
        returncode(0, 0, 0);
    const bytecode init_container = eof_bytecode(init_code, 5).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);
    tx.initcodes.push_back(init_container_nested);

    const auto factory_code =
        sstore(0, txcreate().initcode(keccak256(init_container)).salt(Salt)) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[*tx.to].storage[0x00_bytes32] = to_bytes32(create_address);
    expect.post[create_address].code = deploy_container;
    expect.post[create_address].nonce = 2;
    const auto create_address_nested = compute_eofcreate_address(create_address, Salt);
    expect.post[create_address].storage[0x00_bytes32] = to_bytes32(create_address_nested);
    expect.post[create_address_nested].code = deploy_container_nested;
    expect.post[create_address_nested].nonce = 1;
}

TEST_F(state_transition, txcreate_nested_txcreate_revert)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data_nested = "ffffff"_hex;
    const auto deploy_container_nested =
        eof_bytecode(bytecode(OP_INVALID)).data(deploy_data_nested);

    const auto init_code_nested = returncode(0, 0, 0);
    const bytecode init_container_nested =
        eof_bytecode(init_code_nested, 2).container(deploy_container_nested);

    const auto init_code =
        sstore(0, txcreate().initcode(keccak256(init_container_nested)).salt(Salt)) + revert(0, 0);
    const bytecode init_container = eof_bytecode(init_code, 5);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);
    tx.initcodes.push_back(init_container_nested);

    const auto factory_code =
        sstore(0, txcreate().initcode(keccak256(init_container)).salt(Salt)) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, txcreate_nested_eofcreate)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = "abcdef"_hex;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID)).data(deploy_data);

    const auto deploy_data_nested = "ffffff"_hex;
    const auto deploy_container_nested =
        eof_bytecode(bytecode(OP_INVALID)).data(deploy_data_nested);

    const auto init_code_nested = returncode(0, 0, 0);
    const bytecode init_container_nested =
        eof_bytecode(init_code_nested, 2).container(deploy_container_nested);

    const auto init_code = sstore(0, eofcreate().container(1).salt(Salt)) + returncode(0, 0, 0);
    const bytecode init_container =
        eof_bytecode(init_code, 4).container(deploy_container).container(init_container_nested);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        sstore(0, txcreate().initcode(keccak256(init_container)).salt(Salt)) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[*tx.to].storage[0x00_bytes32] = to_bytes32(create_address);
    expect.post[create_address].code = deploy_container;
    expect.post[create_address].nonce = 2;
    const auto create_address_nested = compute_eofcreate_address(create_address, Salt);
    expect.post[create_address].storage[0x00_bytes32] = to_bytes32(create_address_nested);
    expect.post[create_address_nested].code = deploy_container_nested;
    expect.post[create_address_nested].nonce = 1;
}

TEST_F(state_transition, txcreate_called_balance_too_low)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = "abcdef"_hex;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID)).data(deploy_data);

    const auto init_code = calldatacopy(0, 0, OP_CALLDATASIZE) + returncode(0, 0, OP_CALLDATASIZE);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code = calldatacopy(0, 0, OP_CALLDATASIZE) +
                              sstore(0, txcreate()
                                            .initcode(keccak256(init_container))
                                            .input(0, OP_CALLDATASIZE)
                                            .salt(Salt)
                                            .value(10)) +
                              sstore(1, 1) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, txcreate_clears_returndata)
{
    static constexpr auto returning_address = 0x3000_address;

    rev = EVMC_EXPERIMENTAL;
    const auto deploy_container = eof_bytecode(OP_STOP);

    const auto init_code = returncode(0, 0, 0);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code = sstore(0, extcall(returning_address)) + sstore(1, returndatasize()) +
                              sstore(2, txcreate().initcode(keccak256(init_container)).salt(Salt)) +
                              sstore(3, returndatasize()) + sstore(4, 1) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 5);

    const auto returning_code = ret(0, 10);

    tx.to = To;

    pre[*tx.to] = {.nonce = 1, .code = factory_container};
    pre[returning_address] = {.nonce = 1, .code = returning_code};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x0a_bytes32;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[*tx.to].storage[0x02_bytes32] = to_bytes32(create_address);
    expect.post[*tx.to].storage[0x03_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x04_bytes32] = 0x01_bytes32;
    expect.post[create_address].code = deploy_container;
    expect.post[create_address].nonce = 1;
    expect.post[returning_address].nonce = 1;
}

TEST_F(state_transition, txcreate_failure_after_txcreate_success)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre.get(tx.sender).balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const auto deploy_container = eof_bytecode(OP_STOP);

    const auto init_code = returncode(0, 0, 0);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        sstore(0, txcreate().initcode(keccak256(init_container)).salt(Salt)) +
        sstore(1, txcreate().initcode(keccak256(init_container)).salt(Salt)) +  // address collision
        sstore(2, returndatasize()) + sstore(3, 1) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;

    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 2;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[*tx.to].storage[0x00_bytes32] = to_bytes32(create_address);
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x02_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x03_bytes32] = 0x01_bytes32;
    expect.post[create_address].code = deploy_container;
    expect.post[create_address].nonce = 1;
}

TEST_F(state_transition, txcreate_invalid_initcode)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID));

    const auto init_code = returncode(0, 0, 0);
    const bytes init_container =
        eof_bytecode(init_code, 123).container(deploy_container);  // Invalid EOF

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    // TODO: extract this common code for a testing deployer contract
    const auto factory_code =
        txcreate().initcode(keccak256(init_container)).input(0, 0).salt(Salt) + OP_DUP1 + push(1) +
        OP_SSTORE + ret_top();
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.gas_used = 55756;

    expect.post[tx.sender].nonce = pre.get(tx.sender).nonce + 1;
    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;  // CREATE caller's nonce must not be bumped
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;  // CREATE must fail
}

TEST_F(state_transition, txcreate_truncated_data_initcode)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID));

    const auto init_code = returncode(0, 0, 0);
    const bytes init_container =
        eof_bytecode(init_code, 2).data("", 1).container(deploy_container);  // Truncated data

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    // TODO: extract this common code for a testing deployer contract
    const auto factory_code =
        txcreate().initcode(keccak256(init_container)).input(0, 0).salt(Salt) + OP_DUP1 + push(1) +
        OP_SSTORE + ret_top();
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.gas_used = 55768;

    expect.post[tx.sender].nonce = pre.get(tx.sender).nonce + 1;
    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;  // CREATE caller's nonce must not be bumped
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;  // CREATE must fail
}

TEST_F(state_transition, txcreate_invalid_deploycode)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID), 123);  // Invalid EOF

    const auto init_code = returncode(0, 0, 0);
    const bytes init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        txcreate().initcode(keccak256(init_container)).input(0, 0).salt(Salt) + OP_DUP1 + push(1) +
        OP_SSTORE + ret_top();
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.gas_used = 55768;

    expect.post[tx.sender].nonce = pre.get(tx.sender).nonce + 1;
    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;  // CREATE caller's nonce must not be bumped
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;  // CREATE must fail
}

TEST_F(state_transition, txcreate_missing_initcontainer)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID));

    const auto init_code = returncode(0, 0, 0);
    const bytes init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code = txcreate().initcode(keccak256(bytecode())).input(0, 0).salt(Salt) +
                              OP_DUP1 + push(1) + OP_SSTORE + ret_top();
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.gas_used = 55756;

    expect.post[tx.sender].nonce = pre.get(tx.sender).nonce + 1;
    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;  // CREATE caller's nonce must not be bumped
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;  // CREATE must fail
}

TEST_F(state_transition, txcreate_light_failure_stack)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID));

    const auto init_code = returncode(0, 0, 0);
    const bytes init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        push(0x123) + txcreate().value(1).initcode(keccak256(bytecode())).input(2, 3).salt(Salt) +
        push(1) + OP_SSTORE +  // store result from TXCREATE
        push(2) +
        OP_SSTORE +  // store the preceding push value, nothing else should remain on stack
        ret(0);
    const auto factory_container = eof_bytecode(factory_code, 6);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;  // TXCREATE has pushed 0x0 on stack
    expect.post[*tx.to].storage[0x02_bytes32] =
        0x0123_bytes32;  // TXCREATE fails but has cleared its args first
}

TEST_F(state_transition, txcreate_missing_deploycontainer)
{
    rev = EVMC_EXPERIMENTAL;
    const auto init_code = returncode(0, 0, 0);
    const bytes init_container = eof_bytecode(init_code, 2);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        txcreate().initcode(keccak256(init_container)).input(0, 0).salt(Salt) + OP_DUP1 + push(1) +
        OP_SSTORE + ret_top();
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.gas_used = 55492;

    expect.post[tx.sender].nonce = pre.get(tx.sender).nonce + 1;
    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;  // CREATE caller's nonce must not be bumped
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;  // CREATE must fail
}

TEST_F(state_transition, txcreate_deploy_code_with_dataloadn_invalid)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = bytes(32, 0);
    // DATALOADN{64} - referring to offset out of bounds even after appending aux_data later
    const auto deploy_code = bytecode(OP_DATALOADN) + "0040" + ret_top();
    const auto aux_data = bytes(32, 0);
    const auto deploy_data_size = static_cast<uint16_t>(deploy_data.size() + aux_data.size());
    const auto deploy_container = eof_bytecode(deploy_code, 2).data(deploy_data, deploy_data_size);

    const auto init_code = returncode(0, 0, 0);
    const bytes init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        txcreate().initcode(keccak256(init_container)).input(0, 0).salt(Salt) + OP_DUP1 + push(1) +
        OP_SSTORE + ret_top();
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.gas_used = 56032;

    expect.post[tx.sender].nonce = pre.get(tx.sender).nonce + 1;
    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;  // CREATE caller's nonce must not be bumped
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;  // CREATE must fail
}

TEST_F(state_transition, txcreate_call_created_contract)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = "abcdef"_hex;  // 3 bytes
    const auto static_aux_data =
        "aabbccdd00000000000000000000000000000000000000000000000000000000"_hex;  // 32 bytes
    const auto dynamic_aux_data = "eeff"_hex;                                    // 2 bytes
    const auto deploy_data_size =
        static_cast<uint16_t>(deploy_data.size() + static_aux_data.size());
    const auto deploy_code = rjumpv({6, 12}, calldataload(0)) +  // jump to one of 3 cases
                             35 + OP_DATALOAD + rjump(9) +       // read dynamic aux data
                             OP_DATALOADN + "0000" + rjump(3) +  // read pre_deploy_data_section
                             OP_DATALOADN + "0003" +             // read static aux data
                             ret_top();
    const auto deploy_container = eof_bytecode(deploy_code, 2).data(deploy_data, deploy_data_size);

    const auto init_code = calldatacopy(0, 0, OP_CALLDATASIZE) + returncode(0, 0, OP_CALLDATASIZE);
    const bytecode init_container = eof_bytecode(init_code, 3).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto create_address = compute_eofcreate_address(To, Salt);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0,
            txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt)) +
        mcopy(0, OP_CALLDATASIZE, 32) +        // zero out first 32-byte word of memory
        extcall(create_address).input(0, 1) +  // calldata 0
        OP_POP + sstore(1, returndataload(0)) + mstore8(31, 1) +
        extcall(create_address).input(0, 32) +  // calldata 1
        OP_POP + sstore(2, returndataload(0)) + mstore8(31, 2) +
        extcall(create_address).input(0, 32) +  // calldata 2
        OP_POP + sstore(3, returndataload(0)) + sstore(4, 1) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;

    tx.data = static_aux_data + dynamic_aux_data;

    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = to_bytes32(create_address);
    expect.post[*tx.to].storage[0x01_bytes32] =
        0xabcdefaabbccdd00000000000000000000000000000000000000000000000000_bytes32;
    evmc::bytes32 static_aux_data_32;
    std::copy_n(static_aux_data.data(), static_aux_data.size(), &static_aux_data_32.bytes[0]);
    expect.post[*tx.to].storage[0x02_bytes32] = static_aux_data_32;
    evmc::bytes32 dynamic_aux_data_32;
    std::copy_n(dynamic_aux_data.data(), dynamic_aux_data.size(), &dynamic_aux_data_32.bytes[0]);
    expect.post[*tx.to].storage[0x03_bytes32] = dynamic_aux_data_32;
    expect.post[*tx.to].storage[0x04_bytes32] = 0x01_bytes32;
    expect.post[create_address].nonce = 1;
}


TEST_F(state_transition, create_nested_in_txcreate)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_container = eof_bytecode(OP_STOP);

    const auto init_code = bytecode{OP_DATASIZE} + OP_PUSH0 + OP_PUSH0 + OP_DATACOPY +
                           create().input(0, OP_DATASIZE) + returncode(0, 0, 0);
    const bytes init_container =
        eof_bytecode(init_code, 3).container(deploy_container).data(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        txcreate().initcode(keccak256(init_container)).salt(Salt) + push(1) + OP_SSTORE + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[tx.sender].nonce = pre.get(tx.sender).nonce + 1;
    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, create2_nested_in_txcreate)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_container = eof_bytecode(OP_INVALID);

    const auto init_code = bytecode{OP_DATASIZE} + OP_PUSH0 + OP_PUSH0 + OP_DATACOPY +
                           create2().input(0, OP_DATASIZE).salt(Salt) + returncode(0, 0, 0);
    const bytes init_container =
        eof_bytecode(init_code, 4).container(deploy_container).data(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        txcreate().initcode(keccak256(init_container)).input(0, 0).salt(Salt) + push(1) +
        OP_SSTORE + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[tx.sender].nonce = pre.get(tx.sender).nonce + 1;
    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, txcreate_from_legacy_tx)
{
    rev = EVMC_EXPERIMENTAL;
    tx.type = Transaction::Type::legacy;

    const auto factory_code = sstore(0, txcreate().initcode(keccak256({})).input(0, 0).salt(Salt)) +
                              sstore(1, 1) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[tx.sender].nonce = pre.get(tx.sender).nonce + 1;
    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;  // CREATE caller's nonce must not be bumped
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;  // CREATE must fail
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, txcreate_from_1559_tx)
{
    rev = EVMC_EXPERIMENTAL;
    tx.type = Transaction::Type::eip1559;

    const auto factory_code = sstore(0, txcreate().initcode(keccak256({})).input(0, 0).salt(Salt)) +
                              sstore(1, 1) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[tx.sender].nonce = pre.get(tx.sender).nonce + 1;
    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;  // CREATE caller's nonce must not be bumped
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;  // CREATE must fail
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, txcreate_from_blob_tx)
{
    rev = EVMC_EXPERIMENTAL;

    block.blob_base_fee = 1;

    tx.type = Transaction::Type::blob;
    tx.blob_hashes.push_back(
        0x0100000000000000000000000000000000000000000000000000000000000007_bytes32);

    const auto factory_code = sstore(0, txcreate().initcode(keccak256({})).input(0, 0).salt(Salt)) +
                              sstore(1, 1) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 5);

    tx.to = To;
    tx.max_blob_gas_price = *block.blob_base_fee;
    pre.get(tx.sender).balance =
        GAS_PER_BLOB * tx.max_blob_gas_price + tx.gas_limit * tx.max_gas_price;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[tx.sender].nonce = pre.get(tx.sender).nonce + 1;
    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;  // CREATE caller's nonce must not be bumped
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;  // CREATE must fail
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, txcreate_loop_valid)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = MAX_TX_GAS_LIMIT;
    tx.gas_limit = block.gas_limit;
    pre.get(tx.sender).balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID));

    const auto init_code = returncode(0, 0, 0);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    constexpr auto iterations = 400;
    const auto initcode_hash = keccak256(init_container);
    const auto factory_code =
        push0() +                                             // initial salt
        push(1) + OP_ADD +                                    // increment salt
        OP_DUP1 + push(iterations + 1) + rjumpi(45, OP_EQ) +  // if (salt == iterations + 1) break;
        txcreate().value(0).initcode(initcode_hash).input(0, 0).salt(OP_DUP4) + OP_POP +
        rjump(-56) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 6);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + iterations;
    for (uint16_t salt = 1; salt <= iterations; ++salt)
    {
        const auto create_address = compute_eofcreate_address(*tx.to, bytes32{salt});
        expect.post[create_address].code = deploy_container;
        expect.post[create_address].nonce = 1;
    }
}

TEST_F(state_transition, txcreate_loop_invalid)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = MAX_TX_GAS_LIMIT;
    tx.gas_limit = block.gas_limit;
    pre.get(tx.sender).balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const bytecode init_container = OP_STOP;  // invalid EOF

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    constexpr auto iterations = 500;
    const auto initcode_hash = keccak256(init_container);
    const auto factory_code =
        push0() +                                             // initial salt
        push(1) + OP_ADD +                                    // increment salt
        OP_DUP1 + push(iterations + 1) + rjumpi(45, OP_EQ) +  // if (salt == iterations + 1) break;
        txcreate().value(0).initcode(initcode_hash).input(0, 0).salt(OP_DUP4) + OP_POP +
        rjump(-56) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 6);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;
}

TEST_F(state_transition, legacy_txcreate_empty_auxdata)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = "abcdef"_hex;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID)).data(deploy_data);

    const auto init_code = returncode(0, 0, 0);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        txcreate().initcode(keccak256(init_container)).input(0, 0).salt(Salt) + ret_top();

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[create_address].code = deploy_container;
    expect.post[create_address].nonce = 1;
}

TEST_F(state_transition, legacy_txcreate_auxdata_equal_to_declared)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = "abcdef"_hex;
    const auto aux_data = "aabbccddeeff"_hex;
    const auto deploy_data_size = static_cast<uint16_t>(deploy_data.size() + aux_data.size());
    const auto deploy_container =
        eof_bytecode(bytecode(OP_INVALID)).data(deploy_data, deploy_data_size);

    const auto init_code = calldatacopy(0, 0, OP_CALLDATASIZE) + returncode(0, 0, OP_CALLDATASIZE);
    const bytecode init_container = eof_bytecode(init_code, 3).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt) +
        ret_top();

    tx.to = To;
    tx.data = aux_data;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    const auto expected_container = eof_bytecode(bytecode(OP_INVALID)).data(deploy_data + aux_data);

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[create_address].code = expected_container;
    expect.post[create_address].nonce = 1;
}

TEST_F(state_transition, legacy_txcreate_auxdata_longer_than_declared)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = "abcdef"_hex;
    const auto aux_data1 = "aabbccdd"_hex;
    const auto aux_data2 = "eeff"_hex;
    const auto deploy_data_size = static_cast<uint16_t>(deploy_data.size() + aux_data1.size());
    const auto deploy_container =
        eof_bytecode(bytecode(OP_INVALID)).data(deploy_data, deploy_data_size);

    const auto init_code = calldatacopy(0, 0, OP_CALLDATASIZE) + returncode(0, 0, OP_CALLDATASIZE);
    const bytecode init_container = eof_bytecode(init_code, 3).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt) +
        ret_top();

    tx.to = To;
    tx.data = aux_data1 + aux_data2;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    const auto expected_container =
        eof_bytecode(bytecode(OP_INVALID)).data(deploy_data + aux_data1 + aux_data2);

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[create_address].code = expected_container;
    expect.post[create_address].nonce = 1;
}

TEST_F(state_transition, legacy_txcreate_auxdata_shorter_than_declared)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = "abcdef"_hex;
    const auto aux_data = "aabbccddeeff"_hex;
    const auto deploy_data_size = static_cast<uint16_t>(deploy_data.size() + aux_data.size() + 1);
    const auto deploy_container =
        eof_bytecode(bytecode(OP_INVALID)).data(deploy_data, deploy_data_size);

    const auto init_code = calldatacopy(0, 0, OP_CALLDATASIZE) + returncode(0, 0, OP_CALLDATASIZE);
    const bytecode init_container = eof_bytecode(init_code, 3).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt) +
        ret_top();

    tx.to = To;
    tx.data = aux_data;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
}

TEST_F(state_transition, legacy_txcreate_stray_data_initcontainer)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID));

    const auto init_code = returncode(0, 0, 0);
    const auto stray_data = "abcdef"_hex;
    const bytecode init_container =
        eof_bytecode(init_code, 2).container(deploy_container) + stray_data;

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt) +
        OP_DUP1 + push(1) + OP_SSTORE + ret_top();

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[tx.sender].nonce = pre.get(tx.sender).nonce + 1;
    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;  // CREATE caller's nonce must not be bumped
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;  // CREATE must fail
}

TEST_F(state_transition, legacy_txcreate_dataloadn_referring_to_auxdata)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = bytes(64, 0);
    const auto aux_data = bytes(32, 0);
    const auto deploy_data_size = static_cast<uint16_t>(deploy_data.size() + aux_data.size());
    // DATALOADN{64} - referring to data that will be appended as aux_data
    const auto deploy_code = bytecode(OP_DATALOADN) + "0040" + ret_top();
    const auto deploy_container = eof_bytecode(deploy_code, 2).data(deploy_data, deploy_data_size);

    const auto init_code = returncode(0, 0, 32);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        sstore(0, txcreate().initcode(keccak256(init_container)).input(0, 0).salt(Salt)) +
        sstore(1, 1) + OP_STOP;

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    const auto expected_container = eof_bytecode(deploy_code, 2).data(deploy_data + aux_data);

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[*tx.to].storage[0x00_bytes32] = to_bytes32(create_address);
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
    expect.post[create_address].code = expected_container;
    expect.post[create_address].nonce = 1;
}

TEST_F(state_transition, legacy_txcreate_revert_empty_returndata)
{
    rev = EVMC_EXPERIMENTAL;
    const auto init_code = revert(0, 0);
    const bytecode init_container = eof_bytecode(init_code, 2);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0,
            txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt)) +
        sstore(1, OP_RETURNDATASIZE) + OP_STOP;

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, legacy_txcreate_revert_non_empty_returndata)
{
    rev = EVMC_EXPERIMENTAL;
    const auto init_code = mstore8(0, 0xaa) + revert(0, 1);
    const bytecode init_container = eof_bytecode(init_code, 2);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0,
            txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt)) +
        sstore(1, OP_RETURNDATASIZE) + OP_STOP;

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, legacy_txcreate_initcontainer_aborts)
{
    rev = EVMC_EXPERIMENTAL;
    const auto init_code = bytecode{Opcode{OP_INVALID}};
    const bytecode init_container = eof_bytecode(init_code, 0);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0,
            txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt)) +
        OP_STOP;

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, legacy_txcreate_initcontainer_return)
{
    rev = EVMC_EXPERIMENTAL;
    const auto init_code = bytecode{0xaa + ret_top()};
    const bytecode init_container = eof_bytecode(init_code, 2);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0,
            txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt)) +
        OP_STOP;

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, legacy_txcreate_initcontainer_stop)
{
    rev = EVMC_EXPERIMENTAL;
    const auto init_code = bytecode{Opcode{OP_STOP}};
    const bytecode init_container = eof_bytecode(init_code, 0);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0,
            txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt)) +
        OP_STOP;

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, legacy_txcreate_initcontainer_max_size)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre.get(tx.sender).balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID));

    const auto init_code = returncode(0, 0, 0);
    const bytecode init_container_no_data = eof_bytecode(init_code, 2).container(deploy_container);
    const auto data_size = 0xc000 - init_container_no_data.size();
    const bytecode init_container =
        eof_bytecode(init_code, 2).container(deploy_container).data(bytes(data_size, 0));
    EXPECT_EQ(init_container.size(), 0xc000);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        txcreate().initcode(keccak256(init_container)).input(0, 0).salt(Salt) + ret_top();

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[create_address].code = deploy_container;
    expect.post[create_address].nonce = 1;
}

TEST_F(state_transition, legacy_txcreate_initcontainer_empty)
{
    rev = EVMC_EXPERIMENTAL;

    const bytecode empty_init_container{};

    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID));

    const auto init_code = returncode(0, 0, 0);
    const bytes init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);
    tx.initcodes.push_back(empty_init_container);

    const auto factory_code = txcreate().initcode(keccak256(init_container)) + ret_top();

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.tx_error = INIT_CODE_EMPTY;
    expect.post[*tx.to].exists = true;
}

TEST_F(state_transition, legacy_txcreate_no_initcontainer)
{
    rev = EVMC_EXPERIMENTAL;

    tx.type = Transaction::Type::initcodes;

    const auto factory_code = txcreate().initcode(keccak256(bytecode())) + ret_top();

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.tx_error = INIT_CODE_COUNT_ZERO;
    expect.post[*tx.to].exists = true;
}

TEST_F(state_transition, legacy_txcreate_initcontainer_too_large)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre.get(tx.sender).balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID));

    const auto init_code = returncode(0, 0, 0);
    const bytecode init_container_no_data = eof_bytecode(init_code, 2).container(deploy_container);
    const auto data_size = 0xc001 - init_container_no_data.size();
    const bytecode init_container =
        eof_bytecode(init_code, 2).container(deploy_container).data(bytes(data_size, 0));
    EXPECT_EQ(init_container.size(), 0xc001);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        txcreate().initcode(keccak256(init_container)).input(0, 0).salt(Salt) + ret_top();

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.tx_error = INIT_CODE_SIZE_LIMIT_EXCEEDED;
    expect.post[*tx.to].exists = true;
}

TEST_F(state_transition, legacy_txcreate_too_many_initcontainers)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre.get(tx.sender).balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID));

    const auto init_code = returncode(0, 0, 0);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.assign(257, init_container);

    const auto factory_code =
        txcreate().initcode(keccak256(init_container)).input(0, 0).salt(Salt) + ret_top();

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.tx_error = INIT_CODE_COUNT_LIMIT_EXCEEDED;
    expect.post[*tx.to].exists = true;
}

TEST_F(state_transition, legacy_initcode_transaction_before_prague)
{
    rev = EVMC_CANCUN;

    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID));

    const auto init_code = returncode(0, 0, 0);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.assign(257, init_container);

    tx.to = To;

    expect.tx_error = TX_TYPE_NOT_SUPPORTED;
}

TEST_F(state_transition, legacy_txcreate_deploy_container_max_size)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre.get(tx.sender).balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const auto eof_header_size =
        static_cast<int>(bytecode{eof_bytecode(Opcode{OP_INVALID})}.size() - 1);
    const auto deploy_code = (0x5fff - eof_header_size) * bytecode{Opcode{OP_JUMPDEST}} + OP_STOP;
    const bytecode deploy_container = eof_bytecode(deploy_code);
    EXPECT_EQ(deploy_container.size(), 0x6000);

    // no aux data
    const auto init_code = returncode(0, 0, 0);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0,
            txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt)) +
        OP_STOP;

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[*tx.to].storage[0x00_bytes32] = to_bytes32(create_address);
    expect.post[create_address].code = deploy_container;
}

TEST_F(state_transition, legacy_txcreate_deploy_container_too_large)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre.get(tx.sender).balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const auto eof_header_size =
        static_cast<int>(bytecode{eof_bytecode(Opcode{OP_INVALID})}.size() - 1);
    const auto deploy_code = (0x6000 - eof_header_size) * bytecode{Opcode{OP_JUMPDEST}} + OP_STOP;
    const bytecode deploy_container = eof_bytecode(deploy_code);
    EXPECT_EQ(deploy_container.size(), 0x6001);

    // no aux data
    const auto init_code = returncode(0, 0, 0);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0,
            txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt)) +
        OP_STOP;

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, legacy_txcreate_appended_data_size_larger_than_64K)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre.get(tx.sender).balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const auto aux_data = bytes(std::numeric_limits<uint16_t>::max(), 0);
    const auto deploy_data = "aa"_hex;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID)).data(deploy_data);

    const auto init_code = calldatacopy(0, 0, OP_CALLDATASIZE) + returncode(0, 0, OP_CALLDATASIZE);
    const bytecode init_container = eof_bytecode(init_code, 3).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    static constexpr bytes32 salt2{0xfe};
    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        // with aux data, final data size = 2**16
        sstore(0,
            txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt)) +
        // no aux_data - final data size = 1
        sstore(1, txcreate().initcode(keccak256(init_container)).salt(salt2)) + OP_STOP;

    tx.to = To;
    tx.data = aux_data;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 2;  // 1 successful creation + 1 hard fail
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
    const auto create_address = compute_eofcreate_address(*tx.to, salt2);
    expect.post[*tx.to].storage[0x01_bytes32] = to_bytes32(create_address);
    expect.post[create_address].code = deploy_container;
    expect.post[create_address].nonce = 1;
}

TEST_F(state_transition, legacy_txcreate_deploy_container_with_aux_data_too_large)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre.get(tx.sender).balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const auto eof_header_size =
        static_cast<int>(bytecode{eof_bytecode(Opcode{OP_INVALID})}.size() - 1);
    const auto deploy_code = (0x5fff - eof_header_size) * bytecode{Opcode{OP_JUMPDEST}} + OP_STOP;
    const bytecode deploy_container = eof_bytecode(deploy_code);
    EXPECT_EQ(deploy_container.size(), 0x6000);

    // 1 byte aux data
    const auto init_code = returncode(0, 0, 1);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0,
            txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt)) +
        OP_STOP;

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, legacy_txcreate_nested_txcreate)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = "abcdef"_hex;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID)).data(deploy_data);

    const auto deploy_data_nested = "ffffff"_hex;
    const auto deploy_container_nested =
        eof_bytecode(bytecode(OP_INVALID)).data(deploy_data_nested);

    const auto init_code_nested = returncode(0, 0, 0);
    const bytecode init_container_nested =
        eof_bytecode(init_code_nested, 2).container(deploy_container_nested);

    const auto init_code =
        sstore(0, txcreate().initcode(keccak256(init_container_nested)).salt(Salt)) +
        returncode(0, 0, 0);
    const bytecode init_container = eof_bytecode(init_code, 5).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);
    tx.initcodes.push_back(init_container_nested);

    const auto factory_code =
        sstore(0, txcreate().initcode(keccak256(init_container)).salt(Salt)) + OP_STOP;

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[*tx.to].storage[0x00_bytes32] = to_bytes32(create_address);
    expect.post[create_address].code = deploy_container;
    expect.post[create_address].nonce = 2;
    const auto create_address_nested = compute_eofcreate_address(create_address, Salt);
    expect.post[create_address].storage[0x00_bytes32] = to_bytes32(create_address_nested);
    expect.post[create_address_nested].code = deploy_container_nested;
    expect.post[create_address_nested].nonce = 1;
}

TEST_F(state_transition, legacy_txcreate_nested_txcreate_revert)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data_nested = "ffffff"_hex;
    const auto deploy_container_nested =
        eof_bytecode(bytecode(OP_INVALID)).data(deploy_data_nested);

    const auto init_code_nested = returncode(0, 0, 0);
    const bytecode init_container_nested =
        eof_bytecode(init_code_nested, 2).container(deploy_container_nested);

    const auto init_code =
        sstore(0, txcreate().initcode(keccak256(init_container_nested)).salt(Salt)) + revert(0, 0);
    const bytecode init_container = eof_bytecode(init_code, 5);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);
    tx.initcodes.push_back(init_container_nested);

    const auto factory_code =
        sstore(0, txcreate().initcode(keccak256(init_container)).salt(Salt)) + OP_STOP;

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, legacy_txcreate_nested_eofcreate)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = "abcdef"_hex;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID)).data(deploy_data);

    const auto deploy_data_nested = "ffffff"_hex;
    const auto deploy_container_nested =
        eof_bytecode(bytecode(OP_INVALID)).data(deploy_data_nested);

    const auto init_code_nested = returncode(0, 0, 0);
    const bytecode init_container_nested =
        eof_bytecode(init_code_nested, 2).container(deploy_container_nested);

    const auto init_code = sstore(0, eofcreate().container(1).salt(Salt)) + returncode(0, 0, 0);
    const bytecode init_container =
        eof_bytecode(init_code, 4).container(deploy_container).container(init_container_nested);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        sstore(0, txcreate().initcode(keccak256(init_container)).salt(Salt)) + OP_STOP;

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[*tx.to].storage[0x00_bytes32] = to_bytes32(create_address);
    expect.post[create_address].code = deploy_container;
    expect.post[create_address].nonce = 2;
    const auto create_address_nested = compute_eofcreate_address(create_address, Salt);
    expect.post[create_address].storage[0x00_bytes32] = to_bytes32(create_address_nested);
    expect.post[create_address_nested].code = deploy_container_nested;
    expect.post[create_address_nested].nonce = 1;
}

TEST_F(state_transition, legacy_txcreate_called_balance_too_low)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = "abcdef"_hex;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID)).data(deploy_data);

    const auto init_code = calldatacopy(0, 0, OP_CALLDATASIZE) + returncode(0, 0, OP_CALLDATASIZE);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code = calldatacopy(0, 0, OP_CALLDATASIZE) +
                              sstore(0, txcreate()
                                            .initcode(keccak256(init_container))
                                            .input(0, OP_CALLDATASIZE)
                                            .salt(Salt)
                                            .value(10)) +
                              sstore(1, 1) + OP_STOP;

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, legacy_txcreate_clears_returndata)
{
    static constexpr auto returning_address = 0x3000_address;

    rev = EVMC_EXPERIMENTAL;
    const auto deploy_container = eof_bytecode(OP_STOP);

    const auto init_code = returncode(0, 0, 0);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code = sstore(0, call(returning_address).gas(OP_GAS)) +
                              sstore(1, returndatasize()) +
                              sstore(2, txcreate().initcode(keccak256(init_container)).salt(Salt)) +
                              sstore(3, returndatasize()) + sstore(4, 1) + OP_STOP;

    const auto returning_code = ret(0, 10);

    tx.to = To;

    pre[*tx.to] = {.nonce = 1, .code = factory_code};
    pre[returning_address] = {.nonce = 1, .code = returning_code};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x01_bytes32;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x0a_bytes32;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[*tx.to].storage[0x02_bytes32] = to_bytes32(create_address);
    expect.post[*tx.to].storage[0x03_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x04_bytes32] = 0x01_bytes32;
    expect.post[create_address].code = deploy_container;
    expect.post[create_address].nonce = 1;
    expect.post[returning_address].nonce = 1;
}

TEST_F(state_transition, legacy_txcreate_failure_after_txcreate_success)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre.get(tx.sender).balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const auto deploy_container = eof_bytecode(OP_STOP);

    const auto init_code = returncode(0, 0, 0);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        sstore(0, txcreate().initcode(keccak256(init_container)).salt(Salt)) +
        sstore(1, txcreate().initcode(keccak256(init_container)).salt(Salt)) +  // address collision
        sstore(2, returndatasize()) + sstore(3, 1) + OP_STOP;

    tx.to = To;

    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 2;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[*tx.to].storage[0x00_bytes32] = to_bytes32(create_address);
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x02_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x03_bytes32] = 0x01_bytes32;
    expect.post[create_address].code = deploy_container;
    expect.post[create_address].nonce = 1;
}

TEST_F(state_transition, legacy_txcreate_invalid_initcode)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID));

    const auto init_code = returncode(0, 0, 0);
    const bytes init_container =
        eof_bytecode(init_code, 123).container(deploy_container);  // Invalid EOF

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    // TODO: extract this common code for a testing deployer contract
    const auto factory_code =
        txcreate().initcode(keccak256(init_container)).input(0, 0).salt(Salt) + OP_DUP1 + push(1) +
        OP_SSTORE + ret_top();

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.gas_used = 55756;

    expect.post[tx.sender].nonce = pre.get(tx.sender).nonce + 1;
    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;  // CREATE caller's nonce must not be bumped
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;  // CREATE must fail
}

TEST_F(state_transition, legacy_txcreate_truncated_data_initcode)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID));

    const auto init_code = returncode(0, 0, 0);
    const bytes init_container =
        eof_bytecode(init_code, 2).data("", 1).container(deploy_container);  // Truncated data

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    // TODO: extract this common code for a testing deployer contract
    const auto factory_code =
        txcreate().initcode(keccak256(init_container)).input(0, 0).salt(Salt) + OP_DUP1 + push(1) +
        OP_SSTORE + ret_top();

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.gas_used = 55768;

    expect.post[tx.sender].nonce = pre.get(tx.sender).nonce + 1;
    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;  // CREATE caller's nonce must not be bumped
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;  // CREATE must fail
}

TEST_F(state_transition, legacy_txcreate_invalid_deploycode)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID), 123);  // Invalid EOF

    const auto init_code = returncode(0, 0, 0);
    const bytes init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        txcreate().initcode(keccak256(init_container)).input(0, 0).salt(Salt) + OP_DUP1 + push(1) +
        OP_SSTORE + ret_top();

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.gas_used = 55768;

    expect.post[tx.sender].nonce = pre.get(tx.sender).nonce + 1;
    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;  // CREATE caller's nonce must not be bumped
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;  // CREATE must fail
}

TEST_F(state_transition, legacy_txcreate_missing_initcontainer)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID));

    const auto init_code = returncode(0, 0, 0);
    const bytes init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code = txcreate().initcode(keccak256(bytecode())).input(0, 0).salt(Salt) +
                              OP_DUP1 + push(1) + OP_SSTORE + ret_top();

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.gas_used = 55756;

    expect.post[tx.sender].nonce = pre.get(tx.sender).nonce + 1;
    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;  // CREATE caller's nonce must not be bumped
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;  // CREATE must fail
}

TEST_F(state_transition, legacy_txcreate_light_failure_stack)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID));

    const auto init_code = returncode(0, 0, 0);
    const bytes init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        push(0x123) + txcreate().value(1).initcode(keccak256(bytecode())).input(2, 3).salt(Salt) +
        push(1) + OP_SSTORE +  // store result from TXCREATE
        push(2) +
        OP_SSTORE +  // store the preceding push value, nothing else should remain on stack
        ret(0);
    const auto factory_container = eof_bytecode(factory_code, 6);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;  // TXCREATE has pushed 0x0 on stack
    expect.post[*tx.to].storage[0x02_bytes32] =
        0x0123_bytes32;  // TXCREATE fails but has cleared its args first
}

TEST_F(state_transition, legacy_txcreate_missing_deploycontainer)
{
    rev = EVMC_EXPERIMENTAL;
    const auto init_code = returncode(0, 0, 0);
    const bytes init_container = eof_bytecode(init_code, 2);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        txcreate().initcode(keccak256(init_container)).input(0, 0).salt(Salt) + OP_DUP1 + push(1) +
        OP_SSTORE + ret_top();

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.gas_used = 55492;

    expect.post[tx.sender].nonce = pre.get(tx.sender).nonce + 1;
    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;  // CREATE caller's nonce must not be bumped
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;  // CREATE must fail
}

TEST_F(state_transition, legacy_txcreate_deploy_code_with_dataloadn_invalid)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = bytes(32, 0);
    // DATALOADN{64} - referring to offset out of bounds even after appending aux_data later
    const auto deploy_code = bytecode(OP_DATALOADN) + "0040" + ret_top();
    const auto aux_data = bytes(32, 0);
    const auto deploy_data_size = static_cast<uint16_t>(deploy_data.size() + aux_data.size());
    const auto deploy_container = eof_bytecode(deploy_code, 2).data(deploy_data, deploy_data_size);

    const auto init_code = returncode(0, 0, 0);
    const bytes init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        txcreate().initcode(keccak256(init_container)).input(0, 0).salt(Salt) + OP_DUP1 + push(1) +
        OP_SSTORE + ret_top();

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.gas_used = 56032;

    expect.post[tx.sender].nonce = pre.get(tx.sender).nonce + 1;
    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;  // CREATE caller's nonce must not be bumped
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;  // CREATE must fail
}

TEST_F(state_transition, legacy_txcreate_call_created_contract)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = "abcdef"_hex;  // 3 bytes
    const auto static_aux_data =
        "aabbccdd00000000000000000000000000000000000000000000000000000000"_hex;  // 32 bytes
    const auto dynamic_aux_data = "eeff"_hex;                                    // 2 bytes
    const auto deploy_data_size =
        static_cast<uint16_t>(deploy_data.size() + static_aux_data.size());
    const auto deploy_code = rjumpv({6, 12}, calldataload(0)) +  // jump to one of 3 cases
                             35 + OP_DATALOAD + rjump(9) +       // read dynamic aux data
                             OP_DATALOADN + "0000" + rjump(3) +  // read pre_deploy_data_section
                             OP_DATALOADN + "0003" +             // read static aux data
                             ret_top();
    const auto deploy_container = eof_bytecode(deploy_code, 2).data(deploy_data, deploy_data_size);

    const auto init_code = calldatacopy(0, 0, OP_CALLDATASIZE) + returncode(0, 0, OP_CALLDATASIZE);
    const bytecode init_container = eof_bytecode(init_code, 3).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto create_address = compute_eofcreate_address(To, Salt);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0,
            txcreate().initcode(keccak256(init_container)).input(0, OP_CALLDATASIZE).salt(Salt)) +
        mcopy(0, OP_CALLDATASIZE, 32) +                 // zero out first 32-byte word of memory
        call(create_address).input(0, 1).gas(OP_GAS) +  // calldata 0
        OP_POP + returndatacopy(0, 0, 32) + sstore(1, mload(0)) + mstore(0, 1) +
        call(create_address).input(0, 32).gas(OP_GAS) +  // calldata 1
        OP_POP + returndatacopy(0, 0, 32) + sstore(2, mload(0)) + mstore(0, 2) +
        call(create_address).input(0, 32).gas(OP_GAS) +  // calldata 2
        OP_POP + returndatacopy(0, 0, 32) + sstore(3, mload(0)) + sstore(4, 1) + OP_STOP;

    tx.to = To;

    tx.data = static_aux_data + dynamic_aux_data;

    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = to_bytes32(create_address);
    expect.post[*tx.to].storage[0x01_bytes32] =
        0xabcdefaabbccdd00000000000000000000000000000000000000000000000000_bytes32;
    evmc::bytes32 static_aux_data_32;
    std::copy_n(static_aux_data.data(), static_aux_data.size(), &static_aux_data_32.bytes[0]);
    expect.post[*tx.to].storage[0x02_bytes32] = static_aux_data_32;
    evmc::bytes32 dynamic_aux_data_32;
    std::copy_n(dynamic_aux_data.data(), dynamic_aux_data.size(), &dynamic_aux_data_32.bytes[0]);
    expect.post[*tx.to].storage[0x03_bytes32] = dynamic_aux_data_32;
    expect.post[*tx.to].storage[0x04_bytes32] = 0x01_bytes32;
    expect.post[create_address].nonce = 1;
}


TEST_F(state_transition, legacy_create_nested_in_txcreate)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_container = eof_bytecode(OP_STOP);

    const auto init_code = bytecode{OP_DATASIZE} + OP_PUSH0 + OP_PUSH0 + OP_DATACOPY +
                           create().input(0, OP_DATASIZE) + returncode(0, 0, 0);
    const bytes init_container =
        eof_bytecode(init_code, 3).container(deploy_container).data(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        txcreate().initcode(keccak256(init_container)).salt(Salt) + push(1) + OP_SSTORE + OP_STOP;

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[tx.sender].nonce = pre.get(tx.sender).nonce + 1;
    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, legacy_create2_nested_in_txcreate)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_container = eof_bytecode(OP_INVALID);

    const auto init_code = bytecode{OP_DATASIZE} + OP_PUSH0 + OP_PUSH0 + OP_DATACOPY +
                           create2().input(0, OP_DATASIZE).salt(Salt) + returncode(0, 0, 0);
    const bytes init_container =
        eof_bytecode(init_code, 4).container(deploy_container).data(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    const auto factory_code =
        txcreate().initcode(keccak256(init_container)).input(0, 0).salt(Salt) + push(1) +
        OP_SSTORE + OP_STOP;

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[tx.sender].nonce = pre.get(tx.sender).nonce + 1;
    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, legacy_txcreate_from_legacy_tx)
{
    rev = EVMC_EXPERIMENTAL;
    tx.type = Transaction::Type::legacy;

    const auto factory_code = sstore(0, txcreate().initcode(keccak256({})).input(0, 0).salt(Salt)) +
                              sstore(1, 1) + OP_STOP;

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[tx.sender].nonce = pre.get(tx.sender).nonce + 1;
    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;  // CREATE caller's nonce must not be bumped
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;  // CREATE must fail
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, legacy_txcreate_from_1559_tx)
{
    rev = EVMC_EXPERIMENTAL;
    tx.type = Transaction::Type::eip1559;

    const auto factory_code = sstore(0, txcreate().initcode(keccak256({})).input(0, 0).salt(Salt)) +
                              sstore(1, 1) + OP_STOP;

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[tx.sender].nonce = pre.get(tx.sender).nonce + 1;
    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;  // CREATE caller's nonce must not be bumped
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;  // CREATE must fail
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, legacy_txcreate_from_blob_tx)
{
    rev = EVMC_EXPERIMENTAL;

    block.blob_base_fee = 1;

    tx.type = Transaction::Type::blob;
    tx.blob_hashes.push_back(
        0x0100000000000000000000000000000000000000000000000000000000000007_bytes32);

    const auto factory_code = sstore(0, txcreate().initcode(keccak256({})).input(0, 0).salt(Salt)) +
                              sstore(1, 1) + OP_STOP;

    tx.to = To;
    tx.max_blob_gas_price = *block.blob_base_fee;
    pre.get(tx.sender).balance =
        GAS_PER_BLOB * tx.max_blob_gas_price + tx.gas_limit * tx.max_gas_price;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[tx.sender].nonce = pre.get(tx.sender).nonce + 1;
    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;  // CREATE caller's nonce must not be bumped
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;  // CREATE must fail
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, legacy_txcreate_loop_valid)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = MAX_TX_GAS_LIMIT;
    tx.gas_limit = block.gas_limit;
    pre.get(tx.sender).balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID));

    const auto init_code = returncode(0, 0, 0);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    constexpr auto iterations = 400;
    const auto initcode_hash = keccak256(init_container);
    const auto factory_code =
        push0() +                                            // initial salt
        OP_JUMPDEST + push(1) + OP_ADD +                     // increment salt
        OP_DUP1 + push(iterations + 1) + jumpi(58, OP_EQ) +  // if (salt == iterations + 1) break;
        txcreate().value(0).initcode(initcode_hash).input(0, 0).salt(OP_DUP4) + OP_POP + jump(1) +
        OP_JUMPDEST + OP_STOP;

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce + iterations;
    for (uint16_t salt = 1; salt <= iterations; ++salt)
    {
        const auto create_address = compute_eofcreate_address(*tx.to, bytes32{salt});
        expect.post[create_address].code = deploy_container;
        expect.post[create_address].nonce = 1;
    }
}

TEST_F(state_transition, legacy_txcreate_loop_invalid)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = MAX_TX_GAS_LIMIT;
    tx.gas_limit = block.gas_limit;
    pre.get(tx.sender).balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const bytecode init_container = OP_STOP;  // invalid EOF

    tx.type = Transaction::Type::initcodes;
    tx.initcodes.push_back(init_container);

    constexpr auto iterations = 500;
    const auto initcode_hash = keccak256(init_container);
    const auto factory_code =
        push0() +                                             // initial salt
        push(1) + OP_ADD +                                    // increment salt
        OP_DUP1 + push(iterations + 1) + rjumpi(45, OP_EQ) +  // if (salt == iterations + 1) break;
        txcreate().value(0).initcode(initcode_hash).input(0, 0).salt(OP_DUP4) + OP_POP +
        rjump(-56) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 6);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre.get(*tx.to).nonce;
}

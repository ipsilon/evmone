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


TEST_F(state_transition, create_tx_with_eof_initcode)
{
    rev = EVMC_EXPERIMENTAL;

    const bytecode init_container = eof_bytecode(ret(0, 1));

    tx.data = init_container;
    const auto create_address = compute_create_address(tx.sender, pre[tx.sender].nonce);

    expect.post[tx.sender].nonce = pre[tx.sender].nonce + 1;
    expect.status = EVMC_FAILURE;
    expect.post[create_address].exists = false;
}

TEST_F(state_transition, create_with_eof_initcode)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre[tx.sender].balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const bytecode init_container = eof_bytecode(OP_INVALID);
    const auto factory_code =
        mstore(0, push(init_container)) +
        // init_container will be left-padded in memory to 32 bytes
        sstore(0, create().input(32 - init_container.size(), init_container.size())) + sstore(1, 1);

    tx.to = To;

    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, create_with_eof_initcode_cancun)
{
    rev = EVMC_CANCUN;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre[tx.sender].balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const bytecode init_container = eof_bytecode(OP_INVALID);
    const auto factory_code =
        mstore(0, push(init_container)) +
        // init_container will be left-padded in memory to 32 bytes
        sstore(0, create().input(32 - init_container.size(), init_container.size())) + sstore(1, 1);

    tx.to = To;

    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 1;  // fails by EF execution, nonce bumped.
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, create2_with_eof_initcode)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre[tx.sender].balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const bytecode init_container = eof_bytecode(OP_INVALID);
    const auto factory_code =
        mstore(0, push(init_container)) +
        // init_container will be left-padded in memory to 32 bytes
        sstore(0, create2().input(32 - init_container.size(), init_container.size()).salt(Salt)) +
        sstore(1, 1);

    tx.to = To;

    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, create2_with_eof_initcode_cancun)
{
    rev = EVMC_CANCUN;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre[tx.sender].balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const bytecode init_container = eof_bytecode(OP_INVALID);
    const auto factory_code =
        mstore(0, push(init_container)) +
        // init_container will be left-padded in memory to 32 bytes
        sstore(0, create2().input(32 - init_container.size(), init_container.size()).salt(Salt)) +
        sstore(1, 1);

    tx.to = To;

    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 1;  // fails by EF execution, nonce bumped.
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, creation_tx_deploying_eof)
{
    rev = EVMC_EXPERIMENTAL;

    const bytecode deploy_container = eof_bytecode(bytecode(OP_INVALID));
    const auto init_code = mstore(0, push(deploy_container)) +
                           // deploy_container will be left-padded in memory to 32 bytes
                           ret(32 - deploy_container.size(), deploy_container.size());

    tx.data = init_code;

    expect.status = EVMC_CONTRACT_VALIDATION_FAILURE;
    expect.post[Sender].nonce = pre[Sender].nonce + 1;
    const auto create_address = compute_create_address(Sender, pre[Sender].nonce);
    expect.post[create_address].exists = false;
}

TEST_F(state_transition, create_deploying_eof)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre[tx.sender].balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const bytecode deploy_container = eof_bytecode(bytecode(OP_INVALID));
    const auto init_code = mstore(0, push(deploy_container)) +
                           // deploy_container will be left-padded in memory to 32 bytes
                           ret(32 - deploy_container.size(), deploy_container.size());

    const auto factory_code = mstore(0, push(init_code)) +
                              // init_code will be left-padded in memory to 32 bytes
                              sstore(0, create().input(32 - init_code.size(), init_code.size())) +
                              sstore(1, 1);

    tx.to = To;

    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, create2_deploying_eof)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre[tx.sender].balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const bytecode deploy_container = eof_bytecode(bytecode(OP_INVALID));
    const auto init_code = mstore(0, push(deploy_container)) +
                           // deploy_container will be left-padded in memory to 32 bytes
                           ret(32 - deploy_container.size(), deploy_container.size());

    const auto factory_code =
        mstore(0, push(init_code)) +
        // init_code will be left-padded in memory to 32 bytes
        sstore(0, create2().input(32 - init_code.size(), init_code.size()).salt((Salt))) +
        sstore(1, 1);

    tx.to = To;

    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, eofcreate_empty_auxdata)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = "abcdef"_hex;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID)).data(deploy_data);

    const auto init_code = returncode(0, 0, 0);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    const auto factory_code = eofcreate().container(0).input(0, 0).salt(Salt) + ret_top();
    const auto factory_container = eof_bytecode(factory_code, 4).container(init_container);

    tx.to = To;

    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 1;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[create_address].code = deploy_container;
    expect.post[create_address].nonce = 1;
}

TEST_F(state_transition, eofcreate_auxdata_equal_to_declared)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = "abcdef"_hex;
    const auto aux_data = "aabbccddeeff"_hex;
    const auto deploy_data_size = static_cast<uint16_t>(deploy_data.size() + aux_data.size());
    const auto deploy_container =
        eof_bytecode(bytecode(OP_INVALID)).data(deploy_data, deploy_data_size);

    const auto init_code = calldatacopy(0, 0, OP_CALLDATASIZE) + returncode(0, 0, OP_CALLDATASIZE);
    const bytecode init_container = eof_bytecode(init_code, 3).container(deploy_container);

    const auto factory_code = calldatacopy(0, 0, OP_CALLDATASIZE) +
                              eofcreate().container(0).input(0, OP_CALLDATASIZE).salt(Salt) +
                              ret_top();
    const auto factory_container = eof_bytecode(factory_code, 4).container(init_container);

    tx.to = To;

    tx.data = aux_data;

    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    const auto expected_container = eof_bytecode(bytecode(OP_INVALID)).data(deploy_data + aux_data);

    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 1;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[create_address].code = expected_container;
    expect.post[create_address].nonce = 1;
}

TEST_F(state_transition, eofcreate_auxdata_longer_than_declared)
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

    const auto factory_code = calldatacopy(0, 0, OP_CALLDATASIZE) +
                              eofcreate().container(0).input(0, OP_CALLDATASIZE).salt(Salt) +
                              ret_top();
    const auto factory_container = eof_bytecode(factory_code, 4).container(init_container);

    tx.to = To;

    tx.data = aux_data1 + aux_data2;

    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    const auto expected_container =
        eof_bytecode(bytecode(OP_INVALID)).data(deploy_data + aux_data1 + aux_data2);

    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 1;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[create_address].code = expected_container;
    expect.post[create_address].nonce = 1;
}

TEST_F(state_transition, eofcreate_auxdata_shorter_than_declared)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = "abcdef"_hex;
    const auto aux_data = "aabbccddeeff"_hex;
    const auto deploy_data_size = static_cast<uint16_t>(deploy_data.size() + aux_data.size() + 1);
    const auto deploy_container =
        eof_bytecode(bytecode(OP_INVALID)).data(deploy_data, deploy_data_size);

    const auto init_code = calldatacopy(0, 0, OP_CALLDATASIZE) + returncode(0, 0, OP_CALLDATASIZE);
    const auto init_container = eof_bytecode(init_code, 3).container(deploy_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0, eofcreate().container(0).input(0, OP_CALLDATASIZE).salt(Salt)) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 4).container(init_container);

    tx.to = To;

    tx.data = aux_data;

    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, eofcreate_dataloadn_referring_to_auxdata)
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

    const auto factory_code =
        sstore(0, eofcreate().container(0).input(0, 0).salt(Salt)) + sstore(1, 1) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 4).container(init_container);

    tx.to = To;

    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    const auto expected_container = eof_bytecode(deploy_code, 2).data(deploy_data + aux_data);

    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 1;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[*tx.to].storage[0x00_bytes32] = to_bytes32(create_address);
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
    expect.post[create_address].code = expected_container;
    expect.post[create_address].nonce = 1;
}

TEST_F(state_transition, eofcreate_with_auxdata_and_subcontainer)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = "abcdef"_hex;
    const auto aux_data = "aabbccddeeff"_hex;
    const auto deploy_data_size = static_cast<uint16_t>(deploy_data.size() + aux_data.size());
    const auto deploy_container = eof_bytecode(eofcreate() + OP_STOP, 4)
                                      .container(eof_bytecode(OP_INVALID))
                                      .data(deploy_data, deploy_data_size);

    const auto init_code = calldatacopy(0, 0, OP_CALLDATASIZE) + returncode(0, 0, OP_CALLDATASIZE);
    const bytecode init_container = eof_bytecode(init_code, 3).container(deploy_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0, eofcreate().container(0).input(0, OP_CALLDATASIZE).salt(Salt)) + sstore(1, 1) +
        OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 4).container(init_container);

    tx.to = To;

    tx.data = aux_data;

    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    const auto expected_container = eof_bytecode(eofcreate() + OP_STOP, 4)
                                        .container(eof_bytecode(OP_INVALID))
                                        .data(deploy_data + aux_data);

    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 1;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[*tx.to].storage[0x00_bytes32] = to_bytes32(create_address);
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
    expect.post[create_address].code = expected_container;
    expect.post[create_address].nonce = 1;
}

TEST_F(state_transition, eofcreate_revert_empty_returndata)
{
    rev = EVMC_EXPERIMENTAL;
    const auto init_code = revert(0, 0);
    const auto init_container = eof_bytecode(init_code, 2);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0, eofcreate().container(0).input(0, OP_CALLDATASIZE).salt(Salt)) +
        sstore(1, OP_RETURNDATASIZE) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 4).container(init_container);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, eofcreate_revert_non_empty_returndata)
{
    rev = EVMC_EXPERIMENTAL;
    const auto init_code = mstore8(0, 0xaa) + revert(0, 1);
    const auto init_container = eof_bytecode(init_code, 2);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0, eofcreate().container(0).input(0, OP_CALLDATASIZE).salt(Salt)) +
        sstore(1, OP_RETURNDATASIZE) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 4).container(init_container);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, eofcreate_initcontainer_aborts)
{
    rev = EVMC_EXPERIMENTAL;
    const auto init_code = bytecode{Opcode{OP_INVALID}};
    const auto init_container = eof_bytecode(init_code, 0);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0, eofcreate().container(0).input(0, OP_CALLDATASIZE).salt(Salt)) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 4).container(init_container);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, eofcreate_deploy_container_max_size)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre[tx.sender].balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const auto eof_header_size =
        static_cast<int>(bytecode{eof_bytecode(Opcode{OP_INVALID})}.size() - 1);
    const auto deploy_code = (0x5fff - eof_header_size) * bytecode{Opcode{OP_JUMPDEST}} + OP_STOP;
    const bytecode deploy_container = eof_bytecode(deploy_code);
    EXPECT_EQ(deploy_container.size(), 0x6000);

    // no aux data
    const auto init_code = returncode(0, 0, 0);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0, eofcreate().container(0).input(0, OP_CALLDATASIZE).salt(Salt)) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 4).container(init_container);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 1;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[*tx.to].storage[0x00_bytes32] = to_bytes32(create_address);
    expect.post[create_address].code = deploy_container;
}

TEST_F(state_transition, eofcreate_deploy_container_too_large)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre[tx.sender].balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const auto eof_header_size =
        static_cast<int>(bytecode{eof_bytecode(Opcode{OP_INVALID})}.size() - 1);
    const auto deploy_code = (0x6000 - eof_header_size) * bytecode{Opcode{OP_JUMPDEST}} + OP_STOP;
    const bytecode deploy_container = eof_bytecode(deploy_code);
    EXPECT_EQ(deploy_container.size(), 0x6001);

    // no aux data
    const auto init_code = returncode(0, 0, 0);
    const auto init_container = eof_bytecode(init_code, 2).container(deploy_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0, eofcreate().container(0).input(0, OP_CALLDATASIZE).salt(Salt)) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 4).container(init_container);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, eofcreate_appended_data_size_larger_than_64K)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre[tx.sender].balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const auto aux_data = bytes(std::numeric_limits<uint16_t>::max(), 0);
    const auto deploy_data = "aa"_hex;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID)).data(deploy_data);

    const auto init_code = calldatacopy(0, 0, OP_CALLDATASIZE) + returncode(0, 0, OP_CALLDATASIZE);
    const bytecode init_container = eof_bytecode(init_code, 3).container(deploy_container);

    static constexpr bytes32 salt2{0xfe};
    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        // with aux data, final data size = 2**16
        sstore(0, eofcreate().container(0).input(0, OP_CALLDATASIZE).salt(Salt)) +
        // no aux_data - final data size = 1
        sstore(1, eofcreate().container(0).salt(salt2)) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 4).container(init_container);

    tx.to = To;

    tx.data = aux_data;

    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 2;  // 1 successful creation + 1 hard fail
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
    const auto create_address = compute_eofcreate_address(*tx.to, salt2);
    expect.post[*tx.to].storage[0x01_bytes32] = to_bytes32(create_address);
    expect.post[create_address].code = deploy_container;
    expect.post[create_address].nonce = 1;
}

TEST_F(state_transition, eofcreate_deploy_container_with_aux_data_too_large)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre[tx.sender].balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const auto eof_header_size =
        static_cast<int>(bytecode{eof_bytecode(Opcode{OP_INVALID})}.size() - 1);
    const auto deploy_code = (0x5fff - eof_header_size) * bytecode{Opcode{OP_JUMPDEST}} + OP_STOP;
    const bytecode deploy_container = eof_bytecode(deploy_code);
    EXPECT_EQ(deploy_container.size(), 0x6000);

    // 1 byte aux data
    const auto init_code = returncode(0, 0, 1);
    const auto init_container = eof_bytecode(init_code, 2).container(deploy_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0, eofcreate().container(0).input(0, OP_CALLDATASIZE).salt(Salt)) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 4).container(init_container);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, eofcreate_nested_eofcreate)
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

    const auto factory_code = sstore(0, eofcreate().container(0).salt(Salt)) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 4).container(init_container);

    tx.to = To;

    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 1;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[*tx.to].storage[0x00_bytes32] = to_bytes32(create_address);
    expect.post[create_address].code = deploy_container;
    expect.post[create_address].nonce = 2;
    const auto create_address_nested = compute_eofcreate_address(create_address, Salt);
    expect.post[create_address].storage[0x00_bytes32] = to_bytes32(create_address_nested);
    expect.post[create_address_nested].code = deploy_container_nested;
    expect.post[create_address_nested].nonce = 1;
}

TEST_F(state_transition, eofcreate_nested_eofcreate_revert)
{
    rev = EVMC_EXPERIMENTAL;

    const auto deploy_data_nested = "ffffff"_hex;
    const auto deploy_container_nested =
        eof_bytecode(bytecode(OP_INVALID)).data(deploy_data_nested);

    const auto init_code_nested = returncode(0, 0, 0);
    const auto init_container_nested =
        eof_bytecode(init_code_nested, 2).container(deploy_container_nested);

    const auto init_code = sstore(0, eofcreate().container(0).salt(Salt)) + revert(0, 0);
    const auto init_container = eof_bytecode(init_code, 4).container(init_container_nested);

    const auto factory_code = sstore(0, eofcreate().container(0).salt(Salt)) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 4).container(init_container);

    tx.to = To;

    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, eofcreate_caller_balance_too_low)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_data = "abcdef"_hex;
    const auto deploy_container = eof_bytecode(bytecode{Opcode{OP_INVALID}}).data(deploy_data);

    const auto init_code = calldatacopy(0, 0, OP_CALLDATASIZE) + returncode(0, 0, OP_CALLDATASIZE);
    const auto init_container = eof_bytecode(init_code, 3).container(deploy_container);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0, eofcreate().container(0).input(0, OP_CALLDATASIZE).salt(Salt).value(10)) +
        sstore(1, 1) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 4).container(init_container);

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, eofcreate_not_enough_gas_for_initcode_charge)
{
    rev = EVMC_EXPERIMENTAL;
    const auto deploy_container = eof_bytecode(bytecode(OP_INVALID));

    const auto init_code = returncode(0, 0, 0);
    auto init_container = eof_bytecode(init_code, 2).container(deploy_container);
    const uint16_t init_data_size = std::numeric_limits<uint16_t>::max() / 2 -
                                    static_cast<uint16_t>(bytecode(init_container).size());
    const auto init_data = bytes(init_data_size, 0);
    init_container.data(init_data, init_data_size);
    EXPECT_EQ(bytecode(init_container).size(), std::numeric_limits<uint16_t>::max() / 2);

    const auto factory_code = sstore(0, eofcreate().container(0).salt(Salt)) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 4).container(init_container);

    tx.to = To;
    // tx intrinsic cost + EOFCREATE cost + initcode charge - not enough for pushes before EOFCREATE
    tx.gas_limit = 21'000 + 32'000 + (std::numeric_limits<uint16_t>::max() / 2 + 31) / 32 * 6;

    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.status = EVMC_OUT_OF_GAS;

    expect.post[*tx.to].nonce = pre[*tx.to].nonce;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, eofcreate_not_enough_gas_for_mem_expansion)
{
    rev = EVMC_EXPERIMENTAL;
    auto deploy_container = eof_bytecode(bytecode(OP_INVALID));
    // max size aux data
    const auto aux_data_size = static_cast<uint16_t>(
        std::numeric_limits<uint16_t>::max() - bytecode(deploy_container).size());
    deploy_container.data({}, aux_data_size);
    EXPECT_EQ(
        bytecode(deploy_container).size() + aux_data_size, std::numeric_limits<uint16_t>::max());

    const auto init_code = calldatacopy(0, 0, OP_CALLDATASIZE) + returncode(0, 0, OP_CALLDATASIZE);
    const bytecode init_container = eof_bytecode(init_code, 3).container(deploy_container);

    const auto factory_code =
        sstore(0, eofcreate().container(0).input(0, aux_data_size).salt(Salt)) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 4).container(init_container);

    tx.to = To;
    // tx intrinsic cost + EOFCREATE cost + initcode charge + mem expansion size = not enough for
    // pushes before EOFCREATE
    const auto initcode_size_words = (init_container.size() + 31) / 32;
    const auto aux_data_size_words = (aux_data_size + 31) / 32;
    tx.gas_limit = 21'000 + 32'000 + static_cast<uint16_t>(6 * initcode_size_words) +
                   3 * aux_data_size_words + aux_data_size_words * aux_data_size_words / 512;

    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.status = EVMC_OUT_OF_GAS;

    expect.post[*tx.to].nonce = pre[*tx.to].nonce;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, returncode_not_enough_gas_for_mem_expansion)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre[tx.sender].balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    auto deploy_container = eof_bytecode(bytecode(OP_INVALID));
    // max size aux data
    const auto aux_data_size = static_cast<uint16_t>(
        std::numeric_limits<uint16_t>::max() - bytecode(deploy_container).size());
    deploy_container.data({}, aux_data_size);
    EXPECT_EQ(
        bytecode(deploy_container).size() + aux_data_size, std::numeric_limits<uint16_t>::max());

    const auto init_code = returncode(0, 0, aux_data_size);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    const auto factory_code = eofcreate().container(0).salt(Salt) + OP_POP + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 4).container(init_container);

    tx.to = To;
    // tx intrinsic cost + EOFCREATE cost + initcode charge + mem expansion size = not enough for
    // pushes before RETURNDATALOAD
    const auto initcode_size_words = (init_container.size() + 31) / 32;
    const auto aux_data_size_words = (aux_data_size + 31) / 32;
    tx.gas_limit = 21'000 + 32'000 + static_cast<uint16_t>(6 * initcode_size_words) +
                   3 * aux_data_size_words + aux_data_size_words * aux_data_size_words / 512;

    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 1;
    expect.post[*tx.to].storage[0x00_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, eofcreate_clears_returndata)
{
    static constexpr auto returning_address = 0x3000_address;

    rev = EVMC_EXPERIMENTAL;
    const auto deploy_container = eof_bytecode(OP_STOP);

    const auto init_code = returncode(0, 0, 0);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    const auto factory_code = sstore(0, extcall(returning_address)) + sstore(1, returndatasize()) +
                              sstore(2, eofcreate().container(0).salt(Salt)) +
                              sstore(3, returndatasize()) + sstore(4, 1) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 4).container(init_container);

    const auto returning_code = ret(0, 10);

    tx.to = To;

    pre[*tx.to] = {.nonce = 1, .code = factory_container};
    pre[returning_address] = {.nonce = 1, .code = returning_code};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 1;
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

TEST_F(state_transition, eofcreate_failure_after_eofcreate_success)
{
    rev = EVMC_EXPERIMENTAL;
    block.gas_limit = 10'000'000;
    tx.gas_limit = block.gas_limit;
    pre[tx.sender].balance = tx.gas_limit * tx.max_gas_price + tx.value + 1;

    const auto deploy_container = eof_bytecode(OP_STOP);

    const auto init_code = returncode(0, 0, 0);
    const bytecode init_container = eof_bytecode(init_code, 2).container(deploy_container);

    const auto factory_code = sstore(0, eofcreate().container(0).salt(Salt)) +
                              sstore(1, eofcreate().container(0).salt(Salt)) +  // address collision
                              sstore(2, returndatasize()) + sstore(3, 1) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 4).container(init_container);

    tx.to = To;

    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 2;
    const auto create_address = compute_eofcreate_address(*tx.to, Salt);
    expect.post[*tx.to].storage[0x00_bytes32] = to_bytes32(create_address);
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x02_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x03_bytes32] = 0x01_bytes32;
    expect.post[create_address].code = deploy_container;
    expect.post[create_address].nonce = 1;
}

TEST_F(state_transition, eofcreate_call_created_contract)
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

    const auto create_address = compute_eofcreate_address(To, Salt);

    const auto factory_code =
        calldatacopy(0, 0, OP_CALLDATASIZE) +
        sstore(0, eofcreate().container(0).input(0, OP_CALLDATASIZE).salt(Salt)) +
        mcopy(0, OP_CALLDATASIZE, 32) +                 // zero out first 32-byte word of memory
        extcall(create_address).input(0, 1) + OP_POP +  // calldata 0
        sstore(1, returndataload(0)) + mstore8(31, 1) +
        extcall(create_address).input(0, 32) +  // calldata 1
        OP_POP + sstore(2, returndataload(0)) + mstore8(31, 2) +
        extcall(create_address).input(0, 32) +  // calldata 2
        OP_POP + sstore(3, returndataload(0)) + sstore(4, 1) + OP_STOP;
    const auto factory_container = eof_bytecode(factory_code, 4).container(init_container);

    tx.to = To;

    tx.data = static_aux_data + dynamic_aux_data;

    pre[*tx.to] = {.nonce = 1, .code = factory_container};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 1;
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

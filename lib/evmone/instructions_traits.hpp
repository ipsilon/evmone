// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2020 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "instructions_opcodes.hpp"
#include <array>
#include <optional>

namespace evmone::instr
{
/// The special gas cost value marking an EVM instruction as "undefined".
constexpr int16_t undefined = -1;

/// EIP-2929 constants (https://eips.ethereum.org/EIPS/eip-2929).
/// @{
inline constexpr auto cold_sload_cost = 2100;
inline constexpr auto cold_account_access_cost = 2600;
inline constexpr auto warm_storage_read_cost = 100;

/// Additional cold account access cost.
///
/// The warm access cost is unconditionally applied for every account access instruction.
/// If the access turns out to be cold, this cost must be applied additionally.
inline constexpr auto additional_cold_account_access_cost =
    cold_account_access_cost - warm_storage_read_cost;
/// @}


/// The table of instruction gas costs per EVM revision.
using GasCostTable = std::array<std::array<int16_t, 256>, EVMC_MAX_REVISION + 1>;

/// The EVM revision specific table of EVM instructions gas costs. For instructions undefined
/// in given EVM revision, the value is instr::undefined.
constexpr inline GasCostTable gas_costs = []() noexcept {
    GasCostTable table{};

    for (auto& t : table[EVMC_FRONTIER])
        t = undefined;
    table[EVMC_FRONTIER][OP_STOP] = 0;
    table[EVMC_FRONTIER][OP_ADD] = 3;
    table[EVMC_FRONTIER][OP_MUL] = 5;
    table[EVMC_FRONTIER][OP_SUB] = 3;
    table[EVMC_FRONTIER][OP_DIV] = 5;
    table[EVMC_FRONTIER][OP_SDIV] = 5;
    table[EVMC_FRONTIER][OP_MOD] = 5;
    table[EVMC_FRONTIER][OP_SMOD] = 5;
    table[EVMC_FRONTIER][OP_ADDMOD] = 8;
    table[EVMC_FRONTIER][OP_MULMOD] = 8;
    table[EVMC_FRONTIER][OP_EXP] = 10;
    table[EVMC_FRONTIER][OP_SIGNEXTEND] = 5;
    table[EVMC_FRONTIER][OP_LT] = 3;
    table[EVMC_FRONTIER][OP_GT] = 3;
    table[EVMC_FRONTIER][OP_SLT] = 3;
    table[EVMC_FRONTIER][OP_SGT] = 3;
    table[EVMC_FRONTIER][OP_EQ] = 3;
    table[EVMC_FRONTIER][OP_ISZERO] = 3;
    table[EVMC_FRONTIER][OP_AND] = 3;
    table[EVMC_FRONTIER][OP_OR] = 3;
    table[EVMC_FRONTIER][OP_XOR] = 3;
    table[EVMC_FRONTIER][OP_NOT] = 3;
    table[EVMC_FRONTIER][OP_BYTE] = 3;
    table[EVMC_FRONTIER][OP_KECCAK256] = 30;
    table[EVMC_FRONTIER][OP_ADDRESS] = 2;
    table[EVMC_FRONTIER][OP_BALANCE] = 20;
    table[EVMC_FRONTIER][OP_ORIGIN] = 2;
    table[EVMC_FRONTIER][OP_CALLER] = 2;
    table[EVMC_FRONTIER][OP_CALLVALUE] = 2;
    table[EVMC_FRONTIER][OP_CALLDATALOAD] = 3;
    table[EVMC_FRONTIER][OP_CALLDATASIZE] = 2;
    table[EVMC_FRONTIER][OP_CALLDATACOPY] = 3;
    table[EVMC_FRONTIER][OP_CODESIZE] = 2;
    table[EVMC_FRONTIER][OP_CODECOPY] = 3;
    table[EVMC_FRONTIER][OP_GASPRICE] = 2;
    table[EVMC_FRONTIER][OP_EXTCODESIZE] = 20;
    table[EVMC_FRONTIER][OP_EXTCODECOPY] = 20;
    table[EVMC_FRONTIER][OP_BLOCKHASH] = 20;
    table[EVMC_FRONTIER][OP_COINBASE] = 2;
    table[EVMC_FRONTIER][OP_TIMESTAMP] = 2;
    table[EVMC_FRONTIER][OP_NUMBER] = 2;
    table[EVMC_FRONTIER][OP_PREVRANDAO] = 2;
    table[EVMC_FRONTIER][OP_GASLIMIT] = 2;
    table[EVMC_FRONTIER][OP_POP] = 2;
    table[EVMC_FRONTIER][OP_MLOAD] = 3;
    table[EVMC_FRONTIER][OP_MSTORE] = 3;
    table[EVMC_FRONTIER][OP_MSTORE8] = 3;
    table[EVMC_FRONTIER][OP_SLOAD] = 50;
    table[EVMC_FRONTIER][OP_SSTORE] = 0;
    table[EVMC_FRONTIER][OP_JUMP] = 8;
    table[EVMC_FRONTIER][OP_JUMPI] = 10;
    table[EVMC_FRONTIER][OP_PC] = 2;
    table[EVMC_FRONTIER][OP_MSIZE] = 2;
    table[EVMC_FRONTIER][OP_GAS] = 2;
    table[EVMC_FRONTIER][OP_JUMPDEST] = 1;
    for (auto op = size_t{OP_PUSH1}; op <= OP_PUSH32; ++op)
        table[EVMC_FRONTIER][op] = 3;
    for (auto op = size_t{OP_DUP1}; op <= OP_DUP16; ++op)
        table[EVMC_FRONTIER][op] = 3;
    for (auto op = size_t{OP_SWAP1}; op <= OP_SWAP16; ++op)
        table[EVMC_FRONTIER][op] = 3;
    for (auto op = size_t{OP_LOG0}; op <= OP_LOG4; ++op)
        table[EVMC_FRONTIER][op] = static_cast<int16_t>((op - OP_LOG0 + 1) * 375);
    table[EVMC_FRONTIER][OP_CREATE] = 32000;
    table[EVMC_FRONTIER][OP_CALL] = 40;
    table[EVMC_FRONTIER][OP_CALLCODE] = 40;
    table[EVMC_FRONTIER][OP_RETURN] = 0;
    table[EVMC_FRONTIER][OP_INVALID] = 0;
    table[EVMC_FRONTIER][OP_SELFDESTRUCT] = 0;

    table[EVMC_HOMESTEAD] = table[EVMC_FRONTIER];
    table[EVMC_HOMESTEAD][OP_DELEGATECALL] = 40;

    table[EVMC_TANGERINE_WHISTLE] = table[EVMC_HOMESTEAD];
    table[EVMC_TANGERINE_WHISTLE][OP_BALANCE] = 400;
    table[EVMC_TANGERINE_WHISTLE][OP_EXTCODESIZE] = 700;
    table[EVMC_TANGERINE_WHISTLE][OP_EXTCODECOPY] = 700;
    table[EVMC_TANGERINE_WHISTLE][OP_SLOAD] = 200;
    table[EVMC_TANGERINE_WHISTLE][OP_CALL] = 700;
    table[EVMC_TANGERINE_WHISTLE][OP_CALLCODE] = 700;
    table[EVMC_TANGERINE_WHISTLE][OP_DELEGATECALL] = 700;
    table[EVMC_TANGERINE_WHISTLE][OP_SELFDESTRUCT] = 5000;

    table[EVMC_SPURIOUS_DRAGON] = table[EVMC_TANGERINE_WHISTLE];

    table[EVMC_BYZANTIUM] = table[EVMC_SPURIOUS_DRAGON];
    table[EVMC_BYZANTIUM][OP_RETURNDATASIZE] = 2;
    table[EVMC_BYZANTIUM][OP_RETURNDATACOPY] = 3;
    table[EVMC_BYZANTIUM][OP_STATICCALL] = 700;
    table[EVMC_BYZANTIUM][OP_REVERT] = 0;

    table[EVMC_CONSTANTINOPLE] = table[EVMC_BYZANTIUM];
    table[EVMC_CONSTANTINOPLE][OP_SHL] = 3;
    table[EVMC_CONSTANTINOPLE][OP_SHR] = 3;
    table[EVMC_CONSTANTINOPLE][OP_SAR] = 3;
    table[EVMC_CONSTANTINOPLE][OP_EXTCODEHASH] = 400;
    table[EVMC_CONSTANTINOPLE][OP_CREATE2] = 32000;

    table[EVMC_PETERSBURG] = table[EVMC_CONSTANTINOPLE];

    table[EVMC_ISTANBUL] = table[EVMC_PETERSBURG];
    table[EVMC_ISTANBUL][OP_BALANCE] = 700;
    table[EVMC_ISTANBUL][OP_CHAINID] = 2;
    table[EVMC_ISTANBUL][OP_EXTCODEHASH] = 700;
    table[EVMC_ISTANBUL][OP_SELFBALANCE] = 5;
    table[EVMC_ISTANBUL][OP_SLOAD] = 800;

    table[EVMC_BERLIN] = table[EVMC_ISTANBUL];
    table[EVMC_BERLIN][OP_EXTCODESIZE] = warm_storage_read_cost;
    table[EVMC_BERLIN][OP_EXTCODECOPY] = warm_storage_read_cost;
    table[EVMC_BERLIN][OP_EXTCODEHASH] = warm_storage_read_cost;
    table[EVMC_BERLIN][OP_BALANCE] = warm_storage_read_cost;
    table[EVMC_BERLIN][OP_CALL] = warm_storage_read_cost;
    table[EVMC_BERLIN][OP_CALLCODE] = warm_storage_read_cost;
    table[EVMC_BERLIN][OP_DELEGATECALL] = warm_storage_read_cost;
    table[EVMC_BERLIN][OP_STATICCALL] = warm_storage_read_cost;
    table[EVMC_BERLIN][OP_SLOAD] = warm_storage_read_cost;

    table[EVMC_LONDON] = table[EVMC_BERLIN];
    table[EVMC_LONDON][OP_BASEFEE] = 2;

    table[EVMC_PARIS] = table[EVMC_LONDON];

    table[EVMC_SHANGHAI] = table[EVMC_PARIS];
    table[EVMC_SHANGHAI][OP_PUSH0] = 2;

    table[EVMC_CANCUN] = table[EVMC_SHANGHAI];
    table[EVMC_CANCUN][OP_BLOBHASH] = 3;
    table[EVMC_CANCUN][OP_BLOBBASEFEE] = 2;
    table[EVMC_CANCUN][OP_TLOAD] = warm_storage_read_cost;
    table[EVMC_CANCUN][OP_TSTORE] = warm_storage_read_cost;
    table[EVMC_CANCUN][OP_MCOPY] = 3;

    table[EVMC_PRAGUE] = table[EVMC_CANCUN];

    table[EVMC_OSAKA] = table[EVMC_PRAGUE];
    table[EVMC_OSAKA][OP_CLZ] = 5;

    table[EVMC_EXPERIMENTAL] = table[EVMC_OSAKA];

    return table;
}();

static_assert(gas_costs[EVMC_MAX_REVISION][OP_ADD] > 0, "gas costs missing for a revision");


/// The EVM instruction traits.
struct Traits
{
    /// The instruction name;
    const char* name = nullptr;

    /// Size of the immediate argument in bytes.
    uint8_t immediate_size = 0;

    /// Whether the instruction terminates execution.
    /// This is false for undefined instructions but this can be changed if desired.
    bool is_terminating = false;

    /// The number of stack items the instruction accesses during execution.
    uint8_t stack_height_required = 0;

    /// The stack height change caused by the instruction execution. Can be negative.
    int8_t stack_height_change = 0;

    /// The EVM revision in which the instruction has been defined. For instructions available in
    /// every EVM revision the value is ::EVMC_FRONTIER. For undefined instructions the value is not
    /// available.
    std::optional<evmc_revision> since;
};

/// Determines if an instruction has constant base gas cost across all revisions.
/// Note that this is not true for instructions with constant base gas cost but
/// not available in the first revision (e.g. SHL).
consteval bool has_const_gas_cost(Opcode op) noexcept
{
    const auto g = gas_costs[EVMC_FRONTIER][op];
    for (size_t r = EVMC_FRONTIER + 1; r <= EVMC_MAX_REVISION; ++r)
    {
        if (gas_costs[r][op] != g)
            return false;
    }
    return true;
}


/// The global, EVM revision independent, table of traits of all known EVM instructions.
constexpr inline std::array<Traits, 256> traits = []() noexcept {
    std::array<Traits, 256> table{};

    table[OP_STOP] = {"STOP", 0, true, 0, 0, EVMC_FRONTIER};
    table[OP_ADD] = {"ADD", 0, false, 2, -1, EVMC_FRONTIER};
    table[OP_MUL] = {"MUL", 0, false, 2, -1, EVMC_FRONTIER};
    table[OP_SUB] = {"SUB", 0, false, 2, -1, EVMC_FRONTIER};
    table[OP_DIV] = {"DIV", 0, false, 2, -1, EVMC_FRONTIER};
    table[OP_SDIV] = {"SDIV", 0, false, 2, -1, EVMC_FRONTIER};
    table[OP_MOD] = {"MOD", 0, false, 2, -1, EVMC_FRONTIER};
    table[OP_SMOD] = {"SMOD", 0, false, 2, -1, EVMC_FRONTIER};
    table[OP_ADDMOD] = {"ADDMOD", 0, false, 3, -2, EVMC_FRONTIER};
    table[OP_MULMOD] = {"MULMOD", 0, false, 3, -2, EVMC_FRONTIER};
    table[OP_EXP] = {"EXP", 0, false, 2, -1, EVMC_FRONTIER};
    table[OP_SIGNEXTEND] = {"SIGNEXTEND", 0, false, 2, -1, EVMC_FRONTIER};

    table[OP_LT] = {"LT", 0, false, 2, -1, EVMC_FRONTIER};
    table[OP_GT] = {"GT", 0, false, 2, -1, EVMC_FRONTIER};
    table[OP_SLT] = {"SLT", 0, false, 2, -1, EVMC_FRONTIER};
    table[OP_SGT] = {"SGT", 0, false, 2, -1, EVMC_FRONTIER};
    table[OP_EQ] = {"EQ", 0, false, 2, -1, EVMC_FRONTIER};
    table[OP_ISZERO] = {"ISZERO", 0, false, 1, 0, EVMC_FRONTIER};
    table[OP_AND] = {"AND", 0, false, 2, -1, EVMC_FRONTIER};
    table[OP_OR] = {"OR", 0, false, 2, -1, EVMC_FRONTIER};
    table[OP_XOR] = {"XOR", 0, false, 2, -1, EVMC_FRONTIER};
    table[OP_NOT] = {"NOT", 0, false, 1, 0, EVMC_FRONTIER};
    table[OP_BYTE] = {"BYTE", 0, false, 2, -1, EVMC_FRONTIER};
    table[OP_SHL] = {"SHL", 0, false, 2, -1, EVMC_CONSTANTINOPLE};
    table[OP_SHR] = {"SHR", 0, false, 2, -1, EVMC_CONSTANTINOPLE};
    table[OP_SAR] = {"SAR", 0, false, 2, -1, EVMC_CONSTANTINOPLE};
    table[OP_CLZ] = {"CLZ", 0, false, 1, 0, EVMC_OSAKA};

    table[OP_KECCAK256] = {"KECCAK256", 0, false, 2, -1, EVMC_FRONTIER};

    table[OP_ADDRESS] = {"ADDRESS", 0, false, 0, 1, EVMC_FRONTIER};
    table[OP_BALANCE] = {"BALANCE", 0, false, 1, 0, EVMC_FRONTIER};
    table[OP_ORIGIN] = {"ORIGIN", 0, false, 0, 1, EVMC_FRONTIER};
    table[OP_CALLER] = {"CALLER", 0, false, 0, 1, EVMC_FRONTIER};
    table[OP_CALLVALUE] = {"CALLVALUE", 0, false, 0, 1, EVMC_FRONTIER};
    table[OP_CALLDATALOAD] = {"CALLDATALOAD", 0, false, 1, 0, EVMC_FRONTIER};
    table[OP_CALLDATASIZE] = {"CALLDATASIZE", 0, false, 0, 1, EVMC_FRONTIER};
    table[OP_CALLDATACOPY] = {"CALLDATACOPY", 0, false, 3, -3, EVMC_FRONTIER};
    table[OP_CODESIZE] = {"CODESIZE", 0, false, 0, 1, EVMC_FRONTIER};
    table[OP_CODECOPY] = {"CODECOPY", 0, false, 3, -3, EVMC_FRONTIER};
    table[OP_GASPRICE] = {"GASPRICE", 0, false, 0, 1, EVMC_FRONTIER};
    table[OP_EXTCODESIZE] = {"EXTCODESIZE", 0, false, 1, 0, EVMC_FRONTIER};
    table[OP_EXTCODECOPY] = {"EXTCODECOPY", 0, false, 4, -4, EVMC_FRONTIER};
    table[OP_RETURNDATASIZE] = {"RETURNDATASIZE", 0, false, 0, 1, EVMC_BYZANTIUM};
    table[OP_RETURNDATACOPY] = {"RETURNDATACOPY", 0, false, 3, -3, EVMC_BYZANTIUM};
    table[OP_EXTCODEHASH] = {"EXTCODEHASH", 0, false, 1, 0, EVMC_CONSTANTINOPLE};

    table[OP_BLOCKHASH] = {"BLOCKHASH", 0, false, 1, 0, EVMC_FRONTIER};
    table[OP_COINBASE] = {"COINBASE", 0, false, 0, 1, EVMC_FRONTIER};
    table[OP_TIMESTAMP] = {"TIMESTAMP", 0, false, 0, 1, EVMC_FRONTIER};
    table[OP_NUMBER] = {"NUMBER", 0, false, 0, 1, EVMC_FRONTIER};
    table[OP_PREVRANDAO] = {"PREVRANDAO", 0, false, 0, 1, EVMC_FRONTIER};
    table[OP_GASLIMIT] = {"GASLIMIT", 0, false, 0, 1, EVMC_FRONTIER};
    table[OP_CHAINID] = {"CHAINID", 0, false, 0, 1, EVMC_ISTANBUL};
    table[OP_SELFBALANCE] = {"SELFBALANCE", 0, false, 0, 1, EVMC_ISTANBUL};
    table[OP_BASEFEE] = {"BASEFEE", 0, false, 0, 1, EVMC_LONDON};
    table[OP_BLOBHASH] = {"BLOBHASH", 0, false, 1, 0, EVMC_CANCUN};
    table[OP_BLOBBASEFEE] = {"BLOBBASEFEE", 0, false, 0, 1, EVMC_CANCUN};

    table[OP_POP] = {"POP", 0, false, 1, -1, EVMC_FRONTIER};
    table[OP_MLOAD] = {"MLOAD", 0, false, 1, 0, EVMC_FRONTIER};
    table[OP_MSTORE] = {"MSTORE", 0, false, 2, -2, EVMC_FRONTIER};
    table[OP_MSTORE8] = {"MSTORE8", 0, false, 2, -2, EVMC_FRONTIER};
    table[OP_SLOAD] = {"SLOAD", 0, false, 1, 0, EVMC_FRONTIER};
    table[OP_SSTORE] = {"SSTORE", 0, false, 2, -2, EVMC_FRONTIER};
    table[OP_JUMP] = {"JUMP", 0, false, 1, -1, EVMC_FRONTIER};
    table[OP_JUMPI] = {"JUMPI", 0, false, 2, -2, EVMC_FRONTIER};
    table[OP_PC] = {"PC", 0, false, 0, 1, EVMC_FRONTIER};
    table[OP_MSIZE] = {"MSIZE", 0, false, 0, 1, EVMC_FRONTIER};
    table[OP_GAS] = {"GAS", 0, false, 0, 1, EVMC_FRONTIER};
    table[OP_JUMPDEST] = {"JUMPDEST", 0, false, 0, 0, EVMC_FRONTIER};

    table[OP_TLOAD] = {"TLOAD", 0, false, 1, 0, EVMC_CANCUN};
    table[OP_TSTORE] = {"TSTORE", 0, false, 2, -2, EVMC_CANCUN};
    table[OP_PUSH0] = {"PUSH0", 0, false, 0, 1, EVMC_SHANGHAI};

    table[OP_PUSH1] = {"PUSH1", 1, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH2] = {"PUSH2", 2, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH3] = {"PUSH3", 3, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH4] = {"PUSH4", 4, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH5] = {"PUSH5", 5, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH6] = {"PUSH6", 6, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH7] = {"PUSH7", 7, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH8] = {"PUSH8", 8, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH9] = {"PUSH9", 9, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH10] = {"PUSH10", 10, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH11] = {"PUSH11", 11, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH12] = {"PUSH12", 12, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH13] = {"PUSH13", 13, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH14] = {"PUSH14", 14, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH15] = {"PUSH15", 15, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH16] = {"PUSH16", 16, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH17] = {"PUSH17", 17, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH18] = {"PUSH18", 18, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH19] = {"PUSH19", 19, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH20] = {"PUSH20", 20, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH21] = {"PUSH21", 21, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH22] = {"PUSH22", 22, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH23] = {"PUSH23", 23, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH24] = {"PUSH24", 24, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH25] = {"PUSH25", 25, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH26] = {"PUSH26", 26, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH27] = {"PUSH27", 27, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH28] = {"PUSH28", 28, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH29] = {"PUSH29", 29, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH30] = {"PUSH30", 30, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH31] = {"PUSH31", 31, false, 0, 1, EVMC_FRONTIER};
    table[OP_PUSH32] = {"PUSH32", 32, false, 0, 1, EVMC_FRONTIER};

    table[OP_DUP1] = {"DUP1", 0, false, 1, 1, EVMC_FRONTIER};
    table[OP_DUP2] = {"DUP2", 0, false, 2, 1, EVMC_FRONTIER};
    table[OP_DUP3] = {"DUP3", 0, false, 3, 1, EVMC_FRONTIER};
    table[OP_DUP4] = {"DUP4", 0, false, 4, 1, EVMC_FRONTIER};
    table[OP_DUP5] = {"DUP5", 0, false, 5, 1, EVMC_FRONTIER};
    table[OP_DUP6] = {"DUP6", 0, false, 6, 1, EVMC_FRONTIER};
    table[OP_DUP7] = {"DUP7", 0, false, 7, 1, EVMC_FRONTIER};
    table[OP_DUP8] = {"DUP8", 0, false, 8, 1, EVMC_FRONTIER};
    table[OP_DUP9] = {"DUP9", 0, false, 9, 1, EVMC_FRONTIER};
    table[OP_DUP10] = {"DUP10", 0, false, 10, 1, EVMC_FRONTIER};
    table[OP_DUP11] = {"DUP11", 0, false, 11, 1, EVMC_FRONTIER};
    table[OP_DUP12] = {"DUP12", 0, false, 12, 1, EVMC_FRONTIER};
    table[OP_DUP13] = {"DUP13", 0, false, 13, 1, EVMC_FRONTIER};
    table[OP_DUP14] = {"DUP14", 0, false, 14, 1, EVMC_FRONTIER};
    table[OP_DUP15] = {"DUP15", 0, false, 15, 1, EVMC_FRONTIER};
    table[OP_DUP16] = {"DUP16", 0, false, 16, 1, EVMC_FRONTIER};

    table[OP_SWAP1] = {"SWAP1", 0, false, 2, 0, EVMC_FRONTIER};
    table[OP_SWAP2] = {"SWAP2", 0, false, 3, 0, EVMC_FRONTIER};
    table[OP_SWAP3] = {"SWAP3", 0, false, 4, 0, EVMC_FRONTIER};
    table[OP_SWAP4] = {"SWAP4", 0, false, 5, 0, EVMC_FRONTIER};
    table[OP_SWAP5] = {"SWAP5", 0, false, 6, 0, EVMC_FRONTIER};
    table[OP_SWAP6] = {"SWAP6", 0, false, 7, 0, EVMC_FRONTIER};
    table[OP_SWAP7] = {"SWAP7", 0, false, 8, 0, EVMC_FRONTIER};
    table[OP_SWAP8] = {"SWAP8", 0, false, 9, 0, EVMC_FRONTIER};
    table[OP_SWAP9] = {"SWAP9", 0, false, 10, 0, EVMC_FRONTIER};
    table[OP_SWAP10] = {"SWAP10", 0, false, 11, 0, EVMC_FRONTIER};
    table[OP_SWAP11] = {"SWAP11", 0, false, 12, 0, EVMC_FRONTIER};
    table[OP_SWAP12] = {"SWAP12", 0, false, 13, 0, EVMC_FRONTIER};
    table[OP_SWAP13] = {"SWAP13", 0, false, 14, 0, EVMC_FRONTIER};
    table[OP_SWAP14] = {"SWAP14", 0, false, 15, 0, EVMC_FRONTIER};
    table[OP_SWAP15] = {"SWAP15", 0, false, 16, 0, EVMC_FRONTIER};
    table[OP_SWAP16] = {"SWAP16", 0, false, 17, 0, EVMC_FRONTIER};

    table[OP_LOG0] = {"LOG0", 0, false, 2, -2, EVMC_FRONTIER};
    table[OP_LOG1] = {"LOG1", 0, false, 3, -3, EVMC_FRONTIER};
    table[OP_LOG2] = {"LOG2", 0, false, 4, -4, EVMC_FRONTIER};
    table[OP_LOG3] = {"LOG3", 0, false, 5, -5, EVMC_FRONTIER};
    table[OP_LOG4] = {"LOG4", 0, false, 6, -6, EVMC_FRONTIER};

    table[OP_MCOPY] = {"MCOPY", 0, false, 3, -3, EVMC_CANCUN};

    table[OP_CREATE] = {"CREATE", 0, false, 3, -2, EVMC_FRONTIER};
    table[OP_CALL] = {"CALL", 0, false, 7, -6, EVMC_FRONTIER};
    table[OP_CALLCODE] = {"CALLCODE", 0, false, 7, -6, EVMC_FRONTIER};
    table[OP_RETURN] = {"RETURN", 0, true, 2, -2, EVMC_FRONTIER};
    table[OP_DELEGATECALL] = {"DELEGATECALL", 0, false, 6, -5, EVMC_HOMESTEAD};
    table[OP_CREATE2] = {"CREATE2", 0, false, 4, -3, EVMC_CONSTANTINOPLE};
    table[OP_STATICCALL] = {"STATICCALL", 0, false, 6, -5, EVMC_BYZANTIUM};
    table[OP_REVERT] = {"REVERT", 0, true, 2, -2, EVMC_BYZANTIUM};
    table[OP_INVALID] = {"INVALID", 0, true, 0, 0, EVMC_FRONTIER};
    table[OP_SELFDESTRUCT] = {"SELFDESTRUCT", 0, true, 1, -1, EVMC_FRONTIER};

    return table;
}();

}  // namespace evmone::instr

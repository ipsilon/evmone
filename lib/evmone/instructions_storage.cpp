// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "instructions.hpp"

namespace evmone::instr::core
{
namespace
{
/// The gas cost specification for storage instructions.
struct StorageCostSpec
{
    bool net_cost;        ///< Is this net gas cost metering schedule?
    int16_t warm_access;  ///< Storage warm access cost, YP: G_{warmaccess}
    int16_t set;          ///< Storage addition cost, YP: G_{sset}
    int16_t reset;        ///< Storage modification cost, YP: G_{sreset}
    int16_t clear;        ///< Storage deletion refund, YP: R_{sclear}
};

/// Table of gas cost specification for storage instructions per EVM revision.
/// TODO: This can be moved to instruction traits and be used in other places: e.g.
///       SLOAD cost, replacement for warm_storage_read_cost.
constexpr auto storage_cost_spec = []() noexcept {
    std::array<StorageCostSpec, EVMC_MAX_REVISION + 1> tbl{};

    // Legacy cost schedule.
    for (auto rev : {EVMC_FRONTIER, EVMC_HOMESTEAD, EVMC_TANGERINE_WHISTLE, EVMC_SPURIOUS_DRAGON,
             EVMC_BYZANTIUM, EVMC_PETERSBURG})
        tbl[rev] = {false, 200, 20000, 5000, 15000};

    // Net cost schedule.
    tbl[EVMC_CONSTANTINOPLE] = {true, 200, 20000, 5000, 15000};
    tbl[EVMC_ISTANBUL] = {true, 800, 20000, 5000, 15000};
    tbl[EVMC_BERLIN] = {
        true, instr::warm_storage_read_cost, 20000, 5000 - instr::cold_sload_cost, 15000};
    tbl[EVMC_LONDON] = {
        true, instr::warm_storage_read_cost, 20000, 5000 - instr::cold_sload_cost, 4800};
    tbl[EVMC_PARIS] = tbl[EVMC_LONDON];
    tbl[EVMC_SHANGHAI] = tbl[EVMC_LONDON];
    tbl[EVMC_CANCUN] = tbl[EVMC_LONDON];
    tbl[EVMC_PRAGUE] = tbl[EVMC_LONDON];
    tbl[EVMC_OSAKA] = tbl[EVMC_LONDON];
    tbl[EVMC_AMSTERDAM] = tbl[EVMC_LONDON];
    tbl[EVMC_AMSTERDAM].set = 2900;  // EIP-8037: regular component only (was 20000).
    tbl[EVMC_EXPERIMENTAL] = tbl[EVMC_LONDON];
    return tbl;
}();


struct StorageStoreCost
{
    int16_t gas_cost;
    int16_t gas_refund;
};

// The lookup table of SSTORE costs by the storage update status.
constexpr auto sstore_costs = []() noexcept {
    std::array<std::array<StorageStoreCost, EVMC_STORAGE_MODIFIED_RESTORED + 1>,
        EVMC_MAX_REVISION + 1>
        tbl{};

    for (size_t rev = EVMC_FRONTIER; rev <= EVMC_MAX_REVISION; ++rev)
    {
        auto& e = tbl[rev];
        if (const auto c = storage_cost_spec[rev]; !c.net_cost)  // legacy
        {
            e[EVMC_STORAGE_ADDED] = {c.set, 0};
            e[EVMC_STORAGE_DELETED] = {c.reset, c.clear};
            e[EVMC_STORAGE_MODIFIED] = {c.reset, 0};
            e[EVMC_STORAGE_ASSIGNED] = e[EVMC_STORAGE_MODIFIED];
            e[EVMC_STORAGE_DELETED_ADDED] = e[EVMC_STORAGE_ADDED];
            e[EVMC_STORAGE_MODIFIED_DELETED] = e[EVMC_STORAGE_DELETED];
            e[EVMC_STORAGE_DELETED_RESTORED] = e[EVMC_STORAGE_ADDED];
            e[EVMC_STORAGE_ADDED_DELETED] = e[EVMC_STORAGE_DELETED];
            e[EVMC_STORAGE_MODIFIED_RESTORED] = e[EVMC_STORAGE_MODIFIED];
        }
        else  // net cost
        {
            e[EVMC_STORAGE_ASSIGNED] = {c.warm_access, 0};
            e[EVMC_STORAGE_ADDED] = {c.set, 0};
            e[EVMC_STORAGE_DELETED] = {c.reset, c.clear};
            e[EVMC_STORAGE_MODIFIED] = {c.reset, 0};
            e[EVMC_STORAGE_DELETED_ADDED] = {c.warm_access, static_cast<int16_t>(-c.clear)};
            e[EVMC_STORAGE_MODIFIED_DELETED] = {c.warm_access, c.clear};
            e[EVMC_STORAGE_DELETED_RESTORED] = {
                c.warm_access, static_cast<int16_t>(c.reset - c.warm_access - c.clear)};
            e[EVMC_STORAGE_ADDED_DELETED] = {
                c.warm_access, static_cast<int16_t>(c.set - c.warm_access)};
            e[EVMC_STORAGE_MODIFIED_RESTORED] = {
                c.warm_access, static_cast<int16_t>(c.reset - c.warm_access)};
        }
    }

    return tbl;
}();
}  // namespace

Result sload(StackTop stack, int64_t gas_left, ExecutionState& state) noexcept
{
    auto& x = stack.top();
    const auto key = intx::be::store<evmc::bytes32>(x);

    if (state.rev >= EVMC_BERLIN &&
        state.host.access_storage(state.msg->recipient, key) == EVMC_ACCESS_COLD)
    {
        // The warm storage access cost is already applied (from the cost table).
        // Here we need to apply additional cold storage access cost.
        constexpr auto additional_cold_sload_cost =
            instr::cold_sload_cost - instr::warm_storage_read_cost;
        if ((gas_left -= additional_cold_sload_cost) < 0)
            return {EVMC_OUT_OF_GAS, gas_left};
    }

    x = intx::be::load<uint256>(state.host.get_storage(state.msg->recipient, key));

    return {EVMC_SUCCESS, gas_left};
}

Result sstore(StackTop stack, int64_t gas_left, ExecutionState& state) noexcept
{
    if (state.in_static_mode())
        return {EVMC_STATIC_MODE_VIOLATION, gas_left};

    if (state.rev >= EVMC_ISTANBUL && gas_left <= 2300)
        return {EVMC_OUT_OF_GAS, gas_left};

    const auto key = intx::be::store<evmc::bytes32>(stack.pop());
    const auto value = intx::be::store<evmc::bytes32>(stack.pop());

    const auto gas_cost_cold =
        (state.rev >= EVMC_BERLIN &&
            state.host.access_storage(state.msg->recipient, key) == EVMC_ACCESS_COLD) ?
            instr::cold_sload_cost :
            0;
    const auto status = state.host.set_storage(state.msg->recipient, key, value);

    const auto [gas_cost_warm, gas_refund] = sstore_costs[state.rev][status];
    const auto gas_cost = gas_cost_warm + gas_cost_cold;

    // EIP-8037: charge state gas for state-creating storage operations.
    int64_t state_gas_charged = 0;
    const auto pre_state_gas_left = state.state_gas_left;  // Save for potential rollback.
    if (state.rev >= EVMC_AMSTERDAM)
    {
        if (status == EVMC_STORAGE_ADDED)
        {
            state_gas_charged = 32 * CPSB;
            if (!charge_state_gas(gas_left, state, state_gas_charged))
                return {EVMC_OUT_OF_GAS, gas_left};
        }
        else if (status == EVMC_STORAGE_ADDED_DELETED)
        {
            // Refund state gas for set-then-clear (0 -> Y -> 0).
            state.gas_refund += 32 * CPSB;
        }
    }

    if ((gas_left -= gas_cost) < 0)
    {
        // Regular gas OOG after state gas was charged. Roll back state_gas_used
        // and restore reservoir since the SSTORE didn't complete (state won't grow).
        state.state_gas_used -= state_gas_charged;
        state.state_gas_left = pre_state_gas_left;
        return {EVMC_OUT_OF_GAS, gas_left};
    }
    state.gas_refund += gas_refund;
    return {EVMC_SUCCESS, gas_left};
}
}  // namespace evmone::instr::core

// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2020 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "baseline_instruction_table.hpp"
#include "instructions_traits.hpp"

namespace evmone::baseline
{
namespace
{
consteval auto build_cost_tables() noexcept
{
    std::array<CostTable, EVMC_MAX_REVISION + 1> tables{};
    for (size_t r = EVMC_FRONTIER; r <= EVMC_MAX_REVISION; ++r)
    {
        auto& table = tables[r];
        for (size_t op = 0; op < table.size(); ++op)
        {
            const auto& tr = instr::traits[op];
            const auto since = tr.since;
            table[op] = (since && r >= *since) ? instr::gas_costs[r][op] : instr::undefined;
        }
    }
    return tables;
}

constexpr auto COST_TABLES = build_cost_tables();
}  // namespace

const CostTable& get_baseline_cost_table(evmc_revision rev) noexcept
{
    return COST_TABLES[rev];
}
}  // namespace evmone::baseline

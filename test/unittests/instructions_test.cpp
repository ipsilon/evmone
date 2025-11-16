// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmc/evmc.hpp>
#include <evmone/instructions_traits.hpp>
#include <gtest/gtest.h>

namespace evmone::test
{
namespace
{
constexpr int UNSPECIFIED = -1000000;

consteval int get_revision_defined_in(uint8_t op) noexcept
{
    for (size_t r = EVMC_FRONTIER; r <= EVMC_MAX_REVISION; ++r)
    {
        if (instr::gas_costs[r][op] != instr::undefined)
            return static_cast<int>(r);
    }
    return UNSPECIFIED;
}

consteval bool is_terminating(uint8_t op) noexcept
{
    switch (op)
    {
    case OP_STOP:
    case OP_RETURN:
    case OP_REVERT:
    case OP_INVALID:
    case OP_SELFDESTRUCT:
        return true;
    default:
        return false;
    }
}

template <uint8_t Op>
consteval void validate_traits_of() noexcept
{
    constexpr auto tr = instr::traits[Op];

    // immediate_size
    if constexpr (Op >= OP_PUSH1 && Op <= OP_PUSH32)
        static_assert(tr.immediate_size == Op - OP_PUSH1 + 1);
    else
        static_assert(tr.immediate_size == 0);

    // is_terminating
    static_assert(tr.is_terminating == is_terminating(Op));

    // since
    constexpr auto expected_rev = get_revision_defined_in(Op);
    static_assert(tr.since.has_value() ? *tr.since == expected_rev : expected_rev == UNSPECIFIED);
}

template <std::size_t... Ops>
consteval bool validate_traits(std::index_sequence<Ops...>)
{
    // Instantiate validate_traits_of for each opcode.
    // Validation errors are going to be reported via static_asserts.
    (validate_traits_of<static_cast<Opcode>(Ops)>(), ...);
    return true;
}
static_assert(validate_traits(std::make_index_sequence<256>{}));


// Check some cases for has_const_gas_cost().
static_assert(instr::has_const_gas_cost(OP_STOP));
static_assert(instr::has_const_gas_cost(OP_ADD));
static_assert(instr::has_const_gas_cost(OP_PUSH1));
static_assert(!instr::has_const_gas_cost(OP_SHL));
static_assert(!instr::has_const_gas_cost(OP_BALANCE));
static_assert(!instr::has_const_gas_cost(OP_SLOAD));
}  // namespace

}  // namespace evmone::test

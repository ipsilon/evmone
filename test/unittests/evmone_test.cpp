// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019-2020 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmc/evmc.hpp>
#include <evmone/evmone.h>
#include <evmone/vm.hpp>
#include <gtest/gtest.h>

TEST(evmone, info)
{
    auto vm = evmc::VM{evmc_create_evmone()};
    EXPECT_STREQ(vm.name(), "evmone");
    EXPECT_STREQ(vm.version(), PROJECT_VERSION);
    EXPECT_TRUE(vm.is_abi_compatible());
}

TEST(evmc, result_with_state_gas)
{
    const auto result = evmc::Result{EVMC_SUCCESS, 1, 2, {.left = 3, .spilled = 4}};
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    EXPECT_EQ(result.gas_left, 1);
    EXPECT_EQ(result.gas_refund, 2);
    EXPECT_EQ(result.state_gas_left, 3);
    EXPECT_EQ(result.state_gas_spilled, 4);
    EXPECT_EQ(result.output_data, nullptr);
    EXPECT_EQ(result.output_size, 0);

    const auto default_result = evmc::Result{};
    EXPECT_EQ(default_result.state_gas_left, 0);
    EXPECT_EQ(default_result.state_gas_spilled, 0);

    const uint8_t output[] = {0x01};
    const auto output_result = evmc::Result{EVMC_REVERT, 1, 0, output, std::size(output)};
    EXPECT_EQ(output_result.state_gas_left, 0);
    EXPECT_EQ(output_result.state_gas_spilled, 0);
}

TEST(evmone, set_option_invalid)
{
    auto vm = evmc_create_evmone();
    ASSERT_NE(vm->set_option, nullptr);
    EXPECT_EQ(vm->set_option(vm, "", ""), EVMC_SET_OPTION_INVALID_NAME);
    EXPECT_EQ(vm->set_option(vm, "o", ""), EVMC_SET_OPTION_INVALID_NAME);
    EXPECT_EQ(vm->set_option(vm, "0", ""), EVMC_SET_OPTION_INVALID_NAME);
    vm->destroy(vm);
}

TEST(evmone, set_option_advanced)
{
    auto vm = evmc::VM{evmc_create_evmone()};
    EXPECT_EQ(vm.set_option("advanced", ""), EVMC_SET_OPTION_SUCCESS);

    // This will also enable Advanced.
    EXPECT_EQ(vm.set_option("advanced", "no"), EVMC_SET_OPTION_SUCCESS);
}

TEST(evmone, set_option_cgoto)
{
    evmc::VM vm{evmc_create_evmone()};

#if EVMONE_CGOTO_SUPPORTED
    EXPECT_EQ(vm.set_option("cgoto", ""), EVMC_SET_OPTION_INVALID_VALUE);
    EXPECT_EQ(vm.set_option("cgoto", "yes"), EVMC_SET_OPTION_INVALID_VALUE);
    EXPECT_EQ(vm.set_option("cgoto", "no"), EVMC_SET_OPTION_SUCCESS);
#else
    EXPECT_EQ(vm.set_option("cgoto", "no"), EVMC_SET_OPTION_INVALID_NAME);
#endif
}

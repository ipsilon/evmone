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

TEST(evmone, capabilities)
{
    auto vm = evmc_create_evmone();
    EXPECT_EQ(vm->get_capabilities(vm), evmc_capabilities_flagset{EVMC_CAPABILITY_EVM1});
    vm->destroy(vm);
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

TEST(evmone, set_option_caterpillar)
{
    auto vm = evmc::VM{evmc_create_evmone()};
    EXPECT_EQ(vm.set_option("caterpillar", ""), EVMC_SET_OPTION_SUCCESS);

    // This will also enable Caterpillar.
    EXPECT_EQ(vm.set_option("caterpillar", "no"), EVMC_SET_OPTION_SUCCESS);
}

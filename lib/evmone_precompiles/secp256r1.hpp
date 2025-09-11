// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ecc.hpp"
#include "hash_types.h"

namespace evmmax::secp256r1
{
using namespace intx;

bool verify(const ethash::hash256& e, const uint256& r, const uint256& s, const uint256& qx,
    const uint256& qy) noexcept;

}  // namespace evmmax::secp256r1

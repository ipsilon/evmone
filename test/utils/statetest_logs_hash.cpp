// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2022 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "statetest.hpp"
#include <test/state/rlp.hpp>

namespace evmone::test
{
hash256 logs_hash(const std::vector<state::Log>& logs)
{
    return keccak256(rlp::encode(logs));
}
}  // namespace evmone::test

// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <evmc/bytes.hpp>
#include <test/state/transaction.hpp>

namespace evmone::state
{
/// Defines how to RLP-decode a Transaction.
void rlp_decode(evmc::bytes_view& from, Transaction& to);
}  // namespace evmone::state

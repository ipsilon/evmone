// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <evmc/evmc.hpp>
#include <functional>
#include <iosfwd>
#include <optional>
#include <string>

namespace evmone::tooling
{
/// Arguments for t8n(). Streams are non-owning; the caller manages lifetime.
struct T8NArgs
{
    evmc_revision rev = {};
    uint64_t chain_id = 0;
    std::optional<uint64_t> block_reward;
    bool pre_state_only = false;

    // TODO: Refactor to be filesystem-free; the VM currently opens this file itself.
    std::string opcode_count_file;

    // TODO(C++26): switch the optional stream members to std::optional<std::istream&> /
    // std::optional<std::ostream&>.
    std::istream* alloc = nullptr;        ///< pre-state alloc JSON
    std::istream* env = nullptr;          ///< block env JSON
    std::istream* txs = nullptr;          ///< transactions JSON
    std::istream* blob_params = nullptr;  ///< blob schedule JSON (null = rev default)

    // All outputs are optional. t8n() skips writing to any output that is null.
    std::ostream* out_result = nullptr;
    std::ostream* out_alloc = nullptr;
    std::ostream* out_body = nullptr;

    /// Called once per executed transaction (just before execution) to obtain
    /// a per-tx trace sink; t8n() redirects std::clog to the returned stream
    /// for the duration of that transaction. Unset = tracing disabled.
    std::function<std::ostream&(size_t tx_index, const evmc::bytes32& tx_hash)> open_trace;
};

/// Runs the state transition (t8n), used for JSON tests "filling".
///
/// This command takes some JSON inputs, including a list of transactions, and produces the result
/// post-state as some JSON outputs. The specifics of the JSON formats and options are dictated
/// by execution specs, see https://steel.ethereum.foundation/docs/execution-specs.
///
/// @param vm    The VM instance. The command may modify/overwrite its config (depends on args).
/// @param args  The command arguments.
void t8n(evmc::VM& vm, const T8NArgs& args);
}  // namespace evmone::tooling

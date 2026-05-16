// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "engine_test.hpp"
#include "statetest.hpp"
#include "utils.hpp"

namespace evmone::test
{

namespace
{
template <typename T>
T load_if_exists(const json::json& j, std::string_view key)
{
    if (const auto it = j.find(key); it != j.end())
        return from_json<T>(*it);
    return {};
}

template <typename T>
std::optional<T> load_optional(const json::json& j, std::string_view key)
{
    if (const auto it = j.find(key); it != j.end())
        return from_json<T>(*it);
    return std::nullopt;
}

EnginePayload load_engine_payload(
    const json::json& j, const std::string& network, const BlobSchedule& blob_schedule)
{
    using namespace state;
    EnginePayload p;

    const auto& params = j.at("params");
    if (!params.is_array() || params.size() < 3)
        throw std::runtime_error{"engine payload params: expected an array of length >= 3"};

    const auto& exec_payload = params[0];
    // params[1] (blob versioned hashes) is not stored; covered indirectly by state root.
    const auto& parent_beacon_root_j = params[2];
    // params[3] (execution requests) is not stored; verified implicitly via state root.

    p.block_info.parent_hash = from_json<hash256>(exec_payload.at("parentHash"));
    p.block_info.coinbase = from_json<address>(exec_payload.at("feeRecipient"));
    p.block_info.prev_randao = from_json<bytes32>(exec_payload.at("prevRandao"));
    p.block_info.number = from_json<int64_t>(exec_payload.at("blockNumber"));
    p.block_info.gas_limit = from_json<int64_t>(exec_payload.at("gasLimit"));
    p.block_info.gas_used = from_json<int64_t>(exec_payload.at("gasUsed"));
    p.block_info.timestamp = from_json<int64_t>(exec_payload.at("timestamp"));
    p.block_info.extra_data = from_json<bytes>(exec_payload.at("extraData"));
    p.block_info.base_fee = from_json<uint64_t>(exec_payload.at("baseFeePerGas"));
    p.block_info.blob_gas_used = load_optional<uint64_t>(exec_payload, "blobGasUsed");
    p.block_info.excess_blob_gas = load_optional<uint64_t>(exec_payload, "excessBlobGas");
    p.block_info.parent_beacon_block_root = from_json<hash256>(parent_beacon_root_j);

    // Withdrawals
    if (const auto wit = exec_payload.find("withdrawals"); wit != exec_payload.end())
    {
        for (const auto& w : *wit)
            p.block_info.withdrawals.push_back(from_json<Withdrawal>(w));
    }

    // blob_base_fee, same computation as blockchaintest_loader.cpp
    if (p.block_info.excess_blob_gas.has_value())
    {
        const auto blob_params = get_blob_params(network, blob_schedule, p.block_info.timestamp);
        p.block_info.blob_base_fee =
            compute_blob_gas_price(blob_params, *p.block_info.excess_blob_gas);
    }

    // Raw RLP transactions, no decoding here.
    for (const auto& tx_hex : exec_payload.at("transactions"))
        p.transactions_rlp.push_back(from_json<bytes>(tx_hex));

    p.expected_block_hash = from_json<hash256>(exec_payload.at("blockHash"));
    p.expected_state_root = from_json<hash256>(exec_payload.at("stateRoot"));
    p.expected_receipts_root = from_json<hash256>(exec_payload.at("receiptsRoot"));
    p.expected_logs_bloom = bloom_filter_from_bytes(from_json<bytes>(exec_payload.at("logsBloom")));
    p.expected_gas_used = p.block_info.gas_used;

    if (const auto ve_it = j.find("validationError"); ve_it != j.end())
        p.validation_error = ve_it->get<std::string>();

    return p;
}

EngineTest load_engine_test_case(const std::string& name, const json::json& j)
{
    using namespace state;

    EngineTest et;
    et.name = name;
    et.network = j.at("network").get<std::string>();
    et.rev = to_rev_schedule(et.network);

    if (const auto config_it = j.find("config"); config_it != j.end())
    {
        if (const auto bs_it = config_it->find("blobSchedule"); bs_it != config_it->end())
            et.blob_schedule = from_json<BlobSchedule>(*bs_it);
    }

    et.pre_state = from_json<TestState>(j.at("pre"));
    et.post_state = from_json<TestState>(j.at("postState"));
    et.genesis = from_json<BlockHeader>(j.at("genesisBlockHeader"));
    et.last_block_hash = from_json<hash256>(j.at("lastblockhash"));

    for (const auto& payload_j : j.at("engineNewPayloads"))
        et.payloads.emplace_back(load_engine_payload(payload_j, et.network, et.blob_schedule));

    return et;
}
}  // namespace

std::vector<EngineTest> load_engine_tests(std::string_view json_str)
{
    const auto j = json::json::parse(json_str);
    std::vector<EngineTest> tests;
    for (const auto& [name, entry] : j.items())
    {
        const auto info_it = entry.find("_info");
        if (info_it == entry.end())
            throw UnsupportedTestFeature{"unsupported fixture format: <missing _info>"};
        const auto fmt_it = info_it->find("fixture-format");
        if (fmt_it == info_it->end())
            throw UnsupportedTestFeature{"unsupported fixture format: <missing fixture-format>"};
        const auto fmt = fmt_it->get<std::string>();
        if (fmt != "blockchain_test_engine")
            throw UnsupportedTestFeature{"unsupported fixture format: " + fmt};

        tests.emplace_back(load_engine_test_case(name, entry));
    }
    return tests;
}

}  // namespace evmone::test

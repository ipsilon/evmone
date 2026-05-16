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

    // engineNewPayloads parsing comes in the next task. For now: require the
    // field to exist (per the format) but only accept an empty array.
    const auto& payloads_json = j.at("engineNewPayloads");
    if (!payloads_json.empty())
        throw UnsupportedTestFeature{"engineNewPayloads parsing not implemented yet"};

    return et;
}
}  // namespace

std::vector<EngineTest> load_engine_tests(std::string_view json_str)
{
    const auto j = json::json::parse(json_str);
    std::vector<EngineTest> tests;
    for (const auto& [name, entry] : j.items())
    {
        // Format detection comes in the next task.
        tests.emplace_back(load_engine_test_case(name, entry));
    }
    return tests;
}

}  // namespace evmone::test

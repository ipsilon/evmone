// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "blockchaintest.hpp"
#include "statetest.hpp"
#include "utils.hpp"
#include <test/state/errors.hpp>

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
}  // namespace

template <>
BlockHeader from_json<BlockHeader>(const json::json& j)
{
    return {
        .parent_hash = from_json<hash256>(j.at("parentHash")),
        .coinbase = from_json<address>(j.at("coinbase")),
        .state_root = from_json<hash256>(j.at("stateRoot")),
        .receipts_root = from_json<hash256>(j.at("receiptTrie")),
        .logs_bloom = state::bloom_filter_from_bytes(from_json<bytes>(j.at("bloom"))),
        .difficulty = load_if_exists<int64_t>(j, "difficulty"),
        .prev_randao = load_if_exists<bytes32>(j, "mixHash"),
        .block_number = from_json<int64_t>(j.at("number")),
        .gas_limit = from_json<int64_t>(j.at("gasLimit")),
        .gas_used = from_json<int64_t>(j.at("gasUsed")),
        .timestamp = from_json<int64_t>(j.at("timestamp")),
        .extra_data = from_json<bytes>(j.at("extraData")),
        .base_fee_per_gas = load_if_exists<uint64_t>(j, "baseFeePerGas"),
        .hash = from_json<hash256>(j.at("hash")),
        .transactions_root = from_json<hash256>(j.at("transactionsTrie")),
        .withdrawal_root = load_if_exists<hash256>(j, "withdrawalsRoot"),
        .parent_beacon_block_root = load_if_exists<hash256>(j, "parentBeaconBlockRoot"),
        .blob_gas_used = load_optional<uint64_t>(j, "blobGasUsed"),
        .excess_blob_gas = load_optional<uint64_t>(j, "excessBlobGas"),
        .requests_hash = load_if_exists<hash256>(j, "requestsHash"),
        .slot_number = load_if_exists<uint64_t>(j, "slotNumber"),
    };
}

static TestBlock load_test_block(
    const json::json& j, const std::string& network, const BlobSchedule& blob_schedule)
{
    using namespace state;
    TestBlock tb;

    if (const auto it = j.find("blockHeader"); it != j.end())
    {
        tb.expected_block_header = from_json<BlockHeader>(*it);
        tb.block_info.number = tb.expected_block_header.block_number;
        tb.block_info.timestamp = tb.expected_block_header.timestamp;
        tb.block_info.extra_data = tb.expected_block_header.extra_data;
        tb.block_info.hash = tb.expected_block_header.hash;
        tb.block_info.parent_hash = tb.expected_block_header.parent_hash;

        const auto rev = to_rev_schedule(network).get_revision(tb.block_info.timestamp);
        const auto blob_params = get_blob_params(network, blob_schedule, tb.block_info.timestamp);

        tb.block_info.gas_limit = tb.expected_block_header.gas_limit;
        tb.block_info.gas_used = tb.expected_block_header.gas_used;
        tb.block_info.coinbase = tb.expected_block_header.coinbase;
        tb.block_info.difficulty = tb.expected_block_header.difficulty;
        tb.block_info.prev_randao = tb.expected_block_header.prev_randao;
        tb.block_info.base_fee = tb.expected_block_header.base_fee_per_gas;
        tb.block_info.parent_beacon_block_root = tb.expected_block_header.parent_beacon_block_root;
        tb.block_info.blob_gas_used = tb.expected_block_header.blob_gas_used;
        tb.block_info.excess_blob_gas = tb.expected_block_header.excess_blob_gas;
        tb.block_info.slot_number = tb.expected_block_header.slot_number;

        tb.block_info.blob_base_fee = tb.block_info.excess_blob_gas.has_value() ?
                                          std::optional(state::compute_blob_gas_price(
                                              blob_params, *tb.block_info.excess_blob_gas)) :
                                          std::nullopt;

        // Override prev_randao with difficulty pre-Merge
        if (rev < EVMC_PARIS)
        {
            tb.block_info.prev_randao =
                intx::be::store<bytes32>(intx::uint256{tb.block_info.difficulty});
        }
    }

    if (const auto it = j.find("uncleHeaders"); it != j.end())
    {
        const auto current_block_number = tb.block_info.number;
        for (const auto& ommer : *it)
        {
            tb.block_info.ommers.push_back({from_json<address>(ommer.at("coinbase")),
                static_cast<uint32_t>(
                    current_block_number - from_json<int64_t>(ommer.at("number")))});
        }
    }

    if (const auto withdrawals_it = j.find("withdrawals"); withdrawals_it != j.end())
    {
        try
        {
            for (const auto& withdrawal : *withdrawals_it)
                tb.block_info.withdrawals.push_back(from_json<state::Withdrawal>(withdrawal));
        }
        catch (const std::out_of_range&)
        {
            tb.withdrawals_parse_success = false;
        }
        catch (const std::invalid_argument&)
        {
            tb.withdrawals_parse_success = false;
        }
    }

    if (auto it = j.find("transactions"); it != j.end())
    {
        for (const auto& tx : *it)
            tb.transactions.emplace_back(from_json<Transaction>(tx));
    }

    return tb;
}

namespace
{
/// Maps a legacy "expectException" value to modern EEST-style exception name.
std::string map_legacy_block_exception(std::string_view expected_exception)
{
    using enum state::ErrorCode;
    using Entry = std::pair<std::string_view, state::ErrorCode>;

    static constexpr Entry LEGACY_MAP[]{
        // ethereum/tests (EEST-format):
        {"BlockException.IMPORT_IMPOSSIBLE_UNCLES_OVER_PARIS", INCORRECT_BLOCK_FORMAT},
        {"BlockException.GAS_USED_OVERFLOW", INCORRECT_BLOCK_FORMAT},
        {"BlockException.RLP_STRUCTURES_ENCODING|BlockException.RLP_INVALID_FIELD_OVERFLOW_64",
            INCORRECT_BLOCK_FORMAT},
        // ethereum/legacytests (pre-EEST):
        {"PostParisUncleHashIsNotEmpty", INCORRECT_BLOCK_FORMAT},
        {"3675PreParis1559BlockRejected", INCORRECT_BLOCK_FORMAT},
        {"InvalidNumber", INCORRECT_BLOCK_FORMAT},
        {"InvalidTimestampOlderParent", INVALID_BLOCK_TIMESTAMP_OLDER_THAN_PARENT},
        {"TooMuchGasUsed", INCORRECT_BLOCK_FORMAT},
        {"UncleParentIsNotAncestor", INCORRECT_BLOCK_FORMAT},
        {"InvalidGasLimit2", INVALID_GASLIMIT},
        {"1559BlockImportImpossible_BaseFeeWrong", INVALID_BASEFEE_PER_GAS},
    };

    const auto it = std::ranges::find(LEGACY_MAP, expected_exception, &Entry::first);
    return (it != std::end(LEGACY_MAP)) ? state::make_error_code(it->second).message() :
                                          std::string{expected_exception};
}

BlockchainTest load_blockchain_test_case(const std::string& name, const json::json& j)
{
    using namespace state;

    BlockchainTest bt;
    bt.name = name;
    bt.genesis_block_header = from_json<BlockHeader>(j.at("genesisBlockHeader"));
    bt.pre_state = from_json<TestState>(j.at("pre"));
    bt.network = j.at("network").get<std::string>();
    bt.rev = to_rev_schedule(bt.network);
    if (const auto config_it = j.find("config"); config_it != j.end())
    {
        if (const auto bs_it = config_it->find("blobSchedule"); bs_it != config_it->end())
            bt.blob_schedule = from_json<BlobSchedule>(*bs_it);
    }
    for (const auto& el : j.at("blocks"))
    {
        if (const auto it = el.find("expectException"); it != el.end())
        {
            // `rlp_decoded` holds the `FixtureBlock` element with the relevant block data for
            // invalid blocks within a test. It should be a sibling element to `expectException`.

            // TODO: Add support for invalidly rlp-encoded blocks, which do
            // not have `rlp_decoded`.
            if (!el.contains("rlp_decoded"))
                throw UnsupportedTestFeature(
                    "tests with invalidly rlp-encoded blocks are not supported");

            auto test_block = load_test_block(el.at("rlp_decoded"), bt.network, bt.blob_schedule);
            test_block.expected_exception = map_legacy_block_exception(it->get<std::string>());
            test_block.rlp_size = from_json<bytes>(el.at("rlp")).size();
            bt.test_blocks.emplace_back(test_block);
        }
        else
        {
            auto test_block = load_test_block(el, bt.network, bt.blob_schedule);
            test_block.rlp_size = from_json<bytes>(el.at("rlp")).size();
            bt.test_blocks.emplace_back(test_block);
        }
    }

    bt.expectation.last_block_hash = from_json<hash256>(j.at("lastblockhash"));

    if (const auto it = j.find("postState"); it != j.end())
        bt.expectation.post_state = from_json<TestState>(*it);
    else if (const auto it_hash = j.find("postStateHash"); it_hash != j.end())
        bt.expectation.post_state = from_json<hash256>(*it_hash);

    return bt;
}
}  // namespace

static void from_json(const json::json& j, std::vector<BlockchainTest>& o)
{
    for (const auto& elem_it : j.items())
        o.emplace_back(load_blockchain_test_case(elem_it.key(), elem_it.value()));
}

std::vector<BlockchainTest> load_blockchain_tests(std::istream& input)
{
    return json::json::parse(input).get<std::vector<BlockchainTest>>();
}

}  // namespace evmone::test

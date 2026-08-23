// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "blockchaintest_runner.hpp"
#include <test/state/errors.hpp>
#include <test/state/ethash_difficulty.hpp>
#include <test/state/requests.hpp>
#include <test/state/rlp_decode.hpp>
#include <test/utils/block_transition.hpp>
#include <test/utils/error_matching.hpp>
#include <test/utils/mpt_hash.hpp>
#include <test/utils/rlp.hpp>
#include <test/utils/rlp_encode.hpp>
#include <test/utils/statetest.hpp>
#include <test/utils/test_report.hpp>

namespace evmone::test
{

/// The CL gossip protocol constraint of the maximum block size (EIP-7934).
constexpr size_t MAX_BLOCK_SIZE = 10 * 1024 * 1024;
/// The safety margin for beacon block content (EIP-7934).
constexpr size_t SAFETY_MARGIN = 2 * 1024 * 1024;
/// The maximum EL block size when RLP encoded (EIP-7934).
constexpr size_t MAX_RLP_BLOCK_SIZE = MAX_BLOCK_SIZE - SAFETY_MARGIN;

namespace
{
/// Validates block-level validity unrelated to individual transactions.
///
/// Returns an empty error_code if the block is valid, otherwise the specific validation error.
std::error_code validate_block(evmc_revision rev, state::BlobParams blob_params,
    const TestBlock& test_block, const BlockHeader* parent_header, bool parent_has_ommers) noexcept
{
    using namespace state;

    // Fail if parent header was not found: the block references a parent that is neither the
    // genesis nor any previously-accepted block (an unknown or rejected parent).
    if (parent_header == nullptr)
        return make_error_code(UNKNOWN_PARENT);

    if (test_block.block_info.number != parent_header->block_number + 1)
        return make_error_code(INVALID_BLOCK_NUMBER);

    if (test_block.block_info.gas_used > test_block.block_info.gas_limit)
        return make_error_code(INCORRECT_BLOCK_FORMAT);

    // Some tests have gas limit at INT64_MAX, so we cast to uint64_t to avoid overflow.
    const auto parent_header_gas_limit_u64 = static_cast<uint64_t>(parent_header->gas_limit);
    const auto test_block_gas_limit_u64 = static_cast<uint64_t>(test_block.block_info.gas_limit);
    if (test_block_gas_limit_u64 >=
        parent_header_gas_limit_u64 + parent_header_gas_limit_u64 / 1024)
        return make_error_code(INVALID_GASLIMIT);
    if (test_block_gas_limit_u64 <=
        parent_header_gas_limit_u64 - parent_header_gas_limit_u64 / 1024)
        return make_error_code(INVALID_GASLIMIT);

    // Block gas limit minimum from Yellow Paper.
    if (test_block.block_info.gas_limit < 5000)
        return make_error_code(INVALID_GASLIMIT);

    // FIXME: Some tests have timestamp not fitting into int64_t, type has to be uint64_t.
    if (static_cast<uint64_t>(test_block.block_info.timestamp) <=
        static_cast<uint64_t>(parent_header->timestamp))
        return make_error_code(INVALID_BLOCK_TIMESTAMP_OLDER_THAN_PARENT);

    if (test_block.block_info.difficulty !=
        calculate_difficulty(parent_header->difficulty, parent_has_ommers, parent_header->timestamp,
            test_block.block_info.timestamp, test_block.block_info.number, rev))
        return make_error_code(INCORRECT_BLOCK_FORMAT);

    if (rev >= EVMC_PARIS && !test_block.block_info.ommers.empty())
        return make_error_code(INCORRECT_BLOCK_FORMAT);

    for (const auto& ommer : test_block.block_info.ommers)
    {
        // Check that ommer block number difference with current block is within allowed range.
        // https://github.com/ethereum/execution-specs/blob/ee73be5c4d83a2e3c358bd14990878002e52ba9e/src/ethereum/gray_glacier/fork.py#L623
        if (ommer.delta < 1 || ommer.delta > 6)
            return make_error_code(INCORRECT_BLOCK_FORMAT);
    }

    if (test_block.block_info.extra_data.size() > 32)
        return make_error_code(INCORRECT_BLOCK_FORMAT);

    if (rev >= EVMC_LONDON)
    {
        const auto calculated_base_fee = calc_base_fee(
            parent_header->gas_limit, parent_header->gas_used, parent_header->base_fee_per_gas);
        if (test_block.block_info.base_fee != calculated_base_fee)
            return make_error_code(INVALID_BASEFEE_PER_GAS);
    }

    if (rev >= EVMC_CANCUN)
    {
        // `excess_blob_gas` and `blob_gas_used` mandatory after Cancun and invalid before.
        if (!test_block.block_info.excess_blob_gas.has_value() ||
            !test_block.block_info.blob_gas_used.has_value())
            return make_error_code(INCORRECT_BLOCK_FORMAT);

        // Check that the excess blob gas was updated correctly.
        // According to EIP-7918 current blocks params (`rev`) should be used for parent base fee
        // calculation.
        const auto parent_blob_base_fee =
            compute_blob_gas_price(blob_params, parent_header->excess_blob_gas.value_or(0));
        if (*test_block.block_info.excess_blob_gas !=
            calc_excess_blob_gas(rev, blob_params, parent_header->blob_gas_used.value_or(0),
                parent_header->excess_blob_gas.value_or(0), parent_header->base_fee_per_gas,
                parent_blob_base_fee))
            return make_error_code(INCORRECT_EXCESS_BLOB_GAS);
    }
    else
    {
        if (test_block.block_info.excess_blob_gas.has_value() ||
            test_block.block_info.blob_gas_used.has_value())
            return make_error_code(INCORRECT_BLOCK_FORMAT);
    }

    // `slot_number` is mandatory from Amsterdam and invalid before (EIP-7843).
    if (test_block.block_info.slot_number.has_value() != (rev >= EVMC_AMSTERDAM))
        return make_error_code(INCORRECT_BLOCK_FORMAT);

    // Block is invalid if some of the withdrawal fields failed to be parsed.
    if (!test_block.withdrawals_parse_success)
        return make_error_code(INCORRECT_BLOCK_FORMAT);

    if (rev >= EVMC_OSAKA && test_block.rlp.size() > MAX_RLP_BLOCK_SIZE)
        return make_error_code(RLP_BLOCK_LIMIT_EXCEEDED);

    return {};
}

/// Checks the transaction codec against a block's own serialization: every transaction in it must
/// decode, and encode back to the very same bytes.
void check_transactions_round_trip(bytes_view block_rlp, TestReport& report)
{
    bytes_view body;  // A block is [header, transactions, ...].
    if (!report.check(rlp::take_list_payload(block_rlp, body), "block RLP", "not a list"))
        return;
    if (!report.check(block_rlp.empty(), "block RLP", "trailing bytes after the block"))
        return;
    bytes_view block_header;  // Skipped over.
    if (!report.check(
            rlp::take_list_payload(body, block_header), "block RLP", "the header is not a list"))
        return;
    bytes_view txs;
    if (!report.check(
            rlp::take_list_payload(body, txs), "block RLP", "the transactions are not a list"))
        return;

    while (!txs.empty())
    {
        const auto item = txs;
        rlp::Header h;
        // decode_header() advances txs to the item's payload.
        if (!report.check(
                rlp::decode_header(txs, h), "block RLP", "a transaction has no RLP header"))
            return;
        const auto header_size = item.size() - txs.size();
        txs.remove_prefix(h.payload_length);

        // A legacy transaction is an RLP list here, a typed one an RLP string wrapping the
        // EIP-2718 envelope; the envelope alone is the transaction.
        const auto tx_bytes = h.is_list ? item.substr(0, header_size + h.payload_length) :
                                          item.substr(header_size, h.payload_length);

        const auto tx = state::decode_transaction(tx_bytes);
        if (!tx.has_value())
        {
            report.fail("transaction decoding", hex(tx_bytes));
            return;
        }
        report.check_eq("transaction re-encoding", rlp::encode(*tx), tx_bytes);
    }
}

std::optional<uint64_t> mining_reward(evmc_revision rev) noexcept
{
    if (rev < EVMC_BYZANTIUM)
        return 5'000000000'000000000;
    if (rev < EVMC_PETERSBURG)
        return 3'000000000'000000000;
    if (rev < EVMC_PARIS)
        return 2'000000000'000000000;
    return std::nullopt;
}

std::string print_state(const TestState& s)
{
    std::stringstream out;

    for (const auto& [key, acc] : s)
    {
        out << key << " : \n";
        out << "\tnonce : " << acc.nonce << "\n";
        out << "\tbalance : " << hex0x(acc.balance) << "\n";
        out << "\tcode : " << hex0x(acc.code) << "\n";

        if (!acc.storage.empty())
        {
            out << "\tstorage : \n";
            for (const auto& [s_key, val] : acc.storage)
            {
                if (!is_zero(val))  // Skip 0 values.
                    out << "\t\t" << s_key << " : " << hex0x(val) << "\n";
            }
        }
    }

    return out.str();
}
}  // namespace

void run_blockchain_tests(std::span<const BlockchainTest> tests, evmc::VM& vm, TestReport& report)
{
    for (size_t case_index = 0; case_index != tests.size(); ++case_index)
    {
        const auto& c = tests[case_index];
        const auto rev_schedule = to_rev_schedule(c.network);
        report.start_case(c.name);
        // The network names the whole schedule, so a fork transition shows at the case level and
        // a block needs only its index. The block number would not do: an invalid block does not
        // advance it, so two of them can share one.
        const auto in_case = report.at(c.network, '/', case_index);

        // Validate the genesis block header.
        report.check_eq("genesis block number", c.genesis_block_header.block_number, 0);
        report.check_eq("genesis gas used", c.genesis_block_header.gas_used, 0);
        report.check_eq("genesis transactions root", c.genesis_block_header.transactions_root,
            state::EMPTY_MPT_HASH);
        report.check_eq(
            "genesis receipts root", c.genesis_block_header.receipts_root, state::EMPTY_MPT_HASH);
        report.check_eq("genesis withdrawals root", c.genesis_block_header.withdrawal_root,
            rev_schedule.get_revision(c.genesis_block_header.timestamp) >= EVMC_SHANGHAI ?
                state::EMPTY_MPT_HASH :
                bytes32{});
        report.check_eq("genesis logs bloom", bytes_view{c.genesis_block_header.logs_bloom},
            bytes_view{state::BloomFilter{}});

        TestBlockHashes block_hashes{
            {c.genesis_block_header.block_number, c.genesis_block_header.hash}};

        struct BlockData
        {
            const BlockHeader* header;
            bool has_ommers = false;
            TestState post_state;
            intx::uint256 total_difficulty;
        };
        std::unordered_map<hash256, BlockData> block_data{{{c.genesis_block_header.hash,
            {&c.genesis_block_header, false, c.pre_state, c.genesis_block_header.difficulty}}}};
        const auto* canonical_state = &c.pre_state;
        hash256 canonical_state_root;  // Skip pre-state root hash computation (maybe not needed).
        auto canonical_tip_hash = c.genesis_block_header.hash;
        intx::uint256 max_total_difficulty = c.genesis_block_header.difficulty;

        for (size_t i = 0; i < c.test_blocks.size(); ++i)
        {
            const auto& test_block = c.test_blocks[i];
            const auto& bi = test_block.block_info;

            const auto parent_data_it = block_data.find(test_block.block_info.parent_hash);
            const auto* parent_header =
                parent_data_it != block_data.end() ? parent_data_it->second.header : nullptr;
            const auto parent_has_ommers =
                parent_data_it != block_data.end() && parent_data_it->second.has_ommers;

            const auto rev = rev_schedule.get_revision(bi.timestamp);
            const auto blob_params = get_blob_params(c.network, c.blob_schedule, bi.timestamp);
            const auto blob_gas_limit =
                static_cast<int64_t>(state::max_blob_gas_per_block(blob_params));

            const auto in_block = report.at(i);

            // Invalid blocks are skipped: they may carry transactions that do not even decode.
            if (test_block.expected_exception.empty())
                check_transactions_round_trip(test_block.rlp, report);

            const auto block_error =
                validate_block(rev, blob_params, test_block, parent_header, parent_has_ommers);

            if (test_block.expected_exception.empty())
            {
                if (block_error)
                {
                    report.fail("block validity",
                        "expected the block to be valid: " + block_error.message());
                    // TODO: This and the requests failure below abandon the whole file, not
                    //   just this case. Give each case its own run so only that case stops.
                    return;
                }

                // Block being valid guarantees its parent was found.
                assert(parent_data_it != block_data.end());
                const auto& pre_state = parent_data_it->second.post_state;

                auto res = apply_block(pre_state, vm, bi, block_hashes, test_block.transactions,
                    rev, blob_gas_limit, {.block_reward = mining_reward(rev)});

                if (res.requests_error)
                {
                    report.fail("requests", res.requests_error.message());
                    return;
                }

                block_hashes[test_block.expected_block_header.block_number] =
                    test_block.expected_block_header.hash;
                const auto [inserted_it, _] = block_data.insert({test_block.block_info.hash,
                    {
                        .header = &test_block.expected_block_header,
                        .has_ommers = !test_block.block_info.ommers.empty(),
                        .post_state = std::move(res.block_state),
                        .total_difficulty = parent_data_it->second.total_difficulty +
                                            test_block.block_info.difficulty,
                    }});

                const auto state_root = state::mpt_hash(inserted_it->second.post_state);

                if (inserted_it->second.total_difficulty >= max_total_difficulty)
                {
                    canonical_state = &inserted_it->second.post_state;
                    canonical_state_root = state_root;
                    canonical_tip_hash = test_block.expected_block_header.hash;
                    max_total_difficulty = inserted_it->second.total_difficulty;
                }

                if (!res.rejected.empty())
                {
                    report.fail("transactions in a valid block",
                        "invalid transaction: " + res.rejected.front().error.message());
                }

                report.check_eq("blob gas used", blob_gas_limit - res.blob_gas_left,
                    static_cast<int64_t>(bi.blob_gas_used.value_or(0)));
                report.check_eq(
                    "state root", state_root, test_block.expected_block_header.state_root);

                if (rev >= EVMC_SHANGHAI)
                {
                    report.check_eq("withdrawals root",
                        state::mpt_hash(test_block.block_info.withdrawals),
                        test_block.expected_block_header.withdrawal_root);
                }

                report.check_eq("transactions root", state::mpt_hash(test_block.transactions),
                    test_block.expected_block_header.transactions_root);
                report.check_eq("receipts root", state::mpt_hash(res.receipts),
                    test_block.expected_block_header.receipts_root);
                if (rev >= EVMC_PRAGUE)
                {
                    report.check_eq("requests hash", calculate_requests_hash(res.requests),
                        test_block.expected_block_header.requests_hash);
                }
                report.check_eq(
                    "gas used", res.gas_used, test_block.expected_block_header.gas_used);
                report.check_eq("logs bloom", bytes_view{res.bloom},
                    bytes_view{test_block.expected_block_header.logs_bloom});
            }
            else
            {
                if (block_error)
                {
                    // Block correctly rejected at validation; verify the reason matches the
                    // fixture's expected exception.
                    report.check(
                        is_expected_block_exception(block_error, test_block.expected_exception),
                        "block rejection reason", block_error, test_block.expected_exception);
                    continue;
                }

                // Block being valid guarantees its parent was found.
                assert(parent_data_it != block_data.end());
                const auto& pre_state = parent_data_it->second.post_state;

                // Legacy fixtures name the broken rule in vocabulary evmone does not speak
                // (InvalidStateRoot, TooManyUncles); only the spec names can be compared.
                const auto names_spec_exception =
                    test_block.expected_exception.find("Exception.") != std::string::npos;

                // TODO: The transaction senders come from the fixture instead of being recovered
                //   from the signatures, so evmone never sees the signature the test broke. Such a
                //   transaction executes as the sender the fixture names and the block is rejected
                //   by whatever rule that sender happens to break, or by its state root alone.
                const auto sender_not_recovered = contains_any(
                    test_block.expected_exception, "TransactionException.INVALID_SIGNATURE_VRS");

                const auto res =
                    apply_block(pre_state, vm, bi, block_hashes, test_block.transactions, rev,
                        blob_gas_limit, {.block_reward = mining_reward(rev)});
                if (!res.rejected.empty())
                {
                    // A transaction was rejected: the fixture must name that reason, not merely
                    // some rejection.
                    const auto& rejected = res.rejected.front();
                    if (names_spec_exception && !sender_not_recovered)
                    {
                        report.check(
                            is_expected_tx_exception(rejected.error, test_block.expected_exception),
                            "transaction rejection reason", rejected.error,
                            test_block.expected_exception);
                    }
                    continue;
                }
                if (res.requests_error)
                {
                    if (!sender_not_recovered)
                    {
                        report.check(is_expected_block_exception(
                                         res.requests_error, test_block.expected_exception),
                            "block rejection reason", res.requests_error,
                            test_block.expected_exception);
                    }
                    continue;
                }
                // The block executed, so it is invalid only if it computes something other than
                // its header claims. Each difference below is the symptom of one BlockException:
                // a block failing a check other than the one the fixture names breaks a different
                // rule than the test is about.
                // TODO: Of the ommers only the count and the distance to their nephew are
                //   validated, not the ommer headers themselves, so a fixture that breaks an
                //   ommer's gas limit, number or timestamp reaches execution and lands here.
                const auto ommers_not_validated = !test_block.block_info.ommers.empty();

                // Asserts the fixture names one of @p names, the exceptions the check that just
                // fired is the symptom of. Silent where the reason cannot be compared.
                const auto expect_fixture_names = [&](std::string_view names) {
                    if (!names_spec_exception || ommers_not_validated || sender_not_recovered)
                        return;
                    report.check(contains_any(test_block.expected_exception, names),
                        "block rejection reason", names, test_block.expected_exception);
                };

                if (blob_gas_limit - res.blob_gas_left !=
                    static_cast<int64_t>(bi.blob_gas_used.value_or(0)))
                {
                    expect_fixture_names(
                        "BlockException.INCORRECT_BLOB_GAS_USED|"
                        "BlockException.BLOB_GAS_USED_ABOVE_LIMIT");
                    continue;
                }

                if (state::mpt_hash(res.block_state) != test_block.expected_block_header.state_root)
                {
                    expect_fixture_names("BlockException.INVALID_STATE_ROOT");
                    continue;
                }

                if (rev >= EVMC_SHANGHAI && state::mpt_hash(test_block.block_info.withdrawals) !=
                                                test_block.expected_block_header.withdrawal_root)
                {
                    expect_fixture_names("BlockException.INVALID_WITHDRAWALS_ROOT");
                    continue;
                }
                if (state::mpt_hash(test_block.transactions) !=
                    test_block.expected_block_header.transactions_root)
                {
                    expect_fixture_names("BlockException.INVALID_TRANSACTIONS_ROOT");
                    continue;
                }
                if (state::mpt_hash(res.receipts) != test_block.expected_block_header.receipts_root)
                {
                    expect_fixture_names("BlockException.INVALID_RECEIPTS_ROOT");
                    continue;
                }
                if (rev >= EVMC_PRAGUE && calculate_requests_hash(res.requests) !=
                                              test_block.expected_block_header.requests_hash)
                {
                    expect_fixture_names("BlockException.INVALID_REQUESTS");
                    continue;
                }
                if (res.gas_used != test_block.expected_block_header.gas_used)
                {
                    expect_fixture_names(
                        "BlockException.INVALID_GAS_USED|"
                        "BlockException.GAS_USED_OVERFLOW");
                    continue;
                }
                if (bytes_view{res.bloom} !=
                    bytes_view{test_block.expected_block_header.logs_bloom})
                {
                    expect_fixture_names("BlockException.INVALID_LOG_BLOOM");
                    continue;
                }

                report.fail("block validity", "expected the block to be invalid");
            }
        }
        report.check_eq("canonical chain tip", canonical_tip_hash, c.expectation.last_block_hash);

        const auto expected_post_hash =
            std::holds_alternative<TestState>(c.expectation.post_state) ?
                state::mpt_hash(std::get<TestState>(c.expectation.post_state)) :
                std::get<hash256>(c.expectation.post_state);

        // Get the final state hash. In case none blocks have been applied, compute genesis one.
        const auto canonical_post_hash =
            canonical_state_root ? canonical_state_root : state::mpt_hash(c.pre_state);
        // The state dumps are a callable so that formatting the whole state only happens once
        // the roots are already known to differ.
        report.check_eq("post state root", canonical_post_hash, expected_post_hash, [&] {
            return "Result state:\n" + print_state(*canonical_state) +
                   (std::holds_alternative<TestState>(c.expectation.post_state) ?
                           "\n\nExpected state:\n" +
                               print_state(std::get<TestState>(c.expectation.post_state)) :
                           "");
        });
    }
}

}  // namespace evmone::test

// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2022 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <test/utils/error_matching.hpp>
#include <test/utils/mpt_hash.hpp>
#include <test/utils/rlp.hpp>
#include <test/utils/rlp_encode.hpp>
#include <test/utils/statetest.hpp>
#include <test/utils/test_report.hpp>
#include <iostream>

namespace evmone::test
{
void run_state_test(
    const StateTransitionTest& test, evmc::VM& vm, bool trace_summary, TestReport& report)
{
    report.start_case(test.name);
    for (const auto& [rev, cases, block] : test.cases)
    {
        validate_state(test.pre_state, rev);
        for (size_t case_index = 0; case_index != cases.size(); ++case_index)
        {
            const auto in_case = report.at(evmc::to_string(rev), '/', case_index);
            // if (rev != EVMC_FRONTIER)
            //     continue;
            // if (case_index != 3)
            //     continue;

            const auto& expected = cases[case_index];
            auto state = test.pre_state;
            const auto blob_params = get_blob_params(rev, test.blob_schedule);

            std::optional<state::Transaction> tx;
            std::error_code error;
            if (expected.txbytes.has_value())
            {
                tx = state::decode_transaction(*expected.txbytes);
                if (!tx.has_value())
                {
                    error = make_error_code(state::INVALID_ENCODING);
                }
                else
                {
                    // Decoding is the inverse of encoding: what decoded must encode back exactly.
                    report.check_eq("transaction re-encoding", rlp::encode(*tx), *expected.txbytes);

                    // Recover the signer, as a node does, instead of taking it from JSON.
                    const auto sender = state::recover_sender(*tx, *expected.txbytes);
                    if (sender.has_value())
                        tx->sender = *sender;
                    else
                        error = make_error_code(state::INVALID_SIGNATURE);
                }
            }
            else
            {
                tx = test.multi_tx.get(expected.indexes);
            }

            const auto res =
                error ? error :
                        transition(state, block, test.block_hashes, *tx, rev, vm, block.gas_limit,
                            static_cast<int64_t>(state::max_blob_gas_per_block(blob_params)));

            if (holds_alternative<state::TransactionReceipt>(res))
            {
                // If the transaction is valid, follow the state test convention and do minimal
                // block post-processing with the block reward of 0.
                finalize(state, rev, block.coinbase, 0, {}, {});
            }

            const auto state_root = state::mpt_hash(state);

            if (trace_summary)
            {
                std::clog << '{';
                if (holds_alternative<state::TransactionReceipt>(res))  // if tx valid
                {
                    const auto& r = get<state::TransactionReceipt>(res);
                    if (r.status == EVMC_SUCCESS)
                        std::clog << R"("pass":true)";
                    else
                        std::clog << R"("pass":false,"error":")" << r.status << '"';
                    std::clog << R"(,"gasUsed":"0x)" << std::hex << r.gas_used << R"(",)";
                }
                std::clog << R"("stateRoot":"0x)" << hex(state_root) << "\"}\n";
            }

            if (!expected.exception.empty())
            {
                if (holds_alternative<state::TransactionReceipt>(res))
                {
                    report.fail("transaction validity", "unexpected valid transaction");
                    return;
                }

                // The transaction must be rejected for the reason the fixture states, not merely
                // rejected: a wrong reason is a wrong implementation of the rule being tested.
                const auto& reason = get<std::error_code>(res);
                report.check(is_expected_tx_exception(reason, expected.exception),
                    "transaction rejection reason", reason, expected.exception);

                report.check_eq(
                    "logs hash", logs_hash(std::vector<state::Log>()), expected.logs_hash);
            }
            else
            {
                if (!holds_alternative<state::TransactionReceipt>(res))
                {
                    report.fail("transaction validity",
                        "unexpected invalid transaction: " + get<std::error_code>(res).message());
                    return;
                }

                report.check_eq("logs hash", logs_hash(get<state::TransactionReceipt>(res).logs),
                    expected.logs_hash);
            }

            report.check_eq("state root", state_root, expected.state_hash);
        }
    }
}
}  // namespace evmone::test

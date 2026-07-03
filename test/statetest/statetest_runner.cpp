// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2022 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <test/utils/mpt_hash.hpp>
#include <test/utils/rlp.hpp>
#include <test/utils/statetest.hpp>

namespace evmone::test
{
void run_state_test(const StateTransitionTest& test, evmc::VM& vm, bool trace_summary)
{
    SCOPED_TRACE(test.name);
    for (const auto& [rev, cases, block] : test.cases)
    {
        validate_state(test.pre_state, rev);
        for (size_t case_index = 0; case_index != cases.size(); ++case_index)
        {
            SCOPED_TRACE(std::string{evmc::to_string(rev)} + '/' + std::to_string(case_index));
            // if (rev != EVMC_FRONTIER)
            //     continue;
            // if (case_index != 3)
            //     continue;

            const auto& expected = cases[case_index];
            auto tx = test.multi_tx.get(expected.indexes);
            if (!expected.txbytes.empty())
            {
                // Rely on the signed transaction bytes: decode the real transaction and cross-check
                // the decoded fields against the JSON template as an early check of the decoder.
                // Sender recovery and signature validation come in a later change, so keep the
                // template's sender.
                auto decoded = state::decode_transaction(expected.txbytes);
                ASSERT_TRUE(decoded.has_value()) << "failed to decode txbytes";
                EXPECT_EQ(decoded->type, tx.type);
                EXPECT_EQ(decoded->nonce, tx.nonce);
                EXPECT_EQ(decoded->to, tx.to);
                EXPECT_EQ(decoded->value, tx.value);
                EXPECT_EQ(decoded->data, tx.data);
                EXPECT_EQ(decoded->gas_limit, tx.gas_limit);
                EXPECT_EQ(decoded->max_gas_price, tx.max_gas_price);
                EXPECT_EQ(decoded->max_priority_gas_price, tx.max_priority_gas_price);
                // The sender and the EIP-7702 authorization signers are recovered from the
                // signatures, which a later change adds; the wire carries neither. Until then,
                // carry them over from the JSON template so execution matches.
                decoded->sender = tx.sender;
                ASSERT_EQ(decoded->authorization_list.size(), tx.authorization_list.size());
                for (size_t i = 0; i < decoded->authorization_list.size(); ++i)
                    decoded->authorization_list[i].signer = tx.authorization_list[i].signer;
                tx = std::move(*decoded);
            }
            auto state = test.pre_state;
            const auto blob_params = get_blob_params(rev, test.blob_schedule);

            const auto res = transition(state, block, test.block_hashes, tx, rev, vm,
                block.gas_limit, static_cast<int64_t>(state::max_blob_gas_per_block(blob_params)));

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

            if (expected.exception)
            {
                ASSERT_FALSE(holds_alternative<state::TransactionReceipt>(res))
                    << "unexpected valid transaction";
                EXPECT_EQ(logs_hash(std::vector<state::Log>()), expected.logs_hash);
            }
            else
            {
                ASSERT_TRUE(holds_alternative<state::TransactionReceipt>(res))
                    << "unexpected invalid transaction: " << get<std::error_code>(res).message();
                EXPECT_EQ(logs_hash(get<state::TransactionReceipt>(res).logs), expected.logs_hash);
            }

            EXPECT_EQ(state_root, expected.state_hash);
        }
    }
}
}  // namespace evmone::test

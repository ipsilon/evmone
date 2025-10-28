// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "state_transition.hpp"
#include <test/state/mpt_hash.hpp>
#include <test/statetest/statetest.hpp>
#include <filesystem>
#include <fstream>

namespace evmone::test
{
void state_transition::SetUp()
{
    pre[tx.sender] = {.nonce = 1, .balance = tx.gas_limit * tx.max_gas_price + tx.value + 1};

    // Default expectation (coinbase is added later for valid txs only).
    expect.post[tx.sender].exists = true;
}

class TraceCapture
{
    std::streambuf* m_orig_rdbuf = nullptr;
    std::ostringstream m_trace_stream;

public:
    TraceCapture() { m_orig_rdbuf = std::clog.rdbuf(m_trace_stream.rdbuf()); }
    ~TraceCapture() { std::clog.rdbuf(m_orig_rdbuf); }
    [[maybe_unused]] std::string get_capture() const { return m_trace_stream.str(); }
};

void state_transition::TearDown()
{
    // Validation:

    if (rev < EVMC_LONDON)
    {
        ASSERT_EQ(block.base_fee, 0);
        ASSERT_EQ(tx.type, state::Transaction::Type::legacy);
    }
    if (tx.type == state::Transaction::Type::legacy)
    {
        ASSERT_EQ(tx.max_gas_price, tx.max_priority_gas_price);
    }

    validate_state(pre, rev);

    // Execution:

    auto state = pre;
    const auto trace = !expect.trace.empty();
    auto& selected_vm = trace ? tracing_vm : vm;

    /// Optionally enable trace capturing in form of a RAII object.
    std::optional<TraceCapture> trace_capture;
    if (trace)
        trace_capture.emplace();

    // After EVMC_PRAGUE, get_blob_params will not work like that without a blob schedule.
    // TODO: add a blob schedule to use with state_transition tests, should they be added.
    const auto res = test::transition(state, block, block_hashes, tx, rev, selected_vm,
        block.gas_limit, static_cast<int64_t>(state::max_blob_gas_per_block(get_blob_params(rev))));
    test::finalize(state, rev, block.coinbase, block_reward, block.ommers, block.withdrawals);
    const auto& post = state;

    if (const auto expected_error = make_error_code(expect.tx_error))
    {
        ASSERT_TRUE(holds_alternative<std::error_code>(res))
            << "tx expected to be invalid with error: " << expected_error.message();
        const auto tx_error = std::get<std::error_code>(res);
        EXPECT_EQ(tx_error, expected_error)
            << tx_error.message() << " vs " << expected_error.message();

        EXPECT_EQ(post, pre) << "failed transaction has modified the state";
    }
    else
    {
        ASSERT_TRUE(holds_alternative<TransactionReceipt>(res))
            << std::get<std::error_code>(res).message();
        const auto& receipt = std::get<TransactionReceipt>(res);

        EXPECT_EQ(receipt.status, expect.status);
        if (expect.gas_used.has_value())
        {
            EXPECT_EQ(receipt.gas_used, *expect.gas_used);
        }
        // Update default expectations - valid transaction means coinbase exists unless explicitly
        // requested otherwise
        if (!expect.post.contains(Coinbase))
            expect.post[Coinbase].exists = true;
    }

    if (trace)
    {
        if (expect.trace.starts_with('\n'))  // It's easier to define expected trace with \n.
            expect.trace.remove_prefix(1);
        EXPECT_EQ(trace_capture->get_capture(), expect.trace);
    }

    for (const auto& [addr, expected_acc] : expect.post)
    {
        const auto ait = post.find(addr);
        if (ait == post.end())
        {
            EXPECT_FALSE(expected_acc.exists) << addr << ": should not exist";
            continue;
        }
        EXPECT_TRUE(expected_acc.exists) << addr << ": should exist";

        const auto& acc = ait->second;
        if (expected_acc.nonce.has_value())
        {
            EXPECT_EQ(acc.nonce, *expected_acc.nonce) << addr << ": wrong nonce";
        }
        if (expected_acc.balance.has_value())
        {
            EXPECT_EQ(acc.balance, *expected_acc.balance)
                << addr << ": balance " << to_string(acc.balance) << " vs "
                << to_string(*expected_acc.balance);
        }
        if (expected_acc.code.has_value())
        {
            EXPECT_EQ(acc.code, *expected_acc.code) << addr << ": wrong code";
        }
        for (const auto& [key, expected_value] : expected_acc.storage)
        {
            // Lookup storage values. Map non-existing ones to 0.
            const auto sit = acc.storage.find(key);
            const auto& value = sit != acc.storage.end() ? sit->second : bytes32{};
            EXPECT_EQ(value, expected_value) << addr << ": wrong storage " << key;
        }
        for (const auto& [key, value] : acc.storage)
        {
            // Find unexpected storage keys. This will also report entries with value 0.
            EXPECT_TRUE(expected_acc.storage.contains(key))
                << addr << ": unexpected storage " << key << "=" << value;
        }
    }

    for (const auto& [addr, _] : post)
    {
        EXPECT_TRUE(expect.post.contains(addr)) << addr << ": should not exist";
    }

    if (expect.state_hash)
    {
        EXPECT_EQ(mpt_hash(post), *expect.state_hash);
    }

    if (!export_file_path.empty())
        export_state_test(res, post);
}

void state_transition::export_state_test(
    const std::variant<TransactionReceipt, std::error_code>& res, const TestState& post)
{
    const auto j = to_state_test(export_test_name, block, tx, pre, rev, res, post);
    std::ofstream{export_file_path} << std::setw(2) << j;
}
}  // namespace evmone::test

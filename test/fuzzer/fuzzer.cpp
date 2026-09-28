// evmone-fuzzer: LibFuzzer based testing tool for evmone.
// Copyright 2019 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmone/evmone.h>
#include <test/utils/statetest.hpp>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <random>

using namespace evmone::test;

extern "C" size_t LLVMFuzzerMutate(uint8_t* data, size_t size, size_t max_size);

namespace
{
evmc::VM vm{evmc_create_evmone()};

using Rng = std::minstd_rand;

/// A mutation of one state test's JSON. Returns false if the test has nothing it applies to.
using Mutation = bool (*)(json::json& test, Rng& rng);

struct Strategy
{
    Mutation mutate;
    unsigned priority;  ///< Relative weight among STRATEGIES.
};

/// The element of the object or array @p j at a random position.
auto pick(json::json& j, Rng& rng)
{
    return std::next(j.begin(), static_cast<std::ptrdiff_t>(rng() % j.size()));
}

/// Applies libFuzzer's default mutation to the hex bytes of the JSON string @p field.
void mutate_bytes(json::json& field)
{
    auto bytes = from_hex(field.get<std::string>()).value();
    const auto size = bytes.size();
    bytes.resize(2 * size + 64);  // Room to grow.
    bytes.resize(LLVMFuzzerMutate(bytes.data(), size, bytes.size()));
    field = hex0x(bytes);
}

/// Mutates the code of a random pre-state account.
bool mutate_code(json::json& test, Rng& rng)
{
    auto& pre = test.at("pre");
    if (pre.empty())
        return false;

    mutate_bytes(pick(pre, rng)->at("code"));
    return true;
}

/// Mutates one of the transaction's calldata variants.
bool mutate_calldata(json::json& test, Rng& rng)
{
    auto& data = test.at("transaction").at("data");
    if (data.empty())
        return false;

    mutate_bytes(*pick(data, rng));
    // The cases executing the encoded transaction would not see the change: build it from the
    // fields instead.
    for (auto& [fork, expectations] : test.at("post").items())
    {
        for (auto& e : expectations)
            e.erase("txbytes");
    }
    return true;
}

/// Removes a random account from the pre-state.
bool remove_account(json::json& test, Rng& rng)
{
    auto& pre = test.at("pre");
    if (pre.empty())
        return false;

    pre.erase(pick(pre, rng));
    return true;
}

/// Removes the cases of a random fork, unless it is the only one.
bool remove_fork(json::json& test, Rng& rng)
{
    auto& post = test.at("post");
    if (post.size() < 2)
        return false;

    post.erase(pick(post, rng));
    return true;
}

constexpr Strategy STRATEGIES[] = {
    {mutate_code, 4},
    {mutate_calldata, 2},
    {remove_account, 1},
    {remove_fork, 1},
};

constexpr auto TOTAL_PRIORITY = [] {
    unsigned total = 0;
    for (const auto& strategy : STRATEGIES)
        total += strategy.priority;
    return total;
}();

/// The priority of removing a fixture from an input holding more than one, relative to
/// STRATEGIES. The other fixtures keep their expectations, so nothing is refilled.
constexpr unsigned REMOVE_FIXTURE_PRIORITY = 1;

/// Applies the strategy picked by priority or, if it does not apply, the next one that does.
bool mutate(json::json& test, Rng& rng)
{
    auto r = rng() % TOTAL_PRIORITY;
    size_t first = 0;
    while (r >= STRATEGIES[first].priority)
        r -= STRATEGIES[first++].priority;

    for (size_t i = 0; i != std::size(STRATEGIES); ++i)
    {
        if (STRATEGIES[(first + i) % std::size(STRATEGIES)].mutate(test, rng))
            return true;
    }
    return false;
}

/// Rewrites the expectations of the state @p test to what evmone produces.
/// Throws if the test does not load or its pre-state is invalid in one of its forks.
void refill(const std::string& name, json::json& test)
{
    const auto t = make_state_test(name, test);
    auto fork_cases = t.cases.begin();
    for (auto& [fork, expectations] : test.at("post").items())
    {
        // make_state_test() loads the forks in the order of this object.
        const auto& [rev, cases, block] = *fork_cases++;
        assert(to_rev(fork) == rev);
        validate_state(t.pre_state, rev);
        for (size_t i = 0; i != cases.size(); ++i)
        {
            const auto [tx, res, state_root, logs_hash] =
                execute_state_case(t, rev, block, cases[i], vm);
            auto& e = expectations.at(i);
            e["hash"] = hex0x(state_root);
            e["logs"] = hex0x(logs_hash);
            if (const auto* error = std::get_if<std::error_code>(&res))
                e["expectException"] = error->message();
            else
                e.erase("expectException");
            // Not read by evmone and stale otherwise.
            e.erase("state");
            e.erase("receipt");
        }
    }
    if (const auto info = test.find("_info"); info != test.end())
        info->erase("hash");
}
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const auto j = json::json::parse(data, data + size, nullptr, false);
    if (!j.is_object())
        return -1;

    TestReport report{[](const Failure& failure) {
        std::cerr << failure << '\n';
        std::abort();
    }};
    bool executed = false;
    for (const auto& [name, test] : j.items())
    {
        try
        {
            run_state_test(make_state_test(name, test), vm, {.output = std::clog}, report);
            executed = true;
        }
        catch (const std::exception&)
        {
            // Not a valid state test: skipped.
        }
    }
    return executed ? 0 : -1;
}

namespace
{
/// Parses the input as fixtures, empty if it is not a non-empty JSON object.
json::json parse_fixtures(const uint8_t* data, size_t size)
{
    auto j = json::json::parse(data, data + size, nullptr, false);
    return j.is_object() && !j.empty() ? j : json::json{};
}

/// Refills the state @p test, writes it alone to @p out and returns its size.
/// Returns 0 if the test does not refill or does not fit in @p max_size.
size_t emit(const std::string& name, json::json& test, uint8_t* out, size_t max_size) noexcept
{
    try
    {
        refill(name, test);
        json::json mutant;
        mutant[name] = std::move(test);
        const auto s = mutant.dump();
        if (s.size() > max_size)
            return 0;
        std::memcpy(out, s.data(), s.size());
        return s.size();
    }
    catch (const std::exception&)
    {
        return 0;
    }
}
}  // namespace

extern "C" size_t LLVMFuzzerCustomMutator(
    uint8_t* data, size_t size, size_t max_size, unsigned seed)
{
    auto j = parse_fixtures(data, size);
    if (j.empty())
        return size;

    try
    {
        Rng rng{seed};
        if (j.size() > 1 &&
            rng() % (TOTAL_PRIORITY + REMOVE_FIXTURE_PRIORITY) < REMOVE_FIXTURE_PRIORITY)
        {
            j.erase(pick(j, rng));
            const auto out = j.dump();
            if (out.size() > max_size)
                return size;
            std::memcpy(data, out.data(), out.size());
            return out.size();
        }

        const auto it = pick(j, rng);
        if (!mutate(it.value(), rng))
            return size;
        const auto out_size = emit(it.key(), it.value(), data, max_size);
        return out_size != 0 ? out_size : size;
    }
    catch (const std::exception&)
    {
        return size;
    }
}

/// Copies a random pre-state account of a test of the second input into a test of the first,
/// replacing the account at the same address if there is one. EEST reuses addresses across
/// tests, so a contract often lands where the test calls it.
extern "C" size_t LLVMFuzzerCustomCrossOver(const uint8_t* data1, size_t size1,
    const uint8_t* data2, size_t size2, uint8_t* out, size_t max_out_size, unsigned seed)
{
    auto j1 = parse_fixtures(data1, size1);
    auto j2 = parse_fixtures(data2, size2);
    if (j1.empty() || j2.empty())
        return 0;

    try
    {
        Rng rng{seed};
        const auto it1 = pick(j1, rng);
        auto& donor_pre = pick(j2, rng)->at("pre");
        if (donor_pre.empty())
            return 0;
        const auto account = pick(donor_pre, rng);
        it1->at("pre")[account.key()] = account.value();
        return emit(it1.key(), it1.value(), out, max_out_size);
    }
    catch (const std::exception&)
    {
        return 0;
    }
}

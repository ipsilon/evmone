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

/// Applies libFuzzer's default mutation to the code of a random pre-state account.
bool mutate_code(json::json& test, Rng& rng)
{
    auto& pre = test.at("pre");
    if (pre.empty())
        return false;

    auto& code_field = pick(pre, rng)->at("code");
    auto code = from_hex(code_field.get<std::string>()).value();
    const auto size = code.size();
    code.resize(2 * size + 64);  // Room to grow.
    code.resize(LLVMFuzzerMutate(code.data(), size, code.size()));
    code_field = hex0x(code);
    return true;
}

constexpr Strategy STRATEGIES[] = {
    {mutate_code, 1},
};

/// Applies the strategy picked by priority or, if it does not apply, the next one that does.
bool mutate(json::json& test, Rng& rng)
{
    unsigned total = 0;
    for (const auto& strategy : STRATEGIES)
        total += strategy.priority;

    auto r = rng() % total;
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

extern "C" size_t LLVMFuzzerCustomMutator(
    uint8_t* data, size_t size, size_t max_size, unsigned seed)
{
    auto j = json::json::parse(data, data + size, nullptr, false);
    if (!j.is_object() || j.empty())
        return size;

    try
    {
        Rng rng{seed};
        const auto it = pick(j, rng);
        auto& test = it.value();
        if (!mutate(test, rng))
            return size;
        refill(it.key(), test);

        json::json mutant;
        mutant[it.key()] = std::move(test);
        const auto out = mutant.dump();
        if (out.size() > max_size)
            return size;
        std::memcpy(data, out.data(), out.size());
        return out.size();
    }
    catch (const std::exception&)
    {
        return size;
    }
}

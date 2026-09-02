// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "test_collector.hpp"
#include <algorithm>
#include <iostream>
#include <ranges>

namespace evmone::test
{
namespace fs = std::filesystem;

std::vector<TestFile> collect_test_files(const fs::path& root)
{
    static constexpr auto is_test_file = [](const fs::directory_entry& entry) {
        return entry.is_regular_file() && entry.path().extension() == ".json" &&
               entry.path().filename() != "index.json";
    };
    const auto as_test_file = [&root](const fs::directory_entry& entry) {
        return TestFile{entry.path(), fs::relative(entry.path(), root).parent_path().string()};
    };
    // TODO(gcc-12): Pipe the temporary in directly. Adapting one needs owning_view, which C++20
    //   has but gcc-11's libstdc++ does not implement.
    const fs::recursive_directory_iterator entries{root};

    // TODO(C++23): std::ranges::to<std::vector>() replaces the vector and the copy.
    std::vector<TestFile> files;
    std::ranges::copy(
        entries | std::views::filter(is_test_file) | std::views::transform(as_test_file),
        std::back_inserter(files));
    std::ranges::sort(files);
    return files;
}

void ignore_test_files(std::vector<TestFile>& files, std::span<const fs::path> ignored)
{
    // Whether the path begins with every component of the prefix.
    static constexpr auto is_under = [](const fs::path& path, const fs::path& prefix) {
        // "./B" has to name what "B" names, and a trailing separator, which tab completion adds,
        // is an empty final component of its own.
        auto p = prefix.lexically_normal();
        if (p.filename().empty())
            p = p.parent_path();
        // An empty prefix, which an unset variable expands to, names nothing rather than
        // everything.
        return !p.empty() && std::ranges::mismatch(p, path).in1 == p.end();
    };

    std::erase_if(files, [ignored](const TestFile& file) {
        // The suite name is the file's directory relative to the root, which is what the ignored
        // paths are relative to as well.
        const auto relative = fs::path{file.suite_name} / file.path.filename();
        return std::ranges::any_of(
            ignored, [&relative](const fs::path& prefix) { return is_under(relative, prefix); });
    });
}

bool collect_tests(
    std::vector<TestCase>& cases, const fs::path& root, const TestSettings& settings, evmc::VM& vm)
{
    if (is_directory(root))
    {
        auto files = collect_test_files(root);
        ignore_test_files(files, settings.ignored);
        cases.reserve(cases.size() + files.size());
        for (const auto& file : files)
        {
            // Loaded when the test runs: loading a whole tree up front costs far more.
            cases.push_back(
                {file.path.string(), [path = file.path, &settings, &vm](TestReport& report) {
                     run_fixture_file(path, settings, vm, report);
                 }});
        }
        return true;
    }

    // Naming a file loads it now, to name the fixtures in it. One which cannot be loaded, or
    // which is not a test, becomes a single test reporting why.
    json::json file;
    try
    {
        file = load_fixture_file(root);
    }
    catch (const UnsupportedTestFeature&)
    {
        // A skip, as when collected from a directory, not a broken collection.
        cases.push_back({root.string(),
            [error = std::current_exception()](auto&) { std::rethrow_exception(error); }});
        return true;
    }
    catch (const std::exception& ex)
    {
        // Also reported here: --collect-only never runs the test.
        std::cerr << root.string() << ": " << ex.what() << '\n';
        cases.push_back({root.string(),
            [error = std::current_exception()](auto&) { std::rethrow_exception(error); }});
        return false;
    }

    for (const auto& [name, fixture] : file.items())
    {
        if (!settings.selects(name))
            continue;
        cases.push_back(
            {root.string() + "::" + name, [name, fixture, &settings, &vm](TestReport& report) {
                 run_fixture(name, fixture, settings, vm, report);
             }});
    }
    return true;
}
}  // namespace evmone::test

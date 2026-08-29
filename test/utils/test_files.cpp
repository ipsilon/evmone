// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "test_files.hpp"
#include <algorithm>
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
}  // namespace evmone::test

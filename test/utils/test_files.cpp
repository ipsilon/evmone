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
}  // namespace evmone::test

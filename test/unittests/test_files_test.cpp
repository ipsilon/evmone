// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <test/utils/test_files.hpp>

using namespace evmone::test;
namespace fs = std::filesystem;

namespace
{
/// A collection as collect_test_files() would return it, with a sibling directory whose name
/// begins with another one's.
std::vector<TestFile> collected()
{
    return {
        {"root/bc4895/a.json", "bc4895"},
        {"root/bc4895/nested/c.json", "bc4895/nested"},
        {"root/bc4895-withdrawals/b.json", "bc4895-withdrawals"},
        {"root/top.json", ""},
    };
}

std::vector<std::string> names(const std::vector<TestFile>& files)
{
    std::vector<std::string> result;
    result.reserve(files.size());
    for (const auto& f : files)
        result.push_back(f.path.filename().string());
    return result;
}
}  // namespace

TEST(test_files, ignore_nothing)
{
    const std::vector<std::string> all{"a.json", "c.json", "b.json", "top.json"};

    auto files = collected();
    ignore_test_files(files, {});
    EXPECT_EQ(names(files), all);

    // An empty path, which an unset variable expands to, must not drop everything. Neither must
    // ".", which names the search root: pytest also collects it all for --ignore of the root.
    const std::vector<fs::path> ignored{"", ".", "./"};
    ignore_test_files(files, ignored);
    EXPECT_EQ(names(files), all);
}

TEST(test_files, ignore_directory)
{
    // The sibling shares the prefix as text, but not as a path component.
    auto files = collected();
    const std::vector<fs::path> ignored{"bc4895"};
    ignore_test_files(files, ignored);
    EXPECT_EQ(names(files), (std::vector<std::string>{"b.json", "top.json"}));
}

TEST(test_files, ignore_directory_other_spellings)
{
    for (const auto& spelling : {"bc4895/", "./bc4895"})
    {
        auto files = collected();
        const std::vector<fs::path> ignored{spelling};
        ignore_test_files(files, ignored);
        EXPECT_EQ(names(files), (std::vector<std::string>{"b.json", "top.json"})) << spelling;
    }
}

TEST(test_files, ignore_files)
{
    auto files = collected();
    const std::vector<fs::path> ignored{"bc4895/nested/c.json", "top.json"};
    ignore_test_files(files, ignored);
    EXPECT_EQ(names(files), (std::vector<std::string>{"a.json", "b.json"}));
}

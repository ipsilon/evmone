// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "test_driver.hpp"
#include <test/utils/blockchaintest.hpp>
#include <test/utils/statetest.hpp>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

namespace evmone::test
{
namespace fs = std::filesystem;

namespace
{
/// The report is laid out like pytest's.
constexpr int LINE_WIDTH = 72;
constexpr int PROGRESS_WIDTH = 60;

void banner(std::ostream& out, std::string_view title, char fill = '=')
{
    const auto padding = LINE_WIDTH - static_cast<int>(title.size()) - 2;
    const auto left = std::max(padding / 2, 1);
    out << std::string(static_cast<size_t>(left), fill) << ' ' << title << ' '
        << std::string(static_cast<size_t>(std::max(padding - left, 1)), fill) << '\n';
}

/// A file with something to report: how it counts, and every fixture of it which did not pass.
struct Note
{
    std::string name;
    Outcome outcome;
    std::vector<Result> results;
};

/// One progress character per test, wrapped, each line ending in the percentage done.
class Progress
{
    std::ostream& m_out;
    size_t m_total;
    size_t m_done = 0;
    int m_column = 0;

public:
    Progress(std::ostream& out, size_t total) noexcept : m_out{out}, m_total{total} {}

    void advance(Outcome outcome)
    {
        m_out << static_cast<char>(outcome);
        ++m_done;
        if (++m_column != PROGRESS_WIDTH && m_done != m_total)
            return;

        m_out << std::string(static_cast<size_t>(PROGRESS_WIDTH - m_column), ' ') << " ["
              << std::setw(3) << m_done * 100 / m_total << "%]\n"
              << std::flush;
        m_column = 0;
    }
};

/// What this tool makes of one fixture. EEST names the format in each fixture's "_info", and a
/// heuristic covers the hand-written and pre-EEST files which have no "_info" at all.
enum class Format
{
    state_test,
    blockchain_test,
    unsupported,  ///< A fixture, in a format this tool does not run.
    not_a_test,   ///< Not a fixture at all.
};

Format classify(const json::json& fixture)
{
    if (const auto info = fixture.find("_info"); info != fixture.end())
    {
        if (const auto format = info->find("fixture-format"); format != info->end())
        {
            if (*format == "state_test")
                return Format::state_test;
            if (*format == "blockchain_test")
                return Format::blockchain_test;
            return Format::unsupported;
        }
    }
    // Nothing declares the format: a hand-written or pre-EEST file, or an "_info" without one.
    // Each shape is named by the state it starts from and what is applied to it, never by what
    // it expects, so a fixture whose expectations are missing is still a test and is run.
    // Anything else is not a test at all, as EEST's shared pre-allocation is not.
    if (fixture.contains("pre") && fixture.contains("blocks"))
        return Format::blockchain_test;
    if (fixture.contains("pre") && fixture.contains("transaction"))
        return Format::state_test;
    return Format::not_a_test;
}

/// Parses the fixture file at @p path. Throws UnsupportedTestFeature for a file with no fixture
/// in it, which is nothing to run: fixture directories hold other JSON beside the fixtures.
json::json load_fixture_file(const fs::path& path)
{
    std::ifstream f{path};
    // A stream which never opened reads as EOF, which parses as a syntax error in a file which
    // has none.
    if (!f)
        throw std::runtime_error{"cannot open the file"};
    const auto contents = json::json::parse(f);
    // Not one fixture in it: EEST keeps its shared pre-allocation and an index of the fixtures
    // beside them, and neither is a test. Nor is a document which is not an object at all:
    // items() would walk an array by index, naming its elements "0", "1", ...
    if (!contents.is_object() || std::ranges::none_of(contents.items(), [](const auto& i) {
            return classify(i.value()) != Format::not_a_test;
        }))
        throw UnsupportedTestFeature{"not a test"};
    return contents;
}

/// Runs one fixture of a fixture file. One this tool does not recognise is a fault in the file;
/// one in a format it does not run is skipped.
void run_fixture(const std::string& name, const json::json& fixture, const TestSettings& settings,
    evmc::VM& vm, TestReport& report)
{
    report.start_case(name);  // Names whatever the load itself reports.
    switch (classify(fixture))
    {
    case Format::state_test:
        run_state_test(make_state_test(name, fixture), vm, settings.trace_summary, report);
        break;
    case Format::blockchain_test:
        run_blockchain_test(make_blockchain_test(name, fixture), vm, report);
        break;
    case Format::unsupported:
        throw UnsupportedTestFeature{
            "unsupported fixture format: " + fixture.at("_info").at("fixture-format").dump()};
    case Format::not_a_test:
        // The rest of the file holds fixtures, so this one is broken.
        report.fail("not a test");
        break;
    }
}

}  // namespace

Result run_one(std::string name, const std::function<void(TestReport&)>& run)
{
    Result result{.name = std::move(name)};
    TestReport report{[&result](const Failure& failure) { result.failures.push_back(failure); }};
    report.start_case(result.name);  // Names whatever the run itself reports.

    std::string exception_reason;
    try
    {
        run(report);
    }
    catch (const UnsupportedTestFeature& ex)
    {
        result.outcome = Outcome::skipped;
        result.reason = ex.what();
    }
    catch (const std::exception& ex)
    {
        // One unloadable fixture in a tree of thousands fails its own test, not the run.
        report.fail("exception", ex.what());
        exception_reason = concat("exception: ", ex.what());
    }
    catch (...)
    {
        report.fail("exception", "not derived from std::exception");
        exception_reason = "exception not derived from std::exception";
    }

    // A recorded failure outranks giving up afterwards, in the summary too: the exception is
    // the reason only when nothing failed before it threw.
    if (!result.failures.empty())
    {
        result.outcome = Outcome::failed;
        result.reason = result.failures.size() == 1 && !exception_reason.empty() ?
                            std::move(exception_reason) :
                            result.failures.front().what;
    }
    return result;
}

int run_tests(std::span<const TestCase> cases, std::ostream& out, const RunOptions& options)
{
    if (options.collect_only)
    {
        for (const auto& test : cases)
            out << test.name << '\n';
        return cases.empty() ? NOTHING_VERIFIED : SUCCESS;
    }

    const auto started = std::chrono::steady_clock::now();

    banner(out, "test session starts");
    out << "collected " << cases.size() << (cases.size() == 1 ? " file\n\n" : " files\n\n");

    std::vector<Note> notes;
    Progress row{out, cases.size()};
    size_t failed = 0;
    size_t skipped = 0;
    size_t passed = 0;

    for (const auto& test : cases)
    {
        if (!options.progress)
            out << test.name << '\n';  // The only thing naming what the test prints next.
        out << std::flush;

        // Held until the run ends, as pytest holds them, so nothing interleaves.
        std::vector<Result> results;
        try
        {
            results = test.run();
        }
        catch (...)
        {
            // A test which throws rather than reporting is the one result which says so.
            const auto error = std::current_exception();
            results.push_back(
                run_one(test.name, [&error](TestReport&) { std::rethrow_exception(error); }));
        }
        // A test writes its own output, an EVM trace above all, to another stream.
        std::clog << std::flush;

        // The file counts once, for the worst its fixtures reached. It is skipped only when
        // nothing in it ran at all, so one fixture running is enough to give it a verdict.
        static constexpr auto is = [](Outcome outcome) {
            return [outcome](const Result& result) { return result.outcome == outcome; };
        };
        auto outcome = Outcome::passed;
        if (std::ranges::any_of(results, is(Outcome::failed)))
            outcome = Outcome::failed;
        else if (!results.empty() && std::ranges::none_of(results, is(Outcome::passed)))
            outcome = Outcome::skipped;

        ++(outcome == Outcome::failed ? failed : outcome == Outcome::skipped ? skipped : passed);

        // Every fixture which did not pass is named, including one declined by a file which
        // passed on the fixtures beside it. Otherwise it would vanish from a green run.
        std::erase_if(results, is(Outcome::passed));
        if (!results.empty())
            notes.push_back({test.name, outcome, std::move(results)});

        if (options.progress)
            row.advance(outcome);
    }

    if (failed != 0)
    {
        out << '\n';
        banner(out, "FAILURES");
        for (const auto& note : notes)
        {
            if (note.outcome != Outcome::failed)
                continue;
            banner(out, note.name, '_');
            for (const auto& result : note.results)
            {
                for (const auto& failure : result.failures)
                    out << failure << '\n';
            }
        }
    }

    if (!notes.empty())
    {
        out << '\n';
        banner(out, "short test summary info");
        for (const auto& note : notes)
        {
            for (const auto& result : note.results)
            {
                out << (result.outcome == Outcome::failed ? "FAILED  " : "SKIPPED ") << result.name;
                if (!result.reason.empty())
                    out << " - " << result.reason;
                out << '\n';
            }
        }
    }

    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - started;
    std::ostringstream summary;
    if (failed != 0)
        summary << failed << " failed, ";
    summary << passed << " passed";
    if (skipped != 0)
        summary << ", " << skipped << " skipped";
    summary << " in " << std::fixed << std::setprecision(2) << elapsed.count() << "s";
    out << '\n';
    banner(out, std::move(summary).str());

    if (failed != 0)
        return TESTS_FAILED;
    // No test passed: nothing was collected, or every test was skipped. A test which holds no
    // case of its own still counts as passed, which this does not change.
    return passed == 0 ? NOTHING_VERIFIED : SUCCESS;
}

std::vector<Result> run_fixture_file(
    const fs::path& path, const TestSettings& settings, evmc::VM& vm)
{
    // Named, because items() only borrows: iterating a temporary dangles.
    json::json contents;
    // A file which does not parse, or holds no fixture at all, never gets as far as one: it is
    // itself the only result there is to report.
    if (auto loaded =
            run_one(path.string(), [&](TestReport&) { contents = load_fixture_file(path); });
        loaded.outcome != Outcome::passed)
        return {std::move(loaded)};

    std::vector<Result> results;
    for (const auto& [name, fixture] : contents.items())
    {
        if (!settings.selects(name))
            continue;
        results.push_back(run_one(path.string() + "::" + name,
            [&](TestReport& report) { run_fixture(name, fixture, settings, vm, report); }));
    }
    return results;
}

}  // namespace evmone::test

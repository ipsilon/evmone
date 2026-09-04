// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "test_driver.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

namespace evmone::test
{
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
    out << "collected " << cases.size() << (cases.size() == 1 ? " test\n\n" : " tests\n\n");

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
}  // namespace evmone::test

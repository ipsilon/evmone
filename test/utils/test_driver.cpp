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

/// The outcome of one test, spelled as the progress character for it.
enum class Outcome : char
{
    passed = '.',
    failed = 'F',
    skipped = 's',
};

void banner(std::ostream& out, std::string_view title, char fill = '=')
{
    const auto padding = LINE_WIDTH - static_cast<int>(title.size()) - 2;
    const auto left = std::max(padding / 2, 1);
    out << std::string(static_cast<size_t>(left), fill) << ' ' << title << ' '
        << std::string(static_cast<size_t>(std::max(padding - left, 1)), fill) << '\n';
}

/// A test which did not pass: what the summary says about it and what it recorded.
struct Note
{
    Outcome outcome;
    std::string name;
    std::string reason;
    std::vector<Failure> failures;
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
    size_t passed = 0;
    size_t failed = 0;
    size_t skipped = 0;
    Progress row{out, cases.size()};

    for (const auto& test : cases)
    {
        // Held until the run ends, as pytest holds them, so nothing interleaves.
        std::vector<Failure> failures;
        TestReport report{[&failures](const Failure& failure) { failures.push_back(failure); }};
        report.start_case(test.name);

        auto outcome = Outcome::passed;
        std::string reason;
        std::string exception_reason;
        std::string skip_reason;
        if (!options.progress)
            out << test.name << '\n';  // The only thing naming what the test prints next.
        out << std::flush;
        try
        {
            test.run(report);
        }
        catch (const UnsupportedTestFeature& ex)
        {
            outcome = Outcome::skipped;
            skip_reason = ex.what();
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
        // A test writes its own output, an EVM trace above all, to another stream.
        std::clog << std::flush;

        // A recorded failure outranks giving up afterwards, in the summary too: the exception
        // is the reason only when nothing failed before it threw.
        if (!failures.empty())
        {
            outcome = Outcome::failed;
            reason = failures.size() == 1 && !exception_reason.empty() ?
                         std::move(exception_reason) :
                         failures.front().what;
        }
        else if (outcome == Outcome::skipped)
        {
            reason = std::move(skip_reason);
        }
        if (outcome != Outcome::passed)
            notes.push_back({outcome, test.name, std::move(reason), std::move(failures)});

        switch (outcome)
        {
        case Outcome::passed:
            ++passed;
            break;
        case Outcome::failed:
            ++failed;
            break;
        case Outcome::skipped:
            ++skipped;
            break;
        }
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
            for (const auto& failure : note.failures)
                out << failure << '\n';
        }
    }

    if (!notes.empty())
    {
        out << '\n';
        banner(out, "short test summary info");
        for (const auto& note : notes)
        {
            out << (note.outcome == Outcome::failed ? "FAILED  " : "SKIPPED ") << note.name;
            if (!note.reason.empty())
                out << " - " << note.reason;
            out << '\n';
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

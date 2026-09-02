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

/// The fixture formats this tool runs. EEST names the format in each fixture's "_info", and a
/// heuristic covers the hand-written and pre-EEST files which have no "_info" at all.
enum class Format
{
    state_test,
    blockchain_test,
};

/// What this tool makes of one fixture.
struct Classification
{
    /// How to run it, when this tool runs it.
    std::optional<Format> format;
    /// A fixture, whether or not this tool runs it.
    bool is_fixture = false;
    /// Why it is not run, when it is not.
    std::string reason;
};

Classification classify(const json::json& fixture)
{
    if (const auto info = fixture.find("_info"); info != fixture.end())
    {
        if (const auto format = info->find("fixture-format"); format != info->end())
        {
            if (*format == "state_test")
                return {.format = Format::state_test, .is_fixture = true};
            if (*format == "blockchain_test")
                return {.format = Format::blockchain_test, .is_fixture = true};
            return {.is_fixture = true, .reason = "unsupported fixture format: " + format->dump()};
        }
    }
    // Nothing declares the format: a hand-written or pre-EEST file, or an "_info" without one.
    // Each shape is named by what only it carries; anything else is not a test at all, as EEST's
    // shared pre-allocation kept beside the fixtures is not.
    if (fixture.contains("blocks"))
        return {.format = Format::blockchain_test, .is_fixture = true};
    if (fixture.contains("transaction") && fixture.contains("post"))
        return {.format = Format::state_test, .is_fixture = true};
    return {.reason = "not a test"};
}

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
            reason = ex.what();
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
        if (outcome != Outcome::passed)
            notes.push_back({outcome, test.name, std::move(reason), std::move(failures)});

        if (options.progress)
            row.advance(outcome);
    }

    // Every test which did not pass left exactly one note, so the counts follow from them.
    const auto failed = std::ranges::count(notes, Outcome::failed, &Note::outcome);
    const auto skipped = std::ranges::count(notes, Outcome::skipped, &Note::outcome);
    const auto passed = cases.size() - notes.size();

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

json::json load_fixture_file(const fs::path& path)
{
    std::ifstream f{path};
    auto contents = json::json::parse(f);
    // Not one fixture in it: EEST keeps its shared pre-allocation and an index of the fixtures
    // beside them, and neither is a test.
    if (std::ranges::none_of(
            contents.items(), [](const auto& item) { return classify(item.value()).is_fixture; }))
        throw UnsupportedTestFeature{"not a test"};
    return contents;
}

void run_fixture_file(
    const fs::path& path, const TestSettings& settings, evmc::VM& vm, TestReport& report)
{
    // Named, because items() only borrows: iterating a temporary dangles.
    const auto contents = load_fixture_file(path);

    std::optional<std::string> declined;  // The reason for the first fixture this tool declined.
    bool any_ran = false;
    for (const auto& [name, fixture] : contents.items())
    {
        if (!settings.selects(name))
            continue;
        try
        {
            run_fixture(name, fixture, settings, vm, report);
        }
        catch (const UnsupportedTestFeature& ex)
        {
            // This tool's own limit: a format it does not run, or a fixture its loader refuses.
            if (!declined)
                declined = ex.what();
            continue;
        }
        catch (const std::exception& ex)
        {
            // The fixture is a test and it went wrong, which is this file's verdict but not the
            // end of it: the fixtures after it are still worth running.
            report.fail(concat("exception: ", ex.what()));
        }
        any_ran = true;
    }

    // A file in which this tool ran nothing it was asked for is skipped, not passed.
    // TODO: A file whose cases -k all deselected still passes, as it did before this command
    //   existed, so a filter which matches nothing turns a failing tree green. Skip it instead,
    //   and an empty selection reaches NOTHING_VERIFIED on its own.
    if (!any_ran && declined)
        throw UnsupportedTestFeature{*declined};
}

void run_fixture(const std::string& name, const json::json& fixture, const TestSettings& settings,
    evmc::VM& vm, TestReport& report)
{
    report.start_case(name);  // Names whatever the load itself reports.
    const auto [format, is_fixture, reason] = classify(fixture);
    if (format == Format::state_test)
        run_state_test(make_state_test(name, fixture), vm, settings.trace_summary, report);
    else if (format == Format::blockchain_test)
        run_blockchain_test(make_blockchain_test(name, fixture), vm, report);
    else if (is_fixture)
        throw UnsupportedTestFeature{reason};  // A format this tool does not run.
    else
        report.fail(reason);  // The rest of the file holds fixtures, so this one is broken.
}
}  // namespace evmone::test

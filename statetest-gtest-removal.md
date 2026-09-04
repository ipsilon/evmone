# Removing gtest from evmone-statetest and evmone-blockchaintest

Roadmap item 7. Review of what gtest actually provides today and what has to replace it.

Measured on this worktree (`4fab3dbf`) against EEST `tests@v20.0.1`:

| | files/tests | wall time | stdout |
|---|---|---|---|
| `evmone-statetest fixtures/state_tests` | 8172 tests / 7067 "suites" | 31.5 s | 37 551 lines, 3.0 MB |
| `evmone-blockchaintest fixtures/blockchain_tests` | 8618 tests, 6 skipped | 51.5 s | 38 959 lines, 3.2 MB |

Both fully green. 76 k lines / 6.3 MB of CI log per run says nothing but "everything passed".

`GTest` stays a Hunter dependency either way — `evmone-unittests` is the other user and keeps
it. The win here is code, CLI and log ergonomics, not one fewer package.

## 1. Inventory: what is used

### 1.1 Registration and driving (`statetest.cpp`, `blockchaintest.cpp`)

| gtest | sites | role |
|---|---|---|
| `testing::Test` subclass + `TestBody()` | 4 classes | the unit of registration/reporting |
| `testing::RegisterTest(suite, name, …, factory)` | 4 | dynamic registration from the directory walk |
| `RUN_ALL_TESTS()` | 2 | driver loop + exit code |
| `testing::InitGoogleTest(&argc, argv)` | 2 | strips `--gtest_*` before CLI11 sees argv |
| `testing::FLAGS_gtest_filter` (assigned) | 1 | the default legacy slow-test skip list |
| `GTEST_SKIP()` | 1 | `UnsupportedTestFeature` (6 blockchain fixtures) |

Test identity today: **one JSON file = one test**, named `<dir relative to root>.<file stem>`
in directory mode, `<full path>.<test name>` in single-file mode. 7067 suites for 8172 tests —
the suite level is almost always 1:1, it just doubles the output.

### 1.2 Assertions

| macro | statetest_runner | blockchaintest_runner |
|---|---|---|
| `EXPECT_EQ` | 4 | 17 |
| `EXPECT_TRUE` | 1 | 6 |
| `ASSERT_TRUE` | 1 | 6 |
| `ASSERT_FALSE` | 1 | 2 |
| `SCOPED_TRACE` | 2 | 2 |

40 assertion sites, 4 trace scopes. Many stream an extra `<<` message, and those messages are
load-bearing — three ctest integration tests match on them
(`unexpected invalid transaction: TransactionException.NONCE_MISMATCH_TOO_HIGH` and friends).

`EXPECT_TRUE(false) << "Expected block to be invalid but resulted valid"`
(`blockchaintest_runner.cpp:457`) is an unconditional-fail site — wants a `FAIL()` equivalent.

### 1.3 Implicit behaviour that is easy to miss

Not written anywhere in the sources, but the runners depend on it:

1. **Exceptions become failures, not aborts.** `--gtest_catch_exceptions` defaults on, so a
   malformed JSON file inside an 8000-file tree fails that one test and the run continues.
   Verified: a garbage `.json` yields
   `C++ exception with description "[json.exception.parse_error.101] …" thrown in the test body`
   and one `[ FAILED ]`. Without a `catch` in the new driver, one bad fixture kills the run.
2. **Fatal-assert scope.** `ASSERT_*` returns from the *enclosing void function*, not the test.
   In `expect_transactions_round_trip()` that means "give up on the RLP round-trip, carry on with
   the block". In `run_blockchain_tests()` it means "abandon every remaining case in this file".
3. **Value pretty-printing.** `EXPECT_EQ` prints both sides; `address`/`bytes32` have
   `operator<<` in `statetest.hpp`, `bytes` falls back to a gtest byte dump.
4. **Duplicate `suite.name` pairs are legal** — CI passes two roots to blockchaintest and the
   legacy/non-legacy trees collide by design.
5. **gtest regroups by suite name across roots.** `test/integration/statetest`'s
   `multiple_args_list` pins this: `tests2/SuiteA/test1.json` is listed under the `SuiteA` suite
   opened by `tests1`, not in registration order.
6. Non-zero exit code on any failure; `[ SKIPPED ]` does not fail the run.
7. **Each failure is printed the moment it fires**, not at the end of the test. This one is
   deliberately *not* preserved: `TestReport` collects and the driver reports, so a run that
   segfaults or is killed loses the failures already collected for the test in flight. The
   window is one JSON file, and pytest buffers the same way (its failures land in the FAILURES
   section after the run), so the trade-off is accepted for now.

   **Liftable once gtest is gone**, and cheaply: failures are already values in `TestReport`, so
   the driver can set a sink that `fail()` invokes as it appends — about five lines, no change to
   the data. It belongs behind `-v` rather than the default, because printing detail mid-run
   interleaves with the progress line, and the buffered default is what pytest does.

### 1.4 CLI flags

Used:

| flag | where |
|---|---|
| `--gtest_filter` | circle.yml ×4, `integration/export` ×1, statetest's own default ×1 |
| `--gtest_list_tests` | `integration/statetest` ×3 |

Filter syntax actually exercised: `:`-separated patterns, leading `-` for the negative set, `*`
wildcards. Concrete strings in the tree:

```
-stCreateTest.CreateOOGafterMaxCodesize:stQuadraticComplexityTest.Call50000_sha256:
 stTimeConsuming.static_Call50000_sha256:stTimeConsuming.CALLBlake2f_MaxRounds:VMTests/vmPerformance.*   (statetest default)
-for_amsterdam/*:for_bpo2toamsterdamattime15k/*                                    (circle.yml:429)
-bcValidBlockTest.SimpleTx3LowS                                                    (circle.yml:471)
-bc4895-withdrawals.shanghaiWithoutWithdrawalsRLP:bcInvalidHeaderTest.*:bcUncleHeaderValidity.gasLimitTooLowExactBound  (circle.yml:480)
-*block.*                                                                          (integration/export)
```

Note the statetest default list names `GeneralStateTests` suites that CI no longer runs — the
`ethereum-tests` job only drives `evmone-blockchaintest`. Dead weight, but harmless.

Not used, and confirmed not needed: `--gtest_output=xml|json` (CircleCI's JUnit comes from
`ctest --output-junit`, circle.yml:243), `--gtest_repeat`, `--gtest_shuffle`, `--gtest_brief`,
`--gtest_fail_fast`, `--gtest_color`, death-test flags, sharding env vars.

`--help` today prints gtest's 60-line help *and then* CLI11's. Half of what it advertises
(disabled tests, death tests, XML output) is irrelevant to these tools.

### 1.5 CI and ctest touchpoints

| file:line | current | after |
|---|---|---|
| `circle.yml:179,186` | `--gtest_filter='<<parameters.filter>>'` | repeated `--ignore=…`; the `filter` job parameter becomes a list |
| `circle.yml:429,471,480` | filter strings | unchanged **iff** glob syntax is preserved |
| `test/integration/statetest/CMakeLists.txt:24,39,51` | `--gtest_list_tests` | `--collect-only` + 3 expected-output regexes rewritten |
| `test/integration/export/CMakeLists.txt:34` | `--gtest_filter=-*block.*` | `--ignore state_transition/block` |
| `test/integration/blockchaintest/CMakeLists.txt:17` | `PASS_REGULAR_EXPRESSION ".*2 tests from"` | new summary wording |
| `test/integration/export/CMakeLists.txt:22` | `evmone-unittests --gtest_filter=state_transition.*` | unchanged (unittests keep gtest) |
| `test/{statetest,blockchaintest}/CMakeLists.txt:6` | `GTest::gtest` in `target_link_libraries` | dropped |

The message-matching integration tests (`tx_invalid_*`, `multi_test`, `filter`) keep working as
long as failure output still prints the streamed message and the full test name.

## 2. Proposed replacement

One new gtest-free header+source in `evmone::testutils`, ~150 lines total.

### 2.1 Reporting core — LANDED, macro-free

```cpp
// test/utils/test_report.hpp
namespace evmone::test
{
/// The failures of one test case.
///
/// A runner takes it by reference and records into it; the driver reads failures() when the case
/// returns. Nothing is global: two cases can run at once, each with its own report.
class TestReport
{
public:
    /// Extends the place failures are reported at with @p position, until the scope ends.
    [[nodiscard]] Scope at(const auto&... position);

    /// Records that @p what did not hold.
    void fail(std::string_view what, std::string detail = {},
        const std::source_location& location = std::source_location::current());

    /// Records a failure showing both values if they differ. Returns whether they are equal.
    bool check_eq(std::string_view what, const auto& actual, const auto& expected,
        const std::source_location& location = std::source_location::current());

    [[nodiscard]] std::span<const Failure> failures() const noexcept;
};
}
```

Three functions, no macros. What made the macros unnecessary:

- **`std::source_location` as a defaulted parameter** records the caller — that is what it is for,
  and `__FILE__`/`__LINE__` was the only reason a macro was needed for it.
- **An explicit failing branch is already lazy.** The streamed gtest messages had to be lazy
  because `get<std::error_code>(res)` throws when the check passes and one site formats the whole
  result state. Written as `if (!ok) { report.fail(...); return; }` the detail is only reachable on
  failure, with no lambda or macro to arrange it.
- **The stringified expression is the wrong output here.** These are fixture runners, not unit
  tests: `"state root"` beats `canonical_post_hash == expected_post_hash`. Naming the check is the
  one thing a macro cannot do and it is what the report should say.

Dropping the macros is what lets the report be a parameter instead of a global — the runners take
`TestReport&`, so there is no mutable global state and nothing blocks running cases concurrently.

Fatal checks are a plain `return` written by the author, which preserves gtest's
return-from-enclosing-void-function semantics (D2) while making the early exit visible. There is
no `fatal` concept in the API.

`at()` holds one string and appends with `/`, restoring the previous length when the scope ends,
so nested calls compose (`invalid_nonce/Shanghai/0`) without a `vector<string>` or call sites
repeating each other. It is deliberately not a general context stack: when the driver owns test
identity it either absorbs `at()` or replaces it.

**Reviewed against boost-ext/ut** (verified against its documented API, and pytest 8.4.2 for the
CLI): ut's `expect(expr, source_location = current())` confirms the defaulted-location approach,
and its whole `eq()`/UDL decomposition machinery exists only because `expect(a == b)` collapses to
`bool` before ut can see the operands — a problem `check_eq(what, actual, expected)` does not
have. `expect(...) << msg` evaluates its argument eagerly, which is exactly what breaks here.
ut's BDD `given(...) = [&]{...}` maps onto `at()`, but its lambda nesting fights the
`for (rev) for (case) for (block)` loops, and for data-driven fixtures the scenario is the JSON
file, already named by its path.

### 2.2 Driver

```cpp
struct TestCase
{
    std::string suite;
    std::string name;
    std::filesystem::path file;
    std::function<void()> run;
};

/// Runs @p cases, prints the report, returns the process exit code.
int run_tests(std::span<const TestCase> cases, const Options&);
```

`register_test_files()` in both mains stops calling `RegisterTest` and pushes `TestCase`s
instead — the directory walk, the sort and the naming stay exactly as they are. The driver loop:

```
for each case:
    report.reset()
    try { case.run() }
    catch (const UnsupportedTestFeature& e) { skipped++; continue; }
    catch (const std::exception& e)         { report.fail() << "exception: " << e.what(); }
    tally, print failures
```

That single `catch` covers 1.3(1), and catching `UnsupportedTestFeature` in the driver also fixes
today's asymmetry where blockchaintest's single-file mode prints to stderr and silently registers
nothing.

### 2.3 Selection — no pattern language at all

pytest's `-k` is not the simple mechanism — its own help calls it "a Python evaluable
expression", so implementing it would be *more* code than gtest's glob matcher, not less. The
simple mechanism is collection-time path exclusion. Verified against pytest 8.4.2, the version in
EEST's own venv (`~/proj/execution-specs/.venv`):

```
--collect-only, --co  Only collect tests, don't execute them
--ignore=path         Ignore path during collection (multi-allowed)
--ignore-glob=path    Ignore path pattern during collection (multi-allowed)
--deselect=nodeid_prefix
                      Deselect item (via node id prefix) during collection (multi-allowed)
-x, --exitfirst       Exit instantly on first error or failed test
--maxfail=num         Exit after first num failures or errors
--tb=style            Traceback print mode (auto/long/short/line/native/no)
-v, --verbose / -q, --quiet
```

So:

- **selection = positional paths.** `pytest tests/prague` — already how both binaries work
  (`paths` is variadic and required).
- **exclusion = `--ignore=PATH`, repeatable** ("multi-allowed"). No wildcards, no grammar; a path
  relative to the root, dropping that file or everything under that directory.

pytest splits exclusion in two — `--ignore` takes a *path*, `--deselect` takes a *node-id
prefix* — because in pytest those are different things (a file versus a test inside it). Here,
with one file = one test (D1), the node id and the path are the same string, so a single flag
covers both and `--ignore` is the honest name. `--ignore-glob` exists upstream if a wildcard is
ever genuinely needed; nothing in the tree needs one today, so it stays unbuilt.

**Every current filter string is a plain path under this model — verified, no wildcards needed:**

| today | `--ignore` |
|---|---|
| `-for_amsterdam/*:for_bpo2toamsterdamattime15k/*` | `--ignore for_amsterdam --ignore for_bpo2toamsterdamattime15k` |
| `-bcInvalidHeaderTest.*` | `--ignore bcInvalidHeaderTest` |
| `-bcValidBlockTest.SimpleTx3LowS` | `--ignore bcValidBlockTest/SimpleTx3LowS.json` |
| `-bc4895-withdrawals.shanghaiWithoutWithdrawalsRLP` | `--ignore bc4895-withdrawals/shanghaiWithoutWithdrawalsRLP.json` |
| `-bcUncleHeaderValidity.gasLimitTooLowExactBound` | `--ignore bcUncleHeaderValidity/gasLimitTooLowExactBound.json` |
| `-*block.*` (integration/export) | `--ignore state_transition/block` |
| statetest default (4 exact + `VMTests/vmPerformance.*`) | 4× `--ignore …/x.json` + `--ignore VMTests/vmPerformance` |

The `-*block.*` row was the only one that looked like a real glob. It is not: exported ids are
`state_transition/block.known_block_hash` etc., and the pattern needs the literal `block.`, so it
matches exactly the five files in `state_transition/block/` — `eip7778_block_gas.…` has
`block_gas.`, not `block.`, and is *not* excluded today. It is a directory.

Implementation is a `std::vector<fs::path>` and a component-wise prefix test — ~15 lines, and it
must compare whole path components so `--ignore bc4895` does not also drop `bc4895-withdrawals`.
No `PatternMatchesString`, no expression parser, and `--filter` disappears as a concept.

`-k` stays exactly as it is today: a **plain substring** match on the test name *inside* a file,
which is a different axis from path exclusion. Upstream `-k` takes a Python expression; ours
takes a substring. That divergence is deliberate and worth a comment at the option, so nobody
later "fixes" it into an expression language. A shared driver gives blockchaintest the flag for
free.

`--gtest_list_tests` → **`--collect-only`**, pytest's name for it, emitting a flat id per line
(what pytest `--collect-only -q` does). Flat also sidesteps gtest's cross-root regrouping in
1.3(5). The three expected-output blocks in `test/integration/statetest/CMakeLists.txt` get
rewritten to match.

Worth deciding separately: the statetest default skip list is dead in CI (the `ethereum-tests`
job only drives blockchaintest) and only helps someone running legacy `GeneralStateTests` by
hand. Five baked-in `--ignore` defaults is a lot of ceremony for that; dropping it is defensible.

### 2.4 Output — pytest-shaped console, no colours

**DECIDED: console output only.** No TAP writer, no JUnit writer in the runner. CI reporting
comes later from wrapping the runner in ctest (section 3). Plain ASCII — no ANSI escapes, so no
`isatty` probing and no `--color` flag.

The format follows pytest, which is the vocabulary EEST contributors already read:

```
======================== test session starts =========================
collected 8172 tests

.........................................................  [  0%]
..............................F..........................  [ 47%]
.........................................................  [100%]

============================= FAILURES ===============================
____ for_prague/prague/eip7702_set_code_tx/set_code_txs.tx_into_self ____
statetest_runner.cpp:109: state_root == expected.state_hash
  actual    0x1f3a4b…
  expected  0x9c04e2…
in Prague/0

====================== short test summary info =======================
FAILED  for_prague/…/set_code_txs.tx_into_self - state root mismatch
SKIPPED for_osaka/…/blob_txs.invalid_blob_tx - invalidly rlp-encoded block
============= 1 failed, 8171 passed, 6 skipped in 31.50s =============
```

One progress char per fixture file: `.` pass, `F` fail, `s` skip. A green run is ~120 lines
instead of 37 551.

**Use a flat wrapped dot stream, not pytest's one-line-per-file layout.** pytest prints a line
per module with a dot per test in it; the direct analogue here is a line per directory, and that
degenerates: **6700 of the 7067 fixture directories hold exactly one file**, so it would be 7067
lines with a single dot on almost every one. (Grouping at path depth 3 gives 502 groups if a
middle ground is ever wanted, but the depth is arbitrary.)

Pytest flags worth adopting along with the format, all cheap over the same reporter:

| flag | behaviour |
|---|---|
| `-v` / `-q` | one `PASSED`/`FAILED` line per test / less than the default |
| `-x`, `--maxfail=N` | stop after the first (or N-th) failure — the fork-bump workflow |
| `--tb=long\|short\|line\|no` | how much of the failure block to print |

`-k` already exists with pytest's spelling and meaning; see 2.3.

## 3. Follow-on, once the runners are gtest-free

`statetest_runner.cpp` and `blockchaintest_runner.cpp` can move into `evmone::testutils`, which
is built under `EVMONE_TOOLS` alone and therefore cannot link gtest today — that is the only
reason `run_state_test()` is declared in `test/utils/statetest.hpp` but defined in the statetest
binary. With that gone, `tools/evmone` can grow `statetest` and `blockchaintest` subcommands
alongside `run` and `t8n`, and the two binaries disappear (the second half of roadmap item 7).
The coverage jobs in circle.yml name the binaries explicitly and would need updating at that
point, not before.

**Wrapping the runner in ctest is what produces the CI report.** Registering fixture groups as
ctest tests gives `ctest --output-junit` for free — the same mechanism circle.yml:243 already
uses — so the runner needs no JUnit writer. It also gives `ctest -j` for free, which subsumes
threading the driver (worth ~80 s per job) and avoids needing one `evmc::VM` per thread inside
the tool. What the runner must provide for that: a stable enumeration to partition on
(`--collect-only`), positional paths that select a partition, a correct exit code per invocation, and
failure output that still reads well once ctest captures it with `--output-on-failure`.

## 4. Decisions to make first

- **D1 — test identity. DECIDED: keep "one file = one test".** Every CI filter string, the
  default skip list and the integration expectations port verbatim. Promoting the individual case
  to a test would be nicer, but enumerating them means loading every fixture up front, which is
  far too slow. That is a separate feature, not part of this port.
- **D2 — fatal-assert semantics.** `return` from the enclosing void function (1:1 with gtest,
  keeps `expect_transactions_round_trip()`'s local give-up) or throw-and-catch-per-case.
  *Recommend: `return`, so the port changes no behaviour.*
- **D3 — output. DECIDED: pytest-shaped console only, no colours.** No TAP, no JUnit writer;
  CI reporting comes from wrapping the runner in ctest later.
- **D4 — selection. RESOLVED to `--ignore=PATH`,** see 2.3. No glob matcher and no expression
  grammar: every filter in the tree is a plain path. Positive selection is already the positional
  path arguments. Remaining sub-question: keep or drop the dead statetest default skip list.

## 5. Landing order

Seven commits, each independently reviewable and revertible, each leaving CI green. The two that
touch CI config (5) and CI-visible output (6) are separated from each other and from all the
C++ work.

**1 — `TestReport`, runners ported, gtest bridged. DONE** (macro-free, see 2.1).
Add `test/utils/test_report.{hpp,cpp}` and convert all 40 assertion sites and 4 trace scopes in
`statetest_runner.cpp` / `blockchaintest_runner.cpp`. The gtest `TestBody` in each main becomes
the bridge:

```cpp
TestReport report;
run_state_test(test, report, m_vm, m_trace);
if (report.failed())
    ADD_FAILURE() << report.str();
```

The two runners are gtest-free at the end of this commit; the mains still are not. Same driver,
same flags, same test names, same streamed failure messages — **CI and every ctest expectation
stay green untouched**. This is the largest part of the diff and it gets reviewed on its own.

**2 — move both runners into `evmone::testutils`.**
Pure file move plus CMake. Only possible once they no longer need gtest (testutils is built under
`EVMONE_TOOLS`, which does not link it). Resolves `run_state_test()`'s declaration-without-
definition in `test/utils/statetest.hpp`. No behaviour change.

**3 — collection into testutils.**
`collect_tests(paths) -> std::vector<TestCase>`: the recursive walk, the `index.json` skip, the
sort, the id computation and the directory-vs-single-file dispatch, as **one** implementation
instead of the two near-identical `register_test_files()` copies. Both mains still loop the
vector into `testing::RegisterTest` and still call `RUN_ALL_TESTS()`. The `TestCase::run` thunk
absorbs what actually differs between the tools (statetest's `-k` loop, blockchaintest's
`UnsupportedTestFeature` catch). CI untouched.

This is the commit that removes the duplication, and it lands *before* any gtest is deleted —
which is why the removal is not paid for twice.

**4 — `--ignore` and `--collect-only`, alongside the gtest flags.**
Implemented over the vector from 3; `--gtest_filter` and `--gtest_list_tests` keep working. New
flags are unused by anything yet, so CI is untouched. Includes the one unit test worth writing:
the component-boundary case, `--ignore bc4895` must not drop `bc4895-withdrawals`.

**5 — migrate CI and the integration tests to the new flags.**
No C++ change at all: `circle.yml:179,186` and its `filter` job parameter,
`integration/export/CMakeLists.txt:34`, and `integration/statetest/CMakeLists.txt:24,39,51`
together with their three expected-output blocks (`--collect-only`'s format is ours). After this
nothing in the repo passes a `--gtest_*` flag. Revertible on its own, independently of any C++.

**6 — swap the driver, delete gtest.**
`run_tests()` + the pytest reporter; drop `RegisterTest`, `RUN_ALL_TESTS`, `InitGoogleTest`,
`GTEST_SKIP` and `GTest::gtest` from both binaries. Because 1–4 put the substance in testutils,
what changes in each main here is ~10 lines of glue. The only CI-visible effect left is the
output shape, i.e. `integration/blockchaintest/CMakeLists.txt:17` (`.*2 tests from`).

**7 — merge into `evmone test`.**
Legal only now: `tools/` is built under `EVMONE_TOOLS`, GTest is fetched under `EVMONE_TESTING`
(`test/CMakeLists.txt:13`), and `EVMONE_TOOLS=ON, EVMONE_TESTING=OFF` is a supported
configuration — so folding the runners into the `evmone` CLI any earlier would drag GTest into
the tools build and break that config. By this point both mains are thin CLI11 glue over shared
testutils code, so the merge is a small diff.

Afterwards, as separate work: ctest wrapping (buys JUnit and `-j`) and case-level test identity.

### 5.0 The gtest-free property of testutils is not enforced by CI

Every configure in `circle.yml` and `appveyor.yml` passes `-DEVMONE_TESTING=ON`, so
**`EVMONE_TOOLS=ON, EVMONE_TESTING=OFF` is never built** — the one configuration where GTest is
not fetched, and the whole reason commits 2 and 7 are possible in that order. Nothing would catch
a testing-only dependency creeping into `evmone::testutils`; it was verified by hand for both
commits (`cmake -DEVMONE_TESTING=OFF -DEVMONE_TOOLS=ON`, 0 GTest mentions in the configure log).

Worth adding that configuration to a cheap CI job — `cmake-min` is the natural host — so the
property is enforced rather than assumed by the time commit 7 depends on it.

### 5.1 Why the merge cannot come first

It is the right instinct — the two mains are near-duplicates and nobody wants to port them twice
— but the dependency direction forbids it, per the CMake gating above. The duplication is
removed instead by commit 3: once collection, reporting and the driver live in testutils, each
main is glue that the merge then deletes. Merging first would only move code that commit 6 is
about to delete anyway.

### 5.2 Open question for commit 7: one subcommand or two?

`evmone test <paths>` with per-file format detection, or `evmone statetest` / `evmone
blockchaintest`. Detection is a one-key check — verified on `tests@v20.0.1`, state fixtures carry
`post`/`transaction`/`env` and blockchain fixtures carry `blocks`/`genesisBlockHeader`/
`lastblockhash`, so the presence of `blocks` separates them — and a single command makes a mixed
directory work. Two wrinkles against it: CI already passes the two trees as separate directories,
so detection buys convenience rather than capability; and `-k` and `--trace-summary` are
statetest-only options today, which sit awkwardly on a command that also takes blockchain
fixtures. Unrecognised shapes must be reported as a failure, not silently skipped.

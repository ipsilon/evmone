# AGENTS instructions for the evmone project

## Review guidelines

Divergence from the Ethereum execution specification is a consensus bug and outranks every other finding. Check against the latest merged EIPs (the spec) and ethereum/execution-specs (the reference implementation), not only the released test fixtures, which lag them. Report disagreements between the two instead of picking one.

Also look for:
- Attacker-controlled inputs (bytecode, transactions, precompile inputs) causing a crash, undefined behavior, or work not bounded by the gas charged.
- Off-by-one, sign/width, and overflow errors, especially in shifts, carries/borrows, and limb arithmetic.
- Word-size assumptions (CI also builds 32-bit x86 and riscv32).
- Unpinned or unverified downloads and overly broad permissions in CI workflows.
- Non-obvious invariants that no `assert` pins.
- Hot-path changes (EVM instructions, the interpreter loop, precompile cores) without disassembly or benchmark evidence.
- Changed code that breaks the rules in the sections below, and comments or commit messages that the change makes wrong, including unchanged comments that describe the changed code.

Report all findings, however many, most severe first, including bugs that predate the change in the code you read. Back each with the quoted code or rule, a failing input, a test or measurement you ran, or a quote from the spec or paper; label the rest as suspicions.

The execution-specs tests, which CI runs on every PR, provide the behavioral coverage: suggest a unit test only for what they cannot pin, and report behavior they miss as a gap in them.

evmone deliberately implements the simplest code for the current specification: hardcoded current constants, missing extension points for future EIPs, and missing fast paths for degenerate inputs are not findings. Suggest maintainability changes only when they reduce bug risk.

## Writing code

- Follow the style that `.clang-format` and `.clang-tidy` define, including the identifier naming.
- Doc comments (`///`) state the contract: what the code computes, preconditions, result ranges, aliasing. Match the style of neighboring declarations, but point out a wrong style instead of copying it.
- How the code works (algorithm, implementation details, compiler behavior) goes in `//` comments next to it, inside the body for a function.
- Implement a published algorithm from its original source (paper, book or standard), not from another implementation. Cite the source in the doc comment, and state in `//` comments in the body where the code departs from it. Other implementations may be cited for comparison.
- Do not write comments that state the obvious; when in doubt, leave the comment out. Do not justify an attribute in a comment; a `TODO(<compiler>):` note to recheck it with newer compiler releases is fine.
- Cite an EIP at the end of a comment, as in `// The base fee is burned (EIP-1559).`, not as a prefix.
- Add a new `#include` only when a build fails without it; CI also builds with libc++ and MSVC.

## Commits and pull requests

- Commit messages explain the problem, the change and its effect; the diff shows the details. State performance effects as rough magnitudes ("up to 5%"). Mention testing only when it went beyond what CI runs.
- PRs are squash-merged, so a single-commit PR's title and description match its commit; update them whenever the commit changes. Post anything else, such as benchmark results, as PR comments.
- Keep renames, refactors and independent fixes out of the change that needs them: put them in separate commits and propose them as separate PRs to merge first.

## Building and testing

CMake builds live under `build/`. Reuse an existing one with `cmake --build <build-dir>`, or configure one with tests enabled and warnings as errors, as in CI: `cmake -B build/debug -DCMAKE_BUILD_TYPE=Debug -DEVMONE_TESTING=ON -DCMAKE_COMPILE_WARNING_AS_ERROR=TRUE`.

- Unit and integration tests: `ctest --test-dir <build-dir> --output-on-failure` (filter with `-R <regex>`).
- execution-specs tests: `<build-dir>/bin/evmone test <fixtures>/state_tests <fixtures>/blockchain_tests`, with the fixtures release pinned in `circle.yml`.
- Format C++ changes with clang-format; if CI's lint job still reports a difference, apply its diff. Leave running clang-tidy to CI and fix what it reports.
- Fuzzing, in development or review: use `evmone-fuzzer` (`test/fuzzer`; Clang with `-DEVMONE_FUZZING=ON`), which compares Baseline with Advanced on EVM code only, with a corpus directory kept between runs, not a throwaway harness. For code it does not reach, propose an extension of it as a code change.
- Benchmarks: measure instructions, retired uops and cycles with `perf stat` or `--benchmark_perf_counters=INSTRUCTIONS,CYCLES,<uops event>` (needs libpfm4; configure with `-DBENCHMARK_ENABLE_LIBPFM=ON`). The uops event name depends on the CPU (`RETIRED_OPS` on AMD Zen 3), and an unknown name only gives a warning. Instruction and uop counts are stable but blind to instruction-level parallelism. Cycles are the real cost but noisy: pin the run to a quiet core and repeat the A and B runs alternately. CodSpeed simulates the CPU, so its report is a hint, not a verdict.

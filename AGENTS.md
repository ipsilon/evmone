# AGENTS instructions for the evmone project

## Review guidelines

evmone implements the Ethereum execution specification, so any divergence from it is a consensus bug and outranks every other finding. Compare against the latest merged EIPs and ethereum/execution-specs, not only the released test fixtures, which lag them.

Also look for:
- Attacker-controlled inputs (bytecode, transactions, precompile inputs) that cause a crash, undefined behavior, or work not bounded by the gas charged.
- Off-by-one, sign/width, and overflow errors, especially in shifts, carries/borrows, and limb arithmetic.
- Word-size assumptions (CI also builds 32-bit x86 and riscv32).
- Unpinned or unverified downloads and overly broad permissions in CI workflows.
- Non-obvious invariants that no `assert` pins.

Behavioral coverage comes from the execution-specs tests, which CI runs on every PR. Suggest a unit test only for what they cannot pin; behavior they miss is a gap in those tests, not a missing unit test.

evmone deliberately implements the simplest code for the current specification, so hardcoded current constants, missing extension points for future EIPs, and missing fast paths for degenerate inputs are not findings. Suggest maintainability changes only when they reduce bug risk.

## Building and testing

Out-of-source CMake builds live under `build/`. If one exists, build it directly, e.g. `cmake --build build/debug`.

- Unit and integration tests: `ctest --test-dir build/debug --output-on-failure` (filter with `-R <regex>`).
- execution-specs tests: `build/debug/bin/evmone test <fixtures>/state_tests <fixtures>/blockchain_tests`, with the fixtures from the ethereum/execution-specs releases.

# AGENTS instructions for the evmone project

## Review guidelines

When reviewing changes (code, tests, build scripts, documentation, workflows), expand the review beyond correctness and style. Actively look for:

### Vulnerabilities
- Injection risks (SQL/command/template), unsafe string handling, path traversal, deserialization hazards.
- Memory-safety issues (OOB reads/writes, use-after-free patterns, integer overflow/underflow, uninitialized data).
- Crypto misuse (non-constant-time operations on secrets, weak randomness, incorrect nonce handling, missing domain separation, insecure parameters).
- Authentication/authorization gaps, privilege escalation paths, and broken access controls.
- Supply-chain risks (unpinned dependencies, unsafe download/exec patterns, overly broad CI permissions).

### Security regressions
- Changes that weaken previous security properties (e.g., removal of validation, relaxed checks, reduced entropy, widened accepted inputs).
- New feature flags or configuration paths that silently disable protections.
- Debug/diagnostic code that could leak secrets or sensitive data in logs, metrics, crashes, or error messages.

### Safety issues
- Unexpected behavior under malformed, adversarial, or extreme inputs (fuzz-like thinking).
- Denial-of-service vectors (algorithmic complexity, unbounded loops/recursion, large allocations, input-triggered worst cases).
- Concurrency hazards (data races, deadlocks, TOCTOU, improper locking/atomic usage).
- Resource handling (file descriptors, handles, memory, temporary files, cleanup on error paths).

### Correctness bugs
- Off-by-one errors, boundary conditions, and sign/width issues (especially around shifts, carries/borrows, and limb arithmetic).
- Undefined/implementation-defined behavior (C/C++), especially shifts, aliasing, and overflow.
- Error handling and propagation: ensure failures are detected, returned, and tested.
- Cross-platform/endianness/word-size assumptions that may break on other targets.

### What to include in review feedback
- Call out risks explicitly and suggest mitigations or tests.
- Recommend additional unit tests, property tests, fuzzing targets, or negative tests where appropriate.
- Suggest clarity/maintainability improvements when they reduce future bug risk.
- Flag any unclear invariants, missing comments, or undocumented assumptions.

Other suggestions and improvements are welcome as long as they are constructive, actionable, and help improve quality, security, and maintainability.

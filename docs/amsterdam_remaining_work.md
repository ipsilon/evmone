# Amsterdam Remaining Work

Status against `bal@v5.7.0` test fixtures.

- State tests: 10442 passed / 0 failed ✓ **full pass**
- Blockchain tests: partial EIP-7928 support (see below)

Direct EIP coverage is fully green:

| EIP | State | Blockchain |
|---|---|---|
| EIP-8037 (state creation gas) | 115/115 | 186/186 |
| EIP-7708 (eth transfer logs) | 38/38 | 42/42 |
| EIP-7778 (block gas accounting) | — | 6/6 |

## EIP-7928 Block-Level Access Lists (WIP)

Initial implementation landed: data model (`test/state/bal.hpp/.cpp`), RLP +
keccak256 encoding, gas-limit check, builder + StateView decorator, and
blockchain-runner wiring that validates `blockAccessListHash` against the
execution-derived BAL for Amsterdam+.

Against the `eip7928_block_level_access_lists/` suite (134 tests):
**111 passing / 23 failing**.

To keep OOG'd SLOAD reads out of the BAL, storage and account access was made
lazy at the State level (see `Account::loaded` / `exists_in_state`, the new
`JournalStorageAccess`, and `State::find()` lazy-load path). That refactor
introduced regressions on ~71 ported tests where the execution-time BAL now
contains extra account entries not present in the fixture's reference BAL.

Remaining work to finish EIP-7928:
- Diagnose the extra-account sources on ported tests (likely one remaining
  code path in evmone hits `m_initial.get_account` for addresses that EELS /
  geth consider untouched).
- Unblock the 23 EIP-7928 failures — mostly EIP-7702 delegation interactions
  and a handful of OOG-after-access variants.

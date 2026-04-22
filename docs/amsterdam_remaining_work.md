# Amsterdam Remaining Work

Status against `bal@v5.7.0` test fixtures.

- State tests: 10442 passed / 0 failed ✓ **full pass**
- Blockchain tests (for_amsterdam subset): 2759 / 2777 passing

Direct EIP coverage is fully green:

| EIP | State | Blockchain |
|---|---|---|
| EIP-8037 (state creation gas) | 115/115 | 186/186 |
| EIP-7708 (eth transfer logs) | 38/38 | 42/42 |
| EIP-7778 (block gas accounting) | — | 6/6 |

## EIP-7928 Block-Level Access Lists

Implemented: data model (`test/state/bal.hpp/.cpp`), RLP + keccak256 encoding,
gas-limit check, builder + StateView decorator, and blockchain-runner wiring
that validates `blockAccessListHash` against the execution-derived BAL for
Amsterdam+.

Against `blockchain_tests/for_amsterdam/` (2777 tests):
- Overall: **2759 passing / 16 failing**.
- Direct EIP-7928 suite (134 tests): **124 passing / 10 failing** (all EIP-7702
  delegation-related).

Design: cold account/storage accesses are deferred to the point where the EVM
actually needs the value. `Host::access_storage` / `Host::access_account` mark
slots/accounts warm via lightweight placeholders without hitting the StateView;
the underlying fetch is triggered by `State::find()` / `State::get_storage()`
on demand. `JournalStorageAccess` separates access-status reverts from value
reverts so journal rollback cannot overwrite values loaded after the journal
was written. `call_impl` in `lib/evmone/instructions_calls.cpp` was reordered
to charge memory + value-transfer gas before the delegation lookup, matching
geth's ordering and keeping OOG'd targets out of the BAL.

Remaining EIP-7928 failures are all EIP-7702-specific BAL tests
(`block_access_lists_eip7702/*`) plus `bal_create_and_oog`; those need deeper
investigation of delegation-writer interactions with the builder.

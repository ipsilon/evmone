# Amsterdam Remaining Work

Status against `bal@v5.7.0` test fixtures (`~/proj/fixtures/bal-570/`).

## Summary

| Suite | Passing | Failing | Notes |
|---|---|---|---|
| `state_tests/` | 10442 / 10442 | 0 | ✓ full pass |
| `blockchain_tests/for_amsterdam/` | 2775 / 2777 | 0 | ✓ (2 fixture-level skips) |
| `blockchain_tests_engine/for_amsterdam/` | 0 / 2778 | 2778 | engine-payload format unsupported |
| `blockchain_tests_sync/for_amsterdam/` | 0 / 4 | 4 | engine-payload format unsupported |

Direct EIP coverage is fully green against state+blockchain fixtures:

| EIP | State | Blockchain |
|---|---|---|
| EIP-8037 (state creation gas) | 115/115 | 186/186 |
| EIP-7708 (eth transfer logs) | 38/38 | 42/42 |
| EIP-7778 (block gas accounting) | — | 6/6 |
| EIP-7928 (block-level access lists) | — | 134/134 |

## Remaining failures

### 1. `blockchain_tests_engine/*` and `blockchain_tests_sync/*` — unsupported format

Fixtures use the engine-payload format (`engineNewPayloads` at the top level
instead of `blocks`). `test/utils/blockchaintest_loader.cpp` doesn't parse this
variant, so every test throws on load with
`key 'blocks' not found`.

Not an evmone EVM issue — a test-harness feature gap. These suites overlap
heavily with the regular `blockchain_tests/` suite (same coverage via the
conventional block format), so they're low priority.

**Next step**: extend the loader to parse `engineNewPayloads` → synthesize
equivalent `blocks` entries. Straightforward but not required for correctness.

## Historical notes

### `create_address_warm_after_fail` (fixed)

Pre-existing CREATE gas-accounting bug surfaced during EIP-7928 work. On
sub-frame init-code rejection (EIP-3541 `0xEF`, code-too-big, REVERT, INVALID),
the caller observed a cold-access charge (2500 gas delta) for the CREATE
recipient when making a follow-on access, because `JournalCreate` rollback
erased the recipient from `m_modified` along with its EIP-2929 warm flag.

Per EIP-2929, "access to new created address is never reverted". Fixed by
resetting the entry to a warm-only placeholder on rollback instead of erasing,
preserving the warm flag while still treating the address as non-existent.

### EIP-7928 Block-Level Access Lists (shipped)

Data model, RLP + keccak256, gas-limit check, builder + StateView decorator,
and blockchain-runner wiring validating `blockAccessListHash` against the
execution-derived BAL for Amsterdam+.

Key design choices:

- **Lazy account/storage fetch**: `Host::access_storage` / `access_account`
  mark slots/accounts warm via lightweight placeholders without hitting the
  StateView; the underlying fetch is triggered only when the value is
  actually needed (`State::find` / `State::get_storage`). Keeps OOG-at-gas-check
  storage/account reads out of the BAL.
- **`JournalStorageAccess`** separates access-status reverts from value
  reverts so journal rollback can't overwrite values loaded after the
  journal was written.
- **`call_impl` ordering fix** (EIP-8037 line 126): regular gas — memory
  expansion, value transfer — is charged before the delegation lookup and
  before the state-gas charge, matching geth's `gasCallEIP8037`. Prevents a
  spilled state charge from committing `state_gas_used` when a follow-on
  regular check OOGs.
- **`Host::call` 0x03 quirk**: the RIPEMD re-touch on sub-frame revert goes
  through `get_or_insert_for_access` so the probe (evmone-internal) doesn't
  surface the precompile address to the BAL tracker.
- **System contracts**: `system_call_block_start` / `_block_end` explicitly
  call `state_view.get_account(addr)` before executing so the address is
  observed by the tracker even when the (possibly-stubbed) contract body
  makes no state access.

### Surfaced spec/test issues

- Filed https://github.com/ethereum/execution-specs/issues/2747: missing
  EIP-8037 test covering the CREATE state-charge-then-regular-OOG boundary.
- Filed and closed https://github.com/ethereum/execution-specs/issues/2748:
  apparent spec ambiguity on per-opcode commit semantics — closed after
  finding EIP-8037 line 126 already mandates the ordering.

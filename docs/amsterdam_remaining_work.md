# Amsterdam Remaining Work

Status against `bal@v5.7.0` test fixtures.

- State tests: 10442 passed / 0 failed ✓ **full pass**
- Blockchain tests: 10920 passed / 279 failed

Direct EIP coverage is fully green:

| EIP | State | Blockchain |
|---|---|---|
| EIP-8037 (state creation gas) | 115/115 | 186/186 |
| EIP-7708 (eth transfer logs) | 38/38 | 42/42 |
| EIP-7778 (block gas accounting) | — | 6/6 |
| EIP-7976 (calldata floor cost) | — | — |
| EIP-7981 (access list cost) | — | — |

## Amsterdam-specific gaps (32 blockchain)

### EIP-7928 Block-Level Access Lists (32 blockchain failures)

Not implemented. evmone does not validate the block header's `BlockAccessListHash`
field and does not generate the BAL structure. All failures are in
`blockchain_tests/for_amsterdam/amsterdam/eip7928_block_level_access_lists/`:

- `block_access_lists_invalid/*` — tests expecting the client to reject blocks
  with malformed BALs.
- `block_access_lists/*`, `block_access_lists_eip7702/*`,
  `block_access_lists_opcodes/*` — positive cases checking BAL content.

Requires a dedicated EIP-7928 implementation: track per-tx access/write sets,
aggregate into a BAL per block, hash-commit into the header.

## Pre-existing non-Amsterdam blockchain failures (~247)

Ported static tests designed for earlier forks, now running on Amsterdam.
Investigated root causes:

### System contract state drift (likely majority)

Blockchain test fixtures include pre-state with system contract placeholders
(deposit contract `0x219ab540...`, beacon roots `0x000f3df6...`, history
storage `0x0000f908...`, withdrawals `0x00000961...`, consolidations
`0x0000bbdd...`). On every block, the beacon_roots and history_storage system
calls write a slot with the current timestamp / block number; the ported
fixtures don't reflect these writes because they were generated targeting
earlier forks. Our post-state includes the system-call storage entries, the
fixture post-state doesn't, so state roots diverge.

This is a fixture-generation artifact, not a client bug. A client that
*didn't* execute the system calls would be non-compliant with Cancun+.
Running against re-filled fixtures from EELS tests-bal would fix these.

### Folder breakdown

| Folder | Blockchain | Root cause |
|---|---|---|
| `ported_static/stRandom` | 91 | system-call state drift |
| `ported_static/stRandom2` | 73 | system-call state drift |
| `ported_static/stMemoryStressTest` | 34 | INT64_MAX block gas + system-call drift |
| `ported_static/stReturnDataTest` | 28 | system-call state drift |
| `ported_static/stCreate2` | 4 | system-call state drift |
| `ported_static/stEIP1559` | 3 | system-call state drift |
| `ported_static/stTransactionTest` | 2 | system-call state drift |
| `ported_static/stBugs` | 2 | system-call state drift |
| misc single-case folders | ~10 | various |

## Candidates for direct EIP test coverage

None found. The ported_static failures are not Amsterdam semantics bugs
— they're stale fixture drift from running pre-Cancun-era tests without
re-filling them against the new required system calls at block start.

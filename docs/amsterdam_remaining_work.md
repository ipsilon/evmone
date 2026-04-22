# Amsterdam Remaining Work

Status against `bal@v5.7.0` test fixtures.

- State tests: 10420 passed / 22 failed
- Blockchain tests: 10919 passed / 280 failed

Direct EIP coverage is fully green:

| EIP | State | Blockchain |
|---|---|---|
| EIP-8037 (state creation gas) | 115/115 | 186/186 |
| EIP-7708 (eth transfer logs) | 38/38 | 42/42 |
| EIP-7778 (block gas accounting) | — | 6/6 |
| EIP-7976 (calldata floor cost) | — | — |
| EIP-7981 (access list cost) | — | — |

## Amsterdam-specific gaps

### EIP-7928 Block-Level Access Lists (33 blockchain failures)

Not implemented. evmone does not validate the block header's `BlockAccessListHash`
field and does not generate the BAL structure. All failures are in
`blockchain_tests/for_amsterdam/amsterdam/eip7928_block_level_access_lists/`:

- `block_access_lists_invalid/*` — tests expecting the client to reject blocks
  with malformed BALs.
- `block_access_lists/*`, `block_access_lists_eip7702/*`,
  `block_access_lists_opcodes/*` — positive cases checking BAL content.

Requires a dedicated EIP-7928 implementation: track per-tx access/write sets,
aggregate into a BAL per block, hash-commit into the header.

## Pre-existing non-Amsterdam failures

Tests designed for earlier forks run on Amsterdam and break because of
SELFDESTRUCT / state-gas / balance-handling changes. Not regressions from
the Amsterdam work.

| Folder | Blockchain | State |
|---|---|---|
| `ported_static/stRandom` | ~90 | ~10 |
| `ported_static/stRandom2` | ~70 | ~4 |
| `ported_static/stMemoryStressTest` | ~34 | ~4 |
| `ported_static/stReturnDataTest` | ~28 | — |
| `cancun/eip6780_selfdestruct` | ~14 | ~10 |
| `ported_static/stCreate2` | ~6 | ~2 |
| `tangerine_whistle/eip150_operation_gas_costs` | ~5 | — |
| `constantinople/eip1052_extcodehash` | ~4 | ~4 |
| other (stEIP1559, eip7702, frontier, etc.) | ~30 | ~10 |

Triage priority should go by whether a failure blocks Amsterdam semantics
or is pre-existing ported test drift.

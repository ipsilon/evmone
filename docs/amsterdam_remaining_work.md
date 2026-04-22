# Amsterdam Remaining Work

Status against `bal@v5.7.0` test fixtures.

- State tests: 10394 passed / 48 failed
- Blockchain tests: 10863 passed / 336 failed

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

### EIP-7708 Transfer log emission — ALREADY CORRECT

On closer inspection, the Transfer and Burn log emission is already complete
and the log sequences match the fixtures (no `logs_hash` mismatches).

The 4 burn_logs state failures and 7 blockchain failures in
`amsterdam/eip7708_eth_transfer_logs/` are actually state-root mismatches
caused by gas accounting differences on the underlying CREATE+SELFDESTRUCT
scenarios: balances (especially coinbase) diverge because state-gas refund
for same-tx selfdestruct in nested CREATE/CALL patterns is off. Reclassified
as EIP-8037 SELFDESTRUCT edge cases (next section).

### EIP-8037 SELFDESTRUCT state-gas refund (11 blockchain failures)

Failures cluster around CREATE + SELFDESTRUCT in the same transaction, and
value CALLs into a self-destructed account. Specific tests:

- `state_gas_call.call_{value,zero_value}_to_self_destructed_{burns_value,same_tx_account,header_gas_used}`
- `state_gas_create.selfdestruct_in_create_tx_initcode`
- `state_gas_selfdestruct.create_selfdestruct_{code_deposit_refund_header_check,no_double_refund_with_sstore_restoration,refunds_account_and_storage,refunds_code_deposit_state_gas}`
- `state_gas_selfdestruct.selfdestruct_to_self_in_create_tx`
- `state_gas_selfdestruct.selfdestruct_via_delegatecall_chain`

Root cause: balances differ → gas accounting for SELFDESTRUCT state-gas refund
in combined CREATE+destruct scenarios is incomplete. The existing
`feb5f3bc` (EIP-8037: Refund state gas for same-tx SELFDESTRUCT) covers the
basic same-tx case; these edge cases need refund-ordering fixes around
code deposit and SSTORE restoration interaction.

## Pre-existing non-Amsterdam failures (285 blockchain, 44 state)

Tests designed for earlier forks run on Amsterdam and break because of
SELFDESTRUCT / state-gas / balance-handling changes. Not regressions from
the Amsterdam work.

| Folder | Blockchain | State |
|---|---|---|
| `ported_static/stRandom` | 91 | 10 |
| `ported_static/stRandom2` | 73 | 4 |
| `ported_static/stMemoryStressTest` | 34 | 4 |
| `ported_static/stReturnDataTest` | 28 | 0 |
| `cancun/eip6780_selfdestruct` | 14 | 10 |
| `ported_static/stCreate2` | 6 | 2 |
| `tangerine_whistle/eip150_operation_gas_costs` | 5 | 0 |
| `constantinople/eip1052_extcodehash` | 4 | 4 |
| other (stEIP1559, eip7702, frontier, etc.) | ~30 | ~14 |

Triage priority should go by whether a failure blocks Amsterdam semantics
or is pre-existing ported test drift.

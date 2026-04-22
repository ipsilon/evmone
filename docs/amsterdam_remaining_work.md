# Amsterdam Remaining Work

Status against `bal@v5.7.0` test fixtures.

- State tests: 10442 passed / 0 failed ✓ **full pass**
- Blockchain tests: 11166 passed / 33 failed

Direct EIP coverage is fully green:

| EIP | State | Blockchain |
|---|---|---|
| EIP-8037 (state creation gas) | 115/115 | 186/186 |
| EIP-7708 (eth transfer logs) | 38/38 | 42/42 |
| EIP-7778 (block gas accounting) | — | 6/6 |

## Remaining gaps

### EIP-7928 Block-Level Access Lists (32 blockchain failures)

Not implemented. evmone does not validate the block header's
`BlockAccessListHash` field or generate the BAL structure. All failures
under `blockchain_tests/for_amsterdam/amsterdam/eip7928_block_level_access_lists/`.

Requires a dedicated EIP-7928 implementation: per-tx access/write set
tracking, per-block aggregation, hash commit in the header.

### Header validation: gas_limit minimum (1 blockchain failure)

`frontier/validation/header.gas_limit_below_minimum` — tests that a
block with `gas_limit < 5000` is rejected. evmone's header validator
doesn't enforce this. Unrelated to Amsterdam; a pre-existing gap.

# EIP-7708 Update Proposal: Replace Burn Events with Balance-Preserving SELFDESTRUCT Finalization

## Motivation

EIP-7708 introduces two kinds of ETH movement logs:

- **Transfer logs** — for nonzero-value transactions, `CALL`, and `SELFDESTRUCT` to a different address.
- **Burn logs** — for ETH that leaves the world state. Defined for two cases:
  1. A nonzero-balance `SELFDESTRUCT` to self by a contract created in the same transaction, at opcode execution time.
  2. Any nonzero-balance account marked for deletion by `SELFDESTRUCT` the contract created in the same transaction, at the time of removal during transaction finalization.

The **burn log paths are exotic**: post-EIP-6780, the only accounts that can be marked for deletion are contracts created in the same transaction, and the finalization case further requires that ETH be sent to such an account after it has already self-destructed. The examination of a small sample of Ethereum mainnet blocks (30 consecutive `SELFDESTRUCT` opcode executions) showed **zero occurrences** of either condition: all observed `SELFDESTRUCT` calls had zero balance and distinct beneficiaries.

Burn logs exist solely because ETH can be silently destroyed by the current `SELFDESTRUCT` semantics. If the underlying cause — ETH being destroyed — is eliminated, burn logs become unnecessary.

This proposal eliminates ETH burning by modifying how accounts marked for deletion are processed at transaction finalization. ETH is preserved by construction; only transfer logs are required.

## Specification

### Finalization of deleted accounts (replaces EIP-6780's account removal for marked-for-deletion accounts)

At the point of transaction finalization, for each account that was marked for deletion during the transaction, the following mutations are applied to the State instead of removing the account:

1. The account's **code** is cleared: code hash is reset to `keccak256("")` and the stored code becomes empty.
2. The account's **nonce** is reset to `0`.
3. The account's **storage** is cleared (all slots reset to zero).
4. The account's **balance** is preserved.

After these mutations, the account is subject to the standard empty-account rule (EIP-161): if the resulting balance is zero and nonce is zero and code is empty, the account is removed. Otherwise, the account remains in the state trie holding only its balance.

### SELFDESTRUCT to self

When a contract created in the same transaction executes `SELFDESTRUCT` with itself as the beneficiary, the opcode marks the account for deletion as before, but its balance is preserved.
This is consistent with the behavior of `SELFDESTRUCT` of the pre-existed accounts.

### ETH transfer log

Unchanged.

### ETH burn log

Removed. No burn log is emitted under any condition.

## Rationale

### Why not emit burn logs?

The only way ETH can be removed from the world state under the current EVM is through `SELFDESTRUCT` of a contract created in the same transaction (post-EIP-6780). This proposal redefines the finalization of such accounts so that their balance survives, at which point no ETH-destruction event exists to log.

### Why this is safe for state consistency

An account marked for deletion has its effects confined to the current transaction. Clearing code, nonce, and storage at finalization is equivalent to the account never having been created for the purposes of future transactions, except for the retained balance. The transaction has already paid the creation cost (base, initcode, and code deposit), so the retained balance is not subsidized — it comes from ETH the caller explicitly transferred to the account.

### Future CREATE2 at the same address

A future `CREATE2` at the preserved account's address would not collide under EIP-7610 rules:

- Nonce is 0.
- Code is empty.
- Initial storage is empty.

A nonzero balance alone does not constitute a collision. The address remains available for future deployment.

### SELFDESTRUCT semantics

This proposal further nerfs `SELFDESTRUCT` beyond EIP-6780. Under EIP-6780, `SELFDESTRUCT` on a contract created in the same transaction still removes the account from the state; this proposal reduces that to a balance-preserving clear of the code, nonce, and storage. `SELFDESTRUCT` becomes, in effect, an opcode that transfers balance and resets contract state, without any path that destroys ETH.

Whether this change should be packaged as a separate EIP (a `SELFDESTRUCT` successor) or integrated into EIP-7708 is left as an open question.

## Backwards Compatibility

### Observable differences from EIP-6780

Under EIP-6780, a contract created in the same transaction that self-destructs is fully removed from the state at finalization. Under this proposal:

- If the self-destructed contract received additional ETH after `SELFDESTRUCT` and before transaction end, that balance remains as a freestanding balance-only account (previously: the balance was burned).
- If the self-destructed contract self-destructs to itself with nonzero balance, that balance remains (previously: the balance was burned).
- In all other cases, the net state diff is identical to EIP-6780: the account is removed (because the balance is zero and the EIP-161 empty-account rule applies).

### Gas accounting

There are no gas refunds for `SELFDESTRUCT` since London, so no changes here.

## Security Considerations

### Gas griefing

A contract that sends ETH to a self-destructed contract no longer causes that ETH to be burned. Instead, the ETH becomes owned by a code-less address. In rare attack scenarios where a user's contract relied on ETH being burned (for deflationary accounting or similar), this behavior change would be observable. Such designs would be unusual and rely on `SELFDESTRUCT` semantics that were already scheduled for further nerfing.

### Dust accounts

In the edge case where a self-destructed contract accumulated dust from a later inbound transfer, a balance-only account is created. This is identical in form to any externally owned account with a balance and is not a new class of state entry.


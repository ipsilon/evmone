# Mainnet shift amounts

Data sources for the `synth/mainnet_shifts` benchmarks.

## Trace collection

The shift amounts were traced while executing 200 mainnet blocks (24491136..24640322)
natively with the zilkworm `state_transition` tool. `shl`, `shr` and `sar` in
`lib/evmone/instructions.hpp` were instrumented to append one 65-byte record per executed
instruction, `[opcode:1][shift:32 LE][value:32 LE]`, to a file named by an environment variable:

```cpp
inline void trace_shift(uint8_t op, const uint256& shift, const uint256& value) noexcept
{
    static std::FILE* const f = [] {
        const char* const p = std::getenv("SHIFT_TRACE");
        return p != nullptr ? std::fopen(p, "wb") : nullptr;
    }();
    if (f == nullptr)
        return;
    unsigned char rec[65];
    rec[0] = op;
    std::memcpy(rec + 1, &shift, 32);
    std::memcpy(rec + 33, &value, 32);
    std::fwrite(rec, 1, sizeof(rec), f);
}
```

Each block was executed in a separate process writing `<block>.bin`.
The trace has 4,305,045 shifts: 2,290,352 SHL, 1,719,969 SHR, 294,724 SAR.
Shift amounts greater than 255 (6 SHL, 135 SHR) are skipped by the scripts.

## Scripts

- `select_shift_windows.py <trace_dir> <out.hpp>` generates `mainnet_shift_amounts.hpp`:
  for each instruction, the window of consecutive amounts filling the 64 KiB benchmark code
  with the amount distribution closest to the overall one.
- `gen_synth_shifts.py <trace_dir> <out.hpp>` generates `synth_shift_amounts.hpp`:
  sequences of the same length sampled from models fitted on the whole trace
  (independent draws, first- and second-order Markov chains).

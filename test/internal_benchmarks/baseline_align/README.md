# Baseline code alignment benchmark (throwaway)

Times libevmone built with code alignment flags for `lib/evmone/baseline_execution.cpp` only:

- GCC: default, `-falign-loops=64`, `-falign-labels=32` (#1815). `-falign-loops` does not move GCC's
  computed-goto handlers, so its dispatch is the default one and the variant measures the noise.
- clang: default, `-falign-loops=32`, `-falign-loops=64`. clang's handlers are loop headers (each one's
  indirect jump can return to it), so these align 149 of the 155 handler entries.

Each alignment is built with the SMOD handler padded by 0, 16, 32 and 48 bytes, which moves the handlers
after it: the layout samples. `evmtime` loads all variants into one process and times them in rotating
order; `analyze.py` takes the minimum over the rounds, the mean over the paddings, and the spread over them.

```sh
CC=clang-21 CXX=clang++-21 test/internal_benchmarks/baseline_align/ci.sh local results 2 3
test/internal_benchmarks/baseline_align/analyze.py results/*.json
```

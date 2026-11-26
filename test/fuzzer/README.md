# evmone-fuzzer

> [LibFuzzer] powered testing tool for evmone.

The inputs are [execution-specs] state tests. The mutator changes the code of a pre-state
account, re-executes the test and rewrites its expectations, so every input the fuzzer writes
is a valid state test. Each run checks the expectations.

## Usage

Build with Clang and `-DEVMONE_FUZZING=ON`. Merge the state tests of an execution-specs release
into a corpus, then fuzz it:

    bin/evmone-fuzzer -merge=1 -max_len=67108864 corpus/ fixtures/state_tests/
    bin/evmone-fuzzer -max_len=67108864 corpus/

## License

The evmone-fuzzer source code is licensed under the [Apache License, Version 2.0].


[Apache License, Version 2.0]: https://www.apache.org/licenses/LICENSE-2.0.txt
[execution-specs]: https://github.com/ethereum/execution-specs
[LibFuzzer]: https://llvm.org/docs/LibFuzzer.html

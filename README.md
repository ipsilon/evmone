# evmone

[![ethereum badge]][ethereum]
[![readme style standard badge]][standard readme]
[![codecov badge]][codecov]
[![codspeed badge]][codspeed]
[![circleci badge]][circleci]
[![appveyor badge]][appveyor]
[![license badge]][Apache License, Version 2.0]

> Fast Ethereum Virtual Machine implementation

_evmone_ is a C++ implementation of the Ethereum Virtual Machine (EVM). 
Created by members of the [Ipsilon] (ex-[Ewasm]) team, the project aims for clean, standalone EVM implementation 
that can be imported as an execution module by Ethereum Client projects. 
The codebase of _evmone_ is optimized to provide fast and efficient execution of EVM smart contracts.

### Characteristic of evmone

1. Exposes the [EVMC] API.
2. Requires C++20 standard.
3. The [intx] library is used to provide 256-bit integer precision.
4. The [ethash] library is used to provide Keccak hash function implementation
   needed for the special `KECCAK256` instruction.
5. Contains two interpreters: 
   - **Baseline** (default)
   - **Advanced** (select with the `advanced` option)

### Baseline Interpreter

1. Provides relatively straight-forward but efficient EVM implementation.
2. Performs only minimalistic `JUMPDEST` analysis.

### Advanced Interpreter

1. The _indirect call threading_ is the dispatch method used -
   a loaded EVM program is a table with pointers to functions implementing virtual instructions.
2. The gas cost and stack requirements of block of instructions is precomputed 
   and applied once per block during execution.
3. Performs extensive and expensive bytecode analysis before execution.


## Usage

### As an EVMC module

evmone implements the [EVMC] API for Ethereum Virtual Machines.
The evmone shared library can be loaded by an EVMC host application.
Prebuilt libraries are available from [Releases].
evmone vendors EVMC (in `evmc/`); to be ABI compatible, the host application must use EVMC headers with the same ABI version (the vendored headers are in `evmc/include`).

### Building from source

To build the evmone EVMC module (shared library), test, and benchmark:

1. Fetch the source code:
   ```
   git clone --recursive https://github.com/ethereum/evmone
   cd evmone
   ```

2. Configure the project build and dependencies:
   ##### Linux / OSX
   ```
   cmake -S . -B build -DEVMONE_TESTING=ON
   ```

   ##### Windows
   ```
   cmake -S . -B build -DEVMONE_TESTING=ON -G "Visual Studio 16 2019" -A x64
   ```
   
3. Build:
   ```
   cmake --build build --parallel
   ```


4. Run the unit tests or benchmarking tool:
   ```
   build/bin/evmone-unittests
   build/bin/evmone-bench test/evm-benchmarks/benchmarks
   ```

### Precompiles

Ethereum Precompiled Contracts (_precompiles_ for short) are implemented in two places:
the core parts are in the `evmone_precompiles` library (`lib/evmone_precompiles`),
and the dispatch is in the evmone state library (`test/state`),
which is built with the evmone tools (`EVMONE_TOOLS`).
Two CMake options switch a precompile to an external library instead:

1. `EVMONE_PRECOMPILES_LIBSECP256K1=ON` uses [libsecp256k1] for `ecrecover`.
   The built-in `ecrecover` has degraded performance, so this can be enabled to speed it up.
   The library is fetched and built with evmone.
2. `EVMONE_PRECOMPILES_GMP=ON` uses [GMP] for `expmod`.
   This requires the GMP library (e.g. libgmp-dev) at build and execution time.

### Docker

> [!NOTE]
> This section is outdated: the Docker image is not updated regularly.

Docker images with evmone are available on Docker Hub:
https://hub.docker.com/r/ethereum/evmone.

Having the evmone shared library inside a docker is not very useful on its own,
but the image can be used as the base of another one or you can run benchmarks 
with it.

```bash
docker run --entrypoint evmone-bench ethereum/evmone /src/test/benchmarks
```

## References

1. [Efficient gas calculation algorithm for EVM](docs/efficient_gas_calculation_algorithm.md)

## Maintainer

Paweł Bylica [@chfast]

## License

[![license badge]][Apache License, Version 2.0]

Licensed under the [Apache License, Version 2.0].


[@chfast]: https://github.com/chfast
[appveyor]: https://ci.appveyor.com/project/chfast/evmone/branch/master
[circleci]: https://circleci.com/gh/ethereum/evmone/tree/master
[codecov]: https://codecov.io/gh/ethereum/evmone/
[codspeed]: https://app.codspeed.io/ipsilon/evmone
[Apache License, Version 2.0]: LICENSE
[ethereum]: https://ethereum.org
[EVMC]: https://github.com/ethereum/evmc
[Ipsilon]: https://github.com/ipsilon
[Ewasm]: https://github.com/ewasm
[GMP]: https://gmplib.org
[libsecp256k1]: https://github.com/bitcoin-core/secp256k1
[intx]: https://github.com/chfast/intx
[ethash]: https://github.com/chfast/ethash
[Releases]: https://github.com/ethereum/evmone/releases
[standard readme]: https://github.com/RichardLitt/standard-readme

[appveyor badge]: https://img.shields.io/appveyor/ci/chfast/evmone/master.svg?logo=appveyor
[circleci badge]: https://img.shields.io/circleci/project/github/ethereum/evmone/master.svg?logo=circleci
[codecov badge]: https://img.shields.io/codecov/c/github/ethereum/evmone.svg?logo=codecov
[codspeed badge]: https://img.shields.io/endpoint?url=https://codspeed.io/badge.json
[ethereum badge]: https://img.shields.io/badge/ethereum-EVM-informational.svg?logo=ethereum
[license badge]: https://img.shields.io/github/license/ethereum/evmone.svg?logo=apache
[readme style standard badge]: https://img.shields.io/badge/readme%20style-standard-brightgreen.svg

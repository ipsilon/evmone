# Changelog

Documentation of all notable changes to the **evmone** project.

The format is based on [Keep a Changelog],
and this project adheres to [Semantic Versioning].

[0.15.0] — 2025-04-08

### Changed

- EOF:
  The is the final version compatible with [EOF devnet-0](https://notes.ethereum.org/@ethpandaops/eof-devnet-0).
  - Rename RETURNCONTRACT to RETURNCODE
    [#1153](https://github.com/ethereum/evmone/pull/1153)
  - Optimize allocations when validating the header
    [#1160](https://github.com/ethereum/evmone/pull/1160)
- EVMMAX:
  - evmmax: Add inversion method
    [#1142](https://github.com/ethereum/evmone/pull/1142)
  - evmmax: Use inv() instead of generated addchains
    [#1143](https://github.com/ethereum/evmone/pull/1143)
- Precompiles:
  - Handle trivial inputs to the expmod precompile
    [#1163](https://github.com/ethereum/evmone/pull/1163)
  - Use classic EC point add formula for BN precompiles
    [#1165](https://github.com/ethereum/evmone/pull/1165)
  - Optimize EC point multiplication for BN precompiles
    [#1166](https://github.com/ethereum/evmone/pull/1166)
  - Refactor the BN254 ecpairing precompile
    [#1175](https://github.com/ethereum/evmone/pull/1175)
- EIP-7702: Remove the check for empty in-state accounts
  [#1141](https://github.com/ethereum/evmone/pull/1141)
- Add prestate validation checks to the state test loader
  [#1134](https://github.com/ethereum/evmone/pull/1134)
- Upgrade the silkpre dependency
  [#1173](https://github.com/ethereum/evmone/pull/1173)

### Fixed

- Fix incorrect output size in the BN254 ecpairing precompile
  [#1174](https://github.com/ethereum/evmone/pull/1174)


[0.14.1] — 2025-03-11

### Fixed

- Fixes and improvements to BLS precompiles ([EIP-2537]).
  [#1155](https://github.com/ethereum/evmone/pull/1155)
  [#1154](https://github.com/ethereum/evmone/pull/1154)
  [#1150](https://github.com/ethereum/evmone/pull/1150)
  [#1151](https://github.com/ethereum/evmone/pull/1151)
  [#1148](https://github.com/ethereum/evmone/pull/1148)
- Check for topic when parsing deposit contract logs ([EIP-6110]).
  [#1152](https://github.com/ethereum/evmone/pull/1152)

### Changed

- Improvements to EOF validation.
  [#1137](https://github.com/ethereum/evmone/pull/1137)
  [#1144](https://github.com/ethereum/evmone/pull/1144)
  [#1145](https://github.com/ethereum/evmone/pull/1145)
  

[0.14.0] — 2025-02-19

### Added

- Support for all remaining [Prague] EIPs:
  - [EIP-6110]: Supply validator deposits on chain.
    [#1079](https://github.com/ethereum/evmone/pull/1079)
  - [EIP-7002] and [EIP-7251]: Withdrawal and consolidation requests.
    [#1084](https://github.com/ethereum/evmone/pull/1084)
  - [EIP-7623]: Increase calldata cost.
    [#1095](https://github.com/ethereum/evmone/pull/1095)
    [#1108](https://github.com/ethereum/evmone/pull/1108)
  - [EIP-7685]: General purpose execution layer requests.
    [#1083](https://github.com/ethereum/evmone/pull/1083)
  - [EIP-7691]: Blob throughput increase.
    [#1118](https://github.com/ethereum/evmone/pull/1118)
  - [EIP-7702]: Set EOA account code.
    [#961](https://github.com/ethereum/evmone/pull/961)
- EVMMAX-based BN254 pairing check precompile.
    [#852](hptps://github.com/ethereum/evmone/pull/852)
- New API for transaction execution: `StateView` & `StateDiff`.
  [#802](https://github.com/ethereum/evmone/pull/802)
- Introduce `BlockHashes` interface.
  [#1059](https://github.com/ethereum/evmone/pull/1059)
- Add option `-k` to filter tests by name in `evmone-statetest`.
  [#1111](https://github.com/ethereum/evmone/pull/1111)
- Add support for [EIP-4844] in blockchain tests.
  [#1077](https://github.com/ethereum/evmone/pull/1077)
- Add GDB pretty printers for common bytes and uint256 types.
  [#1024](https://github.com/ethereum/evmone/pull/1024)

### Changed

- Improvements to **EOF** validation and execution:
  - Move EOF to Osaka.
    [#1060](https://github.com/ethereum/evmone/pull/1060)
  - Optimized EOF validation using `std::move`.
    [#1036](https://github.com/ethereum/evmone/pull/1036)
  - Return constant hash of EXTCODEHASH of EOF.
    [#1035](https://github.com/ethereum/evmone/pull/1035)
  - Optimized EOF by reading types on demand.
    [#1034](https://github.com/ethereum/evmone/pull/1034)
  - Move EOF type validation outside of header validation.
    [#1052](https://github.com/ethereum/evmone/pull/1052)
  - Improve `has_full_data()` helper.
    [#1097](https://github.com/ethereum/evmone/pull/1097)
- Updates to BLS precompiles ([EIP-2537]).
  [#1089](https://github.com/ethereum/evmone/pull/1089)
- State and transaction execution improvements:
  - Split transaction and block related types.
    [#1031](https://github.com/ethereum/evmone/pull/1031)
  - Avoid returning bytes_view in system contracts.
    [#1048](https://github.com/ethereum/evmone/pull/1048)
  - Implement `CREATE` address scheme without RLP lib.
    [#1055](https://github.com/ethereum/evmone/pull/1055)
  - Separate transaction validation from transition.
    [#1069](https://github.com/ethereum/evmone/pull/1069)
  - Introduce TransactionProperties.
    [#1098](https://github.com/ethereum/evmone/pull/1098)
  - Simplify code modification indicator in StateDiff.
    [#1117](https://github.com/ethereum/evmone/pull/1117)
- Requirements and dependencies updates:
  - [EVMC] [12.1.0][EVMC 12.1.0] with EIP-7702 support.
    [#1125](https://github.com/ethereum/evmone/pull/1125)
  - [intx] [0.12.1][intx 0.12.1]
    [#1131](https://github.com/ethereum/evmone/pull/1131)
  - [ethash] [1.1.0][ethash 1.1.0]
    [#1131](https://github.com/ethereum/evmone/pull/1131)


## [0.13.0] — 2024-09-23

This release adds BLS precompiles and a system contract for [Prague]
and improves the interpreter API.

### Added

- Implementation of all [EIP-2537] BLS precompiles, enabled in **Prague**:
  [#984](https://github.com/ethereum/evmone/pull/984)
  - uses [blst] [v0.3.15](https://github.com/supranational/blst/releases/tag/v0.3.15) library,
    [#972](https://github.com/ethereum/evmone/pull/972)
    [#986](https://github.com/ethereum/evmone/pull/986)
  - `bls12_g1add` (`0x0b`)
    [#982](https://github.com/ethereum/evmone/pull/982)
  - `bls12_g1mul` (`0x0c`)
    [#994](https://github.com/ethereum/evmone/pull/994)
  - `bls12_g1msm` (`0x0d`)
    [#1010](https://github.com/ethereum/evmone/pull/1010)
  - `bls12_g2add` (`0x0e`)
    [#995](https://github.com/ethereum/evmone/pull/995)
  - `bls12_g2mul` (`0x0f`)
    [#999](https://github.com/ethereum/evmone/pull/999)
  - `bls12_g2msm` (`0x10`)
    [#1010](https://github.com/ethereum/evmone/pull/1010)
  - `bls12_pairing_check` (`0x11`)
    [#1016](https://github.com/ethereum/evmone/pull/1016)
  - `bls12_map_fp_to_g1` (`0x12`)
    [#1012](https://github.com/ethereum/evmone/pull/1012)
  - `bls12_map_fp2_to_g2` (`0x13`)
    [#1012](https://github.com/ethereum/evmone/pull/1012)
- Implementation of KZG proof verification (aka "point evaluation") precompile from [EIP-4844].
  [#979](https://github.com/ethereum/evmone/pull/979)
- Implementation of [EIP-2935] "Serve historical block hashes from state".
  [#953](https://github.com/ethereum/evmone/pull/953)

### Changed

- Refactor `system_call()` in preparation for more **Pectra** system contracts.
  [#976](https://github.com/ethereum/evmone/pull/976)
- Improved Baseline code analysis API.
  [#941](https://github.com/ethereum/evmone/pull/941)
- Provide execution states at VM object level and hide them from public API.
  [#1005](https://github.com/ethereum/evmone/pull/1005)
- Requirements and dependencies updates:
  - Support for 32-bit MSVC compiler has been dropped.
    [#973](https://github.com/ethereum/evmone/pull/973)
  - [intx] [v0.12.0](https://github.com/chfast/intx/releases/tag/v0.12.0)
    [#985](https://github.com/ethereum/evmone/pull/985)
- External test suites:
  - EEST EOF tests upgraded to [eip7692@v1.1.0](https://github.com/ethereum/execution-spec-tests/releases/tag/eip7692%40v1.1.0).
    [#1025](https://github.com/ethereum/evmone/pull/1025)
  - Added EEST tests for Pectra [pectra-devnet-3@v1.5.0](https://github.com/ethereum/execution-spec-tests/releases/tag/pectra-devnet-3%40v1.5.0)
    [#997](https://github.com/ethereum/evmone/pull/997)
  - [ethereum/tests] upgraded to [v14.1](https://github.com/ethereum/tests/releases/tag/v14.1).
    [#980](https://github.com/ethereum/evmone/pull/980)

### Fixed

- Fixed EOF parsing bug allowing multiple subcontainer kinds in the header.
  [#978](https://github.com/ethereum/evmone/pull/978)
- Ensure mandatory fields are included in the exported state tests.
  [#993](https://github.com/ethereum/evmone/pull/993)
- Properly handle EOF additions in `ExecutionState::reset()`.
  [#1004](https://github.com/ethereum/evmone/pull/1004)

### Removed

- The implementation of EOF's `TXCREATE` has been removed. It will be back when scheduled for a network upgrade.
  [#992](https://github.com/ethereum/evmone/pull/992)


## [0.12.0] — 2024-08-08

This release is focused on the Prague upgrade and EOF.

### Added

- Added `evmone-precompiles-bench` tool to benchmark precompiles.
  [#765](https://github.com/ethereum/evmone/pull/765)
- Added native implementations of the precompiled hash functions:
  - RIPEMD160
    [#846](https://github.com/ethereum/evmone/pull/846)
  - BLAKE2bf
    [#857](https://github.com/ethereum/evmone/pull/857)
  - SHA256
    [#924](https://github.com/ethereum/evmone/pull/924)
- Added `validate_eof` EVMC option to validate EOF before execution.
  This option is enabled by default in `evmc run`.
  [#768](https://github.com/ethereum/evmone/pull/768)
  [#960](https://github.com/ethereum/evmone/pull/960)
- Implemented [EIP-7610] "Revert creation in case of non-empty storage"
  in the testing infrastructure.
  [#816](https://github.com/ethereum/evmone/pull/816)
- Added `--version` option to testing tools.
  [#902](https://github.com/ethereum/evmone/pull/902)
- Introduce `TestState` and `TestAccount` to testing infrastructure.
  [#811](https://github.com/ethereum/evmone/pull/811)
- Added support for validating "initcode" containers in `eofparse` and `eoftest`.
  [#934](https://github.com/ethereum/evmone/pull/934)
  [#943](https://github.com/ethereum/evmone/pull/943)

### Changed

- **EVM Object Format (EOF)**
  
  Completed implementation of the [EIP-7692]: EVM Object Format (EOFv1) Meta.
  - Added `EOFCREATE` and `RETURNCONTRACT` instructions.
    [#553](https://github.com/ethereum/evmone/pull/553)
  - Added `TXCREATE` instruction, later moved to the future EOF version (Osaka).
    [#702](https://github.com/ethereum/evmone/pull/702)
    [#889](https://github.com/ethereum/evmone/pull/889)
  - Make `EXT*CALL` instructions Address Space Expansion ready.
    [#915](https://github.com/ethereum/evmone/pull/915)
  - Added EOF validation of sub-container kinds.
    [#876](https://github.com/ethereum/evmone/pull/876)
  - Limit validated container size to `MAX_INITCODE_SIZE`.
    [#930](https://github.com/ethereum/evmone/pull/930)
  - Added `RETURNDATALOAD` instruction.
    [#786](https://github.com/ethereum/evmone/pull/786)
  - Implementation of "less restricted" stack validation.
    [#676](https://github.com/ethereum/evmone/pull/676)
  - Added implementation of `EXCHANGE` from [EIP-663].
    [#839](https://github.com/ethereum/evmone/pull/839)
  - Disallow unreachable code sections in EOF containers.
    [#721](https://github.com/ethereum/evmone/pull/721)
    [#866](https://github.com/ethereum/evmone/pull/866)
  - Restrict `DUPN` and `SWAPN` to EOF only in EOF only.
    [#788](https://github.com/ethereum/evmone/pull/788)
  - Change `DATA*` opcodes.
    [#797](https://github.com/ethereum/evmone/pull/797)
  - Disable EOF ↔ legacy cross-creation.
    [#825](https://github.com/ethereum/evmone/pull/825)
  - Deprecate and reject code/gas-observability in EOF.
    [#834](https://github.com/ethereum/evmone/pull/834)
  - Make EOF opaque for `EXTCODE*` instructions.
    [#587](https://github.com/ethereum/evmone/pull/587)
  - Implement EOF creation transactions.
    [#878](https://github.com/ethereum/evmone/pull/878)
  - Modify EOF `RETURNDATA*` to allow out-of-bounds reads (instead of failing execution).
    [#909](https://github.com/ethereum/evmone/pull/909)
  - Tune EOF validation: disallow truncated data in top-level EOF containers.
    [#921](https://github.com/ethereum/evmone/pull/921)
  - Disallow unreferenced sub-containers and sub-containers of conflicting kinds.
    [#916](https://github.com/ethereum/evmone/pull/916)

- **Testing**

  There are a lot of improvements to the testing tools and test formats.
  In particular, big portion of evmone's unit tests has been re-shaped
  to have a structure of State Tests or EOF Validation Tests.

  Moreover, we added the option to export these tests to JSON and the archive
  of the exported tests ("fixture") is the artifact of this release.

  Upgraded external test suites:
  - [ethereum/tests][Ethereum Execution Tests]: [14.0][tests 14.0]
  - [Execution Spec Tests]: [3.0.0][Execution Spec Tests 3.0.0]

  Other details:
  - Add some missing State Test export features.
    [#807](https://github.com/ethereum/evmone/pull/807)
  - Check for unexpected `EF` prefix in test code.
    [#809](https://github.com/ethereum/evmone/pull/809)
  - EOF Validation Test fixture.
    [#810](https://github.com/ethereum/evmone/pull/810)
  - Export EOF validation unit tests to JSON EOF Validation Tests.
    [#818](https://github.com/ethereum/evmone/pull/818)
  - Output failed test case index in EOF Validation Tests.
    [#820](https://github.com/ethereum/evmone/pull/820)
  - Add `ExportableFixture` for JSON tests exporting.
    [#821](https://github.com/ethereum/evmone/pull/821)
  - Recognize all official fork/revision names.
    [#830](https://github.com/ethereum/evmone/pull/830)
  - Export State Tests with invalid transactions.
    [#858](https://github.com/ethereum/evmone/pull/858)
  - Allow `"to": null` in JSON transactions.
    [#927](https://github.com/ethereum/evmone/pull/927)
  - EOF Validation Tests runner: support "initcode" flag.
    [#936](https://github.com/ethereum/evmone/pull/936)
  - `evmone-blockchaintest`: Simplify genesis handling.
    [#954](https://github.com/ethereum/evmone/pull/954)
  - Optimization: only empty accounts are marked "touched".
    [#785](https://github.com/ethereum/evmone/pull/785)
  - Adjust ethash difficulty if below minimum `0x20000`.
    [#803](https://github.com/ethereum/evmone/pull/803)

- Requirements and dependencies updates:
  - CMake 3.18
    [#840](https://github.com/ethereum/evmone/pull/840)
  - Xcode 15.0
    [#847](https://github.com/ethereum/evmone/pull/847)
  - [EVMC] [12.0.0][EVMC 12.0.0]
    [#966](https://github.com/ethereum/evmone/pull/966)
  - [intx] [0.11.0][intx 0.11.0]
    [#967](https://github.com/ethereum/evmone/pull/967)

- Use 32-byte aligned allocation for Baseline stack space.
  [#907](https://github.com/ethereum/evmone/pull/907)
- Split Baseline analysis and execution into separate files.
  [#946](https://github.com/ethereum/evmone/pull/946)
- Convert EVMMAX to header-only library with full `constexpr` capabilities.
  [#864](https://github.com/ethereum/evmone/pull/864)
  [#964](https://github.com/ethereum/evmone/pull/964)
- Return number of errors from `eofparse`.
  [#873](https://github.com/ethereum/evmone/pull/873)

### Fixed

- Implement Frontier behavior of failing code deployment (testing infrastructure).
  [#824](https://github.com/ethereum/evmone/pull/824)
- Fix error messages for compatibility with external testing tools.
  [#828](https://github.com/ethereum/evmone/pull/828)
  [#886](https://github.com/ethereum/evmone/pull/886)
- Fix initcode handling before EOF is enabled:
  an initcode staring with `EF00` should not be validated as EOF unconditionally.
  [#893](https://github.com/ethereum/evmone/pull/893)
- Fix EOF header parsing bug (introduced by code refactoring).
  [#957](https://github.com/ethereum/evmone/pull/957)
  [#958](https://github.com/ethereum/evmone/pull/958)
- Fix `eoftest` to run all tests from a JSON file.
  [#935](https://github.com/ethereum/evmone/pull/935)
- Improve output buffer handling for precompiles in testing infrastructure.
  This fixes out-of-bound access for some fuzzing-generated state tests.
  [#951](https://github.com/ethereum/evmone/pull/951)


## [0.11.0] — 2023-12-21

This release is focused on [Cancun] and EOF.

### Added

- **[Cancun] Network Upgrade fully supported**
  - [EIP-1153]: Transient storage opcodes
    - transient storage & `TLOAD` and `TSTORE` instructions
      [#669](https://github.com/ethereum/evmone/pull/669)
    - clearing of transient storage between transactions
      [#715](https://github.com/ethereum/evmone/pull/715)
  - [EIP-4788]: Beacon block root in the EVM
    - don't assume the transaction sender exists
      [#731](https://github.com/ethereum/evmone/pull/731)
    - system call to the _Beacon Roots_ contract
      [#709](https://github.com/ethereum/evmone/pull/709)
  - [EIP-4844]: Shard Blob Transactions
    - `BLOBHASH` instruction
      [#668](https://github.com/ethereum/evmone/pull/668)
    - blob transactions
      [#713](https://github.com/ethereum/evmone/pull/713)
    - stub of the `point_evaluation` precompile
      [#730](https://github.com/ethereum/evmone/pull/730)
  - [EIP-5656]: `MCOPY` - Memory copying instruction
    [#629](https://github.com/ethereum/evmone/pull/629)
    [#648](https://github.com/ethereum/evmone/pull/648)
  - [EIP-6780]: `SELFDESTRUCT` only in same transaction
    [#735](https://github.com/ethereum/evmone/pull/735)
  - [EIP-7516]: `BLOBBASEFEE` opcode
    [#708](https://github.com/ethereum/evmone/pull/708)
- **EVM Modular Arithmetic Extensions ([EVMMAX])**
  - Added basic EVMMAX support in form of C++ API.
    [#673](https://github.com/ethereum/evmone/pull/673)
  - Implementation of secp256k1 ECDSA recovery (`ecrecovery` precompile) using EVMMAX
    [#688](https://github.com/ethereum/evmone/pull/688)
  - Implementation of `ecadd` and `ecmul` BN254 precompiles using EVMMAX
    [#716](https://github.com/ethereum/evmone/pull/716)
- Initial support for [Blockchain Tests]
  - block execution
    [#681](https://github.com/ethereum/evmone/pull/681)
    [#679](https://github.com/ethereum/evmone/pull/679)
    [#685](https://github.com/ethereum/evmone/pull/685)
    [#701](https://github.com/ethereum/evmone/pull/701)
  - test format support & results encoding
    [#680](https://github.com/ethereum/evmone/pull/680)
    [#690](https://github.com/ethereum/evmone/pull/690)
    [#694](https://github.com/ethereum/evmone/pull/694)
    [#711](https://github.com/ethereum/evmone/pull/711)
    [#736](https://github.com/ethereum/evmone/pull/736)
  - PoW difficulty calculation
    [#682](https://github.com/ethereum/evmone/pull/682)
    [#718](https://github.com/ethereum/evmone/pull/718)
- Optionally use [Silkworm] as the precompiles implementation.
  [#660](https://github.com/ethereum/evmone/pull/660)
- Support for executing [JSON EOF Tests](https://github.com/ethereum/tests/tree/v13/EOFTests)
  (thanks @gzanitti)
  [#678](https://github.com/ethereum/evmone/pull/678)
- EVM tracing option `--trace` in `evmone-t8n`
  [#616](https://github.com/ethereum/evmone/pull/616)
- Support for compiling for `riscv32` architecture
  [#700](https://github.com/ethereum/evmone/pull/700)
- Ability to export evmone's unit tests to the JSON State Test format
  [#743](https://github.com/ethereum/evmone/pull/743)

### Changed

- **EVM Object Format (EOF)**

  EOF implementation follows the [EOF spec] (aka _Mega EOF Endgame_)
  and is tentatively enabled in the **Prague** EVM revision.
  - Tests have been migrated to [ipsilon/tests/eof](https://github.com/ipsilon/tests/tree/eof)
    [#651](https://github.com/ethereum/evmone/pull/651)
  - Implementation of four new instructions for accessing _data sections_:
    `DATALOAD`, `DATALOADN`, `DATASIZE`, `DATACOPY`
    [#586](https://github.com/ethereum/evmone/pull/586)
    [#663](https://github.com/ethereum/evmone/pull/663)
    [#717](https://github.com/ethereum/evmone/pull/717)
    [#741](https://github.com/ethereum/evmone/pull/741)
  - Forbid `DELEGATECALL` from EOF to legacy contracts during execution
    [#588](https://github.com/ethereum/evmone/pull/588)
  - The data section kind has been changed to `0x04`
    [#632](https://github.com/ethereum/evmone/pull/632)
  - The `RJUMPV` immediate argument meaning has been changed to "max index"
    [#640](https://github.com/ethereum/evmone/pull/640)
  - Implementation of the `JUMPF` instruction and the _non-returning_ functions
    [#644](https://github.com/ethereum/evmone/pull/644)

- Opcodes of new instructions have been assigned following
  [the execution-spec opcodes list](https://github.com/ethereum/execution-specs/tree/master/lists/evm)
  [#665](https://github.com/ethereum/evmone/pull/665)
- State changes are now reverted with the journal
  [#689](https://github.com/ethereum/evmone/pull/689)
- Compatibility of `evmone-statetest` with [goevmlab] has been improved
  [#658](https://github.com/ethereum/evmone/pull/658)
  [#757](https://github.com/ethereum/evmone/pull/757)
- Minimal tested/supported compilers versions:
  [#675](https://github.com/ethereum/evmone/pull/675)
  - GCC 11
  - Clang 13
  - XCode 14.3.1 (bumped from 13.4)
  - Visual Studio 2022
  - CMake 3.16...3.27
- [EVMC] has been upgraded to version [11.0.1][EVMC 11.0.1].
  [#754](https://github.com/ethereum/evmone/pull/754)
  [#738](https://github.com/ethereum/evmone/pull/738)
  [#707](https://github.com/ethereum/evmone/pull/707)
  [#669](https://github.com/ethereum/evmone/pull/669)
- [intx] has been upgraded to version [0.10.1][intx 0.10.1].
  [#674](https://github.com/ethereum/evmone/pull/674)
- [Ethereum Execution Tests] has been upgraded to version [13][tests 13]
  and [Execution Spec Tests] version [1.0.6][Execution Spec Tests 1.0.6] has been added.
  [#737](https://github.com/ethereum/evmone/pull/737)

### Fixed

- EOF: Fix `CALLF` runtime stack overflow check
  [#677](https://github.com/ethereum/evmone/pull/677)
- EOF: Fix missing `CALLF` stack overflow validation
  [#619](https://github.com/ethereum/evmone/pull/619)
- Fixed processing of withdrawals with 0 amount (testing infrastructure)
  [#630](https://github.com/ethereum/evmone/pull/630)
- Fixed handling of _short_ nodes in Merkle Patricia Trie (testing infrastructure)
  [#686](https://github.com/ethereum/evmone/pull/686)


## [0.10.0] — 2023-05-08

The highlights of this release are support for [Shanghai] execution specification upgrade
and implementation of EOF "v1.0". There are also big enhancements to the EVM testing tools
and infrastructure. In particular, we added the [t8n] command-line utility.

As it is tradition, the EVM performance has been improved as well.
Comparing with the previous release using the "main" benchmark suite,
the Baseline interpreter is now:

- 10–45% (mean 25%) faster for GCC builds,
- 0–19% (mean 11%) faster for Clang builds.

### Added

- **[Shanghai] support**:
  - [EIP-3651]: Warm COINBASE (testing infrastructure only).
    [#560](https://github.com/ethereum/evmone/pull/560)
  - [EIP-3860]: Limit and meter initcode.
    [#545](https://github.com/ethereum/evmone/pull/545)
  - [EIP-4895]: Beacon chain push withdrawals as operations (testing infrastructure only).
    [#614](https://github.com/ethereum/evmone/pull/614)
- **EVM Object Format "EOF v1.0"**:
  - The implementation of the revisions of [EIP-3540], [EIP-3670], [EIP-4200], [EIP-4750] and [EIP-5450]
    originally proposed for [Shanghai].
    [#563](https://github.com/ethereum/evmone/pull/563)
    [#572](https://github.com/ethereum/evmone/pull/572)
    [#594](https://github.com/ethereum/evmone/pull/594)
    [#508](https://github.com/ethereum/evmone/pull/508)
  - EOF is currently enabled in the [Cancun] revision but likely to be moved to Prague in the future.
    [#561](https://github.com/ethereum/evmone/pull/561)
  - Added `evmone-eofparse` and `evmone-eofparsefuzz` tools for testing EOF validation.
    [#568](https://github.com/ethereum/evmone/pull/568)
- Implemented [EIP-663]: Unlimited SWAP and DUP instructions (enabled in EOF).
  [#529](https://github.com/ethereum/evmone/pull/529)
- Added implementation of `evmc::Host`, **state transition** and block finalization for testing purposes.
  [#484](https://github.com/ethereum/evmone/pull/484)
  [#519](https://github.com/ethereum/evmone/pull/519)
  [#575](https://github.com/ethereum/evmone/pull/575)
  [#608](https://github.com/ethereum/evmone/pull/608)
  [#609](https://github.com/ethereum/evmone/pull/609)
- Added **[t8n]** tool `evmone-t8n`
  — a command line utility for transaction execution and state transition testing.
  It allows executing and generating tests with cooperation of [retesteth]
  or [Execution Spec Tests].
  [#552](https://github.com/ethereum/evmone/pull/552)
  [#555](https://github.com/ethereum/evmone/pull/555)
  [#558](https://github.com/ethereum/evmone/pull/558)
  [#569](https://github.com/ethereum/evmone/pull/569)
  [#583](https://github.com/ethereum/evmone/pull/583)
  [#590](https://github.com/ethereum/evmone/pull/590)
  [#591](https://github.com/ethereum/evmone/pull/591)
  [#604](https://github.com/ethereum/evmone/pull/604)
  [#606](https://github.com/ethereum/evmone/pull/606)
  [#607](https://github.com/ethereum/evmone/pull/607)
  [#612](https://github.com/ethereum/evmone/pull/612)
- Added partial support for EVM Precompiles
  — gas cost computation and execution via _[JSON stub file](./test/state/precompiles_stub.json)_.
  [#524](https://github.com/ethereum/evmone/pull/524)
- Declarative state transition unit test suite.
  [#589](https://github.com/ethereum/evmone/pull/589)
- CMake option `EVMONE_X86_64_ARCH_LEVEL` to set the
  [x86-64 microarchitecture level](https://en.wikipedia.org/wiki/X86-64#Microarchitecture_levels).
  On Linux and Windows this is set to x86-64-v2 by default.
  [#548](https://github.com/ethereum/evmone/pull/548)

### Changed

- C++20 is now required to build evmone.
  [#502](https://github.com/ethereum/evmone/pull/502)
- Minimal tested/supported compilers versions:
  [#535](https://github.com/ethereum/evmone/pull/535)
  - GCC 11
  - Clang 13
  - XCode 13.4
  - Visual Studio 2022
  - CMake 3.16
- [EVMC] has been upgraded to version [10.1.0][EVMC 10.1.0].
  [#623](https://github.com/ethereum/evmone/pull/623)
- [intx] has been upgraded to version [0.10.0][intx 0.10.0].
  [#622](https://github.com/ethereum/evmone/pull/622)
- [ethash] has been upgraded to version [1.0.0][ethash 1.0.0].
  [#540](https://github.com/ethereum/evmone/pull/540)
- [Ethereum Execution Tests] has been upgraded to version [12.2][tests 12.2].
  [#625](https://github.com/ethereum/evmone/pull/625)
- **Baseline interpreter optimizations**:
  - Better stack overflow/underflow checks.
    [#518](https://github.com/ethereum/evmone/pull/518)
  - SWAP instructions optimization for Clang.
    [#527](https://github.com/ethereum/evmone/pull/527)
  - Pass gas counter to memory grow/check helpers by value.
    [#598](https://github.com/ethereum/evmone/pull/598)
  - Pass gas counter to instructions by value.
    [#600](https://github.com/ethereum/evmone/pull/600)
- Changes to EVM tracing:
  - Instruction trace prints `"gas"` and `"gasUsed"` as hex numbers to match geth.
    [#592](https://github.com/ethereum/evmone/pull/592)
  - C++ tracing API has separated the `gas` parameter.
    [#599](https://github.com/ethereum/evmone/pull/599)
- Improvements to the JSON State Test execution tool `evmone-statetest`:
  - Ability to load tests from multiple dirs/files.
    [#512](https://github.com/ethereum/evmone/pull/512)
  - Validate deployed EOF code before state test execution.
    [#593](https://github.com/ethereum/evmone/pull/593)
    [#601](https://github.com/ethereum/evmone/pull/601)
  - Added `--trace` command-line flag to enable EVM execution tracing.
    [#543](https://github.com/ethereum/evmone/pull/543)
  - Other improvements.
    [#556](https://github.com/ethereum/evmone/pull/556)
    [#603](https://github.com/ethereum/evmone/pull/603)
- Benchmarks (invocable by `evmone-bench`) have been migrated to
  external [evm-benchmarks] which use JSON State Test format.
  [#513](https://github.com/ethereum/evmone/pull/513)
  [#530](https://github.com/ethereum/evmone/pull/530)
- Removed dependency on `evmc::instructions`.
  [#533](https://github.com/ethereum/evmone/pull/533)
  [#534](https://github.com/ethereum/evmone/pull/534)
  [#537](https://github.com/ethereum/evmone/pull/537)

### Fixed

- Fixed calling `Tracer.notify_execution_start`.
  [#531](https://github.com/ethereum/evmone/pull/531)
- Fixed instruction tracing of EOF code.
  [#536](https://github.com/ethereum/evmone/pull/536)

### New Contributors

- **[JSzymanskiJS](https://github.com/ethereum/evmone/commits?author=JSzymanskiJS)**
  [#512](https://github.com/ethereum/evmone/pull/512)
- **[miles170](https://github.com/ethereum/evmone/commits?author=miles170)**
  [#513](https://github.com/ethereum/evmone/pull/513)
- **[rodiazet](https://github.com/ethereum/evmone/commits?author=rodiazet)**
  [#531](https://github.com/ethereum/evmone/pull/531)


## [0.9.1] — 2022-09-07

### Fixed

- Resetting gas refund counter when execution state is reused. 
  [#504](https://github.com/ethereum/evmone/pull/504)


## [0.9.0] — 2022-08-30

In this release we have been focused on improving performance of the Baseline interpreter.
The end result is that the Baseline is **26% faster** than in previous version 0.8.0
and **18% faster** than the current Advanced interpreter while having
over **8x smaller** code analysis cost. The Baseline is now the _default_ interpreter because 
it is simpler and has become better than the Advanced.

The Advanced also has got **4% faster** than in the previous version.

All numbers are from running the "main" benchmark suite
on 4.0 GHz Intel Haswell CPU, using the Clang 15 compiler.

Moreover, evmone now calculates _gas refund_ and reports it back using [EVMC 10][EVMC 10.0.0] API.

Finally, the options `O=2` and `O=0` have been replaced by `advanced`. See below for details.

### Added

- Calculation of EVM gas refunds.
  [#493](https://github.com/ethereum/evmone/pull/493)
- `PUSH0` instruction implementation ([EIP-3855]), enabled in [Shanghai].
  [#448](https://github.com/ethereum/evmone/pull/448)
  [#432](https://github.com/ethereum/evmone/pull/432)
- Experimental [EOF] validation and execution ([EIP-3540]), enabled in [Shanghai].
  [#334](https://github.com/ethereum/evmone/pull/334)
  [#366](https://github.com/ethereum/evmone/pull/366)
  [#471](https://github.com/ethereum/evmone/pull/471)
- _In progress_ State Transition execution tool for testing purposes. So far we've merged:
  - RLP encoding,
    [#463](https://github.com/ethereum/evmone/pull/463)
  - Merkle Patricia Trie root hash computing,
    [#477](https://github.com/ethereum/evmone/pull/477)
    [#478](https://github.com/ethereum/evmone/pull/478)
  - JSON State Transition Test loader.
    [#479](https://github.com/ethereum/evmone/pull/479)

### Changed

- EVMC options `O=0` (use Baseline) and `O=2` (use Advanced) have been replaced with single
  option `advanced` to use the non-default Advanced interpreter.
  [#500](https://github.com/ethereum/evmone/pull/500)
- Baseline has replaced Advanced as the default interpreter. The later can still be selected
  with the `advanced` option. Reasons are explained in the introduction.
  [#500](https://github.com/ethereum/evmone/pull/500)
- _A lot_ of changes related to the optimization of the Baseline interpreter, including
  refactoring and optimization of instructions' implementations.
- The Baseline interpreter now uses "computed goto" dispatch if supported by C++ compiler.
  The "switch" dispatch can be forced with the `cgoto=no` option.
  [#495](https://github.com/ethereum/evmone/pull/495)
- Improvements to basic block metadata placement in the Advanced interpreter.
  [#457](https://github.com/ethereum/evmone/pull/457)
  [#474](https://github.com/ethereum/evmone/pull/474)
- [EVMC] has been upgraded to version [10.0.0][EVMC 10.0.0].
  [#499](https://github.com/ethereum/evmone/pull/499)
- [intx] has been upgrade to version [0.8.0][intx 0.8.0].
  [#446](https://github.com/ethereum/evmone/pull/446)

### Removed

- `evmone-fuzzer` has removed [aleth-interpreter][Aleth] as it is not maintained
  and lacks the latest EVM features.
  [#453](https://github.com/ethereum/evmone/pull/453)


## [0.8.2] — 2021-08-26

### Fixed

- Fixed building of `evmone-standalone` static library when the `llvm-ar` tool is being used.
  [#373](https://github.com/ethereum/evmone/pull/373)
  [#374](https://github.com/ethereum/evmone/pull/374)


## [0.8.1] — 2021-08-03

### Fixed

- baseline: Fix incorrect exit after invalid jump.
  [#370](https://github.com/ethereum/evmone/pull/370)


## [0.8.0] — 2021-07-01

### Added

- Full support for **[London]** EVM revision:
  - [EVMC] upgraded to version [9.0.0][EVMC 9.0.0].
    [#348](https://github.com/ethereum/evmone/pull/348)
  - Implementation of the [EIP-3198] "BASEFEE opcode".
    [#333](https://github.com/ethereum/evmone/pull/333)
- Instruction tracing ([EIP-3155]) can be enabled via `trace` option in Baseline.
  [#325](https://github.com/ethereum/evmone/pull/325)
- Summary of number of executed opcodes is reported if `histogram` option is enabled in Baseline.
  [#323](https://github.com/ethereum/evmone/pull/323)
- The `evmone-bench` now reports time of execution without code analysis under "execute" label.
  The EVMC-like analysis + execution invocation is reported as "total".
  [#343](https://github.com/ethereum/evmone/pull/343)
- The `evmone-bench` has started utilizing `evmc::MockedHost` which allows using
  state-access (e.g. `SLOAD`/`SSTORE`) instructions in benchmarks.
  [#319](https://github.com/ethereum/evmone/pull/319)

### Changed

- Improvements to semi-public `evmone::baseline` API.
  [#314](https://github.com/ethereum/evmone/pull/314)
- The [intx] has been upgraded to version [0.6.0][intx 0.6.0]
  which increases performance of `ADDMOD` instruction.
  [#345](https://github.com/ethereum/evmone/pull/345)
- The [ethash] has been upgraded to version [0.7.0][ethash 0.7.0]
  which provides faster `KECCAK256` implementation.
  [#332](https://github.com/ethereum/evmone/pull/332)
- Optimizations in Baseline interpreter.
  [#315](https://github.com/ethereum/evmone/pull/315)
  [#341](https://github.com/ethereum/evmone/pull/341)
  [#344](https://github.com/ethereum/evmone/pull/344)
- The [Ethereum Execution Tests] upgraded to version [9.0.2][tests 9.0.2].
  [#349](https://github.com/ethereum/evmone/pull/349)


## [0.7.0] — 2021-04-27

### Added

- Full support for **[Berlin]** EVM revision and [EIP-2929].
  [#289](https://github.com/ethereum/evmone/pull/289)
  [#301](https://github.com/ethereum/evmone/pull/301)

### Changed

- [EVMC] has been upgraded to version [8.0.0][EVMC 8.0.0]. This ABI breaking
  change has been required to support **Berlin** revision.
  [#309](https://github.com/ethereum/evmone/pull/309)
- Optimizations to basic `JUMPDEST` analysis used by Baseline interpreter.
  [#306](https://github.com/ethereum/evmone/pull/306)
  [#308](https://github.com/ethereum/evmone/pull/308)
- The Baseline interpreter API has been modified to allow caching
  of the `JUMPDEST` analysis.
  [#305](https://github.com/ethereum/evmone/pull/305)
- The consensus testing is now driven by [Silkworm] as a replacement of 
  the unmaintained [Aleth]. The [Ethereum Execution Tests] [8.0.4][tests 8.0.4] are currently being used.


## [0.6.0] — 2021-04-07

### Added

- New experimental **Baseline** interpreter has been added to the project.
  It provides relatively straight-forward EVM implementation and
  can be enabled with `O=0` option.
  [#261](https://github.com/ethereum/evmone/pull/261)
  [#280](https://github.com/ethereum/evmone/pull/280)
- A set of EVM synthetic benchmarks stressing individual
  low-level EVM instructions.
  [#278](https://github.com/ethereum/evmone/pull/278)
- [Silkworm]-driven integration and Ethereum consensus testing.
  [#290](https://github.com/ethereum/evmone/pull/290)

### Changed

- [EVMC] upgraded to version [7.5.0][EVMC 7.5.0].
  [#294](https://github.com/ethereum/evmone/pull/294)
- `evmone-bench` tool under-the-hood improvements.
  [#286](https://github.com/ethereum/evmone/pull/286)
  [#287](https://github.com/ethereum/evmone/pull/287)
  [#288](https://github.com/ethereum/evmone/pull/288)
- A lot of instructions implementation refactoring to allow code sharing
  between Baseline and Advanced interpreters.


## [0.5.0] — 2020-06-24

### Changed

- [intx] upgraded to version [0.5.0][intx 0.5.0], small performance increase for
  `ADDMOD` and `MULMOD` instructions expected.
  [#239](https://github.com/ethereum/evmone/pull/239)
- [EVMC] upgraded to version [7.4.0][EVMC 7.4.0].
  [#243](https://github.com/ethereum/evmone/pull/243)
- C++ exception handling and Run-Time Type Information (RTTI) have been disabled
  for the evmone library (in GCC and Clang compilers).
  [#244](https://github.com/ethereum/evmone/pull/244)


## [0.4.1] — 2020-04-01

### Fixed

- The release binaries for Windows are now built without AVX instruction set
  enabled. That was never intended and is consistent with binaries for other 
  operating systems.
  [#230](https://github.com/ethereum/evmone/pull/230)

## [0.4.0] — 2019-12-09

### Fixed

- In previous versions evmone incorrectly assumed that code size cannot exceed
  24576 bytes (0x6000) — the limit introduced for the size of newly deployed
  contracts by [EIP-170] in [Spurious Dragon]. The limit do not apply to
  contract creating init code (i.e. in context of "create" transaction or CREATE
  instruction). Therefore, the pre-processing phase in evmone has been reworked
  to raise the technical limits or eliminated them entirely. From now on, only
  blocks of instruction with total base gas cost exceeding 4294967295 (2³² - 1)
  combined with execution gas limit also above this value can cause issues.
  [#217](https://github.com/ethereum/evmone/pull/217)
  [#218](https://github.com/ethereum/evmone/pull/218)
  [#219](https://github.com/ethereum/evmone/pull/219)
  [#221](https://github.com/ethereum/evmone/pull/221)

### Changed

- [EVMC] has been upgraded to version [7.1.0][EVMC 7.1.0].
  [#212](https://github.com/ethereum/evmone/pull/212)

## [0.3.0] — 2019-11-14

This release of evmone adds changes for **[Istanbul]** EVM revision.

### Added

- **Istanbul** EVM revision support with new costs for some instructions ([EIP-1884]).
  [#191](https://github.com/ethereum/evmone/pull/191)
- Implementation of CHAINID instruction from the **Istanbul** EVM revision ([EIP-1344]).
  [#190](https://github.com/ethereum/evmone/pull/190)
- Implementation of SELFBALANCE instruction from the **Istanbul** EVM revision ([EIP-1884]).
  [#24](https://github.com/ethereum/evmone/pull/24)
- Implementation of new cost model for SSTORE from the **Istanbul** EVM revision ([EIP-2200]).
  [#142](https://github.com/ethereum/evmone/pull/142)

### Changed

- [EVMC] has been upgraded to version [7.0.0][EVMC 7.0.0].
  [#204](https://github.com/ethereum/evmone/pull/204)


## [0.2.0] — 2019-09-24

This release of evmone is binary compatible with 0.1 and delivers big performance improvements
– both code preprocessing and execution is **~66%** faster (needs ~40% less time).

### Added

- **evm-test** – the testing tool for [EVMC]-compatible EVM implementations.
  [#85](https://github.com/ethereum/evmone/pull/85)
- **evmone-fuzzer** – the testing tool that fuzzes evmone execution against [aleth-interpreter][Aleth] execution.
  Any other [EVMC]-compatible EVM implementation can be added easily.
  [#162](https://github.com/ethereum/evmone/pull/162)
  [#184](https://github.com/ethereum/evmone/pull/184)
- **evmone-standalone** – single static library that bundles evmone with all its static library dependencies 
  (available on Linux, but support can be extended to other platforms).
  [#95](https://github.com/ethereum/evmone/pull/95)
- The **evmone-bench** tool has learned how to benchmark external [EVMC]-compatible EVMs.
  [#111](https://github.com/ethereum/evmone/pull/111)
- The **evmone-bench** tool sorts test cases by file names and allows organizing them in subfolders.
  [#150](https://github.com/ethereum/evmone/pull/150)
- The docker image [ethereum/evmone](https://hub.docker.com/r/ethereum/evmone)
  with evmone and modified geth is available on Docker Hub.
  [#127](https://github.com/ethereum/evmone/pull/127)


### Changed

#### Optimizations

- Instead of checking basic block preconditions (base gas cost, stack requirements) in the dispatch loop, 
  this is now done in the special "BEGINBLOCK" instruction — execution time reduction **-2–8%**.
  [#74](https://github.com/ethereum/evmone/pull/74)
- New EVM stack implementation has replaced naïve usage of `std::vector<intx::uint256>` — **-8–16%**.
  [#79](https://github.com/ethereum/evmone/pull/79)
- Improvements to interpreter's dispatch loop — **-4–9%**.
  [#107](https://github.com/ethereum/evmone/pull/107)
- Optimization of the JUMPDEST map — up to **-34%**.
  [#80](https://github.com/ethereum/evmone/pull/80)
- Optimizations to code preprocessing / analysis.
  [#121](https://github.com/ethereum/evmone/pull/121)
  [#125](https://github.com/ethereum/evmone/pull/125)
  [#153](https://github.com/ethereum/evmone/pull/153)
  [#168](https://github.com/ethereum/evmone/pull/168)
  [#178](https://github.com/ethereum/evmone/pull/178)
- Push instructions with values up to 8 bytes (PUSH1–PUSH8)
  are now handled much more efficiently — up to **-9%**.
  [#122](https://github.com/ethereum/evmone/pull/122)
- Pointer to next instruction is now obtained in instruction implementations 
  (instead of the dispatch loop) and is kept in CPU registers only — **-3–7%**.
  [#133](https://github.com/ethereum/evmone/pull/133)
- The run-time information about basic blocks has been compressed.
  [#139](https://github.com/ethereum/evmone/pull/139)
  [#144](https://github.com/ethereum/evmone/pull/144)
  
#### Other changes

- The DUP, SWAP, LOG and CALL instructions are now implemented by individual functions (template instances)
  instead of a parametrized function handling each family of instructions.
  [#126](https://github.com/ethereum/evmone/pull/126)
  [#159](https://github.com/ethereum/evmone/pull/159)
- [EVMC] upgraded to version [6.3.1](https://github.com/ethereum/evmc/releases/tag/v6.3.1).
  [#129](https://github.com/ethereum/evmone/pull/129)
  [#77](https://github.com/ethereum/evmone/pull/77)
  [#96](https://github.com/ethereum/evmone/pull/96)
- [intx] upgraded to version [0.4.0](https://github.com/chfast/intx/releases/tag/v0.4.0).
  [#131](https://github.com/ethereum/evmone/pull/131)
- The ability to provide custom opcode table for code preprocessing has been dropped.
  [#167](https://github.com/ethereum/evmone/pull/167)


### Fixed

- The gas calculation for blocks containing an undefined instruction has been fixed.
  This bug could not cause consensus issue because a block with an undefined instruction terminates 
  with an exception despite incorrect gas checking.
  However, execution might have ended with a confusing error code.
  [#93](https://github.com/ethereum/evmone/pull/93)
- Fix for LOG being emitted after _out-of-gas_ exception.
  [#120](https://github.com/ethereum/evmone/pull/120)


## [0.1.1] — 2019-09-11

### Changed

- [EVMC] upgraded to version 6.3.1 (still ABI-compatible with evmone 0.1.0).
  [#171](https://github.com/ethereum/evmone/pull/171)
- Changes to the **evmone-bench** tool backported from 0.2. 
  This allows better performance comparison between 0.1 and 0.2 as both versions
  can run the same set of benchmarks.
  [#172](https://github.com/ethereum/evmone/pull/172)


## [0.1.0] — 2019-06-19

The first release of the evmone project. 
It delivers fully-compatible and high-speed EVM implementation.

### Added

- Support for all current EVM revisions up to [Petersburg].
- Exposes [EVMC] 6 ABI.
- The [intx 0.2.0](https://github.com/chfast/intx/releases/tag/v0.2.0) library is used for 256-bit precision arithmetic. 

[0.15.0]: https://github.com/ethereum/evmone/releases/tag/v0.15.0
[0.14.1]: https://github.com/ethereum/evmone/releases/tag/v0.14.1
[0.14.0]: https://github.com/ethereum/evmone/releases/tag/v0.14.0
[0.13.0]: https://github.com/ethereum/evmone/releases/tag/v0.13.0
[0.12.0]: https://github.com/ethereum/evmone/releases/tag/v0.12.0
[0.11.0]: https://github.com/ethereum/evmone/releases/tag/v0.11.0
[0.10.0]: https://github.com/ethereum/evmone/releases/tag/v0.10.0
[0.9.1]: https://github.com/ethereum/evmone/releases/tag/v0.9.1
[0.9.0]: https://github.com/ethereum/evmone/releases/tag/v0.9.0
[0.8.2]: https://github.com/ethereum/evmone/releases/tag/v0.8.2
[0.8.1]: https://github.com/ethereum/evmone/releases/tag/v0.8.1
[0.8.0]: https://github.com/ethereum/evmone/releases/tag/v0.8.0
[0.7.0]: https://github.com/ethereum/evmone/releases/tag/v0.7.0
[0.6.0]: https://github.com/ethereum/evmone/releases/tag/v0.6.0
[0.5.0]: https://github.com/ethereum/evmone/releases/tag/v0.5.0
[0.4.1]: https://github.com/ethereum/evmone/releases/tag/v0.4.1
[0.4.0]: https://github.com/ethereum/evmone/releases/tag/v0.4.0
[0.3.0]: https://github.com/ethereum/evmone/releases/tag/v0.3.0
[0.2.0]: https://github.com/ethereum/evmone/releases/tag/v0.2.0
[0.1.1]: https://github.com/ethereum/evmone/releases/tag/v0.1.1
[0.1.0]: https://github.com/ethereum/evmone/releases/tag/v0.1.0

[EIP-170]: https://eips.ethereum.org/EIPS/eip-170
[EIP-663]: https://eips.ethereum.org/EIPS/eip-663
[EIP-1153]: https://eips.ethereum.org/EIPS/eip-1153
[EIP-1884]: https://eips.ethereum.org/EIPS/eip-1884
[EIP-1344]: https://eips.ethereum.org/EIPS/eip-1344
[EIP-2200]: https://eips.ethereum.org/EIPS/eip-2200
[EIP-2537]: https://eips.ethereum.org/EIPS/eip-2537
[EIP-2929]: https://eips.ethereum.org/EIPS/eip-2929
[EIP-2935]: https://eips.ethereum.org/EIPS/eip-2935
[EIP-3155]: https://eips.ethereum.org/EIPS/eip-3155
[EIP-3198]: https://eips.ethereum.org/EIPS/eip-3198
[EIP-3540]: https://eips.ethereum.org/EIPS/eip-3540
[EIP-3651]: https://eips.ethereum.org/EIPS/eip-3651
[EIP-3670]: https://eips.ethereum.org/EIPS/eip-3670
[EIP-3855]: https://eips.ethereum.org/EIPS/eip-3855
[EIP-3860]: https://eips.ethereum.org/EIPS/eip-3860
[EIP-4200]: https://eips.ethereum.org/EIPS/eip-4200
[EIP-4750]: https://eips.ethereum.org/EIPS/eip-4750
[EIP-4788]: https://eips.ethereum.org/EIPS/eip-4788
[EIP-4844]: https://eips.ethereum.org/EIPS/eip-4844
[EIP-4895]: https://eips.ethereum.org/EIPS/eip-4895
[EIP-5450]: https://eips.ethereum.org/EIPS/eip-5450
[EIP-5656]: https://eips.ethereum.org/EIPS/eip-5656
[EIP-6780]: https://eips.ethereum.org/EIPS/eip-6780
[EIP-6110]: https://eips.ethereum.org/EIPS/eip-6110
[EIP-7002]: https://eips.ethereum.org/EIPS/eip-7002
[EIP-7251]: https://eips.ethereum.org/EIPS/eip-7251
[EIP-7516]: https://eips.ethereum.org/EIPS/eip-7516
[EIP-7623]: https://eips.ethereum.org/EIPS/eip-7623
[EIP-7610]: https://eips.ethereum.org/EIPS/eip-7610
[EIP-7685]: https://eips.ethereum.org/EIPS/eip-7685
[EIP-7691]: https://eips.ethereum.org/EIPS/eip-7691
[EIP-7692]: https://eips.ethereum.org/EIPS/eip-7692
[EIP-7702]: https://eips.ethereum.org/EIPS/eip-7702

[Spurious Dragon]: https://eips.ethereum.org/EIPS/eip-607
[Petersburg]: https://eips.ethereum.org/EIPS/eip-1716
[Istanbul]: https://eips.ethereum.org/EIPS/eip-1679
[Berlin]: https://github.com/ethereum/execution-specs/blob/master/network-upgrades/mainnet-upgrades/berlin.md
[London]: https://github.com/ethereum/execution-specs/blob/master/network-upgrades/mainnet-upgrades/london.md
[Shanghai]: https://github.com/ethereum/execution-specs/blob/master/network-upgrades/mainnet-upgrades/shanghai.md
[Cancun]: https://github.com/ethereum/execution-specs/blob/master/network-upgrades/mainnet-upgrades/cancun.md
[Prague]: https://eips.ethereum.org/EIPS/eip-7600

[EVMC]: https://github.com/ethereum/evmc
[EVMC 12.1.0]: https://github.com/ethereum/evmc/releases/tag/v12.1.0
[EVMC 12.0.0]: https://github.com/ethereum/evmc/releases/tag/v12.0.0
[EVMC 11.0.1]: https://github.com/ethereum/evmc/releases/tag/v11.0.1
[EVMC 10.1.0]: https://github.com/ethereum/evmc/releases/tag/v10.1.0
[EVMC 10.0.0]: https://github.com/ethereum/evmc/releases/tag/v10.0.0
[EVMC 9.0.0]: https://github.com/ethereum/evmc/releases/tag/v9.0.0
[EVMC 8.0.0]: https://github.com/ethereum/evmc/releases/tag/v8.0.0
[EVMC 7.5.0]: https://github.com/ethereum/evmc/releases/tag/v7.5.0
[EVMC 7.4.0]: https://github.com/ethereum/evmc/releases/tag/v7.4.0
[EVMC 7.1.0]: https://github.com/ethereum/evmc/releases/tag/v7.1.0
[EVMC 7.0.0]: https://github.com/ethereum/evmc/releases/tag/v7.0.0

[intx]: https://github.com/chfast/intx
[intx 0.12.1]: https://github.com/chfast/intx/releases/tag/v0.12.1
[intx 0.11.0]: https://github.com/chfast/intx/releases/tag/v0.11.0
[intx 0.10.1]: https://github.com/chfast/intx/releases/tag/v0.10.1
[intx 0.10.0]: https://github.com/chfast/intx/releases/tag/v0.10.0
[intx 0.8.0]: https://github.com/chfast/intx/releases/tag/v0.8.0
[intx 0.6.0]: https://github.com/chfast/intx/releases/tag/v0.6.0
[intx 0.5.0]: https://github.com/chfast/intx/releases/tag/v0.5.0

[ethash]: https://github.com/chfast/ethash
[ethash 1.1.0]: https://github.com/chfast/ethash/releases/tag/v1.1.0
[ethash 1.0.0]: https://github.com/chfast/ethash/releases/tag/v1.0.0
[ethash 0.7.0]: https://github.com/chfast/ethash/releases/tag/v0.7.0

[ethereum/tests]: https://github.com/ethereum/tests
[Ethereum Execution Tests]: https://github.com/ethereum/tests
[tests 14.0]: https://github.com/ethereum/tests/releases/tag/v14.0
[tests 13]: https://github.com/ethereum/tests/releases/tag/v13
[tests 12.2]: https://github.com/ethereum/tests/releases/tag/v12.2
[tests 9.0.2]: https://github.com/ethereum/tests/releases/tag/9.0.2
[tests 8.0.4]: https://github.com/ethereum/tests/releases/tag/8.0.4

[Execution Spec Tests]: https://github.com/ethereum/execution-spec-tests
[Execution Spec Tests 3.0.0]: https://github.com/ethereum/execution-spec-tests/releases/tag/v3.0.0
[Execution Spec Tests 1.0.6]: https://github.com/ethereum/execution-spec-tests/releases/tag/v1.0.6

[Aleth]: https://github.com/ethereum/aleth
[Blockchain Tests]: https://ethereum-tests.readthedocs.io/en/latest/blockchain-ref.html
[evm-benchmarks]: https://github.com/ipsilon/evm-benchmarks
[EVMMAX]: https://github.com/ethereum/EIPs/pull/6601
[EOF]: https://notes.ethereum.org/@ipsilon/evm-object-format-overview
[EOF spec]: https://github.com/ipsilon/eof/blob/main/spec/eof.md
[goevmlab]: https://github.com/holiman/goevmlab
[retesteth]: https://github.com/ethereum/retesteth
[Silkworm]: https://github.com/torquem-ch/silkworm
[t8n]: https://ethereum-tests.readthedocs.io/en/develop/t8ntool-ref.html
[blst]: https://github.com/supranational/blst

[Keep a Changelog]: https://keepachangelog.com/en/1.1.0/
[Semantic Versioning]: https://semver.org/spec/v2.0.0.html

# evmone: Ethereum Virtual Machine
# Copyright 2023 Pawel Bylica.
# Licensed under the Apache License, Version 2.0. See the LICENSE file.

set(RISCV /usr/local/riscv)

set(CMAKE_SYSTEM_PROCESSOR riscv32)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_C_COMPILER ${RISCV}/bin/clang)
set(CMAKE_CXX_COMPILER ${RISCV}/bin/clang++)

set(CMAKE_CXX_FLAGS_INIT -stdlib=libc++)

set(CMAKE_FIND_ROOT_PATH ${RISCV}/sysroot)
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_CROSSCOMPILING_EMULATOR qemu-riscv32;-L;${CMAKE_FIND_ROOT_PATH})

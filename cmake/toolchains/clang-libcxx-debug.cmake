# evmone: Ethereum Virtual Machine
# Copyright 2024 The evmone Authors.
# Licensed under the Apache License, Version 2.0. See the LICENSE file.

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

set(CMAKE_CXX_FLAGS_INIT "-stdlib=libc++ -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG -D_LIBCPP_REMOVE_TRANSITIVE_INCLUDES=1")

// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

// Instantiates the benchmarked functions in the namespace jdp::JDP_MODE; included once by each
// jumpdest_pointer_mode_*.cpp, which the build compiles with that mode's code layout flags.

#include "jumpdest_pointer_loops.hpp"

#ifndef JDP_MODE
#error "JDP_MODE must name the mode"
#endif

#define JDP_STR2(...) #__VA_ARGS__
#define JDP_STR(...) JDP_STR2(__VA_ARGS__)

namespace jdp::JDP_MODE
{
namespace
{
#define JDP_VARIANT(NAME, K)                                                          \
    [[gnu::noinline]] void jdp_##NAME##_##K(BitsetSpan map, bytes_view code) noexcept \
    {                                                                                 \
        JDP_SHIFT(K);                                                                 \
        NAME##_loop(map, code);                                                       \
    }
#define JDP_VARIANTS(NAME) \
    JDP_VARIANT(NAME, 0)   \
    JDP_VARIANT(NAME, 16)  \
    JDP_VARIANT(NAME, 32)  \
    JDP_VARIANT(NAME, 48)

JDP_VARIANTS(index)
JDP_VARIANTS(zisk)
JDP_VARIANTS(ptr)
JDP_VARIANTS(szisk)
}  // namespace


#define JDP_ENTRY(NAME, K)         \
    Variant                        \
    {                              \
        #NAME, K, jdp_##NAME##_##K \
    }
#define JDP_ENTRIES(NAME) \
    JDP_ENTRY(NAME, 0), JDP_ENTRY(NAME, 16), JDP_ENTRY(NAME, 32), JDP_ENTRY(NAME, 48)
const Variant variants[NUM_VARIANTS] = {
    JDP_ENTRIES(index), JDP_ENTRIES(zisk), JDP_ENTRIES(ptr), JDP_ENTRIES(szisk)};

extern const Mode mode;
#ifdef JDP_FLAGS
const Mode mode{JDP_SUFFIX, JDP_STR(JDP_FLAGS), variants};
#else
const Mode mode{JDP_SUFFIX, nullptr, variants};
#endif
}  // namespace jdp::JDP_MODE

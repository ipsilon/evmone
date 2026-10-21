/* ethash: C/C++ implementation of Ethash, the Ethereum Proof of Work algorithm.
 * Copyright 2018-2019 Pawel Bylica.
 * Licensed under the Apache License, Version 2.0.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

union ethash_hash256
{
    uint64_t word64s[4];
    uint32_t word32s[8];
    uint8_t bytes[32];
    char str[32];
};

#ifdef __cplusplus
}

namespace ethash
{
using hash256 = ethash_hash256;
}

#endif

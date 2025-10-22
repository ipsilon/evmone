// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2022 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <test/state/hash_utils.hpp>
#include <test/state/mpt_hash.hpp>
#include <test/state/rlp.hpp>
#include <test/state/state.hpp>
#include <test/utils/utils.hpp>
#include <bit>

using namespace evmc::literals;
using namespace intx;
using namespace evmone;
using namespace evmone::test;
using namespace testing;

static constexpr auto emptyBytesHash =
    0xc5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470_bytes32;

TEST(state_rlp, empty_bytes_hash)
{
    EXPECT_EQ(keccak256({}), emptyBytesHash);
}

TEST(state_rlp, empty_list_hash)
{
    EXPECT_EQ(keccak256(bytes{0xc0}), EmptyListHash);  // Hash of empty RLP list: 0xc0.
    EXPECT_EQ(keccak256(rlp::encode(std::vector<uint64_t>{})), EmptyListHash);
}

TEST(state_rlp, empty_mpt_hash)
{
    const auto rlp_null = rlp::encode(0);
    EXPECT_EQ(rlp_null, bytes{0x80});
    EXPECT_EQ(keccak256(rlp_null), state::EMPTY_MPT_HASH);
}

TEST(state_rlp, encode_string_short)
{
    EXPECT_EQ(rlp::encode(0x01), "01"_hex);
    EXPECT_EQ(rlp::encode(0x31), "31"_hex);
    EXPECT_EQ(rlp::encode(0x7f), "7f"_hex);
}

TEST(state_rlp, encode_string_long)
{
    const auto buffer = std::make_unique<uint8_t[]>(0xffffff);

    const auto r1 = rlp::encode({buffer.get(), 0xaabb});
    EXPECT_EQ(r1.size(), 0xaabb + 3);
    EXPECT_EQ(hex({r1.data(), 10}), "b9aabb00000000000000");

    const auto r2 = rlp::encode({buffer.get(), 0xffff});
    EXPECT_EQ(r2.size(), 0xffff + 3);
    EXPECT_EQ(hex({r2.data(), 10}), "b9ffff00000000000000");

    const auto r3 = rlp::encode({buffer.get(), 0xaabbcc});
    EXPECT_EQ(r3.size(), 0xaabbcc + 4);
    EXPECT_EQ(hex({r3.data(), 10}), "baaabbcc000000000000");

    const auto r4 = rlp::encode({buffer.get(), 0xffffff});
    EXPECT_EQ(r4.size(), 0xffffff + 4);
    EXPECT_EQ(hex({r4.data(), 10}), "baffffff000000000000");
}

TEST(state_rlp, encode_vector)
{
    const auto x = 0xe1e2e3e4e5e6e7d0d1d2d3d4d5d6d7c0c1c2c3c4c5c6c7b0b1b2b3b4b5b6b7_u256;
    EXPECT_EQ(
        rlp::encode(x), "9fe1e2e3e4e5e6e7d0d1d2d3d4d5d6d7c0c1c2c3c4c5c6c7b0b1b2b3b4b5b6b7"_hex);
    const std::vector<uint256> v(0xffffff / 32, x);
    const auto r = rlp::encode(v);
    EXPECT_EQ(r.size(), v.size() * 32 + 4);
}

TEST(state_rlp, encode_account_with_balance)
{
    const auto expected =
        "f8 44"
        "80"
        "01"
        "a0 56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421"
        "a0 c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470"_hex;

    const auto r = rlp::encode_tuple(uint64_t{0}, 1_u256, state::EMPTY_MPT_HASH, emptyBytesHash);
    EXPECT_EQ(r, expected);
}

TEST(state_rlp, encode_storage_value)
{
    const auto value = 0x00000000000000000000000000000000000000000000000000000000000001ff_bytes32;
    const auto xvalue = rlp::encode(rlp::trim(value));
    EXPECT_EQ(xvalue, "8201ff"_hex);
}

TEST(state_rlp, encode_mpt_node)
{
    const auto path = "2041"_hex;
    const auto value = "765f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f31"_hex;
    const auto node = rlp::encode_tuple(path, value);
    EXPECT_EQ(node, "e18220419d765f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f31"_hex);
}

struct CustomStruct
{
    uint64_t a;
    bytes b;
};

static bytes rlp_encode(const CustomStruct& t)
{
    return rlp::encode_tuple(t.a, t.b);
}

TEST(state_rlp, encode_custom_struct)
{
    const CustomStruct t{1, {0x02, 0x03}};
    EXPECT_EQ(rlp::encode(t), "c4 01 820203"_hex);
}

TEST(state_rlp, encode_custom_struct_list)
{
    const std::vector<CustomStruct> v{{1, {0x02, 0x03}}, {4, {0x05, 0x06}}};
    EXPECT_EQ(rlp::encode(v), "ca c401820203 c404820506"_hex);
}

TEST(state_rlp, encode_uint64)
{
    EXPECT_EQ(rlp::encode(uint64_t{0}), "80"_hex);
    EXPECT_EQ(rlp::encode(uint64_t{1}), "01"_hex);
    EXPECT_EQ(rlp::encode(uint64_t{0x7f}), "7f"_hex);
    EXPECT_EQ(rlp::encode(uint64_t{0x80}), "8180"_hex);
    EXPECT_EQ(rlp::encode(uint64_t{0x81}), "8181"_hex);
    EXPECT_EQ(rlp::encode(uint64_t{0xff}), "81ff"_hex);
    EXPECT_EQ(rlp::encode(uint64_t{0x0100}), "820100"_hex);
    EXPECT_EQ(rlp::encode(uint64_t{0xffff}), "82ffff"_hex);
    EXPECT_EQ(rlp::encode(uint64_t{0x010000}), "83010000"_hex);
    EXPECT_EQ(rlp::encode(uint64_t{0xffffff}), "83ffffff"_hex);
    EXPECT_EQ(rlp::encode(uint64_t{0x01000000}), "8401000000"_hex);
    EXPECT_EQ(rlp::encode(uint64_t{0xffffffff}), "84ffffffff"_hex);
    EXPECT_EQ(rlp::encode(uint64_t{0x0100000000}), "850100000000"_hex);
    EXPECT_EQ(rlp::encode(uint64_t{0xffffffffff}), "85ffffffffff"_hex);
    EXPECT_EQ(rlp::encode(uint64_t{0x010000000000}), "86010000000000"_hex);
    EXPECT_EQ(rlp::encode(uint64_t{0xffffffffffff}), "86ffffffffffff"_hex);
    EXPECT_EQ(rlp::encode(uint64_t{0x01000000000000}), "8701000000000000"_hex);
    EXPECT_EQ(rlp::encode(uint64_t{0xffffffffffffff}), "87ffffffffffffff"_hex);
    EXPECT_EQ(rlp::encode(uint64_t{0x0100000000000000}), "880100000000000000"_hex);
    EXPECT_EQ(rlp::encode(uint64_t{0xffffffffffffffff}), "88ffffffffffffffff"_hex);
}

static bytes to_significant_be_bytes(uint64_t x)
{
    const auto byte_width = (std::bit_width(x) + 7) / 8;
    const auto leading_zero_bits = std::countl_zero(x) & ~7;  // Leading bits rounded down to 8x.
    const auto trimmed_x = x << leading_zero_bits;            // Significant bytes moved to the top.

    uint8_t b[sizeof(x)];
    intx::be::store(b, trimmed_x);
    return bytes{b, static_cast<size_t>(byte_width)};
}

/// The "custom" implementation of RLP encoding of uint64. It trims leading zero bytes and
/// manually constructs bytes with variadic-length encoding.
static bytes rlp_encode_uint64(uint64_t x)
{
    static constexpr uint8_t ShortBase = 0x80;
    if (x < ShortBase)  // Single-byte encoding.
        return bytes{(x == 0) ? ShortBase : static_cast<uint8_t>(x)};

    const auto b = to_significant_be_bytes(x);
    return static_cast<uint8_t>(ShortBase + b.size()) + b;
}

TEST(state_rlp, encode_uint64_custom)
{
    EXPECT_EQ(rlp_encode_uint64(uint64_t{0}), "80"_hex);
    EXPECT_EQ(rlp_encode_uint64(uint64_t{1}), "01"_hex);
    EXPECT_EQ(rlp_encode_uint64(uint64_t{0x7f}), "7f"_hex);
    EXPECT_EQ(rlp_encode_uint64(uint64_t{0x80}), "8180"_hex);
    EXPECT_EQ(rlp_encode_uint64(uint64_t{0x81}), "8181"_hex);
    EXPECT_EQ(rlp_encode_uint64(uint64_t{0xff}), "81ff"_hex);
    EXPECT_EQ(rlp_encode_uint64(uint64_t{0x0100}), "820100"_hex);
    EXPECT_EQ(rlp_encode_uint64(uint64_t{0xffff}), "82ffff"_hex);
    EXPECT_EQ(rlp_encode_uint64(uint64_t{0x010000}), "83010000"_hex);
    EXPECT_EQ(rlp_encode_uint64(uint64_t{0xffffff}), "83ffffff"_hex);
    EXPECT_EQ(rlp_encode_uint64(uint64_t{0x01000000}), "8401000000"_hex);
    EXPECT_EQ(rlp_encode_uint64(uint64_t{0xffffffff}), "84ffffffff"_hex);
    EXPECT_EQ(rlp_encode_uint64(uint64_t{0x0100000000}), "850100000000"_hex);
    EXPECT_EQ(rlp_encode_uint64(uint64_t{0xffffffffff}), "85ffffffffff"_hex);
    EXPECT_EQ(rlp_encode_uint64(uint64_t{0x010000000000}), "86010000000000"_hex);
    EXPECT_EQ(rlp_encode_uint64(uint64_t{0xffffffffffff}), "86ffffffffffff"_hex);
    EXPECT_EQ(rlp_encode_uint64(uint64_t{0x01000000000000}), "8701000000000000"_hex);
    EXPECT_EQ(rlp_encode_uint64(uint64_t{0xffffffffffffff}), "87ffffffffffffff"_hex);
    EXPECT_EQ(rlp_encode_uint64(uint64_t{0x0100000000000000}), "880100000000000000"_hex);
    EXPECT_EQ(rlp_encode_uint64(uint64_t{0xffffffffffffffff}), "88ffffffffffffffff"_hex);
}

TEST(state_rlp, tx_to_rlp_legacy)
{
    // Example from
    // https://eips.ethereum.org/EIPS/eip-155

    state::Transaction tx{};
    tx.type = evmone::state::Transaction::Type::legacy;
    tx.data = ""_b;
    tx.gas_limit = 21000;
    tx.max_gas_price = 20000000000;
    tx.max_priority_gas_price = 20000000000;
    tx.sender = 0x0000000000000000000000000000000000000000_address;
    tx.to = 0x3535353535353535353535353535353535353535_address;
    tx.value = 1000000000000000000_u256;
    tx.access_list = {};
    tx.nonce = 9;
    tx.r = {};
    tx.s = {};
    tx.v = 1;
    tx.chain_id = 1;

    const auto rlp_rep = rlp::encode(tx);
    EXPECT_EQ(rlp_rep,
        "ec"
        "09"
        "8504a817c800"
        "825208"
        "943535353535353535353535353535353535353535"
        "880de0b6b3a7640000"
        "80"
        "01"
        "80"
        "80"_hex);
}

TEST(state_rlp, tx_to_rlp_legacy_with_data)
{
    // Example from
    // https://etherscan.io/tx/0x033e9f8db737193d4666911a164e218d58d80edc64f4ed393d0c48c1ce2673e7

    state::Transaction tx{};
    tx.type = evmone::state::Transaction::Type::legacy;
    tx.data = "0xa0712d680000000000000000000000000000000000000000000000000000000000000003"_hex;
    tx.gas_limit = 421566;
    tx.max_gas_price = 14829580649;
    tx.max_priority_gas_price = 14829580649;
    tx.sender = 0xc9d955665d6f90ef483a1ac0bd2443c17a550db7_address;
    tx.to = 0x963eda46936b489f4a0d153c20e47653d8bbf222_address;
    tx.value = 480000000000000000_u256;
    tx.access_list = {};
    tx.nonce = 0;
    tx.r = 0x3bcaa4f1603d2b3ebe6126f57e0ddefc6c6c58d8bbef7f3b29e14a915bf1828d_u256;
    tx.s = 0x00f37b7a0b6007ef4335a35198485e443051d45b42fea8bacc054721ecccdb5f_u256;
    tx.v = 27;
    tx.chain_id = 1;

    const auto rlp_rep = rlp::encode(tx);
    EXPECT_EQ(rlp_rep,
        "f890"
        "80"
        "850373e97169"
        "83066ebe"
        "94963eda46936b489f4a0d153c20e47653d8bbf222"
        "8806a94d74f4300000"
        "a4a0712d680000000000000000000000000000000000000000000000000000000000000003"
        "1b"
        "a03bcaa4f1603d2b3ebe6126f57e0ddefc6c6c58d8bbef7f3b29e14a915bf1828d"
        "9ff37b7a0b6007ef4335a35198485e443051d45b42fea8bacc054721ecccdb5f"_hex);

    EXPECT_EQ(keccak256(rlp_rep),
        0x033e9f8db737193d4666911a164e218d58d80edc64f4ed393d0c48c1ce2673e7_bytes32);
}

TEST(state_rlp, tx_to_rlp_eip1559)
{
    // Example from
    // https://etherscan.io/tx/0xee8d0f04073a6792b1bd6b1cb0b88cb57984905979d2668f84b9c3dcb8894da6

    state::Transaction tx{};

    tx.type = evmone::state::Transaction::Type::eip1559;
    tx.data = ""_b;
    tx.gas_limit = 30000;
    tx.max_gas_price = 14237787676;
    tx.max_priority_gas_price = 0;
    tx.sender = 0x95222290dd7278aa3ddd389cc1e1d165cc4bafe5_address;
    tx.to = 0x535b918f3724001fd6fb52fcc6cbc220592990a3_address;
    tx.value = 73360267083380739_u256;
    tx.access_list = {};
    tx.nonce = 132949;
    tx.r = 0x2fe690e16de3534bee626150596573d57cb56d0c2e48a02f64c0a03c1636ce2a_u256;
    tx.s = 0x4814f3dc7dac2ee153a2456aa3968717af7400972167dfb00b1cce1c23b6dd9f_u256;
    tx.v = 1;
    tx.chain_id = 1;

    const auto rlp_rep = rlp::encode(tx);
    EXPECT_EQ(rlp_rep,
        "02"
        "f872"
        "01"
        "83020755"
        "80"
        "850350a3661c"
        "827530"
        "94535b918f3724001fd6fb52fcc6cbc220592990a3"
        "880104a0c63421f803"
        "80"
        "c0"
        "01"
        "a02fe690e16de3534bee626150596573d57cb56d0c2e48a02f64c0a03c1636ce2a"
        "a04814f3dc7dac2ee153a2456aa3968717af7400972167dfb00b1cce1c23b6dd9f"_hex);

    EXPECT_EQ(keccak256(rlp_rep),
        0xee8d0f04073a6792b1bd6b1cb0b88cb57984905979d2668f84b9c3dcb8894da6_bytes32);
}

TEST(state_rlp, tx_to_rlp_eip1559_with_data)
{
    // Example taken from
    // https://etherscan.io/tx/0xf9400dd4722908fa7b8d514429aebfd4cd04aaa9faaf044554d2f550422baef9

    state::Transaction tx{};
    tx.type = evmone::state::Transaction::Type::eip1559;
    tx.data =
        "095ea7b3"
        "0000000000000000000000001111111254eeb25477b68fb85ed929f73a960582"
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"_hex;
    tx.gas_limit = 53319;
    tx.max_gas_price = 14358031378;
    tx.max_priority_gas_price = 576312105;
    tx.sender = 0xb24df1ff03fa211458fbd855d08b3d21704bdf2d_address;
    tx.to = 0xc02aaa39b223fe8d0a0e5c4f27ead9083c756cc2_address;
    tx.value = 0;
    tx.access_list = {};
    tx.nonce = 47;
    tx.r = 0x67d25d27169ab09afb516849b85ae96d51e1dfc0853257b2b7401a73cef2b08b_u256;
    tx.s = 0x3d8162a0f285284e02ed4ff387435c2742235a0534964f9b1415d4d10f28ce06_u256;
    tx.v = 1;
    tx.chain_id = 1;

    const auto rlp_rep = rlp::encode(tx);
    EXPECT_EQ(rlp_rep,
        "02"
        "f8b0"
        "012f"
        "842259d329"
        "850357ce2c12"
        "82d047"
        "94c02aaa39b223fe8d0a0e5c4f27ead9083c756cc2"
        "80"
        "b844095ea7b30000000000000000000000001111111254eeb25477b68fb85ed929f73a9"
        "60582ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
        "c0"
        "01a067d25d27169ab09afb516849b85ae96d51e1dfc0853257b2b7401a73cef2b08b"
        "a03d8162a0f285284e02ed4ff387435c2742235a0534964f9b1415d4d10f28ce06"_hex);

    EXPECT_EQ(keccak256(rlp_rep),
        0xf9400dd4722908fa7b8d514429aebfd4cd04aaa9faaf044554d2f550422baef9_bytes32);
}

TEST(state_rlp, tx_to_rlp_eip1559_invalid_v_value)
{
    state::Transaction tx{};
    tx.type = evmone::state::Transaction::Type::eip1559;
    tx.chain_id = 1;
    tx.nonce = 2;
    tx.max_priority_gas_price = 3;
    tx.max_gas_price = 4;
    tx.gas_limit = 5;
    tx.to = {};
    tx.value = 6;
    tx.data = "da"_hex;
    tx.access_list = {};
    tx.v = 7;
    tx.r = 8_u256;
    tx.s = 9_u256;

    const auto rlp_rep = rlp::encode(tx);
    EXPECT_EQ(hex(rlp_rep),
        "02"    // tx_type
        "cd"    // rlp list start
        "01"    // chain_id
        "02"    // nonce
        "03"    // max_priority_fee_per_gas
        "04"    // max_fee_per_gas
        "05"    // gas_limit
        "80"    // to
        "06"    // value
        "81da"  // data
        "c0"    // access_list
        "07"    // sig_parity
        "08"    // sig_r
        "09"    // sig_s
    );
}

TEST(state_rlp, tx_to_rlp_eip2930_invalid_v_value)
{
    state::Transaction tx{};
    tx.type = evmone::state::Transaction::Type::access_list;
    tx.chain_id = 1;
    tx.nonce = 2;
    tx.max_gas_price = 3;
    tx.gas_limit = 4;
    tx.to = {};
    tx.value = 5;
    tx.data = "da"_hex;
    tx.access_list = {};
    tx.v = 6;
    tx.r = 7_u256;
    tx.s = 8_u256;

    const auto rlp_rep = rlp::encode(tx);
    EXPECT_EQ(hex(rlp_rep),
        "01"    // tx_type
        "cc"    // rlp list start
        "01"    // chain_id
        "02"    // nonce
        "03"    // max_fee_per_gas
        "04"    // gas_limit
        "80"    // to
        "05"    // value
        "81da"  // data
        "c0"    // access_list
        "06"    // sig_parity
        "07"    // sig_r
        "08"    // sig_s
    );
}

TEST(state_rlp, tx_to_rlp_eip1559_with_non_empty_access_list)
{
    state::Transaction tx{};
    tx.type = evmone::state::Transaction::Type::eip1559;
    tx.data = "00"_hex;
    tx.gas_limit = 0x3d0900;
    tx.max_gas_price = 0x7d0;
    tx.max_priority_gas_price = 0xa;
    tx.sender = 0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b_address;
    tx.to = 0xcccccccccccccccccccccccccccccccccccccccc_address;
    tx.value = 0;
    tx.access_list = {{0xcccccccccccccccccccccccccccccccccccccccc_address,
        {0x0000000000000000000000000000000000000000000000000000000000000000_bytes32,
            0x0000000000000000000000000000000000000000000000000000000000000001_bytes32}}};
    tx.nonce = 1;
    tx.r = 0xd671815898b8dd34321adbba4cb6a57baa7017323c26946f3719b00e70c755c2_u256;
    tx.s = 0x3528b9efe3be57ea65a933d1e6bbf3b7d0c78830138883c1201e0c641fee6464_u256;
    tx.v = 0;
    tx.chain_id = 1;

    EXPECT_EQ(keccak256(rlp::encode(tx)),
        0xfb18421827800adcf465688e303cc9863045fdb96971473a114677916a3a08a4_bytes32);
}

TEST(state_rlp, tx_to_rlp_eip2930_with_non_empty_access_list)
{
    // https://etherscan.io/tx/0xf076e75aa935552e20e5d9fd4d1dda4ff33399ff3d6ac22843ae646f82c385d4

    state::Transaction tx{};
    tx.type = evmone::state::Transaction::Type::access_list;
    tx.data =
        "0x095ea7b3000000000000000000000000f17d23136b4fead139f54fb766c8795faae09660ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"_hex;
    tx.gas_limit = 51253;
    tx.max_gas_price = 15650965396;
    tx.max_priority_gas_price = 15650965396;
    tx.sender = 0xcb0b99284784d9e400b1020b01fc40ff193d3540_address;
    tx.to = 0x9232a548dd9e81bac65500b5e0d918f8ba93675c_address;
    tx.value = 0;
    tx.access_list = {{0x9232a548dd9e81bac65500b5e0d918f8ba93675c_address,
        {0x8e947fe742892ee6fffe7cfc013acac35d33a3892c58597344bed88b21eb1d2f_bytes32}}};
    tx.nonce = 62;
    tx.r = 0x2cfaa5ffa42172bfa9f83207a257c53ba3a106844ee58e9131466f655ecc11e9_u256;
    tx.s = 0x419366dadd905a16cd433f2953f9ed976560822bb2611ac192b939f7b9c2a98c_u256;
    tx.v = 1;
    tx.chain_id = 1;

    EXPECT_EQ(keccak256(rlp::encode(tx)),
        0xf076e75aa935552e20e5d9fd4d1dda4ff33399ff3d6ac22843ae646f82c385d4_bytes32);
}

TEST(state_rlp, tx_to_rlp_blob)
{
    state::Transaction tx{};

    tx.type = evmone::state::Transaction::Type::blob;
    tx.data = ""_b;
    tx.gas_limit = 30000;
    tx.max_gas_price = 14237787676;
    tx.max_priority_gas_price = 0;
    tx.sender = 0x95222290dd7278aa3ddd389cc1e1d165cc4bafe5_address;
    tx.to = 0x535b918f3724001fd6fb52fcc6cbc220592990a3_address;
    tx.value = 73360267083380739_u256;
    tx.access_list = {};
    tx.nonce = 132949;
    tx.r = 0x2fe690e16de3534bee626150596573d57cb56d0c2e48a02f64c0a03c1636ce2a_u256;
    tx.s = 0x4814f3dc7dac2ee153a2456aa3968717af7400972167dfb00b1cce1c23b6dd9f_u256;
    tx.v = 1;
    tx.chain_id = 1;
    tx.max_blob_gas_price = 4;
    tx.blob_hashes = {0x0111111111111111111111111111111111111111111111111111111111111111_bytes32,
        0x0122222222222222222222222222222222222222222222222222222222222222_bytes32};

    const auto rlp_rep = rlp::encode(tx);
    EXPECT_EQ(rlp_rep,
        "03"
        "f8b7"
        "01"
        "83020755"
        "80"
        "850350a3661c"
        "827530"
        "94535b918f3724001fd6fb52fcc6cbc220592990a3"
        "880104a0c63421f803"
        "80"
        "c0"
        "04"
        "f842"
        "a00111111111111111111111111111111111111111111111111111111111111111"
        "a00122222222222222222222222222222222222222222222222222222222222222"
        "01"
        "a02fe690e16de3534bee626150596573d57cb56d0c2e48a02f64c0a03c1636ce2a"
        "a04814f3dc7dac2ee153a2456aa3968717af7400972167dfb00b1cce1c23b6dd9f"_hex);
}

TEST(state_rlp, tx_to_rlp_blob_invalid_v_value)
{
    state::Transaction tx{};
    tx.type = evmone::state::Transaction::Type::blob;
    tx.chain_id = 1;
    tx.nonce = 2;
    tx.max_priority_gas_price = 3;
    tx.max_gas_price = 4;
    tx.gas_limit = 5;
    tx.to = 0x00_address;
    tx.value = 6;
    tx.data = "da"_hex;
    tx.access_list = {};
    tx.max_blob_gas_price = 7;
    tx.blob_hashes = {};
    tx.v = 8;
    tx.r = 9_u256;
    tx.s = 0xa_u256;

    const auto rlp_rep = rlp::encode(tx);
    EXPECT_EQ(hex(rlp_rep),
        "03"                                          // tx_type
        "e3"                                          // rlp list start
        "01"                                          // chain_id
        "02"                                          // nonce
        "03"                                          // max_priority_fee_per_gas
        "04"                                          // max_fee_per_gas
        "05"                                          // gas_limit
        "940000000000000000000000000000000000000000"  // to
        "06"                                          // value
        "81da"                                        // data
        "c0"                                          // access_list
        "07"                                          // max_fee_per_blob_gas
        "c0"                                          // blob_versioned_hashes
        "08"                                          // sig_parity
        "09"                                          // sig_r
        "0a"                                          // sig_s
    );
}

TEST(state_rlp, tx_to_rlp_blob_invalid_to_value)
{
    state::Transaction tx{};
    tx.type = evmone::state::Transaction::Type::blob;
    tx.chain_id = 1;
    tx.nonce = 2;
    tx.max_priority_gas_price = 3;
    tx.max_gas_price = 4;
    tx.gas_limit = 5;
    tx.to = {};
    tx.value = 6;
    tx.data = "da"_hex;
    tx.access_list = {};
    tx.max_blob_gas_price = 7;
    tx.blob_hashes = {0x0b_bytes32};
    tx.v = 1;
    tx.r = 9_u256;
    tx.s = 0xa_u256;

    const auto rlp_rep = rlp::encode(tx);
    EXPECT_EQ(hex(rlp_rep),
        "03"    // tx_type
        "f0"    // rlp list start
        "01"    // chain_id
        "02"    // nonce
        "03"    // max_priority_fee_per_gas
        "04"    // max_fee_per_gas
        "05"    // gas_limit
        "80"    // to
        "06"    // value
        "81da"  // data
        "c0"    // access_list
        "07"    // max_fee_per_blob_gas
        "e1a0000000000000000000000000000000000000000000000000000000000000000b"  // blob_versioned_hashes
        "01"                                                                    // sig_parity
        "09"                                                                    // sig_r
        "0a"                                                                    // sig_s
    );
}

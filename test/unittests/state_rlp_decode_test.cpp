// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <test/state/rlp_decode.hpp>
#include <test/state/transaction.hpp>
#include <test/utils/rlp.hpp>
#include <test/utils/rlp_encode.hpp>
#include <test/utils/utils.hpp>

using namespace evmc::literals;
using namespace intx;
using namespace evmone;
using namespace evmone::test;

namespace
{
/// A minimal, decodable pre-EIP-155 legacy transaction; the base for the field-mutation rejection
/// tests below.
state::Transaction minimal_legacy_tx()
{
    state::Transaction tx{};
    tx.type = state::Transaction::Type::legacy;
    tx.max_gas_price = 1;
    tx.gas_limit = 21000;
    tx.value = 0;
    tx.r = 1_u256;
    tx.s = 2_u256;
    tx.v = 27;
    return tx;
}

/// Encodes @p tx and decodes it back; the transaction must decode.
state::Transaction round_trip(const state::Transaction& tx)
{
    auto decoded = state::decode_transaction(rlp::encode(tx));
    EXPECT_TRUE(decoded.has_value());
    return decoded.value_or(state::Transaction{});
}

/// Compares all decoded fields of two transactions (sender is not recovered by the decoder).
void expect_tx_eq(const state::Transaction& expected, const state::Transaction& actual)
{
    EXPECT_EQ(actual.type, expected.type);
    EXPECT_EQ(actual.chain_id, expected.chain_id);
    EXPECT_EQ(actual.nonce, expected.nonce);
    EXPECT_EQ(actual.max_priority_gas_price, expected.max_priority_gas_price);
    EXPECT_EQ(actual.max_gas_price, expected.max_gas_price);
    EXPECT_EQ(actual.gas_limit, expected.gas_limit);
    EXPECT_EQ(actual.to, expected.to);
    EXPECT_EQ(actual.value, expected.value);
    EXPECT_EQ(actual.data, expected.data);
    EXPECT_EQ(actual.access_list, expected.access_list);
    EXPECT_EQ(actual.max_blob_gas_price, expected.max_blob_gas_price);
    EXPECT_EQ(actual.blob_hashes, expected.blob_hashes);
    EXPECT_EQ(actual.v, expected.v);
    EXPECT_EQ(actual.r, expected.r);
    EXPECT_EQ(actual.s, expected.s);

    ASSERT_EQ(actual.authorization_list.size(), expected.authorization_list.size());
    for (size_t i = 0; i < expected.authorization_list.size(); ++i)
    {
        const auto& e = expected.authorization_list[i];
        const auto& a = actual.authorization_list[i];
        EXPECT_EQ(a.chain_id, e.chain_id);
        EXPECT_EQ(a.addr, e.addr);
        EXPECT_EQ(a.nonce, e.nonce);
        EXPECT_EQ(a.r, e.r);
        EXPECT_EQ(a.s, e.s);
        EXPECT_EQ(a.v, e.v);
    }
}
}  // namespace

TEST(state_rlp_decode, tx_round_trip)
{
    // decode(encode(tx)) reproduces each typed transaction. Every tx is in "decoded normal form"
    // (typed y_parity in {0, 1}; access-list max_priority mirrors the single gas price), so the
    // decoded transaction must equal the input. Legacy is asymmetric and covered separately.
    using enum state::Transaction::Type;
    const auto to = 0x9232a548dd9e81bac65500b5e0d918f8ba93675c_address;
    const state::AccessList example_access_list{
        {to, {0x8e947fe742892ee6fffe7cfc013acac35d33a3892c58597344bed88b21eb1d2f_bytes32}}};

    std::vector<std::pair<const char*, state::Transaction>> cases;

    {
        state::Transaction tx{};
        tx.type = access_list;  // EIP-2930.
        tx.chain_id = 1;
        tx.nonce = 62;
        tx.max_priority_gas_price = 0x64;  // Mirrors max_gas_price for a single-gas-price tx.
        tx.max_gas_price = 0x64;
        tx.gas_limit = 0xc835;
        tx.to = to;
        tx.data = "0x095ea7b3"_hex;
        tx.access_list = example_access_list;
        tx.r = 0x2c_u256;
        tx.s = 0x41_u256;
        tx.v = 1;
        cases.emplace_back("access_list", tx);
    }
    {
        state::Transaction tx{};
        tx.type = eip1559;
        tx.chain_id = 1;
        tx.nonce = 42;
        tx.max_priority_gas_price = 0x0a;
        tx.max_gas_price = 0x64;
        tx.gas_limit = 0x9c40;
        tx.to = to;
        tx.value = 0x0de0b6b3a7640000_u256;
        tx.data = "0x095ea7b3"_hex;
        tx.access_list = example_access_list;
        tx.r = 0x2c_u256;
        tx.s = 0x41_u256;
        tx.v = 1;
        cases.emplace_back("eip1559", tx);
    }
    {
        state::Transaction tx{};
        tx.type = blob;
        tx.chain_id = 1;
        tx.nonce = 5;
        tx.max_gas_price = 0x64;
        tx.gas_limit = 0x7530;
        tx.to = 0x535b918f3724001fd6fb52fcc6cbc220592990a3_address;
        tx.value = 7_u256;
        tx.max_blob_gas_price = 4;
        tx.blob_hashes = {
            0x0111111111111111111111111111111111111111111111111111111111111111_bytes32,
            0x0122222222222222222222222222222222222222222222222222222222222222_bytes32};
        tx.r = 9_u256;
        tx.s = 0xa_u256;
        tx.v = 1;
        cases.emplace_back("blob", tx);
    }
    {
        state::Transaction tx{};
        tx.type = set_code;  // EIP-7702; auth y_parity may be any value < 2**8.
        tx.chain_id = 1;
        tx.max_gas_price = 7;
        tx.gas_limit = 0x186a0;
        tx.to = 0x1111_address;
        tx.authorization_list = {
            {.chain_id = 1_u256,
                .addr = 0x2222_address,
                .nonce = 0,
                .r = 0x1234_u256,
                .s = 0x5678_u256,
                .v = 2_u256},
            {.chain_id = 1_u256,
                .addr = 0x3333_address,
                .nonce = 7,
                .r = 0x9abc_u256,
                .s = 0xdef0_u256,
                .v = 27_u256},
            {.chain_id = 1_u256,
                .addr = 0x4444_address,
                .nonce = 42,
                .r = 0xaaaa_u256,
                .s = 0xbbbb_u256,
                .v = 0xff_u256},
        };
        tx.r = 1_u256;
        tx.s = 2_u256;
        tx.v = 0;
        cases.emplace_back("set_code", tx);
    }

    for (const auto& [name, tx] : cases)
    {
        SCOPED_TRACE(name);
        expect_tx_eq(tx, round_trip(tx));
    }
}

TEST(state_rlp_decode, tx_round_trip_legacy)
{
    // The legacy encoder writes v verbatim while the decoder splits it into (chain_id, y_parity)
    // and mirrors the single gas price into max_priority_gas_price; so the decoded form differs
    // from the input in exactly those fields.
    // EIP-155: v = 35 + 2 * chain_id + parity. For chain 1: v = 37 (parity 0) or 38 (parity 1).
    for (const auto& [raw_v, parity] : {std::pair{37u, 0u}, std::pair{38u, 1u}})
    {
        SCOPED_TRACE(raw_v);
        state::Transaction in{};
        in.type = state::Transaction::Type::legacy;
        in.nonce = 7;
        in.max_gas_price = 0x0102;
        in.gas_limit = 0x5208;
        in.to = 0x9232a548dd9e81bac65500b5e0d918f8ba93675c_address;
        in.value = 0xabcdef_u256;
        in.data = "0xdeadbeef"_hex;
        in.r = 0x1111_u256;
        in.s = 0x2222_u256;
        in.v = raw_v;

        auto expected = in;
        expected.max_priority_gas_price = in.max_gas_price;
        expected.chain_id = 1;
        expected.v = parity;
        expect_tx_eq(expected, round_trip(in));
    }

    // Pre-EIP-155: v in {27, 28}, no chain id, CREATE.
    for (const auto& [raw_v, parity] : {std::pair{27u, 0u}, std::pair{28u, 1u}})
    {
        SCOPED_TRACE(raw_v);
        auto in = minimal_legacy_tx();
        in.v = raw_v;

        auto expected = in;
        expected.max_priority_gas_price = in.max_gas_price;
        expected.chain_id = 0;
        expected.v = parity;
        expect_tx_eq(expected, round_trip(in));
    }
}

TEST(state_rlp_decode, tx_set_code_auth_y_parity_overflow_rejected)
{
    // EIP-7702 bounds y_parity to < 2**8; a value of 2**8 fails the whole transaction at decode
    // time, matching geth (V uint8) and revm/alloy (y_parity: U8).
    auto tx = minimal_legacy_tx();
    tx.type = state::Transaction::Type::set_code;
    tx.chain_id = 1;
    tx.max_gas_price = 7;
    tx.gas_limit = 0x186a0;
    tx.to = 0x1111_address;
    tx.authorization_list = {{.chain_id = 1_u256,
        .addr = 0x2222_address,
        .nonce = 0,
        .r = 0x1234_u256,
        .s = 0x5678_u256,
        .v = 0x100_u256}};
    tx.v = 0;
    EXPECT_FALSE(state::decode_transaction(rlp::encode(tx)).has_value());
}

TEST(state_rlp_decode, tx_rejects_trailing_data)
{
    // Both the legacy and the typed envelope must reject bytes after the transaction.
    EXPECT_FALSE(
        state::decode_transaction(rlp::encode(minimal_legacy_tx()) + "00"_hex).has_value());

    auto typed = minimal_legacy_tx();
    typed.type = state::Transaction::Type::eip1559;
    typed.chain_id = 1;
    typed.v = 0;  // Typed y_parity must be in {0, 1}.
    EXPECT_FALSE(state::decode_transaction(rlp::encode(typed) + "00"_hex).has_value());
}

TEST(state_rlp_decode, tx_rejects_under_declared_list_length)
{
    // A list-length prefix that declares fewer bytes than the fields that follow must be rejected;
    // the decoder must honor the declared list boundary, not read past it.
    auto rlp = rlp::encode(minimal_legacy_tx());
    ASSERT_GE(rlp[0], 0xc0);  // Short RLP list.
    ASSERT_LT(rlp[0], 0xf8);
    rlp[0] = static_cast<uint8_t>(rlp[0] - 1);  // Declare one byte less than the payload.
    EXPECT_FALSE(state::decode_transaction(rlp).has_value());
}

// Regression: a long-form length near 2**64 must be rejected. The naive check
// `payload_length + length_of_length >= input_len` overflows uint64 and would wrongly accept it.
TEST(state_rlp_decode, header_rejects_long_list_length_overflow)
{
    const bytes input(9, uint8_t{0xff});  // 0xff prefix + eight 0xff length bytes => len ==
                                          // 2**64-1.
    bytes_view v = input;
    rlp::Header h;
    EXPECT_FALSE(rlp::decode_header(v, h));
}

TEST(state_rlp_decode, header_rejects_long_string_length_overflow)
{
    bytes input{uint8_t{0xbf}};      // long string with an 8-byte length...
    input.append(8, uint8_t{0xff});  // ...of 2**64-1.
    bytes_view v = input;
    rlp::Header h;
    EXPECT_FALSE(rlp::decode_header(v, h));
}

// Regression: canonical long-form length encoding must not have a leading zero byte.
TEST(state_rlp_decode, header_rejects_noncanonical_length)
{
    const bytes input{uint8_t{0xb8}, uint8_t{0x00}};  // long string, length byte == 0.
    bytes_view v = input;
    rlp::Header h;
    EXPECT_FALSE(rlp::decode_header(v, h));
}

// Regression: the long form is reserved for payloads longer than the short-form maximum (55).
TEST(state_rlp_decode, header_rejects_noncanonical_long_form_short)
{
    bytes input{uint8_t{0xb8}, uint8_t{0x05}};  // long string declaring a 5-byte payload...
    input.append(5, uint8_t{0x01});             // ...which must use the short form instead.
    bytes_view v = input;
    rlp::Header h;
    EXPECT_FALSE(rlp::decode_header(v, h));
}

// A byte below 0x80 must be its own encoding, not a 1-byte string (0x81 0x7f).
TEST(state_rlp_decode, header_rejects_noncanonical_single_byte)
{
    const bytes in = "0x817f"_hex;
    bytes_view v = in;
    rlp::Header h;
    EXPECT_FALSE(rlp::decode_header(v, h));
}

TEST(state_rlp_decode, header_boundaries)
{
    rlp::Header h;
    const auto rejected = [&h](bytes_view in) {
        bytes_view v = in;
        return !rlp::decode_header(v, h);
    };
    // A declared length must fit the available input.
    EXPECT_TRUE(rejected("0x8301"_hex));  // short string declares 3 bytes, 1 present
    EXPECT_TRUE(rejected("0xb901"_hex));  // long-string length header truncated
    EXPECT_TRUE(rejected("0xc301"_hex));  // short list declares 3 bytes, 1 present
    EXPECT_TRUE(rejected("0xf901"_hex));  // long-list length header truncated
    // Long-form list canonicality (the string variants are checked in the tests above).
    EXPECT_TRUE(rejected("0xf800"_hex));            // leading-zero length byte
    EXPECT_TRUE(rejected("0xf8050101010101"_hex));  // long form used for a <= 55-byte payload

    // A valid long string (payload longer than the short-form maximum) decodes.
    bytes long_str = "0xb838"_hex;  // long string, one length byte 0x38 == 56.
    long_str.append(56, uint8_t{0x11});
    bytes_view v = long_str;
    ASSERT_TRUE(rlp::decode_header(v, h));
    EXPECT_FALSE(h.is_list);
    EXPECT_EQ(h.payload_length, 56u);
}

// Regression: a fixed-width field (address/hash/storage key) must be encoded as exactly N bytes;
// a shorter string was silently zero-padded and accepted.
TEST(state_rlp_decode, fixed_width_requires_exact_length)
{
    evmc::bytes32 out;
    {
        bytes in{uint8_t{0xa0}};  // 32-byte string (exact).
        in.append(32, uint8_t{0x11});
        bytes_view v = in;
        EXPECT_TRUE(rlp::decode(v, out));
        EXPECT_TRUE(v.empty());
    }
    {
        const bytes in{uint8_t{0x82}, uint8_t{0xaa}, uint8_t{0xbb}};  // 2-byte string: too short.
        bytes_view v = in;
        EXPECT_FALSE(rlp::decode(v, out));
    }
    {
        bytes in{uint8_t{0xa1}};  // 33-byte string: too long.
        in.append(33, uint8_t{0x11});
        bytes_view v = in;
        EXPECT_FALSE(rlp::decode(v, out));
    }
    {
        const bytes in = "0xc0"_hex;  // A list where a fixed-width string is expected.
        bytes_view v = in;
        EXPECT_FALSE(rlp::decode(v, out));
    }
}

// A scalar field must be a canonical, width-bounded string, not a leading-zero integer, an
// oversized one, or a list.
TEST(state_rlp_decode, integer_rejects_malformed)
{
    uint64_t u = 0;
    {
        const bytes in = "0x820005"_hex;  // Leading zero byte.
        bytes_view v = in;
        EXPECT_FALSE(rlp::decode(v, u));
    }
    {
        const bytes in = "0x89010000000000000000"_hex;  // 9-byte payload: wider than uint64_t.
        bytes_view v = in;
        EXPECT_FALSE(rlp::decode(v, u));
    }
    {
        const bytes in = "0xc0"_hex;  // A list where a scalar is expected.
        bytes_view v = in;
        EXPECT_FALSE(rlp::decode(v, u));
    }
    {
        bytes_view v;  // Empty input: no header to decode.
        EXPECT_FALSE(rlp::decode(v, u));
    }
}

// The access-list entry [address, [storage keys]] pair must be a two-element list.
TEST(state_rlp_decode, decode_pair_rejects_malformed)
{
    std::pair<evmc::address, std::vector<evmc::bytes32>> e;
    const auto rejected = [&e](const bytes& in) {
        bytes_view v = in;
        return !rlp::decode(v, e);
    };
    EXPECT_TRUE(rejected(bytes{}));             // empty input (no header)
    EXPECT_TRUE(rejected("0x80"_hex));          // not a list
    EXPECT_TRUE(rejected("0xc482aabbc0"_hex));  // first element (address) is not 20 bytes

    bytes second_not_list = "0xd694"_hex;  // [address, <string instead of the keys list>]
    second_not_list.append(20, uint8_t{0x11});
    second_not_list += "0x80"_hex;
    EXPECT_TRUE(rejected(second_not_list));

    bytes trailing = "0xd794"_hex;  // [address, [], <trailing element>]
    trailing.append(20, uint8_t{0x11});
    trailing += "0xc000"_hex;
    EXPECT_TRUE(rejected(trailing));
}

TEST(state_rlp_decode, rejects_container_type_mismatch)
{
    {
        bytes out;
        const bytes in = "0xc0"_hex;  // a list where a byte string is expected
        bytes_view v = in;
        EXPECT_FALSE(rlp::decode(v, out));
    }
    {
        std::vector<uint64_t> out;
        const bytes in = "0x80"_hex;  // a string where a list is expected
        bytes_view v = in;
        EXPECT_FALSE(rlp::decode(v, out));
    }
    {
        std::vector<uint64_t> out;
        bytes_view v;  // empty input: no list header
        EXPECT_FALSE(rlp::decode(v, out));
    }
}

// Regression: a malformed list element must leave the output vector untouched (no partial results).
TEST(state_rlp_decode, vector_unchanged_on_failure)
{
    std::vector<uint64_t> out{1, 2, 3};
    const bytes in = "0xc20500"_hex;  // list [5, <0x00: non-canonical integer>].
    bytes_view v = in;
    EXPECT_FALSE(rlp::decode(v, out));
    EXPECT_EQ(out, (std::vector<uint64_t>{1, 2, 3}));
}

// Regression: a wire gas_limit above INT64_MAX must be rejected, not narrowed to a negative int64.
TEST(state_rlp_decode, tx_rejects_gas_limit_over_int64)
{
    auto tx = minimal_legacy_tx();
    tx.gas_limit = -1;  // Encodes as the unsigned wire value 2**64 - 1.
    EXPECT_FALSE(state::decode_transaction(rlp::encode(tx)).has_value());
}

// Regression: a legacy signature v that is neither 27/28 nor >= 35 must be rejected, not
// underflowed.
TEST(state_rlp_decode, tx_rejects_invalid_legacy_v)
{
    auto tx = minimal_legacy_tx();
    tx.v = 5;  // Invalid: neither pre-155 {27, 28} nor EIP-155 (>= 35).
    EXPECT_FALSE(state::decode_transaction(rlp::encode(tx)).has_value());
}

// Regression: an EIP-155 chain id that does not fit uint64 must be rejected, not truncated.
// Hand-crafted legacy tx with v = 2**65 + 35, i.e. chain id 2**64.
TEST(state_rlp_decode, tx_rejects_chain_id_over_uint64)
{
    const auto rlp = "0xd2808080808080890200000000000000230101"_hex;
    EXPECT_FALSE(state::decode_transaction(rlp).has_value());
}

TEST(state_rlp_decode, tx_rejects_malformed_envelope)
{
    EXPECT_FALSE(state::decode_transaction({}).has_value());            // Empty input.
    EXPECT_FALSE(state::decode_transaction("0x00c0"_hex).has_value());  // Type 0 (t == legacy).
    EXPECT_FALSE(state::decode_transaction("0x05c0"_hex).has_value());  // Unknown type 5.
    EXPECT_FALSE(state::decode_transaction("0x0280"_hex).has_value());  // Typed body is not a list.

    // The EIP-2718 type byte is a raw byte, not an RLP item; wrapping it as a 1-byte RLP string
    // (0x81 0x02, read as type 129, outside the {1..4} range) must be rejected.
    auto typed = minimal_legacy_tx();
    typed.type = state::Transaction::Type::eip1559;
    typed.chain_id = 1;
    typed.v = 0;
    auto wrapped = rlp::encode(typed);
    ASSERT_EQ(wrapped[0], 0x02);
    wrapped.insert(wrapped.begin(), uint8_t{0x81});
    EXPECT_FALSE(state::decode_transaction(wrapped).has_value());
}

// A typed transaction's top-level y_parity must be 0 or 1.
TEST(state_rlp_decode, tx_rejects_typed_v_over_1)
{
    auto tx = minimal_legacy_tx();
    tx.type = state::Transaction::Type::eip1559;
    tx.chain_id = 1;
    tx.v = 2;
    EXPECT_FALSE(state::decode_transaction(rlp::encode(tx)).has_value());
}

// A "to" that is neither empty (CREATE) nor exactly 20 bytes must be rejected.
TEST(state_rlp_decode, tx_rejects_wrong_length_to)
{
    bytes rlp = "0xdc80018093"_hex;  // Legacy list header, then a 19-byte "to" (prefix 0x93).
    rlp.append(19, uint8_t{0x11});
    rlp += "0x80801b0102"_hex;  // value, data, v = 27, r, s.
    EXPECT_FALSE(state::decode_transaction(rlp).has_value());
}

// A transaction truncated before any required field must be rejected, not read past the RLP list.
// Covers every field-decode-failure return in decode_transaction_body.
TEST(state_rlp_decode, tx_rejects_truncated_fields)
{
    using rlp::encode_tuple;
    const auto rejected = [](const bytes& tx) {
        return !state::decode_transaction(tx).has_value();
    };
    const std::vector<uint64_t> l;  // Encodes as an empty RLP list (0xc0), used for access_list.

    // Legacy [nonce, gas_price, gas_limit, to, value, data, v, r, s], truncated before each field:
    EXPECT_TRUE(rejected("0xc0"_hex));                                          // nonce
    EXPECT_TRUE(rejected(encode_tuple(uint64_t{0})));                           // gas_price
    EXPECT_TRUE(rejected(encode_tuple(uint64_t{0}, 1_u256)));                   // gas_limit
    EXPECT_TRUE(rejected(encode_tuple(uint64_t{0}, 1_u256, uint64_t{21000})));  // "to"
    EXPECT_TRUE(
        rejected(encode_tuple(uint64_t{0}, 1_u256, uint64_t{21000}, bytes_view{})));  // value
    EXPECT_TRUE(rejected(
        encode_tuple(uint64_t{0}, 1_u256, uint64_t{21000}, bytes_view{}, 6_u256)));  // data
    EXPECT_TRUE(rejected(encode_tuple(
        uint64_t{0}, 1_u256, uint64_t{21000}, bytes_view{}, 6_u256, bytes_view{})));  // v
    EXPECT_TRUE(rejected("0xf8"_hex));  // malformed legacy list header

    // Typed eip1559 [chain_id, nonce, max_priority, max_fee, gas_limit, to, value, data,
    // access_list, y_parity, r, s], truncated before each field:
    EXPECT_TRUE(rejected("0x02c0"_hex));                                         // chain_id
    EXPECT_TRUE(rejected("0x02"_hex + encode_tuple(uint64_t{1})));               // nonce
    EXPECT_TRUE(rejected("0x02"_hex + encode_tuple(uint64_t{1}, uint64_t{2})));  // max_priority
    EXPECT_TRUE(
        rejected("0x02"_hex + encode_tuple(uint64_t{1}, uint64_t{2}, 3_u256, 4_u256, uint64_t{5},
                                  bytes_view{}, 6_u256, bytes_view{})));  // access_list
    EXPECT_TRUE(
        rejected("0x02"_hex + encode_tuple(uint64_t{1}, uint64_t{2}, 3_u256, 4_u256, uint64_t{5},
                                  bytes_view{}, 6_u256, bytes_view{}, l)));  // y_parity
    EXPECT_TRUE(rejected(
        "0x02"_hex + encode_tuple(uint64_t{1}, uint64_t{2}, 3_u256, 4_u256, uint64_t{5},
                         bytes_view{}, 6_u256, bytes_view{}, l, uint64_t{1}, 7_u256)));  // s
    EXPECT_TRUE(rejected("0x02"_hex + encode_tuple(uint64_t{1}, uint64_t{2}, 3_u256, 4_u256,
                                          uint64_t{5}, bytes_view{}, 6_u256, bytes_view{}, l,
                                          uint64_t{1}, 7_u256, 8_u256, uint64_t{9})));  // trailing
                                                                                        // element

    // Typed blob, truncated before the blob-gas fields:
    EXPECT_TRUE(
        rejected("0x03"_hex + encode_tuple(uint64_t{1}, uint64_t{2}, 3_u256, 4_u256, uint64_t{5},
                                  bytes_view{}, 6_u256, bytes_view{}, l)));  // max_fee_per_blob_gas
    EXPECT_TRUE(rejected("0x03"_hex + encode_tuple(uint64_t{1}, uint64_t{2}, 3_u256, 4_u256,
                                          uint64_t{5}, bytes_view{}, 6_u256, bytes_view{}, l,
                                          7_u256)));  // blob_versioned_hashes
}

// The EIP-7702 authorization_list and its entries are RLP lists of exactly six fields each.
TEST(state_rlp_decode, tx_rejects_malformed_authorization)
{
    const auto rejected = [](const bytes& tx) {
        return !state::decode_transaction(tx).has_value();
    };

    // The authorization_list field is a string, not a list.
    EXPECT_TRUE(rejected("0x04"_hex + rlp::encode_tuple(uint64_t{1}, uint64_t{2}, 3_u256, 4_u256,
                                          uint64_t{5}, bytes_view{}, 6_u256, bytes_view{},
                                          std::vector<uint64_t>{}, bytes_view{})));
    EXPECT_TRUE(rejected("0x04cb0102030405800680c0c100"_hex));    // an entry is not a list
    EXPECT_TRUE(rejected("0x04cc0102030405800680c0c2c101"_hex));  // an entry with too few fields

    // An entry [chain, addr, nonce, y_parity, r] missing its final `s`.
    bytes missing_s = "0x04e40102030405800680c0dad90194"_hex;
    missing_s.append(20, uint8_t{0x22});  // 20-byte authority address.
    missing_s += "0x800101"_hex;          // nonce, y_parity, r; no s.
    EXPECT_TRUE(rejected(missing_s));

    // An entry [chain, addr, nonce, y_parity, r, s, <extra>] with a trailing element.
    bytes extra_field = "0x04e60102030405800680c0dcdb0194"_hex;
    extra_field.append(20, uint8_t{0x22});
    extra_field += "0x8001010101"_hex;  // nonce, y_parity, r, s, extra.
    EXPECT_TRUE(rejected(extra_field));
}

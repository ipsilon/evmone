// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmone_precompiles/secp256k1.hpp>
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
/// tests below. Not constexpr: Transaction is not a literal type before libstdc++ 12, which lacks
/// the constexpr container destructors.
const state::Transaction MINIMAL_LEGACY_TX{
    .type = state::Transaction::Type::legacy,
    .gas_limit = 21000,
    .max_gas_price = 1,
    .r = 1_u256,
    .s = 2_u256,
    .v = 27,
};

/// Encodes @p tx and decodes it back; the transaction must decode.
state::Transaction round_trip(const state::Transaction& tx)
{
    const auto decoded = state::decode_transaction(rlp::encode(tx));
    EXPECT_TRUE(decoded.has_value());
    // The argument cannot be spelled {}: value_or() deduces its parameter type (libc++ rejects it).
    return decoded.value_or(state::Transaction{});
}

/// Decodes @p txbytes and recovers the sender of the transaction; it must decode.
std::optional<address> recover(const bytes& txbytes)
{
    const auto tx = state::decode_transaction(txbytes);
    EXPECT_TRUE(tx.has_value());
    return state::recover_sender(tx.value(), txbytes);
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
        EXPECT_EQ(a.y_parity, e.y_parity);
        EXPECT_EQ(a.r, e.r);
        EXPECT_EQ(a.s, e.s);
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
        {to, {0x8e947fe742892ee6fffe7cfc013acac35d33a3892c58597344bed88b21eb1d2f_bytes32}},
    };

    const std::array cases{
        std::pair{"access_list",  // EIP-2930.
            state::Transaction{
                .type = access_list,
                .data = "0x095ea7b3"_hex,
                .gas_limit = 0xc835,
                .max_gas_price = 0x64,
                .max_priority_gas_price = 0x64,  // Mirrors the single wire gas price.
                .to = to,
                .access_list = example_access_list,
                .chain_id = 1,
                .nonce = 62,
                .r = 0x2c_u256,
                .s = 0x41_u256,
                .v = 1,
            }},
        std::pair{"eip1559",
            state::Transaction{
                .type = eip1559,
                .data = "0x095ea7b3"_hex,
                .gas_limit = 0x9c40,
                .max_gas_price = 0x64,
                .max_priority_gas_price = 0x0a,
                .to = to,
                .value = 0x0de0b6b3a7640000_u256,
                .access_list = example_access_list,
                .chain_id = 1,
                .nonce = 42,
                .r = 0x2c_u256,
                .s = 0x41_u256,
                .v = 1,
            }},
        std::pair{"blob",
            state::Transaction{
                .type = blob,
                .gas_limit = 0x7530,
                .max_gas_price = 0x64,
                .max_blob_gas_price = 4,
                .to = 0x535b918f3724001fd6fb52fcc6cbc220592990a3_address,
                .value = 7_u256,
                .blob_hashes =
                    {
                        0x0111111111111111111111111111111111111111111111111111111111111111_bytes32,
                        0x0122222222222222222222222222222222222222222222222222222222222222_bytes32,
                    },
                .chain_id = 1,
                .nonce = 5,
                .r = 9_u256,
                .s = 0xa_u256,
                .v = 1,
            }},
        std::pair{"set_code",  // EIP-7702; auth y_parity may be any value < 2**8.
            state::Transaction{
                .type = set_code,
                .gas_limit = 0x186a0,
                .max_gas_price = 7,
                .to = 0x1111_address,
                .chain_id = 1,
                .r = 1_u256,
                .s = 2_u256,
                .v = 0,
                .authorization_list =
                    {
                        {
                            .chain_id = 1_u256,
                            .addr = 0x2222_address,
                            .nonce = 0,
                            .y_parity = 2,
                            .r = 0x1234_u256,
                            .s = 0x5678_u256,
                        },
                        {
                            .chain_id = 1_u256,
                            .addr = 0x3333_address,
                            .nonce = 7,
                            .y_parity = 27,
                            .r = 0x9abc_u256,
                            .s = 0xdef0_u256,
                        },
                        {
                            .chain_id = 1_u256,
                            .addr = 0x4444_address,
                            .nonce = 42,
                            .y_parity = 0xff,
                            .r = 0xaaaa_u256,
                            .s = 0xbbbb_u256,
                        },
                    },
            }},
    };

    for (const auto& [name, tx] : cases)
    {
        SCOPED_TRACE(name);
        expect_tx_eq(tx, round_trip(tx));
    }
}

TEST(state_rlp_decode, tx_round_trip_legacy)
{
    // A legacy transaction has a single wire gas price, so the decoded form differs from the input
    // in max_priority_gas_price; v is kept verbatim and the chain id derived from it.
    // EIP-155: v = 35 + 2 * chain_id + parity, with v = 35 the lowest accepted value; before it,
    // v is 27 or 28 and the transaction is bound to no chain.
    for (const auto& [v, chain_id] :
        {std::pair{27u, uint64_t{0}}, std::pair{28u, uint64_t{0}}, std::pair{35u, uint64_t{0}},
            std::pair{36u, uint64_t{0}}, std::pair{37u, uint64_t{1}}, std::pair{38u, uint64_t{1}}})
    {
        SCOPED_TRACE(v);
        const state::Transaction in{
            .type = state::Transaction::Type::legacy,
            .data = "0xdeadbeef"_hex,
            .gas_limit = 0x5208,
            .max_gas_price = 0x0102,
            .to = 0x9232a548dd9e81bac65500b5e0d918f8ba93675c_address,
            .value = 0xabcdef_u256,
            .nonce = 7,
            .r = 0x1111_u256,
            .s = 0x2222_u256,
            .v = v,
        };

        auto expected = in;
        expected.max_priority_gas_price = in.max_gas_price;
        expected.chain_id = chain_id;
        expect_tx_eq(expected, round_trip(in));
    }

    // The largest chain id a legacy transaction can carry: any larger one has a v above uint64.
    auto in = MINIMAL_LEGACY_TX;
    in.v = std::numeric_limits<uint64_t>::max();
    auto expected = in;
    expected.max_priority_gas_price = in.max_gas_price;
    expected.chain_id = (std::numeric_limits<uint64_t>::max() - 35) / 2;
    expect_tx_eq(expected, round_trip(in));
}

TEST(state_rlp_decode, tx_set_code_auth_y_parity_overflow_rejected)
{
    // EIP-7702 bounds y_parity to < 2**8; a value of 2**8 fails the whole transaction at decode
    // time, matching geth (V uint8) and revm/alloy (y_parity: U8). Authorization::y_parity cannot
    // hold such a value, so the tuple is encoded by hand.
    const auto auth = rlp::encode_tuple(
        1_u256, 0x2222_address, uint64_t{0}, 0x100_u256, 0x1234_u256, 0x5678_u256);
    const auto payload = rlp::encode(uint64_t{1}) +                  // chain_id
                         rlp::encode(uint64_t{0}) +                  // nonce
                         rlp::encode(uint64_t{0}) +                  // max_priority_gas_price
                         rlp::encode(uint64_t{7}) +                  // max_gas_price
                         rlp::encode(uint64_t{0x186a0}) +            // gas_limit
                         rlp::encode(0x1111_address) +               // to
                         rlp::encode(uint64_t{0}) +                  // value
                         rlp::encode(bytes_view{}) +                 // data
                         rlp::encode(state::AccessList{}) +          // access_list
                         rlp::internal::wrap_list(auth) +            // authorization_list
                         rlp::encode(uint64_t{0}) +                  // y_parity
                         rlp::encode(1_u256) + rlp::encode(2_u256);  // r, s
    EXPECT_FALSE(
        state::decode_transaction("0x04"_hex + rlp::internal::wrap_list(payload)).has_value());
}

TEST(state_rlp_decode, tx_rejects_trailing_data)
{
    // Both the legacy and the typed envelope must reject bytes after the transaction.
    EXPECT_FALSE(state::decode_transaction(rlp::encode(MINIMAL_LEGACY_TX) + "00"_hex).has_value());

    auto typed = MINIMAL_LEGACY_TX;
    typed.type = state::Transaction::Type::eip1559;
    typed.chain_id = 1;
    typed.v = 0;  // Typed y_parity must be in {0, 1}.
    EXPECT_FALSE(state::decode_transaction(rlp::encode(typed) + "00"_hex).has_value());
}

TEST(state_rlp_decode, tx_rejects_under_declared_list_length)
{
    // A list-length prefix that declares fewer bytes than the fields that follow must be rejected;
    // the decoder must honor the declared list boundary, not read past it.
    auto rlp = rlp::encode(MINIMAL_LEGACY_TX);
    ASSERT_GE(rlp[0], 0xc0);  // Short RLP list.
    ASSERT_LT(rlp[0], 0xf8);
    rlp[0] = static_cast<uint8_t>(rlp[0] - 1);  // Declare one byte less than the payload.
    EXPECT_FALSE(state::decode_transaction(rlp).has_value());
}

TEST(state_rlp_decode, header_boundaries)
{
    // decode_header must reject truncated, overflowing, and non-canonical headers and accept a
    // canonical long form. Each rejection below guards a specific past regression.
    rlp::Header h;
    const auto rejected = [&h](bytes_view in) {
        bytes_view v = in;
        return !rlp::decode_header(v, h);
    };

    // A declared length must fit the available input. Each input sits exactly one byte over what
    // is present, so a bound loosened by one would accept it and hand out a payload running past
    // the input.
    EXPECT_TRUE(rejected("0x8201"_hex));  // short string declares 2 bytes, 1 present
    EXPECT_TRUE(rejected("0xb901"_hex));  // long-string length header truncated
    EXPECT_TRUE(rejected("0xc201"_hex));  // short list declares 2 bytes, 1 present
    EXPECT_TRUE(rejected("0xf901"_hex));  // long-list length header truncated
    EXPECT_TRUE(rejected("0xb838"_hex + bytes(55, 0x11)));  // long string declares 56, 55 present
    EXPECT_TRUE(rejected("0xf838"_hex + bytes(55, 0x11)));  // long list declares 56, 55 present

    // A long-form length near 2**64 must be rejected; the naive bounds check
    // `payload_length + length_of_length >= input_len` overflows uint64 and would accept it.
    EXPECT_TRUE(rejected(bytes(9, uint8_t{0xff})));                         // long list
    EXPECT_TRUE(rejected(bytes{uint8_t{0xbf}} + bytes(8, uint8_t{0xff})));  // long string

    // The long-form length must be canonical: no leading zero byte. The two-byte lengths below
    // are otherwise valid, so only the leading-zero rule can reject them.
    EXPECT_TRUE(rejected("0xb90038"_hex + bytes(56, 0x11)));  // long string, length 0x0038 == 56
    EXPECT_TRUE(rejected("0xf90038"_hex + bytes(56, 0x11)));  // long list, length 0x0038 == 56
    EXPECT_TRUE(rejected("0xb800"_hex));                      // long string, length byte 0
    EXPECT_TRUE(rejected("0xf800"_hex));                      // long list, length byte 0

    // The long form is reserved for payloads longer than the short-form maximum (55); 55 itself
    // must still use the short form.
    EXPECT_TRUE(rejected("0xb837"_hex + bytes(55, 0x11)));  // long string for a 55-byte payload
    EXPECT_TRUE(rejected("0xf837"_hex + bytes(55, 0x11)));  // long list for a 55-byte payload

    // A byte below 0x80 must be its own encoding, not a 1-byte string (0x81 0x7f).
    EXPECT_TRUE(rejected("0x817f"_hex));

    // A valid long string (payload longer than the short-form maximum) decodes.
    auto long_str = "0xb838"_hex;  // long string, one length byte 0x38 == 56.
    long_str.append(56, uint8_t{0x11});
    bytes_view v = long_str;
    ASSERT_TRUE(rlp::decode_header(v, h));
    EXPECT_FALSE(h.is_list);
    EXPECT_EQ(h.payload_length, 56u);
}

TEST(state_rlp_decode, fixed_width_requires_exact_length)
{
    // Regression: a fixed-width field (address/hash/storage key) must be encoded as exactly N
    // bytes; a shorter string was silently zero-padded and accepted.
    // Cover both fixed-width instantiations -- bytes32 (32 bytes) and address (20 bytes) -- across
    // an exact-length accept and every reject path of the span decoder.
    const auto check = [](auto& out, size_t n) {
        const auto rejects = [&out](const bytes& in) {
            bytes_view v = in;
            return !rlp::decode(v, out);
        };
        {
            bytes in{static_cast<uint8_t>(0x80 + n)};  // exactly n bytes
            in.append(n, uint8_t{0x11});
            bytes_view v = in;
            EXPECT_TRUE(rlp::decode(v, out));
            EXPECT_TRUE(v.empty());
        }
        {
            bytes in{static_cast<uint8_t>(0x80 + n - 1)};  // one byte too short
            in.append(n - 1, uint8_t{0x11});
            EXPECT_TRUE(rejects(in));
        }
        {
            bytes in{static_cast<uint8_t>(0x80 + n + 1)};  // one byte too long
            in.append(n + 1, uint8_t{0x11});
            EXPECT_TRUE(rejects(in));
        }
        EXPECT_TRUE(rejects("0xc0"_hex));  // a list where a fixed-width string is expected
        EXPECT_TRUE(rejects(bytes{}));     // empty input: no header to decode
    };
    bytes32 hash;
    check(hash, 32);
    address addr;
    check(addr, 20);
}

TEST(state_rlp_decode, integer_rejects_malformed)
{
    // A scalar field must be a canonical, width-bounded string, not a leading-zero integer, an
    // oversized one, or a list.
    // Exercise both scalar-decode instantiations the transaction decoder uses -- uint64_t (nonce,
    // gas limit) and uint256 (value, r, s, y_parity, chain id) -- across accept and every reject
    // path, so each width's decode<T> is covered independently.
    const auto check = []<typename T>(T) {
        T out{};
        const auto decodes = [&out](const bytes& in) {
            bytes_view v = in;
            return rlp::decode(v, out) && v.empty();
        };
        EXPECT_TRUE(decodes("0x80"_hex));  // canonical zero (empty payload)
        EXPECT_EQ(out, T{});
        EXPECT_TRUE(decodes("0x05"_hex));  // canonical small value
        EXPECT_EQ(out, T{5});
        EXPECT_FALSE(decodes("0x820005"_hex));  // leading zero byte (non-canonical)
        EXPECT_FALSE(decodes("0xc0"_hex));      // a list where a scalar is expected
        EXPECT_FALSE(decodes(bytes{}));         // empty input: no header
    };
    check(uint64_t{});
    check(uint256{});

    // A payload wider than the destination integer is rejected (the width bound is per type).
    {
        const auto in = "0x89010000000000000000"_hex;  // 9-byte payload: wider than uint64_t.
        bytes_view v = in;
        uint64_t u = 0;
        EXPECT_FALSE(rlp::decode(v, u));
    }
    {
        bytes in{uint8_t{0xa1}};  // 33-byte payload: wider than uint256.
        in.append(33, uint8_t{0x11});
        bytes_view v = in;
        uint256 u = 0;
        EXPECT_FALSE(rlp::decode(v, u));
    }
}

TEST(state_rlp_decode, decode_pair_rejects_malformed)
{
    // The access-list entry [address, [storage keys]] pair must be a two-element list.
    std::pair<address, std::vector<bytes32>> e;
    const auto rejected = [&e](const bytes& in) {
        bytes_view v = in;
        return !rlp::decode(v, e);
    };
    EXPECT_TRUE(rejected(bytes{}));             // empty input (no header)
    EXPECT_TRUE(rejected("0x80"_hex));          // not a list
    EXPECT_TRUE(rejected("0xc482aabbc0"_hex));  // first element (address) is not 20 bytes

    auto second_not_list = "0xd694"_hex;  // [address, <string instead of the keys list>]
    second_not_list.append(20, uint8_t{0x11});
    second_not_list += "0x80"_hex;
    EXPECT_TRUE(rejected(second_not_list));

    auto trailing = "0xd794"_hex;  // [address, [], <trailing element>]
    trailing.append(20, uint8_t{0x11});
    trailing += "0xc000"_hex;
    EXPECT_TRUE(rejected(trailing));
}

TEST(state_rlp_decode, rejects_container_type_mismatch)
{
    {
        bytes out;
        const auto in = "0xc0"_hex;  // a list where a byte string is expected
        bytes_view v = in;
        EXPECT_FALSE(rlp::decode(v, out));
    }
    {
        std::vector<uint64_t> out;
        const auto in = "0x80"_hex;  // a string where a list is expected
        bytes_view v = in;
        EXPECT_FALSE(rlp::decode(v, out));
    }
    {
        std::vector<uint64_t> out;
        bytes_view v;  // empty input: no list header
        EXPECT_FALSE(rlp::decode(v, out));
    }
}

TEST(state_rlp_decode, vector_unchanged_on_failure)
{
    // Regression: a malformed list element must leave the output vector untouched (no partial
    // results).
    std::vector<uint64_t> out{1, 2, 3};
    const bytes in = "0xc20500"_hex;  // list [5, <0x00: non-canonical integer>].
    bytes_view v = in;
    EXPECT_FALSE(rlp::decode(v, out));
    EXPECT_EQ(out, (std::vector<uint64_t>{1, 2, 3}));
}

TEST(state_rlp_decode, tx_rejects_gas_limit_over_int64)
{
    // Regression: a wire gas_limit above INT64_MAX must be rejected, not narrowed to a negative
    // int64.
    auto tx = MINIMAL_LEGACY_TX;
    tx.gas_limit = -1;  // Encodes as the unsigned wire value 2**64 - 1.
    EXPECT_FALSE(state::decode_transaction(rlp::encode(tx)).has_value());
}

TEST(state_rlp_decode, tx_rejects_invalid_legacy_v)
{
    // Regression: a legacy signature v that is neither 27/28 nor >= 35 must be rejected, not
    // underflowed.
    auto tx = MINIMAL_LEGACY_TX;
    tx.v = 5;  // Invalid: neither pre-155 {27, 28} nor EIP-155 (>= 35).
    EXPECT_FALSE(state::decode_transaction(rlp::encode(tx)).has_value());
}

TEST(state_rlp_decode, tx_rejects_legacy_v_over_uint64)
{
    // Regression: a legacy v that does not fit uint64 must be rejected, not truncated. This bounds
    // the EIP-155 chain id to (2**64 - 36) / 2, the limit the JSON transaction loader also has.
    // Hand-crafted legacy tx with v = 2**65 + 35, i.e. chain id 2**64.
    const auto rlp = "0xd2808080808080890200000000000000230101"_hex;
    EXPECT_FALSE(state::decode_transaction(rlp).has_value());
}

TEST(state_rlp_decode, tx_rejects_malformed_envelope)
{
    EXPECT_FALSE(state::decode_transaction({}).has_value());            // Empty input.
    EXPECT_FALSE(state::decode_transaction("0x00c0"_hex).has_value());  // Type 0 (t == legacy).
    EXPECT_FALSE(state::decode_transaction("0x05c0"_hex).has_value());  // Unknown type 5.
    EXPECT_FALSE(state::decode_transaction("0x0280"_hex).has_value());  // Typed body is not a list.

    // The type byte must be rejected on its own, before the body is looked at: the two bodies
    // below decode cleanly for the type they are shaped for, so only the type check rejects them.
    // Type 0 takes the legacy field order (no chain_id) once past the type check, type 5 the
    // EIP-1559 one.
    EXPECT_FALSE(state::decode_transaction("0x00ca018080808080801b0102"_hex).has_value());
    EXPECT_FALSE(state::decode_transaction("0x05cc8080808080808080c0800102"_hex).has_value());

    // The EIP-2718 type byte is a raw byte, not an RLP item; wrapping it as a 1-byte RLP string
    // (0x81 0x02, read as type 129, outside the {1..4} range) must be rejected.
    auto typed = MINIMAL_LEGACY_TX;
    typed.type = state::Transaction::Type::eip1559;
    typed.chain_id = 1;
    typed.v = 0;
    auto wrapped = rlp::encode(typed);
    ASSERT_EQ(wrapped[0], 0x02);
    wrapped.insert(wrapped.begin(), uint8_t{0x81});
    EXPECT_FALSE(state::decode_transaction(wrapped).has_value());
}

TEST(state_rlp_decode, tx_rejects_typed_v_over_1)
{
    // A typed transaction's top-level y_parity must be 0 or 1.
    auto tx = MINIMAL_LEGACY_TX;
    tx.type = state::Transaction::Type::eip1559;
    tx.chain_id = 1;
    tx.v = 2;
    EXPECT_FALSE(state::decode_transaction(rlp::encode(tx)).has_value());
}

TEST(state_rlp_decode, tx_rejects_wrong_length_to)
{
    // A "to" that is neither empty (CREATE) nor exactly 20 bytes must be rejected.
    auto rlp = "0xdc80018093"_hex;  // Legacy list header, then a 19-byte "to" (prefix 0x93).
    rlp.append(19, uint8_t{0x11});
    rlp += "0x80801b0102"_hex;  // value, data, v = 27, r, s.
    EXPECT_FALSE(state::decode_transaction(rlp).has_value());
}

TEST(state_rlp_decode, tx_rejects_truncated_fields)
{
    // A transaction truncated before any required field must be rejected, not read past the RLP
    // list. Covers every field-decode-failure return in decode_transaction_body.
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

TEST(state_rlp_decode, tx_rejects_malformed_authorization)
{
    // The EIP-7702 authorization_list and its entries are RLP lists of exactly six fields each.
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
    auto missing_s = "0x04e40102030405800680c0dad90194"_hex;
    missing_s.append(20, uint8_t{0x22});  // 20-byte authority address.
    missing_s += "0x800101"_hex;          // nonce, y_parity, r; no s.
    EXPECT_TRUE(rejected(missing_s));

    // An entry [chain, addr, nonce, y_parity, r, s, <extra>] with a trailing element.
    auto extra_field = "0x04e60102030405800680c0dcdb0194"_hex;
    extra_field.append(20, uint8_t{0x22});
    extra_field += "0x8001010101"_hex;  // nonce, y_parity, r, s, extra.
    EXPECT_TRUE(rejected(extra_field));
}

TEST(state_rlp_decode, decode_authorization_field_positions)
{
    // An authorization tuple [chain_id, addr, nonce, y_parity, r, s] must carry every field: an
    // entry truncated before any one of them is rejected.
    const auto chain = "0x01"_hex;
    const bytes addr =
        bytes{uint8_t{0x94}} + bytes(20, uint8_t{0x11});  // RLP of a 20-byte address.
    const auto nonce = "0x07"_hex;
    const auto y_parity = "0x80"_hex;  // 0
    const auto r = "0x03"_hex;
    const auto s = "0x04"_hex;
    const auto as_list = [](const bytes& payload) {
        return bytes{static_cast<uint8_t>(rlp::SHORT_LIST_BASE + payload.size())} + payload;
    };
    const auto rejected = [](const bytes& entry) {
        bytes_view v = entry;
        state::Authorization a;
        return !state::decode(v, a);
    };
    EXPECT_TRUE(rejected(bytes{uint8_t{rlp::SHORT_LIST_BASE}}));          // []: chain_id missing
    EXPECT_TRUE(rejected(as_list(chain)));                                // addr missing
    EXPECT_TRUE(rejected(as_list(chain + addr)));                         // nonce missing
    EXPECT_TRUE(rejected(as_list(chain + addr + nonce)));                 // y_parity missing
    EXPECT_TRUE(rejected(as_list(chain + addr + nonce + y_parity)));      // r missing
    EXPECT_TRUE(rejected(as_list(chain + addr + nonce + y_parity + r)));  // s missing
    EXPECT_TRUE(rejected(as_list(chain + addr + nonce + y_parity + r + s + s)));  // trailing field

    const auto entry = as_list(chain + addr + nonce + y_parity + r + s);  // complete tuple
    bytes_view v = entry;
    state::Authorization a;
    EXPECT_TRUE(state::decode(v, a));
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(a.nonce, 7u);
}

TEST(state_rlp_decode, recover_sender_legacy_protected)
{
    // The same fields signed twice: over the pre-EIP-155 preimage and over the EIP-155 one for
    // chain 0 (wire v = 35/36). Both decode to chain_id 0, so only the verbatim v says which
    // preimage was signed. No EEST fixture signs for chain 0, which is why this is pinned here.
    // Signer of both: 0x1d694d5ad94f32132ff5c14c901d3ddbee90a550 (private key 0xa5). evmone only
    // recovers, so changing the fields means re-signing each preimage elsewhere, with a low s.
    constexpr auto signer = 0x1d694d5ad94f32132ff5c14c901d3ddbee90a550_address;

    const auto protected_tx =
        "0xf86807820102825208949232a548dd9e81bac65500b5e0d918f8ba93675c83abcdef84deadbeef23"
        "a0d6c3bc8b0fc4456b4687ef74c42a70f0dfd2b2d2575a6f614749685164fe85c2"
        "a02264f7f854576e62b8c8415e5f6fdd0ead0876c2b13b8e8d70ddda9239b95f16"_hex;
    EXPECT_EQ(recover(protected_tx), signer);

    const auto unprotected_tx =
        "0xf86807820102825208949232a548dd9e81bac65500b5e0d918f8ba93675c83abcdef84deadbeef1c"
        "a07290c0bb6429493499b400d2912ef585ee39b7c722d086f6de5b175cb495feae"
        "a011417a917fff6e40eb2f74cdd22fb15c268c0ee6fd5ad97fc55af15aab9ae27f"_hex;
    EXPECT_EQ(recover(unprotected_tx), signer);
}

TEST(state_rlp_decode, recover_sender_rejects_out_of_range_s)
{
    // s = the curve order is outside [1, secp256k1n) yet a canonical 32-byte integer, so the
    // transaction decodes and only the recovery rejects it.
    auto tx = MINIMAL_LEGACY_TX;
    tx.s = evmmax::secp256k1::Curve::ORDER;
    EXPECT_FALSE(recover(rlp::encode(tx)).has_value());
}

TEST(state_rlp_decode, recover_sender_rejects_high_s)
{
    // EIP-2 bounds s to the lower half of the curve order, on top of the [1, secp256k1n) range.
    // One above the bound differs from the largest accepted s in nothing else.
    auto tx = MINIMAL_LEGACY_TX;
    tx.s = evmmax::secp256k1::Curve::ORDER / 2;
    EXPECT_TRUE(recover(rlp::encode(tx)).has_value());

    tx.s += 1;
    EXPECT_FALSE(recover(rlp::encode(tx)).has_value());
}

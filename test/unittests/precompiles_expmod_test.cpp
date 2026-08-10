// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmc/hex.hpp>
#include <gtest/gtest.h>
#include <intx/intx.hpp>
#include <test/state/precompiles_internal.hpp>
#include <test/utils/utils.hpp>
#ifdef EVMONE_PRECOMPILES_GMP
#include <test/state/precompiles_gmp.hpp>
#endif

namespace
{
using evmone::state::ExecutionResult;

/// Builds a big-endian value of given size with MSB, optional LSB, and fill byte.
evmc::bytes make_val(size_t size, uint8_t msb, uint8_t lsb = 0, uint8_t fill = 0)
{
    assert(size >= 2);
    evmc::bytes v(size, fill);
    v.front() = msb;
    v.back() = lsb;
    return v;
}

/// Checks that the result is zero everywhere except the last byte which should equal expected.
void expect_last_byte(const std::span<const uint8_t> result, uint8_t expected)
{
    EXPECT_EQ(result.back(), expected);
    const auto head = result.first(result.size() - 1);
    EXPECT_TRUE(std::ranges::all_of(head, [](uint8_t b) { return b == 0; }));
}


/// Function pointer type for expmod execute implementations.
using ExpmodExecuteFn = ExecutionResult (*)(const uint8_t*, size_t, uint8_t*, size_t) noexcept;

struct ExpmodImpl
{
    const char* name;
    ExpmodExecuteFn fn;
};

/// Parameterized test fixture for expmod implementations.
class expmod : public testing::TestWithParam<ExpmodImpl>
{
protected:
    /// Builds modexp precompile input, executes via the parameterized implementation, and returns
    /// the result.
    static evmc::bytes run(const evmc::bytes& base, const evmc::bytes& exp, const evmc::bytes& mod)
    {
        evmc::bytes input(3 * 32, 0);
        using namespace intx;
        be::unsafe::store(&input[0], uint256{base.size()});
        be::unsafe::store(&input[32], uint256{exp.size()});
        be::unsafe::store(&input[64], uint256{mod.size()});
        input += base;
        input += exp;
        input += mod;

        evmc::bytes result(mod.size(), 0xfe);  // Sentinel fill to detect partial writes.
        const auto [status, output_size] =
            GetParam().fn(input.data(), input.size(), result.data(), result.size());
        EXPECT_EQ(status, EVMC_SUCCESS);
        EXPECT_EQ(output_size, mod.size());
        return result;
    }
};

const ExpmodImpl EXPMOD_IMPLS[] = {
    {"evmone", &evmone::state::expmod_execute_evmone},
#ifdef EVMONE_PRECOMPILES_GMP
    {"gmp", &evmone::state::expmod_execute_gmp},
#endif
};

INSTANTIATE_TEST_SUITE_P(
    impls, expmod, testing::ValuesIn(EXPMOD_IMPLS), [](const auto& x) { return x.param.name; });
}  // namespace

TEST_P(expmod, inputs)
{
    struct TestCase
    {
        std::string_view base_hex;
        std::string_view exp_hex;
        std::string_view mod_hex;
        std::string_view expected_result_hex;
    };

    /// Test vectors for expmod precompile.
    const std::vector<TestCase> test_cases{
        {"", "", "", ""},
        {"", "", "00", "00"},
        {"", "", "01", "00"},
        {"", "", "02", "01"},
        {"", "", "0200", "0001"},
        // 0^0 with multi-word mod 0.
        {"", "", "000000000000000000", "000000000000000000"},
        // 0^0 with multi-word mod 1.
        {"", "", "000000000000000001", "000000000000000000"},
        // 0^0 with multi-word mod > 1.
        {"", "", "ff0000000000000000", "000000000000000001"},
        // 0^0 with multi-word mod > 1.
        {"", "", "ff0000000000000001", "000000000000000001"},
        {"03", "07", "00", "00"},
        {"03", "00", "01", "00"},
        {"03", "07", "01", "00"},
        {"00", "00", "02", "01"},
        {"03", "07", "02", "01"},
        {"02", "03", "00", "00"},
        {"02", "01", "03", "02"},
        {"02", "03", "06", "02"},
        {"03", "", "06", "01"},
        {"03", "00", "06", "01"},
        {"03", "01", "14", "03"},
        {"03", "02", "14", "09"},
        // Even modulus: 3^3 mod 12 = 27 mod 12 = 3.
        {"03", "03", "0c", "03"},
        // Even modulus: 2^5 mod 20 = 32 mod 20 = 12.
        {"02", "05", "14", "0c"},
        // Even modulus: 5^7 mod 24 = 78125 mod 24 = 5.
        {"05", "07", "18", "05"},
        {"03", "03", "03a0", "001b"},
        {"09", "05", "10", "09"},
        {"09", "05", "11", "08"},
        {"09", "05", "13", "10"},
        {"09", "05", "17", "08"},
        {"09", "05", "18", "09"},
        {"03", "80", "ff", "ab"},
        {"03", "1c93", "61", "5f"},
        // base=0 with exp>0: 0^n = 0 for all paths (odd, even, pow2).
        {"00", "01", "07", "00"},
        {"00", "03", "06", "00"},
        {"00", "01", "08", "00"},
        // Different base/mod byte sizes.
        {"0100", "01", "07", "04"},  // large base, small odd mod
        {"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", "01", "0006", "0003"},
        {"02", "05", "060000000000000000", "000000000000000020"},
        // Even modulus with large base: odd part (3) has fewer significant words than the
        // result buffer. Regression test for zeroing trailing words in modexp_odd().
        {"00000000000000000000000000000002", "03", "00000000000000000000000000000006",
            "00000000000000000000000000000002"},
        {"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", "02",
            "fffffffffffffffd", "0000000000001900"},
        {"02", "03", "0100000000000000000000000000000001", "0000000000000000000000000000000008"},
        // Power-of-two modulus bigger than single word.
        {"cc", "11", "00000001000000000000000000000000", "00000000fe8477d6c9cef3cc00000000"},
        // Odd modulus of various word sizes (1, 3, 5 words).
        {"0000000000000002", "01", "8000000000000001", "0000000000000002"},
        {"000000000000000000000000000000000000000000000002", "01",
            "800000000000000000000000000000000000000000000001",
            "000000000000000000000000000000000000000000000002"},
        {"00000000000000000000000000000000000000000000000000000000000000000000000000000002", "01",
            "80000000000000000000000000000000000000000000000000000000000000000000000000000001",
            "00000000000000000000000000000000000000000000000000000000000000000000000000000002"},
        // Full-width base triggers normalization headroom path in rem().
        {"80000000000000000000000000000000", "01", "80000000000000000000000000000001",
            "80000000000000000000000000000000"},
        {
            "03",
            "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2e",
            "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f",
            "0000000000000000000000000000000000000000000000000000000000000001",
        },
        {
            "20000000000000000000000000000000000000000000000000000000000000000000000000000010200000"
            "00000000000000000000000000000000000000000000000000000000000000000000000010200000000000"
            "00000000000000000000000000000000000000000000000000000000000000000010200000000000000000"
            "00000000000000000000000000000000000000000000000000000000000010",
            "03",
            "60000000000000000000000000000000000000000000000000000000000000000000000000000010600000"
            "00000000000000000000000000000000000000000000000000000000000000000000000010600000000000"
            "00000000000000000000000000000000000000000000000000000000000000000010600000000000000000"
            "00000000000000000000000000000000000000000000000000000000000010",
            "291b1e2948112b098f3c987bec2dbac8022f5e4ebd3d4c47f333b30e3de21ca3b8aca475da7d3240291b1e"
            "2948112b098f3c987bec2dbac8022f5e4ebd3d4c47f333b30e3de21ca3b8aca475da7d3240291b1e294811"
            "2b098f3c987bec2dbac8022f5e4ebd3d4c47f333b30e3de21ca3b8aca475da7d3240291b1e2948112b098f"
            "3c987bec2dbac8022f5e4ebd3d4c47f333b30e3de21ca3b8aca475da7d3240",
        },
        {
            "20000000000000000000000000000000000000000000000000000000000000000000000000000010200000"
            "00000000000000000000000000000000000000000000000000000000000000000000000010200000000000"
            "00000000000000000000000000000000000000000000000000000000000000000010200000000000000000"
            "00000000000000000000000000000000000000000000000000000000000010200000000000000000000000"
            "00000000000000000000000000000000000000000000000000000010200000000000000000000000000000"
            "00000000000000000000000000000000000000000000000010200000000000000000000000000000000000"
            "00000000000000000000000000000000000000000010200000000000000000000000000000000000000000"
            "00000000000000000000000000000000000010",
            "03",
            "60000000000000000000000000000000000000000000000000000000000000000000000000000010600000"
            "00000000000000000000000000000000000000000000000000000000000000000000000010600000000000"
            "00000000000000000000000000000000000000000000000000000000000000000010600000000000000000"
            "00000000000000000000000000000000000000000000000000000000000010600000000000000000000000"
            "00000000000000000000000000000000000000000000000000000010600000000000000000000000000000"
            "00000000000000000000000000000000000000000000000010600000000000000000000000000000000000"
            "00000000000000000000000000000000000000000010600000000000000000000000000000000000000000"
            "00000000000000000000000000000000000010",
            "2d408bba3fa657dbb2cd49d4a1d329966c23e59e10c2a65950af0c4b047e185de46ee3d11f9b6b202d408b"
            "ba3fa657dbb2cd49d4a1d329966c23e59e10c2a65950af0c4b047e185de46ee3d11f9b6b202d408bba3fa6"
            "57dbb2cd49d4a1d329966c23e59e10c2a65950af0c4b047e185de46ee3d11f9b6b202d408bba3fa657dbb2"
            "cd49d4a1d329966c23e59e10c2a65950af0c4b047e185de46ee3d11f9b6b202d408bba3fa657dbb2cd49d4"
            "a1d329966c23e59e10c2a65950af0c4b047e185de46ee3d11f9b6b202d408bba3fa657dbb2cd49d4a1d329"
            "966c23e59e10c2a65950af0c4b047e185de46ee3d11f9b6b202d408bba3fa657dbb2cd49d4a1d329966c23"
            "e59e10c2a65950af0c4b047e185de46ee3d11f9b6b202d408bba3fa657dbb2cd49d4a1d329966c23e59e10"
            "c2a65950af0c4b047e185de46ee3d11f9b6b20",
        },
        // Even modulus: (2^320 - 1) * 2^64.
        {
            "03",
            "0300",
            "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
            "0000000000000000",
            "52fb579ce1cc2f9ca0d054fc45bedb199389314a067aaff8f058864885f642c5acbbf6a3673585bb"
            "442488538f42dc01",
        },
        // Even modulus: (2^768 - 1) * 2^320.
        {
            "03",
            "0300",
            "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
            "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
            "ffffffffffffffffffffffffffffffff000000000000000000000000000000000000000000000000"
            "00000000000000000000000000000000",
            "4059444789547ff685087bca8a144d32556b2251613171aa4cf05d8a005015d9b8408ba5f7c89595"
            "e76d925173cf80e552a856b1ce217c1f33940f8241e6adcf76b672c4935a40f4bb36410ee24654c1"
            "14cd718bea878742c703dc5abddbdbfa17d12328a2a6bb9d6e80dc0bc224eef03128625977a1e2c1"
            "d189336e303567d7442488538f42dc01",
        },
        // Large base (48 bytes) with small even modulus (16 bytes).
        {
            "010101010101010101010101010101010101010101010101"
            "010101010101010101010101010101010101010101010101",
            "01",
            "02020202020202020202020202020202",
            "01010101010101010101010101010101",
        },
        // base wider than mod (9 bytes vs 1 byte), odd mod.
        {"000000000000000009", "01", "07", "02"},
        // base >> mod (32 bytes vs 1 byte), odd mod.
        {"0000000000000000000000000000000000000000000000000000000000000002", "01", "07", "02"},
        // mod >> base (32 bytes vs 1 byte), odd mod.
        {"02", "01", "8000000000000000000000000000000000000000000000000000000000000007",
            "0000000000000000000000000000000000000000000000000000000000000002"},
        // mod >> base (32 bytes vs 1 byte), even mod.
        {"02", "01", "8000000000000000000000000000000000000000000000000000000000000006",
            "0000000000000000000000000000000000000000000000000000000000000002"},
        // base >> mod (32 bytes vs 1 byte), even mod.
        {"0000000000000000000000000000000000000000000000000000000000000002", "01", "06", "02"},
        // Test cases for AMM.
        {"03", "02", "09", "00"},
        {"03", "02", "0000000000000000000000000000000000000000000000000000000000000009",
            "0000000000000000000000000000000000000000000000000000000000000000"},
        {"03", "02",
            "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
            "000000000000000000000000000000000000000009",
            "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
            "000000000000000000000000000000000000000000"},
        {"02", "02",
            "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
            "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
            "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
            "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
            "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
            "000000000000000000000000000000000000000000000000000000000000000000000000000000000004"},
        {"fffffffffffffffffffffffffffffffe", "02", "ffffffffffffffffffffffffffffffff",
            "00000000000000000000000000000001"},
        // Small base with multi-word power-of-two modulus (pow2 path: base shorter than mod).
        {"03", "07", "00000000000000000100000000000000000000000000000000",
            "0000000000000000000000000000000000000000000000088b"},
        // Small base with multi-word even modulus (even path: trimmed mod shorter than w).
        // 3^3 mod 12, where 12 is encoded as 16 bytes.
        {"03", "03", "0000000000000000000000000000000c", "00000000000000000000000000000003"},
        // Even modulus with leading zeros: 3^3 mod 12, where 12 is encoded as 32 bytes.
        // CRT product size (odd_size + pow2_size = 2) < declared_mod_size (4 words).
        {"03", "03", "000000000000000000000000000000000000000000000000000000000000000c",
            "0000000000000000000000000000000000000000000000000000000000000003"},
        // Small base with even modulus having large pow2 factor
        // (even/pow2 path: base shorter than num_pow2_words).
        // 3^5 mod (5 * 2^128) = 243.
        {"03", "05", "000000000000000500000000000000000000000000000000",
            "0000000000000000000000000000000000000000000000f3"},
        // Even modulus with 1-word odd part and multi-word pow2 factor.
        // Exercises carry/borrow propagation in add/sub with shorter operand.
        // 2^64 mod (3 * 2^128).
        {"02", "40", "0300000000000000000000000000000000", "0000000000000000010000000000000000"},
        // 2^128 mod (3 * 2^128): carry propagates through all high words in add.
        {"02", "80", "0300000000000000000000000000000000", "0100000000000000000000000000000000"},
        // 2^129 mod (7 * 2^128): carry propagates and is absorbed in nonzero word.
        {"02", "0081", "0700000000000000000000000000000000", "0200000000000000000000000000000000"},

        // Sliding-window exponentiation in modexp_odd. One case per window width w=1..5.
        // Each exponent is built as: top bit (1) | zero run of w+1 bits | one run of w bits
        // | trailing zeros, so its windows exercise both the first (b^1) and last
        // (b^(2^w-1)) precomputed odd powers, the zero run in between being wide enough to
        // keep them in separate windows, traversed by squarings alone. Modulus is the
        // secp256k1 field prime: odd, 4 words, so these also cover the mul_amm<4>
        // specialization.
        // exp_bits=6, w=1: plain binary square-and-multiply, no table.
        {"03", "24", "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f",
            "00000000000000000000000000000000000000000000000002153e468b91c6d1"},
        // exp_bits=10, w=2: windows hit b^1 and b^3.
        {"03", "0230", "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f",
            "e123f780b153ebd75b17a6e7a7133dba60d90a7dbc0f770f08af0055f8e2c7ed"},
        // exp_bits=30, w=3: windows hit b^1 and b^7.
        {"03", "21c00000", "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f",
            "6cc581d10c7d071216edf63238959949056d7cddf5a90711a7c7cdec6b3e861f"},
        // exp_bits=100, w=4: windows hit b^1 and b^15.
        {"03", "083c0000000000000000000000",
            "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f",
            "7ff2c68783b688439f7c43de4cbfe265f8875ec726564a442c2cbd1244f6d99e"},
        // exp_bits=254 (mainnet-typical size), w=5: windows hit b^1 and b^31.
        {"03", "207c000000000000000000000000000000000000000000000000000000000000",
            "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f",
            "40ea9ce0f6a2c94a7bec98114179d8e1a21287312a25c1fdd7bf46e3d723984a"},
        // Same exponent as the w=5 case above, with a 5-word modulus: the cases above only
        // ever run through the mul_amm<4> specialization, this covers the generic
        // std::dynamic_extent instantiation at w > 1.
        {"03", "207c000000000000000000000000000000000000000000000000000000000000",
            "fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f",
            "aa50260a96f69a722fc965bbfec20c21195eda68068b20e9899976f80ed8f6d4f6816bec10fc4ee6"},

        // Random exponents straddling the width thresholds of the fixed-window
        // implementation this replaced, which no longer coincide with the bands above:
        // exp_bits 16..18 (w=2), 48..51 (w=3), 144..148 (w=4). Same modulus as above,
        // except for the last case, which repeats exp_bits=148 with a 5-word modulus.
        {"03", "8005", "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f",
            "79c4559d064ab3615f6da729a1f67265b88ee2eaba22838109bea30fb7bee31b"},
        {"03", "01001b", "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f",
            "a890a61d8d745fae67a345fb031b048c0cf8952b43622263de0fdc4391a6c6a9"},
        {"03", "0200c9", "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f",
            "600614416289329cf72ef906cdfc1dea20339051ec80ed3ff692eb14ed33be81"},
        {"03", "80013b71b865", "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f",
            "fd66fdbe1f0c43e6640c121c366b9061c7f13964a572828c8e3968a50dba847f"},
        {"03", "0100d2c92fc182", "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f",
            "651aace134976d8456fcc35686a57cf12670b2e596dabecd0ddae9984ced96c4"},
        {"03", "0200a6a7ef231d", "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f",
            "f722a91e1faa3b57f0a19af8d4506b395a0a342e9ee2cbe65cd7a63155d38537"},
        {"03", "04013929f7999c", "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f",
            "06f41e370c4ef45a2bc5e1ade1504fbe35e5a42a8f8c2b17ad16a6c657900d48"},
        {"03", "8004cb3ff13151bb9f84a488a5d62e79a680",
            "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f",
            "97265df41405de7f9b35c1037c349ef367cffd34ed6a86cb933fe14f84bb12d1"},
        {"03", "010014b0a1922289f0b19f56c6c373b0e5cd4a",
            "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f",
            "3587c0d41ce1eb59ec2fa686877d8166aa9740f2410f9271592e5f283e3bd738"},
        {"03", "02008d61508c16734bdbe4a9578f4c8185d260",
            "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f",
            "f65d573e0ba5bdc7cc0e31072eb946ffe5138d0cd4bc936cc1a714d17cdaf954"},
        {"03", "040160dce60c2531e93ae750b53938d5b04faf",
            "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f",
            "0648a7caabfd3d4b972c034830faf933179ed038e1e6a6c4c3ad26f330fe1397"},
        {"03", "0802ae8d294c48793907af3e71b536ed84fa84",
            "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f",
            "40c2770e749bcbf7949855252da0258cc5ae80658427a4af8ba3489a81182ee9"},
        {"03", "08f83d563ebc382e09e4b8245edebc817af708",
            "fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f",
            "8016137e4c542dd66f4ab5f668fc0ac76d43353a675f3d4616a56f23757e463ca1093164385ef006"},
    };

    for (const auto& [base_hex, exp_hex, mod_hex, expected_result_hex] : test_cases)
    {
        const auto result =
            run(*evmc::from_hex(base_hex), *evmc::from_hex(exp_hex), *evmc::from_hex(mod_hex));
        EXPECT_EQ(hex(result), expected_result_hex);
    }
}

TEST_P(expmod, large_inputs)
{
    // Tests with base/mod of 1024 and 1025 bytes, covering all modulus types.

    // 1024-byte inputs (EIP-7823 limit).
    expect_last_byte(run({0x02}, {0x01}, make_val(1024, 0x80, 1)), 2);  // odd
    expect_last_byte(run({0x02}, {0x01}, make_val(1024, 0x01)), 2);     // power-of-two
    expect_last_byte(run({0x02}, {0x01}, make_val(1024, 0x80, 2)), 2);  // even (1-bit tz)

    // 1025-byte inputs (exceeds EIP-7823, exercises heap fallback for native impl).
    expect_last_byte(run({0x02}, {0x01}, make_val(1025, 0x80, 1)), 2);  // odd
    expect_last_byte(run({0x02}, {0x01}, make_val(1025, 0x01)), 2);     // power-of-two
    expect_last_byte(run({0x02}, {0x01}, make_val(1025, 0x80, 2)), 2);  // even (1-bit tz)

    // Large base AND even modulus (both 1025 bytes, CRT path with large base).
    // base < mod, so base^1 mod M = base.
    EXPECT_EQ(run(make_val(1025, 0x40, 0x03), {0x01}, make_val(1025, 0x80, 2)),
        make_val(1025, 0x40, 0x03));

    // Even modulus with tiny odd part and large pow2 factor.
    // mod = 3 * 256^1023 (1024 bytes). inv_scratch dominates op_scratch.
    // 2^1 mod M = 2.
    expect_last_byte(run({0x02}, {0x01}, make_val(1024, 0x03)), 2);

    // Dense values: (2^N - 2)^2 mod (2^N - 1) = 1. Tests AMM reduction at various sizes.
    for (const auto n : {size_t{64}, size_t{128}, size_t{256}, size_t{512}, size_t{1024}})
        expect_last_byte(
            run(make_val(n, 0xff, 0xfe, 0xff), {0x02}, make_val(n, 0xff, 0xff, 0xff)), 1);

    // AMM test: 3^2 mod 9 = 0, at various sizes.
    for (const auto n : {size_t{128}, size_t{256}, size_t{1024}})
        expect_last_byte(run({0x03}, {0x02}, make_val(n, 0x00, 0x09)), 0);

    // Full-width base triggers normalization headroom path in rem(). 136-byte values.
    expect_last_byte(run(make_val(136, 0x00, 0x02), {0x01}, make_val(136, 0x80, 0x01)), 2);

    // Large exponent (256 bytes): 2^(0x00...03) mod 6 = 8 mod 6 = 2.
    expect_last_byte(run({0x02}, make_val(256, 0x00, 0x03), {0x06}), 2);
}

TEST(expmod, analysis_oog)
{
    // Tests the gas cost calculation of the expmod precompile.
    // The result cost is expected to prohibit execution.
    static constexpr auto GAS_LIMIT = 1'000'000'000;
    static constexpr std::array inputs{
        // clang-format off
        "0000000000000000000000000000000000000000000000000000000000000001 000000000000000000000000000000000000000000000000 80000000 00000020 0000000000000000000000000000000000000000000000000000000000000001 80",
        "0000000000000000000000000000000000000000000000000000000000000001 000000000000000000000000000000000000000000000000 40000000 00000020 0000000000000000000000000000000000000000000000000000000000000001 80",
        "0000000000000000000000000000000000000000000000000000000000000001 000000000000000000000000000000000000000000000000 20000000 00000020 0000000000000000000000000000000000000000000000000000000000000001 80",
        "0000000000000000000000000000000000000000000000000000000000000001 000000000000000000000000000000000000000000000000 10000000 00000020 0000000000000000000000000000000000000000000000000000000000000001 80",
        "0000000000000000000000000000000000000000000000000000000000000001 000000000000000000000000000000000000000000000000 00000000 80000020 0000000000000000000000000000000000000000000000000000000000000001 80",
        "0000000000000000000000000000000000000000000000000000000000000001 000000000000000000000000000000000000000000000000 00000000 40000020 0000000000000000000000000000000000000000000000000000000000000001 80",
        "0000000000000000000000000000000000000000000000000000000000000001 000000000000000000000000000000000000000000000000 00000000 20000020 0000000000000000000000000000000000000000000000000000000000000001 80",
        "fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff9 0000000000000000000000000000000000000000000000000000000000000001 0000000000000000000000000000000000000000000000000000000000000001",
        "0000000000000000000000000000000000000000000000000000000000000001 0000000000000000000000000000000000000000000000000000000000000040 00000000000000000000000000000000000000000000000000000000ffffffff 80"
        "0000000000000000000000000000000000000000000000000000000000000001 0000000000000000000000000000000000000000000000000000000000000040 00000000000000000000000000000000000000000000000000000000ffffffff 80",
        "00000000000000000000000000000000000000000000000000000000ffffffff 0000000000000000000000000000000000000000000000000000000000000001 0000000000000000000000000000000000000000000000000000000000000001 80",
        "0000000000000000000000000000000000000000000000000000000000000001 00000000000000000000000000000000000000000000000000000000ffffffff 0000000000000000000000000000000000000000000000000000000000000001 80",
        "0000000000000000000000000000000000000000000000000000000080000000 0000000000000000000000000000000000000000000000000000000080000000 0000000000000000000000000000000000000000000000000000000000000001 80",
        // clang-format on
    };

    for (const auto& input_hex : inputs)
    {
        const auto input = evmc::from_spaced_hex(input_hex).value();
        const auto [gas_cost, max_output_size] = evmone::state::expmod_analyze(input, EVMC_PRAGUE);
        EXPECT_GT(gas_cost, GAS_LIMIT);
    }
}

TEST(expmod, incomplete_inputs)
{
    struct TestCase
    {
        std::string_view input_hex;
        std::string_view expected_result_hex;
    };

    // Tests for expmod with raw and incomplete inputs (requires padding input with zero bytes).
    static constexpr auto GAS_LIMIT = 100'000'000;
    const std::string huge_output(0x20000 * 2, '0');
    const std::vector<TestCase> inputs{
        // clang-format off
        {"", ""},
        {"0000000000000000000000000000000000000000000000000000000000000000 0000000000000000000000000000000000000000000000000000000000000000 0000000000000000000000000000000000000000000000000000000000000002 02", "0001"},
        {"0000000000000000000000000000000000000000000000000000000000000001 0000000000000000000000000000000000000000000000000000000000000001 0000000000000000000000000000000000000000000000000000000000000001", "00"},
        {"0000000000000000000000000000000000000000000000000000000000000001 0000000000000000000000000000000000000000000000000000000000000001 0000000000000000000000000000000000000000000000000000000000000001 ba ee", "00"},
        {"0000000000000000000000000000000000000000000000000000000000000001 0000000000000000000000000000000000000000000000000000000000000001 0000000000000000000000000000000000000000000000000000000000000002 ba ee d0", "9000"},
        {"0000000000000000000000000000000000000000000000000000000000000001 0000000000000000000000000000000000000000000000000000000000000001 0000000000000000000000000000000000000000000000000000000000000003 ba ee d000", "100000"},
        {"0000000000000000000000000000000000000000000000000000000000000001 0000000000000000000000000000000000000000000000000000000000000001 0000000000000000000000000000000000000000000000000000000000000003 ba ee d001", "789700"},
        {"0000000000000000000000000000000000000000000000000000000000000001 0000000000000000000000000000000000000000000000000000000000000001 0000000000000000000000000000000000000000000000000000000000000003 ba ee 00d0", "009000"},
        {"0000000000000000000000000000000000000000000000000000000000000001 0000000000000000000000000000000000000000000000000000000000000001 0000000000000000000000000000000000000000000000000000000000000003 ba ee 0000", "000000"},
        {"0000000000000000000000000000000000000000000000000000000000000001 0000000000000000000000000000000000000000000000000000000000000001 0000000000000000000000000000000000000000000000000000000000000003 ba ee 000000 fe", "000000"},
        {"0000000000000000000000000000000000000000000000000000000000000010 0000000000000000000000000000000000000000000000000000000000000010 0000000000000000000000000000000000000000000000000000000000000001 0000000000000000000000000000000001", "00"},
        {"000000000000000000000000000000000000000000000000000000000000ffff 0000000000000000000000000000000000000000000000000000000000000001 0000000000000000000000000000000000000000000000000000000000000001 80", "00"},
        {"0000000000000000000000000000000000000000000000000000000000000000 0000000000000000000000000000000000000000000000000000000000000001 0000000000000000000000000000000000000000000000000000000000000000", ""},
        {"0000000000000000000000000000000000000000000000000000000000000000 0000000000000000000000000000000000000000000000000000000000000002 0000000000000000000000000000000000000000000000000000000000000000 80", ""},
        {"0000000000000000000000000000000000000000000000000000000000000000 0000000000000000000000000000000000000000000000000000000100000000 0000000000000000000000000000000000000000000000000000000000000000 80", ""},
        {"0000000000000000000000000000000000000000000000000000000000020000 0000000000000000000000000000000000000000000000000000000000000020 0000000000000000000000000000000000000000000000000000000000020000 80", huge_output},
        // clang-format on
    };

    for (const auto& [input_hex, expected_result_hex] : inputs)
    {
        const auto input = evmc::from_spaced_hex(input_hex).value();
        const auto [gas_cost, max_output_size] = evmone::state::expmod_analyze(input, EVMC_PRAGUE);
        ASSERT_LT(gas_cost, GAS_LIMIT);
        auto output = std::make_unique_for_overwrite<uint8_t[]>(max_output_size);
        const auto [status, output_size] = evmone::state::expmod_execute(
            input.data(), input.size(), output.get(), max_output_size);
        EXPECT_EQ(status, EVMC_SUCCESS);
        const auto result_hex = evmc::hex({output.get(), output_size});
        EXPECT_EQ(result_hex, expected_result_hex);
    }
}

TEST(expmod, huge_inputs_analysis)
{
    // Tests expmod_analyze for inputs with huge moduli that exceed the native modexp
    // implementation's size limit. These are near the gas limit boundary:
    // the max mod_len for a given exp magnitude that still fits within GAS_LIMIT.
    //
    // Must be pre-Osaka: EIP-7823 (Osaka) caps mod_len at 1024 bytes, so inputs with
    // larger moduli would return GasCostMax instead of the expected gas below GAS_LIMIT.
    static constexpr auto REV = EVMC_PRAGUE;
    static constexpr auto GAS_LIMIT = 100'000'000;
    struct TestCase
    {
        std::string_view input_hex;
        size_t expected_output_size;
    };
    const std::vector<TestCase> inputs{
        // mod_len=0xcc80 (52352), adj_exp=7 (exp=0xff): gas=99922517
        {"0000000000000000000000000000000000000000000000000000000000000001"
         "0000000000000000000000000000000000000000000000000000000000000001"
         "000000000000000000000000000000000000000000000000000000000000cc80"
         "01ffff",
            0xcc80},
        // mod_len=0x21d40 (138560), adj_exp=1 (exp=0x01): gas=99994133
        {"0000000000000000000000000000000000000000000000000000000000000001"
         "0000000000000000000000000000000000000000000000000000000000000001"
         "0000000000000000000000000000000000000000000000000000000000021d40"
         "ff01ff",
            0x21d40},
    };

    for (const auto& [input_hex, expected_output_size] : inputs)
    {
        const auto input = evmc::from_spaced_hex(input_hex).value();
        const auto [gas_cost, max_output_size] = evmone::state::expmod_analyze(input, REV);
        EXPECT_LT(gas_cost, GAS_LIMIT);
        EXPECT_EQ(max_output_size, expected_output_size);
    }
}

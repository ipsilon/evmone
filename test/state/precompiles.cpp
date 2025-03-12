// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2022 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "precompiles.hpp"
#include "precompiles_internal.hpp"
#include "precompiles_stubs.hpp"
#include <evmone_precompiles/blake2b.hpp>
#include <evmone_precompiles/bls.hpp>
#include <evmone_precompiles/bn254.hpp>
#include <evmone_precompiles/kzg.hpp>
#include <evmone_precompiles/ripemd160.hpp>
#include <evmone_precompiles/secp256k1.hpp>
#include <evmone_precompiles/sha256.hpp>
#include <intx/intx.hpp>
#include <array>
#include <bit>
#include <cassert>
#include <limits>
#include <span>

#ifdef EVMONE_PRECOMPILES_OPENSSL
#include "precompiles_openssl.hpp"
#endif

#ifdef EVMONE_PRECOMPILES_SILKPRE
#include "precompiles_silkpre.hpp"
#endif

namespace evmone::state
{
using evmc::bytes;
using evmc::bytes_view;
using namespace evmc::literals;

namespace
{
constexpr auto GasCostMax = std::numeric_limits<int64_t>::max();

constexpr auto BLS12_SCALAR_SIZE = 32;
constexpr auto BLS12_FIELD_ELEMENT_SIZE = 64;
constexpr auto BLS12_G1_POINT_SIZE = 2 * BLS12_FIELD_ELEMENT_SIZE;
constexpr auto BLS12_G2_POINT_SIZE = 4 * BLS12_FIELD_ELEMENT_SIZE;
constexpr auto BLS12_G1_MUL_INPUT_SIZE = BLS12_G1_POINT_SIZE + BLS12_SCALAR_SIZE;
constexpr auto BLS12_G2_MUL_INPUT_SIZE = BLS12_G2_POINT_SIZE + BLS12_SCALAR_SIZE;

constexpr int64_t num_words(size_t size_in_bytes) noexcept
{
    return static_cast<int64_t>((size_in_bytes + 31) / 32);
}

template <int BaseCost, int WordCost>
constexpr int64_t cost_per_input_word(size_t input_size) noexcept
{
    return BaseCost + WordCost * num_words(input_size);
}
}  // namespace

PrecompileAnalysis ecrecover_analyze(bytes_view /*input*/, evmc_revision /*rev*/) noexcept
{
    return {3000, 32};
}

PrecompileAnalysis sha256_analyze(bytes_view input, evmc_revision /*rev*/) noexcept
{
    return {cost_per_input_word<60, 12>(input.size()), 32};
}

PrecompileAnalysis ripemd160_analyze(bytes_view input, evmc_revision /*rev*/) noexcept
{
    return {cost_per_input_word<600, 120>(input.size()), 32};
}

PrecompileAnalysis identity_analyze(bytes_view input, evmc_revision /*rev*/) noexcept
{
    return {cost_per_input_word<15, 3>(input.size()), input.size()};
}

PrecompileAnalysis ecadd_analyze(bytes_view /*input*/, evmc_revision rev) noexcept
{
    return {rev >= EVMC_ISTANBUL ? 150 : 500, 64};
}

PrecompileAnalysis ecmul_analyze(bytes_view /*input*/, evmc_revision rev) noexcept
{
    return {rev >= EVMC_ISTANBUL ? 6000 : 40000, 64};
}

PrecompileAnalysis ecpairing_analyze(bytes_view input, evmc_revision rev) noexcept
{
    const auto base_cost = (rev >= EVMC_ISTANBUL) ? 45000 : 100000;
    const auto element_cost = (rev >= EVMC_ISTANBUL) ? 34000 : 80000;
    const auto num_elements = static_cast<int64_t>(input.size() / 192);
    return {base_cost + num_elements * element_cost, 32};
}

PrecompileAnalysis blake2bf_analyze(bytes_view input, evmc_revision) noexcept
{
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
    return {input.size() == 213 ? intx::be::unsafe::load<uint32_t>(input.data()) : GasCostMax, 64};
}

PrecompileAnalysis expmod_analyze(bytes_view input, evmc_revision rev) noexcept
{
    using namespace intx;

    static constexpr size_t input_header_required_size = 3 * sizeof(uint256);
    const int64_t min_gas = (rev >= EVMC_BERLIN) ? 200 : 0;

    uint8_t input_header[input_header_required_size]{};
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
    std::copy_n(input.data(), std::min(input.size(), input_header_required_size), input_header);

    const auto base_len = be::unsafe::load<uint256>(&input_header[0]);
    const auto exp_len = be::unsafe::load<uint256>(&input_header[32]);
    const auto mod_len = be::unsafe::load<uint256>(&input_header[64]);

    if (base_len == 0 && mod_len == 0)
        return {min_gas, 0};

    static constexpr auto len_limit = std::numeric_limits<size_t>::max();
    if (base_len > len_limit || exp_len > len_limit || mod_len > len_limit)
        return {GasCostMax, 0};

    auto adjusted_len = [input](size_t offset, size_t len) {
        const auto head_len = std::min(len, size_t{32});
        const auto head_explicit_len =
            std::max(std::min(offset + head_len, input.size()), offset) - offset;
        const bytes_view head_explicit_bytes(&input[offset], head_explicit_len);
        const auto top_byte_index = head_explicit_bytes.find_first_not_of(uint8_t{0});
        const size_t exp_bit_width =
            (top_byte_index != bytes_view::npos) ?
                (head_len - top_byte_index - 1) * 8 +
                    static_cast<size_t>(std::bit_width(head_explicit_bytes[top_byte_index])) :
                0;

        return std::max(
            8 * (std::max(len, size_t{32}) - 32) + (std::max(exp_bit_width, size_t{1}) - 1),
            size_t{1});
    };

    static constexpr auto mult_complexity_eip2565 = [](const uint256& x) noexcept {
        const auto w = (x + 7) >> 3;
        return w * w;
    };
    static constexpr auto mult_complexity_eip198 = [](const uint256& x) noexcept {
        const auto x2 = x * x;
        if (x <= 64)
            return x2;
        else if (x <= 1024)
            return (x2 >> 2) + 96 * x - 3072;
        else
            return (x2 >> 4) + 480 * x - 199680;
    };

    const auto max_len = std::max(mod_len, base_len);
    const auto adjusted_exp_len = adjusted_len(
        sizeof(input_header) + static_cast<size_t>(base_len), static_cast<size_t>(exp_len));
    const auto gas = (rev >= EVMC_BERLIN) ?
                         mult_complexity_eip2565(max_len) * adjusted_exp_len / 3 :
                         mult_complexity_eip198(max_len) * adjusted_exp_len / 20;
    return {std::max(min_gas, static_cast<int64_t>(std::min(gas, intx::uint256{GasCostMax}))),
        static_cast<size_t>(mod_len)};
}

PrecompileAnalysis point_evaluation_analyze(bytes_view, evmc_revision) noexcept
{
    static constexpr auto POINT_EVALUATION_PRECOMPILE_GAS = 50000;
    return {POINT_EVALUATION_PRECOMPILE_GAS, 64};
}

PrecompileAnalysis bls12_g1add_analyze(bytes_view, evmc_revision) noexcept
{
    static constexpr auto BLS12_G1ADD_PRECOMPILE_GAS = 375;
    return {BLS12_G1ADD_PRECOMPILE_GAS, BLS12_G1_POINT_SIZE};
}

PrecompileAnalysis bls12_g1msm_analyze(bytes_view input, evmc_revision) noexcept
{
    static constexpr auto G1MUL_GAS_COST = 12000;
    static constexpr uint16_t DISCOUNTS[] = {1000, 949, 848, 797, 764, 750, 738, 728, 719, 712, 705,
        698, 692, 687, 682, 677, 673, 669, 665, 661, 658, 654, 651, 648, 645, 642, 640, 637, 635,
        632, 630, 627, 625, 623, 621, 619, 617, 615, 613, 611, 609, 608, 606, 604, 603, 601, 599,
        598, 596, 595, 593, 592, 591, 589, 588, 586, 585, 584, 582, 581, 580, 579, 577, 576, 575,
        574, 573, 572, 570, 569, 568, 567, 566, 565, 564, 563, 562, 561, 560, 559, 558, 557, 556,
        555, 554, 553, 552, 551, 550, 549, 548, 547, 547, 546, 545, 544, 543, 542, 541, 540, 540,
        539, 538, 537, 536, 536, 535, 534, 533, 532, 532, 531, 530, 529, 528, 528, 527, 526, 525,
        525, 524, 523, 522, 522, 521, 520, 520, 519};

    if (input.empty() || input.size() % BLS12_G1_MUL_INPUT_SIZE != 0)
        return {GasCostMax, 0};

    const auto k = input.size() / BLS12_G1_MUL_INPUT_SIZE;
    assert(k > 0);
    const auto discount = DISCOUNTS[std::min(k, std::size(DISCOUNTS)) - 1];
    const auto cost = (G1MUL_GAS_COST * discount * static_cast<int64_t>(k)) / 1000;
    return {cost, BLS12_G1_POINT_SIZE};
}

PrecompileAnalysis bls12_g2add_analyze(bytes_view, evmc_revision) noexcept
{
    static constexpr auto BLS12_G2ADD_PRECOMPILE_GAS = 600;
    return {BLS12_G2ADD_PRECOMPILE_GAS, BLS12_G2_POINT_SIZE};
}

PrecompileAnalysis bls12_g2msm_analyze(bytes_view input, evmc_revision) noexcept
{
    static constexpr auto G2MUL_GAS_COST = 22500;
    static constexpr uint16_t DISCOUNTS[] = {1000, 1000, 923, 884, 855, 832, 812, 796, 782, 770,
        759, 749, 740, 732, 724, 717, 711, 704, 699, 693, 688, 683, 679, 674, 670, 666, 663, 659,
        655, 652, 649, 646, 643, 640, 637, 634, 632, 629, 627, 624, 622, 620, 618, 615, 613, 611,
        609, 607, 606, 604, 602, 600, 598, 597, 595, 593, 592, 590, 589, 587, 586, 584, 583, 582,
        580, 579, 578, 576, 575, 574, 573, 571, 570, 569, 568, 567, 566, 565, 563, 562, 561, 560,
        559, 558, 557, 556, 555, 554, 553, 552, 552, 551, 550, 549, 548, 547, 546, 545, 545, 544,
        543, 542, 541, 541, 540, 539, 538, 537, 537, 536, 535, 535, 534, 533, 532, 532, 531, 530,
        530, 529, 528, 528, 527, 526, 526, 525, 524, 524};

    if (input.empty() || input.size() % BLS12_G2_MUL_INPUT_SIZE != 0)
        return {GasCostMax, 0};

    const auto k = input.size() / BLS12_G2_MUL_INPUT_SIZE;
    assert(k > 0);
    const auto discount = DISCOUNTS[std::min(k, std::size(DISCOUNTS)) - 1];
    const auto cost = (G2MUL_GAS_COST * discount * static_cast<int64_t>(k)) / 1000;
    return {cost, BLS12_G2_POINT_SIZE};
}

PrecompileAnalysis bls12_pairing_check_analyze(bytes_view input, evmc_revision) noexcept
{
    static constexpr auto PAIR_SIZE = BLS12_G1_POINT_SIZE + BLS12_G2_POINT_SIZE;

    if (input.empty() || input.size() % PAIR_SIZE != 0)
        return {GasCostMax, 0};

    const auto npairs = static_cast<int64_t>(input.size()) / PAIR_SIZE;

    static constexpr auto BLS12_PAIRING_CHECK_BASE_FEE_PRECOMPILE_GAS = 37700;
    static constexpr auto BLS12_PAIRING_CHECK_FEE_PRECOMPILE_GAS = 32600;
    return {BLS12_PAIRING_CHECK_BASE_FEE_PRECOMPILE_GAS +
                BLS12_PAIRING_CHECK_FEE_PRECOMPILE_GAS * npairs,
        32};
}

PrecompileAnalysis bls12_map_fp_to_g1_analyze(bytes_view, evmc_revision) noexcept
{
    static constexpr auto BLS12_MAP_FP_TO_G1_PRECOMPILE_GAS = 5500;
    return {BLS12_MAP_FP_TO_G1_PRECOMPILE_GAS, BLS12_G1_POINT_SIZE};
}

PrecompileAnalysis bls12_map_fp2_to_g2_analyze(bytes_view, evmc_revision) noexcept
{
    static constexpr auto BLS12_MAP_FP2_TO_G2_PRECOMPILE_GAS = 23800;
    return {BLS12_MAP_FP2_TO_G2_PRECOMPILE_GAS, BLS12_G2_POINT_SIZE};
}

ExecutionResult ecrecover_execute(const uint8_t* input, size_t input_size, uint8_t* output,
    [[maybe_unused]] size_t output_size) noexcept
{
    assert(output_size >= 32);

    uint8_t input_buffer[128]{};
    if (input_size != 0)
        std::memcpy(input_buffer, input, std::min(input_size, std::size(input_buffer)));

    ethash::hash256 h{};
    std::memcpy(h.bytes, input_buffer, sizeof(h));

    const auto v = intx::be::unsafe::load<intx::uint256>(input_buffer + 32);
    if (v != 27 && v != 28)
        return {EVMC_SUCCESS, 0};
    const bool parity = v == 28;

    const auto r = intx::be::unsafe::load<intx::uint256>(input_buffer + 64);
    const auto s = intx::be::unsafe::load<intx::uint256>(input_buffer + 96);

    const auto res = evmmax::secp256k1::ecrecover(h, r, s, parity);
    if (res)
    {
        std::memset(output, 0, 12);
        std::memcpy(output + 12, res->bytes, 20);
        return {EVMC_SUCCESS, 32};
    }
    else
        return {EVMC_SUCCESS, 0};
}

ExecutionResult sha256_execute(const uint8_t* input, size_t input_size, uint8_t* output,
    [[maybe_unused]] size_t output_size) noexcept
{
    assert(output_size >= 32);
    crypto::sha256(reinterpret_cast<std::byte*>(output), reinterpret_cast<const std::byte*>(input),
        input_size);
    return {EVMC_SUCCESS, 32};
}

ExecutionResult ripemd160_execute(const uint8_t* input, size_t input_size, uint8_t* output,
    [[maybe_unused]] size_t output_size) noexcept
{
    assert(output_size >= 32);
    output = std::fill_n(output, 12, std::uint8_t{0});
    crypto::ripemd160(reinterpret_cast<std::byte*>(output),
        reinterpret_cast<const std::byte*>(input), input_size);
    return {EVMC_SUCCESS, 32};
}

ExecutionResult expmod_execute(
    const uint8_t* input, size_t input_size, uint8_t* output, size_t output_size) noexcept
{
    static constexpr auto LEN_SIZE = sizeof(intx::uint256);

    // The output size equal to the modulus size.
    // Handle short incomplete input up front. The answer is 0 of the length of the modulus.
    if (output_size == 0 || input_size <= 3 * LEN_SIZE) [[unlikely]]
    {
        std::fill_n(output, output_size, 0);
        return {EVMC_SUCCESS, output_size};
    }

#if defined(EVMONE_PRECOMPILES_OPENSSL)
    return openssl_expmod_execute(input, input_size, output, output_size);
#elif defined(EVMONE_PRECOMPILES_SILKPRE)
    return silkpre_expmod_execute(input, input_size, output, output_size);
#else
    return expmod_stub(input, input_size, output, output_size);
#endif
}

ExecutionResult ecadd_execute(const uint8_t* input, size_t input_size, uint8_t* output,
    [[maybe_unused]] size_t output_size) noexcept
{
    assert(output_size >= 64);

    uint8_t input_buffer[128]{};
    if (input_size != 0)
        std::memcpy(input_buffer, input, std::min(input_size, std::size(input_buffer)));

    const evmmax::bn254::Point p = {intx::be::unsafe::load<intx::uint256>(input_buffer),
        intx::be::unsafe::load<intx::uint256>(input_buffer + 32)};
    const evmmax::bn254::Point q = {intx::be::unsafe::load<intx::uint256>(input_buffer + 64),
        intx::be::unsafe::load<intx::uint256>(input_buffer + 96)};

    if (evmmax::bn254::validate(p) && evmmax::bn254::validate(q))
    {
        const auto res = evmmax::bn254::add(p, q);
        intx::be::unsafe::store(output, res.x);
        intx::be::unsafe::store(output + 32, res.y);
        return {EVMC_SUCCESS, 64};
    }
    else
        return {EVMC_PRECOMPILE_FAILURE, 0};
}

ExecutionResult ecmul_execute(const uint8_t* input, size_t input_size, uint8_t* output,
    [[maybe_unused]] size_t output_size) noexcept
{
    assert(output_size >= 64);

    uint8_t input_buffer[96]{};
    if (input_size != 0)
        std::memcpy(input_buffer, input, std::min(input_size, std::size(input_buffer)));

    const evmmax::bn254::Point p = {intx::be::unsafe::load<intx::uint256>(input_buffer),
        intx::be::unsafe::load<intx::uint256>(input_buffer + 32)};
    const auto c = intx::be::unsafe::load<intx::uint256>(input_buffer + 64);

    if (evmmax::bn254::validate(p))
    {
        const auto res = evmmax::bn254::mul(p, c);
        intx::be::unsafe::store(output, res.x);
        intx::be::unsafe::store(output + 32, res.y);
        return {EVMC_SUCCESS, 64};
    }
    else
        return {EVMC_PRECOMPILE_FAILURE, 0};
}

ExecutionResult ecpairing_execute(const uint8_t* input, size_t input_size, uint8_t* output,
    [[maybe_unused]] size_t output_size) noexcept
{
    assert(output_size >= 32);

    static constexpr size_t PAIR_SIZE = 192;

    if (input_size % PAIR_SIZE != 0)
        return {EVMC_PRECOMPILE_FAILURE, 0};

    if (const auto n_pairs = input_size / PAIR_SIZE; n_pairs > 0)
    {
        std::vector<std::pair<evmmax::bn254::Point, evmmax::bn254::ExtPoint>> pairs;
        pairs.reserve(n_pairs);
        auto input_idx = input;
        for (size_t i = 0; i < n_pairs; ++i)
        {
            const evmmax::bn254::Point p{
                intx::be::unsafe::load<intx::uint256>(input_idx),
                intx::be::unsafe::load<intx::uint256>(input_idx + 32),
            };

            const evmmax::bn254::ExtPoint q{
                {intx::be::unsafe::load<intx::uint256>(input_idx + 96),
                    intx::be::unsafe::load<intx::uint256>(input_idx + 64)},
                {intx::be::unsafe::load<intx::uint256>(input_idx + 160),
                    intx::be::unsafe::load<intx::uint256>(input_idx + 128)},
            };
            pairs.emplace_back(p, q);
            input_idx += PAIR_SIZE;
        }

        const auto res = evmmax::bn254::pairing_check(pairs);

        if (res.has_value())
        {
            intx::be::unsafe::store(output, res.value() ? intx::uint256{1} : intx::uint256{0});
            return {EVMC_SUCCESS, 64};
        }
        else
            return {EVMC_PRECOMPILE_FAILURE, 0};
    }
    else
    {
        intx::be::unsafe::store(output, intx::uint256{1});
        return {EVMC_SUCCESS, 32};
    }
}

ExecutionResult identity_execute(const uint8_t* input, size_t input_size, uint8_t* output,
    [[maybe_unused]] size_t output_size) noexcept
{
    assert(output_size >= input_size);
    std::copy_n(input, input_size, output);
    return {EVMC_SUCCESS, input_size};
}

ExecutionResult blake2bf_execute(const uint8_t* input, [[maybe_unused]] size_t input_size,
    uint8_t* output, [[maybe_unused]] size_t output_size) noexcept
{
    static_assert(std::endian::native == std::endian::little,
        "blake2bf only works correctly on little-endian architectures");
    assert(input_size >= 213);
    assert(output_size >= 64);

    const auto rounds = intx::be::unsafe::load<uint32_t>(input);
    input += sizeof(rounds);

    uint64_t h[8];
    std::memcpy(h, input, sizeof(h));
    input += sizeof(h);

    uint64_t m[16];
    std::memcpy(m, input, sizeof(m));
    input += sizeof(m);

    uint64_t t[2];
    std::memcpy(t, input, sizeof(t));
    input += sizeof(t);

    const auto f = *input;
    if (f != 0 && f != 1) [[unlikely]]
        return {EVMC_PRECOMPILE_FAILURE, 0};

    crypto::blake2b_compress(rounds, h, m, t, f != 0);
    std::memcpy(output, h, sizeof(h));
    return {EVMC_SUCCESS, sizeof(h)};
}

ExecutionResult point_evaluation_execute(const uint8_t* input, size_t input_size, uint8_t* output,
    [[maybe_unused]] size_t output_size) noexcept
{
    assert(output_size >= 64);
    if (input_size != 192)
        return {EVMC_PRECOMPILE_FAILURE, 0};

    const auto r = crypto::kzg_verify_proof(reinterpret_cast<const std::byte*>(&input[0]),
        reinterpret_cast<const std::byte*>(&input[32]),
        reinterpret_cast<const std::byte*>(&input[64]),
        reinterpret_cast<const std::byte*>(&input[96]),
        reinterpret_cast<const std::byte*>(&input[96 + 48]));

    if (!r)
        return {EVMC_PRECOMPILE_FAILURE, 0};

    // Return FIELD_ELEMENTS_PER_BLOB and BLS_MODULUS as padded 32 byte big endian values
    // as required by the EIP-4844.
    intx::be::unsafe::store(output, crypto::FIELD_ELEMENTS_PER_BLOB);
    intx::be::unsafe::store(output + 32, crypto::BLS_MODULUS);
    return {EVMC_SUCCESS, 64};
}

ExecutionResult bls12_g1add_execute(const uint8_t* input, size_t input_size, uint8_t* output,
    [[maybe_unused]] size_t output_size) noexcept
{
    if (input_size != 2 * BLS12_G1_POINT_SIZE)
        return {EVMC_PRECOMPILE_FAILURE, 0};

    assert(output_size == BLS12_G1_POINT_SIZE);

    if (!crypto::bls::g1_add(output, &output[64], input, &input[64], &input[128], &input[192]))
        return {EVMC_PRECOMPILE_FAILURE, 0};

    return {EVMC_SUCCESS, BLS12_G1_POINT_SIZE};
}

ExecutionResult bls12_g1msm_execute(const uint8_t* input, size_t input_size, uint8_t* output,
    [[maybe_unused]] size_t output_size) noexcept
{
    if (input_size % BLS12_G1_MUL_INPUT_SIZE != 0)
        return {EVMC_PRECOMPILE_FAILURE, 0};

    assert(output_size == BLS12_G1_POINT_SIZE);

    if (!crypto::bls::g1_msm(output, &output[64], input, input_size))
        return {EVMC_PRECOMPILE_FAILURE, 0};

    return {EVMC_SUCCESS, BLS12_G1_POINT_SIZE};
}

ExecutionResult bls12_g2add_execute(const uint8_t* input, size_t input_size, uint8_t* output,
    [[maybe_unused]] size_t output_size) noexcept
{
    if (input_size != 2 * BLS12_G2_POINT_SIZE)
        return {EVMC_PRECOMPILE_FAILURE, 0};

    assert(output_size == BLS12_G2_POINT_SIZE);

    if (!crypto::bls::g2_add(output, &output[128], input, &input[128], &input[256], &input[384]))
        return {EVMC_PRECOMPILE_FAILURE, 0};

    return {EVMC_SUCCESS, BLS12_G2_POINT_SIZE};
}

ExecutionResult bls12_g2msm_execute(const uint8_t* input, size_t input_size, uint8_t* output,
    [[maybe_unused]] size_t output_size) noexcept
{
    if (input_size % BLS12_G2_MUL_INPUT_SIZE != 0)
        return {EVMC_PRECOMPILE_FAILURE, 0};

    assert(output_size == BLS12_G2_POINT_SIZE);

    if (!crypto::bls::g2_msm(output, &output[128], input, input_size))
        return {EVMC_PRECOMPILE_FAILURE, 0};

    return {EVMC_SUCCESS, BLS12_G2_POINT_SIZE};
}

ExecutionResult bls12_pairing_check_execute(const uint8_t* input, size_t input_size,
    uint8_t* output, [[maybe_unused]] size_t output_size) noexcept
{
    if (input_size % (BLS12_G1_POINT_SIZE + BLS12_G2_POINT_SIZE) != 0)
        return {EVMC_PRECOMPILE_FAILURE, 0};

    assert(output_size == 32);

    if (!crypto::bls::pairing_check(output, input, input_size))
        return {EVMC_PRECOMPILE_FAILURE, 0};

    return {EVMC_SUCCESS, 32};
}

ExecutionResult bls12_map_fp_to_g1_execute(const uint8_t* input, size_t input_size, uint8_t* output,
    [[maybe_unused]] size_t output_size) noexcept
{
    if (input_size != BLS12_FIELD_ELEMENT_SIZE)
        return {EVMC_PRECOMPILE_FAILURE, 0};

    assert(output_size == BLS12_G1_POINT_SIZE);

    if (!crypto::bls::map_fp_to_g1(output, &output[64], input))
        return {EVMC_PRECOMPILE_FAILURE, 0};

    return {EVMC_SUCCESS, BLS12_G1_POINT_SIZE};
}

ExecutionResult bls12_map_fp2_to_g2_execute(const uint8_t* input, size_t input_size,
    uint8_t* output, [[maybe_unused]] size_t output_size) noexcept
{
    if (input_size != 2 * BLS12_FIELD_ELEMENT_SIZE)
        return {EVMC_PRECOMPILE_FAILURE, 0};

    assert(output_size == BLS12_G2_POINT_SIZE);

    if (!crypto::bls::map_fp2_to_g2(output, &output[128], input))
        return {EVMC_PRECOMPILE_FAILURE, 0};

    return {EVMC_SUCCESS, BLS12_G2_POINT_SIZE};
}

namespace
{
struct PrecompileTraits
{
    decltype(identity_analyze)* analyze = nullptr;
    decltype(identity_execute)* execute = nullptr;
};

inline constexpr std::array<PrecompileTraits, NumPrecompiles> traits{{
    {},  // undefined for 0
    {ecrecover_analyze, ecrecover_execute},
    {sha256_analyze, sha256_execute},
    {ripemd160_analyze, ripemd160_execute},
    {identity_analyze, identity_execute},
    {expmod_analyze, expmod_execute},
    {ecadd_analyze, ecadd_execute},
    {ecmul_analyze, ecmul_execute},
    {ecpairing_analyze, ecpairing_execute},
    {blake2bf_analyze, blake2bf_execute},
    {point_evaluation_analyze, point_evaluation_execute},
    {bls12_g1add_analyze, bls12_g1add_execute},
    {bls12_g1msm_analyze, bls12_g1msm_execute},
    {bls12_g2add_analyze, bls12_g2add_execute},
    {bls12_g2msm_analyze, bls12_g2msm_execute},
    {bls12_pairing_check_analyze, bls12_pairing_check_execute},
    {bls12_map_fp_to_g1_analyze, bls12_map_fp_to_g1_execute},
    {bls12_map_fp2_to_g2_analyze, bls12_map_fp2_to_g2_execute},
}};
}  // namespace

bool is_precompile(evmc_revision rev, const evmc::address& addr) noexcept
{
    // Define compile-time constant,
    // TODO(clang18): workaround for Clang Analyzer bug, fixed in clang 18.
    //                https://github.com/llvm/llvm-project/issues/59493.
    static constexpr evmc::address address_boundary{stdx::to_underlying(PrecompileId::latest)};

    if (evmc::is_zero(addr) || addr > address_boundary)
        return false;

    const auto id = addr.bytes[19];
    if (rev < EVMC_BYZANTIUM && id >= stdx::to_underlying(PrecompileId::since_byzantium))
        return false;

    if (rev < EVMC_ISTANBUL && id >= stdx::to_underlying(PrecompileId::since_istanbul))
        return false;

    if (rev < EVMC_CANCUN && id >= stdx::to_underlying(PrecompileId::since_cancun))
        return false;

    if (rev < EVMC_PRAGUE && id >= stdx::to_underlying(PrecompileId::since_prague))
        return false;

    return true;
}

evmc::Result call_precompile(evmc_revision rev, const evmc_message& msg) noexcept
{
    assert(msg.gas >= 0);

    const auto id = msg.code_address.bytes[19];
    const auto [analyze, execute] = traits[id];

    const bytes_view input{msg.input_data, msg.input_size};
    const auto [gas_cost, max_output_size] = analyze(input, rev);
    const auto gas_left = msg.gas - gas_cost;
    if (gas_left < 0)
        return evmc::Result{EVMC_OUT_OF_GAS};

    // Allocate buffer for the precompile's output and pass its ownership to evmc::Result.
    // TODO: This can be done more elegantly by providing constructor evmc::Result(std::unique_ptr).
    const auto output_data = new (std::nothrow) uint8_t[max_output_size];
    const auto [status_code, output_size] =
        execute(msg.input_data, msg.input_size, output_data, max_output_size);
    const evmc_result result{status_code, status_code == EVMC_SUCCESS ? gas_left : 0, 0,
        output_data, output_size,
        [](const evmc_result* res) noexcept { delete[] res->output_data; }};
    return evmc::Result{result};
}
}  // namespace evmone::state

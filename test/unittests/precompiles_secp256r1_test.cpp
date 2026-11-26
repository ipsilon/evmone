// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmone_precompiles/secp256r1.hpp>
#include <gtest/gtest.h>

using namespace evmone::crypto::secp256r1;
using namespace intx;

namespace
{
struct P256VerifyInput
{
    uint256 h;
    uint256 r;
    uint256 s;
    uint256 x;
    uint256 y;
};

const P256VerifyInput VALID_INPUTS[]{
    {
        0xbb5a52f42f9c9261ed4361f59422a1e30036e7c32b270c8807a419feca605023_u256,
        0x2ba3a8be6b94d5ec80a6d9d1190a436effe50d85a1eee859b8cc6af9bd5c2e18_u256,
        0x4cd60b855d442f5b3c7b11eb6c4e0ae7525fe710fab9aa7c77a67f79e6fadd76_u256,
        0x2927b10512bae3eddcfe467828128bad2903269919f7086069c8c4df6c732838_u256,
        0xc7787964eaac00e5921fb1498a60f4606766b3d9685001558d1a974e7341513e_u256,
    },
    {
        // Valid public key with 0 x-coordinate.
        0xc3d3be9eb3577f217ae0ab360529a30b18adc751aec886328593d7d6fe042809_u256,
        0x3a4e97b44cbf88b90e6205a45ba957e520f63f3c6072b53c244653278a1819d8_u256,
        0x6a184aa037688a5ebd25081fd2c0b10bb64fa558b671bd81955ca86e09d9d722_u256,
        0,
        0x66485c780e2f83d72433bd5d84a06bb6541c2af31dae871728bf856a174f93f4_u256,
    },
    {
        // id="u1_eq_u2_and_Q_eq_G"
        0x7CF27B188D034F7E8A52380304B51AC3C08969E277F21B35A60B48FC47669978_u256,
        0x7CF27B188D034F7E8A52380304B51AC3C08969E277F21B35A60B48FC47669978_u256,
        0x830D84E672FCB08275ADC7FCFB4AE53BFC5D90CB2F25834F4DAE81C6B4FC8BD9_u256,
        G.x.value(),
        G.y.value(),
    },
    {
        // Q = −G: the msm() table entry G + Q is the point at infinity.
        0x6038d49a0c9edc92c761aa73983cffd22aab0cfc8e972d09100f2ea5fa094cb0_u256,
        0xe4958eb9a98b86c3fd80ef08a6702a50532d4470a9d5ccd7f21feff38ebfcf06_u256,
        0x2aa1c1dfb80ec2e1ac69e9f321c50304684efc958443a5bfa1e546dd4e6beb86_u256,
        G.x.value(),
        (-G).y.value(),
    },
    {
        // s is 2²¹¹ in Montgomery form, so its inversion starts with 211 halvings.
        0xe4e07c26c991c0d95d8056b8b9b3ee94fd97fcace24780feb1a55bcce5ef83e7_u256,
        0x6bf85d5fe84598b10ca6199ec09ccd7d35dcd7195fdd1dc7737a0c67006ef251_u256,
        0x455770059d208ffa62df7005e277ffffedd356b381e07ad21c9958082f1d0712_u256,
        0x0c901d423c831ca85e27c73c263ba132721bb9d7a84c4f0380b2a6756fd60133_u256,
        0x1c8870234dec878504c174144fa4b14b66a651691606d8173e55bd37e381569e_u256,
    },
    {
        // s in Montgomery form needs 620 safegcd divsteps, the most we found for the order.
        0xfc0f69b129955591d2a768f0b65119837fde68a53be43d4ea2c231a7ca756fba_u256,
        0x6bf85d5fe84598b10ca6199ec09ccd7d35dcd7195fdd1dc7737a0c67006ef251_u256,
        0xb4fc47bcdfbf49002283b7a3330f100284fc6cfd0054fd6f755e686a6db87be0_u256,
        0x0c901d423c831ca85e27c73c263ba132721bb9d7a84c4f0380b2a6756fd60133_u256,
        0x1c8870234dec878504c174144fa4b14b66a651691606d8173e55bd37e381569e_u256,
    },
    {
        // x₁ = p − 3 ≥ n, r = x₁ − n.
        0xd6e034185702bf60c28acbbb7c6034707d1b4ae465314ab7a2a3ef2a82a2a9ad_u256,
        0x4319055358e8617b0c46353d039cdaab_u256,
        0x97c7075cb68274e6518c04b3cc57126faebfbfbd37818d61_u256,
        0x4d868294bcf24d0c506802c30960964c735170e11eb604bc19e2bb66445911c8_u256,
        0x9223fbada9f5a0b950c805095c092c99908cd0ac50da1222e583e7dd58f8b1c9_u256,
    },
    {
        // Q = 2G: msm() adds Q to an accumulator equal to Q.
        0xe7d1a0ee374ec103d1b9e971f1c99ce69ef66eaf5d76f8423cffb3bad232ff54_u256,
        0x35a591803fc8f3de93fad6e118472ad1189e8c136262fdce2a112df262ed2d3d_u256,
        0x4ac2e7cac2671b76c887e1375327595fb94cc29271200b19db803c70b26b6f81_u256,
        0x7cf27b188d034f7e8a52380304b51ac3c08969e277f21b35a60b48fc47669978_u256,
        0x07775510db8ed040293d9ac69f7430dbba7dade63ce982299e04b79d227873d1_u256,
    },
};

const P256VerifyInput INVALID_INPUTS[]{
    {0, 0, 0, 0, 0},
    {
        // r ≥ n: r + n of a valid signature.
        0xd6e034185702bf60c28acbbb7c6034707d1b4ae465314ab7a2a3ef2a82a2a9ad_u256,
        0xffffffff00000001000000000000000000000000fffffffffffffffffffffffc_u256,
        0x97c7075cb68274e6518c04b3cc57126faebfbfbd37818d61_u256,
        0x4d868294bcf24d0c506802c30960964c735170e11eb604bc19e2bb66445911c8_u256,
        0x9223fbada9f5a0b950c805095c092c99908cd0ac50da1222e583e7dd58f8b1c9_u256,
    },
    {
        // s = 0.
        0xd6e034185702bf60c28acbbb7c6034707d1b4ae465314ab7a2a3ef2a82a2a9ad_u256,
        0x4319055358e8617b0c46353d039cdaab_u256,
        0,
        0x4d868294bcf24d0c506802c30960964c735170e11eb604bc19e2bb66445911c8_u256,
        0x9223fbada9f5a0b950c805095c092c99908cd0ac50da1222e583e7dd58f8b1c9_u256,
    },
    {
        // s ≥ n: s + n of a valid signature.
        0xd6e034185702bf60c28acbbb7c6034707d1b4ae465314ab7a2a3ef2a82a2a9ad_u256,
        0x4319055358e8617b0c46353d039cdaab_u256,
        0xffffffff0000000197c7075cb68274e60e72ff61736eb0f4a2798a8033e4b2b2_u256,
        0x4d868294bcf24d0c506802c30960964c735170e11eb604bc19e2bb66445911c8_u256,
        0x9223fbada9f5a0b950c805095c092c99908cd0ac50da1222e583e7dd58f8b1c9_u256,
    },
    {
        // qx ≥ p: qx + p of a valid signature.
        0x778cf0b386bbb1c4767e806cad7ebcf703c6336d3d15828baab08a582141401c_u256,
        0xb0968f3550f85b97699a43c75dd5205883c77ac40f9d53c816e442e4c4bf3cdc_u256,
        0xbd166b79e2c11a28a22a113ee3931c4e7655e4dcf353d8c0d774134b14132a36_u256,
        0xffffffff00000001000000000000000000000000ffffffffffffffffffffffff_u256,
        0x66485c780e2f83d72433bd5d84a06bb6541c2af31dae871728bf856a174f93f4_u256,
    },
    {
        // qy ≥ p: qy + p of a valid signature.
        0x7b652b733d674a3a42bc4acff69c49d84e8ab3d55d4b10a2b38ce75e6067045e_u256,
        0xd38451ca1323a9207e4c8b0723cdafa06effd7dee890c7d7e9df22dac048996b_u256,
        0x0a7f30652eaf4c2a892e6efee430d3600118523d0ef734829033ac65579265f8_u256,
        0x09e78d4ef60d05f750f6636209092bc43cbdd6b47e11a9de20a9feb2a50bb96c_u256,
        0xffffffff00000001000000000000000000000001000000000000000000000000_u256,
    },
    {
        // Q = (0, 0).
        0xd6e034185702bf60c28acbbb7c6034707d1b4ae465314ab7a2a3ef2a82a2a9ad_u256,
        0x4319055358e8617b0c46353d039cdaab_u256,
        0x97c7075cb68274e6518c04b3cc57126faebfbfbd37818d61_u256,
        0,
        0,
    },
    {
        // Q off the curve, valid for the curve y² = x³ − 3x + b' through Q.
        0,
        0x39c23c23205f2f7926f1e479465016cb273636b04ae1f78695af59d04a1fc712_u256,
        0x239cb141e2b1f9dc6e095be666d84ff0e9772d49d5a1d464052b42a89589a32b_u256,
        0x4d868294bcf24d0c506802c30960964c735170e11eb604bc19e2bb66445911c8_u256,
        0x9223fbada9f5a0b950c805095c092c99908cd0ac50da1222e583e7dd58f8b1ca_u256,
    },
    {
        // Q = −G, z = r: R is infinity with X = 0.
        0xabe321c612bc3fed9fac9aecf6cba4de7f9997249381184066406128a35f4dfb_u256,
        0xabe321c612bc3fed9fac9aecf6cba4de7f9997249381184066406128a35f4dfb_u256,
        0x5341b3880ce1fbb27f6c12ccbe6e9c9cc04482fddddcd4b74f57cf19790038d7_u256,
        G.x.value(),
        (-G).y.value(),
    },
    {
        // R is infinity after msm() adds −P to P.
        0xd05c1c4af280bb4a0291be4c19464678f9504ebef9645928afc79b9938b244f6_u256,
        0x7d09fe13bc95644e486453594e24ec76c826b3eda58c6d84c9662fb0c0cde624_u256,
        0xef2d48ce206028fd97ae66684c5b21b9e26fb8da18eb13f2e4282d0bdc9aa68d_u256,
        0x9dbdb8f150284f65feb352b2f23efa4b24d57d42e72c8e5a30f0babb0cf5a21c_u256,
        0xebea35058e83b04383289a31e44366708f46de5bc4e9fbb06310f696cf2473b4_u256,
    },
    {
        // x₁ ≥ n, r = x₁ − n + 1.
        0x380847cfe1e1f004b8743e34804614e41f8664e0e4b0519308f834a53a5a49be_u256,
        0x4_u256,
        0x12860f55dbd4e4d17a8b53009ceda895ec269f4cea8ca642b79dc50ce43d3504_u256,
        0xd7a5331db76bc6bc39b419eef111aeb2ce93a06aba0f0f991dc5aee3d3b5ce36_u256,
        0xd0b0df04d6b1a636cabd3ded6df3afffdecc04c6f23755afe2915dfc149d478f_u256,
    },
    {
        // x₁ = 0, r = p − n: r + n ≡ x₁ (mod p), but r + n ≥ p.
        0x24fb632ff0b8c9603e8b6a7f7134bd1f173d4fec0442bdc1fd4ddd6d175d9f8f_u256,
        0x4319055358e8617b0c46353d039cdaae_u256,
        0x1e8fc04717fb006a47290d67318ce4c9afacb60f8a6ee2cea97301002926a8a0_u256,
        0x1ff12615d101b23fe5a80726c40757682454acb303ab1d222cad6eb1d4544a5e_u256,
        0x17a1e9c5297c4ca5b4d41fcbcd47b92aac508a86c4302ec1c5314ec795def763_u256,
    },
    {
        // r = x₁ + 2²⁵⁶ − n: r + n wraps to x₁.
        0x1591e1114247d72f2a99e83785994b53d7443f2efaf3871be62233c8a67bd4c1_u256,
        0x789a8e5f5f540b8082d450910340ed1e76dceda932273e63a08bdce84b187f3b_u256,
        0x5fee5c5abce35463a18571e437487f204c1b84313f5f72d8f98560abcd1a97ff_u256,
        0xdef53fd037dee2f4664d22c2cf0130f42ddefcea766fa70612a75116380bcf47_u256,
        0xc7d51b6caf75078f92b9662850d4f8c458983e829c952a46b67ec56615ddb722_u256,
    },
};
}  // namespace

TEST(secp256r1, valid)
{
    for (size_t i = 0; i < std::size(VALID_INPUTS); ++i)
    {
        const auto& [h_int, r, s, x, y] = VALID_INPUTS[i];
        ethash::hash256 h{};
        be::store(h.bytes, h_int);
        const auto result = verify(h, r, s, x, y);
        EXPECT_TRUE(result) << i;
    }
}

TEST(secp256r1, invalid)
{
    for (size_t i = 0; i < std::size(INVALID_INPUTS); ++i)
    {
        const auto& [h_int, r, s, x, y] = INVALID_INPUTS[i];
        ethash::hash256 h{};
        be::store(h.bytes, h_int);
        const auto result = verify(h, r, s, x, y);
        EXPECT_FALSE(result) << i;
    }
}

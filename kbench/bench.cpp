#include "keccak.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char** argv)
{
    const int reps = argc > 1 ? std::atoi(argv[1]) : 15;
    std::vector<uint8_t> buf(136 * 16 + 64);
    for (size_t i = 0; i < buf.size(); ++i)
        buf[i] = uint8_t(i * 31 + 7);

    uint64_t check = 0;
    for (size_t len = 0; len <= 1000 && len <= buf.size(); ++len)
    {
        const auto h = ethash_keccak256(buf.data(), len);
        for (const auto w : h.word64s)
            check = (check ^ w) * 0x100000001b3;
    }
    std::printf("check %016llx\n", static_cast<unsigned long long>(check));

    // 32 B = 1 permutation; 2176 B (16 blocks) ~= 17 permutations.
    for (const size_t len : {size_t{32}, size_t{64}, size_t{136 * 16}})
    {
        const size_t n = len == 32 || len == 64 ? 400000 : 30000;
        std::vector<double> t;
        uint64_t sink = 0;
        for (int r = 0; r < reps; ++r)
        {
            const auto t0 = std::chrono::steady_clock::now();
            for (size_t k = 0; k < n; ++k)
                sink += ethash_keccak256(&buf[(k + sink) & 31], len).word64s[0];
            t.push_back(
                std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0)
                    .count() /
                double(n));
        }
        std::sort(t.begin(), t.end());
        std::printf("len %4zu  min %7.1f  med %7.1f ns/hash%s\n", len, t[0], t[t.size() / 2],
            sink == 42 ? " " : "");
    }
}

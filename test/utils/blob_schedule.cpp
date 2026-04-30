#include "blob_schedule.hpp"
#include "utils.hpp"
#include <string_view>

namespace evmone::test
{
state::BlobParams get_blob_params(evmc_revision rev) noexcept
{
    if (rev >= EVMC_AMSTERDAM)
        return {14, 21, 11684671};
    if (rev >= EVMC_PRAGUE)
        return {6, 9, 5007716};
    if (rev == EVMC_CANCUN)
        return {3, 6, 3338477};
    return {0, 0, 1};
}

state::BlobParams get_blob_params(evmc_revision rev, const BlobSchedule& blob_schedule)
{
    return get_blob_params(evmc::to_string(rev), blob_schedule, 0);
}

state::BlobParams get_blob_params(
    std::string_view network, const BlobSchedule& blob_schedule, int64_t timestamp)
{
    std::string fork;
    if (network == "PragueToOsakaAtTime15k")
        fork = timestamp >= 15'000 ? "Osaka" : "Prague";
    else if (network == "OsakaToBPO1AtTime15k")
        fork = timestamp >= 15'000 ? "BPO1" : "Osaka";
    else if (network == "BPO1ToBPO2AtTime15k")
        fork = timestamp >= 15'000 ? "BPO2" : "BPO1";
    else if (network == "BPO2ToBPO3AtTime15k")
        fork = timestamp >= 15'000 ? "BPO3" : "BPO2";
    else if (network == "BPO3ToBPO4AtTime15k")
        fork = timestamp >= 15'000 ? "BPO4" : "BPO3";
    else if (network == "BPO2ToAmsterdamAtTime15k")
        fork = timestamp >= 15'000 ? "Amsterdam" : "BPO2";
    else
        fork = network;
    if (const auto it = blob_schedule.find(fork); it != blob_schedule.end())
        return it->second;
    else
        return get_blob_params(to_rev_schedule(network).get_revision(timestamp));
}

}  // namespace evmone::test

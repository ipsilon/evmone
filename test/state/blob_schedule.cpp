#include "blob_schedule.hpp"
#include "../utils/utils.hpp"
#include <cstdint>
#include <string_view>

namespace evmone::state
{

BlobSchedule get_blob_schedule_by_bpo_fork(
    std::string_view network, const BlobScheduleMap& schedules, int64_t timestamp) noexcept
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
    else
        fork = network;
    if (const auto it = schedules.find(fork); it != schedules.end())
        return it->second;
    else
        return get_blob_schedule(test::to_rev_schedule(network).get_revision(timestamp));
}

BlobSchedule get_blob_schedule_by_bpo_fork(
    evmc_revision rev, const BlobScheduleMap& schedules) noexcept
{
    return get_blob_schedule_by_bpo_fork(evmc_revision_to_string(rev), schedules, 0);
}

}  // namespace evmone::state
#ifndef VALWOOD_WORLD_SCENERY_INTERIOR_RESIDENCY_H
#define VALWOOD_WORLD_SCENERY_INTERIOR_RESIDENCY_H
#include <cmath>
#include <cstdint>
namespace world_scenery_interior_residency {
// An opted-in isolated-interior project releases genuinely remote outdoor
// fallbacks on either side of travel. Nearby reused assets keep their policy.
inline bool release_remote_pin(bool domain_enabled,bool apartment,double nearest,double far_range,double unload) noexcept {
    return domain_enabled&&!apartment&&std::isfinite(nearest)&&std::isfinite(far_range)&&
        std::isfinite(unload)&&far_range>=0&&unload>=0&&nearest>far_range+unload;
}
// A retained coverage/shadow tier is a residency proxy, not the selected near
// material quality. Preserve the requested 2K/4K edge on every near model.
// Coverage follows the resolved pool rather than imposing a global Ultra cap:
// this 3 GB GPU keeps 512px proxies, while ample pools retain richer facades.
inline unsigned texture_edge(unsigned requested,unsigned lod,bool shadow=false,
                             std::uint64_t pool=0) noexcept {
    if(lod==0&&!shadow)return requested;
    constexpr std::uint64_t GiB=1024ull*1024*1024;
    const unsigned proxy=pool>=12*GiB?requested:pool>=8*GiB?2048:pool>=4*GiB?1024:512;
    if(!proxy)return requested;
    return requested&&requested<proxy?requested:proxy;
}
inline bool texture_fits(std::uint64_t current,std::uint64_t missing,std::uint64_t cap) noexcept {
    return current<=cap&&missing<=cap-current;
}
// Coverage has precedence over optional color refinement. Otherwise reclaim
// only a genuinely farther refinement, with hysteresis to avoid frame churn.
inline bool refinement_reclaimable(bool coverage_missing,double incoming,double candidate) noexcept {
    return std::isfinite(incoming)&&std::isfinite(candidate)&&
        (coverage_missing||candidate>incoming+8.);
}
inline bool parked_refinement_retry(double distance,double blocker,std::uint64_t current,
                                    std::uint64_t cost,std::uint64_t cap) noexcept {
    return std::isfinite(distance)&&std::isfinite(blocker)&&
        (distance+8.<blocker||(cost&&texture_fits(current,cost,cap)));
}
// A failed image bank can retry once its exact reservation fits again. Never
// bypass the admission guard or retry at the same unchanged resident pressure.
inline bool texture_retry(std::uint64_t current,std::uint64_t failed_at,std::uint64_t missing,std::uint64_t cap) noexcept {
    return missing&&current<failed_at&&current<=cap&&missing<=cap-current;
}
}
#endif

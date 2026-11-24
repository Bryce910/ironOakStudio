#ifndef VALWOOD_WORLD_TEXTURE_POOL_POLICY_H
#define VALWOOD_WORLD_TEXTURE_POOL_POLICY_H
// One loading-boundary admission policy for the shared registered-image pool.
// Driver free-memory queries are approximate headroom hints, not guaranteed
// allocations or a portable measurement of dedicated VRAM. No settings, image
// resolution, material roles, effects or per-frame quality decisions change.
// https://registry.khronos.org/OpenGL/extensions/NVX/NVX_gpu_memory_info.txt
// https://registry.khronos.org/OpenGL/extensions/ATI/ATI_meminfo.txt
#include <algorithm>
#include <array>
#include <cstdint>

namespace world_texture_pool_policy {
constexpr std::uint64_t mib=1024ull*1024,gib=1024ull*mib;
constexpr std::uint64_t maximum_bytes=1024ull*gib;
constexpr unsigned nvx_dedicated=0x9047,nvx_available=0x9049,ati_texture_free=0x87FC;
enum class Source {Unknown,Nvx,Ati};
struct Hints {
    Source source=Source::Unknown;
    std::uint64_t available_bytes=0,dedicated_bytes=0; // dedicated comes only from NVX
};
inline const char *name(Source source){
    return source==Source::Nvx?"NVX current-context":source==Source::Ati?"ATI primary texture pool":"unavailable";
}
inline bool sane(const Hints &hint){
    if(hint.source==Source::Unknown||hint.available_bytes>maximum_bytes||hint.dedicated_bytes>maximum_bytes)return false;
    if(hint.source==Source::Nvx)return hint.dedicated_bytes&&hint.available_bytes<=hint.dedicated_bytes;
    return hint.source==Source::Ati&&(!hint.dedicated_bytes||hint.available_bytes<=hint.dedicated_bytes);
}
// The host checks each extension on the actual current SDL GL context. The
// callback is deliberately injectable so CPU tests verify exact query widths,
// units and unsupported paths without creating a context or issuing GL calls.
template<class GetInteger>
inline Hints query(bool supports_nvx,bool supports_ati,GetInteger get){
    std::uint64_t known_dedicated=0;
    if(supports_nvx){
        int dedicated=-1,available=-1;get(nvx_dedicated,&dedicated);get(nvx_available,&available);
        if(dedicated>0&&std::uint64_t(dedicated)*1024<=maximum_bytes)known_dedicated=std::uint64_t(dedicated)*1024;
        if(dedicated>0&&available>=0){
            const Hints hint{Source::Nvx,std::uint64_t(available)*1024,std::uint64_t(dedicated)*1024};
            if(sane(hint))return hint;
        }
    }
    if(supports_ati){
        // ATI_meminfo writes FOUR integers, and the auxiliary pool may overlap
        // the primary pool. Only primary free[0] is usable here; never add aux.
        std::array<int,4> free{{-1,-1,-1,-1}};get(ati_texture_free,free.data());
        if(free[0]>=0&&free[1]>=0&&free[1]<=free[0]){
            // An unusable NVX free field does not invalidate its independently
            // sane dedicated cap. Reject contradictory ATI primary headroom;
            // the ATI query itself never establishes dedicated capacity.
            const Hints hint{Source::Ati,std::uint64_t(free[0])*1024,known_dedicated};
            if(sane(hint))return hint;
        }
    }
    return {};
}
inline std::uint64_t add(std::uint64_t a,std::uint64_t b){
    return a>maximum_bytes||b>maximum_bytes-a?maximum_bytes:a+b;
}
inline std::uint64_t subtract(std::uint64_t a,std::uint64_t b){return a>b?a-b:0;}
struct Resolution {
    std::uint64_t bytes=0,legacy_bytes=0,registered_start_bytes=0;
    std::uint64_t initial_free_bytes=0,driver_reserve_bytes=0,geometry_reserve_bytes=0;
    Source source=Source::Unknown;
    bool grew=false;
};
inline Resolution resolve(std::uint64_t legacy_bytes,const Hints &hint,
                          std::uint64_t registered_start_bytes,std::uint64_t geometry_reserve_bytes){
    Resolution result;result.bytes=result.legacy_bytes=std::min(legacy_bytes,maximum_bytes);
    result.registered_start_bytes=registered_start_bytes;result.geometry_reserve_bytes=geometry_reserve_bytes;
    if(!sane(hint)||registered_start_bytes>maximum_bytes||geometry_reserve_bytes>maximum_bytes)return result;
    result.source=hint.source;result.initial_free_bytes=hint.available_bytes;
    // Preserve all existing 3GB/unknown/low-headroom behavior byte-for-byte.
    // High hardware gains a larger pool only after an ample supported hint.
    if(hint.available_bytes<=4*gib)return result;
    if(hint.dedicated_bytes&&
       (registered_start_bytes>hint.dedicated_bytes||hint.available_bytes>hint.dedicated_bytes-registered_start_bytes))return result;
    result.driver_reserve_bytes=std::max<std::uint64_t>(512*mib,hint.available_bytes/4);
    // Free already excludes current GL stores. Reserve the full prospective
    // streamed-geometry budget again, conservatively, for later buffer growth.
    const auto additional=subtract(subtract(hint.available_bytes,result.driver_reserve_bytes),geometry_reserve_bytes);
    result.bytes=std::max(result.bytes,add(registered_start_bytes,additional));
    result.grew=result.bytes>result.legacy_bytes;
    return result;
}
}
#endif

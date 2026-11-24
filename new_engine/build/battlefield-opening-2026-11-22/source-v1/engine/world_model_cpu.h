#ifndef VALWOOD_WORLD_MODEL_CPU_H
#define VALWOOD_WORLD_MODEL_CPU_H
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
extern "C" {
#include "engine/texture_cache.h"
}

// This representation owns CPU data only. It has no SDL/GL dependency and may
// be prepared on a worker. The render thread consumes already decoded mip data;
// one upload transaction may consume its cache-key ownership tokens.
namespace world_model_cpu {
struct Vertex {float p[3]={},n[3]={},uv[2]={},tangent[4]={},facade_paint=0,baked_light[4]={};};
static_assert(sizeof(Vertex)==17*sizeof(float),"Static vertex stream has no padding");
static_assert(offsetof(Vertex,baked_light)==13*sizeof(float),"Baked light follows the shared source vertex stream");
struct Level {unsigned width=0,height=0;std::vector<std::uint8_t> rgba;};
struct CacheKey {
    TextureCachePrepared *prepared=nullptr;
    ~CacheKey(){texture_cache_prepared_free(prepared);}
    TextureCachePrepared *take(){auto *p=prepared;prepared=nullptr;return p;}
};
struct Texture {
    std::vector<std::uint8_t> encoded;
    std::vector<Level> levels;
    bool srgb=false,opaque=false,bc7_policy=false,compressed=false;
    unsigned bc7_format=0; // 1 sRGB, 2 linear ARM, 3 angular-certified normal
    unsigned texture_edge=0; // effective loading cap; zero when the original already fits
    unsigned min_filter=0x2703,mag_filter=0x2601,wrap_s=0x2901,wrap_t=0x2901;
    std::uint64_t fingerprint=0;
    std::shared_ptr<CacheKey> cache_key;
};
struct Facade {int mode=0,aux[3]={-1,-1,-1};float scale[3]={1,1,1};};
struct Mesh {
    std::vector<Vertex> vertices;std::vector<std::uint32_t> indices;
    std::array<int,7> textures={{-1,-1,-1,-1,-1,-1,-1}}; // base,normal,ARM,emission,facade0..2
    Facade facade;
    float factor[4]={1,1,1,1},roughness=1,metallic=0,cutoff=0,normal_scale=1;
    float emissive_factor[3]={};
    float winter[3]={}; // snow accumulation, ice response, optical thickness
    float leaf_transmission[4]={0,0,0,-1}; // linear tint RGB, view dependency; -1 disables
    bool baked_light=false,source_paint=false;
    unsigned equipment_mask=0;
    std::array<bool,4> detail_sampler={};
};
struct Asset {
    std::vector<Mesh> meshes;std::vector<Texture> textures;
    std::array<std::shared_ptr<CacheKey>,3> neutral;
    float minimum[3]={},maximum[3]={};
    std::uint64_t cpu_bytes=0,gpu_bytes=0,upload_bytes=0,geometry_bytes=0,texture_bytes=0;
};
struct Options {
    std::uint64_t cpu_bytes=384ull*1024*1024;
    unsigned max_texture_size=8192;
    unsigned max_texture_edge=0; // loading quality cap, independent of the hardware limit
    bool bc7=false; // owner captures BPTC support/settings at loading boundary
    bool reuse_decoder_scratch=false; // source batches release transient decode reservations after each image
    const std::atomic<std::uint64_t> *generation=nullptr;
    const std::atomic<bool> *stopping=nullptr;
    std::uint64_t expected_generation=0;
    bool cancelled()const;
};
struct Bounds {float minimum[3]={},maximum[3]={};};
// Exact transformed/base-aligned source positions only. No image decode or GPU
// work; used to retain permanent collision/grounding metadata for unloaded LODs.
bool bounds_only(const char *path,const Options &,Bounds &,std::string &error);
// Bounded static GLB with embedded images or generated local shared-image URIs,
// with the exact existing source transforms,
// base alignment, material factors, facade UV/paint streams and generated basis.
// Original encoded images remain intact; an explicit loading cap selects
// certified compressed mips or reduces pixels. sRGB mips average in linear light.
// Failure/cancellation returns null and leaves every renderer resource untouched.
std::shared_ptr<const Asset> prepare(const char *path,const Options &,std::string &error);
}
#endif

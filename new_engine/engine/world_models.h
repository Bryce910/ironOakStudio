#ifndef VALWOOD_WORLD_MODELS_H
#define VALWOOD_WORLD_MODELS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct WorldModelAsset WorldModelAsset;
typedef struct WorldModelPoseBlend WorldModelPoseBlend;
typedef struct {float x,y,z,yaw,scale;
#ifdef __cplusplus
 float pitch=0,roll=0,axis_x=0,axis_y=0,axis_z=0; // radians; additive axis factors (0 = identity)
#else
 float pitch,roll,axis_x,axis_y,axis_z;
#endif
} WorldModelInstance;
// Optional character stream. Ordinary scenery instance storage is unchanged.
typedef struct {
    float width,cutoff_offset,alpha_exponent,color_strength;
    float hair_tint[3],hide_hair;
} WorldModelCharacterLook;
WorldModelCharacterLook world_model_character_default(void);
enum {WORLD_MODEL_CHARACTER_HAIR=1,WORLD_MODEL_CHARACTER_MASKED_HAIR=2};
unsigned world_model_character_capabilities(const WorldModelAsset *);
typedef struct {
    bool enabled;
    uint64_t candidates,cached_textures,reference_rgba_bytes,block_bytes;
    uint64_t cache_misses,rejected_cache,upload_failures;
} WorldModelTextureBlockStats;
/* Loading-only precomputed sRGB color and certified linear normal/ARM BC7 blocks; original full-resolution
   RGBA uploads remain the fallback. Pure counters: no GL queries or file I/O. */
WorldModelTextureBlockStats world_model_texture_block_stats(void);
typedef struct {uint64_t opaque_draws,masked_draws;} WorldModelDepthMaterialStats;
WorldModelDepthMaterialStats world_model_depth_material_stats(void);
/* Render-thread scope only. The specified depth program must return before
   reading uBase when uAlphaCutoff<=0, as world_foliage_shadow does. Opaque
   meshes then bind their shared neutral texel instead of unused color maps;
   masked meshes keep their complete original alpha image. Other programs are
   unaffected. Returns the previous program for nested/exception-safe restore.
   No queries, allocation, texture mutation, or resource ownership changes. */
unsigned world_model_depth_material_program(unsigned program);
enum {WORLD_MODEL_POSE_BLEND_VERSION=1, WORLD_MODEL_NEUTRAL_TEXTURE_REFS=3,
      WORLD_MODEL_NEUTRAL_SOURCE_BYTES=24, WORLD_MODEL_NEUTRAL_GPU_BYTES=12};

/* Render-thread only, with a current OpenGL 3.3 context. Loads bounded static
   GLB triangle meshes with embedded images and UV0 (baked texture transforms),
   bakes node transforms, and places the model base at Y=0.
   Keeps authored dimensions and full-resolution material images. Each asset owns
   three shared cache references for complete neutral color/ARM/normal samplers;
   the three keys add24 CPU source bytes/12 logical GPU bytes while any owner lives.
   Missing-map flags remain false. Owners release before destroying their context. */
WorldModelAsset *world_model_load(const char *path);
/* Explicit v1 pose loads share normal material/image ownership, retaining only
   exact topology/UV/paint/tangent-sign keys for safe GPU correspondence checks.
   Missing tangents retain the generated direction; v1 fixes their handedness
   from the dominant oriented UV triangle (deterministic seam/fold fallback).
   Mirrored winding is included; all-degenerate UVs use node mirror parity.
   Explicit authored tangent signs are never changed or relaxed.
   Limited to100000 vertices/300000 indices per pose; ordinary loads keep no
   additional CPU geometry. Unknown versions fail without GL mutation. */
WorldModelAsset *world_model_load_pose(const char *path,unsigned version);
// Viewmodels retain the authored origin through every pose instead of base-aligning each frame.
WorldModelAsset *world_model_load_pose_origin(const char *path,unsigned version);
void world_model_free(WorldModelAsset *asset);
/* Placement regeneration only; one reusable GPU instance buffer and bounded
   96KiB CPU staging reserved at load per asset. Uploads do not allocate CPU
   staging. Production uses exact-sized GL_DYNAMIC_DRAW glBufferData uploads
   on the same buffer ID. QA only: VALWOOD_QA_EXACT_INSTANCE_BUFFERS=0 selects
   explicit GL_STREAM_DRAW orphan/subdata with retained power-of-two capacity
   16..4096 (384 bytes..96 KiB); unset/1 keeps the measured production policy.
   At most4096 finite, positive-scale instances. Invalid input leaves
   the previous uploaded count/data and GL bindings unchanged. XYZ stays in the caller's
   coordinate space; its shader owns camera-relative subtraction. */
bool world_model_upload_instances(WorldModelAsset *asset,const WorldModelInstance *instances,size_t count);
/* Optional rooted placement transitions. A finite fade in [0,1] is required
   for every instance; invalid input preserves the last upload and GL bindings.
   Ordinary uploads restore full coverage and disable the optional attribute. */
bool world_model_upload_fading_instances(WorldModelAsset *asset,const WorldModelInstance *instances,const float *fades,size_t count);
/* Uses caller's linked program and lighting/view uniforms. Mesh attributes:
   0 position vec3, 1 normal vec3, 2 UV vec2, 3 tangent vec4, 7 source paint float.
   Instance attributes: 4 offsetXYZ/scale vec4, 5 yaw float (radians), 6 fade.
   Sets uBase/uNormal/uARM samplers 0/1/2, uHasBase/uHasNormal/uHasARM,
   uBaseFactor vec4, uRoughness/uMetallic, uAlphaCutoff and uInstanceFade bool.
   VALWOOD_materials_facade v1 retains original source UV/paint layers on
   bounded near materials: uFacadeMode, uFacadeUVScale vec3 and source aux
   samplers8/9/10. Ordinary meshes set mode0; draw restores mode0 on return.
   Uniform locations are cached for color/shadow programs. Caller disables face
   culling for foliage, owns depth/blend state and restores its texture unit.
   Base/normal/ARM use texture units0/1/2; emissive uses reserved unit3,
   sRGB decoding and factor times KHR emissive strength. Each mesh sets its
   emissive state; draw return resets emissive enable/factor for procedural draws.
   OPAQUE/default materials ignore alpha with cutoff0; MASK uses its declared
   cutoff. BLEND retains a .4 cutout fallback (no sorted translucency here).
   CPU bounds are available after node baking and base alignment. */
void world_model_draw(WorldModelAsset *asset,unsigned program);
/* Draw only the first min(prefix_count, uploaded instance count) placements.
   Zero is a no-op. Does not upload, reorder or change the stored count; a
   caller can use a visible prefix for color and the full set for shadows. */
void world_model_draw_instances(WorldModelAsset *asset,unsigned program,size_t prefix_count);
bool world_model_bounds(const WorldModelAsset *asset,float minimum[3],float maximum[3]);
size_t world_model_instance_count(const WorldModelAsset *asset);

/* Independent bounded instance batches reference existing A/B pose VBOs and
   the A materials/index buffer. No textures or geometry are copied. Both
   target assets must remain alive until the batch is freed or relinked.
   Strict mesh/index/UV/paint/tangent-sign/material validation precedes any
   relink; incompatible inputs preserve the previous targets and GL state.
   Capacity1..4096, version1. Same targets are a no-op. */
WorldModelPoseBlend *world_model_pose_blend_create(size_t capacity,unsigned version);
bool world_model_pose_blend_set_targets(WorldModelPoseBlend *batch,WorldModelAsset *a,WorldModelAsset *b);
/* Finite weights0..1; validates the whole transaction before mutation.
   Uploads placements/weights only, with reusable CPU staging. The stored
   snapshot is reused by every color/reflection/shadow draw until next upload.
   Uses the same production exact-sized GL_DYNAMIC_DRAW policy as static models;
   batch input capacity is still the original caller's exact declared limit.
   QA only: VALWOOD_QA_EXACT_INSTANCE_BUFFERS=0 at asset/batch creation selects
   the experimental orphan/subdata high-water policy for source-identical A/B. */
bool world_model_pose_blend_upload(WorldModelPoseBlend *batch,const WorldModelInstance *instances,const float *weights,size_t count);
// Loading boundary enables attributes12/13 on an existing pose batch. Upload
// then uses reserved storage; no new mesh, textures, draw groups or hot-path VAOs.
bool world_model_pose_blend_enable_character(WorldModelPoseBlend *);
bool world_model_pose_blend_upload_character(WorldModelPoseBlend *,const WorldModelInstance *,const float *,const WorldModelCharacterLook *,size_t);
// Pure diagnostic counter; no GL query or synchronization.
uint64_t world_model_pose_blend_character_uploads(const WorldModelPoseBlend *);
void world_model_pose_blend_draw(WorldModelPoseBlend *batch,unsigned program);
void world_model_pose_blend_draw_instances(WorldModelPoseBlend *batch,unsigned program,size_t prefix_count);
void world_model_pose_blend_free(WorldModelPoseBlend *batch);

#ifdef __cplusplus
}
#include <memory>
namespace world_model_cpu {struct Asset;}
struct WorldModelStaged;
// Owner-thread incremental GL upload of worker-prepared static geometry/images.
// No image decoding, file IO, source-key allocation or generated mip work occurs
// here. Step uploads at most budget bytes. Texture/buffer allocation may still
// incur driver work, so byte bounds are not a hard wall-clock guarantee.
WorldModelStaged *world_model_stage(std::shared_ptr<const world_model_cpu::Asset>,unsigned anisotropy=4,unsigned facade_anisotropy=8);
bool world_model_stage_step(WorldModelStaged *,std::size_t budget,std::size_t *consumed,bool *complete);
WorldModelAsset *world_model_stage_take(WorldModelStaged *); // only on complete
void world_model_stage_free(WorldModelStaged *);
std::size_t world_model_geometry_bytes(const WorldModelAsset *);
std::size_t world_model_staged_new_texture_bytes(const WorldModelStaged *);
bool world_model_warm(WorldModelAsset *,unsigned program); // owner load/commit boundary
class WorldModelDepthMaterialScope {
    unsigned previous;
public:
    explicit WorldModelDepthMaterialScope(unsigned program):previous(world_model_depth_material_program(program)){}
    ~WorldModelDepthMaterialScope(){world_model_depth_material_program(previous);}
    WorldModelDepthMaterialScope(const WorldModelDepthMaterialScope&)=delete;
    WorldModelDepthMaterialScope &operator=(const WorldModelDepthMaterialScope&)=delete;
};
#endif
#endif

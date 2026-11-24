#ifndef VALWOOD_WORLD_SCENERY_H
#define VALWOOD_WORLD_SCENERY_H

#include <cstddef>
#include <cstdint>
#include "engine/world_authoring_limits.h"

struct WorldScenery;
struct WorldSceneryAlias {const char *name=nullptr;float burial=0,elevation=0,max_scale=4;bool tree=false;const char *stem=nullptr;unsigned lods=1,collision=0,vegetation=0;float distance=0,split=0;bool named_near=false;};
struct WorldSceneryDefinition {const char *name=nullptr,*stem=nullptr;unsigned lods=1,collision=1,vegetation=0;float distance=220,split=60;};
// Owner-thread registry transaction; call only while no frame is rendering.
// Caller restores its previous definitions if scene preparation fails.
bool world_scenery_configure(const WorldSceneryDefinition *,std::size_t,char *error=nullptr,std::size_t capacity=0);
bool world_scenery_alias_bounds(const WorldScenery *,const char *,float minimum[3],float maximum[3]);
// Temporary editor-only LOD preview for one committed record within128m.
// SIZE_MAX/-1 restores normal distance selection; no authored data changes.
void world_scenery_preview(WorldScenery *,std::size_t record,int lod);
std::size_t world_scenery_alias_count();
// Owner-thread catalog generation. Includes successful same-count replacements
// and rollback publications; failed transactions leave the generation intact.
std::uint64_t world_scenery_alias_revision() noexcept;
WorldSceneryAlias world_scenery_alias(std::size_t);
struct WorldSceneryMaterial {const char *alias=nullptr;float tint[3]={1,1,1},roughness=1,normal=1,uv=1;};
void world_scenery_materials(WorldScenery *,const WorldSceneryMaterial *,std::size_t);
void world_scenery_graphs(WorldScenery *,const char *const *,std::size_t);
void world_scenery_surfaces(WorldScenery *,const char *const *,std::size_t);
struct WorldSceneryPlacement {
    const char *alias = nullptr;
    double x = 0, z = 0;
    float y = 0, yaw = 0, scale = 1, burial = 0, elevation = 0, offset_y=0;
    float minimum[3] = {}, maximum[3] = {};
    bool rigid = false, foundation = false, tree = false;
    std::size_t source_file = 0;
    float pitch=0,roll=0,sx=1,sy=1,sz=1; // radians and axis multipliers
    std::size_t source_record = 0,source_line = 0; // ordinal within file / text line
};
// CPU-only authoring query using the renderer's exact asset support policy.
// Returns the normal pivot with offset_y=0. Angles in Placement are radians.
using WorldSceneryGroundHeight=double(*)(double,double,const void *);
bool world_scenery_grounded_pivot(const WorldScenery *,const WorldSceneryPlacement &,
                                 WorldSceneryGroundHeight,const void *,double &);
struct WorldSceneryStats {
    std::size_t source_instances = 0, source_trees = 0, source_props = 0;
    std::size_t source_files = 0, source_aliases = 0, source_colliders = 0;
    std::size_t near_trees = 0, mid_trees = 0, far_trees = 0, props = 0;
    std::size_t reused_groups = 0; /* Last prepare reused existing immutable GPU geometry. */
    std::size_t loaded_groups = 0, largest_batch = 0, uploads_this_update = 0;
    /* Latest committed batches: color includes main/mirrored visibility;
     * shadows retain the full existing distance-selected caster set. */
    std::size_t color_instances = 0, shadow_instances = 0;
    std::size_t editor_preview_instances = 0;
    std::uint64_t editor_preview_updates = 0;
    std::uint64_t uploads_total = 0;
    std::size_t shadow_proxy_groups = 0; /* Independent near-group mid-mesh VBOs. */
};

struct WorldScenerySourceStats {
    const char *path = nullptr; /* owned by scene, valid until free */
    std::size_t instances = 0, trees = 0, props = 0, aliases = 0, colliders = 0;
};

/* Render-thread GL3.3 API. Six-column records, with an optional seventh elevation:
 * alias map_u map_v yaw_degrees scale burial_meters [elevation_meters]
 * Comments begin with #. Alias filenames are fixed in the module; the file
 * cannot choose model paths. At most 8192 total records, 4096 per alias,
 * 256 characters per line and 256KiB per source. Per-model buffers retain their
 * existing 4096-instance bound; all scene/batch staging is reserved at load.
 * All referenced assets/LODs must load; failure frees partial resources and
 * reports the line/asset in the optional caller-owned error buffer.
 * Positive elevation up to 3m is allowed only on small stackable starting props.
 * Tables/imported barrels allow at most5cm floor support, stumps10cm including
 * the existing5cm rigid-ground contact offset. Other zero bounds stay zero.
 * Architecture uses foundation-root grounding; platform tops sit 2cm above the
 * terrain minus burial. Gate piers preserve their measured doorway.
 * Atlas must already be initialized. Tree/fern Y uses the root height; rigid
 * props bake lowest terrain support over rotated/scaled actual bounds (9x5
 * samples for cliffs, 5x5 otherwise) minus burial and 5cm contact slack.
 * Load validates dry roots including local pools, scale, burial and a clear
 * authored trail surface.
 */
WorldScenery *world_scenery_load(const char *instances_path, const char *models_directory = "models",
                                char *error = nullptr, std::size_t error_capacity = 0);
/* Merge at most ten authored files into one transaction and shared groups.
 * Duplicate alias/root/elevation records, the 8192-record cap and 4096-per-alias
 * bound apply across all inputs. Intentional stacks may share roots with
 * distinct elevations. */
WorldScenery *world_scenery_load_files(const char *const *instances_paths, std::size_t file_count,
                                      const char *models_directory = "models", char *error = nullptr,
                                      std::size_t error_capacity = 0);
/* Editing transaction. Matching alias/LOD geometry is borrowed from current;
 * new aliases load normally. Current must remain alive and unmodified. Bounds
 * and collisions may be inspected, but prepared scenes cannot update/draw.
 * Freeing an uncommitted prepare releases only its new assets. After all other
 * edit preparation succeeds, commit transfers ownership without GL uploads;
 * current must then be retired/freed. A wrong owner fails without changing either.
 * Reimporting changed model files must use ordinary load_files, not prepare. */
WorldScenery *world_scenery_prepare_files(WorldScenery *current,const char *const *paths,std::size_t count,
    const char *models="models",char *error=nullptr,std::size_t capacity=0);
bool world_scenery_commit_prepared(WorldScenery *prepared,WorldScenery *current);
WorldScenerySourceStats world_scenery_source_stats(const WorldScenery *, std::size_t index);
/* Continuous horizontal capsule sweep, with feet/head vertical overlap. No
 * allocations, atlas/GL calls or mutable state. Trees collide at their trunks;
 * solid props use rotated footprints. Gate piers/header preserve the measured
 * doorway. Initially overlapping motion may only escape or retain penetration.
 * Root may retry X/Z axes for sliding after a blocked full move.
 * Null scene means no scenery; non-finite/invalid capsule inputs are rejected. */
bool world_scenery_can_move(const WorldScenery *, double from_x, double from_z,
                            double to_x, double to_z, float feet_y,
                            float player_height = 1.75f, float radius = .32f);
/* Allocation-free after load. Selection loops over the bounded authored file;
 * 128m camera-relative anchors retain precision, and unchanged batches never
 * upload again. Yaw/pitch are radians, matching the viewer. Visible instances sort front-to-back by root XZ distance with a deterministic
 * position tie-break and no additional buffers. Near shadow casters remain
 * selected off screen in an unchanged shadow-only suffix. Color uses a conservative
 * sphere against main/sea-mirrored perspective frusta (vertical FOV 1.08rad),
 * plus the raised local-pool reflection while within its 220m render radius.
 * Aspect must match the render target; unchanged partitions do not upload.
 * tan_vertical_half_fov=0 preserves the normal tan(.54) selection exactly.
 * An explicit positive tangent supports other projections; cube-map capture
 * passes 1 for 90 degree faces with aspect 1. It changes color selection only;
 * the same original distance-selected shadow caster set remains resident.
 */
void world_scenery_update(WorldScenery *, double camera_x, double camera_y, double camera_z,
                          float yaw, float pitch, float aspect = 16.f / 9.f,
                          float tan_vertical_half_fov = 0.f);
/* Caller sets VP/lighting/texture units7/11 and raster state. Sets only model
 * instance/material and vegetation uniforms; caches locations for two programs.
 * Shadows omit far cards and low plants; the two showcase shrubs explicitly
 * cast masked leaf shadows. Reflection may reuse color draw with its
 * mirrored camera; selection includes both sea and nearby raised-pool views.
 * Showcase prop LODs overlap by 4m either side of their split and use the
 * complementary uPropLodRange shader interval; this uniform resets after draw.
 * Purchased mesh pairs (including two-LOD trees) use that same interval and
 * fade at the end of their range through uSceneryDistanceFade. Both uniforms
 * reset after every filtered/full draw so procedural vegetation is unaffected.
 * Color groups submit front-to-back by their nearest committed visible source
 * bound, including actual scale/height and the current draw camera. This fixed
 * index indirection does not reorder owned groups or shadow submissions, add
 * uploads, or change visibility/materials. Equal bounds use alias/LOD ties;
 * filtered calls retain the corresponding relative order without sticky state.
 */
void world_scenery_draw(WorldScenery *, unsigned int program, double camera_x, double camera_y,
                        double camera_z, bool shadow = false);
/* Same committed batches/materials as draw(), selected per call by exact alias.
 * Null only_alias selects all; null exclude_alias excludes none. Both filters
 * may apply. Selection never persists into subsequent color/shadow draws. */
void world_scenery_draw_filtered(WorldScenery *, unsigned int program, double camera_x, double camera_y,
                                 double camera_z, bool shadow, const char *only_alias, const char *exclude_alias);
WorldSceneryStats world_scenery_stats(const WorldScenery *);
/* Read-only authoring/QA inspection of the grounded pose and actual first-LOD
 * model bounds. No allocations or GL calls; alias points to static storage. */
bool world_scenery_placement(const WorldScenery *, std::size_t index, WorldSceneryPlacement *);
// Transient editor transforms replace only the existing instance stream. All
// angles are radians; y is the mesh's world-space pivot. The batch is atomic,
// allocation-free after load, and affects color, shadows, LOD and streaming.
// Static collision and authored records stay committed until Apply. Clearing
// restores the original poses exactly; clear before preparing a replacement.
struct WorldSceneryEditorTransform {
    std::size_t record=0;
    double x=0,z=0;
    float y=0,yaw=0,scale=1,pitch=0,roll=0,sx=1,sy=1,sz=1,offset_y=0;
};
bool world_scenery_editor_transforms(WorldScenery *,const WorldSceneryEditorTransform *,std::size_t count);
void world_scenery_editor_clear(WorldScenery *);
// Commit level translation/yaw in place, retaining model buffers and updating
// existing collision boxes. Rejects shape changes, unsupported architecture,
// invalid dry/trail support and any pose that would differ after reload.
// On failure the transient preview may remain; committed collision is intact.
bool world_scenery_editor_commit_moves(WorldScenery *,const WorldSceneryEditorTransform *,std::size_t count);
void world_scenery_free(WorldScenery *);
struct WorldSceneryStreamingConfig {
    std::uint64_t cpu_bytes=512ull*1024*1024;
    std::uint64_t geometry_bytes=192ull*1024*1024,texture_bytes=1536ull*1024*1024;
    std::uint64_t upload_bytes_per_frame=2ull*1024*1024;
    float preload_meters=256,unload_meters=384;
    // Project opt-in only: an isolated interior may release unrelated city
    // fallbacks while inside this volume. Outdoor policy remains unchanged.
    bool interior_release_city_fallbacks=false,interior_start_active=false;
    double interior_minimum[3]={},interior_maximum[3]={};
};
struct WorldSceneryStreamingStats {
    bool enabled=false;std::size_t resident=0,wanted=0,loading=0,failed=0;
    std::size_t critical_pending=0; // immediate near demand; excludes failed fallback
    // Logical geometry budget usage: resident buffers + pending reservations.
    // This is not an exact driver/VRAM allocation measurement.
    std::uint64_t geometry_bytes=0,texture_bytes=0,upload_bytes=0,loads=0,evictions=0,cancellations=0;
    std::size_t fallback_groups=0;char error[192]={};
    bool interior_city_suspended=false;
    std::size_t interior_city_groups=0;
    std::uint64_t interior_policy_changes=0;
};
// Streamed startup keeps exact source position bounds/colliders but omits distant
// near meshes and their shadow proxies before any GPU allocation. Their embedded
// images are decoded asynchronously only on approach. Fallback LODs still load
// at the normal loading boundary. The original eager APIs remain unchanged.
WorldScenery *world_scenery_load_files_streamed(const char *const *paths,std::size_t count,
    const char *models,double initial_camera_x,double initial_camera_z,
    const WorldSceneryStreamingConfig &config={},char *error=nullptr,std::size_t capacity=0);
// Loading/Apply boundary only. Takes ownership of current GPU model handles;
// geometry is evicted outside draw distance + hysteresis and restored by one
// CPU worker with byte-bounded owner uploads. Mid/far/single-LOD fallback assets,
// source bounds and every authored collider always remain available.
bool world_scenery_streaming(WorldScenery *,bool,const WorldSceneryStreamingConfig &config={},char *error=nullptr,std::size_t capacity=0);
WorldSceneryStreamingStats world_scenery_streaming_stats(const WorldScenery *);
// Allocation-free runtime suppression and root pose; indices are the immutable
// committed source order reported by world_scenery_placement. Suppression affects
// both rendering and static collision. Full quaternion props use their runtime
// renderer and suppress this original; this static path intentionally keeps yaw.
bool world_scenery_runtime_active(WorldScenery *,std::size_t record,bool active);
bool world_scenery_runtime_pose(WorldScenery *,std::size_t record,double x,float feet_y,double z,float yaw);
bool world_scenery_find_source_record(const WorldScenery *,std::size_t source_file,std::size_t source_record,std::size_t *record);

#endif

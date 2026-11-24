#ifndef VALWOOD_WORLD_ATLAS_H
#define VALWOOD_WORLD_ATLAS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Global meters, east +X / south +Z. UVs refer to the full reference image:
 * x=(u-.5)*width, z=(v-.5)*depth. The authored Seldonne scene stays separate. */
typedef enum {
    WORLD_ATLAS_OCEAN=0,
    WORLD_ATLAS_MASHITO,
    WORLD_ATLAS_VALWOOD,
    WORLD_ATLAS_OLD_TANTHOON,
    WORLD_ATLAS_KERONE,
    WORLD_ATLAS_BALDURNE,
    WORLD_ATLAS_WALLOWS,
    WORLD_ATLAS_CROSSWIND,
    WORLD_ATLAS_REGION_COUNT
} WorldAtlasRegion;

typedef struct {
    float height;             /* terrain bed/elevation; connected water is Y=0 */
    int region;
    float forest_density;     /* 0..1, placement suitability rather than count */
    float moisture;           /* 0..1 */
    float shore_distance;     /* signed meters: positive dry, negative water */
    float land_weight;        /* smooth 0..1 coastal transition */
} WorldAtlasSample;

typedef struct {
    const char *id;
    const char *name;
    double u,v,x,z;
} WorldAtlasLandmark;

/* Three authored, bounded detail districts. These rectangles can be baked into
 * texture-array material masks; coordinates use the same global meters as tiles. */
typedef struct {
    const char *id,*name;
    double xmin,zmin,width,depth;
} WorldAtlasDistrict;

typedef struct {
    float height;             /* actual atlas elevation, including local detail */
    float trail,rock,meadow,moss; /* independent 0..1 material/placement weights */
} WorldAtlasLandscapeSample;

typedef struct {
    double x,z;
    float half_width;         /* trail's clear walking surface, in meters */
} WorldAtlasTrailPoint;

/* Compact starting-area overlay: 513 squared cached cells. Sampling performs
 * four reads, with no route/pad scans. The same masks drive ecology and shading. */
typedef struct {
    double height_delta,path,paving,clearing,grass_exclusion;
} WorldAtlasStartingSample;
WorldAtlasStartingSample world_atlas_starting_sample(double x,double z);
void world_atlas_starting_bounds(double *xmin,double *zmin,double *width,double *depth);

/* Compact Valdoa village overlay, centered at map UV(.487,.320). The core
 * terrace is Y24m; all height/material/ecology edits end at center +/-280m.
 * Cached 513-square masks share StartingSample's named fields. height_delta
 * is reserved/zero: the height query applies the exact analytic terrace. */
typedef WorldAtlasStartingSample WorldAtlasVillageSample;
void world_atlas_village_origin(double *x,double *z);
void world_atlas_village_bounds(double *xmin,double *zmin,double *width,double *depth);
float world_atlas_village_weight(double x,double z);
WorldAtlasVillageSample world_atlas_village_sample(double x,double z);

/* Old Tanthoon camp: bounded cached path/clearing masks on the original natural
 * heightfield. Outside its authored bounds all fields are exactly zero. */
WorldAtlasStartingSample world_atlas_bandit_sample(double x,double z);
void world_atlas_bandit_bounds(double *xmin,double *zmin,double *width,double *depth);

/* Idempotent. Builds a bounded macro atlas once; sampling has no allocations,
 * file access, polygon traversal or GL calls. Call init before worker threads. */
bool world_atlas_init(void);
void world_atlas_shutdown(void);
double world_atlas_width(void);
double world_atlas_depth(void);
double world_atlas_walk_speed(void);
void world_atlas_uv_to_world(double u,double v,double *x,double *z);
void world_atlas_world_to_uv(double x,double z,double *u,double *v);
/* Editor commits are owner-thread transactions. Join tile workers before
 * replacing this callback or its immutable data. NULL preserves base terrain. */
typedef float (*WorldAtlasHeightEdit)(double x,double z,const void *data);
void world_atlas_set_height_edit(WorldAtlasHeightEdit,const void *data);
/* Optional project-scale immutable terrain, loaded after atlas initialization
 * and before starting workers. File format is documented in world_height_region.h.
 * Returns 1 loaded, 0 missing, -1 invalid/read/allocation error. Missing/failed
 * loads preserve the previous region. External absolute heights replace built-in
 * base terrain inside their closed rectangle, before Studio height deltas.
 * Join sampling workers before loading/clearing. Shutdown also clears it. */
int world_atlas_load_height_region(const char *path,char *error,size_t capacity);
void world_atlas_clear_height_region(void);
typedef struct {
    unsigned columns,rows;
    double xmin,zmin,stepx,stepz;
    size_t bytes;
} WorldAtlasHeightRegionInfo;
bool world_atlas_height_region_info(WorldAtlasHeightRegionInfo *);
/* Optional surrounding terrain. The primary authored region wins everywhere
 * it covers; this field only replaces the remaining built-in base. Its load,
 * lifetime, closed bounds and Studio-after-base order match the primary field.
 * This preserves the original source city's resolution and samples exactly. */
int world_atlas_load_fallback_height_region(const char *path,char *error,size_t capacity);
void world_atlas_clear_fallback_height_region(void);
bool world_atlas_fallback_height_region_info(WorldAtlasHeightRegionInfo *);
float world_atlas_base_height(double x,double z);
WorldAtlasSample world_atlas_sample(double x,double z);
WorldAtlasSample world_atlas_sample_uv(double u,double v);
/* Old Tanthoon rainforest ecology, independent of the stable sample ABI.
 * UV center(.235,.670), outer radii(.065,.085); 1-smooth(.36,1,q*q),
 * with a full core to60% radius and smooth falloff to zero at the edge.
 * Only dry Old Tanthoon contributes;
 * shores fade from120 to360m and all existing settlement cores lie outside.
 * Init first. Finite0..1, deterministic, allocation-free bounded sampling. */
float world_atlas_rainforest_weight(double x,double z);
/* Six-point rainforest path, east/south offsets from the biome center.
 * Same allocation-free mask is merged into landscape_sample/mask().trail:
 * full clear half-width1.6m, smoothly fading to zero by2.8m. A fixed AABB
 * rejects other regions before evaluating the five segments. No height edit. */
const WorldAtlasTrailPoint *world_atlas_rainforest_trail_points(size_t *count);
float world_atlas_rainforest_trail_mask(double x,double z);
/* One authored raised freshwater pool. Bounds are conservative water extents;
 * bank feather is sculpted into the SAME height field used for collision/tiles.
 * No connected shoreline, region, atlas scale or WorldAtlasSample ABI changes. */
typedef struct {
    double x,z,radius_x,radius_z,surface_y,bank_feather;
} WorldAtlasPool;
const WorldAtlasPool *world_atlas_rainforest_pool(void);
/* True only over actual submerged bed, not the conservative ellipse. Nullable
 * outputs are zeroed on false. Init first; finite bounded, allocation-free. */
bool world_atlas_local_water(double x,double z,float *surface_y,float *depth);
// Owner must join terrain workers before replacing an immutable edit document.
typedef bool (*WorldAtlasWaterEdit)(double,double,float *,float *,const void *);
void world_atlas_set_water_edit(WorldAtlasWaterEdit,const void *data);
float world_atlas_height(double x,double z);
/* Walking floor includes the lowered castle drawbridge. Terrain/streaming and
 * water retain the real excavated bed; only actors/camera use this surface. */
float world_atlas_walk_height(double x,double z);
/* Unperturbed reference-map shoreline for authoring/regression diagnostics.
 * Effective shore_distance may differ by at most8m inside two local shore
 * patches; runtime terrain, collision, ecology and water use the effective one. */
float world_atlas_reference_shore_distance(double x,double z);
const WorldAtlasLandmark *world_atlas_landmarks(size_t *count);
const char *world_atlas_region_name(int region);
const WorldAtlasDistrict *world_atlas_districts(size_t *count);
/* No allocations or feature traversal for height/material fields. Trail distance
 * evaluates a fixed eight-segment authored path, so its 3–4 m surface survives
 * the coarser terrain/mask grids. Outside districts all four weights are zero. */
WorldAtlasLandscapeSample world_atlas_landscape_sample(double x,double z);
/* Weights only, with height=0. Reuse an existing atlas sample without a second
 * height/noise query; identical trail/material weights to the combined call. */
WorldAtlasLandscapeSample world_atlas_landscape_mask(double x,double z);
const WorldAtlasTrailPoint *world_atlas_trail(size_t *count);

#ifdef __cplusplus
}
#endif

#endif

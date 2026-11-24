#ifndef VALWOOD_WORLD_COVER_H
#define VALWOOD_WORLD_COVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "engine/world_models.h"
#include "engine/world_ecology.h"

#ifdef __cplusplus
extern "C" {
#endif

// Full near geometry remains close to the player; matching mid meshes fade
// over twenty metres without changing roots, canopy density or shadow casters.
#define WORLD_TREE_NEAR_FADE_BEGIN 44.f
#define WORLD_TREE_NEAR_FADE_END 64.f

#define WORLD_COVER_GRASS_CAP 46000u
#define WORLD_COVER_TREE_CAP 7000u
#define WORLD_COVER_ROCK_CAP 420u /* AAA merge: was 180 */
#define WORLD_COVER_FERN_CAP 360u
// Legacy capacities above remain the default for offline/source consumers.
// Runtime quality tiers use these bounded maxima for allocation and uploads.
#define WORLD_COVER_GRASS_MAX_CAP 92000u
#define WORLD_COVER_FERN_MAX_CAP 3600u
#define WORLD_COVER_GRASS_VISIBLE_RADIUS 90.0
#define WORLD_COVER_GRASS_PROTECTED_RADIUS 30.0
#define WORLD_COVER_TREE_PROTECTED_RADIUS 320.0

typedef enum {
    WORLD_COVER_GRASS=0,
    WORLD_COVER_TREES,
    WORLD_COVER_ROCKS,
    WORLD_COVER_FERNS,
    WORLD_COVER_KIND_COUNT
} WorldCoverKind;
typedef struct WorldCover WorldCover;
typedef struct {
    WorldCoverKind kind;
    uint64_t generation;
    double anchor_x,anchor_z;
    size_t count;
    /* X/Z are local to the double anchor; Y is absolute atlas elevation.
     * The pointer remains valid through update/pump and until the next
     * SUCCESSFUL poll of this same kind, or shutdown. Never free this array. */
    const WorldModelInstance *instances;
} WorldCoverField;
typedef struct {
    size_t near_done,near_total,tree_done,tree_total;
    uint64_t near_generation,tree_generation,cancelled_generations;
    bool near_building,tree_building;
} WorldCoverMetrics;

/* CPU only; atlas must be initialized and remain immutable until shutdown.
 * No background thread or GL calls. Create/update/pump/poll on one owner thread.
 * Grass/rocks/ferns anchor every24m, trees every128m; old published arrays remain
 * available while replacements are staged. Grass has a small coverage guard
 * beyond90m for camera movement between near-anchor changes. On overflow, roots
 * inside the protected radius of their generation anchor precede outer roots;
 * each tier uses a stable world-seed priority, never scan-order sampling. */
WorldCover *world_cover_init(void);
void world_cover_shutdown(WorldCover *cover);
/* Owner-thread mask, evaluated only while staging roots. Data must stay
 * immutable through pump. Invalidate stages after changing a committed edit. */
typedef float (*WorldCoverFilter)(int kind,double x,double z,const void *data);
void world_cover_set_filter(WorldCover *,WorldCoverFilter,const void *data);
/* Regions are validated and COPIED atomically. Callback context remains
 * immutable through pump; true excludes a root for paths/foundations. Explicit
 * regions replace baseline ecology, including zero-density clearings. In
 * authored_only mode no baseline roots are generated outside the circles.
 * Invalid input preserves both the previous configuration and live fields. */
typedef bool (*WorldCoverProtected)(int kind,double x,double z,const void *data);
bool world_cover_set_regions(WorldCover *,const WorldCoverRegion *,size_t count,
                            bool authored_only,WorldCoverProtected,const void *data);
int world_cover_region_preset(const WorldCoverRegion *,size_t count,double x,double z);
void world_cover_invalidate(WorldCover *);
/* Density tier0..3 (Low, Medium, High, Ultra), shared with the graphics menu.
 * Only noncolliding grass/fern budgets change. Trees and solid rocks retain
 * identical roots, scales and collision proxies at every tier. The same
 * candidate pool and priority order makes lower-tier fields subsets of higher
 * tiers at a fixed anchor. Rebuilds are bounded and keep published arrays live
 * until replacement polling. Invalid tiers leave all current work intact. */
bool world_cover_set_quality(WorldCover *,int tier);
/* Grass/fern search radii only; bounded work, root caps and physical roots retained. */
bool world_cover_set_detail_scale(WorldCover *,float scale);
/* Invalid/nonfinite or implausibly large coordinates leave current work intact. */
bool world_cover_update(WorldCover *cover,double camera_x,double camera_z);
/* Processes at most min(candidate_budget,4096) candidates. Legacy candidates
 * need one atlas sample; authored candidates use bounded local slope samples.
 * Authored work is further limited to1024 candidates per pump. Never loops to
 * completion, and retains the previous published fields during staging. */
size_t world_cover_pump(WorldCover *cover,size_t candidate_budget);
bool world_cover_poll(WorldCover *cover,WorldCoverField *field);
void world_cover_metrics(const WorldCover *cover,WorldCoverMetrics *metrics);

#ifdef __cplusplus
}
#endif
#endif

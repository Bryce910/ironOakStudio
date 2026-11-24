#ifndef VALWOOD_WORLD_TILES_H
#define VALWOOD_WORLD_TILES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* CPU-only terrain streaming. No OpenGL calls are made by this module. */
#define WORLD_TILES_CAPACITY 256u
#define WORLD_TILES_DEFAULT_CELLS 32u
#define WORLD_TILES_DEFAULT_LODS 7u
#define WORLD_TILES_DEFAULT_SIZE 128.0

typedef struct { int64_t x, z; unsigned lod; } WorldTileKey;
typedef struct { float x, y, z, nx, ny, nz; } WorldTileVertex;
typedef float (*WorldTileHeightFn)(double x, double z, void *userdata);

typedef struct {
    WorldTileHeightFn height;
    void *userdata;             /* immutable until pause or shutdown joins active work */
    double base_tile_size;     /* zero uses 128 m */
    unsigned cells;            /* zero uses 32; supported range 2..64 */
    unsigned lod_levels;       /* zero uses 7; supported range 1..7 */
    float skirt_depth;         /* near skirts: zero=24 m, range (0,30]; LOD>=5 uses >=96 m */
} WorldTilesConfig;

typedef struct {
    WorldTileKey key;
    unsigned slot;             /* stable while this desired record is retained */
    uint64_t generation;       /* compare BOTH slot and generation before upload */
    double origin_x, origin_z;
    float size, min_y, max_y;
    unsigned cells;
    size_t vertex_count, index_count;
    WorldTileVertex *vertices; /* local X/Z; absolute terrain Y, including skirts */
    uint32_t *indices;         /* CCW triangles viewed from above */
} WorldTileMesh;

typedef struct {
    WorldTileKey key;
    unsigned slot;
    uint64_t generation;
    bool leaf;                 /* desired draw leaf; nonleaves are fallback parents */
    bool generated;            /* CPU complete, NOT an acknowledgement of GL upload */
} WorldTileRequest;

typedef struct {
    size_t desired, leaves, queued, building, completed, delivered;
    size_t cpu_bytes, peak_cpu_bytes; /* excludes meshes transferred to caller */
    uint64_t generated_total, cancelled_total, failed_total, updates;
} WorldTilesMetrics;

typedef struct WorldTilesWorld WorldTilesWorld;

WorldTilesWorld *world_tiles_init(const WorldTilesConfig *config);
/* Cheap no-op while the camera remains in its base tile. Invalid coordinates
 * return false and leave the previous desired tree intact. Allocation failures
 * during asynchronous generation are reported in failed_total. */
bool world_tiles_update(WorldTilesWorld *world, double camera_x, double camera_z);
/* Copies at most capacity desired requests, coarse first. Returns TOTAL desired
 * count, including when out==NULL. Renderer keeps all these records resident.
 * Starting from the coarse roots, replace a parent only once ALL FOUR immediate
 * children are uploaded, then recurse. This avoids mixed parent/child overlap. */
size_t world_tiles_desired(WorldTilesWorld *world, WorldTileRequest *out, size_t capacity);
bool world_tiles_is_current(WorldTilesWorld *world, unsigned slot, uint64_t generation);
/* Transfers ownership of one completed mesh to caller, coarse first. A result
 * can become stale on the next update; check is_current immediately before GL
 * upload. Every successful poll requires release, including stale results. */
bool world_tiles_poll_completed(WorldTilesWorld *world, WorldTileMesh *out);
void world_tiles_release(WorldTileMesh *mesh);
void world_tiles_metrics(WorldTilesWorld *world, WorldTilesMetrics *out);
/* Stops new work and waits for the in-flight tile to finish. After true is
 * returned no height callbacks run until resume, so the owning thread can
 * commit or roll back the height/context data without discarding cached tiles.
 * Idempotent, single-owner fence; each pause transaction ends with resume. */
bool world_tiles_pause(WorldTilesWorld *world);
/* Requires a successful pause. Invalidates only desired tiles whose closed
 * footprint (including the normal-sampling stencil) meets these finite ordered
 * world bounds. Affected slots get new generations and queue for regeneration;
 * unaffected requests and completed meshes remain intact. Invalid input or an
 * unpaused worker returns zero without changing records. Already-polled meshes
 * still belong to the caller, which must check their generation before upload. */
size_t world_tiles_invalidate_bounds(WorldTilesWorld *world,
                                    double xmin, double zmin, double xmax, double zmax);
void world_tiles_resume(WorldTilesWorld *world);
/* Cancels queued work and invalidates all slots, without destroying the worker.
 * A subsequent update starts a new tree even at the same camera coordinates. */
void world_tiles_reset(WorldTilesWorld *world);
/* Caller releases already-polled meshes. Must precede atlas/context teardown. */
void world_tiles_shutdown(WorldTilesWorld *world);
bool world_tile_key_equal(WorldTileKey a, WorldTileKey b);
WorldTileKey world_tile_parent(WorldTileKey key);

#endif

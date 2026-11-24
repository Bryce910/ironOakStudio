#include "world_tiles.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* Six-by-six coarse coverage, then nine central parents refined at each level.
 * Seven levels require 36 + 6*36 = 252 records (198 draw leaves). Three parents
 * per axis leave a full tile margin between successive refinement boundaries,
 * so touching leaves never differ by more than one LOD, even at negative keys.
 * Retained ancestors make fallback independent of asynchronous GL upload. */
#define DESIRED_MAX (36u + 36u * (WORLD_TILES_DEFAULT_LODS - 1u))

typedef enum { TILE_EMPTY, TILE_QUEUED, TILE_BUILDING, TILE_READY, TILE_DELIVERED, TILE_FAILED } TileState;
typedef struct {
    WorldTileKey key;
    uint64_t generation;
    TileState state;
    bool desired, leaf, cancelled;
    double priority;
    WorldTileMesh mesh;
} TileRecord;

struct WorldTilesWorld {
    WorldTilesConfig config;
    pthread_t worker;
    pthread_mutex_t mutex;
    pthread_cond_t wake;
    bool stop, paused, worker_busy, camera_valid;
    int64_t camera_tile_x, camera_tile_z;
    TileRecord records[WORLD_TILES_CAPACITY];
    unsigned desired_slots[DESIRED_MAX];
    size_t desired_count, cpu_bytes, peak_cpu_bytes;
    uint64_t next_generation, generated_total, cancelled_total, failed_total, updates;
};

typedef struct { WorldTileKey key; bool leaf; double priority; } DesiredTile;
typedef enum { BUILD_OK, BUILD_CANCELLED, BUILD_FAILED } BuildResult;

bool world_tile_key_equal(WorldTileKey a, WorldTileKey b) {
    return a.x == b.x && a.z == b.z && a.lod == b.lod;
}

/* C signed division truncates toward zero; hierarchy coordinates need floor. */
static int64_t floor_half(int64_t value) {
    return value / 2 - (value < 0 && value % 2 != 0);
}

WorldTileKey world_tile_parent(WorldTileKey key) {
    WorldTileKey parent = {floor_half(key.x), floor_half(key.z), key.lod + 1u};
    return parent;
}

static size_t mesh_bytes(const WorldTileMesh *mesh) {
    return mesh->vertex_count * sizeof(*mesh->vertices) + mesh->index_count * sizeof(*mesh->indices);
}

void world_tiles_release(WorldTileMesh *mesh) {
    if (!mesh) return;
    free(mesh->vertices);
    free(mesh->indices);
    memset(mesh, 0, sizeof(*mesh));
}

static void release_record_mesh(WorldTilesWorld *world, TileRecord *record) {
    if (record->mesh.vertices || record->mesh.indices) {
        world->cpu_bytes -= mesh_bytes(&record->mesh);
        world_tiles_release(&record->mesh);
    }
}

static double tile_size(const WorldTilesWorld *world, unsigned lod) {
    return ldexp(world->config.base_tile_size, (int)lod);
}

static int desired_compare(const void *left, const void *right) {
    const DesiredTile *a = left, *b = right;
    if (a->key.lod != b->key.lod) return a->key.lod > b->key.lod ? -1 : 1;
    if (a->priority != b->priority) return a->priority < b->priority ? -1 : 1;
    if (a->key.z != b->key.z) return a->key.z < b->key.z ? -1 : 1;
    return a->key.x < b->key.x ? -1 : a->key.x > b->key.x;
}

static size_t make_desired(const WorldTilesWorld *world, double x, double z, DesiredTile *out) {
    unsigned top = world->config.lod_levels - 1u;
    double size = tile_size(world, top);
    int64_t cx = (int64_t)floor(x / size), cz = (int64_t)floor(z / size);
    size_t count = 0;
    for (int dz = -3; dz < 3; ++dz) {
        for (int dx = -3; dx < 3; ++dx) {
            out[count++] = (DesiredTile){{cx + dx, cz + dz, top}, true, 0.0};
        }
    }
    for (unsigned lod = top; lod > 0; --lod) {
        size = tile_size(world, lod);
        cx = (int64_t)floor(x / size);
        cz = (int64_t)floor(z / size);
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                WorldTileKey parent = {cx + dx, cz + dz, lod};
                for (size_t i = 0; i < count; ++i) {
                    if (world_tile_key_equal(out[i].key, parent)) { out[i].leaf = false; break; }
                }
                for (int child_z = 0; child_z < 2; ++child_z) {
                    for (int child_x = 0; child_x < 2; ++child_x) {
                        WorldTileKey key = {parent.x * 2 + child_x, parent.z * 2 + child_z, lod - 1u};
                        out[count++] = (DesiredTile){key, true, 0.0};
                    }
                }
            }
        }
    }
    for (size_t i = 0; i < count; ++i) {
        size = tile_size(world, out[i].key.lod);
        double dx = ((double)out[i].key.x + 0.5) * size - x;
        double dz = ((double)out[i].key.z + 0.5) * size - z;
        out[i].priority = dx * dx + dz * dz;
    }
    qsort(out, count, sizeof(*out), desired_compare);
    return count;
}

static bool build_current(WorldTilesWorld *world, unsigned slot, uint64_t generation) {
    pthread_mutex_lock(&world->mutex);
    TileRecord *record = &world->records[slot];
    bool current = !world->stop && record->desired && !record->cancelled && record->generation == generation;
    pthread_mutex_unlock(&world->mutex);
    return current;
}

static float sample_height(const WorldTilesWorld *world, double x, double z) {
    float height = world->config.height(x, z, world->config.userdata);
    return isfinite(height) ? height : 0.0f;
}

static uint32_t perimeter_vertex(unsigned index, unsigned n) {
    unsigned stride = n + 1u;
    if (index <= n) return index;
    index -= n;
    if (index <= n) return index * stride + n;
    index -= n;
    if (index <= n) return n * stride + n - index;
    index -= n;
    return (n - index) * stride;
}

static BuildResult build_mesh(WorldTilesWorld *world, unsigned slot, uint64_t generation,
                              WorldTileKey key, WorldTileMesh *mesh) {
    if (!build_current(world, slot, generation)) return BUILD_CANCELLED;
    unsigned n = world->config.cells, stride = n + 1u;
    size_t surface_vertices = (size_t)stride * stride;
    mesh->key = key;
    mesh->slot = slot;
    mesh->generation = generation;
    mesh->origin_x = (double)key.x * tile_size(world, key.lod);
    mesh->origin_z = (double)key.z * tile_size(world, key.lod);
    mesh->size = (float)tile_size(world, key.lod);
    mesh->cells = n;
    mesh->vertex_count = surface_vertices + 4u * n;
    mesh->index_count = (size_t)n * n * 6u + 4u * n * 6u;
    mesh->vertices = malloc(mesh->vertex_count * sizeof(*mesh->vertices));
    mesh->indices = malloc(mesh->index_count * sizeof(*mesh->indices));
    if (!mesh->vertices || !mesh->indices) {
        world_tiles_release(mesh);
        return BUILD_FAILED;
    }
    pthread_mutex_lock(&world->mutex);
    world->cpu_bytes += mesh_bytes(mesh);
    if (world->cpu_bytes > world->peak_cpu_bytes) world->peak_cpu_bytes = world->cpu_bytes;
    pthread_mutex_unlock(&world->mutex);

    double spacing = tile_size(world, key.lod) / n;
    /* A consistent normal stencil gives shared vertices the same shading at
     * every LOD. Larger meshes do not erase the atlas's small-scale normals. */
    double normal_step = world->config.base_tile_size / n;
    mesh->min_y = FLT_MAX;
    mesh->max_y = -FLT_MAX;
    for (unsigned row = 0; row <= n; ++row) {
        if (!build_current(world, slot, generation)) return BUILD_CANCELLED;
        double local_z = row * spacing, z = mesh->origin_z + local_z;
        for (unsigned col = 0; col <= n; ++col) {
            double local_x = col * spacing, x = mesh->origin_x + local_x;
            float h = sample_height(world, x, z);
            double nx = (double)sample_height(world, x - normal_step, z) - sample_height(world, x + normal_step, z);
            double nz = (double)sample_height(world, x, z - normal_step) - sample_height(world, x, z + normal_step);
            double ny = 2.0 * normal_step, inv_len = 1.0 / sqrt(nx * nx + ny * ny + nz * nz);
            mesh->vertices[(size_t)row * stride + col] = (WorldTileVertex){
                (float)local_x, h, (float)local_z, (float)(nx * inv_len), (float)(ny * inv_len), (float)(nz * inv_len)
            };
            if (h < mesh->min_y) mesh->min_y = h;
            if (h > mesh->max_y) mesh->max_y = h;
        }
    }
    size_t at = 0;
    for (unsigned row = 0; row < n; ++row) {
        for (unsigned col = 0; col < n; ++col) {
            uint32_t a = row * stride + col, b = a + 1u, c = a + stride, d = c + 1u;
            mesh->indices[at++] = a; mesh->indices[at++] = c; mesh->indices[at++] = b;
            mesh->indices[at++] = b; mesh->indices[at++] = c; mesh->indices[at++] = d;
        }
    }
    /* A closed skirt hides mixed-LOD T junctions without resampling or replacing
     * a retained parent's geometry when the camera moves. At distant LODs the
     * macro mountain curvature needs deeper, hidden skirts: measured adjacent
     * edge interpolation error reaches 39.4 m. Bounds include the full skirt. */
    float skirt_depth = key.lod >= 5u ? fmaxf(world->config.skirt_depth, 96.0f) : world->config.skirt_depth;
    unsigned perimeter = 4u * n;
    for (unsigned edge = 0; edge < perimeter; ++edge) {
        WorldTileVertex v = mesh->vertices[perimeter_vertex(edge, n)];
        v.y -= skirt_depth;
        mesh->vertices[surface_vertices + edge] = v;
        if (v.y < mesh->min_y) mesh->min_y = v.y;
    }
    for (unsigned edge = 0; edge < perimeter; ++edge) {
        unsigned next = (edge + 1u) % perimeter;
        uint32_t a = perimeter_vertex(edge, n), b = perimeter_vertex(next, n);
        uint32_t sa = (uint32_t)surface_vertices + edge, sb = (uint32_t)surface_vertices + next;
        mesh->indices[at++] = a; mesh->indices[at++] = b; mesh->indices[at++] = sa;
        mesh->indices[at++] = b; mesh->indices[at++] = sb; mesh->indices[at++] = sa;
    }
    return build_current(world, slot, generation) ? BUILD_OK : BUILD_CANCELLED;
}

static int next_queued(const WorldTilesWorld *world) {
    int best = -1;
    for (unsigned i = 0; i < WORLD_TILES_CAPACITY; ++i) {
        const TileRecord *record = &world->records[i];
        if (record->state != TILE_QUEUED || !record->desired) continue;
        if (best < 0 || record->key.lod > world->records[best].key.lod ||
            (record->key.lod == world->records[best].key.lod && record->priority < world->records[best].priority)) best = (int)i;
    }
    return best;
}

static void *worker_main(void *userdata) {
    WorldTilesWorld *world = userdata;
    pthread_mutex_lock(&world->mutex);
    for (;;) {
        int chosen = next_queued(world);
        while (!world->stop && (world->paused || chosen < 0)) {
            pthread_cond_wait(&world->wake, &world->mutex);
            chosen = next_queued(world);
        }
        if (world->stop) break;
        unsigned slot = (unsigned)chosen;
        TileRecord *record = &world->records[slot];
        record->state = TILE_BUILDING;
        world->worker_busy = true;
        WorldTileKey key = record->key;
        uint64_t generation = record->generation;
        pthread_mutex_unlock(&world->mutex);
        WorldTileMesh mesh = {0};
        BuildResult result = build_mesh(world, slot, generation, key, &mesh);
        pthread_mutex_lock(&world->mutex);
        record = &world->records[slot];
        if (result == BUILD_OK && !world->stop && record->desired && !record->cancelled && record->generation == generation) {
            record->mesh = mesh;
            record->state = TILE_READY;
            ++world->generated_total;
        } else {
            if (mesh.vertices || mesh.indices) {
                world->cpu_bytes -= mesh_bytes(&mesh);
                world_tiles_release(&mesh);
            }
            if (result == BUILD_FAILED && record->desired && !record->cancelled && !world->stop) {
                /* Retry only after a later camera update rather than spin on OOM. */
                record->state = TILE_FAILED;
                ++world->failed_total;
            } else {
                record->state = TILE_EMPTY;
                record->desired = false;
                ++world->cancelled_total;
            }
        }
        world->worker_busy = false;
        /* A pause caller may be waiting for the callback/context fence. */
        pthread_cond_broadcast(&world->wake);
    }
    pthread_mutex_unlock(&world->mutex);
    return NULL;
}

WorldTilesWorld *world_tiles_init(const WorldTilesConfig *config) {
    if (!config || !config->height) return NULL;
    WorldTilesConfig c = *config;
    if (c.base_tile_size == 0) c.base_tile_size = WORLD_TILES_DEFAULT_SIZE;
    if (c.cells == 0) c.cells = WORLD_TILES_DEFAULT_CELLS;
    if (c.lod_levels == 0) c.lod_levels = WORLD_TILES_DEFAULT_LODS;
    if (c.skirt_depth == 0) c.skirt_depth = 24.0f;
    if (!isfinite(c.base_tile_size) || c.base_tile_size < 1.0 || c.base_tile_size > 1000000.0 ||
        c.cells < 2 || c.cells > 64 || c.lod_levels < 1 || c.lod_levels > WORLD_TILES_DEFAULT_LODS ||
        !isfinite(c.skirt_depth) || c.skirt_depth <= 0 || c.skirt_depth > 30.0f) return NULL;
    WorldTilesWorld *world = calloc(1, sizeof(*world));
    if (!world) return NULL;
    world->config = c;
    world->next_generation = 1;
    if (pthread_mutex_init(&world->mutex, NULL) != 0) { free(world); return NULL; }
    if (pthread_cond_init(&world->wake, NULL) != 0) {
        pthread_mutex_destroy(&world->mutex); free(world); return NULL;
    }
    if (pthread_create(&world->worker, NULL, worker_main, world) != 0) {
        pthread_cond_destroy(&world->wake); pthread_mutex_destroy(&world->mutex); free(world); return NULL;
    }
    return world;
}

bool world_tiles_update(WorldTilesWorld *world, double camera_x, double camera_z) {
    if (!world || !isfinite(camera_x) || !isfinite(camera_z)) return false;
    double bx = floor(camera_x / world->config.base_tile_size), bz = floor(camera_z / world->config.base_tile_size);
    /* Leave headroom for central-parent arithmetic and the outer coverage. */
    const double coord_limit = 0x1p60;
    if (!isfinite(bx) || !isfinite(bz) || bx < -coord_limit || bx > coord_limit || bz < -coord_limit || bz > coord_limit) return false;
    int64_t tile_x = (int64_t)bx, tile_z = (int64_t)bz;
    pthread_mutex_lock(&world->mutex);
    if (world->camera_valid && world->camera_tile_x == tile_x && world->camera_tile_z == tile_z) {
        pthread_mutex_unlock(&world->mutex);
        return true;
    }
    DesiredTile desired[DESIRED_MAX];
    size_t count = make_desired(world, camera_x, camera_z, desired);
    unsigned slots[DESIRED_MAX];
    for (unsigned i = 0; i < WORLD_TILES_CAPACITY; ++i) world->records[i].desired = false;
    for (size_t i = 0; i < count; ++i) {
        slots[i] = WORLD_TILES_CAPACITY;
        for (unsigned slot = 0; slot < WORLD_TILES_CAPACITY; ++slot) {
            TileRecord *record = &world->records[slot];
            if (record->state != TILE_EMPTY && !record->cancelled && world_tile_key_equal(record->key, desired[i].key)) {
                record->desired = true;
                record->leaf = desired[i].leaf;
                record->priority = desired[i].priority;
                if (record->state == TILE_FAILED) record->state = TILE_QUEUED;
                slots[i] = slot;
                break;
            }
        }
    }
    for (unsigned slot = 0; slot < WORLD_TILES_CAPACITY; ++slot) {
        TileRecord *record = &world->records[slot];
        if (record->desired || record->state == TILE_EMPTY) continue;
        if (record->state == TILE_BUILDING) { record->cancelled = true; continue; }
        if (record->state == TILE_QUEUED || record->state == TILE_READY) ++world->cancelled_total;
        release_record_mesh(world, record);
        record->state = TILE_EMPTY;
    }
    for (size_t i = 0; i < count; ++i) {
        if (slots[i] != WORLD_TILES_CAPACITY) continue;
        for (unsigned slot = 0; slot < WORLD_TILES_CAPACITY; ++slot) {
            TileRecord *record = &world->records[slot];
            if (record->state != TILE_EMPTY) continue;
            memset(record, 0, sizeof(*record));
            record->key = desired[i].key;
            record->generation = world->next_generation++;
            record->state = TILE_QUEUED;
            record->desired = true;
            record->leaf = desired[i].leaf;
            record->priority = desired[i].priority;
            slots[i] = slot;
            break;
        }
        /* At most 252 desired records plus one cancelled worker can exist. */
        if (slots[i] == WORLD_TILES_CAPACITY) {
            pthread_mutex_unlock(&world->mutex);
            return false;
        }
    }
    memcpy(world->desired_slots, slots, count * sizeof(*slots));
    world->desired_count = count;
    world->camera_tile_x = tile_x;
    world->camera_tile_z = tile_z;
    world->camera_valid = true;
    ++world->updates;
    pthread_cond_signal(&world->wake);
    pthread_mutex_unlock(&world->mutex);
    return true;
}

size_t world_tiles_desired(WorldTilesWorld *world, WorldTileRequest *out, size_t capacity) {
    if (!world) return 0;
    pthread_mutex_lock(&world->mutex);
    size_t count = world->desired_count, copy = count < capacity ? count : capacity;
    if (out) for (size_t i = 0; i < copy; ++i) {
        unsigned slot = world->desired_slots[i];
        const TileRecord *record = &world->records[slot];
        out[i] = (WorldTileRequest){record->key, slot, record->generation, record->leaf,
                                  record->state == TILE_READY || record->state == TILE_DELIVERED};
    }
    pthread_mutex_unlock(&world->mutex);
    return count;
}

bool world_tiles_is_current(WorldTilesWorld *world, unsigned slot, uint64_t generation) {
    if (!world || slot >= WORLD_TILES_CAPACITY || generation == 0) return false;
    pthread_mutex_lock(&world->mutex);
    const TileRecord *record = &world->records[slot];
    bool current = record->desired && !record->cancelled && record->state != TILE_EMPTY && record->generation == generation;
    pthread_mutex_unlock(&world->mutex);
    return current;
}

bool world_tiles_poll_completed(WorldTilesWorld *world, WorldTileMesh *out) {
    if (!world || !out) return false;
    pthread_mutex_lock(&world->mutex);
    for (size_t i = 0; i < world->desired_count; ++i) {
        TileRecord *record = &world->records[world->desired_slots[i]];
        if (record->state != TILE_READY || !record->desired) continue;
        *out = record->mesh;
        world->cpu_bytes -= mesh_bytes(&record->mesh);
        memset(&record->mesh, 0, sizeof(record->mesh));
        record->state = TILE_DELIVERED;
        pthread_mutex_unlock(&world->mutex);
        return true;
    }
    pthread_mutex_unlock(&world->mutex);
    return false;
}

void world_tiles_metrics(WorldTilesWorld *world, WorldTilesMetrics *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!world) return;
    pthread_mutex_lock(&world->mutex);
    out->desired = world->desired_count;
    for (unsigned slot = 0; slot < WORLD_TILES_CAPACITY; ++slot) {
        const TileRecord *r = &world->records[slot];
        if (r->desired && r->leaf) ++out->leaves;
        if (r->state == TILE_QUEUED) ++out->queued;
        if (r->state == TILE_BUILDING) ++out->building;
        if (r->state == TILE_READY) ++out->completed;
        if (r->state == TILE_DELIVERED) ++out->delivered;
    }
    out->cpu_bytes = world->cpu_bytes;
    out->peak_cpu_bytes = world->peak_cpu_bytes;
    out->generated_total = world->generated_total;
    out->cancelled_total = world->cancelled_total;
    out->failed_total = world->failed_total;
    out->updates = world->updates;
    pthread_mutex_unlock(&world->mutex);
}

bool world_tiles_pause(WorldTilesWorld *world) {
    if (!world) return false;
    pthread_mutex_lock(&world->mutex);
    world->paused = true;
    while (world->worker_busy && !world->stop)
        pthread_cond_wait(&world->wake, &world->mutex);
    const bool paused = !world->stop;
    pthread_mutex_unlock(&world->mutex);
    return paused;
}

size_t world_tiles_invalidate_bounds(WorldTilesWorld *world,
                                    double xmin, double zmin, double xmax, double zmax) {
    if (!world || !isfinite(xmin) || !isfinite(zmin) || !isfinite(xmax) || !isfinite(zmax) ||
        xmin > xmax || zmin > zmax) return 0;
    /* Match update's supported coordinate envelope before any bounds math. */
    const double limit = 0x1p60 * world->config.base_tile_size;
    if (xmin < -limit || xmax > limit || zmin < -limit || zmax > limit) return 0;
    pthread_mutex_lock(&world->mutex);
    if (!world->paused || world->worker_busy || world->stop) {
        pthread_mutex_unlock(&world->mutex);
        return 0;
    }
    const double stencil = world->config.base_tile_size / world->config.cells;
    xmin -= stencil; zmin -= stencil; xmax += stencil; zmax += stencil;
    size_t invalidated = 0;
    for (size_t i = 0; i < world->desired_count; ++i) {
        TileRecord *record = &world->records[world->desired_slots[i]];
        const double size = tile_size(world, record->key.lod);
        const double x = (double)record->key.x * size, z = (double)record->key.z * size;
        if (x > xmax || x + size < xmin || z > zmax || z + size < zmin) continue;
        if (record->state == TILE_QUEUED || record->state == TILE_READY)
            ++world->cancelled_total;
        release_record_mesh(world, record);
        record->generation = world->next_generation++;
        record->state = TILE_QUEUED;
        record->cancelled = false;
        ++invalidated;
    }
    pthread_mutex_unlock(&world->mutex);
    return invalidated;
}

void world_tiles_resume(WorldTilesWorld *world) {
    if (!world) return;
    pthread_mutex_lock(&world->mutex);
    world->paused = false;
    pthread_cond_broadcast(&world->wake);
    pthread_mutex_unlock(&world->mutex);
}

void world_tiles_reset(WorldTilesWorld *world) {
    if (!world) return;
    pthread_mutex_lock(&world->mutex);
    world->camera_valid = false;
    world->desired_count = 0;
    for (unsigned slot = 0; slot < WORLD_TILES_CAPACITY; ++slot) {
        TileRecord *record = &world->records[slot];
        record->desired = false;
        if (record->state == TILE_BUILDING) { record->cancelled = true; continue; }
        if (record->state == TILE_QUEUED || record->state == TILE_READY) ++world->cancelled_total;
        release_record_mesh(world, record);
        record->state = TILE_EMPTY;
    }
    pthread_mutex_unlock(&world->mutex);
}

void world_tiles_shutdown(WorldTilesWorld *world) {
    if (!world) return;
    pthread_mutex_lock(&world->mutex);
    world->stop = true;
    pthread_cond_signal(&world->wake);
    pthread_mutex_unlock(&world->mutex);
    pthread_join(world->worker, NULL);
    for (unsigned slot = 0; slot < WORLD_TILES_CAPACITY; ++slot) release_record_mesh(world, &world->records[slot]);
    pthread_cond_destroy(&world->wake);
    pthread_mutex_destroy(&world->mutex);
    free(world);
}

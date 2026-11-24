#define _POSIX_C_SOURCE 200809L
#include "engine/world_tiles.h"

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t changed;
    bool block, entered, proceed, patch;
    bool pause_started, pause_done, pause_ok;
    uint64_t calls;
    WorldTilesWorld *world;
} Fixture;

static double seconds(void) {
    struct timespec value;
    assert(clock_gettime(CLOCK_MONOTONIC, &value) == 0);
    return value.tv_sec + value.tv_nsec / 1e9;
}

static void briefly(void) {
    const struct timespec delay = {0, 100000};
    nanosleep(&delay, NULL);
}

static float expected_height(bool patch, double x, double z) {
    return patch && x >= 144 && x <= 145 && z >= 48 && z <= 49 ? 8.f : 0.f;
}

static float height(double x, double z, void *context) {
    Fixture *f = context;
    pthread_mutex_lock(&f->mutex);
    ++f->calls;
    if (f->block && !f->entered) {
        f->entered = true;
        pthread_cond_broadcast(&f->changed);
        while (!f->proceed) pthread_cond_wait(&f->changed, &f->mutex);
    }
    const bool patch = f->patch;
    pthread_mutex_unlock(&f->mutex);
    return expected_height(patch, x, z);
}

static uint64_t calls(Fixture *f) {
    pthread_mutex_lock(&f->mutex);
    const uint64_t count = f->calls;
    pthread_mutex_unlock(&f->mutex);
    return count;
}

static WorldTileMesh wait_mesh(WorldTilesWorld *world) {
    WorldTileMesh mesh = {0};
    const double deadline = seconds() + 5.;
    while (!world_tiles_poll_completed(world, &mesh)) {
        assert(seconds() < deadline);
        briefly();
    }
    return mesh;
}

static void wait_complete(WorldTilesWorld *world, size_t count) {
    const double deadline = seconds() + 5.;
    for (;;) {
        WorldTilesMetrics m;
        world_tiles_metrics(world, &m);
        if (m.completed == count && !m.building && !m.queued) return;
        assert(seconds() < deadline);
        briefly();
    }
}

static void check_mesh(const WorldTileMesh *mesh, bool patch) {
    const unsigned n = mesh->cells, stride = n + 1;
    const double step = 128. / n;
    for (unsigned row = 0; row <= n; ++row) for (unsigned col = 0; col <= n; ++col) {
        const WorldTileVertex *v = &mesh->vertices[row * stride + col];
        const double x = mesh->origin_x + v->x, z = mesh->origin_z + v->z;
        assert(v->y == expected_height(patch, x, z));
        const double nx = expected_height(patch, x - step, z) - expected_height(patch, x + step, z);
        const double nz = expected_height(patch, x, z - step) - expected_height(patch, x, z + step);
        const double ny = 2 * step, scale = 1. / sqrt(nx * nx + ny * ny + nz * nz);
        assert(fabs(v->nx - nx * scale) < 1e-6);
        assert(fabs(v->ny - ny * scale) < 1e-6);
        assert(fabs(v->nz - nz * scale) < 1e-6);
    }
}

static void *pause_worker(void *context) {
    Fixture *f = context;
    pthread_mutex_lock(&f->mutex);
    f->pause_started = true;
    pthread_cond_broadcast(&f->changed);
    pthread_mutex_unlock(&f->mutex);
    const bool ok = world_tiles_pause(f->world);
    pthread_mutex_lock(&f->mutex);
    f->pause_ok = ok;
    f->pause_done = true;
    pthread_cond_broadcast(&f->changed);
    pthread_mutex_unlock(&f->mutex);
    return NULL;
}

static void test_pause_fence(void) {
    Fixture f = {.mutex = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER, .block = true};
    WorldTilesConfig config = {.height = height, .userdata = &f, .cells = 8, .lod_levels = 1};
    f.world = world_tiles_init(&config);
    assert(f.world && world_tiles_update(f.world, 0, 0));
    pthread_mutex_lock(&f.mutex);
    struct timespec deadline;
    assert(clock_gettime(CLOCK_REALTIME, &deadline) == 0);
    deadline.tv_sec += 5;
    while (!f.entered) assert(pthread_cond_timedwait(&f.changed, &f.mutex, &deadline) == 0);
    pthread_mutex_unlock(&f.mutex);
    pthread_t pause;
    assert(pthread_create(&pause, NULL, pause_worker, &f) == 0);
    pthread_mutex_lock(&f.mutex);
    while (!f.pause_started) assert(pthread_cond_timedwait(&f.changed, &f.mutex, &deadline) == 0);
    assert(!f.pause_done); /* The in-flight sampler still owns the context. */
    f.proceed = true;
    pthread_cond_broadcast(&f.changed);
    pthread_mutex_unlock(&f.mutex);
    assert(pthread_join(pause, NULL) == 0 && f.pause_done && f.pause_ok);
    WorldTilesMetrics before, after;
    world_tiles_metrics(f.world, &before);
    assert(before.building == 0 && before.completed > 0);
    const uint64_t count = calls(&f);
    for (unsigned i = 0; i < 50; ++i) briefly();
    assert(calls(&f) == count); /* No new callback after pause returned. */
    assert(world_tiles_pause(f.world)); /* Idempotent owner fence. */
    world_tiles_metrics(f.world, &after);
    assert(before.generated_total == after.generated_total && before.cpu_bytes == after.cpu_bytes);
    /* Rollback needs no invalidation and resumes all original requests. */
    world_tiles_resume(f.world);
    for (unsigned i = 0; i < 36; ++i) {
        WorldTileMesh mesh = wait_mesh(f.world);
        check_mesh(&mesh, false);
        world_tiles_release(&mesh);
    }
    if (before.queued) assert(calls(&f) > count);
    world_tiles_shutdown(f.world);
    pthread_cond_destroy(&f.changed);
    pthread_mutex_destroy(&f.mutex);
}

static bool intersects(const WorldTileRequest *r, double xmin, double zmin, double xmax, double zmax) {
    const double size = ldexp(128., (int)r->key.lod), step = 16.;
    const double x = r->key.x * size, z = r->key.z * size;
    return x <= xmax + step && x + size >= xmin - step &&
           z <= zmax + step && z + size >= zmin - step;
}

static void check_changed(const WorldTileRequest *before, const WorldTileRequest *after,
                          size_t count, double xmin, double zmin, double xmax, double zmax,
                          WorldTilesWorld *world, size_t expected) {
    size_t changed = 0;
    for (size_t i = 0; i < count; ++i) {
        assert(world_tile_key_equal(before[i].key, after[i].key) && before[i].slot == after[i].slot);
        if (intersects(&before[i], xmin, zmin, xmax, zmax)) {
            ++changed;
            assert(before[i].generation != after[i].generation);
            assert(!world_tiles_is_current(world, before[i].slot, before[i].generation));
            assert(!after[i].generated);
        } else {
            assert(before[i].generation == after[i].generation && before[i].generated == after[i].generated);
            assert(world_tiles_is_current(world, before[i].slot, before[i].generation));
        }
    }
    assert(changed == expected);
}

static void test_bounds_generations_normals_and_undo(void) {
    Fixture f = {.mutex = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER};
    WorldTilesConfig config = {.height = height, .userdata = &f, .cells = 8, .lod_levels = 2};
    WorldTilesWorld *world = world_tiles_init(&config);
    assert(world && world_tiles_update(world, 0, 0));
    WorldTileMesh old[72] = {0};
    for (unsigned i = 0; i < 72; ++i) old[i] = wait_mesh(world);
    WorldTileRequest before[WORLD_TILES_CAPACITY], after[WORLD_TILES_CAPACITY];
    const size_t count = world_tiles_desired(world, before, WORLD_TILES_CAPACITY);
    assert(count == 72 && world_tiles_pause(world));
    assert(world_tiles_invalidate_bounds(world, NAN, 0, 1, 1) == 0);
    assert(world_tiles_invalidate_bounds(world, 0, 0, INFINITY, 1) == 0);
    assert(world_tiles_invalidate_bounds(world, 1, 0, 0, 1) == 0);
    assert(world_tiles_invalidate_bounds(world, 0, 1, 1, 0) == 0);
    assert(world_tiles_invalidate_bounds(world, -1e100, 0, 1e100, 1) == 0);
    assert(world_tiles_invalidate_bounds(world, 10000, 10000, 10001, 10001) == 0);
    world_tiles_desired(world, after, WORLD_TILES_CAPACITY);
    for (size_t i = 0; i < count; ++i) assert(before[i].generation == after[i].generation);
    f.patch = true; /* Protected by the pause fence; worker cannot read it. */
    /* x=144 is outside tile[0,0]'s 128m footprint but exactly on its
     * boundary vertex's +16m normal stencil. That tile MUST regenerate. */
    assert(world_tiles_invalidate_bounds(world, 144, 48, 145, 49) == 3);
    world_tiles_desired(world, after, WORLD_TILES_CAPACITY);
    check_changed(before, after, count, 144, 48, 145, 49, world, 3);
    world_tiles_resume(world);
    assert(world_tiles_invalidate_bounds(world, 0, 0, 100, 100) == 0); /* Must own the fence. */
    bool checked_normal_only_neighbor = false;
    for (unsigned i = 0; i < 3; ++i) {
        WorldTileMesh mesh = wait_mesh(world);
        check_mesh(&mesh, true);
        if (mesh.key.lod == 0 && mesh.key.x == 0 && mesh.key.z == 0) {
            const WorldTileVertex *v = &mesh.vertices[3 * 9 + 8];
            assert(v->y == 0 && v->nx < -.2f);
            checked_normal_only_neighbor = true;
        }
        world_tiles_release(&mesh);
    }
    assert(checked_normal_only_neighbor);
    WorldTilesMetrics m;
    world_tiles_metrics(world, &m);
    assert(m.generated_total == 75 && m.delivered == 72 && m.cpu_bytes == 0 && m.failed_total == 0);
    for (unsigned i = 0; i < 72; ++i) {
        check_mesh(&old[i], false); /* Old polled allocations remain caller-owned. */
        world_tiles_release(&old[i]);
    }
    assert(world_tiles_pause(world));
    world_tiles_desired(world, before, WORLD_TILES_CAPACITY);
    f.patch = false;
    assert(world_tiles_invalidate_bounds(world, 144, 48, 145, 49) == 3);
    world_tiles_desired(world, after, WORLD_TILES_CAPACITY);
    check_changed(before, after, count, 144, 48, 145, 49, world, 3);
    world_tiles_resume(world);
    for (unsigned i = 0; i < 3; ++i) {
        WorldTileMesh mesh = wait_mesh(world);
        check_mesh(&mesh, false);
        world_tiles_release(&mesh);
    }
    world_tiles_metrics(world, &m);
    assert(m.generated_total == 78 && m.delivered == 72 && m.cpu_bytes == 0);
    /* Closed bounds at a negative tile edge refresh both neighboring tiles
     * and their parent while preserving all other slot/generation pairs. */
    assert(world_tiles_pause(world));
    world_tiles_desired(world, before, WORLD_TILES_CAPACITY);
    const size_t n = world_tiles_invalidate_bounds(world, -128, -48, -128, -48);
    assert(n > 0 && n < 10);
    world_tiles_desired(world, after, WORLD_TILES_CAPACITY);
    check_changed(before, after, count, -128, -48, -128, -48, world, n);
    world_tiles_resume(world);
    for (size_t i = 0; i < n; ++i) {
        WorldTileMesh mesh = wait_mesh(world);
        check_mesh(&mesh, false);
        world_tiles_release(&mesh);
    }
    world_tiles_shutdown(world);
    pthread_cond_destroy(&f.changed);
    pthread_mutex_destroy(&f.mutex);
}

static void test_ready_meshes_retained(void) {
    Fixture f = {.mutex = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER};
    WorldTilesConfig config = {.height = height, .userdata = &f, .cells = 8, .lod_levels = 2};
    WorldTilesWorld *world = world_tiles_init(&config);
    assert(world && world_tiles_update(world, 0, 0));
    wait_complete(world, 72);
    assert(world_tiles_pause(world));
    WorldTilesMetrics before, after;
    world_tiles_metrics(world, &before);
    WorldTileRequest original[WORLD_TILES_CAPACITY], changed[WORLD_TILES_CAPACITY];
    const size_t count = world_tiles_desired(world, original, WORLD_TILES_CAPACITY);
    assert(world_tiles_invalidate_bounds(world, 48, 48, 49, 49) == 2);
    world_tiles_desired(world, changed, WORLD_TILES_CAPACITY);
    check_changed(original, changed, count, 48, 48, 49, 49, world, 2);
    world_tiles_metrics(world, &after);
    assert(after.completed == 70 && after.queued == 2 && after.cpu_bytes < before.cpu_bytes);
    size_t polled = 0;
    WorldTileMesh mesh = {0};
    while (world_tiles_poll_completed(world, &mesh)) {
        bool retained = false;
        for (size_t i = 0; i < count; ++i) if (world_tile_key_equal(original[i].key, mesh.key)) {
            assert(original[i].slot == mesh.slot && original[i].generation == mesh.generation);
            assert(!intersects(&original[i], 48, 48, 49, 49));
            retained = true;
        }
        assert(retained);
        check_mesh(&mesh, false);
        world_tiles_release(&mesh);
        ++polled;
    }
    assert(polled == 70);
    const uint64_t count_before = calls(&f);
    for (unsigned i = 0; i < 20; ++i) briefly();
    assert(calls(&f) == count_before);
    world_tiles_resume(world);
    for (unsigned i = 0; i < 2; ++i) {
        mesh = wait_mesh(world);
        check_mesh(&mesh, false);
        world_tiles_release(&mesh);
    }
    world_tiles_metrics(world, &after);
    assert(after.generated_total == 74 && after.cpu_bytes == 0 && after.delivered == 72);
    world_tiles_shutdown(world);
    pthread_cond_destroy(&f.changed);
    pthread_mutex_destroy(&f.mutex);
}

int main(void) {
    assert(!world_tiles_pause(NULL));
    assert(world_tiles_invalidate_bounds(NULL, 0, 0, 1, 1) == 0);
    world_tiles_resume(NULL);
    test_pause_fence();
    test_bounds_generations_normals_and_undo();
    test_ready_meshes_retained();
    puts("PASS: terrain edit pause fence, bounded generations, normal-edge neighbors, cached meshes, rollback and undo");
    return 0;
}

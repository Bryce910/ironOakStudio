#define _POSIX_C_SOURCE 200809L
#include "engine/world_tiles.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t changed;
    bool block, entered, proceed;
} HeightFixture;

static float plane_height(double x, double z, void *userdata) {
    HeightFixture *fixture = userdata;
    if (fixture) {
        pthread_mutex_lock(&fixture->mutex);
        if (fixture->block && !fixture->entered) {
            fixture->entered = true;
            pthread_cond_signal(&fixture->changed);
            while (!fixture->proceed) pthread_cond_wait(&fixture->changed, &fixture->mutex);
        }
        pthread_mutex_unlock(&fixture->mutex);
    }
    return (float)(8.0 + x * 0.01 + z * 0.02);
}

static double seconds(void) {
    struct timespec now;
    assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
    return now.tv_sec + now.tv_nsec / 1e9;
}

static void pause_briefly(void) {
    const struct timespec wait = {0, 100000};
    nanosleep(&wait, NULL);
}

static WorldTileMesh wait_mesh(WorldTilesWorld *world) {
    WorldTileMesh mesh = {0};
    double deadline = seconds() + 10.0;
    while (!world_tiles_poll_completed(world, &mesh)) {
        assert(seconds() < deadline);
        pause_briefly();
    }
    return mesh;
}

static int find_key(const WorldTileRequest *requests, size_t count, WorldTileKey key) {
    for (size_t i = 0; i < count; ++i) if (world_tile_key_equal(requests[i].key, key)) return (int)i;
    return -1;
}

static void check_tree(WorldTilesWorld *world, double camera_x, double camera_z) {
    assert(world_tiles_update(world, camera_x, camera_z));
    WorldTileRequest requests[WORLD_TILES_CAPACITY];
    size_t count = world_tiles_desired(world, requests, WORLD_TILES_CAPACITY);
    assert(count == 252 && count < WORLD_TILES_CAPACITY);
    assert(world_tiles_desired(world, NULL, 0) == count);
    size_t leaves = 0;
    double area = 0;
    unsigned covering_camera = 0;
    for (size_t i = 0; i < count; ++i) {
        WorldTileRequest *request = &requests[i];
        assert(request->slot < WORLD_TILES_CAPACITY && request->generation != 0);
        assert(world_tiles_is_current(world, request->slot, request->generation));
        assert(!world_tiles_is_current(world, request->slot, request->generation + 100000));
        if (i) assert(requests[i - 1].key.lod >= request->key.lod);
        for (size_t j = 0; j < i; ++j) {
            assert(!world_tile_key_equal(request->key, requests[j].key));
            assert(request->slot != requests[j].slot);
        }
        if (request->key.lod < WORLD_TILES_DEFAULT_LODS - 1) {
            int parent = find_key(requests, count, world_tile_parent(request->key));
            assert(parent >= 0 && !requests[parent].leaf);
        }
        if (!request->leaf) {
            assert(request->key.lod > 0);
            for (int dz = 0; dz < 2; ++dz) for (int dx = 0; dx < 2; ++dx) {
                WorldTileKey child = {request->key.x * 2 + dx, request->key.z * 2 + dz, request->key.lod - 1};
                assert(find_key(requests, count, child) >= 0);
            }
            continue;
        }
        ++leaves;
        double size = ldexp(WORLD_TILES_DEFAULT_SIZE, (int)request->key.lod);
        double x0 = request->key.x * size, z0 = request->key.z * size;
        area += size * size;
        if (camera_x >= x0 && camera_x < x0 + size && camera_z >= z0 && camera_z < z0 + size) ++covering_camera;
        for (size_t j = 0; j < i; ++j) if (requests[j].leaf) {
            double other_size = ldexp(WORLD_TILES_DEFAULT_SIZE, (int)requests[j].key.lod);
            double ox = requests[j].key.x * other_size, oz = requests[j].key.z * other_size;
            assert(x0 + size <= ox || ox + other_size <= x0 || z0 + size <= oz || oz + other_size <= z0);
            bool shared_x_edge = (x0 + size == ox || ox + other_size == x0) && fmax(z0, oz) < fmin(z0 + size, oz + other_size);
            bool shared_z_edge = (z0 + size == oz || oz + other_size == z0) && fmax(x0, ox) < fmin(x0 + size, ox + other_size);
            if (shared_x_edge || shared_z_edge) assert(abs((int)request->key.lod - (int)requests[j].key.lod) <= 1);
        }
    }
    assert(leaves == 198 && covering_camera == 1);
    double coarse_size = ldexp(WORLD_TILES_DEFAULT_SIZE, WORLD_TILES_DEFAULT_LODS - 1);
    assert(area == 36.0 * coarse_size * coarse_size);
    /* Two coarse tile widths remain even at the least favorable camera phase. */
    assert(2.0 * coarse_size >= 12000.0);
    WorldTilesMetrics metrics;
    world_tiles_metrics(world, &metrics);
    assert(metrics.desired == 252 && metrics.leaves == 198);
    assert(metrics.queued + metrics.building + metrics.completed + metrics.delivered <= 253);
}

static void check_mesh(const WorldTileMesh *mesh, float skirt_depth) {
    if (mesh->key.lod >= 5) skirt_depth = fmaxf(skirt_depth, 96);
    unsigned n = mesh->cells, stride = n + 1;
    size_t surface_count = (size_t)stride * stride;
    assert(mesh->vertex_count == surface_count + 4 * n);
    assert(mesh->index_count == (size_t)n * n * 6 + 4 * n * 6);
    assert(mesh->origin_x == mesh->key.x * (double)mesh->size);
    assert(mesh->origin_z == mesh->key.z * (double)mesh->size);
    for (size_t i = 0; i < mesh->index_count; ++i) assert(mesh->indices[i] < mesh->vertex_count);
    for (unsigned z = 0; z <= n; ++z) for (unsigned x = 0; x <= n; ++x) {
        const WorldTileVertex *v = &mesh->vertices[(size_t)z * stride + x];
        assert(v->x == x * (double)mesh->size / n && v->z == z * (double)mesh->size / n);
        float expected = plane_height(mesh->origin_x + v->x, mesh->origin_z + v->z, NULL);
        assert(v->y == expected);
        double len = sqrt(v->nx * v->nx + v->ny * v->ny + v->nz * v->nz);
        assert(fabs(len - 1) < 1e-6 && v->ny > .99f);
        assert(fabs(v->nx / v->ny + .01) < 1e-4 && fabs(v->nz / v->ny + .02) < 1e-4);
        assert(v->y >= mesh->min_y && v->y <= mesh->max_y);
    }
    float minimum = INFINITY, maximum = -INFINITY;
    for (size_t i = 0; i < mesh->vertex_count; ++i) {
        minimum = fminf(minimum, mesh->vertices[i].y);
        maximum = fmaxf(maximum, mesh->vertices[i].y);
    }
    assert(mesh->min_y == minimum && mesh->max_y == maximum);
    for (size_t i = surface_count; i < mesh->vertex_count; ++i) {
        const WorldTileVertex *v = &mesh->vertices[i];
        assert(v->x == 0 || v->x == mesh->size || v->z == 0 || v->z == mesh->size);
        assert(v->y == plane_height(mesh->origin_x + v->x, mesh->origin_z + v->z, NULL) - skirt_depth);
    }
    /* All terrain triangles have upward winding. Skirts have nonzero area. */
    for (size_t i = 0; i < mesh->index_count; i += 3) {
        WorldTileVertex a = mesh->vertices[mesh->indices[i]], b = mesh->vertices[mesh->indices[i + 1]], c = mesh->vertices[mesh->indices[i + 2]];
        double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
        double vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
        double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        assert(nx * nx + ny * ny + nz * nz > 0);
        if (i < (size_t)n * n * 6) assert(ny > 0);
    }
}

static void test_hierarchy_and_bound(void) {
    WorldTilesConfig config = {.height = plane_height, .cells = 4};
    WorldTilesWorld *world = world_tiles_init(&config);
    assert(world);
    const double cameras[][2] = {{0, 0}, {127.99, 127.99}, {128, 128}, {-.01, -.01}, {-128, -128}, {-128.01, -8192.01}, {50400, 28350}, {-50400, -28350}};
    for (size_t i = 0; i < sizeof(cameras) / sizeof(cameras[0]); ++i) check_tree(world, cameras[i][0], cameras[i][1]);
    WorldTileRequest before[WORLD_TILES_CAPACITY], after[WORLD_TILES_CAPACITY];
    size_t count = world_tiles_desired(world, before, WORLD_TILES_CAPACITY);
    WorldTilesMetrics a, b;
    world_tiles_metrics(world, &a);
    assert(world_tiles_update(world, -50399, -28349));
    world_tiles_metrics(world, &b);
    assert(a.updates == b.updates); /* Same base tile: no tree reconstruction. */
    assert(!world_tiles_update(world, NAN, 0));
    assert(!world_tiles_update(world, 0, INFINITY));
    assert(!world_tiles_update(world, 1e100, 0));
    assert(world_tiles_desired(world, after, WORLD_TILES_CAPACITY) == count);
    for (size_t i = 0; i < count; ++i) assert(before[i].slot == after[i].slot && before[i].generation == after[i].generation);
    for (int i = 0; i < 128; ++i) {
        double x = (i % 17 - 8) * 8192.0 + .01, z = (i % 23 - 11) * 1024.0 - .01;
        check_tree(world, x, z);
    }
    world_tiles_shutdown(world);
}

static void test_mesh_seams_and_priority(void) {
    WorldTilesConfig config = {.height = plane_height, .cells = 8, .lod_levels = 2, .skirt_depth = 24};
    WorldTilesWorld *world = world_tiles_init(&config);
    assert(world && world_tiles_update(world, -0.01, -0.01));
    const size_t expected_count = 72;
    WorldTileMesh meshes[72] = {0};
    for (size_t i = 0; i < expected_count; ++i) {
        meshes[i] = wait_mesh(world);
        assert(world_tiles_is_current(world, meshes[i].slot, meshes[i].generation));
        assert(meshes[i].key.lod == (i < 36 ? 1u : 0u));
        check_mesh(&meshes[i], 24);
    }
    size_t seam_vertices = 0, mixed_lod_vertices = 0;
    for (size_t a = 0; a < expected_count; ++a) for (size_t b = a + 1; b < expected_count; ++b) {
        WorldTileMesh *ma = &meshes[a], *mb = &meshes[b];
        unsigned stride_a = ma->cells + 1, stride_b = mb->cells + 1;
        for (unsigned za = 0; za <= ma->cells; ++za) for (unsigned xa = 0; xa <= ma->cells; ++xa) {
            if (xa != 0 && xa != ma->cells && za != 0 && za != ma->cells) continue;
            WorldTileVertex va = ma->vertices[za * stride_a + xa];
            double wx = ma->origin_x + va.x, wz = ma->origin_z + va.z;
            double bx = (wx - mb->origin_x) * mb->cells / mb->size, bz = (wz - mb->origin_z) * mb->cells / mb->size;
            if (bx < 0 || bx > mb->cells || bz < 0 || bz > mb->cells || floor(bx) != bx || floor(bz) != bz) continue;
            WorldTileVertex vb = mb->vertices[(unsigned)bz * stride_b + (unsigned)bx];
            assert(va.y == vb.y);
            assert(va.nx == vb.nx && va.ny == vb.ny && va.nz == vb.nz);
            ++seam_vertices;
            if (ma->key.lod != mb->key.lod) ++mixed_lod_vertices;
        }
    }
    assert(seam_vertices > 100 && mixed_lod_vertices > 20);
    WorldTilesMetrics metrics;
    world_tiles_metrics(world, &metrics);
    assert(metrics.desired == expected_count && metrics.delivered == expected_count);
    assert(metrics.cpu_bytes == 0 && metrics.failed_total == 0 && metrics.generated_total == expected_count);
    for (size_t i = 0; i < expected_count; ++i) {
        world_tiles_release(&meshes[i]);
        assert(meshes[i].vertices == NULL && meshes[i].indices == NULL);
        world_tiles_release(&meshes[i]);
    }
    world_tiles_shutdown(world);
}

static void test_cancellation_reset_and_transfer(void) {
    HeightFixture fixture = {.mutex = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER, .block = true};
    WorldTilesConfig config = {.height = plane_height, .userdata = &fixture, .cells = 4};
    WorldTilesWorld *world = world_tiles_init(&config);
    assert(world && world_tiles_update(world, 0, 0));
    pthread_mutex_lock(&fixture.mutex);
    struct timespec deadline;
    assert(clock_gettime(CLOCK_REALTIME, &deadline) == 0);
    deadline.tv_sec += 5;
    while (!fixture.entered) assert(pthread_cond_timedwait(&fixture.changed, &fixture.mutex, &deadline) == 0);
    pthread_mutex_unlock(&fixture.mutex);
    WorldTileRequest old[WORLD_TILES_CAPACITY];
    size_t count = world_tiles_desired(world, old, WORLD_TILES_CAPACITY);
    assert(count == 252);
    world_tiles_reset(world);
    assert(world_tiles_desired(world, NULL, 0) == 0);
    for (size_t i = 0; i < count; ++i) assert(!world_tiles_is_current(world, old[i].slot, old[i].generation));
    /* Same coordinate after reset cannot revive the cancelled in-flight slot. */
    assert(world_tiles_update(world, 0, 0));
    for (size_t i = 0; i < count; ++i) assert(!world_tiles_is_current(world, old[i].slot, old[i].generation));
    for (int i = 0; i < 100; ++i) assert(world_tiles_update(world, 50000 + i * 256.0, 28000 + i * 512.0));
    WorldTilesMetrics metrics;
    world_tiles_metrics(world, &metrics);
    assert(metrics.desired == 252 && metrics.building == 1 && metrics.queued == 252);
    assert(metrics.cancelled_total > 252);
    pthread_mutex_lock(&fixture.mutex);
    fixture.proceed = true;
    pthread_cond_signal(&fixture.changed);
    pthread_mutex_unlock(&fixture.mutex);
    WorldTileMesh transferred = wait_mesh(world);
    assert(world_tiles_is_current(world, transferred.slot, transferred.generation));
    world_tiles_reset(world);
    assert(!world_tiles_is_current(world, transferred.slot, transferred.generation));
    /* The caller's transferred allocation survives reset and shutdown. */
    check_mesh(&transferred, 24);
    world_tiles_shutdown(world);
    check_mesh(&transferred, 24);
    world_tiles_release(&transferred);
    pthread_cond_destroy(&fixture.changed);
    pthread_mutex_destroy(&fixture.mutex);
}

static void test_defaults_and_shutdown(void) {
    assert(!world_tiles_init(NULL));
    WorldTilesConfig bad = {0};
    assert(!world_tiles_init(&bad));
    bad.height = plane_height; bad.cells = 65; assert(!world_tiles_init(&bad));
    bad.cells = 0; bad.lod_levels = 8; assert(!world_tiles_init(&bad));
    bad.lod_levels = 0; bad.skirt_depth = 31; assert(!world_tiles_init(&bad));
    bad.skirt_depth = 0; bad.base_tile_size = NAN; assert(!world_tiles_init(&bad));
    WorldTileKey key = {-1, -3, 0}, parent = world_tile_parent(key);
    assert(parent.x == -1 && parent.z == -2 && parent.lod == 1);
    key = (WorldTileKey){INT64_MIN, INT64_MIN + 1, 0}; parent = world_tile_parent(key);
    assert(parent.x == INT64_MIN / 2 && parent.z == INT64_MIN / 2);
    for (int i = 0; i < 12; ++i) {
        WorldTilesConfig config = {.height = plane_height};
        WorldTilesWorld *world = world_tiles_init(&config);
        assert(world);
        if (i % 2) assert(world_tiles_update(world, -40000 + i, 25000));
        world_tiles_shutdown(world); /* idle, queued and active shutdown paths */
    }
    world_tiles_shutdown(NULL); world_tiles_reset(NULL); world_tiles_release(NULL);
}

int main(void) {
    test_defaults_and_shutdown();
    test_hierarchy_and_bound();
    test_mesh_seams_and_priority();
    test_cancellation_reset_and_transfer();
    puts("World tiles: bounded coverage, negative coordinates, coarse-first queues, seam normals, skirts, cancellation, reset and shutdown passed.");
    return 0;
}

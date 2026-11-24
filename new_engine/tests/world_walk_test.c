#include "engine/world_walk.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>

typedef struct { double base, slope_x, slope_z, origin_x, origin_z; unsigned calls; } Plane;

static float plane(double x, double z, void *userdata) {
    Plane *p = userdata;
    ++p->calls;
    return (float)(p->base + (x - p->origin_x) * p->slope_x + (z - p->origin_z) * p->slope_z);
}

static void traverse(Plane *p, double direction_x, double direction_z, int timing, bool allowed) {
    const double speed = 3.5, duration = 5.0;
    const double mixed[] = {1.0 / 45, 1.0 / 60, 1.0 / 47, 1.0 / 59, 1.0 / 51};
    double x = p->origin_x - 40.5, z = p->origin_z + 80.25, begin_x = x, begin_z = z, time = 0;
    unsigned frames = 0;
    while (duration - time > 1e-10) {
        double dt = timing == 0 ? 1.0 / 45 : timing == 1 ? 1.0 / 60 : mixed[frames % 5];
        dt = fmin(dt, duration - time);
        double next_x = x + direction_x * speed * dt, next_z = z + direction_z * speed * dt;
        bool moved = world_walk_can_move(x, z, next_x, next_z, plane, p, 1.2);
        if (moved != allowed) fprintf(stderr, "Walk mismatch: timing%d frame%u slope %.9f %.9f direction %.9f %.9f at %.9f %.9f\n", timing, frames, p->slope_x, p->slope_z, direction_x, direction_z, x, z);
        assert(moved == allowed);
        if (moved) { x = next_x; z = next_z; }
        time += dt;
        ++frames;
    }
    double distance = hypot(x - begin_x, z - begin_z);
    assert(fabs(distance - (allowed ? speed * duration : 0.0)) < 1e-8);
}

static float short_rise(double x, double z, void *userdata) {
    (void)z; (void)userdata;
    return x >= .03 && x <= .06 ? 1.0f : 0.0f;
}

static float invalid_height(double x, double z, void *userdata) {
    (void)x; (void)z; (void)userdata;
    return NAN;
}

static float invalid_probe(double x, double z, void *userdata) {
    (void)z; (void)userdata;
    return x > .5 ? INFINITY : (float)x;
}

int main(void) {
    /* Actual shared helper at all supported frame rates, including near the
     * threshold and at realistic mountain elevation/global coordinate scale. */
    Plane p = {.base = 2200, .origin_x = -49000, .origin_z = 27000};
    const double direction = sqrt(.5);
    for (int timing = 0; timing < 3; ++timing) {
        p.slope_x = 0; p.slope_z = 0;
        traverse(&p, 1, 0, timing, true);
        p.slope_x = 1.199; traverse(&p, 1, 0, timing, true);
        p.slope_x = 1.2; traverse(&p, 1, 0, timing, true);
        p.slope_x = 1.21; traverse(&p, 1, 0, timing, false);
        p.slope_x = 5.0; traverse(&p, 1, 0, timing, false);
        traverse(&p, -1, 0, timing, true); /* steep descent stays available */
        p.slope_x = p.slope_z = 1.2 * direction;
        traverse(&p, direction, direction, timing, true);
        p.slope_x = p.slope_z = 1.21 * direction;
        traverse(&p, direction, direction, timing, false);
    }
    assert(!world_walk_can_move(0, 0, .04, 0, short_rise, NULL, 1.2));
    assert(world_walk_can_move(.04, 0, .08, 0, short_rise, NULL, 1.2));
    assert(!world_walk_can_move(0, 0, .1, 0, invalid_probe, NULL, 1.2));
    assert(!world_walk_can_move(0, 0, .1, 0, invalid_height, NULL, 1.2));
    assert(!world_walk_can_move(0, 0, 0, 0, invalid_height, NULL, 1.2));
    assert(!world_walk_can_move(0, 0, .1, 0, NULL, NULL, 1.2));
    assert(!world_walk_can_move(NAN, 0, .1, 0, plane, &p, 1.2));
    assert(!world_walk_can_move(0, INFINITY, .1, 0, plane, &p, 1.2));
    assert(!world_walk_can_move(0, 0, NAN, 0, plane, &p, 1.2));
    assert(!world_walk_can_move(0, 0, .1, INFINITY, plane, &p, 1.2));
    assert(!world_walk_can_move(-DBL_MAX, 0, DBL_MAX, 0, plane, &p, 1.2));
    assert(!world_walk_can_move(0, 0, .1, 0, plane, &p, -1));
    assert(!world_walk_can_move(0, 0, .1, 0, plane, &p, INFINITY));
    p = (Plane){0};
    assert(world_walk_can_move(0, 0, 0, 0, plane, &p, 0));
    assert(p.calls == 1);
    assert(world_walk_can_move(0, 0, .1, 0, plane, &p, 0));
    p.slope_x = .01;
    assert(!world_walk_can_move(0, 0, .1, 0, plane, &p, 0));
    puts("World walking: actual helper at45/60/mixed FPS, threshold slopes, diagonal speed, downhill, short rises and finite inputs passed.");
    return 0;
}

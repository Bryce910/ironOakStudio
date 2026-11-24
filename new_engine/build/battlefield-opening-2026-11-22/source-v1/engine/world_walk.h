#ifndef VALWOOD_WORLD_WALK_H
#define VALWOOD_WORLD_WALK_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A continuous-heightfield walking step. No state, allocation or GL work.
 * max_slope is rise/run (1.2 is about 50 degrees). Uphill classification uses a
 * fixed 0.75 m forward probe plus the actual segment, with only float-height
 * quantization tolerance. Descending steps are allowed. The caller clamps its
 * walking timestep/step length and supplies its collision surface (for example
 * max(0, atlas_height) for a sea-level surface). Invalid input/samples fail. */
bool world_walk_can_move(double from_x, double from_z, double to_x, double to_z,
                         float (*height)(double, double, void *), void *userdata,
                         double max_slope);

#ifdef __cplusplus
}
#endif
#endif

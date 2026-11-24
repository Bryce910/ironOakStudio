#include "world_walk.h"

#include <float.h>
#include <math.h>

bool world_walk_can_move(double from_x, double from_z, double to_x, double to_z,
                         float (*height)(double, double, void *), void *userdata,
                         double max_slope) {
    if (!height || !isfinite(from_x) || !isfinite(from_z) || !isfinite(to_x) || !isfinite(to_z) ||
        !isfinite(max_slope) || max_slope < 0.0) return false;
    double dx = to_x - from_x, dz = to_z - from_z, distance = hypot(dx, dz);
    if (!isfinite(distance)) return false;
    float from_height = height(from_x, from_z, userdata);
    if (!isfinite(from_height)) return false;
    if (distance == 0.0) return true;
    float to_height = height(to_x, to_z, userdata);
    if (!isfinite(to_height)) return false;
    double rise = (double)to_height - from_height;
    if (rise <= 0.0) return true;

    /* A spatial baseline makes the climb limit independent of how many small
     * walking steps fit in a second. The actual-segment test additionally stops
     * a sharp short rise that would average out over the forward probe. */
    const double probe_length = 0.75;
    double probe_x = from_x + (dx / distance) * probe_length;
    double probe_z = from_z + (dz / distance) * probe_length;
    if (!isfinite(probe_x) || !isfinite(probe_z)) return false;
    float probe_height = height(probe_x, probe_z, userdata);
    if (!isfinite(probe_height)) return false;
    double magnitude = fmax(1.0, fmax(fabs((double)from_height),
                                    fmax(fabs((double)to_height), fabs((double)probe_height))));
    /* Heights are floats: two samples can round in opposite directions. This
     * is submillimeter at Valwood's elevations, not a per-frame step allowance. */
    double epsilon = 2.0 * FLT_EPSILON * magnitude + 1e-6;
    return rise <= distance * max_slope + epsilon &&
           (double)probe_height - from_height <= probe_length * max_slope + epsilon;
}

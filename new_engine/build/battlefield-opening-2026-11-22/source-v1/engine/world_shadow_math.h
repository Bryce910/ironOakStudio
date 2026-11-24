#ifndef VALWOOD_WORLD_SHADOW_MATH_H
#define VALWOOD_WORLD_SHADOW_MATH_H

#include <cmath>

/* Pure CPU, allocation-free directional shadow projection. The column-major
 * result transforms CAMERA-RELATIVE positions to OpenGL clip coordinates.
 * sun_toward points from the scene toward the sun (not the light ray direction).
 *
 * Camera-relative rendering alone moves the shadow texel grid with the camera.
 * Anchor its light-space X/Y center to a WORLD-space texel lattice instead.
 * Project the absolute double camera position before finding the correction;
 * converting a 100 km origin to float first loses small walking movements.
 * Only the small relative center and final matrix are converted to floats.
 *
 * This stabilizes a fixed sun and fixed radius/resolution. A changing sun still
 * rotates the shadow grid naturally. Depth follows the camera continuously,
 * retaining the existing local near/far coverage. The snapped center stays
 * within half a texel of the requested center. Crossing a lattice boundary
 * produces an intentional whole-texel shadow-map shift.
 * Returns false, without modifying output, for invalid inputs. No OpenGL use.
 */
inline bool world_shadow_directional_matrix(const double camera[3],
                                             const float sun_toward[3],
                                             float radius, int map_side,
                                             const float center_relative[3],
                                             float output[16],
                                             float near_plane = 10.f,
                                             float far_plane = 2200.f,
                                             float eye_distance = 1000.f) {
    if (!camera || !sun_toward || !center_relative || !output ||
        !std::isfinite(radius) || radius <= 0 || map_side <= 0 ||
        !std::isfinite(near_plane) || near_plane < 0 ||
        !std::isfinite(far_plane) || far_plane <= near_plane ||
        !std::isfinite(eye_distance) || eye_distance <= 0)
        return false;

    double sun[3], center[3];
    double length_squared = 0;
    for (int k = 0; k < 3; ++k) {
        if (!std::isfinite(camera[k]) || !std::isfinite(sun_toward[k]) ||
            !std::isfinite(center_relative[k])) return false;
        sun[k] = sun_toward[k];
        center[k] = center_relative[k];
        length_squared += sun[k] * sun[k];
    }
    if (length_squared < 1.e-20) return false;
    const double inverse_length = 1. / std::sqrt(length_squared);
    for (double &component : sun) component *= inverse_length;

    // view forward = -sun. Fall back to Z-up at noon/anti-noon so the
    // look-at basis cannot collapse when sun is parallel to world Y.
    double right[3] = {sun[2], 0., -sun[0]};
    if (std::abs(sun[1]) > .999) {
        right[0] = -sun[1]; right[1] = sun[0]; right[2] = 0.;
    }
    const double right_length = std::sqrt(right[0] * right[0] +
                                          right[1] * right[1] + right[2] * right[2]);
    for (double &component : right) component /= right_length;
    const double up[3] = {sun[1] * right[2] - sun[2] * right[1],
                          sun[2] * right[0] - sun[0] * right[2],
                          sun[0] * right[1] - sun[1] * right[0]};
    const double texel = 2. * double(radius) / double(map_side);
    double desired_x = 0., desired_y = 0.;
    for (int k = 0; k < 3; ++k) {
        desired_x += right[k] * (camera[k] + center[k]);
        desired_y += up[k] * (camera[k] + center[k]);
    }
    if (!std::isfinite(desired_x) || !std::isfinite(desired_y)) return false;

    // remainder computes the nearest lattice correction without subtracting
    // two large rounded absolute positions. Its exact half-texel ties use the
    // even lattice coordinate; either adjacent lattice cell is equally valid.
    const double correction_x = -std::remainder(desired_x, texel);
    const double correction_y = -std::remainder(desired_y, texel);
    for (int k = 0; k < 3; ++k)
        center[k] += right[k] * correction_x + up[k] * correction_y;

    double center_x = 0., center_y = 0., center_sun = 0.;
    for (int k = 0; k < 3; ++k) {
        center_x += right[k] * center[k];
        center_y += up[k] * center[k];
        center_sun += sun[k] * center[k];
    }
    const double depth_scale = -2. / (double(far_plane) - double(near_plane));
    float matrix[16] = {};
    for (int k = 0; k < 3; ++k) {
        matrix[k * 4] = float(right[k] / double(radius));
        matrix[k * 4 + 1] = float(up[k] / double(radius));
        matrix[k * 4 + 2] = float(sun[k] * depth_scale);
    }
    matrix[12] = float(-center_x / double(radius));
    matrix[13] = float(-center_y / double(radius));
    matrix[14] = float(-depth_scale * (center_sun + double(eye_distance)) -
                        (double(far_plane) + double(near_plane)) /
                        (double(far_plane) - double(near_plane)));
    matrix[15] = 1.f;
    for (float value : matrix) if (!std::isfinite(value)) return false;
    for (int k = 0; k < 16; ++k) output[k] = matrix[k];
    return true;
}

#endif

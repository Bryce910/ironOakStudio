#include "engine/world_shadow_math.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

namespace {
using Origin = std::array<double, 3>;
using Matrix = std::array<float, 16>;
using Projection = std::array<double, 3>;
constexpr float radius = 160.f;
constexpr int side = 3072;
constexpr double texel = 2. * radius / side;
constexpr double clip_texel = 2. / side;
const float center[3] = {0.f, -25.f, 0.f};

Matrix make(const Origin &camera, const std::array<float, 3> &sun,
            int resolution = side) {
    Matrix matrix;
    assert(world_shadow_directional_matrix(camera.data(), sun.data(), radius,
                                           resolution, center, matrix.data()));
    return matrix;
}

Projection project(const Matrix &matrix, const Origin &camera, const Origin &point) {
    // Match the engine: subtract the camera origin in double, then convert
    // that small relative coordinate for the GPU's float matrix multiply.
    const float relative[3] = {float(point[0] - camera[0]),
                              float(point[1] - camera[1]),
                              float(point[2] - camera[2])};
    Projection result;
    for (int row = 0; row < 3; ++row) {
        result[row] = matrix[12 + row];
        for (int k = 0; k < 3; ++k)
            result[row] += double(matrix[k * 4 + row]) * relative[k];
    }
    return result;
}

void near(double actual, double expected, double tolerance = 1.e-6) {
    assert(std::isfinite(actual));
    if (std::abs(actual - expected) > tolerance) {
        std::fprintf(stderr, "shadow math mismatch %.12g != %.12g (tolerance %.12g)\n",
                     actual, expected, tolerance);
        assert(false);
    }
}

void stable_grid_walk(Origin start, const std::array<float, 3> &sun, bool vertical) {
    const Matrix first = make(start, sun);
    const Origin point = {start[0] + 41.23, start[1] - 2.7, start[2] - 67.1};
    const Projection original = project(first, start, point);
    int crossed_cells = 0, stationary_cells = 0;
    Projection previous = original;
    // Walking in millimetre increments exercises repeated sub-texel camera
    // movements and hundreds of lattice crossings, without storing state.
    for (int step = 1; step <= 10000; ++step) {
        Origin current = start;
        if (vertical) current[1] += step * .0023;
        else { current[0] += step * .0031; current[2] += step * .0017; }
        const Matrix matrix = make(current, sun);
        const Projection actual = project(matrix, current, point);
        for (int axis = 0; axis < 2; ++axis) {
            const double from_original = (actual[axis] - original[axis]) / clip_texel;
            // A fixed world point can only move by whole shadow texels.
            near(from_original, std::round(from_original), .0011);
            const double from_previous = (actual[axis] - previous[axis]) / clip_texel;
            near(from_previous, std::round(from_previous), .0011);
            if (std::abs(from_previous) < .1) ++stationary_cells;
            else ++crossed_cells;
        }
        previous = actual;
    }
    assert(crossed_cells > 50 && stationary_cells > 10000);
}
}

int main() {
    // Known basis: sun +Z means shadow X/Y coincide with world X/Y. Starting
    // on an exact lattice center makes half-texel boundaries unambiguous.
    const std::array<float, 3> axis_sun = {0.f, 0.f, 1.f};
    const Origin origin = {100000., 25., -100000.};
    const Origin point = {100040., 20., -100090.};
    const Projection initial = project(make(origin, axis_sun), origin, point);
    near(initial[0], .25);
    near(initial[1], .125);
    // Preserve the original ortho(160,10,2200)*look() depth range/sign.
    near(initial[2], (2. * 1090. - 2210.) / 2190.);
    for (double delta : {-.49 * texel, -.1 * texel, .1 * texel, .49 * texel}) {
        Origin moved = origin; moved[0] += delta;
        const Projection actual = project(make(moved, axis_sun), moved, point);
        near(actual[0], initial[0], 8.e-8);
        near(actual[1], initial[1]);
    }
    for (double direction : {-1., 1.}) {
        Origin moved = origin; moved[0] += direction * .51 * texel;
        near(project(make(moved, axis_sun), moved, point)[0],
             initial[0] - direction * clip_texel, 8.e-8);
    }

    // Negative controls: the old camera-relative ortho/look projection (for
    // the known +Z sun) slides continuously instead of retaining whole texels.
    Matrix old_unsnapped = {};
    old_unsnapped[0] = old_unsnapped[5] = 1.f / radius;
    old_unsnapped[10] = -2.f / 2190.f;
    old_unsnapped[13] = 25.f / radius;
    old_unsnapped[14] = -210.f / 2190.f;
    old_unsnapped[15] = 1.f;
    Origin small_step = origin; small_step[0] += .25 * texel;
    const double old_fractional_step =
        (project(old_unsnapped, small_step, point)[0] -
         project(old_unsnapped, origin, point)[0]) / clip_texel;
    assert(std::abs(old_fractional_step - std::round(old_fractional_step)) > .24);
    near(project(make(small_step, axis_sun), small_step, point)[0], initial[0], 8.e-8);

    // A superficially similar fix that converts the world origin to float
    // before snapping also fails: at 100 km this 1 mm move disappears, while
    // the correctly computed camera-relative vertex still moves by 1 mm.
    small_step = origin; small_step[0] += .01 * texel;
    Origin lost_step = small_step;
    for (double &component : lost_step) component = float(component);
    const double imprecise_fractional_step =
        (project(make(lost_step, axis_sun), small_step, point)[0] - initial[0]) / clip_texel;
    assert(std::abs(imprecise_fractional_step - std::round(imprecise_fractional_step)) > .009);
    near(project(make(small_step, axis_sun), small_step, point)[0], initial[0], 8.e-8);

    // Off-axis world-scale coverage: both axes and altitude affect the grid.
    // The old camera-relative unsnapped projection fails the integer-delta
    // invariant immediately; float absolute snapping also fails this bound.
    const std::array<float, 3> diagonal_sun = {.37f, .81f, -.46f};
    stable_grid_walk({100800., 1240., -56700.}, diagonal_sun, false);
    stable_grid_walk({-100800., 1240., 56700.}, diagonal_sun, true);
    stable_grid_walk({100800., 24., -56700.}, axis_sun, false);
    for (int resolution : {2048, 3072, 4096}) {
        const Matrix matrix = make(origin, diagonal_sun, resolution);
        for (float value : matrix) assert(std::isfinite(value));
    }

    // A sun parallel to world up previously produced a degenerate look-at.
    // Test noon, below-horizon and a rotating sun through both poles.
    for (int step = 0; step <= 720; ++step) {
        const double angle = step * 3.14159265358979323846 / 360.;
        const std::array<float, 3> sun = {float(std::sin(angle)),
                                         float(std::cos(angle)), 0.f};
        const Matrix matrix = make(origin, sun);
        assert(matrix == make(origin, sun));
        double right_length = 0., up_length = 0., normal_length = 0., basis_dot = 0.;
        for (int k = 0; k < 3; ++k) {
            const double r = matrix[k * 4] * radius;
            const double u = matrix[k * 4 + 1] * radius;
            right_length += r * r; up_length += u * u; basis_dot += r * u;
            const double n = matrix[k * 4 + 2] * -1095.;
            normal_length += n * n;
        }
        near(right_length, 1.); near(up_length, 1.); near(normal_length, 1.);
        near(basis_dot, 0.);
    }

    // Rejected inputs leave output intact, allowing an explicit caller fallback.
    Matrix untouched; untouched.fill(123.f);
    const Matrix sentinel = untouched;
    const float zero_sun[3] = {};
    assert(!world_shadow_directional_matrix(origin.data(), zero_sun, radius, side, center, untouched.data()));
    assert(!world_shadow_directional_matrix(origin.data(), axis_sun.data(), 0, side, center, untouched.data()));
    assert(!world_shadow_directional_matrix(origin.data(), axis_sun.data(), radius, 0, center, untouched.data()));
    assert(!world_shadow_directional_matrix(origin.data(), axis_sun.data(), radius, side, center, untouched.data(), 10, 10));
    assert(!world_shadow_directional_matrix(nullptr, axis_sun.data(), radius, side, center, untouched.data()));
    Origin invalid_origin = origin; invalid_origin[0] = std::numeric_limits<double>::infinity();
    assert(!world_shadow_directional_matrix(invalid_origin.data(), axis_sun.data(), radius, side, center, untouched.data()));
    auto invalid_sun = axis_sun; invalid_sun[1] = std::numeric_limits<float>::quiet_NaN();
    assert(!world_shadow_directional_matrix(origin.data(), invalid_sun.data(), radius, side, center, untouched.data()));
    assert(untouched == sentinel);
    std::puts("PASS shadow math: world-anchored texels, 100 km origin, 30,000 camera steps, vertical motion, pole-safe sun, invalid inputs and unsnapped/float-origin negative controls");
}

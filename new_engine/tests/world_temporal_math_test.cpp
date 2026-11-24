#include "engine/world_temporal.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

namespace {
using Matrix = std::array<float, 16>;
using Vector = std::array<double, 4>;

Matrix perspective(float yaw, float jx, float jy) {
    Matrix p = {}, view = {}, result = {};
    float f = 1.f / std::tan(.54f);
    p[0] = f / (1600.f / 900.f); p[5] = f;
    p[8] = -jx * 2.f / 1600.f; p[9] = -jy * 2.f / 900.f;
    p[10] = (15000.f + .4f) / (.4f - 15000.f);
    p[11] = -1; p[14] = 2.f * .4f * 15000.f / (.4f - 15000.f);
    view[0] = view[10] = std::cos(yaw); view[5] = view[15] = 1;
    view[2] = -std::sin(yaw); view[8] = std::sin(yaw);
    for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r)
        for (int k = 0; k < 4; ++k) result[c * 4 + r] += p[k * 4 + r] * view[c * 4 + k];
    return result;
}
Vector transform(const Matrix &m, Vector v) {
    Vector result = {};
    for (int r = 0; r < 4; ++r) for (int k = 0; k < 4; ++k) result[r] += m[k * 4 + r] * v[k];
    return result;
}
void expect(Vector actual, Vector expected) {
    for (int k = 0; k < 4; ++k)
        assert(std::abs(actual[k] - expected[k]) < 2.e-5 * std::max(1., std::abs(expected[k])));
}
}

int main() {
    float jitter[8][2];
    for (unsigned i = 0; i < 8; ++i) {
        world_temporal_jitter(i, jitter[i]);
        float repeat[2]; world_temporal_jitter(i + 8000000000ull, repeat);
        for (int k = 0; k < 2; ++k) {
            assert(jitter[i][k] >= -.5f && jitter[i][k] <= .5f);
            assert(jitter[i][k] == repeat[k]);
        }
        for (unsigned k = 0; k < i; ++k)
            assert(jitter[i][0] != jitter[k][0] || jitter[i][1] != jitter[k][1]);
    }
    Matrix current = perspective(.31f, jitter[1][0], jitter[1][1]);
    Matrix previous = perspective(.29f, jitter[0][0], jitter[0][1]), reprojection;
    // Subtraction before float conversion matters at both world and much
    // larger origins. The relative 0.125 m step remains exactly representable.
    const double current_origin[] = {1.e12 + .125, 110., -1.e12 + .25};
    const double previous_origin[] = {1.e12, 109.75, -1.e12};
    assert(world_temporal_reprojection(current.data(), previous.data(), current_origin, previous_origin, reprojection.data()));
    for (Vector point : {Vector{1, 2, -8, 1}, Vector{40, -5, -100, 1}, Vector{-1000, 500, -10000, 1}}) {
        Vector shifted = point;
        for (int k = 0; k < 3; ++k) shifted[k] += current_origin[k] - previous_origin[k];
        expect(transform(reprojection, transform(current, point)), transform(previous, shifted));
    }
    // An identity camera transform preserves the clip coordinates even with
    // jitter; aliasing the output with an input must be safe.
    Matrix aliased = current;
    assert(world_temporal_reprojection(aliased.data(), current.data(), current_origin, current_origin, aliased.data()));
    expect(transform(aliased, {-.2, .4, .7, 1}), {-.2, .4, .7, 1});
    Matrix singular = {};
    assert(!world_temporal_reprojection(singular.data(), previous.data(), current_origin, previous_origin, reprojection.data()));
    assert(!world_temporal_reprojection(current.data(), singular.data(), current_origin, previous_origin, reprojection.data()));
    current[4] = std::numeric_limits<float>::quiet_NaN();
    assert(!world_temporal_reprojection(current.data(), previous.data(), current_origin, previous_origin, reprojection.data()));
    assert(!world_temporal_reprojection(nullptr, previous.data(), current_origin, previous_origin, reprojection.data()));
    world_temporal_jitter(0, nullptr);
    std::puts("PASS temporal math: bounded jitter, exact camera-relative reprojection, large-origin precision, aliasing and invalid matrices");
}

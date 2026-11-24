#ifndef VALWOOD_WORLD_SWIMMING_H
#define VALWOOD_WORLD_SWIMMING_H

#include <algorithm>
#include <cmath>
#include <limits>

// Player-only surface swimming policy. Water and supporting-floor queries,
// ordinary controller movement, and every collision sweep remain with callers.
namespace world_swimming {
constexpr double speed_scale = .6;

inline bool active(bool was_swimming, bool water_present, double eye,
                   double surface, double support_floor) noexcept {
    if (!water_present || !std::isfinite(eye) || !std::isfinite(surface) ||
        !std::isfinite(support_floor)) return false;
    const double depth = surface - support_floor;
    const double above = eye - surface;
    if (!std::isfinite(depth) || !std::isfinite(above)) return false;
    // The distinct entry/exit bands avoid toggling at a shallow shoreline.
    // A bridge's supporting floor above the water always fails the depth gate.
    return was_swimming ? depth >= .95 && above <= 1.0
                        : depth > 1.1 && above <= .85;
}

inline double eye_target(double surface, double support_floor) noexcept {
    if (!std::isfinite(surface) || !std::isfinite(support_floor))
        return std::numeric_limits<double>::quiet_NaN();
    const double water_eye = surface + .32;
    const double ground_eye = support_floor + 1.75;
    if (!std::isfinite(water_eye) || !std::isfinite(ground_eye))
        return std::numeric_limits<double>::quiet_NaN();
    return std::max(water_eye, ground_eye);
}

// An intentional dive retains neutral buoyancy when its control is released.
// Reaching the surface restores automatic surface floating. The caller still
// approves every requested vertical/horizontal move through the real capsule.
struct DepthState { bool diving=false; double target_eye=0.; };
inline double depth_target(DepthState &state,double eye,double surface,double floor,
                           double vertical_speed,double dt) noexcept {
    if(!std::isfinite(eye)||!std::isfinite(surface)||!std::isfinite(floor)||
       !std::isfinite(vertical_speed)||!std::isfinite(dt)||dt<0)return eye;
    const double low=floor+1.75,high=std::max(low,surface+.32);
    if(!std::isfinite(low)||!std::isfinite(high))return eye;
    if(!state.diving&&vertical_speed<-.05){state.diving=true;state.target_eye=eye;}
    if(!state.diving)return high;
    if(!std::isfinite(state.target_eye))state.target_eye=eye;
    state.target_eye=std::clamp(state.target_eye+std::clamp(vertical_speed,-2.4,2.4)*std::clamp(dt,0.,.1),low,high);
    if(vertical_speed>.05&&state.target_eye>=high-.015&&eye>=surface+.17){state={};return high;}
    return state.target_eye;
}
inline bool submerged(double eye,double surface) noexcept {
    return std::isfinite(eye)&&std::isfinite(surface)&&eye<surface-.08;
}

inline double step(double eye, double target, double dt) noexcept {
    if (!std::isfinite(eye) || !std::isfinite(target) ||
        !std::isfinite(dt) || dt <= 0.) return eye;
    const double elapsed = std::min(dt, .1);
    const double fraction = -std::expm1(-6. * elapsed);
    const double limit = 4. * elapsed;
    const double delta = std::clamp((target - eye) * fraction, -limit, limit);
    // This is only the requested eye height, never a collision-approved pose.
    return eye + delta;
}
} // namespace world_swimming

#endif

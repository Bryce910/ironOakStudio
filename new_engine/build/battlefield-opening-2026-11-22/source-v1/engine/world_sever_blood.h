#ifndef VALWOOD_WORLD_SEVER_BLOOD_H
#define VALWOOD_WORLD_SEVER_BLOOD_H
#include "engine/world_combat_vfx.h"
#include "world_combat_capacity.h"
#include <algorithm>
#include <array>
#include <cmath>

// Per-wound arterial pulses. Animation supplies the current exposed stump,
// including after a surviving enemy moves or the corpse collapses.
namespace world_sever_blood {
using Point = world_combat_vfx::Point;
constexpr unsigned actors = world_combat_capacity::npcs, parts = 8, capacity = actors * parts;
constexpr unsigned pulse_count = 6, stage_capacity = 16;
constexpr double pulse_times[pulse_count] = {0., .13, .31, .55, .87, 1.25};
struct Pose {
    bool severed = false, valid = false;
    Point point, axis = {0, 1, 0};
    double floor = 0;
    float strength = 1;
};
struct Pulse {
    Point point, axis;
    double floor = 0;
    float strength = 1;
    unsigned actor = 0, part = 0, index = 0, droplets = 0;
};
struct Stage { std::array<Pulse, stage_capacity> pulses = {}; unsigned count = 0; };
struct Stats { unsigned long long wounds = 0, pulses = 0; unsigned active = 0, peak = 0; };
class System {
    struct Emitter { bool seen = false, active = false; double age = 0; unsigned next = 0; };
    std::array<Emitter, capacity> emitters_ = {};
    Stage stage_;
    Stats stats_;
    double clock_ = 0;
    bool have_clock_ = false;
    static bool finite(Point p) {
        return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
            std::abs(p.x) < 1.e8 && std::abs(p.y) < 1.e7 && std::abs(p.z) < 1.e8;
    }
    static bool valid(const Pose &p) {
        return p.valid && finite(p.point) && finite(p.axis) &&
            std::hypot(p.axis.x, p.axis.y, p.axis.z) > 1.e-6 &&
            std::isfinite(p.floor) && std::abs(p.floor) < 1.e7 &&
            std::isfinite(p.strength) && p.strength >= .1f && p.strength <= 3.f;
    }
public:
    void reset() { emitters_ = {}; stage_ = {}; have_clock_ = false; stats_.active = 0; }
    bool update(const std::array<Pose, capacity> &poses, double dt, double clock, bool paused = false,
                unsigned maximum_emit = stage_capacity) {
        stage_ = {};
        if (!std::isfinite(dt) || dt < 0 || !std::isfinite(clock) || clock < 0 || maximum_emit > stage_capacity) return false;
        if (paused) return true;
        if (have_clock_ && clock + 1.e-6 < clock_) reset();
        clock_ = clock; have_clock_ = true; dt = std::min(dt, .1); stats_.active = 0;
        for (unsigned id = 0; id < capacity; ++id) {
            auto &e = emitters_[id]; const auto &p = poses[id];
            if (!p.severed) { e = {}; continue; }
            const bool fresh = !e.seen;
            if (fresh && valid(p)) { e.seen = e.active = true; e.age = 0; e.next = 0; ++stats_.wounds; }
            if (!e.active) continue;
            if (!fresh) e.age += dt;
            if (!valid(p)) { ++stats_.active; continue; }
            const double length = std::hypot(p.axis.x, p.axis.y, p.axis.z);
            const Point axis{p.axis.x / length, p.axis.y / length, p.axis.z / length};
            while (e.next < pulse_count && e.age + 1.e-8 >= pulse_times[e.next] && stage_.count < maximum_emit) {
                const unsigned index = e.next++;
                stage_.pulses[stage_.count++] = {p.point, axis, p.floor,
                    p.strength * (1.f - .105f * index), id / parts, id % parts, index, index ? 24u : 40u};
                ++stats_.pulses;
            }
            // Saturated frames leave unsent pulses pending, rather than losing
            // a wound's first burst when several independent actors are hit.
            if (e.next == pulse_count) e.active = false;
            stats_.active += e.active;
        }
        stats_.peak = std::max(stats_.peak, stats_.active);
        return true;
    }
    Stage stage() const { return stage_; }
    Stats stats() const { return stats_; }
};
}
#endif

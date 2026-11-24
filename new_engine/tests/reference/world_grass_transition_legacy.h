// Frozen pre-linear-matching production header; only class/include-guard names differ.
#ifndef VALWOOD_WORLD_GRASS_TRANSITION_LEGACY_REFERENCE_H
#define VALWOOD_WORLD_GRASS_TRANSITION_LEGACY_REFERENCE_H

#include "engine/world_cover.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

/* Owner-thread CPU bridge between published grass generations. Common roots
 * retain their opacity and absolute double-precision position; entering and
 * leaving roots use the same 0.8-second linear coverage transition. The renderer
 * consumes field() and fades() at matching indices. No GL or atlas access.
 *
 * All storage is reserved once on construction. replace() sorts reused lookup
 * buffers; advance() is allocation-free and linear in the bounded live count.
 * A field pointer remains valid until replace(), reset(), or an advance() that
 * reports true (which can compact completed outgoing roots).
 */
class LegacyWorldGrassTransition {
public:
    static constexpr size_t max_instances = 2 * WORLD_COVER_GRASS_CAP;
    static constexpr double transition_seconds = .8;
    static constexpr double teleport_distance = 160.;

    LegacyWorldGrassTransition() {
        instances_.reserve(max_instances); staging_instances_.reserve(max_instances);
        fades_.reserve(max_instances); staging_fades_.reserve(max_instances);
        states_.reserve(max_instances); staging_states_.reserve(max_instances);
        lookup_.reserve(max_instances); outgoing_.reserve(max_instances);
        wanted_.reserve(WORLD_COVER_GRASS_CAP); matched_.reserve(max_instances);
    }
    LegacyWorldGrassTransition(const LegacyWorldGrassTransition &) = delete;
    LegacyWorldGrassTransition &operator=(const LegacyWorldGrassTransition &) = delete;
    LegacyWorldGrassTransition(LegacyWorldGrassTransition &&) = delete;
    LegacyWorldGrassTransition &operator=(LegacyWorldGrassTransition &&) = delete;

    const WorldCoverField &field() const { return field_; }
    const std::vector<float> &fades() const { return fades_; }
    // Changes whenever field root bytes/order can change, but not when only
    // opacity advances. Renderers may cache exact root-derived classifications.
    std::uint64_t structure_revision() const { return structure_revision_; }

    void reset() {
        instances_.clear(); fades_.clear(); states_.clear();
        staging_instances_.clear(); staging_fades_.clear(); staging_states_.clear();
        lookup_.clear(); outgoing_.clear(); wanted_.clear(); matched_.clear();
        initialized_ = false; active_ = false; field_ = {};
        ++structure_revision_;
    }

    /* Invalid input leaves the current generation untouched. Duplicate root
     * identities are invalid: generated roots have unique exact yaw/scale bits.
     * A successful replacement requires a renderer upload, even at dt=0.
     */
    bool replace(const WorldCoverField &desired) {
        if (desired.kind != WORLD_COVER_GRASS || desired.count > WORLD_COVER_GRASS_CAP ||
            (desired.count && !desired.instances) ||
            !std::isfinite(desired.anchor_x) || !std::isfinite(desired.anchor_z)) return false;
        wanted_.clear();
        for (size_t i = 0; i < desired.count; ++i) {
            const auto &root = desired.instances[i];
            if (!std::isfinite(root.x) || !std::isfinite(root.y) || !std::isfinite(root.z) ||
                !std::isfinite(root.yaw) || !std::isfinite(root.scale) || root.scale <= 0) return false;
            wanted_.push_back({identity(root), i});
        }
        std::sort(wanted_.begin(), wanted_.end(), by_identity);
        for (size_t i = 1; i < wanted_.size(); ++i)
            if (wanted_[i - 1].id == wanted_[i].id) return false;

        const bool instant = !initialized_ ||
            std::hypot(desired.anchor_x - field_.anchor_x, desired.anchor_z - field_.anchor_z) > teleport_distance;
        staging_instances_.clear(); staging_fades_.clear(); staging_states_.clear();
        active_ = false;
        lookup_.clear();
        if (!instant) {
            for (size_t i = 0; i < instances_.size(); ++i) lookup_.push_back({identity(instances_[i]), i});
            std::sort(lookup_.begin(), lookup_.end(), by_identity);
            matched_.assign(instances_.size(), 0);
        }
        // Desired roots precede outgoing roots, so the current generation
        // always receives its full budget even during rapid replacements.
        for (size_t i = 0; i < desired.count; ++i) {
            const auto &root = desired.instances[i];
            auto found = std::lower_bound(lookup_.begin(), lookup_.end(), RootRef{identity(root), 0}, by_identity);
            if (!instant && found != lookup_.end() && found->id == identity(root)) {
                const size_t old = found->index;
                matched_[old] = 1;
                State state = states_[old]; state.desired = true;
                append(rebased(instances_[old], state, desired), fades_[old], state);
            } else {
                State state{desired.anchor_x + double(root.x), desired.anchor_z + double(root.z), true};
                append(root, instant ? 1.f : 0.f, state);
            }
        }
        if (!instant) {
            outgoing_.clear();
            for (size_t i = 0; i < instances_.size(); ++i)
                if (!matched_[i] && fades_[i] > 0.f) outgoing_.push_back({identity(instances_[i]), i});
            // Repeated replacements can briefly propose more than two full
            // fields. Keep the most visible old roots, dropping nearly hidden
            // ones first with a stable identity tie-breaker.
            std::sort(outgoing_.begin(), outgoing_.end(), [this](const RootRef &a, const RootRef &b) {
                return fades_[a.index] != fades_[b.index] ? fades_[a.index] > fades_[b.index] : a.id < b.id;
            });
            for (const RootRef &old : outgoing_) {
                if (staging_instances_.size() == max_instances) break;
                State state = states_[old.index]; state.desired = false;
                append(rebased(instances_[old.index], state, desired), fades_[old.index], state);
            }
        }
        instances_.swap(staging_instances_); fades_.swap(staging_fades_); states_.swap(staging_states_);
        field_ = desired; initialized_ = true; ++structure_revision_; sync();
        return true;
    }

    bool advance(double seconds) {
        if (!active_ || !std::isfinite(seconds) || seconds <= 0 || instances_.empty()) return false;
        const float step = float(std::min(seconds / transition_seconds, 1.));
        if (step <= 0) return false;
        bool changed = false;
        bool still_active = false;
        size_t write = 0;
        for (size_t read = 0; read < instances_.size(); ++read) {
            const float old = fades_[read];
            float fade = states_[read].desired ? std::min(1.f, old + step) : std::max(0.f, old - step);
            if (states_[read].desired && fade > 1.f - .00001f) fade = 1.f;
            if (!states_[read].desired && fade < .00001f) fade = 0.f;
            changed = changed || fade != old;
            if (!states_[read].desired && fade == 0.f) { changed = true; continue; }
            still_active = still_active || !states_[read].desired || fade < 1.f;
            if (write != read) { instances_[write] = instances_[read]; states_[write] = states_[read]; }
            fades_[write++] = fade;
        }
        if (write != instances_.size()) ++structure_revision_;
        instances_.resize(write); fades_.resize(write); states_.resize(write);
        active_ = still_active;
        sync();
        return changed;
    }

private:
    struct State { double world_x, world_z; bool desired; };
    struct RootRef { std::uint64_t id; size_t index; };
    std::vector<WorldModelInstance> instances_, staging_instances_;
    std::vector<float> fades_, staging_fades_;
    std::vector<State> states_, staging_states_;
    std::vector<RootRef> lookup_, outgoing_, wanted_;
    std::vector<unsigned char> matched_;
    WorldCoverField field_{};
    bool initialized_ = false;
    bool active_ = false;
    std::uint64_t structure_revision_ = 0;

    static std::uint64_t identity(const WorldModelInstance &root) {
        std::uint32_t yaw, scale;
        static_assert(sizeof(yaw) == sizeof(root.yaw), "grass root identity uses float bits");
        std::memcpy(&yaw, &root.yaw, sizeof(yaw)); std::memcpy(&scale, &root.scale, sizeof(scale));
        return std::uint64_t(yaw) << 32 | scale;
    }
    static bool by_identity(const RootRef &a, const RootRef &b) { return a.id < b.id; }
    static WorldModelInstance rebased(WorldModelInstance root, const State &state, const WorldCoverField &field) {
        root.x = float(state.world_x - field.anchor_x); root.z = float(state.world_z - field.anchor_z);
        return root;
    }
    void append(const WorldModelInstance &root, float fade, const State &state) {
        staging_instances_.push_back(root); staging_fades_.push_back(fade); staging_states_.push_back(state);
        active_ = active_ || !state.desired || fade < 1.f;
    }
    void sync() { field_.instances = instances_.data(); field_.count = instances_.size(); }
};

#endif

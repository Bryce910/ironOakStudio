#include "engine/world_grass_transition.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>

namespace {
size_t allocations = 0;
bool count_allocations = false;
std::uint64_t id(const WorldModelInstance &root) {
    std::uint32_t yaw, scale;
    std::memcpy(&yaw, &root.yaw, sizeof(yaw)); std::memcpy(&scale, &root.scale, sizeof(scale));
    return std::uint64_t(yaw) << 32 | scale;
}
WorldModelInstance root(unsigned identity, float x, float z) {
    return {x, 24.f + identity * .001f, z, float(identity), .8f};
}
WorldCoverField field(std::vector<WorldModelInstance> &roots, double x, double z, std::uint64_t generation = 1) {
    return {WORLD_COVER_GRASS, generation, x, z, roots.size(), roots.data()};
}
size_t find(const WorldGrassTransition &registry, const WorldModelInstance &wanted) {
    const auto &current = registry.field();
    for (size_t i = 0; i < current.count; ++i) if (id(current.instances[i]) == id(wanted)) return i;
    assert(!"grass identity missing"); return 0;
}
void opacity(const WorldGrassTransition &registry, const WorldModelInstance &wanted, float expected) {
    assert(std::abs(registry.fades()[find(registry, wanted)] - expected) < .00001f);
}
void compare(const WorldGrassTransition &a, const WorldGrassTransition &b) {
    assert(a.field().count == b.field().count && a.fades().size() == b.fades().size());
    assert(a.field().anchor_x == b.field().anchor_x && a.field().anchor_z == b.field().anchor_z);
    assert(!std::memcmp(a.field().instances, b.field().instances, a.field().count * sizeof(WorldModelInstance)));
    assert(!std::memcmp(a.fades().data(), b.fades().data(), a.fades().size() * sizeof(float)));
}
}

void *operator new(std::size_t bytes) {
    if (count_allocations) ++allocations;
    if (void *memory = std::malloc(bytes ? bytes : 1)) return memory;
    throw std::bad_alloc();
}
void *operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void *memory) noexcept { std::free(memory); }
void operator delete[](void *memory) noexcept { std::free(memory); }
void operator delete(void *memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void *memory, std::size_t) noexcept { std::free(memory); }

int main() {
    WorldGrassTransition registry;
    assert(registry.field().count == 0 && registry.fades().empty());
    assert(!registry.advance(1) && !registry.advance(0) && !registry.advance(-1));
    std::vector<WorldModelInstance> before{
        root(1, -18, -15), root(2, 18, -15), root(3, -18, 15), root(4, 18, 15), root(5, 0, 0)};
    assert(registry.replace(field(before, -50120., 42100.)));
    for (const auto &r : before) opacity(registry, r, 1);
    assert(!registry.advance(.8)); // Initial startup field is immediately opaque.

    // Common roots survive every direction and array reorder. Deliberately
    // perturb regenerated XYZ: the live root must preserve its original world
    // position and exact height/yaw/scale, without a directional exclusion.
    for (const auto &delta : {std::pair<double,double>{24, 0}, {-24, 0}, {0, 24}, {0, -24}, {24, 24}, {-24, -24}}) {
        std::vector<WorldModelInstance> next(before.rbegin(), before.rend());
        double anchor_x = -50120. + delta.first, anchor_z = 42100. + delta.second;
        for (auto &r : next) { r.x -= float(delta.first) + .0001f; r.z -= float(delta.second) - .0001f; r.y += .01f; }
        assert(registry.replace(field(next, anchor_x, anchor_z)));
        assert(registry.field().count == before.size());
        for (const auto &original : before) {
            size_t at = find(registry, original);
            const auto &kept = registry.field().instances[at];
            assert(std::abs(registry.field().anchor_x + kept.x - (-50120. + original.x)) < .00002);
            assert(std::abs(registry.field().anchor_z + kept.z - (42100. + original.z)) < .00002);
            assert(id(kept) == id(original) && kept.y == original.y);
            opacity(registry, original, 1);
        }
        assert(!registry.advance(.4));
    }

    // Enter and leave simultaneously, with matching linear speed and duration.
    std::vector<WorldModelInstance> next{before[0], root(6, 0, 0)};
    assert(registry.replace(field(next, -50120., 42100., 2)));
    opacity(registry, before[0], 1); opacity(registry, next[1], 0);
    for (size_t i = 1; i < before.size(); ++i) opacity(registry, before[i], 1);
    assert(registry.field().count == before.size() + 1);
    assert(!registry.advance(0) && !registry.advance(std::numeric_limits<double>::quiet_NaN()));
    assert(!registry.advance(std::numeric_limits<double>::infinity()));
    assert(registry.advance(.2)); opacity(registry, next[1], .25f); opacity(registry, before[1], .75f);
    assert(registry.advance(.2)); opacity(registry, next[1], .5f); opacity(registry, before[1], .5f);

    // A root reappearing mid-transition resumes its current opacity. It is
    // never duplicated or restarted at zero by a new cover publication.
    std::vector<WorldModelInstance> returning{before[0], before[1], next[1]};
    assert(registry.replace(field(returning, -50096., 42100., 3)));
    opacity(registry, before[1], .5f); opacity(registry, next[1], .5f);
    assert(registry.advance(.4));
    assert(registry.field().count == returning.size());
    for (const auto &r : returning) opacity(registry, r, 1);
    assert(!registry.advance(10));

    // Repeated rebases keep the double world root, avoiding accumulated
    // round-off from converting the previous relative float on every shift.
    const auto reference = registry.field().instances[find(registry, before[0])];
    double wx = registry.field().anchor_x + reference.x, wz = registry.field().anchor_z + reference.z;
    for (unsigned n = 0; n < 1000; ++n) {
        assert(registry.replace(field(returning, -50120. + (n % 2 ? 24. : 0.), 42100. + (n % 3 ? 24. : 0.), n + 4)));
        const auto &kept = registry.field().instances[find(registry, before[0])];
        assert(std::abs(registry.field().anchor_x + kept.x - wx) < .00002);
        assert(std::abs(registry.field().anchor_z + kept.z - wz) < .00002);
    }

    // Invalid generations preserve every renderer-visible byte.
    WorldGrassTransition unchanged;
    std::vector<WorldModelInstance> single{root(99, 2, 3)};
    assert(unchanged.replace(field(single, 2, 3)));
    auto previous = unchanged.field();
    auto invalid = field(single, std::numeric_limits<double>::infinity(), 0);
    assert(!unchanged.replace(invalid));
    invalid = field(single, 0, 0); invalid.kind = WORLD_COVER_TREES; assert(!unchanged.replace(invalid));
    invalid = field(single, 0, 0); invalid.count = WORLD_COVER_GRASS_CAP + 1; assert(!unchanged.replace(invalid));
    invalid = field(single, 0, 0); invalid.instances = nullptr; assert(!unchanged.replace(invalid));
    auto duplicate = single; duplicate.push_back(single[0]); assert(!unchanged.replace(field(duplicate, 0, 0)));
    assert(unchanged.field().instances == previous.instances && unchanged.field().count == previous.count);
    opacity(unchanged, single[0], 1);

    // Teleports replace the invisible distant ring immediately. Explicit reset
    // also restores initial full opacity and accepts an empty grass generation.
    assert(registry.replace(field(single, 50120., -42100., 2000)));
    assert(registry.field().count == 1); opacity(registry, single[0], 1);
    registry.reset(); assert(registry.field().count == 0 && registry.fades().empty());
    assert(registry.replace(field(single, 0, 0))); opacity(registry, single[0], 1);
    std::vector<WorldModelInstance> empty;
    assert(registry.replace(field(empty, 0, 0))); assert(registry.advance(.8));
    assert(registry.field().count == 0 && registry.fades().empty());

    // Saturated, mutually disjoint generations exercise worst-case overlap.
    // A second registry checks deterministic overflow selection. All buffers
    // were reserved in the constructor: neither replacement nor advancement
    // may allocate once incoming fields have been prepared by the caller.
    WorldGrassTransition saturated, deterministic;
    std::vector<WorldModelInstance> batch(WORLD_COVER_GRASS_CAP);
    for (unsigned generation = 0; generation < 8; ++generation) {
        for (size_t i = 0; i < batch.size(); ++i)
            batch[i] = root(unsigned(1000 + generation * batch.size() + i), float(int(i % 200) - 100), float(int(i / 200) - 100));
        auto fresh = field(batch, generation * 24., 0, generation + 1);
        size_t before_allocations = allocations;
        count_allocations = true;
        assert(saturated.replace(fresh) && deterministic.replace(fresh));
        assert(saturated.field().count <= WorldGrassTransition::max_instances);
        assert(saturated.advance(.08) == deterministic.advance(.08));
        count_allocations = false;
        assert(allocations == before_allocations);
        compare(saturated, deterministic);
        // Desired roots preserve input ordering and precede outgoing roots.
        for (size_t i = 0; i < batch.size(); ++i) {
            assert(id(saturated.field().instances[i]) == id(batch[i]));
            assert(std::abs(saturated.fades()[i] - (generation ? .1f : 1.f)) < .00001f);
        }
    }
    assert(saturated.field().count == WorldGrassTransition::max_instances);
    size_t before_allocations = allocations;
    count_allocations = true;
    for (unsigned n = 0; n < 60; ++n) saturated.advance(1. / 60.);
    count_allocations = false;
    assert(allocations == before_allocations && saturated.field().count == batch.size());
    for (float fade : saturated.fades()) assert(fade == 1.f);

    std::puts("PASS grass transition: all-direction common roots, exact identities, stable world roots, symmetric 0.8-second fades, reentry, initial opacity, invalid input, reset/teleport, deterministic bounded overlap, allocation-free update/advance");
}

#ifndef VALWOOD_WORLD_MARKET_HERO_LOOK_H
#define VALWOOD_WORLD_MARKET_HERO_LOOK_H

#include <cmath>

// Authored opening light for the castle/market visual demo. These are the same
// scene-linear sun/sky inputs and final exposure used by the ordinary lighting
// controls. Nothing here changes source material colors, adds an extra light,
// or runs a camera-dependent exposure adjustment while walking.
struct WorldMarketHeroLook {
    float hour = 16.65f;
    float bearing_degrees = -18.f;
    float sun_strength = 1.12f;
    float sky_fill = 1.45f;
    float exposure = 1.24f;
    float grade_strength = .22f;
};

// Apply once at spawn, before explicit visual-QA overrides and user controls.
// A subsequent move between sun, an awning, and an alley must keep one light
// source and exposure. In particular this is not a per-frame location blend.
// Outside the settlement the caller keeps its ordinary world-clock settings.
inline bool world_market_hero_look_build(double village_weight, WorldMarketHeroLook &out) {
    if (!std::isfinite(village_weight) || village_weight <= .9 || village_weight > 1.)
        return false;
    out = WorldMarketHeroLook{};
    return true;
}

#endif

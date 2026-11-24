#include "engine/world_swimming.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {
unsigned checks = 0;
void require(bool value, const char* message) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
bool near(double a, double b, double tolerance = 1e-12) {
    return std::abs(a - b) <= tolerance;
}

template<std::size_t N>
double recover(double start, double target, const std::array<double,N>& times) {
    double eye = start, elapsed = 0.;
    bool monotonic = true, bounded = true, no_overshoot = true;
    for (unsigned frame = 0; elapsed < 8.; ++frame) {
        const double dt = times[frame % N];
        const double next = world_swimming::step(eye, target, dt);
        monotonic &= target >= start ? next >= eye : next <= eye;
        bounded &= std::isfinite(next) && std::abs(next-eye) <= 4.*std::min(dt,.1)+1e-12;
        no_overshoot &= target >= start ? next <= target+1e-12 : next >= target-1e-12;
        eye = next; elapsed += dt;
    }
    require(monotonic, "recovery remains monotonic across frame rates");
    require(bounded, "every request obeys the vertical sweep-distance cap");
    require(no_overshoot, "recovery never overshoots the surface target");
    require(near(eye,target,1e-8), "recovery settles at the requested surface");
    return eye;
}
}

int main() {
    using namespace world_swimming;
    static_assert(speed_scale == .6, "surface swimming uses the agreed speed");

    // Real Seldonne river datum/bed: walking eye is submerged; swimming recovers
    // above the surface while leaving feet safely above the bed.
    require(active(false,true,7.95,8.,6.2), "deep river enters from underwater walking");
    require(near(eye_target(8.,6.2),8.32), "deep water eye sits .32m above water");
    require(!active(false,true,9.35,8.,7.6), "shallow shoreline remains wading");
    require(near(eye_target(8.,7.6),9.35), "shallow target never penetrates support floor");
    require(!active(true,false,8.32,8.,6.2), "dry exit clears existing swimming");
    require(!active(false,true,12.95,8.,11.2), "bridge deck above water stays walking");
    require(!active(true,true,12.95,8.,11.2), "stepping onto bridge exits swimming");
    require(active(false,true,7.95,8.,6.2), "underbridge lower-storey floor permits swimming");
    require(!active(false,true,9.1,8.,6.2), "high jumping eye does not enter swimming");

    // Exact boundaries are expressed at a zero datum to avoid subtraction
    // rounding concealing the intended inclusive/exclusive policy.
    require(!active(false,true,0.,0.,-1.1), "entry depth is strictly greater than 1.1m");
    require(active(false,true,.85,0.,-1.10001), "entry eye threshold is inclusive");
    require(!active(false,true,.85001,0.,-1.2), "above entry eye threshold stays airborne");
    require(!active(false,true,.32,0.,-1.), "hysteresis band cannot initiate swimming");
    require(active(true,true,.32,0.,-1.), "hysteresis band retains existing swimming");
    require(active(true,true,1.,0.,-.95), "exit depth and eye boundaries are inclusive");
    require(!active(true,true,.32,0.,-.94999), "shallow shore exits at lower depth band");
    require(!active(true,true,1.00001,0.,-1.2), "raised eye leaves swimming above exit band");

    bool state = false;
    for (double depth : {1.09,1.101,1.08,1.0,.96,.949}) {
        state = active(state,true,.32,0.,-depth);
        if (depth == 1.09 || depth == .949) require(!state, "shoreline enters/exits at correct bands");
        else require(state, "shoreline fluctuation does not chatter swimming state");
    }

    const auto at30 = recover(3.,8.32,std::array<double,1>{1./30.});
    const auto at60 = recover(3.,8.32,std::array<double,1>{1./60.});
    const auto at120 = recover(3.,8.32,std::array<double,1>{1./120.});
    const auto mixed = recover(3.,8.32,std::array<double,5>{.004,.016,.033,.08,.012});
    require(near(at30,at60,1e-8) && near(at60,at120,1e-8) && near(at120,mixed,1e-8),
            "variable frame rates agree after underwater recovery");
    recover(9.,8.32,std::array<double,3>{.008,.027,.04});
    recover(8.32,8.32,std::array<double,1>{1./60.});
    require(near(step(8.,8.32,.05),8.+.32*(1.-std::exp(-.3))),
            "small requests use exponential surface settling");
    require(near(step(2.,8.32,.1),2.4), "deep recovery is bounded to 4m/s");
    require(near(step(12.,8.32,.1),11.6), "downward recovery is also sweep-bounded");
    require(step(2.,8.32,10.) == step(2.,8.32,.1), "long frame clamps elapsed time to .1s");
    require(step(8.,9.,0.) == 8. && step(8.,9.,-.1) == 8., "nonpositive elapsed time does not move");

    DepthState dive;
    double eye=8.32;
    for(unsigned i=0;i<90;++i){const double target=depth_target(dive,eye,8.,4.5,-2.,1./60.);eye=step(eye,target,1./60.);}
    require(dive.diving&&submerged(eye,8.),"intentional descent enters underwater mode");
    require(eye>=6.25,"diving keeps the real capsule above its riverbed");
    const double held=depth_target(dive,eye,8.,4.5,0.,1./60.);
    for(unsigned i=0;i<180;++i)eye=step(eye,depth_target(dive,eye,8.,4.5,0.,1./60.),1./60.);
    require(near(eye,held,1e-6),"released dive maintains depth rather than forcing surface");
    for(unsigned i=0;i<240;++i)eye=step(eye,depth_target(dive,eye,8.,4.5,2.,1./60.),1./60.);
    require(!dive.diving&&near(eye,8.32,1e-5),"ascending restores automatic surface floating");
    require(!submerged(8.32,8.)&&submerged(7.8,8.),"underwater presentation uses actual eye below water");
    DepthState shallow;require(depth_target(shallow,9.,8.,7.2,-2.,.1)>=8.95,"shallow dive request retains ground clearance");

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (double bad : {nan,inf,-inf}) {
        for (bool previous : {false,true}) {
            require(!active(previous,true,bad,8.,6.2), "invalid eye disables swimming");
            require(!active(previous,true,8.,bad,6.2), "invalid surface disables swimming");
            require(!active(previous,true,8.,8.,bad), "invalid support disables swimming");
        }
        require(step(8.,bad,.016) == 8., "invalid target preserves current eye");
        require(step(8.,9.,bad) == 8., "invalid elapsed time preserves current eye");
        require(std::isnan(eye_target(bad,6.2)) && std::isnan(eye_target(8.,bad)),
                "invalid support target stays explicitly invalid");
    }
    const double largest = std::numeric_limits<double>::max();
    require(!active(false,true,0.,largest,-largest), "overflowing depth fails closed");
    require(!active(true,true,largest,-largest,-largest), "overflowing eye offset fails closed");
    require(step(largest,-largest,.1) <= largest, "extreme finite request does not overflow");
    std::printf("world_swimming_test: PASS %u checks\n",checks);
}

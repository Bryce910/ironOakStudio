#ifndef VALWOOD_WORLD_WEATHER_WETNESS_H
#define VALWOOD_WORLD_WEATHER_WETNESS_H
#include "engine/world_weather_authoring.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

// Weather history is world anchored, independent of camera/tile-cache rebases.
// Sixteen live authored areas plus sixteen frozen residual footprints. This is
// a bounded visual reservoir, not hydrology or a disk-persistent simulation.
namespace world_weather_wetness {
constexpr unsigned capacity=32;
struct Reservoir {float damp=0,film=0;};
struct Layer {
    std::array<float,4> bounds{};
    Reservoir value{};
    std::uint64_t id=0,generation=0;
    int priority=0;
    bool live=false;
};
struct State {
    Reservoir global{};
    std::array<Layer,capacity> layers{};
    unsigned count=0;
    std::uint64_t generation=0,evictions=0;
    bool enabled=false;
};
struct PackedLayer {
    std::array<float,4> bounds{};
    std::array<float,4> channels{}; // damp, surface film, reserved, influence
};
struct Packet {
    Reservoir global{};
    std::array<PackedLayer,capacity> layers{};
    unsigned count=0;
    bool enabled=false;
};
inline float unit(float v){return std::isfinite(v)?std::clamp(v,0.f,1.f):0.f;}
inline Reservoir seeded(float rain){rain=unit(rain);return {rain,rain*rain};}
inline void advance(Reservoir&r,float rain,double dt){
    rain=unit(rain);dt=std::clamp(dt,0.,1.);
    // Porous surfaces saturate first; a continuous film builds more slowly.
    // Removing rain leaves a film and damp soil rather than instantly drying.
    const float wet_rate=rain>r.damp?1.f/18.f:1.f/210.f;
    r.damp+=(rain-r.damp)*float(-std::expm1(-dt*wet_rate));
    const float film_target=rain*r.damp;
    const float film_rate=film_target>r.film?1.f/32.f:1.f/85.f;
    r.film+=(film_target-r.film)*float(-std::expm1(-dt*film_rate));
    r.damp=unit(r.damp);r.film=unit(r.film);
}
inline float weight(const std::array<float,4>&b,double x,double z){
    const double d=std::hypot(x-b[0],z-b[1]);
    if(!std::isfinite(d)||d>=b[3])return 0;
    if(d<=b[2])return 1;
    const float t=std::clamp(float((d-b[2])/(b[3]-b[2])),0.f,1.f);
    return 1-t*t*(3-2*t);
}
inline Reservoir sample(const Packet&p,double x,double z,float legacy=0){
    if(!p.enabled)return seeded(legacy);
    Reservoir out=p.global;
    for(unsigned i=0;i<std::min(p.count,capacity);++i){const auto&l=p.layers[i];const float w=weight(l.bounds,x,z)*unit(l.channels[3]);out.damp+=(l.channels[0]-out.damp)*w;out.film+=(l.channels[1]-out.film)*w;}
    return out;
}
inline Packet packet(const State&s){
    Packet p;p.enabled=s.enabled;p.global=s.global;p.count=std::min(s.count,capacity);
    std::array<unsigned,capacity> order{};for(unsigned i=0;i<p.count;++i)order[i]=i;
    // Stable insertion sort: tiny fixed array, no allocation or allocator use.
    for(unsigned i=1;i<p.count;++i){unsigned key=order[i],j=i;auto less=[&](unsigned a,unsigned b){const auto&x=s.layers[a];const auto&y=s.layers[b];if(x.priority!=y.priority)return x.priority<y.priority;if(x.id!=y.id)return x.id<y.id;return x.generation<y.generation;};while(j&&less(key,order[j-1])){order[j]=order[j-1];--j;}order[j]=key;}
    for(unsigned i=0;i<p.count;++i){const auto&l=s.layers[order[i]];p.layers[i].bounds=l.bounds;p.layers[i].channels={l.value.damp,l.value.film,0,1};}return p;
}
inline bool same_bounds(const std::array<float,4>&a,const std::array<float,4>&b){return a==b;}
inline bool valid(const Packet&p){
    if(p.count>capacity||!std::isfinite(p.global.damp)||!std::isfinite(p.global.film)||p.global.damp<0||p.global.damp>1||p.global.film<0||p.global.film>1)return false;
    for(unsigned i=0;i<p.count;++i){const auto&l=p.layers[i];for(float v:l.bounds)if(!std::isfinite(v))return false;for(float v:l.channels)if(!std::isfinite(v))return false;if(l.bounds[2]<0||l.bounds[3]<.1f||l.bounds[2]>l.bounds[3]||l.channels[0]<0||l.channels[0]>1||l.channels[1]<0||l.channels[1]>1||l.channels[3]<0||l.channels[3]>1)return false;}return true;
}
inline bool update(State&s,const world_weather_authoring::PackedFrame&rain,float legacy_rain,double dt,bool advancing=true,bool seed_startup=true){
    if(rain.count>world_weather_authoring::area_limit||rain.authored_count!=rain.count+unsigned(rain.global_authored)||!std::isfinite(dt)||dt<0||!std::isfinite(legacy_rain)||!std::isfinite(rain.global.rain)||rain.global.rain<0||rain.global.rain>1)return false;
    for(unsigned i=0;i<rain.count;++i){const auto&a=rain.areas[i];for(float v:a.bounds)if(!std::isfinite(v))return false;if(a.bounds[2]<0||a.bounds[3]<.1f||a.bounds[2]>a.bounds[3]||!std::isfinite(a.channels[0])||a.channels[0]<0||a.channels[0]>1||!std::isfinite(a.effects[3])||a.effects[3]<0||a.effects[3]>1)return false;}
    // A never-authored old project follows the old material path byte-for-byte.
    if(!s.enabled&&!rain.authored_count)return true;
    if(!advancing)return true;
    const bool startup=!s.enabled;
    const bool bootstrap=startup&&seed_startup;
    const float base=unit(rain.global_authored?rain.global.rain:legacy_rain);
    if(startup){s.enabled=true;s.global=seeded(bootstrap?base:legacy_rain);}if(!bootstrap)advance(s.global,base,dt);
    for(unsigned i=0;i<s.count;++i)s.layers[i].live=false;
    for(unsigned i=0;i<rain.count;++i){const auto&a=rain.areas[i];unsigned found=capacity;
        for(unsigned j=0;j<s.count;++j)if(s.layers[j].id==a.id&&same_bounds(s.layers[j].bounds,a.bounds)){found=j;break;}
        if(found==capacity){
            if(s.count<capacity)found=s.count++;
            else {
                // Prefer the least-visible inactive residual. Live areas are
                // never displaced while processing this frame's <=16 inputs.
                float best=1.e9f;for(unsigned j=0;j<s.count;++j){bool incoming=false;for(unsigned k=0;k<rain.count;++k)if(s.layers[j].id==rain.areas[k].id&&same_bounds(s.layers[j].bounds,rain.areas[k].bounds)){incoming=true;break;}if(incoming)continue;const auto&v=s.layers[j].value;const float cost=std::abs(v.damp-s.global.damp)+std::abs(v.film-s.global.film);if(cost<best){best=cost;found=j;}}
                if(found==capacity)return false;++s.evictions;
            }
            Layer fresh;fresh.bounds=a.bounds;fresh.id=a.id;fresh.priority=a.priority;fresh.generation=++s.generation;
            const float target=base+(unit(a.channels[0])-base)*unit(a.effects[3]);
            fresh.value=bootstrap?seeded(target):s.global;s.layers[found]=fresh;
        }
        auto&l=s.layers[found];l.live=true;l.priority=a.priority;
        const float target=base+(unit(a.channels[0])-base)*unit(a.effects[3]);
        if(!bootstrap)advance(l.value,target,dt);
    }
    for(unsigned i=0;i<s.count;++i)if(!s.layers[i].live){auto&l=s.layers[i];float target=base;if(rain.authored_count)target=world_weather_authoring::sample(rain,l.bounds[0],l.bounds[1]).rain;advance(l.value,target,dt);}
    // Fully recovered footprints can disappear without a visible seam.
    for(unsigned i=0;i<s.count;){const auto&l=s.layers[i];if(!l.live&&std::abs(l.value.damp-s.global.damp)<.001f&&std::abs(l.value.film-s.global.film)<.001f){s.layers[i]=s.layers[--s.count];}else ++i;}return true;
}
}
#endif

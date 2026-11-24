#ifndef VALWOOD_WORLD_VFX_H
#define VALWOOD_WORLD_VFX_H
#include "engine/world_runtime_editors.h"
#include <algorithm>
#include <array>
#include <cmath>
namespace world_vfx {
struct Preset {const char *name,*description;std::array<float,3> color;};
inline constexpr std::array<Preset,8> presets={{{"Arcane wisps","Orbiting blue lights for magic and enchanted places",{.22f,.65f,1}},
 {"Ember fountain","Rising golden sparks with a warm HDR glow",{1,.28f,.035f}},
 {"Campfire","Animated fire and embers with flickering light",{1,.48f,.12f}},
 {"Chimney smoke","A connected, wind-driven plume",{.65f,.69f,.74f}},
 {"Steam vent","Pale, quickly rising vapor",{.9f,.98f,1}},
 {"Low mist","A wide, slow layer close to the ground",{.76f,.88f,1}},
 {"Sunlit dust","Fine warm motes floating in the air",{1,.82f,.4f}},
 {"Fireflies","Soft green lights drifting around the scene",{.55f,1,.13f}}}};
inline void fit_ceiling(world_studio::Particle &p){p.ceiling=std::max(p.ceiling,p.y+double(p.height)*(p.kind==2?3.2:1.)+.1);}
inline world_studio::Particle make(unsigned index,double x,double y,double z,unsigned seed){
 world_studio::Particle p;p.x=x;p.y=y;p.z=z;p.seed=seed;index=std::min(index,7u);
 switch(index){
 case 0:p.kind=4;p.radius=1.5;p.height=2.2;p.power=2;p.rate=1.6;p.lifetime=2;p.size=1.6;p.wind=0;p.glow=4;break;
 case 1:p.kind=3;p.radius=.85;p.height=3.2;p.power=2.6;p.rate=2;p.lifetime=.65;p.size=1.5;p.wind=0;p.glow=6;break;
 case 2:p.kind=2;p.radius=.24;p.height=.65;p.power=1.2;p.light=4;break;
 case 3:p.kind=0;p.radius=1.5;p.height=6;p.power=.9;p.warmth=.7;p.rate=1.3;p.lifetime=1.5;p.size=1.3;break;
 case 4:p.kind=0;p.radius=.9;p.height=3.4;p.power=.7;p.lifetime=.6;p.wind=.8;break;
 case 5:p.kind=0;p.radius=4;p.height=1.2;p.power=.6;p.size=2;p.rate=1.4;p.lifetime=4;p.wind=.25;break;
 case 6:p.kind=1;p.radius=3;p.height=3;p.power=2;p.size=1.8;p.glow=.25;break;
 case 7:p.kind=4;p.radius=3;p.height=2;p.power=2;p.rate=1.3;p.lifetime=1.6;p.size=.55;p.wind=0;p.glow=5;break;
 }
 if(index!=2&&index!=3)p.tint=presets[index].color;
 p.ceiling=y+.15;fit_ceiling(p);return p;
}
inline void move(world_studio::Particle &p,double x,double y,double z){p.ceiling+=y-p.y;p.x=x;p.y=y;p.z=z;}
// A bounded editor clock: reopening/closing the tool never changes saved data.
struct Clock {double time=0;float speed=1;bool playing=true;void tick(double dt){if(playing&&std::isfinite(dt)&&dt>0)time=std::fmod(time+std::min(dt,.25)*std::clamp(speed,.1f,2.f),64.);}void restart(){time=0;}};
}
#endif

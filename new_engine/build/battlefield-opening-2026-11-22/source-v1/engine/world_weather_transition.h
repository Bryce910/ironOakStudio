#ifndef VALWOOD_WORLD_WEATHER_TRANSITION_H
#define VALWOOD_WORLD_WEATHER_TRANSITION_H
#include "world_weather_authoring.h"
namespace world_weather_authoring {
// Render-thread fixed storage. Profile edits fade without restarting variation
// or storm clocks. Unchanged/legacy maps preserve the exact old fast path.
struct Transition {PackedFrame frame{};bool ready=false;};
inline float approach(float from,float to,float dt,float seconds){return seconds<=0?to:to+(from-to)*std::exp(-std::max(dt,0.f)*4.605170186f/seconds);}
inline Sample approach(Sample a,const Sample&b,float dt,float seconds){
#define WEATHER_FIELD(n) a.n=approach(a.n,b.n,dt,seconds)
 WEATHER_FIELD(rain);WEATHER_FIELD(cloud);WEATHER_FIELD(fog);WEATHER_FIELD(thunder_interval);WEATHER_FIELD(flash_strength);WEATHER_FIELD(thunder_volume);WEATHER_FIELD(storm_weight);WEATHER_FIELD(thunder_weight);WEATHER_FIELD(lightning_weight);
#undef WEATHER_FIELD
 a.transition=b.transition;a.thunder_enabled=a.thunder_weight>1.e-5f;a.lightning_enabled=a.lightning_weight>1.e-5f;return a;
}
inline void transition(Transition&s,const PackedFrame&target,float dt){
 if(!s.ready){s.frame=target;s.ready=true;return;}
 const auto old=s.frame;PackedFrame out=target;
 const float global_seconds=std::max(old.global.transition,target.global.transition);
 out.global=approach(old.global,target.global,dt,global_seconds);
 // Keep an authored global during its finite fade back to legacy values.
 out.global_authored=target.global_authored||(old.global_authored&&
 (std::fabs(out.global.rain-target.global.rain)+std::fabs(out.global.cloud-target.global.cloud)+std::fabs(out.global.fog-target.global.fog)+std::fabs(out.global.thunder_weight-target.global.thunder_weight)+std::fabs(out.global.lightning_weight-target.global.lightning_weight)+std::fabs(out.global.storm_weight-target.global.storm_weight)>.001f));
 if(!out.global_authored)out.global=target.global;
 auto blend=[&](PackedArea from,PackedArea to){const float seconds=to.channels[3];for(unsigned k=0;k<3;++k)to.channels[k]=approach(from.channels[k],to.channels[k],dt,seconds);for(unsigned k=0;k<4;++k){to.effects[k]=approach(from.effects[k],to.effects[k],dt,seconds);to.flags[k]=approach(from.flags[k],to.flags[k],dt,seconds);}return to;};
 for(unsigned i=0;i<target.count;++i){auto from=target.areas[i];bool found=false;for(unsigned j=0;j<old.count;++j)if(old.areas[j].id==from.id){from=old.areas[j];found=true;break;}if(!found)from.effects[3]=0;out.areas[i]=blend(from,target.areas[i]);}
 // Fade layer influence, not its color, so disappearing areas reveal the
 // actual lower-priority local weather rather than a guessed global underlay.
 for(unsigned j=0;j<old.count&&out.count<area_limit;++j){bool found=false;for(unsigned i=0;i<target.count;++i)if(old.areas[j].id==target.areas[i].id)found=true;if(found)continue;auto next=old.areas[j];next.effects[3]=approach(next.effects[3],0,dt,next.channels[3]);if(next.effects[3]>.001f)out.areas[out.count++]=next;}
 std::sort(out.areas.begin(),out.areas.begin()+out.count,[](const PackedArea&a,const PackedArea&b){return a.priority!=b.priority?a.priority<b.priority:a.id<b.id;});out.authored_count=out.count+(out.global_authored?1:0);s.frame=out;
}
}
#endif

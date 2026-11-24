#ifndef VALWOOD_WORLD_WEATHER_AUTHORING_H
#define VALWOOD_WORLD_WEATHER_AUTHORING_H
#include "engine/world_soundscape.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
namespace world_weather_authoring {
inline constexpr unsigned area_limit=16;
struct Range {float minimum=0,maximum=0;};
struct Profile {
 bool rain_enabled=true,thunder_enabled=false,lightning_enabled=false;
 Range rain{.48f,.48f},cloud{.28f,.28f},fog{.62f,.62f},thunder_interval{8,20},flash_strength{.8f,1.2f},thunder_volume{.5f,.8f};
 std::uint32_t seed=1;float variation_seconds=30;
};
struct Global {bool enabled=false;Profile profile;float transition=2;};
struct Area {std::string name="Weather area";double x=0,y=0,z=0;float inner_radius=40,radius=80,transition=2;int priority=0;bool enabled=true;Profile profile;};
struct BoundArea {std::uint64_t id=0;Area area;};
struct Sample {
 float rain=0,cloud=0,fog=0,thunder_interval=12,flash_strength=0,thunder_volume=0,transition=0,storm_weight=0;
 bool thunder_enabled=false,lightning_enabled=false;
 float thunder_weight=0,lightning_weight=0;
};
// Each vec4 maps directly to GLSL. Records are sorted ascending priority, then
// stable entity ID; composition is ordered alpha layering, not winner-takes-all.
struct PackedArea {std::array<float,4> bounds{},channels{},effects{0,0,0,1},flags{};std::uint64_t id=0;int priority=0;};
struct PackedFrame {Sample global;std::array<PackedArea,area_limit> areas{};unsigned count=0,authored_count=0;bool global_authored=false;};
inline bool within(double v,double lo,double hi){return std::isfinite(v)&&v>=lo&&v<=hi;}
inline bool valid(const Range&r,float lo,float hi){return within(r.minimum,lo,hi)&&within(r.maximum,lo,hi)&&r.minimum<=r.maximum;}
inline bool valid(const Profile&p){return valid(p.rain,0,1)&&valid(p.cloud,0,1)&&valid(p.fog,0,1.5f)&&valid(p.thunder_interval,1,180)&&valid(p.flash_strength,0,4)&&valid(p.thunder_volume,0,1)&&within(p.variation_seconds,.1,3600);}
inline bool valid(const Global&g){return valid(g.profile)&&within(g.transition,0,120);}
inline bool valid(const Area&a){return world_soundscape::text(a.name,96)&&a.name.find_first_not_of(' ')!=std::string::npos&&within(a.x,-50400,50400)&&within(a.y,-2000,12000)&&within(a.z,-28400,28400)&&within(a.inner_radius,0,10000)&&within(a.radius,.1,10000)&&a.inner_radius<=a.radius&&within(a.transition,0,120)&&a.priority>=-1000&&a.priority<=1000&&valid(a.profile);}
inline bool valid(const Profile&p,std::string&e){if(!valid(p)){e="Invalid weather profile ranges or variation period";return false;}e.clear();return true;}
inline bool valid(const Global&g,std::string&e){if(!valid(g)){e="Invalid global weather profile or transition";return false;}e.clear();return true;}
inline bool valid(const Area&a,std::string&e){if(!valid(a)){e="Invalid weather area name, position, radii, priority, profile or transition";return false;}e.clear();return true;}
inline bool default_global(const Global&g){const Global d{};return !g.enabled&&g.transition==d.transition&&g.profile.rain_enabled==d.profile.rain_enabled&&g.profile.thunder_enabled==d.profile.thunder_enabled&&g.profile.lightning_enabled==d.profile.lightning_enabled&&g.profile.seed==d.profile.seed&&g.profile.variation_seconds==d.profile.variation_seconds&&g.profile.rain.minimum==d.profile.rain.minimum&&g.profile.rain.maximum==d.profile.rain.maximum&&g.profile.cloud.minimum==d.profile.cloud.minimum&&g.profile.cloud.maximum==d.profile.cloud.maximum&&g.profile.fog.minimum==d.profile.fog.minimum&&g.profile.fog.maximum==d.profile.fog.maximum&&g.profile.thunder_interval.minimum==d.profile.thunder_interval.minimum&&g.profile.thunder_interval.maximum==d.profile.thunder_interval.maximum&&g.profile.flash_strength.minimum==d.profile.flash_strength.minimum&&g.profile.flash_strength.maximum==d.profile.flash_strength.maximum&&g.profile.thunder_volume.minimum==d.profile.thunder_volume.minimum&&g.profile.thunder_volume.maximum==d.profile.thunder_volume.maximum;}
inline std::uint64_t hash(std::uint64_t x){x+=0x9e3779b97f4a7c15ull;x=(x^(x>>30))*0xbf58476d1ce4e5b9ull;x=(x^(x>>27))*0x94d049bb133111ebull;return x^(x>>31);}
inline float smooth(float x){x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);}
// Hash only integer segment IDs. Smooth endpoints share a sample between
// segments, so changing ranges are continuous in seconds and independent of dt.
inline float range_sample(const Range&r,std::uint32_t seed,std::uint64_t id,unsigned channel,double seconds,float period){
 if(!std::isfinite(r.minimum)||!std::isfinite(r.maximum)||r.minimum>r.maximum||!std::isfinite(period)||period<=0)return 0;
 if(r.minimum==r.maximum)return r.minimum;
 if(!std::isfinite(seconds)||seconds<0)seconds=0;seconds=std::min(seconds,1.e12);
 const double t=seconds/period,base=std::floor(t);const auto segment=static_cast<std::uint64_t>(base);
 const auto salt=hash(id)^hash(std::uint64_t(seed)+(std::uint64_t(channel)<<32));
 auto value=[&](std::uint64_t n){return float((hash(salt^hash(n))>>40)*(1.0/16777215.0));};
 const float a=value(segment),b=value(segment+1),v=a+(b-a)*smooth(float(t-base));return r.minimum+(r.maximum-r.minimum)*v;
}
inline Sample profile_sample(const Profile&p,std::uint64_t id,double seconds,float transition){
 Sample s;s.rain=p.rain_enabled?range_sample(p.rain,p.seed,id,0,seconds,p.variation_seconds):0;s.cloud=range_sample(p.cloud,p.seed,id,1,seconds,p.variation_seconds);s.fog=range_sample(p.fog,p.seed,id,2,seconds,p.variation_seconds);s.thunder_interval=range_sample(p.thunder_interval,p.seed,id,3,seconds,p.variation_seconds);s.flash_strength=p.lightning_enabled?range_sample(p.flash_strength,p.seed,id,4,seconds,p.variation_seconds):0;s.thunder_volume=p.thunder_enabled?range_sample(p.thunder_volume,p.seed,id,5,seconds,p.variation_seconds):0;s.transition=transition;s.thunder_weight=p.thunder_enabled?1.f:0.f;s.lightning_weight=p.lightning_enabled?1.f:0.f;s.storm_weight=std::max(s.thunder_weight,s.lightning_weight);s.thunder_enabled=p.thunder_enabled;s.lightning_enabled=p.lightning_enabled;return s;
}
inline float area_weight(const PackedArea&a,double x,double z){if(!std::isfinite(a.effects[3]))return 0;const float influence=std::clamp(a.effects[3],0.f,1.f);const double d=std::hypot(x-a.bounds[0],z-a.bounds[1]);if(!std::isfinite(d)||d>=a.bounds[3])return 0;if(d<=a.bounds[2])return influence;return influence*(1-smooth(float((d-a.bounds[2])/(a.bounds[3]-a.bounds[2]))));}
// No allocation, no IO; output changes only after complete validation. Legacy
// baseline is copied exactly when no authored global/area influences the point.
inline bool valid(const Sample&s){return within(s.rain,0,1)&&within(s.cloud,0,1)&&within(s.fog,0,1.5)&&within(s.thunder_interval,1,180)&&within(s.flash_strength,0,4)&&within(s.thunder_volume,0,1)&&within(s.transition,0,120)&&within(s.storm_weight,0,1)&&within(s.thunder_weight,0,1)&&within(s.lightning_weight,0,1);}
inline bool pack(const Global&g,const BoundArea*areas,unsigned count,const Sample&legacy,double seconds,PackedFrame&out){
 if(!valid(g)||count>area_limit||(count&&!areas)||!valid(legacy))return false;
 PackedFrame p;p.global_authored=g.enabled;p.global=g.enabled?profile_sample(g.profile,0,seconds,g.transition):legacy;p.authored_count=g.enabled?1:0;
 for(unsigned i=0;i<count;++i){if(!areas[i].id||!valid(areas[i].area))return false;for(unsigned j=0;j<i;++j)if(areas[j].id==areas[i].id)return false;if(!areas[i].area.enabled)continue;const auto&a=areas[i].area;const auto s=profile_sample(a.profile,areas[i].id,seconds,a.transition);auto&q=p.areas[p.count++];q.id=areas[i].id;q.priority=a.priority;q.bounds={float(a.x),float(a.z),a.inner_radius,a.radius};q.channels={s.rain,s.cloud,s.fog,s.transition};q.effects={s.thunder_interval,s.flash_strength,s.thunder_volume,1};q.flags={s.thunder_weight,s.lightning_weight,s.storm_weight,0};++p.authored_count;}
 std::sort(p.areas.begin(),p.areas.begin()+p.count,[](const PackedArea&a,const PackedArea&b){return a.priority<b.priority||(a.priority==b.priority&&a.id<b.id);});out=p;return true;
}
inline Sample sample(const PackedFrame&p,double x,double z){Sample s=p.global;if(!std::isfinite(x)||!std::isfinite(z))return s;for(unsigned i=0;i<p.count&&i<area_limit;++i){const auto&a=p.areas[i];const float w=area_weight(a,x,z);if(w<=0)continue;auto blend=[&](float&v,float b){v+=(b-v)*w;};blend(s.rain,a.channels[0]);blend(s.cloud,a.channels[1]);blend(s.fog,a.channels[2]);blend(s.transition,a.channels[3]);blend(s.thunder_interval,a.effects[0]);blend(s.flash_strength,a.effects[1]);blend(s.thunder_volume,a.effects[2]);blend(s.thunder_weight,a.flags[0]);blend(s.lightning_weight,a.flags[1]);}s.storm_weight=std::max(s.thunder_weight,s.lightning_weight);s.thunder_enabled=s.thunder_weight>1.e-5f;s.lightning_enabled=s.lightning_weight>1.e-5f;return s;}
}
#endif

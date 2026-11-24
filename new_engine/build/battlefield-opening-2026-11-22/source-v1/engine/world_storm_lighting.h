#ifndef VALWOOD_WORLD_STORM_LIGHTING_H
#define VALWOOD_WORLD_STORM_LIGHTING_H
#include "engine/world_weather_authoring.h"
#include "engine/world_storm_controller.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
struct WorldStormLightFrame {double x=0,y=0,z=0;float color[3]={.68f,.80f,1.f};float radius=0,gain=0;bool active=false;};
namespace world_storm_lighting {
inline float smooth(float a,float b,float v){if(b<=a)return v>=b?1.f:0.f;const float t=std::clamp((v-a)/(b-a),0.f,1.f);return t*t*(3-2*t);}
inline bool valid_authored(const world_weather_authoring::PackedFrame&p){
 if(p.count>world_weather_authoring::area_limit||p.authored_count!=p.count+unsigned(p.global_authored)||!world_weather_authoring::valid(p.global))return false;
 for(unsigned i=0;i<p.count;++i){const auto&a=p.areas[i];for(float v:a.bounds)if(!std::isfinite(v))return false;for(float v:a.channels)if(!std::isfinite(v))return false;
  if(a.bounds[2]<0||a.bounds[3]<.1f||a.bounds[3]>10000||a.bounds[2]>a.bounds[3]||a.channels[0]<0||a.channels[0]>1||a.channels[1]<0||a.channels[1]>1||a.channels[2]<0||a.channels[2]>1.5f||a.channels[3]<0||a.channels[3]>120||!std::isfinite(a.effects[3])||a.effects[3]<0||a.effects[3]>1)return false;
 }return true;
}
struct Climate {float direct=1,sky=1,ground=1,environment=1,fog=1,desaturate=0,overcast=0;};
inline Climate climate(const world_weather_authoring::Sample&s,float sun_y,bool authored){
 Climate c;if(!authored||!world_weather_authoring::valid(s)||!std::isfinite(sun_y))return c;
 c.overcast=smooth(.58f,.96f,s.cloud);const float amount=std::clamp(std::max(c.overcast,std::max(s.rain*.85f,s.storm_weight*.7f)),0.f,1.f),day=smooth(-.12f,.2f,sun_y);
 c.direct=1-.24f*amount;c.sky=1-amount*(.12f+.18f*day);c.ground=1-.20f*amount;c.environment=1-amount*(.18f+.27f*day);c.fog=1-.22f*amount;c.desaturate=.76f*amount;return c;
}
// Serial-dependent 2--4 continuous pulses. The authoritative strike time,
// thunder delay and event strength remain unchanged; only visual discharge varies.
inline float pulse(std::uint64_t serial,double age){
 if(!std::isfinite(age)||age<0||age>=.6)return 0;const auto key=world_storm::hash(serial^0x5fc79b015aull);const unsigned count=2+unsigned(key%3);double value=0;
 for(unsigned i=0;i<count;++i){const auto k=world_storm::hash(key+i*71);const double start=i==0?0.:.075*i+world_storm::random(k)*.025,rise=.012+world_storm::random(k+1)*.014,decay=.025+world_storm::random(k+2)*.050,amplitude=i==0?1.:.40+world_storm::random(k+3)*.45;const double t=age-start;if(t>=0)value=std::max(value,amplitude*(t<rise?t/rise:std::exp(-(t-rise)/decay)));}
 return float(std::clamp(value,0.,1.))*float(1-smooth(.48f,.6f,float(age)));
}
inline WorldStormLightFrame flash(const world_storm::Output&o,float sun_y){
 WorldStormLightFrame f;const auto&e=o.event;if(!e.lightning||!e.serial||!std::isfinite(sun_y)||!world_storm::finite(e.x,-1.e8,1.e8)||!world_storm::finite(e.y,-1.e8,1.e8)||!world_storm::finite(e.z,-1.e8,1.e8)||!world_storm::finite(e.strength,0,4))return f;
 const float p=pulse(e.serial,o.strike_age),day=smooth(-.12f,.2f,sun_y);f.x=e.x;f.y=e.y;f.z=e.z;f.radius=std::clamp(520.f+e.strength*95.f,520.f,900.f);f.gain=p*e.strength*(4.3f-2.f*day);f.active=f.gain>.0001f;return f;
}
inline float attenuation(const WorldStormLightFrame&f,double x,double y,double z){if(!f.active||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(f.radius)||f.radius<=0)return 0;const double d=std::hypot(std::hypot(x-f.x,y-f.y),z-f.z);if(!std::isfinite(d)||d>=f.radius)return 0;const double w=1-d/f.radius;return float(w*w);}
inline bool valid(const WorldStormLightFrame&f){return world_storm::finite(f.x,-1.e8,1.e8)&&world_storm::finite(f.y,-1.e8,1.e8)&&world_storm::finite(f.z,-1.e8,1.e8)&&world_storm::finite(f.radius,0,2000)&&world_storm::finite(f.gain,0,32)&&world_storm::finite(f.color[0],0,4)&&world_storm::finite(f.color[1],0,4)&&world_storm::finite(f.color[2],0,4)&&(!f.active||f.radius>0);}
// Exact alpha-over ownership at a WORLD point: slot0 is the global baseline,
// slot i+1 is PackedFrame.areas[i]. This never changes condition samples.
inline float coefficient(const world_weather_authoring::PackedFrame&p,unsigned slot,double x,double z){
 if(slot>p.count||p.count>world_weather_authoring::area_limit)return 0;float w=slot==0?1.f:world_weather_authoring::area_weight(p.areas[slot-1],x,z);for(unsigned i=slot;i<p.count;++i)w*=1-world_weather_authoring::area_weight(p.areas[i],x,z);return std::clamp(w,0.f,1.f);
}
struct SourcePresence {float local=0,remote=0;double x=0,z=0;};
inline SourcePresence source_presence(const world_weather_authoring::PackedFrame&p,unsigned slot,double x,double z,std::uint64_t sequence=0,float reach=2000){
 SourcePresence out;out.x=x;out.z=z;if(slot>p.count||p.count>world_weather_authoring::area_limit||!std::isfinite(x)||!std::isfinite(z)||!std::isfinite(reach)||reach<=0)return out;out.local=coefficient(p,slot,x,z);if(out.local>1.e-5f)return out;
 const auto id=slot?p.areas[slot-1].id:0;double best=0;
 for(unsigned i=0;i<9;++i){double px=x,pz=z;if(slot){const auto&a=p.areas[slot-1];px=a.bounds[0];pz=a.bounds[1];if(i){const double angle=(i-1)*.7853981633974483;px+=std::sin(angle)*a.bounds[3]*.55;pz+=std::cos(angle)*a.bounds[3]*.55;}}
 else if(i){const double angle=(i-1)*.7853981633974483,radius=std::min(double(reach)*.72,240.+(i%3)*390.);px+=std::sin(angle)*radius;pz+=std::cos(angle)*radius;}
 const double distance=std::hypot(px-x,pz-z);if(distance>=reach)continue;const double visible=coefficient(p,slot,px,pz)*(1-smooth(reach*.35f,reach,float(distance)));const double score=visible*(.94+.06*world_storm::random(world_storm::hash(id^sequence^i*59)));if(score>best){best=score;out.x=px;out.z=pz;out.remote=float(visible*.35);}}
 return out;
}
struct Point {double x=0,y=0,z=0;};struct Segment {Point a,b;float width=1;};
struct Bolt {std::array<Segment,48> segments{};unsigned count=0;};
inline Bolt bolt(const world_storm::Event&e,double ground){
 Bolt result;if(!e.serial||!std::isfinite(ground)||!world_storm::finite(e.x,-1.e8,1.e8)||!world_storm::finite(e.y,-1.e8,1.e8)||!world_storm::finite(e.z,-1.e8,1.e8))return result;const auto key=world_storm::hash(e.serial^e.source_id);const unsigned n=16+unsigned(key%7);std::array<Point,23> trunk{};const double top=std::max(std::max(e.y,ground+420.),1050.+world_storm::random(key+1)*350.),lean_x=(world_storm::random(key+2)-.5)*180,lean_z=(world_storm::random(key+3)-.5)*180;
 auto append=[&](Point a,Point b,float width){if(result.count<result.segments.size())result.segments[result.count++]={a,b,width};};
 for(unsigned i=0;i<=n;++i){const double t=double(i)/n;const auto k=world_storm::hash(key+i*79);trunk[i]={e.x+lean_x*(1-t)+(world_storm::random(k)-.5)*55*std::sin(t*3.141592653589793),top+(ground-top)*t,e.z+lean_z*(1-t)+(world_storm::random(k+1)-.5)*55*std::sin(t*3.141592653589793)};if(i)append(trunk[i-1],trunk[i],float(.8+.3*world_storm::random(k+2)));}
 const unsigned branches=1+unsigned((key>>8)%3);for(unsigned branch=0;branch<branches;++branch){const auto k=world_storm::hash(key+branch*113);const unsigned start=3+unsigned(k%(n-5)),length=3+unsigned((k>>12)%5);Point a=trunk[start];const double angle=world_storm::random(k+1)*6.283185307179586;
 for(unsigned i=0;i<length;++i){const auto q=world_storm::hash(k+i*31);const double step=28.+world_storm::random(q)*42.;Point b{a.x+std::sin(angle)*step+(world_storm::random(q+1)-.5)*25,std::max(ground+3.,a.y-step*(.6+world_storm::random(q+2))),a.z+std::cos(angle)*step+(world_storm::random(q+3)-.5)*25};append(a,b,.48f*(1-.55f*float(i)/length));a=b;}}
 return result;
}
}
#endif

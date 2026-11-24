#ifndef VALWOOD_WORLD_STORM_CONTROLLER_H
#define VALWOOD_WORLD_STORM_CONTROLLER_H
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace world_storm {
inline constexpr unsigned source_limit=17,pending_limit=8;
// Scalar values come from the authored weather range sampler. Source positions
// and IDs are stable; only listener_relative sources move their origin at a NEW
// strike. A queued strike never chases the listener after it has been emitted.
struct Source {
 std::uint64_t id=0;std::uint32_t seed=1;double x=0,y=0,z=0;
 float radius=200,weight=1,interval=12,flash_strength=1,thunder_volume=.6f;
 float lightning_weight=1,thunder_weight=1;
 bool lightning_enabled=false,thunder_enabled=false,listener_relative=false;
};
struct Frame {double dt=0,x=0,y=0,z=0;bool focused=true,paused=false,loading=false,muted=false,enabled=true,discontinuity=false;};
struct Event {std::uint64_t serial=0,source_id=0;double x=0,y=0,z=0;float strength=0,volume=0,delay=0;bool lightning=false,thunder=false;};
struct Thunder {std::uint64_t serial=0;double x=0,y=0,z=0;float volume=0;};
struct Output {Event event;bool strike=false;float flash=0;double strike_age=0;std::array<Thunder,pending_limit> thunder{};unsigned thunder_count=0;};
struct Stats {std::uint64_t strikes=0,thunder_delivered=0,pending_dropped=0,cancelled=0,discontinuities=0;unsigned pending=0;};
struct Controller {
 struct Pending {Thunder event;double due=0;};
 std::array<Pending,pending_limit> pending{};unsigned pending_count=0;
 double clock=0,remaining=0,age=0,previous_x=0,previous_y=0,previous_z=0;
 std::uint64_t serial=0,sequence=0;bool armed=false,allowed=false,have_position=false,visual=false;
 Event active;Stats stats;
};
inline bool finite(double v,double lo,double hi){return std::isfinite(v)&&v>=lo&&v<=hi;}
inline std::uint64_t hash(std::uint64_t x){x+=0x9e3779b97f4a7c15ull;x=(x^(x>>30))*0xbf58476d1ce4e5b9ull;x=(x^(x>>27))*0x94d049bb133111ebull;return x^(x>>31);}
inline double random(std::uint64_t seed){return double(hash(seed)>>11)*(1./9007199254740992.);}
inline bool valid(const Source&s){return s.id&&finite(s.x,-1.e8,1.e8)&&finite(s.y,-1.e8,1.e8)&&finite(s.z,-1.e8,1.e8)&&finite(s.radius,0,10000)&&finite(s.weight,0,1)&&finite(s.interval,1,180)&&finite(s.flash_strength,0,4)&&finite(s.thunder_volume,0,1)&&finite(s.lightning_weight,0,1)&&finite(s.thunder_weight,0,1);}
inline bool valid(const Frame&f){return finite(f.dt,0,1)&&finite(f.x,-1.e8,1.e8)&&finite(f.y,-1.e8,1.e8)&&finite(f.z,-1.e8,1.e8);}
inline float envelope(double age){
 // Two short, continuous pulses, with a finite tail; no exposure or clock edits.
 if(age<0||age>=.6)return 0;
 const double first=age<.025?age/.025:std::exp(-(age-.025)/.055);
 const double second=age<.13?0:age<.15?(age-.13)/.02*.65:.65*std::exp(-(age-.15)/.075);
 return float(std::clamp(std::max(first,second),0.,1.));
}
inline void cancel(Controller&c){c.stats.cancelled+=c.pending_count;c.pending_count=0;c.stats.pending=0;c.visual=false;c.active={};c.age=0;c.armed=false;c.remaining=0;}
// Fixed bounded storage. Validation completes before changing state/output.
// Pausing, loading, focus/mute/enable changes and >100m teleports discard pending
// weather events and restart a full interval. Nothing catches up on resume.
inline bool update(Controller&c,const Source*s,unsigned count,const Frame&f,Output&out){
 if(count>source_limit||(count&&!s)||!valid(f))return false;
 std::array<unsigned,source_limit> order{};for(unsigned i=0;i<count;++i){if(!valid(s[i]))return false;for(unsigned j=0;j<i;++j)if(s[j].id==s[i].id)return false;order[i]=i;}
 std::sort(order.begin(),order.begin()+count,[&](unsigned a,unsigned b){return s[a].id<s[b].id;});
 Output o;const bool moved=c.have_position&&std::hypot(std::hypot(f.x-c.previous_x,f.z-c.previous_z),f.y-c.previous_y)>100;
 const bool permitted=f.enabled&&f.focused&&!f.paused&&!f.loading&&!f.muted;
 double total=0,rate=0;for(unsigned j=0;j<count;++j){const auto&a=s[order[j]];if(a.lightning_enabled||a.thunder_enabled){const double w=a.weight*std::max(a.lightning_enabled?a.lightning_weight:0.f,a.thunder_enabled?a.thunder_weight:0.f);total+=w;rate+=w/a.interval;}}
 if(!permitted||moved||f.discontinuity||!c.allowed||total<=0){if(c.armed||c.pending_count||c.visual)cancel(c);if(moved||f.discontinuity)++c.stats.discontinuities;}
 c.previous_x=f.x;c.previous_y=f.y;c.previous_z=f.z;c.have_position=true;c.allowed=permitted;
 if(!permitted||total<=0){out=o;return true;}
 c.clock+=f.dt;
 if(!c.armed){c.remaining=total/std::max(rate,1.e-9);c.armed=true;}
 else c.remaining-=f.dt;
 if(c.visual){c.age+=f.dt;if(c.age>=.6)c.visual=false;}
 for(unsigned i=0;i<c.pending_count;){if(c.pending[i].due<=c.clock){o.thunder[o.thunder_count++]=c.pending[i].event;++c.stats.thunder_delivered;c.pending[i]=c.pending[--c.pending_count];}else ++i;}
 if(c.remaining<=0){
  const double ticket=random(++c.sequence)*total;double accumulated=0;const Source*chosen=nullptr;
  for(unsigned j=0;j<count;++j){const auto&a=s[order[j]];if(!a.lightning_enabled&&!a.thunder_enabled)continue;accumulated+=a.weight*std::max(a.lightning_enabled?a.lightning_weight:0.f,a.thunder_enabled?a.thunder_weight:0.f);if(ticket<accumulated){chosen=&a;break;}}
  if(chosen){const auto&a=*chosen;const auto key=hash(c.sequence^a.id^(std::uint64_t(a.seed)<<32));const double angle=random(key)*6.283185307179586,rad=std::sqrt(random(key+1))*a.radius;
   Event e;e.serial=++c.serial;e.source_id=a.id;e.x=(a.listener_relative?f.x:a.x)+std::sin(angle)*rad;e.y=(a.listener_relative?f.y:a.y)+std::max(60.,double(a.radius)*.2);e.z=(a.listener_relative?f.z:a.z)+std::cos(angle)*rad;
   e.lightning=a.lightning_enabled&&a.lightning_weight>0;e.thunder=a.thunder_enabled&&a.thunder_weight>0;
   // Selection already carries each source's alpha-over coefficient. Applying
   // it again here would dim the feather between two equally strong storms.
   // The channel ratio preserves independent lightning/thunder contributions;
   // total effective coverage still fades a lone storm against clear weather.
   const double family=std::max(a.lightning_enabled?a.lightning_weight:0.f,a.thunder_enabled?a.thunder_weight:0.f);
   const float effective_gain=float(std::clamp(total,0.,1.)/std::max(family,1.e-9));
   e.strength=e.lightning?a.flash_strength*a.lightning_weight*effective_gain:0;e.volume=e.thunder?a.thunder_volume*a.thunder_weight*effective_gain:0;
   const double distance=std::hypot(std::hypot(e.x-f.x,e.z-f.z),e.y-f.y);e.delay=float(distance/343.);
   c.active=e;c.age=0;c.visual=e.lightning;o.strike=true;++c.stats.strikes;
   if(e.thunder&&e.volume>0){if(c.pending_count<pending_limit)c.pending[c.pending_count++]={{e.serial,e.x,e.y,e.z,e.volume},c.clock+e.delay};else ++c.stats.pending_dropped;}
   c.remaining=a.interval; // Deliberately discard any missed interval debt.
  }else c.remaining=total/std::max(rate,1.e-9);
 }
 c.stats.pending=c.pending_count;o.event=c.active;o.strike_age=c.age;o.flash=c.visual?envelope(c.age)*c.active.strength:0;out=o;return true;
}
}
#endif

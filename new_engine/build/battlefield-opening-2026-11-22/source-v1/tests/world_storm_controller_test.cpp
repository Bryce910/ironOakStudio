#include "engine/world_storm_controller.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <new>
#include <cstdlib>
static bool track=false;static unsigned allocs=0,checks=0;
void *operator new(std::size_t n){if(track)++allocs;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}
#define CHECK(x) do{++checks;assert(x);}while(false)
int main(){using namespace world_storm;Source s;s.id=1;s.interval=1;s.radius=0;s.x=343;s.lightning_enabled=s.thunder_enabled=true;Controller a,b;Frame f;f.dt=.01;Output x,y;unsigned events=0,heard=0;double event_time=0;std::uint64_t serial=0;track=true;
 for(unsigned i=0;i<1000;++i){CHECK(update(a,&s,1,f,x));CHECK(update(b,&s,1,f,y));CHECK(x.strike==y.strike&&x.event.x==y.event.x&&x.flash==y.flash&&x.thunder_count==y.thunder_count);CHECK(x.flash>=0&&x.flash<=s.flash_strength&&a.stats.pending<=8);if(x.strike){++events;CHECK(x.event.serial>serial&&x.event.x==343&&x.event.y==60&&x.event.z==0);serial=x.event.serial;event_time=a.clock;CHECK(std::abs(x.event.delay-std::hypot(343.,60.)/343.)<1.e-6);}if(x.thunder_count){heard+=x.thunder_count;CHECK(a.clock-event_time<.05);}}
 track=false;CHECK(allocs==0&&events==9&&heard>=8&&a.stats.strikes==events);
 // Source reorder is deterministic; only event-time global origins follow camera.
 Source ss[2]={s,s};ss[1].id=2;ss[1].seed=91;ss[1].radius=80;ss[1].listener_relative=true;Source reverse[2]={ss[1],ss[0]};a={};b={};for(unsigned i=0;i<1000;++i){f.x=i*.02;CHECK(update(a,ss,2,f,x)&&update(b,reverse,2,f,y));CHECK(x.strike==y.strike&&x.event.source_id==y.event.source_id&&x.event.x==y.event.x&&x.event.z==y.event.z);if(x.strike&&x.event.source_id==2)CHECK(std::hypot(x.event.x-f.x,x.event.z-f.z)<=80.00001);}
 // Alpha-over source selection and gain are separate: feathering two equal
 // full storms must not halve brightness/volume; a lone half-covered storm does.
 ss[0]=s;ss[1]=s;ss[1].id=2;ss[0].weight=ss[1].weight=.5f;a={};f={};f.dt=1;
 for(unsigned i=0;i<100;++i){CHECK(update(a,ss,2,f,x));if(x.strike)CHECK(std::abs(x.event.strength-1)<1.e-6&&std::abs(x.event.volume-.6f)<1.e-6);}
 a={};for(unsigned i=0;i<4;++i){CHECK(update(a,ss,1,f,x));if(x.strike)CHECK(std::abs(x.event.strength-.5f)<1.e-6&&std::abs(x.event.volume-.3f)<1.e-6);}
 ss[0].lightning_enabled=false;ss[1].thunder_enabled=false;a={};unsigned light_only=0,thunder_only=0;
 for(unsigned i=0;i<200;++i){CHECK(update(a,ss,2,f,x));if(x.strike){if(x.event.source_id==1){++thunder_only;CHECK(!x.event.lightning&&x.event.thunder&&x.event.strength==0&&std::abs(x.event.volume-.6f)<1.e-6);}else{++light_only;CHECK(x.event.lightning&&!x.event.thunder&&x.event.volume==0&&std::abs(x.event.strength-1)<1.e-6);}}}CHECK(light_only>60&&thunder_only>60);
 a={};ss[0].lightning_enabled=true;ss[0].lightning_weight=.2f;ss[0].thunder_weight=.8f;for(unsigned i=0;i<4;++i){CHECK(update(a,ss,1,f,x));if(x.strike)CHECK(std::abs(x.event.strength-.1f)<1.e-6&&std::abs(x.event.volume-.24f)<1.e-6);}
 // Pauses/focus/loading/mute/teleports erase pending events and do not catch up.
 for(unsigned mode=0;mode<6;++mode){a={};f={};f.dt=.1;for(unsigned i=0;i<12;++i)CHECK(update(a,&s,1,f,x));CHECK(a.stats.strikes==1&&a.pending_count==1);if(mode==0)f.paused=true;if(mode==1)f.focused=false;if(mode==2)f.loading=true;if(mode==3)f.muted=true;if(mode==4)f.discontinuity=true;if(mode==5)f.x=1000;CHECK(update(a,&s,1,f,x));CHECK(a.pending_count==0&&!x.strike&&x.flash==0);f.paused=f.loading=f.muted=f.discontinuity=false;f.focused=true;for(unsigned i=0;i<5;++i){CHECK(update(a,&s,1,f,x));CHECK(!x.strike&&x.thunder_count==0);} }
 // Independent switches, bounded overflow and invalid input atomicity.
 a={};f={};f.dt=1;s.lightning_enabled=false;for(unsigned i=0;i<4;++i){CHECK(update(a,&s,1,f,x));CHECK(x.flash==0&&!x.event.lightning);}CHECK(a.stats.thunder_delivered>0);s.lightning_enabled=true;s.thunder_enabled=false;a={};for(unsigned i=0;i<4;++i)CHECK(update(a,&s,1,f,x));CHECK(a.stats.strikes>0&&a.stats.thunder_delivered==0&&a.pending_count==0);
 s.thunder_enabled=true;s.x=100000;a={};for(unsigned i=0;i<40;++i)CHECK(update(a,&s,1,f,x));CHECK(a.pending_count==8&&a.stats.pending_dropped>0);const auto before=a.stats.strikes;const auto clock=a.clock;s.weight=-1;CHECK(!update(a,&s,1,f,x)&&a.clock==clock&&a.stats.strikes==before);s.weight=1;f.dt=std::numeric_limits<double>::quiet_NaN();CHECK(!update(a,&s,1,f,x)&&a.clock==clock);f.dt=1;Source duplicate[2]={s,s};CHECK(!update(a,duplicate,2,f,x));CHECK(!update(a,&s,18,f,x));
 for(unsigned i=0;i<601;++i){const auto e=envelope(i/1000.);CHECK(e>=0&&e<=1);if(i)CHECK(std::abs(e-envelope((i-1)/1000.))<.05);}CHECK(envelope(.6)==0);
 std::printf("PASS %u storm checks: deterministic/reordered sources, fixed origins, distance-delayed thunder, independent switches, pause/focus/loading/mute/teleport cancellation, bounded overflow, atomic invalid input and zero hot allocations\n",checks);
}

#include "engine/world_weather_wetness.h"
#include <cstdlib>
#include <iostream>
#include <new>
#include <cstring>
static std::uint64_t allocations=0;
void* operator new(std::size_t n){++allocations;if(void*p=std::malloc(n))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete(void*p,std::size_t)noexcept{std::free(p);}
unsigned checks=0;
void check(bool b,const char*l){++checks;if(!b){std::cerr<<"FAIL "<<l<<'\n';std::exit(1);}}
int main(){
 using namespace world_weather_authoring;namespace wet=world_weather_wetness;
 Sample legacy;Global g;PackedFrame rain;wet::State s;
 for(unsigned i=0;i<100;++i)check(wet::update(s,rain,.8,1./60),"legacy update valid");check(!wet::packet(s).enabled,"never-authored project remains exact old material path");
 g.enabled=true;g.profile.rain={0,0};check(pack(g,nullptr,0,legacy,0,rain),"dry global pack");check(wet::update(s,rain,0,0),"authored dry startup");check(s.global.damp==0&&s.global.film==0,"dry startup is dry");
 g.profile.rain={1,1};pack(g,nullptr,0,legacy,0,rain);for(unsigned i=0;i<1080;++i)wet::update(s,rain,0,1./60);
 wet::State late_enable;check(wet::update(late_enable,{},0,1./60),"legacy frames before authoring");check(wet::update(late_enable,rain,0,1./60,true,false),"first live enable may choose gradual uptake");check(late_enable.global.damp>0&&late_enable.global.damp<.002&&late_enable.global.film<.001,"live first storm does not instantly soak dry world");wet::State loaded;wet::update(loaded,rain,0,0);check(loaded.global.damp==1&&loaded.global.film==1,"fresh loaded storm still seeds existing saturation");
 check(s.global.damp>.63&&s.global.damp<.64,"18sec porous uptake analytic");check(s.global.film>0&&s.global.film<s.global.damp,"film follows saturation gradually");
 auto frozen=s;check(wet::update(s,rain,0,1,false),"paused update valid");check(std::memcmp(&s,&frozen,sizeof(s))==0,"pause/focus guard preserves entire state");
 const auto wet_before=s.global;g.profile.rain={0,0};pack(g,nullptr,0,legacy,0,rain);wet::update(s,rain,0,1./60);check(s.global.damp>.99*wet_before.damp&&s.global.film>.99*wet_before.film,"stopping rain does not instantly dry film");for(unsigned i=0;i<12600;++i)wet::update(s,rain,0,1./60);check(s.global.damp>.22&&s.global.damp<.24,"210sec damp drying time");
 std::array<BoundArea,16>a;for(unsigned i=0;i<a.size();++i){a[i].id=i+1;a[i].area.x=-11933.+double(i)*200;a[i].area.z=5310;a[i].area.inner_radius=8;a[i].area.radius=20;a[i].area.priority=int(i);a[i].area.profile.rain={1,1};}
 wet::State local;g.enabled=false;pack(g,a.data(),1,legacy,0,rain);check(wet::update(local,rain,0,0),"local startup");auto p=wet::packet(local);check(wet::sample(p,-11933,5310).damp==1&&wet::sample(p,0,0).damp==0,"loaded local storm seeds only its fixed world footprint");
 a[0].area.x+=100;pack(g,a.data(),1,legacy,0,rain);wet::update(local,rain,0,1./60);p=wet::packet(local);check(p.count==2,"moved area leaves distinct residual generation");check(wet::sample(p,-11933,5310).damp>.99,"old footprint remains wet after move");check(wet::sample(p,a[0].area.x,5310).damp<.01,"new footprint does not teleport old saturation");
 check(wet::sample(p,-11933,5310).damp==wet::sample(p,-11933,5310).damp,"sampling independent of camera/cache origin");
 PackedFrame empty;wet::update(local,empty,0,1./60);check(wet::sample(wet::packet(local),-11933,5310).damp>.99,"removed area retains visible residual wet footprint");
 wet::State full;pack(g,a.data(),16,legacy,0,rain);check(wet::update(full,rain,0,0),"all16 live wet areas");for(unsigned move=0;move<6;++move){for(auto&v:a)v.area.x+=50;pack(g,a.data(),16,legacy,0,rain);check(wet::update(full,rain,0,1./60),"bounded overflow update");check(full.count<=32&&wet::valid(wet::packet(full)),"residual cap valid");for(const auto&v:a){bool found=false;for(unsigned j=0;j<full.count;++j)found|=full.layers[j].id==v.id&&full.layers[j].bounds[0]==float(v.area.x)&&full.layers[j].live;check(found,"current authored area never evicted");}}
 check(full.evictions>0,"overflow is explicit counted residual eviction");
 wet::State overlap;std::array<BoundArea,2>layers;layers[0].id=3;layers[0].area.inner_radius=layers[0].area.radius=5;layers[0].area.profile.rain={1,1};layers[1]=layers[0];layers[1].id=2;layers[1].area.priority=1;layers[1].area.profile.rain={0,0};pack(g,layers.data(),2,legacy,0,rain);wet::update(overlap,rain,0,0);auto ordered=wet::packet(overlap);check(wet::sample(ordered,0,0).damp==0,"higher priority dry shelter masks wet underlayer");check(wet::sample(ordered,5,0).damp==0,"hard edge equals outside without division");std::swap(layers[0],layers[1]);PackedFrame reordered;pack(g,layers.data(),2,legacy,0,reordered);wet::update(overlap,reordered,0,0);auto same=wet::packet(overlap);check(same.count==ordered.count&&same.layers[0].channels==ordered.layers[0].channels&&same.layers[1].channels==ordered.layers[1].channels,"input order does not change stable priority composition");
 // Restore max16 sources for the allocation witness below.
 pack(g,a.data(),16,legacy,0,rain);
 auto bad=wet::packet(full);bad.count=33;check(!wet::valid(bad),"invalid packet count");bad=wet::packet(full);bad.layers[0].channels[0]=NAN;check(!wet::valid(bad),"invalid packet channel");auto before=full;check(!wet::update(full,rain,0,NAN),"NaN dt rejected");check(std::memcmp(&before,&full,sizeof(full))==0,"invalid update preserves state");
 const auto started=allocations;for(unsigned i=0;i<10000;++i){wet::update(full,rain,0,1./60);auto q=wet::packet(full);auto v=wet::sample(q,-11933,5310);if(!std::isfinite(v.damp))std::abort();}check(allocations==started,"10000 fixed16-area updates/packets/samples allocate nothing");
 std::cout<<"PASS "<<checks<<" world wet-history startup/move/dry/priority/cap/noallocation checks\n";
}

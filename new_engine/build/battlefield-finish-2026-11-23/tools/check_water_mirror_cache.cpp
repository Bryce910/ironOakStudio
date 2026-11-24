// CPU contract checks for stationary reflection cadence. These do not claim
// visual acceptance: river banks and moving reflections still need native QA.
#include "engine/world_water_mirror_cache.h"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

int main(){
 using Cache=WorldWaterMirrorCache;unsigned checks=0;
 auto require=[&](bool ok,const char *why){++checks;if(!ok)throw std::runtime_error(why);};
 Cache::Pose p;p.x=-12093.71544;p.y=13.08;p.z=5155.02;p.yaw=1.768;p.pitch=-.04;p.fov=.82;p.aspect=1600./900.;p.level=10.7;p.width=140;p.height=79;
 Cache::Projection vp={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
 Cache cache;require(!cache.can_reuse(1,p),"uncaptured image cannot be reused");
 require(cache.commit(100,p,vp),"valid paired capture commits");
 require(cache.can_reuse(101,p),"legacy caller reuses the next frame");
 require(!cache.can_reuse(102,p),"legacy caller still captures every second frame");
 require(!cache.can_reuse(100,p,false,false,4),"capture frame cannot reuse its own prior work");
 require(!cache.can_reuse(99,p,false,false,4),"rewound frame cannot use future image");
 for(unsigned period=1;period<=4;++period){
  Cache tier;unsigned captures=0,reuses=0;
  // Simulate each tier across several complete intervals. Reused frames must
  // preserve both the actual image's capture projection and its origin.
  for(std::uint64_t frame=1;frame<=24;++frame){
   if(tier.can_reuse(frame,p,false,false,period)){++reuses;require(tier.projection()==vp,"reuse retains capture projection");require(tier.rebased_projection(p)==vp,"stationary rebase retains exact mirrored VP");}
   else{++captures;require(tier.commit(frame,p,vp),"scheduled capture succeeds");}
  }
  require(captures==24/period,"capture frequency matches chosen tier");
  require(reuses==24-captures,"bounded reuse count matches chosen tier");
 }
 require(!cache.can_reuse(101,p,false,false,0),"zero cadence fails closed");
 require(!cache.can_reuse(101,p,false,false,5),"unbounded cadence fails closed");
 for(unsigned period=1;period<=4;++period)for(unsigned age=1;age<period;++age){
  const auto frame=100+age;require(cache.can_reuse(frame,p,false,false,period),"stationary image valid before next scheduled capture");
  require(!cache.can_reuse(frame,p,true,false,period),"forced quality override recaptures immediately");
  require(!cache.can_reuse(frame,p,false,true,period),"temporal cut recaptures immediately");
  // Each creates newly exposed pixels, a new plane or a different render
  // target. It must bypass cadence, including on the last reusable frame.
  for(unsigned field=0;field<11;++field){auto changed=p;
   switch(field){case 0:changed.x+=.001;break;case 1:changed.y+=.001;break;case 2:changed.z+=.001;break;
    case 3:changed.yaw+=.001;break;case 4:changed.pitch+=.001;break;case 5:changed.fov+=.001;break;
    case 6:changed.aspect+=.001;break;case 7:changed.level+=.001;break;case 8:++changed.width;break;
    case 9:++changed.height;break;default:changed.yaw=std::numeric_limits<double>::quiet_NaN();break;}
   require(!cache.can_reuse(frame,changed,false,false,period),"camera/plane/size/invalid change recaptures immediately");
  }
 }
 for(unsigned period=1;period<=4;++period)require(!cache.can_reuse(100+period,p,false,false,period),"image expires exactly at tier boundary");
 // Float-level origin noise retains the legacy tolerance; real camera moves
 // above it never gain a longer reuse window from a lower quality tier.
 auto noise=p;noise.x+=1.e-7;require(cache.can_reuse(101,noise),"legacy numerical tolerance remains unchanged");
 Cache edge;require(edge.commit(std::numeric_limits<std::uint64_t>::max()-1,p,vp),"large frame capture valid");
 require(edge.can_reuse(std::numeric_limits<std::uint64_t>::max(),p,false,false,4),"large frame age subtraction does not overflow");
 require(!edge.can_reuse(0,p,false,false,4),"wrapped frame counter fails closed");
 require(edge.commit(std::numeric_limits<std::uint64_t>::max(),p,vp),"terminal frame capture valid");
 require(!edge.can_reuse(0,p,false,false,4),"terminal capture cannot certify a wrapped image");
 auto invalid=vp;invalid[3]=std::numeric_limits<float>::infinity();require(!cache.commit(105,p,invalid),"invalid capture projection rejected");
 require(!cache.valid()&&!cache.can_reuse(106,p,false,false,4),"partially failed capture invalidates cached image");
 require(cache.commit(110,p,vp),"valid capture recovers after invalidation");cache.invalidate();require(!cache.can_reuse(111,p,false,false,4),"explicit invalidation wins over cadence");
 std::cout<<"PASS "<<checks<<" mirror cache checks; default2, Medium4, High2, Ultra1 capture policy supported\n";
}

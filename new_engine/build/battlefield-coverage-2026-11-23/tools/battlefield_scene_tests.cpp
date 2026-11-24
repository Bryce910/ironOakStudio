#include "engine/world_battlefield_scene.h"
#include <cassert>
#include <cstdio>
#include <set>
using namespace world_battlefield_scene;
int main(){
 assert(world_atlas_init());char error[256]={};
 assert(world_atlas_load_height_region("ground/showcase.height",error,sizeof error)==1);
 Director d;d.prepare();unsigned checks=0;std::set<unsigned long long> hits;
 for(unsigned q=0;q<4;++q)for(unsigned step=0;step<12000;++step){
  double t=step/60.;auto f=d.sample(t,q,-12115,5150,true);assert(f.visible);assert(f.actor_count<=actor_capacity&&f.arrow_count<=arrow_capacity&&f.impact_count<=8);++checks;
  assert(f.actor_count==11u);
  for(unsigned i=0;i<f.actor_count;++i){const auto&a=f.actors[i];assert(std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z)&&std::isfinite(a.yaw));assert(a.progress>=0&&a.progress<=1);assert(std::abs(a.y-world_atlas_walk_height(a.x,a.z))<.0001);++checks;}
  for(unsigned i=0;i<f.arrow_count;++i){const auto&a=f.arrows[i];double n=0;for(unsigned k=0;k<3;++k){assert(std::isfinite(a.base[k])&&std::isfinite(a.tip[k]));n+=(a.base[k]-a.tip[k])*(a.base[k]-a.tip[k]);}assert(std::abs(std::sqrt(n)-.82)<1e-7);++checks;}
  for(unsigned i=0;i<f.impact_count;++i){assert(f.impacts[i].serial>0);if(f.impacts[i].blood)assert(f.impacts[i].serial<=3);hits.insert(f.impacts[i].serial);}
 }
 assert(!d.sample(1,3,0,0).visible);assert(!d.sample(1,3,-12115,5150,false).visible);assert(!d.sample(-1,3,-12115,5150).visible);
 auto before=d.sample(2.3,3,-12115,5150),after=d.sample(5,3,-12115,5150);assert(before.actors[8].motion==Motion::Guard);assert(after.actors[8].motion==Motion::Dead);
 assert(hits.count(1));for(const auto&f:d.flames()){assert(f.y>world_atlas_height(f.x,f.z));assert(f.height>0&&f.width>0);}checks+=8;
 std::printf("Battlefield scene passed %u finite terrain/actor/projectile checks, %zu distinct impact events\n",checks,hits.size());world_atlas_shutdown();
}

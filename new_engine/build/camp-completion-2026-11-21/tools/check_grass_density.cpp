
#include "engine/world_grass_density.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <limits>
#include <vector>
struct Root {float yaw,scale,fade;};
int main(){
 constexpr unsigned count=200000;std::vector<Root> roots;roots.reserve(count);
 std::uint64_t state=0x13041980ull;auto random=[&](){state=state*6364136223846793005ull+1442695040888963407ull;return float((state>>40)*(1.0/16777216.0));};
 for(unsigned i=0;i<count;i++)roots.push_back({random()*6.2831853f,.65f+random()*.8f,float(i%7)/6});
 constexpr float scales[]={.35f,.60f,.80f,1.f};std::array<unsigned,4> tufts={},blades={};
 for(const auto&a:roots){bool old_tuft=false,old_blade=false;for(unsigned tier=0;tier<4;tier++){
  const auto k=world_grass_density::classify(a.yaw,a.scale,scales[tier],tier);
  const bool blade=k.density&&k.blades;assert(!old_tuft||k.density);assert(!old_blade||blade);old_tuft=k.density;old_blade=blade;tufts[tier]+=k.density;blades[tier]+=blade;
  const auto repeat=world_grass_density::classify(a.yaw,a.scale,scales[tier],tier);assert(k.density==repeat.density&&k.blades==repeat.blades);
  if(tier==3){const bool baseline=world_ground_cover_quality::keep(world_ground_cover_quality::generated_seed(a.yaw,a.scale)^0x4c957f2du,world_ground_cover_quality::profile(3).grass_blade_keep);assert(k.density&&k.blades==baseline);}
 }
 assert(!world_grass_density::classify(a.yaw,a.scale,0,3).density);
 assert(world_grass_density::classify(a.yaw,a.scale,std::numeric_limits<float>::quiet_NaN(),3).density);
 }
 for(unsigned t=0;t<4;t++){assert(std::abs(double(tufts[t])/count-scales[t])<.006);assert(std::abs(double(blades[t])/count-scales[t]*world_ground_cover_quality::profile(t).grass_blade_keep)<.006);std::printf("tier=%u input_roots=%u retained_scanned=%u retained_short_blades=%u density=%.2f PASS\n",t,count,tufts[t],blades[t],scales[t]);}
 assert(tufts[3]==count);for(unsigned t=1;t<4;t++)assert(tufts[t]>tufts[t-1]&&blades[t]>blades[t-1]);
 // Reordering/compaction and arbitrary cache-anchor changes cannot change rank:
 // the production helper takes only the original world-generated yaw/scale.
 for(unsigned i=0;i<count;i+=97){const auto&a=roots[i];auto first=world_grass_density::classify(a.yaw,a.scale,.6f,1);auto moved=a; moved.fade=1-a.fade;auto after=world_grass_density::classify(moved.yaw,moved.scale,.6f,1);assert(first.density==after.density&&first.blades==after.blades);}
 std::puts("Nested grass sets, deterministic rooted selection, fade-independent membership, Ultra baseline equivalence and effective density proportions PASS");
}

#include "engine/world_detached_physics.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace world_detached_physics;
struct Terrain {double slope=0;bool bump=false;};
double ground(double x,double z,void *ptr){const auto &g=*static_cast<Terrain*>(ptr);return g.slope*(x+.3*z)+(g.bump?.035*std::exp(-(x*x+z*z)/1.8):0);}
struct Trial {Body state;double sleep=-1,penetration=0;Stats stats;};
std::vector<Vec3> shape(unsigned part,bool mirror){const Vec3 dims={part==2?.34:part==1?.15:.11,part==1?.17:part==2?.40:part>=7?.46:part>=5?.10:.29,part==2?.18:part==1?.13:.09};std::vector<Vec3> p;
 for(int x:{-1,1})for(int y:{-1,1})for(int z:{-1,1})p.push_back({dims.x*x*(mirror?-1:1),dims.y*y,dims.z*z});return p;}
Trial run(const std::vector<Vec3> &points,unsigned part,double fps,Terrain terrain){Trial out;auto &b=out.state;b.position={0,1.2,0};b.orientation=yaw(.63);initialize(b,{points.data(),points.size()},part==2?30:part==1?4.5:part>=7?8.5:part>=5?.65:3.5);
 const Vec3 j=unit({1,.25,.4})*(1.9/b.inverse_mass)+Vec3{0,1.05/b.inverse_mass,0};impulse(b,j,{0,.25,.04});double floor=ground(0,0,&terrain);
 for(unsigned i=0;i<unsigned(fps*8);++i){update(b,{points.data(),points.size()},1/fps,ground,&terrain,floor,out.stats);assert(finite(b.position)&&finite(b.velocity)&&finite(b.omega)&&finite(b.orientation));assert(std::abs(b.orientation.x*b.orientation.x+b.orientation.y*b.orientation.y+b.orientation.z*b.orientation.z+b.orientation.w*b.orientation.w-1)<1e-10);
  for(auto p:points){p=rotate(b.orientation,p)+b.position;out.penetration=std::max(out.penetration,std::max(0.,ground(p.x,p.z,&terrain)-p.y));}
  if(b.sleeping){out.sleep=(i+1)/fps;const auto frozen=b;for(unsigned n=0;n<120;++n)update(b,{points.data(),points.size()},1/fps,ground,&terrain,floor,out.stats);assert(b.position.x==frozen.position.x&&b.position.y==frozen.position.y&&b.position.z==frozen.position.z&&b.orientation.x==frozen.orientation.x&&b.orientation.y==frozen.orientation.y&&b.orientation.z==frozen.orientation.z&&b.orientation.w==frozen.orientation.w);break;}
 }return out;}
int main(){unsigned trials=0;double maxsleep=0,maxplane=0,maxbump=0,maxfps=0;
 for(unsigned part=1;part<=8;++part)for(bool mirror:{false,true})for(double slope:{0.,.1}){const auto points=shape(part,mirror);const auto a=run(points,part,30,{slope,false}),b=run(points,part,60,{slope,false}),c=run(points,part,120,{slope,false});
  for(const auto *v:{&a,&b,&c}){++trials;assert(v->sleep>0&&v->sleep<=3.5);assert(!v->stats.invalid);maxsleep=std::max(maxsleep,v->sleep);maxplane=std::max(maxplane,v->penetration);assert(v->penetration<2e-5);}
  maxfps=std::max(maxfps,length(a.state.position-b.state.position));maxfps=std::max(maxfps,length(b.state.position-c.state.position));assert(maxfps<1e-9);
  const auto bump=run(points,part,60,{slope,true});maxbump=std::max(maxbump,bump.penetration);
 }
 auto points=shape(1,false);Body b;b.position={0,2,0};initialize(b,{points.data(),points.size()},4);Stats s;double floor=0;Terrain g;const auto old=b;update(b,{points.data(),points.size()},0,ground,&g,floor,s);assert(b.position.y==old.position.y&&b.accumulator==old.accumulator&&!s.substeps);
 update(b,{points.data(),points.size()},1,ground,&g,floor,s);assert(s.clamps==1&&s.substeps==6&&b.position.y>1.9);const auto before=b;update(b,{points.data(),points.size()},std::numeric_limits<double>::quiet_NaN(),ground,&g,floor,s);assert(s.invalid==1&&b.position.y==before.position.y);
 std::printf("PASS %u mirrored8part30/60/120flat/slope trials; maxsleep%.3fs maxplane%.9gm maxbumpApprox%.9gm fpsDelta%.9g; pause/large-dt/invalid/sleepdrift PASS\n",trials,maxsleep,maxplane,maxbump,maxfps);
}

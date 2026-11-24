#include "engine/world_castle_visibility.h"
#include <cassert>
#include <cstdio>
#include <cmath>
#include <limits>
#include <cstdint>
namespace {
// Independent exhaustive8corner interval reference retained for regression
// against the optimized monotonic extrema and convex broadphases.
bool reference_wall(const WorldCastleOcclusionView &v,bool xp,double plane,double xmin,double ymin,double zmin,double xmax,double ymax,double zmax){
    const double ea=xp?v.x:v.z,eb=xp?v.z:v.x,oa0=xp?xmin:zmin,oa1=xp?xmax:zmax,ob0=xp?zmin:xmin,ob1=xp?zmax:xmax;
    const double e0=ea-v.horizontal_guard,e1=ea+v.horizontal_guard;
    if(!((e1<plane-.5&&oa0>plane+.5)||(e0>plane+.5&&oa1<plane-.5)))return false;
    double low=1,high=0;
    for(double f:{plane-.5,plane+.5})for(double e:{e0,e1})for(double o:{oa0,oa1}){double t=(f-e)/(o-e);low=std::min(low,t);high=std::max(high,t);}
    if(low<=0||high>=1)return false;
    double b0,b1,y0,y1;
    world_castle_visibility_detail::project_interval(low,high,eb-v.horizontal_guard,eb+v.horizontal_guard,ob0,ob1,b0,b1);
    world_castle_visibility_detail::project_interval(low,high,v.y-v.vertical_guard,v.y+v.vertical_guard,ymin,ymax,y0,y1);
    if(y0<24.50||y1>30.80)return false;
    using world_castle_visibility_detail::one_piece;
    return xp?one_piece(b0,b1,-195,-55,13):plane==-195?one_piece(b0,b1,-65,65,12):one_piece(b0,b1,-65,-5.9,6)||one_piece(b0,b1,5.9,65,6);
}
bool reference(const WorldCastleOcclusionView &v,double x0,double y0,double z0,double x1,double y1,double z1){
    if(!v.active)return false;
    return reference_wall(v,true,65,x0,y0,z0,x1,y1,z1)||reference_wall(v,true,-65,x0,y0,z0,x1,y1,z1)||reference_wall(v,false,-195,x0,y0,z0,x1,y1,z1)||reference_wall(v,false,-55,x0,y0,z0,x1,y1,z1);
}
std::uint64_t state=0x123456789abcdef0ull;
double random01(){state=state*6364136223846793005ull+1442695040888963407ull;return double(state>>11)*(1./9007199254740992.);}
}
int main(){
    const auto view=world_vegetation_visibility_view(91,25.75,-112,-2.39321461,-.1,16./9.);
    assert(view.valid);
    assert(!world_vegetation_sphere_outside(view,85,24.2,-106,.6,90));
    assert(world_vegetation_sphere_outside(view,91+110,24,-112,.6,90));
    assert(!world_vegetation_sphere_outside(view,91+92,24,-112,100,90)); // guarded root fade boundary
    assert(!world_vegetation_sphere_outside({},0,0,0,1,90));
    assert(!world_vegetation_sphere_outside(view,NAN,0,0,1,90));
    const auto wall=world_castle_occlusion_view(91,25.75,-112);
    assert(wall.active);
    assert(world_castle_wall_occludes(wall,39.7,24.,-112.3,40.3,24.5,-111.7));
    assert(!world_castle_wall_occludes(wall,84.7,24.,-112.3,85.3,24.5,-111.7)); // visible front bank
    assert(!world_castle_wall_occludes(wall,39,24,-113,41,60,-111)); // canopy above wall
    assert(!world_castle_wall_occludes(world_castle_occlusion_view(91,174,-112),39,24,-113,41,24.5,-111));
    assert(!world_castle_wall_occludes(world_castle_occlusion_view(0,25.75,-30),-.5,24,-90,.5,24.5,-89)); // south gate
    assert(!world_castle_wall_occludes(world_castle_occlusion_view(91,25.75,-55),39,24,-56,41,24.5,-54)); // tower/end
    assert(!world_castle_wall_occludes(world_castle_occlusion_view(91,25.75,-108.846153846),39.9,24,-108.95,40.1,24.5,-108.75)); // join
    assert(!world_castle_wall_occludes(world_castle_occlusion_view(1000,25,-112),39,24,-113,41,24.5,-111));
    // Dense ground-root sweep: any root rejected by the cached frustum remains
    // invisible for every allowed camera translation/turn, including source
    // geometry offsets within its .6m sphere. Test actual side/top/near planes.
    std::size_t rejected=0,checks=0;
    for(int yaw=0;yaw<8;yaw++)for(int iz=-95;iz<=95;iz+=5)for(int ix=-95;ix<=95;ix+=5){
        auto base=world_vegetation_visibility_view(0,25.75,0,yaw*.7853981634,-.1,16./9.);
        if(!world_vegetation_sphere_outside(base,ix,24.2,iz,.6,90))continue;
        rejected++;
        for(int sx:{-1,1})for(int sz:{-1,1})for(int sy:{-1,1})for(int turn:{-1,1}){
            auto actual=world_vegetation_visibility_view(sx*.75,25.75+sy*.25,sz*.75,yaw*.7853981634+turn*.01,-.1+turn*.005,16./9.);
            for(int gx:{-1,1})for(int gy:{-1,1})for(int gz:{-1,1}){
                const double x=ix+gx*.3464101615,y=24.2+gy*.3464101615,z=iz+gz*.3464101615;
                const double dx=x-actual.x,dy=y-actual.y,dz=z-actual.z;
                const double along=dx*actual.fx+dy*actual.fy+dz*actual.fz;
                const bool visible=std::hypot(double(ix)-actual.x,double(iz)-actual.z)<90&&along>0&&std::abs(dx*actual.rx+dz*actual.rz)<=along*actual.tan_h&&std::abs(dx*actual.ux+dy*actual.uy+dz*actual.uz)<=along*actual.tan_v;
                assert(!visible);checks++;
            }
        }
    }
    constexpr double cameras[][3]={{91,25.75,-112},{-91,25.75,-112},{0,25.75,-100},{0,25.75,-30},{0,25.75,-220},{280,25.75,-100},{91,174,-112}};
    std::size_t equivalent=0,hidden=0;
    for(unsigned i=0;i<300000;i++){
        const auto &c=cameras[i%7];const auto v=world_castle_occlusion_view(c[0]+random01()*2-1,c[1]+random01()*.5-.25,c[2]+random01()*2-1);
        const double x=random01()*420-210,z=random01()*250-250,r=.25+random01()*.75,y=24+random01()*.2,h=.2+random01()*3;
        const bool a=world_castle_wall_occludes(v,x-r,y-.05,z-r,x+r,y+h,z+r),b=reference(v,x-r,y-.05,z-r,x+r,y+h,z+r);
        assert(a==b);equivalent++;hidden+=a;
    }
    assert(hidden>1000);
    std::printf("PASS exact optimized/exhaustive wall equivalence: %zu AABBs, %zu hidden; full slab/camera guards unchanged\n",equivalent,hidden);
    std::printf("PASS castle visibility: guarded grass frustum/fade roots%zu, cached-camera/source-corner checks%zu; frontbank/gate/join/canopy/fly guards retained\n",rejected,checks);
}

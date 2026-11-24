#include "engine/world_torch_lights.h"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
namespace {std::atomic<std::size_t> allocations{0};}
[[gnu::noinline]] void *operator new(std::size_t n){allocations++;if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,std::size_t)noexcept{std::free(p);}
namespace {
VillageTorchEmitter emitter(double x,double z,unsigned seed) {
    return {x,25,z,.4f,.13f,20.f,seed,0,0,1,200,-300,-300,300,300,23,32};
}
void cluster(const WorldTorchLights &out) {
    assert(out.count<=8);
    for(unsigned i=0;i<out.count;i++)for(unsigned axis=0;axis<3;axis++) {
        assert(std::isfinite(out.position_radius[i][axis])&&out.position_radius[i][3]==8);
        assert(out.cluster_min[axis]<=out.position_radius[i][axis]-8);
        assert(out.cluster_max[axis]>=out.position_radius[i][axis]+8);
        assert(std::isfinite(out.color_intensity[i][axis]));
    }
}
std::array<float,9> by_source(const WorldTorchLights &out,const std::array<VillageTorchEmitter,9> &sources) {
    std::array<float,9> result={};
    for(unsigned i=0;i<out.count;i++) {
        unsigned found=9;for(unsigned k=0;k<9;k++)if(out.position_radius[i][0]==float(sources[k].x))found=k;
        assert(found<9);result[found]=out.color_intensity[i][3];
    }
    return result;
}
}
int main() {
    std::array<VillageTorchEmitter,WORLD_TORCH_FLAME_CAPACITY+1> source={};
    for(unsigned i=0;i<source.size();i++)source[i]=emitter((int(i%8)-4)*5,(int(i/8)-2)*7,i+1);
    WorldTorchLights out;
    assert(world_torch_lights_build(source.data(),32,-1310.4,-10207,-1310.4,25.1,-10207,3.125,out));
    assert(out.count<=8&&out.count>0);cluster(out);const auto before=out;
    auto invalid=[&](const VillageTorchEmitter *data,std::size_t count,double ox=0,double oz=0,double cx=0,double cy=25.1,double cz=0,double time=0) {
        out=before;assert(!world_torch_lights_build(data,count,ox,oz,cx,cy,cz,time,out));assert(!std::memcmp(&out,&before,sizeof(out)));
    };
    invalid(source.data(),source.size());invalid(nullptr,1);invalid(source.data(),32,NAN);invalid(source.data(),32,0,INFINITY);
    assert(world_torch_lights_build(source.data(),WORLD_TORCH_FLAME_CAPACITY,0,0,0,25.1,0,0,out));cluster(out);
    invalid(source.data(),32,0,0,NAN);invalid(source.data(),32,0,0,0,INFINITY);invalid(source.data(),32,0,0,0,0,NAN);invalid(source.data(),32,0,0,0,0,0,INFINITY);
    invalid(source.data(),32,100801);invalid(source.data(),32,0,0,0,20001);
    for(unsigned kind=0;kind<13;kind++) {
        auto bad=source;
        auto &e=bad[31];
        switch(kind) {
            case 0:e.x=NAN;break;case 1:e.y=INFINITY;break;case 2:e.z=1001;break;
            case 3:e.height=0;break;case 4:e.width=6;break;case 5:e.intensity=-1;break;
            case 6:e.nx=2;break;case 7:e.plane_d=NAN;break;case 8:e.xmin=e.xmax;break;
            case 9:e.zmax=e.zmin-1;break;case 10:e.ymax=e.ymin;break;
            case 11:e.zmin=-30001;break;case 12:e.intensity=1001;break;
        }
        invalid(bad.data(),32);
    }
    // Changing the village/camera origin together preserves all selection,
    // power and clipping geometry. World fields translate exactly in256m units.
    WorldTorchLights rebased;
    assert(world_torch_lights_build(source.data(),32,-1054.4,-10719,-1054.4,25.1,-10719,3.125,rebased));
    assert(rebased.count==before.count);
    for(unsigned i=0;i<before.count;i++) {
        assert(rebased.position_radius[i][0]-before.position_radius[i][0]==256);
        assert(rebased.position_radius[i][2]-before.position_radius[i][2]==-512);
        assert(rebased.color_intensity[i]==before.color_intensity[i]);
        assert(rebased.bounds[i][0]-before.bounds[i][0]==256&&rebased.bounds[i][3]-before.bounds[i][3]==-512);
        const double qz=-10207+20;
        const double old_plane=before.plane[i][2]*qz+before.plane[i][3];
        const double new_plane=rebased.plane[i][2]*(qz-512)+rebased.plane[i][3];
        assert(std::abs(old_plane-new_plane)<.001);
    }
    auto one=emitter(0,0,812);one.y=25;WorldTorchLights a,b;
    assert(world_torch_lights_build(&one,1,0,0,0,25.1,60,3.125,a)&&a.count==1);
    const float full=one.intensity*world_torch_flicker(3.125,one.seed);
    assert(std::abs(a.color_intensity[0][3]-full)<1.e-5f);
    assert(world_torch_lights_build(&one,1,0,0,0,25.1,70,3.125,b)&&b.count==1);
    assert(std::abs(b.color_intensity[0][3]-full*.5f)<1.e-5f);
    assert(world_torch_lights_build(&one,1,0,0,0,25.1,80,3.125,b)&&b.count==0);
    assert((b.cluster_min==std::array<float,3>{}&&b.cluster_max==std::array<float,3>{}));
    assert(world_torch_lights_build(&one,1,0,0,0,25.1,0,3.25,b));
    assert(std::abs(b.color_intensity[0][3]-one.intensity*world_torch_flicker(3.25,one.seed))<1.e-5f);
    assert(std::abs(b.color_intensity[0][3]-full)>.01f);
    one.seed++;assert(world_torch_lights_build(&one,1,0,0,0,25.1,0,3.25,a));assert(a.color_intensity[0][3]!=b.color_intensity[0][3]);
    one.intensity=0;assert(world_torch_lights_build(&one,1,0,0,0,25.1,0,0,a)&&a.count==0);
    // Disabled sockets remain validated, but neither consume one of the eight
    // candidates nor define the ninth-distance fade for an enabled source.
    std::array<VillageTorchEmitter,9> disabled={};
    for(unsigned i=0;i<8;i++){disabled[i]=emitter(i*.1,0,i+1);disabled[i].intensity=0;}
    disabled[8]=emitter(2,0,900);
    assert(world_torch_lights_build(disabled.data(),9,0,0,0,25.1,0,3.125,a)&&a.count==1);
    assert(a.position_radius[0][0]==2&&std::abs(a.color_intensity[0][3]-20*world_torch_flicker(3.125,900))<1.e-5f);
    disabled[0].width=0;invalid(disabled.data(),9); // even a disabled malformed socket rejects atomically
    for(unsigned i=0;i<8;i++)disabled[i]=emitter(i,0,i+1);
    disabled[8]=emitter(7.05,0,900);disabled[8].intensity=0;
    assert(world_torch_lights_build(disabled.data(),8,0,0,0,25.1,0,3.125,a)&&a.count==8);
    assert(world_torch_lights_build(disabled.data(),9,0,0,0,25.1,0,3.125,b)&&b.count==8);
    assert(!std::memcmp(&a,&b,sizeof(a)));
    for(unsigned i=0;i<8;i++)assert(std::abs(b.color_intensity[i][3]-20*world_torch_flicker(3.125,i+1))<1.e-5f);
    // Seven inner lights are stable while the eighth/ninth sources exchange
    // ranks. Compare by source identity, not sorted array slot; both candidates
    // must reach zero at the exchange and approach it continuously.
    std::array<VillageTorchEmitter,9> crossing={};
    for(unsigned i=0;i<7;i++)crossing[i]=emitter(double(i)-3,0,i+1);
    crossing[7]=emitter(-10,0,8);crossing[8]=emitter(10,0,9);
    std::array<float,9> previous={};float max_rank_delta=0;
    for(int step=-2000;step<=2000;step++) {
        assert(world_torch_lights_build(crossing.data(),crossing.size(),0,0,step*.001,25.1,0,3.125,out));cluster(out);
        const auto powers=by_source(out,crossing);
        if(step>-2000)for(unsigned i=0;i<9;i++)max_rank_delta=std::max(max_rank_delta,std::abs(powers[i]-previous[i]));
        if(std::abs(step)<=1)assert(powers[7]<.00002f&&powers[8]<.00002f);
        if(!step)assert(out.count==7);
        previous=powers;
    }
    assert(max_rank_delta<.02f);
    const auto heap=allocations.load();
    for(unsigned tick=0;tick<36000;tick++) {
        const double time=tick/60.;
        assert(world_torch_lights_build(source.data(),32,-1310.4,-10207,-1310.4+std::sin(time*.3)*18,25.75,-10207+std::cos(time*.4)*14,time,out));
        cluster(out);
    }
    assert(allocations.load()==heap);
    // Source practicals keep the same8-light limit and rank fade, with an
    // explicit actor-only tag and12m reach. Legacy lighting stays unchanged.
    assert(world_torch_lights_build(source.data(),165,0,0,0,25.1,0,0,out,12.f,true));
    assert(out.actor_only&&out.count<=8&&out.count>0);
    for(unsigned i=0;i<out.count;i++)for(unsigned axis=0;axis<3;axis++){
        assert(out.position_radius[i][3]==12.f);
        assert(out.cluster_min[axis]<=out.position_radius[i][axis]-12.f);
        assert(out.cluster_max[axis]>=out.position_radius[i][axis]+12.f);
    }
    const auto actor_lights=out;
    assert(!world_torch_lights_build(source.data(),165,0,0,0,25.1,0,0,out,NAN,true));
    assert(!std::memcmp(&out,&actor_lights,sizeof(out)));
    assert(world_torch_lights_build(source.data(),165,0,0,0,25.1,0,0,out)&&!out.actor_only);
    assert(world_torch_lights_build(nullptr,0,0,0,0,0,0,0,out)&&out.count==0);
    assert((out.cluster_min==std::array<float,3>{}&&out.cluster_max==std::array<float,3>{}));
    std::printf("PASS torch lights CPU: atomic32-input/8-light bounds, invalid guards, disabled-source starvation/fade exclusion, rebased planes/boxes, conservative clusters, shared time/seed flicker,60–80m fade,4001 rank-crossing samples maxdelta%.8f,600s zero allocations\n",max_rank_delta);
}

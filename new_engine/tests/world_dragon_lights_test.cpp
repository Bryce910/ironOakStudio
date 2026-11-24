#include "engine/world_dragon_lights.h"
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
WorldTorchLights torches(unsigned count,double ox=0,double oz=0) {
    std::array<VillageTorchEmitter,8> emitters={};
    for(unsigned i=0;i<count;i++)emitters[i]={double(i*3),25,double(i),.4f,.13f,20.f,i+1,0,0,1,100,-100,-100,100,100,23,32};
    WorldTorchLights out;assert(world_torch_lights_build(emitters.data(),count,ox,oz,ox,25,oz,3.125,out));assert(out.count==count);return out;
}
WorldDragonBreathLight source(double x,double y,double z,float intensity=6,float radius=14) {
    WorldDragonBreathLight b;b.position[0]=x;b.position[1]=y;b.position[2]=z;b.color[0]=1;b.color[1]=.29f;b.color[2]=.04f;b.strength=intensity;b.radius=radius;return b;
}
bool same(const WorldTorchLights &a,const WorldTorchLights &b){return !std::memcmp(&a,&b,sizeof(a));}
void cluster(const WorldTorchLights &lights) {
    for(unsigned axis=0;axis<3;axis++) {
        double lo=1.e100,hi=-1.e100;
        for(unsigned i=0;i<lights.count;i++){
            const auto &p=lights.position_radius[i];lo=std::min(lo,double(p[axis])-p[3]);hi=std::max(hi,double(p[axis])+p[3]);
        }
        assert(lights.cluster_min[axis]<=lo&&lights.cluster_max[axis]>=hi);
        assert(std::abs(lights.cluster_min[axis]-lo)<.04&&std::abs(lights.cluster_max[axis]-hi)<.04);
    }
}
}
int main() {
    auto base=torches(8);auto output=base;
    assert(world_dragon_lights_merge(output,nullptr,0,0,25,0)&&same(output,base));
    auto breath=source(4,27,3);breath.strength=0;
    assert(world_dragon_lights_merge(output,&breath,1,0,25,0)&&same(output,base));
    breath.strength=.001f;assert(world_dragon_lights_merge(output,&breath,1,0,25,0)&&same(output,base));
    breath=source(4,27,3);breath.color[0]=breath.color[1]=breath.color[2]=0;
    assert(world_dragon_lights_merge(output,&breath,1,0,25,0)&&same(output,base));
    breath=source(121,25,0,1000);assert(world_dragon_lights_merge(output,&breath,1,0,25,0)&&same(output,base));
    // A moderate nearby breath must not displace much brighter nearby lamps.
    breath=source(1,25,0,1);assert(world_dragon_lights_merge(output,&breath,1,0,25,0)&&same(output,base));
    // Free slots append while preserving every byte of every retained light.
    base=torches(6);output=base;std::array<WorldDragonBreathLight,2> pair={source(-4,28,2,8),source(5,30,-4,6)};
    assert(world_dragon_lights_merge(output,pair.data(),2,0,25,0)&&output.count==8);cluster(output);
    for(unsigned i=0;i<6;i++)assert(output.position_radius[i]==base.position_radius[i]&&output.color_intensity[i]==base.color_intensity[i]&&
        output.plane[i]==base.plane[i]&&output.bounds[i]==base.bounds[i]&&output.heights[i]==base.heights[i]);
    for(unsigned i=6;i<8;i++){
        assert((output.plane[i]==std::array<float,4>{{0,0,0,1}}));const auto &p=output.position_radius[i];
        assert(output.bounds[i][0]<=double(p[0])-p[3]&&output.bounds[i][2]>=double(p[0])+p[3]);
        assert(output.bounds[i][1]<=double(p[2])-p[3]&&output.bounds[i][3]>=double(p[2])+p[3]);
        assert(output.heights[i][0]<=double(p[1])-p[3]&&output.heights[i][1]>=double(p[1])+p[3]);
    }
    // Capacity stays8. Far/weak sources lose to a high-influence nearby jet;
    // the7 retained source sockets keep all of their original clip geometry.
    base=torches(8);base.color_intensity[3][3]=.01f;output=base;breath=source(4,26,2,8);
    assert(world_dragon_lights_merge(output,&breath,1,0,25,0)&&output.count==8);cluster(output);
    assert(output.position_radius[3][0]==4&&output.position_radius[3][3]==14);
    for(unsigned i=0;i<8;i++)if(i!=3)assert(output.position_radius[i]==base.position_radius[i]&&output.color_intensity[i]==base.color_intensity[i]&&output.plane[i]==base.plane[i]&&output.bounds[i]==base.bounds[i]&&output.heights[i]==base.heights[i]);
    // Ranking depends on radius AND radiance, not only nearest distance.
    base=torches(8);base.color_intensity[7][3]=.01f;base.color_intensity[6][3]=.02f;
    pair={source(0,25,0,2,2),source(10,25,0,15,18)};output=base;
    assert(world_dragon_lights_merge(output,pair.data(),2,0,25,0)&&output.count==8);
    assert(output.position_radius[7][0]==10&&output.position_radius[6][0]==0);cluster(output);
    // Translating sources AND camera by100km preserves selection/intensities.
    const auto origin_result=output;auto translated=base;
    constexpr double ox=98304,oy=512,oz=-98304;
    for(unsigned i=0;i<translated.count;i++){
        translated.position_radius[i][0]+=ox;translated.position_radius[i][1]+=oy;translated.position_radius[i][2]+=oz;
        translated.plane[i][3]-=translated.plane[i][0]*ox+translated.plane[i][1]*oy+translated.plane[i][2]*oz;
        translated.bounds[i][0]+=ox;translated.bounds[i][2]+=ox;translated.bounds[i][1]+=oz;translated.bounds[i][3]+=oz;
        translated.heights[i][0]+=oy;translated.heights[i][1]+=oy;
    }
    for(unsigned k=0;k<3;k++){const float delta=k==0?ox:k==1?oy:oz;translated.cluster_min[k]+=delta;translated.cluster_max[k]+=delta;}
    for(auto &p:pair){p.position[0]+=ox;p.position[1]+=oy;p.position[2]+=oz;}
    assert(world_dragon_lights_merge(translated,pair.data(),2,ox,25+oy,oz));cluster(translated);
    assert(translated.count==origin_result.count);
    for(unsigned i=0;i<8;i++){
        assert(translated.color_intensity[i]==origin_result.color_intensity[i]);
        assert(std::abs(double(translated.position_radius[i][0])-origin_result.position_radius[i][0]-ox)<=.008&&
            std::abs(double(translated.position_radius[i][1])-origin_result.position_radius[i][1]-oy)<=.0001&&
            std::abs(double(translated.position_radius[i][2])-origin_result.position_radius[i][2]-oz)<=.008);
    }
    // Every malformed field, including a late second source, rejects atomically.
    base=torches(8);pair={source(0,25,0),source(1,25,1)};
    auto reject=[&](const WorldDragonBreathLight *p,std::size_t count,double x=0,double y=25,double z=0){output=base;assert(!world_dragon_lights_merge(output,p,count,x,y,z)&&same(output,base));};
    reject(nullptr,1);reject(pair.data(),3);reject(pair.data(),2,NAN);reject(pair.data(),2,0,INFINITY);
    for(unsigned kind=0;kind<10;kind++) {
        auto bad=pair;auto &b=bad[1];
        switch(kind){case 0:b.position[0]=NAN;break;case 1:b.position[1]=1.e9;break;case 2:b.position[2]=INFINITY;break;case 3:b.color[0]=-1;break;case 4:b.color[1]=NAN;break;case 5:b.color[2]=65;break;case 6:b.strength=-1;break;case 7:b.strength=INFINITY;break;case 8:b.radius=0;break;case 9:b.radius=129;break;}
        reject(bad.data(),2);
    }
    for(unsigned kind=0;kind<8;kind++) {
        output=base;
        switch(kind){case 0:output.count=9;break;case 1:output.position_radius[7][1]=NAN;break;case 2:output.color_intensity[7][3]=-1;break;case 3:output.plane[7][0]=2;break;case 4:output.bounds[7][0]=output.bounds[7][2];break;case 5:output.heights[7][0]=output.heights[7][1];break;case 6:output.cluster_min[0]=INFINITY;break;case 7:output.position_radius[7][3]=0;break;}
        const auto bad=output;assert(!world_dragon_lights_merge(output,pair.data(),2,0,25,0)&&same(output,bad));
    }
    // Distance fading is continuous with free capacity, and >=120m is inert.
    WorldTorchLights empty;float previous=100;unsigned fade_samples=0;
    for(int tick=0;tick<=2000;tick++){
        breath=source(100+tick*.01,25,0,100,18);output=empty;
        assert(world_dragon_lights_merge(output,&breath,1,0,25,0));
        const float power=output.count?output.color_intensity[0][3]:0;
        assert(power<=previous+.0001f);if(tick)assert(previous-power<.1f);previous=power;fade_samples++;
    }
    assert(same(output,empty));
    const auto heap=allocations.load();
    for(unsigned tick=0;tick<36000;tick++){
        output=base;breath=source(std::sin(tick/120.)*25,25,std::cos(tick/120.)*25,float(tick%600)/60,14);
        assert(world_dragon_lights_merge(output,&breath,1,0,25,0)&&output.count<=8);
        if(!same(output,base))cluster(output); // Inactive paths preserve even original float-rounding bytes.
    }
    assert(allocations.load()==heap);
    std::printf("PASS dragon lights: byte-identical inactive/tiny/far/overpowered, fixed8 cap, conservative influence ranking/threshold, retained torch geometry, outward clusters/point boxes,100km origin precision, malformed atomic retention,%u distance-fade samples,600s zero allocations\n",fade_samples);
}

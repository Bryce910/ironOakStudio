// Real staging and validation with no GL context or native GPU work.
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <new>
static std::atomic<std::size_t> allocations{0};
[[gnu::noinline]] void *operator new(std::size_t n){allocations++;if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](std::size_t n){return ::operator new(n);}
[[gnu::noinline]] void *operator new(std::size_t n,const std::nothrow_t &)noexcept{allocations++;return std::malloc(n?n:1);}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,std::size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p,std::size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,const std::nothrow_t &)noexcept{std::free(p);}
#include "engine/world_dragon_breath.cpp"
int main() {
    WorldDragonBreath effect;
    std::array<WorldDragonBreathEmitter,2> emitters={};
    emitters[0].position[0]=-3;emitters[0].position[1]=2;emitters[0].position[2]=-5;
    emitters[0].strength=.8f;emitters[0].seed=123;
    emitters[1]=emitters[0];emitters[1].position[0]=3;emitters[1].seed=99;
    emitters[1].variant=WorldDragonBreathVariant::Frost;
    assert(world_dragon_breath_set_emitters(&effect,emitters.data(),2));
    const auto original_emitters=effect.emitters;
    assert(!world_dragon_breath_set_emitters(&effect,emitters.data(),3));
    assert(!world_dragon_breath_set_emitters(&effect,nullptr,1));
    auto invalid=emitters;
    invalid[1].strength=1.00001f;assert(!world_dragon_breath_set_emitters(&effect,invalid.data(),2));
    invalid=emitters;invalid[1].direction[0]=1;assert(!world_dragon_breath_set_emitters(&effect,invalid.data(),2));
    invalid=emitters;invalid[1].position[2]=NAN;assert(!world_dragon_breath_set_emitters(&effect,invalid.data(),2));
    invalid=emitters;invalid[0].length=36.1f;assert(!world_dragon_breath_set_emitters(&effect,invalid.data(),2));
    invalid=emitters;invalid[0].radius=.19f;assert(!world_dragon_breath_set_emitters(&effect,invalid.data(),2));
    invalid=emitters;invalid[0].variant=static_cast<WorldDragonBreathVariant>(2);assert(!world_dragon_breath_set_emitters(&effect,invalid.data(),2));
    assert(!std::memcmp(original_emitters.data(),effect.emitters.data(),sizeof(original_emitters)));
    WorldDragonBreathFrame frame;frame.color=1;frame.depth=2;frame.width=1600;frame.height=900;
    for(unsigned i=0;i<4;i++)frame.vp[i*5]=1;
    assert(frame_valid(frame));
    auto bad=frame;bad.color=bad.depth;assert(!frame_valid(bad));
    bad=frame;bad.near_plane=0;assert(!frame_valid(bad));
    bad=frame;bad.far_plane=.3f;assert(!frame_valid(bad));
    bad=frame;bad.time=INFINITY;assert(!frame_valid(bad));
    bad=frame;bad.width=0;assert(!frame_valid(bad));
    bad=frame;bad.right[2]=1;assert(!frame_valid(bad));
    bad=frame;bad.vp[9]=NAN;assert(!frame_valid(bad));
    bad=frame;bad.wind_xz[0]=NAN;assert(!frame_valid(bad));
    bad=frame;bad.fog[0]=-1;assert(!frame_valid(bad));
    frame.time=3.125;const auto staged=prepare(effect.emitters,effect.count,frame,effect.staging);
    // Preparation cannot issue any GL work from an ordinary ready frame.
    assert(!world_dragon_breath_prepare(&effect,frame,false)&&effect.stats.warmup_failures==1);
    assert(!world_dragon_breath_prepare(&effect,bad,true)&&effect.stats.warmup_failures==2);
    assert(staged.visible==2&&staged.smoke==56&&staged.fire==168);
    std::array<float,flame_count> ordered={};
    for(unsigned i=0;i<flame_count;i++)ordered[i]=effect.staging[staged.smoke+i][8];
    std::sort(ordered.begin(),ordered.end());
    for(unsigned i=1;i<flame_count;i++)assert(std::abs(ordered[i]-ordered[i-1]-1.f/flame_count)<.000001f);
    assert(std::abs(ordered.front()+1.f-ordered.back()-1.f/flame_count)<.000001f);
    // Twelve overlap intervals now span the complete18m jet rather than only
    // the first38%, keeping the hot core connected without more billboards.
    for(unsigned i=0;i<core_count;i++) {
        const auto &core=effect.staging[staged.smoke+flame_count+i];
        assert(core[11]==1&&std::abs(core[8]-(i+.5f)/core_count)<.000001f);
        assert(core[7]*2>emitters[0].length/core_count);
    }
    for(unsigned i=0;i<224;i++)for(float v:effect.staging[i])assert(std::isfinite(v));
    for(unsigned i=1;i<staged.smoke;i++)assert(-effect.staging[i-1][2]>=-effect.staging[i][2]);
    const auto original=effect.staging;
    frame.camera[0]+=100000;frame.camera[1]-=5000;frame.camera[2]+=80000;
    for(auto &e:effect.emitters){e.position[0]+=100000;e.position[1]-=5000;e.position[2]+=80000;}
    assert(prepare(effect.emitters,2,frame,effect.staging).visible==2);
    assert(!std::memcmp(original.data(),effect.staging.data(),sizeof(original)));
    frame.time+=64;prepare(effect.emitters,2,frame,effect.staging);
    assert(!std::memcmp(original.data(),effect.staging.data(),sizeof(original)));
    frame.camera[0]+=10000;assert(prepare(effect.emitters,2,frame,effect.staging).visible==0);frame.camera[0]-=10000;
    WorldDragonBreathLight lights[3];
    assert(world_dragon_breath_lights(&effect,frame.time,lights,3)==2);
    assert(lights[0].color[0]>lights[0].color[2]&&lights[1].color[2]>lights[1].color[0]);
    assert(lights[0].strength>0&&lights[0].strength<2.5&&lights[0].radius<=18);
    assert(world_dragon_breath_lights(&effect,NAN,lights,2)==0);
    assert(world_dragon_breath_lights(&effect,frame.time,nullptr,0)==0);
    // 600s of both visual emitters: fixed224 instances, no allocations,
    // coherent animation, finite staging and bounded visual light strength.
    const auto allocation_count=allocations.load();std::size_t checked=0;
    for(unsigned tick=0;tick<36000;tick++) {
        frame.time=tick/60.;
        const auto p=prepare(effect.emitters,2,frame,effect.staging);assert(p.visible==2&&p.fire+p.smoke==224);
        for(const auto &i:effect.staging){for(float v:i)assert(std::isfinite(v));assert(i[9]>=0&&i[9]<=.45f);checked++;}
        assert(world_dragon_breath_lights(&effect,frame.time,lights,2)==2);
    }
    assert(allocations.load()==allocation_count);
    assert(world_dragon_breath_set_emitters(&effect,nullptr,0));
    assert(world_dragon_breath_draw(&effect,frame)&&effect.stats.draws==0);
    assert(!world_dragon_breath_draw(&effect,bad)&&effect.stats.failures==1);
    assert(!world_dragon_breath_create(nullptr));
    std::printf("PASS dragon breath CPU: strict2/224 cap, atomic validation, finite bounded smoke/fire/core/embers, depth-sorted smoke, exact world rebasing/64s repeat, offscreen/empty noGL,2 bounded colored lights,%zu particles over600s without allocations\n",checked);
}

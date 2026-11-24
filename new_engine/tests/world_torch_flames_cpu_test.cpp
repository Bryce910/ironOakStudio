// Exercise actual fixed staging and validation without creating a GL context.
#include <atomic>
#include <cstdlib>
#include <new>
#include <cassert>
#include <cstring>
#include <limits>
static std::atomic<std::size_t> allocations{0};
[[gnu::noinline]] void *operator new(std::size_t n){allocations++;if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](std::size_t n){return ::operator new(n);}
[[gnu::noinline]] void *operator new(std::size_t n,const std::nothrow_t &)noexcept{allocations++;return std::malloc(n?n:1);}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,std::size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p,std::size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,const std::nothrow_t &)noexcept{std::free(p);}
#include "engine/world_torch_flames.cpp"
int main() {
    // Startup files are optional, bounded and atomic; all165 source holders
    // fit while leaving the Studio's16 overlays inside fixed renderer storage.
    std::vector<WorldTorchFlameEmitter> source(1);source[0].seed=123;
    std::string source_error;const char *source_path="/tmp/valwood-source-flames-cpu.fixture";
    std::remove(source_path);
    assert(world_torch_flames_read_file(source_path,source,source_error)==WorldTorchFlameFileStatus::Missing&&source.size()==1&&source[0].seed==123);
    {
        std::ofstream out(source_path);out<<"VALWOOD_SOURCE_FLAMES 1\n";
        for(unsigned i=0;i<165;i++)out<<"-43600 1284 -24100 .45 .18 .65 "<<i<<'\n';
    }
    assert(world_torch_flames_read_file(source_path,source,source_error)==WorldTorchFlameFileStatus::Loaded&&source.size()==165&&source[164].seed==164);
    const auto source_before=source;
    for(const char *bad:{"VALWOOD_SOURCE_FLAMES 2\n","VALWOOD_SOURCE_FLAMES 1\n0 0 0 nan .18 1 1\n","VALWOOD_SOURCE_FLAMES 1\n0 0 0 .4 .18 1 4294967296\n","VALWOOD_SOURCE_FLAMES 1\n999999 0 0 .4 .18 1 1\n","VALWOOD_SOURCE_FLAMES 1\n0 0 0 .4 .18 1 1 garbage\n"}){
        std::ofstream(source_path)<<bad;
        assert(world_torch_flames_read_file(source_path,source,source_error)==WorldTorchFlameFileStatus::Invalid&&source.size()==source_before.size()&&!std::memcmp(source.data(),source_before.data(),source.size()*sizeof(source[0])));
    }
    {
        std::ofstream out(source_path);out<<"VALWOOD_SOURCE_FLAMES 1\n";
        for(unsigned i=0;i<WORLD_TORCH_FLAME_CAPACITY-15;i++)out<<"0 0 0 .45 .18 .65 "<<i<<'\n';
    }
    assert(world_torch_flames_read_file(source_path,source,source_error)==WorldTorchFlameFileStatus::Invalid&&source.size()==165);
    std::remove(source_path);
    WorldTorchFlames flames;
    std::array<WorldTorchFlameEmitter,WORLD_TORCH_FLAME_CAPACITY+1> emitters={};
    for(unsigned i=0;i<emitters.size();i++) {
        auto &e=emitters[i];e.x=(int(i%8)-4)*.65;e.y=24.;e.z=-8.-int(i/8);
        e.height=.65f;e.width=.17f;e.seed=i*0x812385abu;
    }
    assert(world_torch_flames_set_emitters(&flames,emitters.data(),32));
    const auto before=flames.emitters;
    assert(!world_torch_flames_set_emitters(&flames,emitters.data(),WORLD_TORCH_FLAME_CAPACITY+1));
    assert(!world_torch_flames_set_emitters(&flames,nullptr,1));
    emitters[9].width=NAN;assert(!world_torch_flames_set_emitters(&flames,emitters.data(),32));emitters[9].width=.17f;
    emitters[31].x=INFINITY;assert(!world_torch_flames_set_emitters(&flames,emitters.data(),32));emitters[31].x=emitters[30].x+.65;
    emitters[0].height=.049f;assert(!world_torch_flames_set_emitters(&flames,emitters.data(),32));emitters[0].height=.65f;
    emitters[0].intensity=-1;assert(!world_torch_flames_set_emitters(&flames,emitters.data(),32));emitters[0].intensity=1;
    assert(flames.count==32&&!std::memcmp(before.data(),flames.emitters.data(),sizeof(before)));
    WorldTorchFlameFrame frame;frame.framebuffer=1;frame.width=1600;frame.height=900;frame.camera[1]=25.75;
    for(int i=0;i<4;i++)frame.vp[i*5]=1;
    assert(frame_valid(frame));
    auto invalid=frame;invalid.width=0;assert(!frame_valid(invalid));
    invalid=frame;invalid.up[0]=1;assert(!frame_valid(invalid));
    invalid=frame;invalid.vp[6]=NAN;assert(!frame_valid(invalid));
    invalid=frame;invalid.time=INFINITY;assert(!frame_valid(invalid));
    invalid=frame;invalid.camera[2]=NAN;assert(!frame_valid(invalid));
    invalid=frame;invalid.tan_half_fov=0;assert(!frame_valid(invalid));
    invalid=frame;invalid.wind_xz[0]=NAN;assert(!frame_valid(invalid));
    invalid=frame;invalid.wind_xz[1]=30.01f;assert(!frame_valid(invalid));
    frame.wind_xz[0]=.16f;frame.wind_xz[1]=.05f;
    assert(prepare(flames.emitters,32,frame,flames.staging)==32);
    const auto original=flames.staging;
    frame.camera[0]+=100000;frame.camera[1]-=10000;frame.camera[2]+=73000;
    for(auto &e:flames.emitters){e.x+=100000;e.y-=10000;e.z+=73000;}
    assert(prepare(flames.emitters,32,frame,flames.staging)==32);
    assert(!std::memcmp(original.data(),flames.staging.data(),sizeof(original)));
    // Clipping uses absolute bowl/spark bounds, never a camera-relative height.
    frame.clip_below=true;frame.clip_height=-9900;
    assert(prepare(flames.emitters,32,frame,flames.staging)==0);
    frame.clip_below=false;
    frame.camera[2]+=1000;assert(prepare(flames.emitters,32,frame,flames.staging)==0);frame.camera[2]-=1000;
    // Bound every permitted physical geometry extreme, including3x filtered
    // ember widths, velocity elongation and the clamped30m/s wind input.
    std::size_t envelope_samples=0;
    for(float height:{.05f,.65f,4.f})for(float width:{.02f,.17f,2.f})
    for(float wx:{0.f,.16f,-3.f,30.f})for(float wz:{0.f,.05f,3.f,-30.f}) {
        WorldTorchFlameEmitter e;e.height=height;e.width=width;
        auto f=frame;f.wind_xz[0]=wx;f.wind_xz[1]=wz;
        const auto b=effect_bounds(e,f);
        const double wind=std::min(3.,std::hypot(double(wx),double(wz)));
        const double physical=std::min({double(width)*.14,.022,double(height)*.075});
        const double ember_radius=(.85+std::sqrt(2.)*.30)*width+wind*height*.5+
            physical*std::hypot(3.,3.4);
        const double flame_radius=std::hypot(1.,.68)*width+wind*height*.24;
        assert(ember_radius<b.horizontal&&flame_radius<b.horizontal);
        assert(2.79*height+physical*3.4<b.max_y);
        assert(1.35*height<b.max_y&&b.center_y==b.max_y*.5);
        envelope_samples++;
    }
    // A wind-bent tip can remain on screen while its fuel mouth is outside.
    // The enlarged sphere must retain it, and still reject a disjoint sphere.
    std::array<WorldTorchFlameEmitter,WORLD_TORCH_FLAME_CAPACITY> boundary={};
    boundary[0].height=4;boundary[0].width=.17f;boundary[0].z=-10;
    auto boundary_frame=frame;
    for(double &c:boundary_frame.camera)c=0;
    boundary_frame.clip_below=false;boundary_frame.wind_xz[0]=3;boundary_frame.wind_xz[1]=0;
    const auto bound=effect_bounds(boundary[0],boundary_frame);
    const double th=boundary_frame.tan_half_fov*boundary_frame.width/boundary_frame.height;
    boundary[0].x=10*th+bound.radius*std::hypot(1.,th)*.99;
    assert(prepare(boundary,1,boundary_frame,flames.staging)==1);
    boundary[0].x=10*th+bound.radius*std::hypot(1.,th)*1.01;
    assert(prepare(boundary,1,boundary_frame,flames.staging)==0);
    boundary[0].x=0;boundary_frame.clip_below=true;
    boundary_frame.clip_height=float(bound.max_y-.001);
    assert(prepare(boundary,1,boundary_frame,flames.staging)==1);
    boundary_frame.clip_height=float(bound.max_y+.001);
    assert(prepare(boundary,1,boundary_frame,flames.staging)==0);
    float minimum=2,maximum=0,max_delta=0;std::size_t samples=0;
    for(std::uint32_t seed:{0u,1u,0xffffffffu,0x1234abcdu})for(int tick=-7680;tick<=7680;tick++) {
        const double time=tick/120.;const float f=world_torch_flicker(time,seed);
        assert(std::isfinite(f)&&f>=.819999f&&f<=1.180001f);
        assert(std::abs(f-world_torch_flicker(time+64,seed))<1.e-6f);
        minimum=std::min(minimum,f);maximum=std::max(maximum,f);
        max_delta=std::max(max_delta,std::abs(f-world_torch_flicker(time+1./60.,seed)));samples++;
    }
    assert(maximum-minimum>.20f&&max_delta<.04f);
    assert(world_torch_flicker(NAN,17)==1);
    // Render-frame staging and configuration replacement remain allocation-free.
    const auto count=allocations.load();
    for(unsigned tick=0;tick<36000;tick++) {
        frame.time=tick/60.;assert(prepare(flames.emitters,32,frame,flames.staging)==32);
        if(tick%60==0)assert(world_torch_flames_set_emitters(&flames,flames.emitters.data(),32));
    }
    assert(allocations.load()==count);
    assert(world_torch_flames_set_emitters(&flames,nullptr,0));
    assert(world_torch_flames_draw(&flames,frame)&&world_torch_flames_stats(&flames).draws==0);
    assert(!world_torch_flames_draw(nullptr,frame));
    assert(world_torch_flames_set_emitters(&flames,emitters.data(),WORLD_TORCH_FLAME_CAPACITY));
    const auto full=flames.emitters;
    assert(!world_torch_flames_set_emitters(&flames,emitters.data(),WORLD_TORCH_FLAME_CAPACITY+1));
    assert(flames.count==WORLD_TORCH_FLAME_CAPACITY&&!std::memcmp(full.data(),flames.emitters.data(),sizeof(full)));
    std::printf("PASS torch CPU: bounded256 cap,165-source startup file/16overlay reserve/atomic invalid input, all frame/wind guards, exact origin rebasing, absolute clip/distance/frustum culling, %zu geometry envelopes, %zu bounded flicker samples range%.6f..%.6f max60Hzdelta%.6f,600s/32emitters zero allocations\n",envelope_samples,samples,minimum,maximum,max_delta);
}

#include "engine/world_quality_controller.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>

namespace {bool track=false;std::size_t allocations=0;}
void *operator new(std::size_t n){if(track)allocations++;if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void *p) noexcept{std::free(p);}
void operator delete(void *p,std::size_t) noexcept{std::free(p);}

namespace {
constexpr double dt=1./60.;
bool level(float scale){for(float v:{1.f,.95f,.90f,.85f,.80f,.75f,.70f})if(v==scale)return true;return false;}
struct Fixture {
    QualityController q;
    std::uint64_t serial=0,epoch=1;
    double time=0;
    std::array<double,256> changes={};std::size_t count=0;
    explicit Fixture(QualityController::Settings settings=QualityController::Settings{}){q.reset(settings,epoch);}
    bool frame(double gpu,double cpu=6,double elapsed=dt,bool eligible=true,bool resize=true){
        time+=elapsed;const bool changed=q.submit({++serial,epoch,gpu,cpu,eligible,elapsed});
        assert((level(q.scale())||q.scale()==q.status().min_scale||q.scale()==q.status().max_scale)&&q.scale()>=q.status().min_scale&&q.scale()<=q.status().max_scale);
        if(changed){
            assert(count<changes.size());changes[count++]=time;
            if(resize){assert(q.reconfigure(++epoch));serial=0;}
        }
        return changed;
    }
    void frames(unsigned n,double gpu,double cpu=6){for(unsigned i=0;i<n;i++)frame(gpu,cpu);}
    void cooldowns() const {for(std::size_t i=1;i<count;i++)assert(changes[i]-changes[i-1]>=1.-.000001);}
};
void settings_and_manual(){
    Fixture q;assert(q.q.scale()==1&&q.q.status().gpu_budget_ms>15&&q.q.status().gpu_budget_ms<15.2);
    QualityController::Settings s;s.scale=.86f;s.min_scale=.83f;s.target_fps=60;s.enabled=false;
    q.q.reset(s,2);assert(q.q.scale()==.86f&&q.q.status().min_scale==.83f&&!q.q.status().enabled);
    for(unsigned i=0;i<1000;i++)assert(!q.q.submit({i,2,100,80,true,dt}));
    assert(q.q.scale()==.86f&&!q.q.status().resizes&&!q.q.status().ready);
    s.enabled=true;s.scale=NAN;s.min_scale=NAN;s.target_fps=NAN;q.q.reset(s,3);
    assert(q.q.scale()==1&&q.q.status().min_scale==.7f&&q.q.status().target_fps==60);
    s.scale=.5f;s.min_scale=2;s.target_fps=0;q.q.reset(s,4);
    assert(q.q.scale()==.7f&&q.q.status().min_scale==.7f&&q.q.status().max_scale==.7f&&q.q.status().target_fps==45);
}
void severe_overload_and_floor(){
    Fixture q;
    double below_floor_after=-1;
    for(unsigned i=0;i<180;i++){
        const double gpu=2.+34.*q.q.scale()*q.q.scale();
        q.frame(gpu,6,std::max(dt,(gpu+1.5)/1000.));
        if(gpu<=1000./45.&&below_floor_after<0)below_floor_after=q.time;
    }
    assert(q.count&&q.changes[0]<.20&&below_floor_after>0&&below_floor_after<.25);
    assert(q.q.scale()==.7f&&q.q.status().floor_limited&&!q.q.status().cpu_limited);q.cooldowns();
    Fixture turn;turn.frames(120,9);const double started=turn.time;
    while(!turn.count&&turn.time-started<.5)turn.frame(40);
    assert(turn.count==1&&turn.changes[0]-started<.15); // A new expensive view cannot wait for the slow EMA.
    Fixture hard;hard.frames(300,80,6);assert(hard.q.scale()==.7f&&hard.q.status().floor_limited);
    // The indicator admits a fixed/geometry bottleneck that resolution cannot
    // cure; the controller never promises that reaching its floor gives45FPS.
    assert(hard.q.status().filtered_gpu_ms>22.222);
}
void ordinary_pressure_and_spikes(){
    Fixture normal;
    for(unsigned i=0;i<25;i++)assert(!normal.frame(17.2,6,1./50.));
    assert(normal.q.scale()==1); // Less than half a second is not sustained.
    for(unsigned i=0;i<20&&!normal.count;i++)normal.frame(17.2,6,1./50.);
    assert(normal.count==1&&normal.q.scale()==.95f&&normal.changes[0]>=.5&&normal.changes[0]<.65);
    normal.frames(300,17.2);normal.cooldowns();
    Fixture spikes;spikes.frames(60,13.5);
    for(unsigned i=0;i<2000;i++)spikes.frame(i%31==0?100.:13.5);
    assert(spikes.q.scale()==1&&!spikes.count); // Isolated stalls lose the median vote.
}
void recovery_and_oscillation(){
    QualityController::Settings s;
    Fixture recover(s);recover.frames(120,40);
    assert(recover.q.reconfigure(++recover.epoch));recover.serial=0;
    recover.time=0;recover.count=0;recover.changes={};recover.frames(230,8);
    assert(recover.q.scale()==.7f&&!recover.count);
    recover.frames(20,8);assert(recover.q.scale()==.75f&&recover.count==1&&recover.changes[0]>=4.);
    recover.frames(240,8);assert(recover.q.scale()==.8f&&recover.count==2);recover.cooldowns();
    s.scale=.7f;Fixture alternating(s);
    for(unsigned i=0;i<3600;i++)alternating.frame((i/15)%2?18.:10.);
    assert(alternating.q.scale()==.7f&&!alternating.count); // No four-second clear recovery window.
    Fixture jitter;
    for(unsigned i=0;i<3600;i++)jitter.frame((i%2)?15.5:14.5);
    assert(jitter.count<=1);jitter.cooldowns();
    s.scale=.85f;Fixture ceiling(s);ceiling.frames(120,40);assert(ceiling.q.scale()==.7f);
    ceiling.frames(3000,8);assert(ceiling.q.scale()==.85f&&ceiling.q.status().max_scale==.85f);ceiling.cooldowns();
    for(const auto interval:std::array<std::array<float,2>,2>{{{.84f,.83f},{.73f,.72f}}}){
        s.scale=interval[0];s.min_scale=interval[1];Fixture narrow(s);
        assert(narrow.q.scale()==interval[0]&&narrow.q.status().min_scale==interval[1]);
        narrow.frames(1200,40);assert(narrow.q.scale()==interval[1]&&narrow.q.status().floor_limited);
        narrow.frames(3000,8);assert(narrow.q.scale()==interval[0]&&narrow.q.status().max_scale==interval[0]);
        for(unsigned i=0;i<5000;i++)narrow.frame((i/1200)%2?8.:40.);
        narrow.cooldowns();
    }
}
void cpu_bound_and_present_independence(){
    Fixture cpu;for(unsigned i=0;i<1200;i++)cpu.frame(9,35,.035);
    assert(cpu.q.scale()==1&&!cpu.count&&cpu.q.status().cpu_limited&&!cpu.q.status().floor_limited);
    // wall_dt includes elapsed pacing; only measured work drives pressure.
    Fixture vsync;for(unsigned i=0;i<900;i++)vsync.frame(8,5,1./30.);
    assert(vsync.q.scale()==1&&!vsync.count&&!vsync.q.status().cpu_limited);
    Fixture cpu_and_gpu;
    for(unsigned i=0;i<600;i++)cpu_and_gpu.frame(18,30,.030);
    assert(!cpu_and_gpu.count&&cpu_and_gpu.q.scale()==1&&cpu_and_gpu.q.status().cpu_limited);
    Fixture gpu_miss;
    for(unsigned i=0;i<180;i++)gpu_miss.frame(18,8,.020);
    assert(gpu_miss.count&&gpu_miss.q.scale()<1);
}
void native_vsync_regression(){
    // Timestamp spans can sit near VSync even in a cheap scene. Holding the
    // target cadence is positive evidence against an ordinary quality drop.
    Fixture held;
    for(unsigned i=0;i<7200;i++)held.frame(16.2,6,.01668);
    assert(held.q.scale()==1&&!held.count&&!held.q.status().floor_limited);
    assert(std::abs(held.q.status().filtered_frame_ms-16.68)<.00001);
    Fixture missed;
    for(unsigned i=0;i<1200;i++)missed.frame(16.2,6,.0185);
    assert(missed.count&&missed.q.scale()<1&&!missed.q.status().cpu_limited);missed.cooldowns();
    Fixture cpu;
    for(unsigned i=0;i<1200;i++)cpu.frame(16.2,25,.025);
    assert(!cpu.count&&cpu.q.scale()==1&&cpu.q.status().cpu_limited);
    Fixture spikes;
    for(unsigned i=0;i<7200;i++)spikes.frame(16.2,6,i%37==0?.033:.01668);
    assert(!spikes.count&&spikes.q.scale()==1);
    Fixture recovery;recovery.frames(120,40);assert(recovery.q.scale()==.7f);
    // Low GPU cost alone cannot justify recovery during missed cadence.
    for(unsigned i=0;i<900;i++)recovery.frame(8,6,1./30.);
    assert(recovery.q.scale()==.7f);
    recovery.frames(300,8);assert(recovery.q.scale()>.7f);recovery.cooldowns();
    Fixture ambiguous;ambiguous.frames(120,40);
    for(unsigned i=0;i<7200;i++)ambiguous.frame(16.2,6,.01668);
    assert(ambiguous.q.scale()==.7f&&!ambiguous.q.status().floor_limited);
    // No blind upward probes when pinned timestamps cannot establish headroom.
}
void fresh_samples_and_exclusions(){
    Fixture f;assert(!f.q.submit({0,1,30,6,true,dt}));
    assert(f.q.status().accepted_samples==1);
    for(unsigned i=0;i<1000;i++)assert(!f.q.submit({0,1,30,6,true,dt}));
    assert(f.q.status().accepted_samples==1&&f.q.status().duplicate_samples==1000&&f.q.scale()==1);
    assert(!f.q.submit({999,0,100,10,true,dt})&&f.q.status().stale_samples==1);
    assert(!f.q.submit({1,1,100,10,false,10}));
    assert(!f.q.submit({1,1,100,10,true,.25})); // Excluded serial cannot be revived.
    assert(f.q.status().excluded_samples==1&&f.q.status().duplicate_samples==1001);
    f.serial=1;f.frames(20,18);const float old=f.q.scale();
    f.frame(120,90,dt,false);f.frames(20,18);assert(f.q.scale()==old); // Loading clears pressure.
    std::uint64_t seq=f.serial;
    const std::array<QualityController::Sample,11> invalid={{
        {++seq,1,NAN,5,true,dt},{++seq,1,INFINITY,5,true,dt},{++seq,1,0,5,true,dt},
        {++seq,1,300,5,true,dt},{++seq,1,10,NAN,true,dt},{++seq,1,10,-1,true,dt},
        {++seq,1,10,1001,true,dt},{++seq,1,10,5,true,0},{++seq,1,10,5,true,-dt},
        {++seq,1,10,5,true,1},{++seq,1,10,5,true,NAN}}};
    for(const auto &bad:invalid)assert(!f.q.submit(bad));
    assert(f.q.scale()==old&&f.q.status().invalid_samples==invalid.size());
    assert(f.q.reconfigure(2)&&!f.q.reconfigure(2)&&!f.q.reconfigure(1));
    const auto accepted=f.q.status().accepted_samples;
    assert(!f.q.submit({seq+1,1,200,5,true,.25}));assert(f.q.status().accepted_samples==accepted&&!f.q.status().ready);
    f.epoch=2;f.serial=0;f.frames(50,60);assert(f.q.scale()==old); // Fresh epoch still honors resize cooldown.
    f.frames(20,60);assert(f.q.scale()<old&&f.q.status().epoch==3);
}
void cadence_and_long_run(){
    Fixture a,b,c;
    for(unsigned i=0;i<1200;i++)a.frame(17,6,1./60.);
    for(unsigned i=0;i<900;i++)b.frame(17,6,1./45.);
    for(unsigned i=0;i<1000;i++)c.frame(17,6,i%2?1./75.:1./40.);
    assert(a.q.scale()==1.f&&b.q.scale()==.7f&&c.q.scale()==.7f);a.cooldowns();b.cooldowns();c.cooldowns();
    Fixture long_run;
    for(unsigned i=0;i<100000;i++){
        const double gpu=(i/1800)%3==0?25.:(i/1800)%3==1?8.:14.;
        long_run.frame(gpu,8,dt);
    }
    long_run.cooldowns();assert(long_run.count>10);
}
}

int main(){
    track=true;
    settings_and_manual();severe_overload_and_floor();ordinary_pressure_and_spikes();
    recovery_and_oscillation();cpu_bound_and_present_independence();native_vsync_regression();fresh_samples_and_exclusions();cadence_and_long_run();
    track=false;assert(!allocations);
    std::puts("PASS quality controller: fresh serial/epoch samples only; severe GPU guard, cadence-gated ordinary pressure and recovery; stable60/VSync fidelity preserved; CPU-bound exclusion; cooldown/spike/oscillation resistance; loading/pause/invalid exclusions; exact user interval; zero allocations");
}

#include "engine/world_combat_feedback.h"
#include "engine/world_combat_vfx.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>

using namespace world_combat_feedback;
namespace {
TrainingContactEvent event(TrainingContactKind kind,std::uint64_t serial=1){TrainingContactEvent e;e.kind=kind;e.serial=serial;return e;}
bool same(float a,float b){return std::abs(a-b)<1.e-6f;}
world_combat_vfx::Point center(const world_combat_vfx::Instance &i){world_combat_vfx::Point p;for(const auto &v:i.corners){p.x+=v[0]*.25;p.y+=v[1]*.25;p.z+=v[2]*.25;}return p;}
}
int main(){
    unsigned envelope_cases=0,lifecycle_cases=0,vfx_cases=0;
    for(const auto kind:{TrainingContactKind::EnemyBlock,TrainingContactKind::PlayerBlock,TrainingContactKind::PlayerParry}){
        const auto classified=classify(event(kind));
        assert(classified.impact&&classified.metal&&classified.sound==2&&classified.sound_fade_ms>0);
        assert(classified.rumble_attack_ms>0&&classified.rumble_attack_ms<classified.rumble_ms);
        assert(classified.rumble_tail_low<classified.low&&classified.rumble_tail_high<classified.high);
        for(const double hz:{30.,60.,120.}){
            System system;system.advance(0,0,false);Cue observed,command;
            assert(system.observe(event(kind),{},observed)&&system.take_haptic_command({},command));
            assert(same(command.low,classified.low)&&same(command.high,classified.high));
            assert(command.rumble_ms==classified.rumble_ms);
            assert(!system.take_haptic_command({},command));
            bool tail=false,stopped=false;unsigned changes=1;
            for(unsigned frame=1;frame<40;++frame){system.advance(1./hz,frame/hz,false);
                if(system.take_haptic_command({},command)){
                    ++changes;
                    if(command.rumble_ms){tail=true;assert(same(command.low,classified.rumble_tail_low)&&same(command.high,classified.rumble_tail_high));}
                    else{stopped=true;assert(command.low==0&&command.high==0);}
                }
            }
            assert(tail&&stopped&&changes==3);++envelope_cases;
        }
    }
    // Parry rings longer than a guard; both retain the owned fast transient.
    assert(classify(event(TrainingContactKind::PlayerParry)).sound_fade_ms>classify(event(TrainingContactKind::EnemyBlock)).sound_fade_ms);
    assert(classify(event(TrainingContactKind::EnemyBlock)).sound_fade_ms>classify(event(TrainingContactKind::PlayerBlock)).sound_fade_ms);
    {
        auto broken=event(TrainingContactKind::PlayerBlock);broken.guard_broken=true;broken.damage=25;
        const auto cue=classify(broken);assert(cue.kind==Kind::Block&&cue.priority==4);
        assert(std::strcmp(cue.caption,"GUARD BROKEN")==0&&std::strcmp(cue.hint,"Your guard held")!=0);
        assert(cue.impact&&cue.metal&&cue.sound==2&&cue.damage==0);++lifecycle_cases;
    }
    // Same-frame and later weak events cannot cancel a stronger live motor.
    {
        System s;Cue p,c;s.advance(0,0,false);assert(s.observe(event(TrainingContactKind::EnemyBody,1),{},p));
        assert(s.observe(event(TrainingContactKind::EnemyBlock,2),{},p));assert(s.take_haptic_command({},c));
        assert(same(c.low,.70f)&&same(c.high,.68f));
        s.advance(.04,.04,false);assert(s.take_haptic_command({},c));assert(same(c.low,.70f)&&same(c.high,.36f));
        assert(s.observe(event(TrainingContactKind::PlayerBlock,3),{},p));assert(s.take_haptic_command({},c));assert(same(c.low,.70f)&&same(c.high,.36f));
        assert(s.attack_notice().kind==Kind::Blocked&&s.defense_notice().kind==Kind::Block);++lifecycle_cases;
    }
    // An air swing or miss requests no clang, impact, camera or motors, even
    // when an unrelated old impact notice is still on screen.
    {
        System s;Cue p,c;s.advance(0,0,false);assert(s.observe(event(TrainingContactKind::EnemyBlock),{},p));assert(s.take_haptic_command({},c));
        s.advance(.001,.001,false);auto miss=event(TrainingContactKind::PlayerMiss,2);assert(s.observe(miss,{},p));
        assert(!p.impact&&!p.metal&&p.sound==-1&&!p.sound_fade_ms&&!p.rumble_ms&&p.camera==0);
        assert(!s.take_haptic_command({},c));assert(s.attack_notice().kind==Kind::Miss);
        assert(!s.observe(miss,{},p));++lifecycle_cases;
    }
    // Turning vibration off, pausing, and starting a new bout stop both motors
    // immediately, consume stale events, and cannot replay on resume/re-enable.
    for(unsigned mode=0;mode<3;++mode){
        System s;Cue p,c;s.advance(0,0,false);assert(s.observe(event(TrainingContactKind::PlayerParry),{},p));assert(s.take_haptic_command({},c));
        if(mode==0)assert(s.take_haptic_command({false,false},c));
        if(mode==1){s.advance(.01,.01,true);assert(s.take_haptic_command({},c));assert(!s.observe(event(TrainingContactKind::EnemyBlock,2),{},p));}
        if(mode==2){s.reset();assert(s.take_haptic_command({},c));}
        assert(!c.rumble_ms&&c.low==0&&c.high==0);s.advance(.01,.02,false);assert(!s.take_haptic_command({},c));
        if(mode!=2)assert(!s.observe(event(TrainingContactKind::PlayerParry),{},p));
        ++lifecycle_cases;
    }
    {
        System s;Cue p,c;s.advance(0,0,false);assert(s.observe(event(TrainingContactKind::PlayerBlock),{false,true},p));
        assert(!p.rumble_ms&&p.low==0&&p.high==0&&p.camera==0);assert(!s.take_haptic_command({false,true},c));
        for(unsigned n=2;n<66;++n)assert(s.observe(event(n%2?TrainingContactKind::EnemyBlock:TrainingContactKind::PlayerParry,n),{},p));
        assert(s.stats().haptic_saturated>0&&s.take_haptic_command({},c));assert(c.low<=1&&c.high<=1&&c.rumble_ms<=120);
        s.advance(.25,.25,false);assert(s.take_haptic_command({},c)&&!c.rumble_ms);++lifecycle_cases;
    }
    // Actual contact points, not screen position or aggregate miss counters,
    // anchor the short steel streak; blood is untouched and metal expires.
    for(unsigned kind:{1u,2u,4u}){
        world_combat_vfx::System vfx;world_combat_vfx::Input in;in.resolved_events=true;in.contact_count=1;
        in.contacts[0]={1,kind,{2,1.5,3},{2,0,3},{1,0,0},1,false};WorldTrainingCombat fight;
        vfx.update(fight,in,0);world_combat_vfx::Stage stage;const float right[3]={1,0,0},up[3]={0,1,0};
        unsigned anchored_samples=0;
        for(unsigned n=1;n<=5;++n){fight.clock+=1./60.;vfx.update(fight,in,1./60.);vfx.stage({},right,up,stage);
            unsigned anchored=0;for(unsigned i=0;i<stage.count;++i){const auto point=center(stage.instances[i]);
                if(std::abs(point.x-2)<1e-6&&std::abs(point.y-1.5)<1e-6&&std::abs(point.z-3)<1e-6){++anchored;assert(stage.instances[i].color[3]>0);}}
            assert(anchored==1);++anchored_samples;
        }
        assert(anchored_samples==5&&vfx.stats().metal_bursts==1&&!vfx.stats().body_bursts);
        fight.clock+=.1;vfx.update(fight,in,.1);assert(!vfx.stats().active_particles);++vfx_cases;
    }
    {
        world_combat_vfx::System vfx;world_combat_vfx::Input in;in.resolved_events=true;WorldTrainingCombat fight;
        fight.guarded=99;fight.parries=98;fight.blocks=97;fight.hits=96;fight.misses=95;vfx.update(fight,in,.01);
        assert(!vfx.stats().metal_bursts&&!vfx.stats().body_bursts&&!vfx.stats().active_particles);++vfx_cases;
    }
    std::printf("{\"passed\":true,\"steel_envelope_cases\":%u,\"lifecycle_cases\":%u,\"steel_visual_cases\":%u,\"haptic_rates_hz\":[30,60,120],\"anchored_core_seconds\":%.3f,\"real_geometry_acceptance\":false,\"hardware_controller_acceptance\":false}\n",envelope_cases,lifecycle_cases,vfx_cases,world_combat_vfx::steel_core_lifetime);
}

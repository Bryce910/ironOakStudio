#include "engine/world_sever_blood.h"
#include <cassert>
#include <cstdio>
#include <limits>
using namespace world_sever_blood;
int main(){
    System system;std::array<Pose,capacity> poses={};double time=0;
    poses[5]={true,true,{2,1,3},{1,.2,0},0,1.4f};
    assert(system.update(poses,0,time));assert(system.stage().count==1);
    assert(system.stage().pulses[0].actor==0&&system.stage().pulses[0].part==5);
    assert(system.stats().wounds==1&&system.stats().pulses==1);
    assert(system.update(poses,.1,time+=.1));assert(!system.stage().count);
    poses[5].point={7,1.3,9};
    assert(system.update(poses,.04,time+=.04));assert(system.stage().count==1);
    assert(system.stage().pulses[0].point.x==7&&system.stage().pulses[0].index==1);
    const auto before=system.stats();assert(system.update(poses,1,time+1,true));
    assert(system.stats().pulses==before.pulses&&!system.stage().count);
    for(unsigned n=0;n<20;++n)assert(system.update(poses,.1,time+=.1));
    assert(system.stats().pulses==pulse_count&&!system.stats().active);
    assert(system.update(poses,.1,time+=.1)&&!system.stage().count);
    assert(!system.update(poses,std::numeric_limits<double>::quiet_NaN(),time));
    // A new encounter/explicit reset can create one new wound, without
    // replaying a completed burst on ordinary subsequent frames.
    system.reset();assert(system.update(poses,0,0));assert(system.stage().count==1);
    assert(system.update(poses,.1,.1));
    const auto old_wounds=system.stats().wounds;
    assert(system.update(poses,0,0));assert(system.stage().count==1&&system.stats().wounds==old_wounds+1);
    System many;for(auto &pose:poses)pose={true,true,{0,1,0},{0,1,0},0,1};
    assert(many.update(poses,0,0));assert(many.stage().count==stage_capacity);
    for(unsigned n=0;n<240;++n){assert(many.update(poses,.01,(n+1)*.01));assert(many.stage().count<=stage_capacity);}
    assert(many.stats().wounds==capacity&&many.stats().pulses==capacity*pulse_count&&!many.stats().active);
    System shared;assert(shared.update(poses,0,0,false,0));assert(!shared.stage().count&&shared.stats().active==capacity);
    for(unsigned n=0;n<400;++n){assert(shared.update(poses,.01,(n+1)*.01,false,4));assert(shared.stage().count<=4);}
    assert(shared.stats().pulses==capacity*pulse_count&&!shared.stats().active);
    // Invalid initial poses can become valid once anatomy finishes loading.
    System late;poses={};poses[16].severed=true;
    assert(late.update(poses,0,0)&&!late.stage().count);
    poses[16].valid=true;poses[16].point={0,1,0};
    assert(late.update(poses,.01,.01)&&late.stage().count==1);
    poses[16].severed=false;assert(late.update(poses,.01,.02));assert(!late.stats().active);
    std::puts("PASS severed wounds: moving stumps, independent actors/parts, six finite pulses, pause/rebase, no replay, bounded saturation and late anatomy");
}

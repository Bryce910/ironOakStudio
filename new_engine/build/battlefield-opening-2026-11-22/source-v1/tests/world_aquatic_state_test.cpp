#include "engine/world_aquatic_state.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace world_aquatic_effects;
namespace {
unsigned checks=0;
void require(bool condition,const char* text) {
    ++checks;if(!condition){std::fprintf(stderr,"FAIL %s\n",text);std::exit(1);}
}
struct Water {unsigned calls=0;int kind=0;};
bool habitat(double x,double z,double& surface,double& bed,void* data) {
    auto& water=*static_cast<Water*>(data);++water.calls;surface=8;bed=6.2;
    if(water.kind==1)return false;
    if(water.kind==2)return std::abs(z)<3&&std::abs(x)<40;
    if(water.kind==3)return !(x>-.8&&x<-.7);
    if(water.kind==4){surface=std::numeric_limits<double>::quiet_NaN();return true;}
    return true;
}
void tick(System& system,Input input,Water& water) {
    const auto before=water.calls;system.update(input,habitat,&water);
    require(water.calls-before<=64,"hard habitat callback budget remains64");
    require(system.stats.fish<=fish_cap&&system.stats.particles<=particle_cap&&system.stats.ripples<=ripple_cap,
            "finite fish/particle/ripple capacities hold");
}
}
int main() {
    Input in;in.enabled=true;in.dt=1./60.;in.player[1]=10.;in.observer[1]=9.;
    Water water;water.kind=1;System dry;
    for(unsigned i=0;i<30;i++)tick(dry,in,water);
    require(dry.stats.fish==0&&dry.stats.particles==0&&dry.stats.ripples==0,"dry terrain never hosts aquatic content");
    water.kind=2;System creek;
    bool all_wet=true,all_depth=true;
    for(unsigned frame=0;frame<900;frame++) {
        tick(creek,in,water);
        for(const auto& f:creek.fish)if(f.alive) {
            all_wet=all_wet&&std::abs(f.z)<3&&std::abs(f.x)<40;
            all_depth=all_depth&&f.y>=6.35&&f.y<=7.85&&f.size*.84>=.23&&f.size*.84<=.50;
        }
    }
    require(creek.stats.fish_spawned>0&&creek.stats.fish>0,"bounded search populates narrow real water corridors");
    require(all_wet&&all_depth,"fish remain wet, above bed, under surface, realistically sized");
    require(creek.stats.entries==0,"observer near river does not fabricate player entry");

    water.kind=0;System effects;in.player[1]=8.32;in.swimming=true;
    tick(effects,in,water);
    require(effects.stats.entries==1&&effects.stats.particles>0&&effects.stats.ripples==1,"actual wet transition creates splash crown and ring");
    for(unsigned i=0;i<25;i++)tick(effects,in,water);
    require(effects.stats.entries==1&&effects.stats.strokes==0,"stationary surface swimmer does not repeatedly splash");
    in.moving=true;
    for(unsigned i=0;i<40;i++)tick(effects,in,water);
    require(effects.stats.strokes>=2&&effects.stats.ripples>=2,"moving surface strokes produce trailing ripples");
    in.submerged=true;in.player[1]=7.5;
    for(unsigned i=0;i<20;i++)tick(effects,in,water);
    unsigned bubbles=0;for(const auto& p:effects.particles)if(p.alive&&p.bubble)++bubbles;
    require(effects.stats.bubbles_spawned>0&&bubbles>0,"submerged player releases rising bubbles");
    in.moving=in.submerged=in.swimming=false;in.player[1]=12;
    for(unsigned i=0;i<240;i++)tick(effects,in,water);
    require(effects.stats.particles==0&&effects.stats.ripples==0,"effects expire without lingering allocations or stale rings");
    in.player[1]=8.32;
    tick(effects,in,water);
    require(effects.stats.entries==2,"leaving and reentering water triggers exactly one new burst");
    in.enabled=false;tick(effects,in,water);
    require(effects.stats.fish==0&&effects.stats.particles==0&&effects.stats.ripples==0,"disabled scenes clear all active aquatic content");

    // A fish's segment crosses a dry sliver although its destination is wet.
    // Real habitat midpoint rejection must prevent it crossing the bank.
    System crossing;water.kind=3;in.enabled=true;in.dt=.1;in.player[0]=-2;in.player[1]=7;in.observer[0]=-1;
    crossing.fish[0]={-1,7,0,0,0,.4,true};
    tick(crossing,in,water);
    require(!crossing.fish[0].alive,"midsegment dry bank prevents fish crossing between wet endpoints");
    require(crossing.stats.fish_fled>0,"nearby actual player triggers fish flight response");
    water.kind=4;System invalid;tick(invalid,in,water);
    require(invalid.stats.fish==0&&invalid.stats.entries==0,"nonfinite habitat results fail closed");
    in.player[0]=std::numeric_limits<double>::infinity();tick(crossing,in,water);
    require(crossing.stats.fish==0&&crossing.stats.particles==0,"nonfinite player input clears active state safely");
    // A wake must trail the actual gameplay forward axes at all four headings.
    for(unsigned heading=0;heading<4;++heading){
        System wake;Water open;Input stroke;stroke.enabled=stroke.swimming=stroke.moving=true;stroke.dt=.1;stroke.player[1]=8.32;stroke.yaw=heading*pi*.5;
        for(unsigned f=0;f<4;++f)tick(wake,stroke,open);
        const auto &ring=wake.ripples[1];const double fx=std::sin(stroke.yaw),fz=-std::cos(stroke.yaw);
        require(ring.alive&&ring.x*fx+ring.z*fz<-.3,"stroke wake trails the actual travel heading");
    }
    std::printf("world_aquatic_state_test: PASS %u checks\n",checks);
}

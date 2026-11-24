#ifndef VALWOOD_WORLD_AQUATIC_STATE_H
#define VALWOOD_WORLD_AQUATIC_STATE_H
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace world_aquatic_effects {
constexpr unsigned fish_cap=48,particle_cap=192,ripple_cap=64;
constexpr double pi=3.14159265358979323846;
using Habitat=bool(*)(double x,double z,double& surface,double& bed,void*);
struct Input {
    double dt=0,player[3]={},observer[3]={},yaw=0;
    bool enabled=false,swimming=false,submerged=false,moving=false;
};
struct Fish { double x=0,y=0,z=0,yaw=0,phase=0,size=1; bool alive=false; };
struct Particle {
    double x=0,y=0,z=0,vx=0,vy=0,vz=0,age=0,life=0,size=0;
    bool bubble=false,alive=false;
    double surface=0,bed=0;
};
struct Ripple { double x=0,y=0,z=0,age=0,life=0,size=0; bool alive=false; };
struct Stats {
    std::uint64_t habitat_queries=0,fish_spawned=0,fish_fled=0,entries=0,strokes=0,bubbles_spawned=0;
    unsigned fish=0,particles=0,ripples=0;
};
inline std::uint32_t hash(std::uint32_t value) {
    value^=value>>16;value*=0x7feb352du;value^=value>>15;value*=0x846ca68bu;return value^(value>>16);
}
inline double random(std::uint32_t value) { return (hash(value)&0xffffffu)/16777216.; }
class System {
    std::uint32_t serial_=1;
    unsigned particle_cursor_=0,ripple_cursor_=0,spawn_epoch_=0;
    unsigned query_budget_=0;
    double time_=0,stroke_=0,bubble_=0;
    bool previous_wet_=false;
    bool sample(Habitat query,void* data,double x,double z,double& level,double& bed) {
        if(!query_budget_)return false;
        --query_budget_;
        ++stats.habitat_queries;
        return query&&query(x,z,level,bed,data)&&std::isfinite(level)&&std::isfinite(bed)&&level>bed;
    }
    void ring(double x,double level,double z,double size) {
        auto& r=ripples[ripple_cursor_++%ripple_cap];r={x,level+.028,z,0,1.65,size,true};
    }
    void drops(double x,double level,double bed,double z,unsigned count,double energy) {
        for(unsigned i=0;i<count;i++) {
            const auto seed=serial_++;const double angle=random(seed)*2*pi,speed=(.45+random(seed+11))*energy;
            auto& p=particles[particle_cursor_++%particle_cap];
            p={x,level+.035,z,std::cos(angle)*speed,(1.1+random(seed+29)*1.2)*energy,
               std::sin(angle)*speed,0,.38+random(seed+39)*.20,.018+random(seed+51)*.025,false,true,level,bed};
        }
    }
public:
    std::array<Fish,fish_cap> fish{};
    std::array<Particle,particle_cap> particles{};
    std::array<Ripple,ripple_cap> ripples{};
    Stats stats{};
    double time() const { return time_; }
    void clear() {
        fish={};particles={};ripples={};previous_wet_=false;stroke_=bubble_=0;
        stats.fish=stats.particles=stats.ripples=0;
    }
    void update(const Input& in,Habitat query,void* data=nullptr) {
        bool valid=in.enabled&&query&&std::isfinite(in.dt)&&in.dt>=0&&std::isfinite(in.yaw);
        for(double v:in.player)valid=valid&&std::isfinite(v)&&std::abs(v)<1e8;
        for(double v:in.observer)valid=valid&&std::isfinite(v)&&std::abs(v)<1e8;
        if(!valid){clear();return;}
        const double dt=std::min(in.dt,.1);time_+=dt;query_budget_=64;
        double water=0,bed=0;
        const bool wet=sample(query,data,in.player[0],in.player[2],water,bed)&&in.player[1]-1.75<=water+.05;
        if(wet&&!previous_wet_){drops(in.player[0],water,bed,in.player[2],24,1.25);ring(in.player[0],water,in.player[2],.35);++stats.entries;}
        previous_wet_=wet;
        stroke_+=dt;bubble_+=dt;
        if(wet&&in.moving&&!in.submerged&&stroke_>=(in.swimming?.22:.34)) {
            stroke_=0;const double backx=in.player[0]-std::sin(in.yaw)*.35,backz=in.player[2]+std::cos(in.yaw)*.35;
            drops(backx,water,bed,backz,in.swimming?7:4,in.swimming?.65:.4);
            ring(backx,water,backz,in.swimming?.27:.18);++stats.strokes;
        }
        if(wet&&in.submerged&&bubble_>=.14) {
            bubble_=0;
            for(unsigned i=0;i<3;i++) {
                const auto seed=serial_++;auto& p=particles[particle_cursor_++%particle_cap];
                p={in.player[0]+(random(seed)-.5)*.5,std::min(water-.08,in.player[1]-.35),
                   in.player[2]+(random(seed+1)-.5)*.5,(random(seed+4)-.5)*.15,.35+random(seed+5)*.3,
                   (random(seed+6)-.5)*.15,0,2.,.015+random(seed+7)*.025,true,true,water,bed};++stats.bubbles_spawned;
            }
        }
        stats.particles=stats.ripples=0;
        for(auto& p:particles)if(p.alive) {
            p.age+=dt;p.x+=p.vx*dt;p.y+=p.vy*dt;p.z+=p.vz*dt;
            if(!p.bubble)p.vy-=8.5*dt;
            if(p.age>=p.life||p.y<p.bed+.01||(p.y>=p.surface+.02&&p.bubble))p.alive=false;
            else if(!p.bubble&&p.y<=p.surface&&p.vy<0)p.alive=false;
            if(p.alive)++stats.particles;
        }
        for(auto& r:ripples)if(r.alive){r.age+=dt;if(r.age>=r.life)r.alive=false;else ++stats.ripples;}
        stats.fish=0;
        const auto epoch=spawn_epoch_++;
        for(unsigned i=0;i<fish_cap;i++) {
            auto& f=fish[i];
            if(f.alive&&std::hypot(f.x-in.observer[0],f.z-in.observer[2])>42)f.alive=false;
            // Half the school advances each frame, using midpoint + endpoint
            // habitat tests. Hard total budget includes the player's query.
            if((i%2)!=(epoch%2)||query_budget_<2){if(f.alive)++stats.fish;continue;}
            if(!f.alive) {
                // Six fish share each deterministic school center. A bounded
                // rotating search lets narrow creeks populate without dry fish.
                for(unsigned attempt=0;attempt<3&&!f.alive&&query_budget_>2;attempt++) {
                    const auto seed=std::uint32_t(i/6)*731u+(epoch/12+attempt)*199u;
                    const double angle=random(seed+9)*2*pi,range=4+random(seed+17)*23;
                    const double x=in.observer[0]+std::cos(angle)*range+(random(i+19)-.5)*2;
                    const double z=in.observer[2]+std::sin(angle)*range+(random(i+57)-.5)*2;
                    double level=0,floor=0;
                    if(sample(query,data,x,z,level,floor)&&level-floor>.65) {
                        f={x,floor+(level-floor)*.46,z,angle+pi*.5,random(i+11)*2*pi,.28+random(i+31)*.30,true};
                        ++stats.fish_spawned;
                    }
                }
            }
            if(!f.alive)continue;
            if(query_budget_<2){++stats.fish;continue;}
            const double fish_dt=dt*2;
            const double distance=std::hypot(f.x-in.player[0],f.z-in.player[2]);
            const bool flee=distance<4.5&&std::abs(f.y-in.player[1])<3;
            double desired=f.yaw+.35*std::sin(time_*.7+double(i/6));
            if(flee){desired=std::atan2(f.z-in.player[2],f.x-in.player[0]);++stats.fish_fled;}
            const double turn=std::remainder(desired-f.yaw,2*pi);
            f.yaw+=std::clamp(turn,-fish_dt*2.8,fish_dt*2.8);
            const double speed=flee?2.5:.45+random(i+4)*.35;
            const double nx=f.x+std::cos(f.yaw)*speed*fish_dt,nz=f.z+std::sin(f.yaw)*speed*fish_dt;
            double level=0,floor=0;
            double middle_level=0,middle_floor=0;
            if(sample(query,data,(f.x+nx)*.5,(f.z+nz)*.5,middle_level,middle_floor)&&middle_level-middle_floor>.65&&
               sample(query,data,nx,nz,level,floor)&&level-floor>.65) {
                f.x=nx;f.z=nz;const double target=floor+(level-floor)*(.44+.09*std::sin(time_*.8+f.phase));
                f.y=std::clamp(f.y+(target-f.y)*std::min(1.,fish_dt*2.),floor+.15,level-.15);
            } else {f.alive=false;continue;}
            ++stats.fish;
        }
    }
};
}
#endif

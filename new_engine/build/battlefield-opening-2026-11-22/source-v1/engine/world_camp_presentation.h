#ifndef VALWOOD_WORLD_CAMP_PRESENTATION_H
#define VALWOOD_WORLD_CAMP_PRESENTATION_H
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
// Render-only campsite acting. No story, navigation, collision or input state.
namespace world_camp_presentation {
constexpr double pi=3.14159265358979323846;
// Each arrival actor retains its own appearance and independent idle pair.
constexpr std::array<unsigned,3> arrival_clips={3,1,0};
static_assert(arrival_clips[0]!=arrival_clips[1]&&arrival_clips[0]!=arrival_clips[2]&&arrival_clips[1]!=arrival_clips[2],"arrival idle appearances must be independent");
using Vec=std::array<double,3>;
inline double smooth(double x){x=std::clamp(x,0.,1.);return x*x*(3.-2.*x);}
inline double approach(double value,double goal,double rate,double maximum,double dt){
    return value+std::clamp((goal-value)*(-std::expm1(-rate*dt)),-maximum*dt,maximum*dt);
}
inline bool finite(Vec p){return std::all_of(p.begin(),p.end(),[](double x){return std::isfinite(x)&&std::abs(x)<=1.e6;});}
struct Placement{Vec feet={};double yaw=0;};
struct Input{Vec eye={};double dt=0;int actor=-1;bool speaking=false;unsigned beat=0;};
struct Actor{
    double phase=0,attention=0,speed=1,clock=0,greeting=9,phrase=9,cooldown=0;
    double social=0;
    bool nearby=false,speaking=false,listening=false;
};
struct State{
    std::array<Placement,3> placements={};std::array<Actor,3> actors={};
    // Exact retained source durations, in appearance order. Seated authoring
    // supplies its own periods; an actor never switches to a different mesh.
    std::array<double,3> seconds={4.,4.5,4.7};
    int last_actor=-1;unsigned last_beat=0;bool last_speaking=false;
    std::uint64_t acknowledgments=0;
    bool configure(const std::array<Placement,3>&p,const std::array<double,3>&periods={4.,4.5,4.7}){
        for(const auto&q:p)if(!finite(q.feet)||!std::isfinite(q.yaw))return false;
        for(double t:periods)if(!std::isfinite(t)||t<.5||t>30.)return false;
        *this=State{};placements=p;seconds=periods;for(unsigned i=0;i<3;++i)actors[i].phase=double(i)*.37;return true;
    }
    bool update(const Input&input){
        if(!finite(input.eye)||!std::isfinite(input.dt)||input.dt<0||input.dt>.25||input.actor< -1||input.actor>=3)return false;
        if(input.dt==0)return true;
        const double dt=input.dt;
        const bool phrase_changed=input.actor>=0&&(input.actor!=last_actor||input.beat!=last_beat||input.speaking!=last_speaking);
        constexpr double enter[]={4.5,5.3,4.8};
        for(unsigned i=0;i<3;++i){auto&a=actors[i];const auto&p=placements[i];a.clock+=dt;a.cooldown=std::max(0.,a.cooldown-dt);a.greeting+=dt;a.phrase+=dt;
            const double dx=input.eye[0]-p.feet[0],dz=input.eye[2]-p.feet[2],distance=std::hypot(dx,dz);
            const double relative=std::remainder(std::atan2(dx,dz)-p.yaw,2*pi);
            const bool selected=input.actor==int(i),in_front=std::abs(relative)<1.50;
            const bool near=selected||(in_front&&distance<(a.nearby?enter[i]+1.4:enter[i])&&std::abs(input.eye[1]-(p.feet[1]+1.7))<2.5);
            if(near&&!a.nearby&&a.cooldown==0){a.greeting=-.14*double(i);a.cooldown=8.2+double(i);++acknowledgments;}
            a.nearby=near;a.speaking=selected&&input.speaking;a.listening=selected&&!input.speaking;
            if(selected&&phrase_changed){a.phrase=-.10*double(i);++acknowledgments;}
            a.attention=approach(a.attention,near?1.:0.,near?4.2:2.0,2.5,dt);
            // Nearby companions settle into a quieter listening rhythm. This
            // only retimes a genuine retained idle: there is no fabricated
            // head turn or whole-body swivel on fixed feet.
            const bool company=input.actor>=0&&std::hypot(placements[unsigned(input.actor)].feet[0]-p.feet[0],placements[unsigned(input.actor)].feet[2]-p.feet[2])<8.;
            a.social=approach(a.social,company&&!selected?1.:0.,3.,1.8,dt);
            // React through the retained idle, without resetting its phase or
            // deforming the unrigged head/collar. Distinct greeting envelopes
            // and a quieter listening cadence preserve planted body geometry.
            const auto pulse=[](double age,double length){return age>0&&age<length?std::pow(std::sin(pi*age/length),2):0.;};
            constexpr double greeting_gain[]={.14,.08,.11};
            const double greeting=greeting_gain[i]*pulse(a.greeting,1.15);
            const double phrase=(a.speaking?.07:.035)*pulse(a.phrase,a.speaking?.88:1.25);
            // Slow variation keeps the short loops from repeating in lockstep.
            // Speed stays positive; dialogue beats never seek/reset the pose.
            const double ambient=.96+.075*std::sin((a.clock+double(i)*3.7)*(2*pi/(17.+double(i)*3.)));
            const double base=a.speaking?1.12:a.listening?.67:ambient-.17*a.social;
            const double target_speed=base+greeting+phrase;
            a.speed=approach(a.speed,target_speed,3.,1.,dt);
            a.phase=std::fmod(a.phase+dt*a.speed/seconds[i],1.);
        }
        last_actor=input.actor;last_beat=input.beat;last_speaking=input.speaking;return true;
    }
};
}
#endif

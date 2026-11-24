#ifndef VALWOOD_WORLD_NPC_INTRO_H
#define VALWOOD_WORLD_NPC_INTRO_H
// Optional first-contact timeline. Pure CPU state: no media, voice, actor motion,
// player relocation or quest credit. The host owns camera collision and audio.
#include <algorithm>
#include <array>
#include <cmath>
#include <istream>
#include <locale>
#include <sstream>
#include <string>
#include "world_opening_arrival_camp.h"
namespace world_npc_intro {
struct Config {bool enabled=false;double duration_seconds=36.,skip_seconds=.8;};
struct Vec {double x=0,y=0,z=0;};
struct Camera {Vec eye,target;double fov=.86,fade=0,letterbox=.10;};
struct Frame {
    bool active=false,completed=false,skipped=false;double time=0,duration=0,skip_progress=0;
    unsigned shot_index=0;Camera camera;int speaker_actor=-1;
    const char *title="",*speaker="",*subtitle="";
};
inline bool validate(const Config &c,std::string &error){
    if(!std::isfinite(c.duration_seconds)||c.duration_seconds<.1||c.duration_seconds>300.||
       !std::isfinite(c.skip_seconds)||c.skip_seconds<.3||c.skip_seconds>3.){error="Invalid NPC introduction duration or skip hold";return false;}
    error.clear();return true;
}
// An absent file leaves Config disabled. A supplied valid version-one file opts
// in. Malformed/oversized/trailing records never mutate the caller's config.
inline bool parse(std::istream &input,Config &out,std::string &error){
    std::string bytes;char ch;while(input.get(ch)){if(ch=='\0'||bytes.size()>=512){error="NPC introduction settings exceed bounds";return false;}bytes+=ch;}
    if(input.bad()||(input.fail()&&!input.eof())){error="Unreadable NPC introduction settings";return false;}
    Config staged;staged.enabled=true;std::istringstream rows(bytes);rows.imbue(std::locale::classic());std::string tag;unsigned version=0;
    if(!(rows>>tag>>version)||tag!="VALWOOD_NPC_INTRO"||version!=1||!(rows>>tag>>staged.duration_seconds)||tag!="duration"){error="Malformed NPC introduction settings";return false;}
    rows>>std::ws;if(!rows.eof()){error="Unexpected NPC introduction settings records";return false;}
    if(!validate(staged,error))return false;
    out=staged;return true;
}
namespace detail {
inline double ease(double t){t=std::clamp(t,0.,1.);return t*t*t*(t*(t*6.-15.)+10.);}
inline Vec mix(Vec a,Vec b,double t){return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};}
inline Vec actor_point(const world_opening_arrival_camp::Actor &a,double side,double forward,double height){
    // Resolve shot offsets in a stable yaw-relative X/Z basis.
    return {a.feet.x+std::cos(a.yaw)*side+std::sin(a.yaw)*forward,a.feet.y+height,a.feet.z+std::sin(a.yaw)*side-std::cos(a.yaw)*forward};
}
}
inline Frame sample(const Config &c,const world_opening_arrival_camp::Config &camp,unsigned first_actor,double elapsed,bool reduced_motion=false){
    Frame f;f.active=true;f.duration=c.duration_seconds;
    f.time=std::isfinite(elapsed)?std::clamp(elapsed,0.,c.duration_seconds):0.;
    const double t=f.time/c.duration_seconds;
    constexpr std::array<double,6> cuts{{0,.22,.42,.61,.82,1}};
    while(f.shot_index<4&&t>=cuts[f.shot_index+1])++f.shot_index;
    const unsigned shot=f.shot_index;const double local=(t-cuts[shot])/(cuts[shot+1]-cuts[shot]);
    const auto &a=camp.actors[std::min(first_actor,2u)];const auto &rowan=camp.actors[0];const auto &bram=camp.actors[1];const auto &ellis=camp.actors[2];
    const double h=a.seated?1.00:1.32;Vec eye0,eye1,target;
    switch(shot){
    case 0:eye0=detail::actor_point(a,-2.1,4.4,2.0);eye1=detail::actor_point(a,-1.7,4.0,1.85);target={camp.center.x,camp.center.y+.95,camp.center.z};f.title="STRANGERS BY THE FIRE";break;
    case 1:eye0=detail::actor_point(rowan,.85,2.6,1.50);eye1=detail::actor_point(rowan,.75,2.45,1.47);target=detail::actor_point(rowan,0,0,rowan.seated?1.02:1.40);f.title="THE MEADOWS OF VALOIS";break;
    case 2:eye0=detail::actor_point(bram,-1.1,3.1,1.72);eye1=detail::actor_point(bram,-.95,2.9,1.69);target=detail::actor_point(bram,0,0,bram.seated?1.02:1.40);f.title="A ROAD INTO VALWOOD";break;
    case 3:eye0=detail::actor_point(ellis,-1.4,-2.4,1.65);eye1=detail::actor_point(ellis,-1.25,-2.3,1.62);target=detail::actor_point(ellis,0,0,ellis.seated?1.02:1.40);f.title="OLD MAGIC, UNANSWERED QUESTIONS";break;
    default:eye0=detail::actor_point(a,1.1,3.1,1.55);eye1=detail::actor_point(a,.9,2.8,1.50);target=detail::actor_point(a,0,0,h);f.title="ONE STEP AT A TIME";break;
    }
    // Reduced motion holds one composition for the entire introduction, not
    // five cuts or a moving endpoint. Subtitles and titles still advance.
    if(reduced_motion){f.camera.eye=detail::actor_point(a,-1.7,4.0,1.85);f.camera.target={camp.center.x,camp.center.y+.95,camp.center.z};}
    else {f.camera.eye=detail::mix(eye0,eye1,detail::ease(local));f.camera.target=target;
        const double seconds=local*(cuts[shot+1]-cuts[shot])*c.duration_seconds;
        const double remaining=(1-local)*(cuts[shot+1]-cuts[shot])*c.duration_seconds;
        f.camera.fade=std::max(1-detail::ease(seconds/.16),shot<4?1-detail::ease(remaining/.16):0.);
    }
    struct Line {double until;int actor;const char *speaker,*text;};
    static constexpr Line lines[]={
      {5./36,0,"Rowan","Easy. You're safe by the fire. Take a breath."},
      {9./36,-1,"You","Where am I? This place was only on a screen."},
      {14./36,0,"Rowan","The Meadows of Valois. A small corner of Valwood."},
      {19./36,1,"Bram","The road leads to town. Other roads will take you farther."},
      {24./36,1,"Bram","For now, stay on the path. We'll help you find your bearings."},
      {30./36,2,"Ellis","There's old magic in these woods. We don't understand all of it."},
      {33./36,-1,"You","Then I need to find someone who does."},
      {1.01,0,"Rowan","One step at a time. Start here, with us."}
    };
    for(const auto &line:lines)if(t<line.until){f.speaker_actor=line.actor;f.speaker=line.speaker;f.subtitle=line.text;break;}
    return f;
}
class State {
    Config config_;world_opening_arrival_camp::Config camp_;unsigned actor_=0;
    double elapsed_=0,hold_=0;bool active_=false,armed_=false,paused_=false;
public:
    bool active()const{return active_;}double elapsed()const{return elapsed_;}
    void reset(){active_=armed_=paused_=false;elapsed_=hold_=0;}
    bool begin(const Config &c,const world_opening_arrival_camp::Config &camp,unsigned actor){
        std::string error;if(active_||!c.enabled||!validate(c,error)||!camp.enabled||!world_opening_arrival_camp::validate(camp,error)||actor>=3)return false;
        config_=c;camp_=camp;actor_=actor;elapsed_=hold_=0;active_=true;armed_=paused_=false;return true;
    }
    Frame sample(bool reduced_motion=false)const{if(!active_)return {};auto f=world_npc_intro::sample(config_,camp_,actor_,elapsed_,reduced_motion);f.skip_progress=std::clamp(hold_/config_.skip_seconds,0.,1.);return f;}
    Frame tick(double dt,bool paused,bool confirm_down,bool interact_down,bool cancel_down,bool reduced_motion=false){
        if(!active_)return {};
        if(paused){paused_=true;return sample(reduced_motion);}
        if(paused_){paused_=false;armed_=false;hold_=0;}
        if(!confirm_down&&!interact_down&&!cancel_down)armed_=true;
        dt=std::isfinite(dt)&&dt>0?std::min(dt,.1):0.;elapsed_=std::min(config_.duration_seconds,elapsed_+dt);
        if(armed_&&confirm_down)hold_+=dt;else hold_=0;
        auto f=sample(reduced_motion);f.skipped=hold_+1e-9>=config_.skip_seconds;
        f.completed=f.skipped||elapsed_+1e-9>=config_.duration_seconds;
        if(f.completed){active_=false;f.active=false;}return f;
    }
};
}
#endif

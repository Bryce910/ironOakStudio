#ifndef VALWOOD_WORLD_OPENING_ARRIVAL_CAMP_H
#define VALWOOD_WORLD_OPENING_ARRIVAL_CAMP_H
// Optional first-contact story. The camp's render host shares these exact
// positions; this header owns dialogue/progress only and never moves actors.
#include <array>
#include <cmath>
#include <istream>
#include <locale>
#include <sstream>
#include <string>
namespace world_opening_arrival_camp {
struct Vec {double x=0,y=0,z=0;};
struct Actor {Vec feet;double yaw=0;bool seated=false;};
struct Config {bool enabled=false;Vec center;std::array<Actor,3> actors;double talk_radius=2.7;};
constexpr unsigned complete_mask=7;
inline bool valid_mask(unsigned mask){return mask<=complete_mask;}
inline unsigned completed(unsigned mask){return unsigned(bool(mask&1))+unsigned(bool(mask&2))+unsigned(bool(mask&4));}
inline bool valid(Vec v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::abs(v.x)<1000000&&std::abs(v.z)<1000000&&std::abs(v.y)<100000;}
inline bool validate(const Config &c,std::string &error){
    if(!c.enabled){error.clear();return true;}
    if(!valid(c.center)||!std::isfinite(c.talk_radius)||c.talk_radius<1||c.talk_radius>3){error="Invalid arrival camp center or talk radius";return false;}
    for(unsigned i=0;i<3;++i){const auto &a=c.actors[i];
        if(!valid(a.feet)||!std::isfinite(a.yaw)||std::abs(a.yaw)>100000||std::hypot(a.feet.x-c.center.x,a.feet.z-c.center.z)>15||std::abs(a.feet.y-c.center.y)>3){error="Invalid arrival camper placement";return false;}
        for(unsigned j=0;j<i;++j)if(std::hypot(a.feet.x-c.actors[j].feet.x,a.feet.z-c.actors[j].feet.z)<1.1){error="Arrival campers are too close together";return false;}
    }
    error.clear();return true;
}
inline bool parse(std::istream &input,Config &out,std::string &error){
    std::string bytes;char ch;while(input.get(ch)){if(ch=='\0'||bytes.size()>=4096){error="Arrival camp settings exceed bounds";return false;}bytes+=ch;}
    if(input.bad()||(input.fail()&&!input.eof())){error="Unreadable arrival camp settings";return false;}
    std::istringstream rows(bytes);rows.imbue(std::locale::classic());Config c;c.enabled=true;std::string tag;unsigned version=0;
    if(!(rows>>tag>>version)||tag!="VALWOOD_ARRIVAL_CAMP"||version!=1||!(rows>>tag>>c.center.x>>c.center.y>>c.center.z)||tag!="center"){error="Invalid arrival camp header or center";return false;}
    for(unsigned i=0;i<3;++i){unsigned index=0,seated=0;auto &a=c.actors[i];
        if(!(rows>>tag>>index>>a.feet.x>>a.feet.y>>a.feet.z>>a.yaw>>seated)||tag!="actor"||index!=i||seated>1){error="Arrival camp needs exactly three ordered actors";return false;}a.seated=seated!=0;
    }
    rows>>std::ws;if(!rows.eof()){error="Unexpected arrival camp settings records";return false;}
    if(!validate(c,error))return false;
    out=c;return true;
}
inline const char *name(unsigned i){static constexpr const char *v[]={"Rowan","Bram","Ellis"};return i<3?v[i]:"";}
inline const char *role(unsigned i){static constexpr const char *v[]={"Camp cook","Roadwarden","Traveling herbalist"};return i<3?v[i]:"";}
inline const char *interaction(unsigned i){static constexpr const char *v[]={"Talk to Rowan","Talk to Bram","Talk to Ellis"};return i<3?v[i]:"";}
inline const char *objective(unsigned mask){static constexpr const char *v[]={"Speak to the travelers by the fire (0/3).","Speak to the travelers by the fire (1/3).","Speak to the travelers by the fire (2/3).","Speak to Bram before taking the road."};return v[completed(mask&complete_mask)];}
struct Line {const char *speaker="",*text="";};
inline Line line(unsigned actor,unsigned beat,bool repeat){
    static constexpr std::array<Line,6> rowan={{{"Rowan","Easy now. You landed hard. Take a moment by the fire."},{"You","I was at home. I turned on a game... I need to get back to my family."},{"Rowan","I don't know how you got here, but you're safe with us. There's food on the fire."},{"You","Where is here?"},{"Rowan","The Meadows of Valois. We're resting before the road into town."},{"Rowan","Bram knows the road, and Ellis can help you steady yourself. Take your time before you go."}}};
    static constexpr std::array<Line,6> bram={{{"Bram","Rowan says you came out of that light. Never seen anything like it."},{"You","You said Valois. That was the name on my screen."},{"Bram","Then let's start with what we can see. This road leads to town. Keep to it; the woods are less kind."},{"You","Who might know how I got here?"},{"Bram","Hugh keeps the Copper Lantern in town. He can point you toward Mara. But Rowan could use a hand before you leave."},{"Bram","Get your bearings with Rowan and Ellis. Then come back to me before you take the road."}}};
    static constexpr std::array<Line,6> ellis={{{"Ellis","You're shaking. Breathe. Feel the ground beneath your feet."},{"You","This can't be real. I have to get home."},{"Ellis","I can't explain the light. But you can rest here, and you don't have to face this moment alone."},{"You","It feels real. The heat... the smell of the fire."},{"Ellis","Then take it one step at a time. Get your bearings with all of us before you leave."},{"Ellis","When you're ready, speak to Bram again. He knows the road into town."}}};
    static constexpr std::array<Line,2> rowan_repeat={{{"Rowan","The fire's still warm. Rest here whenever you need."},{"You","Thank you. I'll find my way home."}}};
    static constexpr std::array<Line,2> bram_repeat={{{"Bram","Speak to Rowan and Ellis, then come back to me. There is something you can help us with."},{"You","The road into town. Got it."}}};
    static constexpr std::array<Line,2> ellis_repeat={{{"Ellis","One step at a time. You're steadier than when you arrived."},{"You","I'm ready to look for answers."}}};
    if(actor>=3||beat>=(repeat?2u:6u))return {};
    if(repeat)return actor==0?rowan_repeat[beat]:actor==1?bram_repeat[beat]:ellis_repeat[beat];
    return actor==0?rowan[beat]:actor==1?bram[beat]:ellis[beat];
}
// The battlefield route reaches the same authored camp after an escape,
// rather than depositing the player in front of witnesses to the portal.
inline Line line(unsigned actor,unsigned beat,bool repeat,bool battlefield){
    if(!battlefield)return line(actor,beat,repeat);
    static constexpr std::array<Line,6> rowan={{{"Rowan","Easy. You came out of the fighting. Stay by the fire. You're safe here."},{"You","I was in my apartment. Then the light... arrows, soldiers. I had to run."},{"Rowan","The Broken Field. That battle has swallowed too many people. Here, take a breath. There's food on the fire."},{"You","Where am I? This was supposed to be a game."},{"Rowan","The Meadows of Valois. Town is down the road. Whatever brought you here, you're alive, and you don't have to face it alone."},{"Rowan","Talk to Bram and Ellis. They'll help you get your bearings before you take the road."}}};
    static constexpr std::array<Line,6> bram={{{"Bram","That sword... you came through the Broken Field?"},{"You","A soldier tried to kill me. I took it from him. I didn't have a choice."},{"Bram","Then keep it. You're clear of the battle now. Lower your blade by the fire, but stay alert on the road."},{"You","I need to get home to my family. Who could tell me what happened?"},{"Bram","Hugh keeps the Copper Lantern in town. He can point you toward Mara. First, Rowan could use a hand."},{"Bram","Speak to Rowan and Ellis. Then come back to me before you take the road. We look after each other here."}}};
    static constexpr std::array<Line,6> ellis={{{"Ellis","Your hands are shaking. Watch the flames. Breathe with me."},{"You","I can still hear the arrows. I... I think I killed someone."},{"Ellis","You were trying to live. You're here now, by this fire. Feel the ground under your feet."},{"You","I have a family. I need to find my way back to them."},{"Ellis","Then stay alive for them. Rest. Learn where you are. One step at a time."},{"Ellis","When you're ready, speak to Bram. He knows the road into town, and the people who can help."}}};
    static constexpr std::array<Line,2> rowan_repeat={{{"Rowan","You made it out. The fire is still warm. Rest here whenever you need."},{"You","Thank you. I'll find my way home."}}};
    static constexpr std::array<Line,2> bram_repeat={{{"Bram","Keep the sword for the road. Speak to Rowan and Ellis, then come back to me. There's something you can help us with."},{"You","One thing at a time. I'm listening."}}};
    static constexpr std::array<Line,2> ellis_repeat={{{"Ellis","Your breathing is steadier. You're not on that battlefield anymore."},{"You","I'm ready to look for answers."}}};
    if(actor>=3||beat>=(repeat?2u:6u))return {};
    if(repeat)return actor==0?rowan_repeat[beat]:actor==1?bram_repeat[beat]:ellis_repeat[beat];
    return actor==0?rowan[beat]:actor==1?bram[beat]:ellis[beat];
}
inline int nearest(const Config &c,Vec player,unsigned excluded=0){
    if(!c.enabled||!valid(player))return -1;
    int target=-1;double best=c.talk_radius;
    for(unsigned i=0;i<3;++i){const auto &a=c.actors[i];const double d=std::hypot(player.x-a.feet.x,player.z-a.feet.z);
        if(!(excluded&(1u<<i))&&std::abs(player.y-a.feet.y)<=1.3&&d<=best){target=int(i);best=d;}}
    return target;
}
// HUD eligibility only. Interaction range and dialogue input remain owned by
// nearest()/State; looking away hides a misleading prompt behind the camera.
inline bool prompt_facing(const Config &c,Vec feet,double yaw,double pitch){
    if(!std::isfinite(yaw)||!std::isfinite(pitch))return false;
    const int actor=nearest(c,feet);if(actor<0)return false;
    const auto &a=c.actors[unsigned(actor)];
    const double dx=a.feet.x-feet.x,dz=a.feet.z-feet.z;
    const double dy=a.feet.y+(a.seated?1.0:1.25)-(feet.y+1.75);
    const double length=std::sqrt(dx*dx+dy*dy+dz*dz);if(length<.001)return false;
    const double cp=std::cos(pitch);
    return (dx*std::sin(yaw)*cp+dy*std::sin(pitch)-dz*std::cos(yaw)*cp)/length>.5;
}
inline int objective_actor(const Config &c,Vec player,unsigned mask){
    int target=-1;double best=1e30;
    for(unsigned i=0;i<3;++i)if(!(mask&(1u<<i))){const double d=std::hypot(player.x-c.actors[i].feet.x,player.z-c.actors[i].feet.z);if(d<best){target=int(i);best=d;}}
    return target;
}
class State {
    unsigned spoken_=0,beat_=0;int actor_=-1;bool repeat_=false;
public:
    unsigned spoken()const{return spoken_;}unsigned beat()const{return beat_;}int actor()const{return actor_;}bool repeat()const{return repeat_;}bool active()const{return actor_>=0;}bool complete()const{return spoken_==complete_mask;}
    bool restore(unsigned mask){if(!valid_mask(mask))return false;spoken_=mask;cancel();return true;}
    void reset(){restore(0);}void cancel(){actor_=-1;beat_=0;repeat_=false;}
    bool begin(unsigned actor){if(actor>=3||active())return false;actor_=int(actor);beat_=0;repeat_=(spoken_&(1u<<actor))!=0;return true;}
    // True only when this interaction closes a newly completed exchange.
    bool advance(){if(!active())return false;if(++beat_<(repeat_?2u:6u))return false;const bool changed=!repeat_;spoken_|=1u<<unsigned(actor_);cancel();return changed;}
    Line current()const{return active()?line(unsigned(actor_),beat_,repeat_):Line{};}
};
}
#endif

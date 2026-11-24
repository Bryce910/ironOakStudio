#ifndef VALWOOD_WORLD_BATTLEFIELD_PROLOGUE_H
#define VALWOOD_WORLD_BATTLEFIELD_PROLOGUE_H
// Deterministic opening encounter. The host owns terrain, collision, animation,
// combat receipts, inventory and audio; this module never fabricates a kill.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <istream>
#include <locale>
#include <sstream>
#include <string>

namespace world_battlefield_prologue {
constexpr unsigned save_version=1;
struct Vec {double x=0,y=0,z=0;};
struct Pose {Vec feet;double yaw=0;};
struct Camera {Vec eye,target;double fov=.82,letterbox=0,fade=0;};
enum class Difficulty : unsigned {Easy,Medium,Hard};
enum class Stage : unsigned {Inactive,Dazed,Recover,FleeToAmbush,Duck,Tackle,Struggle,Stab,Stand,Escape,Forest,Campsite,Dead};
enum class Checkpoint : unsigned {BeforeAmbush,SwordSecured,Forest,Campsite};
enum class DeathReason : unsigned {None,MissedDuck,Overpowered,Combat};
enum Event : std::uint32_t {
    None=0,Started=1u<<0,StageChanged=1u<<1,ReleaseControls=1u<<2,
    TeleportPlayer=1u<<3,AmbushStarted=1u<<4,Ducked=1u<<5,TackleImpact=1u<<6,
    StruggleStarted=1u<<7,StabContact=1u<<8,GrantSword=1u<<9,EnsureSword=1u<<10,
    CheckpointReached=1u<<11,EscapeStarted=1u<<12,ForestReached=1u<<13,
    CampsiteReached=1u<<14,Died=1u<<15,Retried=1u<<16,Restored=1u<<17,
    StrugglePress=1u<<18
};
struct Config {
    bool enabled=true;
    Pose arrival{{-12115,11.32447433,5150},1.373400766945016};
    Pose ambush{{-12095,11.29714489,5154},1.373400766945016};
    Pose before_ambush{{-12103,11.8,5152.4},1.373400766945016};
    // The recovered player stands beside the corpse: -.75m right, 1.46m forward.
    Pose escape_spawn{{-12093.715439314844843,11.29714489,5155.021765064069768},1.373400766945016};
    Pose forest{{-12008,11.6698770523,5167},1.3258176636680326};
    Pose campsite{{-11988,11.80000019,5172},1.201462669121488};
    double dazed_seconds=4.6,recover_seconds=1.85,tackle_seconds=1.05;
    double stab_seconds=1.48,stab_contact=.64,stand_seconds=1.10;
    double duck_telegraph=.35,duck_easy=1.25,duck_medium=.95,duck_hard=.72;
    double struggle_easy=8.5,struggle_medium=7.5,struggle_hard=6.5;
    unsigned presses_easy=9,presses_medium=12,presses_hard=15;
    // Accessibility hold uses the same struggle deadline but no repeated taps.
    double hold_easy=2.2,hold_medium=2.7,hold_hard=3.1;
    double press_interval=.065,ambush_radius=4.5,forest_radius=8.,camp_radius=11.;
    double retry_delay=.8,max_frame_dt=.25;
    unsigned required_escape_wins=3;
};
struct Input {
    double dt=0;Vec player_feet;
    bool paused=false,focused=true,loading=false;
    bool uses_controller=false,controller_connected=true;
    bool duck_down=false,struggle_down=false,retry_down=false;
    // One-update, non-repeat physical DOWN events captured by the host. A
    // complete tap between state polls still reaches this input. Consume and
    // clear even on blocked/inactive frames; never replay across ownership.
    // Each pulse is ORed with its polled edge, not counted a second time.
    bool duck_pressed=false,struggle_pressed=false,retry_pressed=false;
    bool hold_struggle=false,reduced_motion=false;
    bool player_dead=false;
    // Monotonic, actual defeated opponents in this escape encounter only.
    unsigned combat_wins=0;
    bool escape_combat_complete=false;
};
struct Save {
    // Absence preserves old opening saves: restoring it does not replay or
    // teleport an existing camp/village save into the new battlefield.
    bool present=false;unsigned version=save_version;
    Checkpoint checkpoint=Checkpoint::BeforeAmbush;
    Difficulty difficulty=Difficulty::Medium;
    bool sword_owned=false;unsigned combat_wins=0;
};
struct Frame {
    Stage stage=Stage::Inactive;Checkpoint checkpoint=Checkpoint::BeforeAmbush;
    Difficulty difficulty=Difficulty::Medium;DeathReason death_reason=DeathReason::None;
    std::uint32_t events=None;
    bool active=false,controls_enabled=false,exclusive_input=false,cinematic=false;
    bool override_player=false,override_attacker=false,sword_owned=false;
    bool duck_window=false,can_retry=false,completed=false;
    Pose player_pose,attacker_pose,teleport_pose;Camera camera;
    Vec objective_position;
    double elapsed=0,stage_progress=0,duck_remaining=0,struggle_progress=0;
    double disorientation=0,muffle=0,pose_progress=0;
    unsigned struggle_presses=0,struggle_required=0,combat_wins=0,combat_required=0;
    const char *chapter="",*objective="",*prompt="",*keyboard_prompt="",*subtitle="";
};
namespace detail {
inline bool fail(std::string &error,const char *why){error=why;return false;}
inline bool valid(Vec p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<1000000&&std::abs(p.y)<100000&&std::abs(p.z)<1000000;}
inline bool valid(Pose p){return valid(p.feet)&&std::isfinite(p.yaw)&&std::abs(p.yaw)<100000;}
inline double ease(double t){t=std::clamp(t,0.,1.);return t*t*(3.-2.*t);}
inline Vec mix(Vec a,Vec b,double t){return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};}
inline double distance(Vec a,Vec b){return std::hypot(a.x-b.x,a.z-b.z);}
inline bool near(Vec a,Vec b,double radius){return valid(a)&&distance(a,b)<=radius&&std::abs(a.y-b.y)<8.;}
inline Vec offset(Pose p,double right,double up,double forward){return {p.feet.x+std::cos(p.yaw)*right+std::sin(p.yaw)*forward,p.feet.y+up,p.feet.z-std::sin(p.yaw)*right+std::cos(p.yaw)*forward};}
// A stage-scoped non-repeat DOWN proves a new press even if release/repress
// completed between held snapshots. Suspension/entry discard before this call.
inline bool rising(bool down,bool pressed,bool &last,bool &armed){if(!down||pressed)armed=true;const bool result=(pressed||(down&&!last))&&armed;last=down;return result;}
}
inline double duck_seconds(const Config &c,Difficulty d){return d==Difficulty::Easy?c.duck_easy:d==Difficulty::Hard?c.duck_hard:c.duck_medium;}
inline double struggle_seconds(const Config &c,Difficulty d){return d==Difficulty::Easy?c.struggle_easy:d==Difficulty::Hard?c.struggle_hard:c.struggle_medium;}
inline double hold_seconds(const Config &c,Difficulty d){return d==Difficulty::Easy?c.hold_easy:d==Difficulty::Hard?c.hold_hard:c.hold_medium;}
inline unsigned required_presses(const Config &c,Difficulty d){return d==Difficulty::Easy?c.presses_easy:d==Difficulty::Hard?c.presses_hard:c.presses_medium;}
inline const char *stage_name(Stage s){switch(s){
case Stage::Inactive:return "inactive";case Stage::Dazed:return "dazed";case Stage::Recover:return "recover";
case Stage::FleeToAmbush:return "flee_to_ambush";case Stage::Duck:return "duck";case Stage::Tackle:return "tackle";
case Stage::Struggle:return "struggle";case Stage::Stab:return "stab";case Stage::Stand:return "stand";
case Stage::Escape:return "escape";case Stage::Forest:return "forest";case Stage::Campsite:return "campsite";
case Stage::Dead:return "dead";}return "invalid";}
inline bool validate(const Config &c,std::string &error){
    for(Pose p:{c.arrival,c.ambush,c.before_ambush,c.escape_spawn,c.forest,c.campsite})if(!detail::valid(p))return detail::fail(error,"Invalid battlefield pose");
    for(double d:{c.dazed_seconds,c.recover_seconds,c.tackle_seconds,c.stab_seconds,c.stand_seconds})if(!std::isfinite(d)||d<.2||d>15)return detail::fail(error,"Invalid battlefield cinematic duration");
    if(!std::isfinite(c.stab_contact)||c.stab_contact<=.05||c.stab_contact>=c.stab_seconds)return detail::fail(error,"Stab contact must precede recovery");
    for(double d:{c.duck_easy,c.duck_medium,c.duck_hard})if(!std::isfinite(d)||d<.35||d>4)return detail::fail(error,"Invalid duck difficulty window");
    if(c.duck_easy<c.duck_medium||c.duck_medium<c.duck_hard||!std::isfinite(c.duck_telegraph)||c.duck_telegraph<0||c.duck_telegraph>2)return detail::fail(error,"Invalid duck telegraph or difficulty ordering");
    for(double d:{c.struggle_easy,c.struggle_medium,c.struggle_hard})if(!std::isfinite(d)||d<3||d>20)return detail::fail(error,"Invalid struggle deadline");
    for(Difficulty d:{Difficulty::Easy,Difficulty::Medium,Difficulty::Hard}){
        const auto n=required_presses(c,d);const double h=hold_seconds(c,d);
        if(n<3||n>30||!std::isfinite(h)||h<1||h+1>struggle_seconds(c,d)||n*c.press_interval+1>struggle_seconds(c,d))return detail::fail(error,"Unreachable struggle settings");
    }
    if(c.presses_easy>c.presses_medium||c.presses_medium>c.presses_hard||c.struggle_easy<c.struggle_medium||c.struggle_medium<c.struggle_hard)return detail::fail(error,"Invalid struggle difficulty ordering");
    for(double r:{c.ambush_radius,c.forest_radius,c.camp_radius})if(!std::isfinite(r)||r<1||r>25)return detail::fail(error,"Invalid battlefield trigger radius");
    if(!std::isfinite(c.press_interval)||c.press_interval<.03||c.press_interval>.3||!std::isfinite(c.retry_delay)||c.retry_delay<.3||c.retry_delay>4||!std::isfinite(c.max_frame_dt)||c.max_frame_dt<.1||c.max_frame_dt>1)return detail::fail(error,"Invalid battlefield input timing");
    if(c.required_escape_wins<2||c.required_escape_wins>4)return detail::fail(error,"Battlefield escape requires two to four real opponents");
    if(detail::distance(c.escape_spawn.feet,detail::offset(c.ambush,-.75,0,1.46))>.25||std::abs(c.escape_spawn.feet.y-c.ambush.feet.y)>.5)return detail::fail(error,"Battlefield recovery root does not match paired disarm capture");
    if(detail::distance(c.arrival.feet,c.ambush.feet)<8||detail::distance(c.arrival.feet,c.ambush.feet)>100||detail::distance(c.ambush.feet,c.forest.feet)<30||detail::distance(c.ambush.feet,c.forest.feet)>500||detail::distance(c.forest.feet,c.campsite.feet)<15||detail::distance(c.forest.feet,c.campsite.feet)>250)return detail::fail(error,"Battlefield route anchors are out of bounds");
    error.clear();return true;
}
inline bool validate_save(const Save &s,const Config &c,std::string &error){
    if(!validate(c,error))return false;
    if(!s.present){error.clear();return true;}
    if(s.version!=save_version||unsigned(s.checkpoint)>unsigned(Checkpoint::Campsite)||unsigned(s.difficulty)>unsigned(Difficulty::Hard)||s.combat_wins>c.required_escape_wins)return detail::fail(error,"Invalid battlefield checkpoint");
    if(s.checkpoint==Checkpoint::BeforeAmbush&&(s.sword_owned||s.combat_wins))return detail::fail(error,"Sword or combat cannot precede the disarm");
    if(s.checkpoint!=Checkpoint::BeforeAmbush&&!s.sword_owned)return detail::fail(error,"Disarmed sword missing from battlefield checkpoint");
    if(s.checkpoint>=Checkpoint::Forest&&s.combat_wins!=c.required_escape_wins)return detail::fail(error,"Forest checkpoint requires actual completed escape combat");
    error.clear();return true;
}
inline bool encode_save(const Save &s,const Config &c,std::string &out,std::string &error){
    if(!validate_save(s,c,error))return false;
    if(!s.present){out.clear();return true;}
    std::ostringstream stream;stream.imbue(std::locale::classic());
    stream<<"VALWOOD_BATTLEFIELD "<<save_version<<"\ncheckpoint "<<unsigned(s.checkpoint)<<"\ndifficulty "<<unsigned(s.difficulty)<<"\nsword "<<unsigned(s.sword_owned)<<"\nwins "<<s.combat_wins<<'\n';out=stream.str();return true;
}
inline bool parse_save(std::istream &input,const Config &c,Save &out,std::string &error){
    std::string bytes;char ch;while(input.get(ch)){if(ch=='\0'||bytes.size()>=512)return detail::fail(error,"Battlefield checkpoint exceeds bounds");bytes+=ch;}
    if(input.bad()||(input.fail()&&!input.eof()))return detail::fail(error,"Unreadable battlefield checkpoint");
    if(bytes.find_first_not_of(" \n\r\t")==std::string::npos){out=Save{};error.clear();return true;}
    std::istringstream stream(bytes);stream.imbue(std::locale::classic());std::string tag;unsigned cp=0,d=0,sword=0;Save staged;staged.present=true;
    if(!(stream>>tag>>staged.version)||tag!="VALWOOD_BATTLEFIELD"||!(stream>>tag>>cp)||tag!="checkpoint"||!(stream>>tag>>d)||tag!="difficulty"||!(stream>>tag>>sword)||tag!="sword"||sword>1||!(stream>>tag>>staged.combat_wins)||tag!="wins"||(stream>>tag))return detail::fail(error,"Malformed battlefield checkpoint");
    staged.checkpoint=Checkpoint(cp);staged.difficulty=Difficulty(d);staged.sword_owned=sword!=0;
    if(!validate_save(staged,c,error))return false;
    out=staged;return true;
}
inline bool decode_save(const std::string &bytes,const Config &c,Save &out,std::string &error){std::istringstream input(bytes);return parse_save(input,c,out,error);}

class State {
    Config config_;Stage stage_=Stage::Inactive;Checkpoint checkpoint_=Checkpoint::BeforeAmbush;Difficulty difficulty_=Difficulty::Medium;DeathReason death_=DeathReason::None;
    double elapsed_=0,hold_=0,last_press_=-100.;unsigned presses_=0,wins_=0;
    bool sword_=false,contact_=false,tackle_impact_=false,suspended_=true;
    bool duck_last_=false,struggle_last_=false,retry_last_=false,duck_armed_=false,struggle_armed_=false,retry_armed_=false;
    Pose death_pose_;Vec last_feet_;
    void latch(const Input &in){duck_last_=in.duck_down;struggle_last_=in.struggle_down;retry_last_=in.retry_down;duck_armed_=!in.duck_down;struggle_armed_=!in.struggle_down;retry_armed_=!in.retry_down;}
    std::uint32_t enter(Stage next,const Input *in=nullptr){stage_=next;elapsed_=hold_=0;last_press_=-100.;presses_=0;tackle_impact_=false;if(in)latch(*in);else duck_armed_=struggle_armed_=retry_armed_=false;return StageChanged|ReleaseControls;}
    std::uint32_t die(DeathReason why,const Input &in){death_=why;death_pose_={detail::valid(in.player_feet)?in.player_feet:last_feet_,config_.ambush.yaw};return enter(Stage::Dead,&in)|Died;}
    Pose checkpoint_pose()const{switch(checkpoint_){case Checkpoint::BeforeAmbush:return config_.before_ambush;case Checkpoint::SwordSecured:return config_.escape_spawn;case Checkpoint::Forest:return config_.forest;case Checkpoint::Campsite:return config_.campsite;}return config_.before_ambush;}
    std::uint32_t resume_checkpoint(const Input *in=nullptr){
        death_=DeathReason::None;contact_=sword_;const Stage next=checkpoint_==Checkpoint::BeforeAmbush?Stage::FleeToAmbush:checkpoint_==Checkpoint::SwordSecured?Stage::Escape:checkpoint_==Checkpoint::Forest?Stage::Forest:Stage::Campsite;
        // A failed escape restarts all opponents; no resurrected enemy remains
        // counted as dead. Mid-escape serialization is likewise checkpointed.
        if(checkpoint_==Checkpoint::SwordSecured)wins_=0;
        return enter(next,in)|TeleportPlayer|ReleaseControls|(sword_?EnsureSword:0u);
    }
    Frame sample(std::uint32_t events,const Input &in)const{
        Frame f;f.stage=stage_;f.checkpoint=checkpoint_;f.difficulty=difficulty_;f.events=events;f.death_reason=death_;f.elapsed=elapsed_;f.sword_owned=sword_;f.active=stage_!=Stage::Inactive&&stage_!=Stage::Campsite;f.completed=stage_==Stage::Campsite;
        f.controls_enabled=stage_==Stage::FleeToAmbush||stage_==Stage::Escape||stage_==Stage::Forest||stage_==Stage::Campsite;
        f.cinematic=stage_==Stage::Dazed||stage_==Stage::Recover||stage_==Stage::Duck||stage_==Stage::Tackle||stage_==Stage::Struggle||stage_==Stage::Stab||stage_==Stage::Stand||stage_==Stage::Dead;
        f.exclusive_input=f.cinematic;f.override_player=f.cinematic;f.override_attacker=stage_==Stage::Duck||stage_==Stage::Tackle||stage_==Stage::Struggle||stage_==Stage::Stab||stage_==Stage::Stand;
        f.teleport_pose=(events&Started)?config_.arrival:checkpoint_pose();f.player_pose=config_.ambush;f.attacker_pose=config_.ambush;f.attacker_pose.feet=detail::offset(config_.ambush,0,0,1.25);f.attacker_pose.yaw+=3.141592653589793;
        f.struggle_presses=presses_;f.struggle_required=required_presses(config_,difficulty_);f.combat_wins=wins_;f.combat_required=config_.required_escape_wins;
        f.chapter="THE WRONG WORLD";f.objective_position=config_.forest.feet;
        const auto camera=[&](Pose p,double right,double up,double back,double lookUp,double lookForward){f.camera.eye=detail::offset(p,right,up,back);f.camera.target=detail::offset(p,0,lookUp,lookForward);f.camera.letterbox=.10;};
        switch(stage_){
        case Stage::Inactive:f.chapter="";break;
        case Stage::Dazed:{double t=elapsed_/config_.dazed_seconds;f.stage_progress=std::clamp(t,0.,1.);f.pose_progress=f.stage_progress;f.player_pose=config_.arrival;f.disorientation=1.-detail::ease(t);f.muffle=.95-.35*detail::ease(t);f.subtitle="Where... where am I?";f.objective="Get up. Move.";camera(config_.arrival,1.05,.55,-2.6,.65,1.3);f.camera.fade=1.-detail::ease(elapsed_/.65);break;}
        case Stage::Recover:{double t=elapsed_/config_.recover_seconds;f.stage_progress=std::clamp(t,0.,1.);f.pose_progress=f.stage_progress;f.player_pose=config_.arrival;f.muffle=.60*(1.-detail::ease(t));f.disorientation=.25*(1.-detail::ease(t));f.subtitle="Into the trees. Keep moving.";f.objective="Run for the forest.";camera(config_.arrival,.7,1.25+detail::ease(t)*.7,-3.1,1.2,2.5);f.camera.letterbox*=1.-detail::ease(t);break;}
        case Stage::FleeToAmbush:f.objective="Run! Find a way through the battle.";f.objective_position=config_.ambush.feet;break;
        case Stage::Duck:{const double window=duck_seconds(config_,difficulty_);f.stage_progress=std::clamp(elapsed_/(config_.duck_telegraph+window),0.,1.);f.pose_progress=f.stage_progress;f.duck_window=elapsed_>=config_.duck_telegraph;f.duck_remaining=std::max(0.,config_.duck_telegraph+window-elapsed_);f.objective="Duck beneath the blade!";f.prompt=f.duck_window?"X  DUCK":"INCOMING!";f.keyboard_prompt=f.duck_window?"Q  DUCK":"INCOMING!";camera(config_.ambush,2.3,1.35,-.8,1.1,.85);break;}
        case Stage::Tackle:{double t=detail::ease(elapsed_/config_.tackle_seconds);f.stage_progress=std::clamp(elapsed_/config_.tackle_seconds,0.,1.);f.pose_progress=f.stage_progress;f.player_pose.feet=detail::offset(config_.ambush,-.10*t,0,1.46*t);f.attacker_pose.feet=detail::offset(config_.ambush,.08*t,0,1.30+.35*t);f.attacker_pose.yaw=config_.ambush.yaw+3.141592653589793-.95*t;f.objective="Take him down!";camera(config_.ambush,2.8,1.2,.9,.75,1.1);break;}
        case Stage::Struggle:f.player_pose={detail::offset(config_.ambush,-.10,0,1.46),config_.ambush.yaw};f.attacker_pose.feet=detail::offset(config_.ambush,.08,0,1.65);f.attacker_pose.yaw=config_.ambush.yaw+3.141592653589793-.95;f.stage_progress=std::clamp(elapsed_/struggle_seconds(config_,difficulty_),0.,1.);f.struggle_progress=in.hold_struggle?std::clamp(hold_/hold_seconds(config_,difficulty_),0.,1.):std::clamp(double(presses_)/f.struggle_required,0.,1.);f.pose_progress=f.struggle_progress;f.objective="Wrench the sword from his hand!";f.prompt=in.hold_struggle?"HOLD A  TAKE THE SWORD":"TAP A  TAKE THE SWORD";f.keyboard_prompt=in.hold_struggle?"HOLD F  TAKE THE SWORD":"TAP F  TAKE THE SWORD";camera(config_.ambush,2.2,1.15,1.9,.55,1.65);break;
        case Stage::Stab:f.player_pose={detail::offset(config_.ambush,-.10,0,1.46),config_.ambush.yaw};f.attacker_pose.feet=detail::offset(config_.ambush,.08,0,1.65);f.attacker_pose.yaw=config_.ambush.yaw+3.141592653589793-.95;f.stage_progress=std::clamp(elapsed_/config_.stab_seconds,0.,1.);f.pose_progress=f.stage_progress;f.objective="";camera(config_.ambush,2.15,1.15,1.9,.6,1.65);break;
        case Stage::Stand:{double t=detail::ease(elapsed_/config_.stand_seconds);f.stage_progress=std::clamp(elapsed_/config_.stand_seconds,0.,1.);f.pose_progress=f.stage_progress;f.player_pose={detail::mix(detail::offset(config_.ambush,-.10,0,1.46),config_.escape_spawn.feet,t),config_.ambush.yaw};f.attacker_pose.feet=detail::offset(config_.ambush,.08,0,1.65);f.attacker_pose.yaw=config_.ambush.yaw+3.141592653589793-.95;f.objective="Get to the forest.";camera(config_.ambush,.65,1.25+t*.8,-3.1,1.2,2.5);f.camera.letterbox*=1.-t;break;}
        case Stage::Escape:f.chapter="BREAK THROUGH";f.objective=wins_<config_.required_escape_wins?"Fight through the soldiers. Reach the forest.":"Reach the shelter of the forest.";break;
        case Stage::Forest:f.chapter="A LIGHT BETWEEN THE TREES";f.objective="Follow the trail toward the campfire.";f.objective_position=config_.campsite.feet;break;
        case Stage::Campsite:f.chapter="SMOKE IN THE MEADOWS";f.objective="Speak to the travelers by the fire.";f.objective_position=config_.campsite.feet;break;
        case Stage::Dead:f.override_player=false;f.can_retry=elapsed_>=config_.retry_delay;f.objective=death_==DeathReason::MissedDuck?"The blade found you.":death_==DeathReason::Overpowered?"You lost the struggle.":"You fell before reaching safety.";f.prompt=f.can_retry?"A  RETRY":"";f.keyboard_prompt=f.can_retry?"ENTER  RETRY":"";camera(death_pose_,2.,2.1,-3.,.6,.7);f.camera.fade=.55*detail::ease(elapsed_/1.1);break;
        }
        if(in.reduced_motion){f.disorientation=0;f.camera.fov=.82;}
        return f;
    }
public:
    bool configure(const Config &c,std::string &error){if(!validate(c,error))return false;config_=c;reset();return true;}
    void reset(){stage_=Stage::Inactive;checkpoint_=Checkpoint::BeforeAmbush;difficulty_=Difficulty::Medium;death_=DeathReason::None;elapsed_=hold_=0;presses_=wins_=0;sword_=contact_=tackle_impact_=false;suspended_=true;duck_last_=struggle_last_=retry_last_=false;duck_armed_=struggle_armed_=retry_armed_=false;last_feet_=config_.arrival.feet;}
    const Config &config()const{return config_;}Stage stage()const{return stage_;}Checkpoint checkpoint()const{return checkpoint_;}bool sword_owned()const{return sword_;}double elapsed()const{return elapsed_;}
    Frame frame(const Input &in=Input{})const{return sample(None,in);}
    Frame start(Difficulty d=Difficulty::Medium){reset();if(!config_.enabled)return frame();difficulty_=unsigned(d)<=unsigned(Difficulty::Hard)?d:Difficulty::Medium;const auto e=enter(Stage::Dazed)|Started|TeleportPlayer|CheckpointReached;return sample(e,Input{});}
    Save save()const{Save s;if(stage_==Stage::Inactive)return s;s.present=true;s.checkpoint=checkpoint_;s.difficulty=difficulty_;s.sword_owned=sword_;s.combat_wins=checkpoint_>=Checkpoint::Forest?config_.required_escape_wins:0;return s;}
    bool restore(const Save &s,Frame &out,std::string &error){if(!validate_save(s,config_,error))return false;reset();if(!s.present){out=frame();return true;}checkpoint_=s.checkpoint;difficulty_=s.difficulty;sword_=s.sword_owned;wins_=s.combat_wins;out=sample(resume_checkpoint()|Restored,Input{});return true;}
    Frame update(const Input &in){
        if(stage_==Stage::Inactive||stage_==Stage::Campsite)return sample(None,in);
        const bool blocked=in.paused||!in.focused||in.loading||(in.uses_controller&&!in.controller_connected);
        if(blocked){suspended_=true;latch(in);return sample(None,in);}
        if(suspended_){suspended_=false;latch(in);return sample(None,in);}
        if(detail::valid(in.player_feet))last_feet_=in.player_feet;
        const bool duck=detail::rising(in.duck_down,in.duck_pressed,duck_last_,duck_armed_);
        const bool struggle=detail::rising(in.struggle_down,in.struggle_pressed,struggle_last_,struggle_armed_);
        const bool retry=detail::rising(in.retry_down,in.retry_pressed,retry_last_,retry_armed_);
        const double dt=std::isfinite(in.dt)?std::clamp(in.dt,0.,config_.max_frame_dt):0.;const double previous=elapsed_;elapsed_+=dt;std::uint32_t events=None;
        if(in.player_dead&&stage_!=Stage::Dead){events|=die(DeathReason::Combat,in);return sample(events,in);}
        switch(stage_){
        case Stage::Dazed:if(elapsed_>=config_.dazed_seconds)events|=enter(Stage::Recover,&in);break;
        case Stage::Recover:if(elapsed_>=config_.recover_seconds)events|=enter(Stage::FleeToAmbush,&in);break;
        case Stage::FleeToAmbush:if(detail::near(in.player_feet,config_.ambush.feet,config_.ambush_radius))events|=enter(Stage::Duck,&in)|AmbushStarted;break;
        case Stage::Duck:
            if(duck&&elapsed_>=config_.duck_telegraph&&previous<config_.duck_telegraph+duck_seconds(config_,difficulty_))events|=enter(Stage::Tackle,&in)|Ducked;
            else if(elapsed_>=config_.duck_telegraph+duck_seconds(config_,difficulty_))events|=die(DeathReason::MissedDuck,in);
            break;
        case Stage::Tackle:
            if(!tackle_impact_&&elapsed_>=config_.tackle_seconds*.44){tackle_impact_=true;events|=TackleImpact;}
            if(elapsed_>=config_.tackle_seconds)events|=enter(Stage::Struggle,&in)|StruggleStarted;
            break;
        case Stage::Struggle:
            if(in.hold_struggle){if(in.struggle_down&&struggle_armed_)hold_+=dt;else if(!in.struggle_down)hold_=std::max(0.,hold_-dt*.65);}
            else{hold_=0;if(struggle&&elapsed_-last_press_>=config_.press_interval&&previous<struggle_seconds(config_,difficulty_)){last_press_=elapsed_;++presses_;events|=StrugglePress;}}
            if((in.hold_struggle&&hold_>=hold_seconds(config_,difficulty_))||(!in.hold_struggle&&presses_>=required_presses(config_,difficulty_)))events|=enter(Stage::Stab,&in);
            else if(elapsed_>=struggle_seconds(config_,difficulty_))events|=die(DeathReason::Overpowered,in);
            break;
        case Stage::Stab:
            if(!contact_&&elapsed_>=config_.stab_contact){contact_=true;sword_=true;checkpoint_=Checkpoint::SwordSecured;events|=StabContact|GrantSword|CheckpointReached;}
            if(elapsed_>=config_.stab_seconds)events|=enter(Stage::Stand,&in);
            break;
        case Stage::Stand:if(elapsed_>=config_.stand_seconds)events|=enter(Stage::Escape,&in)|EscapeStarted|EnsureSword;break;
        case Stage::Escape:
            wins_=std::max(wins_,std::min(config_.required_escape_wins,in.combat_wins));
            if(wins_>=config_.required_escape_wins&&in.escape_combat_complete&&detail::near(in.player_feet,config_.forest.feet,config_.forest_radius)){checkpoint_=Checkpoint::Forest;events|=enter(Stage::Forest,&in)|ForestReached|CheckpointReached;}break;
        case Stage::Forest:if(detail::near(in.player_feet,config_.campsite.feet,config_.camp_radius)){checkpoint_=Checkpoint::Campsite;events|=enter(Stage::Campsite,&in)|CampsiteReached|CheckpointReached;}break;
        case Stage::Dead:if(retry&&previous>=config_.retry_delay)events|=resume_checkpoint(&in)|Retried;break;
        case Stage::Inactive:case Stage::Campsite:break;
        }
        return sample(events,in);
    }
};
} // namespace world_battlefield_prologue
#endif

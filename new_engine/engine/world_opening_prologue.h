#ifndef VALWOOD_WORLD_OPENING_PROLOGUE_H
#define VALWOOD_WORLD_OPENING_PROLOGUE_H
// The opening's deterministic story clock. This header does not move the host
// player, read devices, touch gameplay systems, or perform filesystem IO.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <initializer_list>
#include <istream>
#include <locale>
#include <sstream>
#include <string>
#include <vector>
#include "world_opening_room_lights.h"
#include "world_opening_arrival_camp.h"
#include "world_first_journey.h"

namespace world_opening_prologue {
constexpr unsigned story_version=1;
constexpr double eye_height=1.75;
struct Vec {double x=0,y=0,z=0;};
struct Pose {Vec feet;double yaw=0,pitch=0;};
struct Camera {Vec eye,target;double fov=.9,fade=0,letterbox=0;};
struct Rail {Vec eye_from,eye_to,target_from,target_to;double fov_from=.9,fov_to=.9;};
struct HouseShot {Rail rail;double duration=3.;std::string subtitle;};
struct Config {
    Pose house_spawn{{0,0,0},0,0},meadow_spawn{{100,0,0},0,0};
    Vec house_focus{2,0,0},house_exit{0,0,4},meadow_goal{100,0,18};
    Rail house_rail{{-2,2,-3},{0,1.75,0},{1,1,2},{0,1.75,4},.9,.9};
    Rail transport_rail{{0,1.75,2},{0,1.75,3},{0,1.75,4},{0,1.75,4},.9,.8};
    Rail meadow_rail{{100,5,-5},{100,1.75,0},{100,1,18},{100,1.75,18},.9,.9};
    double house_duration=6.5,transport_duration=4.8,meadow_duration=7.;
    double skip_seconds=.8,house_radius=2.,exit_radius=1.7,goal_radius=2.5;
    // Opt in per project. Existing authored openings keep the front-door flow
    // and the same version-one checkpoint meanings and numeric stage values.
    bool game_start_story=false;
    world_opening_arrival_camp::Config arrival_camp;
    bool first_journey_enabled=false;
    world_first_journey::Config first_journey;
    // Empty keeps the original single house rail and duration. A project may
    // opt in to a short authored sequence without changing checkpoint meaning.
    std::vector<HouseShot> house_shots;
};
enum class Stage : unsigned {Title,HouseEstablish,HouseExplore,Transport,MeadowReveal,MeadowExplore,Complete,GameMenu};
enum class Checkpoint : unsigned {HouseStart,HouseInspected,MeadowStart,Complete};
enum Event : std::uint32_t {
    None=0,Started=1u<<0,StageChanged=1u<<1,Inspected=1u<<2,
    TransportStarted=1u<<3,TransportPulse=1u<<4,Arrived=1u<<5,
    CheckpointReached=1u<<6,Completed=1u<<7,Skipped=1u<<8,
    ReleaseControls=1u<<9,GameOpened=1u<<10
};
struct Save {unsigned version=story_version;Checkpoint checkpoint=Checkpoint::HouseStart;Pose player;unsigned room_lights=world_opening_room_lights::all_on;unsigned camp_spoken=0;bool first_journey_present=false;world_first_journey::Save first_journey;};
struct Input {
    double dt=0;Vec player_feet;
    bool paused=false,confirm_down=false,interact_down=false,reduced_motion=false;
    bool cancel_down=false;
};
struct Frame {
    Stage stage=Stage::Title;std::uint32_t events=None;Camera camera;
    bool cinematic=false,controls_enabled=false,release_controls=false;
    bool interaction_available=false;Vec objective_position;
    const char *chapter="",*objective="",*interaction="",*subtitle="";
    double skip_progress=0,stage_progress=0;unsigned objectives_complete=0;
    unsigned house_shot_index=0;double house_shot_progress=0;
    bool camp_dialogue=false;int camp_actor=-1;unsigned camp_spoken=0,camp_beat=0;
    const char *camp_speaker="",*camp_line="";
};
struct Stats {
    unsigned stages_entered=0,skips=0,house_interactions=0,meadow_interactions=0,checkpoints=0;
    double active_seconds=0,playable_seconds=0;
};
namespace detail {
inline bool valid(Vec v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::abs(v.x)<=1000000.&&std::abs(v.y)<=100000.&&std::abs(v.z)<=1000000.;}
inline bool valid(Pose p){return valid(p.feet)&&std::isfinite(p.yaw)&&std::abs(p.yaw)<=100000.&&std::isfinite(p.pitch)&&std::abs(p.pitch)<=1.5707963267948966;}
inline bool fail(std::string &error,const char *message){error=message;return false;}
inline double ease(double t){t=std::clamp(t,0.,1.);return std::clamp(t*t*t*(t*(t*6.-15.)+10.),0.,1.);}
inline Vec mix(Vec a,Vec b,double t){return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};}
inline bool near(Vec a,Vec b,double radius){return valid(a)&&std::hypot(a.x-b.x,a.z-b.z)<=radius&&std::abs(a.y-b.y)<=1.3;}
inline bool same(Pose a,Pose b){return std::abs(a.feet.x-b.feet.x)<=.000001&&std::abs(a.feet.y-b.feet.y)<=.000001&&std::abs(a.feet.z-b.feet.z)<=.000001&&std::abs(a.yaw-b.yaw)<=.000001&&std::abs(a.pitch-b.pitch)<=.000001;}
inline bool valid(Rail r){
    if(!valid(r.eye_from)||!valid(r.eye_to)||!valid(r.target_from)||!valid(r.target_to)||!std::isfinite(r.fov_from)||!std::isfinite(r.fov_to)||r.fov_from<.35||r.fov_from>1.5||r.fov_to<.35||r.fov_to>1.5)return false;
    const Vec a{r.target_from.x-r.eye_from.x,r.target_from.y-r.eye_from.y,r.target_from.z-r.eye_from.z};
    const Vec b{r.target_to.x-r.eye_to.x-a.x,r.target_to.y-r.eye_to.y-a.y,r.target_to.z-r.eye_to.z-a.z};
    const double length=b.x*b.x+b.y*b.y+b.z*b.z;
    const double t=length>0?std::clamp(-(a.x*b.x+a.y*b.y+a.z*b.z)/length,0.,1.):0.;
    const Vec nearest{a.x+b.x*t,a.y+b.y*t,a.z+b.z*t};
    // A rail whose look vector crosses zero would make yaw/pitch flip even
    // though both endpoint compositions looked valid.
    return nearest.x*nearest.x+nearest.y*nearest.y+nearest.z*nearest.z>=.0001&&std::hypot(std::hypot(r.eye_to.x-r.eye_from.x,r.eye_to.z-r.eye_from.z),r.eye_to.y-r.eye_from.y)<=250.;
}
inline Camera rail(Rail r,double progress,bool reduced){
    // Reduced motion uses one stable composition. No orbit, bob, shake or FOV pull.
    const double t=reduced?1.:ease(progress);
    return {mix(r.eye_from,r.eye_to,t),mix(r.target_from,r.target_to,t),r.fov_from+(r.fov_to-r.fov_from)*t,0,.10};
}
}
inline double house_duration(const Config &c){
    if(c.house_shots.empty())return c.house_duration;
    double total=0;for(const auto &shot:c.house_shots)total+=shot.duration;return total;
}
struct HouseCinematicFrame {
    Camera camera;unsigned shot_index=0;double shot_progress=0;const char *subtitle="";
};
// Config must first pass validate(). The final endpoint is deliberately held
// with no fade-out: authors can match it to the supported gameplay spawn pose.
inline HouseCinematicFrame sample_house_cinematic(const Config &c,double elapsed,bool reduced_motion=false){
    HouseCinematicFrame f;
    if(!std::isfinite(elapsed)||elapsed<0)elapsed=0;
    if(c.house_shots.empty()){
        f.shot_progress=std::clamp(elapsed/c.house_duration,0.,1.);
        f.camera=detail::rail(c.house_rail,f.shot_progress,reduced_motion);
        f.camera.fade=1.-detail::ease(elapsed/.8);return f;
    }
    double local=std::min(elapsed,house_duration(c));
    while(f.shot_index+1<c.house_shots.size()&&local>=c.house_shots[f.shot_index].duration){
        local-=c.house_shots[f.shot_index].duration;++f.shot_index;
    }
    const auto &shot=c.house_shots[f.shot_index];
    f.shot_progress=std::clamp(local/shot.duration,0.,1.);
    f.camera=detail::rail(shot.rail,f.shot_progress,reduced_motion);f.subtitle=shot.subtitle.c_str();
    // Cuts occur entirely behind black. Only the opening receives the slower
    // cold-open fade; following shots use a brief neutral fade on either side.
    const double fade_in=f.shot_index==0?.8:.14;
    f.camera.fade=1.-detail::ease(local/fade_in);
    if(f.shot_index+1<c.house_shots.size())f.camera.fade=std::max(f.camera.fade,1.-detail::ease((shot.duration-local)/.14));
    else f.camera.letterbox*=detail::ease((shot.duration-local)/.8);
    return f;
}
inline const char *stage_name(Stage s){switch(s){
    case Stage::Title:return "title";case Stage::HouseEstablish:return "house_establish";
    case Stage::HouseExplore:return "house_explore";case Stage::Transport:return "transport";
    case Stage::MeadowReveal:return "meadow_reveal";case Stage::MeadowExplore:return "meadow_explore";
    case Stage::Complete:return "complete";case Stage::GameMenu:return "game_menu";}return "invalid";}
inline bool validate(const Config &c,std::string &error){
    if(c.first_journey_enabled&&!world_first_journey::validate(c.first_journey,error))return false;
    if(c.first_journey_enabled&&!c.arrival_camp.enabled)return detail::fail(error,"Connected opening requires the three arrival travelers");
    if(!world_opening_arrival_camp::validate(c.arrival_camp,error))return false;
    if(c.arrival_camp.enabled&&std::hypot(c.meadow_spawn.feet.x-c.arrival_camp.center.x,c.meadow_spawn.feet.z-c.arrival_camp.center.z)>25.)return detail::fail(error,"Opening spawn is too far from the arrival camp");
    if(!detail::valid(c.house_spawn)||!detail::valid(c.meadow_spawn)||!detail::valid(c.house_focus)||!detail::valid(c.house_exit)||!detail::valid(c.meadow_goal)||!detail::valid(c.house_rail)||!detail::valid(c.transport_rail)||!detail::valid(c.meadow_rail))return detail::fail(error,"Invalid opening positions or camera rail");
    for(double d:{c.house_duration,c.transport_duration,c.meadow_duration})if(!std::isfinite(d)||d<.5||d>60.)return detail::fail(error,"Invalid opening cinematic duration");
    for(double r:{c.house_radius,c.exit_radius,c.goal_radius})if(!std::isfinite(r)||r<.5||r>5.)return detail::fail(error,"Invalid opening interaction radius");
    if(!std::isfinite(c.skip_seconds)||c.skip_seconds<.3||c.skip_seconds>3.)return detail::fail(error,"Invalid opening skip duration");
    if(c.house_shots.size()>6)return detail::fail(error,"Too many apartment cinematic shots");
    for(const auto &shot:c.house_shots){
        if(!detail::valid(shot.rail)||!std::isfinite(shot.duration)||shot.duration<1.||shot.duration>12.)return detail::fail(error,"Invalid apartment cinematic shot");
        if(shot.subtitle.size()>240)return detail::fail(error,"Apartment cinematic subtitle exceeds bounds");
        for(unsigned char ch:shot.subtitle)if(ch<32||ch==127)return detail::fail(error,"Invalid apartment cinematic subtitle");
    }
    if(house_duration(c)>45.&&!c.house_shots.empty())return detail::fail(error,"Apartment cinematic sequence exceeds duration bounds");
    if(std::hypot(c.house_spawn.feet.x-c.house_focus.x,c.house_spawn.feet.z-c.house_focus.z)>100.||std::hypot(c.house_spawn.feet.x-c.house_exit.x,c.house_spawn.feet.z-c.house_exit.z)>100.||std::hypot(c.meadow_spawn.feet.x-c.meadow_goal.x,c.meadow_spawn.feet.z-c.meadow_goal.z)>500.)return detail::fail(error,"Opening objective is outside its local area");
    error.clear();return true;
}
inline Save checkpoint_save(Checkpoint checkpoint,const Config &c){
    Save s;s.checkpoint=checkpoint;s.player=(checkpoint==Checkpoint::HouseStart||checkpoint==Checkpoint::HouseInspected)?c.house_spawn:c.meadow_spawn;return s;
}
inline bool validate_save(const Save &s,const Config &c,std::string &error){
    if(!validate(c,error))return false;
    if(s.version!=story_version||unsigned(s.checkpoint)>unsigned(Checkpoint::Complete)||!detail::valid(s.player)||!world_opening_room_lights::valid_mask(s.room_lights)||!world_opening_arrival_camp::valid_mask(s.camp_spoken))return detail::fail(error,"Invalid or incompatible opening checkpoint");
    // Checkpoints are supported, authored spawn poses, not arbitrary camera or
    // cinematic positions. A forged/stale position cannot respawn inside a wall.
    if(!detail::same(s.player,checkpoint_save(s.checkpoint,c).player))return detail::fail(error,"Opening checkpoint position does not match this story layout");
    if(s.first_journey_present&&(!c.first_journey_enabled||!world_first_journey::validate_save(s.first_journey,c.first_journey,error)||s.first_journey.camp_spoken!=s.camp_spoken))return detail::fail(error,"Opening and first-journey progress do not match");
    if(s.first_journey_present&&s.checkpoint<Checkpoint::MeadowStart&&s.first_journey.stage!=world_first_journey::Stage::Travelers)return detail::fail(error,"World quest progress cannot precede arrival");
    error.clear();return true;
}
inline bool encode_save(const Save &s,const Config &c,std::string &out,std::string &error){
    if(!validate_save(s,c,error))return false;
    std::ostringstream stream;stream.imbue(std::locale::classic());stream<<std::setprecision(17);
    stream<<"VALWOOD_OPENING "<<story_version<<"\ncheckpoint "<<unsigned(s.checkpoint)<<"\nplayer "<<s.player.feet.x<<' '<<s.player.feet.y<<' '<<s.player.feet.z<<' '<<s.player.yaw<<' '<<s.player.pitch<<"\nroom_lights "<<s.room_lights<<'\n';
    if(c.arrival_camp.enabled)stream<<"camp_spoken "<<s.camp_spoken<<'\n';
    if(s.first_journey_present){std::string journey;if(!world_first_journey::encode_save(s.first_journey,c.first_journey,journey,error))return false;stream<<"first_journey "<<std::quoted(journey)<<'\n';}
    out=stream.str();error.clear();return true;
}
inline bool parse_save(std::istream &input,const Config &c,Save &out,std::string &error){
    std::string bytes;char ch;while(input.get(ch)){if(ch=='\0'||bytes.size()>=1024)return detail::fail(error,"Opening checkpoint exceeds bounds");bytes+=ch;}
    if(input.bad()||(input.fail()&&!input.eof()))return detail::fail(error,"Unreadable opening checkpoint");
    std::istringstream row(bytes);row.imbue(std::locale::classic());Save staged;std::string tag;unsigned checkpoint=0;
    if(!(row>>tag>>staged.version)||tag!="VALWOOD_OPENING"||!(row>>tag>>checkpoint)||tag!="checkpoint"||checkpoint>unsigned(Checkpoint::Complete)||!(row>>tag>>staged.player.feet.x>>staged.player.feet.y>>staged.player.feet.z>>staged.player.yaw>>staged.player.pitch)||tag!="player")return detail::fail(error,"Malformed opening checkpoint");
    row>>std::ws;
    // Existing version-one checkpoints have no electrical state and open with
    // all room lights on. New checkpoints may have exactly one bounded mask.
    if(!row.eof()){
        if(!(row>>tag>>staged.room_lights)||tag!="room_lights")return detail::fail(error,"Malformed apartment light state");
        row>>std::ws;
        // First-contact progress is an optional version-one extension. Old
        // saves open with no conversations completed; partial dialogue is not
        // serialized and is always safely restarted after load/retry.
        if(!row.eof()){
            if(!(row>>tag>>staged.camp_spoken)||tag!="camp_spoken"||!world_opening_arrival_camp::valid_mask(staged.camp_spoken))return detail::fail(error,"Malformed arrival camp progress");
            row>>std::ws;
            if(!row.eof()){
                std::string journey;
                if(!(row>>tag>>std::quoted(journey))||tag!="first_journey"||!c.first_journey_enabled||journey.size()>2048)return detail::fail(error,"Malformed first-journey checkpoint");
                std::istringstream nested(journey);
                if(!world_first_journey::parse_save(nested,c.first_journey,staged.first_journey,error))return false;
                staged.first_journey_present=true;
                row>>std::ws;if(!row.eof())return detail::fail(error,"Extra opening checkpoint records");
            }
        }
    }
    staged.checkpoint=Checkpoint(checkpoint);if(!validate_save(staged,c,error))return false;
    out=staged;error.clear();return true;
}
class State {
    Config config_;Stage stage_=Stage::Title;Checkpoint checkpoint_=Checkpoint::HouseStart;
    double elapsed_=0,skip_hold_=0;bool inspected_=false,skip_armed_=false,interact_armed_=false;
    bool confirm_last_=false,interact_last_=false,cancel_last_=false,paused_last_=false,pulse_=false,menu_armed_=false;
    Stats stats_;
    unsigned room_lights_=world_opening_room_lights::all_on;
    world_opening_arrival_camp::State camp_;
    double duration()const{return stage_==Stage::HouseEstablish?house_duration(config_):stage_==Stage::Transport?config_.transport_duration:stage_==Stage::MeadowReveal?config_.meadow_duration:0;}
    bool cinematic()const{return stage_==Stage::HouseEstablish||stage_==Stage::Transport||stage_==Stage::MeadowReveal;}
    std::uint32_t enter(Stage next){
        stage_=next;elapsed_=skip_hold_=0;skip_armed_=interact_armed_=menu_armed_=false;pulse_=false;++stats_.stages_entered;
        std::uint32_t events=StageChanged|ReleaseControls;
        if(next==Stage::HouseExplore){checkpoint_=inspected_?Checkpoint::HouseInspected:Checkpoint::HouseStart;events|=CheckpointReached;++stats_.checkpoints;}
        if(next==Stage::Transport)events|=TransportStarted;
        if(next==Stage::GameMenu)events|=GameOpened;
        if(next==Stage::MeadowReveal)events|=Arrived;
        if(next==Stage::MeadowExplore){checkpoint_=Checkpoint::MeadowStart;events|=CheckpointReached;++stats_.checkpoints;}
        if(next==Stage::Complete){checkpoint_=Checkpoint::Complete;events|=CheckpointReached|Completed;++stats_.checkpoints;}
        return events;
    }
public:
    State()=default;
    bool configure(const Config &c,std::string &error){if(!validate(c,error))return false;config_=c;reset();return true;}
    const Config &config()const{return config_;}
    Stage stage()const{return stage_;}double elapsed()const{return elapsed_;}
    bool inspected()const{return inspected_;}Checkpoint checkpoint()const{return checkpoint_;}
    const Stats &stats()const{return stats_;}
    const world_opening_arrival_camp::State &camp()const{return camp_;}
    unsigned room_lights()const{return room_lights_;}
    bool set_room_lights(unsigned mask){if(!world_opening_room_lights::valid_mask(mask))return false;room_lights_=mask;return true;}
    void reset(){stage_=Stage::Title;checkpoint_=Checkpoint::HouseStart;elapsed_=skip_hold_=0;inspected_=skip_armed_=interact_armed_=confirm_last_=interact_last_=cancel_last_=paused_last_=pulse_=menu_armed_=false;stats_={};room_lights_=world_opening_room_lights::all_on;camp_.reset();}
    Save stable_save()const{auto s=checkpoint_save(checkpoint_,config_);s.room_lights=room_lights_;s.camp_spoken=camp_.spoken();return s;}
    bool restore(const Save &saved,std::string &error){
        if(!validate_save(saved,config_,error))return false;
        checkpoint_=saved.checkpoint;inspected_=checkpoint_!=Checkpoint::HouseStart;room_lights_=saved.room_lights;camp_.restore(saved.camp_spoken);
        if(config_.arrival_camp.enabled&&checkpoint_==Checkpoint::Complete&&!camp_.complete())checkpoint_=Checkpoint::MeadowStart;
        stage_=(checkpoint_==Checkpoint::HouseStart||checkpoint_==Checkpoint::HouseInspected)?Stage::HouseExplore:checkpoint_==Checkpoint::MeadowStart?Stage::MeadowExplore:Stage::Complete;
        elapsed_=skip_hold_=0;skip_armed_=interact_armed_=menu_armed_=false;confirm_last_=interact_last_=cancel_last_=paused_last_=pulse_=false;
        error.clear();return true;
    }
    // Retry/reload never resumes an animation halfway through transport.
    void retry(){std::string ignored;restore(stable_save(),ignored);}
    Frame sample(Vec player_feet={},bool reduced_motion=false)const{
        Frame f;f.stage=stage_;f.cinematic=cinematic();f.controls_enabled=stage_==Stage::HouseExplore||stage_==Stage::MeadowExplore||stage_==Stage::Complete;
        const double d=duration();f.stage_progress=d>0?std::clamp(elapsed_/d,0.,1.):0;f.skip_progress=std::clamp(skip_hold_/config_.skip_seconds,0.,1.);
        if(stage_==Stage::HouseEstablish){const auto shot=sample_house_cinematic(config_,elapsed_,reduced_motion);f.camera=shot.camera;f.house_shot_index=shot.shot_index;f.house_shot_progress=shot.shot_progress;if(!config_.house_shots.empty())f.subtitle=shot.subtitle;}
        else if(stage_==Stage::Title){f.camera=detail::rail(config_.house_shots.empty()?config_.house_rail:config_.house_shots.front().rail,0,reduced_motion);}
        else if(stage_==Stage::GameMenu){f.camera=detail::rail(config_.transport_rail,0,false);}
        else if(stage_==Stage::Transport){f.camera=detail::rail(config_.transport_rail,f.stage_progress,reduced_motion);f.camera.fade=detail::ease(f.stage_progress/.32);}
        else if(stage_==Stage::MeadowReveal){f.camera=detail::rail(config_.meadow_rail,f.stage_progress,reduced_motion);f.camera.fade=1.-detail::ease(elapsed_/1.3);}
        else{const Pose p=(stage_==Stage::HouseExplore)?config_.house_spawn:config_.meadow_spawn;f.camera.eye={p.feet.x,p.feet.y+eye_height,p.feet.z};f.camera.target={p.feet.x+std::sin(p.yaw),p.feet.y+eye_height+std::tan(p.pitch),p.feet.z-std::cos(p.yaw)};}
        f.objectives_complete=(inspected_?1u:0u)+((stage_==Stage::MeadowReveal||stage_==Stage::MeadowExplore||stage_==Stage::Complete)?1u:0u)+(stage_==Stage::Complete?1u:0u);
        if(stage_==Stage::Title){f.chapter="VALOIS";f.objective="Begin the journey.";}
        else if(stage_==Stage::HouseEstablish||stage_==Stage::HouseExplore){
            f.chapter=config_.game_start_story?"AN ORDINARY EVENING":"AN ORDINARY MORNING";
            f.objective=config_.game_start_story?(inspected_?"Play a video game: turn on the console by the TV.":"Read the bills, then play a video game on the TV."):(inspected_?"Open the front door.":"Take a look around. Examine the table.");
            f.objective_position=inspected_?config_.house_exit:config_.house_focus;
            f.interaction=config_.game_start_story?(inspected_?"Turn on video game console":"Read the bills"):(inspected_?"Open the front door":"Examine the table");
            f.interaction_available=stage_==Stage::HouseExplore&&detail::near(player_feet,f.objective_position,inspected_?config_.exit_radius:config_.house_radius);
            if(config_.game_start_story&&stage_==Stage::HouseEstablish&&config_.house_shots.empty())f.subtitle=elapsed_<3.2?"Overtime again. And somehow I'm still behind.":"Just one quiet night. That's all I need.";
        }else if(stage_==Stage::GameMenu){f.chapter="VALOIS";f.objective="Select NEW GAME on the TV to begin playing.";f.interaction="NEW GAME";f.subtitle="For a little while, I could be someone else.";}
        else if(stage_==Stage::Transport){f.chapter=config_.game_start_story?"A NEW GAME":"SOMETHING CHANGES";f.objective="";if(config_.game_start_story)f.subtitle=elapsed_<1.6?"Wait... that's not right.":"";}
        else if(stage_==Stage::MeadowReveal||stage_==Stage::MeadowExplore){
            f.chapter="THE MEADOWS OF VALOIS";f.objective="Follow the path to the waystone.";f.objective_position=config_.meadow_goal;f.interaction="Examine the waystone";
            f.interaction_available=stage_==Stage::MeadowExplore&&detail::near(player_feet,config_.meadow_goal,config_.goal_radius);
            if(config_.arrival_camp.enabled){
                f.chapter="SMOKE IN THE MEADOWS";f.objective=world_opening_arrival_camp::objective(camp_.spoken());
                f.interaction_available=false;
                if(!camp_.complete()){
                    const int next=world_opening_arrival_camp::objective_actor(config_.arrival_camp,{player_feet.x,player_feet.y,player_feet.z},camp_.spoken());
                    if(next>=0){const auto &p=config_.arrival_camp.actors[unsigned(next)].feet;f.objective_position={p.x,p.y,p.z};}
                }else f.interaction_available=stage_==Stage::MeadowExplore&&detail::near(player_feet,config_.meadow_goal,config_.goal_radius);
            }
            if(config_.game_start_story&&stage_==Stage::MeadowReveal)f.subtitle=config_.arrival_camp.enabled?(elapsed_<3.?"Where... where am I?":"People. A fire. Maybe they can help."):(elapsed_<3.?"Where... where am I?":"This isn't my apartment.");
        }else{f.chapter="A FIRST STEP";f.objective="The waystone marks a road ahead. Explore the Meadows of Valois.";f.objective_position=config_.meadow_goal;}
        if(config_.arrival_camp.enabled&&(stage_==Stage::MeadowExplore||stage_==Stage::Complete)){
            f.camp_spoken=camp_.spoken();f.camp_dialogue=camp_.active();f.camp_actor=camp_.actor();f.camp_beat=camp_.beat();
            const int target=world_opening_arrival_camp::nearest(config_.arrival_camp,{player_feet.x,player_feet.y,player_feet.z});
            if(target>=0&&!camp_.active()){f.interaction_available=true;f.interaction=world_opening_arrival_camp::interaction(unsigned(target));}
            if(camp_.active()){
                const auto line=camp_.current();f.camp_speaker=line.speaker;f.camp_line=line.text;f.controls_enabled=false;f.interaction_available=false;
                const auto &actor=config_.arrival_camp.actors[unsigned(camp_.actor())];
                const double dx=player_feet.x-actor.feet.x,dz=player_feet.z-actor.feet.z,distance=std::hypot(dx,dz);
                const double nx=distance>.1?dx/distance:std::sin(actor.yaw),nz=distance>.1?dz/distance:-std::cos(actor.yaw);
                f.camera.eye={actor.feet.x+nx*2.5,actor.feet.y+(actor.seated?1.22:1.60),actor.feet.z+nz*2.5};
                f.camera.target={actor.feet.x,actor.feet.y+(actor.seated?1.00:1.32),actor.feet.z};f.camera.fov=.88;f.camera.letterbox=.045;
            }
        }
        return f;
    }
    Frame tick(const Input &in){
        const bool confirm_pressed=in.confirm_down&&!confirm_last_;
        const bool interact_pressed=in.interact_down&&!interact_last_;
        const bool cancel_pressed=in.cancel_down&&!cancel_last_;
        confirm_last_=in.confirm_down;interact_last_=in.interact_down;cancel_last_=in.cancel_down;
        if(in.paused){paused_last_=true;auto f=sample(in.player_feet,in.reduced_motion);f.controls_enabled=false;return f;}
        std::uint32_t events=None;
        if(paused_last_){paused_last_=false;skip_armed_=interact_armed_=menu_armed_=false;skip_hold_=0;events|=ReleaseControls;}
        // A debugger/loading stall never consumes several unseen cutscenes or
        // builds a complete skip hold. Invalid deltas are a zero-time update.
        const double dt=std::isfinite(in.dt)&&in.dt>0?std::min(in.dt,.1):0;
        stats_.active_seconds+=dt;if(stage_==Stage::HouseExplore||stage_==Stage::MeadowExplore||stage_==Stage::Complete)stats_.playable_seconds+=dt;
        if(!in.confirm_down)skip_armed_=true;
        if(!in.interact_down)interact_armed_=true;
        if(stage_==Stage::Title){if(confirm_pressed){events|=enter(Stage::HouseEstablish)|Started;}}
        else if(stage_==Stage::GameMenu){
            // The console interaction and controller A may also be confirmation
            // inputs. A complete release after entry/pause prevents accidental
            // NEW GAME activation and keeps cancellation deliberate as well.
            if(!in.confirm_down&&!in.interact_down&&!in.cancel_down)menu_armed_=true;
            if(menu_armed_&&cancel_pressed)events|=enter(Stage::HouseExplore);
            else if(menu_armed_&&confirm_pressed)events|=enter(Stage::Transport);
        }
        else if(cinematic()){
            elapsed_+=dt;
            if(skip_armed_&&in.confirm_down)skip_hold_+=dt;else skip_hold_=0;
            if(stage_==Stage::Transport&&!pulse_&&elapsed_>=config_.transport_duration*.45){pulse_=true;events|=TransportPulse;}
            const bool skipped=skip_hold_+1e-9>=config_.skip_seconds;
            if(skipped||elapsed_+1e-9>=duration()){
                if(skipped){events|=Skipped;++stats_.skips;}
                const Stage next=stage_==Stage::HouseEstablish?Stage::HouseExplore:stage_==Stage::Transport?Stage::MeadowReveal:Stage::MeadowExplore;
                // If transport is skipped before its pulse, still deliver one
                // transition cue; never leave a half-started portal effect.
                if(stage_==Stage::Transport&&!pulse_)events|=TransportPulse;
                events|=enter(next);
            }
        }else if(config_.arrival_camp.enabled&&camp_.active()){
            // Every line needs a fresh press. Leaving or pausing never grants
            // conversation credit, and a held controller A cannot skip lines.
            if(cancel_pressed){camp_.cancel();interact_armed_=false;events|=ReleaseControls;}
            else if(interact_pressed&&interact_armed_){
                interact_armed_=false;
                if(camp_.advance()){events|=CheckpointReached;++stats_.checkpoints;}
                if(!camp_.active())events|=ReleaseControls;
            }
        }else if(interact_pressed&&interact_armed_){
            const int camp_target=config_.arrival_camp.enabled&&(stage_==Stage::MeadowExplore||stage_==Stage::Complete)?world_opening_arrival_camp::nearest(config_.arrival_camp,{in.player_feet.x,in.player_feet.y,in.player_feet.z}):-1;
            if(camp_target>=0){camp_.begin(unsigned(camp_target));interact_armed_=false;events|=ReleaseControls;}
            const auto here=sample(in.player_feet,in.reduced_motion);
            if(here.interaction_available){
                interact_armed_=false;
                if(stage_==Stage::HouseExplore&&!inspected_){inspected_=true;checkpoint_=Checkpoint::HouseInspected;events|=Inspected|CheckpointReached;++stats_.house_interactions;++stats_.checkpoints;}
                else if(stage_==Stage::HouseExplore){++stats_.house_interactions;events|=enter(config_.game_start_story?Stage::GameMenu:Stage::Transport);}
                else if(stage_==Stage::MeadowExplore&&(!config_.arrival_camp.enabled||camp_.complete())){++stats_.meadow_interactions;events|=enter(Stage::Complete);}
            }
        }
        auto frame=sample(in.player_feet,in.reduced_motion);frame.events=events;frame.release_controls=(events&ReleaseControls)!=0;return frame;
    }
};
}
#endif

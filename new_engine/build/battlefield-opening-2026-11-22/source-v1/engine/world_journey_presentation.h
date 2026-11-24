#ifndef VALWOOD_WORLD_JOURNEY_PRESENTATION_H
#define VALWOOD_WORLD_JOURNEY_PRESENTATION_H
#include "engine/world_first_journey.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

// Transient presentation only: no progress, inventory or combat writes.
namespace world_journey_presentation {
// Exact measured foreground bounds for this frame, shared with world labels.
// Wrapped text and UI scaling must not be approximated by an old timer band.
struct Rect {float left=0,top=0,right=0,bottom=0;};
inline bool overlaps(Rect a,Rect b){return a.left<b.right&&a.right>b.left&&a.top<b.bottom&&a.bottom>b.top;}
struct HudRegions {
    std::array<Rect,12> boxes={};unsigned count=0;
    void clear(){count=0;}
    void add(Rect r){if(count<boxes.size()&&std::isfinite(r.left)&&std::isfinite(r.top)&&std::isfinite(r.right)&&std::isfinite(r.bottom)&&r.right>r.left&&r.bottom>r.top)boxes[count++]=r;}
    bool covers(Rect r,float margin=0)const {r.left-=margin;r.top-=margin;r.right+=margin;r.bottom+=margin;for(unsigned i=0;i<count;++i)if(overlaps(r,boxes[i]))return true;return false;}
};
inline Rect notice_rect(float width,float unit,float objective_bottom,float body_height){
    const float left=22*unit,top=objective_bottom+10*unit;
    return {left,top,std::min(width-22*unit,620*unit),top+55*unit+body_height};
}
inline bool road_departure(world_first_journey::Stage stage){using world_first_journey::Stage;return stage==Stage::Road||stage==Stage::Town;}
// Five milestones describe one continuous chapter. These are derived from the
// real quest stage on every frame, including checkpoint restores; no second
// progress clock or completion receipt is introduced by the HUD.
constexpr unsigned milestones=5;
inline unsigned milestone(world_first_journey::Stage s){
    using world_first_journey::Stage;
    switch(s){case Stage::Travelers:return 0;case Stage::Offer:return 1;
        case Stage::Road:case Stage::Encounter:case Stage::RecoverSupplies:return 2;
        case Stage::ReturnCamp:return 3;case Stage::Town:return 4;case Stage::Complete:return 5;}
    return 0;
}
inline const char *milestone_name(unsigned i){
    static constexpr const char *names[]={"Meet","Equip","Recover","Return","Town"};
    return i<milestones?names[i]:"";
}
inline const char *destination(world_first_journey::Stage s){
    using world_first_journey::Stage;
    switch(s){case Stage::Travelers:return "Travelers";case Stage::Offer:return "Bram";
        case Stage::Road:case Stage::Encounter:return "Road raider";
        case Stage::RecoverSupplies:return "Rowan's satchel";case Stage::ReturnCamp:return "Campfire / Rowan";
        case Stage::Town:return "Copper Lantern / Hugh";case Stage::Complete:return "Chapter complete";}
    return "";
}
enum class Bearing {Ahead,Left,Right,BehindLeft,BehindRight};
inline Bearing bearing(double dx,double dz,double yaw){
    if(!std::isfinite(dx)||!std::isfinite(dz)||!std::isfinite(yaw)||std::hypot(dx,dz)<.01)return Bearing::Ahead;
    const double angle=std::remainder(std::atan2(dx,-dz)-yaw,6.283185307179586);
    if(std::abs(angle)<.35)return Bearing::Ahead;
    if(std::abs(angle)>2.35)return angle<0?Bearing::BehindLeft:Bearing::BehindRight;
    return angle<0?Bearing::Left:Bearing::Right;
}
inline std::string heading(const char *name,double dx,double dz,double yaw){
    const std::string label=name?name:"";
    switch(bearing(dx,dz,yaw)){
        case Bearing::Left:return "<  "+label;case Bearing::Right:return label+"  >";
        case Bearing::BehindLeft:return "<  Turn back: "+label;
        case Bearing::BehindRight:return "Turn back: "+label+"  >";
        default:return label;
    }
}
// Frame the retained character from its front quarter instead of rotating its
// planted feet toward the player. The ordinary camera collision guard still
// has final authority over this render-only request.
inline double portrait_heading(double actor_yaw,double player_dx,double player_dz){
    if(!std::isfinite(actor_yaw))return 0.;
    if(!std::isfinite(player_dx)||!std::isfinite(player_dz)||std::hypot(player_dx,player_dz)<.01)return actor_yaw;
    const double relative=std::remainder(std::atan2(player_dx,player_dz)-actor_yaw,6.283185307179586);
    return actor_yaw+std::clamp(relative,-.6108652381980153,.6108652381980153);
}
enum class Card {None,Equipment,RoadClear,Supplies,Reward,Chapter};
struct Notice {
    Card card=Card::None;float remaining=0;
    void clear(){card=Card::None;remaining=0;}
    void tick(double dt,bool paused){if(!paused&&std::isfinite(dt)&&dt>=0&&dt<=.25){remaining=std::max(0.f,remaining-float(dt));if(remaining==0)card=Card::None;}}
    void events(std::uint32_t e){
        using namespace world_first_journey;
        if(e&Completed){card=Card::Chapter;remaining=6;}
        else if(e&RewardGranted){card=Card::Reward;remaining=6;}
        else if(e&SuppliesRecovered){card=Card::Supplies;remaining=4;}
        else if(e&EncounterWon){card=Card::RoadClear;remaining=5;}
        else if(e&WeaponGranted){card=Card::Equipment;remaining=5;}
    }
};
inline const char *title(Card c){switch(c){
    case Card::Equipment:return "BRAM'S LOAN";case Card::RoadClear:return "THE ROAD IS CLEAR";
    case Card::Supplies:return "ROWAN'S SATCHEL RECOVERED";case Card::Reward:return "A PLACE AT THE FIRE";
    case Card::Chapter:return "FIRST CHAPTER COMPLETE";default:return "";}}
inline const char *detail(Card c,bool pad){switch(c){
    case Card::Equipment:return pad?"Loaner sword received. View: inspect your equipment. Follow the road to the raider.":"Loaner sword received. I: inspect your equipment. Follow the road to the raider.";
    case Card::RoadClear:return "Search the fallen raider for equipment if you wish. Rowan's provision satchel is beside the road.";
    case Card::Supplies:return "Bring the provisions back to Rowan at the campfire.";
    case Card::Reward:return "12 copper  |  a hot meal  |  1 reputation\nHealth and stamina restored. Hugh awaits at the Copper Lantern.";
    case Card::Chapter:return "You have a place to rest and a lead on Mara. Your checkpoint is saved.";default:return "";}}
inline const char *weapon_name(unsigned w){switch(w){case 1:return "bow";case 2:return "sword and shield";case 3:return "two-handed axe";case 4:return "quarterstaff";default:return "two-handed sword";}}
inline std::string weapon_action(unsigned w,bool sheathed,bool pad){return std::string(pad?"D-pad Down: ":"Z: ")+(sheathed?"draw ":"put away ")+weapon_name(w);}
inline const char *dialogue_action(world_first_journey::Dialogue d,unsigned beat){
    if(beat+1<world_first_journey::dialogue_length(d))return "Continue";
    return d==world_first_journey::Dialogue::Offer?"Accept the loan":d==world_first_journey::Dialogue::Return?"Thank Rowan":d==world_first_journey::Dialogue::Town?"A place to rest":"Continue";
}
inline std::string combat_hint(bool pad,bool windup,bool opening,bool attacking,float stamina,const char *direction){
    if(windup)return std::string(pad?"LT + right stick ":"Hold RMB + aim ")+direction+" to guard";
    if(stamina<20)return pad?"Low stamina. Release LT and use B to make space.":"Low stamina. Release guard and use Space to make space.";
    if(attacking)return "Link up to three cuts. Change angle before the next swing.";
    if(opening)return "His guard is open. Strike from another angle.";
    return "Watch the windup. Guard first, then answer with a cut.";
}
}
#endif

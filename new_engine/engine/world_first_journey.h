#ifndef VALWOOD_WORLD_FIRST_JOURNEY_H
#define VALWOOD_WORLD_FIRST_JOURNEY_H
// One connected first quest. The host owns movement, combat simulation,
// presentation and atomic checkpoint IO. This clock never fabricates a hit.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <istream>
#include <locale>
#include <sstream>
#include <string>

namespace world_first_journey {
constexpr unsigned version=1;
constexpr unsigned all_travelers=7;
constexpr unsigned reward_coins=12,reward_food=1,reward_reputation=1;
constexpr std::uint64_t reward_transaction_id=0x56414c574f4f4401ull;
struct Vec {double x=0,y=0,z=0;};
// Defaults use the existing v16 campsite, its sealed walk route and the
// Copper Lantern entry. A project can provide another validated local route.
struct Config {
    Vec camp{-1486.37664868499,18.933204601097398,-10224.910611064201};
    Vec rowan{-1488.5266486849901,18.93575044238202,-10225.5606110642};
    Vec bram{-1484.1766486849899,18.945669940985017,-10225.5606110642};
    Vec encounter{-1404.0077270925497,18.944883621506374,-10212.922994171895};
    Vec supplies{-1397.3368877436963,19.918828885345754,-10208.085637873315};
    Vec retry{-1407.1181674027353,19.065666597715865,-10214.754353427777};
    Vec town{-1455.4,24,-10280};
    double talk_radius=2.7,encounter_radius=4,supplies_radius=2.1,town_radius=3.2;
};
enum class Stage : unsigned {Travelers,Offer,Road,Encounter,RecoverSupplies,ReturnCamp,Town,Complete};
enum class Dialogue : unsigned {None,Offer,Return,Town};
enum class CombatStatus : unsigned {None,Running,Won,Lost,Cancelled};
struct CombatReceipt {std::uint64_t token=0;CombatStatus status=CombatStatus::None;};
enum Event : std::uint32_t {
    None=0,StageChanged=1u<<0,ReleaseControls=1u<<1,DialogueOpened=1u<<2,
    DialogueClosed=1u<<3,WeaponGranted=1u<<4,EncounterRequested=1u<<5,
    EncounterWon=1u<<6,EncounterLost=1u<<7,RetryRequested=1u<<8,
    SuppliesRecovered=1u<<9,RewardGranted=1u<<10,CheckpointReached=1u<<11,
    Completed=1u<<12
};
struct Input {
    double dt=0;Vec player_feet;
    bool enabled=true,paused=false,focused=true;
    bool interact_down=false,confirm_down=false,cancel_down=false,retry_down=false;
    unsigned camp_spoken=0;
    // Only the production combat host may issue this receipt. In particular,
    // a terminal result needs a matching Running receipt from an earlier tick.
    CombatReceipt combat;
};
struct Frame {
    Stage stage=Stage::Travelers;std::uint32_t events=None;
    bool controls_enabled=true,combat_controls_enabled=false,dialogue=false;
    bool interaction_available=false,weapon_unlocked=false,defeated=false;
    bool supplies_recovered=false,reward_granted=false,town_reached=false;
    bool release_controls=false;
    unsigned beat=0,camp_spoken=0,coins=0,food=0,reputation=0;
    Dialogue dialogue_kind=Dialogue::None;std::uint64_t encounter_token=0;
    Vec objective_position,dialogue_target;int dialogue_actor=-1;
    const char *title="A PLACE AT THE FIRE",*objective="",*interaction="";
    const char *speaker="",*line="",*controls_help="";
};
struct Save {
    unsigned schema_version=version;std::uint64_t layout_signature=0;
    Stage stage=Stage::Travelers;unsigned camp_spoken=0;
    bool weapon_unlocked=false,encounter_won=false,supplies_recovered=false;
    bool reward_granted=false,town_reached=false;
    unsigned coins=0,food=0,reputation=0;
};
struct Stats {
    unsigned dialogues_completed=0,encounters_started=0,encounters_won=0;
    unsigned defeats=0,retries=0,supplies_pickups=0,rewards=0,completions=0;
    unsigned ignored_combat_receipts=0;double active_seconds=0;
};
namespace detail {
inline bool valid(Vec v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::abs(v.x)<1000000&&std::abs(v.z)<1000000&&std::abs(v.y)<100000;}
inline double distance(Vec a,Vec b){return std::hypot(a.x-b.x,a.z-b.z);}
inline bool near(Vec a,Vec b,double radius){return valid(a)&&distance(a,b)<=radius&&std::abs(a.y-b.y)<=1.35;}
inline bool fail(std::string &error,const char *s){error=s;return false;}
inline void hash_byte(std::uint64_t &h,unsigned char c){h^=c;h*=1099511628211ull;}
inline void hash_double(std::uint64_t &h,double d){std::uint64_t b=0;std::memcpy(&b,&d,sizeof(b));for(unsigned i=0;i<8;++i)hash_byte(h,static_cast<unsigned char>(b>>(i*8)));}
inline std::uint64_t text_hash(const std::string &s){std::uint64_t h=1469598103934665603ull;for(unsigned char c:s)hash_byte(h,c);return h;}
}
inline const char *stage_name(Stage s){switch(s){
    case Stage::Travelers:return "travelers";case Stage::Offer:return "offer";
    case Stage::Road:return "road";case Stage::Encounter:return "encounter";
    case Stage::RecoverSupplies:return "recover_supplies";case Stage::ReturnCamp:return "return_camp";
    case Stage::Town:return "town";case Stage::Complete:return "complete";}return "invalid";}
inline bool validate(const Config &c,std::string &error){
    for(Vec p:{c.camp,c.rowan,c.bram,c.encounter,c.supplies,c.retry,c.town})if(!detail::valid(p))return detail::fail(error,"Invalid first-journey position");
    if(detail::distance(c.camp,c.rowan)>15||detail::distance(c.camp,c.bram)>15||detail::distance(c.rowan,c.bram)<1.1||std::abs(c.rowan.y-c.camp.y)>3||std::abs(c.bram.y-c.camp.y)>3)return detail::fail(error,"First-journey travelers do not match the campsite");
    const double road=detail::distance(c.camp,c.encounter);
    if(road<12||road>250||detail::distance(c.supplies,c.encounter)>12||detail::distance(c.retry,c.encounter)<3||detail::distance(c.retry,c.encounter)>30||detail::distance(c.camp,c.town)<15||detail::distance(c.camp,c.town)>350)return detail::fail(error,"First-journey route exceeds local bounds");
    if(!std::isfinite(c.talk_radius)||c.talk_radius<1||c.talk_radius>3||!std::isfinite(c.encounter_radius)||c.encounter_radius<2||c.encounter_radius>7||!std::isfinite(c.supplies_radius)||c.supplies_radius<1||c.supplies_radius>3||!std::isfinite(c.town_radius)||c.town_radius<1||c.town_radius>5)return detail::fail(error,"Invalid first-journey interaction radius");
    error.clear();return true;
}
inline std::uint64_t signature(const Config &c){
    std::uint64_t h=1469598103934665603ull;
    for(Vec p:{c.camp,c.rowan,c.bram,c.encounter,c.supplies,c.retry,c.town})for(double d:{p.x,p.y,p.z})detail::hash_double(h,d);
    for(double d:{c.talk_radius,c.encounter_radius,c.supplies_radius,c.town_radius})detail::hash_double(h,d);
    return h;
}
inline bool parse_config(std::istream &input,Config &out,std::string &error){
    std::string bytes;char ch;while(input.get(ch)){if(ch=='\0'||bytes.size()>=4096)return detail::fail(error,"First-journey settings exceed bounds");bytes+=ch;}
    if(input.bad()||(input.fail()&&!input.eof()))return detail::fail(error,"Unreadable first-journey settings");
    std::istringstream rows(bytes);rows.imbue(std::locale::classic());Config next;std::string tag;unsigned v=0;
    if(!(rows>>tag>>v)||tag!="VALWOOD_FIRST_JOURNEY"||v!=version)return detail::fail(error,"Invalid first-journey settings header");
    auto position=[&](const char *name,Vec &p){return bool(rows>>tag>>p.x>>p.y>>p.z)&&tag==name;};
    if(!position("camp",next.camp)||!position("rowan",next.rowan)||!position("bram",next.bram)||!position("encounter",next.encounter)||!position("supplies",next.supplies)||!position("retry",next.retry)||!position("town",next.town)||!(rows>>tag>>next.talk_radius>>next.encounter_radius>>next.supplies_radius>>next.town_radius)||tag!="radii"||!(rows>>tag)||tag!="end")return detail::fail(error,"Malformed first-journey route");
    rows>>std::ws;if(!rows.eof())return detail::fail(error,"Extra first-journey settings records");
    if(!validate(next,error))return false;
    out=next;return true;
}
struct Line {const char *speaker="",*text="";};
inline unsigned dialogue_length(Dialogue d){return d==Dialogue::Offer?7:d==Dialogue::Return?5:d==Dialogue::Town?3:0;}
inline Line dialogue_line(Dialogue d,unsigned beat){
    static constexpr Line offer[]={
        {"Bram","Before you go, there's something you can do for us. A raider took Rowan's provisions and made camp by the road."},
        {"You","I'm not a soldier. Back home, I barely have time to sit down."},
        {"Bram","Then don't fight like a hero. Keep your distance, watch his blade, and give ground when you need to."},
        {"Bram","Take this old sword. Choose a direction and commit to the cut. A swing only counts when the blade reaches him."},
        {"Bram","Hold your guard and match the incoming direction. Catch it early and you can strike before he recovers."},
        {"You","Get the supplies. Come back. One thing at a time."},
        {"Bram","Exactly. Take a moment before you leave. And if you're hurt, fall back. The fire will still be here."}
    };
    static constexpr Line returned[]={
        {"Rowan","You came back. And with the satchel... I was beginning to worry."},
        {"You","I thought about running. I kept hearing what Bram said."},
        {"Rowan","Being afraid and doing it anyway counts for something. Sit. You've earned a hot meal."},
        {"Rowan","Take twelve coins for the road. At the Copper Lantern, ask after Mara. Someone in town may know what brought you here."},
        {"You","Back home, I felt invisible. Thank you for seeing me."}
    };
    static constexpr Line town[]={
        {"Hugh","Rowan sent you? Then come in. Any friend of that fire has a place here."},
        {"You","I'm looking for Mara. And a way home."},
        {"Hugh","Mara keeps the provisions shop by the keep. Rest first. We'll find your next step together."}
    };
    if(beat>=dialogue_length(d))return {};
    return d==Dialogue::Offer?offer[beat]:d==Dialogue::Return?returned[beat]:town[beat];
}
inline bool validate_save(const Save &s,const Config &c,std::string &error){
    if(!validate(c,error))return false;
    if(s.schema_version!=version||s.layout_signature!=signature(c)||unsigned(s.stage)>unsigned(Stage::Complete)||s.camp_spoken>all_travelers||s.stage==Stage::Encounter)return detail::fail(error,"Invalid or incompatible first-journey checkpoint");
    const bool offer=s.stage>=Stage::Offer,weapon=s.stage>=Stage::Road,won=s.stage>=Stage::RecoverSupplies,supplies=s.stage>=Stage::ReturnCamp,reward=s.stage>=Stage::Town,town=s.stage==Stage::Complete;
    if((offer&&s.camp_spoken!=all_travelers)||(!offer&&s.camp_spoken==all_travelers)||s.weapon_unlocked!=weapon||s.encounter_won!=won||s.supplies_recovered!=supplies||s.reward_granted!=reward||s.town_reached!=town||s.coins!=(reward?reward_coins:0)||s.food!=(reward?reward_food:0)||s.reputation!=(reward?reward_reputation:0))return detail::fail(error,"First-journey checkpoint has inconsistent progress or rewards");
    error.clear();return true;
}
inline std::string save_body(const Save &s){
    std::ostringstream rows;rows.imbue(std::locale::classic());
    rows<<"layout "<<s.layout_signature<<"\nstage "<<unsigned(s.stage)<<"\ncamp "<<s.camp_spoken<<"\nprogress "<<unsigned(s.weapon_unlocked)<<' '<<unsigned(s.encounter_won)<<' '<<unsigned(s.supplies_recovered)<<' '<<unsigned(s.reward_granted)<<' '<<unsigned(s.town_reached)<<"\nreward "<<s.coins<<' '<<s.food<<' '<<s.reputation<<"\nend\n";return rows.str();
}
inline bool encode_save(const Save &s,const Config &c,std::string &out,std::string &error){
    if(!validate_save(s,c,error))return false;
    const auto body=save_body(s);std::ostringstream rows;rows.imbue(std::locale::classic());rows<<"VALWOOD_FIRST_JOURNEY_PROGRESS "<<version<<' '<<detail::text_hash(body)<<'\n'<<body;out=rows.str();return true;
}
inline bool parse_save(std::istream &input,const Config &c,Save &out,std::string &error){
    std::string bytes;char ch;while(input.get(ch)){if(ch=='\0'||bytes.size()>=2048)return detail::fail(error,"First-journey checkpoint exceeds bounds");bytes+=ch;}
    if(input.bad()||(input.fail()&&!input.eof()))return detail::fail(error,"Unreadable first-journey checkpoint");
    const auto split=bytes.find('\n');if(split==std::string::npos)return detail::fail(error,"Missing first-journey checkpoint body");
    std::istringstream header(bytes.substr(0,split));header.imbue(std::locale::classic());Save next;std::string tag;std::uint64_t hash=0;
    if(!(header>>tag>>next.schema_version>>hash)||tag!="VALWOOD_FIRST_JOURNEY_PROGRESS")return detail::fail(error,"Invalid first-journey checkpoint header");
    header>>std::ws;if(!header.eof())return detail::fail(error,"Extra first-journey checkpoint header fields");
    const auto body=bytes.substr(split+1);if(detail::text_hash(body)!=hash)return detail::fail(error,"First-journey checkpoint checksum mismatch");
    std::istringstream rows(body);rows.imbue(std::locale::classic());unsigned stage=0,w=0,v=0,s=0,r=0,t=0;
    if(!(rows>>tag>>next.layout_signature)||tag!="layout"||!(rows>>tag>>stage)||tag!="stage"||stage>unsigned(Stage::Complete)||!(rows>>tag>>next.camp_spoken)||tag!="camp"||!(rows>>tag>>w>>v>>s>>r>>t)||tag!="progress"||w>1||v>1||s>1||r>1||t>1||!(rows>>tag>>next.coins>>next.food>>next.reputation)||tag!="reward"||!(rows>>tag)||tag!="end")return detail::fail(error,"Malformed first-journey checkpoint");
    rows>>std::ws;if(!rows.eof())return detail::fail(error,"Extra first-journey checkpoint records");
    next.stage=Stage(stage);next.weapon_unlocked=w!=0;next.encounter_won=v!=0;next.supplies_recovered=s!=0;next.reward_granted=r!=0;next.town_reached=t!=0;
    if(!validate_save(next,c,error))return false;
    out=next;return true;
}
class State {
    Config config_;Stage stage_=Stage::Travelers;unsigned camp_=0;
    Dialogue dialogue_=Dialogue::None;unsigned beat_=0;
    bool weapon_=false,won_=false,supplies_=false,reward_=false,town_=false;
    bool enabled_last_=false,blocked_last_=false,neutral_=false;
    bool interact_last_=false,confirm_last_=false,cancel_last_=false,retry_last_=false;
    bool running_=false,defeated_=false;std::uint64_t token_=0,serial_=0;
    Stats stats_;
    std::uint32_t enter(Stage next){stage_=next;neutral_=false;return StageChanged|ReleaseControls;}
    std::uint32_t open(Dialogue d){dialogue_=d;beat_=0;neutral_=false;return DialogueOpened|ReleaseControls;}
    std::uint32_t close(){dialogue_=Dialogue::None;beat_=0;neutral_=false;return DialogueClosed|ReleaseControls;}
    void clear_input(){neutral_=false;interact_last_=confirm_last_=cancel_last_=retry_last_=false;enabled_last_=blocked_last_=false;}
    void invalidate_receipt(){token_=0;running_=defeated_=false;if(++serial_==0)++serial_;}
public:
    bool configure(const Config &c,std::string &error){if(!validate(c,error))return false;config_=c;reset();return true;}
    const Config &config()const{return config_;}Stage stage()const{return stage_;}
    Dialogue dialogue()const{return dialogue_;}unsigned beat()const{return beat_;}
    bool weapon_unlocked()const{return weapon_;}bool encounter_won()const{return won_;}
    bool supplies_recovered()const{return supplies_;}bool reward_granted()const{return reward_;}
    bool town_reached()const{return town_;}bool defeated()const{return defeated_;}
    bool encounter_running()const{return running_;}std::uint64_t encounter_token()const{return token_;}
    unsigned camp_spoken()const{return camp_;}const Stats &stats()const{return stats_;}
    void reset(){stage_=Stage::Travelers;camp_=0;dialogue_=Dialogue::None;beat_=0;weapon_=won_=supplies_=reward_=town_=false;stats_={};clear_input();invalidate_receipt();}
    Save stable_save()const{
        Save s;s.layout_signature=signature(config_);s.stage=stage_==Stage::Encounter?Stage::Road:stage_;s.camp_spoken=camp_;
        s.weapon_unlocked=weapon_;s.encounter_won=won_;s.supplies_recovered=supplies_;s.reward_granted=reward_;s.town_reached=town_;
        s.coins=reward_?reward_coins:0;s.food=reward_?reward_food:0;s.reputation=reward_?reward_reputation:0;return s;
    }
    bool restore(const Save &s,std::string &error){
        if(!validate_save(s,config_,error))return false;
        stage_=s.stage;camp_=s.camp_spoken;weapon_=s.weapon_unlocked;won_=s.encounter_won;supplies_=s.supplies_recovered;reward_=s.reward_granted;town_=s.town_reached;
        dialogue_=Dialogue::None;beat_=0;clear_input();invalidate_receipt();error.clear();return true;
    }
    // A hint for this authored layout, never a collision authority. The host
    // validates support before placing the player. Camp center is the fire;
    // use the arrival side rather than spawning a restored player in it.
    Vec checkpoint_position()const{
        if(stage_==Stage::Complete)return config_.town;
        if(stage_==Stage::Encounter||stage_==Stage::RecoverSupplies)return config_.retry;
        return {config_.camp.x,config_.camp.y,config_.camp.z+5.2};
    }
    Frame sample(Vec player={})const{
        Frame f;f.stage=stage_;f.weapon_unlocked=weapon_;f.supplies_recovered=supplies_;f.reward_granted=reward_;f.town_reached=town_;f.defeated=defeated_;f.camp_spoken=camp_;f.encounter_token=token_;
        f.coins=reward_?reward_coins:0;f.food=reward_?reward_food:0;f.reputation=reward_?reward_reputation:0;
        f.dialogue=dialogue_!=Dialogue::None;f.dialogue_kind=dialogue_;f.beat=beat_;f.controls_enabled=!f.dialogue&&!defeated_;f.combat_controls_enabled=stage_==Stage::Encounter&&running_&&!defeated_;
        f.controls_help=weapon_?"Choose your cut direction. Hold guard and match the incoming blade. Dodge to make space.":"";
        switch(stage_){
        case Stage::Travelers:f.objective="Speak to all three travelers by the fire.";f.objective_position=config_.camp;break;
        case Stage::Offer:f.objective="Ask Bram how you can help the camp.";f.objective_position=config_.bram;f.interaction="Ask Bram about the missing supplies";f.interaction_available=detail::near(player,config_.bram,config_.talk_radius);break;
        case Stage::Road:f.objective="Follow the road and recover Rowan's stolen provisions.";f.objective_position=config_.encounter;f.interaction="Confront the road raider";f.interaction_available=detail::near(player,config_.encounter,config_.encounter_radius);break;
        case Stage::Encounter:f.objective=defeated_?"You fell. Take a breath, then try again.":"Watch the raider's blade. Defend, make space, then strike.";f.objective_position=config_.encounter;f.interaction=defeated_?"Retry the encounter":"";f.interaction_available=defeated_;break;
        case Stage::RecoverSupplies:f.objective="Recover Rowan's provision satchel.";f.objective_position=config_.supplies;f.interaction="Recover the provision satchel";f.interaction_available=detail::near(player,config_.supplies,config_.supplies_radius);break;
        case Stage::ReturnCamp:f.objective="Bring the provisions back to Rowan at the fire.";f.objective_position=config_.rowan;f.interaction="Return the provisions to Rowan";f.interaction_available=detail::near(player,config_.rowan,config_.talk_radius);break;
        case Stage::Town:f.objective="Follow the road to the Copper Lantern. Ask Hugh about Mara.";f.objective_position=config_.town;f.interaction="Speak to Hugh at the Copper Lantern";f.interaction_available=detail::near(player,config_.town,config_.town_radius);break;
        case Stage::Complete:f.objective="A place to rest. Your next lead is Mara at the provisions shop.";f.objective_position=config_.town;break;
        }
        if(f.dialogue){const auto l=dialogue_line(dialogue_,beat_);f.speaker=l.speaker;f.line=l.text;f.interaction_available=false;
            f.dialogue_actor=dialogue_==Dialogue::Offer?1:dialogue_==Dialogue::Return?0:3;
            f.dialogue_target=dialogue_==Dialogue::Offer?config_.bram:dialogue_==Dialogue::Return?config_.rowan:config_.town;
        }
        return f;
    }
    Frame tick(const Input &in){
        std::uint32_t events=None;
        const bool blocked=in.paused||!in.focused||!detail::valid(in.player_feet)||!std::isfinite(in.dt)||in.dt<0||in.dt>.25;
        const bool all_up=!in.interact_down&&!in.confirm_down&&!in.cancel_down&&!in.retry_down;
        if(!in.enabled||blocked||!enabled_last_||blocked_last_){neutral_=false;events|=ReleaseControls;}
        const bool usable=in.enabled&&!blocked;
        if(usable&&enabled_last_&&!blocked_last_&&all_up)neutral_=true;
        const bool interact=usable&&neutral_&&in.interact_down&&!interact_last_;
        const bool confirm=usable&&neutral_&&in.confirm_down&&!confirm_last_;
        const bool cancel=usable&&neutral_&&in.cancel_down&&!cancel_last_;
        const bool retry=usable&&neutral_&&in.retry_down&&!retry_last_;
        interact_last_=in.interact_down;confirm_last_=in.confirm_down;cancel_last_=in.cancel_down;retry_last_=in.retry_down;
        enabled_last_=in.enabled;blocked_last_=blocked;
        if(usable){
            stats_.active_seconds+=in.dt;
            if(stage_==Stage::Travelers&&in.camp_spoken<=all_travelers){camp_|=in.camp_spoken;if(camp_==all_travelers)events|=enter(Stage::Offer)|CheckpointReached;}
            else if(dialogue_!=Dialogue::None){
                if(cancel)events|=close();
                else if(interact||confirm){
                    neutral_=false;
                    if(++beat_>=dialogue_length(dialogue_)){
                        const auto done=dialogue_;events|=close();++stats_.dialogues_completed;
                        if(done==Dialogue::Offer){weapon_=true;events|=enter(Stage::Road)|WeaponGranted|CheckpointReached;}
                        else if(done==Dialogue::Return&&!reward_){reward_=true;++stats_.rewards;events|=enter(Stage::Town)|RewardGranted|CheckpointReached;}
                        else if(done==Dialogue::Town&&!town_){town_=true;++stats_.completions;events|=enter(Stage::Complete)|Completed|CheckpointReached;}
                    }
                }
            }else if(stage_==Stage::Encounter){
                const auto receipt=in.combat;
                if(receipt.status!=CombatStatus::None){
                    if(receipt.token!=token_||!token_||defeated_||unsigned(receipt.status)>unsigned(CombatStatus::Cancelled))++stats_.ignored_combat_receipts;
                    else if(receipt.status==CombatStatus::Running)running_=true;
                    else if(receipt.status==CombatStatus::Cancelled){invalidate_receipt();events|=enter(Stage::Road)|CheckpointReached;}
                    else if(!running_)++stats_.ignored_combat_receipts;
                    else if(receipt.status==CombatStatus::Won){won_=true;running_=false;++stats_.encounters_won;events|=enter(Stage::RecoverSupplies)|EncounterWon|CheckpointReached;}
                    else if(receipt.status==CombatStatus::Lost){defeated_=true;running_=false;neutral_=false;++stats_.defeats;events|=EncounterLost|ReleaseControls;}
                }
                if(defeated_&&(retry||interact||confirm)&&!(events&EncounterLost)){++stats_.retries;invalidate_receipt();events|=enter(Stage::Road)|RetryRequested|CheckpointReached;}
            }else if(interact||confirm){
                if(stage_==Stage::Offer&&detail::near(in.player_feet,config_.bram,config_.talk_radius))events|=open(Dialogue::Offer);
                else if(stage_==Stage::Road&&detail::near(in.player_feet,config_.encounter,config_.encounter_radius)){
                    if(++serial_==0)++serial_;
                    token_=serial_;running_=defeated_=false;++stats_.encounters_started;events|=enter(Stage::Encounter)|EncounterRequested;
                }else if(stage_==Stage::RecoverSupplies&&detail::near(in.player_feet,config_.supplies,config_.supplies_radius)){
                    supplies_=true;++stats_.supplies_pickups;events|=enter(Stage::ReturnCamp)|SuppliesRecovered|CheckpointReached;
                }else if(stage_==Stage::ReturnCamp&&detail::near(in.player_feet,config_.rowan,config_.talk_radius))events|=open(Dialogue::Return);
                else if(stage_==Stage::Town&&detail::near(in.player_feet,config_.town,config_.town_radius))events|=open(Dialogue::Town);
            }
        }
        auto f=sample(in.player_feet);f.events=events;f.release_controls=(events&ReleaseControls)!=0;
        if(!usable){f.controls_enabled=f.combat_controls_enabled=false;f.interaction_available=false;}
        return f;
    }
};
}
#endif

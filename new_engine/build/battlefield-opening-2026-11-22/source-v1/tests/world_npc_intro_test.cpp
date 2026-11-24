#include "engine/world_opening_prologue.h"
#include <iostream>
#include <limits>
#include <sstream>
using namespace world_opening_prologue;
static unsigned checks=0;static std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<" | "<<error<<'\n';return 1;}}while(0)
static bool close(double a,double b){return std::abs(a-b)<1e-8;}
static bool same(Vec a,Vec b){return close(a.x,b.x)&&close(a.y,b.y)&&close(a.z,b.z);}
static Config config(){
    Config c;c.npc_intro.enabled=true;c.arrival_camp.enabled=true;c.arrival_camp.center={100,0,0};
    c.arrival_camp.actors={{{{98,0,0},.6,true},{{102,0,0},-.4,false},{{100,0,3},2.4,true}}};return c;
}
static Frame tick(State &s,double dt,Vec feet,bool confirm=false,bool interact=false,bool paused=false,bool cancel=false,bool reduced=false){return s.tick({dt,feet,paused,confirm,interact,reduced,cancel});}
static Vec feet(const Config &c,unsigned i){const auto p=c.arrival_camp.actors[i].feet;return {p.x,p.y,p.z};}
static bool restore_meadow(State &s,const Config &c){return s.configure(c,error)&&s.restore(checkpoint_save(Checkpoint::MeadowStart,c),error);}
static Frame begin(State &s,Vec p,bool confirm=false){tick(s,0,p);return tick(s,.016,p,confirm,true);}
int main(){
    Config c=config();CHECK(validate(c,error));CHECK(close(c.npc_intro.duration_seconds,36)&&close(c.npc_intro.skip_seconds,.8));
    // Optional settings are transactional and bounded; absence defaults off.
    CHECK(!world_npc_intro::Config{}.enabled);
    for(double d:{.1,1.25,10.,36.,300.}){world_npc_intro::Config parsed;std::istringstream in("VALWOOD_NPC_INTRO 1\nduration "+std::to_string(d)+"\n");CHECK(world_npc_intro::parse(in,parsed,error));CHECK(parsed.enabled&&close(parsed.duration_seconds,d));}
    for(const std::string &bytes:std::initializer_list<std::string>{"","VALWOOD_NPC_INTRO 2 duration 36","VALWOOD_NPC_INTRO 1 duration .01","VALWOOD_NPC_INTRO 1 duration 300.1","VALWOOD_NPC_INTRO 1 duration nan","VALWOOD_NPC_INTRO 1 duration 36 junk","VALWOOD_NPC_INTRO 1 duration 36 duration 40",std::string(513,'x'),std::string("VALWOOD_NPC_INTRO 1 duration 36")+char(0)+"extra"}){
        world_npc_intro::Config unchanged;unchanged.duration_seconds=42;std::istringstream in(bytes);CHECK(!world_npc_intro::parse(in,unchanged,error));CHECK(!unchanged.enabled&&unchanged.duration_seconds==42);
    }
    Config bad=c;bad.arrival_camp.enabled=false;CHECK(!validate(bad,error));bad=c;bad.npc_intro.duration_seconds=0;CHECK(!validate(bad,error));
    // Any of the three genuine first interactions triggers once, without
    // advancing dialogue/quest progress or moving the player's saved spawn.
    for(unsigned first=0;first<3;++first){
        State s;CHECK(restore_meadow(s,c));const auto p=feet(c,first);auto f=begin(s,p);
        CHECK(f.npc_intro&&f.cinematic&&f.camp_dialogue&&!f.controls_enabled&&f.release_controls);
        CHECK((f.events&NpcIntroStarted)&&!(f.events&CheckpointReached)&&s.camp().beat()==0&&s.camp().spoken()==0&&s.camp().actor()==int(first));
        CHECK(!s.npc_intro_seen()&&!s.stable_save().npc_intro_seen);CHECK(f.npc_intro_duration==36&&f.npc_intro_time==0);
        const auto baseline=s.sample(p);const auto other=s.sample({900,4,-800});CHECK(same(baseline.camera.eye,other.camera.eye)&&same(baseline.camera.target,other.camera.target));
        unsigned complete=0;const auto objectives=f.objectives_complete;const auto checkpoint=s.checkpoint();
        for(unsigned i=0;i<400&&f.npc_intro;++i){
            // Escape cannot close the retained conversation; the host may pause.
            f=tick(s,.1,p,false,false,false,true);complete+=(f.events&NpcIntroCompleted)?1:0;
            CHECK(s.camp().active()&&s.camp().beat()==0&&s.camp().spoken()==0);CHECK(f.objectives_complete==objectives&&s.checkpoint()==checkpoint);
            CHECK(!(f.events&(Completed|Inspected|TransportStarted|Arrived)));
            if(f.npc_intro){CHECK(detail::valid(f.camera.eye)&&detail::valid(f.camera.target));CHECK(std::hypot(std::hypot(f.camera.eye.x-f.camera.target.x,f.camera.eye.z-f.camera.target.z),f.camera.eye.y-f.camera.target.y)>.1);CHECK(f.npc_intro_shot_index<5&&std::string(f.subtitle).size()>5);}
        }
        CHECK(!f.npc_intro&&!f.cinematic&&f.camp_dialogue&&complete==1&&s.npc_intro_seen());
        CHECK(f.release_controls&&(f.events&CheckpointReached)&&!(f.events&Skipped));CHECK(s.camp().beat()==0&&s.stats().checkpoints==0);
        CHECK(s.stable_save().npc_intro_seen&&s.stable_save().camp_spoken==0&&s.stable_save().checkpoint==Checkpoint::MeadowStart);
        // Ending still requires a release before a new ordinary dialogue press.
        tick(s,0,p);f=tick(s,.016,p,false,true);CHECK(s.camp().beat()==1&&!f.npc_intro);
        tick(s,0,p);tick(s,.016,p,false,false,false,true);CHECK(!s.camp().active());
        f=begin(s,feet(c,(first+1)%3));CHECK(!f.npc_intro&&f.camp_dialogue&&s.camp().beat()==0);
    }
    // Held entry button, partial hold, pause, resumed hold, and cancel all stay
    // honest: only a released, continuous confirm hold of .8s skips.
    State s;CHECK(restore_meadow(s,c));auto p=feet(c,0);auto f=begin(s,p,true);
    for(unsigned i=0;i<20;++i){f=tick(s,.1,p,true,true);}CHECK(f.npc_intro&&f.npc_intro_skip_progress==0);
    for(unsigned i=0;i<10;++i){f=tick(s,.1,p,false,true);}CHECK(f.npc_intro&&f.npc_intro_skip_progress==0);
    tick(s,0,p);for(unsigned i=0;i<4;++i)f=tick(s,.1,p,true);CHECK(close(f.npc_intro_skip_progress,.5));
    const double paused_time=f.npc_intro_time;const auto camera=f.camera;
    for(unsigned i=0;i<10;++i){f=tick(s,100,p,true,false,true);CHECK(f.npc_intro_time==paused_time&&close(f.npc_intro_skip_progress,.5)&&same(f.camera.eye,camera.eye));}
    for(unsigned i=0;i<10;++i){f=tick(s,.1,p,true);}CHECK(f.npc_intro&&f.npc_intro_skip_progress==0);
    tick(s,0,p);for(unsigned i=0;i<7;++i)f=tick(s,.1,p,true,true);CHECK(f.npc_intro&&f.npc_intro_skip_progress>.87);
    f=tick(s,.1,p,true,true);CHECK(!f.npc_intro&&(f.events&Skipped)&&(f.events&NpcIntroCompleted)&&f.release_controls&&s.npc_intro_seen());
    CHECK(s.camp().beat()==0&&s.camp().spoken()==0&&s.stats().skips==1);
    for(unsigned i=0;i<10;++i){tick(s,.1,p,true,true);}CHECK(s.camp().beat()==0);tick(s,0,p);tick(s,.01,p,false,true);CHECK(s.camp().beat()==1);
    // Interrupted save/retry replays the intro; committed completion does not.
    State partial;CHECK(restore_meadow(partial,c));f=begin(partial,p);for(unsigned i=0;i<12;++i)tick(partial,.1,p);
    auto saved=partial.stable_save();CHECK(!saved.npc_intro_seen&&saved.camp_spoken==0);partial.retry();CHECK(!partial.camp().active()&&!partial.npc_intro_seen());f=begin(partial,p);CHECK(f.npc_intro&&f.npc_intro_time==0);
    State interrupted;CHECK(interrupted.configure(c,error));CHECK(interrupted.restore(saved,error));CHECK(begin(interrupted,p).npc_intro);
    saved=s.stable_save();std::string encoded;CHECK(encode_save(saved,c,encoded,error));CHECK(encoded.find("npc_intro_seen 1")!=std::string::npos);Save decoded;std::istringstream bytes(encoded);CHECK(parse_save(bytes,c,decoded,error)&&decoded.npc_intro_seen);
    State complete;CHECK(complete.configure(c,error)&&complete.restore(decoded,error));CHECK(!begin(complete,p).npc_intro&&complete.npc_intro_seen());complete.retry();CHECK(complete.npc_intro_seen());
    auto old=checkpoint_save(Checkpoint::MeadowStart,c);CHECK(encode_save(old,c,encoded,error));CHECK(encoded.find("npc_intro_seen")==std::string::npos);decoded.npc_intro_seen=true;std::istringstream old_input(encoded);CHECK(parse_save(old_input,c,decoded,error)&&!decoded.npc_intro_seen);
    for(const std::string suffix:{"npc_intro_seen 2\n","npc_intro_seen 1\nnpc_intro_seen 1\n","npc_intro_seen 1\ncamp_spoken 0\n","npc_intro_seen 1\nunknown 3\n"}){std::istringstream in(encoded+suffix);Save untouched=saved;CHECK(!parse_save(in,c,untouched,error)&&untouched.npc_intro_seen);}
    old.npc_intro_seen=true;old.checkpoint=Checkpoint::HouseStart;old.player=c.house_spawn;CHECK(!validate_save(old,c,error));
    // The appended seen marker also round-trips after both nested records; it
    // must not mint equipment, quest progress, coins or reward receipts.
    Config connected=c;connected.first_journey_enabled=true;CHECK(validate(connected,error));
    world_first_journey::State quest;CHECK(quest.configure(connected.first_journey,error));
    auto nested=checkpoint_save(Checkpoint::MeadowStart,connected);nested.npc_intro_seen=true;
    nested.first_journey_present=true;nested.first_journey=quest.stable_save();nested.inventory_present=true;
    CHECK(encode_save(nested,connected,encoded,error));CHECK(encoded.find("inventory ")<encoded.find("npc_intro_seen 1"));
    std::istringstream nested_input(encoded);Save nested_out;CHECK(parse_save(nested_input,connected,nested_out,error));
    CHECK(nested_out.npc_intro_seen&&nested_out.first_journey_present&&nested_out.inventory_present);
    CHECK(nested_out.camp_spoken==0&&nested_out.first_journey.coins==0&&!nested_out.first_journey.reward_granted&&nested_out.inventory.receipts==0);
    // Existing credited saves and disabled configurations retain original flow.
    for(unsigned mask:{1u,2u,4u,7u}){State credited;CHECK(credited.configure(c,error));auto save=checkpoint_save(Checkpoint::MeadowStart,c);save.camp_spoken=mask;CHECK(credited.restore(save,error));CHECK(!begin(credited,p).npc_intro);}
    Config off=c;off.npc_intro.enabled=false;State legacy;CHECK(restore_meadow(legacy,off));CHECK(!begin(legacy,p).npc_intro&&legacy.camp().beat()==0);
    // Bounded clock, duration endpoints, reduced motion, and all camera shots.
    for(double d:{10.,36.,300.}){Config timed=c;timed.npc_intro.duration_seconds=d;State paced;CHECK(restore_meadow(paced,timed));f=begin(paced,p);double elapsed=0;unsigned shots=0;
        while(f.npc_intro){f=tick(paced,.1,p);elapsed+=.1;if(f.npc_intro)shots|=1u<<f.npc_intro_shot_index;CHECK(elapsed<=d+.100001);}
        CHECK(close(elapsed,d)&&shots==31&&paced.npc_intro_seen());
    }
    // Supplied prerendered clips may be much shorter than authored scene
    // shots. Their actual timeline still completes/persists without quest credit
    // at30/60/120Hz, including pause and a valid held skip where time permits.
    for(double short_duration:{.1,1.25})for(double hz:{30.,60.,120.}){
        Config short_config=c;short_config.npc_intro.duration_seconds=short_duration;State short_state;CHECK(restore_meadow(short_state,short_config));
        auto short_frame=begin(short_state,p);CHECK(short_frame.npc_intro&&short_frame.npc_intro_time==0);
        for(unsigned i=0;i<3;++i){short_frame=tick(short_state,1./hz,p,false,false,true);CHECK(short_frame.npc_intro&&short_frame.npc_intro_time==0);}
        double consumed=0;unsigned endings=0;
        while(short_frame.npc_intro){short_frame=tick(short_state,1./hz,p);consumed+=1./hz;endings+=(short_frame.events&NpcIntroCompleted)?1:0;CHECK(consumed<=short_duration+1./hz+1e-8);}
        CHECK(endings==1&&short_state.npc_intro_seen()&&!(short_frame.events&Skipped));
        CHECK(short_state.camp().active()&&short_state.camp().beat()==0&&short_state.camp().spoken()==0);
        auto short_save=short_state.stable_save();CHECK(short_save.npc_intro_seen&&short_save.camp_spoken==0);
        std::string short_bytes;CHECK(encode_save(short_save,short_config,short_bytes,error));Save short_out;std::istringstream short_input(short_bytes);CHECK(parse_save(short_input,short_config,short_out,error)&&short_out.npc_intro_seen);
        State short_restored;CHECK(short_restored.configure(short_config,error)&&short_restored.restore(short_out,error));CHECK(!begin(short_restored,p).npc_intro);
        State short_skip;CHECK(restore_meadow(short_skip,short_config));short_frame=begin(short_skip,p);tick(short_skip,0,p);
        consumed=0;while(short_frame.npc_intro){short_frame=tick(short_skip,1./hz,p,true,true);consumed+=1./hz;CHECK(consumed<=std::min(short_duration,.8)+1./hz+1e-8);}
        CHECK(bool(short_frame.events&Skipped)==(short_duration>.8));CHECK(short_skip.npc_intro_seen()&&short_skip.camp().beat()==0&&short_skip.camp().spoken()==0);
    }
    State bounded;CHECK(restore_meadow(bounded,c));f=begin(bounded,p);for(double dt:{-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){f=tick(bounded,dt,p);CHECK(f.npc_intro_time==0);}
    f=tick(bounded,500,p);CHECK(close(f.npc_intro_time,.1));
    for(unsigned a=0;a<3;++a){const auto first=world_npc_intro::sample(c.npc_intro,c.arrival_camp,a,0,true);for(unsigned i=0;i<=360;++i){const auto sample=world_npc_intro::sample(c.npc_intro,c.arrival_camp,a,i*.1,true);CHECK(close(sample.camera.eye.x,first.camera.eye.x)&&close(sample.camera.eye.y,first.camera.eye.y)&&close(sample.camera.eye.z,first.camera.eye.z));CHECK(close(sample.camera.target.x,first.camera.target.x)&&sample.camera.fade==0);}}
    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"natural_first_actor_cases\":3,\"quest_credit_from_intro\":false,\"media_or_gpu_required\":false}\n";
}

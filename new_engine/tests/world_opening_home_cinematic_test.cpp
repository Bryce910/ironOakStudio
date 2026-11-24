#include "engine/world_opening_prologue.h"
#include <iostream>
#include <limits>
#include <sstream>
using namespace world_opening_prologue;
static unsigned checks=0;
static std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<" | "<<error<<'\n';return 1;}}while(0)
static bool close(double a,double b,double tolerance=1e-8){return std::abs(a-b)<tolerance;}
static bool same(Vec a,Vec b){return close(a.x,b.x)&&close(a.y,b.y)&&close(a.z,b.z);}
static Frame tick(State &s,double dt=1./60.,bool confirm=false,bool paused=false,bool reduced=false){return s.tick({dt,{},paused,confirm,false,reduced,false});}
static Rail rail(Vec from,Vec to,Vec look_from,Vec look_to,double fov_from=.8,double fov_to=.9){return {from,to,look_from,look_to,fov_from,fov_to};}
static Config sequence(){
    Config c;c.game_start_story=true;c.house_duration=.5;c.house_spawn={{4,2,6},.7,-.1};
    const auto &p=c.house_spawn;const Vec eye{p.feet.x,p.feet.y+eye_height,p.feet.z};
    const Vec target{p.feet.x+std::sin(p.yaw),p.feet.y+eye_height+std::tan(p.pitch),p.feet.z-std::cos(p.yaw)};
    c.house_shots={
        {rail({0,2,0},{.2,2,.1},{1,1,2},{1.2,1,2.1}),1.,"Another shift. Another overdue bill."},
        {rail({2,1,2},{2.1,1.05,2.1},{2,.5,3},{2.1,.5,3.1}),1.5,"The hours keep getting longer."},
        {rail({3.8,3.75,6.1},eye,{4.3,3.65,5.2},target,.85,.9),2.5,"Just a quiet evening. That's all."}
    };return c;
}
int main(){
    Config legacy;CHECK(validate(legacy,error));CHECK(legacy.house_shots.empty()&&close(house_duration(legacy),legacy.house_duration));
    for(double t:{0.,.4,.8,2.,6.5,100.}){
        const auto f=sample_house_cinematic(legacy,t);const auto old=detail::rail(legacy.house_rail,std::clamp(t/legacy.house_duration,0.,1.),false);
        CHECK(same(f.camera.eye,old.eye)&&same(f.camera.target,old.target)&&close(f.camera.fov,old.fov));
        CHECK(close(f.camera.fade,1.-detail::ease(t/.8))&&f.shot_index==0&&std::string(f.subtitle).empty());
    }
    Config c=sequence();CHECK(validate(c,error));CHECK(close(house_duration(c),5.)&&c.house_duration==.5);
    const auto cold=sample_house_cinematic(c,0.);CHECK(cold.camera.fade==1.&&cold.shot_index==0&&cold.shot_progress==0.);
    const auto half=sample_house_cinematic(c,.4);CHECK(close(half.camera.fade,.5));
    const auto ready=sample_house_cinematic(c,.8);CHECK(close(ready.camera.fade,0.));
    CHECK(std::string(cold.subtitle)==c.house_shots.front().subtitle);
    // Exact cuts jump composition only while fully black; fade windows have
    // no camera crossfade and never reverse the shot clock or wrap the sequence.
    for(const auto cut:{1.,2.5}){
        const auto before=sample_house_cinematic(c,cut-1e-7),at=sample_house_cinematic(c,cut),after=sample_house_cinematic(c,cut+1e-7);
        CHECK(before.camera.fade>.99999&&at.camera.fade==1.&&after.camera.fade>.99999);
        CHECK(before.shot_index+1==at.shot_index&&at.shot_index==after.shot_index&&at.shot_progress==0.);
        CHECK(!same(before.camera.eye,at.camera.eye));
    }
    const auto end=sample_house_cinematic(c,5.),held=sample_house_cinematic(c,500.);
    CHECK(end.shot_index==2&&end.shot_progress==1.&&end.camera.fade==0.);
    CHECK(same(end.camera.eye,c.house_shots.back().rail.eye_to)&&same(end.camera.target,c.house_shots.back().rail.target_to)&&end.camera.fov==.9);
    CHECK(same(held.camera.eye,end.camera.eye)&&same(held.camera.target,end.camera.target)&&held.shot_index==2&&held.shot_progress==1.&&held.camera.fade==0.);
    CHECK(end.camera.letterbox==0.&&held.camera.letterbox==0.);
    CHECK(close(sample_house_cinematic(c,4.2).camera.letterbox,.10));
    CHECK(close(sample_house_cinematic(c,4.6).camera.letterbox,.05));
    CHECK(sample_house_cinematic(c,4.8).camera.letterbox<.02);
    CHECK(same(sample_house_cinematic(c,-1.).camera.eye,cold.camera.eye));
    CHECK(same(sample_house_cinematic(c,std::numeric_limits<double>::quiet_NaN()).camera.eye,cold.camera.eye));
    for(bool reduced:{false,true}){
        unsigned prior=0;double progress=0;
        for(unsigned i=0;i<=1000;++i){
            const auto f=sample_house_cinematic(c,i*.005,reduced);const auto &shot=c.house_shots[f.shot_index];
            CHECK(f.shot_index>=prior&&f.shot_index<c.house_shots.size());
            CHECK(f.shot_index!=prior||f.shot_progress>=progress);
            CHECK(detail::valid(f.camera.eye)&&detail::valid(f.camera.target)&&std::isfinite(f.camera.fov));
            CHECK(f.camera.fade>=0&&f.camera.fade<=1&&f.camera.letterbox>=0&&f.camera.letterbox<=.10);
            CHECK(std::string(f.subtitle)==shot.subtitle);
            if(reduced)CHECK(same(f.camera.eye,shot.rail.eye_to)&&same(f.camera.target,shot.rail.target_to)&&close(f.camera.fov,shot.rail.fov_to));
            prior=f.shot_index;progress=f.shot_progress;
        }
    }
    // State advances by the authored sequence total, never the legacy duration.
    // Internal cuts preserve the chapter/stage and issue no checkpoint events.
    State state;CHECK(state.configure(c,error));auto title=state.sample();
    CHECK(same(title.camera.eye,c.house_shots.front().rail.eye_from)&&title.camera.fade==0.);
    auto f=tick(state,.01,true);CHECK(state.stage()==Stage::HouseEstablish&&(f.events&Started)&&f.camera.fade==1.);
    CHECK(std::string(f.subtitle)==c.house_shots.front().subtitle&&std::string(f.chapter)=="AN ORDINARY EVENING");
    tick(state,0);unsigned cuts=0,last_index=0,transitions=0;
    for(unsigned i=0;i<300&&state.stage()==Stage::HouseEstablish;++i){
        f=tick(state,1./60.);transitions+=(f.events&StageChanged)?1:0;
        if(state.stage()==Stage::HouseEstablish){
            CHECK(!(f.events&(CheckpointReached|ReleaseControls|TransportStarted|Started)));
            CHECK(std::string(f.chapter)=="AN ORDINARY EVENING"&&!f.controls_enabled&&f.cinematic);
            CHECK(close(f.stage_progress,state.elapsed()/5.));
            CHECK(std::string(f.subtitle)==c.house_shots[f.house_shot_index].subtitle);
            if(f.house_shot_index!=last_index){++cuts;CHECK(f.house_shot_index==last_index+1&&f.camera.fade>.98);last_index=f.house_shot_index;}
        }
    }
    CHECK(state.stage()==Stage::HouseExplore&&transitions==1&&cuts==2&&state.stats().checkpoints==1&&state.stats().skips==0);
    CHECK((f.events&CheckpointReached)&&(f.events&ReleaseControls)&&f.controls_enabled&&state.stats().active_seconds>5.&&state.stats().active_seconds<5.05);
    CHECK(same(f.camera.eye,end.camera.eye)&&same(f.camera.target,end.camera.target)&&close(f.camera.fov,end.camera.fov));
    // A pause at a cut freezes both the shot clock and fade, and held confirm
    // cannot become an accidental skip on resume or the first gameplay action.
    State paused;CHECK(paused.configure(c,error));tick(paused,.01,true);tick(paused,0);
    for(unsigned i=0;i<9;++i)tick(paused,.1);
    const auto before=paused.sample();const double time=paused.elapsed();
    for(unsigned i=0;i<20;++i){f=tick(paused,.1,true,true);CHECK(close(paused.elapsed(),time)&&close(f.camera.fade,before.camera.fade)&&same(f.camera.eye,before.camera.eye));}
    f=tick(paused,.01,true);CHECK((f.events&ReleaseControls)&&f.skip_progress==0&&f.house_shot_index==0);
    for(unsigned i=0;i<12;++i)f=tick(paused,.1,true);
    CHECK(paused.stage()==Stage::HouseEstablish&&f.house_shot_index==1&&paused.stats().skips==0&&f.skip_progress==0.);
    tick(paused,0);for(unsigned i=0;i<8;++i)f=tick(paused,.1,true);
    CHECK(paused.stage()==Stage::HouseExplore&&(f.events&Skipped)&&(f.events&ReleaseControls)&&paused.stats().skips==1&&paused.checkpoint()==Checkpoint::HouseStart);
    // Sequence metadata is never persisted in checkpoints: existing saves keep
    // exact version-one bytes, supported spawn pose and electrical state.
    const auto saved=state.stable_save();Config no_sequence=c;no_sequence.house_shots.clear();
    std::string sequence_bytes,legacy_bytes;CHECK(encode_save(saved,c,sequence_bytes,error));CHECK(encode_save(saved,no_sequence,legacy_bytes,error));CHECK(sequence_bytes==legacy_bytes);
    Save parsed;std::istringstream input(legacy_bytes);CHECK(parse_save(input,c,parsed,error)&&detail::same(saved.player,parsed.player));
    CHECK(state.restore(parsed,error)&&state.stage()==Stage::HouseExplore&&state.room_lights()==15);
    auto invalid=c;invalid.house_shots.resize(7,c.house_shots.front());CHECK(!validate(invalid,error));
    invalid=c;invalid.house_shots.assign(4,{c.house_shots.front().rail,12.,""});CHECK(!validate(invalid,error));
    invalid=c;invalid.house_shots.assign(3,{c.house_shots.front().rail,12.,""});invalid.house_shots.push_back({c.house_shots.back().rail,9.,""});CHECK(validate(invalid,error)&&house_duration(invalid)==45.);
    for(double duration:{0.,.99,12.01,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){
        invalid=c;invalid.house_shots[0].duration=duration;CHECK(!validate(invalid,error));
    }
    invalid=c;invalid.house_shots[0].rail.target_to=invalid.house_shots[0].rail.eye_to;CHECK(!validate(invalid,error));
    invalid=c;invalid.house_shots[0].subtitle.assign(241,'x');CHECK(!validate(invalid,error));
    for(const std::string &bad:{std::string("bad\ncaption"),std::string("bad\tcaption"),std::string("bad\0caption",11),std::string(1,char(127))}){
        invalid=c;invalid.house_shots[0].subtitle=bad;CHECK(!validate(invalid,error));
    }
    invalid=c;invalid.house_shots[0].subtitle="";CHECK(validate(invalid,error));
    invalid=c;invalid.house_shots[0].duration=0;const auto stable=paused.stage();CHECK(!paused.configure(invalid,error)&&paused.stage()==stable&&paused.config().house_shots.size()==3);
    std::cout<<"world_opening_home_cinematic: "<<checks<<" shot, fade, reduced motion, pause, handoff, legacy checkpoint and validation checks passed\n";
}

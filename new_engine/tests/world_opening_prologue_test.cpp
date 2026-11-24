#include "engine/world_opening_prologue.h"
#include <iostream>
#include <limits>
#include <sstream>
using namespace world_opening_prologue;
static unsigned checks=0;
static std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<" | "<<error<<'\n';return 1;}}while(0)
static Frame tick(State &s,double dt=1./60.,Vec p={},bool confirm=false,bool interact=false,bool paused=false,bool reduced=false,bool cancel=false){return s.tick({dt,p,paused,confirm,interact,reduced,cancel});}
static bool close(double a,double b,double tolerance=1e-8){return std::abs(a-b)<tolerance;}
static bool same(Vec a,Vec b){return close(a.x,b.x)&&close(a.y,b.y)&&close(a.z,b.z);}
static Frame release(State &s,Vec p={}){return tick(s,0,p);}
static Frame interact(State &s,Vec p){release(s,p);return tick(s,1./60.,p,false,true);}
int main(){
    Config c;CHECK(validate(c,error));State state;CHECK(state.configure(c,error));
    auto f=state.sample();CHECK(f.stage==Stage::Title&&!f.cinematic&&!f.controls_enabled);CHECK(std::string(f.chapter)=="VALOIS");
    // Pause cannot begin the story or queue a held start button for resume.
    f=tick(state,.1,{},true,false,true);CHECK(state.stage()==Stage::Title&&state.stats().active_seconds==0);
    f=tick(state,.1,{},true);CHECK(state.stage()==Stage::Title&&(f.events&ReleaseControls));
    release(state);f=tick(state,.01,{},true);CHECK(state.stage()==Stage::HouseEstablish&&(f.events&Started)&&(f.events&StageChanged));
    CHECK(f.release_controls&&f.cinematic&&!f.controls_enabled);
    // The press that began a film cannot skip it. A fresh release/hold is needed.
    for(unsigned i=0;i<90;++i)f=tick(state,1./60.,{},true);
    CHECK(state.stage()==Stage::HouseEstablish&&f.skip_progress==0);
    release(state);for(unsigned i=0;i<47;++i)f=tick(state,1./60.,{},true);
    CHECK(state.stage()==Stage::HouseEstablish&&f.skip_progress>.97&&f.skip_progress<1);
    f=tick(state,1./60.,{},true,true);CHECK(state.stage()==Stage::HouseExplore&&(f.events&Skipped)&&(f.events&CheckpointReached));
    CHECK(f.controls_enabled&&f.release_controls&&state.checkpoint()==Checkpoint::HouseStart&&state.stats().skips==1);
    CHECK(close(std::atan2(f.camera.target.x-f.camera.eye.x,-(f.camera.target.z-f.camera.eye.z)),c.house_spawn.yaw));
    CHECK(close(std::atan2(f.camera.target.y-f.camera.eye.y,std::hypot(f.camera.target.x-f.camera.eye.x,f.camera.target.z-f.camera.eye.z)),c.house_spawn.pitch));
    // A held controller A carried from skip must not inspect on handoff.
    for(unsigned i=0;i<5;++i)f=tick(state,.016,c.house_focus,true,true);
    CHECK(!state.inspected()&&state.stats().house_interactions==0);
    // Interactions use actual spatial and vertical proximity; NaN is no target.
    f=interact(state,{c.house_focus.x+20,c.house_focus.y,c.house_focus.z});CHECK(!state.inspected()&&!f.interaction_available);
    f=interact(state,{c.house_focus.x,c.house_focus.y+2,c.house_focus.z});CHECK(!state.inspected());
    f=interact(state,{std::numeric_limits<double>::quiet_NaN(),0,0});CHECK(!state.inspected());
    f=interact(state,c.house_focus);CHECK(state.inspected()&&(f.events&Inspected)&&(f.events&CheckpointReached));
    CHECK(state.checkpoint()==Checkpoint::HouseInspected&&state.stats().house_interactions==1&&f.objectives_complete==1);
    CHECK(std::string(f.objective)=="Open the front door.");
    const auto inspected=state.stable_save();CHECK(validate_save(inspected,c,error));
    // Holding interact near the door cannot consume a second objective.
    f=tick(state,.016,c.house_exit,false,true);CHECK(state.stage()==Stage::HouseExplore);
    f=interact(state,c.house_exit);CHECK(state.stage()==Stage::Transport&&(f.events&TransportStarted)&&f.cinematic&&!f.controls_enabled);
    CHECK(state.stats().house_interactions==2&&state.checkpoint()==Checkpoint::HouseInspected);
    // Pause freezes the story, fade, skip hold and elapsed/playable counters.
    release(state);for(unsigned i=0;i<12;++i)f=tick(state,1./60.,{},true);
    const double elapsed=state.elapsed(),skip=f.skip_progress,active=state.stats().active_seconds;
    const auto paused_camera=f.camera;
    for(unsigned i=0;i<30;++i){f=tick(state,.1,{},true,false,true);CHECK(close(state.elapsed(),elapsed)&&close(f.skip_progress,skip));CHECK(close(f.camera.fade,paused_camera.fade));}
    CHECK(close(state.stats().active_seconds,active));
    f=tick(state,1./60.,{},true);CHECK(f.skip_progress==0&&(f.events&ReleaseControls));
    // Natural transport delivers one pulse, stays black at relocation, and
    // never unlocks gameplay during the meadow reveal.
    unsigned pulses=0,arrivals=0;double last_fade=f.camera.fade;
    for(unsigned i=0;i<1000&&state.stage()==Stage::Transport;++i){
        f=tick(state,1./60.);pulses+=(f.events&TransportPulse)?1:0;arrivals+=(f.events&Arrived)?1:0;
        if(state.stage()==Stage::Transport){CHECK(f.camera.fade+1e-8>=last_fade);last_fade=f.camera.fade;}
    }
    CHECK(pulses==1&&arrivals==1&&state.stage()==Stage::MeadowReveal&&close(f.camera.fade,1));
    CHECK(!f.controls_enabled&&f.release_controls&&state.checkpoint()==Checkpoint::HouseInspected);
    // Resume during an interrupted arrival still returns to the last safe house
    // checkpoint, rather than saving an ungrounded film camera position.
    State interrupted;CHECK(interrupted.configure(c,error));CHECK(interrupted.restore(state.stable_save(),error));
    CHECK(interrupted.stage()==Stage::HouseExplore&&interrupted.inspected());
    double reveal_fade=1;for(unsigned i=0;i<700&&state.stage()==Stage::MeadowReveal;++i){
        f=tick(state,1./60.);if(state.stage()==Stage::MeadowReveal){CHECK(f.camera.fade<=reveal_fade+1e-8);reveal_fade=f.camera.fade;}
    }
    CHECK(state.stage()==Stage::MeadowExplore&&f.controls_enabled&&(f.events&CheckpointReached));
    CHECK(state.checkpoint()==Checkpoint::MeadowStart&&f.objectives_complete==2);
    CHECK(std::string(f.chapter)=="THE MEADOWS OF VALOIS");
    f=interact(state,{c.meadow_goal.x+50,c.meadow_goal.y,c.meadow_goal.z});CHECK(state.stage()==Stage::MeadowExplore);
    f=interact(state,c.meadow_goal);CHECK(state.stage()==Stage::Complete&&(f.events&Completed)&&(f.events&CheckpointReached));
    CHECK(state.stats().meadow_interactions==1&&f.controls_enabled&&f.objectives_complete==3);
    for(unsigned i=0;i<20;++i)interact(state,c.meadow_goal);
    CHECK(state.stats().meadow_interactions==1); // rewards/milestones are one-shot
    state.retry();CHECK(state.stage()==Stage::Complete&&state.checkpoint()==Checkpoint::Complete);
    // Each safe checkpoint round-trips; corrupted restores are transactional.
    for(auto checkpoint:{Checkpoint::HouseStart,Checkpoint::HouseInspected,Checkpoint::MeadowStart,Checkpoint::Complete}){
        auto saved=checkpoint_save(checkpoint,c);std::string encoded;CHECK(encode_save(saved,c,encoded,error));Save parsed;
        std::istringstream input(encoded);CHECK(parse_save(input,c,parsed,error));CHECK(parsed.checkpoint==checkpoint&&detail::same(parsed.player,saved.player));
        State restored;CHECK(restored.configure(c,error));CHECK(restored.restore(parsed,error));CHECK(restored.checkpoint()==checkpoint);
        CHECK(restored.stage()==(checkpoint==Checkpoint::HouseStart||checkpoint==Checkpoint::HouseInspected?Stage::HouseExplore:checkpoint==Checkpoint::MeadowStart?Stage::MeadowExplore:Stage::Complete));
        f=tick(restored,.01,checkpoint==Checkpoint::HouseStart?c.house_focus:c.house_exit,false,true);CHECK(!(f.events&Inspected)&&!(f.events&TransportStarted));
        const auto before=restored.stage();parsed.player.feet.x+=.01;CHECK(!restored.restore(parsed,error));CHECK(restored.stage()==before);
        Config changed=c;changed.house_spawn.feet.x+=1;changed.meadow_spawn.feet.x+=1;CHECK(!validate_save(saved,changed,error));
    }
    std::string good;CHECK(encode_save(inspected,c,good,error));
    const std::string corruptions[]={"",good+"extra",std::string(1100,'x'),"VALWOOD_OPENING 999\ncheckpoint 1\nplayer 0 0 0 0 0\n","VALWOOD_OPENING 1\ncheckpoint 4\nplayer 0 0 0 0 0\n","VALWOOD_OPENING 1\ncheckpoint 1\nplayer nan 0 0 0 0\n",good+std::string(1,'\0')};
    for(const auto &bytes:corruptions){Save untouched=inspected;std::istringstream input(bytes);CHECK(!parse_save(input,c,untouched,error));CHECK(untouched.checkpoint==inspected.checkpoint&&detail::same(untouched.player,inspected.player));}
    Save bad=inspected;bad.version=2;std::string untouched="unchanged";CHECK(!encode_save(bad,c,untouched,error)&&untouched=="unchanged");
    bad=inspected;bad.player.pitch=2;CHECK(!validate_save(bad,c,error));bad=inspected;bad.checkpoint=Checkpoint(9);CHECK(!validate_save(bad,c,error));
    // CPU cadence changes do not change the chapter. Film cameras stay inside
    // the authored rail bounds and reduced motion holds one stable composition.
    for(double step:{1./30.,1./60.,1./144.}){
        State paced;CHECK(paced.configure(c,error));tick(paced,step,{},true);release(paced);
        Vec stable={};for(unsigned i=0;i<2000&&paced.stage()==Stage::HouseEstablish;++i){
            f=tick(paced,step,{},false,false,false,true);
            if(paced.stage()!=Stage::HouseEstablish)break;
            if(i==0)stable=f.camera.eye;
            CHECK(same(stable,f.camera.eye));CHECK(f.camera.fov==c.house_rail.fov_to);
            CHECK(f.camera.fade>=0&&f.camera.fade<=1&&f.camera.letterbox>=0&&f.camera.letterbox<=.10);
        }
        CHECK(paced.stage()==Stage::HouseExplore&&std::abs(paced.stats().active_seconds-c.house_duration)<step*2+.000001);
    }
    State bounded;CHECK(bounded.configure(c,error));tick(bounded,.016,{},true);release(bounded);const double t0=bounded.elapsed();
    for(double dt:{-1.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()})tick(bounded,dt);
    CHECK(bounded.elapsed()==t0);f=tick(bounded,1000.,{},true);CHECK(bounded.stage()==Stage::HouseEstablish&&bounded.elapsed()<=t0+.100001&&f.skip_progress<.13);
    // A skip of each cinematic is independently armed, commits milestones,
    // and supplies the midpoint cue even when transport was skipped early.
    State skipped;CHECK(skipped.configure(c,error));tick(skipped,.016,{},true);release(skipped);
    for(unsigned i=0;i<8;++i)f=tick(skipped,.1,{},true);
    CHECK(skipped.stage()==Stage::HouseExplore&&skipped.stats().skips==1);
    interact(skipped,c.house_focus);interact(skipped,c.house_exit);release(skipped);pulses=0;
    for(unsigned i=0;i<8;++i){f=tick(skipped,.1,{},true);pulses+=(f.events&TransportPulse)?1:0;}
    CHECK(skipped.stage()==Stage::MeadowReveal&&pulses==1&&(f.events&Arrived));
    for(unsigned i=0;i<20;++i)tick(skipped,.1,{},true);
    CHECK(skipped.stage()==Stage::MeadowReveal); // held skip cannot chain
    release(skipped);for(unsigned i=0;i<8;++i)f=tick(skipped,.1,{},true);
    CHECK(skipped.stage()==Stage::MeadowExplore&&skipped.stats().skips==3&&skipped.checkpoint()==Checkpoint::MeadowStart);
    skipped.reset();CHECK(skipped.stage()==Stage::Title&&!skipped.inspected()&&skipped.stats().active_seconds==0&&skipped.stats().skips==0);
    // Invalid scene changes cannot alter a live story or create reversing rails.
    Config invalid=c;invalid.house_duration=std::numeric_limits<double>::quiet_NaN();CHECK(!bounded.configure(invalid,error)&&bounded.stage()==Stage::HouseEstablish);
    invalid=c;invalid.skip_seconds=0;CHECK(!validate(invalid,error));invalid=c;invalid.goal_radius=10;CHECK(!validate(invalid,error));
    invalid=c;invalid.house_rail.target_from=invalid.house_rail.eye_from;CHECK(!validate(invalid,error));
    invalid=c;invalid.house_rail.eye_from={0,0,0};invalid.house_rail.eye_to={0,0,0};invalid.house_rail.target_from={0,0,1};invalid.house_rail.target_to={0,0,-1};CHECK(!validate(invalid,error));
    invalid=c;invalid.meadow_goal.x+=501;CHECK(!validate(invalid,error));
    // The evening game-start route is an explicit project opt-in. Enum/save
    // values remain compatible with every existing version-one opening.
    CHECK(!c.game_start_story&&story_version==1);
    CHECK(unsigned(Stage::Title)==0&&unsigned(Stage::HouseExplore)==2&&unsigned(Stage::Transport)==3&&unsigned(Stage::Complete)==6&&unsigned(Stage::GameMenu)==7);
    CHECK(unsigned(Checkpoint::HouseStart)==0&&unsigned(Checkpoint::HouseInspected)==1&&unsigned(Checkpoint::Complete)==3);
    CHECK(unsigned(GameOpened)==(1u<<10)&&std::string(stage_name(Stage::GameMenu))=="game_menu");
    Config evening=c;evening.game_start_story=true;
    const Vec old_front_door=c.house_exit;
    evening.house_exit={-3,0,-2}; // author the console independently of the door
    State game;CHECK(game.configure(evening,error));
    f=tick(game,.016,{},true);CHECK(game.stage()==Stage::HouseEstablish&&std::string(f.chapter)=="AN ORDINARY EVENING");
    CHECK(std::string(f.subtitle).find("Overtime")!=std::string::npos);
    release(game);for(unsigned i=0;i<8;++i)f=tick(game,.1,{},true);
    CHECK(game.stage()==Stage::HouseExplore&&std::string(f.objective)=="Read the bills, then play a video game on the TV."&&std::string(f.interaction)=="Read the bills");
    f=interact(game,evening.house_exit);CHECK(!game.inspected()&&game.stage()==Stage::HouseExplore&&!f.interaction_available);
    f=interact(game,evening.house_focus);CHECK(game.inspected()&&(f.events&Inspected)&&(f.events&CheckpointReached));
    CHECK(game.checkpoint()==Checkpoint::HouseInspected&&f.objectives_complete==1&&std::string(f.objective)=="Play a video game: turn on the console by the TV.");
    CHECK(std::string(f.interaction)=="Turn on video game console"&&same(f.objective_position,evening.house_exit));
    f=interact(game,old_front_door);CHECK(game.stage()==Stage::HouseExplore&&!(f.events&(TransportStarted|GameOpened))&&!f.interaction_available);
    f=interact(game,evening.house_exit);CHECK(game.stage()==Stage::GameMenu&&(f.events&GameOpened)&&(f.events&StageChanged)&&(f.events&ReleaseControls));
    CHECK(!(f.events&TransportStarted)&&!f.cinematic&&!f.controls_enabled&&f.objectives_complete==1&&!f.interaction_available);
    CHECK(std::string(f.chapter)=="VALOIS"&&std::string(f.objective)=="Select NEW GAME on the TV to begin playing."&&std::string(f.interaction)=="NEW GAME");
    CHECK(same(f.camera.eye,evening.transport_rail.eye_from)&&same(f.camera.target,evening.transport_rail.target_from));
    const auto reduced_menu=game.sample({},true);CHECK(same(reduced_menu.camera.eye,f.camera.eye)&&close(reduced_menu.camera.fov,f.camera.fov));
    CHECK(game.checkpoint()==Checkpoint::HouseInspected&&game.stats().house_interactions==2);
    // Held E, A (both confirm and interact) or Enter cannot cross from the
    // console interaction into NEW GAME. Releasing only one input is too little.
    for(unsigned i=0;i<20;++i){f=tick(game,.1,evening.house_exit,true,true);CHECK(game.stage()==Stage::GameMenu&&!(f.events&TransportStarted)&&f.skip_progress==0);}
    f=tick(game,.1,evening.house_exit,true,false);CHECK(game.stage()==Stage::GameMenu);
    f=tick(game,.1,evening.house_exit,false,true);CHECK(game.stage()==Stage::GameMenu);
    f=tick(game,.1,evening.house_exit,true,true);CHECK(game.stage()==Stage::GameMenu);
    CHECK(game.elapsed()==0&&game.stats().playable_seconds<2.);
    // Menu cancellation requires a fresh deliberate edge, keeps inspection,
    // and releases gameplay input instead of leaking the action into the room.
    release(game);f=tick(game,.016,evening.house_exit,false,false,false,false,true);
    CHECK(game.stage()==Stage::HouseExplore&&(f.events&ReleaseControls)&&f.controls_enabled&&game.inspected());
    CHECK(!(f.events&(TransportStarted|Inspected|GameOpened))&&game.checkpoint()==Checkpoint::HouseInspected&&f.objectives_complete==1);
    for(unsigned i=0;i<5;++i)f=tick(game,.016,evening.house_exit,false,false,false,false,true);
    CHECK(game.stage()==Stage::HouseExplore&&game.stats().house_interactions==2);
    f=interact(game,evening.house_exit);CHECK(game.stage()==Stage::GameMenu&&(f.events&GameOpened)&&game.stats().house_interactions==3);
    // A menu press made during pause is consumed, and a previously armed menu
    // must be rearmed by a complete release after returning from pause.
    release(game);const auto menu_active=game.stats().active_seconds;
    f=tick(game,.1,{},true,true,true);CHECK(game.stage()==Stage::GameMenu&&!f.controls_enabled&&game.stats().active_seconds==menu_active);
    f=tick(game,.1,{},true,true);CHECK(game.stage()==Stage::GameMenu&&(f.events&ReleaseControls));
    for(unsigned i=0;i<20;++i)f=tick(game,.1,{},true,true);
    CHECK(game.stage()==Stage::GameMenu&&f.skip_progress==0);
    f=tick(game,.1,{},false,true);f=tick(game,.1,{},true,false);CHECK(game.stage()==Stage::GameMenu);
    release(game);f=tick(game,.016,{},true,true);
    CHECK(game.stage()==Stage::Transport&&(f.events&TransportStarted)&&!(f.events&GameOpened)&&f.cinematic&&!f.controls_enabled);
    CHECK(std::string(f.chapter)=="A NEW GAME"&&game.checkpoint()==Checkpoint::HouseInspected&&game.stats().house_interactions==3);
    for(unsigned i=0;i<20;++i)f=tick(game,.1,{},true,true);
    CHECK(game.stage()==Stage::Transport&&f.skip_progress==0); // confirmation never skips its own transport
    const auto transport_save=game.stable_save();CHECK(transport_save.checkpoint==Checkpoint::HouseInspected&&detail::same(transport_save.player,evening.house_spawn));
    game.retry();CHECK(game.stage()==Stage::HouseExplore&&game.inspected()&&game.checkpoint()==Checkpoint::HouseInspected);
    f=tick(game,.016,evening.house_exit,true,true);CHECK(game.stage()==Stage::HouseExplore); // retry requires release
    f=interact(game,evening.house_exit);CHECK(game.stage()==Stage::GameMenu);
    // The same press cannot both dismiss a pause and start the new game.
    release(game);f=tick(game,.1,{},false,false,true);
    f=tick(game,.1,{},true);CHECK(game.stage()==Stage::GameMenu&&(f.events&ReleaseControls));
    release(game);f=tick(game,.016,{},true);CHECK(game.stage()==Stage::Transport&&(f.events&TransportStarted));
    release(game);pulses=0;for(unsigned i=0;i<8;++i){f=tick(game,.1,{},true);pulses+=(f.events&TransportPulse)?1:0;}
    CHECK(game.stage()==Stage::MeadowReveal&&pulses==1&&(f.events&Arrived)&&f.objectives_complete==2);
    CHECK(std::string(f.subtitle)=="Where... where am I?");
    release(game);for(unsigned i=0;i<8;++i)f=tick(game,.1,{},true);
    CHECK(game.stage()==Stage::MeadowExplore&&game.checkpoint()==Checkpoint::MeadowStart&&f.controls_enabled);
    f=interact(game,evening.meadow_goal);CHECK(game.stage()==Stage::Complete&&(f.events&Completed)&&f.objectives_complete==3);
    // Room circuits and old version-one saves retain their exact meaning. A
    // saved console menu resumes safely at bills-read, never in a locked menu.
    for(auto checkpoint:{Checkpoint::HouseStart,Checkpoint::HouseInspected,Checkpoint::MeadowStart,Checkpoint::Complete}){
        State loaded;CHECK(loaded.configure(evening,error));auto saved=checkpoint_save(checkpoint,evening);saved.room_lights=5;
        std::string bytes;CHECK(encode_save(saved,evening,bytes,error));Save decoded;std::istringstream input(bytes);CHECK(parse_save(input,evening,decoded,error));
        CHECK(decoded.room_lights==5&&loaded.restore(decoded,error)&&loaded.room_lights()==5&&loaded.checkpoint()==checkpoint);
        CHECK(loaded.stage()==(checkpoint==Checkpoint::HouseStart||checkpoint==Checkpoint::HouseInspected?Stage::HouseExplore:checkpoint==Checkpoint::MeadowStart?Stage::MeadowExplore:Stage::Complete));
        f=tick(loaded,.016,evening.house_exit,true,true);CHECK(!(f.events&(Inspected|GameOpened|TransportStarted)));
        const auto legacy=bytes.substr(0,bytes.find("room_lights"));Save old;std::istringstream legacy_input(legacy);CHECK(parse_save(legacy_input,evening,old,error)&&old.room_lights==15);
        CHECK(validate_save(old,c,error)); // opt-in does not change checkpoint schema or spawn validation
    }
    State menu_save;CHECK(menu_save.configure(evening,error));CHECK(menu_save.restore(transport_save,error));
    CHECK(menu_save.set_room_lights(6));f=interact(menu_save,evening.house_exit);CHECK(menu_save.stage()==Stage::GameMenu);
    const auto safe_menu=menu_save.stable_save();CHECK(safe_menu.checkpoint==Checkpoint::HouseInspected&&safe_menu.room_lights==6&&validate_save(safe_menu,evening,error));
    CHECK(menu_save.restore(safe_menu,error)&&menu_save.stage()==Stage::HouseExplore&&menu_save.inspected()&&menu_save.room_lights()==6);
    f=interact(menu_save,evening.house_exit);release(menu_save);f=tick(menu_save,.016,{},false,false,true,false,true);
    f=tick(menu_save,.016,{},false,false,false,false,true);CHECK(menu_save.stage()==Stage::GameMenu); // paused cancel cannot leak
    release(menu_save);f=tick(menu_save,.016,{},false,false,false,false,true);CHECK(menu_save.stage()==Stage::HouseExplore&&(f.events&ReleaseControls));
    menu_save.reset();CHECK(menu_save.stage()==Stage::Title&&!menu_save.inspected()&&menu_save.room_lights()==15&&menu_save.stats().house_interactions==0);
    std::cout<<"world_opening_prologue: "<<checks<<" story, hold/release, pause, camera rail, spatial interaction, save corruption and checkpoint checks passed\n";
}

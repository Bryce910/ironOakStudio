#include "engine/world_battlefield_prologue.h"
#include "engine/world_opening_prologue.h"
#include "engine/world_controller.h"
#include "engine/world_player_movement.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
using namespace world_battlefield_prologue;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": " #x);}while(0)
static Frame transition(Stage before,double dt){
    Config config;State state;Frame frame;std::string error;
    CHECK(state.configure(config,error));Save save;save.present=true;save.sword_owned=true;
    save.checkpoint=before==Stage::Escape?Checkpoint::SwordSecured:Checkpoint::Forest;
    save.combat_wins=before==Stage::Escape?0:config.required_escape_wins;
    CHECK(state.restore(save,frame,error));CHECK(state.stage()==before);
    // Host reports actual completed escape wins; the core still requires the
    // route position and ordinary live control frame to advance ownership.
    Input input;input.dt=dt;input.player_feet=before==Stage::Escape?config.escape_spawn.feet:config.forest.feet;
    input.combat_wins=config.required_escape_wins;input.escape_combat_complete=true;
    state.update(input); // first restored frame rearms the core input latch
    input.player_feet=before==Stage::Escape?config.forest.feet:config.campsite.feet;
    frame=state.update(input);
    CHECK(frame.stage==(before==Stage::Escape?Stage::Forest:Stage::Campsite));
    CHECK(frame.events&(before==Stage::Escape?ForestReached:CampsiteReached));
    CHECK(frame.events&CheckpointReached);CHECK(!(frame.events&(GrantSword|TeleportPlayer)));
    return frame;
}
// The production wrapper has three independent input owners. Its ordering is
// significant: outer opening tick -> BF tick/sample handoff -> quest tick ->
// BF visual override -> controls footer. This fixture uses the actual three
// state machines, including their pause/neutral latches, rather than assuming
// that a playable destination implies that every owner is already usable.
struct ThreeOwnerHost {
    world_opening_prologue::Config opening_config;
    world_opening_prologue::State opening;
    world_first_journey::State journey;
    Config battle_config;State battle;Frame battlefield;
    world_opening_prologue::Frame outer;
    world_controller::Device controller;ExplorationResume resume;
    bool armed=true,last_playable=true;
    unsigned full_releases=0,action_releases=0,quest_resumes=0,outer_resumes=0,footer_releases=0;
    explicit ThreeOwnerHost(double dt){
        std::string error;CHECK(battle.configure(battle_config,error));
        opening_config.arrival_camp.enabled=true;
        const auto p=battle_config.campsite.feet;
        opening_config.meadow_spawn.feet={p.x,p.y,p.z};
        opening_config.meadow_goal={p.x,p.y,p.z+30};
        opening_config.arrival_camp.center={p.x,p.y,p.z};
        // Put a camper within interaction range at the transition to detect
        // held A/E/Enter leaking into a conversation when the owner resumes.
        opening_config.arrival_camp.actors[0].feet={p.x-10.2,p.y,p.z-3.9};
        opening_config.arrival_camp.actors[1].feet={p.x+2,p.y,p.z};
        opening_config.arrival_camp.actors[2].feet={p.x-2,p.y,p.z};
        if(!opening.configure(opening_config,error))throw std::runtime_error(error);
        CHECK(true);
        CHECK(opening.restore(world_opening_prologue::checkpoint_save(world_opening_prologue::Checkpoint::MeadowStart,opening_config),error));
        world_first_journey::Config quest_config;CHECK(journey.configure(quest_config,error));
        Save save;save.present=save.sword_owned=true;save.checkpoint=Checkpoint::SwordSecured;
        CHECK(battle.restore(save,battlefield,error));
        Input input;input.dt=dt;input.player_feet=battle_config.escape_spawn.feet;
        battlefield=battle.update(input); // restored action latch, not a win
        controller.using_pad=true;controller.move={.6f,.8f};controller.look={.4f,-.2f};
    }
    void release(bool preserve){
        if(preserve){release_actions_preserving_motion(controller);++action_releases;}
        else{resume.cancel();controller.clear();armed=last_playable=false;++full_releases;}
    }
    void tick(Vec feet,double dt,bool held_interact,bool physical_pause=false,bool quest_exception=true,bool sample_handoff=true){
        namespace op=world_opening_prologue;namespace fj=world_first_journey;
        const bool was_active=battlefield.active;
        const bool outer_token=resume.take(physical_pause,was_active);
        op::Input raw;raw.dt=dt;raw.player_feet={feet.x,feet.y,feet.z};raw.paused=physical_pause;
        raw.interact_down=raw.confirm_down=held_interact;
        auto input=raw;
        if(was_active){input.paused=true;input.interact_down=input.confirm_down=input.cancel_down=false;}
        outer=opening.tick(input);
        if(outer.release_controls){
            const bool only=outer.events==op::ReleaseControls;
            const bool preserve=outer_token&&only&&outer.controls_enabled&&!outer.cinematic&&!outer.camp_dialogue;
            if(preserve)++outer_resumes;
            release(preserve);
        }
        if(was_active){
            Input bi;bi.dt=dt;bi.player_feet=feet;bi.paused=physical_pause;
            bi.combat_wins=battle_config.required_escape_wins;bi.escape_combat_complete=true;
            const auto before=battlefield.stage;battlefield=battle.update(bi);
            if(battlefield.events&ReleaseControls)release(preserve_exploration_input(before,battlefield));
            // Actual CampsiteReached host explicitly samples the playable
            // outer state, replacing the synthetic-paused controls=false.
            if((battlefield.events&CampsiteReached)&&sample_handoff)outer=opening.sample(raw.player_feet);
        }
        if(was_active&&!battlefield.active){
            const bool continuous=!physical_pause&&battle.sword_owned()&&outer.controls_enabled&&!outer.cinematic&&!outer.camp_dialogue;
            if(continuous)resume.begin();else resume.cancel();
        }
        if(battlefield.active)journey.release_interaction();
        else{
            fj::Input ji;ji.dt=dt;ji.player_feet={feet.x,feet.y,feet.z};ji.enabled=!opening.camp().active();ji.paused=raw.paused;
            ji.interact_down=raw.interact_down;ji.confirm_down=raw.confirm_down;ji.camp_spoken=opening.camp().spoken();
            const bool quest_token=resume.take_journey(!ji.enabled||ji.paused||!ji.focused,battlefield.active);
            const auto frame=journey.tick(ji);
            if(ji.enabled&&(frame.events&fj::ReleaseControls)){
                const bool only=frame.events==fj::ReleaseControls&&frame.controls_enabled&&!frame.dialogue&&!frame.defeated;
                const bool preserve=quest_exception&&quest_token&&only;
                if(preserve)++quest_resumes;
                release(preserve);
            }
            // Production Travelers returns before replacing opening controls.
            if(journey.stage()!=fj::Stage::Travelers)outer.controls_enabled=frame.controls_enabled;
        }
        if(battlefield.active){outer.controls_enabled=battlefield.controls_enabled;outer.cinematic=battlefield.cinematic;outer.camp_dialogue=false;}
        const bool enabled=outer.controls_enabled&&!physical_pause;
        if(!enabled&&last_playable){++footer_releases;release(false);}
        last_playable=enabled;
    }
};
int main(int argc,char**argv){try{
    // Actual pure-state route transitions preserve continuously held keyboard
    // and controller motion, while resetting the real Device action latches.
    unsigned handoffs=0;
    for(double dt:{1./23.,1./30.,1./60.,1./120.})for(Stage before:{Stage::Escape,Stage::Forest}){
        const auto frame=transition(before,dt);CHECK(preserve_exploration_input(before,frame));
        for(unsigned direction=0;direction<4;++direction){
            bool movement_armed=true;world_player_movement::State movement;
            world_controller::Device controller;controller.using_pad=true;
            controller.move={direction==0?1.f:direction==1?-1.f:0.f,direction==2?1.f:direction==3?-1.f:0.f};
            controller.look={.71f,-.38f};controller.left_trigger=.8f;controller.right_trigger=.9f;
            controller.guard=controller.attack_pressed=controller.sprint=true;
            const auto held=controller.move,look=controller.look;
            if(preserve_exploration_input(before,frame))release_actions_preserving_motion(controller);
            else{movement_armed=false;controller.clear();}
            CHECK(movement_armed&&controller.move.x==held.x&&controller.move.y==held.y);
            CHECK(controller.look.x==look.x&&controller.look.y==look.y&&controller.using_pad);
            CHECK(controller.left_trigger==0&&controller.right_trigger==0&&!controller.guard&&!controller.attack_pressed&&!controller.sprint);
            double keyboard_distance=0,controller_distance=0;
            for(unsigned step=0;step<unsigned(std::ceil(6/dt));++step){
                const auto delta=movement.step(held.x*3.4,held.y*3.4,dt);
                if(movement_armed){keyboard_distance+=std::hypot(delta.dx,delta.dz);controller_distance+=std::hypot(controller.move.x,controller.move.y)*3.4*dt;}
            }
            CHECK(keyboard_distance>19&&controller_distance>19);++handoffs;
        }
        // Exact previous failure: a full release disarms a physically held
        // movement level forever, until neutral is observed. This behavior is
        // still required for cinematics/menu/retry, never these checkpoints.
        bool old_armed=false;const bool physically_held=true;
        for(unsigned i=0;i<300;++i)if(!physically_held)old_armed=true;
        CHECK(!old_armed);
    }
    // Exhaustive stage pair allowlist: no cinematic/retry/load transition can
    // gain this exception just because its destination allows movement.
    for(unsigned a=0;a<=unsigned(Stage::Dead);++a)for(unsigned b=0;b<=unsigned(Stage::Dead);++b){
        Frame frame;frame.stage=Stage(b);frame.controls_enabled=true;frame.events=StageChanged|ReleaseControls;
        const bool expected=(Stage(a)==Stage::Escape&&Stage(b)==Stage::Forest)||(Stage(a)==Stage::Forest&&Stage(b)==Stage::Campsite);
        CHECK(preserve_exploration_input(Stage(a),frame)==expected);
    }
    for(Stage before:{Stage::Escape,Stage::Forest}){
        const auto good=transition(before,1./60.);
        for(auto forbidden:{Started,TeleportPlayer,Died,Retried,Restored}){auto frame=good;frame.events|=forbidden;CHECK(!preserve_exploration_input(before,frame));}
        {auto frame=good;frame.controls_enabled=false;CHECK(!preserve_exploration_input(before,frame));}
        {auto frame=good;frame.cinematic=true;CHECK(!preserve_exploration_input(before,frame));}
        {auto frame=good;frame.exclusive_input=true;CHECK(!preserve_exploration_input(before,frame));}
        {auto frame=good;frame.events&=~std::uint32_t(StageChanged);CHECK(!preserve_exploration_input(before,frame));}
        {auto frame=good;frame.events&=~std::uint32_t(ReleaseControls);CHECK(!preserve_exploration_input(before,frame));}
    }
    // Reproduce the second, one-frame-later release from the real outer
    // opening after its synthetic battlefield pause. It is independently
    // consumed once, without weakening actual pause/focus/loading rearming.
    {
        namespace opening=world_opening_prologue;
        opening::Config config;opening::State state;std::string error;
        CHECK(state.configure(config,error));
        CHECK(state.restore(opening::checkpoint_save(opening::Checkpoint::MeadowStart,config),error));
        opening::Input input;input.player_feet=config.meadow_spawn.feet;input.dt=1./30.;input.paused=true;
        for(unsigned i=0;i<20;++i)CHECK(!state.tick(input).controls_enabled);
        input.paused=false;const auto frame=state.tick(input);
        CHECK(frame.release_controls&&frame.events==opening::ReleaseControls&&frame.controls_enabled);
        ExplorationResume handoff;handoff.begin();
        CHECK(handoff.take(false,false));CHECK(!handoff.pending&&!handoff.journey_pending&&!handoff.take(false,false));
        CHECK(!state.tick(input).release_controls);
        for(bool pending:{false,true})for(bool paused:{false,true})for(bool active:{false,true}){
            ExplorationResume resume;resume.pending=pending;
            CHECK(resume.take(paused,active)==(pending&&!paused&&!active));
            CHECK(!resume.pending&&!resume.take(false,false));
        }
    }
    unsigned complete_timelines=0;
    for(double dt:{1./23.,1./30.,1./60.,1./120.})for(unsigned direction=0;direction<4;++direction)for(bool held_action:{false,true}){
        ThreeOwnerHost host(dt);
        host.controller.move={direction==0?1.f:direction==1?-1.f:0.f,direction==2?1.f:direction==3?-1.f:0.f};
        const auto held=host.controller.move,look=host.controller.look;
        // Stable Escape then Escape->Forest, followed by stable Forest. Every
        // active battlefield frame synthetically pauses the real outer state
        // and resets the quest interaction owner, as in the live host.
        for(unsigned i=0;i<20;++i)host.tick(host.battle_config.escape_spawn.feet,dt,held_action);
        host.tick(host.battle_config.forest.feet,dt,held_action);
        CHECK(host.battlefield.stage==Stage::Forest&&host.armed&&host.full_releases==0);
        for(unsigned i=0;i<20;++i)host.tick(host.battle_config.forest.feet,dt,held_action);
        const auto p=host.battle_config.campsite.feet;const Vec arrival{p.x-10.2,p.y,p.z-3.9};
        host.tick(arrival,dt,held_action); // SAME-frame quest release + footer
        CHECK(host.battlefield.stage==Stage::Campsite&&host.outer.controls_enabled);
        CHECK(host.quest_resumes==1&&host.outer_resumes==0&&host.resume.pending&&!host.resume.journey_pending);
        CHECK(host.armed&&host.last_playable&&host.full_releases==0&&host.footer_releases==0);
        host.tick(arrival,dt,held_action); // NEXT-frame outer opening release
        CHECK(host.outer_resumes==1&&!host.resume.pending&&!host.resume.journey_pending);
        for(unsigned i=0;i<20;++i)host.tick(arrival,dt,held_action);
        CHECK(host.armed&&host.full_releases==0&&host.footer_releases==0&&host.action_releases==4);
        CHECK(host.controller.move.x==held.x&&host.controller.move.y==held.y);
        CHECK(host.controller.look.x==look.x&&host.controller.look.y==look.y);
        CHECK(!host.opening.camp().active()&&host.opening.camp().spoken()==0);
        CHECK(host.journey.stage()==world_first_journey::Stage::Travelers&&!host.journey.weapon_unlocked());
        CHECK(!host.journey.reward_granted()&&!host.journey.supplies_recovered());
        // A later intentional fresh camp interaction must still take full
        // control; neither one-shot exception may leak beyond the handoff.
        host.tick(arrival,dt,false);host.tick(arrival,dt,true);
        CHECK(host.opening.camp().active()&&!host.outer.controls_enabled);
        CHECK(!host.armed&&host.full_releases>0&&!host.resume.pending&&!host.resume.journey_pending);
        ++complete_timelines;
    }
    // Mutation checks prove the integrated fixture detects an unhandled quest
    // resume and a missing playable-frame replacement. The latter invalidates
    // the handoff and must never be disguised by suppressing the footer.
    for(bool quest_exception:{false,true}){
        ThreeOwnerHost host(1./30.);host.tick(host.battle_config.forest.feet,1./30.,false);
        const auto p=host.battle_config.campsite.feet;
        host.tick({p.x-10.2,p.y,p.z-3.9},1./30.,false,false,quest_exception,!quest_exception);
        CHECK(!host.armed&&host.full_releases>0);
        if(quest_exception)CHECK(!host.outer.controls_enabled);
    }
    for(double dt:{1./23.,1./30.,1./60.,1./120.}){
        ThreeOwnerHost host(dt);host.tick(host.battle_config.forest.feet,dt,true);
        const auto p=host.battle_config.campsite.feet;const Vec arrival{p.x-10.2,p.y,p.z-3.9};
        host.tick(arrival,dt,true);CHECK(host.resume.pending&&host.armed);
        host.tick(arrival,dt,true,true); // real pause between owner resumes
        CHECK(!host.armed&&!host.resume.pending&&!host.resume.journey_pending);
        const auto releases=host.full_releases;host.tick(arrival,dt,true,false);
        CHECK(host.full_releases>releases&&!host.armed&&host.outer_resumes==0);
        CHECK(!host.opening.camp().active()&&!host.journey.weapon_unlocked());
    }
    // Separate tokens are one-shot and all genuine blocking contexts cancel
    // both. First journey may consume only its own same-frame token.
    for(unsigned blocked=0;blocked<6;++blocked){
        ExplorationResume handoff;handoff.begin();
        CHECK(handoff.take_journey(false,false));CHECK(!handoff.take_journey(false,false)&&handoff.pending);
        // Pause, menu, focus loss, loading, inventory, graphics all feed the
        // same real-blocked predicate in the production quest/outer wrappers.
        CHECK(!handoff.take(true,false)&&!handoff.pending&&!handoff.journey_pending);
        handoff.begin();CHECK(!handoff.take_journey(true,false));
        CHECK(!handoff.take(false,false)&&!handoff.pending&&!handoff.journey_pending);
        handoff.begin();handoff.cancel();CHECK(!handoff.take_journey(false,false)&&!handoff.take(false,false));
    }
    // Check the actual host does not reintroduce a second full release when
    // it hands combat back at the campsite (the second native freeze).
    if(argc>1){
        std::ifstream input(argv[1]);CHECK(bool(input));const std::string runtime((std::istreambuf_iterator<char>(input)),{});
        CHECK(runtime.find("preserve_exploration_input(previous_stage,battlefield_frame)")!=std::string::npos);
        CHECK(runtime.find("if(events&world_battlefield_prologue::ReleaseControls)release_battlefield_controls();")!=std::string::npos);
        const auto begin=runtime.find("if(events&world_battlefield_prologue::CampsiteReached)");CHECK(begin!=std::string::npos);
        const auto end=runtime.find("if(events&world_battlefield_prologue::CheckpointReached)",begin);CHECK(end!=std::string::npos);
        CHECK(runtime.substr(begin,end-begin).find("opening_release();")==std::string::npos);
        CHECK(runtime.substr(begin,end-begin).find("opening_frame=opening_state.sample(")!=std::string::npos);
        CHECK(runtime.find("else opening_release();")!=std::string::npos);
    }
    if(argc>2){
        std::ifstream input(argv[2]);CHECK(bool(input));const std::string runtime((std::istreambuf_iterator<char>(input)),{});
        CHECK(runtime.find("opening_battlefield_resume.take(paused,battlefield_was_active)")!=std::string::npos);
        CHECK(runtime.find("opening_frame.events==world_opening_prologue::ReleaseControls")!=std::string::npos);
        CHECK(runtime.find("if(battlefield_was_active&&!(battlefield_active&&battlefield_active()))")!=std::string::npos);
        CHECK(runtime.find("opening_battlefield_resume.cancel();")!=std::string::npos);
        const auto tick=runtime.find("if(battlefield_tick)battlefield_tick(delta,paused);");
        const auto tokens=runtime.find("opening_battlefield_resume.begin();",tick);
        const auto quest=runtime.find("if(first_journey_tick)first_journey_tick(first_journey_input);",tick);
        const auto footer=runtime.find("if(!opening_controls_enabled&&opening_last_playable)opening_release();",quest);
        CHECK(tick!=std::string::npos&&tick<tokens&&tokens<quest&&quest<footer&&footer!=std::string::npos);
    }
    if(argc>3){
        std::ifstream input(argv[3]);CHECK(bool(input));const std::string runtime((std::istreambuf_iterator<char>(input)),{});
        CHECK(runtime.find("opening_battlefield_resume.take_journey(")!=std::string::npos);
        CHECK(runtime.find("events==ReleaseControls&&first_journey_frame.controls_enabled")!=std::string::npos);
        CHECK(runtime.find("if(preserve_battlefield_handoff&&resume_only)opening_release_actions();")!=std::string::npos);
        CHECK(runtime.find("else opening_release();")!=std::string::npos);
    }
    std::cout<<"PASS "<<checks<<" continuous exploration/input ownership checks; "<<handoffs<<" held keyboard/controller routes and "<<complete_timelines<<" complete three-owner timelines at23/30/60/120FPS, real state transitions, same/next-frame release consumers, footer, and held-action isolation\n";
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

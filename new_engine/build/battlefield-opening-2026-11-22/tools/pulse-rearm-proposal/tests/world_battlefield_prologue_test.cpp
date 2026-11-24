#include "engine/world_battlefield_prologue.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace world_battlefield_prologue;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": " #x);}while(0)
struct Run {
    Config config;State state;Input in;Frame last;unsigned grants=0,contacts=0,tackles=0;
    Run(Difficulty d=Difficulty::Medium,double dt=1./60.){std::string error;CHECK(state.configure(config,error));in.dt=dt;in.player_feet=config.arrival.feet;last=state.start(d);CHECK(last.events&Started);CHECK(last.events&TeleportPlayer);CHECK(last.teleport_pose.feet.x==config.arrival.feet.x);step();}
    Frame step(){last=state.update(in);grants+=bool(last.events&GrantSword);contacts+=bool(last.events&StabContact);tackles+=bool(last.events&TackleImpact);return last;}
    void wait(double seconds){for(unsigned i=0;i<unsigned(std::ceil(seconds/in.dt));++i)step();}
    void until(Stage s,unsigned bound=3000){for(unsigned i=0;i<bound&&state.stage()!=s;++i)step();CHECK(state.stage()==s);}
    void at_duck(){until(Stage::FleeToAmbush);in.player_feet=config.ambush.feet;CHECK(step().events&AmbushStarted);CHECK(state.stage()==Stage::Duck);while(state.elapsed()<config.duck_telegraph)step();CHECK(last.duck_window);}
    void dodge(){at_duck();in.duck_down=true;CHECK(step().events&Ducked);CHECK(state.stage()==Stage::Tackle);in.duck_down=false;until(Stage::Struggle);CHECK(tackles==1);}
    void steal(bool hold=false){dodge();in.hold_struggle=hold;if(hold){in.struggle_down=false;step();in.struggle_down=true;until(Stage::Stab);}else{for(unsigned n=0;n<required_presses(config,last.difficulty);++n){in.struggle_down=false;step();in.struggle_down=true;step();if(state.stage()==Stage::Stab)break;while(in.dt<config.press_interval&&state.elapsed()>0&&last.events&StrugglePress){in.struggle_down=false;step();}}
      // At fast rates the cadence floor intentionally ignores machine-speed taps.
      for(unsigned n=0;n<40&&state.stage()==Stage::Struggle;++n){in.struggle_down=false;wait(.09);in.struggle_down=true;step();}}
      CHECK(state.stage()==Stage::Stab);CHECK(!state.sword_owned());in.struggle_down=false;}
    void escaped(){steal();until(Stage::Escape);CHECK(grants==1&&contacts==1);CHECK(state.sword_owned());}
};
int main(){try{
    Config c;std::string error;CHECK(validate(c,error));CHECK(c.required_escape_wins>=2&&c.required_escape_wins<=4);
    {const auto final_root=detail::offset(c.ambush,-.75,0,1.46);CHECK(detail::distance(final_root,c.escape_spawn.feet)<.000001);Config bad=c;bad.escape_spawn.feet.x+=3;CHECK(!validate(bad,error));}
    for(unsigned i=0;i<11;++i){Config b=c;switch(i){case 0:b.stab_contact=b.stab_seconds;break;case 1:b.required_escape_wins=0;break;case 2:b.required_escape_wins=5;break;case 3:b.duck_hard=2;break;case 4:b.presses_medium=100;break;case 5:b.hold_hard=20;break;case 6:b.arrival.feet.x=std::numeric_limits<double>::quiet_NaN();break;case 7:b.forest=c.ambush;break;case 8:b.max_frame_dt=30;break;case 9:b.struggle_easy=2;break;case 10:b.press_interval=.001;break;}CHECK(!validate(b,error));}
    // Complete real-input sequence at 60, 30, 15 and 5fps for each difficulty.
    for(double fps:{60.,30.,15.,5.})for(Difficulty d:{Difficulty::Easy,Difficulty::Medium,Difficulty::Hard}){
        Run r(d,1./fps);r.escaped();const Save sword=r.state.save();CHECK(sword.checkpoint==Checkpoint::SwordSecured&&sword.sword_owned&&sword.combat_wins==0);
        std::string bytes;CHECK(encode_save(sword,c,bytes,error));CHECK(bytes.size()<512);Save restored;CHECK(decode_save(bytes,c,restored,error));State clone;Frame frame;CHECK(clone.configure(c,error));CHECK(clone.restore(restored,frame,error));CHECK(clone.stage()==Stage::Escape&&frame.events&EnsureSword&&!(frame.events&GrantSword));CHECK(frame.events&TeleportPlayer);
        // Spatial shortcut, one receipt, or a completion flag alone cannot win.
        r.in.player_feet=c.forest.feet;r.wait(1);CHECK(r.state.stage()==Stage::Escape);
        r.in.combat_wins=1;r.in.escape_combat_complete=true;r.step();CHECK(r.state.stage()==Stage::Escape);
        r.in.combat_wins=c.required_escape_wins;r.in.escape_combat_complete=false;r.step();CHECK(r.state.stage()==Stage::Escape);
        r.in.escape_combat_complete=true;r.in.player_feet=c.ambush.feet;r.step();CHECK(r.state.stage()==Stage::Escape);
        r.in.player_feet=c.forest.feet;CHECK(r.step().events&ForestReached);CHECK(r.state.stage()==Stage::Forest);CHECK(r.last.controls_enabled&&!r.last.exclusive_input);
        CHECK(r.state.save().combat_wins==c.required_escape_wins);r.in.player_feet=c.campsite.feet;CHECK(r.step().events&CampsiteReached);CHECK(r.last.completed&&!r.last.active&&r.last.sword_owned);CHECK(r.grants==1&&r.contacts==1);const auto complete=r.state.save();CHECK(complete.checkpoint==Checkpoint::Campsite);CHECK(encode_save(complete,c,bytes,error));CHECK(decode_save(bytes,c,restored,error));CHECK(clone.restore(restored,frame,error)&&frame.completed);
    }
    // The final recovery steps smoothly beside the corpse, preserving the
    // grab/stab root until recovery and matching the playable capsule endpoint.
    {Run r;r.steal();const auto grab=detail::offset(c.ambush,-.10,0,1.46);CHECK(detail::distance(r.last.player_pose.feet,grab)<1e-9);r.until(Stage::Stand);CHECK(detail::distance(r.last.player_pose.feet,grab)<1e-9);Vec previous=r.last.player_pose.feet;while(r.state.stage()==Stage::Stand){r.step();if(r.state.stage()==Stage::Stand){const auto expected=detail::mix(grab,c.escape_spawn.feet,detail::ease(r.last.stage_progress));CHECK(detail::distance(expected,r.last.player_pose.feet)<1e-9);CHECK(detail::distance(previous,r.last.player_pose.feet)<.03);previous=r.last.player_pose.feet;}}CHECK(r.state.stage()==Stage::Escape&&r.last.events&EscapeStarted);CHECK(detail::distance(previous,c.escape_spawn.feet)<.01);}
    // A held duck from locomotion cannot satisfy the surprise, then retry is local.
    {Run r;r.until(Stage::FleeToAmbush);r.in.duck_down=true;r.step();r.in.player_feet=c.ambush.feet;r.step();r.until(Stage::Dead);CHECK(r.last.death_reason==DeathReason::MissedDuck&&r.grants==0);CHECK(!r.last.controls_enabled);r.in.retry_down=true;r.step();r.wait(1);CHECK(r.state.stage()==Stage::Dead);r.in.retry_down=false;r.step();r.in.retry_down=true;CHECK(r.step().events&Retried);CHECK(r.state.stage()==Stage::FleeToAmbush);CHECK(r.last.teleport_pose.feet.x==c.before_ambush.feet.x);CHECK(!r.state.sword_owned());}
    // No taps or one indefinitely held tap loses the struggle; holding is opt-in.
    for(bool held:{false,true}){Run r;r.dodge();r.in.struggle_down=held;r.until(Stage::Dead);CHECK(r.last.death_reason==DeathReason::Overpowered);CHECK(r.last.struggle_presses==0);CHECK(r.grants==0);r.in.retry_down=false;r.wait(1);r.in.retry_down=true;CHECK(r.step().events&Retried);CHECK(r.state.checkpoint()==Checkpoint::BeforeAmbush);}
    // A repeated A down level is one press, never an automatic mashing stream.
    {Run r;r.dodge();r.in.struggle_down=true;r.step();CHECK(r.last.struggle_presses==1);r.wait(2);CHECK(r.last.struggle_presses==1&&r.state.stage()==Stage::Struggle);r.in.struggle_down=false;r.step();r.in.struggle_down=true;r.step();CHECK(r.last.struggle_presses==2);}
    // SDL may observe a complete down/up tap between state polls. A buffered
    // physical edge must work even though the current held snapshot is up.
    {Run r;r.at_duck();r.in.duck_pressed=true;CHECK(r.step().events&Ducked);CHECK(!r.in.duck_down&&r.state.stage()==Stage::Tackle);r.in.duck_pressed=false;r.until(Stage::Struggle);}
    {Run r;r.until(Stage::FleeToAmbush);r.in.player_feet=c.ambush.feet;r.step();r.in.duck_pressed=true;r.step();CHECK(r.state.stage()==Stage::Duck&&!(r.last.events&Ducked));r.in.duck_pressed=false;while(r.state.elapsed()<c.duck_telegraph){r.step();}CHECK(r.state.stage()==Stage::Duck);r.in.duck_pressed=true;CHECK(r.step().events&Ducked);}
    for(double fps:{60.,15.,5.})for(Difficulty d:{Difficulty::Easy,Difficulty::Medium,Difficulty::Hard}){
        Run r(d,1./fps);r.dodge();unsigned received=0;
        for(unsigned n=0;n<required_presses(c,d);++n){
            CHECK(!r.in.struggle_down);r.in.struggle_pressed=true;
            CHECK(r.step().events&StrugglePress);++received;r.in.struggle_pressed=false;
            if(r.state.stage()==Stage::Struggle)r.wait(.12);
        }
        CHECK(received==required_presses(c,d));CHECK(r.state.stage()==Stage::Stab);
        r.until(Stage::Escape);CHECK(r.grants==1&&r.contacts==1);
    }
    // Event and level observations of one physical press count once. Further
    // held frames do not count; a genuinely new buffered press after a release
    // between polls can count even if both held snapshots happen to be down.
    {Run r;r.dodge();r.in.struggle_down=r.in.struggle_pressed=true;CHECK(r.step().events&StrugglePress);CHECK(r.last.struggle_presses==1);r.in.struggle_pressed=false;r.wait(.2);CHECK(r.last.struggle_presses==1);r.in.struggle_pressed=true;CHECK(r.step().events&StrugglePress);CHECK(r.last.struggle_presses==2);r.in.struggle_pressed=r.in.struggle_down=false;r.wait(.1);CHECK(r.last.struggle_presses==2);r.in.struggle_pressed=true;CHECK(r.step().events&StrugglePress);CHECK(r.last.struggle_presses==3);}
    // Buffered presses received while suspended, on the resume frame, or from
    // a carried held button never leak through the pause/focus/loading/device
    // boundary. A neutral release and new tap restore normal control.
    for(unsigned mode=0;mode<4;++mode){
        Run r;r.dodge();r.in.struggle_down=true;r.step();CHECK(r.last.struggle_presses==1);
        const double before=r.state.elapsed();r.in.duck_pressed=r.in.struggle_pressed=r.in.retry_pressed=true;
        if(mode==0)r.in.paused=true;
        if(mode==1)r.in.focused=false;
        if(mode==2)r.in.loading=true;
        if(mode==3){r.in.uses_controller=true;r.in.controller_connected=false;}
        r.wait(.5);CHECK(r.state.elapsed()==before&&r.last.struggle_presses==1);CHECK(!(r.last.events&StrugglePress));
        r.in.paused=false;r.in.focused=true;r.in.loading=false;r.in.controller_connected=true;
        r.step();CHECK(r.state.elapsed()==before&&r.last.struggle_presses==1);CHECK(!(r.last.events&StrugglePress));
        r.in.duck_pressed=r.in.struggle_pressed=r.in.retry_pressed=false;r.wait(.1);CHECK(r.last.struggle_presses==1);
        r.in.struggle_pressed=true;CHECK(r.step().events&StrugglePress);CHECK(r.last.struggle_presses==2);
        r.in.struggle_pressed=false;r.wait(.1);CHECK(r.last.struggle_presses==2);
        r.in.struggle_pressed=r.in.struggle_down=false;r.step();r.wait(.1);
        r.in.struggle_pressed=true;CHECK(r.step().events&StrugglePress);CHECK(r.last.struggle_presses==3);
    }
    {Run r;r.dodge();r.in.paused=true;r.in.struggle_pressed=true;r.step();CHECK(r.last.struggle_presses==0);r.in.paused=false;r.step();CHECK(r.last.struggle_presses==0);r.in.struggle_pressed=false;r.wait(.1);CHECK(r.last.struggle_presses==0);r.in.struggle_pressed=true;CHECK(r.step().events&StrugglePress);}
    // Accessibility hold measures actual held time, not a train of brief
    // captured event pulses whose physical button is already released.
    {Run r;r.dodge();r.in.hold_struggle=true;for(unsigned n=0;n<5;++n){r.in.struggle_pressed=true;r.step();CHECK(!(r.last.events&StrugglePress));CHECK(r.last.struggle_progress==0&&r.state.stage()==Stage::Struggle);r.in.struggle_pressed=false;r.wait(.1);}}
    // An event in the frame that enters a new QTE belongs to the old stage;
    // it must not be queued again as the first duck or struggle action.
    {Run r;r.until(Stage::FleeToAmbush);r.in.player_feet=c.ambush.feet;r.in.duck_pressed=true;CHECK(r.step().events&AmbushStarted);CHECK(!(r.last.events&Ducked)&&r.state.stage()==Stage::Duck);r.in.duck_pressed=false;while(r.state.elapsed()<c.duck_telegraph)r.step();CHECK(r.state.stage()==Stage::Duck);r.in.duck_pressed=true;CHECK(r.step().events&Ducked);r.in.duck_pressed=false;
     while(r.state.elapsed()+r.in.dt<c.tackle_seconds){r.step();}
     r.in.struggle_pressed=true;CHECK(r.step().events&StruggleStarted);CHECK(!(r.last.events&StrugglePress)&&r.last.struggle_presses==0);r.in.struggle_pressed=false;r.wait(.1);CHECK(r.last.struggle_presses==0);r.in.struggle_pressed=true;CHECK(r.step().events&StrugglePress);CHECK(r.last.struggle_presses==1);}
    {Run r;r.at_duck();r.in.duck_pressed=true;r.in.struggle_down=true;r.step();r.in.duck_pressed=false;
     while(r.state.elapsed()+r.in.dt<c.tackle_seconds){r.step();}
     r.in.struggle_pressed=true;CHECK(r.step().events&StruggleStarted);CHECK(!(r.last.events&StrugglePress)&&r.last.struggle_presses==0);
     r.in.struggle_pressed=false;r.wait(.1);CHECK(r.last.struggle_presses==0);
     r.in.struggle_pressed=true;CHECK(r.step().events&StrugglePress);CHECK(r.last.struggle_presses==1);
     r.in.struggle_pressed=false;r.wait(.1);CHECK(r.last.struggle_presses==1);
     r.in.struggle_down=false;r.step();r.in.struggle_pressed=true;CHECK(r.step().events&StrugglePress);CHECK(r.last.struggle_presses==2);}
    // A stage-scoped, non-repeat buffered DOWN proves a new physical press
    // even when release/repress both happened between held snapshots.
    {Run r;r.until(Stage::FleeToAmbush);r.in.duck_down=true;r.step();r.in.player_feet=c.ambush.feet;r.in.duck_pressed=true;r.step();CHECK(r.state.stage()==Stage::Duck&&!(r.last.events&Ducked));r.in.duck_pressed=false;while(r.state.elapsed()<c.duck_telegraph){r.step();}CHECK(r.state.stage()==Stage::Duck);r.in.duck_pressed=true;CHECK(r.step().events&Ducked);}
    {Run r;r.at_duck();r.in.retry_down=true;r.until(Stage::Dead);r.wait(1);CHECK(r.state.stage()==Stage::Dead&&!(r.last.events&Retried));r.in.retry_pressed=true;CHECK(r.step().events&Retried);r.in.retry_pressed=false;r.step();CHECK(!(r.last.events&Retried));}
    // A brief retry tap also survives between polls, but neither the pulse
    // that caused death nor an early retry can carry through to a later frame.
    {Run r;r.at_duck();r.until(Stage::Dead);r.in.retry_pressed=true;r.step();CHECK(r.state.stage()==Stage::Dead&&!(r.last.events&Retried));r.in.retry_pressed=false;r.wait(1);CHECK(r.state.stage()==Stage::Dead);r.in.retry_pressed=true;CHECK(r.step().events&Retried);CHECK(!r.in.retry_down&&r.state.stage()==Stage::FleeToAmbush);}
    {Run r;r.dodge();r.in.player_dead=r.in.retry_pressed=true;CHECK(r.step().events&Died);CHECK(!(r.last.events&Retried));r.in.player_dead=r.in.retry_pressed=false;r.wait(1);CHECK(r.state.stage()==Stage::Dead);r.in.retry_down=r.in.retry_pressed=true;CHECK(r.step().events&Retried);}
    // Accessibility hold succeeds on all difficulties and only after fresh release.
    for(Difficulty d:{Difficulty::Easy,Difficulty::Medium,Difficulty::Hard}){Run r(d);r.steal(true);r.until(Stage::Escape);CHECK(r.grants==1);}
    {Run r;r.dodge();r.in.hold_struggle=true;r.in.struggle_down=true;r.step();const double hold_before=r.last.struggle_progress;r.in.paused=true;r.step();const double t=r.state.elapsed();r.wait(30);CHECK(r.state.elapsed()==t);r.in.paused=false;r.step();r.wait(2);CHECK(r.last.struggle_progress==hold_before);r.in.struggle_down=false;r.step();r.in.struggle_down=true;r.until(Stage::Stab);}
    // Paused, unfocused, loading and disconnected controller frames freeze all
    // clocks. Reconnection carrying a held duck is not a fresh defensive action.
    for(unsigned mode=0;mode<4;++mode){Run r;r.at_duck();const double before=r.state.elapsed();r.in.duck_down=true;if(mode==0)r.in.paused=true;if(mode==1)r.in.focused=false;if(mode==2)r.in.loading=true;if(mode==3){r.in.uses_controller=true;r.in.controller_connected=false;}r.wait(10);CHECK(r.state.stage()==Stage::Duck&&r.state.elapsed()==before);r.in.paused=false;r.in.focused=true;r.in.loading=false;r.in.controller_connected=true;r.step();r.step();CHECK(r.state.stage()==Stage::Duck);r.in.duck_down=false;r.step();r.in.duck_down=true;CHECK(r.step().events&Ducked);}
    // Timers are finite and bounded across debugger hitches / corrupt delta time.
    {Run r;r.at_duck();const double before=r.state.elapsed();r.in.dt=std::numeric_limits<double>::quiet_NaN();r.step();CHECK(r.state.elapsed()==before);r.in.dt=-500;r.step();CHECK(r.state.elapsed()==before);r.in.dt=500;r.step();CHECK(r.state.elapsed()<=before+c.max_frame_dt+1e-9);}
    // Real combat death resets the opponent group at its local sword checkpoint;
    // no item duplication or timed escape win is possible.
    {Run r;r.escaped();r.in.combat_wins=2;r.step();CHECK(r.last.combat_wins==2);r.in.player_dead=true;CHECK(r.step().events&Died);CHECK(r.last.death_reason==DeathReason::Combat);r.in.player_dead=false;r.wait(1);r.in.retry_down=true;CHECK(r.step().events&Retried);CHECK(r.state.stage()==Stage::Escape&&r.last.events&EnsureSword);CHECK(r.last.combat_wins==0&&r.grants==1);CHECK(r.last.teleport_pose.feet.x==c.escape_spawn.feet.x);}
    // If death interrupts the stab before contact, there is no owned sword. If
    // it interrupts after contact, checkpoint inventory retains exactly one.
    for(bool after:{false,true}){Run r;r.steal();if(after){while(!r.state.sword_owned())r.step();}r.in.player_dead=true;r.step();CHECK(r.state.stage()==Stage::Dead);CHECK(r.state.sword_owned()==after);CHECK(r.state.checkpoint()==(after?Checkpoint::SwordSecured:Checkpoint::BeforeAmbush));CHECK(r.grants==unsigned(after));}
    // Legacy absence is inert, malformed future data is rejected transactionally.
    {Save legacy;State state;Frame f;CHECK(state.configure(c,error));CHECK(decode_save("",c,legacy,error)&&!legacy.present);CHECK(state.restore(legacy,f,error)&&state.stage()==Stage::Inactive&&f.events==None);std::string bytes="x";CHECK(encode_save(legacy,c,bytes,error)&&bytes.empty());
      Save good;good.present=true;good.checkpoint=Checkpoint::SwordSecured;good.sword_owned=true;CHECK(encode_save(good,c,bytes,error));
      for(const auto &bad:{bytes+"junk\n",std::string(513,'x'),std::string("VALWOOD_BATTLEFIELD 2\ncheckpoint 1\ndifficulty 1\nsword 1\nwins 0\n"),std::string("VALWOOD_BATTLEFIELD 1\ncheckpoint 2\ndifficulty 1\nsword 1\nwins 0\n"),std::string("VALWOOD_BATTLEFIELD 1\ncheckpoint 0\ndifficulty 1\nsword 1\nwins 0\n"),std::string("VALWOOD_BATTLEFIELD 1\ncheckpoint 1\ndifficulty 9\nsword 1\nwins 0\n"),bytes+std::string(1,'\0')+"x"}){Save out=good;CHECK(!decode_save(bad,c,out,error));CHECK(out.sword_owned&&out.checkpoint==good.checkpoint);}}
    // A forest checkpoint uses the live support cook, including an elevated
    // bridge deck, rather than silently replacing it with bare atlas terrain.
    {Config bridge=c;bridge.forest.feet.y=12.14;State state;CHECK(state.configure(bridge,error));Save saved;saved.present=true;saved.checkpoint=Checkpoint::Forest;saved.sword_owned=true;saved.combat_wins=bridge.required_escape_wins;Frame f;CHECK(state.restore(saved,f,error));CHECK(state.stage()==Stage::Forest);CHECK(f.events&TeleportPlayer);CHECK(f.teleport_pose.feet.x==-12008&&f.teleport_pose.feet.z==5167&&f.teleport_pose.feet.y==12.14);CHECK(f.sword_owned&&f.combat_wins==bridge.required_escape_wins);Input in;in.player_feet=bridge.campsite.feet;state.update(in);f=state.update(in);CHECK(f.completed&&state.checkpoint()==Checkpoint::Campsite);}
    // Authoring opt-out and failed config/restore leave existing live state intact.
    {State s;Config off=c;off.enabled=false;CHECK(s.configure(off,error));CHECK(s.start().stage==Stage::Inactive);CHECK(s.configure(c,error));s.start();const auto before=s.stage();Config bad=c;bad.required_escape_wins=500;CHECK(!s.configure(bad,error)&&s.stage()==before);Save badsave;badsave.present=true;badsave.version=9;Frame f;CHECK(!s.restore(badsave,f,error)&&s.stage()==before);}
    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"success_difficulties\":3,\"success_fps\":[60,30,15,5],\"fresh_button_edges\":true,\"actual_combat_receipts_required\":true,\"all_deaths_local_retry\":true,\"legacy_save_inert\":true}\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}

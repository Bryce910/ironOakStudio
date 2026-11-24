#include "engine/world_first_journey.h"
#include "engine/world_training_combat.h"
#include <iostream>
#include <limits>
#include <sstream>

using namespace world_first_journey;
static unsigned checks=0;
static std::string error;
#define CHECK(x) do {++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<" ("<<error<<")\n";return false;}} while(0)
struct Driver {
    State state;Input input;Frame frame;
    explicit Driver(double hz=60){input.dt=1/hz;std::string e;state.configure(Config{},e);input.player_feet=state.config().camp;}
    Frame tick(){return frame=state.tick(input);}
    void up(){input.interact_down=input.confirm_down=input.cancel_down=input.retry_down=false;tick();tick();}
    Frame interact(Vec p){up();input.player_feet=p;input.interact_down=true;return tick();}
    Frame advance(){up();input.confirm_down=true;return tick();}
    void travelers(){input.camp_spoken=all_travelers;up();}
    Frame finish_dialogue(){const auto kind=state.dialogue();Frame f;for(unsigned i=0;i<dialogue_length(kind);++i)f=advance();return f;}
    void loan(){travelers();interact(state.config().bram);finish_dialogue();}
    std::uint64_t encounter(){interact(state.config().encounter);return state.encounter_token();}
    Frame receipt(std::uint64_t token,CombatStatus status){up();input.combat={token,status};const auto f=tick();input.combat={};return f;}
};
static bool loan_and_input(){
    Driver d;CHECK(d.state.stage()==Stage::Travelers);CHECK(d.state.config().encounter.x==-1404.0077270925497);
    d.up();d.input.camp_spoken=1;d.tick();CHECK(d.state.camp_spoken()==1);d.input.camp_spoken=2;d.tick();CHECK(d.state.camp_spoken()==3);
    d.interact(d.state.config().supplies);CHECK(!d.state.weapon_unlocked());CHECK(!d.state.supplies_recovered());
    d.input.camp_spoken=4;d.tick();CHECK(d.state.stage()==Stage::Offer);CHECK(!d.state.weapon_unlocked());
    // Held E across a prerequisite transition grants neither dialogue nor sword.
    for(unsigned i=0;i<60;++i){d.tick();}CHECK(d.state.dialogue()==Dialogue::None);CHECK(!d.state.weapon_unlocked());
    d.interact(d.state.config().rowan);CHECK(d.state.dialogue()==Dialogue::None);
    auto f=d.interact(d.state.config().bram);CHECK(f.events&DialogueOpened);CHECK(f.dialogue_actor==1);CHECK(!f.controls_enabled);CHECK(d.state.beat()==0);
    for(unsigned i=0;i<100;++i){d.tick();}CHECK(d.state.beat()==0);
    // Switching confirmation devices while another is held is not a neutral edge.
    d.input.confirm_down=true;d.tick();CHECK(d.state.beat()==0);
    d.up();d.input.cancel_down=true;f=d.tick();CHECK(f.events&DialogueClosed);CHECK(!d.state.weapon_unlocked());CHECK(d.state.stage()==Stage::Offer);
    d.interact(d.state.config().bram);d.advance();CHECK(d.state.beat()==1);
    const auto before=d.state.stats().active_seconds;
    d.input.paused=true;for(unsigned i=0;i<60;++i){d.tick();}CHECK(d.state.beat()==1);CHECK(d.state.stats().active_seconds==before);
    d.input.paused=false;d.tick();CHECK(d.state.beat()==1);d.input.confirm_down=false;d.tick();d.input.confirm_down=true;d.tick();CHECK(d.state.beat()==2);
    d.up();d.input.focused=false;d.input.confirm_down=true;d.tick();d.input.focused=true;d.tick();CHECK(d.state.beat()==2);
    d.up();d.input.dt=std::numeric_limits<double>::quiet_NaN();d.input.confirm_down=true;f=d.tick();CHECK(!f.controls_enabled);CHECK(d.state.beat()==2);d.input.dt=1./60;
    while(d.state.dialogue()!=Dialogue::None)f=d.advance();
    CHECK(f.events&WeaponGranted);CHECK(d.state.weapon_unlocked());CHECK(d.state.stage()==Stage::Road);CHECK(d.state.stats().dialogues_completed==1);
    for(unsigned i=0;i<60;++i){d.tick();}CHECK(d.state.stage()==Stage::Road);CHECK(!d.state.reward_granted());
    return true;
}
static bool receipt_gating_and_retry(){
    Driver d;d.loan();const auto token=d.encounter();CHECK(token!=0);CHECK(d.state.stage()==Stage::Encounter);CHECK(d.frame.events&EncounterRequested);
    d.receipt(token+1,CombatStatus::Running);CHECK(!d.state.encounter_running());
    d.receipt(token,CombatStatus::Won);CHECK(d.state.stage()==Stage::Encounter);CHECK(!d.state.encounter_won());
    d.receipt(token,CombatStatus::Running);CHECK(d.state.encounter_running());
    d.input.paused=true;d.input.combat={token,CombatStatus::Won};d.tick();CHECK(!d.state.encounter_won());d.input.combat={};d.input.paused=false;d.up();
    d.receipt(token+1,CombatStatus::Won);CHECK(!d.state.encounter_won());
    auto f=d.receipt(token,CombatStatus::Lost);CHECK(f.events&EncounterLost);CHECK(d.state.defeated());CHECK(!d.state.encounter_running());CHECK(d.state.stats().defeats==1);
    d.input.confirm_down=true;for(unsigned i=0;i<30;++i)d.tick();CHECK(d.state.defeated());
    d.up();d.input.retry_down=true;f=d.tick();CHECK(f.events&RetryRequested);CHECK(d.state.stage()==Stage::Road);CHECK(d.state.weapon_unlocked());CHECK(d.state.camp_spoken()==7);CHECK(d.state.stats().retries==1);
    const auto next=d.encounter();CHECK(next!=token);d.receipt(token,CombatStatus::Won);CHECK(!d.state.encounter_won());
    d.receipt(next,CombatStatus::Running);f=d.receipt(next,CombatStatus::Won);CHECK(f.events&EncounterWon);CHECK(d.state.stage()==Stage::RecoverSupplies);CHECK(d.state.stats().encounters_won==1);
    for(unsigned i=0;i<20;++i){d.receipt(next,CombatStatus::Won);}CHECK(d.state.stats().encounters_won==1);
    // A failed host start can be cancelled without inventing a Running receipt.
    Driver pending;pending.loan();const auto cancelled=pending.encounter();f=pending.receipt(cancelled,CombatStatus::Cancelled);CHECK(pending.state.stage()==Stage::Road);CHECK(!pending.state.encounter_won());
    return true;
}
static bool reward_and_complete(){
    Driver d;d.loan();
    d.interact(d.state.config().supplies);CHECK(!d.state.supplies_recovered());
    const auto token=d.encounter();d.receipt(token,CombatStatus::Running);d.receipt(token,CombatStatus::Won);
    d.interact(d.state.config().rowan);CHECK(!d.state.supplies_recovered());
    d.input.player_feet=d.state.config().supplies;d.input.player_feet.y+=4;d.interact(d.input.player_feet);CHECK(!d.state.supplies_recovered());
    auto f=d.interact(d.state.config().supplies);CHECK(f.events&SuppliesRecovered);CHECK(d.state.stage()==Stage::ReturnCamp);CHECK(d.state.stats().supplies_pickups==1);
    for(unsigned i=0;i<60;++i){d.tick();}CHECK(d.state.stats().supplies_pickups==1);CHECK(!d.state.reward_granted());
    d.interact(d.state.config().rowan);CHECK(d.state.dialogue()==Dialogue::Return);CHECK(d.frame.dialogue_actor==0);
    d.advance();d.up();d.input.cancel_down=true;d.tick();CHECK(!d.state.reward_granted());CHECK(d.state.stage()==Stage::ReturnCamp);
    d.interact(d.state.config().rowan);f=d.finish_dialogue();CHECK(f.events&RewardGranted);CHECK(d.state.reward_granted());CHECK(d.state.stage()==Stage::Town);CHECK(d.state.stats().rewards==1);CHECK(f.coins==12&&f.food==1&&f.reputation==1);
    d.interact(d.state.config().rowan);CHECK(d.state.dialogue()==Dialogue::None);CHECK(d.state.stats().rewards==1);
    d.interact(d.state.config().town);CHECK(d.state.dialogue()==Dialogue::Town);CHECK(d.frame.dialogue_actor==3);f=d.finish_dialogue();CHECK(f.events&Completed);CHECK(d.state.stage()==Stage::Complete);CHECK(d.state.stats().completions==1);
    for(unsigned i=0;i<30;++i){d.interact(d.state.config().town);CHECK(d.state.stats().rewards==1);CHECK(d.state.stats().completions==1);CHECK(d.frame.coins==12);}
    const auto saved=d.state.stable_save();CHECK(validate_save(saved,d.state.config(),error));State restored;CHECK(restored.configure(d.state.config(),error));CHECK(restored.restore(saved,error));CHECK(restored.stage()==Stage::Complete);CHECK(restored.sample().coins==12);CHECK(restored.stats().rewards==0);CHECK(restored.stats().completions==0);
    return true;
}
static bool checkpoints_and_parsing(){
    Driver d;std::string bytes;Save parsed;
    auto roundtrip=[&](){const auto saved=d.state.stable_save();CHECK(encode_save(saved,d.state.config(),bytes,error));std::istringstream rows(bytes);CHECK(parse_save(rows,d.state.config(),parsed,error));CHECK(parsed.stage==saved.stage);CHECK(parsed.reward_granted==saved.reward_granted);CHECK(parsed.coins==saved.coins);CHECK(d.state.restore(parsed,error));return true;};
    CHECK(roundtrip());d.travelers();CHECK(roundtrip());d.interact(d.state.config().bram);d.advance();CHECK(roundtrip());CHECK(!d.state.weapon_unlocked());CHECK(d.state.dialogue()==Dialogue::None);
    d.interact(d.state.config().bram);d.finish_dialogue();CHECK(roundtrip());
    const auto token=d.encounter();d.receipt(token,CombatStatus::Running);const auto during=d.state.stable_save();CHECK(during.stage==Stage::Road);CHECK(roundtrip());CHECK(d.state.stage()==Stage::Road);CHECK(!d.state.encounter_running());
    const auto next=d.encounter();CHECK(next!=token);d.receipt(token,CombatStatus::Won);CHECK(!d.state.encounter_won());d.receipt(next,CombatStatus::Running);d.receipt(next,CombatStatus::Won);CHECK(roundtrip());
    d.interact(d.state.config().supplies);CHECK(roundtrip());d.interact(d.state.config().rowan);d.finish_dialogue();CHECK(roundtrip());d.interact(d.state.config().town);d.finish_dialogue();CHECK(roundtrip());
    const auto committed=d.state.stable_save();const auto sentinel=parsed;
    for(std::size_t n=0;n<bytes.size();++n){std::istringstream truncated(bytes.substr(0,n));Save out=sentinel;CHECK(!parse_save(truncated,d.state.config(),out,error));CHECK(out.coins==sentinel.coins&&out.stage==sentinel.stage);}
    for(std::size_t n=0;n<bytes.size();++n){auto corrupt=bytes;corrupt[n]=corrupt[n]=='9'?'8':'9';std::istringstream rows(corrupt);Save out=sentinel;CHECK(!parse_save(rows,d.state.config(),out,error));CHECK(out.coins==sentinel.coins&&out.stage==sentinel.stage);}
    auto bad=committed;bad.coins=24;CHECK(!d.state.restore(bad,error));CHECK(d.state.stage()==Stage::Complete);CHECK(d.state.sample().coins==12);
    bad=committed;bad.camp_spoken=3;CHECK(!d.state.restore(bad,error));bad=committed;bad.weapon_unlocked=false;CHECK(!d.state.restore(bad,error));bad=committed;bad.reward_granted=false;CHECK(!d.state.restore(bad,error));bad=committed;bad.stage=Stage::Encounter;CHECK(!d.state.restore(bad,error));
    auto moved=d.state.config();moved.town.x+=.01;State changed;CHECK(changed.configure(moved,error));CHECK(!changed.restore(committed,error));
    auto invalid=d.state.config();invalid.supplies.x+=100;CHECK(!changed.configure(invalid,error));CHECK(changed.config().town.x==moved.town.x);
    std::ostringstream config;config.precision(17);const auto c=d.state.config();config<<"VALWOOD_FIRST_JOURNEY 1\n";
    auto point=[&](const char *name,Vec p){config<<name<<' '<<p.x<<' '<<p.y<<' '<<p.z<<'\n';};point("camp",c.camp);point("rowan",c.rowan);point("bram",c.bram);point("encounter",c.encounter);point("supplies",c.supplies);point("retry",c.retry);point("town",c.town);config<<"radii "<<c.talk_radius<<' '<<c.encounter_radius<<' '<<c.supplies_radius<<' '<<c.town_radius<<"\nend\n";
    Config decoded;std::istringstream valid(config.str());CHECK(parse_config(valid,decoded,error));CHECK(signature(decoded)==signature(c));
    for(const auto &bad_text:{config.str()+"unexpected",std::string(5000,'x'),config.str().substr(0,30),std::string("VALWOOD_FIRST_JOURNEY 2\n")}){Config before=decoded;std::istringstream bad_config(bad_text);CHECK(!parse_config(bad_config,decoded,error));CHECK(signature(before)==signature(decoded));}
    return true;
}
static bool real_combat_and_rates(){
    // This walkthrough drives the production duel with normal attack/guard
    // commands. No health, terminal state, hit count or outcome is assigned.
    for(double hz:{30.,60.,144.}){
        Driver d(hz);d.loan();const auto token=d.encounter();
        WorldTrainingCombat fight;CHECK(fight.start_arena(100,100,60,true));d.receipt(token,CombatStatus::Running);
        for(unsigned i=0;i<unsigned(90*hz)&&fight.engaged();++i){
            if(fight.enemy_phase==TrainingEnemyPhase::Windup||fight.enemy_phase==TrainingEnemyPhase::Strike){fight.choose(fight.enemy_direction);fight.set_guard(true);}
            else{fight.set_guard(false);fight.choose(fight.enemy_guard==TrainingDirection::Left?TrainingDirection::Right:TrainingDirection::Left);if(fight.player_phase==TrainingPlayerPhase::Ready)fight.attack();}
            fight.update(1/hz,WorldTrainingCombat::enemy_x-1.6,WorldTrainingCombat::enemy_z,false);
            d.input.combat={token,CombatStatus::Running};d.tick();
        }
        CHECK(fight.state==TrainingState::Won);CHECK(fight.hits>=3);CHECK(fight.player_swings>=fight.hits);
        d.input.combat={token,CombatStatus::Won};auto f=d.tick();d.input.combat={};CHECK(f.events&EncounterWon);CHECK(d.state.stage()==Stage::RecoverSupplies);
        d.interact(d.state.config().supplies);d.interact(d.state.config().rowan);d.finish_dialogue();d.interact(d.state.config().town);d.finish_dialogue();CHECK(d.state.stage()==Stage::Complete);CHECK(d.state.stats().rewards==1);CHECK(d.state.stats().encounters_won==1);
        CHECK(d.state.stats().active_seconds>0);
    }
    return true;
}
int main(){if(!loan_and_input()||!receipt_gating_and_retry()||!reward_and_complete()||!checkpoints_and_parsing()||!real_combat_and_rates())return 1;std::cout<<"world_first_journey: "<<checks<<" checks; fresh controls, earned weapon, real duel, defeat/retry, one-time reward, restart and corrupt rollback at 30/60/144 Hz\n";}

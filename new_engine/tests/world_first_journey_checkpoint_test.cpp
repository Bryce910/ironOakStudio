#include "engine/world_opening_prologue.h"
#include <iostream>
#include <sstream>
#include <vector>

namespace opening=world_opening_prologue;
namespace journey=world_first_journey;
static unsigned checks=0;
static std::string error;
#define CHECK(x) do {++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<" | "<<error<<'\n';return false;}} while(0)
struct Driver {
    journey::State state;journey::Input input;
    Driver(){input.dt=1./60.;state.configure({},error);input.player_feet=state.config().camp;}
    void tick(){state.tick(input);}
    void up(){input.interact_down=input.confirm_down=input.cancel_down=input.retry_down=false;tick();tick();}
    void interact(journey::Vec p){up();input.player_feet=p;input.interact_down=true;tick();}
    void finish(){const auto length=journey::dialogue_length(state.dialogue());for(unsigned i=0;i<length;++i){up();input.confirm_down=true;tick();}}
    void receipt(journey::CombatStatus status){up();input.combat={state.encounter_token(),status};tick();input.combat={};}
};
static opening::Config config(){
    opening::Config c;c.first_journey_enabled=true;c.arrival_camp.enabled=true;
    const auto &j=c.first_journey;
    c.arrival_camp.center={j.camp.x,j.camp.y,j.camp.z};
    c.arrival_camp.actors[0]={{j.rowan.x,j.rowan.y,j.rowan.z},1.2772073393863137,false};
    c.arrival_camp.actors[1]={{j.bram.x,j.bram.y,j.bram.z},-1.2835148733979203,false};
    c.arrival_camp.actors[2]={{-1486.2766486849901,18.956187894024009,-10228.110611064201},-.031239833430232785,false};
    c.meadow_spawn.feet={j.camp.x,j.camp.y,j.camp.z+5.2};
    c.meadow_goal={j.supplies.x,j.supplies.y,j.supplies.z};
    return c;
}
static std::string fingerprint(const opening::Save &s){
    std::ostringstream out;out.precision(17);
    out<<s.version<<' '<<unsigned(s.checkpoint)<<' '<<s.player.feet.x<<' '<<s.player.feet.y<<' '<<s.player.feet.z<<' '<<s.player.yaw<<' '<<s.player.pitch<<' '<<s.room_lights<<' '<<s.camp_spoken<<' '<<s.first_journey_present<<' '<<s.first_journey.schema_version<<'\n'<<journey::save_body(s.first_journey);return out.str();
}
static std::vector<journey::Save> earned_saves(){
    Driver d;std::vector<journey::Save> saves{d.state.stable_save()};
    d.input.camp_spoken=7;d.up();saves.push_back(d.state.stable_save());
    d.interact(d.state.config().bram);d.finish();saves.push_back(d.state.stable_save());
    d.interact(d.state.config().encounter);d.receipt(journey::CombatStatus::Running);d.receipt(journey::CombatStatus::Won);saves.push_back(d.state.stable_save());
    d.interact(d.state.config().supplies);saves.push_back(d.state.stable_save());
    d.interact(d.state.config().rowan);d.finish();saves.push_back(d.state.stable_save());
    d.interact(d.state.config().town);d.finish();saves.push_back(d.state.stable_save());return saves;
}
static opening::Save compose(const opening::Config &c,opening::Checkpoint checkpoint,const journey::Save &j){
    auto s=opening::checkpoint_save(checkpoint,c);s.room_lights=5;s.camp_spoken=j.camp_spoken;s.first_journey_present=true;s.first_journey=j;return s;
}
static std::string wrap_raw_nested(const opening::Save &base,const opening::Config &c,const std::string &nested){
    auto plain=base;plain.first_journey_present=false;std::string out;opening::encode_save(plain,c,out,error);std::ostringstream extra;extra<<"first_journey "<<std::quoted(nested)<<'\n';return out+extra.str();
}
static std::string nested_with_body(const std::string &body){
    std::ostringstream out;out<<"VALWOOD_FIRST_JOURNEY_PROGRESS "<<journey::version<<' '<<journey::detail::text_hash(body)<<'\n'<<body;return out.str();
}
static bool legacy_compatibility(){
    opening::Config legacy;
    for(auto checkpoint:{opening::Checkpoint::HouseStart,opening::Checkpoint::HouseInspected,opening::Checkpoint::MeadowStart,opening::Checkpoint::Complete}){
        const auto save=opening::checkpoint_save(checkpoint,legacy);std::string bytes;
        CHECK(opening::encode_save(save,legacy,bytes,error));
        const std::string expected="VALWOOD_OPENING 1\ncheckpoint "+std::to_string(unsigned(checkpoint))+"\nplayer "+(checkpoint<opening::Checkpoint::MeadowStart?"0 0 0 0 0":"100 0 0 0 0")+"\nroom_lights 15\n";
        CHECK(bytes==expected);opening::Save restored;std::istringstream rows(bytes);CHECK(opening::parse_save(rows,legacy,restored,error));CHECK(!restored.first_journey_present);CHECK(fingerprint(restored)==fingerprint(save));
        // The optional extension does not turn a legacy checkpoint into earned
        // quest progress. An old apartment save remains an apartment save.
        const auto current=config();auto moved=opening::checkpoint_save(checkpoint,current);std::string old_current;CHECK(opening::encode_save(moved,current,old_current,error));CHECK(old_current.find("first_journey")==std::string::npos);
        opening::Save imported;std::istringstream old_rows(old_current);CHECK(opening::parse_save(old_rows,current,imported,error));CHECK(!imported.first_journey_present);CHECK(imported.camp_spoken==0);
    }
    const std::string oldest="VALWOOD_OPENING 1\ncheckpoint 1\nplayer 0 0 0 0 0\n";opening::Save old;std::istringstream bytes(oldest);CHECK(opening::parse_save(bytes,legacy,old,error));CHECK(old.room_lights==15&&!old.first_journey_present&&old.camp_spoken==0);
    return true;
}
static bool earned_roundtrips(){
    const auto c=config();CHECK(opening::validate(c,error));const auto saves=earned_saves();CHECK(saves.size()==7);
    CHECK(saves[1].stage==journey::Stage::Offer);CHECK(saves[2].stage==journey::Stage::Road);CHECK(saves[3].stage==journey::Stage::RecoverSupplies);CHECK(saves[4].stage==journey::Stage::ReturnCamp);CHECK(saves[5].stage==journey::Stage::Town);CHECK(saves[6].stage==journey::Stage::Complete);
    for(const auto &j:saves){
        for(auto checkpoint:{opening::Checkpoint::MeadowStart,opening::Checkpoint::Complete}){
            const auto save=compose(c,checkpoint,j);std::string bytes;CHECK(opening::encode_save(save,c,bytes,error));CHECK(bytes.size()<1024);CHECK(bytes.find("first_journey \"VALWOOD_FIRST_JOURNEY_PROGRESS 1 ")!=std::string::npos);CHECK(bytes.find("camp_spoken")<bytes.find("first_journey"));
            opening::Save parsed;std::istringstream rows(bytes);CHECK(opening::parse_save(rows,c,parsed,error));CHECK(fingerprint(parsed)==fingerprint(save));
            opening::State opening_state;journey::State quest_state;CHECK(opening_state.configure(c,error));CHECK(quest_state.configure(c.first_journey,error));CHECK(opening_state.restore(parsed,error));CHECK(quest_state.restore(parsed.first_journey,error));
            CHECK(opening_state.camp().spoken()==j.camp_spoken);CHECK(quest_state.stage()==j.stage);CHECK(quest_state.weapon_unlocked()==j.weapon_unlocked);CHECK(quest_state.sample().coins==j.coins);CHECK(quest_state.stats().rewards==0);CHECK(quest_state.stats().completions==0);
            // The host composes both stable states into ONE atomic save, never
            // two independent files. Restoring does not reissue reward events.
            auto rewritten=opening_state.stable_save();rewritten.first_journey_present=true;rewritten.first_journey=quest_state.stable_save();std::string rewritten_bytes;CHECK(opening::encode_save(rewritten,c,rewritten_bytes,error));
            journey::Input in;in.dt=1./60.;in.player_feet=c.first_journey.rowan;in.camp_spoken=j.camp_spoken;quest_state.tick(in);auto f=quest_state.tick(in);CHECK(!(f.events&journey::RewardGranted));CHECK(quest_state.stats().rewards==0);CHECK(f.coins==j.coins);
            in.interact_down=true;f=quest_state.tick(in);if(j.reward_granted){CHECK(!(f.events&journey::RewardGranted));CHECK(f.coins==journey::reward_coins);CHECK(quest_state.stats().rewards==0);}
        }
    }
    // Before transport only the unearned Travelers state may accompany the
    // house checkpoint. Earned world progress cannot be attached to a house.
    for(auto checkpoint:{opening::Checkpoint::HouseStart,opening::Checkpoint::HouseInspected}){
        auto initial=compose(c,checkpoint,saves.front());std::string bytes;CHECK(opening::encode_save(initial,c,bytes,error));opening::Save parsed;std::istringstream rows(bytes);CHECK(opening::parse_save(rows,c,parsed,error));
        for(std::size_t i=1;i<saves.size();++i){auto earned=compose(c,checkpoint,saves[i]);std::string unchanged="unchanged";CHECK(!opening::encode_save(earned,c,unchanged,error));CHECK(unchanged=="unchanged");}
    }
    auto no_camp=c;no_camp.arrival_camp.enabled=false;CHECK(!opening::validate(no_camp,error));
    return true;
}
static bool corrupt_and_transactional(){
    const auto c=config();const auto saves=earned_saves();const auto good=compose(c,opening::Checkpoint::MeadowStart,saves.back());std::string encoded;CHECK(opening::encode_save(good,c,encoded,error));
    const auto sentinel=compose(c,opening::Checkpoint::HouseInspected,saves.front());const auto original=fingerprint(sentinel);
    auto reject=[&](const std::string &bytes,const opening::Config &configuration){auto out=sentinel;std::istringstream rows(bytes);CHECK(!opening::parse_save(rows,configuration,out,error));CHECK(fingerprint(out)==original);return true;};
    std::string nested;CHECK(journey::encode_save(good.first_journey,c.first_journey,nested,error));
    auto tampered=nested;const auto reward=tampered.find("reward 12 1 1");CHECK(reward!=std::string::npos);tampered[reward+7]='9';CHECK(reject(wrap_raw_nested(good,c,tampered),c));
    auto body=journey::save_body(good.first_journey);const std::string earned_reward="reward 12 1 1";const auto coins=body.find(earned_reward);CHECK(coins!=std::string::npos);body.replace(coins,earned_reward.size(),"reward 24 1 1");CHECK(reject(wrap_raw_nested(good,c,nested_with_body(body)),c));
    body=journey::save_body(good.first_journey);const std::string earned_progress="progress 1 1 1 1 1";const auto progress=body.find(earned_progress);CHECK(progress!=std::string::npos);body.replace(progress,earned_progress.size(),"progress 1 0 1 1 1");CHECK(reject(wrap_raw_nested(good,c,nested_with_body(body)),c));
    auto foreign=good.first_journey;++foreign.layout_signature;CHECK(reject(wrap_raw_nested(good,c,nested_with_body(journey::save_body(foreign))),c));
    tampered=encoded;const auto mask=tampered.find("camp_spoken 7");CHECK(mask!=std::string::npos);tampered[mask+12]='3';CHECK(reject(tampered,c));
    CHECK(reject(encoded+"first_journey "+std::string(1,'"')+nested+"\"\n",c));CHECK(reject(encoded+"extra\n",c));CHECK(reject(encoded+std::string(1,'\0'),c));CHECK(reject(std::string(1025,'x'),c));
    auto legacy=c;legacy.first_journey_enabled=false;CHECK(reject(encoded,legacy));auto shifted=c;shifted.first_journey.town.x+=.01;CHECK(reject(encoded,shifted));
    // Removing an entire optional extension is intentionally a valid legacy
    // checkpoint. Once its tag/quote begins, every truncated nested payload
    // must fail transactionally; trailing record whitespace is insignificant.
    const auto extension=encoded.find("first_journey ");CHECK(extension!=std::string::npos);
    for(std::size_t end=extension+1;end+2<encoded.size();++end){CHECK(reject(encoded.substr(0,end),c));}
    for(std::size_t i=0;i<nested.size();++i){auto corrupt=nested;corrupt[i]=corrupt[i]=='9'?'8':'9';CHECK(reject(wrap_raw_nested(good,c,corrupt),c));}
    std::vector<opening::Save> bad;
    auto inconsistent=good;inconsistent.camp_spoken=3;bad.push_back(inconsistent);inconsistent=good;inconsistent.first_journey.coins=24;bad.push_back(inconsistent);inconsistent=good;inconsistent.first_journey.layout_signature=0;bad.push_back(inconsistent);inconsistent=good;inconsistent.first_journey.stage=journey::Stage::Encounter;bad.push_back(inconsistent);
    opening::State live;CHECK(live.configure(c,error));CHECK(live.restore(good,error));for(const auto &invalid:bad){std::string output="unchanged";CHECK(!opening::encode_save(invalid,c,output,error));CHECK(output=="unchanged");CHECK(!live.restore(invalid,error));CHECK(live.camp().spoken()==7&&live.checkpoint()==opening::Checkpoint::MeadowStart);}
    // Neither recovery/reward flags nor the earned ledger can be changed by
    // loading an invalid outer checkpoint after the inner state is committed.
    journey::State committed;CHECK(committed.configure(c.first_journey,error));CHECK(committed.restore(good.first_journey,error));CHECK(committed.sample().coins==12);CHECK(committed.stats().rewards==0);
    return true;
}
int main(){if(!legacy_compatibility()||!earned_roundtrips()||!corrupt_and_transactional())return 1;std::cout<<"world_first_journey_checkpoint: "<<checks<<" checks; exact legacy schema, atomic nested progress, earned ledger, corruption rollback and no replayed rewards\n";}

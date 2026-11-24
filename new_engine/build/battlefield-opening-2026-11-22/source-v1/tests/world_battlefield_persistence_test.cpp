#include "engine/world_opening_prologue.h"
#include "engine/world_battlefield_prologue.h"
#include <iostream>
#include <stdexcept>
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": " #x);}while(0)
static bool decode_opening(const std::string &bytes,const world_opening_prologue::Config &c,world_opening_prologue::Save &s,std::string &error){std::istringstream input(bytes);return world_opening_prologue::parse_save(input,c,s,error);}
int main(){try{
 namespace op=world_opening_prologue;namespace inv=world_player_inventory;namespace bf=world_battlefield_prologue;namespace fj=world_first_journey;
 std::string error,bytes;op::Config config;CHECK(op::validate(config,error));
 // Minimal old v1 checkpoints, including records without later room-light fields,
 // remain byte-for-byte compatible when no battlefield record is present.
 for(auto cp:{op::Checkpoint::HouseStart,op::Checkpoint::HouseInspected,op::Checkpoint::MeadowStart,op::Checkpoint::Complete}){
  auto s=op::checkpoint_save(cp,config);CHECK(op::encode_save(s,config,bytes,error));CHECK(bytes.find("battlefield_progress")==std::string::npos);op::Save parsed;CHECK(decode_opening(bytes,config,parsed,error)&&parsed.battlefield_progress.empty());std::string again;CHECK(op::encode_save(parsed,config,again,error)&&again==bytes);
  const auto lights=bytes.find("room_lights");const auto oldest=bytes.substr(0,lights);CHECK(decode_opening(oldest,config,parsed,error)&&parsed.battlefield_progress.empty());
 }
 bf::Config bconfig;bf::Save bsave;bsave.present=true;bsave.checkpoint=bf::Checkpoint::SwordSecured;bsave.sword_owned=true;std::string battlefield;CHECK(bf::encode_save(bsave,bconfig,battlefield,error));
 auto opening=op::checkpoint_save(op::Checkpoint::MeadowStart,config);opening.battlefield_progress=battlefield;CHECK(op::encode_save(opening,config,bytes,error));op::Save parsed;CHECK(decode_opening(bytes,config,parsed,error)&&parsed.battlefield_progress==battlefield);bf::Save roundtrip;CHECK(bf::decode_save(parsed.battlefield_progress,bconfig,roundtrip,error)&&roundtrip.sword_owned);
 // Structural layer intentionally does not assume a particular active layout.
 // Host validates the nested payload against the live battlefield config.
 opening.battlefield_progress="opaque future host payload";CHECK(op::encode_save(opening,config,bytes,error));CHECK(decode_opening(bytes,config,parsed,error)&&parsed.battlefield_progress==opening.battlefield_progress);
 opening.battlefield_progress=std::string(512,'x');CHECK(op::encode_save(opening,config,bytes,error));CHECK(decode_opening(bytes,config,parsed,error)&&parsed.battlefield_progress.size()==512);
 opening.battlefield_progress=std::string(513,'x');std::string unchanged="untouched";CHECK(!op::encode_save(opening,config,unchanged,error)&&unchanged=="untouched");opening.battlefield_progress=std::string("abc\0def",7);CHECK(!op::validate_save(opening,config,error));
 opening.battlefield_progress.clear();CHECK(op::encode_save(opening,config,bytes,error));const std::string prefix=bytes;
 for(const std::string &suffix:{std::string("battlefield_progress \"unterminated\n"),std::string("battlefield_progress \"\"\n"),std::string("battlefield_progress unquoted\n"),std::string("battlefield_progress \"")+std::string(513,'x')+"\"\n",std::string("battlefield_progress \"abc\"\nbattlefield_progress \"def\"\n"),std::string("battlefield_progress \"abc\"\nroom_lights 15\n"),std::string("battlefield_progress \"ab")+std::string(1,'\0')+"cd\"\n"}){
  op::Save preserved=opening;preserved.battlefield_progress="previous";CHECK(!decode_opening(prefix+suffix,config,preserved,error));CHECK(preserved.battlefield_progress=="previous");
 }
 // One stolen sword before camp, no armor/side weapons/food, no loan receipt yet.
 inv::State inventory;CHECK(inv::grant_battlefield_sword(inventory));CHECK(inv::valid(inventory));CHECK(inventory.receipts==inv::battlefield_receipt);CHECK(inventory.weapon==inv::Item::Sword);CHECK(inv::count(inventory,inv::Item::Sword)==1);CHECK(inv::occupied(inventory)==1);CHECK(inv::protection(inventory)==0);CHECK(inv::matches_story(inventory,false,false,false));const auto stolen=inv::encode(inventory);CHECK(inv::grant_battlefield_sword(inventory)&&inv::encode(inventory)==stolen);
 inv::State loaded;CHECK(inv::decode(stolen,loaded,error)&&loaded.receipts==inv::battlefield_receipt);CHECK(inv::matches_story(loaded,false,false,false));
 auto invalid=inventory;invalid.counts[unsigned(inv::Item::Sword)]=0;invalid.weapon=inv::Item::None;CHECK(!inv::valid(invalid)&&!inv::matches_story(invalid,false,false,false));const auto missing_bytes=inv::encode(invalid);CHECK(missing_bytes.empty());CHECK(!inv::grant_battlefield_sword(invalid));
 // Accepting Bram's Offer acknowledges existing equipment and never materializes
 // the old practice kit. Reconcile and repeated Offer are once-only operations.
 CHECK(inv::reconcile(inventory,true,false,false));CHECK(inventory.receipts==(inv::battlefield_receipt|inv::loan_receipt));CHECK(inv::matches_story(inventory,true,false,false));CHECK(inv::occupied(inventory)==1&&inventory.weapon==inv::Item::Sword&&inv::protection(inventory)==0);CHECK(inv::count(inventory,inv::Item::SwordShield)==0&&inv::count(inventory,inv::Item::Axe)==0&&inv::count(inventory,inv::Item::Staff)==0&&inv::count(inventory,inv::Item::TravelArmor)==0);const auto acknowledged=inv::encode(inventory);for(unsigned n=0;n<20;++n)CHECK(inv::grant_loan(inventory)&&inv::reconcile(inventory,true,false,false)&&inv::encode(inventory)==acknowledged);
 CHECK(inv::reconcile(inventory,true,true,false)&&inv::count(inventory,inv::Item::Satchel)==1);CHECK(inv::reconcile(inventory,true,true,true));CHECK(inv::matches_story(inventory,true,true,true));float hp=60,stamina=100;std::string message;CHECK(inv::use(inventory,inv::Item::Bread,hp,stamina,message));CHECK(inv::count(inventory,inv::Item::Bread)==0);CHECK(inv::reconcile(inventory,true,true,true)&&inv::count(inventory,inv::Item::Bread)==0);CHECK(inv::grant_loan(inventory)&&inv::count(inventory,inv::Item::Bread)==0);
 // Legacy players still receive the exact old loan kit and never get tagged as
 // battlefield survivors just because they accepted the original sword.
 inv::State legacy;CHECK(inv::grant_loan(legacy));const auto old=inv::loan_kit();CHECK(inv::encode(legacy)==inv::encode(old));CHECK(inv::count(legacy,inv::Item::SwordShield)==1&&inv::count(legacy,inv::Item::TravelArmor)==1);CHECK(!(legacy.receipts&inv::battlefield_receipt));const auto oldbytes=inv::encode(legacy);CHECK(inv::decode(oldbytes,loaded,error)&&inv::encode(loaded)==oldbytes);CHECK(inv::matches_story(legacy,true,false,false));
 // A complete opening checkpoint can persist a stolen sword before the Offer
 // while the first quest's weapon_unlocked flag remains intentionally false.
 config.first_journey_enabled=true;config.arrival_camp.enabled=true;const auto center=config.first_journey.camp;config.arrival_camp.center={center.x,center.y,center.z};config.arrival_camp.actors[0].feet={center.x-2.15,center.y,center.z-.65};config.arrival_camp.actors[1].feet={center.x+2.2,center.y,center.z-.65};config.arrival_camp.actors[2].feet={center.x,center.y,center.z+2.4};config.meadow_spawn.feet={center.x,center.y,center.z+5.2};config.meadow_goal=config.meadow_spawn.feet;CHECK(op::validate(config,error));fj::State journey;CHECK(journey.configure(config.first_journey,error));opening=op::checkpoint_save(op::Checkpoint::MeadowStart,config);opening.first_journey_present=true;opening.first_journey=journey.stable_save();opening.inventory_present=true;opening.inventory={};CHECK(inv::grant_battlefield_sword(opening.inventory));opening.battlefield_progress=battlefield;CHECK(op::validate_save(opening,config,error));CHECK(op::encode_save(opening,config,bytes,error));CHECK(decode_opening(bytes,config,parsed,error)&&parsed.inventory_present&&parsed.first_journey_present);CHECK(!parsed.first_journey.weapon_unlocked);CHECK(parsed.inventory.receipts==inv::battlefield_receipt&&parsed.battlefield_progress==battlefield);
 // Changed dialogue acknowledges the battle and the sword without altering old
 // dialogues, beat counts, route signatures, or quest progression semantics.
 for(auto d:{fj::Dialogue::None,fj::Dialogue::Offer,fj::Dialogue::Return,fj::Dialogue::Town})for(unsigned beat=0;beat<9;++beat){const auto a=fj::dialogue_line(d,beat),b=fj::dialogue_line(d,beat,false);CHECK(std::string(a.text)==b.text&&std::string(a.speaker)==b.speaker);if(d==fj::Dialogue::Town)CHECK(std::string(a.text)==fj::dialogue_line(d,beat,true).text);}
 CHECK(std::string(fj::dialogue_line(fj::Dialogue::Offer,3).text).find("Take this old sword")!=std::string::npos);CHECK(std::string(fj::dialogue_line(fj::Dialogue::Offer,3,true).text).find("sword you brought")!=std::string::npos);CHECK(std::string(fj::dialogue_line(fj::Dialogue::Return,0,true).text).find("battlefield")!=std::string::npos);CHECK(std::string(fj::dialogue_line(fj::Dialogue::Offer,999,true).text).empty());
 // Both first camp visits and completed two-line revisits acknowledge the
 // escaped battle. Existing three-argument dialogue remains unchanged.
 for(unsigned actor=0;actor<3;++actor)for(bool repeat:{false,true})for(unsigned beat=0;beat<8;++beat){
  const auto legacy=world_opening_arrival_camp::line(actor,beat,repeat);
  const auto explicit_legacy=world_opening_arrival_camp::line(actor,beat,repeat,false);
  const auto battle=world_opening_arrival_camp::line(actor,beat,repeat,true);
  CHECK(std::string(legacy.text)==explicit_legacy.text&&std::string(legacy.speaker)==explicit_legacy.speaker);
  CHECK(std::string(battle.text).empty()==(beat>=(repeat?2u:6u)));
 }
 CHECK(std::string(world_opening_arrival_camp::line(0,0,false,true).text).find("fighting")!=std::string::npos);
 CHECK(std::string(world_opening_arrival_camp::line(1,0,false,true).text).find("sword")!=std::string::npos);
 CHECK(std::string(world_opening_arrival_camp::line(2,1,false,true).text).find("arrows")!=std::string::npos);
 {world_opening_arrival_camp::State people;CHECK(!people.repeat());CHECK(people.begin(0)&&!people.repeat());for(unsigned beat=0;beat<6;++beat)people.advance();CHECK(people.begin(0)&&people.repeat());people.cancel();CHECK(!people.repeat());}
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"old_v1_compatible\":true,\"stolen_sword_before_offer\":true,\"offer_no_kit_refill\":true,\"legacy_loan_kit_unchanged\":true,\"nested_battlefield_bounded\":true}\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}

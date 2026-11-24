#include "engine/world_arena_combat.h"
#include "engine/world_player_profile.h"
#include <cstdio>
#include <filesystem>
#include <limits>

using Kind=world_arena_opponents::Kind;
namespace ap=world_arena_progress;
static unsigned checks=0;
#define CHECK(v) do{++checks;if(!(v)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#v);return 1;}}while(0)
static std::string with_checksum(const std::string &body){
    std::ostringstream out;out<<body<<"checksum "<<std::hex<<std::setw(16)<<std::setfill('0')<<ap::fingerprint(body)<<'\n';return out.str();
}
int main(int argc,char **argv){
    CHECK(argc==2);
    const std::filesystem::path directory=argv[1];
    CHECK(world_arena_opponents::valid(Kind::Human)&&world_arena_opponents::valid(Kind::Orc));
    CHECK(!world_arena_opponents::valid(Kind(2))&&!world_arena_opponents::valid(Kind(~0u)));
    CHECK(world_arena_opponents::available(Kind::Human,false));
    CHECK(!world_arena_opponents::available(Kind::Orc,false)&&world_arena_opponents::available(Kind::Orc,true));
    CHECK(!world_arena_opponents::available(Kind(2),true));
    CHECK(ap::Config{}.opponents==Kind::Human&&world_arena_combat::Config{}.opponents==Kind::Human);
    CHECK((ap::Config{8,1}==ap::Config{8,1,Kind::Human}));
    CHECK((ap::Config{8,1,Kind::Human}!=ap::Config{8,1,Kind::Orc}));
    std::string error,bytes;
    ap::Progress initial,parsed;
    // Independent fixed legacy record: no new species field is allowed here.
    const std::string legacy="VALWOOD_ARENA_PROGRESS 1\nselection 8 1\nledger 0 0 0 1 0\nactive 0 8 1\n";
    CHECK(ap::encode(initial,bytes,error)&&bytes==with_checksum(legacy));
    CHECK(ap::parse(bytes,parsed,error)&&parsed.selection.opponents==Kind::Human&&parsed.active_config.opponents==Kind::Human);
    const auto untouched=ap::payload(parsed);
    for(Kind kind:{Kind::Human,Kind::Orc})for(unsigned total=1;total<=8;++total)for(unsigned concurrent=1;concurrent<=total;++concurrent){
        const ap::Config config{total,concurrent,kind};
        CHECK(ap::valid(config)&&ap::payout(config)==total*10);
        CHECK(world_arena_combat::valid({total,concurrent,kind}));
        ap::Progress available_draft=initial;std::uint64_t available_run=53;
        const bool available=ap::allocate_available(initial,config,false,available_draft,available_run,error);
        CHECK(available==(kind==Kind::Human));
        if(kind==Kind::Orc)CHECK(available_run==53&&ap::payload(available_draft)==untouched&&ap::payload(initial)==untouched);
        else CHECK(available_run==1&&available_draft.active_config==config);
        CHECK(ap::allocate_available(initial,config,true,available_draft,available_run,error)&&available_run==1&&available_draft.active_config==config);
        ap::Progress active;std::uint64_t run=0;
        CHECK(ap::allocate(initial,config,active,run,error)&&run==1&&active.next_run_id==2);
        CHECK(active.selection==config&&active.active_config==config&&active.balance==0);
        CHECK(ap::encode(active,bytes,error)&&ap::parse(bytes,parsed,error));
        CHECK(parsed.selection==config&&parsed.active_config==config);
        CHECK(bytes.find(kind==Kind::Orc?"VALWOOD_ARENA_PROGRESS 3\n":"VALWOOD_ARENA_PROGRESS 1\n")==0);
        ap::Progress result;std::uint64_t award=99;
        CHECK(ap::settle(parsed,run,config,false,result,award,error)==ap::Settlement::Rejected&&award==0);
        const ap::Config wrong_species{total,concurrent,kind==Kind::Human?Kind::Orc:Kind::Human};
        CHECK(ap::settle(parsed,run,wrong_species,true,result,award,error)==ap::Settlement::Rejected&&award==0);
        CHECK(ap::settle(parsed,run,config,true,result,award,error)==ap::Settlement::Ready&&award==10*total);
        CHECK(result.balance==10*total&&result.wins==1&&result.opponents==total&&!result.active_run_id);
        ap::Progress duplicate;
        CHECK(ap::settle(result,run,config,true,duplicate,award,error)==ap::Settlement::AlreadySaved&&award==0&&duplicate.balance==result.balance);
        CHECK(ap::settle(result,run,wrong_species,true,duplicate,award,error)==ap::Settlement::Rejected&&award==0);
        ap::Progress retry;std::uint64_t next=0;
        CHECK(ap::allocate(result,wrong_species,retry,next,error)&&next==2&&retry.balance==result.balance&&retry.active_config==wrong_species);
        CHECK(ap::settle(retry,run,config,true,duplicate,award,error)==ap::Settlement::Rejected&&award==0);
        CHECK(ap::encode(retry,bytes,error)&&ap::parse(bytes,parsed,error)&&parsed.selection==wrong_species);
        CHECK(ap::settle(parsed,next,wrong_species,true,duplicate,award,error)==ap::Settlement::Ready&&duplicate.balance==20*total);
        world_arena_combat::Arena arena;WorldTrainingCombat fight;
        CHECK(arena.start(fight,{total,concurrent,kind}));
        CHECK(arena.config().opponents==kind&&arena.config().total==total&&arena.config().simultaneous==concurrent);
        const auto state=arena.state();const auto serial=fight.attack_start_serial;
        CHECK(!arena.start(fight,{total,concurrent,Kind(2)}));
        CHECK(arena.state()==state&&arena.config().opponents==kind&&fight.attack_start_serial==serial);
    }
    for(ap::Config invalid: {ap::Config{0,1,Kind::Orc},ap::Config{9,1,Kind::Orc},ap::Config{2,0,Kind::Orc},ap::Config{2,3,Kind::Orc},ap::Config{2,1,Kind(2)}}){
        CHECK(!ap::valid(invalid)&&!ap::payout(invalid));
        ap::Progress out;std::uint64_t id=73;
        CHECK(!ap::allocate(initial,invalid,out,id,error)&&id==73&&ap::payload(initial)==untouched);
    }
    ap::Progress orcs;std::uint64_t run=0;
    CHECK(ap::allocate(initial,{3,2,Kind::Orc},orcs,run,error));
    CHECK(ap::encode(orcs,bytes,error));
    auto altered=bytes;altered.replace(altered.find("opponents 1 1"),13,"opponents 0 1");
    ap::Progress preserved=initial;
    CHECK(!ap::parse(altered,preserved,error)&&ap::payload(preserved)==untouched);
    for(const std::string &record:{
        "VALWOOD_ARENA_PROGRESS 3\nselection 3 2\nledger 0 0 0 2 0\nactive 1 3 2\nopponents 2 1\n",
        "VALWOOD_ARENA_PROGRESS 3\nselection 3 2\nledger 0 0 0 2 0\nactive 1 3 2\nopponents 1 -1\n",
        "VALWOOD_ARENA_PROGRESS 3\nselection 3 2\nledger 0 0 0 2 0\nactive 1 3 2\nopponents 0 0\n",
        "VALWOOD_ARENA_PROGRESS 3\nselection 3 2\nledger 0 0 0 2 0\nactive 1 3 2\nopponents 1 1\nopponents 1 1\n",
        "VALWOOD_ARENA_PROGRESS 5\nselection 3 2\nledger 0 0 0 2 0\nactive 1 3 2\nopponents 1 1\n"}){
        CHECK(!ap::parse(with_checksum(record),preserved,error)&&ap::payload(preserved)==untouched);
    }
    // Legacy inventory v2 and Orc inventory v4 both remain supported.
    auto inventory=initial;inventory.inventory_present=true;inventory.inventory=world_player_inventory::loan_kit(true);
    CHECK(ap::encode(inventory,bytes,error)&&bytes.find("VALWOOD_ARENA_PROGRESS 2\n")==0);
    CHECK(ap::parse(bytes,parsed,error)&&parsed.inventory_present&&parsed.selection.opponents==Kind::Human);
    inventory.selection.opponents=Kind::Orc;
    CHECK(ap::encode(inventory,bytes,error)&&bytes.find("VALWOOD_ARENA_PROGRESS 4\n")==0);
    CHECK(ap::parse(bytes,parsed,error)&&parsed.inventory_present&&parsed.selection.opponents==Kind::Orc&&parsed.active_config.opponents==Kind::Human);
    // Mixed selected/active species are checksummed without changing receipts.
    auto mixed=orcs;mixed.selection.opponents=Kind::Human;
    CHECK(ap::encode(mixed,bytes,error)&&ap::parse(bytes,parsed,error));
    CHECK(parsed.selection.opponents==Kind::Human&&parsed.active_config.opponents==Kind::Orc);
    // Round-trip the real nested shared profile and atomic on-disk record.
    world_player_profile::State profile;profile.inventory=world_player_inventory::loan_kit(true);profile.arena=orcs;profile.arena_imported=true;
    const auto profile_bytes=world_player_profile::encode(profile);world_player_profile::State decoded;
    CHECK(!profile_bytes.empty()&&world_player_profile::decode(profile_bytes,decoded,error)&&decoded.arena.active_config.opponents==Kind::Orc);
    auto corrupt_profile=profile_bytes;corrupt_profile[corrupt_profile.find("opponents 1 1")+10]='0';
    CHECK(!world_player_profile::decode(corrupt_profile,decoded,error)&&decoded.arena.active_config.opponents==Kind::Orc);
    CHECK(world_player_profile::save(directory/"player.profile",profile,error));
    CHECK(world_player_profile::load(directory/"player.profile",decoded,error)==ap::Load::Loaded&&decoded.arena.selection.opponents==Kind::Orc);
    const auto unchanged_profile=world_player_profile::encode(decoded);
    ap::Progress unavailable_draft=initial;std::uint64_t unavailable_run=91;
    CHECK(!ap::allocate_available(decoded.arena,{8,8,Kind::Orc},false,unavailable_draft,unavailable_run,error));
    CHECK(unavailable_run==91&&ap::payload(unavailable_draft)==untouched&&world_player_profile::encode(decoded)==unchanged_profile);
    CHECK(world_player_profile::load(directory/"player.profile",decoded,error)==ap::Load::Loaded&&world_player_profile::encode(decoded)==unchanged_profile);
    CHECK(ap::save(directory/"arena.progress",orcs,error));
    CHECK(ap::load(directory/"arena.progress",parsed,error)==ap::Load::Loaded&&parsed.active_config.opponents==Kind::Orc);
    auto invalid_save=orcs;invalid_save.selection.opponents=Kind(2);
    CHECK(!ap::save(directory/"arena.progress",invalid_save,error));
    CHECK(ap::load(directory/"arena.progress",parsed,error)==ap::Load::Loaded&&parsed.active_config.opponents==Kind::Orc);
    std::printf("PASS arena opponents: %u checks; 72 Human/Orc configs, unavailable-Orc no-allocation/no-profile-mutation, legacy v1/v2 and Orc v3/v4, nested saves, checksum rejection, retry and species-bound one-time rewards\n",checks);
}

#ifndef VALWOOD_WORLD_PLAYER_PROFILE_H
#define VALWOOD_WORLD_PLAYER_PROFILE_H
#include "world_arena_progress.h"
// One durable character ledger. Checkpoint loads never refund purchases or
// replay receipts; quest tokens themselves remain local to the story.
namespace world_player_profile {
using Item=world_player_inventory::Item;
struct State {
 world_player_inventory::State inventory;
 world_arena_progress::Progress arena;
 std::uint64_t story_coins=0,revision=0;
 bool story_imported=false,arena_imported=false;
};
inline std::uint64_t earned(const State &s){return s.story_coins+s.arena.balance;}
inline bool valid(const State &s){
 return s.revision<=100000000000ull&&s.story_coins<=12&&world_arena_progress::valid(s.arena)&&!s.arena.inventory_present&&
  world_player_inventory::valid(s.inventory)&&s.inventory.spent<=earned(s)&&
  !world_player_inventory::count(s.inventory,Item::Satchel)&&!world_player_inventory::count(s.inventory,Item::WatchLetter);
}
inline void strip_quest(world_player_inventory::State &s){s.counts[unsigned(Item::Satchel)]=s.counts[unsigned(Item::WatchLetter)]=0;}
inline bool import(State &s,const world_player_inventory::State &inventory,std::uint64_t story_coins,const world_arena_progress::Progress *arena,std::string &error){
 if(!world_player_inventory::valid(inventory)||story_coins>12||(arena&&!world_arena_progress::valid(*arena))){error="Existing progress is invalid. It was preserved.";return false;}
 const bool done=arena?s.arena_imported:s.story_imported;if(done)return true;
 State n=s;auto incoming=inventory;strip_quest(incoming);
 for(unsigned i=1;i<incoming.counts.size();++i){
  const auto limit=world_player_inventory::definitions[i].limit;
  const auto combined=std::uint64_t(n.inventory.counts[i])+incoming.counts[i];
  if(world_player_inventory::definitions[i].kind==world_player_inventory::Kind::Food&&combined>limit){error="Combine your provision stacks before migrating this profile.";return false;}
  n.inventory.counts[i]=unsigned(std::min<std::uint64_t>(combined,limit));
 }
 n.inventory.receipts|=incoming.receipts;n.inventory.spent+=incoming.spent;
 // A later legacy import retains the active shared outfit; all owned gear
 // is retained. The offline first migration explicitly selects its outfit.
 if(incoming.weapon!=Item::None&&n.inventory.weapon==Item::None){n.inventory.weapon=incoming.weapon;n.inventory.armor=incoming.armor;}
 if(arena){n.arena=*arena;n.arena.inventory_present=false;n.arena.inventory={};n.arena_imported=true;}
 else{n.story_coins=story_coins;n.story_imported=true;}
 ++n.revision;if(!valid(n)){error="Existing balances cannot be combined safely.";return false;}s=n;error.clear();return true;
}
inline bool reconcile(State &s,bool loan,bool supplies,bool reward,std::string &error){
 auto n=s;
 if(!world_player_inventory::reconcile(n.inventory,loan,supplies,reward)){error="Could not grant the story items.";return false;}
 strip_quest(n.inventory);if(reward)n.story_coins=12;
 if(!valid(n)){error="The player profile could not be verified.";return false;}
 s=n;return true;
}
inline world_player_inventory::State story_snapshot(const State &s,bool loan,bool supplies,bool reward){
 auto out=s.inventory;using namespace world_player_inventory;
 // Backward-compatible checkpoint projection. The profile owns actual spending,
 // while this snapshot still obeys the original quest/checkpoint validation.
 out.counts[unsigned(Item::Bow)]=0;if(out.weapon==Item::Bow)out.weapon=count(out,Item::Sword)?Item::Sword:Item::None;
 out.receipts=(loan?loan_receipt:0)|(supplies?satchel_receipt:0)|(reward?reward_receipt:0)|((loan&&(s.inventory.receipts&loot_receipt))?loot_receipt:0);
 out.counts[unsigned(Item::Satchel)]=unsigned(supplies&&!reward);out.counts[unsigned(Item::WatchLetter)]=unsigned(reward);
 out.spent=std::min<std::uint64_t>(out.spent,reward?12:0);return out;
}
inline std::string encode(const State &s){
 if(!valid(s))return {};std::string arena,error;if(!world_arena_progress::encode(s.arena,arena,error))return {};
 std::ostringstream o;o<<"VALWOOD_PLAYER_PROFILE 1\nrevision "<<s.revision<<"\nimports "<<unsigned(s.story_imported)<<' '<<unsigned(s.arena_imported)<<"\nstory "<<s.story_coins<<"\ninventory "<<std::quoted(world_player_inventory::encode(s.inventory))<<"\narena "<<std::quoted(arena)<<"\n";
 auto body=o.str();std::ostringstream result;result<<body<<"checksum "<<std::hex<<std::setw(16)<<std::setfill('0')<<world_arena_progress::fingerprint(body)<<'\n';return result.str();
}
inline bool decode(const std::string &bytes,State &out,std::string &error){
 if(bytes.empty()||bytes.size()>8192){error="The player profile is empty or too large.";return false;}
 State n;std::istringstream in(bytes);std::string tag,inv,arena,extra;unsigned version=0,a=0,b=0;std::uint64_t hash=0;
 if(!(in>>tag>>version)||tag!="VALWOOD_PLAYER_PROFILE"||version!=1||!(in>>tag>>n.revision)||tag!="revision"||!(in>>tag>>a>>b)||tag!="imports"||a>1||b>1||!(in>>tag>>n.story_coins)||tag!="story"||!(in>>tag>>std::quoted(inv))||tag!="inventory"||!(in>>tag>>std::quoted(arena))||tag!="arena"||!(in>>tag>>std::hex>>hash)||tag!="checksum"||(in>>extra)){error="The player profile could not be verified. Existing progress was preserved.";return false;}
 n.story_imported=a;n.arena_imported=b;
 if(!world_player_inventory::decode(inv,n.inventory,error)||!world_arena_progress::parse(arena,n.arena,error)||!valid(n)||encode(n)!=bytes){error="The player profile checksum or contents are invalid. Existing progress was preserved.";return false;}
 out=n;error.clear();return true;
}
inline world_arena_progress::Load load(const std::filesystem::path &path,State &out,std::string &error){
 std::error_code ec;if(!std::filesystem::exists(path,ec)){if(ec){error=ec.message();return world_arena_progress::Load::IoError;}out={};return world_arena_progress::Load::Missing;}
 auto size=std::filesystem::file_size(path,ec);if(ec||size>8192){error="Could not read the player profile safely.";return world_arena_progress::Load::Invalid;}
 std::ifstream f(path,std::ios::binary);std::string bytes(size,'\0');if(!f||!f.read(bytes.data(),size)){error="Could not read the player profile.";return world_arena_progress::Load::IoError;}
 return decode(bytes,out,error)?world_arena_progress::Load::Loaded:world_arena_progress::Load::Invalid;
}
inline bool save(const std::filesystem::path &path,const State &s,std::string &error){
 auto bytes=encode(s);if(bytes.empty()){error="Invalid player profile. Existing progress was preserved.";return false;}
 std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);if(ec){error="Could not create the player save folder.";return false;}
 const auto pattern=path.u8string()+".tmp-XXXXXX";std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);int fd=mkstemp(name.data());if(fd<0){error="Could not stage player progress.";return false;}
 auto *f=fdopen(fd,"wb");bool ok=false;if(f){ok=std::fwrite(bytes.data(),1,bytes.size(),f)==bytes.size()&&std::fflush(f)==0&&fsync(fd)==0;if(std::fclose(f))ok=false;}else close(fd);
 if(ok)ok=vw_replace(name.data(),path.u8string().c_str())==0;
 if(!ok){unlink(name.data());error="Could not save player progress. Previously saved items and coins were preserved.";return false;}error.clear();return true;
}
}
#endif

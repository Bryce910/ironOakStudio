// Test fixture only: serialize an unseen arrival checkpoint with production APIs.
#include "engine/world_opening_prologue.h"
#include "engine/world_opening_cinematic_settings.h"
#include <filesystem>
#include <fstream>
#include <iostream>
int main(int argc,char **argv){
 if(argc!=2)return 2;
 const std::filesystem::path root=argv[1];
 std::ifstream marker(root/".valwood-npc-intro-qa");std::string tag,extra,error;unsigned version=0;
 if(root.string().rfind("/tmp/valwood-opening-focus-",0)!=0||!(marker>>tag)||tag!="VALWOOD_PRIVATE_NPC_INTRO_V1"||(marker>>extra))return 2;
 world_opening_prologue::Config config;std::ifstream file(root/"opening.config");
 auto record=[&](const char*s){return bool(file>>tag)&&tag==s;};
 auto vec=[&](world_opening_prologue::Vec&v){return bool(file>>v.x>>v.y>>v.z);};
 auto pose=[&](world_opening_prologue::Pose&p){return vec(p.feet)&&bool(file>>p.yaw>>p.pitch);};
 auto rail=[&](world_opening_prologue::Rail&r){return vec(r.eye_from)&&vec(r.eye_to)&&vec(r.target_from)&&vec(r.target_to)&&bool(file>>r.fov_from>>r.fov_to);};
 bool ok=bool(file>>tag>>version)&&tag=="VALWOOD_OPENING_SCENE"&&version==1&&
  record("house_spawn")&&pose(config.house_spawn)&&record("meadow_spawn")&&pose(config.meadow_spawn)&&
  record("house_focus")&&vec(config.house_focus)&&record("house_exit")&&vec(config.house_exit)&&record("meadow_goal")&&vec(config.meadow_goal)&&
  record("house_rail")&&rail(config.house_rail)&&record("transport_rail")&&rail(config.transport_rail)&&record("meadow_rail")&&rail(config.meadow_rail);
 unsigned home_count=0,meadow_count=0;world_opening_prologue::Vec waypoint;
 while(ok&&file>>tag){if(tag=="house_waypoint")++home_count;else if(tag=="meadow_waypoint")++meadow_count;else{ok=false;break;}ok=vec(waypoint)&&world_opening_prologue::detail::valid(waypoint);}
 std::ifstream camp(root/"opening/arrival-camp.settings"),journey(root/"opening/first-journey.settings"),intro(root/"opening/npc-intro.settings"),cinema(root/"opening/cinematic.settings");
 ok=ok&&file.eof()&&home_count>=5&&meadow_count>=2&&
  world_opening_arrival_camp::parse(camp,config.arrival_camp,error)&&
  world_first_journey::parse_config(journey,config.first_journey,error)&&
  world_npc_intro::parse(intro,config.npc_intro,error)&&
  world_opening_cinematic_settings::parse(cinema,config,config.house_shots,error);
 config.first_journey_enabled=true;config.game_start_story=true;
 world_first_journey::State first;
 if(!ok||!world_opening_prologue::validate(config,error)||!first.configure(config.first_journey,error)){std::cerr<<"Intro fixture config: "<<error<<'\n';return 1;}
 auto saved=world_opening_prologue::checkpoint_save(world_opening_prologue::Checkpoint::MeadowStart,config);
 saved.first_journey_present=true;saved.first_journey=first.stable_save();
 saved.inventory_present=true;saved.inventory={};saved.npc_intro_seen=false;
 std::string bytes;if(!world_opening_prologue::encode_save(saved,config,bytes,error)){std::cerr<<error<<'\n';return 1;}
 std::ofstream seed(root/"opening/npc-intro.qa.seed",std::ios::binary),progress(root/"saves/opening.progress",std::ios::binary);
 seed<<bytes;progress<<bytes;seed.close();progress.close();
 if(!seed||!progress)return 1;
 std::cout<<"Production-encoded unseen MeadowStart; camp0, no quest or inventory credit\n";
}

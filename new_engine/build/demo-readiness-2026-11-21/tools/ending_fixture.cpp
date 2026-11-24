#include "engine/world_first_journey.h"
#include <fstream>
#include <iostream>
#include <iomanip>
int main(int argc,char **argv){
 if(argc!=2)return 2;
 world_first_journey::Config c;std::ifstream f(argv[1]);std::string err,out;
 if(!world_first_journey::parse_config(f,c,err)){std::cerr<<err;return 1;}
 world_first_journey::Save s;s.layout_signature=world_first_journey::signature(c);s.stage=world_first_journey::Stage::Complete;s.camp_spoken=7;
 s.weapon_unlocked=s.encounter_won=s.supplies_recovered=s.reward_granted=s.town_reached=true;
 s.coins=world_first_journey::reward_coins;s.food=world_first_journey::reward_food;s.reputation=world_first_journey::reward_reputation;
 if(!world_first_journey::encode_save(s,c,out,err)){std::cerr<<err;return 1;}
 std::cout<<std::quoted(out)<<"\n";
}

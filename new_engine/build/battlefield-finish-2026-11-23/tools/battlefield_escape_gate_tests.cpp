// Checks the real viewer-owned trigger value against the actual production
// combat simulator, so an activation outside its leash cannot regress silently.
#include "engine/world_training_combat.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": " #x);}while(0)
int main(int argc,char **argv){try{
 CHECK(argc==2);std::ifstream file(argv[1]);CHECK(bool(file));std::string source((std::istreambuf_iterator<char>(file)),{});std::smatch match;
 CHECK(std::regex_search(source,match,std::regex("constexpr double battlefield_activation_radius=([0-9.]+);")));
 const double radius=std::stod(match[1]);CHECK(std::isfinite(radius)&&radius>2&&radius<9);
 // Reproduce the original exact failure: the old 14m trigger cancelled its
 // own countdown on the first ordinary combat tick, before AI could approach.
 {WorldTrainingCombat c;CHECK(c.start_threat());c.update(1./60.,c.enemy_x+13.99,c.enemy_z);CHECK(c.state==TrainingState::Cancelled&&!c.engaged());}
 // Every supported activation along the full trigger arc survives the same
 // real update, at low as well as high render rates.
 for(double dt:{1./120.,1./60.,1./30.,1./15.,.25})for(double angle:{0.,.4,1.1,2.4,3.141592653589793,5.8}){
  WorldTrainingCombat c;c.health=57;c.stamina=71;CHECK(c.start_threat());const double d=radius-.0001;
  c.update(dt,c.enemy_x+std::sin(angle)*d,c.enemy_z+std::cos(angle)*d);CHECK(c.state==TrainingState::Countdown&&c.engaged());
  // Once closed to physical sword range, this is a genuinely live threat.
  for(unsigned frame=0;frame<400&&c.state==TrainingState::Countdown;++frame)c.update(dt,c.enemy_x+1.55,c.enemy_z);
  CHECK(c.state==TrainingState::Fighting&&c.enemy_health>0);
 }
 // Retreat still cancels safely. The host's recovery must not count it as a
 // win or leave its slot selected; inspect the actual production branch.
 CHECK(source.find("if(battlefield_enemy_index>=0&&fight.state==TrainingState::Cancelled)")!=std::string::npos);
 const auto start=source.find("if(battlefield_enemy_index>=0&&fight.state==TrainingState::Cancelled)");const auto end=source.find("if(battlefield_enemy_index>=0&&fight.state==TrainingState::Won)",start);CHECK(end!=std::string::npos);
 const auto recovery=source.substr(start,end-start);CHECK(recovery.find("battlefield_enemy_index=-1")!=std::string::npos);CHECK(recovery.find("++battlefield_wins")==std::string::npos);CHECK(recovery.find("fight.health=health;fight.stamina=stamina")!=std::string::npos);
 CHECK(source.find("world_combat_motion_arena_enemy_walk(combat_motion,")==std::string::npos);CHECK(source.find("world_combat_motion_restore_defeated(combat_motion,0)")==std::string::npos);
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"activation_radius\":"<<radius<<",\"old_14m_cancellation_reproduced\":true,\"production_trigger_inside_actual_leash\":true}\n";return 0;
}catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}}

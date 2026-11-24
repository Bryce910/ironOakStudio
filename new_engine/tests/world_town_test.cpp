#include "engine/world_town.h"
#include "engine/world_training_combat.h"
#include <iostream>
#include <limits>
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<'\n';return 1;}}while(0)
int main(){using namespace world_town;State s;std::string message;
 CHECK(valid(s)&&purse(s,0)==0);CHECK(!transact(s,Action::Bread,0,message)&&s.bread==0&&s.spent==0);
 CHECK(transact(s,Action::Welcome,0,message)&&s.bread==1&&s.welcome==1);CHECK(!transact(s,Action::Welcome,0,message));
 CHECK(transact(s,Action::EatBread,0,message)&&s.prepared==2&&s.bread==0);CHECK(!transact(s,Action::EatBread,0,message));
 CHECK(transact(s,Action::Sharpen,25,message));CHECK(transact(s,Action::Rest,25,message));CHECK(s.prepared==7&&purse(s,25)==14);
 CHECK(!transact(s,Action::Meal,25,message)&&s.spent==11);const auto b=consume(s);CHECK(b.damage==1.15f&&b.resistance==.9f&&b.recovery==1.25f&&s.prepared==0);
 const auto neutral=consume(s);CHECK(neutral.damage==1&&neutral.resistance==1&&neutral.recovery==1);
 for(int i=0;i<7;i++){CHECK(transact(s,Action::Apples,25,message));}CHECK(!transact(s,Action::Apples,25,message)&&s.spent==25&&s.apples==7);
 CHECK(!transact(s,Action::Bread,0,message)&&s.spent==25);s={};for(int i=0;i<8;i++){CHECK(transact(s,Action::Bread,100,message));}CHECK(!transact(s,Action::Bread,100,message)&&s.bread==8&&s.spent==24);
 State bad=s;bad.prepared=8;CHECK(!valid(bad)&&!transact(bad,Action::Welcome,100,message));bad=s;bad.welcome=2;CHECK(!valid(bad));bad=s;bad.spent=-1;CHECK(!valid(bad));
 CHECK(nearby(-1297,24,-10305.3)==1);CHECK(nearby(-1297,29,-10305.3)==-1);CHECK(nearby(-1297,24,-10305.3,true)==-1);CHECK(nearby(std::numeric_limits<double>::quiet_NaN(),24,0)==-1);
 // Benefits actually change combat outcomes and expire at the next start.
 WorldTrainingCombat plain,prepared;CHECK(plain.start_threat()&&prepared.start_threat());prepared.town_damage=b.damage;prepared.town_resistance=b.resistance;prepared.town_recovery=b.recovery;
 for(int i=0;i<35;i++){plain.update(.1,WorldTrainingCombat::enemy_x-5,WorldTrainingCombat::enemy_z);prepared.update(.1,WorldTrainingCombat::enemy_x-5,WorldTrainingCombat::enemy_z);}
 plain.stamina=prepared.stamina=40;plain.regen_delay=prepared.regen_delay=0;plain.update(.1,WorldTrainingCombat::enemy_x-5,WorldTrainingCombat::enemy_z);prepared.update(.1,WorldTrainingCombat::enemy_x-5,WorldTrainingCombat::enemy_z);CHECK(prepared.stamina>plain.stamina);
 // Compare actual contact outcomes under the same input and distance.
 plain.enemy_phase=prepared.enemy_phase=TrainingEnemyPhase::Recovery;plain.enemy_time=prepared.enemy_time=0;plain.enemy_duration=prepared.enemy_duration=100;
 plain.choose(TrainingDirection::Left);prepared.choose(TrainingDirection::Left);CHECK(plain.attack()&&prepared.attack());
 for(int i=0;i<5;i++){plain.update(.1,WorldTrainingCombat::enemy_x-2,WorldTrainingCombat::enemy_z);prepared.update(.1,WorldTrainingCombat::enemy_x-2,WorldTrainingCombat::enemy_z);}
 CHECK(prepared.enemy_health<plain.enemy_health&&plain.hits==1&&prepared.hits==1);
 plain.health=prepared.health=100;plain.guard=prepared.guard=false;plain.enemy_phase=prepared.enemy_phase=TrainingEnemyPhase::Windup;plain.enemy_time=prepared.enemy_time=0;plain.enemy_duration=prepared.enemy_duration=.01;
 plain.update(.02,WorldTrainingCombat::enemy_x-2,WorldTrainingCombat::enemy_z);prepared.update(.02,WorldTrainingCombat::enemy_x-2,WorldTrainingCombat::enemy_z);CHECK(prepared.health>plain.health&&plain.health<100);
 prepared.leave();CHECK(prepared.start_threat()&&prepared.town_damage==1&&prepared.town_resistance==1&&prepared.town_recovery==1);
 std::cout<<"world_town: "<<checks<<" transaction, no overdraft, capacity, one-time welcome, benefits and proximity checks\n";
}

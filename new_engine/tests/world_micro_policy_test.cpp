#include "engine/world_micro_policy.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <unistd.h>
using namespace world_living;
int main(int argc,char **argv){
    using namespace world_micro_policy;
    Model m;m.second_bias[1]=8;std::string error;assert(valid(m,error));
    Definition d;d.enabled=true;d.start_minute=600;d.factions.push_back({1,"Valwood",{}});
    Settlement s;s.id=s.faction=1;s.name="Castle";s.stock={10,10,10,10};s.reserve={100,100,100,100};d.settlements.push_back(s);
    Actor a;a.id=a.faction=a.home=1;a.name="Farmer";d.actors.push_back(a);World world;assert(world.configure(d,0,error));
    const auto &state=world.state();Decision decision{&world.definition(),&world.definition().actors[0],&state.actors[0],&state.settlements[0],state.tick,world.day(),3,world.minute(),&state};
    std::array<float,features> input;assert(encode_features(decision,input));assert(input[unsigned(Activity::Work)]==1&&input[6]>.009&&input[6]<.011);
    Action action;float confidence=0;assert(predict(m,input,action,confidence)&&action==Action::Gather&&confidence>.99);
    Context context{&m};Proposal p;assert(policy(decision,p,&context));assert(p.amount==2&&p.action==Action::Gather);assert(world.propose(p,error));
    assert(world.state().settlements[0].stock[0]==12&&world.state().settlements[0].reserve[0]==98);
    // A confident but infeasible learned action falls back to the rules.
    Model bad_choice;bad_choice.second_bias[4]=8;context.model=&bad_choice;assert(!policy(decision,p,&context));
    auto malformed=m;malformed.first[0]=std::numeric_limits<float>::quiet_NaN();assert(!predict(malformed,input,action,confidence));input[0]=std::numeric_limits<float>::infinity();assert(!predict(m,input,action,confidence));
    char path[]="/tmp/valwood-micro-policy-XXXXXX";int fd=mkstemp(path);assert(fd>=0);close(fd);
    {std::ofstream file(path);file<<"VALWOOD_MICRO_POLICY 1 18 24 5\n0\n";}
    auto kept=m;assert(!load(path,kept,error));assert(kept.second_bias==m.second_bias);unlink(path);
    if(argc>1){assert(load(argv[1],kept,error));assert(encode_features(decision,input));assert(predict(kept,input,action,confidence)&&action==Action::Gather);}
    std::cout<<"world micro-policy inference and authoritative proposal guards: passed\n";
}

#include "engine/world_micro_policy.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
namespace world_micro_policy {
namespace {
template<class T>bool weights(const T &a){for(float v:a)if(!std::isfinite(v)||std::abs(v)>32)return false;return true;}
const world_living::Settlement *settlement(const world_living::Definition &d,world_living::Id id,unsigned *index=nullptr){for(unsigned i=0;i<d.settlements.size();i++)if(d.settlements[i].id==id){if(index)*index=i;return &d.settlements[i];}return nullptr;}
float ratio(std::uint64_t amount,std::uint64_t capacity){return capacity?float(std::min(1.,double(amount)/capacity)):0;}
}
bool valid(const Model &m,std::string &error){error.clear();if(!weights(m.first)||!weights(m.first_bias)||!weights(m.second)||!weights(m.second_bias)){error="Micro-policy has non-finite or excessive weights";return false;}return true;}
bool load(const char *path,Model &out,std::string &error){
    error.clear();if(!path||!*path){error="Choose a micro-policy file";return false;}
    std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file||file.tellg()<0||file.tellg()>65536){error="Missing or oversized micro-policy";return false;}file.seekg(0);
    std::string magic,extra;unsigned version=0,f=0,h=0,a=0;if(!(file>>magic>>version>>f>>h>>a)||magic!="VALWOOD_MICRO_POLICY"||version!=1||f!=features||h!=hidden||a!=actions){error="Unsupported micro-policy architecture";return false;}
    Model next;auto read=[&](auto &values){for(float &v:values)if(!(file>>v))return false;return true;};
    if(!read(next.first)||!read(next.first_bias)||!read(next.second)||!read(next.second_bias)||(file>>extra)||file.bad()||!valid(next,error)){if(error.empty())error="Malformed micro-policy weights";return false;}
    out=next;return true;
}
bool encode_features(const world_living::Decision &d,std::array<float,features> &out){
    if(!d.definition||!d.actor||!d.home||!d.actor_state||unsigned(d.actor_state->activity)>5||d.minute>=1440||unsigned(d.actor->work_resource)>=4)return false;
    const auto *home=settlement(*d.definition,d.actor->home);if(!home)return false;
    std::array<float,features> x={};x[unsigned(d.actor_state->activity)]=1;const unsigned r=unsigned(d.actor->work_resource);
    x[6]=ratio(d.home->stock[r],home->capacity[r]);x[7]=float(double(d.home->reserve[r])/(double(d.home->reserve[r])+100));
    unsigned ti=0;const auto *target=settlement(*d.definition,d.actor->trade_target,&ti);
    if(target)x[8]=ratio(d.world_state?d.world_state->settlements[ti].stock[r]:target->stock[r],target->capacity[r]);
    x[9]=ratio(d.home->stock[0],home->capacity[0]);const double phase=d.minute*6.283185307179586/1440.;x[10]=float(std::sin(phase));x[11]=float(std::cos(phase));x[12]=target?1:0;
    for(unsigned i=0;i<5;i++)x[13+i]=(d.actor->allowed_actions&(1u<<i))?1.f:0.f;
    out=x;return true;
}
bool predict(const Model &m,const std::array<float,features> &x,world_living::Action &action,float &confidence){
    if(!weights(m.first)||!weights(m.first_bias)||!weights(m.second)||!weights(m.second_bias))return false;
    for(float v:x)if(!std::isfinite(v)||std::abs(v)>1.001f)return false;
    std::array<float,hidden> layer={};std::array<float,actions> logits={};
    for(unsigned j=0;j<hidden;j++){float sum=m.first_bias[j];for(unsigned i=0;i<features;i++)sum+=x[i]*m.first[i*hidden+j];layer[j]=std::max(0.f,sum);}
    for(unsigned j=0;j<actions;j++){float sum=m.second_bias[j];for(unsigned i=0;i<hidden;i++)sum+=layer[i]*m.second[i*actions+j];if(!std::isfinite(sum))return false;logits[j]=sum;}
    const auto at=std::max_element(logits.begin(),logits.end());double total=0;for(float v:logits)total+=std::exp(double(v-*at));
    action=world_living::Action(unsigned(at-logits.begin()));confidence=float(1/total);return true;
}
bool policy(const world_living::Decision &d,world_living::Proposal &out,void *userdata){
    auto *context=static_cast<Context *>(userdata);if(!context||!context->model)return false;context->inferences++;
    auto fallback=[&](){context->fallbacks++;return false;};std::array<float,features> features_;world_living::Action action;float confidence=0;
    if(!encode_features(d,features_)||!predict(*context->model,features_,action,confidence)||confidence<.55f)return fallback();
    using namespace world_living;const auto &a=*d.actor;const auto &home=*d.home;const auto *home_def=settlement(*d.definition,a.home);const unsigned r=unsigned(a.work_resource);
    if(action==Action::None||!(a.allowed_actions&action_bit(action))||d.tick<d.actor_state->next_action_tick)return fallback();
    Proposal p;p.actor=a.id;p.source=a.home;p.action=action;p.resource=a.work_resource;const Activity activity=d.actor_state->activity;
    if(action==Action::Gather&&activity==Activity::Work)p.amount=std::min({std::uint64_t(a.work_amount),home.reserve[r],home_def->capacity[r]-std::min(home.stock[r],home_def->capacity[r])});
    else if(action==Action::Consume&&activity==Activity::Eat&&d.actor_state->last_meal_day!=d.day&&home.stock[0]>=a.ration){p.resource=Resource::Food;p.amount=a.ration;}
    else if(action==Action::Transfer&&activity==Activity::Work&&a.trade_target){
        unsigned i=0;const auto *target=settlement(*d.definition,a.trade_target,&i);if(!target||!d.world_state)return fallback();
        const auto stock=d.world_state->settlements[i].stock[r];p.target=a.trade_target;p.amount=std::min({std::uint64_t(a.work_amount),home.stock[r],target->capacity[r]-std::min(stock,target->capacity[r])});
    }else if(action!=Action::Patrol||activity!=Activity::Guard)return fallback();
    if(action!=Action::Patrol&&!p.amount)return fallback();
    out=p;context->accepted++;return true;
}
}

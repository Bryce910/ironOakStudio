#ifndef VALWOOD_WORLD_MICRO_POLICY_H
#define VALWOOD_WORLD_MICRO_POLICY_H
#include "engine/world_living.h"
#include <array>
#include <string>
namespace world_micro_policy {
constexpr unsigned features=18,hidden=24,actions=5;
struct Model {
    std::array<float,features*hidden> first={};std::array<float,hidden> first_bias={};
    std::array<float,hidden*actions> second={};std::array<float,actions> second_bias={};
};
struct Context {const Model *model=nullptr;std::uint64_t inferences=0,accepted=0,fallbacks=0;};
bool load(const char *path,Model &,std::string &error);
bool valid(const Model &,std::string &error);
bool encode_features(const world_living::Decision &,std::array<float,features> &);
// Actual small MLP inference, fixed storage, no library/model downloads or
// allocations on ticks. The supplied example weights learn synthetic authored
// economic rules, not language, dialogue, navigation or human-like reasoning.
bool predict(const Model &,const std::array<float,features> &,world_living::Action &,float &confidence);
// Optional World::process policy callback. Builds only currently feasible
// proposals; World::propose applies the authoritative rules again afterward.
bool policy(const world_living::Decision &,world_living::Proposal &,void *context);
}
#endif

#include "engine/world_detached_physics.h"
// Original-rig labels and tiny cut surfaces. Included by motion.cpp only.
namespace {
bool root_ok(double,double,double);
namespace anatomy_math=world_combat_anatomy;
using AnatomyPoint=anatomy_math::Vec3;
struct AnatomyWound {std::array<float,3> point={},axis={};};
struct AnatomyPose {std::array<AnatomyWound,8> wounds={};std::array<anatomy_math::Capsule,16> capsules={};unsigned count=0;std::array<std::vector<AnatomyPoint>,8> hulls;};
struct AnatomySnapshot {WorldModelInstance instance={};unsigned slot=0;float weight=0;bool mirrored=false,valid=false;double clock=-1;std::uint64_t serial=0;unsigned contact_part=0;AnatomyPoint strike_delta={},strike_point={};unsigned variant_mask=1;bool bridge=false;unsigned bridge_slot=0;float bridge_weight=0,bridge_alpha=0;double attack_phase=-1;unsigned frozen_actor=0;std::uint64_t frozen_generation=0;unsigned gait_slot=4;float gait_weight=0,gait_mix=0;};
namespace detached=world_detached_physics;
struct AnatomyPiece {bool active=false,settled=false,mirrored=false;unsigned actor=0,part=0,slot=0,removed=0,variant_mask=1;float weight=0,age=0;std::array<float,3> pivot={};double floor=0;std::uint64_t birth=0;std::size_t support_offset=0,support_count=0;detached::Body body;};
constexpr std::size_t anatomy_support_limit=524288;

struct CombatAnatomy {
 std::vector<float> leg_weights;std::array<float,8> gait_floor={};bool gait_floor_ready=false;
 WorldModelAnatomyStream *stream=nullptr;std::array<WorldModelAsset*,poses> cap_assets={};std::array<WorldModelPoseBlend*,batches> cap_blends={};
 std::array<AnatomyPose,poses> poses_data={};std::array<AnatomySnapshot,actor_capacity> contact={};
 std::array<bool,actor_capacity> mirrored={};std::array<std::uint16_t,actor_capacity> masks={};std::array<AnatomyPiece,64> pieces={};
 std::array<WorldCombatCrowdInstance,npc_capacity> crowd={};unsigned crowd_count=0;WorldCombatAnatomyStats stats={};std::vector<detached::Vec3> support_scratch,support_pool;std::uint64_t piece_serial=0;
};
unsigned anatomy_swapped(unsigned p){return p==3?4:p==4?3:p==5?6:p==6?5:p==7?8:p==8?7:p;}
unsigned anatomy_direction(unsigned d,bool mirror){return mirror&&d<2?1-d:d;}
unsigned anatomy_expanded(unsigned mask){unsigned full=mask;for(unsigned part=1;part<=8;++part)if(anatomy_math::removed(std::uint16_t(mask),TrainingBodyPart(part)))full|=1u<<(part-1);return full;}
AnatomyPoint anatomy_place(AnatomyPoint q,double x,double y,double z,float yaw,bool mirror=false){if(mirror)q.x=-q.x;const double c=std::cos(yaw),s=std::sin(yaw);return {x+c*q.x+s*q.z,y+q.y,z-s*q.x+c*q.z};}
AnatomyPoint anatomy_local(float x,float y,float z){return {double(x),double(y),double(z)};}
AnatomyPoint anatomy_lerp(AnatomyPoint a,AnatomyPoint b,float w){return a+(b-a)*double(w);}
AnatomyPoint anatomy_wound_point(const CombatAnatomy &g,unsigned slot,float weight,unsigned part,bool mirror,bool axis){const auto pair=pose_pair(slot,poses);const unsigned k=(mirror?anatomy_swapped(part):part)-1;
 const auto &a=g.poses_data[pair[0]].wounds[k],&b=g.poses_data[pair[1]].wounds[k];const auto &x=axis?a.axis:a.point,&y=axis?b.axis:b.point;
 auto q=anatomy_lerp(anatomy_local(x[0],x[1],x[2]),anatomy_local(y[0],y[1],y[2]),weight);if(mirror)q.x=-q.x;return q;}
void free_anatomy(CombatAnatomy *g){if(!g)return;for(auto *p:g->cap_blends)if(p)world_model_pose_blend_free(p);for(auto *p:g->cap_assets)if(p)world_model_free(p);world_model_anatomy_stream_free(g->stream);delete g;}
}

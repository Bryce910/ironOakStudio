// Exact posed-PNT packet runtime. Included only by world_combat_motion.cpp.
namespace {
struct CompactStream {
 WorldModelPoseBlend *body=nullptr,*cap=nullptr;WorldModelVertexStream *stream=nullptr,*next_stream=nullptr,*cap_stream=nullptr;bool pair=false,cap_uploaded=false;
 std::vector<WorldModelVertexOverride> staging,next_staging,cap_staging;unsigned slot=~0u;float weight=-1;bool uploaded=false;unsigned gait_slot=4;float gait_weight=0,gait_mix=0;
};
struct CompactMotion {
 world_combat_pose_packet::Packet packet;
 std::array<std::vector<WorldModelRigidWeaponPose>,batches> rigid;
 std::vector<WorldModelRigidWeaponPose> unarmed_rigid,guard_walk_rigid;
 std::array<CompactStream,actor_capacity> actors;
 std::array<CompactStream,64> pieces;unsigned piece_capacity=0;
 WorldModelPoseBlend *arms=nullptr;
 WorldArenaBowPose bow={};
 std::vector<world_combat_weapon_surface::Role> surface_roles;
 std::vector<world_combat_weapon_surface::Tree> surface_trees;
 std::array<std::vector<WorldModelRigidWeaponPose>,172> surface_rigid;
 WorldCombatCompactStats stats;
};
void compact_free(CompactMotion *c){if(!c)return;world_model_pose_blend_free(c->arms);for(auto *list:{&c->actors})for(auto &v:*list){world_model_pose_blend_free(v.body);world_model_pose_blend_free(v.cap);world_model_vertex_stream_free(v.stream);world_model_vertex_stream_free(v.next_stream);world_model_vertex_stream_free(v.cap_stream);}for(auto &v:c->pieces){world_model_pose_blend_free(v.body);world_model_pose_blend_free(v.cap);world_model_vertex_stream_free(v.stream);world_model_vertex_stream_free(v.next_stream);world_model_vertex_stream_free(v.cap_stream);}delete c;}
}

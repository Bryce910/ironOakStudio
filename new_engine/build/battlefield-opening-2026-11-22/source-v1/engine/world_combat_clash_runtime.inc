// Bounded impact pose bridges reuse the existing death-entry CPU/GPU streams
// while the actor is alive; no full pose bank or extra body buffer.
namespace {
AnatomyPoint clash_local_point(const WorldCombatMotion *p,unsigned slot,float weight,const float value[3]){
 const auto pair=pose_pair(slot,p->pose_count);const auto &r=p->body.rigid[slot];world_rigid_weapon_pose::Point point={value[0],value[1],value[2]};
 if(r.enabled)point=world_rigid_weapon_pose::interpolate(point,r.pivot_a,r.pivot_b,r.rotation,weight);
 return {point[0],point[1],point[2]};
}
anatomy_math::Edge clash_local_edge(const WorldCombatMotion *p,unsigned slot,float weight){
 const auto pair=pose_pair(slot,p->pose_count);anatomy_math::Edge edge;
 for(unsigned point=0;point<2;++point){auto value=clash_local_point(p,slot,weight,p->sockets[pair[0]][point]);if(!p->body.rigid[slot].enabled)value=anatomy_lerp({p->sockets[pair[0]][point][0],p->sockets[pair[0]][point][1],p->sockets[pair[0]][point][2]},{p->sockets[pair[1]][point][0],p->sockets[pair[1]][point][1],p->sockets[pair[1]][point][2]},weight);if(point)edge.tip=value;else edge.base=value;}
 return edge;
}
void clash_contact_source(const WorldCombatMotion *p,const Actor &a,unsigned &slot,float &weight){
 if(a.steel_pose_slot<p->batch_count&&std::isfinite(a.steel_pose_weight)&&a.steel_pose_weight>=0&&a.steel_pose_weight<=1){slot=a.steel_pose_slot;weight=a.steel_pose_weight;return;}
 float phase=a.steel_phase;if(p->presentation.upper_body_aimed)phase=world_combat_presentation::source_phase(phase,p->presentation.contact_frames[a.steel_direction]);const float key=std::clamp(phase,0.f,1.f)*11;const unsigned lower=std::min(11u,unsigned(key));slot=12+a.steel_direction*12+lower;weight=key-lower;
}
void clash_prepare_actor(const WorldCombatMotion *p,Actor &a){
 unsigned slot=0;float source=0;clash_contact_source(p,a,slot,source);const auto contact=clash_local_edge(p,slot,source);
 const float floor=std::max(0.f,source-.065f*11);float accepted=source;
 for(unsigned sample=1;sample<=32;++sample){const float candidate=source+(floor-source)*sample/32;const auto value=clash_local_edge(p,slot,candidate);const double distance=std::max(anatomy_math::length(value.base-contact.base),anatomy_math::length(value.tip-contact.tip));if(distance>.035){float good=accepted,bad=candidate;for(unsigned n=0;n<20;++n){const float middle=(good+bad)*.5f;const auto v=clash_local_edge(p,slot,middle);if(std::max(anatomy_math::length(v.base-contact.base),anatomy_math::length(v.tip-contact.tip))<=.035)good=middle;else bad=middle;}accepted=good;break;}accepted=candidate;}
 a.steel_retreat=accepted;
}
bool clash_snapshot(const WorldCombatMotion *p,const Actor &source,double clock,AnatomySnapshot &snapshot){
 if(!source.steel_rebound)return false;Actor a=source;if(a.steel_retreat<=0)clash_prepare_actor(p,a);
 const double elapsed=std::max(0.,clock-a.steel_started);clash_contact_source(p,a,snapshot.slot,snapshot.weight);
 if(elapsed>.018)snapshot.weight+=(a.steel_retreat-snapshot.weight)*smooth(float((elapsed-.018)/.067));
 if(elapsed>=.085){snapshot.bridge=true;snapshot.bridge_slot=snapshot.slot;snapshot.bridge_weight=snapshot.weight;snapshot.bridge_alpha=smooth(float((elapsed-.085)/(opponent_steel_seconds-.085)));snapshot.slot=0;snapshot.weight=0;}
 return true;
}
bool opponent_steel_sample(const WorldCombatMotion *p,const Actor &a,double clock,unsigned &slot,float &weight,unsigned &clip,float &phase,bool &direct,AnatomySnapshot *details=nullptr){
 AnatomySnapshot snapshot;if(!clash_snapshot(p,a,clock,snapshot))return false;slot=snapshot.slot;weight=snapshot.weight;clip=0;phase=0;direct=true;if(details){details->slot=snapshot.slot;details->weight=snapshot.weight;details->bridge=snapshot.bridge;details->bridge_slot=snapshot.bridge_slot;details->bridge_weight=snapshot.bridge_weight;details->bridge_alpha=snapshot.bridge_alpha;}return true;
}
const DeathActorBridge *clash_frozen(const WorldCombatMotion*p,const AnatomySnapshot&s){
 if(!s.frozen_actor||!p->deaths||s.frozen_actor>=actor_capacity)return nullptr;const auto&v=p->deaths->actors[s.frozen_actor];return v.frozen_generation==s.frozen_generation?&v:nullptr;
}
std::array<float,3> clash_pivot(const WorldCombatMotion*p,unsigned slot,float weight){const auto&r=p->body.rigid[slot];std::array<float,3>v;for(unsigned k=0;k<3;++k)v[k]=r.pivot_a[k]+(r.pivot_b[k]-r.pivot_a[k])*weight;return v;}
bool clash_edge_local(const WorldCombatMotion *p,const AnatomySnapshot &snapshot,anatomy_math::Edge &out){
 out=clash_local_edge(p,snapshot.slot,snapshot.weight);if(!snapshot.bridge)return true;if(!p->compact)return false;
 const auto*frozen=clash_frozen(p,snapshot);if(snapshot.frozen_actor&&!frozen)return false;
 const auto pair=pose_pair(snapshot.bridge_slot,p->pose_count),target_pair=pose_pair(snapshot.slot,p->pose_count);const auto &rigid=p->body.rigid[snapshot.bridge_slot],&target_rigid=p->body.rigid[snapshot.slot];std::array<world_rigid_weapon_pose::Point,3>from,to;bool found=false;
 for(const auto &mesh:p->compact->packet.meshes())if(mesh.flags&1){const auto triangle=p->compact->packet.frame(mesh,pair[0]);for(unsigned k=0;k<3;++k){const unsigned id=mesh.offset+triangle[k];if(frozen)std::copy_n(frozen->frozen[id].position,3,from[k].begin());else from[k]=world_rigid_weapon_pose::interpolate(p->compact->packet.position(pair[0],id),rigid.pivot_a,rigid.pivot_b,rigid.rotation,snapshot.bridge_weight);
   to[k]=world_rigid_weapon_pose::interpolate(p->compact->packet.position(target_pair[0],id),target_rigid.pivot_a,target_rigid.pivot_b,target_rigid.rotation,snapshot.weight);}found=true;break;}
 float rotation[4];if(!found||!world_rigid_weapon_pose::rotation(from,to,rotation))return false;const auto pivot=frozen?frozen->frozen_pivot:clash_pivot(p,snapshot.bridge_slot,snapshot.bridge_weight),target_pivot=clash_pivot(p,snapshot.slot,snapshot.weight);
 const auto edge=frozen?frozen->frozen_edge:clash_local_edge(p,snapshot.bridge_slot,snapshot.bridge_weight);for(unsigned point=0;point<2;++point){const auto v=point?edge.tip:edge.base;const auto value=world_rigid_weapon_pose::interpolate({float(v.x),float(v.y),float(v.z)},pivot.data(),target_pivot.data(),rotation,snapshot.bridge_alpha);if(point)out.tip={value[0],value[1],value[2]};else out.base={value[0],value[1],value[2]};}return true;
}
bool clash_metadata(const WorldCombatMotion*p,const AnatomySnapshot&s,AnatomyPose&out){
 const auto target_pair=pose_pair(s.slot,p->pose_count);const auto&a=p->anatomy->poses_data[target_pair[0]],&b=p->anatomy->poses_data[target_pair[1]];if(a.count!=b.count)return false;out.count=a.count;
 for(unsigned i=0;i<a.count;++i){auto&v=out.capsules[i];if(a.capsules[i].part!=b.capsules[i].part)return false;v=a.capsules[i];v.a=anatomy_lerp(a.capsules[i].a,b.capsules[i].a,s.weight);v.b=anatomy_lerp(a.capsules[i].b,b.capsules[i].b,s.weight);v.radius=a.capsules[i].radius;}
 for(unsigned i=0;i<8;++i)for(unsigned k=0;k<3;++k){out.wounds[i].point[k]=a.wounds[i].point[k]+(b.wounds[i].point[k]-a.wounds[i].point[k])*s.weight;out.wounds[i].axis[k]=a.wounds[i].axis[k]+(b.wounds[i].axis[k]-a.wounds[i].axis[k])*s.weight;}
 if(s.gait_mix>0){const auto pair=pose_pair(s.gait_slot,p->pose_count);const float floor=p->anatomy->gait_floor[pair[0]-4]+(p->anatomy->gait_floor[pair[1]-4]-p->anatomy->gait_floor[pair[0]-4])*s.gait_weight;const AnatomyPoint offset={0,floor,0};const auto&x=p->anatomy->poses_data[pair[0]],&y=p->anatomy->poses_data[pair[1]];if(x.count!=out.count||y.count!=out.count)return false;
  for(unsigned i=0;i<out.count;++i)if(unsigned(out.capsules[i].part)>=7){auto&v=out.capsules[i];v.a=anatomy_lerp(v.a,anatomy_lerp(x.capsules[i].a,y.capsules[i].a,s.gait_weight)-offset,s.gait_mix);v.b=anatomy_lerp(v.b,anatomy_lerp(x.capsules[i].b,y.capsules[i].b,s.gait_weight)-offset,s.gait_mix);}
  for(unsigned i=6;i<8;++i)for(unsigned k=0;k<3;++k){out.wounds[i].point[k]+=(x.wounds[i].point[k]+(y.wounds[i].point[k]-x.wounds[i].point[k])*s.gait_weight-(k==1?floor:0.f)-out.wounds[i].point[k])*s.gait_mix;out.wounds[i].axis[k]+=(x.wounds[i].axis[k]+(y.wounds[i].axis[k]-x.wounds[i].axis[k])*s.gait_weight-out.wounds[i].axis[k])*s.gait_mix;}}
 if(!s.bridge)return true;AnatomyPose source;const auto*frozen=clash_frozen(p,s);if(s.frozen_actor&&!frozen)return false;
 if(frozen){source.count=frozen->frozen_pose.count;for(unsigned i=0;i<source.count;++i)source.capsules[i]=frozen->frozen_pose.capsules[i];source.wounds=frozen->frozen_pose.wounds;}
 else{auto from=s;from.bridge=false;from.slot=s.bridge_slot;from.weight=s.bridge_weight;if(!clash_metadata(p,from,source))return false;}
 if(source.count!=out.count)return false;for(unsigned i=0;i<out.count;++i){if(source.capsules[i].part!=out.capsules[i].part)return false;auto&v=out.capsules[i];v.a=anatomy_lerp(source.capsules[i].a,v.a,s.bridge_alpha);v.b=anatomy_lerp(source.capsules[i].b,v.b,s.bridge_alpha);v.radius=source.capsules[i].radius+(v.radius-source.capsules[i].radius)*s.bridge_alpha;}
 for(unsigned i=0;i<8;++i)for(unsigned k=0;k<3;++k){out.wounds[i].point[k]=source.wounds[i].point[k]+(out.wounds[i].point[k]-source.wounds[i].point[k])*s.bridge_alpha;out.wounds[i].axis[k]=source.wounds[i].axis[k]+(out.wounds[i].axis[k]-source.wounds[i].axis[k])*s.bridge_alpha;}return true;
}
AnatomyPoint clash_wound(const WorldCombatMotion *p,const AnatomySnapshot &s,unsigned part,bool axis){
 if(!s.bridge&&s.gait_mix<=0)return anatomy_wound_point(*p->anatomy,s.slot,s.weight,part,s.mirrored,axis);AnatomyPose pose;if(!clash_metadata(p,s,pose))return {};const unsigned k=(s.mirrored?anatomy_swapped(part):part)-1;const auto&v=axis?pose.wounds[k].axis:pose.wounds[k].point;AnatomyPoint q={v[0],v[1],v[2]};if(s.mirrored)q.x=-q.x;return q;
}
// A nonlethal impact freezes the displayed source once, including an
// interrupted clash/reaction. Only existing actor entry streams are reused.
bool npc_reaction_snapshot_source(const WorldCombatMotion*p,unsigned actor,const Actor&a,double clock,AnatomySnapshot&s){
 if(!p->compact||!p->deaths||!actor||actor>=actor_capacity)return false;if(!a.reaction_bridge)return false;const double elapsed=std::max(0.,clock-a.reaction_started);if(elapsed>=npc_reaction_seconds)return false;
 const auto&v=p->deaths->actors[actor];if(!v.frozen_generation)return false;const unsigned d=anatomy_direction(a.reaction_direction,p->anatomy->mirrored[actor]);// First reach one fixed authored entry. A moving target during the frozen
 // bridge can cross the quaternion half-turn and flip the rigid weapon.
 constexpr double entry=npc_reaction_entry_seconds;const auto sample=world_combat_continuity::returning(std::max(0.,elapsed-entry),npc_reaction_seconds-entry,false);
 if(sample.settle){s.slot=p->pose_count+settle_offset+7+d;s.weight=sample.weight;}else{const auto&clip=clips[14+d];const float key=sample.phase*(clip.count-1);s.slot=clip.base+std::min(clip.count-1,unsigned(key));s.weight=key-unsigned(key);}
 s.gait_mix=0;s.bridge=true;s.bridge_slot=0;s.bridge_weight=0;s.bridge_alpha=smooth(float(elapsed/entry));s.frozen_actor=actor;s.frozen_generation=v.frozen_generation;return true;
}
bool npc_reaction_snapshot(const WorldCombatMotion*p,unsigned actor,double clock,AnatomySnapshot&s){return actor<actor_capacity&&npc_reaction_snapshot_source(p,actor,p->actors[actor],clock,s);}
bool npc_reaction_capture(WorldCombatMotion*p,unsigned actor,const Actor&prior,unsigned prior_slot,float prior_weight,double prior_clock){
 if(!p->compact||!p->deaths||!p->anatomy||!actor||actor>=actor_capacity)return false;auto&v=p->deaths->actors[actor];if(v.frozen.size()!=p->compact->packet.vertices()||v.staging.size()!=v.frozen.size())return false;
 AnatomySnapshot source;source.slot=prior_slot;source.weight=prior_weight;guarded_gait_snapshot(p,prior,source);
 if(prior.reaction_bridge)npc_reaction_snapshot_source(p,actor,prior,prior_clock,source);else if(prior.steel_rebound)clash_snapshot(p,prior,prior_clock,source);
 AnatomyPose metadata;if(!clash_metadata(p,source,metadata))return false;anatomy_math::Edge edge;if(!clash_edge_local(p,source,edge))return false;auto pivot=clash_pivot(p,source.slot,source.weight);
 if(source.bridge){const auto*old=clash_frozen(p,source);if(source.frozen_actor&&!old)return false;const auto from=old?old->frozen_pivot:clash_pivot(p,source.bridge_slot,source.bridge_weight);for(unsigned k=0;k<3;++k)pivot[k]=from[k]+(pivot[k]-from[k])*source.bridge_alpha;
  if(!source.frozen_actor&&(!compact_sample(p,source.bridge_slot,source.bridge_weight,v.frozen.data(),v.frozen.size())||(!v.cap_frozen.empty()&&!world_model_pose_blend_sample_pnt(p->anatomy->cap_blends[source.bridge_slot],source.bridge_weight,v.cap_frozen.data(),v.cap_frozen.size()))))return false;
  if(!compact_bridge(p,source.slot,source.weight,v.frozen.data(),source.bridge_alpha,v.staging.data(),v.staging.size())||(!v.cap_frozen.empty()&&!world_model_pose_blend_bridge_pnt(p->anatomy->cap_blends[source.slot],source.weight,v.cap_frozen.data(),source.bridge_alpha,v.cap_staging.data(),v.cap_staging.size())))return false;
  std::copy(v.staging.begin(),v.staging.end(),v.frozen.begin());std::copy(v.cap_staging.begin(),v.cap_staging.end(),v.cap_frozen.begin());
 }else if(!compact_sample(p,source.slot,source.weight,v.frozen.data(),v.frozen.size())||!guarded_gait_sample(p,source,v.frozen.data(),v.frozen.size())||(!v.cap_frozen.empty()&&!world_model_pose_blend_sample_pnt(p->anatomy->cap_blends[source.slot],source.weight,v.cap_frozen.data(),v.cap_frozen.size())))return false;
 v.frozen_pose.count=metadata.count;for(unsigned i=0;i<metadata.count;++i)v.frozen_pose.capsules[i]=metadata.capsules[i];v.frozen_pose.wounds=metadata.wounds;v.frozen_edge=edge;v.frozen_pivot=pivot;if(!++v.frozen_generation)++v.frozen_generation;
 // Retain a coherent last-visible endpoint even if another impact is fatal
 // before a draw uploads this publication frame.
 std::copy(v.frozen.begin(),v.frozen.end(),v.staging.begin());std::copy(v.cap_frozen.begin(),v.cap_frozen.end(),v.cap_staging.begin());
 v.clash=true;v.uploaded=false;v.pose={};v.source_snapshot={};v.source_snapshot.bridge=true;v.source_snapshot.frozen_actor=actor;v.source_snapshot.frozen_generation=v.frozen_generation;return true;
}
bool clash_commit(WorldCombatMotion *p,unsigned actor,const AnatomySnapshot &snapshot){
 auto failed=[&](){if(p&&p->compact)++p->compact->stats.failures;return false;};
 if(!p||!p->deaths||!actor||actor>=actor_capacity||!snapshot.bridge)return failed();auto &v=p->deaths->actors[actor];
 if(!clash_frozen(p,snapshot)&&snapshot.frozen_actor)return failed();
 if(!snapshot.frozen_actor&&(!v.clash||v.source_snapshot.frozen_actor||v.source_snapshot.bridge_slot!=snapshot.bridge_slot||v.source_snapshot.bridge_weight!=snapshot.bridge_weight)){if(!compact_sample(p,snapshot.bridge_slot,snapshot.bridge_weight,v.frozen.data(),v.frozen.size())||!world_model_pose_blend_sample_pnt(p->anatomy->cap_blends[snapshot.bridge_slot],snapshot.bridge_weight,v.cap_frozen.data(),v.cap_frozen.size()))return failed();v.clash=true;v.uploaded=false;v.pose={};}
 if(v.uploaded&&v.source_snapshot.bridge_alpha==snapshot.bridge_alpha&&v.source_snapshot.slot==snapshot.slot&&v.source_snapshot.weight==snapshot.weight&&v.source_snapshot.frozen_generation==snapshot.frozen_generation)return true;v.source_snapshot=snapshot;
 if(!compact_bridge(p,snapshot.slot,snapshot.weight,v.frozen.data(),snapshot.bridge_alpha,v.staging.data(),v.staging.size())||!world_model_pose_blend_bridge_pnt(p->anatomy->cap_blends[snapshot.slot],snapshot.weight,v.cap_frozen.data(),snapshot.bridge_alpha,v.cap_staging.data(),v.cap_staging.size())||!world_model_vertex_stream_upload(v.stream,v.staging.data(),v.staging.size())||!world_model_vertex_stream_upload(v.cap_stream,v.cap_staging.data(),v.cap_staging.size()))return failed();v.uploaded=true;return true;
}
bool clash_draw_actor(WorldCombatMotion *p,unsigned program,unsigned actor,const AnatomySnapshot &snapshot,unsigned mask,TrainingWeaponHand hand){
 if(!snapshot.bridge||!clash_commit(p,actor,snapshot))return false;auto &v=p->deaths->actors[actor];auto draw=WorldModelAnatomyDraw{};draw.removed=anatomy_expanded(mask);draw.mirrored=snapshot.mirrored;draw.hide_weapon=hand==TrainingWeaponHand::None;draw.variant_mask=0;draw.x=snapshot.instance.x;draw.y=snapshot.instance.y;draw.z=snapshot.instance.z;draw.yaw=snapshot.instance.yaw;draw.scale=snapshot.instance.scale;draw.rotation[3]=1;draw.cap_side=1;world_model_pose_blend_draw_anatomy_deformed(v.body,program,&draw);++p->anatomy->stats.body_draws;
 if(mask){draw.caps=true;draw.removed=anatomy_caps_mask(mask);world_model_pose_blend_draw_anatomy_deformed(v.cap,program,&draw);++p->anatomy->stats.cap_draws;}return true;
}
}

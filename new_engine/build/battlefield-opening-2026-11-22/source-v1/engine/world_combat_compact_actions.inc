namespace {
// Guard stays the authoritative upper-body/steel source. Only the original
// skin-weighted legs follow distance-driven walking, shared by all compact rigs.
void guarded_gait_ground(const WorldCombatMotion*p){
 if(!p->compact||!p->anatomy||p->anatomy->gait_floor_ready)return;auto&g=*p->anatomy;
 float neutral=std::numeric_limits<float>::infinity();std::array<float,8> floor;floor.fill(neutral);
 for(const auto&m:p->compact->packet.meshes())if(!(m.flags&1))for(unsigned i=0;i<m.count;++i){const unsigned id=m.offset+i;if(id>=g.leg_weights.size()||g.leg_weights[id]<.99f)continue;neutral=std::min(neutral,p->compact->packet.vertex(0,id)[1]);for(unsigned k=0;k<8;++k)floor[k]=std::min(floor[k],p->compact->packet.vertex(4+k,id)[1]);}
 if(!std::isfinite(neutral))return;for(unsigned k=0;k<8;++k)g.gait_floor[k]=std::isfinite(floor[k])?floor[k]-neutral:0;g.gait_floor_ready=true;
}
void guarded_gait_snapshot(const WorldCombatMotion*p,const Actor&a,AnatomySnapshot&s){
 if(!p->compact||!p->anatomy||s.bridge||a.guarded_gait_mix<=0)return;
 guarded_gait_ground(p);
 const float key=world_combat_locomotion::wrapped(a.guarded_gait_phase)*8;
 s.gait_slot=4+std::min(7u,unsigned(key));s.gait_weight=key-std::floor(key);s.gait_mix=a.guarded_gait_mix*(p->opponents_only?.30f:1.f);
}
bool guarded_gait_sample(const WorldCombatMotion*p,const AnatomySnapshot&s,WorldModelVertexOverride*out,std::size_t count){
 if(s.gait_mix<=0)return true;
 if(!p->compact||!p->anatomy||count!=p->anatomy->leg_weights.size()||s.gait_slot<4||s.gait_slot>11)return false;
 const auto pair=pose_pair(s.gait_slot,p->pose_count);
 const float floor=p->anatomy->gait_floor[pair[0]-4]+(p->anatomy->gait_floor[pair[1]-4]-p->anatomy->gait_floor[pair[0]-4])*s.gait_weight;
 for(const auto&mesh:p->compact->packet.meshes()){
  if(mesh.flags&1)continue; // Weapon position, normals and rigid frame stay exact.
  for(unsigned i=0;i<mesh.count;++i){const unsigned id=mesh.offset+i;const float mix=p->anatomy->leg_weights[id]*s.gait_mix;if(mix<=0)continue;
   const auto*a=p->compact->packet.vertex(pair[0],id),*b=p->compact->packet.vertex(pair[1],id);auto&v=out[id];
   float n[3],t[3];for(unsigned k=0;k<3;++k){v.position[k]+=(a[k]+(b[k]-a[k])*s.gait_weight-(k==1?floor:0.f)-v.position[k])*mix;n[k]=v.normal[k]+(a[k+3]+(b[k+3]-a[k+3])*s.gait_weight-v.normal[k])*mix;t[k]=v.tangent[k]+(a[k+6]+(b[k+6]-a[k+6])*s.gait_weight-v.tangent[k])*mix;}
   float nn=0;for(float f:n)nn+=f*f;if(nn<1.e-12f)return false;nn=std::sqrt(nn);for(float&f:n)f/=nn;float dot=0;for(unsigned k=0;k<3;++k)dot+=n[k]*t[k];float tt=0;for(unsigned k=0;k<3;++k){t[k]-=n[k]*dot;tt+=t[k]*t[k];}if(tt<1.e-12f)return false;tt=std::sqrt(tt);for(unsigned k=0;k<3;++k){v.normal[k]=n[k];v.tangent[k]=t[k]/tt;}
  }
 }return true;
}
bool compact_sample(WorldCombatMotion *p,unsigned slot,float weight,WorldModelVertexOverride *out,std::size_t count){
 if(!p||slot>=p->batch_count)return false;if(!p->compact)return world_model_pose_blend_sample_pnt(p->body.blend[slot],weight,out,count);
 const auto pair=pose_pair(slot,p->pose_count);return p->compact->packet.sample(pair[0],pair[1],weight,p->compact->rigid[slot],out,count);
}
bool compact_bridge(WorldCombatMotion *p,unsigned slot,float weight,const WorldModelVertexOverride*frozen,float alpha,WorldModelVertexOverride*out,std::size_t count){
 if(!p||slot>=p->batch_count)return false;if(!p->compact)return world_model_pose_blend_bridge_pnt(p->body.blend[slot],weight,frozen,alpha,out,count);
 const auto pair=pose_pair(slot,p->pose_count);return p->compact->packet.bridge(pair[0],pair[1],weight,p->compact->rigid[slot],frozen,alpha,out,count);
}
bool compact_stream_prepare(WorldCombatMotion *p,CompactStream &v,bool pair=false){
 if(!p||!p->compact||!p->anatomy)return false;
 v.pair=pair;v.body=world_model_pose_blend_create(1,WORLD_MODEL_POSE_BLEND_VERSION);v.stream=world_model_vertex_stream_create(p->body.assets[0]);
 if(pair)v.next_stream=world_model_vertex_stream_create(p->body.assets[0]);
 try{v.staging.resize(p->compact->packet.vertices());if(pair)v.next_staging.resize(v.staging.size());}catch(...){return false;}
 const WorldModelInstance seed={0,0,0,0,1};const float zero=0;
 const bool ok=v.body&&v.stream&&world_model_pose_blend_set_targets(v.body,p->body.assets[0],p->body.assets[0])&&world_model_pose_blend_attach_anatomy(v.body,p->anatomy->stream)&&(pair?world_model_pose_blend_attach_pose_streams(v.body,v.stream,v.next_stream):world_model_pose_blend_attach_vertex_stream(v.body,v.stream))&&world_model_pose_blend_upload(v.body,&seed,&zero,1);
 if(ok){p->compact->stats.gpu_bytes+=(v.staging.size()+v.next_staging.size())*sizeof(WorldModelVertexOverride);p->compact->stats.staging_bytes+=(v.staging.capacity()+v.next_staging.capacity())*sizeof(WorldModelVertexOverride);}return ok;
}
bool compact_prepare_streams(WorldCombatMotion *p){
 if(!p||!p->compact||!p->anatomy)return false;auto &c=*p->compact;c.guard_walk_rigid.resize(c.packet.meshes().size());
 if(!world_model_pose_blend_attach_anatomy(p->body.blend[0],p->anatomy->stream))return false;
 unsigned npcs=0;for(unsigned who=0;who<actor_capacity;++who)if(p->actor_mask&(1u<<who)){if(who)++npcs;if(!compact_stream_prepare(p,c.actors[who],true))return false;}
 c.piece_capacity=std::min(64u,npcs*8u);for(unsigned i=0;i<c.piece_capacity;++i){auto &piece=c.pieces[i];if(!compact_stream_prepare(p,piece))return false;
  piece.cap=world_model_pose_blend_create(1,WORLD_MODEL_POSE_BLEND_VERSION);piece.cap_stream=world_model_vertex_stream_create(p->anatomy->cap_assets[0]);try{piece.cap_staging.resize(world_model_pose_vertices(p->anatomy->cap_assets[0]));}catch(...){return false;}
  const WorldModelInstance seed={0,0,0,0,1};const float zero=0;if(!piece.cap||!piece.cap_stream||!world_model_pose_blend_set_targets(piece.cap,p->anatomy->cap_assets[0],p->anatomy->cap_assets[0])||!world_model_pose_blend_attach_vertex_stream(piece.cap,piece.cap_stream)||!world_model_pose_blend_upload(piece.cap,&seed,&zero,1))return false;c.stats.gpu_bytes+=piece.cap_staging.size()*sizeof(WorldModelVertexOverride);c.stats.staging_bytes+=piece.cap_staging.capacity()*sizeof(WorldModelVertexOverride);
 }
 return true;
}
bool compact_commit(WorldCombatMotion *p,CompactStream &v,unsigned slot,float weight,const AnatomySnapshot*snapshot=nullptr){
 if(!p||!p->compact||!v.body||!v.stream)return false;
 const AnatomySnapshot neutral;const auto&gait=snapshot?*snapshot:neutral;
 const bool gait_changed=v.gait_mix!=gait.gait_mix||(gait.gait_mix>0&&(v.gait_slot!=gait.gait_slot||v.gait_weight!=gait.gait_weight));
 if(v.uploaded&&v.slot==slot&&v.weight==weight&&!gait_changed)return true;
 if(v.pair&&gait.gait_mix>0){
  // During steady guarded walking, only the eight gait key pairs upload.
  // The shader interpolates legs; both upper endpoints hold the exact guard.
  if(v.uploaded&&v.gait_mix==gait.gait_mix&&v.slot==slot&&v.weight==weight&&v.gait_slot==gait.gait_slot){v.gait_weight=gait.gait_weight;return true;}
  auto first=gait,last=gait;first.gait_weight=0;last.gait_weight=1;
  if(!compact_sample(p,slot,weight,v.staging.data(),v.staging.size()))return false;
  std::copy(v.staging.begin(),v.staging.end(),v.next_staging.begin());
  if(!guarded_gait_sample(p,first,v.staging.data(),v.staging.size())||!guarded_gait_sample(p,last,v.next_staging.data(),v.next_staging.size())||!world_model_pose_blend_upload_pose_pair(v.body,v.stream,v.next_stream,v.staging.data(),v.next_staging.data(),v.staging.size(),p->compact->guard_walk_rigid.data(),p->compact->guard_walk_rigid.size())){++p->compact->stats.failures;return false;}
  v.slot=slot;v.weight=weight;v.gait_slot=gait.gait_slot;v.gait_weight=gait.gait_weight;v.gait_mix=gait.gait_mix;v.uploaded=true;++p->compact->stats.uploads;++p->stats.uploads;return true;
 }
 bool ok=false;if(v.pair){if(v.uploaded&&v.slot==slot&&!gait_changed){v.weight=weight;return true;}const auto pair=pose_pair(slot,p->pose_count);ok=p->compact->packet.raw_pose(pair[0],v.staging.data(),v.staging.size())&&p->compact->packet.raw_pose(pair[1],v.next_staging.data(),v.next_staging.size())&&guarded_gait_sample(p,gait,v.staging.data(),v.staging.size())&&guarded_gait_sample(p,gait,v.next_staging.data(),v.next_staging.size())&&world_model_pose_blend_upload_pose_pair(v.body,v.stream,v.next_stream,v.staging.data(),v.next_staging.data(),v.staging.size(),p->compact->rigid[slot].data(),p->compact->rigid[slot].size());}
 else ok=compact_sample(p,slot,weight,v.staging.data(),v.staging.size())&&guarded_gait_sample(p,gait,v.staging.data(),v.staging.size())&&world_model_vertex_stream_upload(v.stream,v.staging.data(),v.staging.size());
 if(!ok){++p->compact->stats.failures;return false;}
 v.slot=slot;v.weight=weight;v.gait_slot=gait.gait_slot;v.gait_weight=gait.gait_weight;v.gait_mix=gait.gait_mix;v.uploaded=true;++p->compact->stats.uploads;++p->stats.uploads;return true;
}
void compact_draw(WorldCombatMotion *,unsigned,double,double,double,bool,bool);
bool compact_crowd(WorldCombatMotion *,const WorldCombatCrowdInstance *,unsigned);
void compact_draw_crowd(WorldCombatMotion *,unsigned,double,double,double,bool);
}

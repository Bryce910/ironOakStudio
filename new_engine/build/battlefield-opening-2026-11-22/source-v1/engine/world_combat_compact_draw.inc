namespace {
bool compact_bow_commit(WorldCombatMotion *p,CompactStream &v,bool first_person){
 if(!p||!p->compact||p->compact->packet.pose_count()!=210)return false;
 const float draw=std::clamp(p->compact->bow.draw,0.f,1.f);const unsigned slot=draw<.5f?0:1;const float weight=draw<.5f?draw*2:(draw-.5f)*2;
 const unsigned key=batches+96+unsigned(first_person)*2+slot;
 if(v.uploaded&&v.slot==key){v.weight=weight;return true;}
 const unsigned a=204+unsigned(first_person)*3+slot;
 if(!p->compact->packet.raw_pose(a,v.staging.data(),v.staging.size())||!p->compact->packet.raw_pose(a+1,v.next_staging.data(),v.next_staging.size())||!world_model_pose_blend_upload_pose_pair(v.body,v.stream,v.next_stream,v.staging.data(),v.next_staging.data(),v.staging.size(),p->compact->unarmed_rigid.data(),p->compact->unarmed_rigid.size())){++p->compact->stats.failures;return false;}
 v.uploaded=true;v.slot=key;v.weight=weight;++p->compact->stats.uploads;++p->stats.uploads;return true;
}
bool compact_player_commit(WorldCombatMotion *p,CompactStream &v){
 if(!p->unarmed||!p->unarmed->active)return compact_commit(p,v,p->actors[0].slot,p->weights[0]);
 const auto &u=*p->unarmed;const unsigned key=batches+u.slot;if(v.uploaded&&v.slot==key&&v.weight==u.weight)return true;
 const unsigned a=u.slot<32?u.slot:(u.slot-32)/2,mode=u.slot<32?a/16:(u.slot-32)%2,b=u.slot<32?mode*16+(a+1)%16:mode*16+a%16;
 if(v.uploaded&&v.slot==key){v.weight=u.weight;return true;}
 if(!p->compact->packet.raw_pose(172+a,v.staging.data(),v.staging.size())||!p->compact->packet.raw_pose(172+b,v.next_staging.data(),v.next_staging.size())||!world_model_pose_blend_upload_pose_pair(v.body,v.stream,v.next_stream,v.staging.data(),v.next_staging.data(),v.staging.size(),p->compact->unarmed_rigid.data(),p->compact->unarmed_rigid.size())){++p->compact->stats.failures;return false;}
 v.uploaded=true;v.slot=key;v.weight=u.weight;++p->compact->stats.uploads;++p->stats.uploads;return true;
}
void compact_uniforms(WorldCombatMotion*p,unsigned program,double x,double y,double z,double origin_x,double origin_z,bool shadow){
 auto &u=p->locations(program);glUseProgram(program);glUniform3f(u.offset,float(origin_x-x),float(-y),float(origin_z-z));glUniform2f(u.anchor,float(origin_x),float(origin_z));glUniform1i(u.grass,0);glUniform1i(u.vegetation,0);glUniform1i(u.lod,-1);glUniform1f(u.wind,0);glUniform1f(u.height,2);glUniform3f(u.prop,0,0,0);glUniform1f(u.reactive,shadow?0:.85f);
}
void compact_draw(WorldCombatMotion *p,unsigned program,double x,double y,double z,bool shadow,bool show_player){
 if(!p||!p->compact||!p->anatomy)return;compact_uniforms(p,program,x,y,z,p->x,p->z,shadow);
 if(show_player&&(p->actor_mask&1)&&p->actors[0].active&&!p->compact->bow.visible){auto &v=p->compact->actors[0];if(compact_player_commit(p,v)){
  // Equipment belongs to the player, while all NPCs keep their actual weapon.
  AnatomySnapshot snapshot={p->instances[0],p->actors[0].slot,p->weights[0],false,true};snapshot.variant_mask=0;auto body=anatomy_draw(snapshot,0);if(p->unarmed&&p->unarmed->active)body.weight=p->unarmed->weight;body.hide_weapon=(p->unarmed&&p->unarmed->active)||!p->grip_weapon_allowed||!(p->equipment&4);
  world_model_pose_blend_draw_anatomy(v.body,program,&body);
 }}
 for(unsigned who=1;who<actor_capacity;++who)if((p->actor_mask&(1u<<who))&&p->actors[who].active){
  bool crowd_owns=false;for(unsigned i=0;i<p->anatomy->crowd_count;++i)crowd_owns|=p->anatomy->crowd[i].actor_id==who;if(crowd_owns)continue;
  AnatomySnapshot snapshot={p->instances[who],p->actors[who].slot,p->weights[who],p->anatomy->mirrored[who],true};snapshot.variant_mask=0;guarded_gait_snapshot(p,p->actors[who],snapshot);
  if(!npc_reaction_snapshot(p,who,p->actors[who].last_clock,snapshot)&&p->actors[who].steel_rebound)clash_snapshot(p,p->actors[who],p->actors[who].last_clock,snapshot);
  actor_blood_wear(p,program,who);
  const auto mask=p->anatomy->masks[who];const auto hand=anatomy_math::surviving_hand(mask);
  if(!death_draw_actor(p,program,who,snapshot,mask,hand))anatomy_draw_actor(p,program,snapshot,mask,hand,who);
 }
 anatomy_draw_pieces(p,program);glUniform1f(p->locations(program).reactive,0);
}
bool compact_crowd(WorldCombatMotion*p,const WorldCombatCrowdInstance*crowd,unsigned count){
 if(!p||!p->compact||!p->anatomy||count>npc_capacity||(count&&!crowd))return false;
 unsigned ids=0;for(unsigned i=0;i<count;++i){const auto &c=crowd[i];if(!c.actor_id||c.actor_id>=actor_capacity||ids&(1u<<c.actor_id)||!root_ok(c.x,c.y,c.z)||!std::isfinite(c.yaw)||!std::isfinite(c.defeat_phase))return false;ids|=1u<<c.actor_id;}
 auto &g=*p->anatomy;g.crowd_count=0;unsigned fallen=0;
 for(unsigned i=0;i<count;++i){const auto &c=crowd[i];if(!(p->actor_mask&(1u<<c.actor_id)))continue;g.crowd[g.crowd_count++]=c;if(c.yielded)++fallen;}

 if(p->deaths)for(auto &v:p->deaths->actors)v.crowd_override=false;
 p->crowd_x=g.crowd_count?std::floor(g.crowd[0].x/128)*128:0;p->crowd_z=g.crowd_count?std::floor(g.crowd[0].z/128)*128:0;
 for(unsigned i=0;i<g.crowd_count;++i){const auto &c=g.crowd[i];if(c.yielded&&p->deaths&&p->deaths->actors[c.actor_id].pose.valid){death_tick(p,c.actor_id,std::clamp(c.defeat_phase,0.f,1.f)*world_combat_motion::death_duration);if(death_active(p,c.actor_id))p->deaths->actors[c.actor_id].crowd_override=true;}}
 p->stats.crowd_instances=g.crowd_count;p->stats.fallen_instances=fallen;p->stats.peak_fallen_instances=std::max(p->stats.peak_fallen_instances,fallen);return true;
}
void compact_draw_crowd(WorldCombatMotion *p,unsigned program,double x,double y,double z,bool shadow){
 if(!p||!p->compact||!p->anatomy)return;compact_uniforms(p,program,x,y,z,p->crowd_x,p->crowd_z,shadow);
 auto &g=*p->anatomy;for(unsigned i=0;i<g.crowd_count;++i){const auto &c=g.crowd[i];const unsigned who=c.actor_id;if(c.weapon_hand!=TrainingWeaponHand::None)g.mirrored[who]=c.weapon_hand==TrainingWeaponHand::Left;
  actor_blood_wear(p,program,who);
  const float key=c.yielded?std::clamp(c.defeat_phase,0.f,1.f)*23:0;AnatomySnapshot snapshot={{float(c.x-p->crowd_x),float(c.y),float(c.z-p->crowd_z),c.yaw,1},c.yielded?148+std::min(23u,unsigned(key)):0,c.yielded?key-unsigned(key):0,g.mirrored[who],true};snapshot.variant_mask=0;
  if(c.yielded&&p->deaths&&p->deaths->actors[who].pose.valid){const auto &s=p->deaths->actors[who].pose;snapshot.instance.yaw=s.yaw;snapshot.slot=s.slot;snapshot.weight=s.weight;}
  if(!death_draw_actor(p,program,who,snapshot,c.severed_parts,c.weapon_hand,true))anatomy_draw_actor(p,program,snapshot,c.severed_parts,c.weapon_hand,who);
 }actor_blood_wear(p,program,0);glUniform1f(p->locations(program).reactive,0);
}
void compact_draw_arms(WorldCombatMotion *p,unsigned program){
 if(!p||!p->compact||!program||!(p->actor_mask&1)||p->actors[0].defeated||!p->compact->arms||p->compact->bow.visible)return;
 auto &c=*p->compact;auto &v=c.actors[0];if(!compact_player_commit(p,v))return;
 const auto &rigid=p->unarmed&&p->unarmed->active?c.unarmed_rigid:c.rigid[p->actors[0].slot];
 // Same streamed endpoints; only these tiny per-mesh rigid descriptors and
 // instance weight differ. The source weapon has no FP subset triangles.
 // Share rigid data through the loading-time base targets without geometry.
 world_model_pose_blend_share_rigid(c.arms,v.body);
 WorldModelInstance instance=p->arms_instance;const float cs=std::cos(instance.yaw),sn=std::sin(instance.yaw);instance.x+=instance.scale*(cs*.25f+sn*.65f);instance.z+=instance.scale*(-sn*.25f+cs*.65f);const float weight=p->unarmed&&p->unarmed->active?p->unarmed->weight:p->weights[0];
 if(world_model_pose_blend_upload(c.arms,&instance,&weight,1))world_model_pose_blend_draw_equipped(c.arms,program,p->equipment,1);
}

}

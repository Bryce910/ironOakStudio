#include <chrono>
namespace {
unsigned anatomy_caps_mask(unsigned mask){
 if(mask&2u)mask&=~(1u|4u|8u|16u|32u);
 if(mask&4u)mask&=~16u;if(mask&8u)mask&=~32u;return mask;
}
WorldModelAnatomyDraw anatomy_draw(const AnatomySnapshot &s,unsigned removed,unsigned keep=0,bool caps=false){WorldModelAnatomyDraw d={};
 d.removed=caps?anatomy_caps_mask(removed):anatomy_expanded(removed);d.keep_part=keep;d.mirrored=s.mirrored;d.caps=caps;
 d.x=s.instance.x;d.y=s.instance.y;d.z=s.instance.z;d.yaw=s.instance.yaw;d.scale=s.instance.scale;d.weight=s.weight;d.rotation[3]=1;d.cap_side=keep?-1.f:1.f;d.variant_mask=s.variant_mask;
 return d;
}
AnatomySnapshot anatomy_defender(const WorldCombatMotion *p,const TrainingAnatomicalQuery &q,const WorldCombatContactFrame &f){
 const unsigned who=q.opponent_id+1;Actor predicted=p->actors[who];
 if(p->compact&&q.defender_clash_serial&&q.defender_clash_serial!=predicted.steel_serial&&q.contact_clock>=q.defender_clash_time&&q.contact_clock-q.defender_clash_time<=opponent_steel_seconds){predicted.steel_rebound=true;predicted.steel_serial=q.defender_clash_serial;predicted.steel_phase=float(q.defender_clash_phase);predicted.steel_started=q.defender_clash_time;predicted.steel_pose_slot=q.defender_clash_pose_slot;predicted.steel_pose_weight=float(q.defender_clash_pose_weight);predicted.steel_retreat=0;predicted.steel_direction=anatomy_direction(unsigned(q.defender_direction),anatomy_math::surviving_hand(q.severed_parts)==TrainingWeaponHand::Left);}
 const auto &a=predicted;AnatomySnapshot s;s.variant_mask=p->compact?0:world_model_variants::actor(who);
 s.mirrored=p->anatomy->mirrored[who];const auto hand=anatomy_math::surviving_hand(q.severed_parts);if(hand!=TrainingWeaponHand::None)s.mirrored=hand==TrainingWeaponHand::Left;
 unsigned clip=0;float phase=.5f-.5f*std::cos((p->clock+float(std::clamp(q.contact_clock-a.last_clock,0.,.05)))*1.6f+who*.3f);bool direct=false;
 const bool attacking=q.defender_phase==TrainingEnemyPhase::Windup||q.defender_phase==TrainingEnemyPhase::Strike||q.defender_phase==TrainingEnemyPhase::Recovery;
 const unsigned direction=anatomy_direction(unsigned(q.defender_direction),s.mirrored);
 const float elapsed=float(std::clamp(q.contact_clock-a.last_clock,0.,.05));
 auto returning=[&](unsigned index,double time,double duration,bool attack){const auto sample=world_combat_continuity::returning(time,duration,attack);phase=sample.phase;if(sample.settle){s.slot=p->pose_count+settle_offset+index;s.weight=sample.weight;direct=true;}};
 if(a.active&&npc_reaction_snapshot(p,who,q.contact_clock,s)){}
 else if(a.active&&a.hit>elapsed){const unsigned d=anatomy_direction(a.reaction_direction,s.mirrored);clip=14+d;
  const float time=std::max(0.f,world_combat_motion::reaction_duration-a.hit+elapsed);
  const bool entry=p->opponents_only&&opponent_reaction_entry(a,time,p->pose_count,s.slot,s.weight,clip,phase,direct);
  const double lead=p->opponents_only&&a.reaction_entry?opponent_reaction_entry_seconds:0;
  if(!entry)returning(7+d,time-lead,world_combat_motion::reaction_duration-lead,false);}
 else if(a.active&&a.parry>elapsed&&!a.steel_rebound){
  const double duration=a.parry_duration;const double time=duration-(a.parry-elapsed);
  if(a.guard_recoil){clip=7+unsigned(a.guard_direction);float guard=a.guard;const unsigned desired=anatomy_direction(world_combat_guard_pose(unsigned(q.defender_guard),p->opponents_only),s.mirrored);
   const bool held=!attacking&&q.defender_phase!=TrainingEnemyPhase::Stagger;
   guard=std::clamp(guard+elapsed*((desired!=unsigned(a.guard_direction)&&guard>.01f)?-12.f:held?10.f:-12.f),0.f,1.f);
   phase=opponent_guard_recoil(smooth(guard),time,duration);
  }else{clip=13;returning(6,time,duration,false);}
 }
 else if(opponent_steel_sample(p,a,q.contact_clock,s.slot,s.weight,clip,phase,direct,&s)){
  // Contact prediction selects the same stopped source pose as NPC rendering.
 }
 else if(attacking){const auto pp=q.defender_phase==TrainingEnemyPhase::Windup?TrainingPlayerPhase::Windup:q.defender_phase==TrainingEnemyPhase::Strike?TrainingPlayerPhase::Strike:TrainingPlayerPhase::Recovery;
  const float leadin=a.attacking?a.attack_leadin:0;clip=2+direction;
  if(pp==TrainingPlayerPhase::Windup&&q.defender_time<leadin&&a.entry_origin){
   const auto sample=world_combat_transition::entry(a.entry_origin,q.defender_time,leadin,a.attack_direction);using Kind=world_combat_transition::SampleKind;
   if(sample.kind==Kind::Clip){clip=sample.index;phase=sample.phase;if(clips[clip].loop)phase-=std::floor(phase);}
   else{s.slot=sample.kind==Kind::WalkStart?p->pose_count:sample.kind==Kind::WalkReturn?p->pose_count+1+sample.index:sample.kind==Kind::Return?p->pose_count+settle_offset+sample.index:p->pose_count+3+combo_batches+sample.index;s.weight=sample.phase;direct=true;}
  }else if(pp==TrainingPlayerPhase::Windup&&q.defender_time<leadin&&a.idle_entry){
   const double settle=world_combat_attack_entry::settle_time(leadin);
   if(q.defender_time<settle){clip=0;phase=a.idle_entry_phase*(1-world_combat_attack_entry::weight(q.defender_time,settle));}
   else{s.slot=p->pose_count+3+combo_batches+a.attack_direction;s.weight=world_combat_attack_entry::weight(q.defender_time-settle,leadin-settle);direct=true;}
  }else if(pp==TrainingPlayerPhase::Windup&&q.defender_time<leadin&&a.guard_entry){
   const auto sample=world_combat_continuity::guarding(a.guard_entry_phase,q.defender_time,leadin);
   if(sample.entry){s.slot=p->pose_count+3+combo_batches+a.attack_direction;s.weight=sample.weight;direct=true;}else{clip=7+a.attack_guard_direction;phase=sample.phase;}
  }else if(pp==TrainingPlayerPhase::Windup&&q.defender_time<leadin){clip=7+a.attack_guard_direction;phase=smooth(a.attack_guard*float(1-q.defender_time/leadin));}
  else{
   phase=attack_phase(pp,q.defender_time,q.defender_duration,leadin,p->stats.polished);s.attack_phase=phase;
   if(p->presentation.upper_body_aimed)phase=world_combat_presentation::source_phase(phase,p->presentation.contact_frames[direction]);
   if(pp==TrainingPlayerPhase::Recovery){const auto sample=world_combat_continuity::returning(q.defender_time,q.defender_duration,true);phase=.60f+sample.phase*.40f;if(sample.settle){s.slot=p->pose_count+settle_offset+direction;s.weight=sample.weight;direct=true;}}
  }
 }else if((p->opponents_only||p->compact)&&a.active&&a.guard_idle_entry){
  // Predict the very same idle-to-guard pre-roll used by the rendered Orc.
  // An early guard query must not use a raised blade while its visible hands
  // are still settling out of an authored breathing pose.
  constexpr float seconds=.06f;const float time=a.guard_idle_time+elapsed;
  if(time<seconds){clip=0;phase=a.guard_idle_from+(a.guard_idle_to-a.guard_idle_from)*smooth(time/seconds);}
  else{const unsigned desired=anatomy_direction(world_combat_guard_pose(unsigned(q.defender_guard),p->opponents_only),s.mirrored);
   const bool held=q.defender_phase!=TrainingEnemyPhase::Stagger&&((p->compact&&a.active)||(!a.walking&&a.walk==0));
   const bool lowering=desired!=unsigned(a.guard_direction)&&a.guard>.01f;
   const float guard=std::clamp(a.guard+(time-seconds)*(lowering?-12.f:held?10.f:-12.f),0.f,1.f);
   clip=7+(a.guard<.01f?desired:unsigned(a.guard_direction));phase=smooth(guard);}
 }else if(!p->compact&&p->stats.polished&&a.active&&a.guard<=.001f&&(a.walk!=0||a.walking)){
  Actor predicted=a;s.weight=p->weights[who];
  walk_sample(predicted,p->pose_count,true,s.weight,clip,phase,direct,a.walking,elapsed,std::hypot(f.defender[0]-a.last_x,f.defender[2]-a.last_z),nullptr,p->opponents_only?p->opponent_stride_metres:1.3);
  if(direct)s.slot=predicted.slot;
 }else if(a.active&&a.slot>=4&&a.slot<12&&a.guard<=.001f){clip=1;phase=a.phase+float(std::hypot(f.defender[0]-a.last_x,f.defender[2]-a.last_z)/(p->opponents_only?p->opponent_stride_metres:1.3));phase-=std::floor(phase);}
 else if(a.guard>.001f||(elapsed>0&&q.defender_phase!=TrainingEnemyPhase::Stagger&&((p->compact&&a.active)||(!a.walking&&a.walk==0)))){const unsigned desired=anatomy_direction(world_combat_guard_pose(unsigned(q.defender_guard),p->opponents_only),s.mirrored);const bool lowering=a.active&&desired!=unsigned(a.guard_direction)&&a.guard>.01f;
  const float guard=std::clamp(a.guard+elapsed*(lowering?-12.f:q.defender_phase!=TrainingEnemyPhase::Stagger&&((p->compact&&a.active)||(!a.walking&&a.walk==0))?10.f:-12.f),0.f,1.f);clip=7+(!a.active||a.guard<.01f?desired:unsigned(a.guard_direction));phase=smooth(guard);}
 else phase*=world_combat_continuity::idle(std::min(.14f,a.idle_elapsed+elapsed));
 if(!direct){const auto &c=clips[clip];const float key=std::clamp(phase,0.f,c.loop?.999999f:1.f)*(c.loop?c.count:c.count-1);s.slot=c.base+std::min(c.count-1,unsigned(key));s.weight=key-unsigned(key);}
 if(!attacking&&q.defender_phase==TrainingEnemyPhase::Measure&&a.hit<=elapsed&&a.parry<=elapsed)guarded_gait_snapshot(p,a,s);
 s.instance={float(f.defender[0]-p->x),float(f.defender[1]),float(f.defender[2]-p->z),f.defender_yaw,1};s.clock=q.contact_clock;s.valid=true;return s;
}
anatomy_math::Edge anatomy_edge(const WorldCombatMotion *p,unsigned weapon,unsigned direction,float phase,const double root[3],float yaw,bool mirror){
 const Bank *bank=&p->body;const float (*sockets)[2][3]=p->sockets;const world_combat_presentation::Bank *presentation=&p->presentation;
 if(weapon>=2&&weapon<=4&&p->loadouts){const auto &e=p->loadouts->entries[weapon-2];bank=&e.body;sockets=e.sockets;presentation=&e.presentation;}
 direction=anatomy_direction(direction,mirror);if(presentation->upper_body_aimed)phase=world_combat_presentation::source_phase(phase,presentation->contact_frames[direction]);
 const float sample=std::clamp(phase,0.f,1.f)*11;const unsigned key=std::min(11u,unsigned(sample)),slot=12+direction*12+key;const auto pair=pose_pair(slot,p->pose_count);const float w=sample-key;anatomy_math::Edge out;
 for(unsigned point=0;point<2;++point){const auto &rigid=bank->rigid[slot];world_rigid_weapon_pose::Point v={sockets[pair[0]][point][0],sockets[pair[0]][point][1],sockets[pair[0]][point][2]};
  if(rigid.enabled)v=world_rigid_weapon_pose::interpolate(v,rigid.pivot_a,rigid.pivot_b,rigid.rotation,w);
  else for(unsigned k=0;k<3;++k)v[k]+=(sockets[pair[1]][point][k]-v[k])*w;
  const auto q=anatomy_place(anatomy_local(v[0],v[1],v[2]),root[0],root[1],root[2],yaw,mirror);if(point)out.tip=q;else out.base=q;
 }
 // Only the forged axe head cuts; the wooden shaft is never a severing edge.
 if(weapon==unsigned(world_weapon_loadout::Weapon::GreatAxe)){const auto d=out.tip-out.base;const double l=anatomy_math::length(d);if(l>.24)out.base=out.tip-d*(.24/l);}
 return out;
}
anatomy_math::Edge anatomy_defender_edge(const WorldCombatMotion *p,const AnatomySnapshot &s){
 if(s.bridge){anatomy_math::Edge out;if(!clash_edge_local(p,s,out))return {};out.base=anatomy_place(out.base,p->x+s.instance.x,s.instance.y,p->z+s.instance.z,s.instance.yaw,s.mirrored);out.tip=anatomy_place(out.tip,p->x+s.instance.x,s.instance.y,p->z+s.instance.z,s.instance.yaw,s.mirrored);return out;}
 const auto pair=pose_pair(s.slot,p->pose_count);const auto &r=p->body.rigid[s.slot];anatomy_math::Edge out;
 for(unsigned point=0;point<2;++point){world_rigid_weapon_pose::Point v={p->sockets[pair[0]][point][0],p->sockets[pair[0]][point][1],p->sockets[pair[0]][point][2]};
  if(r.enabled)v=world_rigid_weapon_pose::interpolate(v,r.pivot_a,r.pivot_b,r.rotation,s.weight);else for(unsigned k=0;k<3;++k)v[k]+=(p->sockets[pair[1]][point][k]-v[k])*s.weight;
  const auto q=anatomy_place(anatomy_local(v[0],v[1],v[2]),p->x+s.instance.x,s.instance.y,p->z+s.instance.z,s.instance.yaw,s.mirrored);if(point)out.tip=q;else out.base=q;
 }return out;
}
// Both blades can move during one simulation interval. Freezing the guard at
// its final pose would invent an interception before it was visibly raised,
// and can miss a guard which actually crosses an incoming edge mid-frame.
anatomy_math::Contact anatomy_moving_steel(const anatomy_math::Sweep &attack,const anatomy_math::Sweep &guard){
 using namespace anatomy_math;Contact result;
 const auto da=attack.to.base-attack.from.base,db=attack.to.tip-attack.from.tip;
 const auto ga=guard.to.base-guard.from.base,gb=guard.to.tip-guard.from.tip;
 const double speed=std::max(length(da),length(db))+std::max(length(ga),length(gb));
 const double radius=attack.radius+guard.radius;double time=0;
 for(unsigned iteration=0;iteration<512;++iteration){
  const auto d=closest(attack.from.base+da*time,attack.from.tip+db*time,guard.from.base+ga*time,guard.from.tip+gb*time);
  const double gap=d.distance-radius;
  if(gap<=1.e-5){auto normal=d.edge-d.body;const double distance=length(normal);normal=distance>1.e-12?normal*(1./distance):Vec3{0,1,0};return {true,TrainingBodyPart::None,d.body+normal*guard.radius,time};}
  if(speed<=1.e-12)break;const double advance=gap/speed;if(time+advance>1.+1.e-12)break;
  const double next=std::min(1.,time+advance*.95);if(next<=time+1.e-14)break;time=next;
 }return result;
}
anatomy_math::Vec3 anatomy_cross(anatomy_math::Vec3 a,anatomy_math::Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
bool anatomy_in_triangle(anatomy_math::Vec3 point,const std::array<anatomy_math::Vec3,3>&triangle){
 using namespace anatomy_math;const auto x=triangle[1]-triangle[0],y=triangle[2]-triangle[0],v=point-triangle[0];const double xx=dot(x,x),yy=dot(y,y),xy=dot(x,y),xv=dot(x,v),yv=dot(y,v),denominator=xx*yy-xy*xy;
 if(denominator<=1.e-24)return false;const double u=(yy*xv-xy*yv)/denominator,w=(xx*yv-xy*xv)/denominator;return u>=-1.e-10&&w>=-1.e-10&&u+w<=1.+1.e-10;
}
anatomy_math::Closest anatomy_edge_triangle(anatomy_math::Edge edge,const std::array<anatomy_math::Vec3,3>&triangle){
 using namespace anatomy_math;Closest result;result.distance=std::numeric_limits<double>::infinity();
 auto accept=[&](const Closest &v){if(v.distance<result.distance)result=v;};
 for(unsigned i=0;i<3;++i)accept(closest(edge.base,edge.tip,triangle[i],triangle[(i+1)%3]));
 const auto normal=anatomy_cross(triangle[1]-triangle[0],triangle[2]-triangle[0]);const double nn=dot(normal,normal);if(nn<=1.e-24)return result;
 const double from=dot(edge.base-triangle[0],normal),to=dot(edge.tip-triangle[0],normal);
 if(std::abs(to-from)>1.e-20){const double t=-from/(to-from);if(t>=0&&t<=1){const auto point=edge.base+(edge.tip-edge.base)*t;if(anatomy_in_triangle(point,triangle))return {point,point,0};}}
 for(auto point:{edge.base,edge.tip}){const auto projected=point-normal*(dot(point-triangle[0],normal)/nn);if(anatomy_in_triangle(projected,triangle))accept({point,projected,length(point-projected)});}return result;
}
anatomy_math::Contact anatomy_moving_triangle(const anatomy_math::Sweep &attack,const std::array<anatomy_math::Vec3,3>&from,const std::array<anatomy_math::Vec3,3>&to){
 using namespace anatomy_math;Contact result;
 if(length(anatomy_cross(from[1]-from[0],from[2]-from[0]))<1.e-12&&length(anatomy_cross(to[1]-to[0],to[2]-to[0]))<1.e-12)return result;
 // Broadphase uses the swept visible triangle and cutting edge, never a
 // directional shield-sized proxy. It cannot discard an actual crossing.
 for(unsigned axis=0;axis<3;++axis){auto coordinate=[&](Vec3 p){return axis==0?p.x:axis==1?p.y:p.z;};double lo=std::numeric_limits<double>::infinity(),hi=-lo;
  for(auto p:{attack.from.base,attack.from.tip,attack.to.base,attack.to.tip}){lo=std::min(lo,coordinate(p));hi=std::max(hi,coordinate(p));}
  double tl=std::numeric_limits<double>::infinity(),th=-tl;for(unsigned i=0;i<3;++i){tl=std::min({tl,coordinate(from[i]),coordinate(to[i])});th=std::max({th,coordinate(from[i]),coordinate(to[i])});}
  if(hi+attack.radius<tl||th+attack.radius<lo)return result;}
 const auto da=attack.to.base-attack.from.base,db=attack.to.tip-attack.from.tip;double speed=std::max(length(da),length(db));double triangle_speed=0;for(unsigned i=0;i<3;++i)triangle_speed=std::max(triangle_speed,length(to[i]-from[i]));speed+=triangle_speed;double time=0;
 for(unsigned iteration=0;iteration<512;++iteration){std::array<Vec3,3>triangle;for(unsigned i=0;i<3;++i)triangle[i]=from[i]+(to[i]-from[i])*time;
  const auto closest=anatomy_edge_triangle({attack.from.base+da*time,attack.from.tip+db*time},triangle);const double gap=closest.distance-attack.radius;
  if(gap<=1.e-5)return {true,TrainingBodyPart::None,closest.body,time};if(speed<=1.e-12)break;
  const double advance=gap/speed;if(time+advance>1.+1.e-12)break;const double next=std::min(1.,time+advance*.95);if(next<=time+1.e-14)break;time=next;
 }return result;
}
detached::Shape anatomy_shape(const CombatAnatomy &g,const AnatomyPiece &piece){return {g.support_pool.data()+piece.support_offset,piece.support_count};}
void anatomy_compact_support(CombatAnatomy &g){
 std::array<AnatomyPiece*,64> ordered={};unsigned n=0;for(auto &v:g.pieces)if(v.active)ordered[n++]=&v;
 std::sort(ordered.begin(),ordered.begin()+n,[](const auto *a,const auto *b){return a->support_offset<b->support_offset;});std::size_t at=0;
 for(unsigned i=0;i<n;++i){auto &v=*ordered[i];if(v.support_offset!=at)std::memmove(g.support_pool.data()+at,g.support_pool.data()+v.support_offset,v.support_count*sizeof(detached::Vec3));v.support_offset=at;at+=v.support_count;}g.support_pool.resize(at);
}
AnatomyPiece *anatomy_oldest_piece(CombatAnatomy &g,bool sleeping){AnatomyPiece *found=nullptr;for(auto &v:g.pieces)if(v.active&&(!sleeping||v.settled)&&(!found||v.birth<found->birth))found=&v;return found;}
bool anatomy_capture_support(WorldCombatMotion *p,AnatomyPiece &piece,const AnatomySnapshot &snapshot,unsigned prior,unsigned part){
 auto &g=*p->anatomy;g.support_scratch.clear();const auto start=std::chrono::steady_clock::now();
 auto emit=[](float x,float y,float z,void *context){auto &v=*static_cast<std::vector<detached::Vec3>*>(context);if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||v.size()>=240000)return false;v.push_back({x,y,z});return true;};
 auto body=anatomy_draw(snapshot,prior,part),cap=anatomy_draw(snapshot,prior,part,true);bool ok=false;
 try{if(p->compact){const auto index=std::size_t(&piece-g.pieces.data());if(index<p->compact->piece_capacity){auto &v=p->compact->pieces[index];v.uploaded=v.cap_uploaded=false;
   if(snapshot.bridge){if(!clash_commit(p,piece.actor,snapshot))return false;const auto &source=p->deaths->actors[piece.actor];std::copy(source.staging.begin(),source.staging.end(),v.staging.begin());std::copy(source.cap_staging.begin(),source.cap_staging.end(),v.cap_staging.begin());ok=world_model_vertex_stream_upload(v.stream,v.staging.data(),v.staging.size())&&world_model_vertex_stream_upload(v.cap_stream,v.cap_staging.data(),v.cap_staging.size());v.uploaded=v.cap_uploaded=ok;if(!ok)++p->compact->stats.failures;body.weight=cap.weight=0;
    ok=ok&&world_model_pose_blend_capture_support_pnt3(v.body,&body,v.staging.data(),v.staging.size(),emit,&g.support_scratch)&&world_model_pose_blend_capture_support_pnt3(v.cap,&cap,v.cap_staging.data(),v.cap_staging.size(),emit,&g.support_scratch);
   }else ok=compact_commit(p,v,snapshot.slot,snapshot.weight,&snapshot)&&world_model_pose_blend_capture_support_pnt3(v.body,&body,v.staging.data(),v.staging.size(),emit,&g.support_scratch)&&world_model_pose_blend_capture_support3(g.cap_blends[snapshot.slot],&cap,emit,&g.support_scratch);
  }}else ok=world_model_pose_blend_capture_support3(p->enemy_only[snapshot.slot],&body,emit,&g.support_scratch)&&world_model_pose_blend_capture_support3(g.cap_blends[snapshot.slot],&cap,emit,&g.support_scratch);}catch(...){ok=false;}
 if(!ok||g.support_scratch.empty()){++g.stats.support_failures;std::fprintf(stderr,"[Combat anatomy] exactXYZ capture failed actor%u part%u\n",piece.actor,part);return false;}
 auto &points=g.support_scratch;for(auto &v:points)if(snapshot.mirrored)v.x=-v.x;
 std::sort(points.begin(),points.end(),[](const auto &a,const auto &b){return a.x!=b.x?a.x<b.x:a.y!=b.y?a.y<b.y:a.z<b.z;});points.erase(std::unique(points.begin(),points.end(),[](const auto &a,const auto &b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}),points.end());
 detached::Vec3 lo=points[0],hi=lo;for(const auto &v:points){lo.x=std::min(lo.x,v.x);lo.y=std::min(lo.y,v.y);lo.z=std::min(lo.z,v.z);hi.x=std::max(hi.x,v.x);hi.y=std::max(hi.y,v.y);hi.z=std::max(hi.z,v.z);}const auto center=(lo+hi)*.5;piece.pivot={float(center.x),float(center.y),float(center.z)};
 anatomy_compact_support(g);while(g.support_pool.size()+points.size()>anatomy_support_limit){auto *old=anatomy_oldest_piece(g,true);if(!old)old=anatomy_oldest_piece(g,false);if(!old){++g.stats.support_failures;return false;}old->active=false;++g.stats.physics_recycles;anatomy_compact_support(g);}
 piece.support_offset=g.support_pool.size();piece.support_count=points.size();for(const auto &v:points)g.support_pool.push_back(v-center);
 g.stats.support_hull_points_peak=std::max(g.stats.support_hull_points_peak,unsigned(points.size()));g.stats.support_capture_us_max=std::max(g.stats.support_capture_us_max,std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count());
 if(p->variant_stats.loaded)++p->variant_stats.support_captures[(piece.actor-1)%4];return true;
}
void anatomy_launch(WorldCombatMotion *p,unsigned actor,unsigned part,unsigned prior,const WorldTrainingCombat &combat){auto &g=*p->anatomy;auto snapshot=g.contact[actor];
 const bool authority=snapshot.valid&&snapshot.serial==combat.attack_start_serial&&snapshot.contact_part==part&&std::abs(snapshot.clock-combat.clock)<.1&&anatomy_math::finite(snapshot.strike_delta)&&anatomy_math::finite(snapshot.strike_point);
 if(!snapshot.valid||!authority){snapshot={p->instances[actor],p->actors[actor].slot,p->weights[actor],g.mirrored[actor],true};++g.stats.physics_invalid;}
 snapshot.variant_mask=p->compact?0:world_model_variants::actor(actor);
 auto *piece=static_cast<AnatomyPiece*>(nullptr);for(unsigned i=0;i<(p->compact?p->compact->piece_capacity:g.pieces.size());++i){auto &v=g.pieces[i];if(!v.active){piece=&v;break;}}if(!piece){piece=anatomy_oldest_piece(g,true);if(!piece)piece=anatomy_oldest_piece(g,false);++g.stats.physics_recycles;}if(!piece)return;
 *piece={};piece->actor=actor;piece->variant_mask=snapshot.variant_mask;if(!anatomy_capture_support(p,*piece,snapshot,prior,part))return;piece->active=true;piece->birth=++g.piece_serial;piece->part=part;piece->slot=snapshot.slot;piece->weight=snapshot.weight;piece->mirrored=snapshot.mirrored;piece->removed=prior;piece->floor=snapshot.instance.y;
 auto &b=piece->body;b.orientation=detached::yaw(snapshot.instance.yaw);const auto center=detached::rotate(b.orientation,{piece->pivot[0],piece->pivot[1],piece->pivot[2]});b.position={p->x+snapshot.instance.x+center.x,snapshot.instance.y+center.y,p->z+snapshot.instance.z+center.z};
 const double mass=part==1?4.5:part==2?30.:part>=7?8.5:part>=5?.65:3.5;detached::initialize(b,anatomy_shape(g,*piece),mass);
 if(authority){const detached::Vec3 direction=detached::unit({snapshot.strike_delta.x,snapshot.strike_delta.y,snapshot.strike_delta.z},{0,0,0});const detached::Vec3 j=(direction*1.9+detached::Vec3{0,part==1?1.8:1.05,0})*mass;detached::Vec3 r={snapshot.strike_point.x-b.position.x,snapshot.strike_point.y-b.position.y,snapshot.strike_point.z-b.position.z};detached::clamp_length(r,1.1);detached::impulse(b,j,r);}
 ++g.stats.launches[part-1];std::printf("[Combat anatomy] Detached: actor%u part%u pose%u weight%.6f mirror%d serial%llu exactPoints%zu\n",actor,part,piece->slot,piece->weight,int(piece->mirrored),static_cast<unsigned long long>(snapshot.serial),piece->support_count);
}
double anatomy_piece_bottom(const CombatAnatomy &g,const AnatomyPiece &v){return detached::support(v.body,anatomy_shape(g,v)).distance;}
void anatomy_update(WorldCombatMotion *p,const WorldCombatArenaActor *enemies,unsigned count,float dt){if(!p->anatomy)return;auto &g=*p->anatomy;const auto started=std::chrono::steady_clock::now();
 for(unsigned who=1;who<=count;++who){if(!(p->actor_mask&(1u<<who)))continue;const auto &c=enemies[who-1].combat;const unsigned old=g.masks[who],now=c.enemy_severed_parts,added=now&~old;
  for(unsigned part=1;part<=8;++part)if(added&(1u<<(part-1)))anatomy_launch(p,who,part,old,c);
  g.masks[who]=std::uint16_t(now);if(c.enemy_weapon_hand!=TrainingWeaponHand::None)g.mirrored[who]=c.enemy_weapon_hand==TrainingWeaponHand::Left;
 }
 g.stats.active={};g.stats.settled={};g.stats.stumps=0;g.stats.support_points_active=0;
 for(unsigned who=1;who<actor_capacity;++who)if(p->actors[who].active){unsigned m=anatomy_caps_mask(g.masks[who]);for(;m;m&=m-1)++g.stats.stumps;}
 for(auto &v:g.pieces)if(v.active){++g.stats.active[v.part-1];g.stats.support_points_active+=v.support_count;detached::Stats stats;detached::update(v.body,anatomy_shape(g,v),dt,p->fatality_floor,p->fatality_floor_context,v.floor,stats);v.age+=std::isfinite(dt)?std::clamp(dt,0.f,.05f):0;v.settled=v.body.sleeping;if(v.settled)++g.stats.settled[v.part-1];
  g.stats.physics_substeps+=stats.substeps;g.stats.physics_contacts+=stats.contacts;g.stats.physics_sleeps+=stats.sleeps;g.stats.physics_budget_clamps+=stats.clamps;g.stats.physics_invalid+=stats.invalid;g.stats.physics_floor_samples+=stats.floor_samples;g.stats.maximum_ground_penetration=std::max(g.stats.maximum_ground_penetration,stats.penetration);
 }
 g.stats.physics_update_us_max=std::max(g.stats.physics_update_us_max,std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-started).count());
}
void anatomy_draw_actor(WorldCombatMotion *p,unsigned program,const AnatomySnapshot &snapshot,unsigned mask,TrainingWeaponHand hand,unsigned actor=0){auto &g=*p->anatomy;
 if(snapshot.bridge){clash_draw_actor(p,program,actor,snapshot,mask,hand);return;}
 auto body=anatomy_draw(snapshot,mask);body.hide_weapon=hand==TrainingWeaponHand::None;
 if(p->compact){if(actor>=actor_capacity||!(p->actor_mask&(1u<<actor))||!compact_commit(p,p->compact->actors[actor],snapshot.slot,snapshot.weight,&snapshot))return;if(snapshot.gait_mix>0)body.weight=snapshot.gait_weight;world_model_pose_blend_draw_anatomy(p->compact->actors[actor].body,program,&body);}else world_model_pose_blend_draw_anatomy(p->enemy_only[snapshot.slot],program,&body);++g.stats.body_draws;if(snapshot.mirrored)++g.stats.mirrored_draws;
 if(mask){auto cap=anatomy_draw(snapshot,mask,0,true);world_model_pose_blend_draw_anatomy(g.cap_blends[snapshot.slot],program,&cap);++g.stats.cap_draws;}
}
void anatomy_draw_pieces(WorldCombatMotion *p,unsigned program){if(!p->anatomy)return;auto &g=*p->anatomy;
 for(const auto &v:g.pieces)if(v.active){actor_blood_wear(p,program,v.actor);AnatomySnapshot s;s.slot=v.slot;s.weight=v.weight;s.mirrored=v.mirrored;s.variant_mask=v.variant_mask;s.instance={float(v.body.position.x-p->x),float(v.body.position.y),float(v.body.position.z-p->z),0,1};
  auto body=anatomy_draw(s,v.removed,v.part);std::copy(v.pivot.begin(),v.pivot.end(),body.pivot);body.rotation[0]=float(v.body.orientation.x);body.rotation[1]=float(v.body.orientation.y);body.rotation[2]=float(v.body.orientation.z);body.rotation[3]=float(v.body.orientation.w);
  if(p->compact){const auto index=std::size_t(&v-g.pieces.data());if(index>=p->compact->piece_capacity||!p->compact->pieces[index].uploaded)continue;world_model_pose_blend_draw_anatomy_deformed(p->compact->pieces[index].body,program,&body);}else world_model_pose_blend_draw_anatomy(p->enemy_only[v.slot],program,&body);auto cap=body;cap.caps=true;cap.removed=anatomy_caps_mask(v.removed);const auto index=std::size_t(&v-g.pieces.data());if(p->compact&&index<p->compact->piece_capacity&&p->compact->pieces[index].cap_uploaded)world_model_pose_blend_draw_anatomy_deformed(p->compact->pieces[index].cap,program,&cap);else world_model_pose_blend_draw_anatomy(g.cap_blends[v.slot],program,&cap);++g.stats.detached_draws[v.part-1];++g.stats.cap_draws;
  if(p->variant_stats.loaded)++p->variant_stats.piece_draws[(v.actor-1)%4];
 }actor_blood_wear(p,program,0);
}
}
WorldCombatAnatomyStats world_combat_motion_anatomy_stats(const WorldCombatMotion *p){return p&&p->anatomy?p->anatomy->stats:WorldCombatAnatomyStats{};}
namespace {
bool anatomy_read_metadata(const char *path,CombatAnatomy *g,std::vector<unsigned> &counts,std::vector<std::uint64_t> &keys,std::vector<float> &weights,unsigned expected_mesh_count){
 FILE *f=std::fopen(path,"rb");if(!f)return false;
 auto read=[&](void *v,std::size_t n){return std::fread(v,1,n,f)==n;};auto u32=[&](unsigned &v){unsigned char b[4];if(!read(b,4))return false;v=unsigned(b[0])|(unsigned(b[1])<<8)|(unsigned(b[2])<<16)|(unsigned(b[3])<<24);return true;};
 auto flt=[&](float &v){unsigned n=0;if(!u32(n))return false;std::memcpy(&v,&n,4);return std::isfinite(v)&&std::abs(v)<32;};
 char magic[8];unsigned pose_count=0,mesh_count=0,parts_count=0;bool ok=expected_mesh_count>0&&expected_mesh_count<=32&&read(magic,8)&&!std::memcmp(magic,"VWANA002",8)&&u32(pose_count)&&pose_count==poses&&u32(mesh_count)&&mesh_count==expected_mesh_count&&u32(parts_count)&&parts_count==8;
 counts.resize(ok?mesh_count:0);keys.resize(ok?mesh_count:0);weights.clear();
 for(unsigned m=0;ok&&m<mesh_count;++m){ok=u32(counts[m])&&counts[m]>0&&counts[m]<=100000;unsigned lo=0,hi=0;ok=ok&&u32(lo)&&u32(hi);keys[m]=std::uint64_t(lo)|(std::uint64_t(hi)<<32);if(!ok)break;
  const auto at=weights.size();if(at+counts[m]*8>800000){ok=false;break;}weights.resize(at+counts[m]*8);for(std::size_t i=at;ok&&i<weights.size();++i)ok=flt(weights[i])&&weights[i]>=0&&weights[i]<=1;
 }
 for(unsigned i=0;ok&&i<poses;++i){auto &pose=g->poses_data[i];for(auto &w:pose.wounds)for(auto *a:{&w.point,&w.axis})for(auto &x:*a)ok=ok&&flt(x);
  ok=ok&&u32(pose.count)&&pose.count>0&&pose.count<=16;
  for(unsigned j=0;ok&&j<pose.count;++j){unsigned part=0;float v[7]={};ok=u32(part)&&part>0&&part<=8;for(auto &x:v)ok=ok&&flt(x);
   pose.capsules[j]={TrainingBodyPart(part),{v[0],v[1],v[2]},{v[3],v[4],v[5]},v[6]};ok=ok&&v[6]>=.015f&&v[6]<=.65f;}
  for(auto &hull:pose.hulls){unsigned n=0;ok=ok&&u32(n)&&n>=4&&n<=2048;if(!ok)break;hull.resize(n);for(auto &q:hull){float v[3]={};for(auto &x:v)ok=ok&&flt(x);q={v[0],v[1],v[2]};}}
 }
 if(ok){g->leg_weights.resize(weights.size()/8);for(std::size_t i=0;i<g->leg_weights.size();++i)g->leg_weights[i]=std::clamp(weights[i*8+6]+weights[i*8+7],0.f,1.f);}
 ok=ok&&std::fgetc(f)==EOF;std::fclose(f);
 return ok;
}
}
bool world_combat_motion_load_anatomy(WorldCombatMotion *p,const char *directory){
 if(!p||!p->stats.polished||p->anatomy||!directory)return false;char path[1600];const int n=std::snprintf(path,sizeof path,"%s/anatomy.bin",directory);if(n<=0||std::size_t(n)>=sizeof path)return false;
 auto *g=new(std::nothrow)CombatAnatomy;if(!g)return false;std::vector<unsigned> counts;std::vector<std::uint64_t> keys;std::vector<float> weights;
 bool ok=anatomy_read_metadata(path,g,counts,keys,weights,unsigned(world_model_pose_mesh_count(p->body.assets[0])));
 if(ok&&p->compact){const auto &meshes=p->compact->packet.meshes();ok=meshes.size()==counts.size();for(unsigned m=0;ok&&m<counts.size();++m)ok=meshes[m].count==counts[m]&&meshes[m].layout==keys[m];}
 if(ok)g->stream=world_model_anatomy_stream_create(p->body.assets[0],counts.data(),keys.data(),weights.data(),counts.size());ok=ok&&g->stream;
 for(unsigned i=0;ok&&i<poses;++i){std::snprintf(path,sizeof path,"%s/caps/cap_%03u.glb",directory,i);g->cap_assets[i]=world_model_load_pose_support_pnt(path,WORLD_MODEL_POSE_BLEND_VERSION,true);ok=g->cap_assets[i];}
 for(unsigned slot=0;ok&&slot<p->batch_count;++slot){const auto pair=pose_pair(slot,p->pose_count);g->cap_blends[slot]=world_model_pose_blend_create(1,WORLD_MODEL_POSE_BLEND_VERSION);
  ok=g->cap_blends[slot]&&world_model_pose_blend_set_targets(g->cap_blends[slot],g->cap_assets[pair[0]],g->cap_assets[pair[1]])&&(p->compact||world_model_pose_blend_attach_anatomy(p->enemy_only[slot],g->stream));
  // Uniform placement still requires valid enabled instance buffers on GL3.3.
  // Allocate one seed once at loading, including never-before-drawn frozen poses.
  const WorldModelInstance seed={0,0,0,0,1};const float zero=0;
  ok=ok&&world_model_pose_blend_upload(g->cap_blends[slot],&seed,&zero,1)&&(p->compact||world_model_pose_blend_upload(p->enemy_only[slot],&seed,&zero,1));
 }
 if(!ok){free_anatomy(g);std::fprintf(stderr,"[Combat anatomy] Original-rig metadata/cap contract rejected\n");return false;}
 try{g->support_scratch.reserve(240000);g->support_pool.reserve(anatomy_support_limit);}catch(...){free_anatomy(g);return false;}
 for(unsigned i=0;i<poses;++i)g->stats.support_retained_bytes+=world_model_pose_support_bytes(p->body.assets[i])+world_model_pose_support_bytes(g->cap_assets[i]);
 p->anatomy=g;if((p->compact&&!compact_prepare_streams(p))||!death_prepare(p)){p->anatomy=nullptr;free_anatomy(g);return false;}g->stats.loaded=true;std::printf("[Combat anatomy] Ready: 172 original-rig poses, %zu exact layouts, 8 cut regions\n",counts.size());return true;
}
#include "engine/world_combat_weapon_surface_actions.inc"
bool world_combat_motion_contact(const WorldCombatMotion *p,const TrainingAnatomicalQuery &q,const WorldCombatContactFrame &f,TrainingAnatomicalHit &out){
 out={};if(!p||!p->anatomy||q.opponent_id>=npc_capacity||!(p->actor_mask&(1u<<(q.opponent_id+1)))||unsigned(q.weapon)>=5||q.weapon==world_weapon_loadout::Weapon::Bow||unsigned(q.direction)>=5||!std::isfinite(q.phase_begin)||!std::isfinite(q.phase_end)||q.phase_begin<0||q.phase_end>1||q.phase_end<q.phase_begin||!std::isfinite(q.interval_seconds)||q.interval_seconds<0||q.interval_seconds>.1||unsigned(q.defender_phase)>4||unsigned(q.defender_direction)>4||unsigned(q.defender_guard)>4||!std::isfinite(q.defender_time)||q.defender_time<0||!std::isfinite(q.defender_duration)||q.defender_duration<=0||!std::isfinite(q.contact_clock)||q.severed_parts>255)return false;
 if(q.defender_clash_serial&&(!std::isfinite(q.defender_clash_phase)||q.defender_clash_phase<0||q.defender_clash_phase>1||!std::isfinite(q.defender_clash_time)||q.defender_clash_time<0))return false;
 for(const auto *root:{f.player,f.defender})for(unsigned k=0;k<3;++k)if(!std::isfinite(root[k])||std::abs(root[k])>1.e7)return false;
 if(f.attacker_blades||f.attacker_blade_count){
  if(!f.attacker_blades||f.attacker_blade_count<2||f.attacker_blade_count>65)return false;
  for(unsigned i=0;i<f.attacker_blade_count;++i){const auto &v=f.attacker_blades[i];const anatomy_math::Vec3 a{v.base[0],v.base[1],v.base[2]},b{v.tip[0],v.tip[1],v.tip[2]};
   if(!anatomy_math::finite(a)||!anatomy_math::finite(b)||anatomy_math::length(b-a)>4)return false;
  }
 }
 const auto *attacker=f.attacker_motion?f.attacker_motion:p;
 if(!f.attacker_blades&&p->opponents_only&&(!f.attacker_motion||attacker->opponents_only||!attacker->weapon_sockets||(unsigned(q.weapon)>=2&&!attacker->loadouts)))return false;
 if(!std::isfinite(f.player_yaw)||!std::isfinite(f.defender_yaw))return false;auto &g=*p->anatomy;++g.stats.contact_queries;
 const auto defender=anatomy_defender(p,q,f);g.contact[q.opponent_id+1]=defender;AnatomyPose visible_pose;if(!clash_metadata(p,defender,visible_pose))return false;
 std::array<anatomy_math::Capsule,18> capsules={};unsigned count=0;
 const bool visible_weapon=anatomy_math::can_fight(q.severed_parts);
 for(unsigned i=0;i<visible_pose.count;++i){auto c=visible_pose.capsules[i];if(defender.mirrored)c.part=TrainingBodyPart(anatomy_swapped(unsigned(c.part)));
  c.a=anatomy_place(c.a,f.defender[0],f.defender[1],f.defender[2],f.defender_yaw,defender.mirrored);c.b=anatomy_place(c.b,f.defender[0],f.defender[1],f.defender[2],f.defender_yaw,defender.mirrored);capsules[count++]=c;}
 const unsigned segments=f.attacker_blades?f.attacker_blade_count-1:12;
 auto edge_at=[&](unsigned i){if(f.attacker_blades){const auto &v=f.attacker_blades[i];return anatomy_math::Edge{{v.base[0],v.base[1],v.base[2]},{v.tip[0],v.tip[1],v.tip[2]}};}
  return anatomy_edge(attacker,unsigned(q.weapon),unsigned(q.direction),float(q.phase_begin+(q.phase_end-q.phase_begin)*double(i)/segments),f.player,f.player_yaw,false);};
 auto defender_at=[&](double fraction){if(!std::isfinite(q.interval_seconds)||q.interval_seconds<=0)return defender;auto sample=q;const double back=std::min(q.interval_seconds,.1)*(1-fraction);sample.contact_clock-=back;sample.defender_time=std::max(0.,sample.defender_time-back);return anatomy_defender(p,sample,f);};
 auto previous=edge_at(0);anatomy_math::Edge guard_previous;if(visible_weapon)guard_previous=anatomy_defender_edge(p,defender_at(0));
 for(unsigned i=1;i<=segments;++i){auto next=edge_at(i);
  auto hit=anatomy_math::sweep_contact({previous,next,.012},capsules.data(),count,q.severed_parts);
  if(visible_weapon){const auto guard_next=anatomy_defender_edge(p,defender_at(double(i)/segments));const auto steel=anatomy_moving_steel({previous,next,.012},{guard_previous,guard_next,.027});if(steel.hit&&(!hit.hit||steel.time<=hit.time+1.e-9))hit=steel;guard_previous=guard_next;}
  if(visible_weapon)anatomy_structural_weapon_contact(p,{previous,next,.012},defender_at(double(i-1)/segments),defender_at(double(i)/segments),hit);
  if(hit.hit){out.valid=true;out.weapon_contact=hit.part==TrainingBodyPart::None;out.cutting_edge=q.weapon!=world_weapon_loadout::Weapon::Staff;out.part=hit.part;out.point[0]=hit.point.x;out.point[1]=hit.point.y;out.point[2]=hit.point.z;out.sweep_t=(i-1+hit.time)/segments;
   auto &cached=g.contact[q.opponent_id+1];cached=defender_at(out.sweep_t);cached.clock=q.contact_clock;cached.serial=q.attack_serial;cached.contact_part=unsigned(hit.part);cached.strike_point=hit.point;
   if(out.weapon_contact&&!cached.bridge){out.defender_attack_phase=cached.attack_phase;out.defender_pose_slot=cached.slot;out.defender_pose_weight=cached.weight;}
   const auto edge=anatomy_math::Edge{previous.base+(next.base-previous.base)*hit.time,previous.tip+(next.tip-previous.tip)*hit.time};const auto blade=edge.tip-edge.base;const double along=std::clamp(anatomy_math::dot(hit.point-edge.base,blade)/std::max(1e-12,anatomy_math::dot(blade,blade)),0.,1.);cached.strike_delta=(next.base-previous.base)*(1-along)+(next.tip-previous.tip)*along;
   ++g.stats.contact_hits;if(out.weapon_contact)++g.stats.weapon_contacts;return true;}previous=next;
 }return false;
}
namespace {
float anatomy_incoming_phase(const WorldCombatMotion *p,unsigned opponent_id,const TrainingAnatomicalQuery &q,double phase){
 if(q.windup_seconds<=0)return float(phase);
 if(phase<=.36)return attack_phase(TrainingPlayerPhase::Windup,phase/.36*q.windup_seconds,q.windup_seconds,p->actors[opponent_id+1].attack_leadin,p->stats.polished);
 if(phase<=.60)return attack_phase(TrainingPlayerPhase::Strike,(phase-.36)/.24*q.strike_seconds,q.strike_seconds,0,p->stats.polished);
 return float(phase);
}
anatomy_math::Edge anatomy_incoming_edge(const WorldCombatMotion *p,unsigned opponent_id,const TrainingAnatomicalQuery &q,const WorldCombatIncomingFrame &f,double phase){
 return anatomy_edge(p,0,unsigned(q.direction),anatomy_incoming_phase(p,opponent_id,q,phase),f.opponent,f.opponent_yaw,anatomy_math::surviving_hand(q.severed_parts)==TrainingWeaponHand::Left);
}
}
bool world_combat_motion_incoming_blade(const WorldCombatMotion *p,unsigned opponent_id,const TrainingAnatomicalQuery &q,const WorldCombatIncomingFrame &f,double phase,WorldCombatBlade &out){
 out={};if(!p||!p->anatomy||!p->weapon_sockets||opponent_id>=npc_capacity||!(p->actor_mask&(1u<<(opponent_id+1)))||unsigned(q.direction)>=5||q.severed_parts>255||!anatomy_math::can_fight(q.severed_parts)||!std::isfinite(phase)||phase<0||phase>1||!std::isfinite(q.windup_seconds)||q.windup_seconds<0||!std::isfinite(q.strike_seconds)||q.strike_seconds<=0||!root_ok(f.opponent[0],f.opponent[1],f.opponent[2])||!std::isfinite(f.opponent_yaw))return false;
 const auto edge=anatomy_incoming_edge(p,opponent_id,q,f,phase);if(!anatomy_math::finite(edge.base)||!anatomy_math::finite(edge.tip))return false;out.base[0]=edge.base.x;out.base[1]=edge.base.y;out.base[2]=edge.base.z;out.tip[0]=edge.tip.x;out.tip[1]=edge.tip.y;out.tip[2]=edge.tip.z;return true;
}
bool world_combat_motion_incoming_contact(const WorldCombatMotion *p,unsigned opponent_id,const TrainingAnatomicalQuery &q,const WorldCombatIncomingFrame &f,TrainingAnatomicalHit &out){
 out={};if(!p||!p->anatomy||!p->weapon_sockets||opponent_id>=npc_capacity||!(p->actor_mask&(1u<<(opponent_id+1)))||unsigned(q.direction)>=5||q.severed_parts>255||!anatomy_math::can_fight(q.severed_parts)||!std::isfinite(q.phase_begin)||!std::isfinite(q.phase_end)||q.phase_begin<0||q.phase_end>1||q.phase_end<q.phase_begin||!root_ok(f.opponent[0],f.opponent[1],f.opponent[2])||!std::isfinite(f.opponent_yaw)||!f.defender_body||!f.defender_body_count||f.defender_body_count>24)return false;
 if(f.defender_blades||f.defender_blade_count){if(!f.defender_blades||f.defender_blade_count<2||f.defender_blade_count>65)return false;
  for(unsigned i=0;i<f.defender_blade_count;++i){const auto &v=f.defender_blades[i];const anatomy_math::Vec3 a{v.base[0],v.base[1],v.base[2]},b{v.tip[0],v.tip[1],v.tip[2]};if(!anatomy_math::finite(a)||!anatomy_math::finite(b)||anatomy_math::length(b-a)>4)return false;}}
 for(unsigned i=0;i<f.defender_body_count;++i){const auto &c=f.defender_body[i];if(!anatomy_math::valid(c.part)||!anatomy_math::finite(c.a)||!anatomy_math::finite(c.b)||anatomy_math::length(c.b-c.a)>3||!std::isfinite(c.radius)||c.radius<.015||c.radius>.65)return false;}
 if(f.defender_guards||f.defender_guard_count){if(!f.defender_guards||!f.defender_guard_count||f.defender_guard_count>16)return false;for(unsigned i=0;i<f.defender_guard_count;++i){const auto &c=f.defender_guards[i];if(c.part!=TrainingBodyPart::None||!anatomy_math::finite(c.a)||!anatomy_math::finite(c.b)||anatomy_math::length(c.b-c.a)>3||!std::isfinite(c.radius)||c.radius<.015||c.radius>.65)return false;}}
 // A small fixed-stack broadphase preserves source order and exact triangle
 // narrowphase. Each cluster bounds every vertex at BOTH query endpoints;
 // linear moving-guard interpolation stays inside their conservative union.
 constexpr unsigned surface_cluster_size=32,max_surface_clusters=16384/surface_cluster_size;
 struct SurfaceCluster {double lo[3],hi[3];};
 std::array<SurfaceCluster,max_surface_clusters> surface_clusters;
 const unsigned surface_cluster_count=(f.defender_guard_triangle_count+surface_cluster_size-1)/surface_cluster_size;
 if(f.defender_guard_triangles||f.defender_guard_triangles_end||f.defender_guard_triangle_count){
  if(!f.defender_guard_triangles||!f.defender_guard_triangle_count||f.defender_guard_triangle_count>16384)return false;
  for(unsigned i=0;i<f.defender_guard_triangle_count;++i){auto &cluster=surface_clusters[i/surface_cluster_size];
   if(i%surface_cluster_size==0)for(unsigned axis=0;axis<3;++axis){cluster.lo[axis]=std::numeric_limits<double>::infinity();cluster.hi[axis]=-cluster.lo[axis];}
   const auto &from=f.defender_guard_triangles[i],&to=f.defender_guard_triangles_end?f.defender_guard_triangles_end[i]:from;
   for(unsigned j=0;j<9;++j){if(!std::isfinite(from[j])||std::abs(from[j])>1.e7||!std::isfinite(to[j])||std::abs(to[j])>1.e7)return false;
    const unsigned axis=j%3;cluster.lo[axis]=std::min({cluster.lo[axis],from[j],to[j]});cluster.hi[axis]=std::max({cluster.hi[axis],from[j],to[j]});}
  }
 }
 auto &stats=p->anatomy->stats;++stats.contact_queries;
 const unsigned segments=std::max(12u,f.defender_blade_count?f.defender_blade_count-1:0u);
 if(!std::isfinite(q.windup_seconds)||q.windup_seconds<0||!std::isfinite(q.strike_seconds)||q.strike_seconds<=0)return false;
 // Core intervals are linear combat clocks. The visible NPC uses its real
 // entry lead-in and the polished attack curve; sample that curve at each
 // swept time rather than treating the core's linear fraction as a pose key.
 // A zero windup retains direct source-phase queries for asset diagnostics.
 auto visible_phase=[&](double fraction){return anatomy_incoming_phase(p,opponent_id,q,q.phase_begin+(q.phase_end-q.phase_begin)*fraction);};
 auto attack_at=[&](unsigned i){return anatomy_incoming_edge(p,opponent_id,q,f,q.phase_begin+(q.phase_end-q.phase_begin)*double(i)/segments);};
 auto guard_at=[&](unsigned i){const double sample=double(i)*(f.defender_blade_count-1)/segments;const unsigned a=std::min(f.defender_blade_count-1,unsigned(sample)),b=std::min(f.defender_blade_count-1,a+1);const double t=sample-a;
  auto point=[&](const double *x,const double *y){return anatomy_math::Vec3{x[0]+(y[0]-x[0])*t,x[1]+(y[1]-x[1])*t,x[2]+(y[2]-x[2])*t};};return anatomy_math::Edge{point(f.defender_blades[a].base,f.defender_blades[b].base),point(f.defender_blades[a].tip,f.defender_blades[b].tip)};};
 auto previous=attack_at(0);anatomy_math::Edge guard_previous;if(f.defender_blade_count)guard_previous=guard_at(0);
 for(unsigned i=1;i<=segments;++i){const auto next=attack_at(i);auto hit=anatomy_math::sweep_contact({previous,next,.012},f.defender_body,f.defender_body_count);
  auto prefer_steel=[&](const anatomy_math::Contact &steel){if(steel.hit&&(!hit.hit||steel.time<=hit.time+1.e-9))hit=steel;};
  if(f.defender_blade_count){const auto guard_next=guard_at(i);const auto steel=anatomy_moving_steel({previous,next,.012},{guard_previous,guard_next,.027});if(steel.hit&&(!hit.hit||steel.time<=hit.time+1.e-9))hit=steel;guard_previous=guard_next;}
  if(f.defender_guard_count)prefer_steel(anatomy_math::sweep_contact({previous,next,.012},f.defender_guards,f.defender_guard_count));
  double attack_lo[3]={std::numeric_limits<double>::infinity(),std::numeric_limits<double>::infinity(),std::numeric_limits<double>::infinity()},attack_hi[3]={-attack_lo[0],-attack_lo[1],-attack_lo[2]};
  for(auto point:{previous.base,previous.tip,next.base,next.tip}){const double v[3]={point.x,point.y,point.z};for(unsigned axis=0;axis<3;++axis){attack_lo[axis]=std::min(attack_lo[axis],v[axis]);attack_hi[axis]=std::max(attack_hi[axis],v[axis]);}}
  for(unsigned cluster_id=0;cluster_id<surface_cluster_count;++cluster_id){const auto &cluster=surface_clusters[cluster_id];bool overlaps=true;
   for(unsigned axis=0;axis<3;++axis)if(attack_hi[axis]+.0120001<cluster.lo[axis]||cluster.hi[axis]+.0120001<attack_lo[axis]){overlaps=false;break;}
   if(!overlaps)continue;
   const unsigned end=std::min(f.defender_guard_triangle_count,(cluster_id+1)*surface_cluster_size);
   for(unsigned triangle=cluster_id*surface_cluster_size;triangle<end;++triangle){std::array<anatomy_math::Vec3,3>from,to;const auto &a=f.defender_guard_triangles[triangle],&b=f.defender_guard_triangles_end?f.defender_guard_triangles_end[triangle]:a;
    for(unsigned point=0;point<3;++point){const unsigned offset=point*3;const anatomy_math::Vec3 start{a[offset],a[offset+1],a[offset+2]},delta{b[offset]-a[offset],b[offset+1]-a[offset+1],b[offset+2]-a[offset+2]};from[point]=start+delta*(double(i-1)/segments);to[point]=start+delta*(double(i)/segments);}prefer_steel(anatomy_moving_triangle({previous,next,.012},from,to));}
  }
  if(hit.hit){out.valid=true;out.weapon_contact=hit.part==TrainingBodyPart::None;out.cutting_edge=true;out.part=hit.part;out.point[0]=hit.point.x;out.point[1]=hit.point.y;out.point[2]=hit.point.z;out.sweep_t=(i-1+hit.time)/segments;out.attack_phase=visible_phase(out.sweep_t);++stats.contact_hits;if(out.weapon_contact)++stats.weapon_contacts;return true;}previous=next;
 }return false;
}
bool world_combat_motion_team_contact(const WorldCombatMotion *attacker,const WorldCombatMotion *defender,const world_arena_combat::TeamContactQuery &q,TrainingAnatomicalHit &out){
 out={};
 if(!attacker||!defender||q.attacker_id>=npc_capacity||q.defender_id>=npc_capacity||q.attacker_id==q.defender_id||
    !(attacker->actor_mask&(1u<<(q.attacker_id+1)))||!(defender->actor_mask&(1u<<(q.defender_id+1)))||
    !std::isfinite(q.clock)||!std::isfinite(q.interval_seconds)||q.interval_seconds<0||q.interval_seconds>.1||
    !std::isfinite(q.phase_begin)||!std::isfinite(q.phase_end)||q.phase_begin<0||q.phase_end>1||q.phase_end<q.phase_begin||
    !std::isfinite(q.attacker_yaw)||!std::isfinite(q.defender_yaw)||!std::isfinite(q.strike_seconds)||q.strike_seconds<=0||unsigned(q.direction)>=5||
    !anatomy_math::can_fight(q.attacker_state.severed_parts))return false;
 TrainingAnatomicalQuery attack;attack.opponent_id=q.attacker_id;attack.direction=q.direction;attack.attack_serial=q.attack_serial;
 attack.phase_begin=q.phase_begin;attack.phase_end=q.phase_end;attack.interval_seconds=q.interval_seconds;attack.contact_clock=q.clock;
 attack.windup_seconds=q.attacker_state.attack_windup_seconds;attack.strike_seconds=q.strike_seconds;
 attack.severed_parts=q.attacker_state.severed_parts;
 WorldCombatIncomingFrame source;std::copy_n(q.attacker_position.data(),3,source.opponent);source.opponent_yaw=float(q.attacker_yaw);
 std::array<WorldCombatBlade,13> blades;
 for(unsigned i=0;i<blades.size();++i)if(!world_combat_motion_incoming_blade(attacker,q.attacker_id,attack,source,q.phase_begin+(q.phase_end-q.phase_begin)*double(i)/(blades.size()-1),blades[i]))return false;
 auto hit_query=attack;hit_query.opponent_id=q.defender_id;hit_query.severed_parts=q.defender_state.severed_parts;
 hit_query.defender_phase=q.defender_state.phase;hit_query.defender_direction=q.defender_state.direction;hit_query.defender_guard=q.defender_state.guard;
 hit_query.defender_time=q.defender_state.time;hit_query.defender_duration=q.defender_state.duration;
 hit_query.defender_clash_serial=q.defender_state.blocked_attack_serial;hit_query.defender_clash_phase=q.defender_state.blocked_attack_phase;hit_query.defender_clash_time=q.defender_state.blocked_attack_time;
 hit_query.defender_clash_pose_slot=q.defender_state.blocked_pose_slot;hit_query.defender_clash_pose_weight=q.defender_state.blocked_pose_weight;
 WorldCombatContactFrame frame;std::copy_n(q.attacker_position.data(),3,frame.player);std::copy_n(q.defender_position.data(),3,frame.defender);
 frame.player_yaw=float(q.attacker_yaw);frame.defender_yaw=float(q.defender_yaw);frame.attacker_motion=attacker;frame.attacker_blades=blades.data();frame.attacker_blade_count=unsigned(blades.size());
 if(!world_combat_motion_contact(defender,hit_query,frame,out))return false;
 out.attack_phase=anatomy_incoming_phase(attacker,q.attacker_id,attack,q.phase_begin+(q.phase_end-q.phase_begin)*out.sweep_t);
 return true;
}
bool world_combat_motion_stump_at(const WorldCombatMotion *p,TrainingBodyPart part,float collapse,double x,double feet,double z,float yaw,TrainingWeaponHand hand,double point[3],double axis[3],unsigned actor_id){
 if(!p||!p->anatomy||!anatomy_math::valid(part)||!point||!axis||!root_ok(x,feet,z)||!std::isfinite(yaw)||!std::isfinite(collapse))return false;
 if(p->deaths&&actor_id<actor_capacity&&p->deaths->actors[actor_id].pose.valid)collapse=p->deaths->actors[actor_id].pose.phase;
 const float key=std::clamp(collapse,0.f,1.f)*23;const unsigned slot=collapse>=0?148+std::min(23u,unsigned(key)):0;const float weight=collapse>=0?key-unsigned(key):0;
 const bool mirror=hand==TrainingWeaponHand::Left||(hand==TrainingWeaponHand::None&&actor_id>0&&actor_id<actor_capacity&&p->anatomy->mirrored[actor_id]);auto q=anatomy_wound_point(*p->anatomy,slot,weight,unsigned(part),mirror,false);auto n=anatomy_wound_point(*p->anatomy,slot,weight,unsigned(part),mirror,true);
 death_wound_local(p,actor_id,unsigned(part),slot,weight,mirror,q,n);if(p->deaths&&actor_id<actor_capacity&&p->deaths->actors[actor_id].pose.valid)yaw=p->deaths->actors[actor_id].pose.yaw;
 q=anatomy_place(q,x,feet,z,yaw);n=anatomy_place(n,0,0,0,yaw);const double length=std::max(1.e-9,anatomy_math::length(n));point[0]=q.x;point[1]=q.y;point[2]=q.z;axis[0]=n.x/length;axis[1]=n.y/length;axis[2]=n.z/length;return true;
}
bool world_combat_motion_stump(const WorldCombatMotion *p,unsigned actor,TrainingBodyPart part,double point[3],double axis[3]){
 if(!p||!p->anatomy||actor>=actor_capacity||!p->actors[actor].active||!anatomy_math::valid(part)||!point||!axis)return false;const auto &g=*p->anatomy;const auto &instance=p->instances[actor];
 auto q=anatomy_wound_point(g,p->actors[actor].slot,p->weights[actor],unsigned(part),g.mirrored[actor],false);auto n=anatomy_wound_point(g,p->actors[actor].slot,p->weights[actor],unsigned(part),g.mirrored[actor],true);
 if(actor&&p->compact&&(p->actors[actor].steel_rebound||p->actors[actor].reaction_bridge)){AnatomySnapshot snapshot;snapshot.mirrored=g.mirrored[actor];if(npc_reaction_snapshot(p,actor,p->actors[actor].last_clock,snapshot)||clash_snapshot(p,p->actors[actor],p->actors[actor].last_clock,snapshot)){q=clash_wound(p,snapshot,unsigned(part),false);n=clash_wound(p,snapshot,unsigned(part),true);}}
 death_wound_local(p,actor,unsigned(part),p->actors[actor].slot,p->weights[actor],g.mirrored[actor],q,n);
 q=anatomy_place(q,p->x+instance.x,instance.y,p->z+instance.z,instance.yaw);n=anatomy_place(n,0,0,0,instance.yaw);const double length=std::max(1.e-9,anatomy_math::length(n));point[0]=q.x;point[1]=q.y;point[2]=q.z;axis[0]=n.x/length;axis[1]=n.y/length;axis[2]=n.z/length;return true;
}

unsigned world_combat_motion_detached_states(const WorldCombatMotion *p,WorldCombatDetachedPieceState *out,unsigned capacity){
 if(!p||!p->anatomy||!out||!capacity)return 0;unsigned count=0;for(const auto &v:p->anatomy->pieces)if(v.active&&count<std::min(capacity,64u)){auto &s=out[count++];s={};s.active=true;s.settled=v.settled;s.actor=v.actor;s.part=v.part;const auto &b=v.body;s.position[0]=b.position.x;s.position[1]=b.position.y;s.position[2]=b.position.z;s.orientation[0]=b.orientation.x;s.orientation[1]=b.orientation.y;s.orientation[2]=b.orientation.z;s.orientation[3]=b.orientation.w;s.velocity[0]=b.velocity.x;s.velocity[1]=b.velocity.y;s.velocity[2]=b.velocity.z;s.angular_velocity[0]=b.omega.x;s.angular_velocity[1]=b.omega.y;s.angular_velocity[2]=b.omega.z;const auto low=detached::support(b,anatomy_shape(*p->anatomy,v));s.lowest_support=b.position.y+low.distance;s.ground=v.floor;}return count;
}
void world_combat_motion_settle_pieces(WorldCombatMotion *p,float dt){
 if(!p||!p->anatomy||!std::isfinite(dt)||dt<=0)return;
 if(dt>.05f)++p->anatomy->stats.physics_budget_clamps;
 anatomy_update(p,nullptr,0,std::min(dt,.05f));
}

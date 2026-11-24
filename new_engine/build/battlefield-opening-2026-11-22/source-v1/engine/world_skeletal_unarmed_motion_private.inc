// Owned, source-captured civilian transitions. Gameplay owns every swept root.
namespace {
constexpr const char*unarmed_motion_names[]={
 "WalkFwdStart","RunFwdStart","WalkFwdStop_LU","WalkFwdStop_RU","RunFwdStop_LU","RunFwdStop_RU",
 "TurnRt90_Loop","TurnLt90_Loop","TurnRt180","TurnLt180",
 "RunFwdTurn180_R_LU","RunFwdTurn180_R_RU","RunFwdTurn180_L_RU","RunFwdTurn180_L_LU",
 "WalkFwdStart90_L","WalkFwdStart90_R","WalkFwdStart180_L","WalkFwdStart180_R",
 "RunFwdStart90_L","RunFwdStart90_R","RunFwdStart180_L","RunFwdStart180_R"};
void rotate_unarmed_pose(const Skeleton&s,unsigned root,Pose&p,double angle,std::string&e){
 if(std::abs(angle)<1.e-8)return;
 const Quat yaw=qexp({0,float(angle),0});const int parent=s.joints[root].parent;
 const Quat global=qproduct(yaw,world_rotation(s,p,root));
 p.local[root].rotation=parent<0?global:qunit(qproduct(qconjugate(world_rotation(s,p,unsigned(parent))),global));
 world_animation::evaluate(s,p,e);
}
double unarmed_stop_entry(const std::vector<std::array<double,5>>&curve,double speed){
 // These takes include an entire captured braking stride (up to two metres).
 // Releasing a control must not commit that whole stride. Select a later
 // authored tail, preserving its pose/root relationship and physical anchors.
 const double budget=speed<=2.2?std::clamp(speed*.16,.04,.20):speed<=3.8?.30:.45;
 const double distance=std::max(0.,curve.back()[4]-budget);
 for(unsigned n=1;n<curve.size();++n){
  const auto&a=curve[n-1];const auto&b=curve[n];
  if(b[4]<distance)continue;
  const double velocity=(b[4]-a[4])/std::max(1.e-8,b[0]-a[0]);
  // A small analog release keeps the original velocity-matched tail too.
  if(speed<1.2&&velocity>speed)continue;
  const double f=b[4]>a[4]?std::clamp((distance-a[4])/(b[4]-a[4]),0.,1.):0;
  return a[0]+(b[0]-a[0])*f;
 }
 return curve.back()[0];
}
double unarmed_stop_speed(const std::vector<std::array<double,5>>&curve,double time){
 for(unsigned n=1;n<curve.size();++n)if(curve[n][0]>time+1.e-8)
  return (curve[n][4]-curve[n-1][4])/std::max(1.e-8,curve[n][0]-curve[n-1][0]);
 return 0;
}
double unarmed_pose_cost(const Pose&a,const Pose&b,const unsigned*points,unsigned count){
 double cost=0;for(unsigned k=0;k<count;++k){const auto d=vsub(origin(a.global[points[k]]),origin(b.global[points[k]]));cost+=vdot(d,d)*(k<4?3.:.25);}
 return cost;
}
}
bool WorldSkeletalHero::unarmed_pose_velocity(const Pose&previous,const Pose&current,double dt,double previous_yaw,double current_yaw,UnarmedPoseVelocity&velocity,std::string&e)const{
 velocity={};if(dt<.00001||dt>.1||previous.nodes!=current.nodes)return true;
 // Express both observations in the entry's actor frame before measuring
 // local translation and quaternion-log velocity. A turn of the actor must
 // not become a spurious joint impulse when the outgoing pose is continued.
 Pose aligned=previous;rotate_unarmed_pose(asset_.skeleton,unarmed_motion_root_,aligned,std::remainder(previous_yaw-current_yaw,tau),e);if(!e.empty())return false;
 for(unsigned n=0;n<current.nodes;++n){velocity.translation[n]=vmul(vsub(current.local[n].translation,aligned.local[n].translation),float(1/dt));velocity.rotation[n]=vmul(qlog(qproduct(current.local[n].rotation,qconjugate(aligned.local[n].rotation))),float(1/dt));}
 return true;
}
bool WorldSkeletalHero::continue_unarmed_pose(Pose&pose,const UnarmedPoseVelocity&velocity,double seconds,double duration,std::string&e)const{
 if(seconds<=0||seconds>=duration)return true;
 // Continue the outgoing derivative briefly, then smoothly saturate it. The
 // correction is faded by (1-smooth(age/duration)) in the caller. This keeps
 // both endpoint derivatives without extrapolating a limb for the full .5s
 // crouch recovery or crossing a quaternion hemisphere at large angular speed.
 const double horizon=std::min(.075,duration*.5),time=-horizon*std::expm1(-seconds/horizon);
 for(unsigned n=0;n<pose.nodes;++n){
  Vec3 translation=vmul(velocity.translation[n],float(time));const double distance=vlength(translation),limit=std::max(.01,double(vlength(asset_.skeleton.joints[n].rest.translation))*.25);
  if(distance>1.e-9)translation=vmul(translation,float(limit*std::tanh(distance/limit)/distance));
  Vec3 rotation=vmul(velocity.rotation[n],float(time));const double angle=vlength(rotation);if(angle>1.e-9)rotation=vmul(rotation,float(.6*std::tanh(angle/.6)/angle));
  pose.local[n].translation=vadd(pose.local[n].translation,translation);pose.local[n].rotation=qunit(qproduct(qexp(rotation),pose.local[n].rotation));
 }
 return world_animation::evaluate(asset_.skeleton,pose,e);
}
void WorldSkeletalHero::reset_unarmed_motion(){
 motion_enabled_=motion_planned_=motion_have_heading_=motion_blocked_=false;motion_mode_=0;
 unarmed_motion_time_=motion_next_time_=motion_plan_dt_=motion_plan_remaining_=motion_speed_cap_=motion_last_wish_speed_=motion_blocked_time_=motion_capture_age_=0;
 motion_previous_dt_=0;motion_bridge_pending_=false;motion_entry_velocity_={};motion_bridge_velocity_={};
 motion_root_scale_=1;
 motion_bridge_age_=.2;motion_acceptance_=1;motion_actual_speed_=0;motion_support_override_=0;motion_current_support_={};stats_.captured_motion_mode=0;stats_.captured_motion_time=0;
}
bool WorldSkeletalHero::load_unarmed_motion(const char*rig,std::string&e){
 unarmed_motion_available_=false;unarmed_motion_asset_={};reset_unarmed_motion();
 if(!full_unarmed_clips_)return true;
 const auto path=std::filesystem::path(rig).parent_path()/"unarmed_transitions.glb";
 std::error_code ec;if(!std::filesystem::exists(path,ec)){if(ec){e="Cannot inspect unarmed transition sidecar";return false;}return true;}
 Asset bank;if(!world_animation::load_glb(path.string().c_str(),bank,e))return false;
 const auto&a=asset_.skeleton;const auto&b=bank.skeleton;
 if(a.joints.size()!=b.joints.size()||a.palette!=b.palette||a.mesh_node!=b.mesh_node||a.inverse_bind.size()!=b.inverse_bind.size()){e="Unarmed transition skeleton differs";return false;}
 for(unsigned n=0;n<a.joints.size();++n){const auto&x=a.joints[n];const auto&y=b.joints[n];if(x.name!=y.name||x.parent!=y.parent||(std::memcmp(&x.rest,&y.rest,sizeof(Transform))&&!(grip_profile_==2&&x.name=="valwood_weapon_r"))){e="Unarmed transition canonical bind differs";return false;}if(x.name=="root")unarmed_motion_root_=n;}
 for(unsigned n=0;n<a.inverse_bind.size();++n)if(a.inverse_bind[n].m!=b.inverse_bind[n].m){e="Unarmed transition inverse bind differs";return false;}
 if(bank.clips.size()!=22){e="Incomplete unarmed transition bank";return false;}
 for(unsigned k=0;k<22;++k){unsigned found=0;for(unsigned n=0;n<bank.clips.size();++n)if(bank.clips[n].name==std::string("Unarmed_")+unarmed_motion_names[k]){unarmed_motion_captures_[k].clip=n;++found;}
  if(found!=1){e="Missing/ambiguous unarmed transition take";return false;}}
 unarmed_motion_asset_=std::move(bank);unarmed_motion_available_=true;
 // Preserve the source trajectory as a CPU table. No animation keys are added
 // to an existing combat bank or to its appearance-composed asset.
 for(unsigned k=0;k<22;++k){auto&c=unarmed_motion_captures_[k];c.trajectory.clear();const auto&clip=unarmed_motion_asset_.clips[c.clip];const unsigned count=unsigned(std::llround(clip.duration*30));double length=0,last_yaw=0;Vec3 previous;
  for(unsigned n=0;n<=count;++n){const double t=n==count?clip.duration:std::min(clip.duration,double(n)/30);Pose p;if(!sample_unarmed_motion(k,t,p,e,false))return false;
   const Vec3 root=vsub(origin(p.global[unarmed_motion_root_]),origin(rest_.global[unarmed_motion_root_]));
   const Quat q=qproduct(world_rotation(a,p,unarmed_motion_root_),qconjugate(world_rotation(a,rest_,unarmed_motion_root_)));
   const Vec3 f=qrotate(q,{0,0,1});double yaw=std::atan2(f.x,f.z);if(n)yaw=last_yaw+std::remainder(yaw-last_yaw,tau);last_yaw=yaw;
   if(n)length+=std::hypot(root.x-previous.x,root.z-previous.z);previous=root;c.trajectory.push_back({t,root.x,root.z,yaw,length});
  }
  c.angle=c.trajectory.back()[3];const auto&first=c.trajectory[0];const auto&second=c.trajectory[1];const auto&last=c.trajectory.back();const auto&penultimate=c.trajectory[c.trajectory.size()-2];
  c.begin_speed=(second[4]-first[4])/(second[0]-first[0]);c.end_speed=(last[4]-penultimate[4])/(last[0]-penultimate[0]);
 }
 return true;
}
std::array<double,5> WorldSkeletalHero::unarmed_trajectory(unsigned k,double t)const{
 const auto&v=unarmed_motion_captures_[k].trajectory;if(t<=0)return v.front();if(t>=v.back()[0])return v.back();
 const auto it=std::upper_bound(v.begin(),v.end(),t,[](double x,const auto&y){return x<y[0];});const auto&a=*(it-1);const auto&b=*it;const double f=(t-a[0])/(b[0]-a[0]);auto o=a;for(unsigned n=0;n<5;++n)o[n]+=(b[n]-a[n])*f;return o;
}
bool WorldSkeletalHero::sample_unarmed_motion(unsigned k,double t,Pose&p,std::string&e,bool strip)const{
 if(!world_animation::sample(asset_.skeleton,unarmed_motion_asset_.clips[unarmed_motion_captures_[k].clip],std::max(0.,t),false,p,e))return false;
 if(strip){p.local[unarmed_motion_root_]=asset_.skeleton.joints[unarmed_motion_root_].rest;return world_animation::evaluate(asset_.skeleton,p,e);}return true;
}
bool WorldSkeletalHero::initialize_unarmed_motion_feet(std::string&e){
 if(!unarmed_motion_available_)return true;
 const unsigned points[]={feet_[0].ankle,feet_[0].ball,feet_[1].ankle,feet_[1].ball,left_hand_,right_hand_,pelvis_};
 for(unsigned k=0;k<22;++k){auto&c=unarmed_motion_captures_[k];const double duration=c.trajectory.back()[0];std::vector<double> floor;std::array<std::array<float,128>,2> low={},vertical={},horizontal={};
  for(unsigned n=0;n<128;++n){const double t=duration*n/127,a=std::max(0.,t-1./120),b=std::min(duration,t+1./120);Pose p,pa,pb;
   if(!sample_unarmed_motion(k,t,p,e,false)||!sample_unarmed_motion(k,a,pa,e,false)||!sample_unarmed_motion(k,b,pb,e,false))return false;
   double minimum=1.e9;
   for(unsigned side=0;side<2;++side){Vec3 heel,toe,x,y;if(!sole_point(p,side,0,heel,e)||!sole_point(p,side,1,toe,e))return false;unsigned marker=toe.y<heel.y;const Vec3 q=marker?toe:heel;
    if(!sole_point(pa,side,marker,x,e)||!sole_point(pb,side,marker,y,e))return false;low[side][n]=q.y;vertical[side][n]=float(std::abs(y.y-x.y)/std::max(.0001,b-a));horizontal[side][n]=float(std::hypot(y.x-x.x,y.z-x.z)/std::max(.0001,b-a));minimum=std::min(minimum,double(q.y));
    if(n%8==0)for(unsigned vertex:feet_[side].vertices){Vec3 q;if(!world_attack_foot_contact::deform_position(asset_.mesh.vertices[vertex],p,q,e))return false;q=world_animation::transform_point(p.global[asset_.skeleton.mesh_node],q);minimum=std::min(minimum,double(q.y));}
   }floor.push_back(minimum);
  }
  std::sort(floor.begin(),floor.end());c.floor=-floor[floor.size()/20];if(std::abs(c.floor)>.30){e="Unarmed transition floor calibration exceeds bound";return false;}
  for(unsigned side=0;side<2;++side)for(unsigned n=0;n<128;++n)c.support[side][n]=smooth(float((.07-(low[side][n]+c.floor))/.045))*smooth((.65f-vertical[side][n])/.45f)*smooth((.65f-horizontal[side][n])/.45f);
  Pose end;if(!sample_unarmed_motion(k,duration,end,e))return false;
  for(unsigned gait=0;gait<2;++gait){double best=1.e20;for(unsigned n=0;n<128;++n){Pose loop;const auto&clip=asset_.clips[full_unarmed_clip_[gait+1]];if(!world_animation::sample(asset_.skeleton,clip,double(n)/128*clip.duration,true,loop,e))return false;const double cost=unarmed_pose_cost(end,loop,points,7);if(cost<best){best=cost;c.exit_phase[gait]=double(n)/128;}}}
 }
 return true;
}
unsigned WorldSkeletalHero::choose_unarmed_motion(unsigned first,unsigned count,std::string&e)const{
 if(!have_pose_)return first;const unsigned points[]={feet_[0].ankle,feet_[0].ball,feet_[1].ankle,feet_[1].ball,left_hand_,right_hand_,pelvis_};double best=1.e20;unsigned chosen=first;
 for(unsigned k=first;k<first+count;++k){const double time=(first==2||first==4)?unarmed_stop_entry(unarmed_motion_captures_[k].trajectory,motion_actual_speed_):0;Pose p;if(!sample_unarmed_motion(k,time,p,e))return first;const double cost=unarmed_pose_cost(pose_,p,points,7);if(cost<best){best=cost;chosen=k;}}return chosen;
}
bool WorldSkeletalHero::begin_unarmed_motion(unsigned mode,unsigned capture,double heading,double angle_scale,std::string&e){
 if(!e.empty())return false;const unsigned outgoing_mode=motion_mode_;motion_mode_=mode;motion_capture_=capture;unarmed_motion_time_=mode==3?unarmed_stop_entry(unarmed_motion_captures_[capture].trajectory,motion_actual_speed_):0;motion_capture_age_=0;motion_heading_=heading;motion_angle_scale_=angle_scale;motion_root_scale_=1;motion_entry_pose_=have_pose_?pose_:rest_;motion_entry_yaw_=yaw_;motion_entry_support_=stats_.foot_weight;motion_bridge_age_=.2;motion_blend_duration_=.12;
 if(!unarmed_pose_velocity(motion_previous_pose_,motion_entry_pose_,motion_previous_dt_,motion_previous_yaw_,motion_entry_yaw_,motion_entry_velocity_,e))return false;
 if(have_pose_){Pose first;if(!sample_unarmed_motion(capture,unarmed_motion_time_,first,e))return false;unsigned head=spine_;for(unsigned n=0;n<asset_.skeleton.joints.size();++n)if(asset_.skeleton.joints[n].name=="head")head=n;const double dy=std::abs(origin(first.global[head]).y-origin(pose_.global[head]).y);motion_blend_duration_=std::clamp(.12+dy*.5,.12,.32);}
 // A crouched moving pivot cannot be replaced by an upright stopping take
 // at an arbitrary short fixed duration. Bound the pose correction rate;
 // gameplay still brakes immediately along the same accepted stop trajectory.
 if(have_pose_&&outgoing_mode==5&&mode==3){Pose first;if(!sample_unarmed_motion(capture,0,first,e))return false;unsigned head=spine_;for(unsigned n=0;n<asset_.skeleton.joints.size();++n)if(asset_.skeleton.joints[n].name=="head")head=n;
  const double torso=std::max(vlength(vsub(origin(first.global[head]),origin(pose_.global[head]))),vlength(vsub(origin(first.global[pelvis_]),origin(pose_.global[pelvis_]))));double limb=0;for(unsigned n:{feet_[0].ankle,feet_[1].ankle,left_hand_,right_hand_})limb=std::max(limb,double(vlength(vsub(origin(first.global[n]),origin(pose_.global[n])))));
  motion_blend_duration_=std::clamp(std::max({motion_blend_duration_,1.5*torso/.8,1.5*limb/4.}),.12,.50);
 }
 if(mode==1)++stats_.captured_starts;else if(mode==3)++stats_.captured_stops;else if(mode==4)++stats_.captured_turns;else if(mode==5)++stats_.captured_pivots;return true;
}
bool WorldSkeletalHero::plan_unarmed_motion(double vx,double vz,float yaw,double dt,bool enabled,bool paused,MotionRequest&out,std::string&e){
 out={};out.facing_yaw=yaw;e.clear();
 if(!std::isfinite(vx)||!std::isfinite(vz)||!std::isfinite(yaw)||!std::isfinite(dt)||dt<0||dt>.1){e="Invalid captured locomotion request";return false;}
 if(!enabled||!unarmed_motion_available_||!loaded_){reset_unarmed_motion();return true;}
 motion_enabled_=true;out.captured=true;
 if(!motion_have_heading_){motion_facing_=yaw;motion_wish_yaw_=yaw;motion_have_heading_=true;}
 out.facing_yaw=float(motion_facing_);
 if(paused||dt==0){motion_planned_=false;return true;}
 const double speed=std::hypot(vx,vz),wish=speed>.001?std::atan2(vx,vz):motion_wish_yaw_;
 if(motion_blocked_&&(speed<.03||std::abs(std::remainder(wish-motion_wish_yaw_,tau))>.35))motion_blocked_=false;
 if(motion_blocked_){motion_last_wish_speed_=speed;return true;}
 // A camera/low-stick turn is presentation, not a movement lock. A fresh
 // walking request may leave it immediately from the currently displayed
 // pose rather than waiting for the complete 90/180-degree idle take.
 if(motion_mode_==4&&speed>.18)motion_mode_=0;
 // A spatially shortened walking start has not earned the full captured
 // stopping stride. Releasing it must cancel through the existing outgoing
 // pose bridge, even after its faster articulation crosses the old .8m/s
 // early-tap threshold; otherwise a brief turn queues another half metre.
 const bool shortened_walk_start=motion_mode_==1&&motion_root_scale_<.999999;
 if(motion_mode_==1&&speed<.03&&(motion_capture_age_<.25||motion_actual_speed_<.8||shortened_walk_start)){
  motion_mode_=0;motion_bridge_pose_=pose_;motion_entry_yaw_=yaw_;if(!unarmed_pose_velocity(motion_previous_pose_,pose_,motion_previous_dt_,motion_previous_yaw_,yaw_,motion_bridge_velocity_,e))return false;motion_bridge_age_=0;motion_bridge_duration_=.18;motion_entry_support_=stats_.foot_weight;motion_speed_cap_=0;
 }
 if(motion_mode_==0&&speed>.001&&speed<=.18&&std::abs(std::remainder(wish-motion_facing_,tau))>.35){
  const double angle=std::remainder(wish-motion_facing_,tau);const unsigned chosen=(angle<0?6:7)+(std::abs(angle)>2.?2:0);if(!begin_unarmed_motion(4,chosen,motion_facing_,angle/unarmed_motion_captures_[chosen].angle,e))return false;motion_rate_=std::clamp(std::abs(angle)/(tau/4),.65,1.5);motion_speed_cap_=0;
 }else if(motion_mode_==0&&speed>.03){
  const bool run=speed>2.2;const double angle=std::remainder(wish-motion_facing_,tau);unsigned capture=run?1:0;double heading=wish,scale=1;
  if(std::abs(angle)>.45){const unsigned first=run?18:14;capture=first;double best=1.e20;for(unsigned k=first;k<first+4;++k){const double d=std::abs(unarmed_motion_captures_[k].angle-angle);if(d<best){best=d;capture=k;}}heading=motion_facing_;scale=angle/unarmed_motion_captures_[capture].angle;}
  if(!begin_unarmed_motion(1,capture,heading,scale,e))return false;motion_rate_=speed/std::max(.5,unarmed_motion_captures_[capture].end_speed);motion_speed_cap_=speed;
 }else if(motion_mode_==2&&speed<.03){const unsigned first=motion_actual_speed_>2.2?4:2;const unsigned chosen=choose_unarmed_motion(first,2,e);if(!begin_unarmed_motion(3,chosen,motion_facing_,1,e))return false;motion_speed_cap_=motion_actual_speed_;motion_rate_=motion_actual_speed_/std::max(.5,unarmed_motion_captures_[chosen].begin_speed);}
 else if(motion_mode_==2&&speed>2.2&&std::abs(std::remainder(wish-motion_facing_,tau))>2.35){
  const double angle=std::remainder(wish-motion_facing_,tau);unsigned first=angle<0?10:12;const unsigned chosen=choose_unarmed_motion(first,2,e);if(!begin_unarmed_motion(5,chosen,motion_facing_,angle/unarmed_motion_captures_[chosen].angle,e))return false;motion_speed_cap_=motion_actual_speed_;motion_rate_=motion_actual_speed_/std::max(.5,unarmed_motion_captures_[chosen].begin_speed);motion_speed_cap_=speed;
 }else if((motion_mode_==1||motion_mode_==5)&&speed<.03){
  // A very short tap must not finish a long source acceleration after release.
  const unsigned first=motion_actual_speed_>2.2?4:2;const unsigned chosen=choose_unarmed_motion(first,2,e);const double rate=std::max(.35,motion_actual_speed_)/std::max(.5,unarmed_motion_captures_[chosen].begin_speed);if(!begin_unarmed_motion(3,chosen,motion_facing_,1,e))return false;motion_rate_=rate;motion_speed_cap_=motion_actual_speed_;
 }else if(motion_mode_==3&&speed>.03){
  const double angle=std::remainder(wish-motion_facing_,tau);
  if(motion_actual_speed_<.5){
   const bool run=speed>2.2;unsigned chosen=run?1:0;double heading=wish,scale=1;
   if(std::abs(angle)>.45){const unsigned first=run?18:14;double best=1.e20;for(unsigned k=first;k<first+4;++k){const double d=std::abs(unarmed_motion_captures_[k].angle-angle);if(d<best){best=d;chosen=k;}}heading=motion_facing_;scale=angle/unarmed_motion_captures_[chosen].angle;}
   if(!begin_unarmed_motion(1,chosen,heading,scale,e))return false;motion_rate_=speed/std::max(.5,unarmed_motion_captures_[chosen].end_speed);motion_speed_cap_=speed;
  }else if(speed>2.2&&std::abs(angle)>2.35){
   const unsigned chosen=choose_unarmed_motion(angle<0?10:12,2,e);if(!begin_unarmed_motion(5,chosen,motion_facing_,angle/unarmed_motion_captures_[chosen].angle,e))return false;motion_rate_=motion_actual_speed_/std::max(.5,unarmed_motion_captures_[chosen].begin_speed);motion_speed_cap_=speed;
  }else{
   // Resume the moving gait without an idle acceleration. Do not pre-steer
   // before deciding on a pivot: that made its threshold depend on frame dt.
   motion_mode_=2;motion_bridge_pose_=pose_;motion_entry_yaw_=yaw_;if(!unarmed_pose_velocity(motion_previous_pose_,pose_,motion_previous_dt_,motion_previous_yaw_,yaw_,motion_bridge_velocity_,e))return false;motion_bridge_age_=0;motion_speed_cap_=speed;
  }
 }
 else if(motion_mode_==0&&speed<.03&&std::abs(std::remainder(double(yaw)-motion_facing_,tau))>.35){
  const double angle=std::remainder(double(yaw)-motion_facing_,tau);const unsigned chosen=(angle<0?6:7)+(std::abs(angle)>2.?2:0);if(!begin_unarmed_motion(4,chosen,motion_facing_,angle/unarmed_motion_captures_[chosen].angle,e))return false;motion_rate_=std::clamp(std::abs(angle)/(tau/4),.65,1.5);motion_speed_cap_=0;
 }
 // Turning cadence must not be slowed by the apartment's walking-speed cap.
 // The right and left source starts travel very different distances; using
 // speed/end_speed for their entire clock made a right turn last nearly two
 // seconds at 1.25m/s. Play the articulation at a bounded human turn cadence
 // and shorten only its requested root travel. The regular sweep below is
 // still authoritative, and still caps every request to the walking speed.
 if(motion_capture_age_==0&&(motion_mode_==4||(motion_mode_==1&&motion_capture_>=14&&motion_capture_<18))){
  const auto&capture=unarmed_motion_captures_[motion_capture_];
  const double angle=std::abs(capture.angle*motion_angle_scale_);
  const double duration=std::clamp(.45+.10*angle,.50,.78);
  motion_rate_=std::max(motion_rate_,capture.trajectory.back()[0]/duration);
  if(motion_mode_==1){
   const double source_speed=std::max({.5,capture.end_speed,capture.trajectory.back()[4]/capture.trajectory.back()[0]});
   motion_root_scale_=std::min(1.,motion_speed_cap_/(source_speed*std::clamp(motion_rate_,.25,3.)));
  }
 }
 // Match the bounded authored stopping tail to the accepted incoming speed.
 // The same source clock drives root and pose; no root rescale or teleport.
 if(motion_mode_==3&&motion_capture_age_==0){
  const double source_speed=unarmed_stop_speed(unarmed_motion_captures_[motion_capture_].trajectory,unarmed_motion_time_);
  motion_rate_=std::clamp(motion_actual_speed_/std::max(.01,source_speed),.25,3.);
 }
 motion_plan_dt_=0;motion_plan_remaining_=dt;motion_next_time_=unarmed_motion_time_;motion_plan_x_=motion_plan_z_=0;motion_planned_=true;motion_acceptance_=1;
 if(motion_mode_==2){const double angle=std::remainder(wish-motion_facing_,tau);motion_facing_+=angle*(-std::expm1(-dt/.075));out.dx=vx*dt;out.dz=vz*dt;motion_plan_x_=out.dx;motion_plan_z_=out.dz;motion_speed_cap_=speed;}
 else if(motion_mode_!=0){
  const auto&c=unarmed_motion_captures_[motion_capture_];const auto a=unarmed_trajectory(motion_capture_,unarmed_motion_time_);double next=std::min(c.trajectory.back()[0],unarmed_motion_time_+dt*std::clamp(motion_rate_,.25,3.));
  const double limit=motion_speed_cap_*dt;
  auto displacement=[&](double t){const auto b=unarmed_trajectory(motion_capture_,t);return std::hypot(b[1]-a[1],b[2]-a[2])*motion_root_scale_;};
  if(displacement(next)>limit&&limit>0){double lo=unarmed_motion_time_,hi=next;for(unsigned n=0;n<24;++n){double m=(lo+hi)*.5;if(displacement(m)>limit)hi=m;else lo=m;}next=lo;}
  const auto b=unarmed_trajectory(motion_capture_,next);const double x=(b[1]-a[1])*motion_root_scale_,z=(b[2]-a[2])*motion_root_scale_,cs=std::cos(motion_heading_),sn=std::sin(motion_heading_);
  out.dx=cs*x+sn*z;out.dz=-sn*x+cs*z;
  if((motion_mode_==1||motion_mode_==5)&&next>=c.trajectory.back()[0]-1.e-7&&speed>.03){
   const double source_seconds=(next-unarmed_motion_time_)/std::clamp(motion_rate_,.25,3.);
   const double travel_seconds=motion_speed_cap_>0?std::hypot(out.dx,out.dz)/motion_speed_cap_:0;
   const double remaining=std::max(0.,dt-std::max(source_seconds,travel_seconds));
   out.dx+=vx*remaining;out.dz+=vz*remaining;
   // Reserved per-plan storage holds only this frame's accepted loop carry.
   // No extra key, source phase skip, stop travel or physics teleport occurs.
   motion_plan_dt_=speed*remaining;
  }
  motion_plan_x_=out.dx;motion_plan_z_=out.dz;motion_next_time_=next;motion_facing_=motion_heading_+b[3]*motion_angle_scale_;
 }
 out.facing_yaw=float(motion_facing_);motion_last_wish_speed_=speed;if(speed>.03)motion_wish_yaw_=wish;return true;
}
void WorldSkeletalHero::commit_unarmed_motion(const Input&i,double dt){
 if(!motion_enabled_||!motion_planned_||dt<=0)return;
 const double fraction=std::min(1.,dt/std::max(1.e-9,motion_plan_remaining_));
 const double px=motion_plan_x_*fraction,pz=motion_plan_z_*fraction,expected=std::hypot(px,pz);double ratio=1;
 if(expected>1.e-7)ratio=std::clamp((i.dx*px+i.dz*pz)/(expected*expected),0.,1.);
 motion_actual_speed_=std::hypot(i.dx,i.dz)/dt;motion_acceptance_=ratio;motion_plan_dt_*=ratio;unarmed_motion_time_+=(motion_next_time_-unarmed_motion_time_)*fraction*ratio;
 motion_plan_x_-=px;motion_plan_z_-=pz;motion_plan_remaining_=std::max(0.,motion_plan_remaining_-dt);
 if(ratio<.1&&expected>.0001)motion_blocked_time_+=dt;else motion_blocked_time_=0;
 if(motion_blocked_time_>.08){motion_mode_=0;motion_blocked_=true;motion_bridge_pose_=pose_;motion_entry_yaw_=yaw_;motion_bridge_velocity_={};motion_bridge_age_=0;motion_bridge_duration_=.18;motion_entry_support_=stats_.foot_weight;++stats_.captured_blocked;}
 if(motion_mode_!=0&&motion_mode_!=2){const auto tr=unarmed_trajectory(motion_capture_,unarmed_motion_time_);motion_facing_=motion_heading_+tr[3]*motion_angle_scale_;}
 if(motion_plan_remaining_<1.e-7)motion_planned_=false;
}
bool WorldSkeletalHero::unarmed_motion_pose(Pose&target,const Input&i,double dt,std::string&e){
 if(!motion_enabled_||!unarmed_motion_available_)return true;
 motion_capture_age_+=dt;motion_bridge_age_+=dt;
 const auto&s=asset_.skeleton;motion_support_override_=0;
 if(motion_mode_!=0&&motion_mode_!=2){
  Pose captured;if(!sample_unarmed_motion(motion_capture_,unarmed_motion_time_,captured,e))return false;const auto&c=unarmed_motion_captures_[motion_capture_];
  const int parent=s.joints[pelvis_].parent;Vec3 translation=vadd(origin(captured.global[pelvis_]),{0,float(c.floor),0});if(parent>=0&&!inverse_point(captured.global[unsigned(parent)],translation,translation)){e="Singular captured locomotion pelvis";return false;}captured.local[pelvis_].translation=translation;if(!world_animation::evaluate(s,captured,e))return false;
  rotate_unarmed_pose(s,unarmed_motion_root_,captured,std::remainder(motion_facing_-i.yaw,tau),e);if(!e.empty())return false;
  Pose entry=motion_entry_pose_;if(!continue_unarmed_pose(entry,motion_entry_velocity_,motion_capture_age_,motion_blend_duration_,e))return false;rotate_unarmed_pose(s,unarmed_motion_root_,entry,std::remainder(motion_entry_yaw_-i.yaw,tau),e);if(!e.empty())return false;
  const float weight=smooth(float(motion_capture_age_/motion_blend_duration_));motion_support_override_=1;if(!mixpose(s,entry,captured,weight,target,e))return false;
  const double f=std::clamp(unarmed_motion_time_/c.trajectory.back()[0]*127,0.,127.);const unsigned n=std::min(126u,unsigned(f));
  for(unsigned side=0;side<2;++side){const float confidence=c.support[side][n]+float(f-n)*(c.support[side][n+1]-c.support[side][n]);motion_current_support_[side]=motion_entry_support_[side]*(1-weight)+confidence*weight;}
  if(unarmed_motion_time_>=c.trajectory.back()[0]-1.e-6){
   const bool stopped=motion_mode_==3||motion_mode_==4;motion_mode_=stopped?0:2;motion_bridge_pose_=target;motion_entry_yaw_=i.yaw;motion_bridge_pending_=true;motion_bridge_age_=0;motion_bridge_duration_=.12;motion_entry_support_=motion_current_support_;
   if(!stopped){
    full_unarmed_cycles_=std::floor(full_unarmed_cycles_)+c.exit_phase[motion_speed_cap_>2.2?1:0]+motion_plan_dt_/full_unarmed_stride_[motion_speed_cap_>2.2?1:0];
    // The authoritative root already consumed this frame's accepted loop
    // remainder. Consume it visually too: keeping the source endpoint for
    // that whole frame would suppress the outgoing velocity before bridging.
    const double carry=motion_speed_cap_>0?motion_plan_dt_/motion_speed_cap_:0;
    const double sample_dt=std::min(1./120,c.trajectory.back()[0]);Pose previous,end;
    if(!sample_unarmed_motion(motion_capture_,c.trajectory.back()[0]-sample_dt,previous,e)||!sample_unarmed_motion(motion_capture_,c.trajectory.back()[0],end,e))return false;
    const double rate=std::min(std::clamp(motion_rate_,.25,3.),std::clamp(motion_speed_cap_/std::max(.0001,std::max(.5,c.end_speed)*motion_root_scale_),.25,3.));
    if(!unarmed_pose_velocity(previous,end,sample_dt/rate,0,0,motion_bridge_velocity_,e))return false;
    motion_bridge_pending_=false;motion_bridge_age_=carry;
    if(carry>0){Pose loop,entry=motion_bridge_pose_;if(!sample_unarmed_loop(loop,e)||!continue_unarmed_pose(entry,motion_bridge_velocity_,carry,motion_bridge_duration_,e)||!mixpose(s,entry,loop,smooth(float(carry/motion_bridge_duration_)),target,e))return false;motion_support_override_=1-smooth(float(carry/motion_bridge_duration_));}
   }
  }
 }else if(motion_bridge_age_<motion_bridge_duration_&&have_pose_){motion_support_override_=1-smooth(float(motion_bridge_age_/motion_bridge_duration_));motion_current_support_=motion_entry_support_;Pose entry=motion_bridge_pose_,mixed;if(!continue_unarmed_pose(entry,motion_bridge_velocity_,motion_bridge_age_,motion_bridge_duration_,e))return false;rotate_unarmed_pose(s,unarmed_motion_root_,entry,std::remainder(motion_entry_yaw_-i.yaw,tau),e);if(!e.empty()||!mixpose(s,entry,target,smooth(float(motion_bridge_age_/motion_bridge_duration_)),mixed,e))return false;target=std::move(mixed);}
 stats_.captured_motion_mode=motion_mode_;stats_.captured_motion_clip=motion_capture_;stats_.captured_motion_time=float(unarmed_motion_time_);stats_.captured_motion_duration=float(unarmed_motion_captures_[motion_capture_].trajectory.back()[0]);stats_.captured_motion_acceptance=float(motion_acceptance_);return true;
}

namespace world_attack_foot_contact {
// Position queries do not need an inverse-transpose normal. Retain the exact
// reference blend/point arithmetic and joint/weight/finite validation; generic
// render/normal deformation and current-ground support validation stay intact.
static bool deform_position(const world_animation::SkinVertex&vertex,const world_animation::Pose&pose,world_animation::Vec3&position,std::string&error){
 auto finite=[](float value){return std::isfinite(value)&&std::abs(value)<=1.e12f;};
 if(!finite(vertex.position.x)||!finite(vertex.position.y)||!finite(vertex.position.z)||!finite(vertex.normal.x)||!finite(vertex.normal.y)||!finite(vertex.normal.z)||pose.joints>world_animation::joint_limit){error="Invalid foot-position skin input";return false;}
 world_animation::Mat4 blended;blended.m.fill(0);float sum=0;
 for(unsigned k=0;k<4;++k){if(vertex.joints[k]>=pose.joints||!std::isfinite(vertex.weights[k])||vertex.weights[k]<0||vertex.weights[k]>1){error="Invalid foot-position skin joint/weight";return false;}sum+=vertex.weights[k];for(unsigned m=0;m<16;++m)blended.m[m]+=pose.skin[vertex.joints[k]].m[m]*vertex.weights[k];}
 if(std::abs(sum-1)>.002f){error="Invalid foot-position skin weight sum";return false;}
 const auto point=world_animation::transform_point(blended,vertex.position);
 if(!finite(point.x)||!finite(point.y)||!finite(point.z)){error="Foot-position skin overflow";return false;}position=point;return true;
}
}
// Captured contact intent only. Pure look-ahead never changes current playback,
// torso/weapon transforms or the physical capsule. The existing leg solver
// enforces unchanged segment lengths, ground slope and reachable anchors.
bool WorldSkeletalHero::attack_foot_contacts(const world_animation::Pose&result,const WorldTrainingCombat&fight,std::array<float,2>&contacts,std::string&e)const{
const double f=std::clamp(fight.player_time/std::max(.001,fight.player_duration),0.,1.);
  const double phase=fight.player_phase==TrainingPlayerPhase::Windup?.36*f:fight.player_phase==TrainingPlayerPhase::Strike?.36+.24*f:.60+.40*f;
  world_animation::Pose future[2];constexpr double step=.040;
  for(unsigned n=0;n<2;++n){const double next=world_attack_foot_contact::advance(phase,step*(n+1),windup_duration_,strike_duration_,recovery_duration_);if(!attack_pose(std::min(4u,unsigned(fight.attack_direction)),next,future[n],e))return false;}
  for(unsigned side=0;side<2;++side){float height=-100;double horizontal=0,vertical=0;const world_animation::Pose*poses[3]={&result,&future[0],&future[1]};
   Vec3 previous[2];for(unsigned n=0;n<3;++n){float bottom=std::numeric_limits<float>::infinity();for(unsigned vertex:feet_[side].vertices){Vec3 point;if(!world_attack_foot_contact::deform_position(asset_.mesh.vertices[vertex],*poses[n],point,e))return false;point=world_animation::transform_point(poses[n]->global[asset_.skeleton.mesh_node],point);bottom=std::min(bottom,point.y);}height=std::max(height,bottom);
    for(unsigned marker=0;marker<2;++marker){Vec3 point;if(!sole_point(*poses[n],side,marker,point,e))return false;if(n){horizontal=std::max(horizontal,std::hypot(double(point.x-previous[marker].x),double(point.z-previous[marker].z))/step);vertical=std::max(vertical,std::abs(double(point.y-previous[marker].y))/step);}previous[marker]=point;}
   }contacts[side]=world_attack_foot_contact::confidence(height,horizontal,vertical);
  }
 return true;
}

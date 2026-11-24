// Adapt the existing High capture's restrained lower-body weight transfer to
// stationary side/low cuts and thrusts. Only leg-chain rotations change; gameplay root,
// pelvis, torso and weapon retain the committed original attack exactly.
bool WorldSkeletalHero::stationary_cut_legs(unsigned direction,double phase,const world_animation::Pose&entry,world_animation::Pose&out,std::string&e)const{
 if(direction==2||direction>=5)return true;
 const double t=phase<.36?phase/.36:phase<.60?(phase-.36)/.24:(phase-.60)/.40;
 const double frame=phase<.36?31+13*smooth(float(t)):phase<.60?44+12*smooth(float(t)):56+8*smooth(float(t));
 world_animation::Pose captured,reference;if(!sample_attack(2,frame,captured,e))return false;
 const float enter=phase<.36?smooth(float(t)):1.f,release=phase>=.60?smooth(float(t)):0.f;
 // The reference is consumed only through leg locals and ancestor rotation
 // products. Blending/evaluating the whole skin twice here performed unused
 // upper-body FK and palette inversions. Preserve the same local arithmetic
 // and evaluator normalization for the required branches, without those FK.
 reference=entry;std::array<bool,world_animation::node_limit> needed={};
 for(unsigned n=0;n<out.nodes;++n)if(leg_mask_[n])for(int k=int(n);k>=0;k=asset_.skeleton.joints[unsigned(k)].parent)needed[unsigned(k)]=true;
 auto normalize=[](Quat&q){const float n=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;if(!std::isfinite(n)||n<1.e-16f)return false;const float f=1/std::sqrt(n);q={q.x*f,q.y*f,q.z*f,q.w*f};return true;};
 auto blend_local=[&](world_animation::Transform&o,const world_animation::Transform&v,float w){
  o.translation={o.translation.x+(v.translation.x-o.translation.x)*w,o.translation.y+(v.translation.y-o.translation.y)*w,o.translation.z+(v.translation.z-o.translation.z)*w};
  o.scale={o.scale.x+(v.scale.x-o.scale.x)*w,o.scale.y+(v.scale.y-o.scale.y)*w,o.scale.z+(v.scale.z-o.scale.z)*w};o.rotation=mixq(o.rotation,v.rotation,w);return normalize(o.rotation);
 };
 for(unsigned n=0;n<out.nodes;++n)if(needed[n]){
  if(!blend_local(reference.local[n],captured.local[n],enter)||(release>0&&!blend_local(reference.local[n],base_.local[n],release))){e="Invalid stance reference rotation";return false;}
 }
 // During the analytic solve only hip/knee/ankle globals are read. Update
 // precisely that chain between its two rotations, then evaluate the complete
 // pose/palette once after both legs. The unchanged two-bone construction and
 // matrix multiplication order match solve_leg and the general evaluator.
 auto evaluate_chains=[&](){
  // General evaluate normalizes every local quaternion even on unchanged
  // branches. Retain that exact rounding history so later arm/skin/contact
  // results are identical, while only computing FK actually read by this IK.
  for(unsigned n=0;n<out.nodes;++n)if(!normalize(out.local[n].rotation))return false;
  std::array<bool,world_animation::node_limit> done={};
  auto joint=[&](auto&&self,unsigned n)->bool{if(done[n])return true;const auto&t=out.local[n];const auto&q=t.rotation;const float x=q.x,y=q.y,z=q.z,w=q.w;Mat4 m;m.m={
   (1-2*y*y-2*z*z)*t.scale.x,(2*x*y+2*z*w)*t.scale.x,(2*x*z-2*y*w)*t.scale.x,0,
   (2*x*y-2*z*w)*t.scale.y,(1-2*x*x-2*z*z)*t.scale.y,(2*y*z+2*x*w)*t.scale.y,0,
   (2*x*z+2*y*w)*t.scale.z,(2*y*z-2*x*w)*t.scale.z,(1-2*x*x-2*y*y)*t.scale.z,0,
   t.translation.x,t.translation.y,t.translation.z,1};
   const int parent=asset_.skeleton.joints[n].parent;if(parent>=0){if(!self(self,unsigned(parent)))return false;Mat4 product;product.m.fill(0);const auto&a=out.global[unsigned(parent)];for(unsigned c=0;c<4;++c)for(unsigned r=0;r<4;++r)for(unsigned k=0;k<4;++k)product.m[c*4+r]+=a.m[k*4+r]*m.m[c*4+k];m=product;}
   for(float v:m.m)if(!std::isfinite(v)||std::abs(v)>1.e12f)return false;out.global[n]=m;done[n]=true;return true;
  };
  for(const auto&foot:feet_)if(!joint(joint,foot.ankle))return false;return true;
 };
 auto solve_stance_leg=[&](const FootSupport&f,Vec3 target,Quat rotation,bool final){
  const auto&s=asset_.skeleton;const Vec3 hip=origin(out.global[f.hip]),knee=origin(out.global[f.knee]),ankle=origin(out.global[f.ankle]);const float upper=vlength(vsub(knee,hip)),lower=vlength(vsub(ankle,knee)),distance=vlength(vsub(target,hip));
  if(distance>upper+lower-.000005f||distance<std::abs(upper-lower)+.000005f){e="Stance target outside unchanged leg reach";return false;}
  const Vec3 axis=vunit(vsub(target,hip));Vec3 pole=vsub(knee,hip);pole=vsub(pole,vmul(axis,vdot(pole,axis)));if(vlength(pole)<.00001f)pole=vcross(axis,{1,0,0});pole=vunit(pole);
  const float along=(upper*upper+distance*distance-lower*lower)/(2*distance),height=std::sqrt(std::max(0.f,upper*upper-along*along));const Vec3 goal=vadd(hip,vadd(vmul(axis,along),vmul(pole,height)));
  const Quat hip_world=qproduct(between(vsub(knee,hip),vsub(goal,hip)),world_rotation(s,out,f.hip));const int parent=s.joints[f.hip].parent;out.local[f.hip].rotation=qunit(parent<0?hip_world:qproduct(qconjugate(world_rotation(s,out,unsigned(parent))),hip_world));
  if(!evaluate_chains()){e="Invalid stance hip chain";return false;}
  const Vec3 knee_now=origin(out.global[f.knee]);const Quat knee_world=qproduct(between(vsub(origin(out.global[f.ankle]),knee_now),vsub(target,knee_now)),world_rotation(s,out,f.knee));out.local[f.knee].rotation=qunit(qproduct(qconjugate(world_rotation(s,out,f.hip)),knee_world));
  if(!evaluate_chains()){e="Invalid stance knee chain";return false;}
  out.local[f.ankle].rotation=qunit(qproduct(qconjugate(world_rotation(s,out,f.knee)),rotation));return final||evaluate_chains();
 };
 // The captured reference supplies knee bend and ankle orientation, with its
 // global leg orientation compensated against this attack's unchanged pelvis.
 if(!copy_legs(reference,1,out,e))return false;
 for(unsigned side=0;side<2;++side){const auto&f=feet_[side];const Vec3 start=origin(entry.global[f.ankle]),idle=origin(base_.global[f.ankle]),source=origin(captured.global[f.ankle]);
  Vec3 delta=vsub(source,idle);const double horizontal=std::hypot(double(delta.x),double(delta.z));constexpr double step_limit=.12;
  if(horizontal>step_limit){delta.x*=float(step_limit/horizontal);delta.z*=float(step_limit/horizontal);}
  delta.y=std::clamp(delta.y,0.f,.045f);
  if(side==1)delta.z=std::abs(delta.z); // The rear support steps toward the captured weight shift.
  Vec3 target=vadd(start,vmul(vsub(vadd(idle,delta),start),enter));target=vadd(target,vmul(vsub(idle,target),release));
  const Vec3 hip=origin(out.global[f.hip]),knee=origin(out.global[f.knee]),ankle=origin(out.global[f.ankle]);const double reach=vlength(vsub(knee,hip))+vlength(vsub(ankle,knee))-.00002;
  // Keep the chosen sole height when a twisting captured pelvis brings the
  // support close to full extension. A small reachable step is preferable to
  // stretching the leg or dragging a distant stripped-root capture in place.
  Vec3 offset=vsub(target,hip);const double desired_horizontal=std::hypot(double(offset.x),double(offset.z));
  if(side==0&&offset.y<0&&desired_horizontal<reach){const double highest_grounded_span=std::sqrt(std::max(0.,reach*reach-desired_horizontal*desired_horizontal));const float needed=float(hip.y-highest_grounded_span);if(needed>target.y)target.y+=std::min(.065f,needed-target.y);}
  offset=vsub(target,hip);const double y=offset.y,max_horizontal=std::sqrt(std::max(0.,reach*reach-y*y)),distance=std::hypot(double(offset.x),double(offset.z));
  if(distance>max_horizontal&&distance>1.e-8){target.x=hip.x+offset.x*float(max_horizontal/distance);target.z=hip.z+offset.z*float(max_horizontal/distance);}
  const double length=vlength(vsub(target,hip));if(length>=reach){const Vec3 axis=vunit(vsub(target,hip));target=vadd(hip,vmul(axis,float(reach-.00001)));}
  const Quat idle_rotation=world_rotation(asset_.skeleton,base_,f.ankle);Vec3 pivot=qlog(qproduct(world_rotation(asset_.skeleton,captured,f.ankle),qconjugate(idle_rotation)));
  pivot.x=std::clamp(pivot.x,-.08f,.08f);pivot.y=std::clamp(pivot.y,-.22f,.22f);pivot.z=std::clamp(pivot.z,-.08f,.08f);
  Quat rotation=mixq(world_rotation(asset_.skeleton,entry,f.ankle),qunit(qproduct(qexp(pivot),idle_rotation)),enter);rotation=mixq(rotation,idle_rotation,release);
  if(!solve_stance_leg(f,target,rotation,side==1))return false;
 }
 return world_animation::evaluate(asset_.skeleton,out,e);
}

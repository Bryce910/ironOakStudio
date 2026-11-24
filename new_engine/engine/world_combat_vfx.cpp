#include "engine/world_combat_vfx.h"
#include <algorithm>
#include <cmath>
namespace world_combat_vfx {
namespace {
constexpr float trail_lifetime=.10f;
Point add(Point a,Point b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Point sub(Point a,Point b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Point scale(Point a,double t){return {a.x*t,a.y*t,a.z*t};}
double dot(Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Point closest(Blade blade,Point target){const auto d=sub(blade.tip,blade.base);return add(blade.base,scale(d,std::clamp(dot(sub(target,blade.base),d)/std::max(1e-8,dot(d,d)),0.,1.)));}
bool finite(Point p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<1e8&&std::abs(p.y)<1e7&&std::abs(p.z)<1e8;}
Point horizontal(Point from,Point to){auto d=sub(to,from);d.y=0;const double length=std::sqrt(dot(d,d));return length>.00001?scale(d,1/length):Point{0,0,1};}
Point body_surface(const Input &in,unsigned defender){
 if(in.body_contacts[defender].valid)return in.body_contacts[defender].point;
 const auto feet=in.feet[defender],chest=add(feet,{0,1.25,0});
 const auto &blade=in.blades[1-defender];const auto near=blade.valid?closest(blade,chest):add(chest,horizontal(feet,in.feet[1-defender]));
 auto side=horizontal(chest,near);
 if(std::hypot(near.x-chest.x,near.z-chest.z)<.01)side=horizontal(feet,in.feet[1-defender]);
 // The simulator resolves damage against a body, not against the old windup
 // blade pose. Pin the blood to a visible torso capsule instead of allowing
 // a far-away sword socket to leave it floating in empty space.
 return {feet.x+side.x*.34,std::clamp(near.y,feet.y+.65,feet.y+1.48),feet.z+side.z*.34};
}
}
void System::clear(){particles_={};trails_={};previous_={};swing_seen_={};event_seen_=0;seen_={};combat_clock_=-1;impulse_age_=10;impulse_strength_=0;stats_.active_particles=stats_.active_trails=0;stats_.last_material=Material::None;stats_.last_contact={};stats_.last_body_contacts[0]=stats_.last_body_contacts[1]={};}
float System::random(){random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;return float(random_>>8)*(1.f/16777216.f);}
void System::burst(Point point,unsigned kind,unsigned count,Point outward,float strength,double floor_y){
 for(unsigned n=0;n<count;++n){auto at=std::find_if(particles_.begin(),particles_.end(),[](const Particle &p){return p.life<=0;});if(at==particles_.end()){stats_.dropped+=count-n;break;}
  auto &p=*at;p={};p.p=point;p.kind=kind;const float a=random()*6.2831853f,spread=.25f+random();
  if(kind==1){p.life=.32f+random()*.5f;p.size=.10f+random()*.12f;p.v={std::cos(a)*spread*.8,.3+random()*.5,std::sin(a)*spread*.8};p.color={.24f,.18f,.12f};}
  else if(kind==3){
   const bool mist=n<2,clot=n>=2&&n<10;p.kind=mist?4:clot?5:3;p.floor_y=floor_y;
   const float weight=std::sqrt(strength);
   p.life=mist?.08f+random()*(blood_mist_lifetime-.08f):clot?.46f+random()*(blood_particle_lifetime-.46f):.34f+random()*.22f;
   p.size=(mist?.07f+random()*.04f:clot?.022f+random()*.018f:.010f+random()*.014f)*weight;
   const float speed=(mist?.10f+random()*.25f:clot?1.1f+random()*1.1f:1.8f+random()*2.f)*weight;
   const Point side{outward.z,0,-outward.x};
   p.v=add(scale(outward,speed),scale(side,(random()-.5)*speed*.65));
   // Blood leaves the cut sideways, then falls ballistically. The short mist
   // impulse cannot become a rising cloud and clots never hover at the floor.
   p.v.y=std::clamp(p.v.y,-.8,.25)+(mist?.015:.04)+random()*(mist?.06:.24);
   p.color={clot?.10f+random()*.07f:.18f+random()*.10f,.002f+random()*.002f,.002f+random()*.0015f};
  }
  else{p.life=.12f+random()*(kind==2?.15f:.30f);p.size=kind==2?.15f:.009f+random()*.018f;const float speed=kind==2?0:1.8f+random()*3.8f;p.v={std::cos(a)*spread*speed,(.3+random())*speed,std::sin(a)*spread*speed};p.color=kind==2?std::array<float,3>{7.f,6.2f,4.5f}:std::array<float,3>{6.f,3.6f+random()*1.2f,.95f};}
 }
}
void System::trail(unsigned actor,Blade blade,bool overlay){
 auto &old=previous_[actor];if(!old.valid){old=blade;return;}const auto movement=sub(blade.tip,old.tip);const double d=dot(movement,movement);if(d>.0002&&d<4){auto at=std::find_if(trails_.begin(),trails_.end(),[](const Trail &t){return !t.active;});if(at!=trails_.end()){*at={old,blade,0,actor+(overlay?16u:0u),true};stats_.trail_segments++;}else stats_.dropped++;}old=blade;
}
void System::update(const WorldTrainingCombat &combat,const Input &input,double delta){
 if(!std::isfinite(delta)||delta<0)return;
 if(input.contact_count>input.contacts.size())return;
 for(unsigned i=0;i<input.contact_count;++i){const auto &e=input.contacts[i];if(!e.serial||e.kind>4||!finite(e.point)||!finite(e.feet)||!finite(e.direction)||!std::isfinite(e.strength)||e.strength<.25f||e.strength>2.f)return;}
 for(const auto &t:input.actor_trails)if(t.blade.valid&&(!finite(t.blade.base)||!finite(t.blade.tip)))return;
 for(unsigned who=0;who<2;++who)if(!finite(input.feet[who])||(input.blades[who].valid&&(!finite(input.blades[who].base)||!finite(input.blades[who].tip)))||(input.trail_blades[who].valid&&(!finite(input.trail_blades[who].base)||!finite(input.trail_blades[who].tip)))||(input.body_contacts[who].valid&&!finite(input.body_contacts[who].point))||!finite(input.body_directions[who])||!std::isfinite(input.body_strength[who])||input.body_strength[who]<.25f||input.body_strength[who]>2.f)return;
 const std::array<unsigned,6> current={combat.hits,combat.parries,combat.blocks,combat.enemy_hits,combat.guarded,combat.dodges};
 if(combat.clock<combat_clock_||combat.hits<seen_[0]||combat.parries<seen_[1])clear();
 combat_clock_=combat.clock;paused_=input.paused;if(input.paused)return;const float dt=float(std::min(delta,.1));impulse_age_+=dt;
 for(auto &p:particles_)if(p.life>0){p.age+=dt;if(p.age>=p.life){p.life=0;continue;}p.v.y-=(p.kind==1?.1f:p.kind==4?3.f:p.kind>=3?blood_gravity:8.f)*dt;p.p=add(p.p,scale(p.v,dt));if(p.kind>=3&&p.p.y-p.size<=p.floor_y){p.life=0;++stats_.grounded_blood_particles;continue;}if(p.kind==1||p.kind==4){p.size+=dt*(p.kind==4?.02f:.18f);p.v=scale(p.v,std::exp(-dt*(p.kind==4?6.f:2.f)));}}
 for(auto &t:trails_)if(t.active){t.age+=dt;if(t.age>=trail_lifetime)t.active=false;}
 const auto chest0=add(input.feet[0],{0,1.25,0}),chest1=add(input.feet[1],{0,1.25,0});
 Point contact=scale(add(chest0,chest1),.5);
 if(input.blades[0].valid&&input.blades[1].valid){const auto a=closest(input.blades[0],input.blades[1].base),b=closest(input.blades[1],a);contact=scale(add(closest(input.blades[0],b),b),.5);}
 auto body_direction=[&](unsigned defender){const auto d=input.body_directions[defender];const auto length=std::sqrt(dot(d,d));return length>1e-5?scale(d,1/length):horizontal(input.feet[defender],input.feet[1-defender]);};
 if(input.resolved_events){
  for(unsigned n=0;n<input.contact_count;++n){const auto &e=input.contacts[n];if(e.serial<=event_seen_)continue;event_seen_=e.serial;
   if(e.kind==0||e.kind==3){const bool incoming=e.kind==3;const unsigned defender=incoming?0:1;
    const auto direction=std::hypot(e.direction.x,e.direction.y,e.direction.z)>1.e-6?e.direction:Point{0,0,1};
    burst(e.point,3,incoming?(e.finisher?incoming_finisher_particles:incoming_body_particles):(e.finisher?outgoing_finisher_particles:outgoing_body_particles),direction,e.strength,e.feet.y+.02);
    if(incoming){++stats_.incoming;impulse_strength_=.008f;}else{++stats_.hits;burst(add(e.feet,{0,.08,0}),1,7);++stats_.dust_bursts;impulse_strength_=.0032f;}
    ++stats_.body_bursts;stats_.finisher_bursts+=e.finisher;stats_.last_material=Material::Body;stats_.last_contact=stats_.last_body_contact=stats_.last_body_contacts[defender]=e.point;
   }else{const bool parry=e.kind==1;burst(e.point,0,parry?38:18);if(parry){burst(e.point,2,1);++stats_.parries;}else ++stats_.blocks;
    ++stats_.metal_bursts;stats_.last_material=Material::Metal;stats_.last_contact=stats_.last_metal_contact=e.point;impulse_strength_=parry?.0065f:.0032f;
   }impulse_age_=0;
  }
  if(current[5]>seen_[5]){auto point=input.feet[0];point.y+=.07;burst(point,1,14);++stats_.dust_bursts;}
 }else for(unsigned i=0;i<current.size();++i)if(current[i]>seen_[i]){
  if(i==0){const auto hit=body_surface(input,1);burst(hit,3,input.body_finisher[1]?outgoing_finisher_particles:outgoing_body_particles,body_direction(1),input.body_strength[1],input.feet[1].y+.02);burst(add(input.feet[1],{0,.08,0}),1,7);stats_.hits++;stats_.body_bursts++;stats_.finisher_bursts+=input.body_finisher[1];stats_.last_material=Material::Body;stats_.last_contact=stats_.last_body_contact=stats_.last_body_contacts[1]=hit;stats_.dust_bursts++;impulse_strength_=.0032f;}
  else if(i==1){burst(contact,0,38);burst(contact,2,1);stats_.parries++;stats_.metal_bursts++;stats_.last_material=Material::Metal;stats_.last_contact=stats_.last_metal_contact=contact;impulse_strength_=.0065f;}
  else if(i==2||i==4){burst(contact,0,18);stats_.blocks++;stats_.metal_bursts++;stats_.last_material=Material::Metal;stats_.last_contact=stats_.last_metal_contact=contact;impulse_strength_=.0032f;}
  else if(i==3){const auto hit=body_surface(input,0);burst(hit,3,input.body_finisher[0]?incoming_finisher_particles:incoming_body_particles,body_direction(0),input.body_strength[0],input.feet[0].y+.02);stats_.incoming++;stats_.body_bursts++;stats_.finisher_bursts+=input.body_finisher[0];stats_.last_material=Material::Body;stats_.last_contact=stats_.last_body_contact=stats_.last_body_contacts[0]=hit;impulse_strength_=.008f;}
  else{auto point=input.feet[0];point.y+=.07;burst(point,1,14);stats_.dust_bursts++;impulse_strength_=0;}
  impulse_age_=0;
 }
 seen_=current;
 const bool player_active=combat.engaged()&&(combat.player_phase==TrainingPlayerPhase::Strike||(combat.player_phase==TrainingPlayerPhase::Windup&&combat.player_time>combat.player_duration*.62));
 const bool enemy_active=combat.engaged()&&(combat.enemy_phase==TrainingEnemyPhase::Strike||(combat.enemy_phase==TrainingEnemyPhase::Windup&&combat.enemy_time>combat.enemy_duration*.78));
 if(input.resolved_events){for(unsigned actor=0;actor<input.actor_trails.size();++actor){const auto &t=input.actor_trails[actor];if(t.swing!=swing_seen_[actor]){previous_[actor].valid=false;swing_seen_[actor]=t.swing;}if(t.active&&t.blade.valid)trail(actor,t.blade,input.first_person&&actor==0);else previous_[actor].valid=false;}}
 else {if(input.attack_serial!=swing_seen_[0]){previous_[0].valid=false;swing_seen_[0]=input.attack_serial;}
  for(unsigned actor=0;actor<2;++actor){if((actor?enemy_active:player_active)&&input.blades[actor].valid)trail(actor,input.trail_blades[actor].valid?input.trail_blades[actor]:input.blades[actor],input.first_person&&actor==0);else previous_[actor].valid=false;}}
 stats_.active_particles=stats_.active_trails=0;for(const auto &p:particles_)stats_.active_particles+=p.life>0;for(const auto &t:trails_)stats_.active_trails+=t.active;
}
float System::camera_impulse()const{return paused_?0:impulse_strength_*std::exp(-impulse_age_*24)*std::sin(impulse_age_*95);}
void System::stage(Point camera,const float right[3],const float up[3],Stage &out)const{
 out.count=out.particles=out.trails=0;if(!finite(camera))return;for(unsigned i=0;i<3;++i)if(!std::isfinite(right[i])||!std::isfinite(up[i]))return;
 auto put=[&](const std::array<Point,4> &corners,std::array<float,4> color,float style){if(out.count==out.instances.size())return;auto &i=out.instances[out.count++];for(unsigned c=0;c<4;++c)i.corners[c]={float(corners[c].x-camera.x),float(corners[c].y-camera.y),float(corners[c].z-camera.z),0};i.color=color;i.style={style,0,0,0};};
 const Point r{right[0],right[1],right[2]},u{up[0],up[1],up[2]};
 for(const auto &p:particles_)if(p.life>0){const bool blood=p.kind>=3;const float t=p.age/p.life,alpha=(1-t)*(1-t)*(p.kind==1?.32f:p.kind==4?.38f:.95f);Point x=scale(r,p.size),y=scale(u,p.size);if(p.kind==0||p.kind==3||p.kind==5){const double vx=dot(p.v,r),vy=dot(p.v,u),length=std::hypot(vx,vy);if(length>1e-5){const Point axis=add(scale(r,vx/length),scale(u,vy/length)),side=add(scale(r,-vy/length),scale(u,vx/length));x=scale(side,p.size*(p.kind==5?.9:blood?.7:.55));y=scale(axis,p.size+std::min(blood?.055:.085,length*.016));}}put({add(sub(p.p,x),scale(y,-1)),add(add(p.p,x),scale(y,-1)),add(add(p.p,x),y),add(sub(p.p,x),y)},{p.color[0],p.color[1],p.color[2],alpha},p.kind==4?5:blood?4:p.kind==1?1:0);out.particles++;}
 for(const auto &t:trails_)if(t.active){const float alpha=.055f*(1-t.age/trail_lifetime);const auto from=add(t.from.base,scale(sub(t.from.tip,t.from.base),.25)),to=add(t.to.base,scale(sub(t.to.tip,t.to.base),.25));put({from,t.from.tip,t.to.tip,to},{.72f,.74f,.76f,alpha},(t.actor&16)?3:2);out.trails++;}
}
}

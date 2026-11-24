#ifndef VALWOOD_WORLD_CHARACTER_FACE_H
#define VALWOOD_WORLD_CHARACTER_FACE_H
#include "engine/world_animation.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

// Authored Thomas facial controls only. This controller supplies blend weights;
// it never scales a face, moves the skeleton, or changes collision/gameplay.
namespace world_character_face {
constexpr unsigned control_count=15;
inline const std::array<const char *,control_count> &names(){
 static const std::array<const char *,control_count> value={"EyeBlinkLeft","eyeBlinkRight","eyeLookDownLeft","eyeLookInLeft","eyeLookOutLeft","eyeLookUpLeft","eyeLookDownRight","eyeLookInRight","eyeLookOutRight","eyeLookUpRight","mouthSmileLeft","mouthSmileRight","browDownLeft","browDownRight","browInnerUp"};return value;
}
struct Input {
 double dt=0;float speed=0,turn=0,effort=0,pleasant=0;
 bool dead=false,swimming=false;
};
struct State {
 std::uint32_t seed=0x54484f4du;double clock=0,next_blink=2.8,blink_start=-1,next_gaze=1.1;
 float gaze_x=0,gaze_y=0,target_x=0,target_y=0,focus=0,smile=0,dead_close=0;
 bool bound=false,enabled=false,was_dead=false;
 std::array<float,world_animation::morph_limit> weights={};
};
inline float random(State &s){s.seed=s.seed*1664525u+1013904223u;return float(s.seed>>8)*(1.f/16777216.f);}
inline bool supported(const world_animation::Asset &asset){if(asset.morph_targets.size()!=control_count)return false;for(unsigned i=0;i<control_count;++i)if(asset.morph_targets[i].name!=names()[i])return false;return true;}
inline void reset(State &s,std::uint32_t seed=0x54484f4du){s=State{};s.seed=seed?seed:0x54484f4du;s.next_blink=2.2+random(s)*2.2;s.next_gaze=.65+random(s)*.9;}
inline float smooth(float x){x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);}
inline void update(const world_animation::Asset &asset,State &s,const Input &in,world_animation::Pose &pose){
 if(!s.bound){s.enabled=supported(asset);s.bound=true;}
 if(!s.enabled){pose.morph_weights.fill(0);return;}
 // Paused/zero-delta calls reproduce the committed expression exactly. Invalid
 // input is similarly held, and cannot poison subsequent finite weights.
 if(!std::isfinite(in.dt)||in.dt<0||!std::isfinite(in.speed)||!std::isfinite(in.turn)||!std::isfinite(in.effort)||!std::isfinite(in.pleasant)){pose.morph_weights=s.weights;return;}
 if(in.dt==0){pose.morph_weights=s.weights;return;}
 const float dt=float(std::min(in.dt,.25));s.clock+=dt;
 const float follow=1-std::exp(-dt*12),expression=1-std::exp(-dt*5);
 if(in.dead){
  s.was_dead=true;s.dead_close+=(1-s.dead_close)*(1-std::exp(-dt*15));s.gaze_x+=(0-s.gaze_x)*follow;s.gaze_y+=(0-s.gaze_y)*follow;s.focus+=(0-s.focus)*expression;s.smile+=(0-s.smile)*expression;
 }else{
  if(s.was_dead){const auto seed=s.seed;reset(s,seed);s.bound=true;s.enabled=true;}
  s.dead_close=0;
  if(s.clock>=s.next_gaze){
   // Infrequent small eye saccades, with a restrained anticipation of turns.
   // No camera orbit tracking: targets are independent of camera position.
   s.target_x=(random(s)*2-1)*.13f;s.target_y=(random(s)*2-1)*.07f;
   s.next_gaze=s.clock+.8+random(s)*1.7;
  }
  const float turn=std::clamp(in.turn,-1.f,1.f)*.10f;
  s.gaze_x+=(std::clamp(s.target_x+turn,-.22f,.22f)-s.gaze_x)*follow;
  s.gaze_y+=(s.target_y-s.gaze_y)*follow;
  s.focus+=(std::clamp(in.effort+(std::max(0.f,in.speed)-2.f)*.1f+(in.swimming?.2f:0.f),0.f,1.f)-s.focus)*expression;
  // A smile is an explicit contextual input, never an idle loop or breathing.
  s.smile+=(std::clamp(in.pleasant,0.f,1.f)*.22f-s.smile)*expression;
  if(s.clock>=s.next_blink){s.blink_start=s.clock;s.next_blink=s.clock+2.6+random(s)*3.4;}
 }
 float blink=0;
 if(s.blink_start>=0&&!in.dead){const double age=s.clock-s.blink_start;const float close=.075f,hold=.025f,open=.135f;if(age<close)blink=smooth(float(age)/close);else if(age<close+hold)blink=1;else if(age<close+hold+open)blink=1-smooth(float(age-close-hold)/open);else s.blink_start=-1;}
 if(in.dead)blink=s.dead_close;
 const float gaze_scale=in.dead?0.f:(1-blink)*(1-s.dead_close);
 s.weights.fill(0);s.weights[0]=s.weights[1]=std::clamp(blink,0.f,1.f);
 const float x=s.gaze_x*gaze_scale,y=s.gaze_y*gaze_scale;
 if(x>=0){s.weights[4]=x;s.weights[7]=x;}else{s.weights[3]=-x;s.weights[8]=-x;}
 if(y>=0){s.weights[5]=y;s.weights[9]=y;}else{s.weights[2]=-y;s.weights[6]=-y;}
 // The purchased blink keys alone leave a tiny corneal/iris sliver at full
 // closure. Measured source eye-look-down shapes complete lid coverage without
 // scaling/editing geometry. The correction is zero when eyes are open.
 const float closure_down=.35f*blink*blink*blink;s.weights[2]+=closure_down;s.weights[6]+=closure_down;
 s.weights[10]=s.weights[11]=s.smile*(1-s.dead_close);s.weights[12]=s.weights[13]=s.focus*.16f*(1-s.dead_close);
 // Authored inner-brow response is deliberately restrained, tied to exertion.
 s.weights[14]=s.focus*.025f*(1-s.dead_close);
 pose.morph_weights=s.weights;
}
}
#endif

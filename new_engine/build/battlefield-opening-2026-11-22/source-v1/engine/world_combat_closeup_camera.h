#ifndef VALWOOD_WORLD_COMBAT_CLOSEUP_CAMERA_H
#define VALWOOD_WORLD_COMBAT_CLOSEUP_CAMERA_H
#include <algorithm>
#include <cmath>

// Rendering-only framing. No logical camera, aim, actor position or combat
// state enters or leaves this helper. Existing camera sweeps own placement.
struct WorldCombatCloseupFrame {float weight=0,boom=4.2f,lift=0,shoulder=1.04f,target_drop=0;};
class WorldCombatCloseupCamera {
public:
 WorldCombatCloseupFrame update(double distance,float dt,bool active,bool cut,unsigned nearby_opponents=1){
  const bool valid=std::isfinite(distance)&&std::isfinite(dt)&&dt>=0;
  float target=0;
  if(active&&valid){const float t=std::clamp(float((distance-1.3)/2.),0.f,1.f);target=1-t*t*(3-2*t);}
  if(!active||!valid){weight_=crowd_=0;return {};}
  const float crowd_target=float(std::clamp(nearby_opponents,1u,3u)-1);
  if(cut||!initialized_)crowd_=crowd_target;else crowd_+=(crowd_target-crowd_)*(1-std::exp(-std::clamp(dt,0.f,.1f)/.22f));
  if(cut||!initialized_)weight_=target;
  else weight_+=(target-weight_)*(1-std::exp(-std::clamp(dt,0.f,.1f)/.18f));
  initialized_=true;weight_=std::clamp(weight_,0.f,1.f);
  // Exploration sits closer to the hero. Contact opens the boom back out,
  // with additional room for nearby opponents. Both directions use the same
  // damped weight; collision sweeps can still retract immediately. Keep the
  // accepted combat shoulder, boots/weapon framing and logical aim unchanged.
  return {weight_,3.35f+(.50f+.175f*crowd_)*weight_,(.2f+.06f*crowd_)*weight_,1.04f+1.26f*weight_,.48f*weight_};
 }
 void reset(){weight_=crowd_=0;initialized_=false;}
private:
 float weight_=0,crowd_=0;bool initialized_=false;
};

// Captions occupy the free top-center HUD column, leaving the small impact
// glyph at the reticle. Narrow windows use a row below the top-left panel.
struct WorldCombatNoticeLayout {float center_x=0,caption_y=0;};
inline WorldCombatNoticeLayout world_combat_notice_layout(float width,float height,float scale,float plate,unsigned row){
 const float center=width*.5f;
 const bool top_column=center-plate*.5f>=606*scale&&center+plate*.5f<=width-316*scale;
 const float y=(top_column?30.f:220.f)*scale+72.f*scale*std::min(row,1u);
 return {center,std::min(y,std::max(12.f,height-80*scale))};
}
#endif

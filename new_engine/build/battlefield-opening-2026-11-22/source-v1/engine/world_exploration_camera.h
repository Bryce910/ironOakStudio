#ifndef VALWOOD_WORLD_EXPLORATION_CAMERA_H
#define VALWOOD_WORLD_EXPLORATION_CAMERA_H
#include <algorithm>
#include <cmath>

// Render-only exploration framing. Combat supplies its existing fallback
// boom; physical movement, aim, and the player's first/third-person choice
// remain with the host. The host still sweeps both the boom and shoulder.
struct WorldExplorationCameraFrame {float boom=4.2f,pivot_drop=0,shoulder=0;};
class WorldExplorationCamera {
public:
 WorldExplorationCameraFrame update(bool exploring,bool indoors,float combat_boom,float dt,bool cut){
  const bool valid=std::isfinite(combat_boom)&&std::isfinite(dt)&&dt>=0;
  const WorldExplorationCameraFrame target=exploring&&valid?
      WorldExplorationCameraFrame{indoors?2.25f:2.95f,.55f,indoors?.35f:.45f}:
      WorldExplorationCameraFrame{valid?std::clamp(combat_boom,1.f,4.2f):4.2f,0,0};
  if(cut||!initialized_||!valid)frame_=target;
  else{
   const float t=1-std::exp(-std::clamp(dt,0.f,.1f)/.20f);
   // Combat keeps its accepted, already damped distance profile. The boom
   // helper itself eases any extension from the closer exploration view.
   frame_.boom=exploring?frame_.boom+(target.boom-frame_.boom)*t:target.boom;
   frame_.pivot_drop+=(target.pivot_drop-frame_.pivot_drop)*t;
   frame_.shoulder+=(target.shoulder-frame_.shoulder)*t;
  }
  initialized_=true;return frame_;
 }
 void reset(){frame_={};initialized_=false;}
private:
 WorldExplorationCameraFrame frame_{};bool initialized_=false;
};
#endif

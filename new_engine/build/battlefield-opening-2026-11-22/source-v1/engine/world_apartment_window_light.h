#ifndef VALWOOD_WORLD_APARTMENT_WINDOW_LIGHT_H
#define VALWOOD_WORLD_APARTMENT_WINDOW_LIGHT_H
#include <array>
#include <algorithm>
#include <cmath>
#include "engine/world_studio.h"

// Calibrate the seven original Finward window sources. The luminous panes and
// combined Unity bake are not substitutes for incident, shadowed window light.
// Match the imported fixtures only: newly placed/moved lights retain the
// designer's exact values. New scenes store calibrated values in world.studio;
// incident() also supports older snapshots without rewriting their documents.
// Electrical circuits do not own these daylight keys.
namespace world_apartment_window_light {
// 0 = authored daylight keys; 1 = apartment night. Owned by the opening
// placement (scoped to the apartment, restored on every outdoor handoff).
inline float night=0.f;
struct Portal {double x,z;std::array<double,3> socket;bool east;std::array<float,4> bounds;};
inline constexpr std::array<Portal,7> portals={{
 {-3510.861000061,-11004.505000114,{-3510.669873981478,601.904036037910,-11006.213735501857},true,{-3511.2f,-11009.2f,-3506.236f,-11001.919f}},
 {-3498.389000058,-11000.181999996,{-3498.388999938966,602.159845583427,-11002.038628134222},false,{-3499.662f,-11008.172f,-3496.462f,-10998.172f}},
 {-3486.128000259,-11000.209000006,{-3486.114999771119,602.077845566261,-11002.035628232450},false,{-3486.780f,-11007.988f,-3482.080f,-10997.988f}},
 {-3482.964000702,-11000.098999999,{-3482.913000106813,602.077845566261,-11002.035705870405},false,{-3486.780f,-11007.988f,-3482.080f,-10997.988f}},
 {-3479.444000244,-10999.183000028,{-3479.451449394227,602.117845797494,-11001.034027732905},false,{-3481.810f,-11008.750f,-3477.210f,-10996.800f}},
 {-3474.870000839,-10999.183000028,{-3474.867099761964,602.036845707849,-11001.038527764376},false,{-3476.780f,-11008.540f,-3472.780f,-10996.800f}},
 {-3470.593999863,-10999.183000028,{-3470.590002059938,601.975845599130,-11001.042027808722},false,{-3472.106f,-11006.647f,-3470.106f,-10996.800f}}
}};
inline const Portal *match(const world_studio::Light &l){
 if(l.flicker)return nullptr;
 for(const auto &p:portals){
  const bool original=l.cone>=140&&l.cone<=160&&std::abs(l.y-601.715999961)<.03&&
      std::abs(l.x-p.x)<.03&&std::abs(l.z-p.z)<.03;
  const bool calibrated=l.cone>=109&&l.cone<=111&&std::abs(l.y-p.socket[1])<.03&&
      std::abs(l.x-p.socket[0])<.03&&std::abs(l.z-p.socket[2])<.03;
  if(original||calibrated)return &p;
 }
 return nullptr;
}
// Any of the seven window keys as stored in a document: original fixture,
// calibrated socket, or the calibrated cone/height saved at the original pane
// depth (current world.studio). Position, cone and daytime values are untouched.
inline bool window_key(const world_studio::Light &l){
 if(l.flicker)return false;
 for(const auto &p:portals){
  // Within 20 cm of the pane-to-socket segment (stored documents differ in
  // how far the key was pulled in from the glass).
  const double ax=p.x,az=p.z,bx=p.socket[0],bz=p.socket[2],dx=bx-ax,dz=bz-az;
  const double t=std::clamp(((l.x-ax)*dx+(l.z-az)*dz)/std::max(dx*dx+dz*dz,1e-9),0.,1.);
  if(std::hypot(l.x-(ax+t*dx),l.z-(az+t*dz))>.20)continue;
  const bool original=l.cone>=140&&l.cone<=160&&std::abs(l.y-601.715999961)<.03;
  const bool calibrated=l.cone>=109&&l.cone<=111&&std::abs(l.y-p.socket[1])<.03;
  if(original||calibrated)return true;
 }
 return false;
}
// Night: a faint cool moon/street-lamp spill keeps the windows readable
// without competing with the warm room practicals. Exact no-op by day.
inline world_studio::Light night_adjusted(world_studio::Light l){
 if(night<=0||!window_key(l))return l;
 const float n=std::clamp(night,0.f,1.f);
 l.power*=1.f-.93f*n;
 l.color={l.color[0]+(.52f-l.color[0])*n,l.color[1]+(.64f-l.color[1])*n,l.color[2]+(1.f-l.color[2])*n};
 return l;
}
inline world_studio::Light incident(const world_studio::Light &source){
 auto l=source;const auto *p=match(source);if(!p)return night_adjusted(l);
 // Original imported fixture: calibrate it. Already-calibrated documents
 // (cone 110 at the socket) keep their stored daylight values.
 if(source.cone>=140){
  // Start just inside the glazing so its opaque display surface cannot block
  // its own source. Curtains, furniture and the room shell still cast shadows.
  l.x=p->socket[0];l.y=p->socket[1];l.z=p->socket[2];
  l.yaw=p->east?90.f:0.f;l.pitch=-30;l.cone=110;l.radius=8;
  l.power=std::min(40.f,l.power*12.f);l.color={1.f,.86f,.70f};
 }
 return night_adjusted(l);
}
inline bool shadow_source(const std::array<float,4> &position){
 for(const auto &p:portals)if(std::abs(position[0]-p.socket[0])<.04&&
     std::abs(position[1]-p.socket[1])<.04&&std::abs(position[2]-p.socket[2])<.04)return true;
 return false;
}
}
#endif

#ifndef VALWOOD_WORLD_COMBAT_PRESENTATION_H
#define VALWOOD_WORLD_COMBAT_PRESENTATION_H
#include <algorithm>
#include <array>
#include <cmath>
#include "engine/world_models.h"
namespace world_combat_presentation {
// Gameplay still owns .36 contact and .60 recovery. A cooked bank can have
// different authored contact frames; its mesh and blade sockets use this same
// piecewise map, so moving the presentation cannot move a gameplay hit.
inline float source_phase(float phase,float contact_frame) {
    const float contact=std::clamp(contact_frame/11.f,.08f,.58f);
    phase=std::clamp(phase,0.f,1.f);
    if(phase<=.36f)return phase*(contact/.36f);
    if(phase<=.60f)return contact+(phase-.36f)*((.60f-contact)/.24f);
    return phase;
}
struct Bank {
    bool upper_body_aimed=false;
    std::array<float,5> contact_frames={{3.96f,3.96f,3.96f,3.96f,3.96f}};
};
// Each bank uses its own silhouette. The old common .8 scale put the shield
// across the crosshair and cropped nearly the entire axe head above the image.
// The same instance is retained for rendering and viewmodel contact sockets.
inline WorldModelInstance viewmodel(unsigned loadout) {
    if(loadout==2)return {.06f,-1.02f,-.08f,3.14159265359f,.66f};
    if(loadout==3)return {0,-.99f,-.50f,3.14159265359f,.67f};
    if(loadout==4)return {0,-1.03f,-.35f,3.14159265359f,.70f};
    return {0,-1.15f,.05f,3.14159265359f,.80f};
}
}
#endif

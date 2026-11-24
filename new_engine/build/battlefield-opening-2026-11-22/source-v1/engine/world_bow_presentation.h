#ifndef VALWOOD_WORLD_BOW_PRESENTATION_H
#define VALWOOD_WORLD_BOW_PRESENTATION_H
#include <algorithm>
#include <cmath>
// Presentation only: charge, shot timing, stamina and projectile aim remain in
// Arena. Body, arms and both measured hand sockets consume this same value.
struct WorldBowPresentation {
    float draw=0,return_from=0,return_age=.24f;
    bool was_drawing=false,was_visible=false;
    float update(float charge,bool drawing,bool visible,float dt,bool cut=false){
        charge=std::clamp(std::isfinite(charge)?charge:0.f,0.f,1.f);
        dt=std::clamp(std::isfinite(dt)?dt:0.f,0.f,.05f);
        if(!visible||cut){draw=visible?charge:0;return_from=draw;return_age=.24f;was_drawing=drawing;was_visible=visible;return draw;}
        if(!was_visible){draw=charge;return_age=.24f;}
        if(drawing){
            // A fresh draw during the previous return cannot teleport the
            // string hand back to its unloaded pose.
            const float step=4.f*dt;draw+=std::clamp(charge-draw,-step,step);
            return_from=draw;return_age=0;
        }else{
            if(was_drawing){return_from=draw;return_age=0;}
            return_age=std::min(.24f,return_age+dt);
            const float t=return_age/.24f,w=t*t*(3-2*t);
            draw=return_from*(1-w);
        }
        was_visible=true;was_drawing=drawing;return draw;
    }
};
#endif

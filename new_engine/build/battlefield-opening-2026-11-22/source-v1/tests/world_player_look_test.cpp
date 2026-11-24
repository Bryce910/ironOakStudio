#include "engine/world_controller.h"
#include "engine/world_combat_stick.h"
#include <cassert>
#include <limits>
int main(){
    using namespace world_controller;
    assert(camera_stick({}).x==0);
    assert(camera_stick({std::numeric_limits<float>::infinity(),0}).x==0);
    assert(camera_stick(stick(.1f,0,.18f)).x==0);
    float last=0;
    for(int i=1;i<=1000;++i){
        const float t=float(i)/1000;const auto a=camera_stick({t,0});
        assert(a.x>last&&a.x<=t+1e-6f&&a.y==0);last=a.x;
        const auto mirrored=camera_stick({-t,0});assert(std::abs(a.x+mirrored.x)<1e-6f);
        const auto diagonal=camera_stick({t*.6f,t*.8f});
        assert(std::abs(std::hypot(diagonal.x,diagonal.y)-a.x)<1e-6f);
        assert(std::abs(diagonal.x*.8f-diagonal.y*.6f)<1e-6f);
    }
    assert(std::abs(camera_stick({1,0}).x-1)<1e-6f);
    assert(camera_stick({.5f,0}).x>.25f&&camera_stick({.5f,0}).x<.4f);
    assert(std::hypot(camera_stick({1,1}).x,camera_stick({1,1}).y)<=1.000001f);
    // The free-camera curve must not lower the sword's raw gesture radius.
    SwordStick sword;sword.sample({},true,false,true);
    const auto raw=stick(.8f,0,.18f);const auto camera=camera_stick(raw);
    const auto attack=sword.sample(raw,true,false,true);
    assert(attack.attack&&attack.select_direction&&attack.direction==TrainingDirection::Right);
    assert(camera.x<raw.x);
    // Release is immediate: this curve has no stored velocity or pending input.
    assert(camera_stick({}).x==0&&camera_stick({}).y==0);
    std::puts("PASS radial camera precision, monotonic/full-range response, raw sword gesture and immediate release");
}

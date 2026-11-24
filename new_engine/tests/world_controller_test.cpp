#include "engine/world_controller.h"
#include "engine/world_controller_qa.h"
extern "C" {
#include "engine/input_runtime.h"
}
#include <iostream>
#include <limits>
#include <cstring>
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<" | "<<SDL_GetError()<<'\n';return 1;}}while(0)
int main(){
    using namespace world_controller;
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    CHECK(SDL_Init(SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS)==0);
    std::cout<<"Physical controllers visible to SDL before test: "<<SDL_NumJoysticks()<<'\n';
    // Geometry of the deadzone: diagonal motion must preserve direction and
    // cap total speed, while tiny hardware noise stays exactly still.
    CHECK(stick(.1f,.1f,.18f).x==0);CHECK(stick(0,-1,.18f).y==-1);
    auto diagonal=stick(1,1,.18f);CHECK(std::abs(std::hypot(diagonal.x,diagonal.y)-1)<.0001);
    CHECK(stick(std::numeric_limits<float>::quiet_NaN(),1,.18f).x==0);
    for(auto context:{Context::Menu,Context::Dialogue,Context::Retry}){
        CHECK(ui_owns(context));CHECK(route(SDL_CONTROLLER_BUTTON_A,context)==Action::None);
        CHECK(route(SDL_CONTROLLER_BUTTON_DPAD_DOWN,context)==Action::None);
        CHECK(route(SDL_CONTROLLER_BUTTON_X,context)==Action::None);
        CHECK(route(SDL_CONTROLLER_BUTTON_Y,context)==Action::None);
        CHECK(route(SDL_CONTROLLER_BUTTON_B,context)==Action::Back);
    }
    CHECK(route(SDL_CONTROLLER_BUTTON_A,Context::Explore)==Action::Interact);
    CHECK(route(SDL_CONTROLLER_BUTTON_X,Context::Explore)==Action::Jump);
    CHECK(route(SDL_CONTROLLER_BUTTON_B,Context::Combat)==Action::Dodge);
    CHECK(route(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,Context::Combat)==Action::Thrust);
    CHECK(route(SDL_CONTROLLER_BUTTON_DPAD_DOWN,Context::Combat)==Action::Low);
    CHECK(route(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,Context::Review)==Action::NextView);
    CHECK(!std::strcmp(label(SDL_CONTROLLER_TYPE_PS5,SDL_CONTROLLER_BUTTON_A),"Cross"));
    CHECK(!std::strcmp(label(SDL_CONTROLLER_TYPE_XBOX360,SDL_CONTROLLER_BUTTON_A),"A"));
    {
        world_controller_qa::Pad virtual_pad;CHECK(virtual_pad.attach());Device device;
        device.prefer(SDL_JoystickInstanceID(virtual_pad.joystick));CHECK(device.get());
        auto pump=[&](){virtual_pad.tick();SDL_Event e;while(SDL_PollEvent(&e))device.event(e);};
        pump();device.poll(.18f,true);CHECK(!device.attack_pressed&&!device.guard);
        virtual_pad.axis(SDL_CONTROLLER_AXIS_LEFTY,-32768);pump();device.poll(.18f,true);CHECK(device.move.y==-1&&device.using_pad);
        virtual_pad.axis(SDL_CONTROLLER_AXIS_LEFTX,32767);pump();device.poll(.18f,true);CHECK(std::hypot(device.move.x,device.move.y)<=1.00001);
        virtual_pad.axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,32767);pump();device.poll(.18f,true);CHECK(device.attack_pressed);
        device.poll(.18f,true);CHECK(!device.attack_pressed); // Held trigger never repeats.
        device.poll(.18f,false);device.poll(.18f,true);CHECK(!device.attack_pressed); // No shot on menu exit.
        virtual_pad.axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);pump();device.poll(.18f,true);
        virtual_pad.axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,32767);pump();device.poll(.18f,true);CHECK(device.attack_pressed);
        virtual_pad.axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT,32767);pump();device.poll(.18f,true);CHECK(device.guard);
        device.poll(.18f,false);device.poll(.18f,true);CHECK(!device.guard);
        virtual_pad.axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT,-32768);pump();device.poll(.18f,true);
        virtual_pad.axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT,32767);pump();device.poll(.18f,true);CHECK(device.guard);
        virtual_pad.button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER,true);pump();device.poll(.18f,true);CHECK(device.sprint);
        const unsigned rumbles=virtual_pad.rumble_calls;device.rumble(.2f,.4f,80,true);CHECK(virtual_pad.rumble_calls==rumbles+1);
        device.rumble(.2f,.4f,80,false);CHECK(virtual_pad.rumble_calls==rumbles+1);
        SDL_Event focus={};focus.type=SDL_WINDOWEVENT;focus.window.event=SDL_WINDOWEVENT_FOCUS_LOST;device.event(focus);device.poll(.18f,true);
        CHECK(!device.focused&&device.move.x==0&&!device.guard&&!device.attack_pressed);
        focus.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;device.event(focus);device.poll(.18f,true);CHECK(!device.attack_pressed&&!device.guard);
        SDL_Event keyboard={};keyboard.type=SDL_KEYDOWN;device.event(keyboard);CHECK(!device.using_pad);
        SDL_Event foreign={};foreign.type=SDL_CONTROLLERBUTTONDOWN;foreign.cbutton.which=device.id()+100;device.event(foreign);CHECK(!device.using_pad);
        foreign.cbutton.which=device.id();device.event(foreign);CHECK(device.using_pad);
        // Detach while controls are held, then explicitly select a replacement.
        // Test-device selection also prevents touching physical controllers.
        virtual_pad.detach();world_controller_qa::Pad replacement;CHECK(replacement.attach());
        bool disconnected=false;SDL_Event e;while(SDL_PollEvent(&e))disconnected|=device.event(e);
        CHECK(disconnected&&device.disconnects==1&&!device.get());
        device.prefer(SDL_JoystickInstanceID(replacement.joystick));CHECK(device.connects==2&&device.get());
        device.poll(.18f,true);CHECK(!device.attack_pressed&&!device.guard&&!device.sprint&&device.move.x==0);
        // The legacy scene shares the reconnect fix and must clear cached edges.
        InputRuntimeState old;input_runtime_init(&old);old.pad=SDL_GameControllerOpen(replacement.index);CHECK(old.pad);
        old.pad_down[SDL_CONTROLLER_BUTTON_A]=old.pad_prev[SDL_CONTROLLER_BUTTON_A]=true;
        const auto removed=SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(old.pad));
        replacement.detach();InputRuntimeAPI api={};input_runtime_process_events(&old,&api);
        CHECK((!old.pad||SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(old.pad))!=removed)&&!old.pad_down[SDL_CONTROLLER_BUTTON_A]&&!old.pad_prev[SDL_CONTROLLER_BUTTON_A]);
        input_runtime_shutdown(&old);device.close();
    }
    SDL_Quit();std::cout<<"world_controller: "<<checks<<" checks passed (SDL virtual device, routing, axes, triggers, focus, haptics, reconnect)\n";
}

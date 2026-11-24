#ifndef VALWOOD_WORLD_CONTROLLER_H
#define VALWOOD_WORLD_CONTROLLER_H
#include <SDL2/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace world_controller {
struct Stick {float x=0,y=0;};
inline Stick stick(float x,float y,float deadzone){
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(deadzone))return {};
    const float length=std::hypot(x,y),dead=std::clamp(deadzone,.05f,.45f);
    if(length<=dead)return {};
    const float scale=std::min(1.f,(length-dead)/(1-dead))/length;
    return {x*scale,y*scale};
}
enum class Context {Menu,Dialogue,Retry,Explore,Combat,Review};
enum class Action {None,Back,Interact,Jump,Dodge,Journal,Camera,Attack,Thrust,Left,Right,High,Low,PreviousView,NextView};
inline bool ui_owns(Context c){return c==Context::Menu||c==Context::Dialogue||c==Context::Retry;}
inline Action route(SDL_GameControllerButton b,Context c){
    if(b==SDL_CONTROLLER_BUTTON_START)return Action::Back;
    if(ui_owns(c))return b==SDL_CONTROLLER_BUTTON_B?Action::Back:Action::None;
    if(c==Context::Review){
        if(b==SDL_CONTROLLER_BUTTON_B)return Action::Back;
        if(b==SDL_CONTROLLER_BUTTON_LEFTSHOULDER)return Action::PreviousView;
        if(b==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)return Action::NextView;
    }
    if(b==SDL_CONTROLLER_BUTTON_BACK||b==SDL_CONTROLLER_BUTTON_RIGHTSTICK)return Action::Camera;
    if(b==SDL_CONTROLLER_BUTTON_Y&&c!=Context::Review)return Action::Journal;
    if(c==Context::Combat){switch(b){
        case SDL_CONTROLLER_BUTTON_X:return Action::Attack;
        case SDL_CONTROLLER_BUTTON_B:return Action::Dodge;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:return Action::Thrust;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:return Action::Left;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:return Action::Right;
        case SDL_CONTROLLER_BUTTON_DPAD_UP:return Action::High;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:return Action::Low;
        default:return Action::None;
    }}
    if(b==SDL_CONTROLLER_BUTTON_A&&c==Context::Explore)return Action::Interact;
    if(b==SDL_CONTROLLER_BUTTON_X)return Action::Jump;
    return Action::None;
}
inline bool playstation(SDL_GameControllerType type){return type==SDL_CONTROLLER_TYPE_PS3||type==SDL_CONTROLLER_TYPE_PS4||type==SDL_CONTROLLER_TYPE_PS5;}
inline const char *label(SDL_GameControllerType type,SDL_GameControllerButton button){
    const bool ps=playstation(type);
    switch(button){
        case SDL_CONTROLLER_BUTTON_A:return ps?"Cross":"A";
        case SDL_CONTROLLER_BUTTON_B:return ps?"Circle":"B";
        case SDL_CONTROLLER_BUTTON_X:return ps?"Square":"X";
        case SDL_CONTROLLER_BUTTON_Y:return ps?"Triangle":"Y";
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:return ps?"L1":"LB";
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:return ps?"R1":"RB";
        case SDL_CONTROLLER_BUTTON_START:return ps?"Options":"Menu";
        case SDL_CONTROLLER_BUTTON_BACK:return ps?"Share":"View";
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK:return ps?"R3":"RS click";
        default:return "Button";
    }
}
class Device {
    SDL_GameController *pad_=nullptr;
    SDL_JoystickID preferred_=-1;
    bool primed_=false,attack_before_=false,guard_armed_=false,attack_armed_=false;
public:
    bool using_pad=false,focused=true;
    unsigned connects=0,disconnects=0,rumble_requests=0;
    Stick move,look;
    float left_trigger=0,right_trigger=0;
    bool attack_pressed=false,guard=false,sprint=false;
    Device()=default;
    Device(const Device&)=delete;Device &operator=(const Device&)=delete;
    ~Device(){close();}
    SDL_GameController *get()const{return pad_;}
    SDL_JoystickID id()const{return pad_?SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad_)):-1;}
    SDL_GameControllerType type()const{return pad_?SDL_GameControllerGetType(pad_):SDL_CONTROLLER_TYPE_UNKNOWN;}
    const char *name()const{return pad_?SDL_GameControllerName(pad_):"No controller connected";}
    const char *button(SDL_GameControllerButton b)const{return label(type(),b);}
    const char *attack_label()const{return playstation(type())?"R2":"RT";}
    const char *guard_label()const{return playstation(type())?"L2":"LT";}
    void clear(){move={};look={};left_trigger=right_trigger=0;attack_pressed=guard=sprint=false;primed_=false;attack_before_=false;guard_armed_=attack_armed_=false;}
    void stop_rumble(){if(pad_)SDL_GameControllerRumble(pad_,0,0,0);}
    void close(){if(pad_){stop_rumble();SDL_GameControllerClose(pad_);pad_=nullptr;}clear();}
    void open(){
        if(pad_)return;
        for(int i=0;i<SDL_NumJoysticks();++i)if(SDL_IsGameController(i)){
            if(preferred_>=0&&SDL_JoystickGetDeviceInstanceID(i)!=preferred_)continue;
            pad_=SDL_GameControllerOpen(i);
            if(pad_){clear();++connects;std::printf("[Controller] Connected: %s\n",name());break;}
        }
    }
    void prefer(SDL_JoystickID instance){close();preferred_=instance;open();}
    // Returns true only when the controller actually driving this session left.
    bool event(const SDL_Event &e){
        if(e.type==SDL_CONTROLLERDEVICEADDED){open();return false;}
        if(e.type==SDL_CONTROLLERDEVICEREMOVED&&pad_&&e.cdevice.which==id()){
            const bool active=using_pad;close();using_pad=false;++disconnects;open();return active;
        }
        if(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_FOCUS_LOST){focused=false;clear();stop_rumble();}
        if(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_FOCUS_GAINED){focused=true;clear();}
        if(!focused)return false;
        if(e.type==SDL_CONTROLLERBUTTONDOWN&&e.cbutton.which==id())using_pad=true;
        if(e.type==SDL_CONTROLLERAXISMOTION&&e.caxis.which==id()){
            const int value=int(e.caxis.value);
            if((e.caxis.axis<SDL_CONTROLLER_AXIS_TRIGGERLEFT&&std::abs(value)>10000)||
               (e.caxis.axis>=SDL_CONTROLLER_AXIS_TRIGGERLEFT&&value>12000))using_pad=true;
        }
        if(e.type==SDL_KEYDOWN||e.type==SDL_MOUSEBUTTONDOWN||e.type==SDL_MOUSEWHEEL||
           (e.type==SDL_MOUSEMOTION&&(std::abs(e.motion.xrel)>2||std::abs(e.motion.yrel)>2)))using_pad=false;
        return false;
    }
    void poll(float deadzone,bool actions_allowed){
        attack_pressed=guard=sprint=false;move={};look={};
        if(!pad_||!focused)return;
        auto axis=[&](SDL_GameControllerAxis a){const int v=SDL_GameControllerGetAxis(pad_,a);return float(v)/(v<0?32768.f:32767.f);};
        move=stick(axis(SDL_CONTROLLER_AXIS_LEFTX),axis(SDL_CONTROLLER_AXIS_LEFTY),deadzone);
        look=stick(axis(SDL_CONTROLLER_AXIS_RIGHTX),axis(SDL_CONTROLLER_AXIS_RIGHTY),deadzone);
        left_trigger=std::max(0.f,axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT));right_trigger=std::max(0.f,axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT));
        if(!actions_allowed){guard_armed_=attack_armed_=false;attack_before_=right_trigger>.4f;primed_=true;return;}
        if(left_trigger<.15f)guard_armed_=true;
        if(right_trigger<.15f)attack_armed_=true;
        const bool attack=right_trigger>.4f;
        attack_pressed=primed_&&attack_armed_&&attack&&!attack_before_;
        guard=primed_&&guard_armed_&&left_trigger>.4f;attack_before_=attack;primed_=true;
        sprint=SDL_GameControllerGetButton(pad_,SDL_CONTROLLER_BUTTON_LEFTSHOULDER)||SDL_GameControllerGetButton(pad_,SDL_CONTROLLER_BUTTON_LEFTSTICK);
    }
    void rumble(float low,float high,Uint32 milliseconds,bool enabled){
        if(!pad_||!focused||!using_pad||!enabled)return;
        ++rumble_requests;
        SDL_GameControllerRumble(pad_,Uint16(std::clamp(low,0.f,1.f)*65535),Uint16(std::clamp(high,0.f,1.f)*65535),milliseconds);
    }
};
}
#endif

#ifndef VALWOOD_WORLD_CONTROLLER_QA_H
#define VALWOOD_WORLD_CONTROLLER_QA_H
#include <SDL2/SDL.h>
#include <array>
// Explicit QA only: a real SDL virtual joystick, never an OS input injector.
namespace world_controller_qa {
struct Pad {
    SDL_Joystick *joystick=nullptr;int index=-1;unsigned clock=0,rumble_calls=0;
    std::array<unsigned,SDL_CONTROLLER_BUTTON_MAX> releases={};
    static int rumble(void *data,Uint16 low,Uint16 high){if(low||high)++static_cast<Pad*>(data)->rumble_calls;return 0;}
    bool attach(){
        SDL_VirtualJoystickDesc d={};d.version=SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
        d.type=SDL_JOYSTICK_TYPE_GAMECONTROLLER;d.naxes=SDL_CONTROLLER_AXIS_MAX;d.nbuttons=SDL_CONTROLLER_BUTTON_MAX;
        d.button_mask=(1u<<SDL_CONTROLLER_BUTTON_MAX)-1;d.axis_mask=(1u<<SDL_CONTROLLER_AXIS_MAX)-1;
        d.name="Valwood QA controller";d.vendor_id=0x045e;d.product_id=0x028e;d.userdata=this;d.Rumble=rumble;
        index=SDL_JoystickAttachVirtualEx(&d);if(index<0)return false;
        joystick=SDL_JoystickOpen(index);if(!joystick)return false;
        axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT,-32768);axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);SDL_JoystickUpdate();return true;
    }
    void axis(SDL_GameControllerAxis a,Sint16 v){if(joystick)SDL_JoystickSetVirtualAxis(joystick,a,v);}
    void button(SDL_GameControllerButton b,bool down){if(joystick)SDL_JoystickSetVirtualButton(joystick,b,down?1:0);}
    void press(SDL_GameControllerButton b){button(b,true);releases[b]=clock+1;}
    void tick(){++clock;for(unsigned i=0;i<releases.size();++i)if(releases[i]&&clock>=releases[i]){button(SDL_GameControllerButton(i),false);releases[i]=0;}SDL_JoystickUpdate();}
    void detach(){
        if(!joystick)return;
        const auto id=SDL_JoystickInstanceID(joystick);int current=-1;
        for(int i=0;i<SDL_NumJoysticks();++i)if(SDL_JoystickGetDeviceInstanceID(i)==id){current=i;break;}
        if(current>=0)SDL_JoystickDetachVirtual(current);
        SDL_JoystickClose(joystick);joystick=nullptr;index=-1;releases={};
    }
    ~Pad(){detach();}
};
}
#endif

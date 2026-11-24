#include "engine/input_runtime.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void input_runtime_init(InputRuntimeState *state){
    if(!state)return;
    memset(state,0,sizeof(*state));
    state->pad=NULL;
    state->right_drag=false;
}

void input_runtime_shutdown(InputRuntimeState *state){
    if(!state)return;
    if(state->pad){
        SDL_GameControllerClose(state->pad);
        state->pad=NULL;
    }
    state->right_drag=false;
}

void input_runtime_try_open_gamepad(InputRuntimeState *state){
    if(!state||state->pad)return;
    for(int i=0;i<SDL_NumJoysticks();i++){
        if(SDL_IsGameController(i)){
            state->pad=SDL_GameControllerOpen(i);
            if(state->pad){
                memset(state->pad_down,0,sizeof(state->pad_down));
                memset(state->pad_pressed,0,sizeof(state->pad_pressed));
                memset(state->pad_prev,0,sizeof(state->pad_prev));
                printf("[Pad] Opened: %s\n",SDL_GameControllerName(state->pad));
                return;
            }
        }
    }
}

bool input_runtime_has_gamepad(const InputRuntimeState *state){
    return state&&state->pad;
}

float input_runtime_pad_axis(const InputRuntimeState *state,SDL_GameControllerAxis axis,float deadzone){
    if(!state||!state->pad)return 0.0f;
    {
        float v=SDL_GameControllerGetAxis(state->pad,axis)/32767.0f;
        return fabsf(v)>deadzone?v:0.0f;
    }
}

bool input_runtime_pad_btn(const InputRuntimeState *state,SDL_GameControllerButton button){
    return state&&state->pad&&button>=0&&button<SDL_CONTROLLER_BUTTON_MAX&&state->pad_down[button];
}

bool input_runtime_pad_pressed(const InputRuntimeState *state,SDL_GameControllerButton button){
    return state&&state->pad&&button>=0&&button<SDL_CONTROLLER_BUTTON_MAX&&state->pad_pressed[button];
}

InputGamepadFrame input_runtime_poll_gamepad(InputRuntimeState *state){
    InputGamepadFrame frame={0};
    if(!state||!state->pad)return frame;

    frame.connected=true;
    for(int i=0;i<SDL_CONTROLLER_BUTTON_MAX;i++){
        state->pad_down[i]=SDL_GameControllerGetButton(state->pad,(SDL_GameControllerButton)i)!=0;
        state->pad_pressed[i]=state->pad_down[i]&&!state->pad_prev[i];
        state->pad_prev[i]=state->pad_down[i];
    }

    frame.a_down=state->pad_down[SDL_CONTROLLER_BUTTON_A];
    frame.x_down=state->pad_down[SDL_CONTROLLER_BUTTON_X];
    frame.y_down=state->pad_down[SDL_CONTROLLER_BUTTON_Y];
    frame.b_down=state->pad_down[SDL_CONTROLLER_BUTTON_B];
    frame.start_down=state->pad_down[SDL_CONTROLLER_BUTTON_START];
    frame.back_down=state->pad_down[SDL_CONTROLLER_BUTTON_BACK];

    frame.a_pressed=state->pad_pressed[SDL_CONTROLLER_BUTTON_A];
    frame.x_pressed=state->pad_pressed[SDL_CONTROLLER_BUTTON_X];
    frame.y_pressed=state->pad_pressed[SDL_CONTROLLER_BUTTON_Y];
    frame.b_pressed=state->pad_pressed[SDL_CONTROLLER_BUTTON_B];
    frame.start_pressed=state->pad_pressed[SDL_CONTROLLER_BUTTON_START];
    frame.back_pressed=state->pad_pressed[SDL_CONTROLLER_BUTTON_BACK];

    return frame;
}

void input_runtime_process_events(InputRuntimeState *state,InputRuntimeAPI *api){
    SDL_Event ev;

    if(!state||!api)return;

    while(SDL_PollEvent(&ev)){
        if(api->on_event&&api->on_event(&ev,api->userdata)) continue;
        switch(ev.type){
        case SDL_QUIT:
            if(api->on_quit)api->on_quit(api->userdata);
            break;
        case SDL_TEXTINPUT:
            if(api->on_text_input)api->on_text_input(&ev.text,api->userdata);
            break;
        case SDL_KEYDOWN:
            if(api->on_keydown)api->on_keydown(&ev.key,api->userdata);
            break;
        case SDL_MOUSEBUTTONDOWN:
            if(ev.button.button==SDL_BUTTON_LEFT&&api->on_left_mouse_down)
                api->on_left_mouse_down(ev.button.x,ev.button.y,api->userdata);
            if(ev.button.button==SDL_BUTTON_RIGHT){
                state->right_drag=true;
                if(api->on_right_mouse_down)
                    api->on_right_mouse_down(ev.button.x,ev.button.y,api->userdata);
            }
            if(ev.button.button==SDL_BUTTON_MIDDLE&&api->on_middle_mouse_down)
                api->on_middle_mouse_down(ev.button.x,ev.button.y,api->userdata);
            break;
        case SDL_MOUSEBUTTONUP:
            if(ev.button.button==SDL_BUTTON_RIGHT){
                state->right_drag=false;
                if(api->on_right_mouse_up)
                    api->on_right_mouse_up(ev.button.x,ev.button.y,api->userdata);
            }
            break;
        case SDL_MOUSEMOTION:
            if(api->on_mouse_motion)api->on_mouse_motion(&ev.motion,state->right_drag,api->userdata);
            break;
        case SDL_MOUSEWHEEL:
            if(api->on_mouse_wheel)api->on_mouse_wheel(&ev.wheel,api->userdata);
            break;
        case SDL_CONTROLLERBUTTONDOWN:
            if(api->on_controller_button_down)api->on_controller_button_down(&ev.cbutton,api->userdata);
            break;
        case SDL_WINDOWEVENT:
            if(ev.window.event==SDL_WINDOWEVENT_RESIZED&&api->on_window_resized)
                api->on_window_resized(ev.window.data1,ev.window.data2,api->userdata);
            break;
        case SDL_CONTROLLERDEVICEADDED:
            input_runtime_try_open_gamepad(state);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            if(state->pad&&SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(state->pad))==ev.cdevice.which){
                SDL_GameControllerClose(state->pad);
                state->pad=NULL;
                memset(state->pad_down,0,sizeof(state->pad_down));
                memset(state->pad_pressed,0,sizeof(state->pad_pressed));
                memset(state->pad_prev,0,sizeof(state->pad_prev));
                printf("[Pad] Disconnected\n");
                input_runtime_try_open_gamepad(state);
            }
            break;
        default:
            break;
        }
    }
}

#ifndef ENGINE_INPUT_RUNTIME_H
#define ENGINE_INPUT_RUNTIME_H

#include <stdbool.h>
#include <SDL2/SDL.h>

typedef struct{
    SDL_GameController *pad;
    bool pad_down[SDL_CONTROLLER_BUTTON_MAX];
    bool pad_pressed[SDL_CONTROLLER_BUTTON_MAX];
    bool pad_prev[SDL_CONTROLLER_BUTTON_MAX];
    bool pad_a_prev;
    bool pad_x_prev;
    bool pad_y_prev;
    bool pad_b_prev;
    bool pad_start_prev;
    bool pad_back_prev;
    bool right_drag;
}InputRuntimeState;

typedef struct{
    bool connected;
    bool a_down;
    bool x_down;
    bool y_down;
    bool b_down;
    bool start_down;
    bool back_down;
    bool a_pressed;
    bool x_pressed;
    bool y_pressed;
    bool b_pressed;
    bool start_pressed;
    bool back_pressed;
}InputGamepadFrame;

typedef struct{
    bool (*on_event)(const SDL_Event *ev,void *userdata);
    void (*on_quit)(void *userdata);
    void (*on_text_input)(const SDL_TextInputEvent *ev,void *userdata);
    void (*on_keydown)(const SDL_KeyboardEvent *ev,void *userdata);
    void (*on_left_mouse_down)(int x,int y,void *userdata);
    void (*on_right_mouse_down)(int x,int y,void *userdata);
    void (*on_right_mouse_up)(int x,int y,void *userdata);
    void (*on_middle_mouse_down)(int x,int y,void *userdata);
    void (*on_mouse_motion)(const SDL_MouseMotionEvent *ev,bool right_drag,void *userdata);
    void (*on_mouse_wheel)(const SDL_MouseWheelEvent *ev,void *userdata);
    void (*on_controller_button_down)(const SDL_ControllerButtonEvent *ev,void *userdata);
    void (*on_window_resized)(int w,int h,void *userdata);
    void *userdata;
}InputRuntimeAPI;

void input_runtime_init(InputRuntimeState *state);
void input_runtime_shutdown(InputRuntimeState *state);
void input_runtime_try_open_gamepad(InputRuntimeState *state);
void input_runtime_process_events(InputRuntimeState *state,InputRuntimeAPI *api);
bool input_runtime_has_gamepad(const InputRuntimeState *state);
float input_runtime_pad_axis(const InputRuntimeState *state,SDL_GameControllerAxis axis,float deadzone);
bool input_runtime_pad_btn(const InputRuntimeState *state,SDL_GameControllerButton button);
bool input_runtime_pad_pressed(const InputRuntimeState *state,SDL_GameControllerButton button);
InputGamepadFrame input_runtime_poll_gamepad(InputRuntimeState *state);

#endif

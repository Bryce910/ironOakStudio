#ifndef ENGINE_FRAME_RUNTIME_H
#define ENGINE_FRAME_RUNTIME_H

#include <stdbool.h>
#include <SDL2/SDL.h>
#include "engine/pause_menu.h"

#define INTRO_STORY_SECTION_MAX 16
#define INTRO_STORY_TEXT_MAX 1024
#define INTRO_FIREFLY_COUNT 40

typedef struct{
    float x;
    float y;
    float vx;
    float vy;
    float phase;
    float size;
}IntroFirefly;

typedef struct{
    bool active;
    bool completed;
    int section_count;
    int current_section;
    float section_alpha;
    float section_timer;
    float prompt_pulse;
    char sections[INTRO_STORY_SECTION_MAX][INTRO_STORY_TEXT_MAX];
    IntroFirefly fireflies[INTRO_FIREFLY_COUNT];
}IntroSequenceState;

typedef struct{
    void (*rect)(void *hud,float x,float y,float w,float h,float r,float g,float b,float a);
    void (*text)(void *hud,float x,float y,float scale,const char *text,float r,float g,float b,float a);
    void (*flush)(void *hud);
    void (*set_window_title)(void *window,const char *title);
    void (*swap_window)(void *window);
    void (*set_depth_test)(bool enabled);
}FrameHUDAPI;

typedef struct{
    bool dead;
    float health;
    float mana;
    int gold;
    int xp;
    float total_time;
    float intro_fade;
}FrameRuntimeState;

void intro_sequence_init(IntroSequenceState *state,unsigned int seed);
bool intro_sequence_load_from_file(IntroSequenceState *state,const char *path);
void intro_sequence_activate(IntroSequenceState *state);
bool intro_sequence_is_active(const IntroSequenceState *state);
bool intro_sequence_handle_keydown(IntroSequenceState *state,SDL_Keycode keycode);
bool intro_sequence_handle_mouse_button(IntroSequenceState *state,Uint8 button);
bool intro_sequence_handle_controller_button(IntroSequenceState *state,Uint8 button);

void frame_runtime_draw_tail(void *hud,void *window,int win_w,int win_h,
                             FrameRuntimeState *state,PauseMenu *pause,
                             IntroSequenceState *intro,
                             PauseMenuAPI *pause_api,FrameHUDAPI *hud_api,
                             bool *running,bool *restart_to_menu,float dt);

#endif

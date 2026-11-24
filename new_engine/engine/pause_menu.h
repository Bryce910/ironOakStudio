#ifndef ENGINE_PAUSE_MENU_H
#define ENGINE_PAUSE_MENU_H

#include <stdbool.h>

typedef enum{ PM_NONE, PM_ROOT, PM_SAVE_SLOTS, PM_OPTIONS }PauseState;

typedef struct{
    PauseState state;
    bool lmb_prev;
    int active_slider;
}PauseMenu;

typedef struct{
    void (*rect)(void *hud,float x,float y,float w,float h,float r,float g,float b,float a);
    void (*text)(void *hud,float x,float y,float scale,const char *text,float r,float g,float b,float a);
    void (*flush)(void *hud);
    void (*menu_btn)(void *hud,float x,float y,float w,float h,const char *label,float scale,
                     int mx,int my,bool enabled,bool *clicked);
    void (*draw_options)(void *hud,float w,float h,int mx,int my,bool clicked,bool held,
                         int *active_slider,bool *back,bool *credits,void *userdata);
    void (*apply_options_live)(void *userdata);
    void (*save_options)(void *userdata);
    void (*set_fullscreen_from_options)(void *userdata);
    void (*play_click)(int volume,void *userdata);
    void (*save_slot)(int slot,void *userdata);
    void (*notify)(const char *msg,void *userdata);
    void *userdata;
}PauseMenuAPI;

void pause_menu_init(PauseMenu *pm);
bool pause_menu_draw(PauseMenu *pm,void *hud,int win_w,int win_h,float world_time,
                     PauseMenuAPI *api,bool *want_resume,bool *want_quit_menu,
                     bool *want_quit_game);

#endif

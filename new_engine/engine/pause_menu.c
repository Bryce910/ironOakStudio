#include "engine/pause_menu.h"

#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>

static void pause_panel(PauseMenuAPI *api,void *hud,float x,float y,float w,float h,float alpha){
    api->rect(hud,x,y,w,h,0.10f,0.085f,0.065f,0.94f*alpha);
    api->rect(hud,x+10.0f,y+10.0f,w-20.0f,h-20.0f,0.12f,0.10f,0.08f,0.82f*alpha);
    api->rect(hud,x+14.0f,y+14.0f,w-28.0f,1.0f,0.34f,0.27f,0.16f,0.88f*alpha);
    api->rect(hud,x+14.0f,y+h-15.0f,w-28.0f,1.0f,0.34f,0.27f,0.16f,0.58f*alpha);
    api->rect(hud,x+14.0f,y+14.0f,1.0f,h-28.0f,0.26f,0.20f,0.12f,0.28f*alpha);
    api->rect(hud,x+w-15.0f,y+14.0f,1.0f,h-28.0f,0.26f,0.20f,0.12f,0.20f*alpha);
}

void pause_menu_init(PauseMenu *pm){
    if(!pm)return;
    pm->state=PM_NONE;
    pm->lmb_prev=false;
    pm->active_slider=-1;
}

bool pause_menu_draw(PauseMenu *pm,void *hud,int win_w,int win_h,float world_time,
                     PauseMenuAPI *api,bool *want_resume,bool *want_quit_menu,
                     bool *want_quit_game){
    (void)world_time;
    if(!pm||!api||pm->state==PM_NONE)return false;

    float W=(float)win_w,H=(float)win_h;
    int mx,my;
    bool lmb=(SDL_GetMouseState(&mx,&my)&SDL_BUTTON(1))!=0;
    bool clicked=lmb&&!pm->lmb_prev;
    pm->lmb_prev=lmb;
    if(!lmb) pm->active_slider=-1;

    api->rect(hud,0,0,W,H,0.02f,0.015f,0.01f,0.72f);

    float pw=320,ph=0,px=(W-pw)*0.5f;
    float bw=pw-40,bh=44,bx=px+20;
    float gap=12;

    if(pm->state==PM_ROOT){
        ph=5*bh+4*gap+80; float py=(H-ph)*0.5f;
        pause_panel(api,hud,px,py,pw,ph,1.0f);

        {
            const char *title="PAUSED";
            float tsc=2.4f,tw=tsc*8.0f*(float)strlen(title);
            api->text(hud,px+20.0f,py+18.0f,0.92f,"[WAYFARER'S REST]",0.48f,0.52f,0.58f,0.88f);
            api->text(hud,px+(pw-tw)*0.5f,py+36.0f,tsc,title,0.90f,0.74f,0.38f,1.0f);
        }

        float by_=py+74.0f;
        bool c=false;
        api->menu_btn(hud,bx,by_,bw,bh,"RESUME",1.8f,mx,my,true,&c);
        if(c&&clicked){pm->state=PM_NONE;if(want_resume)*want_resume=true;api->play_click(70,api->userdata);}
        by_+=bh+gap;

        api->menu_btn(hud,bx,by_,bw,bh,"SAVE GAME",1.8f,mx,my,true,&c);
        if(c&&clicked){pm->state=PM_SAVE_SLOTS;api->play_click(70,api->userdata);}
        by_+=bh+gap;

        api->menu_btn(hud,bx,by_,bw,bh,"OPTIONS",1.8f,mx,my,true,&c);
        if(c&&clicked){pm->state=PM_OPTIONS;api->play_click(70,api->userdata);}
        by_+=bh+gap;

        api->menu_btn(hud,bx,by_,bw,bh,"QUIT TO MENU",1.8f,mx,my,true,&c);
        if(c&&clicked){
            pm->state=PM_NONE;
            if(want_quit_menu)*want_quit_menu=true;
            api->save_options(api->userdata);
            api->play_click(70,api->userdata);
        }
        by_+=bh+gap;

        api->menu_btn(hud,bx,by_,bw,bh,"QUIT GAME",1.8f,mx,my,true,&c);
        if(c&&clicked){
            pm->state=PM_NONE;
            if(want_quit_game)*want_quit_game=true;
            api->save_options(api->userdata);
            api->play_click(70,api->userdata);
        }
    } else if(pm->state==PM_SAVE_SLOTS){
        ph=3*bh+2*gap+100; float py=(H-ph)*0.5f;
        pause_panel(api,hud,px,py,pw,ph,1.0f);

        {
            const char *title="SAVE TO SLOT";
            float tsc=1.8f,tw=tsc*8.0f*(float)strlen(title);
            api->text(hud,px+20.0f,py+18.0f,0.92f,"[LEDGER]",0.48f,0.52f,0.58f,0.88f);
            api->text(hud,px+(pw-tw)*0.5f,py+36.0f,tsc,title,0.90f,0.74f,0.38f,1.0f);
        }

        float by_=py+74.0f;
        for(int s=0;s<3;s++){
            char lbl[32], nbuf[32];
            bool c2=false;
            snprintf(lbl,sizeof(lbl),"SLOT %d",s+1);
            api->menu_btn(hud,bx,by_,bw,bh,lbl,1.8f,mx,my,true,&c2);
            if(c2&&clicked){
                api->save_slot(s,api->userdata);
                snprintf(nbuf,sizeof(nbuf),"Saved - Slot %d",s+1);
                api->notify(nbuf,api->userdata);
                api->play_click(80,api->userdata);
                pm->state=PM_ROOT;
            }
            by_+=bh+gap;
        }
        api->text(hud,px+20.0f,by_+14.0f,1.05f,"< BACK",0.72f,0.68f,0.58f,0.88f);
        if(mx>=px&&mx<=px+80&&my>=by_+8&&my<=by_+28&&clicked){
            pm->state=PM_ROOT;
            api->play_click(70,api->userdata);
        }
    } else if(pm->state==PM_OPTIONS){
        bool back_opt=false, credits_ign=false;
        api->draw_options(hud,(float)win_w,(float)win_h,mx,my,clicked,lmb,
                          &pm->active_slider,&back_opt,&credits_ign,api->userdata);
        api->apply_options_live(api->userdata);
        api->set_fullscreen_from_options(api->userdata);
        if(clicked&&back_opt){
            api->save_options(api->userdata);
            pm->state=PM_ROOT;
            api->play_click(70,api->userdata);
        }
        return true;
    }

    api->flush(hud);
    return true;
}

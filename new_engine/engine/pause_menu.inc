/* ── Menu page enum ──────────────────────────────────────────── */
/* ═══════════════════════════════════════════════════════════════
   PAUSE MENU
   ═══════════════════════════════════════════════════════════════ */
typedef enum{ PM_NONE, PM_ROOT, PM_SAVE_SLOTS, PM_OPTIONS }PauseState;

typedef struct{
    PauseState state;
    bool       lmb_prev;
    int        active_slider;
}PauseMenu;

static PauseMenu g_pause={PM_NONE,false,-1};

/* Returns true if a click was consumed this frame */

static bool pause_draw(PauseMenu *pm,HUD *h,int win_w,int win_h,
                       /* game refs for save/quit: */
                       Player *player,QuestSystem *qs,AbilitySystem *as,
                       Enemy *enemies,int ne,float world_time,Inventory *inv,
                       LevelSystem *lvl,SaveNotify *note,
                       bool *want_resume,bool *want_quit_menu,bool *want_quit_game){
    if(pm->state==PM_NONE)return false;
    float W=(float)win_w,H=(float)win_h;
    int mx,my; bool lmb=(SDL_GetMouseState(&mx,&my)&SDL_BUTTON(1))!=0;
    bool clicked=lmb&&!pm->lmb_prev;
    pm->lmb_prev=lmb;
    if(!lmb) pm->active_slider=-1;

    /* Dark translucent overlay */
    hud_rect(h,0,0,W,H, 0,0,0,0.60f);

    float pw=320,ph=0,px=(W-pw)*0.5f;
    float bw=pw-40,bh=44,bx=px+20;
    float gap=12;

    if(pm->state==PM_ROOT){
        ph=5*bh+4*gap+80; float py=(H-ph)*0.5f;
        /* Panel */
        hud_rect(h,px,py,pw,ph, 0.06f,0.04f,0.02f,0.95f);
        hud_rect(h,px,py,pw,4,  0.8f,0.6f,0.1f,1); /* top gold bar */

        const char *title="PAUSED";
        float tsc=2.4f,tw=tsc*8.0f*(float)strlen(title);
        hud_str(h,px+(pw-tw)*0.5f,py+14,tsc,title,1.0f,0.85f,0.2f,1.0f);

        float by_=py+56;
        bool c=false;
        /* RESUME */
        menu_btn(h,bx,by_,bw,bh,"RESUME",1.8f,mx,my,true,&c);
        if(c&&clicked){pm->state=PM_NONE;*want_resume=true;audio_play(SND_MENU_CLICK,70);}
        by_+=bh+gap;
        /* SAVE GAME */
        menu_btn(h,bx,by_,bw,bh,"SAVE GAME",1.8f,mx,my,true,&c);
        if(c&&clicked){pm->state=PM_SAVE_SLOTS;audio_play(SND_MENU_CLICK,70);}
        by_+=bh+gap;
        /* OPTIONS */
        menu_btn(h,bx,by_,bw,bh,"OPTIONS",1.8f,mx,my,true,&c);
        if(c&&clicked){ pm->state=PM_OPTIONS; audio_play(SND_MENU_CLICK,70); }
        by_+=bh+gap;
        /* QUIT TO MENU */
        menu_btn(h,bx,by_,bw,bh,"QUIT TO MENU",1.8f,mx,my,true,&c);
        if(c&&clicked){pm->state=PM_NONE;*want_quit_menu=true;options_save(&g_options);audio_play(SND_MENU_CLICK,70);}
        by_+=bh+gap;
        /* QUIT GAME */
        menu_btn(h,bx,by_,bw,bh,"QUIT GAME",1.8f,mx,my,true,&c);
        if(c&&clicked){pm->state=PM_NONE;*want_quit_game=true;options_save(&g_options);audio_play(SND_MENU_CLICK,70);}

    } else if(pm->state==PM_SAVE_SLOTS){
        ph=3*bh+2*gap+100; float py=(H-ph)*0.5f;
        hud_rect(h,px,py,pw,ph, 0.06f,0.04f,0.02f,0.95f);
        hud_rect(h,px,py,pw,4,  0.8f,0.6f,0.1f,1);

        const char *title="SAVE TO SLOT";
        float tsc=1.8f,tw=tsc*8.0f*(float)strlen(title);
        hud_str(h,px+(pw-tw)*0.5f,py+14,tsc,title,1.0f,0.85f,0.2f,1.0f);

        float by_=py+52;
        for(int s=0;s<3;s++){
            char lbl[32];snprintf(lbl,sizeof(lbl),"SLOT %d",s+1);
            bool c2=false;
            menu_btn(h,bx,by_,bw,bh,lbl,1.8f,mx,my,true,&c2);
            if(c2&&clicked){
                game_save(s,player,qs,as,enemies,ne,world_time,inv,lvl);
                char nbuf[32];snprintf(nbuf,sizeof(nbuf),"Saved — Slot %d",s+1);
                save_notify(note,nbuf);
                audio_play(SND_MENU_CLICK,80);
                pm->state=PM_ROOT;
            }
            by_+=bh+gap;
        }
        /* BACK */
        bool cb=false;
        hud_str(h,px+10,by_+14,1.2f,"< BACK",0.7f,0.6f,0.3f,0.85f);
        bool over_back=(mx>=px&&mx<=px+80&&my>=by_+8&&my<=by_+8+20);
        if(over_back&&clicked){pm->state=PM_ROOT;audio_play(SND_MENU_CLICK,70);}
        (void)cb;

    } else if(pm->state==PM_OPTIONS){
        bool back_opt=false, credits_ign=false;
        Options opt_prev=g_options;
        page_options(h,(float)win_w,(float)win_h,mx,my,clicked,
                     &g_options,lmb,&pm->active_slider,&back_opt,&credits_ign);
        audio_set_volumes(g_options.master_vol,g_options.music_vol,g_options.sfx_vol);
        SDL_GL_SetSwapInterval(g_options.vsync?1:0);
        if(g_options.fullscreen!=opt_prev.fullscreen)
            SDL_SetWindowFullscreen(SDL_GL_GetCurrentWindow(),
                g_options.fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0);
        if(clicked&&back_opt){
            options_save(&g_options);
            pm->state=PM_ROOT;
            audio_play(SND_MENU_CLICK,70);
        }
        return true;
    }

    hud_flush(h);
    return true;
}

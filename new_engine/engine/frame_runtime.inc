static void frame_tail_runtime(HUD *hud, PauseMenu *pause,
                               Player *player, QuestSystem *qs,
                               AbilitySystem *as, Enemy *enemies, int ne,
                               float total_time,
                               Inventory *inv, LevelSystem *lvlsys,
                               SaveNotify *save_note,
                               bool *running, bool *restart_to_menu,
                               float *intro_fade, float dt,
                               int win_w, int win_h, SDL_Window *win)
{
    if(player->state==PS_DEAD){
        float cx=(float)win_w*0.5f, cy=(float)win_h*0.38f;
        hud_rect(hud,0,0,(float)win_w,(float)win_h,0,0,0,0.5f);
        hud_str(hud,cx-72,cy,3.5f,"YOU DIED",0.85f,0.05f,0.05f,1.0f);
        hud_str(hud,cx-108,cy+52,1.6f,"ENTER to Respawn",0.90f,0.90f,0.90f,0.85f);
        hud_flush(hud);
    }

    {
        bool want_resume=false, want_quit_menu=false, want_quit_game=false;
        pause_draw(pause,hud,win_w,win_h,
                   player,qs,as,enemies,ne,total_time,inv,lvlsys,save_note,
                   &want_resume,&want_quit_menu,&want_quit_game);
        if(want_quit_game) *running=false;
        if(want_quit_menu){
            *running=false;
            *restart_to_menu=true;
        }
    }

    if(*intro_fade>0.0f){
        *intro_fade=clampf(*intro_fade-dt/1.5f,0.0f,1.0f);
        glDisable(GL_DEPTH_TEST);
        hud_rect(hud,0,0,(float)win_w,(float)win_h,0,0,0,*intro_fade);
        hud_flush(hud);
        glEnable(GL_DEPTH_TEST);
    }

    {
        char title[256];
        snprintf(title,sizeof(title),"RPG | HP:%.0f | MP:%.0f | Gold:%d XP:%d",
                 (double)player->health,(double)as->mana,qs->player_gold,qs->player_xp);
        SDL_SetWindowTitle(win,title);
    }

    SDL_GL_SwapWindow(win);
}

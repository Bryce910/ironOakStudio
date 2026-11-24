static bool death_screen_draw(HUD *h, float *fade_t, float dt,
                              int win_w, int win_h,
                              float hp_max, int gold, int level,
                              int mx, int my);

static void readable_prompt_wrap_text(const char *src,char *dst,size_t dst_cap,int wrap_cols){
    int col=0,last_space=-1;
    size_t di=0;
    const char *s=src?src:"";
    if(!dst||dst_cap==0) return;
    while(*s&&di+1<dst_cap){
        char ch=*s++;
        if(ch=='\r') continue;
        if(ch=='\n'){
            dst[di++]='\n';
            col=0;
            last_space=-1;
            continue;
        }
        dst[di++]=ch;
        if(ch==' '||ch=='\t') last_space=(int)di-1;
        col++;
        if(col>=wrap_cols&&di+1<dst_cap){
            if(last_space>=0){
                dst[last_space]='\n';
                col=(int)di-last_space-1;
                last_space=-1;
            }else{
                dst[di++]='\n';
                col=0;
            }
        }
    }
    dst[di]=0;
}

static void readable_prompt_draw(HUD *hud,int win_w,int win_h){
    char wrapped[1024];
    float W=(float)win_w,H=(float)win_h;
    float pw=fminf(W*0.74f,760.0f),ph=fminf(H*0.46f,320.0f),px=(W-pw)*0.5f,py=(H-ph)*0.5f;
    float ix=px+14.0f,iy=py+14.0f,iw=pw-28.0f,ih=ph-28.0f;
    readable_prompt_wrap_text(g_readable_prompt.text,wrapped,sizeof(wrapped),46);
    hud_rect(hud,0,0,W,H,0.02f,0.015f,0.010f,0.72f);
    hud_rect(hud,px,py,pw,ph,0.10f,0.085f,0.065f,0.97f);
    hud_rect(hud,ix,iy,iw,ih,0.12f,0.10f,0.08f,0.92f);
    hud_rect(hud,px,py,pw,1.0f,0.30f,0.24f,0.14f,0.85f);
    hud_rect(hud,px,py+ph-1.0f,pw,1.0f,0.30f,0.24f,0.14f,0.65f);
    hud_rect(hud,px,py,1.0f,ph,0.30f,0.24f,0.14f,0.55f);
    hud_rect(hud,px+pw-1.0f,py,1.0f,ph,0.30f,0.24f,0.14f,0.55f);
    hud_rect(hud,ix,iy,iw,1.0f,0.24f,0.19f,0.11f,0.55f);
    hud_rect(hud,ix,iy+ih-1.0f,iw,1.0f,0.24f,0.19f,0.11f,0.40f);
    hud_rect(hud,ix,iy,1.0f,ih,0.24f,0.19f,0.11f,0.32f);
    hud_rect(hud,ix+iw-1.0f,iy,1.0f,ih,0.24f,0.19f,0.11f,0.32f);
    hud_str(hud,ix+28.0f,iy+30.0f,1.0f,"[ARCHIVE ENTRY]",0.50f,0.52f,0.58f,0.92f);
    hud_str(hud,ix+28.0f,iy+62.0f,2.3f,"THE FIRST EMBER",0.88f,0.68f,0.34f,1.0f);
    hud_rect(hud,ix+28.0f,iy+88.0f,iw-56.0f,1.0f,0.36f,0.28f,0.16f,0.45f);
    hud_str(hud,ix+28.0f,iy+112.0f,1.20f,wrapped,0.92f,0.88f,0.82f,1.0f);
    hud_str(hud,ix+28.0f,py+ph-40.0f,1.05f,"PRESS E OR ESC TO CLOSE",0.86f,0.68f,0.32f,0.96f);
}

static float g_campfire_menu_scroll=0.0f;
static bool g_campfire_menu_prev_open=false;
static bool g_campfire_menu_scroll_drag=false;
static float g_campfire_menu_scroll_drag_grab=0.0f;

static void ui_runtime_draw(HUD *hud, LineRenderer *lr, PerfCounter *perf,
                            SaveUI *save_ui, SaveNotify *save_note,
                            Player *player, AnimMap *player_amap,
                            AnimMap *enemy_amaps,
                            AbilitySystem *as, QuestSystem *qs,
                            Enemy *enemies, int ne, Inventory *inv,
                            CookingSystem *cooking, ParticleSystem *ps,
                            LevelSystem *lvlsys, float *total_time,
                            float *base_max_hp, float *death_fade, float dt,
                            bool *campfire_menu_open, bool *campfire_lmb_prev,
                            bool show_debug, bool prop_edit, int prop_sel,
                            ProjList *projs, CharModel *char_model,
                            Camera3P *cam, Terrain *terrain,
                            Mat4 proj, Mat4 view, Vec3 cf_pos,
                            float cf_wx, float cf_wz,
                            int win_w, int win_h){
    if(player->state==PS_DEAD){
        int dmx,dmy;
        SDL_GetMouseState(&dmx,&dmy);
        if(death_screen_draw(hud,death_fade,dt,win_w,win_h,
                             player->max_health,qs->player_gold,qs->player_level,
                             dmx,dmy)){
            player->health=player->max_health;
            player->state=PS_IDLE;
            player->anim_idx=player_amap->idle;
            player->anim_time=0;
            player->hit_flash=0;
            player->phys.pos=(Vec3){cf_wx,terrain_h(cf_wx,cf_wz)+0.1f,cf_wz};
            player->phys.vel=(Vec3){0,0,0};
            *death_fade=0.0f;
            player->ooc_timer=6.0f;
        }
    }

    if(g_scene_sys.current==SCENE_SHIRE){
        farming_world_draw(&g_farming,lr,proj,view,*total_time);
        farming_draw(hud,&g_farming,inv,proj,view,win_w,win_h);
    }

    if(g_scene_sys.current!=SCENE_SHIRE){
        g_farm_crate_open=false;
    }else if(g_farm_crate_open&&vec3_dist(player->phys.pos,g_farm_crate_pos)>4.2f){
        g_farm_crate_open=false;
    }

    if(g_scene_sys.current==SCENE_SHIRE&&player->state!=PS_DEAD&&!g_farm_crate_open&&!*campfire_menu_open&&!cooking->active){
        float fcd=vec3_dist(player->phys.pos,g_farm_crate_pos);
        if(fcd<5.2f){
            float alpha=clampf(1.0f-(fcd-2.4f)/2.8f,0.28f,1.0f);
            const char *hint="[E] Open Farm Crate";
            float sc=1.15f;
            float bw=fmaxf(260.0f,sc*8.0f*(float)strlen(hint)+64.0f);
            float bh=56.0f;
            float bx=(float)win_w*0.5f-bw*0.5f;
            float by=(float)win_h*0.68f-bh*0.5f;
            hud_rect(hud,bx,by,bw,bh,0.10f,0.085f,0.065f,0.88f*alpha);
            hud_rect(hud,bx+10.0f,by+10.0f,bw-20.0f,bh-20.0f,0.12f,0.10f,0.08f,0.78f*alpha);
            hud_rect(hud,bx,by,bw,1.0f,0.34f,0.27f,0.16f,0.85f*alpha);
            hud_rect(hud,bx,by+bh-1.0f,bw,1.0f,0.34f,0.27f,0.16f,0.55f*alpha);
            hud_str(hud,bx+20.0f,by+16.0f,0.92f,"[STOREHOUSE]",0.48f,0.52f,0.58f,0.84f*alpha);
            hud_str(hud,bx+20.0f,by+32.0f,sc,hint,0.90f,0.74f,0.38f,alpha);
            hud_flush(hud);
        }
    }

    if(g_scene_sys.current==SCENE_SHIRE&&player->state!=PS_DEAD&&!*campfire_menu_open&&!cooking->active){
        float cfd=vec3_dist(player->phys.pos,cf_pos);
        if(cfd<6.0f){
            float alpha=clampf(1.0f-(cfd-3.0f)/3.0f,0.3f,1.0f);
            const char *hint="[E] Rest / Cook at Campfire";
            float sc=1.18f;
            float bw=fmaxf(318.0f,sc*8.0f*(float)strlen(hint)+64.0f);
            float bh=56.0f;
            float bx=(float)win_w*0.5f-bw*0.5f;
            float by=(float)win_h*0.63f-bh*0.5f;
            hud_rect(hud,bx,by,bw,bh,0.10f,0.085f,0.065f,0.90f*alpha);
            hud_rect(hud,bx+10.0f,by+10.0f,bw-20.0f,bh-20.0f,0.12f,0.10f,0.08f,0.80f*alpha);
            hud_rect(hud,bx,by,bw,1.0f,0.34f,0.27f,0.16f,0.85f*alpha);
            hud_rect(hud,bx,by+bh-1.0f,bw,1.0f,0.34f,0.27f,0.16f,0.55f*alpha);
            hud_str(hud,bx+20.0f,by+16.0f,0.92f,"[CAMPFIRE]",0.48f,0.52f,0.58f,0.84f*alpha);
            hud_str(hud,bx+20.0f,by+32.0f,sc,hint,0.90f,0.74f,0.38f,alpha);
            hud_flush(hud);
        }
    }

    if(player->state!=PS_DEAD&&!inv->open&&!g_farm_crate_open&&!*campfire_menu_open&&!cooking->active){
        bool mount_prompt=mount_system_draw_prompt(&g_mounts,hud,player->phys.pos,win_w,win_h);
        if(!mount_prompt) prop_interact_draw_hud(&g_props,hud,player->phys.pos,win_w,win_h);
    }

    if(g_readable_prompt.open&&player->state!=PS_DEAD){
        readable_prompt_draw(hud,win_w,win_h);
        hud_flush(hud);
        return;
    }

    if(g_scene_sys.current==SCENE_SHIRE&&g_farm_crate_open&&player->state!=PS_DEAD){
        float W=(float)win_w,H=(float)win_h;
        int cmx,cmy;
        Uint32 cmb=SDL_GetMouseState(&cmx,&cmy);
        bool lmb_now=(cmb&SDL_BUTTON(1))!=0;
        bool clicked=lmb_now&&!(*campfire_lmb_prev);
        *campfire_lmb_prev=lmb_now;

        int turnips=storage_count_item(g_farm_storage,FARM_STORAGE_SLOTS,ITEM_TURNIP);
        int carrots=storage_count_item(g_farm_storage,FARM_STORAGE_SLOTS,ITEM_CARROT);
        int cabbage=storage_count_item(g_farm_storage,FARM_STORAGE_SLOTS,ITEM_CABBAGE);
        int pumpkins=storage_count_item(g_farm_storage,FARM_STORAGE_SLOTS,ITEM_PUMPKIN);
        int stored_total=turnips+carrots+cabbage+pumpkins;
        int ship_value=turnips*ITEM_PRICE[ITEM_TURNIP]
                      +carrots*ITEM_PRICE[ITEM_CARROT]
                      +cabbage*ITEM_PRICE[ITEM_CABBAGE]
                      +pumpkins*ITEM_PRICE[ITEM_PUMPKIN];

        hud_rect(hud,0,0,W,H,0.0f,0.0f,0.0f,0.45f);
        {
            float pw=332,bw=280,bh=48,gap=10;
            float ph=4*bh+3*gap+130;
            float px=(W-pw)*0.5f, py=(H-ph)*0.5f;
            float bx=px+(pw-bw)*0.5f, by=py+78;
            bool hov=false;
            hud_rect(hud,px,py,pw,ph,0.06f,0.04f,0.02f,0.96f);
            hud_rect(hud,px,py,pw,3,0.86f,0.68f,0.32f,1.0f);
            {
                const char *title="FARM CRATE";
                float tw=2.0f*8.0f*(float)strlen(title);
                hud_str(hud,px+(pw-tw)*0.5f,py+12,2.0f,title,0.96f,0.84f,0.50f,1.0f);
                {
                    char stats[128];
                    snprintf(stats,sizeof(stats),"Stored %d   Ship Value %dG",stored_total,ship_value);
                    float sw=1.1f*8.0f*(float)strlen(stats);
                    hud_str(hud,px+(pw-sw)*0.5f,py+36,1.1f,stats,0.84f,0.90f,0.78f,0.95f);
                }
                {
                    char crops[160];
                    snprintf(crops,sizeof(crops),"Turnip %d  Carrot %d  Cabbage %d  Pumpkin %d",
                             turnips,carrots,cabbage,pumpkins);
                    float cw=1.0f*8.0f*(float)strlen(crops);
                    hud_str(hud,px+(pw-cw)*0.5f,py+54,1.0f,crops,0.72f,0.84f,0.88f,0.92f);
                }
            }

            hov=(cmx>=bx&&cmx<=bx+bw&&cmy>=by&&cmy<=by+bh);
            hud_rect(hud,bx,by,bw,bh,hov?0.16f:0.09f,hov?0.14f:0.07f,hov?0.09f:0.04f,0.94f);
            hud_rect(hud,bx,by,bw,2,0.58f,0.78f,0.92f,1.0f);
            {
                const char *l="STORE CROPS";
                float lw=1.45f*8.0f*(float)strlen(l);
                hud_str(hud,bx+(bw-lw)*0.5f,by+(bh-14)*0.5f,1.45f,l,0.86f,0.94f,0.98f,1.0f);
            }
            if(hov&&clicked){
                int moved=farm_storage_deposit_crops(inv);
                if(moved>0){
                    char buf[64]; snprintf(buf,sizeof(buf),"Stored %d crops",moved);
                    save_notify(save_note,buf);
                }else save_notify(save_note,"No crops to store");
            }
            by+=bh+gap;

            hov=(cmx>=bx&&cmx<=bx+bw&&cmy>=by&&cmy<=by+bh);
            hud_rect(hud,bx,by,bw,bh,hov?0.16f:0.09f,hov?0.14f:0.07f,hov?0.09f:0.04f,0.94f);
            hud_rect(hud,bx,by,bw,2,0.68f,0.90f,0.72f,1.0f);
            {
                const char *l="WITHDRAW STORAGE";
                float lw=1.35f*8.0f*(float)strlen(l);
                hud_str(hud,bx+(bw-lw)*0.5f,by+(bh-14)*0.5f,1.35f,l,0.90f,0.98f,0.84f,1.0f);
            }
            if(hov&&clicked){
                int moved=farm_storage_withdraw_all(inv);
                if(moved>0){
                    char buf[64]; snprintf(buf,sizeof(buf),"Withdrew %d crops",moved);
                    save_notify(save_note,buf);
                }else save_notify(save_note,"Storage is empty");
            }
            by+=bh+gap;

            hov=(cmx>=bx&&cmx<=bx+bw&&cmy>=by&&cmy<=by+bh);
            hud_rect(hud,bx,by,bw,bh,hov?0.18f:0.10f,hov?0.14f:0.08f,hov?0.07f:0.04f,0.94f);
            hud_rect(hud,bx,by,bw,2,0.96f,0.74f,0.26f,1.0f);
            {
                char l[64];
                snprintf(l,sizeof(l),"SHIP CROPS  [%dG]",ship_value);
                float lw=1.35f*8.0f*(float)strlen(l);
                hud_str(hud,bx+(bw-lw)*0.5f,by+8,1.35f,l,0.98f,0.92f,0.74f,1.0f);
                hud_str(hud,bx+40,by+24,1.0f,"Convert stored produce into gold",0.88f,0.78f,0.58f,0.95f);
            }
            if(hov&&clicked){
                int gold=farm_storage_ship_all(qs);
                if(gold>0){
                    char buf[64]; snprintf(buf,sizeof(buf),"Shipment sent: +%d gold",gold);
                    save_notify(save_note,buf);
                }else save_notify(save_note,"No stored crops to ship");
            }
            by+=bh+gap;

            hov=(cmx>=bx&&cmx<=bx+bw&&cmy>=by&&cmy<=by+bh);
            hud_rect(hud,bx,by,bw,bh,hov?0.14f:0.08f,hov?0.12f:0.06f,hov?0.12f:0.05f,0.92f);
            hud_rect(hud,bx,by,bw,2,0.72f,0.72f,0.78f,1.0f);
            {
                const char *l="CLOSE";
                float lw=1.5f*8.0f*(float)strlen(l);
                hud_str(hud,bx+(bw-lw)*0.5f,by+(bh-14)*0.5f,1.5f,l,0.92f,0.92f,0.96f,1.0f);
            }
            if(hov&&clicked){
                g_farm_crate_open=false;
                audio_play(SND_MENU_CLICK,70);
            }
        }
        if(SDL_GetKeyboardState(NULL)[SDL_SCANCODE_ESCAPE]) g_farm_crate_open=false;
    }

    if(*campfire_menu_open&&!g_campfire_menu_prev_open){
        g_campfire_menu_scroll=0.0f;
        g_campfire_menu_scroll_drag=false;
    }
    g_campfire_menu_prev_open=*campfire_menu_open;

    if(*campfire_menu_open&&player->state!=PS_DEAD){
        float W=(float)win_w,H=(float)win_h;
        int cmx,cmy;
        Uint32 cmb=SDL_GetMouseState(&cmx,&cmy);
        bool lmb_now=(cmb&SDL_BUTTON(1))!=0;
        bool clicked=lmb_now&&!(*campfire_lmb_prev);
        *campfire_lmb_prev=lmb_now;

        hud_rect(hud,0,0,W,H,0.0f,0.0f,0.0f,0.55f);
        {
            const float pw=380.0f;
            const float bw=308.0f;
            const float bh=46.0f;
            const float gap=10.0f;
            const float header_h=62.0f;
            const float footer_h=28.0f;
            const float track_w=12.0f;
            const float content_gap=14.0f;
            const int item_count=12;
            const float content_h=item_count*bh+(item_count-1)*gap;
            float ph=fminf(H*0.82f,720.0f);
            float px=(W-pw)*0.5f, py=(H-ph)*0.5f;
            float view_x=px+24.0f;
            float view_y=py+header_h;
            float view_w=bw;
            float view_h=ph-header_h-footer_h;
            float track_x=view_x+view_w+content_gap;
            float max_scroll=fmaxf(0.0f,content_h-view_h);
            float thumb_h=max_scroll>0.0f?fmaxf(34.0f,view_h*(view_h/content_h)):view_h;
            float thumb_travel=fmaxf(0.0f,view_h-thumb_h);
            float thumb_y=view_y+(max_scroll>0.0f?(g_campfire_menu_scroll/max_scroll)*thumb_travel:0.0f);
            bool hov=false;

            g_campfire_menu_scroll=clampf(g_campfire_menu_scroll,0.0f,max_scroll);
            thumb_y=view_y+(max_scroll>0.0f?(g_campfire_menu_scroll/max_scroll)*thumb_travel:0.0f);

            if(!lmb_now) g_campfire_menu_scroll_drag=false;
            if(clicked&&max_scroll>0.0f&&cmx>=track_x&&cmx<=track_x+track_w&&cmy>=view_y&&cmy<=view_y+view_h){
                if(cmy>=thumb_y&&cmy<=thumb_y+thumb_h){
                    g_campfire_menu_scroll_drag=true;
                    g_campfire_menu_scroll_drag_grab=(float)cmy-thumb_y;
                }else{
                    float target=((float)cmy-view_y-thumb_h*0.5f)/fmaxf(1.0f,thumb_travel);
                    g_campfire_menu_scroll=clampf(target,0.0f,1.0f)*max_scroll;
                    thumb_y=view_y+(g_campfire_menu_scroll/max_scroll)*thumb_travel;
                    g_campfire_menu_scroll_drag=true;
                    g_campfire_menu_scroll_drag_grab=thumb_h*0.5f;
                }
            }
            if(g_campfire_menu_scroll_drag&&lmb_now&&max_scroll>0.0f){
                float target=((float)cmy-view_y-g_campfire_menu_scroll_drag_grab)/fmaxf(1.0f,thumb_travel);
                g_campfire_menu_scroll=clampf(target,0.0f,1.0f)*max_scroll;
                thumb_y=view_y+(g_campfire_menu_scroll/max_scroll)*thumb_travel;
            }

            hud_rect(hud,px,py,pw,ph,0.07f,0.04f,0.01f,0.96f);
            hud_rect(hud,px,py,pw,3,1.0f,0.45f,0.1f,1.0f);
            {
                const char *title="CAMPFIRE";
                float tsc=2.2f, tw=tsc*8.0f*(float)strlen(title);
                hud_str(hud,px+(pw-tw)*0.5f,py+12,tsc,title,1.0f,0.55f,0.15f,1.0f);
                {
                    const char *season=farm_season_name(farm_current_season());
                    float sw=1.15f*8.0f*(float)strlen(season);
                    hud_str(hud,px+(pw-sw)*0.5f,py+36,1.15f,season,0.82f,0.92f,0.66f,0.95f);
                }
            }
            hud_rect(hud,view_x-2.0f,view_y-2.0f,view_w+4.0f,view_h+4.0f,0.11f,0.08f,0.03f,0.50f);

            for(int item_idx=0;item_idx<item_count;item_idx++){
                float by=view_y+(float)item_idx*(bh+gap)-g_campfire_menu_scroll;
                float bx=view_x;
                if(by+bh<view_y||by>view_y+view_h) continue;
                hov=(cmx>=bx&&cmx<=bx+bw&&cmy>=by&&cmy<=by+bh);

                if(item_idx==0){
                    hud_rect(hud,bx,by,bw,bh,hov?0.25f:0.12f,hov?0.15f:0.08f,hov?0.06f:0.03f,0.92f);
                    hud_rect(hud,bx,by,bw,2,0.8f,0.4f,0.1f,1.0f);
                    {
                        const char *l="REST (Restore HP & MP)";
                        float lw=1.5f*8.0f*(float)strlen(l);
                        hud_str(hud,bx+(bw-lw)*0.5f,by+(bh-14)*0.5f,1.5f,l,1.0f,0.9f,0.7f,1.0f);
                    }
                    if(hov&&clicked){
                        float cur_frac=fmodf(*total_time,DAY_DURATION)/DAY_DURATION;
                        float target_frac=0.28f;
                        float skip=(target_frac>cur_frac)
                            ?(target_frac-cur_frac)*DAY_DURATION
                            :(1.0f-(cur_frac-target_frac))*DAY_DURATION;
                        player->health=player->max_health;
                        as->mana=as->max_mana;
                        *total_time+=skip;
                        save_notify(save_note,"Rested — HP and MP restored. Morning.");
                        audio_play(SND_MENU_CLICK,80);
                        *campfire_menu_open=false;
                    }
                }else if(item_idx==1){
                    hud_rect(hud,bx,by,bw,bh,hov?0.22f:0.11f,hov?0.16f:0.08f,hov?0.07f:0.03f,0.92f);
                    hud_rect(hud,bx,by,bw,2,0.92f,0.58f,0.16f,1.0f);
                    {
                        char l[64];
                        snprintf(l,sizeof(l),"COOK RAW FISH  [%d]",inv_count_item(inv,ITEM_FISH_RAW));
                        float lw=1.35f*8.0f*(float)strlen(l);
                        hud_str(hud,bx+(bw-lw)*0.5f,by+8,1.35f,l,1.0f,0.90f,0.72f,1.0f);
                        hud_str(hud,bx+30,by+24,1.05f,"-> Cooked Fish",0.84f,0.74f,0.56f,0.95f);
                    }
                    if(hov&&clicked){
                        if(inv_count_item(inv,ITEM_FISH_RAW)>0){
                            cooking_begin(cooking,ITEM_FISH_RAW,ITEM_MEAL_FISH,ITEM_MEAL_BURNT_FISH);
                            audio_play(SND_MENU_CLICK,80);
                        }else{
                            save_notify(save_note,"Need Raw Fish to cook");
                        }
                    }
                }else if(item_idx==2){
                    hud_rect(hud,bx,by,bw,bh,hov?0.22f:0.11f,hov?0.16f:0.08f,hov?0.07f:0.03f,0.92f);
                    hud_rect(hud,bx,by,bw,2,0.92f,0.58f,0.16f,1.0f);
                    {
                        char l[64];
                        snprintf(l,sizeof(l),"SEAR TROUT  [%d]",inv_count_item(inv,ITEM_FISH_TROUT));
                        float lw=1.35f*8.0f*(float)strlen(l);
                        hud_str(hud,bx+(bw-lw)*0.5f,by+8,1.35f,l,1.0f,0.90f,0.72f,1.0f);
                        hud_str(hud,bx+30,by+24,1.05f,"-> Seared Trout",0.84f,0.74f,0.56f,0.95f);
                    }
                    if(hov&&clicked){
                        if(inv_count_item(inv,ITEM_FISH_TROUT)>0){
                            cooking_begin(cooking,ITEM_FISH_TROUT,ITEM_MEAL_TROUT,ITEM_MEAL_BURNT_TROUT);
                            audio_play(SND_MENU_CLICK,80);
                        }else{
                            save_notify(save_note,"Need River Trout to cook");
                        }
                    }
                }else if(item_idx==3){
                    hud_rect(hud,bx,by,bw,bh,hov?0.24f:0.12f,hov?0.18f:0.09f,hov?0.09f:0.04f,0.92f);
                    hud_rect(hud,bx,by,bw,2,0.98f,0.76f,0.22f,1.0f);
                    {
                        char l[64];
                        snprintf(l,sizeof(l),"GOLDEN FEAST  [%d]",inv_count_item(inv,ITEM_FISH_GOLDEN));
                        float lw=1.35f*8.0f*(float)strlen(l);
                        hud_str(hud,bx+(bw-lw)*0.5f,by+8,1.35f,l,1.0f,0.95f,0.76f,1.0f);
                        hud_str(hud,bx+26,by+24,1.05f,"-> Golden Feast",0.90f,0.82f,0.58f,0.95f);
                    }
                    if(hov&&clicked){
                        if(inv_count_item(inv,ITEM_FISH_GOLDEN)>0){
                            cooking_begin(cooking,ITEM_FISH_GOLDEN,ITEM_MEAL_GOLDEN,ITEM_MEAL_BURNT_FEAST);
                            audio_play(SND_MENU_CLICK,80);
                        }else{
                            save_notify(save_note,"Need Golden Koi to cook");
                        }
                    }
                }else if(item_idx==4){
                    hud_rect(hud,bx,by,bw,bh,hov?0.22f:0.11f,hov?0.16f:0.08f,hov?0.07f:0.03f,0.92f);
                    hud_rect(hud,bx,by,bw,2,0.54f,0.86f,0.42f,1.0f);
                    {
                        char l[64];
                        snprintf(l,sizeof(l),"GARDEN STEW  [%d]",inv_count_item(inv,ITEM_CABBAGE));
                        float lw=1.35f*8.0f*(float)strlen(l);
                        hud_str(hud,bx+(bw-lw)*0.5f,by+8,1.35f,l,0.94f,0.98f,0.80f,1.0f);
                        hud_str(hud,bx+34,by+24,1.05f,"-> Garden Stew",0.74f,0.92f,0.70f,0.95f);
                    }
                    if(hov&&clicked){
                        if(inv_count_item(inv,ITEM_CABBAGE)>0){
                            cooking_begin(cooking,ITEM_CABBAGE,ITEM_MEAL_STEW,ITEM_MEAL_BURNT_FEAST);
                            audio_play(SND_MENU_CLICK,80);
                        }else{
                            save_notify(save_note,"Need Cabbage to cook");
                        }
                    }
                }else if(item_idx==5){
                    hud_rect(hud,bx,by,bw,bh,hov?0.24f:0.12f,hov?0.18f:0.09f,hov?0.08f:0.04f,0.92f);
                    hud_rect(hud,bx,by,bw,2,0.96f,0.62f,0.20f,1.0f);
                    {
                        char l[64];
                        snprintf(l,sizeof(l),"ROAST ROOTS  [%d]",inv_count_item(inv,ITEM_PUMPKIN));
                        float lw=1.35f*8.0f*(float)strlen(l);
                        hud_str(hud,bx+(bw-lw)*0.5f,by+8,1.35f,l,0.98f,0.92f,0.78f,1.0f);
                        hud_str(hud,bx+28,by+24,1.05f,"-> Roast Roots",0.92f,0.76f,0.52f,0.95f);
                    }
                    if(hov&&clicked){
                        if(inv_count_item(inv,ITEM_PUMPKIN)>0){
                            cooking_begin(cooking,ITEM_PUMPKIN,ITEM_MEAL_ROAST_ROOTS,ITEM_MEAL_BURNT_FEAST);
                            audio_play(SND_MENU_CLICK,80);
                        }else{
                            save_notify(save_note,"Need Pumpkin to cook");
                        }
                    }
                }else if(item_idx==6){
                    hud_rect(hud,bx,by,bw,bh,hov?0.20f:0.10f,hov?0.16f:0.08f,hov?0.06f:0.03f,0.92f);
                    hud_rect(hud,bx,by,bw,2,0.58f,0.78f,0.92f,1.0f);
                    {
                        const char *l="STORE CROPS";
                        float lw=1.45f*8.0f*(float)strlen(l);
                        hud_str(hud,bx+(bw-lw)*0.5f,by+(bh-14)*0.5f,1.45f,l,0.86f,0.94f,0.98f,1.0f);
                    }
                    if(hov&&clicked){
                        int moved=farm_storage_deposit_crops(inv);
                        if(moved>0){
                            char buf[64]; snprintf(buf,sizeof(buf),"Stored %d crops",moved);
                            save_notify(save_note,buf);
                        }else save_notify(save_note,"No crops to store");
                    }
                }else if(item_idx==7){
                    hud_rect(hud,bx,by,bw,bh,hov?0.20f:0.10f,hov?0.16f:0.08f,hov?0.06f:0.03f,0.92f);
                    hud_rect(hud,bx,by,bw,2,0.68f,0.90f,0.72f,1.0f);
                    {
                        const char *l="WITHDRAW STORAGE";
                        float lw=1.35f*8.0f*(float)strlen(l);
                        hud_str(hud,bx+(bw-lw)*0.5f,by+(bh-14)*0.5f,1.35f,l,0.90f,0.98f,0.84f,1.0f);
                    }
                    if(hov&&clicked){
                        int moved=farm_storage_withdraw_all(inv);
                        if(moved>0){
                            char buf[64]; snprintf(buf,sizeof(buf),"Withdrew %d items",moved);
                            save_notify(save_note,buf);
                        }else save_notify(save_note,"Storage is empty or inventory is full");
                    }
                }else if(item_idx==8){
                    hud_rect(hud,bx,by,bw,bh,hov?0.22f:0.11f,hov?0.18f:0.09f,hov?0.08f:0.04f,0.92f);
                    hud_rect(hud,bx,by,bw,2,0.98f,0.78f,0.26f,1.0f);
                    {
                        const char *l="SHIP STORED CROPS";
                        float lw=1.30f*8.0f*(float)strlen(l);
                        hud_str(hud,bx+(bw-lw)*0.5f,by+(bh-14)*0.5f,1.30f,l,1.0f,0.95f,0.76f,1.0f);
                    }
                    if(hov&&clicked){
                        int gold=farm_storage_ship_all(qs);
                        if(gold>0){
                            char buf[64]; snprintf(buf,sizeof(buf),"Shipped crops for %d gold",gold);
                            save_notify(save_note,buf);
                        }else save_notify(save_note,"No stored crops to ship");
                    }
                }else if(item_idx==9){
                    hud_rect(hud,bx,by,bw,bh,hov?0.25f:0.12f,hov?0.18f:0.09f,hov?0.08f:0.04f,0.92f);
                    hud_rect(hud,bx,by,bw,2,0.8f,0.4f,0.1f,1.0f);
                    {
                        const char *l="SAVE GAME";
                        float lw=1.5f*8.0f*(float)strlen(l);
                        hud_str(hud,bx+(bw-lw)*0.5f,by+(bh-14)*0.5f,1.5f,l,1.0f,0.9f,0.7f,1.0f);
                    }
                    if(hov&&clicked){
                        game_save(0,player,qs,as,enemies,ne,*total_time,inv,lvlsys);
                        save_notify(save_note,"Saved at campfire.");
                        audio_play(SND_MENU_CLICK,80);
                    }
                }else if(item_idx==10){
                    hud_rect(hud,bx,by,bw,bh,hov?0.25f:0.12f,hov?0.18f:0.09f,hov?0.08f:0.04f,0.92f);
                    hud_rect(hud,bx,by,bw,2,0.8f,0.4f,0.1f,1.0f);
                    {
                        const char *l="SET AS RESPAWN POINT";
                        float lw=1.5f*8.0f*(float)strlen(l);
                        hud_str(hud,bx+(bw-lw)*0.5f,by+(bh-14)*0.5f,1.5f,l,1.0f,0.9f,0.7f,1.0f);
                    }
                    if(hov&&clicked){
                        save_notify(save_note,"Respawn point set to this campfire.");
                        audio_play(SND_MENU_CLICK,80);
                    }
                }else{
                    hud_rect(hud,bx,by,bw,bh,hov?0.20f:0.10f,hov?0.08f:0.04f,hov?0.04f:0.02f,0.92f);
                    hud_rect(hud,bx,by,bw,2,0.5f,0.2f,0.05f,1.0f);
                    {
                        const char *l="LEAVE";
                        float lw=1.5f*8.0f*(float)strlen(l);
                        hud_str(hud,bx+(bw-lw)*0.5f,by+(bh-14)*0.5f,1.5f,l,0.8f,0.7f,0.6f,1.0f);
                    }
                    if(hov&&clicked){*campfire_menu_open=false;audio_play(SND_MENU_CLICK,70);}
                }
            }

            hud_rect(hud,track_x,view_y,track_w,view_h,0.08f,0.08f,0.05f,0.90f);
            hud_rect(hud,track_x+1.0f,thumb_y,track_w-2.0f,thumb_h,
                     max_scroll>0.0f?(g_campfire_menu_scroll_drag?0.82f:0.66f):0.26f,
                     max_scroll>0.0f?(g_campfire_menu_scroll_drag?0.72f:0.60f):0.26f,
                     max_scroll>0.0f?(g_campfire_menu_scroll_drag?0.28f:0.20f):0.18f,
                     0.96f);
            {
                const char *hint=max_scroll>0.0f?"Scroll or drag the bar":"";
                if(hint[0]){
                    float hw=0.95f*8.0f*(float)strlen(hint);
                    hud_str(hud,px+(pw-hw)*0.5f,py+ph-footer_h+6.0f,0.95f,hint,0.80f,0.76f,0.60f,0.88f);
                }
            }
        }
        hud_flush(hud);
        if(SDL_GetKeyboardState(NULL)[SDL_SCANCODE_ESCAPE]){
            *campfire_menu_open=false;
            g_campfire_menu_scroll_drag=false;
        }
    }

    if(cooking->active&&player->state!=PS_DEAD){
        cooking_world_draw(cooking,lr,proj,view,cf_pos,*total_time);
        float W=(float)win_w,H=(float)win_h;
        float pw=460,ph=170,px=(W-pw)*0.5f,py=H*0.72f-ph*0.5f;
        float bar_x=px+34,bar_y=py+84,bar_w=pw-68,bar_h=18;
        int cmx,cmy;
        Uint32 cmb=SDL_GetMouseState(&cmx,&cmy);
        bool lmb_now=(cmb&SDL_BUTTON(1))!=0;
        bool clicked=lmb_now&&!(*campfire_lmb_prev);
        *campfire_lmb_prev=lmb_now;

        cooking_update(cooking,dt);

        hud_rect(hud,px,py,pw,ph,0.06f,0.03f,0.01f,0.95f);
        hud_rect(hud,px,py,pw,3,0.98f,0.56f,0.12f,1.0f);
        hud_str(hud,px+118,py+12,2.0f,"COOKING",1.0f,0.72f,0.24f,1.0f);
        hud_str(hud,px+34,py+42,1.2f,"Click while the marker is in the gold zone",0.88f,0.82f,0.74f,0.95f);
        hud_rect(hud,bar_x,bar_y,bar_w,bar_h,0.10f,0.08f,0.05f,1.0f);
        {
            float good_x=bar_x+(cooking->good_center-cooking->good_width*0.5f)*bar_w;
            float hot_x=bar_x+cooking->overcook_line*bar_w;
            float cur_x=bar_x+cooking->cursor*bar_w;
            hud_rect(hud,good_x,bar_y,cooking->good_width*bar_w,bar_h,0.95f,0.78f,0.18f,0.95f);
            hud_rect(hud,hot_x,bar_y,bar_w-(hot_x-bar_x),bar_h,0.50f,0.12f,0.04f,0.55f);
            hud_rect(hud,cur_x-3,bar_y-6,6,bar_h+12,1.0f,0.98f,0.90f,1.0f);
        }
        hud_str(hud,bar_x,bar_y+30,1.1f,"Undercooked",0.62f,0.70f,0.86f,0.85f);
        hud_str(hud,bar_x+bar_w*0.38f,bar_y+30,1.1f,"Perfect",0.95f,0.86f,0.34f,0.95f);
        hud_str(hud,bar_x+bar_w*0.82f,bar_y+30,1.1f,"Overcooked",0.90f,0.34f,0.22f,0.95f);
        hud_str(hud,px+34,py+126,1.15f,IDEF[cooking->raw_type].name,IDEF[cooking->raw_type].r,IDEF[cooking->raw_type].g,IDEF[cooking->raw_type].b,0.95f);
        hud_str(hud,px+250,py+126,1.1f,"ESC cancels",0.55f,0.55f,0.55f,0.85f);

        if(clicked){
            if(cooking_finish(cooking,inv,save_note,ps,cf_pos)) audio_play(SND_MENU_CLICK,85);
        }
        if(SDL_GetKeyboardState(NULL)[SDL_SCANCODE_ESCAPE]){
            cooking_cancel(cooking);
        }
        hud_flush(hud);
    }

    if(save_ui->open){
        static bool saveui_lmb_prev=false;
        int smx,smy;
        int sv_slot=-1,ld_slot=-1;
        SDL_GetMouseState(&smx,&smy);
        saveui_draw(save_ui,hud,win_w,win_h,smx,smy,&sv_slot,&ld_slot);
        hud_flush(hud);
        {
            bool saveui_lmb=(SDL_GetMouseState(NULL,NULL)&SDL_BUTTON(1))!=0;
            if(!saveui_lmb&&saveui_lmb_prev){
                if(sv_slot>=0){
                    char nb[48];
                    game_save(sv_slot,player,qs,as,enemies,ne,*total_time,inv,lvlsys);
                    snprintf(nb,sizeof(nb),"Saved — Slot %d",sv_slot+1);
                    save_notify(save_note,nb);
                    audio_play(SND_MENU_CLICK,80);
                }
                if(ld_slot>=0){
                    SaveData load_probe;
                    if(save_read_slot(ld_slot,&load_probe)){
                        PlayerClass load_class=player_class_clamp((int)load_probe.player_class);
                        if(load_class!=player->class_id){
                            charmodel_free(char_model);
                            charmodel_init(char_model,g_chardefs[load_class].fbx_path);
                        }
                    }
                    if(game_load(ld_slot,player,qs,as,enemies,ne,total_time,player_amap,enemy_amaps,inv,base_max_hp,lvlsys)){
                        char nb[48];
                        snprintf(nb,sizeof(nb),"Loaded — Slot %d",ld_slot+1);
                        save_notify(save_note,nb);
                        audio_play(SND_MENU_CLICK,80);
                        inv_recalc(inv,player,*base_max_hp);
                        save_ui->open=false;
                    }else{
                        save_notify(save_note,"Slot empty — nothing to load");
                    }
                }
            }
            saveui_lmb_prev=saveui_lmb;
        }
    }

    if(g_skilltree.open){
        skilltree_draw(&g_skilltree,hud,win_w,win_h,player);
        hud_flush(hud);
    }
    if(g_player_appearance.open){
        appearance_draw(&g_player_appearance,hud,win_w,win_h);
        hud_flush(hud);
    }

    if(show_debug){
        debug_draw(hud,lr,perf,player,enemies,ne,NULL,&char_model->base,cam,player_amap,win_w,win_h,proj,view,terrain);
        if(prop_edit&&g_props.count>0){
            props_debug_draw(&g_props,prop_sel,cam->pos);
            props_editor_hud(hud,&g_props,prop_sel,win_w,win_h);
            hud_flush(hud);
        }
    }
    if(g_frame_debug.enabled&&!editor_ui_backend_is_imgui(&g_editor_ui)){
        frame_debugger_draw(hud,win_w,win_h);
        hud_flush(hud);
    }
    proj_draw(projs,lr,proj,view);
    magicvis_draw(&g_magicvis,lr,proj,view);
}

static void player_world_update(Player *player, LevelSystem *lvlsys,
                                AbilitySystem *as, FishingSystem *fishing,
                                Inventory *inv, SaveNotify *save_note,
                                Camera3P *cam, AnimMap *player_amap,
                                PropList *props, float dt,
                                bool fishing_space_pressed){
    float pg=terrain_h(player->phys.pos.x,player->phys.pos.z);
    fishing->near_water=false;

    if(player->state==PS_HIT||player->hit_flash>0.5f) player->ooc_timer=6.0f;
    else player->ooc_timer=clampf(player->ooc_timer-dt,0.0f,6.0f);
    if(player->ooc_timer<=0.0f&&player->state!=PS_DEAD&&player->health<player->max_health){
        float regen_rate=5.0f+lvlsys->regen_bonus;
        player->health=clampf(player->health+regen_rate*dt,0,player->max_health);
    }

    {
        float local_wlevel=WATER_LEVEL;
        bool has_surface_water=water_surface_info(player->phys.pos.x,player->phys.pos.z,&local_wlevel,NULL);
        bool in_water=has_surface_water
                   && (player->phys.pos.y <= local_wlevel + 0.35f)
                   && (pg <= local_wlevel + 0.2f)
                   && player->state != PS_DEAD;

        if(in_water){
            const uint8_t *ks=SDL_GetKeyboardState(NULL);
            bool rise_pressed=ks[SDL_SCANCODE_SPACE]!=0;
            bool dive_pressed=ks[SDL_SCANCODE_LCTRL]!=0||ks[SDL_SCANCODE_C]!=0;
            float swim_surface=local_wlevel-0.55f;
            float target_y=dive_pressed?fmaxf(pg+0.75f,local_wlevel-2.8f):swim_surface;
            float depth=target_y-player->phys.pos.y;
            float buoy=clampf(depth*7.0f,-10.0f,12.0f);
            player->phys.vel.y+=buoy*dt;
            if(rise_pressed) player->phys.vel.y=clampf(player->phys.vel.y + 10.0f*dt, -4.5f, 4.5f);
            if(dive_pressed) player->phys.vel.y=clampf(player->phys.vel.y - 11.0f*dt, -4.8f, 4.5f);
            player->phys.vel.x*=(1.0f-3.5f*dt);
            player->phys.vel.z*=(1.0f-3.5f*dt);
            player->phys.vel.y*=(1.0f-2.5f*dt);
            {
                Vec3 sfwd=cam_forward(cam), srgt=cam_right(cam), smove={0,0,0};
                if(ks[SDL_SCANCODE_W])smove=vec3_add(smove,sfwd);
                if(ks[SDL_SCANCODE_S])smove=vec3_sub(smove,sfwd);
                if(ks[SDL_SCANCODE_D])smove=vec3_add(smove,srgt);
                if(ks[SDL_SCANCODE_A])smove=vec3_sub(smove,srgt);
                smove.y=0;
                if(vec3_len(smove)>0.01f){
                    smove=vec3_scale(vec3_norm(smove),PLAYER_SPEED*0.55f);
                    player->phys.vel.x=smove.x;
                    player->phys.vel.z=smove.z;
                    player->phys.yaw=atan2f(smove.x,-smove.z)+(float)M_PI;
                }
            }
            if(player->phys.pos.y < pg + 0.05f){
                player->phys.pos.y = pg + 0.05f;
                if(player->phys.vel.y < 0) player->phys.vel.y = 0;
            }
            if(player->phys.pos.y > local_wlevel + 0.15f){
                player->phys.pos.y = local_wlevel + 0.15f;
                if(player->phys.vel.y > 0) player->phys.vel.y = 0;
            }
            player->phys.pos=vec3_add(player->phys.pos,vec3_scale(player->phys.vel,dt));
            player->phys.on_ground=false;
            g_player_underwater=(player->phys.pos.y+player->phys.height*0.72f)<(local_wlevel-0.08f);
            g_camera_allow_underwater=g_player_underwater;
            {
                bool swim_moving=vec3_len((Vec3){player->phys.vel.x,0,player->phys.vel.z})>0.3f;
                player->state=PS_SWIM;
                player->anim_idx=swim_moving?player_amap->walk:player_amap->idle;
                player->anim_speed=swim_moving?1.4f:0.6f;
            }
            as->stam=clampf(as->stam - 4.0f*dt, 0, as->max_stam);
            stam_drain(as);
            fishing_stop(fishing);
        }else{
            g_player_underwater=false;
            g_camera_allow_underwater=false;
            if(player->state==PS_SWIM){
                player->state=PS_IDLE;
                player->phys.vel.y=clampf(player->phys.vel.y,0,4.0f);
            }
            phys_update(&player->phys,dt,pg);
            {
                float fish_wl=WATER_LEVEL;
                Vec3 fish_spot={0,0,0};
                bool can_fish=player->phys.on_ground
                           && player->state!=PS_ATTACK
                           && player->state!=PS_ATTACK2
                           && player->state!=PS_ROLL
                           && fishing_find_water(player->phys.pos,&fish_wl,&fish_spot)
                           && fabsf(pg-fish_wl)<2.0f;
                fishing->near_water=can_fish;
                fishing->water_level=fish_wl;
                fishing->spot=fish_spot;
                if(fishing->active&&!can_fish){
                    fishing_stop(fishing);
                }else if(fishing->active){
                    fishing_update(fishing,dt);
                    if(fishing_space_pressed){
                        float catch_half=fishing->target_w*0.5f;
                        float d=fabsf(fishing->cursor-fishing->target);
                        if(d<=catch_half){
                            fishing_gain(fishing,inv,save_note);
                            fishing->misses=0;
                            fishing_retarget(fishing);
                        }else{
                            fishing->misses++;
                            if(fishing->misses>=3){
                                save_notify(save_note,"The fish got away");
                                fishing_stop(fishing);
                            }else{
                                char mbuf[48];
                                snprintf(mbuf,sizeof(mbuf),"Missed the bite (%d/3)",fishing->misses);
                                save_notify(save_note,mbuf);
                            }
                            fishing->banner=0.5f;
                        }
                    }
                    player->phys.vel.x*=0.15f;
                    player->phys.vel.z*=0.15f;
                    player->anim_idx=(fishing->cast_anim>0.0f)?player_amap->attack2:player_amap->idle;
                    player->state=PS_IDLE;
                }
            }
        }
    }

    player->phys.pos.x=clampf(player->phys.pos.x,-WORLD_HALF+5.0f,WORLD_HALF-5.0f);
    player->phys.pos.z=clampf(player->phys.pos.z,-WORLD_HALF+5.0f,WORLD_HALF-5.0f);
    props_stream_update(props,cam->pos);
    props_resolve_capsule(props,&player->phys.pos,&player->phys.vel,
                          &player->phys.on_ground,player->phys.height,player->phys.radius);
    player->anim_time += dt * player->anim_speed;

    {
        int pcx=nav_world_to_cx(player->phys.pos.x);
        int pcz=nav_world_to_cz(player->phys.pos.z);
        if(!g_nav_ready||pcx!=g_nav.pcx||pcz!=g_nav.pcz)
            nav_rebuild(player->phys.pos,props);
    }
}

        bool npc_dialogue_capture=(npc_ed_dlg_mode&&npc_ed_dlg_npc>=0);
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            switch(ev.type){
            case SDL_QUIT:running=false;break;
            case SDL_TEXTINPUT:
                if(npc_ed_dlg_mode&&npc_ed_dlg_npc>=0){
                    for(int ci=0;ev.text.text[ci]&&npc_ed_dlg_len<NPC_LINE_LEN-1;ci++)
                        npc_ed_dlg_buf[npc_ed_dlg_len++]=ev.text.text[ci];
                    npc_ed_dlg_buf[npc_ed_dlg_len]=0;
                }
                break;
            case SDL_KEYDOWN:
                #include "engine/input_keydown.inc"
                break;
            case SDL_MOUSEBUTTONDOWN:
                if(ev.button.button==SDL_BUTTON_LEFT&&inv.open){
                    int imx=ev.button.x,imy=ev.button.y;
                    inv_click(&inv,imx,imy,win_w,win_h,&player,&as,&qs,base_max_hp,&save_note);
                    break;
                }
                if(ev.button.button==SDL_BUTTON_RIGHT){cam_drag=true;cam_lx=ev.button.x;cam_ly=ev.button.y;}
                if(ev.button.button==SDL_BUTTON_LEFT&&npcs.open&&npcs.active_npc>=0&&npcs.talk_state==NPC_STATE_CHAT){
                    NpcDef *_nc=&npcs.npcs[npcs.active_npc];
                    if(_nc->dlg_root_id>=0&&_nc->dlg_cur_node>=0&&_nc->dlg_cur_node<g_dlg_count){
                        DialogueNode *_dn2=&g_dlg_nodes[_nc->dlg_cur_node];
                        if(_dn2->num_choices>0){
                            float _px2,_py2,_pw2,_ph2,_text_y2,_cy2;
                            npc_dialogue_layout(_dn2,win_w,win_h,&_px2,&_py2,&_pw2,&_ph2,&_text_y2,&_cy2);
                            for(int _ci2=0;_ci2<_dn2->num_choices;_ci2++,_cy2+=28.0f){
                                if(ev.button.x>=_px2+18.0f&&ev.button.x<=_px2+_pw2-18.0f&&
                                   ev.button.y>=_cy2&&ev.button.y<=_cy2+24.0f){
                                    npc_dlg_pick(&npcs,_ci2,&qs);break;
                                }
                            }
                        }
                    }
                }
                if(ev.button.button==SDL_BUTTON_LEFT&&!npc_dialogue_capture&&!inv.open&&!prop_place_mode&&!forest_edit&&player.state!=PS_DEAD&&player.state!=PS_MOUNTED&&player.state!=PS_ATTACK&&player.state!=PS_ATTACK2){player.state=PS_ATTACK;player.anim_idx=player_amap.attack;player.attack_timer=0;player.attack_hit=false;player.anim_time=0;audio_play(SND_ATTACK,100);}
                if(ev.button.button==SDL_BUTTON_MIDDLE&&!npc_dialogue_capture&&!forest_edit&&player.state!=PS_DEAD&&player.state!=PS_MOUNTED&&player.state!=PS_ATTACK&&player.state!=PS_ATTACK2){player.state=PS_ATTACK2;player.anim_idx=player_amap.attack2;player.attack_timer=0;player.attack_hit=false;player.anim_time=0;audio_play(SND_ATTACK,100);}
                break;
            case SDL_MOUSEBUTTONUP:if(ev.button.button==SDL_BUTTON_RIGHT)cam_drag=false;break;
            case SDL_MOUSEMOTION:
                if(map_screen.open&&(SDL_GetMouseState(NULL,NULL)&SDL_BUTTON(1))){
                    float scale=WORLD_HALF*2.0f/map_screen.zoom/(float)win_w;
                    map_screen.scroll_x-=ev.motion.xrel*scale;
                    map_screen.scroll_y-=ev.motion.yrel*scale;
                } else if(cam_drag){
                    cam.yaw-=(ev.motion.x-cam_lx)*0.005f;
                    cam.pitch-=(ev.motion.y-cam_ly)*0.005f;
                    cam.pitch=clampf(cam.pitch,0.05f,1.3f);
                    cam_lx=ev.motion.x;cam_ly=ev.motion.y;
                } break;
            case SDL_MOUSEWHEEL:
                #include "engine/input_mousewheel.inc"
                break;
            case SDL_WINDOWEVENT:if(ev.window.event==SDL_WINDOWEVENT_RESIZED){
                win_w=ev.window.data1;win_h=ev.window.data2;
                glViewport(0,0,win_w,win_h);
                hud_resize(&hud,win_w,win_h);
                pfx_resize(&g_pfx,win_w,win_h);
            }break;
            case SDL_CONTROLLERDEVICEADDED:
                if(!g_pad)gamepad_try_open();
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                if(g_pad&&SDL_GameControllerGetJoystick(g_pad)==
                   SDL_JoystickFromInstanceID(ev.cdevice.which)){
                    SDL_GameControllerClose(g_pad);g_pad=NULL;
                    printf("[Pad] Disconnected\n");
                    gamepad_try_open();
                }
                break;
            }
        }
        npc_dialogue_capture=(npc_ed_dlg_mode&&npc_ed_dlg_npc>=0);
        if(npc_dialogue_capture) time_scale=1.0f;
        bool is_paused=(g_pause.state!=PM_NONE);
        if(g_pad&&!is_paused&&!npc_dialogue_capture&&player.state!=PS_DEAD){
            bool pa=pad_btn(SDL_CONTROLLER_BUTTON_A);
            bool px2=pad_btn(SDL_CONTROLLER_BUTTON_X);
            bool py2=pad_btn(SDL_CONTROLLER_BUTTON_Y);
            bool pb=pad_btn(SDL_CONTROLLER_BUTTON_B);
            bool pstart=pad_btn(SDL_CONTROLLER_BUTTON_START);
            bool pback=pad_btn(SDL_CONTROLLER_BUTTON_BACK);
            if(pa&&!g_pad_a_prev&&player.phys.on_ground&&player.state!=PS_MOUNTED&&
               player.state!=PS_ATTACK&&player.state!=PS_ATTACK2&&
               player.state!=PS_ROLL&&player.state!=PS_SWIM){
                player.phys.vel.y=PLAYER_JUMP;player.state=PS_JUMP;
                player.anim_idx=player_amap.jump;player.anim_time=0;
                audio_play(SND_JUMP,90);
            }
            if(px2&&!g_pad_x_prev&&player.state!=PS_MOUNTED&&player.state!=PS_ATTACK&&player.state!=PS_ATTACK2){
                player.state=PS_ATTACK;player.anim_idx=player_amap.attack;
                player.attack_timer=0;player.attack_hit=false;player.anim_time=0;
                audio_play(SND_ATTACK,100);
            }
            if(py2&&!g_pad_y_prev&&player.state!=PS_MOUNTED&&player.state!=PS_ATTACK&&player.state!=PS_ATTACK2){
                player.state=PS_ATTACK2;player.anim_idx=player_amap.attack2;
                player.attack_timer=0;player.attack_hit=false;player.anim_time=0;
                audio_play(SND_ATTACK,100);
            }
            if(pb&&!g_pad_b_prev&&player.state!=PS_MOUNTED&&player.state!=PS_ROLL&&
               player.state!=PS_ATTACK&&player.state!=PS_ATTACK2&&
               player.phys.on_ground&&as.stam>=20.0f){
                player.state=PS_ROLL;player.anim_idx=player_amap.roll;
                player.roll_timer=0;player.anim_time=0;player.dash_iframes=0.5f;
            }
            if(pstart&&!g_pad_start_prev){
                if(g_pause.state==PM_NONE) g_pause.state=PM_ROOT;
                else g_pause.state=PM_NONE;
            }
            if(pback&&!g_pad_back_prev) inv.open=!inv.open;
            g_pad_a_prev=pa; g_pad_x_prev=px2; g_pad_y_prev=py2;
            g_pad_b_prev=pb; g_pad_start_prev=pstart; g_pad_back_prev=pback;
        }

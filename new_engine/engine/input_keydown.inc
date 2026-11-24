                /* ── Tree editor intercepts keys when active ── */
                if(npc_ed_tree_mode&&npc_ed_tree_npc>=0){
                    NpcSpawnPoint *tsp=&g_npc_spawns.points[npc_ed_tree_npc];
                    SDL_Keycode dk=ev.key.keysym.sym;
                    if(npc_ed_tree_editing){
                        if(dk==SDLK_BACKSPACE&&npc_ed_tree_len>0){
                            npc_ed_tree_buf[--npc_ed_tree_len]=0;
                        }else if(dk==SDLK_RETURN||dk==SDLK_KP_ENTER){
                            if(npc_ed_tree_sel>=0&&npc_ed_tree_sel<tsp->tree_count){
                                NpcTreeNode *tn=&tsp->tree[npc_ed_tree_sel];
                                if(npc_ed_tree_field==0){
                                    strncpy(tn->text,npc_ed_tree_buf,199);tn->text[199]=0;
                                    npc_ed_tree_editing=false;npc_ed_tree_buf[0]=0;npc_ed_tree_len=0;
                                    SDL_StopTextInput();npc_spawns_save(&g_npc_spawns);
                                }else if(npc_ed_tree_field==1){
                                    strncpy(tn->speaker,npc_ed_tree_buf,31);tn->speaker[31]=0;
                                    npc_ed_tree_editing=false;npc_ed_tree_buf[0]=0;npc_ed_tree_len=0;
                                    SDL_StopTextInput();npc_spawns_save(&g_npc_spawns);
                                }else if(npc_ed_tree_field==2){
                                    if(npc_ed_tree_choice>=0&&npc_ed_tree_choice<tn->num_choices){
                                        strncpy(tn->choice_label[npc_ed_tree_choice],npc_ed_tree_buf,63);
                                        tn->choice_label[npc_ed_tree_choice][63]=0;
                                    }
                                    npc_ed_tree_editing=false;npc_ed_tree_buf[0]=0;npc_ed_tree_len=0;
                                    SDL_StopTextInput();npc_spawns_save(&g_npc_spawns);
                                }else if(npc_ed_tree_field==3){
                                    /* parse destination node */
                                    int dest=-1;
                                    if(strcmp(npc_ed_tree_buf,"-1")==0||strcmp(npc_ed_tree_buf,"close")==0)dest=-1;
                                    else if(strcmp(npc_ed_tree_buf,"-2")==0||strcmp(npc_ed_tree_buf,"quest")==0)dest=-2;
                                    else if(strcmp(npc_ed_tree_buf,"-3")==0||strcmp(npc_ed_tree_buf,"shop")==0)dest=-3;
                                    else sscanf(npc_ed_tree_buf,"%d",&dest);
                                    if(tn->num_choices<4){
                                        int ci=tn->num_choices++;
                                        tn->choice_next[ci]=dest;
                                        tn->choice_label[ci][0]=0;
                                        npc_ed_tree_choice=ci;
                                    }
                                    /* Now edit the choice label */
                                    npc_ed_tree_field=2;
                                    npc_ed_tree_buf[0]=0;npc_ed_tree_len=0;
                                }
                            }
                        }else if(dk==SDLK_ESCAPE){
                            npc_ed_tree_editing=false;npc_ed_tree_buf[0]=0;npc_ed_tree_len=0;
                            SDL_StopTextInput();
                        }
                        break;
                    }
                    /* Navigation mode */
                    if(dk==SDLK_ESCAPE){
                        npc_ed_tree_mode=false;npc_ed_tree_npc=-1;
                        printf("[NpcTree] Closed\n");
                    }else if(dk==SDLK_UP){
                        if(npc_ed_tree_sel>0)npc_ed_tree_sel--;
                        npc_ed_tree_choice=-1;
                    }else if(dk==SDLK_DOWN){
                        if(tsp->tree_count>0&&npc_ed_tree_sel<tsp->tree_count-1)npc_ed_tree_sel++;
                        npc_ed_tree_choice=-1;
                    }else if(dk==SDLK_TAB){
                        if(tsp->tree_count>0)npc_ed_tree_sel=(npc_ed_tree_sel+1)%tsp->tree_count;
                        npc_ed_tree_choice=-1;
                    }else if(dk==SDLK_a&&tsp->tree_count<NPC_TREE_MAX){
                        NpcTreeNode *newn=&tsp->tree[tsp->tree_count++];
                        memset(newn,0,sizeof(*newn));newn->anim_trigger=-1;
                        npc_ed_tree_sel=tsp->tree_count-1;
                        if(tsp->tree_count==1)tsp->tree_root=0;
                        npc_spawns_save(&g_npc_spawns);
                        printf("[NpcTree] Added node %d\n",npc_ed_tree_sel);
                    }else if(dk==SDLK_DELETE&&npc_ed_tree_sel>=0&&npc_ed_tree_sel<tsp->tree_count){
                        if(npc_ed_tree_choice>=0){
                            /* Delete selected choice */
                            NpcTreeNode *tn=&tsp->tree[npc_ed_tree_sel];
                            if(npc_ed_tree_choice<tn->num_choices){
                                for(int ci=npc_ed_tree_choice;ci<tn->num_choices-1;ci++){
                                    tn->choice_next[ci]=tn->choice_next[ci+1];
                                    strncpy(tn->choice_label[ci],tn->choice_label[ci+1],63);
                                }
                                tn->num_choices--;
                                if(npc_ed_tree_choice>=tn->num_choices)npc_ed_tree_choice=tn->num_choices-1;
                            }
                            npc_spawns_save(&g_npc_spawns);
                        }else{
                            /* Delete selected node — shift down and remap */
                            int del=npc_ed_tree_sel;
                            for(int ni=del;ni<tsp->tree_count-1;ni++)tsp->tree[ni]=tsp->tree[ni+1];
                            tsp->tree_count--;
                            for(int ni=0;ni<tsp->tree_count;ni++){
                                NpcTreeNode *tn=&tsp->tree[ni];
                                for(int ci=0;ci<tn->num_choices;ci++){
                                    if(tn->choice_next[ci]==del)tn->choice_next[ci]=-1;
                                    else if(tn->choice_next[ci]>del)tn->choice_next[ci]--;
                                }
                            }
                            if(tsp->tree_root==del)tsp->tree_root=(tsp->tree_count>0)?0:-1;
                            else if(tsp->tree_root>del)tsp->tree_root--;
                            if(npc_ed_tree_sel>=tsp->tree_count)npc_ed_tree_sel=tsp->tree_count-1;
                            npc_spawns_save(&g_npc_spawns);
                            printf("[NpcTree] Deleted node %d\n",del);
                        }
                    }else if(dk==SDLK_r&&npc_ed_tree_sel>=0&&npc_ed_tree_sel<tsp->tree_count){
                        tsp->tree_root=npc_ed_tree_sel;
                        npc_spawns_save(&g_npc_spawns);
                        printf("[NpcTree] Root -> node %d\n",npc_ed_tree_sel);
                    }else if((dk==SDLK_RETURN||dk==SDLK_KP_ENTER)&&npc_ed_tree_sel>=0&&npc_ed_tree_sel<tsp->tree_count){
                        NpcTreeNode *tn=&tsp->tree[npc_ed_tree_sel];
                        npc_ed_tree_editing=true;npc_ed_tree_field=0;
                        strncpy(npc_ed_tree_buf,tn->text,199);npc_ed_tree_len=(int)strlen(npc_ed_tree_buf);
                        SDL_StartTextInput();
                    }else if(dk==SDLK_q&&npc_ed_tree_sel>=0&&npc_ed_tree_sel<tsp->tree_count){
                        NpcTreeNode *tn=&tsp->tree[npc_ed_tree_sel];
                        npc_ed_tree_editing=true;npc_ed_tree_field=1;
                        strncpy(npc_ed_tree_buf,tn->speaker,31);npc_ed_tree_len=(int)strlen(npc_ed_tree_buf);
                        SDL_StartTextInput();
                    }else if(dk==SDLK_c&&npc_ed_tree_sel>=0&&npc_ed_tree_sel<tsp->tree_count){
                        NpcTreeNode *tn=&tsp->tree[npc_ed_tree_sel];
                        if(tn->num_choices<4){
                            npc_ed_tree_editing=true;npc_ed_tree_field=3;
                            npc_ed_tree_buf[0]=0;npc_ed_tree_len=0;
                            SDL_StartTextInput();
                        }
                    }else if((dk==SDLK_EQUALS||dk==SDLK_KP_PLUS)&&npc_ed_tree_sel>=0&&npc_ed_tree_sel<tsp->tree_count){
                        tsp->tree[npc_ed_tree_sel].anim_trigger++;
                        npc_spawns_save(&g_npc_spawns);
                    }else if((dk==SDLK_MINUS||dk==SDLK_KP_MINUS)&&npc_ed_tree_sel>=0&&npc_ed_tree_sel<tsp->tree_count){
                        if(tsp->tree[npc_ed_tree_sel].anim_trigger>-1)tsp->tree[npc_ed_tree_sel].anim_trigger--;
                        npc_spawns_save(&g_npc_spawns);
                    }else if(dk>=SDLK_1&&dk<=SDLK_4&&npc_ed_tree_sel>=0&&npc_ed_tree_sel<tsp->tree_count){
                        int ci=dk-SDLK_1;
                        if(ci<tsp->tree[npc_ed_tree_sel].num_choices)npc_ed_tree_choice=ci;
                    }
                    break;
                }
                /* ── Dialogue editor intercepts keys when active ── */
                if(npc_ed_dlg_mode&&npc_ed_dlg_npc>=0){
                    NpcSpawnPoint *dsp=&g_npc_spawns.points[npc_ed_dlg_npc];
                    SDL_Keycode dk=ev.key.keysym.sym;
                    if(dk==SDLK_BACKSPACE&&npc_ed_dlg_len>0){
                        npc_ed_dlg_buf[--npc_ed_dlg_len]=0;
                    } else if(dk==SDLK_RETURN||dk==SDLK_KP_ENTER){
                        if(npc_ed_dlg_len>0){
                            if(npc_ed_dlg_sel>=0&&npc_ed_dlg_sel<dsp->dialogue_count)
                                strncpy(dsp->dialogue[npc_ed_dlg_sel],npc_ed_dlg_buf,NPC_LINE_LEN-1);
                            else if(dsp->dialogue_count<NPC_LINES_MAX)
                                strncpy(dsp->dialogue[dsp->dialogue_count++],npc_ed_dlg_buf,NPC_LINE_LEN-1);
                            npc_ed_dlg_buf[0]=0; npc_ed_dlg_len=0; npc_ed_dlg_sel=-1;
                            npc_spawns_save(&g_npc_spawns);
                            for(int ni=0;ni<npcs->count;ni++){
                                float dx=npcs->npcs[ni].pos.x-dsp->pos.x;
                                float dz=npcs->npcs[ni].pos.z-dsp->pos.z;
                                if(dx*dx+dz*dz<1.0f){
                                    npcs->npcs[ni].line_count=0;
                                    for(int di=0;di<dsp->dialogue_count;di++)
                                        npc_add_line(npcs,ni,dsp->dialogue[di]);
                                    break;
                                }
                            }
                        }
                    } else if(dk==SDLK_ESCAPE){
                        npc_ed_dlg_mode=false;
                        npc_ed_dlg_npc=-1;
                        npc_ed_dlg_buf[0]=0; npc_ed_dlg_len=0; npc_ed_dlg_sel=-1;
                        SDL_StopTextInput();
                        printf("[NpcEd] Dialogue mode OFF\n");
                    } else if(dk==SDLK_DELETE&&npc_ed_dlg_sel>=0&&npc_ed_dlg_sel<dsp->dialogue_count){
                        for(int di=npc_ed_dlg_sel;di<dsp->dialogue_count-1;di++)
                            strncpy(dsp->dialogue[di],dsp->dialogue[di+1],NPC_LINE_LEN-1);
                        dsp->dialogue_count--; npc_ed_dlg_sel=-1;
                        npc_ed_dlg_buf[0]=0; npc_ed_dlg_len=0;
                        npc_spawns_save(&g_npc_spawns);
                        for(int ni=0;ni<npcs->count;ni++){
                            float dx=npcs->npcs[ni].pos.x-dsp->pos.x;
                            float dz=npcs->npcs[ni].pos.z-dsp->pos.z;
                            if(dx*dx+dz*dz<1.0f){
                                npcs->npcs[ni].line_count=0;
                                for(int di=0;di<dsp->dialogue_count;di++)
                                    npc_add_line(npcs,ni,dsp->dialogue[di]);
                                break;
                            }
                        }
                    } else if(dk==SDLK_UP){
                        if(npc_ed_dlg_sel==-1&&dsp->dialogue_count>0)
                            npc_ed_dlg_sel=dsp->dialogue_count-1;
                        else if(npc_ed_dlg_sel>0)
                            npc_ed_dlg_sel--;
                        if(npc_ed_dlg_sel>=0){
                            strncpy(npc_ed_dlg_buf,dsp->dialogue[npc_ed_dlg_sel],NPC_LINE_LEN-1);
                            npc_ed_dlg_len=(int)strlen(npc_ed_dlg_buf);
                        }
                    } else if(dk==SDLK_DOWN){
                        if(npc_ed_dlg_sel<dsp->dialogue_count-1){
                            npc_ed_dlg_sel++;
                            strncpy(npc_ed_dlg_buf,dsp->dialogue[npc_ed_dlg_sel],NPC_LINE_LEN-1);
                            npc_ed_dlg_len=(int)strlen(npc_ed_dlg_buf);
                        } else { npc_ed_dlg_sel=-1; npc_ed_dlg_buf[0]=0; npc_ed_dlg_len=0; }
                    } else if(dk==SDLK_m){
                        dsp->is_merchant=!dsp->is_merchant;
                        npc_spawns_save(&g_npc_spawns);
                    }
                    break;
                }else if(prop_place_mode){
                    SDL_Keycode dk=ev.key.keysym.sym;
                    if(!g_placer.prefab_mode&&g_placer.search_focus&&dk==SDLK_BACKSPACE){
                        size_t slen=strlen(g_placer.search);
                        if(slen>0){
                            g_placer.search[slen-1]='\0';
                            placer_rebuild_filter(&g_placer);
                        }
                    }else if(!g_placer.prefab_mode&&g_placer.search_focus&&dk==SDLK_ESCAPE){
                        if(g_placer.search[0]){
                            g_placer.search[0]='\0';
                            placer_rebuild_filter(&g_placer);
                        }else{
                            g_placer.search_focus=false;
                            SDL_StopTextInput();
                        }
                        break;
                    }
                }
                if(g_skilltree.open&&ev.key.keysym.sym!=SDLK_k&&ev.key.keysym.sym!=SDLK_ESCAPE){
                    break;
                }
                if(g_player_appearance.open&&
                   ev.key.keysym.sym!=SDLK_j&&ev.key.keysym.sym!=SDLK_ESCAPE&&
                   ev.key.keysym.sym!=SDLK_TAB&&ev.key.keysym.sym!=SDLK_r&&
                   ev.key.keysym.sym!=SDLK_f&&ev.key.keysym.sym!=SDLK_g&&
                   ev.key.keysym.sym!=SDLK_v&&ev.key.keysym.sym!=SDLK_b&&
                   ev.key.keysym.sym!=SDLK_n){
                    break;
                }
                if(g_player_appearance.open){
                    SDL_Keycode ak=ev.key.keysym.sym;
                    if(ak==SDLK_TAB){
                        g_player_appearance.selected_slot=(g_player_appearance.selected_slot+1)%APPEARANCE_COLOR_SLOTS;
                        audio_play(SND_MENU_CLICK,65);
                        break;
                    }
                    if(ak==SDLK_r){appearance_adjust(&g_player_appearance,g_player_appearance.selected_slot,0,8);break;}
                    if(ak==SDLK_f){appearance_adjust(&g_player_appearance,g_player_appearance.selected_slot,0,-8);break;}
                    if(ak==SDLK_g){appearance_adjust(&g_player_appearance,g_player_appearance.selected_slot,1,8);break;}
                    if(ak==SDLK_v){appearance_adjust(&g_player_appearance,g_player_appearance.selected_slot,1,-8);break;}
                    if(ak==SDLK_b){appearance_adjust(&g_player_appearance,g_player_appearance.selected_slot,2,8);break;}
                    if(ak==SDLK_n){appearance_adjust(&g_player_appearance,g_player_appearance.selected_slot,2,-8);break;}
                }
                {
                    SDL_Scancode sc=ev.key.keysym.scancode;
                    bool gameplay_bind_context=!river_edit&&!path_edit&&!wzone_edit&&!prop_place_mode&&!particle_fx_edit&&
                                               !forest_edit&&!wind_edit&&!spawn_edit_mode&&!mount_edit_mode&&!npc_edit_mode&&!sculpt_edit&&!prop_edit&&!marker_edit;
                    if(gameplay_bind_context){
                        if(options_bind_matches_scancode(sc,BIND_MAP)){
                            if(!g_shop.open&&!inv.open&&!save_ui.open){
                                map_screen.open=!map_screen.open;
                                if(map_screen.open){map_screen.scroll_x=0;map_screen.scroll_y=0;map_screen.zoom=1.0f;}
                                audio_play(SND_MENU_CLICK,70);
                            }
                            break;
                        }
                        if(options_bind_matches_scancode(sc,BIND_INVENTORY)){ inv.open=!inv.open;audio_play(SND_MENU_CLICK,70);break; }
                        if(options_bind_matches_scancode(sc,BIND_APPEARANCE)){
                            if(player.state!=PS_DEAD&&!save_ui.open&&!g_shop.open&&!map_screen.open){
                                if(g_player_appearance.open){
                                    g_player_appearance.open=false;
                                    appearance_set_enabled_from_saved(&g_player_appearance,g_player_tint_enabled);
                                }else{
                                    g_player_appearance.open=true;
                                    inv.open=false;g_skilltree.open=false;g_player_appearance.selected_slot=0;
                                    appearance_restore_saved(&g_player_appearance);
                                    g_player_tint_enabled=true;
                                }
                                audio_play(SND_MENU_CLICK,70);
                            }
                            break;
                        }
                        if(options_bind_matches_scancode(sc,BIND_SKILLTREE)){
                            if(player.state!=PS_DEAD&&!save_ui.open&&!g_shop.open&&!map_screen.open){
                                g_skilltree.open=!g_skilltree.open;
                                if(g_skilltree.open){inv.open=false;g_player_appearance.open=false;}
                                audio_play(SND_MENU_CLICK,70);
                            }
                            break;
                        }
                        if(options_bind_matches_scancode(sc,BIND_POTION)){
                            if(player.state!=PS_DEAD) quick_use_potion(&inv,&player,&as,&qs,base_max_hp,&save_note);
                            break;
                        }
                        if(options_bind_matches_scancode(sc,BIND_INTERACT)){
                            if(player.state!=PS_DEAD){
                                float cf_dist=vec3_dist(player.phys.pos,cf_pos);
                                if(mount_system_try_interact(&g_mounts,&player,&player_amap,&save_note)){
                                }else if(cf_dist<3.5f&&!cooking.active&&player.state!=PS_MOUNTED){
                                    g_farm_crate_open=false;
                                    campfire_menu_open=!campfire_menu_open;
                                    campfire_lmb_prev=true;
                                } else if(farm_crate_in_range(player.phys.pos)&&player.state!=PS_MOUNTED){
                                    farm_crate_toggle(&campfire_menu_open,&campfire_lmb_prev);
                                } else if(fishing.active&&player.state!=PS_MOUNTED){
                                    fishing_stop(&fishing);
                                } else if(fishing.near_water&&player.phys.on_ground&&player.state!=PS_SWIM&&player.state!=PS_MOUNTED){
                                    fishing_start(&fishing,fishing.spot,fishing.water_level);
                                } else {
                                    if(!prop_interact_try_use(&g_props,player.phys.pos,&save_note))
                                        npc_interact(npcs,player.phys.pos,&qs);
                                }
                            }
                            break;
                        }
                        if(options_bind_matches_scancode(sc,BIND_DODGE)){
                            if(player.state!=PS_DEAD&&player.state!=PS_ROLL&&player.state!=PS_SWIM&&player.state!=PS_MOUNTED&&player.phys.on_ground&&as.stam>=20.0f){
                                player.state=PS_ROLL;player.anim_idx=player_amap.roll;player.roll_timer=0;player.anim_time=0;player.dash_iframes=0.5f;
                                stam_consume(&as,20.0f);
                            }
                            break;
                        }
                    }
                }
                switch(ev.key.keysym.sym){
                case SDLK_BACKSLASH:
                    g_toon_shading=!g_toon_shading;
                    printf("[Render] Cartoon shader %s\n",g_toon_shading?"ON":"OFF");
                    break;
                case SDLK_ESCAPE:
                    if(editor_mode||shader_edit||river_edit||path_edit||wzone_edit||prop_place_mode||particle_fx_edit||
                       forest_edit||wind_edit||spawn_edit_mode||mount_edit_mode||npc_edit_mode||sculpt_edit||prop_edit||marker_edit){
                        river_edit=false; path_edit=false; wzone_edit=false; wf_edit_mode=false;
                        prop_place_mode=false; particle_fx_edit=false; forest_edit=false; wind_edit=false;
                        spawn_edit_mode=false; mount_edit_mode=false; npc_edit_mode=false; sculpt_edit=false; prop_edit=false;
                        show_debug=false; marker_edit=false; editor_mode=false; shader_edit=false;
                        SDL_SetRelativeMouseMode(SDL_FALSE);
                        SDL_SetCursor(SDL_GetDefaultCursor());
                        SDL_ShowCursor(SDL_ENABLE);
                        SDL_StopTextInput();
                        save_notify(&save_note,"Exited editor");
                        audio_play(SND_MENU_CLICK,70);
                        break;
                    }
                    if(save_ui.open){save_ui.open=false;audio_play(SND_MENU_CLICK,70);break;}
                    if(g_skilltree.open){g_skilltree.open=false;audio_play(SND_MENU_CLICK,70);break;}
                    if(g_player_appearance.open){g_player_appearance.open=false;appearance_set_enabled_from_saved(&g_player_appearance,g_player_tint_enabled);audio_play(SND_MENU_CLICK,70);break;}
                    if(map_screen.open){map_screen.open=false;break;}
                    if(inv.open){inv.open=false;break;}
                    if(g_shop.open){shop_close(&g_shop);break;}
                    if(npcs->open){npcs->open=false;npcs->active_npc=-1;break;}
                    if(g_pause.state==PM_NONE){
                        g_pause.state=PM_ROOT;
                        g_pause.lmb_prev=true;
                        audio_play(SND_MENU_CLICK,70);
                    } else {
                        g_pause.state=PM_NONE;
                        audio_play(SND_MENU_CLICK,70);
                    }
                    break;
                case SDLK_m:
                    if(!river_edit&&!path_edit&&!wzone_edit&&!prop_place_mode&&!particle_fx_edit&&
                       !forest_edit&&!wind_edit&&!spawn_edit_mode&&!mount_edit_mode&&!npc_edit_mode&&!sculpt_edit&&!prop_edit&&!marker_edit){
                        break;
                    }
                    if(!g_shop.open&&!inv.open&&!save_ui.open){
                        map_screen.open=!map_screen.open;
                        if(map_screen.open){map_screen.scroll_x=0;map_screen.scroll_y=0;map_screen.zoom=1.0f;}
                        audio_play(SND_MENU_CLICK,70);
                    } break;
                case SDLK_z:
                    if((SDL_GetModState()&KMOD_ALT)!=0&&(prop_edit||prop_place_mode)){
                        editor_key_align_z();
                        save_notify(&save_note,"Aligned selection on Z");
                        break;
                    }
                    if((SDL_GetModState()&KMOD_SHIFT)!=0){
                        /* Ctrl+Shift+Z → Redo */
                        redo_perform();
                    } else if(river_edit&&g_criver.count>0){
                        g_criver.count--;
                        g_criver.dirty=true;
                        terrain_rebuild_all(&terrain);
                        terrain_rebake_hmap(&terrain);
                        printf("[RiverEd] Undo — %d points\n",g_criver.count);
                    } else if(path_edit&&g_cpath.count>0){
                        g_cpath.count--;
                        if(g_cpath.count>=2) path_rebuild_mesh();
                        else { g_pmesh.valid=false; }
                        printf("[PathEd] Undo — %d points\n",g_cpath.count);
                    } else {
                        /* Global undo for all editor systems */
                        undo_perform();
                    }
                    break;
                case SDLK_a:
                    if((prop_edit||prop_place_mode)&&(SDL_GetModState()&KMOD_CTRL))
                        prop_multi_add_all();
                    break;
                case SDLK_c:
                    if((SDL_GetModState()&KMOD_ALT)!=0&&(prop_edit||prop_place_mode)){
                        editor_key_distribute_z();
                        save_notify(&save_note,"Distributed selection on Z");
                        break;
                    }
                    if((prop_edit||prop_place_mode)&&(SDL_GetModState()&KMOD_CTRL)&&g_prop_sel_count>0){
                        prop_multi_copy(); break;
                    }
                    if(river_edit){
                        g_criver.count=0; g_criver.dirty=true;
                        terrain_rebuild_all(&terrain);
                        terrain_rebake_hmap(&terrain);
                        printf("[RiverEd] Cleared all points\n");
                    } else if(path_edit){
                        g_cpath.count=0; g_pmesh.valid=false;
                        printf("[PathEd] Cleared all points\n");
                    } else if(wzone_edit){
                        if(wf_edit_mode){
                            g_wfalls.count=0; wf_sel=-1;
                            printf("[WFalls] Cleared\n");
                        } else {
                            g_wzone_count=0; wzone_sel=-1;
                            wzone_rebuild_vaos(&terrain);
                        }
                        printf("[WZoneEd] Cleared all zones\n");
                    } else if(prop_edit&&g_props.count&&prop_sel>=0&&prop_sel<g_props.count){
                        Prop *p=&g_props.list[prop_sel];
                        if(p->active){
                            prop_apply_collision_mode(prop_sel,(PropCollType)(((int)p->coll_type+1)%3));
                        }
                    } else if(prop_place_mode){
                        g_placer.place_coll=(PropCollType)(((int)g_placer.place_coll+1)%3);
                        printf("[PropPlacer] Collision -> %s\n",prop_coll_name(g_placer.place_coll));
                    } else if(wind_edit){
                        g_wind_zone_count=0; wind_sel=-1;
                        printf("[WindEd] Cleared all zones\n");
                    } break;
                case SDLK_s:
                    if((SDL_GetModState()&KMOD_ALT)!=0&&prop_place_mode){
                        editor_key_scatter_current();
                        save_notify(&save_note,"Scattered current asset");
                        break;
                    }
                    if(river_edit){ criver_save(); terrain_rebuild_all(&terrain); terrain_rebake_hmap(&terrain); printf("[RiverEd] Saved + heightmap baked\n"); }
                    else if(path_edit){ cpath_save(); }
                    else if(wzone_edit){
                        if(wf_edit_mode) wf_save();
                        else { wzone_save(); terrain_rebuild_all(&terrain); terrain_rebake_hmap(&terrain); printf("[WZoneEd] Saved + heightmap baked\n"); }
                    }
                    else if(prop_place_mode){
                        if(g_placer.prefab_mode){
                            prefab_library_save(&g_prefabs);
                            save_notify(&save_note,"Prefabs saved");
                        }else{
                            placer_save(&g_props);
                        }
                    }
                    else if(wind_edit){ windzone_save(); }
                    else if(forest_edit){
                        if(forest_brush_kind==0) forest_save(&g_forest);
                        else if(forest_brush_kind==3||forest_brush_kind==4) placer_save(&g_props);
                        else if(forest_brush_kind==5){ worn_mask_save(); terrain_style_mask_save(); }
                        else if(forest_brush_kind==6) farm_field_mask_save();
                        else grass_mask_save();
                    }
                    else if(marker_edit){ world_markers_save(&g_world_markers); }
                    else if(particle_fx_edit){ world_fx_save(&g_world_fx); }
                    else if(spawn_edit_mode){ spawns_save(&g_spawns); }
                    else if(mount_edit_mode){ mount_system_save(&g_mounts); }
                    break;
                case SDLK_w:
                    if(river_edit){
                        int next=(g_wstyle.preset_idx+1)%WATER_PRESET_COUNT;
                        wstyle_apply_preset(next);
                    } else if(wzone_edit&&wzone_sel>=0&&wzone_sel<g_wzone_count){
                        WaterZone z=g_wzones[wzone_sel];
                        z.preset_idx=(z.preset_idx+1)%WATER_PRESET_COUNT;
                        wzone_update_with_undo(wzone_sel,&z,&terrain);
                        printf("[WZoneEd] Zone %d -> %s\n",wzone_sel,
                               WATER_PRESETS[g_wzones[wzone_sel].preset_idx].name);
                    } break;
                case SDLK_DELETE:
                    if((prop_edit||prop_place_mode)&&g_prop_sel_count>1){
                        /* Delete key: remove all multi-selected props */
                        prop_multi_delete_all(&prop_sel);
                        g_placer.selected_prop=prop_sel;
                        break;
                    }
                    if(prop_place_mode&&g_placer.prefab_mode){
                        if(prefab_delete_selected(&g_prefabs,&g_placer)){
                            save_notify(&save_note,"Prefab deleted");
                        }
                        break;
                    }
                    if(prop_edit&&g_props.count&&prop_sel>=0&&prop_sel<g_props.count){
                        Prop *p=&g_props.list[prop_sel];
                        if(p->active){
                            int deleted=prop_sel;
                            int next=props_next_active(&g_props,prop_sel);
                            if(next==prop_sel) next=-1;
                            prop_release_all(p);
                            if(next>=0&&next<g_props.count&&g_props.list[next].active){
                                prop_sel=next;
                                g_placer.selected_prop=next;
                                placer_sync_from_prop(&g_placer,&g_props.list[next]);
                                printf("[PropEdit] Deleted prop %d (%s); selected %d: %s\n",
                                       deleted,p->path,prop_sel,g_props.list[prop_sel].path);
                            }else{
                                prop_sel=-1;
                                g_placer.selected_prop=-1;
                                printf("[PropEdit] Deleted prop %d (%s); no active props remain\n",
                                       deleted,p->path);
                            }
                        }
                    } else if(prop_place_mode){
                        float best=1e9f; int bi=-1;
                        for(int i=0;i<g_props.count;i++){
                            Prop *p=&g_props.list[i];if(!p->active)continue;
                            float dx=p->pos.x-cam.pos.x,dz=p->pos.z-cam.pos.z;
                            float d=dx*dx+dz*dz;if(d<best){best=d;bi=i;}
                        }
                        if(bi>=0){
                            Prop *p=&g_props.list[bi];
                            prop_release_all(p);
                            printf("[PropPlacer] Deleted prop %d (%s)\n",bi,p->path);
                        }
                    } break;
                case SDLK_F5:game_save(0,&player,&qs,&as,enemies,ne,total_time,&inv,&lvlsys);save_notify(&save_note,"Saved — Slot 1");audio_play(SND_MENU_CLICK,80);break;
                case SDLK_F6:
                    if((SDL_GetModState()&KMOD_SHIFT)!=0){
                        game_save(1,&player,&qs,&as,enemies,ne,total_time,&inv,&lvlsys);
                        save_notify(&save_note,"Saved — Slot 2");
                        audio_play(SND_MENU_CLICK,80);
                    }else{
                        particle_fx_edit=!particle_fx_edit;
                        prop_edit=false;show_debug=false;river_edit=false;prop_place_mode=false;spawn_edit_mode=false;mount_edit_mode=false;path_edit=false;wzone_edit=false;forest_edit=false;wind_edit=false;marker_edit=false;
                        npc_edit_mode=false;sculpt_edit=false;
                        SDL_SetRelativeMouseMode(particle_fx_edit?SDL_FALSE:SDL_TRUE);
                        if(particle_fx_edit){
                            if(g_world_fx.selected>=0&&g_world_fx.selected<g_world_fx.count&&g_world_fx.items[g_world_fx.selected].active)
                                world_fx_editor_sync_from_fx(&g_world_fx_editor,&g_world_fx.items[g_world_fx.selected]);
                            else
                                world_fx_editor_reset(&g_world_fx_editor);
                        }
                        printf("[WorldFX] %s\n",particle_fx_edit?"ON":"OFF");
                    }
                    break;
                case SDLK_F7:
                    if((SDL_GetModState()&KMOD_SHIFT)!=0){
                        game_save(2,&player,&qs,&as,enemies,ne,total_time,&inv,&lvlsys);save_notify(&save_note,"Saved — Slot 3");audio_play(SND_MENU_CLICK,80);
                    }else{
                        marker_edit=!marker_edit;
                        prop_edit=false;show_debug=false;river_edit=false;prop_place_mode=false;spawn_edit_mode=false;mount_edit_mode=false;path_edit=false;wzone_edit=false;forest_edit=false;wind_edit=false;
                        npc_edit_mode=false;particle_fx_edit=false;sculpt_edit=false;
                        SDL_SetRelativeMouseMode(marker_edit?SDL_FALSE:SDL_TRUE);
                        printf("[MarkerEd] %s — %s\n",marker_edit?"ON":"OFF",world_marker_kind_name((WorldMarkerKind)marker_sel_type));
                    }
                    break;
                case SDLK_F9:
                    sculpt_edit=!sculpt_edit;
                    spawn_edit_mode=false;mount_edit_mode=false;npc_edit_mode=false;prop_edit=false;marker_edit=false;
                    show_debug=false;river_edit=false;prop_place_mode=false;forest_edit=false;
                    path_edit=false;wzone_edit=false;particle_fx_edit=false;wind_edit=false;
                    SDL_SetRelativeMouseMode(sculpt_edit?SDL_FALSE:SDL_TRUE);
                    printf("[SculptEd] %s  mode=%d  radius=%.0f\n",
                           sculpt_edit?"ON":"OFF",sculpt_mode,(double)sculpt_radius);
                    break;
                case SDLK_F10:
                    if((SDL_GetModState()&KMOD_SHIFT)!=0){
                        wind_edit=!wind_edit;
                        spawn_edit_mode=false;mount_edit_mode=false;npc_edit_mode=false;prop_edit=false;marker_edit=false;
                        show_debug=false;river_edit=false;prop_place_mode=false;sculpt_edit=false;
                        path_edit=false;wzone_edit=false;particle_fx_edit=false;forest_edit=false;
                        SDL_SetRelativeMouseMode(wind_edit?SDL_FALSE:SDL_TRUE);
                        printf("[WindEd] %s — %d zones\n",wind_edit?"ON":"OFF",g_wind_zone_count);
                    }else{
                        forest_edit=!forest_edit;
                        spawn_edit_mode=false;mount_edit_mode=false;npc_edit_mode=false;prop_edit=false;marker_edit=false;
                        show_debug=false;river_edit=false;prop_place_mode=false;sculpt_edit=false;
                        path_edit=false;wzone_edit=false;particle_fx_edit=false;wind_edit=false;
                        SDL_SetRelativeMouseMode(forest_edit?SDL_FALSE:SDL_TRUE);
                        printf("[ForestEd] %s — %s\n",forest_edit?"ON":"OFF",
                               forest_brush_kind==0?"Tree brush":(forest_brush_kind==1?"Grass brush":(forest_brush_kind==2?"Flower brush":(forest_brush_kind==3?"Boulder brush":(forest_brush_kind==4?"Wall brush":(forest_brush_kind==5?"Terrain brush":"Farm field brush"))))));
                    }
                    break;
                case SDLK_i:
                    if(!river_edit&&!path_edit&&!wzone_edit&&!prop_place_mode&&!particle_fx_edit&&
                       !forest_edit&&!wind_edit&&!spawn_edit_mode&&!mount_edit_mode&&!npc_edit_mode&&!sculpt_edit&&!prop_edit&&!marker_edit) break;
                    inv.open=!inv.open;audio_play(SND_MENU_CLICK,70);break;
                case SDLK_j:
                    if(!river_edit&&!path_edit&&!wzone_edit&&!prop_place_mode&&!particle_fx_edit&&
                       !forest_edit&&!wind_edit&&!spawn_edit_mode&&!mount_edit_mode&&!npc_edit_mode&&!sculpt_edit&&!prop_edit&&!marker_edit) break;
                    if(player.state!=PS_DEAD&&!save_ui.open&&!g_shop.open&&!map_screen.open){
                        if(g_player_appearance.open){
                            g_player_appearance.open=false;
                            appearance_set_enabled_from_saved(&g_player_appearance,g_player_tint_enabled);
                        }else{
                            g_player_appearance.open=true;
                            inv.open=false;g_skilltree.open=false;g_player_appearance.selected_slot=0;
                            appearance_restore_saved(&g_player_appearance);
                            g_player_tint_enabled=true;
                        }
                        audio_play(SND_MENU_CLICK,70);
                    }
                    break;
                case SDLK_k:
                    if(!river_edit&&!path_edit&&!wzone_edit&&!prop_place_mode&&!particle_fx_edit&&
                       !forest_edit&&!wind_edit&&!spawn_edit_mode&&!mount_edit_mode&&!npc_edit_mode&&!sculpt_edit&&!prop_edit&&!marker_edit) break;
                    if(player.state!=PS_DEAD&&!save_ui.open&&!g_shop.open&&!map_screen.open){
                        g_skilltree.open=!g_skilltree.open;
                        if(g_skilltree.open){inv.open=false;g_player_appearance.open=false;}
                        audio_play(SND_MENU_CLICK,70);
                    }
                    break;
                case SDLK_h:
                    if(particle_fx_edit){
                        g_world_fx_editor.light_enabled=!g_world_fx_editor.light_enabled;
                        if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
                    }else if(river_edit||path_edit||wzone_edit||prop_place_mode||forest_edit||wind_edit||spawn_edit_mode||mount_edit_mode||npc_edit_mode||sculpt_edit||prop_edit||marker_edit){
                        if(player.state!=PS_DEAD)
                            quick_use_potion(&inv,&player,&as,&qs,base_max_hp,&save_note);
                    }else if(player.state!=PS_DEAD)
                        quick_use_potion(&inv,&player,&as,&qs,base_max_hp,&save_note);
                    break;
                case SDLK_y:
                    if(npcs->open&&npcs->talk_state==NPC_STATE_QUEST_OFFER){npc_quest_accept(npcs,&qs,&save_note);audio_play(SND_MENU_CLICK,80);}
                    else if(prop_edit&&g_props.count){g_props.list[prop_sel].rot_z+=prop_step_rot;}
                    else if(prop_place_mode&&g_placer.selected_prop>=0&&g_placer.selected_prop<g_props.count&&g_props.list[g_placer.selected_prop].active){
                        g_props.list[g_placer.selected_prop].rot_z+=prop_step_rot;
                    }else if(particle_fx_edit){
                        g_world_fx_editor.intensity=clampf(g_world_fx_editor.intensity+0.1f,0.1f,4.0f);
                        if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
                    }
                    break;
                case SDLK_n:
                    if(npcs->open&&npcs->talk_state==NPC_STATE_QUEST_OFFER){npc_quest_decline(npcs);audio_play(SND_MENU_CLICK,70);}
                    else if(prop_edit&&g_props.count){g_props.list[prop_sel].rot_z-=prop_step_rot;}
                    else if(prop_place_mode&&g_placer.selected_prop>=0&&g_placer.selected_prop<g_props.count&&g_props.list[g_placer.selected_prop].active){
                        g_props.list[g_placer.selected_prop].rot_z-=prop_step_rot;
                    }else if(particle_fx_edit){
                        g_world_fx_editor.color.z=clampf(g_world_fx_editor.color.z-0.05f,0.0f,1.5f);
                        if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
                    }
                    break;
                case SDLK_F3:
                    if((SDL_GetModState()&KMOD_SHIFT)!=0){
                        g_frame_debug.enabled=!g_frame_debug.enabled;
                        printf("[FrameDbg] %s\n",g_frame_debug.enabled?"ON":"OFF");
                    }else{
                        show_debug=!show_debug;
                        prop_edit=show_debug;
                        river_edit=false;path_edit=false;wzone_edit=false;particle_fx_edit=false;prop_place_mode=false;forest_edit=false;wind_edit=false;npc_edit_mode=false;spawn_edit_mode=false;mount_edit_mode=false;sculpt_edit=false;marker_edit=false;
                        if(prop_edit){
                            int first=props_first_active(&g_props);
                            if(first>=0){
                                prop_sel=first;
                                g_placer.selected_prop=first;
                                placer_sync_from_prop(&g_placer,&g_props.list[first]);
                                printf("\n[PropEdit] Selected prop %d: %s\n",first,g_props.list[first].path);
                            }
                        }
                    }
                    break;
                case SDLK_F1:
                    path_edit=!path_edit;
                    river_edit=false;prop_edit=false;show_debug=false;prop_place_mode=false;spawn_edit_mode=false;mount_edit_mode=false;wzone_edit=false;particle_fx_edit=false;forest_edit=false;wind_edit=false;npc_edit_mode=false;sculpt_edit=false;marker_edit=false;
                    SDL_SetRelativeMouseMode(path_edit?SDL_FALSE:SDL_TRUE);
                    printf("[PathEd] %s — %d points, width=%.1f\n",path_edit?"ON":"OFF",g_cpath.count,(double)g_cpath.width);
                    break;
                case SDLK_F2:
                    wzone_edit=!wzone_edit;
                    river_edit=false;prop_edit=false;show_debug=false;prop_place_mode=false;spawn_edit_mode=false;mount_edit_mode=false;path_edit=false;particle_fx_edit=false;forest_edit=false;wind_edit=false;npc_edit_mode=false;sculpt_edit=false;marker_edit=false;
                    SDL_SetRelativeMouseMode(wzone_edit?SDL_FALSE:SDL_TRUE);
                    printf("[WZoneEd] %s — %d zones\n",wzone_edit?"ON":"OFF",g_wzone_count);
                    break;
                case SDLK_F4:
                    river_edit=!river_edit;
                    prop_edit=false;show_debug=false;prop_place_mode=false;path_edit=false;wzone_edit=false;particle_fx_edit=false;forest_edit=false;wind_edit=false;npc_edit_mode=false;spawn_edit_mode=false;mount_edit_mode=false;sculpt_edit=false;marker_edit=false;
                    if(river_edit) printf("[RiverEd] ON — %d points loaded\n",g_criver.count);
                    else           printf("[RiverEd] OFF\n");
                    SDL_SetRelativeMouseMode(river_edit?SDL_FALSE:SDL_TRUE);
                    break;
                case SDLK_F8:
                    prop_place_mode=!prop_place_mode;
                    prop_edit=false;show_debug=false;river_edit=false;spawn_edit_mode=false;mount_edit_mode=false;path_edit=false;wzone_edit=false;forest_edit=false;wind_edit=false;particle_fx_edit=false;npc_edit_mode=false;sculpt_edit=false;marker_edit=false;
                    SDL_SetRelativeMouseMode(prop_place_mode?SDL_FALSE:SDL_TRUE);
                    if(!prop_place_mode) g_placer.search_focus=false;
                    if(prop_place_mode&&g_placer.search_focus) SDL_StartTextInput();
                    else SDL_StopTextInput();
                    printf("[PropPlacer] %s\n",prop_place_mode?"ON":"OFF");
                    break;
                case SDLK_F11:
                    npc_edit_mode=!npc_edit_mode;
                    spawn_edit_mode=false;mount_edit_mode=false;prop_edit=false;show_debug=false;river_edit=false;prop_place_mode=false;path_edit=false;wzone_edit=false;forest_edit=false;wind_edit=false;particle_fx_edit=false;sculpt_edit=false;marker_edit=false;
                    SDL_SetRelativeMouseMode(npc_edit_mode?SDL_FALSE:SDL_TRUE);
                    printf("[NpcEd] %s — %d points\n",npc_edit_mode?"ON":"OFF",g_npc_spawns.count);
                    break;
                case SDLK_F12:
                    if((SDL_GetModState()&KMOD_SHIFT)!=0){
                        mount_edit_mode=!mount_edit_mode;
                        spawn_edit_mode=false;npc_edit_mode=false;prop_edit=false;show_debug=false;river_edit=false;prop_place_mode=false;path_edit=false;wzone_edit=false;forest_edit=false;wind_edit=false;particle_fx_edit=false;sculpt_edit=false;marker_edit=false;
                        SDL_SetRelativeMouseMode(mount_edit_mode?SDL_FALSE:SDL_TRUE);
                        printf("[MountEd] %s — %d mounts\n",mount_edit_mode?"ON":"OFF",g_mounts.count);
                    }else{
                        spawn_edit_mode=!spawn_edit_mode;
                        mount_edit_mode=false;npc_edit_mode=false;prop_edit=false;show_debug=false;river_edit=false;prop_place_mode=false;path_edit=false;wzone_edit=false;forest_edit=false;wind_edit=false;particle_fx_edit=false;sculpt_edit=false;marker_edit=false;
                        SDL_SetRelativeMouseMode(spawn_edit_mode?SDL_FALSE:SDL_TRUE);
                        printf("[SpawnEd] %s — %d points\n",spawn_edit_mode?"ON":"OFF",g_spawns.count);
                    }
                    break;
                case SDLK_TAB:
                    if(wzone_edit){
                        wf_edit_mode=!wf_edit_mode; wf_sel=-1;
                        printf("[F2] Sub-mode: %s\n",wf_edit_mode?"Waterfalls":"Water Zones");
                        break;
                    }
                    if(forest_edit){
                        forest_brush_kind=(forest_brush_kind+1)%7;
                        wall_brush_has_start=false;
                        printf("[ForestEd] Brush -> %s\n",
                               forest_brush_kind==0?"Trees":(forest_brush_kind==1?"Grass":(forest_brush_kind==2?"Flowers":(forest_brush_kind==3?"Boulders":(forest_brush_kind==4?"Walls":(forest_brush_kind==5?"Terrain":"Farm Fields"))))));
                        break;
                    }
                    if(prop_edit&&g_props.count>0){
                        int next=props_next_active(&g_props,prop_sel);
                        if(next>=0){
                            prop_sel=next;
                            g_placer.selected_prop=next;
                            placer_sync_from_prop(&g_placer,&g_props.list[next]);
                            printf("[PropEdit] prop %d: %s rx=%.1f ry=%.1f rz=%.1f scale=%.3f\n",
                                   prop_sel,g_props.list[prop_sel].path,
                                   (double)g_props.list[prop_sel].rot_x,
                                   (double)g_props.list[prop_sel].rot_y,
                                   (double)g_props.list[prop_sel].rot_z,
                                   (double)g_props.list[prop_sel].scale);
                        }
                    } else {
                        if(particle_fx_edit){
                            g_world_fx_editor.kind=(WorldFxKind)((g_world_fx_editor.kind+1)%WORLD_FX_COUNT);
                            {
                                WorldFx tmp;
                                world_fx_apply_defaults(&tmp,g_world_fx_editor.kind);
                                g_world_fx_editor.scale=tmp.scale;
                                g_world_fx_editor.height=tmp.height;
                                g_world_fx_editor.intensity=tmp.intensity;
                                g_world_fx_editor.color=tmp.color;
                                g_world_fx_editor.light_enabled=tmp.light_enabled;
                                g_world_fx_editor.light_color=tmp.light_color;
                                g_world_fx_editor.light_radius=tmp.light_radius;
                                g_world_fx_editor.light_intensity=tmp.light_intensity;
                            }
                            if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
                            break;
                        }
                        save_ui.open=!save_ui.open;audio_play(SND_MENU_CLICK,70);
                    } break;
                case SDLK_r:
                    if((SDL_GetModState()&KMOD_ALT)!=0&&(prop_edit||prop_place_mode)){
                        editor_key_replace_selection();
                        save_notify(&save_note,"Replaced selection");
                        break;
                    }
                    if(wzone_edit&&wf_edit_mode&&wf_sel>=0)
                        {g_wfalls.wf[wf_sel].yaw+=0.1745f;wf_save();}
                    else if(prop_edit&&g_props.count)
                        g_props.list[prop_sel].rot_y+=prop_step_rot;
                    else if(prop_place_mode&&g_placer.selected_prop>=0&&g_placer.selected_prop<g_props.count&&g_props.list[g_placer.selected_prop].active){
                        g_props.list[g_placer.selected_prop].rot_y+=prop_step_rot;
                        g_placer.place_ry=g_props.list[g_placer.selected_prop].rot_y;
                    }else if(particle_fx_edit){
                        g_world_fx_editor.color.x=clampf(g_world_fx_editor.color.x+0.05f,0.0f,1.5f);
                        if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
                    }
                    break;
                case SDLK_l:
                    if(particle_fx_edit){
                        g_world_fx_editor.light_enabled=!g_world_fx_editor.light_enabled;
                        if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
                    }
                    break;
                case SDLK_u:
                    if(particle_fx_edit){
                        g_world_fx_editor.intensity=clampf(g_world_fx_editor.intensity-0.1f,0.1f,4.0f);
                        if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
                    }
                    break;
                case SDLK_v:
                    if((prop_edit||prop_place_mode)&&(SDL_GetModState()&KMOD_CTRL)&&g_prop_copy_count>0){
                        /* Ctrl+V: paste at camera target */
                        prop_multi_paste((Vec3){cam.target.x,0,cam.target.z}); break;
                    }
                    if(prop_edit&&g_props.count){g_props.list[prop_sel].rot_y-=prop_step_rot;}
                    else if(prop_place_mode&&g_placer.selected_prop>=0&&g_placer.selected_prop<g_props.count&&g_props.list[g_placer.selected_prop].active){
                        g_props.list[g_placer.selected_prop].rot_y-=prop_step_rot;
                        g_placer.place_ry=g_props.list[g_placer.selected_prop].rot_y;
                    }
                    else if(particle_fx_edit){
                        g_world_fx_editor.color.y=clampf(g_world_fx_editor.color.y-0.05f,0.0f,1.5f);
                        if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
                    }
                    break;
                case SDLK_t:
                    if(forest_edit&&forest_brush_kind==1){
                        g_grass.author_tint=(GrassTintId)((g_grass.author_tint+1)%GRASS_TINT_COUNT);
                        printf("[GrassBrush] Tint -> %s\n",grass_tint_name(g_grass.author_tint));
                    }
                    else if(prop_edit&&g_props.count){g_props.list[prop_sel].rot_x+=prop_step_rot;}
                    else if(prop_place_mode&&g_placer.selected_prop>=0&&g_placer.selected_prop<g_props.count&&g_props.list[g_placer.selected_prop].active){
                        g_props.list[g_placer.selected_prop].rot_x+=prop_step_rot;
                    }
                    break;
                case SDLK_b:
                    if(prop_edit&&g_props.count){g_props.list[prop_sel].rot_x-=prop_step_rot;}
                    else if(prop_place_mode&&g_placer.selected_prop>=0&&g_placer.selected_prop<g_props.count&&g_props.list[g_placer.selected_prop].active){
                        g_props.list[g_placer.selected_prop].rot_x-=prop_step_rot;
                    }
                    else if(particle_fx_edit){
                        g_world_fx_editor.color.z=clampf(g_world_fx_editor.color.z+0.05f,0.0f,1.5f);
                        if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
                    }
                    break;
                case SDLK_EQUALS:case SDLK_PLUS:
                    if(map_screen.open){map_screen.zoom=clampf(map_screen.zoom*1.25f,0.5f,8.0f);}
                    else if(wind_edit&&wind_sel>=0&&wind_sel<g_wind_zone_count){
                        WindZone z=g_wind_zones[wind_sel];
                        z.strength=clampf(z.strength+0.05f,0.0f,1.0f);
                        windzone_update_with_undo(wind_sel,&z);
                        printf("[WindEd] Zone %d strength: %.2f\n",wind_sel,(double)g_wind_zones[wind_sel].strength);
                    }
                    else if(prop_edit&&g_props.count){g_props.list[prop_sel].scale+=prop_step_scale;
                        if(g_props.list[prop_sel].scale<0.01f)g_props.list[prop_sel].scale=0.01f;}
                    else if(prop_place_mode&&g_placer.selected_prop>=0&&g_placer.selected_prop<g_props.count&&g_props.list[g_placer.selected_prop].active){
                        g_props.list[g_placer.selected_prop].scale+=prop_step_scale;
                        if(g_props.list[g_placer.selected_prop].scale<0.01f)g_props.list[g_placer.selected_prop].scale=0.01f;
                        g_placer.place_scale=g_props.list[g_placer.selected_prop].scale;
                    }else if(particle_fx_edit){
                        g_world_fx_editor.scale=clampf(g_world_fx_editor.scale+prop_step_scale,0.05f,12.0f);
                        if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
                    }break;
                case SDLK_MINUS:
                    if(map_screen.open){map_screen.zoom=clampf(map_screen.zoom*0.80f,0.5f,8.0f);}
                    else if(wind_edit&&wind_sel>=0&&wind_sel<g_wind_zone_count){
                        WindZone z=g_wind_zones[wind_sel];
                        z.strength=clampf(z.strength-0.05f,0.0f,1.0f);
                        windzone_update_with_undo(wind_sel,&z);
                        printf("[WindEd] Zone %d strength: %.2f\n",wind_sel,(double)g_wind_zones[wind_sel].strength);
                    }
                    else if(prop_edit&&g_props.count){g_props.list[prop_sel].scale-=prop_step_scale;
                        if(g_props.list[prop_sel].scale<0.01f)g_props.list[prop_sel].scale=0.01f;}
                    else if(prop_place_mode&&g_placer.selected_prop>=0&&g_placer.selected_prop<g_props.count&&g_props.list[g_placer.selected_prop].active){
                        g_props.list[g_placer.selected_prop].scale-=prop_step_scale;
                        if(g_props.list[g_placer.selected_prop].scale<0.01f)g_props.list[g_placer.selected_prop].scale=0.01f;
                        g_placer.place_scale=g_props.list[g_placer.selected_prop].scale;
                    }else if(particle_fx_edit){
                        g_world_fx_editor.scale=clampf(g_world_fx_editor.scale-prop_step_scale,0.05f,12.0f);
                        if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
                    }break;
                case SDLK_BACKSPACE:
                    if(mount_edit_mode&&mount_sel_idx>=0&&mount_sel_idx<g_mounts.count){
                        int deleted=mount_sel_idx;
                        mount_system_delete(&g_mounts,deleted);
                        if(mount_sel_idx>=g_mounts.count) mount_sel_idx=g_mounts.count-1;
                        save_notify(&save_note,"Mount deleted");
                        printf("[MountEd] Deleted mount %d\n",deleted);
                    }
                    break;
                case SDLK_PAGEUP:
                    if(prop_edit&&g_props.count){g_props.list[prop_sel].pos.y+=0.5f;}
                    else if(prop_place_mode&&g_placer.selected_prop>=0&&g_placer.selected_prop<g_props.count&&g_props.list[g_placer.selected_prop].active){g_props.list[g_placer.selected_prop].pos.y+=0.5f;}
                    else if(marker_edit&&marker_sel>=0&&marker_sel<g_world_markers.count&&g_world_markers.items[marker_sel].active){g_world_markers.items[marker_sel].pos.y+=0.5f;}
                    else if(mount_edit_mode&&mount_sel_idx>=0&&mount_sel_idx<g_mounts.count&&g_mounts.mounts[mount_sel_idx].active){g_mounts.mounts[mount_sel_idx].pos.y+=0.5f;}
                    else if(particle_fx_edit&&g_world_fx.selected>=0&&g_world_fx.selected<g_world_fx.count&&g_world_fx.items[g_world_fx.selected].active){world_fx_move_selected_with_undo(g_undo_ls,(Vec3){0.0f,0.5f,0.0f});}
                    break;
                case SDLK_PAGEDOWN:
                    if(prop_edit&&g_props.count){g_props.list[prop_sel].pos.y-=0.5f;}
                    else if(prop_place_mode&&g_placer.selected_prop>=0&&g_placer.selected_prop<g_props.count&&g_props.list[g_placer.selected_prop].active){g_props.list[g_placer.selected_prop].pos.y-=0.5f;}
                    else if(marker_edit&&marker_sel>=0&&marker_sel<g_world_markers.count&&g_world_markers.items[marker_sel].active){g_world_markers.items[marker_sel].pos.y-=0.5f;}
                    else if(mount_edit_mode&&mount_sel_idx>=0&&mount_sel_idx<g_mounts.count&&g_mounts.mounts[mount_sel_idx].active){g_mounts.mounts[mount_sel_idx].pos.y-=0.5f;}
                    else if(particle_fx_edit&&g_world_fx.selected>=0&&g_world_fx.selected<g_world_fx.count&&g_world_fx.items[g_world_fx.selected].active){world_fx_move_selected_with_undo(g_undo_ls,(Vec3){0.0f,-0.5f,0.0f});}
                    break;
                case SDLK_UP:
                    if(prop_edit&&g_props.count){Vec3 fw=cam_forward(&cam);g_props.list[prop_sel].pos.x+=fw.x*0.5f;g_props.list[prop_sel].pos.z+=fw.z*0.5f;}
                    else if(prop_place_mode&&g_placer.selected_prop>=0&&g_placer.selected_prop<g_props.count&&g_props.list[g_placer.selected_prop].active){Vec3 fw=cam_forward(&cam);g_props.list[g_placer.selected_prop].pos.x+=fw.x*0.5f;g_props.list[g_placer.selected_prop].pos.z+=fw.z*0.5f;}
                    else if(marker_edit&&marker_sel>=0&&marker_sel<g_world_markers.count&&g_world_markers.items[marker_sel].active){Vec3 fw=cam_forward(&cam);g_world_markers.items[marker_sel].pos.x+=fw.x*0.5f;g_world_markers.items[marker_sel].pos.z+=fw.z*0.5f;}
                    else if(mount_edit_mode&&mount_sel_idx>=0&&mount_sel_idx<g_mounts.count&&g_mounts.mounts[mount_sel_idx].active){Vec3 fw=cam_forward(&cam);g_mounts.mounts[mount_sel_idx].pos.x+=fw.x*0.5f;g_mounts.mounts[mount_sel_idx].pos.z+=fw.z*0.5f;}
                    else if(particle_fx_edit&&g_world_fx.selected>=0&&g_world_fx.selected<g_world_fx.count&&g_world_fx.items[g_world_fx.selected].active){Vec3 fw=cam_forward(&cam);world_fx_move_selected_with_undo(g_undo_ls,(Vec3){fw.x*0.5f,0.0f,fw.z*0.5f});}
                    break;
                case SDLK_DOWN:
                    if(prop_edit&&g_props.count){Vec3 fw=cam_forward(&cam);g_props.list[prop_sel].pos.x-=fw.x*0.5f;g_props.list[prop_sel].pos.z-=fw.z*0.5f;}
                    else if(prop_place_mode&&g_placer.selected_prop>=0&&g_placer.selected_prop<g_props.count&&g_props.list[g_placer.selected_prop].active){Vec3 fw=cam_forward(&cam);g_props.list[g_placer.selected_prop].pos.x-=fw.x*0.5f;g_props.list[g_placer.selected_prop].pos.z-=fw.z*0.5f;}
                    else if(marker_edit&&marker_sel>=0&&marker_sel<g_world_markers.count&&g_world_markers.items[marker_sel].active){Vec3 fw=cam_forward(&cam);g_world_markers.items[marker_sel].pos.x-=fw.x*0.5f;g_world_markers.items[marker_sel].pos.z-=fw.z*0.5f;}
                    else if(mount_edit_mode&&mount_sel_idx>=0&&mount_sel_idx<g_mounts.count&&g_mounts.mounts[mount_sel_idx].active){Vec3 fw=cam_forward(&cam);g_mounts.mounts[mount_sel_idx].pos.x-=fw.x*0.5f;g_mounts.mounts[mount_sel_idx].pos.z-=fw.z*0.5f;}
                    else if(particle_fx_edit&&g_world_fx.selected>=0&&g_world_fx.selected<g_world_fx.count&&g_world_fx.items[g_world_fx.selected].active){Vec3 fw=cam_forward(&cam);world_fx_move_selected_with_undo(g_undo_ls,(Vec3){-fw.x*0.5f,0.0f,-fw.z*0.5f});}
                    break;
                case SDLK_LEFT:
                    if(prop_edit&&g_props.count){Vec3 ri=cam_right(&cam);g_props.list[prop_sel].pos.x-=ri.x*0.5f;g_props.list[prop_sel].pos.z-=ri.z*0.5f;}
                    else if(prop_place_mode&&g_placer.selected_prop>=0&&g_placer.selected_prop<g_props.count&&g_props.list[g_placer.selected_prop].active){Vec3 ri=cam_right(&cam);g_props.list[g_placer.selected_prop].pos.x-=ri.x*0.5f;g_props.list[g_placer.selected_prop].pos.z-=ri.z*0.5f;}
                    else if(marker_edit&&marker_sel>=0&&marker_sel<g_world_markers.count&&g_world_markers.items[marker_sel].active){Vec3 ri=cam_right(&cam);g_world_markers.items[marker_sel].pos.x-=ri.x*0.5f;g_world_markers.items[marker_sel].pos.z-=ri.z*0.5f;}
                    else if(mount_edit_mode&&mount_sel_idx>=0&&mount_sel_idx<g_mounts.count&&g_mounts.mounts[mount_sel_idx].active){Vec3 ri=cam_right(&cam);g_mounts.mounts[mount_sel_idx].pos.x-=ri.x*0.5f;g_mounts.mounts[mount_sel_idx].pos.z-=ri.z*0.5f;}
                    else if(particle_fx_edit&&g_world_fx.selected>=0&&g_world_fx.selected<g_world_fx.count&&g_world_fx.items[g_world_fx.selected].active){Vec3 ri=cam_right(&cam);world_fx_move_selected_with_undo(g_undo_ls,(Vec3){-ri.x*0.5f,0.0f,-ri.z*0.5f});}
                    break;
                case SDLK_RIGHT:
                    if(prop_edit&&g_props.count){Vec3 ri=cam_right(&cam);g_props.list[prop_sel].pos.x+=ri.x*0.5f;g_props.list[prop_sel].pos.z+=ri.z*0.5f;}
                    else if(prop_place_mode&&g_placer.selected_prop>=0&&g_placer.selected_prop<g_props.count&&g_props.list[g_placer.selected_prop].active){Vec3 ri=cam_right(&cam);g_props.list[g_placer.selected_prop].pos.x+=ri.x*0.5f;g_props.list[g_placer.selected_prop].pos.z+=ri.z*0.5f;}
                    else if(marker_edit&&marker_sel>=0&&marker_sel<g_world_markers.count&&g_world_markers.items[marker_sel].active){Vec3 ri=cam_right(&cam);g_world_markers.items[marker_sel].pos.x+=ri.x*0.5f;g_world_markers.items[marker_sel].pos.z+=ri.z*0.5f;}
                    else if(mount_edit_mode&&mount_sel_idx>=0&&mount_sel_idx<g_mounts.count&&g_mounts.mounts[mount_sel_idx].active){Vec3 ri=cam_right(&cam);g_mounts.mounts[mount_sel_idx].pos.x+=ri.x*0.5f;g_mounts.mounts[mount_sel_idx].pos.z+=ri.z*0.5f;}
                    else if(particle_fx_edit&&g_world_fx.selected>=0&&g_world_fx.selected<g_world_fx.count&&g_world_fx.items[g_world_fx.selected].active){Vec3 ri=cam_right(&cam);world_fx_move_selected_with_undo(g_undo_ls,(Vec3){ri.x*0.5f,0.0f,ri.z*0.5f});}
                    break;
                case SDLK_LEFTBRACKET:
                    if(mount_edit_mode){
                        if(mount_sel_idx>=0&&mount_sel_idx<g_mounts.count&&g_mounts.mounts[mount_sel_idx].active)
                            g_mounts.mounts[mount_sel_idx].yaw-=0.1745f;
                        else
                            mount_place_yaw-=0.1745f;
                        break;
                    }
                    if(forest_edit&&g_forest.fbx_path_count>0){
                        g_forest.active_model_idx--;
                        if(g_forest.active_model_idx<-1) g_forest.active_model_idx=g_forest.fbx_path_count-1;
                        if(g_forest.active_model_idx<0)
                            printf("[ForestEd] Model: RANDOM\n");
                        else{
                            const char *mn=strrchr(g_forest.fbx_paths[g_forest.active_model_idx],'/');
                            printf("[ForestEd] Model %d: %s\n",g_forest.active_model_idx+1,mn?mn+1:g_forest.fbx_paths[g_forest.active_model_idx]);
                        }
                        break;
                    }
                    if(particle_fx_edit){
                        g_world_fx_editor.height=clampf(g_world_fx_editor.height-0.1f,0.1f,8.0f);
                        if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
                    }
                    else if(wzone_edit&&!wf_edit_mode&&wzone_sel>=0){
                        WaterZone z=g_wzones[wzone_sel];
                        z.level-=1.0f;
                        wzone_update_with_undo(wzone_sel,&z,&terrain);
                        printf("[WZoneEd] Zone %d level: %.1f\n",
                               wzone_sel,(double)g_wzones[wzone_sel].level);
                    } else if(wind_edit&&wind_sel>=0&&wind_sel<g_wind_zone_count){
                        WindZone z=g_wind_zones[wind_sel];
                        z.dir_angle-=0.1745f;
                        windzone_update_with_undo(wind_sel,&z);
                        printf("[WindEd] Zone %d dir: %.0f deg\n",wind_sel,
                               (double)(g_wind_zones[wind_sel].dir_angle*180.0f/(float)M_PI));
                    } else if(prop_place_mode){
                        g_placer.place_ry=fmodf(g_placer.place_ry+15.0f,360.0f);
                    } break;
                case SDLK_RIGHTBRACKET:
                    if(mount_edit_mode){
                        if(mount_sel_idx>=0&&mount_sel_idx<g_mounts.count&&g_mounts.mounts[mount_sel_idx].active)
                            g_mounts.mounts[mount_sel_idx].yaw+=0.1745f;
                        else
                            mount_place_yaw+=0.1745f;
                        break;
                    }
                    if(forest_edit&&g_forest.fbx_path_count>0){
                        g_forest.active_model_idx++;
                        if(g_forest.active_model_idx>=g_forest.fbx_path_count) g_forest.active_model_idx=-1;
                        if(g_forest.active_model_idx<0)
                            printf("[ForestEd] Model: RANDOM\n");
                        else{
                            const char *mn=strrchr(g_forest.fbx_paths[g_forest.active_model_idx],'/');
                            printf("[ForestEd] Model %d: %s\n",g_forest.active_model_idx+1,mn?mn+1:g_forest.fbx_paths[g_forest.active_model_idx]);
                        }
                        break;
                    }
                    if(particle_fx_edit){
                        g_world_fx_editor.height=clampf(g_world_fx_editor.height+0.1f,0.1f,8.0f);
                        if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
                    }
                    else if(wzone_edit&&!wf_edit_mode&&wzone_sel>=0){
                        WaterZone z=g_wzones[wzone_sel];
                        z.level+=1.0f;
                        wzone_update_with_undo(wzone_sel,&z,&terrain);
                        printf("[WZoneEd] Zone %d level: %.1f\n",
                               wzone_sel,(double)g_wzones[wzone_sel].level);
                    } else if(wind_edit&&wind_sel>=0&&wind_sel<g_wind_zone_count){
                        WindZone z=g_wind_zones[wind_sel];
                        z.dir_angle+=0.1745f;
                        windzone_update_with_undo(wind_sel,&z);
                        printf("[WindEd] Zone %d dir: %.0f deg\n",wind_sel,
                               (double)(g_wind_zones[wind_sel].dir_angle*180.0f/(float)M_PI));
                    } else if(prop_place_mode){
                        g_placer.place_ry=fmodf(g_placer.place_ry-15.0f+360.0f,360.0f);
                    } break;
                case SDLK_COMMA:
                    if(wind_edit&&wind_sel>=0&&wind_sel<g_wind_zone_count){
                        WindZone z=g_wind_zones[wind_sel];
                        z.gust=clampf(z.gust-0.05f,0.0f,1.0f);
                        windzone_update_with_undo(wind_sel,&z);
                        printf("[WindEd] Zone %d gust: %.2f\n",wind_sel,(double)g_wind_zones[wind_sel].gust);
                    }
                    break;
                case SDLK_PERIOD:
                    if(wind_edit&&wind_sel>=0&&wind_sel<g_wind_zone_count){
                        WindZone z=g_wind_zones[wind_sel];
                        z.gust=clampf(z.gust+0.05f,0.0f,1.0f);
                        windzone_update_with_undo(wind_sel,&z);
                        printf("[WindEd] Zone %d gust: %.2f\n",wind_sel,(double)g_wind_zones[wind_sel].gust);
                    }
                    break;
                case SDLK_p:
                    if((SDL_GetModState()&KMOD_ALT)!=0&&prop_place_mode){
                        editor_key_stamp_path_current();
                        save_notify(&save_note,"Stamped current asset on path");
                        break;
                    }
                    if(prop_place_mode||prop_edit){
                        if((SDL_GetModState()&KMOD_SHIFT)!=0){
                            int idx=prefab_capture_current_editor_selection(&g_prefabs,&g_props,&g_world_fx,&g_placer);
                            if(idx>=0){
                                g_placer.prefab_mode=true;
                                g_placer.search_focus=false;
                                SDL_StopTextInput();
                                g_placer.prefab_sel=idx;
                                g_prefabs.selected=idx;
                                save_notify(&save_note,"Prefab captured");
                            }else{
                                printf("[Prefabs] Select a prop or multi-select props first\n");
                                save_notify(&save_note,"Prefab capture failed");
                            }
                        }else if(prop_place_mode){
                            g_placer.prefab_mode=!g_placer.prefab_mode;
                            if(g_placer.prefab_mode){
                                g_placer.search_focus=false;
                                SDL_StopTextInput();
                            }
                            printf("[Prefabs] %s\n",g_placer.prefab_mode?"Prefab mode ON":"Prefab mode OFF");
                        }
                    } else if(npc_edit_mode){
                    } else { wx.enabled=!wx.enabled; }
                    break;
                case SDLK_d:
                    if((SDL_GetModState()&KMOD_ALT)!=0&&(prop_edit||prop_place_mode)){
                        editor_key_distribute_x();
                        save_notify(&save_note,"Distributed selection on X");
                        break;
                    }
                    if(prop_place_mode&&g_placer.prefab_mode&&(SDL_GetModState()&KMOD_CTRL)){
                        if(prefab_duplicate_selected(&g_prefabs,&g_placer)){
                            save_notify(&save_note,"Prefab duplicated");
                        }
                        break;
                    }
                    if((prop_edit||prop_place_mode)&&(SDL_GetModState()&KMOD_CTRL)){
                        prop_multi_duplicate(); break;
                    }
                    if(npc_edit_mode){
                    }
                    break;
                case SDLK_e:
                    if(river_edit||path_edit||wzone_edit||prop_place_mode||particle_fx_edit||
                       forest_edit||wind_edit||spawn_edit_mode||mount_edit_mode||npc_edit_mode||sculpt_edit||prop_edit||marker_edit) break;
                    if(player.state!=PS_DEAD){
                        float cf_dist=vec3_dist(player.phys.pos,cf_pos);
                        if(cf_dist<3.5f&&!cooking.active){
                            g_farm_crate_open=false;
                            campfire_menu_open=!campfire_menu_open;
                            campfire_lmb_prev=true;
                        } else if(farm_crate_in_range(player.phys.pos)){
                            farm_crate_toggle(&campfire_menu_open,&campfire_lmb_prev);
                        } else if(fishing.active){
                            fishing_stop(&fishing);
                        } else if(fishing.near_water&&player.phys.on_ground&&player.state!=PS_SWIM){
                            fishing_start(&fishing,fishing.spot,fishing.water_level);
                        } else {
                            npc_interact(npcs,player.phys.pos,&qs);
                        }
                    }
                    break;
                case SDLK_RETURN:case SDLK_KP_ENTER:
                    if(player.state==PS_DEAD){
                        player.health=player.max_health;
                        player.state=PS_IDLE;player.anim_idx=player_amap.idle;
                        player.anim_time=0;player.hit_flash=0;
                        player.phys.pos=(Vec3){cf_wx,terrain_h(cf_wx,cf_wz)+0.1f,cf_wz};
                        player.phys.vel=(Vec3){0,0,0};
                        death_fade=0.0f;
                        player.ooc_timer=6.0f;
                    }break;
                case SDLK_0:
                    if(forest_edit){g_forest.active_model_idx=-1;printf("[ForestEd] Model: RANDOM\n");break;}
                    break;
                case SDLK_1:
                    if(forest_edit){g_forest.active_model_idx=0;{const char *mn=strrchr(g_forest.fbx_paths[0],'/');printf("[ForestEd] Model 1: %s\n",mn?mn+1:g_forest.fbx_paths[0]);}break;}
                    if(sculpt_edit){sculpt_mode=0;printf("[SculptEd] RAISE\n");break;}
                    if(marker_edit){marker_sel_type=0;break;}
                    if(npc_edit_mode){npc_sel_type=0;break;}
                    if(spawn_edit_mode){spawn_sel_type=ETYPE_KNIGHT;break;}
                    if(ability_use(&as,0)&&player.state!=PS_DEAD&&player.state!=PS_MOUNTED){
                        if(player.class_id==PLAYER_CLASS_MAGE){
                            player_cast_mage_fireball(&player,&cam,&projs,&ps,&lvlsys);
                        }else{
                            const PlayerClassProfile *cp=player_class_profile(player.class_id);
                            Vec3 fw=cam_forward(&cam);
                            Vec3 spawn={player.phys.pos.x+fw.x*0.8f,
                                        player.phys.pos.y+1.2f,
                                        player.phys.pos.z+fw.z*0.8f};
                            proj_spawn(&projs,spawn,vec3_norm((Vec3){fw.x,0.08f,fw.z}),22.0f,cp->fireball_damage+lvlsys.dmg_bonus,PROJ_FIREBALL,PROJ_OWNER_PLAYER);
                            ps_fire_burst(&ps,spawn,1.2f);
                            ps_magic_burst(&ps,spawn,1.0f,0.4f,0.0f);
                            audio_play(SND_FIREBALL,90);
                        }
                    }break;
                case SDLK_2:
                    if(forest_edit&&g_forest.fbx_path_count>1){g_forest.active_model_idx=1;{const char *mn=strrchr(g_forest.fbx_paths[1],'/');printf("[ForestEd] Model 2: %s\n",mn?mn+1:g_forest.fbx_paths[1]);}break;}
                    if(sculpt_edit){sculpt_mode=1;printf("[SculptEd] LOWER\n");break;}
                    if(marker_edit){marker_sel_type=1;break;}
                    if(npc_edit_mode){npc_sel_type=1;break;}
                    if(spawn_edit_mode){spawn_sel_type=ETYPE_ARCHER;break;}
                    if(ability_use(&as,1)&&player.state!=PS_DEAD&&player.state!=PS_MOUNTED){
                        if(player.class_id==PLAYER_CLASS_MAGE){
                            player_cast_mage_iceball(&player,&cam,&projs,&ps,&lvlsys);
                        }else{
                            const PlayerClassProfile *cp=player_class_profile(player.class_id);
                            float amt=cp->heal_amount+skilltree_heal_bonus(&g_skilltree);
                            player.health=clampf(player.health+amt,0,player.max_health);
                            ps_magic_burst(&ps,player.phys.pos,0.2f,1.0f,0.3f);
                            ps_magic_burst(&ps,(Vec3){player.phys.pos.x,player.phys.pos.y+1.0f,player.phys.pos.z},0.3f,0.5f,1.0f);
                            {
                                char hbuf[48];
                                snprintf(hbuf,sizeof(hbuf),"+%.0f HP (%s)",(double)amt,as.slots[1].name);
                                save_notify(&save_note,hbuf);
                            }
                            audio_play(SND_HEAL,90);
                        }
                    }break;
                case SDLK_3:
                    if(forest_edit&&g_forest.fbx_path_count>2){g_forest.active_model_idx=2;{const char *mn=strrchr(g_forest.fbx_paths[2],'/');printf("[ForestEd] Model 3: %s\n",mn?mn+1:g_forest.fbx_paths[2]);}break;}
                    if(sculpt_edit){sculpt_mode=2;printf("[SculptEd] FLATTEN\n");break;}
                    if(marker_edit){marker_sel_type=2;break;}
                    if(npc_edit_mode){npc_sel_type=2;break;}
                    if(spawn_edit_mode){spawn_sel_type=ETYPE_BRUTE;break;}
                    if(ability_use(&as,2)&&player.state!=PS_DEAD&&player.state!=PS_MOUNTED&&player.state!=PS_ROLL){
                        if(player.class_id==PLAYER_CLASS_MAGE){
                            player_cast_mage_vine_snare(&player,&cam,enemies,ne,&ps,&lvlsys,&save_note);
                        }else{
                            const PlayerClassProfile *cp=player_class_profile(player.class_id);
                            Vec3 fw=cam_forward(&cam);
                            player.phys.vel.x=fw.x*cp->dash_speed;
                            player.phys.vel.z=fw.z*cp->dash_speed;
                            player.phys.vel.y=2.5f;
                            player.dash_iframes=cp->dash_iframes+skilltree_dash_iframe_bonus(&g_skilltree);
                            player.state=PS_ROLL;
                            player.anim_idx=player_amap.roll;
                            player.roll_timer=0;player.anim_time=0;
                            ps_dust_puff(&ps,player.phys.pos,10);
                            audio_play(SND_DASH,90);
                        }
                    }break;
                case SDLK_4:
                    if(sculpt_edit){sculpt_mode=3;printf("[SculptEd] SMOOTH\n");break;}
                    if(marker_edit){marker_sel_type=3;break;}
                    if(npc_edit_mode){npc_sel_type=3;break;}
                    if(spawn_edit_mode){spawn_sel_type=ETYPE_SHADE;break;}
                    if(ability_use(&as,3)&&player.state!=PS_DEAD&&player.state!=PS_MOUNTED){
                        if(player.class_id==PLAYER_CLASS_MAGE){
                            player_cast_mage_ice_wall(&player,enemies,ne,&ps,&lvlsys,&save_note);
                        }else{
                            const PlayerClassProfile *cp=player_class_profile(player.class_id);
                            player.shield_timer=cp->shield_time+skilltree_shield_bonus(&g_skilltree);
                            ps_magic_burst(&ps,player.phys.pos,0.2f,0.4f,1.0f);
                            ps_magic_burst(&ps,(Vec3){player.phys.pos.x,player.phys.pos.y+0.8f,player.phys.pos.z},0.3f,0.1f,0.8f);
                            {
                                char sbuf[64];
                                snprintf(sbuf,sizeof(sbuf),"%s activated! (%.1fs)",as.slots[3].name,(double)(cp->shield_time+skilltree_shield_bonus(&g_skilltree)));
                                save_notify(&save_note,sbuf);
                            }
                            audio_play(SND_SHIELD,95);
                        }
                    }break;
                case SDLK_5:
                    if(sculpt_edit){sculpt_mode=4;printf("[SculptEd] CLIFF\n");break;}
                    if(marker_edit){marker_sel_type=4;break;}
                    if(spawn_edit_mode){spawn_sel_type=ETYPE_BOSS;break;}
                    if(ability_use(&as,4)&&player.state!=PS_DEAD&&player.state!=PS_MOUNTED){
                        if(player.class_id==PLAYER_CLASS_MAGE){
                            player_cast_mage_rockfall(&player,enemies,ne,&ps,&lvlsys,&save_note);
                        }else{
                            const PlayerClassProfile *cp=player_class_profile(player.class_id);
                            int hits=0;
                            for(int mi=0;mi<ne;mi++){
                                Enemy *me=&enemies[mi];
                                if(!me->active||me->state==ES_DEAD)continue;
                                float md=vec3_dist(me->phys.pos,player.phys.pos);
                                if(md>30.0f)continue;
                                float falloff=1.0f-md/30.0f;
                                me->health-=(cp->meteor_damage+lvlsys.dmg_bonus*1.5f)*falloff;
                                Vec3 kdir=vec3_norm(vec3_sub(me->phys.pos,player.phys.pos));
                                me->phys.vel.x+=kdir.x*12.0f*falloff;
                                me->phys.vel.y+=4.0f*falloff;
                                me->phys.vel.z+=kdir.z*12.0f*falloff;
                                me->state=ES_STAGGER;me->stagger_timer=0.6f;
                                ps_burst(&ps,me->phys.pos,1.0f,0.8f,0.2f);
                                hits++;
                            }
                            for(int bi=0;bi<5;bi++)ps_burst(&ps,
                                (Vec3){player.phys.pos.x+((float)rand()/RAND_MAX-0.5f)*8,
                                       player.phys.pos.y,
                                       player.phys.pos.z+((float)rand()/RAND_MAX-0.5f)*8},
                                1.2f,1.0f,0.1f);
                            g_screen_shake=0.5f;
                            char mbuf[48];snprintf(mbuf,sizeof(mbuf),"Meteor! Hit %d enemies",hits);
                            save_notify(&save_note,mbuf);
                            audio_play(SND_METEOR,118);
                        }
                    }break;
                case SDLK_6:
                    if(sculpt_edit){sculpt_mode=5;printf("[SculptEd] GRASS PAINT\n");break;}
                    break;
                case SDLK_7:
                    if(sculpt_edit){sculpt_mode=6;printf("[SculptEd] WORN PATH\n");break;}
                    break;
                case SDLK_f:
                    if((SDL_GetModState()&KMOD_ALT)!=0&&prop_place_mode){
                        editor_key_toggle_browser_favorite();
                        save_notify(&save_note,"Toggled favorite");
                        break;
                    }
                    if(particle_fx_edit){
                        g_world_fx_editor.color.x=clampf(g_world_fx_editor.color.x-0.05f,0.0f,1.5f);
                        if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
                        break;
                    }
                    if(forest_edit&&forest_brush_kind==2){
                        g_grass.author_flower_type=(FlowerTypeId)((g_grass.author_flower_type+FLOWER_TYPE_COUNT-1)%FLOWER_TYPE_COUNT);
                        printf("[FlowerBrush] Type -> %s\n",flower_type_name(g_grass.author_flower_type));
                        break;
                    }
                    if(wzone_edit&&wf_edit_mode&&wf_sel>=0)
                        {g_wfalls.wf[wf_sel].yaw-=0.1745f;wf_save();break;}
                    if(player.state!=PS_DEAD&&player.state!=PS_MOUNTED&&player.state!=PS_ATTACK&&player.state!=PS_ATTACK2){player.state=PS_ATTACK;player.anim_idx=player_amap.attack;player.attack_timer=0;player.attack_hit=false;player.attack_ranged=false;player.anim_time=0;audio_play(SND_ATTACK,100);}break;
                case SDLK_g:
                    if(particle_fx_edit){
                        g_world_fx_editor.color.y=clampf(g_world_fx_editor.color.y+0.05f,0.0f,1.5f);
                        if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
                    } else if(forest_edit&&forest_brush_kind==1){
                        g_grass.author_style=(GrassStyleId)((g_grass.author_style+1)%GRASS_STYLE_COUNT);
                        printf("[GrassBrush] Style -> %s\n",grass_style_name(g_grass.author_style));
                    } else if(forest_edit&&forest_brush_kind==5){
                        terrain_brush_style=(terrain_brush_style+1)%TERRAIN_PAINT_COUNT;
                        printf("[TerrainBrush] Style -> %s\n",terrain_paint_style_name((TerrainPaintStyle)terrain_brush_style));
                    } else if(forest_edit&&forest_brush_kind==2){
                        g_grass.author_flower_type=(FlowerTypeId)((g_grass.author_flower_type+1)%FLOWER_TYPE_COUNT);
                        printf("[FlowerBrush] Type -> %s\n",flower_type_name(g_grass.author_flower_type));
                    } else if(prop_place_mode){
                        g_placer.snap_y=!g_placer.snap_y;
                        printf("[PropPlacer] SnapY %s\n",g_placer.snap_y?"ON":"OFF");
                    } else if(player.state!=PS_DEAD&&player.state!=PS_MOUNTED&&player.state!=PS_ATTACK&&player.state!=PS_ATTACK2){
                        player.state=PS_ATTACK2;player.anim_idx=player_amap.attack2;
                        player.attack_timer=0;player.attack_hit=false;player.attack_ranged=false;player.anim_time=0;
                        audio_play(SND_ATTACK,100);
                    } break;
                case SDLK_q:
                    if(!river_edit&&!path_edit&&!wzone_edit&&!prop_place_mode&&!particle_fx_edit&&
                       !forest_edit&&!wind_edit&&!spawn_edit_mode&&!mount_edit_mode&&!npc_edit_mode&&!sculpt_edit&&!prop_edit&&!marker_edit) break;
                    if(player.state!=PS_DEAD&&player.state!=PS_ROLL&&player.state!=PS_SWIM&&player.state!=PS_MOUNTED&&player.phys.on_ground&&as.stam>=20.0f){
                    player.state=PS_ROLL;player.anim_idx=player_amap.roll;player.roll_timer=0;player.anim_time=0;player.dash_iframes=0.5f;
                    stam_consume(&as,20.0f);
                    }break;
                }

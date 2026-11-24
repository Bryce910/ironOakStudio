        /* Keep undo system's ls pointer in sync each frame */
        g_undo_ls=&ls;
        g_undo_terrain=&terrain;
        g_active_editor_lighting=&ls;

        if(sculpt_edit){
            int smx,smy;
            Uint32 smb=SDL_GetMouseState(&smx,&smy);
            bool lmb=((smb&SDL_BUTTON(1))!=0)&&!editor_gui_mouse_blocked;
            bool rmb=((smb&SDL_BUTTON(3))!=0)&&!editor_gui_mouse_blocked;
            Vec3 hit; bool got_hit=false;
            got_hit=river_screen_to_world(smx,smy,win_w,win_h,proj,view,&hit);
            if(got_hit) hit.y=terrain_h(hit.x,hit.z);

            const Uint8 *sks=SDL_GetKeyboardState(NULL);
            static bool sk_s_prev=false, sk_z_prev=false;
            bool sk_s=sks[SDL_SCANCODE_S]!=0;
            bool sk_z=sks[SDL_SCANCODE_Z]!=0;
            if(sk_s&&!sk_s_prev){
                if(sculpt_mode==5){
                    grass_mask_save();
                    g_grass.last_center=(Vec3){1e9f,0,1e9f};
                } else if(sculpt_mode==6){
                    worn_mask_save();
                    terrain_style_mask_save();
                    g_grass.last_center=(Vec3){1e9f,0,1e9f};
                } else {
                    sculpt_save();
                    terrain_full_sync(&terrain,true);
                    printf("[SculptEd] Full terrain sync\n");
                }
            }
            if((sk_z&&!sk_z_prev)||g_editor_ui_request_sculpt_undo){
                if(sculpt_mode==5&&grass_mask_has_undo){
                    memcpy(g_grass_mask.d,grass_mask_undo,sizeof(g_grass_mask.d));
                    g_grass_coverage_dirty=true;
                    g_grass_mask_active=true;
                    g_grass_mask_seeded=true;
                    grass_mask_has_undo=false;
                    g_grass.last_center=(Vec3){1e9f,0,1e9f};
                    printf("[GrassPaint] Undo applied\n");
                } else if(sculpt_mode==6&&worn_mask_has_undo){
                    memcpy(g_worn_mask.d,worn_mask_undo,sizeof(g_worn_mask.d));
                    memcpy(g_terrain_style_mask.d,terrain_style_undo,sizeof(g_terrain_style_mask.d));
                    g_worn_mask_active=true;
                    worn_mask_upload(&terrain);
                    terrain_style_mask_upload(&terrain);
                    worn_mask_has_undo=false;
                    g_grass.last_center=(Vec3){1e9f,0,1e9f};
                    printf("[WornPath] Undo applied\n");
                } else if(sculpt_mode<5&&sculpt_has_undo){
                    memcpy(g_sculpt.d, sculpt_undo, sizeof(g_sculpt.d));
                    terrain_full_sync(&terrain,true);
                    sculpt_has_undo=false;
                    printf("[SculptEd] Undo applied\n");
                }
                g_editor_ui_request_sculpt_undo=false;
            }
            sk_s_prev=sk_s; sk_z_prev=sk_z;

            if((lmb||rmb)&&got_hit){
                if(!sculpt_lmb_prev){
                    if(sculpt_mode==5){
                        if(!g_grass_mask_seeded) grass_mask_seed_from_terrain();
                        memcpy(grass_mask_undo,g_grass_mask.d,sizeof(g_grass_mask.d));
                        grass_mask_has_undo=true;
                    } else if(sculpt_mode==6){
                        memcpy(worn_mask_undo,g_worn_mask.d,sizeof(g_worn_mask.d));
                        memcpy(terrain_style_undo,g_terrain_style_mask.d,sizeof(g_terrain_style_mask.d));
                        worn_mask_has_undo=true;
                    } else {
                        memcpy(sculpt_undo, g_sculpt.d, sizeof(g_sculpt.d));
                        sculpt_has_undo=true;
                        if(sculpt_mode==2) sculpt_flatten_h=terrain_h(hit.x,hit.z);
                    }
                }
                if(sculpt_mode==5){
                    grass_mask_brush(hit.x,hit.z,sculpt_radius,sculpt_strength,rmb,g_grass.author_style,g_grass.author_tint);
                    g_grass.last_center=(Vec3){1e9f,0,1e9f};
                } else if(sculpt_mode==6){
                    worn_mask_brush(&terrain,hit.x,hit.z,sculpt_radius,sculpt_strength,rmb,TERRAIN_PAINT_WORN);
                    g_grass.last_center=(Vec3){1e9f,0,1e9f};
                } else {
                    int apply_mode = rmb ? 5 : sculpt_mode;
                    float apply_str = sculpt_strength;
                    sculpt_brush(hit.x, hit.z, sculpt_radius, apply_str, apply_mode, sculpt_flatten_h);
                    terrain_rebuild_near(&terrain, hit.x, hit.z, sculpt_radius + TERRAIN_CELL*4);
                }
            }
            if(sculpt_mode<5&&sculpt_lmb_prev&&!(lmb||rmb)){
                sculpt_save();
                terrain_full_sync(&terrain,true);
                printf("[SculptEd] Auto-saved stroke\n");
            }
            sculpt_lmb_prev=(lmb||rmb);

            if(got_hit){
                int segs=32;
                for(int si=0;si<segs;si++){
                    float a0=(float)si/(float)segs*(float)(2*M_PI);
                    float a1=(float)(si+1)/(float)segs*(float)(2*M_PI);
                    float x0=hit.x+cosf(a0)*sculpt_radius;
                    float z0=hit.z+sinf(a0)*sculpt_radius;
                    float x1=hit.x+cosf(a1)*sculpt_radius;
                    float z1=hit.z+sinf(a1)*sculpt_radius;
                    Vec3 pa={x0,terrain_h(x0,z0)+0.15f,z0};
                    Vec3 pb={x1,terrain_h(x1,z1)+0.15f,z1};
                    float br=sculpt_mode==0?0.3f:sculpt_mode==1?1.0f:sculpt_mode==2?1.0f:sculpt_mode==4?1.0f:sculpt_mode==5?0.20f:sculpt_mode==6?0.72f:0.3f;
                    float bg=sculpt_mode==0?1.0f:sculpt_mode==1?0.3f:sculpt_mode==2?0.7f:sculpt_mode==4?0.6f:sculpt_mode==5?0.95f:sculpt_mode==6?0.56f:0.7f;
                    float bb=sculpt_mode==0?0.3f:sculpt_mode==1?0.3f:sculpt_mode==2?0.3f:sculpt_mode==4?0.2f:sculpt_mode==5?0.25f:sculpt_mode==6?0.24f:1.0f;
                    lr_push(pa,pb,br,bg,bb);
                }
                lr_flush(&lr,proj,view);
            }

            if(!editor_ui_backend_is_imgui(&g_editor_ui)){
                static const char *mode_names[]={"RAISE","LOWER","FLATTEN","SMOOTH","CLIFF","GRASS","WORN"};
                static const float mode_r[]={0.3f,1.0f,1.0f,0.3f,1.0f,0.20f,0.72f};
                static const float mode_g[]={1.0f,0.3f,0.7f,0.7f,0.6f,0.95f,0.56f};
                static const float mode_b[]={0.3f,0.3f,0.3f,1.0f,0.2f,0.25f,0.24f};
                float W=(float)win_w, H=(float)win_h;
                float pw=290, lh=20;
                float px=W-pw-6, py=8;
                hud_rect(&hud,px-4,py-4,pw+8,lh*8+28, 0,0,0,0.75f);
                hud_str(&hud,px,py,1.4f,"[F9 TERRAIN EDITOR]",0.4f,0.9f,1.0f,1.0f); py+=lh+4;
                for(int mi=0;mi<7;mi++){
                    bool active=(mi==sculpt_mode);
                    if(active) hud_rect(&hud,px-2,py-1,pw+4,lh+2,0.1f,0.2f,0.3f,0.9f);
                    char mbuf[32]; snprintf(mbuf,sizeof(mbuf),"%d  %s",mi+1,mode_names[mi]);
                    hud_str(&hud,px+4,py,1.2f,mbuf,active?1.0f:mode_r[mi],active?1.0f:mode_g[mi],active?0.4f:mode_b[mi],1.0f);
                    py+=lh;
                }
                py+=4;
                { char rbuf[64]; snprintf(rbuf,sizeof(rbuf),"Radius:   %.0f u",(double)sculpt_radius); hud_str(&hud,px,py,1.2f,rbuf,0.8f,0.8f,0.5f,1.0f); py+=lh; }
                { char sbuf[64]; snprintf(sbuf,sizeof(sbuf),"Strength: %.3f",(double)sculpt_strength); hud_str(&hud,px,py,1.2f,sbuf,0.8f,0.8f,0.5f,1.0f); }
                float by=H-8.0f, blh=17.0f;
                hud_rect(&hud,0,by-blh*2-4,W,blh*2+8,0,0,0,0.68f);
                hud_str(&hud,8,by-=blh,1.3f,"LMB=Paint  RMB=Erase  1-7=Mode  Wheel=Radius  Shift+Wheel=Strength  S=Save  Z=Undo",0.9f,0.9f,0.6f,1.0f);
                if(sculpt_mode==5) hud_str(&hud,8,by-=blh,1.2f,"[Mode 6 paints grass density mask]",0.5f,0.95f,0.55f,1.0f);
                else if(sculpt_mode==6) hud_str(&hud,8,by-=blh,1.2f,"[Mode 7 paints subtle worn path hints]",0.85f,0.72f,0.45f,1.0f);
                else if(sculpt_has_undo) hud_str(&hud,8,by-=blh,1.2f,"[Z to undo last stroke]",0.6f,0.8f,0.6f,1.0f);
                if(sculpt_mode==5&&grass_mask_has_undo) hud_str(&hud,8,by-=blh,1.2f,"[Z to undo grass stroke]",0.6f,0.8f,0.6f,1.0f);
                if(sculpt_mode==6&&worn_mask_has_undo) hud_str(&hud,8,by-=blh,1.2f,"[Z to undo worn-path stroke]",0.6f,0.8f,0.6f,1.0f);
                hud_flush(&hud);
            }
        }

        if(npc_edit_mode){
            int smx,smy;
            Uint32 smb=SDL_GetMouseState(&smx,&smy);
            bool lmb_now=((smb&SDL_BUTTON(1))!=0)&&!editor_gui_mouse_blocked;
            bool rmb_now=((smb&SDL_BUTTON(3))!=0)&&!editor_gui_mouse_blocked;
            Vec3 hit; bool got_hit=false;
            got_hit=river_screen_to_world(smx,smy,win_w,win_h,proj,view,&hit);
            if(got_hit) hit.y=terrain_h(hit.x,hit.z);

            int hov_idx=-1;
            float hov_d2=0.0f;
            bool hov_screen_hit=false;
            {
                float best_screen=28.0f*28.0f;
                for(int i=0;i<g_npc_spawns.count;i++){
                    NpcSpawnPoint *sp=&g_npc_spawns.points[i];
                    float ndcx,ndcy;
                    float sx,sy,dx,dy,sd2;
                    if(!sp->active)continue;
                    if(!world_to_ndc((Vec3){sp->pos.x,sp->pos.y+1.0f,sp->pos.z},proj,view,&ndcx,&ndcy)) continue;
                    sx=(ndcx*0.5f+0.5f)*(float)win_w;
                    sy=(-ndcy*0.5f+0.5f)*(float)win_h;
                    dx=sx-(float)smx;
                    dy=sy-(float)smy;
                    sd2=dx*dx+dy*dy;
                    if(sd2<best_screen){
                        float wx=sp->pos.x-hit.x;
                        float wz=sp->pos.z-hit.z;
                        best_screen=sd2;
                        hov_idx=i;
                        hov_d2=wx*wx+wz*wz;
                        hov_screen_hit=true;
                    }
                }
            }
            if(hov_idx<0&&got_hit){
                float best=6.0f*6.0f;
                for(int i=0;i<g_npc_spawns.count;i++){
                    NpcSpawnPoint *sp=&g_npc_spawns.points[i];
                    float dx=sp->pos.x-hit.x,dz=sp->pos.z-hit.z;
                    float d2;
                    if(!sp->active)continue;
                    d2=dx*dx+dz*dz;
                    if(d2<best){best=d2;hov_idx=i;hov_d2=d2;}
                }
            }

            if(g_editor_selection.kind==EDITOR_SELECTION_NPC&&
               g_editor_selection.index>=0&&g_editor_selection.index<g_npc_spawns.count&&
               g_npc_spawns.points[g_editor_selection.index].active){
                npc_ed_selected_npc=g_editor_selection.index;
            }else if(npc_ed_selected_npc>=0&&npc_ed_selected_npc<g_npc_spawns.count&&
                     g_npc_spawns.points[npc_ed_selected_npc].active){
                editor_selection_set(EDITOR_SELECTION_NPC,npc_ed_selected_npc);
            }
            if(npc_ed_wp_mode){
                npc_ed_wp_npc=(g_editor_selection.kind==EDITOR_SELECTION_NPC)?g_editor_selection.index:-1;
            }else{
                npc_ed_wp_npc=-1;
            }
            if(npc_ed_dlg_mode){
                npc_ed_dlg_npc=(g_editor_selection.kind==EDITOR_SELECTION_NPC)?g_editor_selection.index:-1;
            }else{
                npc_ed_dlg_npc=-1;
            }
            if(npc_ed_tree_mode){
                npc_ed_tree_npc=(g_editor_selection.kind==EDITOR_SELECTION_NPC)?g_editor_selection.index:-1;
            }else{
                npc_ed_tree_npc=-1;
            }

            if(npc_ed_tree_mode)goto npc_skip_place;
            if(npc_ed_wp_mode&&npc_ed_wp_npc>=0){
                NpcSpawnPoint *wsp=&g_npc_spawns.points[npc_ed_wp_npc];
                if(lmb_now&&!npc_ed_lmb_prev&&got_hit&&wsp->wp_count<NPC_WP_MAX){
                    Vec3 wp=hit; wp.y=scene_ground_y(wp.x,wp.z)+0.05f;
                    NpcSpawnPoint next=*wsp;
                    next.waypoints[next.wp_count++]=wp;
                    editor_command_npc_patch(npc_ed_wp_npc,&next,&npcs,EDITOR_COMMAND_KIND_NPC_PATCH,NULL);
                    printf("[NpcEd] Added waypoint %d at (%.1f,%.1f)\n",next.wp_count,(double)wp.x,(double)wp.z);
                }
                if(wsp->wp_count>0){
                    for(int wi=0;wi<wsp->wp_count;wi++){
                        Vec3 a=wsp->waypoints[wi], b=wsp->waypoints[(wi+1)%wsp->wp_count];
                        lr_push(a,b,0.2f,1.0f,0.2f);
                        lr_push((Vec3){a.x-0.5f,a.y,a.z},(Vec3){a.x+0.5f,a.y,a.z},0.2f,1.0f,0.2f);
                        lr_push((Vec3){a.x,a.y,a.z-0.5f},(Vec3){a.x,a.y,a.z+0.5f},0.2f,1.0f,0.2f);
                    }
                    if(got_hit) lr_push(wsp->waypoints[wsp->wp_count-1],hit,0.5f,1.0f,0.5f);
                }
                lr_flush(&lr,proj,view);
            } else {
                if(lmb_now&&!npc_ed_lmb_prev&&got_hit){
                    if(hov_idx>=0&&(hov_screen_hit||hov_d2<=2.25f)){
                        npc_ed_selected_npc=hov_idx;
                        editor_selection_set(EDITOR_SELECTION_NPC,hov_idx);
                        npc_place_mode=false;
                    }else if(npc_place_mode&&g_npc_spawns.count<MAX_NPC_SPAWNS&&npcs.count<NPC_MAX){
                        const char *npc_type_name=(npc_sel_type>=0&&npc_sel_type<NPC_MODEL_MAX&&NPC_TYPE_NAMES[npc_sel_type])?NPC_TYPE_NAMES[npc_sel_type]:"NPC";
                        int created_idx=-1;
                        if(editor_command_npc_create(hit,npc_ed_yaw,npc_sel_type,npc_type_name,&npcs,&created_idx,NULL)){
                            npc_ed_selected_npc=created_idx;
                            editor_selection_set(EDITOR_SELECTION_NPC,npc_ed_selected_npc);
                            npc_place_mode=false;
                        }
                    }
                }
                if(g_editor_selection.kind==EDITOR_SELECTION_NPC&&
                   g_editor_selection.index>=0&&g_editor_selection.index<g_npc_spawns.count&&
                   g_npc_spawns.points[g_editor_selection.index].active){
                    NpcSpawnPoint *np=&g_npc_spawns.points[g_editor_selection.index];
                    int npc_runtime_idx=editor_command_npc_runtime_index_for_spawn(g_editor_selection.index,&npcs);
                    Vec3 npc_origin={np->pos.x,np->pos.y+1.6f,np->pos.z};
                    float npc_len=clampf(vec3_dist(npc_origin,cam.pos)*0.10f,3.5f,20.0f);
                    int npc_hot_axis=(prop_gizmo_active_axis>0)?prop_gizmo_active_axis:
                        editor_gizmo_pick_axis(npc_origin,npc_len,selection_gizmo_space!=0,np->yaw,smx,smy,win_w,win_h,proj,view);
                    bool selected_npc_hit=(hov_idx==g_editor_selection.index&&(hov_screen_hit||hov_d2<=2.25f));
                    static bool npc_drag_active=false;
                    static bool npc_drag_dirty=false;
                    static Vec3 npc_drag_start_pos;
                    static float npc_drag_start_yaw;
                    if(prop_mouse_mode>0){
                        editor_gizmo_draw(npc_origin,npc_len,selection_gizmo_space!=0,np->yaw,npc_hot_axis,prop_gizmo_active_axis);
                        lr_flush(&lr,proj,view);
                    }
                    if(lmb_now&&!npc_ed_lmb_prev&&
                       ((prop_mouse_mode==1&&(selected_npc_hit||npc_hot_axis>0))||
                        (prop_mouse_mode==2&&(selected_npc_hit||npc_hot_axis>0)))){
                        npc_drag_active=true;
                        npc_drag_dirty=false;
                        prop_gizmo_active_axis=(npc_hot_axis>0)?npc_hot_axis:4;
                        prop_mouse_last_x=smx;
                        prop_mouse_last_y=smy;
                        prop_gizmo_drag_accum=0.0f;
                        npc_drag_start_pos=np->pos;
                        npc_drag_start_yaw=np->yaw;
                    }else if(npc_drag_active&&prop_gizmo_active_axis>0&&lmb_now){
                        if(prop_mouse_mode==1){
                            Vec3 go,gx,gy,gz,ga;
                            float osx,osy,asx,asy,axis_dx,axis_dy,axis_len,delta_px;
                            editor_gizmo_axes(npc_origin,npc_len,selection_gizmo_space!=0,npc_drag_start_yaw,&gx,&gy,&gz);
                            go=npc_origin;
                            ga=prop_gizmo_active_axis==1?gx:(prop_gizmo_active_axis==2?gy:gz);
                            if(world_to_ndc(go,proj,view,&osx,&osy)&&world_to_ndc(ga,proj,view,&asx,&asy)){
                                osx=(osx*0.5f+0.5f)*(float)win_w;
                                osy=(1.0f-(osy*0.5f+0.5f))*(float)win_h;
                                asx=(asx*0.5f+0.5f)*(float)win_w;
                                asy=(1.0f-(asy*0.5f+0.5f))*(float)win_h;
                                axis_dx=asx-osx;
                                axis_dy=asy-osy;
                                axis_len=sqrtf(axis_dx*axis_dx+axis_dy*axis_dy);
                                if(axis_len>1e-4f){
                                    axis_dx/=axis_len;
                                    axis_dy/=axis_len;
                                    delta_px=((float)(smx-prop_mouse_last_x))*axis_dx+((float)(smy-prop_mouse_last_y))*axis_dy;
                                    prop_gizmo_drag_accum+=delta_px;
                                }
                            }
                            {
                                float move_delta=prop_gizmo_drag_accum*cam.dist*0.012f;
                                Vec3 x_axis,y_axis,z_axis;
                                if(selection_gizmo_snap) move_delta=editor_gizmo_snap_value(move_delta,0.5f);
                                np->pos=npc_drag_start_pos;
                                editor_gizmo_basis_yaw(selection_gizmo_space!=0,npc_drag_start_yaw,&x_axis,&y_axis,&z_axis);
                                if(prop_gizmo_active_axis==1) np->pos=vec3_add(np->pos,vec3_scale(x_axis,move_delta));
                                else if(prop_gizmo_active_axis==2) np->pos=vec3_add(np->pos,vec3_scale(y_axis,move_delta));
                                else np->pos=vec3_add(np->pos,vec3_scale(z_axis,move_delta));
                                if(npc_runtime_idx>=0) npcs.npcs[npc_runtime_idx].pos=np->pos;
                            }
                            npc_drag_dirty=true;
                        }else if(prop_mouse_mode==2){
                            float yaw_delta_deg=prop_gizmo_drag_accum*0.8f;
                            Vec3 go,gx,gy,gz,ga;
                            float osx,osy,asx,asy,axis_dx,axis_dy,axis_len,delta_px;
                            editor_gizmo_axes(npc_origin,npc_len,selection_gizmo_space!=0,npc_drag_start_yaw,&gx,&gy,&gz);
                            go=npc_origin;
                            ga=(prop_gizmo_active_axis==2)?gy:gy;
                            if(world_to_ndc(go,proj,view,&osx,&osy)&&world_to_ndc(ga,proj,view,&asx,&asy)){
                                osx=(osx*0.5f+0.5f)*(float)win_w;
                                osy=(1.0f-(osy*0.5f+0.5f))*(float)win_h;
                                asx=(asx*0.5f+0.5f)*(float)win_w;
                                asy=(1.0f-(asy*0.5f+0.5f))*(float)win_h;
                                axis_dx=asx-osx;
                                axis_dy=asy-osy;
                                axis_len=sqrtf(axis_dx*axis_dx+axis_dy*axis_dy);
                                if(axis_len>1e-4f){
                                    axis_dx/=axis_len;
                                    axis_dy/=axis_len;
                                    delta_px=((float)(smx-prop_mouse_last_x))*axis_dx+((float)(smy-prop_mouse_last_y))*axis_dy;
                                    prop_gizmo_drag_accum+=delta_px;
                                }
                            }
                            yaw_delta_deg=prop_gizmo_drag_accum*0.8f;
                            if(selection_gizmo_snap) yaw_delta_deg=editor_gizmo_snap_value(yaw_delta_deg,15.0f);
                            np->yaw=npc_drag_start_yaw+yaw_delta_deg*(float)M_PI/180.0f;
                            if(npc_runtime_idx>=0) npcs.npcs[npc_runtime_idx].yaw=np->yaw;
                            npc_drag_dirty=true;
                        }
                        prop_mouse_last_x=smx;
                        prop_mouse_last_y=smy;
                    }else if(npc_drag_active&&!lmb_now){
                        Vec3 final_pos=np->pos;
                        float final_yaw=np->yaw;
                        np->pos=npc_drag_start_pos;
                        np->yaw=npc_drag_start_yaw;
                        if(npc_runtime_idx>=0){
                            npcs.npcs[npc_runtime_idx].pos=npc_drag_start_pos;
                            npcs.npcs[npc_runtime_idx].yaw=npc_drag_start_yaw;
                        }
                        if(npc_drag_dirty){
                            if(fabsf(final_pos.x-npc_drag_start_pos.x)>0.0001f||
                               fabsf(final_pos.y-npc_drag_start_pos.y)>0.0001f||
                               fabsf(final_pos.z-npc_drag_start_pos.z)>0.0001f)
                                editor_command_npc_set_position(g_editor_selection.index,final_pos,&npcs,NULL);
                            else if(fabsf(final_yaw-npc_drag_start_yaw)>0.0001f)
                                editor_command_npc_set_yaw(g_editor_selection.index,final_yaw,&npcs,NULL);
                        }
                        npc_drag_active=false;
                        npc_drag_dirty=false;
                        prop_gizmo_active_axis=0;
                    }
                    if(!lmb_now&&!npc_drag_active&&prop_gizmo_active_axis>0) prop_gizmo_active_axis=0;
                }
            }

            npc_skip_place:;
            npc_ed_lmb_prev=lmb_now;
            npc_ed_rmb_prev=rmb_now;

            npc_spawns_draw_markers(&g_npc_spawns,&lr,&hud,npc_sel_type,hov_idx,proj,view,win_w,win_h);
            if(!editor_ui_backend_is_imgui(&g_editor_ui)){
                npc_spawns_hud(&hud,&g_npc_spawns,npc_sel_type,hov_idx,win_w,win_h);
                if(!npc_ed_wp_mode&&!npc_ed_dlg_mode&&!npc_ed_tree_mode)
                    npc_spawns_hud_inspector(&hud,&g_npc_spawns,hov_idx,win_w,win_h);
                if(npc_ed_wp_mode) npc_spawns_hud_wpmode(&hud,&g_npc_spawns,npc_ed_wp_npc,win_w,win_h);
                if(npc_ed_dlg_mode) npc_spawns_hud_dialogue(&hud,&g_npc_spawns,npc_ed_dlg_npc,npc_ed_dlg_sel,npc_ed_dlg_buf,win_w,win_h);
                if(npc_ed_tree_mode) npc_spawns_hud_tree_editor(&hud,&g_npc_spawns,npc_ed_tree_npc,npc_ed_tree_sel,npc_ed_tree_choice,npc_ed_tree_editing,npc_ed_tree_field,npc_ed_tree_buf,win_w,win_h);
            }
        }

        if(!npcs.open) npc_draw_hints(&npcs,&hud,player.phys.pos,proj,view,win_w,win_h,&qs);
        if(g_scene_sys.current==SCENE_SHIRE&&map_screen.open) map_draw(&map_screen,&hud,win_w,win_h,player.phys.pos,player.phys.yaw,&qs,enemies,ne,&npcs);
        if(g_shop.open){
            int smx2,smy2;SDL_GetMouseState(&smx2,&smy2);
            shop_draw(&g_shop,&hud,win_w,win_h,smx2,smy2,dt,&g_shop_defs[g_active_shop_id],&inv,&qs,&player,base_max_hp);
        }
        npc_draw(&npcs,&hud,win_w,win_h,&qs);
        wi_draw_hud(&world_items,&hud,player.phys.pos,proj,view,win_w,win_h,total_time);
        prop_pickable_draw_hud(&g_props,&hud,player.phys.pos,proj,view,win_w,win_h);
        fishing_draw(&fishing,&hud,win_w,win_h);
        save_notify_draw(&save_note,&hud,win_w,win_h,dt);
        {int imx,imy;SDL_GetMouseState(&imx,&imy); inv_draw(&inv,&hud,win_w,win_h,imx,imy);}
        {
        const char *forest_detail_line="Brush-specific detail controls are available in this panel.";
        char forest_detail_buf[160];
        char fx_line_b[160];
        char fx_line_c[160];
        bool shell_npc_has_focus=false;
        int shell_npc_focus_index=-1;
        const char *shell_npc_focus_name="";
        char *shell_npc_focus_name_buffer=NULL;
        int shell_npc_focus_name_capacity=0;
        char *shell_npc_focus_backstory_buffer=NULL;
        int shell_npc_focus_backstory_capacity=0;
        int shell_npc_waypoint_count=0;
        float shell_npc_waypoint_x[EDITOR_UI_NPC_WAYPOINT_MAX]={0};
        float shell_npc_waypoint_y[EDITOR_UI_NPC_WAYPOINT_MAX]={0};
        float shell_npc_waypoint_z[EDITOR_UI_NPC_WAYPOINT_MAX]={0};
        float shell_npc_patrol_speed=0.0f;
        float shell_npc_patrol_wait=0.0f;
        bool shell_npc_is_merchant=false;
        int shell_npc_shop_id=0;
        const char *shell_npc_shop_name="";
        int shell_npc_dialogue_count=0;
        const char *shell_npc_dialogue_lines[EDITOR_UI_NPC_DIALOGUE_MAX]={0};
        int shell_npc_tree_count=0;
        int shell_npc_tree_root=-1;
        char *shell_npc_tree_text=NULL;
        int shell_npc_tree_text_capacity=0;
        char *shell_npc_tree_speaker=NULL;
        int shell_npc_tree_speaker_capacity=0;
        int *shell_npc_tree_anim_trigger=NULL;
        int shell_npc_tree_choice_count=0;
        char *shell_npc_tree_choice_labels[EDITOR_UI_NPC_TREE_CHOICES_MAX]={0};
        int *shell_npc_tree_choice_next[EDITOR_UI_NPC_TREE_CHOICES_MAX]={0};

        if(forest_brush_kind==0){
            const char *tree_name="Random";
            if(g_forest.active_model_idx>=0&&g_forest.active_model_idx<g_forest.fbx_path_count){
                const char *full=g_forest.fbx_paths[g_forest.active_model_idx];
                const char *base=strrchr(full,'/');
                tree_name=base?base+1:full;
            }
            snprintf(forest_detail_buf,sizeof(forest_detail_buf),"Tree Model: %s",tree_name);
            forest_detail_line=forest_detail_buf;
        }else if(forest_brush_kind==1){
            snprintf(forest_detail_buf,sizeof(forest_detail_buf),"%s  %s  Size %.2f",
                     grass_style_name(g_grass.author_style),grass_tint_name(g_grass.author_tint),(double)grass_brush_size);
            forest_detail_line=forest_detail_buf;
        }else if(forest_brush_kind==2){
            snprintf(forest_detail_buf,sizeof(forest_detail_buf),"%s  Size %.2f  Density %d",
                     flower_type_name(g_grass.author_flower_type),(double)flower_brush_size,flower_brush_density);
            forest_detail_line=forest_detail_buf;
        }else if(forest_brush_kind==4){
            snprintf(forest_detail_buf,sizeof(forest_detail_buf),"Spacing %.2f  Scale %.2f",(double)wall_brush_spacing,(double)wall_brush_scale);
            forest_detail_line=forest_detail_buf;
        }else if(forest_brush_kind==5){
            snprintf(forest_detail_buf,sizeof(forest_detail_buf),"%s  Power %.2f",
                     terrain_paint_style_name((TerrainPaintStyle)terrain_brush_style),(double)terrain_brush_strength);
            forest_detail_line=forest_detail_buf;
        }else if(forest_brush_kind==6){
            snprintf(forest_detail_buf,sizeof(forest_detail_buf),"Farm field mask  Power %.2f",(double)terrain_brush_strength);
            forest_detail_line=forest_detail_buf;
        }

        if(fx_editor_tab==0){
            snprintf(fx_line_b,sizeof(fx_line_b),"Preset %s  Scale %.2f  Height %.2f",
                     world_fx_kind_name(g_world_fx_editor.kind),(double)g_world_fx_editor.scale,(double)g_world_fx_editor.height);
            snprintf(fx_line_c,sizeof(fx_line_c),"Glow %.2f  Light %s  Radius %.1f",
                     (double)g_world_fx_editor.intensity,g_world_fx_editor.light_enabled?"On":"Off",(double)g_world_fx_editor.light_radius);
        }else if(fx_editor_tab==1){
            snprintf(fx_line_b,sizeof(fx_line_b),"Weather %s  Rate %.2f  Wet %.2f",
                     g_weather_editor_override<0?"Auto":(g_weather_editor_override==1?"Rain":(g_weather_editor_override==2?"Snow":"Off")),
                     (double)g_weather_editor_spawn_mul,(double)g_wetness);
            snprintf(fx_line_c,sizeof(fx_line_c),"Fog %.4f  Overcast %.2f  WX %s",
                     (double)g_fog_density,(double)g_overcast,wx.enabled?"On":"Off");
        }else{
            snprintf(fx_line_b,sizeof(fx_line_b),"Exposure %.2f  Bloom %.2f",
                     (double)g_postfx_exposure_mul,(double)g_postfx_bloom_strength);
            snprintf(fx_line_c,sizeof(fx_line_c),"SSAO %.2f  Shader %s",
                     (double)g_postfx_ssao_strength,g_shader_lab.enabled?"On":"Off");
        }

        {
            int focus_idx=(g_editor_selection.kind==EDITOR_SELECTION_NPC&&
                           g_editor_selection.index>=0&&g_editor_selection.index<g_npc_spawns.count&&
                           g_npc_spawns.points[g_editor_selection.index].active)
                ?g_editor_selection.index
                :(npc_ed_selected_npc>=0&&npc_ed_selected_npc<g_npc_spawns.count&&
                  g_npc_spawns.points[npc_ed_selected_npc].active?npc_ed_selected_npc:-1);
            if(focus_idx>=0&&focus_idx<g_npc_spawns.count){
                NpcSpawnPoint *sp=&g_npc_spawns.points[focus_idx];
                shell_npc_has_focus=true;
                shell_npc_focus_index=focus_idx;
                shell_npc_focus_name=sp->name[0]?sp->name:((sp->model_idx>=0&&sp->model_idx<NPC_MODEL_MAX&&NPC_TYPE_NAMES[sp->model_idx])?NPC_TYPE_NAMES[sp->model_idx]:"NPC");
                shell_npc_focus_name_buffer=sp->name;
                shell_npc_focus_name_capacity=(int)sizeof(sp->name);
                shell_npc_focus_backstory_buffer=sp->backstory;
                shell_npc_focus_backstory_capacity=(int)sizeof(sp->backstory);
                shell_npc_waypoint_count=sp->wp_count;
                for(int i=0;i<sp->wp_count&&i<EDITOR_UI_NPC_WAYPOINT_MAX;i++){
                    shell_npc_waypoint_x[i]=sp->waypoints[i].x;
                    shell_npc_waypoint_y[i]=sp->waypoints[i].y;
                    shell_npc_waypoint_z[i]=sp->waypoints[i].z;
                }
                shell_npc_patrol_speed=sp->patrol_speed>0?sp->patrol_speed:1.4f;
                shell_npc_patrol_wait=sp->patrol_wait>0?sp->patrol_wait:2.0f;
                shell_npc_is_merchant=sp->is_merchant;
                shell_npc_shop_id=sp->shop_id;
                shell_npc_shop_name=SHOP_PROFILE_NAMES[(sp->shop_id>=0&&sp->shop_id<SHOP_PROFILE_COUNT)?sp->shop_id:0];
                shell_npc_dialogue_count=sp->dialogue_count;
                for(int i=0;i<sp->dialogue_count&&i<EDITOR_UI_NPC_DIALOGUE_MAX;i++) shell_npc_dialogue_lines[i]=sp->dialogue[i];
                shell_npc_tree_count=sp->tree_count;
                shell_npc_tree_root=sp->tree_root;
                if(npc_ed_tree_mode&&npc_ed_tree_npc==focus_idx&&npc_ed_tree_sel>=0&&npc_ed_tree_sel<sp->tree_count){
                    NpcTreeNode *tn=&sp->tree[npc_ed_tree_sel];
                    shell_npc_tree_text=tn->text;
                    shell_npc_tree_text_capacity=(int)sizeof(tn->text);
                    shell_npc_tree_speaker=tn->speaker;
                    shell_npc_tree_speaker_capacity=(int)sizeof(tn->speaker);
                    shell_npc_tree_anim_trigger=&tn->anim_trigger;
                    shell_npc_tree_choice_count=tn->num_choices;
                    for(int ci=0;ci<tn->num_choices&&ci<EDITOR_UI_NPC_TREE_CHOICES_MAX;ci++){
                        shell_npc_tree_choice_labels[ci]=tn->choice_label[ci];
                        shell_npc_tree_choice_next[ci]=&tn->choice_next[ci];
                    }
                }
            }
        }

        /* Spawn inspector focus */
        {
            g_shell_spawn_has_selection=false;
            g_shell_spawn_model_idx_ptr=NULL;
            g_shell_spawn_model_scale_ptr=NULL;
            g_shell_spawn_hp_over_ptr=NULL;
            g_shell_spawn_dmg_over_ptr=NULL;
            g_shell_spawn_speed_over_ptr=NULL;
            g_shell_spawn_chase_r_over_ptr=NULL;
            g_shell_spawn_atk_r_over_ptr=NULL;
            g_shell_spawn_atk_cd_over_ptr=NULL;
            g_shell_spawn_scale_over_ptr=NULL;
            if(spawn_edit_mode&&spawn_sel_idx>=0&&spawn_sel_idx<g_spawns.count&&g_spawns.points[spawn_sel_idx].active){
                SpawnPoint *_sp=&g_spawns.points[spawn_sel_idx];
                g_shell_spawn_has_selection=true;
                g_shell_spawn_focus_type_name=EDEF[_sp->etype].name;
                g_shell_spawn_model_idx_ptr=&_sp->model_idx;
                {int _ci=_sp->model_idx-1;
                g_shell_spawn_model_scale_ptr=(_ci>=0&&_ci<g_enemy_model_count)?&g_enemy_custom_scale_mul[_ci]:NULL;}
                g_shell_spawn_hp_over_ptr=&_sp->hp_over;
                g_shell_spawn_dmg_over_ptr=&_sp->dmg_over;
                g_shell_spawn_speed_over_ptr=&_sp->speed_over;
                g_shell_spawn_chase_r_over_ptr=&_sp->chase_r_over;
                g_shell_spawn_atk_r_over_ptr=&_sp->atk_r_over;
                g_shell_spawn_atk_cd_over_ptr=&_sp->atk_cd_over;
                g_shell_spawn_scale_over_ptr=&_sp->scale_over;
                const EnemyDef *_d=&EDEF[_sp->etype];
                g_shell_spawn_def_hp=_d->hp;
                g_shell_spawn_def_dmg=_d->damage;
                g_shell_spawn_def_speed=_d->speed;
                g_shell_spawn_def_chase_r=_d->chase_r;
                g_shell_spawn_def_atk_r=_d->attack_r;
                g_shell_spawn_def_atk_cd=_d->attack_cd;
                g_shell_spawn_def_scale=_d->scale;
            }
        }
        g_npc_ed_wp_mode_ptr=&npc_ed_wp_mode;
        g_npc_ed_wp_npc_ptr=&npc_ed_wp_npc;
        g_npc_ed_selected_npc_ptr=&npc_ed_selected_npc;
        g_npc_ed_dlg_mode_ptr=&npc_ed_dlg_mode;
        g_npc_ed_dlg_npc_ptr=&npc_ed_dlg_npc;
        g_npc_ed_dlg_len_ptr=&npc_ed_dlg_len;
        g_npc_ed_dlg_sel_ptr=&npc_ed_dlg_sel;
        g_npc_ed_tree_mode_ptr=&npc_ed_tree_mode;
        g_npc_ed_tree_npc_ptr=&npc_ed_tree_npc;
        g_npc_ed_tree_sel_ptr=&npc_ed_tree_sel;
        g_npc_ed_tree_choice_ptr=&npc_ed_tree_choice;
        g_quest_edit_mode_ptr=&quest_edit_mode;
        g_quest_ed_sel_ptr=&quest_ed_sel;
        g_quest_ed_obj_sel_ptr=&quest_ed_obj_sel;
        g_quest_ed_name_buf_ptr=quest_ed_name_buf;
        g_quest_ed_desc_buf_ptr=quest_ed_desc_buf;
        g_quest_ed_gold_ptr=&quest_ed_gold;
        g_quest_ed_xp_ptr=&quest_ed_xp;
        g_quest_ed_obj_desc_buf_ptr=quest_ed_obj_desc_buf;
        g_quest_ed_obj_required_ptr=&quest_ed_obj_required;
        g_quest_ed_obj_etype_ptr=&quest_ed_obj_etype;
        g_shell_editor_mode_ptr=&editor_mode;
        g_shell_path_edit_ptr=&path_edit;
        g_shell_wzone_edit_ptr=&wzone_edit;
        g_shell_wf_edit_mode_ptr=&wf_edit_mode;
        g_shell_river_edit_ptr=&river_edit;
        g_shell_prop_edit_ptr=&prop_edit;
        g_shell_prop_place_mode_ptr=&prop_place_mode;
        g_shell_particle_fx_edit_ptr=&particle_fx_edit;
        g_shell_marker_edit_ptr=&marker_edit;
        g_shell_forest_edit_ptr=&forest_edit;
        g_shell_wind_edit_ptr=&wind_edit;
        g_shell_sculpt_edit_ptr=&sculpt_edit;
        g_shell_npc_edit_mode_ptr=&npc_edit_mode;
        g_shell_spawn_edit_mode_ptr=&spawn_edit_mode;
        g_shell_shader_edit_ptr=&shader_edit;
        g_shell_plight_edit_ptr=&plight_edit;
        g_shell_decal_edit_ptr=&decal_edit;
        g_shell_szone_edit_ptr=&szone_edit;
        g_shell_prop_mouse_mode_ptr=&prop_mouse_mode;
        g_shell_wind_sel_ptr=&wind_sel;
        g_shell_marker_sel_ptr=&marker_sel;
        g_shell_marker_sel_type_ptr=&marker_sel_type;
        g_shell_wzone_sel_ptr=&wzone_sel;
        g_shell_spawn_sel_type_ptr=&spawn_sel_type;
        g_shell_spawn_sel_model_ptr=&spawn_sel_model;
        g_shell_spawn_sel_idx_ptr=&spawn_sel_idx;
        g_shell_wf_sel_ptr=&wf_sel;
        g_shell_wf_place_width_ptr=&wf_place_width;
        g_shell_wf_place_height_ptr=&wf_place_height;
        g_shell_wf_place_yaw_ptr=&wf_place_yaw;
        g_shell_forest_brush_kind_ptr=&forest_brush_kind;
        g_shell_forest_brush_radius_ptr=&forest_brush_radius;
        g_shell_forest_brush_density_ptr=&forest_brush_density;
        g_shell_terrain_brush_strength_ptr=&terrain_brush_strength;
        g_shell_terrain_brush_style_ptr=&terrain_brush_style;
        g_shell_wall_brush_spacing_ptr=&wall_brush_spacing;
        g_shell_wall_brush_scale_ptr=&wall_brush_scale;
        g_shell_sculpt_mode_ptr=&sculpt_mode;
        g_shell_sculpt_radius_ptr=&sculpt_radius;
        g_shell_sculpt_strength_ptr=&sculpt_strength;
        g_shell_grass_brush_radius_ptr=&grass_brush_radius;
        g_shell_grass_brush_density_ptr=&grass_brush_density;
        g_shell_grass_brush_size_ptr=&grass_brush_size;
        g_shell_grass_brush_hmin_ptr=&grass_brush_hmin;
        g_shell_grass_brush_hmax_ptr=&grass_brush_hmax;
        g_shell_flower_brush_density_ptr=&flower_brush_density;
        g_shell_flower_brush_size_ptr=&flower_brush_size;
        g_shell_npc_sel_type_ptr=&npc_sel_type;
        g_shell_npc_ed_yaw_ptr=&npc_ed_yaw;
        g_shell_fx_editor_tab_ptr=&fx_editor_tab;
        g_shell_wetness_ptr=&g_wetness;
        g_shell_fog_density_ptr=&g_fog_density;
        g_shell_overcast_ptr=&g_overcast;
        for(int _ti=0;_ti<NPC_MODEL_MAX;_ti++){
            int _cnt=0;
            for(int _j=0;_j<g_npc_spawns.count;_j++)
                if(g_npc_spawns.points[_j].active&&g_npc_spawns.points[_j].model_idx==_ti) _cnt++;
            g_shell_npc_type_counts[_ti]=_cnt;
        }
        dlg_graph_sync(npc_ed_tree_mode,npc_ed_tree_npc);
        draw_world_forge_editor_shell(&hud,win_w,win_h,
                                      &editor,
                                      editor_mode,
                                      path_edit,wzone_edit,wf_edit_mode,
                                      river_edit,prop_edit,particle_fx_edit,
                                      marker_edit,forest_edit,wind_edit,
                                      sculpt_edit,npc_edit_mode,spawn_edit_mode,shader_edit,prop_place_mode,
                                      prop_mouse_mode,
                                      g_props.count,g_world_markers.count,g_npc_spawns.count,g_spawns.count,
                                      &terrain,
                                      &g_forest,&g_grass,&g_world_markers,&g_world_fx,&g_world_fx_editor,&g_spawns,&g_npc_spawns,&npcs,&g_placer,
                                      &cam,&editor_cam_target,&show_debug,&prop_sel,wind_sel,marker_sel,marker_sel_type,g_world_markers.count,wzone_sel,spawn_sel_type,spawn_sel_model,
                                      wf_sel,wf_place_width,wf_place_height,wf_place_yaw,
                                      forest_brush_kind,forest_brush_radius,forest_brush_density,
                                      tree_brush_scale,
                                      wall_brush_spacing,wall_brush_scale,
                                      terrain_brush_strength,terrain_brush_style,
                                      grass_brush_radius,grass_brush_density,grass_brush_size,
                                      grass_brush_hmin,grass_brush_hmax,
                                      flower_brush_density,flower_brush_size,forest_detail_line,
                                      sculpt_mode,sculpt_radius,sculpt_strength,
                                      npc_place_mode,npc_sel_type,npc_ed_yaw,npc_ed_selected_npc,
                                      npc_ed_wp_mode,npc_ed_wp_npc,
                                      npc_ed_dlg_mode,npc_ed_dlg_npc,npc_ed_dlg_buf,npc_ed_dlg_len,npc_ed_dlg_sel,
                                      npc_ed_tree_mode,npc_ed_tree_npc,npc_ed_tree_sel,npc_ed_tree_choice,
                                      shell_npc_has_focus,shell_npc_focus_index,
                                      (shell_npc_has_focus&&shell_npc_focus_index>=0&&shell_npc_focus_index<g_npc_spawns.count)?g_npc_spawns.points[shell_npc_focus_index].model_idx:0,
                                      shell_npc_focus_name,shell_npc_focus_name_buffer,shell_npc_focus_name_capacity,
                                      shell_npc_focus_backstory_buffer,shell_npc_focus_backstory_capacity,
                                      (shell_npc_has_focus&&shell_npc_focus_index>=0&&shell_npc_focus_index<g_npc_spawns.count)?
                                          ((g_npc_spawns.points[shell_npc_focus_index].scale>0.0f)?g_npc_spawns.points[shell_npc_focus_index].scale:1.0f):1.0f,
                                      (shell_npc_has_focus&&shell_npc_focus_index>=0&&shell_npc_focus_index<g_npc_spawns.count)?
                                          g_npc_spawns.points[shell_npc_focus_index].visual_yaw_offset:0.0f,
                                      npc_spawn_collision_enabled_at(shell_npc_focus_index),
                                      npc_spawn_patrol_loop_at(shell_npc_focus_index),
                                      shell_npc_waypoint_count,shell_npc_waypoint_x,shell_npc_waypoint_y,shell_npc_waypoint_z,
                                      shell_npc_patrol_speed,shell_npc_patrol_wait,shell_npc_is_merchant,shell_npc_shop_id,shell_npc_shop_name,
                                      shell_npc_dialogue_count,shell_npc_dialogue_lines,
                                      shell_npc_tree_count,shell_npc_tree_root,shell_npc_tree_text,shell_npc_tree_text_capacity,
                                      shell_npc_tree_speaker,shell_npc_tree_speaker_capacity,shell_npc_tree_anim_trigger,
                                      shell_npc_tree_choice_count,shell_npc_tree_choice_labels,shell_npc_tree_choice_next,
                                      fx_editor_tab,g_wetness,g_fog_density,g_overcast,fx_line_b,fx_line_c,&wx,
                                      &player,&char_model,&player_amap,&qs,&as,enemies,ne,&total_time,&inv,&lvlsys,&save_note,
                                      plight_edit,plight_sel,plight_place_r,plight_place_g,plight_place_b,
                                      plight_place_radius,plight_place_intensity,plight_place_y_offset,
                                      plight_place_flicker,plight_place_flicker_speed,
                                      decal_edit,decal_sel,decal_place_type,
                                      decal_place_size,decal_place_alpha,decal_place_yaw,
                                      szone_edit,szone_sel,szone_sound_idx,
                                      szone_place_radius,szone_place_vol);
        }
        editor_gui_lmb_prev=editor_gui_lmb;
        hud_flush(&hud);

        if(player.shield_timer>0){
            float pulse=0.5f+0.5f*sinf(total_time*8.0f);
            float a=0.08f+0.05f*pulse;
            hud_rect(&hud,0,0,(float)win_w,(float)win_h, 0.1f,0.3f,0.9f,a);
            {
                float max_shield=4.0f;
                float sfrac=clampf(player.shield_timer/max_shield,0,1);
                hud_rect(&hud,0,(float)win_h-5,(float)win_w*sfrac,5, 0.2f,0.5f,1.0f,0.85f);
            }
            hud_flush(&hud);
        }

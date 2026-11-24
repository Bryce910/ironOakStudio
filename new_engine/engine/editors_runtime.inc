        int editor_gui_mx=0,editor_gui_my=0;
        Uint32 editor_gui_mb=SDL_GetMouseState(&editor_gui_mx,&editor_gui_my);
        bool editor_gui_lmb=(editor_gui_mb&SDL_BUTTON(1))!=0;
        bool editor_gui_click=editor_gui_lmb&&!editor_gui_lmb_prev;
        bool editor_imgui_active=editor_ui_backend_is_imgui(&g_editor_ui);
        bool editor_gui_hud_click=editor_gui_click&&!editor_imgui_active;
        float editor_gui_x=14.0f, editor_gui_y=84.0f;
        float editor_gui_w=224.0f, editor_gui_h=560.0f;
        bool editor_gui_mouse_blocked=editor_ui_wants_mouse(&g_editor_ui);
        if(!editor_imgui_active){
            editor_gui_mouse_blocked=
                editor_gui_mouse_blocked||
                (editor_gui_mx>=(int)editor_gui_x&&editor_gui_mx<=(int)(editor_gui_x+editor_gui_w)&&
                editor_gui_my>=(int)editor_gui_y&&editor_gui_my<=(int)(editor_gui_y+editor_gui_h));
            #define EDITOR_GUI_RESET() do{ \
                path_edit=false; wzone_edit=false; wf_edit_mode=false; river_edit=false; \
                prop_edit=false; show_debug=false; prop_place_mode=false; surface_edit=false; collision_edit=false; particle_fx_edit=false; \
                marker_edit=false; forest_edit=false; wind_edit=false; sculpt_edit=false; \
                npc_edit_mode=false; spawn_edit_mode=false; mount_edit_mode=false; shader_edit=false; \
                plight_edit=false; decal_edit=false; szone_edit=false; interior_edit=false; \
            }while(0)
            #define EDITOR_GUI_SAVE_WORLD() do{ \
                cpath_save(); \
                wzone_save(); terrain_rebuild_all(&terrain); wzone_rebuild_vaos(&terrain); \
                wf_save(); \
                criver_save(); \
                placer_save(&g_props); \
                surface_pieces_save(&g_surfaces); \
                collision_volumes_save(&g_collision_volumes); \
                forest_save(&g_forest); \
                grass_mask_save(); \
                flower_mask_save(); \
                worn_mask_save(); \
                terrain_style_mask_save(); \
                farm_field_mask_save(); \
                windzone_save(); \
                world_markers_save(&g_world_markers); \
                world_fx_save(&g_world_fx); \
                placed_lights_save(&g_placed_lights); \
                decals_save(&g_decals); \
                sound_zones_save(&g_sound_zones); \
                interior_material_assignments_save(); \
                spawns_save(&g_spawns); \
                mount_system_save(&g_mounts); \
                npc_spawns_save(&g_npc_spawns); \
                terrain_rebake_hmap(&terrain); \
                save_notify(&save_note,"World data saved"); \
            }while(0)
            #define EDITOR_SIDEBAR_ITEM(YY,IS_ACTIVE,ACTION) do{ \
                float _x=24.0f; float _y=(YY); float _w=216.0f; float _h=17.0f; \
                bool _over=editor_gui_mx>=(int)_x&&editor_gui_mx<=(int)(_x+_w)&&editor_gui_my>=(int)(_y-1.0f)&&editor_gui_my<=(int)(_y+_h); \
                if(_over&&editor_gui_hud_click){ ACTION; audio_play(SND_MENU_CLICK,70); } \
            }while(0)
            EDITOR_SIDEBAR_ITEM(102.0f,sculpt_edit,{
                EDITOR_GUI_RESET(); editor_mode=true; editor_cam_target=cam.target; cam.dist=clampf(cam.dist,18.0f,320.0f); sculpt_edit=true; SDL_SetRelativeMouseMode(SDL_FALSE);
            });
            EDITOR_SIDEBAR_ITEM(122.0f,forest_edit,{
                EDITOR_GUI_RESET(); editor_mode=true; editor_cam_target=cam.target; cam.dist=clampf(cam.dist,18.0f,320.0f); forest_edit=true; SDL_SetRelativeMouseMode(SDL_FALSE);
            });
            EDITOR_SIDEBAR_ITEM(142.0f,forest_edit&&forest_brush_kind==4,{
                EDITOR_GUI_RESET(); editor_mode=true; editor_cam_target=cam.target; cam.dist=clampf(cam.dist,18.0f,320.0f); forest_edit=true; forest_brush_kind=4; SDL_SetRelativeMouseMode(SDL_FALSE);
            });
            EDITOR_SIDEBAR_ITEM(162.0f,wind_edit,{
                EDITOR_GUI_RESET(); editor_mode=true; editor_cam_target=cam.target; cam.dist=clampf(cam.dist,18.0f,320.0f); wind_edit=true; SDL_SetRelativeMouseMode(SDL_FALSE);
            });
            EDITOR_SIDEBAR_ITEM(182.0f,particle_fx_edit,{
                EDITOR_GUI_RESET(); editor_mode=true; editor_cam_target=cam.target; cam.dist=clampf(cam.dist,18.0f,320.0f); particle_fx_edit=true; SDL_SetRelativeMouseMode(SDL_FALSE);
                if(g_world_fx.selected>=0&&g_world_fx.selected<g_world_fx.count&&g_world_fx.items[g_world_fx.selected].active)
                    world_fx_editor_sync_from_fx(&g_world_fx_editor,&g_world_fx.items[g_world_fx.selected]);
                else
                    world_fx_editor_reset(&g_world_fx_editor);
            });
            EDITOR_SIDEBAR_ITEM(202.0f,shader_edit,{
                EDITOR_GUI_RESET(); editor_mode=true; shader_edit=true; editor_cam_target=cam.target; cam.dist=clampf(cam.dist,24.0f,320.0f); SDL_SetRelativeMouseMode(SDL_FALSE); SDL_StopTextInput();
            });
            EDITOR_SIDEBAR_ITEM(266.0f,path_edit,{
                EDITOR_GUI_RESET(); editor_mode=true; editor_cam_target=cam.target; cam.dist=clampf(cam.dist,18.0f,320.0f); path_edit=true; SDL_SetRelativeMouseMode(SDL_FALSE);
            });
            EDITOR_SIDEBAR_ITEM(286.0f,wzone_edit&&!wf_edit_mode,{
                EDITOR_GUI_RESET(); editor_mode=true; editor_cam_target=cam.target; cam.dist=clampf(cam.dist,18.0f,320.0f); wzone_edit=true; wf_edit_mode=false; SDL_SetRelativeMouseMode(SDL_FALSE);
            });
            EDITOR_SIDEBAR_ITEM(306.0f,wzone_edit&&wf_edit_mode,{
                EDITOR_GUI_RESET(); editor_mode=true; editor_cam_target=cam.target; cam.dist=clampf(cam.dist,18.0f,320.0f); wzone_edit=true; wf_edit_mode=true; SDL_SetRelativeMouseMode(SDL_FALSE);
            });
            EDITOR_SIDEBAR_ITEM(326.0f,river_edit,{
                EDITOR_GUI_RESET(); editor_mode=true; editor_cam_target=cam.target; cam.dist=clampf(cam.dist,18.0f,320.0f); river_edit=true; SDL_SetRelativeMouseMode(SDL_FALSE);
            });
            EDITOR_SIDEBAR_ITEM(346.0f,prop_edit,{
                EDITOR_GUI_RESET(); editor_mode=true; editor_cam_target=cam.target; cam.dist=clampf(cam.dist,18.0f,320.0f); show_debug=true; prop_edit=true; SDL_SetRelativeMouseMode(SDL_FALSE);
                if(g_props.count>0){ int first=props_first_active(&g_props); if(first>=0){ prop_sel=first; g_placer.selected_prop=first; editor_selection_set(EDITOR_SELECTION_PROP,first); placer_sync_from_prop(&g_placer,&g_props.list[first]); } }
            });
            EDITOR_SIDEBAR_ITEM(366.0f,prop_place_mode,{
                EDITOR_GUI_RESET(); editor_mode=true; editor_cam_target=cam.target; cam.dist=clampf(cam.dist,18.0f,320.0f); prop_place_mode=true; SDL_SetRelativeMouseMode(SDL_FALSE);
                if(g_placer.search_focus) SDL_StartTextInput(); else SDL_StopTextInput();
            });
            EDITOR_SIDEBAR_ITEM(386.0f,surface_edit,{
                EDITOR_GUI_RESET(); editor_mode=true; editor_cam_target=cam.target; cam.dist=clampf(cam.dist,18.0f,320.0f); surface_edit=true; SDL_SetRelativeMouseMode(SDL_FALSE);
            });
            EDITOR_SIDEBAR_ITEM(406.0f,collision_edit,{
                EDITOR_GUI_RESET(); editor_mode=true; editor_cam_target=cam.target; cam.dist=clampf(cam.dist,18.0f,320.0f); collision_edit=true; SDL_SetRelativeMouseMode(SDL_FALSE);
            });
            EDITOR_SIDEBAR_ITEM(426.0f,marker_edit,{
                EDITOR_GUI_RESET(); editor_mode=true; editor_cam_target=cam.target; cam.dist=clampf(cam.dist,18.0f,320.0f); marker_edit=true; SDL_SetRelativeMouseMode(SDL_FALSE);
            });
            EDITOR_SIDEBAR_ITEM(446.0f,plight_edit,{
                EDITOR_GUI_RESET(); editor_mode=true; editor_cam_target=cam.target; cam.dist=clampf(cam.dist,18.0f,320.0f); plight_edit=true; SDL_SetRelativeMouseMode(SDL_FALSE);
            });
            EDITOR_SIDEBAR_ITEM(466.0f,decal_edit,{
                EDITOR_GUI_RESET(); editor_mode=true; editor_cam_target=cam.target; cam.dist=clampf(cam.dist,18.0f,320.0f); decal_edit=true; SDL_SetRelativeMouseMode(SDL_FALSE);
            });
            EDITOR_SIDEBAR_ITEM(486.0f,szone_edit,{
                EDITOR_GUI_RESET(); editor_mode=true; editor_cam_target=cam.target; cam.dist=clampf(cam.dist,18.0f,320.0f); szone_edit=true; SDL_SetRelativeMouseMode(SDL_FALSE);
            });
            EDITOR_SIDEBAR_ITEM(506.0f,interior_edit,{
                EDITOR_GUI_RESET(); editor_mode=true; editor_cam_target=cam.target; cam.dist=clampf(cam.dist,18.0f,320.0f); interior_edit=true; if(interior_selected_idx<0&&g_interior_idx>=0) interior_selected_idx=g_interior_idx; SDL_SetRelativeMouseMode(SDL_FALSE);
            });
            EDITOR_SIDEBAR_ITEM(550.0f,npc_edit_mode,{
                EDITOR_GUI_RESET(); editor_mode=true; editor_cam_target=cam.target; cam.dist=clampf(cam.dist,18.0f,320.0f); npc_edit_mode=true; SDL_SetRelativeMouseMode(SDL_FALSE);
            });
            EDITOR_SIDEBAR_ITEM(570.0f,spawn_edit_mode,{
                EDITOR_GUI_RESET(); editor_mode=true; editor_cam_target=cam.target; cam.dist=clampf(cam.dist,18.0f,320.0f); spawn_edit_mode=true; SDL_SetRelativeMouseMode(SDL_FALSE);
            });
            EDITOR_SIDEBAR_ITEM(590.0f,mount_edit_mode,{
                EDITOR_GUI_RESET(); editor_mode=true; editor_cam_target=cam.target; cam.dist=clampf(cam.dist,18.0f,320.0f); mount_edit_mode=true; SDL_SetRelativeMouseMode(SDL_FALSE);
            });
            EDITOR_SIDEBAR_ITEM(634.0f,editor_mode&&!path_edit&&!wzone_edit&&!river_edit&&!prop_edit&&!prop_place_mode&&!surface_edit&&!collision_edit&&!particle_fx_edit&&!marker_edit&&!forest_edit&&!wind_edit&&!sculpt_edit&&!npc_edit_mode&&!spawn_edit_mode&&!mount_edit_mode&&!shader_edit&&!plight_edit&&!decal_edit&&!szone_edit&&!interior_edit,{
                EDITOR_GUI_RESET(); editor_mode=true; editor_cam_target=cam.target; cam.dist=clampf(cam.dist,24.0f,320.0f); SDL_SetRelativeMouseMode(SDL_FALSE); SDL_StopTextInput();
            });
            EDITOR_SIDEBAR_ITEM(654.0f,false,{
                EDITOR_GUI_RESET(); editor_mode=false; SDL_SetRelativeMouseMode(SDL_FALSE); SDL_SetCursor(SDL_GetDefaultCursor()); SDL_ShowCursor(SDL_ENABLE); SDL_StopTextInput();
            });
            EDITOR_SIDEBAR_ITEM(674.0f,false,{
                EDITOR_GUI_SAVE_WORLD();
            });
            #undef EDITOR_SIDEBAR_ITEM

            {
                bool over=false;
                float bx=24.0f, by=632.0f;
                float bw=100.0f, bh=24.0f, gap=10.0f;
                #define EDITOR_GUI_BUTTON(LABEL,COL,ROW,ACTION) do{ \
                    float _x=bx+((COL)!=0?(bw+gap):0.0f); \
                    float _y=by+((float)(ROW))*(bh+8.0f); \
                    over=menu_btn(&hud,_x,_y,bw,bh,LABEL,1.05f,editor_gui_mx,editor_gui_my,true,NULL); \
                    if(over&&editor_gui_hud_click){ ACTION; audio_play(SND_MENU_CLICK,70); } \
                }while(0)
                EDITOR_GUI_BUTTON("Save S1",0,0,{
                game_save(0,&player,&qs,&as,enemies,ne,total_time,&inv,&lvlsys);
                save_notify(&save_note,"Saved — Slot 1");
            });
                EDITOR_GUI_BUTTON("Save S2",1,0,{
                game_save(1,&player,&qs,&as,enemies,ne,total_time,&inv,&lvlsys);
                save_notify(&save_note,"Saved — Slot 2");
            });
                EDITOR_GUI_BUTTON("Save S3",0,1,{
                game_save(2,&player,&qs,&as,enemies,ne,total_time,&inv,&lvlsys);
                save_notify(&save_note,"Saved — Slot 3");
            });
                EDITOR_GUI_BUTTON("Save World",1,1,{
                    EDITOR_GUI_SAVE_WORLD();
                });
                #undef EDITOR_GUI_BUTTON
            }
            #undef EDITOR_GUI_SAVE_WORLD
            #undef EDITOR_GUI_RESET
        }

        if(!editor_imgui_active&&(path_edit||wzone_edit||river_edit||particle_fx_edit||marker_edit||forest_edit||wind_edit||sculpt_edit||npc_edit_mode||spawn_edit_mode||mount_edit_mode||shader_edit||plight_edit||decal_edit||szone_edit)){
            float tx=(float)win_w-294.0f, ty=266.0f, tw=276.0f, th=path_edit?454.0f:(particle_fx_edit?390.0f:334.0f);
            float bx=tx+12.0f, by=ty+34.0f, bw=78.0f, bh=24.0f, gap=8.0f;
            bool over_tool_panel=editor_gui_mx>=(int)tx&&editor_gui_mx<=(int)(tx+tw)&&editor_gui_my>=(int)ty&&editor_gui_my<=(int)(ty+th);
            char ibuf1[160]="";
            char ibuf2[160]="";
            char ibuf3[160]="";
            const char *tool_title=
                path_edit?"Path Tool":
                (wzone_edit?(wf_edit_mode?"Waterfall Tool":"Pond and Lake Tool"):
                (river_edit?"River Tool":
                (particle_fx_edit?"Effects":
                (marker_edit?"Markers":
                (forest_edit?"Forest Tool":
                (wind_edit?"Wind Tool":
                (sculpt_edit?"Terrain Tool":
                (npc_edit_mode?"NPC Tool":
                (spawn_edit_mode?"Spawn Tool":
                (mount_edit_mode?"Mount Tool":
                (plight_edit?"Point Lights":
                (decal_edit?"Decals":
                (szone_edit?"Sound Zones":"Shader Lab")))))))))))));
            const char *tool_hint_a=
                path_edit?"Left click adds path points":
                (wzone_edit?(wf_edit_mode?"Left click places/selects a waterfall":"Left click places or selects a pond or lake"):
                (river_edit?"Left click adds river points":
                (particle_fx_edit?"Tune world, weather, and post effects":
                (marker_edit?"Left click places/selects a marker":
                (forest_edit?"Paint world detail directly onto the terrain":
                (wind_edit?"Left click places/selects a wind field":
                (sculpt_edit?"Paint terrain height, grass, or worn paths":
                (npc_edit_mode?"Left click places/selects an NPC":
                (spawn_edit_mode?"Left click places/selects an enemy spawn":
                (mount_edit_mode?"Left click places/selects a rideable mount":
                (plight_edit?"Left click places a point light":
                (decal_edit?"Left click places a surface decal":
                (szone_edit?"Left click places a sound zone":"Choose a shader and reload it live")))))))))))));
            const char *tool_hint_b=
                path_edit?"Right click removes the last point":
                (wzone_edit?(wf_edit_mode?"Right click deletes the hovered waterfall":"Right click deletes the hovered pond or lake"):
                (river_edit?"Right click removes the last point":
                (particle_fx_edit?"Use tabs to switch editing groups":
                (marker_edit?"Right click deletes the hovered marker":
                (forest_edit?"Radius and brush type are shown below":
                (wind_edit?"Right click deletes the hovered wind field":
                (sculpt_edit?"Use Undo after a bad stroke":
                (npc_edit_mode?"Use yaw to face the NPC before placing":
                (spawn_edit_mode?"Pick a spawn type, then place it":
                (mount_edit_mode?"Bracket keys rotate, arrows move, Backspace deletes":
                (plight_edit?"Right click removes the nearest light":
                (decal_edit?"Right click removes the nearest decal":
                (szone_edit?"Right click removes the nearest sound zone":"Edit the file on disk, then press Reload")))))))))))));
            editor_shell_panel(&hud,tx,ty,tw,th);
            editor_gui_mouse_blocked=editor_gui_mouse_blocked||over_tool_panel;
            menu_text(&hud,tx+14.0f,ty+9.0f,1.00f,tool_title,0.94f,0.78f,0.30f,0.99f);
            hud_str(&hud,tx+14.0f,ty+24.0f,0.72f,tool_hint_a,0.83f,0.82f,0.68f,0.94f);
            hud_str(&hud,tx+14.0f,ty+36.0f,0.70f,tool_hint_b,0.58f,0.68f,0.42f,0.90f);
            #define TOOL_BTN(COL,ROW,LABEL,ACTION) do{ \
                float _x=bx+((float)(COL))*(bw+gap); \
                float _y=(by+8.0f)+((float)(ROW))*(bh+8.0f); \
                bool _over=menu_btn(&hud,_x,_y,bw,bh,LABEL,1.0f,editor_gui_mx,editor_gui_my,true,NULL); \
                if(_over&&editor_gui_click){ ACTION; audio_play(SND_MENU_CLICK,70); } \
            }while(0)
            #define TOOL_ROW(ROW,LABEL,VALUE,LEFT_LABEL,RIGHT_LABEL,LEFT_ACTION,RIGHT_ACTION) do{ \
                float _y=(by+4.0f)+((float)(ROW))*30.0f; \
                int _step=editor_shell_stepper(&hud,bx,_y,tw-24.0f,26.0f,LABEL,VALUE,editor_gui_mx,editor_gui_my,editor_gui_click,LEFT_LABEL,RIGHT_LABEL); \
                if(_step<0){ LEFT_ACTION; audio_play(SND_MENU_CLICK,70); } \
                else if(_step>0){ RIGHT_ACTION; audio_play(SND_MENU_CLICK,70); } \
            }while(0)
            if(path_edit){
                char width_buf[64];
                char style_buf[64];
                char path_buf[64];
                char tint_r_buf[32],tint_g_buf[32],tint_b_buf[32];
                snprintf(width_buf,sizeof(width_buf),"%.1f m",(double)g_cpath.width);
                snprintf(style_buf,sizeof(style_buf),"%s",cpath_style_name(g_cpath.style));
                snprintf(path_buf,sizeof(path_buf),"%d / %d",g_cpath_slot>=0?g_cpath_slot+1:g_saved_cpath_count+1,g_saved_cpath_count+(g_cpath.count>=2&&g_cpath_slot<0?1:0));
                snprintf(tint_r_buf,sizeof(tint_r_buf),"%.2f",(double)g_cpath.tint.x);
                snprintf(tint_g_buf,sizeof(tint_g_buf),"%.2f",(double)g_cpath.tint.y);
                snprintf(tint_b_buf,sizeof(tint_b_buf),"%.2f",(double)g_cpath.tint.z);
                TOOL_ROW(0,"Path",path_buf,"<",">",{
                    if(g_saved_cpath_count>0){
                        cpath_store_active();
                        int next=(g_cpath_slot>=0?g_cpath_slot:g_saved_cpath_count)-1;
                        if(next<0) next=g_saved_cpath_count-1;
                        cpath_select_slot(next);
                    }
                },{
                    if(g_saved_cpath_count>0){
                        cpath_store_active();
                        int next=(g_cpath_slot>=0?g_cpath_slot:-1)+1;
                        if(next>=g_saved_cpath_count) next=0;
                        cpath_select_slot(next);
                    }
                });
                TOOL_ROW(1,"Width",width_buf,"-","+",{
                    g_cpath.width=clampf(g_cpath.width-0.5f,0.5f,40.0f);
                    if(g_cpath.count>=2) path_rebuild_mesh();
                },{
                    g_cpath.width=clampf(g_cpath.width+0.5f,0.5f,40.0f);
                    if(g_cpath.count>=2) path_rebuild_mesh();
                });
                TOOL_ROW(2,"Surface",style_buf,"<",">",{
                    g_cpath.style=(g_cpath.style+CPATH_STYLE_COUNT-1)%CPATH_STYLE_COUNT;
                    if(g_cpath.count>=2) path_rebuild_mesh();
                },{
                    g_cpath.style=(g_cpath.style+1)%CPATH_STYLE_COUNT;
                    if(g_cpath.count>=2) path_rebuild_mesh();
                });
                TOOL_ROW(3,"Tint R",tint_r_buf,"-","+",{
                    g_cpath.tint.x=clampf(g_cpath.tint.x-0.05f,0.35f,1.75f);
                },{
                    g_cpath.tint.x=clampf(g_cpath.tint.x+0.05f,0.35f,1.75f);
                });
                TOOL_ROW(4,"Tint G",tint_g_buf,"-","+",{
                    g_cpath.tint.y=clampf(g_cpath.tint.y-0.05f,0.35f,1.75f);
                },{
                    g_cpath.tint.y=clampf(g_cpath.tint.y+0.05f,0.35f,1.75f);
                });
                TOOL_ROW(5,"Tint B",tint_b_buf,"-","+",{
                    g_cpath.tint.z=clampf(g_cpath.tint.z-0.05f,0.35f,1.75f);
                },{
                    g_cpath.tint.z=clampf(g_cpath.tint.z+0.05f,0.35f,1.75f);
                });
                TOOL_BTN(0,7,"New Path",{ cpath_store_active(); cpath_select_slot(-1); });
                TOOL_BTN(1,7,"Save Path",{ cpath_save(); });
                TOOL_BTN(2,7,"Undo",{ if(g_cpath.count>0){ g_cpath.count--; if(g_cpath.count>=2) path_rebuild_mesh(); else g_pmesh.valid=false; } });
                TOOL_BTN(0,8,"Clear",{ g_cpath.count=0; g_pmesh.valid=false; });
                snprintf(ibuf1,sizeof(ibuf1),"Path %d of %d   Surface: %s",g_cpath_slot>=0?g_cpath_slot+1:0,g_saved_cpath_count,cpath_style_name(g_cpath.style));
                snprintf(ibuf2,sizeof(ibuf2),"Points: %d / %d   Width: %.1f",g_cpath.count,CPATH_MAX_PTS,(double)g_cpath.width);
                snprintf(ibuf3,sizeof(ibuf3),"Tint: %.2f %.2f %.2f",(double)g_cpath.tint.x,(double)g_cpath.tint.y,(double)g_cpath.tint.z);
            }else if(wzone_edit&&!wf_edit_mode){
                char radius_buf[64];
                char level_buf[64];
                char preset_buf[64];
                if(wzone_sel>=0&&wzone_sel<g_wzone_count){
                    snprintf(radius_buf,sizeof(radius_buf),"%.0f m",(double)g_wzones[wzone_sel].radius);
                    snprintf(level_buf,sizeof(level_buf),"%.2f",(double)g_wzones[wzone_sel].level);
                    snprintf(preset_buf,sizeof(preset_buf),"%s",WATER_PRESETS[g_wzones[wzone_sel].preset_idx].name);
                }else{
                    snprintf(radius_buf,sizeof(radius_buf),"Select zone");
                    snprintf(level_buf,sizeof(level_buf),"Select zone");
                    snprintf(preset_buf,sizeof(preset_buf),"Select zone");
                }
                TOOL_ROW(0,"Radius",radius_buf,"-","+",{
                    if(wzone_sel>=0&&wzone_sel<g_wzone_count){ WaterZone z=g_wzones[wzone_sel]; z.radius=clampf(z.radius-5.0f,10.0f,1000.0f); wzone_update_with_undo(wzone_sel,&z,&terrain); }
                },{
                    if(wzone_sel>=0&&wzone_sel<g_wzone_count){ WaterZone z=g_wzones[wzone_sel]; z.radius=clampf(z.radius+5.0f,10.0f,1000.0f); wzone_update_with_undo(wzone_sel,&z,&terrain); }
                });
                TOOL_ROW(1,"Water Level",level_buf,"-","+",{
                    if(wzone_sel>=0&&wzone_sel<g_wzone_count){ WaterZone z=g_wzones[wzone_sel]; z.level-=0.25f; wzone_update_with_undo(wzone_sel,&z,&terrain); }
                },{
                    if(wzone_sel>=0&&wzone_sel<g_wzone_count){ WaterZone z=g_wzones[wzone_sel]; z.level+=0.25f; wzone_update_with_undo(wzone_sel,&z,&terrain); }
                });
                TOOL_ROW(2,"Surface",preset_buf,"<",">",{
                    if(wzone_sel>=0&&wzone_sel<g_wzone_count){ WaterZone z=g_wzones[wzone_sel]; z.preset_idx=(z.preset_idx+WATER_PRESET_COUNT-1)%WATER_PRESET_COUNT; wzone_update_with_undo(wzone_sel,&z,&terrain); }
                },{
                    if(wzone_sel>=0&&wzone_sel<g_wzone_count){ WaterZone z=g_wzones[wzone_sel]; z.preset_idx=(z.preset_idx+1)%WATER_PRESET_COUNT; wzone_update_with_undo(wzone_sel,&z,&terrain); }
                });
                TOOL_BTN(0,4,"Waterfalls",{ wf_edit_mode=true; wf_sel=-1; });
                TOOL_BTN(1,4,"Save Water",{ wzone_save(); terrain_rebuild_all(&terrain); terrain_rebake_hmap(&terrain); });
                TOOL_BTN(2,4,"Clear",{ g_wzone_count=0; wzone_sel=-1; terrain_rebuild_all(&terrain); wzone_rebuild_vaos(&terrain); });
                snprintf(ibuf1,sizeof(ibuf1),"Zones: %d   Selected: %d",g_wzone_count,wzone_sel>=0?wzone_sel+1:0);
                if(wzone_sel>=0&&wzone_sel<g_wzone_count){
                    snprintf(ibuf2,sizeof(ibuf2),"Radius: %.0f   Level: %.2f",(double)g_wzones[wzone_sel].radius,(double)g_wzones[wzone_sel].level);
                    snprintf(ibuf3,sizeof(ibuf3),"Preset: %s",WATER_PRESETS[g_wzones[wzone_sel].preset_idx].name);
                }else{
                    snprintf(ibuf2,sizeof(ibuf2),"Click terrain to place or select a pond or lake.");
                    snprintf(ibuf3,sizeof(ibuf3),"Adjust radius, level, and surface to shape the shoreline.");
                }
            }else if(wzone_edit&&wf_edit_mode){
                char wf_width_buf[64];
                char wf_height_buf[64];
                char wf_yaw_buf[64];
                float wf_width=(wf_sel>=0&&wf_sel<g_wfalls.count)?g_wfalls.wf[wf_sel].width:wf_place_width;
                float wf_height=(wf_sel>=0&&wf_sel<g_wfalls.count)?g_wfalls.wf[wf_sel].height:wf_place_height;
                float wf_yaw=(wf_sel>=0&&wf_sel<g_wfalls.count)?g_wfalls.wf[wf_sel].yaw:wf_place_yaw;
                snprintf(wf_width_buf,sizeof(wf_width_buf),"%.1f m",(double)wf_width);
                snprintf(wf_height_buf,sizeof(wf_height_buf),"%.1f m",(double)wf_height);
                snprintf(wf_yaw_buf,sizeof(wf_yaw_buf),"%.0f deg",(double)(wf_yaw*57.2957795f));
                TOOL_ROW(0,"Width",wf_width_buf,"-","+",{
                    if(wf_sel>=0&&wf_sel<g_wfalls.count){ g_wfalls.wf[wf_sel].width=clampf(g_wfalls.wf[wf_sel].width-0.5f,1.0f,80.0f);} else wf_place_width=clampf(wf_place_width-0.5f,1.0f,80.0f);
                },{
                    if(wf_sel>=0&&wf_sel<g_wfalls.count){ g_wfalls.wf[wf_sel].width=clampf(g_wfalls.wf[wf_sel].width+0.5f,1.0f,80.0f);} else wf_place_width=clampf(wf_place_width+0.5f,1.0f,80.0f);
                });
                TOOL_ROW(1,"Height",wf_height_buf,"-","+",{
                    if(wf_sel>=0&&wf_sel<g_wfalls.count){ g_wfalls.wf[wf_sel].height=clampf(g_wfalls.wf[wf_sel].height-1.0f,2.0f,200.0f);} else wf_place_height=clampf(wf_place_height-1.0f,2.0f,200.0f);
                },{
                    if(wf_sel>=0&&wf_sel<g_wfalls.count){ g_wfalls.wf[wf_sel].height=clampf(g_wfalls.wf[wf_sel].height+1.0f,2.0f,200.0f);} else wf_place_height=clampf(wf_place_height+1.0f,2.0f,200.0f);
                });
                TOOL_ROW(2,"Facing",wf_yaw_buf,"<",">",{
                    if(wf_sel>=0&&wf_sel<g_wfalls.count){ g_wfalls.wf[wf_sel].yaw-=0.1745f; } else wf_place_yaw-=0.1745f;
                },{
                    if(wf_sel>=0&&wf_sel<g_wfalls.count){ g_wfalls.wf[wf_sel].yaw+=0.1745f; } else wf_place_yaw+=0.1745f;
                });
                TOOL_BTN(0,4,"Back Zones",{ wf_edit_mode=false; wf_sel=-1; });
                TOOL_BTN(1,4,"Save Falls",{ wf_save(); });
                snprintf(ibuf1,sizeof(ibuf1),"Waterfalls: %d   Selected: %d",g_wfalls.count,wf_sel>=0?wf_sel+1:0);
                if(wf_sel>=0&&wf_sel<g_wfalls.count){
                    snprintf(ibuf2,sizeof(ibuf2),"Width: %.1f   Height: %.1f",(double)g_wfalls.wf[wf_sel].width,(double)g_wfalls.wf[wf_sel].height);
                    snprintf(ibuf3,sizeof(ibuf3),"Yaw: %.0f deg",(double)(g_wfalls.wf[wf_sel].yaw*57.2957795f));
                }else{
                    snprintf(ibuf2,sizeof(ibuf2),"Place Width: %.1f   Height: %.1f",(double)wf_place_width,(double)wf_place_height);
                    snprintf(ibuf3,sizeof(ibuf3),"Preview Yaw: %.0f deg",(double)(wf_place_yaw*57.2957795f));
                }
            }else if(river_edit){
                char river_width_buf[64];
                char river_style_buf[64];
                snprintf(river_width_buf,sizeof(river_width_buf),"%.0f m",(double)g_criver.width);
                snprintf(river_style_buf,sizeof(river_style_buf),"%s",WATER_PRESETS[g_wstyle.preset_idx].name);
                TOOL_ROW(0,"Width",river_width_buf,"-","+",{
                    g_criver.width=clampf(g_criver.width-5.0f,10.0f,400.0f);
                    g_criver.dirty=true;
                    terrain_rebuild_all(&terrain);
                    terrain_rebake_hmap(&terrain);
                },{
                    g_criver.width=clampf(g_criver.width+5.0f,10.0f,400.0f);
                    g_criver.dirty=true;
                    terrain_rebuild_all(&terrain);
                    terrain_rebake_hmap(&terrain);
                });
                TOOL_ROW(1,"Surface",river_style_buf,"<",">",{
                    wstyle_apply_preset((g_wstyle.preset_idx+WATER_PRESET_COUNT-1)%WATER_PRESET_COUNT);
                },{
                    wstyle_apply_preset((g_wstyle.preset_idx+1)%WATER_PRESET_COUNT);
                });
                TOOL_BTN(0,3,"Undo",{ if(g_criver.count>0){ g_criver.count--; g_criver.dirty=true; terrain_rebuild_all(&terrain); terrain_rebake_hmap(&terrain); } });
                TOOL_BTN(1,3,"Clear",{ g_criver.count=0; g_criver.dirty=true; terrain_rebuild_all(&terrain); terrain_rebake_hmap(&terrain); });
                TOOL_BTN(2,3,"Save River",{ criver_save(); terrain_rebuild_all(&terrain); terrain_rebake_hmap(&terrain); });
                snprintf(ibuf1,sizeof(ibuf1),"Points: %d",g_criver.count);
                snprintf(ibuf2,sizeof(ibuf2),"Width: %.0f",(double)g_criver.width);
                snprintf(ibuf3,sizeof(ibuf3),"Water Style: %s",WATER_PRESETS[g_wstyle.preset_idx].name);
            }else if(sculpt_edit){
                char sculpt_mode_buf[64];
                char sculpt_radius_buf[64];
                char sculpt_power_buf[64];
                const char *sculpt_mode_name=
                    sculpt_mode==0?"Raise":
                    sculpt_mode==1?"Lower":
                    sculpt_mode==2?"Flatten":
                    sculpt_mode==3?"Smooth":
                    sculpt_mode==4?"Noise":
                    sculpt_mode==5?"Grass":
                    "Worn";
                snprintf(sculpt_mode_buf,sizeof(sculpt_mode_buf),"%s",sculpt_mode_name);
                snprintf(sculpt_radius_buf,sizeof(sculpt_radius_buf),"%.0f m",(double)sculpt_radius);
                snprintf(sculpt_power_buf,sizeof(sculpt_power_buf),"%.3f",(double)sculpt_strength);
                TOOL_ROW(0,"Mode",sculpt_mode_buf,"<",">",{ sculpt_mode=(sculpt_mode+6)%7; },{
                    sculpt_mode=(sculpt_mode+1)%7;
                });
                TOOL_ROW(1,"Radius",sculpt_radius_buf,"-","+",{
                    sculpt_radius=clampf(sculpt_radius-2.0f,3.0f,300.0f);
                },{
                    sculpt_radius=clampf(sculpt_radius+2.0f,3.0f,300.0f);
                });
                TOOL_ROW(2,"Power",sculpt_power_buf,"-","+",{
                    sculpt_strength=clampf(sculpt_strength-0.005f,0.002f,0.5f);
                },{
                    sculpt_strength=clampf(sculpt_strength+0.005f,0.002f,0.5f);
                });
                TOOL_BTN(0,4,"Save Terrain",{ if(sculpt_mode==5) grass_mask_save(); else if(sculpt_mode==6){ worn_mask_save(); terrain_style_mask_save(); } else { sculpt_save(); terrain_rebake_hmap(&terrain);} });
                TOOL_BTN(1,4,"Undo",{
                    if(sculpt_mode==5&&grass_mask_has_undo){
                        memcpy(g_grass_mask.d,grass_mask_undo,sizeof(g_grass_mask.d));
                        g_grass_mask_active=true;
                        g_grass_mask_seeded=true;
                        grass_mask_has_undo=false;
                        g_grass.last_center.x=1e9f;
                        g_grass.last_center.y=0.0f;
                        g_grass.last_center.z=1e9f;
                    } else if(sculpt_mode==6&&worn_mask_has_undo){
                        memcpy(g_worn_mask.d,worn_mask_undo,sizeof(g_worn_mask.d));
                        memcpy(g_terrain_style_mask.d,terrain_style_undo,sizeof(g_terrain_style_mask.d));
                        g_worn_mask_active=true;
                        worn_mask_upload(&terrain);
                        terrain_style_mask_upload(&terrain);
                        worn_mask_has_undo=false;
                        g_grass.last_center.x=1e9f;
                        g_grass.last_center.y=0.0f;
                        g_grass.last_center.z=1e9f;
                    } else if(sculpt_mode<5&&sculpt_has_undo){
                        memcpy(g_sculpt.d,sculpt_undo,sizeof(g_sculpt.d));
                        terrain_full_sync(&terrain,true);
                        sculpt_has_undo=false;
                    }
                });
                snprintf(ibuf1,sizeof(ibuf1),"Mode: %d   Radius: %.0f",sculpt_mode+1,(double)sculpt_radius);
                snprintf(ibuf2,sizeof(ibuf2),"Power: %.3f", (double)sculpt_strength);
                snprintf(ibuf3,sizeof(ibuf3),"%s",
                         sculpt_mode==5?"Grass density mask painter":
                         (sculpt_mode==6?"Worn path painter":"Terrain height sculptor"));
            }else if(forest_edit){
                TOOL_BTN(0,0,"< Brush",{ forest_brush_kind=(forest_brush_kind+6)%7; });
                TOOL_BTN(1,0,"Brush >",{ forest_brush_kind=(forest_brush_kind+1)%7; });
                TOOL_BTN(2,0,"Save Brush",{ if(forest_brush_kind==0) forest_save(&g_forest); else if(forest_brush_kind==2) flower_mask_save(); else if(forest_brush_kind==3||forest_brush_kind==4) placer_save(&g_props); else if(forest_brush_kind==5){ worn_mask_save(); terrain_style_mask_save(); } else if(forest_brush_kind==6){ farm_field_mask_save(); } else grass_mask_save(); });
                TOOL_BTN(0,1,"< Radius",{ forest_brush_radius=clampf(forest_brush_radius-2.0f,4.0f,220.0f); });
                TOOL_BTN(1,1,"Radius >",{ forest_brush_radius=clampf(forest_brush_radius+2.0f,4.0f,220.0f); });
                TOOL_BTN(2,1,forest_brush_kind==1||forest_brush_kind==2||forest_brush_kind==5||forest_brush_kind==6?"Undo":"Save",{ 
                    if(forest_brush_kind==1&&grass_mask_has_undo){
                        memcpy(g_grass_mask.d,grass_mask_undo,sizeof(g_grass_mask.d));
                        memcpy(g_grass_style_mask.d,grass_style_undo,sizeof(g_grass_style_mask.d));
                        memcpy(g_grass_tint_mask.d,grass_tint_undo,sizeof(g_grass_tint_mask.d));
                        g_grass_mask_active=true;
                        g_grass_mask_seeded=true;
                        grass_mask_has_undo=false;
                        g_grass.last_center.x=1e9f;
                        g_grass.last_center.y=0.0f;
                        g_grass.last_center.z=1e9f;
                    } else if(forest_brush_kind==2&&flower_mask_has_undo){
                        memcpy(g_flower_mask.d,flower_mask_undo,sizeof(g_flower_mask.d));
                        memcpy(g_flower_type_mask.d,flower_type_undo,sizeof(g_flower_type_mask.d));
                        g_flower_mask_active=true;
                        g_flower_mask_seeded=true;
                        flower_mask_has_undo=false;
                        g_grass.last_center.x=1e9f;
                        g_grass.last_center.y=0.0f;
                        g_grass.last_center.z=1e9f;
                    } else if(forest_brush_kind==5&&worn_mask_has_undo){
                        memcpy(g_worn_mask.d,worn_mask_undo,sizeof(g_worn_mask.d));
                        memcpy(g_terrain_style_mask.d,terrain_style_undo,sizeof(g_terrain_style_mask.d));
                        g_worn_mask_active=true;
                        worn_mask_has_undo=false;
                        worn_mask_upload(&terrain);
                        terrain_style_mask_upload(&terrain);
                    } else if(forest_brush_kind==6&&farm_mask_has_undo){
                        memcpy(g_farm_field_mask.d,farm_mask_undo,sizeof(g_farm_field_mask.d));
                        g_farm_field_mask_active=true;
                        farm_mask_has_undo=false;
                        farming_rebuild_from_mask(&g_farming);
                    } else if(forest_brush_kind==3||forest_brush_kind==4){
                        placer_save(&g_props);
                    } else if(forest_brush_kind==0){
                        forest_save(&g_forest);
                    }
                });
                TOOL_BTN(0,2,"Density -",{ forest_brush_density=forest_brush_density>1?forest_brush_density-1:1; });
                TOOL_BTN(1,2,"Density +",{ forest_brush_density=forest_brush_density<120?forest_brush_density+1:120; });
                if(forest_brush_kind==1){
                    TOOL_BTN(2,2,"Size +",{ grass_brush_size=clampf(grass_brush_size+0.08f,0.35f,3.0f); });
                    TOOL_BTN(0,3,"Size -",{ grass_brush_size=clampf(grass_brush_size-0.08f,0.35f,3.0f); });
                    TOOL_BTN(1,3,"Style",{ g_grass.author_style=(GrassStyleId)((g_grass.author_style+1)%GRASS_STYLE_COUNT); });
                    TOOL_BTN(2,3,"Tint",{ g_grass.author_tint=(GrassTintId)((g_grass.author_tint+1)%GRASS_TINT_COUNT); });
                    TOOL_BTN(0,4,"HMin -",{ grass_brush_hmin=clampf(grass_brush_hmin-0.06f,0.20f,3.5f); if(grass_brush_hmin>grass_brush_hmax) grass_brush_hmin=grass_brush_hmax; });
                    TOOL_BTN(1,4,"HMin +",{ grass_brush_hmin=clampf(grass_brush_hmin+0.06f,0.20f,3.5f); if(grass_brush_hmin>grass_brush_hmax) grass_brush_hmin=grass_brush_hmax; });
                    TOOL_BTN(2,4,"HMax +",{ grass_brush_hmax=clampf(grass_brush_hmax+0.08f,0.30f,4.0f); if(grass_brush_hmax<grass_brush_hmin) grass_brush_hmax=grass_brush_hmin; });
                    TOOL_BTN(0,5,"HMax -",{ grass_brush_hmax=clampf(grass_brush_hmax-0.08f,0.30f,4.0f); if(grass_brush_hmax<grass_brush_hmin) grass_brush_hmax=grass_brush_hmin; });
                }else if(forest_brush_kind==2){
                    TOOL_BTN(2,2,"Size +",{ flower_brush_size=clampf(flower_brush_size+0.08f,0.35f,3.0f); });
                    TOOL_BTN(0,3,"Size -",{ flower_brush_size=clampf(flower_brush_size-0.08f,0.35f,3.0f); });
                    TOOL_BTN(1,3,"Type -",{ g_grass.author_flower_type=(FlowerTypeId)((g_grass.author_flower_type+FLOWER_TYPE_COUNT-1)%FLOWER_TYPE_COUNT); });
                    TOOL_BTN(2,3,"Type +",{ g_grass.author_flower_type=(FlowerTypeId)((g_grass.author_flower_type+1)%FLOWER_TYPE_COUNT); });
                }else if(forest_brush_kind==4){
                    TOOL_BTN(2,2,"Spc +",{ wall_brush_spacing=clampf(wall_brush_spacing+0.25f,1.0f,24.0f); });
                    TOOL_BTN(0,3,"Spc -",{ wall_brush_spacing=clampf(wall_brush_spacing-0.25f,1.0f,24.0f); });
                    TOOL_BTN(1,3,"Scale-",{ wall_brush_scale=clampf(wall_brush_scale-0.05f,0.25f,20.0f); });
                    TOOL_BTN(2,3,"Scale+",{ wall_brush_scale=clampf(wall_brush_scale+0.05f,0.25f,20.0f); });
                }else if(forest_brush_kind==5){
                    TOOL_BTN(2,2,"Power+",{ terrain_brush_strength=clampf(terrain_brush_strength+0.04f,0.04f,1.0f); });
                    TOOL_BTN(0,3,"Power-",{ terrain_brush_strength=clampf(terrain_brush_strength-0.04f,0.04f,1.0f); });
                    TOOL_BTN(1,3,"Style",{ terrain_brush_style=(terrain_brush_style+1)%TERRAIN_PAINT_COUNT; });
                }else if(forest_brush_kind==6){
                    TOOL_BTN(2,2,"Power+",{ terrain_brush_strength=clampf(terrain_brush_strength+0.04f,0.04f,1.0f); });
                    TOOL_BTN(0,3,"Power-",{ terrain_brush_strength=clampf(terrain_brush_strength-0.04f,0.04f,1.0f); });
                }else if(forest_brush_kind==0){
                    TOOL_BTN(2,2,"Model <",{ if(g_forest.fbx_path_count>0){ g_forest.active_model_idx--; if(g_forest.active_model_idx<-1) g_forest.active_model_idx=g_forest.fbx_path_count-1; } });
                    TOOL_BTN(0,3,"Random",{ g_forest.active_model_idx=-1; });
                    TOOL_BTN(1,3,"Model >",{ if(g_forest.fbx_path_count>0){ g_forest.active_model_idx++; if(g_forest.active_model_idx>=g_forest.fbx_path_count) g_forest.active_model_idx=-1; } });
                    TOOL_BTN(2,3,"Scale+",{ tree_brush_scale=clampf(tree_brush_scale+0.05f,0.25f,20.0f); });
                    TOOL_BTN(0,4,"Scale-",{ tree_brush_scale=clampf(tree_brush_scale-0.05f,0.25f,20.0f); });
                }
                snprintf(ibuf1,sizeof(ibuf1),"Brush: %d   Radius: %.0f",forest_brush_kind+1,(double)forest_brush_radius);
                snprintf(ibuf2,sizeof(ibuf2),"Density: %d",forest_brush_density);
                if(forest_brush_kind==0){
                    const char *tree_name="Random";
                    if(g_forest.active_model_idx>=0&&g_forest.active_model_idx<g_forest.fbx_path_count){
                        const char *full=g_forest.fbx_paths[g_forest.active_model_idx];
                        const char *base=strrchr(full,'/');
                        tree_name=base?base+1:full;
                    }
                    snprintf(ibuf3,sizeof(ibuf3),"Tree Model: %s  Scale: %.2f",tree_name,(double)tree_brush_scale);
                }else if(forest_brush_kind==1){
                    snprintf(ibuf3,sizeof(ibuf3),"%s  %s  Size %.2f",grass_style_name(g_grass.author_style),grass_tint_name(g_grass.author_tint),(double)grass_brush_size);
                }else if(forest_brush_kind==2){
                    snprintf(ibuf3,sizeof(ibuf3),"%s  Size %.2f  Density %d",flower_type_name(g_grass.author_flower_type),(double)flower_brush_size,flower_brush_density);
                }else if(forest_brush_kind==4){
                    snprintf(ibuf3,sizeof(ibuf3),"Spacing %.2f  Scale %.2f", (double)wall_brush_spacing,(double)wall_brush_scale);
                }else if(forest_brush_kind==5){
                    snprintf(ibuf3,sizeof(ibuf3),"%s  Power %.2f",terrain_paint_style_name((TerrainPaintStyle)terrain_brush_style),(double)terrain_brush_strength);
                }else if(forest_brush_kind==6){
                    snprintf(ibuf3,sizeof(ibuf3),"Farm field mask  Power %.2f  Plots %d",(double)terrain_brush_strength,g_farming.count);
                }
            }else if(wind_edit){
                TOOL_BTN(0,0,"Angle <",{ if(wind_sel>=0&&wind_sel<g_wind_zone_count){ WindZone z=g_wind_zones[wind_sel]; z.dir_angle-=0.15f; windzone_update_with_undo(wind_sel,&z); } else g_wind.dir_angle-=0.15f; });
                TOOL_BTN(1,0,"Angle >",{ if(wind_sel>=0&&wind_sel<g_wind_zone_count){ WindZone z=g_wind_zones[wind_sel]; z.dir_angle+=0.15f; windzone_update_with_undo(wind_sel,&z); } else g_wind.dir_angle+=0.15f; });
                TOOL_BTN(2,0,"Save Wind",{ windzone_save(); });
                TOOL_BTN(0,1,"< Radius",{ if(wind_sel>=0&&wind_sel<g_wind_zone_count){ WindZone z=g_wind_zones[wind_sel]; z.radius=clampf(z.radius-4.0f,8.0f,800.0f); windzone_update_with_undo(wind_sel,&z); } });
                TOOL_BTN(1,1,"Radius >",{ if(wind_sel>=0&&wind_sel<g_wind_zone_count){ WindZone z=g_wind_zones[wind_sel]; z.radius=clampf(z.radius+4.0f,8.0f,800.0f); windzone_update_with_undo(wind_sel,&z); } });
                TOOL_BTN(2,1,"Power+",{ if(wind_sel>=0&&wind_sel<g_wind_zone_count){ WindZone z=g_wind_zones[wind_sel]; z.strength=clampf(z.strength+0.1f,0.0f,5.0f); windzone_update_with_undo(wind_sel,&z); } });
                TOOL_BTN(0,2,"Power-",{ if(wind_sel>=0&&wind_sel<g_wind_zone_count){ WindZone z=g_wind_zones[wind_sel]; z.strength=clampf(z.strength-0.1f,0.0f,5.0f); windzone_update_with_undo(wind_sel,&z); } });
                TOOL_BTN(1,2,"Gust -",{ if(wind_sel>=0&&wind_sel<g_wind_zone_count){ WindZone z=g_wind_zones[wind_sel]; z.gust=clampf(z.gust-0.05f,0.0f,1.0f); windzone_update_with_undo(wind_sel,&z); } });
                TOOL_BTN(2,2,"Gust +",{ if(wind_sel>=0&&wind_sel<g_wind_zone_count){ WindZone z=g_wind_zones[wind_sel]; z.gust=clampf(z.gust+0.05f,0.0f,1.0f); windzone_update_with_undo(wind_sel,&z); } });
                TOOL_BTN(0,3,"Clear",{ g_wind_zone_count=0; wind_sel=-1; });
                snprintf(ibuf1,sizeof(ibuf1),"Zones: %d   Selected: %d",g_wind_zone_count,wind_sel>=0?wind_sel+1:0);
                if(wind_sel>=0&&wind_sel<g_wind_zone_count){
                    snprintf(ibuf2,sizeof(ibuf2),"Radius %.0f   Power %.2f",(double)g_wind_zones[wind_sel].radius,(double)g_wind_zones[wind_sel].strength);
                    snprintf(ibuf3,sizeof(ibuf3),"Angle %.0f deg   Gust %.2f",(double)(g_wind_zones[wind_sel].dir_angle*57.2957795f),(double)g_wind_zones[wind_sel].gust);
                }else{
                    snprintf(ibuf2,sizeof(ibuf2),"Click terrain to place or select a wind field.");
                    snprintf(ibuf3,sizeof(ibuf3),"Use Clear to remove all wind fields.");
                }
            }else if(marker_edit){
                TOOL_BTN(0,0,"Type <",{ marker_sel_type=(marker_sel_type+WORLD_MARKER_COUNT-1)%WORLD_MARKER_COUNT; });
                TOOL_BTN(1,0,"Type >",{ marker_sel_type=(marker_sel_type+1)%WORLD_MARKER_COUNT; });
                TOOL_BTN(2,0,"Save Mark",{ world_markers_save(&g_world_markers); });
                TOOL_BTN(0,1,"Rad <",{ if(marker_sel>=0&&marker_sel<g_world_markers.count&&g_world_markers.items[marker_sel].active) editor_command_marker_set_radius(marker_sel,g_world_markers.items[marker_sel].radius-2.0f,NULL); });
                TOOL_BTN(1,1,"Rad >",{ if(marker_sel>=0&&marker_sel<g_world_markers.count&&g_world_markers.items[marker_sel].active) editor_command_marker_set_radius(marker_sel,g_world_markers.items[marker_sel].radius+2.0f,NULL); });
                TOOL_BTN(2,1,"Delete",{ if(marker_sel>=0&&marker_sel<g_world_markers.count){ editor_command_marker_delete(marker_sel,NULL); if(marker_sel>=g_world_markers.count) marker_sel=g_world_markers.count-1; } });
                TOOL_BTN(0,2,"Y -",{ if(marker_sel>=0&&marker_sel<g_world_markers.count&&g_world_markers.items[marker_sel].active) editor_command_marker_set_position(marker_sel,(Vec3){g_world_markers.items[marker_sel].pos.x,g_world_markers.items[marker_sel].pos.y-0.5f,g_world_markers.items[marker_sel].pos.z},NULL); });
                TOOL_BTN(1,2,"Y +",{ if(marker_sel>=0&&marker_sel<g_world_markers.count&&g_world_markers.items[marker_sel].active) editor_command_marker_set_position(marker_sel,(Vec3){g_world_markers.items[marker_sel].pos.x,g_world_markers.items[marker_sel].pos.y+0.5f,g_world_markers.items[marker_sel].pos.z},NULL); });
                snprintf(ibuf1,sizeof(ibuf1),"Marker Type: %s",world_marker_kind_name((WorldMarkerKind)marker_sel_type));
                snprintf(ibuf2,sizeof(ibuf2),"Selected: %d   Total: %d",marker_sel>=0?marker_sel+1:0,g_world_markers.count);
                if(marker_sel>=0&&marker_sel<g_world_markers.count&&g_world_markers.items[marker_sel].active)
                    snprintf(ibuf3,sizeof(ibuf3),"Radius %.1f   Height %.1f",(double)g_world_markers.items[marker_sel].radius,(double)g_world_markers.items[marker_sel].pos.y);
                else
                    snprintf(ibuf3,sizeof(ibuf3),"Click terrain to place a marker or select an existing one.");
            }else if(spawn_edit_mode){
                TOOL_BTN(0,0,"Type <",{ spawn_sel_type=(EnemyType)(((int)spawn_sel_type+ETYPE_COUNT-1)%ETYPE_COUNT); });
                TOOL_BTN(1,0,"Type >",{ spawn_sel_type=(EnemyType)(((int)spawn_sel_type+1)%ETYPE_COUNT); });
                TOOL_BTN(2,0,"Save Spwn",{ spawns_save(&g_spawns); });
                TOOL_BTN(0,1,"Mdl <",{ if(g_enemy_model_count>0){spawn_sel_model=(spawn_sel_model+g_enemy_model_count)%g_enemy_model_count;} });
                TOOL_BTN(1,1,"Mdl >",{ if(g_enemy_model_count>0){spawn_sel_model=(spawn_sel_model+1)%g_enemy_model_count;} });
                snprintf(ibuf1,sizeof(ibuf1),"Type: %s",EDEF[spawn_sel_type].name);
                if(g_enemy_model_count>0)
                    snprintf(ibuf2,sizeof(ibuf2),"Model: %s  [%d/%d]",g_enemy_model_names[spawn_sel_model],spawn_sel_model+1,g_enemy_model_count);
                else
                    snprintf(ibuf2,sizeof(ibuf2),"Model: [EDEF default] (drop FBX in models/enemy/)");
                snprintf(ibuf3,sizeof(ibuf3),"Spawn Points: %d  LMB place  RMB remove",g_spawns.count);
            }else if(mount_edit_mode){
                char class_buf[64];
                char sel_buf[64];
                char yaw_buf[64];
                const MountClassDef *mount_def=mount_class_def((MountClassId)mount_sel_class);
                snprintf(class_buf,sizeof(class_buf),"%.48s",mount_def->name);
                snprintf(sel_buf,sizeof(sel_buf),"%d / %d",mount_sel_idx>=0?mount_sel_idx+1:0,g_mounts.count);
                snprintf(yaw_buf,sizeof(yaw_buf),"%.0f deg",(double)(mount_place_yaw*57.2957795f));
                if(mount_sel_idx>=0&&mount_sel_idx<g_mounts.count&&g_mounts.mounts[mount_sel_idx].active){
                    const MountInstance *m=&g_mounts.mounts[mount_sel_idx];
                    mount_def=mount_class_def(m->class_id);
                    snprintf(class_buf,sizeof(class_buf),"%.48s",mount_def->name);
                    snprintf(yaw_buf,sizeof(yaw_buf),"%.0f deg",(double)(m->yaw*57.2957795f));
                }
                TOOL_ROW(0,"Class",class_buf,"<",">",{
                    mount_sel_class=(int)mount_system_cycle_class(mount_sel_class,-1);
                    if(mount_sel_idx>=0&&mount_sel_idx<g_mounts.count&&g_mounts.mounts[mount_sel_idx].active)
                        g_mounts.mounts[mount_sel_idx].class_id=(MountClassId)mount_sel_class;
                },{
                    mount_sel_class=(int)mount_system_cycle_class(mount_sel_class,1);
                    if(mount_sel_idx>=0&&mount_sel_idx<g_mounts.count&&g_mounts.mounts[mount_sel_idx].active)
                        g_mounts.mounts[mount_sel_idx].class_id=(MountClassId)mount_sel_class;
                });
                TOOL_ROW(1,"Selected",sel_buf,"<",">",{
                    if(g_mounts.count>0) mount_sel_idx=(mount_sel_idx<=0)?(g_mounts.count-1):(mount_sel_idx-1);
                },{
                    if(g_mounts.count>0) mount_sel_idx=(mount_sel_idx+1)%g_mounts.count;
                });
                TOOL_ROW(2,"Yaw",yaw_buf,"-","+",{
                    if(mount_sel_idx>=0&&mount_sel_idx<g_mounts.count&&g_mounts.mounts[mount_sel_idx].active) g_mounts.mounts[mount_sel_idx].yaw-=0.1745f;
                    else mount_place_yaw-=0.1745f;
                },{
                    if(mount_sel_idx>=0&&mount_sel_idx<g_mounts.count&&g_mounts.mounts[mount_sel_idx].active) g_mounts.mounts[mount_sel_idx].yaw+=0.1745f;
                    else mount_place_yaw+=0.1745f;
                });
                TOOL_BTN(0,4,"Save",{ mount_system_save(&g_mounts); save_notify(&save_note,"Mounts saved"); });
                TOOL_BTN(1,4,"Delete",{
                    if(mount_sel_idx>=0&&mount_sel_idx<g_mounts.count){
                        mount_system_delete(&g_mounts,mount_sel_idx);
                        if(mount_sel_idx>=g_mounts.count) mount_sel_idx=g_mounts.count-1;
                    }
                });
                snprintf(ibuf1,sizeof(ibuf1),"Mounts: %d / %d",g_mounts.count,MOUNT_MAX);
                snprintf(ibuf2,sizeof(ibuf2),"Class: %s",class_buf);
                snprintf(ibuf3,sizeof(ibuf3),"LMB place/select  RMB delete  Shift+F12 toggle");
            }else if(npc_edit_mode){
                TOOL_BTN(0,0,"Type <",{ npc_sel_type=(npc_sel_type+NPC_MODEL_MAX-1)%NPC_MODEL_MAX; });
                TOOL_BTN(1,0,"Type >",{ npc_sel_type=(npc_sel_type+1)%NPC_MODEL_MAX; });
                TOOL_BTN(2,0,"Save NPC",{ npc_spawns_save(&g_npc_spawns); });
                TOOL_BTN(0,1,"Yaw -",{ npc_ed_yaw-=0.15f; });
                TOOL_BTN(1,1,"Yaw +",{ npc_ed_yaw+=0.15f; });
                TOOL_BTN(2,1,"Spd +",{
                    int hi=-1; float best=6.0f*6.0f; Vec3 ecp=editor_cam_target;
                    for(int ni=0;ni<g_npc_spawns.count;ni++){NpcSpawnPoint *sp=&g_npc_spawns.points[ni];if(!sp->active)continue;float dx=sp->pos.x-ecp.x;float dz=sp->pos.z-ecp.z;if(dx*dx+dz*dz<best){best=dx*dx+dz*dz;hi=ni;}}
                    if(hi>=0){float spd=g_npc_spawns.points[hi].patrol_speed>0?g_npc_spawns.points[hi].patrol_speed:1.4f;g_npc_spawns.points[hi].patrol_speed=clampf(spd+0.2f,0.2f,6.0f);npc_spawns_save(&g_npc_spawns);}
                });
                TOOL_BTN(0,2,"Spd -",{
                    int hi=-1; float best=6.0f*6.0f; Vec3 ecp=editor_cam_target;
                    for(int ni=0;ni<g_npc_spawns.count;ni++){NpcSpawnPoint *sp=&g_npc_spawns.points[ni];if(!sp->active)continue;float dx=sp->pos.x-ecp.x;float dz=sp->pos.z-ecp.z;if(dx*dx+dz*dz<best){best=dx*dx+dz*dz;hi=ni;}}
                    if(hi>=0){float spd=g_npc_spawns.points[hi].patrol_speed>0?g_npc_spawns.points[hi].patrol_speed:1.4f;g_npc_spawns.points[hi].patrol_speed=clampf(spd-0.2f,0.2f,6.0f);npc_spawns_save(&g_npc_spawns);}
                });
                TOOL_BTN(1,2,"Wait+",{
                    int hi=-1; float best=6.0f*6.0f; Vec3 ecp=editor_cam_target;
                    for(int ni=0;ni<g_npc_spawns.count;ni++){NpcSpawnPoint *sp=&g_npc_spawns.points[ni];if(!sp->active)continue;float dx=sp->pos.x-ecp.x;float dz=sp->pos.z-ecp.z;if(dx*dx+dz*dz<best){best=dx*dx+dz*dz;hi=ni;}}
                    if(hi>=0){float wt=g_npc_spawns.points[hi].patrol_wait>0?g_npc_spawns.points[hi].patrol_wait:2.0f;g_npc_spawns.points[hi].patrol_wait=clampf(wt+0.5f,0.0f,15.0f);npc_spawns_save(&g_npc_spawns);}
                });
                TOOL_BTN(2,2,"Wait-",{
                    int hi=-1; float best=6.0f*6.0f; Vec3 ecp=editor_cam_target;
                    for(int ni=0;ni<g_npc_spawns.count;ni++){NpcSpawnPoint *sp=&g_npc_spawns.points[ni];if(!sp->active)continue;float dx=sp->pos.x-ecp.x;float dz=sp->pos.z-ecp.z;if(dx*dx+dz*dz<best){best=dx*dx+dz*dz;hi=ni;}}
                    if(hi>=0){float wt=g_npc_spawns.points[hi].patrol_wait>0?g_npc_spawns.points[hi].patrol_wait:2.0f;g_npc_spawns.points[hi].patrol_wait=clampf(wt-0.5f,0.0f,15.0f);npc_spawns_save(&g_npc_spawns);}
                });
                snprintf(ibuf1,sizeof(ibuf1),"NPC Type: %s",NPC_TYPE_NAMES[npc_sel_type]);
                snprintf(ibuf2,sizeof(ibuf2),"Placed NPCs: %d",g_npc_spawns.count);
                snprintf(ibuf3,sizeof(ibuf3),"Hover+T=TreeEd  D=Dialogue  P=Waypoints");
            }else if(particle_fx_edit){
                TOOL_BTN(0,0,"World",{ fx_editor_tab=0; });
                TOOL_BTN(1,0,"Weather",{ fx_editor_tab=1; });
                TOOL_BTN(2,0,"Post",{ fx_editor_tab=2; });
                if(fx_editor_tab==0){
                    TOOL_BTN(0,1,"Preset <",{ int next=(int)g_world_fx_editor.kind-1; while(next<0) next+=WORLD_FX_COUNT; g_world_fx_editor.kind=(WorldFxKind)(next%WORLD_FX_COUNT); if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(&ls,&g_world_fx_editor); });
                    TOOL_BTN(1,1,"Preset >",{ g_world_fx_editor.kind=(WorldFxKind)(((int)g_world_fx_editor.kind+1)%WORLD_FX_COUNT); if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(&ls,&g_world_fx_editor); });
                    TOOL_BTN(2,1,"Save FX",{ world_fx_save(&g_world_fx); visual_profile_capture_weather(g_wetness,g_fog_density,g_overcast); visual_profile_save(); });
                    TOOL_BTN(0,2,"Scale -",{ g_world_fx_editor.scale=clampf(g_world_fx_editor.scale-0.05f,0.05f,12.0f); if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(&ls,&g_world_fx_editor); });
                    TOOL_BTN(1,2,"Scale +",{ g_world_fx_editor.scale=clampf(g_world_fx_editor.scale+0.05f,0.05f,12.0f); if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(&ls,&g_world_fx_editor); });
                    TOOL_BTN(2,2,"Light",{ g_world_fx_editor.light_enabled=!g_world_fx_editor.light_enabled; if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(&ls,&g_world_fx_editor); });
                    TOOL_BTN(0,3,"Height-",{ g_world_fx_editor.height=clampf(g_world_fx_editor.height-0.10f,0.0f,20.0f); if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(&ls,&g_world_fx_editor); });
                    TOOL_BTN(1,3,"Height+",{ g_world_fx_editor.height=clampf(g_world_fx_editor.height+0.10f,0.0f,20.0f); if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(&ls,&g_world_fx_editor); });
                    TOOL_BTN(2,3,"Glow +",{ g_world_fx_editor.intensity=clampf(g_world_fx_editor.intensity+0.10f,0.10f,4.0f); if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(&ls,&g_world_fx_editor); });
                    TOOL_BTN(0,4,"Glow -",{ g_world_fx_editor.intensity=clampf(g_world_fx_editor.intensity-0.10f,0.10f,4.0f); if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(&ls,&g_world_fx_editor); });
                    TOOL_BTN(1,4,"L Rad+",{ g_world_fx_editor.light_radius=clampf(g_world_fx_editor.light_radius+0.5f,0.0f,40.0f); if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(&ls,&g_world_fx_editor); });
                    TOOL_BTN(2,4,"L Rad-",{ g_world_fx_editor.light_radius=clampf(g_world_fx_editor.light_radius-0.5f,0.0f,40.0f); if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(&ls,&g_world_fx_editor); });
                    TOOL_BTN(0,5,"R +",{ g_world_fx_editor.color.x=clampf(g_world_fx_editor.color.x+0.05f,0.0f,1.5f); if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(&ls,&g_world_fx_editor); });
                    TOOL_BTN(1,5,"G +",{ g_world_fx_editor.color.y=clampf(g_world_fx_editor.color.y+0.05f,0.0f,1.5f); if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(&ls,&g_world_fx_editor); });
                    TOOL_BTN(2,5,"B +",{ g_world_fx_editor.color.z=clampf(g_world_fx_editor.color.z+0.05f,0.0f,1.5f); if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(&ls,&g_world_fx_editor); });
                    TOOL_BTN(0,6,"R -",{ g_world_fx_editor.color.x=clampf(g_world_fx_editor.color.x-0.05f,0.0f,1.5f); if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(&ls,&g_world_fx_editor); });
                    TOOL_BTN(1,6,"G -",{ g_world_fx_editor.color.y=clampf(g_world_fx_editor.color.y-0.05f,0.0f,1.5f); if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(&ls,&g_world_fx_editor); });
                    TOOL_BTN(2,6,"B -",{ g_world_fx_editor.color.z=clampf(g_world_fx_editor.color.z-0.05f,0.0f,1.5f); if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(&ls,&g_world_fx_editor); });
                    snprintf(ibuf1,sizeof(ibuf1),"FX Preset: %s",world_fx_kind_name(g_world_fx_editor.kind));
                    snprintf(ibuf2,sizeof(ibuf2),"Scale %.2f   Height %.2f",(double)g_world_fx_editor.scale,(double)g_world_fx_editor.height);
                    snprintf(ibuf3,sizeof(ibuf3),"Glow %.2f   Light %s",(double)g_world_fx_editor.intensity,g_world_fx_editor.light_enabled?"On":"Off");
                }else if(fx_editor_tab==1){
                    TOOL_BTN(0,1,"Auto",{ g_weather_editor_override=-1; visual_profile_mark_dirty(); });
                    TOOL_BTN(1,1,"Rain",{ g_weather_editor_override=1; wx.enabled=true; visual_profile_mark_dirty(); });
                    TOOL_BTN(2,1,"Snow",{ g_weather_editor_override=2; wx.enabled=true; visual_profile_mark_dirty(); });
                    TOOL_BTN(0,2,"Off",{ g_weather_editor_override=0; visual_profile_mark_dirty(); });
                    TOOL_BTN(1,2,wx.enabled?"WX On":"WX Off",{ wx.enabled=!wx.enabled; visual_profile_mark_dirty(); });
                    TOOL_BTN(2,2,"Rate +",{ g_weather_editor_spawn_mul=clampf(g_weather_editor_spawn_mul+0.1f,0.1f,4.0f); visual_profile_mark_dirty(); });
                    TOOL_BTN(0,3,"Rate -",{ g_weather_editor_spawn_mul=clampf(g_weather_editor_spawn_mul-0.1f,0.1f,4.0f); visual_profile_mark_dirty(); });
                    TOOL_BTN(1,3,"Wet +",{ g_wetness=clampf(g_wetness+0.05f,0.0f,1.0f); visual_profile_mark_dirty(); });
                    TOOL_BTN(2,3,"Wet -",{ g_wetness=clampf(g_wetness-0.05f,0.0f,1.0f); visual_profile_mark_dirty(); });
                    TOOL_BTN(0,4,"Fog +",{ g_fog_density=clampf(g_fog_density+0.0002f,0.0f,0.05f); visual_profile_mark_dirty(); });
                    TOOL_BTN(1,4,"Fog -",{ g_fog_density=clampf(g_fog_density-0.0002f,0.0f,0.05f); visual_profile_mark_dirty(); });
                    TOOL_BTN(2,4,"Cloud+",{ g_overcast=clampf(g_overcast+0.05f,0.0f,1.0f); visual_profile_mark_dirty(); });
                    TOOL_BTN(0,5,"Cloud-",{ g_overcast=clampf(g_overcast-0.05f,0.0f,1.0f); visual_profile_mark_dirty(); });
                    TOOL_BTN(1,5,"HFog+",{ g_height_fog_density=clampf(g_height_fog_density+0.002f,0.0f,0.2f); visual_profile_mark_dirty(); });
                    TOOL_BTN(2,5,"HFog-",{ g_height_fog_density=clampf(g_height_fog_density-0.002f,0.0f,0.2f); visual_profile_mark_dirty(); });
                    TOOL_BTN(0,6,"HBase+",{ g_height_fog_base=clampf(g_height_fog_base+1.0f,-20.0f,40.0f); visual_profile_mark_dirty(); });
                    TOOL_BTN(1,6,"HBase-",{ g_height_fog_base=clampf(g_height_fog_base-1.0f,-20.0f,40.0f); visual_profile_mark_dirty(); });
                    snprintf(ibuf1,sizeof(ibuf1),"Weather: %s",g_weather_editor_override<0?"Auto":(g_weather_editor_override==1?"Rain":(g_weather_editor_override==2?"Snow":"Off")));
                    snprintf(ibuf2,sizeof(ibuf2),"Fog %.4f   Overcast %.2f",(double)g_fog_density,(double)g_overcast);
                    snprintf(ibuf3,sizeof(ibuf3),"HFog %.3f  HBase %.1f",(double)g_height_fog_density,(double)g_height_fog_base);
                }else{
                    TOOL_BTN(0,1,g_options.bloom?"Bloom":"NoBloom",{ g_options.bloom=!g_options.bloom; visual_profile_mark_dirty(); });
                    TOOL_BTN(1,1,"Exp +",{ g_postfx_exposure_mul=clampf(g_postfx_exposure_mul+0.05f,0.25f,3.0f); visual_profile_mark_dirty(); });
                    TOOL_BTN(2,1,"Exp -",{ g_postfx_exposure_mul=clampf(g_postfx_exposure_mul-0.05f,0.25f,3.0f); visual_profile_mark_dirty(); });
                    TOOL_BTN(0,2,"Bloom+",{ g_postfx_bloom_strength=clampf(g_postfx_bloom_strength+0.05f,0.0f,2.5f); visual_profile_mark_dirty(); });
                    TOOL_BTN(1,2,"Bloom-",{ g_postfx_bloom_strength=clampf(g_postfx_bloom_strength-0.05f,0.0f,2.5f); visual_profile_mark_dirty(); });
                    TOOL_BTN(2,2,"SSAO +",{ g_postfx_ssao_strength=clampf(g_postfx_ssao_strength+0.05f,0.0f,1.5f); visual_profile_mark_dirty(); });
                    TOOL_BTN(0,3,"SSAO -",{ g_postfx_ssao_strength=clampf(g_postfx_ssao_strength-0.05f,0.0f,1.5f); visual_profile_mark_dirty(); });
                    TOOL_BTN(1,3,"DOF  +",{ g_dof_aperture=clampf(g_dof_aperture+0.02f,0.0f,0.5f); visual_profile_mark_dirty(); });
                    TOOL_BTN(2,3,"DOF  -",{ g_dof_aperture=clampf(g_dof_aperture-0.02f,0.0f,0.5f); visual_profile_mark_dirty(); });
                    TOOL_BTN(0,4,"Foc  +",{ g_dof_focus_dist=clampf(g_dof_focus_dist+1.0f,2.0f,200.0f); visual_profile_mark_dirty(); });
                    TOOL_BTN(1,4,"Foc  -",{ g_dof_focus_dist=clampf(g_dof_focus_dist-1.0f,2.0f,200.0f); visual_profile_mark_dirty(); });
                    TOOL_BTN(2,4,g_shader_lab.enabled?"ShaderOn":"ShaderOff",{ if(g_shader_lab.prog) g_shader_lab.enabled=!g_shader_lab.enabled; visual_profile_mark_dirty(); });
                    TOOL_BTN(0,5,"ApplyFx",{ if(g_shader_lab.prog&&g_shader_lab.selected>=0) g_shader_lab.enabled=true; visual_profile_mark_dirty(); });
                    TOOL_BTN(0,6,"TOD +1h",{ total_time+=DAY_DURATION/24.0f; });
                    TOOL_BTN(1,6,"TOD -1h",{ total_time-=DAY_DURATION/24.0f; if(total_time<0.0f) total_time=0.0f; });
                    TOOL_BTN(2,6,"TOD Noon",{ total_time=floorf(total_time/DAY_DURATION)*DAY_DURATION+DAY_DURATION*0.5f; });
                    TOOL_BTN(0,7,g_fxaa_enabled?"FXAA On":"FXAA Off",{ g_fxaa_enabled=!g_fxaa_enabled; visual_profile_mark_dirty(); });
                    {float tod_frac=fmodf(total_time,DAY_DURATION)/DAY_DURATION;
                     int tod_h=(int)(tod_frac*24.0f); float tod_m=fmodf(tod_frac*24.0f,1.0f)*60.0f;
                     snprintf(ibuf1,sizeof(ibuf1),"Exposure %.2f   Bloom %.2f",(double)g_postfx_exposure_mul,(double)g_postfx_bloom_strength);
                     snprintf(ibuf2,sizeof(ibuf2),"SSAO %.2f   DOF %.2f Foc %.0f",(double)g_postfx_ssao_strength,(double)g_dof_aperture,(double)g_dof_focus_dist);
                     snprintf(ibuf3,sizeof(ibuf3),"Time %02d:%02.0f  Shader: %s",tod_h,tod_m,shader_lab_selected_name(&g_shader_lab));}
                }
                TOOL_BTN(0,8,"Natural",{ visual_profile_apply_look(VISUAL_LOOK_NATURAL,&wx,&g_wetness,&g_fog_density,&g_overcast,&total_time); });
                TOOL_BTN(1,8,"Cine",{ visual_profile_apply_look(VISUAL_LOOK_CINEMATIC,&wx,&g_wetness,&g_fog_density,&g_overcast,&total_time); });
                TOOL_BTN(2,8,"Storm",{ visual_profile_apply_look(VISUAL_LOOK_STORM,&wx,&g_wetness,&g_fog_density,&g_overcast,&total_time); });
                TOOL_BTN(0,9,"LowLt",{ visual_profile_apply_look(VISUAL_LOOK_LOW_LIGHT,&wx,&g_wetness,&g_fog_density,&g_overcast,&total_time); });
                TOOL_BTN(1,9,"Reset",{ visual_profile_apply_look(VISUAL_LOOK_RESET,&wx,&g_wetness,&g_fog_density,&g_overcast,&total_time); });
                TOOL_BTN(2,9,"SaveLook",{ visual_profile_capture_weather(g_wetness,g_fog_density,g_overcast); visual_profile_save(); });
            }else if(plight_edit){
                TOOL_ROW(0,"Radius",plight_place_radius>=0.0f?"{var}":"--","< Radius","> Radius",
                    { plight_place_radius=fmaxf(2.0f,plight_place_radius-1.0f); },
                    { plight_place_radius=fminf(60.0f,plight_place_radius+1.0f); });
                TOOL_ROW(1,"Intens",plight_place_intensity>=0.0f?"{var}":"--","< Int","> Int",
                    { plight_place_intensity=fmaxf(0.05f,plight_place_intensity-0.05f); },
                    { plight_place_intensity=fminf(4.0f,plight_place_intensity+0.05f); });
                TOOL_ROW(2,"YOff",plight_place_y_offset>=0.0f?"{var}":"--","< Y","> Y",
                    { plight_place_y_offset=fmaxf(0.0f,plight_place_y_offset-0.1f); },
                    { plight_place_y_offset+=0.1f; });
                TOOL_BTN(0,3,"R -",{ plight_place_r=clampf(plight_place_r-0.05f,0.0f,1.0f); });
                TOOL_BTN(1,3,"R +",{ plight_place_r=clampf(plight_place_r+0.05f,0.0f,1.0f); });
                TOOL_BTN(2,3,"Flicker",{ plight_place_flicker=!plight_place_flicker; });
                TOOL_BTN(0,4,"G -",{ plight_place_g=clampf(plight_place_g-0.05f,0.0f,1.0f); });
                TOOL_BTN(1,4,"G +",{ plight_place_g=clampf(plight_place_g+0.05f,0.0f,1.0f); });
                TOOL_BTN(2,4,"Del Sel",{ editor_plight_delete_index(&editor,&ls,plight_sel); });
                TOOL_BTN(0,5,"B -",{ plight_place_b=clampf(plight_place_b-0.05f,0.0f,1.0f); });
                TOOL_BTN(1,5,"B +",{ plight_place_b=clampf(plight_place_b+0.05f,0.0f,1.0f); });
                TOOL_BTN(2,5,"Save",{ placed_lights_save(&g_placed_lights); save_notify(&save_note,"Lights saved"); });
                snprintf(ibuf1,sizeof(ibuf1),"Color R%.2f G%.2f B%.2f",(double)plight_place_r,(double)plight_place_g,(double)plight_place_b);
                snprintf(ibuf2,sizeof(ibuf2),"Radius:%.1f  Int:%.2f  YOff:%.2f  Flicker:%s",
                         (double)plight_place_radius,(double)plight_place_intensity,(double)plight_place_y_offset,plight_place_flicker?"ON":"OFF");
                snprintf(ibuf3,sizeof(ibuf3),"Sel:%d  LMB=Place  RMB=Del nearest",plight_sel);
            }else if(decal_edit){
                static const char *decal_names[]={"Scorch","Moss","Rune","Crack","Blood","Mud"};
                const char *dn=(decal_place_type>=0&&decal_place_type<DECAL_COUNT)?decal_names[decal_place_type]:"?";
                TOOL_BTN(0,0,"Type <",{ decal_place_type=(decal_place_type+DECAL_COUNT-1)%DECAL_COUNT; });
                TOOL_BTN(1,0,"Type >",{ decal_place_type=(decal_place_type+1)%DECAL_COUNT; });
                TOOL_BTN(2,0,"Del Sel",{ editor_decal_delete_index(&editor,decal_sel); });
                TOOL_ROW(1,"Size",decal_place_size>=0.0f?"{var}":"--","< Size","> Size",
                    { decal_place_size=fmaxf(0.5f,decal_place_size-0.25f); },
                    { decal_place_size=fminf(20.0f,decal_place_size+0.25f); });
                TOOL_ROW(2,"Alpha",decal_place_alpha>=0.0f?"{var}":"--","< Alpha","> Alpha",
                    { decal_place_alpha=clampf(decal_place_alpha-0.05f,0.05f,1.0f); },
                    { decal_place_alpha=clampf(decal_place_alpha+0.05f,0.05f,1.0f); });
                TOOL_ROW(3,"Yaw",decal_place_yaw>=0.0f?"{var}":"--","< Yaw","> Yaw",
                    { decal_place_yaw-=0.1745f; },
                    { decal_place_yaw+=0.1745f; });
                TOOL_BTN(0,4,"Save",{ decals_save(&g_decals); save_notify(&save_note,"Decals saved"); });
                TOOL_BTN(1,4,"Rebuild",{ decals_rebuild_vbo(&g_decals); });
                snprintf(ibuf1,sizeof(ibuf1),"Type: %s  Count: %d",dn,g_decals.count);
                snprintf(ibuf2,sizeof(ibuf2),"Size:%.2f  Alpha:%.2f  Yaw:%.0f deg",(double)decal_place_size,(double)decal_place_alpha,(double)(decal_place_yaw*57.2957795f));
                snprintf(ibuf3,sizeof(ibuf3),"Sel:%d  LMB=Place  RMB=Del nearest",decal_sel);
            }else if(szone_edit){
                extern const char *g_szone_sound_names[];
                const char *sn=(szone_sound_idx>=0&&szone_sound_idx<SZONE_SOUND_COUNT)?g_szone_sound_names[szone_sound_idx]:"?";
                TOOL_BTN(0,0,"Snd <",{ szone_sound_idx=(szone_sound_idx+SZONE_SOUND_COUNT-1)%SZONE_SOUND_COUNT; });
                TOOL_BTN(1,0,"Snd >",{ szone_sound_idx=(szone_sound_idx+1)%SZONE_SOUND_COUNT; });
                TOOL_BTN(2,0,"Del Sel",{ editor_szone_delete_index(&editor,szone_sel); });
                TOOL_ROW(1,"Radius",szone_place_radius>=0.0f?"{var}":"--","< Rad","> Rad",
                    { szone_place_radius=fmaxf(4.0f,szone_place_radius-2.0f); },
                    { szone_place_radius=fminf(120.0f,szone_place_radius+2.0f); });
                TOOL_ROW(2,"Volume",szone_place_vol>=0.0f?"{var}":"--","< Vol","> Vol",
                    { szone_place_vol=clampf(szone_place_vol-5.0f,5.0f,127.0f); },
                    { szone_place_vol=clampf(szone_place_vol+5.0f,5.0f,127.0f); });
                TOOL_BTN(0,3,"Save",{ sound_zones_save(&g_sound_zones); save_notify(&save_note,"Sound zones saved"); });
                TOOL_BTN(1,3,"Preview",{ if(szone_sound_idx>=0&&szone_sound_idx<SZONE_SOUND_COUNT) audio_play(g_szone_sound_ids[szone_sound_idx],(int)szone_place_vol); });
                snprintf(ibuf1,sizeof(ibuf1),"Sound: %s  Count: %d",sn,g_sound_zones.count);
                snprintf(ibuf2,sizeof(ibuf2),"Radius:%.0f  Volume:%.0f",(double)szone_place_radius,(double)szone_place_vol);
                snprintf(ibuf3,sizeof(ibuf3),"Sel:%d  LMB=Place  RMB=Del nearest",szone_sel);
            }else if(shader_edit){
                TOOL_BTN(0,0,"Shader <",{ if(g_shader_lab.count>0) g_shader_lab.selected=(g_shader_lab.selected+g_shader_lab.count-1)%g_shader_lab.count; visual_profile_mark_dirty(); });
                TOOL_BTN(1,0,"Shader >",{ if(g_shader_lab.count>0) g_shader_lab.selected=(g_shader_lab.selected+1)%g_shader_lab.count; visual_profile_mark_dirty(); });
                TOOL_BTN(2,0,g_shader_lab.enabled?"Disable":"Enable",{ if(g_shader_lab.prog) g_shader_lab.enabled=!g_shader_lab.enabled; visual_profile_mark_dirty(); });
                TOOL_BTN(0,1,"Reload",{ shader_lab_reload_selected(&g_shader_lab); visual_profile_mark_dirty(); });
                TOOL_BTN(1,1,"Rescan",{ shader_lab_refresh(&g_shader_lab); visual_profile_mark_dirty(); });
                TOOL_BTN(2,1,"Apply",{ shader_lab_reload_selected(&g_shader_lab); visual_profile_mark_dirty(); });
                snprintf(ibuf1,sizeof(ibuf1),"File %d / %d",g_shader_lab.count>0?g_shader_lab.selected+1:0,g_shader_lab.count);
                snprintf(ibuf2,sizeof(ibuf2),"%s",shader_lab_selected_name(&g_shader_lab));
                snprintf(ibuf3,sizeof(ibuf3),"%s",g_shader_lab.status);
            }
            {
                float iy=path_edit?(ty+334.0f):(ty+214.0f);
                editor_shell_panel(&hud,tx+10.0f,iy,tw-20.0f,104.0f);
                hud_str(&hud,tx+22.0f,iy+8.0f,0.76f,"INSPECTOR",0.58f,0.68f,0.42f,0.94f);
                hud_rect(&hud,tx+22.0f,iy+22.0f,tw-44.0f,1.0f,0.25f,0.34f,0.16f,0.30f);
                if(ibuf1[0]) hud_str(&hud,tx+22.0f,iy+32.0f,0.84f,ibuf1,0.94f,0.82f,0.56f,0.98f);
                if(ibuf2[0]) hud_str(&hud,tx+22.0f,iy+46.0f,0.78f,ibuf2,0.76f,0.88f,0.96f,0.94f);
                if(ibuf3[0]) hud_str(&hud,tx+22.0f,iy+60.0f,0.76f,ibuf3,0.62f,0.76f,0.64f,0.92f);
            }
            #undef TOOL_ROW
            #undef TOOL_BTN
        }

        /* ── Water Zone Editor (F2) ───────────────────────────── */
        if(wzone_edit){
            /* ── WATERFALL SUB-MODE ── */
            if(wf_edit_mode){
                int wmx2,wmy2;
                Uint32 wmb2=SDL_GetMouseState(&wmx2,&wmy2);
                bool lmb2=((wmb2&SDL_BUTTON(1))!=0)&&!editor_gui_mouse_blocked;
                bool rmb2=((wmb2&SDL_BUTTON(3))!=0)&&!editor_gui_mouse_blocked;
                Vec3 wfhit; bool wfgot=false;
                wfgot=river_screen_to_world(wmx2,wmy2,win_w,win_h,proj,view,&wfhit);
                if(wfgot) wfhit.y=terrain_h(wfhit.x,wfhit.z);

                if(lmb2&&!wf_lmb_prev&&wfgot){
                    float best2=12.0f; int besti2=-1;
                    for(int wi=0;wi<g_wfalls.count;wi++){
                        Waterfall *_w=&g_wfalls.wf[wi];
                        if(!_w->active)continue;
                        float dd=sqrtf((_w->pos.x-wfhit.x)*(_w->pos.x-wfhit.x)+
                                       (_w->pos.z-wfhit.z)*(_w->pos.z-wfhit.z));
                        if(dd<best2){best2=dd;besti2=wi;}
                    }
                    if(besti2>=0){ wf_sel=besti2; }
                    else if(g_wfalls.count<MAX_WATERFALLS){
                        int ni=wf_add(wfhit,wf_place_yaw,wf_place_width,wf_place_height);
                        if(ni>=0){
                            wf_sel=ni;
                            Vec3 base={wfhit.x,wfhit.y-wf_place_height+0.6f,wfhit.z};
                            g_wfalls.wf[ni].mist_em=ps_add_emitter(&ps,base,
                                6,0.88f,0.93f,1.0f,0.14f,0.9f);
                            wf_save();
                            printf("[WFalls] Placed waterfall %d at (%.0f,%.0f,%.0f) W=%.1f H=%.1f\n", ni,
                                   (double)wfhit.x,(double)wfhit.y,(double)wfhit.z,
                                   (double)wf_place_width,(double)wf_place_height);
                        }
                    }
                }
                if(rmb2&&wfgot&&g_wfalls.count>0){
                    float best3=20.0f; int bi3=-1;
                    for(int wi=0;wi<g_wfalls.count;wi++){
                        Waterfall *_w=&g_wfalls.wf[wi];
                        if(!_w->active)continue;
                        float dd=sqrtf((_w->pos.x-wfhit.x)*(_w->pos.x-wfhit.x)+
                                       (_w->pos.z-wfhit.z)*(_w->pos.z-wfhit.z));
                        if(dd<best3){best3=dd;bi3=wi;}
                    }
                    if(bi3>=0){
                        int em=g_wfalls.wf[bi3].mist_em;
                        if(em>=0&&em<ps.ecount) ps.emitters[em].active=false;
                        for(int wi=bi3;wi<g_wfalls.count-1;wi++)
                            g_wfalls.wf[wi]=g_wfalls.wf[wi+1];
                        g_wfalls.count--;
                        if(wf_sel>=g_wfalls.count)wf_sel=g_wfalls.count-1;
                        wf_save();
                        printf("[WFalls] Deleted -- %d remaining\n",g_wfalls.count);
                    }
                }
                wf_lmb_prev=lmb2;

                if(wfgot&&wf_sel<0){
                    float cy2=cosf(wf_place_yaw),sy2=sinf(wf_place_yaw);
                    Vec3 right2={cy2,0,-sy2};
                    float hw2=wf_place_width*0.5f;
                    Vec3 TL2={wfhit.x-right2.x*hw2,wfhit.y,             wfhit.z-right2.z*hw2};
                    Vec3 TR2={wfhit.x+right2.x*hw2,wfhit.y,             wfhit.z+right2.z*hw2};
                    Vec3 BL2={wfhit.x-right2.x*hw2,wfhit.y-wf_place_height,wfhit.z-right2.z*hw2};
                    Vec3 BR2={wfhit.x+right2.x*hw2,wfhit.y-wf_place_height,wfhit.z+right2.z*hw2};
                    lr_push(TL2,TR2,0.4f,1.0f,0.6f);lr_push(BL2,BR2,0.4f,1.0f,0.6f);
                    lr_push(TL2,BL2,0.4f,1.0f,0.6f);lr_push(TR2,BR2,0.4f,1.0f,0.6f);
                    lr_flush(&lr,proj,view);
                }
                wf_editor_draw(&g_wfalls,&lr,proj,view,wf_sel);
                if(!editor_ui_backend_is_imgui(&g_editor_ui))
                    wf_editor_hud(&hud,&g_wfalls,wf_sel,win_w,win_h);

            } else {
                int wmx,wmy;
                Uint32 wmb=SDL_GetMouseState(&wmx,&wmy);
                bool lmb_now=((wmb&SDL_BUTTON(1))!=0)&&!editor_gui_mouse_blocked;
                bool rmb_now=((wmb&SDL_BUTTON(3))!=0)&&!editor_gui_mouse_blocked;

                if(lmb_now&&!wzone_lmb_prev){
                    Vec3 hit;
                    if(river_screen_to_world(wmx,wmy,win_w,win_h,proj,view,&hit)){
                        float best=20.0f; int besti=-1;
                        for(int zi=0;zi<g_wzone_count;zi++){
                            float d=sqrtf((g_wzones[zi].cx-hit.x)*(g_wzones[zi].cx-hit.x)+
                                          (g_wzones[zi].cz-hit.z)*(g_wzones[zi].cz-hit.z));
                            if(d<best){best=d;besti=zi;}
                        }
                        if(besti>=0){ wzone_sel=besti; }
                        else if(g_wzone_count<MAX_WATER_ZONES){
                            int new_idx=-1;
                            wzone_place_with_undo(hit.x,hit.z,80.0f,1,terrain_h(hit.x,hit.z),&terrain,&new_idx);
                            wzone_sel=new_idx;
                            printf("[WZoneEd] Zone %d placed at (%.0f, %.0f) — %s\n",
                                   wzone_sel,(double)hit.x,(double)hit.z,
                                   WATER_PRESETS[g_wzones[wzone_sel].preset_idx].name);
                        }
                    }
                }
                if(rmb_now&&g_wzone_count>0){
                    Vec3 hit;
                    if(river_screen_to_world(wmx,wmy,win_w,win_h,proj,view,&hit)){
                        float best=1e9f; int besti=-1;
                        for(int zi=0;zi<g_wzone_count;zi++){
                            float d=sqrtf((g_wzones[zi].cx-hit.x)*(g_wzones[zi].cx-hit.x)+
                                          (g_wzones[zi].cz-hit.z)*(g_wzones[zi].cz-hit.z));
                            if(d<best){best=d;besti=zi;}
                        }
                        if(besti>=0){
                            wzone_delete_with_undo(besti,&terrain);
                            if(wzone_sel>=g_wzone_count)wzone_sel=g_wzone_count-1;
                            printf("[WZoneEd] Removed zone — %d remaining\n",g_wzone_count);
                        }
                    }
                }
                wzone_lmb_prev=lmb_now;

                wzone_editor_draw(&lr,proj,view,wzone_sel);
                if(!editor_ui_backend_is_imgui(&g_editor_ui))
                    wzone_editor_hud(&hud,win_w,win_h,wzone_sel);
            }
        }

        if(wind_edit){
            int wmx,wmy;
            Uint32 wmb=SDL_GetMouseState(&wmx,&wmy);
            bool lmb_now=((wmb&SDL_BUTTON(1))!=0)&&!editor_gui_mouse_blocked;
            bool rmb_now=((wmb&SDL_BUTTON(3))!=0)&&!editor_gui_mouse_blocked;
            Vec3 hit;
            bool got_hit=river_screen_to_world(wmx,wmy,win_w,win_h,proj,view,&hit);

            if(lmb_now&&!wind_lmb_prev){
                if(got_hit){
                    float best=20.0f; int besti=-1;
                    for(int zi=0;zi<g_wind_zone_count;zi++){
                        WindZone *z=&g_wind_zones[zi];
                        if(!z->active) continue;
                        float dx=z->cx-hit.x, dz=z->cz-hit.z;
                        float d=sqrtf(dx*dx+dz*dz);
                        if(d<best){ best=d; besti=zi; }
                    }
                    if(besti>=0){
                        wind_sel=besti;
                    }else if(g_wind_zone_count<MAX_WIND_ZONES){
                        WindZone z;
                        memset(&z,0,sizeof(z));
                        z.cx=hit.x; z.cz=hit.z;
                        z.radius=70.0f;
                        z.dir_angle=g_wind.dir_angle;
                        z.strength=g_wind.total;
                        z.gust=g_wind.gust;
                        z.active=true;
                        windzone_place_with_undo(&z,&wind_sel);
                        printf("[WindEd] Zone %d placed at (%.0f, %.0f)\n",
                               wind_sel,(double)hit.x,(double)hit.z);
                    }
                }
            }
            if(rmb_now&&g_wind_zone_count>0){
                if(got_hit){
                    float best=1e9f; int besti=-1;
                    for(int zi=0;zi<g_wind_zone_count;zi++){
                        WindZone *z=&g_wind_zones[zi];
                        if(!z->active) continue;
                        float dx=z->cx-hit.x, dz=z->cz-hit.z;
                        float d=sqrtf(dx*dx+dz*dz);
                        if(d<best){ best=d; besti=zi; }
                    }
                    if(besti>=0){
                        windzone_delete_with_undo(besti);
                        if(wind_sel>=g_wind_zone_count) wind_sel=g_wind_zone_count-1;
                        printf("[WindEd] Removed zone — %d remaining\n",g_wind_zone_count);
                    }
                }
            }
            wind_lmb_prev=lmb_now;

            if(got_hit){
                float preview_radius=70.0f;
                float preview_angle=g_wind.dir_angle;
                float preview_strength=g_wind.total;
                if(wind_sel>=0&&wind_sel<g_wind_zone_count&&g_wind_zones[wind_sel].active){
                    preview_radius=g_wind_zones[wind_sel].radius;
                    preview_angle=g_wind_zones[wind_sel].dir_angle;
                    preview_strength=g_wind_zones[wind_sel].strength;
                }
                {
                    float y=terrain_h(hit.x,hit.z)+0.4f;
                    int segs=28;
                    for(int s=0;s<segs;s++){
                        float a0=(float)s/(float)segs*2.0f*(float)M_PI;
                        float a1=(float)(s+1)/(float)segs*2.0f*(float)M_PI;
                        lr_push((Vec3){hit.x+cosf(a0)*preview_radius,y,hit.z+sinf(a0)*preview_radius},
                                (Vec3){hit.x+cosf(a1)*preview_radius,y,hit.z+sinf(a1)*preview_radius},
                                0.35f,0.95f,1.0f);
                    }
                    {
                        float len=12.0f+preview_strength*22.0f;
                        float ax=cosf(preview_angle), az=sinf(preview_angle);
                        Vec3 p0={hit.x,y+0.5f,hit.z};
                        Vec3 p1={hit.x+ax*len,y+0.5f,hit.z+az*len};
                        Vec3 side={-az,0.0f,ax};
                        Vec3 h0={p1.x-ax*3.2f+side.x*2.0f,p1.y,p1.z-az*3.2f+side.z*2.0f};
                        Vec3 h1={p1.x-ax*3.2f-side.x*2.0f,p1.y,p1.z-az*3.2f-side.z*2.0f};
                        lr_push(p0,p1,0.35f,0.95f,1.0f);
                        lr_push(h0,p1,0.35f,0.95f,1.0f);
                        lr_push(h1,p1,0.35f,0.95f,1.0f);
                    }
                }
            }

            wind_editor_draw(&lr,proj,view,wind_sel);
            if(!editor_ui_backend_is_imgui(&g_editor_ui))
                wind_editor_hud(&hud,win_w,win_h,wind_sel);
        }

        if(path_edit){
            int pmx2,pmy2;
            Uint32 pmb2=SDL_GetMouseState(&pmx2,&pmy2);
            bool lmb_now=((pmb2&SDL_BUTTON(1))!=0)&&!editor_gui_mouse_blocked;
            bool rmb_now=((pmb2&SDL_BUTTON(3))!=0)&&!editor_gui_mouse_blocked;
            if(lmb_now&&!path_lmb_prev){
                Vec3 hit;
                if(river_screen_to_world(pmx2,pmy2,win_w,win_h,proj,view,&hit)
                   &&g_cpath.count<CPATH_MAX_PTS){
                    g_cpath.pts[g_cpath.count*2  ]=hit.x;
                    g_cpath.pts[g_cpath.count*2+1]=hit.z;
                    g_cpath.count++;
                    if(g_cpath.count>=2) path_rebuild_mesh();
                    printf("[PathEd] Point %d: (%.0f, %.0f)\n",g_cpath.count,(double)hit.x,(double)hit.z);
                }
            }
            if(rmb_now&&!path_rmb_prev&&g_cpath.count>0){
                g_cpath.count--;
                if(g_cpath.count>=2) path_rebuild_mesh();
                else g_pmesh.valid=false;
                printf("[PathEd] Removed last point — %d remaining\n",g_cpath.count);
            }
            path_lmb_prev=lmb_now;
            path_rmb_prev=rmb_now;

            path_editor_draw(&lr,proj,view);
            if(!editor_ui_backend_is_imgui(&g_editor_ui))
                path_editor_hud(&hud,win_w,win_h);
        }

        if(river_edit){
            int rmx,rmy;
            Uint32 rmb=SDL_GetMouseState(&rmx,&rmy);
            bool lmb_now=((rmb&SDL_BUTTON(1))!=0)&&!editor_gui_mouse_blocked;
            bool rmb_now=((rmb&SDL_BUTTON(3))!=0)&&!editor_gui_mouse_blocked;
            if(lmb_now&&!river_lmb_prev){
                Vec3 hit;
                if(river_screen_to_world(rmx,rmy,win_w,win_h,proj,view,&hit)){
                    int near_idx=criver_find_point_near(hit.x,hit.z,fmaxf(g_criver.width*0.45f,12.0f));
                    if(near_idx>=0){
                        river_sel_point=near_idx;
                        printf("[RiverEd] Selected point %d\n",river_sel_point+1);
                    }else if(g_criver.count<CRIVER_MAX_PTS){
                        int idx=g_criver.count;
                        g_criver.pts[idx*2  ]=hit.x;
                        g_criver.pts[idx*2+1]=hit.z;
                        if(idx>0){
                            g_criver.width_scale[idx]=g_criver.width_scale[idx-1];
                            g_criver.depth_scale[idx]=g_criver.depth_scale[idx-1];
                            g_criver.flow_speed[idx]=g_criver.flow_speed[idx-1];
                            g_criver.foam[idx]=g_criver.foam[idx-1];
                        }else{
                            criver_point_set_defaults(idx);
                        }
                        g_criver.count++;
                        river_sel_point=idx;
                        g_criver.dirty=true;
                        terrain_rebuild_all(&terrain);
                        terrain_rebake_hmap(&terrain);
                        printf("[RiverEd] Point %d: (%.0f, %.0f)\n",
                               g_criver.count,(double)hit.x,(double)hit.z);
                    }
                }
            }
            if(rmb_now&&!river_rmb_prev&&g_criver.count>0){
                int remove_idx=(river_sel_point>=0&&river_sel_point<g_criver.count)?river_sel_point:(g_criver.count-1);
                criver_delete_point(remove_idx);
                if(g_criver.count<=0) river_sel_point=-1;
                else if(river_sel_point>=g_criver.count) river_sel_point=g_criver.count-1;
                g_criver.dirty=true;
                terrain_rebuild_all(&terrain);
                terrain_rebake_hmap(&terrain);
                printf("[RiverEd] Removed point %d — %d remaining\n",remove_idx+1,g_criver.count);
            }
            river_lmb_prev=lmb_now;
            river_rmb_prev=rmb_now;

            river_editor_draw(&lr,proj,view);
            if(!editor_ui_backend_is_imgui(&g_editor_ui)){
                river_editor_hud(&hud,win_w,win_h);
                hud_flush(&hud);
            }
        }

        if(marker_edit){
            int mmx,mmy;
            Uint32 mmb=SDL_GetMouseState(&mmx,&mmy);
            bool lmb_now=((mmb&SDL_BUTTON(1))!=0)&&!editor_gui_mouse_blocked;
            bool rmb_now=((mmb&SDL_BUTTON(3))!=0)&&!editor_gui_mouse_blocked;
            Vec3 hit; bool got_hit=false;
            int hover_marker=-1;
            got_hit=river_screen_to_world(mmx,mmy,win_w,win_h,proj,view,&hit);
            if(got_hit) hit.y=terrain_h(hit.x,hit.z)+0.05f;
            if(got_hit) hover_marker=world_markers_pick(&g_world_markers,hit,8.0f);

            if(lmb_now&&!marker_lmb_prev&&got_hit){
                int picked=hover_marker;
                if(picked>=0){
                    marker_sel=picked;
                    marker_sel_type=(int)g_world_markers.items[picked].kind;
                    editor_selection_set(EDITOR_SELECTION_MARKER,picked);
                }else if(g_world_markers.count<MAX_WORLD_MARKERS){
                    float radius=((WorldMarkerKind)marker_sel_type==WORLD_MARKER_AUDIO)?22.0f:(((WorldMarkerKind)marker_sel_type==WORLD_MARKER_TRIGGER)?18.0f:12.0f);
                    int placed_idx=-1;
                    if(editor_command_marker_create((WorldMarkerKind)marker_sel_type,hit,radius,&placed_idx,NULL)){
                        marker_sel=placed_idx;
                        marker_sel_type=(int)g_world_markers.items[placed_idx].kind;
                        editor_selection_set(EDITOR_SELECTION_MARKER,marker_sel);
                        printf("[MarkerEd] Placed %s at (%.1f, %.1f, %.1f)\n",
                               g_world_markers.items[placed_idx].name,
                               (double)g_world_markers.items[placed_idx].pos.x,
                               (double)g_world_markers.items[placed_idx].pos.y,
                               (double)g_world_markers.items[placed_idx].pos.z);
                    }
                }
            }
            if(rmb_now&&!marker_rmb_prev&&got_hit){
                int picked=world_markers_pick(&g_world_markers,hit,12.0f);
                if(picked>=0){
                    editor_command_marker_delete(picked,NULL);
                    if(marker_sel>=g_world_markers.count) marker_sel=g_world_markers.count-1;
                    printf("[MarkerEd] Deleted marker %d\n",picked);
                }
            }

            if(g_editor_selection.kind==EDITOR_SELECTION_MARKER&&
               g_editor_selection.index>=0&&g_editor_selection.index<g_world_markers.count&&
               g_world_markers.items[g_editor_selection.index].active){
                WorldMarker *mk=&g_world_markers.items[g_editor_selection.index];
                Vec3 marker_origin={mk->pos.x,mk->pos.y+fmaxf(0.8f,mk->radius*0.12f),mk->pos.z};
                float marker_len=clampf(vec3_dist(marker_origin,cam.pos)*0.10f,3.0f,18.0f);
                int marker_hot_axis=(prop_gizmo_active_axis>0)?prop_gizmo_active_axis:
                    editor_gizmo_pick_axis(marker_origin,marker_len,false,0.0f,mmx,mmy,win_w,win_h,proj,view);
                bool selected_marker_hit=(hover_marker==g_editor_selection.index);
                static bool marker_drag_active=false;
                static bool marker_drag_dirty=false;
                static Vec3 marker_drag_start_pos;
                static float marker_drag_start_radius;
                if(prop_mouse_mode>0){
                    editor_gizmo_draw(marker_origin,marker_len,false,0.0f,marker_hot_axis,prop_gizmo_active_axis);
                    lr_flush(&lr,proj,view);
                }
                if(lmb_now&&!marker_lmb_prev&&
                   ((prop_mouse_mode==1&&(selected_marker_hit||marker_hot_axis>0))||
                    (prop_mouse_mode==3&&(selected_marker_hit||marker_hot_axis>0)))){
                    marker_drag_active=true;
                    marker_drag_dirty=false;
                    prop_gizmo_active_axis=(marker_hot_axis>0)?marker_hot_axis:4;
                    prop_mouse_last_x=mmx;
                    prop_mouse_last_y=mmy;
                    prop_gizmo_drag_accum=0.0f;
                    marker_drag_start_pos=mk->pos;
                    marker_drag_start_radius=mk->radius;
                }else if(marker_drag_active&&prop_gizmo_active_axis>0&&lmb_now){
                    if(prop_mouse_mode==1){
                        Vec3 go,gx,gy,gz,ga;
                        float osx,osy,asx,asy,axis_dx,axis_dy,axis_len,delta_px;
                        editor_gizmo_axes(marker_origin,marker_len,false,0.0f,&gx,&gy,&gz);
                        go=marker_origin;
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
                                delta_px=((float)(mmx-prop_mouse_last_x))*axis_dx+((float)(mmy-prop_mouse_last_y))*axis_dy;
                                prop_gizmo_drag_accum+=delta_px;
                            }
                        }
                        {
                            float move_delta=prop_gizmo_drag_accum*cam.dist*0.012f;
                            if(selection_gizmo_snap) move_delta=editor_gizmo_snap_value(move_delta,0.5f);
                            mk->pos=marker_drag_start_pos;
                            if(prop_gizmo_active_axis==1) mk->pos.x+=move_delta;
                            else if(prop_gizmo_active_axis==2) mk->pos.y+=move_delta;
                            else mk->pos.z+=move_delta;
                        }
                        marker_drag_dirty=true;
                    }else if(prop_mouse_mode==3){
                        float delta_px=(float)(mmx-prop_mouse_last_x);
                        prop_gizmo_drag_accum+=delta_px;
                        mk->radius=clampf(marker_drag_start_radius+prop_gizmo_drag_accum*cam.dist*0.006f,2.0f,200.0f);
                        if(selection_gizmo_snap) mk->radius=clampf(editor_gizmo_snap_value(mk->radius,1.0f),2.0f,200.0f);
                        marker_drag_dirty=true;
                    }
                    prop_mouse_last_x=mmx;
                    prop_mouse_last_y=mmy;
                }else if(marker_drag_active&&!lmb_now){
                    Vec3 final_pos=mk->pos;
                    float final_radius=mk->radius;
                    mk->pos=marker_drag_start_pos;
                    mk->radius=marker_drag_start_radius;
                    if(marker_drag_dirty){
                        if(fabsf(final_pos.x-marker_drag_start_pos.x)>0.0001f||
                           fabsf(final_pos.y-marker_drag_start_pos.y)>0.0001f||
                           fabsf(final_pos.z-marker_drag_start_pos.z)>0.0001f)
                            editor_command_marker_set_position(g_editor_selection.index,final_pos,NULL);
                        else if(fabsf(final_radius-marker_drag_start_radius)>0.0001f)
                            editor_command_marker_set_radius(g_editor_selection.index,final_radius,NULL);
                    }
                    marker_drag_active=false;
                    marker_drag_dirty=false;
                    prop_gizmo_active_axis=0;
                }
                if(!lmb_now&&!marker_drag_active&&prop_gizmo_active_axis>0) prop_gizmo_active_axis=0;
            }
            marker_lmb_prev=lmb_now;
            marker_rmb_prev=rmb_now;

            world_markers_draw(&g_world_markers,&lr,&hud,marker_sel,proj,view,win_w,win_h);
            if(got_hit){
                Vec3 p0={hit.x-0.45f,hit.y+1.0f,hit.z};
                Vec3 p1={hit.x+0.45f,hit.y+1.0f,hit.z};
                Vec3 p2={hit.x,hit.y+1.0f,hit.z-0.45f};
                Vec3 p3={hit.x,hit.y+1.0f,hit.z+0.45f};
                lr_push(p0,p1,1.0f,1.0f,1.0f);
                lr_push(p2,p3,1.0f,1.0f,1.0f);
                lr_flush(&lr,proj,view);
            }
            if(!editor_ui_backend_is_imgui(&g_editor_ui))
                world_markers_hud(&hud,&g_world_markers,marker_sel,(WorldMarkerKind)marker_sel_type,win_w,win_h);
        }

        if(prop_place_mode){
            int pmx,pmy;
            Uint32 pmb=SDL_GetMouseState(&pmx,&pmy);
            bool lmb_raw=(pmb&SDL_BUTTON(1))!=0;
            bool lmb_now=lmb_raw&&!editor_gui_mouse_blocked;
            bool rmb_now=((pmb&SDL_BUTTON(3))!=0)&&!editor_gui_mouse_blocked;
            const Uint8 *pks=SDL_GetKeyboardState(NULL);
            bool place_new=pks[SDL_SCANCODE_LSHIFT]||pks[SDL_SCANCODE_RSHIFT];
            float search_x,search_y,search_w,search_h;
            bool over_search=false;
            bool over_transform_panel=false;
            bool over_placer_settings=false;
            Vec3 hit; bool got_hit=false;
            placer_search_box_rect(win_w,win_h,&search_x,&search_y,&search_w,&search_h);
            over_search=(pmx>=(int)search_x&&pmx<=(int)(search_x+search_w)&&
                         pmy>=(int)search_y&&pmy<=(int)(search_y+search_h));
            if(!editor_ui_backend_is_imgui(&g_editor_ui)){
            {
                float px,py,pw,ph,lh;
                int vis;
                placer_palette_panel_rect(win_w,win_h,&px,&py,&pw,&ph,&lh,&vis);
                float list_y=py+(g_placer.prefab_mode?74.0f:86.0f);
                bool over_palette=pmx>=(int)(px-4.0f)&&pmx<=(int)(px+pw+4.0f)&&
                                  pmy>=(int)(list_y-4.0f)&&pmy<=(int)(list_y+lh*(float)vis+4.0f);
                if(over_palette) editor_gui_mouse_blocked=true;
                if(lmb_now&&!g_placer.lmb_prev&&over_palette){
                    int row=(int)((((float)pmy)-list_y)/lh);
                    if(row>=0&&row<vis){
                        if(g_placer.prefab_mode){
                            int idx=g_placer.prefab_scroll+row;
                            if(idx>=0&&idx<g_prefabs.count){
                                g_placer.prefab_sel=idx;
                                g_placer.search_focus=false;
                                SDL_StopTextInput();
                                audio_play(SND_MENU_CLICK,70);
                            }
                        }else{
                            int fidx=g_placer.palette_scroll+row;
                            if(fidx>=0&&fidx<g_placer.filtered_count){
                                g_placer.palette_sel=fidx;
                                g_placer.search_focus=false;
                                SDL_StopTextInput();
                                audio_play(SND_MENU_CLICK,70);
                            }
                        }
                    }
                }
            }
            {
                float stx=(float)win_w-290.0f, sty=278.0f, stw=272.0f, sth=214.0f;
                float sbx=stx+10.0f, sby=sty+28.0f, sbw=58.0f, sbh=24.0f, sgap=8.0f;
                bool over;
                char pbuf[160];
                const char *model_name=placer_selected_model(&g_placer);
                const char *prefab_name=prefab_selected_name(&g_placer,&g_prefabs);
                const char *short_name=model_name?strrchr(model_name,'/'):NULL;
                short_name=short_name?short_name+1:(model_name?model_name:"(none)");
                over_placer_settings=pmx>=(int)stx&&pmx<=(int)(stx+stw)&&pmy>=(int)sty&&pmy<=(int)(sty+sth);
                editor_gui_mouse_blocked=editor_gui_mouse_blocked||over_placer_settings;
                editor_shell_panel(&hud,stx,sty,stw,sth);
                hud_str(&hud,stx+12.0f,sty+9.0f,0.86f,"MODEL PLACER",0.48f,0.60f,0.35f,0.95f);
                #define PLACER_BTN(COL,ROW,LABEL,ACTION) do{ \
                    float _x=sbx+((float)(COL))*(sbw+sgap); \
                    float _y=sby+((float)(ROW))*(sbh+8.0f); \
                    over=menu_btn(&hud,_x,_y,sbw,sbh,LABEL,1.0f,pmx,pmy,true,NULL); \
                    if(over&&lmb_raw&&!g_placer.lmb_prev){ ACTION; audio_play(SND_MENU_CLICK,70); } \
                }while(0)
                PLACER_BTN(0,0,g_placer.prefab_mode?"Model":"Prefab",{
                    g_placer.prefab_mode=!g_placer.prefab_mode;
                    g_placer.search_focus=false;
                    if(g_placer.prefab_mode) SDL_StopTextInput();
                });
                PLACER_BTN(1,0,"< Item",{
                    if(g_placer.prefab_mode&&g_prefabs.count>0){
                        g_placer.prefab_sel=(g_placer.prefab_sel+g_prefabs.count-1)%g_prefabs.count;
                        if(g_placer.prefab_scroll>g_placer.prefab_sel) g_placer.prefab_scroll=g_placer.prefab_sel;
                    }else if(!g_placer.prefab_mode&&g_placer.filtered_count>0){
                        g_placer.palette_sel=(g_placer.palette_sel+g_placer.filtered_count-1)%g_placer.filtered_count;
                        if(g_placer.palette_scroll>g_placer.palette_sel) g_placer.palette_scroll=g_placer.palette_sel;
                    }
                });
                PLACER_BTN(2,0,"Item >",{
                    if(g_placer.prefab_mode&&g_prefabs.count>0){
                        g_placer.prefab_sel=(g_placer.prefab_sel+1)%g_prefabs.count;
                        if(g_placer.prefab_sel>=g_placer.prefab_scroll+12) g_placer.prefab_scroll=g_placer.prefab_sel-11;
                    }else if(!g_placer.prefab_mode&&g_placer.filtered_count>0){
                        g_placer.palette_sel=(g_placer.palette_sel+1)%g_placer.filtered_count;
                        if(g_placer.palette_sel>=g_placer.palette_scroll+12) g_placer.palette_scroll=g_placer.palette_sel-11;
                    }
                });
                PLACER_BTN(3,0,"Search",{
                    if(!g_placer.prefab_mode){
                        g_placer.search_focus=!g_placer.search_focus;
                        if(g_placer.search_focus) SDL_StartTextInput(); else SDL_StopTextInput();
                    }
                });
                PLACER_BTN(0,1,"Rot -",{
                    g_placer.place_ry-=15.0f;
                    if(g_placer.place_ry<-180.0f) g_placer.place_ry+=360.0f;
                });
                PLACER_BTN(1,1,"Rot +",{
                    g_placer.place_ry+=15.0f;
                    if(g_placer.place_ry>180.0f) g_placer.place_ry-=360.0f;
                });
                PLACER_BTN(2,1,"Scale-",{
                    g_placer.place_scale=clampf(g_placer.place_scale-0.05f,0.05f,20.0f);
                });
                PLACER_BTN(3,1,"Scale+",{
                    g_placer.place_scale=clampf(g_placer.place_scale+0.05f,0.05f,20.0f);
                });
                PLACER_BTN(0,2,"Coll",{
                    g_placer.place_coll=(PropCollType)(((int)g_placer.place_coll+1)%3);
                });
                PLACER_BTN(1,2,g_placer.snap_y?"SnapY":"FreeY",{
                    g_placer.snap_y=!g_placer.snap_y;
                });
                PLACER_BTN(2,2,"Save",{
                    placer_save(&g_props);
                });
                PLACER_BTN(3,2,"Rescan",{
                    placer_scan_models(&g_placer);
                    g_placer.search_focus=false;
                    SDL_StopTextInput();
                });
                #undef PLACER_BTN
                {
                    char row_buf[160];
                    int step=0;
                    snprintf(row_buf,sizeof(row_buf),"%s",g_placer.prefab_mode?(prefab_name?prefab_name:"(none)"):short_name);
                    step=editor_shell_stepper(&hud,stx+12.0f,sty+110.0f,stw-24.0f,26.0f,
                                              g_placer.prefab_mode?"Prefab":"Model",row_buf,
                                              pmx,pmy,lmb_raw&&!g_placer.lmb_prev,"<",">");
                    if(step<0){
                        if(g_placer.prefab_mode&&g_prefabs.count>0){
                            g_placer.prefab_sel=(g_placer.prefab_sel+g_prefabs.count-1)%g_prefabs.count;
                            if(g_placer.prefab_scroll>g_placer.prefab_sel) g_placer.prefab_scroll=g_placer.prefab_sel;
                        }else if(!g_placer.prefab_mode&&g_placer.filtered_count>0){
                            g_placer.palette_sel=(g_placer.palette_sel+g_placer.filtered_count-1)%g_placer.filtered_count;
                            if(g_placer.palette_scroll>g_placer.palette_sel) g_placer.palette_scroll=g_placer.palette_sel;
                        }
                        audio_play(SND_MENU_CLICK,70);
                    }else if(step>0){
                        if(g_placer.prefab_mode&&g_prefabs.count>0){
                            g_placer.prefab_sel=(g_placer.prefab_sel+1)%g_prefabs.count;
                            if(g_placer.prefab_sel>=g_placer.prefab_scroll+12) g_placer.prefab_scroll=g_placer.prefab_sel-11;
                        }else if(!g_placer.prefab_mode&&g_placer.filtered_count>0){
                            g_placer.palette_sel=(g_placer.palette_sel+1)%g_placer.filtered_count;
                            if(g_placer.palette_sel>=g_placer.palette_scroll+12) g_placer.palette_scroll=g_placer.palette_sel-11;
                        }
                        audio_play(SND_MENU_CLICK,70);
                    }
                    snprintf(row_buf,sizeof(row_buf),"%.0f deg",(double)g_placer.place_ry);
                    step=editor_shell_stepper(&hud,stx+12.0f,sty+138.0f,stw-24.0f,26.0f,
                                              "Rotation",row_buf,pmx,pmy,lmb_raw&&!g_placer.lmb_prev,"-","+");
                    if(step<0){
                        g_placer.place_ry-=15.0f;
                        if(g_placer.place_ry<-180.0f) g_placer.place_ry+=360.0f;
                        audio_play(SND_MENU_CLICK,70);
                    }else if(step>0){
                        g_placer.place_ry+=15.0f;
                        if(g_placer.place_ry>180.0f) g_placer.place_ry-=360.0f;
                        audio_play(SND_MENU_CLICK,70);
                    }
                    snprintf(row_buf,sizeof(row_buf),"%.2f",(double)g_placer.place_scale);
                    step=editor_shell_stepper(&hud,stx+12.0f,sty+166.0f,stw-24.0f,26.0f,
                                              "Scale",row_buf,pmx,pmy,lmb_raw&&!g_placer.lmb_prev,"-","+");
                    if(step<0){
                        g_placer.place_scale=clampf(g_placer.place_scale-0.05f,0.05f,20.0f);
                        audio_play(SND_MENU_CLICK,70);
                    }else if(step>0){
                        g_placer.place_scale=clampf(g_placer.place_scale+0.05f,0.05f,20.0f);
                        audio_play(SND_MENU_CLICK,70);
                    }
                }
                snprintf(pbuf,sizeof(pbuf),"%s  |  %s  |  %s",
                         prop_coll_name(g_placer.place_coll),g_placer.snap_y?"SnapY":"FreeY",
                         g_placer.prefab_mode?"Prefab Pack":"Single Model");
                hud_str(&hud,stx+12.0f,sty+196.0f,0.74f,pbuf,0.62f,0.76f,0.64f,0.90f);
            }
            } /* end !imgui HUD placer panels */
            if(!editor_ui_backend_is_imgui(&g_editor_ui)&&g_placer.selected_prop>=0&&g_placer.selected_prop<g_props.count&&g_props.list[g_placer.selected_prop].active){
                float tx=(float)win_w-290.0f, ty=446.0f, tw=272.0f, th=124.0f;
                float bx=tx+10.0f, by=ty+28.0f, bw=58.0f, bh=24.0f, gap=8.0f;
                bool over;
                Prop *selp=&g_props.list[g_placer.selected_prop];
                over_transform_panel=pmx>=(int)tx&&pmx<=(int)(tx+tw)&&pmy>=(int)ty&&pmy<=(int)(ty+th);
                editor_shell_panel(&hud,tx,ty,tw,th);
                hud_str(&hud,tx+12.0f,ty+9.0f,0.86f,"SELECTED PROP",0.48f,0.60f,0.35f,0.95f);
                over=menu_btn(&hud,bx,by,bw,bh,"Move",1.0f,pmx,pmy,true,NULL);
                if(prop_mouse_mode==1) hud_rect(&hud,bx-2.0f,by-2.0f,bw+4.0f,bh+4.0f,0.85f,0.68f,0.20f,0.75f);
                if(over&&lmb_raw&&!g_placer.lmb_prev){ prop_mouse_mode=1; prop_mouse_dragging=false; audio_play(SND_MENU_CLICK,70); }
                over=menu_btn(&hud,bx+bw+gap,by,bw,bh,"Rotate",1.0f,pmx,pmy,true,NULL);
                if(prop_mouse_mode==2) hud_rect(&hud,bx+bw+gap-2.0f,by-2.0f,bw+4.0f,bh+4.0f,0.85f,0.68f,0.20f,0.75f);
                if(over&&lmb_raw&&!g_placer.lmb_prev){ prop_mouse_mode=2; prop_mouse_dragging=false; audio_play(SND_MENU_CLICK,70); }
                over=menu_btn(&hud,bx+(bw+gap)*2.0f,by,bw,bh,"Scale",1.0f,pmx,pmy,true,NULL);
                if(prop_mouse_mode==3) hud_rect(&hud,bx+(bw+gap)*2.0f-2.0f,by-2.0f,bw+4.0f,bh+4.0f,0.85f,0.68f,0.20f,0.75f);
                if(over&&lmb_raw&&!g_placer.lmb_prev){ prop_mouse_mode=3; prop_mouse_dragging=false; audio_play(SND_MENU_CLICK,70); }
                over=menu_btn(&hud,bx+(bw+gap)*3.0f,by,bw,bh,"Delete",1.0f,pmx,pmy,true,NULL);
                if(over&&lmb_raw&&!g_placer.lmb_prev){
                    int bi=g_placer.selected_prop;
                    editor_command_prop_delete(bi,NULL);
                    g_placer.selected_prop=-1;
                    prop_mouse_mode=0;
                    audio_play(SND_MENU_CLICK,70);
                }
                over=menu_btn(&hud,bx,by+bh+8.0f,bw,bh,"Y -",1.0f,pmx,pmy,true,NULL);
                if(over&&lmb_raw&&!g_placer.lmb_prev){
                    editor_command_prop_set_y(g_placer.selected_prop,selp->pos.y-0.5f,&g_placer,NULL);
                    audio_play(SND_MENU_CLICK,70);
                }
                over=menu_btn(&hud,bx+bw+gap,by+bh+8.0f,bw,bh,"Y +",1.0f,pmx,pmy,true,NULL);
                if(over&&lmb_raw&&!g_placer.lmb_prev){
                    editor_command_prop_set_y(g_placer.selected_prop,selp->pos.y+0.5f,&g_placer,NULL);
                    audio_play(SND_MENU_CLICK,70);
                }
                over=menu_btn(&hud,bx+(bw+gap)*2.0f,by+bh+8.0f,bw,bh,"Undo",1.0f,pmx,pmy,true,NULL);
                if(over&&lmb_raw&&!g_placer.lmb_prev){ undo_perform(); audio_play(SND_MENU_CLICK,70); }
                over=menu_btn(&hud,bx+(bw+gap)*3.0f,by+bh+8.0f,bw,bh,"Save",1.0f,pmx,pmy,true,NULL);
                if(over&&lmb_raw&&!g_placer.lmb_prev){ placer_save(&g_props); audio_play(SND_MENU_CLICK,70); }
                {
                    char mbuf[160],nbuf[160];
                    const char *base=strrchr(selp->path,'/');
                    base=base?base+1:selp->path;
                    snprintf(mbuf,sizeof(mbuf),"%s  |  %s",
                             base,prop_mouse_mode==1?"Move":prop_mouse_mode==2?"Rotate":prop_mouse_mode==3?"Scale":"Select");
                    snprintf(nbuf,sizeof(nbuf),"Pos %.1f %.1f %.1f   Scale %.2f",
                             (double)selp->pos.x,(double)selp->pos.y,(double)selp->pos.z,(double)selp->scale);
                    hud_str(&hud,tx+12.0f,ty+92.0f,0.78f,mbuf,0.83f,0.77f,0.56f,0.92f);
                    hud_str(&hud,tx+12.0f,ty+104.0f,0.74f,nbuf,0.74f,0.86f,0.95f,0.90f);
                }
            }
            got_hit=river_screen_to_world(pmx,pmy,win_w,win_h,proj,view,&hit);
            if(got_hit){
                if(g_placer.snap_y) hit.y=scene_ground_y(hit.x,hit.z);
                g_placer.ghost_pos=hit;
                g_placer.has_ghost=true;
            }
            if(!editor_ui_backend_is_imgui(&g_editor_ui)&&lmb_now&&!g_placer.lmb_prev){
                bool want_focus=(!g_placer.prefab_mode&&over_search);
                if(g_placer.search_focus!=want_focus){
                    g_placer.search_focus=want_focus;
                    if(g_placer.search_focus) SDL_StartTextInput();
                    else SDL_StopTextInput();
                }
            }
            if(g_placer.selected_prop>=0&&g_placer.selected_prop<g_props.count&&g_props.list[g_placer.selected_prop].active&&prop_mouse_mode>0&&!over_search&&!over_transform_panel&&!over_placer_settings){
                Prop *sp=&g_props.list[g_placer.selected_prop];
                static PropSnap prop_drag_snap_before;
                if(prop_mouse_mode==1&&lmb_now&&got_hit){
                    if(!prop_mouse_dragging){
                        prop_mouse_dragging=true;
                        prop_drag_snap_before=undo_snap_prop(g_placer.selected_prop);
                    }
                    sp->pos.x=hit.x;
                    sp->pos.z=hit.z;
                    if(g_placer.snap_y) sp->pos.y=scene_ground_y(hit.x,hit.z);
                    prop_refresh_collision(sp);
                    placer_sync_from_prop(&g_placer,sp);
                }else if((prop_mouse_mode==2||prop_mouse_mode==3)&&lmb_raw){
                    if(!prop_mouse_dragging){
                        prop_mouse_dragging=true;
                        prop_drag_snap_before=undo_snap_prop(g_placer.selected_prop);
                        prop_mouse_last_x=pmx;
                        prop_mouse_last_y=pmy;
                    }else{
                        int dx=pmx-prop_mouse_last_x;
                        int dy=pmy-prop_mouse_last_y;
                        if(prop_mouse_mode==2){
                            sp->rot_y+=dx*0.6f;
                            sp->rot_x+=dy*0.25f;
                        }else if(prop_mouse_mode==3){
                            sp->scale=clampf(sp->scale-dy*0.01f,0.01f,20.0f);
                            if(g_placer.snap_y) sp->pos.y=scene_ground_y(sp->pos.x,sp->pos.z);
                        }
                        prop_refresh_collision(sp);
                        prop_mouse_last_x=pmx;
                        prop_mouse_last_y=pmy;
                        placer_sync_from_prop(&g_placer,sp);
                    }
                }else if(!lmb_raw){
                    if(prop_mouse_dragging){
                        editor_command_prop_commit_xform(g_placer.selected_prop,prop_drag_snap_before,&g_placer,NULL);
                    }
                    prop_mouse_dragging=false;
                }
            }else if(lmb_now&&!g_placer.lmb_prev&&!over_search&&!over_transform_panel&&!over_placer_settings&&got_hit&&((!g_placer.prefab_mode&&g_placer.filtered_count>0)||(g_placer.prefab_mode&&g_prefabs.count>0))){
                int picked=props_pick_screen(&g_props,pmx,pmy,win_w,win_h,proj,view,28.0f);
                if(!place_new&&picked>=0){
                    g_placer.selected_prop=picked;
                    prop_sel=picked;
                    prop_mouse_mode=0;
                    editor_selection_set(EDITOR_SELECTION_PROP,picked);
                    placer_sync_from_prop(&g_placer,&g_props.list[picked]);
                    printf("[PropPlacer] Selected prop %d (%s)\n",picked,g_props.list[picked].path);
                }else{
                    if(g_placer.prefab_mode){
                        if(g_prefabs.count>0&&g_placer.prefab_sel>=0&&g_placer.prefab_sel<g_prefabs.count){
                            int idx=-1;
                            editor_command_prefab_place_instance(g_placer.prefab_sel,hit,g_placer.place_ry,
                                                                 g_placer.place_scale,&ls,&idx,NULL);
                            if(idx>=0){
                                g_placer.selected_prop=idx;
                                prop_sel=idx;
                                editor_selection_set(EDITOR_SELECTION_PROP,idx);
                                printf("[Prefabs] Placed '%s' at (%.1f,%.1f,%.1f) ry=%.0f sc=%.2f\n",
                                       g_prefabs.items[g_placer.prefab_sel].name,
                                       (double)hit.x,(double)hit.y,(double)hit.z,
                                       (double)g_placer.place_ry,(double)g_placer.place_scale);
                            }
                        }
                    }else{
                        const char *path=placer_selected_model(&g_placer);
                        int idx=-1;
                        if(path&&editor_command_prop_create_model(path,hit,0,g_placer.place_ry,0,
                                         g_placer.place_scale,(int)g_placer.place_coll,
                                         g_placer.snap_y,&idx,NULL)){
                            g_placer.selected_prop=idx;
                            prop_sel=idx;
                            editor_selection_set(EDITOR_SELECTION_PROP,idx);
                            printf("[PropPlacer] Placed '%s' at (%.1f,%.1f,%.1f) ry=%.0f sc=%.2f\n",
                                   path,(double)hit.x,(double)hit.y,(double)hit.z,
                                   (double)g_placer.place_ry,(double)g_placer.place_scale);
                        }
                    }
                }
            }
            if(rmb_now&&!g_placer.rmb_prev&&!over_transform_panel&&!over_placer_settings&&got_hit){
                int bi=props_pick_screen(&g_props,pmx,pmy,win_w,win_h,proj,view,34.0f);
                if(bi>=0){
                    Prop *p=&g_props.list[bi];
                    editor_command_prop_delete(bi,NULL);
                    if(g_placer.selected_prop==bi) g_placer.selected_prop=-1;
                    if(editor_selection_is(EDITOR_SELECTION_PROP,bi)) editor_selection_clear();
                    printf("[PropPlacer] Deleted prop %d (%s)\n",bi,p->path);
                }
            }
            g_placer.lmb_prev=lmb_now;
            g_placer.rmb_prev=rmb_now;
            if(!lmb_raw) prop_mouse_dragging=false;
            if(g_placer.has_ghost)
                placer_draw_ghost(&lr,g_placer.ghost_pos,g_placer.place_scale,proj,view);
            if(g_placer.selected_prop>=0&&g_placer.selected_prop<g_props.count&&g_props.list[g_placer.selected_prop].active)
                props_debug_draw(&g_props,g_placer.selected_prop,cam.pos);
            if(!editor_ui_backend_is_imgui(&g_editor_ui)) placer_hud(&hud,&g_placer,win_w,win_h);
        }

        if(particle_fx_edit){
            static bool fx_gizmo_drag_active=false;
            static bool fx_gizmo_drag_dirty=false;
            static WorldFx fx_gizmo_drag_before;
            int fmx,fmy;
            Uint32 fmb=SDL_GetMouseState(&fmx,&fmy);
            bool lmb_raw=(fmb&SDL_BUTTON(1))!=0;
            bool fx_panel_blocked=!editor_ui_backend_is_imgui(&g_editor_ui)&&world_fx_panel_hit(fmx,fmy,win_w,win_h);
            bool lmb_now=lmb_raw&&!editor_gui_mouse_blocked&&!fx_panel_blocked;
            bool rmb_now=((fmb&SDL_BUTTON(3))!=0)&&!editor_gui_mouse_blocked&&!fx_panel_blocked;
            const Uint8 *fks=SDL_GetKeyboardState(NULL);
            bool force_place=fks[SDL_SCANCODE_LSHIFT]||fks[SDL_SCANCODE_RSHIFT];
            Vec3 hit; bool got_hit=river_screen_to_world(fmx,fmy,win_w,win_h,proj,view,&hit);
            bool fx_gizmo_hovered=false;
            if(got_hit){
                hit.y=terrain_h(hit.x,hit.z);
                g_world_fx_editor.ghost_pos=hit;
                g_world_fx_editor.has_ghost=true;
            }else{
                g_world_fx_editor.has_ghost=false;
            }
            if(g_world_fx.selected>=0&&g_world_fx.selected<g_world_fx.count&&g_world_fx.items[g_world_fx.selected].active){
                WorldFx *fx=&g_world_fx.items[g_world_fx.selected];
                Vec3 fx_origin=(Vec3){fx->pos.x,fx->pos.y+fmaxf(0.45f,fx->height*0.50f),fx->pos.z};
                float fx_len=clampf(vec3_dist(fx_origin,cam.pos)*0.14f,1.0f,5.0f);
                int fx_hot_axis=(prop_gizmo_active_axis>0)?prop_gizmo_active_axis:
                    editor_gizmo_pick_axis(fx_origin,fx_len,false,0.0f,fmx,fmy,win_w,win_h,proj,view);
                fx_gizmo_hovered=fx_hot_axis>0;
                editor_gizmo_draw(fx_origin,fx_len,false,0.0f,fx_hot_axis,prop_gizmo_active_axis);
                if(!fx_gizmo_drag_active&&lmb_now&&!g_placer.lmb_prev&&fx_hot_axis>0){
                    prop_gizmo_active_axis=fx_hot_axis;
                    prop_gizmo_drag_accum=0.0f;
                    prop_gizmo_start_pos=fx->pos;
                    prop_mouse_last_x=fmx;
                    prop_mouse_last_y=fmy;
                    fx_gizmo_drag_before=*fx;
                    fx_gizmo_drag_active=true;
                    fx_gizmo_drag_dirty=false;
                }else if(fx_gizmo_drag_active&&prop_gizmo_active_axis>0&&lmb_raw){
                    Vec3 go,gx,gy,gz,ga;
                    float osx,osy,asx,asy,axis_dx,axis_dy,axis_len,delta_px;
                    if(prop_gizmo_active_axis>=5&&prop_gizmo_active_axis<=7){
                        Vec3 ro,rd,pro,prd,cur_hit,prev_hit;
                        Vec3 ppt,pnorm;
                        if(prop_gizmo_active_axis==5){ppt=(Vec3){0,prop_gizmo_start_pos.y,0};pnorm=(Vec3){0,1,0};}
                        else if(prop_gizmo_active_axis==6){ppt=(Vec3){0,0,prop_gizmo_start_pos.z};pnorm=(Vec3){0,0,1};}
                        else{ppt=(Vec3){prop_gizmo_start_pos.x,0,0};pnorm=(Vec3){1,0,0};}
                        editor_screen_to_ray(fmx,fmy,win_w,win_h,proj,view,&ro,&rd);
                        editor_screen_to_ray(prop_mouse_last_x,prop_mouse_last_y,win_w,win_h,proj,view,&pro,&prd);
                        if(editor_ray_plane_intersect(ro,rd,ppt,pnorm,&cur_hit)&&editor_ray_plane_intersect(pro,prd,ppt,pnorm,&prev_hit)){
                            float ddx=cur_hit.x-prev_hit.x;
                            float ddy=cur_hit.y-prev_hit.y;
                            float ddz=cur_hit.z-prev_hit.z;
                            if(prop_gizmo_active_axis==5){fx->pos.x+=ddx;fx->pos.z+=ddz;}
                            else if(prop_gizmo_active_axis==6){fx->pos.x+=ddx;fx->pos.y+=ddy;}
                            else{fx->pos.y+=ddy;fx->pos.z+=ddz;}
                            if(selection_gizmo_snap){
                                fx->pos.x=editor_gizmo_snap_value(fx->pos.x,0.5f);
                                fx->pos.y=editor_gizmo_snap_value(fx->pos.y,0.5f);
                                fx->pos.z=editor_gizmo_snap_value(fx->pos.z,0.5f);
                            }
                            fx_gizmo_drag_dirty=true;
                        }
                    }else{
                        editor_gizmo_axes(fx_origin,fx_len,false,0.0f,&gx,&gy,&gz);
                        go=fx_origin;
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
                                float move_delta;
                                Vec3 x_axis,y_axis,z_axis;
                                axis_dx/=axis_len;
                                axis_dy/=axis_len;
                                delta_px=((float)(fmx-prop_mouse_last_x))*axis_dx+((float)(fmy-prop_mouse_last_y))*axis_dy;
                                prop_gizmo_drag_accum+=delta_px;
                                move_delta=prop_gizmo_drag_accum*cam.dist*0.012f;
                                if(selection_gizmo_snap) move_delta=editor_gizmo_snap_value(move_delta,0.5f);
                                fx->pos=prop_gizmo_start_pos;
                                editor_gizmo_basis_yaw(false,0.0f,&x_axis,&y_axis,&z_axis);
                                if(prop_gizmo_active_axis==1) fx->pos=vec3_add(fx->pos,vec3_scale(x_axis,move_delta));
                                else if(prop_gizmo_active_axis==2) fx->pos=vec3_add(fx->pos,vec3_scale(y_axis,move_delta));
                                else fx->pos=vec3_add(fx->pos,vec3_scale(z_axis,move_delta));
                                fx_gizmo_drag_dirty=true;
                            }
                        }
                    }
                    prop_mouse_last_x=fmx;
                    prop_mouse_last_y=fmy;
                    world_fx_editor_sync_from_fx(&g_world_fx_editor,fx);
                    if(fx->light_enabled&&fx->light_idx>=0&&fx->light_idx<ls.num_points){
                        ls.points[fx->light_idx].pos=(Vec3){fx->pos.x,fx->pos.y+0.65f*fx->height,fx->pos.z};
                    }
                }else if(fx_gizmo_drag_active&&!lmb_raw){
                    if(fx_gizmo_drag_dirty&&g_world_fx.selected>=0&&g_world_fx.selected<g_world_fx.count)
                        undo_push_wfx_edit(g_world_fx.selected,fx_gizmo_drag_before,g_world_fx.items[g_world_fx.selected]);
                    fx_gizmo_drag_active=false;
                    fx_gizmo_drag_dirty=false;
                    prop_gizmo_active_axis=0;
                }
            }else if(fx_gizmo_drag_active&&!lmb_raw){
                fx_gizmo_drag_active=false;
                fx_gizmo_drag_dirty=false;
                prop_gizmo_active_axis=0;
            }
            if(lmb_now&&!g_placer.lmb_prev&&got_hit){
                int picked=world_fx_pick_nearest(&g_world_fx,hit,3.0f);
                if(fx_gizmo_hovered||fx_gizmo_drag_active){
                    /* Gizmo owns this click. */
                }else if(!force_place&&picked>=0){
                    g_world_fx.selected=picked;
                    world_fx_editor_sync_from_fx(&g_world_fx_editor,&g_world_fx.items[picked]);
                    printf("[WorldFX] Selected %d (%s)\n",picked,world_fx_kind_name(g_world_fx.items[picked].kind));
                }else{
                    int idx=world_fx_place_with_undo(&g_world_fx,&ls,hit,&g_world_fx_editor);
                    if(idx>=0) printf("[WorldFX] Placed %s at (%.1f,%.1f,%.1f)\n",world_fx_kind_name(g_world_fx.items[idx].kind),(double)hit.x,(double)hit.y,(double)hit.z);
                }
            }
            if(rmb_now&&!g_placer.rmb_prev&&got_hit){
                int picked=world_fx_pick_nearest(&g_world_fx,hit,4.0f);
                if(picked>=0){
                    world_fx_delete_with_undo(&g_world_fx,&ls,picked);
                    printf("[WorldFX] Deleted effect %d\n",picked);
                }
            }
            if(!lmb_raw&&!fx_gizmo_drag_active&&prop_gizmo_active_axis>0)
                prop_gizmo_active_axis=0;
            g_placer.lmb_prev=lmb_now;
            g_placer.rmb_prev=rmb_now;
            world_fx_draw_editor(&lr,&g_world_fx,&g_world_fx_editor,proj,view);
            if(!editor_ui_backend_is_imgui(&g_editor_ui))
                world_fx_hud(&hud,&g_world_fx,&g_world_fx_editor,&ls,win_w,win_h);
        }

        if(prop_edit){
            int emx,emy;
            Uint32 emb=SDL_GetMouseState(&emx,&emy);
            bool lmb_raw=(emb&SDL_BUTTON(1))!=0;
            bool lmb_now=lmb_raw&&!editor_gui_mouse_blocked;
            static bool prop_edit_lmb_prev=false;
            bool over_transform_panel=false;
            int hover_prop=props_pick_screen(&g_props,emx,emy,win_w,win_h,proj,view,42.0f);
            Vec3 prop_hit={0};
            bool prop_got_hit=river_screen_to_world(emx,emy,win_w,win_h,proj,view,&prop_hit);
            if(prop_got_hit) prop_hit.y=scene_ground_y(prop_hit.x,prop_hit.z);
            if(!editor_ui_backend_is_imgui(&g_editor_ui)&&g_props.count>0&&prop_sel>=0&&prop_sel<g_props.count&&g_props.list[prop_sel].active){
                Prop *sp=&g_props.list[prop_sel];
                float sx=0.0f,sy=0.0f,sw=0.0f,sh=0.0f;
                float tx,ty,tw=272.0f,th=196.0f;
                float bx,by,bw=58.0f,bh=24.0f,gap=8.0f;
                bool over;
                if(!prop_selection_billboard_rect(sp,win_w,win_h,proj,view,&sx,&sy,&sw,&sh)){
                    sx=(float)win_w*0.5f-40.0f;
                    sy=(float)win_h*0.5f;
                    sw=80.0f;
                    sh=120.0f;
                }
                tx=clampf(sx+sw*0.5f-tw*0.5f,264.0f,(float)win_w-tw-18.0f);
                ty=clampf(sy-th-18.0f,26.0f,(float)win_h-th-54.0f);
                bx=tx+10.0f; by=ty+28.0f;
                over_transform_panel=emx>=(int)tx&&emx<=(int)(tx+tw)&&emy>=(int)ty&&emy<=(int)(ty+th);
                editor_shell_panel(&hud,tx,ty,tw,th);
                hud_str(&hud,tx+12.0f,ty+9.0f,0.86f,"TRANSFORM TOOLBAR",0.58f,0.68f,0.42f,0.95f);
                over=menu_btn(&hud,bx,by,bw,bh,"Select",1.0f,emx,emy,true,NULL);
                if(prop_mouse_mode==0) hud_rect(&hud,bx-2.0f,by-2.0f,bw+4.0f,bh+4.0f,0.85f,0.68f,0.20f,0.75f);
                if(over&&lmb_raw&&!prop_edit_lmb_prev){ prop_mouse_mode=0; prop_mouse_dragging=false; prop_gizmo_active_axis=0; audio_play(SND_MENU_CLICK,70); }
                over=menu_btn(&hud,bx+bw+gap,by,bw,bh,"Move",1.0f,emx,emy,true,NULL);
                if(prop_mouse_mode==1) hud_rect(&hud,bx+bw+gap-2.0f,by-2.0f,bw+4.0f,bh+4.0f,0.85f,0.68f,0.20f,0.75f);
                if(over&&lmb_raw&&!prop_edit_lmb_prev){ prop_mouse_mode=1; prop_mouse_dragging=false; prop_gizmo_active_axis=0; audio_play(SND_MENU_CLICK,70); }
                over=menu_btn(&hud,bx+(bw+gap)*2.0f,by,bw,bh,"Rotate",1.0f,emx,emy,true,NULL);
                if(prop_mouse_mode==2) hud_rect(&hud,bx+(bw+gap)*2.0f-2.0f,by-2.0f,bw+4.0f,bh+4.0f,0.85f,0.68f,0.20f,0.75f);
                if(over&&lmb_raw&&!prop_edit_lmb_prev){ prop_mouse_mode=2; prop_mouse_dragging=false; prop_gizmo_active_axis=0; audio_play(SND_MENU_CLICK,70); }
                over=menu_btn(&hud,bx+(bw+gap)*3.0f,by,bw,bh,"Scale",1.0f,emx,emy,true,NULL);
                if(prop_mouse_mode==3) hud_rect(&hud,bx+(bw+gap)*3.0f-2.0f,by-2.0f,bw+4.0f,bh+4.0f,0.85f,0.68f,0.20f,0.75f);
                if(over&&lmb_raw&&!prop_edit_lmb_prev){ prop_mouse_mode=3; prop_mouse_dragging=false; prop_gizmo_active_axis=0; audio_play(SND_MENU_CLICK,70); }
                over=menu_btn(&hud,bx,by+bh+8.0f,bw,bh,"Y -",1.0f,emx,emy,true,NULL);
                if(over&&lmb_raw&&!prop_edit_lmb_prev){
                    editor_command_prop_set_y(prop_sel,g_props.list[prop_sel].pos.y-0.5f,&g_placer,NULL);
                    audio_play(SND_MENU_CLICK,70);
                }
                over=menu_btn(&hud,bx+bw+gap,by+bh+8.0f,bw,bh,"Y +",1.0f,emx,emy,true,NULL);
                if(over&&lmb_raw&&!prop_edit_lmb_prev){
                    editor_command_prop_set_y(prop_sel,g_props.list[prop_sel].pos.y+0.5f,&g_placer,NULL);
                    audio_play(SND_MENU_CLICK,70);
                }
                over=menu_btn(&hud,bx+(bw+gap)*2.0f,by+bh+8.0f,bw,bh,"Ground",1.0f,emx,emy,true,NULL);
                if(over&&lmb_raw&&!prop_edit_lmb_prev){
                    editor_command_prop_set_y(prop_sel,scene_ground_y(g_props.list[prop_sel].pos.x,g_props.list[prop_sel].pos.z),&g_placer,NULL);
                    audio_play(SND_MENU_CLICK,70);
                }
                over=menu_btn(&hud,bx+(bw+gap)*3.0f,by+bh+8.0f,bw,bh,"Save",1.0f,emx,emy,true,NULL);
                if(over&&lmb_raw&&!prop_edit_lmb_prev){ placer_save(&g_props); audio_play(SND_MENU_CLICK,70); }
                over=menu_btn(&hud,bx,by+(bh+8.0f)*2.0f,bw,bh,g_placer.snap_y?"SnapY":"FreeY",1.0f,emx,emy,true,NULL);
                if(over&&lmb_raw&&!prop_edit_lmb_prev){ g_placer.snap_y=!g_placer.snap_y; audio_play(SND_MENU_CLICK,70); }
                over=menu_btn(&hud,bx+bw+gap,by+(bh+8.0f)*2.0f,bw,bh,g_prop_sel_count>1?"Del All":"Delete",1.0f,emx,emy,true,NULL);
                if(over&&lmb_raw&&!prop_edit_lmb_prev){
                    if(g_prop_sel_count>1){
                        prop_multi_delete_all(&prop_sel);
                    } else {
                        editor_command_prop_delete(prop_sel,NULL);
                    }
                    prop_mouse_mode=0; prop_gizmo_active_axis=0;
                    audio_play(SND_MENU_CLICK,70);
                }
                over=menu_btn(&hud,bx,by+(bh+8.0f)*3.0f,bw,bh,"None",1.0f,emx,emy,true,NULL);
                if(g_props.list[prop_sel].coll_type==PROP_COLL_NONE) hud_rect(&hud,bx-2.0f,by+(bh+8.0f)*3.0f-2.0f,bw+4.0f,bh+4.0f,0.85f,0.68f,0.20f,0.75f);
                if(over&&lmb_raw&&!prop_edit_lmb_prev){ prop_apply_collision_mode(prop_sel,PROP_COLL_NONE); audio_play(SND_MENU_CLICK,70); }
                over=menu_btn(&hud,bx+bw+gap,by+(bh+8.0f)*3.0f,bw,bh,"AABB",1.0f,emx,emy,true,NULL);
                if(g_props.list[prop_sel].coll_type==PROP_COLL_AABB) hud_rect(&hud,bx+bw+gap-2.0f,by+(bh+8.0f)*3.0f-2.0f,bw+4.0f,bh+4.0f,0.85f,0.68f,0.20f,0.75f);
                if(over&&lmb_raw&&!prop_edit_lmb_prev){ prop_apply_collision_mode(prop_sel,PROP_COLL_AABB); audio_play(SND_MENU_CLICK,70); }
                over=menu_btn(&hud,bx+(bw+gap)*2.0f,by+(bh+8.0f)*3.0f,bw,bh,"Mesh",1.0f,emx,emy,true,NULL);
                if(g_props.list[prop_sel].coll_type==PROP_COLL_MESH) hud_rect(&hud,bx+(bw+gap)*2.0f-2.0f,by+(bh+8.0f)*3.0f-2.0f,bw+4.0f,bh+4.0f,0.85f,0.68f,0.20f,0.75f);
                if(over&&lmb_raw&&!prop_edit_lmb_prev){ prop_apply_collision_mode(prop_sel,PROP_COLL_MESH); audio_play(SND_MENU_CLICK,70); }
                hud_str(&hud,tx+12.0f,ty+150.0f,0.82f,
                        prop_mouse_mode==1?"Drag a colored X / Y / Z handle to move on one axis":
                        prop_mouse_mode==2?"Drag a colored X / Y / Z handle to rotate on one axis":
                        prop_mouse_mode==3?"Drag left or right to scale the selected prop":
                        "Select mode: click a prop in the world to target it",
                        0.84f,0.80f,0.62f,0.92f);
                prop_selection_billboard_draw(&hud,sp,win_w,win_h,proj,view,true,false);
            }
            /* Gizmo pick + draw — runs for all UI backends */
            if(g_props.count>0&&prop_sel>=0&&prop_sel<g_props.count&&g_props.list[prop_sel].active){
                Prop *sp=&g_props.list[prop_sel];
                bool edit_custom_box_preview=prop_custom_box_gizmo_mode&&prop_has_custom_collision(sp)&&
                                             sp->custom_coll_selected>=0&&sp->custom_coll_selected<sp->custom_coll_count;
                if(edit_custom_box_preview){
                    SurfacePiece box_proxy;
                    prop_custom_collision_proxy_from_prop(sp,&sp->custom_coll[sp->custom_coll_selected],&box_proxy);
                    prop_gizmo_hot_axis=prop_gizmo_active_axis>0?prop_gizmo_active_axis:
                        surface_gizmo_pick_axis(&box_proxy,emx,emy,win_w,win_h,proj,view,cam.pos,selection_gizmo_space!=0);
                    if(prop_mouse_mode>0){
                        surface_gizmo_draw(&box_proxy,cam.pos,selection_gizmo_space!=0,prop_gizmo_hot_axis,prop_gizmo_active_axis);
                        lr_flush(&lr,proj,view);
                    }
                }else{
                    prop_gizmo_hot_axis=prop_gizmo_active_axis>0?prop_gizmo_active_axis:prop_gizmo_pick_axis(sp,emx,emy,win_w,win_h,proj,view,cam.pos,selection_gizmo_space!=0);
                    if(prop_mouse_mode>0){
                        prop_gizmo_draw(sp,cam.pos,selection_gizmo_space!=0,prop_gizmo_hot_axis,prop_gizmo_active_axis,proj,view);
                        lr_flush(&lr,proj,view);
                    }
                }
            }
            if(lmb_now&&!prop_edit_lmb_prev&&!over_transform_panel&&hover_prop>=0){
                if(SDL_GetModState()&KMOD_SHIFT){
                    /* Shift+click: toggle prop in multi-selection */
                    prop_multi_toggle(hover_prop);
                    if(g_prop_multi_sel[hover_prop]){
                        prop_sel=hover_prop; g_placer.selected_prop=hover_prop;
                        editor_selection_set(EDITOR_SELECTION_PROP,hover_prop);
                        placer_sync_from_prop(&g_placer,&g_props.list[hover_prop]);
                    }
                } else if(hover_prop!=prop_sel){
                    prop_multi_clear(); prop_multi_add(hover_prop);
                    prop_sel=hover_prop; g_placer.selected_prop=hover_prop;
                    editor_selection_set(EDITOR_SELECTION_PROP,hover_prop);
                    prop_mouse_dragging=false; prop_gizmo_active_axis=0;
                    placer_sync_from_prop(&g_placer,&g_props.list[hover_prop]);
                    printf("[PropEdit] Selected prop %d: %s\n",hover_prop,g_props.list[hover_prop].path);
                }
            }
            if(g_props.count>0&&prop_sel>=0&&prop_sel<g_props.count&&g_props.list[prop_sel].active&&prop_mouse_mode>0&&!over_transform_panel){
                Prop *sp=&g_props.list[prop_sel];
                bool selected_prop_hit=false;
                bool edit_custom_box=prop_custom_box_gizmo_mode&&prop_has_custom_collision(sp)&&
                                     sp->custom_coll_selected>=0&&sp->custom_coll_selected<sp->custom_coll_count;
                float sel_x=0.0f,sel_y=0.0f,sel_w=0.0f,sel_h=0.0f;
                if(prop_selection_billboard_rect(sp,win_w,win_h,proj,view,&sel_x,&sel_y,&sel_w,&sel_h)){
                    selected_prop_hit=(float)emx>=sel_x&&(float)emx<=sel_x+sel_w&&
                                      (float)emy>=sel_y&&(float)emy<=sel_y+sel_h;
                }
                if(edit_custom_box){
                    int selected_box=sp->custom_coll_selected;
                    SurfacePiece box_proxy;
                    int box_hot_axis;
                    bool can_begin_drag;
                    static bool prop_box_dragging=false;
                    static bool prop_box_drag_dirty=false;
                    static PropSnap prop_box_drag_snap_before;
                    prop_custom_collision_proxy_from_prop(sp,&sp->custom_coll[selected_box],&box_proxy);
                    box_hot_axis=(prop_gizmo_active_axis>0)?prop_gizmo_active_axis:
                        surface_gizmo_pick_axis(&box_proxy,emx,emy,win_w,win_h,proj,view,cam.pos,selection_gizmo_space!=0);
                    can_begin_drag=(prop_mouse_mode==1)?
                        (box_hot_axis>0||selected_prop_hit):
                        (prop_mouse_mode==2||prop_mouse_mode==3)?
                        (selected_prop_hit||box_hot_axis>0):false;
                    if(lmb_now&&!prop_edit_lmb_prev&&can_begin_drag){
                        prop_box_dragging=true;
                        prop_box_drag_dirty=false;
                        prop_box_drag_snap_before=undo_snap_prop(prop_sel);
                        prop_gizmo_active_axis=(box_hot_axis>0)?box_hot_axis:4;
                        prop_mouse_last_x=emx;
                        prop_mouse_last_y=emy;
                        prop_gizmo_drag_accum=0.0f;
                        prop_gizmo_start_pos=box_proxy.pos;
                        prop_gizmo_start_rot_y=box_proxy.yaw_deg;
                        prop_gizmo_start_rot_x=box_proxy.width;
                        prop_gizmo_start_rot_z=box_proxy.height;
                        prop_gizmo_start_scale=box_proxy.thickness;
                    }else if(prop_box_dragging&&prop_gizmo_active_axis>0&&lmb_raw){
                        Vec3 go,gx,gy,gz,ga;
                        float osx,osy,asx,asy,axis_dx,axis_dy,axis_len;
                        float delta_px=0.0f;
                        if(prop_mouse_mode==1&&prop_gizmo_active_axis==4){
                            if(prop_got_hit){
                                box_proxy.pos.x=prop_hit.x;
                                box_proxy.pos.z=prop_hit.z;
                                prop_box_drag_dirty=true;
                            }
                        }else{
                            surface_gizmo_axes(&box_proxy,cam.pos,selection_gizmo_space!=0,&go,&gx,&gy,&gz);
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
                                    delta_px=((float)(emx-prop_mouse_last_x))*axis_dx+((float)(emy-prop_mouse_last_y))*axis_dy;
                                    prop_gizmo_drag_accum+=delta_px;
                                }
                            }else{
                                prop_gizmo_drag_accum+=(float)(emx-prop_mouse_last_x);
                            }
                            if(prop_mouse_mode==1){
                                float move_delta=prop_gizmo_drag_accum*cam.dist*0.012f;
                                Vec3 x_axis,y_axis,z_axis;
                                if(selection_gizmo_snap) move_delta=editor_gizmo_snap_value(move_delta,0.5f);
                                box_proxy.pos=prop_gizmo_start_pos;
                                editor_gizmo_basis_yaw(selection_gizmo_space!=0,prop_gizmo_start_rot_y*(float)M_PI/180.0f,&x_axis,&y_axis,&z_axis);
                                if(prop_gizmo_active_axis==1) box_proxy.pos=vec3_add(box_proxy.pos,vec3_scale(x_axis,move_delta));
                                else if(prop_gizmo_active_axis==2) box_proxy.pos=vec3_add(box_proxy.pos,vec3_scale(y_axis,move_delta));
                                else box_proxy.pos=vec3_add(box_proxy.pos,vec3_scale(z_axis,move_delta));
                                prop_box_drag_dirty=true;
                            }else if(prop_mouse_mode==2){
                                box_proxy.yaw_deg=prop_gizmo_start_rot_y+prop_gizmo_drag_accum*0.8f;
                                if(selection_gizmo_snap) box_proxy.yaw_deg=editor_gizmo_snap_value(box_proxy.yaw_deg,15.0f);
                                prop_box_drag_dirty=true;
                            }else if(prop_mouse_mode==3){
                                box_proxy.width=clampf(prop_gizmo_start_rot_x,0.10f,128.0f);
                                box_proxy.height=clampf(prop_gizmo_start_rot_z,0.10f,128.0f);
                                box_proxy.thickness=clampf(prop_gizmo_start_scale,0.10f,128.0f);
                                if(prop_gizmo_active_axis==1) box_proxy.width=clampf(prop_gizmo_start_rot_x+prop_gizmo_drag_accum*cam.dist*0.012f,0.10f,128.0f);
                                else if(prop_gizmo_active_axis==2) box_proxy.height=clampf(prop_gizmo_start_rot_z+prop_gizmo_drag_accum*cam.dist*0.012f,0.10f,128.0f);
                                else if(prop_gizmo_active_axis==3) box_proxy.thickness=clampf(prop_gizmo_start_scale+prop_gizmo_drag_accum*cam.dist*0.012f,0.10f,128.0f);
                                else if(prop_gizmo_active_axis==4){
                                    float mul=1.0f+prop_gizmo_drag_accum*0.01f;
                                    if(mul<0.25f) mul=0.25f;
                                    box_proxy.width=clampf(prop_gizmo_start_rot_x*mul,0.10f,128.0f);
                                    box_proxy.height=clampf(prop_gizmo_start_rot_z*mul,0.10f,128.0f);
                                    box_proxy.thickness=clampf(prop_gizmo_start_scale*mul,0.10f,128.0f);
                                }
                                if(selection_gizmo_snap){
                                    box_proxy.width=clampf(editor_gizmo_snap_value(box_proxy.width,0.10f),0.10f,128.0f);
                                    box_proxy.height=clampf(editor_gizmo_snap_value(box_proxy.height,0.10f),0.10f,128.0f);
                                    box_proxy.thickness=clampf(editor_gizmo_snap_value(box_proxy.thickness,0.10f),0.10f,128.0f);
                                }
                                prop_box_drag_dirty=true;
                            }
                        }
                        prop_custom_collision_apply_proxy(sp,&sp->custom_coll[selected_box],&box_proxy);
                        prop_refresh_collision(sp);
                        prop_mouse_last_x=emx;
                        prop_mouse_last_y=emy;
                    }else if(prop_box_dragging&&!lmb_raw){
                        if(prop_box_drag_dirty){
                            undo_push_prop_xform(prop_sel,prop_box_drag_snap_before,undo_snap_prop(prop_sel));
                            editor_command_runtime_note_change(editor_commands_runtime());
                        }
                        prop_box_dragging=false;
                        prop_box_drag_dirty=false;
                        prop_gizmo_active_axis=0;
                    }
                }else{
                    bool can_begin_drag=(prop_mouse_mode==1)?(prop_gizmo_hot_axis>0||selected_prop_hit):
                                         (prop_mouse_mode==2||prop_mouse_mode==3)?(hover_prop==prop_sel||selected_prop_hit||prop_gizmo_hot_axis>0):
                                         false;
                    static PropSnap prop_edit_drag_snap_before;
                    if(lmb_now&&!prop_edit_lmb_prev&&can_begin_drag){
                        prop_mouse_dragging=true;
                        prop_edit_drag_snap_before=undo_snap_prop(prop_sel);
                        prop_multi_drag_begin();
                        prop_gizmo_active_axis=(prop_gizmo_hot_axis>0)?prop_gizmo_hot_axis:4;
                        prop_mouse_last_x=emx;
                        prop_mouse_last_y=emy;
                        prop_gizmo_drag_accum=0.0f;
                        prop_gizmo_start_pos=sp->pos;
                        prop_gizmo_start_rot_x=sp->rot_x;
                        prop_gizmo_start_rot_y=sp->rot_y;
                        prop_gizmo_start_rot_z=sp->rot_z;
                        prop_gizmo_start_scale=sp->scale;
                    }else if(prop_mouse_dragging&&prop_gizmo_active_axis>0&&lmb_raw){
                        Vec3 go,gx,gy,gz,ga;
                        float osx,osy,asx,asy,axis_dx,axis_dy,axis_len;
                        float delta_px=0.0f;
                        if(prop_mouse_mode==1&&prop_gizmo_active_axis==4){
                            if(prop_got_hit){
                                sp->pos.x=prop_hit.x;
                                sp->pos.z=prop_hit.z;
                                if(g_placer.snap_y) sp->pos.y=scene_ground_y(sp->pos.x,sp->pos.z);
                                else sp->pos.y=prop_gizmo_start_pos.y;
                                prop_refresh_collision(sp);
                                if(g_prop_sel_count>1) prop_multi_drag_apply(prop_sel,sp->pos,g_placer.snap_y);
                                placer_sync_from_prop(&g_placer,sp);
                            }
                        }else if(prop_mouse_mode==2&&prop_gizmo_active_axis==4){
                            int dx=emx-prop_mouse_last_x;
                            int dy=emy-prop_mouse_last_y;
                            sp->rot_x+=dy*0.25f;
                            sp->rot_y+=dx*0.6f;
                            prop_refresh_collision(sp);
                            placer_sync_from_prop(&g_placer,sp);
                        }else if(prop_mouse_mode==3&&prop_gizmo_active_axis==4){
                            delta_px=(float)(emx-prop_mouse_last_x);
                            prop_gizmo_drag_accum+=delta_px;
                            sp->scale=clampf(prop_gizmo_start_scale+prop_gizmo_drag_accum*0.01f,0.01f,20.0f);
                            if(selection_gizmo_snap) sp->scale=clampf(editor_gizmo_snap_value(sp->scale,0.1f),0.01f,20.0f);
                            if(g_placer.snap_y) sp->pos.y=scene_ground_y(sp->pos.x,sp->pos.z);
                            prop_refresh_collision(sp);
                            placer_sync_from_prop(&g_placer,sp);
                        }else if(prop_mouse_mode==1&&prop_gizmo_active_axis>=5&&prop_gizmo_active_axis<=7){
                            Vec3 ro,rd,pro,prd,cur_hit,prev_hit;
                            Vec3 ppt,pnorm;
                            if(prop_gizmo_active_axis==5){ppt=(Vec3){0,prop_gizmo_start_pos.y,0};pnorm=(Vec3){0,1,0};}
                            else if(prop_gizmo_active_axis==6){ppt=(Vec3){0,0,prop_gizmo_start_pos.z};pnorm=(Vec3){0,0,1};}
                            else{ppt=(Vec3){prop_gizmo_start_pos.x,0,0};pnorm=(Vec3){1,0,0};}
                            editor_screen_to_ray(emx,emy,win_w,win_h,proj,view,&ro,&rd);
                            editor_screen_to_ray(prop_mouse_last_x,prop_mouse_last_y,win_w,win_h,proj,view,&pro,&prd);
                            if(editor_ray_plane_intersect(ro,rd,ppt,pnorm,&cur_hit)&&editor_ray_plane_intersect(pro,prd,ppt,pnorm,&prev_hit)){
                                float ddx=cur_hit.x-prev_hit.x;
                                float ddy=cur_hit.y-prev_hit.y;
                                float ddz=cur_hit.z-prev_hit.z;
                                if(prop_gizmo_active_axis==5){sp->pos.x+=ddx;sp->pos.z+=ddz;}
                                else if(prop_gizmo_active_axis==6){sp->pos.x+=ddx;sp->pos.y+=ddy;}
                                else{sp->pos.y+=ddy;sp->pos.z+=ddz;}
                                if(g_placer.snap_y&&prop_gizmo_active_axis==5) sp->pos.y=scene_ground_y(sp->pos.x,sp->pos.z);
                                prop_refresh_collision(sp);
                                placer_sync_from_prop(&g_placer,sp);
                            }
                        }else{
                            prop_gizmo_axes(sp,cam.pos,selection_gizmo_space!=0,&go,&gx,&gy,&gz);
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
                                    delta_px=((float)(emx-prop_mouse_last_x))*axis_dx+((float)(emy-prop_mouse_last_y))*axis_dy;
                                    prop_gizmo_drag_accum+=delta_px;
                                    if(prop_mouse_mode==1){
                                        float move_delta=prop_gizmo_drag_accum*cam.dist*0.012f;
                                        Vec3 x_axis,y_axis,z_axis;
                                        if(selection_gizmo_snap) move_delta=editor_gizmo_snap_value(move_delta,0.5f);
                                        sp->pos=prop_gizmo_start_pos;
                                        editor_gizmo_basis_yaw(selection_gizmo_space!=0,prop_gizmo_start_rot_y*(float)M_PI/180.0f,&x_axis,&y_axis,&z_axis);
                                        if(prop_gizmo_active_axis==1) sp->pos=vec3_add(sp->pos,vec3_scale(x_axis,move_delta));
                                        else if(prop_gizmo_active_axis==2) sp->pos=vec3_add(sp->pos,vec3_scale(y_axis,move_delta));
                                        else sp->pos=vec3_add(sp->pos,vec3_scale(z_axis,move_delta));
                                        if(g_placer.snap_y&&prop_gizmo_active_axis!=2) sp->pos.y=scene_ground_y(sp->pos.x,sp->pos.z);
                                    }else if(prop_mouse_mode==2){
                                        float rot_delta=prop_gizmo_drag_accum*0.8f;
                                        if(selection_gizmo_snap) rot_delta=editor_gizmo_snap_value(rot_delta,15.0f);
                                        sp->rot_x=prop_gizmo_start_rot_x;
                                        sp->rot_y=prop_gizmo_start_rot_y;
                                        sp->rot_z=prop_gizmo_start_rot_z;
                                        if(prop_gizmo_active_axis==1) sp->rot_x+=rot_delta;
                                        else if(prop_gizmo_active_axis==2) sp->rot_y+=rot_delta;
                                        else sp->rot_z+=rot_delta;
                                    }
                                    prop_refresh_collision(sp);
                                    placer_sync_from_prop(&g_placer,sp);
                                }
                            }
                        }
                        prop_mouse_last_x=emx;
                        prop_mouse_last_y=emy;
                    }else if(!lmb_raw){
                        if(prop_mouse_dragging){
                            editor_command_prop_commit_xform(prop_sel,prop_edit_drag_snap_before,&g_placer,NULL);
                        }
                        prop_mouse_dragging=false;
                        prop_gizmo_active_axis=0;
                    }
                }
            }
            if(!over_transform_panel&&prop_mouse_mode==0){
                if(lmb_now&&!prop_edit_lmb_prev){
                    if(hover_prop>=0){
                        if(SDL_GetModState()&KMOD_SHIFT){
                            prop_multi_toggle(hover_prop);
                            if(g_prop_multi_sel[hover_prop]){
                                prop_sel=hover_prop; g_placer.selected_prop=hover_prop;
                                editor_selection_set(EDITOR_SELECTION_PROP,hover_prop);
                                placer_sync_from_prop(&g_placer,&g_props.list[hover_prop]);
                            }
                        }else{
                            prop_multi_clear(); prop_multi_add(hover_prop);
                            prop_sel=hover_prop; g_placer.selected_prop=hover_prop;
                            editor_selection_set(EDITOR_SELECTION_PROP,hover_prop);
                            prop_gizmo_active_axis=0;
                            placer_sync_from_prop(&g_placer,&g_props.list[hover_prop]);
                            printf("[PropEdit] Selected prop %d: %s\n",hover_prop,g_props.list[hover_prop].path);
                        }
                        prop_box_sel=false;
                    }else{
                        prop_box_sel=true;
                        prop_box_x0=prop_box_x1=emx;
                        prop_box_y0=prop_box_y1=emy;
                        if(!(SDL_GetModState()&KMOD_SHIFT)) prop_multi_clear();
                    }
                }
                if(prop_box_sel){
                    prop_box_x1=emx;
                    prop_box_y1=emy;
                    if(!lmb_raw){
                        bool shift=(SDL_GetModState()&KMOD_SHIFT)!=0;
                        prop_multi_box_select(prop_box_x0,prop_box_y0,prop_box_x1,prop_box_y1,proj,view,win_w,win_h,shift);
                        if(g_prop_sel_count>0){
                            for(int _bi=0;_bi<g_props.count;_bi++){
                                if(g_prop_multi_sel[_bi]&&g_props.list[_bi].active){
                                    prop_sel=_bi;
                                    g_placer.selected_prop=_bi;
                                    editor_selection_set(EDITOR_SELECTION_PROP,_bi);
                                    placer_sync_from_prop(&g_placer,&g_props.list[_bi]);
                                    break;
                                }
                            }
                        }
                        prop_box_sel=false;
                    }else{
                        prop_box_sel_draw(&hud,prop_box_x0,prop_box_y0,prop_box_x1,prop_box_y1);
                    }
                }
            }
            /* Draw hover highlight */
            if(hover_prop>=0&&hover_prop!=prop_sel&&prop_mouse_mode==0&&!g_prop_multi_sel[hover_prop]){
                props_hover_draw(&g_props,hover_prop);
                prop_selection_billboard_draw(&hud,&g_props.list[hover_prop],win_w,win_h,proj,view,false,true);
            }
            /* Draw all multi-selected props (non-primary) */
            prop_multi_draw_highlights(&hud,prop_sel,win_w,win_h,proj,view);
            /* Multi-select count HUD */
            if(g_prop_sel_count>1){
                char _msbuf[48]; snprintf(_msbuf,sizeof(_msbuf),"%d props selected  Ctrl+D dup  Del delete",g_prop_sel_count);
                hud_str(&hud,8.0f,(float)win_h-38.0f,0.92f,_msbuf,0.3f,0.9f,1.0f,0.95f);
            }
            prop_edit_lmb_prev=lmb_now;
            if(!lmb_raw){ prop_mouse_dragging=false; prop_gizmo_active_axis=0; }
        }

        if(forest_edit){
            int fmx,fmy;
            Uint32 fmb=SDL_GetMouseState(&fmx,&fmy);
            bool lmb_now=((fmb&SDL_BUTTON(1))!=0)&&!editor_gui_mouse_blocked;
            bool rmb_now=((fmb&SDL_BUTTON(3))!=0)&&!editor_gui_mouse_blocked;
            Vec3 hit; bool got_hit=false;
            got_hit=river_screen_to_world(fmx,fmy,win_w,win_h,proj,view,&hit);
            if(got_hit) hit.y=terrain_h(hit.x,hit.z);
            if(forest_brush_kind==1){
                g_grass.size_mul=grass_brush_size;
                g_grass.style.h_min=0.28f*grass_brush_hmin;
                g_grass.style.h_max=0.74f*grass_brush_hmax;
                if(g_grass.style.h_max<g_grass.style.h_min) g_grass.style.h_max=g_grass.style.h_min;
                g_grass.density_mul=1.0f+0.077f*(float)(grass_brush_density-1);
                g_grass.push_radius=1.15f;
                g_grass.push_strength=0.55f;
                const Uint8 *gks=SDL_GetKeyboardState(NULL);
                bool grass_save=gks[SDL_SCANCODE_S]!=0;
                bool grass_undo=gks[SDL_SCANCODE_Z]!=0;
                if(grass_save&&!grass_brush_s_prev){
                    grass_mask_save();
                    g_grass.last_center=(Vec3){1e9f,0,1e9f};
                    printf("[GrassBrush] Saved plains grass mask\n");
                }
                if(((grass_undo&&!grass_brush_z_prev)||g_editor_ui_request_forest_undo)&&grass_mask_has_undo){
                    memcpy(g_grass_mask.d,grass_mask_undo,sizeof(g_grass_mask.d));
                    memcpy(g_grass_style_mask.d,grass_style_undo,sizeof(g_grass_style_mask.d));
                    memcpy(g_grass_tint_mask.d,grass_tint_undo,sizeof(g_grass_tint_mask.d));
                    g_grass_mask_active=true;
                    g_grass_mask_seeded=true;
                    grass_mask_has_undo=false;
                    g_grass.last_center=(Vec3){1e9f,0,1e9f};
                    printf("[GrassBrush] Undo applied\n");
                }
                if(g_editor_ui_request_forest_undo) g_editor_ui_request_forest_undo=false;
                grass_brush_s_prev=grass_save;
                grass_brush_z_prev=grass_undo;
                bool brush_now=lmb_now||rmb_now;
                if(brush_now&&got_hit){
                    if(!forest_lmb_prev){
                        if(!g_grass_mask_seeded) grass_mask_seed_from_terrain();
                        memcpy(grass_mask_undo,g_grass_mask.d,sizeof(g_grass_mask.d));
                        memcpy(grass_style_undo,g_grass_style_mask.d,sizeof(g_grass_style_mask.d));
                        memcpy(grass_tint_undo,g_grass_tint_mask.d,sizeof(g_grass_tint_mask.d));
                        grass_mask_has_undo=true;
                    }
                    {
                        float strength=clampf(0.010f*(float)grass_brush_density,0.012f,0.95f);
                        grass_mask_brush(hit.x,hit.z,grass_brush_radius,strength,rmb_now,g_grass.author_style,g_grass.author_tint);
                        g_grass.last_center=(Vec3){1e9f,0,1e9f};
                    }
                }
                forest_lmb_prev=brush_now;
                if(got_hit){
                    int segs=32;
                    for(int si=0;si<segs;si++){
                        float a0=(float)si/(float)segs*(float)(2*M_PI);
                        float a1=(float)(si+1)/(float)segs*(float)(2*M_PI);
                        float x0=hit.x+cosf(a0)*grass_brush_radius;
                        float z0=hit.z+sinf(a0)*grass_brush_radius;
                        float x1=hit.x+cosf(a1)*grass_brush_radius;
                        float z1=hit.z+sinf(a1)*grass_brush_radius;
                        Vec3 pa={x0,terrain_h(x0,z0)+0.18f,z0};
                        Vec3 pb={x1,terrain_h(x1,z1)+0.18f,z1};
                        lr_push(pa,pb,0.45f,0.95f,0.35f);
                    }
                }
                lr_flush(&lr,proj,view);
                if(!editor_ui_backend_is_imgui(&g_editor_ui))
                    grass_brush_hud(&hud,g_grass.author_style,g_grass.author_tint,grass_brush_radius,grass_brush_density,grass_brush_size,
                                    grass_brush_hmin,grass_brush_hmax,win_w,win_h);
            }else if(forest_brush_kind==2){
                g_grass.flower_size_mul=flower_brush_size;
                const Uint8 *gks=SDL_GetKeyboardState(NULL);
                bool flower_save=gks[SDL_SCANCODE_S]!=0;
                bool flower_undo=gks[SDL_SCANCODE_Z]!=0;
                if(flower_save&&!grass_brush_s_prev){
                    flower_mask_save();
                    g_grass.last_center=(Vec3){1e9f,0,1e9f};
                    printf("[FlowerBrush] Saved flower masks\n");
                }
                if(((flower_undo&&!grass_brush_z_prev)||g_editor_ui_request_forest_undo)&&flower_mask_has_undo){
                    memcpy(g_flower_mask.d,flower_mask_undo,sizeof(g_flower_mask.d));
                    memcpy(g_flower_type_mask.d,flower_type_undo,sizeof(g_flower_type_mask.d));
                    g_flower_mask_active=true;
                    g_flower_mask_seeded=true;
                    flower_mask_has_undo=false;
                    g_grass.last_center=(Vec3){1e9f,0,1e9f};
                    printf("[FlowerBrush] Undo applied\n");
                }
                if(g_editor_ui_request_forest_undo) g_editor_ui_request_forest_undo=false;
                grass_brush_s_prev=flower_save;
                grass_brush_z_prev=flower_undo;
                bool brush_now=lmb_now||rmb_now;
                if(brush_now&&got_hit){
                    if(!forest_lmb_prev){
                        if(!g_flower_mask_seeded) grass_mask_seed_from_terrain();
                        memcpy(flower_mask_undo,g_flower_mask.d,sizeof(g_flower_mask.d));
                        memcpy(flower_type_undo,g_flower_type_mask.d,sizeof(g_flower_type_mask.d));
                        flower_mask_has_undo=true;
                    }
                    {
                        float strength=clampf(0.010f*(float)flower_brush_density,0.012f,0.95f);
                        flower_mask_brush(hit.x,hit.z,grass_brush_radius,strength,rmb_now,g_grass.author_flower_type);
                        g_grass.last_center=(Vec3){1e9f,0,1e9f};
                    }
                }
                forest_lmb_prev=brush_now;
                if(got_hit){
                    int segs=32;
                    for(int si=0;si<segs;si++){
                        float a0=(float)si/(float)segs*(float)(2*M_PI);
                        float a1=(float)(si+1)/(float)segs*(float)(2*M_PI);
                        float x0=hit.x+cosf(a0)*grass_brush_radius;
                        float z0=hit.z+sinf(a0)*grass_brush_radius;
                        float x1=hit.x+cosf(a1)*grass_brush_radius;
                        float z1=hit.z+sinf(a1)*grass_brush_radius;
                        Vec3 pa={x0,terrain_h(x0,z0)+0.18f,z0};
                        Vec3 pb={x1,terrain_h(x1,z1)+0.18f,z1};
                        lr_push(pa,pb,0.95f,0.86f,0.35f);
                    }
                }
                lr_flush(&lr,proj,view);
                if(!editor_ui_backend_is_imgui(&g_editor_ui))
                    flower_brush_hud(&hud,g_grass.author_flower_type,grass_brush_radius,flower_brush_density,
                                     flower_brush_size,win_w,win_h);
            }else if(forest_brush_kind==3){
                const char *boulder_source=placer_selected_model(&g_placer);
                bool boulder_ready=placer_is_boulder_source(boulder_source);
                grass_brush_s_prev=false;
                grass_brush_z_prev=false;
                forest_lmb_prev=false;
                if(lmb_now&&got_hit&&boulder_ready) boulder_brush_paint(&g_props,hit,forest_brush_radius,forest_brush_density);
                if(rmb_now&&got_hit&&boulder_ready) boulder_brush_erase(&g_props,hit,forest_brush_radius);
                if(got_hit){
                    int segs=32;
                    for(int si=0;si<segs;si++){
                        float a0=(float)si/(float)segs*(float)(2*M_PI);
                        float a1=(float)(si+1)/(float)segs*(float)(2*M_PI);
                        float x0=hit.x+cosf(a0)*forest_brush_radius;
                        float z0=hit.z+sinf(a0)*forest_brush_radius;
                        float x1=hit.x+cosf(a1)*forest_brush_radius;
                        float z1=hit.z+sinf(a1)*forest_brush_radius;
                        Vec3 pa={x0,terrain_h(x0,z0)+0.2f,z0};
                        Vec3 pb={x1,terrain_h(x1,z1)+0.2f,z1};
                        lr_push(pa,pb,0.70f,0.78f,0.86f);
                    }
                }
                lr_flush(&lr,proj,view);
                if(!editor_ui_backend_is_imgui(&g_editor_ui))
                    boulder_brush_hud(&hud,boulder_source,forest_brush_radius,forest_brush_density,win_w,win_h);
            }else if(forest_brush_kind==4){
                const char *wall_source=placer_selected_model(&g_placer);
                bool wall_ready=(wall_source&&wall_source[0]);
                grass_brush_s_prev=false;
                grass_brush_z_prev=false;
                if(rmb_now&&wall_brush_has_start){
                    wall_brush_has_start=false;
                }else{
                    if(lmb_now&&!forest_lmb_prev&&got_hit&&wall_ready){
                        if(!wall_brush_has_start){
                            wall_brush_start=hit;
                            wall_brush_has_start=true;
                            printf("[WallBrush] Start set at %.1f, %.1f\n",(double)hit.x,(double)hit.z);
                        }else{
                            wall_brush_build(&g_props,wall_brush_start,hit,wall_brush_spacing,wall_brush_scale);
                            wall_brush_has_start=false;
                        }
                    }
                    if(rmb_now&&got_hit&&wall_ready) wall_brush_erase(&g_props,hit,forest_brush_radius);
                }
                forest_lmb_prev=lmb_now;
                if(got_hit){
                    int segs=32;
                    for(int si=0;si<segs;si++){
                        float a0=(float)si/(float)segs*(float)(2*M_PI);
                        float a1=(float)(si+1)/(float)segs*(float)(2*M_PI);
                        float x0=hit.x+cosf(a0)*forest_brush_radius;
                        float z0=hit.z+sinf(a0)*forest_brush_radius;
                        float x1=hit.x+cosf(a1)*forest_brush_radius;
                        float z1=hit.z+sinf(a1)*forest_brush_radius;
                        Vec3 pa={x0,terrain_h(x0,z0)+0.2f,z0};
                        Vec3 pb={x1,terrain_h(x1,z1)+0.2f,z1};
                        lr_push(pa,pb,0.84f,0.72f,0.42f);
                    }
                    if(wall_brush_has_start){
                        Vec3 sa={wall_brush_start.x,terrain_h(wall_brush_start.x,wall_brush_start.z)+0.35f,wall_brush_start.z};
                        Vec3 sb={hit.x,terrain_h(hit.x,hit.z)+0.35f,hit.z};
                        lr_push(sa,sb,0.95f,0.88f,0.52f);
                    }
                }
                lr_flush(&lr,proj,view);
                if(!editor_ui_backend_is_imgui(&g_editor_ui))
                    wall_brush_hud(&hud,wall_source,wall_brush_has_start,wall_brush_start,forest_brush_radius,wall_brush_spacing,wall_brush_scale,win_w,win_h);
            }else if(forest_brush_kind==5){
                const Uint8 *tks=SDL_GetKeyboardState(NULL);
                bool terrain_save=tks[SDL_SCANCODE_S]!=0;
                bool terrain_undo=tks[SDL_SCANCODE_Z]!=0;
                if(terrain_save&&!grass_brush_s_prev){
                    worn_mask_save();
                    terrain_style_mask_save();
                    printf("[TerrainBrush] Saved terrain wear mask\n");
                }
                if(((terrain_undo&&!grass_brush_z_prev)||g_editor_ui_request_forest_undo)&&worn_mask_has_undo){
                    memcpy(g_worn_mask.d,worn_mask_undo,sizeof(g_worn_mask.d));
                    memcpy(g_terrain_style_mask.d,terrain_style_undo,sizeof(g_terrain_style_mask.d));
                    g_worn_mask_active=true;
                    worn_mask_has_undo=false;
                    worn_mask_upload(&terrain);
                    terrain_style_mask_upload(&terrain);
                    printf("[TerrainBrush] Undo applied\n");
                }
                if(g_editor_ui_request_forest_undo) g_editor_ui_request_forest_undo=false;
                grass_brush_s_prev=terrain_save;
                grass_brush_z_prev=terrain_undo;
                {
                    bool brush_now=lmb_now||rmb_now;
                    if(brush_now&&got_hit){
                        if(!forest_lmb_prev){
                            memcpy(worn_mask_undo,g_worn_mask.d,sizeof(g_worn_mask.d));
                            memcpy(terrain_style_undo,g_terrain_style_mask.d,sizeof(g_terrain_style_mask.d));
                            worn_mask_has_undo=true;
                        }
                        worn_mask_brush(&terrain,hit.x,hit.z,forest_brush_radius,terrain_brush_strength,rmb_now,(TerrainPaintStyle)terrain_brush_style);
                    }
                    forest_lmb_prev=brush_now;
                }
                if(got_hit){
                    int segs=32;
                    for(int si=0;si<segs;si++){
                        float a0=(float)si/(float)segs*(float)(2*M_PI);
                        float a1=(float)(si+1)/(float)segs*(float)(2*M_PI);
                        float x0=hit.x+cosf(a0)*forest_brush_radius;
                        float z0=hit.z+sinf(a0)*forest_brush_radius;
                        float x1=hit.x+cosf(a1)*forest_brush_radius;
                        float z1=hit.z+sinf(a1)*forest_brush_radius;
                        Vec3 pa={x0,terrain_h(x0,z0)+0.18f,z0};
                        Vec3 pb={x1,terrain_h(x1,z1)+0.18f,z1};
                        lr_push(pa,pb,0.74f,0.62f,0.36f);
                    }
                }
                lr_flush(&lr,proj,view);
                if(!editor_ui_backend_is_imgui(&g_editor_ui))
                    terrain_brush_hud(&hud,forest_brush_radius,terrain_brush_strength,(TerrainPaintStyle)terrain_brush_style,win_w,win_h);
            }else if(forest_brush_kind==6){
                const Uint8 *fks=SDL_GetKeyboardState(NULL);
                bool farm_save=fks[SDL_SCANCODE_S]!=0;
                bool farm_undo=fks[SDL_SCANCODE_Z]!=0;
                if(farm_save&&!grass_brush_s_prev){
                    farm_field_mask_save();
                    printf("[FarmBrush] Saved farm field mask\n");
                }
                if(((farm_undo&&!grass_brush_z_prev)||g_editor_ui_request_forest_undo)&&farm_mask_has_undo){
                    memcpy(g_farm_field_mask.d,farm_mask_undo,sizeof(g_farm_field_mask.d));
                    g_farm_field_mask_active=true;
                    farm_mask_has_undo=false;
                    farming_rebuild_from_mask(&g_farming);
                    printf("[FarmBrush] Undo applied\n");
                }
                if(g_editor_ui_request_forest_undo) g_editor_ui_request_forest_undo=false;
                grass_brush_s_prev=farm_save;
                grass_brush_z_prev=farm_undo;
                {
                    bool brush_now=lmb_now||rmb_now;
                    if(brush_now&&got_hit){
                        if(!forest_lmb_prev){
                            memcpy(farm_mask_undo,g_farm_field_mask.d,sizeof(g_farm_field_mask.d));
                            farm_mask_has_undo=true;
                        }
                        farm_field_mask_brush(hit.x,hit.z,forest_brush_radius,terrain_brush_strength,rmb_now);
                        farming_rebuild_from_mask(&g_farming);
                    }
                    forest_lmb_prev=brush_now;
                }
                if(got_hit){
                    int segs=32;
                    for(int si=0;si<segs;si++){
                        float a0=(float)si/(float)segs*(float)(2*M_PI);
                        float a1=(float)(si+1)/(float)segs*(float)(2*M_PI);
                        float x0=hit.x+cosf(a0)*forest_brush_radius;
                        float z0=hit.z+sinf(a0)*forest_brush_radius;
                        float x1=hit.x+cosf(a1)*forest_brush_radius;
                        float z1=hit.z+sinf(a1)*forest_brush_radius;
                        Vec3 pa={x0,terrain_h(x0,z0)+0.18f,z0};
                        Vec3 pb={x1,terrain_h(x1,z1)+0.18f,z1};
                        lr_push(pa,pb,0.72f,0.88f,0.36f);
                    }
                }
                lr_flush(&lr,proj,view);
                if(!editor_ui_backend_is_imgui(&g_editor_ui))
                    farm_brush_hud(&hud,forest_brush_radius,terrain_brush_strength,g_farming.count,win_w,win_h);
            }else{
                grass_brush_s_prev=false;
                grass_brush_z_prev=false;
                int hov_idx=-1;
                if(got_hit){
                    float best=8.0f*8.0f;
                    for(int i=0;i<g_forest.count;i++){
                        ForestTree *t=&g_forest.trees[i];
                        float dx=t->pos.x-hit.x,dz=t->pos.z-hit.z;
                        float d2=dx*dx+dz*dz;
                        if(d2<best){best=d2;hov_idx=i;}
                    }
                }
                forest_lmb_prev=false;
                if(lmb_now&&got_hit) forest_brush_paint(&g_forest,hit,forest_brush_radius,forest_brush_density,tree_brush_scale,tree_brush_scale);
                if(rmb_now&&got_hit) forest_brush_erase(&g_forest,&g_props,hit,forest_brush_radius);
                if(got_hit){
                    int segs=32;
                    for(int si=0;si<segs;si++){
                        float a0=(float)si/(float)segs*(float)(2*M_PI);
                        float a1=(float)(si+1)/(float)segs*(float)(2*M_PI);
                        float x0=hit.x+cosf(a0)*forest_brush_radius;
                        float z0=hit.z+sinf(a0)*forest_brush_radius;
                        float x1=hit.x+cosf(a1)*forest_brush_radius;
                        float z1=hit.z+sinf(a1)*forest_brush_radius;
                        Vec3 pa={x0,terrain_h(x0,z0)+0.2f,z0};
                        Vec3 pb={x1,terrain_h(x1,z1)+0.2f,z1};
                        lr_push(pa,pb,0.25f,0.95f,0.45f);
                    }
                }
                if(hov_idx>=0){
                    ForestTree *t=&g_forest.trees[hov_idx];
                    float h=t->scale*6.0f;
                    float r=t->scale*1.2f;
                    lr_push((Vec3){t->pos.x-r,t->pos.y,t->pos.z-r},(Vec3){t->pos.x+r,t->pos.y,t->pos.z-r}, 0.8f,1.0f,0.3f);
                    lr_push((Vec3){t->pos.x+r,t->pos.y,t->pos.z-r},(Vec3){t->pos.x+r,t->pos.y,t->pos.z+r}, 0.8f,1.0f,0.3f);
                    lr_push((Vec3){t->pos.x+r,t->pos.y,t->pos.z+r},(Vec3){t->pos.x-r,t->pos.y,t->pos.z+r}, 0.8f,1.0f,0.3f);
                    lr_push((Vec3){t->pos.x-r,t->pos.y,t->pos.z+r},(Vec3){t->pos.x-r,t->pos.y,t->pos.z-r}, 0.8f,1.0f,0.3f);
                    lr_push((Vec3){t->pos.x,t->pos.y,t->pos.z},(Vec3){t->pos.x,t->pos.y+h,t->pos.z}, 0.8f,1.0f,0.3f);
                }
                lr_flush(&lr,proj,view);
            }
        }

        if(spawn_edit_mode){
            int smx,smy;
            Uint32 smb=SDL_GetMouseState(&smx,&smy);
            bool lmb_now=((smb&SDL_BUTTON(1))!=0)&&!editor_gui_mouse_blocked;
            bool rmb_now=((smb&SDL_BUTTON(3))!=0)&&!editor_gui_mouse_blocked;
            Vec3 hit; bool got_hit=false;
            got_hit=river_screen_to_world(smx,smy,win_w,win_h,proj,view,&hit);
            if(got_hit) hit.y=terrain_h(hit.x,hit.z);
            int hov_idx=-1;
            if(got_hit){
                float best=6.0f*6.0f;
                for(int i=0;i<g_spawns.count;i++){
                    SpawnPoint *sp=&g_spawns.points[i];
                    if(!sp->active)continue;
                    float dx=sp->pos.x-hit.x,dz=sp->pos.z-hit.z;
                    float d2=dx*dx+dz*dz;
                    if(d2<best){best=d2;hov_idx=i;}
                }
            }
            if(lmb_now&&!spawn_lmb_prev&&got_hit){
                if(hov_idx>=0){
                    spawn_sel_idx=hov_idx;
                    editor_selection_set(EDITOR_SELECTION_SPAWN,hov_idx);
                } else if(g_spawns.count<MAX_SPAWN_POINTS){
                    int placed_idx=-1;
                    if(editor_command_spawn_create((int)spawn_sel_type,hit,
                                                  (g_enemy_model_count>0)?spawn_sel_model:0,
                                                  -1.0f,-1.0f,-1.0f,-1.0f,-1.0f,-1.0f,-1.0f,
                                                  &placed_idx,NULL)){
                        SpawnPoint *sp=&g_spawns.points[placed_idx];
                        spawn_sel_idx=placed_idx;
                        editor_selection_set(EDITOR_SELECTION_SPAWN,spawn_sel_idx);
                        for(int fi=0;fi<MAX_ENEMIES;fi++){
                            if(!enemies[fi].active||enemies[fi].state==ES_DEAD){
                                enemy_spawn(&enemies[fi],spawn_sel_type,hit,&enemy_amaps[spawn_sel_type]);
                                enemies[fi].model_idx=sp->model_idx;
                                if(fi>=ne) ne=fi+1;
                                break;
                            }
                        }
                        printf("[SpawnEd] Placed %s at (%.1f,%.1f,%.1f) — auto-saved\n",
                               EDEF[spawn_sel_type].name,(double)hit.x,(double)hit.y,(double)hit.z);
                    }
                }
            }
            if(rmb_now&&!spawn_rmb_prev&&hov_idx>=0){
                printf("[SpawnEd] Deleted spawn %d (%s)\n",
                       hov_idx,EDEF[g_spawns.points[hov_idx].etype].name);
                editor_command_spawn_delete(hov_idx,NULL);
                if(spawn_sel_idx>hov_idx) spawn_sel_idx--;
                else if(spawn_sel_idx==hov_idx) spawn_sel_idx=(hov_idx<g_spawns.count)?hov_idx:g_spawns.count-1;
            }
            if(g_editor_selection.kind==EDITOR_SELECTION_SPAWN&&
               g_editor_selection.index>=0&&g_editor_selection.index<g_spawns.count&&
               g_spawns.points[g_editor_selection.index].active){
                SpawnPoint *sp=&g_spawns.points[g_editor_selection.index];
                Vec3 spawn_origin={sp->pos.x,sp->pos.y+1.25f,sp->pos.z};
                float spawn_len=clampf(vec3_dist(spawn_origin,cam.pos)*0.10f,3.5f,20.0f);
                int spawn_hot_axis=(prop_gizmo_active_axis>0)?prop_gizmo_active_axis:
                    editor_gizmo_pick_axis(spawn_origin,spawn_len,false,0.0f,smx,smy,win_w,win_h,proj,view);
                bool selected_spawn_hit=(hov_idx==g_editor_selection.index);
                static bool spawn_drag_active=false;
                static bool spawn_drag_dirty=false;
                static Vec3 spawn_drag_start_pos;
                static float spawn_drag_start_scale;
                if(prop_mouse_mode>0){
                    editor_gizmo_draw(spawn_origin,spawn_len,false,0.0f,spawn_hot_axis,prop_gizmo_active_axis);
                    lr_flush(&lr,proj,view);
                }
                if(lmb_now&&!spawn_lmb_prev&&
                   ((prop_mouse_mode==1&&(selected_spawn_hit||spawn_hot_axis>0))||
                    (prop_mouse_mode==3&&(selected_spawn_hit||spawn_hot_axis>0)))){
                    spawn_drag_active=true;
                    spawn_drag_dirty=false;
                    prop_gizmo_active_axis=(spawn_hot_axis>0)?spawn_hot_axis:4;
                    prop_mouse_last_x=smx;
                    prop_mouse_last_y=smy;
                    prop_gizmo_drag_accum=0.0f;
                    spawn_drag_start_pos=sp->pos;
                    spawn_drag_start_scale=(sp->scale_over>0.0f)?sp->scale_over:1.0f;
                }else if(spawn_drag_active&&prop_gizmo_active_axis>0&&lmb_now){
                    if(prop_mouse_mode==1){
                        Vec3 go,gx,gy,gz,ga;
                        float osx,osy,asx,asy,axis_dx,axis_dy,axis_len,delta_px;
                        editor_gizmo_axes(spawn_origin,spawn_len,false,0.0f,&gx,&gy,&gz);
                        go=spawn_origin;
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
                            if(selection_gizmo_snap) move_delta=editor_gizmo_snap_value(move_delta,0.5f);
                            sp->pos=spawn_drag_start_pos;
                            if(prop_gizmo_active_axis==1) sp->pos.x+=move_delta;
                            else if(prop_gizmo_active_axis==2) sp->pos.y+=move_delta;
                            else sp->pos.z+=move_delta;
                        }
                        spawn_drag_dirty=true;
                    }else if(prop_mouse_mode==3){
                        float delta_px=(float)(smx-prop_mouse_last_x);
                        prop_gizmo_drag_accum+=delta_px;
                        sp->scale_over=clampf(spawn_drag_start_scale+prop_gizmo_drag_accum*0.01f,0.10f,5.0f);
                        if(selection_gizmo_snap) sp->scale_over=clampf(editor_gizmo_snap_value(sp->scale_over,0.1f),0.10f,5.0f);
                        spawn_drag_dirty=true;
                    }
                    prop_mouse_last_x=smx;
                    prop_mouse_last_y=smy;
                }else if(spawn_drag_active&&!lmb_now){
                    Vec3 final_pos=sp->pos;
                    float final_scale=sp->scale_over;
                    sp->pos=spawn_drag_start_pos;
                    sp->scale_over=spawn_drag_start_scale;
                    if(spawn_drag_dirty){
                        if(fabsf(final_pos.x-spawn_drag_start_pos.x)>0.0001f||
                           fabsf(final_pos.y-spawn_drag_start_pos.y)>0.0001f||
                           fabsf(final_pos.z-spawn_drag_start_pos.z)>0.0001f)
                            editor_command_spawn_set_position(g_editor_selection.index,final_pos,NULL);
                        else if(fabsf(final_scale-spawn_drag_start_scale)>0.0001f)
                            editor_command_spawn_set_scale_override(g_editor_selection.index,final_scale,NULL);
                    }
                    spawn_drag_active=false;
                    spawn_drag_dirty=false;
                    prop_gizmo_active_axis=0;
                }
                if(!lmb_now&&!spawn_drag_active&&prop_gizmo_active_axis>0) prop_gizmo_active_axis=0;
            }
            spawn_lmb_prev=lmb_now;
            spawn_rmb_prev=rmb_now;
            spawned_draw_markers(&g_spawns,&lr,&hud,spawn_sel_type,hov_idx,proj,view,win_w,win_h);
            if(!editor_ui_backend_is_imgui(&g_editor_ui))
                spawned_hud(&hud,&g_spawns,spawn_sel_type,hov_idx,win_w,win_h);
        }

        if(mount_edit_mode){
            int mmx,mmy;
            Uint32 mmb=SDL_GetMouseState(&mmx,&mmy);
            bool lmb_now=((mmb&SDL_BUTTON(1))!=0)&&!editor_gui_mouse_blocked;
            bool rmb_now=((mmb&SDL_BUTTON(3))!=0)&&!editor_gui_mouse_blocked;
            Vec3 hit; bool got_hit=false;
            int hov_idx=-1;
            got_hit=river_screen_to_world(mmx,mmy,win_w,win_h,proj,view,&hit);
            if(got_hit) hit.y=terrain_h(hit.x,hit.z);
            if(got_hit){
                float best=7.0f*7.0f;
                for(int i=0;i<g_mounts.count;i++){
                    MountInstance *m=&g_mounts.mounts[i];
                    if(!m->active) continue;
                    float dx=m->pos.x-hit.x,dz=m->pos.z-hit.z;
                    float d2=dx*dx+dz*dz;
                    if(d2<best){ best=d2; hov_idx=i; }
                }
            }
            if(lmb_now&&!mount_lmb_prev&&got_hit){
                if(hov_idx>=0){
                    mount_sel_idx=hov_idx;
                    mount_sel_class=(int)g_mounts.mounts[hov_idx].class_id;
                    mount_place_yaw=g_mounts.mounts[hov_idx].yaw;
                }else if(g_mounts.count<MOUNT_MAX){
                    MountInstance *m=mount_system_spawn(&g_mounts,(MountClassId)mount_sel_class,hit,mount_place_yaw);
                    if(m){
                        mount_sel_idx=g_mounts.count-1;
                        mount_sel_class=(int)m->class_id;
                        save_notify(&save_note,"Mount placed");
                    }
                }
            }
            if(rmb_now&&!mount_rmb_prev&&hov_idx>=0){
                mount_system_delete(&g_mounts,hov_idx);
                if(mount_sel_idx==hov_idx) mount_sel_idx=(hov_idx<g_mounts.count)?hov_idx:(g_mounts.count-1);
                else if(mount_sel_idx>hov_idx) mount_sel_idx--;
                save_notify(&save_note,"Mount deleted");
            }
            mount_lmb_prev=lmb_now;
            mount_rmb_prev=rmb_now;

            for(int i=0;i<g_mounts.count;i++){
                MountInstance *m=&g_mounts.mounts[i];
                if(!m->active) continue;
                float r=(i==mount_sel_idx)?1.3f:0.9f;
                float cr=(i==mount_sel_idx)?1.0f:0.45f;
                float cg=(i==mount_sel_idx)?0.9f:0.75f;
                float cb=(i==mount_sel_idx)?0.2f:1.0f;
                Vec3 p0={m->pos.x-r,m->pos.y+0.1f,m->pos.z};
                Vec3 p1={m->pos.x+r,m->pos.y+0.1f,m->pos.z};
                Vec3 p2={m->pos.x,m->pos.y+0.1f,m->pos.z-r};
                Vec3 p3={m->pos.x,m->pos.y+0.1f,m->pos.z+r};
                Vec3 p4={m->pos.x,m->pos.y+0.2f,m->pos.z};
                Vec3 p5={m->pos.x+sinf(m->yaw)*2.0f,m->pos.y+0.2f,m->pos.z+cosf(m->yaw)*2.0f};
                lr_push(p0,p1,cr,cg,cb);
                lr_push(p2,p3,cr,cg,cb);
                lr_push(p4,p5,cr,cg,cb);
            }
            if(got_hit){
                float r=0.7f;
                Vec3 p0={hit.x-r,hit.y+0.15f,hit.z};
                Vec3 p1={hit.x+r,hit.y+0.15f,hit.z};
                Vec3 p2={hit.x,hit.y+0.15f,hit.z-r};
                Vec3 p3={hit.x,hit.y+0.15f,hit.z+r};
                Vec3 p4={hit.x,hit.y+0.25f,hit.z};
                Vec3 p5={hit.x+sinf(mount_place_yaw)*1.6f,hit.y+0.25f,hit.z+cosf(mount_place_yaw)*1.6f};
                lr_push(p0,p1,0.95f,0.95f,0.95f);
                lr_push(p2,p3,0.95f,0.95f,0.95f);
                lr_push(p4,p5,0.95f,0.95f,0.95f);
            }
            lr_flush(&lr,proj,view);
            if(!editor_ui_backend_is_imgui(&g_editor_ui)){
                const MountClassDef *hud_def=mount_class_def((MountClassId)mount_sel_class);
                char mount_buf[128];
                char help_buf[128];
                snprintf(mount_buf,sizeof(mount_buf),"Mounts: %d / %d   Class: %s",g_mounts.count,MOUNT_MAX,hud_def->name);
                snprintf(help_buf,sizeof(help_buf),"Shift+F12 tool  S save  Backspace delete  [ ] yaw");
                hud_str(&hud,(float)win_w-420.0f,128.0f,0.90f,mount_buf,0.92f,0.82f,0.35f,0.98f);
                hud_str(&hud,(float)win_w-420.0f,146.0f,0.78f,help_buf,0.74f,0.80f,0.82f,0.92f);
            }
        }

        /* ── Point Light Editor ────────────────────────────────── */
        if(plight_edit){
            static bool plight_drag_active=false;
            static bool plight_drag_dirty=false;
            int pmx,pmy;
            Uint32 pmb=SDL_GetMouseState(&pmx,&pmy);
            bool lmb_now=((pmb&SDL_BUTTON(1))!=0)&&!editor_gui_mouse_blocked;
            bool rmb_now=((pmb&SDL_BUTTON(3))!=0)&&!editor_gui_mouse_blocked;
            Vec3 hit; bool got_hit=false;
            got_hit=river_screen_to_world(pmx,pmy,win_w,win_h,proj,view,&hit);
            if(got_hit) hit.y=terrain_h(hit.x,hit.z);
            if(prop_mouse_mode==1&&plight_sel>=0&&plight_sel<g_placed_lights.count&&g_placed_lights.items[plight_sel].active){
                PlacedLight *pl=&g_placed_lights.items[plight_sel];
                int hot_axis=(prop_gizmo_active_axis>0)?prop_gizmo_active_axis:
                    placed_light_gizmo_pick_axis(pl,pmx,pmy,win_w,win_h,proj,view,cam.pos);
                placed_light_gizmo_draw(pl,cam.pos,hot_axis,prop_gizmo_active_axis);
                lr_flush(&lr,proj,view);
                if(!plight_drag_active&&lmb_now&&!plight_lmb_prev&&hot_axis>0){
                    prop_gizmo_active_axis=hot_axis;
                    prop_gizmo_drag_accum=0.0f;
                    prop_gizmo_start_pos=pl->pos;
                    prop_mouse_last_x=pmx;
                    prop_mouse_last_y=pmy;
                    plight_drag_active=true;
                    plight_drag_dirty=false;
                }else if(plight_drag_active&&prop_gizmo_active_axis>0&&lmb_now){
                    Vec3 go,gx,gy,gz,ga;
                    float osx,osy,asx,asy,axis_dx,axis_dy,axis_len;
                    float delta_px;
                    editor_gizmo_axes(placed_light_gizmo_origin(pl),placed_light_gizmo_handle_len(pl,cam.pos),false,0.0f,&gx,&gy,&gz);
                    go=placed_light_gizmo_origin(pl);
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
                            delta_px=((float)(pmx-prop_mouse_last_x))*axis_dx+((float)(pmy-prop_mouse_last_y))*axis_dy;
                            prop_gizmo_drag_accum+=delta_px;
                        }
                    }else{
                        prop_gizmo_drag_accum+=(float)(pmx-prop_mouse_last_x);
                    }
                    {
                        float move_delta=prop_gizmo_drag_accum*cam.dist*0.012f;
                        Vec3 x_axis,y_axis,z_axis;
                        if(selection_gizmo_snap) move_delta=editor_gizmo_snap_value(move_delta,0.5f);
                        pl->pos=prop_gizmo_start_pos;
                        editor_gizmo_basis_yaw(false,0.0f,&x_axis,&y_axis,&z_axis);
                        if(prop_gizmo_active_axis==1) pl->pos=vec3_add(pl->pos,vec3_scale(x_axis,move_delta));
                        else if(prop_gizmo_active_axis==2) pl->pos=vec3_add(pl->pos,vec3_scale(y_axis,move_delta));
                        else if(prop_gizmo_active_axis==3) pl->pos=vec3_add(pl->pos,vec3_scale(z_axis,move_delta));
                        placed_light_runtime_sync(&g_placed_lights,&ls,plight_sel);
                        plight_drag_dirty=true;
                    }
                    prop_mouse_last_x=pmx;
                    prop_mouse_last_y=pmy;
                }else if(plight_drag_active&&!lmb_now){
                    if(plight_drag_dirty){
                        PlacedLight before=g_placed_lights.items[plight_sel];
                        before.pos=prop_gizmo_start_pos;
                        undo_push_plight_edit(plight_sel,before,g_placed_lights.items[plight_sel]);
                        placed_lights_save(&g_placed_lights);
                    }
                    plight_drag_active=false;
                    plight_drag_dirty=false;
                    prop_gizmo_active_axis=0;
                }
            }else if(prop_gizmo_active_axis>0&&!lmb_now){
                prop_gizmo_active_axis=0;
                plight_drag_active=false;
                plight_drag_dirty=false;
            }
            if(prop_mouse_mode==0&&lmb_now&&!plight_lmb_prev&&got_hit){
                float best=3.0f*3.0f; int bi=-1;
                for(int i=0;i<g_placed_lights.count;i++){
                    if(!g_placed_lights.items[i].active)continue;
                    float dx=g_placed_lights.items[i].pos.x-hit.x;
                    float dz=g_placed_lights.items[i].pos.z-hit.z;
                    float d2=dx*dx+dz*dz;
                    if(d2<best){best=d2;bi=i;}
                }
                if(bi>=0){
                    plight_sel=bi;
                    plight_place_type=g_placed_lights.items[bi].type;
                    plight_place_r=g_placed_lights.items[bi].color.x;
                    plight_place_g=g_placed_lights.items[bi].color.y;
                    plight_place_b=g_placed_lights.items[bi].color.z;
                    plight_place_radius=g_placed_lights.items[bi].radius;
                    plight_place_intensity=g_placed_lights.items[bi].intensity;
                    plight_place_y_offset=g_placed_lights.items[bi].y_offset;
                    plight_place_flicker=g_placed_lights.items[bi].flicker;
                    plight_place_flicker_speed=g_placed_lights.items[bi].flicker_speed;
                    plight_place_spot_yaw=g_placed_lights.items[bi].spot_yaw_deg;
                    plight_place_spot_pitch=g_placed_lights.items[bi].spot_pitch_deg;
                    plight_place_spot_inner_deg=g_placed_lights.items[bi].spot_inner_deg;
                    plight_place_spot_outer_deg=g_placed_lights.items[bi].spot_outer_deg;
                }else if(g_placed_lights.count<MAX_PLACED_LIGHTS){
                    int _plc_before=g_placed_lights.count;
                    PlacedLight *pl=&g_placed_lights.items[g_placed_lights.count];
                    pl->uid=world_alloc_light_uid();
                    pl->pos=(Vec3){hit.x,hit.y,hit.z};
                    pl->type=plight_place_type;
                    pl->color=(Vec3){plight_place_r,plight_place_g,plight_place_b};
                    pl->radius=plight_place_radius;
                    pl->intensity=plight_place_intensity;
                    pl->y_offset=plight_place_y_offset;
                    pl->flicker=plight_place_flicker;
                    pl->flicker_speed=plight_place_flicker_speed;
                    pl->spot_yaw_deg=plight_place_spot_yaw;
                    pl->spot_pitch_deg=plight_place_spot_pitch;
                    pl->spot_inner_deg=plight_place_spot_inner_deg;
                    pl->spot_outer_deg=plight_place_spot_outer_deg;
                    pl->enabled=true;
                    pl->active=true;
                    pl->light_idx=lighting_add_point(&ls,
                        (Vec3){hit.x,hit.y+plight_place_y_offset,hit.z},
                        (Vec3){plight_place_r,plight_place_g,plight_place_b},
                        plight_place_radius,plight_place_intensity);
                    placed_light_runtime_sync(&g_placed_lights,&ls,g_placed_lights.count);
                    plight_sel=g_placed_lights.count;
                    g_placed_lights.count++;
                    { UndoSnap _s; memset(&_s,0,sizeof(_s)); _s.plight=*pl;
                      undo_push_place(UNDO_PLIGHT_PLACE,_plc_before,_plc_before,_s); }
                    placed_lights_save(&g_placed_lights);
                }
            }
            if(prop_mouse_mode==0&&rmb_now&&!plight_rmb_prev&&got_hit){
                float best=12.0f*12.0f; int bi=-1;
                for(int i=0;i<g_placed_lights.count;i++){
                    if(!g_placed_lights.items[i].active)continue;
                    float dx=g_placed_lights.items[i].pos.x-hit.x;
                    float dz=g_placed_lights.items[i].pos.z-hit.z;
                    float d2=dx*dx+dz*dz;
                    if(d2<best){best=d2;bi=i;}
                }
                if(bi>=0){
                    { UndoSnap _s; memset(&_s,0,sizeof(_s)); _s.plight=g_placed_lights.items[bi];
                      undo_push_delete(UNDO_PLIGHT_DELETE,bi,g_placed_lights.count,_s); }
                    if(g_placed_lights.items[bi].light_idx>=0)
                        ls.points[g_placed_lights.items[bi].light_idx].intensity=0.0f;
                    g_placed_lights.items[bi].active=false;
                    if(plight_sel==bi) plight_sel=-1;
                    placed_lights_save(&g_placed_lights);
                }
            }
            plight_lmb_prev=lmb_now;
            plight_rmb_prev=rmb_now;
            placed_lights_draw_editor(&g_placed_lights,&lr,&hud,plight_sel,proj,view,win_w,win_h);
        }

        /* ── Decal Editor ──────────────────────────────────────── */
        if(decal_edit){
            int dmx,dmy;
            Uint32 dmb=SDL_GetMouseState(&dmx,&dmy);
            bool lmb_now=((dmb&SDL_BUTTON(1))!=0)&&!editor_gui_mouse_blocked;
            bool rmb_now=((dmb&SDL_BUTTON(3))!=0)&&!editor_gui_mouse_blocked;
            Vec3 hit; bool got_hit=false;
            got_hit=river_screen_to_world(dmx,dmy,win_w,win_h,proj,view,&hit);
            if(got_hit) hit.y=terrain_h(hit.x,hit.z);
            if(lmb_now&&!decal_lmb_prev&&got_hit&&g_decals.count<MAX_DECALS){
                int _dc_before=g_decals.count;
                WorldDecal *d=&g_decals.items[g_decals.count];
                d->pos=hit;
                d->size=decal_place_size;
                d->alpha=decal_place_alpha;
                d->yaw=decal_place_yaw;
                d->type=decal_place_type;
                d->active=true;
                g_decals.count++;
                { UndoSnap _s; memset(&_s,0,sizeof(_s)); _s.decal=*d;
                  undo_push_place(UNDO_DECAL_PLACE,_dc_before,_dc_before,_s); }
                decal_sel=g_decals.count-1;
                g_decals.gpu_dirty=true;
                decals_rebuild_vbo(&g_decals);
                decals_save(&g_decals);
            }
            if(rmb_now&&!decal_rmb_prev&&got_hit){
                float best=8.0f*8.0f; int bi=-1;
                for(int i=0;i<g_decals.count;i++){
                    if(!g_decals.items[i].active)continue;
                    float dx=g_decals.items[i].pos.x-hit.x;
                    float dz=g_decals.items[i].pos.z-hit.z;
                    float d2=dx*dx+dz*dz;
                    if(d2<best){best=d2;bi=i;}
                }
                if(bi>=0){
                    { UndoSnap _s; memset(&_s,0,sizeof(_s)); _s.decal=g_decals.items[bi];
                      undo_push_delete(UNDO_DECAL_DELETE,bi,g_decals.count,_s); }
                    g_decals.items[bi].active=false;
                    if(decal_sel==bi) decal_sel=-1;
                    g_decals.gpu_dirty=true;
                    decals_rebuild_vbo(&g_decals);
                    decals_save(&g_decals);
                }
            }
            decal_lmb_prev=lmb_now;
            decal_rmb_prev=rmb_now;
            decals_draw_editor(&g_decals,&lr,&hud,decal_sel,proj,view,win_w,win_h);
        }

        /* ── Sound Zone Editor ─────────────────────────────────── */
        if(szone_edit){
            int zzmx,zzmy;
            Uint32 zzmb=SDL_GetMouseState(&zzmx,&zzmy);
            bool lmb_now=((zzmb&SDL_BUTTON(1))!=0)&&!editor_gui_mouse_blocked;
            bool rmb_now=((zzmb&SDL_BUTTON(3))!=0)&&!editor_gui_mouse_blocked;
            Vec3 hit; bool got_hit=false;
            got_hit=river_screen_to_world(zzmx,zzmy,win_w,win_h,proj,view,&hit);
            if(got_hit) hit.y=terrain_h(hit.x,hit.z);
            if(lmb_now&&!szone_lmb_prev&&got_hit){
                float best_select_metric=1e30f;
                int best_select_idx=-1;
                for(int i=0;i<g_sound_zones.count;i++){
                    if(!g_sound_zones.items[i].active) continue;
                    float dx=g_sound_zones.items[i].cx-hit.x;
                    float dz=g_sound_zones.items[i].cz-hit.z;
                    float d=sqrtf(dx*dx+dz*dz);
                    float ring_band=fmaxf(2.5f,fminf(8.0f,g_sound_zones.items[i].radius*0.2f));
                    float ring_delta=fabsf(d-g_sound_zones.items[i].radius);
                    bool near_center=(d<=fmaxf(3.0f,g_sound_zones.items[i].radius*0.18f));
                    bool near_ring=(ring_delta<=ring_band);
                    bool inside_zone=(d<=g_sound_zones.items[i].radius);
                    if(!(near_center||near_ring||inside_zone)) continue;
                    if(ring_delta<best_select_metric){
                        best_select_metric=ring_delta;
                        best_select_idx=i;
                    }
                }
                if(best_select_idx>=0){
                    szone_sel=best_select_idx;
                    {
                        SoundZone *_sz=&g_sound_zones.items[szone_sel];
                        szone_type=(_sz->type==SOUND_ZONE_TYPE_MUSIC)?SOUND_ZONE_TYPE_MUSIC:SOUND_ZONE_TYPE_AMBIENT;
                        szone_sound_idx=(_sz->sound_idx>=0&&_sz->sound_idx<SZONE_SOUND_COUNT)?_sz->sound_idx:0;
                        szone_place_radius=_sz->radius;
                        szone_place_vol=_sz->volume;
                        szone_fade_len=_sz->fade_len;
                        szone_music_idx=0;
                        if(_sz->type==SOUND_ZONE_TYPE_MUSIC){
                            int _music_idx=sound_zone_music_index_from_path(_sz->music_path);
                            if(_music_idx>=0) szone_music_idx=_music_idx;
                        }
                    }
                } else if(g_sound_zones.count<MAX_SOUND_ZONES){
                    int _szc_before=g_sound_zones.count;
                    SoundZone *sz=&g_sound_zones.items[g_sound_zones.count];
                    memset(sz,0,sizeof(*sz));
                    sz->cx=hit.x; sz->cz=hit.z;
                    sz->radius=szone_place_radius;
                    sz->type=(szone_type==SOUND_ZONE_TYPE_MUSIC)?SOUND_ZONE_TYPE_MUSIC:SOUND_ZONE_TYPE_AMBIENT;
                    sz->sound_idx=szone_sound_idx;
                    sz->volume=szone_place_vol;
                    sz->fade_len=clampf(szone_fade_len,0.0f,szone_place_radius);
                    sz->active=true;
                    sz->cooldown=0.0f;
                    if(sz->type==SOUND_ZONE_TYPE_MUSIC&&
                       szone_music_idx>=0&&szone_music_idx<g_sound_zone_music_option_count){
                        snprintf(sz->music_path,sizeof(sz->music_path),"%s",
                                 g_sound_zone_music_paths[szone_music_idx]);
                    }
                    g_sound_zones.count++;
                    { UndoSnap _s; memset(&_s,0,sizeof(_s)); _s.szone=*sz;
                      undo_push_place(UNDO_SZONE_PLACE,_szc_before,_szc_before,_s); }
                    szone_sel=g_sound_zones.count-1;
                    sound_zones_save(&g_sound_zones);
                }
            }
            if(rmb_now&&!szone_rmb_prev&&got_hit){
                float best=szone_place_radius*szone_place_radius; int bi=-1;
                for(int i=0;i<g_sound_zones.count;i++){
                    if(!g_sound_zones.items[i].active)continue;
                    float dx=g_sound_zones.items[i].cx-hit.x;
                    float dz=g_sound_zones.items[i].cz-hit.z;
                    float d2=dx*dx+dz*dz;
                    if(d2<best){best=d2;bi=i;}
                }
                if(bi>=0){
                    { UndoSnap _s; memset(&_s,0,sizeof(_s)); _s.szone=g_sound_zones.items[bi];
                      undo_push_delete(UNDO_SZONE_DELETE,bi,g_sound_zones.count,_s); }
                    g_sound_zones.items[bi].active=false;
                    if(szone_sel==bi) szone_sel=-1;
                    sound_zones_save(&g_sound_zones);
                }
            }
            szone_lmb_prev=lmb_now;
            szone_rmb_prev=rmb_now;
            sound_zones_draw_editor(&g_sound_zones,&lr,&hud,szone_sel,proj,view,win_w,win_h);
        }

        if(interior_edit){
            int imx,imy;
            Uint32 imb=SDL_GetMouseState(&imx,&imy);
            bool lmb_now=((imb&SDL_BUTTON(1))!=0)&&!editor_gui_mouse_blocked;
            int hit_interior=-1;
            int hit_surface=INTERIOR_SURFACE_WALL;
            Vec3 hit_pos={0};
            bool got_interior_hit=interior_pick_surface(imx,imy,win_w,win_h,proj,view,&hit_interior,&hit_surface,&hit_pos);
            if(got_interior_hit){
                interior_selected_idx=hit_interior;
            }
            if(lmb_now&&!interior_lmb_prev&&got_interior_hit&&interior_material_sel>=0&&interior_material_sel<g_interior_material_count){
                unsigned int needed=(hit_surface==INTERIOR_SURFACE_FLOOR)?INTERIOR_SURFACE_FLAG_FLOOR:
                                    (hit_surface==INTERIOR_SURFACE_CEILING)?INTERIOR_SURFACE_FLAG_CEILING:
                                    INTERIOR_SURFACE_FLAG_WALL;
                if((g_interior_materials[interior_material_sel].surface_flags&needed)!=0){
                    g_interiors[hit_interior].material_ids[hit_surface]=interior_material_sel;
                    interior_surface_target=hit_surface;
                }
            }
            if(got_interior_hit){
                float ring_r=0.45f;
                for(int seg=0;seg<24;seg++){
                    float a0=(float)seg*(float)(2.0*M_PI/24.0);
                    float a1=(float)(seg+1)*(float)(2.0*M_PI/24.0);
                    Vec3 p0={hit_pos.x+cosf(a0)*ring_r,hit_pos.y,hit_pos.z+sinf(a0)*ring_r};
                    Vec3 p1={hit_pos.x+cosf(a1)*ring_r,hit_pos.y,hit_pos.z+sinf(a1)*ring_r};
                    lr_push(p0,p1,0.95f,0.86f,0.30f);
                }
            }
            interior_lmb_prev=lmb_now;
        }

        if(surface_edit){
            int smx,smy;
            Uint32 smb=SDL_GetMouseState(&smx,&smy);
            bool lmb_now=((smb&SDL_BUTTON(1))!=0)&&!editor_gui_mouse_blocked;
            int hit_surface_idx=-1;
            Vec3 hit_pos={0};
            bool got_surface_hit=surface_piece_pick_screen(smx,smy,win_w,win_h,proj,view,&hit_surface_idx,&hit_pos);
            bool selected_surface_hit=(g_editor_selection.kind==EDITOR_SELECTION_SURFACE&&g_editor_selection.index==hit_surface_idx);
            static bool surface_drag_active=false;
            static bool surface_drag_dirty=false;
            if(lmb_now&&!surface_lmb_prev){
                if(got_surface_hit){
                    editor_selection_set(EDITOR_SELECTION_SURFACE,hit_surface_idx);
                    surface_width=g_surfaces.items[hit_surface_idx].width;
                    surface_height=g_surfaces.items[hit_surface_idx].height;
                    surface_thickness=g_surfaces.items[hit_surface_idx].thickness;
                    surface_kind=(int)g_surfaces.items[hit_surface_idx].kind;
                    surface_coll_type=(int)g_surfaces.items[hit_surface_idx].coll_type;
                    surface_material_sel=g_surfaces.items[hit_surface_idx].material_id;
                }else{
                    Vec3 place_pos={0};
                    if(surface_piece_pick_place_point(smx,smy,win_w,win_h,proj,view,&place_pos)){
                        if(surface_kind==SURFACE_PIECE_WALL) place_pos.y+=surface_height*0.5f;
                        else place_pos.y+=surface_thickness*0.5f;
                        if(editor_command_surface_create(surface_kind,place_pos,surface_place_yaw,
                                                         surface_width,surface_height,surface_thickness,
                                                         surface_material_sel,surface_coll_type,
                                                         &hit_surface_idx,NULL)){
                            editor_selection_set(EDITOR_SELECTION_SURFACE,hit_surface_idx);
                        }
                    }
                }
            }
            if(g_editor_selection.kind==EDITOR_SELECTION_SURFACE&&
               g_editor_selection.index>=0&&g_editor_selection.index<g_surfaces.count&&
               g_surfaces.items[g_editor_selection.index].active){
                SurfacePiece *ssp=&g_surfaces.items[g_editor_selection.index];
                int surface_hot_axis=(prop_gizmo_active_axis>0)?prop_gizmo_active_axis:
                    surface_gizmo_pick_axis(ssp,smx,smy,win_w,win_h,proj,view,cam.pos,selection_gizmo_space!=0);
                bool can_begin_drag=(prop_mouse_mode==1)?
                    (surface_hot_axis>0||selected_surface_hit):
                    (prop_mouse_mode==2||prop_mouse_mode==3)?
                    (selected_surface_hit||surface_hot_axis>0):false;
                if(prop_mouse_mode>0){
                    surface_gizmo_draw(ssp,cam.pos,selection_gizmo_space!=0,surface_hot_axis,prop_gizmo_active_axis);
                    lr_flush(&lr,proj,view);
                }
                if(lmb_now&&!surface_lmb_prev&&can_begin_drag){
                    surface_drag_active=true;
                    surface_drag_dirty=false;
                    if(surface_hot_axis>0) prop_gizmo_active_axis=surface_hot_axis;
                    else prop_gizmo_active_axis=4;
                    prop_mouse_last_x=smx;
                    prop_mouse_last_y=smy;
                    prop_gizmo_drag_accum=0.0f;
                    prop_gizmo_start_pos=ssp->pos;
                    prop_gizmo_start_rot_y=ssp->yaw_deg;
                    prop_gizmo_start_rot_x=ssp->width;
                    prop_gizmo_start_rot_z=ssp->height;
                    prop_gizmo_start_scale=ssp->thickness;
                }else if(surface_drag_active&&prop_gizmo_active_axis>0&&lmb_now){
                    Vec3 go,gx,gy,gz,ga;
                    float osx,osy,asx,asy,axis_dx,axis_dy,axis_len;
                    float delta_px;
                    if(prop_mouse_mode==1&&prop_gizmo_active_axis==4){
                        Vec3 place_pos={0};
                        if(surface_piece_pick_place_point(smx,smy,win_w,win_h,proj,view,&place_pos)){
                            ssp->pos.x=place_pos.x;
                            ssp->pos.z=place_pos.z;
                            surface_drag_dirty=true;
                        }
                    }else{
                        surface_gizmo_axes(ssp,cam.pos,selection_gizmo_space!=0,&go,&gx,&gy,&gz);
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
                        }else{
                            prop_gizmo_drag_accum+=(float)(smx-prop_mouse_last_x);
                        }
                        if(prop_mouse_mode==1){
                            float move_delta=prop_gizmo_drag_accum*cam.dist*0.012f;
                            Vec3 x_axis,y_axis,z_axis;
                            if(selection_gizmo_snap) move_delta=editor_gizmo_snap_value(move_delta,0.5f);
                            ssp->pos=prop_gizmo_start_pos;
                            editor_gizmo_basis_yaw(selection_gizmo_space!=0,prop_gizmo_start_rot_y*(float)M_PI/180.0f,&x_axis,&y_axis,&z_axis);
                            if(prop_gizmo_active_axis==1) ssp->pos=vec3_add(ssp->pos,vec3_scale(x_axis,move_delta));
                            else if(prop_gizmo_active_axis==2) ssp->pos=vec3_add(ssp->pos,vec3_scale(y_axis,move_delta));
                            else if(prop_gizmo_active_axis==3) ssp->pos=vec3_add(ssp->pos,vec3_scale(z_axis,move_delta));
                            surface_drag_dirty=true;
                        }else if(prop_mouse_mode==2){
                            ssp->yaw_deg=prop_gizmo_start_rot_y+prop_gizmo_drag_accum*0.8f;
                            if(selection_gizmo_snap) ssp->yaw_deg=editor_gizmo_snap_value(ssp->yaw_deg,15.0f);
                            surface_drag_dirty=true;
                        }else if(prop_mouse_mode==3){
                            ssp->width=clampf(prop_gizmo_start_rot_x,0.25f,64.0f);
                            ssp->height=clampf(prop_gizmo_start_rot_z,0.25f,64.0f);
                            ssp->thickness=clampf(prop_gizmo_start_scale,0.05f,8.0f);
                            if(prop_gizmo_active_axis==1){
                                ssp->width=clampf(prop_gizmo_start_rot_x+prop_gizmo_drag_accum*cam.dist*0.012f,0.25f,64.0f);
                            }else if(prop_gizmo_active_axis==2){
                                if(ssp->kind==SURFACE_PIECE_WALL)
                                    ssp->height=clampf(prop_gizmo_start_rot_z+prop_gizmo_drag_accum*cam.dist*0.012f,0.25f,64.0f);
                                else
                                    ssp->thickness=clampf(prop_gizmo_start_scale+prop_gizmo_drag_accum*cam.dist*0.006f,0.05f,8.0f);
                            }else if(prop_gizmo_active_axis==3){
                                if(ssp->kind==SURFACE_PIECE_WALL)
                                    ssp->thickness=clampf(prop_gizmo_start_scale+prop_gizmo_drag_accum*cam.dist*0.006f,0.05f,8.0f);
                                else
                                    ssp->height=clampf(prop_gizmo_start_rot_z+prop_gizmo_drag_accum*cam.dist*0.012f,0.25f,64.0f);
                            }else if(prop_gizmo_active_axis==4){
                                float mul=1.0f+prop_gizmo_drag_accum*0.01f;
                                if(mul<0.25f) mul=0.25f;
                                ssp->width=clampf(prop_gizmo_start_rot_x*mul,0.25f,64.0f);
                                ssp->height=clampf(prop_gizmo_start_rot_z*mul,0.25f,64.0f);
                                ssp->thickness=clampf(prop_gizmo_start_scale*mul,0.05f,8.0f);
                            }
                            if(selection_gizmo_snap){
                                ssp->width=clampf(editor_gizmo_snap_value(ssp->width,0.25f),0.25f,64.0f);
                                ssp->height=clampf(editor_gizmo_snap_value(ssp->height,0.25f),0.25f,64.0f);
                                ssp->thickness=clampf(editor_gizmo_snap_value(ssp->thickness,0.05f),0.05f,8.0f);
                            }
                            surface_drag_dirty=true;
                        }
                    }
                    prop_mouse_last_x=smx;
                    prop_mouse_last_y=smy;
                    surface_width=ssp->width;
                    surface_height=ssp->height;
                    surface_thickness=ssp->thickness;
                }else if(surface_drag_active&&!lmb_now){
                    if(surface_drag_dirty)
                        editor_command_surface_commit_xform(g_editor_selection.index,
                                                            prop_gizmo_start_pos,
                                                            prop_gizmo_start_rot_y,
                                                            prop_gizmo_start_rot_x,
                                                            prop_gizmo_start_rot_z,
                                                            prop_gizmo_start_scale,
                                                            NULL);
                    surface_drag_active=false;
                    surface_drag_dirty=false;
                    prop_gizmo_active_axis=0;
                }
            }else if(prop_gizmo_active_axis>0&&!lmb_now){
                surface_drag_active=false;
                surface_drag_dirty=false;
                prop_gizmo_active_axis=0;
            }
            if(got_surface_hit){
                float ring_r=0.35f;
                for(int seg=0;seg<20;seg++){
                    float a0=(float)seg*(float)(2.0*M_PI/20.0);
                    float a1=(float)(seg+1)*(float)(2.0*M_PI/20.0);
                    Vec3 p0={hit_pos.x+cosf(a0)*ring_r,hit_pos.y,hit_pos.z+sinf(a0)*ring_r};
                    Vec3 p1={hit_pos.x+cosf(a1)*ring_r,hit_pos.y,hit_pos.z+sinf(a1)*ring_r};
                    lr_push(p0,p1,0.30f,0.92f,0.95f);
                }
            }
            if(g_editor_selection.kind==EDITOR_SELECTION_SURFACE&&
               g_editor_selection.index>=0&&g_editor_selection.index<g_surfaces.count&&
               g_surfaces.items[g_editor_selection.index].active){
                surface_piece_debug_wireframe(&g_surfaces.items[g_editor_selection.index],1.00f,0.86f,0.26f);
            }
            if(got_surface_hit&&
               (g_editor_selection.kind!=EDITOR_SELECTION_SURFACE||g_editor_selection.index!=hit_surface_idx)&&
               hit_surface_idx>=0&&hit_surface_idx<g_surfaces.count&&g_surfaces.items[hit_surface_idx].active){
                surface_piece_debug_wireframe(&g_surfaces.items[hit_surface_idx],0.30f,0.92f,0.95f);
            }
            surface_lmb_prev=lmb_now;
        }
        if(collision_edit){
            int smx,smy;
            Uint32 smb=SDL_GetMouseState(&smx,&smy);
            bool lmb_now=((smb&SDL_BUTTON(1))!=0)&&!editor_gui_mouse_blocked;
            int hit_idx=-1;
            Vec3 hit_pos={0};
            bool got_hit=collision_volume_pick_screen(smx,smy,win_w,win_h,proj,view,&hit_idx,&hit_pos);
            bool selected_hit=(g_editor_selection.kind==EDITOR_SELECTION_COLLISION_VOLUME&&g_editor_selection.index==hit_idx);
            static bool collision_drag_active=false;
            static bool collision_drag_dirty=false;
            static float collision_drag_start_width=4.0f;
            if(lmb_now&&!collision_lmb_prev){
                if(got_hit){
                    Vec3 motion_offset={0};
                    editor_selection_set(EDITOR_SELECTION_COLLISION_VOLUME,hit_idx);
                    collision_width=g_collision_volumes.items[hit_idx].width;
                    collision_height=g_collision_volumes.items[hit_idx].height;
                    collision_depth=g_collision_volumes.items[hit_idx].depth;
                    collision_shape=g_collision_volumes.items[hit_idx].shape;
                    collision_place_yaw=g_collision_volumes.items[hit_idx].yaw_deg;
                    collision_place_pitch=g_collision_volumes.items[hit_idx].pitch_deg;
                    collision_enabled=g_collision_volumes.items[hit_idx].enabled;
                    collision_nav_blocker=g_collision_volumes.items[hit_idx].nav_blocker;
                    if(collision_volume_motion_describe(hit_idx,&collision_motion_enabled,&motion_offset,
                                                       &collision_motion_period,&collision_motion_phase)){
                        collision_motion_x=motion_offset.x;
                        collision_motion_y=motion_offset.y;
                        collision_motion_z=motion_offset.z;
                    }
                }else{
                    Vec3 place_pos={0};
                    if(surface_piece_pick_place_point(smx,smy,win_w,win_h,proj,view,&place_pos)){
                        place_pos.y+=collision_height*0.5f;
                        hit_idx=collision_volume_register(&g_collision_volumes,place_pos,collision_shape,collision_place_yaw,collision_place_pitch,
                                                          collision_width,collision_height,collision_depth,
                                                          collision_enabled,collision_nav_blocker);
                        if(hit_idx>=0){
                            g_collision_volumes.items[hit_idx].motion_enabled=collision_motion_enabled;
                            g_collision_volumes.items[hit_idx].motion_offset=(Vec3){collision_motion_x,collision_motion_y,collision_motion_z};
                            g_collision_volumes.items[hit_idx].motion_period=fmaxf(0.1f,collision_motion_period);
                            g_collision_volumes.items[hit_idx].motion_phase=collision_motion_phase;
                            collision_volumes_save(&g_collision_volumes);
                            editor_selection_set(EDITOR_SELECTION_COLLISION_VOLUME,hit_idx);
                        }
                    }
                }
            }
            if(g_editor_selection.kind==EDITOR_SELECTION_COLLISION_VOLUME&&
               g_editor_selection.index>=0&&g_editor_selection.index<g_collision_volumes.count&&
               g_collision_volumes.items[g_editor_selection.index].active){
                CollisionVolume *cv=&g_collision_volumes.items[g_editor_selection.index];
                SurfacePiece cv_proxy={.active=true,.kind=SURFACE_PIECE_WALL,.pos=cv->pos,.yaw_deg=cv->yaw_deg,.width=cv->width,.height=cv->height,.thickness=cv->depth};
                int hot_axis=(prop_gizmo_active_axis>0)?prop_gizmo_active_axis:
                    surface_gizmo_pick_axis(&cv_proxy,smx,smy,win_w,win_h,proj,view,cam.pos,selection_gizmo_space!=0);
                bool can_begin_drag=(prop_mouse_mode==1)?
                    (hot_axis>0||selected_hit):
                    (prop_mouse_mode==2||prop_mouse_mode==3)?
                    (selected_hit||hot_axis>0):false;
                if(prop_mouse_mode>0){
                    surface_gizmo_draw(&cv_proxy,cam.pos,selection_gizmo_space!=0,hot_axis,prop_gizmo_active_axis);
                    lr_flush(&lr,proj,view);
                }
                if(lmb_now&&!collision_lmb_prev&&can_begin_drag){
                    collision_drag_active=true;
                    collision_drag_dirty=false;
                    if(hot_axis>0) prop_gizmo_active_axis=hot_axis;
                    else prop_gizmo_active_axis=4;
                    prop_mouse_last_x=smx;
                    prop_mouse_last_y=smy;
                    prop_gizmo_drag_accum=0.0f;
                    prop_gizmo_start_pos=cv->pos;
                    prop_gizmo_start_rot_y=cv->yaw_deg;
                    prop_gizmo_start_rot_x=cv->pitch_deg;
                    prop_gizmo_start_rot_z=cv->height;
                    prop_gizmo_start_scale=cv->depth;
                    collision_drag_start_width=cv->width;
                }else if(collision_drag_active&&prop_gizmo_active_axis>0&&lmb_now){
                    Vec3 go,gx,gy,gz,ga;
                    float osx,osy,asx,asy,axis_dx,axis_dy,axis_len;
                    float delta_px;
                    if(prop_mouse_mode==1&&prop_gizmo_active_axis==4){
                        Vec3 place_pos={0};
                        if(surface_piece_pick_place_point(smx,smy,win_w,win_h,proj,view,&place_pos)){
                            cv->pos.x=place_pos.x;
                            cv->pos.z=place_pos.z;
                            collision_drag_dirty=true;
                        }
                    }else{
                        surface_gizmo_axes(&cv_proxy,cam.pos,selection_gizmo_space!=0,&go,&gx,&gy,&gz);
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
                        }else{
                            prop_gizmo_drag_accum+=(float)(smx-prop_mouse_last_x);
                        }
                        if(prop_mouse_mode==1){
                            float move_delta=prop_gizmo_drag_accum*cam.dist*0.012f;
                            Vec3 x_axis,y_axis,z_axis;
                            if(selection_gizmo_snap) move_delta=editor_gizmo_snap_value(move_delta,0.5f);
                            cv->pos=prop_gizmo_start_pos;
                            editor_gizmo_basis_yaw(selection_gizmo_space!=0,prop_gizmo_start_rot_y*(float)M_PI/180.0f,&x_axis,&y_axis,&z_axis);
                            if(prop_gizmo_active_axis==1) cv->pos=vec3_add(cv->pos,vec3_scale(x_axis,move_delta));
                            else if(prop_gizmo_active_axis==2) cv->pos=vec3_add(cv->pos,vec3_scale(y_axis,move_delta));
                            else if(prop_gizmo_active_axis==3) cv->pos=vec3_add(cv->pos,vec3_scale(z_axis,move_delta));
                            collision_drag_dirty=true;
                        }else if(prop_mouse_mode==2){
                            if(prop_gizmo_active_axis==1&&collision_volume_shape_sanitize(cv->shape)==COLLISION_VOLUME_SHAPE_BOX){
                                cv->pitch_deg=clampf(prop_gizmo_start_rot_x+prop_gizmo_drag_accum*0.8f,-89.0f,89.0f);
                                if(selection_gizmo_snap) cv->pitch_deg=editor_gizmo_snap_value(cv->pitch_deg,15.0f);
                            }else{
                                cv->yaw_deg=prop_gizmo_start_rot_y+prop_gizmo_drag_accum*0.8f;
                                if(selection_gizmo_snap) cv->yaw_deg=editor_gizmo_snap_value(cv->yaw_deg,15.0f);
                            }
                            collision_drag_dirty=true;
                        }else if(prop_mouse_mode==3){
                            cv->width=clampf(collision_drag_start_width,0.25f,64.0f);
                            cv->height=clampf(prop_gizmo_start_rot_z,0.25f,64.0f);
                            cv->depth=clampf(prop_gizmo_start_scale,0.25f,64.0f);
                            if(prop_gizmo_active_axis==1)
                                cv->width=clampf(collision_drag_start_width+prop_gizmo_drag_accum*cam.dist*0.012f,0.25f,64.0f);
                            else if(prop_gizmo_active_axis==2)
                                cv->height=clampf(prop_gizmo_start_rot_z+prop_gizmo_drag_accum*cam.dist*0.012f,0.25f,64.0f);
                            else if(prop_gizmo_active_axis==3)
                                cv->depth=clampf(prop_gizmo_start_scale+prop_gizmo_drag_accum*cam.dist*0.012f,0.25f,64.0f);
                            else if(prop_gizmo_active_axis==4){
                                float mul=1.0f+prop_gizmo_drag_accum*0.01f;
                                if(mul<0.25f) mul=0.25f;
                                cv->width=clampf(collision_drag_start_width*mul,0.25f,64.0f);
                                cv->height=clampf(prop_gizmo_start_rot_z*mul,0.25f,64.0f);
                                cv->depth=clampf(prop_gizmo_start_scale*mul,0.25f,64.0f);
                            }
                            if(selection_gizmo_snap){
                                cv->width=clampf(editor_gizmo_snap_value(cv->width,0.25f),0.25f,64.0f);
                                cv->height=clampf(editor_gizmo_snap_value(cv->height,0.25f),0.25f,64.0f);
                                cv->depth=clampf(editor_gizmo_snap_value(cv->depth,0.25f),0.25f,64.0f);
                            }
                            collision_drag_dirty=true;
                        }
                    }
                    prop_mouse_last_x=smx;
                    prop_mouse_last_y=smy;
                    collision_width=cv->width;
                    collision_height=cv->height;
                    collision_depth=cv->depth;
                    collision_shape=cv->shape;
                    collision_place_yaw=cv->yaw_deg;
                    collision_place_pitch=cv->pitch_deg;
                }else if(collision_drag_active&&!lmb_now){
                    if(collision_drag_dirty) collision_volumes_save(&g_collision_volumes);
                    collision_drag_active=false;
                    collision_drag_dirty=false;
                    prop_gizmo_active_axis=0;
                }
                {
                    collision_volume_debug_wireframe(cv,1.00f,0.30f,0.10f);
                }
            }else if(prop_gizmo_active_axis>0&&!lmb_now){
                collision_drag_active=false;
                collision_drag_dirty=false;
                prop_gizmo_active_axis=0;
            }
            if(got_hit&&
               (g_editor_selection.kind!=EDITOR_SELECTION_COLLISION_VOLUME||g_editor_selection.index!=hit_idx)&&
               hit_idx>=0&&hit_idx<g_collision_volumes.count&&g_collision_volumes.items[hit_idx].active){
                collision_volume_debug_wireframe(&g_collision_volumes.items[hit_idx],0.30f,0.92f,0.95f);
            }
            collision_lmb_prev=lmb_now;
        }

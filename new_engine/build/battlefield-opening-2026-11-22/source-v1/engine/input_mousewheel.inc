                if(wzone_edit&&wf_edit_mode&&wf_sel>=0){
                    Waterfall *_wfw=&g_wfalls.wf[wf_sel];
                    const Uint8 *_kwf=SDL_GetKeyboardState(NULL);
                    if(_kwf[SDL_SCANCODE_LSHIFT]||_kwf[SDL_SCANCODE_RSHIFT]){
                        _wfw->width=clampf(_wfw->width+ev.wheel.y*0.5f,1.0f,80.0f);
                        printf("[WFalls] Width: %.1f\n",(double)_wfw->width);
                    } else {
                        _wfw->height=clampf(_wfw->height+ev.wheel.y*1.0f,2.0f,200.0f);
                        printf("[WFalls] Height: %.1f\n",(double)_wfw->height);
                    }
                    if(_wfw->mist_em>=0&&_wfw->mist_em<ps.ecount)
                        ps.emitters[_wfw->mist_em].pos=(Vec3){
                            _wfw->pos.x,_wfw->pos.y-_wfw->height+0.6f,_wfw->pos.z};
                    wf_save();
                } else if(sculpt_edit){
                    const Uint8 *ksw=SDL_GetKeyboardState(NULL);
                    if(ksw[SDL_SCANCODE_LSHIFT]||ksw[SDL_SCANCODE_RSHIFT]){
                        sculpt_strength=clampf(sculpt_strength+ev.wheel.y*0.005f,0.002f,0.5f);
                        printf("[SculptEd] Strength: %.3f\n",(double)sculpt_strength);
                    } else {
                        sculpt_radius=clampf(sculpt_radius+ev.wheel.y*2.0f,3.0f,300.0f);
                        printf("[SculptEd] Radius: %.0f\n",(double)sculpt_radius);
                    }
                } else if(wind_edit&&wind_sel>=0){
                    const Uint8 *ksw=SDL_GetKeyboardState(NULL);
                    if(ksw[SDL_SCANCODE_LCTRL]||ksw[SDL_SCANCODE_RCTRL]){
                        WindZone z=g_wind_zones[wind_sel];
                        z.radius=clampf(z.radius+ev.wheel.y*4.0f,8.0f,800.0f);
                        windzone_update_with_undo(wind_sel,&z);
                        printf("[WindEd] Zone %d radius: %.1f\n",
                               wind_sel,(double)g_wind_zones[wind_sel].radius);
                    }
                } else if(forest_edit){
                    const Uint8 *ksw=SDL_GetKeyboardState(NULL);
                    if(forest_brush_kind==1){
                        if((ksw[SDL_SCANCODE_LALT]||ksw[SDL_SCANCODE_RALT])&&
                           (ksw[SDL_SCANCODE_LSHIFT]||ksw[SDL_SCANCODE_RSHIFT])){
                            grass_brush_hmax=clampf(grass_brush_hmax+ev.wheel.y*0.08f,0.30f,4.0f);
                            if(grass_brush_hmax<grass_brush_hmin) grass_brush_hmax=grass_brush_hmin;
                            printf("[GrassBrush] Height max: %.2f\n",(double)grass_brush_hmax);
                        } else if(ksw[SDL_SCANCODE_LALT]||ksw[SDL_SCANCODE_RALT]){
                            grass_brush_hmin=clampf(grass_brush_hmin+ev.wheel.y*0.06f,0.20f,3.5f);
                            if(grass_brush_hmin>grass_brush_hmax) grass_brush_hmin=grass_brush_hmax;
                            printf("[GrassBrush] Height min: %.2f\n",(double)grass_brush_hmin);
                        } else if(ksw[SDL_SCANCODE_LCTRL]||ksw[SDL_SCANCODE_RCTRL]){
                            grass_brush_size=clampf(grass_brush_size+ev.wheel.y*0.08f,0.35f,3.0f);
                            printf("[GrassBrush] Size: %.2f\n",(double)grass_brush_size);
                        } else if(ksw[SDL_SCANCODE_LSHIFT]||ksw[SDL_SCANCODE_RSHIFT]){
                            grass_brush_density+=ev.wheel.y;
                            if(grass_brush_density<1) grass_brush_density=1;
                            if(grass_brush_density>120) grass_brush_density=120;
                            printf("[GrassBrush] Density: %d\n",grass_brush_density);
                        } else {
                            grass_brush_radius=clampf(grass_brush_radius+ev.wheel.y*2.0f,4.0f,180.0f);
                            printf("[GrassBrush] Radius: %.0f\n",(double)grass_brush_radius);
                        }
                    } else if(forest_brush_kind==2){
                        if(ksw[SDL_SCANCODE_LCTRL]||ksw[SDL_SCANCODE_RCTRL]){
                            flower_brush_size=clampf(flower_brush_size+ev.wheel.y*0.08f,0.35f,3.0f);
                            printf("[FlowerBrush] Size: %.2f\n",(double)flower_brush_size);
                        } else if(ksw[SDL_SCANCODE_LSHIFT]||ksw[SDL_SCANCODE_RSHIFT]){
                            flower_brush_density+=ev.wheel.y;
                            if(flower_brush_density<1) flower_brush_density=1;
                            if(flower_brush_density>120) flower_brush_density=120;
                            printf("[FlowerBrush] Density: %d\n",flower_brush_density);
                        } else {
                            grass_brush_radius=clampf(grass_brush_radius+ev.wheel.y*2.0f,4.0f,180.0f);
                            printf("[FlowerBrush] Radius: %.0f\n",(double)grass_brush_radius);
                        }
                    } else if(forest_brush_kind==4){
                        if(ksw[SDL_SCANCODE_LCTRL]||ksw[SDL_SCANCODE_RCTRL]){
                            wall_brush_scale=clampf(wall_brush_scale+ev.wheel.y*0.05f,0.25f,20.0f);
                            printf("[WallBrush] Scale: %.2f\n",(double)wall_brush_scale);
                        } else if(ksw[SDL_SCANCODE_LSHIFT]||ksw[SDL_SCANCODE_RSHIFT]){
                            wall_brush_spacing=clampf(wall_brush_spacing+ev.wheel.y*0.25f,1.0f,24.0f);
                            printf("[WallBrush] Spacing: %.2f\n",(double)wall_brush_spacing);
                        } else {
                            forest_brush_radius=clampf(forest_brush_radius+ev.wheel.y*2.0f,4.0f,180.0f);
                            printf("[WallBrush] Erase radius: %.0f\n",(double)forest_brush_radius);
                        }
                    } else if(forest_brush_kind==5){
                        if(ksw[SDL_SCANCODE_LSHIFT]||ksw[SDL_SCANCODE_RSHIFT]){
                            terrain_brush_strength=clampf(terrain_brush_strength+ev.wheel.y*0.04f,0.04f,1.0f);
                            printf("[TerrainBrush] Strength: %.2f\n",(double)terrain_brush_strength);
                        } else {
                            forest_brush_radius=clampf(forest_brush_radius+ev.wheel.y*2.0f,4.0f,220.0f);
                            printf("[TerrainBrush] Radius: %.0f\n",(double)forest_brush_radius);
                        }
                    } else if(forest_brush_kind==6){
                        if(ksw[SDL_SCANCODE_LSHIFT]||ksw[SDL_SCANCODE_RSHIFT]){
                            terrain_brush_strength=clampf(terrain_brush_strength+ev.wheel.y*0.04f,0.04f,1.0f);
                            printf("[FarmBrush] Strength: %.2f\n",(double)terrain_brush_strength);
                        } else {
                            forest_brush_radius=clampf(forest_brush_radius+ev.wheel.y*2.0f,4.0f,220.0f);
                            printf("[FarmBrush] Radius: %.0f\n",(double)forest_brush_radius);
                        }
                    } else {
                        if(ksw[SDL_SCANCODE_LSHIFT]||ksw[SDL_SCANCODE_RSHIFT]){
                            forest_brush_density+=ev.wheel.y;
                            if(forest_brush_density<1) forest_brush_density=1;
                            if(forest_brush_density>12) forest_brush_density=12;
                            printf("[ForestEd] Density: %d\n",forest_brush_density);
                        } else {
                            forest_brush_radius=clampf(forest_brush_radius+ev.wheel.y*2.0f,6.0f,160.0f);
                            printf("[ForestEd] Radius: %.0f\n",(double)forest_brush_radius);
                        }
                    }
                } else if(wzone_edit&&wf_edit_mode&&wf_sel<0){
                    const Uint8 *_kwfp=SDL_GetKeyboardState(NULL);
                    if(_kwfp[SDL_SCANCODE_LSHIFT]||_kwfp[SDL_SCANCODE_RSHIFT])
                        wf_place_width=clampf(wf_place_width+ev.wheel.y*0.5f,1.0f,80.0f);
                    else
                        wf_place_height=clampf(wf_place_height+ev.wheel.y*1.0f,2.0f,200.0f);
                } else if(npc_edit_mode){
                    npc_ed_yaw=fmodf(npc_ed_yaw+(float)ev.wheel.y*0.2f,(float)(2*M_PI));
                } else if(marker_edit&&marker_sel>=0&&marker_sel<g_world_markers.count&&g_world_markers.items[marker_sel].active){
                    g_world_markers.items[marker_sel].radius=clampf(g_world_markers.items[marker_sel].radius+ev.wheel.y*2.0f,2.0f,200.0f);
                    printf("[MarkerEd] Radius: %.1f\n",(double)g_world_markers.items[marker_sel].radius);
                } else if(wzone_edit&&wzone_sel>=0){
                    const Uint8 *ksw=SDL_GetKeyboardState(NULL);
                    if(ksw[SDL_SCANCODE_LCTRL]||ksw[SDL_SCANCODE_RCTRL]){
                        WaterZone z=g_wzones[wzone_sel];
                        z.radius=clampf(z.radius+ev.wheel.y*5.0f,10.0f,1000.0f);
                        wzone_update_with_undo(wzone_sel,&z,&terrain);
                        printf("[WZoneEd] Zone %d radius: %.0f\n",
                               wzone_sel,(double)g_wzones[wzone_sel].radius);
                    }
                } else if(path_edit){
                    const Uint8 *ksp=SDL_GetKeyboardState(NULL);
                    if(ksp[SDL_SCANCODE_LCTRL]||ksp[SDL_SCANCODE_RCTRL]){
                        g_cpath.width=clampf(g_cpath.width+ev.wheel.y*0.5f,0.5f,40.0f);
                        if(g_cpath.count>=2) path_rebuild_mesh();
                        printf("[PathEd] Width: %.1f\n",(double)g_cpath.width);
                    }
                } else if(river_edit){
                    const Uint8 *ks2=SDL_GetKeyboardState(NULL);
                    if(ks2[SDL_SCANCODE_LCTRL]||ks2[SDL_SCANCODE_RCTRL]){
                        g_criver.width=clampf(g_criver.width+ev.wheel.y*5.0f,10.0f,400.0f);
                        g_criver.dirty=true;
                        terrain_rebuild_all(&terrain);
                        terrain_rebake_hmap(&terrain);
                        printf("[RiverEd] Width: %.0f\n",(double)g_criver.width);
                    }
                } else if(prop_place_mode){
                    const Uint8 *ks=SDL_GetKeyboardState(NULL);
                    if(ks[SDL_SCANCODE_LCTRL]||ks[SDL_SCANCODE_RCTRL]){
                        g_placer.place_scale=clampf(g_placer.place_scale+ev.wheel.y*0.05f,0.05f,10.0f);
                    } else if(g_placer.prefab_mode) {
                        if(ks[SDL_SCANCODE_LSHIFT]||ks[SDL_SCANCODE_RSHIFT]){
                            g_placer.prefab_capture_radius=clampf(g_placer.prefab_capture_radius+ev.wheel.y*1.0f,2.0f,48.0f);
                        } else if(g_prefabs.count>0){
                            int vis=12;
                            g_placer.prefab_sel=clampf((float)(g_placer.prefab_sel-ev.wheel.y),
                                                       0,(float)(g_prefabs.count-1));
                            g_prefabs.selected=g_placer.prefab_sel;
                            if(g_placer.prefab_sel<g_placer.prefab_scroll)
                                g_placer.prefab_scroll=g_placer.prefab_sel;
                            if(g_placer.prefab_sel>=g_placer.prefab_scroll+vis)
                                g_placer.prefab_scroll=g_placer.prefab_sel-vis+1;
                        }
                    } else if(g_placer.filtered_count>0) {
                        g_placer.palette_sel=clampf((float)(g_placer.palette_sel-ev.wheel.y),
                                                    0,(float)(g_placer.filtered_count-1));
                        int vis=12;
                        if(g_placer.palette_sel<g_placer.palette_scroll)
                            g_placer.palette_scroll=g_placer.palette_sel;
                        if(g_placer.palette_sel>=g_placer.palette_scroll+vis)
                            g_placer.palette_scroll=g_placer.palette_sel-vis+1;
                        placer_apply_selected_defaults(&g_placer);
                    }
                } else if(particle_fx_edit){
                    const Uint8 *ks=SDL_GetKeyboardState(NULL);
                    if(ks[SDL_SCANCODE_LCTRL]||ks[SDL_SCANCODE_RCTRL]){
                        g_world_fx_editor.scale=clampf(g_world_fx_editor.scale+ev.wheel.y*0.05f,0.05f,12.0f);
                    }else if(ks[SDL_SCANCODE_LSHIFT]||ks[SDL_SCANCODE_RSHIFT]){
                        g_world_fx_editor.intensity=clampf(g_world_fx_editor.intensity+ev.wheel.y*0.10f,0.1f,4.0f);
                    }else if(ks[SDL_SCANCODE_LALT]||ks[SDL_SCANCODE_RALT]){
                        g_world_fx_editor.light_radius=clampf(g_world_fx_editor.light_radius+ev.wheel.y*0.5f,0.0f,40.0f);
                    }else{
                        int next=(int)g_world_fx_editor.kind-ev.wheel.y;
                        while(next<0) next+=WORLD_FX_COUNT;
                        g_world_fx_editor.kind=(WorldFxKind)(next%WORLD_FX_COUNT);
                    }
                    if(g_world_fx.selected>=0) world_fx_update_selected_with_undo(g_undo_ls,&g_world_fx_editor);
                } else if(map_screen.open){
                    map_screen.zoom*=(ev.wheel.y>0)?1.25f:0.80f;
                    map_screen.zoom=clampf(map_screen.zoom,0.5f,8.0f);
                } else if(editor_mode){
                    cam.dist-=ev.wheel.y*4.0f;
                    cam.dist=clampf(cam.dist,8.0f,320.0f);
                } else {
                    cam.dist-=ev.wheel.y*1.5f;
                    cam.dist=clampf(cam.dist,3.0f,THIRD_PERSON_PLAY_MAX_DIST);
                }

static void render_scene_runtime(PostFX *pfx, WaterReflect *reflect,
                                 Skybox *skybox, Terrain *terrain,
                                 PropList *props, ForestSystem *forest,
                                 PuddleSystem *puddles,
                                 FootprintSystem *footprints,
                                 GrassSystem *grass, WaterInteraction *wint,
                                 BirdSystem *birds, AnimalSystem *animals,
                                 WaterfallSystem *waterfalls, WeatherSystem *wx,
                                 FishingSystem *fishing, LineRenderer *lr,
                                 RoomRenderer *room_rend, HUD *hud,
                                 SkinProg *skin, ParticleSystem *ps,
                                 LightingSystem *ls,
                                 Camera3P *cam, Vec3 shake_off,
                                 Mat4 proj, Mat4 view, Vec3 fog_col,
                                 float total_time, float overcast,
                                 float wetness, float fog_density,
                                 WindSystem *wind, SceneID current_scene,
                                 int interior_idx,
                                 int frame_count, int win_w, int win_h,
                                 Inventory *inv,
                                 Player *player, CharModel *char_model,
                                 Enemy *enemies, int ne,
                                 CharModel enemy_models[],
                                 bool enemy_model_loaded[],
                                 CharModel npc_models[], bool npc_model_ok[],
                                 int has_npc_model, NpcSystem *npcs,
                                 CharModel *enemy_custom_models,
                                 int enemy_custom_count,
                                 bool *enemy_custom_ok){
    Uint64 render_begin=SDL_GetPerformanceCounter();
    float actors_ms=0.0f;
    float characters_ms=0.0f;
    float birds_ms=0.0f;
    float animals_ms=0.0f;
    bool render_shire=(current_scene==SCENE_SHIRE);
    bool render_room=(current_scene==SCENE_APARTMENT)||(interior_idx>=0);
    frame_debugger_gpu_begin(GPU_TIMER_RENDER_TOTAL,frame_count);
    LightingSystem room_ls_storage;
    LightingSystem *scene_ls=ls;
    if(render_room){
        room_ls_storage=*ls;
        room_ls_storage.ambient=(Vec3){0.012f,0.014f,0.018f};
        room_ls_storage.sun_color=vec3_scale(ls->sun_color,0.02f);
        scene_ls=&room_ls_storage;
    }
    bool want_reflect=reflect->ready&&render_shire&&reflection_should_render(cam,player->phys.pos);
    bool do_reflect_update=want_reflect&&reflection_should_refresh(reflect,cam,frame_count);
    if(do_reflect_update){
        Uint64 reflect_begin=SDL_GetPerformanceCounter();
        frame_debugger_gpu_begin(GPU_TIMER_REFLECTION,frame_count);
        float wl=WATER_LEVEL;
        Vec3 rcam={cam->pos.x+shake_off.x,2.0f*wl-(cam->pos.y+shake_off.y),cam->pos.z+shake_off.z};
        Vec3 rtgt={cam->target.x,2.0f*wl-cam->target.y,cam->target.z};
        Mat4 rview=mat4_lookat(rcam,rtgt,(Vec3){0,-1,0});
        glBindFramebuffer(GL_FRAMEBUFFER,reflect->fbo);
        glViewport(0,0,reflect->w,reflect->h);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glCullFace(GL_FRONT);
        if(render_shire){
            skybox_draw(skybox,proj,rview,scene_ls,overcast);
            terrain_draw_land(terrain,scene_ls,proj,rview,rcam,total_time,
                              wind->snow_accum,wetness,fog_density,fog_col);
        }
        glCullFace(GL_BACK);
        if(pfx->ready) glBindFramebuffer(GL_FRAMEBUFFER,pfx->hdr_fbo);
        else glBindFramebuffer(GL_FRAMEBUFFER,0);
        glViewport(0,0,win_w,win_h);
        frame_debugger_gpu_end(GPU_TIMER_REFLECTION);
        reflect->last_cam_pos=cam->pos;
        reflect->last_cam_target=cam->target;
        reflect->last_update_frame=frame_count;
        g_frame_debug.reflection_ms=frame_debugger_smooth(g_frame_debug.reflection_ms,
            perf_counter_ms(reflect_begin,SDL_GetPerformanceCounter()));
    }else{
        g_frame_debug.reflection_ms=frame_debugger_smooth(g_frame_debug.reflection_ms,0.0f);
        frame_debugger_gpu_set_zero(GPU_TIMER_REFLECTION);
    }

    {
        GLuint wp=terrain->water_prog;
        glUseProgram(wp);
        if(want_reflect){
            glActiveTexture(GL_TEXTURE8);
            glBindTexture(GL_TEXTURE_2D,reflect->tex);
            glUniform1i(glGetUniformLocation(wp,"uReflectTex"),8);
            glUniform1i(glGetUniformLocation(wp,"uHasReflect"),1);
        }else{
            glUniform1i(glGetUniformLocation(wp,"uHasReflect"),0);
        }
        glUniform2f(glGetUniformLocation(wp,"uScreenSize"),(float)win_w,(float)win_h);
        glUseProgram(0);
    }

    if(render_shire) skybox_draw(skybox,proj,view,scene_ls,overcast);
    if(render_shire){
        Uint64 pass_begin=SDL_GetPerformanceCounter();
        frame_debugger_gpu_begin(GPU_TIMER_TERRAIN,frame_count);
        terrain_draw_land(terrain,scene_ls,proj,view,cam->pos,total_time,
                          wind->snow_accum,wetness,fog_density,fog_col);
        frame_debugger_gpu_end(GPU_TIMER_TERRAIN);
        g_frame_debug.terrain_ms=frame_debugger_smooth(g_frame_debug.terrain_ms,
            perf_counter_ms(pass_begin,SDL_GetPerformanceCounter()));
        path_draw(scene_ls,proj,view,cam->pos,fog_density,fog_col);
        decals_draw(&g_decals,proj,view);
        pass_begin=SDL_GetPerformanceCounter();
        frame_debugger_gpu_begin(GPU_TIMER_PROPS,frame_count);
        props_draw(props,skin,scene_ls,proj,view,cam->pos,fog_density,fog_col);
        frame_debugger_gpu_end(GPU_TIMER_PROPS);
        g_frame_debug.props_ms=frame_debugger_smooth(g_frame_debug.props_ms,
            perf_counter_ms(pass_begin,SDL_GetPerformanceCounter()));
        puddle_draw(puddles,scene_ls,proj,view,cam->pos,total_time,wetness);
        footprint_draw(footprints,proj,view);
        /* Snapshot opaque scene color/depth after terrain/props and before water. */
        if(pfx->ready){
            glBindTexture(GL_TEXTURE_2D,pfx->scene_grab_tex);
            glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,win_w,win_h);
            glBindTexture(GL_TEXTURE_2D,0);
            g_scene_color_tex=pfx->scene_grab_tex;
            g_scene_depth_tex=pfx->hdr_depth_tex;
        }else{
            g_scene_color_tex=0;
            g_scene_depth_tex=0;
        }
        terrain_draw_water(terrain,scene_ls,proj,view,cam->pos,total_time,fog_density,fog_col);
        {
            pass_begin=SDL_GetPerformanceCounter();
            frame_debugger_gpu_begin(GPU_TIMER_GRASS,frame_count);
            float grass_wx=wind->wx, grass_wz=wind->wz, grass_total=wind->total, grass_gust=wind->gust;
            wind_sample_at(wind,player->phys.pos.x,player->phys.pos.z,&grass_wx,&grass_wz,&grass_total,&grass_gust);
            float dew=(scene_ls->day_frac>=0.18f&&scene_ls->day_frac<=0.30f)
                     ? (1.0f-clampf((scene_ls->day_frac-0.18f)/0.12f,0.0f,1.0f))
                     : 0.0f;
            grass_draw(grass,scene_ls,proj,view,player->phys.pos,cam->pos,total_time,
                       grass_wx,grass_wz,grass_total,grass_gust,wetness,dew,fog_density,fog_col,win_h);
            frame_debugger_gpu_end(GPU_TIMER_GRASS);
            g_frame_debug.grass_ms=frame_debugger_smooth(g_frame_debug.grass_ms,
                perf_counter_ms(pass_begin,SDL_GetPerformanceCounter()));
        }
        wint_draw(wint,scene_ls,proj,view,player->phys.pos,cam->pos,total_time);
        pass_begin=SDL_GetPerformanceCounter();
        frame_debugger_gpu_begin(GPU_TIMER_FOREST,frame_count);
        forest_draw(forest,props,scene_ls,proj,view,cam->pos,wind->snow_accum,fog_col,fog_density);
        frame_debugger_gpu_end(GPU_TIMER_FOREST);
        g_frame_debug.forest_ms=frame_debugger_smooth(g_frame_debug.forest_ms,
            perf_counter_ms(pass_begin,SDL_GetPerformanceCounter()));
        pass_begin=SDL_GetPerformanceCounter();
        frame_debugger_gpu_begin(GPU_TIMER_BIRDS,frame_count);
        bird_system_draw(birds,skin,proj,view,cam->pos,scene_ls);
        frame_debugger_gpu_end(GPU_TIMER_BIRDS);
        birds_ms+=perf_counter_ms(pass_begin,SDL_GetPerformanceCounter());
        pass_begin=SDL_GetPerformanceCounter();
        frame_debugger_gpu_begin(GPU_TIMER_ANIMALS,frame_count);
        animal_system_draw(animals,skin,proj,view,cam->pos,scene_ls);
        frame_debugger_gpu_end(GPU_TIMER_ANIMALS);
        animals_ms+=perf_counter_ms(pass_begin,SDL_GetPerformanceCounter());
        mount_system_draw_world(&g_mounts,skin,proj,view,cam->pos,scene_ls);
    }else if(render_room){
        interior_draw_room(room_rend,hud,win_w,win_h,scene_ls,proj,view);
        {
            Uint64 pass_begin=SDL_GetPerformanceCounter();
            frame_debugger_gpu_begin(GPU_TIMER_PROPS,frame_count);
            props_draw(props,skin,scene_ls,proj,view,cam->pos,fog_density,fog_col);
            frame_debugger_gpu_end(GPU_TIMER_PROPS);
            g_frame_debug.props_ms=frame_debugger_smooth(g_frame_debug.props_ms,
                perf_counter_ms(pass_begin,SDL_GetPerformanceCounter()));
        }
        g_frame_debug.terrain_ms=frame_debugger_smooth(g_frame_debug.terrain_ms,0.0f);
        frame_debugger_gpu_set_zero(GPU_TIMER_TERRAIN);
        g_frame_debug.grass_ms=frame_debugger_smooth(g_frame_debug.grass_ms,0.0f);
        frame_debugger_gpu_set_zero(GPU_TIMER_GRASS);
        g_frame_debug.forest_ms=frame_debugger_smooth(g_frame_debug.forest_ms,0.0f);
        frame_debugger_gpu_set_zero(GPU_TIMER_FOREST);
        g_frame_debug.characters_ms=frame_debugger_smooth(g_frame_debug.characters_ms,0.0f);
        frame_debugger_gpu_set_zero(GPU_TIMER_CHARACTERS);
        g_frame_debug.birds_ms=frame_debugger_smooth(g_frame_debug.birds_ms,0.0f);
        frame_debugger_gpu_set_zero(GPU_TIMER_BIRDS);
        g_frame_debug.animals_ms=frame_debugger_smooth(g_frame_debug.animals_ms,0.0f);
        frame_debugger_gpu_set_zero(GPU_TIMER_ANIMALS);
        g_frame_debug.actors_ms=frame_debugger_smooth(g_frame_debug.actors_ms,0.0f);
        frame_debugger_gpu_set_zero(GPU_TIMER_ACTORS);
    }else{
        g_frame_debug.terrain_ms=frame_debugger_smooth(g_frame_debug.terrain_ms,0.0f);
        frame_debugger_gpu_set_zero(GPU_TIMER_TERRAIN);
        g_frame_debug.props_ms=frame_debugger_smooth(g_frame_debug.props_ms,0.0f);
        frame_debugger_gpu_set_zero(GPU_TIMER_PROPS);
        g_frame_debug.grass_ms=frame_debugger_smooth(g_frame_debug.grass_ms,0.0f);
        frame_debugger_gpu_set_zero(GPU_TIMER_GRASS);
        g_frame_debug.forest_ms=frame_debugger_smooth(g_frame_debug.forest_ms,0.0f);
        frame_debugger_gpu_set_zero(GPU_TIMER_FOREST);
        g_frame_debug.characters_ms=frame_debugger_smooth(g_frame_debug.characters_ms,0.0f);
        frame_debugger_gpu_set_zero(GPU_TIMER_CHARACTERS);
        g_frame_debug.birds_ms=frame_debugger_smooth(g_frame_debug.birds_ms,0.0f);
        frame_debugger_gpu_set_zero(GPU_TIMER_BIRDS);
        g_frame_debug.animals_ms=frame_debugger_smooth(g_frame_debug.animals_ms,0.0f);
        frame_debugger_gpu_set_zero(GPU_TIMER_ANIMALS);
        g_frame_debug.actors_ms=frame_debugger_smooth(g_frame_debug.actors_ms,0.0f);
        frame_debugger_gpu_set_zero(GPU_TIMER_ACTORS);
    }

    {
        Uint64 pass_begin=SDL_GetPerformanceCounter();
        frame_debugger_gpu_begin(GPU_TIMER_CHARACTERS,frame_count);
        render_character_runtime(skin,proj,view,cam->pos,scene_ls,inv,current_scene,player,char_model,
                             enemies,ne,enemy_models,enemy_model_loaded,
                             npc_models,npc_model_ok,frame_count,npcs,
                             has_npc_model,
                             enemy_custom_models,enemy_custom_count,enemy_custom_ok);
        frame_debugger_gpu_end(GPU_TIMER_CHARACTERS);
        characters_ms+=perf_counter_ms(pass_begin,SDL_GetPerformanceCounter());
    }
    actors_ms=characters_ms+birds_ms+animals_ms;
    g_frame_debug.characters_ms=frame_debugger_smooth(g_frame_debug.characters_ms,characters_ms);
    g_frame_debug.birds_ms=frame_debugger_smooth(g_frame_debug.birds_ms,birds_ms);
    g_frame_debug.animals_ms=frame_debugger_smooth(g_frame_debug.animals_ms,animals_ms);
    g_frame_debug.actors_ms=frame_debugger_smooth(g_frame_debug.actors_ms,actors_ms);
    g_frame_debug.actors_gpu_ms=g_frame_debug.characters_gpu_ms+g_frame_debug.birds_gpu_ms+g_frame_debug.animals_gpu_ms;

    {
        Uint64 pass_begin=SDL_GetPerformanceCounter();
        frame_debugger_gpu_begin(GPU_TIMER_PARTICLES,frame_count);
    ps_draw(ps,proj,view);
        frame_debugger_gpu_end(GPU_TIMER_PARTICLES);
        g_frame_debug.particles_ms=frame_debugger_smooth(g_frame_debug.particles_ms,
            perf_counter_ms(pass_begin,SDL_GetPerformanceCounter()));
    }
    if(render_shire) wf_draw(waterfalls,proj,view,total_time);
    {
        Uint64 pass_begin=SDL_GetPerformanceCounter();
        frame_debugger_gpu_begin(GPU_TIMER_WEATHER,frame_count);
    if(render_shire) wx_draw(wx,proj,view);
        frame_debugger_gpu_end(GPU_TIMER_WEATHER);
        g_frame_debug.weather_ms=frame_debugger_smooth(g_frame_debug.weather_ms,
            perf_counter_ms(pass_begin,SDL_GetPerformanceCounter()));
    }
    if(render_shire) fishing_world_draw(fishing,lr,proj,view,player->phys.pos);
    surface_pieces_draw(&g_surface_rend,&g_surfaces,scene_ls,proj,view);
    if(pfx->ready){
        /* Project sun position to screen UV for god rays */
        {
            /* sun is at cam_pos - sun_dir * large_dist (sun_dir points toward scene) */
            float sx=cam->pos.x-scene_ls->sun_dir.x*4000.0f;
            float sy=cam->pos.y-scene_ls->sun_dir.y*4000.0f;
            float sz=cam->pos.z-scene_ls->sun_dir.z*4000.0f;
            /* view transform */
            float vx=view.m[0]*sx+view.m[4]*sy+view.m[8]*sz+view.m[12];
            float vy=view.m[1]*sx+view.m[5]*sy+view.m[9]*sz+view.m[13];
            float vz=view.m[2]*sx+view.m[6]*sy+view.m[10]*sz+view.m[14];
            float vw=view.m[3]*sx+view.m[7]*sy+view.m[11]*sz+view.m[15];
            /* proj transform */
            float cx=proj.m[0]*vx+proj.m[4]*vy+proj.m[8]*vz+proj.m[12]*vw;
            float cy=proj.m[1]*vx+proj.m[5]*vy+proj.m[9]*vz+proj.m[13]*vw;
            float cw=proj.m[3]*vx+proj.m[7]*vy+proj.m[11]*vz+proj.m[15]*vw;
            if(cw>0.0f){
                pfx->sun_screen_x=cx/cw*0.5f+0.5f;
                pfx->sun_screen_y=cy/cw*0.5f+0.5f;
            }
            pfx->sun_col_r=scene_ls->sun_color.x;
            pfx->sun_col_g=scene_ls->sun_color.y;
            pfx->sun_col_b=scene_ls->sun_color.z;
        }
        Uint64 pass_begin=SDL_GetPerformanceCounter();
        frame_debugger_gpu_begin(GPU_TIMER_POSTFX,frame_count);
        pfx_composite(pfx,win_w,win_h,scene_ls->sun_elev,total_time);
        frame_debugger_gpu_end(GPU_TIMER_POSTFX);
        g_frame_debug.postfx_ms=frame_debugger_smooth(g_frame_debug.postfx_ms,
            perf_counter_ms(pass_begin,SDL_GetPerformanceCounter()));
    }else{
        g_frame_debug.postfx_ms=frame_debugger_smooth(g_frame_debug.postfx_ms,0.0f);
        frame_debugger_gpu_set_zero(GPU_TIMER_POSTFX);
    }
    frame_debugger_gpu_end(GPU_TIMER_RENDER_TOTAL);
    g_engine_perf.raw_render_cpu_ms=perf_counter_ms(render_begin,SDL_GetPerformanceCounter());
    g_frame_debug.render_total_ms=frame_debugger_smooth(g_frame_debug.render_total_ms,g_engine_perf.raw_render_cpu_ms);
}

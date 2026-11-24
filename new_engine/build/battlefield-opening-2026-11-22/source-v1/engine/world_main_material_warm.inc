    // The probe target has one RGB16F output and the reflection target has one
    // RGBA16F output. Neither prepares the normal two-RGBA16F fragment exports,
    // procedural-grass input layout or actor pose inputs in the shadow pass.
    // Exercise those real draws while loading, without touching temporal state.
    struct MainMaterialWarmStats {
        bool ready=false;
        unsigned calls=0,passes=0,startup_calls=0,destination_calls=0,failures=0;
        unsigned terrain_passes=0,static_passes=0,grass_passes=0,scanned_passes=0,near_tree_passes=0,pose_passes=0;
        double milliseconds=0;
    } main_material_warm;
    auto warm_main_materials=[&](const Camera &destination,float visual_time,bool startup_loading)->bool {
        // Call sites are deliberately confined to initial loading and the
        // ready-to-publish destination transition. No ordinary draw calls here.
        const Uint64 begin=SDL_GetPerformanceCounter();
        if(!startup_loading&&!destination_loading) {
            main_material_warm.ready=false;main_material_warm.failures++;
            std::fprintf(stderr,"[World] Refusing material preparation outside loading\n");
            return false;
        }
        main_material_warm.calls++;
        if(startup_loading)main_material_warm.startup_calls++;
        else main_material_warm.destination_calls++;
        main_material_warm.ready=false;
        const unsigned first_pass=main_material_warm.passes;
        struct State {
            GLint draw=0,read=0,program=0,vao=0,array=0,active=0,viewport[4]={},buffers[2]={};
            GLint depth_function=GL_LESS,cull_mode=GL_BACK,polygon[2]={GL_FILL,GL_FILL};
            GLfloat offset_factor=0,offset_units=0;
            GLboolean depth=0,cull=0,blend=0,scissor=0,offset=0,seamless=0,depth_write=0,color_write[4]={};
            struct TextureUnits {GLint image=0,array=0,volume=0,cube=0,sampler=0;};
            std::array<TextureUnits,18> textures={};
            unsigned texture_count=0;
            State() {
                GLint available_texture_units=0;glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,&available_texture_units);
                texture_count=std::min(18u,unsigned(std::max(0,available_texture_units)));
                glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
                glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);
                glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&array);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
                glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_DRAW_BUFFER0,&buffers[0]);glGetIntegerv(GL_DRAW_BUFFER1,&buffers[1]);
                glGetIntegerv(GL_DEPTH_FUNC,&depth_function);glGetIntegerv(GL_CULL_FACE_MODE,&cull_mode);glGetIntegerv(GL_POLYGON_MODE,polygon);
                glGetFloatv(GL_POLYGON_OFFSET_FACTOR,&offset_factor);glGetFloatv(GL_POLYGON_OFFSET_UNITS,&offset_units);
                depth=glIsEnabled(GL_DEPTH_TEST);cull=glIsEnabled(GL_CULL_FACE);blend=glIsEnabled(GL_BLEND);
                scissor=glIsEnabled(GL_SCISSOR_TEST);offset=glIsEnabled(GL_POLYGON_OFFSET_FILL);seamless=glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS);
                glGetBooleanv(GL_DEPTH_WRITEMASK,&depth_write);glGetBooleanv(GL_COLOR_WRITEMASK,color_write);
                for(unsigned i=0;i<texture_count;i++) {
                    glActiveTexture(GL_TEXTURE0+i);auto &t=textures[i];
                    glGetIntegerv(GL_TEXTURE_BINDING_2D,&t.image);glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY,&t.array);
                    glGetIntegerv(GL_TEXTURE_BINDING_3D,&t.volume);glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP,&t.cube);
                    glGetIntegeri_v(GL_SAMPLER_BINDING,i,&t.sampler);
                }
                glActiveTexture(GLenum(active));
            }
            void restore() const {
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(draw));glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(read));
                if(draw) {const GLenum b[]={GLenum(buffers[0]),GLenum(buffers[1])};glDrawBuffers(2,b);}
                else glDrawBuffer(GLenum(buffers[0]));
                glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
                for(unsigned i=0;i<texture_count;i++) {
                    glActiveTexture(GL_TEXTURE0+i);const auto &t=textures[i];
                    glBindTexture(GL_TEXTURE_2D,GLuint(t.image));glBindTexture(GL_TEXTURE_2D_ARRAY,GLuint(t.array));
                    glBindTexture(GL_TEXTURE_3D,GLuint(t.volume));glBindTexture(GL_TEXTURE_CUBE_MAP,GLuint(t.cube));glBindSampler(i,GLuint(t.sampler));
                }
                glActiveTexture(GLenum(active));glBindVertexArray(GLuint(vao));glBindBuffer(GL_ARRAY_BUFFER,GLuint(array));glUseProgram(GLuint(program));
                auto enable=[](GLenum cap,GLboolean value){if(value)glEnable(cap);else glDisable(cap);};
                enable(GL_DEPTH_TEST,depth);enable(GL_CULL_FACE,cull);enable(GL_BLEND,blend);enable(GL_SCISSOR_TEST,scissor);
                enable(GL_POLYGON_OFFSET_FILL,offset);enable(GL_TEXTURE_CUBE_MAP_SEAMLESS,seamless);
                glDepthFunc(GLenum(depth_function));glCullFace(GLenum(cull_mode));glDepthMask(depth_write);
                glColorMask(color_write[0],color_write[1],color_write[2],color_write[3]);
                glPolygonOffset(offset_factor,offset_units);glPolygonMode(GL_FRONT_AND_BACK,GLenum(polygon[0]));
            }
        } state;
        // Fail on a pre-existing GL error too: loading must not hide an earlier
        // renderer failure by draining errors before it performs its own work.
        GLenum error=glGetError();
        bool ready=target.fbo&&target.indirect&&target.w>0&&target.h>0&&error==GL_NO_ERROR;
        const Camera saved_selection=dressing.selection_camera;
        const bool saved_has_selection=dressing.has_selection;
        const float weather_weight=weather_disabled?0.f:world_atlas_rainforest_weight(destination.x,destination.z);
        const float wx=1.1f*wind*weather_weight,wz=.38f*wind*weather_weight;
        const float wetness=std::clamp((.20f+.64f*rain)*weather_weight,0.f,1.f);
        const Lighting light=local_lighting(hour,cloud,visual_time,world_atlas_rainforest_weight(destination.x,destination.z),bearing,sun_strength,sky_fill);
        WorldTorchLights torches;
        const bool camp=std::hypot(destination.x-bandit_camp_center_x,destination.z-bandit_camp_center_z)<300.;
        ready=ready&&(camp?world_torch_lights_build(bandit_torch_emitters,qa_bandit_lights_off?0:std::size(bandit_torch_emitters),
            bandit_camp_center_x,bandit_camp_center_z,destination.x,destination.y,destination.z,visual_time,torches):
            world_torch_lights_build(town_torch_emitters.data(),city_showcase?0:town_torch_emitters.size(),creek_origin_x,creek_origin_z,
                destination.x,destination.y,destination.z,visual_time,torches));
        M4 lightvp={};const double eye[]={destination.x,destination.y,destination.z};
        const float sun[]={light.sun.x,light.sun.y,light.sun.z},center[]={0,-25,0};
        ready=ready&&world_shadow_directional_matrix(eye,sun,160.f,shadow.side,center,lightvp.m);
        auto prepare_land=[&](const Camera &c) {
            land.use();light_program(land,light,c);torch_program(land,torches);bind_local_probes(land,c);
            weather_program(land,wx,wz,wetness);land.i("uClipBelowWater",0);land.f("uWaterClipLevel",0);
            land.v2("uWorldSize",float(world_atlas_width()),float(world_atlas_depth()));
            land.v3("uEye",{float(c.x),float(c.y),float(c.z)});land.m4("uLightVP",lightvp);
            const char *names[]={"uGrass","uRock","uSand","uDirt","uGrassNormal","uRockNormal","uClimate","uShadow","uGrassARM","uRockARM"};
            for(int i=0;i<10;i++){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,i==7?shadow.depth:materials[i]);land.i(names[i],i);}
            glActiveTexture(GL_TEXTURE11);glBindTexture(GL_TEXTURE_2D,macro_height);land.i("uMacroHeight",11);landscape.bind(land);
        };
        auto prepare_foliage_program=[&](Program &program,const Camera &c,const M4 &vp) {
            program.use();program.i("uArchitectureWeathering",1);program.i("uShadowGather",shadow_gather_enabled?1:0);program.i("uFacadeMode",0);landscape.bind_shelter(program);
            program.i("uColorCoverageCull",zero_coverage_cull_enabled?1:0);
            studio_surface_gpu.bind(program,c.x,c.y,c.z);
            program.i("uClipBelowWater",0);program.f("uWaterClipLevel",0);program.m4("uVP",vp);program.m4("uLightVP",lightvp);
            light_program(program,light,c);torch_program(program,torches);bind_local_probes(program,c);weather_program(program,wx,wz,wetness);
            program.f("uTime",visual_time);program.f("uTemporalPhase",0);program.i("uWindHistoryEnabled",1);program.f("uWindFrameDelta",qa_fixed_time?0.f:1.f/60.f);
            program.f("uActorReactive",0);program.f("uPixelScale",float(target.h)/(2*std::tan(.54f)));
            program.v2("uWorldSize",float(world_atlas_width()),float(world_atlas_depth()));
            glActiveTexture(GL_TEXTURE7);glBindTexture(GL_TEXTURE_2D,shadow.depth);program.i("uShadow",7);
            glActiveTexture(GL_TEXTURE11);glBindTexture(GL_TEXTURE_2D,macro_height);program.i("uMacroHeight",11);
        };
        auto prepare_foliage=[&](const Camera &c,const M4 &vp){prepare_foliage_program(foliage,c,vp);};
        for(unsigned direction=0;ready&&direction<4;direction++) {
            Camera warm=destination;warm.yaw=destination.yaw+direction*1.57079632679f;warm.pitch=-.08f;
            dressing.select(warm,false,false,false);
            world_scenery_update(scenery,warm.x,warm.y,warm.z,warm.yaw,warm.pitch,warm.aspect);
            const M4 vp=product(perspective(1.08f,warm.aspect,.4f,15000.f),look({0,0,0},warm.forward(),{0,1,0}));
            glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);
            glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
            glBindFramebuffer(GL_FRAMEBUFFER,shadow.fbo);glViewport(0,0,shadow.side,shadow.side);glClear(GL_DEPTH_BUFFER_BIT);
            glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(2,3);
            {
            WorldModelDepthMaterialScope depth_materials(foliageShadow.id);
            if(wildlife_preparation_enabled&&world_wildlife_stats(wildlife).pose_assets) {
                glDisable(GL_CULL_FACE);foliageShadow.use();foliageShadow.m4("uVP",lightvp);
                foliageShadow.f("uTime",visual_time);weather_program(foliageShadow,wx,wz,wetness);
                WorldWildlifePrepareFrame prep;
                prep.program=foliageShadow.id;prep.pass=2;prep.x=warm.x;prep.y=warm.y;prep.z=warm.z;
                prep.yaw=warm.yaw;prep.pitch=warm.pitch;prep.loading=true;
                ready=world_wildlife_prepare(wildlife,prep);
                glClear(GL_DEPTH_BUFFER_BIT);glEnable(GL_CULL_FACE);
                if(!ready)break;
            }
            terrain.draw(shadow.prog,warm,lightvp,true);glDisable(GL_CULL_FACE);
            foliageShadow.use();foliageShadow.m4("uVP",lightvp);foliageShadow.f("uTime",visual_time);weather_program(foliageShadow,wx,wz,wetness);
            dressing.draw(foliageShadow,warm,true);outcrops.draw(foliageShadow,warm);
            world_scenery_shadow_clip(scenery,authored_shadow_clip_enabled?lightvp.m:nullptr);
            world_scenery_draw(scenery,foliageShadow.id,warm.x,warm.y,warm.z,true);
            world_scenery_shadow_clip(scenery,nullptr);
            {auto &p=live_programs.crowd_shadow;p.use();p.m4("uVP",lightvp);p.f("uTime",visual_time);weather_program(p,wx,wz,wetness);WorldModelDepthMaterialScope character_depth(p.id);world_village_population_draw(population,p.id,warm.x,warm.y,warm.z,true);foliageShadow.use();}
            world_trail_camp_draw(trail_camp,foliageShadow.id,warm.x,warm.y,warm.z,warm.yaw,warm.pitch,warm.aspect,2);
            world_castle_dragons_draw(dragons,foliageShadow.id,warm.x,warm.y,warm.z,warm.yaw,warm.pitch,warm.aspect,2);
            world_castle_training_draw(training,foliageShadow.id,warm.x,warm.y,warm.z,true);
            world_village_watch_draw(watch,foliageShadow.id,warm.x,warm.y,warm.z,true);
            world_village_pets_draw(pets,foliageShadow.id,warm.x,warm.y,warm.z,true);
            ready=draw_live_shadow(warm,lightvp,visual_time)&&ready;
            }
            glDisable(GL_POLYGON_OFFSET_FILL);
            glBindFramebuffer(GL_FRAMEBUFFER,target.fbo);glViewport(0,0,target.w,target.h);
            const GLenum buffers[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};glDrawBuffers(2,buffers);
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glEnable(GL_CULL_FACE);
            if(wildlife_preparation_enabled&&world_wildlife_stats(wildlife).pose_assets) {
                glDisable(GL_CULL_FACE);prepare_foliage(warm,vp);
                WorldWildlifePrepareFrame prep;
                prep.program=foliage.id;prep.pass=0;prep.x=warm.x;prep.y=warm.y;prep.z=warm.z;
                prep.yaw=warm.yaw;prep.pitch=warm.pitch;prep.loading=true;
                ready=world_wildlife_prepare(wildlife,prep);
                // Temporary actors never enter the retained loading image.
                glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glEnable(GL_CULL_FACE);
                if(!ready)break;
            }
            prepare_land(warm);showcase_terrain.bind(land.id);terrain.draw(land,warm,vp);
            glDisable(GL_CULL_FACE);
            if(foliage.generated_grass&&dressing.scanned_grass_count)prepare_foliage_program(*foliage.generated_grass,warm,vp);
            prepare_foliage(warm,vp);
            dressing.draw(foliage,warm,false,foliage.generated_grass.get(),false);outcrops.draw(foliage,warm);horizon.draw(foliage,warm);
            foliage.i("uSourceMaterialAO",city_showcase?1:0);
            world_scenery_draw(scenery,foliage.id,warm.x,warm.y,warm.z);
            foliage.i("uSourceMaterialAO",0);
            // Snapshot draws do not advance resident animation or increment
            // the regular frustum-submission counters.
            prepare_foliage_program(live_programs.crowd,warm,vp);world_village_population_draw(population,live_programs.crowd.id,warm.x,warm.y,warm.z);foliage.use();
            world_trail_camp_draw(trail_camp,foliage.id,warm.x,warm.y,warm.z,warm.yaw,warm.pitch,warm.aspect);
            world_castle_dragons_draw(dragons,foliage.id,warm.x,warm.y,warm.z,warm.yaw,warm.pitch,warm.aspect);
            world_castle_training_draw(training,foliage.id,warm.x,warm.y,warm.z);
            world_village_watch_draw(watch,foliage.id,warm.x,warm.y,warm.z);
            world_village_pets_draw(pets,foliage.id,warm.x,warm.y,warm.z);
            ready=draw_live_color(warm,vp,lightvp,light,torches,visual_time,wetness,false,0)&&ready;
            const auto warm_water_detail=world_water_quality_profile(graphics.water_quality);
            if(!startup_loading&&wildlife_preparation_enabled&&world_wildlife_stats(wildlife).pose_assets&&graphics.reflections&&warm_water_detail.planar) {
                const auto *pool=world_atlas_rainforest_pool();
                const bool near_pool=std::hypot(warm.x-pool->x,warm.z-pool->z)<220.;
                if(near_pool||castle_creek_nearby(warm)||world_atlas_village_weight(warm.x,warm.z)>0.f) {
                    const float mirror_scale=std::min(graphics.reflection_scale,warm_water_detail.mirror_scale_cap);
                    ready=reflection.resize(std::max(1,int(std::lround(target.w*mirror_scale))),
                        std::max(1,int(std::lround(target.h*mirror_scale))));
                    M4 reflected={};
                    if(ready)ready=draw_water_reflection(warm,light,torches,lightvp,visual_time,
                        near_pool?float(pool->surface_y):float(WORLD_CASTLE_CREEK_SURFACE_Y),
                        wx,wz,wetness,0,reflected,false,0);
                    if(!ready)break;
                }
            }
            if(ready&&dragon_breath){
                const V3 forward=warm.forward(),right=warm.right(),up=norm(cross(right,forward));
                const auto f=dragon_breath_frame(warm,light,right,up,forward,vp,target.color,target.depth,target.w,target.h,visual_time,wx,wz);
                ready=world_dragon_breath_prepare(dragon_breath,f,true);
            }
            error=glGetError();ready=ready&&error==GL_NO_ERROR;
            if(ready) {
                main_material_warm.passes++;main_material_warm.terrain_passes++;
                main_material_warm.static_passes+=world_scenery_stats(scenery).color_instances>0;
                main_material_warm.grass_passes+=dressing.grass.blades>0;
                main_material_warm.scanned_passes+=dressing.scanned_grass_count>0;
                main_material_warm.near_tree_passes+=dressing.near_tree_count>0;
                main_material_warm.pose_passes+=world_village_population_stats(population).draw_groups>0||
                    world_castle_training_stats(training).visible>0||world_village_watch_stats(watch).visible>0||
                    world_village_pets_stats(pets).draw_groups>0;
            }
        }
        // Restore every selection mutated by the preview without advancing the
        // simulation or cover fades. The next ordinary draw uses the same VBOs
        // and camera it would have used without a loading preview.
        dressing.select(saved_has_selection?saved_selection:destination,false,false,false);
        dressing.selection_camera=saved_selection;dressing.has_selection=saved_has_selection;
        world_scenery_update(scenery,destination.x,destination.y,destination.z,destination.yaw,destination.pitch,destination.aspect);
        if(ready) {
            const M4 vp=product(perspective(1.08f,destination.aspect,.4f,15000.f),look({0,0,0},destination.forward(),{0,1,0}));
            prepare_land(destination);land.m4("uVP",vp);prepare_foliage(destination,vp);
            foliageShadow.use();foliageShadow.m4("uVP",lightvp);foliageShadow.f("uTime",visual_time);weather_program(foliageShadow,wx,wz,wetness);
        }
        state.restore();
        // This is a loading boundary, never a ready walking-frame fence.
        glFinish();const GLenum restore_error=glGetError();
        if(error==GL_NO_ERROR)error=restore_error;
        ready=restore_error==GL_NO_ERROR&&ready;
        // Encoded equality keys are cold after loading. Keep their exact
        // bytes on the unlinked backing file, releasing hundreds of MiB of
        // CPU working pages before ordinary play. GPU textures are untouched.
        (void)texture_cache_trim_source_pages();
        main_material_warm.milliseconds+=millis(begin);main_material_warm.ready=ready;
        if(!ready)main_material_warm.failures++;
        std::printf("[World] Main material layouts %s: %s, %u/4 headings, %.3f ms, GL0x%x (loading only)\n",
            startup_loading?"startup":"destination",ready?"ready":"FAILED",main_material_warm.passes-first_pass,millis(begin),unsigned(error));
        return ready;
    };

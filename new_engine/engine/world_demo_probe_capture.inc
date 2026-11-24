    // Static local radiance is captured only while a destination is loading.
    // Each showcase region retains its own small cache on return trips. Keep
    // the original village/rainforest banks separate from the mountain town.
    constexpr unsigned local_probe_region_count=4;
    std::array<WorldLocalProbes*,local_probe_region_count> local_probe_regions={};
    std::array<float,local_probe_region_count> local_probe_hours={},local_probe_bearings={};
    std::array<std::array<float,3>,local_probe_region_count> local_probe_looks={};
    std::array<bool,local_probe_region_count> local_probe_clouds={};
    std::array<double,3> mountain_probe_destination={};
    unsigned local_probe_loading_bakes=0;
    const bool local_probes_enabled=std::getenv("VALWOOD_QA_DISABLE_LOCAL_PROBES")==nullptr;
    auto probe_region=[&](const Camera &c) {
        if(world_opening_interior::contains(opening_interior,{c.x,c.y,c.z}))return 3;
        if(city_showcase)return 2;
        if(world_atlas_village_weight(c.x,c.z)>0.f)return 0;
        if(world_atlas_rainforest_weight(c.x,c.z)>0.f)return 1;
        if(stronghold_finish&&std::hypot(c.x-stronghold_x,c.z-stronghold_z)<1400.)return 2;
        return -1;
    };
    auto bind_local_probes=[&](Program &program,const Camera &c,bool allow=true) {
        // Active sampler types must have distinct units even with count zero.
        program.i("uLocalProbe0",4);program.i("uLocalProbe1",5);
        program.i("uLocalProbeCount",0);program.f("uLocalIrradianceWeight",0);
        program.f("uLocalIrradianceSecondaryWeight",0);
        const int region=probe_region(c);
        const double eye[]={c.x,c.y,c.z};
        if(region<0||!local_probe_regions[region]) {
            // Retain complete sampler formats outside cached showcase regions;
            // disabled binding leaves counts and both SH weights exactly zero.
            for(auto *cached:local_probe_regions)if(cached) {
                world_local_probes_bind(cached,program.id,eye,false);
                break;
            }
            return;
        }
        const bool matching_light=std::fabs(hour-local_probe_hours[region])<.05f&&
            std::fabs(bearing-local_probe_bearings[region])<.5f&&
            std::fabs(cloud-local_probe_looks[region][0])<.01f&&
            std::fabs(sun_strength-local_probe_looks[region][1])<.01f&&
            std::fabs(sky_fill-local_probe_looks[region][2])<.01f&&graphics.clouds==local_probe_clouds[region];
        world_local_probes_bind(local_probe_regions[region],program.id,eye,
            local_probes_enabled&&allow&&matching_light);
    };
    auto ensure_local_probes=[&](const Camera &destination)->bool {
        const int region=probe_region(destination);
        if(!local_probes_enabled||region<0)return true;
        if(local_probe_regions[region]) {
            if(region!=2||(std::hypot(destination.x-mountain_probe_destination[0],
                    destination.z-mountain_probe_destination[2])<48.&&
                    std::abs(destination.y-mountain_probe_destination[1])<24.))return true;
            // This function runs only during loading, never while walking.
            // A new showcase destination gets a local capture without placing
            // source-scene rooms into the previous oversized hall's bounds.
            world_local_probes_free(local_probe_regions[region]);local_probe_regions[region]=nullptr;
        }
        const Uint64 begin=SDL_GetPerformanceCounter();
        std::array<WorldLocalProbeDef,WORLD_LOCAL_PROBE_LIMIT> defs={};size_t count=0;
        auto add=[&](double x,double y,double z,double xmin,double xmax,double zmin,double zmax,
                float ymin,float ymax,bool interior=false) {
            auto &d=defs[count++];d.position[0]=x;d.position[1]=y;d.position[2]=z;
            d.box_min[0]=float(xmin);d.box_min[1]=ymin;d.box_min[2]=float(zmin);
            d.box_max[0]=float(xmax);d.box_max[1]=ymax;d.box_max[2]=float(zmax);d.interior=interior;
        };
        if(region==3){
            for(const auto &room:opening_interior.probes){
                auto &d=defs[count++];
                for(unsigned k=0;k<3;k++){d.position[k]=room.position[k];d.box_min[k]=float(room.box.minimum[k]);d.box_max[k]=float(room.box.maximum[k]);}
                d.interior=true;
            }
            std::printf("[OpeningInterior] Capturing %zu room-bounded probes during loading\n",count);
        } else if(region==0) {
            auto town=[&](double x,double z,double xmin,double xmax,double zmin,double zmax,bool interior=false) {
                const double wx=creek_origin_x+x,wz=creek_origin_z+z;
                const float ground=world_atlas_walk_height(wx,wz);
                add(wx,ground+1.9,wz,creek_origin_x+xmin,creek_origin_x+xmax,
                    creek_origin_z+zmin,creek_origin_z+zmax,ground-2.5f,ground+(interior?4.5f:18.f),interior);
            };
            town(-68,34,-95,-40,-40,75);
            town(-130,55,-145,-115,-30,115);
            town(-145,-82,-153,-137,-90,-74,true);
            town(-145,-60,-163,-126,-73,-43);
            town(95,43,80,118,-35,120);
            town(26.4,-79.41525254542,18.4,34.4,-87.41525254542,-71.41525254542,true);
        } else if(region==1) {
            const auto *pool=world_atlas_rainforest_pool();
            const double px=pool->x-3,pz=pool->z-25;
            const double ux=(.23499007936508-.5)*world_atlas_width();
            const double uz=(.67123437623216-.5)*world_atlas_depth();
            for(auto point:std::array<std::array<double,2>,2>{{{{px,pz}},{{ux,uz}}}}) {
                const float ground=world_atlas_walk_height(point[0],point[1]);
                add(point[0],ground+2.5,point[1],point[0]-65,point[0]+65,point[1]-65,point[1]+65,
                    ground-5,ground+30);
            }
        } else {
            // Keep the source showcase's scale/layout independent of the old
            // hall. Capture at this destination plus clear nearby positions;
            // do not guess which source assemblies form an interior volume.
            auto mountain=[&](double x,double z,double y) {
                add(x,y,z,x-60,x+60,z-60,z+60,float(y-32),float(y+48));
            };
            mountain(destination.x,destination.z,destination.y);
            for(auto offset:std::array<std::array<double,2>,4>{{{{-12,0}},{{12,0}},{{0,-12}},{{0,12}}}}) {
                const double x=destination.x+offset[0],z=destination.z+offset[1];
                const double ground=world_atlas_walk_height(x,z);
                const double y=std::max(double(destination.y),ground+2.5);
                if(!std::isfinite(y)||std::abs(y-destination.y)>6.)continue;
                // Avoid baking from inside a wall or on its far side. At
                // elevated review views this tests the same capture height,
                // while low views remain above their actual terrain support.
                if(!world_scenery_can_move(scenery,x,z,x,z,float(y-.2),.4f,.2f)||
                        !world_scenery_can_move(scenery,destination.x,destination.z,x,z,
                            float(y-.2),.4f,.2f))continue;
                mountain(x,z,y);
            }
            mountain_probe_destination={destination.x,destination.y,destination.z};
            std::printf("[Grandstone] Local radiance domains: %zu destination/clear-neighbor probes at %.2f %.2f %.2f\n",
                count,destination.x,destination.y,destination.z);
        }
        auto *probes=world_local_probes_create(defs.data(),count,128);
        if(!probes)return false;
        local_probe_regions[region]=probes;
        local_probe_hours[region]=hour;local_probe_bearings[region]=bearing;
        local_probe_looks[region]={cloud,sun_strength,sky_fill};local_probe_clouds[region]=graphics.clouds;
        if(!world_local_probes_register_program(probes,land.id)||
           !world_local_probes_register_program(probes,foliage.id)||
           !world_local_probes_register_program(probes,live_programs.skeletal.id)||!world_local_probes_register_program(probes,live_programs.rigid.id))return false;
        auto capture=[&](const WorldLocalProbeCapture &face)->bool {
            Camera eye=destination;eye.x=face.eye[0];eye.y=face.eye[1];eye.z=face.eye[2];eye.aspect=1;
            eye.yaw=std::atan2(face.forward[0],-face.forward[2]);eye.pitch=std::asin(face.forward[1]);
            const V3 forward={face.forward[0],face.forward[1],face.forward[2]};
            const V3 right={face.right[0],face.right[1],face.right[2]},up={face.up[0],face.up[1],face.up[2]};
            const float time=22.f;
            const Lighting light=local_lighting(hour,cloud,time,world_atlas_rainforest_weight(eye.x,eye.z),bearing,sun_strength,sky_fill);
            M4 lightvp;const double camera[]={eye.x,eye.y,eye.z};
            const float sun[]={light.sun.x,light.sun.y,light.sun.z},center[]={0,-25,0};
            if(!world_shadow_directional_matrix(camera,sun,160.f,shadow.side,center,lightvp.m))return false;
            world_scenery_update(scenery,eye.x,eye.y,eye.z,eye.yaw,eye.pitch,1.f,1.f);
            glBindFramebuffer(GL_FRAMEBUFFER,shadow.fbo);glViewport(0,0,shadow.side,shadow.side);
            glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
            glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(2,3);glClear(GL_DEPTH_BUFFER_BIT);
            {
            WorldModelDepthMaterialScope depth_materials(foliageShadow.id);
            terrain.draw(shadow.prog,eye,lightvp,true);
            foliageShadow.use();foliageShadow.m4("uVP",lightvp);foliageShadow.f("uTime",time);
            weather_program(foliageShadow,0,0,0);dressing.draw(foliageShadow,eye,true);outcrops.draw(foliageShadow,eye);
            world_scenery_draw(scenery,foliageShadow.id,eye.x,eye.y,eye.z,true);
            }
            glDisable(GL_POLYGON_OFFSET_FILL);
            glBindFramebuffer(GL_FRAMEBUFFER,face.framebuffer);glViewport(0,0,face.side,face.side);
            glDrawBuffer(GL_COLOR_ATTACHMENT0);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);
            auto atmosphere=sky_frame(eye,light,right,up,forward,0,0,false);
            atmosphere.aspect=1;atmosphere.tan_half_fov=1;atmosphere.sun_disc=false;atmosphere.volumetric_clouds=graphics.clouds;
            if(!world_sky_draw(sky,atmosphere,face.framebuffer,face.side,face.side,WORLD_SKY_PROBE_VIEW))return false;
            glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDisable(GL_CULL_FACE);
            const M4 vp=product(perspective(1.57079632679f,1,.4f,15000.f),look({0,0,0},forward,up));
            WorldTorchLights torches;
            if(!world_torch_lights_build(town_torch_emitters.data(),city_showcase?0:town_torch_emitters.size(),
                creek_origin_x,creek_origin_z,eye.x,eye.y,eye.z,time,torches))return false;
            studio_lights(torches,eye,time);
            land.use();light_program(land,light,eye);torch_program(land,torches);weather_program(land,0,0,0);
            bind_local_probes(land,eye,false);land.f("uHaze",0);land.i("uClipBelowWater",0);
            land.v2("uWorldSize",float(world_atlas_width()),float(world_atlas_depth()));
            land.v3("uEye",{float(eye.x),float(eye.y),float(eye.z)});land.m4("uLightVP",lightvp);
            const char *names[]={"uGrass","uRock","uSand","uDirt","uGrassNormal","uRockNormal","uClimate","uShadow","uGrassARM","uRockARM"};
            for(int i=0;i<10;i++){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,i==7?shadow.depth:materials[i]);land.i(names[i],i);}
            glActiveTexture(GL_TEXTURE11);glBindTexture(GL_TEXTURE_2D,macro_height);land.i("uMacroHeight",11);
            landscape.bind(land);showcase_terrain.bind(land.id);terrain.draw(land,eye,vp);
            foliage.use();foliage.i("uFacadeMode",0);landscape.bind_shelter(foliage);
            foliage.m4("uVP",vp);foliage.m4("uLightVP",lightvp);light_program(foliage,light,eye);torch_program(foliage,torches);
            weather_program(foliage,0,0,0);bind_local_probes(foliage,eye,false);foliage.f("uHaze",0);
            foliage.i("uClipBelowWater",0);foliage.f("uActorReactive",0);foliage.f("uTime",time);
            foliage.i("uWindHistoryEnabled",0);foliage.f("uWindFrameDelta",0);foliage.f("uTemporalPhase",0);
            foliage.f("uPixelScale",face.side*.5f);foliage.v2("uWorldSize",float(world_atlas_width()),float(world_atlas_depth()));
            glActiveTexture(GL_TEXTURE7);glBindTexture(GL_TEXTURE_2D,shadow.depth);foliage.i("uShadow",7);
            glActiveTexture(GL_TEXTURE11);glBindTexture(GL_TEXTURE_2D,macro_height);foliage.i("uMacroHeight",11);
            dressing.draw_reflected(foliage,eye);outcrops.draw(foliage,eye);horizon.draw(foliage,eye);
            foliage.i("uSourceMaterialAO",city_showcase?1:0);
            world_scenery_draw(scenery,foliage.id,eye.x,eye.y,eye.z);
            foliage.i("uSourceMaterialAO",0);
            return glGetError()==GL_NO_ERROR;
        };
        const bool trees_ready=dressing.prepare_probe_trees()&&horizon.prepare_probe_trees(destination);
        bool ok=trees_ready&&world_local_probes_capture(probes,[](const WorldLocalProbeCapture &f,void *p){
            return (*static_cast<decltype(capture)*>(p))(f);
        },&capture);
        const bool restored_dressing=dressing.restore_probe_trees(),restored_horizon=horizon.restore_probe_trees();
        ok=ok&&restored_dressing&&restored_horizon;
        world_scenery_update(scenery,destination.x,destination.y,destination.z,destination.yaw,destination.pitch,destination.aspect);
        glBindFramebuffer(GL_FRAMEBUFFER,target.fbo);glViewport(0,0,target.w,target.h);
        glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glEnable(GL_CULL_FACE);glDisable(GL_BLEND);glActiveTexture(GL_TEXTURE0);
        if(ok){glFinish();local_probe_loading_bakes++;}
        std::printf("[World] Local radiance region%d: %s, %zu probes, %.1f ms (loading only)\n",region,ok?"ready":"FAILED",count,millis(begin));
        return ok;
    };
    // The opening host applies its indoor sun/sky/exposure after this include.
    // Defer only opted-in openings so their one-shot cache matches that light.
    if(running&&!(opening_enabled&&opening_interior.enabled)&&!ensure_local_probes(cam)){launch->exit_code=1;running=false;}

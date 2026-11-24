    // Normal-launch connections. Built-in markers remain a separate persistent
    // registry; the user's world.studio is not rewritten by this fallback.
    std::unique_ptr<WorldLiveVisuals> live_visuals;
    std::uint64_t live_visual_generation=0,live_visual_epoch=0;
    std::uint64_t live_rollback_serial=0;
    std::vector<std::pair<world_runtime::Id,std::size_t>> live_scene_records;
    std::vector<std::size_t> live_suppressed_records;
    world_gameplay::Id live_training_encounter=0;
    auto refresh_live_visuals=[&]()->bool{
        if(!studio.components_ready||!studio.components_runtime)return true;
        studio.refresh_scene(scenery);
        auto next=std::make_unique<WorldLiveVisuals>();
        for(const auto &visual:studio.components_demo.visuals){
            world_runtime::Entity pose;if(!studio.components_runtime->entity(visual.entity,pose))continue;
            const bool ok=visual.kind==world_runtime_demo::VisualKind::Crate?next->add_rigid(visual.entity,visual.asset.c_str(),"purchased_crate"):next->add_actor(visual.entity,pose);
            if(!ok){studio.message="Live scene visuals rejected: "+next->error;next->release();return false;}
        }
        if(presentation){world_runtime::Entity captain;if(!studio.components_runtime->entity(world_runtime_demo::instructor_id,captain)||!next->add_actor(world_runtime_demo::instructor_id,captain)){studio.message=next->error;next->release();return false;}world_castle_training_replace_instructor(training,true);}
        for(const auto &binding:studio.components_active_document.living){const auto *node=world_entity::find(studio.live.entities,binding.entity);if(node&&node->binding.kind==world_entity::Kind::Actor&&node->binding.index<studio.live.actors.size()){const auto &actor=studio.live.actors[node->binding.index];if(!next->add_edited_actor(binding.entity,node->binding.index,world_village_pose_archetypes[actor.appearance].walk_count!=0)){studio.message=next->error;next->release();return false;}}}
        std::vector<std::pair<world_runtime::Id,std::size_t>> records;
        std::vector<std::size_t> suppressed;
        for(const auto &node:studio.live.entities.nodes){
            if(node.binding.kind!=world_entity::Kind::SourceModel&&node.binding.kind!=world_entity::Kind::Placement)continue;
            world_studio::Pose pose;const unsigned key=node.binding.kind==world_entity::Kind::Placement?8192+node.binding.index:node.binding.index;
            if(!studio.pose(key,pose,&studio.live))continue;
            std::size_t found=SIZE_MAX;unsigned matches=0;
            const auto matches_pose=[&](std::size_t i){WorldSceneryPlacement p;return world_scenery_placement(scenery,i,&p)&&pose.alias==p.alias&&std::abs(p.x-pose.root.x)<.001&&std::abs(p.z-pose.root.z)<.001&&std::abs(p.y-pose.root.y)<.06&&std::abs(p.scale-pose.scale)<.0001;};
            // Prefer the editor's committed record identity. Distinct objects
            // may intentionally share a model and the exact same transform.
            if(key<studio.applied_records.size()&&studio.applied_records[key]!=SIZE_MAX&&matches_pose(studio.applied_records[key])){found=studio.applied_records[key];matches=1;}
            else for(std::size_t i=0;i<world_scenery_stats(scenery).source_instances;i++)if(matches_pose(i)){found=i;matches++;}
            if(matches!=1)continue; // Ambiguous roots never borrow another object.
            records.push_back({node.id,found});
            for(const auto &binding:studio.components_active_document.physics)if(binding.entity==node.id){
                if(!next->add_rigid(node.id,studio.model_path(pose.alias).c_str(),pose.alias.c_str())){studio.message="Physical scene model rejected: "+next->error;next->release();return false;}suppressed.push_back(found);break;
            }
        }
        if(!next->warm(live_programs.skeletal.id,live_programs.skeletal_shadow.id,live_programs.rigid.id,live_programs.rigid_shadow.id)||!next->update(*studio.components_runtime,*studio.components_frame,scenery,nullptr,0,cam.x,cam.z,false)){
            studio.message="Live scene preparation failed: "+next->error;next->release();return false;
        }
        // A scene rebuild can change static record indices without changing
        // these actors. Preserve exact handles, gait, action and patrol state.
        bool same=live_visuals&&live_visuals->animation&&next->animation&&live_visuals->actor_count==next->actor_count&&live_visuals->animation->identity()==next->animation->identity();
        for(unsigned i=0;same&&i<next->actor_count;i++)same=next->actors[i].entity==live_visuals->actors[i].entity&&next->actors[i].handle==live_visuals->actors[i].handle;
        if(same){world_runtime_animation::Snapshot state;live_visuals->animation->checkpoint(state);if(!next->animation->restore(state,next->context,next->error)){studio.message=next->error;next->release();return false;}for(unsigned i=0;i<next->actor_count;i++){next->actors[i].patrol_leg=live_visuals->actors[i].patrol_leg;next->actors[i].action_clock=live_visuals->actors[i].action_clock;}next->play_saved=live_visuals->play_saved;next->play_snapshot=live_visuals->play_snapshot;next->play_actors=live_visuals->play_actors;if(!next->update(*studio.components_runtime,*studio.components_frame,scenery,nullptr,0,cam.x,cam.z,false)){studio.message=next->error;next->release();return false;}}
        if(live_visuals)live_visuals->release();
        live_visuals=std::move(next);active_live_visuals=live_visuals.get();
        world_village_population_clear_runtime(population);
        if(!live_visuals->update(*studio.components_runtime,*studio.components_frame,scenery,population,0,cam.x,cam.z,false)){studio.message=live_visuals->error;return false;}
        for(auto record:live_suppressed_records)world_scenery_runtime_active(scenery,record,true);
        live_scene_records=std::move(records);live_suppressed_records=std::move(suppressed);
        for(auto record:live_suppressed_records)world_scenery_runtime_active(scenery,record,false);
        for(unsigned i=0;i<world_village_resident_count;i++)world_village_population_runtime_active(population,i,true);
        // Only a unique, explicitly bound logical identity can replace a base
        // resident. Repeated crowd role names never identify a person reliably.
        for(const auto &visual:studio.components_demo.visuals)if(visual.living_actor){
            std::size_t found=0;unsigned matches=0;for(unsigned i=0;i<world_village_resident_count;i++){WorldVillageResident resident;if(world_village_population_resident(population,i,&resident)&&resident.name&&visual.name==resident.name){found=i;matches++;}}
            if(matches==1)world_village_population_runtime_active(population,found,false);
        }
        live_visual_generation=studio.components_generation;live_visual_epoch=world_program_epoch;
        active_world_runtime=studio.components_runtime.get();return true;
    };
    valid=studio.components_start(creek_origin_x,creek_origin_z,{WORLD_TRAIL_CAMP_X,WORLD_TRAIL_CAMP_Y,WORLD_TRAIL_CAMP_Z})&&valid;
    if(studio.components_ready)valid=refresh_live_visuals()&&valid;
    else std::fprintf(stderr,"[Live world] %s\n",studio.message.c_str());
    if(!valid)launch->exit_code=1;
    if(!std::getenv("VALWOOD_QA_DISABLE_ASSET_STREAMING")){
        char error[256]={};if(!world_scenery_streaming(scenery,true,{},error,sizeof error)){std::fprintf(stderr,"[Streaming] %s\n",error);studio.message=std::string("Automatic asset streaming unavailable: ")+error;valid=false;}
    }
    auto draw_live_color=[&](const Camera &eye,const M4 &view,const M4 &light_vp,const Lighting &light,const WorldTorchLights &torches,float time,float wetness,bool reflected,float level){
        if(!live_visuals)return true;
        auto prepare=[&](Program &p){p.use();p.m4("uVP",view);p.m4("uLightVP",light_vp);light_program(p,light,eye);torch_program(p,torches);bind_local_probes(p,eye);landscape.bind_shelter(p);studio_surface_gpu.bind(p,eye.x,eye.y,eye.z);weather_program(p,last_wind_x,last_wind_z,wetness);p.i("uClipBelowWater",reflected?1:0);p.f("uWaterClipLevel",level);p.f("uTime",time);p.i("uArchitectureWeathering",1);p.i("uShadowGather",shadow_gather_enabled?1:0);p.f("uTemporalPhase",0);p.i("uWindHistoryEnabled",0);p.v2("uWorldSize",float(world_atlas_width()),float(world_atlas_depth()));glActiveTexture(GL_TEXTURE7);glBindTexture(GL_TEXTURE_2D,shadow.depth);p.i("uShadow",7);glActiveTexture(GL_TEXTURE11);glBindTexture(GL_TEXTURE_2D,macro_height);p.i("uMacroHeight",11);};
        prepare(live_programs.skeletal);int graph=-1;for(unsigned i=0;i<studio.live.graphs.size();i++)if(studio.live.graphs[i].alias=="__npc_0__")graph=int(i);const bool ok=live_visuals->draw_actors(live_programs.skeletal,eye.x,eye.y,eye.z,graph);
        prepare(live_programs.rigid);live_visuals->draw_rigids(live_programs.rigid,eye.x,eye.y,eye.z,&studio.live);return ok;
    };
    auto draw_live_shadow=[&](const Camera &eye,const M4 &light_vp,float time){
        if(!live_visuals)return true;
        live_programs.skeletal_shadow.use();live_programs.skeletal_shadow.m4("uVP",light_vp);live_programs.skeletal_shadow.f("uTime",time);
        int graph=-1;for(unsigned i=0;i<studio.live.graphs.size();i++)if(studio.live.graphs[i].alias=="__npc_0__")graph=int(i);
        const bool ok=live_visuals->draw_actors(live_programs.skeletal_shadow,eye.x,eye.y,eye.z,graph);
        live_programs.rigid_shadow.use();live_programs.rigid_shadow.m4("uVP",light_vp);live_programs.rigid_shadow.f("uTime",time);WorldModelDepthMaterialScope depth_scope(live_programs.rigid_shadow.id);live_visuals->draw_rigids(live_programs.rigid_shadow,eye.x,eye.y,eye.z,&studio.live);return ok;
    };

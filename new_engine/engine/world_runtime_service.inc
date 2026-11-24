        // One authoritative logical clock, then one physical/gameplay tick,
        // then one pose packet shared by every render pass.
        const bool live_allowed=!playtest_enabled&&!opening_enabled&&!milestone_review.active()&&!preview_blocked()&&!studio.production_freeze()&&!studio.cinematic_preview&&!destination_loading;
        studio.living_service(live_allowed,!launch->frames&&!launch->benchmark&&!studio.production_job.valid());
        if(live_visuals&&studio.components_play_snapshot_ready&&!live_visuals->play_saved)live_visuals->checkpoint();
        if(live_visuals&&!studio.components_play_snapshot_ready&&live_visuals->play_saved){
            if(studio.components_rollback_serial!=live_rollback_serial&&!live_visuals->restore()){std::fprintf(stderr,"[Live world] Animation restore: %s\n",live_visuals->error.c_str());launch->exit_code=1;running=false;}
            live_visuals->play_saved=false;live_rollback_serial=studio.components_rollback_serial;
        }
        studio.components_service(launch->frames?1./60.:dt,Studio::components_wall(),{cam.x,cam.y-1.75,cam.z},live_allowed&&!studio.open);
        if(qa_training&&studio.components_runtime&&studio.components_runtime->current_line()&&!fight.engaged())studio.components_runtime->choose(0);
        if(studio.components_ready&&studio.components_generation!=live_visual_generation){
            if(!refresh_live_visuals()){std::fprintf(stderr,"[Live world] %s\n",studio.message.c_str());launch->exit_code=1;running=false;}
        }
        if(live_visuals&&live_visual_epoch!=world_program_epoch){
            if(!live_visuals->warm(live_programs.skeletal.id,live_programs.skeletal_shadow.id,live_programs.rigid.id,live_programs.rigid_shadow.id)){launch->exit_code=1;running=false;}
            live_visual_epoch=world_program_epoch;
        }
        if(studio.components_ready&&studio.components_runtime){
            active_world_runtime=studio.components_runtime.get();active_live_visuals=live_visuals.get();
            for(const auto &binding:live_scene_records){
                if(std::find(live_suppressed_records.begin(),live_suppressed_records.end(),binding.second)!=live_suppressed_records.end())continue;
                world_runtime::Entity entity;if(studio.components_runtime->entity(binding.first,entity))world_scenery_runtime_active(scenery,binding.second,entity.active);
            }
            world_gameplay::Command command;
            while(studio.components_poll_request(command)){
                if(playtest_enabled||opening_enabled)continue; // Dedicated challenges do not drive the old quest encounters.
                if(command.kind==world_gameplay::CommandKind::StartTraining){
                    if(command.entity==world_runtime_demo::arena_id&&training&&fight.start(fight.instructor_x,25.75,fight.instructor_z)){
                        if(presentation){preview_arena_x=creek_origin_x;preview_arena_z=creek_origin_z;preview_arena_feet=24;fight.story_assist=preview_settings.story_assist;fight.watch_reward=studio.components_runtime->counter(world_runtime_demo::renown_counter)>0;}
                        if(presentation&&town_enabled){const auto benefit=world_town::consume(town_state);fight.town_damage=benefit.damage;fight.town_resistance=benefit.resistance;fight.town_recovery=benefit.recovery;}
                        live_training_encounter=command.key;fight_starts++;fight_begin_frame=frame;fight_end_frame=0;
                        cam.x=creek_origin_x+fight.start_x;cam.z=creek_origin_z+fight.start_z;cam.y=25.75;cam.vy=0;cam.grounded=true;cam.fly=false;
                        cam.yaw=1.57079633f;cam.pitch=-.12f;temporal_cut=true;look_active=!presentation||!preview_pad.using_pad;fight_mouse_x=fight_mouse_y=0;
                        fight_capture_focused=(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)!=0;SDL_SetRelativeMouseMode(look_active?SDL_TRUE:SDL_FALSE);
                    }else {studio.message="The training host could not start that encounter.";studio.components_runtime->enqueue({world_gameplay::EventKind::EncounterFinished,0,command.key,0,0});}
                }else if(command.kind==world_gameplay::CommandKind::StartEncounter){
                    if(presentation&&command.key==world_presentation::threat_encounter&&fight.start_threat(preview_settings.story_assist,studio.components_runtime->counter(world_runtime_demo::renown_counter)>0)){
                        const double crossing_x=WORLD_TRAIL_CAMP_X+world_presentation::crossing_offset_x,crossing_z=WORLD_TRAIL_CAMP_Z+world_presentation::crossing_offset_z;
                        preview_arena_x=crossing_x-fight.enemy_x;preview_arena_z=crossing_z-fight.enemy_z;preview_arena_feet=walking_height(crossing_x,crossing_z,nullptr);
                        cam.x=preview_arena_x+fight.start_x;cam.z=preview_arena_z+fight.start_z;cam.y=walking_height(cam.x,cam.z,nullptr)+1.75;cam.vy=0;cam.grounded=true;cam.fly=false;
                        cam.yaw=1.57079633f;cam.pitch=-.12f;temporal_cut=true;look_active=!presentation||!preview_pad.using_pad;fight_mouse_x=fight_mouse_y=0;
                        if(presentation&&town_enabled){const auto benefit=world_town::consume(town_state);fight.town_damage=benefit.damage;fight.town_resistance=benefit.resistance;fight.town_recovery=benefit.recovery;}
                        live_training_encounter=command.key;fight_starts++;fight_begin_frame=frame;fight_end_frame=0;SDL_SetRelativeMouseMode(look_active?SDL_TRUE:SDL_FALSE);preview_audio.cue(2);
                    }else{studio.message="That encounter has no supported combat host.";studio.components_runtime->enqueue({world_gameplay::EventKind::EncounterFinished,0,command.key,0,0});}
                }
            }
            if(!opening_enabled&&studio.components_runtime->current_line()){look_active=false;fight.set_guard(false);SDL_SetRelativeMouseMode(SDL_FALSE);}
            if(live_training_encounter&&!fight.engaged()&&(fight.state==TrainingState::Won||fight.state==TrainingState::Lost||fight.state==TrainingState::Cancelled)){
                studio.components_runtime->enqueue({world_gameplay::EventKind::EncounterFinished,0,live_training_encounter,0,fight.state==TrainingState::Won?1:0});live_training_encounter=0;
            }
            if(live_visuals&&!live_visuals->update(*studio.components_runtime,*studio.components_frame,scenery,population,qa_fixed_time?0:(launch->frames?1./60.:dt),cam.x,cam.z,live_allowed&&!studio.open,cam.fly?std::numeric_limits<double>::quiet_NaN():cam.y-1.75)){
                std::fprintf(stderr,"[Live world] %s\n",live_visuals->error.c_str());launch->exit_code=1;running=false;
            }
        }

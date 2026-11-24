    const bool qa_live=launch->frames&&std::getenv("VALWOOD_QA_LIVE_WORLD");
    const bool qa_journey=qa_live&&std::getenv("VALWOOD_QA_DEMO_JOURNEY");
    unsigned qa_live_phase=0;std::uint64_t qa_live_wait=0;
    world_runtime::Snapshot qa_live_snapshot;world_runtime::Entity qa_live_crate;
    world_runtime_animation::Snapshot qa_live_animation;
    std::array<WorldLiveVisuals::Actor,64> qa_live_actors;
    auto live_workflow=[&](){
        if(!qa_live||destination_loading||!terrain.view_ready(cam.x,cam.z,cam.fly))return;
        auto fail=[&](const char *why){std::fprintf(stderr,"[Live QA] FAIL phase%u: %s\n",qa_live_phase,why);launch->exit_code=1;running=false;qa_live_phase=99;};
        auto interact_at=[&](world_runtime::Id id){world_runtime::Entity entity;if(!studio.components_runtime->entity(id,entity))return false;cam.x=entity.pose.position.x+2;cam.z=entity.pose.position.z;cam.y=entity.pose.position.y+1.75;cam.vy=0;cam.fly=false;cam.grounded=true;cam.yaw=-1.57079633f;cam.pitch=0;temporal_cut=true;SDL_Event event={};event.type=SDL_KEYDOWN;event.key.windowID=SDL_GetWindowID(window);event.key.state=SDL_PRESSED;event.key.keysym.sym=SDLK_e;event.key.keysym.unused=0x56414c54u;return SDL_PushEvent(&event)==1;};
        if(qa_live_phase==0&&frame>130){
            if(!studio.components_ready||!live_visuals||live_visuals->actor_count!=6||live_visuals->rigid_count!=2||!world_scenery_streaming_stats(scenery).enabled){fail("Normal launch live actor/physics/streaming setup");return;}
            world_runtime::Entity instructor;
            if(!studio.components_runtime->entity(world_runtime_demo::instructor_id,instructor)||!studio.components_runtime->entity(world_runtime_demo::castle_crate_id,qa_live_crate)||!studio.components_runtime->snapshot(qa_live_snapshot,studio.message)){fail("Persistent marker/body snapshot");return;}
            live_visuals->animation->checkpoint(qa_live_animation);qa_live_actors=live_visuals->actors;
            world_physics::Pose tilted;tilted.position=qa_live_crate.pose.position;tilted.position.y+=1.2;tilted.rotation={0,0,float(std::sin(.2)),float(std::cos(.2))};
            if(!studio.components_runtime->teleport_body(700002,tilted)||!studio.components_runtime->impulse_body(700002,{8,60,2})||!interact_at(world_runtime_demo::instructor_id)){fail("Actual tipped crate impulse or instructor key event");return;}
            qa_live_phase=1;qa_live_wait=frame;
        }else if(qa_live_phase==1&&frame>qa_live_wait){
            if(!studio.components_runtime->current_line()||!studio.components_runtime->choose(0)){fail("Actual conversation/choice");return;}
            qa_live_phase=2;qa_live_wait=frame;
        }else if(qa_live_phase==2&&frame>qa_live_wait+1){
            if(!fight.engaged()||live_training_encounter!=world_runtime_demo::training_encounter_id){fail("Typed StartTraining did not reach actual combat host");return;}
            fight.leave();qa_live_phase=3;qa_live_wait=frame;
        }else if(qa_live_phase==3&&frame>qa_live_wait+3){
            world_runtime::Entity moved;
            if(live_training_encounter||!studio.components_runtime->entity(world_runtime_demo::castle_crate_id,moved)||std::abs(moved.pose.position.y-qa_live_crate.pose.position.y)<.0001||std::abs(moved.pose.rotation.z)<.01||!live_visuals->draws){fail("Physical motion/render/training finish lifecycle");return;}
            if(!interact_at(world_runtime_demo::instructor_id)){fail("Training restart key event");return;}
            qa_live_phase=4;qa_live_wait=frame;
        }else if(qa_live_phase==4&&frame>qa_live_wait){
            if(!studio.components_runtime->current_line()||!studio.components_runtime->choose(0)){fail("Training restart conversation");return;}
            qa_live_phase=5;qa_live_wait=frame;
        }else if(qa_live_phase==5&&frame>qa_live_wait+1){
            if(!fight.engaged()||live_training_encounter!=world_runtime_demo::training_encounter_id||fight_starts<2){fail("Completed encounter did not restart");return;}
            fight.leave();live_training_encounter=0;
            if(!studio.components_runtime->restore(qa_live_snapshot,Studio::components_wall(),studio.message)||!live_visuals->animation->restore(qa_live_animation,live_visuals->context,studio.message)){fail("Live systems and animation rollback");return;}
            live_visuals->actors=qa_live_actors;
            world_runtime_animation::ActorState restored_animation;if(!live_visuals->animation->state(qa_live_actors[0].handle,restored_animation,studio.message)||restored_animation.gait_phase!=qa_live_animation.actors[qa_live_actors[0].handle.slot].gait_phase){fail("Animation phase was not restored");return;}
            studio.components_runtime->read_frame(*studio.components_frame);
            world_runtime::Entity restored,steward;
            if(!studio.components_runtime->entity(world_runtime_demo::castle_crate_id,restored)||std::abs(restored.pose.position.x-qa_live_crate.pose.position.x)>1e-8||std::abs(restored.pose.position.y-qa_live_crate.pose.position.y)>1e-8||std::abs(restored.pose.position.z-qa_live_crate.pose.position.z)>1e-8||std::abs(restored.pose.rotation.z-qa_live_crate.pose.rotation.z)>1e-6||std::abs(restored.pose.rotation.w-qa_live_crate.pose.rotation.w)>1e-6||!studio.components_runtime->entity(world_runtime_demo::jungle_actor_first,steward)){fail("Body pivot restore or jungle marker");return;}
            cam.x=steward.pose.position.x+2;cam.z=steward.pose.position.z;cam.y=steward.pose.position.y+1.75;cam.vy=0;cam.fly=false;cam.grounded=true;temporal_cut=true;destination_loading=true;current_name="Live jungle camp test";
            qa_live_phase=6;qa_live_wait=frame;
        }else if(qa_live_phase==6&&frame>qa_live_wait){
            if(!interact_at(world_runtime_demo::jungle_actor_first)){fail("Jungle NPC key event");return;}
            qa_live_phase=7;qa_live_wait=frame;
        }else if(qa_live_phase==7&&frame>qa_live_wait){
            if(!studio.components_runtime->current_line()||!studio.components_runtime->choose(0)){fail("Jungle dialogue dispatch");return;}
            qa_live_phase=8;qa_live_wait=frame;
            std::printf("[Live QA] PASS: six skeletal actors, two physical props, actual SDL E/dialogue/choice, training finish/restart, impulse/full-pivot pose, all-system rollback, jungle travel/streaming and local NPC conversation\n");std::fflush(stdout);
        }else if(qa_journey&&qa_live_phase==8&&frame>qa_live_wait+2){
            world_runtime::Entity instructor;if(!studio.components_runtime->entity(world_runtime_demo::instructor_id,instructor)){fail("Journey return marker");return;}
            cam.x=instructor.pose.position.x+2;cam.z=instructor.pose.position.z;cam.y=instructor.pose.position.y+1.75;temporal_cut=true;destination_loading=true;qa_live_phase=9;qa_live_wait=frame;
        }else if(qa_journey&&qa_live_phase==9&&frame>qa_live_wait+1){
            if(!interact_at(world_runtime_demo::instructor_id)){fail("Journey instructor key");return;}qa_live_phase=90;qa_live_wait=frame;
        }else if(qa_journey&&qa_live_phase==90&&frame>qa_live_wait){
            if(!studio.components_runtime->current_line()||!studio.components_runtime->recollection().knowledge||!studio.components_runtime->choose(0)){fail("Memory-backed instructor dialogue");return;}qa_live_phase=10;qa_live_wait=frame;
        }else if(qa_journey&&qa_live_phase==10&&frame>qa_live_wait+2){
            if(!fight.engaged()){fail("Journey actual training host");return;}
            // Outcome injection only for host integration. Combat rule tests
            // separately exercise attacks/parries, stamina and round wins.
            fight.state=TrainingState::Won;qa_live_phase=11;qa_live_wait=frame;
        }else if(qa_journey&&qa_live_phase==11&&frame>qa_live_wait+3){
            if(studio.components_runtime->counter(world_runtime_demo::mastery_counter)!=1){fail("Actual host completion did not progress journey");return;}
            world_runtime::Entity steward;if(!studio.components_runtime->entity(world_runtime_demo::jungle_actor_first,steward)){fail("Journey steward entity");return;}
            cam.x=steward.pose.position.x+2;cam.z=steward.pose.position.z;cam.y=steward.pose.position.y+1.75;temporal_cut=true;destination_loading=true;qa_live_phase=12;qa_live_wait=frame;
        }else if(qa_journey&&qa_live_phase==12&&frame>qa_live_wait){
            if(!interact_at(world_runtime_demo::jungle_actor_first)){fail("Journey steward key");return;}qa_live_phase=13;qa_live_wait=frame;
        }else if(qa_journey&&qa_live_phase==13&&frame>qa_live_wait){
            if(studio.components_runtime->choices().count!=2||!studio.components_runtime->choose(1)){fail("Trail report choice");return;}qa_live_phase=14;qa_live_wait=frame;
        }else if(qa_journey&&qa_live_phase==14&&frame>qa_live_wait+2){
            if(studio.components_runtime->counter(world_runtime_demo::camp_counter)!=1){fail("Trail report progress");return;}
            world_runtime::Entity instructor;if(!studio.components_runtime->entity(world_runtime_demo::instructor_id,instructor)){fail("Return instructor entity");return;}
            cam.x=instructor.pose.position.x+2;cam.z=instructor.pose.position.z;cam.y=instructor.pose.position.y+1.75;temporal_cut=true;destination_loading=true;qa_live_phase=15;qa_live_wait=frame;
        }else if(qa_journey&&qa_live_phase==15&&frame>qa_live_wait){
            if(!interact_at(world_runtime_demo::instructor_id)){fail("Return instructor key");return;}qa_live_phase=16;qa_live_wait=frame;
        }else if(qa_journey&&qa_live_phase==16&&frame>qa_live_wait){
            const auto remembered=studio.components_runtime->recollection();if(!remembered.recognition||!std::strstr(remembered.recognition,"victory")||!studio.components_runtime->choose(2)){fail("Recognition or return report choice");return;}qa_live_phase=17;qa_live_wait=frame;
        }else if(qa_journey&&qa_live_phase==17&&frame>qa_live_wait+2){
            if(studio.components_runtime->quest(world_runtime_demo::journey_id)!=world_gameplay::Progress::Complete||studio.components_runtime->counter(world_runtime_demo::coin_counter)!=25||!save_journey()){fail("Journal reward and F5 save path");return;}
            if(!studio.components_runtime->restore(qa_live_snapshot,Studio::components_wall(),studio.message)){fail("Clear progress before resume");return;}
            if(studio.components_runtime->counter(world_runtime_demo::coin_counter)!=0||!load_journey()){fail("F8 persistent load path");return;}
            qa_live_phase=18;qa_live_wait=frame;
        }else if(qa_journey&&qa_live_phase==18&&frame>qa_live_wait+2){
            if(studio.components_runtime->quest(world_runtime_demo::journey_id)!=world_gameplay::Progress::Complete||studio.components_runtime->counter(world_runtime_demo::coin_counter)!=25){fail("Persistent reward was not restored");return;}
            if(!interact_at(world_runtime_demo::instructor_id)){fail("Post-resume recognition key");return;}qa_live_phase=19;qa_live_wait=frame;
        }else if(qa_journey&&qa_live_phase==19&&frame>qa_live_wait){
            if(studio.components_runtime->choices().count!=2||!studio.components_runtime->recollection().recognition){fail("Post-resume recognition or reward duplication");return;}
            journey_open=true;qa_live_phase=20;
            std::printf("[Journey QA] PASS: actual E/dialogue/training-host completion/trail report/return reward, witnessed recognition, full persistent save and resume, no repeated reward\n");std::fflush(stdout);
        }
    };

    const bool qa_preview=presentation&&launch->frames&&std::getenv("VALWOOD_QA_PRESENTATION");
    unsigned qa_preview_phase=0,qa_preview_inputs=0,qa_preview_walk_steps=0,qa_preview_blocks=0;
    std::uint64_t qa_preview_wait=0;double qa_preview_pause_clock=0;
    bool qa_preview_done=false,qa_retry_completed=false,qa_retry_requested=false,qa_retry_chosen=false,qa_dodge_requested=false;const bool qa_test_retry=std::getenv("VALWOOD_QA_PRESENTATION_RETRY");unsigned qa_preview_waypoint=0;
    auto preview_workflow=[&](){
        if(!qa_preview||qa_preview_done)return;
        auto fail=[&](const char *why){std::fprintf(stderr,"[Preview QA] FAIL phase%u: %s\n",qa_preview_phase,why);launch->exit_code=1;running=false;};
        auto key=[&](SDL_Keycode k){SDL_Event e={};e.type=SDL_KEYDOWN;e.key.windowID=SDL_GetWindowID(window);e.key.state=SDL_PRESSED;e.key.keysym.sym=k;e.key.keysym.unused=0x56414c54u;if(SDL_PushEvent(&e)==1)qa_preview_inputs++;};
        auto button=[&](Uint8 b,bool down){SDL_Event e={};e.type=down?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;e.button.windowID=SDL_GetWindowID(window);e.button.button=b;e.button.which=0x56414c54u;e.button.state=down?SDL_PRESSED:SDL_RELEASED;if(SDL_PushEvent(&e)==1)qa_preview_inputs++;};
        auto at=[&](world_runtime::Id id){world_runtime::Entity e;if(!studio.components_runtime->entity(id,e))return false;cam.x=e.pose.position.x+2.3;cam.z=e.pose.position.z;cam.y=walking_height(cam.x,cam.z,nullptr)+1.75;cam.yaw=-1.57079633f;cam.pitch=0;cam.fly=false;cam.grounded=true;cam.vy=0;temporal_cut=true;return true;};
        const char *view=std::getenv("VALWOOD_QA_PRESENTATION_VIEW");
        auto finish=[&](const char *label){qa_preview_done=true;launch->frames=int(frame+90);std::printf("[Preview QA] PASS %s: phase%u, inputs%u, walking steps%u, blocked%u, hits%u, parries%u\n",label,qa_preview_phase,qa_preview_inputs,qa_preview_walk_steps,qa_preview_blocks,fight.hits,fight.parries);std::fflush(stdout);};
        if(frame==1){SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_PumpEvents();}
        if(destination_loading||preview_travel_phase)return;
        if(frame>uint64_t(launch->frames)-120&&!qa_preview_done){fail("Workflow exceeded its frame budget");return;}
        if(qa_preview_phase==0&&frame>150){if(!preview_initial||!studio.components_ready||!live_visuals||live_visuals->actor_count!=7){fail("Preview actor/document startup");return;}if(view&&!std::strcmp(view,"title")){finish("title");return;}key(SDLK_RETURN);qa_preview_phase=1;qa_preview_wait=frame;}
        else if(qa_preview_phase==1&&frame>qa_preview_wait+5){if(!at(world_runtime_demo::instructor_id)){fail("Corvin scene marker");return;}key(SDLK_e);qa_preview_phase=2;qa_preview_wait=frame;}
        else if(qa_preview_phase==2&&frame>qa_preview_wait+3){if(!studio.components_runtime->current_line()){fail("Actual E conversation");return;}if(view&&!std::strcmp(view,"conversation")){finish("conversation");return;}key(SDLK_1);qa_preview_phase=3;qa_preview_wait=frame;}
        else if(qa_preview_phase==3&&frame>qa_preview_wait+4){if(!fight.engaged()){fail("Trial host start");return;}if(view&&!std::strcmp(view,"trial")){finish("trial");return;}key(SDLK_ESCAPE);qa_preview_pause_clock=fight.clock;qa_preview_phase=4;qa_preview_wait=frame;}
        else if(qa_preview_phase==4&&frame>qa_preview_wait+18){if(preview_screen!=2||std::abs(fight.clock-qa_preview_pause_clock)>1.e-5){fail("Pause advanced the encounter");return;}key(SDLK_ESCAPE);qa_preview_phase=5;qa_preview_wait=frame;}
        else if(qa_preview_phase==5&&fight.state==TrainingState::Won&&studio.components_runtime->counter(world_runtime_demo::mastery_counter)==1){key(SDLK_e);qa_preview_phase=6;qa_preview_wait=frame;}
        else if(qa_preview_phase==6&&frame>qa_preview_wait+5){if(world_atlas_village_weight(cam.x,cam.z)>.1){fail("Chapter travel did not reach the forest");return;}qa_preview_phase=7;qa_preview_waypoint=0;qa_preview_wait=frame;}
        else if(qa_preview_phase==7){
            static const double path[][2]={{-26727,9735.496705929329},{-26710,9700.496705929329},{-26720,9660.496705929329},{-26707,9666.496705929329},{-26700,9658.496705929329},{-26694,9658.496705929329}};
            if(preview_stage()==PreviewChapter::Threat&&std::hypot(cam.x-WORLD_TRAIL_CAMP_X-world_presentation::crossing_offset_x,cam.z-WORLD_TRAIL_CAMP_Z-world_presentation::crossing_offset_z)<10){key(SDLK_e);qa_preview_phase=8;qa_preview_wait=frame;return;}
            const auto &p=path[qa_preview_waypoint];const double dx=p[0]-cam.x,dz=p[1]-cam.z,distance=std::hypot(dx,dz);
            if(distance<.10){if(++qa_preview_waypoint==std::size(path)){key(SDLK_e);qa_preview_phase=8;qa_preview_wait=frame;}return;}
            const double step=std::min(.09,distance);const auto result=move_camera(cam,scenery,cam.x+dx/distance*step,cam.z+dz/distance*step);qa_preview_walk_steps++;qa_preview_blocks+=result.collision||result.slope;
            cam.y=walking_height(cam.x,cam.z,nullptr)+1.75;cam.yaw=float(std::atan2(dx,-dz));cam.pitch=-.04f;cam.grounded=true;cam.fly=false;
            if(qa_preview_blocks>5){fail("Presentation trail has blocked walking steps");return;}
        }
        else if(qa_preview_phase==8&&frame>qa_preview_wait+5){
            if(!fight.engaged()||!fight.trail_threat){fail("Crossing encounter host");return;}
            const double ex=preview_arena_x+fight.enemy_x,ez=preview_arena_z+fight.enemy_z;
            if(!world_scenery_can_move(scenery,cam.x,cam.z,cam.x,cam.z,float(cam.y-1.75+.015),1.75f,.37f)||!world_scenery_can_move(scenery,ex,ez,ex,ez,preview_arena_feet+.015f,1.75f,.37f)){fail("Crossing player/opponent static capsule clearance");return;}
            if(view&&!std::strcmp(view,"crossing")){finish("crossing");return;}qa_preview_phase=9;
        }
        else if(qa_preview_phase==9&&fight.state==TrainingState::Won&&studio.components_runtime->counter(world_presentation::threat_counter)==1){if(!at(world_runtime_demo::jungle_actor_first)){fail("Rowan scene marker");return;}key(SDLK_e);qa_preview_phase=10;qa_preview_wait=frame;}
        else if(qa_preview_phase==10&&frame>qa_preview_wait+4){if(!studio.components_runtime->current_line()||studio.components_runtime->choices().count!=2){fail("Rowan report availability");return;}key(SDLK_2);qa_preview_phase=11;qa_preview_wait=frame;}
        else if(qa_preview_phase==11&&frame>qa_preview_wait+5){if(studio.components_runtime->counter(world_runtime_demo::camp_counter)!=1){fail("Report choice did not commit");return;}key(SDLK_e);qa_preview_phase=12;qa_preview_wait=frame;}
        else if(qa_preview_phase==12&&frame>qa_preview_wait+5){if(world_atlas_village_weight(cam.x,cam.z)<.2||!at(world_runtime_demo::instructor_id)){fail("Homecoming chapter");return;}key(SDLK_e);qa_preview_phase=13;qa_preview_wait=frame;}
        else if(qa_preview_phase==13&&frame>qa_preview_wait+5){const auto memory=studio.components_runtime->recollection();if(!memory.recognition||!std::strstr(memory.recognition,"victory")){fail("Corvin did not remember actual trial victory");return;}key(SDLK_3);qa_preview_phase=14;qa_preview_wait=frame;}
        else if(qa_preview_phase==14&&frame>qa_preview_wait+6){
            if(preview_screen!=4||studio.components_runtime->quest(world_runtime_demo::journey_id)!=world_gameplay::Progress::Complete||studio.components_runtime->counter(world_runtime_demo::coin_counter)!=25){fail("Ending or one-time reward");return;}
            if(!save_journey()){fail("Preview progress save");return;}
            if(!studio.components_runtime->restore(*preview_initial,Studio::components_wall(),studio.message)||studio.components_runtime->counter(world_runtime_demo::coin_counter)!=0||!load_journey()){fail("Fresh-state save resume");return;}
            qa_preview_phase=15;qa_preview_wait=frame;
        }else if(qa_preview_phase==15&&frame>qa_preview_wait+5){
            if(studio.components_runtime->counter(world_runtime_demo::coin_counter)!=25||studio.components_runtime->counter(world_presentation::threat_counter)!=1){fail("Persistent crossing/reward state");return;}
            if(town_enabled){
                if(!town_transact(world_town::Action::Sharpen)||!town_transact(world_town::Action::Meal)||!town_transact(world_town::Action::Rest)||!town_transact(world_town::Action::Bread)||!town_transact(world_town::Action::Apples)||!load_journey()) {fail("Reward-funded town services and saved inventory");return;}
                if(town_state.spent!=20||town_state.bread!=1||town_state.apples!=1||town_state.prepared!=7||world_town::purse(town_state,studio.components_runtime->counter(world_runtime_demo::coin_counter))!=5){fail("Town purse/inventory after actual quest reward and load");return;}
                std::puts("[Preview QA] PASS reward-funded armory, supper, rest, provisions, no overdraft and atomic inventory restart");
            }
            preview_screen=4;finish("complete journey, actual input combat, memory, checkpoint and ending");
        }
        if(qa_test_retry&&qa_preview_phase==5&&!qa_retry_completed){
            if(fight.state==TrainingState::Lost&&!qa_retry_requested){if(studio.components_runtime->counter(world_runtime_demo::mastery_counter)!=0){fail("Defeat awarded trial mastery");return;}key(SDLK_RETURN);qa_retry_requested=true;std::printf("[Preview QA] Actual defeat reached; Enter retries the encounter\n");}
            else if(qa_retry_requested&&!qa_retry_chosen&&studio.components_runtime->current_line()){key(SDLK_1);qa_retry_chosen=true;}
            else if(qa_retry_requested&&fight.state==TrainingState::Countdown){qa_retry_completed=true;std::printf("[Preview QA] Actual retry countdown reached\n");}
            if(!qa_retry_completed){if(fight.state==TrainingState::Fighting){const double ex=preview_arena_x+fight.enemy_x,ez=preview_arena_z+fight.enemy_z;const double dx=ex-cam.x,dz=ez-cam.z,d=std::hypot(dx,dz);if(d>2.18)move_camera(cam,scenery,cam.x+dx/d*std::min(.035,d-2.18),cam.z+dz/d*std::min(.035,d-2.18));}return;}
        }
        if(fight.state==TrainingState::Fighting&&preview_screen==1&&(qa_preview_phase==5||qa_preview_phase==9)){
            const double ex=preview_arena_x+fight.enemy_x,ez=preview_arena_z+fight.enemy_z;const double dx=ex-cam.x,dz=ez-cam.z,distance=std::hypot(dx,dz);if(distance>2.18){const double step=std::min(.035,distance-2.18);move_camera(cam,scenery,cam.x+dx/distance*step,cam.z+dz/distance*step);}
            if(!qa_dodge_requested&&fight.clock>4&&fight.player_phase==TrainingPlayerPhase::Ready&&fight.stamina>=28){key(SDLK_SPACE);qa_dodge_requested=true;return;}
            auto direction=[&](TrainingDirection d){key(d==TrainingDirection::Left?SDLK_LEFT:d==TrainingDirection::Right?SDLK_RIGHT:d==TrainingDirection::High?SDLK_UP:d==TrainingDirection::Low?SDLK_DOWN:SDLK_t);};
            if(fight.enemy_phase==TrainingEnemyPhase::Windup){direction(fight.enemy_direction);const bool defend=fight.enemy_duration-fight.enemy_time<.17;if(defend&&!fight.guard)button(SDL_BUTTON_RIGHT,true);if(!defend&&fight.guard)button(SDL_BUTTON_RIGHT,false);}
            else{if(fight.guard)button(SDL_BUTTON_RIGHT,false);if(fight.player_phase==TrainingPlayerPhase::Ready&&fight.stamina>=23){direction(TrainingDirection((unsigned(fight.enemy_guard)+1)%5));button(SDL_BUTTON_LEFT,true);button(SDL_BUTTON_LEFT,false);}}
        }
        if(view&&((!std::strcmp(view,"parry")&&fight.parries>0)||(!std::strcmp(view,"impact")&&fight.hits>0))&&!qa_preview_done){finish("combat effect capture");launch->frames=int(frame+4);}
        if(fight.state==TrainingState::Lost){fail("Actual-input combat bot lost the encounter");return;}
    };

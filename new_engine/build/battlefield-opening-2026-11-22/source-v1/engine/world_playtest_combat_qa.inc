// Explicit, bounded end-to-end sword challenge QA. Included after the playtest
// callbacks. All combat decisions use tagged SDL input and ordinary simulation;
// this workflow never assigns combat HP, phases, damage, hits or result state.
// Grandstone's existing first-hall transition is test setup, not walking proof.
bool playtest_combat_qa_done=false,playtest_combat_qa_hall_setup=false;
bool playtest_combat_qa_continue_ui=false;
unsigned playtest_combat_qa_phase=0,playtest_combat_qa_events=0,playtest_combat_qa_cases=0;
unsigned playtest_combat_qa_initial_starts=0,playtest_combat_qa_loss_hits=0;
unsigned playtest_combat_qa_win_hits=0,playtest_combat_qa_win_parries=0,playtest_combat_qa_win_swings=0;
float playtest_combat_qa_win_health=0;
uint64_t playtest_combat_qa_begin_frame=0,playtest_combat_qa_action_frame=0;
double playtest_combat_qa_walked=0;
unsigned playtest_combat_qa_blocked=0,playtest_combat_qa_stuck=0;
std::string playtest_combat_qa_saved_bytes;
world_playtest_slice::Session playtest_combat_qa_victory_session;
auto playtest_combat_workflow=[&](uint64_t qa_frame){
    if(!playtest_combat_qa||playtest_combat_qa_done||!running)return;
    auto fail=[&](const char *reason){
        std::fprintf(stderr,"[Playtest combat QA] FAIL phase %u after %u events/%u cases: %s\n",
                     playtest_combat_qa_phase,playtest_combat_qa_events,playtest_combat_qa_cases,reason);
        launch->exit_code=1;running=false;
    };
    if(!playtest_combat_qa_begin_frame)playtest_combat_qa_begin_frame=qa_frame+1;
    if(qa_frame>=playtest_combat_qa_begin_frame+17000){fail("Bounded combat QA timed out.");return;}
    if(destination_loading||qa_frame<=uint64_t(launch->warmup)||!terrain.view_ready(cam.x,cam.z,false))return;
    auto key_event=[&](SDL_Keycode code,bool down){
        SDL_Event input={};input.type=down?SDL_KEYDOWN:SDL_KEYUP;
        input.key.windowID=SDL_GetWindowID(window);input.key.state=down?SDL_PRESSED:SDL_RELEASED;
        input.key.repeat=0;input.key.keysym.sym=code;input.key.keysym.scancode=SDL_GetScancodeFromKey(code);
        input.key.keysym.mod=KMOD_NONE;input.key.keysym.unused=0x56414c54u;
        if(SDL_PushEvent(&input)!=1){fail("SDL rejected a tagged key event.");return false;}
        ++playtest_combat_qa_events;return true;
    };
    auto key=[&](SDL_Keycode code){return key_event(code,true)&&key_event(code,false);};
    auto button=[&](Uint8 code,bool down){
        SDL_Event input={};input.type=down?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;
        input.button.windowID=SDL_GetWindowID(window);input.button.button=code;
        input.button.which=0x56414c54u;input.button.state=down?SDL_PRESSED:SDL_RELEASED;
        if(SDL_PushEvent(&input)!=1){fail("SDL rejected a tagged mouse event.");return false;}
        ++playtest_combat_qa_events;return true;
    };
    auto direction=[&](TrainingDirection d){
        if(fight.selected==d)return true;
        return key(d==TrainingDirection::Left?SDLK_LEFT:d==TrainingDirection::Right?SDLK_RIGHT:
                   d==TrainingDirection::High?SDLK_UP:d==TrainingDirection::Low?SDLK_DOWN:SDLK_t);
    };
    auto read_saved=[&](std::string &bytes){
        std::ifstream file("saves/playtest.progress",std::ios::binary);if(!file)return false;
        std::array<char,4097> data={};file.read(data.data(),std::streamsize(data.size()));
        if(file.bad()||file.gcount()>4096)return false;
        bytes.assign(data.data(),size_t(file.gcount()));return true;
    };
    auto next=[&](){++playtest_combat_qa_phase;playtest_combat_qa_action_frame=qa_frame;};
    auto passed=[&](const char *label){
        ++playtest_combat_qa_cases;std::printf("[Playtest combat QA] PASS %s\n",label);std::fflush(stdout);
    };
    const uint64_t age=qa_frame>=playtest_combat_qa_action_frame?qa_frame-playtest_combat_qa_action_frame:0;
    auto healthy_start=[&](){
        return fight.engaged()&&fight.state==TrainingState::Countdown&&fight.health==100&&fight.stamina==100&&
               fight.round==0&&fight.hits==0&&fight.parries==0&&fight.enemy_hits==0&&
               !cam.fly&&cam.grounded&&third_person&&preview_screen==1;
    };
    switch(playtest_combat_qa_phase){
        case 0:
            if(stronghold_finish&&!playtest_section){
                if(!playtest_select(1)){fail("Could not set up the supported first-hall sector.");return;}
                playtest_combat_qa_hall_setup=true;
                std::printf("[Playtest combat QA] SETUP: first-hall sector transition; no walking-completion claim.\n");
                return;
            }
            if(!playtest_combat_ready||!combat_motion||cam.fly||!cam.grounded||!third_person||preview_screen!=1||studio.open){
                fail("The sword challenge did not start with a grounded third-person hero and loaded rig.");return;
            }
            SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_PumpEvents();
            playtest_combat_qa_initial_starts=playtest_combat_starts;
            passed("grounded third-person hero and challenge setup");next();return;
        case 1:{
            const double dx=playtest_combat_start_x-cam.x,dz=playtest_combat_start_z-cam.z,distance=std::hypot(dx,dz);
            if(std::hypot(playtest_combat_enemy_x-cam.x,playtest_combat_enemy_z-cam.z)>5.5){
                if(distance<.025){fail("The validated challenge start is outside its interaction radius.");return;}
                const double step=std::min(distance,world_atlas_walk_speed()/60.);
                auto motion=move_camera(cam,scenery,cam.x+dx/distance*step,cam.z+dz/distance*step);
                playtest_combat_qa_walked+=motion.actual;playtest_combat_qa_blocked+=motion.collision;
                cam.y=walking_height(cam.x,cam.z,nullptr)+1.75;cam.fly=false;cam.grounded=true;cam.vy=0;
                if(motion.actual<step*.2)++playtest_combat_qa_stuck;else playtest_combat_qa_stuck=0;
                if(playtest_combat_qa_stuck>120)fail("The source-collision approach to the challenge is blocked.");
                return;
            }
            if(key(SDLK_F5))next();return;
        }
        case 2:
            if(age<2)return;
            if(fight.engaged()||playtest_notice!="Playtest session saved."||!read_saved(playtest_combat_qa_saved_bytes)){
                fail("Could not create the pre-fight save through F5.");return;
            }
            passed("F5 baseline save before combat");if(key(SDLK_e))next();return;
        case 3:
            if(age<2)return;
            if(!healthy_start()||playtest_combat_starts!=playtest_combat_qa_initial_starts+1||
               world_combat_motion_stats(combat_motion).actors!=2){fail("E did not begin the visible, healthy sword challenge.");return;}
            passed("SDL E starts the sword challenge");if(key(SDLK_F5))next();return;
        case 4:{
            if(age<2)return;
            std::string saved;
            if(playtest_notice!="Finish or restart the sword challenge before saving or loading."||
               !read_saved(saved)||saved!=playtest_combat_qa_saved_bytes){fail("F5 changed the save during an active fight.");return;}
            passed("mid-fight F5 preserves the pre-fight session");next();return;
        }
        case 5:
            // Intentionally provide no attack, guard, dodge or HP mutation.
            if(fight.engaged())return;
            if(fight.state!=TrainingState::Lost||fight.health!=0||fight.enemy_hits<5||fight.hits||fight.parries){
                fail("The unprotected player did not receive real attacks and reach defeat.");return;
            }
            playtest_combat_qa_loss_hits=fight.enemy_hits;
            passed("enemy attacks reduce player health to an actual defeat");if(key(SDLK_e))next();return;
        case 6:
            if(age<2)return;
            if(!healthy_start()||playtest_combat_starts!=playtest_combat_qa_initial_starts+2){
                fail("E did not retry defeat with reset health, stamina and fight counters.");return;
            }
            passed("SDL E retries defeat with a fresh fight");next();return;
        case 7:
            if(fight.state==TrainingState::Won){
                if(!fight.trail_threat||fight.health<=0||fight.hits<3||!fight.parries||!fight.player_swings){
                    fail("Duel victory lacks actual sword hits and timed parries.");return;
                }
                playtest_combat_qa_win_hits=fight.hits;playtest_combat_qa_win_parries=fight.parries;
                playtest_combat_qa_win_swings=fight.player_swings;playtest_combat_qa_win_health=fight.health;
                passed("SDL attacks and timed guards win the duel with real hit/parry results");
                next();return;
            }
            if(!fight.engaged()){fail("The strategic SDL-input policy ended without victory.");return;}
            if(fight.state!=TrainingState::Fighting)return;
            if(fight.enemy_phase==TrainingEnemyPhase::Windup){
                if(!direction(fight.enemy_direction))return;
                const bool defend=fight.enemy_duration-fight.enemy_time<.17;
                if(defend&&!fight.guard)button(SDL_BUTTON_RIGHT,true);
                if(!defend&&fight.guard)button(SDL_BUTTON_RIGHT,false);
            }else{
                if(fight.guard&&!button(SDL_BUTTON_RIGHT,false))return;
                if(fight.player_phase==TrainingPlayerPhase::Ready&&fight.stamina>=23){
                    if(!direction(TrainingDirection((unsigned(fight.enemy_guard)+1)%5)))return;
                    if(button(SDL_BUTTON_LEFT,true))button(SDL_BUTTON_LEFT,false);
                }
            }
            return;
        case 8:{
            if(age<2)return;
            if(fight.state!=TrainingState::Won||playtest_combat_result_screen!=TrainingState::Won){
                fail("Victory did not display the actual challenge result UI.");return;
            }
            auto &saved=playtest_combat_qa_victory_session;saved.section=playtest_section;
            saved.fingerprint=world_playtest_slice::fingerprint(playtest_state.path());saved.index=playtest_state.index();
            saved.x=cam.x;saved.eye_y=cam.y;saved.z=cam.z;saved.yaw=cam.yaw;saved.pitch=cam.pitch;saved.third_person=third_person;
            // Navigate the actual default-focused Retry button to the adjacent
            // Continue button, then confirm on separate down/up frames. Never
            // invoke the Continue callback directly from this QA workflow.
            if(key_event(SDLK_DOWN,true))next();return;
        }
        case 9:
            if(age<1)return;
            if(key_event(SDLK_DOWN,false))next();return;
        case 10:
            if(age<2)return;
            if(key_event(SDLK_RETURN,true))next();return;
        case 11:
            if(age<1)return;
            if(key_event(SDLK_RETURN,false))next();return;
        case 12:{
            if(age<2)return;
            const auto &saved=playtest_combat_qa_victory_session;
            if(fight.state!=TrainingState::Idle||playtest_combat_starts!=playtest_combat_qa_initial_starts+2||
               cam.fly||!cam.grounded||!third_person||preview_screen!=1||
               playtest_section!=saved.section||playtest_state.index()!=saved.index||
               world_playtest_slice::fingerprint(playtest_state.path())!=saved.fingerprint||
               std::hypot(cam.x-saved.x,cam.z-saved.z)>.01||std::abs(cam.y-saved.eye_y)>.01||
               std::abs(cam.yaw-saved.yaw)>.001||std::abs(cam.pitch-saved.pitch)>.001||
               playtest_notice!="Exploration resumed. Return to the practice knight for another sword challenge."){
                fail("ImGui Down/Enter did not Continue exploring while preserving route and camera.");return;
            }
            playtest_combat_qa_continue_ui=true;
            passed("ImGui Down/Enter Continue exploring preserves the route and camera");
            if(key(SDLK_e))next();return;
        }
        case 13:
            if(age<2)return;
            if(!healthy_start()||playtest_combat_starts!=playtest_combat_qa_initial_starts+3){
                fail("E did not start a fresh challenge after Continue exploring.");return;
            }
            passed("SDL E starts a fresh challenge after Continue exploring");if(key(SDLK_r))next();return;
        case 14:
            if(age<2)return;
            if(fight.state!=TrainingState::Idle||fight.health!=100||fight.stamina!=100||fight.hits||fight.parries||fight.enemy_hits||
               cam.fly||!cam.grounded||!third_person||preview_screen!=1||
               std::hypot(cam.x-playtest_state.spawn().x,cam.z-playtest_state.spawn().z)>.01||
               world_combat_motion_stats(combat_motion).actors!=2){
                fail("R did not restore the hero, opponent and fresh grounded playtest state.");return;
            }
            passed("SDL R resets the active challenge and restores the third-person playtest");
            playtest_combat_qa_done=true;
            // Let the normal engine frame boundary render and capture this
            // final state. Exiting the loop here bypasses --capture entirely.
            launch->frames=launch->rendered_frames+1;
            std::printf("[Playtest combat QA] COMPLETE %u cases, %u tagged events; defeat %u hits, victory %u hits/%u parries/%.1f health\n",
                        playtest_combat_qa_cases,playtest_combat_qa_events,playtest_combat_qa_loss_hits,
                        playtest_combat_qa_win_hits,playtest_combat_qa_win_parries,playtest_combat_qa_win_health);
            std::fflush(stdout);return;
        default:fail("Invalid combat QA phase.");return;
    }
};
auto playtest_combat_qa_report=[&](FILE *file){
    std::fprintf(file,"  \"playtest_combat_qa\": {\"enabled\": %s, \"passed\": %s, \"phase\": %u, \"events\": %u, \"cases\": %u, \"continue_exploring_ui\": %s, \"first_hall_setup_transition\": %s, \"walked_to_challenge_m\": %.6f, \"approach_blocked_steps\": %u, \"loss_enemy_hits\": %u, \"win_hits\": %u, \"win_parries\": %u, \"win_swings\": %u, \"win_health\": %.3f, \"combat_state_mutations\": 0},\n",
                 playtest_combat_qa?"true":"false",playtest_combat_qa_done?"true":"false",playtest_combat_qa_phase,
                 playtest_combat_qa_events,playtest_combat_qa_cases,playtest_combat_qa_continue_ui?"true":"false",playtest_combat_qa_hall_setup?"true":"false",
                 playtest_combat_qa_walked,playtest_combat_qa_blocked,playtest_combat_qa_loss_hits,
                 playtest_combat_qa_win_hits,playtest_combat_qa_win_parries,playtest_combat_qa_win_swings,playtest_combat_qa_win_health);
};

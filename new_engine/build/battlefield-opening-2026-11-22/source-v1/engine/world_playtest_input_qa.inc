// Explicit bounded native input QA. Included after the opt-in playtest callbacks.
// Real SDL key events exercise the ordinary input/ImGui paths; no player warps.
// Grandstone's final completion flag is synthetic only to test E section travel.
bool playtest_input_done=false;
unsigned playtest_input_phase=0,playtest_input_events=0,playtest_input_cases=0;
bool playtest_input_synthetic_completion=false;
uint64_t playtest_input_begin_frame=0,playtest_input_action_frame=0;
unsigned playtest_input_restart_before=0;
world_playtest_slice::Session playtest_input_saved;
auto playtest_input_workflow=[&](uint64_t qa_frame){
    if(!playtest_input_qa||playtest_input_done||!running)return;
    auto fail=[&](const char *reason){
        std::fprintf(stderr,"[Playtest input QA] FAIL phase %u after %u events/%u cases: %s\n",
                     playtest_input_phase,playtest_input_events,playtest_input_cases,reason);
        launch->exit_code=1;running=false;
    };
    if(!playtest_input_begin_frame)playtest_input_begin_frame=qa_frame+1;
    if(qa_frame>=playtest_input_begin_frame+3600){fail("Bounded input QA timed out.");return;}
    if(destination_loading||qa_frame<=uint64_t(launch->warmup)||!terrain.view_ready(cam.x,cam.z,false))return;
    auto event=[&](SDL_Keycode key,bool down){
        SDL_Event input={};input.type=down?SDL_KEYDOWN:SDL_KEYUP;
        input.key.windowID=SDL_GetWindowID(window);input.key.state=down?SDL_PRESSED:SDL_RELEASED;
        input.key.repeat=0;input.key.keysym.sym=key;input.key.keysym.scancode=SDL_GetScancodeFromKey(key);
        input.key.keysym.mod=KMOD_NONE;input.key.keysym.unused=0x56414c54u;
        if(SDL_PushEvent(&input)!=1){fail("SDL rejected a tagged key event.");return false;}
        ++playtest_input_events;return true;
    };
    auto tap=[&](SDL_Keycode key){return event(key,true)&&event(key,false);};
    auto next=[&](){++playtest_input_phase;playtest_input_action_frame=qa_frame;};
    auto passed=[&](const char *label){
        ++playtest_input_cases;std::printf("[Playtest input QA] PASS %s\n",label);std::fflush(stdout);
    };
    const uint64_t age=qa_frame>=playtest_input_action_frame?qa_frame-playtest_input_action_frame:0;
    switch(playtest_input_phase){
        case 0:
            if(preview_screen!=1||cam.fly||!cam.grounded||!third_person||studio.open||show_graphics){fail("Playtest did not start grounded in third-person play mode.");return;}
            SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_PumpEvents();
            playtest_input_restart_before=playtest_restarts;
            if(tap(SDLK_r)){next();}return;
        case 1:
            if(age<2)return;
            if(playtest_restarts!=playtest_input_restart_before+1||preview_screen!=1||cam.fly||!cam.grounded||!third_person){fail("R did not restart the grounded third-person route.");return;}
            passed("R restart");if(tap(SDLK_v)){next();}return;
        case 2:
            if(age<2)return;
            if(third_person){fail("V did not switch to first person.");return;}
            passed("V first-person camera");if(tap(SDLK_F5)){next();}return;
        case 3:{
            if(age<2)return;
            std::ifstream file("saves/playtest.progress");std::string error;
            if(!file||!world_playtest_slice::parse_session(file,playtest_input_saved,error)||
               playtest_input_saved.section!=playtest_section||
               playtest_input_saved.fingerprint!=world_playtest_slice::fingerprint(playtest_state.path())||
               playtest_input_saved.third_person||std::hypot(playtest_input_saved.x-cam.x,playtest_input_saved.z-cam.z)>.01||
               std::abs(playtest_input_saved.eye_y-cam.y)>.01||playtest_notice!="Playtest session saved."){
                fail("F5 did not write the current grounded first-person session.");return;
            }
            passed("F5 session save");if(tap(SDLK_v)){next();}return;
        }
        case 4:
            if(age<2)return;
            if(!third_person){fail("The second V did not return to third person.");return;}
            passed("V third-person camera");if(tap(SDLK_F8)){next();}return;
        case 5:
            if(age<2)return;
            if(third_person||playtest_section!=playtest_input_saved.section||cam.fly||!cam.grounded||
               std::hypot(cam.x-playtest_input_saved.x,cam.z-playtest_input_saved.z)>.01||
               std::abs(cam.y-playtest_input_saved.eye_y)>.01||playtest_state.index()<playtest_input_saved.index||
               playtest_notice!="Playtest session restored."){
                fail("F8 did not restore the saved position, progress and camera mode.");return;
            }
            // Confirm that loading overrides the default camera, then leave
            // the acceptance run with the ordinary visible-hero view enabled.
            if(tap(SDLK_v)){next();}return;
        case 6:
            if(age<2)return;
            if(!third_person){fail("V did not restore the visible third-person hero after loading.");return;}
            passed("F8 session restore and return to third person");if(tap(SDLK_ESCAPE)){next();}return;
        case 7:
            if(age<2)return;
            if(preview_screen!=2||playtest_last_screen!=2){fail("Escape did not show and focus the pause menu.");return;}
            passed("Escape pause");
            // One full frame of Enter-down, then release. Do not invoke Resume
            // directly: the actual default-focused ImGui button must activate.
            if(event(SDLK_RETURN,true)){next();}return;
        case 8:
            if(age<1)return;
            if(event(SDLK_RETURN,false)){next();}return;
        case 9:
            if(age<2)return;
            if(preview_screen!=1||cam.fly||!cam.grounded){fail("Enter did not activate the default Resume button.");return;}
            passed("ImGui Enter Resume");if(tap(SDLK_F11)){next();}return;
        case 10:
            if(age<2)return;
            if(studio.open||preview_screen!=1||cam.fly){fail("F11 escaped the playtest into the editor.");return;}
            passed("F11 editor shortcut blocked");if(tap(SDLK_F10)){next();}return;
        case 11:
            if(age<2)return;
            if(!show_graphics){fail("F10 did not open graphics settings.");return;}
            passed("F10 graphics");if(tap(SDLK_ESCAPE)){next();}return;
        case 12:
            if(age<2)return;
            if(show_graphics||preview_screen!=1||studio.open){fail("Escape did not return from graphics to play.");return;}
            passed("Escape closes graphics");
            if(stronghold_finish){
                if(playtest_section!=0){fail("Grandstone control test did not start at the snowy entrance.");return;}
                std::string error;
                if(!playtest_state.restore_index(playtest_state.path().nodes.size(),error)){fail("Could not prepare the bounded section-travel control case.");return;}
                playtest_input_synthetic_completion=true;
                std::printf("[Playtest input QA] CONTROL ONLY: synthetic route completion enables E; no walking-completion claim.\n");
                if(tap(SDLK_e)){next();}return;
            }
            next();return;
        case 13:
            if(stronghold_finish){
                if(age<2)return;
                if(playtest_section!=1||!active_playtest_walk||cam.fly||!cam.grounded||!third_person||preview_screen!=1){fail("E did not enter the supported third-person first-hall section.");return;}
                passed("E first-hall section travel (control-only completion)");
            }
            playtest_input_done=true;running=false;
            std::printf("[Playtest input QA] COMPLETE %u cases, %u tagged events, section %u; no movement replay\n",
                        playtest_input_cases,playtest_input_events,playtest_section);
            std::fflush(stdout);return;
        default:fail("Invalid input QA phase.");return;
    }
};

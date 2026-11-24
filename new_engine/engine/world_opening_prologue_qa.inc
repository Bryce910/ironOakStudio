// Explicit, bounded integration QA. Uses ordinary SDL virtual-pad axes and
// tagged key events. Never assigns the player camera or story state.
// Host contract: opening_enabled/opening_qa, opening_state/opening_frame,
// existing preview_screen/preview_pad/controller_test_pad and ordinary camera.
bool opening_prologue_qa_done=false;
const char *opening_console_only_qa_option=std::getenv("VALWOOD_QA_OPENING_CONSOLE_ONLY");
const bool opening_console_only_qa=opening_qa&&opening_console_only_qa_option&&std::strcmp(opening_console_only_qa_option,"1")==0;
const char *opening_entry_qa_option=std::getenv("VALWOOD_QA_OPENING_CINEMATIC");
const bool opening_entry_qa_enabled=opening_entry_qa_option&&std::strcmp(opening_entry_qa_option,"1")==0;
unsigned opening_entry_qa_shots=0,opening_entry_qa_frames=0;
double opening_entry_qa_seconds=0,opening_entry_qa_handoff_error=0;
bool opening_entry_qa_handoff=false;
const char *opening_entry_film_dir=std::getenv("VALWOOD_QA_OPENING_FILM_DIR");
const bool opening_entry_film_enabled=opening_entry_qa_enabled&&opening_entry_film_dir&&*opening_entry_film_dir;
unsigned opening_entry_film_rendered=0,opening_entry_film_frames=0;
Uint64 opening_entry_film_started=0;
unsigned opening_prologue_qa_phase=0,opening_prologue_qa_events=0;
unsigned opening_prologue_qa_movement_frames=0,opening_prologue_qa_restores=0,opening_prologue_qa_retries=0;
unsigned opening_prologue_qa_captures=0,opening_prologue_qa_capture_failures=0;
uint64_t opening_prologue_qa_action_frame=0,opening_prologue_qa_begin=0;
uint64_t opening_prologue_qa_trace_frame=0;
unsigned opening_prologue_qa_focus_reacquires=0;
uint64_t opening_prologue_qa_handoff_begin=0,opening_prologue_qa_handoff_release=0;
unsigned opening_prologue_qa_handoff_frames=0;
unsigned opening_prologue_qa_menu_phase=0;
uint64_t opening_prologue_qa_menu_frame=0;
unsigned opening_prologue_qa_game_phase=0;
uint64_t opening_prologue_qa_game_frame=0;
std::vector<std::string> opening_prologue_qa_checks;
std::vector<world_opening_prologue::Vec> opening_prologue_qa_home,opening_prologue_qa_meadow;
std::vector<std::pair<SDL_Keycode,uint64_t>> opening_prologue_qa_releases;
std::string opening_prologue_qa_failure,opening_prologue_qa_capture_pending;
double opening_prologue_qa_walked=0,opening_prologue_qa_look=0;
double opening_prologue_qa_x=0,opening_prologue_qa_y=0,opening_prologue_qa_z=0,opening_prologue_qa_yaw=0;
double opening_prologue_qa_clock=0,opening_prologue_qa_jump_floor=0,opening_prologue_qa_jump_apex=0;
double opening_prologue_qa_previous_x=0,opening_prologue_qa_previous_z=0;
bool opening_prologue_qa_measure=false,opening_prologue_qa_initialized=false;
unsigned opening_prologue_qa_node=0,opening_prologue_qa_skips=0;
unsigned opening_prologue_qa_reveal_neutral_frames=0,opening_prologue_qa_reveal_active_frames=0;
const char *opening_prologue_qa_capture_dir=std::getenv("VALWOOD_QA_OPENING_CAPTURE_DIR");
auto opening_prologue_qa_fail=[&](const char *reason){
    opening_prologue_qa_failure=reason;
    std::fprintf(stderr,"[Opening QA] FAIL phase %u (%s): %s; player %.9f %.9f %.9f\n",
        opening_prologue_qa_phase,world_opening_prologue::stage_name(opening_state.stage()),reason,cam.x,cam.y-1.75,cam.z);
    std::fflush(stderr);launch->exit_code=1;running=false;
};
auto opening_prologue_qa_key=[&](SDL_Keycode key,bool down){
    SDL_Event event={};event.type=down?SDL_KEYDOWN:SDL_KEYUP;
    event.key.windowID=SDL_GetWindowID(window);event.key.state=down?SDL_PRESSED:SDL_RELEASED;
    event.key.keysym.sym=key;event.key.keysym.scancode=SDL_GetScancodeFromKey(key);
    event.key.keysym.unused=0x56414c54u;
    if(SDL_PushEvent(&event)!=1){opening_prologue_qa_fail("SDL rejected a tagged keyboard event");return false;}
    ++opening_prologue_qa_events;return true;
};
auto opening_prologue_qa_tap=[&](SDL_Keycode key,uint64_t frame){
    if(opening_prologue_qa_key(key,true))opening_prologue_qa_releases.push_back({key,frame+1});
};
auto opening_prologue_qa_axes=[&](Sint16 side,Sint16 forward,Sint16 look=0){
    controller_test_pad.axis(SDL_CONTROLLER_AXIS_LEFTX,side);
    controller_test_pad.axis(SDL_CONTROLLER_AXIS_LEFTY,forward);
    controller_test_pad.axis(SDL_CONTROLLER_AXIS_RIGHTX,look);
    controller_test_pad.axis(SDL_CONTROLLER_AXIS_RIGHTY,0);
};
auto opening_prologue_qa_pass=[&](const char *label){
    opening_prologue_qa_checks.push_back(label);
    std::printf("[Opening QA] PASS %s\n",label);std::fflush(stdout);
};
auto opening_prologue_qa_position=[&](){opening_prologue_qa_x=cam.x;opening_prologue_qa_y=cam.y;opening_prologue_qa_z=cam.z;opening_prologue_qa_yaw=cam.yaw;};
auto opening_prologue_qa_stationary=[&](){return std::hypot(cam.x-opening_prologue_qa_x,cam.z-opening_prologue_qa_z)<.0001&&std::abs(cam.y-opening_prologue_qa_y)<.0001&&std::abs(cam.yaw-opening_prologue_qa_yaw)<.0001;};
auto opening_prologue_qa_next=[&](uint64_t frame){++opening_prologue_qa_phase;opening_prologue_qa_action_frame=frame;};
auto opening_prologue_qa_walk=[&](const std::vector<world_opening_prologue::Vec> &path,unsigned last,double tolerance=.12){
    if(opening_prologue_qa_node>=path.size()||last>=path.size()){opening_prologue_qa_fail("Authored walk node is outside its sealed route");return false;}
    const auto &point=path[opening_prologue_qa_node];
    const double dx=point.x-cam.x,dz=point.z-cam.z,distance=std::hypot(dx,dz);
    if(distance<tolerance){
        opening_prologue_qa_axes(0,0);opening_prologue_qa_measure=false;
        if(opening_prologue_qa_node==last)return true;
        ++opening_prologue_qa_node;return false;
    }
    // Recover only a real walk, never a pause/menu/held-input assertion.
    // Releasing ordinary SDL axes lets the production neutral gate rearm;
    // this QA driver never writes a player pose, story state or gate flag.
    if(opening_qa&&preview_screen==1&&preview_pad.focused&&
       opening_frame.controls_enabled&&!destination_loading&&
       !(SDL_GetWindowFlags(window)&SDL_WINDOW_MINIMIZED)&&
       !opening_movement_armed){
        opening_prologue_qa_axes(0,0);return false;
    }
    // Solve a desired world direction into the ordinary current camera basis.
    // Camera pose is observed, never changed by the QA driver.
    const double side=(dx*std::cos(cam.yaw)+dz*std::sin(cam.yaw))/distance;
    const double forward=(dx*std::sin(cam.yaw)-dz*std::cos(cam.yaw))/distance;
    const double desired=std::clamp(distance/(world_atlas_walk_speed()*(1./60.)),.1,1.);
    const double raw=preview_settings.deadzone+(1-preview_settings.deadzone)*desired;
    opening_prologue_qa_axes(Sint16(std::clamp(side*raw,-1.,1.)*32767),Sint16(std::clamp(-forward*raw,-1.,1.)*32767));
    ++opening_prologue_qa_movement_frames;opening_prologue_qa_measure=true;
    opening_prologue_qa_previous_x=cam.x;opening_prologue_qa_previous_z=cam.z;
    return false;
};
#include "world_opening_room_lights_qa.inc"
#include "world_opening_arrival_camp_qa.inc"
auto opening_prologue_workflow=[&](uint64_t qa_frame){
    if(!opening_qa||opening_prologue_qa_done||!running)return;
    using Stage=world_opening_prologue::Stage;
    if(!opening_prologue_qa_begin)opening_prologue_qa_begin=qa_frame+1;
    const uint64_t opening_prologue_qa_budget=first_journey_qa?30000u:15500u;
    if(qa_frame>=opening_prologue_qa_begin+opening_prologue_qa_budget){opening_prologue_qa_fail("Bounded opening workflow timed out");return;}
    if(opening_prologue_qa_measure){
        const double moved=std::hypot(cam.x-opening_prologue_qa_previous_x,cam.z-opening_prologue_qa_previous_z);
        if(moved>.7){opening_prologue_qa_fail("Unexpected player discontinuity during a walked route");return;}
        opening_prologue_qa_walked+=moved;opening_prologue_qa_measure=false;
    }
    controller_test_pad.tick();
    for(auto it=opening_prologue_qa_releases.begin();it!=opening_prologue_qa_releases.end();){
        if(it->second<=qa_frame){opening_prologue_qa_key(it->first,false);it=opening_prologue_qa_releases.erase(it);}else ++it;
    }
    if(!opening_prologue_qa_initialized){
        std::ifstream file("opening/qa.walk");std::string token;unsigned version=0;
        auto path=[&](const char *name,std::vector<world_opening_prologue::Vec> &points,unsigned minimum){
            unsigned count=0;if(!(file>>token>>count)||token!=name||count<minimum||count>512)return false;
            points.resize(count);for(auto &p:points)if(!(file>>p.x>>p.y>>p.z)||!world_opening_prologue::detail::valid(p))return false;
            return true;
        };
        if(!(file>>token>>version)||token!="VALWOOD_OPENING_QA"||version!=1||
           !path("home",opening_prologue_qa_home,5)||!path("meadow",opening_prologue_qa_meadow,2)||!(file>>token)||token!="end"){
            opening_prologue_qa_fail("Missing or invalid sealed opening QA paths");return;
        }
        file>>std::ws;if(!file.eof()){opening_prologue_qa_fail("Extra QA path records");return;}
        if(!controller_test_pad.joystick||preview_pad.id()!=SDL_JoystickInstanceID(controller_test_pad.joystick)){
            opening_prologue_qa_fail("Opening QA does not own its SDL virtual controller");return;
        }
        SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_PumpEvents();preview_pad.focused=true;
        opening_prologue_qa_initialized=true;
    }
    const unsigned opening_prologue_qa_window_flags=SDL_GetWindowFlags(window);
    if(qa_frame>=opening_prologue_qa_trace_frame){
        std::printf("[Opening QA] TRACE frame%llu phase%u roomphase%u room%u node%u flags0x%x focused%d loading%d controls%d armed%d reacquires%u player%.3f %.3f %.3f\n",
            (unsigned long long)qa_frame,opening_prologue_qa_phase,opening_room_lights_qa_phase,opening_room_lights_qa_index,opening_prologue_qa_node,
            opening_prologue_qa_window_flags,int(preview_pad.focused),int(destination_loading),int(opening_controls_enabled),int(opening_movement_armed),opening_prologue_qa_focus_reacquires,cam.x,cam.y-1.75,cam.z);
        std::fflush(stdout);opening_prologue_qa_trace_frame=qa_frame+600;
    }
    if(!preview_pad.focused||!(opening_prologue_qa_window_flags&SDL_WINDOW_INPUT_FOCUS)){
        if(!opening_prologue_qa_focus_reacquires){std::printf("[Opening QA] Reacquiring native window focus at frame%llu phase%u flags0x%x focused%d\n",(unsigned long long)qa_frame,opening_prologue_qa_phase,opening_prologue_qa_window_flags,int(preview_pad.focused));std::fflush(stdout);}
        ++opening_prologue_qa_focus_reacquires;
        // Keep the replay's physical stick state intact. Production focus
        // handling releases logical controls; walking helpers request neutral
        // explicitly when the ordinary movement gate needs rearming.
        SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_PumpEvents();return;
    }
    if(destination_loading||qa_frame<=uint64_t(launch->warmup)||!terrain.view_ready(cam.x,cam.z,false))return;
    const uint64_t age=qa_frame>=opening_prologue_qa_action_frame?qa_frame-opening_prologue_qa_action_frame:0;
    const auto stage=opening_state.stage();
    switch(opening_prologue_qa_phase){
    case 0:
        if(stage!=Stage::Title){opening_prologue_qa_fail("Fresh isolated opening did not start at its title");return;}
        opening_prologue_qa_tap(SDLK_RETURN,qa_frame);opening_prologue_qa_next(qa_frame);return;
    case 1:
        if(age<3)return;
        if(stage!=Stage::HouseEstablish||opening_frame.controls_enabled){opening_prologue_qa_fail("Begin did not start the locked house cinematic");return;}
        if(opening_entry_qa_enabled&&opening_state.config().house_shots.empty()){
            opening_prologue_qa_fail("Natural cinematic acceptance requires authored apartment shots");return;
        }
        opening_prologue_qa_pass("title_begin");opening_prologue_qa_position();
        opening_prologue_qa_axes(20000,-20000,20000);
        opening_prologue_qa_next(qa_frame);return;
    case 2:
        if(opening_entry_qa_enabled){
            if(stage==Stage::HouseEstablish){
                if(opening_frame.controls_enabled||!opening_prologue_qa_stationary()){
                    opening_prologue_qa_fail("Directed apartment cinematic accepted held movement/look input");return;
                }
                ++opening_entry_qa_frames;opening_entry_qa_seconds=opening_state.elapsed();
                const unsigned shot=opening_frame.house_shot_index;
                if(shot>=opening_state.config().house_shots.size()||shot>=6){
                    opening_prologue_qa_fail("Directed cinematic reported an invalid shot index");return;
                }
                if(opening_frame.house_shot_progress>=.42&&!(opening_entry_qa_shots&(1u<<shot))){
                    opening_entry_qa_shots|=1u<<shot;
                    opening_prologue_qa_capture_pending="house-shot-"+std::to_string(shot+1);
                }
                return;
            }
            const auto &shots=opening_state.config().house_shots;
            const double duration=world_opening_prologue::house_duration(opening_state.config());
            if(stage!=Stage::HouseExplore||opening_state.stats().skips!=0||
               opening_entry_qa_seconds<duration-.05||opening_entry_qa_frames<unsigned(duration*50)){
                opening_prologue_qa_fail("Apartment entry did not play its complete natural story clock");return;
            }
            if(opening_entry_qa_shots!=(1u<<shots.size())-1u){
                opening_prologue_qa_fail("Natural apartment entry missed a rendered shot capture");return;
            }
            const auto &last=shots.back().rail;
            const double dx=last.target_to.x-last.eye_to.x,dy=last.target_to.y-last.eye_to.y,dz=last.target_to.z-last.eye_to.z;
            opening_entry_qa_handoff_error=std::max({
                std::abs(cam.x-last.eye_to.x),std::abs(cam.y-last.eye_to.y),std::abs(cam.z-last.eye_to.z),
                std::abs(std::remainder(double(cam.yaw)-std::atan2(dx,-dz),6.283185307179586)),
                std::abs(double(cam.pitch)-std::atan2(dy,std::hypot(dx,dz))),std::abs(double(cam.fov)-last.fov_to)});
            if(opening_entry_qa_handoff_error>.0001){
                opening_prologue_qa_fail("Final cinematic camera did not match the playable spawn and FOV");return;
            }
            opening_entry_qa_handoff=true;
            opening_prologue_qa_pass("house_cinematic_control_lock");
            opening_prologue_qa_pass("house_cinematic_natural");
            opening_prologue_qa_pass("house_cinematic_all_shots");
            opening_prologue_qa_pass("house_cinematic_match_handoff");
            opening_prologue_qa_capture_pending="house-handoff";
            opening_prologue_qa_next(qa_frame);return;
        }
        if(age<90)return;
        if(!opening_prologue_qa_stationary()){opening_prologue_qa_fail("House cinematic accepted movement/look input");return;}
        opening_prologue_qa_pass("house_cinematic_control_lock");
        opening_prologue_qa_capture_pending="house-establish";
        opening_prologue_qa_skips=opening_state.stats().skips;opening_prologue_qa_key(SDLK_RETURN,true);
        opening_prologue_qa_next(qa_frame);return;
    case 3:
        if(stage==Stage::HouseEstablish){if(age>100)opening_prologue_qa_fail("Hold confirm did not skip the house cinematic");return;}
        if(stage!=Stage::HouseExplore||opening_state.stats().skips!=opening_prologue_qa_skips+(opening_entry_qa_enabled?0u:1u)){opening_prologue_qa_fail("House entry entered the wrong stage or skip state");return;}
        if(!opening_prologue_qa_handoff_begin){
            opening_prologue_qa_handoff_begin=qa_frame;
            opening_prologue_qa_key(SDLK_RETURN,false);if(!opening_entry_qa_enabled)opening_prologue_qa_pass("house_skip");
            return;
        }
        if(opening_prologue_qa_handoff_frames<6){
            if(!opening_frame.controls_enabled||preview_screen!=1)return;
            if(std::abs(int(SDL_GameControllerGetAxis(preview_pad.get(),SDL_CONTROLLER_AXIS_LEFTY)))<19000||
               std::abs(int(SDL_GameControllerGetAxis(preview_pad.get(),SDL_CONTROLLER_AXIS_RIGHTX)))<19000){opening_prologue_qa_fail("Handoff did not retain nonneutral movement/look sticks");return;}
            if(!opening_prologue_qa_stationary()){opening_prologue_qa_fail("Held cinematic input moved or turned the player before neutral handoff");return;}
            ++opening_prologue_qa_handoff_frames;
            if(opening_prologue_qa_handoff_frames<6)return;
            opening_prologue_qa_axes(0,0);opening_prologue_qa_handoff_release=qa_frame;
            return;
        }
        // A complete neutral poll must arm ordinary movement/look before a
        // new right-stick action. Do not replace held axes with another action.
        if(qa_frame<opening_prologue_qa_handoff_release+3)return;
        opening_prologue_qa_pass("neutral_handoff");
        if(third_person||cam.fly||!cam.grounded){opening_prologue_qa_fail("Opening father did not enter grounded first-person play");return;}
        opening_prologue_qa_pass("first_person_father");opening_prologue_qa_yaw=cam.yaw;
        opening_prologue_qa_axes(0,0,22000);opening_prologue_qa_next(qa_frame);return;
    case 4:
        if(age<12)return;
        opening_prologue_qa_look=std::abs(std::remainder(double(cam.yaw)-opening_prologue_qa_yaw,6.28318530718));
        if(opening_prologue_qa_look<.05){opening_prologue_qa_fail("Right stick did not turn the playable camera");return;}
        opening_prologue_qa_pass("controller_look");opening_prologue_qa_axes(0,0);
        opening_prologue_qa_tap(SDLK_ESCAPE,qa_frame);opening_prologue_qa_next(qa_frame);return;
    case 5:
        if(age<3)return;
        if(preview_screen!=2){opening_prologue_qa_fail("Escape did not pause the opening");return;}
        opening_prologue_qa_position();opening_prologue_qa_clock=opening_state.stats().active_seconds;
        // Horizontal movement still tests the pause gate without intentionally
        // navigating away from Resume in the vertical pause menu.
        opening_prologue_qa_axes(32768,0,22000);opening_prologue_qa_next(qa_frame);return;
    case 6:
        if(age<15)return;
        if(!opening_prologue_qa_stationary()||std::abs(opening_state.stats().active_seconds-opening_prologue_qa_clock)>.0001){opening_prologue_qa_fail("Pause allowed player or story clock to advance");return;}
        opening_prologue_qa_pass("pause_freezes_player");opening_prologue_qa_axes(0,0);
        opening_prologue_qa_tap(SDLK_RETURN,qa_frame);opening_prologue_qa_next(qa_frame);return;
    case 7:
        if(opening_prologue_qa_menu_phase==0){
            if(age<6)return;
            if(preview_screen!=1||!opening_frame.controls_enabled){opening_prologue_qa_fail("Enter did not confirm the focused Resume button");return;}
            opening_prologue_qa_pass("resume");opening_prologue_qa_pass("menu_keyboard_confirm");
            controller_test_pad.press(SDL_CONTROLLER_BUTTON_START);++opening_prologue_qa_events;
            opening_prologue_qa_menu_phase=1;opening_prologue_qa_menu_frame=qa_frame;return;
        }
        if(opening_prologue_qa_menu_phase==1){
            if(qa_frame<opening_prologue_qa_menu_frame+8)return;
            if(preview_screen!=2||SDL_GameControllerGetButton(preview_pad.get(),SDL_CONTROLLER_BUTTON_START)){
                opening_prologue_qa_fail("Controller Start did not pause and release before menu confirmation");return;
            }
            controller_test_pad.button(SDL_CONTROLLER_BUTTON_A,true);++opening_prologue_qa_events;
            opening_prologue_qa_menu_phase=2;opening_prologue_qa_menu_frame=qa_frame;return;
        }
        if(opening_prologue_qa_menu_phase==2){
            if(qa_frame<opening_prologue_qa_menu_frame+2)return;
            controller_test_pad.button(SDL_CONTROLLER_BUTTON_A,false);++opening_prologue_qa_events;
            opening_prologue_qa_menu_phase=3;opening_prologue_qa_menu_frame=qa_frame;return;
        }
        if(qa_frame<opening_prologue_qa_menu_frame+6)return;
        if(preview_screen!=1||!opening_frame.controls_enabled||SDL_GameControllerGetButton(preview_pad.get(),SDL_CONTROLLER_BUTTON_A)){
            opening_prologue_qa_fail("Controller A did not confirm Resume and release to ordinary controls");return;
        }
        opening_prologue_qa_pass("controller_pause_menu");opening_prologue_qa_node=0;opening_prologue_qa_next(qa_frame);return;
    case 8:
        if(opening_room_lights_qa_enabled&&!opening_room_lights_qa_done){opening_room_lights_qa_tick(qa_frame);return;}
        if(!opening_prologue_qa_walk(opening_prologue_qa_home,1))return;
        opening_prologue_qa_pass("house_walk");opening_prologue_qa_tap(SDLK_e,qa_frame);opening_prologue_qa_next(qa_frame);return;
    case 9:
        if(age<3)return;
        if(!opening_state.inspected()||opening_state.checkpoint()!=world_opening_prologue::Checkpoint::HouseInspected){opening_prologue_qa_fail(opening_state.config().game_start_story?"Real bills interaction did not update the opening objective":"Real notebook interaction did not update the opening objective");return;}
        opening_prologue_qa_pass(opening_state.config().game_start_story?"bills_interaction":"notebook_interaction");
        opening_prologue_qa_capture_pending=opening_state.config().game_start_story?"house-bills":"house-notebook";
        opening_prologue_qa_tap(SDLK_F5,qa_frame);opening_prologue_qa_next(qa_frame);return;
    case 10:{
        if(age<3)return;
        std::ifstream file("saves/opening.progress");world_opening_prologue::Save saved;std::string error;
        if(!file||!world_opening_prologue::parse_save(file,opening_state.config(),saved,error)||saved.checkpoint!=world_opening_prologue::Checkpoint::HouseInspected){opening_prologue_qa_fail("F5 did not write the inspected-house checkpoint");return;}
        opening_prologue_qa_pass("checkpoint_save");opening_prologue_qa_node=2;opening_prologue_qa_next(qa_frame);return;
    }
    case 11:
        if(!opening_prologue_qa_walk(opening_prologue_qa_home,2))return;
        opening_prologue_qa_tap(SDLK_F8,qa_frame);opening_prologue_qa_next(qa_frame);return;
    case 12:{
        if(age<3)return;
        const auto &spawn=opening_state.config().house_spawn.feet;
        if(stage!=Stage::HouseExplore||!opening_state.inspected()||std::hypot(cam.x-spawn.x,cam.z-spawn.z)>.03||std::abs(cam.y-1.75-spawn.y)>.03){opening_prologue_qa_fail("F8 did not restore the supported checkpoint and inspected objective");return;}
        ++opening_prologue_qa_restores;opening_prologue_qa_pass("checkpoint_restore");
        if(opening_console_only_qa)opening_prologue_qa_capture_pending="console-objective";
        opening_prologue_qa_node=0;opening_prologue_qa_next(qa_frame);return;
    }
    case 13:
        if(!opening_prologue_qa_walk(opening_prologue_qa_home,unsigned(opening_prologue_qa_home.size()-1)))return;
        if(opening_state.config().game_start_story){
            opening_prologue_qa_pass("console_walk");
            // The same ordinary A interaction that reaches the console must
            // not also confirm New Game when its menu appears.
            controller_test_pad.button(SDL_CONTROLLER_BUTTON_A,true);++opening_prologue_qa_events;
        }else{
            opening_prologue_qa_pass("front_door_walk");opening_prologue_qa_tap(SDLK_e,qa_frame);
        }
        opening_prologue_qa_next(qa_frame);return;
    case 14:
        if(opening_state.config().game_start_story){
            const uint64_t game_age=qa_frame>=opening_prologue_qa_game_frame?qa_frame-opening_prologue_qa_game_frame:0;
            if(opening_prologue_qa_game_phase==0){
                if(age<6)return;
                if(stage!=Stage::GameMenu||opening_frame.controls_enabled||opening_state.checkpoint()!=world_opening_prologue::Checkpoint::HouseInspected){opening_prologue_qa_fail("Console A did not open the locked game menu at the bills checkpoint");return;}
                opening_prologue_qa_pass("game_menu_open");opening_prologue_qa_capture_pending="game-menu";
                opening_prologue_qa_position();opening_prologue_qa_axes(24000,-24000,24000);
                opening_prologue_qa_game_phase=1;opening_prologue_qa_game_frame=qa_frame;return;
            }
            if(opening_prologue_qa_game_phase==1){
                if(game_age<25)return;
                if(stage!=Stage::GameMenu||opening_frame.controls_enabled||!opening_prologue_qa_stationary()){
                    opening_prologue_qa_fail("Held console A auto-started New Game or allowed movement/look in its menu");return;
                }
                opening_prologue_qa_pass("game_menu_held_confirm");opening_prologue_qa_pass("game_menu_control_lock");
                opening_prologue_qa_axes(0,0);controller_test_pad.button(SDL_CONTROLLER_BUTTON_A,false);++opening_prologue_qa_events;
                opening_prologue_qa_game_phase=2;opening_prologue_qa_game_frame=qa_frame;return;
            }
            if(opening_prologue_qa_game_phase==2){
                // The menu must observe a complete neutral input poll before
                // a new cancel press, just as it does before New Game.
                if(game_age<5)return;
                if(stage!=Stage::GameMenu){opening_prologue_qa_fail("Releasing console A unexpectedly closed New Game");return;}
                opening_prologue_qa_tap(SDLK_ESCAPE,qa_frame);
                opening_prologue_qa_game_phase=3;opening_prologue_qa_game_frame=qa_frame;return;
            }
            if(opening_prologue_qa_game_phase==3){
                if(game_age<6)return;
                if(stage!=Stage::HouseExplore||preview_screen!=1||!opening_frame.controls_enabled||!opening_state.inspected()||opening_state.checkpoint()!=world_opening_prologue::Checkpoint::HouseInspected){opening_prologue_qa_fail("Game-menu cancel did not return to the playable apartment with bills progress");return;}
                opening_prologue_qa_pass("game_menu_cancel");opening_prologue_qa_tap(SDLK_e,qa_frame);
                opening_prologue_qa_game_phase=4;opening_prologue_qa_game_frame=qa_frame;return;
            }
            if(opening_prologue_qa_game_phase==4){
                if(game_age<6)return;
                if(stage!=Stage::GameMenu||opening_frame.controls_enabled){opening_prologue_qa_fail("Fresh E did not reopen New Game at the console");return;}
                opening_prologue_qa_pass("game_menu_reopen");
                controller_test_pad.button(SDL_CONTROLLER_BUTTON_A,true);++opening_prologue_qa_events;
                opening_prologue_qa_game_phase=5;opening_prologue_qa_game_frame=qa_frame;return;
            }
            if(game_age<3)return;
            if(stage!=Stage::Transport){opening_prologue_qa_fail("Fresh New Game A did not start transport");return;}
            controller_test_pad.button(SDL_CONTROLLER_BUTTON_A,false);++opening_prologue_qa_events;
            opening_prologue_qa_pass("game_menu_fresh_start");
        }else{
            if(age<3)return;
            if(stage!=Stage::Transport){opening_prologue_qa_fail("Front door interaction did not start transport");return;}
            opening_prologue_qa_pass("front_door_interaction");
        }
        opening_prologue_qa_position();opening_prologue_qa_axes(24000,-24000,24000);
        opening_prologue_qa_capture_pending="transport";opening_prologue_qa_next(qa_frame);return;
    case 15:
        if(age<(opening_state.config().game_start_story?170u:12u))return;
        if(!opening_prologue_qa_stationary()||opening_frame.controls_enabled){opening_prologue_qa_fail("Transport accepted movement/look input");return;}
        if(opening_state.config().game_start_story){
            if(stage!=Stage::Transport){opening_prologue_qa_fail("The late game-start transport capture missed its cinematic");return;}
            opening_prologue_qa_capture_pending="transport-glow";
        }
        opening_prologue_qa_pass("transport_control_lock");opening_prologue_qa_axes(0,0);
        if(opening_console_only_qa){
            if(!opening_state.config().game_start_story){opening_prologue_qa_fail("Console-only rehearsal requires the real TV New Game opening");return;}
            opening_prologue_qa_pass("console_only_complete");opening_prologue_qa_done=true;
            // Render the queued late-transport capture, then exit before the
            // authored outdoor relocation. Ordinary/full QA keeps its skip path.
            launch->frames=int(launch->rendered_frames+1);
            std::printf("[Opening QA] CONSOLE COMPLETE %zu checks, %u events, %.3fm walked; real New Game, no outdoor relocation\n",opening_prologue_qa_checks.size(),opening_prologue_qa_events,opening_prologue_qa_walked);std::fflush(stdout);return;
        }
        opening_prologue_qa_skips=opening_state.stats().skips;
        controller_test_pad.button(SDL_CONTROLLER_BUTTON_A,true);++opening_prologue_qa_events;
        opening_prologue_qa_next(qa_frame);return;
    case 16:
        if(stage==Stage::Transport){if(age>100)opening_prologue_qa_fail("Hold confirm did not skip transport");return;}
        if(stage!=Stage::MeadowReveal||opening_state.stats().skips!=opening_prologue_qa_skips+1){opening_prologue_qa_fail("Transport did not enter the Meadows of Valois reveal");return;}
        opening_prologue_qa_pass("transport_skip");opening_prologue_qa_next(qa_frame);return;
    case 17:
        // Holding the same confirm through a scene boundary must not cascade.
        if(age<65)return;
        if(stage!=Stage::MeadowReveal||opening_state.stats().skips!=opening_prologue_qa_skips+1){opening_prologue_qa_fail("Held confirm cascaded across the transport boundary");return;}
        controller_test_pad.button(SDL_CONTROLLER_BUTTON_A,false);++opening_prologue_qa_events;
        opening_prologue_qa_pass("controller_cinematic_skip");opening_prologue_qa_capture_pending="meadow-reveal";
        opening_prologue_qa_next(qa_frame);return;
    case 18:
        if(age>1800){opening_prologue_qa_fail("Meadow reveal did not become ready for neutral confirmation");return;}
        if(destination_loading||preview_screen!=1||show_graphics||!preview_pad.focused||(SDL_GetWindowFlags(window)&SDL_WINDOW_MINIMIZED)){
            opening_prologue_qa_reveal_neutral_frames=0;return;
        }
        // Loading pauses the core and disarms skipping on resume. Give it
        // complete neutral, unpaused ticks before beginning a fresh hold.
        if(++opening_prologue_qa_reveal_neutral_frames<5)return;
        opening_prologue_qa_reveal_active_frames=0;
        opening_prologue_qa_key(SDLK_RETURN,true);opening_prologue_qa_next(qa_frame);return;
    case 19:
        if(destination_loading||preview_screen!=1||show_graphics||!preview_pad.focused||(SDL_GetWindowFlags(window)&SDL_WINDOW_MINIMIZED)){
            opening_prologue_qa_key(SDLK_RETURN,false);opening_prologue_qa_reveal_neutral_frames=0;
            opening_prologue_qa_phase=18;opening_prologue_qa_action_frame=qa_frame;return;
        }
        ++opening_prologue_qa_reveal_active_frames;
        if(stage==Stage::MeadowReveal){if(opening_prologue_qa_reveal_active_frames>100)opening_prologue_qa_fail("Hold confirm did not skip the meadow reveal");return;}
        if(stage!=Stage::MeadowExplore||!opening_frame.controls_enabled){opening_prologue_qa_fail("Meadow reveal did not return playable control");return;}
        opening_prologue_qa_key(SDLK_RETURN,false);opening_prologue_qa_pass("meadow_reveal");
        opening_prologue_qa_jump_floor=cam.y;opening_prologue_qa_jump_apex=cam.y;
        controller_test_pad.press(SDL_CONTROLLER_BUTTON_X);++opening_prologue_qa_events;opening_prologue_qa_next(qa_frame);return;
    case 20:
        opening_prologue_qa_jump_apex=std::max(opening_prologue_qa_jump_apex,cam.y);
        if(age<10)return;
        if(cam.grounded&&std::abs(cam.y-opening_prologue_qa_jump_floor)<.04){
            if(opening_prologue_qa_jump_apex-opening_prologue_qa_jump_floor<.4){opening_prologue_qa_fail("Controller jump did not rise before landing");return;}
            opening_prologue_qa_pass("meadow_jump_landing");opening_prologue_qa_node=0;opening_prologue_qa_next(qa_frame);return;
        }
        if(age>160)opening_prologue_qa_fail("Controller jump did not land on the meadow terrain");
        return;
    case 21:
        if(opening_state.config().arrival_camp.enabled&&!opening_arrival_qa_done){opening_arrival_qa_tick(qa_frame);return;}
        if(!opening_prologue_qa_walk(opening_prologue_qa_meadow,unsigned(opening_prologue_qa_meadow.size()-1)))return;
        opening_prologue_qa_pass("meadow_walk");controller_test_pad.press(SDL_CONTROLLER_BUTTON_A);++opening_prologue_qa_events;opening_prologue_qa_next(qa_frame);return;
    case 22:
        if(age<3)return;
        if(stage!=Stage::Complete||opening_state.stats().meadow_interactions!=1){opening_prologue_qa_fail("Actual waystone interaction did not complete the opening objective");return;}
        opening_prologue_qa_pass("waystone_interaction");opening_prologue_qa_pass("controller_interaction");opening_prologue_qa_capture_pending="waystone-complete";
        opening_prologue_qa_tap(SDLK_r,qa_frame);opening_prologue_qa_next(qa_frame);return;
    case 23:{
        if(age<3)return;
        const auto &spawn=opening_state.config().meadow_spawn.feet;
        if(stage!=Stage::Complete||std::hypot(cam.x-spawn.x,cam.z-spawn.z)>.03||std::abs(cam.y-1.75-spawn.y)>.1||third_person||cam.fly||!cam.grounded){opening_prologue_qa_fail("Retry checkpoint did not preserve progress and return a supported ordinary father");return;}
        ++opening_prologue_qa_retries;opening_prologue_qa_pass("checkpoint_retry");
        opening_prologue_qa_axes(0,0);opening_prologue_qa_done=true;
        launch->frames=int(launch->rendered_frames+1);
        std::printf("[Opening QA] COMPLETE %zu checks, %u events, %.3fm actual movement; zero QA player/story writes\n",opening_prologue_qa_checks.size(),opening_prologue_qa_events,opening_prologue_qa_walked);std::fflush(stdout);return;
    }
    default:opening_prologue_qa_fail("Invalid opening QA phase");return;
    }
};
auto opening_prologue_qa_capture=[&](){
    if(!opening_qa)return;
    bool film=false;
    if(opening_entry_film_enabled&&!opening_state.config().house_shots.empty()&&!destination_loading&&
       opening_frame.stage==world_opening_prologue::Stage::HouseEstablish){
        film=(opening_entry_film_rendered++%4u)==0;
    }
    const std::string name=opening_prologue_qa_capture_pending;opening_prologue_qa_capture_pending.clear();
    const bool still=!name.empty()&&opening_prologue_qa_capture_dir&&*opening_prologue_qa_capture_dir;
    if(!still&&!film)return;
    if(film&&opening_entry_film_frames>=700){opening_prologue_qa_fail("Bounded apartment film capture exceeded 700 frames");return;}
    std::vector<unsigned char> pixels(size_t(display_w)*display_h*4);
    glBindFramebuffer(GL_FRAMEBUFFER,0);glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadPixels(0,0,display_w,display_h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    const bool gl_ok=glGetError()==GL_NO_ERROR;
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,display_w,display_h,32,SDL_PIXELFORMAT_RGBA32);
    bool ok=gl_ok&&surface;
    if(ok){for(int y=0;y<display_h;++y)std::memcpy(static_cast<unsigned char*>(surface->pixels)+size_t(y)*surface->pitch,pixels.data()+size_t(display_h-1-y)*display_w*4,size_t(display_w)*4);}
    if(ok&&still){
        const auto path=std::filesystem::path(opening_prologue_qa_capture_dir)/(name+".bmp");
        ok=SDL_SaveBMP(surface,path.u8string().c_str())==0;
        if(ok)++opening_prologue_qa_captures;
    }
    if(ok&&film){
        char filename[32];std::snprintf(filename,sizeof filename,"entry-%04u.bmp",opening_entry_film_frames);
        const auto root=std::filesystem::path(opening_entry_film_dir);
        ok=SDL_SaveBMP(surface,(root/filename).u8string().c_str())==0;
        if(ok){
            const Uint64 now=SDL_GetPerformanceCounter();if(!opening_entry_film_started)opening_entry_film_started=now;
            std::ofstream record(root/"manifest.jsonl",std::ios::app);record.imbue(std::locale::classic());
            record<<std::setprecision(17)<<"{\"file\": "<<world_json::quote(filename)
                  <<", \"output_frame\": "<<opening_entry_film_frames<<", \"rendered_frame\": "<<(opening_entry_film_rendered-1)
                  <<", \"story_seconds\": "<<opening_state.elapsed()<<", \"shot_index\": "<<opening_frame.house_shot_index
                  <<", \"shot_progress\": "<<opening_frame.house_shot_progress<<", \"wall_seconds\": "
                  <<double(now-opening_entry_film_started)/double(SDL_GetPerformanceFrequency())
                  <<", \"width\": "<<display_w<<", \"height\": "<<display_h<<"}\n";
            record.flush();ok=bool(record);if(ok)++opening_entry_film_frames;
        }
    }
    if(surface)SDL_FreeSurface(surface);
    if(!ok){++opening_prologue_qa_capture_failures;opening_prologue_qa_fail("Native opening HUD or film capture failed");return;}
};
auto opening_prologue_qa_report=[&](FILE *file){
    std::fprintf(file,"  \"opening_arrival_portraits\": {\"facing_errors_radians\": [%.9f,%.9f,%.9f], \"maximum_allowed_radians\": 0.06, \"qa_player_pose_writes\": 0},\n",opening_arrival_qa_facing_errors[0],opening_arrival_qa_facing_errors[1],opening_arrival_qa_facing_errors[2]);
    std::fprintf(file,"  \"opening_arrival_qa\": {\"enabled\": %s, \"passed\": %s, \"phase\": %u, \"actors_completed\": %u, \"qa_player_pose_writes\": 0, \"qa_story_state_writes\": 0},\n",opening_state.config().arrival_camp.enabled?"true":"false",opening_arrival_qa_done?"true":"false",opening_arrival_qa_phase,opening_arrival_qa_actor);
    std::fprintf(file,"  \"opening_prologue_qa\": {\"enabled\": %s, \"passed\": %s, \"game_start_story\": %s, \"phase\": %u, \"events\": %u, \"movement_frames\": %u, \"walked_m\": %.9f, \"controller_look_radians\": %.9f, \"neutral_handoff_playable_frames\": %u, \"qa_player_pose_writes\": 0, \"qa_story_state_writes\": 0, \"checkpoint_restores\": %u, \"checkpoint_retries\": %u, \"captures\": %u, \"capture_failures\": %u, \"failure\": %s, \"checks\": [",
        opening_qa?"true":"false",opening_prologue_qa_done?"true":"false",opening_state.config().game_start_story?"true":"false",opening_prologue_qa_phase,opening_prologue_qa_events,opening_prologue_qa_movement_frames,opening_prologue_qa_walked,opening_prologue_qa_look,opening_prologue_qa_handoff_frames,opening_prologue_qa_restores,opening_prologue_qa_retries,opening_prologue_qa_captures,opening_prologue_qa_capture_failures,world_json::quote(opening_prologue_qa_failure).c_str());
    for(size_t i=0;i<opening_prologue_qa_checks.size();++i)std::fprintf(file,"%s%s",i?", ":"",world_json::quote(opening_prologue_qa_checks[i]).c_str());
    std::fprintf(file,"]},\n");
    std::fprintf(file,"  \"opening_entry_qa\": {\"enabled\": %s, \"shot_count\": %zu, \"shot_mask\": %u, \"cinematic_frames\": %u, \"observed_seconds\": %.9f, \"duration_seconds\": %.9f, \"matched_handoff\": %s, \"handoff_max_error\": %.12f, \"film_enabled\": %s, \"film_frames\": %u, \"film_rendered_frames\": %u},\n",
        opening_entry_qa_enabled?"true":"false",opening_state.config().house_shots.size(),opening_entry_qa_shots,
        opening_entry_qa_frames,opening_entry_qa_seconds,world_opening_prologue::house_duration(opening_state.config()),
        opening_entry_qa_handoff?"true":"false",opening_entry_qa_handoff_error,
        opening_entry_film_enabled?"true":"false",opening_entry_film_frames,opening_entry_film_rendered);
};

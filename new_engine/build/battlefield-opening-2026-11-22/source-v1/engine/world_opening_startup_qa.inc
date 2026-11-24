// Bounded, marked input replay only. Never writes story state or player pose.
const char *opening_startup_option=std::getenv("VALWOOD_QA_OPENING_STARTUP_ONLY");
const bool opening_startup_qa=opening_qa&&opening_startup_option&&std::strcmp(opening_startup_option,"1")==0;
const char *opening_startup_natural_option=std::getenv("VALWOOD_QA_OPENING_STARTUP_NATURAL");
const bool opening_startup_natural=opening_startup_natural_option&&std::strcmp(opening_startup_natural_option,"1")==0;
unsigned opening_startup_phase=0,opening_startup_neutral=0,opening_startup_shots=0;
uint64_t opening_startup_started=0,opening_startup_action=0,opening_startup_handoff_frame=0;
unsigned opening_startup_settled_frames=0;std::uint64_t opening_startup_settled_plain_frames=0;
bool opening_startup_early_body_verified=false,opening_startup_settled_verified=false;
double opening_startup_seconds=0;
bool opening_startup_initialized=false,opening_startup_began=false,opening_startup_skipped=false;
auto opening_startup_write=[&](bool passed,const char *failure){
    const char *path=std::getenv("VALWOOD_QA_OPENING_STARTUP_RAW");
    if(!path||!*path){opening_prologue_qa_fail("Startup raw output path is required");return;}
    std::ofstream out(path);out.imbue(std::locale::classic());
    out<<std::setprecision(17)<<"{\"schema\":\"valwood.opening-load-fix.startup-raw.v1\",\"enabled\":true,\"passed\":"<<(passed?"true":"false")
       <<",\"natural_mode\":"<<(opening_startup_natural?"true":"false")<<",\"controller_begin\":"<<(opening_startup_began?"true":"false")
       <<",\"controller_skip\":"<<(opening_startup_skipped?"true":"false")<<",\"natural_seconds\":"<<opening_startup_seconds
       <<",\"neutral_handoff_frames\":"<<opening_startup_neutral
       <<",\"early_visible_body_verified\":"<<(opening_startup_early_body_verified?"true":"false")
       <<",\"settled_camera_checked\":"<<(opening_startup_settled_verified?"true":"false")
       <<",\"settled_handoff_frames\":"<<opening_startup_settled_frames
       <<",\"settled_capture\":\"house-third-person-ready\""
       <<",\"settled_transition_active\":"<<(opening_dialogue_blend.transitioning()?"true":"false")<<",\"shot_mask\":"<<opening_startup_shots
       <<",\"initial_held_frames\":84,\"events\":"<<opening_prologue_qa_events<<",\"captures\":"<<opening_prologue_qa_captures
       <<",\"third_person\":"<<(third_person?"true":"false")<<",\"civilian_ready\":"<<(story_civilian_ready?"true":"false")
       <<",\"armored\":"<<(story_appearance_armored?"true":"false")<<",\"plain_indoor_frames\":"<<story_plain_indoor_frames
       <<",\"equipment_weapon_visible\":"<<(skeletal_hero.stats().equipment_weapon_visible?"true":"false")
       <<",\"qa_story_state_writes\":0,\"qa_player_pose_writes\":0,\"failure\":"<<world_json::quote(failure)<<"}\n";
    out.flush();if(!out)opening_prologue_qa_fail("Startup raw output could not be written");
};
auto opening_startup_workflow=[&](uint64_t now){
    using Stage=world_opening_prologue::Stage;
    auto fail=[&](const char *why){opening_startup_write(false,why);opening_prologue_qa_fail(why);};
    if(!opening_startup_started)opening_startup_started=now+1;
    if(now>opening_startup_started+(opening_startup_natural?1900u:1100u)){fail("Bounded startup replay timed out");return;}
    controller_test_pad.tick();
    if(!opening_startup_initialized){
        std::ifstream marker(".valwood-opening-startup-qa");std::string token,extra;
        if(launch->benchmark||launch->frames>(opening_startup_natural?2000:1200)||!(marker>>token)||token!="VALWOOD_PRIVATE_OPENING_STARTUP_V1"||(marker>>extra)||
           !opening_prologue_qa_capture_dir||!*opening_prologue_qa_capture_dir||!std::getenv("VALWOOD_QA_OPENING_STARTUP_RAW")||
           std::filesystem::exists("cinematics/world.vcuts")||std::filesystem::exists("saves/opening.progress")||
           !controller_test_pad.joystick||preview_pad.id()!=SDL_JoystickInstanceID(controller_test_pad.joystick)){
            fail("Startup replay requires a marked fresh no-library project and its own SDL virtual controller");return;
        }
        SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_PumpEvents();
        opening_startup_initialized=true;
    }
    const auto flags=SDL_GetWindowFlags(window);
    if(!preview_pad.focused||!(flags&SDL_WINDOW_INPUT_FOCUS)){
        SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_PumpEvents();return;
    }
    if(destination_loading||show_graphics||!terrain.view_ready(cam.x,cam.z,false))return;
    const auto stage=opening_state.stage();
    auto button=[&](bool held){controller_test_pad.button(SDL_CONTROLLER_BUTTON_A,held);SDL_JoystickUpdate();++opening_prologue_qa_events;};
    switch(opening_startup_phase){
    case 0:
        if(stage!=Stage::Title||opening_have_save){fail("Fresh startup did not begin at its unsaved title");return;}
        opening_prologue_qa_position();opening_prologue_qa_capture_pending="fresh-title";
        opening_startup_action=now;opening_startup_phase=1;return;
    case 1:
        if(now-opening_startup_action<3)return;
        button(true);opening_startup_action=now;opening_startup_phase=2;return;
    case 2:
        if(stage==Stage::Title){if(now-opening_startup_action>30)fail("Actual controller A did not activate Begin");return;}
        if(stage!=Stage::HouseEstablish){fail("Controller Begin entered the wrong opening stage");return;}
        opening_startup_began=true;opening_startup_action=now;opening_startup_phase=3;return;
    case 3:
        if(stage!=Stage::HouseEstablish||opening_state.stats().skips||!opening_prologue_qa_stationary()){
            fail("Initial held A skipped the cinematic or moved its locked player");return;
        }
        if(now-opening_startup_action<84)return;
        button(false);opening_startup_action=now;opening_startup_phase=4;return;
    case 4:
        opening_startup_seconds=std::max(opening_startup_seconds,opening_state.elapsed());
        if(stage==Stage::HouseEstablish){
            if(!opening_prologue_qa_stationary()){fail("Natural house cinematic moved the logical player");return;}
            if(opening_frame.house_shot_progress>.4)opening_startup_shots|=1u<<opening_frame.house_shot_index;
            if(opening_state.elapsed()<.8)return;
            if(opening_frame.house_shot_index==0&&opening_prologue_qa_captures<2&&opening_frame.camera.fade>.05){fail("Advancing first house shot remained fully faded");return;}
            if(opening_startup_natural){
                if(!opening_startup_shots||opening_prologue_qa_captures<2)opening_prologue_qa_capture_pending="house-advancing";
                return;
            }
            opening_prologue_qa_capture_pending="house-advancing";button(true);opening_startup_action=now;opening_startup_phase=5;return;
        }
        if(!opening_startup_natural||stage!=Stage::HouseExplore||opening_state.stats().skips||opening_startup_shots!=15u){
            fail("Natural cinematic did not traverse all four authored shots");return;
        }
        opening_prologue_qa_capture_pending="house-handoff";opening_startup_handoff_frame=now;opening_startup_phase=6;return;
    case 5:
        if(stage==Stage::HouseEstablish){if(now-opening_startup_action>100)fail("Fresh held controller A did not skip");return;}
        if(stage!=Stage::HouseExplore||opening_state.stats().skips!=1){fail("Controller skip entered the wrong stage or repeated");return;}
        opening_startup_skipped=true;button(false);opening_prologue_qa_capture_pending="house-after-skip";opening_startup_handoff_frame=now;opening_startup_phase=6;return;
    case 6:
        if(stage!=Stage::HouseExplore){fail("Controller handoff cascaded into another stage");return;}
        if(!opening_controls_enabled||!opening_movement_armed)return;
        if(++opening_startup_neutral<6)return;
        if(!third_person){fail("Fresh journey did not default to third person");return;}
        if(story_civilian_requested&&(!story_civilian_ready||story_appearance_armored||!story_plain_indoor_frames||
            !skeletal_hero_active||skeletal_hero.stats().equipment_weapon_visible)){
            fail("Fresh journey did not draw a plain dressed, unarmed father indoors");return;
        }
        if(opening_prologue_qa_captures<3){fail("Startup replay missed an early framebuffer capture");return;}
        opening_startup_early_body_verified=true;opening_prologue_qa_position();opening_startup_phase=7;return;
    case 7:
        if(stage!=Stage::HouseExplore||!third_person||!opening_prologue_qa_stationary()){
            fail("Settling the third-person handoff changed story stage, view or logical player pose");return;
        }
        if(SDL_GameControllerGetButton(preview_pad.get(),SDL_CONTROLLER_BUTTON_A)||
           SDL_GameControllerGetAxis(preview_pad.get(),SDL_CONTROLLER_AXIS_LEFTX)||SDL_GameControllerGetAxis(preview_pad.get(),SDL_CONTROLLER_AXIS_LEFTY)||
           SDL_GameControllerGetAxis(preview_pad.get(),SDL_CONTROLLER_AXIS_RIGHTX)||SDL_GameControllerGetAxis(preview_pad.get(),SDL_CONTROLLER_AXIS_RIGHTY)){
            fail("Settled camera review requires genuinely neutral controller input");return;
        }
        // Keep the early body visibility assertion above, then wait beyond the
        // complete0.45s authored handoff. A transient close-up is not the ready view.
        if(now-opening_startup_handoff_frame<36||opening_dialogue_blend.transitioning()||!opening_controls_enabled||!opening_movement_armed){
            if(now-opening_startup_handoff_frame>240)fail("Third-person camera handoff did not settle under neutral controls");return;
        }
        if(!opening_last_render_camera_valid||!world_opening_dialogue_camera::finite(opening_last_render_camera)||
           (story_civilian_requested&&(!story_civilian_ready||story_appearance_armored||!skeletal_hero_active||skeletal_hero.stats().equipment_weapon_visible))){
            fail("Settled third-person frame lacks the supported plain unarmed hero or finite camera");return;
        }
        opening_startup_settled_frames=unsigned(now-opening_startup_handoff_frame);
        opening_startup_settled_plain_frames=story_plain_indoor_frames;
        opening_prologue_qa_capture_pending="house-third-person-ready";opening_startup_phase=8;return;
    case 8:
        if(!opening_prologue_qa_capture_pending.empty())return;
        if(stage!=Stage::HouseExplore||!third_person||opening_dialogue_blend.transitioning()||opening_prologue_qa_captures<4||
           (story_civilian_requested&&story_plain_indoor_frames<=opening_startup_settled_plain_frames)){
            fail("Settled third-person screenshot did not capture the actually drawn plain hero");return;
        }
        opening_startup_settled_verified=true;
        opening_prologue_qa_pass("settled_third_person_ready_frame_after_visible_handoff");
        opening_startup_write(true,"");if(!running)return;
        opening_prologue_qa_done=true;opening_prologue_qa_pass("fresh_controller_begin_natural_clock_and_skip");
        launch->frames=int(launch->rendered_frames+1);return;
    default:fail("Invalid startup replay phase");return;
    }
};

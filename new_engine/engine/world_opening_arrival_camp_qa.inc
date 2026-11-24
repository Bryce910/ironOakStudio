// This opt-in driver acts through the same SDL controls as a player. The
// authored approach lanes stay south/east of tents, benches and the firepit.
unsigned opening_arrival_qa_phase=0,opening_arrival_qa_actor=0,opening_arrival_qa_restore_before=0;
uint64_t opening_arrival_qa_frame=0;
bool opening_arrival_qa_done=false,opening_arrival_qa_path_ready=false;
std::array<double,3> opening_arrival_qa_facing_errors={{-1,-1,-1}};
std::vector<world_opening_prologue::Vec> opening_arrival_qa_path;
auto opening_arrival_qa_tick=[&](uint64_t qa_frame)->bool{
    const auto &config=opening_state.config().arrival_camp;
    if(!config.enabled)return true;
    const auto &camp=opening_state.camp();
    auto next=[&](unsigned phase){opening_arrival_qa_phase=phase;opening_arrival_qa_frame=qa_frame;};
    const uint64_t age=qa_frame>=opening_arrival_qa_frame?qa_frame-opening_arrival_qa_frame:0;
    auto point=[&](double x,double z){return world_opening_prologue::Vec{config.center.x+x,config.center.y,config.center.z+z};};
    auto begin_path=[&](std::initializer_list<world_opening_prologue::Vec> path){opening_arrival_qa_path=path;opening_prologue_qa_node=0;opening_arrival_qa_path_ready=true;};
    auto facing_error=[&](){
        WorldTrailCamper person;
        if(!world_trail_camp_actor(arrival_camp,opening_arrival_qa_actor,&person))return 10.;
        return std::abs(std::remainder(double(person.yaw)-std::atan2(cam.x-person.x,cam.z-person.z),6.283185307179586));
    };
    auto aimed=[&](){
        if(!opening_movement_armed){opening_prologue_qa_axes(0,0);return false;}
        const auto &actor=config.actors[opening_arrival_qa_actor];
        const double dx=actor.feet.x-cam.x,dz=actor.feet.z-cam.z;
        const double yaw=std::remainder(std::atan2(dx,-dz)-double(cam.yaw),6.283185307179586);
        const double pitch=std::atan2(actor.feet.y+(actor.seated?1.12:1.55)-cam.y,std::hypot(dx,dz))-double(cam.pitch);
        if(std::abs(yaw)<.045&&std::abs(pitch)<.045){opening_prologue_qa_axes(0,0);return true;}
        auto axis=[&](double error){if(std::abs(error)<.035)return Sint16(0);const double raw=preview_settings.deadzone+(1-preview_settings.deadzone)*std::clamp(std::abs(error)*2.5,.15,.75);return Sint16(std::copysign(raw*32767,error));};
        opening_prologue_qa_axes(0,0,axis(yaw));
        controller_test_pad.axis(SDL_CONTROLLER_AXIS_RIGHTY,axis(-pitch*(preview_settings.invert_y?-1.:1.)));
        return false;
    };
    switch(opening_arrival_qa_phase){
    case 0:
        if(!opening_arrival_qa_path_ready){
            if(opening_arrival_qa_actor==0)begin_path({point(-2.15,1.8)});
            else if(opening_arrival_qa_actor==1)begin_path({point(2.2,1.8)});
            else begin_path({point(3.6,1.0),point(3.6,-2.0),point(1.9,-2.0),point(1.9,-2.9)});
        }
        if(!opening_prologue_qa_walk(opening_arrival_qa_path,unsigned(opening_arrival_qa_path.size()-1),.10))return false;
        opening_arrival_qa_path_ready=false;next(1);return false;
    case 1:
        if(!aimed())return false;
        if(world_opening_arrival_camp::nearest(config,{cam.x,cam.y-1.75,cam.z})!=int(opening_arrival_qa_actor)){opening_prologue_qa_fail("Authored first-contact approach did not target its actual camper");return false;}
        if(opening_arrival_qa_actor==0){
            const auto render=world_trail_camp_stats(arrival_camp);
            if(!render.active||render.actors!=3||render.pose_assets!=12||render.failures||render.unsafe_roots){opening_prologue_qa_fail("First-contact campsite did not render exactly three safe retained actors");return false;}
            opening_prologue_qa_pass("arrival_three_campers");
            controller_test_pad.button(SDL_CONTROLLER_BUTTON_A,true);++opening_prologue_qa_events;next(2);
        }else{opening_prologue_qa_tap(SDLK_e,qa_frame);next(6);}
        return false;
    case 2:
        if(age<5)return false;
        if(!camp.active()||camp.actor()!=0||camp.beat()!=0||opening_frame.controls_enabled){opening_prologue_qa_fail("Fresh controller A did not open Rowan's first conversation");return false;}
        opening_prologue_qa_pass("arrival_controller_start");
        opening_prologue_qa_position();opening_prologue_qa_axes(22000,-22000,22000);next(3);return false;
    case 3:
        if(age<24)return false;
        if(facing_error()>.06){if(age>120)opening_prologue_qa_fail("Speaking arrival camper did not face the actual player within two seconds");return false;}
        opening_arrival_qa_facing_errors[opening_arrival_qa_actor]=facing_error();opening_prologue_qa_capture_pending="arrival-rowan";
        if(!camp.active()||camp.actor()!=0||camp.beat()!=0||camp.spoken()!=0||!opening_prologue_qa_stationary()){opening_prologue_qa_fail("Held controller A skipped a camp line or dialogue accepted movement/look");return false;}
        opening_prologue_qa_pass("arrival_held_confirm");opening_prologue_qa_pass("arrival_dialogue_control_lock");
        opening_prologue_qa_axes(0,0);controller_test_pad.button(SDL_CONTROLLER_BUTTON_A,false);++opening_prologue_qa_events;next(4);return false;
    case 4:
        if(age<5)return false;
        opening_prologue_qa_tap(SDLK_ESCAPE,qa_frame);next(5);return false;
    case 5:
        if(age<5)return false;
        if(camp.active()||camp.spoken()!=0||preview_screen!=1){opening_prologue_qa_fail("Leaving an unfinished camp conversation granted credit or opened the pause menu");return false;}
        opening_prologue_qa_pass("arrival_cancel_no_credit");opening_prologue_qa_tap(SDLK_e,qa_frame);next(6);return false;
    case 6:
        if(age<5)return false;
        if(facing_error()>.06){if(age>120)opening_prologue_qa_fail("First-line camp portrait did not settle toward the actual player within two seconds");return false;}
        opening_arrival_qa_facing_errors[opening_arrival_qa_actor]=facing_error();
        if(!camp.active()||camp.actor()!=int(opening_arrival_qa_actor)||camp.beat()!=0){opening_prologue_qa_fail("Fresh E did not open the targeted first-contact exchange");return false;}
        opening_prologue_qa_capture_pending=std::string("arrival-")+(opening_arrival_qa_actor==0?"rowan-reopen":opening_arrival_qa_actor==1?"bram":"ellis");
        next(7);return false;
    case 7:
        if(age<6)return false;
        if(camp.active()){
            // Alternate ordinary E and virtual-pad A to verify both controls
            // advance one line per complete release, including the final line.
            if(camp.beat()%2){controller_test_pad.press(SDL_CONTROLLER_BUTTON_A);++opening_prologue_qa_events;}
            else opening_prologue_qa_tap(SDLK_e,qa_frame);
            opening_arrival_qa_frame=qa_frame;return false;
        }
        if(camp.spoken()!=((1u<<(opening_arrival_qa_actor+1))-1)){opening_prologue_qa_fail("Completed camp exchange did not commit exactly its speaker's progress bit");return false;}
        opening_prologue_qa_pass(opening_arrival_qa_actor==0?"arrival_rowan_complete":opening_arrival_qa_actor==1?"arrival_bram_complete":"arrival_ellis_complete");
        if(++opening_arrival_qa_actor<3){next(0);return false;}
        opening_prologue_qa_tap(SDLK_F5,qa_frame);next(8);return false;
    case 8:{
        if(age<5)return false;
        std::ifstream file("saves/opening.progress");world_opening_prologue::Save saved;std::string error;
        if(!file||!world_opening_prologue::parse_save(file,opening_state.config(),saved,error)||saved.camp_spoken!=7){opening_prologue_qa_fail("F5 did not persist all three first-contact conversations");return false;}
        opening_prologue_qa_pass("arrival_checkpoint_save");
        // Return through the clear eastern lane before restoring the supported
        // story checkpoint. No QA player pose or story field is assigned.
        begin_path({point(1.9,-2.0),point(3.6,-2.0),point(3.6,3.5)});next(9);return false;
    }
    case 9:
        if(!opening_prologue_qa_walk(opening_arrival_qa_path,unsigned(opening_arrival_qa_path.size()-1),.10))return false;
        opening_arrival_qa_restore_before=opening_restores;
        opening_prologue_qa_tap(SDLK_F8,qa_frame);next(10);return false;
    case 10:{
        if(age<6)return false;
        const auto &spawn=opening_state.config().meadow_spawn.feet;
        std::ifstream file("saves/opening.progress");world_opening_prologue::Save saved;std::string error;
        if(!file||!world_opening_prologue::parse_save(file,opening_state.config(),saved,error)||saved.camp_spoken!=7){opening_prologue_qa_fail("F8 arrival verification could not read the actual saved three-speaker progress");return false;}
        if(opening_restores!=opening_arrival_qa_restore_before+1||std::hypot(cam.x-spawn.x,cam.z-spawn.z)>.03||std::abs(cam.y-1.75-spawn.y)>.10||camp.spoken()!=7||camp.active()||opening_state.stage()!=world_opening_prologue::Stage::MeadowExplore){opening_prologue_qa_fail("F8 did not perform one supported checkpoint restore with all three camp conversations");return false;}
        ++opening_prologue_qa_restores;opening_prologue_qa_pass("arrival_checkpoint_restore");
        opening_prologue_qa_capture_pending="arrival-camp-met";opening_arrival_qa_done=true;
        opening_prologue_qa_node=0;opening_prologue_qa_axes(0,0);return true;
    }
    default:opening_prologue_qa_fail("Invalid first-contact QA phase");return false;
    }
};

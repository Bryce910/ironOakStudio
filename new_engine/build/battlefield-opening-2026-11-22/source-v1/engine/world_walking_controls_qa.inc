// Opt-in input-only integration route. The ordinary viewer owns all player,
// camera, collision and controller state. This observer never assigns them.
struct WalkingControlsStage {
    unsigned samples=0,allowed=0;double seconds=0,distance=0,tail_speed=0,tail_seconds=0;
    double start_x=0,start_z=0,start_yaw=0,last_yaw=0,tail_yaw=0,yaw_distance=0;
};
const char *walking_controls_view_env=std::getenv("VALWOOD_WALKING_QA_VIEW");
const char *walking_controls_output_env=std::getenv("VALWOOD_WALKING_QA_OUTPUT");
const bool walking_controls_view_valid=walking_controls_view_env&&
    (!std::strcmp(walking_controls_view_env,"third")||!std::strcmp(walking_controls_view_env,"first"));
const bool walking_controls_want_third=walking_controls_view_env&&!std::strcmp(walking_controls_view_env,"third");
const char *walking_controls_stowed_env=std::getenv("VALWOOD_WALKING_QA_STOWED");
const bool walking_controls_want_stowed=walking_controls_stowed_env&&!std::strcmp(walking_controls_stowed_env,"1");
bool walking_controls_stow_requested=false;
const bool walking_controls_boot_stowed=weapon_sheathed;
const bool walking_controls_world_neutral_required=story_civilian_ready&&!first_journey_enabled&&!arena_enabled&&!joust_enabled;
bool walking_controls_default_neutral_checked=false,walking_controls_draw_requested=false,walking_controls_draw_settled=false,walking_controls_default_capture=false;
unsigned walking_controls_default_neutral_samples=0;
unsigned long long walking_controls_default_wait_start=0;
double walking_controls_default_x=0,walking_controls_default_z=0;
unsigned walking_controls_relaxed_samples=0,walking_controls_full_idle_samples=0,walking_controls_full_moving_samples=0;float walking_controls_relaxed_peak=0;
const std::filesystem::path walking_controls_output=walking_controls_output_env?walking_controls_output_env:"";
const std::array<const char *,22> walking_controls_names={"ready","view","neutral","deadzone","small","small-release","full","diagonal","reverse","release","backward","backward-release","look","look-full","look-release","pause-request","menu-held","menu-neutral","resume-request","resumed-neutral","fresh-small","final-release"};
// The captured locomotion profile allows complete source takes before judging
// the settled tail. The original input/controller regression profile is kept.
const bool walking_controls_captured_profile=qa_walking_controls&&std::getenv("VALWOOD_QA_CAPTURED_WALKING");
const std::array<double,22> walking_controls_durations=walking_controls_captured_profile?
    std::array<double,22>{0,0,1.20,.35,2.50,1.90,2.50,2.50,2.50,1.90,2.50,1.90,.40,.40,.40,0,.35,.20,0,.60,2.50,1.90}:
    std::array<double,22>{0,0,1.20,.35,.80,.40,.80,.80,.80,1.20,.80,.50,.40,.40,.40,0,.35,.20,0,.60,.60,.50};
std::array<WalkingControlsStage,22> walking_controls_stages={};
unsigned walking_controls_phase=0,walking_controls_checks=0,walking_controls_captures=0,walking_controls_view_switches=0;
unsigned long long walking_controls_samples=0,walking_controls_events=0,walking_controls_frame=0,walking_controls_begin_frame=0;
bool walking_controls_passed=false,walking_controls_failed=false,walking_controls_view_requested=false,walking_controls_action_sent=false;
double walking_controls_age=0,walking_controls_wait=0,walking_controls_previous_x=0,walking_controls_previous_z=0,walking_controls_previous_yaw=0;
bool walking_controls_previous_valid=false;
std::string walking_controls_capture_pending;unsigned walking_controls_capture_phase=0;
std::vector<std::string> walking_controls_labels,walking_controls_capture_names;
std::ofstream walking_controls_csv;
auto walking_controls_axes=[&](Sint16 x,Sint16 y,Sint16 look=0){
    controller_test_pad.axis(SDL_CONTROLLER_AXIS_LEFTX,x);controller_test_pad.axis(SDL_CONTROLLER_AXIS_LEFTY,y);
    controller_test_pad.axis(SDL_CONTROLLER_AXIS_RIGHTX,look);controller_test_pad.axis(SDL_CONTROLLER_AXIS_RIGHTY,0);
    SDL_JoystickUpdate();walking_controls_events+=4;
};
auto walking_controls_fail=[&](const char *reason){
    if(walking_controls_failed)return;
    walking_controls_failed=true;walking_controls_axes(0,0);
    std::fprintf(stderr,"[Walking QA] FAIL phase%u %s\n",walking_controls_phase,reason);
    launch->exit_code=1;running=false;
};
auto walking_controls_check=[&](bool ok,const char *label){
    if(!ok){walking_controls_fail(label);return false;}
    ++walking_controls_checks;walking_controls_labels.emplace_back(label);
    std::printf("[Walking QA] PASS %s\n",label);return true;
};
auto walking_controls_key=[&](SDL_Keycode code){
    for(bool down:{true,false}){SDL_Event e={};e.type=down?SDL_KEYDOWN:SDL_KEYUP;e.key.windowID=SDL_GetWindowID(window);
        e.key.state=down?SDL_PRESSED:SDL_RELEASED;e.key.keysym.sym=code;e.key.keysym.scancode=SDL_GetScancodeFromKey(code);e.key.keysym.unused=0x56414c54u;
        if(SDL_PushEvent(&e)!=1){walking_controls_fail("tagged SDL key rejected");return;}++walking_controls_events;}
};
auto walking_controls_enter=[&](unsigned phase){
    walking_controls_phase=phase;walking_controls_age=0;walking_controls_action_sent=false;
    auto &s=walking_controls_stages[phase];s={};s.start_x=cam.x;s.start_z=cam.z;s.start_yaw=s.last_yaw=cam.yaw;
    Sint16 x=0,y=0,look=0;
    if(phase==3)y=-1000;
    if(phase==4||phase==20)y=-14000;
    if(phase==6||phase==16)y=-32768;
    if(phase==7){x=32767;y=-32768;}
    if(phase==8){x=-32768;y=32767;}
    if(phase==10)y=32767;
    if(phase==12)look=14000;
    if(phase==13)look=32767;
    walking_controls_axes(x,y,look);
};
auto walking_controls_stage_check=[&](){
    const auto &s=walking_controls_stages[walking_controls_phase];const double tail=s.tail_seconds>0?s.tail_speed/s.tail_seconds:0;
    const double full=walking_controls_stages[6].tail_seconds>0?walking_controls_stages[6].tail_speed/walking_controls_stages[6].tail_seconds:0;
    const double maximum=double(world_atlas_walk_speed());bool ok=s.samples>=3&&s.seconds>0;
    switch(walking_controls_phase){
      case 2:case 3:case 19:ok=ok&&s.distance<.003&&tail<.03;break;
      case 4:case 20:ok=ok&&tail>.05&&tail<maximum*.85;break;
      case 6:case 10:ok=ok&&tail>maximum*.70&&tail<maximum*1.10;break;
      case 7:case 8:ok=ok&&full>.1&&tail>full*.70&&tail<full*1.08;break;
      case 5:case 9:case 11:case 21:ok=ok&&tail<.03;break;
      case 12:ok=ok&&s.yaw_distance>.002&&s.distance<.003;break;
      case 13:{const auto &small=walking_controls_stages[12];const double small_rate=small.seconds>0?small.yaw_distance/small.seconds:0;
        const double rate=s.yaw_distance/s.seconds;ok=ok&&small_rate>0&&rate>small_rate*1.25&&rate<=2.2*preview_settings.sensitivity*1.15&&s.distance<.003;break;}
      case 14:ok=ok&&tail<.03&&s.yaw_distance<.002;break;
      case 16:case 17:ok=ok&&s.allowed==0&&s.distance<.0001;break;
      default:break;
    }
    return walking_controls_check(ok,(std::string("ordinary input phase ")+walking_controls_names[walking_controls_phase]).c_str());
};
auto walking_controls_workflow=[&](unsigned long long current_frame){
    if(!qa_walking_controls||walking_controls_passed||walking_controls_failed||!running)return;
    walking_controls_frame=current_frame;controller_test_pad.tick();
    if(!walking_controls_view_valid||walking_controls_output.empty()||!walking_controls_output.is_absolute()||!std::filesystem::is_directory(walking_controls_output)){
        walking_controls_fail("bounded private output and explicit view required");return;}
    if(current_frame+10>=launch->frames){walking_controls_fail("frame budget exhausted");return;}
    const bool focused=(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)&&preview_pad.focused;
    if(walking_controls_phase==0){
        walking_controls_axes(0,0);
        if(!focused||destination_loading||current_frame<=launch->warmup||!terrain.view_ready(cam.x,cam.z,false)||world_scenery_streaming_stats(scenery).critical_pending)return;
        if(!walking_controls_check(!cam.fly&&cam.grounded&&!cam.swimming&&!studio.open&&!studio.cinematic_preview&&!preview_blocked()&&!fight.engaged(),"naturally focused grounded exploration"))return;
        if(!walking_controls_check(controller_test_pad.joystick&&preview_pad.id()==SDL_JoystickInstanceID(controller_test_pad.joystick),"owned real SDL virtual controller"))return;
        if(!walking_controls_check(active_playtest_walk||active_city_physics,"production collision route available"))return;
        if(walking_controls_want_stowed&&walking_controls_world_neutral_required&&!walking_controls_boot_stowed){walking_controls_fail("plain World exploration did not boot stowed");return;}
        if(walking_controls_want_stowed&&walking_controls_boot_stowed){
            // Observe the actual startup pose before requesting any view or
            // equipment control. A later Z toggle must not hide a bad default.
            if(!walking_controls_default_wait_start){walking_controls_default_wait_start=current_frame;walking_controls_default_x=cam.x;walking_controls_default_z=cam.z;}
            if(current_frame>walking_controls_default_wait_start+240){walking_controls_fail("default neutral did not settle before controls");return;}
            if(!weapon_sheathed||!skeletal_hero_loaded||!skeletal_hero.active().full_unarmed_settled()||
               !skeletal_hero.stats().full_unarmed||skeletal_hero.stats().full_unarmed_transition||
               skeletal_hero.stats().full_unarmed_weight<.999f||skeletal_hero.stats().full_unarmed_idle_frames<6||
               std::hypot(preview_pad.move.x,preview_pad.move.y)>.0001||std::hypot(player_move.vx,player_move.vz)>.025)return;
            if(++walking_controls_default_neutral_samples<6)return;
            if(!walking_controls_check(std::hypot(cam.x-walking_controls_default_x,cam.z-walking_controls_default_z)<.003,"initial default is settled full-body neutral before controls"))return;
            walking_controls_default_neutral_checked=true;
            if(third_person==walking_controls_want_third){
                if(!walking_controls_capture_pending.empty()){walking_controls_fail("initial neutral checkpoint overlap");return;}
                walking_controls_capture_pending="walking-start";walking_controls_capture_phase=0;walking_controls_default_capture=true;
            }
        }
        const auto csv_path=walking_controls_output/"walking.frames.csv";
        if(std::filesystem::exists(csv_path)){walking_controls_fail("preserve previous frame samples");return;}
        walking_controls_csv.open(csv_path);walking_controls_csv.imbue(std::locale::classic());walking_controls_csv<<std::setprecision(17);
        walking_controls_csv<<"frame,phase,label,view,dt,input_allowed,focused,raw_x,raw_y,raw_look_x,command_x,command_y,x,y,z,yaw,dx,dz,speed,gait_phase,gait_weight,gait_speed,pose_slot,pose_weight,velocity_x,velocity_z,walk_speed,collision_steps,slope_steps,sliding_steps,weapon_stowed,relaxed_walk_weight,captured_mode,captured_clip,captured_time,captured_acceptance,captured_starts,captured_stops,captured_pivots\n";
        if(!walking_controls_csv){walking_controls_fail("private sample file unavailable");return;}
        walking_controls_begin_frame=current_frame;walking_controls_enter(1);return;
    }
    if(!focused){walking_controls_fail("natural focus lost; external focus-loss behavior is not certified");return;}
    if(walking_controls_phase==1){
        if(third_person==walking_controls_want_third){
            // Request the ordinary equipment action, then observe its real
            // acknowledgement and blend completion. Never assign game state.
            if(walking_controls_want_stowed){
                if(walking_controls_boot_stowed){
                    // First preserve a rendered default-neutral checkpoint,
                    // then prove ordinary draw -> settled -> stow still works.
                    if(!walking_controls_default_neutral_checked){walking_controls_fail("default neutral was not checked before controls");return;}
                    if(walking_controls_default_capture&&walking_controls_captures==0)return;
                    if(!walking_controls_draw_requested){walking_controls_draw_requested=true;walking_controls_key(SDLK_z);return;}
                    if(!walking_controls_draw_settled){
                        if(weapon_sheathed||!skeletal_hero_loaded||!skeletal_hero.equipment_combat_ready()||skeletal_hero.stats().equipment_transition){
                            if(current_frame>walking_controls_begin_frame+240)walking_controls_fail("ordinary draw action did not settle");return;
                        }
                        if(!walking_controls_check(true,"ordinary draw from default neutral settles armed"))return;
                        walking_controls_draw_settled=true;
                    }
                }
                if(!walking_controls_stow_requested){walking_controls_stow_requested=true;walking_controls_key(SDLK_z);return;}
                if(!weapon_sheathed||!skeletal_hero_loaded||!skeletal_hero.equipment_stowed_settled()||!skeletal_hero.active().full_unarmed_settled()||skeletal_hero.stats().full_unarmed_transition||skeletal_hero.stats().full_unarmed_weight<.999f){
                    if(current_frame>walking_controls_begin_frame+240)walking_controls_fail("ordinary stow action did not settle");return;
                }
            }
            if(walking_controls_view_requested)++walking_controls_view_switches;
            if(!walking_controls_check(true,"actual requested camera view acknowledged"))return;
            walking_controls_enter(2);return;
        }
        if(!walking_controls_view_requested){walking_controls_view_requested=true;walking_controls_key(SDLK_v);}
        if(current_frame>walking_controls_begin_frame+120)walking_controls_fail("actual camera view acknowledgement timed out");
        return;
    }
    if(third_person!=walking_controls_want_third){walking_controls_fail("actual camera view changed unexpectedly");return;}
    if(walking_controls_phase==15||walking_controls_phase==18){
        const bool pause=walking_controls_phase==15;
        if(!walking_controls_action_sent){controller_test_pad.press(SDL_CONTROLLER_BUTTON_START);SDL_JoystickUpdate();++walking_controls_events;walking_controls_action_sent=true;}
        if(preview_screen==(pause?2:1)){
            if(!walking_controls_check(pause||walking_controls_stages[18].distance<.0001,pause?"actual START opens pause":"actual START resumes exploration without stale motion"))return;
            walking_controls_enter(walking_controls_phase+1);return;
        }
        if(walking_controls_age>2)walking_controls_fail("actual pause/resume acknowledgement timed out");
        return;
    }
    if(walking_controls_age<walking_controls_durations[walking_controls_phase])return;
    if(!walking_controls_stage_check())return;
    if(walking_controls_phase==21){
        if(walking_controls_captured_profile){
            const auto &m=skeletal_hero.stats();
            if(!walking_controls_check(m.captured_starts>=3&&m.captured_stops>=3&&m.captured_pivots>=1,
                "ordinary swept movement used captured starts stops and reversal"))return;
        }
        walking_controls_axes(0,0);
        if(!walking_controls_check(walking_controls_captures==6,"six bounded actual rendered checkpoints"))return;
        if(walking_controls_want_stowed&&!walking_controls_check(walking_controls_stow_requested&&weapon_sheathed&&walking_controls_relaxed_samples>10&&walking_controls_relaxed_peak>.5f&&walking_controls_full_idle_samples>5&&walking_controls_full_moving_samples>10,"ordinary stow used full-body human standing and movement"))return;
        if(walking_controls_want_stowed&&walking_controls_world_neutral_required&&!walking_controls_check(walking_controls_boot_stowed&&walking_controls_default_neutral_checked&&walking_controls_default_neutral_samples>=6&&walking_controls_draw_requested&&walking_controls_draw_settled,"plain World default neutral and ordinary draw-stow roundtrip verified"))return;
        walking_controls_passed=true;walking_controls_phase=100;launch->frames=unsigned(current_frame+4);return;
    }
    walking_controls_enter(walking_controls_phase+1);
};
auto walking_controls_sample=[&](unsigned long long current_frame,float delta,bool allowed){
    if(!qa_walking_controls||walking_controls_passed||walking_controls_failed)return;
    if(!std::isfinite(delta)||delta<0||delta>.101f){walking_controls_fail("invalid production dt");return;}
    if(walking_controls_phase==0){walking_controls_wait+=delta;if(walking_controls_wait>30)walking_controls_fail("natural focus/readiness wait exceeded thirty seconds");return;}
    if(walking_controls_phase==1)return;
    if(!std::isfinite(cam.x)||!std::isfinite(cam.y)||!std::isfinite(cam.z)||!std::isfinite(cam.yaw)||cam.fly||cam.swimming){walking_controls_fail("invalid ordinary player sample");return;}
    if(walking_controls_samples>=12000){walking_controls_fail("bounded frame sample limit exceeded");return;}
    const double dx=walking_controls_previous_valid?cam.x-walking_controls_previous_x:0,dz=walking_controls_previous_valid?cam.z-walking_controls_previous_z:0;
    const double speed=delta>0?std::hypot(dx,dz)/delta:0;
    if(!std::isfinite(speed)||speed>double(world_atlas_walk_speed())*1.3+.05){walking_controls_fail("unexpected ordinary displacement spike");return;}
    auto &s=walking_controls_stages[walking_controls_phase];++s.samples;s.allowed+=allowed;s.seconds+=delta;s.distance+=std::hypot(dx,dz);s.last_yaw=cam.yaw;if(walking_controls_previous_valid)s.yaw_distance+=std::abs(std::remainder(double(cam.yaw)-walking_controls_previous_yaw,6.28318530718));
    const double before=walking_controls_age;walking_controls_age+=delta;
    const double duration=walking_controls_durations[walking_controls_phase];
    if(duration>0&&walking_controls_age>=duration-.15){s.tail_speed+=speed*delta;s.tail_seconds+=delta;if(walking_controls_previous_valid)s.tail_yaw+=std::abs(std::remainder(double(cam.yaw)-walking_controls_previous_yaw,6.28318530718));}
    const auto motion=world_combat_motion_stats(combat_motion);const auto pose=world_combat_motion_pose(combat_motion,0);
    const bool focused=(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)&&preview_pad.focused;
    const int raw_x=preview_pad.get()?SDL_GameControllerGetAxis(preview_pad.get(),SDL_CONTROLLER_AXIS_LEFTX):0;
    const int raw_y=preview_pad.get()?SDL_GameControllerGetAxis(preview_pad.get(),SDL_CONTROLLER_AXIS_LEFTY):0;
    const int raw_look=preview_pad.get()?SDL_GameControllerGetAxis(preview_pad.get(),SDL_CONTROLLER_AXIS_RIGHTX):0;
    const float relaxed_walk=skeletal_hero_loaded?skeletal_hero.stats().relaxed_walk_weight:0;
    if(walking_controls_want_stowed){
        if(!weapon_sheathed){walking_controls_fail("stowed walking unexpectedly drew a weapon");return;}
        if(allowed&&speed>.05&&relaxed_walk>.02f)++walking_controls_relaxed_samples;
        walking_controls_relaxed_peak=std::max(walking_controls_relaxed_peak,relaxed_walk);
        const auto&full=skeletal_hero.stats();
        if(allowed&&full.full_unarmed&&full.full_unarmed_weight>.99f&&!full.full_unarmed_transition){
            if(speed<.025)++walking_controls_full_idle_samples;
            if(speed>.05)++walking_controls_full_moving_samples;
        }
    }
    walking_controls_csv<<current_frame<<','<<walking_controls_phase<<','<<walking_controls_names[walking_controls_phase]<<','<<(third_person?"third":"first")<<','<<delta<<','<<allowed<<','<<focused<<','<<raw_x<<','<<raw_y<<','<<raw_look<<','<<preview_pad.move.x<<','<<preview_pad.move.y<<','<<cam.x<<','<<cam.y<<','<<cam.z<<','<<cam.yaw<<','<<dx<<','<<dz<<','<<speed<<','<<motion.locomotion_phase<<','<<motion.locomotion_weight<<','<<motion.locomotion_speed<<','<<pose.slot<<','<<pose.blend<<','<<player_move.vx<<','<<player_move.vz<<','<<world_atlas_walk_speed()<<','<<manual_collision_steps<<','<<manual_slope_steps<<','<<manual_sliding_steps<<','<<weapon_sheathed<<','<<relaxed_walk<<','<<skeletal_hero.stats().captured_motion_mode<<','<<skeletal_hero.stats().captured_motion_clip<<','<<skeletal_hero.stats().captured_motion_time<<','<<skeletal_hero.stats().captured_motion_acceptance<<','<<skeletal_hero.stats().captured_starts<<','<<skeletal_hero.stats().captured_stops<<','<<skeletal_hero.stats().captured_pivots<<'\n';
    ++walking_controls_samples;walking_controls_previous_x=cam.x;walking_controls_previous_z=cam.z;walking_controls_previous_yaw=cam.yaw;walking_controls_previous_valid=true;
    if(!walking_controls_csv){walking_controls_fail("private frame samples failed");return;}
    if(duration>0&&before<duration&&walking_controls_age>=duration){
        const char *capture=nullptr;
        switch(walking_controls_phase){case 2:if(!walking_controls_default_capture)capture="walking-start";break;case 6:capture="walking-full";break;case 7:capture="walking-diagonal";break;case 9:capture="walking-stop";break;case 16:capture="walking-paused";break;case 19:capture="walking-resumed";break;default:break;}
        if(capture){if(!walking_controls_capture_pending.empty()){walking_controls_fail("checkpoint queue overlap");return;}walking_controls_capture_pending=capture;walking_controls_capture_phase=walking_controls_phase;}
    }
};
auto walking_controls_capture=[&](){
    if(!qa_walking_controls||walking_controls_failed||walking_controls_capture_pending.empty())return;
    const auto name=walking_controls_capture_pending;walking_controls_capture_pending.clear();const auto path=walking_controls_output/(name+".bmp");
    if(display_w!=1280||display_h!=720||std::filesystem::exists(path)){walking_controls_fail("bounded checkpoint dimensions/path mismatch");return;}
    const Uint32 began=SDL_GetTicks();
    for(;;){std::error_code error;unsigned pending=0;for(std::filesystem::directory_iterator it(walking_controls_output,error),end;!error&&it!=end;it.increment(error))if(it->path().extension()==".bmp")++pending;
        if(error||SDL_GetTicks()-began>=5000){walking_controls_fail("bounded screenshot consumer wait failed");return;}if(pending<2)break;SDL_Delay(1);}
    std::vector<unsigned char> pixels(std::size_t(display_w)*display_h*4);GLint alignment=4;glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);glBindFramebuffer(GL_FRAMEBUFFER,0);glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,display_w,display_h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());glPixelStorei(GL_PACK_ALIGNMENT,alignment);
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,display_w,display_h,32,SDL_PIXELFORMAT_RGBA32);bool ok=surface&&glGetError()==GL_NO_ERROR;
    if(ok){for(int y=0;y<display_h;++y)std::memcpy(static_cast<unsigned char*>(surface->pixels)+std::size_t(y)*surface->pitch,pixels.data()+std::size_t(display_h-1-y)*display_w*4,std::size_t(display_w)*4);ok=SDL_SaveBMP(surface,path.u8string().c_str())==0;}
    if(surface)SDL_FreeSurface(surface);
    if(!ok){walking_controls_fail("actual rendered checkpoint failed");return;}
    std::ofstream state(walking_controls_output/(name+".json"));state.imbue(std::locale::classic());state<<std::setprecision(17)<<"{\"name\":"<<world_json::quote(name)<<",\"frame\":"<<walking_controls_frame<<",\"phase\":"<<walking_controls_capture_phase<<",\"requested_view\":"<<world_json::quote(walking_controls_want_third?"third":"first")<<",\"actual_view\":"<<world_json::quote(third_person?"third":"first")<<",\"third_person\":"<<(third_person?"true":"false")<<",\"focused\":"<<(((SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)&&preview_pad.focused)?"true":"false")<<",\"position\":["<<cam.x<<','<<cam.y<<','<<cam.z<<"],\"yaw\":"<<cam.yaw<<",\"preview_screen\":"<<preview_screen<<"}\n";state.flush();if(!state){walking_controls_fail("actual checkpoint sidecar failed");return;}
    ++walking_controls_captures;walking_controls_capture_names.push_back(name);
};
auto walking_controls_finish=[&](){
    if(!qa_walking_controls)return;
    walking_controls_axes(0,0);if(!walking_controls_passed&&!walking_controls_failed)walking_controls_fail("route ended before completion");
    if(walking_controls_csv.is_open()){walking_controls_csv.flush();if(!walking_controls_csv)walking_controls_fail("final sample flush failed");walking_controls_csv.close();}
};
auto walking_controls_report=[&](FILE *f){
    std::fprintf(f,"  \"walking_stowed_review\": {\"requested\":%s,\"boot_stowed\":%s,\"default_neutral_checked\":%s,\"default_neutral_samples\":%u,\"ordinary_draw_requested\":%s,\"ordinary_draw_settled\":%s,\"ordinary_stow_requested\":%s,\"relaxed_samples\":%u,\"relaxed_peak\":%.9g,\"full_idle_samples\":%u,\"full_moving_samples\":%u},\n",walking_controls_want_stowed?"true":"false",walking_controls_boot_stowed?"true":"false",walking_controls_default_neutral_checked?"true":"false",walking_controls_default_neutral_samples,walking_controls_draw_requested?"true":"false",walking_controls_draw_settled?"true":"false",walking_controls_stow_requested?"true":"false",walking_controls_relaxed_samples,walking_controls_relaxed_peak,walking_controls_full_idle_samples,walking_controls_full_moving_samples);
    std::fprintf(f,"  \"walking_controls_qa\": {\"enabled\":%s,\"passed\":%s,\"failed\":%s,\"phase\":%u,\"checks\":%u,\"samples\":%llu,\"events\":%llu,\"captures\":%u,\"requested_view\":\"%s\",\"actual_view\":\"%s\",\"view_switches\":%u,\"forced_focus\":false,\"injected_player_pose\":false,\"actual_sdl_controller\":true,\"external_focus_loss_certified\":false,\"check_labels\":[",qa_walking_controls?"true":"false",walking_controls_passed?"true":"false",walking_controls_failed?"true":"false",walking_controls_phase,walking_controls_checks,walking_controls_samples,walking_controls_events,walking_controls_captures,walking_controls_want_third?"third":"first",third_person?"third":"first",walking_controls_view_switches);
    for(unsigned i=0;i<walking_controls_labels.size();++i)std::fprintf(f,"%s%s",i?",":"",world_json::quote(walking_controls_labels[i]).c_str());std::fprintf(f,"],\"capture_names\":[");for(unsigned i=0;i<walking_controls_capture_names.size();++i)std::fprintf(f,"%s%s",i?",":"",world_json::quote(walking_controls_capture_names[i]).c_str());std::fprintf(f,"],\"stages\":[");
    for(unsigned i=2;i<22;++i){if(i>2)std::fprintf(f,",");const auto &s=walking_controls_stages[i];std::fprintf(f,"{\"phase\":%u,\"name\":%s,\"samples\":%u,\"allowed_samples\":%u,\"seconds\":%.9g,\"distance\":%.12g,\"tail_speed\":%.12g,\"yaw_delta\":%.12g,\"yaw_distance\":%.12g}",i,world_json::quote(walking_controls_names[i]).c_str(),s.samples,s.allowed,s.seconds,s.distance,s.tail_seconds>0?s.tail_speed/s.tail_seconds:0,std::remainder(s.last_yaw-s.start_yaw,6.28318530718),s.yaw_distance);}std::fprintf(f,"]},\n");
};

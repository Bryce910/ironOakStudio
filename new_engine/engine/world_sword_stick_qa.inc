// Explicit, bounded controller regression. All actions use the real SDL virtual
// joystick or tagged ordinary E/R/Esc/V keys. This driver observes combat and
// camera state; it never assigns health, phases, player poses or story progress.
bool sword_stick_qa_passed=false;
bool sword_stick_qa_first_guards=false;
unsigned sword_stick_qa_checks=0,sword_stick_qa_events=0,sword_stick_qa_captures=0;
unsigned sword_stick_qa_phase=0,sword_stick_qa_direction=0;
uint64_t sword_stick_qa_action_frame=0,sword_stick_qa_started=0;
uint64_t sword_stick_qa_current_frame=0;
uint64_t sword_stick_qa_camera_release_frame=0;
double sword_stick_qa_clock=0;
std::vector<std::string> sword_stick_qa_labels,sword_stick_qa_capture_names;
std::string sword_stick_qa_capture_pending;
const char *sword_stick_qa_capture_dir=std::getenv("VALWOOD_QA_SWORD_STICK_CAPTURE_DIR");
auto sword_stick_workflow=[&](uint64_t qa_frame){
    if(!qa_stick_combat||sword_stick_qa_passed||!running)return;
    sword_stick_qa_current_frame=qa_frame;
    controller_test_pad.tick();
    if(sword_stick_qa_camera_release_frame&&qa_frame>=sword_stick_qa_camera_release_frame){
        controller_test_pad.button(SDL_CONTROLLER_BUTTON_RIGHTSTICK,false);
        ++sword_stick_qa_events;sword_stick_qa_camera_release_frame=0;
    }
    auto fail=[&](const char *reason){
        std::fprintf(stderr,"[Sword stick QA] FAIL phase %u direction %u after %u checks: %s (screen=%d state=%u guard=%d selected=%u attack=%u player_phase=%u swings=%u)\n",
            sword_stick_qa_phase,sword_stick_qa_direction,sword_stick_qa_checks,reason,
            preview_screen,unsigned(fight.state),int(fight.guard),unsigned(fight.selected),
            unsigned(fight.attack_direction),unsigned(fight.player_phase),fight.player_swings);
        launch->exit_code=1;running=false;
    };
    auto check=[&](bool good,const char *label){
        if(!good){fail(label);return false;}
        ++sword_stick_qa_checks;sword_stick_qa_labels.emplace_back(label);
        std::printf("[Sword stick QA] PASS %s\n",label);std::fflush(stdout);return true;
    };
    auto next=[&](unsigned phase){sword_stick_qa_phase=phase;sword_stick_qa_action_frame=qa_frame;};
    auto key=[&](SDL_Keycode code){
        for(bool down:{true,false}){
            SDL_Event input={};input.type=down?SDL_KEYDOWN:SDL_KEYUP;
            input.key.windowID=SDL_GetWindowID(window);input.key.state=down?SDL_PRESSED:SDL_RELEASED;
            input.key.keysym.sym=code;input.key.keysym.scancode=SDL_GetScancodeFromKey(code);
            input.key.keysym.unused=0x56414c54u;
            if(SDL_PushEvent(&input)!=1){fail("tagged SDL key rejected");return false;}
            ++sword_stick_qa_events;
        }
        return true;
    };
    auto axis=[&](SDL_GameControllerAxis code,Sint16 value){controller_test_pad.axis(code,value);++sword_stick_qa_events;};
    auto stick=[&](Sint16 x,Sint16 y){axis(SDL_CONTROLLER_AXIS_RIGHTX,x);axis(SDL_CONTROLLER_AXIS_RIGHTY,y);};
    auto trigger=[&](bool down){axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT,down?32767:-32768);};
    auto camera_click=[&](){
        // Exercise the normal controller Camera action without switching input
        // ownership to the keyboard while LT remains physically held. Keep the
        // down state through two complete SDL poll/render passes before up.
        controller_test_pad.button(SDL_CONTROLLER_BUTTON_RIGHTSTICK,true);
        ++sword_stick_qa_events;sword_stick_qa_camera_release_frame=qa_frame+2;
    };
    auto neutral=[&](){
        stick(0,0);trigger(false);axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);
        controller_test_pad.button(SDL_CONTROLLER_BUTTON_X,false);
        controller_test_pad.button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,false);
    };
    auto cardinal=[&](unsigned direction){
        stick(direction==0?-32768:direction==1?32767:0,direction==2?-32768:direction==3?32767:0);
    };
    auto attack_direction=[&](){
        return sword_stick_qa_direction==0?TrainingDirection::Left:sword_stick_qa_direction==1?TrainingDirection::Right:
               sword_stick_qa_direction==2?TrainingDirection::Low:TrainingDirection::High;
    };
    auto guard_direction=[&](){
        return sword_stick_qa_direction==0?TrainingDirection::Left:sword_stick_qa_direction==1?TrainingDirection::Right:
               sword_stick_qa_direction==2?TrainingDirection::High:TrainingDirection::Low;
    };
    static const char *names[]={"left","right","up","down"};
    auto capture=[&](const std::string &name){sword_stick_qa_capture_pending=name;};
    if(!sword_stick_qa_started)sword_stick_qa_started=qa_frame+1;
    if(qa_frame>sword_stick_qa_started+5800||qa_frame+40>=uint64_t(launch->frames)){
        fail("bounded virtual-pad workflow timed out");return;
    }
    if(destination_loading||qa_frame<=uint64_t(launch->warmup)||!terrain.view_ready(cam.x,cam.z,false))return;
    const uint64_t age=qa_frame>=sword_stick_qa_action_frame?qa_frame-sword_stick_qa_action_frame:0;
    switch(sword_stick_qa_phase){
    case 0:
        SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_PumpEvents();
        if(!check(playtest_enabled&&playtest_combat_ready&&combat_motion&&preview_screen==1&&
                  !cam.fly&&cam.grounded&&third_person&&!studio.open,"grounded third-person practice lane"))return;
        if(!check(preview_pad.id()==SDL_JoystickInstanceID(controller_test_pad.joystick),"exclusive real SDL virtual controller"))return;
        neutral();next(1);return;
    case 1:
        if(age<6)return;
        if(key(SDLK_e))next(2);return;
    case 2:
        if(age<2)return;
        if(!check(fight.state==TrainingState::Countdown&&fight.health==100&&fight.stamina==100,
                  "ordinary E starts fresh countdown"))return;
        cardinal(1);next(3);return;
    case 3:
        if(fight.state==TrainingState::Countdown){
            if(fight.player_phase!=TrainingPlayerPhase::Ready||fight.player_swings){fail("countdown accepted a sword attack");}return;
        }
        if(!check(fight.state==TrainingState::Fighting&&fight.player_swings==0&&fight.player_phase==TrainingPlayerPhase::Ready,
                  "countdown held stick cannot queue an attack"))return;
        next(4);return;
    case 4:
        if(age<20)return;
        if(!check(fight.player_swings==0&&fight.player_phase==TrainingPlayerPhase::Ready,
                  "countdown handoff requires neutral stick"))return;
        stick(0,0);sword_stick_qa_direction=0;next(5);return;
    case 5:
        if(age<6)return;
        if(fight.state!=TrainingState::Fighting||fight.player_phase!=TrainingPlayerPhase::Ready||fight.stamina<20)return;
        cardinal(sword_stick_qa_direction);next(6);return;
    case 6:
        if(age<2)return;
        if(!check(fight.state==TrainingState::Fighting&&fight.player_phase!=TrainingPlayerPhase::Ready&&
                  fight.attack_direction==attack_direction()&&!fight.guard,
                  (std::string("direct flick ")+names[sword_stick_qa_direction]+" starts matching attack without RT").c_str()))return;
        next(7);return;
    case 7:
        // Observe the animated stroke itself, rather than the idle frame before it.
        if(sword_stick_qa_capture_pending.empty()&&
           fight.player_phase==TrainingPlayerPhase::Strike&&fight.player_time>=.04&&
           std::find(sword_stick_qa_capture_names.begin(),sword_stick_qa_capture_names.end(),
                     std::string("third-attack-")+names[sword_stick_qa_direction])==sword_stick_qa_capture_names.end())
            capture(std::string("third-attack-")+names[sword_stick_qa_direction]);
        if(age<75||fight.player_phase!=TrainingPlayerPhase::Ready)return;
        if(!check(fight.player_swings==1,
                  (std::string("held ")+names[sword_stick_qa_direction]+" stick produces exactly one swing").c_str()))return;
        if(++sword_stick_qa_direction==4){neutral();if(key(SDLK_r))next(20);return;}
        neutral();if(key(SDLK_r))next(8);return;
    case 8:
        if(age<3)return;
        if(!check(fight.state==TrainingState::Idle&&fight.player_swings==0,
                  "ordinary R resets attack fixture"))return;
        if(key(SDLK_e))next(9);return;
    case 9:
        if(fight.state==TrainingState::Countdown)return;
        if(!check(fight.state==TrainingState::Fighting&&fight.player_swings==0&&fight.player_phase==TrainingPlayerPhase::Ready,
                  "fresh neutral countdown reaches ready combat"))return;
        next(5);return;
    case 20:
        if(age<3)return;
        if(!check(fight.state==TrainingState::Idle&&fight.player_swings==0,"ordinary R resets guard fixture"))return;
        if(key(SDLK_e))next(21);return;
    case 21:
        if(fight.state==TrainingState::Countdown)return;
        if(fight.state!=TrainingState::Fighting){fail("guard fixture did not enter combat");return;}
        trigger(true);next(22);return;
    case 22:
        if(age<24)return;
        {
            const auto pose=world_combat_motion_pose(combat_motion,0);
            if(!pose.active||pose.guard_direction!=unsigned(TrainingDirection::Thrust)||pose.guard_weight<.95f||pose.slot!=91){
                if(age>180)fail("center guard never reached its actual renderer pose");
                return;
            }
        }
        if(!check(fight.guard&&fight.selected==TrainingDirection::Thrust&&fight.player_swings==0&&
                  fight.player_phase==TrainingPlayerPhase::Ready&&third_person!=sword_stick_qa_first_guards&&preview_pad.using_pad,
                  sword_stick_qa_first_guards?"first-person LT center reaches rendered guard without attack":
                  "LT neutral selects center thrust guard without attack"))return;
        capture(sword_stick_qa_first_guards?"first-guard-center":"third-guard-center");next(220);return;
    case 220:
        // Keep the photographed input unchanged through the ordinary poll,
        // simulation and render before selecting the next guard.
        if(age<2||!sword_stick_qa_capture_pending.empty())return;
        sword_stick_qa_direction=0;cardinal(0);next(23);return;
    case 23:
        if(age<24)return;
        {
            const auto pose=world_combat_motion_pose(combat_motion,0);
            if(!pose.active||pose.guard_direction!=unsigned(guard_direction())||pose.guard_weight<.95f||
               pose.slot!=75+4*unsigned(guard_direction())){
                if(age>180)fail("cardinal guard never reached its actual renderer pose");
                return;
            }
        }
        if(!check(fight.guard&&fight.selected==guard_direction()&&fight.player_swings==0&&
                  fight.player_phase==TrainingPlayerPhase::Ready&&third_person!=sword_stick_qa_first_guards,
                  (sword_stick_qa_first_guards?
                   std::string("first-person LT ")+names[sword_stick_qa_direction]+" reaches rendered guard without attack":
                   std::string("LT ")+names[sword_stick_qa_direction]+" selects matching guard without attack").c_str()))return;
        capture(std::string(sword_stick_qa_first_guards?"first-guard-":"third-guard-")+names[sword_stick_qa_direction]);next(230);return;
    case 230:
        if(age<2||!sword_stick_qa_capture_pending.empty())return;
        if(++sword_stick_qa_direction<4){cardinal(sword_stick_qa_direction);next(23);return;}
        if(!sword_stick_qa_first_guards){
            sword_stick_qa_first_guards=true;stick(0,0);
            camera_click();next(22);return;
        }
        trigger(false);camera_click();next(24);return;
    case 24:
        if(age<18)return;
        if(!check(!fight.guard&&fight.player_phase==TrainingPlayerPhase::Ready&&fight.player_swings==0&&third_person&&preview_pad.using_pad,
                  "LT release with deflected stick never attacks"))return;
        cardinal(1);next(25);return;
    case 25:
        if(age<12)return;
        if(!check(fight.player_phase==TrainingPlayerPhase::Ready&&fight.player_swings==0,
                  "direction change after LT release still requires neutral"))return;
        neutral();next(26);return;
    case 26:
        if(age<6)return;
        cardinal(1);next(27);return;
    case 27:
        if(age<2)return;
        if(!check(fight.attack_direction==TrainingDirection::Right&&fight.player_phase!=TrainingPlayerPhase::Ready,
                  "fresh neutral return rearms attack after guard"))return;
        neutral();if(key(SDLK_r))next(30);return;
    case 30:
        if(age<3)return;
        if(fight.state!=TrainingState::Idle){fail("simultaneous fixture reset failed");return;}
        if(key(SDLK_e))next(31);return;
    case 31:
        if(fight.state==TrainingState::Countdown)return;
        if(fight.state!=TrainingState::Fighting){fail("simultaneous fixture did not enter combat");return;}
        next(32);return;
    case 32:
        if(age<6)return;
        // All four controls change before the same SDL poll/event pass.
        trigger(true);cardinal(1);axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,32767);
        controller_test_pad.button(SDL_CONTROLLER_BUTTON_X,true);
        controller_test_pad.button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,true);sword_stick_qa_events+=2;
        next(33);return;
    case 33:
        if(age<8)return;
        if(!check(fight.guard&&fight.selected==TrainingDirection::Right&&fight.player_swings==0&&
                  fight.player_phase==TrainingPlayerPhase::Ready,
                  "same-frame LT stick RT X RB gives guard priority"))return;
        axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);
        controller_test_pad.button(SDL_CONTROLLER_BUTTON_X,false);
        controller_test_pad.button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,false);sword_stick_qa_events+=2;
        next(34);return;
    case 34:
        if(age<10)return;
        if(!check(fight.guard&&fight.player_swings==0&&fight.player_phase==TrainingPlayerPhase::Ready,
                  "releasing alternate attack controls cannot replay guarded attack"))return;
        trigger(false);next(35);return;
    case 35:
        if(age<15)return;
        if(!check(!fight.guard&&fight.player_swings==0&&fight.player_phase==TrainingPlayerPhase::Ready,
                  "simultaneous guard release consumes held stick"))return;
        neutral();if(key(SDLK_r))next(40);return;
    case 40:
        if(age<3)return;
        if(fight.state!=TrainingState::Idle){fail("pause fixture reset failed");return;}
        if(key(SDLK_e))next(41);return;
    case 41:
        if(fight.state==TrainingState::Countdown)return;
        if(fight.state!=TrainingState::Fighting){fail("pause fixture did not enter combat");return;}
        next(42);return;
    case 42:
        if(age<6)return;
        sword_stick_qa_clock=fight.clock;
        cardinal(1);if(key(SDLK_ESCAPE))next(43);return;
    case 43:
        if(age<20)return;
        if(!check(preview_screen==2&&std::abs(fight.clock-sword_stick_qa_clock)<.02&&fight.player_swings==0,
                  "pause freezes combat and consumes simultaneous stick motion"))return;
        if(key(SDLK_ESCAPE))next(44);return;
    case 44:
        if(age<20)return;
        if(!check(preview_screen==1&&fight.player_swings==0&&fight.player_phase==TrainingPlayerPhase::Ready,
                  "resume with held stick requires neutral before attack"))return;
        stick(0,0);if(key(SDLK_v))next(45);return;
    case 45:
        if(age<6)return;
        if(!check(!third_person&&fight.player_phase==TrainingPlayerPhase::Ready,"ordinary V selects first-person combat"))return;
        cardinal(2);next(46);return;
    case 46:
        if(age<2)return;
        if(!check(fight.player_phase!=TrainingPlayerPhase::Ready&&fight.attack_direction==TrainingDirection::Low,
                  "first-person upward flick performs rising cut after neutral"))return;
        next(47);return;
    case 47:
        if(fight.player_phase==TrainingPlayerPhase::Strike&&fight.player_time>=.04&&
           sword_stick_qa_capture_pending.empty()&&
           std::find(sword_stick_qa_capture_names.begin(),sword_stick_qa_capture_names.end(),"first-attack-up")==sword_stick_qa_capture_names.end())
            capture("first-attack-up");
        if(age<75||fight.player_phase!=TrainingPlayerPhase::Ready)return;
        if(!check(fight.player_swings==1,"first-person held stick does not repeat"))return;
        neutral();if(key(SDLK_r))next(48);return;
    case 48:
        if(age<3)return;
        if(!check(fight.state==TrainingState::Idle&&third_person&&cam.grounded&&!cam.fly,
                  "ordinary R restores grounded third-person exploration"))return;
        if(!check(sword_stick_qa_captures==15,"all four attacks five guards and first-person stroke captured"))return;
        sword_stick_qa_passed=true;launch->frames=int(qa_frame+30);
        std::printf("[Sword stick QA] COMPLETE %u checks, %u SDL inputs, %u actual rendered captures; zero combat/player/story writes\n",
            sword_stick_qa_checks,sword_stick_qa_events,sword_stick_qa_captures);std::fflush(stdout);return;
    default:fail("invalid workflow phase");return;
    }
};
auto sword_stick_qa_capture=[&](){
    if(!qa_stick_combat||sword_stick_qa_capture_pending.empty())return;
    const auto name=sword_stick_qa_capture_pending;sword_stick_qa_capture_pending.clear();
    bool ok=sword_stick_qa_capture_dir&&*sword_stick_qa_capture_dir;
    std::vector<unsigned char> pixels(size_t(display_w)*display_h*4);
    glBindFramebuffer(GL_FRAMEBUFFER,0);glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadPixels(0,0,display_w,display_h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    ok=ok&&glGetError()==GL_NO_ERROR;
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,display_w,display_h,32,SDL_PIXELFORMAT_RGBA32);
    ok=ok&&surface;
    if(ok){
        for(int y=0;y<display_h;++y)
            std::memcpy(static_cast<unsigned char*>(surface->pixels)+size_t(y)*surface->pitch,
                        pixels.data()+size_t(display_h-1-y)*display_w*4,size_t(display_w)*4);
        const auto path=std::filesystem::path(sword_stick_qa_capture_dir)/(name+".bmp");
        ok=SDL_SaveBMP(surface,path.u8string().c_str())==0;
        if(ok){
            // This is recorded at the same render boundary as the image, after
            // SDL polling and simulation. The runner verifies the actual pose
            // and guard against the filename, rather than trusting its label.
            const auto state_path=std::filesystem::path(sword_stick_qa_capture_dir)/(name+".json");
            std::ofstream state(state_path);state.imbue(std::locale::classic());
            state<<std::setprecision(17)<<"{\"name\": "<<world_json::quote(name)
                 <<", \"selected\": "<<unsigned(fight.selected)
                 <<", \"attack_direction\": "<<unsigned(fight.attack_direction)
                 <<", \"guard\": "<<(fight.guard?"true":"false")
                 <<", \"player_phase\": "<<unsigned(fight.player_phase)
                 <<", \"player_time\": "<<fight.player_time
                 <<", \"third_person\": "<<(third_person?"true":"false")
                 <<", \"frame\": "<<sword_stick_qa_current_frame;
            const auto pose=world_combat_motion_pose(combat_motion,0);
            state<<", \"renderer_pose\": {\"active\": "<<(pose.active?"true":"false")
                 <<", \"slot\": "<<pose.slot<<", \"guard_direction\": "<<pose.guard_direction
                 <<", \"guard_weight\": "<<pose.guard_weight<<", \"blend\": "<<pose.blend<<"}";
            auto blade=[&](const char *label,bool viewmodel){
                WorldCombatBlade value;const bool available=world_combat_motion_blade(combat_motion,0,value,viewmodel);
                state<<", "<<world_json::quote(label)<<": {\"available\": "<<(available?"true":"false")
                     <<", \"base\": ["<<value.base[0]<<", "<<value.base[1]<<", "<<value.base[2]
                     <<"], \"tip\": ["<<value.tip[0]<<", "<<value.tip[1]<<", "<<value.tip[2]<<"]}";
            };
            blade("world_blade",false);blade("viewmodel_blade",true);state<<"}\n";
            state.flush();ok=bool(state);
        }
    }
    if(surface)SDL_FreeSurface(surface);
    if(!ok){std::fprintf(stderr,"[Sword stick QA] rendered capture failed: %s\n",name.c_str());launch->exit_code=1;running=false;return;}
    ++sword_stick_qa_captures;sword_stick_qa_capture_names.push_back(name);
};
auto sword_stick_qa_report=[&](FILE *file){
    std::fprintf(file,"  \"sword_stick\": {\"qa\": %s, \"passed\": %s, \"checks\": %u, \"events\": %u, \"captures\": %u, \"phase\": %u, \"combat_state_mutations\": 0, \"player_pose_writes\": 0, \"story_state_writes\": 0, \"check_labels\": [",
        qa_stick_combat?"true":"false",sword_stick_qa_passed?"true":"false",sword_stick_qa_checks,sword_stick_qa_events,sword_stick_qa_captures,sword_stick_qa_phase);
    for(size_t i=0;i<sword_stick_qa_labels.size();++i)std::fprintf(file,"%s%s",i?", ":"",world_json::quote(sword_stick_qa_labels[i]).c_str());
    std::fprintf(file,"], \"capture_names\": [");
    for(size_t i=0;i<sword_stick_qa_capture_names.size();++i)std::fprintf(file,"%s%s",i?", ":"",world_json::quote(sword_stick_qa_capture_names[i]).c_str());
    std::fprintf(file,"]},\n");
};

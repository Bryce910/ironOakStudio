// Bounded end-to-end arena rehearsal. The driver only observes game state and
// supplies ordinary tagged SDL keys plus a real virtual gamepad. It never sets
// health, actor/player poses, AI phases, damage, progress or victory fields.
bool arena_qa_passed=false;
unsigned arena_qa_phase=0,arena_qa_checks=0,arena_qa_events=0,arena_qa_captures=0;
unsigned arena_qa_total_hits=0,arena_qa_total_parries=0,arena_qa_old_hits=0,arena_qa_old_parries=0;
unsigned arena_qa_loss_hits=0,arena_qa_shot_attempts=0;
unsigned arena_qa_guarded_before=0,arena_qa_metal_before=0;
float arena_qa_guarded_health=0;
bool arena_qa_body_checked=false,arena_qa_parry_checked=false;
unsigned arena_qa_focus_loss_frames=0,arena_qa_focus_requests=0,arena_qa_keyboard_capture_frames=0;
uint64_t arena_qa_frame=0,arena_qa_action=0,arena_qa_camera_release=0,arena_qa_attack_after=0;
uint64_t arena_qa_next_focus_request=0,arena_qa_next_policy_trace=0;
double arena_qa_pause_clock=0;
double arena_qa_blood_pause_elapsed=0;
unsigned arena_qa_blood_pause_meshes=0,arena_qa_blood_pause_stains=0;
std::uint64_t arena_qa_blood_floor_serial=0;
bool arena_qa_blood_floor_checked=false;
world_blood_effects::Stain arena_qa_blood_witness;
double arena_qa_blood_witness_elapsed=0,arena_qa_blood_persistent_age=0;
bool arena_qa_blood_persistent_checked=false,arena_qa_blood_witness_anchored=false;
std::uint64_t arena_qa_gore_body_frame=0,arena_qa_finisher_frame=0;
bool arena_qa_gore_checked=false,arena_qa_finisher_checked=false;
double arena_qa_finisher_clock=0,arena_qa_blood_settle_elapsed=0;
unsigned arena_qa_blood_settle_stains=0;
bool arena_qa_blood_settled_checked=false;
bool arena_qa_character_fall_checked=false,arena_qa_character_grounded_checked=false,arena_qa_first_defeat_saved=false;
world_arena_combat::Vec3 arena_qa_first_defeat_position;
float arena_qa_first_defeat_yaw=0;
double arena_qa_character_grounded_age=0;
std::array<bool,8> arena_qa_fought={};
std::uint64_t arena_qa_initial_purse=0,arena_qa_initial_wins=0,arena_qa_first_run=0,arena_qa_one_run=0;
std::uint64_t arena_qa_combo_serial=0,arena_qa_combo_links=0;
float arena_qa_combo_stamina=0;
unsigned arena_qa_combo_poses=0,arena_qa_concurrent_attackers=0,arena_qa_menu_release=0;
unsigned arena_qa_locked_target=8;std::uint64_t arena_qa_lock_attack_serial=0;
SDL_GameControllerButton arena_qa_menu_button=SDL_CONTROLLER_BUTTON_INVALID;
bool arena_qa_combo_first=false,arena_qa_combo_second=false,arena_qa_combo_third=false,arena_qa_wallet_verified=false;
std::vector<std::string> arena_qa_labels,arena_qa_capture_names;
std::string arena_qa_capture_pending;
const char *arena_qa_capture_dir=std::getenv("VALWOOD_QA_ARENA_CAPTURE_DIR");
auto arena_workflow=[&](uint64_t qa_frame){
    if(!arena_qa||arena_qa_passed||!running)return;
    arena_qa_frame=qa_frame;controller_test_pad.tick();
    const bool window_focused=(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)!=0;
    if(!window_focused||!preview_pad.focused){
        ++arena_qa_focus_loss_frames;
        if(qa_frame>=arena_qa_next_focus_request){
            // Request actual SDL window focus, as at startup. The ordinary SDL
            // focus-gained event owns Device::focused; QA never assigns it.
            std::printf("[Arena QA] FOCUS request frame%llu window%d device%d phase%u\n",(unsigned long long)qa_frame,int(window_focused),int(preview_pad.focused),arena_qa_phase);
            SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_PumpEvents();
            ++arena_qa_focus_requests;arena_qa_next_focus_request=qa_frame+30;
        }
    }
    if(ImGui::GetIO().WantCaptureKeyboard)++arena_qa_keyboard_capture_frames;
    if(arena_qa_camera_release&&qa_frame>=arena_qa_camera_release){controller_test_pad.button(SDL_CONTROLLER_BUTTON_RIGHTSTICK,false);++arena_qa_events;arena_qa_camera_release=0;}
    if(arena_qa_menu_release&&qa_frame>=arena_qa_menu_release){controller_test_pad.button(arena_qa_menu_button,false);++arena_qa_events;arena_qa_menu_release=0;arena_qa_menu_button=SDL_CONTROLLER_BUTTON_INVALID;}
    using S=world_arena_combat::State;using W=world_arena_combat::Weapon;using C=world_arena_combat::ChallengerState;
    auto fail=[&](const char *why){std::fprintf(stderr,"[Arena QA] FAIL phase %u: %s state%u wave%zu defeated%u HP%.1f enemyHP%.1f shots%u arrows%u screen%d focus%d using_pad%d keyboard%d text_input%d active_widget%d draft%d/%d menu_button%d release%u selected%u player_phase%u LT%.3f RS%.3f,%.3f\n",arena_qa_phase,why,unsigned(arena_game.state()),arena_game.current_index(),arena_game.defeated(),fight.health,fight.enemy_health,arena_game.shots(),arena_game.arrow_hits(),preview_screen,int(preview_pad.focused),int(preview_pad.using_pad),int(ImGui::GetIO().WantCaptureKeyboard),int(ImGui::GetIO().WantTextInput),int(ImGui::IsAnyItemActive()),arena_total_draft,arena_simultaneous_draft,int(arena_qa_menu_button),arena_qa_menu_release,unsigned(fight.selected),unsigned(fight.player_phase),preview_pad.left_trigger,preview_pad.look.x,preview_pad.look.y);launch->exit_code=1;running=false;};
    auto check=[&](bool good,const char *label){if(!good){fail(label);return false;}++arena_qa_checks;arena_qa_labels.emplace_back(label);std::printf("[Arena QA] PASS %s\n",label);std::fflush(stdout);return true;};
    auto next=[&](unsigned phase){arena_qa_phase=phase;arena_qa_action=qa_frame;};
    auto key=[&](SDL_Keycode code){for(bool down:{true,false}){SDL_Event e={};e.type=down?SDL_KEYDOWN:SDL_KEYUP;e.key.windowID=SDL_GetWindowID(window);e.key.state=down?SDL_PRESSED:SDL_RELEASED;e.key.keysym.sym=code;e.key.keysym.scancode=SDL_GetScancodeFromKey(code);e.key.keysym.unused=0x56414c54u;if(SDL_PushEvent(&e)!=1){fail("tagged key rejected");return false;}++arena_qa_events;}return true;};
    auto menu_button=[&](SDL_GameControllerButton button){
        if(arena_qa_menu_release)return false;
        // A short real controller A chooses SliderScalar's PreferTweak path.
        // Enter deliberately chooses its numeric text-input path, while A
        // held for .60s also enters text. Three frames are a bounded tap.
        controller_test_pad.button(button,true);++arena_qa_events;
        arena_qa_menu_button=button;arena_qa_menu_release=unsigned(qa_frame+3);return true;
    };
    auto axis=[&](SDL_GameControllerAxis a,Sint16 v){controller_test_pad.axis(a,v);++arena_qa_events;};
    auto stick=[&](Sint16 x,Sint16 y){axis(SDL_CONTROLLER_AXIS_RIGHTX,x);axis(SDL_CONTROLLER_AXIS_RIGHTY,y);};
    auto lt=[&](bool down){axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT,down?32767:-32768);};
    auto rt=[&](bool down){axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,down?32767:-32768);};
    auto camera=[&](){controller_test_pad.button(SDL_CONTROLLER_BUTTON_RIGHTSTICK,true);++arena_qa_events;arena_qa_camera_release=qa_frame+2;};
    auto neutral=[&](){stick(0,0);lt(false);rt(false);};
    auto guard_stick=[&](TrainingDirection direction){stick(direction==TrainingDirection::Left?-32768:direction==TrainingDirection::Right?32767:0,direction==TrainingDirection::High?-32768:direction==TrainingDirection::Low?32767:0);};
    auto attack_stick=[&](TrainingDirection direction){stick(direction==TrainingDirection::Left?-32768:direction==TrainingDirection::Right?32767:0,direction==TrainingDirection::Low?-32768:direction==TrainingDirection::High?32767:0);};
    auto capture=[&](const char *name){if(std::find(arena_qa_capture_names.begin(),arena_qa_capture_names.end(),name)==arena_qa_capture_names.end())arena_qa_capture_pending=name;};
    auto queued=[&](){unsigned count=0;for(const auto &actor:arena_game.challengers())count+=actor.state!=C::Defeated&&actor.state!=C::Unused;return count;};
    auto saved_purse=[&](std::uint64_t expected){world_arena_progress::Progress saved;std::string error;return world_arena_progress::load("saves/arena.progress",saved,error)==world_arena_progress::Load::Loaded&&saved.balance==expected&&saved.balance==arena_progress.balance&&saved.wins==arena_progress.wins&&saved.last_settled_run_id==arena_progress.last_settled_run_id;};
    auto sword_policy=[&](){
        if(!arena_controls_ready()){neutral();return;}
        if(arena_game.state()!=S::Fighting||fight.state!=TrainingState::Fighting){neutral();return;}
        if(fight.enemy_phase==TrainingEnemyPhase::Windup){
            if(fight.enemy_duration-fight.enemy_time<.17&&fight.player_phase==TrainingPlayerPhase::Ready&&fight.stamina>=10){guard_stick(fight.enemy_direction);lt(true);}
            else{stick(0,0);lt(false);}return;
        }
        if(preview_pad.guard||fight.guard){lt(false);stick(0,0);arena_qa_attack_after=qa_frame+5;return;}
        if(fight.player_phase!=TrainingPlayerPhase::Ready){stick(0,0);return;}
        const auto enemy=arena_game.active_position();const double distance=std::hypot(cam.x-arena_origin_x-enemy.x,cam.z-arena_origin_z-enemy.z);
        if(qa_frame>=arena_qa_attack_after&&fight.stamina>=35&&distance<=2.34&&fight.enemy_phase!=TrainingEnemyPhase::Strike){attack_stick(TrainingDirection((unsigned(fight.enemy_guard)+1)%4));arena_qa_attack_after=qa_frame+36;}
        else stick(0,0);
    };
    auto aim=[&](){
        const auto enemy=arena_game.active_position();
        const double target_x=arena_origin_x+enemy.x,target_z=arena_origin_z+enemy.z;
        // Aim through the actual displayed reticle. The third-person camera
        // sits behind/above the hero, so its ray differs from the logical eye.
        // Ballistic drop still uses muzzle-to-enemy travel, not camera range.
        const double muzzle_x=cam.x+std::sin(cam.yaw)*std::cos(cam.pitch)*.65;
        const double muzzle_z=cam.z-std::cos(cam.yaw)*std::cos(cam.pitch)*.65;
        const double travel=std::hypot(target_x-muzzle_x,target_z-muzzle_z);
        const double drop=world_arena_combat::Arena::gravity*travel*travel/(2*28.*28.);
        const double dx=target_x-arena_aim_camera.x,dz=target_z-arena_aim_camera.z,range=std::hypot(dx,dz);
        const double yaw=std::remainder(std::atan2(dx,-dz)-double(arena_aim_camera.yaw),6.283185307179586);
        const double pitch=std::atan2(arena_floor+enemy.y+1.05+drop-arena_aim_camera.y,range)-double(arena_aim_camera.pitch);
        auto raw=[&](double error){if(std::abs(error)<.018)return Sint16(0);const double v=preview_settings.deadzone+(1-preview_settings.deadzone)*std::clamp(std::abs(error)*3.,.13,.75);return Sint16(std::copysign(v*32767,error));};
        stick(raw(yaw),raw(-pitch*(preview_settings.invert_y?-1.:1.)));return std::abs(yaw)<.025&&std::abs(pitch)<.025;
    };
    if(qa_frame+40>=uint64_t(launch->frames)){fail("bounded arena input rehearsal timed out");return;}
    if(destination_loading||qa_frame<=uint64_t(launch->warmup)||!terrain.view_ready(cam.x,cam.z,false))return;
    if(!window_focused||!preview_pad.focused)return;
    const uint64_t age=qa_frame>=arena_qa_action?qa_frame-arena_qa_action:0;
    if(((arena_qa_phase>=20&&arena_qa_phase<=24)||(arena_qa_phase>=33&&arena_qa_phase<=41))&&age>240){
        fail("controller menu navigation timed out (text_input and active_widget identify accidental numeric editing)");return;
    }
    switch(arena_qa_phase){
    case 0:
        SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_PumpEvents();
        if(!check(arena_ui_phase==ArenaUiPhase::Setup&&preview_screen==2&&arena_game.state()==S::Idle&&arena_total_draft==8&&arena_simultaneous_draft==1&&arena_progress_ready,"setup opens before combat with eight total and one simultaneous"))return;
        arena_qa_initial_purse=arena_progress.balance;arena_qa_initial_wins=arena_progress.wins;
        neutral();capture("arena-setup");next(200);return;
    case 200:
        if(age<6)return;if(key(SDLK_RETURN))next(201);return;
    case 201:
        if(age<6||!arena_controls_ready())return;
        if(!check(arena_ui_phase==ArenaUiPhase::Fight&&arena_run_id>0&&arena_progress.balance==arena_qa_initial_purse,"focused setup Start allocates a fresh run without awarding coins"))return;
        arena_qa_first_run=arena_run_id;
        if(!check(arena_enabled&&preview_screen==1&&third_person&&cam.grounded&&!cam.fly&&queued()==8,"eight visible challengers and grounded third-person start"))return;
        if(!check(preview_pad.id()==SDL_JoystickInstanceID(controller_test_pad.joystick),"exclusive real SDL virtual gamepad"))return;
        if(!check(blood_enabled&&world_blood_stats(blood_renderer).provenance_loaded&&world_blood_stats(blood_renderer).loaded_variants>=1,"purchased blood source bank is loaded"))return;
        {const auto motion=world_combat_motion_stats(combat_motion);
        if(!check(character_polish_enabled&&arena_game.keep_defeated&&motion.polished&&motion.pose_assets==344&&motion.reaction_assets==40&&motion.death_assets==24,"polished fighter loads directional reactions and real collapse"))return;}
        neutral();capture("arena-eight");if(key(SDLK_2))next(1);return;
    case 1:
        if(age<4)return;if(!check(arena_game.weapon()==W::Bow,"keyboard 2 equips bow"))return;
        if(key(SDLK_1))next(2);return;
    case 2:
        if(age<4)return;if(!check(arena_game.weapon()==W::Sword,"keyboard 1 equips sword"))return;
        controller_test_pad.press(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);++arena_qa_events;next(3);return;
    case 3:
        if(age<5)return;if(!check(arena_game.weapon()==W::Bow,"controller D-pad right equips bow"))return;
        controller_test_pad.press(SDL_CONTROLLER_BUTTON_DPAD_LEFT);++arena_qa_events;next(4);return;
    case 4:
        if(age<5)return;if(!check(arena_game.weapon()==W::Sword,"controller D-pad left equips sword"))return;
        arena_qa_pause_clock=fight.clock;if(key(SDLK_ESCAPE))next(5);return;
    case 5:
        if(age<20)return;if(!check(preview_screen==2&&std::abs(fight.clock-arena_qa_pause_clock)<.02,"pause freezes real arena combat"))return;
        if(key(SDLK_ESCAPE))next(6);return;
    case 6:
        if(age<8)return;if(!check(preview_screen==1,"ordinary Escape resumes arena"))return;
        next(7);return;
    case 7:
        if(arena_game.state()!=S::Lost)return;
        if(!check(fight.health==0&&fight.enemy_hits>=5&&arena_game.defeated()==0,"unguarded enemy attacks cause real defeat"))return;
        arena_qa_loss_hits=fight.enemy_hits;capture("arena-defeat");next(70);return;
    case 70:
        if(age<4)return;
        if(!check(arena_ui_phase==ArenaUiPhase::Results&&arena_progress.balance==arena_qa_initial_purse&&saved_purse(arena_qa_initial_purse),"real defeat leaves the persistent purse unchanged"))return;
        if(key(SDLK_RETURN))next(8);return;
    case 8:
        if(age<5||!arena_controls_ready())return;if(!check(arena_game.state()==S::Entering&&queued()==8&&arena_game.defeated()==0&&fight.health==100&&fight.enemy_hits==0&&arena_run_id>arena_qa_first_run,"focused result button retries all eight with a new run receipt"))return;
        capture("arena-retry");if(key(SDLK_2))next(9);return;
    case 9:
        if(arena_game.state()!=S::Fighting||fight.state!=TrainingState::Fighting)return;
        if(!aim())return;camera();next(10);return;
    case 10:
        if(age<6)return;if(!check(!third_person&&arena_game.weapon()==W::Bow,"controller camera enters first-person bow"))return;
        rt(true);next(11);return;
    case 11:
        aim();if(arena_game.draw_fraction()<.7)return;
        if(!check(arena_game.drawing(),"fresh RT physically draws bow"))return;
        lt(true);next(115);return;
    case 115:
        aim();if(age<20)return;
        if(!check(arena_game.drawing()&&cam.fov<.95f&&!fight.guard,"LT steadies bow aim without sword guard"))return;
        lt(false);
        capture("bow-first-draw");next(110);return;
    case 110:
        if(age<3)return;arena_qa_pause_clock=fight.clock;if(key(SDLK_ESCAPE))next(111);return;
    case 111:
        if(age<20)return;if(!check(preview_screen==2&&!arena_game.drawing()&&arena_game.shots()==0&&std::abs(fight.clock-arena_qa_pause_clock)<.02,"bow pause cancels draw and freezes fight"))return;
        if(key(SDLK_ESCAPE))next(112);return;
    case 112:
        if(age<15)return;if(!check(preview_screen==1&&!arena_game.drawing()&&arena_game.shots()==0,"held bow trigger cannot fire or redraw after resume"))return;
        rt(false);next(113);return;
    case 113:
        if(age<6)return;rt(true);next(114);return;
    case 114:
        aim();if(arena_game.draw_fraction()<.8)return;
        camera();next(12);return;
    case 12:
        if(age<6||!aim())return;if(!check(third_person&&arena_game.drawing(),"controller camera preserves third-person drawn bow"))return;
        capture("bow-third-draw");next(13);return;
    case 13:
        if(age<3)return;stick(0,0);rt(false);++arena_qa_shot_attempts;next(14);return;
    case 14:{
        bool flying=false;for(const auto &arrow:arena_game.arrows())flying|=arrow.state==world_arena_combat::ArrowState::Flying;
        if(flying)capture("arrow-flight");
        if(arena_game.arrow_hits()>0){
            if(!check(arena_game.shots()>0&&fight.enemy_health<90,"charged physical arrow hits the active knight"))return;
            const auto effect=combat_vfx.stats();bool anchored=false;
            for(const auto &arrow:arena_game.arrows())if(arrow.state==world_arena_combat::ArrowState::HitEnemy){const auto p=effect.last_body_contacts[1];anchored|=std::hypot(p.x-arena_origin_x-arrow.position.x,p.z-arena_origin_z-arrow.position.z)<.0001&&std::abs(p.y-arena_floor-arrow.position.y)<.0001;}
            if(!check(effect.body_bursts>0&&effect.last_material==world_combat_vfx::Material::Body&&anchored,"physical arrow produces blood at its resolved body contact"))return;
            capture("arrow-hit");neutral();next(143);return;
        }
        if(age>70){if(arena_qa_shot_attempts>=4){fail("four aimed physical arrows missed the knight");return;}rt(true);next(140);}return;
    }
    case 140:
        aim();if(arena_game.draw_fraction()<.85)return;stick(0,0);rt(false);++arena_qa_shot_attempts;next(14);return;
    case 143:{
        neutral();if(age<6)return;
        const auto source=world_blood_stats(blood_renderer);const auto stage=blood_effects.stage();
        if(!check(stage.mesh_count>0&&source.active_meshes>0&&source.mesh_draws>0&&source.last_frame>0&&source.frame_transitions>0,"purchased volumetric mesh animates at body contact"))return;
        for(unsigned i=0;i<stage.stain_count;++i)if(stage.stains[i].defender==1){
            arena_qa_blood_floor_serial=stage.stains[i].serial;arena_qa_blood_witness=stage.stains[i];
            arena_qa_blood_witness_elapsed=blood_effects.stats().elapsed;
        }
        capture("blood-fluid");next(147);return;
    }
    case 147:
        if(age<3)return;if(key(SDLK_ESCAPE))next(144);return;
    case 144:
        if(age<8)return;
        if(preview_screen!=2){fail("blood playback pause did not open");return;}
        arena_qa_blood_pause_elapsed=blood_effects.stats().elapsed;
        arena_qa_blood_pause_meshes=blood_effects.stage().mesh_count;arena_qa_blood_pause_stains=blood_effects.stage().stain_count;
        next(145);return;
    case 145:
        if(age<20)return;
        if(!check(preview_screen==2&&blood_effects.stats().elapsed==arena_qa_blood_pause_elapsed&&blood_effects.stage().mesh_count==arena_qa_blood_pause_meshes&&blood_effects.stage().stain_count==arena_qa_blood_pause_stains&&arena_qa_blood_pause_meshes>0,"pause freezes purchased fluid and floor stains"))return;
        capture("blood-paused");next(148);return;
    case 148:
        if(age<3)return;if(key(SDLK_ESCAPE))next(146);return;
    case 146:
        if(age<6)return;if(preview_screen!=1){fail("blood playback did not resume");return;}
        neutral();if(key(SDLK_1))next(141);return;
    case 141:{
        // Deliberately strike the knight's displayed guard once. This is an
        // ordinary stick swing, proving weapon contact without body damage.
        if(arena_game.state()==S::Lost){fail("lost before deliberate weapon contact");return;}
        if(arena_game.weapon()!=W::Sword||fight.player_phase!=TrainingPlayerPhase::Ready)return;
        const auto enemy=arena_game.active_position();const double distance=std::hypot(cam.x-arena_origin_x-enemy.x,cam.z-arena_origin_z-enemy.z);
        if(fight.enemy_phase==TrainingEnemyPhase::Measure&&fight.enemy_duration-fight.enemy_time>.29&&distance<=2.34&&fight.stamina>=20){
            arena_qa_guarded_before=fight.guarded;arena_qa_guarded_health=fight.enemy_health;arena_qa_metal_before=combat_vfx.stats().metal_bursts;
            if(fight.enemy_guard==TrainingDirection::Thrust){controller_test_pad.press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);++arena_qa_events;}
            else attack_stick(fight.enemy_guard);
            next(142);
        }else neutral();return;
    }
    case 142:
        stick(0,0);
        if(fight.guarded>arena_qa_guarded_before){
            const auto effect=combat_vfx.stats();
            if(!check(fight.enemy_health==arena_qa_guarded_health&&effect.metal_bursts>arena_qa_metal_before&&effect.last_material==world_combat_vfx::Material::Metal&&combat_contact_kind==3,"guarded sword contact produces iron sparks without body damage"))return;
            capture("metal-block");next(15);return;
        }
        if(age>60){fail("deliberate guarded sword contact was not resolved");return;}return;
    case 15:{
        if(arena_game.weapon()!=W::Sword){if(age>8)fail("keyboard sword switch did not complete");return;}
        if(qa_frame>=arena_qa_next_policy_trace){
            const auto enemy=arena_game.active_position();const double distance=std::hypot(cam.x-arena_origin_x-enemy.x,cam.z-arena_origin_z-enemy.z);
            std::printf("[Arena QA] POLICY frame%llu wave%zu defeated%u HP%.1f enemyHP%.1f stamina%.1f enemy_phase%u remaining%.3f player_phase%u guard%d selected%u focus%d window%d using_pad%d keyboard%d LT%.3f RS%.3f,%.3f distance%.3f\n",(unsigned long long)qa_frame,arena_game.current_index(),arena_game.defeated(),fight.health,fight.enemy_health,fight.stamina,unsigned(fight.enemy_phase),fight.enemy_duration-fight.enemy_time,unsigned(fight.player_phase),int(fight.guard),unsigned(fight.selected),int(preview_pad.focused),int(window_focused),int(preview_pad.using_pad),int(ImGui::GetIO().WantCaptureKeyboard),preview_pad.left_trigger,preview_pad.look.x,preview_pad.look.y,distance);
            std::fflush(stdout);arena_qa_next_policy_trace=qa_frame+180;
        }
        if(!arena_qa_body_checked&&fight.hits>1&&fight.player_phase==TrainingPlayerPhase::Strike&&combat_contact_kind==1){
            const auto effect=combat_vfx.stats();
            if(!check(effect.body_bursts>0&&effect.last_material==world_combat_vfx::Material::Body&&combat_contact_damage>0,"sword body contact produces blood and a damage cue"))return;
            capture("body-hit");arena_qa_body_checked=true;arena_qa_gore_body_frame=qa_frame;
        }
        if(!arena_qa_parry_checked&&fight.parries>0&&combat_contact_kind==4){
            const auto effect=combat_vfx.stats();
            if(!check(effect.metal_bursts>0&&effect.last_material==world_combat_vfx::Material::Metal,"timed parry produces iron sparks and a steel cue"))return;
            capture("metal-parry");arena_qa_parry_checked=true;
        }
        if(!arena_qa_gore_checked&&arena_qa_gore_body_frame&&qa_frame>=arena_qa_gore_body_frame+7){
            const auto source=world_blood_stats(blood_renderer);const auto stage=blood_effects.stage();
            if(!check(stage.mesh_count>=2&&source.secondary_draws>0&&source.active_meshes>=2&&combat_vfx.stats().body_bursts>0,"directional body damage produces crossed heavy fluid sprays"))return;
            capture("gore-impact");arena_qa_gore_checked=true;
        }
        if(!arena_qa_finisher_checked){
            const auto stage=blood_effects.stage();bool final_hit=false;
            for(unsigned i=0;i<stage.mesh_count;++i)final_hit|=stage.meshes[i].finisher&&stage.meshes[i].defender==1;
            if(final_hit&&fight.enemy_health<=0&&!arena_qa_finisher_frame){arena_qa_finisher_frame=qa_frame;arena_qa_finisher_clock=blood_effects.stats().elapsed;}
            if(arena_qa_finisher_frame&&qa_frame>=arena_qa_finisher_frame+8){
                if(!check(final_hit&&world_blood_stats(blood_renderer).finisher_draws>0&&blood_effects.stats().max_strength>=1.5f&&combat_vfx.stats().finisher_bursts>0,"finishing body hit produces a heavier blood burst"))return;
                capture("finisher-blood");arena_qa_finisher_checked=true;
            }
        }
        if(fight.hits<arena_qa_old_hits)arena_qa_old_hits=0;
        if(fight.parries<arena_qa_old_parries)arena_qa_old_parries=0;
        arena_qa_total_hits+=fight.hits-arena_qa_old_hits;arena_qa_total_parries+=fight.parries-arena_qa_old_parries;
        arena_qa_old_hits=fight.hits;arena_qa_old_parries=fight.parries;
        if(!arena_qa_blood_floor_checked&&arena_game.current_index()>=1&&fight.state==TrainingState::Fighting){
            const auto stage=blood_effects.stage();bool retained=false;
            for(unsigned i=0;i<stage.stain_count;++i)retained|=stage.stains[i].serial==arena_qa_blood_floor_serial;
            if(!check(retained&&world_blood_stats(blood_renderer).stain_draws>0,"blood floor stains persist into the next challenger"))return;
            capture("blood-stain");arena_qa_blood_floor_checked=true;
        }
        if(arena_game.defeated()>0){
            const auto &first=arena_game.challengers()[0];
            if(!arena_qa_first_defeat_saved){arena_qa_first_defeat_position=first.position;arena_qa_first_defeat_yaw=first.defeat_yaw;arena_qa_first_defeat_saved=true;}
            const bool anchored=first.position.x==arena_qa_first_defeat_position.x&&first.position.y==arena_qa_first_defeat_position.y&&first.position.z==arena_qa_first_defeat_position.z&&first.defeat_yaw==arena_qa_first_defeat_yaw;
            if(!arena_qa_character_fall_checked&&first.defeat_age>=.75){
                const auto motion=world_combat_motion_stats(combat_motion);
                if(!check(anchored&&first.defeat_age<1.3&&motion.polished&&motion.fallen_instances>=1&&first.defeat_age/world_combat_motion::death_duration<1,"defeated fighter performs a grounded collapse"))return;
                capture("defeat-fall");arena_qa_character_fall_checked=true;
            }
            if(!arena_qa_character_grounded_checked&&arena_game.current_index()>=1&&first.defeat_age>=world_combat_motion::death_duration){
                const auto motion=world_combat_motion_stats(combat_motion);arena_qa_character_grounded_age=first.defeat_age;
                if(!check(anchored&&arena_qa_character_fall_checked&&motion.fallen_instances>=1,"fallen fighter remains grounded while the next challenger enters"))return;
                capture("defeat-grounded");arena_qa_character_grounded_checked=true;
            }
        }
        if(!arena_qa_blood_settled_checked&&arena_qa_finisher_frame&&arena_game.current_index()==0&&
                arena_game.state()==S::Intermission&&blood_effects.stats().elapsed-arena_qa_finisher_clock>=1.05){
            const auto stage=blood_effects.stage();const auto rendered=world_blood_stats(blood_renderer);
            arena_qa_blood_settle_elapsed=blood_effects.stats().elapsed-arena_qa_finisher_clock;
            arena_qa_blood_settle_stains=stage.stain_count;
            if(!check(stage.mesh_count==0&&rendered.active_meshes==0&&stage.stain_count>0&&
                    rendered.persistent_stains_drawn>0&&rendered.minimum_stain_fade==1.f,
                    "blood settles within one second while ground splatter remains"))return;
            capture("blood-settled");arena_qa_blood_settled_checked=true;
        }
        if(!arena_qa_blood_persistent_checked&&arena_game.current_index()>=4&&
                blood_effects.stats().elapsed-arena_qa_blood_witness_elapsed>=30.){
            const auto stage=blood_effects.stage();const auto rendered=world_blood_stats(blood_renderer);
            const double elapsed=blood_effects.stats().elapsed-arena_qa_blood_witness_elapsed;
            bool retained=false;
            for(unsigned i=0;i<stage.stain_count;++i){const auto &mark=stage.stains[i];
                if(mark.serial!=arena_qa_blood_floor_serial)continue;
                retained=true;arena_qa_blood_persistent_age=mark.age;
                arena_qa_blood_witness_anchored=mark.position.x==arena_qa_blood_witness.position.x&&
                    mark.position.y==arena_qa_blood_witness.position.y&&mark.position.z==arena_qa_blood_witness.position.z&&
                    mark.yaw==arena_qa_blood_witness.yaw&&mark.hit_id==arena_qa_blood_witness.hit_id&&mark.persistent&&
                    std::abs(mark.age-arena_qa_blood_witness.age-elapsed)<1e-6;
            }
            if(!check(retained&&arena_qa_blood_witness_anchored&&arena_qa_blood_persistent_age>=30.&&
                    blood_effects.config().persistent_stains&&rendered.minimum_stain_fade==1.f&&
                    rendered.oldest_stain_age>=30.&&rendered.persistent_stains_drawn==stage.stain_count&&stage.stain_count>0,
                    "ground blood remains anchored and fully visible after thirty seconds"))return;
            capture("blood-persistent");arena_qa_blood_persistent_checked=true;
        }
        if(arena_game.defeated()>=2)capture("queue-progress");
        if(arena_game.state()==S::Won){
            bool all=true;for(const auto &actor:arena_game.challengers())all&=actor.state==C::Defeated;
            for(bool fought:arena_qa_fought)all&=fought;
            if(!check(all&&arena_game.defeated()==8&&fight.health>0&&arena_game.arrow_hits()>0&&arena_qa_total_hits>=8&&arena_qa_total_parries>0,"eight sequential real duels won with bow hit sword contacts and parries"))return;
            if(!check(!arena_reward_pending&&arena_saved_reward==80&&arena_progress.balance==arena_qa_initial_purse+80&&arena_progress.wins==arena_qa_initial_wins+1&&saved_purse(arena_qa_initial_purse+80),"eight-opponent victory saves exactly eighty arena coins"))return;
            const auto gore=blood_effects.stats();const auto source=world_blood_stats(blood_renderer);
            if(!check(arena_qa_gore_checked&&arena_qa_finisher_checked&&arena_qa_blood_persistent_checked&&arena_qa_blood_settled_checked&&arena_qa_character_fall_checked&&arena_qa_character_grounded_checked&&world_combat_motion_stats(combat_motion).peak_fallen_instances==8&&gore.finisher_events>=8&&gore.secondary_spawned==gore.impacts_spawned&&source.secondary_draws>0&&source.peak_meshes<=8&&source.peak_stains<=world_blood_effects::stain_capacity,"heavy blood sprays and finishers remain bounded through eight fights"))return;
            capture("arena-victory");neutral();next(16);return;
        }
        if(arena_game.state()==S::Lost){fail("ordinary sword and parry policy lost the arena run");return;}
        if(!arena_controls_ready()){neutral();return;}
        if(arena_game.state()!=S::Fighting||fight.state!=TrainingState::Fighting){stick(0,0);lt(false);return;}
        arena_qa_fought[arena_game.current_index()]=true;
        if(fight.enemy_phase==TrainingEnemyPhase::Windup){
            if(fight.enemy_duration-fight.enemy_time<.17&&fight.player_phase==TrainingPlayerPhase::Ready&&fight.stamina>=10){guard_stick(fight.enemy_direction);lt(true);}
            else{stick(0,0);lt(false);}return;
        }
        if(preview_pad.guard||fight.guard){lt(false);stick(0,0);arena_qa_attack_after=qa_frame+5;return;}
        if(fight.player_phase!=TrainingPlayerPhase::Ready){stick(0,0);return;}
        const auto enemy=arena_game.active_position();const double distance=std::hypot(cam.x-arena_origin_x-enemy.x,cam.z-arena_origin_z-enemy.z);
        if(qa_frame>=arena_qa_attack_after&&fight.stamina>=35&&distance<=2.34&&fight.enemy_phase!=TrainingEnemyPhase::Strike){
            const auto direction=TrainingDirection((unsigned(fight.enemy_guard)+1)%4);
            attack_stick(direction);arena_qa_attack_after=qa_frame+36;
        }else stick(0,0);return;
    }
    case 16:
        if(age<5)return;if(key(SDLK_RETURN))next(17);return;
    case 17:
        if(age<5||!arena_controls_ready())return;if(!check(arena_game.state()==S::Entering&&queued()==8&&arena_game.defeated()==0&&fight.health==100&&arena_game.shots()==0,"focused repeat starts a fresh eight-knight arena"))return;
        if(!check(blood_effects.stage().mesh_count==0&&blood_effects.stage().stain_count==0&&world_blood_stats(blood_renderer).active_meshes==0&&world_blood_stats(blood_renderer).active_stains==0,"fresh run clears purchased fluid and floor stains"))return;
        capture("arena-reset");next(18);return;
    case 18:
        if(age<4)return;if(!check(arena_qa_captures==23,"all original combat checkpoints and setup captured"))return;
        if(key(SDLK_r))next(20);return;
    case 20:
        if(age<6)return;
        if(!check(arena_ui_phase==ArenaUiPhase::Setup&&arena_progress.balance==arena_qa_initial_purse+80,"ending a fresh run opens settings without another reward"))return;
        if(menu_button(SDL_CONTROLLER_BUTTON_DPAD_DOWN))next(21);return;
    case 21:
        if(age<7)return;if(menu_button(SDL_CONTROLLER_BUTTON_A))next(22);return;
    case 22:
        if(age<7)return;
        if(arena_total_draft>1){if(age%8==0)menu_button(SDL_CONTROLLER_BUTTON_DPAD_LEFT);return;}
        if(menu_button(SDL_CONTROLLER_BUTTON_A))next(23);return;
    case 23:
        if(age<7)return;if(menu_button(SDL_CONTROLLER_BUTTON_DPAD_UP))next(24);return;
    case 24:
        if(age<7)return;
        if(!check(arena_total_draft==1&&arena_simultaneous_draft==1,"ordinary menu navigation selects one total and one simultaneous"))return;
        capture("arena-one-setup");if(menu_button(SDL_CONTROLLER_BUTTON_A))next(25);return;
    case 25:
        if(!arena_controls_ready()){neutral();return;}
        if(arena_game.state()!=S::Fighting||fight.state!=TrainingState::Fighting||fight.player_phase!=TrainingPlayerPhase::Ready)return;
        if(!check(arena_game.config().total==1&&arena_game.config().simultaneous==1&&queued()==1,"one-opponent selection starts exactly one real challenger"))return;
        arena_qa_one_run=arena_run_id;arena_qa_combo_serial=fight.attack_start_serial;arena_qa_combo_links=fight.combo_links;arena_qa_combo_stamina=fight.stamina;
        attack_stick(TrainingDirection::Left);next(26);return;
    case 26:
        stick(0,0);
        if(!arena_qa_combo_first&&fight.player_phase==TrainingPlayerPhase::Strike){
            const auto pose=world_combat_motion_pose(combat_motion,0);
            if(!check(fight.attack_start_serial==arena_qa_combo_serial+1&&fight.stamina<=arena_qa_combo_stamina-15.9f&&pose.active,"first fresh flick commits a paid animated combo swing"))return;
            arena_qa_combo_first=true;arena_qa_combo_poses|=1;capture("combo-first");
        }
        if(arena_qa_combo_first&&fight.player_phase==TrainingPlayerPhase::Recovery){attack_stick(TrainingDirection::Right);next(27);}return;
    case 27:
        stick(0,0);
        if(!arena_qa_combo_second&&fight.player_phase==TrainingPlayerPhase::Strike&&fight.attack_start_serial==arena_qa_combo_serial+2){
            const auto pose=world_combat_motion_pose(combat_motion,0);
            if(!check(fight.combo_links==arena_qa_combo_links+1&&fight.stamina<=arena_qa_combo_stamina-31.9f&&pose.active,"fresh recovery flick links a second paid animated swing"))return;
            arena_qa_combo_second=true;arena_qa_combo_poses|=2;capture("combo-second");
        }
        if(arena_qa_combo_second&&fight.player_phase==TrainingPlayerPhase::Recovery){attack_stick(TrainingDirection::High);next(28);}return;
    case 28:
        // Keep this third physical stick gesture held through recovery: a
        // fourth attack would expose replaying a held direction as new input.
        if(fight.player_phase!=TrainingPlayerPhase::Strike||fight.attack_start_serial!=arena_qa_combo_serial+3)return;
        if(!check(fight.combo_links==arena_qa_combo_links+2&&fight.combo_count==3&&fight.stamina<=arena_qa_combo_stamina-47.9f&&world_combat_motion_pose(combat_motion,0).active,"third fresh recovery flick completes a three-swing paid combo"))return;
        arena_qa_combo_third=true;arena_qa_combo_poses|=4;capture("combo-third");next(29);return;
    case 29:
        if(age<70)return;
        if(!check(fight.attack_start_serial==arena_qa_combo_serial+3&&!fight.pending_attack&&arena_qa_combo_poses==7,"held third gesture cannot create a fourth combo swing"))return;
        capture("combo-held");neutral();arena_qa_attack_after=qa_frame+6;next(30);return;
    case 30:
        if(arena_game.state()==S::Lost){fail("one-opponent real combo and parry run lost");return;}
        if(arena_game.state()!=S::Won){sword_policy();return;}
        if(!check(arena_game.defeated()==1&&!arena_reward_pending&&arena_saved_reward==10&&arena_progress.balance==arena_qa_initial_purse+90&&arena_progress.wins==arena_qa_initial_wins+2&&saved_purse(arena_qa_initial_purse+90),"one-opponent real victory persists exactly ten more coins"))return;
        neutral();capture("arena-one-victory");next(31);return;
    case 31:
        if(age<90)return;
        if(!check(arena_progress.balance==arena_qa_initial_purse+90&&saved_purse(arena_qa_initial_purse+90),"repeated result frames and disk reload cannot duplicate coins"))return;
        arena_qa_wallet_verified=true;if(key(SDLK_RETURN))next(32);return;
    case 32:
        if(age<6||!arena_controls_ready())return;
        if(!check(arena_run_id>arena_qa_one_run&&arena_progress.balance==arena_qa_initial_purse+90&&arena_game.config().total==1,"repeat allocates a unique receipt while preserving the saved purse"))return;
        if(key(SDLK_r))next(33);return;
    case 33:
        if(age<7)return;if(menu_button(SDL_CONTROLLER_BUTTON_DPAD_DOWN))next(34);return;
    case 34:
        if(age<7)return;if(menu_button(SDL_CONTROLLER_BUTTON_A))next(35);return;
    case 35:
        if(age<7)return;
        if(arena_total_draft<8){if(age%8==0)menu_button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);return;}
        if(menu_button(SDL_CONTROLLER_BUTTON_A))next(36);return;
    case 36:
        if(age<7)return;if(menu_button(SDL_CONTROLLER_BUTTON_DPAD_DOWN))next(37);return;
    case 37:
        if(age<7)return;if(menu_button(SDL_CONTROLLER_BUTTON_A))next(38);return;
    case 38:
        if(age<7)return;
        if(arena_simultaneous_draft<8){if(age%8==0)menu_button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);return;}
        if(menu_button(SDL_CONTROLLER_BUTTON_A))next(39);return;
    case 39:
        if(age<7)return;if(menu_button(SDL_CONTROLLER_BUTTON_DPAD_UP))next(40);return;
    case 40:
        if(age<7)return;if(menu_button(SDL_CONTROLLER_BUTTON_DPAD_UP))next(41);return;
    case 41:
        if(age<7)return;
        if(!check(arena_total_draft==8&&arena_simultaneous_draft==8,"ordinary menu navigation selects eight simultaneous opponents"))return;
        capture("arena-many-setup");if(menu_button(SDL_CONTROLLER_BUTTON_A))next(42);return;
    case 42:
        neutral();
        if(arena_game.config().total!=8||arena_game.config().simultaneous!=8||arena_ui_phase!=ArenaUiPhase::Fight)return;
        if(arena_game.active_count()==8){
            if(!check(arena_game.config().simultaneous==8&&arena_game.entering_count()==0,"controller A starts eight genuinely active independent opponents"))return;
            if(!check(!arena_target_lock.locked()&&arena_camera_many_frames>0&&arena_camera_heading_error<.0001&&arena_camera_lateral_error<.0001,"eight-opponent camera keeps the player centered without automatic enemy framing"))return;
            capture("arena-many-active");arena_qa_lock_attack_serial=fight.attack_start_serial;next(48);
        }return;
    case 48:
        neutral();if(age<2)return;
        if(menu_button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))next(45);return;
    case 45:
        neutral();if(age<8)return;
        if(!check(arena_target_lock.locked()&&fight.attack_start_serial==arena_qa_lock_attack_serial,"controller RB locks an enemy without starting a thrust"))return;
        arena_qa_locked_target=arena_target_lock.target();capture("arena-lock-on");next(46);return;
    case 46:
        neutral();if(age<24)return;
        if(!check(arena_target_lock.target()==arena_qa_locked_target&&arena_game.current_index()==arena_qa_locked_target&&arena_camera_heading_error<.0001&&arena_camera_lateral_error<.0001,"explicit lock keeps one target and retains the normal player camera"))return;
        if(menu_button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))next(47);return;
    case 47:
        neutral();if(age<8)return;
        if(!check(!arena_target_lock.locked()&&fight.attack_start_serial==arena_qa_lock_attack_serial&&arena_camera_heading_error<.0001&&arena_camera_lateral_error<.0001,"second RB releases the target without a camera snap or attack"))return;
        next(43);return;
    case 43:
        neutral();arena_qa_concurrent_attackers=0;
        for(unsigned id=0;id<8;++id)if(const auto *opponent=arena_game.opponent(id))arena_qa_concurrent_attackers+=opponent->hits_dealt>0;
        if(arena_game.state()!=S::Lost)return;
        if(!check(fight.health==0&&arena_qa_concurrent_attackers>=2&&arena_game.defeated()==0,"independent simultaneous opponents inflict a genuine shared-player defeat"))return;
        if(!check(arena_progress.balance==arena_qa_initial_purse+90&&saved_purse(arena_qa_initial_purse+90),"simultaneous defeat awards no coins and preserves both saved victories"))return;
        capture("arena-many-defeat");next(44);return;
    case 44:
        if(age<5)return;if(!check(arena_qa_captures==33,"all configured combat combo and purse checkpoints captured"))return;
        arena_qa_passed=true;launch->frames=int(qa_frame+30);std::printf("[Arena QA] COMPLETE %u checks, %u SDL inputs, %u captures, real loss%u hits, eight victories/%u contacts/%u parries\n",arena_qa_checks,arena_qa_events,arena_qa_captures,arena_qa_loss_hits,arena_qa_total_hits,arena_qa_total_parries);std::fflush(stdout);return;
    default:fail("invalid arena QA phase");return;
    }
};
auto arena_qa_capture=[&](){
    if(!arena_qa||arena_qa_capture_pending.empty())return;
    const auto name=arena_qa_capture_pending;arena_qa_capture_pending.clear();bool ok=arena_qa_capture_dir&&*arena_qa_capture_dir;
    std::vector<unsigned char> pixels(size_t(display_w)*display_h*4);glBindFramebuffer(GL_FRAMEBUFFER,0);glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,display_w,display_h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());ok=ok&&glGetError()==GL_NO_ERROR;
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,display_w,display_h,32,SDL_PIXELFORMAT_RGBA32);ok=ok&&surface;
    if(ok){for(int y=0;y<display_h;++y)std::memcpy(static_cast<unsigned char*>(surface->pixels)+size_t(y)*surface->pitch,pixels.data()+size_t(display_h-1-y)*display_w*4,size_t(display_w)*4);
        const auto root=std::filesystem::path(arena_qa_capture_dir);ok=SDL_SaveBMP(surface,(root/(name+".bmp")).u8string().c_str())==0;
        std::array<unsigned,5> counts={};for(const auto &actor:arena_game.challengers())++counts[unsigned(actor.state)];
        unsigned flying=0,impacts=0;for(const auto &arrow:arena_game.arrows()){flying+=arrow.state==world_arena_combat::ArrowState::Flying;impacts+=arrow.state==world_arena_combat::ArrowState::HitEnemy;}
        const auto effect=combat_vfx.stats();world_combat_vfx::Stage staged;
        const float axis_right[3]={1,0,0},axis_up[3]={0,1,0};combat_vfx.stage({cam.x,cam.y,cam.z},axis_right,axis_up,staged);
        unsigned blood=0,metal=0;for(unsigned i=0;i<staged.count;++i){const auto &v=staged.instances[i];blood+=v.style[0]==4;metal+=v.style[0]==0&&v.color[0]>1;}
        const auto purchased=world_blood_stats(blood_renderer);
        const auto motion=world_combat_motion_stats(combat_motion);const auto &first_defeat=arena_game.challengers()[0];
        const bool first_defeat_anchored=arena_qa_first_defeat_saved&&first_defeat.position.x==arena_qa_first_defeat_position.x&&first_defeat.position.y==arena_qa_first_defeat_position.y&&first_defeat.position.z==arena_qa_first_defeat_position.z&&first_defeat.defeat_yaw==arena_qa_first_defeat_yaw;
        const auto gore_stage=blood_effects.stage();unsigned live_finishers=0;
        for(unsigned i=0;i<gore_stage.mesh_count;++i)live_finishers+=gore_stage.meshes[i].finisher;
        std::ostringstream blood_json;blood_json.imbue(std::locale::classic());
        blood_json<<", \"blood_loaded\": "<<(purchased.provenance_loaded?"true":"false")
            <<", \"blood_variants\": "<<purchased.loaded_variants<<", \"blood_source_frames\": "<<purchased.source_frames
            <<", \"blood_meshes\": "<<purchased.active_meshes<<", \"blood_stains\": "<<purchased.active_stains
            <<", \"blood_frame\": "<<purchased.last_frame<<", \"blood_frame_transitions\": "<<purchased.frame_transitions
            <<", \"blood_mesh_draws\": "<<purchased.mesh_draws<<", \"blood_stain_draws\": "<<purchased.stain_draws
            <<", \"blood_elapsed\": "<<blood_effects.stats().elapsed<<", \"blood_paused\": "<<(preview_screen==2?"true":"false")
            <<", \"blood_failures\": "<<purchased.failures
            <<", \"blood_scale\": "<<WorldBloodFrame{}.scale<<", \"blood_secondary_draws\": "<<purchased.secondary_draws
            <<", \"blood_finisher_draws\": "<<purchased.finisher_draws<<", \"blood_finisher_events\": "<<blood_effects.stats().finisher_events
            <<", \"blood_max_strength\": "<<blood_effects.stats().max_strength<<", \"blood_initial_finishers\": "<<combat_vfx.stats().finisher_bursts
            <<", \"blood_live_finishers\": "<<live_finishers
            <<", \"blood_persistent_mode\": "<<(blood_effects.config().persistent_stains?"true":"false")
            <<", \"blood_stain_capacity\": "<<world_blood_effects::stain_capacity
            <<", \"blood_minimum_stain_fade\": "<<purchased.minimum_stain_fade
            <<", \"blood_oldest_stain_age\": "<<purchased.oldest_stain_age
            <<", \"blood_persistent_stains_drawn\": "<<purchased.persistent_stains_drawn
            <<", \"blood_witness_serial\": "<<arena_qa_blood_floor_serial
            <<", \"blood_witness_age\": "<<arena_qa_blood_persistent_age
            <<", \"blood_witness_anchored\": "<<(arena_qa_blood_witness_anchored?"true":"false")
            <<", \"blood_playback_rate\": "<<world_blood_effects::playback_rate
            <<", \"blood_max_air_seconds\": "<<blood_effects.config().mesh_duration+.035
            <<", \"blood_staged_meshes\": "<<gore_stage.mesh_count
            <<", \"blood_settle_elapsed\": "<<arena_qa_blood_settle_elapsed
            <<", \"motion_polished\": "<<(motion.polished?"true":"false")
            <<", \"motion_pose_assets\": "<<motion.pose_assets<<", \"reaction_assets\": "<<motion.reaction_assets<<", \"death_assets\": "<<motion.death_assets
            <<", \"fallen_count\": "<<motion.fallen_instances<<", \"first_defeat_age\": "<<first_defeat.defeat_age
            <<", \"first_defeat_phase\": "<<std::clamp(first_defeat.defeat_age/world_combat_motion::death_duration,0.,1.)
            <<", \"first_defeat_anchored\": "<<(first_defeat_anchored?"true":"false");
        blood_json<<", \"total_opponents\": "<<arena_game.config().total<<", \"simultaneous_opponents\": "<<arena_game.config().simultaneous
            <<", \"unused\": "<<counts[4]<<", \"ui_phase\": "<<unsigned(arena_ui_phase)<<", \"purse\": "<<arena_progress.balance
            <<", \"run_id\": "<<arena_run_id<<", \"saved_reward\": "<<arena_saved_reward<<", \"reward_pending\": "<<(arena_reward_pending?"true":"false")
            <<", \"attack_serial\": "<<fight.attack_start_serial<<", \"combo_links\": "<<fight.combo_links<<", \"combo_count\": "<<fight.combo_count<<", \"stamina\": "<<fight.stamina
            <<", \"initial_purse\": "<<arena_qa_initial_purse<<", \"draft_total\": "<<arena_total_draft<<", \"draft_simultaneous\": "<<arena_simultaneous_draft
            <<", \"payout_preview\": "<<10*arena_total_draft<<", \"combo_pending\": "<<(fight.pending_attack?"true":"false")
            <<", \"concurrent_damage_sources\": "<<arena_qa_concurrent_attackers
            <<", \"target_locked\": "<<(arena_target_lock.locked()?"true":"false")<<", \"lock_target\": "<<arena_target_lock.target()<<", \"lock_toggles\": "<<arena_lock_toggles
            <<", \"camera_heading_error\": "<<arena_camera_heading_error<<", \"camera_lateral_error\": "<<arena_camera_lateral_error<<", \"camera_pitch_error\": "<<arena_camera_pitch_error;
        std::ofstream state(root/(name+".json"));state.imbue(std::locale::classic());state<<std::setprecision(17)<<"{\"name\": "<<world_json::quote(name)<<", \"frame\": "<<arena_qa_frame<<", \"state\": "<<unsigned(arena_game.state())<<", \"weapon\": "<<unsigned(arena_game.weapon())<<", \"third_person\": "<<(third_person?"true":"false")<<", \"drawing\": "<<(arena_game.drawing()?"true":"false")<<", \"draw\": "<<arena_game.draw_fraction()<<", \"shots\": "<<arena_game.shots()<<", \"arrow_hits\": "<<arena_game.arrow_hits()<<", \"flying_arrows\": "<<flying<<", \"arrow_hit_markers\": "<<impacts<<", \"defeated\": "<<arena_game.defeated()<<", \"current\": "<<arena_game.current_index()<<", \"health\": "<<fight.health<<", \"waiting\": "<<counts[0]<<", \"entering\": "<<counts[1]<<", \"active\": "<<counts[2]<<", \"yielded\": "<<counts[3]<<", \"body_bursts\": "<<effect.body_bursts<<", \"metal_bursts\": "<<effect.metal_bursts<<", \"material\": "<<unsigned(effect.last_material)<<", \"blood_particles\": "<<blood<<", \"metal_particles\": "<<metal<<", \"contact_kind\": "<<combat_contact_kind<<", \"damage_cue\": "<<combat_contact_damage<<", \"body_audio\": "<<training_sounds.body_events<<", \"metal_audio\": "<<training_sounds.metal_events<<", \"player_pose\": "<<world_combat_motion_pose(combat_motion,0).slot<<", \"enemy_pose\": "<<world_combat_motion_pose(combat_motion,1).slot<<blood_json.str()<<"}\n";state.flush();ok=ok&&bool(state);
    }
    if(surface)SDL_FreeSurface(surface);if(!ok){std::fprintf(stderr,"[Arena QA] capture failed: %s\n",name.c_str());launch->exit_code=1;running=false;return;}
    ++arena_qa_captures;arena_qa_capture_names.push_back(name);
};
auto arena_qa_report=[&](FILE *file){
    std::fprintf(file,"  \"arena_camera\": {\"player_follow_frames\": %llu, \"multiple_opponent_frames\": %llu, \"locked_frames\": %llu, \"lock_toggles\": %u, \"maximum_heading_error\": %.9f, \"maximum_lateral_error\": %.9f, \"maximum_pitch_error\": %.9f},\n",(unsigned long long)arena_camera_frames,(unsigned long long)arena_camera_many_frames,(unsigned long long)arena_camera_locked_frames,arena_lock_toggles,arena_camera_max_heading_error,arena_camera_max_lateral_error,arena_camera_max_pitch_error);
    std::fprintf(file,"  \"arena_qa\": {\"enabled\": %s, \"passed\": %s, \"checks\": %u, \"events\": %u, \"captures\": %u, \"loss_hits\": %u, \"total_hits\": %u, \"total_parries\": %u, \"focus_loss_frames\": %u, \"focus_reacquire_requests\": %u, \"keyboard_capture_frames\": %u, \"combat_state_mutations\": 0, \"player_pose_writes\": 0, \"story_state_writes\": 0, \"check_labels\": [",arena_qa?"true":"false",arena_qa_passed?"true":"false",arena_qa_checks,arena_qa_events,arena_qa_captures,arena_qa_loss_hits,arena_qa_total_hits,arena_qa_total_parries,arena_qa_focus_loss_frames,arena_qa_focus_requests,arena_qa_keyboard_capture_frames);
    for(size_t i=0;i<arena_qa_labels.size();++i)std::fprintf(file,"%s%s",i?", ":"",world_json::quote(arena_qa_labels[i]).c_str());std::fprintf(file,"], \"capture_names\": [");
    for(size_t i=0;i<arena_qa_capture_names.size();++i)std::fprintf(file,"%s%s",i?", ":"",world_json::quote(arena_qa_capture_names[i]).c_str());
    std::fprintf(file,"], \"initial_purse\": %llu, \"final_purse\": %llu, \"wallet_disk_verified\": %s, \"combo_pose_witnesses\": %u, \"concurrent_damage_sources\": %u, \"final_total\": %u, \"final_simultaneous\": %u},\n",
        (unsigned long long)arena_qa_initial_purse,(unsigned long long)arena_progress.balance,arena_qa_wallet_verified?"true":"false",arena_qa_combo_poses,arena_qa_concurrent_attackers,arena_game.config().total,arena_game.config().simultaneous);
};

// Separate first-person corrected-geometry witness. Only real SDL controller
// input and read-only observations; never player/enemy poses, health or AI.
const double grip_now=double(SDL_GetPerformanceCounter())/SDL_GetPerformanceFrequency();
if(grip_started&&grip_now-grip_started>40){fail("bounded first-person grip workflow timed out");return;}
if(arena_qa_phase>=10&&arena_game.state()==S::Lost){fail("first-person grip workflow suffered a real defeat");return;}
if(arena_qa_phase>=10&&!arena_controls_ready()){neutral();return;}
const char *grip_guard_names[]={"grip-axe-first-guard","grip-staff-first-guard"};
const char *grip_attack_names[]={"grip-axe-first-33","grip-staff-first-26"};
const char *grip_guard_labels[]={"grip axe raised guard is captured through the real first-person renderer","grip staff raised guard is captured through the real first-person renderer"};
const char *grip_paid_labels[]={"grip axe high-gain frame belongs to one real paid Right cut","grip staff high-gain frame belongs to one real paid Right cut"};
const char *grip_frame_labels[]={"grip axe33 interior blend actually draws corrected first-person geometry","grip staff26 interior blend actually draws corrected first-person geometry"};
const auto expected_grip_weapon=grip_weapon_index?W::Staff:W::GreatAxe;
switch(arena_qa_phase){
case 0:
    SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_PumpEvents();
    if(!check(arena_ui_phase==ArenaUiPhase::Setup&&preview_screen==2&&arena_game.state()==S::Idle&&arena_progress_ready,"grip setup is an unstarted private arena"))return;
    grip_started=grip_now;arena_qa_initial_purse=arena_progress.balance;
    neutral();capture("grip-setup");if(menu_button(SDL_CONTROLLER_BUTTON_DPAD_DOWN))next(1);return;
case 1:
    if(age<7)return;if(menu_button(SDL_CONTROLLER_BUTTON_A))next(2);return;
case 2:
    if(age<7)return;
    if(arena_total_draft>1){if(age%8==0)menu_button(SDL_CONTROLLER_BUTTON_DPAD_LEFT);return;}
    if(menu_button(SDL_CONTROLLER_BUTTON_A))next(3);return;
case 3:
    if(age<7)return;if(menu_button(SDL_CONTROLLER_BUTTON_DPAD_UP))next(4);return;
case 4:
    if(age<7)return;
    if(!check(arena_total_draft==1&&arena_simultaneous_draft==1,"grip ordinary menu selects one real opponent"))return;
    if(menu_button(SDL_CONTROLLER_BUTTON_A))next(10);return;
case 10:{
    neutral();if(arena_game.state()!=S::Fighting||fight.state!=TrainingState::Fighting||fight.player_phase!=TrainingPlayerPhase::Ready)return;
    const auto stats=world_combat_motion_grip_stats(combat_motion);
    if(!check(arena_game.config().total==1&&queued()==1&&stats.loaded&&stats.fp_certified_pairs>0&&stats.failures==0,"grip one-challenger start loads separately certified first-person metadata"))return;
    if(key(SDLK_3))next(11);return;
}
case 11:
    neutral();if(age<4)return;
    if(arena_game.weapon()!=W::SwordShield){fail("ordinary key3 did not equip sword and shield before axe cycle");return;}
    controller_test_pad.press(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);++arena_qa_events;next(12);return;
case 12:
    neutral();if(age<4)return;
    if(!check(arena_game.weapon()==W::GreatAxe&&fight.weapon==W::GreatAxe,"grip ordinary weapon controls equip the two-handed axe"))return;
    if(third_person)camera();next(13);return;
case 13:
    neutral();if(age<6)return;
    if(!check(!third_person&&!weapon_sheathed,"grip ordinary camera control enters first person"))return;
    next(14);return;
case 14:{
    lt(true);guard_stick(TrainingDirection::Right);
    const auto pose=world_combat_motion_pose(combat_motion,0);
    if(fight.player_phase!=TrainingPlayerPhase::Ready||!fight.guard||pose.guard_weight<.9f)return;
    if(!check(arena_game.weapon()==expected_grip_weapon&&!third_person&&pose.active&&pose.guard_weight>=.9f,grip_guard_labels[grip_weapon_index]))return;
    grip_requested_slot=std::numeric_limits<unsigned>::max();capture(grip_guard_names[grip_weapon_index]);
    next(15);return;
}
case 15:
    neutral();if(age<2)return;
    if(fight.player_phase!=TrainingPlayerPhase::Ready||fight.stamina<30)return;
    grip_serial_before=fight.attack_start_serial;grip_stamina_before=fight.stamina;
    grip_draws_before=world_combat_motion_grip_stats(combat_motion).arms_draws;
    ++grip_attempts;attack_stick(TrainingDirection::Right);
    grip_requested_slot=grip_weapon_index?26:33;capture(grip_attack_names[grip_weapon_index]);next(16);return;
case 16:{
    neutral();
    if(std::find(arena_qa_capture_names.begin(),arena_qa_capture_names.end(),grip_attack_names[grip_weapon_index])!=arena_qa_capture_names.end()){
        const float cost=16*world_weapon_loadout::profile(expected_grip_weapon).stamina;
        if(!check(fight.attack_start_serial==grip_serial_before+1&&fight.attack_direction==TrainingDirection::Right&&fight.stamina<=grip_stamina_before-cost+.1f,grip_paid_labels[grip_weapon_index]))return;
        if(!check(grip_captured_slot==grip_requested_slot&&grip_captured_blend>.05f&&grip_captured_blend<.95f&&!third_person&&world_combat_motion_grip_stats(combat_motion).arms_draws>grip_draws_before&&world_combat_motion_grip_stats(combat_motion).failures==0,grip_frame_labels[grip_weapon_index]))return;
        grip_requested_slot=std::numeric_limits<unsigned>::max();next(17);return;
    }
    if(fight.player_phase==TrainingPlayerPhase::Ready&&fight.attack_start_serial>grip_serial_before){
        if(grip_attempts>=2){fail("two real Right cuts never rendered the certified interior FP grip window");return;}
        next(15);
    }
    return;
}
case 17:
    neutral();if(fight.player_phase!=TrainingPlayerPhase::Ready)return;
    if(grip_weapon_index==0){controller_test_pad.press(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);++arena_qa_events;next(18);return;}
    if(!check(arena_progress.balance==arena_qa_initial_purse&&saved_purse(arena_qa_initial_purse),"grip workflow preserves the private saved purse without a victory grant"))return;
    capture("grip-first-final");next(19);return;
case 18:
    neutral();if(age<4)return;
    if(!check(arena_game.weapon()==W::Staff&&fight.weapon==W::Staff&&!third_person,"grip ordinary weapon cycle equips staff in first person"))return;
    grip_weapon_index=1;grip_attempts=0;next(14);return;
case 19:
    neutral();if(age<2)return;
    if(!check(arena_qa_captures==6&&world_combat_motion_grip_stats(combat_motion).arms_draws>0&&world_combat_motion_grip_stats(combat_motion).failures==0,"grip six real-render checkpoints include both corrected first-person attack windows"))return;
    arena_qa_passed=true;launch->frames=int(qa_frame+30);
    std::printf("[Arena QA] GRIP COMPLETE %u checks, %u SDL inputs, %u captures\n",arena_qa_checks,arena_qa_events,arena_qa_captures);std::fflush(stdout);return;
default:fail("invalid first-person grip workflow phase");return;
}

// This optional route uses only the enclosing real virtual controller and
// tagged SDL key helpers. It writes QA witnesses, never combat/pose state.
// It is separate from the inherited94-check route and does not certify it.
const double continuity_now=double(SDL_GetPerformanceCounter())/SDL_GetPerformanceFrequency();
if(continuity_started&&continuity_now-continuity_started>40){fail("bounded continuity input supplement timed out");return;}
if(arena_qa_phase>=10&&arena_game.state()==S::Lost){fail("continuity input supplement suffered a real defeat");return;}
if(arena_qa_phase>=10&&!arena_controls_ready()){neutral();return;}
switch(arena_qa_phase){
case 0:
    SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_PumpEvents();
    if(!check(arena_ui_phase==ArenaUiPhase::Setup&&preview_screen==2&&arena_game.state()==S::Idle&&arena_progress_ready,"continuity setup is an unstarted private arena"))return;
    continuity_started=continuity_now;arena_qa_initial_purse=arena_progress.balance;
    neutral();capture("continuity-setup");if(menu_button(SDL_CONTROLLER_BUTTON_DPAD_DOWN))next(1);return;
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
    if(!check(arena_total_draft==1&&arena_simultaneous_draft==1,"continuity ordinary menu selects one real opponent"))return;
    if(menu_button(SDL_CONTROLLER_BUTTON_A))next(10);return;
case 10:
    neutral();
    if(arena_game.state()!=S::Fighting||fight.state!=TrainingState::Fighting||fight.player_phase!=TrainingPlayerPhase::Ready)return;
    if(!check(arena_game.config().total==1&&arena_game.config().simultaneous==1&&queued()==1&&fight.attack_start_serial==0,"continuity Start begins one challenger without an injected attack"))return;
    next(11);return;
case 11:{
    lt(true);guard_stick(TrainingDirection::Left);
    const auto pose=world_combat_motion_pose(combat_motion,0);
    if(!fight.guard||pose.guard_weight<.9f||fight.player_phase!=TrainingPlayerPhase::Ready)return;
    if(!check(fight.guard&&pose.active&&pose.guard_weight>=.9f,"continuity guarded entry begins from a genuinely rendered raised guard"))return;
    continuity_guarded_before=world_combat_motion_continuity_stats(combat_motion).guarded_entries;
    continuity_serial_before=fight.attack_start_serial;continuity_stamina_before=fight.stamina;
    capture("continuity-guard-ready");next(12);return;
}
case 12:
    // Hold LT through the preceding guard capture, then allow one complete
    // neutral sample here. SwordStick rearms only on that sample, not on a
    // same-frame LT-release/flick.
    // At max render dt .05 the previous >=.9 guard loses only .6, staying
    // positive for the actual guarded-entry classification on this new serial.
    neutral();if(age<2)return;attack_stick(TrainingDirection::Left);next(13);return;
case 13:{
    neutral();const auto counters=world_combat_motion_continuity_stats(combat_motion);
    if(fight.attack_start_serial==continuity_serial_before){if(age>30)fail("fresh guarded-entry flick did not commit a real attack");return;}
    if(counters.guarded_entries<=continuity_guarded_before){if(fight.player_phase==TrainingPlayerPhase::Recovery)fail("guarded entry was not selected by actual motion rendering");return;}
    if(!check(fight.attack_start_serial==continuity_serial_before+1&&fight.stamina<=continuity_stamina_before-15.9f,"continuity fresh flick commits exactly one paid guarded attack"))return;
    if(!check(counters.guarded_entries>continuity_guarded_before&&world_combat_motion_pose(combat_motion,0).active,"continuity guarded-entry rendering counter advances after real input"))return;
    continuity_guarded_observed=true;capture("continuity-guard-entry");next(14);return;
}
case 14:
    neutral();
    if(fight.player_phase!=TrainingPlayerPhase::Ready||world_combat_motion_pose(combat_motion,0).slot>=4||fight.stamina<48)return;
    continuity_serial_before=fight.attack_start_serial;continuity_links_before=fight.combo_links;continuity_stamina_before=fight.stamina;
    attack_stick(TrainingDirection::Right);next(15);return;
case 15:
    neutral();
    if(fight.attack_start_serial!=continuity_serial_before+1||fight.player_phase!=TrainingPlayerPhase::Strike)return;
    if(!check(fight.stamina<=continuity_stamina_before-15.9f&&world_combat_motion_pose(combat_motion,0).active,"continuity late-link preparation commits a paid first swing"))return;
    capture("continuity-late-first");next(16);return;
case 16:
    neutral();
    if(fight.player_phase!=TrainingPlayerPhase::Recovery||fight.player_time<.10)return;
    if(fight.player_time>=fight.player_duration-.025){fail("late-recovery observation window was missed");return;}
    capture("continuity-before-late");next(17);return;
case 17:
    neutral();
    // The preceding BMP readback can stall its following render frame. Let
    // that single frame finish before injecting a fresh late-link gesture.
    if(age<2)return;
    if(fight.player_phase!=TrainingPlayerPhase::Recovery){fail("first swing completed before the delayed recovery input");return;}
    if(fight.player_time<.145)return;
    if(!check(!fight.pending_attack&&fight.player_time>WorldTrainingCombat::combo_link_seconds&&fight.player_time<fight.player_duration-.01,"continuity second flick is deliberately delayed beyond the early combo bridge"))return;
    continuity_late_input_time=fight.player_time;continuity_recovery_pose=world_combat_motion_pose(combat_motion,0).slot;
    continuity_late_before=world_combat_motion_continuity_stats(combat_motion).late_combo_finishes;
    // Leave this physical gesture held to check that it cannot replay a link.
    attack_stick(TrainingDirection::High);next(18);return;
case 18:{
    const auto counters=world_combat_motion_continuity_stats(combat_motion);
    if(fight.attack_start_serial==continuity_serial_before+1){if(age>30)fail("delayed recovery flick did not commit a real combo");return;}
    if(counters.late_combo_finishes<=continuity_late_before){if(fight.player_phase==TrainingPlayerPhase::Recovery)fail("late combo did not select an actual finish or unwind branch");return;}
    if(!check(fight.attack_start_serial==continuity_serial_before+2&&fight.combo_links==continuity_links_before+1&&fight.combo_count==2&&fight.stamina<=continuity_stamina_before-31.9f,"continuity delayed flick links exactly one second paid swing"))return;
    if(!check(counters.late_combo_finishes>continuity_late_before&&world_combat_motion_pose(combat_motion,0).active,"continuity late finish or unwind rendering counter advances after real input"))return;
    continuity_late_observed=true;capture("continuity-late-finish");next(19);return;
}
case 19:
    // Observe the held gesture beyond actual recovery at every render rate.
    if(fight.player_phase!=TrainingPlayerPhase::Ready)return;
    if(!continuity_held_ready_clock)continuity_held_ready_clock=continuity_now;
    if(continuity_now-continuity_held_ready_clock<.25)return;
    if(!check(fight.attack_start_serial==continuity_serial_before+2&&fight.combo_links==continuity_links_before+1&&!fight.pending_attack,"continuity held delayed gesture cannot replay a third swing"))return;
    if(!check(arena_progress.balance==arena_qa_initial_purse&&saved_purse(arena_qa_initial_purse),"continuity input supplement preserves its private saved purse"))return;
    neutral();capture("continuity-held");next(20);return;
case 20:
    neutral();if(age<2)return;
    if(!check(continuity_guarded_observed&&continuity_late_observed&&arena_qa_captures==7,"continuity all seven actual-render checkpoints and both branch witnesses are captured"))return;
    arena_qa_passed=true;launch->frames=int(qa_frame+30);
    std::printf("[Arena QA] CONTINUITY COMPLETE %u checks, %u SDL inputs, %u captures\n",arena_qa_checks,arena_qa_events,arena_qa_captures);std::fflush(stdout);return;
default:fail("invalid continuity supplement phase");return;
}

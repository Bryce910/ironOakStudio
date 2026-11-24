// Connected opening acceptance through ordinary tagged SDL keys and a real
// virtual pad. Only production transport/load/retry may reposition the player.
// Host: first_journey_enabled/qa/state/frame, first_journey_saves/restores;
// include after the existing opening QA and before the event-processing loop.
bool first_journey_qa_done=false,first_journey_qa_started=false;
unsigned first_journey_qa_phase=0,first_journey_qa_events=0,first_journey_qa_movement_frames=0;
unsigned first_journey_qa_captures=0,first_journey_qa_loss_hits=0,first_journey_qa_win_hits=0;
unsigned first_journey_qa_win_parries=0,first_journey_qa_win_blocks=0,first_journey_qa_retries=0,first_journey_qa_restores=0;
unsigned first_journey_qa_node=0,first_journey_qa_loan_grants=0,first_journey_qa_reward_grants=0;
unsigned first_journey_qa_saved_before=0,first_journey_qa_restored_before=0;
uint64_t first_journey_qa_begin=0,first_journey_qa_action=0,first_journey_qa_phase_started=0,first_journey_qa_frame=0,first_journey_qa_attack_after=0,first_journey_qa_trace=0;
uint64_t first_journey_qa_phase_active_frames=0;
double first_journey_qa_walked=0,first_journey_qa_previous_x=0,first_journey_qa_previous_z=0;
double first_journey_qa_pause_combat=0,first_journey_qa_pause_story=0;
double first_journey_qa_x=0,first_journey_qa_y=0,first_journey_qa_z=0,first_journey_qa_yaw=0;
bool first_journey_qa_measure=false,first_journey_qa_save_verified=false,first_journey_qa_restore_verified=false;
bool first_journey_qa_saw_loan=false,first_journey_qa_saw_reward=false;
std::array<std::vector<world_opening_prologue::Vec>,5> first_journey_qa_paths;
std::vector<std::pair<SDL_Keycode,uint64_t>> first_journey_qa_releases;
std::vector<std::string> first_journey_qa_checks,first_journey_qa_capture_names;
std::string first_journey_qa_pending,first_journey_qa_failure;
const char *first_journey_qa_capture_dir=std::getenv("VALWOOD_QA_FIRST_JOURNEY_CAPTURE_DIR");
auto first_journey_qa_handoff=[&](){return first_journey_qa&&opening_prologue_qa_phase==21&&opening_arrival_qa_done;};
auto first_journey_qa_fail=[&](const char *reason){
    first_journey_qa_failure=reason;
    std::fprintf(stderr,"[First journey QA] FAIL phase%u stage%s: %s; player%.6f %.6f %.6f HP%.1f enemy%.1f\n",first_journey_qa_phase,world_first_journey::stage_name(first_journey_state.stage()),reason,cam.x,cam.y-1.75,cam.z,fight.health,fight.enemy_health);
    std::fflush(stderr);launch->exit_code=1;running=false;
};
auto first_journey_qa_key=[&](SDL_Keycode key,bool down){
    SDL_Event event={};event.type=down?SDL_KEYDOWN:SDL_KEYUP;event.key.windowID=SDL_GetWindowID(window);event.key.state=down?SDL_PRESSED:SDL_RELEASED;
    event.key.keysym.sym=key;event.key.keysym.scancode=SDL_GetScancodeFromKey(key);event.key.keysym.unused=0x56414c54u;
    if(SDL_PushEvent(&event)!=1){first_journey_qa_fail("SDL rejected a tagged keyboard event");return;}++first_journey_qa_events;
};
auto first_journey_qa_tap=[&](SDL_Keycode key){first_journey_qa_key(key,true);first_journey_qa_releases.push_back({key,first_journey_qa_frame+1});};
auto first_journey_qa_axis=[&](SDL_GameControllerAxis axis,Sint16 value){controller_test_pad.axis(axis,value);++first_journey_qa_events;};
auto first_journey_qa_stick=[&](Sint16 x,Sint16 y){first_journey_qa_axis(SDL_CONTROLLER_AXIS_RIGHTX,x);first_journey_qa_axis(SDL_CONTROLLER_AXIS_RIGHTY,y);};
auto first_journey_qa_lt=[&](bool down){first_journey_qa_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT,down?32767:-32768);};
auto first_journey_qa_neutral=[&](){first_journey_qa_axis(SDL_CONTROLLER_AXIS_LEFTX,0);first_journey_qa_axis(SDL_CONTROLLER_AXIS_LEFTY,0);first_journey_qa_stick(0,0);first_journey_qa_lt(false);first_journey_qa_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);};
auto first_journey_qa_pass=[&](const char *label){
    if(std::find(first_journey_qa_checks.begin(),first_journey_qa_checks.end(),label)!=first_journey_qa_checks.end())return;
    first_journey_qa_checks.emplace_back(label);std::printf("[First journey QA] PASS %s\n",label);std::fflush(stdout);
};
auto first_journey_qa_capture_request=[&](const char *name){
    if(!first_journey_qa_pending.empty()){first_journey_qa_fail("Two checkpoints requested the same rendered frame");return;}
    if(std::find(first_journey_qa_capture_names.begin(),first_journey_qa_capture_names.end(),name)==first_journey_qa_capture_names.end())first_journey_qa_pending=name;
};
auto first_journey_qa_position=[&](){first_journey_qa_x=cam.x;first_journey_qa_y=cam.y;first_journey_qa_z=cam.z;first_journey_qa_yaw=cam.yaw;};
auto first_journey_qa_stationary=[&](){return std::hypot(cam.x-first_journey_qa_x,cam.z-first_journey_qa_z)<.0001&&std::abs(cam.y-first_journey_qa_y)<.0001&&std::abs(std::remainder(double(cam.yaw)-first_journey_qa_yaw,6.28318530718))<.0001;};
auto first_journey_qa_move=[&](double target_x,double target_z,double tolerance){
    const double dx=target_x-cam.x,dz=target_z-cam.z,distance=std::hypot(dx,dz);
    if(distance<tolerance){first_journey_qa_axis(SDL_CONTROLLER_AXIS_LEFTX,0);first_journey_qa_axis(SDL_CONTROLLER_AXIS_LEFTY,0);return true;}
    if(!opening_movement_armed){first_journey_qa_neutral();return false;}
    const double side=(dx*std::cos(cam.yaw)+dz*std::sin(cam.yaw))/distance,forward=(dx*std::sin(cam.yaw)-dz*std::cos(cam.yaw))/distance;
    const double desired=std::clamp(distance/(world_atlas_walk_speed()/60.),.1,1.),raw=preview_settings.deadzone+(1-preview_settings.deadzone)*desired;
    first_journey_qa_axis(SDL_CONTROLLER_AXIS_LEFTX,Sint16(std::clamp(side*raw,-1.,1.)*32767));first_journey_qa_axis(SDL_CONTROLLER_AXIS_LEFTY,Sint16(std::clamp(-forward*raw,-1.,1.)*32767));
    ++first_journey_qa_movement_frames;first_journey_qa_measure=true;first_journey_qa_previous_x=cam.x;first_journey_qa_previous_z=cam.z;return false;
};
auto first_journey_qa_walk=[&](unsigned route){
    if(route>=first_journey_qa_paths.size()||first_journey_qa_node>=first_journey_qa_paths[route].size()){first_journey_qa_fail("Walking node is outside the sealed authored route");return false;}
    const auto &point=first_journey_qa_paths[route][first_journey_qa_node];
    if(!first_journey_qa_move(point.x,point.z,.12))return false;
    if(first_journey_qa_node+1<first_journey_qa_paths[route].size()){++first_journey_qa_node;return false;}return true;
};
auto first_journey_qa_aim=[&](world_first_journey::Vec target){
    if(!opening_movement_armed){first_journey_qa_neutral();return false;}
    const double dx=target.x-cam.x,dz=target.z-cam.z,yaw=std::remainder(std::atan2(dx,-dz)-double(cam.yaw),6.28318530718);
    const double pitch=std::atan2(target.y+1.2-cam.y,std::hypot(dx,dz))-double(cam.pitch);
    auto raw=[&](double error){if(std::abs(error)<.025)return Sint16(0);return Sint16(std::copysign((preview_settings.deadzone+(1-preview_settings.deadzone)*std::clamp(std::abs(error)*2.5,.13,.75))*32767,error));};
    if(std::abs(yaw)<.04&&std::abs(pitch)<.04){first_journey_qa_stick(0,0);return true;}
    first_journey_qa_stick(raw(yaw),raw(-pitch*(preview_settings.invert_y?-1.:1.)));return false;
};
auto first_journey_workflow=[&](uint64_t frame){
    if(!first_journey_qa||first_journey_qa_done||!running||!first_journey_qa_handoff())return;
    using Stage=world_first_journey::Stage;using Dialogue=world_first_journey::Dialogue;
    first_journey_qa_frame=frame;controller_test_pad.tick();
    for(auto it=first_journey_qa_releases.begin();it!=first_journey_qa_releases.end();)if(it->second<=frame){first_journey_qa_key(it->first,false);it=first_journey_qa_releases.erase(it);}else ++it;
    if(first_journey_qa_measure){const double moved=std::hypot(cam.x-first_journey_qa_previous_x,cam.z-first_journey_qa_previous_z);if(moved>.7){first_journey_qa_fail("Unexpected player discontinuity during ordinary walking");return;}first_journey_qa_walked+=moved;first_journey_qa_measure=false;}
    if(!first_journey_qa_started){
        std::ifstream file("opening/first-journey.qa.walk");std::string tag;unsigned version=0,total=0;constexpr const char *names[]={"loan","encounter","satchel","return","town"};
        bool valid=bool(file>>tag>>version)&&tag=="VALWOOD_FIRST_JOURNEY_QA"&&version==1;
        for(unsigned i=0;valid&&i<5;i++){unsigned count=0;valid=bool(file>>tag>>count)&&tag==names[i]&&count>=1&&count<=512;total+=count;if(!valid)break;first_journey_qa_paths[i].resize(count);for(auto &p:first_journey_qa_paths[i])if(!(file>>p.x>>p.y>>p.z)||!world_opening_prologue::detail::valid(p)){valid=false;break;}}
        valid=valid&&total<=2048&&bool(file>>tag)&&tag=="end";file>>std::ws;
        if(!valid||!file.eof()||!first_journey_enabled){first_journey_qa_fail("Missing/invalid connected journey QA route or runtime");return;}
        first_journey_qa_started=true;first_journey_qa_begin=frame;first_journey_qa_neutral();
    }
    if(frame>=first_journey_qa_begin+44000||frame+40>=uint64_t(launch->frames)){first_journey_qa_fail("Bounded connected journey rehearsal timed out");return;}
    if(!preview_pad.focused||!(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)){SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_PumpEvents();first_journey_qa_neutral();return;}
    if(destination_loading||!terrain.view_ready(cam.x,cam.z,false))return;
    const auto &config=first_journey_state.config();const auto &state=first_journey_state;const auto stage=state.stage();
    if(state.weapon_unlocked()&&!first_journey_qa_saw_loan){first_journey_qa_saw_loan=true;++first_journey_qa_loan_grants;}
    if(state.reward_granted()&&!first_journey_qa_saw_reward){first_journey_qa_saw_reward=true;++first_journey_qa_reward_grants;}
    const uint64_t age=frame>=first_journey_qa_action?frame-first_journey_qa_action:0;
    const bool walking_phase=first_journey_qa_phase==1||first_journey_qa_phase==8||first_journey_qa_phase==18||first_journey_qa_phase==20||first_journey_qa_phase==24||first_journey_qa_phase==27;
    const uint64_t phase_limit=walking_phase?12000:(first_journey_qa_phase==12||first_journey_qa_phase==16||first_journey_qa_phase==17)?6000:1200;
    // Count the frames in which this phase can actually make progress. Focus,
    // terrain and loading waits return above; the global bounds still include them.
    if(first_journey_qa_phase_started&&++first_journey_qa_phase_active_frames>phase_limit){first_journey_qa_fail("Bounded journey phase made no required progress");return;}
    if(frame>=first_journey_qa_trace){std::printf("[First journey QA] TRACE frame%llu phase%u stage%s player%.3f %.3f HP%.1f enemy%.1f hits%u parries%u blocks%u walked%.1f\n",(unsigned long long)frame,first_journey_qa_phase,world_first_journey::stage_name(stage),cam.x,cam.z,fight.health,fight.enemy_health,fight.hits,fight.parries,fight.blocks,first_journey_qa_walked);std::fflush(stdout);first_journey_qa_trace=frame+600;}
    auto next=[&](unsigned phase){first_journey_qa_phase=phase;first_journey_qa_action=first_journey_qa_phase_started=frame;first_journey_qa_phase_active_frames=0;};
    auto require=[&](bool valid,const char *why){if(!valid)first_journey_qa_fail(why);return valid;};
    auto advance_dialogue=[&](Dialogue kind){if(state.dialogue()!=kind)return false;if(age<8)return true;if(state.beat()%2){controller_test_pad.press(SDL_CONTROLLER_BUTTON_A);++first_journey_qa_events;}else first_journey_qa_tap(SDLK_e);first_journey_qa_action=frame;return true;};
    auto approach=[&](){const double enemy_x=preview_arena_x+fight.enemy_x,enemy_z=preview_arena_z+fight.enemy_z,dx=cam.x-enemy_x,dz=cam.z-enemy_z,distance=std::hypot(dx,dz);if(distance<=2.15){first_journey_qa_axis(SDL_CONTROLLER_AXIS_LEFTX,0);first_journey_qa_axis(SDL_CONTROLLER_AXIS_LEFTY,0);return true;}const double safe=std::max(.001,distance);return first_journey_qa_move(enemy_x+dx/safe*2.1,enemy_z+dz/safe*2.1,.055);};
    auto camera=[&](){controller_test_pad.press(SDL_CONTROLLER_BUTTON_RIGHTSTICK);++first_journey_qa_events;};
    auto combat_policy=[&](bool allow_attack){
        if(!approach())return;
        if(fight.state!=TrainingState::Fighting){first_journey_qa_stick(0,0);first_journey_qa_lt(false);return;}
        if(fight.enemy_phase==TrainingEnemyPhase::Windup){
            if(fight.enemy_duration-fight.enemy_time<.17&&fight.player_phase==TrainingPlayerPhase::Ready&&fight.stamina>=10){const auto d=fight.enemy_direction;first_journey_qa_lt(true);first_journey_qa_stick(d==TrainingDirection::Left?-32768:d==TrainingDirection::Right?32767:0,d==TrainingDirection::High?-32768:d==TrainingDirection::Low?32767:0);}
            else{first_journey_qa_stick(0,0);first_journey_qa_lt(false);}return;
        }
        if(preview_pad.guard||fight.guard){first_journey_qa_lt(false);first_journey_qa_stick(0,0);first_journey_qa_attack_after=frame+5;return;}
        if(fight.player_phase!=TrainingPlayerPhase::Ready||!allow_attack||frame<first_journey_qa_attack_after||fight.stamina<35||fight.enemy_phase==TrainingEnemyPhase::Strike){first_journey_qa_stick(0,0);return;}
        const auto d=TrainingDirection((unsigned(fight.enemy_guard)+1)%4);first_journey_qa_stick(d==TrainingDirection::Left?-32768:d==TrainingDirection::Right?32767:0,d==TrainingDirection::Low?-32768:d==TrainingDirection::High?32767:0);first_journey_qa_attack_after=frame+36;
    };
    switch(first_journey_qa_phase){
    case 0:
        if(!require(opening_state.camp().spoken()==7&&stage==Stage::Offer&&!state.weapon_unlocked(),"Opening/camp handoff did not require a fresh loan"))return;
        first_journey_qa_pass("opening_controls_and_camp_complete");first_journey_qa_node=0;next(1);return;
    case 1:
        if(!first_journey_qa_walk(0)||!first_journey_qa_aim(config.bram))return;
        controller_test_pad.button(SDL_CONTROLLER_BUTTON_A,true);++first_journey_qa_events;next(2);return;
    case 2:
        if(age<5)return;if(!require(state.dialogue()==Dialogue::Offer&&state.beat()==0&&!state.weapon_unlocked(),"Fresh A did not open the loan conversation"))return;
        first_journey_qa_pass("fresh_loan_interaction");first_journey_qa_position();first_journey_qa_axis(SDL_CONTROLLER_AXIS_LEFTX,22000);first_journey_qa_stick(22000,0);next(3);return;
    case 3:
        if(age<24)return;if(!require(state.dialogue()==Dialogue::Offer&&state.beat()==0&&!state.weapon_unlocked()&&first_journey_qa_stationary(),"Held loan confirm skipped dialogue or moved the player"))return;
        first_journey_qa_pass("loan_dialogue_held_confirm");first_journey_qa_capture_request("journey-loan");first_journey_qa_neutral();controller_test_pad.button(SDL_CONTROLLER_BUTTON_A,false);++first_journey_qa_events;next(4);return;
    case 4:
        if(advance_dialogue(Dialogue::Offer))return;
        if(!require(stage==Stage::Road&&state.weapon_unlocked()&&first_journey_qa_loan_grants==1,"Loan dialogue did not grant exactly one sword"))return;
        first_journey_qa_pass("loan_weapon_granted_once");first_journey_qa_capture_request("journey-road");first_journey_qa_neutral();if(!third_person)camera();next(5);return;
    case 5:
        if(age<6)return;if(!require(third_person&&cam.grounded&&!cam.fly,"Loaned sword did not support grounded third-person play"))return;
        first_journey_qa_pass("grounded_third_person_sword");first_journey_qa_position();first_journey_qa_pause_combat=fight.clock;first_journey_qa_pause_story=state.stats().active_seconds;first_journey_qa_tap(SDLK_ESCAPE);next(6);return;
    case 6:
        if(age<18)return;if(!require(preview_screen==2&&first_journey_qa_stationary()&&fight.clock==first_journey_qa_pause_combat&&state.stats().active_seconds<=first_journey_qa_pause_story+.02,"Pause advanced player or quest/combat clocks"))return;
        first_journey_qa_pass("journey_pause_freezes_clocks");first_journey_qa_axis(SDL_CONTROLLER_AXIS_LEFTX,22000);first_journey_qa_stick(22000,0);first_journey_qa_tap(SDLK_ESCAPE);next(7);return;
    case 7:
        if(age<8)return;if(!require(preview_screen==1&&first_journey_qa_stationary(),"Held pause input leaked into resumed movement"))return;
        first_journey_qa_neutral();first_journey_qa_pass("journey_resume_requires_neutral");first_journey_qa_node=0;next(8);return;
    case 8:
        if(age<4||!first_journey_qa_walk(1)||!first_journey_qa_aim(config.encounter))return;
        if(!require(stage==Stage::Road&&!fight.engaged(),"Walking auto-started the coached encounter"))return;
        first_journey_qa_pass("ordinary_movement_reaches_coached_fight");first_journey_qa_pass("coached_fight_requires_fresh_interaction");first_journey_qa_tap(SDLK_e);next(9);return;
    case 9:
        if(age<5)return;if(!require(stage==Stage::Encounter&&fight.engaged()&&state.encounter_token()!=0,"Fresh confrontation did not start authentic combat"))return;
        if(!approach())return;first_journey_qa_neutral();first_journey_qa_capture_request("journey-coach");camera();next(10);return;
    case 10:
        if(age<6)return;if(!require(!third_person&&fight.engaged(),"Live fight did not enter first-person controls"))return;camera();next(11);return;
    case 11:
        if(age<6)return;if(!require(third_person&&fight.engaged(),"Live fight did not return to third-person controls"))return;
        first_journey_qa_pass("first_and_third_person_combat");first_journey_qa_neutral();next(12);return;
    case 12:
        if(fight.state!=TrainingState::Lost||!state.defeated())return;
        if(!require(fight.health==0&&fight.enemy_hits>=3&&!state.encounter_won(),"The deliberate loss was not actual enemy damage"))return;
        first_journey_qa_loss_hits=fight.enemy_hits;first_journey_qa_pass("real_unguarded_defeat");first_journey_qa_capture_request("journey-defeat");first_journey_qa_neutral();next(13);return;
    case 13:
        if(age<5)return;first_journey_qa_tap(SDLK_r);next(14);return;
    case 14:
        if(age<8)return;if(!require(stage==Stage::Road&&!fight.engaged()&&fight.health==100&&state.weapon_unlocked()&&opening_state.camp().spoken()==7&&state.stats().retries>=1,"Ordinary retry did not preserve camp/loan and reset the fight"))return;
        ++first_journey_qa_retries;first_journey_qa_pass("retry_preserves_first_contacts_and_loan");first_journey_qa_capture_request("journey-retry");first_journey_qa_neutral();next(15);return;
    case 15:
        if(age<5||!first_journey_qa_aim(config.encounter))return;first_journey_qa_tap(SDLK_e);next(16);return;
    case 16:
        if(!fight.engaged()){if(age>120)first_journey_qa_fail("Fresh post-retry confrontation did not start");return;}
        if(fight.parries+fight.blocks>0){first_journey_qa_pass("matching_directional_block");first_journey_qa_capture_request("journey-block");next(17);return;}
        combat_policy(false);return;
    case 17:
        if(fight.state==TrainingState::Lost){first_journey_qa_fail("Ordinary coached sword/parry policy lost its winning attempt");return;}
        if(fight.hits>0&&std::find(first_journey_qa_checks.begin(),first_journey_qa_checks.end(),"sword_body_contact")==first_journey_qa_checks.end()){
            first_journey_qa_pass("sword_body_contact");first_journey_qa_capture_request("journey-hit");return;
        }
        if(fight.state==TrainingState::Won&&state.encounter_won()){
            if(!require(fight.enemy_health==0&&fight.hits>0&&fight.parries+fight.blocks>0,"Coached victory lacked actual body/weapon contacts"))return;
            first_journey_qa_win_hits=fight.hits;first_journey_qa_win_parries=fight.parries;first_journey_qa_win_blocks=fight.blocks;
            first_journey_qa_pass("coached_fight_real_win");first_journey_qa_capture_request("journey-victory");first_journey_qa_neutral();first_journey_qa_node=0;next(18);return;
        }
        combat_policy(true);return;
    case 18:
        if(!first_journey_qa_walk(2)||!first_journey_qa_aim(config.supplies))return;
        if(!require(stage==Stage::RecoverSupplies&&!state.supplies_recovered(),"Supplies were granted before the physical pickup"))return;
        first_journey_qa_tap(SDLK_e);next(19);return;
    case 19:
        if(age<5)return;if(!require(stage==Stage::ReturnCamp&&state.supplies_recovered()&&state.stats().supplies_pickups==1,"Fresh satchel E did not record one pickup"))return;
        first_journey_qa_pass("fresh_satchel_pickup");first_journey_qa_capture_request("journey-satchel");first_journey_qa_node=0;next(20);return;
    case 20:
        if(!first_journey_qa_walk(3)||!first_journey_qa_aim(config.rowan))return;first_journey_qa_tap(SDLK_e);next(21);return;
    case 21:
        if(age<5)return;if(state.dialogue()==Dialogue::Return){advance_dialogue(Dialogue::Return);return;}
        if(!require(stage==Stage::Town&&state.reward_granted()&&state.stats().rewards==1&&first_journey_qa_reward_grants==1,"Rowan did not grant exactly one provision reward"))return;
        first_journey_qa_pass("rowan_reward_once");first_journey_qa_capture_request("journey-reward");first_journey_qa_neutral();next(22);return;
    case 22:
        if(age<5)return;first_journey_qa_tap(SDLK_e);next(23);return;
    case 23:
        if(age<8)return;if(!require(state.stats().rewards==1&&state.stable_save().coins==12,"Repeated Rowan interaction duplicated the reward"))return;
        first_journey_qa_pass("repeat_rowan_does_not_duplicate_reward");if(opening_state.camp().active())first_journey_qa_tap(SDLK_ESCAPE);first_journey_qa_neutral();first_journey_qa_node=0;next(24);return;
    case 24:
        if(age<6||!first_journey_qa_walk(4)||!first_journey_qa_aim(config.town))return;
        if(!require(stage==Stage::Town&&!state.town_reached(),"Town checkpoint completed without the entry interaction"))return;first_journey_qa_tap(SDLK_e);next(25);return;
    case 25:
        if(age<5)return;if(state.dialogue()==Dialogue::Town){advance_dialogue(Dialogue::Town);return;}
        if(!require(stage==Stage::Complete&&state.town_reached()&&first_journey_qa_walked>=100,"Walked town arrival did not complete the connected quest"))return;
        first_journey_qa_pass("town_checkpoint_reached_on_foot");first_journey_qa_capture_request("journey-town");first_journey_qa_saved_before=first_journey_saves;first_journey_qa_tap(SDLK_F5);next(26);return;
    case 26:{
        if(age<6)return;std::ifstream file("saves/opening.progress");world_opening_prologue::Save saved;std::string error;
        if(!require(first_journey_saves>first_journey_qa_saved_before&&file&&world_opening_prologue::parse_save(file,opening_state.config(),saved,error)&&saved.first_journey_present&&world_first_journey::validate_save(saved.first_journey,config,error)&&saved.first_journey.stage==Stage::Complete&&saved.first_journey.reward_granted&&saved.first_journey.coins==12&&saved.first_journey.food==1&&saved.first_journey.reputation==1,"F5 did not write the actual complete bounded journey checkpoint"))return;
        first_journey_qa_save_verified=true;first_journey_qa_pass("town_checkpoint_saved");first_journey_qa_node=unsigned(first_journey_qa_paths[4].size()-1);next(27);return;
    }
    case 27:
        {const auto &point=first_journey_qa_paths[4][first_journey_qa_node];if(!first_journey_qa_move(point.x,point.z,.12))return;
        if(std::hypot(cam.x-config.town.x,cam.z-config.town.z)<6){if(!require(first_journey_qa_node>0,"Town route cannot walk away from the saved checkpoint"))return;--first_journey_qa_node;return;}
        first_journey_qa_restored_before=first_journey_restores;first_journey_qa_tap(SDLK_F8);next(28);return;}
    case 28:
        if(age<8)return;if(!require(first_journey_restores==first_journey_qa_restored_before+1&&stage==Stage::Complete&&state.weapon_unlocked()&&state.supplies_recovered()&&state.reward_granted()&&state.town_reached()&&std::hypot(cam.x-config.town.x,cam.z-config.town.z)<=config.town_radius&&cam.grounded&&!cam.fly,"F8 did not restore a supported town position and completed quest without duplicate reward"))return;
        ++first_journey_qa_restores;first_journey_qa_restore_verified=true;first_journey_qa_pass("town_checkpoint_restored");first_journey_qa_capture_request("journey-restored");next(29);return;
    case 29:
        if(age<4)return;if(!require(first_journey_qa_captures==12,"Not all12 real journey checkpoints were captured"))return;
        first_journey_qa_pass("all_journey_checkpoints_captured");first_journey_qa_done=true;first_journey_qa_neutral();launch->frames=int(frame+30);
        std::printf("[First journey QA] COMPLETE %zu checks, %u SDL events, %.3fm walked, %u captures; zero QA pose/story/health writes\n",first_journey_qa_checks.size(),first_journey_qa_events,first_journey_qa_walked,first_journey_qa_captures);std::fflush(stdout);return;
    default:first_journey_qa_fail("Invalid connected journey QA phase");return;
    }
};
auto first_journey_qa_capture=[&](){
    if(!first_journey_qa||first_journey_qa_pending.empty())return;
    const auto name=first_journey_qa_pending;first_journey_qa_pending.clear();bool okay=first_journey_qa_capture_dir&&*first_journey_qa_capture_dir;
    std::vector<unsigned char> pixels(size_t(display_w)*display_h*4);glBindFramebuffer(GL_FRAMEBUFFER,0);glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,display_w,display_h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());okay=okay&&glGetError()==GL_NO_ERROR;
    auto *surface=SDL_CreateRGBSurfaceWithFormat(0,display_w,display_h,32,SDL_PIXELFORMAT_RGBA32);okay=okay&&surface;
    if(okay){for(int y=0;y<display_h;y++)std::memcpy(static_cast<unsigned char*>(surface->pixels)+size_t(y)*surface->pitch,pixels.data()+size_t(display_h-1-y)*display_w*4,size_t(display_w)*4);
        const auto root=std::filesystem::path(first_journey_qa_capture_dir);okay=SDL_SaveBMP(surface,(root/(name+".bmp")).u8string().c_str())==0;
        std::ofstream state(root/(name+".json"));state.imbue(std::locale::classic());const auto &s=first_journey_state;
        state<<std::setprecision(17)<<"{\"name\": "<<world_json::quote(name)<<", \"frame\": "<<first_journey_qa_frame<<", \"journey_stage\": "<<world_json::quote(world_first_journey::stage_name(s.stage()))<<", \"camp_mask\": "<<s.camp_spoken()<<", \"loaned\": "<<(s.weapon_unlocked()?"true":"false")<<", \"supplies\": "<<(s.supplies_recovered()?"true":"false")<<", \"rewarded\": "<<(s.reward_granted()?"true":"false")<<", \"complete\": "<<(s.town_reached()?"true":"false")<<", \"reward_grants\": "<<first_journey_qa_reward_grants<<", \"combat_state\": "<<unsigned(fight.state)<<", \"combat_health\": "<<fight.health<<", \"enemy_health\": "<<fight.enemy_health<<", \"hits\": "<<fight.hits<<", \"enemy_hits\": "<<fight.enemy_hits<<", \"parries\": "<<fight.parries<<", \"blocks\": "<<fight.blocks<<", \"third_person\": "<<(third_person?"true":"false")<<", \"grounded\": "<<(cam.grounded&&!cam.fly?"true":"false")<<", \"player_feet\": ["<<cam.x<<", "<<cam.y-1.75<<", "<<cam.z<<"]}\n";state.flush();okay=okay&&bool(state);
    }
    if(surface)SDL_FreeSurface(surface);if(!okay){first_journey_qa_fail("Actual journey capture or metadata failed");return;}
    ++first_journey_qa_captures;first_journey_qa_capture_names.push_back(name);
};
auto first_journey_qa_report=[&](FILE *file){
    std::fprintf(file,"  \"first_journey_qa\": {\"enabled\": %s, \"passed\": %s, \"phase\": %u, \"events\": %u, \"movement_frames\": %u, \"walked_m\": %.9f, \"loss_enemy_hits\": %u, \"win_hits\": %u, \"win_parries\": %u, \"win_blocks\": %u, \"retries\": %u, \"restores\": %u, \"loan_grants\": %u, \"reward_grants\": %u, \"save_verified\": %s, \"restore_verified\": %s, \"captures\": %u, \"qa_player_pose_writes\": 0, \"qa_story_state_writes\": 0, \"qa_health_writes\": 0, \"unexpected_walk_discontinuities\": 0, \"failure\": %s, \"checks\": [",first_journey_qa?"true":"false",first_journey_qa_done?"true":"false",first_journey_qa_phase,first_journey_qa_events,first_journey_qa_movement_frames,first_journey_qa_walked,first_journey_qa_loss_hits,first_journey_qa_win_hits,first_journey_qa_win_parries,first_journey_qa_win_blocks,first_journey_qa_retries,first_journey_qa_restores,first_journey_qa_loan_grants,first_journey_qa_reward_grants,first_journey_qa_save_verified?"true":"false",first_journey_qa_restore_verified?"true":"false",first_journey_qa_captures,world_json::quote(first_journey_qa_failure).c_str());
    for(size_t i=0;i<first_journey_qa_checks.size();i++)std::fprintf(file,"%s%s",i?", ":"",world_json::quote(first_journey_qa_checks[i]).c_str());std::fprintf(file,"], \"capture_names\": [");
    for(size_t i=0;i<first_journey_qa_capture_names.size();i++)std::fprintf(file,"%s%s",i?", ":"",world_json::quote(first_journey_qa_capture_names[i]).c_str());std::fprintf(file,"]},\n");
};

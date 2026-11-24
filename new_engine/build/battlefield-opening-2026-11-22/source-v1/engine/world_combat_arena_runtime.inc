// Practice setup, independent purse, and real shared-player arena combat.
enum class ArenaUiPhase {Setup,Fight,Results};
ArenaUiPhase arena_ui_phase=ArenaUiPhase::Setup;
world_arena_progress::Progress arena_progress;
world_arena_progress::Config arena_run_config={};
world_arena_progress::NeutralGate arena_menu_gate,arena_gameplay_gate;
std::uint64_t arena_run_id=0,arena_saved_reward=0;
std::uint64_t arena_visual_contact_serial=0;
int arena_weapon_draft=0,arena_opponents_draft=0;
int arena_armor_finish_draft=0;
if(arena_enabled){std::error_code finish_ec;const auto finish_bytes=std::filesystem::file_size("arena_armor_finish.cfg",finish_ec);std::ifstream finish_file;unsigned style=0;
 if(!finish_ec&&finish_bytes<=1024)finish_file.open("arena_armor_finish.cfg");
 if(finish_file&&world_armor_wear::read(finish_file,style))arena_armor_finish_draft=int(style);
}
int arena_total_draft=8,arena_simultaneous_draft=1,arena_teammates_draft=0,arena_difficulty_draft=0,arena_focus_panel=-1;
bool arena_progress_ready=false,arena_reward_pending=false,arena_gameplay_ready=false,arena_was_modal=true;
bool arena_begin_pending=false;unsigned arena_begin_pending_frames=0;
std::string arena_progress_error;
std::array<WorldCombatArenaActor,world_combat_capacity::npcs> arena_motion_actors={};
bool arena_mouse_draw=false,arena_mouse_aim=false,arena_bow_held_before=false,arena_bow_armed=false;
unsigned arena_previous_shots=0,arena_enemy_steps=0;
world_arena_combat::Vec3 arena_last_enemy={};
Camera arena_aim_camera=cam;
WorldBowPresentation arena_bow_presentation;float arena_bow_visual_draw=0;
WorldArenaTargetLock arena_target_lock;
WorldArenaFocusGesture arena_pad_focus,arena_mouse_focus;
bool arena_focus_marker_visible=false;
unsigned arena_lock_cycles=0,arena_lock_releases=0,arena_lock_retargets=0;
unsigned arena_lock_toggles=0;
std::uint64_t arena_camera_frames=0,arena_camera_many_frames=0,arena_camera_locked_frames=0;
double arena_camera_heading_error=0,arena_camera_lateral_error=0,arena_camera_pitch_error=0;
double arena_camera_max_heading_error=0,arena_camera_max_lateral_error=0,arena_camera_max_pitch_error=0;
bool arena_camera_visual_valid=true;double arena_camera_focus_error=0,arena_camera_max_focus_error=0;unsigned long long arena_camera_shoulder_frames=0,arena_camera_unsafe_frames=0;
bool arena_camera_focus_transition=false;double arena_camera_goal_error=0,arena_camera_goal_l1_error=0,arena_camera_player_lateral=0;
unsigned long long arena_camera_transition_frames=0,arena_camera_steady_frames=0;
double arena_camera_max_transition_error=0,arena_camera_max_steady_focus_error=0,arena_camera_max_transition_age=0;
std::function<void()> arena_shot_sound;
auto arena_bow=[&](){return arena_enabled&&arena_game.weapon()==world_arena_combat::Weapon::Bow;};
auto arena_finished=[&](){return arena_game.state()==world_arena_combat::State::Won||arena_game.state()==world_arena_combat::State::Lost;};
auto arena_results_paused=[&](){return !preview_pad.focused||show_graphics||destination_loading||inventory_open||
    show_map||studio.open||studio.production_freeze()||studio.cinematic_preview||
    (studio.components_runtime&&studio.components_runtime->current_line());};
auto arena_raw_held=[&](){
    const auto *keys=SDL_GetKeyboardState(nullptr);
    for(auto key:{SDL_SCANCODE_RETURN,SDL_SCANCODE_KP_ENTER,SDL_SCANCODE_SPACE,SDL_SCANCODE_E,SDL_SCANCODE_R,
                  SDL_SCANCODE_ESCAPE,SDL_SCANCODE_W,SDL_SCANCODE_A,SDL_SCANCODE_S,SDL_SCANCODE_D,
                  SDL_SCANCODE_LEFT,SDL_SCANCODE_RIGHT,SDL_SCANCODE_UP,SDL_SCANCODE_DOWN,SDL_SCANCODE_T,
                  SDL_SCANCODE_1,SDL_SCANCODE_2,SDL_SCANCODE_3,SDL_SCANCODE_4,SDL_SCANCODE_5,SDL_SCANCODE_V,SDL_SCANCODE_J,SDL_SCANCODE_TAB,SDL_SCANCODE_Q,
                  SDL_SCANCODE_LSHIFT,SDL_SCANCODE_RSHIFT})if(keys[key])return true;
    if(SDL_GetMouseState(nullptr,nullptr)&(SDL_BUTTON_LMASK|SDL_BUTTON_RMASK|SDL_BUTTON_MMASK))return true;
    if(auto *pad=preview_pad.get()){
        for(int button=0;button<SDL_CONTROLLER_BUTTON_MAX;++button)if(SDL_GameControllerGetButton(pad,SDL_GameControllerButton(button)))return true;
        for(int axis=0;axis<SDL_CONTROLLER_AXIS_MAX;++axis){
            const int value=SDL_GameControllerGetAxis(pad,SDL_GameControllerAxis(axis));
            if(axis>=SDL_CONTROLLER_AXIS_TRIGGERLEFT?value>5000:std::abs(value)>6000)return true;
        }
    }
    return false;
};
auto arena_release=[&](){
    arena_pad_focus.reset();arena_mouse_focus.reset();
    arena_menu_gate.reset();arena_gameplay_gate.reset();arena_gameplay_ready=false;
    arena_game.cancel_draw();fight.clear_buffer();fight.set_guard(false);sword_stick.reset();
    arena_mouse_draw=arena_mouse_aim=arena_bow_held_before=arena_bow_armed=false;
    fight_mouse_x=fight_mouse_y=0;preview_release_mouse();
};
auto arena_modal_neutral=[&](){arena_release();};
auto arena_controls_ready=[&](){return !arena_enabled||(arena_ui_phase==ArenaUiPhase::Fight&&preview_screen==1&&!arena_finished()&&arena_gameplay_ready);};
auto arena_look_held=[&](){return arena_enabled&&!arena_bow()&&arena_controls_ready()&&
    (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_Q]||(preview_pad.get()&&preview_pad.left_trigger<=.4f&&SDL_GameControllerGetButton(preview_pad.get(),SDL_CONTROLLER_BUTTON_LEFTSHOULDER)));};
auto arena_refresh_input=[&](){
    if(!arena_enabled)return;
    if(arena_begin_pending&&++arena_begin_pending_frames>=2){
        arena_begin_pending=false;arena_begin_pending_frames=0;
        arena_begin();arena_focus_panel=-1;arena_release();
    }
    const bool focused=equipment_qa||duel_qa||(preview_pad.focused&&(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS));
    const bool modal=arena_ui_phase!=ArenaUiPhase::Fight||preview_screen!=1||show_graphics||destination_loading;
    if(modal!=arena_was_modal||!focused){arena_release();arena_was_modal=modal;}
    arena_gameplay_ready=!modal&&arena_gameplay_gate.observe(focused,arena_raw_held());
};
auto arena_load_progress=[&](){
    world_arena_progress::Progress staged;
    arena_progress_ready=player_profile_ready;
    if(arena_progress_ready&&!player_profile.arena_imported){
        const auto loaded=world_arena_progress::load("saves/arena.progress",staged,arena_progress_error);
        arena_progress_ready=loaded==world_arena_progress::Load::Missing||loaded==world_arena_progress::Load::Loaded;
        if(arena_progress_ready){auto next=player_profile;
            auto legacy=staged.inventory_present?staged.inventory:world_player_inventory::loan_kit(true);
            arena_progress_ready=world_player_profile::import(next,legacy,0,&staged,arena_progress_error)&&player_profile_commit(next,arena_progress_error);
        }
    }
    if(arena_progress_ready){arena_progress=player_profile.arena;player_inventory=player_profile.inventory;
        arena_weapon_draft=int(world_player_inventory::weapon_type(player_inventory.weapon));arena_total_draft=int(arena_progress.selection.total);arena_simultaneous_draft=int(arena_progress.selection.simultaneous);arena_opponents_draft=int(arena_progress.selection.opponents);arena_teammates_draft=int(arena_progress.selection.teammates);arena_difficulty_draft=int(world_arena_options::difficulty_index(arena_progress.selection.difficulty));}
    return arena_progress_ready;
};
auto arena_save_reward=[&](){
    if(!arena_reward_pending||!arena_progress_ready)return !arena_reward_pending;
    world_arena_progress::Progress staged;std::uint64_t award=0;
    const auto result=world_arena_progress::settle(arena_progress,arena_run_id,arena_run_config,
        arena_game.state()==world_arena_combat::State::Won&&arena_game.defeated()==arena_run_config.total,
        staged,award,arena_progress_error);
    if(result==world_arena_progress::Settlement::Rejected)return false;
    if(result==world_arena_progress::Settlement::Ready){auto next=player_profile;next.arena=staged;
        if(!player_profile_commit(next,arena_progress_error))return false;}
    arena_progress=staged;arena_saved_reward=world_arena_progress::payout(arena_run_config);arena_reward_pending=false;
    playtest_tell(world_currency::format(arena_saved_reward)+" saved to your shared purse.");return true;
};
auto arena_show_setup=[&](){
    // Leaving a live bout is a loss: allocating the next run consumes a new
    // receipt ID, and no transient combat result can change the saved purse.
    if(arena_reward_pending)return;
    equipment_cancel_pending();
    arena_game.cancel(fight);arena_target_lock.clear();arena_ui_phase=ArenaUiPhase::Setup;preview_screen=2;
    arena_focus_panel=-1;arena_release();
};
auto arena_sync=[&](){
    const auto enemy=arena_game.active_position();
    preview_arena_x=arena_origin_x+enemy.x-fight.enemy_x;preview_arena_z=arena_origin_z+enemy.z-fight.enemy_z;preview_arena_feet=float(arena_floor);
    playtest_combat_enemy_x=arena_origin_x+enemy.x;playtest_combat_enemy_z=arena_origin_z+enemy.z;
};
auto arena_weapon=[&](world_arena_combat::Weapon weapon){
    if(fight.player_phase!=TrainingPlayerPhase::Ready){playtest_tell("Finish the swing before switching weapons.");return;}
    if(!world_weapon_loadout::valid(weapon))return;
    if(unsigned(weapon)>=2&&!world_combat_motion_loadouts_ready(combat_motion)){playtest_tell("This project needs the weapon loadout asset bank.");return;}
    if(equipment_request_weapon&&equipment_request_weapon(weapon))return;
    if(inventory_select_weapon&&!inventory_select_weapon(weapon))return;
    arena_pad_focus.reset();arena_mouse_focus.reset();
    weapon_sheathed=false;arena_game.set_weapon(weapon);fight.weapon=weapon;if(weapon==world_arena_combat::Weapon::Bow)arena_target_lock.clear();fight.clear_buffer();fight.set_guard(false);sword_stick.reset();arena_mouse_draw=arena_mouse_aim=false;
    arena_bow_armed=false;arena_bow_held_before=false;
    const auto &profile=world_weapon_loadout::profile(weapon);
    playtest_tell(std::string(profile.name)+" equipped. "+profile.description);

};
auto arena_focus_press=[&](bool mouse){
    if(!arena_controls_ready()||arena_bow())return;
    auto &gesture=mouse?arena_mouse_focus:arena_pad_focus;
    if(!gesture.press())return;
    const bool was_locked=arena_target_lock.locked();
    const unsigned previous=arena_target_lock.target();
    const bool locked=arena_target_lock.cycle(arena_game,{cam.x-arena_origin_x,cam.y-1.75-arena_floor,cam.z-arena_origin_z},cam.yaw);
    ++arena_lock_toggles;
    if(locked&&was_locked&&arena_target_lock.target()!=previous)++arena_lock_cycles;
    playtest_tell(locked?"Focused: opponent "+std::to_string(arena_target_lock.target()+1)+". Tap RB / middle mouse: next. Hold: release.":"No approaching opponents in focus range.");
};
auto arena_focus_release=[&](bool mouse){(mouse?arena_mouse_focus:arena_pad_focus).reset();};
auto arena_release_bow=[&](){
    const auto forward=cam.forward(),right=cam.right();
    world_arena_combat::Vec3 origin{cam.x-arena_origin_x+forward.x*.65+right.x*.10,cam.y-arena_floor-.25,cam.z-arena_origin_z+forward.z*.65+right.z*.10};
    // Converge from the hands onto the surface under the displayed reticle.
    // A miss retains the screen ray; the enemy center is never an aim target.
    const auto ray=arena_aim_camera.forward();
    const world_arena_combat::Vec3 eye{arena_aim_camera.x-arena_origin_x,arena_aim_camera.y-arena_floor,arena_aim_camera.z-arena_origin_z},line{ray.x*100,ray.y*100,ray.z*100};
    double nearest=1.;
    auto surface=[&](double t){if(t>.0001&&t<nearest&&world_arena_combat::dot(eye+line*t-origin,line)>.01)nearest=t;};
    if(arena_game.state()==world_arena_combat::State::Fighting)for(const auto &actor:arena_game.challengers())
        if(unsigned(&actor-arena_game.challengers().data())<arena_game.config().total&&actor.state==world_arena_combat::ChallengerState::Active)surface(world_arena_combat::swept_capsule(eye,eye+line,actor.position,arena_game.opponent_body_radius(unsigned(&actor-arena_game.challengers().data())),arena_game.opponent_body_height(unsigned(&actor-arena_game.challengers().data())),0));
    if(line.y<-.00001)surface(-eye.y/line.y);
    const double edge=arena_game.bounds.half_extent;
    for(double sign:{-1.,1.}){if(std::abs(line.x)>.00001)surface((sign*edge-eye.x)/line.x);if(std::abs(line.z)>.00001)surface((sign*edge-eye.z)/line.z);}
    const auto target=eye+line*nearest;
    if(arena_game.release_draw(fight,origin,target-origin)){
        if(arena_shot_sound)arena_shot_sound();preview_pad.rumble(.12f,.22f,55,preview_settings.vibration);
    }
};
arena_begin=[&](){
    if(!arena_enabled||destination_loading||!arena_progress_ready||arena_reward_pending)return false;
    const world_arena_progress::Config config{unsigned(arena_total_draft),unsigned(arena_simultaneous_draft),world_arena_opponents::Kind(arena_opponents_draft),unsigned(arena_teammates_draft),world_arena_options::difficulty(unsigned(arena_difficulty_draft))};
    const bool orc_opponents=config.opponents==world_arena_opponents::Kind::Orc;
    // Load only at the between-match boundary, before allocating a run or
    // changing its body/arrow profile. A failed bank leaves the purse intact.
    if(orc_opponents&&!arena_select_orc_character(arena_progress_error))return false;
    if(!arena_player_roster_ready||arena_player_character_draft<0||unsigned(arena_player_character_draft)>=arena_player_roster.entries.size()){
        arena_progress_error="Choose an available knight before starting.";return false;
    }
    const std::string player_character=arena_player_roster.entries[unsigned(arena_player_character_draft)].id;
    if(!arena_apply_player_character||!arena_apply_player_character(player_character,arena_progress_error))return false;
    skeletal_hero.set_blood_wear(world_armor_wear::finish(unsigned(arena_armor_finish_draft),world_armor_wear::bank_seed(player_character.c_str())));
    if(!arena_prepare_appearances(config.opponents,config.total,config.teammates,player_character,arena_progress_error)){arena_show_setup();return false;}
    double opponent_radius=.15,opponent_height=1.;
    for(unsigned n=0;n<config.total+config.teammates;++n){
        const unsigned i=n<config.total?n:world_combat_capacity::opponents+n-config.total;
        const auto bank=world_combat_motion_opponents_stats(arena_opponent_motion(i));
        if(!world_arena_combat::Arena::valid_opponent_body_profile(bank.body_radius_metres,bank.body_height_metres)){
            arena_progress_error="An opponent's measured body dimensions are invalid.";return false;
        }
        opponent_radius=std::max(opponent_radius,bank.body_radius_metres);opponent_height=std::max(opponent_height,bank.body_height_metres);
    }
    world_arena_progress::Progress staged;std::uint64_t id=0;
    if(!world_arena_progress::allocate_available(arena_progress,config,arena_orc_assets_ready,staged,id,arena_progress_error))return false;
    {auto next=player_profile;next.arena=staged;if(!player_profile_commit(next,arena_progress_error))return false;}
    equipment_cancel_pending();weapon_sheathed=false;
    arena_progress=staged;arena_run_id=id;arena_run_config=config;arena_saved_reward=0;
    fight.difficulty=config.difficulty;
    if(!arena_game.start(fight,{config.total,config.simultaneous,config.opponents,config.teammates,config.difficulty},preview_settings.story_assist)){
        arena_progress_error="These opponent settings could not start a fight. No coins were awarded.";return false;
    }
    // Values were checked before allocating/committing this run. Apply them
    // before any simulation frame, including when switching back to Humans.
    arena_game.set_opponent_body_profile(opponent_radius,opponent_height);
    for(unsigned n=0;n<config.total+config.teammates;++n){const unsigned i=n<config.total?n:world_combat_capacity::opponents+n-config.total;const auto bank=world_combat_motion_opponents_stats(arena_opponent_motion(i));
        arena_game.set_opponent_body_profile(i,bank.body_radius_metres,bank.body_height_metres);
    }
    if(!world_armor_wear::save("arena_armor_finish.cfg",unsigned(arena_armor_finish_draft)))std::fprintf(stderr,"[Arena armor] Could not remember cosmetic finish; this fight retains the selected finish.\n");
    std::string character_save_error;
    if(!world_knight_roster::save_selection("arena_player_character.cfg",arena_player_roster,player_character,character_save_error)){
        arena_player_roster_error=character_save_error;
        std::fprintf(stderr,"[Arena player] %s\n",character_save_error.c_str());
    }else arena_player_roster_error.clear();
    if(!arena_compact_roster_enabled&&orc_opponents&&arena_orc_catalog_ready){
        const char *temporary="arena_orc_character.cfg.tmp";
        std::ofstream selection(temporary,std::ios::trunc);
        selection<<"VALWOOD_ORC_CHARACTER 1 "<<arena_orc_character_loaded<<" end\n";
        selection.close();
        if(!selection||std::rename(temporary,"arena_orc_character.cfg")!=0)
            std::fprintf(stderr,"[Arena Orcs] Character choice could not be remembered for the next launch\n");
    }
    world_combat_motion_reset_arena_opponents(combat_motion);
    world_combat_motion_reset_arena_opponents(arena_orc_motion);
    arena_each_appearance([](auto *motion){world_combat_motion_reset_arena_opponents(motion);});
    arena_ui_phase=ArenaUiPhase::Fight;arena_target_lock.clear();arena_focus_panel=-1;arena_weapon(world_arena_combat::Weapon(arena_weapon_draft));
    cam.x=arena_origin_x;cam.y=arena_floor+1.75;cam.z=arena_origin_z-5;cam.yaw=3.14159265359f;cam.pitch=-.04f;
    cam.fly=false;cam.grounded=true;cam.vy=0;third_person=true;avatar_initialized=false;preview_screen=1;
    arena_mouse_draw=arena_mouse_aim=arena_bow_held_before=false;arena_bow_armed=false;
    arena_previous_shots=0;arena_enemy_steps=0;arena_last_enemy=arena_game.active_position();
    combat_vfx_arrow_hits=0;combat_feedback.reset();
    combat_feedback_legacy_serial=combat_feedback_legacy_attack=0;combat_feedback_clock=-1;combat_feedback_was_paused=false;
    training_sounds.mute();preview_pad.stop_rumble();
    blood_effects.reset();combat_vfx.clear();neck_blood.reset();sever_blood.reset();
    arena_release();arena_sync();temporal_cut=true;++playtest_combat_starts;
    playtest_tell(std::to_string(config.total)+" "+world_arena_opponents::name(config.opponents)+", "+std::to_string(config.simultaneous)+" at once. Victory earns "+world_currency::format(world_arena_progress::payout(config))+".");
    std::printf("[Arena] Run %llu: %s / %u total / %u concurrent\n",(unsigned long long)arena_run_id,world_arena_opponents::name(arena_game.config().opponents),config.total,config.simultaneous);return true;
};
arena_key=[&](SDL_Keycode key){
    if(!arena_enabled)return false;
    if(key==SDLK_F10)return false;
    if(key==SDLK_ESCAPE){
        if(show_graphics)show_graphics=false;
        else if(arena_ui_phase==ArenaUiPhase::Fight)preview_screen=preview_screen==1?2:1;
        else if(arena_ui_phase==ArenaUiPhase::Results&&!arena_reward_pending)arena_show_setup();
        arena_focus_panel=-1;arena_release();return true;
    }
    if(key==SDLK_F5||key==SDLK_F8){playtest_tell("Arena coins save automatically. Fights begin as fresh runs; there is no mid-fight load.");return true;}
    // ImGui already received the event and owns all menu confirmation. Enter
    // must never bypass the focused Results/Setup button or restart a bout.
    if(show_graphics||arena_ui_phase!=ArenaUiPhase::Fight||preview_screen!=1)return true;
    if(key==SDLK_r){arena_show_setup();return true;}
    if(key==SDLK_e||key==SDLK_RETURN||key==SDLK_KP_ENTER)return true;
    if(!arena_controls_ready())return true;
    if(key==SDLK_1||key==SDLK_KP_1){arena_weapon(world_arena_combat::Weapon::Sword);return true;}
    if(key==SDLK_2||key==SDLK_KP_2){arena_weapon(world_arena_combat::Weapon::Bow);return true;}
    if(key>=SDLK_3&&key<=SDLK_5){arena_weapon(world_arena_combat::Weapon(int(key-SDLK_1)));return true;}
    return false;
};
auto arena_controls=[&](bool allowed){
    if(!arena_enabled)return;
    const bool held=arena_mouse_draw||(preview_pad.using_pad&&preview_pad.right_trigger>.4f);
    if(!allowed||arena_finished()||!arena_bow()){
        arena_game.cancel_draw();arena_bow_armed=false;arena_bow_held_before=held;return;
    }
    if(equipment_change.active()){arena_game.cancel_draw();arena_bow_armed=false;arena_bow_held_before=held;return;}
    if(!held)arena_bow_armed=true;
    if(held&&!arena_bow_held_before&&arena_bow_armed&&!equipment_change.active()){if(!equipment_request_draw()&&equipment_actions_ready())arena_game.begin_draw(fight);}
    if(!weapon_sheathed&&!held&&arena_bow_held_before&&arena_bow_armed)arena_release_bow();
    arena_bow_held_before=held;fight.set_guard(false);
};
auto arena_tick=[&](float delta,bool paused){
    if(!arena_enabled)return;
    world_arena_render_equipment(arena_renderer,world_player_inventory::appearance(player_inventory));
    if(!paused&&arena_controls_ready()){
        const bool pad_release=arena_pad_focus.update(delta),mouse_release=arena_mouse_focus.update(delta);
        if(pad_release||mouse_release){
            arena_target_lock.clear();++arena_lock_releases;
            playtest_tell("Focus released. Tap RB / middle mouse to focus an opponent.");
        }
    }
    const auto focus_player=world_arena_combat::Vec3{cam.x-arena_origin_x,cam.y-1.75-arena_floor,cam.z-arena_origin_z};
    const unsigned previous_target=arena_target_lock.target();
    arena_target_lock.maintain(arena_game,focus_player);
    if(previous_target<world_combat_capacity::opponents&&arena_target_lock.target()<world_combat_capacity::opponents&&previous_target!=arena_target_lock.target())++arena_lock_retargets;
    const float model_yaw=3.141592653589793f-cam.yaw;
    if(arena_ui_phase==ArenaUiPhase::Fight)arena_game.update(delta,fight,{cam.x-arena_origin_x,cam.y-1.75-arena_floor,cam.z-arena_origin_z},model_yaw,paused||!arena_controls_ready(),arena_target_lock.target());
    else if(arena_ui_phase==ArenaUiPhase::Results&&arena_game.state()==world_arena_combat::State::Won)
        arena_game.update(delta,fight,{cam.x-arena_origin_x,cam.y-1.75-arena_floor,cam.z-arena_origin_z},model_yaw,arena_results_paused());
    const unsigned after_tick_target=arena_target_lock.target();
    arena_target_lock.maintain(arena_game,focus_player);
    if(after_tick_target<world_combat_capacity::opponents&&arena_target_lock.target()<world_combat_capacity::opponents&&after_tick_target!=arena_target_lock.target())++arena_lock_retargets;
    if(arena_ui_phase==ArenaUiPhase::Fight&&arena_finished()){
        arena_ui_phase=ArenaUiPhase::Results;preview_screen=2;arena_focus_panel=-1;arena_release();
        arena_reward_pending=arena_game.state()==world_arena_combat::State::Won;
        if(arena_reward_pending)arena_save_reward();
    }
    arena_sync();
    const auto enemy=arena_game.active_position();
    const bool moving=std::hypot(enemy.x-arena_last_enemy.x,enemy.z-arena_last_enemy.z)>.0001;
    if(moving)++arena_enemy_steps;arena_last_enemy=enemy;
    std::array<WorldCombatCrowdInstance,world_combat_capacity::npcs> crowd={};unsigned count=0;
    for(unsigned id=0;id<arena_game.challengers().size();++id){
        const auto &actor=arena_game.challengers()[id];auto &motion=arena_motion_actors[id];motion={};
        const bool active=actor.state==world_arena_combat::ChallengerState::Active||actor.state==world_arena_combat::ChallengerState::Entering;
        motion.combat=arena_game.view(fight,id);motion.x=arena_origin_x+actor.position.x;motion.y=arena_floor;motion.z=arena_origin_z+actor.position.z;
        motion.visible=active&&arena_ui_phase!=ArenaUiPhase::Setup;
        const unsigned target=arena_game.target(id);motion.facing_target_valid=target<=world_arena_combat::Arena::player_target;
        motion.facing_target_x=target<world_combat_capacity::npcs?arena_origin_x+arena_game.challengers()[target].position.x:cam.x;
        motion.facing_target_z=target<world_combat_capacity::npcs?arena_origin_z+arena_game.challengers()[target].position.z:cam.z;motion.walking=actor.moving&&!paused&&arena_controls_ready();
        if(active||actor.state==world_arena_combat::ChallengerState::Unused)continue;
        auto &v=crowd[count++];v.x=arena_origin_x+actor.position.x;v.y=arena_floor;v.z=arena_origin_z+actor.position.z;v.yielded=actor.state==world_arena_combat::ChallengerState::Defeated;
        v.yaw=v.yielded&&character_polish_enabled?actor.defeat_yaw:3.14159265359f;
        v.decapitated=motion.combat.enemy_decapitated;
        v.severed_parts=motion.combat.enemy_severed_parts;v.weapon_hand=motion.combat.enemy_weapon_hand;v.actor_id=id+1;
        v.defeat_phase=v.yielded&&character_polish_enabled?float(std::clamp(actor.defeat_age/world_combat_motion::death_duration,0.,1.)):0;
    }
    if(arena_compact_roster_enabled)arena_each_appearance([&](auto *motion){world_combat_motion_crowd(motion,crowd.data(),count);});
    else world_combat_motion_crowd(arena_opponent_motion(),crowd.data(),count);
};
auto arena_prepare_render=[&](const Camera &eye,float animation_dt,bool animation_cut){
    if(!arena_enabled||!arena_renderer)return;
    arena_aim_camera=eye;
    arena_camera_heading_error=std::abs(std::remainder(double(eye.yaw)-cam.yaw,6.283185307179586));
    arena_camera_pitch_error=std::abs(double(eye.pitch)-cam.pitch);
    arena_camera_lateral_error=std::abs((cam.x-eye.x)*std::cos(double(eye.yaw))+(cam.z-eye.z)*std::sin(double(eye.yaw)));
    arena_camera_player_lateral=std::abs((cam.x-eye.x)*std::cos(double(cam.yaw))+(cam.z-eye.z)*std::sin(double(cam.yaw)));
    arena_camera_focus_error=0;
    const bool framed=third_person&&!cam.fly&&combat_shoulder_active&&!arena_bow();
    double goal_yaw=cam.yaw,goal_pitch=cam.pitch;
    if(framed){
        const double dx=combat_shoulder_target_x-eye.x,dz=combat_shoulder_target_z-eye.z;
        const double yaw=std::atan2(dx,-dz),pitch=std::atan2(combat_shoulder_target_y-eye.y,std::hypot(dx,dz));
        goal_yaw=yaw;goal_pitch=pitch;
        arena_camera_focus_error=std::max(std::abs(std::remainder(double(eye.yaw)-yaw,6.283185307179586)),std::abs(double(eye.pitch)-pitch));
        ++arena_camera_shoulder_frames;arena_camera_max_focus_error=std::max(arena_camera_max_focus_error,arena_camera_focus_error);
    }
    const double yaw_error=std::abs(std::remainder(double(eye.yaw)-goal_yaw,6.283185307179586));
    const double pitch_error=std::abs(double(eye.pitch)-goal_pitch);
    arena_camera_goal_error=std::max(yaw_error,pitch_error);
    arena_camera_goal_l1_error=yaw_error+pitch_error;
    arena_camera_focus_transition=combat_focus_applied&&combat_focus_frame.transitioning;
    // Retain the raw heading/pitch/lateral deltas above. A render-only shoulder
    // and its brief orientation transition differ from logical aim. Validate
    // actual world displacement in the player's frame, not the rotating view.
    // Only the finite transition gets an error budget; settled lock retains the
    // original exact-centering invariant and free view returns to exact aim.
    arena_camera_visual_valid=combat_shoulder_safe&&combat_shoulder_offset<=world_combat_shoulder_maximum+.00001f&&
        arena_camera_player_lateral<=world_combat_shoulder_maximum+.00001&&
        world_combat_focus_frame_valid(combat_focus_frame,eye.yaw,eye.pitch,goal_yaw,goal_pitch,framed);
    arena_camera_unsafe_frames+=!arena_camera_visual_valid;
    if(third_person&&!cam.fly){
        if(arena_camera_focus_transition){++arena_camera_transition_frames;arena_camera_max_transition_error=std::max(arena_camera_max_transition_error,arena_camera_goal_error);arena_camera_max_transition_age=std::max(arena_camera_max_transition_age,combat_focus_frame.elapsed_seconds);}
        else{++arena_camera_steady_frames;if(framed)arena_camera_max_steady_focus_error=std::max(arena_camera_max_steady_focus_error,arena_camera_focus_error);}
        ++arena_camera_frames;arena_camera_many_frames+=arena_game.active_count()>1;arena_camera_locked_frames+=arena_target_lock.locked();
        arena_camera_max_heading_error=std::max(arena_camera_max_heading_error,arena_camera_heading_error);
        arena_camera_max_pitch_error=std::max(arena_camera_max_pitch_error,arena_camera_pitch_error);
        arena_camera_max_lateral_error=std::max(arena_camera_max_lateral_error,arena_camera_lateral_error);
    }
    WorldArenaBowPose bow;bow.visible=arena_bow()&&!weapon_sheathed;bow.first_person=!third_person;bow.x=third_person?cam.x:eye.x;bow.y=third_person?cam.y-1.75:eye.y;bow.z=third_person?cam.z:eye.z;bow.yaw=cam.yaw;bow.pitch=cam.pitch;bow.draw=arena_bow_visual_draw=arena_bow_presentation.update(float(arena_game.draw_fraction()),arena_game.drawing(),bow.visible,animation_dt,animation_cut);
    std::array<WorldArenaRenderArrow,24> arrows={};unsigned count=0;
    for(const auto &arrow:arena_game.arrows())if(arrow.state!=world_arena_combat::ArrowState::Unused){
        auto &v=arrows[count++];v.visible=true;const double speed=std::max(.01,std::sqrt(world_arena_combat::dot(arrow.velocity,arrow.velocity)));
        v.tip[0]=arena_origin_x+arrow.position.x;v.tip[1]=arena_floor+arrow.position.y;v.tip[2]=arena_origin_z+arrow.position.z;
        v.base[0]=v.tip[0]-arrow.velocity.x/speed*.65;v.base[1]=v.tip[1]-arrow.velocity.y/speed*.65;v.base[2]=v.tip[2]-arrow.velocity.z/speed*.65;
    }
    if(arena_player_bank&&!world_combat_motion_bow_pose(arena_player_bank,bow)){std::fprintf(stderr,"[Arena player] Dedicated bow pose failed\n");launch->exit_code=1;running=false;}
    if(!world_arena_render_update(arena_renderer,bow,arrows.data(),count)){launch->exit_code=1;running=false;}
};
arena_draw=[&](const Camera &eye,bool loading){
    arena_focus_marker_visible=false;
    if(loading||show_graphics||inventory_open)return;
    const auto &io=ImGui::GetIO();const float w=io.DisplaySize.x,h=io.DisplaySize.y,u=std::clamp(std::min(w/1600.f,h/900.f),.65f,1.4f);
    auto *dl=ImGui::GetForegroundDrawList();const ImU32 gold=IM_COL32(231,195,127,255),white=IM_COL32(242,239,226,255);
    auto text=[&](float x,float y,float size,const std::string &s,ImU32 color){dl->AddText(ImGui::GetFont(),size*u,{x*u,y*u},color,s.c_str());};
    if(arena_ui_phase==ArenaUiPhase::Fight&&preview_screen==1){
    dl->AddRectFilled({22*u,20*u},{590*u,163*u},IM_COL32(9,18,21,220),6*u);
    text(40,32,13,"VALWOOD  /  COMBAT PRACTICE",gold);
    text(40,56,23,"ARENA CHALLENGE",white);
    text(40,91,15,"Defeated "+std::to_string(arena_game.defeated())+" / "+std::to_string(arena_game.config().total)+"   |   Active "+std::to_string(arena_game.active_count())+"   |   "+(std::to_string(unsigned(arena_game.weapon())+1)+"  "+world_weapon_loadout::profile(arena_game.weapon()).name),gold);
    std::string state="Next challenger approaching";
    if(arena_ui_phase==ArenaUiPhase::Setup)state="Choose the challenge before entering the ring.";
    if(arena_game.state()==world_arena_combat::State::Fighting)state=fight.state==TrainingState::Countdown?"Ready in "+std::to_string(int(std::ceil(fight.phase_time))):std::string("Fight  |  Focus guard: ")+world_training_direction_name(fight.enemy_guard);
    if(arena_game.state()==world_arena_combat::State::Intermission)state=character_polish_enabled?"Opponent defeated. Next challenger entering shortly.":"Opponent yielded. Next challenger entering shortly.";
    if(arena_game.state()==world_arena_combat::State::Won)state="All "+std::to_string(arena_run_config.total)+" opponents defeated.";
    if(arena_game.state()==world_arena_combat::State::Lost)state="You yielded. No coins awarded.";
    if(arena_compact_roster_enabled&&!arena_enemy_appearances.empty()&&arena_ui_phase!=ArenaUiPhase::Setup){
        const unsigned id=unsigned(arena_game.current_index());
        if(id<arena_enemy_appearances.size())state=arena_enemy_appearances[id]+"  |  "+state;
    }else if(arena_game.config().opponents==world_arena_opponents::Kind::Orc&&arena_orc_catalog_ready&&arena_ui_phase!=ArenaUiPhase::Setup){
        const int index=arena_orc_roster.index(arena_orc_character_loaded);
        if(index>=0)state=arena_orc_roster.entries[unsigned(index)].display_name+"  |  "+state;
    }
    text(40,119,13,state,white);
    if(arena_ui_phase==ArenaUiPhase::Fight)text(40,145,12,std::string(world_arena_options::difficulty_name(arena_run_config.difficulty))+"  |  Teammates "+std::to_string(arena_game.teammate_alive())+" / "+std::to_string(arena_run_config.teammates),IM_COL32(146,224,169,255));
    if(arena_ui_phase==ArenaUiPhase::Fight)text(40,190,12,preview_pad.using_pad?(weapon_sheathed?"D-pad Down: draw weapon":"D-pad Down: put weapon away"):(weapon_sheathed?"Z: draw weapon":"Z: put weapon away"),white);
    if(arena_ui_phase==ArenaUiPhase::Fight&&!arena_bow()){
        text(40,166,14,arena_target_lock.locked()?"LOCKED: OPPONENT "+std::to_string(arena_target_lock.target()+1)+"  |  TAP RB / middle: next  |  HOLD: release":"PLAYER CAMERA  |  TAP RB / middle mouse: focus",gold);
        if(arena_target_lock.locked()){
            const auto &actor=arena_game.challengers()[arena_target_lock.target()];
            const double dx=arena_origin_x+actor.position.x-eye.x,dy=arena_floor+actor.position.y+2.1-eye.y,dz=arena_origin_z+actor.position.z-eye.z;
            const auto f=eye.forward(),r=eye.right(),up=norm(cross(r,f));
            const double depth=dx*f.x+dy*f.y+dz*f.z,lens=std::tan(eye.fov*.5);
            if(depth>.1&&lens>.01){
                const float x=float((.5+(dx*r.x+dy*r.y+dz*r.z)/(2*depth*lens*eye.aspect))*w);
                const float y=float((.5-(dx*up.x+dy*up.y+dz*up.z)/(2*depth*lens))*h);
                if(x>0&&x<w&&y>0&&y<h){arena_focus_marker_visible=true;dl->AddCircle({x,y},10*u,gold,20,2*u);dl->AddCircleFilled({x,y},2*u,gold);}
            }
        }
    }
    if(arena_ui_phase==ArenaUiPhase::Fight){
        const auto f=eye.forward(),r=eye.right(),up=norm(cross(r,f));const double lens=std::tan(eye.fov*.5);
        for(unsigned n=0;n<arena_run_config.teammates;++n){const unsigned id=world_combat_capacity::opponents+n;
            const auto &actor=arena_game.challengers()[id];const auto *ally=arena_game.opponent(id);
            if(!ally||actor.state!=world_arena_combat::ChallengerState::Active)continue;
            const double dx=arena_origin_x+actor.position.x-eye.x,dy=arena_floor+actor.position.y+2.15-eye.y,dz=arena_origin_z+actor.position.z-eye.z;
            const double depth=dx*f.x+dy*f.y+dz*f.z;
            if(depth<=.1||depth>35||lens<=.01)continue;
            const float x=float((.5+(dx*r.x+dy*r.y+dz*r.z)/(2*depth*lens*eye.aspect))*w),y=float((.5-(dx*up.x+dy*up.y+dz*up.z)/(2*depth*lens))*h);
            if(x<25*u||x>w-25*u||y<25*u||y>h-25*u)continue;
            const auto green=IM_COL32(128,233,155,255);const std::string label="ALLY "+std::to_string(n+1);
            dl->AddText(ImGui::GetFont(),11*u,{x-20*u,y-17*u},green,label.c_str());
            dl->AddRectFilled({x-20*u,y},{x+20*u,y+4*u},IM_COL32(12,23,18,220));
            dl->AddRectFilled({x-20*u,y},{x+(-20+40*std::clamp(ally->health/std::max(1.f,ally->max_health),0.f,1.f))*u,y+4*u},green);
        }
    }
    const float bottom=h/u;
    dl->AddRectFilled({22*u,h-153*u},{425*u,h-55*u},IM_COL32(9,18,21,220),6*u);
    text(40,bottom-140,14,"HEALTH "+std::to_string(int(fight.health))+"   STAMINA "+std::to_string(int(fight.stamina)),white);
    auto bar=[&](float y,float value,ImU32 color){dl->AddRectFilled({40*u,h-y*u},{398*u,h-(y-8)*u},IM_COL32(35,42,43,255));dl->AddRectFilled({40*u,h-y*u},{(40+358*std::clamp(value,0.f,1.f))*u,h-(y-8)*u},color);};
    bar(113,fight.health/100,IM_COL32(194,74,69,255));bar(98,fight.stamina/100,IM_COL32(99,177,126,255));
    text(40,bottom-79,12,"COMBO "+std::to_string(fight.combo_count)+" / 3  |  Each swing uses stamina",gold);
    unsigned visible_opponents=0;
    std::vector<unsigned> arena_hud_opponents;for(unsigned id=0;id<arena_game.config().total;++id)arena_hud_opponents.push_back(id);
    std::stable_sort(arena_hud_opponents.begin(),arena_hud_opponents.end(),[&](unsigned a,unsigned b){
        if(a==b)return false;
        if(arena_target_lock.locked()&&(a==arena_target_lock.target()||b==arena_target_lock.target()))return a==arena_target_lock.target();
        const auto &p=arena_game.challengers()[a].position,&q=arena_game.challengers()[b].position;
        return std::hypot(arena_origin_x+p.x-cam.x,arena_origin_z+p.z-cam.z)<std::hypot(arena_origin_x+q.x-cam.x,arena_origin_z+q.z-cam.z);
    });
    for(unsigned id:arena_hud_opponents){
        const auto &actor=arena_game.challengers()[id];if(actor.state!=world_arena_combat::ChallengerState::Active&&actor.state!=world_arena_combat::ChallengerState::Entering)continue;
        const auto *opponent=arena_game.opponent(id);if(!opponent)continue;
        if(visible_opponents>=std::min(6u,std::max(1u,unsigned(std::max(0.f,(h/u-230.f)/46.f)))))continue;
        const float right=w/u-290,y=26+visible_opponents++*46;
        dl->AddRectFilled({(right-10)*u,(y-5)*u},{w-22*u,(y+36)*u},IM_COL32(9,18,21,220),4*u);
        const bool incoming=actor.state==world_arena_combat::ChallengerState::Active&&opponent->phase==TrainingEnemyPhase::Windup;
        std::string label="Opponent "+std::to_string(id+1)+(actor.state==world_arena_combat::ChallengerState::Entering?"  entering":incoming?std::string("  ")+world_training_direction_name(opponent->direction):std::string("  guard ")+world_training_direction_name(opponent->guard));
        if(arena_target_lock.locked()&&arena_target_lock.target()==id)label="LOCKED  "+label;
        text(right,y,12,label,incoming||arena_target_lock.target()==id?gold:white);
        dl->AddRectFilled({right*u,(y+22)*u},{(right+240)*u,(y+28)*u},IM_COL32(35,42,43,255));
        dl->AddRectFilled({right*u,(y+22)*u},{(right+240*std::clamp(opponent->health/std::max(1.f,opponent->max_health),0.f,1.f))*u,(y+28)*u},gold);
    }
    if(arena_bow()&&!weapon_sheathed){
        const float cx=w*.5f,cy=h*.5f,r=(10-6*float(arena_game.draw_fraction()))*u;
        dl->AddLine({cx-r-5*u,cy},{cx-r,cy},white,1.5f*u);dl->AddLine({cx+r,cy},{cx+r+5*u,cy},white,1.5f*u);dl->AddLine({cx,cy-r-5*u},{cx,cy-r},white,1.5f*u);dl->AddCircle({cx,cy},2*u,gold);
        text(w/u*.5f-100,bottom-115,14,"DRAW "+std::to_string(int(arena_game.draw_fraction()*100))+"%  /  HITS "+std::to_string(arena_game.arrow_hits()),gold);
    }else if(fight.guard)text(w/u*.5f-180,bottom-115,16,std::string("YOUR BLOCK: ")+world_training_direction_name(fight.selected),gold);
    if(fight.enemy_phase==TrainingEnemyPhase::Windup&&fight.state==TrainingState::Fighting)text(w/u*.5f-180,bottom-146,17,std::string("INCOMING: ")+world_training_direction_name(fight.enemy_direction),gold);
    if(fight.feedback_time>0)text(w/u*.5f-180,bottom-87,13,fight.feedback,white);
    text(30,bottom-55,14,world_weapon_loadout::profile(arena_game.weapon()).name,white);
    text(30,bottom-35,13,arena_bow()?"1 Sword  2 Bow  3 Shield  4 Axe  5 Staff  |  D-pad: cycle  |  Hold/release RT or LMB: shoot  |  LT/RMB: aim  |  Esc / Start: pause":"1 Sword  2 Bow  3 Shield  4 Axe  5 Staff  |  D-pad: cycle  |  RS: swing  |  LT + RS: block  |  Hold LB / Q: look  |  B: dodge  |  RS click / V: camera",white);
    }
    if(preview_screen!=1||arena_ui_phase!=ArenaUiPhase::Fight){
        ImGui::GetBackgroundDrawList()->AddRectFilled({0,0},{w,h},IM_COL32(4,10,13,145));
        const int panel=int(arena_ui_phase)*10+(show_graphics?1:0);
        const bool ready=arena_menu_gate.observe(arena_roster_qa||blade_clash_qa||(preview_pad.focused&&(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)),arena_raw_held());
        const bool focus=arena_focus_panel!=panel;
        const bool character_setup=arena_ui_phase==ArenaUiPhase::Setup;
        ImGui::SetNextWindowPos({w*.5f,h*.5f},ImGuiCond_Always,{.5f,.5f});
        ImGui::SetNextWindowSize(character_setup?ImVec2{std::min(w-24*u,940*u),std::min(h-24*u,780*u)}:ImVec2{520*u,0},ImGuiCond_Always);
        if(focus)ImGui::SetNextWindowFocus();
        ImGui::Begin("Arena practice###arena-practice-menu",nullptr,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings|(character_setup?0:ImGuiWindowFlags_AlwaysAutoResize));
        bool default_focus=focus&&ready;if(ready)arena_focus_panel=panel;
        auto button=[&](const char *label){const bool pressed=ImGui::Button(label,{-1,35*u});world_controller::default_focus(default_focus);return pressed;};
        ImGui::TextUnformatted(arena_ui_phase==ArenaUiPhase::Setup?"CHOOSE YOUR ARENA CHALLENGE":arena_ui_phase==ArenaUiPhase::Results?(arena_game.state()==world_arena_combat::State::Won?"VICTORY":"YOU YIELD"):"PAUSED");
        if(arena_progress_ready){ImGui::Text("Shared purse: %s",world_currency::format(player_profile_balance()).c_str());ImGui::TextDisabled("100 copper = 1 silver  |  100 silver = 1 gold");}
        else ImGui::TextUnformatted("Arena purse unavailable");
        ImGui::BeginDisabled(!ready||arena_begin_pending);
        if(!arena_progress_ready){
            ImGui::TextWrapped("Your existing purse could not be loaded. It has not been overwritten.");
            if(button("Retry loading arena purse")){arena_load_progress();arena_focus_panel=-1;arena_release();}
        }else if(arena_ui_phase==ArenaUiPhase::Setup){
            const auto total=unsigned(arena_total_draft);
            const auto selected_opponents=world_arena_opponents::Kind(arena_opponents_draft);
            auto selected_knight=[&]()->const world_knight_roster::Entry*{
                return arena_player_roster_ready&&arena_player_character_draft>=0&&unsigned(arena_player_character_draft)<arena_player_roster.entries.size()?&arena_player_roster.entries[unsigned(arena_player_character_draft)]:nullptr;
            };
            const auto *knight=selected_knight();
            const char *opponent_name=selected_opponents==world_arena_opponents::Kind::Orc?"Orcs":"Knights";
            ImGui::TextWrapped("%s  |  %s  |  %u %s  |  %d at once  |  %d teammates  |  Victory: +%u copper",knight?knight->display_name.c_str():"Choose your knight",world_arena_options::difficulty_name(world_arena_options::difficulty(unsigned(arena_difficulty_draft))),total,opponent_name,arena_simultaneous_draft,arena_teammates_draft,10*total);
            const bool start_available=knight&&world_arena_opponents::available(selected_opponents,arena_orc_catalog_ready||arena_orc_assets_ready);
            ImGui::BeginDisabled(!start_available);
            const bool start_pressed=ImGui::Button("Start fight###arena-start",{-1,35*u});
            if(start_available)world_controller::default_focus(default_focus);
            if(start_pressed){arena_begin_pending=true;arena_begin_pending_frames=0;}
            ImGui::EndDisabled();
            // Keep the confirm button and footer visible while either setup
            // column scrolls. Merely browsing portraits never reloads rigs.
            const float footer_space=2*ImGui::GetFrameHeightWithSpacing()+4*ImGui::GetTextLineHeightWithSpacing()+24*u;
            const float body_height=std::max(170*u,ImGui::GetContentRegionAvail().y-footer_space);
            if(ImGui::BeginTable("arena-character-setup",2,ImGuiTableFlags_SizingStretchProp)){
                ImGui::TableSetupColumn("Challenge",ImGuiTableColumnFlags_WidthStretch,1.15f);
                ImGui::TableSetupColumn("Your knight",ImGuiTableColumnFlags_WidthStretch,1.f);
                ImGui::TableNextColumn();
                ImGui::BeginChild("arena-character-controls",{0,body_height},false);
                ImGui::TextUnformatted("YOUR KNIGHT");
                ImGui::SetNextItemWidth(-1);
                if(ImGui::Combo("###arena-player-character",&arena_player_character_draft,
                    [](void *data,int index)->const char*{
                        const auto &entries=static_cast<world_knight_roster::Roster*>(data)->entries;
                        return index>=0&&unsigned(index)<entries.size()?entries[unsigned(index)].display_name.c_str():nullptr;
                    },&arena_player_roster,int(arena_player_roster.entries.size()),8))arena_progress_error.clear();
                if(!knight&&arena_player_roster_ready)world_controller::default_focus(default_focus);
                ImGui::TextDisabled("%u prebuilt appearances",unsigned(arena_player_roster.entries.size()));
                const char *armor_finishes[]={"Clean armor","Blood-marked","Battle-bloodied"};
                ImGui::TextUnformatted("Armor finish in battle");ImGui::SetNextItemWidth(-1);
                ImGui::Combo("###arena-armor-finish",&arena_armor_finish_draft,armor_finishes,3);
                ImGui::TextWrapped("Knight opponents have a mix of clean and blood-marked armor.");
                ImGui::Separator();ImGui::TextUnformatted("YOUR CHALLENGE");
                const char *difficulty_names[]={"Easy","Medium","Hard"};
                ImGui::TextUnformatted("Difficulty");ImGui::SetNextItemWidth(-1);
                ImGui::Combo("###arena-difficulty",&arena_difficulty_draft,difficulty_names,3);
                ImGui::TextWrapped("%s",arena_difficulty_draft==0?"Relaxed defenses. Practice your swings and timing.":arena_difficulty_draft==1?"More blocking. Vary your angles and counter after their swings.":"Tight defenses. Change angles, time your openings and punish committed attacks.");
                ImGui::TextUnformatted("Total opponents (1-24)");ImGui::SetNextItemWidth(-1);
                if(ImGui::SliderInt("###arena-total",&arena_total_draft,1,int(world_combat_capacity::opponents),"%d",ImGuiSliderFlags_AlwaysClamp))arena_simultaneous_draft=std::min(arena_simultaneous_draft,arena_total_draft);
                ImGui::TextUnformatted("At the same time");ImGui::SetNextItemWidth(-1);
                ImGui::SliderInt("###arena-simultaneous",&arena_simultaneous_draft,1,arena_total_draft,"%d",ImGuiSliderFlags_AlwaysClamp);
                ImGui::TextUnformatted("Your teammates (0-6)");ImGui::SetNextItemWidth(-1);
                ImGui::SliderInt("###arena-teammates",&arena_teammates_draft,0,int(world_combat_capacity::teammates),"%d",ImGuiSliderFlags_AlwaysClamp);
                ImGui::TextWrapped("Allied knights fight beside you. Friendly fire is off; rewards count opponents only.");
                if(arena_total_draft+arena_teammates_draft>8)ImGui::TextDisabled("Large battles use balanced NPC textures.");
                const char *weapon_names[]={"Two-handed sword","Bow","Sword & shield","Two-handed axe","Quarterstaff"};
                ImGui::TextUnformatted("Starting weapon");ImGui::SetNextItemWidth(-1);
                ImGui::Combo("###arena-weapon",&arena_weapon_draft,weapon_names,5);
                ImGui::TextWrapped("%s",world_weapon_loadout::profile(world_arena_combat::Weapon(arena_weapon_draft)).description);
                const char *opponent_names[]={"Knights","Orcs"};
                ImGui::TextUnformatted("Opponents");ImGui::SetNextItemWidth(-1);
                if(ImGui::Combo("###arena-opponents",&arena_opponents_draft,opponent_names,2))arena_progress_error.clear();
                if(!start_available)world_controller::default_focus(default_focus);
                ImGui::TextWrapped("Each opponent receives a random appearance from the selected roster.");
                if(!arena_orc_assets_ready&&!arena_orc_catalog_ready)ImGui::TextWrapped("Orc characters are unavailable. Knight fights are ready.");
                if(selected_opponents==world_arena_opponents::Kind::Orc&&arena_orc_assets_ready&&!arena_orc_original_textures_ready)
                    ImGui::TextWrapped("Orc preview materials; original texture maps pending.");
                ImGui::TextWrapped("Defeat every selected opponent to earn 10 copper each. Ending a run or losing earns no coins.");
                ImGui::EndChild();
                ImGui::TableNextColumn();
                ImGui::BeginChild("arena-character-preview",{0,body_height},false);
                if(const auto *preview=selected_knight()){
                    ImGui::TextWrapped("%s",preview->display_name.c_str());
                    const float portrait_height=std::max(64*u,std::min(body_height-4*ImGui::GetTextLineHeightWithSpacing(),ImGui::GetContentRegionAvail().x/0.8f));
                    const unsigned portrait=arena_player_portrait_texture?arena_player_portrait_texture(preview->id):0;
                    if(portrait)ImGui::Image((ImTextureID)(intptr_t)portrait,{portrait_height*.8f,portrait_height});
                    else {ImGui::Dummy({portrait_height*.8f,portrait_height});ImGui::TextDisabled("Portrait unavailable");}
                    if(!preview->description.empty())ImGui::TextWrapped("%s",preview->description.c_str());
                    if(preview->id==arena_player_character_loaded)ImGui::TextDisabled("Current knight");
                }else ImGui::TextWrapped("Your knight roster is unavailable.");
                if(!arena_player_roster_error.empty())ImGui::TextWrapped("%s",arena_player_roster_error.c_str());
                ImGui::EndChild();ImGui::EndTable();
            }
        }else if(arena_ui_phase==ArenaUiPhase::Results){
            ImGui::Text("%s  |  Defeated %u / %u  |  Arrow hits %u",world_arena_opponents::name(arena_run_config.opponents),arena_game.defeated(),arena_run_config.total,arena_game.arrow_hits());
            if(arena_reward_pending){
                ImGui::TextWrapped("Victory earned %s. Saving failed; your saved purse has not changed.",world_currency::format(world_arena_progress::payout(arena_run_config)).c_str());
                if(button("Retry saving victory coins"))arena_save_reward();
            }else{
                if(arena_saved_reward)ImGui::Text("+%s saved",world_currency::format(arena_saved_reward).c_str());
                else ImGui::TextUnformatted("No reward for this run. Try again when ready.");
                if(button("Fight again###arena-repeat"))arena_begin();
                if(button("Change opponents###arena-change"))arena_show_setup();
            }
        }else{
            for(unsigned i=0;i<5;++i)if(button(world_weapon_loadout::profiles[i].name))arena_weapon(world_arena_combat::Weapon(i));
            if(button("Resume###arena-resume")){preview_screen=1;arena_focus_panel=-1;arena_release();}
            if(button("End run and change opponents"))arena_show_setup();
        }
        if(character_setup){
            const float count=inventory_menu_open?3.f:2.f;
            const float width=(ImGui::GetContentRegionAvail().x-(count-1)*ImGui::GetStyle().ItemSpacing.x)/count;
            if(inventory_menu_open){if(ImGui::Button("Inventory  /  I or View",{width,35*u}))inventory_menu_open();ImGui::SameLine();}
            if(ImGui::Button("Graphics settings",{width,35*u})){show_graphics=true;graphics_draft=graphics;arena_focus_panel=-1;arena_release();}ImGui::SameLine();
            if(ImGui::Button("Exit arena",{width,35*u}))running=false;
        }else{
            if(inventory_menu_open&&button("Inventory  /  I or View"))inventory_menu_open();
            if(button("Graphics settings")){show_graphics=true;graphics_draft=graphics;arena_focus_panel=-1;arena_release();}
            if(button("Exit arena"))running=false;
        }
        ImGui::EndDisabled();
        if(arena_begin_pending)ImGui::TextWrapped("Preparing your arena challenge and character animations...");
        if(!ready)ImGui::TextUnformatted("Release the controls to continue.");
        if(!arena_progress_error.empty())ImGui::TextWrapped("%s",arena_progress_error.c_str());
        ImGui::Separator();
        if(character_setup){
            ImGui::BeginDisabled(!ready||arena_begin_pending);
            if(ImGui::Button("Controls & combat help",{-1,ImGui::GetFrameHeight()}))ImGui::OpenPopup("arena-character-help");
            ImGui::EndDisabled();
            if(ImGui::IsPopupOpen("arena-character-help"))ImGui::SetNextWindowSize({std::min(w-32*u,620*u),0},ImGuiCond_Appearing);
        }
        if(!character_setup||ImGui::BeginPopup("arena-character-help")){
            if(character_setup){
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+std::min(w-48*u,580*u));
                ImGui::TextWrapped("Choose your knight from the appearance list before starting.");
                ImGui::TextWrapped("Change counts: tap A, press left / right, then tap A again. Keyboard: Enter, type a number, Enter. Mouse: drag the slider.");
            }
            ImGui::TextWrapped("Left stick / D-pad or arrows: navigate. A / Enter or mouse: select. Sword combo: flick, center, then flick during recovery; up to 3 strikes (stamina depends on weapon). LT + stick blocks and cancels a queued cut. Tap RB / middle mouse: focus / next opponent; hold .45 seconds: release. Hold LB / Q to look; center before swinging. LB + RB / T + left mouse: thrust. Bow: hold / release RT. 1-5 or D-pad left / right: cycle weapons. B / Space: dodge. V / right-stick click: camera.");
            ImGui::TextWrapped("Sharp blades can sever exposed body parts. An enemy who loses a hand may keep fighting with the other. Head cuts and vital thrusts are lethal.");
            if(character_setup){ImGui::PopTextWrapPos();ImGui::EndPopup();}
        }
        ImGui::End();
    }
};
if(arena_enabled){
    // Initial placement was already loaded by the private playtest route.
    arena_game.bounds.half_extent=11.8;
    arena_load_progress();arena_game.cancel(fight);arena_sync();arena_last_enemy=arena_game.active_position();
    preview_screen=2;arena_release();
    std::printf("[Arena] Setup ready: %d %s / %d concurrent, Orc assets %s, purse %llu; 24m floor %.2f %.2f %.2f\n",arena_total_draft,world_arena_opponents::name(world_arena_opponents::Kind(arena_opponents_draft)),arena_simultaneous_draft,arena_orc_assets_ready?"ready":"unavailable",(unsigned long long)arena_progress.balance,arena_origin_x,arena_floor,arena_origin_z);
}

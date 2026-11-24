// Isolated opening host: authored cameras never move the playable capsule.
world_opening_prologue::State opening_state;
world_opening_prologue::Frame opening_frame;
world_npc_intro_media::Media opening_intro_media;
// Marked offscreen QA may inspect authored shot samples without changing the
// story clock, media clock, player position, completion, or save outcomes.
std::function<const world_npc_intro::Frame*()> npc_intro_qa_visual_sample;
GLuint opening_intro_texture=0;
int opening_intro_texture_width=0,opening_intro_texture_height=0;
bool opening_intro_was_active=false,opening_intro_resolution_failed=false;
unsigned opening_intro_starts=0,opening_intro_completions=0,opening_intro_uploads=0;
unsigned opening_intro_frames=0,opening_intro_shots=0,opening_intro_camera_fallbacks=0;
unsigned opening_intro_previous_shot=~0u;
std::unique_ptr<world_playtest_walk::Support> opening_house_walk;
bool opening_confirm=false,opening_interact=false,opening_jump=false,opening_begin_requested=false;
bool opening_cancel=false,opening_enter=false,opening_game_click=false,opening_game_cancel_click=false,opening_camp_click=false;
int opening_previous_camp_actor=-1;
world_opening_dialogue_camera::State opening_dialogue_blend;
world_opening_dialogue_camera::Pose opening_last_render_camera;
bool opening_last_render_camera_valid=false;
bool opening_last_camera_directed=false;
float opening_dialogue_letterbox=0,opening_checkpoint_seconds=0;
bool opening_arrival_music_pending=false;
float opening_bills_seconds=0;
unsigned opening_game_menus=0,opening_new_games=0;
unsigned opening_camp_guidance_frames=0;
world_journey_presentation::HudRegions opening_hud_regions;
unsigned opening_previous_house_shot=~0u;
WorldOpeningCinematicAudio opening_cinematic_audio;
bool opening_cinematic_audio_attempted=false,opening_cinematic_audio_was_ready=false;
unsigned opening_cinematic_cue_mask=0;
float opening_hud_reveal=1;
bool opening_last_playable=false,opening_have_save=false,opening_menu_confirm=false;
bool opening_room_lights_enabled=false;
world_opening_room_lights::Controls opening_room_light_controls;
int opening_room_light_target=-1;
struct OpeningRoomLightBinding {size_t index=0;unsigned room=0;float power=0;};
std::vector<OpeningRoomLightBinding> opening_room_light_bindings;
auto opening_apply_room_lights=[&](){
    world_opening_room_lights::render_mask(opening_room_lights_enabled,opening_state.room_lights());
    if(!opening_room_lights_enabled)return;
    for(const auto &binding:opening_room_light_bindings)if(binding.index<studio.live.lights.size())
        studio.live.lights[binding.index].power=world_opening_room_lights::on(opening_state.room_lights(),binding.room)?binding.power:0.f;
};
const float opening_meadow_sun=sun_strength,opening_meadow_sky=sky_fill,opening_meadow_exposure=exposure;
int opening_title_selection=0;
int opening_aux_return=0;bool opening_controls_pad=false;
unsigned opening_arrivals=0,opening_restores=0,opening_retries=0,opening_saves=0;
std::string opening_notice,opening_panel;
float opening_notice_seconds=0;
world_battlefield_prologue::ExplorationResume opening_battlefield_resume;
auto opening_tell=[&](const std::string &s){opening_notice=s;opening_notice_seconds=5;};
auto opening_release_actions=[&](){
    battlefield_release_edges();
    world_battlefield_prologue::release_actions_preserving_motion(preview_pad);
    preview_pad.stop_rumble();fight.set_guard(false);fight.clear_buffer();
    sword_stick.reset();equipment_action_gate.reset();opening_jump=false;
};
auto opening_release=[&](){
    opening_battlefield_resume.cancel();
    battlefield_release_edges();
    preview_release_mouse();preview_pad.clear();fight.set_guard(false);
    opening_last_playable=false;
    opening_jump=false;opening_movement_armed=false;
    opening_room_light_controls.release();opening_room_light_target=-1;
};
auto opening_save=[&]()->bool{
    if(!opening_enabled)return false;
    std::string bytes,error;
    auto checkpoint=opening_state.stable_save();
    if(first_journey_attach_save)first_journey_attach_save(checkpoint);
    if(battlefield_attach_save)battlefield_attach_save(checkpoint);
    if(inventory_attach_save)inventory_attach_save(checkpoint);
    if(!world_opening_prologue::encode_save(checkpoint,opening_state.config(),bytes,error)){opening_tell(error);return false;}
    std::error_code ec;std::filesystem::create_directories("saves",ec);
    if(ec){opening_tell("Could not create the checkpoint folder.");return false;}
    std::string pattern="saves/opening.progress.tmp-XXXXXX";std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);
    const int fd=mkstemp(name.data());if(fd<0){opening_tell("Could not stage this checkpoint.");return false;}
    FILE *f=fdopen(fd,"wb");bool ok=false;
    if(f){ok=std::fwrite(bytes.data(),1,bytes.size(),f)==bytes.size()&&std::fflush(f)==0&&fsync(fd)==0;if(std::fclose(f))ok=false;}else close(fd);
    if(ok)ok=vw_replace(name.data(),"saves/opening.progress")==0;
    if(!ok){unlink(name.data());opening_tell("Could not commit this checkpoint.");return false;}
    if(cutscene_progress_save&&!cutscene_progress_save(error)){opening_tell(error);return false;}
    opening_have_save=true;++opening_saves;if(first_journey_enabled)++first_journey_saves;opening_checkpoint_seconds=2.8f;return true;
};
// Apartment night: the home scenes play at night; every outdoor placement
// (transport arrival, outdoor checkpoint load/retry, encounter retry) restores
// the world's own hour. VALWOOD_APARTMENT_NIGHT=0 keeps the authored hour.
const bool apartment_night_enabled=[]{const char *v=std::getenv("VALWOOD_APARTMENT_NIGHT");return !v||std::strcmp(v,"0")!=0;}();
const float apartment_night_hour=21.75f;
bool apartment_night_active=false;float apartment_day_hour=hour;
auto opening_place=[&](const world_opening_prologue::Pose &p,bool house)->bool{
    opening_intro_media.stop();opening_intro_was_active=false;
    float floor=0;
    bool supported=false;
    if(house)supported=opening_house_walk&&opening_house_walk->sample(p.feet.x,p.feet.z,p.feet.y,floor);
    else{
        floor=std::max(world_atlas_walk_height(p.feet.x,p.feet.z),0.f);supported=true;
        // Outdoor checkpoints may stand on a bridge deck or authored stair,
        // above the terrain cook. Query the same collision support as walking.
        const auto *support=structure_support?structure_support.get():structure_baseline_walk;
        float deck=0;if(support&&support->sample(p.feet.x,p.feet.z,p.feet.y,deck))floor=deck;
    }
    if(!supported||!std::isfinite(floor)||std::abs(floor-p.feet.y)>.18){opening_tell("The opening checkpoint has no supported floor.");return false;}
    // Apartment support is temporary; every outdoor handoff/load/retry must
    // restore the exact current world cook, including source and bridge mesh.
    active_playtest_walk=house?opening_house_walk.get():(structure_support?structure_support.get():structure_baseline_walk);
    active_structure_world=!house&&(structure_support||structure_baseline_policy);
    active_city_physics=house||structure_support?nullptr:structure_baseline_city;
    playtest_walk_reference=p.feet.y;
    sun_strength=house?.16f:opening_meadow_sun;sky_fill=house?.85f:opening_meadow_sky;exposure=house?1.12f:opening_meadow_exposure;
    if(house&&apartment_night_enabled){
        if(!apartment_night_active){apartment_day_hour=hour;apartment_night_active=true;}
        hour=apartment_night_hour;exposure=1.18f;
    }else if(!house&&apartment_night_active){hour=apartment_day_hour;apartment_night_active=false;}
    world_apartment_window_light::night=apartment_night_active?1.f:0.f;
    cam.x=p.feet.x;cam.y=p.feet.y+1.75;cam.z=p.feet.z;cam.yaw=float(p.yaw);cam.pitch=float(p.pitch);cam.fov=1.04719755f;
    // Placement changes the capsule, never the player's selected camera view.
    // A new journey establishes its default separately; transport/load/retry
    // retain an explicit first- or third-person choice.
    cam.fly=false;cam.grounded=true;cam.vy=0;cam.swimming=false;cam.dive={};avatar_initialized=false;
    studio.open=false;show_tools=show_map=show_graphics=journey_open=false;
    milestone_review.view=world_milestone::View::None;portrait_focus_distance=0;
    if(studio.components_runtime)studio.components_runtime->close_dialogue();
    fight=WorldTrainingCombat{};live_training_encounter=0;preview_pending_save=false;
    current_name=house?(opening_state.config().game_start_story?"An Ordinary Evening":"An Ordinary Morning"):"The Meadows of Valois";
    destination_loading=true;temporal_cut=true;opening_dialogue_blend.forget();opening_dialogue_letterbox=0;opening_release();return true;
};
auto opening_load=[&]()->bool{
    std::ifstream file("saves/opening.progress");world_opening_prologue::Save saved;std::string error;
    if(!file||!world_opening_prologue::parse_save(file,opening_state.config(),saved,error)){opening_tell(error.empty()?"No opening checkpoint yet.":error);return false;}
    world_opening_prologue::State staged=opening_state;
    if(!staged.restore(saved,error)){opening_tell(error);return false;}
    if(first_journey_restore_save&&!first_journey_restore_save(saved,error)){opening_tell(error);return false;}
    if(battlefield_restore_save&&!battlefield_restore_save(saved,error)){opening_tell(error);return false;}
    if(inventory_restore_save&&!inventory_restore_save(saved,error)){opening_tell(error);return false;}
    const bool house=saved.checkpoint<=world_opening_prologue::Checkpoint::HouseInspected;
    auto pose=saved.player;if(!house&&first_journey_checkpoint_pose)first_journey_checkpoint_pose(pose);
    if(!house&&battlefield_checkpoint_pose)battlefield_checkpoint_pose(pose);
    if(!opening_place(pose,house))return false;
    opening_state=std::move(staged);opening_frame=opening_state.sample({cam.x,cam.y-1.75,cam.z},preview_settings.reduced_motion);
    if(first_journey_commit_restore)first_journey_commit_restore();
    if(battlefield_commit_restore)battlefield_commit_restore();
    if(inventory_commit_restore)inventory_commit_restore();
    opening_apply_room_lights();
    opening_controls_enabled=false;opening_last_playable=false;preview_screen=1;opening_have_save=true;++opening_restores;if(first_journey_enabled)++first_journey_restores;
    if(cutscene_progress_load&&!cutscene_progress_load(error)){opening_tell(error);return false;}
    if(demo_presentation_enabled&&first_journey_enabled&&first_journey_state.stage()==world_first_journey::Stage::Complete){preview_screen=5;opening_panel.clear();opening_release();}
    opening_tell("Checkpoint restored");return true;
};
auto opening_retry=[&](){
    auto saved=opening_state.stable_save();if(first_journey_attach_save)first_journey_attach_save(saved);if(battlefield_attach_save)battlefield_attach_save(saved);if(inventory_attach_save)inventory_attach_save(saved);
    std::string error;auto staged=opening_state;
    if(!staged.restore(saved,error)||(first_journey_restore_save&&!first_journey_restore_save(saved,error))){opening_tell(error);return;}
    if(battlefield_restore_save&&!battlefield_restore_save(saved,error)){opening_tell(error);return;}
    if(inventory_restore_save&&!inventory_restore_save(saved,error)){opening_tell(error);return;}
    auto pose=saved.player;const bool house=saved.checkpoint<=world_opening_prologue::Checkpoint::HouseInspected;
    if(!house&&first_journey_checkpoint_pose)first_journey_checkpoint_pose(pose);
    if(!house&&battlefield_checkpoint_pose)battlefield_checkpoint_pose(pose);
    if(!opening_place(pose,house)){opening_tell("Checkpoint could not be safely restarted.");return;}
    opening_state=std::move(staged);if(first_journey_commit_restore)first_journey_commit_restore();if(battlefield_commit_restore)battlefield_commit_restore();if(inventory_commit_restore)inventory_commit_restore();
    opening_frame=opening_state.sample({cam.x,cam.y-1.75,cam.z},preview_settings.reduced_motion);
    opening_apply_room_lights();
    opening_controls_enabled=false;opening_last_playable=false;preview_screen=1;++opening_retries;opening_tell("Checkpoint restarted");
};
auto opening_new_journey=[&]()->bool{
    // A previous outdoor checkpoint may still own the playable capsule and
    // terrain support. Re-enter the real apartment before publishing Started.
    // Failed placement leaves the current story and durable player data intact.
    opening_begin_requested=false;
    const auto spawn=opening_state.config().house_spawn;
    if(!opening_house_walk||!opening_house_walk->standing_clear(spawn.feet.x,spawn.feet.z,spawn.feet.y)){
        opening_tell("The apartment starting point is obstructed.");return false;
    }
    if(!opening_place(spawn,true))return false;
    if(battlefield_reset)battlefield_reset();
    third_person=true;
    opening_state.reset();
    opening_frame=opening_state.sample({cam.x,cam.y-1.75,cam.z},preview_settings.reduced_motion);
    opening_apply_room_lights();
    opening_controls_enabled=false;opening_last_playable=false;preview_screen=1;
    opening_panel.clear();opening_notice.clear();opening_notice_seconds=0;
    opening_bills_seconds=opening_checkpoint_seconds=0;opening_arrival_music_pending=false;
    opening_game_click=opening_game_cancel_click=opening_camp_click=opening_menu_confirm=false;
    opening_previous_camp_actor=-1;opening_previous_house_shot=~0u;
    opening_begin_requested=true;return true;
};
if(opening_enabled){
    world_opening_prologue::Config config;std::string error,token;unsigned version=0;
    std::ifstream file("opening.config");file.imbue(std::locale::classic());
    auto vec=[&](world_opening_prologue::Vec &v){return bool(file>>v.x>>v.y>>v.z);};
    auto pose=[&](world_opening_prologue::Pose &p){return vec(p.feet)&&bool(file>>p.yaw>>p.pitch);};
    auto rail=[&](world_opening_prologue::Rail &r){return vec(r.eye_from)&&vec(r.eye_to)&&vec(r.target_from)&&vec(r.target_to)&&bool(file>>r.fov_from>>r.fov_to);};
    auto record=[&](const char *name){return bool(file>>token)&&token==name;};
    bool ok=bool(file>>token>>version)&&token=="VALWOOD_OPENING_SCENE"&&version==1;
    ok=ok&&record("house_spawn")&&pose(config.house_spawn)&&record("meadow_spawn")&&pose(config.meadow_spawn)&&
        record("house_focus")&&vec(config.house_focus)&&record("house_exit")&&vec(config.house_exit)&&record("meadow_goal")&&vec(config.meadow_goal)&&
        record("house_rail")&&rail(config.house_rail)&&record("transport_rail")&&rail(config.transport_rail)&&record("meadow_rail")&&rail(config.meadow_rail);
    unsigned home_count=0,meadow_count=0;world_opening_prologue::Vec point;
    while(ok&&file>>token){
        if(token=="house_waypoint")++home_count;else if(token=="meadow_waypoint")++meadow_count;else{ok=false;break;}
        if(home_count>512||meadow_count>512||!vec(point)||!world_opening_prologue::detail::valid(point))ok=false;
    }
    std::ifstream game_file("opening/game-start.settings");
    if(game_file){
        unsigned game_version=0;std::string game_tag;
        bool valid=bool(game_file>>game_tag>>game_version)&&game_tag=="VALWOOD_GAME_START"&&game_version==1;
        game_file>>std::ws;valid=valid&&game_file.eof();
        if(!valid){std::fprintf(stderr,"[Opening] Invalid game-start story settings\n");return 1;}
        config.game_start_story=true;
    }
    config.goal_radius=3.;
    std::ifstream arrival_camp_file("opening/arrival-camp.settings");
    if(arrival_camp_file){
        if(!world_opening_arrival_camp::parse(arrival_camp_file,config.arrival_camp,error)){
            std::fprintf(stderr,"[Opening] Invalid arrival camp: %s\n",error.c_str());return 1;
        }
        WorldTrailCampPlacement placements[3];
        for(unsigned i=0;i<3;++i){const auto &a=config.arrival_camp.actors[i];placements[i]={a.feet.x,a.feet.y,a.feet.z,float(a.yaw),a.seated};}
        const auto &center=config.arrival_camp.center;
        if(!world_trail_camp_configure_arrival(arrival_camp,center.x,center.y,center.z,placements,3)){
            std::fprintf(stderr,"[Opening] Could not configure the three arrival travelers\n");return 1;
        }
        arrival_camp_enabled=true;
        std::printf("[Opening] Three arrival travelers configured: Rowan, Bram, Ellis\n");
    }
    if(first_journey_enabled){
        std::ifstream journey_file("opening/first-journey.settings");
        if(!journey_file||!world_first_journey::parse_config(journey_file,config.first_journey,error)){
            std::fprintf(stderr,"[Opening] Invalid connected first quest: %s\n",error.c_str());return 1;
        }
        config.first_journey_enabled=true;
    }
    std::ifstream cinematic_file("opening/cinematic.settings");
    if(cinematic_file&&!world_opening_cinematic_settings::parse(cinematic_file,config,config.house_shots,error)){
        std::fprintf(stderr,"[Opening] Invalid apartment cinematic: %s\n",error.c_str());return 1;
    }
    std::ifstream intro_file("opening/npc-intro.settings");
    if(intro_file&&!world_npc_intro::parse(intro_file,config.npc_intro,error)){
        std::fprintf(stderr,"[Opening] Invalid NPC introduction: %s\n",error.c_str());return 1;
    }
    if(config.npc_intro.enabled){
        std::string media_error;
        if(opening_intro_media.load(".",media_error)){
            config.npc_intro.duration_seconds=opening_intro_media.duration();
            std::printf("[NPC intro] Supplied media ready, duration %.3fs\n",opening_intro_media.duration());
        }else if(!media_error.empty())std::fprintf(stderr,"[NPC intro] %s; using the in-game introduction.\n",media_error.c_str());
    }
    ok=ok&&file.eof()&&home_count>=5&&meadow_count>=2&&opening_state.configure(config,error);
    if(ok){opening_house_walk=std::make_unique<world_playtest_walk::Support>();ok=opening_house_walk->load("opening/home.settings",error);}
    if(!ok){std::fprintf(stderr,"[Opening] Invalid authored opening: %s\n",error.c_str());return 1;}
    if(!opening_place(config.house_spawn,true)){std::fprintf(stderr,"[Opening] Unsupported house spawn\n");return 1;}
    preview_screen=0;opening_frame=opening_state.sample({cam.x,cam.y-1.75,cam.z});opening_controls_enabled=false;
    std::ifstream authored_controls_file("opening/room-controls.settings");
    if(authored_controls_file&&!world_opening_room_lights::parse_authored(authored_controls_file,error)){
        std::fprintf(stderr,"[Opening] Invalid authored room controls: %s\n",error.c_str());return 1;
    }
    std::ifstream electrical_file("opening/room-lights.settings");
    if(electrical_file){
        unsigned electrical_version=0;std::string electrical_tag;
        bool valid=bool(electrical_file>>electrical_tag>>electrical_version)&&electrical_tag=="VALWOOD_ROOM_LIGHTS"&&electrical_version==1;
        electrical_file>>std::ws;valid=valid&&electrical_file.eof();
        if(!valid){std::fprintf(stderr,"[Opening] Invalid apartment electrical controls\n");return 1;}
        for(size_t i=0;i<studio.live.lights.size();++i){
            const auto &light=studio.live.lights[i];const int room=world_opening_room_lights::practical_room({light.x,light.y,light.z});
            if(room>=0)opening_room_light_bindings.push_back({i,unsigned(room),light.power});
        }
        if(opening_room_light_bindings.size()!=world_opening_room_lights::practicals.size()){
            std::fprintf(stderr,"[Opening] Apartment practical light circuit bindings are incomplete\n");return 1;
        }
        opening_room_lights_enabled=true;opening_apply_room_lights();
        std::printf("[Opening] Four room switches bound to %zu practical lights; window daylight stays independent\n",opening_room_light_bindings.size());
    }
    std::ifstream saved_file("saves/opening.progress");world_opening_prologue::Save saved;
    opening_have_save=saved_file&&world_opening_prologue::parse_save(saved_file,config,saved,error);
}
opening_key=[&](SDL_Keycode key)->bool{
    if(!opening_enabled)return false;
    if((preview_screen==4||preview_screen==6)&&key==SDLK_ESCAPE){preview_screen=opening_aux_return;opening_panel.clear();opening_release();return true;}
    if(demo_presentation_enabled&&key==SDLK_F1){opening_aux_return=preview_screen==0?0:2;opening_controls_pad=preview_pad.using_pad;preview_screen=4;opening_panel.clear();opening_release();return true;}
    if(key==SDLK_ESCAPE){
        if(opening_frame.npc_intro){
            if(show_graphics)show_graphics=false;
            else preview_screen=preview_screen==1?2:1;
            opening_release();return true;
        }
        if(!show_graphics&&preview_screen==1&&(opening_state.camp().active()||first_journey_dialogue_active))return true;
        if(show_graphics)show_graphics=false;
        else if(preview_screen==1&&opening_state.stage()==world_opening_prologue::Stage::GameMenu){opening_game_cancel_click=true;return true;}
        else if(preview_screen==0){/* Title owns the exit button. */}
        else preview_screen=preview_screen==1?2:1;
        opening_release();return true;
    }
    if(show_graphics){if(key==SDLK_F10)show_graphics=false;return true;}
    if(key==SDLK_F10){show_graphics=true;graphics_draft=graphics;opening_release();return true;}
    if(key==SDLK_F8){opening_load();return true;}
    if(preview_screen==0){
        // Enter activates the same focused action as controller confirm.
        // SDL's synthetic/first-frame Enter is not guaranteed to activate an
        // ImGui button, so keep its selected action explicit.
        if(key==SDLK_RETURN||key==SDLK_KP_ENTER){
            if(opening_title_selection==0)opening_new_journey();
            else if(opening_title_selection==1)opening_load();
            else if(opening_title_selection==2){show_graphics=true;graphics_draft=graphics;}
            else if(opening_title_selection==3)running=false;
            else if(opening_title_selection==4){opening_aux_return=0;opening_controls_pad=preview_pad.using_pad;preview_screen=4;opening_panel.clear();}
            else if(opening_title_selection==5){opening_aux_return=0;preview_screen=6;opening_panel.clear();}
        }
        return true;
    }
    if((preview_screen==2||preview_screen==3||preview_screen==4||preview_screen==5||preview_screen==6)&&!show_graphics&&(key==SDLK_RETURN||key==SDLK_KP_ENTER)){opening_menu_confirm=true;return true;}
    // Cinematic battle actions are polled by their exclusive Q/F/Enter or
    // X/A owner. Only pause, graphics and the menus above consume events here.
    if(battlefield_cinematic&&battlefield_cinematic())return true;
    if(key==SDLK_j||key==SDLK_TAB){preview_screen=preview_screen==3?1:3;opening_release();return true;}
    if(key==SDLK_F5){if(!opening_frame.cinematic)opening_save();return true;}
    if(key==SDLK_r&&preview_screen==1&&!opening_frame.cinematic){if(first_journey_enabled&&first_journey_defeated){first_journey_retry_down=true;return true;}opening_retry();return true;}
    if(preview_screen!=1||show_graphics||destination_loading)return true;
    if(opening_state.stage()==world_opening_prologue::Stage::GameMenu)return true;
    if(opening_controls_enabled&&key==SDLK_v)return false;
    if(first_journey_enabled&&opening_controls_enabled&&(first_journey_state.weapon_unlocked()||(battlefield_sword_owned&&battlefield_sword_owned()))&&key==SDLK_z)return false;
    if(first_journey_enabled&&fight.engaged()&&(key==SDLK_LEFT||key==SDLK_RIGHT||key==SDLK_UP||key==SDLK_DOWN||key==SDLK_t||key==SDLK_SPACE))return false;
    if(key==SDLK_SPACE){if(opening_controls_enabled)opening_jump=true;return true;}
    if(key==SDLK_e||key==SDLK_RETURN||key==SDLK_KP_ENTER)return true;
    // Block authoring, atlas travel and hero/camera swaps in the opening copy.
    return key!=SDLK_w&&key!=SDLK_a&&key!=SDLK_s&&key!=SDLK_d&&key!=SDLK_LSHIFT;
};
opening_tick=[&](float delta){
    if(!opening_enabled)return;
    const bool paused=(preview_screen!=1&&preview_screen!=0)||show_graphics||destination_loading||!preview_pad.focused||(SDL_GetWindowFlags(window)&SDL_WINDOW_MINIMIZED);
    const bool battlefield_was_active=battlefield_active&&battlefield_active();
    const bool preserve_battlefield_resume=opening_battlefield_resume.take(paused,battlefield_was_active);
    if(!paused){opening_notice_seconds=std::max(0.f,opening_notice_seconds-delta);opening_checkpoint_seconds=std::max(0.f,opening_checkpoint_seconds-delta);}
    const bool score_apartment=opening_state.stage()<=world_opening_prologue::Stage::Transport||opening_state.stage()==world_opening_prologue::Stage::GameMenu;
    const float score_gain=opening_frame.npc_intro?.18f:score_apartment?(!opening_state.config().house_shots.empty()&&opening_state.stage()!=world_opening_prologue::Stage::Transport?.10f:.35f):1.f;
    preview_audio.set_context(!paused&&!inventory_open&&preview_screen==1,preview_settings.music*score_gain);
    const bool pad_a=preview_pad.get()&&preview_pad.focused&&SDL_GameControllerGetButton(preview_pad.get(),SDL_CONTROLLER_BUTTON_A);
    world_opening_prologue::Input input;
    input.dt=delta;input.player_feet={cam.x,cam.y-1.75,cam.z};input.paused=paused&&opening_state.stage()!=world_opening_prologue::Stage::Title;
    input.confirm_down=opening_state.stage()==world_opening_prologue::Stage::Title?opening_begin_requested:(opening_confirm||pad_a);
    if(opening_state.stage()==world_opening_prologue::Stage::GameMenu)input.confirm_down=opening_enter||pad_a;
    input.confirm_down=input.confirm_down||opening_game_click;
    input.cancel_down=opening_cancel||opening_game_cancel_click||(preview_pad.get()&&preview_pad.focused&&SDL_GameControllerGetButton(preview_pad.get(),SDL_CONTROLLER_BUTTON_B));
    input.interact_down=opening_interact||pad_a||opening_camp_click;input.reduced_motion=preview_settings.reduced_motion;
    const bool switches_playable=opening_room_lights_enabled&&!paused&&opening_state.stage()==world_opening_prologue::Stage::HouseExplore;
    opening_room_light_target=-1;
    // Story interactions retain first priority. A room circuit can never
    // swallow the notebook or front-door action, and all interactions observe
    // the same raw edge so carrying A/E from a switch cannot trigger a quest.
    if(switches_playable&&!opening_state.sample(input.player_feet).interaction_available){
        const double cp=std::cos(cam.pitch);
        opening_room_light_target=world_opening_room_lights::target({cam.x,cam.y,cam.z},{std::sin(cam.yaw)*cp,std::sin(cam.pitch),-std::cos(cam.yaw)*cp});
    }
    const int toggled_room=opening_room_light_controls.tick(switches_playable,input.interact_down,opening_room_light_target);
    if(toggled_room>=0)opening_state.set_room_lights(opening_state.room_lights()^(1u<<unsigned(toggled_room)));
    const auto first_journey_input=input;
    if(battlefield_active&&battlefield_active()){
        // Preserve the old opening checkpoint meanings while the battlefield
        // exclusively owns progression, prompts, camera and action buttons.
        input.paused=true;input.interact_down=input.confirm_down=input.cancel_down=false;
    }
    if(first_journey_enabled&&first_journey_state.stage()!=world_first_journey::Stage::Travelers&&!opening_state.camp().active())input.interact_down=false;
    opening_frame=opening_state.tick(input);opening_begin_requested=false;opening_game_click=false;opening_game_cancel_click=false;opening_camp_click=false;
    const int camp_actor=opening_frame.camp_dialogue?opening_frame.camp_actor:-1;
    if(camp_actor!=opening_previous_camp_actor){temporal_cut=true;opening_previous_camp_actor=camp_actor;}
    if(opening_frame.npc_intro){
        ++opening_intro_frames;opening_intro_shots|=1u<<opening_frame.npc_intro_shot_index;
        if(!opening_intro_was_active){
            ++opening_intro_starts;opening_intro_resolution_failed=false;
            opening_intro_texture_width=opening_intro_texture_height=0;
            opening_intro_media.play(opening_frame.npc_intro_time);opening_release();
        }
        if(opening_intro_previous_shot!=opening_frame.npc_intro_shot_index){temporal_cut=true;opening_intro_previous_shot=opening_frame.npc_intro_shot_index;}
        opening_intro_media.update(opening_frame.npc_intro_time,paused,preview_settings.voice,true);
    }else if(opening_intro_was_active){
        opening_intro_media.stop();opening_intro_previous_shot=~0u;temporal_cut=true;
        if(opening_frame.events&world_opening_prologue::NpcIntroCompleted)++opening_intro_completions;
        opening_release();
    }
    opening_intro_was_active=opening_frame.npc_intro;
    if(opening_frame.stage==world_opening_prologue::Stage::HouseEstablish&&!opening_state.config().house_shots.empty()){
        if(opening_previous_house_shot!=opening_frame.house_shot_index){temporal_cut=true;opening_previous_house_shot=opening_frame.house_shot_index;}
    }else opening_previous_house_shot=~0u;
    if(!paused)opening_bills_seconds=std::max(0.f,opening_bills_seconds-delta);
    using OpeningStage=world_opening_prologue::Stage;
    const bool game_story=opening_state.config().game_start_story;
    world_opening_game_screen::state={{game_story?1.f:0.f,opening_frame.stage==OpeningStage::GameMenu?1.f:(opening_frame.stage==OpeningStage::Transport?2.f:0.f),float(opening_frame.stage_progress),0.f}};
    opening_apply_room_lights();
    const auto events=opening_frame.events;
    const bool directed_home=!opening_state.config().house_shots.empty();
    if(directed_home&&!opening_cinematic_audio_attempted){
        int audio_rate=0,audio_channels=0;Uint16 audio_format=0;
        if(Mix_QuerySpec(&audio_rate,&audio_format,&audio_channels)){
            opening_cinematic_audio_attempted=true;
            if(!opening_cinematic_audio.init())std::fprintf(stderr,"[Opening] Apartment sound unavailable; opening continues with existing audio.\n");
            else {opening_cinematic_audio_was_ready=true;std::printf("[Opening] Original apartment Foley and ambience ready\n");}
        }
    }
    const bool home_audio=directed_home&&(opening_frame.stage==OpeningStage::HouseEstablish||opening_frame.stage==OpeningStage::HouseExplore||opening_frame.stage==OpeningStage::GameMenu);
    opening_cinematic_audio.updateAmbient(delta,home_audio,paused,preview_settings.music);
    if(events&world_opening_prologue::Started){opening_cinematic_cue_mask=0;opening_hud_reveal=0;if(first_journey_reset)first_journey_reset();player_inventory=player_profile.inventory;}
    if(!paused&&opening_frame.stage==OpeningStage::HouseExplore)opening_hud_reveal=std::min(1.f,opening_hud_reveal+delta/.8f);
    if(directed_home&&!paused&&opening_frame.stage==OpeningStage::HouseEstablish){
        if(!(opening_cinematic_cue_mask&1u)){opening_cinematic_audio.play(WorldOpeningCinematicAudio::Cue::DoorClose,.9f);opening_cinematic_cue_mask|=1u;}
        if(opening_state.elapsed()>=1.05&&!(opening_cinematic_cue_mask&2u)){opening_cinematic_audio.play(WorldOpeningCinematicAudio::Cue::BagDown,.9f);opening_cinematic_cue_mask|=2u;}
    }
    if(events&world_opening_prologue::Started)preview_screen=1;
    if(events&world_opening_prologue::Arrived){
        if(battlefield_start)battlefield_start();
        else if(!opening_place(opening_state.config().meadow_spawn,false)){launch->exit_code=1;running=false;return;}
        ++opening_arrivals;opening_arrival_music_pending=!battlefield_start;
    }
    if(opening_frame.release_controls){
        const bool resume_only=opening_frame.events==world_opening_prologue::ReleaseControls;
        if(preserve_battlefield_resume&&resume_only&&opening_frame.controls_enabled&&!opening_frame.cinematic&&!opening_frame.camp_dialogue)
            opening_release_actions();
        else opening_release();
    }
    if(events&world_opening_prologue::GameOpened){++opening_game_menus;opening_bills_seconds=0;if(directed_home)opening_cinematic_audio.play(WorldOpeningCinematicAudio::Cue::ConsolePower,.85f);else preview_audio.cue(4);}
    if(events&world_opening_prologue::TransportStarted){if(game_story)++opening_new_games;preview_audio.cue(4);}
    if(events&world_opening_prologue::TransportPulse){if(!game_story)preview_audio.cue(2);if(preview_settings.vibration)preview_pad.rumble(.15f,.35f,180,true);}
    if(events&world_opening_prologue::Inspected){if(game_story){opening_bills_seconds=9;opening_tell("I need a break. I'll play a video game on the TV.");}else opening_tell("A quiet morning. Check the front door.");}
    if(events&world_opening_prologue::CheckpointReached)opening_save();
    if(game_story&&(events&world_opening_prologue::Completed))opening_tell("Valois. The name on the screen. How am I standing here?");
    if(toggled_room>=0){
        // A toggle is deliberately saved at the same supported story
        // checkpoint as F5; it never serializes a camera or arbitrary pose.
        const bool saved=opening_save();
        if(saved)opening_tell(std::string(world_opening_room_lights::switches[unsigned(toggled_room)].name)+" lights "+(world_opening_room_lights::on(opening_state.room_lights(),unsigned(toggled_room))?"on":"off"));
    }
    if(opening_arrival_music_pending&&!paused&&!destination_loading){preview_audio.cue(2);opening_arrival_music_pending=false;}
    if(battlefield_tick)battlefield_tick(delta,paused);
    // The quest owner resumes in this same frame; the outer opening resumes
    // one frame later. Each gets a separate, narrowly scoped action-release
    // token before either owner can discard continuously held movement.
    if(battlefield_was_active&&!(battlefield_active&&battlefield_active())){
        const bool continuous=!paused&&battlefield_sword_owned&&battlefield_sword_owned()&&
            opening_frame.controls_enabled&&!opening_frame.cinematic&&!opening_frame.camp_dialogue;
        if(continuous)opening_battlefield_resume.begin();else opening_battlefield_resume.cancel();
    }
    if(first_journey_tick)first_journey_tick(first_journey_input);
    if(battlefield_apply_frame)battlefield_apply_frame(opening_frame);
    opening_controls_enabled=opening_frame.controls_enabled&&!paused&&!destination_loading;
    if(opening_controls_enabled&&!opening_movement_armed){
        const auto *keys=SDL_GetKeyboardState(nullptr);
        bool neutral=!keys[SDL_SCANCODE_W]&&!keys[SDL_SCANCODE_A]&&!keys[SDL_SCANCODE_S]&&!keys[SDL_SCANCODE_D];
        if(preview_pad.get())for(auto axis:{SDL_CONTROLLER_AXIS_LEFTX,SDL_CONTROLLER_AXIS_LEFTY,SDL_CONTROLLER_AXIS_RIGHTX,SDL_CONTROLLER_AXIS_RIGHTY})
            neutral=neutral&&std::abs(int(SDL_GameControllerGetAxis(preview_pad.get(),axis)))<=int((preview_settings.deadzone+.02f)*32767);
        opening_movement_armed=neutral;
    }
    if(opening_controls_enabled&&!opening_last_playable&&!preview_pad.using_pad&&!launch->frames){look_active=true;SDL_SetRelativeMouseMode(SDL_TRUE);}
    if(!opening_controls_enabled&&opening_last_playable)opening_release();
    opening_last_playable=opening_controls_enabled;
};
auto opening_render_camera=[&](Camera &eye,float delta){
    if(!opening_enabled)return;
    using world_opening_dialogue_camera::Pose;
    const Pose base{eye.x,eye.y,eye.z,eye.yaw,eye.pitch,eye.fov,0};
    const bool directed=opening_frame.cinematic||opening_frame.stage==world_opening_prologue::Stage::Title||opening_frame.stage==world_opening_prologue::Stage::GameMenu;
    auto c=opening_frame.camera;
    if(opening_frame.npc_intro&&npc_intro_qa&&npc_intro_qa_visual_sample){
        if(const auto *sample=npc_intro_qa_visual_sample())c={{sample->camera.eye.x,sample->camera.eye.y,sample->camera.eye.z},{sample->camera.target.x,sample->camera.target.y,sample->camera.target.z},sample->camera.fov,sample->camera.fade,sample->camera.letterbox};
    }
    auto clear=[&](const Pose &a,const Pose &b){
        if(std::hypot(a.x-b.x,a.y-b.y,a.z-b.z)<.00001)return true;
        const float low=float(std::min(a.y,b.y)-.18),height=float(std::abs(a.y-b.y)+.36);
        if(!world_scenery_can_move(scenery,a.x,a.z,b.x,b.z,low,height,.16f)||b.y<world_atlas_walk_height(b.x,b.z)+.5)return false;
        if(active_playtest_walk||active_city_physics){
            ++source_camera_casts;world_physics::Hit hit;
            const world_physics::Vec3 center{a.x,a.y,a.z},movement{b.x-a.x,b.y-a.y,b.z-a.z};
            const bool queried=active_playtest_walk?active_playtest_walk->cast_capsule(center,.16,.02,movement,hit):active_city_physics->cast_capsule(center,.16,.02,movement,hit);
            if(!queried)++source_camera_query_failures;
            if(!queried||(hit.hit&&hit.fraction<.999f)){++source_camera_blocks;return false;}
        }
        return true;
    };
    if(opening_frame.npc_intro){
        // Authored shots move only the render eye. Reject obstructed positions
        // instead of moving the playable capsule through the camp scenery.
        const Pose requested{c.eye.x,c.eye.y,c.eye.z,0,0,c.fov,c.letterbox};
        const Pose player{cam.x,cam.y,cam.z,cam.yaw,cam.pitch,cam.fov,0};
        if(!clear(player,requested)){c.eye={cam.x,cam.y,cam.z};++opening_intro_camera_fallbacks;}
    }else if(opening_frame.camp_dialogue){
        // The same portrait language is used for first contacts and the quest.
        WorldTrailCamper actor;
        const bool found=opening_frame.camp_actor>=0?
            world_trail_camp_actor(arrival_camp,unsigned(opening_frame.camp_actor),&actor):
            first_journey_dialogue_active&&world_trail_camp_actor(first_journey_townsfolk,0,&actor);
        if(found){
            const double heading=world_journey_presentation::portrait_heading(actor.yaw,cam.x-actor.x,cam.z-actor.z);
            c.eye.x=c.target.x+std::sin(heading)*2.35;
            c.eye.z=c.target.z+std::cos(heading)*2.35;
        }
        const double dx=c.eye.x-c.target.x,dz=c.eye.z-c.target.z,d=std::max(.001,std::hypot(dx,dz));
        c.eye.x=c.target.x+dx/d*2.35;c.eye.z=c.target.z+dz/d*2.35;c.fov=.86;c.letterbox=.055;
        const Pose requested{c.eye.x,c.eye.y,c.eye.z,0,0,c.fov,c.letterbox};
        const Pose player{cam.x,cam.y,cam.z,cam.yaw,cam.pitch,cam.fov,0};
        if(!clear(player,requested))c.eye={cam.x,cam.y,cam.z};
    }
    Pose goal=base;
    if(directed||opening_frame.camp_dialogue){
        const double dx=c.target.x-c.eye.x,dy=c.target.y-c.eye.y,dz=c.target.z-c.eye.z;
        goal={c.eye.x,c.eye.y,c.eye.z,std::atan2(dx,-dz),std::atan2(dy,std::max(.001,std::hypot(dx,dz))),c.fov,c.letterbox};
        if(!preview_settings.reduced_motion&&!opening_state.config().house_shots.empty()&&opening_frame.stage==world_opening_prologue::Stage::HouseEstablish&&opening_frame.house_shot_index==1)
            portrait_focus_distance=float(std::sqrt(dx*dx+dy*dy+dz*dz));
    }
    Pose result=goal;
    if(directed){opening_dialogue_blend.forget();opening_dialogue_letterbox=float(goal.letterbox);}
    else{
        // The last authored apartment shot hands off to the player's selected
        // view through the same collision-checked render-only camera blend.
        if(opening_last_camera_directed&&opening_last_render_camera_valid&&
           (opening_frame.stage==world_opening_prologue::Stage::HouseExplore||
            (battlefield_enabled&&(battlefield_frame.stage==world_battlefield_prologue::Stage::FleeToAmbush||battlefield_frame.stage==world_battlefield_prologue::Stage::Escape)))){
            const auto start=third_person?world_opening_dialogue_camera::body_safe_handoff_start(
                opening_last_render_camera,base,cam.x,cam.y-1.75,cam.z):opening_last_render_camera;
            const bool orientation_only=third_person&&start.x==base.x&&start.y==base.y&&start.z==base.z;
            opening_dialogue_blend.handoff(start,base,orientation_only);
        }
        const bool paused=preview_screen!=1||show_graphics||inventory_open||destination_loading||!preview_pad.focused;
        result=opening_dialogue_blend.update(base,goal,opening_frame.camp_dialogue,delta,paused,preview_settings.reduced_motion,clear);
        opening_dialogue_letterbox=float(result.letterbox);
    }
    eye.x=result.x;eye.y=result.y;eye.z=result.z;eye.yaw=float(result.yaw);eye.pitch=float(result.pitch);eye.fov=float(result.fov);
    opening_last_render_camera=result;opening_last_render_camera_valid=true;
    opening_last_camera_directed=directed;
};
opening_draw=[&](const Camera &eye,bool loading){
    opening_hud_regions.clear();
    if(!opening_enabled||show_graphics)return;
    auto &io=ImGui::GetIO();const float w=io.DisplaySize.x,h=io.DisplaySize.y;
    const float u=std::clamp(std::min(w/1600.f,h/900.f),.65f,1.5f),scale=u*preview_settings.text_scale;
    auto *dl=ImGui::GetForegroundDrawList();
    const float hud_alpha=opening_frame.stage==world_opening_prologue::Stage::HouseExplore&&!opening_state.config().house_shots.empty()?float(world_opening_prologue::detail::ease(opening_hud_reveal)):1.f;
    const ImU32 white=IM_COL32(244,238,222,int(255*hud_alpha)),gold=IM_COL32(226,198,137,int(255*hud_alpha));
    auto text=[&](float x,float y,float size,ImU32 color,const char *value){dl->AddText(ImGui::GetFont(),size*scale,{x,y},color,value);};
    auto center=[&](float y,float size,ImU32 color,const char *value){const auto extent=ImGui::GetFont()->CalcTextSizeA(size*scale,10000,0,value);const float x=(w-extent.x)*.5f;text(x,y,size,color,value);opening_hud_regions.add({x-6*u,y-4*u,x+extent.x+6*u,y+extent.y+4*u});};
    const bool pad=preview_pad.using_pad&&preview_pad.get();
    const std::string confirm=pad?preview_pad.button(SDL_CONTROLLER_BUTTON_A):"E";
    const std::string skip=pad?preview_pad.button(SDL_CONTROLLER_BUTTON_A):"ENTER / SPACE";
    const float bars=opening_dialogue_letterbox*h;
    if(bars>0){ImGui::GetBackgroundDrawList()->AddRectFilled({0,0},{w,bars},IM_COL32(0,0,0,255));ImGui::GetBackgroundDrawList()->AddRectFilled({0,h-bars},{w,h},IM_COL32(0,0,0,255));}
    const bool game_story=opening_state.config().game_start_story;
    const bool game_transport=game_story&&opening_frame.stage==world_opening_prologue::Stage::Transport;
    const bool game_arrival=game_story&&opening_frame.stage==world_opening_prologue::Stage::MeadowReveal&&!(battlefield_active&&battlefield_active());
    const float fade=loading&&preview_screen==1?1.f:(game_transport?float(world_opening_prologue::detail::ease((opening_frame.stage_progress-.48)/.43)):float(opening_frame.camera.fade));
    if(fade>0)ImGui::GetBackgroundDrawList()->AddRectFilled({0,0},{w,h},(game_transport||game_arrival)&&!loading?IM_COL32(225,243,249,int(std::clamp(fade,0.f,1.f)*255)):IM_COL32(0,0,0,int(std::clamp(fade,0.f,1.f)*255)));
    auto subtitle=[&](const char *value){
        if(!value||!*value)return;
        const float size=19*scale;const auto ts=ImGui::GetFont()->CalcTextSizeA(size,w-90*u,0,value);
        dl->AddRectFilled({(w-ts.x)*.5f-18*u,h*.875f-9*u},{(w+ts.x)*.5f+18*u,h*.875f+ts.y+9*u},IM_COL32(5,10,14,220),4*u);
        center(h*.875f,19,white,value);
    };
    // The held movie belongs behind menus; foreground subtitles and skip HUD
    // are added only during playback. The exact last frame survives a pause.
    const bool intro_video=opening_frame.npc_intro&&opening_intro_media.ready()&&!opening_intro_media.failed()&&opening_intro_media.mode()==world_npc_intro_media::Mode::Video;
    if(intro_video){
        auto *background=ImGui::GetBackgroundDrawList();
        const bool video=true;
        if(video){
            world_npc_intro_media::Frame image;
            if(opening_intro_media.take_frame(image)&&!image.rgba.empty()){
                GLint binding=0,alignment=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding);glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);
                if(!opening_intro_texture)glGenTextures(1,&opening_intro_texture);
                glBindTexture(GL_TEXTURE_2D,opening_intro_texture);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
                if(opening_intro_texture_width!=int(image.width)||opening_intro_texture_height!=int(image.height)){
                    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,image.width,image.height,0,GL_RGBA,GL_UNSIGNED_BYTE,image.rgba.data());
                    opening_intro_texture_width=int(image.width);opening_intro_texture_height=int(image.height);
                    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
                }else glTexSubImage2D(GL_TEXTURE_2D,0,0,0,image.width,image.height,GL_RGBA,GL_UNSIGNED_BYTE,image.rgba.data());
                glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);glBindTexture(GL_TEXTURE_2D,GLuint(binding));++opening_intro_uploads;
            }
            background->AddRectFilled({0,0},{w,h},IM_COL32(0,0,0,255));
            if(opening_intro_texture&&opening_intro_texture_width>0&&opening_intro_texture_height>0){
                const float fit=std::min(w/opening_intro_texture_width,h/opening_intro_texture_height);
                const float fw=opening_intro_texture_width*fit,fh=opening_intro_texture_height*fit;
                background->AddImage((ImTextureID)(intptr_t)opening_intro_texture,{(w-fw)*.5f,(h-fh)*.5f},{(w+fw)*.5f,(h+fh)*.5f});
            }
        }
    }
    auto menu=[&](const char *id,float width){
        const std::string panel=std::string(id)+std::to_string(preview_screen);
        const bool fresh=opening_panel!=panel;if(fresh){opening_panel=panel;ImGui::SetNextWindowFocus();}
        const bool pause_menu=preview_screen!=0;
        // Keep the title's short actions and help on screen at 640x360. A
        // narrow scaled panel forced extra text lines while the readable font
        // floor stayed fixed; its bottom then became an unnecessary scrollbar.
        if(!pause_menu)width=std::min(w-24*u,std::max(width,std::min(480.f,w*.72f)));
        const float menu_y=pause_menu?std::max(18.f*u,h*.08f):h*.29f;
        ImGui::SetNextWindowPos({(w-width)*.5f,menu_y});ImGui::SetNextWindowSize({width,0});
        ImGui::SetNextWindowSizeConstraints({width,0},{width,std::max(120.f,h-menu_y-18*u)});
        ImGui::PushStyleColor(ImGuiCol_WindowBg,{.025f,.035f,.043f,.96f});ImGui::PushStyleColor(ImGuiCol_Button,{.14f,.18f,.19f,1});ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{.26f,.31f,.29f,1});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,pause_menu?ImVec2{25*u,22*u}:ImVec2{std::ceil(20*u),std::ceil(14*u)});ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,pause_menu?ImVec2{10*u,12*u}:ImVec2{8*u,8*u});
        ImGui::Begin(id,nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::SetWindowFontScale(std::max(.85f,1.1f*scale));return fresh;
    };
    auto menu_button=[&](const char *label,ImVec2 size){
        const bool clicked=ImGui::Button(label,size);
        return clicked||(opening_menu_confirm&&ImGui::IsItemFocused());
    };
    auto end_menu=[&](){opening_menu_confirm=false;ImGui::End();ImGui::PopStyleVar(2);ImGui::PopStyleColor(3);};
    if(demo_presentation_enabled&&(preview_screen==4||preview_screen==5||preview_screen==6)){
        ImGui::GetBackgroundDrawList()->AddRectFilled({0,0},{w,h},IM_COL32(5,12,17,235));
        const bool controls=preview_screen==4,ending=preview_screen==5;
        const bool focus=menu("###demo-information",std::min(w-28*u,760*u));
        if(ending){
            ImGui::TextColored({.89f,.78f,.54f,1},"FIRST CHAPTER COMPLETE");
            ImGui::TextUnformatted("THANKS FOR PLAYING");ImGui::Separator();
            ImGui::TextWrapped("You found a place at the fire, recovered the stolen provisions and reached the Copper Lantern. Your chapter checkpoint is saved.");
            ImGui::TextWrapped("This is the end of this playtest. You can keep exploring or exit below.");ImGui::Spacing();
            ImGui::TextColored({.89f,.78f,.54f,1},"Let us know on Discord how the game was.");
            if(menu_button("CONTINUE EXPLORING",{-1,40*u})){preview_screen=1;opening_panel.clear();opening_release();}
            if(focus)ImGui::SetItemDefaultFocus();
            if(menu_button("EXIT DEMO",{-1,40*u}))running=false;
        }else{
            ImGui::TextUnformatted(controls?"HOW TO PLAY":"KNOWN PLAYTEST ISSUES");ImGui::Separator();
            if(controls){
                if(ImGui::RadioButton("Keyboard / mouse",!opening_controls_pad))opening_controls_pad=false;
                ImGui::SameLine();if(ImGui::RadioButton("Controller",opening_controls_pad))opening_controls_pad=true;
                world_demo_presentation::controls(opening_controls_pad,battlefield_enabled);
            }else ImGui::TextWrapped("%s",world_demo_presentation::known_issues(battlefield_enabled));
            ImGui::Spacing();if(menu_button("BACK",{-1,38*u})){preview_screen=opening_aux_return;opening_panel.clear();opening_release();}if(focus)ImGui::SetItemDefaultFocus();
        }
        end_menu();return;
    }
    if(preview_screen==0){
        if(demo_presentation_enabled){
            if(demo_logo)dl->AddImage((ImTextureID)(intptr_t)demo_logo,{30*u,22*u},{94*u,86*u});
            text(110*u,43*u,13,gold,"IRON OAK GAME STUDIO  |  EARLY PLAYTEST");
        }
        center(h*.12f,17,gold,"LEGENDS OF VALWOOD");center(h*.17f,36,white,game_story?"AN ORDINARY EVENING":"AN ORDINARY MORNING");
        bool focus=menu("###opening-title",500*u);
        ImGui::TextWrapped("%s",game_story?"Another shift. Another bill. One quiet evening to himself.":"An ordinary home. An unfamiliar world.");ImGui::Separator();
        if(ImGui::Button("BEGIN THE JOURNEY",{-1,42*u}))opening_new_journey();
        if(focus)ImGui::SetItemDefaultFocus();
        if(ImGui::IsItemFocused())opening_title_selection=0;
        if(opening_have_save){if(ImGui::Button("CONTINUE CHECKPOINT",{-1,42*u}))opening_load();if(ImGui::IsItemFocused())opening_title_selection=1;}
        if(demo_presentation_enabled){
            if(ImGui::Button("HOW TO PLAY  /  F1",{-1,36*u})){opening_aux_return=0;opening_controls_pad=pad;preview_screen=4;opening_panel.clear();opening_release();}if(ImGui::IsItemFocused())opening_title_selection=4;
            if(ImGui::Button("KNOWN PLAYTEST ISSUES",{-1,36*u})){opening_aux_return=0;preview_screen=6;opening_panel.clear();opening_release();}if(ImGui::IsItemFocused())opening_title_selection=5;
        }
        if(ImGui::Button("GRAPHICS AND DISPLAY",{-1,42*u})){show_graphics=true;graphics_draft=graphics;}
        if(ImGui::IsItemFocused())opening_title_selection=2;
        if(ImGui::Button("EXIT",{-1,42*u}))running=false;
        if(ImGui::IsItemFocused())opening_title_selection=3;
        ImGui::TextWrapped("Keyboard: arrows / Enter to choose. Controller: D-pad / confirm.");end_menu();return;
    }
    if(preview_screen==2||preview_screen==3){
        bool focus=menu("###opening-pause",580*u);ImGui::TextUnformatted(preview_screen==3?"JOURNAL":"PAUSED");ImGui::Separator();
        ImGui::TextWrapped("%s",opening_frame.chapter);ImGui::TextWrapped("%s",opening_frame.objective);
        if(game_story&&preview_screen==3){
            ImGui::Separator();ImGui::TextWrapped("Another late shift. Rent. Electricity. Groceries. I keep doing the sums. Every day feels the same, and I'm tired before it even starts.");
            if(opening_state.inspected())ImGui::TextWrapped("Rent due Friday. Electricity overdue. Extra shift tomorrow. Tonight I just need one hour that belongs to me.");
            if(opening_frame.stage==world_opening_prologue::Stage::MeadowReveal||opening_frame.stage==world_opening_prologue::Stage::MeadowExplore||opening_frame.stage==world_opening_prologue::Stage::Complete)ImGui::TextWrapped("I pressed New Game. There was a light... and now I can feel the grass. Valois was the name on the television. How am I here?");
            if(opening_state.config().arrival_camp.enabled&&opening_frame.stage>=world_opening_prologue::Stage::MeadowExplore){
                ImGui::Separator();ImGui::TextWrapped("Travelers met: %u / 3",world_opening_arrival_camp::completed(opening_state.camp().spoken()));
                if(opening_state.camp().spoken()&1u)ImGui::TextWrapped("Rowan offered a place by the fire. I'm in the Meadows of Valois.");
                if(opening_state.camp().spoken()&2u)ImGui::TextWrapped(first_journey_enabled?(first_journey_state.reward_granted()?"The provisions are back. Rowan sent me to Hugh at the Copper Lantern to ask about Mara.":first_journey_state.weapon_unlocked()?((battlefield_sword_owned&&battlefield_sword_owned())?"I kept the sword I escaped the battle with. Recover Rowan's stolen provisions, then return to the fire.":"Bram lent me a weapon. Recover Rowan's stolen provisions, then return to the fire."):"Bram knows the road. Speak to him again before leaving the fire."):"Bram said the waystone marks the road into town. Ask for Mara at the tavern.");
                if(opening_state.camp().spoken()&4u)ImGui::TextWrapped("Ellis helped me steady myself. One step at a time. I have to find a way home.");
            }
        }
        if(menu_button("RESUME",{-1,40*u})){preview_screen=1;opening_panel.clear();}if(focus)ImGui::SetItemDefaultFocus();
        if(inventory_menu_open&&menu_button("INVENTORY  /  I or View",{-1,36*u}))inventory_menu_open();
        if(!opening_frame.cinematic){if(menu_button("SAVE CHECKPOINT",{-1,36*u}))opening_save();if(menu_button("RESTART CHECKPOINT",{-1,36*u}))opening_retry();}
        if(opening_have_save&&menu_button("LOAD CHECKPOINT",{-1,36*u}))opening_load();
        if(menu_button("GRAPHICS AND DISPLAY",{-1,36*u})){show_graphics=true;graphics_draft=graphics;}
        if(demo_presentation_enabled){
            if(menu_button("HOW TO PLAY  /  F1",{-1,36*u})){opening_aux_return=2;opening_controls_pad=pad;preview_screen=4;opening_panel.clear();opening_release();}
            if(menu_button("KNOWN PLAYTEST ISSUES",{-1,36*u})){opening_aux_return=2;preview_screen=6;opening_panel.clear();opening_release();}
        }
        preview_combat_settings_ui();
        if(battlefield_pause_options)battlefield_pause_options();
        ImGui::Separator();
        ImGui::SliderFloat("Look sensitivity",&preview_settings.sensitivity,.3f,3.f);ImGui::SliderFloat("Stick deadzone",&preview_settings.deadzone,.08f,.35f);
        ImGui::SliderFloat(opening_state.config().house_shots.empty()?"Music":"Music and apartment sounds",&preview_settings.music,0.f,1.f);
        if(opening_state.config().npc_intro.enabled)ImGui::SliderFloat("Dialogue voice",&preview_settings.voice,0.f,1.f);
        ImGui::Checkbox("Subtitles",&preview_settings.subtitles);
        if(ImGui::Checkbox("Controller vibration",&preview_settings.vibration)&&!preview_settings.vibration)preview_pad.stop_rumble();
        ImGui::Checkbox("Invert look Y",&preview_settings.invert_y);ImGui::Checkbox("Reduced motion",&preview_settings.reduced_motion);
        if(menu_button("EXIT",{-1,36*u}))running=false;
        end_menu();return;
    }
    opening_panel.clear();
    if(loading&&demo_presentation_enabled){
        const bool outdoors=opening_frame.stage==world_opening_prologue::Stage::MeadowReveal||opening_frame.stage==world_opening_prologue::Stage::MeadowExplore||opening_frame.stage==world_opening_prologue::Stage::Complete;
        const bool field=battlefield_enabled&&battlefield_frame.active;
        world_demo_presentation::loading((ImTextureID)(intptr_t)demo_logo,outdoors?(field?"THE BROKEN FIELD":"THE MEADOWS OF VALOIS"):"AN ORDINARY EVENING",outdoors?(field?"Preparing the battlefield and escape route":"Preparing woodland, water and travelers"):"Preparing the apartment",outdoors,SDL_GetTicks64()*.001,field);return;
    }
    if(loading){const bool outdoors=opening_frame.stage==world_opening_prologue::Stage::MeadowReveal||opening_frame.stage==world_opening_prologue::Stage::MeadowExplore||opening_frame.stage==world_opening_prologue::Stage::Complete;center(h*.50f,24,white,outdoors?"THE MEADOWS OF VALOIS":(game_story?"AN ORDINARY EVENING":"PREPARING THE JOURNEY"));if(game_story)center(h*.56f,14,gold,outdoors?"A fire in the distance.":"Coming home.");return;}
    if(preview_screen==1&&!loading&&battlefield_draw_hud&&battlefield_draw_hud())return;
    if(opening_frame.stage==world_opening_prologue::Stage::GameMenu){
        // The television remains visible. Confirmation is handled by the
        // deterministic story gate, including a full release after entry.
        center(h*.69f,19,gold,"Choose NEW GAME to start playing.");
        ImGui::SetNextWindowPos({w*.5f,h*.775f},ImGuiCond_Always,{.5f,.5f});
        ImGui::SetNextWindowSize({360*u,0});ImGui::SetNextWindowBgAlpha(.88f);
        ImGui::Begin("###home-new-game",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoNavInputs);
        if(ImGui::Button("NEW GAME",{-1,42*u}))opening_game_click=true;
        ImGui::End();
        const std::string hint=pad?preview_pad.button(SDL_CONTROLLER_BUTTON_A)+std::string(": New Game     ")+preview_pad.button(SDL_CONTROLLER_BUTTON_B)+": Step away":"ENTER: New Game     ESC: Step away";
        center(h*.827f,14,white,hint.c_str());subtitle(opening_frame.subtitle);return;
    }
    if(opening_frame.npc_intro){
        const bool video=opening_intro_media.ready()&&!opening_intro_media.failed()&&opening_intro_media.mode()==world_npc_intro_media::Mode::Video;
        const auto *sample=npc_intro_qa&&npc_intro_qa_visual_sample?npc_intro_qa_visual_sample():nullptr;
        if(!video){
            text(42*u,38*u,13,gold,"LEGENDS OF VALWOOD");
        }
        if(preview_settings.subtitles){
            const char *caption=opening_intro_media.ready()&&!opening_intro_media.failed()?opening_intro_media.subtitle(sample?sample->time:opening_frame.npc_intro_time):(sample?sample->subtitle:opening_frame.subtitle);
            if(caption&&*caption){
                const float font_size=22*scale,wrap=std::min(w-96*u,1050*u);
                const auto size=ImGui::GetFont()->CalcTextSizeA(font_size,wrap,wrap,caption);
                const float x=(w-wrap)*.5f,y=h*.81f;
                dl->AddRectFilled({x-22*u,y-13*u},{x+wrap+22*u,y+size.y+15*u},IM_COL32(4,9,13,220),5*u);
                dl->AddText(ImGui::GetFont(),font_size,{x,y},white,caption,nullptr,wrap);
            }
        }
        const std::string hint="Hold "+skip+" to skip    "+(pad?preview_pad.button(SDL_CONTROLLER_BUTTON_START):std::string("ESC"))+": pause";
        text(32*u,h-38*u,14,white,hint.c_str());
        if(opening_frame.npc_intro_skip_progress>0)dl->AddRectFilled({32*u,h-15*u},{32*u+210*u*float(opening_frame.npc_intro_skip_progress),h-11*u},gold);
        return;
    }
    if(opening_frame.cinematic){
        const double p=opening_frame.stage_progress;
        const bool directed_home=opening_frame.stage==world_opening_prologue::Stage::HouseEstablish&&!opening_state.config().house_shots.empty();
        if(p>.12&&p<.78&&!game_transport&&(!directed_home||opening_frame.house_shot_index==3)){center(h*.76f,14,gold,"LEGENDS OF VALWOOD");center(h*.80f,28,white,opening_frame.chapter);}
        subtitle(opening_frame.subtitle);
        const std::string hint="Hold "+skip+" to skip";text(32*u,h-36*u,13,white,hint.c_str());
        if(opening_frame.skip_progress>0)dl->AddRectFilled({32*u,h-15*u},{32*u+180*u*float(opening_frame.skip_progress),h-12*u},gold);
        return;
    }
    if(opening_checkpoint_seconds>0&&!opening_frame.cinematic){
        const char *saved="CHECKPOINT SAVED";const float tw=ImGui::GetFont()->CalcTextSizeA(11*scale,10000,0,saved).x;
        text(w-tw-26*u,h-47*u,11,IM_COL32(226,198,137,int(185*std::min(1.f,opening_checkpoint_seconds))),saved);
    }
    if(first_journey_draw&&first_journey_draw(loading))return;
    if(opening_frame.camp_dialogue){
        const unsigned actor=unsigned(opening_frame.camp_actor);
        const std::string character=std::string(world_opening_arrival_camp::name(actor))+" | "+world_opening_arrival_camp::role(actor);
        text(32*u,32*u,15,gold,character.c_str());
        ImGui::SetNextWindowPos({w*.5f,h-24*u},ImGuiCond_Always,{.5f,1});
        ImGui::SetNextWindowSize({std::min(w-48*u,900*u),0});
        ImGui::SetNextWindowSizeConstraints({0,0},{w-48*u,h*.45f});ImGui::SetNextWindowBgAlpha(.95f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{24*u,18*u});
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{10*u,12*u});
        ImGui::Begin("###arrival-camp-dialogue",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoNavInputs);
        ImGui::SetWindowFontScale(std::max(.85f,1.15f*scale));
        ImGui::TextColored({.88f,.78f,.54f,1.f},"%s",opening_frame.camp_speaker);
        ImGui::TextWrapped("%s",opening_frame.camp_line);
        ImGui::Spacing();
        if(ImGui::Button("Continue",{180*u,38*u}))opening_camp_click=true;
        const unsigned beats=(opening_state.camp().spoken()&(1u<<actor))?2u:6u;
        const std::string controls=confirm+": continue    "+(pad?preview_pad.button(SDL_CONTROLLER_BUTTON_B):std::string("ESC"))+": leave conversation";
        ImGui::TextWrapped("%s   |   %u / %u",controls.c_str(),opening_state.camp().beat()+1,beats);ImGui::End();ImGui::PopStyleVar(2);return;
    }
    const bool camp_guidance=opening_state.config().arrival_camp.enabled&&
        opening_frame.stage==world_opening_prologue::Stage::MeadowExplore&&!opening_state.camp().complete();
    const auto objective_extent=ImGui::GetFont()->CalcTextSizeA(18*scale,10000,std::min(w-76*u,620*u),opening_frame.objective);
    const float objective_footer=59*u+objective_extent.y+10*u;
    const float card_bottom=objective_footer+(camp_guidance?36*u+12*scale+12*u:12*scale+12*u);
    dl->AddRectFilled({22*u,22*u},{std::min(w-22*u,680*u),card_bottom},IM_COL32(7,15,19,int(190*hud_alpha)),5*u);
    opening_hud_regions.add({22*u,22*u,std::min(w-22*u,680*u),card_bottom});
    text(38*u,34*u,13,gold,opening_frame.chapter);dl->AddText(ImGui::GetFont(),18*scale,{38*u,59*u},white,opening_frame.objective,nullptr,std::min(w-76*u,620*u));
    const double distance=std::hypot(cam.x-opening_frame.objective_position.x,cam.z-opening_frame.objective_position.z);
    const bool console_goal=game_story&&opening_frame.stage==world_opening_prologue::Stage::HouseExplore&&opening_state.inspected();
    if(!camp_guidance&&opening_frame.stage!=world_opening_prologue::Stage::Complete){char s[96];std::snprintf(s,sizeof s,console_goal?"TV / VIDEO GAME CONSOLE   %.0f m":"%.0f m",distance);text(38*u,objective_footer,12,gold,s);}
    if(camp_guidance){
        const auto &camp=opening_state.config().arrival_camp;
        const unsigned spoken=opening_state.camp().spoken();
        const int next=world_opening_arrival_camp::objective_actor(camp,{cam.x,cam.y-1.75,cam.z},spoken);
        if(next>=0){
            const auto &p=camp.actors[unsigned(next)].feet;
            const auto heading=world_journey_presentation::heading(world_opening_arrival_camp::name(unsigned(next)),p.x-cam.x,p.z-cam.z,cam.yaw);
            char label[160];std::snprintf(label,sizeof label,"Next: %s  |  %s  |  %.0f m",heading.c_str(),world_opening_arrival_camp::role(unsigned(next)),std::hypot(p.x-cam.x,p.z-cam.z));
            text(38*u,objective_footer,12,gold,label);
        }
        for(unsigned i=0;i<3;++i){
            const float x=41*u+i*std::min(182*u,(w-100*u)/3),y=objective_footer+36*u;
            const ImU32 c=(spoken&(1u<<i))?gold:IM_COL32(181,194,193,230);
            if(spoken&(1u<<i)){
                dl->AddLine({x-3*u,y},{x,y+3*u},c,1.5f*u);
                dl->AddLine({x,y+3*u},{x+6*u,y-4*u},c,1.5f*u);
            }else dl->AddCircle({x,y},4*u,c,12,u);
            text(x+13*u,y-7*u,12,c,world_opening_arrival_camp::name(i));
        }
        ++opening_camp_guidance_frames;
    }
    if(console_goal){
        // Pin the next action to the actual television, without turning or
        // moving the player's camera. Looking away keeps a directional hint.
        const auto &target=opening_state.config().transport_rail.target_from;
        const V3 f=eye.forward(),r=norm(cross(f,{0,1,0})),up=cross(r,f);
        const V3 delta={float(target.x-eye.x),float(target.y+.35-eye.y),float(target.z-eye.z)};
        const float depth=dot(delta,f),tangent=std::tan(eye.fov*.5f);
        const float px=depth>.1f?w*.5f+dot(delta,r)/depth/tangent*h*.5f:-1.f;
        const float py=depth>.1f?h*.5f-dot(delta,up)/depth/tangent*h*.5f:-1.f;
        if(depth>.1f&&px>48*u&&px<w-48*u&&py>130*u&&py<h*.68f){
            const float y=std::max(150*u,py);
            dl->AddQuadFilled({px,y-8*u},{px+8*u,y},{px,y+8*u},{px-8*u,y},gold);
            const char *label="TV / GAME CONSOLE";
            const float tw=ImGui::GetFont()->CalcTextSizeA(14*scale,10000,0,label).x;
            const float x=std::clamp(px-tw*.5f,16*u,w-tw-16*u);
            dl->AddRectFilled({x-9*u,y-40*u},{x+tw+9*u,y-15*u},IM_COL32(7,15,19,230),4*u);
            text(x,y-37*u,14,gold,label);
        }else{
            const double angle=std::remainder(std::atan2(target.x-eye.x,-(target.z-eye.z))-eye.yaw,6.28318530718);
            const char *hint=std::abs(angle)<.35?"LOOK TOWARD THE TV / GAME CONSOLE":angle<0?"<  TV / VIDEO GAME CONSOLE":"TV / VIDEO GAME CONSOLE  >";
            const float tw=ImGui::GetFont()->CalcTextSizeA(17*scale,10000,0,hint).x;
            dl->AddRectFilled({(w-tw)*.5f-12*u,h*.64f-7*u},{(w+tw)*.5f+12*u,h*.64f+23*u},IM_COL32(7,15,19,230),4*u);
            center(h*.64f,17,gold,hint);
        }
    }
    const float cx=w*.5f,cy=h*.5f;dl->AddCircleFilled({cx,cy},2*u,IM_COL32(245,241,226,int(200*hud_alpha)));
    const bool camp_prompt_stage=opening_state.config().arrival_camp.enabled&&
        (opening_frame.stage==world_opening_prologue::Stage::MeadowExplore||opening_frame.stage==world_opening_prologue::Stage::Complete);
    const bool prompt_in_view=!camp_prompt_stage||world_opening_arrival_camp::prompt_facing(
        opening_state.config().arrival_camp,{cam.x,cam.y-1.75,cam.z},cam.yaw,cam.pitch);
    if(opening_frame.interaction_available&&prompt_in_view){const std::string prompt="[ "+confirm+" ]  "+opening_frame.interaction;center(h*.72f,20,white,prompt.c_str());}
    else if(opening_room_light_target>=0){const unsigned room=unsigned(opening_room_light_target);const std::string prompt="[ "+confirm+" ]  "+(world_opening_room_lights::on(opening_state.room_lights(),room)?"Switch off ":"Switch on ")+world_opening_room_lights::switches[room].name+" lights";center(h*.72f,20,white,prompt.c_str());}
    if(opening_notice_seconds>0)center(h*.84f,15,gold,opening_notice.c_str());
    if(game_story&&opening_bills_seconds>0&&opening_frame.stage==world_opening_prologue::Stage::HouseExplore){
        const float left=w-410*u,top=145*u;
        dl->AddRectFilled({left,top},{w-30*u,top+246*u},IM_COL32(224,215,192,245),3*u);
        const ImU32 ink=IM_COL32(40,44,43,255),red=IM_COL32(132,47,37,255);
        text(left+22*u,top+18*u,13,red,"DUE / OVERDUE");
        text(left+22*u,top+52*u,18,ink,"RENT - due Friday");
        text(left+22*u,top+82*u,18,ink,"Electricity - final reminder");
        dl->AddLine({left+22*u,top+116*u},{w-52*u,top+116*u},IM_COL32(94,91,81,130));
        text(left+22*u,top+132*u,16,ink,"Tomorrow: another extra shift.");
        text(left+22*u,top+170*u,15,ink,"A video game. Just one hour to myself.");
        text(left+22*u,top+212*u,12,ink,pad?"Y / Triangle: reread in journal":"J: reread in journal");
    }
    const bool apartment_controls=opening_frame.stage<=world_opening_prologue::Stage::Transport||opening_frame.stage==world_opening_prologue::Stage::GameMenu;
    const std::string controls=pad?std::string("Left stick: move   Right stick: look   ")+(apartment_controls?std::string():std::string(preview_pad.button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER))+": sprint   ")+preview_pad.button(SDL_CONTROLLER_BUTTON_X)+": jump   "+preview_pad.button(SDL_CONTROLLER_BUTTON_A)+": interact   "+preview_pad.button(SDL_CONTROLLER_BUTTON_START)+": pause":(apartment_controls?"WASD: move   Mouse: look   Space: jump   E: interact   Esc: pause":"WASD: move   Mouse: look   Shift: sprint   Space: jump   E: interact   Esc: pause");
    text(30*u,h-34*u,12,white,controls.c_str());
};
auto opening_report=[&](FILE *f){
    const auto s=opening_state.stats();
    std::fprintf(f,"  \"npc_introduction\": {\"enabled\": %s, \"seen\": %s, \"starts\": %u, \"completions\": %u, \"frames\": %u, \"shot_mask\": %u, \"camera_fallbacks\": %u, \"video_uploads\": %u, \"native_resolution_fallback\": %s},\n",opening_state.config().npc_intro.enabled?"true":"false",opening_state.npc_intro_seen()?"true":"false",opening_intro_starts,opening_intro_completions,opening_intro_frames,opening_intro_shots,opening_intro_camera_fallbacks,opening_intro_uploads,opening_intro_resolution_failed?"true":"false");
    std::fprintf(f,"  \"opening_arrival_camp\": {\"enabled\": %s, \"spoken_mask\": %u, \"completed\": %u, \"dialogue_active\": %s, \"actor\": %d, \"beat\": %u},\n",opening_state.config().arrival_camp.enabled?"true":"false",opening_state.camp().spoken(),world_opening_arrival_camp::completed(opening_state.camp().spoken()),opening_state.camp().active()?"true":"false",opening_state.camp().actor(),opening_state.camp().beat());
    std::fprintf(f,"  \"opening_prologue\": {\"enabled\": %s, \"stage\": %s, \"first_person\": %s, \"arrivals\": %u, \"saves\": %u, \"restores\": %u, \"retries\": %u, \"skips\": %u, \"active_seconds\": %.4f, \"playable_seconds\": %.4f},\n",opening_enabled?"true":"false",world_json::quote(world_opening_prologue::stage_name(opening_state.stage())).c_str(),!third_person?"true":"false",opening_arrivals,opening_saves,opening_restores,opening_retries,s.skips,s.active_seconds,s.playable_seconds);
    std::fprintf(f,"  \"opening_room_lights\": {\"enabled\": %s, \"mask\": %u, \"target\": %d, \"bindings\": %zu, \"toggles\": [%u,%u,%u,%u], \"powers\": [",opening_room_lights_enabled?"true":"false",opening_state.room_lights(),opening_room_light_target,opening_room_light_bindings.size(),opening_room_light_controls.toggles[0],opening_room_light_controls.toggles[1],opening_room_light_controls.toggles[2],opening_room_light_controls.toggles[3]);
    for(size_t i=0;i<opening_room_light_bindings.size();++i){const auto &b=opening_room_light_bindings[i];std::fprintf(f,"%s%.5f",i?",":"",b.index<studio.live.lights.size()?studio.live.lights[b.index].power:-1.f);}
    std::fprintf(f,"]},\n");
    std::fprintf(f,"  \"opening_game_start\": {\"enabled\": %s, \"menus\": %u, \"new_games\": %u, \"screen_mode\": %.0f},\n",opening_state.config().game_start_story?"true":"false",opening_game_menus,opening_new_games,world_opening_game_screen::state[1]);
    std::fprintf(f,"  \"opening_cinematic_entry\": {\"enabled\": %s, \"shots\": %zu, \"duration\": %.3f, \"audio_available\": %s, \"audio_starts\": %u, \"cue_mask\": %u, \"cue_starts\": [%u,%u,%u,%u]},\n",opening_state.config().house_shots.empty()?"false":"true",opening_state.config().house_shots.size(),world_opening_prologue::house_duration(opening_state.config()),opening_cinematic_audio_was_ready?"true":"false",opening_cinematic_audio.starts,opening_cinematic_cue_mask,opening_cinematic_audio.cue_starts[0],opening_cinematic_audio.cue_starts[1],opening_cinematic_audio.cue_starts[2],opening_cinematic_audio.cue_starts[3]);
};

// Isolated opening host: authored cameras never move the playable capsule.
world_opening_prologue::State opening_state;
world_opening_prologue::Frame opening_frame;
std::unique_ptr<world_playtest_walk::Support> opening_house_walk;
bool opening_confirm=false,opening_interact=false,opening_jump=false,opening_begin_requested=false;
bool opening_cancel=false,opening_enter=false,opening_game_click=false,opening_game_cancel_click=false,opening_camp_click=false;
int opening_previous_camp_actor=-1;
float opening_bills_seconds=0;
unsigned opening_game_menus=0,opening_new_games=0;
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
unsigned opening_arrivals=0,opening_restores=0,opening_retries=0,opening_saves=0;
std::string opening_notice,opening_panel;
float opening_notice_seconds=0;
auto opening_tell=[&](const std::string &s){opening_notice=s;opening_notice_seconds=5;};
auto opening_release=[&](){
    preview_release_mouse();preview_pad.clear();fight.set_guard(false);
    opening_jump=false;opening_movement_armed=false;
    opening_room_light_controls.release();opening_room_light_target=-1;
};
auto opening_save=[&]()->bool{
    if(!opening_enabled)return false;
    std::string bytes,error;
    auto checkpoint=opening_state.stable_save();
    if(first_journey_attach_save)first_journey_attach_save(checkpoint);
    if(!world_opening_prologue::encode_save(checkpoint,opening_state.config(),bytes,error)){opening_tell(error);return false;}
    std::error_code ec;std::filesystem::create_directories("saves",ec);
    if(ec){opening_tell("Could not create the checkpoint folder.");return false;}
    std::string pattern="saves/opening.progress.tmp-XXXXXX";std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);
    const int fd=mkstemp(name.data());if(fd<0){opening_tell("Could not stage this checkpoint.");return false;}
    FILE *f=fdopen(fd,"wb");bool ok=false;
    if(f){ok=std::fwrite(bytes.data(),1,bytes.size(),f)==bytes.size()&&std::fflush(f)==0&&fsync(fd)==0;if(std::fclose(f))ok=false;}else close(fd);
    if(ok)ok=vw_replace(name.data(),"saves/opening.progress")==0;
    if(!ok){unlink(name.data());opening_tell("Could not commit this checkpoint.");return false;}
    opening_have_save=true;++opening_saves;if(first_journey_enabled)++first_journey_saves;opening_tell("Checkpoint saved");return true;
};
auto opening_place=[&](const world_opening_prologue::Pose &p,bool house)->bool{
    float floor=0;
    const bool supported=house?opening_house_walk&&opening_house_walk->sample(p.feet.x,p.feet.z,p.feet.y,floor):(floor=std::max(world_atlas_walk_height(p.feet.x,p.feet.z),0.f),true);
    if(!supported||!std::isfinite(floor)||std::abs(floor-p.feet.y)>.18){opening_tell("The opening checkpoint has no supported floor.");return false;}
    active_playtest_walk=house?opening_house_walk.get():nullptr;playtest_walk_reference=p.feet.y;
    sun_strength=house?.16f:opening_meadow_sun;sky_fill=house?.85f:opening_meadow_sky;exposure=house?1.12f:opening_meadow_exposure;
    cam.x=p.feet.x;cam.y=p.feet.y+1.75;cam.z=p.feet.z;cam.yaw=float(p.yaw);cam.pitch=float(p.pitch);cam.fov=1.04719755f;
    cam.fly=false;cam.grounded=true;cam.vy=0;third_person=false;avatar_initialized=false;
    studio.open=false;show_tools=show_map=show_graphics=journey_open=false;
    milestone_review.view=world_milestone::View::None;portrait_focus_distance=0;
    if(studio.components_runtime)studio.components_runtime->close_dialogue();
    fight=WorldTrainingCombat{};live_training_encounter=0;preview_pending_save=false;
    current_name=house?(opening_state.config().game_start_story?"An Ordinary Evening":"An Ordinary Morning"):"The Meadows of Valois";
    destination_loading=true;temporal_cut=true;opening_release();return true;
};
auto opening_load=[&]()->bool{
    std::ifstream file("saves/opening.progress");world_opening_prologue::Save saved;std::string error;
    if(!file||!world_opening_prologue::parse_save(file,opening_state.config(),saved,error)){opening_tell(error.empty()?"No opening checkpoint yet.":error);return false;}
    world_opening_prologue::State staged=opening_state;
    if(!staged.restore(saved,error)){opening_tell(error);return false;}
    if(first_journey_restore_save&&!first_journey_restore_save(saved,error)){opening_tell(error);return false;}
    const bool house=saved.checkpoint<=world_opening_prologue::Checkpoint::HouseInspected;
    auto pose=saved.player;if(!house&&first_journey_checkpoint_pose)first_journey_checkpoint_pose(pose);
    if(!opening_place(pose,house))return false;
    opening_state=std::move(staged);opening_frame=opening_state.sample({cam.x,cam.y-1.75,cam.z},preview_settings.reduced_motion);
    if(first_journey_commit_restore)first_journey_commit_restore();
    opening_apply_room_lights();
    opening_controls_enabled=false;opening_last_playable=false;preview_screen=1;opening_have_save=true;++opening_restores;if(first_journey_enabled)++first_journey_restores;
    opening_tell("Checkpoint restored");return true;
};
auto opening_retry=[&](){
    auto saved=opening_state.stable_save();if(first_journey_attach_save)first_journey_attach_save(saved);
    std::string error;auto staged=opening_state;
    if(!staged.restore(saved,error)||(first_journey_restore_save&&!first_journey_restore_save(saved,error))){opening_tell(error);return;}
    auto pose=saved.player;const bool house=saved.checkpoint<=world_opening_prologue::Checkpoint::HouseInspected;
    if(!house&&first_journey_checkpoint_pose)first_journey_checkpoint_pose(pose);
    if(!opening_place(pose,house)){opening_tell("Checkpoint could not be safely restarted.");return;}
    opening_state=std::move(staged);if(first_journey_commit_restore)first_journey_commit_restore();
    opening_frame=opening_state.sample({cam.x,cam.y-1.75,cam.z},preview_settings.reduced_motion);
    opening_apply_room_lights();
    opening_controls_enabled=false;opening_last_playable=false;preview_screen=1;++opening_retries;opening_tell("Checkpoint restarted");
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
    if(key==SDLK_ESCAPE){
        if(!show_graphics&&preview_screen==1&&(opening_state.camp().active()||first_journey_dialogue_active))return true;
        if(show_graphics)show_graphics=false;
        else if(preview_screen==1&&opening_state.stage()==world_opening_prologue::Stage::GameMenu){opening_game_cancel_click=true;return true;}
        else if(preview_screen==0){/* Title owns the exit button. */}
        else preview_screen=preview_screen==1?2:1;
        opening_release();return true;
    }
    if(show_graphics){if(key==SDLK_F10)show_graphics=false;return true;}
    if(key==SDLK_F10){show_graphics=true;graphics_draft=graphics;opening_release();return true;}
    if(preview_screen==0){
        // Enter activates the same focused action as controller confirm.
        // SDL's synthetic/first-frame Enter is not guaranteed to activate an
        // ImGui button, so keep its selected action explicit.
        if(key==SDLK_RETURN||key==SDLK_KP_ENTER){
            if(opening_title_selection==0){opening_state.reset();opening_begin_requested=true;preview_screen=1;opening_panel.clear();opening_bills_seconds=0;}
            else if(opening_title_selection==1)opening_load();
            else if(opening_title_selection==2){show_graphics=true;graphics_draft=graphics;}
            else if(opening_title_selection==3)running=false;
        }
        return true;
    }
    if((preview_screen==2||preview_screen==3)&&!show_graphics&&(key==SDLK_RETURN||key==SDLK_KP_ENTER)){opening_menu_confirm=true;return true;}
    if(key==SDLK_j||key==SDLK_TAB){preview_screen=preview_screen==3?1:3;opening_release();return true;}
    if(key==SDLK_F5){if(!opening_frame.cinematic)opening_save();return true;}
    if(key==SDLK_F8){opening_load();return true;}
    if(key==SDLK_r&&preview_screen==1&&!opening_frame.cinematic){if(first_journey_enabled&&first_journey_defeated){first_journey_retry_down=true;return true;}opening_retry();return true;}
    if(preview_screen!=1||show_graphics||destination_loading)return true;
    if(opening_state.stage()==world_opening_prologue::Stage::GameMenu)return true;
    if(first_journey_enabled&&opening_controls_enabled&&first_journey_state.weapon_unlocked()&&key==SDLK_v)return false;
    if(first_journey_enabled&&fight.engaged()&&(key==SDLK_LEFT||key==SDLK_RIGHT||key==SDLK_UP||key==SDLK_DOWN||key==SDLK_t||key==SDLK_SPACE))return false;
    if(key==SDLK_SPACE){if(opening_controls_enabled)opening_jump=true;return true;}
    if(key==SDLK_e||key==SDLK_RETURN||key==SDLK_KP_ENTER)return true;
    // Block authoring, atlas travel and hero/camera swaps in the opening copy.
    return key!=SDLK_w&&key!=SDLK_a&&key!=SDLK_s&&key!=SDLK_d&&key!=SDLK_LSHIFT;
};
opening_tick=[&](float delta){
    if(!opening_enabled)return;
    opening_notice_seconds=std::max(0.f,opening_notice_seconds-delta);
    const bool paused=(preview_screen!=1&&preview_screen!=0)||show_graphics||destination_loading||!preview_pad.focused||(SDL_GetWindowFlags(window)&SDL_WINDOW_MINIMIZED);
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
    if(first_journey_enabled&&first_journey_state.stage()!=world_first_journey::Stage::Travelers&&!opening_state.camp().active())input.interact_down=false;
    opening_frame=opening_state.tick(input);opening_begin_requested=false;opening_game_click=false;opening_game_cancel_click=false;opening_camp_click=false;
    const int camp_actor=opening_frame.camp_dialogue?opening_frame.camp_actor:-1;
    if(camp_actor!=opening_previous_camp_actor){temporal_cut=true;opening_previous_camp_actor=camp_actor;}
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
    if(events&world_opening_prologue::Started){opening_cinematic_cue_mask=0;opening_hud_reveal=0;if(first_journey_reset)first_journey_reset();}
    if(!paused&&opening_frame.stage==OpeningStage::HouseExplore)opening_hud_reveal=std::min(1.f,opening_hud_reveal+delta/.8f);
    if(directed_home&&!paused&&opening_frame.stage==OpeningStage::HouseEstablish){
        if(!(opening_cinematic_cue_mask&1u)){opening_cinematic_audio.play(WorldOpeningCinematicAudio::Cue::DoorClose,.9f);opening_cinematic_cue_mask|=1u;}
        if(opening_state.elapsed()>=1.05&&!(opening_cinematic_cue_mask&2u)){opening_cinematic_audio.play(WorldOpeningCinematicAudio::Cue::BagDown,.9f);opening_cinematic_cue_mask|=2u;}
    }
    if(events&world_opening_prologue::Started)preview_screen=1;
    if(events&world_opening_prologue::Arrived){
        if(!opening_place(opening_state.config().meadow_spawn,false)){launch->exit_code=1;running=false;return;}
        ++opening_arrivals;preview_audio.cue(2);
    }
    if(opening_frame.release_controls)opening_release();
    if(events&world_opening_prologue::GameOpened){++opening_game_menus;opening_bills_seconds=0;if(directed_home)opening_cinematic_audio.play(WorldOpeningCinematicAudio::Cue::ConsolePower,.85f);else preview_audio.cue(4);}
    if(events&world_opening_prologue::TransportStarted){if(game_story)++opening_new_games;preview_audio.cue(4);}
    if(events&world_opening_prologue::TransportPulse){preview_audio.cue(2);if(preview_settings.vibration)preview_pad.rumble(.15f,.35f,180,true);}
    if(events&world_opening_prologue::Inspected){if(game_story){opening_bills_seconds=9;opening_tell("I need a break. I'll play a video game on the TV.");}else opening_tell("A quiet morning. Check the front door.");}
    if(events&world_opening_prologue::CheckpointReached)opening_save();
    if(game_story&&(events&world_opening_prologue::Completed))opening_tell("Valois. The name on the screen. How am I standing here?");
    if(toggled_room>=0){
        // A toggle is deliberately saved at the same supported story
        // checkpoint as F5; it never serializes a camera or arbitrary pose.
        const bool saved=opening_save();
        if(saved)opening_tell(std::string(world_opening_room_lights::switches[unsigned(toggled_room)].name)+" lights "+(world_opening_room_lights::on(opening_state.room_lights(),unsigned(toggled_room))?"on":"off"));
    }
    if(first_journey_tick)first_journey_tick(first_journey_input);
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
auto opening_render_camera=[&](Camera &eye){
    if(!opening_enabled)return;
    portrait_focus_distance=0;
    if(!opening_frame.cinematic&&!opening_frame.camp_dialogue&&opening_frame.stage!=world_opening_prologue::Stage::Title&&opening_frame.stage!=world_opening_prologue::Stage::GameMenu)return;
    auto c=opening_frame.camera;
    if(opening_frame.camp_dialogue){
        // A stable waist-up composition moves the render eye only. If that
        // short adjustment intersects scenery, keep the actual player's eye.
        const bool safe=world_scenery_can_move(scenery,cam.x,cam.z,c.eye.x,c.eye.z,float(std::min(cam.y,c.eye.y))-.15f,.3f,.16f)&&c.eye.y>world_atlas_walk_height(c.eye.x,c.eye.z)+.75;
        if(!safe)c.eye={cam.x,cam.y,cam.z};
    }
    const double dx=c.target.x-c.eye.x,dy=c.target.y-c.eye.y,dz=c.target.z-c.eye.z;
    eye.x=c.eye.x;eye.y=c.eye.y;eye.z=c.eye.z;eye.yaw=float(std::atan2(dx,-dz));eye.pitch=float(std::atan2(dy,std::max(.001,std::hypot(dx,dz))));eye.fov=float(c.fov);
    if(!preview_settings.reduced_motion&&!opening_state.config().house_shots.empty()&&opening_frame.stage==world_opening_prologue::Stage::HouseEstablish&&opening_frame.house_shot_index==1)
        portrait_focus_distance=float(std::sqrt(dx*dx+dy*dy+dz*dz));
};
opening_draw=[&](const Camera &eye,bool loading){
    if(!opening_enabled||show_graphics)return;
    auto &io=ImGui::GetIO();const float w=io.DisplaySize.x,h=io.DisplaySize.y;
    const float u=std::clamp(std::min(w/1600.f,h/900.f),.65f,1.5f),scale=u*preview_settings.text_scale;
    auto *dl=ImGui::GetForegroundDrawList();
    const float hud_alpha=opening_frame.stage==world_opening_prologue::Stage::HouseExplore&&!opening_state.config().house_shots.empty()?float(world_opening_prologue::detail::ease(opening_hud_reveal)):1.f;
    const ImU32 white=IM_COL32(244,238,222,int(255*hud_alpha)),gold=IM_COL32(226,198,137,int(255*hud_alpha));
    auto text=[&](float x,float y,float size,ImU32 color,const char *value){dl->AddText(ImGui::GetFont(),size*scale,{x,y},color,value);};
    auto center=[&](float y,float size,ImU32 color,const char *value){const float tw=ImGui::GetFont()->CalcTextSizeA(size*scale,10000,0,value).x;text((w-tw)*.5f,y,size,color,value);};
    const bool pad=preview_pad.using_pad&&preview_pad.get();
    const std::string confirm=pad?preview_pad.button(SDL_CONTROLLER_BUTTON_A):"E";
    const std::string skip=pad?preview_pad.button(SDL_CONTROLLER_BUTTON_A):"ENTER / SPACE";
    const float bars=float(opening_frame.camera.letterbox)*h;
    if(bars>0){ImGui::GetBackgroundDrawList()->AddRectFilled({0,0},{w,bars},IM_COL32(0,0,0,255));ImGui::GetBackgroundDrawList()->AddRectFilled({0,h-bars},{w,h},IM_COL32(0,0,0,255));}
    const bool game_story=opening_state.config().game_start_story;
    const bool game_transport=game_story&&opening_frame.stage==world_opening_prologue::Stage::Transport;
    const bool game_arrival=game_story&&opening_frame.stage==world_opening_prologue::Stage::MeadowReveal;
    const float fade=loading&&preview_screen==1?1.f:(game_transport?float(world_opening_prologue::detail::ease((opening_frame.stage_progress-.48)/.43)):float(opening_frame.camera.fade));
    if(fade>0)ImGui::GetBackgroundDrawList()->AddRectFilled({0,0},{w,h},(game_transport||game_arrival)&&!loading?IM_COL32(225,243,249,int(std::clamp(fade,0.f,1.f)*255)):IM_COL32(0,0,0,int(std::clamp(fade,0.f,1.f)*255)));
    auto subtitle=[&](const char *value){
        if(!value||!*value)return;
        const float size=19*scale;const auto ts=ImGui::GetFont()->CalcTextSizeA(size,w-90*u,0,value);
        dl->AddRectFilled({(w-ts.x)*.5f-18*u,h*.875f-9*u},{(w+ts.x)*.5f+18*u,h*.875f+ts.y+9*u},IM_COL32(5,10,14,220),4*u);
        center(h*.875f,19,white,value);
    };
    auto menu=[&](const char *id,float width){
        const std::string panel=std::string(id)+std::to_string(preview_screen);
        const bool fresh=opening_panel!=panel;if(fresh){opening_panel=panel;ImGui::SetNextWindowFocus();}
        ImGui::SetNextWindowPos({(w-width)*.5f,h*.29f});ImGui::SetNextWindowSize({width,0});
        ImGui::PushStyleColor(ImGuiCol_WindowBg,{.025f,.035f,.043f,.96f});ImGui::PushStyleColor(ImGuiCol_Button,{.14f,.18f,.19f,1});ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{.26f,.31f,.29f,1});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{25*u,22*u});ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{10*u,12*u});
        ImGui::Begin(id,nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::SetWindowFontScale(std::max(.85f,1.1f*scale));return fresh;
    };
    auto menu_button=[&](const char *label,ImVec2 size){
        const bool clicked=ImGui::Button(label,size);
        return clicked||(opening_menu_confirm&&ImGui::IsItemFocused());
    };
    auto end_menu=[&](){opening_menu_confirm=false;ImGui::End();ImGui::PopStyleVar(2);ImGui::PopStyleColor(3);};
    if(preview_screen==0){
        center(h*.12f,17,gold,"LEGENDS OF VALWOOD");center(h*.17f,36,white,game_story?"AN ORDINARY EVENING":"AN ORDINARY MORNING");
        bool focus=menu("###opening-title",500*u);
        ImGui::TextWrapped("%s",game_story?"Another shift. Another bill. One quiet evening to himself.":"An ordinary home. An unfamiliar world.");ImGui::Separator();
        if(ImGui::Button("BEGIN THE JOURNEY",{-1,42*u})){opening_state.reset();opening_begin_requested=true;preview_screen=1;opening_panel.clear();opening_bills_seconds=0;}
        if(focus)ImGui::SetItemDefaultFocus();
        if(ImGui::IsItemFocused())opening_title_selection=0;
        if(opening_have_save){if(ImGui::Button("CONTINUE CHECKPOINT",{-1,42*u}))opening_load();if(ImGui::IsItemFocused())opening_title_selection=1;}
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
                if(opening_state.camp().spoken()&2u)ImGui::TextWrapped("Bram said the waystone marks the road into town. Ask for Mara at the tavern.");
                if(opening_state.camp().spoken()&4u)ImGui::TextWrapped("Ellis helped me steady myself. One step at a time. I have to find a way home.");
            }
        }
        if(menu_button("RESUME",{-1,40*u})){preview_screen=1;opening_panel.clear();}if(focus)ImGui::SetItemDefaultFocus();
        if(!opening_frame.cinematic){if(menu_button("SAVE CHECKPOINT",{-1,36*u}))opening_save();if(menu_button("RESTART CHECKPOINT",{-1,36*u}))opening_retry();}
        if(opening_have_save&&menu_button("LOAD CHECKPOINT",{-1,36*u}))opening_load();
        if(menu_button("GRAPHICS AND DISPLAY",{-1,36*u})){show_graphics=true;graphics_draft=graphics;}
        ImGui::SliderFloat("Look sensitivity",&preview_settings.sensitivity,.3f,3.f);ImGui::SliderFloat("Stick deadzone",&preview_settings.deadzone,.08f,.35f);
        ImGui::SliderFloat(opening_state.config().house_shots.empty()?"Music":"Music and apartment sounds",&preview_settings.music,0.f,1.f);
        if(ImGui::Checkbox("Controller vibration",&preview_settings.vibration)&&!preview_settings.vibration)preview_pad.stop_rumble();
        ImGui::Checkbox("Invert look Y",&preview_settings.invert_y);ImGui::Checkbox("Reduced motion",&preview_settings.reduced_motion);
        if(menu_button("EXIT",{-1,36*u}))running=false;
        end_menu();return;
    }
    opening_panel.clear();
    if(loading){const bool outdoors=opening_frame.stage==world_opening_prologue::Stage::MeadowReveal||opening_frame.stage==world_opening_prologue::Stage::MeadowExplore||opening_frame.stage==world_opening_prologue::Stage::Complete;center(h*.50f,24,white,outdoors?"THE MEADOWS OF VALOIS":(game_story?"AN ORDINARY EVENING":"PREPARING THE JOURNEY"));if(game_story)center(h*.56f,14,gold,outdoors?"A fire in the distance.":"Coming home.");return;}
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
    if(opening_frame.cinematic){
        const double p=opening_frame.stage_progress;
        const bool directed_home=opening_frame.stage==world_opening_prologue::Stage::HouseEstablish&&!opening_state.config().house_shots.empty();
        if(p>.12&&p<.78&&!game_transport&&(!directed_home||opening_frame.house_shot_index==3)){center(h*.76f,14,gold,"LEGENDS OF VALWOOD");center(h*.80f,28,white,opening_frame.chapter);}
        subtitle(opening_frame.subtitle);
        const std::string hint="Hold "+skip+" to skip";text(32*u,h-36*u,13,white,hint.c_str());
        if(opening_frame.skip_progress>0)dl->AddRectFilled({32*u,h-15*u},{32*u+180*u*float(opening_frame.skip_progress),h-12*u},gold);
        return;
    }
    if(first_journey_draw&&first_journey_draw(loading))return;
    if(opening_frame.camp_dialogue){
        const unsigned actor=unsigned(opening_frame.camp_actor);
        const std::string character=std::string(world_opening_arrival_camp::name(actor))+" | "+world_opening_arrival_camp::role(actor);
        text(32*u,32*u,15,gold,character.c_str());
        ImGui::SetNextWindowPos({w*.5f,h*.75f},ImGuiCond_Always,{.5f,0});
        ImGui::SetNextWindowSize({std::min(w-48*u,900*u),0});ImGui::SetNextWindowBgAlpha(.94f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{24*u,18*u});
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{10*u,12*u});
        ImGui::Begin("###arrival-camp-dialogue",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoNavInputs);
        ImGui::SetWindowFontScale(std::max(.85f,1.15f*scale));
        ImGui::TextColored({.88f,.78f,.54f,1.f},"%s",opening_frame.camp_speaker);
        ImGui::TextWrapped("%s",opening_frame.camp_line);
        if(ImGui::Button("Continue",{155*u,34*u}))opening_camp_click=true;
        ImGui::SameLine();const std::string controls=confirm+": continue    "+(pad?preview_pad.button(SDL_CONTROLLER_BUTTON_B):std::string("ESC"))+": leave conversation";
        ImGui::TextUnformatted(controls.c_str());ImGui::End();ImGui::PopStyleVar(2);return;
    }
    dl->AddRectFilled({22*u,22*u},{std::min(w-22*u,680*u),112*u},IM_COL32(7,15,19,int(190*hud_alpha)),5*u);
    text(38*u,34*u,13,gold,opening_frame.chapter);dl->AddText(ImGui::GetFont(),18*scale,{38*u,59*u},white,opening_frame.objective,nullptr,std::min(w-76*u,620*u));
    const double distance=std::hypot(cam.x-opening_frame.objective_position.x,cam.z-opening_frame.objective_position.z);
    const bool console_goal=game_story&&opening_frame.stage==world_opening_prologue::Stage::HouseExplore&&opening_state.inspected();
    if(opening_frame.stage!=world_opening_prologue::Stage::Complete){char s[96];std::snprintf(s,sizeof s,console_goal?"TV / VIDEO GAME CONSOLE   %.0f m":"%.0f m",distance);text(38*u,87*u,12,gold,s);}
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
    const std::string controls=pad?std::string("Left stick: move   Right stick: look   ")+preview_pad.button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER)+": sprint   "+preview_pad.button(SDL_CONTROLLER_BUTTON_X)+": jump   "+preview_pad.button(SDL_CONTROLLER_BUTTON_A)+": interact   "+preview_pad.button(SDL_CONTROLLER_BUTTON_START)+": pause":"WASD: move   Mouse: look   Shift: sprint   Space: jump   E: interact   Esc: pause";
    text(30*u,h-34*u,12,white,controls.c_str());
};
auto opening_report=[&](FILE *f){
    const auto s=opening_state.stats();
    std::fprintf(f,"  \"opening_arrival_camp\": {\"enabled\": %s, \"spoken_mask\": %u, \"completed\": %u, \"dialogue_active\": %s, \"actor\": %d, \"beat\": %u},\n",opening_state.config().arrival_camp.enabled?"true":"false",opening_state.camp().spoken(),world_opening_arrival_camp::completed(opening_state.camp().spoken()),opening_state.camp().active()?"true":"false",opening_state.camp().actor(),opening_state.camp().beat());
    std::fprintf(f,"  \"opening_prologue\": {\"enabled\": %s, \"stage\": %s, \"first_person\": %s, \"arrivals\": %u, \"saves\": %u, \"restores\": %u, \"retries\": %u, \"skips\": %u, \"active_seconds\": %.4f, \"playable_seconds\": %.4f},\n",opening_enabled?"true":"false",world_json::quote(world_opening_prologue::stage_name(opening_state.stage())).c_str(),!third_person?"true":"false",opening_arrivals,opening_saves,opening_restores,opening_retries,s.skips,s.active_seconds,s.playable_seconds);
    std::fprintf(f,"  \"opening_room_lights\": {\"enabled\": %s, \"mask\": %u, \"target\": %d, \"bindings\": %zu, \"toggles\": [%u,%u,%u,%u], \"powers\": [",opening_room_lights_enabled?"true":"false",opening_state.room_lights(),opening_room_light_target,opening_room_light_bindings.size(),opening_room_light_controls.toggles[0],opening_room_light_controls.toggles[1],opening_room_light_controls.toggles[2],opening_room_light_controls.toggles[3]);
    for(size_t i=0;i<opening_room_light_bindings.size();++i){const auto &b=opening_room_light_bindings[i];std::fprintf(f,"%s%.5f",i?",":"",b.index<studio.live.lights.size()?studio.live.lights[b.index].power:-1.f);}
    std::fprintf(f,"]},\n");
    std::fprintf(f,"  \"opening_game_start\": {\"enabled\": %s, \"menus\": %u, \"new_games\": %u, \"screen_mode\": %.0f},\n",opening_state.config().game_start_story?"true":"false",opening_game_menus,opening_new_games,world_opening_game_screen::state[1]);
    std::fprintf(f,"  \"opening_cinematic_entry\": {\"enabled\": %s, \"shots\": %zu, \"duration\": %.3f, \"audio_available\": %s, \"audio_starts\": %u, \"cue_mask\": %u, \"cue_starts\": [%u,%u,%u,%u]},\n",opening_state.config().house_shots.empty()?"false":"true",opening_state.config().house_shots.size(),world_opening_prologue::house_duration(opening_state.config()),opening_cinematic_audio_was_ready?"true":"false",opening_cinematic_audio.starts,opening_cinematic_cue_mask,opening_cinematic_audio.cue_starts[0],opening_cinematic_audio.cue_starts[1],opening_cinematic_audio.cue_starts[2],opening_cinematic_audio.cue_starts[3]);
};

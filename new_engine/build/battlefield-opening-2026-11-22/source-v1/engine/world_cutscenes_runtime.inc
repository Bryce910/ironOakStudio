// Published cutscenes are a gameplay host of the existing CPU timeline. The
// logical hero/camera and conversation states stay put; only render_cam changes.
world_cutscenes::Runtime cutscene_runtime,cutscene_preview_runtime;
world_cutscenes::Frame cutscene_frame;
bool cutscene_library_ready=false,cutscene_preview=false,cutscene_pause=false;
bool cutscene_return_neutral=false,cutscene_enter=false,cutscene_space=false,cutscene_interact_down=false;
bool cutscene_audio_ready=false,cutscene_audio_paused=false,cutscene_suspended=false;
bool cutscene_from_interaction=false;
bool cutscene_progress_ready=true;
double cutscene_audio_previous=0;
std::uint64_t cutscene_audio_id=0,cutscene_audio_generation=0;
unsigned cutscene_starts=0,cutscene_completions=0,cutscene_skips=0,cutscene_previews=0,cutscene_audio_failures=0;
unsigned cutscene_lock_violations=0;double cutscene_root_delta_max=0,cutscene_aim_delta_max=0;
Camera cutscene_return_camera;
std::string cutscene_error;
auto cutscene_progress_path=[&](){return (std::filesystem::u8path(studio.project_directory)/"saves/cutscenes.progress").u8string();};
cutscene_progress_save=[&](std::string &error){
    if(cutscene_preview){error.clear();return true;}
    if(!cutscene_progress_ready){error="The invalid cutscene completion record is preserved. Repair it or explicitly start a New Journey before saving cutscene progress.";return false;}
    return world_cutscenes::save_progress(cutscene_progress_path().c_str(),cutscene_runtime.progress(),error);
};
cutscene_progress_load=[&](std::string &error){
    world_cutscenes::Progress next;
    if(!world_cutscenes_host::read_progress(cutscene_progress_path(),next,error)||!cutscene_runtime.set_progress(next,error)){cutscene_progress_ready=false;return false;}
    cutscene_runtime.reset(true);cutscene_progress_ready=true;return true;
};
cutscene_progress_reset=[&](){
    cutscene_runtime.reset(false);cutscene_preview_runtime.reset(false);cutscene_frame={};cutscene_preview=false;cutscene_progress_ready=true;
    // A new journey hands control to the opening, which owns its own release gate.
    authored_cutscene_locked=false;cutscene_return_neutral=false;cutscene_pause=false;
    if(studio.cinematic_audio&&cutscene_audio_id)world_cinematics_audio::stop_all(studio.cinematic_audio);
    cutscene_audio_id=0;std::string error;if(!cutscene_progress_save(error))std::fprintf(stderr,"[Cutscenes] New journey progress: %s\n",error.c_str());
};
auto cutscene_opening_priority=[&](){
    return opening_enabled&&(opening_frame.cinematic||opening_frame.npc_intro||
        opening_state.stage()==world_opening_prologue::Stage::Title||opening_state.stage()==world_opening_prologue::Stage::GameMenu);
};
auto cutscene_audio_files=[&](const world_cutscenes::PreparedLibrary &library,
                            std::vector<world_cinematics_audio::FileAsset> &files,std::string &error){
    std::error_code ec;const auto root=std::filesystem::canonical(std::filesystem::u8path(studio.project_directory),ec);
    if(ec){error="The cutscene project folder is unavailable.";return false;}
    auto add=[&](const std::string &asset){
        if(asset.empty())return true;
        if(!world_soundscape::asset_path(asset)){error="Cutscene audio must reference a project-relative PCM WAV.";return false;}
        if(std::any_of(files.begin(),files.end(),[&](const auto &f){return f.asset==asset;}))return true;
        if(files.size()>=world_cinematics_audio::asset_limit){error="Map sounds and cutscenes share a32-WAV library budget.";return false;}
        const auto path=std::filesystem::canonical(root/std::filesystem::u8path(asset),ec);
        if(ec||!world_cutscenes::detail::contained(root,path)||!std::filesystem::is_regular_file(path,ec)||ec){error="Cutscene WAV is missing or outside the project: "+asset;return false;}
        files.push_back({asset,path.u8string()});return true;
    };
    for(const auto &sound:world_sound_emitters)if(!add(sound.sound.asset))return false;
    for(const auto &sequence:library.sequences){
        for(const auto &zone:sequence.timeline.zones)if(!add(zone.asset))return false;
        for(const auto &cue:sequence.timeline.clips)if(!add(cue.asset))return false;
    }return true;
};
std::function<bool(std::string&)> cutscene_prepare_audio=[&](std::string &error){
    if(!cutscene_library_ready){error.clear();return true;}
    std::vector<world_cinematics_audio::FileAsset> files;
    if(!cutscene_audio_files(cutscene_runtime.library(),files,error))return false;
    if(files.empty()){cutscene_audio_ready=true;cutscene_audio_generation=0;error.clear();return true;}
    if(!studio.cinematic_audio){studio.cinematic_audio=world_cinematics_audio::create({},error);if(!studio.cinematic_audio)return false;}
    if(!world_cinematics_audio::collect(studio.cinematic_audio)){error="Wait for the previous audio bank to finish switching, then reload the published library.";return false;}
    if(!world_cinematics_audio::configure_library_files(studio.cinematic_audio,files,error))return false;
    for(const auto &s:cutscene_runtime.library().sequences)
        if(!world_cinematics_audio::prepared_document_valid(studio.cinematic_audio,s.timeline,error))return false;
    cutscene_audio_generation=world_cinematics_audio::stats(studio.cinematic_audio).generation;
    cutscene_audio_ready=true;studio.cinematic_audio_active=studio.cinematic_audio_ready=false;return true;
};
studio.cinematic_reload_library=[&](std::string &error){
    if(authored_cutscene_locked){error="Finish the current published take before reloading.";return false;}
    world_cutscenes::Document document;world_cutscenes::PreparedLibrary prepared;world_cutscenes::Runtime next;
    const auto path=(std::filesystem::u8path(studio.project_directory)/"cinematics/world.vcuts").u8string();
    if(!world_cutscenes::load(path.c_str(),document,error)||!world_cutscenes::prepare(studio.project_directory.c_str(),document,prepared,error)||
       !next.configure(prepared,error)||!next.set_progress(cutscene_runtime.progress(),error))return false;
    auto before=std::move(cutscene_runtime);cutscene_runtime=std::move(next);const bool previous_ready=cutscene_library_ready,previous_audio_ready=cutscene_audio_ready;
    cutscene_library_ready=true;cutscene_audio_ready=false;
    if(!cutscene_prepare_audio(error)){
        cutscene_runtime=std::move(before);cutscene_library_ready=previous_ready;cutscene_audio_ready=previous_audio_ready;
        if(previous_audio_ready&&studio.cinematic_audio){
            for(const auto &s:cutscene_runtime.library().sequences){std::string ignored;if(!world_cinematics_audio::prepared_document_valid(studio.cinematic_audio,s.timeline,ignored)){cutscene_audio_ready=false;break;}}
            if(cutscene_audio_ready)cutscene_audio_generation=world_cinematics_audio::stats(studio.cinematic_audio).generation;
        }return false;
    }
    cutscene_frame={};cutscene_return_neutral=true;cutscene_error.clear();
    std::printf("[Cutscenes] Published library ready: %zu sequences, one bounded shared WAV bank\n",prepared.sequences.size());return true;
};
studio.cinematic_play_sequence=[&](std::uint64_t id,std::string &error){
    if(authored_cutscene_locked||cutscene_opening_priority()||studio.cinematic_preview||studio.production_freeze()){
        error="Finish the opening or current camera preview before playing a published take.";return false;
    }
    if(studio.cinematic_unsaved()){error="Publish the cinematic library before gameplay playback.";return false;}
    if(!studio.cinematic_reload_library(error))return false;
    const auto *sequence=cutscene_runtime.find(id);if(!sequence){error="The selected published cutscene is unavailable.";return false;}
    world_cutscenes::PreparedLibrary preview;preview.document.sequences.push_back(sequence->sequence);preview.sequences.push_back(*sequence);
    preview.document.sequences[0].enabled=true;preview.document.sequences[0].once=false;preview.document.sequences[0].trigger.kind=world_cutscenes::TriggerKind::Manual;
    preview.sequences[0].sequence=preview.document.sequences[0];
    if(!cutscene_preview_runtime.configure(preview,error)||!cutscene_preview_runtime.manual_start(id))return false;
    cutscene_preview=true;cutscene_from_interaction=false;authored_cutscene_locked=true;cutscene_pause=false;cutscene_return_neutral=false;++cutscene_previews;return true;
};
// Preserve shared PCM after an explicit Sound Apply. Never reload it in tick.
cutscene_sound_uses_library=[&](){return cutscene_library_ready;};
cutscene_sound_reprepare=[&](std::string &error){
    const bool ready=cutscene_audio_ready;const auto generation=cutscene_audio_generation;
    cutscene_audio_ready=false;if(cutscene_prepare_audio(error))return true;
    if(ready&&studio.cinematic_audio&&world_cinematics_audio::library_ready(studio.cinematic_audio)&&
       world_cinematics_audio::stats(studio.cinematic_audio).generation==generation)cutscene_audio_ready=true;
    return false;
};
{
    std::error_code ec;const auto path=std::filesystem::u8path(studio.project_directory)/"cinematics/world.vcuts";
    if(std::filesystem::exists(path,ec)){
        std::string error;if(!studio.cinematic_reload_library(error)){cutscene_error=error;std::fprintf(stderr,"[Cutscenes] Published library not playable: %s\n",error.c_str());}
    }
    // This record is independent of the library. A designer may publish the
    // first library after startup; old completion data must still be retained.
    std::string error;if(!cutscene_progress_load(error)){cutscene_error=error;std::fprintf(stderr,"[Cutscenes] Saved completion record preserved; gameplay triggers disabled until repair or New Journey: %s\n",error.c_str());}
}
auto cutscene_target_position=[&](std::uint64_t id,world_cutscenes::Vec3 &out){
    // Explicit published camp IDs; these are not render model indices.
    if(id>=1&&id<=3){
        if(opening_enabled&&opening_state.config().arrival_camp.enabled){const auto &p=opening_state.config().arrival_camp.actors[unsigned(id-1)].feet;out={p.x,p.y,p.z};return true;}
        WorldTrailCamper camper;if(world_trail_camp_actor(trail_camp,unsigned(id-1),&camper)){out={camper.x,camper.y,camper.z};return true;}
        return false;
    }
    world_runtime::Entity entity;if(studio.components_runtime&&studio.components_runtime->entity(id,entity)&&entity.active){out={entity.pose.position.x,entity.pose.position.y,entity.pose.position.z};return true;}
    return false;
};
auto cutscene_start_context=[&](){
    world_cutscenes_host::InteractionContext gate;
    gate.library_ready=cutscene_library_ready;gate.audio_ready=cutscene_audio_ready;gate.progress_ready=cutscene_progress_ready;
    gate.editor=studio.open;gate.locked=authored_cutscene_locked;gate.return_neutral=cutscene_return_neutral;gate.opening_priority=cutscene_opening_priority();
    gate.loading=destination_loading;gate.graphics=show_graphics;gate.map=show_map;gate.inventory=inventory_open;gate.keyboard_owned=ImGui::GetIO().WantCaptureKeyboard||ImGui::IsAnyItemActive();
    gate.opening_dialogue=opening_enabled&&opening_frame.camp_dialogue;gate.component_dialogue=studio.components_runtime&&studio.components_runtime->current_line();gate.journey_dialogue=first_journey_dialogue_active;
    gate.camera_preview=studio.cinematic_preview;gate.production_freeze=studio.production_freeze();gate.flying=cam.fly;gate.fighting=fight.engaged();gate.menu=presentation&&preview_screen!=1;
    return gate;
};
auto cutscene_try_interact=[&](){
    if(!world_cutscenes_host::interaction_allowed(cutscene_start_context()))return false;
    std::uint64_t target=0;double nearest=1e30;
    for(const auto &p:cutscene_runtime.library().sequences){const auto &s=p.sequence;
        if(!s.enabled||s.trigger.kind!=world_cutscenes::TriggerKind::Interact||(s.once&&cutscene_runtime.completed(s.id)))continue;
        world_cutscenes::Vec3 position;if(!cutscene_target_position(s.trigger.target_id,position))continue;
        const double dx=position.x-cam.x,dz=position.z-cam.z,distance=std::hypot(dx,dz);
        const bool visible=world_cutscenes_host::interaction_visible({cam.x,cam.y,cam.z},cam.yaw,position,s.trigger.extent.x,
            [&](world_cinematics::Vec3 eye,world_cinematics::Vec3 chest){
                const world_physics::Vec3 start{eye.x,eye.y,eye.z};
                const double length=std::hypot(std::hypot(chest.x-eye.x,chest.y-eye.y),chest.z-eye.z),trim=length>.25?(length-.20)/length:1.;
                const world_physics::Vec3 delta{(chest.x-eye.x)*trim,(chest.y-eye.y)*trim,(chest.z-eye.z)*trim};
                world_physics::Hit hit;
                if(active_playtest_walk||active_city_physics){const bool ok=active_playtest_walk?active_playtest_walk->raycast(start,delta,hit):active_city_physics->raycast(start,delta,hit);if(!ok||hit.hit)return false;}
                if(studio.components_ready&&studio.components_runtime&&studio.components_runtime->stats().physics.bodies){world_runtime::Id entity=0;if(!studio.components_runtime->raycast({eye.x,eye.y,eye.z},{delta.x,delta.y,delta.z},hit,entity)||
                    (hit.hit&&entity!=s.trigger.target_id))return false;}
                for(unsigned i=0;i<8;++i){const double a=double(i)/8,b=double(i+1)/8;
                    const double ax=eye.x+delta.x*a,az=eye.z+delta.z*a,bx=eye.x+delta.x*b,bz=eye.z+delta.z*b,ay=eye.y+delta.y*a,by=eye.y+delta.y*b;
                    if(!world_scenery_can_move(scenery,ax,az,bx,bz,float(std::min(ay,by)-.035),float(std::abs(by-ay)+.07),.035f))return false;
                    if(!active_playtest_walk&&!active_city_physics&&world_atlas_walk_height(bx,bz)>by-.05)return false;
                }return true;
            });
        if(!visible)continue;
        if(distance<nearest){nearest=distance;target=s.trigger.target_id;}
    }
    if(!target||!cutscene_runtime.interact(target))return false;
    opening_confirm=opening_interact=opening_enter=false;cutscene_interact_down=true;cutscene_from_interaction=true;authored_cutscene_locked=true;return true;
};
auto cutscene_event=[&](const SDL_Event &event){
    const bool key=event.type==SDL_KEYDOWN||event.type==SDL_KEYUP;
    if(key&&!event.key.repeat){const bool down=event.type==SDL_KEYDOWN;const auto code=event.key.keysym.sym;
        if(code==SDLK_RETURN||code==SDLK_KP_ENTER)cutscene_enter=down;
        if(code==SDLK_SPACE)cutscene_space=down;
        if(code==SDLK_e)cutscene_interact_down=down;
    }
    if(event.type==SDL_CONTROLLERBUTTONUP&&event.cbutton.which==preview_pad.id()&&event.cbutton.button==SDL_CONTROLLER_BUTTON_A)cutscene_interact_down=false;
    if(event.type==SDL_WINDOWEVENT&&event.window.event==SDL_WINDOWEVENT_FOCUS_LOST){cutscene_enter=cutscene_space=cutscene_interact_down=false;}
    if(!authored_cutscene_locked){
        if(key&&event.type==SDL_KEYDOWN&&!event.key.repeat&&event.key.keysym.sym==SDLK_e&&cutscene_try_interact())return true;
        if(event.type==SDL_CONTROLLERBUTTONDOWN&&event.cbutton.which==preview_pad.id()&&event.cbutton.button==SDL_CONTROLLER_BUTTON_A&&cutscene_try_interact())return true;
        return false;
    }
    const bool pause_key=key&&event.type==SDL_KEYDOWN&&!event.key.repeat&&event.key.keysym.sym==SDLK_ESCAPE;
    const bool pause_pad=event.type==SDL_CONTROLLERBUTTONDOWN&&event.cbutton.which==preview_pad.id()&&event.cbutton.button==SDL_CONTROLLER_BUTTON_START;
    if(pause_key||pause_pad){if(presentation&&preview_screen!=1){preview_screen=1;cutscene_pause=false;}else cutscene_pause=!cutscene_pause;preview_release_mouse();return true;}
    if(key&&event.type==SDL_KEYDOWN&&!event.key.repeat&&event.key.keysym.sym==SDLK_F10){show_graphics=!show_graphics;if(show_graphics)graphics_draft=graphics;return true;}
    return key||event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_MOUSEBUTTONUP||event.type==SDL_MOUSEMOTION||event.type==SDL_MOUSEWHEEL||event.type==SDL_CONTROLLERBUTTONDOWN||event.type==SDL_CONTROLLERBUTTONUP;
};
auto cutscene_neutral=[&](){
    const auto *keys=SDL_GetKeyboardState(nullptr);
    for(auto key:{SDL_SCANCODE_W,SDL_SCANCODE_A,SDL_SCANCODE_S,SDL_SCANCODE_D,SDL_SCANCODE_E,SDL_SCANCODE_SPACE,SDL_SCANCODE_RETURN,SDL_SCANCODE_KP_ENTER,SDL_SCANCODE_LSHIFT})if(keys[key])return false;
    if(cutscene_enter||cutscene_space||cutscene_interact_down||(SDL_GetMouseState(nullptr,nullptr)&(SDL_BUTTON_LMASK|SDL_BUTTON_RMASK)))return false;
    if(preview_pad.get()){
        for(auto button:{SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_B,SDL_CONTROLLER_BUTTON_X,SDL_CONTROLLER_BUTTON_Y,SDL_CONTROLLER_BUTTON_START})if(SDL_GameControllerGetButton(preview_pad.get(),button))return false;
        for(auto axis:{SDL_CONTROLLER_AXIS_LEFTX,SDL_CONTROLLER_AXIS_LEFTY,SDL_CONTROLLER_AXIS_RIGHTX,SDL_CONTROLLER_AXIS_RIGHTY})if(std::abs(int(SDL_GameControllerGetAxis(preview_pad.get(),axis)))>6000)return false;
        if(SDL_GameControllerGetAxis(preview_pad.get(),SDL_CONTROLLER_AXIS_TRIGGERLEFT)>8000||SDL_GameControllerGetAxis(preview_pad.get(),SDL_CONTROLLER_AXIS_TRIGGERRIGHT)>8000)return false;
    }return true;
};
auto cutscene_tick=[&](double seconds){
    // Input handoff remains releasable even when the optional library is absent.
    const bool neutral=cutscene_neutral();if(cutscene_return_neutral&&neutral)cutscene_return_neutral=false;
    if(!cutscene_library_ready&&!cutscene_preview)return;
    if(cutscene_audio_generation&&(!studio.cinematic_audio||!world_cinematics_audio::library_ready(studio.cinematic_audio)||world_cinematics_audio::stats(studio.cinematic_audio).generation!=cutscene_audio_generation))cutscene_audio_ready=false;
    const auto flags=SDL_GetWindowFlags(window);const bool focused=!(flags&SDL_WINDOW_MINIMIZED)&&(launch->frames||(flags&SDL_WINDOW_INPUT_FOCUS));
    const bool blocked=cutscene_opening_priority()||destination_loading||show_graphics||inventory_open||show_map||studio.production_freeze()||studio.cinematic_preview;
    world_cutscenes::Input input;input.dt=seconds;input.position={cam.x,cam.y-1.75,cam.z};input.focused=focused;
    input.paused=cutscene_pause||blocked||(!cutscene_preview&&presentation&&preview_screen!=1);
    cutscene_suspended=input.paused||!focused;
    input.neutral=neutral;input.confirm=cutscene_enter||cutscene_space||(preview_pad.get()&&SDL_GameControllerGetButton(preview_pad.get(),SDL_CONTROLLER_BUTTON_A));
    input.allow_triggers=cutscene_audio_ready&&!blocked&&!cutscene_return_neutral&&(cutscene_preview||world_cutscenes_host::interaction_allowed(cutscene_start_context()));
    auto &runtime=cutscene_preview?cutscene_preview_runtime:cutscene_runtime;
    if(!runtime.tick(input,cutscene_frame)){cutscene_error="Published cutscene input was rejected.";return;}
    if(cutscene_frame.started){
        cutscene_return_camera=cam;authored_cutscene_locked=true;cutscene_pause=false;cutscene_audio_id=0;++cutscene_starts;
        fight.set_guard(false);fight.clear_buffer();sword_stick.reset();preview_release_mouse();cam.vy=0;temporal_cut=true;
        opening_confirm=opening_interact=opening_enter=false;
    }
    if(cutscene_frame.completed){
        ++cutscene_completions;cutscene_skips+=cutscene_frame.skipped;
        cam=cutscene_return_camera;
        if(studio.cinematic_audio)world_cinematics_audio::stop_all(studio.cinematic_audio);
        cutscene_audio_id=0;authored_cutscene_locked=false;cutscene_pause=false;cutscene_return_neutral=true;temporal_cut=true;
        preview_release_mouse();fight.set_guard(false);fight.clear_buffer();sword_stick.reset();opening_release();
        if(!cutscene_preview){std::string error;if(!cutscene_progress_save(error)){cutscene_error=error;std::fprintf(stderr,"[Cutscenes] Completion save: %s\n",error.c_str());}}
        else{cutscene_preview_runtime.reset(false);cutscene_preview=false;}
        if(cutscene_from_interaction){
            if(opening_enabled)opening_tell("The take is complete. Release the controls, then press E / A again to continue talking.");
            else{preview_notice="Release the controls, then press E / A again to talk.";preview_notice_time=5;}
            cutscene_from_interaction=false;
        }
    }
};
auto cutscene_render_camera=[&](Camera &camera){
    if(!cutscene_frame.active)return;const auto &pose=cutscene_frame.camera;const auto forward=world_cinematics::forward(pose.rotation);
    const double root_delta=std::hypot(std::hypot(cam.x-cutscene_return_camera.x,cam.y-cutscene_return_camera.y),cam.z-cutscene_return_camera.z);
    const double aim_delta=std::hypot(cam.yaw-cutscene_return_camera.yaw,cam.pitch-cutscene_return_camera.pitch);
    cutscene_root_delta_max=std::max(cutscene_root_delta_max,root_delta);cutscene_aim_delta_max=std::max(cutscene_aim_delta_max,aim_delta);cutscene_lock_violations+=(root_delta>1e-5||aim_delta>1e-5);
    camera.x=pose.position.x;camera.y=pose.position.y;camera.z=pose.position.z;
    camera.yaw=float(std::atan2(forward.x,-forward.z));camera.pitch=float(std::asin(std::clamp(forward.y,-1.,1.)));camera.fov=float(pose.fov_degrees*.017453292519943295);
};
auto cutscene_audio_frame=[&](const Camera &camera){
    if(!cutscene_frame.active)return false;
    const auto &runtime=cutscene_preview?cutscene_preview_runtime:cutscene_runtime;const auto *sequence=runtime.find(cutscene_frame.id);
    if(!sequence)return true;
    if(!studio.cinematic_audio||!world_cinematics_audio::library_ready(studio.cinematic_audio)){
        if(!sequence->timeline.clips.empty()||!sequence->timeline.zones.empty()){if(++cutscene_audio_failures==1)std::fprintf(stderr,"[Cutscenes] Prepared audio ownership changed; reload the published library.\n");}
        return true;
    }
    world_cinematics::Step step;step.previous=cutscene_audio_previous;step.time=cutscene_frame.time;step.elapsed=std::max(0.,step.time-step.previous);
    step.paused=cutscene_suspended;
    step.jumped=cutscene_audio_id!=cutscene_frame.id||cutscene_audio_paused!=step.paused;world_cinematics::CameraPose listener=cutscene_frame.camera;
    listener.position={camera.x,camera.y,camera.z};
    const float gain=preview_settings.voice*audio_master_gain();
    const auto result=world_cinematics_audio::update_prepared_document(studio.cinematic_audio,sequence->timeline,step,listener,gain);
    if(result==world_cinematics_audio::UpdateResult::Ok){cutscene_audio_previous=step.time;cutscene_audio_id=cutscene_frame.id;cutscene_audio_paused=step.paused;}
    else if(result!=world_cinematics_audio::UpdateResult::QueueFull){++cutscene_audio_failures;if(cutscene_audio_failures==1)std::fprintf(stderr,"[Cutscenes] Prepared timeline audio frame was rejected.\n");}
    return true;
};
auto cutscene_draw=[&](){
    if(!cutscene_frame.active)return;
    const auto &io=ImGui::GetIO();auto *background=ImGui::GetBackgroundDrawList();auto *foreground=ImGui::GetForegroundDrawList();
    const float w=io.DisplaySize.x,h=io.DisplaySize.y,scale=std::clamp(std::min(w/1600.f,h/900.f),.65f,1.5f);
    if(cutscene_frame.letterbox){const float bar=h*.075f;background->AddRectFilled({0,0},{w,bar},IM_COL32(0,0,0,255));background->AddRectFilled({0,h-bar},{w,h},IM_COL32(0,0,0,255));}
    const auto *sequence=(cutscene_preview?cutscene_preview_runtime:cutscene_runtime).find(cutscene_frame.id);
    std::string hint=cutscene_pause?"PAUSED  |  Esc / Start: resume":sequence&&sequence->sequence.skip_allowed?"Hold Enter / Space / A to skip  |  Esc / Start: pause":"Esc / Start: pause";
    if(cutscene_preview)hint="PUBLISHED TAKE PREVIEW  |  "+hint;
    const float size=14*scale;const auto width=ImGui::GetFont()->CalcTextSizeA(size,10000,0,hint.c_str()).x;
    foreground->AddText(ImGui::GetFont(),size,{(w-width)*.5f,h*.945f},IM_COL32(231,225,209,235),hint.c_str());
    if(cutscene_frame.skip_progress>0){const float width_bar=160*scale,x=(w-width_bar)*.5f,y=h*.918f;foreground->AddRectFilled({x,y},{x+width_bar,y+3*scale},IM_COL32(55,53,48,240));foreground->AddRectFilled({x,y},{x+width_bar*float(cutscene_frame.skip_progress),y+3*scale},IM_COL32(226,199,140,255));}
};

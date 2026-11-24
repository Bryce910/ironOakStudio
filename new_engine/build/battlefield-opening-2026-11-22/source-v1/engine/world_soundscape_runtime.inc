// World-map sound placements share the existing owned cinematic mixer. A
// successful explicit bank commit selects one mode; they never race two DSP
// adapters or replace the ordinary music/footstep buses.
std::vector<world_soundscape::Emitter> world_sound_emitters;
std::vector<std::string> world_sound_assets;
bool world_sound_active=false,world_sound_muted=false,world_sound_bank_busy=false,world_sound_apply_pending=false;
std::uint64_t world_sound_solo=0,world_sound_audition=0,world_sound_audition_serial=0;
unsigned world_sound_prepares=0,world_sound_auto_loads=0,world_sound_fast_applies=0;
unsigned long long world_sound_frames=0,world_sound_update_failures=0;
std::function<bool(const world_studio::Document &,std::string &)> world_sound_prepare=
    [&](const world_studio::Document &document,std::string &error){
        world_sound_bank_busy=false;
        std::vector<world_soundscape::Emitter> next;
        if(!world_authoring::sound_emitters(document,next,error))return false;
        std::vector<std::string> assets;
        for(const auto &emitter:next)if(!emitter.sound.asset.empty()&&
            std::find(assets.begin(),assets.end(),emitter.sound.asset)==assets.end())assets.push_back(emitter.sound.asset);
        std::sort(assets.begin(),assets.end());
        if(next.empty()&&!world_sound_active){world_sound_emitters.clear();world_sound_assets.clear();error.clear();return true;}
        if(next.empty()&&!studio.cinematic_audio){world_sound_emitters.clear();world_sound_assets.clear();world_sound_active=false;error.clear();return true;}
        if(!studio.cinematic_audio){studio.cinematic_audio=world_cinematics_audio::create({},error);if(!studio.cinematic_audio)return false;}
        const bool shared_library=cutscene_sound_uses_library&&cutscene_sound_uses_library();
        if((!world_sound_active||assets!=world_sound_assets)&&!shared_library){
            world_cinematics_audio::SoundscapeConfiguration config;config.emitters=next;
            for(const auto &asset:assets)config.files.push_back({asset,(std::filesystem::u8path(studio.project_directory)/std::filesystem::u8path(asset)).u8string()});
            if(!world_cinematics_audio::collect(studio.cinematic_audio)){
                world_sound_bank_busy=true;error="Waiting for the previous audio bank to finish switching.";return false;
            }
            if(!world_cinematics_audio::configure_soundscape_files(studio.cinematic_audio,config,error))return false;
            ++world_sound_prepares;
        }
        auto previous_emitters=std::move(world_sound_emitters);auto previous_assets=std::move(world_sound_assets);
        const bool previous_active=world_sound_active;
        world_sound_emitters=std::move(next);world_sound_assets=std::move(assets);
        world_sound_active=true;world_sound_audition=0;
        studio.cinematic_audio_active=studio.cinematic_audio_ready=false;
        const bool shared_prepare_needed=shared_library&&(!previous_active||world_sound_assets!=previous_assets||!world_cinematics_audio::library_ready(studio.cinematic_audio));
        if(shared_prepare_needed&&cutscene_sound_reprepare&&!cutscene_sound_reprepare(error)){
            world_sound_emitters=std::move(previous_emitters);world_sound_assets=std::move(previous_assets);world_sound_active=previous_active;return false;
        }
        error.clear();return true;
    };
studio.sound_prepare=[&](const world_studio::Document &document,std::string &error){
    if(!world_sound_prepare(document,error))return false;
    world_sound_muted=false;return true;
};
studio.sound_audition=[&](unsigned index,std::string &error){
    if(index>=studio.draft.sounds.size()){error="Choose a placed sound before auditioning it.";return false;}
    if(studio.draft.sounds[index].asset.empty()){error="Choose a WAV track for this sound first.";return false;}
    if(!world_sound_prepare(studio.draft,error))return false;
    if(index>=world_sound_emitters.size()){error="The selected sound has no prepared identity.";return false;}
    world_sound_audition=world_sound_emitters[index].id;
    if(++world_sound_audition_serial==0)++world_sound_audition_serial;
    world_sound_muted=false;return true;
};
studio.sound_stop=[&](){world_sound_muted=true;world_sound_audition=0;world_sound_solo=0;};
studio.sound_monitor=[&](bool muted,int index){
    world_sound_muted=muted;world_sound_audition=0;world_sound_solo=0;
    if(index>=0&&unsigned(index)<studio.draft.sounds.size()){
        for(const auto &node:studio.draft.entities.nodes)
            if(node.binding==world_entity::Binding{world_entity::Kind::Sound,unsigned(index)}){world_sound_solo=node.id;break;}
    }
};
studio.sound_yield=[&](){world_sound_active=false;world_sound_audition=world_sound_solo=0;};
auto world_sound_frame=[&](const Camera &listener,const Camera &view,float seconds,bool focused,bool paused){
    if(!world_sound_active||!studio.cinematic_audio)return;
    world_cinematics_audio::SoundscapeFrame frame;
    // The editor hears its camera. In gameplay, use the hero's head position
    // with the rendered view orientation: a third-person boom cannot move the
    // hero out of a small audible zone.
    frame.listener.position={listener.x,listener.y,listener.z};
    frame.listener.rotation=world_cinematics::orientation(view.yaw,view.pitch);
    frame.dt=std::clamp(double(seconds),0.,1.);frame.focused=focused;frame.paused=paused;
    frame.loading=destination_loading;frame.muted=world_sound_muted;
    frame.solo_id=world_sound_solo;frame.audition_id=world_sound_audition;frame.audition_serial=world_sound_audition_serial;
    for(auto &emitter:world_sound_emitters){const auto weather=authored_weather_at(emitter.sound.x,emitter.sound.z);emitter.condition_gain=world_soundscape::weather_gain(emitter.sound,weather.rain,weather.storm_weight);}
    const auto result=world_cinematics_audio::library_ready(studio.cinematic_audio)?world_cinematics_audio::update_soundscape_prepared(studio.cinematic_audio,world_sound_emitters.data(),world_sound_emitters.size(),frame):world_cinematics_audio::update_soundscape(studio.cinematic_audio,world_sound_emitters.data(),world_sound_emitters.size(),frame);
    ++world_sound_frames;
    if(result!=world_cinematics_audio::UpdateResult::Ok&&result!=world_cinematics_audio::UpdateResult::QueueFull){
        ++world_sound_update_failures;
        if(world_sound_update_failures==1)std::fprintf(stderr,"[World audio] Playback frame rejected; applied sound placements preserved\n");
    }
};
// A sound-only Apply has no scenery, tile, collision, lighting or navigation
// rebuild. Scalar listener data and the entity graph are render-thread owned;
// terrain workers retain their exact live vector and field addresses.
auto world_sound_try_apply=[&](const Camera &weather_eye){
    if(world_sound_apply_pending){
        if(studio.cinematic_audio&&!world_cinematics_audio::collect(studio.cinematic_audio)){studio.apply=false;return true;}
        world_sound_apply_pending=false;studio.apply=true;
    }
    if(!studio.apply||studio.structure_cooking||studio.production_freeze())return false;
    if(world_sound_bank_busy&&studio.cinematic_audio&&!world_cinematics_audio::collect(studio.cinematic_audio)){studio.apply=false;world_sound_apply_pending=true;return true;}
    world_sound_bank_busy=false;
    std::string error;
    if(!world_studio::environment_only_change(studio.live,studio.draft))return false;
    const auto began=std::chrono::steady_clock::now();studio.apply=false;
    std::vector<world_weather_authoring::BoundArea> next_weather;
    if(!studio.production_retry_components()||!studio.production_sync(error)||
       !world_studio::valid(studio.draft,error)||!studio.components_scene_prevalidate(studio.draft,error)||
       !world_weather_prepare(studio.draft,next_weather,error)||!world_sound_prepare(studio.draft,error)){
        if(world_sound_bank_busy){studio.apply=false;world_sound_apply_pending=true;studio.message=error;return true;}
        ++studio.rejected;studio.message=error.empty()?"Sound preparation failed; the applied world is preserved.":error;
        return true;
    }
    world_weather_commit(studio.draft,std::move(next_weather),&weather_eye);
    studio.live.sounds=studio.draft.sounds;studio.live.entities=studio.draft.entities;studio.live.weather_global=studio.draft.weather_global;studio.live.weather_areas=studio.draft.weather_areas;
    ++studio.applied;++world_sound_fast_applies;studio.dirty=studio.rebuild=false;
    studio.production_applied();
    if(!studio.components_scene_applied()){active_world_runtime=nullptr;studio.components_ready=false;studio.open=true;}
    else studio.message="Weather and sound placements applied. Save All keeps them with this map.";
    studio.last_apply_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();
    studio.last_apply_terrain=studio.last_apply_masks=false;++studio.terrain_rebuild_skipped;
    return true;
};
if(!studio.live.sounds.empty()){
    std::string error;
    if(world_sound_prepare(studio.live,error)){++world_sound_auto_loads;std::printf("[World audio] Loaded %zu map sound placements\n",world_sound_emitters.size());}
    else{studio.message="Map audio could not load: "+error;std::fprintf(stderr,"[World audio] %s\n",error.c_str());}
}

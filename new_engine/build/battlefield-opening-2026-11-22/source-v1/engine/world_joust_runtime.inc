// Optional mounted arena host. Ordinary exploration and sword arena continue
// through their original paths whenever joust_enabled is false.
world_joust::Game joust_game;
world_combat_vfx::System joust_dust;
WorldTrainingCombat joust_dust_context;
unsigned joust_landings=0,joust_dust_peak=0;
unsigned long long joust_dust_draws=0;
float joust_landing_age=10;
// Two scene-owned voices, decoded once. No file reads or voice allocation
// during play; pausing/focus loss silences both without a delayed cheer.
struct JoustAudienceAudio {
    struct Probe {std::uint64_t samples=0,nonzero=0,clipped=0,callbacks=0;double energy=0,peak=0;double rms()const{return samples?std::sqrt(energy/double(samples)):0;}};
    Mix_Chunk *bed=nullptr,*cheer=nullptr;int first=-1,volume=100,hz=0,channels=0;Uint16 format=0;
    bool active=false,ready=false,probe_registered=false;float swell=0,cooldown=0;
    unsigned events=0,updates=0,restarts=0;std::uint64_t cheer_until=0,preview_until=0;
    float bed_source_rms=0,cheer_source_rms=0,bed_normalized_rms=0,cheer_normalized_rms=0;
    std::filesystem::path settings_path;std::string settings_message;
    SDL_SpinLock probe_lock=0;Probe measured;
    static float sample(const Uint8 *bytes,unsigned index,Uint16 fmt){
        if(fmt==AUDIO_S16SYS){Sint16 v;std::memcpy(&v,bytes+index*2,2);return float(v)/32768.f;}
        float v;std::memcpy(&v,bytes+index*4,4);return std::isfinite(v)?v:0.f;
    }
    static void meter(int,void *stream,int length,void *userdata){
        auto &a=*static_cast<JoustAudienceAudio*>(userdata);const unsigned stride=a.format==AUDIO_S16SYS?2:4;
        Probe batch;batch.callbacks=1;
        for(unsigned i=0;i<unsigned(std::max(0,length))/stride;++i){const double v=sample(static_cast<const Uint8*>(stream),i,a.format),m=std::abs(v);++batch.samples;batch.nonzero+=m>1.e-5;batch.clipped+=m>=.999;batch.energy+=v*v;batch.peak=std::max(batch.peak,m);}
        SDL_AtomicLock(&a.probe_lock);a.measured.samples+=batch.samples;a.measured.nonzero+=batch.nonzero;a.measured.clipped+=batch.clipped;a.measured.callbacks+=batch.callbacks;a.measured.energy+=batch.energy;a.measured.peak=std::max(a.measured.peak,batch.peak);SDL_AtomicUnlock(&a.probe_lock);
    }
    bool begin_probe(){
        if(!ready||probe_registered)return false;
        SDL_AtomicLock(&probe_lock);measured={};SDL_AtomicUnlock(&probe_lock);
        probe_registered=Mix_RegisterEffect(MIX_CHANNEL_POST,meter,nullptr,this)!=0;return probe_registered;
    }
    Probe probe(){SDL_AtomicLock(&probe_lock);const auto value=measured;SDL_AtomicUnlock(&probe_lock);return value;}
    void end_probe(){if(probe_registered&&Mix_QuerySpec(nullptr,nullptr,nullptr))Mix_UnregisterEffect(MIX_CHANNEL_POST,meter);probe_registered=false;}
    bool normalize(Mix_Chunk *clip,float &source_rms,float &result_rms){
        const unsigned stride=format==AUDIO_S16SYS?2:4,count=clip->alen/stride;if(!count)return false;
        double peak=0,energy=0;for(unsigned i=0;i<count;++i){const double v=sample(clip->abuf,i,format);peak=std::max(peak,std::abs(v));energy+=v*v;}
        if(!std::isfinite(peak)||peak<1.e-5)return false;
        source_rms=float(std::sqrt(energy/count));const float gain=float(std::min(3.,.75/peak));energy=0;
        for(unsigned i=0;i<count;++i){const float v=std::clamp(sample(clip->abuf,i,format)*gain,-.75f,.75f);energy+=double(v)*v;if(format==AUDIO_S16SYS){const Sint16 q=Sint16(std::lround(v*32767));std::memcpy(clip->abuf+i*stride,&q,2);}else std::memcpy(clip->abuf+i*stride,&v,4);}
        result_rms=float(std::sqrt(energy/count));return true;
    }
    bool trim_lead(Mix_Chunk *clip){
        // The licensed recordings build very quietly for their first two
        // seconds. Trim only our decoded copy so a crowd reacts immediately.
        const unsigned sample_bytes=format==AUDIO_S16SYS?2:4,frame_bytes=unsigned(channels)*sample_bytes;
        const std::uint64_t skip=std::uint64_t(hz)*frame_bytes*2;
        if(clip->alen<=skip+std::uint64_t(hz)*frame_bytes)return false;
        SDL_memmove(clip->abuf,clip->abuf+skip,clip->alen-unsigned(skip));clip->alen-=unsigned(skip);
        const unsigned fade_frames=std::min(unsigned(hz*.032),clip->alen/frame_bytes);
        for(unsigned frame=0;frame<fade_frames;++frame)for(int channel=0;channel<channels;++channel){
            const unsigned i=frame*unsigned(channels)+unsigned(channel);const float v=sample(clip->abuf,i,format)*float(frame)/std::max(1u,fade_frames-1);
            if(format==AUDIO_S16SYS){const Sint16 q=Sint16(std::lround(v*32767));std::memcpy(clip->abuf+i*sample_bytes,&q,2);}else std::memcpy(clip->abuf+i*sample_bytes,&v,4);
        }
        return true;
    }
    void load_settings(){
        volume=100;settings_message.clear();std::ifstream in(settings_path);if(!in)return;
        std::string header,line,end,extra;int value=-1;std::getline(in,header);std::getline(in,line);std::getline(in,end);
        std::istringstream setting(line);std::string key;if(header!="VALWOOD_JOUST_AUDIO 1"||!(setting>>key>>value)||key!="crowd_volume"||value<0||value>100||(setting>>extra)||end!="end"||(in>>extra)){settings_message="Invalid crowd settings; using 100%.";return;}volume=value;
    }
    bool save_settings(){
        const auto temporary=settings_path.u8string()+".tmp";std::ofstream out(temporary,std::ios::trunc);out<<"VALWOOD_JOUST_AUDIO 1\ncrowd_volume "<<volume<<"\nend\n";out.close();
        std::error_code error;if(out)std::filesystem::rename(temporary,settings_path,error);else error=std::make_error_code(std::errc::io_error);
        if(error){std::error_code ignored;std::filesystem::remove(temporary,ignored);settings_message="Crowd volume could not be saved.";return false;}settings_message.clear();return true;
    }
    void set_volume(int value){volume=std::clamp(value,0,100);}
    bool load(const std::filesystem::path &project){
        release();if(!Mix_QuerySpec(&hz,&format,&channels)||(format!=AUDIO_S16SYS&&format!=AUDIO_F32SYS)||hz<=0||channels<=0)return false;
        settings_path=std::filesystem::absolute(project)/"joust-audio.settings";load_settings();
        bed=Mix_LoadWAV((std::filesystem::absolute(project)/"joust-audio/crowd.wav").u8string().c_str());cheer=Mix_LoadWAV((std::filesystem::absolute(project)/"joust-audio/cheer.wav").u8string().c_str());
        if(!bed||!cheer||!trim_lead(bed)||!trim_lead(cheer)||!normalize(bed,bed_source_rms,bed_normalized_rms)||!normalize(cheer,cheer_source_rms,cheer_normalized_rms)){release();return false;}
        first=Mix_AllocateChannels(-1);
        if(first<0||first>126||Mix_AllocateChannels(first+2)!=first+2){first=-1;release();return false;}
        // Paused looping chunks remain occupied in SDL_mixer, keeping generic
        // Mix_PlayChannel(-1) callers out of these appended scene-owned slots.
        for(int i=0;i<2;++i){Mix_Volume(first+i,0);Mix_SetPanning(first+i,255,255);if(Mix_PlayChannel(first+i,i?cheer:bed,-1)<0){release();return false;}Mix_Pause(first+i);}
        ready=true;return true;
    }
    void hit(bool helmet){
        if(!ready)return;
        swell=std::max(swell,helmet?1.f:.7f);
        if(!active||cooldown>0||volume==0||audio_master_gain()<=0)return;
        Mix_Volume(first+1,int(std::lround(88*audio_master_gain()*volume*.01f)));
        if(Mix_PlayChannel(first+1,cheer,-1)>=0){++events;cooldown=1.8f;const std::uint64_t bytes_per_second=std::uint64_t(hz)*channels*(format==AUDIO_S16SYS?2:4);const std::uint64_t duration=cheer->alen*1000ull/bytes_per_second;cheer_until=SDL_GetTicks64()+std::max<std::uint64_t>(1,duration>40?duration-40:duration);}
    }
    void preview(){if(!ready||volume==0||audio_master_gain()<=0)return;preview_until=SDL_GetTicks64()+1800;active=true;cooldown=0;hit(true);}
    void update(float dt,bool audible,bool focused=true){
        if(!ready)return;
        ++updates;
        const float step=std::clamp(std::isfinite(dt)?dt:0.f,0.f,.25f);swell=std::max(0.f,swell-step*.35f);cooldown=std::max(0.f,cooldown-step);
        if(!focused)preview_until=0;
        const bool intentional_preview=focused&&SDL_GetTicks64()<preview_until;
        if(!audible&&!intentional_preview){Mix_Pause(first);Mix_Pause(first+1);active=false;cheer_until=0;swell=cooldown=0;return;}
        const float gain=std::clamp(audio_master_gain(),0.f,1.f)*volume*.01f;
        if(cheer_until&&(SDL_GetTicks64()>=cheer_until||gain<=0)){Mix_Pause(first+1);cheer_until=0;}
        Mix_Volume(first,int(std::lround((cheer_until?40:64)*gain)));Mix_Volume(first+1,int(std::lround(88*gain)));
        if(Mix_GetChunk(first)!=bed||!Mix_Playing(first)){if(Mix_PlayChannel(first,bed,-1)>=0)++restarts;}
        if(Mix_GetChunk(first+1)!=cheer||!Mix_Playing(first+1)){if(Mix_PlayChannel(first+1,cheer,-1)>=0){Mix_Pause(first+1);cheer_until=0;++restarts;}}
        if(Mix_Paused(first))Mix_Resume(first);
        active=Mix_Playing(first)&&Mix_GetChunk(first)==bed;
    }
    void release(){
        end_probe();if(first>=0&&Mix_QuerySpec(nullptr,nullptr,nullptr)){if(Mix_GetChunk(first)==bed)Mix_HaltChannel(first);if(Mix_GetChunk(first+1)==cheer)Mix_HaltChannel(first+1);if(Mix_AllocateChannels(-1)==first+2)Mix_AllocateChannels(first);}
        first=-1;active=ready=false;cheer_until=preview_until=0;cooldown=swell=0;if(bed)Mix_FreeChunk(bed);if(cheer)Mix_FreeChunk(cheer);bed=cheer=nullptr;
    }
    ~JoustAudienceAudio(){release();}
} joust_audience_audio;
std::array<bool,SDL_NUM_SCANCODES> joust_keys={};
int joust_difficulty=0,joust_passes=3;
bool joust_fall_view=false;
bool joust_paused=false,joust_thrust=false,joust_guard=false,joust_controls_armed=false,joust_trigger_down=false,joust_input_context_valid=false,joust_last_pad=false;
float joust_aim_x=0,joust_aim_y=0,joust_idle_clock=0,joust_feedback_age=10,joust_incoming_age=10;
std::uint64_t joust_seen_contact=0;unsigned joust_started=0,joust_completed=0,joust_impacts=0;
world_joust::Phase joust_last_phase=world_joust::Phase::Setup;
std::string joust_feedback;
if(joust_enabled){
    joust_game.set_grip_offset(joust_renderer.grip);third_person=true;weapon_sheathed=true;
    preview_screen=1;cam.fly=false;cam.swimming=false;fight.leave();
    if(!joust_audience_audio.load(std::filesystem::u8path(studio.project_directory)))std::fprintf(stderr,"[Joust audio] Audience recording unavailable\n");
}
auto joust_release=[&](){joust_thrust=joust_guard=false;joust_keys.fill(false);joust_controls_armed=false;joust_trigger_down=false;joust_game.interrupt_thrust();look_active=false;preview_pad.stop_rumble();SDL_SetRelativeMouseMode(SDL_FALSE);};
auto joust_resume=[&](){joust_paused=false;preview_screen=1;joust_controls_armed=false;if(!preview_pad.using_pad&&joust_game.phase==world_joust::Phase::Charge)SDL_SetRelativeMouseMode(SDL_TRUE);};
auto joust_start=[&](){
    world_joust::Setup s;s.difficulty=joust_difficulty;s.passes=joust_passes;s.practice=true;
    joust_game.configure(s);joust_game.set_grip_offset(joust_renderer.grip);joust_game.start();
    joust_aim_x=joust_aim_y=0;joust_seen_contact=0;joust_paused=false;joust_controls_armed=false;joust_feedback.clear();joust_feedback_age=joust_incoming_age=joust_landing_age=10;joust_dust.clear();preview_screen=1;++joust_started;
    if(!preview_pad.using_pad)SDL_SetRelativeMouseMode(SDL_TRUE);
    temporal_cut=true;
};
auto joust_confirm=[&](){
    if(show_graphics){show_graphics=false;joust_resume();return;}
    if(joust_game.phase==world_joust::Phase::Setup){joust_start();return;}
    if(joust_paused){joust_resume();return;}
    if(joust_game.phase==world_joust::Phase::MatchOver)joust_start();
    else if(joust_game.phase==world_joust::Phase::Result){joust_game.next_pass();joust_controls_armed=false;joust_feedback.clear();joust_feedback_age=joust_incoming_age=10;temporal_cut=true;if(!preview_pad.using_pad)SDL_SetRelativeMouseMode(SDL_TRUE);}
};
auto joust_setup=[&](){joust_game.configure({joust_difficulty,joust_passes,true});joust_game.set_grip_offset(joust_renderer.grip);joust_paused=false;preview_screen=1;joust_seen_contact=0;joust_aim_x=joust_aim_y=0;joust_feedback.clear();joust_feedback_age=joust_incoming_age=joust_landing_age=10;joust_dust.clear();joust_release();temporal_cut=true;};
auto joust_event=[&](const SDL_Event &event){
    if(!joust_enabled)return false;
    const bool disconnected=preview_pad.event(event);
    if(event.type==SDL_CONTROLLERDEVICEADDED||event.type==SDL_CONTROLLERDEVICEREMOVED){preview_sync_controller_ui();if(disconnected){joust_paused=true;preview_screen=2;joust_release();}return true;}
    if(event.type==SDL_WINDOWEVENT){
        if(event.window.event==SDL_WINDOWEVENT_FOCUS_LOST){joust_paused=joust_game.phase!=world_joust::Phase::Setup;preview_screen=joust_paused?2:1;joust_release();}
        return false; // window close/resize/focus lifecycle stays with the host
    }
    if(event.type==SDL_QUIT)return false;
    if(!preview_pad.focused&&(event.type==SDL_KEYDOWN||event.type==SDL_CONTROLLERBUTTONDOWN||event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_MOUSEMOTION||event.type==SDL_MOUSEWHEEL))return true;
    if(event.type==SDL_KEYDOWN&&!event.key.repeat){
        const auto key=event.key.keysym.sym;const auto scan=SDL_GetScancodeFromKey(key);if(scan>0&&scan<SDL_NUM_SCANCODES)joust_keys[scan]=true;
        if(key==SDLK_F10){show_graphics=!show_graphics;if(show_graphics)graphics_draft=graphics;joust_paused=true;preview_screen=2;joust_release();return true;}
        if(key==SDLK_ESCAPE){if(show_graphics)show_graphics=false;else if(joust_game.phase!=world_joust::Phase::Setup){if(joust_paused)joust_resume();else{joust_paused=true;preview_screen=2;joust_release();}}return true;}
        if(show_graphics)return true;
        if(joust_game.phase==world_joust::Phase::Setup||joust_paused){
            if(key==SDLK_LEFTBRACKET||key==SDLK_RIGHTBRACKET){joust_audience_audio.set_volume(joust_audience_audio.volume+(key==SDLK_RIGHTBRACKET?10:-10));joust_audience_audio.save_settings();return true;}
            if(key==SDLK_c){joust_audience_audio.preview();return true;}
        }
        if(key==SDLK_RETURN||key==SDLK_KP_ENTER){joust_confirm();return true;}
        if(key==SDLK_r){joust_setup();return true;}
        if(key==SDLK_v){third_person=!third_person;temporal_cut=true;return true;}
        if(joust_game.phase==world_joust::Phase::Setup){
            if(key==SDLK_LEFT)joust_difficulty=std::max(0,joust_difficulty-1);
            if(key==SDLK_RIGHT)joust_difficulty=std::min(2,joust_difficulty+1);
            if(key==SDLK_UP)joust_passes=std::min(9,joust_passes+2);
            if(key==SDLK_DOWN)joust_passes=std::max(1,joust_passes-2);
        }
        if(key==SDLK_SPACE)joust_thrust=true;
        if(key==SDLK_e)joust_guard=true;
        return true;
    }
    if(event.type==SDL_KEYUP){const auto scan=SDL_GetScancodeFromKey(event.key.keysym.sym);if(scan>0&&scan<SDL_NUM_SCANCODES)joust_keys[scan]=false;if(event.key.keysym.sym==SDLK_SPACE)joust_thrust=false;if(event.key.keysym.sym==SDLK_e)joust_guard=false;return true;}
    if(event.type==SDL_CONTROLLERBUTTONDOWN&&event.cbutton.which==preview_pad.id()){
        const auto b=SDL_GameControllerButton(event.cbutton.button);
        if(b==SDL_CONTROLLER_BUTTON_START||b==SDL_CONTROLLER_BUTTON_B){if(show_graphics)show_graphics=false;else if(joust_paused)joust_resume();else if(joust_game.phase!=world_joust::Phase::Setup){joust_paused=true;preview_screen=2;joust_release();}return true;}
        if(show_graphics)return false;
        if(joust_game.phase==world_joust::Phase::Setup||joust_paused){
            if(b==SDL_CONTROLLER_BUTTON_LEFTSHOULDER||b==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER){joust_audience_audio.set_volume(joust_audience_audio.volume+(b==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER?10:-10));joust_audience_audio.save_settings();return true;}
            if(b==SDL_CONTROLLER_BUTTON_Y){joust_audience_audio.preview();return true;}
        }
        if(b==SDL_CONTROLLER_BUTTON_A)joust_confirm();
        if(b==SDL_CONTROLLER_BUTTON_RIGHTSTICK){third_person=!third_person;temporal_cut=true;}
        if(b==SDL_CONTROLLER_BUTTON_X&&(joust_paused||joust_game.phase==world_joust::Phase::MatchOver))joust_setup();
        if(joust_game.phase==world_joust::Phase::Setup){
            if(b==SDL_CONTROLLER_BUTTON_DPAD_LEFT)joust_difficulty=std::max(0,joust_difficulty-1);
            if(b==SDL_CONTROLLER_BUTTON_DPAD_RIGHT)joust_difficulty=std::min(2,joust_difficulty+1);
            if(b==SDL_CONTROLLER_BUTTON_DPAD_UP)joust_passes=std::min(9,joust_passes+2);
            if(b==SDL_CONTROLLER_BUTTON_DPAD_DOWN)joust_passes=std::max(1,joust_passes-2);
        }
        return true;
    }
    if(event.type==SDL_CONTROLLERAXISMOTION||event.type==SDL_CONTROLLERBUTTONUP)return true;
    if(event.type==SDL_MOUSEMOTION){
        if(!joust_paused&&!show_graphics&&(joust_game.phase==world_joust::Phase::Countdown||joust_game.phase==world_joust::Phase::Charge)){
            joust_aim_x=std::clamp(joust_aim_x-float(event.motion.xrel)*.0035f,-1.f,1.f);joust_aim_y=std::clamp(joust_aim_y-float(event.motion.yrel)*.0035f,-1.f,1.f);
        }return true;
    }
    if(event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_MOUSEBUTTONUP){
        if(show_graphics||joust_paused||joust_game.phase==world_joust::Phase::Setup||joust_game.phase==world_joust::Phase::Result||joust_game.phase==world_joust::Phase::MatchOver)return true;
        const bool down=event.type==SDL_MOUSEBUTTONDOWN;
        if(event.button.button==SDL_BUTTON_LEFT)joust_thrust=down;
        if(event.button.button==SDL_BUTTON_RIGHT)joust_guard=down;
        if(down){preview_pad.using_pad=false;SDL_SetRelativeMouseMode(SDL_TRUE);}return true;
    }
    return event.type==SDL_MOUSEWHEEL;
};
auto joust_tick=[&](float step){
    if(!joust_enabled)return;
    const bool paused=joust_paused||show_graphics||destination_loading||(!preview_pad.focused&&!launch->frames);
    auto &g=joust_game;
    joust_audience_audio.update(step,!paused,preview_pad.focused&&!destination_loading&&!show_graphics);
    if(!paused){
        joust_idle_clock+=step;preview_pad.poll(preview_settings.deadzone,false);
        world_joust::Input input;
        const auto &keys=joust_keys;
        const bool using_pad=preview_pad.using_pad&&preview_pad.get();
        if(joust_input_context_valid&&using_pad!=joust_last_pad){joust_controls_armed=false;g.interrupt_thrust();}
        joust_input_context_valid=true;joust_last_pad=using_pad;
        if(joust_trigger_down){if(preview_pad.right_trigger<=.18f)joust_trigger_down=false;}else if(preview_pad.right_trigger>=.35f)joust_trigger_down=true;
        if(using_pad){
            input.throttle=-preview_pad.move.y;input.steer=-preview_pad.move.x;
            // Right stick is lance aim, not an orbit camera, just as requested.
            input.aim_x=-preview_pad.look.x;input.aim_y=-preview_pad.look.y;
            input.thrust=joust_trigger_down;input.guard=preview_pad.left_trigger>.35f;
        }else{
            input.throttle=float(keys[SDL_SCANCODE_W])-float(keys[SDL_SCANCODE_S]);input.steer=float(keys[SDL_SCANCODE_A])-float(keys[SDL_SCANCODE_D]);
            input.aim_x=joust_aim_x;input.aim_y=joust_aim_y;input.thrust=joust_thrust;input.guard=joust_guard;
        }
        const bool raw_pad_neutral=!preview_pad.get()||(preview_pad.right_trigger<=.18f&&preview_pad.left_trigger<=.18f);
        if(!joust_controls_armed){if(raw_pad_neutral&&!joust_thrust&&!joust_guard&&!input.thrust&&!input.guard)joust_controls_armed=true;else input.guard=false;}
        input.thrust_allowed=joust_controls_armed;
        const auto old_player_hit=g.player_hit,old_opponent_hit=g.opponent_hit;
        const auto old_thrust=g.player.thrust_serial;
        const bool player_on_ground=g.player.fall_grounded,rival_on_ground=g.opponent.fall_grounded;
        g.update(step,input);joust_feedback_age+=step;joust_incoming_age+=step;joust_landing_age+=step;
        if(g.player.thrust_serial!=old_thrust)training_sounds.play(0,49,0,audio_master_gain());
        if((!player_on_ground&&g.player.fall_grounded)||(!rival_on_ground&&g.opponent.fall_grounded)){
            training_sounds.play(1,g.player.fall_grounded?86:67,0,audio_master_gain());
            training_sounds.play(2,36,0,audio_master_gain(),180);
            for(const auto *r:{&g.player,&g.opponent})if(r->fall_grounded&&((r==&g.player&&!player_on_ground)||(r==&g.opponent&&!rival_on_ground))){
                joust_dust.ground_impact({-1800+r->fall_position.x,64.10,-10500+r->fall_position.z},1.2f);++joust_landings;
            }
            joust_landing_age=0;
            if(!player_on_ground&&g.player.fall_grounded)preview_pad.rumble(.65f,.35f,190,preview_settings.vibration);
        }
        if(g.contact_serial!=joust_seen_contact){
            const bool outgoing=old_player_hit==world_joust::Hit::Miss&&g.player_hit!=world_joust::Hit::Miss;
            const auto &contact=outgoing?g.player_contact:g.opponent_contact;
            if(old_player_hit==world_joust::Hit::Miss&&g.player_hit!=world_joust::Hit::Miss)joust_splinters.emit(g.player_contact);
            if(old_opponent_hit==world_joust::Hit::Miss&&g.opponent_hit!=world_joust::Hit::Miss)joust_splinters.emit(g.opponent_contact);
            joust_feedback=outgoing?std::string(world_joust::hit_name(contact.hit))+" HIT  +"+std::to_string(int(contact.hit)):"RIVAL HIT — "+std::string(world_joust::hit_name(contact.hit));
            if(outgoing&&old_opponent_hit==world_joust::Hit::Miss&&g.opponent_hit!=world_joust::Hit::Miss)joust_feedback+="  /  RIVAL "+std::string(world_joust::hit_name(g.opponent_hit));
            joust_feedback_age=0;joust_seen_contact=g.contact_serial;++joust_impacts;
            const bool incoming=old_opponent_hit==world_joust::Hit::Miss&&g.opponent_hit!=world_joust::Hit::Miss;
            if(incoming)joust_incoming_age=0;
            const bool helmet=g.player_hit==world_joust::Hit::Helmet||g.opponent_hit==world_joust::Hit::Helmet;
            if(g.player_hit==world_joust::Hit::Helmet)joust_feedback="UNHORSED THE RIVAL  +3";
            if(incoming&&g.opponent_hit==world_joust::Hit::Helmet)joust_feedback=outgoing?joust_feedback+"  /  YOU WERE UNHORSED":"YOU WERE UNHORSED";
            joust_audience_audio.hit(helmet);
            training_sounds.play(contact.hit==world_joust::Hit::Shield?2:1,incoming?91:78,0,audio_master_gain());
            preview_pad.rumble(incoming?.85f:.4f,incoming?.7f:.65f,incoming?230:165,preview_settings.vibration);
        }
        if(g.phase!=joust_last_phase){
            if(g.phase==world_joust::Phase::Result||g.phase==world_joust::Phase::MatchOver){joust_release();SDL_SetRelativeMouseMode(SDL_FALSE);}
            if(g.phase==world_joust::Phase::MatchOver)++joust_completed;
            joust_last_phase=g.phase;
        }
    }
    joust_splinters.update(step,paused);
    world_combat_vfx::Input dust_input;dust_input.paused=paused;dust_input.resolved_events=true;
    joust_dust.update(joust_dust_context,dust_input,step);
    const auto rider_eye=world_joust::rider_eye(g.player);
    cam.x=-1800+rider_eye.x;cam.y=64.08+rider_eye.y;cam.z=-10500+rider_eye.z;
    const bool fall_view=g.player.unhorsed||g.opponent.unhorsed;
    if(fall_view!=joust_fall_view){temporal_cut=true;joust_fall_view=fall_view;}
    cam.yaw=world_joust::pi-g.player.horse.yaw;cam.pitch=-.04f;cam.fly=false;cam.swimming=false;cam.vy=0;
    if(!joust_renderer.update(g,joust_idle_clock)){std::fprintf(stderr,"[Joust] Mounted GPU pose update failed\n");launch->exit_code=1;running=false;}
};
auto joust_camera=[&](Camera &eye){
    if(!joust_enabled)return;
    const auto view=world_joust_view::sample(joust_game,third_person,preview_settings.reduced_motion,joust_feedback_age);
    eye=cam;eye.x=-1800+view.eye.x;eye.y=64.08+view.eye.y;eye.z=-10500+view.eye.z;
    eye.yaw=view.yaw;eye.pitch=view.pitch;eye.fov=view.fov;
    if(!preview_settings.reduced_motion&&joust_landing_age<.25f)
        eye.pitch+=.018f*std::sin(joust_landing_age*34.f)*std::exp(-joust_landing_age*14.f);
};
auto joust_audio_ui=[&](){
    ImGui::Separator();ImGui::TextUnformatted("CROWD AUDIO");
    if(!joust_audience_audio.ready){ImGui::TextWrapped("Crowd recordings are unavailable.");return;}
    int value=joust_audience_audio.volume;
    if(ImGui::SliderInt("Crowd volume",&value,0,100,"%d%%"))joust_audience_audio.set_volume(value);
    if(ImGui::IsItemDeactivatedAfterEdit())joust_audience_audio.save_settings();
    if(ImGui::Button("Preview crowd")&&preview_pad.focused)joust_audience_audio.preview();
    ImGui::TextWrapped("%s",preview_pad.using_pad?"LB / RB: volume   Y: preview":"[ / ]: volume   C: preview");
    if(audio_master_gain()<=0)ImGui::TextDisabled("Master volume is muted.");
    else ImGui::TextDisabled("Crowd follows your master volume.");
    if(!joust_audience_audio.settings_message.empty())ImGui::TextWrapped("%s",joust_audience_audio.settings_message.c_str());
};
auto joust_ui=[&](const Camera &eye,bool loading){
    if(!joust_enabled||show_graphics||loading||launch->no_hud)return;
    auto &io=ImGui::GetIO();const float w=io.DisplaySize.x,h=io.DisplaySize.y,u=std::clamp(std::min(w/1600.f,h/900.f),.65f,1.35f);
    auto *dl=ImGui::GetForegroundDrawList();const ImU32 gold=IM_COL32(234,196,114,255),white=IM_COL32(245,242,230,255),muted=IM_COL32(188,197,197,255);
    auto text=[&](float x,float y,float size,ImU32 color,const std::string &s){dl->AddText(ImGui::GetFont(),size*u,{x,y},color,s.c_str());};
    auto center=[&](float y,float size,ImU32 color,const std::string &s){const float tw=ImGui::GetFont()->CalcTextSizeA(size*u,10000,0,s.c_str()).x;text((w-tw)*.5f,y,size,color,s);};
    const bool pad=preview_pad.using_pad&&preview_pad.get();auto &g=joust_game;
    if(joust_incoming_age<.48f){
        const float pulse=(1.f-joust_incoming_age/.48f)*(preview_settings.reduced_motion?.42f:1.f);
        const ImU32 edge=IM_COL32(169,35,22,int(100*pulse)),clear=IM_COL32(169,35,22,0);
        dl->AddRectFilledMultiColor({0,0},{34*u,h},edge,clear,clear,edge);
        dl->AddRectFilledMultiColor({w-34*u,0},{w,h},clear,edge,edge,clear);
    }

    const char *difficulties[]={"Easy","Medium","Hard"};
    if(g.phase==world_joust::Phase::Setup){
        ImGui::SetNextWindowPos({28*u,38*u},ImGuiCond_Always);ImGui::SetNextWindowSize({430*u,0},ImGuiCond_Always);ImGui::SetNextWindowBgAlpha(.94f);
        if(ImGui::Begin("Valwood Jousting Grounds",nullptr,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoNav)){
            ImGui::TextColored({.91f,.77f,.45f,1},"MOUNTED PRACTICE");ImGui::TextWrapped("Aim around the rival's shield and thrust as you pass. Either rider can be knocked from the saddle by a clean helmet hit. Keep your shield high when the rival aims at your head.");ImGui::Spacing();
            ImGui::TextUnformatted("Difficulty");
            for(int i=0;i<3;++i){if(i)ImGui::SameLine();if(ImGui::RadioButton(difficulties[i],joust_difficulty==i))joust_difficulty=i;}
            ImGui::TextWrapped("%s",joust_difficulty==0?"Broad openings; learn your timing and aim.":joust_difficulty==1?"The rival covers targets and aims more consistently.":"The rival tracks your lance. Change your aim late to find an opening.");
            ImGui::Spacing();if(ImGui::Button("-##passes")){joust_passes=std::max(1,joust_passes-2);}ImGui::SameLine();ImGui::Text("%d passes",joust_passes);ImGui::SameLine();if(ImGui::Button("+##passes"))joust_passes=std::min(9,joust_passes+2);
            ImGui::Separator();ImGui::TextWrapped("1 point: shield   2: body   3: helmet + unhorse\nHighest score wins. Practice match; no wager.");ImGui::Spacing();
            if(ImGui::Button("START JOUST",{-1,42*u}))joust_start();
            ImGui::TextWrapped("%s",pad?"D-pad left/right: difficulty\nD-pad up/down: passes   A: start":"Arrow left/right: difficulty\nArrow up/down: passes   Enter: start");
            ImGui::Separator();ImGui::TextWrapped("%s",pad?"Left stick: pace / steer\nRight stick: aim lance\nPress RT: thrust forward   LT: shield\nRelease RT to ready the next thrust\nRS click: camera   Menu: pause":"W/S: pace   A/D: steer\nMouse: aim lance\nPress LMB / Space: thrust forward\nRelease to ready the next thrust\nHold RMB / E: shield\nV: camera   Esc: pause   F10: graphics");
            joust_audio_ui();ImGui::TextDisabled("Crowd recording: Gregor Quendel / CC BY 4.0");
            if(ImGui::Button("Close practice"))running=false;
        }ImGui::End();return;
    }
    dl->AddRectFilled({22*u,20*u},{395*u,135*u},IM_COL32(12,23,28,222),6*u);
    text(40*u,32*u,16,gold,"VALWOOD / JOUSTING GROUNDS");text(40*u,59*u,29,white,std::to_string(g.player_score)+"  —  "+std::to_string(g.opponent_score));
    text(40*u,98*u,16,muted,"Pass "+std::to_string(g.pass_index+1)+" / "+std::to_string(g.setup.passes)+"  |  "+difficulties[g.setup.difficulty]);
    dl->AddRectFilled({w-298*u,20*u},{w-20*u,90*u},IM_COL32(12,23,28,222),6*u);
    text(w-280*u,29*u,17,white,g.opponent.unhorsed?"RIVAL / UNHORSED":"RIVAL / MOUNTED KNIGHT");
    text(w-280*u,57*u,16,muted,g.opponent.unhorsed?"Helmet strike / 3 points":"Closing: "+std::to_string(int(g.distance_to_opponent()))+" m");
    if(g.phase==world_joust::Phase::Countdown)center(h*.30f,65,gold,std::to_string(std::max(1,int(std::ceil(g.countdown())))));
    if(g.phase==world_joust::Phase::Charge){
        if(!g.player.unhorsed&&!g.opponent.unhorsed&&!g.opponent_resolved&&g.opponent.attack_target==world_joust::Hit::Helmet&&g.opponent.attack_commit>.05f){
            const std::string warning="RIVAL AIMS HIGH - GUARD";const float width=ImGui::GetFont()->CalcTextSizeA(18*u,10000,0,warning.c_str()).x;
            dl->AddRectFilled({(w-width)*.5f-12*u,h-155*u},{(w+width)*.5f+12*u,h-123*u},IM_COL32(12,23,28,215),5*u);center(h-149*u,18,gold,warning);
        }
        center(h-112*u,18,white,g.player.unhorsed?"YOU ARE UNHORSED — wait for the next pass":g.opponent.unhorsed?"RIVAL UNHORSED — clean helmet strike":g.player_broken?"LANCE BROKEN — finish your pass":g.player.thrust_phase==world_joust::ThrustPhase::Load?"SETTING YOUR THRUST":g.player.thrust_active?"THRUSTING":g.player.thrust_phase==world_joust::ThrustPhase::Recover?"LANCE RECOVERING":!g.player.thrust_armed?"RELEASE RT / LMB TO READY YOUR LANCE":pad?"AIM WITH RIGHT STICK — PRESS RT TO THRUST":"AIM WITH MOUSE — PRESS LMB / SPACE TO THRUST");
        center(h-80*u,16,muted,pad?"LS: pace / steer  |  RS: aim  |  RT: thrust  |  LT: shield  |  RS click: camera":"W/S: pace  |  A/D: steer  |  Mouse: aim  |  LMB / Space: thrust  |  RMB: shield  |  V: camera");
        dl->AddRectFilled({w*.5f-100*u,h-45*u},{w*.5f+100*u,h-35*u},IM_COL32(0,0,0,160),4*u);dl->AddRectFilled({w*.5f-100*u,h-45*u},{w*.5f-100*u+200*u*std::clamp(g.player.horse.speed/11.4f,0.f,1.f),h-35*u},gold,4*u);
        auto project=[&](world_joust::Vec3 p,ImVec2 &out){
            const V3 q{float(-1800+p.x-eye.x),float(64.08+p.y-eye.y),float(-10500+p.z-eye.z)},forward=eye.forward(),right=eye.right(),up=norm(cross(right,forward));const float depth=dot(q,forward);if(depth<.12f)return false;
            const float sx=dot(q,right)/(depth*std::tan(eye.fov*.5f)*eye.aspect),sy=dot(q,up)/(depth*std::tan(eye.fov*.5f));out={w*(.5f+sx*.5f),h*(.5f-sy*.5f)};return sx>-.95f&&sx<.95f&&sy>-.95f&&sy<.95f;
        };
        ImVec2 point;if(!g.player.unhorsed&&!g.opponent.unhorsed&&!g.player_broken&&project(g.player.lance_tip,point)){dl->AddCircle(point,9*u,g.player.thrust_active?gold:muted,24,1.5f*u);dl->AddLine({point.x-15*u,point.y},{point.x+15*u,point.y},white);dl->AddLine({point.x,point.y-15*u},{point.x,point.y+15*u},white);}
        if(!g.player.unhorsed&&!g.opponent.unhorsed&&g.setup.difficulty==0&&g.distance_to_opponent()<28){for(const auto p:{g.opponent.torso_center,g.opponent.helmet_center})if(project(p,point))dl->AddCircle(point,13*u,IM_COL32(226,192,120,165),24,1.2f*u);}
    }
    if(joust_feedback_age<1.8f&&!joust_feedback.empty()){
        float size=30*u;float tw=ImGui::GetFont()->CalcTextSizeA(size,10000,0,joust_feedback.c_str()).x;
        if(tw>w-90*u){size*=(w-90*u)/tw;tw=w-90*u;}
        const float x=(w-tw)*.5f,y=h*.21f;
        dl->AddRectFilled({x-18*u,y-9*u},{x+tw+18*u,y+size+9*u},IM_COL32(12,23,28,216),6*u);
        dl->AddText(ImGui::GetFont(),size,{x,y},gold,joust_feedback.c_str());
    }
    if(g.phase==world_joust::Phase::Result||g.phase==world_joust::Phase::MatchOver||joust_paused){
        ImGui::SetNextWindowPos({w*.5f,h*.5f},ImGuiCond_Always,{.5f,.5f});ImGui::SetNextWindowSize({440*u,0},ImGuiCond_Always);ImGui::SetNextWindowBgAlpha(.95f);
        if(ImGui::Begin("Joust result",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoNav)){
            if(joust_paused){ImGui::TextUnformatted("PAUSED");if(ImGui::Button("Resume / Enter / A",{-1,40*u}))joust_resume();joust_audio_ui();}
            else{ImGui::TextColored({.91f,.77f,.45f,1},"%s",g.phase==world_joust::Phase::MatchOver?(g.winner()>0?"VICTORY":g.winner()<0?"RIVAL WINS":"DRAW"):"PASS COMPLETE");
                ImGui::Text("You: %s  |  Rival: %s",world_joust::hit_name(g.player_hit),world_joust::hit_name(g.opponent_hit));ImGui::Text("Score %d : %d",g.player_score,g.opponent_score);ImGui::Spacing();
                if(ImGui::Button(g.phase==world_joust::Phase::MatchOver?"Joust again / Enter / A":"Next pass / Enter / A",{-1,40*u}))joust_confirm();}
            if(ImGui::Button("Setup / R",{-1,34*u}))joust_setup();
            if(ImGui::Button("Close practice",{-1,30*u}))running=false;
        }ImGui::End();
    }
};

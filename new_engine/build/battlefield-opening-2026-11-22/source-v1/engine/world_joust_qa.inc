// Private native mounted rehearsal. Inputs enter through real SDL events or
// SDL's owned virtual controller; game scores/contacts are never assigned here.
const char *joust_qa_dir_env=std::getenv("VALWOOD_QA_JOUST_DIR");
const char *joust_qa_mode_env=std::getenv("VALWOOD_QA_JOUST_MODE");
const std::filesystem::path joust_qa_dir=joust_qa_dir_env?joust_qa_dir_env:"";
const bool joust_qa_third_person_route=joust_qa_mode_env&&std::string(joust_qa_mode_env)=="controller-third";
const bool joust_qa_controller=joust_qa_mode_env&&(std::string(joust_qa_mode_env)=="controller"||joust_qa_third_person_route);
unsigned joust_qa_stage=0,joust_qa_captures=0,joust_qa_inputs=0,joust_qa_contacts=0,joust_qa_geometry_checks=0,joust_qa_gl_errors=0;
std::uint64_t joust_qa_since=0,joust_qa_last_frame=0,joust_qa_audio_begin=0;
bool joust_qa_valid=false,joust_qa_done=false,joust_qa_failed=false,joust_qa_previous_valid=false;
float joust_qa_pause_elapsed=0,joust_qa_max_grip_error=0;
float joust_qa_stroke_begin=0,joust_qa_direction_begin=0,joust_qa_peak_extension=0,joust_qa_peak_amount=0;
std::uint64_t joust_qa_stroke_serial=0,joust_qa_pass_serial=0;
unsigned joust_qa_direction_index=0,joust_qa_active_contacts=0,joust_qa_timed_pushes=0,joust_qa_cross_source_checks=0;
unsigned joust_qa_received_hits[4]={},joust_qa_falling_frames[2]={},joust_qa_grounded_frames[2]={};
unsigned joust_qa_peak_rendered_unhorsed=0,joust_qa_peak_rendered_grounded=0;
bool joust_qa_fall_captured[2]={},joust_qa_ground_captured[2]={},joust_qa_empty_horse_captured[2]={};
bool joust_qa_ejection_captured[2]={},joust_qa_settled_captured[2]={};
float joust_qa_peak_fall_pose[2]={},joust_qa_peak_fall_settle[2]={},joust_qa_peak_ground_time[2]={};
int joust_qa_motion_sample[2]={-1,-1};
unsigned joust_qa_observed_landings=0,joust_qa_landing_audio_checks=0,joust_qa_dust_pause_checks=0;
bool joust_qa_landing_pause_started=false;
float joust_qa_dust_pause_age=0;
unsigned joust_qa_dust_pause_bursts=0,joust_qa_dust_pause_landings=0;
world_combat_vfx::Stage joust_qa_dust_pause_stage;
unsigned long long joust_qa_previous_audio=0;
bool joust_qa_first_person_fall=false,joust_qa_third_person_fall=false;
std::uint64_t joust_qa_audience_begin=0;
JoustAudienceAudio::Probe joust_qa_muted_pcm,joust_qa_crowd_pcm,joust_qa_cheer_pcm;
int joust_qa_audio_volume=100;
std::uint64_t joust_qa_incoming_captured_serial=0;
int joust_qa_incoming_captured_pass=-1;
float joust_qa_peak_reaction=0,joust_qa_peak_incoming_view=0,joust_qa_peak_crowd_swell=0;
bool joust_qa_timed_push=false,joust_qa_no_thrust_pass=false;
float joust_qa_direction_up=0,joust_qa_direction_down=0,joust_qa_direction_left=0,joust_qa_direction_right=0;
world_joust::Vec3 joust_qa_pause_position;
world_joust::Game joust_qa_previous;
std::string joust_qa_error,joust_qa_pending;
std::vector<std::string> joust_qa_checks,joust_qa_capture_paths,joust_qa_rows,joust_qa_direction_rows,joust_qa_fall_rows;
int joust_qa_player_points=0,joust_qa_opponent_points=0,joust_qa_recorded_pass=-1;
auto joust_qa_fail=[&](const std::string &error){
    if(joust_qa_failed)return;
    joust_qa_failed=true;joust_qa_error=error;launch->exit_code=1;running=false;
    std::fprintf(stderr,"[Joust QA] FAIL stage %u: %s\n",joust_qa_stage,error.c_str());std::fflush(stderr);
};
auto joust_qa_check=[&](bool ok,const char *what){if(!ok){joust_qa_fail(what);return false;}return true;};
auto joust_qa_pass=[&](const char *what){if(std::find(joust_qa_checks.begin(),joust_qa_checks.end(),what)==joust_qa_checks.end())joust_qa_checks.push_back(what);};
auto joust_qa_key=[&](SDL_Keycode key){
    SDL_Event e{};e.type=SDL_KEYDOWN;e.key.windowID=SDL_GetWindowID(window);e.key.state=SDL_PRESSED;e.key.keysym.sym=key;e.key.keysym.scancode=SDL_GetScancodeFromKey(key);e.key.keysym.unused=0x56414c54u;
    if(SDL_PushEvent(&e)!=1){joust_qa_fail("SDL rejected tagged key press");return;}
    e.type=SDL_KEYUP;e.key.state=SDL_RELEASED;if(SDL_PushEvent(&e)!=1){joust_qa_fail("SDL rejected tagged key release");return;}joust_qa_inputs+=2;
};
auto joust_qa_button=[&](Uint8 button,bool down){
    SDL_Event e{};e.type=down?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;e.button.windowID=SDL_GetWindowID(window);e.button.which=0x56414c54u;e.button.button=button;e.button.state=down?SDL_PRESSED:SDL_RELEASED;
    if(SDL_PushEvent(&e)!=1){joust_qa_fail("SDL rejected tagged mouse button");return;}++joust_qa_inputs;
};
auto joust_qa_mouse=[&](int x,int y){
    SDL_Event e{};e.type=SDL_MOUSEMOTION;e.motion.windowID=SDL_GetWindowID(window);e.motion.which=0x56414c54u;e.motion.xrel=x;e.motion.yrel=y;
    if(SDL_PushEvent(&e)!=1){joust_qa_fail("SDL rejected tagged relative lance aim");return;}++joust_qa_inputs;
};
auto joust_qa_pad_press=[&](SDL_GameControllerButton b){controller_test_pad.press(b);SDL_JoystickUpdate();++joust_qa_inputs;};
auto joust_qa_axis=[&](SDL_GameControllerAxis a,Sint16 v){controller_test_pad.axis(a,v);SDL_JoystickUpdate();++joust_qa_inputs;};
auto joust_qa_aim=[&](float x,float y){
    // Invert the actual radial deadzone so requested target angles are stable
    // across the user's copied controller preference, without changing it.
    const float radius=std::hypot(x,y),dead=std::clamp(preview_settings.deadzone,.05f,.45f);
    const float raw=radius>0?(dead+(1.f-dead)*std::min(radius,1.f))/radius:0;
    joust_qa_axis(SDL_CONTROLLER_AXIS_RIGHTX,Sint16(std::clamp(x*raw,-1.f,1.f)*32767));
    joust_qa_axis(SDL_CONTROLLER_AXIS_RIGHTY,Sint16(std::clamp(-y*raw,-1.f,1.f)*32767));
};
auto joust_qa_set_aim=[&](float screen_x,float up){
    if(joust_qa_controller)joust_qa_aim(screen_x,up);
    else joust_qa_mouse(int(std::lround((screen_x+joust_aim_x)/.0035f)),int(std::lround((joust_aim_y-up)/.0035f)));
};
auto joust_qa_push=[&](bool down){if(joust_qa_controller)joust_qa_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,down?32767:-32768);else joust_qa_button(SDL_BUTTON_LEFT,down);};
auto joust_qa_focus=[&](bool focused){
    SDL_Event e{};e.type=SDL_WINDOWEVENT;e.window.windowID=SDL_GetWindowID(window);e.window.event=focused?SDL_WINDOWEVENT_FOCUS_GAINED:SDL_WINDOWEVENT_FOCUS_LOST;
    if(SDL_PushEvent(&e)!=1){joust_qa_fail("SDL rejected private focus lifecycle event");return;}++joust_qa_inputs;
};
auto joust_qa_confirm=[&](){if(joust_qa_controller)joust_qa_pad_press(SDL_CONTROLLER_BUTTON_A);else joust_qa_key(SDLK_RETURN);};
auto joust_qa_pause=[&](){if(joust_qa_controller)joust_qa_pad_press(SDL_CONTROLLER_BUTTON_START);else joust_qa_key(SDLK_ESCAPE);};
auto joust_qa_camera_toggle=[&](){if(joust_qa_controller)joust_qa_pad_press(SDL_CONTROLLER_BUTTON_RIGHTSTICK);else joust_qa_key(SDLK_v);};
auto joust_qa_next_stage=[&](unsigned stage,std::uint64_t frame){joust_qa_stage=stage;joust_qa_since=frame;};
auto joust_qa_dust_stage=[&](){
    world_combat_vfx::Stage result;const float right[3]={1,0,0},up[3]={0,1,0};
    joust_dust.stage({0,0,0},right,up,result);return result;
};
auto joust_qa_same_dust=[&](const world_combat_vfx::Stage &a,const world_combat_vfx::Stage &b){
    if(a.count!=b.count||a.particles!=b.particles||a.trails!=b.trails)return false;
    for(unsigned i=0;i<a.count;++i){
        for(unsigned c=0;c<4;++c)if(a.instances[i].corners[c]!=b.instances[i].corners[c])return false;
        if(a.instances[i].color!=b.instances[i].color||a.instances[i].style!=b.instances[i].style)return false;
    }
    return true;
};
auto joust_qa_step=[&](std::uint64_t frame){
    if(!joust_qa||joust_qa_done||joust_qa_failed)return;
    joust_qa_last_frame=frame;
    controller_test_pad.tick();
    if(!joust_qa_valid){
        std::ifstream marker(joust_qa_dir/".valwood-joust-qa");std::string token,extra;marker>>token;
        if(!joust_qa_check(joust_enabled&&launch->frames&&!launch->benchmark&&joust_qa_dir.is_absolute()&&token=="VALWOOD_PRIVATE_JOUST_V1"&&!(marker>>extra),"Missing marked private native joust output directory"))return;
        if(!joust_qa_check(!joust_qa_mode_env||std::string(joust_qa_mode_env)=="keyboard"||std::string(joust_qa_mode_env)=="controller"||joust_qa_third_person_route,"Unknown joust QA mode"))return;
        if(!joust_qa_check(joust_renderer.ready&&joust_renderer.failures()==0,"Mounted assets are not ready"))return;
        if(!joust_qa_check(controller_test_pad.joystick&&preview_pad.id()==SDL_JoystickInstanceID(controller_test_pad.joystick),"Owned actual SDL virtual controller missing"))return;
        joust_qa_valid=true;joust_qa_audio_begin=training_sounds.events;joust_qa_previous_audio=training_sounds.events;joust_qa_audience_begin=joust_audience_audio.events;joust_qa_pass("private-marked-output-and-owned-SDL-controller");joust_qa_since=frame;
        SDL_Event focus{};focus.type=SDL_WINDOWEVENT;focus.window.windowID=SDL_GetWindowID(window);focus.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;
        if(SDL_PushEvent(&focus)!=1){joust_qa_fail("SDL rejected private focus lifecycle event");return;}++joust_qa_inputs;
    }
    if(frame+12>=std::uint64_t(launch->frames)){joust_qa_fail("Bounded native joust rehearsal timed out");return;}
    auto &g=joust_game;
    joust_qa_peak_crowd_swell=std::max(joust_qa_peak_crowd_swell,joust_renderer.crowd_swell());
    joust_qa_peak_rendered_unhorsed=std::max(joust_qa_peak_rendered_unhorsed,joust_renderer.rendered_unhorsed_riders());
    joust_qa_peak_rendered_grounded=std::max(joust_qa_peak_rendered_grounded,joust_renderer.rendered_grounded_riders());
    if(joust_qa_previous_valid&&g.pass_index==joust_qa_previous.pass_index){
        const unsigned edges=unsigned(!joust_qa_previous.player.fall_grounded&&g.player.fall_grounded)+unsigned(!joust_qa_previous.opponent.fall_grounded&&g.opponent.fall_grounded);
        if(edges){
            joust_qa_observed_landings+=edges;
            if(!joust_qa_check(training_sounds.events>=joust_qa_previous_audio+2&&joust_dust.stats().active_particles>0&&joust_landing_age<.04f,"Real rider ground edge did not dispatch dust and paired landing audio"))return;
            ++joust_qa_landing_audio_checks;
        }
    }
    if(!joust_qa_check(joust_landings==joust_qa_observed_landings&&joust_dust.stats().dust_bursts==joust_qa_observed_landings,"Landing dust repeated or reset without an actual grounded transition"))return;
    joust_qa_previous_audio=training_sounds.events;
    // Observe real previous/current simulation state. The receipt point must
    // lie inside the corresponding target's translating swept proxy volume.
    // 3% tolerance accounts for the two 120Hz substeps inside one render frame.
    if(joust_qa_previous_valid&&g.pass_index==joust_qa_previous.pass_index&&g.contact_serial>joust_qa_previous.contact_serial){
        for(int side=0;side<2;++side){
            const auto hit=side?g.opponent_hit:g.player_hit,old=side?joust_qa_previous.opponent_hit:joust_qa_previous.player_hit;
            if(hit==world_joust::Hit::Miss||old!=world_joust::Hit::Miss)continue;
            const auto &contact=side?g.opponent_contact:g.player_contact;
            if(!joust_qa_check(contact.active_thrust&&contact.thrust_serial>0&&contact.thrust_time>=world_joust::thrust_load_end-1.e-4f&&contact.thrust_time<=world_joust::thrust_active_end+1.e-4f&&contact.thrust_amount>=0&&contact.thrust_amount<=1,"Physical contact did not carry an active timed thrust receipt"))return;
            ++joust_qa_active_contacts;
            const auto &target=side?g.player:g.opponent,&before=side?joust_qa_previous.player:joust_qa_previous.opponent;
            if(!joust_qa_check(target.hit_received==hit&&target.hit_serial>before.hit_serial,"Actual mounted hit did not reach the recipient reaction state"))return;
            ++joust_qa_received_hits[unsigned(hit)];
            if(hit==world_joust::Hit::Helmet&&!joust_qa_check(target.unhorsed,"An actual helmet hit did not unhorse its recipient"))return;
            const auto c0=hit==world_joust::Hit::Shield?before.shield_center:hit==world_joust::Hit::Helmet?before.helmet_center:before.torso_center;
            const auto c1=hit==world_joust::Hit::Shield?target.shield_center:hit==world_joust::Hit::Helmet?target.helmet_center:target.torso_center;
            const auto radii=(hit==world_joust::Hit::Shield?world_joust::shield_radii:hit==world_joust::Hit::Helmet?world_joust::helmet_radii:world_joust::torso_radii)*1.03f;
            float t=0;world_joust::Vec3 normal;
            if(!joust_qa_check(contact.speed>=9&&world_joust::swept_ellipsoid(contact.point,contact.point,c0,c1,radii,t,normal),"Actual contact receipt is outside the swept mounted target geometry"))return;
            ++joust_qa_contacts;++joust_qa_geometry_checks;
            if(joust_qa_pending.empty())joust_qa_pending="pass"+std::to_string(g.pass_index+1)+"-impact";
        }
    }
    if(g.phase!=world_joust::Phase::Setup){
        // Once unhorsed, the source hand follows the articulated fall pose;
        // released gear is no longer a held, scoring mounted contact segment.
        if(!g.player.unhorsed){
            const float grip_error=world_joust::length(g.grip_offset()-joust_renderer.grip);joust_qa_max_grip_error=std::max(joust_qa_max_grip_error,grip_error);
            if(!joust_qa_check(grip_error<.001f&&std::abs(world_joust::length(g.player.lance_tip-g.player.lance_start)-world_joust::Game::lance_length())<.001f,"Mounted lance grip or 4.8m contact geometry diverged"))return;
            const auto axial=world_joust::normalized(g.player.lance_tip-g.player.lance_start);
            joust_qa_peak_extension=std::max(joust_qa_peak_extension,world_joust::dot(g.player.lance_start-g.player.lance_rest_grip,axial));
            joust_qa_peak_amount=std::max(joust_qa_peak_amount,g.player.thrust_amount);
        }
        joust_qa_peak_reaction=std::max({joust_qa_peak_reaction,g.player.reaction_amount,g.opponent.reaction_amount});
        if(g.player.reaction_age<.48f){
            Camera actual_eye=cam;joust_camera(actual_eye);
            const float eye_motion=std::abs(actual_eye.pitch-cam.pitch)+std::abs(actual_eye.yaw-cam.yaw);
            joust_qa_peak_incoming_view=std::max(joust_qa_peak_incoming_view,eye_motion);
            if(joust_qa_pending.empty()&&g.player.hit_received!=world_joust::Hit::Miss&&g.player.reaction_age>.07f&&g.player.reaction_age<.20f&&(g.pass_index!=joust_qa_incoming_captured_pass||g.player.hit_serial!=joust_qa_incoming_captured_serial)){
                if(!joust_qa_check(joust_incoming_age<.48f,"Actual incoming hit did not activate its camera/HUD feedback envelope"))return;
                joust_qa_incoming_captured_serial=g.player.hit_serial;joust_qa_incoming_captured_pass=g.pass_index;joust_qa_pending="player-incoming-hit";
            }
        }
        for(unsigned side=0;side<2;++side){
            const auto &r=side?g.opponent:g.player;
            if(!r.unhorsed)continue;
            if(!joust_qa_check(r.hit_received==world_joust::Hit::Helmet&&std::isfinite(r.fall_position.x)&&std::isfinite(r.fall_position.y)&&std::isfinite(r.fall_position.z),"Unhorsed rider lacks a real helmet receipt or finite fall state"))return;
            if(r.fall_grounded)++joust_qa_grounded_frames[side];else ++joust_qa_falling_frames[side];
            joust_qa_peak_fall_pose[side]=std::max(joust_qa_peak_fall_pose[side],r.fall_pose_progress);
            joust_qa_peak_fall_settle[side]=std::max(joust_qa_peak_fall_settle[side],r.fall_settle_progress);
            joust_qa_peak_ground_time[side]=std::max(joust_qa_peak_ground_time[side],r.fall_ground_time);
            const std::string who=side?"rival":"player";
            if(joust_paused)continue;
            std::string pose;
            if(joust_qa_pending.empty()&&!r.fall_grounded&&r.fall_age>.08f&&!joust_qa_ejection_captured[side]){
                if(!side){
                    if(!joust_qa_check(joust_fall_view&&(joust_qa_third_person_route?third_person:!third_person),"Player helmet fall camera did not match the tested mounted camera mode"))return;
                    if(joust_qa_third_person_route)joust_qa_third_person_fall=true;else joust_qa_first_person_fall=true;
                }
                pose="ejection";joust_qa_ejection_captured[side]=true;
            }else if(joust_qa_pending.empty()&&!r.fall_grounded&&r.fall_age>.45f&&!joust_qa_fall_captured[side]){
                pose="midfall";joust_qa_fall_captured[side]=true;
            }else if(joust_qa_pending.empty()&&r.fall_grounded&&!joust_qa_ground_captured[side]){
                if(!joust_qa_check(std::abs(r.fall_position.y-world_joust::fall_floor_height)<.001f,"Grounded rider does not rest on the core floor plane"))return;
                pose="first-floor";joust_qa_ground_captured[side]=true;
            }else if(joust_qa_pending.empty()&&r.fall_grounded&&r.fall_ground_time>=1.05f&&r.fall_settled&&!joust_qa_settled_captured[side]){
                if(!joust_qa_check(r.fall_pose_progress>.95f&&r.fall_settle_progress>.95f,"Grounded rider retained the frozen mounted pose instead of settling"))return;
                pose="settled-body";joust_qa_settled_captured[side]=true;
            }else if(joust_qa_pending.empty()&&r.fall_grounded&&world_joust::length(r.horse.position-r.fall_position)>4.f&&!joust_qa_empty_horse_captured[side]){
                pose="riderless-horse";joust_qa_empty_horse_captured[side]=true;
            }
            if(pose.empty()&&joust_qa_pending.empty()&&r.fall_age<1.6f){const int sample=int(r.fall_age/.075f);if(sample>joust_qa_motion_sample[side]){joust_qa_motion_sample[side]=sample;pose="motion-"+std::to_string(sample);}}
            if(!pose.empty()){
                joust_qa_pending=who+"-"+pose;
                std::ostringstream row;row<<"{\"pass\":"<<g.pass_index+1<<",\"rider\":"<<world_json::quote(who)<<",\"capture\":"<<world_json::quote(joust_qa_pending)<<",\"fall_age\":"<<r.fall_age<<",\"ground_time\":"<<r.fall_ground_time<<",\"pose_progress\":"<<r.fall_pose_progress<<",\"settle_progress\":"<<r.fall_settle_progress<<",\"settled\":"<<(r.fall_settled?"true":"false")<<",\"grounded\":"<<(r.fall_grounded?"true":"false")<<"}";joust_qa_fall_rows.push_back(row.str());
            }
        }
    }
    joust_qa_previous=g;joust_qa_previous_valid=true;
    if(!joust_qa_landing_pause_started&&joust_qa_stage==12&&joust_qa_pending.empty()&&joust_qa_observed_landings>0&&joust_dust.stats().active_particles>0){
        joust_qa_landing_pause_started=true;joust_qa_pause_elapsed=g.elapsed;joust_qa_dust_pause_age=joust_landing_age;
        joust_qa_dust_pause_bursts=joust_dust.stats().dust_bursts;joust_qa_dust_pause_landings=joust_landings;joust_qa_dust_pause_stage=joust_qa_dust_stage();
        joust_qa_pause();joust_qa_next_stage(30,frame);
    }
    const auto age=frame-joust_qa_since;
    switch(joust_qa_stage){
    case 0:
        if(age<8||destination_loading)return;
        if(joust_qa_controller)joust_qa_pad_press(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);else joust_qa_key(SDLK_DOWN);
        joust_qa_next_stage(1,frame);return;
    case 1:
        if(joust_qa_controller&&age==4)joust_qa_pad_press(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
        if(age<10)return;
        if(!joust_qa_check(g.phase==world_joust::Phase::Setup&&joust_difficulty==(joust_qa_controller?2:0)&&joust_passes==(joust_qa_controller?3:1),"Actual setup difficulty/pass selection did not follow SDL inputs"))return;
        joust_qa_pass("SDL-setup-difficulty-and-pass-selection");joust_qa_pending="setup";joust_qa_next_stage(2,frame);return;
    case 2:
        if(!joust_qa_pending.empty()||age<3)return;
        if(!joust_qa_check(audio_master_gain()>.99f&&joust_audience_audio.ready,"Saved master gain was not initialized before the atlas audio scene"))return;
        joust_qa_audio_volume=joust_audience_audio.volume;joust_audience_audio.set_volume(0);joust_audience_audio.update(0,true);
        if(!joust_qa_check(joust_audience_audio.begin_probe(),"Cannot meter actual native mixer output"))return;
        joust_qa_next_stage(40,frame);return;
    case 40:
        if(age<35)return;
        joust_qa_muted_pcm=joust_audience_audio.probe();joust_audience_audio.end_probe();
        joust_audience_audio.set_volume(joust_qa_audio_volume);joust_audience_audio.update(0,true);
        if(!joust_qa_check(joust_audience_audio.begin_probe(),"Cannot meter restored native crowd output"))return;
        joust_qa_next_stage(41,frame);return;
    case 41:
        if(age<35)return;
        joust_qa_crowd_pcm=joust_audience_audio.probe();joust_audience_audio.end_probe();
        if(!joust_qa_check(joust_qa_muted_pcm.samples>0&&joust_qa_crowd_pcm.nonzero>1000&&joust_qa_crowd_pcm.rms()>.01&&joust_qa_crowd_pcm.rms()>joust_qa_muted_pcm.rms()+.003,"Crowd did not contribute audible PCM above the native scene's muted baseline"))return;
        joust_audience_audio.preview();
        if(!joust_qa_check(joust_audience_audio.begin_probe(),"Cannot meter actual crowd cheer preview"))return;
        joust_qa_next_stage(42,frame);return;
    case 42:
        if(age<35)return;
        joust_qa_cheer_pcm=joust_audience_audio.probe();joust_audience_audio.end_probe();
        if(!joust_qa_check(joust_qa_cheer_pcm.nonzero>1000&&joust_qa_cheer_pcm.rms()>.01,"Native cheer preview had no audible mixer output"))return;
        joust_qa_pass("actual-postmix-crowd-muted-enabled-preview-and-saved-master-gain");
        joust_qa_confirm();joust_qa_next_stage(3,frame);return;
    case 3:
        if(age<6)return;
        if(!joust_qa_check(g.phase==world_joust::Phase::Countdown&&g.setup.difficulty==(joust_qa_controller?2:0)&&g.setup.passes==(joust_qa_controller?3:1)&&g.setup.practice,"SDL start did not configure the actual mounted match"))return;
        joust_qa_push(false);joust_qa_set_aim(0,.8f);joust_qa_direction_index=0;joust_qa_direction_begin=g.elapsed;joust_qa_pass_serial=g.thrust_serial;
        joust_qa_next_stage(4,frame);return;
    case 4:
        if(g.elapsed-joust_qa_direction_begin<.32f)return;
        {
            static const char *names[]={"up","down","left","right"};
            static const float desired_x[]={0,0,-.8f,.8f},desired_y[]={.8f,-.8f,0,0};
            const unsigned i=joust_qa_direction_index;
            if(!joust_qa_check(i<4&&g.player.couched&&!g.player.thrust_active&&g.thrust_serial==joust_qa_pass_serial&&g.player_hit==world_joust::Hit::Miss,"Ready lance did not aim independently of RT"))return;
            if(!joust_qa_check(std::abs(g.player.aim_x+desired_x[i])<.12f&&std::abs(g.player.aim_y-desired_y[i])<.12f,"Actual right-stick direction did not reach the mounted lance"))return;
            const auto tip=g.player.lance_tip-g.player.lance_start;const auto right=cam.right();const float projected=tip.x*right.x+tip.z*right.z;
            if(i==0)joust_qa_direction_up=tip.y;else if(i==1)joust_qa_direction_down=tip.y;else if(i==2)joust_qa_direction_left=projected;else joust_qa_direction_right=projected;
            std::ostringstream row;row<<"{\"pass\":"<<g.pass_index+1<<",\"direction\":"<<world_json::quote(names[i])<<",\"physical_right_stick\":"<<(joust_qa_controller?"true":"false")<<",\"aim_x\":"<<g.player.aim_x<<",\"aim_y\":"<<g.player.aim_y<<",\"screen_right_metres\":"<<projected<<",\"vertical_metres\":"<<tip.y<<",\"camera_right\":["<<right.x<<","<<right.y<<","<<right.z<<"],\"thrust_serial\":"<<g.thrust_serial<<"}";joust_qa_direction_rows.push_back(row.str());
            joust_qa_pending=std::string(g.pass_index?"reverse-aim-":"aim-")+names[i];++joust_qa_direction_index;
            if(joust_qa_direction_index<4){joust_qa_set_aim(desired_x[joust_qa_direction_index],desired_y[joust_qa_direction_index]);joust_qa_direction_begin=g.elapsed;joust_qa_since=frame;return;}
            if(!joust_qa_check(joust_qa_direction_up-joust_qa_direction_down>.6f&&joust_qa_direction_right-joust_qa_direction_left>.5f,"Visible lance tip did not move up/down/left/right in the actual camera frame"))return;
            joust_qa_pass(g.pass_index?"SDL-four-direction-aim-correct-in-reverse-camera-frame":"SDL-four-direction-aim-moves-ready-lance-in-camera-frame");joust_qa_set_aim(0,g.pass_index?-.2f:.8f);
            if(g.pass_index){joust_qa_timed_push=false;joust_qa_no_thrust_pass=false;if(third_person&&!joust_qa_third_person_route)joust_qa_camera_toggle();joust_qa_next_stage(12,frame);}else joust_qa_next_stage(5,frame);return;
        }
    case 5:
        if(!joust_qa_pending.empty()||g.phase!=world_joust::Phase::Charge||g.phase_time<.12f)return;
        if(!joust_qa_check(g.player.horse.speed>0&&joust_controls_armed&&g.player.thrust_armed&&g.player_hit==world_joust::Hit::Miss,"Neutral ready lance did not permit a new committed push"))return;
        joust_qa_stroke_serial=g.thrust_serial;joust_qa_stroke_begin=g.elapsed;joust_qa_push(true);joust_qa_next_stage(6,frame);return;
    case 6:
        if(g.elapsed-joust_qa_stroke_begin<.32f)return;
        if(!joust_qa_check(g.thrust_serial==joust_qa_stroke_serial+1&&g.player.thrust_active&&joust_qa_peak_amount>.95f&&joust_qa_peak_extension>.55f&&g.player_hit==world_joust::Hit::Miss,"RT did not produce the larger visible forward lance extension"))return;
        joust_qa_pass("SDL-RT-rising-edge-produces-forward-extension");joust_qa_pending="thrust-extended";joust_qa_next_stage(7,frame);return;
    case 7:
        if(!joust_qa_pending.empty()||g.elapsed-joust_qa_stroke_begin<.95f)return;
        if(!joust_qa_check(g.thrust_serial==joust_qa_stroke_serial+1&&!g.player.thrust_active&&!g.player.thrust_armed&&g.player.thrust_amount<.03f&&g.player_hit==world_joust::Hit::Miss,"Held RT repeated or an early expired push scored a contact"))return;
        joust_qa_pass("held-RT-does-not-auto-repeat-and-early-push-expires");joust_qa_pending="charge-third-person";
        joust_qa_pause_elapsed=g.elapsed;joust_qa_pause_position=g.player.horse.position;joust_qa_pause();joust_qa_next_stage(8,frame);return;
    case 8:
        if(age<10)return;
        if(!joust_qa_check(joust_paused&&g.elapsed==joust_qa_pause_elapsed&&world_joust::length(g.player.horse.position-joust_qa_pause_position)<1.e-6f,"Pause did not freeze actual mounted simulation"))return;
        if(!joust_qa_check(joust_audience_audio.ready&&!joust_audience_audio.active,"Pause did not stop actual decoded audience ambience"))return;
        joust_qa_pass("pause-freezes-horse-and-simulation-clock");joust_qa_pending="paused";
        joust_qa_next_stage(9,frame);return;
    case 9:
        if(!joust_qa_pending.empty()||age<3)return;
        joust_qa_pause();joust_qa_next_stage(10,frame);return;
    case 10:
        if(age<16)return;
        if(!joust_qa_check(!joust_paused&&g.elapsed>joust_qa_pause_elapsed&&g.thrust_serial==joust_qa_stroke_serial+1&&!g.player.thrust_active,"Held input started a new thrust across pause/resume"))return;
        if(!joust_qa_check(joust_audience_audio.ready&&joust_audience_audio.active,"Audience ambience did not resume with the actual game"))return;
        if(joust_qa_controller&&!joust_qa_check(!joust_controls_armed&&!g.player.thrust_armed,"Held RT rearmed either host or core across pause/resume"))return;
        joust_qa_pass("held-RT-disarmed-after-resume-until-neutral");joust_qa_push(false);joust_qa_next_stage(11,frame);return;
    case 11:
        if(age<8)return;
        if(!joust_qa_check(joust_controls_armed&&g.player.thrust_armed,"Release did not rearm both actual host and committed stroke"))return;
        joust_qa_pass("RT-release-rearms-next-committed-thrust");joust_qa_push(true);joust_qa_stroke_serial=g.thrust_serial;joust_qa_stroke_begin=g.elapsed;joust_qa_next_stage(16,frame);return;
    case 16:
        if(g.elapsed-joust_qa_stroke_begin<.22f)return;
        if(!joust_qa_check(g.thrust_serial==joust_qa_stroke_serial+1&&g.player.thrust_active,"Released and re-pressed RT did not begin a second committed stroke"))return;
        joust_qa_stroke_serial=g.thrust_serial;joust_qa_pause_elapsed=g.elapsed;joust_qa_pause_position=g.player.horse.position;joust_qa_focus(false);joust_qa_next_stage(17,frame);return;
    case 17:
        if(age<10)return;
        if(!joust_qa_check(joust_paused&&!preview_pad.focused&&g.elapsed==joust_qa_pause_elapsed&&world_joust::length(g.player.horse.position-joust_qa_pause_position)<1.e-6f&&!g.player.thrust_active,"Focus loss did not freeze horses and cancel the actual active thrust"))return;
        joust_qa_pass("SDL-focus-loss-cancels-thrust-and-freezes-horse");joust_qa_pending="focus-paused";joust_qa_focus(true);joust_qa_next_stage(18,frame);return;
    case 18:
        if(!joust_qa_pending.empty()||age<6)return;
        if(!joust_qa_check(preview_pad.focused&&joust_paused&&g.elapsed==joust_qa_pause_elapsed,"Focus gain resumed the mounted game without confirmation"))return;
        // A real keyboard action deliberately resumes a controller session:
        // physical RT remains held, so changing the active source must not
        // bypass either the host neutral gate or the core thrust gate.
        joust_qa_key(SDLK_ESCAPE);joust_qa_next_stage(19,frame);return;
    case 19:
        if(age<16)return;
        if(!joust_qa_check(!joust_paused&&g.thrust_serial==joust_qa_stroke_serial&&!g.player.thrust_active,"Held RT restarted the interrupted stroke after focus/resume"))return;
        if(joust_qa_controller&&!joust_qa_check(!joust_controls_armed&&!g.player.thrust_armed,"Focus/resume rearmed held physical RT"))return;
        joust_qa_pass("focus-resume-held-RT-requires-release-before-push");
        if(joust_qa_controller){
            if(!joust_qa_check(!preview_pad.using_pad&&preview_pad.right_trigger>.9f,"Tagged keyboard resume did not preserve the physical held RT witness"))return;
            ++joust_qa_cross_source_checks;joust_qa_pass("keyboard-resume-held-RT-cannot-bypass-neutral-gate");
            // Virtual game-controller triggers map [-32768,32767] to [0,1].
            // .25 is below the press threshold and above the release threshold.
            joust_qa_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-16384);joust_qa_next_stage(23,frame);return;
        }
        joust_qa_push(false);joust_qa_next_stage(20,frame);return;
    case 23:
        if(age<8)return;
        if(!joust_qa_check(!preview_pad.using_pad&&preview_pad.right_trigger>.20f&&preview_pad.right_trigger<.30f&&!joust_controls_armed&&!g.player.thrust_armed&&!g.player.thrust_active&&g.thrust_serial==joust_qa_stroke_serial,"Quarter RT incorrectly rearmed while keyboard remained active"))return;
        ++joust_qa_cross_source_checks;joust_qa_set_aim(.8f,0);joust_qa_next_stage(24,frame);return;
    case 24:
        if(age<8)return;
        if(!joust_qa_check(preview_pad.using_pad&&preview_pad.right_trigger>.20f&&preview_pad.right_trigger<.30f&&!joust_controls_armed&&!g.player.thrust_armed&&!g.player.thrust_active&&g.thrust_serial==joust_qa_stroke_serial,"Right-stick source switch rearmed the still-held quarter RT"))return;
        ++joust_qa_cross_source_checks;joust_qa_pass("quarter-RT-stays-disarmed-through-right-stick-source-switch");joust_qa_pending="cross-source-disarmed";
        joust_qa_push(false);joust_qa_set_aim(0,.8f);joust_qa_next_stage(20,frame);return;
    case 20:
        if(age<8)return;
        if(!joust_qa_check(joust_controls_armed&&g.player.thrust_armed,"Neutral trigger did not rearm after focus interruption"))return;
        joust_qa_camera_toggle();joust_qa_next_stage(21,frame);return;
    case 21:
        if(age<6)return;
        if(!joust_qa_check(!third_person,"Actual camera toggle did not enter first person"))return;
        joust_qa_pass("SDL-first-and-third-person-camera-toggle");joust_qa_pending="charge-first-person";joust_qa_next_stage(22,frame);return;
    case 22:
        if(!joust_qa_pending.empty()||age<4)return;
        joust_qa_camera_toggle();joust_qa_timed_push=false;joust_qa_no_thrust_pass=false;joust_qa_pass_serial=g.thrust_serial;joust_qa_next_stage(12,frame);return;
    case 30:
        if(age<10)return;
        if(!joust_qa_check(joust_paused&&g.elapsed==joust_qa_pause_elapsed&&joust_landing_age==joust_qa_dust_pause_age&&joust_landings==joust_qa_dust_pause_landings&&joust_dust.stats().dust_bursts==joust_qa_dust_pause_bursts&&joust_qa_same_dust(joust_qa_dust_pause_stage,joust_qa_dust_stage()),"Pause advanced or repeated the actual landed rider dust envelope"))return;
        ++joust_qa_dust_pause_checks;joust_qa_pending="landing-dust-paused";joust_qa_next_stage(31,frame);return;
    case 31:
        if(!joust_qa_pending.empty()||age<3)return;
        joust_qa_pause();joust_qa_next_stage(32,frame);return;
    case 32:
        if(age<8)return;
        if(!joust_qa_check(!joust_paused&&g.elapsed>joust_qa_pause_elapsed&&joust_landing_age>joust_qa_dust_pause_age&&joust_landings==joust_qa_dust_pause_landings&&joust_dust.stats().dust_bursts==joust_qa_dust_pause_bursts&&!joust_qa_same_dust(joust_qa_dust_pause_stage,joust_qa_dust_stage()),"Resume failed to advance existing dust or repeated its landing event"))return;
        ++joust_qa_dust_pause_checks;joust_qa_pass("real-ground-edge-dust-and-audio-freeze-resume-without-retrigger");joust_qa_next_stage(12,frame);return;
    case 12:
        if(g.phase==world_joust::Phase::Charge){
            if(!joust_qa_timed_push&&!joust_qa_no_thrust_pass){
                if(!joust_qa_check(g.player_hit==world_joust::Hit::Miss&&g.thrust_serial==joust_qa_pass_serial,"A ready lance scored or started a push without a new trigger press"))return;
                if(g.distance_to_opponent()<=9.f){
                    if(!joust_qa_check(joust_controls_armed&&g.player.thrust_armed,"Timed physical contact was not preceded by neutral trigger rearming"))return;
                    joust_qa_push(true);joust_qa_timed_push=true;++joust_qa_timed_pushes;
                }
            }
            return;
        }
        if(g.phase!=world_joust::Phase::Result&&g.phase!=world_joust::Phase::MatchOver)return;
        if(joust_qa_recorded_pass!=g.pass_index){
            if(joust_qa_no_thrust_pass){
                if(!joust_qa_check(g.player_hit==world_joust::Hit::Miss&&g.thrust_serial==joust_qa_pass_serial,"No-RT pass scored a passive ready-lance hit"))return;
                joust_qa_pass("complete-no-RT-pass-does-not-score-passive-lance-contact");
            }else if(!joust_qa_check(joust_qa_timed_push&&g.thrust_serial==joust_qa_pass_serial+1&&g.player_hit!=world_joust::Hit::Miss&&g.player_contact.active_thrust,"Timed RT push did not create an actual active-stroke hit"))return;
            joust_qa_recorded_pass=g.pass_index;joust_qa_player_points+=int(g.player_hit);joust_qa_opponent_points+=int(g.opponent_hit);
            if(!joust_qa_check(g.player_score==joust_qa_player_points&&g.opponent_score==joust_qa_opponent_points,"Actual match totals differ from physical pass contacts"))return;
            std::ostringstream row;row<<"{\"pass\":"<<g.pass_index+1<<",\"player_hit\":"<<int(g.player_hit)<<",\"opponent_hit\":"<<int(g.opponent_hit)<<",\"player_score\":"<<g.player_score<<",\"opponent_score\":"<<g.opponent_score<<",\"contact_serial\":"<<g.contact_serial<<",\"player_thrust_serial\":"<<g.player_contact.thrust_serial<<",\"player_active_thrust\":"<<(g.player_contact.active_thrust?"true":"false")<<",\"player_contact_thrust_time\":"<<g.player_contact.thrust_time<<",\"timed_trigger_press\":"<<(joust_qa_timed_push?"true":"false")<<",\"deliberate_no_trigger_pass\":"<<(joust_qa_no_thrust_pass?"true":"false")<<",\"player_unhorsed\":"<<(g.player.unhorsed?"true":"false")<<",\"rival_unhorsed\":"<<(g.opponent.unhorsed?"true":"false")<<",\"player_grounded\":"<<(g.player.fall_grounded?"true":"false")<<",\"rival_grounded\":"<<(g.opponent.fall_grounded?"true":"false")<<"}";joust_qa_rows.push_back(row.str());
            joust_qa_pending="pass"+std::to_string(g.pass_index+1)+"-result";
        }
        joust_qa_next_stage(13,frame);return;
    case 13:
        if(!joust_qa_pending.empty()||age<5)return;
        if(g.pass_index+1>=g.setup.passes){
            if(g.phase!=world_joust::Phase::MatchOver)return;
            if(!joust_qa_check(joust_started==1&&joust_completed==1&&g.reward_coins==0&&joust_qa_rows.size()==std::size_t(g.setup.passes),"Actual full practice match did not complete once without payout"))return;
            if(!joust_qa_check(joust_renderer.frames()>100&&joust_renderer.draws()>100&&joust_renderer.shadow_draws()>0&&joust_renderer.failures()==0,"Actual mounted renderer did not draw valid color/shadow poses"))return;
            if(!joust_qa_check(joust_renderer.arm_updates()>100&&joust_renderer.arm_failures()==0&&joust_renderer.max_grip_error()<.002f&&joust_renderer.max_hand_surface_error()<.020f&&joust_renderer.max_hand_surface_drift()<.002f,"Actual source glove and right-arm pose did not stay bound to the moving lance"))return;
            if(!joust_qa_check(joust_qa_contacts>0&&joust_impacts>0&&training_sounds.events>joust_qa_audio_begin,"Actual mounted contact did not dispatch feedback/audio"))return;
            if(!joust_qa_check(joust_qa_active_contacts==joust_qa_contacts&&joust_qa_timed_pushes>0,"Active-stroke contact provenance was not checked"))return;
            if(!joust_qa_check(g.thrust_serial==2+joust_qa_timed_pushes&&joust_qa_timed_pushes==unsigned(joust_qa_controller?2:1),"Actual player thrust total differs from deliberate rising-edge presses"))return;
            if(!joust_qa_check(joust_qa_direction_rows.size()==std::size_t(joust_qa_controller?8:4),"Actual forward/reverse directional lance samples are incomplete"))return;
            if(!joust_qa_check(joust_qa_cross_source_checks==unsigned(joust_qa_controller?3:0),"Actual cross-source held-trigger checks are incomplete"))return;
            if(!joust_qa_check(joust_renderer.spectator_count()>0&&joust_renderer.crowd_draws()>100&&joust_renderer.crowd_uploads()>0&&joust_renderer.reaction_updates()>0,"Actual stadium spectators or rider reaction draws are missing"))return;
            for(float distance:joust_renderer.max_limb_excursion())if(!joust_qa_check(distance>.16f,"A hand or foot failed to bend visibly relative to the source skeleton"))return;
            if(!joust_qa_check(joust_renderer.fall_joint_count()==73&&joust_renderer.varied_fall_updates()>10&&joust_renderer.max_fall_secondary_angle()>.4f&&joust_renderer.max_fall_secondary_angle()<1.65f&&joust_renderer.max_fall_release_phase_span()>.05f&&joust_renderer.max_fall_release_phase_span()<=1.f&&joust_renderer.max_fall_segment_length_error()<.00001f,"Native fall lacked actual source rig, varied joint release or secondary response"))return;
            if(!joust_qa_check(joust_renderer.crowd_family_count()>=5&&joust_renderer.crowd_hair_family_count()>=4&&joust_renderer.crowd_style_count()>=5&&joust_renderer.seated_spectator_count()>=48&&joust_renderer.animated_spectator_count()>=240&&joust_renderer.crowd_pose_pairs()>=160,"Actual crowd lacks the authored body/hair/style families and seated cheering poses"))return;
            if(!joust_qa_check(joust_audience_audio.ready&&joust_audience_audio.updates>100&&joust_audience_audio.events>joust_qa_audience_begin,"Decoded audience bed and contact cheers were not dispatched"))return;
            if(!joust_qa_check(joust_qa_observed_landings>0&&joust_landings==joust_qa_observed_landings&&joust_dust.stats().dust_bursts==joust_qa_observed_landings&&joust_dust_peak>0&&joust_dust_draws>0&&joust_qa_landing_audio_checks>0&&joust_qa_dust_pause_checks==2,"Actual landing dust/audio lacked ground-edge provenance, native draws or pause/resume verification"))return;
            {
                const unsigned side=joust_qa_controller?0:1;
                if(!joust_qa_check(joust_qa_falling_frames[side]>0&&joust_qa_grounded_frames[side]>0&&joust_qa_fall_captured[side]&&joust_qa_ground_captured[side]&&joust_qa_empty_horse_captured[side]&&joust_qa_peak_rendered_unhorsed>0&&joust_qa_peak_rendered_grounded>0&&joust_renderer.riderless_horse_draws()>0&&joust_renderer.unhorsed_rider_draws()>0&&joust_renderer.min_fallen_vertex_y()>=.019f,"Real helmet impact did not produce drawn falling/grounded rider and riderless horse above the venue floor"))return;
                if(!joust_qa_check(joust_qa_ejection_captured[side]&&joust_qa_settled_captured[side]&&joust_qa_peak_fall_pose[side]>.95f&&joust_qa_peak_fall_settle[side]>.95f&&joust_qa_peak_ground_time[side]>=1.05f&&joust_renderer.articulated_fall_updates()>0&&joust_renderer.settled_fall_updates()>0,"Native fall did not articulate from ejection through an extended settled ground pose"))return;
                if(!joust_qa_check(joust_renderer.released_gear_draws()>0&&joust_renderer.min_released_gear_y()>=.019f,"Released lance or shield was missing or crossed the venue floor"))return;
                if(joust_qa_controller&&!joust_qa_check((joust_qa_third_person_route?joust_qa_third_person_fall:joust_qa_first_person_fall)&&joust_qa_received_hits[1]>0&&joust_qa_received_hits[2]>0&&joust_qa_received_hits[3]>0&&joust_qa_incoming_captured_serial>0&&joust_qa_peak_reaction>.1f,"Controller rehearsal did not show the selected unhorse view plus shield/torso/head reactions"))return;
            }
            joust_qa_pass("complete-physical-match-pass-totals-and-no-practice-payout");joust_qa_pass("actual-mounted-pose-color-shadow-and-feedback-dispatch");joust_qa_pass("timed-RT-push-scores-only-during-committed-contact-window");
            joust_qa_pass("helmet-hit-unhorses-real-rider-falls-grounds-and-leaves-empty-horse");joust_qa_pass("actual-stadium-spectators-and-decoded-audience-pause-resume-cheers");
            joust_qa_pass("actual-articulated-ejection-midfall-first-floor-and-settled-body");joust_qa_pass("source-derived-five-family-four-hair-seated-and-standing-cheering-crowd");
            if(joust_qa_controller)joust_qa_pass(joust_qa_third_person_route?"incoming-shield-torso-helmet-reactions-and-third-person-fall-view":"incoming-shield-torso-helmet-reactions-and-first-person-external-fall-view");
            joust_qa_pending="match-complete";joust_qa_next_stage(15,frame);return;
        }
        joust_qa_push(false);if(joust_qa_controller)joust_qa_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT,-32768);
        joust_qa_confirm();joust_qa_next_stage(14,frame);return;
    case 14:
        if(age<8)return;
        if(!joust_qa_check(g.phase==world_joust::Phase::Countdown&&joust_controls_armed&&g.player.thrust_armed,"SDL next pass did not reset horses and rearm neutral controls"))return;
        joust_qa_set_aim(0,.8f);joust_qa_no_thrust_pass=g.pass_index==2;joust_qa_timed_push=false;joust_qa_pass_serial=g.thrust_serial;
        if(g.pass_index==2&&!third_person)joust_qa_camera_toggle();
        joust_qa_pass("SDL-next-pass-and-distinct-right-stick-aims");
        if(g.pass_index==1){joust_qa_direction_index=0;joust_qa_direction_begin=g.elapsed;joust_qa_next_stage(4,frame);}else joust_qa_next_stage(12,frame);return;
    case 15:
        if(!joust_qa_pending.empty()||age<3)return;
        joust_qa_done=true;running=false;return;
    default:joust_qa_fail("Unknown private rehearsal stage");return;
    }
};
auto joust_qa_capture=[&](int width,int height){
    if(!joust_qa||!joust_qa_valid||joust_qa_failed||joust_qa_pending.empty())return;
    if(!joust_qa_check(width>=640&&height>=360&&width<=7680&&height<=4320,"Native capture dimensions are invalid"))return;
    std::vector<unsigned char> pixels(std::size_t(width)*height*3);GLint alignment=4;glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());glPixelStorei(GL_PACK_ALIGNMENT,alignment);
    const auto error=glGetError();if(error!=GL_NO_ERROR){++joust_qa_gl_errors;joust_qa_fail("Native mounted framebuffer has a GL error");return;}
    const std::size_t stride=std::size_t(width)*3;for(int y=0;y<height/2;++y)std::swap_ranges(pixels.data()+std::size_t(y)*stride,pixels.data()+std::size_t(y+1)*stride,pixels.data()+std::size_t(height-1-y)*stride);
    auto *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(),width,height,24,width*3,SDL_PIXELFORMAT_RGB24);auto path=joust_qa_dir/(joust_qa_pending+".bmp");if(std::filesystem::exists(path))path=joust_qa_dir/("contact"+std::to_string(joust_qa_captures+1)+"-"+joust_qa_pending+".bmp");const bool saved=surface&&SDL_SaveBMP(surface,path.u8string().c_str())==0;if(surface)SDL_FreeSurface(surface);
    if(!joust_qa_check(saved,"Could not save actual mounted framebuffer"))return;
    joust_qa_capture_paths.push_back(path.u8string());++joust_qa_captures;std::printf("[Joust QA] CAPTURE %s\n",path.u8string().c_str());std::fflush(stdout);joust_qa_pending.clear();
};
auto joust_qa_finish=[&](){
    if(!joust_qa)return;
    const bool passed=joust_qa_valid&&joust_qa_done&&!joust_qa_failed&&joust_qa_captures>=7&&joust_qa_gl_errors==0;
    if(joust_qa_valid){
        std::ofstream out(joust_qa_dir/"JOUST_NATIVE_RAW.json");out<<std::setprecision(12);
        out<<"{\"schema\":\"valwood.joust.native-sdl-rehearsal.v6\",\"passed\":"<<(passed?"true":"false")<<",\"error\":"<<world_json::quote(joust_qa_error)<<",\"mode\":"<<world_json::quote(joust_qa_third_person_route?"controller-third":joust_qa_controller?"controller":"keyboard")
           <<",\"live_state_injected\":false,\"contacts_injected\":false,\"actual_SDL_event_route\":true,\"owned_virtual_controller\":true,\"private_practice_only\":true,\"controller_feel_accepted\":false,\"source_triangle_anatomy_claim\":false,\"collision_proxy_sweep_tolerance\":0.03"
           <<",\"difficulty\":"<<joust_game.setup.difficulty<<",\"passes\":"<<joust_game.setup.passes<<",\"last_frame\":"<<joust_qa_last_frame<<",\"input_events\":"<<joust_qa_inputs<<",\"captures\":"<<joust_qa_captures<<",\"physical_contacts\":"<<joust_qa_contacts<<",\"verified_contact_proxy_sweeps\":"<<joust_qa_geometry_checks
           <<",\"maximum_grip_error_m\":"<<joust_qa_max_grip_error<<",\"player_score\":"<<joust_game.player_score<<",\"opponent_score\":"<<joust_game.opponent_score<<",\"match_starts\":"<<joust_started<<",\"match_completions\":"<<joust_completed<<",\"feedback_dispatches\":"<<joust_impacts<<",\"audio_dispatches\":"<<training_sounds.events-joust_qa_audio_begin
           <<",\"player_thrusts\":"<<joust_game.thrust_serial<<",\"timed_trigger_pushes\":"<<joust_qa_timed_pushes<<",\"active_thrust_contacts\":"<<joust_qa_active_contacts<<",\"maximum_forward_extension_m\":"<<joust_qa_peak_extension<<",\"maximum_thrust_amount\":"<<joust_qa_peak_amount<<",\"right_stick_directions\":"<<joust_qa_direction_rows.size()<<",\"cross_source_neutral_checks\":"<<joust_qa_cross_source_checks
           <<",\"received_hit_types\":["<<joust_qa_received_hits[0]<<","<<joust_qa_received_hits[1]<<","<<joust_qa_received_hits[2]<<","<<joust_qa_received_hits[3]<<"],\"falling_frames\":["<<joust_qa_falling_frames[0]<<","<<joust_qa_falling_frames[1]<<"],\"grounded_frames\":["<<joust_qa_grounded_frames[0]<<","<<joust_qa_grounded_frames[1]<<"],\"first_person_player_unhorse_view\":"<<(joust_qa_first_person_fall?"true":"false")<<",\"third_person_player_unhorse_view\":"<<(joust_qa_third_person_fall?"true":"false")<<",\"maximum_reaction_amount\":"<<joust_qa_peak_reaction<<",\"maximum_incoming_view_angle_delta\":"<<joust_qa_peak_incoming_view
           <<",\"audio_master_gain\":"<<audio_master_gain()<<",\"audio_probe_controls_only\":true,\"mixer_driver\":"<<world_json::quote(SDL_GetCurrentAudioDriver()?SDL_GetCurrentAudioDriver():"")
           <<",\"muted_postmix_samples\":"<<joust_qa_muted_pcm.samples<<",\"muted_postmix_rms\":"<<joust_qa_muted_pcm.rms()<<",\"crowd_postmix_samples\":"<<joust_qa_crowd_pcm.samples<<",\"crowd_postmix_nonzero\":"<<joust_qa_crowd_pcm.nonzero<<",\"crowd_postmix_rms\":"<<joust_qa_crowd_pcm.rms()<<",\"crowd_postmix_peak\":"<<joust_qa_crowd_pcm.peak<<",\"crowd_postmix_clipped\":"<<joust_qa_crowd_pcm.clipped
           <<",\"cheer_postmix_rms\":"<<joust_qa_cheer_pcm.rms()<<",\"cheer_postmix_nonzero\":"<<joust_qa_cheer_pcm.nonzero<<",\"audience_ready\":"<<(joust_audience_audio.ready?"true":"false")<<",\"audience_events\":"<<joust_audience_audio.events-joust_qa_audience_begin<<",\"audience_updates\":"<<joust_audience_audio.updates<<",\"spectators\":"<<joust_renderer.spectator_count()<<",\"crowd_draws\":"<<joust_renderer.crowd_draws()<<",\"crowd_uploads\":"<<joust_renderer.crowd_uploads()<<",\"maximum_crowd_swell\":"<<joust_qa_peak_crowd_swell
           <<",\"crowd_families\":"<<joust_renderer.crowd_family_count()<<",\"crowd_hair_families\":"<<joust_renderer.crowd_hair_family_count()<<",\"crowd_styles\":"<<joust_renderer.crowd_style_count()<<",\"seated_spectators\":"<<joust_renderer.seated_spectator_count()<<",\"animated_spectators\":"<<joust_renderer.animated_spectator_count()<<",\"crowd_pose_pairs\":"<<joust_renderer.crowd_pose_pairs()
           <<",\"observed_ground_edges\":"<<joust_qa_observed_landings<<",\"landing_events\":"<<joust_landings<<",\"landing_audio_edge_checks\":"<<joust_qa_landing_audio_checks<<",\"landing_dust_bursts\":"<<joust_dust.stats().dust_bursts<<",\"peak_landing_dust_particles\":"<<joust_dust_peak<<",\"landing_dust_draws\":"<<joust_dust_draws<<",\"landing_dust_pause_checks\":"<<joust_qa_dust_pause_checks
           <<",\"peak_fall_pose_progress\":["<<joust_qa_peak_fall_pose[0]<<","<<joust_qa_peak_fall_pose[1]<<"],\"peak_fall_settle_progress\":["<<joust_qa_peak_fall_settle[0]<<","<<joust_qa_peak_fall_settle[1]<<"],\"peak_fall_ground_time\":["<<joust_qa_peak_ground_time[0]<<","<<joust_qa_peak_ground_time[1]<<"],\"articulated_fall_updates\":"<<joust_renderer.articulated_fall_updates()<<",\"settled_fall_updates\":"<<joust_renderer.settled_fall_updates()
           <<",\"independent_limb_excursion_m\":["<<joust_renderer.max_limb_excursion()[0]<<","<<joust_renderer.max_limb_excursion()[1]<<","<<joust_renderer.max_limb_excursion()[2]<<","<<joust_renderer.max_limb_excursion()[3]<<"],\"fall_joint_count\":"<<joust_renderer.fall_joint_count()<<",\"varied_fall_updates\":"<<joust_renderer.varied_fall_updates()<<",\"maximum_fall_secondary_angle_rad\":"<<joust_renderer.max_fall_secondary_angle()<<",\"maximum_fall_release_phase_span\":"<<joust_renderer.max_fall_release_phase_span()<<",\"maximum_fall_segment_length_error_m\":"<<joust_renderer.max_fall_segment_length_error()<<",\"released_gear_draws\":"<<joust_renderer.released_gear_draws()<<",\"minimum_released_gear_support_y_m\":"<<joust_renderer.min_released_gear_y()
           <<",\"rendered_unhorsed_riders\":"<<joust_renderer.rendered_unhorsed_riders()<<",\"rendered_grounded_riders\":"<<joust_renderer.rendered_grounded_riders()<<",\"peak_rendered_unhorsed_riders\":"<<joust_qa_peak_rendered_unhorsed<<",\"peak_rendered_grounded_riders\":"<<joust_qa_peak_rendered_grounded<<",\"riderless_horse_draws\":"<<joust_renderer.riderless_horse_draws()<<",\"unhorsed_rider_draws\":"<<joust_renderer.unhorsed_rider_draws()<<",\"minimum_fallen_vertex_y_m\":"<<joust_renderer.min_fallen_vertex_y()<<",\"maximum_rider_floor_correction_m\":"<<joust_renderer.max_rider_floor_correction()<<",\"renderer_reaction_updates\":"<<joust_renderer.reaction_updates()
           <<",\"renderer_ready\":"<<(joust_renderer.ready?"true":"false")<<",\"renderer_pose_updates\":"<<joust_renderer.frames()<<",\"renderer_color_draws\":"<<joust_renderer.draws()<<",\"renderer_shadow_draws\":"<<joust_renderer.shadow_draws()<<",\"renderer_failures\":"<<joust_renderer.failures()<<",\"renderer_arm_updates\":"<<joust_renderer.arm_updates()<<",\"renderer_arm_failures\":"<<joust_renderer.arm_failures()<<",\"maximum_rendered_grip_error_m\":"<<joust_renderer.max_grip_error()<<",\"maximum_rendered_hand_surface_error_m\":"<<joust_renderer.max_hand_surface_error()<<",\"maximum_rendered_hand_surface_drift_m\":"<<joust_renderer.max_hand_surface_drift()<<",\"GL_errors\":"<<joust_qa_gl_errors<<",\"checks\":[";
        for(unsigned i=0;i<joust_qa_checks.size();++i){out<<(i?",":"")<<world_json::quote(joust_qa_checks[i]);}
        out<<"],\"passes_reviewed\":[";for(unsigned i=0;i<joust_qa_rows.size();++i){out<<(i?",":"")<<joust_qa_rows[i];}
        out<<"],\"directions_reviewed\":[";for(unsigned i=0;i<joust_qa_direction_rows.size();++i){out<<(i?",":"")<<joust_qa_direction_rows[i];}
        out<<"],\"fall_stages_reviewed\":[";for(unsigned i=0;i<joust_qa_fall_rows.size();++i){out<<(i?",":"")<<joust_qa_fall_rows[i];}
        out<<"],\"capture_paths\":[";for(unsigned i=0;i<joust_qa_capture_paths.size();++i){out<<(i?",":"")<<world_json::quote(joust_qa_capture_paths[i]);}
        out<<"]}\n";
        if(!out){launch->exit_code=1;std::fprintf(stderr,"[Joust QA] Could not write native result\n");}
    }
    if(!passed)launch->exit_code=1;
    std::fprintf(stderr,"[Joust QA] %s mode=%s passes=%zu captures=%u contacts=%u: %s\n",passed?"PASS":"FAIL",joust_qa_third_person_route?"controller-third":joust_qa_controller?"controller":"keyboard",joust_qa_rows.size(),joust_qa_captures,joust_qa_contacts,joust_qa_error.c_str());
};

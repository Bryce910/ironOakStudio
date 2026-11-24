// Practice setup, independent purse, and real shared-player arena combat.
enum class ArenaUiPhase {Setup,Fight,Results};
ArenaUiPhase arena_ui_phase=ArenaUiPhase::Setup;
world_arena_progress::Progress arena_progress;
world_arena_progress::Config arena_run_config={};
world_arena_progress::NeutralGate arena_menu_gate,arena_gameplay_gate;
std::uint64_t arena_run_id=0,arena_saved_reward=0;
int arena_total_draft=8,arena_simultaneous_draft=1,arena_focus_panel=-1;
bool arena_progress_ready=false,arena_reward_pending=false,arena_gameplay_ready=false,arena_was_modal=true;
std::string arena_progress_error;
std::array<WorldCombatArenaActor,8> arena_motion_actors={};
bool arena_mouse_draw=false,arena_mouse_aim=false,arena_bow_held_before=false,arena_bow_armed=false;
unsigned arena_previous_shots=0,arena_enemy_steps=0;
world_arena_combat::Vec3 arena_last_enemy={};
Camera arena_aim_camera=cam;
WorldArenaTargetLock arena_target_lock;
unsigned arena_lock_toggles=0;
std::uint64_t arena_camera_frames=0,arena_camera_many_frames=0,arena_camera_locked_frames=0;
double arena_camera_heading_error=0,arena_camera_lateral_error=0,arena_camera_pitch_error=0;
double arena_camera_max_heading_error=0,arena_camera_max_lateral_error=0,arena_camera_max_pitch_error=0;
std::function<void()> arena_shot_sound;
auto arena_bow=[&](){return arena_enabled&&arena_game.weapon()==world_arena_combat::Weapon::Bow;};
auto arena_finished=[&](){return arena_game.state()==world_arena_combat::State::Won||arena_game.state()==world_arena_combat::State::Lost;};
auto arena_raw_held=[&](){
    const auto *keys=SDL_GetKeyboardState(nullptr);
    for(auto key:{SDL_SCANCODE_RETURN,SDL_SCANCODE_KP_ENTER,SDL_SCANCODE_SPACE,SDL_SCANCODE_E,SDL_SCANCODE_R,
                  SDL_SCANCODE_ESCAPE,SDL_SCANCODE_W,SDL_SCANCODE_A,SDL_SCANCODE_S,SDL_SCANCODE_D,
                  SDL_SCANCODE_LEFT,SDL_SCANCODE_RIGHT,SDL_SCANCODE_UP,SDL_SCANCODE_DOWN,SDL_SCANCODE_T,
                  SDL_SCANCODE_1,SDL_SCANCODE_2,SDL_SCANCODE_V,SDL_SCANCODE_J,SDL_SCANCODE_TAB,SDL_SCANCODE_Q,
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
    arena_menu_gate.reset();arena_gameplay_gate.reset();arena_gameplay_ready=false;
    arena_game.cancel_draw();fight.clear_buffer();fight.set_guard(false);sword_stick.reset();
    arena_mouse_draw=arena_mouse_aim=arena_bow_held_before=arena_bow_armed=false;
    fight_mouse_x=fight_mouse_y=0;preview_release_mouse();
};
auto arena_modal_neutral=[&](){arena_release();};
auto arena_controls_ready=[&](){return !arena_enabled||(arena_ui_phase==ArenaUiPhase::Fight&&preview_screen==1&&!arena_finished()&&arena_gameplay_ready);};
auto arena_look_held=[&](){return arena_enabled&&!arena_bow()&&arena_controls_ready()&&
    (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_Q]||(preview_pad.get()&&SDL_GameControllerGetButton(preview_pad.get(),SDL_CONTROLLER_BUTTON_LEFTSHOULDER)));};
auto arena_refresh_input=[&](){
    if(!arena_enabled)return;
    const bool focused=preview_pad.focused&&(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS);
    const bool modal=arena_ui_phase!=ArenaUiPhase::Fight||preview_screen!=1||show_graphics||destination_loading;
    if(modal!=arena_was_modal||!focused){arena_release();arena_was_modal=modal;}
    arena_gameplay_ready=!modal&&arena_gameplay_gate.observe(focused,arena_raw_held());
};
auto arena_load_progress=[&](){
    world_arena_progress::Progress staged;
    const auto loaded=world_arena_progress::load("saves/arena.progress",staged,arena_progress_error);
    arena_progress_ready=loaded==world_arena_progress::Load::Missing||loaded==world_arena_progress::Load::Loaded;
    if(arena_progress_ready){arena_progress=staged;arena_total_draft=int(staged.selection.total);arena_simultaneous_draft=int(staged.selection.simultaneous);}
    return arena_progress_ready;
};
auto arena_save_reward=[&](){
    if(!arena_reward_pending||!arena_progress_ready)return !arena_reward_pending;
    world_arena_progress::Progress staged;std::uint64_t award=0;
    const auto result=world_arena_progress::settle(arena_progress,arena_run_id,arena_run_config,
        arena_game.state()==world_arena_combat::State::Won&&arena_game.defeated()==arena_run_config.total,
        staged,award,arena_progress_error);
    if(result==world_arena_progress::Settlement::Rejected)return false;
    if(result==world_arena_progress::Settlement::Ready&&!world_arena_progress::save("saves/arena.progress",staged,arena_progress_error))return false;
    arena_progress=staged;arena_saved_reward=world_arena_progress::payout(arena_run_config);arena_reward_pending=false;
    playtest_tell(std::to_string(arena_saved_reward)+" arena coins saved.");return true;
};
auto arena_show_setup=[&](){
    // Leaving a live bout is a loss: allocating the next run consumes a new
    // receipt ID, and no transient combat result can change the saved purse.
    if(arena_reward_pending)return;
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
    arena_game.set_weapon(weapon);if(weapon==world_arena_combat::Weapon::Bow)arena_target_lock.clear();fight.clear_buffer();fight.set_guard(false);sword_stick.reset();arena_mouse_draw=arena_mouse_aim=false;
    arena_bow_armed=false;arena_bow_held_before=false;
    playtest_tell(weapon==world_arena_combat::Weapon::Sword?"Sword equipped. Right-stick flicks swing; LT + stick blocks.":"Bow equipped. Hold and release RT / left mouse to shoot. Right stick aims.");
};
auto arena_toggle_lock=[&](){
    if(!arena_controls_ready()||arena_bow())return;
    const bool was_locked=arena_target_lock.locked();
    const bool locked=arena_target_lock.toggle(arena_game,{cam.x-arena_origin_x,cam.y-1.75-arena_floor,cam.z-arena_origin_z},cam.yaw);
    ++arena_lock_toggles;
    playtest_tell(locked?"Locked onto opponent "+std::to_string(arena_target_lock.target()+1)+". RB / middle mouse releases.":was_locked?"Target released. Normal player camera.":"Face an approaching opponent to lock on.");
};
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
        if(actor.state==world_arena_combat::ChallengerState::Active)surface(world_arena_combat::swept_capsule(eye,eye+line,actor.position,.34,1.8,0));
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
    const world_arena_progress::Config config{unsigned(arena_total_draft),unsigned(arena_simultaneous_draft)};
    world_arena_progress::Progress staged;std::uint64_t id=0;
    if(!world_arena_progress::allocate(arena_progress,config,staged,id,arena_progress_error)||
       !world_arena_progress::save("saves/arena.progress",staged,arena_progress_error))return false;
    arena_progress=staged;arena_run_id=id;arena_run_config=config;arena_saved_reward=0;
    if(!arena_game.start(fight,{config.total,config.simultaneous},preview_settings.story_assist)){
        arena_progress_error="These opponent settings could not start a fight. No coins were awarded.";return false;
    }
    arena_ui_phase=ArenaUiPhase::Fight;arena_target_lock.clear();arena_focus_panel=-1;arena_weapon(world_arena_combat::Weapon::Sword);
    cam.x=arena_origin_x;cam.y=arena_floor+1.75;cam.z=arena_origin_z-5;cam.yaw=3.14159265359f;cam.pitch=-.04f;
    cam.fly=false;cam.grounded=true;cam.vy=0;third_person=true;avatar_initialized=false;preview_screen=1;
    arena_mouse_draw=arena_mouse_aim=arena_bow_held_before=false;arena_bow_armed=false;
    arena_previous_shots=0;arena_enemy_steps=0;arena_last_enemy=arena_game.active_position();
    combat_contact_kind=combat_vfx_arrow_hits=0;combat_contact_time=combat_contact_damage=0;
    blood_effects.reset();combat_vfx.clear();
    arena_release();arena_sync();temporal_cut=true;++playtest_combat_starts;
    playtest_tell(std::to_string(config.total)+" opponents, "+std::to_string(config.simultaneous)+" at once. Victory earns "+std::to_string(world_arena_progress::payout(config))+" coins.");return true;
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
    return false;
};
auto arena_controls=[&](bool allowed){
    if(!arena_enabled)return;
    const bool held=arena_mouse_draw||(preview_pad.using_pad&&preview_pad.right_trigger>.4f);
    if(!allowed||arena_finished()||!arena_bow()){
        arena_game.cancel_draw();arena_bow_armed=false;arena_bow_held_before=held;return;
    }
    if(!held)arena_bow_armed=true;
    if(held&&!arena_bow_held_before&&arena_bow_armed)arena_game.begin_draw(fight);
    if(!held&&arena_bow_held_before&&arena_bow_armed)arena_release_bow();
    arena_bow_held_before=held;fight.set_guard(false);
};
auto arena_tick=[&](float delta,bool paused){
    if(!arena_enabled)return;
    const float model_yaw=3.141592653589793f-cam.yaw;
    if(arena_ui_phase==ArenaUiPhase::Fight)arena_game.update(delta,fight,{cam.x-arena_origin_x,cam.y-1.75-arena_floor,cam.z-arena_origin_z},model_yaw,paused||!arena_controls_ready(),arena_target_lock.target());
    else if(arena_ui_phase==ArenaUiPhase::Results&&arena_game.state()==world_arena_combat::State::Won)
        arena_game.update(delta,fight,{cam.x-arena_origin_x,cam.y-1.75-arena_floor,cam.z-arena_origin_z},model_yaw,!preview_pad.focused||show_graphics||destination_loading);
    arena_target_lock.maintain(arena_game,{cam.x-arena_origin_x,cam.y-1.75-arena_floor,cam.z-arena_origin_z});
    if(arena_ui_phase==ArenaUiPhase::Fight&&arena_finished()){
        arena_ui_phase=ArenaUiPhase::Results;preview_screen=2;arena_focus_panel=-1;arena_release();
        arena_reward_pending=arena_game.state()==world_arena_combat::State::Won;
        if(arena_reward_pending)arena_save_reward();
    }
    arena_sync();
    const auto enemy=arena_game.active_position();
    const bool moving=std::hypot(enemy.x-arena_last_enemy.x,enemy.z-arena_last_enemy.z)>.0001;
    if(moving)++arena_enemy_steps;arena_last_enemy=enemy;
    std::array<WorldCombatCrowdInstance,8> crowd={};unsigned count=0;
    for(unsigned id=0;id<arena_game.challengers().size();++id){
        const auto &actor=arena_game.challengers()[id];auto &motion=arena_motion_actors[id];motion={};
        const bool active=actor.state==world_arena_combat::ChallengerState::Active||actor.state==world_arena_combat::ChallengerState::Entering;
        motion.combat=arena_game.view(fight,id);motion.x=arena_origin_x+actor.position.x;motion.y=arena_floor;motion.z=arena_origin_z+actor.position.z;
        motion.visible=active&&arena_ui_phase!=ArenaUiPhase::Setup;motion.walking=actor.moving&&!paused&&arena_controls_ready();
        if(active||actor.state==world_arena_combat::ChallengerState::Unused)continue;
        auto &v=crowd[count++];v.x=arena_origin_x+actor.position.x;v.y=arena_floor;v.z=arena_origin_z+actor.position.z;v.yielded=actor.state==world_arena_combat::ChallengerState::Defeated;
        v.yaw=v.yielded&&character_polish_enabled?actor.defeat_yaw:3.14159265359f;
        v.defeat_phase=v.yielded&&character_polish_enabled?float(std::clamp(actor.defeat_age/world_combat_motion::death_duration,0.,1.)):0;
    }
    world_combat_motion_crowd(combat_motion,crowd.data(),count);
};
auto arena_prepare_render=[&](const Camera &eye){
    if(!arena_enabled||!arena_renderer)return;
    arena_aim_camera=eye;
    arena_camera_heading_error=std::abs(std::remainder(double(eye.yaw)-cam.yaw,6.283185307179586));
    arena_camera_pitch_error=std::abs(double(eye.pitch)-cam.pitch);
    arena_camera_lateral_error=std::abs((cam.x-eye.x)*std::cos(double(eye.yaw))+(cam.z-eye.z)*std::sin(double(eye.yaw)));
    if(third_person&&!cam.fly){
        ++arena_camera_frames;arena_camera_many_frames+=arena_game.active_count()>1;arena_camera_locked_frames+=arena_target_lock.locked();
        arena_camera_max_heading_error=std::max(arena_camera_max_heading_error,arena_camera_heading_error);
        arena_camera_max_pitch_error=std::max(arena_camera_max_pitch_error,arena_camera_pitch_error);
        arena_camera_max_lateral_error=std::max(arena_camera_max_lateral_error,arena_camera_lateral_error);
    }
    WorldArenaBowPose bow;bow.visible=arena_bow();bow.first_person=!third_person;bow.x=third_person?cam.x:eye.x;bow.y=third_person?cam.y-1.75:eye.y;bow.z=third_person?cam.z:eye.z;bow.yaw=cam.yaw;bow.pitch=cam.pitch;bow.draw=float(arena_game.draw_fraction());
    std::array<WorldArenaRenderArrow,24> arrows={};unsigned count=0;
    for(const auto &arrow:arena_game.arrows())if(arrow.state!=world_arena_combat::ArrowState::Unused){
        auto &v=arrows[count++];v.visible=true;const double speed=std::max(.01,std::sqrt(world_arena_combat::dot(arrow.velocity,arrow.velocity)));
        v.tip[0]=arena_origin_x+arrow.position.x;v.tip[1]=arena_floor+arrow.position.y;v.tip[2]=arena_origin_z+arrow.position.z;
        v.base[0]=v.tip[0]-arrow.velocity.x/speed*.65;v.base[1]=v.tip[1]-arrow.velocity.y/speed*.65;v.base[2]=v.tip[2]-arrow.velocity.z/speed*.65;
    }
    if(!world_arena_render_update(arena_renderer,bow,arrows.data(),count)){launch->exit_code=1;running=false;}
};
arena_draw=[&](const Camera &eye,bool loading){
    if(loading||show_graphics)return;
    const auto &io=ImGui::GetIO();const float w=io.DisplaySize.x,h=io.DisplaySize.y,u=std::clamp(std::min(w/1600.f,h/900.f),.65f,1.4f);
    auto *dl=ImGui::GetForegroundDrawList();const ImU32 gold=IM_COL32(231,195,127,255),white=IM_COL32(242,239,226,255);
    auto text=[&](float x,float y,float size,const std::string &s,ImU32 color){dl->AddText(ImGui::GetFont(),size*u,{x*u,y*u},color,s.c_str());};
    dl->AddRectFilled({22*u,20*u},{590*u,148*u},IM_COL32(9,18,21,220),6*u);
    text(40,32,13,"VALWOOD  /  COMBAT PRACTICE",gold);
    text(40,56,23,"ARENA CHALLENGE",white);
    text(40,91,15,"Defeated "+std::to_string(arena_game.defeated())+" / "+std::to_string(arena_game.config().total)+"   |   Active "+std::to_string(arena_game.active_count())+"   |   "+(arena_bow()?"2  BOW":"1  SWORD"),gold);
    std::string state="Next challenger approaching";
    if(arena_ui_phase==ArenaUiPhase::Setup)state="Choose the challenge before entering the ring.";
    if(arena_game.state()==world_arena_combat::State::Fighting)state=fight.state==TrainingState::Countdown?"Ready in "+std::to_string(int(std::ceil(fight.phase_time))):std::string("Fight  |  Focus guard: ")+world_training_direction_name(fight.enemy_guard);
    if(arena_game.state()==world_arena_combat::State::Intermission)state=character_polish_enabled?"Opponent defeated. Next challenger entering shortly.":"Opponent yielded. Next challenger entering shortly.";
    if(arena_game.state()==world_arena_combat::State::Won)state="All "+std::to_string(arena_run_config.total)+" opponents defeated.";
    if(arena_game.state()==world_arena_combat::State::Lost)state="You yielded. No coins awarded.";
    text(40,119,13,state,white);
    if(arena_ui_phase==ArenaUiPhase::Fight&&!arena_bow()){
        text(40,166,14,arena_target_lock.locked()?"LOCKED: OPPONENT "+std::to_string(arena_target_lock.target()+1)+"  |  RB / middle mouse: release":"PLAYER CAMERA  |  RB / middle mouse: lock target",gold);
        if(arena_target_lock.locked()){
            const auto &actor=arena_game.challengers()[arena_target_lock.target()];
            const double dx=arena_origin_x+actor.position.x-eye.x,dy=arena_floor+actor.position.y+2.1-eye.y,dz=arena_origin_z+actor.position.z-eye.z;
            const auto f=eye.forward(),r=eye.right(),up=norm(cross(r,f));
            const double depth=dx*f.x+dy*f.y+dz*f.z,lens=std::tan(eye.fov*.5);
            if(depth>.1&&lens>.01){
                const float x=float((.5+(dx*r.x+dy*r.y+dz*r.z)/(2*depth*lens*eye.aspect))*w);
                const float y=float((.5-(dx*up.x+dy*up.y+dz*up.z)/(2*depth*lens))*h);
                if(x>0&&x<w&&y>0&&y<h){dl->AddCircle({x,y},10*u,gold,20,2*u);dl->AddCircleFilled({x,y},2*u,gold);}
            }
        }
    }
    const float bottom=h/u;
    dl->AddRectFilled({22*u,h-153*u},{425*u,h-55*u},IM_COL32(9,18,21,220),6*u);
    text(40,bottom-140,14,"HEALTH "+std::to_string(int(fight.health))+"   STAMINA "+std::to_string(int(fight.stamina)),white);
    auto bar=[&](float y,float value,ImU32 color){dl->AddRectFilled({40*u,h-y*u},{398*u,h-(y-8)*u},IM_COL32(35,42,43,255));dl->AddRectFilled({40*u,h-y*u},{(40+358*std::clamp(value,0.f,1.f))*u,h-(y-8)*u},color);};
    bar(113,fight.health/100,IM_COL32(194,74,69,255));bar(98,fight.stamina/100,IM_COL32(99,177,126,255));
    text(40,bottom-79,12,"COMBO "+std::to_string(fight.combo_count)+" / 3  |  Each swing uses stamina",gold);
    unsigned visible_opponents=0;
    for(unsigned id=0;id<arena_game.config().total;++id){
        const auto &actor=arena_game.challengers()[id];if(actor.state!=world_arena_combat::ChallengerState::Active&&actor.state!=world_arena_combat::ChallengerState::Entering)continue;
        const auto *opponent=arena_game.opponent(id);if(!opponent)continue;
        const float right=w/u-290,y=26+visible_opponents++*46;
        dl->AddRectFilled({(right-10)*u,(y-5)*u},{w-22*u,(y+36)*u},IM_COL32(9,18,21,220),4*u);
        const bool incoming=actor.state==world_arena_combat::ChallengerState::Active&&opponent->phase==TrainingEnemyPhase::Windup;
        std::string label="Opponent "+std::to_string(id+1)+(actor.state==world_arena_combat::ChallengerState::Entering?"  entering":incoming?std::string("  ")+world_training_direction_name(opponent->direction):std::string("  guard ")+world_training_direction_name(opponent->guard));
        if(arena_target_lock.locked()&&arena_target_lock.target()==id)label="LOCKED  "+label;
        text(right,y,12,label,incoming||arena_target_lock.target()==id?gold:white);
        dl->AddRectFilled({right*u,(y+22)*u},{(right+240)*u,(y+28)*u},IM_COL32(35,42,43,255));
        dl->AddRectFilled({right*u,(y+22)*u},{(right+240*std::clamp(opponent->health/std::max(1.f,opponent->max_health),0.f,1.f))*u,(y+28)*u},gold);
    }
    if(arena_bow()){
        const float cx=w*.5f,cy=h*.5f,r=(10-6*float(arena_game.draw_fraction()))*u;
        dl->AddLine({cx-r-5*u,cy},{cx-r,cy},white,1.5f*u);dl->AddLine({cx+r,cy},{cx+r+5*u,cy},white,1.5f*u);dl->AddLine({cx,cy-r-5*u},{cx,cy-r},white,1.5f*u);dl->AddCircle({cx,cy},2*u,gold);
        text(w/u*.5f-100,bottom-115,14,"DRAW "+std::to_string(int(arena_game.draw_fraction()*100))+"%  /  HITS "+std::to_string(arena_game.arrow_hits()),gold);
    }else if(fight.guard)text(w/u*.5f-180,bottom-115,16,std::string("YOUR BLOCK: ")+world_training_direction_name(fight.selected),gold);
    if(combat_contact_time>0&&preview_screen==1){
        const bool body=combat_contact_kind==1||combat_contact_kind==2;
        const ImU32 color=body?IM_COL32(255,104,88,255):IM_COL32(255,232,171,255);
        const float cx=w*.5f,cy=h*.5f,r=9*u,length=6*u;
        for(float sx:{-1.f,1.f})for(float sy:{-1.f,1.f})dl->AddLine({cx+sx*r,cy+sy*r},{cx+sx*(r+length),cy+sy*(r+length)},color,2*u);
        const std::string label=combat_contact_kind==1?"BODY HIT  -"+std::to_string(int(std::lround(combat_contact_damage))):combat_contact_kind==2?"YOU WERE HIT  -"+std::to_string(int(std::lround(combat_contact_damage))):combat_contact_kind==4?"PARRY  /  STEEL":"STEEL BLOCK";
        text(w/u*.5f-90,h/u*.5f+30,16,label,color);
    }
    if(fight.enemy_phase==TrainingEnemyPhase::Windup&&fight.state==TrainingState::Fighting)text(w/u*.5f-180,bottom-146,17,std::string("INCOMING: ")+world_training_direction_name(fight.enemy_direction),gold);
    if(fight.feedback_time>0)text(w/u*.5f-180,bottom-87,13,fight.feedback,white);
    text(30,bottom-35,13,arena_bow()?"1 Sword / 2 Bow  |  D-pad: weapons  |  Hold/release RT or LMB: shoot  |  LT/RMB: aim  |  Esc / Start: pause":"1 Sword / 2 Bow  |  RS: swing  |  LT + RS: block  |  Hold LB / Q: look  |  B: dodge  |  RS click / V: camera",white);
    if(preview_screen!=1||arena_ui_phase!=ArenaUiPhase::Fight){
        dl->AddRectFilled({0,0},{w,h},IM_COL32(4,10,13,145));
        const int panel=int(arena_ui_phase)*10+(show_graphics?1:0);
        const bool ready=arena_menu_gate.observe(preview_pad.focused&&(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS),arena_raw_held());
        const bool focus=arena_focus_panel!=panel;
        ImGui::SetNextWindowPos({w*.5f,h*.5f},ImGuiCond_Always,{.5f,.5f});ImGui::SetNextWindowSize({520*u,0},ImGuiCond_Always);
        if(focus)ImGui::SetNextWindowFocus();
        ImGui::Begin("Arena practice###arena-practice-menu",nullptr,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings);
        bool default_focus=focus&&ready;if(ready)arena_focus_panel=panel;
        auto button=[&](const char *label){const bool pressed=ImGui::Button(label,{-1,35*u});world_controller::default_focus(default_focus);return pressed;};
        ImGui::TextUnformatted(arena_ui_phase==ArenaUiPhase::Setup?"CHOOSE YOUR ARENA CHALLENGE":arena_ui_phase==ArenaUiPhase::Results?(arena_game.state()==world_arena_combat::State::Won?"VICTORY":"YOU YIELD"):"PAUSED");
        if(arena_progress_ready)ImGui::Text("Arena purse: %llu coins",(unsigned long long)arena_progress.balance);
        else ImGui::TextUnformatted("Arena purse unavailable");
        ImGui::BeginDisabled(!ready);
        if(!arena_progress_ready){
            ImGui::TextWrapped("Your existing purse could not be loaded. It has not been overwritten.");
            if(button("Retry loading arena purse")){arena_load_progress();arena_focus_panel=-1;arena_release();}
        }else if(arena_ui_phase==ArenaUiPhase::Setup){
            const auto total=unsigned(arena_total_draft);
            ImGui::Text("%u opponents  |  %d at once  |  Victory: +%u coins",total,arena_simultaneous_draft,10*total);
            if(button("Start fight###arena-start"))arena_begin();
            if(ImGui::SliderInt("Total opponents###arena-total",&arena_total_draft,1,8,"%d",ImGuiSliderFlags_AlwaysClamp))arena_simultaneous_draft=std::min(arena_simultaneous_draft,arena_total_draft);
            ImGui::SliderInt("At the same time###arena-simultaneous",&arena_simultaneous_draft,1,arena_total_draft,"%d",ImGuiSliderFlags_AlwaysClamp);
            ImGui::TextWrapped("Change counts: tap A, press left / right, then tap A again. Keyboard: Enter, type a number, Enter. Mouse: drag the slider.");
            ImGui::TextWrapped("Defeat every selected opponent to earn 10 coins each. A loss or an ended run earns no coins.");
        }else if(arena_ui_phase==ArenaUiPhase::Results){
            ImGui::Text("Defeated %u / %u  |  Arrow hits %u",arena_game.defeated(),arena_run_config.total,arena_game.arrow_hits());
            if(arena_reward_pending){
                ImGui::TextWrapped("Victory earned %llu coins. Saving failed; your saved purse has not changed.",(unsigned long long)world_arena_progress::payout(arena_run_config));
                if(button("Retry saving victory coins"))arena_save_reward();
            }else{
                if(arena_saved_reward)ImGui::Text("+%llu coins saved",(unsigned long long)arena_saved_reward);
                else ImGui::TextUnformatted("No reward for this run. Try again when ready.");
                if(button("Fight again###arena-repeat"))arena_begin();
                if(button("Change opponents###arena-change"))arena_show_setup();
            }
        }else{
            if(button("Resume###arena-resume")){preview_screen=1;arena_focus_panel=-1;arena_release();}
            if(button("End run and change opponents"))arena_show_setup();
        }
        if(button("Graphics settings")){show_graphics=true;graphics_draft=graphics;arena_focus_panel=-1;arena_release();}
        if(button("Exit arena"))running=false;
        ImGui::EndDisabled();
        if(!ready)ImGui::TextUnformatted("Release the controls to continue.");
        if(!arena_progress_error.empty())ImGui::TextWrapped("%s",arena_progress_error.c_str());
        ImGui::Separator();
        ImGui::TextWrapped("Left stick / D-pad or arrows: navigate. A / Enter or mouse: select. Sword combo: flick, center, then flick during recovery; up to 3 cuts (48 stamina). LT + stick blocks and cancels a queued cut. Hold LB / Q to look; center before swinging. Bow: hold / release RT. 1 / 2 or D-pad left / right: weapons. B / Space: dodge. V / right-stick click: camera.");
        ImGui::End();
    }
};
if(arena_enabled){
    // Initial placement was already loaded by the private playtest route.
    arena_game.bounds.half_extent=11.8;
    arena_load_progress();arena_game.cancel(fight);arena_sync();arena_last_enemy=arena_game.active_position();
    preview_screen=2;arena_release();
    std::printf("[Arena] Setup ready: %d opponents / %d concurrent, purse %llu; 24m floor %.2f %.2f %.2f\n",arena_total_draft,arena_simultaneous_draft,(unsigned long long)arena_progress.balance,arena_origin_x,arena_floor,arena_origin_z);
}

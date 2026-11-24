// Opt-in environment and sword-combat playtests. Data/saves belong to a separate project.
unsigned playtest_section=0,playtest_completed_sections=0,playtest_restarts=0,playtest_save_load_checks=0;
bool playtest_qa_done=false;
double playtest_qa_distance=0;
uint64_t playtest_qa_blocks=0,playtest_qa_slopes=0,playtest_qa_frames=0;
size_t playtest_qa_node=0;unsigned playtest_qa_hold=0,playtest_stuck=0;
std::string playtest_notice;float playtest_notice_seconds=0;
int playtest_last_screen=-1;
bool playtest_combat_ready=false;
double playtest_combat_start_x=0,playtest_combat_start_z=0,playtest_combat_enemy_x=0,playtest_combat_enemy_z=0;
float playtest_combat_feet=0;
unsigned playtest_combat_starts=0,playtest_combat_wins=0,playtest_combat_defeats=0;
TrainingState playtest_combat_previous=TrainingState::Idle,playtest_combat_result_screen=TrainingState::Idle;
std::function<void()> playtest_combat_setup;
std::string playtest_combat_error;
auto playtest_tell=[&](const std::string &message){playtest_notice=message;playtest_notice_seconds=6;};
auto playtest_combat_record_result=[&](){
    if(fight.state==playtest_combat_previous)return;
    if(fight.state==TrainingState::Won)++playtest_combat_wins;
    if(fight.state==TrainingState::Lost)++playtest_combat_defeats;
    playtest_combat_previous=fight.state;
};
auto playtest_place=[&](const world_playtest_slice::Spawn &s){
    playtest_walk_reference=s.eye_y-1.75;cam.x=s.x;cam.z=s.z;
    cam.y=walking_height(s.x,s.z,nullptr)+1.75;cam.yaw=float(s.yaw);cam.pitch=float(s.pitch);
    cam.fly=false;cam.grounded=true;cam.vy=0;third_person=true;avatar_initialized=false;
    current_name=playtest_state.path().name.c_str();studio.open=false;
    if(studio.components_runtime)studio.components_runtime->close_dialogue();
    show_tools=show_map=show_graphics=journey_open=false;preview_screen=1;preview_pending_save=false;
    milestone_review.view=world_milestone::View::None;playtest_combat_record_result();fight=WorldTrainingCombat{};live_training_encounter=0;preview_release_mouse();
    playtest_combat_previous=TrainingState::Idle;playtest_combat_ready=false;
    if(playtest_combat_setup)playtest_combat_setup();
    destination_loading=true;temporal_cut=true;
};
auto playtest_select=[&](unsigned section)->bool{
    if(section>1||(!stronghold_finish&&section))return false;
    world_playtest_slice::Path route;std::string error;
    if(!world_playtest_slice::load(section?"playtest-hall.route":"playtest.route",route,error)){playtest_tell(error);return false;}
    std::unique_ptr<world_playtest_walk::Support> support;
    if(stronghold_finish){
        support=std::make_unique<world_playtest_walk::Support>();
        if(!support->load(section?"playtest/hall.settings":"playtest/approach.settings",error)){playtest_tell(error);return false;}
        float floor=0;
        if(!support->sample(route.spawn.x,route.spawn.z,route.spawn.eye_y-1.75,floor)||std::abs(floor-(route.spawn.eye_y-1.75))>.15){playtest_tell("Route spawn does not match its collision floor.");return false;}
    }
    if(!playtest_state.start(route,&error)){playtest_tell(error);return false;}
    playtest_walk=std::move(support);active_playtest_walk=playtest_walk.get();playtest_section=section;
    playtest_qa_node=0;playtest_qa_hold=0;playtest_stuck=0;playtest_place(playtest_state.spawn());
    return true;
};
auto playtest_save=[&]()->bool{
    if(fight.engaged()){playtest_tell("Finish or restart the sword challenge before saving or loading.");return false;}
    if(!playtest_enabled||destination_loading||!cam.grounded||cam.fly){playtest_tell("Stand on the ground and finish loading before saving.");return false;}
    world_playtest_slice::Session session;session.section=playtest_section;session.fingerprint=world_playtest_slice::fingerprint(playtest_state.path());
    session.index=playtest_state.index();session.x=cam.x;session.eye_y=cam.y;session.z=cam.z;session.yaw=cam.yaw;session.pitch=cam.pitch;session.third_person=third_person;
    std::string bytes,error;if(!world_playtest_slice::encode_session(session,bytes,error)){playtest_tell(error);return false;}
    std::error_code ec;std::filesystem::create_directories("saves",ec);if(ec){playtest_tell(ec.message());return false;}
    std::string pattern="saves/playtest.progress.tmp-XXXXXX";std::vector<char> temp(pattern.begin(),pattern.end());temp.push_back(0);
    const int fd=mkstemp(temp.data());if(fd<0){playtest_tell("Could not stage the playtest save.");return false;}
    FILE *file=fdopen(fd,"wb");bool ok=false;
    if(file){ok=std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size()&&std::fflush(file)==0&&fsync(fd)==0;if(std::fclose(file)!=0)ok=false;}
    else close(fd);
    if(ok)ok=vw_replace(temp.data(),"saves/playtest.progress")==0;
    if(!ok){unlink(temp.data());playtest_tell("Could not commit the playtest save.");return false;}
    playtest_tell("Playtest session saved.");return true;
};
auto playtest_load=[&]()->bool{
    if(fight.engaged()){playtest_tell("Finish or restart the sword challenge before saving or loading.");return false;}
    std::ifstream file("saves/playtest.progress");world_playtest_slice::Session session;std::string error;
    if(!file||!world_playtest_slice::parse_session(file,session,error)){playtest_tell(error.empty()?"No saved playtest session yet.":error);return false;}
    if(std::abs(session.x)>world_atlas_width()*.5-1||std::abs(session.z)>world_atlas_depth()*.5-1){playtest_tell("Saved position is outside this world.");return false;}
    if(!stronghold_finish&&session.section){playtest_tell("This save belongs to another section.");return false;}
    world_playtest_slice::Path route;
    if(!world_playtest_slice::load(session.section?"playtest-hall.route":"playtest.route",route,error)){playtest_tell(error);return false;}
    world_playtest_slice::State staged;if(!staged.start(route,&error)||!world_playtest_slice::restore_session(session,staged,error)){playtest_tell(error);return false;}
    std::unique_ptr<world_playtest_walk::Support> support;
    if(stronghold_finish){
        support=std::make_unique<world_playtest_walk::Support>();
        if(!support->load(session.section?"playtest/hall.settings":"playtest/approach.settings",error)){playtest_tell(error);return false;}
        float floor=0;if(!support->sample(session.x,session.z,session.eye_y-1.75,floor)||std::abs(floor-(session.eye_y-1.75))>.15){playtest_tell("Saved position no longer has supported ground.");return false;}
    }else if(std::abs(world_atlas_walk_height(session.x,session.z)-(session.eye_y-1.75))>.15){playtest_tell("Saved position no longer matches the terrain.");return false;}
    if(support||active_city_physics){
        world_physics::Hit hit;const world_physics::Vec3 center{session.x,session.eye_y-1.75+.9,session.z},delta{0,.01,0};
        const bool queried=support?support->cast_capsule(center,.37f,.50f,delta,hit):active_city_physics->cast_capsule(center,.37f,.50f,delta,hit);
        if(!queried||(hit.hit&&hit.fraction<.999f&&hit.normal.y<.65f)){playtest_tell("Saved position has obstructed collision/headroom.");return false;}
    }
    if(!world_scenery_can_move(scenery,session.x,session.z,session.x,session.z,float(session.eye_y-1.75),1.75f,.37f)){playtest_tell("Saved position is obstructed.");return false;}
    playtest_state=std::move(staged);playtest_walk=std::move(support);active_playtest_walk=playtest_walk.get();playtest_section=session.section;
    playtest_place({session.x,session.eye_y,session.z,session.yaw,session.pitch});cam.y=session.eye_y;cam.yaw=float(session.yaw);cam.pitch=float(session.pitch);third_person=session.third_person;
    playtest_tell("Playtest session restored.");return true;
};
// Find a small real walking lane close to the start. Both combatants and the
// sidestep/riposte space must pass the ordinary terrain, mesh and actor casts.
// This adds a dedicated sparring opponent; ambient residents are never targets.
playtest_combat_setup=[&](){
    playtest_combat_ready=false;playtest_combat_error.clear();
    if(!playtest_enabled||!combat_motion)return;
    if(arena_enabled){playtest_combat_ready=true;playtest_combat_start_x=arena_origin_x;playtest_combat_start_z=arena_origin_z-5;playtest_combat_enemy_x=arena_origin_x;playtest_combat_enemy_z=arena_origin_z+3;playtest_combat_feet=float(arena_floor);preview_arena_x=arena_origin_x-fight.enemy_x;preview_arena_z=arena_origin_z+3-fight.enemy_z;preview_arena_feet=float(arena_floor);return;}
    if(stronghold_finish&&!playtest_section){playtest_combat_error="The sword challenge is in the first hall. Use the pause menu to travel there.";return;}
    const auto &spawn=playtest_state.spawn();const double reference=spawn.eye_y-1.75;
    const double saved_reference=playtest_walk_reference;playtest_walk_reference=reference;
    auto floor=[&](double x,double z,float &feet){
        if(active_playtest_walk)return active_playtest_walk->sample(x,z,reference,feet);
        if(world_atlas_local_water(x,z,nullptr,nullptr))return false;
        feet=walking_height(x,z,nullptr);return std::isfinite(feet);
    };
    auto clear=[&](double x,double z,float feet){
        if(!world_scenery_can_move(scenery,x,z,x,z,feet,1.75f,.37f)||
           !world_stronghold_can_move(active_stronghold,x,z,x,z,feet,.37f)||
           !world_live_actor_move_clear(active_live_visuals,active_world_runtime,x,z,x,z,feet,1.75f,.37f))return false;
        world_physics::Hit hit;
        if(active_playtest_walk&&(!active_playtest_walk->cast_capsule({x,feet+.9,z},.37f,.50f,{0,.01,0},hit)||
           (hit.hit&&hit.fraction<.999f&&hit.normal.y<.65f)))return false;
        if(active_city_physics&&(!active_city_physics->cast_capsule({x,feet+.9,z},.37f,.50f,{0,.01,0},hit)||
           (hit.hit&&hit.fraction<.999f&&hit.normal.y<.65f)))return false;
        return true;
    };
    auto lane=[&](double ax,double az,double bx,double bz,float feet){
        Camera probe=cam;probe.x=ax;probe.z=az;probe.y=feet+1.75;probe.fly=false;probe.grounded=true;probe.vy=0;
        const unsigned n=std::max(1u,unsigned(std::ceil(std::hypot(bx-ax,bz-az)/.12)));
        for(unsigned i=1;i<=n;++i){const double t=double(i)/n,nx=ax+(bx-ax)*t,nz=az+(bz-az)*t;
            const auto motion=move_camera(probe,scenery,nx,nz);
            if(motion.collision||motion.slope||std::hypot(probe.x-nx,probe.z-nz)>.015)return false;
            probe.y=walking_height(probe.x,probe.z,nullptr)+1.75;
        }
        return true;
    };
    double heading=spawn.yaw;
    for(const auto &node:playtest_state.path().nodes)if(std::hypot(node.x-spawn.x,node.z-spawn.z)>1){heading=std::atan2(node.x-spawn.x,-(node.z-spawn.z));break;}
    // The first candidate follows the reviewed route. Small rotations and
    // offsets handle foliage without moving the encounter to another district.
    for(unsigned offset=0;offset<5&&!playtest_combat_ready;++offset)for(unsigned angle=0;angle<16&&!playtest_combat_ready;++angle){
        const double a=heading+(angle?double((angle+1)/2)*(angle%2?1.:-1.)*.392699081698724:0);
        const double fx=std::sin(a),fz=-std::cos(a),rx=std::cos(a),rz=std::sin(a);
        const double side=offset?((offset+1)/2)*(offset%2?1.25:-1.25):0;
        const double sx=spawn.x+rx*side,sz=spawn.z+rz*side,ex=sx+fx*2.15,ez=sz+fz*2.15;
        float sf=0,ef=0;if(!floor(sx,sz,sf)||!floor(ex,ez,ef)||std::abs(sf-reference)>.25||std::abs(sf-ef)>.22||!clear(sx,sz,sf)||!clear(ex,ez,ef))continue;
        if(!lane(spawn.x,spawn.z,sx,sz,float(reference))||!lane(sx,sz,ex,ez,sf))continue;
        bool space=true;
        for(const double sign:{-1.,1.}){float side_floor=0;
            const double x=sx+rx*.65*sign,z=sz+rz*.65*sign;
            if(!floor(x,z,side_floor)||std::abs(side_floor-sf)>.22||!clear(x,z,side_floor)||!lane(sx,sz,x,z,sf)){space=false;break;}}
        if(!space)continue;
        playtest_combat_start_x=sx;playtest_combat_start_z=sz;playtest_combat_enemy_x=ex;playtest_combat_enemy_z=ez;playtest_combat_feet=ef;
        preview_arena_x=ex-fight.enemy_x;preview_arena_z=ez-fight.enemy_z;preview_arena_feet=ef;
        playtest_combat_ready=true;
        // Start facing the visible opponent with the existing armed hero.
        cam.yaw=float(std::atan2(ex-cam.x,-(ez-cam.z)));cam.pitch=-.08f;
        std::printf("[Playtest combat] Validated %s section %u lane: player %.6f %.6f enemy %.6f %.6f feet %.6f\n",playtest_area,playtest_section,sx,sz,ex,ez,double(ef));
    }
    playtest_walk_reference=saved_reference;
    if(!playtest_combat_ready)playtest_combat_error="No clear sword challenge lane could be validated in this section.";
};
auto playtest_combat_visible=[&](){return playtest_enabled&&playtest_combat_ready&&std::hypot(cam.x-playtest_combat_enemy_x,cam.z-playtest_combat_enemy_z)<100.;};
auto playtest_combat_can_step=[&](double x,double z){return arena_enabled||!fight.engaged()||(playtest_combat_ready&&std::hypot(x-playtest_combat_enemy_x,z-playtest_combat_enemy_z)<=5.5);};
auto playtest_combat_begin=[&]()->bool{
    if(arena_enabled&&arena_begin)return arena_begin();
    if(!playtest_enabled||!playtest_combat_ready||fight.engaged()||destination_loading||preview_screen!=1||cam.fly)return false;
    if(std::hypot(cam.x-playtest_combat_start_x,cam.z-playtest_combat_start_z)>6){playtest_tell("Return to the practice knight near the section start to challenge them.");return false;}
    playtest_combat_record_result();
    if(!fight.start_threat(preview_settings.story_assist,false))return false;
    // The existing single-opponent duel owns health, stamina, hit contacts,
    // guard/parry, reaction animation, sounds and VFX. It grants no quest credit.
    live_training_encounter=0;++playtest_combat_starts;++fight_starts;
    playtest_combat_previous=fight.state;
    cam.x=playtest_combat_start_x;cam.z=playtest_combat_start_z;
    playtest_walk_reference=playtest_combat_feet;cam.y=walking_height(cam.x,cam.z,nullptr)+1.75;
    cam.vy=0;cam.grounded=true;cam.fly=false;third_person=true;avatar_initialized=false;
    cam.yaw=float(std::atan2(playtest_combat_enemy_x-cam.x,-(playtest_combat_enemy_z-cam.z)));cam.pitch=-.12f;
    temporal_cut=true;fight_mouse_x=fight_mouse_y=0;look_active=!preview_pad.using_pad;
    fight_capture_focused=(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)!=0;
    SDL_SetRelativeMouseMode(look_active?SDL_TRUE:SDL_FALSE);
    playtest_tell("Sword challenge: attack another guard angle, parry the windup, and watch your stamina.");return true;
};
// Results own keyboard/controller focus until the player explicitly resumes
// exploration. Preserve route progress and placement while ending the duel.
auto playtest_combat_continue=[&](){
    if(fight.engaged())return false;
    playtest_combat_record_result();
    fight=WorldTrainingCombat{};playtest_combat_previous=TrainingState::Idle;
    playtest_combat_result_screen=TrainingState::Idle;live_training_encounter=0;
    preview_screen=1;preview_release_mouse();ImGui::SetWindowFocus(nullptr);
    playtest_tell("Exploration resumed. Return to the practice knight for another sword challenge.");return true;
};
playtest_restart=[&](){
    playtest_state.reset();playtest_place(playtest_state.spawn());playtest_qa_node=0;playtest_qa_hold=0;playtest_stuck=0;
    ++playtest_restarts;playtest_tell("Section restarted. Sword challenge reset; follow the gold markers to explore.");
};
playtest_use=[&](){
    if(!playtest_enabled||preview_screen!=1||destination_loading||fight.engaged())return false;
    if(playtest_combat_ready&&std::hypot(cam.x-playtest_combat_start_x,cam.z-playtest_combat_start_z)<=6)return playtest_combat_begin();
    if(!stronghold_finish||!playtest_state.complete())return false;
    if(playtest_select(playtest_section?0:1))playtest_tell(playtest_section?"First hall loaded as a separate walkable section.":"Returned to the snowy entrance section.");
    return true;
};
playtest_key=[&](SDL_Keycode key){
    if(arena_enabled&&arena_key&&arena_key(key))return true;
    if(key==SDLK_F10)return false;
    if(key==SDLK_ESCAPE){if(show_graphics)show_graphics=false;else preview_screen=preview_screen==1?2:1;preview_release_mouse();return true;}
    if(show_graphics)return true;
    if(preview_screen!=1)return true;
    if(key==SDLK_r){playtest_restart();return true;}
    if(key==SDLK_F5){playtest_save();return true;}if(key==SDLK_F8){playtest_load();return true;}
    if(key==SDLK_e){playtest_use();return true;}
    if((key==SDLK_RETURN||key==SDLK_KP_ENTER)&&!fight.engaged()){
        // Enter activates the focused result button through ImGui. Retrying
        // here would steal Enter from the adjacent Continue exploring button.
        if(fight.state!=TrainingState::Won&&fight.state!=TrainingState::Lost&&fight.state!=TrainingState::Cancelled)playtest_combat_begin();
        return true;
    }
    if(fight.engaged()&&(key==SDLK_LEFT||key==SDLK_RIGHT||key==SDLK_UP||key==SDLK_DOWN||key==SDLK_t||key==SDLK_SPACE))return false;
    if(key==SDLK_j||key==SDLK_TAB){preview_screen=2;preview_release_mouse();return true;}
    return key!=SDLK_w&&key!=SDLK_a&&key!=SDLK_s&&key!=SDLK_d&&key!=SDLK_v&&key!=SDLK_SPACE&&key!=SDLK_LSHIFT&&key!=SDLK_RSHIFT;
};
playtest_tick=[&](float delta){
    playtest_notice_seconds=std::max(0.f,playtest_notice_seconds-std::clamp(delta,0.f,.1f));
    playtest_combat_record_result();
    if(destination_loading||preview_screen!=1||cam.fly||fight.engaged())return;
    const bool was=playtest_state.complete();
    playtest_state.update(cam.x,cam.y-1.75,cam.z);
    if(!was&&playtest_state.complete()){++playtest_completed_sections;playtest_tell("Route complete. Keep exploring, or restart from the pause menu.");}
};
auto playtest_drive=[&](uint64_t current_frame){
    if(!playtest_qa||playtest_qa_done||destination_loading||current_frame<=uint64_t(launch->warmup)||!terrain.view_ready(cam.x,cam.z,false))return;
    const auto &nodes=playtest_state.path().nodes;
    if(playtest_qa_node<nodes.size()){
        const auto &p=nodes[playtest_qa_node];const double dx=p.x-cam.x,dz=p.z-cam.z,d=std::hypot(dx,dz);
        if(d<.025){++playtest_qa_node;return;}
        const double step=std::min(d,world_atlas_walk_speed()/60.);
        auto motion=move_camera(cam,scenery,cam.x+dx/d*step,cam.z+dz/d*step);
        playtest_qa_distance+=motion.actual;playtest_qa_blocks+=motion.collision;playtest_qa_slopes+=motion.slope;++playtest_qa_frames;
        cam.y=walking_height(cam.x,cam.z,nullptr)+1.75;cam.yaw=float(std::atan2(dx,-dz));cam.pitch=-.025f;cam.fly=false;cam.grounded=true;cam.vy=0;
        if(motion.actual<step*.2)++playtest_stuck;else playtest_stuck=0;
        if(playtest_stuck>120){std::fprintf(stderr,"[Playtest QA] Blocked in section %u, point %zu at %.3f %.3f\n",playtest_section,playtest_qa_node,cam.x,cam.z);launch->exit_code=1;running=false;}
        return;
    }
    if(++playtest_qa_hold<90)return;
    if(!playtest_state.complete()||!playtest_save()||!playtest_load()){
        std::fprintf(stderr,"[Playtest QA] Completion/save/load failed: %s\n",playtest_notice.c_str());launch->exit_code=1;running=false;return;
    }
    ++playtest_save_load_checks;
    if(stronghold_finish&&!playtest_section){if(!playtest_select(1)){launch->exit_code=1;running=false;}return;}
    playtest_qa_done=true;running=false;
};
playtest_draw=[&](const Camera &eye,bool loading){
    if(arena_enabled&&arena_draw){arena_draw(eye,loading);return;}
    if(show_graphics)return;
    const auto &io=ImGui::GetIO();const float w=io.DisplaySize.x,h=io.DisplaySize.y;
    const float u=std::clamp(std::min(w/1600.f,h/900.f),.65f,1.4f)*preview_settings.text_scale;
    auto *dl=ImGui::GetForegroundDrawList();const ImU32 gold=IM_COL32(225,192,128,255),white=IM_COL32(241,237,225,255),muted=IM_COL32(185,197,190,255);
    auto text=[&](float x,float y,float size,ImU32 color,const char *s){dl->AddText(ImGui::GetFont(),size*u,{x,y},color,s);};
    const bool pad=preview_pad.using_pad&&preview_pad.get();const std::string confirm=preview_pad.button(SDL_CONTROLLER_BUTTON_A);
    dl->AddRectFilled({24*u,24*u},{std::min(w-24*u,590*u),137*u},IM_COL32(8,20,21,205),6*u);
    text(42*u,36*u,12,gold,"LEGENDS OF VALWOOD  /  PLAYTEST");text(42*u,58*u,22,white,playtest_state.path().name.c_str());
    char status[192];std::snprintf(status,sizeof status,"%zu / %zu landmarks  |  %s",playtest_state.reached(),playtest_state.total(),playtest_state.complete()?"ROUTE COMPLETE":playtest_state.getlabel());
    text(42*u,94*u,14,muted,status);
    if(playtest_notice_seconds>0)text(42*u,151*u,14,gold,playtest_notice.c_str());
    if(preview_screen==1&&!loading){
        const auto *goal=playtest_state.next();
        if(goal&&!fight.engaged()){
            const double dx=goal->x-eye.x,dy=goal->ground_y+2.1-eye.y,dz=goal->z-eye.z;const auto f=eye.forward(),r=eye.right(),up=cross(r,f);
            const double depth=dx*f.x+dy*f.y+dz*f.z;float px=w*.5f,py=h*.5f;
            if(depth>.1){px+=float((dx*r.x+dy*r.y+dz*r.z)/depth/std::tan(eye.fov*.5f)/eye.aspect)*w*.5f;py-=float((dx*up.x+dy*up.y+dz*up.z)/depth/std::tan(eye.fov*.5f))*h*.5f;}
            else px=dx*r.x+dz*r.z>0?w-45*u:45*u;
            px=std::clamp(px,45*u,w-45*u);py=std::clamp(py,185*u,h-160*u);const float r1=7*u;
            dl->AddQuadFilled({px,py-r1},{px+r1,py},{px,py+r1},{px-r1,py},gold);
            char distance[48];std::snprintf(distance,sizeof distance,"%.0f m",playtest_state.distance(cam.x,cam.z));text(px-16*u,py+14*u,13,white,distance);
        }
        const auto &path=playtest_state.path();double xmin=path.spawn.x,xmax=xmin,zmin=path.spawn.z,zmax=zmin;
        for(const auto &p:path.nodes){xmin=std::min(xmin,p.x);xmax=std::max(xmax,p.x);zmin=std::min(zmin,p.z);zmax=std::max(zmax,p.z);}
        const float mw=190*u,mh=175*u,mx=w-mw-38*u,my=60*u;const double span=std::max({xmax-xmin,zmax-zmin,12.})*1.18;
        auto point=[&](double x,double z){return ImVec2(mx+mw*.5f+float((x-(xmin+xmax)*.5)/span)*mw,my+mh*.5f-float((z-(zmin+zmax)*.5)/span)*mh);};
        dl->AddRectFilled({mx-12*u,my-25*u},{mx+mw+12*u,my+mh+12*u},IM_COL32(8,20,21,205),6*u);text(mx,my-19*u,11,gold,"WALKING ROUTE");
        ImVec2 previous=point(path.spawn.x,path.spawn.z);
        for(const auto &p:path.nodes){const auto current=point(p.x,p.z);dl->AddLine(previous,current,IM_COL32(178,153,99,210),1.7f*u);if(p.visible)dl->AddCircleFilled(current,3*u,gold);previous=current;}
        const auto here=point(cam.x,cam.z);dl->AddCircleFilled(here,4*u,IM_COL32(97,230,190,255));dl->AddLine(here,{here.x+std::sin(cam.yaw)*11*u,here.y+std::cos(cam.yaw)*11*u},white,2*u);
        const std::string controls=fight.engaged()?
            (pad?"RS: swing (up: rising / down: overhead)  |  LT + RS: block  |  LT + centered RS: thrust guard  |  B: dodge  |  START: pause":
                 "Left mouse: attack  |  Right mouse: guard / parry  |  Arrows: angle  |  T: thrust  |  Space: dodge  |  R: restart  |  Esc: pause"):
            (pad?"LS: walk  |  RS: look  |  A: sword challenge / travel  |  RS click: camera  |  START: pause":
                 "WASD: walk  |  Right mouse: look  |  E: sword challenge / travel  |  V: camera  |  R: restart  |  F5 / F8: save / load  |  Esc: pause");
        dl->AddRectFilled({24*u,h-63*u},{w-24*u,h-24*u},IM_COL32(8,20,21,205),5*u);text(40*u,h-51*u,13,muted,controls.c_str());
        if(fight.engaged()){
            const float bx=42*u,by=h-235*u,bw=335*u;
            dl->AddRectFilled({bx-18*u,by-14*u},{bx+bw+18*u,h-82*u},IM_COL32(8,20,21,220),6*u);
            text(bx,by,15,gold,"SWORD CHALLENGE  /  PRACTICE KNIGHT");
            auto bar=[&](float y,float value,ImU32 color){dl->AddRectFilled({bx,y},{bx+bw,y+8*u},IM_COL32(28,42,42,255),3*u);dl->AddRectFilled({bx,y},{bx+bw*std::clamp(value,0.f,1.f),y+8*u},color,3*u);};
            char health[96];std::snprintf(health,sizeof health,"YOUR HEALTH %.0f    STAMINA %.0f",fight.health,fight.stamina);text(bx,by+25*u,13,white,health);
            bar(by+46*u,fight.health/100.f,IM_COL32(192,75,67,255));bar(by+60*u,fight.stamina/100.f,IM_COL32(114,179,126,255));
            text(bx,by+77*u,12,muted,"OPPONENT");bar(by+96*u,fight.enemy_health/std::max(1.f,fight.enemy_max_health),IM_COL32(211,164,87,255));
            std::string combat_status;
            if(fight.state==TrainingState::Countdown)combat_status="GET READY  "+std::to_string(int(std::ceil(fight.phase_time)));
            else if(fight.enemy_phase==TrainingEnemyPhase::Windup)combat_status=(fight.guard?std::string("YOUR BLOCK: ")+world_training_direction_name(fight.selected)+"  |  ":std::string())+std::string(fight.enemy_duration-fight.enemy_time<.24?"PARRY NOW: ":"INCOMING: ")+world_training_direction_name(fight.enemy_direction);
            else combat_status=std::string(fight.guard?"YOUR BLOCK: ":"YOUR ANGLE: ")+world_training_direction_name(fight.selected)+"  |  OPPONENT GUARD: "+world_training_direction_name(fight.enemy_guard);
            text(w*.5f-215*u,h-123*u,17,gold,combat_status.c_str());
            if(fight.feedback_time>0)text(w*.5f-215*u,h-96*u,13,white,fight.feedback);
        }else if(fight.state==TrainingState::Won||fight.state==TrainingState::Lost||fight.state==TrainingState::Cancelled){
            const bool won=fight.state==TrainingState::Won;
            ImGui::SetNextWindowPos({w*.5f,h-205*u},ImGuiCond_Always,{.5f,0});ImGui::SetNextWindowSize({440*u,0},ImGuiCond_Always);
            const bool focus=playtest_combat_result_screen!=fight.state;if(focus)ImGui::SetNextWindowFocus();playtest_combat_result_screen=fight.state;
            ImGui::Begin("Sword challenge result",nullptr,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings);
            ImGui::TextColored(won?ImVec4(.55f,.9f,.66f,1):ImVec4(.95f,.7f,.5f,1),"%s",won?"VICTORY - sword challenge complete":"YOU YIELD - try another approach");
            ImGui::Text("Hits %u  |  Parries %u  |  Blocks %u",fight.hits,fight.parries,fight.blocks);
            bool first_focus=focus;
            if(ImGui::Button(won?"Fight again  [E / Enter]":"Retry sword challenge  [E / Enter]",{-1,32*u}))playtest_combat_begin();
            world_controller::default_focus(first_focus);
            if(ImGui::Button("Continue exploring",{-1,32*u}))playtest_combat_continue();
            ImGui::TextWrapped("Choose Continue exploring to leave the result screen, or R to restart this section.");ImGui::End();
        }else{
            playtest_combat_result_screen=TrainingState::Idle;
            if(playtest_combat_ready&&std::hypot(cam.x-playtest_combat_start_x,cam.z-playtest_combat_start_z)<=6){
                const std::string prompt="["+(pad?confirm:std::string("E"))+"]  START SWORD CHALLENGE";text(w*.5f-180*u,h-108*u,17,gold,prompt.c_str());
            }else if(stronghold_finish&&!playtest_section){
                const std::string prompt="PAUSE > TRAVEL TO FIRST HALL FOR THE SWORD CHALLENGE";text(w*.5f-290*u,h-136*u,15,gold,prompt.c_str());
            }
        }
        if(stronghold_finish&&playtest_state.complete()&&!fight.engaged()&&(!playtest_combat_ready||std::hypot(cam.x-playtest_combat_start_x,cam.z-playtest_combat_start_z)>6)){
            const std::string prompt="["+(pad?confirm:std::string("E"))+"]  "+(playtest_section?"RETURN TO SNOWY ENTRANCE":"TRAVEL TO FIRST HALL + SWORD CHALLENGE");text(w*.5f-240*u,h-108*u,15,gold,prompt.c_str());
        }

    }
    if(preview_screen!=1){
        dl->AddRectFilled({0,0},{w,h},IM_COL32(4,10,13,155));
        const float width=std::min(w-40*u,470*u);ImGui::SetNextWindowPos({(w-width)*.5f,125*u},ImGuiCond_Always);ImGui::SetNextWindowSize({width,0},ImGuiCond_Always);
        const bool focus=playtest_last_screen!=preview_screen;if(focus)ImGui::SetNextWindowFocus();playtest_last_screen=preview_screen;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{24*u,22*u});ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{10*u,11*u});ImGui::PushStyleColor(ImGuiCol_WindowBg,{.035f,.065f,.065f,.98f});
        ImGui::Begin("Playtest paused",nullptr,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetWindowFontScale(std::max(.8f,u));bool default_focus=focus;
        auto button=[&](const char *label){bool pressed=ImGui::Button(label,{-1,35*u});world_controller::default_focus(default_focus);return pressed;};
        ImGui::TextWrapped("%s",playtest_state.path().name.c_str());ImGui::Text("Landmarks: %zu / %zu",playtest_state.reached(),playtest_state.total());ImGui::Separator();
        if(button("Resume")){preview_screen=1;preview_release_mouse();}
        if(button("Restart section and sword challenge"))playtest_restart();
        if(playtest_combat_ready&&!fight.engaged()){
            ImGui::BeginDisabled(std::hypot(cam.x-playtest_combat_start_x,cam.z-playtest_combat_start_z)>6);
            if(button("Start sword challenge")){preview_screen=1;playtest_combat_begin();}ImGui::EndDisabled();
        }
        if(stronghold_finish&&button(playtest_section?"Travel to snowy entrance":"Travel to first hall + sword challenge"))playtest_select(playtest_section?0:1);
        ImGui::BeginDisabled(loading||!cam.grounded||fight.engaged());if(button("Save playtest session"))playtest_save();ImGui::EndDisabled();
        ImGui::BeginDisabled(fight.engaged()||!std::filesystem::is_regular_file("saves/playtest.progress"));if(button("Load saved session"))playtest_load();ImGui::EndDisabled();
        if(button("Graphics and display")){show_graphics=true;graphics_draft=graphics;}
        if(button("Exit playtest"))running=false;
        ImGui::TextWrapped("Your existing armed hero can fight a dedicated practice knight near the start. Ambient residents are not combat targets. Gold markers guide free exploration; R resets this section and its fight.");
        if(!playtest_combat_error.empty())ImGui::TextWrapped("%s",playtest_combat_error.c_str());
        ImGui::End();ImGui::PopStyleColor();ImGui::PopStyleVar(2);
    }else playtest_last_screen=1;
};
if(playtest_enabled){
    if(studio.open){std::fprintf(stderr,"[Playtest] The saved scene needs repair in its artist workspace.\n");valid=false;launch->exit_code=1;running=false;}
    else{playtest_place(playtest_state.spawn());
        // The playable Grandstone start is its first-hall encounter. Bounded
        // route/input QA still begins at the approach to retain coverage there.
        if(stronghold_finish&&!playtest_qa&&!playtest_input_qa&&!playtest_select(1)){valid=false;launch->exit_code=1;running=false;}
        std::printf("[Playtest] %s: %zu route points, %zu landmarks\n",playtest_state.path().name.c_str(),playtest_state.path().nodes.size(),playtest_state.total());}
}
#include "world_playtest_input_qa.inc"

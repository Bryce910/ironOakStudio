// Disposable root-run actual-input water confluence regression. Geometry is
// created only by viewport clicks and the ordinary Finish river button.
if(qa_water_refine){
    struct WaterQA {unsigned phase=0,checks=0;uint64_t since=0,started=0,draws=0;size_t ribbons=0,history=0;unsigned applies=0;bool pending=false,failed=false;ImVec2 requested{},a{},b{};double ax=0,az=0,bx=0,bz=0;float joined_level=0;std::string before,after;};
    static WaterQA wq;
    auto check=[&](bool ok,const char *label){++wq.checks;if(!ok){wq.failed=true;launch->exit_code=1;running=false;std::fprintf(stderr,"[Water refine QA] FAIL phase %u: %s | %s\n",wq.phase,label,studio.message.c_str());std::fflush(stderr);}return ok;};
    auto bytes=[&](const world_studio::Document &doc){std::string text,error;check(world_studio::encode(doc,text,error),"encode document");return text;};
    auto emit=[&](ImVec2 at,int button){SDL_WarpMouseInWindow(window,int(at.x),int(at.y));SDL_PumpEvents();SDL_Event e{};e.type=SDL_MOUSEMOTION;e.motion.windowID=SDL_GetWindowID(window);e.motion.which=0x56414c54u;e.motion.x=int(at.x);e.motion.y=int(at.y);SDL_PushEvent(&e);ImGui::GetIO().AddMousePosEvent(at.x,at.y);if(button>=0){e={};e.type=button?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;e.button.windowID=SDL_GetWindowID(window);e.button.which=0x56414c54u;e.button.button=SDL_BUTTON_LEFT;e.button.state=button?SDL_PRESSED:SDL_RELEASED;e.button.x=int(at.x);e.button.y=int(at.y);SDL_PushEvent(&e);ImGui::GetIO().AddMouseButtonEvent(0,button!=0);}};
    auto mouse=[&](ImVec2 at,int button){wq.requested=at;wq.pending=button==1;emit(at,button==1?-1:button);wq.since=frame;};
    auto key=[&](SDL_Keycode code,bool ctrl=false,bool shift=false){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.windowID=SDL_GetWindowID(window);e.key.state=SDL_PRESSED;e.key.keysym.sym=code;e.key.keysym.unused=0x56414c54u;e.key.keysym.mod=SDL_Keymod((ctrl?KMOD_CTRL:0)|(shift?KMOD_SHIFT:0));SDL_PushEvent(&e);e.type=SDL_KEYUP;e.key.state=SDL_RELEASED;e.key.keysym.mod=KMOD_NONE;SDL_PushEvent(&e);wq.since=frame;};
    auto normalized=[&](world_studio::Document doc){doc.entities.next_id=1;for(const auto &node:doc.entities.nodes)doc.entities.next_id=std::max(doc.entities.next_id,node.id+1);return bytes(doc);};
    const bool reopen=std::getenv("VALWOOD_QA_WATER_REOPEN")!=nullptr;
    const bool settled=!destination_loading&&terrain.view_ready(cam.x,cam.z,cam.fly)&&!studio.dirty&&!studio.apply&&!studio.designer_frame_pending&&!studio.production_freeze();
    auto screen=[&](double x,double z,ImVec2 &at){auto &io=ImGui::GetIO();return studio.project_point(cam,{x,world_atlas_height(x,z)+.02,z},at)&&studio.designer_viewport(io.DisplaySize.x,io.DisplaySize.y).contains(at.x,at.y);};
    auto observe_join=[&](){float level=0,shore=0;check(studio.live.surface_field&&world_surface::river(*studio.live.surface_field,wq.bx,wq.bz,level,shore),"joined river has sampled water");check(std::abs(level-wq.joined_level)<.06,"joined river preserves existing visible water level");check(world_atlas_height(wq.bx,wq.bz)<level-.25,"joined bed remains submerged");check(!studio_surface_gpu.rivers.ranges.empty()&&studio_river_draws>wq.draws,"junction issues actual main-view water draws");};
    if(wq.started&&wq.phase!=200&&SDL_GetTicks64()-wq.started>600000)check(false,"bounded water workflow timed out");
    const bool focused=(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)!=0&&!ImGui::GetIO().AppFocusLost;
    if(!focused){if(frame%30==0){SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);}wq.since=frame;}
    else if(!wq.failed){
        if(wq.pending&&frame>wq.since){emit(wq.requested,1);wq.pending=false;wq.since=frame;}
        if(wq.phase==0&&settled&&(reopen||qa_refine_passed)){
            if(!wq.started)wq.started=SDL_GetTicks64();
            check(studio.live.surfaces.ribbons.size()>=size_t(reopen?2:1),"saved fixture has required river paths");
            if(!wq.failed){
                studio.open=true;studio.designer_content=false;studio.designer_tools=true;studio.designer_immersive=false;studio.tab=studio.requested_tab=15;
                if(reopen){const auto &r=studio.live.surfaces.ribbons.back();wq.bx=r.points.back().x;wq.bz=r.points.back().z;float shore=0;check(world_surface::river(*studio.live.surface_field,wq.bx,wq.bz,wq.joined_level,shore),"reopened junction surface exists");if(const char *expected=std::getenv("VALWOOD_QA_WATER_EXPECTED_LEVEL")){char *end=nullptr;const double value=std::strtod(expected,&end);check(end&&!*end&&std::isfinite(value)&&std::abs(value-wq.joined_level)<.06,"fresh process preserves previously observed junction water level");}wq.phase=90;}
                else{
                    wq.ribbons=studio.live.surfaces.ribbons.size();wq.history=studio.history.back.size();wq.applies=studio.applied;wq.before=normalized(studio.draft);
                    studio.landscape_choose_water(2);studio.landscape_river_width=6;studio.landscape_river_bank=3;studio.landscape_river_depth=1.2f;
                    const auto &r=studio.live.surfaces.ribbons.back();
                    bool found=false;auto source=studio.landscape_river_source();
                    for(double fraction:{.5,.25,.75})if(!found){
                        const double bx=r.points.front().x+(r.points.back().x-r.points.front().x)*fraction,bz=r.points.front().z+(r.points.back().z-r.points.front().z)*fraction;float level=0,shore=0;std::string footprint_error;
                        if(!world_surface::river(*studio.live.surface_field,bx,bz,level,shore)||!studio.landscape_water_footprint(std::round(bx),std::round(bz),studio.landscape_water_clearance()+1,footprint_error))continue;
                        for(double length:{24.,32.,48.,64.})for(unsigned i=0;i<32&&!found;++i){
                            const double angle=i*6.283185307179586/32,x=bx+std::cos(angle)*length,z=bz+std::sin(angle)*length,h=studio.landscape_river_point_height(studio.draft,x,z);float other=0;
                            if(h<level+.05||world_atlas_local_water(x,z,&other,nullptr)||!studio.landscape_water_footprint(std::round(x),std::round(z),studio.landscape_water_clearance()+1,footprint_error))continue;
                            std::vector<world_terrain_authoring::Point> points={{x,z,h},{bx,bz,level}};auto candidate=studio.draft;world_terrain_authoring::Report report;std::string error;
                            if(world_terrain_authoring::simple_river(candidate,points,6,3,1.2f,source,report,error)){wq.ax=x;wq.az=z;wq.bx=bx;wq.bz=bz;wq.joined_level=level;found=true;}
                        }
                    }
                    check(found,"safe tributary preflight reaches existing river");wq.phase=1;
                }
                cam.pitch=-1.1f;cam.fly=true;studio.designer_frame_center={(wq.ax+wq.bx)*.5,world_atlas_height(wq.bx,wq.bz),(wq.az+wq.bz)*.5};
                if(reopen)studio.designer_frame_center={wq.bx,world_atlas_height(wq.bx,wq.bz),wq.bz};
                studio.designer_frame_radius=60;studio.designer_frame_reveal=false;studio.designer_frame_pending=true;wq.draws=studio_river_draws;wq.since=frame;
            }
        }else if(wq.phase==1&&settled&&frame-wq.since>8){if(check(screen(wq.ax,wq.az,wq.a)&&screen(wq.bx,wq.bz,wq.b),"tributary endpoints project inside viewport")){mouse(wq.a,1);wq.phase=2;}}
        else if(wq.phase==2&&frame-wq.since>2){mouse(wq.requested,0);wq.phase=3;}
        else if(wq.phase==3&&frame-wq.since>4){check(studio.landscape_river_points.size()==1,"upstream viewport click accepted");mouse(wq.b,1);wq.phase=4;}
        else if(wq.phase==4&&frame-wq.since>2){mouse(wq.requested,0);wq.phase=5;}
        else if(wq.phase==5&&frame-wq.since>4){check(studio.landscape_river_points.size()==2,"viewport click on existing river accepted");const auto at=studio.landscape_buttons[13];if(at.y>160&&at.y<ImGui::GetIO().DisplaySize.y-80){mouse(at,1);wq.phase=6;}else if(frame%5==0){emit({ImGui::GetIO().DisplaySize.x-100,ImGui::GetIO().DisplaySize.y*.7f},-1);SDL_Event e{};e.type=SDL_MOUSEWHEEL;e.wheel.windowID=SDL_GetWindowID(window);e.wheel.which=0x56414c54u;e.wheel.y=at.y<160?3:-3;e.wheel.preciseY=float(e.wheel.y);SDL_PushEvent(&e);}}
        else if(wq.phase==6&&frame-wq.since>2){mouse(wq.requested,0);wq.phase=7;}
        else if(wq.phase==7&&settled&&frame-wq.since>8){check(studio.live.surfaces.ribbons.size()==wq.ribbons+1&&studio.applied==wq.applies+1,"Finish river publishes exactly one tributary");check(studio.history.back.size()==std::min(size_t(32),wq.history+1),"joined tributary has one Undo");observe_join();wq.after=normalized(studio.draft);key(SDLK_z,true);wq.phase=8;}
        else if(wq.phase==8&&settled&&frame-wq.since>8){check(normalized(studio.draft)==wq.before,"Undo restores complete pre-tributary document");key(SDLK_y,true);wq.phase=9;}
        else if(wq.phase==9&&settled&&frame-wq.since>8){check(normalized(studio.draft)==wq.after,"Redo restores complete joined river");observe_join();key(SDLK_s,true,true);wq.phase=10;}
        else if(wq.phase==10&&settled&&frame-wq.since>20){world_studio::Document saved;std::string error;check(world_studio::load(studio.path.c_str(),saved,error)&&normalized(saved)==wq.after,"Save All persists joined river");wq.phase=90;wq.since=frame;}
        else if(wq.phase==90&&settled&&frame-wq.since>90){observe_join();qa_water_passed=!wq.failed;wq.phase=200;std::printf("[Water refine QA] PASS %u checks; actual viewport tributary join, Finish, Undo/Redo, Save/reopen and water draws\n",wq.checks);std::fflush(stdout);launch->frames=launch->rendered_frames+1;}
    }
    qa_water_phase=wq.phase;qa_water_checks=wq.checks;qa_water_joined_level=wq.joined_level;
}

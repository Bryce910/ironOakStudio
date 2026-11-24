// Actual toolbar/viewport input. Runs only against an explicit /tmp document;
// the fixture changes camera/settings, never installs a fabricated final scene.
        if(qa_landscape&&studio.path.rfind("/tmp/",0)==0){
            ImGui::GetIO().IniFilename=nullptr;
            struct LandscapeQA {unsigned phase=0,checks=0,applies=0,strokes=0,focus_frames=0,stationary_mode=0,stationary_samples=0,water_mode=0,water_applies=0,water_creations=0;std::uint64_t since=0,next_focus=0,stationary_since=0,water_draws_before=0;size_t history=0,props=0,areas=0,ribbons=0,waters=0,water_history=0;double x=0,z=0,old_height=0,stationary_walk_before=0,stationary_height_after=0,stationary_walk_after=0,stationary_first_delta=0,stationary_pick_x=0,stationary_pick_z=0,water_old_height=0,water_old_walk=0,water_height_after=0,water_walk_after=0;float old_pitch=0;bool failed=false,pending_press=false;std::string before,shaped,stationary_before,stationary_after,water_before,water_after;ImVec2 a,b,requested;Camera stationary_previous_camera;world_studio::Water authored_water;world_studio::Document water_before_document;world_entity::Id water_after_id=1;std::vector<world_terrain_authoring::Point> river;};static LandscapeQA q;
            auto check=[&](bool good,const char *label){++q.checks;if(!good){int raw_x=0,raw_y=0;const auto raw_buttons=SDL_GetMouseState(&raw_x,&raw_y);const auto &io=ImGui::GetIO();std::fprintf(stderr,"[Landscape QA] FAIL phase %u: %s | %s | focus=%d imgui_lost=%d capture=%d io=(%.2f,%.2f) down=%d raw=(%d,%d) buttons=%u requested=(%.2f,%.2f) requested_pixel_rect=[%d,%d,%d,%d] pending_press=%d\n",q.phase,label,studio.message.c_str(),int((SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)!=0),int(io.AppFocusLost),int(io.WantCaptureMouse),io.MousePos.x,io.MousePos.y,int(io.MouseDown[0]),raw_x,raw_y,unsigned(raw_buttons),q.requested.x,q.requested.y,int(q.requested.x),int(q.requested.y),int(q.requested.x)+1,int(q.requested.y)+1,int(q.pending_press));std::fflush(stderr);launch->exit_code=1;q.failed=true;running=false;}return good;};
            auto bytes=[&](const world_studio::Document &d){std::string text,error;if(!world_studio::encode(d,text,error))check(false,"encode actual authoring state");return text;};
            auto emit_mouse=[&](ImVec2 p,int button){
                SDL_WarpMouseInWindow(window,int(p.x),int(p.y));
                SDL_PumpEvents();
                SDL_Event e{};e.type=SDL_MOUSEMOTION;e.motion.windowID=SDL_GetWindowID(window);e.motion.x=int(p.x);e.motion.y=int(p.y);SDL_PushEvent(&e);
                ImGui::GetIO().AddMousePosEvent(p.x,p.y);
                if(button>=0){e={};e.type=button?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;e.button.windowID=SDL_GetWindowID(window);e.button.button=SDL_BUTTON_LEFT;e.button.state=button?SDL_PRESSED:SDL_RELEASED;e.button.x=int(p.x);e.button.y=int(p.y);SDL_PushEvent(&e);ImGui::GetIO().AddMouseButtonEvent(0,button!=0);}
            };
            // Submit hover first. A later frame submits DOWN at this exact
            // recorded widget/viewport point; UP never samples stale IO state.
            auto mouse=[&](ImVec2 p,int button){q.requested=p;q.pending_press=button==1;emit_mouse(p,button==1?-1:button);q.since=frame;};
            auto click=[&](unsigned id){mouse(studio.landscape_buttons[id],1);};
            auto release=[&](){mouse(q.requested,0);};
            auto key=[&](SDL_Keycode code,bool ctrl=false,bool shift=false){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.windowID=SDL_GetWindowID(window);e.key.state=SDL_PRESSED;e.key.keysym.sym=code;e.key.keysym.mod=SDL_Keymod((ctrl?KMOD_CTRL:0)|(shift?KMOD_SHIFT:0));SDL_PushEvent(&e);e.type=SDL_KEYUP;e.key.state=SDL_RELEASED;SDL_PushEvent(&e);q.since=frame;};
            auto screen=[&](double x,double z,ImVec2 &p){const auto &io=ImGui::GetIO();return studio.project_point(cam,{x,world_atlas_height(x,z)+.02,z},p)&&studio.designer_viewport(io.DisplaySize.x,io.DisplaySize.y).contains(p.x,p.y);};
            // This observes the committed atlas and collision sampler. The
            // water itself must have come from the real toolbar/viewport path;
            // no fixture object or direct authoring operation is installed.
            auto basin=[&](const world_studio::Water &w){
                float level=0,depth=0;
                check(w.source==3&&w.enabled,"created basin has enabled local water");
                check(world_atlas_local_water(w.x,w.z,&level,&depth)&&std::abs(level-w.level)<.02&&depth>.25,"basin centre samples the committed water surface and depth");
                check(world_atlas_height(w.x,w.z)<w.level-.25&&std::abs(world_atlas_walk_height(w.x,w.z)-world_atlas_height(w.x,w.z))<.02,"basin centre has a submerged rendered and walking collision floor");
                bool dry=true;const double rim=double(w.radius_x);
                for(unsigned i=0;i<8;++i){const double a=i*6.283185307179586/8,x=w.x+std::cos(a)*rim,z=w.z+std::sin(a)*rim;dry&=world_atlas_height(x,z)>w.level+.02&&!world_atlas_local_water(x,z,nullptr,nullptr);}
                check(dry,"basin has dry terrain around its full water shoreline");
            };
            const bool settled=!destination_loading&&terrain.view_ready(cam.x,cam.z,cam.fly)&&!studio.dirty&&!studio.apply&&!studio.designer_frame_pending&&!studio.production_freeze();
            const bool reopen=std::getenv("VALWOOD_QA_EDITOR_LANDSCAPE_REOPEN")!=nullptr;
            const bool input_focus=(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)!=0&&!ImGui::GetIO().AppFocusLost;
            if(!input_focus){
                q.focus_frames=0;q.since=frame;
                if(frame>=q.next_focus){std::fprintf(stderr,"[Landscape QA] FOCUS request frame %llu phase %u\n",(unsigned long long)frame,q.phase);SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_PumpEvents();q.next_focus=frame+30;}
            }else if(++q.focus_frames>=2){
            if(q.pending_press&&frame>q.since){emit_mouse(q.requested,1);q.pending_press=false;q.since=frame;}
            if(reopen){
                if(q.phase==0&&frame>45&&settled){
                    check(!studio.live.ecology.empty()&&!studio.live.terrain.empty()&&!studio.live.surfaces.ribbons.empty(),"fresh process loads saved landscape features");
                    if(!q.failed){const auto &r=studio.live.ecology.back();cam.x=r.x;cam.z=r.z+45;cam.y=world_atlas_height(cam.x,cam.z)+35;cam.yaw=0;cam.pitch=-.65f;cam.fly=true;studio.open=true;studio.designer_tools=true;studio.tab=studio.requested_tab=15;q.phase=100;q.since=frame;}
                }else if(q.phase==100&&settled&&frame-q.since>40&&dressing.tree_count>0&&dressing.scanned_grass_count>0){
                    check(!studio_surface_gpu.rivers.ranges.empty(),"fresh process rebuilds real river mesh");
                    check(studio.rejected==0,"fresh process accepts landscape state");
                    if(check(studio.live.waters.size()>=2,"fresh process loads saved Pond and Lake water bodies")){
                        const auto n=studio.live.waters.size();check(std::abs(studio.live.waters[n-2].radius_x-12)<.01&&std::abs(studio.live.waters[n-1].radius_x-24)<.01,"fresh process preserves separate Pond and Lake sizes");
                        check(std::abs(studio.live.waters[n-2].basin_depth-1)<.01&&std::abs(studio.live.waters[n-1].basin_depth-3)<.01,"fresh process preserves shallow Pond and unchanged Lake depth recipes");
                        q.water_mode=0;q.phase=101;q.since=frame;
                    }
                }else if(q.phase==101&&settled&&frame-q.since>4){
                    const auto n=studio.live.waters.size();q.authored_water=studio.live.waters[n-2+q.water_mode];q.water_draws_before=studio_pond_draws[n-2+q.water_mode];
                    cam.pitch=-1.2f;studio.designer_frame_center={q.authored_water.x,q.authored_water.level,q.authored_water.z};studio.designer_frame_radius=std::max(32.f,q.authored_water.radius_x*1.6f);studio.designer_frame_reveal=false;studio.designer_frame_pending=true;q.phase=102;q.since=frame;
                }else if(q.phase==102&&settled&&frame-q.since>8){
                    basin(q.authored_water);
                    // The renderer counter is advanced only by the actual
                    // main-view local-water glDrawArrays call, never by the
                    // startup shader warm pass or a CPU visibility query.
                    check(studio_pond_draws[studio.live.waters.size()-2+q.water_mode]>q.water_draws_before,"fresh process draws the saved local water through the real renderer");
                    if(q.water_mode==0){q.water_mode=1;q.phase=101;q.since=frame;}
                    else{qa_landscape_passed=!q.failed;q.phase=200;std::fprintf(stderr,"[Landscape QA] REOPEN PASS %u checks; saved terrain/ecology/river/Pond/Lake, collision floors, dry rims and actual local water draws\n",q.checks);std::fflush(stderr);}
                }
            }else if(q.phase==0&&frame>45&&settled){
                studio.open=true;studio.designer_content=false;studio.designer_tools=false;studio.designer_immersive=false;studio.landscape_radius=16;studio.landscape_strength=2;studio.landscape_density=.85f;
                q.props=studio.live.placements.size();q.areas=studio.live.ecology.size();q.ribbons=studio.live.surfaces.ribbons.size();q.waters=studio.live.waters.size();q.before=bytes(studio.draft);q.since=frame;q.phase=1;
            }else if(q.phase==1&&frame-q.since>4){mouse(studio.landscape_open_screen,1);q.phase=2;}
            else if(q.phase==2&&frame-q.since>1){release();q.phase=3;}
            else if(q.phase==3&&frame-q.since>4){check(studio.tab==15&&studio.designer_tools,"real Landscape toolbar opens workspace");click(0);q.phase=4;}
            else if(q.phase==4&&frame-q.since>1){release();q.phase=5;}
            else if(q.phase==5&&settled&&frame-q.since>6){
                check(studio.target,"Find open ground chooses a real target");q.x=studio.x;q.z=studio.z;q.history=studio.history.back.size();q.applies=studio.applied;q.strokes=studio.landscape_committed_strokes;
                check(screen(q.x-7,q.z,q.a)&&screen(q.x+7,q.z,q.b),"open terrain is visible in viewport");q.old_height=world_atlas_height(q.x-7,q.z);mouse(q.a,1);q.phase=6;
            }else if(q.phase==6&&frame-q.since>3){check(studio.landscape_painting&&studio.dirty,"viewport mouse starts terrain stroke");check(studio.applied==q.applies,"stroke does not Apply mid-drag");mouse(q.b,-1);q.phase=7;}
            else if(q.phase==7&&frame-q.since>3){check(studio.landscape_brush_samples>=2,"continuous terrain drag paints multiple samples");release();q.phase=8;}
            else if(q.phase==8&&settled&&frame-q.since>5){
                check(studio.landscape_committed_strokes==q.strokes+1&&studio.applied==q.applies+1,"one mouse release commits once");check(studio.history.back.size()==std::min(size_t(32),q.history+1),"continuous stroke is one Undo");
                check(world_atlas_height(q.x-7,q.z)>q.old_height+.2,"rendered collision terrain rises");q.shaped=bytes(studio.draft);q.history=studio.history.back.size();q.applies=studio.applied;mouse(q.a,1);q.phase=9;
            }else if(q.phase==9&&frame-q.since>3){check(studio.landscape_painting,"second real stroke begins");key(SDLK_ESCAPE);q.phase=10;}
            else if(q.phase==10&&frame-q.since>3){release();q.phase=11;}
            else if(q.phase==11&&frame-q.since>4){check(!studio.landscape_painting&&bytes(studio.draft)==q.shaped,"Escape rolls back whole stroke");check(studio.history.back.size()==q.history&&studio.applied==q.applies,"cancel adds no history or Apply");q.old_pitch=cam.pitch;cam.pitch=1.35f;const auto v=studio.designer_viewport(ImGui::GetIO().DisplaySize.x,ImGui::GetIO().DisplaySize.y);mouse({float(v.x+v.w*.5),float(v.y+v.h*.4)},1);q.phase=12;}
            else if(q.phase==12&&frame-q.since>3){check(!studio.target&&!studio.landscape_painting,"sky click cannot reuse stale ground target");release();cam.pitch=q.old_pitch;key(SDLK_z,true);q.phase=13;}
            else if(q.phase==13&&frame-q.since>5){key(SDLK_s,true,true);q.phase=14;}
            else if(q.phase==14&&settled&&frame-q.since>8){check(bytes(studio.draft)==q.before,"Ctrl+Z restores complete pre-stroke landscape");key(SDLK_y,true);q.phase=15;}
            else if(q.phase==15&&frame-q.since>5){key(SDLK_s,true,true);q.phase=16;}
            else if(q.phase==16&&settled&&frame-q.since>8){
                check(bytes(studio.draft)==q.shaped,"Ctrl+Y restores complete shaped landscape");
                // Look down steeply for both held strokes. A projected point
                // on a raised hill can be hidden behind its nearer slope from
                // the original low-angle Find Open view; bounds alone do not
                // prove that the terrain picker hits the intended surface.
                q.stationary_previous_camera=cam;cam.pitch=-1.2f;
                studio.designer_frame_center={q.x-7,world_atlas_height(q.x-7,q.z),q.z};studio.designer_frame_radius=32;studio.designer_frame_reveal=false;studio.designer_frame_pending=true;
                q.stationary_mode=0;click(4);q.phase=40;
            }
            // Keep the cursor at one recorded viewport pixel. A movement-only
            // brush stamps once here, so this fails the actual regression even
            // when the original moving-drag checks above still pass. Wait on
            // wall time instead of frames: an uncapped QA run may be very fast.
            else if(q.phase==40&&frame-q.since>1){release();q.phase=41;}
            else if(q.phase==41&&settled&&frame-q.since>4){
                check(studio.landscape_page==0&&studio.landscape_shape==int(q.stationary_mode),q.stationary_mode?"real Lower button selects lowering":"real Raise button selects raising");
                q.stationary_before=bytes(studio.draft);q.history=studio.history.back.size();q.applies=studio.applied;q.strokes=studio.landscape_committed_strokes;
                q.old_height=world_atlas_height(q.x-7,q.z);q.stationary_walk_before=world_atlas_walk_height(q.x-7,q.z);
                if(check(screen(q.x-7,q.z,q.a),"stationary sculpt point is visible")){mouse(q.a,1);q.phase=42;}
            }else if(q.phase==42&&frame-q.since>3){
                check(studio.landscape_painting&&studio.dirty,"stationary mouse hold starts a real terrain stroke");
                check(studio.applied==q.applies,"stationary stroke does not Apply before release");
                check(studio.target&&std::hypot(studio.x-(q.x-7),studio.z-q.z)<1,"stationary brush picks the intended visible terrain within one metre");
                q.stationary_pick_x=studio.x;q.stationary_pick_z=studio.z;
                // Freeze the actual hit once; all later height assertions use
                // this same world point, including after rebuilding and Undo.
                q.old_height=world_atlas_height(q.stationary_pick_x,q.stationary_pick_z);q.stationary_walk_before=world_atlas_walk_height(q.stationary_pick_x,q.stationary_pick_z);
                q.stationary_samples=studio.landscape_brush_samples;q.stationary_first_delta=world_studio::height_delta(studio.draft,q.stationary_pick_x,q.stationary_pick_z);
                q.stationary_since=SDL_GetTicks64();q.phase=43;
            }else if(q.phase==43&&SDL_GetTicks64()-q.stationary_since>=1000){
                const auto &io=ImGui::GetIO();const double direction=q.stationary_mode?-1.:1.;
                check(studio.landscape_painting&&io.MouseDown[0]&&std::abs(io.MousePos.x-q.requested.x)<1&&std::abs(io.MousePos.y-q.requested.y)<1,"same viewport pixel stays held without mouse movement");
                check(studio.target&&std::hypot(studio.x-q.stationary_pick_x,studio.z-q.stationary_pick_z)<.1,"stationary cursor keeps the same actual terrain hit");
                check(studio.landscape_brush_samples>=q.stationary_samples+3,q.stationary_mode?"stationary Lower repeats after its first stamp":"stationary Raise repeats after its first stamp");
                check(direction*(world_studio::height_delta(studio.draft,q.stationary_pick_x,q.stationary_pick_z)-q.stationary_first_delta)>.2,"held brush keeps changing actual draft height");
                check(studio.applied==q.applies&&studio.history.back.size()==q.history,"held brush leaves live Apply and Undo untouched until release");
                std::fprintf(stderr,"[Landscape QA] Stationary %s held %llu ms at pixel (%.1f,%.1f), fixed actual terrain (%.6f,%.6f): %u additional stamps; draft delta %.3f -> %.3f\n",q.stationary_mode?"Lower":"Raise",(unsigned long long)(SDL_GetTicks64()-q.stationary_since),q.requested.x,q.requested.y,q.stationary_pick_x,q.stationary_pick_z,studio.landscape_brush_samples-q.stationary_samples,q.stationary_first_delta,double(world_studio::height_delta(studio.draft,q.stationary_pick_x,q.stationary_pick_z)));std::fflush(stderr);
                release();q.phase=44;
            }else if(q.phase==44&&settled&&frame-q.since>5){
                const double direction=q.stationary_mode?-1.:1.;
                check(studio.landscape_committed_strokes==q.strokes+1&&studio.applied==q.applies+1,"stationary mouse release commits exactly once");
                check(studio.history.back.size()==std::min(size_t(32),q.history+1),"stationary hold is exactly one Undo transaction");
                q.stationary_height_after=world_atlas_height(q.stationary_pick_x,q.stationary_pick_z);q.stationary_walk_after=world_atlas_walk_height(q.stationary_pick_x,q.stationary_pick_z);
                check(direction*(q.stationary_height_after-q.old_height)>.2,q.stationary_mode?"stationary Lower lowers committed rendered terrain":"stationary Raise raises committed rendered terrain");
                check(direction*(q.stationary_walk_after-q.stationary_walk_before)>.2,q.stationary_mode?"stationary Lower lowers actual walking collision floor":"stationary Raise raises actual walking collision floor");
                q.stationary_after=bytes(studio.draft);key(SDLK_z,true);q.phase=45;
            }else if(q.phase==45&&frame-q.since>5){key(SDLK_s,true,true);q.phase=46;}
            else if(q.phase==46&&settled&&frame-q.since>8){
                check(bytes(studio.draft)==q.stationary_before,"one Undo restores the complete pre-hold landscape");
                check(std::abs(world_atlas_height(q.stationary_pick_x,q.stationary_pick_z)-q.old_height)<.02&&std::abs(world_atlas_walk_height(q.stationary_pick_x,q.stationary_pick_z)-q.stationary_walk_before)<.02,"one Undo restores rendered terrain and walking collision height");
                key(SDLK_y,true);q.phase=47;
            }else if(q.phase==47&&frame-q.since>5){key(SDLK_s,true,true);q.phase=48;}
            else if(q.phase==48&&settled&&frame-q.since>8){
                check(bytes(studio.draft)==q.stationary_after,"one Redo restores the complete held stroke");
                check(std::abs(world_atlas_height(q.stationary_pick_x,q.stationary_pick_z)-q.stationary_height_after)<.02&&std::abs(world_atlas_walk_height(q.stationary_pick_x,q.stationary_pick_z)-q.stationary_walk_after)<.02,"one Redo restores rendered terrain and walking collision height");
                if(q.stationary_mode==0){q.stationary_mode=1;click(5);q.phase=40;}else{key(SDLK_z,true);q.phase=49;}
            }else if(q.phase==49&&frame-q.since>5){key(SDLK_z,true);q.phase=50;}
            else if(q.phase==50&&frame-q.since>5){key(SDLK_s,true,true);q.phase=51;}
            else if(q.phase==51&&settled&&frame-q.since>8){
                // Restore the original moving-stroke geometry before the
                // unchanged forest and downhill-river regression sequence.
                check(bytes(studio.draft)==q.shaped,"stationary regression leaves the original river fixture intact");cam=q.stationary_previous_camera;click(2);q.phase=17;
            }
            else if(q.phase==17&&frame-q.since>1){release();q.phase=18;}
            else if(q.phase==18&&frame-q.since>4){check(studio.landscape_page==1,"real Plant tab opens");studio.landscape_radius=32;click(4);q.phase=19;}
            else if(q.phase==19&&frame-q.since>1){release();q.phase=20;}
            else if(q.phase==20&&frame-q.since>4){check(studio.landscape_plant==0,"real Forest tool selected");check(screen(q.x,q.z,q.a),"forest ground visible");mouse(q.a,1);q.phase=21;}
            else if(q.phase==21&&frame-q.since>3){release();q.phase=22;}
            else if(q.phase==22&&settled&&frame-q.since>12&&dressing.tree_count>0&&dressing.scanned_grass_count>0){
                check(studio.live.ecology.size()==q.areas+1,"forest stroke persists an ecology region");check(studio.live.placements.size()==q.props,"bulk forest spends no object placements");check(dressing.cover!=nullptr&&dressing.tree_count>0&&dressing.scanned_grass_count>0,"real tree and scanned grass roots render");click(3);q.phase=23;
            }else if(q.phase==23&&frame-q.since>1){release();q.phase=24;}
            else if(q.phase==24&&frame-q.since>4){
                check(studio.landscape_page==2,"real River tab opens");studio.landscape_river_width=8;studio.landscape_river_bank=4;studio.landscape_river_depth=1.2f;
                // Pick a safe downhill route; validation is only a fixture
                // preflight. The actual document is changed solely by clicks.
                bool found=false;auto source=studio.landscape_river_source();
                for(unsigned i=0;i<16&&!found;++i){double angle=i*6.283185307179586/16,dx=std::cos(angle)*8,dz=std::sin(angle)*8;
                    std::vector<world_terrain_authoring::Point> points={{q.x-dx,q.z-dz,world_atlas_height(q.x-dx,q.z-dz)},{q.x+dx,q.z+dz,world_atlas_height(q.x+dx,q.z+dz)}};
                    if(points[1].height>points[0].height)std::swap(points[0],points[1]);
                    auto draft=studio.draft;world_terrain_authoring::Report report;std::string error;
                    if(studio.project_point(cam,{points[0].x,points[0].height,points[0].z},q.a)&&studio.project_point(cam,{points[1].x,points[1].height,points[1].z},q.b)&&world_terrain_authoring::simple_river(draft,points,8,4,1.2f,source,report,error)){q.river=points;found=true;}}
                if(check(found,"open ground supports a bounded downhill river")){mouse(q.a,1);q.phase=25;}
            }else if(q.phase==25&&frame-q.since>1){release();q.phase=26;}
            else if(q.phase==26&&frame-q.since>4){check(studio.landscape_river_points.size()==1,"first viewport river point");mouse(q.b,1);q.phase=27;}
            else if(q.phase==27&&frame-q.since>1){release();q.phase=28;}
            else if(q.phase==28&&frame-q.since>4){check(studio.landscape_river_points.size()==2,"second viewport river point");click(13);q.phase=29;}
            else if(q.phase==29&&frame-q.since>1){release();q.phase=30;}
            else if(q.phase==30&&settled&&frame-q.since>8){
                check(studio.live.surfaces.ribbons.size()==q.ribbons+1&&!studio_surface_gpu.rivers.ranges.empty(),"Finish river commits real water mesh");
                const auto &r=studio.live.surfaces.ribbons.back();double x=(r.points[0].x+r.points[1].x)*.5,z=(r.points[0].z+r.points[1].z)*.5;float level=0,shore=0;
                check(studio.live.surface_field&&world_surface::river(*studio.live.surface_field,x,z,level,shore),"committed river has sampled water");check(world_atlas_height(x,z)<level&&world_atlas_local_water(x,z,nullptr,nullptr),"river bed and collision sit below flowing surface");
                check(studio_surface_gpu.rivers.visible(cam.x,cam.z),"new river mesh is in visible local range");check(studio_river_draws>0,"new river issues an actual main-view water draw");key(SDLK_s,true);q.phase=31;
            }else if(q.phase==31&&settled&&frame-q.since>12){
                world_studio::Document loaded;std::string error;check(world_studio::load(studio.path.c_str(),loaded,error)&&loaded.ecology.size()==q.areas+1&&loaded.terrain.size()==studio.live.terrain.size()&&loaded.surfaces.ribbons.size()==q.ribbons+1,"real Ctrl+S persists full landscape");
                check(studio.rejected==0&&studio.landscape_cancelled_strokes>0,"all accepted edits and explicit cancellation accounted");
                // Keep all original river/save checks above before extending
                // the fixture. The new water bodies are made only by clicks.
                if(check(q.waters+2<=8,"landscape fixture has room for both authored water bodies")){q.water_mode=0;click(14);q.phase=60;}
            }else if(q.phase==60&&frame-q.since>1){release();q.phase=61;}
            else if(q.phase==61&&frame-q.since>4){
                check(studio.landscape_page==2&&studio.landscape_water_kind==int(q.water_mode)&&studio.landscape_water_armed,q.water_mode?"real Lake button arms lake placement":"real Pond button arms pond placement");
                check(std::abs(studio.landscape_water_radius-(q.water_mode?24.f:12.f))<.01,"water preset has the expected Pond or Lake radius");
                check(std::abs(studio.landscape_water_depth-(q.water_mode?3.f:1.f))<.01,"water preset has shallow Pond or unchanged Lake depth");click(0);q.phase=62;
            }else if(q.phase==62&&frame-q.since>1){release();q.phase=63;}
            else if(q.phase==63&&settled&&frame-q.since>6){
                check(studio.target,"water Find open ground chooses a real clear target");q.x=studio.x;q.z=studio.z;
                cam.pitch=-1.2f;studio.designer_frame_center={q.x,world_atlas_height(q.x,q.z),q.z};studio.designer_frame_radius=std::max(32.f,studio.landscape_water_radius*1.6f);studio.designer_frame_reveal=false;studio.designer_frame_pending=true;q.phase=64;q.since=frame;
            }else if(q.phase==64&&settled&&frame-q.since>6){
                q.water_before_document=studio.draft;q.water_before=bytes(studio.draft);q.water_history=studio.history.back.size();q.water_applies=studio.applied;q.water_creations=studio.landscape_ponds_created;q.water_draws_before=studio_pond_draws[q.waters+q.water_mode];
                q.water_old_height=world_atlas_height(q.x,q.z);q.water_old_walk=world_atlas_walk_height(q.x,q.z);
                if(check(screen(q.x,q.z,q.a),"clear basin centre is visible in the viewport")){mouse(q.a,1);q.phase=65;}
            }else if(q.phase==65&&frame-q.since>12){release();q.phase=66;}
            else if(q.phase==66&&settled&&frame-q.since>8){
                if(check(studio.live.waters.size()==q.waters+q.water_mode+1,q.water_mode?"one real viewport click creates one Lake":"one held viewport click creates one Pond")){
                    q.authored_water=studio.live.waters.back();
                    check(std::hypot(q.authored_water.x-q.x,q.authored_water.z-q.z)<1,"created water uses the actual visible ground pick");
                    check(std::abs(q.authored_water.radius_x-(q.water_mode?24.f:12.f))<.01&&std::abs(q.authored_water.radius_z-q.authored_water.radius_x)<.01,"created water preserves its preset radius");
                    check(std::abs(q.authored_water.basin_depth-(q.water_mode?3.f:1.f))<.01,"actual viewport creation persists shallow Pond or unchanged Lake depth recipe");
                    check(studio.applied==q.water_applies+1&&studio.landscape_ponds_created==q.water_creations+1,"basin and water publish in one Apply without repeating while held");
                    check(studio.history.back.size()==std::min(size_t(32),q.water_history+1),"basin and water add exactly one Undo transaction");
                    check(studio_pond_draws[studio.live.waters.size()-1]>q.water_draws_before,"created local water issues a real main-view draw");basin(q.authored_water);
                    q.water_height_after=world_atlas_height(q.authored_water.x,q.authored_water.z);q.water_walk_after=world_atlas_walk_height(q.authored_water.x,q.authored_water.z);q.water_after=bytes(studio.draft);q.water_after_id=studio.draft.entities.next_id;
                    std::fprintf(stderr,"[Landscape QA] %s made by actual viewport click: centre (%.6f,%.6f), radius %.2f, depth %.2f, level %.3f, terrain %.3f, walk %.3f, draws %llu\n",q.water_mode?"Lake":"Pond",q.authored_water.x,q.authored_water.z,q.authored_water.radius_x,q.authored_water.basin_depth,q.authored_water.level,q.water_height_after,q.water_walk_after,(unsigned long long)(studio_pond_draws[studio.live.waters.size()-1]-q.water_draws_before));std::fflush(stderr);
                    key(SDLK_z,true);q.phase=67;
                }
            }else if(q.phase==67&&frame-q.since>5){key(SDLK_s,true,true);q.phase=68;}
            else if(q.phase==68&&settled&&frame-q.since>8){
                // Undo deliberately retains issued entity IDs so a deleted
                // water ID is never reused. Compare every authored field after
                // advancing ONLY the expected allocator high-water mark; all
                // entity nodes, terrain samples, masks and water stay exact.
                auto expected=q.water_before_document;
                check(studio.draft.entities.next_id==q.water_after_id&&studio.draft.entities.next_id>=expected.entities.next_id,"water Undo preserves exactly the issued entity ID high-water mark");
                expected.entities.next_id=studio.draft.entities.next_id;
                const auto restored=bytes(studio.draft),normalized=bytes(expected);
                if(restored!=normalized||studio.live.waters.size()!=q.waters+q.water_mode){
                    // QA-only /tmp evidence retains both raw states and the
                    // single-field normalization for a precise failure diff.
                    const std::string stem=studio.path+".water-undo-"+std::to_string(q.water_mode);
                    std::ofstream(stem+"-before.studio")<<q.water_before;
                    std::ofstream(stem+"-expected.studio")<<normalized;
                    std::ofstream(stem+"-actual.studio")<<restored;
                    std::fprintf(stderr,"[Landscape QA] Water Undo diagnostic %s: waters %zu expected %zu; ID before %llu after %llu restored %llu\n",stem.c_str(),studio.live.waters.size(),q.waters+q.water_mode,(unsigned long long)q.water_before_document.entities.next_id,(unsigned long long)q.water_after_id,(unsigned long long)studio.draft.entities.next_id);std::fflush(stderr);
                }
                check(restored==normalized&&studio.live.waters.size()==q.waters+q.water_mode,"one Undo restores the complete pre-basin document and removes its water");
                check(std::abs(world_atlas_height(q.x,q.z)-q.water_old_height)<.02&&std::abs(world_atlas_walk_height(q.x,q.z)-q.water_old_walk)<.02,"one Undo restores rendered terrain and walking collision before the basin");key(SDLK_y,true);q.phase=69;
            }else if(q.phase==69&&frame-q.since>5){key(SDLK_s,true,true);q.phase=70;}
            else if(q.phase==70&&settled&&frame-q.since>8){
                check(bytes(studio.draft)==q.water_after&&studio.live.waters.size()==q.waters+q.water_mode+1,"one Redo restores the complete basin and its water together");
                check(std::abs(world_atlas_height(q.authored_water.x,q.authored_water.z)-q.water_height_after)<.02&&std::abs(world_atlas_walk_height(q.authored_water.x,q.authored_water.z)-q.water_walk_after)<.02,"one Redo restores the submerged rendered and collision floor");
                if(q.water_mode==0){
                    q.water_history=studio.history.back.size();q.water_applies=studio.applied;
                    if(check(screen(q.authored_water.x,q.authored_water.z,q.a),"existing Pond is visible for the protected overlap check")){mouse(q.a,1);q.phase=71;}
                }else{key(SDLK_s,true);q.phase=75;}
            }else if(q.phase==71&&frame-q.since>3){release();q.phase=72;}
            else if(q.phase==72&&settled&&frame-q.since>5){
                check(bytes(studio.draft)==q.water_after&&studio.live.waters.size()==q.waters+1,"clicking an existing protected Pond changes neither basin nor water objects");
                check(studio.history.back.size()==q.water_history&&studio.applied==q.water_applies,"blocked overlap adds no Undo or Apply");key(SDLK_ESCAPE);q.phase=73;
            }else if(q.phase==73&&frame-q.since>4){
                check(!studio.landscape_water_armed&&bytes(studio.draft)==q.water_after,"Escape disarms water placement without changing the document");q.water_mode=1;click(15);q.phase=60;
            }else if(q.phase==75&&settled&&frame-q.since>12){
                world_studio::Document loaded;std::string error;
                check(world_studio::load(studio.path.c_str(),loaded,error)&&loaded.waters.size()==q.waters+2&&loaded.terrain.size()==studio.live.terrain.size()&&loaded.surfaces.ribbons.size()==q.ribbons+1&&loaded.ecology.size()==q.areas+1,"real Ctrl+S persists Pond, Lake and every original landscape feature");
                check(bytes(loaded)==bytes(studio.live),"saved landscape reload exactly preserves authored water and basin state");
                check(studio.rejected==0,"accepted Pond and Lake edits trigger no scene build rejection");qa_landscape_passed=!q.failed;q.phase=200;
                std::fprintf(stderr,"[Landscape QA] PASS %u checks; original terrain/forest/river regression plus actual Pond/Lake clicks, main-view water draws, centre/rim collision checks, atomic Undo/Redo, protected overlap, Escape and Save/reload\n",q.checks);std::fflush(stderr);
            }
            }
            qa_landscape_phase=q.phase;qa_landscape_checks=q.checks;
            if(q.phase!=200&&!q.failed&&frame>3800){check(false,"landscape fixture completed before bounded deadline");q.phase=999;qa_landscape_phase=q.phase;}
        }

        // Real inspector input against a complete disposable project; no draft
        // setter substitutes for the user's transform, history or save actions.
        if(qa_structure){
            ImGui::GetIO().IniFilename=nullptr;
            struct StructureQA {unsigned phase=0,checks=0,input=0,source=0;std::uint64_t since=0,input_since=0,original_fp=0,moved_fp=0;bool failed=false,initialized=false,camera_sent=false,camera_wheel=false,camera_checked=false;double target=0,original_x=0,camera_x=0,camera_y=0,camera_z=0;unsigned dolly=0;ImVec2 mouse;std::array<world_physics::Vec3,3> face;world_structure_collision::Capture original_source;};static StructureQA q;
            auto check=[&](bool ok,const char *label){if(q.failed)return false;++q.checks;if(!ok){std::fprintf(stderr,"[Structure QA] FAIL phase%u input%u: %s | %s\n",q.phase,q.input,label,studio.message.c_str());
                const auto &p=structure_wheel_probe;int mx=0,my=0;const auto buttons=SDL_GetMouseState(&mx,&my);const auto &io=ImGui::GetIO();
                std::fprintf(stderr,"[Structure QA] Input diagnostics: frame=%llu cook=%d wheel_events=%u raw=(%d,%d) normalized=(%d,%d) buttons=%u eligible=%d capture=%d text=%d active=%d focus=%d app_focus=%d inside=%d current_raw=(%d,%d) current_buttons=%u imgui=(%.1f,%.1f) current_capture=%d current_text=%d current_active=%d camera=(%.6f,%.6f,%.6f) camera_start=(%.6f,%.6f,%.6f) dolly=%u/%u\n",
                    (unsigned long long)frame,int(studio.structure_cooking),p.events,p.raw_x,p.raw_y,p.x,p.y,p.buttons,int(p.eligible),int(p.capture),int(p.text),int(p.active),int(p.focus),int(p.app_focus),int(p.inside),mx,my,buttons,io.MousePos.x,io.MousePos.y,int(editor_ui_wants_mouse(&ui)),int(io.WantTextInput),int(ImGui::IsAnyItemActive()),cam.x,cam.y,cam.z,q.camera_x,q.camera_y,q.camera_z,editor_dolly_events,q.dolly);
                std::fflush(stderr);q.failed=true;launch->exit_code=1;running=false;}return ok;};
            auto hover=[&](ImVec2 p){if(q.failed)return;q.mouse=p;SDL_WarpMouseInWindow(window,int(p.x),int(p.y));SDL_PumpEvents();SDL_Event e{};e.type=SDL_MOUSEMOTION;e.motion.windowID=SDL_GetWindowID(window);e.motion.which=0x56414c54u;e.motion.x=int(p.x);e.motion.y=int(p.y);SDL_PushEvent(&e);ImGui::GetIO().AddMousePosEvent(p.x,p.y);};
            auto key=[&](SDL_Keycode code,bool ctrl=false,bool shift=false){if(q.failed)return;SDL_Event e{};e.type=SDL_KEYDOWN;e.key.windowID=SDL_GetWindowID(window);e.key.state=SDL_PRESSED;e.key.keysym.sym=code;e.key.keysym.mod=SDL_Keymod((ctrl?KMOD_CTRL:0)|(shift?KMOD_SHIFT:0));e.key.keysym.unused=0x56414c54u;check(SDL_PushEvent(&e)==1,"actual tagged keyboard press");e.type=SDL_KEYUP;e.key.state=SDL_RELEASED;e.key.keysym.mod=KMOD_NONE;SDL_PushEvent(&e);};
            auto button=[&](bool down,bool ctrl){if(q.failed)return;SDL_Event e{};e.type=down?SDL_KEYDOWN:SDL_KEYUP;e.key.windowID=SDL_GetWindowID(window);e.key.state=down?SDL_PRESSED:SDL_RELEASED;e.key.keysym.sym=SDLK_LCTRL;e.key.keysym.mod=down&&ctrl?KMOD_CTRL:KMOD_NONE;e.key.keysym.unused=0x56414c54u;SDL_PushEvent(&e);ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl,down&&ctrl);
                e={};e.type=down?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;e.button.windowID=SDL_GetWindowID(window);e.button.which=0x56414c54u;e.button.button=SDL_BUTTON_LEFT;e.button.state=down?SDL_PRESSED:SDL_RELEASED;e.button.x=int(q.mouse.x);e.button.y=int(q.mouse.y);SDL_PushEvent(&e);ImGui::GetIO().AddMouseButtonEvent(0,down);};
            auto pose=[&](WorldSceneryPlacement &p){return q.source<studio.applied_records.size()&&studio.applied_records[q.source]!=SIZE_MAX&&world_scenery_placement(scenery,studio.applied_records[q.source],&p);};
            auto face_hit=[&](const WorldSceneryPlacement &p){const world_entity::Transform t{p.x,p.y,p.z,p.yaw*57.29577951308232,p.scale,p.pitch*57.29577951308232,p.roll*57.29577951308232,p.sx,p.sy,p.sz};const auto matrix=vw_transform::matrix(t);std::array<vw_transform::V,3> v;
                for(unsigned i=0;i<3;++i){v[i]=vw_transform::point(matrix,{q.face[i].x,q.face[i].y,q.face[i].z});}
                vw_transform::V c{},a{},b{};for(unsigned k=0;k<3;++k){c[k]=(v[0][k]+v[1][k]+v[2][k])/3;a[k]=v[1][k]-v[0][k];b[k]=v[2][k]-v[0][k];}vw_transform::V n{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};const auto length=std::hypot(std::hypot(n[0],n[1]),n[2]);if(length<1e-6)return false;for(auto &x:n)x/=length;world_physics::Hit hit;return active_playtest_walk&&active_playtest_walk->raycast({c[0]+n[0]*.2,c[1]+n[1]*.2,c[2]+n[2]*.2},{-n[0]*.4,-n[1]*.4,-n[2]*.4},hit)&&hit.hit&&std::abs(hit.fraction-.5)<.025;};
            auto publication=[&](double expected){WorldSceneryPlacement p;if(!check(pose(p)&&std::abs(p.x-expected)<1e-4,"committed renderer has exact edited architecture pose"))return false;if(!check(face_hit(p),"committed collision matches actual transformed source face"))return false;world_structure_collision::Capture capture;std::string error;return check(world_structure_collision::capture(studio.project_directory,scenery,studio.live,studio.structure_mode(),capture,error)&&capture.fingerprint==structure_live_fingerprint,"source collision fingerprint exactly matches actual rendered scene");};
            auto restored_fingerprint=[&](){
                if(structure_live_fingerprint==q.original_fp)return true;
                world_structure_collision::Capture now;std::string error;
                if(world_structure_collision::capture(studio.project_directory,scenery,studio.live,studio.structure_mode(),now,error)){
                    std::fprintf(stderr,"[Structure QA] Undo source mismatch original=%016llx restored=%016llx instances=%zu/%zu\n",(unsigned long long)q.original_fp,(unsigned long long)structure_live_fingerprint,q.original_source.instances.size(),now.instances.size());
                    unsigned differences=0;for(std::size_t i=0;i<std::min(q.original_source.instances.size(),now.instances.size())&&differences<8;++i){const auto &a=q.original_source.instances[i],&b=now.instances[i];
                        const double av[]={a.pose.x,a.pose.y,a.pose.z,a.pose.yaw,a.pose.scale,a.pose.pitch,a.pose.roll,a.pose.sx,a.pose.sy,a.pose.sz};
                        const double bv[]={b.pose.x,b.pose.y,b.pose.z,b.pose.yaw,b.pose.scale,b.pose.pitch,b.pose.roll,b.pose.sx,b.pose.sy,b.pose.sz};
                        if(a.alias==b.alias&&a.model==b.model&&!std::memcmp(av,bv,sizeof av))continue;
                        ++differences;std::fprintf(stderr,"[Structure QA] Undo instance%zu original=%s restored=%s\n",i,a.alias.c_str(),b.alias.c_str());
                        static const char *names[]={"x","y","z","yaw","scale","pitch","roll","sx","sy","sz"};
                        for(unsigned k=0;k<10;++k)if(std::memcmp(&av[k],&bv[k],sizeof(double)))std::fprintf(stderr,"[Structure QA]   %s original=%.17g (%a) restored=%.17g (%a)\n",names[k],av[k],av[k],bv[k],bv[k]);
                    }
                    std::fflush(stderr);
                }
                return false;
            };
            const bool ready=!destination_loading&&terrain.view_ready(cam.x,cam.z,cam.fly)&&!studio.apply&&!studio.structure_cooking&&!studio.designer_frame_pending&&!studio.project_save_job.valid()&&!studio.project_save_requested&&!studio.production_freeze();
            const bool focus=(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)!=0&&!ImGui::GetIO().AppFocusLost;
            if(!q.initialized){q.initialized=true;const auto project=std::filesystem::absolute(studio.project_directory).lexically_normal();check(project.generic_string().find("/build/collision-water-")!=std::string::npos&&project.filename().string().find("qa-project")==0&&std::filesystem::absolute(studio.path).lexically_normal().parent_path()==project&&!std::getenv("VALWOOD_QA_STUDIO_DOCUMENT"),"full disposable structural QA project only");}
            if(!q.failed&&q.phase!=200){
                if(!focus){q.since=q.input_since=frame;SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_PumpEvents();}
                else if(q.input&&frame-q.input_since>3){
                    switch(q.input){
                    case 1:check(studio.structure_move_field.x>0,"real East transform field visible");hover(studio.structure_move_field);q.input=2;break;
                    case 2:button(true,true);q.input=3;break;
                    case 3:button(false,false);q.input=4;break;
                    case 4:check(ImGui::IsAnyItemActive(),"Ctrl-click opens real scalar text entry");key(SDLK_a,true);q.input=5;break;
                    case 5:{SDL_Event e{};e.type=SDL_TEXTINPUT;e.text.windowID=SDL_GetWindowID(window);std::snprintf(e.text.text,sizeof e.text.text,"%.8f",q.target);check(SDL_PushEvent(&e)==1,"actual scalar text replacement");q.input=6;break;}
                    case 6:key(SDLK_RETURN);q.input=7;break;
                    case 7:q.input=0;q.since=frame;break;
                    }q.input_since=frame;
                }else if(!q.input){
                    if(studio.structure_cooking&&!q.camera_sent){
                        const auto v=studio.designer_viewport(ImGui::GetIO().DisplaySize.x,ImGui::GetIO().DisplaySize.y);hover({v.x+v.w*.5f,v.y+v.h*.5f});q.camera_x=cam.x;q.camera_y=cam.y;q.camera_z=cam.z;q.dolly=editor_dolly_events;
                        q.camera_sent=true;q.since=frame;
                    }
                    if(q.camera_sent&&!q.camera_wheel&&frame-q.since>3){
                        SDL_Event e{};e.type=SDL_MOUSEWHEEL;e.wheel.windowID=SDL_GetWindowID(window);e.wheel.which=0x56414c54u;e.wheel.y=1;
#if SDL_VERSION_ATLEAST(2,0,18)
                        e.wheel.preciseY=1;
#endif
                        SDL_PushEvent(&e);q.camera_wheel=true;q.since=frame;
                    }
                    if(q.camera_wheel&&!q.camera_checked&&frame-q.since>3){check(editor_dolly_events==q.dolly+1&&std::hypot(std::hypot(cam.x-q.camera_x,cam.y-q.camera_y),cam.z-q.camera_z)>.01,"actual viewport camera remains responsive during detached collision preparation");q.camera_checked=true;}
                    if(ready&&frame-q.since>7)switch(q.phase){
                    case 0:if(frame>55){
                        check(active_structure_world&&structure_support&&active_playtest_walk==structure_support.get(),"full source collision is active");studio.open=true;studio.designer_immersive=false;studio.designer_content=false;studio.designer_tools=false;studio.designer_layout.right=430;studio.tab=studio.requested_tab=2;look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);
                        bool found=false;double closest=1e30;const bool reopen=std::getenv("VALWOOD_QA_STRUCTURE_REOPEN")!=nullptr;
                        if(reopen){for(const auto &o:studio.draft.overrides)if(o.index<studio.source_poses.size()&&world_editor_scene_guard::protected_alias(studio.structure_mode(),studio.source_poses[o.index].alias)&&!world_editor_scene_guard::immutable_ground_alias(studio.source_poses[o.index].alias)){q.source=o.index;found=true;break;}}
                        else for(unsigned i=0;i<studio.source_poses.size();++i){const auto &p=studio.source_poses[i];const bool eligible=city_showcase?p.alias.find("_sm_boundary_gate_01")!=std::string::npos:p.alias.rfind("nm_scene_built_",0)==0;const double distance=std::hypot(p.root.x-cam.x,p.root.z-cam.z);if(eligible&&distance<closest){q.source=i;closest=distance;found=true;}}
                        check(found,"find actual editable source architecture");if(!found)break;
                        check(studio.designer_select(world_designer::Ref::source_model(q.source)),"select real source through production selection");check(!studio.structure_selection_protected(),"architecture supports normal editor transforms");studio.designer_frame_selection();q.original_x=studio.source_poses[q.source].root.x;q.target=q.original_x+80;q.original_fp=structure_live_fingerprint;
                        world_structure_collision::Catalog catalog;std::string error;
                        if(!check(world_structure_collision::capture(studio.project_directory,scenery,studio.live,studio.structure_mode(),q.original_source,error)&&q.original_source.fingerprint==q.original_fp,"retain exact original source capture for Undo"))break;check(world_structure_collision::load_catalog(studio.project_directory,catalog,error),"load checksummed source collision metadata");const auto binding=catalog.find(studio.source_poses[q.source].alias);check(binding!=catalog.end()&&!binding->second.parts.empty(),"selected source has exact UCX/triangle geometry");
                        if(binding!=catalog.end()&&!binding->second.parts.empty()){world_structure_collision::Part part;check(world_structure_collision::load_part(studio.project_directory,binding->second.parts.front(),part,error),"read real source collision part");WorldSceneryPlacement p;check(pose(p),"selected source is a committed render record");bool matched=false;for(const auto &t:part.triangles){for(unsigned i=0;i<3;++i)q.face[i]=part.vertices[t[i]];if(face_hit(p)){matched=true;break;}}check(matched,"find a real source face visible to actual Jolt ray query");}
                        if(reopen){if(!publication(q.target)||!check(!studio.project_has_unsaved(),"fresh process opens saved transformed architecture"))break;q.phase=20;}else{q.phase=1;q.input=1;q.input_since=frame;}q.since=frame;
                    }break;
                    case 1:if(!publication(q.target)||!check(structure_async_started>0&&structure_async_commits>0,"architecture reached asynchronous source collision publication"))break;q.moved_fp=structure_live_fingerprint;if(!check(q.moved_fp!=q.original_fp,"source collision identity changes with rendered transform"))break;key(SDLK_z,true);if(q.failed)break;q.phase=2;q.since=frame;break;
                    case 2:if(!publication(q.original_x)||!check(restored_fingerprint(),"Undo restores exact source collision fingerprint"))break;key(SDLK_y,true);if(q.failed)break;q.phase=3;q.since=frame;break;
                    case 3:if(!publication(q.target)||!check(structure_live_fingerprint==q.moved_fp,"Redo restores exact matching collision"))break;key(SDLK_s,true,true);if(q.failed)break;q.phase=4;q.since=frame;break;
                    case 4:if(studio.project_save_failed)check(false,"coordinated Save All succeeds");else if(studio.project_saved_together&&!studio.project_has_unsaved()){world_studio::Document disk;std::string error,a,b;check(world_studio::load(studio.path.c_str(),disk,error)&&world_studio::encode(disk,a,error)&&world_studio::encode(studio.live,b,error)&&a==b,"Save All persists exact live architectural source transforms");q.phase=20;q.since=frame;}break;
                    case 20:if(frame-q.since>12){if(!publication(q.target)||!check(!studio.project_has_unsaved(),"completed scene is saved"))break;q.phase=200;qa_structure_passed=true;std::fprintf(stderr,"[Structure QA] PASS %u checks; actual scalar edit, async publication, camera input, Undo/Redo, Save All/reopen\n",q.checks);std::fflush(stderr);}break;
                    }
                }
            }
            if(q.phase!=200&&!q.failed&&(frame>2200||frame+2>=std::uint64_t(launch->frames))){check(false,"structural workflow completes inside bounded replay");q.phase=999;}
            qa_structure_checks=q.checks;qa_structure_phase=q.phase;
        }

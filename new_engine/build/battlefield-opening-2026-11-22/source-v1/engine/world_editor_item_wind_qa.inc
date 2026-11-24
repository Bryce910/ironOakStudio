// Explicit full disposable-project integration. No user scene is eligible.
        if(qa_item_wind){
            ImGui::GetIO().IniFilename=nullptr;
            struct ItemWindQA {unsigned phase=0,checks=0,source=0,applies=0;std::uint64_t since=0;size_t history=0,placements=0;bool failed=false,initialized=false;std::string before,off;world_studio::Document original,disabled_snapshot;};static ItemWindQA q;
            auto check=[&](bool good,const char *label){++q.checks;if(!good){std::fprintf(stderr,"[Item wind QA] FAIL phase %u: %s | %s\n",q.phase,label,studio.message.c_str());std::fflush(stderr);q.failed=true;launch->exit_code=1;running=false;}return good;};
            auto bytes=[&](const world_studio::Document &d){std::string text,error;check(world_studio::encode(d,text,error),"encode actual scene");return text;};
            auto key=[&](SDL_Keycode code,bool ctrl=false,bool shift=false){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.windowID=SDL_GetWindowID(window);e.key.state=SDL_PRESSED;e.key.keysym.sym=code;e.key.keysym.mod=SDL_Keymod((ctrl?KMOD_CTRL:0)|(shift?KMOD_SHIFT:0));e.key.keysym.unused=0x56414c54u;check(SDL_PushEvent(&e)==1,"tagged actual shortcut press");e.type=SDL_KEYUP;e.key.state=SDL_RELEASED;e.key.keysym.mod=KMOD_NONE;check(SDL_PushEvent(&e)==1,"tagged actual shortcut release");q.since=frame;};
            auto hover=[&](ImVec2 p){SDL_WarpMouseInWindow(window,int(p.x),int(p.y));SDL_PumpEvents();SDL_Event e{};e.type=SDL_MOUSEMOTION;e.motion.windowID=SDL_GetWindowID(window);e.motion.which=0x56414c54u;e.motion.x=int(p.x);e.motion.y=int(p.y);SDL_PushEvent(&e);ImGui::GetIO().AddMousePosEvent(p.x,p.y);q.since=frame;};
            auto viewport=[&](){const auto v=studio.designer_viewport(ImGui::GetIO().DisplaySize.x,ImGui::GetIO().DisplaySize.y);hover({v.x+v.w*.5f,v.y+v.h*.5f});};
            auto button=[&](bool down){const auto p=studio.designer_wind_enable_point;SDL_Event e{};e.type=down?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;e.button.windowID=SDL_GetWindowID(window);e.button.which=0x56414c54u;e.button.button=SDL_BUTTON_LEFT;e.button.state=down?SDL_PRESSED:SDL_RELEASED;e.button.x=int(p.x);e.button.y=int(p.y);check(SDL_PushEvent(&e)==1,"tagged actual inspector click");ImGui::GetIO().AddMouseButtonEvent(0,down);std::fprintf(stderr,"[Item wind QA] click phase%u down%d xy%.1f,%.1f freeze%d playing%d mouse%.1f,%.1f capture%d active%d\n",q.phase,int(down),p.x,p.y,int(studio.production_freeze()),int(studio.production_playing()),ImGui::GetIO().MousePos.x,ImGui::GetIO().MousePos.y,int(ImGui::GetIO().WantCaptureMouse),int(ImGui::IsAnyItemActive()));q.since=frame;};
            auto renderer_off=[&](const world_studio::Document &d){
                const auto n=world_scenery_stats(studio.scene_ptr).source_instances;
                check(q.source<studio.applied_records.size()&&studio.applied_records[q.source]!=SIZE_MAX,"selected source maps to a committed render record");
                WorldSceneryPlacement selected;
                const size_t selected_record=q.source<studio.applied_records.size()?studio.applied_records[q.source]:SIZE_MAX;
                check(world_scenery_placement(studio.scene_ptr,selected_record,&selected)&&selected.wind_sway==0&&selected.wind_flutter==0,"exact selected source render record has wind disabled");
                bool neighbor=false;const auto alias=studio.source_poses[q.source].alias;
                for(size_t i=0;i<n;++i){WorldSceneryPlacement p;if(i!=selected_record&&world_scenery_placement(studio.scene_ptr,i,&p)&&p.alias&&alias==p.alias&&p.wind_sway==1&&p.wind_flutter==1)neighbor=true;}
                check(neighbor,"another same-alias instance keeps default wind in renderer");
                world_studio::Pose p;check(world_studio::effective_pose(d,studio.source_poses,q.source,p)&&!p.wind_enabled&&p.wind_sway==1&&p.wind_flutter==1,"disabled item retains its latent normal strengths");
            };
            const bool ready=!destination_loading&&terrain.view_ready(cam.x,cam.z,cam.fly)&&!studio.apply&&!studio.designer_frame_pending&&!studio.project_save_job.valid()&&!studio.project_save_requested&&!studio.production_freeze();
            const bool focus=(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)!=0&&!ImGui::GetIO().AppFocusLost;
            if(!q.failed&&q.phase!=200){
                if(!q.initialized){const auto root=std::filesystem::absolute(studio.project_directory).lexically_normal();const auto scene=std::filesystem::absolute(studio.path).lexically_normal();q.initialized=true;check(root.generic_string().find("/build/item-wind-")!=std::string::npos&&root.filename().string().find("qa-project")==0&&scene.parent_path()==root&&!std::getenv("VALWOOD_QA_STUDIO_DOCUMENT"),"confined to a full private item-wind qa-project");}
                if(!focus){q.since=frame;SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_PumpEvents();}
                else if(q.phase==0&&ready&&frame>55){
                    studio.open=true;studio.designer_tools=false;studio.tab=studio.requested_tab=2;look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);
                    const bool reopen=std::getenv("VALWOOD_QA_ITEM_WIND_REOPEN")!=nullptr;
                    bool found=false;
                    if(reopen){for(const auto &o:studio.draft.overrides)if(!o.wind_enabled){q.source=o.index;found=true;break;}}
                    else{double closest=1.e30;for(unsigned i=0;i<studio.source_poses.size();++i){const auto &p=studio.source_poses[i];bool tree=false;for(size_t a=0;a<world_scenery_alias_count();++a){const auto alias=world_scenery_alias(a);if(alias.name&&p.alias==alias.name){tree=alias.tree;break;}}const double distance=std::hypot(p.root.x-cam.x,p.root.z-cam.z);if(tree&&distance<closest){q.source=i;closest=distance;found=true;}}}
                    check(found,"find a real source tree");
                    if(!q.failed){check(studio.designer_select(world_designer::Ref::source_model(q.source)),"select tree through production selection");studio.designer_frame_selection();q.since=frame;
                        if(reopen){renderer_off(studio.draft);check(!studio.draft.placements.empty()&&!studio.draft.placements.back().wind_enabled,"fresh process retains disabled copied tree");check(!studio.project_has_unsaved(),"fresh full project is saved");q.phase=20;}
                        else{q.original=studio.draft;q.before=bytes(studio.draft);q.history=studio.history.back.size();q.placements=studio.draft.placements.size();q.applies=studio.applied;q.phase=1;}}
                }else if(q.phase==1&&ready&&frame-q.since>6){check(studio.designer_wind_enable_point.x>0,"actual inspector exposes wind checkbox");hover(studio.designer_wind_enable_point);q.phase=2;
                }else if(q.phase==2&&frame-q.since>4){button(true);q.phase=3;
                }else if(q.phase==3&&frame-q.since>2){button(false);q.phase=4;
                }else if(q.phase==4&&ready&&frame-q.since>6){
                    check(studio.history.back.size()==q.history+1,"one inspector click creates exactly one Undo");check(studio.applied>q.applies,"wind change reaches production Apply");renderer_off(studio.live);q.disabled_snapshot=studio.draft;q.off=bytes(studio.draft);check(q.off.rfind("VALWOOD_STUDIO 10\n",0)==0,"nondefault wind upgrades saved format to10");viewport();key(SDLK_z,true);q.phase=5;
                }else if(q.phase==5&&ready&&frame-q.since>6){auto baseline=q.original;
                    // Apply synchronizes the still-selected source back into
                    // the authoring graph. This registration is allowed; its
                    // pose, source identity and flags must remain exact.
                    for(const auto &node:studio.draft.entities.nodes)if(!world_entity::find(baseline.entities,node.id)){
                        world_studio::Pose original;check(world_studio::effective_pose(q.original,studio.source_poses,q.source,original),"read unchanged source pose for Undo registration");
                        const world_entity::Transform expected{original.root.x,original.root.y,original.root.z,original.yaw*57.29577951308232,original.scale,original.pitch*57.29577951308232,original.roll*57.29577951308232,original.sx,original.sy,original.sz};
                        check(node.binding==world_entity::Binding{world_entity::Kind::SourceModel,q.source}&&node.parent==0&&node.visible&&!node.locked&&node.name==original.alias&&node.source_key==world_authoring::source_key(studio.identity_sources[q.source])&&world_editor_scene_guard::same_transform(node.local,expected),"Undo adds only the unchanged selected source registration");
                        baseline.entities.nodes.push_back(node);
                    }
                    baseline.entities.next_id=studio.draft.entities.next_id;baseline.source_signature=studio.signature;
                    const bool exact=studio.draft.entities.next_id>=q.original.entities.next_id&&bytes(studio.draft)==bytes(baseline);
                    if(!exact){std::ofstream(studio.project_directory+"/.qa-wind-undo-expected.txt")<<bytes(baseline);std::ofstream(studio.project_directory+"/.qa-wind-undo-actual.txt")<<bytes(studio.draft);}
                    check(exact,"Ctrl+Z restores all content, apart from verified source registration and monotonic ID metadata");
                    check(!studio.designer_selection.active.empty(),"source tree stays selected after Undo removes its first registration");
                    WorldSceneryPlacement restored;check(q.source<studio.applied_records.size()&&world_scenery_placement(studio.scene_ptr,studio.applied_records[q.source],&restored)&&restored.wind_sway==1&&restored.wind_flutter==1,"Undo restores wind on exact renderer instance");key(SDLK_y,true);q.phase=6;
                }else if(q.phase==6&&ready&&frame-q.since>6){auto disabled=q.disabled_snapshot;disabled.entities.next_id=studio.draft.entities.next_id;check(bytes(studio.draft)==bytes(disabled),"actual Ctrl+Y restores complete disabled-wind scene with monotonic ID metadata");viewport();key(SDLK_c,true);q.phase=7;
                }else if(q.phase==7&&frame-q.since>5){check(studio.designer_clipboard.ready(),"actual Ctrl+C owns a source tree prefab");key(SDLK_v,true);q.phase=8;
                }else if(q.phase==8&&ready&&frame-q.since>8){check(studio.draft.placements.size()==q.placements+1,"actual Ctrl+V creates one tree copy");if(studio.draft.placements.size()>q.placements){const auto &p=studio.draft.placements.back();check(!p.wind_enabled&&p.wind_sway==1&&p.wind_flutter==1,"pasted tree retains independent wind settings");}renderer_off(studio.live);viewport();key(SDLK_s,true,true);q.phase=9;
                }else if(q.phase==9&&frame-q.since>8){
                    if(studio.project_save_failed)check(false,"coordinated Save All succeeds");
                    else if(ready&&studio.project_saved_together&&!studio.project_has_unsaved()){
                        world_studio::Document disk;std::string error;check(world_studio::load(studio.path.c_str(),disk,error),"fresh codec opens actual saved scene");check(bytes(disk)==bytes(studio.live),"Save All persists exact applied per-item settings");check(!disk.placements.empty()&&!disk.placements.back().wind_enabled,"saved copy retains wind disabled");renderer_off(studio.live);studio.designer_select(world_designer::Ref::source_model(q.source));q.since=frame;q.phase=20;
                    }
                }else if(q.phase==20&&ready&&frame-q.since>12){q.phase=200;qa_item_wind_passed=true;std::fprintf(stderr,"[Item wind QA] PASS %u checks; actual inspector click, Undo/Redo, clipboard, Apply, Save All and renderer factors\n",q.checks);std::fflush(stderr);}
            }
            if(q.phase!=200&&!q.failed&&(frame>650||frame+2>=uint64_t(launch->frames))){check(false,"wind workflow completes before bounded deadline");q.phase=999;}
            qa_item_wind_checks=q.checks;qa_item_wind_phase=q.phase;
        }

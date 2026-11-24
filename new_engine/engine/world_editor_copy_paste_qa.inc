// Native shortcuts, Apply, render records and collision are exercised against
// a private scene only. The normal editor never runs this fixture.
        if(launch->frames&&!launch->benchmark&&studio.path.rfind("/tmp/",0)==0&&std::getenv("VALWOOD_QA_EDITOR_COPY_PASTE")){
            ImGui::GetIO().IniFilename=nullptr;
            struct ClipboardQA {unsigned phase=0,checks=0,applies=0,copies=0,source=0;bool repaired=false;std::uint64_t since=0;size_t base=0,instances=0,colliders=0,history=0,recovery_models=0;double dx=0,dz=0,step=0;world_entity::Transform original;std::vector<world_entity::Id> roots;};
            static ClipboardQA q;
            auto check=[&](bool good,const char *label){++q.checks;if(!good){std::fprintf(stderr,"[Editor clipboard QA] FAIL phase %u: %s | %s\n",q.phase,label,studio.message.c_str());std::fflush(stderr);launch->exit_code=1;running=false;}return good;};
            auto key=[&](SDL_Keycode code){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.windowID=SDL_GetWindowID(window);e.key.state=SDL_PRESSED;e.key.keysym.sym=code;e.key.keysym.mod=KMOD_CTRL;SDL_PushEvent(&e);e.type=SDL_KEYUP;e.key.state=SDL_RELEASED;SDL_PushEvent(&e);q.since=frame;};
            const bool settled=!destination_loading&&terrain.view_ready(cam.x,cam.z,cam.fly)&&!studio.dirty;
            if(q.phase==0&&!q.repaired&&std::getenv("VALWOOD_QA_EDITOR_COPY_PASTE_RECOVERY")&&frame>40&&settled){
                const std::string recovery=std::getenv("VALWOOD_QA_EDITOR_COPY_PASTE_RECOVERY");
                if(!check(recovery.rfind("/tmp/",0)==0,"recovery regression uses a private copy"))continue;
                world_studio::Document draft;
                if(!check(world_studio::load(recovery.c_str(),draft,studio.message),"load exact user draft regression"))continue;
                q.recovery_models=draft.placements.size();studio.draft=std::move(draft);studio.changed(true);
                q.history=studio.history.back.size();q.applies=studio.applied+1;
                studio.designer_separate_overlaps();
                check(studio.apply&&studio.history.back.size()==q.history+1,"overlap repair is one undoable scene update");
                check(studio.draft.placements.size()==q.recovery_models,"repair preserves every placed model");
                q.repaired=true;q.phase=80;
            }else if(q.phase==80&&studio.applied==q.applies&&settled&&!studio.designer_paste_pending&&!studio.designer_frame_pending){
                check(studio.rejected==0,"exact user draft now applies without duplicate-root rejection");
                check(studio.live.placements.size()==q.recovery_models,"all recovered models rendered");
                check(studio.selected_added>=0&&!studio.selection_keys.empty(),"repaired model has visible model selection");
                const auto output=(std::filesystem::path(studio.path).parent_path()/"repaired-draft.world.studio").string();
                check(world_studio::save(output.c_str(),studio.live,studio.message),"retain repaired private draft evidence");
                std::fprintf(stderr,"[Editor clipboard QA] repaired user draft: %zu models retained, no rejected Apply\n",q.recovery_models);
                q.phase=0;
            }else if(q.phase==0&&frame>40&&settled){
                using R=world_designer::Ref;studio.open=true;studio.designer_tools=false;
                bool found=false;for(unsigned i=0;i<studio.source_poses.size();++i)if(studio.source_poses[i].alias=="purchased_house_a"){world_studio::Pose p;if(studio.pose(i,p)){q.source=i;found=true;break;}}
                if(!check(found,"visible purchased house exists"))continue;
                check(studio.designer_select(R::source_model(q.source)),"select actual house");
                check(world_designer::world_pose(studio.draft,studio.designer_context(),studio.designer_selection.active,q.original,studio.message),"read house world transform");
                const auto &source=studio.source_poses[q.source];float lo[3],hi[3];
                if(!check(world_scenery_alias_bounds(scenery,source.alias.c_str(),lo,hi),"actual GLB bounds available"))continue;
                q.dx=std::cos(q.original.yaw*.017453292519943295);q.dz=-std::sin(q.original.yaw*.017453292519943295);
                const auto matrix=vw_transform::matrix(q.original);double low=1e30,high=-1e30;
                for(unsigned corner=0;corner<8;++corner){const auto p=vw_transform::point(matrix,{corner&1?hi[0]:lo[0],corner&2?hi[1]:lo[1],corner&4?hi[2]:lo[2]});const double v=p[0]*q.dx+p[2]*q.dz;low=std::min(low,v);high=std::max(high,v);}
                q.step=high-low+.75;studio.designer_paste_options.gap=.75;
                q.base=studio.draft.placements.size();q.instances=world_scenery_stats(scenery).source_instances;q.colliders=world_scenery_stats(scenery).source_colliders;q.history=studio.history.back.size();
                key(SDLK_c);q.phase=1;
            }else if(q.phase==1&&frame-q.since>4){
                check(studio.designer_clipboard.ready(),"Ctrl+C fills scene clipboard");
                check(studio.history.back.size()==q.history&&studio.draft.placements.size()==q.base,"copy creates no history or placement");
                q.applies=studio.applied+1;key(SDLK_v);q.phase=2;
            }else if(q.phase==2&&studio.applied==q.applies&&settled&&!studio.designer_paste_pending&&!studio.designer_frame_pending){
                ++q.copies;
                check(studio.live.placements.size()==q.base+q.copies,"Ctrl+V commits exactly one house");
                check(world_scenery_stats(scenery).source_instances==q.instances+q.copies,"house exists in actual render scene");
                check(world_scenery_stats(scenery).source_colliders>q.colliders,"pasted houses have static collision");
                check(studio.history.back.size()==q.history+q.copies,"each paste is exactly one undo entry");
                const auto root=studio.designer_selection.active.id;q.roots.push_back(root);
                check(root!=0,"pasted copy selected by fresh persistent ID");
                check(studio.selected_added==int(q.base+q.copies-1)&&studio.selection_keys==std::vector<unsigned>{8192+unsigned(q.base+q.copies-1)},"actual pasted mesh selected, without empty wrapper");
                world_studio::Pose p;check(studio.pose(8192+unsigned(q.base+q.copies-1),p),"read actual pasted model");
                check(std::abs(p.root.x-(q.original.x+q.dx*q.step*q.copies))<.001&&std::abs(p.root.z-(q.original.z+q.dz*q.step*q.copies))<.001,"mesh footprint and gap snap repeated row");
                WorldSceneryPlacement rendered;
                check(world_scenery_placement(scenery,studio.applied_records[8192+unsigned(q.base+q.copies-1)],&rendered)&&std::abs(rendered.x-p.root.x)<.001&&std::abs(rendered.z-p.root.z)<.001,"GPU record matches pasted selection");
                ImVec2 screen;const auto viewport=studio.designer_viewport(float(display_w),float(display_h));const auto transform=vw_transform::matrix(vw_transform::pose(p));bool in_view=true;
                for(unsigned corner=0;corner<8;++corner){const auto v=vw_transform::point(transform,{corner&1?p.maximum.x:p.minimum.x,corner&2?p.maximum.y:p.minimum.y,corner&4?p.maximum.z:p.minimum.z});in_view&=studio.project_point(cam,{v[0],v[1],v[2]},screen)&&viewport.contains(screen.x,screen.y);}
                check(in_view,"all pasted mesh corners automatically revealed in viewport");
                if(q.copies<3){q.applies=studio.applied+1;key(SDLK_v);}
                else{q.applies=studio.applied+1;key(SDLK_z);q.phase=3;}
            }else if(q.phase==3&&studio.applied==q.applies&&settled){
                check(studio.live.placements.size()==q.base+2&&world_scenery_stats(scenery).source_instances==q.instances+2,"Ctrl+Z removes one rendered house");
                q.applies=studio.applied+1;key(SDLK_y);q.phase=4;
            }else if(q.phase==4&&studio.applied==q.applies&&settled){
                check(studio.live.placements.size()==q.base+3&&world_scenery_stats(scenery).source_instances==q.instances+3,"Ctrl+Y restores rendered house");
                q.applies=studio.applied+1;key(SDLK_z);q.phase=5;
            }else if(q.phase==5&&studio.applied==q.applies&&settled){
                q.applies=studio.applied+1;key(SDLK_v);q.phase=6;
            }else if(q.phase==6&&studio.applied==q.applies&&settled){
                check(studio.live.placements.size()==q.base+3,"paste branches after undo");
                const auto &last=studio.live.placements.back();
                check(std::abs(last.x-(q.original.x+q.dx*q.step*3))<.001&&std::abs(last.z-(q.original.z+q.dz*q.step*3))<.001,"new paste fills undone slot without gap");
                check(studio.designer_selection.active.id!=q.roots.back(),"undo branch gets fresh identity");q.roots.back()=studio.designer_selection.active.id;
                world_entity::Transform original;check(world_designer::world_pose(studio.draft,studio.designer_context(),world_designer::Ref::source_model(q.source),original,studio.message)&&original.x==q.original.x&&original.z==q.original.z,"original source house unchanged");
                studio.designer_select(world_designer::Ref::source_model(q.source));key(SDLK_c);q.phase=7;
            }else if(q.phase==7&&frame-q.since>4){
                q.applies=studio.applied+1;key(SDLK_v);q.phase=8;
            }else if(q.phase==8&&studio.applied==q.applies&&settled&&!studio.designer_paste_pending&&!studio.designer_frame_pending){
                check(studio.live.placements.size()==q.base+4&&studio.rejected==0,"recopying original skips occupied slots and renders fourth copy");
                const auto &last=studio.live.placements.back();check(std::abs(last.x-(q.original.x+q.dx*q.step*4))<.001&&std::abs(last.z-(q.original.z+q.dz*q.step*4))<.001,"recopied row has no stacked root");
                q.roots.push_back(studio.designer_selection.active.id);
                auto row=studio.designer_selection;row.items.push_back(world_designer::Ref::source_model(q.source));for(auto root:q.roots)if(root!=row.active.id)row.items.push_back(world_designer::Ref::entity(root));
                studio.designer_frame_selection(&row);studio.save();studio.designer_paste_notice_until=Studio::production_now()+600;
                std::fprintf(stderr,"[Editor clipboard QA] PASS %u checks; house stride %.3f m; 4 pasted houses with collision and automatic reveal\n",q.checks,q.step);std::fflush(stderr);q.phase=9;
            }
            if(q.phase!=9&&frame>1000){check(false,"fixture completed before deadline");q.phase=9;}
        }

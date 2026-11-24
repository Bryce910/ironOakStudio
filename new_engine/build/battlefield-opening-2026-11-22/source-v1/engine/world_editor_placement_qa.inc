// Public ImGui mouse events exercise the real asset card -> viewport -> Apply
// path. Enabled only for bounded runs using a private /tmp scene document.
        if(launch->frames&&!launch->benchmark&&studio.path.rfind("/tmp/",0)==0&&std::getenv("VALWOOD_QA_EDITOR_PLACEMENT")){
            struct PlacementQA {unsigned phase=0,cycle=0,checks=0,applies=0;std::uint64_t since=0;size_t base=0,instances=0,history=0,groups=0;ImVec2 card{},ground{},handle{};world_studio::Pose before,end;WorldScenery *scene=nullptr;};
            static PlacementQA q;
            auto check=[&](bool good,const char *label){++q.checks;if(!good){std::fprintf(stderr,"[Editor placement QA] FAIL %u: %s | %s\n",q.phase,label,studio.message.c_str());std::fflush(stderr);launch->exit_code=1;running=false;}return good;};
            auto step=[&](unsigned phase){q.phase=phase;q.since=frame;};
            auto select_card=[&](const char *name){
                const int index=studio.designer_asset_index(name);if(!check(index>=0,"test asset registered"))return;
                studio.designer_asset_select(unsigned(index));std::snprintf(studio.designer_asset_search,sizeof studio.designer_asset_search,"%s",name);
                studio.designer_asset_category=0;studio.designer_asset_favorites_only=false;studio.designer_asset_refilter=true;studio.designer_content=true;studio.designer_immersive=false;
            };
            auto &io=ImGui::GetIO();
            auto move_mouse=[&](ImVec2 point){SDL_WarpMouseInWindow(window,int(point.x),int(point.y));io.AddMousePosEvent(point.x,point.y);};
            if(q.phase==0&&frame>30&&!destination_loading&&terrain.view_ready(cam.x,cam.z,cam.fly)){
                studio.open=true;studio.designer_tools=false;studio.tab=2;studio.designer_layout={};
                q.base=studio.draft.placements.size();q.instances=world_scenery_stats(scenery).source_instances;
                const auto rect=studio.designer_viewport(float(display_w),float(display_h));
                q.ground={rect.x+rect.w*.5f,rect.y+rect.h*.8f};
                select_card("starting_crate");step(1);
            }else if(q.phase==1&&frame-q.since>=5){
                q.card=studio.designer_asset_selected_card;
                check(q.card.x>0&&q.card.y>q.ground.y,"real content card submitted");
                move_mouse(q.card);io.AddMouseButtonEvent(0,true);step(2);
            }else if(q.phase==2&&frame-q.since>=4){
                move_mouse({q.card.x+18,q.card.y});step(3);
            }else if(q.phase==3&&frame-q.since>=4){
                check(studio.designer_asset_placing(),"real card began drag payload");
                move_mouse(q.ground);step(4);
            }else if(q.phase==4&&frame-q.since>=5){
                check(studio.target,"viewport drag found ground");
                check(studio.draft.placements.size()==q.base,"dragging does not append objects");
                q.applies=studio.applied+1;io.AddMouseButtonEvent(0,false);step(5);
            }else if(q.phase==5&&studio.applied==q.applies&&!destination_loading&&!studio.dirty){
                check(studio.live.placements.size()==q.base+1,"release automatically committed one object");
                check(world_scenery_stats(scenery).source_instances==q.instances+1,"new object is in GPU scenery scene");
                check(!studio.last_apply_terrain,"placement retained terrain worker");
                check(studio.selected_added==int(q.base),"dropped object remains selected");
                world_studio::Pose p;check(studio.pose(WORLD_ADDED_KEY_BASE+unsigned(q.base),p),"committed object has render bounds");
                check(p.maximum.y-p.minimum.y>.1,"actual model bounds loaded");
                check(world_scenery_stats(scenery).color_instances>0,"color batches submitted");
                if(q.cycle)check(p.alias=="pine"&&p.maximum.y-p.minimum.y>10,"tree uses actual tall mesh");
                q.before=p;q.scene=scenery;q.history=studio.history.back.size();q.groups=world_scenery_stats(scenery).source_aliases;q.applies=studio.applied;
                studio.tab=2;studio.gizmo_mode=0;step(11);
            }else if(q.phase==6&&studio.applied==q.applies&&!destination_loading){
                check(studio.live.placements.size()==q.base&&world_scenery_stats(scenery).source_instances==q.instances,"Undo removes rendered object");
                q.applies=studio.applied+1;studio.designer_command(world_designer_viewport::Command::Redo);check(studio.apply,"Redo queued render update");step(7);
            }else if(q.phase==7&&studio.applied==q.applies&&!destination_loading){
                check(studio.live.placements.size()==q.base+1&&world_scenery_stats(scenery).source_instances==q.instances+1,"Redo restores rendered object");
                check(studio.designer_select(studio.designer_binding_ref({world_entity::Kind::Placement,unsigned(q.base)})),"restored object selects");
                q.applies=studio.applied+1;studio.designer_command(world_designer_viewport::Command::Delete);check(studio.apply,"Delete queued render update");step(8);
            }else if(q.phase==8&&studio.applied==q.applies&&!destination_loading){
                check(studio.live.placements.size()==q.base&&world_scenery_stats(scenery).source_instances==q.instances,"Delete removes rendered object");
                ++q.cycle;select_card("pine");step(1);
            }else if(q.phase==11&&frame-q.since>=5){
                auto pivot=q.before.root;pivot.y+=std::min(2.,(q.before.maximum.y-q.before.minimum.y)*q.before.scale*.5);
                const double length=std::clamp(std::hypot(std::hypot(pivot.x-cam.x,pivot.z-cam.z),pivot.y-cam.y)*.12,1.,15.);
                auto end=pivot;end.x+=length;ImVec2 a,b;
                check(studio.project_point(cam,pivot,a)&&studio.project_point(cam,end,b),"real X gizmo projects");
                q.handle={a.x+(b.x-a.x)*.75f,a.y+(b.y-a.y)*.75f};
                move_mouse(q.handle);io.AddMouseButtonEvent(0,true);step(12);
            }else if(q.phase==12&&frame-q.since>=5){
                check(studio.drag.active,"real gizmo began drag");step(13);
            }else if(q.phase==13){
                WorldSceneryPlacement actual;world_studio::Pose expected;
                const auto record=studio.applied_records[WORLD_ADDED_KEY_BASE+unsigned(q.base)];
                check(world_scenery_placement(scenery,record,&actual)&&studio.pose(WORLD_ADDED_KEY_BASE+unsigned(q.base),expected),"inspect live dragged model");
                check(std::abs(actual.x-expected.root.x)<1e-5&&std::abs(actual.z-expected.root.z)<1e-5,"actual model follows drag before release");
                check(world_scenery_stats(scenery).editor_preview_instances==1,"one transient rendered instance");
                check(scenery==q.scene&&studio.applied==q.applies&&!studio.dirty&&studio.history.back.size()==q.history,"drag does not rebuild, Apply, dirty save, or append history");
                // Distant residency may evict normally during these frames;
                // the scene/catalog must stay intact rather than reload.
                check(world_scenery_stats(scenery).source_aliases==q.groups,"drag retains the scene asset catalog");
                if(frame-q.since<120)move_mouse({q.handle.x+float(frame-q.since)*.45f,q.handle.y});
                else{
                    q.end=expected;check(std::abs(q.end.root.x-q.before.root.x)>.1,"model moved across drag frames");
                    std::fprintf(stderr,"[Editor drag QA] measured cycle %u frames %llu %llu\n",q.cycle,(unsigned long long)q.since,(unsigned long long)frame);std::fflush(stderr);
                    if(q.cycle!=1){io.AddMouseButtonEvent(0,false);q.applies=studio.applied+1;step(14);}
                    else{io.AddKeyEvent(ImGuiKey_Escape,true);step(16);}
                }
            }else if(q.phase==14&&studio.applied==q.applies&&!destination_loading&&frame-q.since>=10){
                world_studio::Pose p;check(studio.pose(WORLD_ADDED_KEY_BASE+unsigned(q.base),p)&&std::abs(p.root.x-q.end.root.x)<1e-5,"release commits the visible model pose");
                check(studio.history.back.size()==q.history+1,"one drag creates one undo step");
                check(studio.fast_move_commits>0&&scenery==q.scene&&studio.object_lighting_pending,"release commits in place without a scene/lighting refresh");
                std::fprintf(stderr,"[Editor fast move QA] cycle %u commit %.3f ms\n",q.cycle,studio.last_apply_ms);std::fflush(stderr);
                const auto exposure=studio.draft.look.exposure;studio.draft.look.exposure+=.1f;studio.designer_move_fast_candidate=true;
                check(!studio.try_fast_object_move(scenery)&&studio.draft.look.exposure!=studio.live.look.exposure,"fast move preserves unrelated staged lighting changes");studio.draft.look.exposure=exposure;
                if(q.cycle==2){check(studio.save(),"fast moved tree saves to private document");std::fprintf(stderr,"[Editor placement QA] PASS %u checks; 360 live gizmo frames, in-place crate/tree collision commits, cancel, Undo/Redo/Delete and save\n",q.checks);std::fflush(stderr);step(10);}
                else{q.applies=studio.applied+1;studio.designer_command(world_designer_viewport::Command::Undo);step(15);}
            }else if(q.phase==15&&studio.applied==q.applies&&!destination_loading){
                world_studio::Pose p;check(studio.pose(WORLD_ADDED_KEY_BASE+unsigned(q.base),p)&&std::abs(p.root.x-q.before.root.x)<1e-5,"Undo restores rendered transform");
                q.applies=studio.applied+1;studio.designer_command(world_designer_viewport::Command::Undo);check(studio.apply,"Undo placement queued render update");step(6);
            }else if(q.phase==16&&!studio.drag.active){
                io.AddKeyEvent(ImGuiKey_Escape,false);io.AddMouseButtonEvent(0,false);step(17);
            }else if(q.phase==17&&frame-q.since>=4){
                WorldSceneryPlacement actual;const auto record=studio.applied_records[WORLD_ADDED_KEY_BASE+unsigned(q.base)];
                check(world_scenery_placement(scenery,record,&actual)&&std::abs(actual.x-q.before.root.x)<1e-5,"Escape restores actual mesh pose");
                check(world_scenery_stats(scenery).editor_preview_instances==0&&studio.applied==q.applies&&studio.history.back.size()==q.history,"cancel restores without Apply or history");
                q.cycle=2;q.before.root={actual.x,actual.y,actual.z};q.history=studio.history.back.size();step(11);
            }
            if(studio.rejected)check(false,"placement transaction was rejected");
            if(q.phase!=10&&frame-q.since>600)check(false,"drag/preview transaction timed out");
            if(frame+1==std::uint64_t(launch->frames)&&q.phase!=10)check(false,"native fixture completed before frame limit");
        }

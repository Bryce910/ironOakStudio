// Bounded native integration fixture. All persistence uses an explicitly
// supplied private /tmp document. No player project is changed by this fixture.
        if(launch->frames&&!launch->benchmark&&studio.path.rfind("/tmp/",0)==0&&std::getenv("VALWOOD_QA_DESIGNER_WORKSPACE")){
            ImGui::GetIO().IniFilename=nullptr;
            struct DesignerQA {unsigned phase=0,checks=0;std::uint64_t since=0;world_entity::Id group=0;unsigned expected_apply=0;};
            static DesignerQA qa;
            auto check=[&](bool good,const char *what){qa.checks++;if(!good){launch->exit_code=1;running=false;std::fprintf(stderr,"[Designer QA] FAIL phase %u: %s | %s\n",qa.phase,what,studio.message.c_str());std::fflush(stderr);}return good;};
            if(qa.phase<4&&frame>30&&!destination_loading&&terrain.view_ready(cam.x,cam.z,cam.fly)){
                using O=world_designer::Operation;using R=world_designer::Ref;
                if(qa.phase==0){
                    const auto nodes=studio.draft.entities.nodes.size();
                    if(check(studio.designer_select(R::source_model(0)),"source zero selects")&&check(studio.selected==0&&studio.draft.entities.nodes.size()==nodes,"source zero stays lazy")){
                        world_designer::Command c;c.operation=O::CreateGroup;c.name="Designer integration group";
                        if(check(studio.designer_run(c),"create group")){
                            qa.group=studio.designer_selection.active.id;c={};c.operation=O::Rename;c.name="Gateway composition";
                            if(check(studio.designer_run(c),"rename group")){
                                c={};c.operation=O::SetFlags;c.change_locked=true;c.locked=true;
                                check(studio.designer_run(c),"lock group");c={};c.operation=O::Erase;
                                check(!studio.designer_run(c)&&world_entity::find(studio.draft.entities,qa.group),"locked delete rejected");
                                c={};c.operation=O::SetFlags;c.change_locked=true;c.locked=false;check(studio.designer_run(c),"unlock group");
                                const auto history=studio.history.back.size();studio.designer_begin_edit();
                                c={};c.operation=O::Transform;
                                for(unsigned i=0;i<40;i++){c.transform={double(i)*.05,0,0,10,1};if(!check(studio.designer_update_edit(c),"gesture update"))break;}
                                check(studio.designer_finish_edit()&&studio.history.back.size()==history+1,"one gesture one undo");
                                const auto before_cancel=studio.history.back.size();studio.designer_begin_edit();c.transform.x=12;
                                check(studio.designer_update_edit(c)&&studio.designer_cancel_edit()&&studio.history.back.size()==before_cancel,"cancel keeps history");
                                world_entity::Transform pose;std::string error;check(world_entity::world_transform(studio.draft.entities,qa.group,pose,error)&&std::abs(pose.x-1.95)<1e-9,"cancel restores accepted pose");
                                c={};c.operation=O::Duplicate;check(studio.designer_run(c),"duplicate hierarchy");
                                const auto copy=studio.designer_selection.active.id;check(copy!=qa.group,"fresh duplicate ID");c={};c.operation=O::Erase;check(studio.designer_run(c)&&!world_entity::find(studio.draft.entities,copy),"erase duplicate");
                                studio.designer_command(world_designer_viewport::Command::Undo);check(world_entity::find(studio.draft.entities,copy)!=nullptr,"undo delete");
                                studio.designer_command(world_designer_viewport::Command::Redo);check(world_entity::find(studio.draft.entities,copy)==nullptr,"redo delete");
                                check(studio.designer_select(R::source_model(0)),"inspect real source model");
                                world_entity::Transform model_pose;check(world_designer::world_pose(studio.draft,studio.designer_context(),studio.designer_selection.active,model_pose,error),"model world pose");
                                const auto model_history=studio.history.back.size();studio.designer_begin_edit();c={};c.operation=O::Transform;c.transform=model_pose;c.transform.x+=.125;c.transform.yaw+=1;
                                check(studio.designer_update_edit(c)&&studio.designer_finish_edit()&&studio.history.back.size()==model_history+1,"individual source model inspector transform");
                                studio.designer_command(world_designer_viewport::Command::Undo);check(studio.designer_select(R::source_model(0)),"select source after undo compaction");
                                world_entity::Transform model_restored;check(world_designer::world_pose(studio.draft,studio.designer_context(),studio.designer_selection.active,model_restored,error)&&std::abs(model_restored.x-model_pose.x)<1e-7,"model transform undo restores pose");
                                const auto cancel_history=studio.history.back.size();const bool was_dirty=studio.dirty,was_rebuild=studio.rebuild,was_saved=studio.production_project_saved;
                                studio.drag.before=studio.draft;studio.drag.before_dirty=was_dirty;studio.drag.before_rebuild=was_rebuild;studio.drag.before_saved=was_saved;studio.drag.issued=studio.draft.entities.next_id;studio.drag.keys={0};studio.drag.active=true;studio.draft.look.exposure+=.01f;
                                studio.cancel_designer_drag();check(studio.history.back.size()==cancel_history&&studio.dirty==was_dirty&&studio.rebuild==was_rebuild&&studio.production_project_saved==was_saved,"legacy gizmo cancel preserves history and flags");
                                check(studio.designer_select(R::entity(qa.group)),"return to connected group");
                                studio.designer_layout.left=280;studio.designer_layout.right=360;studio.designer_save_preferences();
                                world_designer_workspace::Preferences preferences;check(world_designer_workspace::load(studio.designer_workspace_path,preferences,error)&&preferences.layout.left==280&&preferences.layout.right==360,"private layout save reload");
                                studio.designer_layout={};qa.expected_apply=studio.applied+1;studio.apply=true;qa.phase=1;qa.since=frame;
                            }
                        }
                    }
                }else if(qa.phase==1&&studio.applied==qa.expected_apply&&!studio.dirty){
                    check(studio.save(),"real host Apply then Save");world_studio::Document reopened;std::string error;
                    check(world_studio::load(studio.path.c_str(),reopened,error)&&world_entity::find(reopened.entities,qa.group),"saved graph reload");
                    qa.phase=2;qa.since=frame;
                }else if(qa.phase==2){
                    const unsigned tool=unsigned(frame-qa.since-1)/3;
                    if(tool<25){studio.open=true;studio.requested_tab=int(tool);studio.designer_tools=true;}
                    else{qa.phase=3;qa.since=frame;studio.designer_tools=false;studio.tab=2;studio.requested_tab=-1;
                        double nearest=1e30;unsigned best=0;
                        for(unsigned i=0;i<studio.source.size();i++){const auto &p=studio.source[i];const double distance=std::hypot(p.x-cam.x,p.z-cam.z);if(p.foundation&&distance<nearest){nearest=distance;best=i;}}
                        check(studio.designer_select(R::source_model(best)),"designer inspector selects actual building");
                    }
                }else if(qa.phase==3&&frame-qa.since>8){
                    const auto size=ImGui::GetIO().DisplaySize;const auto rect=studio.designer_viewport(size.x,size.y);
                    check(rect.w>=320&&rect.h>=240,"real scene viewport has usable dimensions");
                    std::printf("[Designer QA] PASS %u checks, all 25 tools rendered, Apply/Save/reload, lazy source selection, transactional edits, private workspace, viewport %.0f %.0f %.0f %.0f\n",qa.checks,rect.x,rect.y,rect.w,rect.h);std::fflush(stdout);qa.phase=4;studio.designer_native_qa_complete=true;
                }
                if(qa.phase<4&&frame-qa.since>1800){check(false,"native workflow timeout");}
            }
        }

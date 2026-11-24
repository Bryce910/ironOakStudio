// Explicit private-project native fixture. No production authoring writes.
        if(launch->frames&&studio.path.rfind("/tmp/",0)==0&&std::getenv("VALWOOD_QA_EDITOR_COMPLETION")){
            ImGui::GetIO().IniFilename=nullptr;
            struct CompletionQA {unsigned phase=0,checks=0,applies=0;size_t record=SIZE_MAX;world_entity::Transform expected;std::string original_physics,original_gameplay;};static CompletionQA q;
            auto verify=[&](bool good,const char *what){q.checks++;if(!good){launch->exit_code=1;running=false;std::fprintf(stderr,"[Editor completion QA] FAIL phase %u: %s | %s\n",q.phase,what,studio.message.c_str());}return good;};
            if(q.phase==0&&frame>30&&!destination_loading&&terrain.view_ready(cam.x,cam.z,cam.fly)){
                if(!verify(studio.project_directory.rfind("/tmp/valwood-editor-completion-",0)==0,"disposable whole project"))continue;
                for(size_t i=0;i<studio.source.size();i++)if(studio.source_names[i]=="starting_crate"||studio.source_names[i]=="starting_axe"){q.record=i;break;}
                if(!verify(q.record!=SIZE_MAX,"test prop exists"))continue;
                world_designer::Command command;command.operation=world_designer::Operation::Transform;std::string error;
                if(!verify(studio.designer_select(world_designer::Ref::source_model(unsigned(q.record))),"select source prop")||!verify(world_designer::world_pose(studio.draft,studio.designer_context(),studio.designer_selection.active,q.expected,error),"read source transform"))continue;
                q.expected.pitch=23;q.expected.roll=-17;q.expected.sx=1.2;q.expected.sy=.8;q.expected.sz=1.15;command.transform=q.expected;
                if(!verify(studio.designer_run(command),"full source transform command"))continue;
                q.applies=studio.applied+1;studio.apply=true;q.phase=1;
            }else if(q.phase==1&&studio.applied==q.applies&&!studio.dirty){
                verify(studio.terrain_rebuild_skipped>0&&!studio.last_apply_terrain,"object apply retained terrain");verify(!studio.last_apply_masks,"ordinary prop retained shelter masks");world_studio::Pose p;verify(studio.pose(unsigned(q.record),p),"inspect committed full transform");verify(std::abs(p.pitch-q.expected.pitch*.017453292519943295)<1e-5&&std::abs(p.roll-q.expected.roll*.017453292519943295)<1e-5&&std::abs(p.sx-1.2)<1e-5,"rendered pose matches inspector");
                studio.project_save_all();verify(studio.project_save_job.valid(),"background save started");q.phase=2;
            }else if(q.phase==2&&!studio.project_save_job.valid()){
                std::string error;world_project::Files files;if(!verify(world_project::load(studio.project_directory,studio.project_checkpoint_name,files,error),"first coordinated checkpoint"))continue;
                for(const auto &f:files){if(f.path=="world.physics")q.original_physics=f.bytes;if(f.path=="world.gameplay")q.original_gameplay=f.bytes;}
                verify(!q.original_physics.empty()&&!q.original_gameplay.empty(),"typed assets included");verify(studio.editor_session&&!studio.editor_session->stats().dirty,"scene recovery base synchronized after project save");studio.physics_document.gravity.x+=.25;studio.physics_modified=true;studio.gameplay_document.name+=" checkpoint second";studio.gameplay_dirty=true;studio.project_save_all();verify(studio.project_save_job.valid(),"second background save started");q.phase=3;
            }else if(q.phase==3&&!studio.project_save_job.valid()){
                std::string error;world_project::Files files;verify(world_project::load(studio.project_directory,std::string(studio.project_checkpoint_name)+".previous",files,error),"previous generation exists");studio.project_restore_previous();verify(studio.project_save_job.valid(),"coordinated restore started");q.phase=4;
            }else if(q.phase==4&&!studio.project_save_job.valid()){
                std::string error;world_project::Files files;verify(world_project::read(studio.project_directory,{"world.physics","world.gameplay"},files,error)&&files[0].bytes==q.original_physics&&files[1].bytes==q.original_gameplay,"both subsystems rolled back together");verify(studio.project_restart_required&&!studio.production_can_save(),"live restart fence prevents mixed generations");world_studio::Document saved;verify(world_studio::load(studio.path.c_str(),saved,error)&&!saved.overrides.empty(),"schema 6 persisted source transform");verify(!world_project::pending(studio.project_directory,studio.project_checkpoint_name),"journal cleaned");q.phase=5;std::fprintf(stderr,"[Editor completion QA] PASS %u checks | apply %.3f ms | terrain retained %u | grouped save/previous/restore/restart fence\n",q.checks,studio.last_apply_ms,studio.terrain_rebuild_skipped);std::fflush(stderr);
            }
            studio.open=true;studio.designer_tools=true;studio.requested_tab=14;
            if(frame+1==std::uint64_t(launch->frames)&&q.phase!=5){verify(false,"fixture completed before frame limit");}
        }

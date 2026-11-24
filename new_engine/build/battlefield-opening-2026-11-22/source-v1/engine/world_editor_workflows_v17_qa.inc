// Private, bounded integration check for the designer workflow revision.
        if(launch->frames&&studio.path.rfind("/tmp/",0)==0&&std::getenv("VALWOOD_QA_EDITOR_WORKFLOWS_V17")){
            struct WorkflowQA {int phase=0;unsigned checks=0,source=0;double before_x=0,start_x=0,start_z=0;world_designer::Ref selected;std::uint64_t since=0;};
            static WorkflowQA q;
            auto check=[&](bool good,const char *name){++q.checks;if(!good){std::fprintf(stderr,"[Editor workflows v17 QA] FAIL phase %d %s: %s\n",q.phase,name,studio.message.c_str());launch->exit_code=1;running=false;}return good;};
            if(frame>30&&!ui_loading&&!destination_loading&&terrain.view_ready(cam.x,cam.z,cam.fly)&&running&&q.phase<5){
                if(q.phase==0){
                    double nearest=1e30;for(unsigned i=0;i<studio.source.size();++i){world_studio::Pose candidate;if(!studio.pose(i,candidate)||candidate.hidden)continue;const double distance=std::hypot(candidate.root.x-cam.x,candidate.root.z-cam.z);if(distance<nearest){nearest=distance;q.source=i;}}
                    studio.designer_refresh_source_categories();
                    if(check(nearest<250,"nearby source exists")&&check(studio.designer_workflow(15)==0&&studio.designer_workflow(1)==1&&studio.designer_workflow(5)==2&&studio.designer_workflow(12)==3&&studio.designer_workflow(10)==4,"five workflow routes")&&
                       check(studio.designer_source_categories.size()==studio.source_names.size()&&studio.designer_world_filter==0,"nearby world filter and cached categories")&&
                       check(studio.designer_source_category("nm_city_sm_house") ==1&&studio.designer_source_category("nm_oak_tree") ==2&&studio.designer_source_category("nm_rock_ground") ==3&&studio.designer_source_category("nm_street_lamp") ==4,"world category labels")&&
                       check(studio.designer_select(world_designer::Ref::source_model(q.source)),"select source model")){
                        q.selected=studio.designer_selection.active;world_studio::Pose p;
                        if(check(studio.pose(q.source,p),"baseline pose")){q.before_x=p.root.x;const double before_y=p.root.y,before_z=p.root.z,yaw=p.yaw;studio.gizmo_local=false;studio.move_snap=.25f;studio.designer_nudge(0,1,false,false);
                            if(check(studio.pose(q.source,p)&&std::abs(p.root.x-q.before_x-.25)<.001,"quarter-metre nudge")){
                                studio.designer_nudge(0,1,true,false);
                                check(studio.pose(q.source,p)&&std::abs(p.root.x-q.before_x-.275)<.001,"fine nudge");
                                studio.designer_nudge(1,1,false,true);
                                check(studio.pose(q.source,p)&&std::abs(p.root.y-before_y-2.5)<.001,"coarse vertical nudge");
                                studio.designer_command(world_designer_viewport::Command::Undo);
                                check(studio.pose(q.source,p)&&std::abs(p.root.y-before_y)<.001,"vertical nudge undo");
                                studio.designer_command(world_designer_viewport::Command::Undo);
                                studio.designer_command(world_designer_viewport::Command::Undo);
                                check(studio.pose(q.source,p)&&std::abs(p.root.x-q.before_x)<.001,"nudge undo sequence");
                                studio.gizmo_local=true;studio.designer_nudge(0,1,false,false);
                                check(studio.pose(q.source,p)&&std::abs(p.root.x-q.before_x-.25*std::cos(yaw))<.001&&std::abs(p.root.z-before_z+.25*std::sin(yaw))<.001,"local-axis nudge");
                                studio.designer_command(world_designer_viewport::Command::Undo);studio.gizmo_local=false;
                                check(studio.pose(q.source,p)&&std::abs(p.root.x-q.before_x)<.001&&std::abs(p.root.z-before_z)<.001,"local-axis undo");
                                q.phase=1;q.since=frame;std::fprintf(stderr,"[Editor workflows v17 QA] nudges and groups passed\n");}}
                    }
                }else if(q.phase==1){studio.designer_audit();check(studio.designer_audit_open,"audit opens");q.phase=2;q.since=frame;std::fprintf(stderr,"[Editor workflows v17 QA] audit started\n");}
                else if(q.phase==2&&!studio.designer_audit_job.valid()){
                    check(studio.designer_select(world_designer::Ref::source_model(q.source)),"restore selection for play");
                    q.start_x=cam.x;q.start_z=cam.z;studio.designer_play_here();check(studio.production_playing()&&studio.designer_play_spawn_pending,"play from selection begins");q.phase=3;q.since=frame;std::fprintf(stderr,"[Editor workflows v17 QA] play started\n");
                }else if(q.phase==3&&!studio.designer_play_spawn_pending&&!studio.open){
                    check(std::hypot(cam.x-studio.designer_play_spawn.x,cam.z-studio.designer_play_spawn.z)<=7.1,"player starts beside selection on walkable ground");
                    check(studio.editor_session->request_stop(),"request Stop");studio.production_stop(world_editor::StopChoice::DiscardPlayChanges);q.phase=4;q.since=frame;std::fprintf(stderr,"[Editor workflows v17 QA] stop requested\n");
                }else if(q.phase==4&&studio.open&&!studio.production_restore&&!studio.production_playing()){
                    check(std::find(studio.selection_keys.begin(),studio.selection_keys.end(),q.source)!=studio.selection_keys.end(),"selection restored after Stop");
                    check(std::hypot(cam.x-q.start_x,cam.z-q.start_z)<1,"editor camera restored after Stop");
                    std::fprintf(stderr,"[Editor workflows v17 QA] PASS %u checks\n",q.checks);q.phase=5;running=false;
                }
                if(q.phase<5&&frame-q.since>900)check(false,"workflow timed out");
            }
        }

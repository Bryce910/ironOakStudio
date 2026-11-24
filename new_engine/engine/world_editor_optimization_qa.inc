// Extra native editor checks, confined to the existing private correctness
// fixture. Setup/edits are excluded from the later steady-state profile.
        if(launch->frames&&!launch->benchmark&&studio.path.rfind("/tmp/",0)==0&&std::getenv("VALWOOD_QA_EDITOR_OPTIMIZATION")&&studio.designer_native_qa_complete&&studio.applied&&!destination_loading){
            struct EditorOptimizationQA {
                unsigned phase=0,checks=0;std::uint64_t since=0;
                world_entity::Transform base;world_designer_scene::Counters rows;
                std::uint64_t inspector=0,catalog=0,baselines=0;std::size_t history=0;
                bool dirty=false,saved=false,rebuild=false;ImVec2 pick;
            };static EditorOptimizationQA opt;
            auto check_opt=[&](bool value,const char *what){opt.checks++;if(!value){std::fprintf(stderr,"[Editor optimization QA] FAIL %u: %s | %s\n",opt.phase,what,studio.message.c_str());launch->exit_code=1;running=false;}return value;};
            auto step_opt=[&](unsigned next){opt.phase=next;opt.since=frame;};
            if(opt.phase==0){
                const auto generation=world_scenery_alias_revision();std::string error;
                check_opt(studio.configure(studio.draft,error)&&world_scenery_alias_revision()!=generation,"successful same-count catalog publication advances generation");
                const auto accepted=world_scenery_alias_revision();WorldSceneryDefinition invalid;char reason[128]{};
                check_opt(!world_scenery_configure(&invalid,1,reason,sizeof reason)&&world_scenery_alias_revision()==accepted,"failed catalog transaction retains generation");
                check_opt(studio.designer_select(world_designer::Ref::source_model(0)),"select source for gesture");
                check_opt(world_designer::world_pose(studio.draft,studio.designer_context(),studio.designer_selection.active,opt.base,error),"read baseline pose");
                opt.history=studio.history.back.size();opt.dirty=studio.dirty;opt.saved=studio.production_project_saved;opt.rebuild=studio.rebuild;
                studio.designer_begin_edit();world_designer::Command c;c.operation=world_designer::Operation::Transform;c.transform=opt.base;c.transform.x+=.01;
                check_opt(studio.designer_update_edit(c),"first gesture preview");step_opt(1);
            }else if(opt.phase==1&&frame-opt.since>2){
                studio.designer_rebuild_rows();opt.rows=studio.designer_scene_index.counters();step_opt(2);
            }else if(opt.phase==2){
                check_opt(studio.designer_edit_scope.active(),"gesture remains active until explicit finish/cancel");
                world_designer::Command c;c.operation=world_designer::Operation::Transform;c.transform=opt.base;c.transform.x+=.01+.0001*double(frame-opt.since);
                check_opt(studio.designer_update_edit(c),"continuous property preview");
                if(frame-opt.since>=60){
                    studio.designer_rebuild_rows();const auto now=studio.designer_scene_index.counters();
                    check_opt(now.source_filter_rebuilds==opt.rows.source_filter_rebuilds&&now.authored_row_rebuilds==opt.rows.authored_row_rebuilds,"pose-only gesture does not rebuild source/hierarchy rows");
                    check_opt(studio.designer_cancel_edit()&&studio.history.back.size()==opt.history,"cancelled gesture retains history");
                    check_opt(studio.dirty==opt.dirty&&studio.rebuild==opt.rebuild&&studio.production_project_saved==opt.saved,"cancelled gesture retains saved state");step_opt(3);
                }
            }else if(opt.phase==3&&frame-opt.since>3){
                studio.designer_tools=false;studio.tab=2;opt.rows=studio.designer_scene_index.counters();opt.inspector=studio.designer_inspector_refreshes;
                opt.catalog=studio.designer_asset_registry.rebuilds();opt.baselines=studio.designer_baseline_copies;step_opt(4);
            }else if(opt.phase==4&&frame-opt.since>=100){
                const auto now=studio.designer_scene_index.counters();
                check_opt(now.source_filter_rebuilds==opt.rows.source_filter_rebuilds&&now.authored_row_rebuilds==opt.rows.authored_row_rebuilds,"passive rows reuse caches");
                check_opt(studio.designer_inspector_refreshes==opt.inspector,"passive inspector reuses validated values");
                check_opt(studio.designer_asset_registry.rebuilds()==opt.catalog,"passive catalog does not scan aliases");
                check_opt(studio.designer_baseline_copies==opt.baselines,"passive frames do not copy world document");
                double nearest=1e30;bool found=false;const auto display=ImGui::GetIO().DisplaySize;const auto rect=studio.designer_viewport(display.x,display.y);
                for(unsigned i=0;i<studio.source.size();i++){
                    const auto &record=studio.source[i];if(record.tree||!record.foundation)continue;world_studio::Pose pose;if(!studio.pose(i,pose))continue;
                    const double lx=(pose.minimum.x+pose.maximum.x)*pose.scale*.5,lz=(pose.minimum.z+pose.maximum.z)*pose.scale*.5,co=std::cos(pose.yaw),si=std::sin(pose.yaw);
                    ImVec2 screen;const world_studio::Vec center={pose.root.x+co*lx+si*lz,pose.root.y+(pose.minimum.y+pose.maximum.y)*pose.scale*.5,pose.root.z-si*lx+co*lz};
                    const double distance=std::hypot(std::hypot(center.x-cam.x,center.z-cam.z),center.y-cam.y);
                    if(distance>1&&distance<nearest&&studio.project_point(cam,center,screen)&&rect.contains(screen.x,screen.y)){nearest=distance;opt.pick=screen;found=true;}
                }
                if(check_opt(found,"find actual visible prop for mesh selection")){
                    studio.queue_pick(cam,opt.pick,false);const auto ray=studio.mouse_ray(cam,opt.pick);std::vector<std::pair<double,unsigned>> reference;
                    for(unsigned i=0;i<studio.source.size()+studio.draft.placements.size();i++){const unsigned key=i<studio.source.size()?i:8192+i-unsigned(studio.source.size());world_studio::Pose pose;double distance=0;if(studio.pose(key,pose)&&world_studio::ray_bounds(ray,pose,distance)&&distance<700)reference.push_back({distance,key});}
                    std::sort(reference.begin(),reference.end());if(reference.size()>16)reference.resize(16);std::vector<unsigned> keys;for(const auto &entry:reference)keys.push_back(entry.second);
                    check_opt(keys==studio.picking_candidates,"indexed candidates equal full-world precise bounds oracle");step_opt(5);
                }
            }else if(opt.phase==5&&!studio.picking.valid()){
                check_opt(studio.picking_cache_misses>0,"first precise mesh selection parses candidate assets");
                check_opt(studio.picking_exact_checks<studio.source.size()/2,"spatial index avoids full-world exact tests");
                studio.queue_pick(cam,opt.pick,false);step_opt(6);
            }else if(opt.phase==6&&!studio.picking.valid()){
                check_opt(studio.picking_cache_hits>0,"repeat selection reuses parsed mesh reports");
                std::printf("[Editor optimization QA] PASS %u checks; 60-frame gesture reused rows; idle inspector/catalog/baselines cached; pick exact %u/%zu, mesh cache %llu hits/%llu misses\n",opt.checks,studio.picking_exact_checks,studio.source.size(),(unsigned long long)studio.picking_cache_hits,(unsigned long long)studio.picking_cache_misses);std::fflush(stdout);step_opt(7);
            }
            if(opt.phase<7&&frame-opt.since>600)check_opt(false,"editor test transaction timeout");
        }

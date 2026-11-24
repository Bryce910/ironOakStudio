        // Bounded replay against a complete disposable project. All authoring
        // changes go through real buttons, viewport clicks and shortcuts.
        if(qa_editor_npc){
            ImGui::GetIO().IniFilename=nullptr;
            struct NpcQA {unsigned phase=0,checks=0,click=0,rejects=0,applies=0;std::uint64_t since=0,click_since=0,identity=0;size_t count=0,history=0;bool failed=false,initialized=false,source=false;float pitch=-.93f;ImVec2 mouse;std::array<world_navigation::Point,3> points;WorldVillageResident first;std::string stable;};static NpcQA q;
            auto check=[&](bool good,const char *label){++q.checks;if(!good){std::fprintf(stderr,"[NPC paths QA] FAIL phase%u: %s | %s | %s\n",q.phase,label,studio.message.c_str(),studio.npc_notice.c_str());std::fflush(stderr);q.failed=true;launch->exit_code=1;running=false;}return good;};
            auto bytes=[&](const world_studio::Document &d){std::string value,error;check(world_studio::encode(d,value,error),"encode real scene");return value;};
            auto hover=[&](ImVec2 p){q.mouse=p;SDL_WarpMouseInWindow(window,int(p.x),int(p.y));SDL_PumpEvents();SDL_Event e{};e.type=SDL_MOUSEMOTION;e.motion.windowID=SDL_GetWindowID(window);e.motion.which=0x56414c54u;e.motion.x=int(p.x);e.motion.y=int(p.y);SDL_PushEvent(&e);ImGui::GetIO().AddMousePosEvent(p.x,p.y);};
            auto button=[&](bool down){hover(q.mouse);SDL_Event e{};e.type=down?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;e.button.windowID=SDL_GetWindowID(window);e.button.which=0x56414c54u;e.button.button=SDL_BUTTON_LEFT;e.button.state=down?SDL_PRESSED:SDL_RELEASED;e.button.x=int(q.mouse.x);e.button.y=int(q.mouse.y);check(SDL_PushEvent(&e)==1,"tagged real mouse button");ImGui::GetIO().AddMouseButtonEvent(0,down);};
            auto click=[&](ImVec2 p){check(p.x>0&&p.y>0&&p.x<ImGui::GetIO().DisplaySize.x&&p.y<ImGui::GetIO().DisplaySize.y-30,"control or terrain point is visible");hover(p);q.click=1;q.click_since=frame;};
            auto point_click=[&](unsigned index){ImVec2 p;const auto &v=q.points[index];const auto size=ImGui::GetIO().DisplaySize;const auto viewport=studio.designer_viewport(size.x,size.y);if(check(studio.project_point(cam,{v.x,v.y,v.z},p)&&viewport.contains(p.x,p.y),"project real navigable ground into the viewport"))click(p);};
            auto key=[&](SDL_Keycode code,bool ctrl=false,bool shift=false){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.windowID=SDL_GetWindowID(window);e.key.state=SDL_PRESSED;e.key.keysym.sym=code;e.key.keysym.mod=SDL_Keymod((ctrl?KMOD_CTRL:0)|(shift?KMOD_SHIFT:0));e.key.keysym.unused=0x56414c54u;check(SDL_PushEvent(&e)==1,"tagged actual shortcut");e.type=SDL_KEYUP;e.key.state=SDL_RELEASED;e.key.keysym.mod=KMOD_NONE;SDL_PushEvent(&e);q.since=frame;};
            auto resident=[&](WorldVillageResident &r){if(q.count>=studio.live.actors.size())return false;const auto &a=studio.live.actors[q.count];size_t index=a.source>=0?size_t(a.source):world_village_resident_count;if(a.source<0)for(size_t i=0;i<q.count;i++)index+=studio.live.actors[i].source<0;return world_village_population_resident(population,index,&r);};
            auto actor_id=[&](){for(const auto &n:studio.draft.entities.nodes)if(n.binding==world_entity::Binding{world_entity::Kind::Actor,unsigned(q.count)})return n.id;return std::uint64_t(0);};
            auto source_view=[&](){
                // Real parapets occlude a high camera placed outside the stair
                // flight. Choose an actual view of all three treads; never
                // weaken the production picker's first visible hit rule.
                const auto &a=q.points.front(),&b=q.points.back();
                const double cx=(a.x+b.x)*.5,cz=(a.z+b.z)*.5,cy=(a.y+b.y)*.5;
                const auto size=ImGui::GetIO().DisplaySize;const auto viewport=studio.designer_viewport(size.x,size.y);
                for(double up:{8.,5.,12.,18.})for(double radius:{6.,9.,4.,12.})for(unsigned direction=0;direction<16;++direction){
                    Camera candidate=cam;const double angle=direction*6.283185307179586/16;
                    candidate.x=cx+std::cos(angle)*radius;candidate.z=cz+std::sin(angle)*radius;candidate.y=std::max(a.y,b.y)+up;
                    candidate.yaw=float(std::atan2(cx-candidate.x,-(cz-candidate.z)));candidate.pitch=float(std::atan2(cy-candidate.y,std::hypot(cx-candidate.x,cz-candidate.z)));
                    bool visible=true;
                    for(const auto &p:q.points){
                        ImVec2 at;world_physics::Hit hit;const double dx=p.x-candidate.x,dy=p.y-candidate.y,dz=p.z-candidate.z,length=std::hypot(std::hypot(dx,dz),dy);
                        if(!studio.project_point(candidate,{p.x,p.y,p.z},at)||!viewport.contains(at.x-12,at.y-12)||!viewport.contains(at.x+12,at.y+12)||
                           !active_playtest_walk->raycast({candidate.x,candidate.y,candidate.z},{dx/length*700,dy/length*700,dz/length*700},hit)||!hit.hit||hit.normal.y<.65||
                           std::hypot(std::hypot(hit.position.x-p.x,hit.position.z-p.z),hit.position.y-p.y)>.08){visible=false;break;}
                        const auto pixel_ray=studio.mouse_ray(candidate,{float(int(at.x)),float(int(at.y))});
                        if(!active_playtest_walk->raycast({candidate.x,candidate.y,candidate.z},{pixel_ray.direction.x*700,pixel_ray.direction.y*700,pixel_ray.direction.z*700},hit)||!hit.hit||hit.normal.y<.65||
                           std::hypot(std::hypot(hit.position.x-p.x,hit.position.z-p.z),hit.position.y-p.y)>.12){visible=false;break;}
                    }
                    if(visible){cam=candidate;cam.fly=true;cam.grounded=false;cam.vy=0;q.pitch=cam.pitch;std::fprintf(stderr,"[NPC paths QA] source view %.3f %.3f %.3f yaw%.4f pitch%.4f; three unobstructed first-hit tread rays\n",cam.x,cam.y,cam.z,cam.yaw,cam.pitch);return true;}
                }
                return false;
            };
            const bool ready=!destination_loading&&terrain.view_ready(cam.x,cam.z,cam.fly)&&!studio.apply&&!studio.designer_frame_pending&&!studio.project_save_job.valid()&&!studio.project_save_requested&&!studio.production_freeze()&&!studio.structure_cooking;
            const bool focus=(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)!=0&&!ImGui::GetIO().AppFocusLost;
            if(!q.initialized){q.initialized=true;q.source=std::getenv("VALWOOD_QA_EDITOR_NPC_SOURCE")!=nullptr;const auto root=std::filesystem::absolute(studio.project_directory).lexically_normal();check((root.generic_string().find("/build/npc-paths-")!=std::string::npos||root.generic_string().find("/build/collision-water-")!=std::string::npos)&&root.filename().string().find("qa-project")==0&&std::filesystem::absolute(studio.path).lexically_normal().parent_path()==root&&!std::getenv("VALWOOD_QA_STUDIO_DOCUMENT"),"full disposable NPC QA project only");}
            if(!q.failed&&q.phase!=200){
                if(!focus){q.since=frame;q.click_since=frame;SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_PumpEvents();}
                else if(q.click){if(q.click==1&&frame-q.click_since>4){button(true);q.click=2;q.click_since=frame;}else if(q.click==2&&frame-q.click_since>2){button(false);q.click=3;q.click_since=frame;}else if(q.click==3&&ready&&frame-q.click_since>7){q.click=0;q.since=frame;}}
                else if(ready&&frame-q.since>7){
                    switch(q.phase){
                    case 0:if(frame>55){
                        if(!check(population!=nullptr,"animated character library loaded in this area"))break;
                        studio.open=true;studio.designer_immersive=false;studio.designer_content=false;studio.designer_tools=true;studio.designer_layout.right=430;studio.tab=studio.requested_tab=10;look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);
                        if(std::getenv("VALWOOD_QA_EDITOR_NPC_REOPEN")){
                            if(!check(!studio.draft.actors.empty(),"saved NPC exists in fresh process"))break;
                            q.count=studio.draft.actors.size()-1;q.identity=actor_id();check(q.identity!=0,"saved durable NPC identity");check(studio.draft.actors[q.count].points.size()==3,"saved walking points survive reopen");studio.actor_selection=int(q.count);studio.designer_select(studio.designer_binding_ref({world_entity::Kind::Actor,unsigned(q.count)}));studio.npc_frame();q.phase=30;
                        }else{
                            q.count=studio.draft.actors.size();q.history=studio.history.back.size();q.applies=studio.applied;
                            bool found=false;auto query=studio.nav_query(scenery);if(q.source){
                                if(!check(active_playtest_walk&&active_structure_world,"complete source-floor collision published before NPC placement"))break;
                                const auto &path=world_grandstone_qa::paths[8];
                                for(unsigned at=10;at+24<path.count&&!found;at+=5){
                                    std::array<world_navigation::Point,3> candidate;
                                    bool floors=true;
                                    for(unsigned i=0;i<3;++i){const auto &v=path.points[at+i*12];float floor=0;floors=floors&&active_playtest_walk->surface(v.x,v.z,v.feet,floor,.45);candidate[i]={v.x,floor,v.z};}
                                    if(!floors||std::abs(candidate[2].y-candidate[0].y)<.8)continue;
                                    world_studio::Actor a;a.points.clear();for(const auto &v:candidate){a.points.push_back({v.x,v.z});a.floor_heights.push_back(v.y);}
                                    std::vector<world_navigation::Point> routed;std::string error;
                                    if(world_npc_source_navigation::route(a,true,true,active_playtest_walk,query,nullptr,routed,error)){q.points=candidate;found=true;}
                                }
                            }
                            for(unsigned ring=0;!q.source&&ring<14&&!found;ring++)for(int dz=-int(ring);dz<=int(ring)&&!found;dz++)for(int dx=-int(ring);dx<=int(ring)&&!found;dx++){
                                if(std::max(std::abs(dx),std::abs(dz))!=int(ring))continue;
                                std::array<world_navigation::Point,3> p={{{cam.x+dx*9-3,0,cam.z+dz*9},{cam.x+dx*9+3,0,cam.z+dz*9},{cam.x+dx*9+3,0,cam.z+dz*9-5}}};bool clear=true;
                                for(auto &v:p)clear=clear&&query.ground(query.data,v.x,v.z,v.y);
                                for(unsigned i=0;i<p.size()&&clear;i++)clear=query.clear(query.data,p[i],p[i],.35f)&&(i==0||query.clear(query.data,p[i-1],p[i],.35f));
                                if(clear){std::vector<std::array<double,2>> authored;for(const auto &v:p)authored.push_back({v.x,v.z});std::vector<world_navigation::Point> routed;std::string error;clear=world_navigation::path(nullptr,query,authored,false,.40f,routed,error);}if(clear){q.points=p;found=true;}
                            }
                            if(!check(found,"find actual dry collision-free terrain for test patrol"))break;
                            if(q.source){if(!check(source_view(),"all three stair waypoints are visible before actual input"))break;}
                            else{const auto &p=q.points[0];cam.x=p.x+3;cam.z=p.z+18;cam.y=p.y+24;cam.yaw=0;cam.pitch=-.93f;cam.fly=true;cam.grounded=false;cam.vy=0;q.pitch=cam.pitch;}
                            const auto &p=q.points[0];std::fprintf(stderr,"[NPC paths QA] patch %.3f %.3f %.3f stock%d\n",p.x,p.y,p.z,int(population_enabled));q.phase=1;
                        }q.since=frame;}break;
                    case 1:click(studio.npc_add_button);q.phase=2;break;
                    case 2:check(studio.npc_mode==decltype(studio)::NpcMode::Add,"Add NPC button arms placement");point_click(0);q.phase=3;break;
                    case 3:{if(!check(studio.draft.actors.size()==q.count+1&&studio.live.actors.size()==q.count+1,"ground click visibly applies exactly one NPC"))break;
                        check(studio.applied>q.applies,"NPC reaches runtime Apply without manual Apply");q.identity=actor_id();check(q.identity!=0,"added NPC has durable identity");if(q.source){check(studio.live.actors[q.count].floor_heights.size()==1,"clicked source floor height persists on actor");}check(studio.history.back.size()==q.history+1,"one placement one Undo");WorldVillageResident r;if(!check(resident(r)&&std::hypot(r.x-q.points[0].x,r.z-q.points[0].z)<.15,"renderer resident starts at clicked ground"))break;if(q.source){float floor=0;check(active_playtest_walk->sample(r.x,r.z,r.y,floor)&&std::abs(floor-r.y)<.04,"NPC feet follow source collision instead of lower atlas");}check(world_village_population_stats(population).loaded_pose_assets==world_village_pose_group_count,"complete character pose bank is loaded");if(!population_enabled)check(world_village_population_stats(population).residents==q.count+1,"authored library does not add stock crowds");click(studio.npc_draw_button);q.phase=4;break;}
                    case 4:check(studio.npc_mode==decltype(studio)::NpcMode::Append,"Draw walking path button arms viewport");point_click(1);q.phase=5;break;
                    case 5:check(studio.live.actors.size()>q.count&&studio.live.actors[q.count].points.size()==2,"first path point applies live");point_click(2);q.phase=6;break;
                    case 6:if(!check(studio.live.actors.size()>q.count&&studio.live.actors[q.count].points.size()==3&&!studio.npc_preview_route.empty(),"second point produces real collision-checked route"))break;
                        if(q.source){const auto &a=studio.live.actors[q.count];check(a.floor_heights.size()==3&&std::abs(a.floor_heights.back()-a.floor_heights.front())>.6,"saved source waypoints traverse real stair heights");}q.stable=bytes(studio.draft);q.history=studio.history.back.size();q.rejects=studio.npc_rejections;point_click(2);q.phase=7;break;
                    case 7:check(studio.npc_rejections>q.rejects&&studio.npc_notice_error,"duplicate waypoint reports clear rejection");check(bytes(studio.draft)==q.stable&&studio.history.back.size()==q.history,"rejected point preserves scene and Undo history");q.rejects=studio.npc_rejections;cam.pitch=1.2f;q.since=frame;q.phase=70;break;
                    case 70:{const auto v=studio.designer_viewport(ImGui::GetIO().DisplaySize.x,ImGui::GetIO().DisplaySize.y);click({v.x+v.w*.5f,v.y+v.h*.5f});q.phase=71;break;}
                    case 71:check(studio.npc_rejections>q.rejects&&!studio.target,"sky click rejects instead of reusing old target");check(bytes(studio.draft)==q.stable&&studio.history.back.size()==q.history,"no-ground rejection preserves scene and history");cam.pitch=q.pitch;key(SDLK_ESCAPE);q.phase=8;break;
                    case 8:check(studio.npc_mode==decltype(studio)::NpcMode::None,"Escape finishes path");check(resident(q.first),"read live patrol start");q.since=frame;q.phase=9;break;
                    case 9:if(frame-q.since>85){WorldVillageResident r;check(resident(r)&&std::hypot(r.x-q.first.x,r.z-q.first.z)>.25,"real animated NPC walks along authored path");key(SDLK_z,true);q.phase=10;}break;
                    case 10:check(studio.live.actors.size()>q.count&&studio.live.actors[q.count].points.size()==2&&actor_id()==q.identity,"Undo removes last waypoint and preserves NPC identity");key(SDLK_y,true);q.phase=11;break;
                    case 11:check(studio.live.actors.size()>q.count&&studio.live.actors[q.count].points.size()==3&&actor_id()==q.identity,"Redo restores walking route and durable identity");hover({700,450});key(SDLK_DELETE);q.phase=12;break;
                    case 12:check(studio.live.actors.size()==q.count&&studio.draft.actors.size()==q.count,"actual Delete removes selected NPC from scene");key(SDLK_z,true);q.phase=13;break;
                    case 13:check(studio.live.actors.size()==q.count+1&&actor_id()==q.identity,"Undo restores deleted NPC and route identity");key(SDLK_s,true,true);q.phase=14;break;
                    case 14:if(studio.project_save_failed)check(false,"Save All succeeds");else if(studio.project_saved_together&&!studio.project_has_unsaved()){world_studio::Document disk;std::string error;check(world_studio::load(studio.path.c_str(),disk,error),"read saved scene from disk");check(bytes(disk)==bytes(studio.live),"Save All persists complete live NPC scene");q.phase=30;q.since=frame;}break;
                    case 30:if(frame-q.since>15){WorldVillageResident r;if(!check(resident(r),"saved or restored NPC is active in renderer"))break;
                        check(actor_id()==q.identity,"durable identity remains selected");check(world_village_population_stats(population).visible>0&&world_village_population_stats(population).uploads>0,"NPC pose batch is visible and submitted to renderer");if(q.source){const auto &a=studio.live.actors[q.count];check(a.floor_heights.size()==3,"source-floor anchors survive Save All/reopen");float floor=0;check(active_playtest_walk&&active_playtest_walk->sample(r.x,r.z,r.y,floor)&&std::abs(floor-r.y)<.04,"rendered NPC feet remain on the selected source storey");check(world_village_population_stats(population).source_floor_failures==0,"source-floor animation samples have no dropped actors");}check(!studio.project_has_unsaved(),"project is saved at completion");if(q.failed)break;q.phase=200;qa_npc_passed=true;std::fprintf(stderr,"[NPC paths QA] PASS %u checks; real Add/path clicks, rejected duplicate, walking, Undo/Redo/Delete, Save All/reopen\n",q.checks);std::fflush(stderr);}break;
                    }
                }
            }
            if(q.phase!=200&&!q.failed&&(frame>1050||frame+2>=uint64_t(launch->frames))){check(false,"NPC workflow completes within bounded frames");q.phase=999;}
            if(q.phase==200&&q.source&&!q.failed){
                WorldVillageResident r;if(resident(r)){
                    cam.x=r.x+3;cam.z=r.z+5;cam.y=r.y+2.6;cam.fly=true;cam.grounded=false;cam.vy=0;
                    cam.yaw=float(std::atan2(r.x-cam.x,-(r.z-cam.z)));
                    cam.pitch=float(std::atan2(r.y+1-cam.y,std::hypot(r.x-cam.x,r.z-cam.z)));
                }
            }
            qa_npc_checks=q.checks;qa_npc_phase=q.phase;
        }

// Opt-in integration fixture. Include after Studio; GPU is a template so this
// can precede the procedural GL implementation. Only a marked private project
// may be changed. Coordinates/camera are fixture-controlled; production create,
// update, point-move and delete commands plus actual SDL shortcuts do the work.
struct EditorStructureSplineQA {
    enum class Result {Inactive,Running,Passed,Failed};
    struct Snapshot {
        world_studio::Document document;
        std::uint64_t combined=0,procedural=0,vertices=0,triangles=0;
        unsigned segments=0;
    };
    unsigned phase=0,checks=0,first_gl_error=0,creates=0,updates=0,moves=0,deletes=0,cancels=0,undos=0,redos=0,saves=0,rejected=0;
    unsigned walk_probes=0,run_probes=0,rail_probes=0,underpass_probes=0,wall_probes=0,publication_checks=0,checkpoint_files=0;
    bool initialized=false,passed=false,failed=false,reopen=false,checkpoint_verified=false,scene_verified=false;
    std::uint64_t started=0,since=0,since_frame=0,ready_since=0,ready_frame=0,scene_fingerprint=0,checkpoint_fingerprint=0;
    std::size_t placements=0,initial_history=0;
    world_structure::Id wall_owner=0,bridge_owner=0;
    double x=0,z=0,camera_x=0,camera_y=0,camera_z=0;float camera_yaw=0,camera_pitch=0;
    std::string directory,failure,expected_scene;
    Snapshot base,updated,moved,deleted;
    world_structure_runtime::Prepared published;
    static std::uint64_t fingerprint(const std::string &text){
        std::uint64_t h=14695981039346656037ull;for(unsigned char c:text){h^=c;h*=1099511628211ull;}return h;
    }
    bool check(bool ok,const char *label){
        ++checks;if(ok)return true;
        if(!failed){failure=label;std::fprintf(stderr,"[Structure spline QA] FAIL phase%u: %s\n",phase,label);std::fflush(stderr);}failed=true;return false;
    }
    std::string bytes(const world_studio::Document &document){std::string text,error;if(!world_studio::encode(document,text,error))check(false,"encode complete authored document");return text;}
    void advance(unsigned next,std::uint64_t frame){phase=next;since=SDL_GetTicks64();since_frame=frame;ready_since=0;}
    static double ground(double px,double pz,const void *){return world_atlas_height(px,pz);}
    bool guard(Studio &s){
        namespace fs=std::filesystem;std::error_code error;
        const auto raw=fs::absolute(s.project_directory).lexically_normal();
        const auto root=fs::weakly_canonical(raw,error);
        if(!check(!error&&root==raw&&!fs::is_symlink(fs::symlink_status(raw)),"ordinary canonical disposable project only"))return false;
        if(!check(root.filename().string().rfind("qa-project-",0)==0&&root.generic_string().find("/build/structure-splines-2026-11-07/")!=std::string::npos,"private structure-spline build project"))return false;
        const auto scene=fs::absolute(s.path).lexically_normal();
        if(!check(scene.parent_path()==root&&!fs::is_symlink(fs::symlink_status(scene))&&!std::getenv("VALWOOD_QA_STUDIO_DOCUMENT"),"ordinary private scene owns coordinated Save All"))return false;
        const auto marker=root/".valwood-structure-spline-qa";
        if(!check(fs::is_regular_file(fs::symlink_status(marker))&&!fs::is_symlink(fs::symlink_status(marker)),"ordinary explicit opt-in marker"))return false;
        std::ifstream input(marker);std::string token;std::getline(input,token);
        if(!check(token=="VALWOOD_STRUCTURE_SPLINE_QA_V1","explicit disposable-project write authorization"))return false;
        directory=root.string();return true;
    }
    void camera(Camera &cam){cam.x=camera_x;cam.y=camera_y;cam.z=camera_z;cam.yaw=camera_yaw;cam.pitch=camera_pitch;cam.fly=true;cam.grounded=false;cam.vy=0;}
    void frame_area(Camera &cam,double height){
        camera_x=x+27;camera_z=z+32;camera_y=height+24;
        camera_yaw=float(std::atan2(x-camera_x,camera_z-z));camera_pitch=float(std::atan2(height+2-camera_y,std::hypot(x-camera_x,z-camera_z)));camera(cam);
    }
    void shortcut(Studio &s,SDL_Window *window,SDL_Keycode code,bool shift=false){
        const auto size=ImGui::GetIO().DisplaySize;const auto v=s.designer_viewport(size.x,size.y);
        const int px=int(v.x+v.w*.5f),py=int(v.y+v.h*.5f);SDL_WarpMouseInWindow(window,px,py);SDL_PumpEvents();ImGui::GetIO().AddMousePosEvent(float(px),float(py));
        SDL_Event event{};event.type=SDL_KEYDOWN;event.key.windowID=SDL_GetWindowID(window);event.key.state=SDL_PRESSED;event.key.keysym.sym=code;
        event.key.keysym.unused=0x56414c54u;event.key.keysym.mod=SDL_Keymod(KMOD_CTRL|(shift?KMOD_SHIFT:0));
        check(SDL_PushEvent(&event)==1,"submit actual tagged production shortcut");event.type=SDL_KEYUP;event.key.state=SDL_RELEASED;event.key.keysym.mod=KMOD_NONE;
        check(SDL_PushEvent(&event)==1,"release actual production shortcut");
        if(code==SDLK_z)++undos;else if(code==SDLK_y)++redos;else if(code==SDLK_s)++saves;
    }
    const world_structure::Definition *find(const Studio &s,world_structure::Id owner)const{for(const auto &d:s.live.structures)if(d.id==owner)return &d;return nullptr;}
    template<class GPU> bool publication(Studio &s,WorldScenery *scene,const GPU &gpu,std::uint64_t combined,unsigned owners){
        if(!check(s.live.structures.size()==owners&&s.draft.structures.size()==owners&&s.live.placements.size()==placements&&s.draft.placements.size()==placements,"owned definitions replace geometry without ordinary-placement duplicates"))return false;
        if(!check(bytes(s.live)==bytes(s.draft),"published and draft authored definitions agree"))return false;
        world_structure_collision::Capture capture;std::string error;
        if(!check(world_structure_collision::capture(s.project_directory,scene,s.live,s.structure_mode(),capture,error,ground,nullptr),"recapture exact committed procedural/source geometry"))return false;
        if(!check(capture.fingerprint==combined&&capture.procedural.fingerprint==gpu.fingerprint,"rendered procedural and combined Jolt collision identities match current definitions"))return false;
        if(!check(capture.procedural.meshes.size()==owners&&gpu.ranges.size()==owners&&gpu.walls+gpu.bridges==owners&&gpu.segments==capture.procedural.segments&&gpu.triangles==capture.procedural.triangles,"exact owner, segment and triangle counts agree with actual GPU upload"))return false;
        if(!check(gpu.bytes==capture.procedural.vertices*sizeof(world_structure_runtime::Vertex)&&(!owners||(gpu.vao&&gpu.vbo)),"actual GL vertex storage exactly matches collision-baked mesh"))return false;
        published=std::move(capture.procedural);++publication_checks;return true;
    }
    Snapshot snapshot(Studio &s,std::uint64_t combined)const{
        Snapshot v;v.document=s.live;v.combined=combined;v.procedural=published.fingerprint;v.vertices=published.vertices;v.triangles=published.triangles;v.segments=published.segments;return v;
    }
    bool exact(Studio &s,const Snapshot &expected,std::uint64_t combined){
        auto a=s.live,b=expected.document;const auto highwater=std::max(a.next_structure_id,b.next_structure_id);a.next_structure_id=b.next_structure_id=highwater;
        const auto entity_highwater=std::max(a.entities.next_id,b.entities.next_id);a.entities.next_id=b.entities.next_id=entity_highwater;
        return check(bytes(a)==bytes(b)&&combined==expected.combined&&published.fingerprint==expected.procedural&&published.vertices==expected.vertices&&published.triangles==expected.triangles&&published.segments==expected.segments,"Undo/Redo restores exact authored path plus matching owned GL/Jolt geometry");
    }
    bool walking(const Studio &s,const world_playtest_walk::Support *support){
        if(!check(support&&support->loaded(),"actual active Jolt walking owner is loaded"))return false;
        const auto *bridge=find(s,bridge_owner),*wall=find(s,wall_owner);
        if(!check(bridge&&wall&&bridge->kind==world_structure::Kind::Bridge&&wall->kind==world_structure::Kind::Wall,"find saved wall and bridge by stable owner identity"))return false;
        world_structure::Generated generated;std::string error;if(!check(world_structure::build(*bridge,generated,error),"derive authored centerline for actual capsule route"))return false;
        // Walking and running use the production can_move capsule/substep path,
        // at two distinct per-tick distances; no analytic floor substitutes.
        for(unsigned speed=0;speed<2;++speed){
            bool first=true;world_structure::Point previous;float feet=0;
            for(const auto &segment:generated.segments){const unsigned count=std::max(1u,unsigned(std::ceil(segment.horizontal_length/(speed?.42:.12))));
                for(unsigned j=0;j<=count;++j){const auto point=world_structure::mix(segment.a,segment.b,double(j)/count);
                    if(first){if(!check(support->sample(point.x,point.z,point.y,feet)&&std::abs(feet-point.y)<.05,"actual Jolt deck establishes exact starting feet"))return false;
                    previous=point;first=false;continue;}
                    if(world_structure::distance(previous,point)<1e-7)continue;
                    float next=0;double expected=0;if(!check(world_structure::deck_height(segment,point.x,point.z,expected),"authored route lies on exact deck triangles"))return false;
                    if(!check(support->can_move(previous.x,previous.z,point.x,point.z,feet,next)&&std::abs(next-expected)<.05,"production capsule traverses continuous sloped/curved bridge without snagging or falling"))return false;
                    previous=point;feet=next;if(speed)++run_probes;else ++walk_probes;
                }
            }
        }
        const auto &segment=generated.segments[generated.segments.size()/2];auto center=world_structure::mix(segment.a,segment.b,.5);
        if(!check(world_structure::deck_height(segment,center.x,center.z,center.y),"centre probe uses exact shared deck triangle height"))return false;
        const auto left=world_structure::mix(segment.left_a,segment.left_b,.5);const double dx=left.x-center.x,dz=left.z-center.z;
        world_physics::Hit hit;
        if(!check(support->raycast({center.x,center.y+2,center.z},{0,-3,0},hit)&&hit.hit&&hit.normal.y>.65&&std::abs(hit.position.y-center.y)<.035,"actual Jolt top face matches visible deck"))return false;
        for(int sign:{-1,1}){
            if(!check(support->cast_capsule_movement({center.x,center.y+.9,center.z},.37f,.50f,{sign*dx*2.3,0,sign*dz*2.3},hit)&&hit.hit&&hit.fraction<.95&&hit.normal.y<.65,"physical rail prevents player-size capsule leaving either side"))return false;
            ++rail_probes;
        }
        if(!check(support->raycast({center.x,center.y-bridge->thickness-1,center.z},{0,1.5,0},hit)&&hit.hit&&hit.normal.y<-.65&&std::abs(hit.position.y-(center.y-bridge->thickness))<.035,"actual closed underside blocks upward passage at visible deck thickness"))return false;
        ++underpass_probes;
        float lower=0;const double terrain=world_atlas_height(center.x,center.z);
        if(!check(!support->sample(center.x,center.z,terrain,lower)||lower<center.y-2,"overhead bridge never becomes an invisible ground-storey floor"))return false;
        ++underpass_probes;
        world_structure::Generated wall_path;if(!check(world_structure::build(*wall,wall_path,error),"derive wall crossing probe"))return false;
        const auto &wall_segment=wall_path.segments[wall_path.segments.size()/2];auto p=world_structure::mix(wall_segment.a,wall_segment.b,.5);if(wall->follow_ground)p.y=(world_atlas_height(wall_segment.a.x,wall_segment.a.z)+world_atlas_height(wall_segment.b.x,wall_segment.b.z))*.5;
        const double length=wall_segment.horizontal_length,nx=-(wall_segment.b.z-wall_segment.a.z)/length,nz=(wall_segment.b.x-wall_segment.a.x)/length;
        if(!check(support->cast_capsule_movement({p.x+nx*2,p.y+.9,p.z+nz*2},.37f,.50f,{-nx*4,0,-nz*4},hit)&&hit.hit&&hit.fraction<.95&&hit.normal.y<.65,"visible stone wall blocks actual player-size crossing capsule"))return false;
        ++wall_probes;
        return true;
    }
    bool verify_checkpoint(Studio &s){
        world_project::Files recorded,current;std::string error;
        if(!check(!world_project::pending(directory,s.project_checkpoint_name)&&world_project::load(directory,s.project_checkpoint_name,recorded,error),"coordinated Save All checkpoint loads without pending recovery journal"))return false;
        std::vector<std::string> names;for(const auto &f:recorded)names.push_back(f.path);
        if(!check(world_project::read(directory,names,current,error)&&current.size()==recorded.size(),"read every coordinated authoring checkpoint file"))return false;
        for(unsigned i=0;i<current.size();++i)if(!check(current[i].path==recorded[i].path&&current[i].exists==recorded[i].exists&&current[i].bytes==recorded[i].bytes,"all saved authoring checkpoint bytes match actual project"))return false;
        world_studio::Document disk;
        if(!check(world_studio::load(s.path.c_str(),disk,error)&&bytes(disk)==bytes(s.live),"saved studio file exactly matches complete published authored scene"))return false;
        scene_verified=true;checkpoint_files=unsigned(current.size());checkpoint_verified=true;std::ifstream input(std::filesystem::path(directory)/s.project_checkpoint_name,std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(input)),{});checkpoint_fingerprint=fingerprint(text);return check(!text.empty(),"checkpoint has observed persisted bytes");
    }
    bool deleted_collision(const world_playtest_walk::Support *support){
        const world_structure::Definition *bridge=nullptr;for(const auto &d:moved.document.structures)if(d.id==bridge_owner)bridge=&d;
        if(!check(bridge&&support,"retain deleted bridge's observed former geometry"))return false;
        world_structure::Generated path;std::string error;if(!check(world_structure::build(*bridge,path,error),"derive former bridge probe without recreating geometry"))return false;
        const auto &s=path.segments[path.segments.size()/2];auto p=world_structure::mix(s.a,s.b,.5);
        if(!check(world_structure::deck_height(s,p.x,p.z,p.y),"former bridge probe lies on observed deck"))return false;
        world_physics::Hit hit;return check(support->raycast({p.x,p.y+.3,p.z},{0,-.6,0},hit)&&!hit.hit,"deleted owner leaves no ghost Jolt deck at its former location");
    }
    bool write_expected(Studio &s,std::uint64_t combined){
        expected_scene=bytes(s.live);scene_fingerprint=fingerprint(expected_scene);moved=snapshot(s,combined);
        std::ofstream out(std::filesystem::path(directory)/".structure-spline-expected",std::ios::binary);
        out<<"VALWOOD_STRUCTURE_SPLINE_EXPECTED_V1\n"<<std::setprecision(17)<<x<<' '<<z<<'\n'<<wall_owner<<' '<<bridge_owner<<'\n'<<moved.combined<<' '<<moved.procedural<<' '<<moved.vertices<<' '<<moved.triangles<<' '<<moved.segments<<'\n'<<placements<<' '<<checkpoint_fingerprint<<'\n'<<camera_x<<' '<<camera_y<<' '<<camera_z<<' '<<camera_yaw<<' '<<camera_pitch<<'\n'<<expected_scene.size()<<' '<<scene_fingerprint<<'\n';out.write(expected_scene.data(),std::streamsize(expected_scene.size()));out.close();return check(bool(out),"persist observed exact scene and GL/Jolt identities for a separate reopen process");
    }
    bool read_expected(){
        std::ifstream in(std::filesystem::path(directory)/".structure-spline-expected",std::ios::binary);std::string magic;std::getline(in,magic);
        if(!check(magic=="VALWOOD_STRUCTURE_SPLINE_EXPECTED_V1","fresh process reads explicit saved workflow expectations"))return false;
        std::size_t size=0;in>>x>>z>>wall_owner>>bridge_owner>>moved.combined>>moved.procedural>>moved.vertices>>moved.triangles>>moved.segments>>placements>>checkpoint_fingerprint>>camera_x>>camera_y>>camera_z>>camera_yaw>>camera_pitch>>size>>scene_fingerprint;in.get();
        if(!check(bool(in)&&size>0&&size<=16*1024*1024,"bounded complete saved-scene expectation"))return false;
        expected_scene.resize(size);in.read(&expected_scene[0],std::streamsize(size));return check(bool(in)&&fingerprint(expected_scene)==scene_fingerprint,"fresh process expectation bytes match observed workflow fingerprint");
    }
    void report(){
        if(directory.empty())return;
        const auto now=SDL_GetTicks64();std::ofstream out(std::filesystem::path(directory)/(reopen?"structure-spline-reopen.json":"structure-spline-workflow.json"));
        out<<"{\n\"schema\":\"valwood.structure-splines.runtime-qa.v1\",\n\"passed\":"<<(passed?"true":"false")<<",\n\"failed\":"<<(failed?"true":"false")<<",\n\"phase\":"<<phase<<",\n\"checks\":"<<checks<<",\n\"first_gl_error\":"<<first_gl_error<<",\n\"reopen\":"<<(reopen?"true":"false")<<",\n\"elapsed_seconds\":"<<(now-started)*.001<<",\n\"failure\":"<<world_json::quote(failure)<<",\n\"creates\":"<<creates<<",\n\"property_updates\":"<<updates<<",\n\"point_moves\":"<<moves<<",\n\"deletes\":"<<deletes<<",\n\"cancels\":"<<cancels<<",\n\"undos\":"<<undos<<",\n\"redos\":"<<redos<<",\n\"saves\":"<<saves<<",\n\"walk_probes\":"<<walk_probes<<",\n\"run_probes\":"<<run_probes<<",\n\"rail_probes\":"<<rail_probes<<",\n\"underpass_probes\":"<<underpass_probes<<",\n\"wall_probes\":"<<wall_probes<<",\n\"publication_checks\":"<<publication_checks<<",\n\"wall_owner\":\""<<wall_owner<<"\",\n\"bridge_owner\":\""<<bridge_owner<<"\",\n\"combined_fingerprint\":\""<<moved.combined<<"\",\n\"procedural_fingerprint\":\""<<published.fingerprint<<"\",\n\"vertices\":"<<published.vertices<<",\n\"triangles\":"<<published.triangles<<",\n\"segments\":"<<published.segments<<",\n\"owners\":"<<published.meshes.size()<<",\n\"walls\":"<<published.walls<<",\n\"bridges\":"<<published.bridges<<",\n\"scene_verified\":"<<(scene_verified?"true":"false")<<",\n\"scene_fingerprint\":\""<<scene_fingerprint<<"\",\n\"checkpoint_fingerprint\":\""<<checkpoint_fingerprint<<"\",\n\"checkpoint_verified\":"<<(checkpoint_verified?"true":"false")<<",\n\"checkpoint_files\":"<<checkpoint_files<<",\n\"ordinary_placements\":"<<placements<<",\n\"interaction_scope\":\"Production create/update/commit-point/delete commands and tagged SDL Undo/Redo/Save All. Coordinates and camera are fixture-controlled; no physical pointer or controller acceptance claim. Actual active Jolt capsule walking/running, rails, wall and underside queries.\"\n}\n";
    }
    template<class GPU> Result tick(Studio &s,Camera &cam,WorldScenery *scene,SDL_Window *window,std::uint64_t frame,bool bounded,bool externally_ready,const GPU &gpu,const world_playtest_walk::Support *support,std::uint64_t combined,bool pending_ready){
        if(!bounded||!std::getenv("VALWOOD_QA_STRUCTURE_SPLINES"))return Result::Inactive;
        if(passed)return Result::Passed;
        if(failed)return Result::Failed;
        const auto now=SDL_GetTicks64();
        if(!initialized){initialized=true;started=since=now;since_frame=frame;reopen=std::getenv("VALWOOD_QA_STRUCTURE_SPLINES_REOPEN")!=nullptr;if(!guard(s)){report();return Result::Failed;}ImGui::GetIO().IniFilename=nullptr;}
        const auto error=glGetError();if(error){first_gl_error=error;check(false,"editing and actual GL upload/draw frames have no OpenGL error");report();return Result::Failed;}
        if(now-started>600000||now-since>180000){check(false,"structure workflow completes inside bounded phase deadline");report();return Result::Failed;}
        const bool ready=externally_ready&&!s.apply&&!s.structure_cooking&&!pending_ready&&!s.designer_frame_pending&&!s.project_save_job.valid()&&!s.project_save_requested&&!s.production_freeze()&&world_scenery_streaming_stats(scene).critical_pending==0;
        if(ready){if(!ready_since){ready_since=now;ready_frame=frame;}}else ready_since=0;
        const bool settled=ready&&now-ready_since>=250&&frame-ready_frame>=4;
        if((SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)==0||ImGui::GetIO().AppFocusLost){if(frame%30==0){SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);}return Result::Running;}
        if(s.project_save_failed){check(false,"coordinated Save All succeeds");report();return Result::Failed;}
        if(phase>=2&&s.rejected>rejected){check(false,"valid structure command publishes without Apply rejection");report();return Result::Failed;}
        if(!settled)return Result::Running;
        switch(phase){
        case 0:
            if(frame<40)break;
            s.open=true;s.designer_content=false;s.designer_immersive=false;s.designer_tools=true;s.tab=s.requested_tab=5;SDL_SetRelativeMouseMode(SDL_FALSE);
            if(reopen){if(!read_expected())break;
            camera(cam);advance(90,frame);break;}
            placements=s.live.placements.size();initial_history=s.history.back.size();rejected=s.rejected;
            if(!check(s.live.structures.empty()&&s.draft.structures.empty(),"fresh workflow has no prior test structures"))break;
            s.landscape_page=0;s.landscape_radius=64;s.target=false;s.landscape_find_open(cam);
            if(!check(s.target&&s.designer_frame_pending,"production Find open ground chooses actual landscape workspace"))break;
            x=s.x;z=s.z;advance(1,frame);break;
        case 1:{
            const auto before=bytes(s.draft);s.structure_spline_begin(world_structure::Kind::Wall);s.structure_spline_new.points={{x,world_atlas_height(x,z),z}};
            if(!check(!s.structure_spline_finish()&&bytes(s.draft)==before&&s.history.back.size()==initial_history,"incomplete creation leaves document/history/owned geometry unchanged"))break;
            s.structure_spline_cancel();++cancels;
            if(!check(bytes(s.draft)==before&&!s.apply,"cancelled creation leaves no ghost pieces"))break;
            const double height=world_atlas_height(x,z);frame_area(cam,height);advance(2,frame);break;}
        case 2:{
            s.structure_spline_begin(world_structure::Kind::Wall);auto &d=s.structure_spline_new;d.height=3;d.thickness=.55;d.follow_ground=true;d.points={{x+8,world_atlas_height(x+8,z-8),z-8},{x+8,world_atlas_height(x+8,z+8),z+8},{x+18,world_atlas_height(x+18,z+14),z+14}};
            if(!check(s.structure_spline_finish(),"production Finish builds terrain-following stone wall"))break;
            wall_owner=s.structure_spline_owner;++creates;advance(3,frame);break;}
        case 3:{
            if(!publication(s,scene,gpu,combined,1))break;
            const double height=std::max({double(world_atlas_height(x-7,z-10)),double(world_atlas_height(x-7,z+4)),double(world_atlas_height(x+1,z+16))})+4;
            s.structure_spline_begin(world_structure::Kind::Bridge);auto &d=s.structure_spline_new;d.width=4;d.spacing=3;d.railings=true;d.supports=false;d.follow_ground=false;d.points={{x-7,height,z-10},{x-7,height+.4,z+4},{x+1,height+.8,z+16}};
            if(!check(s.structure_spline_finish(),"production Finish builds raised continuous bridge"))break;
            bridge_owner=s.structure_spline_owner;++creates;advance(4,frame);break;}
        case 4:{
            if(!publication(s,scene,gpu,combined,2)||!walking(s,support))break;
            base=snapshot(s,combined);
            s.structure_spline_owner=bridge_owner;s.designer_finish_edit();s.checkpoint();auto *d=s.structure_spline_selected();if(!check(d!=nullptr,"select bridge property target"))break;
            d->width=4.8;d->smooth=true;
            if(!check(s.structure_spline_update(),"production Update rebuilds width and 3D curved path"))break;
            ++updates;advance(5,frame);break;}
        case 5:{
            if(!publication(s,scene,gpu,combined,2)||!check(published.fingerprint!=base.procedural,"property changes replace matching geometry"))break;
            updated=snapshot(s,combined);
            const auto *d=find(s,bridge_owner);if(!check(d&&d->points.size()==3,"editable bridge points retained"))break;
            auto p=d->points.back();p.x+=3;p.y+=.25;
            if(!check(s.structure_spline_commit_point(bridge_owner,2,p),"production 3D point-move command rebuilds owned structure"))break;
            ++moves;advance(6,frame);break;}
        case 6:
            if(!publication(s,scene,gpu,combined,2)||!walking(s,support)||!check(published.fingerprint!=updated.procedural,"moved point changes actual uploaded/colliding mesh"))break;
            moved=snapshot(s,combined);shortcut(s,window,SDLK_z);advance(7,frame);break;
        case 7:if(!publication(s,scene,gpu,combined,2)||!exact(s,updated,combined))break;
        shortcut(s,window,SDLK_z);advance(8,frame);break;
        case 8:if(!publication(s,scene,gpu,combined,2)||!exact(s,base,combined))break;
        shortcut(s,window,SDLK_y);advance(9,frame);break;
        case 9:if(!publication(s,scene,gpu,combined,2)||!exact(s,updated,combined))break;
        shortcut(s,window,SDLK_y);advance(10,frame);break;
        case 10:
            if(!publication(s,scene,gpu,combined,2)||!exact(s,moved,combined))break;
            s.structure_spline_owner=bridge_owner;if(!check(s.structure_spline_delete(),"production Delete removes one owned structure"))break;
            ++deletes;advance(11,frame);break;
        case 11:
            if(!publication(s,scene,gpu,combined,1)||!check(!find(s,bridge_owner)&&find(s,wall_owner),"deletion removes only selected owner and its uploaded/colliding mesh")||!deleted_collision(support))break;
            shortcut(s,window,SDLK_z);advance(12,frame);break;
        case 12:
            if(!publication(s,scene,gpu,combined,2)||!exact(s,moved,combined)||!walking(s,support))break;
            shortcut(s,window,SDLK_s,true);advance(13,frame);break;
        case 13:
            if(!s.project_saved_together||s.project_has_unsaved())break;
            if(!publication(s,scene,gpu,combined,2)||!verify_checkpoint(s))break;
            if(!check(gpu.draws>0&&gpu.shadow_draws>0,"actual procedural color and shadow draw paths executed"))break;
            if(!write_expected(s,combined))break;
            passed=true;phase=100;report();std::fprintf(stderr,"[Structure spline QA] PASS %u checks; create/update/point/delete, actual Undo/Redo/Save All, GL/Jolt match, walk/run/rails/underpass\n",checks);std::fflush(stderr);return Result::Passed;
        case 90:{
            if(!publication(s,scene,gpu,combined,2)||!check(bytes(s.live)==expected_scene&&combined==moved.combined&&published.fingerprint==moved.procedural&&published.vertices==moved.vertices&&published.triangles==moved.triangles&&published.segments==moved.segments,"fresh process opens exact saved definitions with identical generated GL/Jolt geometry"))break;
            const auto expected_checkpoint=checkpoint_fingerprint;
            if(!walking(s,support)||!verify_checkpoint(s)||!check(checkpoint_fingerprint==expected_checkpoint&&!s.project_has_unsaved(),"fresh process retains exact coordinated authoring checkpoint without edits"))break;
            if(!check(gpu.draws>0&&gpu.shadow_draws>0,"fresh process renders and shadows saved structures"))break;
            passed=true;phase=100;report();std::fprintf(stderr,"[Structure spline QA] PASS REOPEN %u checks\n",checks);std::fflush(stderr);return Result::Passed;}
        }
        if(failed){report();return Result::Failed;}return Result::Running;
    }
};

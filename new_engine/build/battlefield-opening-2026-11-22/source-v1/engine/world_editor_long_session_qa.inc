// Opt-in sustained editing against an ordinary disposable full project.
// Include after Studio and Camera. The rendering/terrain types are templated
// so this fixture can sit beside AssetBrowserQA without moving engine types.
// This is production gesture/shortcut integration, not physical input QA.
struct EditorLongSessionQA {
    enum class Result {Inactive,Running,Passed,Failed};
    struct Snapshot {
        std::string document;
        std::array<double,81> height{},walk{};
        std::uint64_t fingerprint=0;
    };
    struct Operation {
        unsigned block=0,index=0,page=0,tool=0,mask=15;
        float radius=128,strength=.4f,density=.65f;
        unsigned stamps=3;
        double stamp_ms=0,edit_visible_ms=0,undo_visible_ms=0,redo_visible_ms=0,restore_visible_ms=0;
        double main_thread_ms=0;
        unsigned terrain_tiles=0,ecology_areas=0;
        unsigned edit_commit=0,undo_commit=0,redo_commit=0,restore_commit=0;
        std::uint64_t begin_frame=0,end_frame=0,before_fingerprint=0,after_fingerprint=0;
        double begin_seconds=0,end_seconds=0;
    };
    unsigned phase=0,checks=0,edits=0,undos=0,redos=0,blocks=0,measured_edits=0,measured_actions=0;
    unsigned warm_edits=0,warm_cursor=0,cap_added=0,cap_undos=0,operation=0,save_count=0;
    unsigned first_gl_error=0,rejections=0,applies=0,strokes=0,cancelled=0;
    std::size_t history_before=0,forward_before=0,areas_before=0;
    bool initialized=false,passed=false,failed=false,reopen=false,acceptance=false;
    std::uint64_t started=0,measured_started=0,since=0,since_frame=0,last_progress=0,last_tick=0,ready_since=0,ready_since_frame=0;
    double requested_seconds=1200,x=0,z=0,camera_x=0,camera_y=0,camera_z=0;
    float camera_yaw=0,camera_pitch=-1.2f;
    std::string directory,failure;
    Snapshot before,after,baseline;
    std::vector<Snapshot> warm_states,cap_states;
    std::vector<Operation> samples;
    Operation current;

    static std::uint64_t fingerprint(const std::string &text){
        std::uint64_t value=14695981039346656037ull;
        for(unsigned char byte:text){value^=byte;value*=1099511628211ull;}return value;
    }
    bool check(bool value,const char *label){
        ++checks;if(value)return true;
        failed=true;failure=label;
        std::fprintf(stderr,"[Editor long session QA] FAIL phase%u: %s\n",phase,label);std::fflush(stderr);return false;
    }
    std::string bytes(const world_studio::Document &document){
        std::string text,error;if(!world_studio::encode(document,text,error))check(false,"encode complete actual scene");return text;
    }
    bool guard(Studio &s){
        std::error_code error;const auto root=std::filesystem::weakly_canonical(s.project_directory,error);
        if(!check(!error,"resolve full disposable project"))return false;
        const auto name=root.filename().string(),generic=root.generic_string();
        if(!check(name.rfind("qa-project-editor-long-session-",0)==0&&generic.find("/build/editor-long-session-")!=std::string::npos,"writes stay in private long-session build project"))return false;
        const auto scene=std::filesystem::weakly_canonical(s.path,error);
        if(!check(!error&&scene.parent_path()==root&&!std::getenv("VALWOOD_QA_STUDIO_DOCUMENT"),"full project owns scene and coordinated saves"))return false;
        std::ifstream marker(root/".valwood-editor-long-session-qa");std::string token;std::getline(marker,token);
        if(!check(token=="VALWOOD_EDITOR_LONG_SESSION_QA_V1","explicit disposable-project opt-in marker"))return false;
        directory=root.string();
        if(const char *text=std::getenv("VALWOOD_QA_EDITOR_LONG_SESSION_SECONDS")){
            char *end=nullptr;const double seconds=std::strtod(text,&end);
            if(!check(end!=text&&!*end&&std::isfinite(seconds)&&seconds>=30&&seconds<=7200,"duration is 30..7200 seconds"))return false;requested_seconds=seconds;
        }
        acceptance=requested_seconds>=1200;return true;
    }
    void advance(unsigned next,std::uint64_t frame){phase=next;since=SDL_GetTicks64();since_frame=frame;ready_since=0;}
    void hover(Studio &s,SDL_Window *window){
        const auto size=ImGui::GetIO().DisplaySize;const auto v=s.designer_viewport(size.x,size.y);
        const ImVec2 point{v.x+v.w*.5f,v.y+v.h*.5f};SDL_WarpMouseInWindow(window,int(point.x),int(point.y));SDL_PumpEvents();
        SDL_Event event{};event.type=SDL_MOUSEMOTION;event.motion.windowID=SDL_GetWindowID(window);event.motion.which=0x56414c54u;event.motion.x=int(point.x);event.motion.y=int(point.y);SDL_PushEvent(&event);ImGui::GetIO().AddMousePosEvent(point.x,point.y);
    }
    void key(Studio &s,SDL_Window *window,SDL_Keycode code,bool ctrl=false,bool shift=false){
        hover(s,window);SDL_Event event{};event.type=SDL_KEYDOWN;event.key.windowID=SDL_GetWindowID(window);event.key.state=SDL_PRESSED;
        event.key.keysym.sym=code;event.key.keysym.unused=0x56414c54u;event.key.keysym.mod=SDL_Keymod((ctrl?KMOD_CTRL:0)|(shift?KMOD_SHIFT:0));
        check(SDL_PushEvent(&event)==1,"submit tagged production shortcut");event.type=SDL_KEYUP;event.key.state=SDL_RELEASED;event.key.keysym.mod=KMOD_NONE;check(SDL_PushEvent(&event)==1,"release tagged shortcut");
    }
    void camera(Camera &cam){
        cam.x=camera_x;cam.y=camera_y;cam.z=camera_z;cam.yaw=camera_yaw;cam.pitch=camera_pitch;cam.fly=true;cam.vy=0;cam.grounded=false;
    }
    Snapshot snapshot(Studio &s){
        Snapshot result;result.document=bytes(s.draft);result.fingerprint=fingerprint(result.document);
        for(unsigned j=0;j<9;++j)for(unsigned i=0;i<9;++i){const unsigned at=j*9+i;const double xx=x+(int(i)-4)*32.,zz=z+(int(j)-4)*32.;result.height[at]=world_atlas_height(xx,zz);result.walk[at]=world_atlas_walk_height(xx,zz);}
        return result;
    }
    bool exact(Studio &s,const Snapshot &expected){
        if(!check(bytes(s.draft)==expected.document&&bytes(s.live)==expected.document,"Undo/Redo restores exact complete draft and published document"))return false;
        bool same=true,finite=true;
        for(unsigned j=0;j<9;++j)for(unsigned i=0;i<9;++i){const unsigned at=j*9+i;const double xx=x+(int(i)-4)*32.,zz=z+(int(j)-4)*32.;const double h=world_atlas_height(xx,zz),w=world_atlas_walk_height(xx,zz);finite=finite&&std::isfinite(h)&&std::isfinite(w);same=same&&std::abs(h-expected.height[at])<.02&&std::abs(w-expected.walk[at])<.02;}
        return check(finite&&same,"restored terrain and walking collision match saved 81-point grid");
    }
    template<class DressingType>
    bool validate(Studio &s,const DressingType &dressing){
        if(!check(bytes(s.live)==bytes(s.draft),"settled published scene matches actual authoring draft"))return false;
        if(!check(generated_cover_collision_ready,"generated tree and rock collision is ready"))return false;
        bool roots=true,heights=true,blocked=true;unsigned collision_probes=0,grass_probes=0;
        for(unsigned kind:{unsigned(WORLD_COVER_TREES),unsigned(WORLD_COVER_ROCKS)}){
            const auto &field=dressing.fields[kind];const auto collision_kind=kind==WORLD_COVER_TREES?world_cover_collision::Kind::Trunk:world_cover_collision::Kind::Rock;
            const auto &obstacles=generated_cover_collision.obstacles(collision_kind);
            roots=roots&&obstacles.size()==field.count&&generated_cover_collision.generation(collision_kind)==field.generation;
            if(field.count&&!field.instances)roots=false;
            if(field.instances)for(std::size_t i=0;i<field.count;++i){
                const auto &root=field.instances[i];
                roots=roots&&std::isfinite(root.x)&&std::isfinite(root.y)&&std::isfinite(root.z)&&root.scale>0;
                if(i<obstacles.size()){const auto &obstacle=obstacles[i];roots=roots&&obstacle.source_index==i&&std::abs(obstacle.x-field.anchor_x-root.x)<1.e-5&&std::abs(obstacle.z-field.anchor_z-root.z)<1.e-5;}
            }
            // Sample real published blockers, including tree and rock paths.
            // Starting outside avoids the locomotion service's escape rule.
            const auto stride=std::max(std::size_t(1),obstacles.size()/8);
            for(std::size_t i=0;i<obstacles.size()&&collision_probes<16;i+=stride){const auto &o=obstacles[i];
                if(o.ymax-o.ymin<.05)continue;
                const bool rock=collision_kind==world_cover_collision::Kind::Rock;
                const double local_x=rock?(o.xmin+o.xmax)*.5:0.,local_z=rock?(o.zmin+o.zmax)*.5:0.;
                const double centre_x=o.x+o.cosine*local_x+o.sine*local_z,centre_z=o.z-o.sine*local_x+o.cosine*local_z;
                const double dx=rock?o.cosine:1.,dz=rock?-o.sine:0.,reach=2*o.extent+1.;
                blocked=blocked&&!generated_cover_collision.can_move(centre_x-dx*reach,centre_z-dz*reach,centre_x+dx*reach,centre_z+dz*reach,o.ymin+.01,std::max(.1,o.ymax-o.ymin-.02),.37);++collision_probes;
            }
        }
        const auto &grass=dressing.fields[WORLD_COVER_GRASS];const auto &fades=dressing.grass_transition.fades();
        roots=roots&&grass.count==fades.size()&&grass.count<=WorldGrassTransition::max_instances;
        const auto stride=std::max(std::size_t(1),grass.count/96);
        if(grass.instances)for(std::size_t i=0;i<grass.count;i+=stride){
            // Ignore explicitly outgoing roots during the normal cross-fade.
            // At the settled observation point every visible retained root
            // must use the current committed terrain Y, including after Undo.
            if(i<fades.size()&&fades[i]>.99f){const auto &root=grass.instances[i];const double expected=world_atlas_height(grass.anchor_x+root.x,grass.anchor_z+root.z);heights=heights&&std::isfinite(root.y)&&std::abs(root.y-expected)<.035;++grass_probes;}
        }
        bool ground=true;for(unsigned j=0;j<9;++j)for(unsigned i=0;i<9;++i){const double xx=x+(int(i)-4)*32.,zz=z+(int(j)-4)*32.;const double h=world_atlas_height(xx,zz),w=world_atlas_walk_height(xx,zz);ground=ground&&std::isfinite(h)&&std::isfinite(w)&&std::abs(h-w)<.035;}
        return check(roots,"published tree/rock roots and collision generations agree")&&check(blocked,"sampled generated roots block actual crossing movement")&&check(heights,"visible retained grass roots follow committed terrain height")&&check(ground,"rendered and walkable ground agree across large edited grid");
    }
    bool gesture(Studio &s,Operation &op){
        const auto start=SDL_GetPerformanceCounter();s.landscape_page=int(op.page);s.landscape_shape=int(op.tool);s.landscape_plant=int(op.tool);s.landscape_radius=op.radius;s.landscape_strength=op.strength;s.landscape_density=op.density;s.landscape_layer_mask=op.mask;s.landscape_seed=87123;
        s.x=x;s.z=z;s.y=world_atlas_height(x,z);s.target=true;history_before=s.history.back.size();forward_before=s.history.forward.size();areas_before=s.draft.ecology.size();applies=s.applied;strokes=s.landscape_committed_strokes;rejections=s.rejected;
        if(!check(s.landscape_begin(),"begin actual production landscape gesture"))return false;
        for(unsigned i=0;i<op.stamps&&!failed;++i){s.x=x+(double(i)-(op.stamps-1)*.5)*16.;s.z=z+(op.page?8.:0);s.y=world_atlas_height(s.x,s.z);s.target=true;if(!check(s.landscape_stamp(),"stamp through production terrain/vegetation tool"))return false;}
        if(!check(s.landscape_stroke_changed,"production gesture changes real draft"))return false;s.landscape_finish();
        op.stamp_ms=1000.*double(SDL_GetPerformanceCounter()-start)/SDL_GetPerformanceFrequency();
        return check(s.apply&&!s.landscape_painting&&s.landscape_committed_strokes==strokes+1,"one gesture finishes as one pending Apply")&&check(s.history.back.size()==std::min(std::size_t(32),history_before+1),"one gesture adds exactly one bounded Undo entry");
    }
    Operation recipe(unsigned index){
        Operation op;op.block=blocks;op.index=index;
        if(index<4){op.tool=index<2?index:index+1;op.page=0;op.strength=index==2?.8f:.4f;}
        else if(index<8){op.page=1;op.tool=index==4?0u:index==5?4u:index==6?6u:7u;op.mask=index==5||index==7?1u:index==6?4u:15u;}
        else{op.radius=256;op.stamps=1;op.strength=.2f;op.page=index==9?1u:0u;op.tool=0;op.density=.5f;}
        return op;
    }
    void progress(Studio &s,std::uint64_t frame,bool ready){
        if(directory.empty())return;const auto now=SDL_GetTicks64();if(last_progress&&now-last_progress<1000)return;last_progress=now;
        std::ofstream output(std::filesystem::path(directory)/"editor-long-session-progress.jsonl",std::ios::app);
        output<<"{\"schema\":\"valwood.editor-long-session-progress.v1\",\"reopen\":"<<(reopen?"true":"false")<<",\"seconds\":"<<(now-started)*.001<<",\"measured_seconds\":"<<(measured_started?(now-measured_started)*.001:0)<<",\"phase\":"<<phase<<",\"frame\":"<<frame<<",\"ready\":"<<(ready?"true":"false")<<",\"baseline_observation\":"<<(phase==55&&ready?"true":"false")<<",\"baseline_fingerprint\":\""<<baseline.fingerprint<<"\",\"operation\":"<<operation<<",\"operation_page\":"<<current.page<<",\"operation_tool\":"<<current.tool<<",\"operation_radius\":"<<current.radius<<",\"blocks\":"<<blocks<<",\"edits\":"<<edits<<",\"undos\":"<<undos<<",\"redos\":"<<redos<<",\"measured_edits\":"<<measured_edits<<",\"measured_actions\":"<<measured_actions<<",\"applies\":"<<s.applied<<",\"terrain_tiles\":"<<s.live.terrain.size()<<",\"ecology_regions\":"<<s.live.ecology.size()<<",\"history_back\":"<<s.history.back.size()<<",\"history_forward\":"<<s.history.forward.size()<<",\"last_apply_ms\":"<<s.last_apply_ms<<",\"collision_ready\":"<<(generated_cover_collision_ready?"true":"false")<<",\"generated_trunks\":"<<generated_cover_collision.obstacles(world_cover_collision::Kind::Trunk).size()<<",\"generated_rocks\":"<<generated_cover_collision.obstacles(world_cover_collision::Kind::Rock).size()<<"}\n";
    }
    void report(){
        if(directory.empty())return;const auto now=SDL_GetTicks64();
        std::ofstream output(std::filesystem::path(directory)/(reopen?"editor-long-session-reopen.json":"editor-long-session-workflow.json"));
        output<<"{\n\"schema\":\"valwood.editor-long-session.runtime-qa.v1\",\n\"passed\":"<<(passed?"true":"false")<<",\n\"failed\":"<<(failed?"true":"false")<<",\n\"full_acceptance_duration\":"<<(acceptance?"true":"false")<<",\n\"reopen\":"<<(reopen?"true":"false")<<",\n\"phase\":"<<phase<<",\n\"checks\":"<<checks<<",\n\"first_gl_error\":"<<first_gl_error<<",\n\"failure\":"<<world_json::quote(failure)<<",\n\"requested_seconds\":"<<requested_seconds<<",\n\"wall_seconds\":"<<(now-started)*.001<<",\n\"elapsed_seconds\":"<<(measured_started?(now-measured_started)*.001:0)<<",\n\"measured_seconds\":"<<(measured_started?(now-measured_started)*.001:0)<<",\n\"blocks\":"<<blocks<<",\n\"edits\":"<<edits<<",\n\"undos\":"<<undos<<",\n\"redos\":"<<redos<<",\n\"undo_redo\":"<<(undos+redos)<<",\n\"measured_edits\":"<<measured_edits<<",\n\"measured_actions\":"<<measured_actions<<",\n\"warm_history_edits\":"<<warm_edits<<",\n\"near_cap_regions_added\":"<<cap_added<<",\n\"save_count\":"<<save_count<<",\n\"centre\":["<<std::setprecision(17)<<x<<','<<z<<"],\n\"interaction_scope\":\"Production landscape begin/stamp/finish gestures; real tagged Undo/Redo/Save All shortcuts. Camera and brush setup are fixture-controlled. No physical-input acceptance claim.\",\n\"operations\":[";
        for(std::size_t i=0;i<samples.size();++i){const auto &op=samples[i];if(i)output<<',';output<<"{\"block\":"<<op.block<<",\"index\":"<<op.index<<",\"page\":"<<op.page<<",\"tool\":"<<op.tool<<",\"mask\":"<<op.mask<<",\"radius\":"<<op.radius<<",\"stamps\":"<<op.stamps<<",\"begin_frame\":"<<op.begin_frame<<",\"end_frame\":"<<op.end_frame<<",\"begin_seconds\":"<<op.begin_seconds<<",\"end_seconds\":"<<op.end_seconds<<",\"before_fingerprint\":\""<<op.before_fingerprint<<"\",\"after_fingerprint\":\""<<op.after_fingerprint<<"\",\"edit_commit\":"<<op.edit_commit<<",\"undo_commit\":"<<op.undo_commit<<",\"redo_commit\":"<<op.redo_commit<<",\"restore_commit\":"<<op.restore_commit<<",\"stamp_ms\":"<<op.stamp_ms<<",\"edit_visible_ms\":"<<op.edit_visible_ms<<",\"undo_visible_ms\":"<<op.undo_visible_ms<<",\"redo_visible_ms\":"<<op.redo_visible_ms<<",\"restore_visible_ms\":"<<op.restore_visible_ms<<",\"main_thread_ms\":"<<op.main_thread_ms<<",\"terrain_tiles\":"<<op.terrain_tiles<<",\"ecology_areas\":"<<op.ecology_areas<<'}';}
        output<<"]\n}\n";
    }
    bool write_expected(Studio &s){
        baseline=snapshot(s);std::ofstream output(std::filesystem::path(directory)/".editor-long-session-expected",std::ios::binary);
        output<<"VALWOOD_EDITOR_LONG_SESSION_EXPECTED_V1\n"<<std::setprecision(17)<<x<<' '<<z<<' '<<camera_x<<' '<<camera_y<<' '<<camera_z<<' '<<camera_yaw<<' '<<camera_pitch<<'\n';
        for(unsigned i=0;i<81;++i)output<<baseline.height[i]<<' '<<baseline.walk[i]<<'\n';
        output<<baseline.document.size()<<' '<<baseline.fingerprint<<'\n';output.write(baseline.document.data(),std::streamsize(baseline.document.size()));output.close();
        return check(bool(output),"persist exact observed scene and collision-height reopen expectations");
    }
    bool read_expected(){
        std::ifstream input(std::filesystem::path(directory)/".editor-long-session-expected",std::ios::binary);std::string magic;std::getline(input,magic);
        if(!check(magic=="VALWOOD_EDITOR_LONG_SESSION_EXPECTED_V1","fresh process reads explicit workflow expectation"))return false;
        input>>x>>z>>camera_x>>camera_y>>camera_z>>camera_yaw>>camera_pitch;for(unsigned i=0;i<81;++i)input>>baseline.height[i]>>baseline.walk[i];
        std::size_t size=0;input>>size>>baseline.fingerprint;input.get();if(!check(bool(input)&&size>0&&size<=4*1024*1024,"bounded saved complete-scene expectation"))return false;
        baseline.document.resize(size);input.read(&baseline.document[0],std::streamsize(size));return check(bool(input)&&fingerprint(baseline.document)==baseline.fingerprint,"fresh process expectation bytes match fingerprint");
    }
    template<class DressingType,class TerrainType>
    Result tick(Studio &s,Camera &cam,const Camera &render_cam,DressingType &dressing,TerrainType &terrain,WorldScenery *scenery,SDL_Window *window,std::uint64_t frame,bool bounded_editor,bool externally_ready){
        if(!std::getenv("VALWOOD_QA_EDITOR_LONG_SESSION")||!bounded_editor)return Result::Inactive;
        if(passed)return Result::Passed;if(failed)return Result::Failed;const auto now=SDL_GetTicks64();
        if(!initialized){initialized=true;started=since=now;since_frame=frame;reopen=std::getenv("VALWOOD_QA_EDITOR_LONG_SESSION_REOPEN")!=nullptr;
            if(!guard(s)){report();return Result::Failed;}ImGui::GetIO().IniFilename=nullptr;
        }
        const auto error=glGetError();if(error){first_gl_error=error;check(false,"editing frames have no OpenGL error");report();return Result::Failed;}
        WorldCoverMetrics cover{};world_cover_metrics(dressing.cover,&cover);bool cover_ready=dressing.cover&&!cover.near_building&&!cover.tree_building;
        for(unsigned kind=1;kind<4;++kind){const auto &field=dressing.fields[kind];const double step=kind==WORLD_COVER_TREES?128.:24.;
            cover_ready=cover_ready&&field.generation==(kind==WORLD_COVER_TREES?cover.tree_generation:cover.near_generation)&&field.anchor_x==std::floor(render_cam.x/step+.5)*step&&field.anchor_z==std::floor(render_cam.z/step+.5)*step;}
        const bool ready=externally_ready&&cover_ready&&!s.dirty&&!s.apply&&!s.landscape_painting&&!s.structure_cooking&&!s.designer_frame_pending&&!s.project_save_job.valid()&&!s.project_save_requested&&!s.production_freeze()&&terrain.view_ready(cam.x,cam.z,cam.fly)&&world_scenery_streaming_stats(scenery).critical_pending==0;
        if(ready){if(!ready_since){ready_since=now;ready_since_frame=frame;}}else ready_since=0;
        // Let all ordinary grass cross-fades complete before judging roots.
        // Bounded editor rendering advances its animation clock by1/60 per
        // frame, so wall time alone is insufficient below60 FPS.
        const bool settled=ready&&now-ready_since>=1100&&frame-ready_since_frame>=52;
        progress(s,frame,ready);
        if(now-started>std::uint64_t((requested_seconds+1800)*1000)||now-since>180000){check(false,"long-session phase completed within wall-time deadline");report();return Result::Failed;}
        const bool focus=(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)!=0&&!ImGui::GetIO().AppFocusLost;
        if(!focus){if(frame%60==0){SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);}return Result::Running;}
        if(s.rejected>rejections&&phase>=10&&phase!=42){check(false,(std::string("valid edit Apply rejected: ")+s.message).c_str());report();return Result::Failed;}
        switch(phase){
        case 0:
            if(!settled||frame<50)break;
            s.open=true;s.designer_content=false;s.designer_immersive=false;s.designer_tools=true;s.tab=s.requested_tab=15;SDL_SetRelativeMouseMode(SDL_FALSE);hover(s,window);rejections=s.rejected;
            if(reopen){if(!read_expected())break;camera(cam);advance(90,frame);break;}
            s.landscape_page=0;s.landscape_radius=256;s.target=false;s.landscape_find_open(cam);
            if(!check(s.target&&s.designer_frame_pending,"Find open ground locates large actual authoring workspace"))break;
            x=s.x;z=s.z;advance(1,frame);break;
        case 1:
            if(!settled)break;
            // Keep one identical camera/streaming footprint for every measured
            // block. Looking almost straight down exposes the sculpted roots.
            camera_x=x;camera_z=z+40;camera_y=world_atlas_height(x,z)+90;camera_yaw=0;camera_pitch=-1.2f;camera(cam);advance(2,frame);break;
        case 2:
            if(!settled)break;
            if(!validate(s,dressing))break;warm_states.push_back(snapshot(s));warm_edits=0;advance(10,frame);break;
        case 10:{
            if(!settled)break;Operation op;op.radius=32;op.strength=.2f;op.stamps=1;op.tool=warm_edits%2;before=snapshot(s);
            if(!gesture(s,op))break;advance(11,frame);break;}
        case 11:
            if(!settled)break;if(!validate(s,dressing))break;check(s.applied==applies+1,"warm gesture publishes once");++edits;++warm_edits;warm_states.push_back(snapshot(s));
            if(warm_edits<40)advance(10,frame);else{check(s.history.back.size()==32,"forty real edits retain exactly bounded history capacity");warm_cursor=40;key(s,window,SDLK_z,true);++undos;advance(12,frame);}break;
        case 12:
            if(!settled)break;if(!check(warm_cursor>8,"history rollover cursor stays inside retained entries"))break;--warm_cursor;if(!exact(s,warm_states[warm_cursor])||!validate(s,dressing))break;
            if(warm_cursor>8){key(s,window,SDLK_z,true);++undos;advance(12,frame);}else{check(s.history.back.empty()&&s.history.forward.size()==32,"oldest eight edits are no longer exposed by Undo");key(s,window,SDLK_y,true);++redos;advance(13,frame);}break;
        case 13:
            if(!settled)break;++warm_cursor;if(!exact(s,warm_states[warm_cursor])||!validate(s,dressing))break;
            if(warm_cursor<40){key(s,window,SDLK_y,true);++redos;advance(13,frame);}else{warm_states.clear();warm_states.shrink_to_fit();advance(18,frame);}break;
        case 18:{
            // A persistent, modest hill makes smoothing a meaningful edit
            // even if Find open ground selected an otherwise planar meadow.
            if(!settled)break;Operation op;op.radius=128;op.strength=1.2f;op.stamps=3;op.tool=4;if(!gesture(s,op))break;advance(19,frame);break;}
        case 19:
            if(!settled)break;if(!validate(s,dressing))break;++edits;advance(20,frame);break;
        case 20:{
            if(!settled)break;Operation op;op.page=1;op.tool=0;op.radius=128;op.stamps=1;op.density=.65f;if(!gesture(s,op))break;advance(21,frame);break;}
        case 21:
            if(!settled)break;if(!validate(s,dressing))break;++edits;
            if(!check(dressing.fields[WORLD_COVER_GRASS].count>0&&dressing.fields[WORLD_COVER_TREES].count>0,"large baseline forest produces actual grass and tree roots"))break;
            baseline=snapshot(s);before=baseline;cancelled=s.landscape_cancelled_strokes;history_before=s.history.back.size();applies=s.applied;
            s.landscape_page=0;s.landscape_shape=0;s.landscape_radius=128;s.landscape_strength=.4f;s.x=x;s.z=z;s.y=world_atlas_height(x,z);s.target=true;
            if(!check(s.landscape_begin()&&s.landscape_stamp()&&s.landscape_stroke_changed,"cancel regression starts a real large dirty stroke"))break;
            key(s,window,SDLK_ESCAPE);advance(22,frame);break;
        case 22:
            if(frame-since_frame<3)break;
            if(!check(!s.landscape_painting&&s.landscape_cancelled_strokes==cancelled+1&&s.applied==applies&&s.history.back.size()==history_before,"Escape cancels entire gesture without Apply or history"))break;
            if(!exact(s,before))break;cap_states.push_back(snapshot(s));cap_added=cap_undos=0;advance(40,frame);break;
        case 40:{
            if(!settled)break;
            if(s.draft.ecology.size()==WORLD_COVER_REGION_LIMIT){before=snapshot(s);history_before=s.history.back.size();applies=s.applied;s.landscape_page=1;s.landscape_plant=4;s.landscape_radius=8;s.landscape_layer_mask=1;s.x=x;s.z=z;s.target=true;
                if(!check(s.landscape_begin(),"begin brush at documented ecology cap"))break;const bool accepted=s.landscape_stamp();
                if(!check(!accepted&&!s.landscape_painting&&s.message.find("limit")!=std::string::npos,"extra vegetation stroke is safely rejected at documented area limit"))break;
                if(!check(bytes(s.draft)==before.document&&s.applied==applies&&s.history.back.size()==history_before,"limit rejection preserves document publication and Undo"))break;
                if(cap_added){key(s,window,SDLK_z,true);++undos;advance(43,frame);}else advance(49,frame);break;}
            Operation op;op.page=1;op.tool=4;op.mask=1;op.radius=8;op.stamps=1;op.density=.05f;if(!gesture(s,op))break;advance(41,frame);break;}
        case 41:
            if(!settled)break;if(!validate(s,dressing))break;++cap_added;++edits;cap_states.push_back(snapshot(s));advance(40,frame);break;
        case 43:
            if(!settled)break;++cap_undos;if(!check(cap_undos<=cap_added,"cap cleanup stays in retained real gestures"))break;
            if(!exact(s,cap_states[cap_added-cap_undos])||!validate(s,dressing))break;
            if(cap_undos<cap_added){key(s,window,SDLK_z,true);++undos;advance(43,frame);}else advance(49,frame);break;
        case 49:
            if(!settled)break;if(!exact(s,baseline))break;cap_states.clear();cap_states.shrink_to_fit();measured_started=now;operation=0;advance(50,frame);break;
        case 50:
            if(!settled)break;if(!exact(s,baseline)||!validate(s,dressing))break;before=snapshot(s);current=recipe(operation);
            current.begin_frame=frame;current.begin_seconds=(now-started)*.001;current.before_fingerprint=before.fingerprint;
            if(!gesture(s,current))break;advance(51,frame);break;
        case 51:
            if(!settled)break;if(!validate(s,dressing)||!check(s.applied==applies+1,"measured gesture publishes exactly once"))break;
            after=snapshot(s);current.after_fingerprint=after.fingerprint;current.edit_commit=s.applied;current.edit_visible_ms=double(now-since);current.main_thread_ms=s.last_apply_ms;current.terrain_tiles=unsigned(s.live.terrain.size());current.ecology_areas=unsigned(s.live.ecology.size());++edits;++measured_edits;
            key(s,window,SDLK_z,true);++undos;++measured_actions;advance(52,frame);break;
        case 52:
            if(!settled)break;if(!exact(s,before)||!validate(s,dressing))break;current.undo_commit=s.applied;current.undo_visible_ms=double(now-since);key(s,window,SDLK_y,true);++redos;++measured_actions;advance(53,frame);break;
        case 53:
            if(!settled)break;if(!exact(s,after)||!validate(s,dressing))break;current.redo_commit=s.applied;current.redo_visible_ms=double(now-since);key(s,window,SDLK_z,true);++undos;++measured_actions;advance(54,frame);break;
        case 54:
            if(!settled)break;if(!exact(s,before)||!validate(s,dressing))break;current.restore_commit=s.applied;current.restore_visible_ms=double(now-since);current.end_frame=frame;current.end_seconds=(now-started)*.001;samples.push_back(current);advance(55,frame);break;
        case 55:
            // An explicit steady navigation/rendering window after every
            // restore lets the runner compare frame pacing at identical load.
            cam.yaw=camera_yaw+float(.12*std::sin(double(now-since)*.004));
            if(!settled||now-since<1500)break;
            cam.yaw=camera_yaw;
            if(++operation<10){advance(50,frame);break;}operation=0;++blocks;report();key(s,window,SDLK_s,true,true);advance(60,frame);break;
        case 60:
            if(s.project_save_failed){check(false,"periodic coordinated Save All succeeds");break;}
            if(!settled||!s.project_saved_together||s.project_has_unsaved())break;
            ++save_count;
            if(now-measured_started>=std::uint64_t(requested_seconds*1000)&&measured_edits>=(acceptance?100u:10u)&&measured_actions>=(acceptance?300u:30u)&&blocks>=(acceptance?10u:1u)){advance(70,frame);}else advance(50,frame);break;
        case 70:{
            if(!settled)break;if(!exact(s,baseline)||!validate(s,dressing))break;
            world_studio::Document disk;std::string error;
            if(!check(world_studio::load(s.path.c_str(),disk,error)&&bytes(disk)==bytes(s.live),"final saved scene codec equals exact published full project"))break;
            if(!check(!world_project::pending(s.project_directory,Studio::project_checkpoint_name),"final coordinated save leaves no interrupted journal"))break;
            if(!write_expected(s))break;passed=true;phase=100;report();std::fprintf(stderr,"[Editor long session QA] PASS %u checks, %u measured edits, %u measured Undo/Redo, %u blocks, %.1fs measured; full_duration=%d\n",checks,measured_edits,measured_actions,blocks,(now-measured_started)*.001,int(acceptance));std::fflush(stderr);return Result::Passed;}
        case 90:
            if(!settled)break;if(!exact(s,baseline)||!validate(s,dressing))break;
            if(!check(!s.project_has_unsaved()&&!world_project::pending(s.project_directory,Studio::project_checkpoint_name),"fresh process loads complete clean saved project"))break;
            if(!check(dressing.fields[WORLD_COVER_GRASS].count>0&&dressing.fields[WORLD_COVER_TREES].count>0,"fresh process regenerates visible forest and matching blockers"))break;
            passed=true;phase=100;report();std::fprintf(stderr,"[Editor long session QA] PASS REOPEN %u checks\n",checks);std::fflush(stderr);return Result::Passed;
        default:check(false,"recognized long-session workflow phase");break;
        }
        if(failed){report();return Result::Failed;}last_tick=now;return Result::Running;
    }
};

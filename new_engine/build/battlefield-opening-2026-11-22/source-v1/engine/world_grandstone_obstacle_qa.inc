// Frame-gated native regression of real source stairs, doors and gravity.
// Independent case setup teleports are reported explicitly. Every measured
// route step calls ordinary move_camera, retaining scenery and actor checks.
struct GrandstoneObstacleResult {
    bool selected=false,forward=false,reverse=false;
    unsigned steps=0,nodes=0,samples=0;
    double meters=0,maximum_node_error=0,maximum_resample_drift=0;
};
std::array<GrandstoneObstacleResult,std::size(world_grandstone_qa::cases)> grandstone_obstacle_results={};
const bool grandstone_obstacle_fast=std::getenv("VALWOOD_QA_GRANDSTONE_FAST")!=nullptr;
const char *grandstone_obstacle_capture_dir=std::getenv("VALWOOD_QA_GRANDSTONE_CAPTURE_DIR");
bool grandstone_obstacle_done=false,grandstone_obstacle_reverse=false;
unsigned grandstone_obstacle_phase=0,grandstone_obstacle_case=0,grandstone_obstacle_node=0;
unsigned grandstone_obstacle_passed_paths=0,grandstone_obstacle_directions=0;
unsigned grandstone_obstacle_blocked=0,grandstone_obstacle_slope=0,grandstone_obstacle_dropped=0;
unsigned grandstone_obstacle_setups=0,grandstone_obstacle_gate=0,grandstone_obstacle_gate_blocks=0;
unsigned grandstone_obstacle_gate_tests=0,grandstone_obstacle_captures=0,grandstone_obstacle_capture_failures=0;
unsigned grandstone_obstacle_hold=0,grandstone_obstacle_jump_steps=0;
uint64_t grandstone_obstacle_begin_frame=0;
double grandstone_obstacle_walked=0,grandstone_obstacle_jump_floor=0,grandstone_obstacle_jump_apex=0;
double grandstone_obstacle_jump_ceiling=0;
bool grandstone_obstacle_jump_landed=false,grandstone_obstacle_jump_ceiling_contact=false;
std::string grandstone_obstacle_capture_pending,grandstone_obstacle_failure;
auto grandstone_obstacle_selected=[&](unsigned index){return !grandstone_obstacle_fast||index==0||index==1||index==2||index==6;};
auto grandstone_obstacle_fail=[&](const char *reason){
    grandstone_obstacle_failure=reason;
    std::fprintf(stderr,"[Grandstone obstacle QA] FAIL phase %u case %u node %u: %s; pose %.9f %.9f %.9f\n",
        grandstone_obstacle_phase,grandstone_obstacle_case,grandstone_obstacle_node,reason,cam.x,cam.y-1.75,cam.z);
    std::fflush(stderr);launch->exit_code=1;running=false;
};
auto grandstone_obstacle_resample=[&](GrandstoneObstacleResult &result){
    if(!active_playtest_walk)return false;
    float first=0,second=0;const double feet=cam.y-1.75;
    if(!active_playtest_walk->sample(cam.x,cam.z,feet,first)||
       !active_playtest_walk->sample(cam.x,cam.z,first,second))return false;
    result.samples+=2;
    const double drift=std::max(std::abs(double(first)-second),std::abs(feet-first));
    result.maximum_resample_drift=std::max(result.maximum_resample_drift,drift);
    return std::isfinite(drift)&&drift<=.03;
};
auto grandstone_obstacle_setup=[&](unsigned section,const world_grandstone_qa::Point &point,double tx,double tz){
    // A complete source scene covers both districts. Retain that published
    // collision generation during rehearsal; switching to old bounded sector
    // files here would silently test different geometry from the editor.
    float feet=0;
    const bool already_supported=active_playtest_walk&&
        active_playtest_walk->sample(point.x,point.z,point.feet,feet)&&std::abs(feet-point.feet)<=.7;
    if(!already_supported){
        if(!playtest_select(section)||!active_playtest_walk||
           !active_playtest_walk->sample(point.x,point.z,point.feet,feet)||std::abs(feet-point.feet)>.7)return false;
    }
    playtest_section=section;
    playtest_walk_reference=feet;
    cam.x=point.x;cam.z=point.z;cam.y=double(feet)+1.75;
    cam.yaw=float(std::atan2(tx-point.x,-(tz-point.z)));cam.pitch=-.15f;
    cam.fly=false;cam.grounded=true;cam.vy=0;third_person=true;avatar_initialized=false;
    fight=WorldTrainingCombat{};playtest_combat_previous=TrainingState::Idle;
    playtest_combat_result_screen=TrainingState::Idle;live_training_encounter=0;
    studio.open=false;preview_screen=1;show_tools=show_map=show_graphics=journey_open=false;
    destination_loading=true;temporal_cut=true;++grandstone_obstacle_setups;
    std::printf("[Grandstone obstacle QA] SETUP teleport %u section %u: %.9f %.9f %.9f; independent regression case, not continuous-route proof\n",
        grandstone_obstacle_setups,section,cam.x,cam.y-1.75,cam.z);std::fflush(stdout);
    return true;
};
auto grandstone_obstacle_workflow=[&](uint64_t qa_frame){
    if(!grandstone_obstacle_qa||grandstone_obstacle_done||!running)return;
    if(!grandstone_obstacle_begin_frame)grandstone_obstacle_begin_frame=qa_frame+1;
    if(qa_frame>=grandstone_obstacle_begin_frame+19500){grandstone_obstacle_fail("Bounded obstacle workflow timed out");return;}
    if(destination_loading||qa_frame<=uint64_t(launch->warmup)||!terrain.view_ready(cam.x,cam.z,false)||
       world_scenery_streaming_stats(scenery).critical_pending)return;
    if(fight.state!=TrainingState::Idle||cam.fly||studio.open||preview_screen!=1){grandstone_obstacle_fail("Ordinary grounded exploration state was lost");return;}
    if(std::abs(view_near_plane()-.12f)>.001f){grandstone_obstacle_fail("Source camera near-plane correction is inactive");return;}
    if(!grandstone_obstacle_capture_pending.empty())return;
    if(grandstone_obstacle_phase==0){
        while(grandstone_obstacle_case<std::size(world_grandstone_qa::cases)&&!grandstone_obstacle_selected(grandstone_obstacle_case))++grandstone_obstacle_case;
        if(grandstone_obstacle_case==std::size(world_grandstone_qa::cases)){grandstone_obstacle_phase=4;return;}
        const auto &test=world_grandstone_qa::cases[grandstone_obstacle_case];
        const auto &path=world_grandstone_qa::paths[grandstone_obstacle_case];
        if(path.count<2||!grandstone_obstacle_setup(test.section,path.points[0],path.points[1].x,path.points[1].z)){
            grandstone_obstacle_fail("Case start lacks a valid source floor");return;
        }
        grandstone_obstacle_results[grandstone_obstacle_case].selected=true;
        grandstone_obstacle_reverse=false;grandstone_obstacle_node=1;grandstone_obstacle_hold=0;
        grandstone_obstacle_phase=1;return;
    }
    if(grandstone_obstacle_phase==1||grandstone_obstacle_phase==3){
        const auto &path=world_grandstone_qa::paths[grandstone_obstacle_case];
        auto &result=grandstone_obstacle_results[grandstone_obstacle_case];
        const auto &target=path.points[grandstone_obstacle_node];
        const double dx=target.x-cam.x,dz=target.z-cam.z,distance=std::hypot(dx,dz);
        if(distance>.001){
            const double step=std::min(distance,world_atlas_walk_speed()*1.7/60.);
            const double old_feet=cam.y-1.75;
            cam.yaw=float(std::atan2(dx,-dz));cam.pitch=-.15f;
            const auto motion=move_camera(cam,scenery,cam.x+dx/distance*step,cam.z+dz/distance*step);
            ++result.steps;result.meters+=motion.actual;grandstone_obstacle_walked+=motion.actual;
            grandstone_obstacle_blocked+=motion.collision;grandstone_obstacle_slope+=motion.slope;
            if(motion.actual<step-.002){++grandstone_obstacle_dropped;grandstone_obstacle_fail("Actual movement dropped or slid a validated route step");return;}
            if(motion.collision||motion.slope){grandstone_obstacle_fail("Actual movement reported a route obstruction");return;}
            if(!cam.grounded||std::abs((cam.y-1.75)-old_feet)>.8){grandstone_obstacle_fail("Grounded stair motion dropped or changed storey");return;}
            if(std::hypot(target.x-cam.x,target.z-cam.z)>.001)return;
        }
        ++result.nodes;
        const double error=std::abs(cam.y-1.75-target.feet);
        result.maximum_node_error=std::max(result.maximum_node_error,error);
        if(error>.7){grandstone_obstacle_fail("Actual feet departed from accepted source path");return;}
        if(!grandstone_obstacle_resample(result)){grandstone_obstacle_fail("Repeated source support queries drifted or lost the current floor");return;}
        if(!grandstone_obstacle_reverse){
            if(++grandstone_obstacle_node<path.count)return;
            result.forward=true;++grandstone_obstacle_directions;grandstone_obstacle_hold=20;
            grandstone_obstacle_phase=2;return;
        }
        if(grandstone_obstacle_node){--grandstone_obstacle_node;return;}
        result.reverse=true;++grandstone_obstacle_directions;++grandstone_obstacle_passed_paths;
        std::printf("[Grandstone obstacle QA] PASS %s forward+reverse %.6f m, %u steps, %u support samples\n",
            world_grandstone_qa::cases[grandstone_obstacle_case].name,result.meters,result.steps,result.samples);std::fflush(stdout);
        ++grandstone_obstacle_case;grandstone_obstacle_phase=0;return;
    }
    if(grandstone_obstacle_phase==2){
        if(grandstone_obstacle_hold){--grandstone_obstacle_hold;return;}
        const auto &path=world_grandstone_qa::paths[grandstone_obstacle_case];
        const auto &look=path.points[path.count-2];
        cam.yaw=float(std::atan2(look.x-cam.x,-(look.z-cam.z)));cam.pitch=-.18f;
        temporal_cut=true;
        if(grandstone_obstacle_capture_dir&&*grandstone_obstacle_capture_dir)
            grandstone_obstacle_capture_pending=world_grandstone_qa::cases[grandstone_obstacle_case].name;
        grandstone_obstacle_reverse=true;grandstone_obstacle_node=unsigned(path.count-2);grandstone_obstacle_phase=3;return;
    }
    if(grandstone_obstacle_phase==4){
        if(grandstone_obstacle_gate==std::size(world_grandstone_qa::gates)*2){grandstone_obstacle_phase=6;return;}
        const auto &gate=world_grandstone_qa::gates[grandstone_obstacle_gate/2];
        const auto &a=grandstone_obstacle_gate%2?gate.goal:gate.start;
        const auto &b=grandstone_obstacle_gate%2?gate.start:gate.goal;
        if(!grandstone_obstacle_setup(gate.section,a,b.x,b.z)){grandstone_obstacle_fail("Closed-gate side lacks a valid floor");return;}
        grandstone_obstacle_phase=5;return;
    }
    if(grandstone_obstacle_phase==5){
        const auto &gate=world_grandstone_qa::gates[grandstone_obstacle_gate/2];
        const auto &b=grandstone_obstacle_gate%2?gate.start:gate.goal;
        const auto motion=move_camera(cam,scenery,b.x,b.z);++grandstone_obstacle_gate_tests;
        if(!motion.collision||motion.actual>.002){grandstone_obstacle_fail("Actual move_camera passed through a visibly closed gate leaf");return;}
        ++grandstone_obstacle_gate_blocks;
        std::printf("[Grandstone obstacle QA] PASS %s side %u: visible solid leaf blocks actual move_camera\n",gate.name,grandstone_obstacle_gate%2);
        ++grandstone_obstacle_gate;grandstone_obstacle_phase=4;return;
    }
    if(grandstone_obstacle_phase==6){
        const auto &point=world_grandstone_qa::paths[0].points[0];
        if(!grandstone_obstacle_setup(1,point,point.x+2,point.z)){grandstone_obstacle_fail("Ordinary jump setup lacks source floor");return;}
        grandstone_obstacle_jump_floor=cam.y-1.75;grandstone_obstacle_jump_apex=grandstone_obstacle_jump_floor;
        world_physics::Hit ceiling;
        if(!active_playtest_walk->raycast({cam.x,cam.y+.05,cam.z},{0,100,0},ceiling)){
            grandstone_obstacle_fail("Actual source ceiling query failed");return;
        }
        grandstone_obstacle_jump_ceiling=ceiling.hit?ceiling.position.y:0;
        grandstone_obstacle_phase=7;return;
    }
    if(grandstone_obstacle_phase==7){
        cam.vy=4.7f;cam.grounded=false;grandstone_obstacle_phase=8;return;
    }
    if(grandstone_obstacle_phase==8){
        const double previous=cam.y;cam.vy-=9.81f/60.f;
        const double requested=cam.y+cam.vy/60.;
        move_camera_vertical(cam,scenery,requested,grandstone_obstacle_jump_floor+1.75);
        ++grandstone_obstacle_jump_steps;
        grandstone_obstacle_jump_apex=std::max(grandstone_obstacle_jump_apex,cam.y-1.75);
        if(requested>previous&&cam.y<requested-.005)grandstone_obstacle_jump_ceiling_contact=true;
        if(grandstone_obstacle_jump_ceiling&&cam.y+.02>grandstone_obstacle_jump_ceiling){grandstone_obstacle_fail("Ordinary jump penetrated a source ceiling");return;}
        if(grandstone_obstacle_jump_steps>180){grandstone_obstacle_fail("Ordinary gravity jump did not land");return;}
        if(!cam.grounded)return;
        grandstone_obstacle_jump_landed=std::abs(cam.y-1.75-grandstone_obstacle_jump_floor)<.03&&
            grandstone_obstacle_jump_apex-grandstone_obstacle_jump_floor>.6;
        if(!grandstone_obstacle_jump_landed){grandstone_obstacle_fail("Ordinary source-floor jump lost apex or landing");return;}
        std::printf("[Grandstone obstacle QA] PASS ordinary gravity jump: rise %.6f m, %u steps, supported landing; ceiling query Y %.6f\n",
            grandstone_obstacle_jump_apex-grandstone_obstacle_jump_floor,grandstone_obstacle_jump_steps,grandstone_obstacle_jump_ceiling);
        grandstone_obstacle_done=true;launch->frames=launch->rendered_frames+1;
        std::printf("[Grandstone obstacle QA] COMPLETE %u paths/%u directions %.6f m, %u gate blocks; %u explicit setup teleports\n",
            grandstone_obstacle_passed_paths,grandstone_obstacle_directions,grandstone_obstacle_walked,grandstone_obstacle_gate_blocks,grandstone_obstacle_setups);
        std::fflush(stdout);return;
    }
    grandstone_obstacle_fail("Invalid obstacle workflow phase");
};
auto grandstone_obstacle_capture=[&](){
    if(!grandstone_obstacle_qa||grandstone_obstacle_capture_pending.empty())return;
    const std::string name=grandstone_obstacle_capture_pending;grandstone_obstacle_capture_pending.clear();
    std::error_code error;const std::filesystem::path directory(grandstone_obstacle_capture_dir);
    std::filesystem::create_directories(directory,error);
    const auto file=directory/(name+".bmp");
    bool ok=!error&&display_w>0&&display_h>0;
    if(ok){
        std::vector<unsigned char> pixels(size_t(display_w)*display_h*3);
        GLint alignment=4,framebuffer=0,read_buffer=0;
        glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&framebuffer);glGetIntegerv(GL_READ_BUFFER,&read_buffer);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,0);glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);
        glReadPixels(0,0,display_w,display_h,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
        glPixelStorei(GL_PACK_ALIGNMENT,alignment);glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(framebuffer));glReadBuffer(GLenum(read_buffer));
        const size_t stride=size_t(display_w)*3;
        for(int row=0;row<display_h/2;++row)std::swap_ranges(pixels.data()+row*stride,pixels.data()+(row+1)*stride,pixels.data()+(display_h-1-row)*stride);
        SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(),display_w,display_h,24,int(stride),SDL_PIXELFORMAT_RGB24);
        ok=surface&&SDL_SaveBMP(surface,file.u8string().c_str())==0;if(surface)SDL_FreeSurface(surface);
    }
    if(!ok){++grandstone_obstacle_capture_failures;grandstone_obstacle_fail("Native final-HUD obstacle capture failed");return;}
    ++grandstone_obstacle_captures;std::printf("[Grandstone obstacle QA] Captured %s\n",file.u8string().c_str());std::fflush(stdout);
};
auto grandstone_obstacle_report=[&](FILE *file){
    std::fprintf(file,"  \"grandstone_obstacle_qa\": {\"enabled\": %s, \"passed\": %s, \"fast_subset\": %s, \"phase\": %u, \"paths_passed\": %u, \"directions_passed\": %u, \"walked_m\": %.9f, \"blocked_steps\": %u, \"slope_steps\": %u, \"dropped_steps\": %u, \"setup_teleports\": %u, \"gate_tests\": %u, \"gate_blocks\": %u, \"near_plane_m\": %.6f, \"captures\": %u, \"capture_failures\": %u, \"jump_landed\": %s, \"jump_steps\": %u, \"jump_rise_m\": %.6f, \"jump_source_ceiling_y\": %.6f, \"jump_contacted_ceiling\": %s, \"failure\": %s, \"cases\": [",
        grandstone_obstacle_qa?"true":"false",grandstone_obstacle_done&&grandstone_obstacle_failure.empty()?"true":"false",
        grandstone_obstacle_fast?"true":"false",grandstone_obstacle_phase,grandstone_obstacle_passed_paths,grandstone_obstacle_directions,
        grandstone_obstacle_walked,grandstone_obstacle_blocked,grandstone_obstacle_slope,grandstone_obstacle_dropped,grandstone_obstacle_setups,
        grandstone_obstacle_gate_tests,grandstone_obstacle_gate_blocks,double(view_near_plane()),grandstone_obstacle_captures,
        grandstone_obstacle_capture_failures,grandstone_obstacle_jump_landed?"true":"false",grandstone_obstacle_jump_steps,
        grandstone_obstacle_jump_apex-grandstone_obstacle_jump_floor,grandstone_obstacle_jump_ceiling,
        grandstone_obstacle_jump_ceiling_contact?"true":"false",world_json::quote(grandstone_obstacle_failure).c_str());
    for(unsigned i=0;i<std::size(world_grandstone_qa::cases);++i){
        const auto &r=grandstone_obstacle_results[i];if(i)std::fputc(',',file);
        std::fprintf(file,"{\"name\": \"%s\", \"selected\": %s, \"forward\": %s, \"reverse\": %s, \"steps\": %u, \"nodes\": %u, \"support_samples\": %u, \"walked_m\": %.9f, \"maximum_node_error_m\": %.9f, \"maximum_resample_drift_m\": %.9f}",
            world_grandstone_qa::cases[i].name,r.selected?"true":"false",r.forward?"true":"false",r.reverse?"true":"false",r.steps,r.nodes,r.samples,r.meters,r.maximum_node_error,r.maximum_resample_drift);
    }
    std::fprintf(file,"], \"source_collision_partitions\": %zu},\n",active_playtest_walk?active_playtest_walk->partitions():0);
};

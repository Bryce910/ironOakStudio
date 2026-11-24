// Included inside Studio. This is a CPU-only, explicitly configured Jolt
// laboratory. Its file/history/preview are independent of world.studio.
    world_physics::Document physics_document;
    std::unique_ptr<world_physics::World> physics_preview;
    std::vector<world_physics::Document> physics_undo,physics_redo;
    world_physics::Id physics_next_id=1;
    std::uint64_t physics_revision=0,physics_preview_revision=0,physics_event_total=0;
    std::array<world_physics::Event,32> physics_recent_events={};
    unsigned physics_event_cursor=0,physics_event_count=0;
    ImGuiID physics_gesture=0;
    int physics_selection=-1;
    bool physics_modified=false,physics_ready=false,physics_running=false;
    bool physics_overlay=true,physics_overlay_live=false;
    unsigned physics_overlay_segments=0,physics_overlay_triangles=0;
    double physics_clock=0;
    float physics_impulse[3]={0,3,0},physics_ray_distance=100;
    world_physics::Hit physics_hit;
    bool physics_has_query=false;
    char physics_file[512]="world.physics";

    bool physics_path(std::string &out){
        const std::filesystem::path relative(physics_file);
        if(relative.empty()||relative.is_absolute()||relative.extension()!=".physics"){
            message="Choose a project-relative .physics filename.";return false;
        }
        for(const auto &part:relative)if(part==".."||part=="."||part.empty()){
            message="Physics paths cannot contain traversal components.";return false;
        }
        if(std::string(physics_file).find_first_of("\r\n\t")!=std::string::npos){message="Invalid physics filename.";return false;}
        std::error_code ec;
        const auto root=std::filesystem::weakly_canonical(project_directory,ec);
        if(ec){message=ec.message();return false;}
        const auto candidate=std::filesystem::weakly_canonical(root/relative,ec);
        if(ec){message=ec.message();return false;}
        const auto within=candidate.lexically_relative(root);
        if(within.empty()||within.is_absolute()) {message="Physics file must remain inside this project.";return false;}
        for(const auto &part:within)if(part==".."){message="Physics file resolves outside this project.";return false;}
        out=(root/relative).string();return true;
    }
    void physics_checkpoint(){
        if(physics_undo.size()==8)physics_undo.erase(physics_undo.begin());
        physics_undo.push_back(physics_document);physics_redo.clear();
    }
    void physics_changed(){
        ++physics_revision;physics_modified=true;physics_running=false;
        physics_clock=0;physics_has_query=false;
    }
    // Snapshot only once for an active widget gesture, before its first change.
    // Scalar/vector edits do not copy collision meshes during untouched frames.
    template<class Value,class Widget> void physics_field(Value &value,Widget widget){
        const Value before=value;
        if(widget()){
            const Value after=value;value=before;
            const ImGuiID id=ImGui::GetItemID();
            if(!ImGui::IsItemActive()||physics_gesture!=id){physics_checkpoint();physics_gesture=id;}
            value=after;physics_changed();
        }
        if(ImGui::IsItemDeactivated())physics_gesture=0;
    }
    void physics_history(bool redo){
        auto &from=redo?physics_redo:physics_undo;auto &to=redo?physics_undo:physics_redo;
        if(from.empty())return;
        if(to.size()==8)to.erase(to.begin());
        to.push_back(std::move(physics_document));physics_document=std::move(from.back());from.pop_back();
        physics_selection=-1;physics_gesture=0;physics_changed();
        message="Physics document history restored. Configure the preview explicitly.";
    }
    void physics_reseed_ids(){
        for(const auto &body:physics_document.bodies){
            if(body.id==std::numeric_limits<world_physics::Id>::max()){physics_next_id=0;return;}
            if(physics_next_id)physics_next_id=std::max(physics_next_id,body.id+1);
        }
    }
    bool physics_stale()const{return !physics_ready||physics_preview_revision!=physics_revision;}
    void physics_configure(){
        if(!physics_preview)physics_preview=std::make_unique<world_physics::World>();
        if(!physics_preview->configure(physics_document,message))return;
        physics_ready=true;physics_running=false;physics_preview_revision=physics_revision;
        physics_event_count=physics_event_cursor=0;physics_event_total=0;physics_clock=0;physics_has_query=false;
        message="Jolt preview configured at the authored poses. It does not move rendered scenery.";
    }
    void physics_collect_events(){
        if(!physics_preview)return;
        std::array<world_physics::Event,256> events;
        for(unsigned batch=0;batch<world_physics::event_limit/events.size();++batch){
            const unsigned count=physics_preview->drain_events(events.data(),unsigned(events.size()));
            for(unsigned i=0;i<count;++i){physics_recent_events[physics_event_cursor]=events[i];physics_event_cursor=(physics_event_cursor+1)%unsigned(physics_recent_events.size());physics_event_count=std::min(physics_event_count+1,unsigned(physics_recent_events.size()));++physics_event_total;}
            if(count<events.size())break;
        }
    }
    void physics_service(double now,bool allow_work){
        if(!std::isfinite(now)||now<0)return;
        if(!allow_work||!physics_running||physics_stale()) {physics_clock=0;return;}
        const double dt=physics_clock>0?std::max(0.,now-physics_clock):0;
        physics_clock=now;
        if(!physics_preview->step(dt)) {physics_running=false;message="Physics preview stopped after an update error. Inspect its capacity/debt counters.";}
        physics_collect_events();
    }
    void physics_add_body(){
        if(!target){message="Choose ground in the viewport before adding a physics body.";return;}
        if(!physics_next_id||physics_document.bodies.size()>=world_physics::body_limit){message="Physics body/ID budget reached.";return;}
        world_physics::Body body;body.id=physics_next_id;
        physics_next_id=physics_next_id==std::numeric_limits<world_physics::Id>::max()?0:physics_next_id+1;
        body.name="Body "+std::to_string(body.id);body.pose.position={x,y+.5,z};
        physics_checkpoint();physics_document.bodies.push_back(std::move(body));
        physics_selection=int(physics_document.bodies.size()-1);physics_gesture=0;physics_changed();
    }
    void physics_remove_body(){
        if(physics_selection<0||unsigned(physics_selection)>=physics_document.bodies.size())return;
        const auto id=physics_document.bodies[unsigned(physics_selection)].id;physics_checkpoint();
        physics_document.doors.erase(std::remove_if(physics_document.doors.begin(),physics_document.doors.end(),[&](const auto &d){return d.body==id;}),physics_document.doors.end());
        physics_document.bodies.erase(physics_document.bodies.begin()+physics_selection);
        physics_selection=-1;physics_gesture=0;physics_changed();
    }
    void physics_body_panel(){
        if(physics_selection<0||unsigned(physics_selection)>=physics_document.bodies.size())return;
        auto &body=physics_document.bodies[unsigned(physics_selection)];
        ImGui::Text("Physics body ID %llu (independent of scene IDs)",(unsigned long long)body.id);
        char name[96];std::snprintf(name,sizeof name,"%s",body.name.c_str());
        physics_field(body.name,[&]{if(!ImGui::InputText("Body name",name,sizeof name))return false;body.name=name;return true;});
        int shape=int(body.shape),motion=int(body.motion),layer=int(body.layer);
        physics_field(body.shape,[&]{if(!ImGui::Combo("Collision shape",&shape,"Box\0Sphere\0Capsule\0Imported static mesh\0"))return false;if(shape==3&&body.vertices.empty()){message="Static meshes must come from a validated physics document; this panel does not cook render meshes.";return false;}body.shape=world_physics::Shape(shape);return true;});
        // Imported mesh arrays remain owned when selecting a primitive, so Undo
        // never discards original collision geometry. Clear explicitly below.
        if(body.shape!=world_physics::Shape::Mesh&&(!body.vertices.empty()||!body.triangles.empty())){
            ImGui::TextWrapped("This primitive still owns imported mesh arrays. Clear them before validation, or switch back to Imported static mesh.");
            if(ImGui::Button("Clear imported collision arrays")){physics_checkpoint();body.vertices.clear();body.triangles.clear();physics_changed();}
        }
        physics_field(body.motion,[&]{if(!ImGui::Combo("Motion",&motion,"Static\0Dynamic\0Kinematic\0"))return false;body.motion=world_physics::Motion(motion);return true;});
        physics_field(body.layer,[&]{if(!ImGui::SliderInt("Collision layer",&layer,0,7))return false;body.layer=unsigned(layer);return true;});
        physics_field(body.sensor,[&]{return ImGui::Checkbox("Sensor volume",&body.sensor);});
        physics_field(body.allow_sleep,[&]{return ImGui::Checkbox("Allow sleeping",&body.allow_sleep);});
        physics_field(body.pose.position.x,[&]{return ImGui::InputDouble("Body east (m)",&body.pose.position.x,.25,5);});
        physics_field(body.pose.position.y,[&]{return ImGui::InputDouble("Body height (m)",&body.pose.position.y,.25,5);});
        physics_field(body.pose.position.z,[&]{return ImGui::InputDouble("Body south (m)",&body.pose.position.z,.25,5);});
        float rotation[]={body.pose.rotation.x,body.pose.rotation.y,body.pose.rotation.z,body.pose.rotation.w};
        physics_field(body.pose.rotation,[&]{if(!ImGui::InputFloat4("Rotation quaternion XYZW",rotation))return false;body.pose.rotation={rotation[0],rotation[1],rotation[2],rotation[3]};return true;});
        if(ImGui::Button("Normalize body rotation")){
            auto q=body.pose.rotation;const double length=std::sqrt(double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w);
            if(!std::isfinite(length)||length<1e-8)message="Enter a finite nonzero rotation quaternion.";
            else{physics_checkpoint();body.pose.rotation={float(q.x/length),float(q.y/length),float(q.z/length),float(q.w/length)};physics_changed();}
        }
        if(body.shape!=world_physics::Shape::Mesh){
            const char *sx=body.shape==world_physics::Shape::Box?"X half extent (m)":"Radius (m)";
            physics_field(body.size.x,[&]{return ImGui::InputDouble(sx,&body.size.x,.05,.5);});
            if(body.shape!=world_physics::Shape::Sphere)physics_field(body.size.y,[&]{return ImGui::InputDouble(body.shape==world_physics::Shape::Box?"Y half extent (m)":"Cylinder half height (m)",&body.size.y,.05,.5);});
            if(body.shape==world_physics::Shape::Box)physics_field(body.size.z,[&]{return ImGui::InputDouble("Z half extent (m)",&body.size.z,.05,.5);});
        }else ImGui::Text("Imported static collision: %zu vertices / %zu triangles",body.vertices.size(),body.triangles.size());
        physics_field(body.friction,[&]{return ImGui::SliderFloat("Friction",&body.friction,0,2);});
        physics_field(body.restitution,[&]{return ImGui::SliderFloat("Restitution",&body.restitution,0,1);});
        if(body.motion!=world_physics::Motion::Static){
            physics_field(body.mass,[&]{return ImGui::InputFloat("Mass kg (0 = density)",&body.mass,1,10);});
            physics_field(body.density,[&]{return ImGui::InputFloat("Density kg/m3",&body.density,10,100);});
            physics_field(body.gravity_factor,[&]{return ImGui::SliderFloat("Gravity factor",&body.gravity_factor,0,4);});
            float velocity[]={float(body.linear_velocity.x),float(body.linear_velocity.y),float(body.linear_velocity.z)};
            physics_field(body.linear_velocity,[&]{if(!ImGui::InputFloat3("Initial linear velocity",velocity))return false;body.linear_velocity={velocity[0],velocity[1],velocity[2]};return true;});
            float angular[]={float(body.angular_velocity.x),float(body.angular_velocity.y),float(body.angular_velocity.z)};
            physics_field(body.angular_velocity,[&]{if(!ImGui::InputFloat3("Initial angular velocity",angular))return false;body.angular_velocity={angular[0],angular[1],angular[2]};return true;});
        }
        auto door=std::find_if(physics_document.doors.begin(),physics_document.doors.end(),[&](const auto &d){return d.body==body.id;});
        if(door==physics_document.doors.end()){
            if(body.motion==world_physics::Motion::Kinematic&&!body.sensor&&body.shape!=world_physics::Shape::Mesh&&physics_document.doors.size()<world_physics::door_limit&&ImGui::Button("Add kinematic door hinge")){
                world_physics::Door hinge;hinge.body=body.id;hinge.pivot=body.pose.position;hinge.pivot.x-=body.size.x;
                physics_checkpoint();physics_document.doors.push_back(hinge);physics_changed();
            }
        }else{
            ImGui::SeparatorText("Kinematic door hinge");
            physics_field(door->pivot.x,[&]{return ImGui::InputDouble("Pivot east",&door->pivot.x,.1,1);});
            physics_field(door->pivot.y,[&]{return ImGui::InputDouble("Pivot height",&door->pivot.y,.1,1);});
            physics_field(door->pivot.z,[&]{return ImGui::InputDouble("Pivot south",&door->pivot.z,.1,1);});
            float axis[]={float(door->axis.x),float(door->axis.y),float(door->axis.z)};
            physics_field(door->axis,[&]{if(!ImGui::InputFloat3("Unit hinge axis",axis))return false;door->axis={axis[0],axis[1],axis[2]};return true;});
            physics_field(door->minimum,[&]{return ImGui::SliderFloat("Minimum angle (radians)",&door->minimum,-6.283185f,0);});
            physics_field(door->maximum,[&]{return ImGui::SliderFloat("Maximum angle (radians)",&door->maximum,0,6.283185f);});
            physics_field(door->speed,[&]{return ImGui::SliderFloat("Door speed rad/s",&door->speed,.01f,10);});
            if(ImGui::Button("Remove door hinge")){physics_checkpoint();physics_document.doors.erase(door);physics_changed();}
        }
        if(ImGui::Button("Delete selected physics body"))physics_remove_body();
    }
    void physics_preview_panel(const Camera &camera){
        ImGui::SeparatorText("Actual Jolt CPU preview");
        if(ImGui::Button("Configure / reset preview"))physics_configure();
        ImGui::BeginDisabled(physics_stale());
        ImGui::SameLine();if(ImGui::Button(physics_running?"Pause physics":"Run physics")){physics_running=!physics_running;physics_clock=0;}
        ImGui::SameLine();if(ImGui::Button("Step 1/60s")){physics_running=false;physics_clock=0;if(!physics_preview->step(world_physics::fixed_step))message="Physics step failed; inspect the error counters.";physics_collect_events();}
        ImGui::SliderFloat("Raycast length (m)",&physics_ray_distance,1,1000);
        if(ImGui::Button("Raycast from camera")){
            const auto direction=camera.forward();
            physics_has_query=physics_preview->raycast({camera.x,camera.y,camera.z},{direction.x*physics_ray_distance,direction.y*physics_ray_distance,direction.z*physics_ray_distance},physics_hit);
            if(!physics_has_query)message="Jolt raycast was rejected.";
        }
        if(physics_selection>=0&&unsigned(physics_selection)<physics_document.bodies.size()){
            const auto &body=physics_document.bodies[unsigned(physics_selection)];world_physics::BodyState state;
            if(physics_preview&&physics_preview->body_state(body.id,state)){
                ImGui::Text("Preview XYZ %.3f / %.3f / %.3f | %s",state.pose.position.x,state.pose.position.y,state.pose.position.z,state.active?"active":"sleeping/static");
                ImGui::Text("Velocity %.3f / %.3f / %.3f",state.linear_velocity.x,state.linear_velocity.y,state.linear_velocity.z);
            }
            ImGui::InputFloat3("Preview impulse (N s)",physics_impulse);
            if(ImGui::Button("Apply preview impulse")&&!physics_preview->impulse(body.id,{physics_impulse[0],physics_impulse[1],physics_impulse[2]}))message="Impulse needs a valid dynamic body and finite values.";
            auto hinge=std::find_if(physics_document.doors.begin(),physics_document.doors.end(),[&](const auto &d){return d.body==body.id;});
            if(hinge!=physics_document.doors.end()){
                if(ImGui::Button("Preview open door"))physics_preview->door_target(body.id,hinge->maximum);
                ImGui::SameLine();if(ImGui::Button("Preview close door"))physics_preview->door_target(body.id,hinge->minimum);
            }
        }
        ImGui::EndDisabled();
        if(physics_stale())ImGui::TextWrapped("The preview is unconfigured or has an older document. Configure/reset to preview these edits.");
        if(physics_has_query){if(physics_hit.hit)ImGui::Text("Ray hit body %llu at %.3f / %.3f / %.3f; fraction %.4f",(unsigned long long)physics_hit.body,physics_hit.position.x,physics_hit.position.y,physics_hit.position.z,physics_hit.fraction);else ImGui::TextUnformatted("Raycast completed: no collision hit.");}
        if(physics_ready){
            const auto stats=physics_preview->stats();
            ImGui::Text("Bodies %u / %u | active %u | sensors %u / %u",stats.bodies,world_physics::body_limit,stats.active_bodies,stats.sensors,world_physics::sensor_limit);
            ImGui::Text("Tick %llu | substeps %u | dropped debt %.4fs",(unsigned long long)stats.ticks,stats.substeps,stats.dropped_seconds);
            ImGui::Text("Errors %llu | event overflow %llu | pair overflow %llu",(unsigned long long)stats.update_errors,(unsigned long long)stats.event_overflows,(unsigned long long)stats.pair_overflows);
            ImGui::Text("Heap fallback requests: update %llu / query %llu",(unsigned long long)stats.total_frame_allocations,(unsigned long long)stats.query_allocations);
            ImGui::Text("Events observed %llu | retained latest %u",(unsigned long long)physics_event_total,physics_event_count);
            if(ImGui::TreeNode("Latest contacts / triggers")){
                static const char *kinds[]={"Contact begin","Contact end","Trigger enter","Trigger exit"};
                for(unsigned i=0;i<physics_event_count;++i){const auto &event=physics_recent_events[(physics_event_cursor+unsigned(physics_recent_events.size())-physics_event_count+i)%unsigned(physics_recent_events.size())];ImGui::Text("%s | %llu -> %llu | tick %llu",kinds[unsigned(event.kind)],(unsigned long long)event.a,(unsigned long long)event.b,(unsigned long long)event.tick);}
                ImGui::TreePop();
            }
        }
    }
    // Debug-only projection of the selected collider. Keep rotation and world
    // translation in double precision; camera-relative conversion is owned by
    // the normal viewport projector. This does not move or skin scene models.
    bool physics_overlay_pose(world_physics::Pose &pose){
        physics_overlay_live=false;
        if(physics_selection<0||unsigned(physics_selection)>=physics_document.bodies.size())return false;
        const auto &body=physics_document.bodies[unsigned(physics_selection)];pose=body.pose;
        if(!physics_stale()&&physics_preview){world_physics::BodyState state;
            if(physics_preview->body_state(body.id,state)){pose=state.pose;physics_overlay_live=true;}
        }
        const auto &q=pose.rotation;
        const double length=std::sqrt(double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w);
        if(!std::isfinite(pose.position.x)||!std::isfinite(pose.position.y)||!std::isfinite(pose.position.z)||
           !std::isfinite(length)||std::abs(length-1)>=.01||std::abs(q.x)>1||std::abs(q.y)>1||std::abs(q.z)>1||std::abs(q.w)>1)return false;
        // The adapter normalizes accepted authored quaternions before Jolt.
        pose.rotation={float(q.x/length),float(q.y/length),float(q.z/length),float(q.w/length)};
        return true;
    }
    static world_studio::Vec physics_overlay_point(const world_physics::Pose &pose,world_physics::Vec3 local){
        const auto &q=pose.rotation;
        const double tx=2*(double(q.y)*local.z-double(q.z)*local.y),
                     ty=2*(double(q.z)*local.x-double(q.x)*local.z),
                     tz=2*(double(q.x)*local.y-double(q.y)*local.x);
        return {pose.position.x+local.x+double(q.w)*tx+double(q.y)*tz-double(q.z)*ty,
                pose.position.y+local.y+double(q.w)*ty+double(q.z)*tx-double(q.x)*tz,
                pose.position.z+local.z+double(q.w)*tz+double(q.x)*ty-double(q.y)*tx};
    }
    void physics_draw_overlay(const Camera &camera){
        physics_overlay_segments=physics_overlay_triangles=0;physics_overlay_live=false;
        if(!physics_overlay||physics_selection<0||unsigned(physics_selection)>=physics_document.bodies.size())return;
        world_physics::Pose pose;if(!physics_overlay_pose(pose))return;
        const auto &body=physics_document.bodies[unsigned(physics_selection)];
        auto *draw=ImGui::GetBackgroundDrawList();
        const ImU32 color=body.sensor?IM_COL32(96,222,249,235):physics_overlay_live?IM_COL32(107,239,139,235):IM_COL32(255,201,101,235);
        auto line=[&](world_physics::Vec3 a,world_physics::Vec3 b){
            const auto wa=physics_overlay_point(pose,a),wb=physics_overlay_point(pose,b);
            auto finite=[](world_studio::Vec p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);};
            if(!finite(wa)||!finite(wb))return;
            ImVec2 pa,pb;if(!project_point(camera,wa,pa)||!project_point(camera,wb,pb))return;
            auto screen=[](ImVec2 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::abs(p.x)<1e6f&&std::abs(p.y)<1e6f;};
            if(!screen(pa)||!screen(pb))return;
            draw->AddLine(pa,pb,color,1.5f);++physics_overlay_segments;
        };
        constexpr unsigned ring_segments=32;constexpr double pi=3.14159265358979323846;
        auto dimension=[](double value){return std::isfinite(value)&&value>=.01&&value<=1000;};
        if(body.shape==world_physics::Shape::Box){
            if(!dimension(body.size.x)||!dimension(body.size.y)||!dimension(body.size.z))return;
            std::array<world_physics::Vec3,8> corners;
            for(unsigned i=0;i<corners.size();++i)corners[i]={(i&1?1:-1)*body.size.x,(i&2?1:-1)*body.size.y,(i&4?1:-1)*body.size.z};
            for(unsigned i=0;i<corners.size();++i)for(unsigned axis:{1u,2u,4u})if(!(i&axis))line(corners[i],corners[i|axis]);
        }else if(body.shape==world_physics::Shape::Sphere){
            if(!dimension(body.size.x))return;
            const double radius=body.size.x;
            for(unsigned plane=0;plane<3;++plane)for(unsigned i=0;i<ring_segments;++i){
                const double a=2*pi*i/ring_segments,b=2*pi*(i+1)/ring_segments;
                auto point=[&](double angle)->world_physics::Vec3{const double c=radius*std::cos(angle),s=radius*std::sin(angle);return plane==0?world_physics::Vec3{c,s,0}:plane==1?world_physics::Vec3{c,0,s}:world_physics::Vec3{0,c,s};};
                line(point(a),point(b));
            }
        }else if(body.shape==world_physics::Shape::Capsule){
            if(!dimension(body.size.x)||!dimension(body.size.y))return;
            const double radius=body.size.x,half_height=body.size.y;
            // The cylinder's two equators and four spines, then two orthogonal
            // pairs of hemispheres. Local Y matches Jolt CapsuleShapeSettings.
            for(double side:{-1.,1.})for(unsigned i=0;i<ring_segments;++i){
                const double a=2*pi*i/ring_segments,b=2*pi*(i+1)/ring_segments;
                line({radius*std::cos(a),side*half_height,radius*std::sin(a)},
                     {radius*std::cos(b),side*half_height,radius*std::sin(b)});
            }
            for(unsigned i=0;i<4;++i){const double angle=pi*i*.5;
                line({radius*std::cos(angle),-half_height,radius*std::sin(angle)},
                     {radius*std::cos(angle),half_height,radius*std::sin(angle)});
            }
            for(unsigned plane=0;plane<2;++plane)for(double side:{-1.,1.})for(unsigned i=0;i<ring_segments/2;++i){
                auto point=[&](double angle)->world_physics::Vec3{const double c=radius*std::cos(angle),y=side*(half_height+radius*std::sin(angle));return plane==0?world_physics::Vec3{c,y,0}:world_physics::Vec3{0,y,c};};
                line(point(2*pi*i/ring_segments),point(2*pi*(i+1)/ring_segments));
            }
        }else if(body.shape==world_physics::Shape::Mesh){
            const auto count=std::min<std::size_t>(128,body.triangles.size());
            for(std::size_t i=0;i<count;++i){const auto triangle=body.triangles[i];
                if(triangle[0]>=body.vertices.size()||triangle[1]>=body.vertices.size()||triangle[2]>=body.vertices.size())continue;
                for(unsigned edge=0;edge<3;++edge)line(body.vertices[triangle[edge]],body.vertices[triangle[(edge+1)%3]]);
                ++physics_overlay_triangles;
            }
        }
    }
    void physics_panel(const Camera &camera){
        if(!ImGui::BeginTabItem("Physics",nullptr,requested_tab==20?ImGuiTabItemFlags_SetSelected:0)){physics_clock=0;return;}
        tab=20;
        ImGui::TextWrapped("Author collision bodies, eight collision layers and kinematic hinges. This independent .physics document has its own eight-step Undo/Redo and explicit file save; scene Undo and Apply do not modify it.");
        const bool play_locked=production_playing()||production_freeze();
        if(play_locked)ImGui::TextWrapped("Stop scene Play before editing or running the independent physics preview.");
        ImGui::BeginDisabled(play_locked);
        ImGui::InputText("Physics file (project relative)",physics_file,sizeof physics_file);
        if(ImGui::Button("Load physics file")){
            std::string filename;world_physics::Document loaded;
            if(physics_path(filename)&&world_physics::load(filename.c_str(),loaded,message)){
                physics_checkpoint();physics_document=std::move(loaded);physics_selection=-1;physics_gesture=0;
                physics_reseed_ids();physics_changed();physics_modified=false;message="Physics file loaded. Configure the independent preview explicitly.";
            }
        }
        ImGui::SameLine();if(ImGui::Button("Save physics file")){
            std::string filename;
            if(physics_path(filename)){
                std::error_code ec;std::filesystem::create_directories(std::filesystem::path(filename).parent_path(),ec);
                if(ec)message=ec.message();else if(world_physics::save(filename.c_str(),physics_document,message)){physics_modified=false;message="Physics document saved separately from world.studio.";}
            }
        }
        ImGui::SameLine();ImGui::TextUnformatted(physics_modified?"Unsaved physics edits":"Physics file unchanged");
        ImGui::BeginDisabled(physics_undo.empty());if(ImGui::Button("Undo physics"))physics_history(false);ImGui::EndDisabled();
        ImGui::SameLine();ImGui::BeginDisabled(physics_redo.empty());if(ImGui::Button("Redo physics"))physics_history(true);ImGui::EndDisabled();
        physics_field(physics_document.gravity.x,[&]{return ImGui::InputDouble("Gravity X",&physics_document.gravity.x,.1,1);});
        physics_field(physics_document.gravity.y,[&]{return ImGui::InputDouble("Gravity Y",&physics_document.gravity.y,.1,1);});
        physics_field(physics_document.gravity.z,[&]{return ImGui::InputDouble("Gravity Z",&physics_document.gravity.z,.1,1);});
        if(ImGui::TreeNode("Collision masks (both layers must allow contact)")){
            for(unsigned layer=0;layer<world_physics::layer_count;++layer){
                ImGui::PushID(int(layer));int mask=int(physics_document.layer_masks[layer]);
                physics_field(physics_document.layer_masks[layer],[&]{if(!ImGui::SliderInt("Layer mask bits",&mask,0,255))return false;physics_document.layer_masks[layer]=unsigned(mask);return true;});
                ImGui::SameLine();ImGui::Text("layer %u",layer);ImGui::PopID();
            }
            ImGui::TreePop();
        }
        if(ImGui::Button("Add box body at picked ground"))physics_add_body();
        ImGui::BeginChild("Physics bodies",{0,120},true);
        for(unsigned i=0;i<physics_document.bodies.size();++i){const auto &body=physics_document.bodies[i];ImGui::PushID(int(i));if(ImGui::Selectable((std::to_string(body.id)+" | "+body.name).c_str(),physics_selection==int(i))){physics_selection=int(i);physics_gesture=0;}ImGui::PopID();}
        ImGui::EndChild();
        physics_body_panel();physics_preview_panel(camera);
        physics_service(production_now(),!play_locked);
        ImGui::Checkbox("Selected collider wireframe",&physics_overlay);physics_draw_overlay(camera);
        if(physics_overlay&&physics_selection>=0)ImGui::Text("Collider overlay: %s | %u projected edges",physics_overlay_live?"current Jolt preview pose":"authored pose",physics_overlay_segments);
        ImGui::TextWrapped("The selected collider overlay is a debug drawing without depth occlusion: lines show through scenery. Mesh overlays draw at most the first 128 triangles. Invalid dimensions/rotations or off-view edges can suppress the drawing.");
        ImGui::TextWrapped("The preview runs actual Jolt dynamics and queries on isolated CPU bodies. It does not move rendered scenery, bind scene entity IDs, replace character collision, or join scene Play/Stop snapshots. Use Configure/reset after document edits. Imported triangle meshes must be static and are validated on load/configure/save. Dynamic motion is bounded to four 60 Hz substeps per frame; dropped time and overflow are shown above.");
        ImGui::EndDisabled();
        ImGui::EndTabItem();
    }

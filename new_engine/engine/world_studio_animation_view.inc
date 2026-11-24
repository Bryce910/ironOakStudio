// Include inside Studio after tools_view; viewer must include world_animation.h.
// Call animation_panel(cam) inside the existing ImGui tab bar. This inspector
// owns only CPU data. Loading and full-mesh bounds measurement are explicit;
// active-tab sampling and the <=256-node overlay perform no asset IO.
    world_animation::Asset animation_asset;
    world_animation::Pose animation_pose;
    world_animation::Playback animation_playback;
    world_animation::EventBuffer animation_events;
    world_animation::Socket animation_socket;
    world_animation::Mat4 animation_socket_matrix;
    std::array<bool,world_animation::node_limit> animation_palette_nodes={};
    world_animation::Vec3 animation_mesh_min,animation_mesh_max,animation_deformed_min,animation_deformed_max;
    bool animation_ready=false,animation_paused=true,animation_loop=true,animation_overlay=true;
    bool animation_origin_ready=false,animation_socket_ready=false,animation_measured_bounds=false;
    double animation_origin_x=0,animation_origin_y=0,animation_origin_z=0,animation_last_clock=0,animation_bounds_time=0;
    int animation_last_frame=-2,animation_mode=0,animation_target=0,animation_secondary=0,animation_joint=0;
    float animation_rate=1,animation_mix=.5f,animation_fade=.25f,animation_preview_scale=1,animation_preview_yaw=0;
    std::size_t animation_total_keys=0,animation_total_channels=0;
    char animation_file[1024]="models/animations/white_knight_brown_armor.glb",animation_socket_name[256]={};
    std::string animation_error,animation_sample_error,animation_socket_error,animation_loaded_file;

    bool animation_resolve_file(std::filesystem::path &resolved,std::string &error)const{
        const std::filesystem::path requested(animation_file);
        if(requested.empty()||requested.is_absolute()||requested.has_root_name()){
            error="Choose a project-relative embedded .glb file.";return false;
        }
        for(const auto &part:requested)if(part==".."){
            error="Animation paths cannot leave the project directory.";return false;
        }
        auto extension=requested.extension().string();
        std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return char(std::tolower(c));});
        if(extension!=".glb"){error="The skeletal importer accepts embedded binary glTF (.glb).";return false;}
        std::error_code ec;const auto root=std::filesystem::canonical(project_directory,ec);
        if(ec){error="Cannot resolve the current project directory.";return false;}
        resolved=std::filesystem::canonical(root/requested,ec);
        if(ec){error="Animation file does not exist or cannot be read.";return false;}
        const auto relative=resolved.lexically_relative(root);
        if(relative.empty()||relative.is_absolute()||*relative.begin()==".."){
            error="The resolved animation file must stay inside this project; copy linked source assets into the project first.";return false;
        }
        if(!std::filesystem::is_regular_file(resolved,ec)||ec){error="Animation source must be a regular file.";return false;}
        const auto size=std::filesystem::file_size(resolved,ec);
        if(ec||!size||size>32u*1024u*1024u){error="Embedded animation GLB must be nonempty and at most 32 MiB.";return false;}
        return true;
    }
    static void animation_extend_bounds(world_animation::Vec3 p,world_animation::Vec3 &minimum,world_animation::Vec3 &maximum){
        minimum.x=std::min(minimum.x,p.x);minimum.y=std::min(minimum.y,p.y);minimum.z=std::min(minimum.z,p.z);
        maximum.x=std::max(maximum.x,p.x);maximum.y=std::max(maximum.y,p.y);maximum.z=std::max(maximum.z,p.z);
    }
    void animation_load(){
        std::filesystem::path file;if(!animation_resolve_file(file,animation_error))return;
        world_animation::Asset candidate;world_animation::Pose pose;
        if(!world_animation::load_glb(file.u8string().c_str(),candidate,animation_error)||
           !world_animation::validate(candidate,animation_error)||
           !world_animation::rest_pose(candidate.skeleton,pose,animation_error))return;
        if(!candidate.clips.empty()&&!world_animation::sample(candidate.skeleton,candidate.clips[0],0,true,pose,animation_error))return;
        world_animation::Vec3 minimum{FLT_MAX,FLT_MAX,FLT_MAX},maximum{-FLT_MAX,-FLT_MAX,-FLT_MAX};
        for(const auto &vertex:candidate.mesh.vertices)animation_extend_bounds(vertex.position,minimum,maximum);
        if(candidate.mesh.vertices.empty())minimum=maximum={};
        animation_asset=std::move(candidate);animation_pose=pose;animation_ready=true;
        animation_mesh_min=minimum;animation_mesh_max=maximum;animation_playback={};animation_events={};
        animation_target=animation_secondary=animation_joint=animation_mode=0;animation_paused=true;animation_loop=true;
        animation_socket_ready=animation_measured_bounds=false;animation_socket={};animation_socket_name[0]=0;animation_socket_error.clear();
        animation_loaded_file=animation_file;animation_last_frame=-2;animation_palette_nodes.fill(false);
        animation_total_keys=animation_total_channels=0;
        for(const auto &clip:animation_asset.clips){animation_total_channels+=clip.channels.size();for(const auto &channel:clip.channels)animation_total_keys+=channel.times.size();}
        for(unsigned node:animation_asset.skeleton.palette)animation_palette_nodes[node]=true;
        if(target){animation_origin_x=x;animation_origin_y=y+.04;animation_origin_z=z;animation_origin_ready=true;}
        animation_error.clear();animation_sample_error.clear();message="Skeletal asset validated. Inspect real animation clips and the bone overlay in Animation.";
    }
    bool animation_sample_preview(double delta){
        if(!animation_ready)return false;
        if(animation_asset.clips.empty())return true;
        if(animation_mode==1){
            const unsigned a=unsigned(animation_target),b=unsigned(animation_secondary);
            const double duration=animation_asset.clips[a].duration,time=animation_playback.time+delta;
            const double phase=duration>0?(animation_loop?std::fmod(time,duration):std::min(time,duration))/duration:0;
            const world_animation::Blend inputs[]={{a,phase*duration,1-animation_mix},{b,phase*animation_asset.clips[b].duration,animation_mix}};
            if(!world_animation::blend(animation_asset,inputs,2,animation_loop,animation_pose,animation_sample_error))return false;
            animation_playback.time=time;animation_playback.clip=a;animation_playback.loop=animation_loop;animation_events={};return true;
        }
        animation_playback.loop=animation_loop;
        return world_animation::advance(animation_asset,animation_playback,delta,animation_pose,animation_events,animation_sample_error);
    }
    void animation_measure_bounds(){
        if(!animation_ready)return;
        world_animation::Vec3 minimum{FLT_MAX,FLT_MAX,FLT_MAX},maximum{-FLT_MAX,-FLT_MAX,-FLT_MAX};
        for(const auto &vertex:animation_asset.mesh.vertices){world_animation::Vec3 position,normal;
            if(!world_animation::deform_vertex(vertex,animation_pose,position,normal,animation_error))return;
            animation_extend_bounds(position,minimum,maximum);
        }
        if(animation_asset.mesh.vertices.empty())minimum=maximum={};
        animation_deformed_min=minimum;animation_deformed_max=maximum;animation_bounds_time=animation_playback.time;animation_measured_bounds=true;
    }
    world_studio::Vec animation_world_point(world_animation::Vec3 point)const{
        const double angle=animation_preview_yaw*.017453292519943295,c=std::cos(angle),s=std::sin(angle),scale=animation_preview_scale;
        return {animation_origin_x+scale*(c*point.x+s*point.z),animation_origin_y+scale*point.y,animation_origin_z+scale*(-s*point.x+c*point.z)};
    }
    bool animation_project(const Camera &camera,world_animation::Vec3 point,ImVec2 &screen)const{
        const auto world=animation_world_point(point);
        if(!std::isfinite(world.x)||!std::isfinite(world.y)||!std::isfinite(world.z)||
           std::abs(world.x-camera.x)>1000||std::abs(world.y-camera.y)>1000||std::abs(world.z-camera.z)>1000)return false;
        return project_point(camera,world,screen);
    }
    void animation_draw_overlay(const Camera &camera){
        if(!animation_ready||!animation_overlay||!animation_origin_ready)return;
        auto *draw=ImGui::GetBackgroundDrawList();
        for(unsigned i=0;i<animation_pose.nodes;i++){
            const auto p=world_animation::transform_point(animation_pose.global[i],{});ImVec2 at;
            if(!animation_project(camera,p,at))continue;
            const bool selected=int(i)==animation_joint;const ImU32 color=selected?IM_COL32(83,238,245,255):
                animation_palette_nodes[i]?IM_COL32(255,207,103,225):IM_COL32(164,173,183,145);
            const int parent=animation_asset.skeleton.joints[i].parent;ImVec2 from;
            if(parent>=0&&animation_project(camera,world_animation::transform_point(animation_pose.global[unsigned(parent)],{}),from))draw->AddLine(from,at,color,selected?2.5f:1.5f);
            draw->AddCircleFilled(at,selected?4.f:2.f,color);
            if(selected)draw->AddText({at.x+7,at.y},color,animation_asset.skeleton.joints[i].name.c_str());
        }
        if(animation_socket_ready){
            const auto origin=world_animation::transform_point(animation_socket_matrix,{});ImVec2 at;
            if(animation_project(camera,origin,at)){
                const world_animation::Vec3 axes[]={{.2f,0,0},{0,.2f,0},{0,0,.2f}};
                const ImU32 colors[]={IM_COL32(255,90,80,255),IM_COL32(98,230,120,255),IM_COL32(90,160,255,255)};
                for(unsigned i=0;i<3;i++){ImVec2 end;if(animation_project(camera,world_animation::transform_point(animation_socket_matrix,axes[i]),end))draw->AddLine(at,end,colors[i],2);}
                draw->AddCircle(at,6,IM_COL32(255,255,255,245),12,2);draw->AddText({at.x+8,at.y+8},IM_COL32(255,255,255,245),"Named socket");
            }
        }
    }
    void animation_clip_combo(const char *label,int &selection){
        const auto &clips=animation_asset.clips;if(clips.empty())return;
        selection=std::clamp(selection,0,int(clips.size())-1);
        if(ImGui::BeginCombo(label,clips[unsigned(selection)].name.c_str())){
            for(unsigned i=0;i<clips.size();i++){ImGui::PushID(int(i));char item[256];std::snprintf(item,sizeof item,"%u | %s | %.3fs",i,clips[i].name.c_str(),clips[i].duration);
                if(ImGui::Selectable(item,selection==int(i)))selection=int(i);
                ImGui::PopID();
            }ImGui::EndCombo();
        }
    }
    void animation_panel(const Camera &camera){
        if(!ImGui::BeginTabItem("Animation",nullptr,requested_tab==21?ImGuiTabItemFlags_SetSelected:0))return;
        tab=21;
        ImGui::TextWrapped("Inspect an imported skeletal GLB: real rig, clip samples, phase-synchronized blends, crossfades and named sockets. Existing demo crowds still use their current renderer; this CPU bone preview does not attach GPU skinning to them.");
        ImGui::InputText("Skeletal GLB (project relative)",animation_file,sizeof animation_file);
        if(ImGui::Button("Load / validate skeletal asset"))animation_load();
        ImGui::SameLine();if(animation_ready&&ImGui::Button("Release preview data")){animation_asset={};animation_ready=animation_socket_ready=animation_measured_bounds=false;animation_error.clear();animation_sample_error.clear();}
        if(!animation_error.empty())ImGui::TextWrapped("Animation: %s. Previous valid asset and pose are retained when an operation fails.",animation_error.c_str());
        if(!animation_sample_error.empty())ImGui::TextWrapped("Sampling: %s. Preview paused at its last valid pose.",animation_sample_error.c_str());
        if(!animation_ready){ImGui::TextWrapped("Use an embedded GLB with an actual skin and animation tracks. Baked static pose GLBs do not contain a reusable skeleton. Limits: 256 nodes,128 skin joints,64 clips,32 MiB source.");ImGui::EndTabItem();return;}
        const auto &skeleton=animation_asset.skeleton;
        ImGui::TextWrapped("Loaded: %s",animation_loaded_file.c_str());
        ImGui::Text("%zu nodes / %zu skin joints | %zu clips | %zu channels / %zu keys",skeleton.joints.size(),skeleton.palette.size(),animation_asset.clips.size(),animation_total_channels,animation_total_keys);
        ImGui::Text("%zu vertices / %zu triangles | %zu surfaces | %zu materials / %zu embedded images",animation_asset.mesh.vertices.size(),animation_asset.mesh.indices.size()/3,animation_asset.mesh.primitives.size(),animation_asset.materials.size(),animation_asset.images.size());
        ImGui::Text("Mesh-local imported bounds: [%.3f %.3f %.3f] to [%.3f %.3f %.3f]",animation_mesh_min.x,animation_mesh_min.y,animation_mesh_min.z,animation_mesh_max.x,animation_mesh_max.y,animation_mesh_max.z);
        const int frame=ImGui::GetFrameCount();const double now=ImGui::GetTime();
        const double elapsed=animation_last_frame==frame-1?std::clamp(now-animation_last_clock,0.,.1):0;
        animation_last_frame=frame;animation_last_clock=now;
        bool refresh=false;double step=0;
        if(!animation_asset.clips.empty()){
            if(ImGui::Combo("Preview mode",&animation_mode,"Clip / crossfade\0Two-clip blend\0")){
                animation_playback.fading=animation_playback.source_frozen=false;animation_playback.clip=unsigned(animation_target);refresh=true;
            }
            const int prior_target=animation_target;animation_clip_combo(animation_mode?"Blend clip A":"Transition target",animation_target);
            if(animation_mode==1){const int prior_secondary=animation_secondary;animation_clip_combo("Blend clip B",animation_secondary);refresh|=prior_secondary!=animation_secondary;refresh|=ImGui::SliderFloat("Clip B weight",&animation_mix,0,1);
                if(prior_target!=animation_target){animation_playback.time=0;refresh=true;}
                ImGui::TextWrapped("Both clips use the same normalized phase, with their actual authored durations.");
            }else{
                ImGui::SliderFloat("Crossfade seconds",&animation_fade,0,2);
                if(ImGui::Button("Crossfade to selected clip")){
                    if(world_animation::transition(animation_asset,animation_playback,unsigned(animation_target),animation_fade,animation_error,true))refresh=true;
                }
                const auto &active=animation_asset.clips[animation_playback.clip];ImGui::Text("Playing: %s%s",active.name.c_str(),animation_playback.fading?" | crossfading":"");
                if(animation_playback.fading)ImGui::Text("Fade %.3f / %.3fs",animation_playback.fade_elapsed,animation_playback.fade_duration);
            }
            ImGui::Checkbox("Pause preview",&animation_paused);ImGui::SameLine();refresh|=ImGui::Checkbox("Loop clips",&animation_loop);
            ImGui::SliderFloat("Playback rate",&animation_rate,.05f,2.f);
            const double duration=animation_asset.clips[animation_mode?unsigned(animation_target):animation_playback.clip].duration;
            float time=duration>0?float(animation_loop?std::fmod(animation_playback.time,duration):std::min(animation_playback.time,duration)):0;
            if(duration>0&&ImGui::SliderFloat("Clip time (s)",&time,0,float(duration),"%.3f")){
                animation_playback.time=time;animation_playback.fading=animation_playback.source_frozen=false;animation_paused=true;refresh=true;
            }
            if(ImGui::Button("Restart preview")){animation_playback.time=0;animation_playback.fading=animation_playback.source_frozen=false;refresh=true;}
            ImGui::SameLine();if(ImGui::Button("Step 1/60 s")){animation_paused=true;step=1./60.;}
            if(!animation_paused)step=elapsed*animation_rate;
            if(refresh||step>0)if(!animation_sample_preview(step))animation_paused=true;
            if(!animation_loop&&duration>0&&animation_playback.time>=duration)animation_paused=true;
            ImGui::Text("Preview time %.3fs | emitted events %u | dropped %llu",animation_playback.time,animation_events.count,(unsigned long long)animation_events.dropped);
            if(animation_mode==0)for(unsigned i=0;i<animation_events.count;i++){
                const auto &hit=animation_events.hits[i];const auto &events=animation_asset.clips[animation_playback.clip].events;
                if(hit.index<events.size())ImGui::Text("%s @ %.3fs",events[hit.index].name.c_str(),hit.absolute_time);
            }
        }else ImGui::TextWrapped("This asset has a skin and rest pose but no animation clips.");
        if(ImGui::Button("Measure current deformed mesh bounds"))animation_measure_bounds();
        if(animation_measured_bounds)ImGui::Text("Measured mesh-local bounds at %.3fs: [%.3f %.3f %.3f] to [%.3f %.3f %.3f]",animation_bounds_time,animation_deformed_min.x,animation_deformed_min.y,animation_deformed_min.z,animation_deformed_max.x,animation_deformed_max.y,animation_deformed_max.z);
        ImGui::Separator();
        ImGui::Checkbox("Draw rig / socket over world",&animation_overlay);
        if(ImGui::Button("Use picked ground as preview origin")){
            if(target){animation_origin_x=x;animation_origin_y=y+.04;animation_origin_z=z;animation_origin_ready=true;}else message="Pick ground in the viewport first.";
        }
        if(animation_origin_ready)ImGui::Text("Preview origin %.2f %.2f %.2f",animation_origin_x,animation_origin_y,animation_origin_z);
        ImGui::SliderFloat("Preview uniform scale",&animation_preview_scale,.001f,100.f,"%.3f",ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Preview yaw",&animation_preview_yaw,-180,180);
        animation_joint=std::clamp(animation_joint,0,int(skeleton.joints.size())-1);
        if(ImGui::BeginCombo("Rig node",skeleton.joints[unsigned(animation_joint)].name.c_str())){
            for(unsigned i=0;i<skeleton.joints.size();i++){ImGui::PushID(int(i));char label[320];std::snprintf(label,sizeof label,"%u | %s%s",i,skeleton.joints[i].name.c_str(),animation_palette_nodes[i]?" [skin joint]":" [helper]");if(ImGui::Selectable(label,animation_joint==int(i)))animation_joint=int(i);ImGui::PopID();}ImGui::EndCombo();
        }
        const auto &local=animation_pose.local[unsigned(animation_joint)];const auto global=world_animation::transform_point(animation_pose.global[unsigned(animation_joint)],{});
        ImGui::Text("Parent %d | global %.3f %.3f %.3f",skeleton.joints[unsigned(animation_joint)].parent,global.x,global.y,global.z);
        ImGui::Text("Local position %.3f %.3f %.3f | scale %.3f %.3f %.3f",local.translation.x,local.translation.y,local.translation.z,local.scale.x,local.scale.y,local.scale.z);
        ImGui::Text("Local quaternion %.4f %.4f %.4f %.4f",local.rotation.x,local.rotation.y,local.rotation.z,local.rotation.w);
        if(ImGui::Button("Use selected node for named socket"))std::snprintf(animation_socket_name,sizeof animation_socket_name,"%s",skeleton.joints[unsigned(animation_joint)].name.c_str());
        ImGui::InputText("Socket joint name",animation_socket_name,sizeof animation_socket_name);
        float offset[]={animation_socket.local.translation.x,animation_socket.local.translation.y,animation_socket.local.translation.z};
        if(ImGui::InputFloat3("Socket local offset",offset)){animation_socket.local.translation={offset[0],offset[1],offset[2]};}
        if(ImGui::Button("Resolve named socket")){animation_socket.joint=animation_socket_name;animation_socket_ready=world_animation::socket_transform(skeleton,animation_pose,animation_socket,animation_socket_matrix,animation_socket_error);}
        if(animation_socket_ready)animation_socket_ready=world_animation::socket_transform(skeleton,animation_pose,animation_socket,animation_socket_matrix,animation_socket_error);
        if(!animation_socket_error.empty())ImGui::TextWrapped("Socket: %s",animation_socket_error.c_str());
        if(animation_socket_ready){const auto position=world_animation::transform_point(animation_socket_matrix,{});ImGui::Text("Resolved socket global %.3f %.3f %.3f",position.x,position.y,position.z);}
        ImGui::TextWrapped("Imported axes and units are preserved. Gold bones are skin joints; gray bones are helper nodes. The overlay uses the current sampled pose and named socket transform, with no guessed retarget mapping. Preview time pauses while this tab is hidden; frame deltas are capped at0.1s before playback rate. Full-mesh bounds run only when requested.");
        animation_draw_overlay(camera);
        ImGui::EndTabItem();
    }

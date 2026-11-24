    world_cinematics::Document cinematic_document;
    world_cinematics::Transport cinematic_transport;
    world_cinematics::Step cinematic_step;
    Camera cinematic_return;
    bool cinematic_preview=false,cinematic_restore_camera=false,cinematic_cut=false;
    int cinematic_selection=-1;float cinematic_key_time=0,cinematic_duration=20;
    char cinematic_file[1024]="cinematics/demo.vct";
    world_cinematics_audio::Audio *cinematic_audio=nullptr;
    world_cinematics::Transport cinematic_audio_clock;
    bool cinematic_audio_ready=false,cinematic_audio_active=false,cinematic_audio_was_preview=false,cinematic_audio_focused=true;
    int cinematic_zone=-1;char cinematic_audio_asset[512]="audio/demo/forest.wav";
    void cinematic_audio_shutdown(){world_cinematics_audio::destroy(cinematic_audio);cinematic_audio=nullptr;cinematic_audio_ready=cinematic_audio_active=false;}
    float cinematic_outside()const{return cinematic_audio_ready&&cinematic_audio_active?world_cinematics_audio::stats(cinematic_audio).outside_gain:1;}
    void cinematic_audio_prepare(){
        if(!cinematic_audio){cinematic_audio=world_cinematics_audio::create({},message);if(!cinematic_audio)return;}
        world_cinematics_audio::Configuration config;config.document=cinematic_document;
        auto asset=[&](const std::string &name){if(name.empty()||std::any_of(config.files.begin(),config.files.end(),[&](const auto &f){return f.asset==name;}))return;config.files.push_back({name,(std::filesystem::path(project_directory)/name).string()});};
        for(const auto &z:cinematic_document.zones)asset(z.asset);
        for(const auto &c:cinematic_document.clips)asset(c.asset);
        world_cinematics_audio::collect(cinematic_audio);
        if(world_cinematics_audio::configure_files(cinematic_audio,config,message)){cinematic_audio_ready=cinematic_audio_active=true;cinematic_audio_clock.configure(cinematic_document.duration,cinematic_document.loop);cinematic_audio_was_preview=!cinematic_preview;message="Authored zones and cues are playing through the owned spatial audio bus.";}
    }
    void cinematic_audio_frame(const Camera &camera,double now,bool focused){
        if(!cinematic_audio_ready||!cinematic_audio_active)return;
        if(!focused){if(cinematic_audio_focused)world_cinematics_audio::stop_all(cinematic_audio);cinematic_audio_focused=false;return;}
        world_cinematics::Step step;
        if(cinematic_preview)step=cinematic_step;else if(!cinematic_audio_clock.advance(now,step))return;
        if(cinematic_audio_was_preview!=cinematic_preview||!cinematic_audio_focused)step.jumped=true;
        cinematic_audio_was_preview=cinematic_preview;cinematic_audio_focused=true;
        world_cinematics::CameraPose listener;listener.position={camera.x,camera.y,camera.z};listener.rotation=world_cinematics::orientation(camera.yaw,camera.pitch);listener.fov_degrees=camera.fov*57.29577951308232;
        const auto result=world_cinematics_audio::update(cinematic_audio,step,listener);if(result!=world_cinematics_audio::UpdateResult::Ok&&result!=world_cinematics_audio::UpdateResult::QueueFull)message="Authored audio frame was rejected; inspect the timeline configuration.";
    }
    void cinematic_audio_panel(const Camera &camera){
        if(!ImGui::CollapsingHeader("Spatial audio zones / timeline cues"))return;
        ImGui::InputText("PCM WAV asset (project relative)",cinematic_audio_asset,sizeof cinematic_audio_asset);
        if(ImGui::Button("Add ambience zone at target")&&target&&cinematic_document.zones.size()<world_cinematics::zone_limit){auto next=cinematic_document;world_cinematics::Zone zone;zone.id=1;for(const auto &z:next.zones)zone.id=std::max(zone.id,z.id+1);zone.asset=cinematic_audio_asset;zone.name="Zone "+std::to_string(zone.id);zone.center={x,y+2,z};zone.gain=.3f;next.zones.push_back(zone);if(world_cinematics::validate(next,message)){cinematic_document=std::move(next);cinematic_zone=int(cinematic_document.zones.size()-1);}}
        for(unsigned i=0;i<cinematic_document.zones.size();i++)if(ImGui::Selectable(cinematic_document.zones[i].name.c_str(),cinematic_zone==int(i)))cinematic_zone=int(i);
        if(cinematic_zone>=0&&unsigned(cinematic_zone)<cinematic_document.zones.size()){
            auto next=cinematic_document;auto &zone=next.zones[unsigned(cinematic_zone)];float half[]={float(zone.extent.x),float(zone.extent.y),float(zone.extent.z)},blend=float(zone.blend);int priority=zone.priority,shape=int(zone.shape),bus=int(zone.bus);bool edit=false;
            edit|=ImGui::InputDouble("Zone east",&zone.center.x,.5,5);edit|=ImGui::InputDouble("Zone height",&zone.center.y,.5,5);edit|=ImGui::InputDouble("Zone south",&zone.center.z,.5,5);edit|=ImGui::SliderFloat3("Half extents / sphere radius",half,.5,100);
            edit|=ImGui::Combo("Zone shape",&shape,"Sphere\0Box\0");edit|=ImGui::SliderInt("Zone priority",&priority,-10,10);edit|=ImGui::SliderInt("Mix bus",&bus,0,7);edit|=ImGui::SliderFloat("Boundary feather (m)",&blend,.1,30);edit|=ImGui::SliderFloat("Zone gain",&zone.gain,0,1);edit|=ImGui::SliderFloat("Dry signal",&zone.dry,0,1);edit|=ImGui::SliderFloat("Reverb send",&zone.reverb,0,1);
            zone.extent={half[0],half[1],half[2]};zone.blend=blend;zone.priority=priority;zone.shape=world_cinematics::Shape(shape);zone.bus=unsigned(bus);
            if(edit&&world_cinematics::validate(next,message))cinematic_document=std::move(next);
            if(ImGui::Button("Remove audio zone")){cinematic_document.zones.erase(cinematic_document.zones.begin()+cinematic_zone);cinematic_zone=-1;}
        }
        if(ImGui::Button("Add cue at key time")&&cinematic_document.clips.size()<world_cinematics::clip_limit){auto next=cinematic_document;world_cinematics::Clip cue;cue.id=1;for(const auto &c:next.clips)cue.id=std::max(cue.id,c.id+1);cue.asset=cinematic_audio_asset;cue.start=cinematic_key_time;cue.duration=std::min(3.,next.duration-cue.start);cue.gain=.3f;if(cue.duration>0){next.clips.push_back(cue);if(world_cinematics::validate(next,message))cinematic_document=std::move(next);}else message="Choose a cue time before the timeline ends.";}
        if(ImGui::Button("Prepare / apply spatial audio"))cinematic_audio_prepare();
        ImGui::SameLine();if(ImGui::Button("Mute authored audio")&&cinematic_audio){world_cinematics_audio::stop_all(cinematic_audio);cinematic_audio_active=false;}
        if(cinematic_audio){const auto s=world_cinematics_audio::stats(cinematic_audio);ImGui::Text("PCM %.1fMiB | voices %u | outside %.2f",s.decoded_bytes/1048576.,s.active_voices,s.outside_gain);}
        ImGui::TextWrapped("Audio preparation reads WAV files explicitly. Playback uses owned channels, equal-power zone transitions and stereo reverb. A muted bus leaves the normal demo ambience active. Cues shorter than a rendered frame can be skipped after a stall.");
        (void)camera;
    }
    void cinematic_camera(Camera &camera,double now){
        if(cinematic_restore_camera){camera=cinematic_return;cinematic_restore_camera=false;cinematic_cut=true;}
        if(!cinematic_preview)return;
        if(!cinematic_transport.advance(now,cinematic_step)){message="Invalid cinematic clock";return;}
        world_cinematics::CameraPose p;if(!world_cinematics::sample_camera(cinematic_document,cinematic_step.time,p))return;
        auto f=world_cinematics::forward(p.rotation);camera.x=p.position.x;camera.y=p.position.y;camera.z=p.position.z;
        camera.yaw=float(std::atan2(f.x,-f.z));camera.pitch=float(std::asin(std::clamp(f.y,-1.,1.)));camera.fly=true;camera.grounded=false;camera.vy=0;
        camera.fov=float(p.fov_degrees*.017453292519943295);cinematic_cut|=cinematic_step.jumped;
        if(cinematic_step.finished){cinematic_preview=false;cinematic_restore_camera=true;}
    }
    void cinematic_panel(const Camera &camera){
        if(!ImGui::BeginTabItem("Cinematics",nullptr,requested_tab==17?ImGuiTabItemFlags_SetSelected:0))return;
        tab=17;
        ImGui::TextWrapped("Record camera keys from the editor view. Timelines sample world-space positions, quaternion rotation and vertical field of view. Preview temporarily controls the camera.");
        ImGui::InputText("Timeline file",cinematic_file,sizeof cinematic_file);
        ImGui::SliderFloat("Timeline duration (s)",&cinematic_duration,1,300);
        ImGui::SliderFloat("Key time (s)",&cinematic_key_time,0,cinematic_duration);
        if(ImGui::Button("Record current view")){
            if(cinematic_preview)message="Stop the camera preview before editing its keys.";
            else{auto next=cinematic_document;next.duration=cinematic_duration;world_cinematics::CameraKey key;key.time=cinematic_key_time;key.pose.position={camera.x,camera.y,camera.z};key.pose.rotation=world_cinematics::orientation(camera.yaw,camera.pitch);key.pose.fov_degrees=camera.fov*57.29577951308232;
                auto at=std::find_if(next.camera.begin(),next.camera.end(),[&](const auto &k){return std::abs(k.time-key.time)<.0001;});if(at==next.camera.end())next.camera.push_back(key);else *at=key;
                std::sort(next.camera.begin(),next.camera.end(),[](const auto &a,const auto &b){return a.time<b.time;});if(world_cinematics::validate(next,message)){cinematic_document=std::move(next);message="Camera key recorded.";}
            }
        }
        for(unsigned i=0;i<cinematic_document.camera.size();i++)if(ImGui::Selectable(("Key "+std::to_string(i+1)+" | "+std::to_string(cinematic_document.camera[i].time)+"s").c_str(),cinematic_selection==int(i)))cinematic_selection=int(i);
        if(cinematic_selection>=0&&unsigned(cinematic_selection)<cinematic_document.camera.size()&&!cinematic_preview){
            auto next=cinematic_document;auto &key=next.camera[unsigned(cinematic_selection)];float fov=float(key.pose.fov_degrees);int curve=int(key.curve);
            bool edit=ImGui::SliderFloat("Vertical field of view",&fov,30,100);key.pose.fov_degrees=fov;
            if(ImGui::Combo("Key interpolation",&curve,"Linear\0Smooth\0Hold\0")){key.curve=world_cinematics::Curve(curve);edit=true;}
            if(edit&&world_cinematics::validate(next,message))cinematic_document=std::move(next);
            if(ImGui::Button("Delete selected camera key")){cinematic_document.camera.erase(cinematic_document.camera.begin()+cinematic_selection);cinematic_selection=-1;}
        }
        if(!cinematic_preview&&ImGui::Button("Preview camera timeline")){
            std::string error;if(world_cinematics::validate(cinematic_document,error)&&!cinematic_document.camera.empty()&&cinematic_transport.configure(cinematic_document.duration,cinematic_document.loop)){
                cinematic_return=camera;cinematic_preview=true;cinematic_cut=true;cinematic_transport.seek(0);message="Camera preview active. Studio remains available to stop or scrub.";
            }else message=error.empty()?"Record at least one camera key first.":error;
        }
        if(cinematic_preview){
            if(ImGui::Button("Stop camera preview")){cinematic_preview=false;cinematic_restore_camera=true;}
            ImGui::SameLine();if(ImGui::Button(cinematic_transport.paused()?"Resume timeline":"Pause timeline"))cinematic_transport.pause(!cinematic_transport.paused());
            float time=float(cinematic_transport.time());if(ImGui::SliderFloat("Scrub (s)",&time,0,float(cinematic_document.duration))){cinematic_transport.seek(time);cinematic_cut=true;}
        }
        if(ImGui::Button("Save timeline")){auto file=std::filesystem::path(cinematic_file);if(file.is_relative())file=std::filesystem::path(project_directory)/file;std::error_code ec;std::filesystem::create_directories(file.parent_path(),ec);if(ec)message=ec.message();else if(world_cinematics::save(file.u8string().c_str(),cinematic_document,message))message="Cinematic timeline saved.";}
        ImGui::SameLine();if(!cinematic_preview&&ImGui::Button("Load timeline")){auto file=std::filesystem::path(cinematic_file);if(file.is_relative())file=std::filesystem::path(project_directory)/file;if(world_cinematics::load(file.u8string().c_str(),cinematic_document,message)){cinematic_duration=float(cinematic_document.duration);cinematic_selection=-1;message="Cinematic timeline loaded.";}}
        cinematic_audio_panel(camera);
        ImGui::TextWrapped("Camera preview supports yaw, pitch and field of view. Roll remains a timeline-library feature. Timeline files include audio zones, cues and authored bus envelopes.");
        ImGui::EndTabItem();
    }

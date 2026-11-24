    world_studio_cinematics::Model cinematic_model;
    world_cinematics::Document cinematic_document;
    world_cinematics::Transport cinematic_transport;
    world_cinematics::Step cinematic_step;
    Camera cinematic_return;
    bool cinematic_preview=false,cinematic_restore_camera=false,cinematic_cut=false,cinematic_initialized=false,cinematic_recovery_pending=false;
    int cinematic_selection=-1,cinematic_zone=-1,cinematic_cue=-1;
    float cinematic_key_time=0,cinematic_duration=20;
    char cinematic_file[1024]="cinematics/demo.vct",cinematic_audio_asset[512]="audio/demo/forest.wav";
    world_cinematics_audio::Audio *cinematic_audio=nullptr;
    world_cinematics::Transport cinematic_audio_clock;
    bool cinematic_audio_ready=false,cinematic_audio_active=false,cinematic_audio_was_preview=false,cinematic_audio_focused=true;
    std::function<bool(std::uint64_t,std::string&)> cinematic_play_sequence;
    std::function<bool(std::string&)> cinematic_reload_library;
    bool cinematic_unsaved()const{return cinematic_model.dirty();}
    void cinematic_sync(bool reset=false){if(const auto *e=cinematic_model.selected()){cinematic_document=e->timeline;cinematic_duration=float(e->timeline.duration);std::snprintf(cinematic_file,sizeof cinematic_file,"%s",e->sequence.timeline.c_str());}else cinematic_document={};if(reset){cinematic_selection=cinematic_zone=cinematic_cue=-1;cinematic_key_time=0;}}
    void cinematic_stop(){if(cinematic_preview){cinematic_preview=false;cinematic_restore_camera=true;}cinematic_audio_shutdown();}
    void cinematic_init(){if(cinematic_initialized)return;cinematic_initialized=true;cinematic_recovery_pending=world_project::pending(project_directory,"cinematics/world.vcutsproject");if(cinematic_model.open(project_directory,false,message)){cinematic_sync(true);message="Cinematic library loaded. Create a sequence, record its views, then Publish.";}}
    bool cinematic_commit(world_studio_cinematics::State next,bool merge=false){if(!cinematic_model.edit(std::move(next),message,merge))return false;cinematic_sync();cinematic_audio_shutdown();message="Cinematic draft changed. Publish saves the library and every timeline together.";return true;}
    bool cinematic_commit_timeline(world_cinematics::Document next,bool merge=false){auto state=cinematic_model.state();auto it=std::find_if(state.entries.begin(),state.entries.end(),[&](const auto&e){return e.sequence.id==state.selected;});if(it==state.entries.end())return false;it->timeline=std::move(next);return cinematic_commit(std::move(state),merge);}
    bool cinematic_publish(){if(!cinematic_model.publish(message)){cinematic_recovery_pending=world_project::pending(project_directory,"cinematics/world.vcutsproject");return false;}message="Published cinematics/world.vcuts and all timelines. Other sequences were preserved.";if(cinematic_reload_library){std::string error;if(!cinematic_reload_library(error))message+=" Runtime preparation: "+error;}return true;}
    void cinematic_audio_shutdown(){world_cinematics_audio::destroy(cinematic_audio);cinematic_audio=nullptr;cinematic_audio_ready=cinematic_audio_active=false;}
    float cinematic_outside()const{return cinematic_audio_ready&&cinematic_audio_active?world_cinematics_audio::stats(cinematic_audio).outside_gain:1;}
    void cinematic_audio_prepare(){
        if(!cinematic_audio){cinematic_audio=world_cinematics_audio::create({},message);if(!cinematic_audio)return;}
        world_cinematics_audio::Configuration config;config.document=cinematic_document;
        auto asset=[&](const std::string &name){if(name.empty()||std::any_of(config.files.begin(),config.files.end(),[&](const auto &f){return f.asset==name;}))return;config.files.push_back({name,(std::filesystem::path(project_directory)/name).string()});};
        for(const auto &z:cinematic_document.zones)asset(z.asset);
        for(const auto &c:cinematic_document.clips)asset(c.asset);
        world_cinematics_audio::collect(cinematic_audio);
        if(world_cinematics_audio::configure_files(cinematic_audio,config,message)){if(sound_yield)sound_yield();cinematic_audio_ready=cinematic_audio_active=true;cinematic_audio_clock.configure(cinematic_document.duration,cinematic_document.loop);cinematic_audio_was_preview=!cinematic_preview;message="Authored zones and cues are playing through the owned spatial audio bus.";}
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
    void cinematic_audio_panel(const Camera &){
        if(!ImGui::CollapsingHeader("Narration and sound cues"))return;
        ImGui::TextWrapped("Import MP3: Import_Valwood_Cutscene_Voice.sh recording.mp3 --project <folder>. Paste the printed PCM WAV path below.");
        ImGui::InputText("New cue WAV",cinematic_audio_asset,sizeof cinematic_audio_asset);
        if(ImGui::Button("Add narration at playhead")&&cinematic_document.clips.size()<world_cinematics::clip_limit){auto next=cinematic_document;world_cinematics::Clip cue;cue.id=1;for(const auto &c:next.clips)cue.id=std::max(cue.id,c.id+1);cue.asset=cinematic_audio_asset;cue.start=cinematic_key_time;cue.duration=std::min(3.,next.duration-cue.start);cue.gain=1;if(cue.duration>0){next.clips.push_back(cue);if(cinematic_commit_timeline(std::move(next)))cinematic_cue=int(cinematic_document.clips.size()-1);}else message="Move the playhead before the end to add a cue.";}
        for(unsigned i=0;i<cinematic_document.clips.size();++i){const auto &c=cinematic_document.clips[i];const auto text="Cue "+std::to_string(c.id)+" | "+std::to_string(c.start)+"s | "+c.asset;if(ImGui::Selectable(text.c_str(),cinematic_cue==int(i)))cinematic_cue=int(i);}
        if(cinematic_cue>=0&&unsigned(cinematic_cue)<cinematic_document.clips.size()){
            auto next=cinematic_document;auto &c=next.clips[unsigned(cinematic_cue)];char asset[512];std::snprintf(asset,sizeof asset,"%s",c.asset.c_str());bool edit=ImGui::InputText("Cue WAV path",asset,sizeof asset);c.asset=asset;
            edit|=ImGui::InputDouble("Cue start (s)",&c.start,.1,1,"%.3f");edit|=ImGui::InputDouble("Cue duration (s)",&c.duration,.1,1,"%.3f");edit|=ImGui::InputDouble("Source offset (s)",&c.source_offset,.1,1,"%.3f");
            edit|=ImGui::SliderFloat("Cue volume",&c.gain,0,1);edit|=ImGui::InputFloat("Fade in (s)",&c.fade_in,.05f,.5f,"%.3f");edit|=ImGui::InputFloat("Fade out (s)",&c.fade_out,.05f,.5f,"%.3f");
            if(edit)cinematic_commit_timeline(std::move(next),ImGui::IsAnyItemActive());
            if(ImGui::Button("Remove selected cue")){auto doc=cinematic_document;doc.clips.erase(doc.clips.begin()+cinematic_cue);if(cinematic_commit_timeline(std::move(doc)))cinematic_cue=-1;}
        }
        if(ImGui::TreeNode("Spatial ambience zones")){
            ImGui::BeginDisabled(!target);if(ImGui::Button("Add zone at viewport target")&&cinematic_document.zones.size()<world_cinematics::zone_limit){auto next=cinematic_document;world_cinematics::Zone a;a.id=1;for(const auto &z:next.zones)a.id=std::max(a.id,z.id+1);a.asset=cinematic_audio_asset;a.name="Zone "+std::to_string(a.id);a.center={x,y+2,z};a.gain=.3f;next.zones.push_back(a);if(cinematic_commit_timeline(std::move(next)))cinematic_zone=int(cinematic_document.zones.size()-1);}ImGui::EndDisabled();
            for(unsigned i=0;i<cinematic_document.zones.size();++i)if(ImGui::Selectable(cinematic_document.zones[i].name.c_str(),cinematic_zone==int(i)))cinematic_zone=int(i);
            if(cinematic_zone>=0&&unsigned(cinematic_zone)<cinematic_document.zones.size()){
                auto next=cinematic_document;auto &a=next.zones[unsigned(cinematic_zone)];int shape=int(a.shape);float extent[]={float(a.extent.x),float(a.extent.y),float(a.extent.z)};bool edit=false;
                edit|=ImGui::InputDouble("Zone X",&a.center.x,.5,5);edit|=ImGui::InputDouble("Zone Y",&a.center.y,.5,5);edit|=ImGui::InputDouble("Zone Z",&a.center.z,.5,5);edit|=ImGui::Combo("Zone shape",&shape,"Sphere\0Box\0");edit|=ImGui::InputFloat3("Zone radius / half extents",extent);a.extent={extent[0],extent[1],extent[2]};a.shape=world_cinematics::Shape(shape);
                edit|=ImGui::SliderFloat("Zone volume",&a.gain,0,1);edit|=ImGui::SliderFloat("Zone reverb",&a.reverb,0,1);edit|=ImGui::InputDouble("Zone feather (m)",&a.blend,.1,1);edit|=ImGui::InputInt("Zone priority",&a.priority);
                if(edit)cinematic_commit_timeline(std::move(next),ImGui::IsAnyItemActive());
                if(ImGui::Button("Remove zone")){auto doc=cinematic_document;doc.zones.erase(doc.zones.begin()+cinematic_zone);if(cinematic_commit_timeline(std::move(doc)))cinematic_zone=-1;}
            }ImGui::TreePop();
        }
        if(ImGui::Button("Prepare audio preview"))cinematic_audio_prepare();ImGui::SameLine();if(ImGui::Button("Mute preview audio")&&cinematic_audio){world_cinematics_audio::stop_all(cinematic_audio);cinematic_audio_active=false;}
        if(cinematic_audio){const auto stats=world_cinematics_audio::stats(cinematic_audio);ImGui::Text("PCM %.1f MiB | voices %u",stats.decoded_bytes/1048576.,stats.active_voices);}
    }
    void cinematic_panel(const Camera &camera){
        if(!ImGui::BeginTabItem("Cinematics",nullptr,requested_tab==17?ImGuiTabItemFlags_SetSelected:0))return;
        tab=17;cinematic_init();if(!ImGui::IsAnyItemActive())cinematic_model.end_gesture();
        ImGui::TextWrapped("Build a sequence, record camera views, choose how it starts, then Publish. Preview leaves your player in place.");
        ImGui::TextColored(cinematic_unsaved()?ImVec4{1,.72f,.28f,1}:ImVec4{.4f,.85f,.65f,1},"%s",cinematic_unsaved()?"UNPUBLISHED CINEMATIC CHANGES":"CINEMATICS SAVED");
        ImGui::BeginDisabled(cinematic_preview);
        if(ImGui::Button("New sequence")){world_cinematics::CameraPose p;p.position={camera.x,camera.y,camera.z};p.rotation=world_cinematics::orientation(camera.yaw,camera.pitch);p.fov_degrees=camera.fov*57.29577951308232;if(cinematic_model.create(p,message)){cinematic_stop();cinematic_sync(true);}}
        ImGui::SameLine();ImGui::BeginDisabled(!cinematic_model.can_undo());if(ImGui::Button("Undo cinematic")){cinematic_stop();cinematic_model.undo();cinematic_sync(true);}ImGui::EndDisabled();
        ImGui::SameLine();ImGui::BeginDisabled(!cinematic_model.can_redo());if(ImGui::Button("Redo cinematic")){cinematic_stop();cinematic_model.redo();cinematic_sync(true);}ImGui::EndDisabled();
        const auto *selected_sequence=cinematic_model.selected();
        if(ImGui::BeginCombo("Sequence",selected_sequence?selected_sequence->sequence.name.c_str():"Create a sequence")){
            for(const auto &entry:cinematic_model.state().entries)if(ImGui::Selectable((entry.sequence.name+"##"+std::to_string(entry.sequence.id)).c_str(),entry.sequence.id==cinematic_model.state().selected)){cinematic_stop();cinematic_model.select(entry.sequence.id);cinematic_sync(true);}ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        if(const auto *entry=cinematic_model.selected()){
            ImGui::BeginDisabled(cinematic_preview);
            const auto selected_id=entry->sequence.id;auto state=cinematic_model.state();auto &sequence=*std::find_if(state.entries.begin(),state.entries.end(),[&](const auto&e){return e.sequence.id==state.selected;});char name[96];std::snprintf(name,sizeof name,"%s",sequence.sequence.name.c_str());
            if(ImGui::InputText("Sequence name",name,sizeof name)){sequence.sequence.name=name;sequence.timeline.name=name;cinematic_commit(std::move(state),ImGui::IsAnyItemActive());}
            ImGui::Text("Persistent sequence ID: %llu",static_cast<unsigned long long>(selected_id));
            if(ImGui::Button("Delete sequence..."))ImGui::OpenPopup("Delete cinematic sequence?");
            if(ImGui::BeginPopupModal("Delete cinematic sequence?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){ImGui::TextWrapped("Remove this sequence from the library? Undo restores it. Existing timeline files remain on disk.");if(ImGui::Button("Remove from library")){cinematic_stop();cinematic_model.erase(cinematic_model.state().selected,message);cinematic_sync(true);ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button("Cancel deletion"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
            ImGui::EndDisabled();
            if(cinematic_model.selected()){
                if(ImGui::CollapsingHeader("Camera timeline",ImGuiTreeNodeFlags_DefaultOpen)){
                    ImGui::BeginDisabled(cinematic_preview);
                    ImGui::InputText("Timeline file",cinematic_file,sizeof cinematic_file);
                    if(ImGui::Button("Use timeline filename")){auto next=cinematic_model.state();for(auto&e:next.entries)if(e.sequence.id==next.selected)e.sequence.timeline=cinematic_file;cinematic_commit(std::move(next));}
                    ImGui::SameLine();if(ImGui::Button("Load existing .vct")){if(cinematic_model.load_timeline(cinematic_file,message)){cinematic_stop();cinematic_sync(true);}}
                    double duration=cinematic_document.duration;if(ImGui::InputDouble("Duration (seconds)",&duration,.5,5,"%.3f")){auto doc=cinematic_document;doc.duration=duration;cinematic_commit_timeline(std::move(doc),ImGui::IsAnyItemActive());}
                    ImGui::SliderFloat("Playhead (s)",&cinematic_key_time,0,float(cinematic_document.duration));
                    if(ImGui::Button("Record view at playhead")){
                        auto doc=cinematic_document;world_cinematics::CameraKey k;k.time=cinematic_key_time;k.pose.position={camera.x,camera.y,camera.z};k.pose.rotation=world_cinematics::orientation(camera.yaw,camera.pitch);k.pose.fov_degrees=camera.fov*57.29577951308232;
                        auto at=std::find_if(doc.camera.begin(),doc.camera.end(),[&](const auto&v){return std::abs(v.time-k.time)<.0001;});if(at==doc.camera.end())doc.camera.push_back(k);else *at=k;std::sort(doc.camera.begin(),doc.camera.end(),[](const auto&a,const auto&b){return a.time<b.time;});if(cinematic_commit_timeline(std::move(doc)))for(unsigned i=0;i<cinematic_document.camera.size();++i)if(std::abs(cinematic_document.camera[i].time-k.time)<.0001)cinematic_selection=int(i);
                    }
                    for(unsigned i=0;i<cinematic_document.camera.size();++i){char label[128];std::snprintf(label,sizeof label,"Shot %u  |  %.3f s",i+1,cinematic_document.camera[i].time);if(ImGui::Selectable(label,cinematic_selection==int(i))){cinematic_selection=int(i);cinematic_key_time=float(cinematic_document.camera[i].time);}}
                    if(cinematic_selection>=0&&unsigned(cinematic_selection)<cinematic_document.camera.size()){
                        auto doc=cinematic_document;auto &k=doc.camera[unsigned(cinematic_selection)];double t=k.time;auto f=world_cinematics::forward(k.pose.rotation);float angles[]={float(std::atan2(f.x,-f.z)*57.29577951308232),float(std::asin(std::clamp(f.y,-1.,1.))*57.29577951308232)},fov=float(k.pose.fov_degrees);int curve=int(k.curve);bool edit=false;
                        edit|=ImGui::InputDouble("Exact key time (s)",&t,.1,1,"%.3f");k.time=t;edit|=ImGui::InputDouble("Camera X",&k.pose.position.x,.1,1);edit|=ImGui::InputDouble("Camera Y",&k.pose.position.y,.1,1);edit|=ImGui::InputDouble("Camera Z",&k.pose.position.z,.1,1);
                        if(ImGui::InputFloat2("Yaw / pitch (degrees)",angles)){k.pose.rotation=world_cinematics::orientation(angles[0]*.017453292519943295,angles[1]*.017453292519943295);edit=true;}
                        edit|=ImGui::SliderFloat("Vertical FOV",&fov,30,100);k.pose.fov_degrees=fov;edit|=ImGui::Combo("Transition to next shot",&curve,"Linear\0Smooth\0Hold\0");k.curve=world_cinematics::Curve(curve);
                        if(edit){std::sort(doc.camera.begin(),doc.camera.end(),[](const auto&a,const auto&b){return a.time<b.time;});if(cinematic_commit_timeline(std::move(doc),ImGui::IsAnyItemActive()))for(unsigned i=0;i<cinematic_document.camera.size();++i)if(cinematic_document.camera[i].time==t)cinematic_selection=int(i);}
                        if(ImGui::Button("Replace pose with current view")){auto d=cinematic_document;auto &q=d.camera[unsigned(cinematic_selection)].pose;q.position={camera.x,camera.y,camera.z};q.rotation=world_cinematics::orientation(camera.yaw,camera.pitch);q.fov_degrees=camera.fov*57.29577951308232;cinematic_commit_timeline(std::move(d));}
                        if(ImGui::Button("Duplicate at playhead")){auto d=cinematic_document;auto q=d.camera[unsigned(cinematic_selection)];q.time=cinematic_key_time;if(std::abs(q.time-d.camera[unsigned(cinematic_selection)].time)<.0001)q.time+=.25;d.camera.push_back(q);std::sort(d.camera.begin(),d.camera.end(),[](const auto&a,const auto&b){return a.time<b.time;});cinematic_commit_timeline(std::move(d));}
                        ImGui::BeginDisabled(cinematic_selection==0);if(ImGui::Button("Move shot earlier")){auto d=cinematic_document;const auto i=unsigned(cinematic_selection);auto a=d.camera[i-1].time,b=d.camera[i].time;std::swap(d.camera[i-1],d.camera[i]);d.camera[i-1].time=a;d.camera[i].time=b;if(cinematic_commit_timeline(std::move(d)))--cinematic_selection;}ImGui::EndDisabled();
                        ImGui::SameLine();ImGui::BeginDisabled(unsigned(cinematic_selection+1)>=cinematic_document.camera.size());if(ImGui::Button("Move shot later")){auto d=cinematic_document;const auto i=unsigned(cinematic_selection);auto a=d.camera[i].time,b=d.camera[i+1].time;std::swap(d.camera[i],d.camera[i+1]);d.camera[i].time=a;d.camera[i+1].time=b;if(cinematic_commit_timeline(std::move(d)))++cinematic_selection;}ImGui::EndDisabled();
                        ImGui::TextWrapped("Move shot changes view order while keeping timeline times fixed.");ImGui::BeginDisabled(cinematic_document.camera.size()<2);if(ImGui::Button("Delete selected shot")){auto d=cinematic_document;d.camera.erase(d.camera.begin()+cinematic_selection);if(cinematic_commit_timeline(std::move(d)))cinematic_selection=-1;}ImGui::EndDisabled();
                    }
                    ImGui::EndDisabled();
                }
                if(ImGui::CollapsingHeader("Trigger and playback",ImGuiTreeNodeFlags_DefaultOpen)){
                    ImGui::BeginDisabled(cinematic_preview);auto next=cinematic_model.state();auto &s=*std::find_if(next.entries.begin(),next.entries.end(),[&](const auto&e){return e.sequence.id==next.selected;});auto &q=s.sequence;int kind=int(q.trigger.kind),shape=int(q.trigger.shape);bool edit=false;
                    edit|=ImGui::Checkbox("Enabled",&q.enabled);ImGui::SameLine();edit|=ImGui::Checkbox("Play once per save",&q.once);edit|=ImGui::Checkbox("Player can skip",&q.skip_allowed);ImGui::SameLine();edit|=ImGui::Checkbox("Letterbox",&q.letterbox);
                    if(ImGui::Combo("Start sequence",&kind,"Manual\0Enter area\0Interact with NPC / entity\0")){q.trigger.kind=world_cutscenes::TriggerKind(kind);if(kind==2&&!q.trigger.target_id)q.trigger.target_id=1;edit=true;}
                    edit|=ImGui::InputInt("Trigger priority",&q.priority);edit|=ImGui::InputDouble("Repeat cooldown (s)",&q.trigger.cooldown_seconds,.25,1);
                    if(q.trigger.kind==world_cutscenes::TriggerKind::Area){
                        edit|=ImGui::Combo("Area shape",&shape,"Sphere\0Box\0");q.trigger.shape=world_cinematics::Shape(shape);
                        edit|=ImGui::InputDouble("Area X",&q.trigger.center.x,.5,5);edit|=ImGui::InputDouble("Area Y",&q.trigger.center.y,.5,5);edit|=ImGui::InputDouble("Area Z",&q.trigger.center.z,.5,5);
                        ImGui::BeginDisabled(!target);if(ImGui::Button("Place area at viewport target")){q.trigger.center={x,y,z};edit=true;}ImGui::EndDisabled();
                        ImGui::SameLine();if(ImGui::Button("Center area at camera feet")){q.trigger.center={camera.x,camera.y-1.75,camera.z};edit=true;}
                        if(q.trigger.shape==world_cinematics::Shape::Sphere)edit|=ImGui::InputDouble("Area radius (m)",&q.trigger.extent.x,.25,1);
                        else{edit|=ImGui::InputDouble("Half width X (m)",&q.trigger.extent.x,.25,1);edit|=ImGui::InputDouble("Half height Y (m)",&q.trigger.extent.y,.25,1);edit|=ImGui::InputDouble("Half depth Z (m)",&q.trigger.extent.z,.25,1);}
                    }else if(q.trigger.kind==world_cutscenes::TriggerKind::Interact){
                        int npc=q.trigger.target_id>=1&&q.trigger.target_id<=3?int(q.trigger.target_id):0;if(ImGui::Combo("Known target",&npc,"Custom persistent ID\0Rowan\0Bram\0Ellis\0")&&npc){q.trigger.target_id=std::uint64_t(npc);edit=true;}
                        edit|=ImGui::InputScalar("Persistent target ID",ImGuiDataType_U64,&q.trigger.target_id);ImGui::TextWrapped("Use a persistent actor/entity ID. Camp IDs: Rowan1, Bram2, Ellis3. Model indexes are not IDs. After an interaction cutscene, press Interact again to begin the normal conversation.");
                    }
                    if(edit)cinematic_commit(std::move(next),ImGui::IsAnyItemActive());ImGui::EndDisabled();
                }
                ImGui::BeginDisabled(cinematic_preview);cinematic_audio_panel(camera);ImGui::EndDisabled();
                ImGui::SeparatorText("Preview and publish");
                if(!cinematic_preview&&ImGui::Button("Preview camera and cues")){
                    std::string error;if(world_cinematics::validate(cinematic_document,error)&&!cinematic_document.camera.empty()&&cinematic_transport.configure(cinematic_document.duration,false)){cinematic_return=camera;cinematic_preview=true;cinematic_cut=true;cinematic_transport.seek(0);const bool has_audio=!cinematic_document.clips.empty()||!cinematic_document.zones.empty();if(has_audio)cinematic_audio_prepare();if(has_audio&&!cinematic_audio_ready)message="Camera preview active; audio unavailable: "+message;else message="Editor preview active. Stop restores the previous editor camera.";}else message=error;
                }
                if(cinematic_preview){if(ImGui::Button("Stop preview"))cinematic_stop();ImGui::SameLine();if(ImGui::Button(cinematic_transport.paused()?"Resume preview":"Pause preview"))cinematic_transport.pause(!cinematic_transport.paused());float time=float(cinematic_transport.time());if(ImGui::SliderFloat("Preview scrub (s)",&time,0,float(cinematic_document.duration))){cinematic_transport.seek(time);cinematic_cut=true;}}
                ImGui::BeginDisabled(cinematic_preview);if(ImGui::Button("Publish cinematic library"))cinematic_publish();ImGui::SameLine();ImGui::BeginDisabled(cinematic_unsaved()||!cinematic_play_sequence);if(ImGui::Button("Play published in world")){std::string error;if(!cinematic_play_sequence(cinematic_model.state().selected,error))message=error;}ImGui::EndDisabled();ImGui::EndDisabled();
            }
        }
        ImGui::BeginDisabled(cinematic_preview);if(cinematic_reload_library&&ImGui::Button("Reload published gameplay library")){std::string error;if(cinematic_reload_library(error))message="Published triggers and game audio are prepared.";else message=error;}
        if(ImGui::Button("Reload library...")){if(cinematic_unsaved())ImGui::OpenPopup("Discard cinematic draft?");else if(cinematic_model.open(project_directory,false,message)){cinematic_stop();cinematic_sync(true);}}
        if(ImGui::BeginPopupModal("Discard cinematic draft?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){ImGui::TextWrapped("Reload discards all unpublished sequence, camera, trigger and narration changes. Publish first to keep them.");if(ImGui::Button("Discard and reload")){if(cinematic_model.open(project_directory,true,message)){cinematic_stop();cinematic_sync(true);ImGui::CloseCurrentPopup();}}ImGui::SameLine();if(ImGui::Button("Keep editing"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
        if(cinematic_recovery_pending){ImGui::TextWrapped("An interrupted cinematic publish is pending. Recover it before editing.");if(ImGui::Button("Finish cinematic publish")){if(cinematic_model.recover(project_directory,true,message)){cinematic_recovery_pending=false;cinematic_sync(true);}}if(ImGui::Button("Roll back cinematic publish")){if(cinematic_model.recover(project_directory,false,message)){cinematic_recovery_pending=false;cinematic_sync(true);}}}
        ImGui::EndDisabled();ImGui::TextWrapped("Cinematics publish separately from Save All. Unsaved sequences remain protected on reload and exit. After previewing audio, reload the published gameplay library to re-arm game triggers.");ImGui::EndTabItem();
    }

    void cinematic_overlay(const Camera &camera){
        if(tab!=17||!cinematic_initialized)return;const auto *e=cinematic_model.selected();if(!e||e->sequence.trigger.kind!=world_cutscenes::TriggerKind::Area)return;
        const auto &t=e->sequence.trigger;auto *draw=ImGui::GetBackgroundDrawList();const auto color=e->sequence.enabled?IM_COL32(105,221,211,235):IM_COL32(170,160,140,215);
        auto line=[&](world_cinematics::Vec3 a,world_cinematics::Vec3 b){ImVec2 p,q;if(project_point(camera,{a.x,a.y,a.z},p)&&project_point(camera,{b.x,b.y,b.z},q))draw->AddLine(p,q,color,2);};
        if(t.shape==world_cinematics::Shape::Sphere){for(int plane=0;plane<3;++plane)for(unsigned i=0;i<48;++i){const double a=i*6.283185307179586/48,b=(i+1)*6.283185307179586/48;auto p=t.center,q=t.center;const double ca=std::cos(a)*t.extent.x,sa=std::sin(a)*t.extent.x,cb=std::cos(b)*t.extent.x,sb=std::sin(b)*t.extent.x;if(plane==0){p.x+=ca;p.z+=sa;q.x+=cb;q.z+=sb;}else if(plane==1){p.x+=ca;p.y+=sa;q.x+=cb;q.y+=sb;}else{p.y+=ca;p.z+=sa;q.y+=cb;q.z+=sb;}line(p,q);}}
        else{world_cinematics::Vec3 corners[8];for(unsigned i=0;i<8;++i)corners[i]={t.center.x+(i&1?1:-1)*t.extent.x,t.center.y+(i&2?1:-1)*t.extent.y,t.center.z+(i&4?1:-1)*t.extent.z};for(unsigned i=0;i<8;++i)for(unsigned bit:{1u,2u,4u})if(!(i&bit))line(corners[i],corners[i|bit]);}
        ImVec2 p;if(project_point(camera,{t.center.x,t.center.y,t.center.z},p)){draw->AddCircleFilled(p,4,color);const auto label=e->sequence.name+(t.shape==world_cinematics::Shape::Sphere?" | sphere trigger":" | box trigger");draw->AddText({p.x+9,p.y},color,label.c_str());}
    }

// Included inside Studio. Placed audio is ordinary authored Document data;
// these controls never own a mixer or perform a terrain/scenery rebuild.
    int sound_selection=-1,sound_solo=-1,sound_arm_frame=-1,sound_drag_axis=-1;
    ImVec2 sound_drag_start{};world_entity::Transform sound_drag_pose;float sound_drag_length=1;
    bool sound_muted=false,sound_placing=false,sound_moving=false,sound_input_consumed=false;
    bool sound_catalog_scanned=false,sound_editing=false,sound_preview_pending=false,designer_audio_browser=false;
    char sound_search[128]{},sound_import_path[1024]{};
    std::uint64_t sound_solo_entity=0;
    std::string sound_clip;
    std::vector<std::string> sound_catalog;
    std::function<bool(const world_studio::Document&,std::string&)> sound_prepare;
    std::function<bool(unsigned,std::string&)> sound_audition;
    std::function<void()> sound_stop,sound_yield;
    std::function<void(bool,int)> sound_monitor;
    static constexpr const char *sound_payload_type="VALWOOD_AUDIO_CLIP";
    struct SoundPayload {char path[241]{};};
    void sound_cancel(){sound_placing=sound_moving=false;sound_arm_frame=-1;if(sound_drag_axis>=0){designer_cancel_edit();sound_drag_axis=-1;}}
    bool sound_is_placing()const{const auto *p=ImGui::GetDragDropPayload();return sound_placing||sound_moving||(p&&p->IsDataType(sound_payload_type));}
    bool sound_can_edit(){return production_can_save()&&!production_playing()&&!production_freeze();}
    std::string sound_name(unsigned index)const{
        for(const auto &node:draft.entities.nodes)if(node.binding==world_entity::Binding{world_entity::Kind::Sound,index})return node.name;
        return index<draft.sounds.size()?draft.sounds[index].name:"Sound";
    }
    void sound_open(){weather_cancel();landscape_cancel();designer_asset_cancel();designer_tools=true;designer_immersive=false;designer_content=true;designer_audio_browser=true;tab=requested_tab=25;}
    void sound_preview_apply(){if(sound_can_edit()&&!ImGui::IsAnyItemActive())apply=true;else sound_preview_pending=true;}
    void sound_property_changed(){if(!sound_editing)history.checkpoint(runtime_before);sound_editing=true;changed(false);sound_preview_pending=true;}
    void sound_finish_properties(){
        if(!ImGui::IsAnyItemActive()){sound_editing=false;if(sound_preview_pending&&sound_can_edit()){sound_preview_pending=false;apply=true;}}
    }
    bool sound_select(unsigned index){
        if(index>=draft.sounds.size())return false;
        const auto ref=designer_binding_ref({world_entity::Kind::Sound,index});if(ref.empty()||!designer_select(ref))return false;
        sound_selection=int(index);return true;
    }
    void sound_monitor_changed(){
        sound_solo_entity=0;if(sound_solo>=0&&unsigned(sound_solo)<draft.sounds.size())sound_solo_entity=world_designer::from_binding(draft,{world_entity::Kind::Sound,unsigned(sound_solo)}).id;
        if(sound_monitor)sound_monitor(sound_muted,sound_solo);
    }
    void sound_reconcile_monitor(){
        if(sound_solo<0)return;int index=-1;for(const auto &node:draft.entities.nodes)if(node.id==sound_solo_entity&&node.binding.kind==world_entity::Kind::Sound)index=int(node.binding.index);
        if(index!=sound_solo){sound_solo=index;if(index<0)sound_solo_entity=0;if(sound_monitor)sound_monitor(sound_muted,sound_solo);}
    }
    void sound_catalog_refresh(){
        sound_catalog.clear();std::size_t visited=0;const auto base=std::filesystem::u8path(project_directory);
        for(const char *folder:{"sounds","audio"}){
            std::error_code ec;const auto root=base/folder;if(!std::filesystem::is_directory(root,ec))continue;
            std::filesystem::recursive_directory_iterator it(root,std::filesystem::directory_options::skip_permission_denied,ec),end;
            while(!ec&&it!=end&&visited++<4096&&sound_catalog.size()<512){
                if(it.depth()>8)it.disable_recursion_pending();
                if(it->is_regular_file(ec)){
                    const auto relative=it->path().lexically_relative(base).generic_u8string();
                    if(world_soundscape::asset_path(relative))sound_catalog.push_back(relative);
                }
                ec.clear();it.increment(ec);
            }
        }
        std::sort(sound_catalog.begin(),sound_catalog.end());sound_catalog.erase(std::unique(sound_catalog.begin(),sound_catalog.end()),sound_catalog.end());
        sound_catalog_scanned=true;if(sound_clip.empty()&&!sound_catalog.empty())sound_clip=sound_catalog.front();
    }
    static bool sound_wav_supported(const std::vector<unsigned char> &bytes,std::string &error){
        auto u16=[&](std::size_t i){return unsigned(bytes[i])|(unsigned(bytes[i+1])<<8);};
        auto u32=[&](std::size_t i){return std::uint64_t(bytes[i])|(std::uint64_t(bytes[i+1])<<8)|(std::uint64_t(bytes[i+2])<<16)|(std::uint64_t(bytes[i+3])<<24);};
        if(bytes.size()<12||std::memcmp(bytes.data(),"RIFF",4)||std::memcmp(bytes.data()+8,"WAVE",4)){error="Choose a PCM WAV file.";return false;}
        const auto end=u32(4)+8;if(end>bytes.size()||end<12){error="The WAV header is incomplete.";return false;}
        unsigned tag=0,channels=0,rate=0,bits=0,align=0;std::uint64_t data_size=0;bool format=false,data=false;
        for(std::uint64_t at=12;at+8<=end;){const auto size=u32(std::size_t(at)+4),next=at+8+size+(size&1);if(next>end){error="The WAV contains a truncated chunk.";return false;}
            if(!std::memcmp(bytes.data()+at,"fmt ",4)){if(format||size<16||size>4096){error="The WAV format header is invalid.";return false;}format=true;tag=u16(at+8);channels=u16(at+10);rate=unsigned(u32(at+12));align=u16(at+20);bits=u16(at+22);}
            if(!std::memcmp(bytes.data()+at,"data",4)){if(data||!size){error="The WAV needs one nonempty audio stream.";return false;}data=true;data_size=size;}
            at=next;
        }
        if(!format||!data||channels<1||channels>2||rate<8000||rate>96000||!((tag==1&&bits==16)||(tag==3&&bits==32))||align!=channels*(bits/8)||data_size%std::max(1u,align)){error="Use PCM16 or float32 WAV, mono/stereo, 8–96 kHz.";return false;}
        return true;
    }
    bool sound_import_file(){
        if(!sound_can_edit()){message="Stop Play before importing a sound.";return false;}
        const auto source_file=std::filesystem::u8path(sound_import_path);std::error_code ec;
        const auto size=std::filesystem::file_size(source_file,ec);
        if(ec||size<12||size>32u*1024u*1024u){message="Choose an existing WAV up to 32 MiB.";return false;}
        std::ifstream source_stream(source_file,std::ios::binary);std::vector<unsigned char> bytes(std::size_t(size),0);
        if(!source_stream.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size()))){message="Could not read the source WAV.";return false;}
        if(!sound_wav_supported(bytes,message))return false;
        const auto base=std::filesystem::u8path(project_directory);auto destination=base;
        // Never import through the commonly shared immutable audio/ bank.
        for(const char *part:{"sounds","imported"}){
            destination/=part;const auto status=std::filesystem::symlink_status(destination,ec);
            if(ec&&ec!=std::errc::no_such_file_or_directory){message="Cannot inspect the sound import folder: "+ec.message();return false;}ec.clear();
            if(std::filesystem::is_symlink(status)||(std::filesystem::exists(status)&&!std::filesystem::is_directory(status))){message="Sound imports require real project-local sounds/imported folders; symbolic links are not writable import targets.";return false;}
            if(!std::filesystem::exists(status)&&!std::filesystem::create_directory(destination,ec)){message="Cannot create the sound import folder: "+ec.message();return false;}
        }
        std::string stem;for(unsigned char c:source_file.stem().u8string())if(stem.size()<48)stem.push_back((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')?char(c):'_');if(stem.empty())stem="clip";
        std::uint64_t hash=14695981039346656037ull;for(unsigned char c:bytes){hash^=c;hash*=1099511628211ull;}char fingerprint[17];std::snprintf(fingerprint,sizeof fingerprint,"%016llx",static_cast<unsigned long long>(hash));
        const std::string relative="sounds/imported/"+stem+"-"+fingerprint+".wav";destination=base/std::filesystem::u8path(relative);
        const auto status=std::filesystem::symlink_status(destination,ec);if(ec&&ec!=std::errc::no_such_file_or_directory){message="Cannot inspect sound destination.";return false;}ec.clear();
        if(std::filesystem::is_symlink(status)){message="The sound destination is a symbolic link; import cancelled.";return false;}
        if(std::filesystem::exists(status)){
            if(!std::filesystem::is_regular_file(status)||std::filesystem::file_size(destination,ec)!=bytes.size()){message="An existing different import has the same name; it was preserved.";return false;}
            std::ifstream existing(destination,std::ios::binary);std::vector<unsigned char> previous(bytes.size());if(!existing.read(reinterpret_cast<char*>(previous.data()),std::streamsize(previous.size()))||previous!=bytes){message="An existing different import has the same name; it was preserved.";return false;}
        }else{
            if(!std::filesystem::copy_file(source_file,destination,std::filesystem::copy_options::none,ec)){message="Could not copy the WAV into this project: "+ec.message();return false;}
            std::ifstream copied(destination,std::ios::binary);std::vector<unsigned char> check(bytes.size());if(std::filesystem::file_size(destination,ec)!=bytes.size()||!copied.read(reinterpret_cast<char*>(check.data()),std::streamsize(check.size()))||check!=bytes){message="The source WAV changed while importing. The incomplete private copy was removed.";std::filesystem::remove(destination,ec);return false;}
        }
        sound_catalog_refresh();sound_clip=relative;message="WAV imported into this project's sounds/imported folder. Select a spot to place it.";return true;
    }
    bool sound_add(double px,double py,double pz,const std::string &asset){
        if(!sound_can_edit()){message="Stop Play before placing sounds.";return false;}
        if(draft.sounds.size()>=world_soundscape::sound_limit){message="This world has 64 sound placements. Remove one before adding another.";return false;}
        world_soundscape::Sound sound;sound.x=px;sound.y=py;sound.z=pz;sound.asset=asset;sound.name=asset.empty()?"World sound":std::filesystem::u8path(asset).stem().u8string();
        if(!world_soundscape::text(sound.name,world_soundscape::name_limit))sound.name="World sound";
        auto next=draft;next.sounds.push_back(sound);if(!world_studio::valid(next,message))return false;
        designer_finish_edit();checkpoint();draft=std::move(next);sound_selection=int(draft.sounds.size()-1);sound_select(unsigned(sound_selection));sound_cancel();changed(false);sound_preview_apply();designer_tools=true;tab=requested_tab=25;
        message=asset.empty()?"Sound placed. Choose a WAV in the inspector.":"Sound placed. Audition it, or walk through its range. Save keeps the placement.";return true;
    }
    bool sound_set_clip(unsigned index,const std::string &asset){
        if(!sound_can_edit()||index>=draft.sounds.size()||!world_entity::editable(draft.entities,{world_entity::Kind::Sound,index}))return false;
        auto next=draft;next.sounds[index].asset=asset;if(!world_studio::valid(next,message))return false;
        designer_finish_edit();checkpoint();draft=std::move(next);changed(false);sound_preview_apply();return true;
    }
    void sound_delete(){
        if(!sound_can_edit()||sound_selection<0||unsigned(sound_selection)>=draft.sounds.size())return;
        sound_cancel();sound_solo=-1;sound_monitor_changed();
        world_designer::Command command;command.operation=world_designer::Operation::Erase;
        if(sound_select(unsigned(sound_selection))&&designer_run(command)){sound_selection=-1;sound_preview_apply();}
    }
    bool sound_prepare_now(){
        if(!sound_prepare){message="Audio playback is not ready yet.";return false;}
        if(!production_sync(message))return false;
        if(!sound_prepare(draft,message))return false;sound_muted=false;return true;
    }
    void sound_listen(unsigned index){
        if(index>=draft.sounds.size()||draft.sounds[index].asset.empty()){message="Choose a WAV for this sound first.";return;}
        if(!sound_prepare||!sound_audition){message="Audio playback is not ready yet.";return;}
        if(!sound_prepare_now())return;sound_muted=false;sound_monitor_changed();if(sound_audition(index,message))message="Auditioning the selected clip. Stop ends the preview; Solo keeps its real distance falloff.";
    }
    void sound_browser(){
        if(!sound_catalog_scanned)sound_catalog_refresh();
        ImGui::SetNextItemWidth(std::max(120.f,ImGui::GetContentRegionAvail().x-90));ImGui::InputTextWithHint("##audio-search","Find a WAV…",sound_search,sizeof sound_search);ImGui::SameLine();if(ImGui::Button("Refresh"))sound_catalog_refresh();
        if(ImGui::BeginChild("Audio clips",{0,std::max(45.f,ImGui::GetContentRegionAvail().y-62)},true)){
            for(const auto &asset:sound_catalog){if(!world_designer_workspace::matches(asset,sound_search))continue;ImGui::PushID(asset.c_str());
                if(ImGui::Selectable(asset.c_str(),sound_clip==asset))sound_clip=asset;
                if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)){SoundPayload payload;std::snprintf(payload.path,sizeof payload.path,"%s",asset.c_str());ImGui::SetDragDropPayload(sound_payload_type,&payload,sizeof payload);ImGui::TextUnformatted(asset.c_str());ImGui::TextDisabled("Drop on the landscape to place sound");ImGui::EndDragDropSource();}
                ImGui::PopID();
            }
            if(sound_catalog.empty())ImGui::TextWrapped("No project WAVs found. Import one in the Audio inspector.");
        }ImGui::EndChild();
        ImGui::BeginDisabled(sound_clip.empty()||!sound_can_edit());
        if(ImGui::Button("Place selected clip…")){sound_open();sound_placing=true;sound_moving=false;sound_arm_frame=ImGui::GetFrameCount();}
        ImGui::SameLine();if(ImGui::Button("Use for selected sound")&&sound_selection>=0)sound_set_clip(unsigned(sound_selection),sound_clip);
        ImGui::EndDisabled();ImGui::TextDisabled("WAV clips | drag onto the map | files remain inside this project");
    }
    bool sound_asset_budget(){
        std::vector<std::string> assets;for(const auto &sound:draft.sounds)if(!sound.asset.empty()&&std::find(assets.begin(),assets.end(),sound.asset)==assets.end())assets.push_back(sound.asset);
        if(assets.size()<=world_soundscape::asset_limit)return true;message="This world already uses 32 different clips. Reuse one, or remove another clip first.";return false;
    }
    void sound_panel(const Camera &){
        if(!ImGui::BeginTabItem("Audio",nullptr,requested_tab==25?ImGuiTabItemFlags_SetSelected:0))return;tab=25;
        if(!sound_catalog_scanned)sound_catalog_refresh();
        ImGui::TextColored({.50f,.84f,.93f,1},"WORLD AUDIO");ImGui::SameLine();ImGui::TextDisabled("%zu / 64 placements",draft.sounds.size());
        ImGui::TextWrapped("Place a speaker on the map, choose its clip, then set how far it carries.");
        ImGui::BeginDisabled(!sound_can_edit());
        if(ImGui::Button(sound_placing?"Cancel placement":"Place sound on map…",{-1,28})){if(sound_placing)sound_cancel();else{sound_placing=true;sound_moving=false;sound_arm_frame=ImGui::GetFrameCount();designer_asset_cancel();}}
        if(sound_placing||sound_moving)ImGui::TextColored({.61f,.91f,.76f,1},sound_moving?"Click the new spot. Escape cancels.":"Click a spot on the landscape. Escape cancels.");
        if(ImGui::Button("Browse project clips",{-1,0})){designer_audio_browser=true;designer_content=true;}
        if(ImGui::CollapsingHeader("Import a WAV")){ImGui::TextUnformatted("Source WAV file");ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##sound-source","Paste a local .wav file path",sound_import_path,sizeof sound_import_path);if(ImGui::Button("Copy WAV into this project"))sound_import_file();ImGui::TextWrapped("PCM16 / float32, mono or stereo. Imports go into sounds/imported; original files stay intact.");}
        ImGui::SeparatorText("Placed sounds");
        for(unsigned i=0;i<draft.sounds.size();++i){ImGui::PushID(int(i));if(ImGui::Selectable(sound_name(i).c_str(),sound_selection==int(i)))sound_select(i);ImGui::PopID();}
        if(sound_selection>=0&&unsigned(sound_selection)<draft.sounds.size()){
            const unsigned index=unsigned(sound_selection);ImGui::SeparatorText("Selected sound");ImGui::BeginDisabled(!world_entity::editable(draft.entities,{world_entity::Kind::Sound,index}));
            auto &s=draft.sounds[index];const auto previous=s;bool edit=false,rename=false,clip_changed=false;char name[97];std::snprintf(name,sizeof name,"%s",sound_name(index).c_str());
            ImGui::TextUnformatted("Name");ImGui::SetNextItemWidth(-1);if(ImGui::InputText("##sound-name",name,sizeof name)){s.name=name;edit=rename=true;}
            ImGui::TextUnformatted("WAV clip");ImGui::SetNextItemWidth(-1);if(ImGui::BeginCombo("##sound-clip",s.asset.empty()?"Choose a clip…":s.asset.c_str())){if(ImGui::Selectable("Unassigned",s.asset.empty())){s.asset.clear();edit=clip_changed=true;}for(const auto &asset:sound_catalog)if(ImGui::Selectable(asset.c_str(),s.asset==asset)){s.asset=asset;edit=clip_changed=true;}ImGui::EndCombo();}
            auto label=[&](const char*text){ImGui::TextWrapped("%s",text);ImGui::SetNextItemWidth(-1);};
            label("Volume");edit|=ImGui::SliderFloat("##sound-volume",&s.volume,0,1,"%.2f");edit|=ImGui::Checkbox("Enabled",&s.enabled);ImGui::SameLine();edit|=ImGui::Checkbox("Loop",&s.loop);edit|=ImGui::Checkbox("Positional sound",&s.spatial);
            ImGui::BeginDisabled(!s.spatial);label("Full volume radius · metres");edit|=ImGui::DragFloat("##sound-inner-radius",&s.inner_radius,.2f,0,s.radius,"%.1f",ImGuiSliderFlags_AlwaysClamp);label("Silent beyond · metres");edit|=ImGui::DragFloat("##sound-outer-radius",&s.radius,.5f,std::max(.1f,s.inner_radius),10000,"%.1f",ImGuiSliderFlags_AlwaysClamp);ImGui::EndDisabled();
            label("Fade in · seconds");edit|=ImGui::SliderFloat("##sound-fade-in",&s.fade_in,0,60,"%.2f");label("Fade out · seconds");edit|=ImGui::SliderFloat("##sound-fade-out",&s.fade_out,0,60,"%.2f");
            label("Play when");int condition=int(s.weather_condition);if(ImGui::Combo("##sound-weather",&condition,"Always\0Rain\0Dry\0Storm\0")){s.weather_condition=world_soundscape::WeatherCondition(condition);edit=true;}
            if(s.weather_condition!=world_soundscape::WeatherCondition::Always){label("Rain intensity range · minimum / maximum");edit|=ImGui::DragFloatRange2("##sound-rain-range",&s.min_rain,&s.max_rain,.005f,0,1,"%.2f","%.2f",ImGuiSliderFlags_AlwaysClamp);ImGui::TextWrapped("Uses weather at this sound's position. Existing fade times blend it in and out; Audition ignores this condition.");}
            label("East · X");edit|=ImGui::InputDouble("##sound-x",&s.x,.25,2,"%.2f");label("Height · Y");edit|=ImGui::InputDouble("##sound-y",&s.y,.25,2,"%.2f");label("South · Z");edit|=ImGui::InputDouble("##sound-z",&s.z,.25,2,"%.2f");
            if(edit){if(!world_soundscape::valid(s,message)||(clip_changed&&!sound_asset_budget()))s=previous;else{sound_property_changed();if(rename){const auto ref=world_designer::from_binding(draft,{world_entity::Kind::Sound,index});if(auto *node=world_entity::find(draft.entities,ref.id))node->name=s.name;}}}
            if(ImGui::Button("Move to map spot…")){sound_moving=true;sound_placing=false;sound_arm_frame=ImGui::GetFrameCount();designer_asset_cancel();}
            ImGui::SameLine();if(ImGui::Button("Frame")){sound_select(index);designer_frame_selection();}
            if(ImGui::Button("Delete sound")){sound_delete();ImGui::EndDisabled();ImGui::EndDisabled();ImGui::EndTabItem();return;}
            ImGui::EndDisabled();
            ImGui::SeparatorText("Listen");
            if(ImGui::Button("Audition clip"))sound_listen(index);ImGui::SameLine();if(ImGui::Button("Stop")){if(sound_stop)sound_stop();sound_muted=true;sound_solo=-1;sound_monitor_changed();}
            bool solo=sound_solo==int(index);if(ImGui::Checkbox("Solo selected in the world",&solo)){if(sound_prepare_now()){sound_solo=solo?int(index):-1;sound_monitor_changed();}}
        }
        ImGui::EndDisabled();
        if(ImGui::Checkbox("Mute placed sounds",&sound_muted))sound_monitor_changed();
        if(ImGui::Button("Preview all placed sounds",{-1,0})){if(sound_prepare_now()){sound_muted=false;sound_solo=-1;sound_monitor_changed();}}
        ImGui::TextWrapped(dirty?"Changes preview when you release the control. Save keeps them in this world.":"Speaker markers and their ranges are editor guides. Save keeps your sound placements.");
        ImGui::EndTabItem();
    }
    void sound_overlay(const Camera &cam){
        if(tab!=25&&sound_selection<0)return;auto *draw=ImGui::GetBackgroundDrawList();
        auto line=[&](world_studio::Vec a,world_studio::Vec b,ImU32 color){ImVec2 pa,pb;if(project_point(cam,{a.x,a.y,a.z},pa)&&project_point(cam,{b.x,b.y,b.z},pb))draw->AddLine(pa,pb,color,1.5f);};
        for(unsigned i=0;i<draft.sounds.size();++i){if(!world_entity::visible(draft.entities,{world_entity::Kind::Sound,i}))continue;const auto &s=draft.sounds[i];ImVec2 p;if(!project_point(cam,{s.x,s.y+.35,s.z},p))continue;
            const bool selected_sound=sound_selection==int(i);const auto color=selected_sound?IM_COL32(255,207,109,255):s.enabled?IM_COL32(115,213,235,240):IM_COL32(135,143,150,210);
            draw->AddCircleFilled(p,10,IM_COL32(20,29,36,225),20);draw->AddRect({p.x-6,p.y-3},{p.x-2,p.y+3},color,0,0,1.5f);draw->AddTriangle({p.x-2,p.y-3},{p.x+3,p.y-7},{p.x+3,p.y+7},color,1.5f);draw->AddLine({p.x+7,p.y-4},{p.x+9,p.y},color,1.5f);draw->AddLine({p.x+9,p.y},{p.x+7,p.y+4},color,1.5f);
            if(selected_sound){draw->AddText({p.x+14,p.y-7},color,sound_name(i).c_str());if(s.spatial)for(float radius_value:{s.inner_radius,s.radius})for(unsigned j=0;j<64;++j){const double a=j*6.283185307179586/64,b=(j+1)*6.283185307179586/64;line({s.x+std::cos(a)*radius_value,s.y+.03,s.z+std::sin(a)*radius_value},{s.x+std::cos(b)*radius_value,s.y+.03,s.z+std::sin(b)*radius_value},radius_value==s.radius?color:IM_COL32(111,210,231,205));}}
        }
        if((sound_placing||sound_moving)&&target){ImVec2 p;if(project_point(cam,{x,y+.2,z},p)){draw->AddCircle(p,12,IM_COL32(112,228,181,255),24,2);draw->AddText({p.x+16,p.y},IM_COL32(190,240,218,255),sound_moving?"Move sound here":"Place sound here");}}
    }
    void sound_gizmo(const Camera &cam){
        if(sound_drag_axis>=0){
            if(!sound_can_edit()||ImGui::GetIO().AppFocusLost||ImGui::IsKeyPressed(ImGuiKey_Escape)){designer_cancel_edit();sound_drag_axis=-1;sound_input_consumed=true;return;}
            if(!ImGui::IsMouseDown(0)){designer_finish_edit();sound_drag_axis=-1;sound_input_consumed=true;return;}
        }
        if(sound_drag_axis>=0&&!designer_edit_scope.active())sound_drag_axis=-1;
        if(sound_selection<0||unsigned(sound_selection)>=draft.sounds.size()||!sound_can_edit())return;
        const unsigned index=unsigned(sound_selection);if(!world_entity::editable(draft.entities,{world_entity::Kind::Sound,index})||sound_is_placing()||drag.active)return;
        if(sound_drag_axis<0&&gizmo_mode!=0)return;
        auto &io=ImGui::GetIO();const auto viewport=designer_viewport(io.DisplaySize.x,io.DisplaySize.y);
        world_entity::Transform pose;if(!world_designer::world_pose(draft,designer_context(),designer_selection.active,pose,message))return;
        const world_studio::Vec pivot=sound_drag_axis>=0?world_studio::Vec{sound_drag_pose.x,sound_drag_pose.y,sound_drag_pose.z}:world_studio::Vec{pose.x,pose.y,pose.z};
        ImVec2 center;if(!project_point(cam,pivot,center))return;
        const float length=sound_drag_axis>=0?sound_drag_length:float(std::clamp(std::hypot(std::hypot(pivot.x-cam.x,pivot.z-cam.z),pivot.y-cam.y)*.12,1.,15.));
        auto *draw=ImGui::GetBackgroundDrawList();draw->PushClipRect({viewport.x,viewport.y},{viewport.x+viewport.w,viewport.y+viewport.h},true);
        const ImU32 colors[]={IM_COL32(244,91,79,255),IM_COL32(111,220,105,255),IM_COL32(79,155,245,255)};int hover=-1;
        for(int axis=0;axis<3;++axis){auto end=pivot;if(axis==0)end.x+=length;if(axis==1)end.y+=length;if(axis==2)end.z+=length;ImVec2 screen;if(!project_point(cam,end,screen))continue;
            draw->AddLine(center,screen,colors[axis],3);draw->AddCircleFilled(screen,5,colors[axis]);draw->AddText({screen.x+6,screen.y},colors[axis],axis==0?"X":axis==1?"Y":"Z");
            const float dx=screen.x-center.x,dy=screen.y-center.y,den=dx*dx+dy*dy;if(den<4)continue;const float along=std::clamp(((io.MousePos.x-center.x)*dx+(io.MousePos.y-center.y)*dy)/den,0.f,1.f);
            if(std::hypot(io.MousePos.x-center.x-dx*along,io.MousePos.y-center.y-dy*along)<9)hover=axis;
        }draw->PopClipRect();
        if(sound_drag_axis<0&&hover>=0&&viewport.contains(io.MousePos.x,io.MousePos.y)&&!designer_blocks_viewport()&&!io.WantCaptureMouse&&ImGui::IsMouseClicked(0)){
            if(!designer_begin_edit())return;sound_drag_axis=hover;sound_drag_start=io.MousePos;sound_drag_pose=pose;sound_drag_length=length;sound_input_consumed=true;
        }
        if(sound_drag_axis<0)return;sound_input_consumed=true;
        if(io.AppFocusLost||ImGui::IsKeyPressed(ImGuiKey_Escape)){designer_cancel_edit();sound_drag_axis=-1;return;}
        if(!io.MouseDown[0]){designer_finish_edit();sound_drag_axis=-1;return;}
        auto end=pivot;if(sound_drag_axis==0)end.x+=length;if(sound_drag_axis==1)end.y+=length;if(sound_drag_axis==2)end.z+=length;ImVec2 screen;if(!project_point(cam,end,screen))return;
        const double dx=screen.x-center.x,dy=screen.y-center.y,den=dx*dx+dy*dy;if(den<4)return;double amount=((io.MousePos.x-sound_drag_start.x)*dx+(io.MousePos.y-sound_drag_start.y)*dy)/den*length;
        if(snap||io.KeyCtrl)amount=std::round(amount/std::max(.01f,move_snap))*std::max(.01f,move_snap);
        auto next=sound_drag_pose;if(sound_drag_axis==0)next.x+=amount;if(sound_drag_axis==1)next.y+=amount;if(sound_drag_axis==2)next.z+=amount;
        if(next.x!=pose.x||next.y!=pose.y||next.z!=pose.z){world_designer::Command command;command.operation=world_designer::Operation::Transform;command.transform=next;designer_update_edit(command);}
    }
    void sound_input(const Camera &cam){
        if(sound_input_consumed)return;if(sound_drag_axis>=0){sound_input_consumed=true;return;}if(!sound_can_edit()){sound_cancel();return;}auto &io=ImGui::GetIO();
        if(io.AppFocusLost||ImGui::IsKeyPressed(ImGuiKey_Escape)){sound_cancel();return;}
        const auto viewport=designer_viewport(io.DisplaySize.x,io.DisplaySize.y);if(!viewport.contains(io.MousePos.x,io.MousePos.y)||designer_blocks_viewport()||io.WantTextInput||drag.active)return;
        if(ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow|ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))return;
        const auto *payload=ImGui::GetDragDropPayload();std::string dropped;
        if(payload&&payload->IsDataType(sound_payload_type)&&payload->DataSize==int(sizeof(SoundPayload))){const auto *p=static_cast<const SoundPayload*>(payload->Data);if(std::memchr(p->path,0,sizeof p->path))dropped=p->path;if(!world_soundscape::asset_path(dropped))dropped.clear();}
        if(sound_placing||sound_moving||!dropped.empty()){
            target=false;pick(cam,io.MousePos.x,io.MousePos.y);sound_input_consumed=true;
            const bool commit=!dropped.empty()?ImGui::IsMouseReleased(0):(ImGui::GetFrameCount()>sound_arm_frame&&ImGui::IsMouseClicked(0));
            if(commit&&target){if(sound_moving&&sound_selection>=0&&unsigned(sound_selection)<draft.sounds.size()){
                    const unsigned index=unsigned(sound_selection);if(!world_entity::editable(draft.entities,{world_entity::Kind::Sound,index})){message="Unlock the sound before moving it.";return;}
                    designer_finish_edit();checkpoint();auto &s=draft.sounds[index];s.x=x;s.y=y+.12;s.z=z;changed(false);sound_cancel();sound_preview_apply();
                }else sound_add(x,y+.12,z,dropped.empty()?sound_clip:dropped);}
            return;
        }
        if((tab!=25&&sound_selection<0)||!ImGui::IsMouseClicked(0))return;
        double best=16;int selected_sound=-1;for(unsigned i=0;i<draft.sounds.size();++i){if(!world_entity::visible(draft.entities,{world_entity::Kind::Sound,i}))continue;const auto &s=draft.sounds[i];ImVec2 p;if(project_point(cam,{s.x,s.y+.35,s.z},p)){const double distance=std::hypot(p.x-io.MousePos.x,p.y-io.MousePos.y);if(distance<best){best=distance;selected_sound=int(i);}}}
        if(selected_sound>=0){sound_select(unsigned(selected_sound));sound_input_consumed=true;}
    }

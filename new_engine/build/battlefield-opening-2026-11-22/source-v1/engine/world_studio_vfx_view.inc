// Included inside Studio. Render-only previews use bounded particle arrays;
// completed gestures still pass through the normal scene validation fence.
    unsigned vfx_preset=0,vfx_previews=0;
    bool vfx_library_open=true,vfx_placing=false,vfx_solo=false,vfx_previewing=false,vfx_pending_commit=false;
    int vfx_arm_frame=-1,vfx_preview_solo=-2;
    std::uint64_t vfx_preview_revision=~std::uint64_t(0);
    unsigned vfx_preview_commits=~0u;
    world_vfx::Clock vfx_clock;
    std::array<ImVec4,22> vfx_controls={};
    void vfx_control(unsigned i){const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();vfx_controls[i]={a.x,a.y,b.x,b.y};}
    std::string vfx_name(unsigned index)const{
        for(const auto &n:draft.entities.nodes)if(n.binding==world_entity::Binding{world_entity::Kind::Particle,index})return n.name;
        return "Effect "+std::to_string(index+1);
    }
    void vfx_frame(){
        if(particle_selection<0||unsigned(particle_selection)>=draft.particles.size())return;
        const auto &p=draft.particles[unsigned(particle_selection)];const double h=p.height*(p.kind==2?3.2:1.);
        designer_frame_center={p.x,p.y+h*.5,p.z};designer_frame_radius=std::max(1.4,std::hypot(double(p.radius),h*.5));designer_frame_pending=true;
    }
    bool vfx_add(unsigned preset,double px,double py,double pz,bool frame_effect=true){
        if(production_freeze()||production_playing()||!production_can_save()){message="Stop Play before adding VFX.";return false;}
        if(draft.particles.size()>=16){message="The scene already has 16 authored effects. Remove an effect before adding another.";return false;}
        auto p=world_vfx::make(preset,px,py,pz,unsigned(production_id_highwater*7919+98101));
        auto next=draft;next.particles.push_back(p);if(!world_studio::valid(next,message))return false;
        checkpoint();draft.particles.push_back(p);particle_selection=int(draft.particles.size()-1);
        const auto ref=designer_binding_ref({world_entity::Kind::Particle,unsigned(particle_selection)});
        if(ref.kind==world_designer::Kind::Entity)if(auto *node=world_entity::find(draft.entities,ref.id))node->name=world_vfx::presets[std::min(preset,7u)].name;
        if(!ref.empty())designer_select(ref);
        changed();vfx_library_open=false;vfx_placing=false;vfx_clock.restart();designer_tools=true;tab=requested_tab=11;
        if(frame_effect)vfx_frame();
        designer_preview_object_edit();
        message="VFX added. Changes preview live; Save keeps the effect in this world.";return true;
    }
    void vfx_duplicate(){
        if(particle_selection<0||unsigned(particle_selection)>=draft.particles.size()||draft.particles.size()>=16)return;
        const unsigned from=unsigned(particle_selection);auto p=draft.particles[from];p.source=-1;p.x+=std::max(.6f,p.radius*1.5f);p.seed+=7919;
        auto next=draft;next.particles.push_back(p);if(!world_studio::valid(next,message))return;
        const auto name=vfx_name(from)+" copy";checkpoint();draft.particles.push_back(p);particle_selection=int(draft.particles.size()-1);
        const auto ref=designer_binding_ref({world_entity::Kind::Particle,unsigned(particle_selection)});
        if(ref.kind==world_designer::Kind::Entity)if(auto *node=world_entity::find(draft.entities,ref.id))node->name=name;
        if(!ref.empty())designer_select(ref);
        changed();vfx_frame();designer_preview_object_edit();
    }
    void vfx_panel(const Camera &cam){
        ImGui::TextColored({.45f,.82f,1,1},"VFX STUDIO");ImGui::SameLine();ImGui::TextDisabled("%zu / 16 effects",draft.particles.size());
        ImGui::SetNextItemOpen(vfx_library_open);vfx_library_open=ImGui::CollapsingHeader("Effect library");if(vfx_library_open){
            const float width=(ImGui::GetContentRegionAvail().x-6)*.5f;
            for(unsigned i=0;i<world_vfx::presets.size();++i){
                const auto &preset=world_vfx::presets[i];ImGui::PushID(int(i));
                const auto c=preset.color;ImGui::PushStyleColor(ImGuiCol_Button,vfx_preset==i?ImVec4{.16f,.32f,.43f,1}:ImVec4{.10f,.14f,.19f,1});
                if(ImGui::Button(preset.name,{width,34}))vfx_preset=i;
                vfx_control(8+i);if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",preset.description);
                const auto a=ImGui::GetItemRectMin();ImGui::GetWindowDrawList()->AddRectFilled({a.x,a.y},{a.x+3,a.y+34},ImGui::ColorConvertFloat4ToU32({c[0],c[1],c[2],1}));
                ImGui::PopStyleColor();ImGui::PopID();if(!(i%2))ImGui::SameLine(0,6);
            }
            ImGui::TextWrapped("%s",world_vfx::presets[vfx_preset].description);
            ImGui::BeginDisabled(draft.particles.size()>=16||production_playing());
            ImGui::PushStyleColor(ImGuiCol_Button,{.12f,.36f,.29f,1});
            if(ImGui::Button("Add in front of camera",{-1,30})){const auto f=cam.forward();const double px=cam.x+f.x*7,pz=cam.z+f.z*7,py=std::max(double(world_atlas_height(px,pz))+.08,cam.y+f.y*7-1.);vfx_add(vfx_preset,px,py,pz);}
            vfx_control(0);ImGui::PopStyleColor();
            if(ImGui::Button(vfx_placing?"Cancel placement (Esc)":"Place on ground...",{-1,26})){vfx_placing=!vfx_placing;vfx_arm_frame=ImGui::GetFrameCount();designer_asset_cancel();}vfx_control(18);
            ImGui::EndDisabled();
            if(vfx_placing)ImGui::TextColored({.6f,.9f,.75f,1},"Click the landscape to place your effect.");
        }
        ImGui::SeparatorText("Preview simulation");
        if(ImGui::Button(vfx_clock.playing?"Pause":"Resume"))vfx_clock.playing=!vfx_clock.playing;
        vfx_control(1);
        ImGui::SameLine();if(ImGui::Button("Restart"))vfx_clock.restart();
        ImGui::SameLine();ImGui::SetNextItemWidth(85);ImGui::SliderFloat("Speed",&vfx_clock.speed,.1f,2.f,"%.1fx");
        float time=float(vfx_clock.time);ImGui::SetNextItemWidth(-1);if(ImGui::SliderFloat("##vfx-timeline",&time,0,63.99f,"Preview %.2f s")){vfx_clock.time=time;vfx_clock.playing=false;}
        vfx_control(16);if(ImGui::IsItemHovered())ImGui::SetTooltip("Scrub or pause the VFX simulation. This editor clock affects particles and flames while VFX Studio is open; game time resumes when you leave.");
        if(ImGui::Checkbox("Solo selected effect",&vfx_solo))vfx_preview_revision=~std::uint64_t(0);
        vfx_control(2);
        ImGui::SeparatorText("Scene effects");
        if(draft.particles.empty())ImGui::TextWrapped("Choose a preset, then add it to the scene.");
        for(unsigned i=0;i<draft.particles.size();++i){ImGui::PushID(int(i));const auto name=vfx_name(i)+(draft.particles[i].enabled?"":" (off)");
            if(ImGui::Selectable(name.c_str(),particle_selection==int(i))){const auto ref=designer_binding_ref({world_entity::Kind::Particle,i});if(!ref.empty())designer_select(ref);particle_selection=int(i);}
            if(ImGui::IsItemHovered()&&ImGui::IsMouseDoubleClicked(0))vfx_frame();
            ImGui::PopID();
        }
        if(particle_selection>=0&&unsigned(particle_selection)<draft.particles.size()){
            const unsigned index=unsigned(particle_selection);ImGui::PushID(int(index)+100);
            if(ImGui::Button("Frame effect"))vfx_frame();
            ImGui::SameLine();
            if(ImGui::Button("Inspector")){designer_tools=false;}
            ImGui::BeginDisabled(!world_entity::editable(draft.entities,{world_entity::Kind::Particle,index}));
            if(ImGui::Button("Duplicate")){vfx_duplicate();ImGui::EndDisabled();ImGui::PopID();return;}vfx_control(3);ImGui::SameLine();
            if(ImGui::Button("Remove")){if(production_remove({world_entity::Kind::Particle,index})){particle_selection=-1;vfx_solo=false;designer_preview_object_edit();}ImGui::EndDisabled();ImGui::PopID();return;}vfx_control(4);
            auto p=draft.particles[index];bool edit=ImGui::Checkbox("Enabled",&p.enabled);
            if(ImGui::CollapsingHeader("Shape and appearance",ImGuiTreeNodeFlags_DefaultOpen)){
                int kind=int(p.kind);ImGui::BeginDisabled(p.source>=0);if(ImGui::Combo("Renderer",&kind,"Smoke / vapor\0Dust motes\0Fire + embers\0Rising sparks\0Orbiting wisps\0")){p.kind=unsigned(kind);p.radius=std::min(p.radius,p.kind==2?2.f:15.f);p.height=std::min(p.height,p.kind==2?4.f:30.f);edit=true;}ImGui::EndDisabled();
                edit|=ImGui::SliderFloat("Spread (m)",&p.radius,.12f,p.kind==2?2.f:15.f,"%.2f");
                edit|=ImGui::SliderFloat("Height (m)",&p.height,.15f,p.kind==2?4.f:30.f,"%.2f");
                edit|=ImGui::SliderFloat("Intensity",&p.power,0,4,"%.2f");vfx_control(5);
                if(p.kind!=2){edit|=ImGui::ColorEdit3("Tint",p.tint.data(),ImGuiColorEditFlags_NoInputs);vfx_control(6);edit|=ImGui::SliderFloat("Glow",&p.glow,0,8,"%.2f");vfx_control(7);edit|=ImGui::SliderFloat("Size",&p.size,.25f,2.f,"%.2fx");}
                else edit|=ImGui::SliderFloat("Light",&p.light,0,20,"%.1f");
            }
            if(p.kind!=2&&ImGui::CollapsingHeader("Motion and density",ImGuiTreeNodeFlags_DefaultOpen)){
                edit|=ImGui::SliderFloat("Density",&p.rate,.25f,2.f,"%.2fx");edit|=ImGui::SliderFloat("Lifetime",&p.lifetime,.25f,4.f,"%.2fx");
                if(p.kind==0){edit|=ImGui::SliderFloat("Wind",&p.wind,0,2,"%.2f");edit|=ImGui::SliderFloat("Warmth",&p.warmth,0,1,"%.2f");}
                if(ImGui::Button("New variation")){p.seed+=7919;edit=true;}
            }
            const bool position=ImGui::CollapsingHeader("Position");vfx_control(20);if(position){
                const double old_y=p.y;edit|=ImGui::InputDouble("East / X",&p.x,.25,2,"%.2f");edit|=ImGui::InputDouble("Height / Y",&p.y,.25,2,"%.2f");edit|=ImGui::InputDouble("South / Z",&p.z,.25,2,"%.2f");p.ceiling+=p.y-old_y;
                if(ImGui::Button("Snap effect to terrain")){const double py=world_atlas_height(p.x,p.z)+.08;world_vfx::move(p,p.x,py,p.z);edit=true;}vfx_control(19);
                if(target&&ImGui::Button("Move to picked ground")){world_vfx::move(p,x,y+.08,z);edit=true;}
            }
            if(edit){world_vfx::fit_ceiling(p);auto next=draft;next.particles[index]=p;if(world_studio::valid(next,message)){runtime_edit_changed();draft.particles[index]=p;vfx_pending_commit=true;}}
            ImGui::EndDisabled();ImGui::PopID();
        }
        if(ImGui::CollapsingHeader("Edit existing world effect")){
            if(ImGui::BeginCombo("Source","Choose smoke, dust or fire")){for(unsigned i=0;i<base_particles.size()+base_flames.size();++i){int id=i<base_particles.size()?int(i):64+int(i-base_particles.size());std::string label=id<64?"Smoke/dust "+std::to_string(id):"Fire "+std::to_string(id-64);if(ImGui::Selectable(label.c_str())){edit_particle_source(id);vfx_frame();designer_preview_object_edit();}}ImGui::EndCombo();}
        }
        ImGui::Spacing();ImGui::TextDisabled("Live preview  |  Ctrl+Z undo  |  Save keeps VFX");
    }
    void vfx_update(WorldSceneParticles *particles,WorldTorchFlames *flames){
        const bool active=open&&tab==11&&!production_playing()&&!production_freeze();
        if(vfx_pending_commit&&!ImGui::IsAnyItemActive()){vfx_pending_commit=false;designer_preview_object_edit();}
        if(active)vfx_clock.tick(ImGui::GetIO().DeltaTime);else vfx_placing=false;
        world_scene_particles_preview_time(particles,active?vfx_clock.time:-1);world_torch_flames_preview_time(flames,active?vfx_clock.time:-1);
        const int solo=active&&vfx_solo&&particle_selection>=0&&unsigned(particle_selection)<draft.particles.size()?particle_selection:-1;
        if((active||vfx_previewing)&&(active!=vfx_previewing||revision!=vfx_preview_revision||runtime_commits!=vfx_preview_commits||solo!=vfx_preview_solo)){
            RuntimePrepared next;std::string error;
            if(prepare_vfx(active?draft:live,next,error,solo)&&world_scene_particles_set_emitters(particles,next.particles.data(),next.particles.size())&&world_torch_flames_set_emitters(flames,next.flames.data(),next.flames.size())){vfx_previews++;vfx_preview_revision=revision;vfx_preview_commits=runtime_commits;vfx_preview_solo=solo;vfx_previewing=active;}
            else message="VFX preview retained the last valid effect: "+error;
        }
    }
    void vfx_overlay(const Camera &cam){
        if(!open||tab!=11)return;
        auto *draw=ImGui::GetBackgroundDrawList();
        for(unsigned i=0;i<draft.particles.size();++i){const auto &p=draft.particles[i];ImVec2 at;if(!project_point(cam,{p.x,p.y+.12,p.z},at))continue;
            const auto c=p.tint;const auto color=particle_selection==int(i)?IM_COL32(255,220,115,255):ImGui::ColorConvertFloat4ToU32({std::max(.3f,c[0]),std::max(.3f,c[1]),std::max(.3f,c[2]),.9f});
            draw->AddCircle(at,7,color,16,2);draw->AddText({at.x+10,at.y-8},color,vfx_name(i).c_str());
        }
        if(vfx_placing){const auto mouse=ImGui::GetIO().MousePos;draw->AddCircle(mouse,12,IM_COL32(115,230,190,255),24,2);draw->AddText({mouse.x+18,mouse.y},IM_COL32(170,255,220,255),"Click ground to place VFX | Esc cancels");}
    }
    void vfx_world_click(const Camera &cam){
        if(vfx_placing){if(target&&ImGui::GetFrameCount()>vfx_arm_frame)vfx_add(vfx_preset,x,y+.08,z,false);return;}
        double best=28;int found=-1;const auto mouse=ImGui::GetIO().MousePos;
        for(unsigned i=0;i<draft.particles.size();++i){const auto &p=draft.particles[i];ImVec2 at;if(project_point(cam,{p.x,p.y+.12,p.z},at)){const auto distance=std::hypot(at.x-mouse.x,at.y-mouse.y);if(distance<best){best=distance;found=int(i);}}}
        if(found>=0){const auto ref=designer_binding_ref({world_entity::Kind::Particle,unsigned(found)});if(!ref.empty())designer_select(ref);particle_selection=found;}
    }

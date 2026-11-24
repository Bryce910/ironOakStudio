// Included inside Studio. Profiles and areas use the ordinary world document,
// stable entity IDs and shared History. Weather-only edits never cook terrain.
    int weather_selection=-1,weather_arm_frame=-1,weather_drag_axis=-1;
    bool weather_placing=false,weather_moving=false,weather_editing=false,weather_pending=false,weather_input_consumed=false;
    ImVec2 weather_drag_start{};world_entity::Transform weather_drag_pose;float weather_drag_length=1;
    void weather_cancel(){weather_placing=weather_moving=false;weather_arm_frame=-1;if(weather_drag_axis>=0){designer_cancel_edit();weather_drag_axis=-1;}}
    void weather_open(){sound_cancel();landscape_cancel();designer_asset_cancel();designer_tools=true;designer_immersive=false;tab=requested_tab=26;}
    void weather_preview_apply(){if(sound_can_edit()&&!ImGui::IsAnyItemActive())apply=true;else weather_pending=true;}
    void weather_property_changed(){if(!weather_editing)history.checkpoint(runtime_before);weather_editing=true;changed(false);weather_pending=true;}
    void weather_finish_properties(){if(!ImGui::IsAnyItemActive()){weather_editing=false;if(weather_pending&&sound_can_edit()){weather_pending=false;apply=true;}}}
    static world_weather_authoring::Profile weather_preset(int preset){
        world_weather_authoring::Profile p;
        if(preset==1){p.rain_enabled=false;p.rain={0,0};p.cloud={.05f,.15f};p.fog={.02f,.06f};}
        else if(preset==2){p.rain_enabled=false;p.rain={0,0};p.cloud={.70f,.90f};p.fog={.10f,.25f};}
        else if(preset==3){p.rain={.35f,.65f};p.cloud={.65f,.90f};p.fog={.20f,.50f};}
        else if(preset==4){p.rain={.75f,1};p.cloud={.85f,1};p.fog={.40f,.90f};p.thunder_enabled=p.lightning_enabled=true;p.thunder_interval={6,14};p.flash_strength={.70f,1.20f};p.thunder_volume={.50f,.80f};}
        return p;
    }
    world_weather_authoring::Profile weather_existing_profile()const{
        world_weather_authoring::Profile p;p.rain_enabled=draft.look.rain>0;p.rain={draft.look.rain,draft.look.rain};p.cloud={draft.look.cloud,draft.look.cloud};p.fog={draft.look.fog,draft.look.fog};return p;
    }
    bool weather_profile_controls(world_weather_authoring::Profile&p){
        bool edit=false;int preset=0;ImGui::TextUnformatted("Preset");ImGui::SetNextItemWidth(-1);
        if(ImGui::Combo("##weather-preset",&preset,"Custom\0Clear\0Overcast\0Rain\0Storm\0")&&preset){const auto seed=p.seed;p=weather_preset(preset);p.seed=seed;edit=true;}
        auto range=[&](const char*label,world_weather_authoring::Range&r,float low,float high,float speed,const char*format){ImGui::TextWrapped("%s",label);ImGui::SetNextItemWidth(-1);return ImGui::DragFloatRange2((std::string("##weather-")+label).c_str(),&r.minimum,&r.maximum,speed,low,high,format,format,ImGuiSliderFlags_AlwaysClamp);};
        edit|=ImGui::Checkbox("Rain",&p.rain_enabled);ImGui::BeginDisabled(!p.rain_enabled);edit|=range("Rain intensity · minimum / maximum",p.rain,0,1,.005f,"%.2f");ImGui::EndDisabled();
        edit|=ImGui::Checkbox("Thunder",&p.thunder_enabled);ImGui::SameLine();edit|=ImGui::Checkbox("Lightning",&p.lightning_enabled);
        ImGui::BeginDisabled(!p.thunder_enabled&&!p.lightning_enabled);edit|=range("Storm interval · seconds",p.thunder_interval,1,180,.2f,"%.1f");ImGui::EndDisabled();
        ImGui::BeginDisabled(!p.lightning_enabled);edit|=range("Lightning strength · minimum / maximum",p.flash_strength,0,4,.01f,"%.2f");ImGui::EndDisabled();
        ImGui::BeginDisabled(!p.thunder_enabled);edit|=range("Thunder volume · minimum / maximum",p.thunder_volume,0,1,.005f,"%.2f");ImGui::EndDisabled();
        if(ImGui::CollapsingHeader("Clouds, mist and variation")){
            edit|=range("Cloud cover · minimum / maximum",p.cloud,0,1,.005f,"%.2f");edit|=range("Ground mist · minimum / maximum",p.fog,0,1.5f,.005f,"%.2f");
            ImGui::TextUnformatted("Weather variation · seconds");ImGui::SetNextItemWidth(-1);edit|=ImGui::DragFloat("##weather-period",&p.variation_seconds,.2f,.1f,3600,"%.1f",ImGuiSliderFlags_AlwaysClamp);
            ImGui::TextUnformatted("Variation seed");ImGui::SetNextItemWidth(-1);edit|=ImGui::InputScalar("##weather-seed",ImGuiDataType_U32,&p.seed);
        }
        return edit;
    }
    bool weather_select(unsigned index){if(index>=draft.weather_areas.size())return false;const auto ref=designer_binding_ref({world_entity::Kind::Weather,index});if(ref.empty()||!designer_select(ref))return false;weather_selection=int(index);return true;}
    bool weather_add(double px,double py,double pz){
        if(!sound_can_edit())return false;if(draft.weather_areas.size()>=world_weather_authoring::area_limit){message="This world has 16 weather areas. Remove one before adding another.";return false;}
        auto next=draft;world_weather_authoring::Area area;area.x=px;area.y=py;area.z=pz;area.name="Rain area "+std::to_string(next.weather_areas.size()+1);area.profile=weather_preset(3);area.profile.seed=unsigned(next.weather_areas.size()+1);next.weather_areas.push_back(area);
        if(!world_studio::valid(next,message))return false;designer_finish_edit();checkpoint();draft=std::move(next);weather_select(unsigned(draft.weather_areas.size()-1));weather_cancel();changed(false);weather_preview_apply();designer_tools=true;tab=requested_tab=26;message="Weather area placed. Adjust its full-effect radius and blend edge; Save All keeps it.";return true;
    }
    void weather_delete(){if(!sound_can_edit()||weather_selection<0||unsigned(weather_selection)>=draft.weather_areas.size())return;weather_cancel();world_designer::Command c;c.operation=world_designer::Operation::Erase;if(weather_select(unsigned(weather_selection))&&designer_run(c)){weather_selection=-1;weather_preview_apply();}}
    void weather_panel(const Camera &){
        if(!ImGui::BeginTabItem("Weather",nullptr,requested_tab==26?ImGuiTabItemFlags_SetSelected:0))return;tab=26;ImGui::TextColored({.55f,.80f,.95f,1},"WORLD WEATHER");
        ImGui::TextWrapped("Set the world's baseline, then place areas where the weather should change. Rain, wet surfaces and weather sounds follow the same areas.");
        ImGui::BeginDisabled(!sound_can_edit());
        if(ImGui::CollapsingHeader("Global baseline",ImGuiTreeNodeFlags_DefaultOpen)){
            auto &g=draft.weather_global;const auto before=g;bool edit=false;
            if(ImGui::Checkbox("Override existing world weather",&g.enabled)){if(g.enabled&&world_weather_authoring::default_global(before))g.profile=weather_existing_profile();edit=true;}
            if(!g.enabled)ImGui::TextWrapped("Existing world weather remains the baseline outside your areas. Enable the override to choose a new global preset.");
            ImGui::BeginDisabled(!g.enabled);ImGui::PushID("global-weather");edit|=weather_profile_controls(g.profile);ImGui::TextUnformatted("Transition · seconds");ImGui::SetNextItemWidth(-1);edit|=ImGui::DragFloat("##global-weather-transition",&g.transition,.1f,0,120,"%.1f",ImGuiSliderFlags_AlwaysClamp);ImGui::PopID();ImGui::EndDisabled();
            if(edit){if(!world_weather_authoring::valid(g,message))g=before;else weather_property_changed();}
        }
        ImGui::SeparatorText("Weather areas");ImGui::TextDisabled("%zu / 16 areas",draft.weather_areas.size());
        if(ImGui::Button(weather_placing?"Cancel placement":"Place weather area on map…",{-1,28})){if(weather_placing)weather_cancel();else{sound_cancel();designer_asset_cancel();weather_placing=true;weather_moving=false;weather_arm_frame=ImGui::GetFrameCount();}}
        if(weather_placing||weather_moving)ImGui::TextColored({.61f,.91f,.76f,1},"Click terrain to place the center. Escape cancels.");
        for(unsigned i=0;i<draft.weather_areas.size();++i){const auto&a=draft.weather_areas[i];ImGui::PushID(int(i));if(ImGui::Selectable((a.name+" · priority "+std::to_string(a.priority)).c_str(),weather_selection==int(i)))weather_select(i);ImGui::PopID();}
        if(weather_selection>=0&&unsigned(weather_selection)<draft.weather_areas.size()){
            const unsigned index=unsigned(weather_selection);auto&a=draft.weather_areas[index];const auto before=a;bool edit=false,rename=false;
            ImGui::SeparatorText("Selected area");ImGui::BeginDisabled(!world_entity::editable(draft.entities,{world_entity::Kind::Weather,index}));char name[97];std::snprintf(name,sizeof name,"%s",a.name.c_str());
            ImGui::TextUnformatted("Name");ImGui::SetNextItemWidth(-1);if(ImGui::InputText("##weather-name",name,sizeof name)){a.name=name;edit=rename=true;}
            edit|=ImGui::Checkbox("Area enabled",&a.enabled);
            auto scalar=[&](const char*label,float&v,float speed,float lo,float hi){ImGui::TextWrapped("%s",label);ImGui::SetNextItemWidth(-1);return ImGui::DragFloat((std::string("##weather-area-")+label).c_str(),&v,speed,lo,hi,"%.1f",ImGuiSliderFlags_AlwaysClamp);};
            edit|=scalar("Full-effect radius · metres",a.inner_radius,.5f,0,a.radius);edit|=scalar("Outer radius · metres",a.radius,.5f,std::max(.1f,a.inner_radius),10000);
            ImGui::TextUnformatted("Priority · higher overrides lower");ImGui::SetNextItemWidth(-1);edit|=ImGui::DragInt("##weather-priority",&a.priority,1,-1000,1000,"%d",ImGuiSliderFlags_AlwaysClamp);
            edit|=scalar("Transition · seconds",a.transition,.1f,0,120);ImGui::PushID("area-profile");edit|=weather_profile_controls(a.profile);ImGui::PopID();
            if(edit){if(!world_weather_authoring::valid(a,message))a=before;else{weather_property_changed();if(rename){const auto ref=world_designer::from_binding(draft,{world_entity::Kind::Weather,index});if(auto*n=world_entity::find(draft.entities,ref.id))n->name=a.name;}}}
            if(ImGui::Button("Move area center…")){sound_cancel();designer_asset_cancel();weather_moving=true;weather_placing=false;weather_arm_frame=ImGui::GetFrameCount();}ImGui::SameLine();if(ImGui::Button("Frame area")){weather_select(index);designer_frame_selection();}
            if(ImGui::Button("Delete area")){weather_delete();ImGui::EndDisabled();ImGui::EndDisabled();ImGui::EndTabItem();return;}ImGui::EndDisabled();
        }
        ImGui::EndDisabled();
        if(ImGui::Button("Wind and gust areas",{-1,0})){weather_cancel();designer_tools=true;tab=requested_tab=23;}
        ImGui::TextWrapped("The inner ring has full effect; the outer ring fades to the surrounding weather. Overlapping areas blend in priority order. Ground soaks and dries gradually. Thunder follows strike distance and is muffled indoors. Save All keeps your profiles and areas.");ImGui::EndTabItem();
    }
    void weather_overlay(const Camera &cam){
        if(tab!=26&&weather_selection<0)return;auto*draw=ImGui::GetBackgroundDrawList();
        auto ring=[&](double x,double y,double z,float radius,ImU32 color){for(unsigned j=0;j<64;++j){const double a=j*6.283185307179586/64,b=(j+1)*6.283185307179586/64;ImVec2 p,q;if(project_point(cam,{x+std::cos(a)*radius,y,z+std::sin(a)*radius},p)&&project_point(cam,{x+std::cos(b)*radius,y,z+std::sin(b)*radius},q))draw->AddLine(p,q,color,1.5f);}};
        for(unsigned i=0;i<draft.weather_areas.size();++i){if(!world_entity::visible(draft.entities,{world_entity::Kind::Weather,i}))continue;const auto&a=draft.weather_areas[i];ImVec2 p;if(!project_point(cam,{a.x,a.y+.4,a.z},p))continue;const auto color=weather_selection==int(i)?IM_COL32(255,207,109,255):a.enabled?IM_COL32(145,195,245,235):IM_COL32(135,143,150,210);draw->AddCircleFilled(p,11,IM_COL32(20,29,36,225),20);draw->AddCircle({p.x-4,p.y},4,color,12,1.5f);draw->AddCircle({p.x+1,p.y-3},5,color,12,1.5f);draw->AddLine({p.x-8,p.y+5},{p.x+8,p.y+5},color,1.5f);
            if(weather_selection==int(i)){draw->AddText({p.x+15,p.y-7},color,a.name.c_str());ring(a.x,a.y+.04,a.z,a.inner_radius,IM_COL32(130,180,230,220));ring(a.x,a.y+.04,a.z,a.radius,color);}}
        if((weather_placing||weather_moving)&&target){ImVec2 p;if(project_point(cam,{x,y+.3,z},p)){draw->AddCircle(p,12,IM_COL32(112,228,181,255),24,2);draw->AddText({p.x+16,p.y},IM_COL32(190,240,218,255),weather_moving?"Move weather center here":"Place weather area here");}}
    }
    void weather_gizmo(const Camera &cam){
        if(weather_drag_axis>=0){
            if(!sound_can_edit()||ImGui::GetIO().AppFocusLost||ImGui::IsKeyPressed(ImGuiKey_Escape)){designer_cancel_edit();weather_drag_axis=-1;weather_input_consumed=true;return;}
            if(!ImGui::IsMouseDown(0)){designer_finish_edit();weather_drag_axis=-1;weather_input_consumed=true;return;}
        }
        if(weather_drag_axis>=0&&!designer_edit_scope.active())weather_drag_axis=-1;
        if(weather_selection<0||unsigned(weather_selection)>=draft.weather_areas.size()||!sound_can_edit())return;
        const unsigned index=unsigned(weather_selection);if(!world_entity::editable(draft.entities,{world_entity::Kind::Weather,index})||(weather_placing||weather_moving)||drag.active)return;
        if(weather_drag_axis<0&&gizmo_mode!=0)return;
        auto &io=ImGui::GetIO();const auto viewport=designer_viewport(io.DisplaySize.x,io.DisplaySize.y);
        world_entity::Transform pose;if(!world_designer::world_pose(draft,designer_context(),designer_selection.active,pose,message))return;
        const world_studio::Vec pivot=weather_drag_axis>=0?world_studio::Vec{weather_drag_pose.x,weather_drag_pose.y,weather_drag_pose.z}:world_studio::Vec{pose.x,pose.y,pose.z};
        ImVec2 center;if(!project_point(cam,pivot,center))return;
        const float length=weather_drag_axis>=0?weather_drag_length:float(std::clamp(std::hypot(std::hypot(pivot.x-cam.x,pivot.z-cam.z),pivot.y-cam.y)*.12,1.,15.));
        auto *draw=ImGui::GetBackgroundDrawList();draw->PushClipRect({viewport.x,viewport.y},{viewport.x+viewport.w,viewport.y+viewport.h},true);
        const ImU32 colors[]={IM_COL32(244,91,79,255),IM_COL32(111,220,105,255),IM_COL32(79,155,245,255)};int hover=-1;
        for(int axis=0;axis<3;++axis){auto end=pivot;if(axis==0)end.x+=length;if(axis==1)end.y+=length;if(axis==2)end.z+=length;ImVec2 screen;if(!project_point(cam,end,screen))continue;
            draw->AddLine(center,screen,colors[axis],3);draw->AddCircleFilled(screen,5,colors[axis]);draw->AddText({screen.x+6,screen.y},colors[axis],axis==0?"X":axis==1?"Y":"Z");
            const float dx=screen.x-center.x,dy=screen.y-center.y,den=dx*dx+dy*dy;if(den<4)continue;const float along=std::clamp(((io.MousePos.x-center.x)*dx+(io.MousePos.y-center.y)*dy)/den,0.f,1.f);
            if(std::hypot(io.MousePos.x-center.x-dx*along,io.MousePos.y-center.y-dy*along)<9)hover=axis;
        }draw->PopClipRect();
        if(weather_drag_axis<0&&hover>=0&&viewport.contains(io.MousePos.x,io.MousePos.y)&&!designer_blocks_viewport()&&!io.WantCaptureMouse&&ImGui::IsMouseClicked(0)){
            if(!designer_begin_edit())return;weather_drag_axis=hover;weather_drag_start=io.MousePos;weather_drag_pose=pose;weather_drag_length=length;weather_input_consumed=true;
        }
        if(weather_drag_axis<0)return;weather_input_consumed=true;
        if(io.AppFocusLost||ImGui::IsKeyPressed(ImGuiKey_Escape)){designer_cancel_edit();weather_drag_axis=-1;return;}
        if(!io.MouseDown[0]){designer_finish_edit();weather_drag_axis=-1;return;}
        auto end=pivot;if(weather_drag_axis==0)end.x+=length;if(weather_drag_axis==1)end.y+=length;if(weather_drag_axis==2)end.z+=length;ImVec2 screen;if(!project_point(cam,end,screen))return;
        const double dx=screen.x-center.x,dy=screen.y-center.y,den=dx*dx+dy*dy;if(den<4)return;double amount=((io.MousePos.x-weather_drag_start.x)*dx+(io.MousePos.y-weather_drag_start.y)*dy)/den*length;
        if(snap||io.KeyCtrl)amount=std::round(amount/std::max(.01f,move_snap))*std::max(.01f,move_snap);
        auto next=weather_drag_pose;if(weather_drag_axis==0)next.x+=amount;if(weather_drag_axis==1)next.y+=amount;if(weather_drag_axis==2)next.z+=amount;
        if(next.x!=pose.x||next.y!=pose.y||next.z!=pose.z){world_designer::Command command;command.operation=world_designer::Operation::Transform;command.transform=next;designer_update_edit(command);}
    }
    void weather_input(const Camera &cam){
        if(weather_input_consumed||weather_drag_axis>=0)return;if(!sound_can_edit()){weather_cancel();return;}auto&io=ImGui::GetIO();if(io.AppFocusLost||ImGui::IsKeyPressed(ImGuiKey_Escape)){weather_cancel();return;}
        if(tab!=26&&(weather_placing||weather_moving)){weather_cancel();return;}const auto viewport=designer_viewport(io.DisplaySize.x,io.DisplaySize.y);
        if(!viewport.contains(io.MousePos.x,io.MousePos.y)||designer_blocks_viewport()||io.WantTextInput||drag.active||ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow|ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))return;
        if(weather_placing||weather_moving){target=false;pick(cam,io.MousePos.x,io.MousePos.y);weather_input_consumed=true;if(target&&ImGui::GetFrameCount()>weather_arm_frame&&ImGui::IsMouseClicked(0)){
                if(weather_moving&&weather_selection>=0&&unsigned(weather_selection)<draft.weather_areas.size()){const unsigned index=unsigned(weather_selection);if(!world_entity::editable(draft.entities,{world_entity::Kind::Weather,index})){message="Unlock the weather area before moving it.";return;}designer_finish_edit();checkpoint();auto&a=draft.weather_areas[index];a.x=x;a.y=y+.12;a.z=z;weather_cancel();changed(false);weather_preview_apply();}else weather_add(x,y+.12,z);}return;}
        if((tab!=26&&weather_selection<0)||!ImGui::IsMouseClicked(0))return;double best=16;int index=-1;for(unsigned i=0;i<draft.weather_areas.size();++i){if(!world_entity::visible(draft.entities,{world_entity::Kind::Weather,i}))continue;const auto&a=draft.weather_areas[i];ImVec2 p;if(project_point(cam,{a.x,a.y+.4,a.z},p)){const double distance=std::hypot(p.x-io.MousePos.x,p.y-io.MousePos.y);if(distance<best){best=distance;index=int(i);}}}if(index>=0){weather_select(unsigned(index));weather_input_consumed=true;}
    }

    int surface_point_selection=0,surface_point_move_ribbon=-1;
    bool surface_point_move_armed=false;
    unsigned surface_point_commits=0,surface_point_rejections=0;
    ImVec2 surface_point_move_button;
    bool surface_point_preview_valid=false;std::string surface_point_preview_error;
    bool surface_commit_point(unsigned ribbon,unsigned point,double px,double pz){
        if(apply||production_freeze()||ribbon>=draft.surfaces.ribbons.size()||point>=draft.surfaces.ribbons[ribbon].points.size())return false;
        auto next=draft;auto &p=next.surfaces.ribbons[ribbon].points[point];
        if(std::hypot(px-p.x,pz-p.z)<.001){surface_point_move_armed=false;message="Point position is unchanged.";return true;}
        p.x=px;p.z=pz;std::string error;world_surface::BakedRibbon path;
        if(!world_studio::valid(next,error)||!world_surface::bake_ribbon(next.surfaces.ribbons[ribbon],path,error)){
            ++surface_point_rejections;landscape_creation_failure("Path unchanged: "+error);return false;
        }
        landscape_river_source();
        if(!landscape_river_roots_ok){++surface_point_rejections;landscape_creation_failure("Path unchanged: existing root protection could not be prepared.");return false;}
        const double reach=path.source.width*.5+path.source.shoulder+2.9;
        for(const auto &sample:path.samples){
            const auto &v=sample.position;
            if(landscape_river_roots.overlaps(v.x,v.z,reach)){++surface_point_rejections;landscape_creation_failure("Path unchanged: a tree, plant or object is inside the channel or banks.");return false;}
            for(const auto &w:draft.waters)if(w.source==3&&w.enabled){const double dx=(v.x-w.x)/(reach+w.radius_x+2),dz=(v.z-w.z)/(reach+w.radius_z+2);if(dx*dx+dz*dz<1){++surface_point_rejections;landscape_creation_failure("Path unchanged: its banks overlap a pond or lake.");return false;}}
            for(double z=std::floor((v.z-reach)/2)*2;z<=v.z+reach;z+=2)for(double x=std::floor((v.x-reach)/2)*2;x<=v.x+reach;x+=2)
                if(std::hypot(x-v.x,z-v.z)<=reach&&landscape_fixed_ground_protected(x,z,this)){++surface_point_rejections;landscape_creation_failure("Path unchanged: the full channel or banks overlap protected terrain, a building or route.");return false;}
        }
        if(!world_pond_field::prepare(next,{world_atlas_base_height},error)){
            ++surface_point_rejections;landscape_creation_failure("Path unchanged: "+error);return false;
        }
        designer_finish_edit();cancel_designer_drag();checkpoint();draft=std::move(next);changed(true);
        landscape_wait_for_build();apply=true;surface_point_move_armed=false;++surface_point_commits;
        message="Path point moved. Rebuilding the channel and banks; one Undo restores the previous route.";return true;
    }
    void paths_panel(){
        if(!ImGui::BeginTabItem("Wind / Roads",nullptr,requested_tab==23?ImGuiTabItemFlags_SetSelected:0))return;
        tab=23;auto &data=draft.surfaces;
        if(ImGui::CollapsingHeader("Local wind volumes",ImGuiTreeNodeFlags_DefaultOpen)){
            if(ImGui::Button("New wind volume at target")&&target&&data.winds.size()<world_surface::wind_limit){checkpoint();world_surface::Wind w;w.name="Wind "+std::to_string(data.winds.size()+1);w.center={x,y+3,z};w.seed=unsigned(data.winds.size())+313;data.winds.push_back(w);surface_wind=int(data.winds.size()-1);changed();}
            for(unsigned i=0;i<data.winds.size();i++)if(ImGui::Selectable(data.winds[i].name.c_str(),surface_wind==int(i)))surface_wind=int(i);
            if(surface_wind>=0&&unsigned(surface_wind)<data.winds.size()){
                auto &w=data.winds[unsigned(surface_wind)];bool e=ImGui::InputScalarN("Wind center",ImGuiDataType_Double,w.center.data(),3,nullptr,nullptr,"%.2f");
                e|=ImGui::SliderFloat3("Wind radius XYZ",w.radius.data(),.5,128);e|=ImGui::SliderFloat2("Wind direction XZ",w.direction.data(),-1,1);e|=ImGui::SliderFloat("Wind speed",&w.speed,0,5);e|=ImGui::SliderFloat("Gust amount",&w.gust,0,1);e|=ImGui::SliderFloat("Gust period (seconds)",&w.period,.5,64);if(e)runtime_edit_changed();
                if(ImGui::Button("Move wind to target")&&target){checkpoint();w.center={x,y+3,z};changed();}
                if(ImGui::Button("Remove wind volume")){checkpoint();data.winds.erase(data.winds.begin()+surface_wind);surface_wind=-1;changed();}
            }
            ImGui::TextWrapped("Smooth ellipsoid falloff adds to global weather wind. Gusts are deterministic and periodic; combined speed is capped at5m/s. Vegetation color and shadow use the same displacement.");
        }
        if(ImGui::CollapsingHeader("Terrain road / river splines",ImGuiTreeNodeFlags_DefaultOpen)){
            auto create=[&](world_surface::RibbonKind kind){if(!target||data.ribbons.size()>=world_surface::ribbon_limit)return;checkpoint();world_surface::Ribbon r;r.kind=kind;r.name=(kind==world_surface::RibbonKind::Road?"Road ":"River ")+std::to_string(data.ribbons.size()+1);r.layer=data.layers.empty()?-1:std::clamp(surface_layer,0,int(data.layers.size()-1));r.points={{x,y,z},{x+12,y,z}};data.ribbons.push_back(r);surface_ribbon=int(data.ribbons.size()-1);changed(true);};
            if(ImGui::Button("New road at target"))create(world_surface::RibbonKind::Road);
            ImGui::SameLine();if(ImGui::Button("New river at target"))create(world_surface::RibbonKind::River);
            for(unsigned i=0;i<data.ribbons.size();i++)if(ImGui::Selectable(data.ribbons[i].name.c_str(),surface_ribbon==int(i))){if(surface_ribbon!=int(i)){surface_point_selection=0;surface_point_move_armed=false;}surface_ribbon=int(i);}
            if(surface_ribbon>=0&&unsigned(surface_ribbon)<data.ribbons.size()){
                auto &r=data.ribbons[unsigned(surface_ribbon)];char name[65];std::snprintf(name,sizeof name,"%s",r.name.c_str());bool e=false;if(ImGui::InputText("Path name",name,sizeof name)){r.name=name;e=true;}
                e|=ImGui::Checkbox("Smooth Catmull-Rom curve",&r.smooth);e|=ImGui::SliderFloat("Path width (m)",&r.width,.5,32);e|=ImGui::SliderFloat("Shoulder / bank (m)",&r.shoulder,.1,12);
                if(r.kind==world_surface::RibbonKind::River)e|=ImGui::SliderFloat("Channel depth (m)",&r.depth,.1,8);
                e|=surface_layer_combo(r.kind==world_surface::RibbonKind::River?"Riverbed / bank material":"Road surface",r.layer,true);
                if(e)runtime_edit_changed(true);
                surface_point_selection=std::clamp(surface_point_selection,0,int(r.points.size()-1));
                ImGui::TextWrapped("Click a numbered point in the scene to select it. Pick a new position below, then click terrain; Esc cancels. The point's channel height stays unchanged.");
                if(ImGui::Button(surface_point_move_armed?"Cancel point move":"Pick selected point position",{-1,30})){
                    const bool arm=!surface_point_move_armed;landscape_cancel();surface_point_move_armed=arm;surface_point_move_ribbon=surface_ribbon;
                    if(arm)message="Move the preview point over terrain, then click to commit one path edit. Esc cancels.";
                }
                {const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();surface_point_move_button={(a.x+b.x)*.5f,(a.y+b.y)*.5f};}
                if(ImGui::Button("Move selected point to picked target",{-1,0})&&target){surface_commit_point(unsigned(surface_ribbon),unsigned(surface_point_selection),x,z);ImGui::EndTabItem();return;}
                for(unsigned i=0;i<r.points.size();i++){ImGui::PushID(int(i));if(ImGui::Selectable(("Point "+std::to_string(i+1)).c_str(),surface_point_selection==int(i))){surface_point_selection=int(i);surface_point_move_armed=false;}double values[]={r.points[i].x,r.points[i].y,r.points[i].z};if(ImGui::InputScalarN("Point XYZ",ImGuiDataType_Double,values,3,nullptr,nullptr,"%.2f")){r.points[i]={values[0],values[1],values[2]};runtime_edit_changed(true);}ImGui::PopID();}
                if(ImGui::Button("Append target point")&&target&&r.points.size()<32){checkpoint();r.points.push_back({x,y,z});changed(true);}
                ImGui::SameLine();if(ImGui::Button("Remove last point")&&r.points.size()>2){checkpoint();r.points.pop_back();changed(true);}
                if(ImGui::Button("Use current ground heights")){checkpoint();for(auto &p:r.points)p.y=world_atlas_height(p.x,p.z);changed(true);}
                world_surface::BakedRibbon baked;std::string error;if(world_surface::bake_ribbon(r,baked,error))ImGui::Text("%zu samples | %.1fm curve",baked.samples.size(),baked.samples.back().distance);else ImGui::TextWrapped("%s",error.c_str());
                if(ImGui::Button("Remove path")){checkpoint();data.ribbons.erase(data.ribbons.begin()+surface_ribbon);surface_ribbon=-1;changed(true);}
            }
            ImGui::TextWrapped("Keep points0.25–256m apart; split routes beyond1024m. The existing object spline tool remains available for fences and repeated props. Road/river Apply checks fixed foundations and navigation clearance before changing terrain.");
        }
        ImGui::EndTabItem();
    }
    void surface_overlay(const Camera &cam){
        if(!open||tab!=23)surface_point_move_armed=false;
        if(!open||(tab!=22&&tab!=23))return;
        auto *draw=ImGui::GetBackgroundDrawList();auto color=IM_COL32(120,210,255,220);
        auto line=[&](world_surface::Point a,world_surface::Point b){ImVec2 p,q;if(project_point(cam,{a.x,a.y,a.z},p)&&project_point(cam,{b.x,b.y,b.z},q))draw->AddLine(p,q,color,2);};
        if(tab==22&&surface_projection>=0&&unsigned(surface_projection)<draft.surfaces.projections.size()){
            const auto &p=draft.surfaces.projections[unsigned(surface_projection)];std::array<world_surface::Point,8> corners;
            for(unsigned i=0;i<8;i++){
                double v[]={((i&1)?1:-1)*p.size[0]*.5,((i&2)?1:-1)*p.size[1]*.5,((i&4)?1:-1)*p.size[2]*.5};
                for(unsigned axis=0;axis<3;axis++){unsigned j=(axis+1)%3,k=(axis+2)%3;double angle=p.rotation[axis]*.017453292519943295,c=std::cos(angle),s=std::sin(angle),a=v[j],b=v[k];v[j]=c*a-s*b;v[k]=s*a+c*b;}
                corners[i]={p.position[0]+v[0],p.position[1]+v[1],p.position[2]+v[2]};
            }
            for(unsigned i=0;i<8;i++)for(unsigned axis=0;axis<3;axis++)if(!(i&(1u<<axis)))line(corners[i],corners[i|(1u<<axis)]);
        }
        if(tab==23&&surface_ribbon>=0&&unsigned(surface_ribbon)<draft.surfaces.ribbons.size()){
            const auto &original=draft.surfaces.ribbons[unsigned(surface_ribbon)];auto preview=original;
            surface_point_selection=std::clamp(surface_point_selection,0,int(preview.points.size()-1));
            auto &io=ImGui::GetIO();const auto viewport=designer_viewport(io.DisplaySize.x,io.DisplaySize.y);
            const bool eligible=viewport.contains(io.MousePos.x,io.MousePos.y)&&!io.WantCaptureMouse&&!io.MouseDown[1]&&!drag.active&&!designer_blocks_viewport();
            if(surface_point_move_armed&&(surface_point_move_ribbon!=surface_ribbon||ImGui::IsKeyPressed(ImGuiKey_Escape))){surface_point_move_armed=false;message="Point move cancelled; the path is unchanged.";}
            bool have_preview=false;
            if(surface_point_move_armed&&eligible){target=false;pick(cam,io.MousePos.x,io.MousePos.y);if(target){auto &point=preview.points[unsigned(surface_point_selection)];point.x=x;point.z=z;have_preview=true;}}
            world_surface::BakedRibbon r;surface_point_preview_error.clear();surface_point_preview_valid=world_surface::bake_ribbon(preview,r,surface_point_preview_error);
            if(surface_point_preview_valid)for(unsigned i=1;i<r.samples.size();i++){auto a=r.samples[i-1],b=r.samples[i];for(int side:{-1,0,1})line({a.position.x+side*a.right[0]*r.source.width*.5,a.position.y+.12,a.position.z+side*a.right[1]*r.source.width*.5},{b.position.x+side*b.right[0]*r.source.width*.5,b.position.y+.12,b.position.z+side*b.right[1]*r.source.width*.5});
                if(original.kind==world_surface::RibbonKind::River)for(int side:{-1,1}){
                    ImVec2 p,q;const double width=r.source.width*.5+r.source.shoulder;
                    if(project_point(cam,{a.position.x+side*a.right[0]*width,a.position.y+.15,a.position.z+side*a.right[1]*width},p)&&project_point(cam,{b.position.x+side*b.right[0]*width,b.position.y+.15,b.position.z+side*b.right[1]*width},q))draw->AddLine(p,q,IM_COL32(103,199,235,175),1.5f);
                }}
            int hovered=-1;float closest=12*12;
            for(unsigned i=0;i<preview.points.size();++i){const auto &v=preview.points[i];ImVec2 at;if(!project_point(cam,{v.x,v.y+.3,v.z},at))continue;
                const auto tint=surface_point_selection==int(i)?IM_COL32(255,220,110,255):color;
                draw->AddCircleFilled(at,surface_point_selection==int(i)?7.f:5.f,tint);draw->AddText({at.x+9,at.y-6},tint,std::to_string(i+1).c_str());
                const float dx=at.x-io.MousePos.x,dy=at.y-io.MousePos.y,d=dx*dx+dy*dy;if(d<closest){closest=d;hovered=int(i);}
            }
            if(surface_point_move_armed&&have_preview){
                const auto tint=surface_point_preview_valid?IM_COL32(150,245,215,255):IM_COL32(255,150,95,255);
                draw->AddText({io.MousePos.x+18,io.MousePos.y+18},tint,surface_point_preview_valid?"Channel + banks preview | click to rebuild terrain/collision":surface_point_preview_error.c_str());
                if(ImGui::IsMouseClicked(0)){
                    if(surface_point_preview_valid)surface_commit_point(unsigned(surface_ribbon),unsigned(surface_point_selection),x,z);
                    else{++surface_point_rejections;landscape_creation_failure("Path unchanged: "+surface_point_preview_error);}
                    return;
                }
            }else if(eligible&&hovered>=0&&ImGui::IsMouseClicked(0))surface_point_selection=hovered;
        }
        if(tab==23&&surface_wind>=0&&unsigned(surface_wind)<draft.surfaces.winds.size()){
            const auto &w=draft.surfaces.winds[unsigned(surface_wind)];for(unsigned i=0;i<48;i++){double a=i*6.283185307/48,b=(i+1)*6.283185307/48;line({w.center[0]+std::cos(a)*w.radius[0],w.center[1],w.center[2]+std::sin(a)*w.radius[2]},{w.center[0]+std::cos(b)*w.radius[0],w.center[1],w.center[2]+std::sin(b)*w.radius[2]});}
            double n=std::hypot(w.direction[0],w.direction[1]);if(n>.001)line({w.center[0],w.center[1],w.center[2]},{w.center[0]+w.direction[0]*5/n,w.center[1],w.center[2]+w.direction[1]*5/n});
        }
    }

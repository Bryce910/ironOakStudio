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
            for(unsigned i=0;i<data.ribbons.size();i++)if(ImGui::Selectable(data.ribbons[i].name.c_str(),surface_ribbon==int(i)))surface_ribbon=int(i);
            if(surface_ribbon>=0&&unsigned(surface_ribbon)<data.ribbons.size()){
                auto &r=data.ribbons[unsigned(surface_ribbon)];char name[65];std::snprintf(name,sizeof name,"%s",r.name.c_str());bool e=false;if(ImGui::InputText("Path name",name,sizeof name)){r.name=name;e=true;}
                e|=ImGui::Checkbox("Smooth Catmull-Rom curve",&r.smooth);e|=ImGui::SliderFloat("Path width (m)",&r.width,.5,32);e|=ImGui::SliderFloat("Shoulder / bank (m)",&r.shoulder,.1,12);
                if(r.kind==world_surface::RibbonKind::River)e|=ImGui::SliderFloat("Channel depth (m)",&r.depth,.1,8);
                else e|=surface_layer_combo("Road surface",r.layer,true);
                if(e)runtime_edit_changed(true);
                for(unsigned i=0;i<r.points.size();i++){ImGui::PushID(int(i));double values[]={r.points[i].x,r.points[i].y,r.points[i].z};if(ImGui::InputScalarN("Point XYZ",ImGuiDataType_Double,values,3,nullptr,nullptr,"%.2f")){r.points[i]={values[0],values[1],values[2]};runtime_edit_changed(true);}ImGui::PopID();}
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
            world_surface::BakedRibbon r;std::string error;if(world_surface::bake_ribbon(draft.surfaces.ribbons[unsigned(surface_ribbon)],r,error))for(unsigned i=1;i<r.samples.size();i++){auto a=r.samples[i-1],b=r.samples[i];for(int side:{-1,0,1})line({a.position.x+side*a.right[0]*r.source.width*.5,a.position.y+.12,a.position.z+side*a.right[1]*r.source.width*.5},{b.position.x+side*b.right[0]*r.source.width*.5,b.position.y+.12,b.position.z+side*b.right[1]*r.source.width*.5});}
        }
        if(tab==23&&surface_wind>=0&&unsigned(surface_wind)<draft.surfaces.winds.size()){
            const auto &w=draft.surfaces.winds[unsigned(surface_wind)];for(unsigned i=0;i<48;i++){double a=i*6.283185307/48,b=(i+1)*6.283185307/48;line({w.center[0]+std::cos(a)*w.radius[0],w.center[1],w.center[2]+std::sin(a)*w.radius[2]},{w.center[0]+std::cos(b)*w.radius[0],w.center[1],w.center[2]+std::sin(b)*w.radius[2]});}
            double n=std::hypot(w.direction[0],w.direction[1]);if(n>.001)line({w.center[0],w.center[1],w.center[2]},{w.center[0]+w.direction[0]*5/n,w.center[1],w.center[2]+w.direction[1]*5/n});
        }
    }

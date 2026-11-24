    int surface_image_pick=0,surface_import_target=-1;bool surface_replace=false;
    int surface_layer=0,surface_image_role=0,surface_projection=-1,surface_wind=-1,surface_ribbon=-1;
    float surface_brush_radius=4,surface_brush_amount=.5f;bool surface_erase=false;
    char surface_import_path[1024]={};std::future<world_surface::ImportResult> surface_import;
    world_surface::Image surface_import_original;
    static std::string surface_unique_name(const std::string &base,const world_surface::Data &data,bool image){
        for(unsigned i=0;i<100;i++){auto name=base+(i?" "+std::to_string(i+1):"");bool used=false;
            if(image){for(const auto &v:data.images)used|=v.name==name;}else for(const auto &v:data.layers)used|=v.name==name;
            if(!used)return name;}
        return "Unnamed";
    }
    bool surface_layer_combo(const char *label,int &selected,bool source=false){
        const auto &layers=draft.surfaces.layers;const char *preview=selected>=0&&unsigned(selected)<layers.size()?layers[unsigned(selected)].name.c_str():"Preserve source";bool edit=false;
        if(ImGui::BeginCombo(label,preview)){if(source&&ImGui::Selectable("Preserve source",selected<0)){selected=-1;edit=true;}for(unsigned i=0;i<layers.size();i++)if(ImGui::Selectable(layers[i].name.c_str(),selected==int(i))){selected=int(i);edit=true;}ImGui::EndCombo();}return edit;
    }
    bool surface_map_combo(const char *label,int &selected,world_surface::ImageRole role){
        const auto &images=draft.surfaces.images;const char *preview=selected>=0&&unsigned(selected)<images.size()?images[unsigned(selected)].name.c_str():"No image / constant";bool edit=false;
        if(ImGui::BeginCombo(label,preview)){if(ImGui::Selectable("No image / constant",selected<0)){selected=-1;edit=true;}for(unsigned i=0;i<images.size();i++)if(images[i].role==role&&ImGui::Selectable(images[i].name.c_str(),selected==int(i))){selected=int(i);edit=true;}ImGui::EndCombo();}return edit;
    }
    void surface_panel(){
        if(!ImGui::BeginTabItem("Surfaces",nullptr,requested_tab==22?ImGuiTabItemFlags_SetSelected:0))return;
        tab=22;auto &data=draft.surfaces;
        if(surface_import.valid()&&surface_import.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){
            auto result=surface_import.get();if(result.ok){
                world_surface::Data next;int pick=surface_image_pick;
                if(world_surface::stage_image_import(data,result.image,surface_import_target,surface_import_target>=0?&surface_import_original:nullptr,next,pick,message)){
                    checkpoint();data=std::move(next);surface_image_pick=pick;changed();message="Image revision staged. Apply prepares filtered maps; Undo restores the previous revision.";
                }
            }else message=result.error;

        }
        if(ImGui::CollapsingHeader("Texture library",ImGuiTreeNodeFlags_DefaultOpen)){
            ImGui::InputText("Source image path",surface_import_path,sizeof surface_import_path);
            if(!data.images.empty())ImGui::Checkbox("Reimport selected image (preserve role)",&surface_replace);
            ImGui::Combo("Map role",&surface_image_role,"Albedo / sRGB\0Normal / linear OpenGL\0AO-Roughness-Metallic / linear\0");
            if(surface_import.valid())ImGui::TextUnformatted("Validating and copying the source in the background...");
            else if(ImGui::Button(surface_replace?"Stage image revision":"Import image")&&(surface_replace||data.images.size()<world_surface::image_limit)){surface_import_target=surface_replace&&!data.images.empty()?std::clamp(surface_image_pick,0,int(data.images.size()-1)):-1;if(surface_import_target>=0)surface_import_original=data.images[unsigned(surface_import_target)];const std::string source=surface_import_path,project=project_directory;const auto role=surface_import_target>=0?surface_import_original.role:world_surface::ImageRole(surface_image_role);surface_import=std::async(std::launch::async,[project,source,role](){return world_surface::import_image(project,source,role);});}
            ImGui::Text("%zu / %u images | source originals retained",data.images.size(),world_surface::image_limit);
            for(unsigned i=0;i<data.images.size();i++){std::string label=std::to_string(i)+": "+data.images[i].name;if(ImGui::Selectable(label.c_str(),surface_image_pick==int(i)))surface_image_pick=int(i);}
            if(surface_image_pick>=0&&unsigned(surface_image_pick)<data.images.size()){
                bool used=false;for(const auto &l:data.layers)used|=l.color==surface_image_pick||l.normal==surface_image_pick||l.arm==surface_image_pick;
                for(const auto &g:draft.graphs)for(const auto &n:g.nodes)used|=world_graph::texture_op(n.op)&&int(n.value[3])==surface_image_pick;
                ImGui::BeginDisabled(used||surface_import.valid());if(ImGui::Button("Remove unused image")){checkpoint();data.images.erase(data.images.begin()+surface_image_pick);for(auto &l:data.layers)for(auto map:{&l.color,&l.normal,&l.arm})if(*map>surface_image_pick)(*map)--;for(auto &g:draft.graphs)for(auto &n:g.nodes)if(world_graph::texture_op(n.op)&&n.value[3]>surface_image_pick)n.value[3]--;surface_image_pick=std::max(0,surface_image_pick-1);changed();}ImGui::EndDisabled();
                if(used)ImGui::TextWrapped("Used images can be reimported; remove their material/graph references before deletion.");
            }
            ImGui::TextWrapped("Imported sources are kept in the project. Runtime tiles are filtered at1020px; originals keep their full resolution. Choose OpenGL or DirectX normal convention on each material layer. ARM is AO/R, roughness/G, metallic/B.");
        }
        if(ImGui::Button("New PBR layer")&&data.layers.size()<world_surface::layer_limit){checkpoint();world_surface::Layer layer;layer.name=surface_unique_name("Material",data,false);data.layers.push_back(layer);surface_layer=int(data.layers.size()-1);changed();}
        surface_layer_combo("Active layer",surface_layer);
        if(surface_layer>=0&&unsigned(surface_layer)<data.layers.size()){
            auto &layer=data.layers[unsigned(surface_layer)];char name[65];std::snprintf(name,sizeof name,"%s",layer.name.c_str());bool edit=false;
            if(ImGui::InputText("Layer name",name,sizeof name)){layer.name=name;edit=true;}
            edit|=surface_map_combo("Albedo",layer.color,world_surface::ImageRole::Color);edit|=surface_map_combo("Normal",layer.normal,world_surface::ImageRole::Normal);edit|=surface_map_combo("ARM",layer.arm,world_surface::ImageRole::ARM);
            edit|=ImGui::ColorEdit3("Layer tint",layer.tint.data(),ImGuiColorEditFlags_Float);edit|=ImGui::SliderFloat("Metres per tile / UV divisor",&layer.metres,.05,16,"%.2f");
            edit|=ImGui::SliderFloat("Roughness multiplier",&layer.roughness,.04,1);edit|=ImGui::SliderFloat("Metallic multiplier",&layer.metallic,0,1);edit|=ImGui::SliderFloat("Normal strength",&layer.normal_strength,0,2);edit|=ImGui::Checkbox("DirectX normal (invert green)",&layer.flip_normal_y);
            if(edit)runtime_edit_changed();
            if(ImGui::CollapsingHeader("Blend on selected model / terrain",ImGuiTreeNodeFlags_DefaultOpen)){
                const auto alias=active_alias();auto binding=[&](const std::string &target){auto at=std::find_if(data.bindings.begin(),data.bindings.end(),[&](const auto &b){return b.alias==target;});if(at==data.bindings.end()&&data.bindings.size()<64){checkpoint();world_surface::Binding b;b.alias=target;b.first=surface_layer;data.bindings.push_back(b);changed();}};
                if(ImGui::Button("Bind selected model"))binding(alias);
                ImGui::SameLine();if(ImGui::Button("Bind terrain"))binding("__terrain__");
                ImGui::TextWrapped("Selected alias: %s. A model binding affects every instance/LOD of that alias.",alias.c_str());
                for(unsigned i=0;i<data.bindings.size();i++){
                    auto &b=data.bindings[i];ImGui::PushID(int(i));if(ImGui::TreeNode(b.alias.c_str())){bool e=surface_layer_combo("First",b.first,true);e|=surface_layer_combo("Second",b.second,true);int mode=int(b.mode);
                        if(ImGui::Combo("Mask",&mode,"Constant / first layer\0World noise\0World height\0Surface slope\0")){b.mode=world_surface::Blend(mode);e=true;}
                        e|=ImGui::SliderFloat("Overall amount",&b.amount,0,1);e|=ImGui::InputFloat("UV / noise scale",&b.scale,.05,.5);e|=ImGui::InputFloat("Mask threshold",&b.threshold,.05,.5);e|=ImGui::InputFloat("Mask softness",&b.softness,.01,.1);if(e)runtime_edit_changed();
                        bool remove=ImGui::Button("Remove binding");ImGui::TreePop();if(remove){checkpoint();data.bindings.erase(data.bindings.begin()+i);changed();ImGui::PopID();break;}}
                    ImGui::PopID();
                }
            }
            if(ImGui::CollapsingHeader("Terrain layer painting",ImGuiTreeNodeFlags_DefaultOpen)){
                ImGui::SliderFloat("Paint radius",&surface_brush_radius,2,64);ImGui::SliderFloat("Paint opacity",&surface_brush_amount,0,1);ImGui::Checkbox("Erase toward original terrain",&surface_erase);
                if(ImGui::Button("Stamp at world target")&&target&&data.paint.size()<world_surface::paint_limit){checkpoint();data.paint.push_back({x,z,surface_brush_radius,surface_brush_amount,surface_layer,surface_erase});changed();}
                ImGui::Text("%zu / %u strokes; bounded64world tiles",data.paint.size(),world_surface::paint_limit);
                if(ImGui::Button("Clear paint strokes")&&!data.paint.empty()){checkpoint();data.paint.clear();changed();}
                ImGui::TextWrapped("Paint changes material weights only. Sculpt and foliage masks remain independent. Shared tile edges sample the same2m grid. Use Apply to rebuild the weight field.");
            }
            bool layer_used=false;for(const auto &b:data.bindings)layer_used|=b.first==surface_layer||b.second==surface_layer;for(const auto &p:data.paint)layer_used|=!p.erase&&p.layer==surface_layer;for(const auto &p:data.projections)layer_used|=p.layer==surface_layer;for(const auto &r:data.ribbons)layer_used|=r.layer==surface_layer;
            ImGui::BeginDisabled(layer_used);bool remove_layer=ImGui::Button("Remove unused layer");ImGui::EndDisabled();if(remove_layer){checkpoint();data.layers.erase(data.layers.begin()+surface_layer);for(auto &b:data.bindings){if(b.first>surface_layer)b.first--;if(b.second>surface_layer)b.second--;}for(auto &p:data.paint)if(p.layer>surface_layer)p.layer--;for(auto &p:data.projections)if(p.layer>surface_layer)p.layer--;for(auto &r:data.ribbons)if(r.layer>surface_layer)r.layer--;surface_layer=std::max(0,surface_layer-1);changed();ImGui::EndTabItem();return;}
            if(ImGui::Button("New image projection at target")&&target&&data.projections.size()<world_surface::projection_limit){checkpoint();world_surface::Projection p;p.name="Projection "+std::to_string(data.projections.size()+1);p.position={x,y,z};p.layer=surface_layer;data.projections.push_back(p);surface_projection=int(data.projections.size()-1);changed();}
        }
        for(unsigned i=0;i<data.projections.size();i++)if(ImGui::Selectable(data.projections[i].name.c_str(),surface_projection==int(i)))surface_projection=int(i);
        if(surface_projection>=0&&unsigned(surface_projection)<data.projections.size()){
            auto &p=data.projections[unsigned(surface_projection)];bool e=surface_layer_combo("Projection layer",p.layer);e|=ImGui::InputScalarN("Projection center",ImGuiDataType_Double,p.position.data(),3,nullptr,nullptr,"%.2f");
            e|=ImGui::SliderFloat3("Projection size XYZ",p.size.data(),.05,32);e|=ImGui::SliderFloat3("Projection rotation XYZ",p.rotation.data(),-180,180);e|=ImGui::SliderFloat("Projection opacity",&p.opacity,0,1);
            e|=ImGui::SliderFloat("Edge feather",&p.feather,.01,.5);e|=ImGui::SliderFloat("Facing cutoff",&p.normal_cutoff,-1,.99);if(e)runtime_edit_changed();
            if(ImGui::Button("Move projection to target")&&target){checkpoint();p.position={x,y,z};changed();}
            if(ImGui::Button("Remove projection")){checkpoint();data.projections.erase(data.projections.begin()+surface_projection);surface_projection=-1;changed();}
        }
        ImGui::TextWrapped("Projection boxes use image alpha and PBR maps on terrain/opaque scenery. Local +Y faces the receiving surface; rotate90degrees for a wall. Nearest8boxes render within180m. Apply validates resources and shaders before replacing the live look.");
        ImGui::EndTabItem();
    }

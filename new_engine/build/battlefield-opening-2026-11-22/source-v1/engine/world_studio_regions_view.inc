    world_regions::Document region_document;
    bool region_modified=false;
    void regions_edited(){region_modified=project_aux_dirty=true;project_saved_together=false;}
    int region_selection=-1,region_layer_selection=-1;
    world_regions::Id region_next_id=1,layer_next_id=1;
    char region_file[512]="world.regions";
    std::string region_path()const{auto file=std::filesystem::path(region_file);return (file.is_relative()?std::filesystem::path(project_directory)/file:file).string();}
    void region_ids(){
        region_next_id=layer_next_id=1;
        const auto maximum=std::numeric_limits<world_regions::Id>::max();
        for(const auto &r:region_document.regions){
            if(r.id==maximum)region_next_id=0;else if(region_next_id)region_next_id=std::max(region_next_id,r.id+1);
            for(const auto &l:r.layers){if(l.id==maximum)layer_next_id=0;else if(layer_next_id)layer_next_id=std::max(layer_next_id,l.id+1);}
        }
    }
    void regions_init(){std::string ignored;if(access(region_path().c_str(),F_OK)==0&&world_regions::load(region_path().c_str(),region_document,ignored))region_ids();}

    void region_membership(){
        if(region_selection<0||unsigned(region_selection)>=region_document.regions.size())return;
        const auto &r=region_document.regions[unsigned(region_selection)];if(region_layer_selection<0||unsigned(region_layer_selection)>=r.layers.size()){message="Choose a region layer first.";return;}
        if(r.locked||r.layers[unsigned(region_layer_selection)].locked){message="Unlock the region and layer before changing membership.";return;}
        const auto previous=draft;if(!production_sync(message))return;
        auto next=region_document;auto &target_layer=next.regions[unsigned(region_selection)].layers[unsigned(region_layer_selection)];
        for(unsigned key:selection_keys){const world_entity::Binding b={key<WORLD_ADDED_KEY_BASE?world_entity::Kind::SourceModel:world_entity::Kind::Placement,key<WORLD_ADDED_KEY_BASE?key:key-WORLD_ADDED_KEY_BASE};const world_entity::Node *found=nullptr;for(const auto &n:draft.entities.nodes)if(n.binding==b){found=&n;break;}if(!found){draft=previous;message="Register this source object before assigning its layer.";return;}for(auto &area:next.regions)for(auto &layer:area.layers)layer.entities.erase(std::remove(layer.entities.begin(),layer.entities.end(),found->id),layer.entities.end());target_layer.entities.push_back(found->id);}
        if(world_regions::validate(next,message)){history.checkpoint(previous);changed();region_document=std::move(next);regions_edited();message="Selected entities assigned. Save All preserves scene identities and region metadata together.";}else draft=previous;
    }
    void region_flags(){
        auto next=draft;
        for(const auto &r:region_document.regions)for(const auto &layer:r.layers)for(auto id:layer.entities){auto *node=world_entity::find(next.entities,id);if(!node){message="Region membership refers to a missing scene entity. Repair its membership before pushing flags.";return;}node->visible=r.visible&&layer.visible;node->locked=r.locked||layer.locked;}
        if(!world_studio::valid(next,message)||!structure_change_allowed(next,message))return;
        checkpoint();draft=std::move(next);changed(true);message="Layer visibility and locks staged on scene entities. Apply rebuilds scenery/collision and enabled attachments.";
    }
    void region_panel(){
        if(!ImGui::BeginTabItem("Regions / Layers",nullptr,requested_tab==18?ImGuiTabItemFlags_SetSelected:0))return;
        tab=18;ImGui::TextWrapped("Organize stable scene IDs into bounded world regions and named layers. Pushing visibility and locks changes the draft; Apply commits the resulting scene.");
        if(ImGui::InputText("Region document",region_file,sizeof region_file))regions_edited();
        if(ImGui::Button("New128m region at target")&&target&&region_document.regions.size()<world_regions::region_limit&&region_next_id){world_regions::Region r;r.id=region_next_id++;r.name="Region "+std::to_string(r.id);r.bounds={{x-64,-1000,z-64},{x+64,12000,z+64}};region_document.regions.push_back(r);region_selection=int(region_document.regions.size()-1);region_layer_selection=-1;regions_edited();}
        for(unsigned i=0;i<region_document.regions.size();i++)if(ImGui::Selectable((std::to_string(region_document.regions[i].id)+" | "+region_document.regions[i].name).c_str(),region_selection==int(i))){region_selection=int(i);region_layer_selection=-1;}
        if(region_selection>=0&&unsigned(region_selection)<region_document.regions.size()){
            auto &r=region_document.regions[unsigned(region_selection)];bool edited=false;char name[65];std::snprintf(name,sizeof name,"%s",r.name.c_str());if(ImGui::InputText("Region name",name,sizeof name)){r.name=name;edited=true;}
            edited|=ImGui::Checkbox("Region visible",&r.visible);ImGui::SameLine();edited|=ImGui::Checkbox("Region locked",&r.locked);
            edited|=ImGui::InputDouble("Minimum east",&r.bounds.minimum.x,1,16);edited|=ImGui::InputDouble("Maximum east",&r.bounds.maximum.x,1,16);edited|=ImGui::InputDouble("Minimum south",&r.bounds.minimum.z,1,16);edited|=ImGui::InputDouble("Maximum south",&r.bounds.maximum.z,1,16);
            edited|=ImGui::InputDouble("Load distance",&r.load_distance,8,32);edited|=ImGui::InputDouble("Preload distance",&r.preload_distance,8,32);edited|=ImGui::InputDouble("Unload distance",&r.unload_distance,8,32);
            if(!r.locked&&r.layers.size()<world_regions::layer_limit&&layer_next_id&&ImGui::Button("New layer")){world_regions::Layer l;l.id=layer_next_id++;l.name="Layer "+std::to_string(l.id);r.layers.push_back(l);region_layer_selection=int(r.layers.size()-1);edited=true;}
            for(unsigned i=0;i<r.layers.size();i++)if(ImGui::Selectable((r.layers[i].name+" | "+std::to_string(r.layers[i].entities.size())+" entities").c_str(),region_layer_selection==int(i)))region_layer_selection=int(i);
            if(region_layer_selection>=0&&unsigned(region_layer_selection)<r.layers.size()){
                auto &layer=r.layers[unsigned(region_layer_selection)];std::snprintf(name,sizeof name,"%s",layer.name.c_str());if(ImGui::InputText("Layer name",name,sizeof name)){layer.name=name;edited=true;}
                edited|=ImGui::Checkbox("Layer visible",&layer.visible);ImGui::SameLine();edited|=ImGui::Checkbox("Layer locked",&layer.locked);
                if(ImGui::Button("Move selected models into layer"))region_membership();
            }
            if(edited)regions_edited();
        }
        if(ImGui::Button("Push layer visibility / locks to draft"))region_flags();
        if(ImGui::Button("Save region document")){if(dirty||!production_project_saved)message="Use Save All to preserve scene identities and regions together.";else if(world_regions::save(region_path().c_str(),region_document,message)){region_modified=false;message="World region/layer metadata saved. Save All also records the complete project checkpoint.";}}
        ImGui::SameLine();if(ImGui::Button("Load region document")){world_regions::Document next;if(world_regions::load(region_path().c_str(),next,message)){region_document=std::move(next);region_selection=region_layer_selection=-1;region_ids();message="Region metadata loaded. Push flags explicitly to stage scene changes.";}}
        ImGui::TextWrapped("These regions organize scene IDs; they do not create independently mergeable streaming worlds. Push flags to stage scene changes. Save All includes region metadata in the same checkpoint as world.studio.");
        ImGui::EndTabItem();
    }

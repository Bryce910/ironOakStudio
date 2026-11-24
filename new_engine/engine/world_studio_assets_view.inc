    char environment_source[1024]={};bool asset_editing_=false;std::future<world_assets::EnvironmentImport> environment_import;
    struct BrowserItem {std::string name;bool directory=false;};
    std::vector<BrowserItem> browser_items;char browser_directory[1024]={};int browser_target=0;bool browser_hdr=false;
    void browse_refresh(){
        browser_items.clear();std::error_code error;std::filesystem::directory_iterator at(browser_directory,error);
        if(error){message="Cannot open that directory: "+error.message();return;}
        for(const auto &entry:at){if(browser_items.size()==512){message="Showing the first512 entries; use a narrower folder.";break;}auto name=entry.path().filename().string();if(name.empty()||name[0]=='.')continue;bool directory=entry.is_directory(error);if(error){error.clear();continue;}auto ext=entry.path().extension().string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return char(std::tolower(c));});if(directory||ext==(browser_hdr?".hdr":".glb"))browser_items.push_back({name,directory});}
        std::sort(browser_items.begin(),browser_items.end(),[](const auto &a,const auto &b){return a.directory!=b.directory?a.directory>b.directory:a.name<b.name;});
    }
    void browse(int target,bool hdr=false){
        browser_target=target;browser_hdr=hdr;if(!*browser_directory){const char *home=std::getenv("HOME");
#ifdef _WIN32
            if(!home)home=std::getenv("USERPROFILE");
#endif
            std::snprintf(browser_directory,sizeof browser_directory,"%s/Downloads",home?home:".");}browse_refresh();ImGui::OpenPopup("Choose source asset");
    }
    void browser_panel(){
        ImGui::SetNextWindowSize({720,470},ImGuiCond_FirstUseEver);if(!ImGui::BeginPopupModal("Choose source asset",nullptr,ImGuiWindowFlags_NoCollapse))return;
        ImGui::InputText("Folder",browser_directory,sizeof browser_directory);ImGui::SameLine();if(ImGui::Button("Open"))browse_refresh();
        if(ImGui::Button("Parent folder")){auto parent=std::filesystem::path(browser_directory).parent_path().string();std::snprintf(browser_directory,sizeof browser_directory,"%s",parent.c_str());browse_refresh();}
        ImGui::BeginChild("Files",{0,-42},true);
        std::string folder;bool chosen=false;
        for(const auto &entry:browser_items){std::string label=(entry.directory?"[Folder] ":"")+entry.name;if(ImGui::Selectable(label.c_str(),false,ImGuiSelectableFlags_AllowDoubleClick)&&ImGui::IsMouseDoubleClicked(0)){auto path=(std::filesystem::path(browser_directory)/entry.name).string();if(entry.directory){folder=path;break;}if(path.size()>=1024){message="Source path is too long.";break;}if(browser_hdr)std::snprintf(environment_source,sizeof environment_source,"%s",path.c_str());else std::snprintf(import_sources[browser_target],sizeof import_sources[0],"%s",path.c_str());chosen=true;break;}}
        ImGui::EndChild();if(!folder.empty()){std::snprintf(browser_directory,sizeof browser_directory,"%s",folder.c_str());browse_refresh();}if(chosen)ImGui::CloseCurrentPopup();ImGui::TextUnformatted("Double-click a file or folder.");ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
    }
    std::string active_alias()const{
        if(selected>=0&&size_t(selected)<source.size())return source[size_t(selected)].alias;
        if(selected_added>=0&&size_t(selected_added)<draft.placements.size())return draft.placements[size_t(selected_added)].alias;
        const auto a=world_scenery_alias(size_t(std::max(alias,0)));return a.name?a.name:"";
    }
    void hierarchy_panel(const Camera &cam){
        if(!ImGui::CollapsingHeader("Nearby scene hierarchy"))return;
        if(ImGui::Button("Refresh nearby objects")){
            std::vector<std::pair<double,unsigned>> roots;roots.reserve(source.size()+draft.placements.size());
            for(unsigned i=0;i<source.size()+draft.placements.size();i++){unsigned key=i<source.size()?i:8192+i-unsigned(source.size());world_studio::Pose p;if(!world_studio::effective_pose(draft,source_poses,key,p))continue;if(*filter&&p.alias.find(filter)==std::string::npos)continue;double distance=std::hypot(p.root.x-cam.x,p.root.z-cam.z);if(distance<250)roots.push_back({distance,key});}
            std::sort(roots.begin(),roots.end());nearby.clear();for(size_t i=0;i<std::min<size_t>(256,roots.size());i++)nearby.push_back(roots[i].second);
        }
        ImGui::BeginChild("Scene roots",{0,150},true);
        for(unsigned key:nearby){world_studio::Pose p;if(!world_studio::effective_pose(draft,source_poses,key,p))continue;if(*filter&&p.alias.find(filter)==std::string::npos)continue;ImGui::PushID(int(key));auto label=std::to_string(key)+" | "+p.alias+(p.hidden?" [hidden]":"");if(ImGui::Selectable(label.c_str(),std::find(selection_keys.begin(),selection_keys.end(),key)!=selection_keys.end()))choose(key,ImGui::GetIO().KeyCtrl);ImGui::PopID();}
        ImGui::EndChild();
    }
    void asset_panel(const world_studio::Document &before){
        const std::string name=active_alias();WorldSceneryAlias info;for(size_t i=0;i<world_scenery_alias_count();i++)if(name==world_scenery_alias(i).name){info=world_scenery_alias(i);break;}
        ImGui::TextWrapped("Selected asset: %s",name.c_str());
        if(ImGui::Button("Inspect LOD0"))inspect_asset(name);
        ImGui::SameLine();if(info.lods>1&&ImGui::Button("Inspect LOD1"))inspect_asset(name,1);
        if(info.lods>2&&ImGui::Button("Inspect LOD2"))inspect_asset(name,2);
        if(!selection_keys.empty()){ImGui::TextUnformatted("Preview the selected object's real mesh:");for(unsigned i=0;i<info.lods;i++){ImGui::PushID(int(i));if(ImGui::Button(("Show LOD"+std::to_string(i)).c_str())){preview_lod=int(i);reload_shaders=apply=true;}ImGui::SameLine();ImGui::PopID();}if(ImGui::Button("Automatic LOD")){preview_lod=-1;reload_shaders=apply=true;}ImGui::TextWrapped("Temporary preview within128m; does not alter saved distances. Automatic selection returns when Studio closes.");}
        if(inspecting.valid())ImGui::TextUnformatted("Reading model/material metadata in the background...");
        if(inspected){
            ImGui::Separator();ImGui::TextWrapped("Inspection: %s",inspected_alias.c_str());const auto &r=*inspected;
            if(!r.supported)ImGui::TextWrapped("Cannot load: %s",r.error.c_str());
            else{ImGui::Text("%llu vertices / %llu triangles",(unsigned long long)r.vertices,(unsigned long long)r.triangles);ImGui::Text("%.2f x %.2f x %.2fm | source %.2fMiB",r.maximum.x-r.minimum.x,r.maximum.y,r.maximum.z-r.minimum.z,r.file_bytes/1048576.);ImGui::Text("%zu maps | RGBA mip estimate %.1fMiB",r.images.size(),r.reference_texture_bytes/1048576.);ImGui::TextWrapped("Geometry uses metres. The RGBA estimate excludes compression/sharing and is not measured VRAM.");
                for(size_t i=0;i<r.materials.size();i++){const auto &m=r.materials[i];ImGui::PushID(int(i));if(ImGui::TreeNode(m.name.c_str())){
                    auto map=[&](const char *label,int index,const char *space){if(index<0||size_t(index)>=r.images.size()){ImGui::Text("%s: absent",label);return;}const auto &im=r.images[size_t(index)];ImGui::Text("%s: image%d %dx%d (%s)",label,index,im.width,im.height,space);};
                    map("Albedo",m.color,"sRGB");map("Normal",m.normal,"linear");map("ARM",m.arm,"linear");map("Emissive",m.emissive,"sRGB");ImGui::Text("Normal %.2f | Roughness %.2f | Metal %.2f",m.normal_strength,m.roughness,m.metallic);ImGui::Text("Alpha: %s",m.blended?"BLEND cutout fallback":(m.masked?"MASK":"OPAQUE"));ImGui::TreePop();}ImGui::PopID();}
            }
            for(const auto &warning:r.warnings)ImGui::TextWrapped("- %s",warning.c_str());
        }
        ImGui::Separator();ImGui::TextUnformatted("Project import / reimport (static GLB)");
        ImGui::Checkbox("Replace selected alias on Apply",&import_replace);
        if(!import_replace)ImGui::InputText("New alias",import_alias,sizeof import_alias);else ImGui::TextWrapped("Reimport target: %s",name.c_str());
        for(int i=0;i<3;i++){ImGui::PushID(i);ImGui::InputText(i==0?"LOD0 source":(i==1?"LOD1 source":"LOD2 (three-stage trees)"),import_sources[i],sizeof import_sources[i]);ImGui::SameLine();if(ImGui::SmallButton("Browse"))browse(i);ImGui::PopID();}
        ImGui::InputFloat("Metres per source unit",&import_units,.01,1,"%.4f");ImGui::Combo("Imported type",&import_vegetation,"Rigid prop\0Plant\0Tree (needs LOD1)\0");ImGui::Combo("Collision",&import_collision,"None\0Box\0Circle\0Trunk\0Preserve source collision\0");
        if(importing.valid())ImGui::TextUnformatted("Validating and copying an immutable revision...");
        else if(ImGui::Button("Import / stage reimport")){
            std::string target=import_replace?name:import_alias;bool already=false;for(size_t i=0;i<world_scenery_alias_count();i++)if(target==world_scenery_alias(i).name)already=true;
            if((!import_replace&&already)||target.empty())message="Choose a unique alias, or enable Replace selected alias.";
            else if(!*import_sources[0])message="Choose an embedded static GLB for LOD0.";
            else if(draft.assets.size()>=WORLD_CUSTOM_ASSET_LIMIT&&std::none_of(draft.assets.begin(),draft.assets.end(),[&](const auto &a){return a.alias==target;}))message="128 asset definitions reached.";
            else{
                import_definition={};import_definition.alias=target;import_definition.lods=*import_sources[2]?3:(*import_sources[1]?2:1);import_definition.collision=unsigned(import_collision);import_definition.vegetation=unsigned(import_vegetation);import_definition.units=import_units;if(import_replace&&info.name){import_definition.distance=info.distance;import_definition.split=info.split;import_definition.vegetation=info.vegetation;import_definition.collision=4;}
                world_studio::Document check;auto definition=import_definition;definition.stem="imports/check/asset";check.assets.push_back(definition);
                if(world_studio::valid(check,message)){const auto cooker=import_cooker,project=project_directory;const double units=import_units;std::array<std::string,3> paths={import_sources[0],import_sources[1],import_sources[2]};importing=std::async(std::launch::async,[cooker,project,target,paths,units](){return world_assets::import_model(cooker,project,target,paths,units);});message="Import running; the committed scene stays active.";}
            }
        }
        ImGui::TextWrapped("Original files are preserved. Reimport creates a new revision; Apply must succeed before it replaces loaded meshes. Undo returns to the previous revision. Animated rigs/FBX need the separate character/cooking pipeline.");
        if(info.name&&ImGui::CollapsingHeader("LOD distance / collision settings")){
            auto at=std::find_if(draft.assets.begin(),draft.assets.end(),[&](const auto &a){return a.alias==name;});
            if(at==draft.assets.end()){if(ImGui::Button("Override asset settings")&&draft.assets.size()<WORLD_CUSTOM_ASSET_LIMIT){checkpoint();world_studio::Asset a;a.alias=name;a.stem=info.stem;a.lods=info.lods;a.collision=4;a.vegetation=info.vegetation;a.distance=info.distance;a.split=std::min(info.split,info.distance-5);draft.assets.push_back(a);changed(true);}}
            else{bool edit=false;edit|=ImGui::SliderFloat("Draw distance (m)",&at->distance,30,2000);edit|=ImGui::SliderFloat("LOD split (m)",&at->split,10,std::min(500.f,at->distance-5));int collision=int(at->collision);if(ImGui::Combo("Collision override",&collision,"None\0Box\0Circle\0Trunk\0Preserve source\0")){at->collision=unsigned(collision);edit=true;}if(edit){if(!asset_editing_)history.checkpoint(before);asset_editing_=true;changed(true);}else if(!ImGui::IsAnyItemActive())asset_editing_=false;ImGui::TextWrapped("Box collision covers the whole model, including doorways. Preserve source keeps authored castle/tavern openings.");}
        }
        browser_panel();
    }

    void environment_panel(const world_studio::Document &before){
        if(environment_import.valid()&&environment_import.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){auto result=environment_import.get();if(result.ok){checkpoint();draft.environment.path=result.path;draft.environment.enabled=true;changed();message="HDRI staged. Apply prepares GGX reflections and diffuse irradiance during loading.";}else message=result.error;}
        ImGui::Separator();ImGui::TextUnformatted("HDR environment");ImGui::InputText("Radiance HDR source",environment_source,sizeof environment_source);ImGui::SameLine();if(ImGui::Button("Browse HDR"))browse(0,true);
        if(!environment_import.valid()){if(ImGui::Button("Import HDR panorama")){const std::string source=environment_source,project=project_directory;environment_import=std::async(std::launch::async,[source,project](){return world_assets::import_environment(project,source);});message="Checking and copying the HDR panorama...";}}else ImGui::TextUnformatted("Validating panorama in the background...");
        auto &e=draft.environment;if(!e.path.empty()){ImGui::TextWrapped("Project copy: %s",e.path.c_str());bool edit=ImGui::Checkbox("Enable HDR environment",&e.enabled);edit|=ImGui::SliderFloat("Environment rotation",&e.rotation,-180,180);edit|=ImGui::SliderFloat("Environment intensity",&e.intensity,0,8);edit|=ImGui::SliderFloat("Environment blend",&e.strength,0,1);if(edit){if(!environment_editing_)history.checkpoint(before);environment_editing_=true;changed();}else if(!ImGui::IsAnyItemActive())environment_editing_=false;}
        ImGui::TextWrapped("Use a2:1 Radiance .hdr panorama. Rotation affects sky, diffuse light and reflections together. Align the separate sun controls to the image. Existing cloud/fog/canopy shelter stays active. Source maps and filtered cubes load only during Apply; intensity/rotation do not rebake them.");browser_panel();
    }
    bool environment_editing_=false;

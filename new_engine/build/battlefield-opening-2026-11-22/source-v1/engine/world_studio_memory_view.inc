    world_npc_memory::Bank npc_memory;
    world_npc_memory::Result npc_recall;
    char npc_memory_path[512]="ai/memories/npc_mara_fenwick.vwmemory";
    char npc_memory_tags[256]="player, jungle, bandits";
    double npc_memory_hour=72;
    bool npc_memory_shareable=false;
    int npc_appearance_target=16;
    world_runtime::Id npc_mind_target=world_runtime_demo::jungle_actor_first;
    std::string npc_loaded_path,npc_appearance_note;
    std::vector<world_npc_appearance::Binding> npc_look_bindings;
    std::string memory_appearance_file()const{return (std::filesystem::path(project_directory)/"ai/npc_appearances.vwlooks").string();}
    bool memory_appearance_resolve(const world_npc_memory::Profile &profile,unsigned source,world_npc_appearance::Resolved &out,std::string &error){
        WorldVillageAppearanceInfo info;
        if(source>=world_village_resident_count||!world_village_population_appearance_info(population_ptr,source,&info)){error="Target resident is unavailable";return false;}
        return world_npc_appearance::resolve(profile.appearance,info.neutral_height,info.capabilities,out,error);
    }
    void memory_appearance_init(bool announce=false){
        const auto path=memory_appearance_file();std::error_code ec;if(!population_ptr||!std::filesystem::exists(path,ec)){if(announce)message="No saved appearance bindings are available; current appearances retained.";return;}
        const auto report=[&](const std::string &diagnostic){if(!announce&&!message.empty())message+=" | NPC appearance: "+diagnostic;else message=diagnostic;};
        std::string error;std::vector<world_npc_appearance::Binding> next;
        if(!world_npc_appearance::load_bindings(path,next,error)){report(error);return;}
        std::array<world_npc_appearance::Resolved,32> resolved;
        for(unsigned i=0;i<next.size();i++){
            auto file=std::filesystem::path(next[i].file);if(file.is_relative())file=std::filesystem::path(project_directory)/file;
            world_npc_memory::Bank bank;if(!bank.load(file.u8string().c_str(),error)||!memory_appearance_resolve(bank.document().profile,next[i].resident,resolved[i],error)){report("Appearance bindings were not changed: "+error);return;}
        }
        std::array<WorldVillageAppearanceChange,64> changes;unsigned count=0;
        for(const auto &old:npc_look_bindings)changes[count++]={old.resident,1,{1,0,1,0,{1,1,1},0},true};
        for(unsigned i=0;i<next.size();i++)changes[count++]={next[i].resident,resolved[i].scale,resolved[i].look,false};
        if(!world_village_population_apply_appearances(population_ptr,changes.data(),count)){report("Appearance reload rejected by the renderer; previous appearances and bindings retained.");return;}
        npc_look_bindings=std::move(next);if(announce)message="Saved NPC appearance bindings loaded";
    }
    void memory_appearance_panel(){
        ImGui::Separator();ImGui::TextUnformatted("Apply to a 3D resident");
        ImGui::TextWrapped("Choose the exact crowd resident to change. This visual slot is separate from the profile's living actor ID. Other residents sharing its model keep their own appearance. Saved bindings reload exported profiles when the engine starts.");
        ImGui::SliderInt("Visual resident slot",&npc_appearance_target,0,int(world_village_resident_count)-1);
        WorldVillageResident resident;WorldVillageAppearanceInfo info;
        const bool available=world_village_population_resident(population_ptr,unsigned(npc_appearance_target),&resident)&&world_village_population_appearance_info(population_ptr,unsigned(npc_appearance_target),&info);
        if(available){ImGui::Text("%s | %s",resident.name,resident.archetype);ImGui::Text("At %.1f, %.1f | neutral height %.1f cm",resident.x,resident.z,100*info.neutral_height);ImGui::TextUnformatted(info.capabilities&WORLD_MODEL_CHARACTER_MASKED_HAIR?"Hair: color, baldness, card density and strand coverage":info.capabilities&WORLD_MODEL_CHARACTER_HAIR?"Hair: color and baldness":"Hair: combined material; appearance retained");}
        ImGui::BeginDisabled(!available||production_playing()||production_freeze());
        if(ImGui::Button("Apply profile appearance")){
            world_npc_appearance::Resolved look;std::string error;
            auto at=std::find_if(npc_look_bindings.begin(),npc_look_bindings.end(),[&](const auto &b){return b.resident==unsigned(npc_appearance_target);});
            if(at==npc_look_bindings.end()&&npc_look_bindings.size()>=32)message="32 appearance bindings reached. Clear one before adding another.";
            else if(npc_loaded_path.empty())message="Load the exported profile first.";
            else if(memory_appearance_resolve(npc_memory.document().profile,unsigned(npc_appearance_target),look,error)&&world_village_population_appearance(population_ptr,unsigned(npc_appearance_target),look.scale,&look.look)){
                if(at==npc_look_bindings.end())npc_look_bindings.push_back({unsigned(npc_appearance_target),npc_loaded_path});else at->file=npc_loaded_path;
                project_aux_dirty=true;npc_appearance_note=look.note;message="Appearance applied to visual resident "+std::to_string(npc_appearance_target)+". Save appearance bindings to retain it.";
            }else message=error.empty()?"Appearance application rejected":error;
        }
        if(ImGui::Button("Clear this resident appearance")){
            world_village_population_appearance(population_ptr,unsigned(npc_appearance_target),1,nullptr);
            npc_look_bindings.erase(std::remove_if(npc_look_bindings.begin(),npc_look_bindings.end(),[&](const auto &b){return b.resident==unsigned(npc_appearance_target);}),npc_look_bindings.end());project_aux_dirty=true;npc_appearance_note.clear();message="Original appearance restored. Save bindings to retain this change.";
        }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(production_playing()||production_freeze());
        if(ImGui::Button("Save appearance bindings")){
            std::string error;
            if(world_npc_appearance::save_project_bindings(project_directory,npc_look_bindings,error))message="Appearance bindings and portable profile snapshots saved in this project.";else message=error;
        }
        ImGui::SameLine();if(ImGui::Button("Reload saved appearances"))memory_appearance_init(true);ImGui::EndDisabled();
        if(!npc_appearance_note.empty())ImGui::TextWrapped("%s",npc_appearance_note.c_str());
        ImGui::TextWrapped("Body shape is a cosmetic scale, not a new anatomical mesh. Density/thickness adjust the existing masked hair cards; new hairstyles, skin/eye masks, age morphs and scars need compatible authored assets.");
    }
    void memory_panel(){
        ImGui::Separator();
        if(!ImGui::CollapsingHeader("NPC memory"))return;
        ImGui::TextWrapped("Load a memory exported by the local browser Memory Studio. This inspector uses the same bounded CPU recall API available to AI decision code. Explicit live bindings below let shareable memories inform conversations and duty-tagged goals guide NPC destinations.");
        ImGui::InputText("Memory export file",npc_memory_path,sizeof npc_memory_path);
        if(ImGui::Button("Load NPC memory")){
            auto path=std::filesystem::path(npc_memory_path);if(path.is_relative())path=std::filesystem::path(project_directory)/path;
            std::string error;if(npc_memory.load(path.u8string().c_str(),error)){npc_loaded_path=path.string();npc_recall={};message="NPC memory loaded: "+npc_memory.document().profile.name;}else message=error;
        }
        if(!npc_memory.ready())return;
        const auto &d=npc_memory.document();ImGui::Text("%s | %s | %zu memories",d.profile.name.c_str(),d.profile.role.c_str(),d.memories.size());
        if(!d.profile.actor_id)ImGui::TextUnformatted("Profile is unbound (actor ID0). No current resident was reassigned.");
        else {const auto at=std::find_if(living_design.actors.begin(),living_design.actors.end(),[&](const auto &a){return a.id==d.profile.actor_id;});if(at==living_design.actors.end())ImGui::Text("Actor ID%u is not present in the current living-world design.",d.profile.actor_id);else ImGui::Text("Actor ID%u: %s",d.profile.actor_id,at->name.c_str());}
        ImGui::TextWrapped("%s",d.profile.summary.c_str());
        if(ImGui::TreeNode("Physical attributes")){
            const auto &a=d.profile.appearance;
            ImGui::TextWrapped("Use Apply to a 3D resident below for supported height, build/weight and hair changes. The selected mesh determines which features can be rendered.");
            if(a.height_cm)ImGui::Text("Height: %.1f cm",*a.height_cm);else ImGui::TextUnformatted("Height: unspecified");
            if(a.weight_kg)ImGui::Text("Weight: %.1f kg",*a.weight_kg);else ImGui::TextUnformatted("Weight: unspecified");
            if(a.age_years)ImGui::Text("Age: %.1f years",*a.age_years);else ImGui::TextUnformatted("Age: unspecified");
            const auto descriptor=[](const char *label,const std::string &s){ImGui::TextWrapped("%s: %s",label,s.empty()?"unspecified":s.c_str());};
            descriptor("Body build",a.build);descriptor("Skin tone",a.skin_tone);descriptor("Eye color",a.eye_color);
            descriptor("Hair color",a.hair_color);descriptor("Hair style",a.hair_style);descriptor("Hair texture",a.hair_texture);
            descriptor("Hair strand thickness",a.hair_thickness);descriptor("Hair density",a.hair_density);
            descriptor("Facial hair",a.facial_hair);descriptor("Distinguishing features",a.distinguishing_features);ImGui::TreePop();
        }
        memory_appearance_panel();
        if(components_ready&&components_runtime){
            ImGui::Separator();ImGui::TextUnformatted("Live mind and witnessed memories");
            components_entity_combo("Memory scene entity",npc_mind_target);
            ImGui::BeginDisabled(production_playing()||production_freeze());
            if(ImGui::Button("Bind profile to live NPC")){
                if(components_runtime->bind_memory(npc_mind_target,d,message)){project_aux_dirty=true;message="Profile connected. Shareable recall and duty goals are active; save mind bindings to retain the authored connection.";}
            }
            if(ImGui::Button("Save live mind bindings")){
                std::error_code ec;const auto folder=std::filesystem::path(project_directory)/"ai";std::filesystem::create_directories(folder,ec);
                if(ec)message=ec.message();else if(components_runtime->save_minds((folder/"npc_minds.vwminds").u8string().c_str(),message))message="Portable live mind bindings saved. Player experiences are kept separately with F5.";
            }
            ImGui::SameLine();if(ImGui::Button("Reload live mind bindings"))components_runtime->load_minds((std::filesystem::path(project_directory)/"ai/npc_minds.vwminds").u8string().c_str(),message);ImGui::EndDisabled();
            if(const auto *seen=components_runtime->experiences()){ImGui::Text("%u experience records | %llu witnessed observations",seen->count,(unsigned long long)seen->sequence);}
            ImGui::TextWrapped("Duty goals use tags duty + guard/work/rest. Sleep and meals retain priority. Private memories may guide decisions but are never surfaced in public conversation. Names do not identify scene residents; bindings use persistent IDs.");
        }
        ImGui::InputText("Recall tags (commas)",npc_memory_tags,sizeof npc_memory_tags);
        ImGui::InputDouble("Recall game hour",&npc_memory_hour,1,24,"%.1f");
        ImGui::Checkbox("Only shareable memories",&npc_memory_shareable);
        if(ImGui::Button("Preview NPC recall")){
            std::array<std::string,world_npc_memory::query_tag_limit> storage;world_npc_memory::Query query;query.game_hour=npc_memory_hour;query.shareable_only=npc_memory_shareable;
            std::istringstream in(npc_memory_tags);std::string tag;bool overflow=false;
            while(std::getline(in,tag,',')){auto first=tag.find_first_not_of(" \t\r\n"),last=tag.find_last_not_of(" \t\r\n");if(first==std::string::npos)continue;tag=tag.substr(first,last-first+1);for(char &c:tag)if(c>='A'&&c<='Z')c=char(c-'A'+'a');if(query.count==storage.size()){overflow=true;break;}storage[query.count]=tag;query.tags[query.count]=storage[query.count];query.count++;}
            npc_recall=overflow?world_npc_memory::Result{}:npc_memory.recall(query);if(!npc_recall.valid)message="Invalid recall query. Use at most16 lowercase trigger tags and a finite game hour.";
        }
        if(npc_recall.valid&&!npc_recall.count)ImGui::TextUnformatted("No memories match this context and game hour.");
        for(unsigned i=0;i<npc_recall.count;i++){
            const auto &hit=npc_recall.hits[i];const auto &m=d.memories[hit.index];ImGui::PushID(int(i));
            if(ImGui::TreeNode(m.title.c_str())){ImGui::Text("Score%.3f | %s | confidence%.0f%% | %s",hit.score,world_npc_memory::source_name(m.source),100*m.confidence,m.shareable?"shareable":"private");ImGui::Text("%u matched tags | age weight%.3f",hit.matched_tags,hit.decay);ImGui::TextWrapped("%s",m.body.c_str());ImGui::TreePop();}ImGui::PopID();
        }
    }

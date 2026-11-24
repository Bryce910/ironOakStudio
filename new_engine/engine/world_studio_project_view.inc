// Coordinated authoring checkpoint. Workers own all copies; runtime saves are
// excluded. Restore is a disk transaction and deliberately requires restart so
// no live subsystem can retain half of an older project generation.
    std::future<std::pair<bool,std::string>> project_save_job;
    bool project_journal_pending=false,project_restart_required=false,project_restoring=false,project_aux_dirty=false,project_quit_after_save=false;
    std::string project_status;WorldSceneryStreamingStats project_streaming;
    static constexpr const char *project_checkpoint_name="world.vwproject";
    void project_init(){project_journal_pending=world_project::pending(project_directory,project_checkpoint_name);}
    void project_poll(){
        if(!project_save_job.valid()||project_save_job.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return;
        std::pair<bool,std::string> result;try{result=project_save_job.get();}catch(const std::exception &e){result={false,std::string("Project I/O failed: ")+e.what()};}project_status=result.second;project_journal_pending=world_project::pending(project_directory,project_checkpoint_name);
        if(result.first){if(project_restoring)project_restart_required=true;else{physics_modified=gameplay_dirty=components_modified=living_template_dirty=project_aux_dirty=false;production_project_saved=true;std::string cleanup_error;if(editor_session&&!editor_session->clean_saved(live,cleanup_error))project_status+=" Recovery cleanup: "+cleanup_error;if(project_quit_after_save)designer_exit_approved=true;}}
        project_restoring=false;project_quit_after_save=false;message=project_status;
    }
    void project_save_all(){
        if(!production_can_save()||project_save_job.valid()||living_save_job.valid()||production_job.valid()||project_journal_pending){message="Finish Play, pending jobs or interrupted-save recovery before saving the project.";return;}
        if(dirty||apply){message="Apply the scene draft first; Save project captures the applied scene and current subsystem drafts.";return;}
        auto scene=live;auto physics=physics_document;auto gameplay=gameplay_document;auto definition=living_design;auto manifest=components_document;
        const bool include_manifest=components_manifest_loaded||components_modified,include_living=living_ready;
        std::string physics_name=include_manifest&&!manifest.physics_asset.empty()?manifest.physics_asset:physics_file;
        std::string gameplay_name=include_manifest&&!manifest.gameplay_asset.empty()?manifest.gameplay_asset:gameplay_filename;
        std::string living_name=include_manifest&&!manifest.living_asset.empty()?manifest.living_asset:"world.living";
        const std::string root=project_directory,scene_name=std::filesystem::path(path).filename().string(),manifest_name=components_file;
        auto looks=npc_look_bindings;for(auto &look:looks){auto p=std::filesystem::path(look.file);if(p.is_relative())look.file=(std::filesystem::path(root)/p).string();}
        std::string minds;if(components_runtime&&components_runtime->configured()&&!components_runtime->encode_minds(minds,message))return;
        const auto entities=components_entities;
        project_save_job=std::async(std::launch::async,[scene,physics,gameplay,definition,manifest,include_manifest,include_living,physics_name,gameplay_name,living_name,root,scene_name,manifest_name,looks,minds,entities]()mutable{
            namespace fs=std::filesystem;std::string error;std::string temporary=vw_temp_pattern("valwood-project-save-XXXXXX");if(!mkdtemp(temporary.data()))return std::make_pair(false,std::string("Cannot create project staging directory"));
            struct Cleanup{fs::path p;~Cleanup(){std::error_code ec;fs::remove_all(p,ec);}} cleanup{temporary};const fs::path stage=temporary;std::unordered_map<std::string,world_project::File> guards;
            auto destination=[&](const std::string &n){world_project::Files guard;if(!world_project::read(root,{n},guard,error))return fs::path{};guards.emplace(n,guard[0]);auto p=stage/n;std::error_code ec;fs::create_directories(p.parent_path(),ec);if(ec){error=ec.message();return fs::path{};}return p;};
            auto p=destination(scene_name);if(p.empty()||!world_studio::save(p.u8string().c_str(),scene,error))return std::make_pair(false,error);
            p=destination(physics_name);if(p.empty()||!world_physics::save(p.u8string().c_str(),physics,error))return std::make_pair(false,error);
            p=destination(gameplay_name);if(p.empty()||!world_gameplay::save(p.u8string().c_str(),gameplay,error))return std::make_pair(false,error);
            if(include_living){p=destination(living_name);world_living::World initial;if(p.empty()||!initial.configure(definition,0,error)||!initial.save(p.string(),error))return std::make_pair(false,error);}
            if(include_manifest){p=destination(manifest_name);world_runtime::Configuration check;if(p.empty()||!world_runtime::save(p.u8string().c_str(),manifest,error)||!world_runtime::load_project(stage.string(),manifest,entities,0,check,error))return std::make_pair(false,"Referenced subsystem validation: "+error);}
            if(!looks.empty()||fs::exists(fs::path(root)/"ai/npc_appearances.vwlooks")){if(destination("ai/npc_appearances.vwlooks").empty()||!world_npc_appearance::save_project_bindings(stage.string(),looks,error))return std::make_pair(false,error);}
            if(!minds.empty()){p=destination("ai/npc_minds.vwminds");if(p.empty())return std::make_pair(false,error);std::ofstream out(p,std::ios::binary);out<<minds;out.close();if(!out)return std::make_pair(false,std::string("Cannot stage NPC mind bindings"));}
            // Include already-authored memory/policy assets; never personal saves.
            if(fs::is_directory(fs::path(root)/"ai"))for(const auto &entry:fs::recursive_directory_iterator(fs::path(root)/"ai")){
                if(entry.is_symlink())return std::make_pair(false,std::string("Symlink in project AI assets"));
                if(!entry.is_regular_file())continue;
                const auto ext=entry.path().extension();if(ext!=".vwmemory"&&ext!=".vwml")continue;const auto name=entry.path().lexically_relative(root).generic_string();world_project::Files input;if(!world_project::read(root,{name},input,error))return std::make_pair(false,error);guards.emplace(name,input[0]);p=destination(name);if(p.empty())return std::make_pair(false,error);std::ofstream out(p,std::ios::binary);out<<input[0].bytes;out.close();if(!out)return std::make_pair(false,std::string("Cannot stage AI asset"));
            }
            std::vector<std::string> names;for(const auto &entry:fs::recursive_directory_iterator(stage))if(entry.is_regular_file())names.push_back(entry.path().lexically_relative(stage).generic_string());std::sort(names.begin(),names.end());world_project::Files after,before;if(!world_project::read(stage.string(),names,after,error))return std::make_pair(false,error);for(const auto &name:names){auto found=guards.find(name);if(found!=guards.end())before.push_back(found->second);else{world_project::Files guard;if(!world_project::read(root,{name},guard,error))return std::make_pair(false,error);before.push_back(guard[0]);}}if(!world_project::commit(root,project_checkpoint_name,after,before,error))return std::make_pair(false,error);
            return std::make_pair(true,"Project saved together: "+std::to_string(after.size())+" authoring assets, with a previous checkpoint and crash-recovery journal. Player progress is separate.");
        });message="Saving project checkpoint in the background...";
    }
    void project_recover(bool finish){
        if(project_save_job.valid()||production_playing()||production_restore||living_save_job.valid())return;
        const auto root=project_directory;project_restoring=true;project_save_job=std::async(std::launch::async,[root,finish](){std::string error;bool ok=world_project::recover(root,project_checkpoint_name,finish,error);return std::make_pair(ok,ok?std::string("Project recovery completed. Restart the engine to load every subsystem together."):error);});
    }
    void project_restore_previous(){
        if(!production_can_save()||project_save_job.valid()||living_save_job.valid()||project_journal_pending)return;
        const auto root=project_directory;project_restoring=true;project_save_job=std::async(std::launch::async,[root](){std::string error;world_project::Files previous,current;if(!world_project::load(root,std::string(project_checkpoint_name)+".previous",previous,error))return std::make_pair(false,error);world_project::Files latest;if(world_project::load(root,project_checkpoint_name,latest,error)){for(const auto &f:latest)if(std::none_of(previous.begin(),previous.end(),[&](const auto &old){return old.path==f.path;}))previous.push_back({f.path,"",false});}error.clear();std::vector<std::string> names;for(const auto &f:previous)names.push_back(f.path);if(!world_project::read(root,names,current,error)||!world_project::commit(root,project_checkpoint_name,previous,current,error))return std::make_pair(false,error);return std::make_pair(true,std::string("Previous authoring checkpoint restored. Restart the engine to load every subsystem together."));});
    }
    void project_panel(){
        ImGui::SeparatorText("Whole-project saving");ImGui::TextWrapped("Save the applied scene, current physics/gameplay/living definitions, component manifest and NPC memory/appearance assets together. Player saves and source models are excluded. Individual tools retain their own Undo histories.");
        ImGui::BeginDisabled(!production_can_save()||project_save_job.valid()||project_journal_pending);if(ImGui::Button("Save project checkpoint"))project_save_all();
        if(ImGui::Button("Restore previous project checkpoint..."))ImGui::OpenPopup("Restore whole project?");
        ImGui::EndDisabled();
        if(ImGui::BeginPopupModal("Restore whole project?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){ImGui::TextWrapped("This restores the previous saved authoring files together. Unsaved edits are not included. Restart is required afterward; the current checkpoint is retained as the new previous generation.");if(ImGui::Button("Restore previous checkpoint")){project_restore_previous();ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
        if(project_journal_pending&&!project_save_job.valid()){ImGui::TextWrapped("An interrupted multi-file save is pending. Choose to finish it or roll all included files back. Conflicting external edits are preserved and reported.");if(ImGui::Button("Finish interrupted project save"))project_recover(true);if(ImGui::Button("Roll back interrupted project save"))project_recover(false);}
        if(project_save_job.valid())ImGui::TextUnformatted("Project I/O running in background; authoring is held until completion.");
        if(project_restart_required)ImGui::TextWrapped("Restart required after project restore. Close and relaunch the engine before editing or playing.");
        if(!project_status.empty())ImGui::TextWrapped("%s",project_status.c_str());
        ImGui::SeparatorText("Rebuild diagnostics");ImGui::Text("Last Apply: %.2f ms | terrain %s",last_apply_ms,last_apply_terrain?"rebuilt":"retained");ImGui::Text("Terrain rebuilds %u | skipped %u",terrain_rebuild_count,terrain_rebuild_skipped);
        ImGui::SeparatorText("Asset streaming budget");ImGui::Text("Geometry %.1f / 192 MiB",project_streaming.geometry_bytes/1048576.);ImGui::Text("Textures %.1f / 1536 MiB",project_streaming.texture_bytes/1048576.);ImGui::Text("Resident %zu | waiting %zu | failed %zu",project_streaming.resident,project_streaming.critical_pending,project_streaming.failed);ImGui::TextWrapped("Tracked shared-resource budgets are not total VRAM measurements. Imports and shader compilation may still pause at explicit Apply boundaries.");
        if(project_streaming.failed||project_streaming.texture_bytes>1382ull*1024*1024||project_streaming.geometry_bytes>172ull*1024*1024)ImGui::TextColored({1,.65f,.25f,1},"Asset budget pressure: inspect LODs and texture sizes before adding more.");
        if(project_streaming.error[0])ImGui::TextWrapped("Streaming: %s",project_streaming.error);
    }

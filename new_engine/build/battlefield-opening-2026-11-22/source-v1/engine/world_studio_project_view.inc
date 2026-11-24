// Coordinated authoring checkpoint. Workers own all copies; runtime saves are
// excluded. Restore is a disk transaction and deliberately requires restart so
// no live subsystem can retain half of an older project generation.
    std::future<std::pair<bool,std::string>> project_save_job;
    bool project_journal_pending=false,project_restart_required=false,project_restoring=false,project_aux_dirty=false,project_quit_after_save=false;
    bool project_save_requested=false,project_save_failed=false,project_saved_together=false;
    unsigned project_requested_rejections=0;
    std::string project_status;WorldSceneryStreamingStats project_streaming;
    static constexpr const char *project_checkpoint_name="world.vwproject";
    void project_init(){project_journal_pending=world_project::pending(project_directory,project_checkpoint_name);}
    void project_poll(){
        if(project_has_unsaved())project_saved_together=false;
        if(!project_save_job.valid()||project_save_job.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return;
        std::pair<bool,std::string> result;try{result=project_save_job.get();}catch(const std::exception &e){result={false,std::string("Project I/O failed: ")+e.what()};}project_status=result.second;project_journal_pending=world_project::pending(project_directory,project_checkpoint_name);
        project_save_failed=!result.first;
        if(result.first){if(project_restoring)project_restart_required=true;else{physics_modified=gameplay_dirty=components_modified=living_template_dirty=project_aux_dirty=region_modified=false;production_project_saved=project_saved_together=true;std::string cleanup_error;if(editor_session&&!editor_session->clean_saved(live,cleanup_error)){project_status+=" Recovery cleanup: "+cleanup_error;project_save_failed=true;}if(project_quit_after_save&&!project_save_failed&&!cinematic_unsaved())designer_exit_approved=true;}}
        project_restoring=false;project_quit_after_save=false;message=project_status;
    }
    bool project_has_unsaved()const{return dirty||apply||!production_project_saved||physics_modified||gameplay_dirty||components_modified||living_template_dirty||project_aux_dirty||region_modified||cinematic_unsaved();}
    const char *project_save_label()const{
        if(project_journal_pending)return "SAVE RECOVERY REQUIRED";
        if(project_restart_required)return "RESTART REQUIRED";
        if(project_save_job.valid())return "SAVING ALL";
        if(project_save_requested||designer_save_pending)return "APPLYING FOR SAVE";
        if(project_save_failed)return "SAVE NEEDS ATTENTION";
        if(cinematic_unsaved())return "CINEMATICS NOT PUBLISHED";
        if(dirty)return "DRAFT";
        if(project_has_unsaved())return "UNSAVED CHANGES";
        return project_saved_together?"SAVED ALL":"SCENE SAVED";
    }
    void project_request_save_all(){
        if(cinematic_unsaved()){project_quit_after_save=designer_save_exit=false;requested_tab=17;message="Publish cinematic changes in Cinematics before Save All or Save and Exit.";return;}
        if(!production_can_save()||production_playing()||living_save_job.valid()||production_job.valid()||importing.valid()||environment_import.valid()||surface_import.valid()){
            message="Finish Play, imports or pending recovery/jobs before Save All.";return;
        }
        if(drag.active||landscape_painting){message="Release or cancel the current editing gesture before Save All.";return;}
        designer_finish_edit();project_save_failed=false;project_status.clear();
        // Legacy bounded input fixtures edit only an explicit /tmp scene file;
        // never publish that fixture over its immutable source project's files.
        const auto parent=std::filesystem::absolute(path).parent_path().lexically_normal();
        const auto root=std::filesystem::absolute(project_directory).lexically_normal();
        if(parent!=root){
            const char *qa=std::getenv("VALWOOD_QA_STUDIO_DOCUMENT");
            if(qa&&path==qa&&path.rfind("/tmp/",0)==0){if(dirty||apply){apply=true;designer_save_pending=true;designer_save_rejected=rejected;}else save();return;}
            message="Save All needs the scene document inside this project's work folder. The external scene and project files were preserved.";project_save_failed=true;return;
        }
        if(dirty||apply){project_save_requested=true;project_requested_rejections=rejected;apply=true;message="Save All: applying the scene, then saving every supported authoring document together.";}
        else project_save_all();
    }
    void project_service_save_all(){
        if(!project_save_requested)return;
        if(rejected>project_requested_rejections){project_save_requested=project_quit_after_save=designer_save_exit=false;project_save_failed=true;project_status="Save All stopped because the scene Apply was rejected. Correct the draft and retry; saved files were preserved.";message=project_status;return;}
        if(dirty||apply)return;
        project_save_requested=false;project_save_all();if(!project_save_job.valid())project_quit_after_save=designer_save_exit=false;
    }
    void project_save_all(){
        if(cinematic_unsaved()){project_quit_after_save=designer_save_exit=false;requested_tab=17;message="Cinematic edits are not published. Publish them in Cinematics first.";return;}
        if(!production_can_save()||project_save_job.valid()||living_save_job.valid()||production_job.valid()||project_journal_pending){message="Finish Play, pending jobs or interrupted-save recovery before saving the project.";return;}
        if(dirty||apply){message="Apply the scene draft first; Save project captures the applied scene and current subsystem drafts.";return;}
        if(!structure_change_allowed(live,message)){project_save_failed=true;return;}
        if(!world_regions::validate(region_document,message)){project_save_failed=true;return;}
        for(const auto &region:region_document.regions)for(const auto &layer:region.layers)for(auto id:layer.entities)if(!world_entity::find(live.entities,id)){message="Save All: a region layer refers to a missing scene entity. Repair its membership before saving.";project_save_failed=true;return;}
        auto scene=live;auto physics=physics_document;auto gameplay=gameplay_document;auto definition=living_design;auto manifest=components_document;
        const bool include_manifest=components_manifest_loaded||components_modified,include_living=living_ready;
        std::string physics_name=include_manifest&&!manifest.physics_asset.empty()?manifest.physics_asset:physics_file;
        std::string gameplay_name=include_manifest&&!manifest.gameplay_asset.empty()?manifest.gameplay_asset:gameplay_filename;
        std::string living_name=include_manifest&&!manifest.living_asset.empty()?manifest.living_asset:"world.living";
        const std::string root=project_directory,scene_name=std::filesystem::path(path).filename().string(),manifest_name=components_file;
        const auto region_relative=std::filesystem::path(region_path()).lexically_normal().lexically_relative(std::filesystem::path(root).lexically_normal());
        if(region_relative.empty()||region_relative.is_absolute()||std::any_of(region_relative.begin(),region_relative.end(),[](const auto &part){return part=="..";})){message="Save All requires the Regions document inside the project work folder.";project_save_failed=true;return;}
        const auto regions=region_document;const auto region_name=region_relative.generic_string();const bool include_regions=region_modified||!regions.regions.empty()||!regions.assets.empty()||std::filesystem::exists(region_path());
        auto looks=npc_look_bindings;for(auto &look:looks){auto p=std::filesystem::path(look.file);if(p.is_relative())look.file=(std::filesystem::path(root)/p).string();}
        std::string minds;if(components_runtime&&components_runtime->configured()&&!components_runtime->encode_minds(minds,message))return;
        const auto entities=components_entities;
        project_save_job=std::async(std::launch::async,[scene,physics,gameplay,definition,manifest,include_manifest,include_living,physics_name,gameplay_name,living_name,root,scene_name,manifest_name,looks,minds,entities,regions,region_name,include_regions]()mutable{
            namespace fs=std::filesystem;std::string error;std::string temporary=vw_temp_pattern("valwood-project-save-XXXXXX");if(!mkdtemp(temporary.data()))return std::make_pair(false,std::string("Cannot create project staging directory"));
            struct Cleanup{fs::path p;~Cleanup(){std::error_code ec;fs::remove_all(p,ec);}} cleanup{temporary};const fs::path stage=temporary;std::unordered_map<std::string,world_project::File> guards;
            auto destination=[&](const std::string &n){world_project::Files guard;if(!world_project::read(root,{n},guard,error))return fs::path{};guards.emplace(n,guard[0]);auto p=stage/n;std::error_code ec;fs::create_directories(p.parent_path(),ec);if(ec){error=ec.message();return fs::path{};}return p;};
            auto p=destination(scene_name);if(p.empty()||!world_studio::save(p.u8string().c_str(),scene,error))return std::make_pair(false,error);
            p=destination(physics_name);if(p.empty()||!world_physics::save(p.u8string().c_str(),physics,error))return std::make_pair(false,error);
            p=destination(gameplay_name);if(p.empty()||!world_gameplay::save(p.u8string().c_str(),gameplay,error))return std::make_pair(false,error);
            if(include_living){p=destination(living_name);world_living::World initial;if(p.empty()||!initial.configure(definition,0,error)||!initial.save(p.string(),error))return std::make_pair(false,error);}
            if(include_manifest){p=destination(manifest_name);world_runtime::Configuration check;if(p.empty()||!world_runtime::save(p.u8string().c_str(),manifest,error)||!world_runtime::load_project(stage.string(),manifest,entities,0,check,error))return std::make_pair(false,"Referenced subsystem validation: "+error);}
            if(include_regions){p=destination(region_name);if(p.empty()||!world_regions::save(p.u8string().c_str(),regions,error))return std::make_pair(false,error);}
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
        });project_save_failed=false;project_saved_together=false;message="Save All: writing the coordinated project checkpoint...";
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
        structure_status_panel();
        ImGui::SeparatorText("Save All project changes");ImGui::TextWrapped("Ctrl+Shift+S applies the scene and saves physics, gameplay, living definitions, regions/layers, component and NPC assets together. Player progress and original source models are separate.");
        ImGui::Text("Status: %s",project_save_label());
        if(cinematic_unsaved())ImGui::TextWrapped("Cinematics: unpublished. Open Cinematics and Publish to save camera, trigger and narration edits.");
        if(project_has_unsaved())ImGui::TextWrapped("Unsaved: scene %s | physics %s | gameplay %s | regions %s | other project data %s",dirty||apply||!production_project_saved?"yes":"no",physics_modified?"yes":"no",gameplay_dirty?"yes":"no",region_modified?"yes":"no",components_modified||living_template_dirty||project_aux_dirty?"yes":"no");
        ImGui::BeginDisabled(!production_can_save()||project_save_job.valid()||project_journal_pending);if(ImGui::Button("Save All   Ctrl+Shift+S"))project_request_save_all();
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

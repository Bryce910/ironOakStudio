// Included inside Studio. Production tools own no GL resources and stage the
// same draft as existing tools. Live changes still use the Apply fence.
    std::unique_ptr<world_editor::Session> editor_session;
    bool production_restore=false,production_restore_failed=false,production_project_saved=false;
    world_studio::Document production_after_stop;
    world_studio::History production_play_history;
    std::string engine_directory;
    std::vector<world_authoring::Source> identity_sources;
    std::future<world_production::Result> production_job;
    std::string production_diagnostics;
    char package_output[1024]={},frame_profile[1024]={},frame_report[1024]={};
    char prefab_name[65]="Connected bundle",prefab_file[1024]="prefabs/new_bundle.vwprefab";
    std::vector<world_entity::Binding> prefab_attachments;
    world_entity::Id entity_selected=0,entity_parent=0;
    float attachment_radius=20;bool prefab_loaded=false;
    world_prefab::Prefab prefab_clipboard;
    std::vector<std::pair<world_entity::Id,world_entity::Transform>> entity_worlds;
    std::uint64_t entity_world_revision=~std::uint64_t(0);
    world_entity::Id production_id_highwater=1;
    static double production_now(){return SDL_GetTicks64()*.001;}
    static double production_ground(double px,double pz,const void *context){return world_atlas_base_height(px,pz)+world_studio::height_delta(*static_cast<const world_studio::Document *>(context),px,pz);}
    void production_init(){
        production_project_saved=loaded;
        char *base=SDL_GetBasePath();if(base){engine_directory=base;SDL_free(base);}
        std::error_code ec;engine_directory=std::filesystem::weakly_canonical(engine_directory,ec).string();
        editor_session=std::make_unique<world_editor::Session>();std::string error;
        if(!editor_session->open(path,loaded?draft:world_studio::Document{},production_now(),error))message="Recovery unavailable: "+error;
        else{editor_session->probe(error);if(!error.empty())message=error;}
        auto build=std::filesystem::path(engine_directory)/"builds"/("demo-"+std::to_string(std::time(nullptr)));
        std::snprintf(package_output,sizeof package_output,"%s",build.c_str());
        auto report=std::filesystem::path(engine_directory)/"builds"/("frames-"+std::to_string(std::time(nullptr))+".json");
        std::snprintf(frame_report,sizeof frame_report,"%s",report.c_str());
    }
    bool production_sync(std::string &error){
        auto next=draft;auto graph=next.entities;graph.next_id=std::max(graph.next_id,production_id_highwater);
        if(!world_authoring::reconcile(next,graph,identity_sources,signature,production_ground,&next,error))return false;
        bool registered_source=false;
        for(unsigned key:selection_keys)if(key<WORLD_ADDED_KEY_BASE&&key<identity_sources.size()){
            const world_entity::Binding binding{world_entity::Kind::SourceModel,key};bool registered=false;for(const auto &n:graph.nodes)if(n.binding==binding){registered=true;break;}if(registered)continue;
            world_studio::Pose p;if(!world_studio::effective_pose(next,source_poses,key,p)||p.hidden){error="Selected source is hidden or unavailable";return false;}
            world_entity::Id created=0;if(!world_entity::add(graph,0,p.alias,binding,{p.root.x,p.root.y,p.root.z,p.yaw*57.29577951308232,p.scale,p.pitch*57.29577951308232,p.roll*57.29577951308232,p.sx,p.sy,p.sz},created,error))return false;
            world_entity::find(graph,created)->source_key=world_authoring::source_key(identity_sources[key]);registered_source=true;
        }
        // Selection can materialize a source identity after Undo removed its
        // first registration. Canonicalize that identity now, exactly as the
        // next synchronization will, instead of publishing the display pose
        // adjusted against the previously moved scene. This changes no IDs or
        // history and keeps existing parents/other child world poses intact.
        if(registered_source&&!world_authoring::synchronize(next,graph,identity_sources,production_ground,&next,error))return false;
        production_id_highwater=graph.next_id;next.entities=std::move(graph);draft=std::move(next);revision++;entity_world_revision=~std::uint64_t(0);return true;
    }
    bool production_can_save(){return !structure_cooking&&!project_journal_pending&&!project_save_job.valid()&&!project_restart_required&&!production_restore&&(!editor_session||editor_session->save_allowed());}
    bool production_playing()const{return editor_session&&editor_session->stats().mode==world_editor::Mode::Playing;}
    bool production_freeze()const{return project_journal_pending||project_save_job.valid()||project_restart_required||production_restore||(editor_session&&editor_session->stats().mode==world_editor::Mode::StopPending);}
    bool production_begin(){
        if(!editor_session||production_restore||production_playing()||production_freeze()){message="Finish the current Play/Stop transaction first.";return false;}
        if(cinematic_preview){message="Stop the camera timeline before starting scene Play.";return false;}
        if(living_save_job.valid()){message="Wait for the living-world save before Play.";return false;}
        std::string error;if(!production_sync(error)){message=error;return false;}if(!components_play_begin())return false;if(!editor_session->play_begin(draft,live,error)){components_play_snapshot_ready=false;message=error;return false;}
        production_play_history=history;living_play_begin();physics_running=false;physics_clock=0;open=false;
        message="Play active. F11 opens Studio; Stop returns to the authoring snapshot.";return true;
    }
    void production_service(bool allow_work){
        if(production_job.valid()&&production_job.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){auto result=production_job.get();production_diagnostics=std::move(result.output);message=result.ok?"Production check completed. Review the diagnostics.":result.error;}
        if(!editor_session)return;
        std::string error;if(!editor_session->tick(production_now(),draft,allow_work&&!production_restore&&!production_job.valid(),error)&&!error.empty())message=error;
    }
    void production_edited(){if(editor_session)editor_session->changed(production_now());production_id_highwater=std::max(production_id_highwater,draft.entities.next_id);production_project_saved=project_saved_together=false;entity_world_revision=~std::uint64_t(0);}
    bool production_remove(world_entity::Binding binding,bool geometry=false){
        auto next=draft;if(!world_authoring::erase_record(next,binding,message))return false;
        checkpoint();draft=std::move(next);
        for(auto at=prefab_attachments.begin();at!=prefab_attachments.end();){if(*at==binding)at=prefab_attachments.erase(at);else{if(at->kind==binding.kind&&at->index>binding.index)--at->index;++at;}}
        if(entity_selected&&!world_entity::find(draft.entities,entity_selected))entity_selected=0;
        changed(geometry);return true;
    }
    void production_applied(){
        std::string error;
        if(production_restore){draft=std::move(production_after_stop);production_restore=production_restore_failed=false;dirty=true;rebuild=true;clear_selection();if(designer_restore_selection_after_play){designer_selection=designer_play_selection;designer_reconcile_selection();designer_restore_selection_after_play=false;open=true;}}
        if(editor_session&&!editor_session->applied(live,error))message="Applied, but recovery checkpoint failed: "+error;
    }
    void production_stop(world_editor::StopChoice choice){
        auto staged=draft,committed=live;std::string error;
        if(!components_can_stop(choice))return;
        if(!editor_session||!editor_session->play_stop(choice,staged,committed,production_now(),error)){message=error;return;}
        if(choice==world_editor::StopChoice::DiscardPlayChanges)history=std::move(production_play_history);
        production_play_history={};
        const bool components_ok=components_play_stop(choice);
        if(components_ok)living_play_stop(choice);
        staged.entities.next_id=std::max(staged.entities.next_id,production_id_highwater);committed.entities.next_id=std::max(committed.entities.next_id,production_id_highwater);
        production_after_stop=std::move(staged);draft=std::move(committed);production_restore=true;production_restore_failed=!components_ok;
        // Runtime readers retain live until the normal transaction succeeds.
        rebuild=dirty=true;apply=components_ok;clear_selection();if(components_ok)message="Restoring the authoring scene through Apply...";
    }
    bool production_retry_components(){if(!components_stop_failed)return true;const auto choice=components_pending_stop_choice;if(!components_play_stop(choice))return false;living_play_stop(choice);return true;}
    void production_session_panel(){
        project_panel();
        if(!editor_session)return;
        const auto stats=editor_session->stats();
        if(stats.mode==world_editor::Mode::Editing&&!production_restore){
            if(ImGui::Button("Play scene"))production_begin();
        }else if(stats.mode==world_editor::Mode::Playing){if(ImGui::Button("Stop play"))editor_session->request_stop();}
        else if(stats.mode==world_editor::Mode::StopPending){
            ImGui::TextWrapped("Stop: restore the pre-Play document, or keep authored edits made during Play. Transient actor poses are reset by Apply.");
            if(ImGui::Button("Discard play changes"))production_stop(world_editor::StopChoice::DiscardPlayChanges);
            if(ImGui::Button("Keep authoring changes"))production_stop(world_editor::StopChoice::KeepAuthoringChanges);
        }
        if(production_restore){ImGui::TextWrapped("Authoring scene restore pending. Saving and another Play are held until Apply succeeds.");if(production_restore_failed&&ImGui::Button("Retry scene restore")){apply=true;production_restore_failed=false;}}
        if(stats.recovery!=world_editor::Recovery::None){
            ImGui::TextWrapped("A recovery file is available for inspection. It never replaces the project automatically.");
            if(ImGui::Button("Inspect recovery")){std::string error;editor_session->probe(error);message=error.empty()?"Recovery matches this saved base.":error;}
            if(stats.recovery==world_editor::Recovery::Available&&ImGui::Button("Recover draft"))designer_recover_draft();
            if(ImGui::Button("Discard recovery files")){std::string error;if(editor_session->wait(error)&&editor_session->discard(error))message="Recovery files discarded. Saved project preserved.";else message=error;}
        }
        ImGui::Text("Recovery writes %llu | failed %llu%s",(unsigned long long)stats.writes,(unsigned long long)stats.failures,stats.worker_running?" | saving draft...":"");
    }
    world_prefab::Selection production_selection(){
        world_prefab::Selection chosen;chosen.ground={production_ground,&draft};chosen.objects=prefab_attachments;
        for(unsigned key:selection_keys){
            const world_entity::Binding binding={key<WORLD_ADDED_KEY_BASE?world_entity::Kind::SourceModel:world_entity::Kind::Placement,key<WORLD_ADDED_KEY_BASE?key:key-WORLD_ADDED_KEY_BASE};
            if(std::find(chosen.objects.begin(),chosen.objects.end(),binding)==chosen.objects.end())chosen.objects.push_back(binding);
            if(key<WORLD_ADDED_KEY_BASE&&key<source.size()){
                world_studio::Pose p;if(!pose(key,p))continue;world_prefab::SourceModel s;s.index=key;s.root_y=p.root.y;
                s.placement={p.alias,p.root.x,p.root.z,float(p.yaw*57.29577951308232),float(p.scale),p.burial,p.elevation,p.offset_y,float(p.pitch*57.29577951308232),float(p.roll*57.29577951308232),float(p.sx),float(p.sy),float(p.sz),p.wind_enabled,p.wind_sway,p.wind_flutter};chosen.source_models.push_back(std::move(s));
            }
        }return chosen;
    }
    void production_prefab_panel(){
        ImGui::TextWrapped("A connected bundle includes selected models and explicitly checked attachments. Routes, lights, emitters and local ponds move with its root.");
        ImGui::InputText("Bundle name",prefab_name,sizeof prefab_name);ImGui::InputText("Prefab file",prefab_file,sizeof prefab_file);
        ImGui::SliderFloat("Attachment search radius (m)",&attachment_radius,2,64);
        if(target){
            auto check=[&](world_entity::Binding binding,const std::string &label,double px,double pz){if(std::hypot(px-x,pz-z)>attachment_radius)return;bool selected=std::find(prefab_attachments.begin(),prefab_attachments.end(),binding)!=prefab_attachments.end();ImGui::PushID(int(unsigned(binding.kind)*1000+binding.index));if(ImGui::Checkbox(label.c_str(),&selected)){auto at=std::find(prefab_attachments.begin(),prefab_attachments.end(),binding);if(selected)prefab_attachments.push_back(binding);else if(at!=prefab_attachments.end())prefab_attachments.erase(at);}ImGui::PopID();};
            if(ImGui::CollapsingHeader("Choose nearby attachments")){
                for(unsigned i=0;i<draft.lights.size();i++)check({world_entity::Kind::Light,i},"Light "+std::to_string(i+1),draft.lights[i].x,draft.lights[i].z);
                for(unsigned i=0;i<draft.actors.size();i++)if(!draft.actors[i].points.empty())check({world_entity::Kind::Actor,i},draft.actors[i].name,draft.actors[i].points[0][0],draft.actors[i].points[0][1]);
                for(unsigned i=0;i<draft.particles.size();i++)check({world_entity::Kind::Particle,i},"Emitter "+std::to_string(i+1),draft.particles[i].x,draft.particles[i].z);
                for(unsigned i=0;i<draft.waters.size();i++)if(draft.waters[i].source==3)check({world_entity::Kind::Water,i},"Local pond "+std::to_string(i+1),draft.waters[i].x,draft.waters[i].z);
            }
        }
        if(ImGui::Button("Clear attachment selection"))prefab_attachments.clear();
        if(ImGui::Button("Capture selected bundle")){
            if(!target)message="Pick a bundle origin in the world first.";
            else if(world_prefab::capture(draft,production_selection(),{x,y,z,0,1},prefab_name,prefab_clipboard,message)){prefab_loaded=true;message="Connected bundle captured. Save it or place another instance.";}
        }
        if(ImGui::Button("Save captured prefab")){
            if(!prefab_loaded)message="Capture or load a prefab first.";
            else{auto file=std::filesystem::path(prefab_file);if(file.is_relative())file=std::filesystem::path(project_directory)/file;std::error_code ec;std::filesystem::create_directories(file.parent_path(),ec);if(ec)message=ec.message();else if(world_prefab::save(file.u8string().c_str(),prefab_clipboard,message))message="Connected prefab saved.";}
        }
        ImGui::SameLine();if(ImGui::Button("Load prefab")){auto file=std::filesystem::path(prefab_file);if(file.is_relative())file=std::filesystem::path(project_directory)/file;if(world_prefab::load(file.u8string().c_str(),prefab_clipboard,message)){prefab_loaded=true;message="Connected prefab loaded. Pick a destination.";}}
        if(prefab_loaded){
            ImGui::Text("%s | %zu objects",prefab_clipboard.name.c_str(),prefab_clipboard.graph.nodes.size()-1);
            if(ImGui::Button("Instantiate at target")&&target&&production_sync(message)){auto next=draft;world_prefab::Instance result;world_prefab::Ground ground{production_ground,&next};
                if(world_prefab::instantiate(prefab_clipboard,next,next.entities,{x,y,z,yaw,scale},0,result,message,ground)){checkpoint();draft=std::move(next);entity_selected=result.root;changed(true);prefab_attachments.clear();clear_selection();message="Connected instance staged. Apply updates its scenery and attachments.";}
            }
        }
        ImGui::Separator();ImGui::TextUnformatted("Persistent hierarchy");
        if(ImGui::Button("Register / synchronize scene identities")){if(production_sync(message)){changed();message="Scene records have persistent identities. Save after Apply.";}}
        if(entity_world_revision!=revision){std::string error;if(world_entity::world_transforms(draft.entities,entity_worlds,error))entity_world_revision=revision;else message=error;}
        ImGui::BeginChild("Durable entities",{0,160},true);
        for(const auto &node:draft.entities.nodes){ImGui::PushID(std::to_string(node.id).c_str());std::string label=std::to_string(node.id)+" | "+node.name+(node.parent?" (child)":"");if(ImGui::Selectable(label.c_str(),entity_selected==node.id))entity_selected=node.id;ImGui::PopID();}ImGui::EndChild();
        const auto *node=world_entity::find(draft.entities,entity_selected);if(!node)return;
        const bool bundle_root=node->binding.kind==world_entity::Kind::None;
        bool entity_visible=node->visible,entity_locked=node->locked;
        ImGui::Text("ID %llu | parent %llu",(unsigned long long)node->id,(unsigned long long)node->parent);
        bool flags=ImGui::Checkbox("Entity visible",&entity_visible);ImGui::SameLine();flags|=ImGui::Checkbox("Entity locked",&entity_locked);
        if(flags){checkpoint();auto *change=world_entity::find(draft.entities,entity_selected);change->visible=entity_visible;change->locked=entity_locked;changed(true);}
        if(ImGui::BeginCombo("New parent",entity_parent?std::to_string(entity_parent).c_str():"Scene root")){if(ImGui::Selectable("Scene root",!entity_parent))entity_parent=0;for(const auto &n:draft.entities.nodes)if(n.id!=entity_selected&&ImGui::Selectable((std::to_string(n.id)+" | "+n.name).c_str(),entity_parent==n.id))entity_parent=n.id;ImGui::EndCombo();}
        bool inherited_lock=false;for(auto id=entity_selected;id;){const auto *ancestor=world_entity::find(draft.entities,id);if(!ancestor)break;inherited_lock|=ancestor->locked;id=ancestor->parent;}
        ImGui::BeginDisabled(inherited_lock);
        if(ImGui::Button("Reparent, preserving world position")){auto next=draft;if(world_entity::reparent(next.entities,entity_selected,entity_parent,true,message)){checkpoint();draft=std::move(next);changed();}}
        if(bundle_root){
            world_entity::Transform t;bool found=false;for(const auto &p:entity_worlds)if(p.first==entity_selected){t=p.second;found=true;break;}
            if(found){bool edit=false;edit|=ImGui::InputDouble("Bundle east",&t.x,.25,2);edit|=ImGui::InputDouble("Bundle height",&t.y,.25,2);edit|=ImGui::InputDouble("Bundle south",&t.z,.25,2);edit|=ImGui::InputDouble("Bundle yaw",&t.yaw,1,15);edit|=ImGui::InputDouble("Bundle scale",&t.scale,.05,.25);
                if(edit){auto next=draft;world_prefab::Ground ground{production_ground,&next};if(world_prefab::transform_instance(next,next.entities,entity_selected,t,message,ground)){checkpoint();draft=std::move(next);changed(true);}}
            }
        }
        ImGui::EndDisabled();
        ImGui::TextWrapped("Transforms currently support XYZ, yaw and uniform scale. Noncircular ponds accept quarter turns. Whole bundles use their unbound root; individual objects retain the existing viewport gizmos.");
    }
    void production_build_panel(){
        ImGui::TextWrapped("Validate dependencies, build a native Linux demo, or audit an existing native frame capture. These explicit jobs run in a worker; do not run an FPS measurement concurrently.");
        ImGui::InputText("New package destination",package_output,sizeof package_output);
        auto launch=[&](world_production::Task task){if(production_job.valid()||production_playing()||production_restore){message="Finish the current job or Stop Play first.";return;}world_production::Request r;r.task=task;r.engine=engine_directory;r.project=project_directory;r.output=task==world_production::Task::AuditFrames?frame_report:package_output;r.profile=frame_profile;if(task!=world_production::Task::Validate){std::error_code ec;std::filesystem::create_directories(std::filesystem::path(r.output).parent_path(),ec);if(ec){message=ec.message();return;}}production_diagnostics.clear();production_job=std::async(std::launch::async,[r](){return world_production::run(r);});message="Production worker started.";};
        if(ImGui::Button("Validate project dependencies"))launch(world_production::Task::Validate);
        ImGui::SameLine();if(ImGui::Button("Build Linux demo")){if(dirty||!production_project_saved)message="Apply and Save the scene before building its package.";else launch(world_production::Task::Package);}
        ImGui::InputText("Native frame profile JSON",frame_profile,sizeof frame_profile);ImGui::InputText("New frame report",frame_report,sizeof frame_report);
        if(ImGui::Button("Check every frame against45 FPS"))launch(world_production::Task::AuditFrames);
        if(production_job.valid())ImGui::TextUnformatted("Production job running...");
        if(!production_diagnostics.empty()){ImGui::BeginChild("Production diagnostics",{0,240},true);ImGui::TextUnformatted(production_diagnostics.c_str());ImGui::EndChild();}
        ImGui::TextWrapped("The package includes the current runtime and saved project dependencies. It does not strip editor code or build Windows binaries. Existing output destinations are preserved; choose a new destination for each build.");
    }
    void production_panel(){
        if(ImGui::BeginTabItem("Production",nullptr,requested_tab==14?ImGuiTabItemFlags_SetSelected:0)){tab=14;production_session_panel();if(ImGui::CollapsingHeader("Connected prefabs / hierarchy",ImGuiTreeNodeFlags_DefaultOpen))production_prefab_panel();if(ImGui::CollapsingHeader("Validate / package / frame budget"))production_build_panel();ImGui::EndTabItem();}
    }

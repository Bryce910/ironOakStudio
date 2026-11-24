// Included inside Studio. Designer UI shares the existing draft/Apply fence.
    world_designer_viewport::Layout designer_layout;
    bool designer_immersive=false,designer_content=true,designer_ground_grid=false;
    bool designer_palette_open=false,designer_help=false,designer_preferences_dirty=false;
    bool designer_close_requested=false,designer_exit_approved=false,designer_save_pending=false,designer_save_exit=false;
    bool designer_frame_pending=false,designer_tools=false;
    world_studio::Vec designer_frame_center,designer_creation_origin;
    double designer_frame_radius=1,designer_preferences_time=0;
    char designer_search[128]{},designer_palette_search[128]{},designer_rename[128]{};
    world_designer::Ref designer_inspected;
    std::string designer_inspected_name;
    std::string designer_workspace_path;
    world_designer_scene::Index designer_scene_index;
    std::string designer_scene_source_signature,designer_scene_source_token;
    std::uint64_t designer_scene_source_generation=0,designer_inspector_source_generation=0;
    std::uint64_t designer_inspector_revision=~std::uint64_t(0),designer_inspector_refreshes=0;
    world_designer::Ref designer_inspector_ref;
    world_entity::Transform designer_inspector_transform;
    std::string designer_inspector_name,designer_inspector_error,designer_inspector_source_signature;
    ImVec2 designer_wind_enable_point{},designer_wind_sway_point{},designer_wind_flutter_point{};
    bool designer_inspector_pose_ok=false,designer_inspector_visible=false,designer_inspector_locked=false,designer_inspector_inherited_lock=false;
    std::vector<world_entity::Id> designer_collapsed;
    int designer_world_filter=0;
    std::vector<unsigned char> designer_source_categories;
    std::uint64_t designer_source_categories_generation=0;
    world_designer::Clipboard designer_clipboard;
    world_designer::PasteOptions designer_paste_options;
    bool designer_reveal_paste=true,designer_frame_reveal=false,designer_paste_pending=false,designer_paste_failed=false;
    float designer_frame_heading=0;
    unsigned designer_paste_started_apply=0,designer_paste_started_reject=0;
    double designer_paste_notice_until=0;
    std::string designer_paste_notice;
    std::string designer_paste_success="Pasted model is visible. Ctrl+V adds the next copy.";
    world_designer::Ref designer_paste_previous;
    world_designer::Ref designer_last_pasted;
    bool designer_recovery_dismissed=false;
    world_designer::ModelBounds designer_model_bounds(){
        world_designer::ModelBounds bounds;
        bounds.context=this;bounds.get=+[](const std::string &alias,world_studio::Vec &minimum,world_studio::Vec &maximum,const void *context){
            const auto *studio=static_cast<const Studio *>(context);float lo[3],hi[3];
            if(!world_scenery_alias_bounds(studio->scene_ptr,alias.c_str(),lo,hi))return false;
            minimum={lo[0],lo[1],lo[2]};maximum={hi[0],hi[1],hi[2]};return true;
        };
        bounds.grounded_pivot=+[](const std::string &alias,const world_entity::Transform &transform,double burial,double elevation,world_prefab::Ground ground,double &height,const void *context){
            const auto *studio=static_cast<const Studio *>(context);WorldSceneryPlacement p;p.alias=alias.c_str();p.x=transform.x;p.z=transform.z;
            p.yaw=float(transform.yaw*.017453292519943295);p.scale=float(transform.scale);p.pitch=float(transform.pitch*.017453292519943295);p.roll=float(transform.roll*.017453292519943295);p.sx=float(transform.sx);p.sy=float(transform.sy);p.sz=float(transform.sz);p.burial=float(burial);p.elevation=float(elevation);
            return world_scenery_grounded_pivot(studio->scene_ptr,p,ground.height,ground.context,height);
        };
        return bounds;
    }
    void designer_snap_terrain(){
        if(!production_can_save()||production_playing()||production_freeze()){message="Stop Play before snapping scenery to terrain.";return;}
        designer_finish_edit();world_studio::Document next;world_designer::Result result;
        if(!world_designer::snap_to_terrain(draft,designer_selection,designer_context(),designer_model_bounds(),next,result,message)){
            designer_paste_notice=message;designer_paste_failed=true;designer_paste_notice_until=production_now()+10;return;
        }
        if(!result.changed){message="Selection is already snapped to terrain.";designer_paste_notice=message;designer_paste_failed=false;designer_paste_notice_until=production_now()+5;return;}
        if(!structure_change_allowed(next,message)){designer_paste_notice=message;designer_paste_failed=true;designer_paste_notice_until=production_now()+10;return;}
        const bool audio_only=world_studio::environment_only_change(draft,next);history.checkpoint(draft);draft=std::move(next);designer_selection=std::move(result.selection);
        production_id_highwater=std::max(production_id_highwater,draft.entities.next_id);
        designer_move_fast_candidate=false;
        changed(!audio_only);designer_publish_selection();if(audio_only)sound_preview_apply();else designer_preview_object_edit();
        designer_last_pasted=designer_selection.active;designer_paste_previous={};
        designer_paste_started_apply=applied;designer_paste_started_reject=rejected;designer_paste_pending=apply;designer_paste_failed=false;
        designer_paste_success="Selection is snapped to terrain. End snaps again.";
        designer_paste_notice=apply?"Snapping selection to terrain...":"Terrain snap staged. Apply previews it.";designer_paste_notice_until=production_now()+8;
        if(designer_reveal_paste)designer_reveal_pasted();
        message=designer_paste_notice;
    }
    void designer_copy(){
        designer_finish_edit();
        const bool copied=designer_clipboard.copy(draft,designer_selection,designer_context(),designer_model_bounds(),message);
        if(copied)message="Copied selection. Ctrl+V pastes the next snapped copy.";
        designer_paste_notice=message;designer_paste_failed=!copied;designer_paste_notice_until=production_now()+5;
    }
    void designer_reveal_pasted(){
        auto reveal=designer_selection;world_designer::Ref resolved;world_entity::Binding binding;std::string ignored;
        if(!designer_last_pasted.empty()&&world_designer::resolve(draft,designer_context(),designer_last_pasted,resolved,binding,ignored)){reveal.items={resolved};reveal.active=resolved;}
        if(!designer_paste_previous.empty()&&world_designer::resolve(draft,designer_context(),designer_paste_previous,resolved,binding,ignored)&&std::find(reveal.items.begin(),reveal.items.end(),resolved)==reveal.items.end())reveal.items.push_back(resolved);
        designer_frame_selection(&reveal);designer_frame_reveal=true;
        world_entity::Transform pose;if(world_designer::world_pose(draft,designer_context(),reveal.active,pose,ignored))designer_frame_heading=float(pose.yaw*.017453292519943295+(designer_paste_options.direction>=2?1.5707963267948966:0));
    }
    void designer_paste(){
        if(!production_can_save()||production_playing()||production_freeze()){message="Stop Play before pasting scenery.";return;}
        designer_finish_edit();world_studio::Document next;world_designer::Result result;
        if(!designer_clipboard.paste(draft,designer_context(),designer_paste_options,next,result,message)){
            designer_paste_notice=message;designer_paste_failed=true;designer_paste_notice_until=production_now()+10;return;
        }
        if(!structure_change_allowed(next,message)){designer_paste_notice=message;designer_paste_failed=true;designer_paste_notice_until=production_now()+10;return;}
        designer_paste_previous=designer_selection.active;
        const bool audio_only=world_studio::environment_only_change(draft,next);history.checkpoint(draft);draft=std::move(next);designer_selection=std::move(result.selection);
        designer_last_pasted=designer_selection.active;
        production_id_highwater=std::max(production_id_highwater,draft.entities.next_id);
        changed(!audio_only);designer_publish_selection();if(audio_only)sound_preview_apply();else designer_preview_object_edit();
        designer_tools=false;tab=requested_tab=2;
        designer_paste_pending=apply;designer_paste_started_apply=applied;designer_paste_started_reject=rejected;
        designer_paste_success=audio_only?"Sound copied. Ctrl+V adds the next copy.":"Pasted model is visible. Ctrl+V adds the next copy.";
        designer_paste_failed=false;designer_paste_notice=audio_only?"Sound copy placed; Save keeps it.":apply?"Adding pasted model to the scene...":"Copy staged. Click Apply to display the model.";
        designer_paste_notice_until=production_now()+8;
        if(designer_reveal_paste)designer_reveal_pasted();
        message=designer_paste_notice;
    }
    void designer_separate_overlaps(){
        if(!production_can_save()||production_playing()||production_freeze())return;
        designer_finish_edit();world_studio::Document next;world_designer::Result result;
        if(!world_designer::separate_overlapping_models(draft,designer_context(),designer_model_bounds(),next,result,message)){designer_paste_notice=message;designer_paste_failed=true;designer_paste_notice_until=production_now()+10;return;}
        if(!result.changed){message="No exact model overlaps found.";return;}
        if(!structure_change_allowed(next,message))return;
        history.checkpoint(draft);draft=std::move(next);designer_selection=std::move(result.selection);
        designer_last_pasted=designer_selection.active;
        production_id_highwater=std::max(production_id_highwater,draft.entities.next_id);
        changed(true);designer_publish_selection();apply=true;
        designer_paste_previous={};designer_paste_pending=true;designer_paste_started_apply=applied;designer_paste_started_reject=rejected;
        designer_paste_failed=false;designer_paste_notice="Separated overlapping copies. Updating the scene...";designer_paste_notice_until=production_now()+8;
        designer_paste_success="Overlapping copies are separated and visible.";
        if(designer_reveal_paste)designer_reveal_pasted();
    }
    void designer_recover_draft(){
        if(!editor_session||!production_can_save()||production_playing()||production_freeze())return;
        designer_finish_edit();auto recovered=draft;std::string error;
        if(!editor_session->recover(recovered,production_now(),error)){message=error;return;}
        auto context=designer_context();context.ground.context=&recovered;
        world_studio::Document repaired;world_designer::Result result;
        if(!world_designer::separate_overlapping_models(recovered,context,designer_model_bounds(),repaired,result,error)){message="Recovery preserved: "+error;return;}
        checkpoint();draft=std::move(repaired);
        if(result.changed)designer_selection=std::move(result.selection);
        else if(!draft.placements.empty()){const auto ref=designer_binding_ref({world_entity::Kind::Placement,unsigned(draft.placements.size()-1)});designer_selection.items={ref};designer_selection.active=ref;}
        production_id_highwater=std::max(production_id_highwater,draft.entities.next_id);
        changed(true);designer_reconcile_selection();apply=true;
        designer_last_pasted=designer_selection.active;designer_paste_previous={};designer_recovery_dismissed=true;
        designer_paste_pending=true;designer_paste_started_apply=applied;designer_paste_started_reject=rejected;designer_paste_failed=false;
        designer_paste_notice=result.changed?"Recovered your unsaved scene and separated overlapping copies. Updating...":"Recovered your unsaved scene. Updating...";
        designer_paste_success="Recovered scene is visible. Save keeps your changes.";
        designer_paste_notice_until=production_now()+10;if(!designer_selection.active.empty())designer_reveal_pasted();message=designer_paste_notice;
    }
    void designer_paste_feedback(const Camera &cam){
        if(designer_paste_pending){
            if(rejected>designer_paste_started_reject){designer_paste_pending=false;designer_paste_failed=true;designer_paste_notice="Scene update failed: "+message;designer_paste_notice_until=production_now()+20;}
            else if(applied>designer_paste_started_apply){designer_paste_pending=false;designer_paste_failed=false;designer_paste_notice=designer_paste_success;designer_paste_notice_until=production_now()+8;if(designer_reveal_paste)designer_reveal_pasted();}
        }
        const bool recovery=editor_session&&!designer_recovery_dismissed&&!dirty&&editor_session->stats().recovery==world_editor::Recovery::Available;
        if(!recovery&&(designer_paste_notice.empty()||(!designer_paste_pending&&production_now()>designer_paste_notice_until)))return;
        const auto size=ImGui::GetIO().DisplaySize;const auto viewport=designer_viewport(size.x,size.y);
        ImGui::SetNextWindowPos({viewport.x+12,viewport.y+12});ImGui::SetNextWindowSize({std::min(440.f,viewport.w-24),0});ImGui::SetNextWindowBgAlpha(.94f);
        if(ImGui::Begin("Paste feedback",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoNav)){
            if(recovery){
                ImGui::TextWrapped("Your unsaved scene is available, including the copies you placed.");
                if(ImGui::Button("Recover unsaved scene"))designer_recover_draft();
                ImGui::SameLine();if(ImGui::SmallButton("Later"))designer_recovery_dismissed=true;
                ImGui::End();return;
            }
            ImGui::PushStyleColor(ImGuiCol_Text,designer_paste_failed?ImVec4{1,.5f,.36f,1}:ImVec4{.55f,.94f,.8f,1});ImGui::TextWrapped("%s",designer_paste_notice.c_str());ImGui::PopStyleColor();
            if(ImGui::SmallButton("Frame selection F"))designer_reveal_pasted();
            ImGui::SameLine();
            if(ImGui::SmallButton("Undo"))designer_command(world_designer_viewport::Command::Undo);
            if(designer_paste_failed&&ImGui::Button("Separate overlapping copies"))designer_separate_overlaps();
            if(!designer_object_preview&&dirty){ImGui::SameLine();if(ImGui::SmallButton("Apply"))apply=true;}
        }ImGui::End();(void)cam;
    }
    void designer_paste_settings(){
        ImGui::Checkbox("Snap pasted assets",&designer_paste_options.snap);
        ImGui::Checkbox("Reveal pasted copy",&designer_reveal_paste);
        ImGui::SetNextItemWidth(125);ImGui::Combo("Row direction",&designer_paste_options.direction,"Right\0Left\0Forward\0Back\0");
        ImGui::BeginDisabled(!designer_paste_options.snap);
        ImGui::SetNextItemWidth(100);ImGui::InputDouble("Gap (m)",&designer_paste_options.gap,.25,1,"%.2f");
        designer_paste_options.gap=std::isfinite(designer_paste_options.gap)?std::clamp(designer_paste_options.gap,0.,100.):0.;
        ImGui::EndDisabled();
        ImGui::TextDisabled("Direction follows the copied object's yaw.");
    }
    static const char *designer_tool_name(int index){
        static const char *names[]={"Terrain","Foliage","Objects","Lights","Materials","Splines / Walls / Bridges","Decals","Atmosphere","Shaders","Asset inspector","NPC / Navigation","VFX Studio","Water","Material graph","Production / Prefabs","Landscape","Living world","Cinematics","Regions / Layers","Gameplay","Physics","Animation","Surfaces","Wind / Roads","Live components","Audio","Weather"};
        return names[std::clamp(index,0,26)];
    }
    static int designer_workflow(int tool){
        switch(tool){
        case 0:case 15:case 22:case 7:case 26:return 0;
        case 1:case 6:case 23:return 1;
        case 2:case 3:case 4:case 5:case 8:case 9:case 13:case 18:case 20:return 2;
        case 12:return 3;
        default:return 4;
        }
    }
    std::future<std::vector<std::string>> designer_audit_job;
    std::vector<std::string> designer_audit_issues;
    bool designer_audit_open=false;
    bool designer_play_spawn_pending=false,designer_restore_selection_after_play=false;
    world_studio::Vec designer_play_spawn{};
    world_designer::Selection designer_play_selection;
    void designer_play_here(){
        world_entity::Transform selected;std::string error;
        if(!designer_selection.active.empty()&&world_designer::world_pose(draft,designer_context(),designer_selection.active,selected,error)){
            designer_play_spawn={selected.x,selected.y,selected.z};
        }else if(target)designer_play_spawn={x,y,z};
        else{message="Select an object or click the ground to choose a playtest start.";return;}
        designer_play_selection=designer_selection;
        landscape_cancel();designer_finish_edit();designer_asset_cancel();
        if(production_begin()){designer_play_spawn_pending=true;designer_restore_selection_after_play=true;}
    }
    void designer_audit(){
        if(designer_audit_job.valid()){message="Scene audit is still running.";return;}
        designer_audit_issues.clear();std::string error;
        if(!world_studio::valid(draft,error))designer_audit_issues.push_back("Scene validation: "+error);
        for(unsigned i=0;i<draft.placements.size();++i){world_studio::Pose p;if(!pose(WORLD_ADDED_KEY_BASE+i,p))continue;
            const double ground=world_atlas_height(p.root.x,p.root.z),bottom=p.root.y+p.minimum.y*p.scale;
            if(std::isfinite(ground)&&bottom-ground>1.5&&bottom-ground<12&&world_atlas_village_weight(p.root.x,p.root.z)<.01f)
                designer_audit_issues.push_back("Possible floating exterior prop #"+std::to_string(i+1)+" ("+p.alias+"): "+std::to_string(int((bottom-ground)*10)/10.)+" m above ground");
        }
        for(const auto &a:draft.assets){std::string lower=a.alias;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return char(std::tolower(c));});
            if(a.collision==1&&(lower.find("door")!=std::string::npos||lower.find("gate")!=std::string::npos||lower.find("arch")!=std::string::npos))designer_audit_issues.push_back("Check doorway collision: "+a.alias+" uses a solid box. Preserve source collision for an open passage.");
        }
        unsigned doorway_overlaps=0;
        for(unsigned i=0;i<source_names.size()&&doorway_overlaps<60;++i){std::string lower=source_names[i];std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return char(std::tolower(c));});
            if(lower.find("door")==std::string::npos&&lower.find("gate")==std::string::npos&&lower.find("arch")==std::string::npos)continue;
            world_studio::Pose doorway;if(!pose(i,doorway)||doorway.hidden)continue;
            for(unsigned j=0;j<draft.placements.size()&&doorway_overlaps<60;++j){const auto &placed=draft.placements[j];if(!landscape_solid_placement(draft,placed.alias))continue;
                world_studio::Pose prop;if(!pose(WORLD_ADDED_KEY_BASE+j,prop)||prop.hidden)continue;
                if(std::hypot(prop.root.x-doorway.root.x,prop.root.z-doorway.root.z)<2.2&&std::abs(prop.root.y-doorway.root.y)<3){
                    designer_audit_issues.push_back("Possible blocked doorway: "+placed.alias+" overlaps "+source_names[i]+"; test player passage.");++doorway_overlaps;
                }
            }
        }
        for(unsigned i=0;i<draft.actors.size();++i)if(draft.actors[i].enabled){std::vector<world_navigation::Point> route;if(!npc_route(draft,i,route,error))designer_audit_issues.push_back("NPC route "+draft.actors[i].name+": "+error);}
        for(unsigned i=0;i<draft.ecology.size();++i){const auto &r=draft.ecology[i];const double area=3.141592653589793*r.radius*r.radius;
            const double candidate=area*(r.density[0]/9.+r.density[1]/64.+r.density[2]/324.+r.density[3]/25.);
            if(candidate>6500)designer_audit_issues.push_back("Foliage cluster #"+std::to_string(i+1)+" is dense (~"+std::to_string(int(candidate))+" candidate cells); review draw distance and frame pacing.");
        }
        std::vector<std::pair<std::string,std::string>> paths;std::set<std::string> aliases;
        for(const auto &p:draft.placements)aliases.insert(p.alias);
        for(const auto &m:draft.materials)aliases.insert(m.alias);
        for(const auto &alias:aliases)paths.emplace_back(alias,model_path(alias));
        designer_audit_job=std::async(std::launch::async,[paths=std::move(paths)](){std::vector<std::string> issues;
            for(const auto &[alias,path]:paths){if(path.empty()||!std::filesystem::exists(path)){issues.push_back("Missing model/material source: "+alias);continue;}
                const auto report=world_assets::inspect(path);if(!report.supported)issues.push_back("Unreadable model/material: "+alias+" ("+report.error+")");
                else if(report.materials.empty()||report.images.empty())issues.push_back("Untextured asset: "+alias+" (review material import)");
            }return issues;});
        designer_audit_open=true;message="Scene audit started. Results appear in the Audit panel.";
    }
    void designer_audit_panel(){
        if(designer_audit_job.valid()&&designer_audit_job.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){auto more=designer_audit_job.get();designer_audit_issues.insert(designer_audit_issues.end(),more.begin(),more.end());message=designer_audit_issues.empty()?"Scene audit found no issues.":"Scene audit found "+std::to_string(designer_audit_issues.size())+" items to review.";}
        if(!designer_audit_open)return;ImGui::SetNextWindowSize({620,430},ImGuiCond_FirstUseEver);
        if(ImGui::Begin("Scene audit",&designer_audit_open)){if(ImGui::Button("Run again"))designer_audit();ImGui::SameLine();ImGui::TextDisabled("Read-only; no scene changes");
            if(designer_audit_job.valid())ImGui::TextUnformatted("Checking model files and material imports...");
            else if(designer_audit_issues.empty())ImGui::TextColored({.55f,.9f,.7f,1},"No issues found by the current checks.");
            else{ImGui::Text("%zu items to review",designer_audit_issues.size());ImGui::BeginChild("Audit results",{0,0},true);for(const auto &issue:designer_audit_issues)ImGui::BulletText("%s",issue.c_str());ImGui::EndChild();}}
        ImGui::End();
    }
    void designer_nudge(int axis,int sign,bool fine,bool coarse){
        if(production_freeze()||production_playing()||designer_blocks_viewport()||structure_cooking)return;
        const double distance=std::clamp(double(move_snap),.01,10.)*(fine?.1:coarse?10.:1.);
        world_studio::Vec delta{};const double yaw=gizmo_local&&selection_keys.size()==1?([&](){world_studio::Pose p;return pose(selection_keys.front(),p)?p.yaw:0.;})():0.;
        if(axis==0){delta.x=std::cos(yaw)*distance*sign;delta.z=-std::sin(yaw)*distance*sign;}
        else if(axis==1)delta.y=distance*sign;
        else{delta.x=std::sin(yaw)*distance*sign;delta.z=std::cos(yaw)*distance*sign;}
        designer_finish_edit();
        if(!selection_keys.empty()){
            auto next=draft;std::string error;
            if(!world_studio::transform_selection(next,source_poses,selection_keys,{},delta,0,1,error)){message=error;return;}
            auto graph=next.entities;if(!world_authoring::synchronize(next,graph,identity_sources,production_ground,&next,error)){message=error;return;}next.entities=std::move(graph);
            if(!world_studio::valid(next,error)||!structure_change_allowed(next,error)||!components_scene_prevalidate(next,error)){message=error;return;}
            const bool started_clean=!dirty;history.checkpoint(draft);draft=std::move(next);designer_move_fast_candidate=started_clean;changed(true);designer_publish_selection();designer_preview_object_edit();
        }else if(!designer_selection.active.empty()){
            world_entity::Transform transform;std::string error;
            if(!world_designer::world_pose(draft,designer_context(),designer_selection.active,transform,error)){message=error;return;}
            transform.x+=delta.x;transform.y+=delta.y;transform.z+=delta.z;
            world_designer::Command command;command.operation=world_designer::Operation::Transform;command.transform=transform;
            if(!designer_run(command))return;
        }else return;
        last_move_distance=distance;message="Nudged selection "+std::to_string(distance)+" m "+(gizmo_local?"on local axes.":"on world axes.");
    }
    world_designer_viewport::Rect designer_viewport(float width,float height)const{
        auto layout=designer_layout;if(!designer_content)layout.bottom=0;
        if(designer_immersive){layout.left=layout.right=layout.bottom=0;layout.top=40;}
        return world_designer_viewport::compute(width,height,layout,open);
    }
    bool designer_blocks_viewport()const{return production_freeze()||designer_palette_open||designer_close_requested||designer_help||designer_save_exit;}
    bool designer_should_confirm_close()const{return physics_modified||gameplay_dirty||components_modified||living_template_dirty||project_aux_dirty||dirty||!production_project_saved||production_playing()||production_freeze()||designer_edit_scope.active()||(drag.active&&drag.modified);}
    void designer_preferences_changed(){designer_preferences_dirty=true;designer_preferences_time=production_now();}
    world_designer_workspace::Preferences designer_preferences()const{
        world_designer_workspace::Preferences p;p.layout=designer_layout;p.tool=std::clamp(tab,0,26);p.category=designer_asset_category;
        p.grid=designer_asset_grid;p.favorites_only=designer_asset_favorites_only;p.ground_grid=designer_ground_grid;
        p.favorites=designer_asset_favorites;p.recent=designer_asset_recent;return p;
    }
    void designer_init(){
        designer_recovery_dismissed=!editor_session||editor_session->stats().recovery!=world_editor::Recovery::Available;
        designer_workspace_path=path+".workspace";world_designer_workspace::Preferences p;std::string error;
        if(world_designer_workspace::load(designer_workspace_path,p,error)){
            designer_layout=p.layout;tab=requested_tab=p.tool;designer_asset_category=p.category;designer_asset_grid=p.grid;
            designer_asset_favorites_only=p.favorites_only;designer_ground_grid=p.ground_grid;
            designer_asset_favorites=std::move(p.favorites);designer_asset_recent=std::move(p.recent);
        }
    }
    void designer_save_preferences(){std::string error;if(world_designer_workspace::save(designer_workspace_path,designer_preferences(),error))designer_preferences_dirty=false;else message=error;}
    void designer_load_saved(){
        if(production_freeze()){message="Finish scene restoration before loading.";return;}
        designer_finish_edit();cancel_designer_drag();designer_asset_cancel();
        world_studio::Document next;if(!world_studio::load(path.c_str(),next,message))return;
        checkpoint();next.next_structure_id=std::max(next.next_structure_id,draft.next_structure_id);draft=std::move(next);clear_selection();changed(true);designer_reconcile_selection();message="Saved world staged. Apply previews it.";
    }
    void designer_create_group(){
        world_designer::Command c;c.operation=world_designer::Operation::CreateGroup;c.name="New group";
        const auto origin=target?world_studio::Vec{x,y,z}:designer_creation_origin;c.transform={origin.x,origin.y,origin.z,0,1};
        world_entity::Transform selected_pose;std::string error;if(!designer_selection.active.empty()&&world_designer::world_pose(draft,designer_context(),designer_selection.active,selected_pose,error))c.transform=selected_pose;
        designer_run(c);designer_tools=false;
    }
    void designer_sync_scene_index(){
        if(designer_scene_source_signature!=signature||designer_scene_source_generation!=designer_source_generation){
            designer_scene_source_signature=signature;designer_scene_source_generation=designer_source_generation;
            designer_scene_source_token=signature+":"+std::to_string(designer_source_generation);
        }
        designer_scene_index.sync(draft.entities,source_names,designer_scene_source_token,revision,+[](const std::string &name){return world_designer_assets::humanize(name);});
    }
    void designer_frame_selection(const world_designer::Selection *references=nullptr){
        designer_sync_scene_index();
        world_studio::Vec lo={1e30,1e30,1e30},hi={-1e30,-1e30,-1e30};bool any=false;
        auto point=[&](double px,double py,double pz,double radius){lo.x=std::min(lo.x,px-radius);lo.y=std::min(lo.y,py-radius);lo.z=std::min(lo.z,pz-radius);hi.x=std::max(hi.x,px+radius);hi.y=std::max(hi.y,py+radius);hi.z=std::max(hi.z,pz+radius);any=true;};
        std::vector<bool> framed_models(WORLD_ADDED_KEY_BASE+draft.placements.size());
        auto model=[&](unsigned key){if(key>=framed_models.size()||framed_models[key])return;framed_models[key]=true;world_studio::Pose p;if(!pose(key,p))return;const auto matrix=vw_transform::matrix(vw_transform::pose(p));for(unsigned corner=0;corner<8;++corner){const auto q=vw_transform::point(matrix,{corner&1?p.maximum.x:p.minimum.x,corner&2?p.maximum.y:p.minimum.y,corner&4?p.maximum.z:p.minimum.z});point(q[0],q[1],q[2],0);}};
        const auto &selection=references?*references:designer_selection;
        for(unsigned key:world_designer::project(draft,designer_context(),selection).keys)model(key);
        std::vector<world_entity::Id> roots;for(const auto &ref:selection.items)if(ref.kind==world_designer::Kind::Entity)roots.push_back(ref.id);
        // One parent-first pass forms a deduplicated subtree union. No repeated
        // ancestor searches, including for a fully nested 1024-node selection.
        for(auto index:designer_scene_index.descendants(roots)){
            const auto &node=draft.entities.nodes[index];
            if(node.binding.kind==world_entity::Kind::SourceModel)model(node.binding.index);
            else if(node.binding.kind==world_entity::Kind::Placement)model(WORLD_ADDED_KEY_BASE+node.binding.index);
            else{world_entity::Transform world;std::string error;if(designer_scene_index.world_pose(draft.entities,node.id,world,error)){const double radius=node.binding.kind==world_entity::Kind::Weather&&node.binding.index<draft.weather_areas.size()?draft.weather_areas[node.binding.index].radius:2;point(world.x,world.y,world.z,radius);}}
        }
        if(!any&&target)point(x,y,z,5);
        if(!any){message="Select an object or pick ground to frame.";return;}
        designer_frame_center={(lo.x+hi.x)*.5,(lo.y+hi.y)*.5,(lo.z+hi.z)*.5};designer_frame_radius=std::max(1.,std::sqrt((hi.x-lo.x)*(hi.x-lo.x)+(hi.y-lo.y)*(hi.y-lo.y)+(hi.z-lo.z)*(hi.z-lo.z))*.5);designer_frame_pending=true;
    }
    void designer_command(world_designer_viewport::Command command){
        using C=world_designer_viewport::Command;using O=world_designer::Operation;
        if(structure_cooking&&command!=C::Frame&&command!=C::Copy){message="Source collision is rebuilding in the background. Camera navigation remains available.";return;}
        if(landscape_painting){if(command==C::Save||command==C::ApplySave){message="Release or cancel the landscape stroke before saving.";return;}landscape_cancel();if(command==C::Undo||command==C::Redo)return;}
        if(command==C::Undo&&structure_spline_drawing&&!structure_spline_new.points.empty()){structure_spline_new.points.pop_back();message="Last structure point removed.";return;}
        if(command==C::Undo&&!landscape_river_points.empty()){landscape_river_points.pop_back();message="Last river point removed.";return;}
        if(command==C::Palette){designer_palette_open=true;designer_palette_search[0]=0;return;}
        if(designer_blocks_viewport())return;
        if(command==C::Select||command==C::Move||command==C::Yaw||command==C::Scale){gizmo_mode=command==C::Select?3:command==C::Move?0:command==C::Yaw?1:2;tab=requested_tab=2;designer_tools=false;return;}
        if(command==C::Frame){if(structure_spline_active()){if(const auto *value=structure_spline_selected())structure_spline_frame(*value);else designer_frame_selection();}else designer_frame_selection();return;}
        if(command==C::Delete&&structure_spline_active()){structure_spline_delete();return;}
        if(command==C::SnapTerrain){designer_snap_terrain();return;}
        if(command==C::Copy){designer_copy();return;}
        if(command==C::Paste){designer_paste();return;}
        if(command==C::Undo||command==C::Redo){
            if(production_freeze())return;
            const auto landscape_before_undo=revision;designer_cancel_edit();restore_history(command==C::Redo);designer_reconcile_selection();
            if((tab==15||tab==5)&&revision!=landscape_before_undo)apply=true;
            world_designer::Ref ref;world_entity::Binding binding;std::string ignored;
            const bool exists=!designer_last_pasted.empty()&&world_designer::resolve(draft,designer_context(),designer_last_pasted,ref,binding,ignored);
            if(command==C::Redo&&exists){designer_select(ref);if(designer_reveal_paste)designer_reveal_pasted();}
            if(command==C::Undo&&!designer_last_pasted.empty()&&!exists){designer_paste_pending=false;designer_paste_failed=false;designer_paste_notice="Paste undone. Ctrl+Y restores it.";designer_paste_notice_until=production_now()+5;}
            return;
        }
        if(command==C::Save){designer_finish_edit();save();return;}
        if(command==C::ApplySave){project_request_save_all();return;}
        world_designer::Command edit;edit.operation=command==C::Duplicate?O::Duplicate:O::Erase;designer_run(edit);
    }
    bool designer_selected(const world_designer::Ref &ref)const{return std::find(designer_selection.items.begin(),designer_selection.items.end(),ref)!=designer_selection.items.end();}
    void designer_rebuild_rows(){
        designer_sync_scene_index();designer_scene_index.filter(designer_search,designer_collapsed);
    }
    static unsigned char designer_source_category(const std::string &name){
        auto has=[&](const char *word){return name.find(word)!=std::string::npos;};
        if(has("ground")||has("terrain")||has("river")||has("water")||has("road")||has("path")||has("snow")||has("rock")||has("cliff")||has("bank")||has("mud"))return 3;
        if(has("_tree")||has("tree_")||has("oak")||has("beech")||has("pine")||has("grass")||has("fern")||has("flower")||has("bush")||has("shrub")||has("leaf")||has("vine")||has("moss")||has("forest")||has("foliage")||has("jungle")||has("palm"))return 2;
        if(has("house")||has("shop")||has("castle")||has("wall")||has("door")||has("roof")||has("stair")||has("window")||has("tavern")||has("tower")||has("bridge")||has("fence")||has("gate")||has("pillar")||has("arch")||has("building")||has("fort")||has("hall"))return 1;
        if(has("cart")||has("crate")||has("barrel")||has("bench")||has("lamp")||has("table")||has("chair")||has("bed")||has("shelf")||has("sign")||has("basket")||has("pot")||has("bucket")||has("prop")||has("decor"))return 4;
        return 5;
    }
    void designer_refresh_source_categories(){
        if(designer_source_categories_generation==designer_source_generation&&designer_source_categories.size()==source_names.size())return;
        designer_source_categories.resize(source_names.size());
        for(unsigned i=0;i<source_names.size();++i)designer_source_categories[i]=designer_source_category(source_names[i]);
        designer_source_categories_generation=designer_source_generation;
    }
    bool designer_local_visible(world_designer::Ref ref)const{
        world_designer::Ref resolved;world_entity::Binding binding;std::string error;
        if(!world_designer::resolve(draft,designer_context(),ref,resolved,binding,error))return false;
        if(binding.kind==world_entity::Kind::SourceModel)for(const auto &o:draft.overrides)if(o.index==binding.index&&o.hidden)return false;
        const auto *node=resolved.kind==world_designer::Kind::Entity?world_entity::find(draft.entities,resolved.id):nullptr;return !node||node->visible;
    }
    void designer_row_menu(world_designer::Ref ref){
        if(ImGui::BeginPopupContextItem()){
            if(!designer_selected(ref))designer_select(ref);
            using O=world_designer::Operation;world_designer::Command c;c.subject=ref;
            if(ImGui::MenuItem("Frame selection","F"))designer_frame_selection();
            if(ImGui::MenuItem("Snap to terrain","End"))designer_snap_terrain();
            if(ImGui::MenuItem("Copy","Ctrl+C"))designer_copy();
            if(ImGui::MenuItem("Paste next snapped copy","Ctrl+V",false,designer_clipboard.ready()))designer_paste();
            if(ImGui::MenuItem("Duplicate","Ctrl+D")){c.operation=O::Duplicate;designer_run(c);}
            if(ImGui::MenuItem("Delete","Delete")){c.operation=O::Erase;designer_run(c);}
            if(ImGui::MenuItem("Unparent (keep world position)")){c.operation=O::Reparent;c.parent=0;designer_run(c);}
            if(ImGui::MenuItem("Hide / show")){c.operation=O::SetFlags;c.change_visible=true;c.visible=!designer_local_visible(ref);designer_run(c);}
            if(ImGui::MenuItem("Lock / unlock")){c.operation=O::SetFlags;c.change_locked=true;const auto *n=ref.kind==world_designer::Kind::Entity?world_entity::find(draft.entities,ref.id):nullptr;c.locked=n?!n->locked:true;designer_run(c);}
            ImGui::EndPopup();
        }
    }
    void designer_entity_drop(world_entity::Id parent){
        if(ImGui::BeginDragDropTarget()){
            if(const auto *p=ImGui::AcceptDragDropPayload("VALWOOD_ENTITY"))if(p->DataSize==sizeof(world_entity::Id)){
                world_entity::Id id=0;std::memcpy(&id,p->Data,sizeof id);world_designer::Command c;c.operation=world_designer::Operation::Reparent;c.subject=world_designer::Ref::entity(id);c.parent=parent;designer_run(c);
            }ImGui::EndDragDropTarget();
        }
    }
    void designer_outliner(const Camera &cam){
        ImGui::TextUnformatted("SCENE OUTLINER");ImGui::SameLine();if(ImGui::SmallButton("+ Group")){designer_create_group();}
        ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##scene-search","Search all objects...",designer_search,sizeof designer_search);
        designer_rebuild_rows();ImGui::TextDisabled("%zu authored  |  %zu imported",draft.entities.nodes.size(),source.size());
        ImGui::Separator();ImGui::Selectable("World root  (drop here to unparent)",false);designer_entity_drop(0);
        ImGui::BeginChild("Scene rows",{0,0});
        if(designer_search[0])ImGui::SetNextItemOpen(true,ImGuiCond_Always);
        if(ImGui::CollapsingHeader("Imported scenery")){
            designer_refresh_source_categories();
            const char *filters[]={"Near camera (150 m)","Buildings and structures","Trees and plants","Ground and water","Props and details","Other","All imported objects"};
            if(!designer_search[0]){ImGui::SetNextItemWidth(-1);ImGui::Combo("##world-filter",&designer_world_filter,filters,IM_ARRAYSIZE(filters));}
            else ImGui::TextDisabled("Searching all imported objects");
            const auto &sources=designer_scene_index.sources();
            std::vector<unsigned> shown;shown.reserve(sources.size());
            for(unsigned index:sources){
                if(index>=designer_source_categories.size()||index>=source_poses.size())continue;
                const auto filter=designer_search[0]?6:designer_world_filter;
                const bool selected_here=designer_selected(world_designer::Ref::source_model(index));
                if(!selected_here&&filter==0){const auto &p=source_poses[index].root;if(std::hypot(p.x-cam.x,p.z-cam.z)>150.||std::abs(p.y-cam.y)>(cam.y>100.?45.:100.))continue;}
                else if(!selected_here&&filter<6&&designer_source_categories[index]!=filter)continue;
                shown.push_back(index);
            }
            ImGui::TextDisabled("%zu shown  |  Search covers all areas",shown.size());
            ImGuiListClipper clip;clip.Begin(int(shown.size()));while(clip.Step())for(int row=clip.DisplayStart;row<clip.DisplayEnd;row++){
                const auto index=shown[size_t(row)];const auto ref=world_designer::Ref::source_model(index);ImGui::PushID(int(index)+8192);
                const auto &label=designer_scene_index.source_label(index);if(ImGui::Selectable(label.c_str(),designer_selected(ref),ImGuiSelectableFlags_AllowDoubleClick)){designer_select(ref,ImGui::GetIO().KeyCtrl);designer_tools=false;if(ImGui::IsMouseDoubleClicked(0))designer_frame_selection();}
                designer_row_menu(ref);if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\nSource %u",source_names[index].c_str(),index);ImGui::PopID();
            }
        }
        if(ImGui::CollapsingHeader("Authored scene",ImGuiTreeNodeFlags_DefaultOpen)){
            const auto &rows=designer_scene_index.rows();
            ImGuiListClipper clip;clip.Begin(int(rows.size()));while(clip.Step())for(int row=clip.DisplayStart;row<clip.DisplayEnd;row++){
                // Keep row metadata by value: a command can replace the draft.
                const auto entry=rows[size_t(row)];const auto ref=world_designer::Ref::entity(entry.id);const auto *n=designer_scene_index.node(draft.entities,entry.id);if(!n)continue;
                const auto row_id=std::to_string(entry.id);ImGui::PushID(row_id.c_str());ImGui::Indent(float(entry.depth)*12);
                if(entry.children){if(ImGui::SmallButton(entry.collapsed?">":"v")){auto at=std::find(designer_collapsed.begin(),designer_collapsed.end(),entry.id);if(at==designer_collapsed.end())designer_collapsed.push_back(entry.id);else designer_collapsed.erase(at);}ImGui::SameLine();}
                const auto name=n->name;std::string label=(!n->visible?"[hidden] ":"")+std::string(n->locked?"[lock] ":"")+name;
                if(ImGui::Selectable(label.c_str(),designer_selected(ref),ImGuiSelectableFlags_AllowDoubleClick)){designer_select(ref,ImGui::GetIO().KeyCtrl);designer_tools=false;if(ImGui::IsMouseDoubleClicked(0))designer_frame_selection();}
                designer_row_menu(ref);
                if(ImGui::BeginDragDropSource()){const auto id=entry.id;ImGui::SetDragDropPayload("VALWOOD_ENTITY",&id,sizeof id);ImGui::TextUnformatted(name.c_str());ImGui::EndDragDropSource();}
                designer_entity_drop(entry.id);ImGui::Unindent(float(entry.depth)*12);ImGui::PopID();
            }
        }
        ImGui::EndChild();
    }
    void designer_refresh_inspector(world_designer::Ref ref){
        if(designer_inspector_revision==revision&&designer_inspector_ref==ref&&designer_inspector_source_signature==signature&&designer_inspector_source_generation==designer_source_generation)return;
        designer_sync_scene_index();const auto context=designer_context();++designer_inspector_refreshes;
        designer_inspector_revision=revision;designer_inspector_ref=ref;designer_inspector_source_signature=signature;designer_inspector_source_generation=designer_source_generation;
        designer_inspector_name=world_designer::name(draft,context,ref);
        designer_inspector_error.clear();designer_inspector_pose_ok=world_designer::world_pose(draft,context,ref,designer_inspector_transform,designer_inspector_error);
        designer_inspector_visible=designer_local_visible(ref);
        const auto *node=ref.kind==world_designer::Kind::Entity?designer_scene_index.node(draft.entities,ref.id):nullptr;
        designer_inspector_locked=node&&node->locked;
        designer_inspector_inherited_lock=node&&designer_scene_index.inherited_locked(node->id);
    }
    bool designer_city_architecture_selected(){
        if(!active_city_physics)return false;
        auto architecture=[](const std::string &alias){
            if(alias.rfind("nm_city_",0)!=0)return false;
            for(const char *part:{"_sm_house_","_sm_shop_","_sm_boundary_","_sm_castle","_sm_ground_mesh","_sm_wooden_stairs_"})if(alias.find(part)!=std::string::npos)return true;
            return false;
        };
        auto model=[&](world_entity::Binding binding){
            if(binding.kind==world_entity::Kind::SourceModel&&binding.index<source_poses.size())return architecture(source_poses[binding.index].alias);
            if(binding.kind==world_entity::Kind::Placement&&binding.index<draft.placements.size())return architecture(draft.placements[binding.index].alias);
            return false;
        };
        const auto context=designer_context();std::vector<world_entity::Id> roots;
        for(const auto &ref:designer_selection.items){world_designer::Ref resolved;world_entity::Binding binding;std::string error;if(world_designer::resolve(draft,context,ref,resolved,binding,error)){if(model(binding))return true;if(resolved.kind==world_designer::Kind::Entity)roots.push_back(resolved.id);}}
        for(auto index:designer_scene_index.descendants(roots))if(model(draft.entities.nodes[index].binding))return true;
        return false;
    }
    void designer_item_wind(){
        designer_wind_enable_point=designer_wind_sway_point=designer_wind_flutter_point={};
        designer_sync_scene_index();const auto context=designer_context();
        auto keys=world_designer::project(draft,context,designer_selection).keys;
        std::vector<world_entity::Id> roots;
        for(const auto &ref:designer_selection.items)if(ref.kind==world_designer::Kind::Entity)roots.push_back(ref.id);
        for(auto index:designer_scene_index.descendants(roots)){
            const auto binding=draft.entities.nodes[index].binding;
            if(binding.kind==world_entity::Kind::SourceModel)keys.push_back(binding.index);
            else if(binding.kind==world_entity::Kind::Placement)keys.push_back(WORLD_ADDED_KEY_BASE+binding.index);
        }
        std::sort(keys.begin(),keys.end());keys.erase(std::unique(keys.begin(),keys.end()),keys.end());
        if(keys.empty())return;
        world_studio::Pose first;bool have=false,mixed_enabled=false,mixed_sway=false,mixed_flutter=false,locked=false,trees=false;
        size_t responsive=0,count=0;
        static std::uint64_t catalog_revision=~std::uint64_t(0);
        static std::unordered_map<std::string,unsigned> wind_capabilities;
        if(catalog_revision!=world_scenery_alias_revision()){
            wind_capabilities.clear();catalog_revision=world_scenery_alias_revision();
            for(size_t i=0;i<world_scenery_alias_count();++i){const auto asset=world_scenery_alias(i);if(asset.name)wind_capabilities.emplace(asset.name,asset.tree?2:asset.wind_response?1:0);}
        }
        for(unsigned key:keys){
            world_studio::Pose value;if(!world_studio::effective_pose(draft,source_poses,key,value))continue;
            if(!have){first=value;have=true;}else{mixed_enabled|=value.wind_enabled!=first.wind_enabled;mixed_sway|=value.wind_sway!=first.wind_sway;mixed_flutter|=value.wind_flutter!=first.wind_flutter;}
            ++count;const auto binding=world_entity::Binding{key<WORLD_ADDED_KEY_BASE?world_entity::Kind::SourceModel:world_entity::Kind::Placement,key<WORLD_ADDED_KEY_BASE?key:key-WORLD_ADDED_KEY_BASE};
            locked|=!world_entity::editable(draft.entities,binding);
            const auto capability=wind_capabilities.find(value.alias);if(capability!=wind_capabilities.end()){responsive+=capability->second>0;trees|=capability->second==2;}
        }
        if(!have)return;
        ImGui::SeparatorText("Item wind");
        ImGui::TextDisabled("%zu model%s%s",count,count==1?"":"s",count>1?" · applies to selection":"");
        if(!responsive){ImGui::TextWrapped("These meshes have no wind response. Buildings and rigid props stay still.");return;}
        if(responsive<count)ImGui::TextWrapped("%zu vegetation items respond; rigid props stay still.",responsive);
        const bool disabled=production_freeze()||production_playing()||locked;
        ImGui::BeginDisabled(disabled);
        bool enabled=first.wind_enabled;
        if(mixed_enabled)ImGui::TextDisabled("Wind enabled: mixed selection");
        const bool toggled=ImGui::Checkbox("Wind affects item",&enabled);
        {const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();designer_wind_enable_point={(lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f};}
        if(toggled){world_designer::Command c;c.operation=world_designer::Operation::SetWind;c.change_wind_enabled=true;c.wind_enabled=enabled;designer_run(c);}
        ImGui::BeginDisabled(!enabled&&!mixed_enabled);
        auto strength=[&](const char *label,float value,bool mixed,bool flutter,ImVec2 &point){
            if(mixed)ImGui::TextDisabled("%s: mixed",label);
            ImGui::TextUnformatted(label);ImGui::SetNextItemWidth(-1);
            const std::string slider_id=std::string("##item-wind-")+label;
            const bool edited=ImGui::SliderFloat(slider_id.c_str(),&value,0.f,2.f,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
            {const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();point={(lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f};}
            if(ImGui::IsItemActivated())designer_begin_edit();
            if(edited){world_designer::Command c;c.operation=world_designer::Operation::SetWind;c.change_wind_sway=!flutter;c.change_wind_flutter=flutter||!trees;c.wind_sway=value;c.wind_flutter=value;designer_update_edit(c);}
            if(ImGui::IsItemDeactivated())designer_finish_edit();
        };
        strength(trees?"Tree sway":"Wind strength",first.wind_sway,mixed_sway,false,designer_wind_sway_point);
        if(trees)strength("Leaf flutter",first.wind_flutter,mixed_flutter,true,designer_wind_flutter_point);
        ImGui::EndDisabled();
        if(ImGui::SmallButton("Reset item wind")){world_designer::Command c;c.operation=world_designer::Operation::SetWind;c.change_wind_enabled=c.change_wind_sway=c.change_wind_flutter=true;c.wind_enabled=true;c.wind_sway=c.wind_flutter=1;designer_run(c);}
        ImGui::EndDisabled();
        if(locked)ImGui::TextWrapped("Unlock the selected models and their parents to edit wind.");
        ImGui::TextWrapped("Responds to world wind and local wind zones. 1x uses the asset's normal motion.");
    }
    void designer_inspector(){
        if(designer_edit_scope.active()&&ImGui::IsKeyPressed(ImGuiKey_Escape))designer_cancel_edit();
        const auto ref=designer_selection.active;
        if(ref.empty()){
            ImGui::TextUnformatted("INSPECTOR");ImGui::Spacing();ImGui::TextWrapped("Select an object in the scene or outliner to edit it.");
            ImGui::Separator();ImGui::TextDisabled("Scene navigation");ImGui::BulletText("Mouse wheel: zoom in / out");ImGui::BulletText("RMB + WASD: fly camera");ImGui::BulletText("RMB + wheel: flight speed");ImGui::BulletText("Q / W / E / R: transform tools");ImGui::BulletText("F: frame selection");ImGui::BulletText("Ctrl+P: find tools and commands");
            ImGui::Spacing();if(ImGui::Button("Landscape tools",{-1,0})){designer_tools=true;tab=requested_tab=15;}if(ImGui::Button("Lighting and atmosphere",{-1,0})){designer_tools=true;tab=requested_tab=7;}return;
        }
        designer_refresh_inspector(ref);const auto &name=designer_inspector_name;
        if((ref!=designer_inspected||name!=designer_inspected_name)&&!ImGui::IsAnyItemActive()){designer_inspected=ref;designer_inspected_name=name;std::snprintf(designer_rename,sizeof designer_rename,"%s",name.c_str());}
        ImGui::TextDisabled("%zu selected",designer_selection.items.size());ImGui::SetNextItemWidth(-1);
        if(ImGui::InputText("##object-name",designer_rename,sizeof designer_rename,ImGuiInputTextFlags_EnterReturnsTrue)){
            world_designer::Command c;c.operation=world_designer::Operation::Rename;c.name=designer_rename;designer_run(c);
        }
        const bool protected_structure=structure_selection_protected();
        bool visible=designer_inspector_visible,locked=designer_inspector_locked;
        ImGui::BeginDisabled(protected_structure);
        if(ImGui::Checkbox("Visible",&visible)){world_designer::Command c;c.operation=world_designer::Operation::SetFlags;c.change_visible=true;c.visible=visible;designer_run(c);}
        ImGui::EndDisabled();
        ImGui::SameLine();if(ImGui::Checkbox("Locked",&locked)){world_designer::Command c;c.operation=world_designer::Operation::SetFlags;c.change_locked=true;c.locked=locked;designer_run(c);}
        // Commands can replace the graph; never retain a Node pointer across one.
        designer_refresh_inspector(ref);
        if(protected_structure){
            ImGui::PushStyleColor(ImGuiCol_Text,ImVec4{1.f,.78f,.4f,1.f});
            ImGui::TextWrapped("IMPORTED BASE GROUND: this environment field shares fixed terrain coverage and materials. Use Landscape for ordinary terrain; buildings, stairs, doors and walls support automatic collision rebuilding.");
            ImGui::PopStyleColor();
            structure_status_panel();
        }
        ImGui::BeginDisabled(protected_structure);
        if(ImGui::Button("Snap to Terrain   End",{-1,0}))designer_snap_terrain();
        ImGui::EndDisabled();
        if(!protected_structure&&structure_selection_uses_mesh())structure_status_panel();
        designer_item_wind();
        ImGui::SeparatorText("World transform");auto transform=designer_inspector_transform;
        if(designer_inspector_pose_ok){
            bool disabled=production_freeze()||designer_inspector_inherited_lock||protected_structure;
            ImGui::BeginDisabled(disabled);ImGui::TextDisabled("Active object · metres / degrees");
            auto field=[&](const char *label,double &value,float speed,const double *minimum=nullptr,const double *maximum=nullptr){
                ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(label);ImGui::SameLine(110);ImGui::SetNextItemWidth(-1);const std::string id=std::string("##transform-")+label;bool changed_value=ImGui::DragScalar(id.c_str(),ImGuiDataType_Double,&value,speed,minimum,maximum,"%.3f",ImGuiSliderFlags_AlwaysClamp);
                if(std::strcmp(label,"East · X")==0){const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();structure_move_field={(lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f};}
                if(ImGui::IsItemActivated())designer_begin_edit();
                if(changed_value){world_designer::Command c;c.operation=world_designer::Operation::Transform;c.transform=transform;designer_update_edit(c);}
                if(ImGui::IsItemDeactivated())designer_finish_edit();
            };
            field("East · X",transform.x,.1f);field("Height · Y",transform.y,.1f);field("South · Z",transform.z,.1f);if(designer_last_projection.sound<0&&designer_last_projection.weather<0){field("Yaw · Y",transform.yaw,.25f);field("Pitch · X",transform.pitch,.25f);field("Roll · Z",transform.roll,.25f);const double axis_min=.1,axis_max=8;field("Scale X",transform.sx,.005f,&axis_min,&axis_max);field("Scale Y",transform.sy,.005f,&axis_min,&axis_max);field("Scale Z",transform.sz,.005f,&axis_min,&axis_max);const double minimum=.001,maximum=10000;field("Uniform scale",transform.scale,.005f,&minimum,&maximum);}
            ImGui::EndDisabled();if(disabled&&!protected_structure)ImGui::TextWrapped("Unlock the object and its parents to transform it.");
            if(designer_edit_scope.active()&&ImGui::IsKeyPressed(ImGuiKey_Escape))designer_cancel_edit();
        }else ImGui::TextWrapped("%s",designer_inspector_error.c_str());
        ImGui::Separator();if(ImGui::Button("Frame",{-1,0}))designer_frame_selection();
        ImGui::SeparatorText("Copy / paste");
        if(ImGui::Button("Copy  Ctrl+C"))designer_copy();
        ImGui::SameLine();
        ImGui::BeginDisabled(!designer_clipboard.ready());if(ImGui::Button("Paste  Ctrl+V"))designer_paste();ImGui::EndDisabled();
        designer_paste_settings();
        if(ImGui::Button("Duplicate",{ImGui::GetContentRegionAvail().x*.48f,0}))designer_command(world_designer_viewport::Command::Duplicate);
        ImGui::SameLine();if(ImGui::Button("Delete",{-1,0}))designer_command(world_designer_viewport::Command::Delete);
        if(designer_last_projection.tool>=0&&ImGui::Button("Open component settings",{-1,0})){designer_tools=true;tab=requested_tab=designer_last_projection.tool;}
        if(!selection_keys.empty()){
            ImGui::SeparatorText("Model");ImGui::TextWrapped("%s",active_alias().c_str());
            if(ImGui::Button("Inspect source / LODs",{-1,0})){inspect_asset(active_alias());designer_tools=true;tab=requested_tab=9;}
            if(ImGui::Button("Material and surface settings",{-1,0})){designer_tools=true;tab=requested_tab=4;}
        }
    }
    bool designer_window(const char *name,ImVec2 position,ImVec2 size,ImGuiWindowFlags extra=0){
        ImGui::SetNextWindowPos(position);ImGui::SetNextWindowSize({std::max(1.f,size.x),std::max(1.f,size.y)});
        return ImGui::Begin(name,nullptr,ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings|extra);
    }
    void designer_splitter(const char *name,ImVec2 position,ImVec2 size,float &value,float sign,float minimum,float maximum){
        ImGui::SetCursorScreenPos(position);ImGui::InvisibleButton(name,size);
        if(ImGui::IsItemHovered()||ImGui::IsItemActive()){ImGui::SetMouseCursor(size.x<size.y?ImGuiMouseCursor_ResizeEW:ImGuiMouseCursor_ResizeNS);ImGui::GetWindowDrawList()->AddRectFilled(position,{position.x+size.x,position.y+size.y},IM_COL32(66,151,158,170));}
        if(ImGui::IsItemActive()){value=std::clamp(value+sign*(size.x<size.y?ImGui::GetIO().MouseDelta.x:ImGui::GetIO().MouseDelta.y),minimum,maximum);designer_preferences_changed();}
    }
    void designer_toolbar(const Camera &cam,float fps,bool &graphics){
        const auto display=ImGui::GetIO().DisplaySize;const auto viewport=designer_viewport(display.x,display.y);
        if(designer_window("Valwood workspace",{0,0},{display.x,viewport.y},ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_MenuBar|ImGuiWindowFlags_NoScrollbar)){
            if(ImGui::BeginMenuBar()){
                ImGui::TextColored({.38f,.81f,.77f,1},"VALWOOD");ImGui::SameLine();
                if(ImGui::BeginMenu("File")){
                    if(ImGui::MenuItem("Save All project changes","Ctrl+Shift+S",false,production_can_save()))project_request_save_all();
                    if(ImGui::MenuItem("Save applied world","Ctrl+S"))designer_command(world_designer_viewport::Command::Save);
                    if(ImGui::MenuItem("Load saved world",nullptr,false,!production_freeze()))designer_load_saved();
                    if(ImGui::MenuItem("Recover unsaved scene",nullptr,false,editor_session&&editor_session->stats().recovery==world_editor::Recovery::Available))designer_recover_draft();
                    if(ImGui::MenuItem("Save workspace layout"))designer_save_preferences();
                    ImGui::Separator();if(ImGui::MenuItem("Quit")){if(designer_should_confirm_close())designer_close_requested=true;else designer_exit_approved=true;}ImGui::EndMenu();
                }
                if(ImGui::BeginMenu("Edit")){
                    if(ImGui::MenuItem("Undo","Ctrl+Z"))designer_command(world_designer_viewport::Command::Undo);
                    if(ImGui::MenuItem("Redo","Ctrl+Shift+Z"))designer_command(world_designer_viewport::Command::Redo);
                    ImGui::Separator();
                    if(ImGui::MenuItem("Copy selection","Ctrl+C"))designer_command(world_designer_viewport::Command::Copy);
                    if(ImGui::MenuItem("Paste next snapped copy","Ctrl+V",false,designer_clipboard.ready()))designer_command(world_designer_viewport::Command::Paste);
                    if(ImGui::BeginMenu("Paste settings")){designer_paste_settings();ImGui::EndMenu();}
                    if(ImGui::MenuItem("Separate overlapping copies"))designer_separate_overlaps();
                    if(ImGui::MenuItem("Snap to terrain","End"))designer_snap_terrain();
                    if(ImGui::MenuItem("Duplicate selection","Ctrl+D"))designer_command(world_designer_viewport::Command::Duplicate);
                    if(ImGui::MenuItem("Delete selection","Delete"))designer_command(world_designer_viewport::Command::Delete);
                    ImGui::Separator();if(ImGui::MenuItem("Auto-preview object edits",nullptr,&designer_object_preview))designer_move_fast_candidate=false;
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Render placements and completed object edits automatically. Uses Apply validation for the current draft; saving remains explicit.");
                    ImGui::EndMenu();
                }
                if(ImGui::BeginMenu("Create")){
                    if(ImGui::MenuItem("Empty group")){designer_create_group();}
                    if(ImGui::MenuItem("Browse models")){designer_content=true;designer_audio_browser=false;}
                    for(int i:{3,10,11,12,17,25,26})if(ImGui::MenuItem(designer_tool_name(i))){if(i==26)weather_open();else if(i==25)sound_open();else{weather_cancel();sound_cancel();designer_tools=true;tab=requested_tab=i;}}
                    ImGui::EndMenu();
                }
                if(ImGui::BeginMenu("Quick Spots")){
                    auto jump=[&](const char *label,const char *id){
                        for(size_t i=0;i<std::size(scenic_views);++i)if(!std::strcmp(scenic_views[i].id,id)){
                            if(ImGui::MenuItem(label))quickspot_destination=int(i);
                            break;
                        }
                    };
                    ImGui::TextDisabled("Opening journey");
                    jump("Apartment | starting home","apartment");
                    jump("Seldonne | arrival campsite","seldonne-arrival");
                    jump("Seldonne | meadow road","seldonne-road");
                    jump("Seldonne | town approach","seldonne-town");
                    ImGui::Separator();ImGui::TextDisabled("Main regions");
                    jump("Valdoa | castle village","castle-village");
                    jump("Valdoa | market square","castle-market");
                    jump("Valdoa | castle courtyard","castle-courtyard");
                    jump("Old Tanthoon | jungle path","rainforest");
                    jump("Old Tanthoon | camp","rainforest-camp");
                    jump("Grandstone | snowy entrance","stronghold-gate");
                    jump("Grandstone | first hall","stronghold-hall");
                    jump("Crosswind | coast","coast");
                    ImGui::Separator();ImGui::TextDisabled("Moves the editor camera; your draft stays intact.");
                    ImGui::EndMenu();
                }
                if(ImGui::BeginMenu("Tools")){
                    if(ImGui::MenuItem("Audio editor"))sound_open();
                    if(ImGui::MenuItem("Weather editor"))weather_open();
                    if(ImGui::MenuItem("VFX editor")){weather_cancel();sound_cancel();landscape_cancel();designer_tools=true;designer_immersive=false;tab=requested_tab=11;}
                    if(ImGui::MenuItem("Scene audit"))designer_audit();
                    ImGui::EndMenu();
                }
                if(ImGui::Button("Landscape")){weather_cancel();sound_cancel();landscape_cancel();clear_selection();designer_tools=true;designer_immersive=false;tab=requested_tab=15;landscape_load_coverage();}
                {const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();landscape_open_screen={(a.x+b.x)*.5f,(a.y+b.y)*.5f};}
                if(ImGui::BeginMenu("View")){
                    ImGui::SetNextItemWidth(140);
                    if(ImGui::InputFloat("Fly speed (m/s)",&fly_speed,5.f,25.f,"%.0f"))fly_speed=std::clamp(std::isfinite(fly_speed)?fly_speed:40.f,1.f,1000.f);
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("Hold RMB + WASD to fly; Q/E move down/up.\nScroll over the viewport to zoom in/out.\nHold RMB + scroll to adjust flight speed (1-1000 m/s).\nApartment movement uses 15%% of the set speed (1-12 m/s); Shift doubles it.\nOutdoors, Shift gives 4x speed. Ctrl enables precise movement.");
                    ImGui::Separator();
                    ImGui::MenuItem("Immersive viewport",nullptr,&designer_immersive);ImGui::MenuItem("Content browser",nullptr,&designer_content);
                    if(city_showcase&&ImGui::MenuItem("Fixed source collision",nullptr,&structure_show_collision)&&structure_show_collision&&!structure_collision_loaded)structure_load_collision();
                    if(ImGui::MenuItem("Ground grid",nullptr,&designer_ground_grid))designer_preferences_changed();
                    if(ImGui::MenuItem("Reset workspace")){designer_layout={};designer_content=true;designer_immersive=false;designer_preferences_changed();}
                    if(ImGui::MenuItem("Graphics / performance","F10"))graphics=true;
                    ImGui::EndMenu();
                }
                if(ImGui::BeginMenu("Help")){if(ImGui::MenuItem("Controls and workflow"))designer_help=true;
                    ImGui::EndMenu();}
                ImGui::EndMenuBar();
            }
            if(!designer_immersive){
                ImGui::BeginDisabled(production_freeze());
                if(ImGui::Button("Apply")){designer_finish_edit();apply=true;}ImGui::SameLine();
                if(ImGui::Button("Save All"))designer_command(world_designer_viewport::Command::ApplySave);ImGui::SameLine();
                if(ImGui::Button("Undo"))designer_command(world_designer_viewport::Command::Undo);
                {auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();undo_screen={(a.x+b.x)*.5f,(a.y+b.y)*.5f};}ImGui::SameLine();
                if(ImGui::Button("Redo"))designer_command(world_designer_viewport::Command::Redo);
                ImGui::EndDisabled();ImGui::SameLine();ImGui::TextDisabled("|");ImGui::SameLine();
                using C=world_designer_viewport::Command;
                const char *names[]={"Select Q","Move W","Rotate E","Scale R"};const C commands[]={C::Select,C::Move,C::Yaw,C::Scale};const int modes[]={3,0,1,2};
                for(int i=0;i<4;i++){if(i)ImGui::SameLine();bool active=gizmo_mode==modes[i];if(active)ImGui::PushStyleColor(ImGuiCol_Button,{.16f,.36f,.38f,1});if(ImGui::Button(names[i]))designer_command(commands[i]);{auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();if(i==0)select_mode_screen={(a.x+b.x)*.5f,(a.y+b.y)*.5f};if(i==1)move_mode_screen={(a.x+b.x)*.5f,(a.y+b.y)*.5f};}if(active)ImGui::PopStyleColor();}
                ImGui::SameLine();ImGui::Checkbox("Snap",&snap);ImGui::SameLine();if(ImGui::Button("Frame F"))designer_frame_selection();
                ImGui::SameLine();ImGui::TextDisabled("|");ImGui::SameLine();
                ImGui::BeginDisabled(production_freeze());
                if(!production_playing()){if(ImGui::Button("Play Here"))designer_play_here();}
                else if(ImGui::Button("Stop")){editor_session->request_stop();designer_tools=true;tab=requested_tab=14;}
                ImGui::EndDisabled();ImGui::SameLine();
                if(ImGui::Button("More..."))ImGui::OpenPopup("More editor actions");
                if(ImGui::BeginPopup("More editor actions")){
                    ImGui::BeginDisabled(production_freeze());
                    if(ImGui::MenuItem("Copy selection","Ctrl+C"))designer_command(C::Copy);
                    if(ImGui::MenuItem("Paste next snapped copy","Ctrl+V",false,designer_clipboard.ready()))designer_command(C::Paste);
                    if(ImGui::MenuItem("Snap selection to terrain","End"))designer_command(C::SnapTerrain);
                    ImGui::Separator();
                    if(ImGui::MenuItem("Audit scene"))designer_audit();
                    if(!production_playing()&&ImGui::MenuItem("Play from default start")){landscape_cancel();designer_finish_edit();designer_asset_cancel();production_begin();}
                    ImGui::EndDisabled();
                    ImGui::Separator();if(ImGui::MenuItem("Find tools and commands","Ctrl+P"))designer_command(C::Palette);
                    ImGui::EndPopup();
                }
                (void)cam;(void)fps;
            }
        }ImGui::End();
    }
    void designer_popups(){
        if(designer_palette_open)ImGui::OpenPopup("Find tools and commands");
        ImGui::SetNextWindowSize({560,440},ImGuiCond_Appearing);
        if(ImGui::BeginPopupModal("Find tools and commands",&designer_palette_open,ImGuiWindowFlags_NoSavedSettings)){
            if(ImGui::IsWindowAppearing())ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##command-search","Type a tool or command...",designer_palette_search,sizeof designer_palette_search);
            ImGui::BeginChild("Command results",{0,-32});
            const char *labels[]={"Frame selection","Copy selection","Paste next snapped copy","Duplicate selection","Delete selection","Save all project changes","Undo","Redo","Snap to terrain"};using C=world_designer_viewport::Command;const C commands[]={C::Frame,C::Copy,C::Paste,C::Duplicate,C::Delete,C::ApplySave,C::Undo,C::Redo,C::SnapTerrain};
            bool first_result=true;const bool enter=ImGui::IsKeyPressed(ImGuiKey_Enter);
            for(int i=0;i<9;i++)if(world_designer_workspace::matches(labels[i],designer_palette_search))if(([&](){bool activated=ImGui::Selectable(labels[i],first_result);if(first_result&&enter)activated=true;first_result=false;return activated;})()){designer_palette_open=false;ImGui::CloseCurrentPopup();designer_command(commands[i]);}
            for(int i=0;i<27;i++)if(world_designer_workspace::matches(designer_tool_name(i),designer_palette_search))if(([&](){bool activated=ImGui::Selectable(designer_tool_name(i),first_result);if(first_result&&enter)activated=true;first_result=false;return activated;})()){tab=requested_tab=i;if(i==26)weather_open();else if(i==25)sound_open();else{weather_cancel();sound_cancel();}designer_tools=true;designer_immersive=false;designer_palette_open=false;designer_preferences_changed();ImGui::CloseCurrentPopup();}
            ImGui::EndChild();if(ImGui::Button("Close")||ImGui::IsKeyPressed(ImGuiKey_Escape)){designer_palette_open=false;ImGui::CloseCurrentPopup();}ImGui::EndPopup();
        }
        if(designer_help)ImGui::OpenPopup("Designer controls");
        if(ImGui::BeginPopupModal("Designer controls",&designer_help,ImGuiWindowFlags_AlwaysAutoResize)){
            ImGui::TextUnformatted("RMB + WASD / Q / E   Fly camera horizontally / vertically\nMouse wheel          Zoom in / out over the viewport\nRMB + wheel          Adjust flight speed\nShift / Ctrl         Move faster / precisely\nView menu            Set Fly speed (1-1000 m/s)\nQ / W / E / R        Select / Move / Yaw / Uniform scale\nF                    Frame selection or picked ground\nEnd                  Snap selection to terrain\nCtrl+click           Add or remove an object from selection\nAlt+click            Pick apartment furniture through walls\nCtrl+C / Ctrl+V      Copy / Paste next snapped copy\nCtrl+D / Delete      Duplicate / Delete selection\nCtrl+Z / Ctrl+Y      Undo / Redo\nCtrl+P               Find tools and commands\nCtrl+S               Save the applied world\nCtrl+Shift+S         Apply draft, then save\nF11                  Return to game / designer camera");
            ImGui::Separator();ImGui::TextUnformatted("Drag assets from the browser into the scene.\nDrag hierarchy rows to reparent; world position is preserved.\nApply previews the draft. Save stores the applied world.");
            if(ImGui::Button("Done")){designer_help=false;ImGui::CloseCurrentPopup();}ImGui::EndPopup();
        }
        if(designer_close_requested)ImGui::OpenPopup("Save before leaving?");
        if(ImGui::BeginPopupModal("Save before leaving?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
            ImGui::TextUnformatted("There are unsaved changes in this workspace.");
            ImGui::BeginDisabled(!production_can_save()||designer_save_exit);
            if(ImGui::Button("Save All and quit")){designer_save_exit=project_quit_after_save=true;project_request_save_all();if(!project_save_requested&&!project_save_job.valid())project_quit_after_save=designer_save_exit=false;}
            if(ImGui::Button("Keep scene recovery and quit")){designer_finish_edit();if(editor_session&&editor_session->flush_recovery(draft,production_now(),message))designer_exit_approved=true;}
            ImGui::EndDisabled();
            if(!production_can_save())ImGui::TextUnformatted("Complete Stop Play in Production before saving or keeping recovery.");
            if(physics_modified||gameplay_dirty||components_modified||living_template_dirty||project_aux_dirty||region_modified)ImGui::TextWrapped("Scene recovery stores the scene draft only. Use Save All to preserve the other unsaved project documents before leaving.");
            if(designer_save_exit)ImGui::TextUnformatted("Applying and saving the scene...");
            if(ImGui::Button("Quit without saving")){designer_save_pending=designer_save_exit=false;designer_exit_approved=true;}
            ImGui::SameLine();if(ImGui::Button("Cancel")){designer_close_requested=designer_save_exit=designer_save_pending=false;ImGui::CloseCurrentPopup();}
            ImGui::TextWrapped("%s",message.c_str());ImGui::EndPopup();
        }
    }
    void designer_grid(const Camera &cam){
        if(!designer_ground_grid)return;
        auto *draw=ImGui::GetBackgroundDrawList();
        const double step=std::max(1.,std::pow(10.,std::floor(std::log10(std::max(1.,double(cam.y)*.08)))));
        const double cx=std::round((target?x:cam.x)/step)*step,cz=std::round((target?z:cam.z)/step)*step;
        const double ground=target?y:world_atlas_height(cx,cz);
        for(int i=-15;i<=15;i++){ImVec2 a,b;const auto col=IM_COL32(137,168,179,i%5==0?90:36);
            if(project_point(cam,{cx+i*step,ground+.04,cz-15*step},a)&&project_point(cam,{cx+i*step,ground+.04,cz+15*step},b))draw->AddLine(a,b,col);
            if(project_point(cam,{cx-15*step,ground+.04,cz+i*step},a)&&project_point(cam,{cx+15*step,ground+.04,cz+i*step},b))draw->AddLine(a,b,col);}
    }

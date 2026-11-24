// Fixed source-geometry safety and a preview of actual city UCX collision.
    unsigned structure_guard_rejections=0,structure_overlay_edges=0;
    bool structure_cooking=false;
    ImVec2 structure_move_field={};
    bool structure_show_collision=false,structure_collision_loaded=false;
    world_physics::Document structure_collision_document;
    std::string structure_collision_status;
    world_editor_scene_guard::Mode structure_mode()const{
        return city_showcase?world_editor_scene_guard::Mode::City:
            stronghold_finish&&source_showcase?world_editor_scene_guard::Mode::Grandstone:world_editor_scene_guard::Mode::None;
    }
    bool structure_alias_protected(const std::string &alias)const{return world_editor_scene_guard::protected_alias(structure_mode(),alias);}
    bool structure_change_allowed(const world_studio::Document &candidate,std::string &error,bool count=true){
        const bool ok=world_editor_scene_guard::validate(live,candidate,source_poses,structure_mode(),error);
        if(!ok&&count)++structure_guard_rejections;
        return ok;
    }
    bool structure_selection_uses_mesh(){
        designer_sync_scene_index();const auto context=designer_context();std::vector<world_entity::Id> roots;
        auto model=[&](world_entity::Binding b){return b.kind==world_entity::Kind::SourceModel&&b.index<source_poses.size()?structure_alias_protected(source_poses[b.index].alias):b.kind==world_entity::Kind::Placement&&b.index<draft.placements.size()?structure_alias_protected(draft.placements[b.index].alias):false;};
        for(const auto &ref:designer_selection.items){world_designer::Ref resolved;world_entity::Binding b;std::string ignored;if(world_designer::resolve(draft,context,ref,resolved,b,ignored)){if(model(b))return true;if(resolved.kind==world_designer::Kind::Entity)roots.push_back(resolved.id);}}
        for(auto index:designer_scene_index.descendants(roots))if(model(draft.entities.nodes[index].binding))return true;
        return false;
    }
    // All structural transforms use the staged source-mesh cook, including
    // Undo/Redo. The inspector need not disable legitimate world design edits.
    bool structure_selection_protected(){
        designer_sync_scene_index();const auto context=designer_context();std::vector<world_entity::Id> roots;
        auto ground=[&](world_entity::Binding b){return b.kind==world_entity::Kind::SourceModel&&b.index<source_poses.size()?world_editor_scene_guard::immutable_ground_alias(source_poses[b.index].alias):b.kind==world_entity::Kind::Placement&&b.index<draft.placements.size()?world_editor_scene_guard::immutable_ground_alias(draft.placements[b.index].alias):false;};
        for(const auto &ref:designer_selection.items){world_designer::Ref resolved;world_entity::Binding binding;std::string ignored;if(world_designer::resolve(draft,context,ref,resolved,binding,ignored)){if(ground(binding))return true;if(resolved.kind==world_designer::Kind::Entity)roots.push_back(resolved.id);}}
        for(auto index:designer_scene_index.descendants(roots))if(ground(draft.entities.nodes[index].binding))return true;
        return false;
    }
    bool structure_geometry_changed(const world_studio::Document &before,const world_studio::Document &after)const{
        if(!structure_spline_same(before.structures,after.structures))return true;
        return world_editor_scene_guard::geometry_changed(before,after,source_poses,structure_mode());
    }
    void structure_load_collision(){
        structure_collision_loaded=active_playtest_walk&&active_playtest_walk->loaded();
        if(!structure_collision_loaded){structure_show_collision=false;structure_collision_status="Structural collision is preparing; the previous world remains active.";return;}
        structure_collision_status="Previewing committed source triangles/UCX. Moving, scaling, copying, hiding or deleting architecture rebuilds matching floors and blockers before it appears in play.";
    }
    void structure_status_panel(){
        if(structure_mode()==world_editor_scene_guard::Mode::None)return;
        ImGui::SeparatorText("Architecture collision");
        ImGui::TextWrapped("Source triangles keep doors, arches and stairs open. Structural edits rebuild floors and blockers together; failed cooks keep the previous scene. Save All preserves your edits.");
        if(ImGui::Checkbox("Preview structural collision",&structure_show_collision)&&structure_show_collision)structure_load_collision();
        if(structure_show_collision)ImGui::Text("Nearby mesh wireframe: %u edges (up to 4096)",structure_overlay_edges);
        if(!structure_collision_status.empty())ImGui::TextWrapped("%s",structure_collision_status.c_str());
    }
    void structure_overlay(const Camera &cam){
        structure_overlay_edges=0;if(!structure_show_collision||!open||!active_playtest_walk||!active_playtest_walk->loaded())return;
        structure_collision_loaded=true;auto *draw=ImGui::GetBackgroundDrawList();
        const auto &documents=active_playtest_walk->documents();
        for(std::size_t document_index=0;document_index<documents.size();++document_index){
          const auto &document=documents[document_index];
          for(std::size_t body_index=0;body_index<document.bodies.size();++body_index){
            const auto &body=document.bodies[body_index];if(body.shape!=world_physics::Shape::Mesh)continue;
            if(const auto *bounds=active_playtest_walk->body_bounds(document_index,body_index)){
                const double dx=std::max({bounds->xmin-cam.x,cam.x-bounds->xmax,0.});
                const double dz=std::max({bounds->zmin-cam.z,cam.z-bounds->zmax,0.});
                if(dx*dx+dz*dz>120.*120.)continue;
            }
            for(const auto &triangle:body.triangles){
                const auto first=physics_overlay_point(body.pose,body.vertices[triangle[0]]);
                if(std::hypot(first.x-cam.x,first.z-cam.z)>120)continue;
                for(unsigned edge=0;edge<3;++edge){
                    const auto a=physics_overlay_point(body.pose,body.vertices[triangle[edge]]),b=physics_overlay_point(body.pose,body.vertices[triangle[(edge+1)%3]]);ImVec2 pa,pb;
                    if(project_point(cam,a,pa)&&project_point(cam,b,pb)){draw->AddLine(pa,pb,IM_COL32(83,239,168,210),1.f);if(++structure_overlay_edges>=4096)return;}
                }
            }
          }
        }
    }

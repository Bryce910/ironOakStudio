// Include inside Studio after the subsystem/production members. The including
// translation unit supplies engine/world_designer.h. No rendering work here.
    world_designer::Selection designer_selection;
    world_designer::LegacySelection designer_last_projection;
    world_designer::EditScope designer_edit_scope;
    mutable world_designer::SourceCatalog designer_source_catalog;
    mutable std::string designer_source_key_signature;
    mutable bool designer_source_catalog_ready=false;
    mutable std::uint64_t designer_source_generation=1;
    bool designer_projection_ready=false,designer_edit_geometry=false;
    bool designer_native_qa_complete=false;
    bool designer_move_fast_candidate=false,designer_edit_started_clean=false;
    static bool designer_validate_candidate(const world_studio::Document &candidate,const void *context,std::string &error){return static_cast<const Studio *>(context)->components_scene_prevalidate(candidate,error);}
    void designer_invalidate_sources()const{designer_source_catalog_ready=false;if(++designer_source_generation==0)designer_source_generation=1;}
    world_designer::Context designer_context()const{
        world_designer::Context context;context.source_poses=&source_poses;
        if(!designer_source_catalog_ready||designer_source_catalog.identities().size()!=identity_sources.size()||designer_source_key_signature!=signature){designer_source_catalog=world_designer::SourceCatalog(identity_sources);designer_source_key_signature=signature;designer_source_catalog_ready=true;}
        context.source_identities=&designer_source_catalog.identities();context.source_keys=&designer_source_catalog.keys();context.source_catalog=&designer_source_catalog;
        context.ground={production_ground,&draft};context.source_signature=signature;
        context.validate_candidate=designer_validate_candidate;context.validation_context=this;
        if(components_ready){
            for(const auto &binding:components_active_document.physics)context.protected_entities.push_back(binding.entity);
            for(const auto &binding:components_active_document.living)context.protected_entities.push_back(binding.entity);
            // Conservative authoring guard for configured dialogue/encounter
            // identities as well as the directly bound motion components.
            for(const auto &entity:gameplay_document.entities)context.protected_entities.push_back(entity.id);
        }return context;
    }
    void designer_publish_selection(){
        designer_last_projection=world_designer::project(draft,designer_context(),designer_selection);
        const auto &p=designer_last_projection;selection_keys=p.keys;entity_selected=p.entity;
        selected=p.selected;selected_added=p.selected_added;light=p.light;actor_selection=p.actor;area_selection=p.area;particle_selection=p.particle;water_selection=p.water;sound_selection=p.sound;weather_selection=p.weather;
        designer_projection_ready=true;
    }
    void designer_reconcile_selection(){world_designer::reconcile(draft,designer_context(),designer_selection);designer_publish_selection();}
    bool designer_select(world_designer::Ref ref,bool append=false){
        if(designer_edit_scope.active())designer_finish_edit();
        if(!world_designer::select(draft,designer_context(),designer_selection,ref,append,message))return false;
        designer_publish_selection();return true;
    }
    world_designer::Ref designer_binding_ref(world_entity::Binding binding){
        auto ref=world_designer::from_binding(draft,binding);if(!ref.empty()||binding.kind==world_entity::Kind::None)return ref;
        // A newly appended authored record may not have passed Apply yet. Give
        // authored records IDs, without eagerly registering base-scene sources.
        auto graph=draft.entities;if(!world_authoring::synchronize(draft,graph,identity_sources,production_ground,&draft,message))return {};
        draft.entities=std::move(graph);production_id_highwater=std::max(production_id_highwater,draft.entities.next_id);
        revision++;entity_world_revision=~std::uint64_t(0);return world_designer::from_binding(draft,binding);
    }
    void designer_capture_legacy(){
        const auto old=designer_last_projection;
        if(entity_selected&&(!designer_projection_ready||entity_selected!=old.entity)){
            if(designer_select(world_designer::Ref::entity(entity_selected)))return;
        }
        if(selection_keys!=old.keys||(!designer_projection_ready&&!selection_keys.empty())){
            const auto keys=selection_keys;world_designer::Selection next;bool first=true;
            for(unsigned key:keys){const auto binding=world_entity::Binding{key<WORLD_ADDED_KEY_BASE?world_entity::Kind::SourceModel:world_entity::Kind::Placement,key<WORLD_ADDED_KEY_BASE?key:key-WORLD_ADDED_KEY_BASE};const auto ref=designer_binding_ref(binding);if(ref.empty())continue;if(world_designer::select(draft,designer_context(),next,ref,!first,message))first=false;}
            designer_selection=std::move(next);designer_publish_selection();return;
        }
        world_entity::Binding binding;bool changed_selection=false;
        if(tab==3&&light!=old.light&&light>=0){binding={world_entity::Kind::Light,unsigned(light)};changed_selection=true;}
        else if(tab==10&&actor_selection!=old.actor&&actor_selection>=0){binding={world_entity::Kind::Actor,unsigned(actor_selection)};changed_selection=true;}
        else if(tab==10&&area_selection!=old.area&&area_selection>=0){binding={world_entity::Kind::Navigation,unsigned(area_selection)};changed_selection=true;}
        else if(tab==11&&particle_selection!=old.particle&&particle_selection>=0){binding={world_entity::Kind::Particle,unsigned(particle_selection)};changed_selection=true;}
        else if(tab==12&&water_selection!=old.water&&water_selection>=0){binding={world_entity::Kind::Water,unsigned(water_selection)};changed_selection=true;}
        else if(tab==25&&sound_selection!=old.sound&&sound_selection>=0){binding={world_entity::Kind::Sound,unsigned(sound_selection)};changed_selection=true;}
        else if(tab==26&&weather_selection!=old.weather&&weather_selection>=0){binding={world_entity::Kind::Weather,unsigned(weather_selection)};changed_selection=true;}
        if(changed_selection){const auto ref=designer_binding_ref(binding);if(!ref.empty())designer_select(ref);}
        else if(!designer_projection_ready)designer_publish_selection();
    }
    bool designer_finish_edit(){
        if(!designer_edit_scope.active())return false;
        const bool committed=designer_edit_scope.commit(history);if(committed){changed(designer_edit_geometry);if(designer_edit_geometry){designer_move_fast_candidate=designer_edit_started_clean;designer_preview_object_edit();}else if(designer_last_projection.sound>=0||designer_last_projection.weather>=0)sound_preview_apply();}
        designer_edit_geometry=false;designer_reconcile_selection();return committed;
    }
    bool designer_cancel_edit(){
        if(!designer_edit_scope.cancel(draft,designer_selection))return false;
        production_id_highwater=std::max(production_id_highwater,draft.entities.next_id);
        revision++;entity_world_revision=~std::uint64_t(0);designer_edit_geometry=false;designer_publish_selection();return true;
    }
    bool designer_begin_edit(){if(!designer_edit_scope.active())designer_edit_started_clean=!dirty;return designer_edit_scope.begin(draft,designer_selection);}
    bool designer_update_edit(const world_designer::Command &command){
        if(command.operation==world_designer::Operation::Transform&&structure_selection_protected()){message="Source layout is protected because its collision/support is cooked separately. Edit decorative props or use the structural source workflow in Project tools.";++structure_guard_rejections;return false;}
        if(!designer_edit_scope.active()&&!designer_begin_edit())return false;
        // EditScope publishes only after validation succeeds, so a second full
        // draft copy here provided no additional rollback protection.
        world_designer::Result result;
        if(!designer_edit_scope.update(designer_context(),command,draft,designer_selection,result,message))return false;
        designer_edit_geometry|=result.geometry;
        revision++;entity_world_revision=~std::uint64_t(0);designer_publish_selection();return true;
    }
    bool designer_run(const world_designer::Command &command){
        if(production_freeze()){message="Finish scene restoration before editing designer entities.";return false;}
        designer_finish_edit();world_studio::Document next;world_designer::Result result;
        if(!world_designer::execute(draft,designer_selection,designer_context(),command,next,result,message))return false;
        if(!result.changed){designer_selection=std::move(result.selection);designer_publish_selection();return true;}
        if(!structure_change_allowed(next,message))return false;
        const bool audio_only=world_studio::environment_only_change(draft,next);history.checkpoint(draft);draft=std::move(next);designer_selection=std::move(result.selection);
        production_id_highwater=std::max(production_id_highwater,draft.entities.next_id);
        changed(result.geometry);designer_publish_selection();if(result.geometry)designer_preview_object_edit();else if(audio_only)sound_preview_apply();return true;
    }

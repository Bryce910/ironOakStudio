// Included inside Studio after the gameplay, physics and living inspectors.
    world_runtime::Document components_document;
    world_runtime::Document components_active_document;
    world_runtime_demo::Seed components_demo;
    std::unique_ptr<world_runtime::Runtime> components_runtime;
    std::unique_ptr<world_runtime::Frame> components_frame=std::make_unique<world_runtime::Frame>();
    std::vector<world_runtime::Entity> components_entities;
    std::vector<world_runtime::Entity> components_committed_entities;
    world_runtime::Snapshot components_play_snapshot;
    world_gameplay::Commands components_last_commands;
    unsigned components_request_cursor=0;
    std::uint64_t components_generation=0,components_rollback_serial=0,components_frames=0;
    bool components_ready=false,components_manifest_loaded=false,components_manifest_invalid=false,components_modified=false,components_play_snapshot_ready=false;
    world_runtime::Id components_selected_entity=0;
    world_physics::Id components_selected_body=0;
    world_living::Id components_selected_living=0;
    bool components_show_dialogue=true;
    bool components_scene_refresh_pending=false;
    bool components_stop_failed=false;
    world_editor::StopChoice components_pending_stop_choice=world_editor::StopChoice::DiscardPlayChanges;
    char components_file[256]="world.components";
    std::string components_diagnostic;

    static double components_wall(){return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();}
    static bool components_scene_entities(const world_studio::Document &doc,std::vector<world_runtime::Entity> &out,std::string &error){
        if(!world_entity::validate(doc.entities,&doc,error))return false;
        std::vector<std::pair<world_entity::Id,world_entity::Transform>> transforms;
        if(!world_entity::world_transforms(doc.entities,transforms,error))return false;
        std::vector<world_runtime::Entity> next;next.reserve(transforms.size());
        for(const auto &entry:transforms){const auto *node=world_entity::find(doc.entities,entry.first);if(!node){error="Missing committed entity during runtime binding";return false;}
            bool active=true;for(const auto *parent=node;parent;parent=parent->parent?world_entity::find(doc.entities,parent->parent):nullptr)active&=parent->visible;
            if(node->binding.kind==world_entity::Kind::Actor)active&=doc.actors[node->binding.index].enabled;
            if(node->binding.kind==world_entity::Kind::SourceModel)for(const auto &edit:doc.overrides)if(edit.index==node->binding.index)active&=!edit.hidden;
            const auto &t=entry.second;const double a=t.yaw*.008726646259971648;world_runtime::Entity entity;entity.id=entry.first;entity.pose.position={t.x,t.y,t.z};entity.pose.rotation={0,float(std::sin(a)),0,float(std::cos(a))};entity.scale=t.scale;entity.active=active;next.push_back(entity);
        }out=std::move(next);return true;
    }
    bool components_asset_path(const char *name,std::string &out){
        const std::filesystem::path relative(name?name:"");if(relative.empty()||relative.is_absolute()||relative.extension()!=".components"){message="Choose a project-relative .components filename.";return false;}for(const auto &part:relative)if(part==".."||part=="."||part.empty()){message="Components paths cannot traverse directories.";return false;}
        auto p=std::filesystem::path(project_directory);std::error_code ec;for(const auto &part:relative){p/=part;const auto status=std::filesystem::symlink_status(p,ec);if(!ec&&std::filesystem::is_symlink(status)){message="Components path cannot traverse symlinks.";return false;}ec.clear();}out=p.string();return true;
    }
    void components_init(){
        std::string file;if(!components_asset_path(components_file,file))return;
        std::error_code ec;if(!std::filesystem::exists(file,ec))return;
        if(world_runtime::load(file.c_str(),components_document,components_diagnostic))components_manifest_loaded=true;
        else{components_manifest_invalid=true;message="Live components manifest was not loaded: "+components_diagnostic;}
    }
    bool components_install(world_runtime::Configuration &configuration){
        if(!world_live_visuals_validate(live,configuration.components,components_demo,configuration.entities,components_diagnostic)){message="Live visual bindings rejected: "+components_diagnostic;return false;}
        auto next=std::make_unique<world_runtime::Runtime>();
        if(!next->configure(configuration,components_diagnostic)){message="Live components configuration rejected: "+components_diagnostic;return false;}
        for(auto &visual:components_demo.visuals){visual.living_actor=0;for(const auto &binding:configuration.components.living)if(binding.entity==visual.entity)for(const auto &actor:living_world.definition().actors)if(actor.id==binding.actor){visual.living_actor=actor.id;visual.name=actor.name;if(auto *node=world_entity::find(components_demo.markers,visual.entity))node->name=actor.name;}}
        const auto mind_file=std::filesystem::path(project_directory)/"ai/npc_minds.vwminds";
        std::error_code mind_error;if(std::filesystem::exists(mind_file,mind_error)&&!next->load_minds(mind_file.u8string().c_str(),components_diagnostic)){message="Saved NPC minds rejected: "+components_diagnostic;return false;}
        components_runtime=std::move(next);components_active_document=configuration.components;components_ready=true;components_entities=configuration.entities;components_generation++;components_frames=0;components_last_commands.count=0;components_request_cursor=0;components_play_snapshot={};components_play_snapshot_ready=false;
        components_runtime->read_frame(*components_frame);
        return true;
    }
    // Host calls once AFTER scenery capture and living_init, while loading.
    bool components_start(double castle_x,double castle_z,world_runtime::Vec jungle){
        if(components_manifest_invalid){message="Repair or explicitly replace the invalid components manifest before enabling the live runtime.";return false;}
        std::error_code living_file_error;if(!living_ready&&(std::filesystem::exists(std::filesystem::path(project_directory)/"saves/world.living",living_file_error)||std::filesystem::exists(std::filesystem::path(project_directory)/"world.living",living_file_error))){message="Existing living-world data could not be loaded. Repair it before initializing demo progress; its file is preserved.";return false;}
        if(components_manifest_loaded&&!living_ready&&!components_document.living_asset.empty()){
            world_runtime::Document logical_asset;logical_asset.living_asset=components_document.living_asset;world_runtime::Configuration initial;
            if(!world_runtime::load_project(project_directory,logical_asset,{},components_wall(),initial,components_diagnostic)){message="Saved components living asset could not initialize: "+components_diagnostic;return false;}
            living_world=std::move(initial.living);living_ready=true;living_modified=true;living_design=living_world.definition();living_clock_poll=0;living_save_clock=production_now();
        }
        std::vector<world_runtime::Entity> entities;if(!components_scene_entities(live,entities,components_diagnostic)){message=components_diagnostic;return false;}
        world_runtime_demo::Sites sites;sites.castle_x=castle_x;sites.castle_z=castle_z;sites.castle_floor=24;sites.jungle_x=jungle.x;sites.jungle_z=jungle.z;sites.jungle_floor=jungle.y;
        if(!world_runtime_demo::build(sites,entities,living_world,!living_ready,components_wall(),components_demo,components_diagnostic)){message="Built-in live component scene rejected: "+components_diagnostic;return false;}
        if(std::getenv("VALWOOD_PRESENTATION")&&!components_manifest_loaded&&!world_runtime_demo::presentation(sites,components_demo,world_atlas_walk_height(jungle.x+world_presentation::crossing_offset_x,jungle.z+world_presentation::crossing_offset_z),components_diagnostic)){message="Preview journey rejected: "+components_diagnostic;return false;}
        if(components_demo.initialized_living){living_ready=true;living_modified=true;living_design=living_world.definition();living_clock_poll=0;living_save_clock=production_now();}
        auto configuration=components_demo.configuration;
        if(components_manifest_loaded){if(!world_runtime::load_project(project_directory,components_document,configuration.entities,components_wall(),configuration,components_diagnostic)){message="Saved live components could not configure: "+components_diagnostic;return false;}configuration.external_living=&living_world;configuration.drive_living_clock=false;configuration.living_enabled=living_ready;}
        else components_document=configuration.components;
        if(!components_install(configuration))return false;
        components_committed_entities=std::move(entities);components_scene_refresh_pending=false;
        physics_document=configuration.physics;physics_modified=false;physics_selection=-1;physics_reseed_ids();gameplay_document=configuration.gameplay;gameplay_dirty=false;gameplay_baseline_valid=gameplay_editing=false;
        components_diagnostic=components_manifest_loaded?"Saved live scene components are active.":components_demo.status;message=components_diagnostic;return true;
    }
    bool components_export_demo_assets(){
        if(!production_can_save()||!components_ready){message="Stop Play and prepare the live demo before exporting its assets.";return false;}
        const auto root=std::filesystem::path(project_directory);const char *files[]={"runtime-demo.physics","runtime-demo.gameplay","runtime-demo.living","world.components"};std::error_code ec;for(const auto *file:files)if(std::filesystem::exists(std::filesystem::symlink_status(root/file,ec))){message="Demo export preserves existing files. Choose the separate subsystem Save tools to update an existing exported set.";return false;}
        world_living::World authored;if(!authored.configure(living_world.definition(),0,message))return false;
        if(!world_physics::save((root/files[0]).u8string().c_str(),components_demo.configuration.physics,message)||!world_gameplay::save((root/files[1]).u8string().c_str(),components_demo.configuration.gameplay,message)||!authored.save((root/files[2]).string(),message)||!world_runtime::save((root/files[3]).u8string().c_str(),components_demo.configuration.components,message)){message="Demo export stopped; existing user files were preserved. Newly written unreferenced subsystem assets may remain: "+message;return false;}
        components_document=components_demo.configuration.components;components_manifest_loaded=true;components_modified=false;std::snprintf(components_file,sizeof components_file,"world.components");message="New typed demo assets and world.components exported. The user's world.studio and player resource state were preserved.";return true;
    }
    bool components_configure_current(){
        if(!production_can_save()){message="Stop Play before configuring live components.";return false;}
        if(dirty){message="Apply the scene draft before configuring persistent component bindings.";return false;}
        world_runtime::Configuration c;c.components=components_document;if(!components_scene_entities(live,c.entities,components_diagnostic)){message=components_diagnostic;return false;}
        for(const auto &node:components_demo.markers.nodes)for(const auto &entity:components_demo.configuration.entities)if(entity.id==node.id)c.entities.push_back(entity);
        c.physics=physics_document;c.gameplay=gameplay_document;c.physics_enabled=!physics_document.bodies.empty();c.gameplay_enabled=!gameplay_document.entities.empty()||!gameplay_document.rules.empty();c.living_enabled=living_ready;c.external_living=&living_world;c.drive_living_clock=false;c.living=living_world;
        if(!components_install(c))return false;
        components_scene_entities(live,components_committed_entities,components_diagnostic);components_scene_refresh_pending=false;
        components_diagnostic="Live components now use the committed scene and current physics/gameplay editor definitions.";message=components_diagnostic;return true;
    }
    bool components_scene_applied(){
        if(!components_ready||!components_runtime)return true;
        if(production_playing()||production_freeze()){components_scene_refresh_pending=true;return true;}
        std::vector<world_runtime::Entity> authored;if(!components_scene_entities(live,authored,components_diagnostic)){message=components_diagnostic;return false;}
        auto equal=[](const world_runtime::Entity &a,const world_runtime::Entity &b){return a.id==b.id&&a.scale==b.scale&&a.active==b.active&&a.pose.position.x==b.pose.position.x&&a.pose.position.y==b.pose.position.y&&a.pose.position.z==b.pose.position.z&&a.pose.rotation.x==b.pose.rotation.x&&a.pose.rotation.y==b.pose.rotation.y&&a.pose.rotation.z==b.pose.rotation.z&&a.pose.rotation.w==b.pose.rotation.w;};
        if(authored.size()==components_committed_entities.size()&&std::equal(authored.begin(),authored.end(),components_committed_entities.begin(),equal)){components_scene_refresh_pending=false;return true;}
        auto merged=authored;for(const auto &node:components_demo.markers.nodes)for(const auto &entity:components_demo.configuration.entities)if(entity.id==node.id)merged.push_back(entity);
        if(!components_runtime->rebind_scene(merged,components_diagnostic)){message="Applied scene needs explicit live-component repair: "+components_diagnostic;return false;}
        components_committed_entities=std::move(authored);components_entities=std::move(merged);components_generation++;components_scene_refresh_pending=false;components_last_commands.count=0;components_runtime->read_frame(*components_frame);components_request_cursor=0;return true;
    }
    // Call before any renderer, scenery or shader commit. This stays active
    // during Play: deferred reconfiguration must not permit dangling references.
    bool components_scene_prevalidate(const world_studio::Document &next,std::string &error)const{
        if(!components_ready||!components_runtime)return true;
        for(const auto &node:next.entities.nodes){bool bound=false;for(const auto &b:components_active_document.physics)bound|=b.entity==node.id;for(const auto &b:components_active_document.living)bound|=b.entity==node.id;if(bound){world_entity::Transform t;if(!world_entity::world_transform(next.entities,node.id,t,error))return false;if(!vw_transform::simple(t)){error="A live physics/NPC binding requires its supported level uniform transform. Edit the collider/character adapter before applying pitch, roll or axis scale.";return false;}}}
        std::vector<world_runtime::Entity> entities;if(!components_scene_entities(next,entities,error))return false;
        for(const auto &node:components_demo.markers.nodes)for(const auto &entity:components_demo.configuration.entities)if(entity.id==node.id)entities.push_back(entity);
        if(!world_live_visuals_validate(next,components_active_document,components_demo,entities,error))return false;
        return components_runtime->can_rebind_scene(entities,error);
    }
    void components_service(double dt,double wall,world_runtime::Vec player,bool allow){
        components_request_cursor=0;components_frame->requests.count=0;
        if(!components_ready||!components_runtime||!allow)return;
        if(components_scene_refresh_pending&&!production_playing()&&!production_freeze()&&!components_scene_applied())return;
        if(!components_runtime->tick(std::clamp(dt,0.,.25),wall,player,*components_frame)){components_ready=false;message="Live components stopped after an invalid clock, changed logical binding or physics update error.";return;}
        components_last_commands=components_frame->requests;components_frames++;
        if(components_frame->gameplay_result!=world_gameplay::Result::Ok&&components_frame->gameplay_result!=world_gameplay::Result::NotConfigured)message="Gameplay transaction exceeded its budget or rejected an input; accepted input remains queued.";
    }
    bool components_poll_request(world_gameplay::Command &out){if(components_request_cursor>=components_frame->requests.count)return false;out=components_frame->requests.values[components_request_cursor++];return true;}
    bool components_interact(world_runtime::Vec player){if(!components_ready||!components_runtime)return false;world_runtime::Id id=0;return components_runtime->interact_nearest(player,3.25,id);}
    bool components_play_begin(){components_play_snapshot_ready=false;if(!components_ready||!components_runtime)return true;if(!components_runtime->snapshot(components_play_snapshot,components_diagnostic)){message=components_diagnostic;return false;}components_play_snapshot_ready=true;return true;}
    bool components_can_stop(world_editor::StopChoice){
        if(!components_play_snapshot_ready)return true;
        if(!components_runtime){message="Live runtime disappeared during Play; restore remains pending.";return false;}
        if(!components_runtime->can_restore(components_play_snapshot,components_wall(),components_diagnostic)){message="Live component restore preflight failed: "+components_diagnostic;return false;}return true;
    }
    bool components_play_stop(world_editor::StopChoice choice){
        components_pending_stop_choice=choice;
        if(!components_can_stop(choice)){components_stop_failed=true;return false;}
        // Both choices discard transient simulation. Keep applies only to the
        // authored documents/designs; those are managed by the editor Session.
        if(components_play_snapshot_ready&&components_runtime){if(!components_runtime->restore(components_play_snapshot,components_wall(),components_diagnostic)){components_stop_failed=true;message="Live component Play restoration failed: "+components_diagnostic;return false;}components_rollback_serial++;components_runtime->read_frame(*components_frame);}
        components_stop_failed=false;components_play_snapshot_ready=false;components_play_snapshot={};components_frame->requests.count=0;components_last_commands.count=0;components_request_cursor=0;return true;
    }
    void components_dialogue(){
        if(!components_show_dialogue||!components_ready||!components_runtime)return;
        const auto *line=components_runtime->current_line();if(!line)return;
        const auto size=ImGui::GetIO().DisplaySize;ImGui::SetNextWindowPos({size.x*.5f,size.y-20},ImGuiCond_Always,{.5f,1});ImGui::SetNextWindowSize({std::min(680.f,size.x-32),0},ImGuiCond_Always);ImGui::SetNextWindowSizeConstraints({std::min(320.f,size.x-32),100},{std::max(320.f,size.x-32),std::max(160.f,size.y*.70f)});
        if(ImGui::Begin("Valwood conversation",nullptr,ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_AlwaysAutoResize)){
            const auto remembered=components_runtime->recollection();
            if(remembered.name)ImGui::TextColored({.95f,.79f,.48f,1},"%s",remembered.name);
            if(remembered.recognition)ImGui::TextWrapped("%s",remembered.recognition);
            if(remembered.knowledge){ImGui::TextWrapped("%s",remembered.knowledge);ImGui::Separator();}
            ImGui::TextWrapped("%s",line->text.c_str());ImGui::Separator();const auto available=components_runtime->choices();for(unsigned i=0;i<available.count;i++){const unsigned index=available.indices[i];ImGui::PushID(int(index));if(ImGui::Button(line->choices[index].text.c_str(),{-1,0})){components_runtime->choose(index);ImGui::PopID();break;}ImGui::PopID();}if(ImGui::SmallButton("Leave conversation"))components_runtime->close_dialogue();}ImGui::End();
    }

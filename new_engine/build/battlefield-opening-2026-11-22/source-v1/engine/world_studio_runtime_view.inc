    WorldVillagePopulation *population_ptr=nullptr;
    std::vector<WorldSceneParticleEmitter> base_particles;
    std::vector<WorldTorchFlameEmitter> base_flames;
    std::vector<world_navigation::Grid> navigation_live;
    world_navigation::Grid navigation_preview;
    enum class NpcMode {None,Add,Append,MovePoint,Navigation,SelectExisting};
    NpcMode npc_mode=NpcMode::None;
    int npc_edit_actor=-1,npc_edit_point=-1;
    bool npc_input_consumed=false,npc_frame_pending=false,npc_target_source=false;
    unsigned npc_accepts=0,npc_rejections=0;
    ImVec2 npc_add_button,npc_draw_button,npc_finish_button,npc_remove_button,npc_frame_button,npc_appearance_button,npc_nav_button;
    std::vector<world_navigation::Point> npc_preview_route;
    std::string npc_notice;
    bool npc_notice_error=false,npc_scene_pending=false;
    unsigned npc_started_apply=0,npc_started_reject=0;
    void npc_schedule(){npc_scene_pending=true;npc_started_apply=applied;npc_started_reject=rejected;apply=true;}
    void npc_service(){
        if(!npc_scene_pending)return;
        if(rejected>npc_started_reject){npc_scene_pending=false;npc_notice_error=true;npc_notice="Scene update failed: "+message+" The previous visible NPC and route remain active.";}
        else if(applied>npc_started_apply){npc_scene_pending=false;npc_notice_error=false;npc_notice="NPC changes are visible. Cyan lines show the collision-checked route. Save All keeps your changes.";}
    }
    void npc_cancel(){const bool finishing=npc_mode!=NpcMode::None;npc_mode=NpcMode::None;npc_edit_actor=npc_edit_point=-1;if(finishing&&npc_notice_error&&!npc_scene_pending){npc_notice_error=false;npc_notice="Placement finished. Existing NPCs and routes are preserved.";}}
    static void npc_rect(ImVec2 &point){const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();point={(a.x+b.x)*.5f,(a.y+b.y)*.5f};}
    void npc_fail(const std::string &error){++npc_rejections;npc_notice="NPC unchanged: "+error;npc_notice_error=true;message=npc_notice;}
    void npc_arm(NpcMode mode,int actor=-1,int point=-1){
        if(production_freeze()||production_playing()||apply||structure_cooking){npc_fail("Wait for the scene update or stop Play before editing NPCs.");return;}
        if(mode==NpcMode::Add&&(!population_ptr||draft.actors.size()>=32)){npc_fail(!population_ptr?"The animated character library is unavailable.":"The 32 NPC edit limit is reached. Remove an edit first.");return;}
        if((mode==NpcMode::Append||mode==NpcMode::MovePoint)&&(actor<0||unsigned(actor)>=draft.actors.size()||!world_entity::editable(draft.entities,{world_entity::Kind::Actor,unsigned(actor)}))){npc_fail("Select an unlocked NPC first.");return;}
        if(mode==NpcMode::Append&&!world_village_pose_archetypes[draft.actors[unsigned(actor)].appearance].walk_count){npc_fail("This appearance is stationary. Choose a walking appearance before drawing a path.");return;}
        npc_mode=mode;npc_edit_actor=actor;npc_edit_point=point;npc_notice_error=false;
        npc_notice=mode==NpcMode::Add?"Click dry ground to place an animated NPC. Escape cancels.":mode==NpcMode::Append?"Click ground to add walking points. Finish path or Escape stops drawing.":mode==NpcMode::MovePoint?"Click ground to move the selected waypoint. Escape cancels.":mode==NpcMode::Navigation?"Click ground for the center of the navigation area.":"Click near the existing resident to edit them.";message=npc_notice;
    }
    bool npc_route(const world_studio::Document &doc,unsigned index,std::vector<world_navigation::Point> &route,std::string &error,WorldScenery *scene=nullptr){
        if(index>=doc.actors.size()){error="The selected NPC no longer exists.";return false;}
        const auto &a=doc.actors[index];
        const bool enabled=a.enabled&&world_entity::visible(doc.entities,{world_entity::Kind::Actor,index});
        const bool walking=world_village_pose_archetypes[a.appearance].walk_count!=0;
        auto query=nav_query(scene?scene:scene_ptr);
        if(!a.floor_heights.empty())return world_npc_source_navigation::route(a,enabled,walking,active_playtest_walk,query,
            a.navigation>=0?&doc.navigation[unsigned(a.navigation)]:nullptr,route,error);
        if(enabled)for(auto p:a.points)if(npc_imported_ground_unsupported(p[0],p[1])){error="This legacy NPC has no saved source-floor heights. Move its first waypoint onto the visible floor, then redraw the path.";return false;}
        world_navigation::Grid grid;const world_navigation::Grid *area=nullptr;
        if(enabled&&a.navigation>=0){if(unsigned(a.navigation)>=doc.navigation.size()){error="Choose a valid navigation area.";return false;}if(!npc_bake(doc.navigation[unsigned(a.navigation)],scene?scene:scene_ptr,grid,error))return false;area=&grid;}
        if(!world_npc_authoring::route(a,enabled,world_village_pose_archetypes[a.appearance].walk_count!=0,query,area,route,error)){error+=" Choose clear ground, or move the NPC onto a visible source floor before drawing an interior path.";return false;}
        return true;
    }
    bool npc_publish(world_studio::Document next,int index,bool coalesce=false){
        std::string error;std::vector<world_navigation::Point> route;
        if(!world_studio::valid(next,error)||(index>=0&&!npc_route(next,unsigned(index),route,error))){npc_fail(error);return false;}
        if(coalesce){if(!runtime_editing)history.checkpoint(runtime_before);runtime_editing=true;}else checkpoint();
        draft=std::move(next);actor_selection=index;changed();
        if(index>=0){const auto ref=designer_binding_ref({world_entity::Kind::Actor,unsigned(index)});if(!ref.empty())designer_select(ref);}
        npc_preview_route=std::move(route);npc_schedule();++npc_accepts;npc_notice_error=false;
        npc_notice="NPC and collision-checked route updating. Save All keeps your changes.";message=npc_notice;return true;
    }
    void npc_frame(){
        if(actor_selection<0||unsigned(actor_selection)>=draft.actors.size())return;
        const auto &a=draft.actors[unsigned(actor_selection)];if(a.points.empty())return;
        double lo_x=a.points[0][0],hi_x=lo_x,lo_z=a.points[0][1],hi_z=lo_z;
        for(auto p:a.points){lo_x=std::min(lo_x,p[0]);hi_x=std::max(hi_x,p[0]);lo_z=std::min(lo_z,p[1]);hi_z=std::max(hi_z,p[1]);}
        const double px=(lo_x+hi_x)*.5,pz=(lo_z+hi_z)*.5;
        double low=world_atlas_walk_height(px,pz),high=low;if(!a.floor_heights.empty()){low=*std::min_element(a.floor_heights.begin(),a.floor_heights.end());high=*std::max_element(a.floor_heights.begin(),a.floor_heights.end());}
        designer_frame_center={px,(low+high)*.5+1,pz};designer_frame_radius=std::max({4.,std::hypot(hi_x-lo_x,hi_z-lo_z)*.6+2,(high-low)*.6+2});designer_frame_heading=0;designer_frame_reveal=true;designer_frame_pending=true;npc_frame_pending=true;
    }
    bool npc_place_at(double px,double pz){
        if(apply||structure_cooking||production_freeze()||production_playing()){npc_fail("Wait for the scene update before placing another point.");return false;}
        if(npc_mode==NpcMode::Add){auto next=draft;world_studio::Actor a;a.name="Traveler "+std::to_string(next.actors.size()+1);a.points.push_back({px,pz});if(npc_target_source)a.floor_heights.push_back(y);next.actors.push_back(a);if(!npc_publish(std::move(next),int(draft.actors.size())))return false;npc_cancel();return true;}
        if(npc_mode==NpcMode::Navigation){if(draft.navigation.size()>=4){npc_fail("The four navigation area limit is reached.");return false;}auto next=draft;world_studio::NavigationArea area;area.x=px;area.z=pz;area.name="Navigation "+std::to_string(next.navigation.size()+1);area.source_floor=npc_target_source;area.floor_y=area.source_floor?y:0;next.navigation.push_back(area);world_navigation::Grid grid;std::string error;if(!npc_bake(area,scene_ptr,grid,error)||!grid.open){npc_fail(error.empty()?"The navigation area contains no walkable ground.":error);return false;}checkpoint();draft=std::move(next);area_selection=int(draft.navigation.size()-1);navigation_preview=std::move(grid);changed();npc_schedule();npc_cancel();return true;}
        if(npc_mode==NpcMode::SelectExisting){double best=12;int found=-1;WorldVillageResident resident;for(unsigned i=0;i<world_village_resident_count;i++){WorldVillageResident r;if(world_village_population_resident(population_ptr,i,&r)&&std::hypot(r.x-px,r.z-pz)<best){best=std::hypot(r.x-px,r.z-pz);found=int(i);resident=r;}}if(found<0){npc_fail("No existing resident within 12 m. Click closer to a resident.");return false;}for(unsigned i=0;i<draft.actors.size();i++)if(draft.actors[i].source==found){actor_selection=int(i);const auto ref=designer_binding_ref({world_entity::Kind::Actor,i});if(!ref.empty())designer_select(ref);npc_cancel();return true;}if(draft.actors.size()>=32){npc_fail("The 32 NPC edit limit is reached.");return false;}auto next=draft;world_studio::Actor a;a.source=found;a.name=resident.name?resident.name:"Resident";for(unsigned k=0;k<std::size(world_village_pose_archetypes);k++)if(resident.archetype&&std::strcmp(resident.archetype,world_village_pose_archetypes[k].name)==0)a.appearance=k;a.points.push_back({resident.x,resident.z});float floor=0;if(active_playtest_walk&&active_playtest_walk->surface(resident.x,resident.z,resident.y,floor))a.floor_heights.push_back(floor);next.actors.push_back(a);if(!npc_publish(std::move(next),int(draft.actors.size())))return false;npc_cancel();return true;}
        if(npc_edit_actor<0||unsigned(npc_edit_actor)>=draft.actors.size()||actor_selection!=npc_edit_actor||!world_entity::editable(draft.entities,{world_entity::Kind::Actor,unsigned(npc_edit_actor)})){npc_fail("The NPC selection changed or is locked.");npc_cancel();return false;}
        auto next=draft;auto &a=next.actors[unsigned(npc_edit_actor)];
        if(!a.floor_heights.empty()&&!npc_target_source){npc_fail("This patrol follows source floors. Click a visible floor or stair tread.");return false;}
        if(a.floor_heights.empty()&&npc_target_source){
            if(npc_mode==NpcMode::MovePoint&&npc_edit_point==0&&a.points.size()==1)a.floor_heights.push_back(y);
            else{npc_fail("Clear the walking path, then move its first point onto the source floor before adding interior waypoints.");return false;}
        }
        if(npc_mode==NpcMode::Append){if(a.points.size()>=32){npc_fail("The 32 waypoint limit is reached.");return false;}a.points.push_back({px,pz});if(!a.floor_heights.empty())a.floor_heights.push_back(y);}
        else if(npc_mode==NpcMode::MovePoint){if(npc_edit_point<0||unsigned(npc_edit_point)>=a.points.size()){npc_cancel();return false;}a.points[unsigned(npc_edit_point)]={px,pz};if(!a.floor_heights.empty())a.floor_heights[unsigned(npc_edit_point)]=y;}
        else return false;
        const auto mode=npc_mode;const int index=npc_edit_actor;if(!npc_publish(std::move(next),index))return false;if(mode==NpcMode::MovePoint)npc_cancel();return true;
    }
    void npc_input(const Camera &cam){
        npc_input_consumed=false;npc_service();
        if(structure_cooking)return;
        if(npc_mode==NpcMode::None)return;
        if(!open||tab!=10||production_freeze()||production_playing()||ImGui::IsKeyPressed(ImGuiKey_Escape)){npc_cancel();return;}
        if((npc_mode==NpcMode::Append||npc_mode==NpcMode::MovePoint)&&(actor_selection!=npc_edit_actor||npc_edit_actor<0||unsigned(npc_edit_actor)>=draft.actors.size()||!world_entity::editable(draft.entities,{world_entity::Kind::Actor,unsigned(npc_edit_actor)}))){npc_cancel();return;}
        auto &io=ImGui::GetIO();const auto viewport=designer_viewport(io.DisplaySize.x,io.DisplaySize.y);
        if(!viewport.contains(io.MousePos.x,io.MousePos.y)||io.WantCaptureMouse||io.MouseDown[1]||drag.active||designer_blocks_viewport())return;
        npc_input_consumed=true;target=false;pick(cam,io.MousePos.x,io.MousePos.y);
        auto *draw=ImGui::GetBackgroundDrawList();draw->AddText({io.MousePos.x+16,io.MousePos.y+18},IM_COL32(120,240,195,255),target?(npc_mode==NpcMode::Add?"Click to place NPC":npc_mode==NpcMode::Append?"Click to add walking point | Esc finishes":"Click ground to place | Esc cancels"):"Point at clear ground");
        if(target){ImVec2 at;if(project_point(cam,{x,y+.15,z},at))draw->AddCircle(at,10,IM_COL32(120,240,195,255),24,2);}
        if(ImGui::IsMouseClicked(0)){if(target)npc_place_at(x,z);else npc_fail("No walkable floor hit. Point at clear terrain or an open floor, away from walls.");}
    }

    int actor_selection=-1,area_selection=-1,particle_selection=-1,water_selection=-1;
    unsigned runtime_commits=0;bool runtime_qa_ready=false;
    bool water_move_armed=false;int water_geometry_index=-1;
    float water_geometry_radius=6,water_geometry_depth=1.5f;
    std::uint64_t water_geometry_revision=~std::uint64_t(0);
    unsigned water_geometry_commits=0,water_geometry_rejections=0;
    ImVec2 water_geometry_button,water_move_button,water_frame_button,water_geometry_radius_slider,water_geometry_depth_slider;
    bool water_publish_derived(world_studio::Document next,unsigned index){
        if(index>=next.waters.size()||apply||production_freeze()||!world_entity::editable(draft.entities,{world_entity::Kind::Water,index}))return false;
        auto source=landscape_pond_source();std::string error;const auto &w=next.waters[index];
        const double clearance=w.radius_x+std::max(4.,double(w.radius_x)*.25)+2.9;
        if((w.enabled&&!landscape_water_footprint(w.x,w.z,clearance,error,int(index)))||!world_pond_field::prepare(next,source,error)){
            ++water_geometry_rejections;landscape_creation_failure("Pond unchanged: "+error);return false;
        }
        designer_finish_edit();cancel_designer_drag();checkpoint();draft=std::move(next);changed(true);
        landscape_wait_for_build();apply=true;water_move_armed=false;++water_geometry_commits;
        message="Updating pond and its basin. The former ground is restored; Undo reverses this whole edit.";return true;
    }
    bool water_change_geometry(unsigned index,double px,double pz,float radius,float depth){
        if(index>=draft.waters.size()||draft.waters[index].basin_depth<=0)return false;
        auto next=draft;auto &w=next.waters[index];w.x=px;w.z=pz;w.radius_x=w.radius_z=radius;w.basin_depth=depth;
        return water_publish_derived(std::move(next),index);
    }
    void water_frame_selected(){
        if(water_selection<0||unsigned(water_selection)>=draft.waters.size())return;
        const auto &w=draft.waters[unsigned(water_selection)];if(w.source!=3)return;
        designer_frame_center={w.x,w.level,w.z};designer_frame_radius=std::max(7.,double(w.radius_x)*1.2);
        designer_frame_heading=0;designer_frame_reveal=true;designer_frame_pending=true;
    }
    struct RuntimePrepared {
        std::vector<world_navigation::Grid> grids;
        std::array<WorldVillageActorConfig,32> actors={};unsigned actor_count=0;
        std::vector<WorldSceneParticleEmitter> particles;
        std::vector<WorldTorchFlameEmitter> flames;
    };
    static bool water_sample(double px,double pz,float *level,float *depth,const void *data){
        const auto &doc=*static_cast<const world_studio::Document *>(data);
        if(doc.surface_field){float water=0,shore=0;if(world_surface::river(*doc.surface_field,px,pz,water,shore)){float d=water-world_atlas_height(px,pz);if(d>.005f){if(level)*level=water;if(depth)*depth=d;return true;}}}
        for(const auto &w:doc.waters)if(w.source==3&&w.enabled&&world_entity::visible(doc.entities,{world_entity::Kind::Water,unsigned(&w-doc.waters.data())})){double dx=(px-w.x)/w.radius_x,dz=(pz-w.z)/w.radius_z;if(dx*dx+dz*dz>=1)continue;float d=float(w.level)-world_atlas_height(px,pz);if(d>.005f){if(level)*level=float(w.level);if(depth)*depth=d;return true;}}
        return false;
    }
    static bool npc_imported_ground_unsupported(double px,double pz){
        if(!stronghold_finish||!source_showcase||(active_playtest_walk&&(active_structure_world||active_playtest_walk->contains(px,pz))))return false;
        WorldAtlasHeightRegionInfo region={};
        if(!world_atlas_height_region_info(&region))return true;
        // The imported district's source floors and cliffs are above the
        // terrain. An XZ terrain point cannot select their correct storey.
        const double xmax=region.xmin+(region.columns-1)*region.stepx,zmax=region.zmin+(region.rows-1)*region.stepz;
        return px>=region.xmin-1&&px<=xmax+1&&pz>=region.zmin-1&&pz<=zmax+1;
    }
    static bool nav_ground(void *,double px,double pz,double &height){
        if(npc_imported_ground_unsupported(px,pz))return false;
        height=world_atlas_walk_height(px,pz);
        if(!std::isfinite(height)||height<=.1||world_atlas_sample(px,pz).land_weight<=.75||world_atlas_local_water(px,pz,nullptr,nullptr))return false;
        if(!world_npc_source_navigation::terrain_floor(active_playtest_walk,active_structure_world,px,pz,height))return false;
        return true;
    }
    static float npc_terrain_height(double px,double pz,void *){return std::max(world_atlas_walk_height(px,pz),0.f);}
    static bool nav_clear(void *data,const world_navigation::Point &a,const world_navigation::Point &b,float radius){
        const double feet=std::min(a.y,b.y)+.01;
        if(!world_scenery_can_move(static_cast<WorldScenery *>(data),a.x,a.z,b.x,b.z,float(feet),1.75f,radius)||!generated_cover_collision_ready||!generated_cover_collision.can_move(a.x,a.z,b.x,b.z,feet,1.75,radius))return false;
        if(!world_stronghold_can_move(active_stronghold,a.x,a.z,b.x,b.z,feet,radius))return false;
        if(active_playtest_walk&&!world_npc_source_navigation::terrain_clear(*active_playtest_walk,active_structure_world,a,b,radius,npc_terrain_height,nullptr))return false;
        const world_physics::Vec3 center{a.x,feet+.9,a.z},delta{b.x-a.x,b.y-a.y,b.z-a.z};
        auto blocked=[](const world_physics::Hit &hit){return hit.hit&&hit.normal.y<.65&&hit.fraction<.999f;};
        if(active_city_physics&&!active_structure_world){world_physics::Hit hit;if(!active_city_physics->cast_capsule(center,radius,.50f,delta,hit)||blocked(hit))return false;}
        if(active_world_runtime&&active_world_runtime->configured()){world_physics::Hit hit;world_runtime::Id entity=0;if(!active_world_runtime->cast_capsule(center,radius,.50f,delta,hit,entity)||blocked(hit))return false;}
        return true;
    }
    world_navigation::Query nav_query(WorldScenery *scene)const{return {scene,nav_ground,nav_clear};}
    bool npc_bake(const world_studio::NavigationArea &area,WorldScenery *scene,world_navigation::Grid &grid,std::string &error)const {
        auto query=nav_query(scene);
        return area.source_floor?world_npc_source_navigation::bake(area,active_playtest_walk,query,grid,error):world_navigation::bake(area,query,grid,error);
    }
    static bool npc_source_floor(double px,double pz,double reference,double &height,const void *) {
        float floor=0;
        if(!active_playtest_walk||!active_playtest_walk->sample(px,pz,reference,floor))return false;
        height=floor;return true;
    }
    bool prepare_runtime(const world_studio::Document &doc,WorldScenery *scene,RuntimePrepared &next,std::string &error){
        next={};
        for(const auto &w:doc.waters)if(w.source==3&&w.enabled){
            if(w.level-world_atlas_height(w.x,w.z)<.08){error="Water "+std::to_string(unsigned(&w-doc.waters.data())+1)+" has no submerged center. Undo or remove this water edit in Water, then recreate it with Landscape > Water > Pond / Lake.";return false;}
            for(unsigned i=0;i<32;i++){double a=i*6.283185307/32;if(world_atlas_height(w.x+std::cos(a)*w.radius_x,w.z+std::sin(a)*w.radius_z)<w.level+.035){error="Water "+std::to_string(unsigned(&w-doc.waters.data())+1)+" has no dry rim. Undo or remove this water edit in Water, then recreate its closed basin with Landscape > Water > Pond / Lake. The previous world is preserved.";return false;}}
        }
        for(const auto &area:doc.navigation){world_navigation::Grid grid;if(!npc_bake(area,scene,grid,error))return false;next.grids.push_back(std::move(grid));}
        if(!doc.actors.empty()&&!population_ptr){error="Character pose library unavailable";return false;}
        for(const auto &a:doc.actors){auto &c=next.actors[next.actor_count++];c.source=a.source;c.appearance=a.appearance;c.enabled=a.enabled&&world_entity::visible(doc.entities,{world_entity::Kind::Actor,next.actor_count-1});c.speed=a.speed;c.dwell=a.dwell;c.yaw=a.yaw*.01745329252f;std::snprintf(c.name.data(),c.name.size(),"%s",a.name.c_str());
            for(const auto &node:doc.entities.nodes)if(node.binding==world_entity::Binding{world_entity::Kind::Actor,next.actor_count-1}){c.identity=node.id;break;}
            c.source_floor=!a.floor_heights.empty();
            std::vector<world_navigation::Point> route;
            if(!npc_route(doc,next.actor_count-1,route,error,scene)){error=a.name+": "+error;return false;}
            c.count=unsigned(route.size());std::copy(route.begin(),route.end(),c.points.begin());
            if(c.source_floor)for(unsigned i=0;i<c.count;++i)for(unsigned j=0;j<a.points.size();++j)
                if(std::hypot(c.points[i].x-a.points[j][0],c.points[i].z-a.points[j][1])<.1&&std::abs(c.points[i].y-a.floor_heights[j])<.8)c.pauses[i]=true;
        }
        if(!world_village_population_configuration_valid(next.actors.data(),next.actor_count)){error="Invalid population configuration";return false;}
        return prepare_vfx(doc,next,error);
    }
    bool prepare_vfx(const world_studio::Document &doc,RuntimePrepared &next,std::string &error,int solo=-1){
        next.particles=base_particles;next.flames=base_flames;
        if(solo>=0){for(auto &e:next.particles)e.strength=0;for(auto &e:next.flames)e.intensity=0;}
        for(const auto &p:doc.particles){
            if(solo>=0&&unsigned(&p-doc.particles.data())!=unsigned(solo))continue;
            if(p.kind==2){WorldTorchFlameEmitter e{p.x,p.y,p.z,p.height,p.radius,p.enabled&&world_entity::visible(doc.entities,{world_entity::Kind::Particle,unsigned(&p-doc.particles.data())})?p.power:0,p.seed};if(p.source>=64&&unsigned(p.source-64)<base_flames.size())next.flames[unsigned(p.source-64)]=e;else if(p.source<0)next.flames.push_back(e);else{error="Fire source no longer exists";return false;}}
            else{WorldSceneParticleEmitter e{{p.x,p.y,p.z},p.kind==0?WorldSceneParticleKind::Smoke:p.kind==1?WorldSceneParticleKind::Dust:p.kind==3?WorldSceneParticleKind::Sparks:WorldSceneParticleKind::Wisps,p.radius,p.height,p.enabled&&world_entity::visible(doc.entities,{world_entity::Kind::Particle,unsigned(&p-doc.particles.data())})?p.power:0,p.warmth,p.seed,p.ceiling,p.rate,p.lifetime,p.size,p.wind,{p.tint[0],p.tint[1],p.tint[2]},p.glow};if(p.source>=0&&unsigned(p.source)<base_particles.size())next.particles[unsigned(p.source)]=e;else if(p.source<0)next.particles.push_back(e);else{error="Smoke/dust source no longer exists";return false;}}
        }
        if(next.particles.size()>WORLD_SCENE_PARTICLE_EMITTER_CAPACITY||next.flames.size()>WORLD_TORCH_FLAME_CAPACITY){error="Shared particle emitter budget exceeded";return false;}
        if(!world_scene_particles_validate_emitters(next.particles.data(),next.particles.size())||!world_torch_flames_validate_emitters(next.flames.data(),next.flames.size())){error="Runtime configuration exceeds renderer or animation limits";return false;}
        return true;
    }
    bool commit_runtime(RuntimePrepared &next,WorldSceneParticles *particles,WorldTorchFlames *flames){
        // prepare_runtime has already validated all source kinds, paths and budgets.
        if(population_ptr&&!world_village_population_configure(population_ptr,next.actors.data(),next.actor_count))return false;
        world_village_population_source_floor(population_ptr,npc_source_floor);
        if(!world_scene_particles_set_emitters(particles,next.particles.data(),next.particles.size())||!world_torch_flames_set_emitters(flames,next.flames.data(),next.flames.size()))return false;
        navigation_live=std::move(next.grids);runtime_commits++;return true;
    }
    void runtime_panel(const Camera &cam){
        npc_service();
        if(tab==10&&ImGui::BeginTabItem("NPC / Nav",nullptr,requested_tab==10?ImGuiTabItemFlags_SetSelected:0)){tab=10;
            ImGui::TextWrapped("Place an animated NPC, then draw where they walk. Each accepted edit updates the scene; Save All keeps it.");
            if(ImGui::Button("Add NPC",{-1,28}))npc_arm(NpcMode::Add);
            npc_rect(npc_add_button);
            if(ImGui::Button("Choose existing resident",{-1,24}))npc_arm(NpcMode::SelectExisting);
            if(npc_mode!=NpcMode::None){if(ImGui::Button("Finish / cancel placement",{-1,26}))npc_cancel();npc_rect(npc_finish_button);}
            if(!npc_notice.empty()){ImGui::PushStyleColor(ImGuiCol_Text,npc_notice_error?ImVec4{1,.55f,.38f,1}:ImVec4{.55f,.92f,.77f,1});ImGui::TextWrapped("%s",npc_notice.c_str());ImGui::PopStyleColor();}
            ImGui::SeparatorText("NPCs");
            for(unsigned i=0;i<draft.actors.size();i++){ImGui::PushID(int(i));if(ImGui::Selectable(draft.actors[i].name.c_str(),actor_selection==int(i))){npc_cancel();actor_selection=int(i);const auto ref=designer_binding_ref({world_entity::Kind::Actor,i});if(!ref.empty())designer_select(ref);}ImGui::PopID();}
            if(actor_selection>=0&&unsigned(actor_selection)<draft.actors.size()){
                const unsigned index=unsigned(actor_selection);const bool editable=world_entity::editable(draft.entities,{world_entity::Kind::Actor,index});
                ImGui::BeginDisabled(!editable||apply||production_freeze());auto a=draft.actors[index];bool edit=false;
                if(ImGui::Button("Frame NPC and path",{-1,24}))npc_frame();
                npc_rect(npc_frame_button);
                ImGui::BeginDisabled(!world_village_pose_archetypes[a.appearance].walk_count);if(ImGui::Button("Draw walking path",{-1,28}))npc_arm(NpcMode::Append,int(index));npc_rect(npc_draw_button);ImGui::EndDisabled();
                char name[65];std::snprintf(name,sizeof name,"%s",a.name.c_str());if(ImGui::InputText("Resident name",name,sizeof name)){a.name=name;edit=true;}
                edit|=ImGui::Checkbox("Resident enabled",&a.enabled);
                if(ImGui::BeginCombo("Appearance",world_village_pose_archetypes[a.appearance].name)){for(unsigned i=0;i<std::size(world_village_pose_archetypes);i++){const bool allowed=a.points.size()<2||world_village_pose_archetypes[i].walk_count;ImGui::BeginDisabled(!allowed);if(ImGui::Selectable(world_village_pose_archetypes[i].name,a.appearance==i)){a.appearance=i;edit=true;}ImGui::EndDisabled();if(!allowed&&ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("Stationary appearance. Clear the walking path first.");}ImGui::EndCombo();}npc_rect(npc_appearance_button);
                ImGui::TextDisabled(world_village_pose_archetypes[a.appearance].walk_count?"Supports walking and idle animation":"Stationary animation only");
                edit|=ImGui::SliderFloat("Walking speed (m/s)",&a.speed,.3,2.5);edit|=ImGui::SliderFloat("Waypoint pause (s)",&a.dwell,.4,15);edit|=ImGui::SliderFloat("Stationary facing",&a.yaw,-180,180);edit|=ImGui::Checkbox("Closed patrol (off = return same path)",&a.loop);
                const char *area=a.navigation<0?"Direct collision checks":draft.navigation[unsigned(a.navigation)].name.c_str();if(ImGui::BeginCombo("Navigation area",area)){if(ImGui::Selectable("Direct collision checks",a.navigation<0)){a.navigation=-1;edit=true;}for(unsigned i=0;i<draft.navigation.size();i++)if(ImGui::Selectable(draft.navigation[i].name.c_str(),a.navigation==int(i))){a.navigation=int(i);edit=true;}ImGui::EndCombo();}
                ImGui::Text("%zu / 32 waypoints | %s",a.points.size(),a.source<0?"new NPC":"existing resident edit");
                if(ImGui::Button("Remove last waypoint")&&a.points.size()>1){a.points.pop_back();if(!a.floor_heights.empty())a.floor_heights.pop_back();edit=true;}
                ImGui::SameLine();if(ImGui::Button("Clear walking path")&&a.points.size()>1){a.points.resize(1);if(!a.floor_heights.empty())a.floor_heights.resize(1);edit=true;npc_cancel();}
                for(unsigned i=0;i<a.points.size();i++){ImGui::PushID(int(i));ImGui::Text("Waypoint %u",i+1);ImGui::SameLine();if(ImGui::SmallButton("Move in scene"))npc_arm(NpcMode::MovePoint,int(index),int(i));edit|=ImGui::InputDouble("East",&a.points[i][0],.5,5,"%.2f");edit|=ImGui::InputDouble("South",&a.points[i][1],.5,5,"%.2f");ImGui::PopID();}
                if(edit){auto candidate=draft;candidate.actors[index]=std::move(a);npc_publish(std::move(candidate),int(index),true);}
                if(ImGui::Button("Remove NPC edit",{-1,26})){npc_cancel();if(production_remove({world_entity::Kind::Actor,index})){actor_selection=-1;designer_reconcile_selection();npc_preview_route.clear();npc_schedule();npc_notice="NPC edit removed. Save All keeps this change.";npc_notice_error=false;}}
                npc_rect(npc_remove_button);ImGui::EndDisabled();
            }
            ImGui::SeparatorText("Obstacle detours");
            ImGui::TextWrapped("For routes around walls or rocks, add a navigation area and choose it for the NPC. Click the floor to choose its storey. Source-floor areas support actual stairs and open doorways; closed doors remain solid.");
            if(ImGui::Button("Add navigation area",{-1,25}))npc_arm(NpcMode::Navigation);
            npc_rect(npc_nav_button);
            for(unsigned i=0;i<draft.navigation.size();i++)if(ImGui::Selectable(draft.navigation[i].name.c_str(),area_selection==int(i))){npc_cancel();area_selection=int(i);}
            if(area_selection>=0&&unsigned(area_selection)<draft.navigation.size()){
                const unsigned index=unsigned(area_selection);ImGui::BeginDisabled(!world_entity::editable(draft.entities,{world_entity::Kind::Navigation,index})||apply||production_freeze());auto a=draft.navigation[index];bool edit=ImGui::InputDouble("Area east",&a.x,1,10,"%.1f");edit|=ImGui::InputDouble("Area south",&a.z,1,10,"%.1f");edit|=ImGui::SliderFloat("Area span (m)",&a.span,8,96);edit|=ImGui::SliderFloat("Cell (m)",&a.cell,1,4);edit|=ImGui::SliderFloat("Agent radius (m)",&a.radius,.2,a.source_floor?.37f:.8f);edit|=ImGui::SliderFloat("Maximum slope",&a.slope,.1,1);
                if(edit){auto candidate=draft;candidate.navigation[index]=a;world_navigation::Grid grid;std::string error;bool valid=world_studio::valid(candidate,error)&&npc_bake(a,scene_ptr,grid,error)&&grid.open;for(unsigned i=0;valid&&i<candidate.actors.size();i++)if(candidate.actors[i].navigation==int(index)){std::vector<world_navigation::Point> route;valid=npc_route(candidate,i,route,error);}if(valid){if(!runtime_editing)history.checkpoint(runtime_before);runtime_editing=true;draft=std::move(candidate);navigation_preview=std::move(grid);changed();npc_schedule();}else npc_fail(error.empty()?"The area has no traversable ground.":error);}
                ImGui::Text("Preview: %u traversable / %zu cells",navigation_preview.open,navigation_preview.cells.size());if(ImGui::Button("Delete navigation area")){
                    // Erase through the same identity-aware operation as the
                    // committed removal. Detaching a patrol from A* may make
                    // its direct path unsafe; do not publish that candidate.
                    auto candidate=draft;std::string error;
                    bool valid=world_authoring::erase_record(candidate,{world_entity::Kind::Navigation,index},error);
                    for(unsigned i=0;valid&&i<candidate.actors.size();++i){std::vector<world_navigation::Point> route;if(!npc_route(candidate,i,route,error)){error=candidate.actors[i].name+": "+error;valid=false;}}
                    if(!valid)npc_fail("Navigation area retained. Reroute its NPCs before removing it. "+error);
                    else if(production_remove({world_entity::Kind::Navigation,index})){npc_cancel();area_selection=-1;navigation_preview={};npc_schedule();}
                }ImGui::EndDisabled();
            }
            ImGui::TextWrapped("32 NPC edits, 32 points per NPC, four local navigation areas. Escape finishes placement. Rejected points leave the NPC and its previous route unchanged.");ImGui::EndTabItem();
        }
        if(tab==11&&ImGui::BeginTabItem("VFX",nullptr,requested_tab==11?ImGuiTabItemFlags_SetSelected:0)){tab=11;vfx_panel(cam);ImGui::EndTabItem();}
        if(tab==12&&ImGui::BeginTabItem("Water",nullptr,requested_tab==12?ImGuiTabItemFlags_SetSelected:0)){tab=12;
            ImGui::TextWrapped("Create real water and its terrain together in Landscape. The preview follows your mouse; click clear ground to place.");
            if(ImGui::Button("Create pond"))landscape_choose_water(0,true);
            ImGui::SameLine();
            if(ImGui::Button("Create lake"))landscape_choose_water(1,true);
            ImGui::SameLine();
            if(ImGui::Button("Draw river"))landscape_choose_water(2,true);
            ImGui::Separator();ImGui::TextUnformatted("Existing water appearance");
            const bool water_full=draft.waters.size()>=8;
            ImGui::Text("Water objects: %zu / 8",draft.waters.size());
            if(water_full)ImGui::TextWrapped("Water limit reached. Undo a placement or remove a water edit below before adding another pond or lake. Rivers use the separate road / river path budget.");
            for(unsigned kind=0;kind<3;kind++){
                if(kind)ImGui::SameLine();
                auto at=std::find_if(draft.waters.begin(),draft.waters.end(),[&](const auto &w){return w.source==kind;});
                ImGui::BeginDisabled(at==draft.waters.end()&&water_full);
                if(ImGui::Button(kind==0?"Ocean":kind==1?"Forest pool":"Castle moat")){
                    if(at==draft.waters.end()){checkpoint();world_studio::Water w;w.source=kind;draft.waters.push_back(w);water_selection=int(draft.waters.size()-1);changed(true);}
                    else water_selection=int(at-draft.waters.begin());
                }ImGui::EndDisabled();
            }
            for(unsigned i=0;i<draft.waters.size();i++)if(ImGui::Selectable(("Water "+std::to_string(i+1)+(draft.waters[i].source==3?" pond":" fixed shore")).c_str(),water_selection==int(i))){if(water_selection!=int(i)){water_move_armed=false;water_geometry_index=-1;}water_selection=int(i);}
            if(water_selection>=0&&unsigned(water_selection)<draft.waters.size()){
                const unsigned index=unsigned(water_selection);
                ImGui::BeginDisabled(!world_entity::editable(draft.entities,{world_entity::Kind::Water,index})||apply||production_freeze());
                auto &w=draft.waters[index];const bool derived=w.source==3&&w.basin_depth>0;
                bool enabled=w.enabled;
                if(ImGui::Checkbox("Water visible",&enabled)){
                    if(derived){auto next=draft;next.waters[index].enabled=enabled;water_publish_derived(std::move(next),index);}
                    else{w.enabled=enabled;runtime_edit_changed(true);}
                }
                // A successful candidate replaces the vector; use a fresh
                // reference for subsequent appearance widgets.
                auto &current=draft.waters[index];
                if(current.source==3){
                    ImGui::Text("Basin: E%.1f N%.1f | %.0f x %.0f m",current.x,-current.z,current.radius_x*2,current.radius_z*2);
                    if(ImGui::Button("Frame selected pond",{-1,28}))water_frame_selected();
                    {const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();water_frame_button={(a.x+b.x)*.5f,(a.y+b.y)*.5f};}
                    if(derived){
                        if(water_geometry_index!=water_selection||water_geometry_revision!=revision){water_geometry_index=water_selection;water_geometry_revision=revision;water_geometry_radius=current.radius_x;water_geometry_depth=current.basin_depth;}
                        float diameter=water_geometry_radius*2;
                        if(ImGui::SliderFloat("Editable basin diameter",&diameter,12,64,"%.0f m"))water_geometry_radius=diameter*.5f;
                        {const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();water_geometry_radius_slider={a.x+ImGui::CalcItemWidth()*.5f,(a.y+b.y)*.5f};}
                        ImGui::SliderFloat("Editable basin depth",&water_geometry_depth,.5,6,"%.1f m");
                        {const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();water_geometry_depth_slider={a.x+ImGui::CalcItemWidth()*.5f,(a.y+b.y)*.5f};}
                        if(ImGui::Button("Apply basin size",{-1,30}))water_change_geometry(index,current.x,current.z,water_geometry_radius,water_geometry_depth);
                        {const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();water_geometry_button={(a.x+b.x)*.5f,(a.y+b.y)*.5f};}
                        if(ImGui::Button(water_move_armed?"Cancel pond move":"Pick new pond position",{-1,30})){
                            water_move_armed=!water_move_armed;landscape_water_armed=false;surface_point_move_armed=false;
                            if(water_move_armed){landscape_pond_source();message="Move the outline over clear terrain, then click to move the pond and basin. Esc cancels.";}
                        }
                        {const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();water_move_button={(a.x+b.x)*.5f,(a.y+b.y)*.5f};}
                        if(ImGui::Button("Move pond to picked target",{-1,0})&&target)water_change_geometry(index,x,z,water_geometry_radius,water_geometry_depth);
                        ImGui::TextWrapped("Moving, resizing, disabling or removing this pond restores its former ground. Each committed change has one Undo.");
                    }else ImGui::TextWrapped("Legacy baked basin: its sculpt is permanent terrain. To change position or size, Undo its original creation or recreate it on clear ground. Removing this water keeps that baked sculpt.");
                }else ImGui::TextWrapped("Existing shoreline geometry and level stay fixed.");
                auto &appearance=draft.waters[index];bool e=false;
                e|=ImGui::ColorEdit3("Water transmission tint",appearance.tint.data(),ImGuiColorEditFlags_Float);
                e|=ImGui::SliderFloat3("Absorption RGB",appearance.absorption.data(),.25,3);
                e|=ImGui::SliderFloat("Surface roughness",&appearance.roughness,.5,2);
                e|=ImGui::SliderFloat("Wave normal amplitude",&appearance.waves,0,2);
                e|=ImGui::SliderFloat("Flow speed",&appearance.flow,0,2);
                e|=ImGui::SliderFloat("Contact foam",&appearance.foam,0,2);if(e)runtime_edit_changed(true);
                if(ImGui::Button("Remove water edit")){
                    if(production_remove({world_entity::Kind::Water,index},true)){
                        water_selection=-1;water_move_armed=false;
                        if(derived){landscape_wait_for_build();apply=true;message="Removing pond and restoring its original ground. Undo restores the entire pond.";}
                    }
                }
                ImGui::EndDisabled();
            }
            ImGui::TextWrapped("Ponds and lakes need a submerged center and closed dry banks. Shape changes validate before publication; Save stores the committed world.");ImGui::EndTabItem();
        }
        (void)cam;
    }
    world_studio::Document runtime_before;bool runtime_editing=false;
    void runtime_edit_changed(bool geometry=false){if(!runtime_editing)history.checkpoint(runtime_before);runtime_editing=true;changed(geometry);}
    void edit_particle_source(int id){
        auto at=std::find_if(draft.particles.begin(),draft.particles.end(),[&](const auto &p){return p.source==id;});if(at!=draft.particles.end()){particle_selection=int(at-draft.particles.begin());return;}if(draft.particles.size()==16)return;world_studio::Particle p;p.source=id;
        if(id<64){auto e=base_particles[unsigned(id)];p.x=e.position[0];p.y=e.position[1];p.z=e.position[2];p.kind=e.kind==WorldSceneParticleKind::Smoke?0:1;p.radius=e.radius;p.height=e.height;p.power=e.strength;p.warmth=e.warmth;p.seed=e.seed;p.ceiling=e.ceiling_y;p.rate=e.rate;p.lifetime=e.lifetime;p.size=e.size;p.wind=e.wind;p.tint={e.tint[0],e.tint[1],e.tint[2]};p.glow=e.glow;}
        else{auto e=base_flames[unsigned(id-64)];p.x=e.x;p.y=e.y;p.z=e.z;p.kind=2;p.radius=std::max(.12f,e.width);p.height=std::max(.15f,e.height);p.power=e.intensity;p.seed=e.seed;p.ceiling=e.y+3.2*p.height+.1;}
        checkpoint();draft.particles.push_back(p);particle_selection=int(draft.particles.size()-1);changed();
    }
    void runtime_overlay(const Camera &cam){
        npc_input(cam);
        if(!open||tab<10||tab>12){water_move_armed=false;return;}
        auto *draw=ImGui::GetBackgroundDrawList();const auto color=IM_COL32(96,221,184,230);
        auto line=[&](world_navigation::Point a,world_navigation::Point b){ImVec2 p,q;if(project_point(cam,{a.x,a.y,a.z},p)&&project_point(cam,{b.x,b.y,b.z},q))draw->AddLine(p,q,color,2);};
        if(tab==12&&water_move_armed&&water_selection>=0&&unsigned(water_selection)<draft.waters.size()){
            if(ImGui::IsKeyPressed(ImGuiKey_Escape)){water_move_armed=false;message="Pond move cancelled; its basin is unchanged.";return;}
            auto &io=ImGui::GetIO();const auto viewport=designer_viewport(io.DisplaySize.x,io.DisplaySize.y);
            if(viewport.contains(io.MousePos.x,io.MousePos.y)&&!io.WantCaptureMouse&&!io.MouseDown[1]&&!drag.active&&!designer_blocks_viewport()){
                target=false;pick(cam,io.MousePos.x,io.MousePos.y);
                if(target){
                    const double clearance=water_geometry_radius+std::max(4.,double(water_geometry_radius)*.25)+2.9;std::string error;
                    const bool allowed=landscape_water_footprint(x,z,clearance,error,water_selection);
                    const auto tint=allowed?IM_COL32(100,230,195,240):IM_COL32(255,150,80,240);
                    const double level=world_atlas_base_height(x,z)+world_studio::height_delta(draft,x,z)-.3;
                    for(double radius:{double(water_geometry_radius),clearance})for(unsigned i=0;i<64;++i){
                        const double a=i*6.283185307179586/64,b=(i+1)*6.283185307179586/64;ImVec2 p,q;
                        if(project_point(cam,{x+std::cos(a)*radius,level+.1,z+std::sin(a)*radius},p)&&project_point(cam,{x+std::cos(b)*radius,level+.1,z+std::sin(b)*radius},q))draw->AddLine(p,q,tint,2);
                    }
                    draw->AddText({io.MousePos.x+16,io.MousePos.y+18},tint,allowed?"Click to move pond and basin | Esc cancels":"Blocked footprint: choose clear terrain");
                    if(ImGui::IsMouseClicked(0)){
                        if(allowed)water_change_geometry(unsigned(water_selection),x,z,water_geometry_radius,water_geometry_depth);
                        else{++water_geometry_rejections;landscape_creation_failure("Pond unchanged: "+error);}
                        return;
                    }
                }
            }
        }
        if(tab==10){
            if(actor_selection>=0&&unsigned(actor_selection)<draft.actors.size()){
                const auto &a=draft.actors[unsigned(actor_selection)];world_navigation::Point previous;
                for(unsigned i=0;i<a.points.size();i++){auto p=a.points[i];world_navigation::Point v{p[0],(a.floor_heights.empty()?world_atlas_walk_height(p[0],p[1]):a.floor_heights[i])+.12,p[1]};ImVec2 at;if(project_point(cam,{v.x,v.y,v.z},at)){draw->AddCircleFilled(at,6,IM_COL32(255,209,90,255));draw->AddText({at.x+8,at.y},color,std::to_string(i+1).c_str());}if(i)line(previous,v);previous=v;}
                if(a.loop&&a.points.size()>2)line(previous,{a.points.front()[0],(a.floor_heights.empty()?world_atlas_walk_height(a.points.front()[0],a.points.front()[1]):a.floor_heights.front())+.12,a.points.front()[1]});
                static std::uint64_t route_revision=~std::uint64_t(0);static int route_actor=-1;
                if(route_revision!=revision||route_actor!=actor_selection){std::string ignored;if(!npc_route(draft,unsigned(actor_selection),npc_preview_route,ignored))npc_preview_route.clear();route_revision=revision;route_actor=actor_selection;}
                for(unsigned i=1;i<npc_preview_route.size();i++){auto a=npc_preview_route[i-1],b=npc_preview_route[i];a.y+=.19;b.y+=.19;ImVec2 p,q;if(project_point(cam,{a.x,a.y,a.z},p)&&project_point(cam,{b.x,b.y,b.z},q))draw->AddLine(p,q,IM_COL32(70,235,255,255),3);}
            }
            auto &g=navigation_preview;unsigned stride=std::max(1u,unsigned(g.cells.size()/900));for(unsigned i=0;i<g.cells.size();i+=stride){const auto &c=g.cells[i];if(!c.open)continue;ImVec2 p;if(project_point(cam,{g.area.x-g.side*g.area.cell*.5+(i%g.side+.5)*g.area.cell,c.y+.08,g.area.z-g.side*g.area.cell*.5+(i/g.side+.5)*g.area.cell},p))draw->AddCircleFilled(p,1.5,IM_COL32(62,211,126,130));}
        }
        if(tab==11&&particle_selection>=0&&unsigned(particle_selection)<draft.particles.size()){auto &p=draft.particles[unsigned(particle_selection)];for(unsigned i=0;i<32;i++){double a=i*6.283185307/32,b=(i+1)*6.283185307/32;line({p.x+std::cos(a)*p.radius,p.y,p.z+std::sin(a)*p.radius},{p.x+std::cos(b)*p.radius,p.y,p.z+std::sin(b)*p.radius});}line({p.x,p.y,p.z},{p.x,p.kind==2?p.y+p.height*3.2:std::min(p.ceiling,p.y+p.height),p.z});}
        if(tab==12&&water_selection>=0&&unsigned(water_selection)<draft.waters.size()){const auto &w=draft.waters[unsigned(water_selection)];if(w.source==3)for(unsigned i=0;i<48;i++){double a=i*6.283185307/48,b=(i+1)*6.283185307/48;line({w.x+std::cos(a)*w.radius_x,w.level,w.z+std::sin(a)*w.radius_z},{w.x+std::cos(b)*w.radius_x,w.level,w.z+std::sin(b)*w.radius_z});}}
    }

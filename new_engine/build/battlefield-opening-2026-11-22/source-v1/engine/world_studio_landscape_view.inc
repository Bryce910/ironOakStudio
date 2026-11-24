// Beginner landscape workspace. Included inside Studio; edits remain drafts
// until one mouse-release transaction publishes them through normal Apply.
    int landscape_page=0,landscape_shape=0,landscape_plant=0,landscape_area=-1;
    float landscape_radius=48,landscape_strength=1.5f,landscape_density=.7f;
    unsigned landscape_layer_mask=15,landscape_layer_commits=0,landscape_area_updates=0;
    float landscape_max_slope=50;
    float landscape_species_scale[4]={1,1,1,1};
    std::array<ImVec2,4> landscape_layer_buttons={};ImVec2 landscape_area_update_button;
    int landscape_area_edit_index=-1;std::uint64_t landscape_area_edit_revision=~std::uint64_t(0);
    world_studio::Ecology landscape_area_edit;unsigned landscape_area_edit_mask=15;
    float landscape_river_width=8,landscape_river_bank=4,landscape_river_depth=1.2f;
    int landscape_water_kind=2; // Existing River automation keeps page 2 / kind 2.
    float landscape_water_radius=12,landscape_water_depth=1.f;
    bool landscape_water_armed=true,landscape_water_hover_blocked=true;
    unsigned landscape_ponds_created=0;
    double landscape_water_hover_x=1e30,landscape_water_hover_z=1e30;
    float landscape_water_hover_radius=-1;
    std::uint64_t landscape_water_hover_revision=~std::uint64_t(0);
    std::string landscape_water_hover_error;
    unsigned landscape_seed=1,landscape_committed_strokes=0,landscape_cancelled_strokes=0;
    unsigned landscape_brush_samples=0,landscape_rivers_created=0;
    bool landscape_advanced=false,landscape_painting=false,landscape_stroke_changed=false;
    bool landscape_before_dirty=false,landscape_before_rebuild=false,landscape_before_saved=false;
    double landscape_last_x=0,landscape_last_z=0,landscape_flatten_height=0;
    double landscape_next_shape_stamp=0;
    std::uint64_t landscape_hover_revision=~std::uint64_t(0);
    bool landscape_build_pending=false;
    unsigned landscape_build_applied=0,landscape_build_rejected=0,landscape_build_undos=0;
    std::uint64_t landscape_build_error_revision=0;
    std::string landscape_build_error;
    world_studio::Document landscape_before;
    std::vector<world_terrain_authoring::Point> landscape_river_points;
    std::array<ImVec2,20> landscape_buttons={};ImVec2 landscape_open_screen;
    struct LandscapeCoverage {
        WorldHeightRegion data={};
        ~LandscapeCoverage(){world_height_region_clear(&data);}
    } landscape_city_ground,landscape_countryside_ground;
    bool landscape_coverage_ready=false,landscape_coverage_error=false;
    std::vector<AdvancedFoundation> landscape_live_foundations;
    std::vector<AdvancedProtectedPath> landscape_live_paths;
    void landscape_load_coverage(){
        if(landscape_coverage_ready)return;
        char error[256]={};
        landscape_coverage_error=world_height_region_load(&landscape_city_ground.data,"ground/city.coverage",error,sizeof error)<0;
        landscape_coverage_error|=world_height_region_load(&landscape_countryside_ground.data,"ground/countryside.coverage",error,sizeof error)<0;
        landscape_coverage_ready=true;
    }
    bool landscape_imported_ground(double px,double pz,bool countryside=true)const{
        if(landscape_coverage_error)return true;
        float covered=0;
        if(world_height_region_sample(&landscape_city_ground.data,px,pz,&covered)&&covered>.01f)return true;
        return countryside&&world_height_region_sample(&landscape_countryside_ground.data,px,pz,&covered)&&covered>.01f;
    }
    world_landscape_protection::Roads landscape_draft_roads;
    bool landscape_draft_roads_ok=true;
    bool landscape_solid_placement(const world_studio::Document &doc,const std::string &alias)const{
        for(const auto &asset:doc.assets)if(asset.alias==alias)return asset.collision!=0&&asset.vegetation==0;
        for(size_t i=0;i<world_scenery_alias_count();++i){const auto asset=world_scenery_alias(i);if(alias==asset.name)return asset.collision!=0&&!asset.tree&&asset.vegetation==0;}
        return false;
    }
    void landscape_collect_foundations(const world_studio::Document &doc,std::vector<AdvancedFoundation> &out)const{
        out.clear();
        auto append=[&](const world_studio::Pose &p,double margin){const auto f=world_landscape_protection::footprint(p,margin);out.push_back({f.x,f.z,f.radius});};
        for(unsigned i=0;i<source_poses.size();++i)if(source[i].foundation){world_studio::Pose p;if(pose(i,p,&doc)&&!p.hidden)append(p,3);}
        for(unsigned i=0;i<doc.placements.size();++i)if(landscape_solid_placement(doc,doc.placements[i].alias)){world_studio::Pose p;if(pose(WORLD_ADDED_KEY_BASE+i,p,&doc)&&!p.hidden)append(p,2);}
    }
    void landscape_refresh_protection(){
        landscape_load_coverage();landscape_live_foundations.clear();landscape_live_paths.clear();
        landscape_collect_foundations(live,landscape_live_foundations);
        auto segment=[&](double ax,double az,double bx,double bz){const double dx=bx-ax,dz=bz-az;landscape_live_paths.push_back({ax,az,dx,dz,dx*dx+dz*dz,std::min(ax,bx)-4,std::max(ax,bx)+4,std::min(az,bz)-4,std::max(az,bz)+4});};
        auto route=[&](const WorldAtlasTrailPoint *p,size_t count){for(size_t i=1;i<count;++i)segment(p[i-1].x,p[i-1].z,p[i].x,p[i].z);};
        size_t count=0;auto points=world_atlas_rainforest_trail_points(&count);route(points,count);points=world_atlas_trail(&count);route(points,count);
        for(const auto &actor:live.actors){for(size_t i=1;i<actor.points.size();++i)segment(actor.points[i-1][0],actor.points[i-1][1],actor.points[i][0],actor.points[i][1]);if(actor.points.size()==1)landscape_live_foundations.push_back({actor.points[0][0],actor.points[0][1],2});if(actor.loop&&actor.points.size()>2)segment(actor.points.back()[0],actor.points.back()[1],actor.points.front()[0],actor.points.front()[1]);}
    }
    static bool landscape_live_protected(int kind,double px,double pz,const void *data){
        const auto &s=*static_cast<const Studio*>(data);(void)kind;
        if(s.landscape_imported_ground(px,pz,false)||world_atlas_village_weight(px,pz)>.01f||world_atlas_bandit_sample(px,pz).clearing>.01f||std::hypot(px-WORLD_TRAIL_CAMP_X,pz-WORLD_TRAIL_CAMP_Z)<38)return true;
        if(world_atlas_local_water(px,pz,nullptr,nullptr))return true;
        if(s.live.surface_field&&world_surface::road_mask(*s.live.surface_field,px,pz)>.01f)return true;
        for(const auto &f:s.landscape_live_foundations){const double dx=px-f.x,dz=pz-f.z;if(std::abs(dx)<f.radius&&std::abs(dz)<f.radius&&dx*dx+dz*dz<f.radius*f.radius)return true;}
        for(const auto &p:s.landscape_live_paths){if(px<p.minx||px>p.maxx||pz<p.minz||pz>p.maxz)continue;const double t=p.length2>1e-8?std::clamp(((px-p.x)*p.dx+(pz-p.z)*p.dz)/p.length2,0.,1.):0;if(std::hypot(px-p.x-p.dx*t,pz-p.z-p.dz*t)<4)return true;}
        return false;
    }
    static bool landscape_terrain_protected(double px,double pz,const void *data){
        const auto &s=*static_cast<const Studio*>(data);
        return s.landscape_imported_ground(px,pz)||advanced_protected(px,pz,data);
    }
    static bool landscape_fixed_ground_protected(double px,double pz,const void *data){
        // Custom water is deliberately excluded: replay validates overlap
        // from its temporary water list. Fixed shores remain protected.
        const auto &s=*static_cast<const Studio*>(data);
        if(s.landscape_imported_ground(px,pz)||!s.landscape_river_roots_ok||s.landscape_river_roots.blocked(px,pz))return true;
        if(world_atlas_village_weight(px,pz)>.01f||world_atlas_bandit_sample(px,pz).clearing>.01f||std::hypot(px-WORLD_TRAIL_CAMP_X,pz-WORLD_TRAIL_CAMP_Z)<38)return true;
        if(world_atlas_base_height(px,pz)+world_studio::height_delta(s.draft,px,pz)<1)return true;
        const auto *pool=world_atlas_rainforest_pool();
        if(pool){const double dx=(px-pool->x)/pool->radius_x,dz=(pz-pool->z)/pool->radius_z;if(dx*dx+dz*dz<1)return true;}
        for(const auto &f:s.advanced_foundations){const double dx=px-f.x,dz=pz-f.z;if(std::abs(dx)<=f.radius&&std::abs(dz)<=f.radius&&dx*dx+dz*dz<f.radius*f.radius)return true;}
        for(const auto &p:s.advanced_paths){if(px<p.minx||px>p.maxx||pz<p.minz||pz>p.maxz)continue;const double t=p.length2>1e-8?std::clamp(((px-p.x)*p.dx+(pz-p.z)*p.dz)/p.length2,0.,1.):0;if(std::hypot(px-p.x-p.dx*t,pz-p.z-p.dz*t)<4)return true;}
        return false;
    }
    static bool landscape_pond_static_protected(double px,double pz,const void *data){
        const auto &s=*static_cast<const Studio*>(data);
        return !s.landscape_draft_roads_ok||s.landscape_draft_roads.blocked(px,pz)||landscape_fixed_ground_protected(px,pz,data);
    }
    world_terrain_authoring::Source landscape_pond_source(){
        landscape_river_source();return {world_atlas_base_height,landscape_pond_static_protected,this};
    }
    world_terrain_authoring::Source landscape_source(){landscape_load_coverage();advanced_prepare_protection();return {world_atlas_base_height,landscape_terrain_protected,this};}
    struct LandscapeGroundFeedback {const char *title,*detail;bool blocked;};
    LandscapeGroundFeedback landscape_ground_feedback()const{
        if(landscape_page==1&&!landscape_layer_mask)return {"CHOOSE A PLANT LAYER","Select at least one layer below. Unchecked layers stay unchanged.",true};
        if(!target)return {"NO GROUND UNDER THE BRUSH","Point down at nearby ground, or use Find open ground above.",true};
        if(landscape_coverage_error)return {"GROUND DATA NEEDS ATTENTION","Terrain protection data could not be read. Reload the project before shaping.",true};
        float covered=0;
        if(world_height_region_sample(&landscape_city_ground.data,x,z,&covered)&&covered>.01f)
            return {"PROTECTED: IMPORTED CITY FLOOR","This floor is part of an imported model. Use Find open ground to shape the landscape.",true};
        if(landscape_page!=1&&world_height_region_sample(&landscape_countryside_ground.data,x,z,&covered)&&covered>.01f)
            return {"PROTECTED: IMPORTED COUNTRYSIDE","This ground is part of an imported model. Use Find open ground to shape editable terrain.",true};
        if(landscape_page==2){
            if(landscape_water_kind!=2&&draft.waters.size()>=8)return {"WATER LIMIT REACHED","All 8 water objects are used. Undo a placement or remove a water edit in Water before creating another pond or lake.",true};
            if(landscape_water_kind==2&&draft.surfaces.ribbons.size()>=world_surface::ribbon_limit)return {"PATH LIMIT REACHED","All 16 road / river paths are used. Remove a path before drawing another river.",true};
            if(!landscape_water_armed)return {"WATER PLACEMENT CANCELLED","Select Pond, Lake or River below to start again. Your existing landscape is unchanged.",true};
            if(landscape_water_hover_blocked)return {"WATER FOOTPRINT BLOCKED",landscape_water_hover_error.empty()?"Move the entire footprint onto clear terrain, or use Find open ground.":landscape_water_hover_error.c_str(),true};
            return {landscape_water_kind==2?"READY TO DRAW RIVER":landscape_water_kind==0?"READY TO CREATE POND":"READY TO CREATE LAKE",landscape_water_kind==2?"Click ground points, then Finish river. The route is ordered downhill automatically.":"Click once inside the scene. The basin, dry banks and water build together with one Undo.",false};
        }
        const bool blocked=landscape_page==1?landscape_live_protected(0,x,z,this):landscape_terrain_protected(x,z,this);
        if(blocked)return {"PROTECTED GROUND","Buildings, roads, water and travel routes stay fixed. Use Find open ground for a clear workspace.",true};
        if(landscape_painting&&landscape_page==0)return {"SHAPING: RELEASE TO BUILD","The wire grid previews your draft. Keep holding to shape; release to build. Esc cancels this stroke.",false};
        if(landscape_page==0)return {"READY TO SHAPE","Hold left mouse to shape, even without moving. Release to build; Esc cancels the whole stroke.",false};
        return {"EDITABLE GROUND",landscape_page==1?"Drag with left mouse to plant. Release to build.":"Click ground points along a downhill route, then Finish river.",false};
    }
    static double landscape_draft_height(const world_studio::Document &doc,double px,double pz){
        return world_atlas_base_height(px,pz)+world_studio::height_delta(doc,px,pz)+(doc.surface_field?world_surface::terrain_offset(*doc.surface_field,px,pz):0);
    }
    static double landscape_river_point_height(const world_studio::Document &doc,double px,double pz){
        float level=0,shore=0;
        // Pick rays hit the excavated bed. Tributaries join the visible water
        // surface instead of inheriting that bed depth and diving under it.
        if(doc.surface_field&&world_surface::river(*doc.surface_field,px,pz,level,shore))return level;
        return landscape_draft_height(doc,px,pz);
    }
    void landscape_clear_build_feedback(){
        landscape_build_pending=false;landscape_build_error.clear();
        landscape_build_applied=applied;landscape_build_rejected=rejected;landscape_build_undos=ui_undos;
        landscape_build_error_revision=revision;
    }
    void landscape_wait_for_build(){
        landscape_clear_build_feedback();landscape_build_pending=true;
    }
    void landscape_poll_build(){
        // Apply publishes its result after the editor panel. Keep a rejected
        // build's exact diagnostic before hover feedback replaces the footer.
        // A later edit/Undo or successful Apply invalidates an old diagnostic.
        if(!landscape_build_error.empty()&&(revision!=landscape_build_error_revision||ui_undos!=landscape_build_undos||applied>landscape_build_applied))landscape_build_error.clear();
        if(!landscape_build_pending)return;
        if(ui_undos!=landscape_build_undos){landscape_clear_build_feedback();return;}
        if(rejected>landscape_build_rejected){
            landscape_build_error=message.empty()?"The scene rejected this landscape build. Fix invalid scene objects or Undo this stroke, then Apply again.":message;
            landscape_build_error_revision=revision;landscape_build_pending=false;
        }else if(applied>landscape_build_applied)landscape_clear_build_feedback();
    }
    world_landscape_protection::DryRoots landscape_river_roots;
    bool landscape_river_roots_ok=true;
    mutable bool landscape_river_root_obstructed=false;
    static bool landscape_river_protected(double px,double pz,const void *data){
        const auto &s=*static_cast<const Studio*>(data);
        if(!s.landscape_river_roots_ok)return true;
        if(s.landscape_river_roots.blocked(px,pz)){s.landscape_river_root_obstructed=true;return true;}
        // Existing authored rivers are deliberately joinable. Keep fixed
        // shores, roads, structures, NPC/travel routes and ponds protected.
        if(!s.landscape_draft_roads_ok||s.landscape_draft_roads.blocked(px,pz)||landscape_fixed_ground_protected(px,pz,data))return true;
        for(const auto &w:s.draft.waters)if(w.source==3&&w.enabled){const double dx=(px-w.x)/(w.radius_x+2),dz=(pz-w.z)/(w.radius_z+2);if(dx*dx+dz*dz<1)return true;}
        return false;
    }
    world_terrain_authoring::Source landscape_river_source(){
        landscape_source();landscape_river_root_obstructed=false;
        landscape_river_roots_ok=landscape_river_roots.prepare(draft,source_poses,message);
        return {world_atlas_base_height,landscape_river_protected,this};
    }
    void landscape_cancel(){
        water_move_armed=false;surface_point_move_armed=false;
        landscape_water_armed=false;landscape_water_hover_revision=~std::uint64_t(0);
        if(landscape_painting){draft=std::move(landscape_before);dirty=landscape_before_dirty;rebuild=landscape_before_rebuild;production_project_saved=landscape_before_saved;landscape_painting=landscape_stroke_changed=false;++revision;++landscape_cancelled_strokes;message="Landscape stroke cancelled. Your previous landscape is preserved.";}
        landscape_river_points.clear();
    }
    bool landscape_begin(){
        if(landscape_painting||production_freeze()||apply)return false;
        if(importing.valid()||environment_import.valid()||surface_import.valid()){message="Let the current asset import finish before painting.";return false;}
        landscape_clear_build_feedback();
        designer_finish_edit();cancel_designer_drag();clear_selection();
        landscape_before=draft;landscape_before_dirty=dirty;landscape_before_rebuild=rebuild;landscape_before_saved=production_project_saved;
        landscape_painting=true;landscape_stroke_changed=false;landscape_last_x=x;landscape_last_z=z;landscape_flatten_height=y;landscape_next_shape_stamp=production_now();return true;
    }
    bool landscape_stamp(){
        if(!landscape_painting||!target)return false;
        world_terrain_authoring::Report report;std::string error;
        if(landscape_page==0){
            // A held brush advances on a clock, never once per rendered frame.
            // Schedule from now so a slow build/focus pause cannot queue a burst
            // of old stamps. Plant tools retain their spatial spacing below.
            landscape_next_shape_stamp=production_now()+.12;
            if(!world_terrain_authoring::simple_brush(draft,x,z,landscape_radius,landscape_strength,world_terrain_authoring::SimpleBrushMode(landscape_shape),landscape_flatten_height,landscape_seed,landscape_source(),report,error)){
                landscape_cancel();message="Landscape unchanged: "+error;return false;
            }
            if(!report.changed_nodes){message=report.protected_nodes?"Buildings, roads and imported ground are protected. Use Find open ground.":"No height change at this strength.";return true;}
        }else{
            if(!landscape_layer_mask){message="Select at least one plant layer.";return false;}
            if(landscape_live_protected(0,x,z,this)){message="This spot is protected. Paint on clear ground outside buildings and paths.";return true;}
            auto next=draft;world_studio::Ecology region;region.x=x;region.z=z;region.radius=landscape_radius;region.seed=landscape_seed;
            const float d=landscape_density;
            const float mixes[8][4]={{.85f,.7f,.1f,.45f},{1,0,.03f,.1f},{.65f,.9f,.1f,1},{.12f,1,0,.15f},{1,0,0,0},{.18f,0,0,1},{.1f,0,1,0},{0,0,0,0}};
            for(unsigned i=0;i<4;++i)region.density[i]=std::clamp(mixes[landscape_plant][i]*d*landscape_species_scale[i],0.f,1.f);
            region.preset=landscape_plant==2?2u:landscape_plant==1||landscape_plant==4?0u:1u;
            if(landscape_layer_mask!=15)region.preset|=landscape_layer_mask<<8;
            region.max_slope=landscape_max_slope;
            if(!world_ecology::valid(region)){landscape_cancel();message="Choose ground away from the map edge.";return false;}
            if(next.ecology.size()>=WORLD_COVER_REGION_LIMIT){landscape_cancel();message="Painted area limit reached. Remove an area below, or undo before painting more.";return false;}
            next.ecology.push_back(region);if(!world_studio::valid(next,error)){landscape_cancel();message="Landscape unchanged: "+error;return false;}
            draft=std::move(next);landscape_area=int(draft.ecology.size()-1);
        }
        landscape_stroke_changed=true;++landscape_brush_samples;changed(landscape_page==0);landscape_last_x=x;landscape_last_z=z;
        message=landscape_page==0?"Shaping landscape. Wire grid previews the draft; release to build, Esc cancels.":"Painting landscape. Release to build; Esc cancels the whole stroke.";return true;
    }
    void landscape_finish(){
        if(!landscape_painting)return;
        landscape_painting=false;
        if(!landscape_stroke_changed){draft=std::move(landscape_before);dirty=landscape_before_dirty;rebuild=landscape_before_rebuild;production_project_saved=landscape_before_saved;return;}
        history.checkpoint(landscape_before);landscape_before={};landscape_stroke_changed=false;++landscape_committed_strokes;
        if(landscape_page==1)++landscape_layer_commits;
        landscape_wait_for_build();apply=true;message="Building landscape. Undo restores the whole brush stroke.";
    }
    void landscape_creation_failure(const std::string &error){
        message=error;landscape_build_error=error;landscape_build_pending=false;
        landscape_build_error_revision=revision;landscape_build_applied=applied;landscape_build_undos=ui_undos;
    }
    double landscape_water_clearance()const{
        return landscape_water_kind==2?landscape_river_width*.5+landscape_river_bank+2.9:
            landscape_water_radius+std::max(4.f,landscape_water_radius*.25f)+2.9;
    }
    bool landscape_water_footprint(double px,double pz,double clearance,std::string &error,int ignored_water=-1)const{
        if(!landscape_river_roots_ok){error="Existing model protection could not be prepared. Fix the scene before creating water.";return false;}
        if(std::abs(px)+clearance>50390||std::abs(pz)+clearance>28390){error="The entire water footprint must stay inside the editable world.";return false;}
        // Roots need a full footprint test, not only the center ray. A circle
        // overlap is conservative and avoids repeating the root scan per node.
        if(landscape_river_roots.overlaps(px,pz,clearance)){error="An existing tree, plant or object is inside the water or banks. Move the footprint, or move that object first.";return false;}
        for(const auto &water:draft.waters)if(water.source==3){
            if(int(&water-draft.waters.data())==ignored_water)continue;
            const double dx=(px-water.x)/(clearance+water.radius_x+2),dz=(pz-water.z)/(clearance+water.radius_z+2);
            if(dx*dx+dz*dz<1){error="This footprint overlaps an existing pond or lake. Move farther away, or Undo / remove the earlier water edit.";return false;}
        }
        for(const auto &ribbon:draft.surfaces.ribbons){
            if(landscape_water_kind==2&&ribbon.kind==world_surface::RibbonKind::River)continue;
            world_surface::BakedRibbon path;
            if(!world_surface::bake_ribbon(ribbon,path,error))return false;
            // nearest() intentionally returns an empty zero-distance hit
            // outside its tight bounds; it cannot test an expanded footprint.
            const double exclusion=clearance+ribbon.width*.5+ribbon.shoulder+2.9;
            for(unsigned i=1;i<path.samples.size();++i){
                const auto &a=path.samples[i-1].position,&b=path.samples[i].position;
                const double dx=b.x-a.x,dz=b.z-a.z,length2=dx*dx+dz*dz;
                const double t=length2>1e-12?std::clamp(((px-a.x)*dx+(pz-a.z)*dz)/length2,0.,1.):0;
                if(std::hypot(px-a.x-dx*t,pz-a.z-dz*t)<=exclusion){error="The water or its banks overlap an existing road or river. Move the footprint clear of the path.";return false;}
            }
        }
        // Match the global 2m sculpt lattice, with interpolation clearance.
        for(double zz=std::floor((pz-clearance)/2)*2;zz<=pz+clearance;zz+=2)
            for(double xx=std::floor((px-clearance)/2)*2;xx<=px+clearance;xx+=2)
                if(std::hypot(xx-px,zz-pz)<=clearance&&(ignored_water>=0?landscape_pond_static_protected(xx,zz,this):landscape_water_kind==2?landscape_river_protected(xx,zz,this):landscape_terrain_protected(xx,zz,this))){
                    error="The water or dry banks overlap a building, route, imported floor or existing water. Move the entire outline onto clear terrain.";return false;
                }
        error.clear();return true;
    }
    void landscape_water_hover(){
        // Cache on one-metre cells. Extra one-metre clearance covers the
        // maximum movement within a cached cell; placement rechecks exactly.
        const double px=std::round(x),pz=std::round(z),clearance=landscape_water_clearance();
        if(landscape_water_hover_revision==revision&&landscape_water_hover_x==px&&landscape_water_hover_z==pz&&landscape_water_hover_radius==float(clearance))return;
        landscape_water_hover_revision=revision;landscape_water_hover_x=px;landscape_water_hover_z=pz;landscape_water_hover_radius=float(clearance);
        landscape_water_hover_blocked=!landscape_water_footprint(px,pz,clearance+1,landscape_water_hover_error);
    }
    void landscape_choose_water(int kind,bool show_workspace=false){
        landscape_cancel();landscape_page=2;landscape_water_kind=kind;landscape_water_armed=true;
        if(kind!=2){landscape_water_radius=kind==0?12.f:24.f;landscape_water_depth=kind==0?1.f:3.f;}
        landscape_clear_build_feedback();
        if(show_workspace){designer_finish_edit();cancel_designer_drag();clear_selection();tab=requested_tab=15;designer_tools=true;designer_immersive=false;landscape_advanced=false;target=false;}
        message=kind==2?"Click terrain points, then Finish river. Esc cancels.":"Move over clear terrain and click once to carve the basin and fill it with water. Esc cancels.";
    }
    bool landscape_place_pond(){
        if(!target||!landscape_water_armed||production_freeze()||apply)return false;
        if(draft.waters.size()>=8){landscape_creation_failure("Water unchanged: all 8 water objects are used. Undo or remove a water edit first.");return false;}
        if(importing.valid()||environment_import.valid()||surface_import.valid()){landscape_creation_failure("Let the current asset import finish before creating water.");return false;}
        auto source=landscape_pond_source();std::string error;
        if(!landscape_water_footprint(x,z,landscape_water_clearance(),error)){landscape_creation_failure("Water unchanged: "+error);return false;}
        auto next=draft;world_terrain_authoring::Report report;
        if(!world_pond_field::create(next,x,z,landscape_water_radius,landscape_water_depth,source,report,error)){
            landscape_creation_failure("Water unchanged: "+error);return false;
        }
        designer_finish_edit();cancel_designer_drag();clear_selection();checkpoint();draft=std::move(next);water_selection=int(draft.waters.size()-1);
        changed(true);landscape_wait_for_build();apply=true;++landscape_ponds_created;
        message=landscape_water_kind==0?"Building pond, basin and dry banks. Undo removes the whole placement.":"Building lake, basin and dry banks. Undo removes the whole placement.";return true;
    }
    bool landscape_finish_river(){
        if(landscape_river_points.size()<2){landscape_creation_failure("Click at least two ground points, then Finish river.");return false;}
        if(production_freeze()||apply)return false;
        if(importing.valid()||environment_import.valid()||surface_import.valid()){landscape_creation_failure("Let the current asset import finish before creating a river.");return false;}
        auto next=draft;world_terrain_authoring::Report report;std::string error;
        auto route=landscape_river_points;
        for(auto &point:route)point.height=landscape_river_point_height(draft,point.x,point.z);
        const bool reversed=route.front().height<route.back().height;if(reversed)std::reverse(route.begin(),route.end());
        auto source=landscape_river_source();
        if(!landscape_river_roots_ok){landscape_creation_failure("River unchanged: existing model protection could not be prepared. Your drawn points are preserved.");return false;}
        if(!world_terrain_authoring::simple_river(next,route,landscape_river_width,landscape_river_bank,landscape_river_depth,source,report,error)){
            landscape_creation_failure(landscape_river_root_obstructed?"River unchanged: an existing tree, plant or object is in the river or its banks. Move the route, or move the object first. Your drawn points are preserved.":"River unchanged: "+error+". Your drawn points are preserved; Undo last point or Cancel river to correct the route.");return false;
        }
        designer_finish_edit();cancel_designer_drag();clear_selection();checkpoint();draft=std::move(next);changed(true);landscape_wait_for_build();apply=true;surface_ribbon=int(draft.surfaces.ribbons.size()-1);landscape_river_points.clear();++landscape_rivers_created;
        message=reversed?"River route reversed downhill. Building channel, banks and water; one Undo removes it.":"Building river, banks and flowing water. Undo removes the whole river.";return true;
    }
    void landscape_find_open(const Camera &cam){
        // Prefer a clear beginner workspace suitable for both sculpting and a
        // later river, rather than framing decorative roots as empty ground.
        auto protected_source=landscape_river_source();
        const double workspace_radius=landscape_page==2?std::max(48.,landscape_water_clearance()):double(landscape_radius);
        for(double distance:{128.,256.,384.,512.,768.,1024.,1536.,2048.,3072.,4096.})for(unsigned direction=0;direction<16;++direction){
            const double angle=direction*6.283185307179586/16;
            const double px=cam.x+std::cos(angle)*distance,pz=cam.z+std::sin(angle)*distance;
            if(std::abs(px)+workspace_radius>50390||std::abs(pz)+workspace_radius>28390)continue;
            if(!landscape_river_roots_ok||landscape_river_roots.overlaps(px,pz,workspace_radius))continue;
            bool good=true;for(unsigned i=0;i<9;++i){const double a=i*6.283185307179586/8;const double xx=px+(i?std::cos(a)*workspace_radius:0),zz=pz+(i?std::sin(a)*workspace_radius:0);if(protected_source.protected_point(xx,zz,protected_source.context)){good=false;break;}}
            if(!good)continue;
            std::string footprint_error;if(landscape_page==2&&!landscape_water_footprint(px,pz,workspace_radius,footprint_error))continue;
            x=px;z=pz;y=world_atlas_height(x,z);target=true;designer_frame_center={x,y,z};designer_frame_radius=std::max(32.,workspace_radius*.8);designer_frame_heading=0;designer_frame_reveal=true;designer_frame_pending=true;
            if(landscape_page==2){landscape_water_armed=true;landscape_water_hover_revision=~std::uint64_t(0);landscape_water_hover();}
            message=landscape_page==2?"Clear landscape framed. Choose Pond / Lake and click once, or draw River points.":"Open landscape framed. Choose a tool and drag on the ground.";return;
        }
        message="No clear landscape nearby. Fly beyond the settlement, point down, then paint.";
    }
    void landscape_button_record(unsigned id){const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();landscape_buttons[id]={(a.x+b.x)*.5f,(a.y+b.y)*.5f};}
    void landscape_panel(const Camera &cam){
        landscape_poll_build();
        ImGui::TextUnformatted("BUILD LANDSCAPE");
        ImGui::TextWrapped("Choose a tool, then paint on the ground. Each stroke has one Undo.");
        if(ImGui::Button("Find open ground",{-1,30}))landscape_find_open(cam);landscape_button_record(0);
        if(!landscape_build_error.empty()){
            const float wrap=std::max(80.f,ImGui::GetContentRegionAvail().x-ImGui::GetStyle().WindowPadding.x*2-2);
            const float height=std::clamp(ImGui::CalcTextSize(landscape_build_error.c_str(),nullptr,false,wrap).y+110.f,110.f,240.f);
            ImGui::PushStyleColor(ImGuiCol_ChildBg,{.32f,.08f,.07f,.96f});
            if(ImGui::BeginChild("Landscape build failure",{0,height},true)){
                ImGui::PushTextWrapPos(0);ImGui::TextColored({1,.66f,.53f,1},"LANDSCAPE COULD NOT BUILD");
                ImGui::TextWrapped("%s",landscape_build_error.c_str());
                ImGui::TextWrapped("Rejected changes do not reach the world. Correct the spot / route or fix the scene object named above, then try again. Undo restores your previous draft.");ImGui::PopTextWrapPos();
            }ImGui::EndChild();ImGui::PopStyleColor();
        }
        const auto feedback=landscape_ground_feedback();
        const float feedback_width=std::max(80.f,ImGui::GetContentRegionAvail().x-ImGui::GetStyle().WindowPadding.x*2-2);
        const float feedback_height=ImGui::CalcTextSize(feedback.title,nullptr,false,feedback_width).y+ImGui::CalcTextSize(feedback.detail,nullptr,false,feedback_width).y+ImGui::GetStyle().WindowPadding.y*2+ImGui::GetStyle().ItemSpacing.y+8;
        ImGui::PushStyleColor(ImGuiCol_ChildBg,feedback.blocked?ImVec4{.25f,.12f,.08f,.9f}:ImVec4{.08f,.22f,.17f,.9f});
        if(ImGui::BeginChild("Landscape ground feedback",{0,feedback_height},true,ImGuiWindowFlags_NoScrollbar)){
            ImGui::PushTextWrapPos(0);
            ImGui::TextColored(feedback.blocked?ImVec4{1,.69f,.44f,1}:ImVec4{.58f,1,.77f,1},"%s",feedback.title);
            ImGui::TextWrapped("%s",feedback.detail);ImGui::PopTextWrapPos();
        }ImGui::EndChild();ImGui::PopStyleColor();
        const float third=std::max(40.f,(ImGui::GetContentRegionAvail().x-12)/3);
        const char *pages[]={"Shape","Plant","Water"};
        for(unsigned i=0;i<3;++i){if(i)ImGui::SameLine();const bool selected=landscape_page==int(i);if(selected)ImGui::PushStyleColor(ImGuiCol_Button,{.18f,.40f,.34f,1});if(ImGui::Button(pages[i],{third,36})){landscape_cancel();landscape_page=int(i);if(i==2)landscape_water_armed=true;}landscape_button_record(1+i);if(selected)ImGui::PopStyleColor();}
        ImGui::Separator();
        const float half=std::max(40.f,(ImGui::GetContentRegionAvail().x-8)/2);
        if(landscape_page!=2){
            const char *shapes[]={"Raise","Lower","Flatten","Smooth","Rolling hills"};
            const char *plants[]={"Forest","Meadow","Jungle","Trees","Grass","Plants","Rocks","Clear growth"};
            const unsigned count=landscape_page==0?5:8;int &selected_tool=landscape_page==0?landscape_shape:landscape_plant;
            for(unsigned i=0;i<count;++i){if(i%2)ImGui::SameLine();const bool selected=selected_tool==int(i);if(selected)ImGui::PushStyleColor(ImGuiCol_Button,{.19f,.38f,.29f,1});if(ImGui::Button(landscape_page==0?shapes[i]:plants[i],{i==4&&landscape_page==0?-1.f:half,34})){landscape_cancel();selected_tool=int(i);if(landscape_page==0&&i==4)landscape_strength=8;}landscape_button_record(4+i);if(landscape_page==1&&i==7&&ImGui::IsItemHovered())ImGui::SetTooltip("Clears generated growth. Imported foliage and individual props remain editable objects.");if(selected)ImGui::PopStyleColor();}
            float diameter=landscape_radius*2;if(ImGui::SliderFloat("Brush width",&diameter,16,512,"%.0f m"))landscape_radius=diameter*.5f;
            if(landscape_page==0)ImGui::SliderFloat("Strength",&landscape_strength,.1f,8,"%.1f");else{float percent=landscape_density*100;if(ImGui::SliderFloat("Density",&percent,5,100,"%.0f%%"))landscape_density=percent*.01f;}
            if(ImGui::Button("Smaller",{half,0}))landscape_radius=std::max(8.f,landscape_radius*.5f);ImGui::SameLine();if(ImGui::Button("Larger",{half,0}))landscape_radius=std::min(256.f,landscape_radius*2);
            ImGui::TextWrapped(landscape_page==0?"Hold left mouse to shape open terrain; moving is optional. Release to build the wire preview. Buildings, streets and imported ground stay protected.":"Paint whole areas with trees and ground cover. Growth streams around the camera; individual props are separate.");
            if(landscape_page==1){
                ImGui::Separator();ImGui::TextUnformatted("Brush affects these layers");
                const char *layers[]={"Grass","Trees","Rocks","Ferns"};
                for(unsigned i=0;i<4;++i){if(i%2)ImGui::SameLine(0,18);ImGui::PushID(1200+int(i));bool selected=(landscape_layer_mask&(1u<<i))!=0;
                    if(ImGui::Checkbox(layers[i],&selected)){if(selected)landscape_layer_mask|=1u<<i;else landscape_layer_mask&=~(1u<<i);}
                    const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();landscape_layer_buttons[i]={(lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f};ImGui::PopID();}
                ImGui::SeparatorText("Coverage per species");
                for(unsigned i=0;i<4;++i){ImGui::PushID(2400+int(i));ImGui::SliderFloat(layers[i],&landscape_species_scale[i],0,1.5f,"%.2fx");ImGui::PopID();}
                const double area=3.141592653589793*landscape_radius*landscape_radius;
                const double spacing[]={3,8,18,5};double candidates=0;
                for(unsigned i=0;i<4;++i)if(landscape_layer_mask&(1u<<i))candidates+=area/(spacing[i]*spacing[i])*landscape_density*landscape_species_scale[i];
                ImGui::Text("Brush footprint: %.0f m2 | ~%.0f candidate cells",area,candidates);
                ImGui::TextColored(candidates>6500?ImVec4{1,.63f,.35f,1}:ImVec4{.55f,.88f,.72f,1},"Streaming cost: %s",candidates>6500?"HIGH - use smaller strokes":candidates>2500?"MEDIUM":"LOW");
                ImGui::TextDisabled("Estimate before slope, water, paths and overlap filtering.");
                ImGui::SliderFloat("Brush maximum slope",&landscape_max_slope,0,85,"%.0f deg");
                ImGui::TextWrapped("Unchecked layers stay unchanged. Clear growth erases only the checked layers.");
                if(!landscape_layer_mask)ImGui::TextColored({1,.65f,.4f,1},"Select at least one layer to paint.");
            }
            if(landscape_page==1&&!draft.ecology.empty()){
                landscape_area=std::clamp(landscape_area,0,int(draft.ecology.size()-1));
                if(ImGui::BeginCombo("Painted area",("Area "+std::to_string(landscape_area+1)).c_str())){for(unsigned i=0;i<draft.ecology.size();++i)if(ImGui::Selectable(("Area "+std::to_string(i+1)).c_str(),landscape_area==int(i)))landscape_area=int(i);ImGui::EndCombo();}
                if(landscape_area_edit_index!=landscape_area||landscape_area_edit_revision!=revision){landscape_area_edit=draft.ecology[unsigned(landscape_area)];landscape_area_edit_mask=world_ecology::layer_mask(landscape_area_edit);landscape_area_edit_index=landscape_area;landscape_area_edit_revision=revision;}
                if(ImGui::CollapsingHeader("Edit selected area")){
                    const char *labels[]={"Area grass","Area trees","Area rocks","Area ferns"};
                    for(unsigned i=0;i<4;++i)ImGui::SliderFloat(labels[i],&landscape_area_edit.density[i],0,1,"%.2f");
                    ImGui::SliderFloat("Area maximum slope",&landscape_area_edit.max_slope,0,85,"%.0f deg");
                    auto &mask=landscape_area_edit_mask;const char *owners[]={"Own grass","Own trees","Own rocks","Own ferns"};
                    for(unsigned i=0;i<4;++i){if(i%2)ImGui::SameLine(0,12);bool selected=(mask&(1u<<i))!=0;if(ImGui::Checkbox(owners[i],&selected)){if(selected)mask|=1u<<i;else mask&=~(1u<<i);}}
                    // Never encode an empty mask as legacy full ownership.
                    const bool empty_mask=mask==0;
                    if(!empty_mask)landscape_area_edit.preset=world_ecology::habitat(landscape_area_edit)|(mask==15?0:mask<<8);
                    ImGui::BeginDisabled(empty_mask||apply||production_freeze());
                    if(ImGui::Button("Update selected area",{-1,30})){
                        auto next=draft;next.ecology[unsigned(landscape_area)]=landscape_area_edit;std::string error;
                        if(world_studio::valid(next,error)){checkpoint();draft=std::move(next);changed();landscape_wait_for_build();apply=true;++landscape_area_updates;message="Updating selected growth layers and slope; Undo restores the area.";}
                        else landscape_creation_failure("Area unchanged: "+error);
                    }const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();landscape_area_update_button={(lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f};ImGui::EndDisabled();
                    if(empty_mask)ImGui::TextWrapped("Keep at least one owned layer, or remove this area below.");
                }
                if(ImGui::Button("Remove this area",{-1,0})){checkpoint();draft.ecology.erase(draft.ecology.begin()+landscape_area);changed();apply=true;landscape_area=-1;}
            }
        }else{
            const char *water_tools[]={"Pond","Lake","River"};
            for(unsigned i=0;i<3;++i){if(i)ImGui::SameLine();const bool selected=landscape_water_kind==int(i);if(selected)ImGui::PushStyleColor(ImGuiCol_Button,{.13f,.34f,.43f,1});if(ImGui::Button(water_tools[i],{third,34}))landscape_choose_water(int(i));landscape_button_record(14+i);if(selected)ImGui::PopStyleColor();}
            if(landscape_water_kind!=2){
                ImGui::TextWrapped("Click once on clear terrain. The basin, dry banks and water build together; no separate sculpting or Apply is needed.");
                ImGui::Text("Water objects: %zu / 8",draft.waters.size());
                const bool full=draft.waters.size()>=8;
                if(full)ImGui::TextColored({1,.63f,.42f,1},"Water limit reached: Undo or remove a water edit first.");
                ImGui::BeginDisabled(full);
                float diameter=landscape_water_radius*2;if(ImGui::SliderFloat("Basin diameter",&diameter,12,64,"%.0f m"))landscape_water_radius=diameter*.5f;
                ImGui::SliderFloat("Basin depth",&landscape_water_depth,.5f,6,"%.1f m");
                ImGui::EndDisabled();
                ImGui::TextWrapped("Inner outline: water surface. Outer outline: the full protected basin and bank footprint. Orange means it cannot be placed. Esc cancels placement.");
                if(!landscape_water_armed&&ImGui::Button("Resume water placement",{-1,30}))landscape_water_armed=true;
            }else{
                ImGui::TextWrapped("Click along a route, then Finish river. Channel, banks and flowing water build together. Uphill-drawn routes are reversed downhill.");
                const bool full=draft.surfaces.ribbons.size()>=world_surface::ribbon_limit;
                ImGui::Text("Road / river paths: %zu / %u",draft.surfaces.ribbons.size(),world_surface::ribbon_limit);
                if(full)ImGui::TextColored({1,.63f,.42f,1},"Path limit reached: remove a road or river first.");
                ImGui::BeginDisabled(full);
                ImGui::Text("%zu points (2-32; up to 256 m per segment)",landscape_river_points.size());
                ImGui::BeginDisabled(landscape_river_points.size()<2);if(ImGui::Button("Finish river",{-1,40}))landscape_finish_river();landscape_button_record(13);ImGui::EndDisabled();
                ImGui::SliderFloat("River width",&landscape_river_width,4,24,"%.1f m");ImGui::SliderFloat("Banks",&landscape_river_bank,2,12,"%.1f m");ImGui::SliderFloat("Depth",&landscape_river_depth,.5f,4,"%.1f m");
                ImGui::EndDisabled();
                if(ImGui::Button("Undo last point",{half,0})&&!landscape_river_points.empty()){landscape_river_points.pop_back();landscape_clear_build_feedback();}ImGui::SameLine();if(ImGui::Button("Cancel river",{half,0})){landscape_river_points.clear();landscape_clear_build_feedback();}
                if(!landscape_water_armed&&ImGui::Button("Resume river drawing",{-1,30}))landscape_water_armed=true;
            }
        }
        ImGui::Separator();if(ImGui::Button("Undo landscape",{-1,28}))designer_command(world_designer_viewport::Command::Undo);
        if(ImGui::Button("Save landscape",{-1,32}))designer_command(world_designer_viewport::Command::ApplySave);
        landscape_advanced=ImGui::CollapsingHeader("Advanced landscape tools");if(landscape_advanced)production_terrain_panel();
    }
    void landscape_input(const Camera &cam){
        auto &io=ImGui::GetIO();const auto viewport=designer_viewport(io.DisplaySize.x,io.DisplaySize.y);
        if(!open||tab!=15||landscape_advanced||production_freeze()||production_playing()||designer_blocks_viewport()){if(landscape_painting||!landscape_river_points.empty())landscape_cancel();return;}
        const bool eligible=viewport.contains(io.MousePos.x,io.MousePos.y)&&!io.WantCaptureMouse&&!drag.active&&!designer_asset_placing()&&!vfx_placing&&!io.MouseDown[1];
        if(landscape_painting&&!io.MouseDown[0]){landscape_finish();return;}
        if(!eligible)return;
        target=false;pick(cam,io.MousePos.x,io.MousePos.y);if(!target)return;
        if(landscape_hover_revision!=revision){if(landscape_page==2)landscape_river_source();else landscape_source();landscape_hover_revision=revision;}
        if(landscape_page==2){
            // Entering Water can reuse a shape hover revision; prepare roots
            // independently the first time rather than trusting an old target.
            if(landscape_water_hover_revision==~std::uint64_t(0))landscape_river_source();
            landscape_water_hover();
            if(!landscape_water_armed||apply)return;
            if(ImGui::IsMouseClicked(0)){
                if(landscape_water_kind!=2){landscape_place_pond();return;}
                if(draft.surfaces.ribbons.size()>=world_surface::ribbon_limit){landscape_creation_failure("River unchanged: all 16 road / river paths are used. Remove a path first.");return;}
                if(landscape_water_hover_blocked){landscape_creation_failure("River point not added: "+landscape_water_hover_error);return;}
                if(landscape_river_points.size()>=32){landscape_creation_failure("Finish this river before adding more points.");return;}
                if(!landscape_river_points.empty()){const auto &last=landscape_river_points.back();const double distance=std::hypot(x-last.x,z-last.z);if(distance<.25||distance>256){landscape_creation_failure("Choose the next river point between 0.25 and 256 metres away. Your drawn points are preserved.");return;}}
                landscape_clear_build_feedback();landscape_river_points.push_back({x,z,landscape_river_point_height(draft,x,z)});message="River point added. Click the next point, then Finish river. Existing rivers join into one water surface; Esc cancels the route.";
            }return;
        }
        if(ImGui::IsMouseClicked(0)&&landscape_begin())landscape_stamp();
        else if(landscape_painting&&io.MouseDown[0]&&
                ((landscape_page==0&&production_now()>=landscape_next_shape_stamp)||
                 std::hypot(x-landscape_last_x,z-landscape_last_z)>=landscape_radius*(landscape_page==0?.3:1.1)))landscape_stamp();
    }
    void landscape_overlay(const Camera &cam){
        if(!open||tab!=15||landscape_advanced)return;auto *draw=ImGui::GetBackgroundDrawList();
        if(target&&landscape_page!=2){const bool blocked=landscape_ground_feedback().blocked;const ImU32 color=blocked?IM_COL32(245,125,85,230):IM_COL32(112,230,157,235);
            for(unsigned i=0;i<64;++i){const double a=i*6.283185307179586/64,b=(i+1)*6.283185307179586/64;ImVec2 p,q;const double ax=x+std::cos(a)*landscape_radius,az=z+std::sin(a)*landscape_radius,bx=x+std::cos(b)*landscape_radius,bz=z+std::sin(b)*landscape_radius;
                if(project_point(cam,{ax,landscape_draft_height(draft,ax,az)+.25,az},p)&&project_point(cam,{bx,landscape_draft_height(draft,bx,bz)+.25,bz},q))draw->AddLine(p,q,color,2);}
            if(landscape_page==1&&!blocked){ImVec2 center;if(project_point(cam,{x,landscape_draft_height(draft,x,z)+.3,z},center)){const int dots=std::min(120,int(landscape_density*120));for(int i=0;i<dots;++i){const double a=double(i)*2.399963229728653,r=landscape_radius*std::sqrt((i+.5)/120.);ImVec2 p;if(project_point(cam,{x+std::cos(a)*r,landscape_draft_height(draft,x+std::cos(a)*r,z+std::sin(a)*r)+.28,z+std::sin(a)*r},p))draw->AddCircleFilled(p,1.5f,IM_COL32(122,229,153,130));}}}
        }
        if(target&&landscape_page==2&&landscape_water_armed){
            const bool blocked=landscape_ground_feedback().blocked;
            const ImU32 color=blocked?IM_COL32(255,139,83,235):IM_COL32(106,216,255,240);
            const double inner=landscape_water_kind==2?landscape_river_width*.5:landscape_water_radius;
            const double outer=landscape_water_clearance();
            for(double radius:{inner,outer})for(unsigned i=0;i<64;++i){
                const double a=i*6.283185307179586/64,b=(i+1)*6.283185307179586/64;
                const double ax=x+std::cos(a)*radius,az=z+std::sin(a)*radius,bx=x+std::cos(b)*radius,bz=z+std::sin(b)*radius;ImVec2 p,q;
                if(project_point(cam,{ax,landscape_draft_height(draft,ax,az)+.3,az},p)&&project_point(cam,{bx,landscape_draft_height(draft,bx,bz)+.3,bz},q))draw->AddLine(p,q,color,radius==inner?2.5f:1.5f);
            }
        }
        if(target&&landscape_page==0&&landscape_painting&&landscape_stroke_changed){
            // Preview the actual sampled draft surface while the GPU terrain
            // worker keeps using the committed copy until mouse release.
            const double step=landscape_radius/6;
            for(int iz=-6;iz<6;++iz)for(int ix=-6;ix<6;++ix){
                const double ax=x+ix*step,az=z+iz*step;if(std::hypot(ax-x,az-z)>landscape_radius)continue;
                ImVec2 corners[4];bool visible=true;for(int k=0;k<4;++k){const double px=ax+(k==1||k==2?step:0),pz=az+(k>=2?step:0);visible&=project_point(cam,{px,landscape_draft_height(draft,px,pz)+.18,pz},corners[k]);}
                if(visible)draw->AddQuadFilled(corners[0],corners[1],corners[2],corners[3],IM_COL32(102,215,162,36));
            }
            auto grid_line=[&](double ax,double az,double bx,double bz){
                if(std::hypot(ax-x,az-z)>landscape_radius||std::hypot(bx-x,bz-z)>landscape_radius)return;
                ImVec2 a,b;
                if(project_point(cam,{ax,landscape_draft_height(landscape_before,ax,az)+.16,az},a)&&project_point(cam,{bx,landscape_draft_height(landscape_before,bx,bz)+.16,bz},b))draw->AddLine(a,b,IM_COL32(238,207,139,75),1);
                if(project_point(cam,{ax,landscape_draft_height(draft,ax,az)+.20,az},a)&&project_point(cam,{bx,landscape_draft_height(draft,bx,bz)+.20,bz},b))draw->AddLine(a,b,IM_COL32(116,255,191,220),1.5f);
            };
            for(int iz=-6;iz<=6;++iz)for(int ix=-6;ix<6;++ix){
                grid_line(x+ix*step,z+iz*step,x+(ix+1)*step,z+iz*step);
                grid_line(x+iz*step,z+ix*step,x+iz*step,z+(ix+1)*step);
            }
            ImVec2 before,after;
            if(project_point(cam,{x,landscape_draft_height(landscape_before,x,z)+.20,z},before)&&project_point(cam,{x,landscape_draft_height(draft,x,z)+.20,z},after)){
                draw->AddLine(before,after,IM_COL32(255,224,125,255),3);draw->AddCircleFilled(after,4,IM_COL32(116,255,191,255));
            }
        }
        const auto &io=ImGui::GetIO();const auto viewport=designer_viewport(io.DisplaySize.x,io.DisplaySize.y);
        if(viewport.w>160&&viewport.h>100&&viewport.contains(io.MousePos.x,io.MousePos.y)&&!io.WantCaptureMouse&&!io.MouseDown[1]&&!designer_blocks_viewport()){
            const auto feedback=landscape_ground_feedback();char label[256];
            if(target&&landscape_page==0&&landscape_painting&&landscape_stroke_changed)
                std::snprintf(label,sizeof label,"DRAFT HEIGHT %+.2f m\nRelease to build | Esc cancels",landscape_draft_height(draft,x,z)-landscape_draft_height(landscape_before,x,z));
            else if(feedback.blocked)std::snprintf(label,sizeof label,"%s\n%s",feedback.title,landscape_page==2?"See Water panel for the reason":"Use Find open ground in Landscape");
            else if(landscape_page==2&&landscape_water_kind!=2)std::snprintf(label,sizeof label,"%s | %.0f m wide, %.1f m deep\nClick once to carve and fill | Esc cancels",landscape_water_kind==0?"POND":"LAKE",landscape_water_radius*2,landscape_water_depth);
            else std::snprintf(label,sizeof label,"%s\n%s",feedback.title,landscape_page==0?"Hold left mouse; release to build":landscape_page==1?"Drag left mouse; release to build":"Click to add a river point");
            const float width=std::min(300.f,std::max(120.f,viewport.w-24.f));const auto text_size=ImGui::CalcTextSize(label,nullptr,false,width-16);
            const float px=std::clamp(io.MousePos.x+20,viewport.x+8,std::max(viewport.x+8,viewport.x+viewport.w-width-8)),py=std::clamp(io.MousePos.y+22,viewport.y+8,std::max(viewport.y+8,viewport.y+viewport.h-text_size.y-20));
            draw->AddRectFilled({px,py},{px+width,py+text_size.y+14},IM_COL32(13,23,28,232),5);
            draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),{px+8,py+7},feedback.blocked?IM_COL32(255,184,126,255):IM_COL32(178,255,215,255),label,nullptr,width-16);
        }
        ImVec2 previous;bool have_previous=false;for(const auto &point:landscape_river_points){ImVec2 at;if(project_point(cam,{point.x,point.height+.35,point.z},at)){draw->AddCircleFilled(at,5,IM_COL32(95,205,255,255));if(have_previous)draw->AddLine(previous,at,IM_COL32(95,205,255,235),3);previous=at;have_previous=true;}else have_previous=false;}
    }

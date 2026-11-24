// Streamed-world authoring. Included inside the viewer's private namespace.
// Documents never change while a terrain worker reads their committed copy.
#include "engine/world_studio_history_rebuild.h"
struct Studio {
    world_studio::Document draft,live;
    world_studio::History history;
    std::vector<WorldSceneryPlacement> source;
    std::vector<bool> source_explicit_pivots;
    std::vector<std::string> source_names;std::string signature;
    std::string path="world.studio";unsigned applied=0,rejected=0;
    bool reload_shaders=false,fail_shader_test=false;
    bool open=false,dirty=false,apply=false,rebuild=false,loaded=false;
    int quickspot_destination=-1;
    int tab=0,mode=0,alias=0,selected=-1,selected_added=-1,light=-1,decal=-1,spline=-1;
    float radius=8,strength=.6f,scale=1,yaw=0,mask=0,plane=0,fly_speed=40;
    bool plane_set=false;std::vector<size_t> nearby;
    int vegetation_kind=0,scatter_count=8;
    double x=0,y=0,z=0;
    bool target=false;
    char filter[96]={};
    std::string message="Click the world to choose a target. Apply commits the draft; Save stores the applied world.";
    #include "engine/world_studio_tools_view.inc"
    #include "engine/world_studio_assets_view.inc"
    #include "engine/world_studio_runtime_view.inc"
    #include "engine/world_studio_vfx_view.inc"
    #include "engine/world_studio_graph_view.inc"
    #include "engine/world_studio_surface_runtime.inc"
    #include "engine/world_studio_surface_view.inc"
    #include "engine/world_studio_paths_view.inc"
    #include "engine/world_studio_production_view.inc"
    #include "engine/world_studio_living_view.inc"
    #include "engine/world_studio_memory_view.inc"
    #include "engine/world_studio_audio_view.inc"
    #include "engine/world_studio_weather_view.inc"
    #include "engine/world_studio_cinematic_view.inc"
    #include "engine/world_studio_terrain_view.inc"
    #include "engine/world_studio_regions_view.inc"
    #include "engine/world_studio_gameplay_view.inc"
    #include "engine/world_studio_physics_view.inc"
    #include "engine/world_studio_animation_view.inc"
    #include "engine/world_studio_components_view.inc"
    #include "engine/world_studio_designer_selection.inc"
    #include "engine/world_studio_designer_assets.inc"
    #include "engine/world_studio_designer_workspace.inc"
    #include "engine/world_studio_project_view.inc"
    #include "engine/world_studio_landscape_view.inc"
    #include "engine/world_studio_structure_view.inc"
    #include "engine/world_studio_structure_splines_view.inc"
    explicit Studio(const char *test_path=nullptr){if(test_path&&std::strncmp(test_path,"/tmp/",5)==0&&std::strlen(test_path)<512)path=test_path;loaded=world_studio::load(path.c_str(),draft,message);live=draft;if(!prepare_surface_field(live,message)){live={};loaded=false;dirty=open=true;}project_directory=std::filesystem::current_path().string();char *base=SDL_GetBasePath();if(base){import_cooker=std::string(base)+"tools/import_world_asset.py";SDL_free(base);}if(!configure(live,message)){live={};loaded=false;dirty=open=true;std::string ignored;configure(live,ignored);}if(!loaded&&!dirty)message=access(path.c_str(),F_OK)==0?"Saved edits could not be loaded: "+message:"No saved edits. Choose a target in the viewport.";production_init();living_init();regions_init();components_init();designer_init();project_init();if(const char *mode=std::getenv("VALWOOD_CINEMATIC_EDITOR");mode&&std::strcmp(mode,"1")==0){open=true;designer_tools=true;designer_immersive=false;tab=requested_tab=17;}}
    void capture(WorldScenery *scene){
        const auto n=world_scenery_stats(scene).source_instances;source.reserve(n);source_names.reserve(n);source_explicit_pivots.reserve(n);
        for(size_t i=0;i<n;i++){WorldSceneryPlacement p;if(world_scenery_placement(scene,i,&p)){source_names.emplace_back(p.alias);source.push_back(p);source_explicit_pivots.push_back(world_scenery_explicit_pivot(scene,i));}}
        std::ostringstream identity;identity<<std::setprecision(17);
        for(size_t i=0;i<source.size();i++){auto &p=source[i];p.alias=source_names[i].c_str();identity<<p.alias<<' '<<p.x<<' '<<p.z<<' '<<p.yaw<<' '<<p.scale<<' '<<p.burial<<' '<<p.elevation<<' '<<p.source_file<<'\n';if(p.offset_y!=0)identity<<"offset "<<p.offset_y<<'\n';}
        uint64_t hash=14695981039346656037ull;for(unsigned char c:identity.str()){hash^=c;hash*=1099511628211ull;}char text[17];std::snprintf(text,sizeof text,"%016llx",static_cast<unsigned long long>(hash));signature=text;
        source_poses.reserve(source.size());for(const auto &p:source){world_studio::Pose a;a.alias=p.alias;a.root={p.x,p.y,p.z};a.yaw=p.yaw;a.scale=p.scale;a.burial=p.burial;a.elevation=p.elevation;a.offset_y=p.offset_y;a.pitch=p.pitch;a.roll=p.roll;a.sx=p.sx;a.sy=p.sy;a.sz=p.sz;a.wind_sway=p.wind_sway;a.wind_flutter=p.wind_flutter;a.minimum={p.minimum[0],p.minimum[1],p.minimum[2]};a.maximum={p.maximum[0],p.maximum[1],p.maximum[2]};source_poses.push_back(a);}
        if(!source_bound(draft)){draft.source_signature=live.source_signature=signature;}
        for(const auto &p:source){world_authoring::Source value;value.alias=p.alias;const auto stats=world_scenery_source_stats(scene,p.source_file);value.file=stats.path?stats.path:std::to_string(p.source_file);value.x=p.x;value.z=p.z;value.y=p.y;value.yaw=p.yaw*57.29577951308232;value.scale=p.scale;value.burial=p.burial;value.elevation=p.elevation;value.offset_y=p.offset_y;value.pitch=p.pitch*57.29577951308232;value.roll=p.roll*57.29577951308232;value.sx=p.sx;value.sy=p.sy;value.sz=p.sz;identity_sources.push_back(std::move(value));}
        std::string binding_error;auto next_draft=draft,next_live=live;auto draft_graph=next_draft.entities,live_graph=next_live.entities;
        if(world_authoring::reconcile(next_draft,draft_graph,identity_sources,signature,production_ground,&next_draft,binding_error)&&world_authoring::reconcile(next_live,live_graph,identity_sources,signature,production_ground,&next_live,binding_error)){next_draft.entities=std::move(draft_graph);next_live.entities=std::move(live_graph);draft=std::move(next_draft);live.entities=std::move(next_live.entities);live.source_signature=std::move(next_live.source_signature);live.overrides=std::move(next_live.overrides);live.groups=std::move(next_live.groups);production_id_highwater=std::max(draft.entities.next_id,live.entities.next_id);}else{message=binding_error;dirty=open=true;}

        refresh_scene(scene);landscape_refresh_protection();designer_invalidate_sources();revision++;entity_world_revision=~std::uint64_t(0);picking_registry=0;
    }
    void checkpoint(){history.checkpoint(draft);dirty=true;revision++;production_edited();}
    void changed(bool geometry=false){dirty=true;rebuild|=geometry;revision++;production_edited();}
    void clear_selection(){designer_selection.clear();entity_selected=0;designer_projection_ready=false;selection_keys.clear();selected=selected_added=-1;drag.active=false;preview_lod=-1;}
    void restore_history(bool redo){
        sound_cancel();weather_cancel();auto preserved=designer_selection;const auto context=designer_context();
        // A first edit registers a source model with a persistent entity ID.
        // Undo can remove that registration while leaving the source itself.
        // Retain canonical source identity so the same visible item stays picked.
        auto source_reference=[&](world_designer::Ref ref){world_designer::Ref resolved;world_entity::Binding binding;std::string error;
            if(world_designer::resolve(draft,context,ref,resolved,binding,error)&&binding.kind==world_entity::Kind::SourceModel){
                auto stable=world_designer::Ref::source_model(binding.index);
                if(binding.index<identity_sources.size())stable.source_key=world_authoring::source_key(identity_sources[binding.index]);
                return stable;
            }return ref;
        };
        for(auto &ref:preserved.items)ref=source_reference(ref);preserved.active=source_reference(preserved.active);
        const auto &snapshots=redo?history.forward:history.back;
        const bool audio_only=!snapshots.empty()&&world_studio::environment_only_change(draft,snapshots.back());
        const bool history_rebuild=!snapshots.empty()&&world_studio_history::requires_scenery_rebuild(draft,snapshots.back(),rebuild);
        if(redo?history.redo(draft):history.undo(draft)){if(!redo)ui_undos++;clear_selection();designer_selection=std::move(preserved);changed(history_rebuild);designer_reconcile_selection();if(audio_only)sound_preview_apply();else designer_preview_object_edit();message=designer_object_preview?(redo?"Redo restored. Updating the scene.":"Undo restored. Updating the scene."):(redo?"Redo restored a draft. Apply to preview.":"Undo restored a draft. Apply to preview.");}}
    double last_apply_ms=0;bool last_apply_terrain=false,last_apply_masks=false;unsigned terrain_rebuild_count=0,terrain_rebuild_skipped=0;
    static bool terrain_inputs_changed(const world_studio::Document &a,const world_studio::Document &b){
        if(!world_surface::same_paths(a.surfaces,b.surfaces)||a.terrain.size()!=b.terrain.size()||a.waters.size()!=b.waters.size()||a.stamps.size()!=b.stamps.size())return true;
        for(size_t i=0;i<a.terrain.size();i++){const auto &x=a.terrain[i],&y=b.terrain[i];if(x.x!=y.x||x.z!=y.z||x.delta!=y.delta)return true;}
        for(size_t i=0;i<a.waters.size();i++){const auto &x=a.waters[i],&y=b.waters[i];if(x.source!=y.source||x.enabled!=y.enabled||x.x!=y.x||x.z!=y.z||x.level!=y.level||x.radius_x!=y.radius_x||x.radius_z!=y.radius_z||x.basin_depth!=y.basin_depth)return true;}
        // Water shading can change without terrain, but preserve the callback's
        // complete water vector while its worker reads it. Treat any edit as a fence.
        for(size_t i=0;i<a.waters.size();i++){const auto &x=a.waters[i],&y=b.waters[i];if(x.tint!=y.tint||x.absorption!=y.absorption||x.roughness!=y.roughness||x.waves!=y.waves||x.flow!=y.flow||x.foam!=y.foam)return true;}
        for(size_t i=0;i<a.stamps.size();i++){const auto &x=a.stamps[i],&y=b.stamps[i];if(x.x!=y.x||x.z!=y.z||x.radius!=y.radius||x.value!=y.value||x.kind!=y.kind)return true;}
        return false;
    }
    static bool graphs_changed(const world_studio::Document &a,const world_studio::Document &b){
        if(a.graphs.size()!=b.graphs.size())return true;
        for(size_t i=0;i<a.graphs.size();i++){const auto &x=a.graphs[i],&y=b.graphs[i];if(x.alias!=y.alias||x.outputs!=y.outputs||x.nodes.size()!=y.nodes.size())return true;for(size_t j=0;j<x.nodes.size();j++){const auto &u=x.nodes[j],&v=y.nodes[j];if(u.id!=v.id||u.op!=v.op||u.inputs!=v.inputs||u.value!=v.value)return true;}}return false;
    }
    static bool surface_changed(const world_surface::Data &a,const world_surface::Data &b){std::string x,y,error;return !world_surface::encode(a,x,error)||!world_surface::encode(b,y,error)||x!=y;}
    static bool shelter_changed(const WorldScenery *a,const WorldScenery *b){
        std::vector<WorldSceneryPlacement> x,y;auto gather=[](const WorldScenery *s,std::vector<WorldSceneryPlacement> &out){const auto count=world_scenery_stats(s).source_instances;for(size_t i=0;i<count;i++){WorldSceneryPlacement p;if(world_scenery_placement(s,i,&p)&&(p.foundation||p.tree))out.push_back(p);}};gather(a,x);gather(b,y);if(x.size()!=y.size())return true;
        for(size_t i=0;i<x.size();i++){const auto &p=x[i],&q=y[i];if(std::strcmp(p.alias,q.alias)||std::abs(p.x-q.x)>1e-5||std::abs(p.y-q.y)>5e-4||std::abs(p.z-q.z)>1e-5||std::abs(p.yaw-q.yaw)>2e-6||std::abs(p.scale-q.scale)>1e-6||std::abs(p.pitch-q.pitch)>2e-6||std::abs(p.roll-q.roll)>2e-6||std::abs(p.sx-q.sx)>1e-6||std::abs(p.sy-q.sy)>1e-6||std::abs(p.sz-q.sz)>1e-6||!std::equal(p.minimum,p.minimum+3,q.minimum)||!std::equal(p.maximum,p.maximum+3,q.maximum))return true;}return false;
    }
    bool geometry()const{return !live.structures.empty()||!draft.structures.empty()||!live.terrain.empty()||!live.placements.empty()||!live.overrides.empty()||!live.stamps.empty()||!draft.terrain.empty()||!draft.placements.empty()||!draft.overrides.empty()||!draft.stamps.empty();}
    bool safe_brush()const{
        // Fixed demo navigation and building floors are not terrain-editable.
        if(world_atlas_village_weight(x,z)>.01f||world_atlas_bandit_sample(x,z).clearing>.01)return false;
        for(const auto &p:source)if(p.foundation&&std::hypot(x-p.x,z-p.z)<radius+std::max({std::abs(p.minimum[0]),std::abs(p.maximum[0]),std::abs(p.minimum[2]),std::abs(p.maximum[2])})*p.scale+3)return false;
        auto route_overlap=[&](const WorldAtlasTrailPoint *points,size_t count){for(size_t i=1;i<count;i++){double dx=points[i].x-points[i-1].x,dz=points[i].z-points[i-1].z,length2=dx*dx+dz*dz;if(length2<.001)continue;double t=std::clamp(((x-points[i-1].x)*dx+(z-points[i-1].z)*dz)/length2,0.,1.);if(std::hypot(x-points[i-1].x-dx*t,z-points[i-1].z-dz*t)<radius+4)return true;}return false;};
        size_t count=0;auto points=world_atlas_rainforest_trail_points(&count);if(route_overlap(points,count))return false;
        points=world_atlas_trail(&count);if(route_overlap(points,count))return false;
        if(std::hypot(x-WORLD_TRAIL_CAMP_X,z-WORLD_TRAIL_CAMP_Z)<radius+38)return false;
        return !world_atlas_local_water(x,z,nullptr,nullptr)&&world_atlas_height(x,z)>radius*.5f;
    }
    void stamp(){
        if(!target){message="Choose dry ground in the viewport first.";return;}
        if(tab==0){
            if(!safe_brush()){message="This brush overlaps a fixed demo foundation, water, or navigation area. Sculpt open terrain outside the settlement.";return;}
            auto next=draft;
            if(world_studio::brush(next,x,z,radius,strength,mode,plane_set?plane:float(y),world_atlas_base_height,message)){checkpoint();draft=std::move(next);changed(true);message="Terrain stroke staged. Apply rebuilds the terrain and grounding.";}
        }else if(tab==1){
            if(draft.stamps.size()>=world_studio::stamp_limit){message="32 mask strokes reached. Undo or clear the masks.";return;}
            checkpoint();draft.stamps.push_back({x,z,radius,mask,vegetation_kind});changed(true);message="Vegetation mask staged. Apply regenerates the streamed roots.";
        }
    }
    bool save(){
        if(!production_can_save()){message="Stop Play and complete scene restoration before saving.";return false;}
        if(dirty){message="Apply the draft before saving.";return false;}
        if(editor_session&&!editor_session->wait(message))return false;
        bool ok=world_studio::save(path.c_str(),live,message);if(!ok)return false;
        production_project_saved=true;
        if(editor_session&&!editor_session->clean_saved(live,message)){message="World saved, but recovery cleanup failed: "+message;return false;}
        message="Applied world saved to world.studio.";return true;
    }
    WorldScenery *scene(const world_studio::Document &doc,std::string &error,WorldScenery *reuse=nullptr){
        // Source files are derived, private transactions. The purchased source
        // files stay intact; world.studio alone is the durable edit document.
        std::string folder=vw_temp_pattern("valwood-studio-XXXXXX");if(!mkdtemp(folder.data())){error="Cannot create scene transaction";return nullptr;}
        struct Cleanup {std::string root;std::vector<std::string> files;~Cleanup(){for(const auto &p:files)unlink(p.c_str());rmdir(root.c_str());}} cleanup{folder,{}};
        if(source_bound(doc)&&doc.source_signature!=signature){error="The base scene changed. Clear source overrides and reselect objects; saved indices cannot safely identify this version.";return nullptr;}
        for(const auto &o:doc.overrides)if(o.index>=source.size()){error="Source object no longer exists in this scene";return nullptr;}
        for(const auto &g:doc.groups)for(unsigned key:g.members)if(key<WORLD_ADDED_KEY_BASE&&key>=source.size()){error="Source group object no longer exists";return nullptr;}
        if(!configure(doc,error))return nullptr;
        struct RegistryRollback{Studio *owner;bool committed=false;~RegistryRollback(){if(!committed){std::string ignored;owner->configure(owner->live,ignored);}}} rollback{this};
        const bool terrain_changed=terrain_inputs_changed(live,doc);
        std::vector<std::pair<world_entity::Id,world_entity::Transform>> world_poses;if(!world_entity::world_transforms(doc.entities,world_poses,error))return nullptr;
        // Resolve each authored binding once. A large imported scene mostly has
        // unregistered source roots; searching every entity and every world pose
        // again for each root made a local terrain stroke scale quadratically.
        std::sort(world_poses.begin(),world_poses.end(),[](const auto &a,const auto &b){return a.first<b.first;});
        std::vector<std::pair<world_entity::Id,size_t>> node_indices;node_indices.reserve(doc.entities.nodes.size());
        for(size_t i=0;i<doc.entities.nodes.size();i++)node_indices.emplace_back(doc.entities.nodes[i].id,i);
        std::sort(node_indices.begin(),node_indices.end(),[](const auto &a,const auto &b){return a.first<b.first;});
        std::vector<unsigned char> visibility(doc.entities.nodes.size());
        auto node_visible=[&](auto &&self,size_t index)->bool{
            if(visibility[index])return visibility[index]==2;
            const auto &node=doc.entities.nodes[index];bool value=node.visible;
            if(value&&node.parent){const auto parent=std::lower_bound(node_indices.begin(),node_indices.end(),node.parent,[](const auto &entry,world_entity::Id id){return entry.first<id;});value=self(self,parent->second);}
            visibility[index]=value?2:1;return value;
        };
        const float no_pivot=std::numeric_limits<float>::quiet_NaN();
        std::vector<float> source_pivots(source.size(),no_pivot),placement_pivots(doc.placements.size(),no_pivot);
        std::vector<bool> source_visible(source.size(),true),placement_visible(doc.placements.size(),true);
        for(size_t i=0;i<doc.entities.nodes.size();i++){
            const auto &node=doc.entities.nodes[i];const auto pose=std::lower_bound(world_poses.begin(),world_poses.end(),node.id,[](const auto &entry,world_entity::Id id){return entry.first<id;});
            if(node.binding.kind==world_entity::Kind::SourceModel&&node.binding.index<source_pivots.size()){source_pivots[node.binding.index]=float(pose->second.y);source_visible[node.binding.index]=node_visible(node_visible,i);}
            else if(node.binding.kind==world_entity::Kind::Placement&&node.binding.index<placement_pivots.size()){placement_pivots[node.binding.index]=float(pose->second.y);placement_visible[node.binding.index]=node_visible(node_visible,i);}
        }
        std::vector<const world_studio::Override *> source_edits(source.size(),nullptr);
        for(const auto &edit:doc.overrides)if(!source_edits[edit.index])source_edits[edit.index]=&edit;
        std::array<std::ostringstream,10> contents;
        auto write=[&](size_t file,const char *name,double x,double z,double yaw,float scale,float burial,float elevation,float offset_y,double pitch=0,double roll=0,float sx=1,float sy=1,float sz=1,float pivot=std::numeric_limits<float>::quiet_NaN(),bool preserve_source_root=false,float wind_sway=1,float wind_flutter=1){double u,v;world_atlas_world_to_uv(x,z,&u,&v);world_studio_scene_record::write(contents[file],{name,u,v,yaw,scale,burial,elevation,offset_y,pitch,roll,sx,sy,sz,pivot,preserve_source_root,wind_sway,wind_flutter});};
        for(size_t i=0;i<source.size();i++){
            const auto &p=source[i];const auto *edit=source_edits[i];
            if((edit&&edit->hidden)||!source_visible[i])continue;
            // A wind-only override must retain an intentionally elevated root.
            // Source identity and grounding are independent of its response.
            const bool same_transform=!edit||(std::abs(edit->x-p.x)<1.e-8&&std::abs(edit->z-p.z)<1.e-8&&
                std::abs(std::remainder(double(edit->yaw)-p.yaw*57.29577951308232,360.))<1.e-4&&
                edit->scale==p.scale&&edit->offset_y==p.offset_y&&
                std::abs(edit->pitch-p.pitch*57.29577951308232)<1.e-4&&std::abs(edit->roll-p.roll*57.29577951308232)<1.e-4&&
                edit->sx==p.sx&&edit->sy==p.sy&&edit->sz==p.sz);
            write(std::min<size_t>(8,p.source_file),p.alias,edit?edit->x:p.x,edit?edit->z:p.z,edit?double(edit->yaw):std::clamp(double(p.yaw)*180/3.141592653589793,-360.,360.),edit?edit->scale:p.scale,p.burial,p.elevation,edit?edit->offset_y:p.offset_y,edit?edit->pitch:p.pitch*57.29577951308232,edit?edit->roll:p.roll*57.29577951308232,edit?edit->sx:p.sx,edit?edit->sy:p.sy,edit?edit->sz:p.sz,std::isnan(source_pivots[i])?p.y:source_pivots[i],same_transform&&(source_explicit_pivots[i]||!terrain_changed),edit?(edit->wind_enabled?edit->wind_sway:0):p.wind_sway,edit?(edit->wind_enabled?edit->wind_flutter:0):p.wind_flutter);
        }
        for(unsigned i=0;i<doc.placements.size();i++)if(placement_visible[i]){const auto &p=doc.placements[i];write(9,p.alias.c_str(),p.x,p.z,p.yaw,p.scale,p.burial,p.elevation,p.offset_y,p.pitch,p.roll,p.sx,p.sy,p.sz,std::isnan(placement_pivots[i])?float(production_ground(p.x,p.z,&doc)+p.offset_y+p.elevation-p.burial):placement_pivots[i],false,p.wind_enabled?p.wind_sway:0,p.wind_enabled?p.wind_flutter:0);}
        for(size_t i=0;i<contents.size();i++)if(!contents[i].str().empty()){
            auto path=std::string(folder)+"/"+std::to_string(i)+".txt";cleanup.files.push_back(path);std::ofstream f(path);f<<contents[i].str();if(!f){error="Cannot write scene transaction";return nullptr;}
        }
        std::vector<const char*> paths;for(const auto &p:cleanup.files)paths.push_back(p.c_str());
        char detail[512]={};auto *result=world_scenery_prepare_files(reuse,paths.data(),paths.size(),"models",detail,sizeof(detail));
        if(!result)error=detail;else rollback.committed=true;
        return result;
    }
    bool moved_architecture()const{
        for(unsigned i=0;i<source.size();i++)if(source[i].foundation&&!world_entity::visible(live.entities,{world_entity::Kind::SourceModel,i}))return true;
        for(const auto &o:live.overrides)if(o.index<source.size()){
            const auto &p=source[o.index];if(!p.foundation)continue;
            if(o.pitch!=p.pitch*57.29577951308232||o.roll!=p.roll*57.29577951308232||o.sx!=p.sx||o.sy!=p.sy||o.sz!=p.sz||o.hidden||std::hypot(o.x-p.x,o.z-p.z)>.001||std::abs(o.offset_y-p.offset_y)>.001||std::abs(o.scale-p.scale)>.00001||std::abs(std::remainder(o.yaw-p.yaw*180/3.141592653589793,360.))>.001)return true;
        }return false;
    }
    void materials(WorldScenery *scene){std::array<WorldSceneryMaterial,64> values={};size_t n=0;for(const auto &m:live.materials){auto &v=values[n++];v.alias=m.alias.c_str();std::copy(m.tint.begin(),m.tint.end(),v.tint);v.roughness=m.roughness;v.normal=m.normal;v.uv=m.uv;}world_scenery_materials(scene,values.data(),n);std::array<const char*,8> names={};for(unsigned i=0;i<live.graphs.size();i++)names[i]=live.graphs[i].alias.c_str();world_scenery_graphs(scene,names.data(),live.graphs.size());world_village_population_graphs(population_ptr,names.data(),live.graphs.size());std::array<const char*,64> surfaces={};for(unsigned i=0;i<live.surfaces.bindings.size();i++)surfaces[i]=live.surfaces.bindings[i].alias.c_str();world_scenery_surfaces(scene,surfaces.data(),live.surfaces.bindings.size());}
    static float height(double x,double z,const void *data){const auto &doc=*static_cast<const world_studio::Document*>(data);return world_studio::height_delta(doc,x,z)+(doc.surface_field?world_surface::terrain_offset(*doc.surface_field,x,z):0);}
    static float cover(int kind,double x,double z,const void *data){const auto &doc=*static_cast<const world_studio::Document*>(data);if(world_ecology::at_layer(doc.ecology.data(),doc.ecology.size(),x,z,kind)<0&&stronghold_finish&&std::hypot(x-stronghold_x,z-stronghold_z)<(source_showcase?1800:350))return 0;return world_studio::vegetation(doc,kind,x,z)*(doc.surface_field?1-world_surface::road_mask(*doc.surface_field,x,z):1);}
    void bind(){landscape_refresh_protection();world_atlas_set_height_edit(live.terrain.empty()&&!live.surface_field?nullptr:height,&live);world_atlas_set_water_edit(live.waters.empty()&&!live.surface_field?nullptr:water_sample,&live);}
    void pick(const Camera &cam,float mx,float my){
        target=false;npc_target_source=false;
        const auto &io=ImGui::GetIO();const auto viewport=designer_viewport(io.DisplaySize.x,io.DisplaySize.y);
        if(!world_designer_viewport::valid(viewport)||!std::isfinite(mx)||!std::isfinite(my))return;
        // Use the same logical-pixel viewport projection as mesh selection and
        // overlays, even on the first frame after a panel or window resize.
        const auto ray=mouse_ray(cam,{mx,my});const V3 direction={float(ray.direction.x),float(ray.direction.y),float(ray.direction.z)};
        const double maximum_pick_distance=open&&tab==15?8192.:700.;
        world_physics::Hit source_hit;bool source_candidate=false;double source_distance=maximum_pick_distance+1;
        if(open&&(tab==10||structure_spline_active())&&active_playtest_walk){
            source_candidate=active_playtest_walk->raycast({cam.x,cam.y,cam.z},{direction.x*maximum_pick_distance,direction.y*maximum_pick_distance,direction.z*maximum_pick_distance},source_hit)&&source_hit.hit;
            if(source_candidate)source_distance=source_hit.fraction*maximum_pick_distance;
        }
        auto source_target=[&](){
            if(!source_candidate)return false;
            if(source_hit.normal.y<.65||!active_playtest_walk->contains(source_hit.position.x,source_hit.position.z)){
                message="Point at an open floor or stair tread. Walls cannot support an NPC.";return true;
            }
            x=source_hit.position.x;y=source_hit.position.y;z=source_hit.position.z;target=true;npc_target_source=true;return true;
        };
        double previous=.4;
        for(double t=.4;t<maximum_pick_distance;t+=std::max(.5,t*.012)){
            const double px=cam.x+direction.x*t,pz=cam.z+direction.z*t,py=cam.y+direction.y*t;
            if(py<=world_atlas_height(px,pz)){
                double lo=previous,hi=t;for(int i=0;i<12;i++){double m=(lo+hi)*.5;double h=world_atlas_height(cam.x+direction.x*m,cam.z+direction.z*m);if(cam.y+direction.y*m>h)lo=m;else hi=m;}
                if(source_distance<=hi+.005&&source_target())return;
                x=cam.x+direction.x*hi;z=cam.z+direction.z*hi;y=world_atlas_height(x,z);target=true;return;
            }
            if(source_distance<=t&&source_target())return;
            previous=t;
        }
        if(source_target())return;
        message=open&&tab==15?"No terrain hit. Point down, fly closer, or use Find open ground.":"No ground hit within 700m. Point down at the landscape.";
    }
    void select(){
        selected=selected_added=-1;double best=32;
        for(size_t i=0;i<source.size();i++){const auto &p=source[i];double px=p.x,pz=p.z;bool hidden=false;for(const auto &o:draft.overrides)if(o.index==i){px=o.x;pz=o.z;hidden=o.hidden;break;}if(hidden)continue;double d=std::hypot(x-px,z-pz);if(d<best){best=d;selected=int(i);}}
        for(size_t i=0;i<draft.placements.size();i++){auto &p=draft.placements[i];double d=std::hypot(x-p.x,z-p.z);if(d<best){best=d;selected=-1;selected_added=int(i);}}
        if(selected>=0)choose(unsigned(selected),false,false);else if(selected_added>=0)choose(WORLD_ADDED_KEY_BASE+unsigned(selected_added),false,false);
        message=best<32?"Selected nearest root. Transform or hide it in Objects.":"No object root within 32m.";
    }
    void designer_tool_contents(const Camera &cam){
        const int tool=std::clamp(tab,0,26);requested_tab=tool;
        // Take a property baseline only when input can mutate a widget. Passive
        // editor frames do not copy the whole world document.
        const auto &io=ImGui::GetIO();const bool focused=ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        bool keys=false;for(const auto &key:io.KeysData)keys|=key.DownDuration==0;
        const bool input=(ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)&&io.MouseDown[0])||
            (focused&&(io.WantTextInput||ImGui::IsAnyItemActive()||!io.InputQueueCharacters.empty()||keys));
        const bool coalescing=editing_||((tool>=10&&tool<=12)&&runtime_editing)||(tool==9&&asset_editing_)||(tool==7&&environment_editing_)||(tool==25&&sound_editing)||(tool==26&&weather_editing);
        if(input&&(tool!=designer_baseline_tool||(!coalescing&&designer_baseline_revision!=revision))){runtime_before=draft;designer_baseline_revision=revision;designer_baseline_tool=tool;designer_baseline_copies++;}
        const auto &before=runtime_before;bool edited=false,structural=false;
        if(ImGui::BeginTabBar("Authoring")){
            if(tool==0&&ImGui::BeginTabItem("Terrain",nullptr,requested_tab==0?ImGuiTabItemFlags_SetSelected:0)){tab=0;
                ImGui::Combo("Brush",&mode,"Raise\0Lower\0Flatten to picked height\0Smooth\0");ImGui::SliderFloat("Radius (m)",&radius,2,32);ImGui::SliderFloat("Strength (m)",&strength,.05f,4);
                if(mode==2){if(target&&!plane_set){plane=float(y);plane_set=true;}if(ImGui::InputFloat("Flatten height (m)",&plane,.25,2,"%.2f"))plane_set=true;if(ImGui::Button("Use picked ground height")&&target){plane=float(y);plane_set=true;}}
                if(ImGui::Button("Stamp terrain"))stamp();
                ImGui::Text("%zu /64 local 128m tiles",draft.terrain.size());
                ImGui::TextWrapped("The same edited heights drive terrain tiles, collision, scenery grounding and weather. Fixed demo foundations and navigation areas are protected.");
                if(ImGui::Button("Clear terrain edits")){checkpoint();draft.terrain.clear();changed(true);}
                ImGui::EndTabItem();}
            if(tool==1&&ImGui::BeginTabItem("Foliage",nullptr,requested_tab==1?ImGuiTabItemFlags_SetSelected:0)){tab=1;
                ImGui::Combo("Procedural layer",&vegetation_kind,"Grass\0Trees\0Rocks\0Ferns\0");ImGui::SliderFloat("Mask radius (m)",&radius,2,32);ImGui::SliderFloat("Remaining density",&mask,0,1);
                if(ImGui::Button("Paint density mask"))stamp();
                ImGui::Text("%zu /32 mask strokes",draft.stamps.size());
                ImGui::TextWrapped("Mask suppresses procedural roots. For extra vegetation choose tree/plant aliases in Objects and scatter them.");
                if(ImGui::Button("Clear density masks")){checkpoint();draft.stamps.clear();changed(true);}
                ImGui::EndTabItem();}
            if(tool==2&&ImGui::BeginTabItem("Objects",nullptr,requested_tab==2?ImGuiTabItemFlags_SetSelected:0)){tab=2;
                ImGui::InputText("Filter library",filter,sizeof(filter));
                if(ImGui::BeginCombo("Cooked model",world_scenery_alias(alias).name)){
                    for(size_t i=0;i<world_scenery_alias_count();i++){const auto a=world_scenery_alias(i);if(*filter&&!std::strstr(a.name,filter))continue;if(ImGui::Selectable(a.name,int(i)==alias))alias=int(i);}ImGui::EndCombo();}
                auto info=world_scenery_alias(alias);ImGui::SliderFloat("New scale",&scale,.15f,info.max_scale);ImGui::SliderFloat("New yaw",&yaw,-180,180);
                if(ImGui::Button("Place at target")&&target&&draft.placements.size()<world_studio::placement_limit){designer_place_asset(unsigned(alias));}
                ImGui::SliderInt("Scatter count",&scatter_count,1,32);ImGui::SliderFloat("Scatter radius",&radius,2,32);
                if(ImGui::Button("Scatter models")&&target){auto points=world_studio::scatter(info.name,x,z,radius,unsigned(scatter_count),scale,unsigned(draft.placements.size()));if(points.size()+draft.placements.size()<=world_studio::placement_limit){checkpoint();draft.placements.insert(draft.placements.end(),points.begin(),points.end());changed(true);}}
                if(ImGui::Button("Select nearest target root")&&target)select();
                if(ImGui::Button("Inspect selected asset")){inspect_asset(active_alias());requested_tab=9;}
                hierarchy_panel(cam);
                world_studio::Override *o=nullptr;world_studio::Placement *p=nullptr;
                if(selected>=0&&size_t(selected)<source.size()){
                    const auto &base=source[size_t(selected)];ImGui::Text("Source%u: %s",unsigned(selected),base.alias);
                    for(auto &value:draft.overrides)if(value.index==unsigned(selected))o=&value;
                    if(!o&&draft.overrides.size()<512&&ImGui::Button("Edit this source object")&&world_entity::editable(draft.entities,{world_entity::Kind::SourceModel,unsigned(selected)})){checkpoint();draft.source_signature=signature;draft.overrides.push_back({unsigned(selected),base.x,base.z,float(base.yaw*180/3.141592653589793),base.scale,false});o=&draft.overrides.back();}
                }
                if(selected_added>=0&&size_t(selected_added)<draft.placements.size()){p=&draft.placements[size_t(selected_added)];ImGui::Text("Added%u: %s",unsigned(selected_added),p->alias.c_str());}
                auto transform=[&](double &px,double &pz,float &pyaw,float &pscale){
                    edited|=ImGui::InputDouble("East (m)",&px,1,10,"%.2f");edited|=ImGui::InputDouble("South (m)",&pz,1,10,"%.2f");edited|=ImGui::SliderFloat("Yaw",&pyaw,-360,360);edited|=ImGui::SliderFloat("Scale",&pscale,.15f,4);structural=true;
                };
                ImGui::BeginDisabled((selected>=0&&!world_entity::editable(draft.entities,{world_entity::Kind::SourceModel,unsigned(selected)}))||(selected_added>=0&&!world_entity::editable(draft.entities,{world_entity::Kind::Placement,unsigned(selected_added)})));
                if(o){transform(o->x,o->z,o->yaw,o->scale);edited|=ImGui::InputFloat("Height offset (m)",&o->offset_y,.1,1);edited|=ImGui::Checkbox("Hidden",&o->hidden);}
                if(p){transform(p->x,p->z,p->yaw,p->scale);edited|=ImGui::InputFloat("Height offset (m)",&p->offset_y,.1,1);if(ImGui::Button("Delete added object")){checkpoint();world_studio::erase_placement(draft,unsigned(selected_added));selection_keys.clear();selected_added=-1;changed(true);}}
                ImGui::EndDisabled();
                selection_panel();
                ImGui::Text("%zu added /1024; %zu source overrides",draft.placements.size(),draft.overrides.size());
                if(!draft.overrides.empty()&&ImGui::Button("Clear source overrides")){bool locked=false;for(const auto &value:draft.overrides)locked|=!world_entity::editable(draft.entities,{world_entity::Kind::SourceModel,value.index});if(locked)message="Unlock all affected source entities before clearing their overrides.";else{checkpoint();draft.overrides.clear();draft.source_signature=signature;changed(true);}}
                ImGui::TextWrapped("Apply validates dry roots, authored trail clearance, per-model scale, burial, collision and instance budgets. Rejected drafts leave the previous scene active.");
                ImGui::EndTabItem();}
            if(tool==3&&ImGui::BeginTabItem("Lights",nullptr,requested_tab==3?ImGuiTabItemFlags_SetSelected:0)){tab=3;
                if(ImGui::Button("Add point light at target")&&target&&draft.lights.size()<world_studio::light_limit){checkpoint();world_studio::Light l;l.x=x;l.y=y+2.5;l.z=z;draft.lights.push_back(l);light=int(draft.lights.size()-1);}
                for(size_t i=0;i<draft.lights.size();i++){ImGui::PushID(int(i));if(ImGui::Selectable(("Light "+std::to_string(i+1)).c_str(),light==int(i)))light=int(i);ImGui::PopID();}
                if(light>=0&&size_t(light)<draft.lights.size()){ImGui::BeginDisabled(!world_entity::editable(draft.entities,{world_entity::Kind::Light,unsigned(light)}));auto &l=draft.lights[size_t(light)];
                    edited|=ImGui::InputDouble("Light east",&l.x,.25,2);edited|=ImGui::InputDouble("Light height",&l.y,.25,2);edited|=ImGui::InputDouble("Light south",&l.z,.25,2);edited|=ImGui::ColorEdit3("Color",l.color.data());edited|=ImGui::SliderFloat("Power",&l.power,0,40);edited|=ImGui::SliderFloat("Range (m)",&l.radius,.5,40);edited|=ImGui::SliderFloat("Cone (180=point)",&l.cone,5,180);edited|=ImGui::SliderFloat("Direction yaw",&l.yaw,-180,180);edited|=ImGui::SliderFloat("Direction pitch",&l.pitch,-90,90);edited|=ImGui::Checkbox("Flicker",&l.flicker);
                    if(ImGui::Button("Delete light")){if(production_remove({world_entity::Kind::Light,unsigned(light)}))light=-1;}ImGui::EndDisabled();}
                ImGui::TextWrapped("Point and spot lights share the existing eight-light budget with torches. These lights do not cast additional shadow maps. Nearby sources fade by contribution.");ImGui::EndTabItem();}
            if(tool==9&&ImGui::BeginTabItem("Assets",nullptr,requested_tab==9?ImGuiTabItemFlags_SetSelected:0)){tab=9;asset_panel(before);ImGui::EndTabItem();}
            if(tool==4&&ImGui::BeginTabItem("Materials",nullptr,requested_tab==4?ImGuiTabItemFlags_SetSelected:0)){tab=4;
                const char *name=selected>=0&&size_t(selected)<source.size()?source[size_t(selected)].alias:
                    selected_added>=0&&size_t(selected_added)<draft.placements.size()?draft.placements[size_t(selected_added)].alias.c_str():world_scenery_alias(alias).name;
                ImGui::TextWrapped("Model alias: %s (authored instances / LODs)",name);
                world_studio::Material *material=nullptr;for(auto &m:draft.materials)if(m.alias==name)material=&m;
                if(!material&&draft.materials.size()<64&&ImGui::Button("Override this material")){checkpoint();draft.materials.push_back({});material=&draft.materials.back();material->alias=name;}
                if(material){edited|=ImGui::ColorEdit3("Albedo tint",material->tint.data(),ImGuiColorEditFlags_Float);edited|=ImGui::SliderFloat("Roughness multiplier",&material->roughness,.25,2);edited|=ImGui::SliderFloat("Normal strength",&material->normal,0,2);edited|=ImGui::SliderFloat("UV tiling",&material->uv,.25,8);
                    if(ImGui::Button("Remove material override")){checkpoint();draft.materials.erase(std::remove_if(draft.materials.begin(),draft.materials.end(),[&](const auto &m){return m.alias==name;}),draft.materials.end());}
                }
                ImGui::TextWrapped("Source PBR textures retain their normal strength, color space and mipmaps. Overrides multiply the source; they cannot restore detail absent in a source texture.");ImGui::EndTabItem();}
            if(tool==5&&ImGui::BeginTabItem("Walls / Bridges",nullptr,requested_tab==5?ImGuiTabItemFlags_SetSelected:0)){tab=5;
                structure_spline_panel(edited,structural);if(structure_spline_mode==2){
                if(ImGui::Button("New placement spline")&&draft.splines.size()<16){checkpoint();world_studio::Spline s;s.alias=world_scenery_alias(alias).name;draft.splines.push_back(s);spline=int(draft.splines.size()-1);}
                for(size_t i=0;i<draft.splines.size();i++)if(ImGui::Selectable(("Spline "+std::to_string(i+1)).c_str(),spline==int(i)))spline=int(i);
                if(spline>=0&&size_t(spline)<draft.splines.size()){auto &s=draft.splines[size_t(spline)];ImGui::Text("%s | %zu points",s.alias.c_str(),s.points.size());
                    edited|=ImGui::SliderFloat("Spacing (m)",&s.spacing,.5,40);edited|=ImGui::SliderFloat("Spline model scale",&s.scale,.15,4);edited|=ImGui::Combo("Interpolation",&s.kind,"Linear\0Smooth Catmull-Rom\0");
                    if(ImGui::Button("Append target point")&&target&&s.points.size()<32){checkpoint();s.points.push_back({x,z});}
                    if(ImGui::Button("Remove last point")&&!s.points.empty()){checkpoint();s.points.pop_back();}
                    if(ImGui::Button("Bake models along spline")){std::vector<world_studio::Placement> values;if(world_studio::build_spline(s,values,message)&&values.size()+draft.placements.size()<=world_studio::placement_limit){checkpoint();draft.placements.insert(draft.placements.end(),values.begin(),values.end());changed(true);message="Spline baked into editable model instances.";}}
                }ImGui::TextWrapped("Choose a model in Objects first. Baking creates real grounded objects with collision and normal distance culling.");}ImGui::EndTabItem();}
            if(tool==6&&ImGui::BeginTabItem("Decals",nullptr,requested_tab==6?ImGuiTabItemFlags_SetSelected:0)){tab=6;
                if(ImGui::Button("Add projected surface patch")&&target&&draft.decals.size()<32){checkpoint();world_studio::Decal d;d.x=x;d.y=y;d.z=z;draft.decals.push_back(d);decal=int(draft.decals.size()-1);}
                for(size_t i=0;i<draft.decals.size();i++)if(ImGui::Selectable(("Patch "+std::to_string(i+1)).c_str(),decal==int(i)))decal=int(i);
                if(decal>=0&&size_t(decal)<draft.decals.size()){auto &d=draft.decals[size_t(decal)];edited|=ImGui::InputDouble("Patch east",&d.x,.25,2);edited|=ImGui::InputDouble("Patch height",&d.y,.25,2);edited|=ImGui::InputDouble("Patch south",&d.z,.25,2);edited|=ImGui::SliderFloat3("Box size",d.size.data(),.1,32);edited|=ImGui::SliderFloat("Patch rotation",&d.yaw,-180,180);edited|=ImGui::ColorEdit3("Patch color",d.color.data());edited|=ImGui::SliderFloat("Opacity",&d.opacity,0,1);edited|=ImGui::SliderFloat("Patch roughness",&d.roughness,0,1);edited|=ImGui::Combo("Pattern",&d.kind,"Soil\0Moss\0Worn road\0Wet stain\0");
                    if(ImGui::Button("Delete patch")){checkpoint();draft.decals.erase(draft.decals.begin()+decal);decal=-1;}}
                ImGui::TextWrapped("Procedural box projection onto terrain and rigid models, before lighting. Eight nearest patches render; no extra draw passes or texture allocations.");ImGui::EndTabItem();}
            if(tool>=10&&tool<=12)runtime_panel(cam);
            if(tool==13)graph_panel();
            if(tool==22)surface_panel();
            if(tool==23)paths_panel();
            if(tool==14)production_panel();
            if(tool==16)living_panel();
            if(tool==17)cinematic_panel(cam);
            if(tool==18)region_panel();
            if(tool==20)physics_panel(cam);
            if(tool==21)animation_panel(cam);
            if(tool==24)components_panel(cam);
            if(tool==25)sound_panel(cam);
            if(tool==26)weather_panel(cam);
            if(tool==19&&ImGui::BeginTabItem("Gameplay",nullptr,requested_tab==19?ImGuiTabItemFlags_SetSelected:0)){tab=19;production_gameplay_panel();ImGui::EndTabItem();}
            if(tool==15&&ImGui::BeginTabItem("Landscape",nullptr,requested_tab==15?ImGuiTabItemFlags_SetSelected:0)){tab=15;landscape_panel(cam);ImGui::EndTabItem();}
            if(tool==8&&ImGui::BeginTabItem("Shaders",nullptr,requested_tab==8?ImGuiTabItemFlags_SetSelected:0)){tab=8;
                ImGui::TextWrapped("GLSL source reload: terrain, models and water compile together. A link failure retains the previous scene and shader programs.");
                ImGui::TextWrapped("Sources: shaders/world_terrain.*, world_foliage.*, world_water.*. Edit these files in the source editor, then reload here.");
                if(ImGui::Button("Compile and reload material shaders")){reload_shaders=true;apply=true;}
                ImGui::TextWrapped("Material node authoring is in Graph; HDRI controls are in Atmosphere. Source reload remains available for shader development.");ImGui::EndTabItem();}
            if(tool==7&&ImGui::BeginTabItem("Atmosphere",nullptr,requested_tab==7?ImGuiTabItemFlags_SetSelected:0)){tab=7;auto &l=draft.look;
                edited|=ImGui::SliderFloat("Time of day",&l.hour,5.5,20);edited|=ImGui::SliderFloat("Sun bearing",&l.bearing,-180,180);edited|=ImGui::SliderFloat("Direct sunlight",&l.sun,.25,2);edited|=ImGui::SliderFloat("Sky irradiance",&l.sky,.25,2);edited|=ImGui::SliderFloat("Exposure",&l.exposure,.5,1.75);edited|=ImGui::SliderFloat("Detail sharpening",&l.sharpness,0,.65);edited|=ImGui::SliderFloat("Wind",&l.wind,0,1.5);
                if(ImGui::Button("Weather profiles and areas",{-1,0}))weather_open();
                environment_panel(before);
                ImGui::TextWrapped("The rainforest keeps its overcast canopy response. Apply invalidates cached local lighting so it can bake again during loading.");ImGui::EndTabItem();}
            ImGui::EndTabBar();
        }
        if(edited){
            if(!editing_)history.checkpoint(before);
            editing_=true;changed(structural);
        }else if(!ImGui::IsAnyItemActive()){if(editing_&&tool==2)designer_preview_object_edit();editing_=false;}
        if(!ImGui::IsAnyItemActive())runtime_editing=false;
        if(requested_tab==tool)requested_tab=-1;
    }
    void panel(const Camera &cam,float fps,size_t terrain_bytes,bool &graphics){
        project_poll();poll_jobs();project_service_save_all();alias=std::clamp(alias,0,int(world_scenery_alias_count())-1);
        if(designer_save_pending&&!dirty&&!apply){designer_save_pending=false;if(save()){if(designer_save_exit){project_quit_after_save=true;project_save_all();if(!project_save_job.valid())project_quit_after_save=designer_save_exit=false;}}else designer_save_exit=false;}
        if(designer_save_pending&&rejected>designer_save_rejected){designer_save_pending=designer_save_exit=false;}
        if(!open){weather_cancel();weather_finish_properties();sound_cancel();sound_finish_properties();npc_cancel();landscape_cancel();drag.active=false;if(preview_lod>=0){preview_lod=-1;reload_shaders=apply=true;}return;}
        designer_capture_legacy();designer_creation_origin={cam.x,world_atlas_height(cam.x,cam.z),cam.z};
        if(requested_tab>=0){tab=std::clamp(requested_tab,0,26);if(tab!=2)designer_tools=true;}
        requested_tab=-1;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,1);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,3);ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{12,9});
        const auto size=ImGui::GetIO().DisplaySize;const auto viewport=designer_viewport(size.x,size.y);
        ImGui::BeginDisabled(structure_cooking);designer_toolbar(cam,fps,graphics);ImGui::EndDisabled();
        if(!designer_immersive){
            if(designer_window("Scene",{0,viewport.y},{viewport.x,viewport.h},ImGuiWindowFlags_NoTitleBar)){ImGui::BeginDisabled(structure_cooking||project_save_job.valid()||project_restart_required);designer_outliner(cam);ImGui::EndDisabled();}
            ImGui::End();
            const float right=viewport.x+viewport.w;
            if(designer_window("Details",{right,viewport.y},{size.x-right,viewport.h},ImGuiWindowFlags_NoTitleBar)){
                const float half=ImGui::GetContentRegionAvail().x*.5f-4;
                if(ImGui::Selectable("Inspector",!designer_tools,0,{half,24}))designer_tools=false;
                ImGui::SameLine();if(ImGui::Selectable("Tools",designer_tools,0,{half,24}))designer_tools=true;
                ImGui::Separator();
                ImGui::BeginDisabled(structure_cooking||project_save_job.valid()||project_restart_required);if(!designer_tools)designer_inspector();else{
                    const char *workflows[]={"Terrain","Vegetation","Structures","Water","Living World"};
                    const int defaults[]={15,1,2,12,10};const int current=designer_workflow(tab);
                    for(int group=0;group<5;++group){if(group%2)ImGui::SameLine();
                        if(ImGui::Selectable(workflows[group],current==group,0,{ImGui::GetContentRegionAvail().x*(group%2?1.f:.49f),24})&&current!=group){tab=requested_tab=defaults[group];designer_preferences_changed();}}
                    ImGui::SetNextItemWidth(-1);
                    if(ImGui::BeginCombo("##authoring-tool",designer_tool_name(tab))){
                        for(int i=0;i<27;i++)if(designer_workflow(i)==designer_workflow(tab)&&ImGui::Selectable(designer_tool_name(i),tab==i)){tab=requested_tab=i;designer_preferences_changed();}
                        ImGui::EndCombo();}
                    if(ImGui::TreeNode("All 27 specialist tools")){for(int i=0;i<27;i++)if(ImGui::Selectable(designer_tool_name(i),tab==i)){tab=requested_tab=i;designer_preferences_changed();}ImGui::TreePop();}
                    ImGui::BeginChild("Tool properties",{0,0});designer_tool_contents(cam);ImGui::EndChild();
                }
                ImGui::EndDisabled();
            }ImGui::End();
            if(designer_content){
                const float y0=viewport.y+viewport.h,status=std::min(designer_layout.status,std::max(0.f,size.y-y0));
                if(designer_window("Content browser",{0,y0},{size.x,std::max(1.f,size.y-y0-status)},ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoScrollbar)){ImGui::BeginDisabled(structure_cooking||production_freeze());if(ImGui::Selectable("Models",!designer_audio_browser,0,{80,20}))designer_audio_browser=false;ImGui::SameLine();if(ImGui::Selectable("Audio",designer_audio_browser,0,{80,20}))designer_audio_browser=true;ImGui::Separator();if(designer_audio_browser)sound_browser();else designer_assets_panel();ImGui::EndDisabled();}
                ImGui::End();
            }
            auto splitter=[&](const char *id,ImVec2 pos,ImVec2 area,float &value,float sign,float lo,float hi){
                ImGui::SetNextWindowPos(pos);ImGui::SetNextWindowSize(area);ImGui::SetNextWindowBgAlpha(0);ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize,{1,1});
                ImGui::Begin(id,nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoScrollbar);
                designer_splitter("##resize",pos,area,value,sign,lo,hi);ImGui::End();ImGui::PopStyleVar(2);};
            splitter("Resize outliner",{viewport.x-3,viewport.y},{6,viewport.h},designer_layout.left,1,160,520);
            splitter("Resize details",{right-3,viewport.y},{6,viewport.h},designer_layout.right,-1,260,640);
            if(designer_content)splitter("Resize browser",{0,viewport.y+viewport.h-3},{size.x,6},designer_layout.bottom,-1,112,480);
        }
        const float statusheight=std::min(designer_layout.status,size.y);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{10,3});ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize,{1,1});
        if(designer_window("Workspace status",{0,size.y-statusheight},{size.x,statusheight},ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoScrollbar)){
            ImGui::TextColored(project_has_unsaved()||project_save_failed?ImVec4{.95f,.72f,.32f,1}:ImVec4{.4f,.8f,.67f,1},"%s",project_save_label());
            ImGui::SameLine();ImGui::TextDisabled("| %.1f FPS | %.0f MiB terrain |",fps,terrain_bytes/1048576.);ImGui::SameLine();ImGui::TextUnformatted(message.c_str());
            if(object_lighting_pending){ImGui::SameLine();ImGui::TextColored({.95f,.72f,.32f,1},"| cached lighting preview");}
        }ImGui::End();ImGui::PopStyleVar(2);
        ImGui::PopStyleVar(4);
        designer_capture_legacy();if(!structure_cooking)designer_asset_drop(cam);designer_capture_legacy();
        sound_input_consumed=weather_input_consumed=false;if(!structure_cooking){draw_gizmos(cam);sound_gizmo(cam);weather_gizmo(cam);}
        designer_paste_feedback(cam);
        auto *draw=ImGui::GetBackgroundDrawList();draw->PushClipRect({viewport.x,viewport.y},{viewport.x+viewport.w,viewport.y+viewport.h},true);
        if(!structure_cooking){runtime_overlay(cam);vfx_overlay(cam);weather_input(cam);weather_overlay(cam);sound_input(cam);sound_overlay(cam);cinematic_overlay(cam);surface_overlay(cam);structure_spline_overlay(cam);if(tab==15&&landscape_advanced)production_terrain_preview(cam);landscape_input(cam);}structure_overlay(cam);landscape_overlay(cam);designer_grid(cam);
        if(!structure_cooking&&tab!=15&&!structure_spline_active()&&!npc_input_consumed&&!weather_input_consumed&&!weather_placing&&!weather_moving&&!sound_input_consumed&&!sound_is_placing()&&!drag.active&&!designer_asset_placing()&&!designer_blocks_viewport()&&!ImGui::GetIO().WantCaptureMouse&&viewport.contains(ImGui::GetIO().MousePos.x,ImGui::GetIO().MousePos.y)&&ImGui::IsMouseClicked(0)){
            pick(cam,ImGui::GetIO().MousePos.x,ImGui::GetIO().MousePos.y);
            if(tab==0||tab==1)stamp();else if(tab==2)queue_pick(cam,ImGui::GetIO().MousePos,ImGui::GetIO().KeyCtrl,ImGui::GetIO().KeyAlt);else if(tab==11)vfx_world_click(cam);
        }
        if(target){
            ImVec2 center;if(project_point(cam,{x,y+.15,z},center)){draw->AddCircle(center,7,IM_COL32(255,209,90,255),16,2);}
            if(tab<2)for(int i=0;i<48;i++){double a=i*6.283185307/48,b=(i+1)*6.283185307/48;double ax=x+std::cos(a)*radius,az=z+std::sin(a)*radius,bx=x+std::cos(b)*radius,bz=z+std::sin(b)*radius;ImVec2 pa,pb;if(project_point(cam,{ax,world_atlas_height(ax,az)+.15,az},pa)&&project_point(cam,{bx,world_atlas_height(bx,bz)+.15,bz},pb))draw->AddLine(pa,pb,IM_COL32(255,209,90,200),2);}
        }draw->PopClipRect();
        weather_finish_properties();sound_reconcile_monitor();sound_finish_properties();designer_popups();designer_audit_panel();
        if(designer_preferences_dirty&&!ImGui::IsAnyItemActive()&&production_now()-designer_preferences_time>.75)designer_save_preferences();
        if(designer_save_pending)designer_save_rejected=rejected;
    }
    unsigned designer_save_rejected=0;
    std::uint64_t designer_baseline_revision=~std::uint64_t(0),designer_baseline_copies=0;int designer_baseline_tool=-1;
    bool editing_=false;
};
static bool studio_castle_culling=true;
static const world_studio::Document *studio_document=nullptr;
static void studio_decals(Program &p,const Camera &cam){
    std::array<std::pair<double,size_t>,32> order={};size_t n=0;
    if(studio_document)for(size_t i=0;i<studio_document->decals.size();i++){const auto &d=studio_document->decals[i];double distance=std::hypot(d.x-cam.x,d.z-cam.z);if(distance<160)order[n++]={distance,i};}
    std::sort(order.begin(),order.begin()+n);n=std::min<size_t>(8,n);p.i("uEditorDecalCount",int(n));
    if(!n)return;
    std::array<std::array<float,4>,8> position={},size={},color={},params={};
    std::array<float,3> minimum={FLT_MAX,FLT_MAX,FLT_MAX},maximum={-FLT_MAX,-FLT_MAX,-FLT_MAX};
    for(size_t i=0;i<n;i++){const auto &d=studio_document->decals[order[i].second];float a=d.yaw*.01745329252f;position[i]={float(d.x),float(d.y),float(d.z),std::cos(a)};size[i]={d.size[0]*.5f,d.size[1]*.5f,d.size[2]*.5f,std::sin(a)};color[i]={d.color[0],d.color[1],d.color[2],d.opacity};params[i]={float(d.kind),d.roughness,0,0};
        const float extent[]={std::abs(std::cos(a))*d.size[0]*.5f+std::abs(std::sin(a))*d.size[2]*.5f,d.size[1]*.5f,std::abs(std::sin(a))*d.size[0]*.5f+std::abs(std::cos(a))*d.size[2]*.5f};
        for(unsigned axis=0;axis<3;axis++){minimum[axis]=std::min(minimum[axis],position[i][axis]-extent[axis]-.01f);maximum[axis]=std::max(maximum[axis],position[i][axis]+extent[axis]+.01f);}
    }
    glUniform3fv(p.at("uEditorDecalMinimum"),1,minimum.data());glUniform3fv(p.at("uEditorDecalMaximum"),1,maximum.data());
    glUniform4fv(p.at("uEditorDecalPosition[0]"),int(n),position[0].data());glUniform4fv(p.at("uEditorDecalSize[0]"),int(n),size[0].data());glUniform4fv(p.at("uEditorDecalColor[0]"),int(n),color[0].data());glUniform4fv(p.at("uEditorDecalParams[0]"),int(n),params[0].data());
}
static void studio_lights(WorldTorchLights &lights,const Camera &cam,float time,const world_studio::Document *fx=nullptr,double fx_time=-1,int solo=-1){
    if(!fx)fx=studio_document;
    if(!studio_document||(studio_document->lights.empty()&&(!fx||fx->particles.empty())))return;
    struct Candidate {float score=0;unsigned index=0;bool editor=false,fire=false;};
    std::array<Candidate,64> values={};unsigned count=0;
    for(unsigned i=0;i<lights.count;i++){auto &p=lights.position_radius[i];float d=float(std::hypot(p[0]-cam.x,p[2]-cam.z));const auto &c=lights.color_intensity[i];float energy=c[3]*std::max({c[0],c[1],c[2]});if(energy>0)values[count++]={energy/(4+d*d),i,false};}
    for(unsigned i=0;i<studio_document->lights.size();i++){if(!world_entity::visible(studio_document->entities,{world_entity::Kind::Light,i}))continue;const auto l=world_apartment_window_light::incident(studio_document->lights[i]);float d=float(std::hypot(l.x-cam.x,l.z-cam.z));float energy=l.power*std::max({l.color[0],l.color[1],l.color[2]});if(d<l.radius+50&&energy>0)values[count++]={energy/(4+d*d),i,true};}
    for(unsigned i=0;i<fx->particles.size();i++){if((solo>=0&&unsigned(solo)!=i)||!world_entity::visible(fx->entities,{world_entity::Kind::Particle,i}))continue;const auto &p=fx->particles[i];if(p.kind!=2||!p.enabled||p.power<=0||p.light<=0)continue;float d=float(std::hypot(p.x-cam.x,p.z-cam.z));if(d<60)values[count++]={p.light/(4+d*d),i,false,true};}
    std::sort(values.begin(),values.begin()+count,[](const auto &a,const auto &b){return a.score==b.score?std::tie(a.editor,a.index)<std::tie(b.editor,b.index):a.score>b.score;});
    WorldTorchLights result;
    std::array<float,64> scores={};for(unsigned i=0;i<count;i++)scores[i]=values[i].score;const auto weights=world_studio::light_weights(scores,count);
    for(unsigned k=0;k<std::min(count,8u);k++){
        const auto &c=values[k];const unsigned i=result.count++;const float fade=weights[k];
        if(c.fire){const auto &p=fx->particles[c.index];float r=8+2*p.height;result.position_radius[i]={float(p.x),float(p.y+p.height*.5),float(p.z),r};result.color_intensity[i]={1,.50f,.18f,p.light*world_torch_flicker(fx_time>=0?fx_time:time,p.seed)};result.plane[i]={0,0,0,1};result.bounds[i]={float(p.x-r),float(p.z-r),float(p.x+r),float(p.z+r)};result.heights[i]={float(p.y-r),float(p.y+r)};}
        else if(!c.editor){result.position_radius[i]=lights.position_radius[c.index];result.color_intensity[i]=lights.color_intensity[c.index];result.plane[i]=lights.plane[c.index];result.bounds[i]=lights.bounds[c.index];result.heights[i]=lights.heights[c.index];result.direction_cone[i]=lights.direction_cone[c.index];}
        else{const auto l=world_apartment_window_light::night_adjusted(studio_document->lights[c.index]);result.position_radius[i]={float(l.x),float(l.y),float(l.z),l.radius};float flicker=l.flicker?world_torch_flicker(time,91800+c.index):1;result.color_intensity[i]={l.color[0],l.color[1],l.color[2],l.power*flicker};result.plane[i]={0,0,0,1};result.bounds[i]={float(l.x-l.radius),float(l.z-l.radius),float(l.x+l.radius),float(l.z+l.radius)};result.heights[i]={float(l.y-l.radius),float(l.y+l.radius)};if(l.cone<179.5){const float a=l.yaw*.01745329252f,b=l.pitch*.01745329252f;result.direction_cone[i]={std::sin(a)*std::cos(b),std::sin(b),-std::cos(a)*std::cos(b),std::cos(l.cone*.5f*.01745329252f)};}}
        if(c.editor&&!c.fire){
            const auto &source=studio_document->lights[c.index];
            if(const auto *portal=world_apartment_window_light::match(source)){
                const auto l=world_apartment_window_light::incident(source);
                result.position_radius[i]={float(l.x),float(l.y),float(l.z),l.radius};
                result.color_intensity[i]={l.color[0],l.color[1],l.color[2],l.power};
                const float a=l.yaw*.01745329252f,b=l.pitch*.01745329252f;
                result.direction_cone[i]={std::sin(a)*std::cos(b),std::sin(b),-std::cos(a)*std::cos(b),std::cos(l.cone*.5f*.01745329252f)};
                result.bounds[i]=portal->bounds;result.heights[i]={600.f,602.72f};
                // Restrict the key to the inside half-space of its actual pane.
                result.plane[i]=portal->east?std::array<float,4>{1,0,0,float(-portal->socket[0]+.22)}:
                    std::array<float,4>{0,0,-1,float(portal->socket[2]+.22)};
            }
        }
        result.color_intensity[i][3]*=fade;
        for(unsigned axis=0;axis<3;axis++){float lo=result.position_radius[i][axis]-result.position_radius[i][3],hi=result.position_radius[i][axis]+result.position_radius[i][3];result.cluster_min[axis]=i?std::min(result.cluster_min[axis],lo):lo;result.cluster_max[axis]=i?std::max(result.cluster_max[axis],hi):hi;}
    }lights=result;
}

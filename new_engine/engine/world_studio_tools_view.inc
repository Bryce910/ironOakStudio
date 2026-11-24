    // Render-thread UI state; CPU jobs own copied paths/poses, never GL objects.
    WorldScenery *scene_ptr=nullptr;
    std::vector<world_studio::Pose> source_poses,applied_poses;
    std::vector<unsigned> selection_keys;std::vector<size_t> applied_records;int preview_lod=-1;
    std::uint64_t revision=0;
    std::uint64_t object_preview_revision=~std::uint64_t(0);
    std::vector<WorldSceneryEditorTransform> object_preview_transforms;
    WorldScenery *object_preview_scene=nullptr;
    bool object_lighting_pending=false;
    unsigned fast_move_commits=0;
    void collect_object_preview(bool gizmo,bool changed_only=false){
        object_preview_transforms.clear();
        auto add=[&](unsigned key){
            if(key>=applied_records.size()||applied_records[key]==SIZE_MAX)return;
            world_studio::Pose p;if(!pose(key,p))return;
            const auto &old=applied_poses[key];
            if(changed_only&&p.root.x==old.root.x&&p.root.z==old.root.z&&std::abs(p.root.y-old.root.y)<1e-6&&std::abs(p.yaw-old.yaw)<1e-6&&p.scale==old.scale&&std::abs(p.pitch-old.pitch)<1e-6&&std::abs(p.roll-old.roll)<1e-6&&p.sx==old.sx&&p.sy==old.sy&&p.sz==old.sz&&p.offset_y==old.offset_y)return;
            object_preview_transforms.push_back({applied_records[key],p.root.x,p.root.z,float(p.root.y),float(p.yaw),float(p.scale),float(p.pitch),float(p.roll),float(p.sx),float(p.sy),float(p.sz),p.offset_y});
        };
        if(gizmo){for(unsigned key:drag.keys)add(key);}
        else{for(const auto &o:draft.overrides)add(o.index);for(unsigned i=0;i<draft.placements.size();i++)add(8192+i);}
    }
    void clear_object_preview(WorldScenery *scene){
        world_scenery_editor_clear(scene);object_preview_scene=nullptr;object_preview_revision=~std::uint64_t(0);
    }
    void sync_object_preview(WorldScenery *scene){
        if(!open||production_playing()||production_freeze()||(!drag.active&&!designer_edit_scope.active()&&!(editing_&&tab==2))){
            if(object_preview_scene)clear_object_preview(scene);
            return;
        }
        if(object_preview_scene==scene&&object_preview_revision==revision)return;
        if(!components_scene_prevalidate(draft,message)){clear_object_preview(scene);return;}
        // Inspector hierarchy edits bake descendant world poses into these
        // bounded records. Do not scan the thousands of untouched base models.
        collect_object_preview(drag.active);
        if(world_scenery_editor_transforms(scene,object_preview_transforms.data(),object_preview_transforms.size())){
            object_preview_scene=scene;object_preview_revision=revision;
        }else{clear_object_preview(scene);message="Could not preview this object transform. Previous scene preserved.";}
    }
    bool try_fast_object_move(WorldScenery *scene){
        if(!designer_move_fast_candidate)return false;
        designer_move_fast_candidate=false;
        if(!designer_object_preview||!production_can_save()||reload_shaders||production_restore||draft.placements.size()!=live.placements.size())return false;
        const auto started=std::chrono::steady_clock::now();std::string error;
        if(!production_sync(error)||!world_studio::valid(draft,error)||!components_scene_prevalidate(draft,error))return false;
        // Async imports or another tool can finish during a gesture. Compare
        // the complete canonical document with only pose records normalized;
        // never drop unrelated edits when publishing this narrow transaction.
        for(const auto &old:live.entities.nodes){const auto *n=world_entity::find(draft.entities,old.id);if(!n||n->parent!=old.parent||n->name!=old.name||!(n->binding==old.binding)||n->source_key!=old.source_key||n->visible!=old.visible||n->locked!=old.locked)return false;}
        for(const auto &n:draft.entities.nodes)if(!world_entity::find(live.entities,n.id)&&(n.binding.kind!=world_entity::Kind::SourceModel||n.parent||!n.visible||n.locked))return false;
        for(size_t i=0;i<live.placements.size();i++){const auto &a=live.placements[i],&b=draft.placements[i];if(a.alias!=b.alias||a.burial!=b.burial||a.elevation!=b.elevation)return false;}
        for(const auto &a:live.overrides)if(std::none_of(draft.overrides.begin(),draft.overrides.end(),[&](const auto &b){return a.index==b.index;}))return false;
        for(const auto &b:draft.overrides){const auto a=std::find_if(live.overrides.begin(),live.overrides.end(),[&](const auto &v){return v.index==b.index;});if(b.hidden!=(a==live.overrides.end()?false:a->hidden))return false;}
        auto remainder=draft;remainder.entities=live.entities;remainder.placements=live.placements;remainder.overrides=live.overrides;remainder.source_signature=live.source_signature;
        std::string expected,actual;
        if(!world_studio::encode(live,expected,error)||!world_studio::encode(remainder,actual,error)||expected!=actual)return false;
        collect_object_preview(false,true);
        if(!world_scenery_editor_commit_moves(scene,object_preview_transforms.data(),object_preview_transforms.size()))return false;
        live.placements=draft.placements;live.overrides=draft.overrides;live.entities=draft.entities;live.source_signature=draft.source_signature;
        for(const auto &t:object_preview_transforms)for(unsigned key=0;key<applied_records.size();key++)if(applied_records[key]==t.record){
            auto &p=applied_poses[key];p.root={t.x,t.y,t.z};p.yaw=t.yaw;p.offset_y=t.offset_y;break;
        }
        object_preview_scene=nullptr;object_preview_revision=~std::uint64_t(0);object_lighting_pending=true;
        apply=dirty=rebuild=false;++applied;++fast_move_commits;++terrain_rebuild_skipped;
        last_apply_terrain=last_apply_masks=false;production_applied();
        if(!components_scene_applied())components_ready=false;
        last_apply_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
        message="Move committed. Cached lighting is in preview; Apply refreshes it. Save keeps the scene.";return true;
    }
    struct PickResult {unsigned key=0;bool hit=false,append=false;std::uint64_t revision=0;std::string alias,path;std::shared_ptr<const world_assets::Report> report;};
    world_editor_pick::Index picking_index;
    std::uint64_t picking_registry=0,picking_inspection=0;
    std::vector<unsigned> picking_broadphase,picking_candidates;
    std::shared_ptr<world_editor_pick::MeshCache> picking_cache=std::make_shared<world_editor_pick::MeshCache>();
    unsigned picking_exact_checks=0,picking_index_nodes=0,picking_index_rebuilds=0;
    std::uint64_t picking_cache_hits=0,picking_cache_misses=0;
    std::future<PickResult> picking;
    std::future<world_assets::Report> inspecting;
    std::future<world_assets::ImportResult> importing;
    std::shared_ptr<const world_assets::Report> inspected;
    std::string inspected_alias,inspected_path,inspection_pending_alias,inspection_pending_path;
    world_studio::Asset import_definition;
    std::string import_cooker,project_directory;
    char import_sources[3][1024]={},import_alias[96]="user_model",group_name[65]="New group";
    float import_units=1;int import_collision=1,import_vegetation=0;
    bool import_replace=false,group_pick=true,snap=false;
    float move_snap=.25f,rotate_snap=15,scale_snap=.1f;
    int gizmo_mode=0,requested_tab=2;unsigned gizmo_drags=0,gizmo_finishes=0,pick_requests=0,mesh_hits=0,ui_undos=0;unsigned qa_logged_hit=UINT_MAX;ImVec2 undo_screen={},select_mode_screen={},move_mode_screen={};
    struct Drag {bool active=false,modified=false,before_dirty=false,before_rebuild=false,before_saved=false;int axis=0;ImVec2 start;world_studio::Vec pivot;float length=1;world_studio::Document before;std::vector<unsigned> keys;world_studio::Vec last_delta;double last_angle=0,last_factor=1;world_entity::Id issued=1;} drag;
    void restore_designer_drag(){
        // Preview IDs cannot be reused, but cancellation is not a new edit.
        const auto issued=std::max({drag.issued,draft.entities.next_id,production_id_highwater});
        draft=drag.before;draft.entities.next_id=std::max(draft.entities.next_id,issued);
        production_id_highwater=std::max(production_id_highwater,issued);
        dirty=drag.before_dirty;rebuild=drag.before_rebuild;production_project_saved=drag.before_saved;
        revision++;entity_world_revision=~std::uint64_t(0);
    }
    void cancel_designer_drag(){if(!drag.active)return;restore_designer_drag();drag.active=false;message="Transform cancelled.";}
    bool designer_drag_changed(const world_studio::Document &next)const{
        for(unsigned key:drag.keys){world_studio::Pose a,b;if(!world_studio::effective_pose(drag.before,source_poses,key,a)||!world_studio::effective_pose(next,source_poses,key,b))return true;
            if(a.root.x!=b.root.x||a.root.y!=b.root.y||a.root.z!=b.root.z||a.yaw!=b.yaw||a.scale!=b.scale||a.offset_y!=b.offset_y)return true;}
        return false;
    }
    bool configure(const world_studio::Document &doc,std::string &error){
        std::array<WorldSceneryDefinition,WORLD_CUSTOM_ASSET_LIMIT> values={};size_t n=0;
        for(const auto &a:doc.assets)values[n++]={a.alias.c_str(),a.stem.c_str(),a.lods,a.collision,a.vegetation,a.distance,a.split};
        char detail[256]={};const bool ok=world_scenery_configure(values.data(),n,detail,sizeof detail);if(!ok)error=detail;return ok;
    }
    static bool source_bound(const world_studio::Document &doc){
        if(!doc.overrides.empty())return true;
        for(const auto &n:doc.entities.nodes)if(n.binding.kind==world_entity::Kind::SourceModel)return true;
        for(const auto &g:doc.groups)for(unsigned key:g.members)if(key<8192)return true;
        return false;
    }
    void choose(unsigned key,bool append=false,bool expand_group=true){
        std::vector<unsigned> keys={key};if(group_pick&&expand_group)for(const auto &g:draft.groups)if(std::find(g.members.begin(),g.members.end(),key)!=g.members.end()){keys=g.members;break;}
        if(!append)selection_keys.clear();
        for(unsigned value:keys){auto at=std::find(selection_keys.begin(),selection_keys.end(),value);if(append&&at!=selection_keys.end())selection_keys.erase(at);else if(at==selection_keys.end()&&selection_keys.size()<128)selection_keys.push_back(value);}
        selected=selected_added=-1;if(!selection_keys.empty()){unsigned active=selection_keys.back();if(active<8192)selected=int(active);else selected_added=int(active-8192);}
    }
    void rebuild_picking_index(){
        std::vector<world_editor_pick::Footprint> footprints;footprints.reserve(source_poses.size());
        for(unsigned i=0;i<source_poses.size();i++){
            auto base=source_poses[i];
            if(i<applied_poses.size()&&applied_poses[i].alias==base.alias){base.minimum=applied_poses[i].minimum;base.maximum=applied_poses[i].maximum;}
            world_editor_pick::Footprint bounds;if(world_editor_pick::footprint(i,base,bounds))footprints.push_back(bounds);
        }
        if(picking_index.rebuild(footprints)){picking_registry=world_scenery_alias_revision();picking_index_rebuilds++;}
    }
    void refresh_scene(WorldScenery *current){
        if(scene_ptr==current)return;
        scene_ptr=current;applied_poses.assign(8192+live.placements.size(),{});applied_records.assign(applied_poses.size(),SIZE_MAX);size_t record=0;
        auto capture_pose=[&](unsigned key){WorldSceneryPlacement a;const size_t index=record++;if(!world_scenery_placement(current,index,&a))return;applied_records[key]=index;auto &p=applied_poses[key];p.alias=a.alias;p.root={a.x,a.y,a.z};p.yaw=a.yaw;p.scale=a.scale;p.offset_y=a.offset_y;p.pitch=a.pitch;p.roll=a.roll;p.sx=a.sx;p.sy=a.sy;p.sz=a.sz;p.burial=a.burial;p.elevation=a.elevation;p.minimum={a.minimum[0],a.minimum[1],a.minimum[2]};p.maximum={a.maximum[0],a.maximum[1],a.maximum[2]};};
        for(unsigned i=0;i<source.size();i++){bool hidden=false;for(const auto &o:live.overrides)if(o.index==i){hidden=o.hidden;break;}if(!hidden&&world_entity::visible(live.entities,{world_entity::Kind::SourceModel,i}))capture_pose(i);}
        for(unsigned i=0;i<live.placements.size();i++)if(world_entity::visible(live.entities,{world_entity::Kind::Placement,i}))capture_pose(8192+i);
        rebuild_picking_index();
    }
    bool pose(unsigned key,world_studio::Pose &p,const world_studio::Document *document=nullptr)const{
        const auto &doc=document?*document:draft;
        if(!world_studio::effective_pose(doc,source_poses,key,p)||p.hidden)return false;
        const auto *committed=key<applied_poses.size()&&applied_poses[key].alias==p.alias?&applied_poses[key]:nullptr;
        if(committed){p.minimum=committed->minimum;p.maximum=committed->maximum;p.root.y=committed->root.y+p.offset_y-committed->offset_y;if(p.root.x!=committed->root.x||p.root.z!=committed->root.z)p.root.y+=world_atlas_height(p.root.x,p.root.z)-world_atlas_height(committed->root.x,committed->root.z);}
        else if(key>=8192){
            float lo[3],hi[3];if(world_scenery_alias_bounds(scene_ptr,p.alias.c_str(),lo,hi)){p.minimum={lo[0],lo[1],lo[2]};p.maximum={hi[0],hi[1],hi[2]};}
            else if(inspected&&inspected_alias==p.alias&&inspected->supported){p.minimum=inspected->minimum;p.maximum=inspected->maximum;}
            else{p.minimum={-.5,0,-.5};p.maximum={.5,1,.5};}
            p.root.y+=world_atlas_height(p.root.x,p.root.z)-p.burial+p.elevation;
        }
        return true;
    }
    std::string model_path(const std::string &name,unsigned lod=0)const{
        for(size_t i=0;i<world_scenery_alias_count();i++){auto a=world_scenery_alias(i);if(name!=a.name)continue;return project_directory+"/models/"+a.stem+(a.tree?(lod==0?"_near.glb":(lod==1?"_mid.glb":"_far.glb")):(lod?"_mid.glb":(a.named_near?"_near.glb":".glb")));}
        for(const auto &a:draft.assets)if(a.alias==name)return project_directory+"/models/"+a.stem+(a.vegetation==2?"_near.glb":".glb");
        return {};
    }
    bool project_point(const Camera &cam,world_studio::Vec p,ImVec2 &screen)const{
        const V3 f=cam.forward(),r=norm(cross(f,{0,1,0})),u=cross(r,f),d={float(p.x-cam.x),float(p.y-cam.y),float(p.z-cam.z)};const float depth=dot(d,f);if(depth<.25)return false;
        const auto size=ImGui::GetIO().DisplaySize;const auto viewport=designer_viewport(size.x,size.y);
        return world_designer_viewport::project(viewport,depth,dot(d,r),dot(d,u),std::tan(cam.fov*.5f),screen.x,screen.y);
    }
    world_studio::Ray mouse_ray(const Camera &cam,ImVec2 mouse)const{
        const auto size=ImGui::GetIO().DisplaySize;const auto viewport=designer_viewport(size.x,size.y);float nx=0,ny=0;world_designer_viewport::ndc(viewport,mouse.x,mouse.y,nx,ny);
        V3 f=cam.forward(),r=norm(cross(f,{0,1,0})),u=cross(r,f);auto d=norm(add(f,add(mul(r,nx*world_designer_viewport::aspect(viewport)*std::tan(cam.fov*.5f)),mul(u,ny*std::tan(cam.fov*.5f)))));return {{cam.x,cam.y,cam.z},{d.x,d.y,d.z}};
    }
    void queue_pick(const Camera &cam,ImVec2 mouse,bool append){
        if(picking.valid()||drag.active)return;
        struct Candidate {double distance;unsigned key;world_studio::Pose pose;std::string path;};
        std::vector<Candidate> candidates;const auto ray=mouse_ray(cam,mouse);double distance=0;
        if(picking_registry!=world_scenery_alias_revision()||picking_index.size()!=source_poses.size())rebuild_picking_index();
        world_editor_pick::QueryStats query;picking_index.query(ray,700,picking_broadphase,&query);picking_index_nodes=query.nodes;picking_exact_checks=0;
        std::vector<bool> overridden(source.size(),false);for(const auto &o:draft.overrides)if(o.index<overridden.size())overridden[o.index]=true;
        auto test=[&](unsigned key){picking_exact_checks++;world_studio::Pose p;if(pose(key,p)&&world_studio::ray_bounds(ray,p,distance)&&distance<700)candidates.push_back({distance,key,std::move(p),{}});};
        for(unsigned key:picking_broadphase)if(key<overridden.size()&&!overridden[key])test(key);
        for(const auto &o:draft.overrides)if(o.index<source.size())test(o.index);
        for(unsigned i=0;i<draft.placements.size();i++)test(8192+i);
        std::sort(candidates.begin(),candidates.end(),[](const auto &a,const auto &b){return a.distance==b.distance?a.key<b.key:a.distance<b.distance;});if(candidates.size()>16)candidates.resize(16);
        picking_candidates.clear();for(const auto &candidate:candidates)picking_candidates.push_back(candidate.key);
        for(auto &candidate:candidates)candidate.path=model_path(candidate.pose.alias);
        if(path.rfind("/tmp/",0)==0&&std::getenv("VALWOOD_QA_STUDIO_INTERACTION")){
            std::printf("[Studio interaction] candidates %zu ray %.5f %.5f %.5f mode%d\n",candidates.size(),ray.direction.x,ray.direction.y,ray.direction.z,gizmo_mode);
            for(const auto &c:candidates)std::printf("[Studio interaction] candidate %u %s %.3f %.3f %.3f box %.3f %.3f %.3f / %.3f %.3f %.3f\n",c.key,c.pose.alias.c_str(),c.pose.root.x,c.pose.root.y,c.pose.root.z,c.pose.minimum.x,c.pose.minimum.y,c.pose.minimum.z,c.pose.maximum.x,c.pose.maximum.y,c.pose.maximum.z);
            std::fflush(stdout);
        }
        const auto request=revision;const auto cache=picking_cache;const world_editor_pick::Epoch epoch={world_scenery_alias_revision(),picking_inspection};
        pick_requests++;message="Resolving mesh selection...";
        picking=std::async(std::launch::async,[candidates=std::move(candidates),ray,append,request,cache,epoch](){
            PickResult result;result.append=append;result.revision=request;double best=701;std::string path;std::shared_ptr<const world_assets::Report> report;
            for(const auto &c:candidates){if(c.distance>best)break;if(path!=c.path||!report){path=c.path;report=cache->get(path,epoch,[](const auto &p){return world_assets::inspect(p,true);});}
                double hit=0;bool accurate=report->supported&&!report->geometry.empty();if(accurate?!world_assets::ray_mesh(*report,ray,c.pose,hit):!world_studio::ray_bounds(ray,c.pose,hit))continue;
                if(hit<best){best=hit;result.hit=true;result.key=c.key;result.alias=c.pose.alias;result.path=path;result.report=report;}
            }return result;
        });
    }
    void poll_jobs(){
        using namespace std::chrono_literals;
        if(picking.valid()&&picking.wait_for(0ms)==std::future_status::ready){auto result=picking.get();{const auto stats=picking_cache->stats();picking_cache_hits=stats.hits;picking_cache_misses=stats.misses;}if(result.revision!=revision)message="Scene changed while selecting; click again.";else if(result.hit){mesh_hits++;choose(result.key,result.append);inspected=result.report;inspected_alias=result.alias;inspected_path=result.path;message=inspected->supported?"Mesh selected. Ctrl-click toggles selection; handles stage a transform.":"Bounds selected; inspect the unsupported source before reimport.";}else{if(!result.append)selection_keys.clear();selected=selected_added=-1;message="No authored mesh hit. Procedural vegetation uses the Foliage masks.";}}
        if(inspecting.valid()&&inspecting.wait_for(0ms)==std::future_status::ready){inspected=std::make_shared<world_assets::Report>(inspecting.get());inspected_alias=inspection_pending_alias;inspected_path=inspection_pending_path;message=inspected->supported?"Asset inspection ready.":inspected->error;}
        if(open&&importing.valid()&&importing.wait_for(0ms)==std::future_status::ready){auto result=importing.get();
            if(result.ok){for(unsigned lod=0;lod<import_definition.lods;lod++){const std::string path=project_directory+"/models/"+result.stem+(lod==0?".glb":(lod==1?"_mid.glb":"_far.glb"));auto *probe=world_model_load(path.c_str());if(!probe){result.ok=false;result.error="GPU material/geometry validation failed for LOD"+std::to_string(lod)+"; previous definition preserved.";break;}world_model_free(probe);}texture_cache_trim_source_pages();}
            if(!result.ok)message=result.error;else{picking_inspection++;checkpoint();import_definition.stem=result.stem;auto at=std::find_if(draft.assets.begin(),draft.assets.end(),[&](const auto &a){return a.alias==import_definition.alias;});if(at==draft.assets.end())draft.assets.push_back(import_definition);else *at=import_definition;changed(true);inspected=std::make_shared<world_assets::Report>(std::move(result.report));inspected_alias=import_definition.alias;inspected_path=project_directory+"/models/"+result.stem+".glb";message="Imported revision staged. Apply validates GPU materials, LODs and collision; Save keeps it.";}}
    }
    void inspect_asset(const std::string &name,unsigned lod=0){
        if(inspecting.valid())return;
        picking_inspection++;inspection_pending_alias=name;inspection_pending_path=model_path(name,lod);const auto path=inspection_pending_path;
        inspecting=std::async(std::launch::async,[path](){return world_assets::inspect(path);});
    }
    void selection_panel(){
        ImGui::Text("%zu selected | viewport handles",selection_keys.size());
        ImGui::Checkbox("Snap (also hold Ctrl)",&snap);if(snap){ImGui::InputFloat("Move step (m)",&move_snap,.05,.25);ImGui::InputFloat("Yaw step (degrees)",&rotate_snap,1,5);move_snap=std::clamp(move_snap,.01f,10.f);rotate_snap=std::clamp(rotate_snap,1.f,90.f);}
        ImGui::Checkbox("Select saved groups as a unit",&group_pick);
        if(ImGui::Button("Duplicate selection")&&!selection_keys.empty()){auto next=draft;std::vector<unsigned> copies;if(world_studio::duplicate_selection(next,source_poses,selection_keys,{2,0,0},copies,message)){checkpoint();draft=std::move(next);selection_keys=std::move(copies);selected=-1;selected_added=int(selection_keys.back()-8192);changed(true);}}
        ImGui::SameLine();if(ImGui::Button("Delete / hide selection")&&!selection_keys.empty()){auto next=draft;if(world_studio::delete_selection(next,source_poses,selection_keys,message)){checkpoint();draft=std::move(next);selection_keys.clear();selected=selected_added=-1;changed(true);}}
        ImGui::InputText("Group name",group_name,sizeof group_name);
        if(ImGui::Button("Group selected")&&!selection_keys.empty()&&draft.groups.size()<32){checkpoint();draft.source_signature=signature;draft.groups.push_back({*group_name?group_name:"Group",selection_keys});changed();}
        for(size_t i=0;i<draft.groups.size();i++){ImGui::PushID(int(i));if(ImGui::Selectable(draft.groups[i].name.c_str(),selection_keys==draft.groups[i].members)){selection_keys=draft.groups[i].members;unsigned key=selection_keys.back();selected=key<8192?int(key):-1;selected_added=key>=8192?int(key-8192):-1;}ImGui::SameLine();if(ImGui::SmallButton("Ungroup")){checkpoint();draft.groups.erase(draft.groups.begin()+i);changed();ImGui::PopID();break;}ImGui::PopID();}
        ImGui::TextWrapped("Translation XYZ, yaw gizmos and uniform scale; use the Inspector for pitch, roll and axis scaling. The model follows edits in real time. Release finishes one undo step and updates collision/lighting. Fixed NPC/FX attachments remain separate.");
    }
    void draw_gizmos(const Camera &cam){
        if(tab!=2)return;
        const auto size=ImGui::GetIO().DisplaySize;const auto viewport=designer_viewport(size.x,size.y);
        if(selection_keys.empty())return;
        auto &io=ImGui::GetIO();auto *draw=ImGui::GetBackgroundDrawList();draw->PushClipRect({viewport.x,viewport.y},{viewport.x+viewport.w,viewport.y+viewport.h},true);
        struct ClipScope{ImDrawList *list;~ClipScope(){list->PopClipRect();}} clip_scope{draw};world_studio::Vec pivot={};size_t count=0;
        for(unsigned key:selection_keys){world_studio::Pose p;if(!pose(key,p))continue;pivot.x+=p.root.x;pivot.y+=p.root.y+std::min(2.,(p.maximum.y-p.minimum.y)*p.scale*.5);pivot.z+=p.root.z;count++;
            ImVec2 corners[8];bool visible[8]={};const auto matrix=vw_transform::matrix(vw_transform::pose(p));for(unsigned k=0;k<8;k++){const auto q=vw_transform::point(matrix,{k&1?p.maximum.x:p.minimum.x,k&2?p.maximum.y:p.minimum.y,k&4?p.maximum.z:p.minimum.z});visible[k]=project_point(cam,{q[0],q[1],q[2]},corners[k]);}for(unsigned k=0;k<8;k++)for(unsigned axis:{1u,2u,4u})if(!(k&axis)&&visible[k]&&visible[k|axis])draw->AddLine(corners[k],corners[k|axis],IM_COL32(255,205,76,210),1.5f);
        }
        if(!count)return;
        pivot.x/=count;pivot.y/=count;pivot.z/=count;
        if(drag.active)pivot=drag.pivot;
        const float length=drag.active?drag.length:float(std::clamp(std::hypot(std::hypot(pivot.x-cam.x,pivot.z-cam.z),pivot.y-cam.y)*.12,1.,15.));ImVec2 center;if(!project_point(cam,pivot,center))return;
        const ImU32 colors[]={IM_COL32(244,91,79,255),IM_COL32(111,220,105,255),IM_COL32(79,155,245,255)};int hover=-1;
        auto segment_distance=[&](ImVec2 a,ImVec2 b){float dx=b.x-a.x,dy=b.y-a.y,den=dx*dx+dy*dy;if(den<4)return 1e6f;float t=std::clamp(((io.MousePos.x-a.x)*dx+(io.MousePos.y-a.y)*dy)/den,0.f,1.f);return std::hypot(io.MousePos.x-a.x-dx*t,io.MousePos.y-a.y-dy*t);};
        if(gizmo_mode==0)for(int axis=0;axis<3;axis++){auto end=pivot;if(axis==0)end.x+=length;if(axis==1)end.y+=length;if(axis==2)end.z+=length;ImVec2 screen;if(!project_point(cam,end,screen))continue;draw->AddLine(center,screen,colors[axis],3);draw->AddCircleFilled(screen,5,colors[axis]);draw->AddText({screen.x+6,screen.y},colors[axis],axis==0?"X":(axis==1?"Y":"Z"));if(segment_distance(center,screen)<9)hover=axis;}

        if(!drag.active&&applied==2&&rejected==2&&qa_logged_hit!=mesh_hits&&path.rfind("/tmp/",0)==0&&std::getenv("VALWOOD_QA_STUDIO_INTERACTION")){
            ImVec2 end,body=center;auto point=pivot;point.x+=length;world_studio::Pose p;
            if(pose(selection_keys.back(),p)){double lx=(p.minimum.x+p.maximum.x)*p.scale*.5,lz=(p.minimum.z+p.maximum.z)*p.scale*.5,c=std::cos(p.yaw),s=std::sin(p.yaw);project_point(cam,{p.root.x+c*lx+s*lz,p.root.y+(p.minimum.y+p.maximum.y)*p.scale*.5,p.root.z-s*lx+c*lz},body);}
            if(project_point(cam,point,end)){ImVec2 start{center.x+(end.x-center.x)*.75f,center.y+(end.y-center.y)*.75f};std::printf("[Studio interaction] ready %u %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f\n",mesh_hits,start.x,start.y,body.x,body.y,undo_screen.x,undo_screen.y,select_mode_screen.x,select_mode_screen.y,move_mode_screen.x,move_mode_screen.y);std::fflush(stdout);qa_logged_hit=mesh_hits;}
        }
        if(gizmo_mode==1){for(int i=0;i<64;i++){double a=i*6.283185307/64,b=(i+1)*6.283185307/64;ImVec2 p,q;if(project_point(cam,{pivot.x+std::cos(a)*length,pivot.y,pivot.z+std::sin(a)*length},p)&&project_point(cam,{pivot.x+std::cos(b)*length,pivot.y,pivot.z+std::sin(b)*length},q)){draw->AddLine(p,q,colors[1],3);if(segment_distance(p,q)<9)hover=3;}}}
        if(gizmo_mode==2){ImVec2 handle{center.x+55,center.y-55};draw->AddLine(center,handle,IM_COL32(245,220,104,255),3);draw->AddRectFilled({handle.x-6,handle.y-6},{handle.x+6,handle.y+6},IM_COL32(245,220,104,255));if(segment_distance(center,handle)<9)hover=4;}
        if(!drag.active&&hover>=0&&viewport.contains(io.MousePos.x,io.MousePos.y)&&!designer_asset_placing()&&!vfx_placing&&!designer_blocks_viewport()&&!io.WantCaptureMouse&&ImGui::IsMouseClicked(0)){gizmo_drags++;drag.active=true;drag.modified=false;drag.before_dirty=dirty;drag.before_rebuild=rebuild;drag.before_saved=production_project_saved;drag.axis=hover;drag.start=io.MousePos;drag.pivot=pivot;drag.length=length;drag.before=draft;drag.keys=selection_keys;drag.last_delta={};drag.last_angle=0;drag.last_factor=1;drag.issued=draft.entities.next_id;message="Transform draft. Release to finish; Escape cancels; Apply commits.";}
        if(drag.active){
            if(ImGui::IsKeyPressed(ImGuiKey_Escape)){cancel_designer_drag();return;}
            if(!io.MouseDown[0]){gizmo_finishes++;if(drag.modified&&designer_drag_changed(draft)){history.checkpoint(drag.before);designer_move_fast_candidate=!drag.before_dirty&&drag.axis<4;drag.active=false;changed(true);designer_preview_object_edit();message=designer_object_preview?"Transform finished. Updating the scene.":"Transform staged. Apply updates the world.";}else{restore_designer_drag();drag.active=false;message="Transform unchanged.";}return;}
            world_studio::Vec delta={};double angle=0,factor=1;bool snapping=snap||io.KeyCtrl;const float dx=io.MousePos.x-drag.start.x,dy=io.MousePos.y-drag.start.y;
            if(drag.axis<3){auto end=drag.pivot;if(drag.axis==0)end.x+=drag.length;if(drag.axis==1)end.y+=drag.length;if(drag.axis==2)end.z+=drag.length;ImVec2 a,b;if(!project_point(cam,drag.pivot,a)||!project_point(cam,end,b))return;double ax=b.x-a.x,ay=b.y-a.y,den=ax*ax+ay*ay;if(den<4)return;double amount=(dx*ax+dy*ay)/den*drag.length;if(snapping)amount=std::round(amount/move_snap)*move_snap;if(drag.axis==0)delta.x=amount;if(drag.axis==1)delta.y=amount;if(drag.axis==2)delta.z=amount;
            }else if(drag.axis==3){world_studio::Vec a,b;if(world_studio::ray_plane(mouse_ray(cam,drag.start),drag.pivot,{0,1,0},a)&&world_studio::ray_plane(mouse_ray(cam,io.MousePos),drag.pivot,{0,1,0},b))angle=std::remainder(std::atan2(a.z-drag.pivot.z,a.x-drag.pivot.x)-std::atan2(b.z-drag.pivot.z,b.x-drag.pivot.x),6.283185307)*57.295779513;else angle=dx*.5;if(snapping)angle=std::round(angle/rotate_snap)*rotate_snap;
            }else{factor=std::exp(std::clamp(double(dx-dy)*.008,-3.,3.));if(snapping)factor=std::max(.1,std::round(factor/.1)*.1);}
            if(delta.x==drag.last_delta.x&&delta.y==drag.last_delta.y&&delta.z==drag.last_delta.z&&angle==drag.last_angle&&factor==drag.last_factor)return;
            drag.last_delta=delta;drag.last_angle=angle;drag.last_factor=factor;
            if(delta.x==0&&delta.y==0&&delta.z==0&&angle==0&&factor==1){restore_designer_drag();drag.modified=false;return;}
            auto next=drag.before;next.entities.next_id=std::max(next.entities.next_id,drag.issued);
            if(world_studio::transform_selection(next,source_poses,drag.keys,drag.pivot,delta,angle,factor,message)){
                next.source_signature=signature;
                if(!designer_drag_changed(next)){restore_designer_drag();drag.modified=false;return;}
                auto graph=next.entities;
                if(!world_authoring::synchronize(next,graph,identity_sources,production_ground,&next,message))return;
                next.entities=std::move(graph);
                if(!components_scene_prevalidate(next,message))return;
                drag.issued=std::max(drag.issued,next.entities.next_id);draft=std::move(next);drag.modified=true;
                // Only final release enters history/recovery. The render-only
                // instance preview follows this revision on the next frame.
                revision++;entity_world_revision=~std::uint64_t(0);
            }
        }
    }

    void sync_preview(WorldScenery *scene){
        size_t record=SIZE_MAX;if(open&&preview_lod>=0&&!selection_keys.empty()){unsigned key=selection_keys.back();if(key<applied_records.size())record=applied_records[key];}
        world_scenery_preview(scene,record,preview_lod);
    }

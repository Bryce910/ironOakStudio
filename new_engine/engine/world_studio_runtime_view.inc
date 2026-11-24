    WorldVillagePopulation *population_ptr=nullptr;
    std::vector<WorldSceneParticleEmitter> base_particles;
    std::vector<WorldTorchFlameEmitter> base_flames;
    std::vector<world_navigation::Grid> navigation_live;
    world_navigation::Grid navigation_preview;
    int actor_selection=-1,area_selection=-1,particle_selection=-1,water_selection=-1;
    unsigned runtime_commits=0;bool runtime_qa_ready=false;
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
    static bool nav_ground(void *,double px,double pz,double &height){height=world_atlas_walk_height(px,pz);return std::isfinite(height)&&height>.1&&world_atlas_sample(px,pz).land_weight>.75&&!world_atlas_local_water(px,pz,nullptr,nullptr);}
    static bool nav_clear(void *data,const world_navigation::Point &a,const world_navigation::Point &b,float radius){return world_scenery_can_move(static_cast<WorldScenery *>(data),a.x,a.z,b.x,b.z,float(std::min(a.y,b.y))+.01f,1.75f,radius);}
    world_navigation::Query nav_query(WorldScenery *scene)const{return {scene,nav_ground,nav_clear};}
    bool prepare_runtime(const world_studio::Document &doc,WorldScenery *scene,RuntimePrepared &next,std::string &error){
        next={};auto query=nav_query(scene);
        for(const auto &w:doc.waters)if(w.source==3&&w.enabled){
            if(w.level-world_atlas_height(w.x,w.z)<.08){error="Pond center must contain at least8cm of water. Sculpt a basin or raise its plane.";return false;}
            for(unsigned i=0;i<32;i++){double a=i*6.283185307/32;if(world_atlas_height(w.x+std::cos(a)*w.radius_x,w.z+std::sin(a)*w.radius_z)<w.level+.035){error="Pond needs a dry rim: lower the water or sculpt a closed basin. No floating water edge is committed.";return false;}}
        }
        for(const auto &area:doc.navigation){world_navigation::Grid grid;if(!world_navigation::bake(area,query,grid,error))return false;next.grids.push_back(std::move(grid));}
        if(!doc.actors.empty()&&!population_ptr){error="Character pose library unavailable";return false;}
        for(const auto &a:doc.actors){auto &c=next.actors[next.actor_count++];c.source=a.source;c.appearance=a.appearance;c.enabled=a.enabled&&world_entity::visible(doc.entities,{world_entity::Kind::Actor,next.actor_count-1});c.speed=a.speed;c.dwell=a.dwell;c.yaw=a.yaw*.01745329252f;std::snprintf(c.name.data(),c.name.size(),"%s",a.name.c_str());
            if(a.points.size()>1&&!world_village_pose_archetypes[a.appearance].walk_count){error="This appearance has idle poses only. Choose a walking appearance or a single stationary point.";return false;}
            std::vector<world_navigation::Point> route;if(!a.enabled){route.push_back({a.points[0][0],world_atlas_walk_height(a.points[0][0],a.points[0][1]),a.points[0][1]});}
            else if(!world_navigation::path(a.navigation>=0?&next.grids[unsigned(a.navigation)]:nullptr,query,a.points,a.loop,a.navigation>=0?doc.navigation[unsigned(a.navigation)].radius:.32f,route,error)){error=a.name+": "+error;return false;}
            c.count=unsigned(route.size());std::copy(route.begin(),route.end(),c.points.begin());
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
        if(!world_scene_particles_set_emitters(particles,next.particles.data(),next.particles.size())||!world_torch_flames_set_emitters(flames,next.flames.data(),next.flames.size()))return false;
        navigation_live=std::move(next.grids);runtime_commits++;return true;
    }
    void runtime_panel(const Camera &cam){
        if(tab==10&&ImGui::BeginTabItem("NPC / Nav",nullptr,requested_tab==10?ImGuiTabItemFlags_SetSelected:0)){tab=10;
            if(ImGui::Button("Add resident at target")&&target&&draft.actors.size()<32){checkpoint();world_studio::Actor a;a.points.push_back({x,z});draft.actors.push_back(a);actor_selection=int(draft.actors.size()-1);changed();}
            if(ImGui::Button("Edit nearest existing resident")&&target&&population_ptr){double best=12;int found=-1;WorldVillageResident resident;for(unsigned i=0;i<world_village_resident_count;i++){WorldVillageResident r;if(world_village_population_resident(population_ptr,i,&r)&&std::hypot(r.x-x,r.z-z)<best){best=std::hypot(r.x-x,r.z-z);found=int(i);resident=r;}}if(found<0)message="No village resident within12m of the target.";else{auto at=std::find_if(draft.actors.begin(),draft.actors.end(),[&](const auto &a){return a.source==found;});if(at!=draft.actors.end())actor_selection=int(at-draft.actors.begin());else if(draft.actors.size()<32){checkpoint();world_studio::Actor a;a.source=found;a.name=resident.name;for(unsigned k=0;k<std::size(world_village_pose_archetypes);k++)if(resident.archetype&&std::strcmp(resident.archetype,world_village_pose_archetypes[k].name)==0)a.appearance=k;a.points.push_back({resident.x,resident.z});draft.actors.push_back(a);actor_selection=int(draft.actors.size()-1);changed();}}}
            for(unsigned i=0;i<draft.actors.size();i++){ImGui::PushID(int(i));if(ImGui::Selectable(draft.actors[i].name.c_str(),actor_selection==int(i)))actor_selection=int(i);ImGui::PopID();}
            if(actor_selection>=0&&unsigned(actor_selection)<draft.actors.size()){
                ImGui::BeginDisabled(!world_entity::editable(draft.entities,{world_entity::Kind::Actor,unsigned(actor_selection)}));
                auto &a=draft.actors[unsigned(actor_selection)];char name[65];std::snprintf(name,sizeof name,"%s",a.name.c_str());bool edit=false;
                if(ImGui::InputText("Resident name",name,sizeof name)){a.name=name;edit=true;}
                edit|=ImGui::Checkbox("Resident enabled",&a.enabled);int appearance=int(a.appearance);if(ImGui::BeginCombo("Appearance",world_village_pose_archetypes[a.appearance].name)){for(unsigned i=0;i<std::size(world_village_pose_archetypes);i++)if(ImGui::Selectable(world_village_pose_archetypes[i].name,appearance==int(i))){a.appearance=i;edit=true;}ImGui::EndCombo();}
                edit|=ImGui::SliderFloat("Walking speed (m/s)",&a.speed,.3,2.5);edit|=ImGui::SliderFloat("Waypoint pause (s)",&a.dwell,.4,15);edit|=ImGui::SliderFloat("Stationary facing",&a.yaw,-180,180);edit|=ImGui::Checkbox("Closed patrol (off = return same path)",&a.loop);
                const char *area=a.navigation<0?"Direct capsule checks":draft.navigation[unsigned(a.navigation)].name.c_str();if(ImGui::BeginCombo("Navigation area",area)){if(ImGui::Selectable("Direct capsule checks",a.navigation<0)){a.navigation=-1;edit=true;}for(unsigned i=0;i<draft.navigation.size();i++)if(ImGui::Selectable(draft.navigation[i].name.c_str(),a.navigation==int(i))){a.navigation=int(i);edit=true;}ImGui::EndCombo();}
                if(edit){runtime_edit_changed();}
                ImGui::Text("%zu authored points | source %d",a.points.size(),a.source);
                if(ImGui::Button("Append target waypoint")&&target&&a.points.size()<32){checkpoint();a.points.push_back({x,z});changed();}
                ImGui::SameLine();if(ImGui::Button("Remove last waypoint")&&a.points.size()>1){checkpoint();a.points.pop_back();changed();}
                for(unsigned i=0;i<a.points.size();i++){ImGui::PushID(int(i));bool e=ImGui::InputDouble("East",&a.points[i][0],.5,5,"%.2f");ImGui::SameLine();e|=ImGui::InputDouble("South",&a.points[i][1],.5,5,"%.2f");if(e)runtime_edit_changed();ImGui::PopID();}
                if(ImGui::Button("Remove resident edit")){if(production_remove({world_entity::Kind::Actor,unsigned(actor_selection)}))actor_selection=-1;}
                ImGui::EndDisabled();
            }
            ImGui::Separator();ImGui::TextUnformatted("Local navigation bake");
            if(ImGui::Button("New area at target")&&target&&draft.navigation.size()<4){checkpoint();world_studio::NavigationArea a;a.x=x;a.z=z;a.name="Area "+std::to_string(draft.navigation.size()+1);draft.navigation.push_back(a);area_selection=int(draft.navigation.size()-1);changed();}
            for(unsigned i=0;i<draft.navigation.size();i++)if(ImGui::Selectable(draft.navigation[i].name.c_str(),area_selection==int(i)))area_selection=int(i);
            if(area_selection>=0&&unsigned(area_selection)<draft.navigation.size()){ImGui::BeginDisabled(!world_entity::editable(draft.entities,{world_entity::Kind::Navigation,unsigned(area_selection)}));auto &a=draft.navigation[unsigned(area_selection)];bool e=ImGui::InputDouble("Area east",&a.x,1,10,"%.1f");e|=ImGui::InputDouble("Area south",&a.z,1,10,"%.1f");e|=ImGui::SliderFloat("Area span (m)",&a.span,8,192);e|=ImGui::SliderFloat("Cell (m)",&a.cell,1,4);e|=ImGui::SliderFloat("Agent radius (m)",&a.radius,.2,.8);e|=ImGui::SliderFloat("Maximum slope",&a.slope,.1,1);if(e)runtime_edit_changed();
                if(ImGui::Button("Bake / preview committed ground")){if(world_navigation::bake(a,nav_query(scene_ptr),navigation_preview,message))message="Navigation preview ready. Apply rebakes the draft terrain and routes.";}
                ImGui::Text("Preview: %u traversable / %zu cells",navigation_preview.open,navigation_preview.cells.size());
                if(ImGui::Button("Delete navigation area")){if(production_remove({world_entity::Kind::Navigation,unsigned(area_selection)})){area_selection=-1;navigation_preview={};}}
                ImGui::EndDisabled();
            }
            ImGui::TextWrapped("Capsule/slope/water checks + local A*. Paths bake on Apply; blocked edits retain the previous world. Idle-only appearances cannot patrol. Original140 residents remain unless explicitly overridden.");ImGui::EndTabItem();
        }
        if(tab==11&&ImGui::BeginTabItem("VFX",nullptr,requested_tab==11?ImGuiTabItemFlags_SetSelected:0)){tab=11;vfx_panel(cam);ImGui::EndTabItem();}
        if(tab==12&&ImGui::BeginTabItem("Water",nullptr,requested_tab==12?ImGuiTabItemFlags_SetSelected:0)){tab=12;
            for(unsigned kind=0;kind<3;kind++){if(kind)ImGui::SameLine();if(ImGui::Button(kind==0?"Ocean":kind==1?"Forest pool":"Castle moat")){auto at=std::find_if(draft.waters.begin(),draft.waters.end(),[&](const auto &w){return w.source==kind;});if(at==draft.waters.end()&&draft.waters.size()<8){checkpoint();world_studio::Water w;w.source=kind;draft.waters.push_back(w);water_selection=int(draft.waters.size()-1);changed(true);}else if(at!=draft.waters.end())water_selection=int(at-draft.waters.begin());}}
            if(ImGui::Button("Carve 6m pond at target")&&target&&draft.waters.size()<8){float previous_radius=radius;radius=10;bool safe=safe_brush();radius=previous_radius;if(!safe)message="Choose open dry terrain with 10m clearance outside foundations and fixed routes.";else{auto next=draft;if(world_studio::carve_pond(next,x,z,world_atlas_base_height,message)){checkpoint();draft=std::move(next);water_selection=int(draft.waters.size()-1);changed(true);}}}
            if(ImGui::Button("Add pond to existing basin")&&target&&draft.waters.size()<8){checkpoint();world_studio::Water w;w.source=3;w.x=x;w.z=z;w.level=y+.5;draft.waters.push_back(w);water_selection=int(draft.waters.size()-1);changed(true);}
            for(unsigned i=0;i<draft.waters.size();i++)if(ImGui::Selectable(("Water "+std::to_string(i+1)+(draft.waters[i].source==3?" pond":" fixed shore")).c_str(),water_selection==int(i)))water_selection=int(i);
            if(water_selection>=0&&unsigned(water_selection)<draft.waters.size()){ImGui::BeginDisabled(!world_entity::editable(draft.entities,{world_entity::Kind::Water,unsigned(water_selection)}));auto &w=draft.waters[unsigned(water_selection)];bool e=ImGui::Checkbox("Water visible",&w.enabled);if(w.source==3){e|=ImGui::InputDouble("Pond east",&w.x,.5,5);e|=ImGui::InputDouble("Pond south",&w.z,.5,5);e|=ImGui::InputDouble("Water plane",&w.level,.05,.5);e|=ImGui::SliderFloat("East-west radius",&w.radius_x,1,32);e|=ImGui::SliderFloat("North-south radius",&w.radius_z,1,32);}else ImGui::TextWrapped("Existing shoreline geometry and level stay fixed.");e|=ImGui::ColorEdit3("Water transmission tint",w.tint.data(),ImGuiColorEditFlags_Float);e|=ImGui::SliderFloat3("Absorption RGB",w.absorption.data(),.25,3);e|=ImGui::SliderFloat("Surface roughness",&w.roughness,.5,2);e|=ImGui::SliderFloat("Wave normal amplitude",&w.waves,0,2);e|=ImGui::SliderFloat("Flow speed",&w.flow,0,2);e|=ImGui::SliderFloat("Contact foam",&w.foam,0,2);if(e)runtime_edit_changed(true);if(ImGui::Button("Remove water edit")){if(production_remove({world_entity::Kind::Water,unsigned(water_selection)},true))water_selection=-1;}ImGui::EndDisabled();}
            ImGui::TextWrapped("Custom ponds require a submerged center and dry rim. Apply updates cover, weather and navigation. Refraction samples the separate opaque snapshot. Additional ponds use environment reflections; the existing single planar reflection budget is retained. Removing water keeps separately sculpted terrain; Undo restores both.");ImGui::EndTabItem();
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
        if(!open||tab<10||tab>12)return;
        auto *draw=ImGui::GetBackgroundDrawList();const auto color=IM_COL32(96,221,184,230);
        auto line=[&](world_navigation::Point a,world_navigation::Point b){ImVec2 p,q;if(project_point(cam,{a.x,a.y,a.z},p)&&project_point(cam,{b.x,b.y,b.z},q))draw->AddLine(p,q,color,2);};
        if(tab==10){if(actor_selection>=0&&unsigned(actor_selection)<draft.actors.size()){const auto &a=draft.actors[unsigned(actor_selection)];world_navigation::Point previous;for(unsigned i=0;i<a.points.size();i++){auto p=a.points[i];world_navigation::Point v{p[0],world_atlas_walk_height(p[0],p[1])+.12,p[1]};ImVec2 at;if(project_point(cam,{v.x,v.y,v.z},at)){draw->AddCircleFilled(at,5,color);draw->AddText({at.x+6,at.y},color,std::to_string(i+1).c_str());}if(i)line(previous,v);previous=v;}}
            auto &g=navigation_preview;unsigned stride=std::max(1u,unsigned(g.cells.size()/900));for(unsigned i=0;i<g.cells.size();i+=stride){const auto &c=g.cells[i];if(!c.open)continue;ImVec2 p;if(project_point(cam,{g.area.x-g.side*g.area.cell*.5+(i%g.side+.5)*g.area.cell,c.y+.08,g.area.z-g.side*g.area.cell*.5+(i/g.side+.5)*g.area.cell},p))draw->AddCircleFilled(p,1.5,IM_COL32(62,211,126,130));}}
        if(tab==11&&particle_selection>=0&&unsigned(particle_selection)<draft.particles.size()){auto &p=draft.particles[unsigned(particle_selection)];for(unsigned i=0;i<32;i++){double a=i*6.283185307/32,b=(i+1)*6.283185307/32;line({p.x+std::cos(a)*p.radius,p.y,p.z+std::sin(a)*p.radius},{p.x+std::cos(b)*p.radius,p.y,p.z+std::sin(b)*p.radius});}line({p.x,p.y,p.z},{p.x,p.kind==2?p.y+p.height*3.2:std::min(p.ceiling,p.y+p.height),p.z});}
        if(tab==12&&water_selection>=0&&unsigned(water_selection)<draft.waters.size()){const auto &w=draft.waters[unsigned(water_selection)];if(w.source==3)for(unsigned i=0;i<48;i++){double a=i*6.283185307/48,b=(i+1)*6.283185307/48;line({w.x+std::cos(a)*w.radius_x,w.level,w.z+std::sin(a)*w.radius_z},{w.x+std::cos(b)*w.radius_x,w.level,w.z+std::sin(b)*w.radius_z});}}
    }

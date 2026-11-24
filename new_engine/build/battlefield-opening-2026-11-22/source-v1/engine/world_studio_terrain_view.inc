// Include inside Studio after tools_view. The viewer includes
// world_terrain_authoring.h first. Call production_terrain_panel() inside its
// Advanced terrain tab; call production_terrain_preview(cam) only for that tab.
    world_terrain_authoring::Region advanced_region;
    world_terrain_authoring::Heightmap advanced_heightmap;
    world_terrain_authoring::Erosion advanced_erosion;
    world_terrain_authoring::Biome advanced_biome;
    world_terrain_authoring::Corridor advanced_corridor;
    world_terrain_authoring::Report advanced_report;
    bool advanced_region_ready=false,advanced_map_ready=false,advanced_relocate=false,advanced_has_report=false;
    int advanced_extent=0,advanced_seed=1;
    float advanced_export_min=0,advanced_export_max=256,advanced_import_strength=1,advanced_import_border=8;
    float advanced_grade_offset=0;
    char advanced_heightmap_path[512]="terrain_heightmap.pgm";
    struct AdvancedFoundation {double x,z,radius;};
    struct AdvancedProtectedPath {double x,z,dx,dz,length2,minx,maxx,minz,maxz;};
    std::vector<AdvancedFoundation> advanced_foundations;
    std::vector<AdvancedProtectedPath> advanced_paths;
    void advanced_add_protected_segment(double ax,double az,double bx,double bz){
        const double dx=bx-ax,dz=bz-az;
        advanced_paths.push_back({ax,az,dx,dz,dx*dx+dz*dz,std::min(ax,bx)-4,std::max(ax,bx)+4,std::min(az,bz)-4,std::max(az,bz)+4});
    }
    void advanced_prepare_protection(){
        advanced_foundations.clear();advanced_paths.clear();
        landscape_collect_foundations(draft,advanced_foundations);
        landscape_draft_roads_ok=landscape_draft_roads.prepare(draft.surfaces,message);
        auto append=[&](const WorldAtlasTrailPoint *points,size_t count){for(size_t i=1;i<count;i++)advanced_add_protected_segment(points[i-1].x,points[i-1].z,points[i].x,points[i].z);};
        size_t count=0;const auto *points=world_atlas_rainforest_trail_points(&count);append(points,count);points=world_atlas_trail(&count);append(points,count);
        for(const auto &actor:draft.actors){for(size_t i=1;i<actor.points.size();i++)advanced_add_protected_segment(actor.points[i-1][0],actor.points[i-1][1],actor.points[i][0],actor.points[i][1]);if(actor.points.size()==1)advanced_foundations.push_back({actor.points[0][0],actor.points[0][1],2});if(actor.loop&&actor.points.size()>2)advanced_add_protected_segment(actor.points.back()[0],actor.points.back()[1],actor.points.front()[0],actor.points.front()[1]);}
    }
    static bool advanced_protected(double px,double pz,const void *data){
        const auto *self=static_cast<const Studio *>(data);
        if(!self->landscape_draft_roads_ok||self->landscape_draft_roads.blocked(px,pz))return true;
        if(world_atlas_village_weight(px,pz)>.01f||world_atlas_bandit_sample(px,pz).clearing>.01f||std::hypot(px-WORLD_TRAIL_CAMP_X,pz-WORLD_TRAIL_CAMP_Z)<38)return true;
        if(world_atlas_local_water(px,pz,nullptr,nullptr)||world_atlas_base_height(px,pz)+world_studio::height_delta(self->draft,px,pz)<1)return true;
        for(const auto &f:self->advanced_foundations){const double dx=px-f.x,dz=pz-f.z;if(std::abs(dx)<=f.radius&&std::abs(dz)<=f.radius&&dx*dx+dz*dz<f.radius*f.radius)return true;}
        for(const auto &p:self->advanced_paths){if(px<p.minx||px>p.maxx||pz<p.minz||pz>p.maxz)continue;const double t=p.length2>1e-8?std::clamp(((px-p.x)*p.dx+(pz-p.z)*p.dz)/p.length2,0.,1.):0;if(std::hypot(px-p.x-p.dx*t,pz-p.z-p.dz*t)<4)return true;}
        for(const auto &w:self->draft.waters)if(w.source==3&&w.enabled){const double dx=(px-w.x)/(w.radius_x+2),dz=(pz-w.z)/(w.radius_z+2);if(dx*dx+dz*dz<1)return true;}
        return false;
    }
    world_terrain_authoring::Source advanced_source(){advanced_prepare_protection();return {world_atlas_base_height,advanced_protected,this};}
    void advanced_finish(world_studio::Document &&candidate,const world_terrain_authoring::Report &report,const char *operation){
        advanced_report=report;advanced_has_report=true;
        if(!report.changed_nodes&&!report.added_masks&&!report.added_tiles){message=std::string(operation)+": no terrain or mask change.";return;}
        checkpoint();draft=std::move(candidate);changed(true);message=std::string(operation)+" staged. Apply rebuilds grounding, terrain and ecology; Save persists the applied result.";
    }
    void production_terrain_panel(){
        namespace terrain=world_terrain_authoring;
        ImGui::TextWrapped("Bounded terrain production tools. Operations edit a draft; Undo restores the whole operation. Apply rebuilds the world. Fixed demo floors, routes and water are protected.");
        ImGui::Combo("Region extent",&advanced_extent,"128 x128m\0 256 x256m\0 512 x512m\0");
        if(ImGui::Button("Choose region from picked ground")){
            if(!target)message="Pick open ground first.";
            else{const unsigned side=64u*(1u<<unsigned(std::clamp(advanced_extent,0,2)))+1;advanced_region={std::floor(x/128)*128,std::floor(z/128)*128,side,side};advanced_region_ready=true;message="Advanced terrain region selected. Yellow outline previews its boundary.";}
        }
        if(advanced_region_ready)ImGui::Text("Region %.0f x%.0fm | E%.1f N%.1f | %zu /64 tiles",(advanced_region.columns-1)*2.,(advanced_region.rows-1)*2.,advanced_region.x,-advanced_region.z,draft.terrain.size());
        ImGui::Separator();
        if(ImGui::CollapsingHeader("16-bit heightmap import / export",ImGuiTreeNodeFlags_DefaultOpen)){
            if(ImGui::InputText("Heightmap PGM path",advanced_heightmap_path,sizeof advanced_heightmap_path))advanced_map_ready=false;
            ImGui::InputFloat("Export minimum metres",&advanced_export_min,1,10);ImGui::InputFloat("Export maximum metres",&advanced_export_max,1,10);
            if(ImGui::Button("Export selected region")){
                if(!advanced_region_ready)message="Choose a region before export.";
                else{terrain::Heightmap map;if(terrain::sample_heightmap(draft,advanced_region,advanced_export_min,advanced_export_max,{world_atlas_base_height},map,message)&&terrain::save_heightmap(advanced_heightmap_path,map,message))message="16-bit PGM exported with2m spacing, world origin and metre range metadata.";}
            }
            ImGui::SameLine();if(ImGui::Button("Read heightmap")){advanced_map_ready=false;terrain::Heightmap map;if(terrain::load_heightmap(advanced_heightmap_path,map,message)){advanced_heightmap=std::move(map);advanced_map_ready=true;message="Heightmap read. Import explicitly to stage its terrain.";}}
            if(advanced_map_ready)ImGui::Text("Read %u x%u | E%.1f N%.1f | %.2f..%.2fm",advanced_heightmap.region.columns,advanced_heightmap.region.rows,advanced_heightmap.region.x,-advanced_heightmap.region.z,advanced_heightmap.minimum,advanced_heightmap.maximum);
            ImGui::Checkbox("Relocate import to selected region origin",&advanced_relocate);
            ImGui::SliderFloat("Height import strength",&advanced_import_strength,0,1);ImGui::SliderFloat("Height edge blend (m)",&advanced_import_border,2,64);
            if(ImGui::Button("Import heightmap into draft")){
                if(!advanced_map_ready)message="Read a Valwood16-bit PGM first.";
                else if(advanced_relocate&&!advanced_region_ready)message="Choose a destination region before relocating.";
                else{auto map=advanced_heightmap;if(advanced_relocate){map.region.x=advanced_region.x;map.region.z=advanced_region.z;}auto next=draft;terrain::Report report;if(terrain::import_heightmap(next,map,{advanced_import_strength,advanced_import_border},advanced_source(),report,message))advanced_finish(std::move(next),report,"Heightmap import");}
            }
            ImGui::TextWrapped("PGM P5 uses65535 big-endian samples and a Valwood origin/metre-range comment. Import preserves edge continuity and clamps local displacement to +/-32m. Existing source files stay intact.");
        }
        if(ImGui::CollapsingHeader("Hydraulic / thermal erosion")){
            int iterations=int(advanced_erosion.iterations);if(ImGui::SliderInt("Erosion iterations",&iterations,1,128))advanced_erosion.iterations=unsigned(iterations);ImGui::InputInt("Rain seed",&advanced_seed);advanced_seed=std::max(0,advanced_seed);
            ImGui::SliderFloat("Rain per step",&advanced_erosion.rain,0,.25f);ImGui::SliderFloat("Evaporation",&advanced_erosion.evaporation,0,1);ImGui::SliderFloat("Sediment capacity",&advanced_erosion.capacity,0,12);ImGui::SliderFloat("Hydraulic removal",&advanced_erosion.erosion,0,1);ImGui::SliderFloat("Sediment deposition",&advanced_erosion.deposition,0,1);ImGui::SliderFloat("Thermal relaxation",&advanced_erosion.thermal,0,1);ImGui::SliderFloat("Talus slope",&advanced_erosion.talus,0,2);ImGui::SliderFloat("Erosion edge blend (m)",&advanced_erosion.border,2,64);
            if(ImGui::Button("Erode selected region into draft")){
                if(!advanced_region_ready)message="Choose a terrain region first.";
                else{advanced_erosion.region=advanced_region;advanced_erosion.seed=unsigned(advanced_seed);auto next=draft;terrain::Report report;if(terrain::erode(next,advanced_erosion,advanced_source(),report,message))advanced_finish(std::move(next),report,"Erosion");}
            }
            ImGui::TextWrapped("Rain transports sediment downhill; thermal relaxation moves slopes toward the talus limit. Large regions / many iterations can pause this explicit editing operation.");
        }
        if(ImGui::CollapsingHeader("Constrained biome paint")){
            ImGui::Combo("Biome foliage layer",&advanced_biome.layer,"Grass\0Trees\0Rocks\0Ferns\0");ImGui::SliderFloat("Biome brush radius",&advanced_biome.radius,2,64);ImGui::SliderFloat("Remaining biome density",&advanced_biome.density,0,1);
            ImGui::InputFloat("Minimum terrain height",&advanced_biome.minimum_height,1,10);ImGui::InputFloat("Maximum terrain height",&advanced_biome.maximum_height,1,10);ImGui::SliderFloat("Minimum slope degrees",&advanced_biome.minimum_slope,0,90);ImGui::SliderFloat("Maximum slope degrees",&advanced_biome.maximum_slope,0,90);int divisions=int(advanced_biome.divisions);if(ImGui::SliderInt("Biome subdivisions",&divisions,1,5))advanced_biome.divisions=unsigned(divisions);
            if(ImGui::Button("Paint constrained density into draft")){
                if(!target)message="Pick the biome brush center first.";
                else{advanced_biome.x=x;advanced_biome.z=z;auto next=draft;terrain::Report report;if(terrain::paint_biome(next,advanced_biome,advanced_source(),report,message))advanced_finish(std::move(next),report,"Biome mask paint");}
            }
            ImGui::TextWrapped("Accepted circular footprints satisfy sampled height, slope and protection rules. These use the existing32-mask budget; this does not introduce a separate biome raster.");
        }
        if(ImGui::CollapsingHeader("Graded road / river corridor")){
            int kind=int(advanced_corridor.kind);if(ImGui::Combo("Corridor type",&kind,"Road grade\0River channel\0"))advanced_corridor.kind=terrain::CorridorKind(kind);
            ImGui::InputFloat("Picked height offset (m)",&advanced_grade_offset,.1f,1);if(ImGui::Button("Append picked control point")){
                if(!target)message="Pick corridor ground first.";
                else if(advanced_corridor.points.size()>=32)message="32 corridor control points reached.";
                else advanced_corridor.points.push_back({x,z,world_atlas_base_height(x,z)+world_studio::height_delta(draft,x,z)+advanced_grade_offset});
            }
            ImGui::SameLine();if(ImGui::Button("Clear corridor points"))advanced_corridor.points.clear();
            for(size_t i=0;i<advanced_corridor.points.size();i++){ImGui::PushID(int(i));auto &p=advanced_corridor.points[i];ImGui::Text("Point %u",unsigned(i+1));ImGui::InputDouble("Point east",&p.x,1,10);ImGui::InputDouble("Point south",&p.z,1,10);ImGui::InputDouble("Center grade height",&p.height,.1,1);ImGui::PopID();}
            ImGui::SliderFloat("Corridor half width",&advanced_corridor.half_width,.5f,16);ImGui::SliderFloat("Bank / shoulder width",&advanced_corridor.bank_width,2,32);if(advanced_corridor.kind==terrain::CorridorKind::River)ImGui::SliderFloat("Channel depth",&advanced_corridor.depth,0,16);ImGui::SliderFloat("Maximum rise / run",&advanced_corridor.maximum_grade,0,1);
            const char *layers[]={"Clear grass","Clear trees","Clear rocks","Clear ferns"};for(unsigned i=0;i<4;i++){bool enabled=(advanced_corridor.vegetation_layers&(1u<<i))!=0;if(ImGui::Checkbox(layers[i],&enabled)){if(enabled)advanced_corridor.vegetation_layers|=1u<<i;else advanced_corridor.vegetation_layers&=~(1u<<i);}}
            ImGui::SliderFloat("Corridor remaining vegetation",&advanced_corridor.vegetation_density,0,1);
            if(ImGui::Button("Sculpt corridor into draft")){auto next=draft;terrain::Report report;if(terrain::sculpt_corridor(next,advanced_corridor,advanced_source(),report,message))advanced_finish(std::move(next),report,"Road / river sculpt");}
            ImGui::TextWrapped("Roads grade terrain; rivers carve a parabolic bed with blended banks. River control heights must descend. Water surfaces and road material meshes are authored separately. The fixed demo routes stay protected.");
        }
        if(advanced_has_report){ImGui::Separator();ImGui::Text("Last operation: %u nodes | %u new tiles | %u masks",advanced_report.changed_nodes,advanced_report.added_tiles,advanced_report.added_masks);ImGui::Text("Largest change %.3fm | %u clamped samples",advanced_report.largest_change,advanced_report.clamped_nodes);}
    }
    void production_terrain_preview(const Camera &cam){
        auto *draw=ImGui::GetBackgroundDrawList();
        if(advanced_region_ready){const double width=(advanced_region.columns-1)*2.,depth=(advanced_region.rows-1)*2.;const std::array<std::array<double,2>,5> corners={{{advanced_region.x,advanced_region.z},{advanced_region.x+width,advanced_region.z},{advanced_region.x+width,advanced_region.z+depth},{advanced_region.x,advanced_region.z+depth},{advanced_region.x,advanced_region.z}}};
            for(unsigned side=1;side<corners.size();side++)for(unsigned step=0;step<16;step++){ImVec2 a,b;const auto &p=corners[side-1],&q=corners[side];auto point=[&](double t){const double px=p[0]+(q[0]-p[0])*t,pz=p[1]+(q[1]-p[1])*t;return world_studio::Vec{px,world_atlas_base_height(px,pz)+world_studio::height_delta(draft,px,pz)+.15,pz};};if(project_point(cam,point(step/16.),a)&&project_point(cam,point((step+1)/16.),b))draw->AddLine(a,b,IM_COL32(250,205,75,220),2);}
        }
        ImVec2 previous;bool have_previous=false;for(const auto &p:advanced_corridor.points){ImVec2 current;const bool visible=project_point(cam,{p.x,p.height+.2,p.z},current);if(visible){draw->AddCircleFilled(current,4,IM_COL32(100,218,255,240));if(have_previous)draw->AddLine(previous,current,IM_COL32(100,218,255,220),2);}previous=current;have_previous=visible;}
    }

    static float surface_ground(double px,double pz,void *data){return world_atlas_base_height(px,pz)+world_studio::height_delta(*static_cast<world_studio::Document *>(data),px,pz);}
    bool prepare_surface_field(world_studio::Document &doc,std::string &error,const world_studio::Document *previous=nullptr){
        bool incremental=false;std::vector<std::pair<int,int>> changed_tiles;
        if(previous&&world_surface::same_paths(previous->surfaces,doc.surfaces)&&previous->waters.size()==doc.waters.size()&&
           std::none_of(doc.waters.begin(),doc.waters.end(),[](const auto &w){return w.enabled&&w.basin_depth>0;})){
            bool same_water=true;
            for(size_t i=0;i<doc.waters.size();++i){const auto &a=doc.waters[i],&b=previous->waters[i];
                if(a.source!=b.source||a.enabled!=b.enabled||a.x!=b.x||a.z!=b.z||a.level!=b.level||
                   a.radius_x!=b.radius_x||a.radius_z!=b.radius_z||a.basin_depth!=b.basin_depth){same_water=false;break;}
            }
            if(same_water){
                auto find_tile=[](const auto &tiles,int x,int z)->const world_studio::TerrainTile*{
                    for(const auto &tile:tiles)if(tile.x==x&&tile.z==z)return &tile;return nullptr;
                };
                for(const auto &tile:doc.terrain){const auto *old=find_tile(previous->terrain,tile.x,tile.z);
                    if(!old||old->delta!=tile.delta)changed_tiles.emplace_back(tile.x,tile.z);
                }
                for(const auto &tile:previous->terrain)if(!find_tile(doc.terrain,tile.x,tile.z))changed_tiles.emplace_back(tile.x,tile.z);
                if(world_studio::valid(doc,error)){
                    if(previous->surface_field){
                        auto next=std::make_shared<world_surface::TerrainField>();
                        if(world_surface::rebake_sculpt(*previous->surface_field,surface_ground,&doc,changed_tiles,*next,error)){
                            doc.surface_field=std::move(next);incremental=true;
                        }
                    }else if(doc.surfaces.ribbons.empty()){
                        doc.surface_field.reset();incremental=true;
                    }
                }else return false;
            }
        }
        if(!incremental&&!world_pond_field::prepare(doc,{world_atlas_base_height},error))return false;
        if(!doc.surface_field)return true;
        const auto &field=doc.surface_field;
        for(const auto &tile:field->tiles){if(incremental&&!std::any_of(changed_tiles.begin(),changed_tiles.end(),[&](const auto &changed){return std::abs(changed.first-tile.x)<=1&&std::abs(changed.second-tile.z)<=1;}))continue;for(unsigned z=0;z<65;z++)for(unsigned x=0;x<65;x++){
            if(std::abs(tile.delta[z*65+x])<.15f)continue;
            const double wx=tile.x*128.+x*2,wz=tile.z*128.+z*2;
            if(world_atlas_village_weight(wx,wz)>.01f||world_atlas_bandit_sample(wx,wz).clearing>.01){error="Road/river grade overlaps protected settlement terrain; keep existing street height or move outside its foundations";return false;}
            for(const auto &p:source)if(p.foundation){double dx=wx-p.x,dz=wz-p.z,c=std::cos(p.yaw),s=std::sin(p.yaw),lx=(c*dx-s*dz)/p.scale,lz=(s*dx+c*dz)/p.scale;if(lx>p.minimum[0]-2&&lx<p.maximum[0]+2&&lz>p.minimum[2]-2&&lz<p.maximum[2]+2){error="Road/river cut overlaps a source foundation";return false;}}
        }}
        if(!world_terrain_authoring::combined_height_valid(doc,*field,error,incremental?&changed_tiles:nullptr))return false;
        return true;
    }
    void surface_terrain_binding(Program &p)const{
        int binding=-1;for(unsigned i=0;i<live.surfaces.bindings.size();i++)if(live.surfaces.bindings[i].alias=="__terrain__")binding=int(i);
        p.i("uSurfaceBinding",binding);surface_graph(p,"__terrain__");
    }
    void surface_graph(Program &p,const char *alias)const{
        int graph=-1;for(unsigned i=0;i<live.graphs.size();i++)if(live.graphs[i].alias==alias)graph=int(i);p.i("uStudioGraphIndex",graph);
    }

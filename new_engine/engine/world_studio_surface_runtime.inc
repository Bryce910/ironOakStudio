    static float surface_ground(double px,double pz,void *data){return world_atlas_base_height(px,pz)+world_studio::height_delta(*static_cast<world_studio::Document *>(data),px,pz);}
    bool prepare_surface_field(world_studio::Document &doc,std::string &error){
        if(doc.surfaces.ribbons.empty()){doc.surface_field.reset();return true;}
        auto field=std::make_shared<world_surface::TerrainField>();
        if(!world_surface::bake_terrain(doc.surfaces,surface_ground,&doc,*field,error))return false;
        for(const auto &tile:field->tiles)for(unsigned z=0;z<65;z++)for(unsigned x=0;x<65;x++){
            if(std::abs(tile.delta[z*65+x])<.15f)continue;
            const double wx=tile.x*128.+x*2,wz=tile.z*128.+z*2;
            if(world_atlas_village_weight(wx,wz)>.01f||world_atlas_bandit_sample(wx,wz).clearing>.01){error="Road/river grade overlaps protected settlement terrain; keep existing street height or move outside its foundations";return false;}
            for(const auto &p:source)if(p.foundation){double dx=wx-p.x,dz=wz-p.z,c=std::cos(p.yaw),s=std::sin(p.yaw),lx=(c*dx-s*dz)/p.scale,lz=(s*dx+c*dz)/p.scale;if(lx>p.minimum[0]-2&&lx<p.maximum[0]+2&&lz>p.minimum[2]-2&&lz<p.maximum[2]+2){error="Road/river cut overlaps a source foundation";return false;}}
        }
        doc.surface_field=std::move(field);return true;
    }
    void surface_terrain_binding(Program &p)const{
        int binding=-1;for(unsigned i=0;i<live.surfaces.bindings.size();i++)if(live.surfaces.bindings[i].alias=="__terrain__")binding=int(i);
        p.i("uSurfaceBinding",binding);surface_graph(p,"__terrain__");
    }
    void surface_graph(Program &p,const char *alias)const{
        int graph=-1;for(unsigned i=0;i<live.graphs.size();i++)if(live.graphs[i].alias==alias)graph=int(i);p.i("uStudioGraphIndex",graph);
    }

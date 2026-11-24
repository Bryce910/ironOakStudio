// Bounded sparse source data. Enlarging a head changes global draw offsets,
// never leg correspondence: the payload addresses primitive/local vertices.
bool world_combat_motion_load_locomotion(WorldCombatMotion *p,const char *directory,char *error,std::size_t capacity){
    WorldModelMaterialBatch material_batch_scope;
    if(error&&capacity)error[0]=0;
    if(!p||!directory||!*directory||p->locomotion||!p->loadouts||!p->unarmed)return false;
    char path[1400];const int length=std::snprintf(path,sizeof path,"%s/directional_locomotion.bin",directory);
    if(length<0||std::size_t(length)>=sizeof path)return false;
    auto *file=std::fopen(path,"rb");if(!file)return false;
    auto *out=new(std::nothrow)Locomotion;if(!out){std::fclose(file);return false;}
    bool okay=true;
    auto word=[&](){unsigned char b[4]={};if(std::fread(b,1,4,file)!=4)okay=false;return unsigned(b[0])|(unsigned(b[1])<<8)|(unsigned(b[2])<<16)|(unsigned(b[3])<<24);};
    auto scalar=[&](){unsigned bits=word();float f=0;std::memcpy(&f,&bits,4);if(!std::isfinite(f))okay=false;return f;};
    try{
        char magic[8];if(std::fread(magic,1,8,file)!=8)throw "Invalid locomotion header";
        const unsigned version=word();
        const bool npc_union=version==2&&!std::memcmp(magic,"VWLOCO02",8);
        if((!npc_union&&(version!=1||std::memcmp(magic,"VWLOCO01",8)))||word()!=5||word()!=16||word()!=4||!okay)throw "Unsupported locomotion format";
        for(float &f:out->strides){f=scalar();if(f<.5f||f>3)throw "Invalid locomotion stride";}
        for(unsigned bank=0;bank<5;++bank){auto &b=out->banks[bank];
            auto *asset=bank==0?p->body.assets[0]:bank==4?p->unarmed->assets[0]:p->loadouts->entries[bank-1].body.assets[0];
            const unsigned meshes=word(),original_total=word(),sparse=word();
            // V2 permits an empty override only for the authored four-outfit
            // NPC union. Its baked legs stay intact; the separate FPS stream
            // and the other four player loadouts retain their original data.
            const bool baked_npc_legs=npc_union&&bank==0&&sparse==0&&world_model_pose_variant_mask(asset)==15;
            if(!okay||!meshes||meshes>128||meshes!=world_model_pose_mesh_count(asset)||(!sparse&&!baked_npc_legs)||sparse>10000||original_total>100000)throw "Locomotion dimensions rejected";
            std::vector<unsigned> expected(meshes),offset(meshes),actual(meshes),mesh(sparse),local(sparse);unsigned total=0,original_sum=0;
            for(unsigned i=0;i<meshes;++i){expected[i]=word();original_sum+=expected[i];actual[i]=unsigned(world_model_pose_mesh_vertices(asset,i));offset[i]=total;total+=actual[i];}
            if(baked_npc_legs&&expected!=actual)throw "NPC union locomotion layout rejected";
            if(original_sum!=original_total||total>100000)throw "Locomotion draw size rejected";
            for(unsigned &v:mesh)v=word();for(unsigned &v:local)v=word();
            b.masks.resize(sparse);b.indices.resize(sparse);
            for(unsigned i=0;i<sparse;++i){b.masks[i]=scalar();
                if(mesh[i]>=meshes||local[i]>=expected[mesh[i]]||expected[mesh[i]]!=actual[mesh[i]]||b.masks[i]<=0||b.masks[i]>1)throw "Locomotion vertex correspondence rejected";
                b.indices[i]=offset[mesh[i]]+local[i];
            }
            {auto sorted=b.indices;std::sort(sorted.begin(),sorted.end());if(std::adjacent_find(sorted.begin(),sorted.end())!=sorted.end())throw "Repeated locomotion vertex";}
            b.templates.resize(std::size_t(sparse)*64);
            // Supported native and Windows targets are little-endian IEEE754.
            static_assert(sizeof(std::array<float,9>)==36,"Packed locomotion samples");
            if(std::fread(b.templates.data(),sizeof(b.templates[0]),b.templates.size(),file)!=b.templates.size())throw "Truncated locomotion samples";
            for(const auto &v:b.templates)for(float f:v)if(!std::isfinite(f)||std::abs(f)>8)throw "Unbounded locomotion sample";
            if(!okay)throw "Truncated locomotion samples";
            for(const auto &v:b.templates){const float n=v[3]*v[3]+v[4]*v[4]+v[5]*v[5],t=v[6]*v[6]+v[7]*v[7]+v[8]*v[8];if(n<.8f||n>1.2f||t<.8f||t>1.2f||v[1]<-.0001f)throw "Invalid locomotion surface basis";}
            b.staging.resize(total);b.stream=world_model_vertex_stream_create(asset);if(!b.stream)throw "Locomotion GPU allocation failed";
        }
        if(std::fgetc(file)!=EOF||!okay)throw "Unexpected locomotion tail";
        // All CPU dimensions are accepted before touching existing pose VAOs.
        p->locomotion=out;
        for(unsigned bank=0;bank<5;++bank){auto &b=out->banks[bank];
            if(bank==4){for(auto *batch:p->unarmed->blends)if(!world_model_pose_blend_attach_vertex_stream(batch,b.stream))throw "Unarmed gait attachment failed";}
            else{auto &body=bank==0?p->body:p->loadouts->entries[bank-1].body;for(auto *batch:body.blend)if(batch&&!world_model_pose_blend_attach_vertex_stream(batch,b.stream))throw "Combat gait attachment failed";}
            p->stats.locomotion_gpu_bytes+=b.staging.size()*sizeof(WorldModelVertexOverride);
        }
        p->stats.locomotion_loaded=true;std::fclose(file);return true;
    }catch(const char *message){if(error&&capacity)std::snprintf(error,capacity,"%s",message);}
    catch(...){if(error&&capacity)std::snprintf(error,capacity,"Locomotion loading failed");}
    std::fclose(file);if(p->locomotion!=out)free_locomotion(out);return false;
}
namespace {
void commit_locomotion(WorldCombatMotion *p,unsigned bank,bool upload=true){
    if(!p||!p->locomotion||bank>=5)return;auto &g=*p->locomotion;
    if(g.state.weight<=0)return;auto &b=g.banks[bank];const unsigned count=unsigned(b.indices.size());
    if(!count)return;
    const float phase=g.state.phase*16;const unsigned lo=unsigned(phase)%16,hi=(lo+1)%16;const float w=phase-std::floor(phase);
    for(unsigned i=0;i<count;++i){auto &v=b.staging[b.indices[i]];float q[9]={};
        for(unsigned d=0;d<4;++d){const auto &a=b.templates[(d*16+lo)*count+i],&z=b.templates[(d*16+hi)*count+i];
            for(unsigned k=0;k<9;++k)q[k]+=(a[k]+(z[k]-a[k])*w)*g.state.direction[d];}
        std::copy_n(q,3,v.position);std::copy_n(q+3,3,v.normal);std::copy_n(q+6,3,v.tangent);v.weight=b.masks[i]*g.state.weight;
    }
    if(upload&&world_model_vertex_stream_upload(b.stream,b.staging.data(),b.staging.size()))++p->stats.locomotion_uploads;
}
}

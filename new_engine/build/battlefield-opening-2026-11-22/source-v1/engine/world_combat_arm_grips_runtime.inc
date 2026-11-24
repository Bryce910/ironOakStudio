// Player-only arm correction shares the existing body deformation buffer.
// All file parsing, topology checks and GPU allocation happen at loading.
namespace {
Bank &grip_bank(WorldCombatMotion *p,unsigned bank,bool arms){
    if(bank==0)return arms?p->arms:p->body;
    auto &entry=p->loadouts->entries[bank]; // metadata1=axe,2=staff
    return arms?entry.arms:entry.body;
}
unsigned grip_locomotion_bank(unsigned bank){return bank==0?0:bank+1;}
world_arm_grip_stream::NativePose grip_native_path(const WorldModelRigidWeaponPose &source,bool arms){
    world_arm_grip_stream::NativePose out;
    std::copy_n(source.rotation,4,out.rotation);std::copy_n(source.pivot_a,3,out.pivot_a);std::copy_n(source.pivot_b,3,out.pivot_b);
    if(arms){out.pivot_a[0]-=.25f;out.pivot_b[0]-=.25f;out.pivot_a[2]-=.65f;out.pivot_b[2]-=.65f;}
    return out;
}
void grip_failure(WorldCombatMotion *p,const char *message){
    ++p->grip_stats.failures;
    if(p->arm_grips){p->arm_grips->healthy=false;p->arm_grips->body_active=p->arm_grips->arms_active=false;}
    std::fprintf(stderr,"[Arm grips] Disabled after a validation failure: %s\n",message);
}
}
bool world_combat_motion_load_grips(WorldCombatMotion *p,const char *path,char *error,std::size_t capacity){
    if(error&&capacity)error[0]=0;
    if(!p||!path||!*path||p->arm_grips||!p->locomotion||!p->loadouts)return false;
    auto fail=[&](const char *message){if(error&&capacity)std::snprintf(error,capacity,"%s",message);return false;};
    auto *file=std::fopen(path,"rb");if(!file)return fail("Cannot open arm grip metadata");
    auto *out=new(std::nothrow)ArmGrips;
    if(!out){std::fclose(file);return fail("Arm grip allocation failed");}
    bool okay=false;
    try{
        if(std::fseek(file,0,SEEK_END)!=0)throw "Arm grip file size failed";
        const long bytes=std::ftell(file);
        if(bytes<16||bytes>256L*1024*1024||std::fseek(file,0,SEEK_SET)!=0)throw "Arm grip file byte budget rejected";
        std::vector<unsigned char> input(static_cast<std::size_t>(bytes));
        if(std::fread(input.data(),1,input.size(),file)!=input.size())throw "Truncated arm grip metadata";
        std::array<world_arm_grip_stream::Layout,3> layouts;
        for(unsigned bank=0;bank<3;++bank)for(unsigned view=0;view<2;++view){
            auto *asset=grip_bank(p,bank,view!=0).assets[0];auto &counts=layouts[bank].views[view];
            const auto meshes=world_model_pose_mesh_count(asset);if(!meshes||meshes>128)throw "Arm grip model layout rejected";
            for(std::size_t i=0;i<meshes;++i)counts.push_back(unsigned(world_model_pose_mesh_vertices(asset,i)));
        }
        std::string reason;
        if(!world_arm_grip_stream::load(input.data(),input.size(),layouts,out->database,reason)){
            if(error&&capacity)std::snprintf(error,capacity,"%s",reason.c_str());
            std::fclose(file);free_arm_grips(out);return false;
        }
        for(const auto &bank:out->database.banks){
            auto &body=p->locomotion->banks[grip_locomotion_bank(bank.id)];
            for(const auto &v:bank.views[0].vertices)if((v.flags&2)&&v.global>=body.staging.size())throw "Arm grip body stream correspondence rejected";
            // Arm outputs and the existing locomotion masks must be disjoint.
            for(const auto &v:bank.views[0].vertices)if(v.flags&2)
                if(std::find(body.indices.begin(),body.indices.end(),v.global)!=body.indices.end())throw "Arm grip and gait masks overlap";
            for(unsigned slot=0;slot<batches;++slot){
                const auto pair=pose_pair(slot,poses);const auto &native=grip_bank(p,bank.id,false).rigid[slot];
                if(bank.pairs[slot].accepted){
                    if(!native.enabled||pair[0]!=bank.pairs[slot].a||pair[1]!=bank.pairs[slot].b)throw "Arm grip batch does not have the certified rigid path";
                    const auto actual=grip_native_path(native,false);
                    if(!world_arm_grip_stream::detail::same_native(actual,bank.pairs[slot].native))throw "Arm grip native weapon path differs from metadata";
                }
                // Body and viewmodel may intentionally have different authored
                // holds. Validate each enabled view against its own real rigid
                // path; never certify one view using the other's weapon.
                if(bank.pairs[slot].fp_accepted){
                    const auto &source=grip_bank(p,bank.id,true).rigid[slot];
                    if(!source.enabled||pair[0]!=bank.pairs[slot].a||pair[1]!=bank.pairs[slot].b)throw "First-person grip has no certified rigid path";
                    const auto fp_native=grip_native_path(source,true);
                    if(!world_arm_grip_stream::detail::same_native(fp_native,bank.pairs[slot].native))throw "First-person grip native weapon path differs from metadata";
                }
            }
            auto &fp=out->arms[bank.id];
            if(!bank.views[1].vertices.empty()){
                std::size_t count=0;for(unsigned n:layouts[bank.id].views[1])count+=n;
                fp.staging.resize(count);fp.stream=world_model_vertex_stream_create(grip_bank(p,bank.id,true).assets[0]);
                if(!fp.stream)throw "Arm grip first-person stream allocation failed";
            }
        }
        // Keep ownership even if a later GL attachment fails, so no borrowed
        // VAO can outlive its buffer during ordinary error cleanup.
        p->arm_grips=out;
        for(const auto &bank:out->database.banks){
            auto &fp=out->arms[bank.id];if(fp.stream)
                for(auto *batch:grip_bank(p,bank.id,true).blend)if(batch&&!world_model_pose_blend_attach_vertex_stream(batch,fp.stream))throw "Arm grip first-person attachment failed";
            p->grip_stats.extra_gpu_bytes+=fp.staging.size()*sizeof(WorldModelVertexOverride);
            for(const auto &pair:bank.pairs){p->grip_stats.certified_pairs+=pair.accepted;p->grip_stats.fp_certified_pairs+=pair.fp_accepted;}
        }
        p->grip_stats.resident_cpu_bytes=out->database.resident_bytes+sizeof(ArmGrips)-sizeof(world_arm_grip_stream::Database);
        for(const auto &view:out->arms)p->grip_stats.resident_cpu_bytes+=view.staging.capacity()*sizeof(WorldModelVertexOverride);
        if(p->grip_stats.resident_cpu_bytes>512ull*1024*1024||p->grip_stats.extra_gpu_bytes>1024ull*1024)throw "Arm grip resident/GPU resource budget rejected";
        p->grip_stats.loaded=true;p->grip_stats.banks=unsigned(out->database.banks.size());p->grip_stats.metadata_bytes=out->database.input_bytes;
        std::printf("[Arm grips] Loaded %u banks, %u bounded pairs, %llu metadata bytes; body reuses gait buffer\n",p->grip_stats.banks,p->grip_stats.certified_pairs,p->grip_stats.metadata_bytes);
        okay=true;
    }catch(const char *message){fail(message);}
    catch(...){fail("Arm grip loading allocation failed");}
    std::fclose(file);if(!okay&&p->arm_grips!=out)free_arm_grips(out);return okay;
}
WorldCombatGripStats world_combat_motion_grip_stats(const WorldCombatMotion *p){return p?p->grip_stats:WorldCombatGripStats{};}
namespace {
bool copy_grip_overrides(std::vector<WorldModelVertexOverride> &staging,const world_arm_grip_stream::Output &output){
    for(std::size_t i=0;i<output.count;++i){const auto &v=output.vertices[i];if(v.index>=staging.size())return false;}
    for(std::size_t i=0;i<output.count;++i){const auto &source=output.vertices[i];auto &v=staging[source.index];
        std::copy_n(source.position,3,v.position);std::copy_n(source.normal,3,v.normal);std::copy_n(source.tangent,3,v.tangent);v.weight=1;
    }
    return true;
}
void commit_player_deformation(WorldCombatMotion *p,unsigned bank){
    if(!p||!p->locomotion||bank>=4)return;
    auto *grips=p->arm_grips;
    if(!grips){commit_locomotion(p,bank);return;}
    const auto started=std::chrono::steady_clock::now();
    grips->body_active=grips->arms_active=false;
    auto &body=p->locomotion->banks[bank];
    // Clear every old optional mask before combining this frame. In
    // particular, a stopped gait cannot leave the previous legs over a hit.
    for(auto &v:body.staging)v.weight=0;
    commit_locomotion(p,bank,false);
    const bool gait=p->locomotion->state.weight>0&&!p->actors[0].defeated;
    const int metadata=bank==0?0:bank==2?1:bank==3?2:-1;
    // Exact baked endpoints need no correction or redundant GPU upload.
    if(grips->healthy&&p->grip_weapon_allowed&&metadata>=0&&!p->actors[0].defeated&&p->weights[0]>0&&p->weights[0]<1){
        const unsigned slot=p->actors[0].slot;const auto pair=pose_pair(slot,p->pose_count);
        for(unsigned view=0;view<2;++view){
            const auto &rigid=grip_bank(p,unsigned(metadata),view!=0).rigid[slot];
            const auto native=grip_native_path(rigid,view!=0);
            world_arm_grip_stream::Output output;
            const auto result=world_arm_grip_stream::evaluate(grips->database,unsigned(metadata),view,pair[0],pair[1],p->weights[0],native,output);
            if(result==world_arm_grip_stream::Status::Rejected){grip_failure(p,"Certified arm evaluation rejected actual pose");break;}
            if(result==world_arm_grip_stream::Status::Disabled)continue;
            auto &staging=view?grips->arms[unsigned(metadata)].staging:body.staging;
            if(view)for(auto &v:staging)v.weight=0;
            if(!copy_grip_overrides(staging,output)){grip_failure(p,"Arm output escaped actual model layout");break;}
            if(view){
                auto &fp=grips->arms[unsigned(metadata)];
                if(!world_model_vertex_stream_upload(fp.stream,fp.staging.data(),fp.staging.size())){grip_failure(p,"First-person arm upload failed");break;}
                grips->arms_active=true;++p->grip_stats.arms_frames;++p->grip_stats.uploads;
            }else{grips->body_active=true;++p->grip_stats.body_frames;}
        }
    }
    if(!grips->healthy){for(auto &v:body.staging)v.weight=0;commit_locomotion(p,bank,false);}
    if(gait||grips->body_active){
        if(world_model_vertex_stream_upload(body.stream,body.staging.data(),body.staging.size())){++p->stats.locomotion_uploads;++p->grip_stats.uploads;}
        else grip_failure(p,"Combined player body upload failed");
    }
    ++p->grip_stats.frames;if(!grips->body_active&&!grips->arms_active)++p->grip_stats.disabled_frames;
    const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
    p->grip_stats.total_cpu_ms+=ms;p->grip_stats.maximum_cpu_ms=std::max(p->grip_stats.maximum_cpu_ms,ms);
}
}

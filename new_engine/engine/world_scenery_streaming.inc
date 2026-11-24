// Included once after WorldScenery/Group definitions. Actual world_regions
// callbacks consume worker-prepared model data; no legacy load is called here.
static bool scenery_source_decoder_scratch(const char *alias){
    // Decoder accounting is independent of the source chunk LOD/shadow policy.
    // Both imported publisher families release each image decoder before the
    // next one; do not accumulate that already-freed scratch against768MiB.
    return alias&&(std::strncmp(alias,"nm_scene_",9)==0||std::strncmp(alias,"nm_city_",8)==0);
}
struct SceneryStreaming {
    struct Definition {std::string primary,shadow;bool pinned=false,city=false;float minimum[3]={},maximum[3]={};};
    struct Payload {std::shared_ptr<const world_model_cpu::Asset> primary,shadow;};
    struct Handle {
        bool used=false,complete=false;std::size_t group=0;
        WorldModelAsset *primary=nullptr,*shadow=nullptr;
        WorldModelStaged *stages[2]={};
        unsigned phase=0;
    };
    WorldScenery *scene=nullptr;WorldSceneryStreamingConfig config;
    std::vector<Definition> definitions;
    std::array<Handle,group_cap*2> handles={};
    std::array<bool,group_cap> wanted_before={};
    std::array<unsigned char,group_cap> latest_demand={};
    std::array<std::uint64_t,group_cap> failure_texture_bytes={};
    world_regions::Coordinator coordinator;
    std::uint64_t frame=0;unsigned anisotropy=1,facade_anisotropy=1,max_texture_size=8192;bool bc7=false,configured=false;
    char error[192]={};
    bool city_suspended=false;std::uint64_t interior_policy_changes=0;
    explicit SceneryStreaming(WorldScenery *s,const WorldSceneryStreamingConfig &c):scene(s),config(c),city_suspended(c.interior_release_city_fallbacks&&c.interior_start_active){}
    bool interior_contains(double x,double y,double z)const{
        const double p[]={x,y,z};
        if(!config.interior_release_city_fallbacks)return false;
        for(unsigned k=0;k<3;k++)if(!std::isfinite(p[k])||p[k]<=config.interior_minimum[k]||p[k]>=config.interior_maximum[k])return false;
        return true;
    }
    bool suppressed(std::size_t i)const{return city_suspended&&definitions[i].city;}
    bool pinned(std::size_t i)const{
        if(owner_city_mid_deferred(i))return false;
        return definitions[i].pinned&&!suppressed(i);
    }
    // The accepted eager loader checks the shared texture cache before decode
    // and lets the driver generate mipmaps. Preparing every shared 4K city map
    // repeatedly on the CPU makes the apartment exit take minutes.
    // Restore only opted-in city MID fallbacks here, one owner-thread model per
    // update. Desired critical demand is retained until each adoption completes.
    std::array<bool,group_cap> owner_mid_fallback={};
    bool owner_city_mid_deferred(std::size_t i)const{
        if(city_suspended||!definitions[i].city||!definitions[i].pinned||
           scene->groups[i].source_lod!=1||scene->groups[i].asset||owner_mid_fallback[i])return false;
        const auto state=coordinator.state(i+1);
        return !state.resident&&!state.loading&&!state.failed;
    }
    void restore_one_city_mid(){
        for(std::size_t i=0;i<definitions.size();i++){
            if(!owner_city_mid_deferred(i))continue;
            auto &group=scene->groups[i];const auto &definition=definitions[i];
            Handle *handle=nullptr;std::size_t token=0;
            for(std::size_t k=0;k<handles.size();k++)if(!handles[k].used){handle=&handles[k];token=k+1;break;}
            const auto before=texture_cache_stats();const auto budgets=coordinator.stats();
            const auto started=SDL_GetTicks64();const char *reason=nullptr;
            std::unique_ptr<WorldModelAsset,decltype(&world_model_free)> asset(nullptr,world_model_free);
            if(!handle)reason="No free owner handle";
            else if(!definition.shadow.empty())reason="Unexpected MID shadow payload";
            else if(before.untracked_gpu_images||before.gpu_bytes>config.texture_bytes)reason="Existing texture admission failure";
            else asset.reset(world_model_load(definition.primary.c_str()));
            if(!reason&&!asset)reason="Eager city MID load failed";
            std::size_t geometry=0;float minimum[3]={},maximum[3]={};
            if(!reason){
                if(!world_model_bounds(asset.get(),minimum,maximum))reason="Missing restored MID bounds";
                else for(unsigned k=0;k<3;k++)if(std::abs(minimum[k]-definition.minimum[k])>1e-4f||
                     std::abs(maximum[k]-definition.maximum[k])>1e-4f){reason="Source MID bounds changed; reimport required";break;}
            }
            const auto after=texture_cache_stats();
            if(!reason){
                geometry=world_model_geometry_bytes(asset.get());
                const auto reserved=budgets.resident_gpu_bytes+budgets.pending_gpu_reservation;
                if(after.untracked_gpu_images||after.gpu_bytes>config.texture_bytes)reason="Restored MID exceeded texture budget";
                else if(reserved>config.geometry_bytes||geometry>config.geometry_bytes-reserved)reason="Restored MID exceeded geometry budget";
            }
            // Existing eager APIs expose exact shared GPU admission only after
            // upload. A single reviewed source model is the bounded temporary
            // peak; reject and release it before adoption on any excess.
            if(!reason){
                for(const auto &program:scene->uniforms)if(program.program&&program.epoch==world_program_epoch)
                    (void)world_model_warm(asset.get(),program.program);
                *handle={};handle->used=handle->complete=true;handle->group=i;
                handle->primary=asset.get();handle->phase=2;
                if(!coordinator.adopt(i+1,token,0,geometry)){*handle={};reason="Coordinator rejected restored MID ownership";}
            }
            if(reason){
                asset.reset();owner_mid_fallback[i]=true;
                std::snprintf(error,sizeof(error),"%s: %s; original asynchronous fallback retained",group.alias_name.c_str(),reason);
                std::fprintf(stderr,"[OpeningInterior] Owner MID restore rolled back: %s\n",error);
            }else{
                group.asset=asset.release();group.shadow_asset=nullptr;group.uploaded.clear();group.visible_count=0;
                (void)coordinator.pin(i+1,true);error[0]=0;
                std::printf("[OpeningInterior] Owner restored MID %s in %llu ms; textures %llu bytes, geometry %zu bytes, new textures %llu bytes\n",
                    group.alias_name.c_str(),(unsigned long long)(SDL_GetTicks64()-started),
                    (unsigned long long)after.gpu_bytes,geometry,
                    (unsigned long long)(after.gpu_bytes>=before.gpu_bytes?after.gpu_bytes-before.gpu_bytes:0));
            }
            break;
        }
    }
    ~SceneryStreaming(){configured=false;(void)coordinator.stop();}
    static std::shared_ptr<const world_regions::CpuBlob> load(const world_regions::LoadRequest &request,void *context){
        auto &s=*static_cast<SceneryStreaming *>(context);const auto index=std::size_t(request.region-1);if(index>=s.definitions.size())return {};
        world_model_cpu::Options options;options.cpu_bytes=request.cpu_limit;options.max_texture_size=s.max_texture_size;options.bc7=s.bc7;options.reuse_decoder_scratch=s.scene->source_bounds_lod&&scenery_source_decoder_scratch(s.scene->groups[index].alias_name.c_str());options.generation=request.current_generation;options.stopping=request.stopping;options.expected_generation=request.generation;
        auto payload=std::make_shared<Payload>();std::string message;payload->primary=world_model_cpu::prepare(s.definitions[index].primary.c_str(),options,message);
        if(!payload->primary){if(request.cancelled())return {};if(request.error&&request.error_capacity)std::snprintf(request.error,request.error_capacity,"%s HIGH: %s",s.scene->groups[index].alias_name.c_str(),message.c_str());std::fprintf(stderr,"[SceneryStreaming] %s HIGH prepare failed: %s (CPU limit %.1f MiB)\n",s.scene->groups[index].alias_name.c_str(),message.c_str(),request.cpu_limit/1048576.);return {};}
        const auto &definition=s.definitions[index];for(unsigned i=0;i<3;i++)if(std::abs(payload->primary->minimum[i]-definition.minimum[i])>1e-4f||std::abs(payload->primary->maximum[i]-definition.maximum[i])>1e-4f){if(request.error&&request.error_capacity)std::snprintf(request.error,request.error_capacity,"Source geometry bounds changed; reimport the asset before streaming");std::fprintf(stderr,"[SceneryStreaming] Load failed alias=%s LOD=%u primary=%s reason=bounds-mismatch axis=%u expected=%.9g..%.9g actual=%.9g..%.9g\n",s.scene->groups[index].alias_name.c_str(),s.scene->groups[index].source_lod,definition.primary.c_str(),i,double(definition.minimum[i]),double(definition.maximum[i]),double(payload->primary->minimum[i]),double(payload->primary->maximum[i]));return {};}
        std::uint64_t cpu=sizeof(world_regions::CpuBlob)+sizeof(Payload)+payload->primary->cpu_bytes,gpu=payload->primary->geometry_bytes,upload=payload->primary->upload_bytes;
        if(!s.definitions[index].shadow.empty()){
            if(cpu>=request.cpu_limit){std::fprintf(stderr,"[SceneryStreaming] Load failed alias=%s LOD=%u primary=%s shadow=%s reason=no-shadow-CPU-reservation primary_cpu=%llu budget=%llu\n",s.scene->groups[index].alias_name.c_str(),s.scene->groups[index].source_lod,definition.primary.c_str(),definition.shadow.c_str(),(unsigned long long)cpu,(unsigned long long)request.cpu_limit);return {};}
            options.cpu_bytes=request.cpu_limit-cpu;
            payload->shadow=world_model_cpu::prepare(s.definitions[index].shadow.c_str(),options,message);
            if(!payload->shadow){if(request.cancelled())return {};if(request.error&&request.error_capacity)std::snprintf(request.error,request.error_capacity,"%s shadow: %s",s.scene->groups[index].alias_name.c_str(),message.c_str());std::fprintf(stderr,"[SceneryStreaming] %s shadow prepare failed: %s (CPU limit %.1f MiB)\n",s.scene->groups[index].alias_name.c_str(),message.c_str(),options.cpu_bytes/1048576.);return {};}
            cpu+=payload->shadow->cpu_bytes;gpu+=payload->shadow->geometry_bytes;upload+=payload->shadow->upload_bytes;
        }
        if(request.cancelled())return {};
        if(cpu>request.cpu_limit){std::fprintf(stderr,"[SceneryStreaming] Load failed alias=%s LOD=%u primary=%s reason=CPU-blob-overflow cpu=%llu budget=%llu\n",s.scene->groups[index].alias_name.c_str(),s.scene->groups[index].source_lod,definition.primary.c_str(),(unsigned long long)cpu,(unsigned long long)request.cpu_limit);return {};}
        if(gpu>s.config.geometry_bytes||upload>s.config.cpu_bytes+s.config.geometry_bytes)std::fprintf(stderr,"[SceneryStreaming] Load exceeds coordinator reservation alias=%s LOD=%u primary=%s geometry=%llu geometry_budget=%llu transfer=%llu transfer_budget=%llu\n",s.scene->groups[index].alias_name.c_str(),s.scene->groups[index].source_lod,definition.primary.c_str(),(unsigned long long)gpu,(unsigned long long)s.config.geometry_bytes,(unsigned long long)upload,(unsigned long long)(s.config.cpu_bytes+s.config.geometry_bytes));
        auto blob=std::make_shared<world_regions::CpuBlob>();blob->payload=payload;blob->cpu_bytes=cpu;blob->gpu_bytes=gpu;blob->upload_bytes=upload;blob->upload_bytes_upper_bound=true;return blob;
    }
    static world_regions::UploadResult upload(const world_regions::UploadRequest &request,void *context){
        auto &s=*static_cast<SceneryStreaming *>(context);world_regions::UploadResult result;const auto index=std::size_t(request.region-1);if(index>=s.definitions.size()||!request.blob){result.success=false;return result;}
        Handle *h=nullptr;std::size_t token=std::size_t(request.token);
        if(token){if(token>s.handles.size()||!s.handles[token-1].used){result.success=false;return result;}h=&s.handles[token-1];}
        else{
            for(std::size_t i=0;i<s.handles.size();i++)if(!s.handles[i].used){h=&s.handles[i];token=i+1;*h={};h->used=true;h->group=index;break;}
            if(!h){std::fprintf(stderr,"[SceneryStreaming] Upload failed alias=%s LOD=%u primary=%s reason=no-free-handle\n",s.scene->groups[index].alias_name.c_str(),s.scene->groups[index].source_lod,s.definitions[index].primary.c_str());result.success=false;return result;}
            const auto payload=std::static_pointer_cast<const Payload>(request.blob->payload);
            if(!payload||!(h->stages[0]=world_model_stage(payload->primary,s.anisotropy,s.facade_anisotropy))||(payload->shadow&&!(h->stages[1]=world_model_stage(payload->shadow,s.anisotropy,s.facade_anisotropy)))){std::fprintf(stderr,"[SceneryStreaming] Upload failed alias=%s LOD=%u primary=%s shadow=%s reason=stage-initialization\n",s.scene->groups[index].alias_name.c_str(),s.scene->groups[index].source_lod,s.definitions[index].primary.c_str(),s.definitions[index].shadow.c_str());result.success=false;result.token=token;return result;}
        }
        result.token=token;
        const auto texture_stats=texture_cache_stats();const auto textures=texture_stats.gpu_bytes;
        std::uint64_t missing=0;for(auto *stage:h->stages)missing+=world_model_staged_new_texture_bytes(stage);
        if(texture_stats.untracked_gpu_images||textures>s.config.texture_bytes||missing>s.config.texture_bytes-textures){std::snprintf(s.error,sizeof(s.error),"Texture admission failed; pinned fallback stays visible (%llu MiB resident)",static_cast<unsigned long long>(textures/(1024*1024)));std::fprintf(stderr,"[SceneryStreaming] Texture admission failed alias=%s LOD=%u primary=%s shadow=%s resident=%llu missing=%llu budget=%llu untracked=%zu; pinned fallback retained\n",s.scene->groups[index].alias_name.c_str(),s.scene->groups[index].source_lod,s.definitions[index].primary.c_str(),s.definitions[index].shadow.c_str(),(unsigned long long)textures,(unsigned long long)missing,(unsigned long long)s.config.texture_bytes,texture_stats.untracked_gpu_images);s.failure_texture_bytes[index]=textures;result.success=false;return result;}
        while(h->phase<2&&result.consumed_bytes<request.byte_budget){
            auto *stage=h->stages[h->phase];if(!stage){h->phase++;continue;}
            std::size_t bytes=0;bool complete=false;
            if(!world_model_stage_step(stage,std::size_t(request.byte_budget-result.consumed_bytes),&bytes,&complete)){std::fprintf(stderr,"[SceneryStreaming] Upload failed alias=%s LOD=%u primary=%s phase=%u reason=stage-step\n",s.scene->groups[index].alias_name.c_str(),s.scene->groups[index].source_lod,s.definitions[index].primary.c_str(),h->phase);result.success=false;return result;}
            result.consumed_bytes+=bytes;
            if(!complete)break;
            auto *asset=world_model_stage_take(stage);if(!asset)std::fprintf(stderr,"[SceneryStreaming] Upload failed alias=%s LOD=%u primary=%s phase=%u reason=completed-stage-has-no-asset\n",s.scene->groups[index].alias_name.c_str(),s.scene->groups[index].source_lod,s.definitions[index].primary.c_str(),h->phase);world_model_stage_free(stage);h->stages[h->phase]=nullptr;
            for(const auto &program:s.scene->uniforms)if(program.program&&program.epoch==world_program_epoch)(void)world_model_warm(asset,program.program);
            if(h->phase==0)h->primary=asset;else h->shadow=asset;h->phase++;
        }
        h->complete=h->phase==2;result.complete=h->complete;result.gpu_bytes=h->complete?request.blob->gpu_bytes:0;if(h->complete)s.error[0]=0;return result;
    }
    static void release(world_regions::Id,std::uint64_t token,void *context){
        auto &s=*static_cast<SceneryStreaming *>(context);if(!token||token>s.handles.size())return;auto &h=s.handles[std::size_t(token-1)];if(!h.used)return;
        auto &group=s.scene->groups[h.group];
        // Coordinator shutdown returns complete committed handles to the scene;
        // partial uploads are always destroyed. The scene remains valid if an
        // editor preparation fails after streaming has been suspended.
        const bool stopping=!s.configured;
        if(group.asset==h.primary){if(!stopping){group.asset=nullptr;group.shadow_asset=nullptr;group.visible_count=0;group.uploaded.clear();}else{h.primary=nullptr;h.shadow=nullptr;}}
        for(auto *stage:h.stages)world_model_stage_free(stage);
        world_model_free(h.primary);world_model_free(h.shadow);h={};
    }
    bool init(){
        if(scene->groups.size()>world_regions::region_limit){std::snprintf(error,sizeof(error),"Scene exceeds streaming group capacity");return false;}
        GLint maximum=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);if(maximum>0)max_texture_size=unsigned(maximum);
        if(SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic")||SDL_GL_ExtensionSupported("GL_ARB_texture_filter_anisotropic")){float limit=1;glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,&limit);if(std::isfinite(limit)&&limit>=2){anisotropy=unsigned(std::min(4.f,limit));facade_anisotropy=unsigned(std::min(8.f,limit));}}
        const char *disabled=SDL_getenv("VALWOOD_QA_DISABLE_OPAQUE_BC7");bc7=!(disabled&&!std::strcmp(disabled,"1"))&&SDL_GL_ExtensionSupported("GL_ARB_texture_compression_bptc");
        world_regions::Document document;definitions.reserve(scene->groups.size());document.regions.reserve(scene->groups.size());
        for(std::size_t i=0;i<scene->groups.size();i++){
            const auto &g=scene->groups[i];Definition d;d.primary=g.asset_path;d.pinned=g.source_lod!=0||aliases[g.alias].lods==1;d.city=config.interior_release_city_fallbacks&&world_opening_interior::city_alias(g.alias_name.c_str());
            std::copy(g.minimum,g.minimum+3,d.minimum);std::copy(g.maximum,g.maximum+3,d.maximum);
            if(g.source_lod==0&&(aliases[g.alias].tree||aliases[g.alias].cast_shadow)&&!(scene->source_bounds_lod&&source_chunk(g.alias)))d.shadow=scene->model_directory+"/"+aliases[g.alias].stem+"_mid.glb";
            definitions.push_back(std::move(d));world_regions::Region region;region.id=i+1;region.name=g.alias_name.substr(0,80)+" LOD "+std::to_string(g.source_lod);if(config.interior_release_city_fallbacks&&definitions.back().pinned)region.priority=1;region.bounds.minimum={0,-1,0};region.bounds.maximum={1,1,1};world_regions::Layer layer;layer.id=i+1;region.layers.push_back(std::move(layer));document.regions.push_back(std::move(region));
        }
        world_regions::Config limits;limits.cpu_bytes=config.cpu_bytes;limits.gpu_bytes=config.geometry_bytes;limits.upload_bytes_per_frame=config.upload_bytes_per_frame;limits.upload_jobs_per_frame=1;
        std::string message;if(!coordinator.configure(document,limits,{load,upload,release,this},message)){std::snprintf(error,sizeof(error),"%s",message.c_str());return false;}
        for(std::size_t i=0;i<scene->groups.size();i++){
            auto &g=scene->groups[i];if(g.asset){auto &h=handles[i];h.used=h.complete=true;h.group=i;h.primary=g.asset;h.shadow=g.shadow_asset;if(!coordinator.adopt(i+1,i+1,0,world_model_geometry_bytes(g.asset)+world_model_geometry_bytes(g.shadow_asset))){std::snprintf(error,sizeof(error),"Cannot adopt current scenery model");return false;}}
            coordinator.demand(i+1,pinned(i)?2:0,1e7);if(pinned(i))coordinator.pin(i+1,true);
        }
        configured=true;return true;
    }
    void update(double x,double y,double z){
        if(!configured||!std::isfinite(x)||!std::isfinite(z))return;
        const bool next_suspended=interior_contains(x,y,z);
        if(next_suspended!=city_suspended){
            city_suspended=next_suspended;++interior_policy_changes;
            owner_mid_fallback.fill(false);
            for(std::size_t i=0;i<definitions.size();i++)if(definitions[i].city)
                coordinator.pin(i+1,pinned(i));
            std::printf("[OpeningInterior] City fallback policy: %s at %.2f %.2f %.2f\n",city_suspended?"suspended":"restored",x,y,z);
        }
        restore_one_city_mid();
        for(std::size_t i=0;i<scene->groups.size();i++){
            const auto &g=scene->groups[i];double nearest=1e7;
            for(auto record:g.records){const auto &p=scene->placements[record];if(!p.active)continue;
                const double distance=scene->source_bounds_lod&&source_chunk(g.alias)&&!source_backdrop(g.alias)?
                    source_distances(p,g.lod_minimum,g.lod_maximum,x,y,z).nearest:streaming_distance(p,g.minimum,g.maximum,x,z);
                nearest=std::min(nearest,distance);
            }
            const auto state=coordinator.state(i+1);const bool retain=state.resident||state.loading;
            const bool unused_horizon_high=scene->source_horizon&&source_backdrop(g.alias)&&g.source_lod==0;
            const int demand=(unused_horizon_high||suppressed(i))?0:(pinned(i)?2:(nearest<=g.hi?2:(nearest<=g.hi+(retain?config.unload_meters:config.preload_meters)?1:0)));
            const bool owner_deferred=owner_city_mid_deferred(i);
            latest_demand[i]=static_cast<unsigned char>(owner_deferred?2:demand);
            const auto texture_bytes=texture_cache_stats().gpu_bytes;
            if(state.failed&&demand&&(!wanted_before[i]||(failure_texture_bytes[i]>texture_bytes&&failure_texture_bytes[i]-texture_bytes>=32ull*1024*1024)))(void)coordinator.reload(i+1);
            wanted_before[i]=demand!=0;
            coordinator.demand(i+1,owner_deferred?0:demand,nearest);
        }
        coordinator.request({x,0,z},++frame);
        for(std::size_t i=0;i<scene->groups.size();i++)if(!pinned(i)&&!coordinator.state(i+1).wanted)(void)coordinator.retire(i+1);
        coordinator.tick();
        for(std::size_t i=0;i<scene->groups.size();i++){const auto state=coordinator.state(i+1);if(state.resident&&state.token&&state.token<=handles.size()){const auto &h=handles[std::size_t(state.token-1)];if(h.complete&&scene->groups[i].asset!=h.primary){auto &g=scene->groups[i];g.asset=h.primary;g.shadow_asset=h.shadow;g.uploaded.clear();g.visible_count=0;}}}
    }
};
static bool scenery_stream_config_valid(const WorldSceneryStreamingConfig &config){
    if(config.interior_release_city_fallbacks)for(unsigned k=0;k<3;k++)
        if(!std::isfinite(config.interior_minimum[k])||!std::isfinite(config.interior_maximum[k])||
           config.interior_maximum[k]-config.interior_minimum[k]<.1||config.interior_maximum[k]-config.interior_minimum[k]>1024.)return false;
    return config.cpu_bytes>=1024*1024&&config.cpu_bytes<=(1ull<<48)&&config.geometry_bytes&&config.geometry_bytes<=(1ull<<48)&&config.texture_bytes&&config.upload_bytes_per_frame>=128*1024&&std::isfinite(config.preload_meters)&&std::isfinite(config.unload_meters)&&config.preload_meters>=64&&config.unload_meters>=config.preload_meters;}
bool world_scenery_streaming(WorldScenery *scene,bool enabled,const WorldSceneryStreamingConfig &config,char *error,std::size_t capacity){
    if(!scene||scene->borrow_source)return false;
    if(!enabled){
        scene->stream_requested=false;if(scene->streaming)scene->streaming->configured=false;scene->streaming.reset();
        for(auto &g:scene->groups)if(!g.asset&&!(scene->source_horizon&&source_backdrop(g.alias)&&g.source_lod==0)){
            std::unique_ptr<WorldModelAsset,decltype(&world_model_free)> asset(world_model_load(g.asset_path.c_str()),world_model_free),shadow(nullptr,world_model_free);
            if(!asset){error_text(error,capacity,"Cannot restore eager scenery model: "+g.asset_path);return false;}
            if(g.source_lod==0&&(aliases[g.alias].tree||aliases[g.alias].cast_shadow)&&!(scene->source_bounds_lod&&source_chunk(g.alias))){const auto path=scene->model_directory+"/"+aliases[g.alias].stem+"_mid.glb";shadow.reset(world_model_load(path.c_str()));if(!shadow){error_text(error,capacity,"Cannot restore eager shadow proxy: "+path);return false;}}
            g.asset=asset.release();g.shadow_asset=shadow.release();g.uploaded.clear();g.visible_count=0;
        }return true;
    }
    if(scene->streaming)return true;
    if(!scenery_stream_config_valid(config)){error_text(error,capacity,"Invalid scenery streaming budgets");return false;}
    try{auto stream=std::make_unique<SceneryStreaming>(scene,config);if(!stream->init()){error_text(error,capacity,stream->error);return false;}scene->stream_config=config;scene->stream_requested=true;scene->streaming=std::move(stream);return true;}catch(...){error_text(error,capacity,"Cannot initialize scenery streaming");return false;}
}
WorldSceneryStreamingStats world_scenery_streaming_stats(const WorldScenery *scene){
    WorldSceneryStreamingStats result;if(!scene||!scene->streaming)return result;const auto &s=*scene->streaming;const auto stats=s.coordinator.stats();result.enabled=true;result.interior_city_suspended=s.city_suspended;result.interior_policy_changes=s.interior_policy_changes;for(const auto &d:s.definitions)result.interior_city_groups+=d.city;result.resident=stats.resident;result.wanted=stats.wanted;result.loading=stats.loading;result.failed=stats.failed;result.geometry_bytes=stats.resident_gpu_bytes+stats.pending_gpu_reservation;result.texture_bytes=texture_cache_stats().gpu_bytes;result.upload_bytes=stats.frame_upload_bytes;result.loads=stats.loads;result.evictions=stats.evictions;result.cancellations=stats.cancellations;for(std::size_t i=0;i<scene->groups.size();i++){result.fallback_groups+=scene->groups[i].fallback;const auto state=s.coordinator.state(i+1);if((!s.definitions[i].pinned||(s.definitions[i].city&&!s.city_suspended))&&s.latest_demand[i]==2&&!state.resident&&!state.failed){
        // City/source HIGH is optional refinement when the MID of the same
        // spatial batch is already usable. A geometry-budget wait must not
        // trap travel, controls or the presentation route behind a loading UI.
        const auto &group=scene->groups[i];
        const bool city_mid=world_opening_interior::city_alias(group.alias_name.c_str());
        const bool usable_source_mid=(source_chunk(group.alias)||city_mid)&&std::any_of(scene->groups.begin(),scene->groups.end(),[&](const Group &other){return other.alias==group.alias&&other.source_lod==1&&other.asset&&(!city_mid||s.coordinator.state(std::size_t(&other-scene->groups.data())+1).resident);});
        result.critical_pending+=!usable_source_mid;
    }}std::snprintf(result.error,sizeof(result.error),"%s",*s.error?s.error:stats.last_error.data());return result;
}

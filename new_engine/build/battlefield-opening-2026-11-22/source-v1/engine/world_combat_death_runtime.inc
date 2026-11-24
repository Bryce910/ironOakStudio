// Included by motion.cpp. Full-PNT streams exist only for the assigned NPC
// bodies during their short death entry. All ordinary and settled draws keep
// using the original shared immutable pose batches.
namespace {
bool death_prepare(WorldCombatMotion *p){
    if(!p||!p->anatomy||!p->stats.polished)return false;
    if(p->deaths)return true;
    auto *g=new(std::nothrow)DeathBridges;if(!g)return false;
    const auto vertices=world_model_pose_vertices(p->body.assets[0]);
    const auto cap_vertices=world_model_pose_vertices(p->anatomy->cap_assets[0]);
    bool ok=vertices>0&&vertices<=100000&&cap_vertices>0&&cap_vertices<=100000;
    try{for(unsigned who=1;ok&&who<actor_capacity;++who){if(!(p->actor_mask&(1u<<who)))continue;auto &v=g->actors[who];
        v.frozen.resize(vertices);v.staging.resize(vertices);
        v.cap_frozen.resize(cap_vertices);v.cap_staging.resize(cap_vertices);
        v.body=world_model_pose_blend_create(1,WORLD_MODEL_POSE_BLEND_VERSION);
        v.cap=world_model_pose_blend_create(1,WORLD_MODEL_POSE_BLEND_VERSION);
        v.stream=world_model_vertex_stream_create(p->body.assets[0]);
        v.cap_stream=world_model_vertex_stream_create(p->anatomy->cap_assets[0]);
        ok=v.body&&v.cap&&v.stream&&v.cap_stream&&
            world_model_pose_blend_set_targets(v.body,p->body.assets[p->compact?0:148],p->body.assets[p->compact?0:149])&&
            world_model_pose_blend_set_targets(v.cap,p->anatomy->cap_assets[148],p->anatomy->cap_assets[149])&&
            world_model_pose_blend_attach_anatomy(v.body,p->anatomy->stream)&&
            world_model_pose_blend_attach_vertex_stream(v.body,v.stream)&&
            world_model_pose_blend_attach_vertex_stream(v.cap,v.cap_stream);
        const WorldModelInstance seed={0,0,0,0,1};const float zero=0;
        ok=ok&&world_model_pose_blend_upload(v.body,&seed,&zero,1)&&world_model_pose_blend_upload(v.cap,&seed,&zero,1);
    }}catch(...){ok=false;}
    if(!ok){free_deaths(g);return false;}
    for(unsigned i=0;i<poses;++i)g->stats.source_cpu_bytes+=world_model_pose_pnt_bytes(p->body.assets[i])+world_model_pose_pnt_bytes(p->anatomy->cap_assets[i]);
    g->stats.staging_cpu_bytes=sizeof(DeathBridges);
    for(unsigned who=1;who<actor_capacity;++who){if(!(p->actor_mask&(1u<<who)))continue;const auto &v=g->actors[who];
        g->stats.staging_cpu_bytes+=(v.frozen.capacity()+v.staging.capacity()+v.cap_frozen.capacity()+v.cap_staging.capacity())*sizeof(WorldModelVertexOverride);
        g->stats.extra_gpu_bytes+=(vertices+cap_vertices)*sizeof(WorldModelVertexOverride);
    }
    g->stats.loaded=true;p->deaths=g;return true;
}
void death_reset(WorldCombatMotion *p){if(p&&p->deaths)for(auto &v:p->deaths->actors){v.pose={};v.source_snapshot={};v.elapsed=0;v.mirrored=false;v.uploaded=v.crowd_override=v.clash=false;}}
bool death_begin(WorldCombatMotion *p,unsigned who,unsigned slot,float weight,float yaw,bool mirror,double clock){
    if(!p->deaths||!who||who>=actor_capacity)return false;
    auto &g=*p->deaths;auto &v=g.actors[who];const bool from_clash=v.clash&&(v.uploaded||(v.source_snapshot.frozen_actor==who&&v.source_snapshot.frozen_generation==v.frozen_generation));auto old_snapshot=v.source_snapshot;old_snapshot.mirrored=mirror;
    if(from_clash){std::copy(v.staging.begin(),v.staging.end(),v.frozen.begin());std::copy(v.cap_staging.begin(),v.cap_staging.end(),v.cap_frozen.begin());}
    v.pose={};v.elapsed=0;v.uploaded=v.crowd_override=v.clash=false;
    // Keep the actual previous rendered sample, including its facing. Contact
    // prediction may advance within this simulation tick; it governs the
    // detached piece but must not silently replace this continuity endpoint.
    (void)clock;AnatomySnapshot gait_source;gait_source.slot=slot;gait_source.weight=weight;guarded_gait_snapshot(p,p->actors[who],gait_source);
    if(slot>=p->batch_count||(!from_clash&&(!compact_sample(p,slot,weight,v.frozen.data(),v.frozen.size())||!guarded_gait_sample(p,gait_source,v.frozen.data(),v.frozen.size())||
       !world_model_pose_blend_sample_pnt(p->anatomy->cap_blends[slot],weight,v.cap_frozen.data(),v.cap_frozen.size())))){++g.stats.failures;return false;}
    v.source_snapshot=from_clash?old_snapshot:gait_source;if(!from_clash){v.source_snapshot.slot=slot;v.source_snapshot.weight=weight;v.source_snapshot.mirrored=mirror;}
    v.pose.valid=true;v.pose.bridging=true;v.pose.source_slot=slot;v.pose.source_weight=weight;v.pose.alpha=0;v.pose.yaw=yaw;v.mirrored=mirror;
    ++g.stats.captures;return true;
}
bool death_tick(WorldCombatMotion *p,unsigned who,float seconds){
    if(!p->deaths||!who||who>=actor_capacity)return false;
    auto &g=*p->deaths;auto &v=g.actors[who];if(!v.pose.valid)return false;
    // A stable corpse can be encountered by live-actor and crowd paths in one
    // frame. Never rewind its render clock or re-enter a completed bridge.
    if(!std::isfinite(seconds))return false;
    v.elapsed=std::max(v.elapsed,std::clamp(seconds,0.f,world_combat_motion::death_duration));
    const auto sample=world_combat_death_transition::sample(v.elapsed);
    const bool changed=v.pose.alpha!=sample.alpha||v.pose.slot!=sample.slot||v.pose.weight!=sample.weight;
    const bool was_bridging=v.pose.bridging;
    v.pose.bridging=sample.bridging;v.pose.slot=sample.slot;v.pose.weight=sample.weight;v.pose.phase=sample.phase;v.pose.alpha=sample.alpha;
    if(was_bridging&&!sample.bridging)++g.stats.completions;
    // The first zero-time commit still uploads the captured pose once.
    // If a crowd batch was prepared just before this actor crossed .20s,
    // keep its one-actor draw until the next crowd publication. Upload the
    // exact current authored endpoint instead of dropping it for one frame.
    if(!sample.bridging&&!v.crowd_override)return true;
    if(!changed&&v.uploaded)return true;
    const auto start=std::chrono::steady_clock::now();
    const bool ok=compact_bridge(p,sample.slot,sample.weight,v.frozen.data(),sample.alpha,v.staging.data(),v.staging.size())&&
        world_model_pose_blend_bridge_pnt(p->anatomy->cap_blends[sample.slot],sample.weight,v.cap_frozen.data(),sample.alpha,v.cap_staging.data(),v.cap_staging.size())&&
        world_model_vertex_stream_upload(v.stream,v.staging.data(),v.staging.size())&&
        world_model_vertex_stream_upload(v.cap_stream,v.cap_staging.data(),v.cap_staging.size());
    g.stats.maximum_update_ms=std::max(g.stats.maximum_update_ms,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
    if(!ok){++g.stats.failures;v.pose.bridging=false;return false;}
    ++g.stats.frames;v.uploaded=true;return true;
}
bool death_active(const WorldCombatMotion *p,unsigned who){return p&&p->deaths&&who>0&&who<actor_capacity&&p->deaths->actors[who].pose.valid&&(p->deaths->actors[who].pose.bridging||p->deaths->actors[who].crowd_override);}
bool death_wound_local(const WorldCombatMotion *p,unsigned who,unsigned part,unsigned slot,float weight,bool mirror,AnatomyPoint &point,AnatomyPoint &axis){
    if(!death_active(p,who)||!p->anatomy)return false;const auto &v=p->deaths->actors[who];
    const auto &s=v.pose;auto q=clash_wound(p,v.source_snapshot,part,false);
    auto n=clash_wound(p,v.source_snapshot,part,true);
    point=world_combat_death_transition::blend(q,anatomy_wound_point(*p->anatomy,slot,weight,part,mirror,false),s.alpha);
    axis=world_combat_death_transition::blend(n,anatomy_wound_point(*p->anatomy,slot,weight,part,mirror,true),s.alpha);
    if(anatomy_math::length(axis)<1.e-9)axis=s.alpha<.5f?n:anatomy_wound_point(*p->anatomy,slot,weight,part,mirror,true);
    return true;
}
bool death_draw_actor(WorldCombatMotion *p,unsigned program,unsigned who,AnatomySnapshot snapshot,unsigned mask,TrainingWeaponHand hand,bool crowd=false){
    if(!death_active(p,who))return false;auto &g=*p->deaths;auto &v=g.actors[who];snapshot.instance.yaw=v.pose.yaw;
    WorldModelAnatomyDraw body={};body.removed=anatomy_expanded(mask);body.mirrored=v.mirrored;body.hide_weapon=hand==TrainingWeaponHand::None;body.variant_mask=world_model_variants::actor(who);
    body.x=snapshot.instance.x;body.y=snapshot.instance.y;body.z=snapshot.instance.z;body.yaw=snapshot.instance.yaw;body.scale=snapshot.instance.scale;body.rotation[3]=1;body.cap_side=1;
    world_model_pose_blend_draw_anatomy_deformed(v.body,program,&body);++g.stats.body_draws;
    if(mask){body.caps=true;body.removed=anatomy_caps_mask(mask);world_model_pose_blend_draw_anatomy_deformed(v.cap,program,&body);++g.stats.cap_draws;}
    if(crowd)++g.stats.crowd_draws;
    if(p->variant_stats.loaded){++p->variant_stats.body_draws[(who-1)%4];if(crowd)++p->variant_stats.crowd_draws[(who-1)%4];}
    if(p->anatomy){++p->anatomy->stats.body_draws;if(mask)++p->anatomy->stats.cap_draws;if(v.mirrored)++p->anatomy->stats.mirrored_draws;}
    return true;
}
}

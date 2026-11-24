// Ambient road life owns its routes and pose banks independently of Studio's
// authored NPC slots and the three arrival-camp conversation characters.
WorldRoadTraffic *road_traffic=nullptr;
static thread_local bool road_traffic_validation_active=false;
bool road_traffic_loaded=false,road_traffic_validated=false,road_traffic_route_failed=false;
WorldRoadTrafficConfig road_traffic_config;
WorldRoadTrafficQuery road_traffic_query;
road_traffic_query.context=scenery;
road_traffic_query.ground=+[](double x,double z,double reference,double &height,const void *){
    if(!std::isfinite(x)||!std::isfinite(z)||!std::isfinite(reference)||
       world_atlas_local_water(x,z,nullptr,nullptr))return false;
    height=std::max(double(world_atlas_walk_height(x,z)),0.);
    if(active_playtest_walk){
        float source=0;
        if(active_playtest_walk->sample(x,z,reference,source))height=source;
        else if(!active_structure_world)return false;
    }
    return std::isfinite(height)&&height>.1&&std::abs(height-reference)<2;
};
road_traffic_query.clear=+[](double ax,double az,double bx,double bz,double feet,double height,double radius,const void *data){
    const auto *scene=static_cast<const WorldScenery *>(data);
    auto fail=[&](const char *why){
        if(road_traffic_validation_active)std::fprintf(stderr,"[Road traffic collision] %s at %.3f %.3f -> %.3f %.3f feet%.3f height%.3f radius%.3f\n",why,ax,az,bx,bz,feet,height,radius);
        return false;
    };
    if(!world_scenery_can_move(scene,ax,az,bx,bz,float(feet),float(height),float(radius)))return fail("static scenery");
    if(!generated_cover_collision_ready||!generated_cover_collision.can_move(ax,az,bx,bz,feet,height,radius))return fail("generated vegetation or rocks");
    if(!world_stronghold_can_move(active_stronghold,ax,az,bx,bz,feet,float(radius)))return fail("stronghold");
    double next=std::max(double(world_atlas_walk_height(bx,bz)),0.);
    if(active_playtest_walk){
        float source=0;
        if(active_playtest_walk->sample(bx,bz,feet,source))next=source;
        else if(!active_structure_world)return false;
    }
    if(next-feet>world_playtest_walk::max_step+.002||feet-next>world_playtest_walk::max_drop+.002)return fail("step or drop");
    const float r=float(std::min(radius,height*.49));
    const float half=float(std::max(.01,height*.5-r));
    const world_physics::Vec3 center{ax,feet+height*.5+.015,az},delta{bx-ax,next-feet,bz-az};
    auto blocked=[](const world_physics::Hit &hit){return hit.hit&&hit.fraction<.999&&hit.normal.y<.65;};
    if(active_playtest_walk){world_physics::Hit hit;if(!active_playtest_walk->cast_capsule_movement(center,r,half,delta,hit)||blocked(hit))return fail("source support collision");}
    if(active_city_physics){world_physics::Hit hit;if(!active_city_physics->cast_capsule(center,r,half,delta,hit)||blocked(hit))return fail("city physics");}
    if(!world_live_actor_move_clear(active_live_visuals,active_world_runtime,ax,az,bx,bz,float(feet),float(height),float(radius)))return fail("living actors");
    if(active_world_runtime&&active_world_runtime->configured()){
        world_physics::Hit hit;world_runtime::Id entity=0;
        if(!active_world_runtime->cast_capsule(center,r,half,delta,hit,entity)||blocked(hit))return fail("authored physics");
    }
    return true;
};
if(first_journey_enabled&&std::filesystem::is_regular_file("opening/road-traffic.settings")){
    char error[512]={};
    if(!world_road_traffic_config_load("opening/road-traffic.settings",road_traffic_config,error,sizeof error)){
        std::fprintf(stderr,"[Road traffic] Invalid authored route: %s\n",error);return 1;
    }
    startup_status("walking travelers, mounted riders and market carts");
    road_traffic=world_road_traffic_load("models","road-traffic/manifest.cfg",road_traffic_config,error,sizeof error);
    if(!road_traffic){std::fprintf(stderr,"[Road traffic] Assets failed: %s\n",error);return 1;}
    road_traffic_loaded=true;
}
auto road_traffic_tick=[&](double step){
    if(!road_traffic)return;
    const bool outdoors=opening_state.stage()==world_opening_prologue::Stage::MeadowReveal||first_journey_outdoors();
    // Destination collision/support owners settle only at the end of loading.
    // The apartment's support must never validate an outdoor route mid-transport.
    if(outdoors&&!destination_loading&&!road_traffic_validated&&!road_traffic_route_failed){
        char error[512]={};
        road_traffic_validation_active=true;
        const bool route_ready=world_road_traffic_validate_route(road_traffic,road_traffic_query,error,sizeof error);
        road_traffic_validation_active=false;
        if(!route_ready){
            std::fprintf(stderr,"[Road traffic] Route is obstructed: %s\n",error);
            // An artist can obstruct an ambient route. Keep the story playable,
            // quarantine the invalid traffic and report the actual bad sample.
            road_traffic_route_failed=true;active_road_traffic=nullptr;return;
        }
        road_traffic_validated=true;
        std::printf("[Road traffic] Actual road lanes and turn clearings validated\n");
    }
    WorldRoadTrafficFrame frame;
    frame.dt=step;frame.player_x=cam.x;frame.player_feet_y=cam.y-1.75;frame.player_z=cam.z;
    frame.enabled=outdoors&&road_traffic_validated&&!destination_loading;
    // Ambient travelers remain alive behind conversation/cinematic shots.
    // User pause, focus loss and loading freeze their simulation clock.
    frame.paused=preview_screen!=1||show_map||show_graphics||studio.open||destination_loading||!preview_pad.focused;
    // The satchel remains an active interaction after the raider falls. Keep
    // both quest envelopes clear until the player actually recovers it.
    frame.encounter_cleared=first_journey_state.supplies_recovered();frame.query=road_traffic_query;
    world_road_traffic_update(road_traffic,frame);
    active_road_traffic=frame.enabled?road_traffic:nullptr;
};

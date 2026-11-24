#include "engine/world_motion_filter.h"
// Begin CPU authoring admission. Keep this ahead of GL staging: a component
// configuration must fit every potential visual, including initially hidden
// objects that gameplay can activate later. The two seed crates share the same
// bounded renderer pool as authored physical models.
bool world_live_visuals_validate(const world_studio::Document &scene,
                                 const world_runtime::Document &components,
                                 const world_runtime_demo::Seed &seed,
                                 const std::vector<world_runtime::Entity> &entities,
                                 std::string &error){
    unsigned actors=0,rigids=0;
    for(const auto &visual:seed.visuals){
        bool present=false;for(const auto &entity:entities)if(entity.id==visual.entity){present=true;break;}
        if(present){if(visual.kind==world_runtime_demo::VisualKind::Crate)++rigids;else ++actors;}
    }
    for(const auto &binding:components.physics)for(const auto &node:scene.entities.nodes)if(node.id==binding.entity){
        if(node.binding.kind==world_entity::Kind::SourceModel||node.binding.kind==world_entity::Kind::Placement)++rigids;
        break;
    }
    for(const auto &binding:components.living)for(const auto &node:scene.entities.nodes)if(node.id==binding.entity){
        if(node.binding.kind==world_entity::Kind::Actor)++actors;
        break;
    }
    if(std::getenv("VALWOOD_PRESENTATION"))++actors; // the preview's skeletal captain
    if(rigids>32){error="Live scene requires "+std::to_string(rigids)+" rigid renderers; capacity is 32 including built-in props. Previous live scene preserved.";return false;}
    if(actors>64){error="Live scene requires "+std::to_string(actors)+" actor renderers; capacity is 64 including built-in actors. Previous live scene preserved.";return false;}
    error.clear();return true;
}
// End CPU authoring admission.

struct WorldLivePrograms {
    Program skeletal,skeletal_shadow,rigid,rigid_shadow,crowd,crowd_shadow;
    bool load(){const char *skeletal_vertex=std::getenv("VALWOOD_MODULAR_KNIGHT_SHADER");
        if(!skeletal_vertex||!*skeletal_vertex)skeletal_vertex="shaders/world_skeletal.vs.glsl";
        std::fprintf(stderr,"[Skeletal shader] %s\n",skeletal_vertex);
        return skeletal.load(skeletal_vertex,"shaders/world_foliage.fs.glsl")&&skeletal_shadow.load(skeletal_vertex,"shaders/world_foliage_shadow.fs.glsl")&&rigid.load("shaders/world_dynamic.vs.glsl","shaders/world_foliage.fs.glsl")&&rigid_shadow.load("shaders/world_dynamic.vs.glsl","shaders/world_foliage_shadow.fs.glsl")&&crowd.load("shaders/world_foliage.vs.glsl","shaders/world_foliage.fs.glsl",false,true)&&crowd_shadow.load("shaders/world_foliage.vs.glsl","shaders/world_foliage_shadow.fs.glsl",false,true);}
    void free(){skeletal.free();skeletal_shadow.free();rigid.free();rigid_shadow.free();crowd.free();crowd_shadow.free();}
};
// GL-owner live scene adapter. Included after Program, before the viewer.
// CPU runtime identities are explicit; no array index is treated as a body ID.
struct WorldLiveVisuals {
    using Id=world_runtime::Id;
    struct Actor {Id entity=0;world_runtime_animation::Handle handle;unsigned patrol_leg=0;double action_clock=0;unsigned edited_slot=32;bool can_walk=true;world_motion::State motion={};};
    struct Rigid {Id entity=0;WorldModelAsset *model=nullptr;WorldModelInstance uploaded={};bool have_upload=false,visible=false,reactive=false;world_physics::Quaternion rotation;std::string alias;};
    std::unique_ptr<world_runtime_animation::Manager> animation;
    world_runtime_animation::Gpu gpu;
    std::array<Actor,64> actors={};unsigned actor_count=0;
    std::array<Rigid,32> rigids={};unsigned rigid_count=0;
    world_runtime_animation::Context context;
    world_runtime_animation::Snapshot play_snapshot;
    std::array<Actor,64> play_actors={};bool play_saved=false;
    std::uint64_t frames=0,failures=0,moved=0,draws=0;unsigned visible=0;
    std::uint64_t player_yields=0;mutable std::uint64_t player_blocks=0;
    double minimum_player_separation=std::numeric_limits<double>::infinity();
    std::string error;
    const bool watch_quarter=access("models/watch_quarter/frontages.glb",R_OK)==0;
    bool load_knights(){
        if(animation)return true;
        auto config=world_runtime_animation::knight_config(true);
        if(std::getenv("VALWOOD_PRESENTATION")){config.attention_head="Head";config.attention_neck="neck";config.gesture_arm="RightForeArm";config.gesture_hand="RightHand";config.gesture_chest="Spine02";}
        auto next=world_runtime_animation::Manager::load("models/animations/white_knight_brown_armor_demo.glb",config,64,error);
        if(!next||!world_runtime_animation::create_gpu(*next,gpu,error))return false;
        animation=std::move(next);return true;
    }
    bool add_actor(Id entity,const world_runtime::Entity &pose){
        if(actor_count==actors.size()||!load_knights())return false;
        world_runtime_animation::ActorDesc desc;
        desc.placement={{pose.pose.position.x,pose.pose.position.y,pose.pose.position.z},0,float(pose.scale)};
        desc.gait_phase=std::fmod(double(actor_count)*.137,1.);
        auto handle=animation->spawn(desc,error);if(handle.slot>=64)return false;
        actors[actor_count++]={entity,handle,0,double(actor_count)*.83};return true;
    }
    bool add_edited_actor(Id entity,unsigned edited_slot,bool can_walk){if(actor_count==actors.size()||edited_slot>=32){error="Live actor binding capacity reached";return false;}actors[actor_count++]={entity,{},0,0,edited_slot,can_walk};return true;}
    bool add_rigid(Id entity,const char *path,const char *alias=""){
        if(rigid_count==rigids.size()){error="32 live rigid renderers reached";return false;}
        auto *model=world_model_load(path);if(!model){error=std::string("Cannot load live rigid model ")+path;return false;}
        rigids[rigid_count].entity=entity;rigids[rigid_count].alias=alias?alias:"";rigids[rigid_count++].model=model;return true;
    }
    bool warm(unsigned color,unsigned depth,unsigned rigid_color=0,unsigned rigid_depth=0){
        if(animation&&(!world_runtime_animation::warm_gpu(*animation,gpu,color,error)||!world_runtime_animation::warm_gpu(*animation,gpu,depth,error)))return false;
        for(unsigned i=0;i<rigid_count;i++)if((rigid_color&&!world_model_warm(rigids[i].model,rigid_color))||(rigid_depth&&!world_model_warm(rigids[i].model,rigid_depth))){error="Cannot warm live rigid materials";return false;}
        return true;
    }
    static bool terrain_height(double x,double z,double &height,void *){height=world_atlas_height(x,z);return std::isfinite(height);}
    void checkpoint(){if(animation)animation->checkpoint(play_snapshot);play_actors=actors;play_saved=true;}
    bool restore(){if(!play_saved)return true;if(animation&&!animation->restore(play_snapshot,context,error))return false;actors=play_actors;play_saved=false;return true;}
    bool update(world_runtime::Runtime &runtime,const world_runtime::Frame &frame,WorldScenery *scene,WorldVillagePopulation *population,double dt,double x,double z,bool advance,double player_feet=std::numeric_limits<double>::quiet_NaN()){
        frames++;visible=0;dt=std::clamp(dt,0.,.1);
        context.origin={std::floor(x/128.)*128.,0,std::floor(z/128.)*128.};
        context.terrain=terrain_height;context.terrain_userdata=nullptr;
        // Two generous horizontal volumes cover the nearest castle/camp actors.
        // Actors outside this active area retain their logical schedule/position.
        context.volume_count=1;auto &volume=context.volumes[0];volume.count=4;
        // Graph offsets are capped to .15 world metres independently of actor
        // scale. Widen the host volume instead of changing exact-rig identity
        // or its scale-dependent skin influence bounds.
        constexpr double range=180.+.15;
        volume.planes[0]={{1,0,0},-x+range};volume.planes[1]={{-1,0,0},x+range};
        volume.planes[2]={{0,0,1},-z+range};volume.planes[3]={{0,0,-1},z+range};
        for(unsigned i=0;i<actor_count;i++){
            auto &a=actors[i];world_runtime::Entity entity;if(!runtime.entity(a.entity,entity)){if(a.edited_slot==32)animation->set_enabled(a.handle,false,error);else if(population){WorldVillageActorRuntime hidden;hidden.active=false;world_village_population_actor_runtime(population,a.edited_slot,&hidden);}continue;}
            const bool quarter_watch=watch_quarter&&a.entity==world_runtime_demo::castle_actor_first;
            // The new inn occupies the old east patrol. Keep the same saved
            // identity/memory, moving an old checkpoint's post clear of the inn.
            if(quarter_watch&&entity.pose.position.x>-1294&&entity.pose.position.x<-1274&&entity.pose.position.z>-10299&&entity.pose.position.z<-10275){
                entity.pose.position={-1267.4,24,-10269.5};runtime.set_scene_pose(a.entity,entity.pose);
            }
            world_runtime_animation::ActorState state;
            // The exact owned rig's complete clip sweep fits inside a 10m
            // root envelope (asset contract checked offline). Logical lives
            // keep advancing; distant visual poses need no per-frame sampling.
            const double visual_radius=range+10.*entity.scale;
            const bool visual_active=entity.active&&std::abs(entity.pose.position.x-x)<=visual_radius&&std::abs(entity.pose.position.z-z)<=visual_radius;
            if(a.edited_slot==32&&(!animation->set_enabled(a.handle,visual_active,error)||!animation->state(a.handle,state,error)))return false;
            float speed=0;bool motion_requested=false;const auto old=entity.pose.position;
            if(entity.active&&std::isfinite(player_feet)&&player_feet+1.75>old.y+.05&&player_feet<old.y+1.8*entity.scale){minimum_player_separation=std::min(minimum_player_separation,std::hypot(old.x-x,old.z-z));}
            const auto *conversation=runtime.current_line();
            const bool speaking=std::getenv("VALWOOD_PRESENTATION")&&conversation&&((a.entity==world_runtime_demo::instructor_id&&conversation->id==730011)||(a.entity==world_runtime_demo::jungle_actor_first&&conversation->id==730021));
            if(advance&&!speaking&&entity.active&&std::hypot(old.x-x,old.z-z)<220.)for(unsigned m=0;m<frame.motion_count;m++)if(frame.motion[m].entity==a.entity){
                motion_requested=true;const auto &target=frame.motion[m];auto goal=target.destination;if(quarter_watch)goal={-1267.4,24,-10269.5};
                // Guard posts have explicit two-way patrols, rather than orbiting
                // or moving every civilian forever while at work or asleep.
                if(target.activity==world_living::Activity::Guard)goal.x+=(a.patrol_leg&1)?3.:-3.;
                double dx=goal.x-old.x,dz=goal.z-old.z,distance=std::hypot(dx,dz);
                if(distance<=target.arrival_radius+.01){a.motion.speed=0;if(target.activity==world_living::Activity::Guard)a.patrol_leg^=1;}
                else if(a.can_walk&&target.speed>0&&dt>0){
                    const auto q=entity.pose.rotation;const double old_yaw=std::atan2(2*(q.w*q.y+q.x*q.z),1-2*(q.y*q.y+q.z*q.z));
                    const auto filtered=world_motion::advance(a.motion,old_yaw,std::atan2(dx,dz),distance,target.arrival_radius,target.speed,dt);
                    const double yaw=filtered.yaw;
                    entity.pose.rotation={0,float(std::sin(yaw*.5)),0,float(std::cos(yaw*.5))};runtime.set_scene_pose(a.entity,entity.pose);
                    // Plant, turn, then accelerate into the outgoing path. The
                    // imported walk does not moonwalk through a patrol reversal.
                    if(filtered.distance<=0)break;
                    double step=filtered.distance,nx=old.x+dx/distance*step,nz=old.z+dz/distance*step;
                    const double ny=world_atlas_height(nx,nz);
                    if(std::isfinite(player_feet)&&player_feet+1.75>std::min(old.y,ny)+.05&&player_feet<std::max(old.y,ny)+1.8*entity.scale&&
                       !world_motion::sweep_clear(old.x,old.z,nx,nz,x,z,.37+.30*entity.scale+.15)){a.motion.speed=0;player_yields++;break;}
                    if(std::abs(ny-old.y)<.65&&world_scenery_can_move(scene,old.x,old.z,nx,nz,float(std::min(old.y,ny))+.04f,1.75f,.27f)){
                        world_physics::Hit hit;Id scene_id=0;
                        const bool tested=runtime.cast_capsule({old.x,old.y+.9,old.z},.27f,.60f,{nx-old.x,ny-old.y,nz-old.z},hit,scene_id);
                        if(!tested||!hit.hit||hit.normal.y>.65){entity.pose.position={nx,ny,nz};runtime.set_scene_pose(a.entity,entity.pose);speed=float(step/dt);moved++;}
                    }
                }break;
            }
            if(advance&&!motion_requested){a.motion.speed=std::max(0.,a.motion.speed-2.8*dt);a.motion.turn_rate*=std::exp(-dt/.12);}
            const auto &q=entity.pose.rotation;const float yaw=std::atan2(2*(q.w*q.y+q.x*q.z),1-2*(q.y*q.y+q.z*q.z));
            auto placement=state.placement;placement.position={entity.pose.position.x,entity.pose.position.y,entity.pose.position.z};placement.yaw=yaw;placement.scale=float(entity.scale);
            if(a.edited_slot<32){WorldVillageActorRuntime pose;pose.x=entity.pose.position.x;pose.y=entity.pose.position.y;pose.z=entity.pose.position.z;pose.yaw=yaw;pose.speed=speed;pose.active=entity.active;if(population&&!world_village_population_actor_runtime(population,a.edited_slot,&pose)){error="Authored actor runtime pose rejected";return false;}}
            else{
                if(std::getenv("VALWOOD_PRESENTATION")){
                    const double dx=x-old.x,dz=z-old.z,distance=std::hypot(dx,dz);
                    const bool attentive=distance<6&&speed<.15f;
                    const float desired=float(std::atan2(dx,dz));
                    if(attentive)placement.yaw=state.placement.yaw+float(std::clamp(std::remainder(double(desired-state.placement.yaw),6.28318530718),-1.7*dt,1.7*dt));
                    const float head_yaw=std::clamp(float(std::remainder(double(desired-placement.yaw),6.28318530718)),-.65f,.65f);
                    if(!animation->set_attention(a.handle,head_yaw,0,attentive,speaking,error))return false;
                }
                if(!animation->set_motion(a.handle,placement,speed,error))return false;
            }
        }
        if(animation){if(!animation->update(advance?dt:0,context,error))return false;visible=animation->stats().visible;
            world_animation_render::Bindings bindings;bindings.cull_face=false;
            if(!world_runtime_animation::upload_frame(*animation,gpu,bindings,error))return false;
        }
        for(unsigned i=0;i<rigid_count;i++){
            auto &r=rigids[i];world_runtime::Entity entity;r.visible=runtime.entity(r.entity,entity)&&entity.active&&std::hypot(entity.pose.position.x-x,entity.pose.position.z-z)<180.;if(!r.visible)continue;
            const auto &p=entity.pose.position;WorldModelInstance instance={float(p.x-context.origin.x),float(p.y),float(p.z-context.origin.z),0,float(entity.scale)};
            r.reactive=!r.have_upload||std::memcmp(&instance,&r.uploaded,sizeof instance)||std::memcmp(&r.rotation,&entity.pose.rotation,sizeof r.rotation);
            if(!r.have_upload||std::memcmp(&instance,&r.uploaded,sizeof instance)){if(!world_model_upload_instances(r.model,&instance,1)){error="Live rigid instance upload failed";return false;}r.uploaded=instance;r.have_upload=true;}r.rotation=entity.pose.rotation;
        }return true;
    }
    void base(Program &program,double x,double y,double z){
        program.v3("uOffset",{float(context.origin.x-x),float(-y),float(context.origin.z-z)});program.v2("uWorldAnchor",float(context.origin.x),float(context.origin.z));
        program.i("uVegetation",0);program.f("uWind",0);program.f("uTreeHeight",2);program.i("uLodLevel",-1);program.i("uGrass",0);program.i("uStudioGraphIndex",-1);program.i("uSurfaceBinding",-1);
        program.v3("uPropLodRange",{});program.v2("uSceneryDistanceFade",0,0);program.v3("uStudioLodPreview",{});
        program.v2("uStudioNormalUV",1,1);glUniform4f(program.at("uStudioTintRough"),1,1,1,1);
    }
    bool draw_actors(Program &program,double x,double y,double z,int graph=-1){
        if(!animation||!animation->frame().revision)return true;
        base(program,x,y,z);
        world_animation_render::Bindings bindings;bindings.cull_face=false;bindings.studio_graph_index=graph;
        if(!world_runtime_animation::draw_pass(*animation,gpu,program.id,bindings,error)){failures++;return false;}draws++;return true;
    }
    void draw_rigids(Program &program,double x,double y,double z,const world_studio::Document *document=nullptr){
        base(program,x,y,z);for(unsigned i=0;i<rigid_count;i++){const auto &r=rigids[i];if(!r.visible)continue;program.i("uStudioGraphIndex",-1);program.i("uSurfaceBinding",-1);glUniform4f(program.at("uStudioTintRough"),1,1,1,1);program.v2("uStudioNormalUV",1,1);
            if(document){for(unsigned n=0;n<document->graphs.size();n++)if(document->graphs[n].alias==r.alias)program.i("uStudioGraphIndex",int(n));for(unsigned n=0;n<document->surfaces.bindings.size();n++)if(document->surfaces.bindings[n].alias==r.alias)program.i("uSurfaceBinding",int(n));for(const auto &m:document->materials)if(m.alias==r.alias){glUniform4f(program.at("uStudioTintRough"),m.tint[0],m.tint[1],m.tint[2],m.roughness);program.v2("uStudioNormalUV",m.normal,m.uv);}}
            program.f("uActorReactive",r.reactive?.85f:0.f);glUniform4f(program.at("uRigidQuaternion"),r.rotation.x,r.rotation.y,r.rotation.z,r.rotation.w);world_model_draw(r.model,program.id);draws++;}program.f("uActorReactive",0);program.i("uStudioGraphIndex",-1);program.i("uSurfaceBinding",-1);
    }
    void release(){if(frames)std::printf("[Live contacts] NPC yields %llu, player blocks %llu, minimum player separation %.6fm\n",(unsigned long long)player_yields,(unsigned long long)player_blocks,minimum_player_separation);world_runtime_animation::drop_gpu(gpu);animation.reset();for(unsigned i=0;i<rigid_count;i++)world_model_free(rigids[i].model);actor_count=rigid_count=0;}
};
bool world_live_actor_move_clear(const WorldLiveVisuals *visuals,const world_runtime::Runtime *runtime,
                                double ax,double az,double bx,double bz,float feet,float height,float radius){
    if(!visuals||!runtime||!runtime->configured())return true;
    for(unsigned i=0;i<visuals->actor_count;i++){
        world_runtime::Entity entity;
        if(!runtime->entity(visuals->actors[i].entity,entity)||!entity.active)continue;
        const auto &p=entity.pose.position;
        if(feet+height<=p.y+.05||feet>=p.y+1.8*entity.scale-.05)continue;
        const double combined=radius+.30*entity.scale;
        if(!world_motion::sweep_clear(ax,az,bx,bz,p.x,p.z,combined)){visuals->player_blocks++;return false;}
    }
    return true;
}

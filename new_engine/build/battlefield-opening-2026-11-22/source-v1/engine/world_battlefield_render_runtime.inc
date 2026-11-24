// Included inside viewer main after battlefield state and resident hero options.
// Host owns actors/frame/config/director/scene_frame/scene_seconds/quality/visible/
// enabled. Caller sets the chosen skeletal program's VP and ordinary production
// light/probe/shadow uniforms before battlefield_draw; it restores its program.
bool battlefield_render_ready=false,battlefield_render_uploaded=false;
std::string battlefield_render_error;
world_battlefield_scene::ArrowRenderer battlefield_arrow_renderer;
std::uint64_t battlefield_render_epoch=0,battlefield_render_updates=0,battlefield_render_failures=0;
std::array<const void*,4> battlefield_motion_identity={};
std::array<std::uint64_t,4> battlefield_motion_uploads={};
std::array<world_animation_render::Instance,2> battlefield_motion_roots={};
std::array<bool,4> battlefield_motion_valid={};
world_battlefield_prologue::Pose battlefield_dead_root;
WorldBattlefieldActors::Beat battlefield_actor_beat=WorldBattlefieldActors::Beat::Hidden;
auto battlefield_prepare_render=[&](){
    if(!battlefield_enabled)return true;
    if(!battlefield_render_ready){
        const char *capture=std::getenv("VALWOOD_BATTLEFIELD_CAPTURE");
        const char *rig=std::getenv("VALWOOD_MODULAR_KNIGHT");
        const char *knight=std::getenv("VALWOOD_BATTLEFIELD_KNIGHT_BODY");
        const std::string player=story_civilian_option&&*story_civilian_option?(std::filesystem::path(story_civilian_option).parent_path()/"body.glb").string():std::string{};
        const std::string default_knight=story_armored_option&&*story_armored_option?(std::filesystem::path(story_armored_option).parent_path()/"body.glb").string():std::string{};
        if(!rig||!*rig||!capture||!*capture||player.empty()){
            battlefield_render_error="Battlefield rig, source capture and civilian appearance are required";return false;
        }
        if(!battlefield_actors.load_cpu(rig,capture,player.c_str(),knight&&*knight?knight:default_knight.empty()?nullptr:default_knight.c_str(),battlefield_render_error))return false;
        battlefield_director.prepare();
        char error[256]={};if(!battlefield_arrow_renderer.init(error,sizeof error)){battlefield_render_error=error;return false;}
    }
    if(!battlefield_actors.prepare(live_programs.skeletal.id,live_programs.skeletal_shadow.id,world_program_epoch,battlefield_render_error))return false;
    if(object_motion&&!battlefield_actors.warm_extra(world_object_motion_program(object_motion),battlefield_render_error))return false;
    battlefield_render_epoch=world_program_epoch;battlefield_render_ready=true;return true;
};
auto battlefield_update_visuals=[&](double dt){
    if(!battlefield_enabled)return true;
    if(!battlefield_render_ready||battlefield_render_epoch!=world_program_epoch)if(!battlefield_prepare_render())return false;
    if(!std::isfinite(dt)||dt<0){battlefield_render_error="Invalid battlefield visual delta";return false;}
    // The gameplay runtime owns the unpaused scene clock and retry policy.
    battlefield_scene_frame=battlefield_director.sample(battlefield_scene_seconds,battlefield_quality,cam.x,cam.z,battlefield_visible);
    if(battlefield_decorate_scene)battlefield_decorate_scene(battlefield_scene_frame);
    WorldBattlefieldActors::Input input;input.dt=dt;input.seconds=battlefield_frame.elapsed;
    input.progress=battlefield_frame.pose_progress;input.struggle=battlefield_frame.struggle_progress;
    const auto stage=battlefield_frame.stage;using Stage=world_battlefield_prologue::Stage;using Beat=WorldBattlefieldActors::Beat;
    auto anchor=battlefield_config.ambush;
    battlefield_actor_beat=Beat::Hidden;
    switch(stage){
    case Stage::Dazed:anchor=battlefield_config.arrival;battlefield_actor_beat=Beat::Dazed;break;
    case Stage::Recover:anchor=battlefield_config.arrival;battlefield_actor_beat=Beat::Rise;break;
    case Stage::Duck:battlefield_actor_beat=Beat::Ambush;break;
    case Stage::Tackle:battlefield_actor_beat=Beat::Tackle;break;
    case Stage::Struggle:battlefield_actor_beat=Beat::Struggle;break;
    case Stage::Stab:battlefield_actor_beat=Beat::Stab;break;
    case Stage::Stand:battlefield_actor_beat=Beat::Recover;break;
    case Stage::Escape:case Stage::Forest:case Stage::Campsite:if(battlefield_frame.sword_owned)battlefield_actor_beat=Beat::Aftermath;break;
    case Stage::Dead:
        if(battlefield_frame.events&world_battlefield_prologue::Died)battlefield_dead_root={{cam.x,cam.y-1.75,cam.z},3.14159265359-cam.yaw};
        anchor=battlefield_dead_root;battlefield_actor_beat=Beat::Death;break;
    default:break;
    }
    input.beat=battlefield_visible?battlefield_actor_beat:Beat::Hidden;
    input.x=anchor.feet.x;input.y=anchor.feet.y;input.z=anchor.feet.z;input.yaw=float(anchor.yaw);
    input.reset=(battlefield_frame.events&(world_battlefield_prologue::Started|world_battlefield_prologue::Retried|world_battlefield_prologue::Restored))!=0;
    if(!battlefield_actors.update_cpu(input,battlefield_render_error)||!battlefield_actors.update_background(battlefield_scene_frame.actors.data(),battlefield_scene_frame.actor_count,battlefield_render_error))return false;
    // Native combat has updated first. Republish its retained slot171 corpses
    // after any combat-clock reset, before the color/reflection/shadow passes.
    // Replacing them with a different skeletal death clip would visibly pop.
    if(battlefield_publish_corpses&&!battlefield_publish_corpses()){
        battlefield_render_error="Retained battlefield corpse publication failed";return false;
    }
    world_animation_render::Bindings bindings;bindings.cull_face=false;
    if(!battlefield_actors.upload(bindings,battlefield_render_error)||!battlefield_arrow_renderer.update(battlefield_scene_frame))return false;
    battlefield_render_uploaded=true;++battlefield_render_updates;return true;
};
// Motion mode reuses the host's object-motion VP/camera-delta state and draws
// previous palettes only after one matching update on the same renderer.
auto battlefield_draw=[&](Program &program,const Camera &eye,bool shadow=false,bool motion=false,bool reset=false){
    if(!battlefield_render_ready||!battlefield_render_uploaded||!battlefield_visible)return true;
    if(motion&&!battlefield_actors.warm_extra(program.id,battlefield_render_error))return false;
    program.use();program.i("uStudioGraphIndex",-1);program.i("uSurfaceBinding",-1);
    glUniform4f(program.at("uStudioTintRough"),1,1,1,1);program.v2("uStudioNormalUV",1,1);
    world_animation_render::Bindings bindings;bindings.cull_face=false;
    auto prior=[&](unsigned index,const void *identity,std::uint64_t uploads,unsigned palette,int offset,const world_animation_render::Instance *root){
        if(!motion)return;
        const bool previous=!reset&&battlefield_motion_valid[index]&&identity==battlefield_motion_identity[index]&&uploads==battlefield_motion_uploads[index]+1&&palette;
        glActiveTexture(GL_TEXTURE18);glBindTexture(GL_TEXTURE_BUFFER,previous?palette:0);glBindSampler(18,0);glActiveTexture(GL_TEXTURE0);
        program.i("uPrevSkinPalette",18);program.i("uHasPrevPalette",previous?1:0);program.i("uPrevPaletteOffset",previous?offset:0);
        if(root&&battlefield_motion_valid[index]){
            const auto&p=battlefield_motion_roots[index];program.v3("uRootDelta",{root->x-p.x,root->y-p.y,root->z-p.z});
            program.f("uRootYawDelta",std::remainder(root->yaw-p.yaw,6.28318530718f));
        }else{program.v3("uRootDelta",{});program.f("uRootYawDelta",0);}
        battlefield_motion_identity[index]=identity;battlefield_motion_uploads[index]=uploads;battlefield_motion_valid[index]=!reset;
        if(root)battlefield_motion_roots[index]=*root;
    };
    bool ok=true;for(unsigned n=0;n<2;++n){
        const auto role=WorldBattlefieldActors::Role(n);const auto&instance=battlefield_actors.instance(role);
        const void *identity=nullptr;std::uint64_t uploads=0;int offset=0;
        const unsigned palette=motion?battlefield_actors.previous_palette(role,identity,uploads,&offset):0;
        prior(n,identity,uploads,palette,offset,&instance);
        if(reset)continue;
        program.v3("uOffset",{float(instance.x-eye.x),float(-eye.y),float(instance.z-eye.z)});program.v2("uWorldAnchor",instance.x,instance.z);
        ok=battlefield_actors.draw(role,program.id,bindings,battlefield_render_error)&&ok;
    }
    const void *identity=nullptr;std::uint64_t uploads=0;int offset=0;
    const unsigned palette=motion?battlefield_actors.background_previous_palette(identity,uploads,&offset):0;
    prior(2,identity,uploads,palette,offset,nullptr);
    const auto &extra_origin=battlefield_actors.background_origin();
    if(!reset){program.v3("uOffset",{float(extra_origin[0]-eye.x),float(extra_origin[1]-eye.y),float(extra_origin[2]-eye.z)});program.v2("uWorldAnchor",extra_origin[0],extra_origin[2]);ok=battlefield_actors.draw_background(program.id,bindings,battlefield_render_error)&&ok;}
    identity=nullptr;uploads=0;offset=0;
    const unsigned archer_palette=motion?battlefield_actors.archer_previous_palette(identity,uploads,&offset):0;
    prior(3,identity,uploads,archer_palette,offset,nullptr);
    if(!reset){program.v3("uOffset",{float(extra_origin[0]-eye.x),float(extra_origin[1]-eye.y),float(extra_origin[2]-eye.z)});program.v2("uWorldAnchor",extra_origin[0],extra_origin[2]);ok=battlefield_actors.draw_archers(program.id,bindings,battlefield_render_error)&&ok;}
    if(motion){glActiveTexture(GL_TEXTURE18);glBindTexture(GL_TEXTURE_BUFFER,0);glActiveTexture(GL_TEXTURE0);program.i("uHasPrevPalette",0);program.i("uPrevPaletteOffset",0);}
    if(!ok)++battlefield_render_failures;(void)shadow;return ok;
};
auto battlefield_draw_arrows=[&](Program &program,const Camera &eye,bool shadow=false){
    if(battlefield_render_ready&&battlefield_render_uploaded&&battlefield_visible)battlefield_arrow_renderer.draw(program.id,eye.x,eye.y,eye.z,shadow);
};
auto battlefield_release_render=[&](){battlefield_arrow_renderer.release();battlefield_actors.release();battlefield_render_ready=battlefield_render_uploaded=false;};

#ifndef VALWOOD_WORLD_JOUST_RENDER_H
#define VALWOOD_WORLD_JOUST_RENDER_H
#include "engine/world_models.h"
#include "engine/world_joust.h"
#include "engine/world_joust_arm_private.h"
#include "engine/world_joust_rider_private.h"
#include "engine/world_joust_fall_private.h"
#include "engine/world_joust_crowd_private.h"
#include <SDL2/SDL_opengl.h>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

// GL-owner resources. Pose models and their texture cache references are loaded
// once; simulation and draw frames never read files or recook geometry.
class WorldJoustRender {
    struct Pose {WorldModelAsset *asset=nullptr;world_joust::Vec3 hand;std::vector<WorldModelVertexOverride> body;world_joust_arm_private::Joints joints;};
    struct Actor {WorldModelPoseBlend *blend=nullptr;WorldModelVertexStream *stream=nullptr;WorldModelAsset *a=nullptr,*b=nullptr;std::vector<WorldModelVertexOverride> vertices;const Pose *fall_a=nullptr,*fall_b=nullptr;world_joust_fall_private::Bank::Frame fall_frame;float fall_w=0,floor_lift=0;bool unhorsed=false;std::vector<world_joust_fall_private::PNT> impact;world_joust::Vec3 impact_palm,gear_lance,gear_shield,gear_direction,gear_velocity;};
    std::vector<Pose> charge_,idle_;
    std::array<Actor,2> actors_={};
    WorldJoustCrowd crowd_;
    world_joust_fall_private::Bank fall_bank_;
    unsigned long long articulated_updates_=0,settled_updates_=0,released_gear_draws_=0;
    std::array<float,4> max_limb_excursion_={};
    float min_gear_y_=1.e9f,max_fall_secondary_=0,max_fall_phase_span_=0,max_fall_segment_error_=0;unsigned long long varied_fall_updates_=0;
    WorldModelAsset *arena_=nullptr,*lance_=nullptr,*shield_=nullptr;
    WorldModelInstanceBatch *lances_=nullptr,*shields_=nullptr;
    float stride_=3.18f,idle_cycle_=2.f,charge_cycle_=.8f,lance_length_=4.8f;
    unsigned long long frames_=0,draws_=0,shadow_draws_=0,failures_=0;
    unsigned long long arm_updates_=0,arm_failures_=0;
    float max_grip_error_=0,max_hand_surface_error_=0,max_hand_surface_drift_=0,max_floor_correction_=0,min_fallen_y_=1.e9f;
    unsigned unhorsed_=0,grounded_=0;unsigned long long reaction_updates_=0,unhorsed_draws_=0,riderless_draws_=0;
    static world_joust::Vec3 vector3(const float *p){return {p[0],p[1],p[2]};}
    static void store3(float *p,world_joust::Vec3 v){p[0]=v.x;p[1]=v.y;p[2]=v.z;}
    bool deform_arm(Actor &actor,const Pose &pose_a,const Pose &pose_b,float fraction,const world_joust::Rider &r){
        namespace arm=world_joust_arm_private;namespace body=world_joust_rider_private;using V=world_joust::Vec3;
        const bool entering=r.unhorsed&&!actor.unhorsed;
        if(entering){actor.fall_a=&pose_a;actor.fall_b=&pose_b;actor.fall_w=fraction;actor.gear_lance=r.lance_start;actor.gear_shield=r.shield_center;actor.gear_direction=world_joust::normalized(r.lance_tip-r.lance_start);actor.gear_velocity={r.fall_velocity.x*.55f,1.1f,r.fall_velocity.z};}
        actor.unhorsed=r.unhorsed;const auto &a=actor.unhorsed?*actor.fall_a:pose_a,&b=actor.unhorsed?*actor.fall_b:pose_b;const float w=actor.unhorsed?actor.fall_w:fraction;
        const auto joints=arm::blend(a.joints,b.joints,w);
        // Physical source palm already includes the shared torso lunge and
        // recoil/fall transform. Only the remaining arm-relative reach is IK.
        const V palm_world=world_joust::rider_point(r,joints.palm);
        const V local_delta=body::inverse_rotation(r,r.lance_grip-palm_world);
        arm::Solution solution;if(!r.unhorsed||entering){if(!arm::solve(joints,joints.palm+local_delta,solution)){++arm_failures_;return false;}}
        const body::ModelTransform transform(r);
        float nearest=1.e9f,source_nearest=1.e9f,minimum=1.e9f;
        const V body_local=body::inverse_rotation(r,r.body_offset);
        if(entering)actor.impact_palm=solution.target.palm+body_local;
        if(r.unhorsed){fall_bank_.prepare(actor.fall_frame,r);for(unsigned limb=0;limb<4;++limb)max_limb_excursion_[limb]=std::max(max_limb_excursion_[limb],actor.fall_frame.limb_excursion[limb]);max_fall_secondary_=std::max(max_fall_secondary_,actor.fall_frame.max_secondary);max_fall_phase_span_=std::max(max_fall_phase_span_,actor.fall_frame.phase_span);max_fall_segment_error_=std::max(max_fall_segment_error_,actor.fall_frame.max_segment_error);if(actor.fall_frame.max_secondary>.001f)++varied_fall_updates_;}
        const V falling_palm=r.unhorsed?world_joust_fall_private::stage_point(r,actor.impact_palm,world_joust_fall_private::right_palms):V{};
        for(std::size_t i=0;i<body::vertices.size();++i){
            const auto &region=body::vertices[i];const auto &weight=region.arm;const auto &pa=a.body[i],&pb=b.body[i];auto &out=actor.vertices[region.index];
            const V p=arm::lerp(vector3(pa.position),vector3(pb.position),w);
            const V n=arm::unit(arm::lerp(vector3(pa.normal),vector3(pb.normal),w),vector3(pa.normal));
            V tangent=arm::lerp(vector3(pa.tangent),vector3(pb.tangent),w);tangent=arm::unit(tangent-n*world_joust::dot(tangent,n),vector3(pa.tangent));
            if(r.unhorsed){
                if(entering){V dn=arm::unit(solution.direction(n,weight),n),dt=solution.direction(tangent,weight);dt=arm::unit(dt-dn*world_joust::dot(dt,dn),tangent);actor.impact[i]={solution.point(p,weight)+body_local*region.torso,dn,dt};}
                const auto q=fall_bank_.sample(i,actor.impact[i],r,actor.fall_frame);
                const V model=transform.point(q.p,0);minimum=std::min(minimum,model.y+r.horse.position.y);
                store3(out.position,model);out.weight=1;store3(out.normal,transform.vector(q.n));store3(out.tangent,transform.vector(q.t));
            }else{
                const V deformed=solution.point(p,weight),normal=arm::unit(solution.direction(n,weight),n);
                V deformed_tangent=solution.direction(tangent,weight);deformed_tangent=arm::unit(deformed_tangent-normal*world_joust::dot(deformed_tangent,normal),tangent);
                const V model=transform.point(deformed,region.torso);minimum=std::min(minimum,model.y+r.horse.position.y);
                store3(out.position,model);out.weight=1;store3(out.normal,transform.vector(normal));store3(out.tangent,transform.vector(deformed_tangent));
                if(weight.hand>.9999f){nearest=std::min(nearest,world_joust::length(deformed-solution.target.palm));source_nearest=std::min(source_nearest,world_joust::length(p-joints.palm));}
            }
        }
        actor.floor_lift=0;
        if(r.unhorsed){
            // Actual transformed source vertices establish support, including
            // curled knees, boots, plume and the frozen source torso lunge.
            actor.floor_lift=r.fall_grounded?.02f-minimum:std::max(0.f,.02f-minimum);
            for(const auto &region:body::vertices)actor.vertices[region.index].position[1]+=actor.floor_lift;
            max_floor_correction_=std::max(max_floor_correction_,std::abs(actor.floor_lift));min_fallen_y_=std::min(min_fallen_y_,minimum+actor.floor_lift);
        }
        if(r.unhorsed){++articulated_updates_;if(r.fall_settled)++settled_updates_;(void)falling_palm;}
        else {
        max_grip_error_=std::max(max_grip_error_,solution.grip_error);
        max_hand_surface_error_=std::max(max_hand_surface_error_,nearest);
        max_hand_surface_drift_=std::max(max_hand_surface_drift_,std::abs(nearest-source_nearest));
        if(!std::isfinite(nearest)||nearest>.02f||std::abs(nearest-source_nearest)>.002f||solution.grip_error>.002f){++arm_failures_;return false;}
        }
        if(!world_model_vertex_stream_upload(actor.stream,actor.vertices.data(),actor.vertices.size())){++arm_failures_;return false;}
        if(r.reaction_amount>.001f)++reaction_updates_;
        ++arm_updates_;return true;
    }
    static void integer(unsigned p,const char *n,int v){glUniform1i(glGetUniformLocation(p,n),v);}
    static void scalar(unsigned p,const char *n,float v){glUniform1f(glGetUniformLocation(p,n),v);}
    static bool safe_path(const std::string &s){const std::filesystem::path p(s);if(p.empty()||p.is_absolute())return false;for(const auto &part:p)if(part=="..")return false;return true;}
public:
    bool ready=false;
    world_joust::Vec3 grip={-.35f,2.10f,.33f};
    bool load(const char *directory,std::string &error){
        release();const std::filesystem::path root(directory);std::ifstream in(root/"mount.cfg");std::string word;unsigned version=0;
        if(!(in>>word>>version)||word!="VALWOOD_JOUST_MOUNT"||version!=1){error="Missing or invalid mounted pose manifest";return false;}
        unsigned declared_charge=0,declared_idle=0;std::string lance_path,shield_path;
        while(in>>word){
            if(word=="charge_count")in>>declared_charge;
            else if(word=="idle_count")in>>declared_idle;
            else if(word=="charge_cycle")in>>charge_cycle_;
            else if(word=="charge_stride")in>>stride_;
            else if(word=="idle_cycle")in>>idle_cycle_;
            else if(word=="lance_length")in>>lance_length_;
            else if(word=="lance")in>>std::quoted(lance_path);
            else if(word=="shield")in>>std::quoted(shield_path);
            else if(word=="lance_tip"||word=="lance_grip"){
                world_joust::Vec3 point;in>>point.x>>point.y>>point.z;
                const auto expected=word=="lance_tip"?world_joust::Vec3{0,0,world_joust::Game::lance_length()}:world_joust::Vec3{};
                if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z)||world_joust::length(point-expected)>.001f){error="Lance source endpoints differ from gameplay";release();return false;}
            }
            else if(word=="charge"||word=="idle"){
                const bool moving=word=="charge";std::string path,hand;Pose p;
                if(!(in>>std::quoted(path)>>hand>>p.hand.x>>p.hand.y>>p.hand.z)||hand!="right_hand"||!safe_path(path)){error="Invalid owned riding pose record";release();return false;}
                if((moving?charge_.size():idle_.size())>=24){error="Mounted pose capacity exceeded";release();return false;}
                p.asset=world_model_load_pose_support_pnt((root/path).c_str(),WORLD_MODEL_POSE_BLEND_VERSION,true);
                if(!p.asset){error="Could not load mounted pose: "+path;release();return false;}
                (moving?charge_:idle_).push_back(p);
            }else if(word=="end")break;
            else {error="Unknown mounted asset manifest field: "+word;release();return false;}
            if(!in){error="Truncated mounted asset manifest";release();return false;}
        }
        if(declared_charge!=charge_.size()||declared_idle!=idle_.size()||charge_.size()<8||idle_.empty()||!std::isfinite(stride_)||stride_<1||stride_>12||!std::isfinite(idle_cycle_)||idle_cycle_<.1||idle_cycle_>20||!std::isfinite(lance_length_)||std::abs(lance_length_-world_joust::Game::lance_length())>.001f||!safe_path(lance_path)||!safe_path(shield_path)){
            error="Mounted asset dimensions, cycle or pose count differs from joust rules";release();return false;
        }
        arena_=world_model_load((root/"arena.glb").c_str());
        lance_=world_model_load_pose_origin((root/lance_path).c_str(),WORLD_MODEL_POSE_BLEND_VERSION);
        if(arena_) {WorldModelInstance a{0,0,0,0,1};if(!world_model_upload_instances(arena_,&a,1)){error="Joust venue upload failed";release();return false;}}
        if(lance_)lances_=world_model_instance_batch_create(lance_,2);
        shield_=world_model_load_pose_origin((root/shield_path).c_str(),WORLD_MODEL_POSE_BLEND_VERSION);
        if(shield_)shields_=world_model_instance_batch_create(shield_,2);
        for(auto &a:actors_)a.blend=world_model_pose_blend_create(1,WORLD_MODEL_POSE_BLEND_VERSION);
        if(!arena_||!lance_||!lances_||!shield_||!shields_||!actors_[0].blend||!actors_[1].blend){error="Joust model GPU resources unavailable";release();return false;}
        // Validate every adjacent pair once, before any game inputs are enabled.
        for(const auto *bank:{&charge_,&idle_})for(std::size_t i=0;i<bank->size();++i)
            if(!world_model_pose_blend_set_targets(actors_[0].blend,(*bank)[i].asset,(*bank)[(i+1)%bank->size()].asset)){
                error="Mounted pose topology is inconsistent";release();return false;
            }
        // Cache the small genuine right-arm subset once. No full-mesh CPU
        // sampling, allocation, asset reads or recooking happens in update().
        if(charge_.size()!=16||idle_.size()!=4){error="Mounted arm source bank differs from its proven 20-pose mapping";release();return false;}
        std::vector<WorldModelVertexOverride> source_vertices(world_joust_arm_private::encoded_vertices);unsigned pose_index=0;
        for(auto *bank:{&charge_,&idle_})for(auto &p:*bank){
            if(world_model_pose_vertices(p.asset)!=source_vertices.size()||!world_model_pose_blend_set_targets(actors_[0].blend,p.asset,p.asset)||!world_model_pose_blend_sample_pnt(actors_[0].blend,0,source_vertices.data(),source_vertices.size())){error="Mounted arm source PNT is unavailable";release();return false;}
            p.joints=world_joust_arm_private::source_poses[pose_index++];
            if(world_joust::length(p.hand-p.joints.palm)>.001f){error="Mounted arm source palm differs from pose manifest";release();return false;}
            p.body.reserve(world_joust_rider_private::vertices.size());for(const auto &region:world_joust_rider_private::vertices)p.body.push_back(source_vertices[region.index]);
        }
        for(auto &actor:actors_){
            actor.stream=world_model_vertex_stream_create(charge_[0].asset);actor.vertices.resize(world_joust_arm_private::encoded_vertices);actor.impact.resize(world_joust_rider_private::vertices.size());
            if(!actor.stream){error="Mounted arm stream unavailable";release();return false;}
        }
        grip=charge_[0].hand;
        for(const auto *bank:{&charge_,&idle_})for(const auto &p:*bank){
            if(world_joust::length(p.hand-grip)>.03f){error="Mounted right-hand grip drifts beyond the contact tolerance";release();return false;}
        }
        if(!fall_bank_.load(root.parent_path()/"joust-fall-assets")){error="Source articulated fall bank missing or invalid";release();return false;}
        if(!crowd_.load(root.parent_path()/"joust-stadium-assets",error)){release();return false;}
        ready=true;std::printf("[Joust render] Loaded %zu charge + %zu idle poses, grip %.4f %.4f %.4f, lance %.2fm\n",charge_.size(),idle_.size(),grip.x,grip.y,grip.z,lance_length_);return true;
    }
    bool update(const world_joust::Game &g,float idle_clock){
        if(!ready)return false;
        if(!crowd_.update(idle_clock,g.contact_serial)){++failures_;return false;}
        unhorsed_=grounded_=0;
        const world_joust::Rider *riders[]={&g.player,&g.opponent};std::array<WorldModelInstance,2> lance_instances={},shield_instances={};
        for(unsigned j=0;j<2;++j){
            const auto &r=*riders[j];unhorsed_+=r.unhorsed;grounded_+=r.unhorsed&&r.fall_grounded;const bool moving=r.horse.speed>.25f;const auto &bank=moving?charge_:idle_;
            // The core gait accumulates distance * .85; restore travelled metres.
            const float cycles=moving?r.horse.gait/(.85f*stride_):idle_clock/idle_cycle_;
            const float frame=(cycles-std::floor(cycles))*float(bank.size());const unsigned a=std::min(unsigned(frame),unsigned(bank.size()-1)),b=(a+1)%unsigned(bank.size());const float w=frame-float(a);
            auto &animated=actors_[j];
            if(animated.a!=bank[a].asset||animated.b!=bank[b].asset){
                if(!world_model_pose_blend_set_targets(animated.blend,bank[a].asset,bank[b].asset)||!world_model_pose_blend_attach_vertex_stream(animated.blend,animated.stream)){++failures_;return false;}
                animated.a=bank[a].asset;animated.b=bank[b].asset;
            }
            if(!deform_arm(animated,bank[a],bank[b],w,r)){++failures_;return false;}
            WorldModelInstance actor{r.horse.position.x,r.horse.position.y,r.horse.position.z,r.horse.yaw,1};
            if(!world_model_pose_blend_upload(actors_[j].blend,&actor,&w,1)){++failures_;return false;}
            auto direction=world_joust::normalized(r.lance_tip-r.lance_start);
            const float lift=animated.floor_lift;
            const auto shield_rotation=world_joust_rider_private::instance_euler(r);
            shield_instances[j]={r.shield_center.x,r.shield_center.y+lift,r.shield_center.z,shield_rotation.yaw,1,shield_rotation.pitch,shield_rotation.roll};
            const bool broken=j==0?g.player_broken:g.opponent_broken;
            const float length_scale=broken?.31f:1.f;
            if(r.unhorsed){
                // Equipment releases at the actual impact sockets. Bounded
                // ballistic props settle independently of the articulating arms.
                const float t=r.fall_age,travel=t<.7f?t:.7f+.35f*(1.f-std::exp(-(t-.7f)/.35f));
                auto released=[&](world_joust::Vec3 p){return world_joust::Vec3{std::clamp(p.x+animated.gear_velocity.x*travel,-9.f,9.f),p.y+1.1f*t-4.905f*t*t,std::clamp(p.z+animated.gear_velocity.z*travel,-58.f,58.f)};};
                direction=world_joust::normalized(animated.gear_direction+world_joust::Vec3{0,-animated.gear_direction.y*world_joust::smooth_unit(t/.70f),0});
                auto lp=released(animated.gear_lance);const float support=length_scale*(.135f+std::max(-direction.y*4.8f,direction.y*1.167151f));lp.y=std::max(lp.y,.02f+support);
                lance_instances[j]={lp.x,lp.y,lp.z,std::atan2(direction.x,direction.z),length_scale,-std::asin(std::clamp(direction.y,-1.f,1.f))};
                auto sp=released(animated.gear_shield);const float pitch=world_joust::pi*.5f*world_joust::smooth_unit(t/.65f);const float shield_support=std::abs(std::cos(pitch))*.48f+std::abs(std::sin(pitch))*.07182f;sp.y=std::max(sp.y,.02f+shield_support);
                shield_instances[j]={sp.x,sp.y,sp.z,r.fall_yaw,1,pitch,0};min_gear_y_=std::min(min_gear_y_,std::min(lp.y-support,sp.y-shield_support));
            }else lance_instances[j]={r.lance_start.x,r.lance_start.y+lift,r.lance_start.z,std::atan2(direction.x,direction.z),length_scale,-std::asin(std::clamp(direction.y,-1.f,1.f))};
        }
        if(!world_model_instance_batch_upload(shields_,shield_instances.data(),2)||!world_model_instance_batch_upload(lances_,lance_instances.data(),2)){++failures_;return false;}
        ++frames_;return true;
    }
    void draw(unsigned program,double eye_x,double eye_y,double eye_z,bool shadow,bool show_player=true){
        if(!ready||!program)return;
        glUseProgram(program);glUniform3f(glGetUniformLocation(program,"uOffset"),float(-1800-eye_x),float(64.08-eye_y),float(-10500-eye_z));
        glUniform2f(glGetUniformLocation(program,"uWorldAnchor"),-1800,-10500);
        for(const char *name:{"uGrass","uVegetation","uInstanceFade","uPoseBlendEnabled","uCharacterEnabled","uBakedLightEnabled","uSourcePaintEnabled","uFullTransform","uFacadeMode","uRootedGrassWind"})integer(program,name,0);
        integer(program,"uLodLevel",-1);integer(program,"uStudioGraphIndex",-1);integer(program,"uSurfaceBinding",-1);scalar(program,"uWind",0);scalar(program,"uActorReactive",shadow?0:.85f);scalar(program,"uTreeHeight",4);
        glUniform3f(glGetUniformLocation(program,"uPropLodRange"),0,0,0);glUniform4f(glGetUniformLocation(program,"uStudioTintRough"),1,1,1,1);glUniform2f(glGetUniformLocation(program,"uStudioNormalUV"),1,1);
        scalar(program,"uActorReactive",0);world_model_draw(arena_,program);scalar(program,"uActorReactive",shadow?0:.85f);for(unsigned i=0;i<2;++i)if(i||shadow||show_player){world_model_pose_blend_draw_equipped_deformed(actors_[i].blend,program,0,1);if(actors_[i].unhorsed){++unhorsed_draws_;++riderless_draws_;released_gear_draws_+=2;}}crowd_.draw(program);world_model_instance_batch_draw(lances_,program);world_model_instance_batch_draw(shields_,program);
        scalar(program,"uActorReactive",0);if(shadow)++shadow_draws_;else ++draws_;
    }
    void release(){
        ready=false;crowd_.release();fall_bank_.clear();for(auto &a:actors_){world_model_pose_blend_free(a.blend);world_model_vertex_stream_free(a.stream);a={};}world_model_instance_batch_free(lances_);lances_=nullptr;
        world_model_instance_batch_free(shields_);shields_=nullptr;world_model_free(shield_);shield_=nullptr;
        world_model_free(lance_);lance_=nullptr;world_model_free(arena_);arena_=nullptr;
        for(auto *bank:{&charge_,&idle_}){for(auto &p:*bank)world_model_free(p.asset);bank->clear();}
    }
    unsigned long long frames()const{return frames_;}unsigned long long draws()const{return draws_;}unsigned long long shadow_draws()const{return shadow_draws_;}unsigned long long failures()const{return failures_;}
    unsigned long long arm_updates()const{return arm_updates_;}unsigned long long arm_failures()const{return arm_failures_;}
    unsigned spectator_count()const{return crowd_.spectators();}unsigned long long crowd_draws()const{return crowd_.draws();}unsigned long long crowd_uploads()const{return crowd_.uploads();}float crowd_swell()const{return crowd_.swell();}
    unsigned rendered_unhorsed_riders()const{return unhorsed_;}unsigned rendered_grounded_riders()const{return grounded_;}
    unsigned long long riderless_horse_draws()const{return riderless_draws_;}unsigned long long unhorsed_rider_draws()const{return unhorsed_draws_;}unsigned long long reaction_updates()const{return reaction_updates_;}
    float max_rider_floor_correction()const{return max_floor_correction_;}float min_fallen_vertex_y()const{return min_fallen_y_;}float rider_floor_lift(unsigned rider)const{return rider<2?actors_[rider].floor_lift:0;}
    unsigned long long articulated_fall_updates()const{return articulated_updates_;}unsigned long long settled_fall_updates()const{return settled_updates_;}
    const std::array<float,4>& max_limb_excursion()const{return max_limb_excursion_;}
    float max_fall_segment_length_error()const{return max_fall_segment_error_;}
    unsigned fall_joint_count()const{return fall_bank_.joint_count();}unsigned long long varied_fall_updates()const{return varied_fall_updates_;}float max_fall_secondary_angle()const{return max_fall_secondary_;}float max_fall_release_phase_span()const{return max_fall_phase_span_;}
    unsigned long long released_gear_draws()const{return released_gear_draws_;}float min_released_gear_y()const{return min_gear_y_;}
    unsigned crowd_family_count()const{return crowd_.family_count();}unsigned crowd_hair_family_count()const{return crowd_.hair_family_count();}unsigned crowd_style_count()const{return crowd_.style_count();}
    unsigned seated_spectator_count()const{return crowd_.seated_count();}unsigned animated_spectator_count()const{return crowd_.animated_spectators();}unsigned crowd_pose_pairs()const{return crowd_.fixed_pose_pairs();}
    float max_grip_error()const{return max_grip_error_;}float max_hand_surface_error()const{return max_hand_surface_error_;}float max_hand_surface_drift()const{return max_hand_surface_drift_;}
};
#endif

/* CPU integration: real encoded bounds, atlas, scene grounding/colliders and
 * bandit runtime. Rendering/model uploads are stubbed; no GL context or GPU. */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include "engine/world_models.h"
#include "engine/world_scenery.h"
#include "engine/world_atlas.h"
#include "engine/world_bandit_camp.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <iterator>
#include <new>
#include <unistd.h>
#include "engine/world_bandit_route.inc"

namespace {std::size_t active_models=0,active_batches=0,allocations=0;bool track=false;}
void *operator new(std::size_t bytes){if(track)allocations++;if(void *p=std::malloc(bytes?bytes:1))return p;throw std::bad_alloc();}
void operator delete(void *p)noexcept{std::free(p);}
void operator delete(void *p,std::size_t)noexcept{std::free(p);}
struct WorldModelAsset {float low[3],high[3];std::size_t count=0;};
struct WorldModelPoseBlend {std::size_t capacity=0;};

extern "C" WorldModelAsset *world_model_load(const char *path){
    cgltf_options options{};cgltf_data *data=nullptr;
    assert(cgltf_parse_file(&options,path,&data)==cgltf_result_success);
    assert(cgltf_load_buffers(&options,data,path)==cgltf_result_success);
    assert(cgltf_validate(data)==cgltf_result_success);
    auto *out=new WorldModelAsset;
    for(int k=0;k<3;k++){out->low[k]=std::numeric_limits<float>::max();out->high[k]=-out->low[k];}
    const cgltf_scene *scene=data->scene?data->scene:&data->scenes[0];
    for(std::size_t ni=0;ni<data->nodes_count;ni++){
        const auto *node=&data->nodes[ni];if(!node->mesh)continue;
        const auto *root=node;while(root->parent)root=root->parent;
        if(std::find(scene->nodes,scene->nodes+scene->nodes_count,root)==scene->nodes+scene->nodes_count)continue;
        float matrix[16];cgltf_node_transform_world(node,matrix);
        for(std::size_t pi=0;pi<node->mesh->primitives_count;pi++){
            const auto &p=node->mesh->primitives[pi];const cgltf_accessor *position=nullptr;
            for(std::size_t ai=0;ai<p.attributes_count;ai++)if(p.attributes[ai].type==cgltf_attribute_type_position)position=p.attributes[ai].data;
            assert(position);
            for(std::size_t vi=0;vi<position->count;vi++){
                float source[3];assert(cgltf_accessor_read_float(position,vi,source,3));
                for(int k=0;k<3;k++){
                    const float value=matrix[k]*source[0]+matrix[4+k]*source[1]+matrix[8+k]*source[2]+matrix[12+k];
                    assert(std::isfinite(value));out->low[k]=std::min(out->low[k],value);out->high[k]=std::max(out->high[k],value);
                }
            }
        }
    }
    assert(out->low[1]<=out->high[1]);out->high[1]-=out->low[1];out->low[1]=0;
    cgltf_free(data);active_models++;return out;
}
extern "C" WorldModelAsset *world_model_load_pose(const char *path,unsigned version){assert(version==WORLD_MODEL_POSE_BLEND_VERSION);return world_model_load(path);}
extern "C" bool world_model_bounds(const WorldModelAsset *a,float *low,float *high){if(!a)return false;std::copy(a->low,a->low+3,low);std::copy(a->high,a->high+3,high);return true;}
extern "C" void world_model_free(WorldModelAsset *a){if(!a)return;assert(active_models);active_models--;delete a;}
extern "C" bool world_model_upload_instances(WorldModelAsset *a,const WorldModelInstance *,std::size_t n){assert(a&&n<=4096);a->count=n;return true;}
extern "C" bool world_model_upload_fading_instances(WorldModelAsset *a,const WorldModelInstance *v,const float *,std::size_t n){return world_model_upload_instances(a,v,n);}
extern "C" void world_model_draw_instances(WorldModelAsset *,unsigned,std::size_t){assert(false&&"CPU fixture must not draw");}
extern "C" WorldModelPoseBlend *world_model_pose_blend_create(std::size_t capacity,unsigned version){assert(version==WORLD_MODEL_POSE_BLEND_VERSION);active_batches++;auto *b=new WorldModelPoseBlend;b->capacity=capacity;return b;}
extern "C" bool world_model_pose_blend_set_targets(WorldModelPoseBlend *b,WorldModelAsset *a,WorldModelAsset *c){return b&&a&&c;}
extern "C" void world_model_pose_blend_free(WorldModelPoseBlend *b){if(!b)return;assert(active_batches);active_batches--;delete b;}
extern "C" bool world_model_pose_blend_upload(WorldModelPoseBlend *b,const WorldModelInstance *,const float *,std::size_t n){return b&&n<=b->capacity;}
extern "C" void world_model_pose_blend_draw(WorldModelPoseBlend *,unsigned){assert(false&&"CPU fixture must not draw");}
extern "C" GLint glGetUniformLocation(GLuint,const GLchar *){assert(false&&"CPU fixture must not query GL");return -1;}
extern "C" void glUseProgram(GLuint){assert(false);}
extern "C" void glUniform1i(GLint,GLint){assert(false);}
extern "C" void glUniform1f(GLint,GLfloat){assert(false);}
extern "C" void glUniform2f(GLint,GLfloat,GLfloat){assert(false);}
extern "C" void glUniform3f(GLint,GLfloat,GLfloat,GLfloat){assert(false);}

namespace {
float support(double x,double z,float yaw,float hx,float hz,float &spread){
    float low=1e9f,high=-1e9f;const double c=std::cos(yaw),s=std::sin(yaw);
    for(int ix=-1;ix<=1;ix++)for(int iz=-1;iz<=1;iz++){
        const double px=x+c*ix*hx+s*iz*hz,pz=z-s*ix*hx+c*iz*hz;
        assert(!world_atlas_local_water(px,pz,nullptr,nullptr));const float h=world_atlas_height(px,pz);
        assert(std::isfinite(h)&&h>.3f);low=std::min(low,h);high=std::max(high,h);
    }
    spread=high-low;assert(spread<.05f);return high+.005f;
}
void path_point(unsigned index,double distance,double &x,double &z){
    const auto &path=bandit_camp_paths[index];distance=std::fmod(distance,path.length);if(distance<0)distance+=path.length;
    unsigned lo=0,hi=path.count-1;
    while(hi-lo>1){const unsigned mid=(lo+hi)/2;if(bandit_camp_path_points[path.first+mid].distance<=distance)lo=mid;else hi=mid;}
    const auto &a=bandit_camp_path_points[path.first+lo],&b=bandit_camp_path_points[path.first+hi];const double w=(distance-a.distance)/(b.distance-a.distance);
    x=a.x+(b.x-a.x)*w;z=a.z+(b.z-a.z)*w;
}
}
int main(int argc,char **argv){
    if(argc>1){assert(!chdir(argv[1]));}
    assert(world_atlas_init());
    const char *files[]={"scenery/authored_instances.txt","scenery/starting_area_instances.txt","scenery/rainforest_instances.txt","scenery/castle_village_instances.txt","scenery/tanthoon_bandit_camp_instances.txt"};
    char error[512]{};auto *scene=world_scenery_load_files(files,5,"models",error,sizeof(error));
    if(!scene){std::fprintf(stderr,"Scene failure: %s\n",error);return 1;}
    const auto stats=world_scenery_stats(scene);assert(stats.source_files==5&&stats.source_instances==5585);
    assert(world_scenery_source_stats(scene,4).instances==296);
    std::size_t player_sweeps=0,npc_sweeps=0,masked_models=0,mask_probes=0,vegetation_exceptions=0;
    double minimum_mask=1;float worst_spread=0;
    for(std::size_t i=0;i<stats.source_instances;i++){
        WorldSceneryPlacement p;assert(world_scenery_placement(scene,i,&p));if(p.source_file!=4)continue;
        const bool vegetation=!std::strcmp(p.alias,"pine")||!std::strcmp(p.alias,"fir")||!std::strcmp(p.alias,"broadleaf")||
            !std::strncmp(p.alias,"fern",4)||!std::strcmp(p.alias,"showcase_sorrel");
        if(vegetation){vegetation_exceptions++;continue;}
        const double c=std::cos(p.yaw),s=std::sin(p.yaw);
        for(unsigned iz=0;iz<=8;iz++)for(unsigned ix=0;ix<=8;ix++){
            const double lx=(p.minimum[0]+(p.maximum[0]-p.minimum[0])*ix/8.)*p.scale;
            const double lz=(p.minimum[2]+(p.maximum[2]-p.minimum[2])*iz/8.)*p.scale;
            const double x=p.x+c*lx+s*lz,z=p.z-s*lx+c*lz;
            const auto mask=world_atlas_bandit_sample(x,z);minimum_mask=std::min(minimum_mask,mask.grass_exclusion);mask_probes++;
            if(mask.grass_exclusion<.98){std::fprintf(stderr,"Unmasked rotated model %s local(%g,%g) grass%g\n",p.alias,x-bandit_camp_x,z-bandit_camp_z,mask.grass_exclusion);return 1;}
            assert(mask.height_delta==0);
        }
        masked_models++;
    }
    assert(masked_models+vegetation_exceptions==296&&masked_models>150&&vegetation_exceptions>80);
    for(std::size_t k=1;k<std::size(bandit_walk_route);k++){
        const auto &a=bandit_walk_route[k-1],&b=bandit_walk_route[k];const double dx=b[0]-a[0],dz=b[1]-a[1],length=std::hypot(dx,dz);const int count=int(std::ceil(length/.25));
        for(double lane:{-.7,0.,.7})for(int i=1;i<=count;i++){
            const double ax=a[0]+dx*(i-1)/count-dz/length*lane,az=a[1]+dz*(i-1)/count+dx/length*lane;
            const double bx=a[0]+dx*i/count-dz/length*lane,bz=a[1]+dz*i/count+dx/length*lane;
            const float feet=std::max(world_atlas_height(ax,az),world_atlas_height(bx,bz))+.005f;
            assert(!world_atlas_local_water(bx,bz,nullptr,nullptr));
            if(!world_scenery_can_move(scene,ax,az,bx,bz,feet,2.f,.4f)){
                std::fprintf(stderr,"Blocked player segment%zu step%d lane%g local(%g,%g)\n",k,i,lane,bx-bandit_camp_x,bz-bandit_camp_z);return 1;
            }
            player_sweeps++;
        }
    }
    for(unsigned p=0;p<std::size(bandit_camp_paths);p++){
        const auto &path=bandit_camp_paths[p];const auto &clip=bandit_camp_clips[p];const int steps=int(std::ceil(path.length/.2));
        for(int i=1;i<=steps;i++){
            const double t=double(i)*path.length/steps;double ax,az,bx,bz,lx,lz,rx,rz;
            path_point(p,t-path.length/steps,ax,az);path_point(p,t,bx,bz);path_point(p,t-.2,lx,lz);path_point(p,t+.2,rx,rz);
            float spread=0;const float feet=support(bx,bz,float(std::atan2(rx-lx,rz-lz)),clip.foot_x,clip.foot_z,spread);worst_spread=std::max(worst_spread,spread);
            assert(world_scenery_can_move(scene,ax,az,bx,bz,feet,2.f,.4f));npc_sweeps++;
        }
    }
    for(const auto &a:bandit_camp_actors)if(a.path<0){float spread=0;const auto &clip=bandit_camp_clips[a.mode];const float feet=support(a.x,a.z,a.yaw,clip.foot_x,clip.foot_z,spread);assert(world_scenery_can_move(scene,a.x,a.z,a.x,a.z,feet,2.f,.4f));worst_spread=std::max(worst_spread,spread);}
    auto *bandits=world_bandit_camp_create("models");assert(bandits&&world_bandit_camp_commit(bandits,true,error,sizeof(error)));
    track=true;const auto before=allocations;
    for(unsigned tick=0;tick<=18000;tick++){
        world_bandit_camp_update(bandits,scene,tick/30.f);
        const auto current=world_bandit_camp_stats(bandits);
        assert(current.actors==10&&current.walking==5&&current.running==1&&current.sentries==4&&current.unsafe_roots==0);
    }
    assert(allocations==before);track=false;
    world_bandit_camp_free(bandits);world_scenery_free(scene);assert(!active_models&&!active_batches);world_atlas_shutdown();
    std::printf("PASS real scene CPU collisions:5 sources/5585records (296camp),actual encoded bounds, %zu three-lane player sweeps≤.25m,%zu NPCpath sweeps≤.2m,4source-foot stances,maxsole%.6fm;600s/18001 real runtime updates,10actors/0unsafe roots/0hot allocations,all stub ownership freed;grass mask%zu models/%zu rotated probes/min%.6f,%zu intended vegetation exceptions;noGPU\n",player_sweeps,npc_sweeps,worst_spread,masked_models,mask_probes,minimum_mask,vegetation_exceptions);
}

/* CPU integration: real encoded bounds, atlas, scene grounding/colliders and
 * additive castle streets. Rendering/model uploads are stubbed; no GL or GPU. */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include "engine/world_models.h"
#include "engine/world_scenery.h"
#include "engine/world_atlas.h"
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
#include "engine/world_castle_streets_route.inc"

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
struct VillageRoad {double ax,az,bx,bz,width,paving;};
struct VillageRect {double x,z,hx,hz,cosine,sine,paving,grass;};
#include "engine/world_castle_streets_ground.inc"
struct VillageLifeDef {const char *name,*role,*district;unsigned archetype;double x,z;float yaw,y_offset;unsigned route;double phase;bool walker,seated;};
#include "engine/world_village_life_data.inc"
double origin_x=0,origin_z=0;
std::size_t sweeps=0;
void sweep(WorldScenery *scene,double ax,double az,double bx,double bz,const char *label,double lane=0){
    const double dx=bx-ax,dz=bz-az,length=std::hypot(dx,dz);assert(length>0);
    const int n=int(std::ceil(length/.25));const double lx=-dz/length*lane,lz=dx/length*lane;
    for(int i=1;i<=n;i++){
        const double x0=origin_x+ax+dx*(i-1)/n+lx,z0=origin_z+az+dz*(i-1)/n+lz;
        const double x1=origin_x+ax+dx*i/n+lx,z1=origin_z+az+dz*i/n+lz;
        const float feet=std::max(world_atlas_height(x0,z0),world_atlas_height(x1,z1))+.02f;
        assert(std::isfinite(feet)&&!world_atlas_local_water(x1,z1,nullptr,nullptr));
        if(!world_scenery_can_move(scene,x0,z0,x1,z1,feet,2.f,.4f)){
            std::fprintf(stderr,"Blocked %s lane%g local(%g,%g)->(%g,%g)\n",label,lane,x0-origin_x,z0-origin_z,x1-origin_x,z1-origin_z);std::exit(1);
        }
        sweeps++;
    }
}
}
int main(int argc,char **argv){
    if(argc>1)assert(!chdir(argv[1]));
    assert(world_atlas_init());world_atlas_village_origin(&origin_x,&origin_z);
    const char *files[]={"scenery/authored_instances.txt","scenery/starting_area_instances.txt","scenery/rainforest_instances.txt","scenery/castle_village_instances.txt","scenery/tanthoon_bandit_camp_instances.txt","scenery/castle_streets_instances.txt"};
    char error[512]{};auto *scene=world_scenery_load_files(files,6,"models",error,sizeof(error));
    if(!scene){std::fprintf(stderr,"Scene failure: %s\n",error);return 1;}
    const auto stats=world_scenery_stats(scene);const auto added=world_scenery_source_stats(scene,5);
    assert(stats.source_files==6&&stats.source_instances==5585+added.instances&&added.instances>190&&added.instances<650);
    assert(world_scenery_source_stats(scene,4).instances==296);
    std::size_t mask_probes=0,masked=0,plants=0;double minimum_mask=1;
    for(std::size_t i=0;i<stats.source_instances;i++){
        WorldSceneryPlacement p;assert(world_scenery_placement(scene,i,&p));if(p.source_file!=5)continue;
        if(!std::strcmp(p.alias,"starting_celandine")||!std::strcmp(p.alias,"starting_wildflower")){plants++;continue;}
        const double c=std::cos(p.yaw),s=std::sin(p.yaw);
        for(unsigned iz=0;iz<=8;iz++)for(unsigned ix=0;ix<=8;ix++){
            const double lx=(p.minimum[0]+(p.maximum[0]-p.minimum[0])*ix/8.)*p.scale;
            const double lz=(p.minimum[2]+(p.maximum[2]-p.minimum[2])*iz/8.)*p.scale;
            const auto mask=world_atlas_village_sample(p.x+c*lx+s*lz,p.z-s*lx+c*lz);
            minimum_mask=std::min(minimum_mask,mask.grass_exclusion);mask_probes++;
            if(mask.grass_exclusion<.98){std::fprintf(stderr,"Unmasked actual model %s local(%g,%g) grass%g\n",p.alias,p.x-origin_x+c*lx+s*lz,p.z-origin_z-s*lx+c*lz,mask.grass_exclusion);return 1;}
            assert(mask.height_delta==0);
        }
        masked++;
    }
    assert(masked+plants==added.instances&&masked>100&&plants==104);
    const std::size_t before_tour=sweeps;
    for(std::size_t k=1;k<std::size(castle_streets_route);k++){
        const auto &a=castle_streets_route[k-1],&b=castle_streets_route[k];
        // Three lanes through the actual2.4m tavern door; source furniture is
        // tested by this same production compound-collider sweep.
        for(double lane:{-.5,0.,.5})sweep(scene,a[0],a[1],b[0],b[1],"tour",lane);
    }
    const auto tour_sweeps=sweeps-before_tour;
    for(const auto &r:castle_street_roads){
        // Side strips certify useful pavement width, not merely a thin line.
        const double side=r.ax==-145&&r.bz==-82?.5:1.;
        for(double lane:{-side,0.,side})sweep(scene,r.ax,r.az,r.bx,r.bz,"street",lane);
    }
    const double walkers[][2]={{-130,-28},{-130,46},{-132,46},{-132,-28},{-130,-28}};
    for(std::size_t k=1;k<std::size(walkers);k++)sweep(scene,walkers[k-1][0],walkers[k-1][1],walkers[k][0],walkers[k][1],"new NPC walker");
    unsigned standing=0,seated=0;
    for(const auto &def:village_life_defs){
        if(def.walker)continue;
        assert(world_atlas_height(origin_x+def.x,origin_z+def.z)==24.f);
        if(def.seated){seated++;continue;} // Bent source bodies sit on .495m benches; a standing capsule is inapplicable.
        const double x=origin_x+def.x,z=origin_z+def.z;
        assert(world_scenery_can_move(scene,x,z+.2,x,z,24.f+def.y_offset+.005f,2.f,.4f));standing++;
    }
    assert(standing==6&&seated==4);
    // Existing original population/pet routes are eastern of this district;
    // prove each full production route rather than assuming source separation.
    const double original[][4][2]={
        {{-18,-75},{18,-75},{18,-85},{-18,-85}},{{-1,-42},{-1,178},{1,178},{1,-42}},
        {{2.4,55},{93,55},{93,125},{2.4,125}},{{-2.4,125},{-93,125},{-93,55},{-2.4,55}},
        {{-4.7,-42},{-4.7,44},{-3.3,44},{-3.3,-42}},{{60.95,-25},{60.95,47},{62.55,47},{62.55,-25}},
        {{17,-12},{17,32},{18.8,32},{18.8,-12}},{{-36.8,68},{-36.8,115},{-35.2,115},{-35.2,68}},
        {{25,150.2},{65,150.2},{65,151.8},{25,151.8}},
        {{21.2,44.8},{21.2,48.2},{22.4,48.2},{22.4,44.8}},{{-20.8,45.5},{-20.8,48.5},{-22,48.5},{-22,45.5}},
        {{46,30},{46,40.5},{47.2,40.5},{47.2,30}},{{-46,30},{-46,40.5},{-47.6,40.5},{-47.6,30}},
        {{19,180},{19,192},{20.6,192},{20.6,180}},{{-19,180},{-19,192},{-20.6,192},{-20.6,180}}
    };
    for(const auto &route:original)for(unsigned k=0;k<4;k++)sweep(scene,route[k][0],route[k][1],route[(k+1)%4][0],route[(k+1)%4][1],"original resident/pet route");
    world_scenery_free(scene);world_atlas_shutdown();assert(active_models==0&&active_batches==0);
    std::printf("PASS castle streets production CPU scene: %zu files/%zu records/%zu added; %zu tour sweeps/%zu total<=.25m sweeps, 6standing/4seated source anchors, %zu masked models/%zu actual-footprint probes/minmask%.6f; %zu intentional plants; no GL/context/draw; ownership0.\n",stats.source_files,stats.source_instances,added.instances,tour_sweeps,sweeps,masked,mask_probes,minimum_mask,plants);
}

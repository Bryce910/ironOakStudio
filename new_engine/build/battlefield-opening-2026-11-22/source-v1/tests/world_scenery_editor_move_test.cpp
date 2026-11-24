#include "tests/world_scenery_pivot_support.inc"
#include "engine/world_scenery_pivot.h"
#include "engine/world_studio_scene_record.h"
#include <iostream>

struct WorldModelInstanceBatch {};
extern "C" WorldModelInstanceBatch *world_model_instance_batch_create(WorldModelAsset *,size_t){return nullptr;}
extern "C" bool world_model_instance_batch_upload(WorldModelInstanceBatch *,const WorldModelInstance *,size_t){return false;}
extern "C" void world_model_instance_batch_draw(WorldModelInstanceBatch *,unsigned){}
extern "C" void world_model_instance_batch_free(WorldModelInstanceBatch *){}

static void check(bool value,const char *label){
    if(!value){std::fprintf(stderr,"FAIL %s\n",label);std::abort();}
}
static WorldScenery *load(const std::string &path,const world_studio_scene_record::Record &record){
    {std::ofstream file(path);world_studio_scene_record::write(file,record);check(bool(file),"write temporary scene");}
    char error[512]={};auto *scene=world_scenery_load(path.c_str(),"models",error,sizeof(error));
    if(!scene)std::fprintf(stderr,"%s\n",error);
    check(scene!=nullptr,"load authored decoration");return scene;
}
int main(int argc,char **argv){
    check(argc==2,"project path supplied");
    check(world_atlas_init(),"initialize atlas");
    char error[512]={};
    check(world_atlas_load_height_region((std::string(argv[1])+"/ground/showcase.height").c_str(),error,sizeof(error))==1,"load project heights");
    const WorldSceneryDefinition definitions[]={{"fixture_interior","showcase_decor",1,0,0,240,75},{"fixture_wall","showcase_decor",1,1,0,240,75}};
    check(world_scenery_configure(definitions,2,error,sizeof(error)),"configure decoration and collidable structure");
    size_t count=0;const auto *trail=world_atlas_trail(&count);check(count>2,"trail available");
    const double dx=trail[2].x-trail[1].x,dz=trail[2].z-trail[1].z,length=std::hypot(dx,dz);
    const double x=trail[1].x-dz/length*45,z=trail[1].z+dx/length*45;
    world_studio_scene_record::Record record;record.name="fixture_interior";world_atlas_world_to_uv(x,z,&record.u,&record.v);
    record.pitch=12;record.pivot=600;record.preserve_source_root=true;
    const std::string path="/tmp/valwood-editor-move-"+std::to_string(getpid())+".txt";
    auto *scene=load(path,record);WorldSceneryPlacement before{};
    check(world_scenery_placement(scene,0,&before)&&world_scenery_explicit_pivot(scene,0),"elevated source pose is explicit");
    check(world_scenery_stats(scene).source_colliders==0,"decoration has no movement collider");
    WorldSceneryEditorTransform move{0,before.x+1,before.z,before.y+.5f,before.yaw+.1f,before.scale,before.pitch,before.roll,before.sx,before.sy,before.sz,before.offset_y+.5f};
    check(world_scenery_editor_commit_moves(scene,&move,1),"elevated full-transform move commits in place");
    WorldSceneryPlacement committed{};check(world_scenery_placement(scene,0,&committed),"committed pose available");
    check(std::abs(committed.x-move.x)<1.e-6&&std::abs(committed.y-move.y)<1.e-4&&std::abs(committed.yaw-move.yaw)<1.e-5,"committed pose matches preview");
    world_scenery_editor_clear(scene);WorldSceneryPlacement after_clear{};
    check(world_scenery_placement(scene,0,&after_clear)&&after_clear.y==committed.y,"clearing preview preserves the commit");
    record.yaw=double(move.yaw)*57.29577951308232;record.offset_y=move.offset_y;record.pivot=move.y;world_atlas_world_to_uv(move.x,move.z,&record.u,&record.v);
    auto *reloaded=load(path,record);WorldSceneryPlacement reopened{};
    check(world_scenery_placement(reloaded,0,&reopened)&&std::abs(reopened.x-committed.x)<1.e-5&&std::abs(reopened.y-committed.y)<1.e-4,"save and reload reproduce in-place move");
    world_scenery_free(reloaded);world_scenery_free(scene);
    record.name="fixture_wall";scene=load(path,record);
    check(world_scenery_placement(scene,0,&before)&&world_scenery_stats(scene).source_colliders>0,"structural fixture has collision");
    move={0,before.x+1,before.z,before.y+.5f,before.yaw+.1f,before.scale,before.pitch,before.roll,before.sx,before.sy,before.sz,before.offset_y+.5f};
    check(!world_scenery_editor_commit_moves(scene,&move,1),"collidable elevated structure rejects decorative fast path");
    world_scenery_editor_clear(scene);check(world_scenery_placement(scene,0,&reopened)&&reopened.x==before.x&&reopened.y==before.y,"rejected structural preview restores collider pose");
    world_scenery_free(scene);
    check(world_scenery_configure(nullptr,0,error,sizeof(error)),"restore alias registry");
    check(!active_models,"all model fixtures released");world_atlas_shutdown();std::remove(path.c_str());
    std::cout<<"PASS elevated furniture move, save/reopen, and structural collision guard\n";
}

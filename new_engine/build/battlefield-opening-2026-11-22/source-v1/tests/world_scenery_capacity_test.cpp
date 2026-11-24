#include "tests/world_scenery_pivot_support.inc"
#include "engine/world_studio_scene_record.h"
#include "engine/world_scenery_duplicate_index.h"
#include <filesystem>
#include <random>

struct WorldModelInstanceBatch {};
extern "C" WorldModelInstanceBatch *world_model_instance_batch_create(WorldModelAsset *,size_t){return nullptr;}
extern "C" bool world_model_instance_batch_upload(WorldModelInstanceBatch *,const WorldModelInstance *,size_t){return false;}
extern "C" void world_model_instance_batch_draw(WorldModelInstanceBatch *,unsigned){}
extern "C" void world_model_instance_batch_free(WorldModelInstanceBatch *){}
static unsigned checks=0;
static void check(bool value,const char *label){++checks;if(!value){std::fprintf(stderr,"FAIL %s\n",label);std::abort();}}
static void record(std::ostream &out,unsigned alias,unsigned index){
    const auto name="fixture_"+std::to_string(alias);
    world_studio_scene_record::Record r;r.name=name.c_str();
    world_atlas_world_to_uv(30000+(index%64)*.05,20000+(index/64)*.05,&r.u,&r.v);
    r.pivot=600;r.preserve_source_root=true;world_studio_scene_record::write(out,r);
}
int main(){
    check(WORLD_SOURCE_RECORD_LIMIT==65536&&WORLD_ADDED_KEY_BASE==65536,"expanded namespace");
    check(WORLD_SCENERY_ALIAS_RECORD_LIMIT==4096&&WORLD_SCENERY_SOURCE_FILE_LIMIT==16,"separate GPU/source guards");
    // Compare the new index against the previous exact predicate, including
    // negative cells, near-boundary roots, stacked offsets and float rounding.
    struct Root {unsigned alias;double x,z;float e,y;};std::vector<Root> old;
    WorldSceneryDuplicateIndex index;std::mt19937 random(2031);
    for(unsigned i=0;i<12000;++i){
        Root r{unsigned(random()%3),(int(random()%200)-100)*.003,(int(random()%200)-100)*.003,float(random()%300)*.01f,float(int(random()%257)-128)};
        if(i%2&&old.size()){r=old[random()%old.size()];r.x+=double(int(random()%21)-10)*.001;r.y+=float(int(random()%21)-10)*.001f;}
        bool duplicate=false;for(const auto &p:old)if(p.alias==r.alias&&std::abs(p.e+p.y-r.e-r.y)<.01&&std::hypot(p.x-r.x,p.z-r.z)<.01){duplicate=true;break;}
        check(index.insert(r.alias,r.x,r.z,r.e,r.y)==!duplicate,"duplicate semantics match exhaustive predicate");
        if(!duplicate)old.push_back(r);
    }
    check(world_atlas_init(),"real atlas initializes");
    std::vector<std::string> names;std::vector<WorldSceneryDefinition> definitions;
    for(unsigned i=0;i<17;++i)names.push_back("fixture_"+std::to_string(i));
    for(const auto &name:names)definitions.push_back({name.c_str(),"showcase_decor",1,0,1,240,75});
    char error[512]={};check(world_scenery_configure(definitions.data(),definitions.size(),error,sizeof(error)),"configure seventeen test aliases");
    const auto dir=std::filesystem::path("/tmp")/("valwood-capacity-"+std::to_string(getpid()));std::filesystem::create_directories(dir);
    std::vector<std::string> files;for(unsigned i=0;i<16;++i){files.push_back((dir/(std::to_string(i)+".txt")).string());std::ofstream out(files.back());for(unsigned n=0;n<4096;++n)record(out,i,n);}
    std::vector<const char *> paths;for(const auto &file:files)paths.push_back(file.c_str());
    const auto begin=std::chrono::steady_clock::now();
    auto *scene=world_scenery_load_files(paths.data(),paths.size(),"models",error,sizeof(error));
    if(!scene)std::fprintf(stderr,"%s\n",error);check(scene!=nullptr,"all65536 records load across16 sources");
    const auto stats=world_scenery_stats(scene);check(stats.source_instances==65536&&stats.source_files==16&&stats.source_aliases==16,"complete capacity statistics");
    WorldSceneryPlacement last;check(world_scenery_placement(scene,65535,&last)&&std::string(last.alias)=="fixture_15","highest source index is intact");
    check(!world_scenery_placement(scene,65536,&last),"source boundary rejects missing index");
    world_scenery_update(scene,30000,600,20000,0,0);check(world_scenery_stats(scene).largest_batch<=4096,"selection retains GPU batch bound");
    world_scenery_free(scene);check(active_models==0,"capacity scene releases all models");
    {std::ofstream out(files.back(),std::ios::app);record(out,16,0);}
    scene=world_scenery_load_files(paths.data(),paths.size(),"models",error,sizeof(error));check(!scene&&std::strstr(error,"65536-record limit exceeded"),"record65537 fails before allocation/upload");check(active_models==0,"over-capacity failure frees resources");
    {std::ofstream out(files[0],std::ios::app);record(out,0,4096);}
    scene=world_scenery_load(files[0].c_str(),"models",error,sizeof(error));check(!scene&&std::strstr(error,"4096-instance alias limit exceeded"),"peralias4097 stays guarded");
    paths.push_back(paths[0]);scene=world_scenery_load_files(paths.data(),paths.size(),"models",error,sizeof(error));check(!scene&&std::strstr(error,"source-file count"),"source17 stays guarded");
    check(world_scenery_configure(nullptr,0,error,sizeof(error)),"restore alias registry");world_atlas_shutdown();std::filesystem::remove_all(dir);
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
    std::printf("PASS scenery capacity: %u checks; 65536-source records, unchanged4096 batches, exact duplicate policy; %.3fs including three full parser passes\n",checks,seconds);
}

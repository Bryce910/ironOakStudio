// Actual source parser, LOD selection, shadow clipping and instance propagation;
// the model/GL resource boundary is deterministic CPU-only test support.
#include <atomic>
#define glGetUniformLocation item_wind_prior_uniform_location
#define glUniform1i item_wind_prior_uniform_i
#define world_model_draw_instances item_wind_prior_draw_instances
#include "tests/world_scenery_pivot_support.inc"
#undef glGetUniformLocation
#undef glUniform1i
#undef world_model_draw_instances
static int item_wind_scope=0;
extern "C" void glUniform1i(GLint location,GLint value){if(location==17)item_wind_scope=value;item_wind_prior_uniform_i(location,value);}
extern "C" void world_model_draw_instances(WorldModelAsset *asset,unsigned program,size_t count){assert(item_wind_scope==1);item_wind_prior_draw_instances(asset,program,count);}
#include <iomanip>
#include <iostream>
extern "C" GLint glGetUniformLocation(GLuint p,const GLchar *name){
    if(!std::strcmp(name,"uRootedGrassWind"))return 16;
    if(!std::strcmp(name,"uItemWindEnabled"))return 17;
    return item_wind_prior_uniform_location(p,name);
}
struct WorldModelInstanceBatch {WorldModelAsset asset;size_t capacity;};
extern "C" WorldModelInstanceBatch *world_model_instance_batch_create(WorldModelAsset *asset,size_t capacity){return new WorldModelInstanceBatch{*asset,capacity};}
extern "C" bool world_model_instance_batch_upload(WorldModelInstanceBatch *b,const WorldModelInstance *p,size_t count){assert(count<=b->capacity);return world_model_upload_instances(&b->asset,p,count);}
extern "C" void world_model_instance_batch_draw(WorldModelInstanceBatch *b,unsigned program){world_model_draw_instances(&b->asset,program,b->asset.count);}
extern "C" void world_model_instance_batch_free(WorldModelInstanceBatch *b){delete b;}
namespace {
unsigned checks=0;
void check(bool value,const char *label){if(!value){std::fprintf(stderr,"FAIL %s\n",label);std::abort();}++checks;}
std::string path;
WorldScenery *load(const std::string &text,bool valid=true,const char *expected_error=nullptr){
    {std::ofstream f(path);f<<text;check(bool(f),"write isolated scene fixture");}
    char error[512]={};auto *scene=world_scenery_load(path.c_str(),"models",error,sizeof(error));
    if(valid&&!scene)std::fprintf(stderr,"%s\n",error);
    check(valid?scene!=nullptr:scene==nullptr,valid?"production parser accepts record":"production parser rejects malformed item wind");
    if(expected_error)check(std::strstr(error,expected_error)!=nullptr,"rejection reports the specific record-bound failure");
    return scene;
}
std::string row(const char *alias,double x,double z,const std::string &suffix){double u=0,v=0;world_atlas_world_to_uv(x,z,&u,&v);std::ostringstream s;s<<std::setprecision(17)<<alias<<' '<<u<<' '<<v<<" 0 1 0"<<suffix<<'\n';return s.str();}
void inspect(float x,float second_x){
    check(item_wind_scope==0,"item wind scope restored after scenery draw");
    size_t instances=0;
    for(size_t j=0;j<captured_draws.count;j++)for(size_t k=0;k<captured_draws.records[j].count;k++){
        const auto &r=captured_draws.records[j];const auto &p=r.asset->instances[k];const double wx=double(p.x)+r.anchor[0];
        if(std::abs(wx-x)<.01)check(p.wind_sway==0&&p.wind_flutter==0,"disabled copied object retains response through actual draw");
        else {check(std::abs(wx-second_x)<.01,"draw belongs to second same-alias object");check(p.wind_sway==1.75f&&p.wind_flutter==.25f,"same-alias object retains distinct response through actual draw");}
        ++instances;
    }
    check(instances>0,"draw contains inspected instances");
}
}
int main(){
    check(world_atlas_init(),"real atlas initialized");size_t count=0;const auto *trail=world_atlas_trail(&count);check(count>2,"dry fixture root from real route");
    double dx=trail[2].x-trail[1].x,dz=trail[2].z-trail[1].z,length=std::hypot(dx,dz),x=trail[1].x-dz/length*45,z=trail[1].z+dx/length*45;
    path="/tmp/valwood-item-wind-parser-"+std::to_string(getpid())+".txt";
    WorldModelInstance ordinary={0,0,0,0,1};check(ordinary.wind_sway==1&&ordinary.wind_flutter==1,"legacy aggregate instance defaults unchanged");
    for(const char *numeric:{""," 0"," 0 0"," 0 0 0 0 1 1 1"," 0 0 0 0 1 1 1 100"}){
        auto *scene=load(row("pine",x,z,numeric));WorldSceneryPlacement p;check(world_scenery_placement(scene,0,&p)&&p.wind_sway==1&&p.wind_flutter==1,"all legacy numeric formats retain default wind");world_scenery_free(scene);
        scene=load(row("pine",x,z,std::string(numeric)+" wind .4 1.8"));check(world_scenery_placement(scene,0,&p)&&p.wind_sway==.4f&&p.wind_flutter==1.8f,"tagged suffix accepted after each legacy format");world_scenery_free(scene);
    }
    for(const char *bad:{" wind"," wind 1"," wind -1 1"," wind 1 2.01"," wind nan 1"," wind 1 inf"," wind 1 1 wind 0 0"," wind 1 1 4"," 0 0 0 wind 1 1"," 0 0 0 0 1 1 wind 1 1"})load(row("pine",x,z,bad),false);
    // A valid numeric pivot is extended with fractional zeros to the exact
    // historical byte boundary. This is an actual accepted legacy record,
    // rather than whitespace that a parser could trim before accounting.
    std::string long_base=row("pine",x,z," 0 0 0 0 1 1 1 100");long_base.pop_back();
    check(long_base.size()<255,"long fixture starts inside the legacy budget");
    long_base+='.';long_base.append(256-long_base.size(),'0');
    check(long_base.size()==256,"exact256-byte meaningful legacy base fixture");
    auto *bounded=load(long_base+'\n');world_scenery_free(bounded);
    const std::string wind_tail=" wind .4 1.8";
    check(long_base.size()+wind_tail.size()>256,"wind extension crosses old line limit");
    bounded=load(long_base+wind_tail+'\n');WorldSceneryPlacement bounded_pose;
    check(world_scenery_placement(bounded,0,&bounded_pose)&&bounded_pose.y==100&&bounded_pose.wind_sway==.4f&&bounded_pose.wind_flutter==1.8f,"long base plus wind preserves exact pivot and both factors");world_scenery_free(bounded);
    std::string full_tail=wind_tail+" #";full_tail.append(64-full_tail.size(),'c');
    check(long_base.size()+full_tail.size()==320,"exact320-byte extended record fixture");
    bounded=load(long_base+full_tail+'\n');world_scenery_free(bounded);
    load(long_base+"0\n",false,"256 legacy");
    load(long_base+"0"+wind_tail+'\n',false,"base exceeds 256");
    load(long_base+full_tail+"x\n",false,"320");
    load(long_base.substr(0,240)+full_tail+"x\n",false,"tail exceeds 64");
    load(long_base+" # wind .4 1.8\n",false,"256 legacy");
    load(long_base+" # filler wind .4 1.8\n",false,"256 legacy");
    load(long_base+"  wind .4 1.8\n",false,"base exceeds 256");
    load(long_base+"wind .4 1.8\n",false,"invalid numeric");
    load(long_base+" wind .4 #1.8\n",false,"invalid item wind");
    load(std::string(257,' ')+'\n'+row("pine",x,z,""),false,"256 legacy");
    load('#'+std::string(256,'c')+'\n'+row("pine",x,z,""),false,"256 legacy");
    auto *scene=load(row("pine",x,z," 0 0 0 0 1 1 1 100 wind 0 0")+row("pine",x+40,z," 0 0 0 0 1 1 1 100 wind 1.75 .25"));
    capture_draws=true;
    for(float distance:{35.f,125.f,330.f}){
        world_scenery_update(scene,x,110,z+distance,0,0,16.f/9);
        captured_draws={};world_scenery_draw(scene,1,x,110,z+distance,false);inspect(float(x),float(x+40));
        if(distance<200){captured_draws={};world_scenery_draw(scene,2,x,110,z+distance,true);inspect(float(x),float(x+40));}
    }
    // Force a partial light-frustum selection: only the first small crown is
    // inside this narrow orthographic volume; the independent shadow batch
    // must copy both item factors along with the selected pose.
    world_scenery_update(scene,x,110,z+35,0,0,16.f/9);float vp[16]={};vp[0]=.12f;vp[5]=.015f;vp[10]=.015f;vp[15]=1;
    world_scenery_shadow_clip(scene,vp);captured_draws={};world_scenery_draw(scene,2,x,110,z+35,true);inspect(float(x),float(x+40));
    const auto stats=world_scenery_stats(scene);check(stats.shadow_clip_rejected>0&&stats.shadow_clip_submitted>0,"actual shadow clipping exercises partial independent batch");
    world_scenery_free(scene);check(!active_models,"all shared models freed");std::remove(path.c_str());world_atlas_shutdown();
    std::printf("PASS %u item wind checks: actual legacy/tagged parser including256+64byte bounds/comment-trick rejection, independent copied items, near/mid/far and shadow proxy streams, clipped shadow batch; CPU only\n",checks);
}

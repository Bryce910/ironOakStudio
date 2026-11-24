// Actual scenery loader/selection with deterministic atlas/model/GL controls.
// Private batch inspection is confined to this one executable, with no ABI.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage"
#include "engine/world_scenery.cpp"
#pragma GCC diagnostic pop
#include <atomic>
#include <cassert>
#include <unistd.h>
namespace {std::atomic<std::size_t> heap{0};unsigned active=0,draws=0;float wind=0;int vegetation=0;const void *last_asset=nullptr;}
[[gnu::noinline]] void *operator new(std::size_t n){heap++;if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,std::size_t)noexcept{std::free(p);}
struct WorldModelAsset {std::string path;std::size_t count=0;};
extern "C" WorldModelAsset *world_model_load(const char *p){active++;return new WorldModelAsset{p,0};}
extern "C" void world_model_free(WorldModelAsset *p){assert(p);active--;delete p;}
extern "C" bool world_model_bounds(const WorldModelAsset *p,float lo[3],float hi[3]){lo[0]=lo[2]=-2;lo[1]=0;hi[0]=hi[2]=2;hi[1]=p->path.find("wheat")!=std::string::npos?1.1f:2.2f;return true;}
extern "C" bool world_model_upload_instances(WorldModelAsset *p,const WorldModelInstance *,std::size_t n){assert(n<=4096);p->count=n;return true;}
extern "C" void world_model_draw_instances(WorldModelAsset *p,unsigned,std::size_t n){assert(n<=p->count&&vegetation==6&&wind==.6f);draws++;last_asset=p;}
extern "C" GLint glGetUniformLocation(GLuint,const GLchar *n){const char *names[]={"uOffset","uWorldAnchor","uGrass","uVegetation","uLodLevel","uWind","uTreeHeight","uPropLodRange"};for(unsigned i=0;i<8;i++)if(!std::strcmp(n,names[i]))return int(i+1);return -1;}
extern "C" void glUseProgram(GLuint){}
extern "C" void glUniform1i(GLint l,GLint v){if(l==4)vegetation=v;if(l==3)assert(v==0);}
extern "C" void glUniform1f(GLint l,GLfloat v){if(l==6)wind=v;}
extern "C" void glUniform2f(GLint,GLfloat,GLfloat){}
extern "C" void glUniform3f(GLint,GLfloat,GLfloat,GLfloat){}
extern "C" WorldAtlasSample world_atlas_sample(double,double){return {24,1,0,.5f,100,1};}
extern "C" float world_atlas_height(double,double){return 24;}
extern "C" bool world_atlas_local_water(double,double,float*,float*){return false;}
extern "C" void world_atlas_uv_to_world(double u,double v,double*x,double*z){*x=(u-.5)*100800;*z=(v-.5)*56708.8;}
extern "C" void world_atlas_village_origin(double*x,double*z){*x=*z=0;}
extern "C" const WorldAtlasTrailPoint *world_atlas_trail(std::size_t*n){*n=0;return nullptr;}
extern "C" const WorldAtlasPool *world_atlas_rainforest_pool(){static const WorldAtlasPool p={10000,10000,24,5,5,.5};return &p;}
int main() {
    char path[160],error[512];std::snprintf(path,sizeof(path),"/tmp/valwood-crop-selection-%ld.txt",long(getpid()));
    for(const char *name:{"village_crop_corn","village_crop_wheat"}) {
        {std::ofstream file(path);file<<name<<" .5 .5 0 1 0\n";}
        auto *scene=world_scenery_load(path,"models",error,sizeof(error));if(!scene)std::fprintf(stderr,"%s\n",error);assert(scene);
        const auto initial=world_scenery_stats(scene);assert(initial.loaded_groups==2&&initial.shadow_proxy_groups==1&&initial.source_colliders==0);
        assert(scene->groups[0].vegetation==6&&scene->groups[1].vegetation==6&&scene->groups[0].shadow&&!scene->groups[1].shadow);
        assert(scene->groups[0].hi==64&&scene->groups[1].lo==56&&scene->groups[1].hi==320);
        assert(scene->groups[0].asset!=scene->groups[0].shadow_asset&&scene->groups[0].shadow_asset->path.find("_mid.glb")!=std::string::npos);
        WorldSceneryPlacement p;assert(world_scenery_placement(scene,0,&p)&&p.y==24&&!p.rigid&&!p.foundation);
        assert(world_scenery_can_move(scene,-3,0,3,0,24));
        for(float distance:{20.f,56.f,60.f,63.99f,64.f,280.f,300.f,320.f}) {
            world_scenery_update(scene,0,25.75,distance,0,0);
            const auto stats=world_scenery_stats(scene);
            assert(stats.shadow_instances==(distance<64?1u:0u));
            assert(stats.color_instances==(distance<56?1u:distance<64?2u:distance<320?1u:0u));
            if(distance<64){world_scenery_draw(scene,1,0,25.75,distance,true);assert(last_asset==scene->groups[0].shadow_asset);}
        }
        world_scenery_update(scene,0,25.75,20,3.14159265f,0);assert(world_scenery_stats(scene).shadow_instances==1&&world_scenery_stats(scene).color_instances==0);
        world_scenery_update(scene,0,25.75,60,0,0);const auto previous=heap.load();
        for(unsigned frame=0;frame<36000;frame++){world_scenery_update(scene,0,25.75,60,0,0);assert(world_scenery_stats(scene).uploads_this_update==0);}
        assert(heap.load()==previous);
        // Existing measured geometry sphere+2m must cover every yaw/scale and
        // the proven .35m full crop-wind envelope, including reflected views.
        for(float scale:{.15f,1.f,4.f}) {
            const auto &g=scene->groups[0];const double support=std::hypot(g.radius*scale,g.height*scale*.5)+.35;
            assert(support<g.sphere_radius*scale+2.);
        }
        world_scenery_free(scene);assert(active==0);
    }
    std::remove(path);std::puts("PASS crop scenery: Root/None aliases, explicit near/mid,56–64m bands,320m endpoint, independent near mid-shadow proxy/offscreen caster, no far casters, full source+wind bounds,600s stationary zero allocations/uploads");
}

#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "engine/world_atlas.h"
#include "engine/world_cover.h"
#include "engine/world_grass_transition.h"
#include "engine/world_castle_visibility.h"
#include <array>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <tuple>
#include <utility>

namespace {bool track=false,capture_candidates=true;std::size_t allocations=0;}
[[gnu::noinline]] void *operator new(std::size_t n){if(track)allocations++;if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,std::size_t)noexcept{std::free(p);}
struct Buffer {std::vector<unsigned char> bytes;std::uint64_t uploads=0;};
static std::array<Buffer,64> buffers;
static GLuint next_buffer=1,bound_array=0,bound_element=0,next_vao=1;
void glGenBuffers(GLsizei n,GLuint *out){for(int i=0;i<n;i++){assert(next_buffer<buffers.size());out[i]=next_buffer++;buffers[out[i]].bytes.reserve(WorldGrassTransition::max_instances*6*sizeof(float));}}
void glGenVertexArrays(GLsizei n,GLuint *out){for(int i=0;i<n;i++)out[i]=next_vao++;}
void glBindBuffer(GLenum target,GLuint id){(target==GL_ARRAY_BUFFER?bound_array:bound_element)=id;}
void glBufferData(GLenum target,GLsizeiptr size,const void *source,GLenum){auto &b=buffers[target==GL_ARRAY_BUFFER?bound_array:bound_element];assert(size>=0&&std::size_t(size)<=b.bytes.capacity());b.bytes.resize(std::size_t(size));if(size)std::memcpy(b.bytes.data(),source,std::size_t(size));b.uploads++;}
void glBindVertexArray(GLuint){}void glDeleteBuffers(GLsizei,const GLuint*){}void glDeleteVertexArrays(GLsizei,const GLuint*){}
void glEnableVertexAttribArray(GLuint){}void glVertexAttribPointer(GLuint,GLint,GLenum,GLboolean,GLsizei,const void*){}
void glVertexAttribDivisor(GLuint,GLuint){}void glUniform4f(GLint,GLfloat,GLfloat,GLfloat,GLfloat){}
void glDrawElementsInstanced(GLenum,GLsizei,GLenum,const void*,GLsizei){}
struct V3 {float x,y,z;};
static V3 norm(V3 a){const float d=std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);return {a.x/d,a.y/d,a.z/d};}
struct Camera {
    double x=0,y=0,z=0;float yaw=0,pitch=0,aspect=16.f/9;
    V3 forward()const{return {std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)};}
    V3 right()const{return {std::cos(yaw),0,std::sin(yaw)};}
};
struct Program {unsigned id=1;int at(const char*){return 1;}void i(const char*,int){}void f(const char*,float){}void v3(const char*,V3){}void v2(const char*,float,float){}};
struct WorldModelAsset {
    std::vector<WorldModelInstance> instances;std::vector<float> fades;
    float minimum[3]={-1,0,-1},maximum[3]={1,1,1};
    std::uint64_t uploads=0;bool failing=false,fading=false;
};
WorldModelAsset *world_model_load(const char *path){auto *a=new WorldModelAsset;a->instances.reserve(4096);a->fades.reserve(4096);a->maximum[1]=std::strstr(path,"grass")?.45f:std::strstr(path,"fern")?.9f:std::strstr(path,"rock")?3.2f:std::strstr(path,"fir")?22.f:25.f;return a;}
void world_model_free(WorldModelAsset *a){delete a;}
bool world_model_bounds(const WorldModelAsset *a,float *lo,float *hi){if(!a)return false;std::copy(a->minimum,a->minimum+3,lo);std::copy(a->maximum,a->maximum+3,hi);return true;}
size_t world_model_instance_count(const WorldModelAsset *a){return a?a->instances.size():0;}
static bool upload(WorldModelAsset *a,const WorldModelInstance *p,const float *f,size_t n,bool fading){
    if(!a)return false;
    a->uploads++;if(a->failing){a->failing=false;return false;}
    assert(n<=a->instances.capacity());a->instances.resize(n);if(n)std::memcpy(a->instances.data(),p,n*sizeof(*p));
    a->fades.resize(fading?n:0);if(fading&&n)std::memcpy(a->fades.data(),f,n*sizeof(*f));a->fading=fading;return true;
}
bool world_model_upload_instances(WorldModelAsset *a,const WorldModelInstance *p,size_t n){return upload(a,p,nullptr,n,false);}
bool world_model_upload_fading_instances(WorldModelAsset *a,const WorldModelInstance *p,const float *f,size_t n){return upload(a,p,f,n,true);}
void world_model_draw(WorldModelAsset*,unsigned){}
#include "dressing_selection_viewer.inc"

struct Snapshot {WorldCoverField field={};std::vector<WorldModelInstance> roots;WorldCoverField get()const{auto out=field;out.instances=roots.data();out.count=roots.size();return out;}};
using Published=std::array<Snapshot,WORLD_COVER_KIND_COUNT>;
static Published publish(double x,double z){
    Published out;auto *cover=world_cover_init();assert(cover&&world_cover_update(cover,x,z));
    for(unsigned pass=0;;pass++){
        assert(pass<1000);world_cover_pump(cover,4096);WorldCoverField f;
        while(world_cover_poll(cover,&f)){out[f.kind].field=f;out[f.kind].roots.assign(f.instances,f.instances+f.count);}
        WorldCoverMetrics m;world_cover_metrics(cover,&m);if(!m.near_building&&!m.tree_building)break;
    }
    world_cover_shutdown(cover);return out;
}
template<class A,class B> static void same_bytes(const std::vector<A> &a,const std::vector<B> &b){static_assert(sizeof(A)==sizeof(B));assert(a.size()==b.size());assert(a.empty()||!std::memcmp(a.data(),b.data(),a.size()*sizeof(A)));}
static void same_asset(const WorldModelAsset *a,const WorldModelAsset *b){assert(bool(a)==bool(b));if(!a)return;same_bytes(a->instances,b->instances);same_bytes(a->fades,b->fades);assert(a->fading==b->fading&&a->uploads==b->uploads);}
template<class D> static void install(D &d,const Published &p){
    for(unsigned k=0;k<WORLD_COVER_KIND_COUNT;k++)d.fields[k]=p[k].get();
    assert(d.grass_transition.replace(p[WORLD_COVER_GRASS].get()));d.fields[WORLD_COVER_GRASS]=d.grass_transition.field();
    d.tree_species.clear();const auto &f=d.fields[WORLD_COVER_TREES];
    for(std::size_t i=0;i<f.count;i++){const auto &a=f.instances[i];d.tree_species.push_back(static_cast<unsigned char>(D::species(D::root_hash(f,a),world_atlas_rainforest_weight(f.anchor_x+a.x,f.anchor_z+a.z))));}
    d.has_selection=false;
}
template<class D,class C> static void compare(const D &d,const C &control){
    assert(d.groups.size()==control.groups.size());
    for(std::size_t i=0;i<d.groups.size();i++){
        const auto &a=d.groups[i],&b=control.groups[i];assert(d.observed[i]==control.observed[i]&&a.selected==b.selected);
        same_bytes(a.visible,b.visible);same_bytes(a.uploaded,b.uploaded);same_bytes(a.visible_fades,b.visible_fades);same_bytes(a.uploaded_fades,b.uploaded_fades);
        same_asset(a.asset,b.asset);same_asset(a.shadow_asset,b.shadow_asset);same_asset(a.overflow_asset,b.overflow_asset);
    }
    for(unsigned i=0;i<4;i++){same_bytes(d.reflection_instances[i],control.reflection_instances[i]);same_bytes(d.reflection_uploaded[i],control.reflection_uploaded[i]);same_asset(d.reflection_assets[i],control.reflection_assets[i]);}
    same_bytes(d.grass.procedural_keep,control.grass.procedural_keep);same_bytes(d.grass.staging,control.grass.staging);
    assert(d.grass.blades==control.grass.blades&&d.grass.x==control.grass.x&&d.grass.z==control.grass.z);
    same_bytes(buffers[d.grass.instances].bytes,buffers[control.grass.instances].bytes);
    assert(buffers[d.grass.instances].uploads==buffers[control.grass.instances].uploads);
    assert(d.scanned_culled_frustum==control.scanned_culled_frustum&&d.scanned_culled_wall==control.scanned_culled_wall&&d.scanned_grass_count==control.scanned_grass_count);
    assert(d.tree_count==control.tree_count&&d.near_tree_count==control.near_tree_count&&d.mid_tree_count==control.mid_tree_count&&d.far_tree_count==control.far_tree_count);
    assert(d.peak_scanned_group_candidates==control.peak_scanned_group_candidates&&d.scanned_grass_cap_drops==control.scanned_grass_cap_drops);
    const auto &f=d.fields[WORLD_COVER_GRASS];for(std::size_t i=0;i<f.count;i++)assert(d.grass.procedural_keep[i]==!(std::sin(f.instances[i].yaw*733.1f+f.instances[i].scale*91.7f)>0));
}
static void revision_contract(){
    WorldGrassTransition t;std::vector<WorldModelInstance> a={{0,1,0,.1f,1},{1,1,0,.2f,1}},b={{0,1,0,.1f,1},{2,1,0,.3f,1}};
    WorldCoverField f={WORLD_COVER_GRASS,1,0,0,a.size(),a.data()};auto revision=t.structure_revision();assert(t.replace(f)&&t.structure_revision()==revision+1);
    revision=t.structure_revision();assert(!t.advance(.1)&&t.structure_revision()==revision);f.instances=b.data();assert(t.replace(f)&&t.structure_revision()==revision+1);
    revision=t.structure_revision();assert(t.advance(.2)&&t.structure_revision()==revision);assert(t.advance(.6)&&t.structure_revision()==revision+1);
    revision=t.structure_revision();f.anchor_x=NAN;assert(!t.replace(f)&&t.structure_revision()==revision);assert(!t.advance(-1)&&t.structure_revision()==revision);
    t.reset();assert(t.structure_revision()==revision+1);f.anchor_x=0;assert(t.replace(f));revision=t.structure_revision();
    std::reverse(b.begin(),b.end());assert(t.replace(f)&&t.structure_revision()==revision+1); // same count/generation, different source order
}
int main(int argc,char **argv){
    revision_contract();assert(world_atlas_init());Dressing fast;LegacyDressing legacy;RevisionDressing revision;DispatchDressing dispatch;
    assert(fast.init()&&legacy.init()&&revision.init()&&dispatch.init());auto all=std::tie(fast,legacy,revision,dispatch);
    auto each=[&](auto action){std::apply([&](auto &...d){(action(d),...);},all);};
    auto equal=[&](){compare(fast,legacy);compare(revision,legacy);compare(dispatch,legacy);};
    std::array<std::size_t,21> original_caps;for(std::size_t i=0;i<original_caps.size();i++)original_caps[i]=fast.groups[i].cap;
    std::size_t checked=0,wall_rejections=0;
    auto frame=[&](Camera cam,bool changed=true){track=true;allocations=0;each([&](auto &d){d.select(cam,true,changed);});track=false;assert(allocations==0);equal();checked++;wall_rejections+=fast.scanned_culled_wall;};
    track=true;allocations=0;void *positive=::operator new(17);track=false;assert(allocations==1);::operator delete(positive);
    const auto *pool=world_atlas_rainforest_pool();double vx,vz;world_atlas_village_origin(&vx,&vz);
    const std::array<std::array<double,2>,2> regions={{{pool->x+7,pool->z+120},{vx+80,vz-80}}};
    Camera last;
    for(const auto &region:regions){
        auto p=publish(region[0],region[1]);each([&](auto &d){install(d,p);});
        for(unsigned i=0;i<72;i++){
            Camera cam;cam.x=region[0]+(i%3)*.37;cam.z=region[1]-(i%7)*13.;cam.y=world_atlas_height(cam.x,cam.z)+1.75+(i%5==0?40:0);
            cam.yaw=float(i)*.1745329252f;cam.pitch=i%3==0?-.45f:i%3==1?.08f:1.2f;cam.aspect=i%4==0?.6f:i%4==1?3.2f:16.f/9;
            frame(cam);last=cam;
        }
        auto next=publish(region[0]+24,region[1]);each([&](auto &d){install(d,next);});
        const auto initial_fast=fast.grass.classification_refreshes,initial_legacy=legacy.grass.classification_refreshes;
        for(unsigned i=0;i<50;i++){
            each([&](auto &d){const bool changed=d.grass_transition.advance(1./60.);if(changed)d.fields[WORLD_COVER_GRASS]=d.grass_transition.field();});
            last.yaw+=.001f;frame(last);
        }
        assert(fast.grass.classification_refreshes-initial_fast<legacy.grass.classification_refreshes-initial_legacy);
        // Failed replacement and reordered same-generation replacement preserve
        // state or invalidate classification respectively, even at equal count.
        auto bad=next[WORLD_COVER_GRASS].get();bad.anchor_x=NAN;each([&](auto &d){const auto r=d.grass_transition.structure_revision();assert(!d.grass_transition.replace(bad)&&d.grass_transition.structure_revision()==r);});frame(last);
        std::reverse(next[WORLD_COVER_GRASS].roots.begin(),next[WORLD_COVER_GRASS].roots.end());each([&](auto &d){install(d,next);});frame(last);
        // Force real cap overflow/ties, unusual group fallback, failed upload
        // rollback, and loading-only probe payload restoration.
        each([&](auto &d){for(auto &g:d.groups)g.cap=7;});frame(last);
        each([&](auto &d){d.groups[0].lod=2;});frame(last);each([&](auto &d){d.groups[0].lod=0;});
        each([&](auto &d){d.groups.back().variants=13;});frame(last);each([&](auto &d){d.groups.back().variants=12;});
        std::size_t victim=0;while(victim<legacy.groups.size()&&legacy.groups[victim].visible.empty())victim++;assert(victim<legacy.groups.size());
        each([&](auto &d){d.groups[victim].uploaded.clear();d.groups[victim].asset->failing=true;});frame(last);
        each([&](auto &d){assert(!d.groups[victim].asset->failing);});
        each([&](auto &d){assert(d.prepare_probe_trees());});equal();each([&](auto &d){assert(d.restore_probe_trees());});equal();frame(last);
        each([&](auto &d){d.grass_transition.reset();assert(d.grass_transition.replace(next[WORLD_COVER_GRASS].get()));d.fields[WORLD_COVER_GRASS]=d.grass_transition.field();});frame(last);
        std::printf("PASS region %.1f/%.1f: grass%zu/tree%zu real roots, camera/fade/reorder/failure/reset/probe cases\n",region[0],region[1],p[WORLD_COVER_GRASS].roots.size(),p[WORLD_COVER_TREES].roots.size());
    }
    // Synthetic exact interval endpoints and their adjacent float values also
    // create repeated squared distances, with every group forced over its cap.
    Published boundaries;const float distances[]={0,62,70,82,215,265,1680};
    for(unsigned kind=0;kind<WORLD_COVER_KIND_COUNT;kind++){
        auto &p=boundaries[kind];p.field={WorldCoverKind(kind),200,regions[0][0],regions[0][1],0,nullptr};
        for(unsigned i=0;i<(kind==WORLD_COVER_TREES?7000u:kind==WORLD_COVER_GRASS?2048u:128u);i++){
            float distance=distances[i%std::size(distances)];if(i%3==1)distance=std::nextafter(distance,0.f);if(i%3==2)distance=std::nextafter(distance,INFINITY);
            p.roots.push_back({i%2?distance:-distance,134.f+(i%5)*.2f,0,float(i)*.00131f+.00001f,1.f});
        }
    }
    each([&](auto &d){install(d,boundaries);});
    for(unsigned i=0;i<24;i++){Camera cam;cam.x=regions[0][0];cam.z=regions[0][1];cam.y=135.75;cam.yaw=float(i)*.2617993878f;cam.pitch=i%3==0?-.6f:i%3==1?0.f:.4f;frame(cam);}
    assert(wall_rejections>0&&fast.scanned_grass_cap_drops>0);
    std::printf("PASS actual Dressing:%zu full-source cases, all uncapped pairs/order, capped pairs/order, root/opacity/overflow/shadow/reflection/upload bytes/counts; zero guarded allocations. Classification refreshes legacy%llu/revision%llu/dispatch%llu/combined%llu; seeds legacy%llu/combined%llu\n",checked,(unsigned long long)legacy.grass.classification_refreshes,(unsigned long long)revision.grass.classification_refreshes,(unsigned long long)dispatch.grass.classification_refreshes,(unsigned long long)fast.grass.classification_refreshes,(unsigned long long)legacy.grass.classified_seeds,(unsigned long long)fast.grass.classified_seeds);
    if(argc>1&&!std::strcmp(argv[1],"--benchmark")){
        auto before=publish(regions[0][0],regions[0][1]),after=publish(regions[0][0]+24,regions[0][1]);
        each([&](auto &d){install(d,before);install(d,after);assert(d.grass_transition.advance(.3));d.fields[WORLD_COVER_GRASS]=d.grass_transition.field();for(std::size_t i=0;i<original_caps.size();i++)d.groups[i].cap=original_caps[i];});
        last.x=regions[0][0]+24;last.z=regions[0][1];last.y=world_atlas_height(last.x,last.z)+1.75;frame(last);
        capture_candidates=false;std::array<Camera,64> views;for(std::size_t i=0;i<views.size();i++){views[i]=last;views[i].yaw=float(i)*.0981747704f;views[i].pitch=i%2?.08f:-.3f;}
        auto time=[&](auto &d){const auto begin=std::chrono::steady_clock::now();for(const auto &cam:views)d.select(cam,true,true,false);return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();};
        std::array<double,4> elapsed={};for(unsigned repeat=0;repeat<12;repeat++)if(repeat%2){elapsed[3]+=time(fast);elapsed[2]+=time(dispatch);elapsed[1]+=time(revision);elapsed[0]+=time(legacy);}else{elapsed[0]+=time(legacy);elapsed[1]+=time(revision);elapsed[2]+=time(dispatch);elapsed[3]+=time(fast);}
        std::printf("BENCH Dressing CPU%zu grass/%zu trees, original caps, opacity-only snapshot,64views x12: legacy%.6f/revision%.6f/dispatch%.6f/combined%.6f ms per select; inertGL stubs, candidate observer disabled, no nativeFPS claim\n",fast.fields[WORLD_COVER_GRASS].count,fast.fields[WORLD_COVER_TREES].count,elapsed[0]/768,elapsed[1]/768,elapsed[2]/768,elapsed[3]/768);
    }
    each([](auto &d){d.release();});world_atlas_shutdown();
}

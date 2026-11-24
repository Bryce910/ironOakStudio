/* Differential CPU fixtures compile actual TerrainGPU/HorizonForest source.
 * No OpenGL driver, worker thread, renderer or image-quality shortcut is used. */
#include "engine/world_tiles.h"
#include "engine/world_atlas.h"
#include "engine/world_models.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>
static bool track=false;static size_t allocations=0;
[[gnu::noinline]] void *operator new(size_t n){if(track)++allocations;if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}[[gnu::noinline]] void operator delete(void *p,size_t)noexcept{std::free(p);}
using GLuint=unsigned;constexpr unsigned GL_TRIANGLES=1,GL_UNSIGNED_INT=2;
void glBindVertexArray(unsigned){}void glDrawElements(unsigned,unsigned,unsigned,const void*){}
struct V3{float x,y,z;};struct M4{};
struct Camera{double x=0,y=25,z=0;float aspect=16.f/9,yaw=0,pitch=0;
 V3 forward()const{return {std::sin(yaw),0,-std::cos(yaw)};}V3 right()const{return {std::cos(yaw),0,std::sin(yaw)};}};
struct Program{unsigned id=1;void use(){}void m4(const char*,M4){}void v3(const char*,V3){}void v2(const char*,float,float){}void i(const char*,int){}void f(const char*,float){}};
struct GpuTile{unsigned vao=0,count=0;uint64_t generation=0;WorldTileKey key={};double x=0,z=0;float size=128;size_t bytes=0;
 void upload(const WorldTileMesh&m){vao=1;generation=m.generation;key=m.key;}void release(){}};
float sample_height(double,double,void*){return 0;}
bool keyeq(WorldTileKey a,WorldTileKey b){return a.x==b.x&&a.z==b.z&&a.lod==b.lod;}
struct WorldTilesWorld{std::vector<WorldTileRequest> desired;std::vector<WorldTileMesh> completed;size_t cursor=0;};
WorldTilesWorld *world_tiles_init(const WorldTilesConfig*){auto*w=new WorldTilesWorld;w->desired.reserve(256);w->completed.reserve(256);return w;}
bool world_tiles_update(WorldTilesWorld*,double,double){return true;}
size_t world_tiles_desired(WorldTilesWorld*w,WorldTileRequest*out,size_t cap){assert(w->desired.size()<=cap);std::copy(w->desired.begin(),w->desired.end(),out);return w->desired.size();}
bool world_tiles_poll_completed(WorldTilesWorld*w,WorldTileMesh*out){if(w->cursor==w->completed.size())return false;*out=w->completed[w->cursor++];return true;}
bool world_tiles_is_current(WorldTilesWorld*w,unsigned slot,uint64_t generation){for(auto&r:w->desired)if(r.slot==slot&&r.generation==generation)return true;return false;}
void world_tiles_release(WorldTileMesh*){}void world_tiles_shutdown(WorldTilesWorld*w){delete w;}
struct WorldModelAsset{size_t count=0;};WorldModelAsset*world_model_load(const char*){return new WorldModelAsset;}
void world_model_free(WorldModelAsset*p){delete p;}bool world_model_upload_instances(WorldModelAsset*p,const WorldModelInstance*,size_t n){p->count=n;return true;}
void world_model_draw(WorldModelAsset*,unsigned){}
WorldAtlasSample world_atlas_sample(double,double){return {};}
WorldAtlasLandscapeSample world_atlas_landscape_mask(double,double){return {};}
bool world_atlas_local_water(double,double,float*,float*){return false;}
const WorldAtlasPool*world_atlas_rainforest_pool(){static WorldAtlasPool p={};return &p;}
#include "viewer_cache_classes.inc"
static WorldTileMesh mesh(WorldTileRequest r){WorldTileMesh m={};m.key=r.key;m.slot=r.slot;m.generation=r.generation;return m;}
static void terrain_tests(){
 TerrainGPU cached;LegacyTerrainGPU legacy;assert(cached.init()&&legacy.init());
 std::vector<WorldTileRequest> records={{{0,0,1},0,1,false,false}};
 for(int z=0;z<2;z++)for(int x=0;x<2;x++)records.push_back({{x,z,0},unsigned(records.size()),1,true,false});
 auto frame=[&](const std::vector<WorldTileMesh>&uploads,bool expect_rebuild){
  for(auto*w:{cached.stream,legacy.stream}){w->desired=records;w->completed=uploads;w->cursor=0;}
  cached.hierarchy_visits=legacy.hierarchy_visits=0;cached.hierarchy_builds=legacy.hierarchy_builds=0;cached.update(0,0,256);legacy.update(0,0,256);
  assert(cached.draws==legacy.draws);assert((cached.hierarchy_builds>0)==expect_rebuild);
 };
 frame({mesh(records[0])},true);assert(cached.draws.size()==1&&cached.draws[0]==0);
 for(size_t i=1;i<records.size();i++)frame({mesh(records[i])},true);
 assert(cached.draws.size()==4);
 for(auto&w:records){w.generated=true;}frame({},true);
 track=true;allocations=0;
 for(int i=0;i<10000;i++){cached.hierarchy_visits=legacy.hierarchy_visits=0;cached.hierarchy_builds=legacy.hierarchy_builds=0;cached.update(0,0,0);legacy.update(0,0,0);assert(cached.draws==legacy.draws&&cached.hierarchy_visits==0&&legacy.hierarchy_visits>0);}
 track=false;assert(allocations==0);
 frame({mesh(records[1])},true); // Same-generation upload still invalidates.
 auto stale=mesh(records[1]);stale.generation++;frame({stale},false);
 records[0].leaf=true;frame({},true);assert(cached.draws==std::vector<unsigned>{0});
 records[0].leaf=false;frame({},true);
 records[1].generation++;frame({},true);assert(cached.draws==std::vector<unsigned>{0});
 frame({mesh(records[1])},true);assert(cached.draws.size()==4);
 records[2].slot=42;frame({},true);frame({mesh(records[2])},true);
 records[3].key.x=7;frame({},true);records[3].key.x=0;frame({},true);
 records[3].key.z=7;frame({},true);records[3].key.z=1;frame({},true);
 records[3].key.lod=2;frame({},true);records[3].key.lod=0;frame({},true);
 std::reverse(records.begin(),records.end());frame({},true);
 records.pop_back();frame({},true);records.clear();frame({},true);frame({},false);
 cached.release();legacy.release();std::puts("PASS terrain cache: actual-source differential draw order across deferred/stale/same-generation uploads, all metadata changes, reorder/removal and10000 unchanged frames; zero hot allocations");
}
static void horizon_tests(){
 HorizonForest h;assert(h.init());Camera cam;
 const double anchors[][2]={{0,0},{-4096,2048},{10240,-8192}};
 for(auto&anchor:anchors){
  cam.x=anchor[0];cam.z=anchor[1];h.target_x=cam.x;h.target_z=cam.z;
  h.pending.clear();
  for(size_t i=0;i<h.root_cap;i++){
   float x=float((int(i%400)-200)*48+.249999999),z=float((int(i/400)-125)*48-.249999999);
   if(i<8){x=std::nextafter(.25f,i%2?1.f:-1.f);z=std::nextafter(-.25f,i%2?1.f:-1.f);}
   h.pending.push_back({uint64_t(i),{x,50,z,0,1}});
  }
  h.building=true;h.cx=h.max_x=0;h.cz=h.max_z=0;h.shoot=1;
  track=true;allocations=0;h.update(cam,1);track=false;
  assert(allocations==0&&h.live.size()==h.root_cap&&h.live_bucket.size()==h.live.size());
  for(size_t i=0;i<h.live.size();i++){
   auto&a=h.live[i];auto expected=HorizonForest::hash(int64_t(std::floor((h.live_x+a.x)*4)),int64_t(std::floor((h.live_z+a.z)*4)),1973)%8;
   assert(h.live_bucket[i]==expected);
  }
  auto bytes=h.live_bucket;auto roots=h.live;cam.x+=1024;h.update(cam,0);
  assert(h.building&&h.live_bucket==bytes&&h.live.size()==roots.size()&&!std::memcmp(h.live.data(),roots.data(),roots.size()*sizeof(WorldModelInstance)));
  cam.x+=1024;h.update(cam,0);assert(h.live_bucket==bytes);
 }
 h.release();std::puts("PASS horizon cache:300000 actual published roots match legacy hash at positive/negative anchors and quarter-metre boundaries; pending/aborted replacements preserve old roots; zero publication allocations");
}
int main(){terrain_tests();horizon_tests();}

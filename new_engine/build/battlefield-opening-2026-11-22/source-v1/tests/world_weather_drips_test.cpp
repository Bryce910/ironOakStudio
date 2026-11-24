// Standalone CPU test supplies the already-linked third-party implementations.
// Production world_weather_drips.inc contains declarations only.
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#undef CGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#include "engine/world_weather_drips.inc"
#include <iostream>
#include <vector>
#include <cstdlib>
#include <new>
static unsigned long allocations=0;
void*operator new(std::size_t n){++allocations;if(auto*p=std::malloc(n))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}
struct WorldScenery{std::vector<WorldSceneryPlacement>placements;std::array<bool,2>active{true,true};std::uint64_t revision=0;};
static std::array<WorldSceneryAlias,2>aliases={WorldSceneryAlias{},WorldSceneryAlias{}};
std::size_t world_scenery_alias_count(){return aliases.size();}
WorldSceneryAlias world_scenery_alias(std::size_t i){return aliases[i];}
WorldSceneryStats world_scenery_stats(const WorldScenery*s){WorldSceneryStats r;r.source_instances=s?s->placements.size():0;return r;}
bool world_scenery_placement(const WorldScenery*s,std::size_t i,WorldSceneryPlacement*out){if(!s||i>=s->placements.size())return false;*out=s->placements[i];return true;}
bool world_scenery_placement_active(const WorldScenery*s,std::size_t i)noexcept{return s&&i<s->placements.size()&&i<s->active.size()&&s->active[i];}
std::uint64_t world_scenery_activity_revision(const WorldScenery*s)noexcept{return s?s->revision:0;}
static unsigned checks=0;
void check(bool b,const char*l){++checks;if(!b){std::cerr<<"FAIL "<<l<<'\n';std::exit(1);}}
int main(int argc,char**argv){
 namespace drips=world_weather_drips;check(argc==2,"fixture models path");aliases[0].name="tree";aliases[0].stem="testtree";aliases[0].tree=true;aliases[1].name="house_roof";aliases[1].stem="testroof";
 WorldScenery s;WorldSceneryPlacement tree;tree.alias="tree";tree.tree=true;tree.x=2;tree.z=1;tree.y=10;tree.scale=2;tree.yaw=.7;tree.pitch=.1;tree.roll=-.2;tree.sx=.8;tree.sy=1.5;tree.sz=1.2;s.placements.push_back(tree);WorldSceneryPlacement roof;roof.alias="house_roof";roof.foundation=true;roof.x=3;roof.z=2;s.placements.push_back(roof);
 drips::Cache cache;auto ground=[](double,double,void*){return 0.f;};check(drips::prepare(cache,&s,argv[1],0,0,ground,nullptr),"parse actual synthetic GLBs and embedded PNG");check(cache.asset_count==2&&cache.stats.assets==2,"two actual supported assets");check(cache.assets[0].count==1,"transparent leaf omitted, opaque leaf retained");check(cache.stats.alpha_rejected==1,"true atlas alpha footprint tested");const auto&p=cache.assets[0].points[0].p;check(std::abs(p[0]-2.31)<1.e-6&&std::abs(p[1]-4)<1.e-6&&std::abs(p[2]-1.46)<1.e-6,"node transforms plus global model minY normalization match renderer");check(cache.assets[1].count==4,"roof quad has four boundary eaves, shared diagonal omitted");for(unsigned i=0;i<4;++i){const auto&q=cache.assets[1].points[i];check(q.kind==1&&std::abs(q.p[1]-3)<1.e-6,"roof edge is actual triangle plane");check(std::abs(q.p[0]-2)>.9||std::abs(q.p[2]-1)>.9,"anchor is outer eave, not internal diagonal");}
 check(cache.count==5&&cache.stats.leaf_anchors==1&&cache.stats.roof_anchors==4,"true leaf and roof anchors exposed");auto transform=vw_transform::matrix({tree.x,tree.y,tree.z,tree.yaw*57.29577951308232,tree.scale,tree.pitch*57.29577951308232,tree.roll*57.29577951308232,tree.sx,tree.sy,tree.sz});const auto expected=vw_transform::point(transform,p);bool found=false;for(unsigned i=0;i<cache.count;++i)if(cache.points[i].kind==0){const auto&v=cache.points[i];found=true;check(std::abs(v.x-expected[0])<1.e-6&&std::abs(v.y-(expected[1]-.012))<1.e-6&&std::abs(v.z-expected[2])<1.e-6,"exact yaw/pitch/roll/nonuniform instance transform");}check(found,"leaf world anchor present");
 drips::Asset blocked;blocked.tree=true;blocked.stem="testblocked";std::uint64_t budget=256ull*1024*1024;drips::Stats blocked_stats;check(!drips::load_asset(blocked,std::string(argv[1])+"/testblocked_near.glb",budget,blocked_stats)&&blocked_stats.occluded==1,"real opaque overhead triangle suppresses hidden source");budget=1;drips::Asset denied;check(!drips::load_asset(denied,std::string(argv[1])+"/testroof.glb",budget,blocked_stats),"encoded byte budget enforced before parse");
 {std::uint64_t remaining=64ull*1024*1024;drips::Stats stats;drips::GeometryReader reader(remaining,stats);cgltf_data*raw=nullptr;check(reader.parse(std::string(argv[1])+"/testroof.glb",raw),"bounded GLB JSON/BIN parser");std::unique_ptr<cgltf_data,void(*)(cgltf_data*)>data(raw,cgltf_free);const auto consumed=reader.used;std::uint32_t byte=0;check(!reader.read(UINT64_MAX,sizeof(byte),&byte)&&reader.used==consumed,"absolute read overflow rejected without IO");cgltf_buffer_view view{};view.buffer=&data->buffers[0];view.offset=UINT64_MAX;view.size=4;check(!reader.view(&view)&&view.data==nullptr,"BIN view offset overflow rejected before allocation");view.offset=0;view.size=UINT64_MAX;check(!reader.view(&view)&&view.data==nullptr,"BIN view length overflow rejected before allocation");}
 s.active[1]=false;++s.revision;drips::refresh(cache,&s,0,0,ground,nullptr);check(cache.count==1&&cache.stats.roof_anchors==0,"stationary runtime-hide invalidates cached sources by revision");drips::Cache inactive;check(drips::prepare(inactive,&s,argv[1],0,0,ground,nullptr)&&inactive.count==1&&inactive.stats.roof_anchors==0,"inactive roof omitted from startup preparation");s.active[1]=true;++s.revision;drips::refresh(cache,&s,0,0,ground,nullptr);check(cache.stats.roof_anchors==4,"stationary reactivation restores true roof sources");
 const auto before=allocations;const auto refreshes=cache.stats.refreshes;for(unsigned i=0;i<10000;++i)drips::refresh(cache,&s,0,0,ground,nullptr);check(cache.stats.refreshes==refreshes,"sub8m movement skips reselection");for(unsigned i=0;i<100;++i)drips::refresh(cache,&s,0,0,ground,nullptr,true);check(allocations==before,"refresh and selection use no heap allocations");check(cache.count<=64,"fixed GPU anchor cap");
 drips::refresh(cache,&s,1000,1000,ground,nullptr);check(cache.count==0,"remote sources omitted");drips::refresh(cache,&s,0,0,[](double,double,void*){return 100.f;},nullptr,true);check(cache.count==0,"below-ground points cannot become floating emitters");std::cout<<"PASS "<<checks<<" actual GLB/PNG alpha/eave/world-transform/noallocation checks\n";
}

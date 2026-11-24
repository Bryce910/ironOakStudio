#include "engine/world_pond_field.h"
#include "engine/world_surface_field.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
unsigned checks=0;
void check(bool ok,const char *message){++checks;if(!ok)throw std::runtime_error(message);}
float flat(double,double){return 30;}
float slope(double x,double z){return float(30+x*.08-z*.04);}
float invalid(double,double){return std::numeric_limits<float>::quiet_NaN();}
bool protected_root(double x,double z,const void *){return std::hypot(x-256,z-128)<2;}
using Document=world_studio::Document;
using Source=world_terrain_authoring::Source;
using Report=world_terrain_authoring::Report;
bool terrain_equal(const Document &a,const Document &b){
    if(a.terrain.size()!=b.terrain.size())return false;
    for(unsigned i=0;i<a.terrain.size();++i){const auto &x=a.terrain[i],&y=b.terrain[i];if(x.x!=y.x||x.z!=y.z||x.delta!=y.delta)return false;}
    return true;
}
std::string bytes(const Document &d){std::string out,error;check(world_studio::encode(d,out,error),"encode fixture");return out;}
double height(const Document &d,float(*base)(double,double),double x,double z){
    return double(base(x,z))+world_studio::height_delta(d,x,z)+(d.surface_field?world_surface::terrain_offset(*d.surface_field,x,z):0);
}
void enclosed(const Document &d,unsigned index,float(*base)(double,double)){
    const auto &w=d.waters[index];check(w.level-height(d,base,w.x,w.z)>=.08,"center submerged after merged bilinear field");
    for(unsigned i=0;i<2048;++i){const double a=i*6.283185307179586/2048;
        check(height(d,base,w.x+std::cos(a)*w.radius_x,w.z+std::sin(a)*w.radius_z)>=w.level+.035,"entire rendered cutoff has dry rim");}
}
void restored(const Document &a,const Document &b,float(*base)(double,double),double cx,double cz,float radius){
    for(double z=cz-radius;z<=cz+radius;z+=1.3)for(double x=cx-radius;x<=cx+radius;x+=1.1)
        check(std::abs(height(a,base,x,z)-height(b,base,x,z))<1e-5,"deleted or moved pond restores exact original heights");
}
}
int main(){try{
    std::string error;Report report;Document base;
    check(world_terrain_authoring::simple_brush(base,128,128,48,2,world_terrain_authoring::SimpleBrushMode::Raise,30,71,Source{flat},report,error),"base sculpt fixture");
    const auto original=bytes(base);auto pond=base;
    check(world_pond_field::create(pond,127.3,128.8,12,2,Source{flat},report,error),"create derived pond on existing sculpt");
    check(terrain_equal(pond,base),"derived creation leaves sculpt byte-exact");
    check(pond.waters.size()==1&&pond.waters[0].basin_depth==2&&pond.surface_field,"basin recipe and derived field retained");enclosed(pond,0,flat);
    const auto created=pond;const auto old_level=pond.waters[0].level;
    pond.waters[0].tint={.2f,.8f,1.2f};pond.waters[0].waves=.3f;pond.waters[0].foam=1.4f;
    pond.waters[0].x=256.3;pond.waters[0].z=128.6;
    check(world_pond_field::prepare(pond,Source{flat},error),"move derived pond");
    check(terrain_equal(pond,base)&&pond.waters[0].tint[0]==.2f&&pond.waters[0].waves==.3f&&pond.waters[0].foam==1.4f,"move preserves sculpt and appearance");
    check(pond.waters[0].level<old_level-.5,"move recomputes level from new uncarved ground");
    restored(pond,base,flat,127.3,128.8,18);enclosed(pond,0,flat);
    const double old_outside=height(pond,flat,270.3,128.6);
    pond.waters[0].radius_x=pond.waters[0].radius_z=24;
    check(world_pond_field::prepare(pond,Source{flat},error),"enlarge derived basin");
    check(height(pond,flat,270.3,128.6)<old_outside-.1,"larger radius actually expands the submerged bowl");enclosed(pond,0,flat);
    pond.waters[0].radius_x=pond.waters[0].radius_z=6;
    check(world_pond_field::prepare(pond,Source{flat},error),"shrink derived basin");
    check(std::abs(height(pond,flat,270.3,128.6)-height(base,flat,270.3,128.6))<1e-5,"shrinking restores former outer basin");enclosed(pond,0,flat);
    pond.waters[0].enabled=false;
    const auto disabled_level=pond.waters[0].level;
    check(world_pond_field::prepare(pond,Source{flat},error),"disable restores basin ground");
    check(!pond.surface_field&&pond.waters[0].level==disabled_level,"disabled recipe keeps style/level without carving");restored(pond,base,flat,256.3,128.6,14);
    pond.waters[0].enabled=true;
    check(world_pond_field::prepare(pond,Source{flat},error),"re-enable reconstructs basin");enclosed(pond,0,flat);
    check(world_pond_field::create(pond,512.4,128.7,8,1.5,Source{flat},report,error),"independent second derived pond");
    const auto second=pond.waters[1];const auto with_two=pond;
    pond.waters.erase(pond.waters.begin());
    check(world_pond_field::prepare(pond,Source{flat},error),"remove first pond");
    restored(pond,base,flat,256.3,128.6,14);enclosed(pond,0,flat);
    check(pond.waters[0].x==second.x&&pond.waters[0].z==second.z&&pond.waters[0].level==second.level&&pond.waters[0].tint==second.tint,"independent water settings preserved");
    for(double z=118;z<140;z+=.7)for(double x=500;x<525;x+=.7)
        check(height(pond,flat,x,z)==height(with_two,flat,x,z),"independent basin geometry unchanged");
    pond.waters.clear();check(world_pond_field::prepare(pond,Source{flat},error),"remove final derived pond");
    check(!pond.surface_field&&bytes(pond)==original,"all pond removal restores original document exactly");

    // Legacy basins stay in the durable sculpt and their water/style remains
    // untouched while new recipes can be created, moved and removed nearby.
    Document legacy;
    check(world_terrain_authoring::simple_pond(legacy,700,128,12,2,Source{flat},report,error),"legacy baked pond fixture");
    const auto legacy_bytes=bytes(legacy);const auto legacy_ground=legacy;
    check(world_pond_field::create(legacy,900,128,12,2,Source{flat},report,error),"new pond beside a legacy basin");
    check(terrain_equal(legacy,legacy_ground)&&legacy.waters[0].basin_depth==0,"legacy sculpt and zero-depth marker preserved");
    legacy.waters.pop_back();check(world_pond_field::prepare(legacy,Source{flat},error),"delete new pond beside legacy");
    check(bytes(legacy)==legacy_bytes,"legacy scene restores byte-exact after derived deletion");enclosed(legacy,0,flat);

    Document paths;world_surface::Ribbon road;road.kind=world_surface::RibbonKind::Road;
    road.width=6;road.shoulder=3;road.points={{384,32,512},{480,32,512}};paths.surfaces.ribbons.push_back(road);
    auto river=road;river.name="Stream";river.kind=world_surface::RibbonKind::River;
    river.points={{384,30,640},{480,29,640}};river.depth=2;paths.surfaces.ribbons.push_back(river);
    check(world_pond_field::prepare(paths,Source{flat},error),"prepare road and river without ponds");
    const auto road_only=paths;const double road_height=height(paths,flat,432,512);
    const float road_mask=world_surface::road_mask(*paths.surface_field,432,512);
    const double river_bed=height(paths,flat,432,640);
    float water_level=0,shore=0;
    check(world_surface::river(*paths.surface_field,432,640,water_level,shore),"river metadata fixture");
    const auto river_level=water_level;
    check(world_pond_field::create(paths,128,128,24,3,Source{flat},report,error),"derived pond coexists with road/river field");
    for(const auto &old:road_only.surface_field->tiles){
        const auto found=std::find_if(paths.surface_field->tiles.begin(),paths.surface_field->tiles.end(),[&](const auto &t){return t.x==old.x&&t.z==old.z;});
        check(found!=paths.surface_field->tiles.end()&&found->delta==old.delta&&found->road==old.road&&found->ribbons==old.ribbons&&found->count==old.count,"complete unrelated road/river tile contents unchanged");
    }
    check(height(paths,flat,432,512)==road_height&&world_surface::road_mask(*paths.surface_field,432,512)==road_mask,"unrelated road grade and cover mask preserved");
    check(height(paths,flat,432,640)==river_bed&&world_surface::river(*paths.surface_field,432,640,water_level,shore)&&water_level==river_level,"unrelated river bed and water metadata preserved");
    paths.waters.clear();check(world_pond_field::prepare(paths,Source{flat},error),"remove pond from combined field");
    restored(paths,road_only,flat,128,128,34);
    check(paths.surface_field->ribbons.size()==2&&height(paths,flat,432,512)==road_height,"road and river survive final pond deletion");

    // Water bindings retain their published indices despite a different
    // temporary replay list. The graph is never copied back from that list.
    auto entities=created;
    world_entity::Node node;node.id=1;node.name="Pond";node.binding={world_entity::Kind::Water,0};
    entities.entities.next_id=2;entities.entities.nodes.push_back(node);
    const auto graph_before=bytes(entities);
    check(world_pond_field::prepare(entities,Source{flat},error)&&bytes(entities)==graph_before,"derived preparation preserves water entity bindings exactly");

    // Each failure is atomic, including field pointer identity and outputs
    // that a previous successful recipe has already prepared.
    auto guarded=created;const auto guarded_bytes=bytes(guarded);const auto field=guarded.surface_field;
    Report sentinel;sentinel.changed_nodes=123;
    check(!world_pond_field::create(guarded,256,128,12,2,Source{flat,protected_root},sentinel,error),"static foundation protection rejects basin");
    check(bytes(guarded)==guarded_bytes&&guarded.surface_field==field&&sentinel.changed_nodes==123,"failed creation preserves document field and report");
    check(!world_pond_field::create(guarded,128,128,12,2,Source{flat},sentinel,error),"overlapping derived ponds rejected");
    check(bytes(guarded)==guarded_bytes&&guarded.surface_field==field,"overlap failure preserves previous basin");
    check(!world_pond_field::prepare(guarded,Source{invalid},error)&&bytes(guarded)==guarded_bytes&&guarded.surface_field==field,"invalid sampler failure is atomic");
    auto moved_bad=with_two;moved_bad.waters[0].x=moved_bad.waters[1].x;moved_bad.waters[0].z=moved_bad.waters[1].z;
    const auto bad_bytes=bytes(moved_bad);const auto bad_field=moved_bad.surface_field;
    check(!world_pond_field::prepare(moved_bad,Source{flat},error)&&bytes(moved_bad)==bad_bytes&&moved_bad.surface_field==bad_field,"failed overlapping move preserves draft and prior field");
    auto road_overlap=road_only;world_studio::Water overlapping;overlapping.source=3;overlapping.x=432;overlapping.z=512;overlapping.level=30;overlapping.radius_x=overlapping.radius_z=12;overlapping.basin_depth=2;road_overlap.waters.push_back(overlapping);
    const auto overlap_bytes=bytes(road_overlap);
    check(!world_pond_field::prepare(road_overlap,Source{flat},error)&&bytes(road_overlap)==overlap_bytes,"pond cannot modify road grade or bank");
    Document budget;
    for(unsigned i=0;i<16;++i){
        auto r=road;r.name="Road"+std::to_string(i);r.points={{i*1024.+16,32,64},{i*1024.+208,32,64},{i*1024.+400,32,64}};budget.surfaces.ribbons.push_back(r);
    }
    check(world_pond_field::prepare(budget,Source{flat},error)&&budget.surface_field->tiles.size()==64,"exact derived64tile road budget fixture");
    const auto budget_bytes=bytes(budget);const auto budget_field=budget.surface_field;
    check(!world_pond_field::create(budget,20032,600,12,2,Source{flat},sentinel,error),"combined pond and road tile budget rejects overflow");
    check(bytes(budget)==budget_bytes&&budget.surface_field==budget_field,"tile-budget failure preserves prior roads and document");
    for(float radius:{6.f,24.f,32.f})for(float depth:{.5f,3.f,6.f})for(double phase:{.01,.99,1.51}){
        Document d;check(world_pond_field::create(d,-128+phase,128-phase,radius,depth,Source{slope},report,error),"sloping seam-crossing derived basin");
        check(d.terrain.empty(),"sloping basin does not bake sculpt");enclosed(d,0,slope);
        d.waters.clear();check(world_pond_field::prepare(d,Source{slope},error)&&!d.surface_field,"sloping basin deletion restores base field");
    }
    std::cout<<"Derived pond fields: "<<checks<<" checks passed\n";
}catch(const std::exception &e){std::cerr<<"Derived pond field failed after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}

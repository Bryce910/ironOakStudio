// Differential CPU test against the preserved, previously published field.
// The production projection/codec executes with synthetic source-tree bounds;
// no GPU, GL objects, atlas mutation or canonical project access is involved.
#define WorldForestShelter BaselineWorldForestShelter
#include "../../../vegetation-quality-2026-11-21/source-v1/engine/world_forest_shelter.h"
#undef WorldForestShelter
#undef VALWOOD_WORLD_FOREST_SHELTER_H
#include "engine/world_forest_shelter.h"
#include <cstdio>
#include <cstdlib>
#include <limits>

struct WorldScenery {std::vector<WorldSceneryPlacement> placements;};
WorldSceneryStats world_scenery_stats(const WorldScenery *s){WorldSceneryStats out;if(s)out.source_instances=s->placements.size();return out;}
bool world_scenery_placement(const WorldScenery *s,std::size_t index,WorldSceneryPlacement *out){if(!s||!out||index>=s->placements.size())return false;*out=s->placements[index];return true;}
static unsigned checks=0;static std::size_t pixel_checks=0;
static void check(bool ok,const char *label){++checks;if(!ok){std::fprintf(stderr,"FAIL %s\n",label);std::abort();}}
static float height(double x,double z){return float(7.4+2*std::sin(x*.03)+1.3*std::cos(z*.02));}
static void add_trees(WorldScenery &scene,double cx,double cz){
    for(unsigned i=0;i<11;++i){
        WorldSceneryPlacement p;p.tree=i!=10;p.alias="fixture_crown";p.x=cx+double(i%3)*2-2;p.z=cz+double(i/3)*2-3;
        p.y=height(p.x,p.z);p.yaw=float(i*.39);p.scale=.9f+float(i%3)*.15f;
        p.minimum[0]=-12;p.minimum[1]=0;p.minimum[2]=-10;p.maximum[0]=12;p.maximum[1]=18+float(i);p.maximum[2]=10;
        if(i==6){p.pitch=.12f;p.roll=-.08f;p.sx=1.2f;p.sy=.85f;p.sz=1.1f;}
        if(i==9){p.minimum[0]=p.minimum[2]=-.1f;p.maximum[0]=p.maximum[2]=.1f;}
        scene.placements.push_back(p);
    }
}
template<class A,class B> static void same_geometry_metadata(const A&a,const B&b){
    check(a.bounds==b.bounds,"field domains unchanged");
    check(a.height_min==b.height_min&&a.height_range==b.height_range,"height codec limits unchanged");
    check(a.trees==b.trees&&a.arrival_region==b.arrival_region,"source crown ownership unchanged");
}
static void compare(const WorldScenery &scene,double cx,double cz,unsigned side,bool arrival){
    BaselineWorldForestShelter before;WorldForestShelter after;std::vector<unsigned char> a,b;
    check(before.build(&scene,cx,cz,int(side),a,height,arrival),"baseline builds");
    check(after.build(&scene,cx,cz,int(side),b,height,arrival),"candidate builds");
    same_geometry_metadata(before,after);check(a.size()==b.size()&&a.size()==std::size_t(side)*side*4,"RGBA storage unchanged");
    if(!arrival){check(a==b,"legacy field bytes identical");check(before.minimum_access==after.minimum_access&&before.mean_access==after.mean_access,"legacy statistics identical");}
    bool brighter=false,uncovered=false;unsigned max_delta=0;
    for(std::size_t i=0;i<a.size();i+=4){
        check(a[i+1]==b[i+1]&&a[i+2]==b[i+2]&&a[i+3]==b[i+3],"ground/crown/alpha bytes unchanged");
        check(b[i]>=a[i]&&unsigned(b[i]-a[i])<=21,"sky change bounded to approximately0.08 access");
        if(a[i]==255){check(b[i]==255,"uncovered sky remains unchanged");uncovered=true;}
        brighter=brighter||b[i]>a[i];max_delta=std::max(max_delta,unsigned(b[i]-a[i]));++pixel_checks;
    }
    if(arrival&&side>=129){
        check(brighter&&uncovered,"improves crown shade while preserving clearings");
        check(std::abs(after.minimum_access-.60f)<1e-6f&&std::abs(before.minimum_access-.52f)<1e-6f,"expected dense-crown floors");
        check(after.mean_access>=before.mean_access&&after.mean_access<=1,"mean sky remains bounded");
    }
    std::printf("%s side%u: trees%u min%.6f->%.6f mean%.6f->%.6f height%.1f+%.1f maxbyte+%u\n",arrival?"Arrival":"Legacy",side,after.trees,before.minimum_access,after.minimum_access,before.mean_access,after.mean_access,after.height_min,after.height_range,max_delta);
}
int main(){
    WorldScenery scene;add_trees(scene,-11988,5172);add_trees(scene,4000,-6000);
    for(unsigned side:{2u,7u,129u,513u,1025u}){compare(scene,4000,-6000,side,false);compare(scene,-11988,5172,side,true);}
    BaselineWorldForestShelter old;WorldForestShelter next;std::vector<unsigned char>a,b;
    check(old.build_with_arrival(&scene,4000,-6000,true,-11988,5172,129,a,height),"baseline arrival dispatch");
    check(next.build_with_arrival(&scene,4000,-6000,true,-11988,5172,129,b,height)&&next.arrival_region,"arrival dispatch opts in");same_geometry_metadata(old,next);check(b!=a,"arrival dispatcher receives brighter field");
    check(old.build_with_arrival(&scene,4000,-6000,false,-11988,5172,129,a,height),"baseline missing-arrival dispatch");
    check(next.build_with_arrival(&scene,4000,-6000,false,-11988,5172,129,b,height)&&a==b&&!next.arrival_region,"no arrival retains exact legacy field");
    check(old.build_with_arrival(&scene,4000,-6000,true,20000,20000,129,a,height),"baseline empty-arrival fallback");
    check(next.build_with_arrival(&scene,4000,-6000,true,20000,20000,129,b,height)&&a==b&&!next.arrival_region,"empty arrival retains exact legacy field");
    check(!next.build(nullptr,0,0,129,b,height),"null scene rejected");check(!next.build(&scene,0,0,1,b,height)&&!next.build(&scene,0,0,1026,b,height),"dimensions remain bounded");
    check(!next.build(&scene,0,0,129,b,[](double,double){return std::numeric_limits<float>::quiet_NaN();},true),"nonfinite ground remains rejected");
    std::printf("PASS canopy differential CPU: %u checks, %zu pixels; legacy identical, arrival sky bounded, all geometry/height bytes unchanged\n",checks,pixel_checks);
}

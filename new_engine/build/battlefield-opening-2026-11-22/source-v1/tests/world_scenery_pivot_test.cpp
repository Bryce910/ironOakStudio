// Actual scenery parser/query and transaction writer. Model/GL boundaries use
// deterministic CPU controls; no SDL window or graphics context is created.
#include "tests/world_scenery_pivot_support.inc"
#include "engine/world_scenery_pivot.h"
#include "engine/world_studio_scene_record.h"
#include <iomanip>
#include <iostream>
#include <locale>
namespace {
unsigned checks=0;
void check(bool value,const char *label){if(!value){std::fprintf(stderr,"FAIL %s\n",label);std::abort();}++checks;}
WorldScenery *load(const std::string &path,const std::string &text,bool good=true){
    {std::ofstream f(path);f<<text;check(bool(f),"write private parser fixture");}
    char error[512]={};auto *s=world_scenery_load(path.c_str(),"models",error,sizeof(error));
    if(good&&!s)std::fprintf(stderr,"%s\n",error);
    check(good?s!=nullptr:!s&&std::strstr(error,"dry, finite terrain"),good?"actual loader accepts fixture":"old pivot loss is rejected by actual dry-root parser");return s;
}
std::string encode(const world_studio_scene_record::Record &r){std::ostringstream o;o.imbue(std::locale::classic());world_studio_scene_record::write(o,r);return o.str();}
float raise(double x,double z,const void *context){const auto *point=static_cast<const double *>(context);return std::hypot(x-point[0],z-point[1])<40?2.f:0.f;}
}
int main(int argc,char **argv){
    check(argc==2,"sealed Grandstone project supplied");
    check(world_atlas_init(),"actual atlas initialized");
    std::string project=argv[1];char error[512]={};
    check(world_atlas_load_height_region((project+"/ground/showcase.height").c_str(),error,sizeof(error))==1,"actual Grandstone source height loaded");
    std::ifstream input(project+"/scenery/tanthoon_bandit_camp_instances.txt");std::string line,name;
    while(std::getline(input,line)&&(!line.size()||line[0]=='#')){}
    std::istringstream fields(line);world_studio_scene_record::Record authored;
    fields>>name>>authored.u>>authored.v>>authored.yaw>>authored.scale>>authored.burial>>authored.elevation>>authored.offset_y>>authored.pitch>>authored.roll>>authored.sx>>authored.sy>>authored.sz>>authored.pivot;
    check(bool(fields)&&name=="nm_scene_backdrop_4_0","actual failing first slot4 row parsed");authored.name=name.c_str();
    check(authored.pitch==0&&authored.roll==0&&authored.sx==1&&authored.sy==1&&authored.sz==1,"regression is identity TRS");
    double x=0,z=0;world_atlas_uv_to_world(authored.u,authored.v,&x,&z);const auto ocean=world_atlas_sample(x,z);
    check(ocean.height==-85&&ocean.shore_distance< -1600,"actual backdrop root is below sea on macro atlas");
    const WorldSceneryDefinition definition={name.c_str(),"showcase_source_horizon",1,0,0,2000,35};
    check(world_scenery_configure(&definition,1,error,sizeof(error)),"configure real custom alias registry");
    std::string path="/tmp/valwood-explicit-pivot-"+std::to_string(getpid())+".txt";
    auto *source=load(path,line+'\n');WorldSceneryPlacement pose;
    check(world_scenery_placement(source,0,&pose)&&world_scenery_explicit_pivot(source,0),"query and placement enumerate the same parsed source index");
    check(!world_scenery_explicit_pivot(nullptr,0)&&!world_scenery_explicit_pivot(source,1)&&!world_scenery_explicit_pivot(source,std::numeric_limits<std::size_t>::max()),"null and out-of-range queries are safe");
    check(std::abs(double(pose.y)-1262.74674988)<.0001,"loader retains exact imported float pivot");
    authored.pivot=pose.y;authored.preserve_source_root=false;load(path,encode(authored),false);
    authored.preserve_source_root=world_scenery_explicit_pivot(source,0);auto *fixed=load(path,encode(authored));
    WorldSceneryPlacement fixed_pose;check(world_scenery_placement(fixed,0,&fixed_pose)&&fixed_pose.y==pose.y,"fixed writer plus parser retain absolute identity pivot");
    check(world_scenery_explicit_pivot(fixed,0),"explicit provenance survives transaction reload");world_scenery_free(fixed);world_scenery_free(source);
    size_t n=0;const auto *trail=world_atlas_trail(&n);check(n>2,"ordinary dry source route available");
    const double dx=trail[2].x-trail[1].x,dz=trail[2].z-trail[1].z,length=std::hypot(dx,dz);double normal[2]={trail[1].x-dz/length*45,trail[1].z+dx/length*45};
    world_studio_scene_record::Record grounded;grounded.name="pine";world_atlas_world_to_uv(normal[0],normal[1],&grounded.u,&grounded.v);
    source=load(path,encode(grounded));check(world_scenery_placement(source,0,&pose)&&!world_scenery_explicit_pivot(source,0),"ordinary parsed source has no explicit-pivot provenance");
    grounded.pivot=pose.y;const bool terrain_changed=true,edited=false;
    grounded.preserve_source_root=!edited&&(world_scenery_explicit_pivot(source,0)||!terrain_changed);
    check(!grounded.preserve_source_root,"terrain edit keeps ordinary source re-grounding enabled");
    world_atlas_set_height_edit(raise,normal);check(world_atlas_sample(x,z).height==ocean.height,"local Raise leaves distant backdrop terrain unchanged");
    fixed=load(path,encode(grounded));check(world_scenery_placement(fixed,0,&fixed_pose)&&std::abs(fixed_pose.y-pose.y-2)<.001,"ordinary nonexplicit source follows actual raised terrain");
    check(!world_scenery_explicit_pivot(fixed,0),"ordinary terrain transaction does not invent fixed provenance");world_scenery_free(fixed);world_scenery_free(source);
    fixed=load(path,encode(authored));check(world_scenery_placement(fixed,0,&fixed_pose)&&std::abs(double(fixed_pose.y)-1262.74674988)<.0001,"same remote Raise retains imported absolute backdrop");world_scenery_free(fixed);
    fixed=load(path,encode(authored)+encode(grounded));WorldSceneryPlacement mixed0,mixed1;
    check(world_scenery_placement(fixed,0,&mixed0)&&world_scenery_placement(fixed,1,&mixed1)&&std::string(mixed0.alias)==name&&std::string(mixed1.alias)=="pine","mixed parser enumeration retains authored record order");
    check(world_scenery_explicit_pivot(fixed,0)&&!world_scenery_explicit_pivot(fixed,1),"mixed fixed and ordinary roots have distinct matching provenance");world_scenery_free(fixed);
    check(!active_models,"all CPU fixture models released");world_atlas_set_height_edit(nullptr,nullptr);check(world_scenery_configure(nullptr,0,error,sizeof(error)),"alias registry restored");world_atlas_shutdown();std::remove(path.c_str());
    std::cout<<"PASS "<<checks<<" explicit-pivot checks; real Grandstone/ocean row, real CAPI/parser provenance, old-reject/new-accept, repeated transaction, normal source follows terrain, immutable source fixture, no graphics\n";
}

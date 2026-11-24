#include "engine/world_designer.h"
#include "engine/world_transform_math.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>

static double ground(double,double,const void *){return 24.1;}
static bool bounds(const std::string &,world_studio::Vec &lo,world_studio::Vec &hi,const void *){lo={-3,0,-2};hi={3,8,2};return true;}
static std::string bytes(const std::string &path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static std::string encoded(const world_studio::Document &d){std::string text,error;assert(world_studio::encode(d,text,error));return text;}
static std::string content(world_studio::Document d){d.entities.next_id=1;for(const auto &node:d.entities.nodes)d.entities.next_id=std::max(d.entities.next_id,node.id+1);return encoded(d);}

int main(int argc,char **argv){
    assert(argc>1);char temporary[]="/tmp/valwood-private-editor-roundtrip-XXXXXX";const auto *directory=mkdtemp(temporary);assert(directory);
    for(int arg=1;arg<argc;++arg){
        const std::string original_bytes=bytes(argv[arg]);std::string error;world_studio::Document base;
        if(!world_studio::load(argv[arg],base,error)){std::cerr<<argv[arg]<<": "<<error<<'\n';return 1;}
        assert(!base.assets.empty());const auto before=content(base);auto edited=base;
        const auto asset=std::find_if(base.assets.begin(),base.assets.end(),[](const auto &a){return a.alias.find("house")!=std::string::npos||a.alias.find("castle")!=std::string::npos;});
        world_studio::Placement p;p.alias=(asset==base.assets.end()?base.assets.front():*asset).alias;p.x=-1439;p.z=-10069;p.offset_y=2;p.yaw=37;p.pitch=11;p.roll=-7;p.sx=.7;p.sy=1.3;p.sz=1.9;
        const auto index=unsigned(edited.placements.size());edited.placements.push_back(p);world_entity::Id id=0;
        assert(world_entity::add(edited.entities,0,"Readiness temporary house",{world_entity::Kind::Placement,index},{p.x,26.1,p.z,p.yaw,p.scale,p.pitch,p.roll,p.sx,p.sy,p.sz},id,error));
        world_designer::Context context;context.ground={ground,nullptr};world_designer::Selection selection;
        assert(world_designer::select(edited,context,selection,world_designer::Ref::entity(id),false,error));
        world_designer::Clipboard clipboard;assert(clipboard.copy(edited,selection,context,{bounds,nullptr},error));
        world_studio::Document pasted;world_designer::Result result;world_designer::PasteOptions options;options.gap=1;
        assert(clipboard.paste(edited,context,options,pasted,result,error));assert(pasted.placements.size()==base.placements.size()+2);
        const auto &copy=pasted.placements.back();
        const auto original_transform=vw_transform::matrix({0,0,0,p.yaw,p.scale,p.pitch,p.roll,p.sx,p.sy,p.sz});
        const auto copied_transform=vw_transform::matrix({0,0,0,copy.yaw,copy.scale,copy.pitch,copy.roll,copy.sx,copy.sy,copy.sz});double matrix_error=0;
        for(unsigned axis=0;axis<3;++axis)for(unsigned column=0;column<3;++column)matrix_error=std::max(matrix_error,std::abs(original_transform.a[axis][column]-copied_transform.a[axis][column]));
        if(matrix_error>=1e-5)std::cerr<<"Copied shape differs: "<<matrix_error<<"; yaw/pitch/roll/scale/sxyz "<<copy.yaw<<' '<<copy.pitch<<' '<<copy.roll<<' '<<copy.scale<<' '<<copy.sx<<' '<<copy.sy<<' '<<copy.sz<<'\n';
        assert(copy.alias==p.alias&&matrix_error<1e-5);
        world_studio::History history;history.checkpoint(base);history.checkpoint(edited);auto current=pasted;
        assert(history.undo(current)&&content(current)==content(edited));assert(history.redo(current)&&content(current)==content(pasted));
        const auto path=std::string(directory)+"/project-"+std::to_string(arg)+".studio";assert(world_studio::save(path.c_str(),current,error));world_studio::Document reopened;
        assert(world_studio::load(path.c_str(),reopened,error)&&encoded(reopened)==encoded(current));
        assert(history.undo(current)&&history.undo(current)&&content(current)==before);
        assert(bytes(argv[arg])==original_bytes);
        std::cout<<"PASS private editor project "<<argv[arg]<<": assets="<<base.assets.size()<<", existing placements="<<base.placements.size()<<", exact palette/scene copy-paste+undo/redo+save/reopen; source bytes unchanged\n";
    }
    std::filesystem::remove_all(directory);
}

#ifndef VALWOOD_WORLD_STRUCTURE_COLLISION_H
#define VALWOOD_WORLD_STRUCTURE_COLLISION_H
#include "engine/world_assets.h"
#include "engine/world_editor_scene_guard.h"
#include "engine/world_physics.h"
#include "engine/world_scenery.h"
#include "engine/world_transform_math.h"
#include "engine/world_structure_runtime.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

// Geometry-only structural cook. Capture runs at an editor loading boundary;
// build owns its inputs and may run on a CPU worker. It never reads GL objects,
// changes a live physics world, or rewrites the project's source assets.
namespace world_structure_collision {
constexpr std::size_t part_bytes_limit=64u*1024u*1024u;
constexpr std::size_t geometry_bytes_limit=768u*1024u*1024u;
constexpr unsigned world_limit=192;
inline std::uint64_t hash_bytes(const void *bytes,std::size_t size,std::uint64_t value=14695981039346656037ull){const auto *p=static_cast<const unsigned char *>(bytes);for(std::size_t i=0;i<size;++i){value^=p[i];value*=1099511628211ull;}return value;}
inline std::uint64_t hash_text(const std::string &s,std::uint64_t value){const std::uint64_t size=s.size();value=hash_bytes(&size,sizeof size,value);return hash_bytes(s.data(),s.size(),value);}
inline bool safe_relative(const std::string &path){if(path.empty()||path.size()>512||path.find('\\')!=std::string::npos)return false;const auto p=std::filesystem::u8path(path);if(p.is_absolute())return false;for(const auto &part:p)if(part==".."||part==".")return false;return true;}
inline bool file_stamp(const std::string &project,const std::string &name,std::uint64_t &stamp,std::string &error){
    std::error_code ec;const auto path=std::filesystem::u8path(project)/std::filesystem::u8path(name);
    const auto size=std::filesystem::file_size(path,ec);if(ec){error="Missing structural source geometry: "+name;return false;}
    const auto time=std::filesystem::last_write_time(path,ec);if(ec){error="Cannot inspect structural source revision: "+name;return false;}
    const auto ticks=time.time_since_epoch().count();stamp=hash_bytes(&size,sizeof size);stamp=hash_bytes(&ticks,sizeof ticks,stamp);return true;
}
struct Part {std::vector<world_physics::Vec3> vertices;std::vector<std::array<std::uint32_t,3>> triangles;std::uint64_t fingerprint=0;};
struct Binding {std::string model;std::vector<std::string> parts;bool empty=false;std::uint64_t model_bytes=0,model_hash=0;};
using Catalog=std::map<std::string,Binding>;
inline bool load_catalog(const std::string &project,Catalog &catalog,std::string &error){
    const auto file=std::filesystem::u8path(project)/"collision/structures/catalog";std::ifstream in(file,std::ios::binary);
    if(!in){error="Missing structural source collision catalog: "+file.u8string();return false;}
    std::string line;if(!std::getline(in,line)||line!="VALWOOD_STRUCTURE_COLLISION 1"){error="Unsupported structural collision catalog";return false;}
    Catalog next;unsigned count=0;
    while(std::getline(in,line)){if(line.empty()||line[0]=='#')continue;if(line.size()>2048||++count>4096){error="Structural collision catalog exceeds its bounds";return false;}
        std::istringstream row(line);std::string kind,alias,model,path,extra;row>>kind;
        if(kind=="model"){
            std::uint64_t bytes=0,hash=0;std::string encoded;row>>std::quoted(alias)>>bytes>>encoded;
            const auto found=next.find(alias);if(!row||(row>>extra)||found==next.end()||found->second.empty||found->second.model_bytes||!bytes||encoded.size()!=16||!std::all_of(encoded.begin(),encoded.end(),[](unsigned char c){return std::isxdigit(c)!=0;})){error="Malformed structural model revision binding";return false;}
            try{hash=std::stoull(encoded,nullptr,16);}catch(...){error="Invalid structural model revision hash";return false;}
            found->second.model_bytes=bytes;found->second.model_hash=hash;continue;
        }
        row>>std::quoted(alias)>>std::quoted(model);
        if(kind=="mesh"){row>>std::quoted(path);}
        if(!row||(row>>extra)||alias.empty()||alias.size()>95||!safe_relative(model)||(kind!="none"&&kind!="mesh")||(kind=="mesh"&&!safe_relative(path))){error="Malformed structural collision catalog row";return false;}
        auto found=next.find(alias);if(found!=next.end()&&(found->second.model!=model||found->second.empty||kind=="none")){error="Conflicting structural source collision bindings";return false;}
        auto &binding=next[alias];binding.model=model;binding.empty=kind=="none";if(kind=="mesh")binding.parts.push_back(path);
    }
    if(!in.eof()||next.empty()){error="Cannot read structural source collision catalog";return false;}
    for(const auto &entry:next)if(!entry.second.empty&&!entry.second.model_bytes){error="Structural collision lacks immutable model revision evidence: "+entry.first;return false;}
    catalog=std::move(next);return true;
}
inline bool load_part(const std::string &project,const std::string &relative,Part &part,std::string &error){
    if(!safe_relative(relative)){error="Invalid relative structural collision part";return false;}
    const auto path=std::filesystem::u8path(project)/std::filesystem::u8path(relative);std::ifstream in(path,std::ios::binary|std::ios::ate);
    if(!in){error="Cannot open structural collision part: "+relative;return false;}const auto size=in.tellg();
    if(size<24||std::uint64_t(size)>part_bytes_limit){error="Structural collision part exceeds its byte budget";return false;}
    in.seekg(0);std::vector<unsigned char> bytes(static_cast<std::size_t>(size));if(!in.read(reinterpret_cast<char *>(bytes.data()),size)){error="Cannot read structural collision part";return false;}
    std::uint32_t nv=0,nt=0;std::uint64_t expected=0;std::memcpy(&nv,bytes.data()+8,4);std::memcpy(&nt,bytes.data()+12,4);std::memcpy(&expected,bytes.data()+bytes.size()-8,8);
    if(std::memcmp(bytes.data(),"VWCOL1\0\0",8)||!nv||!nt||nv>3000000||nt>1000000||16ull+12ull*nv+12ull*nt+8!=bytes.size()||hash_bytes(bytes.data(),bytes.size()-8)!=expected){error="Structural collision checksum, version or geometry count is invalid: "+relative;return false;}
    Part next;next.fingerprint=expected;next.vertices.resize(nv);next.triangles.resize(nt);const unsigned char *p=bytes.data()+16;
    for(auto &v:next.vertices){float xyz[3];std::memcpy(xyz,p,12);p+=12;for(float x:xyz)if(!std::isfinite(x)||std::abs(x)>100000){error="Nonfinite or unbounded structural collision geometry";return false;}v={xyz[0],xyz[1],xyz[2]};}
    for(auto &t:next.triangles){std::memcpy(t.data(),p,12);p+=12;for(auto ix:t)if(ix>=nv){error="Structural collision index is outside its vertex buffer";return false;}}
    part=std::move(next);return true;
}
struct Instance {std::string alias,model;world_entity::Transform pose;std::uint64_t identity=0;};
struct Capture {std::string project;Catalog catalog;std::vector<Instance> instances;world_structure_runtime::Prepared procedural;std::uint64_t fingerprint=0;};
struct Geometry {
    std::vector<world_physics::Document> documents;
    double xmin=1e30,xmax=-1e30,zmin=1e30,zmax=-1e30,ymin=1e30,ymax=-1e30;
    std::uint64_t fingerprint=0,vertices=0,triangles=0;unsigned instances=0;
    bool empty()const{return documents.empty();}
};
inline std::string model_path(const std::string &name,const world_studio::Document &document){
    // Candidate asset definitions override the live registry until publication.
    for(const auto &a:document.assets)if(a.alias==name)return "models/"+a.stem+(a.vegetation==2?"_near.glb":".glb");
    for(std::size_t i=0;i<world_scenery_alias_count();++i){const auto a=world_scenery_alias(i);if(a.name&&name==a.name)return std::string("models/")+a.stem+((a.tree||a.named_near)?"_near.glb":".glb");}
    return {};
}
inline bool capture(const std::string &project,const WorldScenery *scene,const world_studio::Document &document,world_editor_scene_guard::Mode mode,Capture &capture,std::string &error,
                    world_structure_runtime::Ground ground=nullptr,const void *ground_context=nullptr){
    Capture next;next.project=project;
    if(!world_structure_runtime::prepare(document.structures,ground,ground_context,next.procedural,error))return false;
    if(mode==world_editor_scene_guard::Mode::None){next.fingerprint=next.procedural.meshes.empty()?0:next.procedural.fingerprint;capture=std::move(next);return true;}
    if(!load_catalog(project,next.catalog,error))return false;
    const auto count=world_scenery_stats(scene).source_instances;next.instances.reserve(count);std::uint64_t hash=14695981039346656037ull;
    for(std::size_t i=0;i<count;++i){WorldSceneryPlacement p;if(!world_scenery_placement(scene,i,&p)){error="Cannot snapshot prepared structural geometry";return false;}
        if(!p.alias)continue;
        const auto found=next.catalog.find(p.alias);
        if(found==next.catalog.end()&&!world_editor_scene_guard::protected_alias(mode,p.alias))continue;
        Instance item;item.alias=p.alias;item.model=model_path(item.alias,document);item.identity=i+1;
        item.pose={p.x,p.y,p.z,p.yaw*57.29577951308232,p.scale,p.pitch*57.29577951308232,p.roll*57.29577951308232,p.sx,p.sy,p.sz};
        if(item.model.empty()||!safe_relative(item.model)){error="Missing model revision for structural object "+item.alias;return false;}
        if(found!=next.catalog.end()&&found->second.empty)continue;
        std::uint64_t model_stamp=0;if(!file_stamp(project,item.model,model_stamp,error))return false;
        hash=hash_bytes(&model_stamp,sizeof model_stamp,hash);
        // The scene snapshot already includes entity visibility, hierarchy,
        // source overrides, grounding and explicit pivots. Never recompute Y
        // from the atlas or an enclosing model box here.
        hash=hash_text(item.alias,hash);hash=hash_text(item.model,hash);
        for(double value:{item.pose.x,item.pose.y,item.pose.z,item.pose.yaw,item.pose.scale,item.pose.pitch,item.pose.roll,item.pose.sx,item.pose.sy,item.pose.sz})hash=hash_bytes(&value,sizeof value,hash);
        next.instances.push_back(std::move(item));
    }
    for(const auto &row:next.catalog){hash=hash_text(row.first,hash);hash=hash_text(row.second.model,hash);hash=hash_bytes(&row.second.model_bytes,sizeof row.second.model_bytes,hash);hash=hash_bytes(&row.second.model_hash,sizeof row.second.model_hash,hash);for(const auto &part:row.second.parts){std::uint64_t stamp=0;if(!file_stamp(project,part,stamp,error))return false;hash=hash_text(part,hash);hash=hash_bytes(&stamp,sizeof stamp,hash);}}
    if(!next.procedural.meshes.empty())hash=hash_bytes(&next.procedural.fingerprint,sizeof next.procedural.fingerprint,hash);
    next.fingerprint=hash;capture=std::move(next);return true;
}
// A bounded geometry cache belongs to one CPU cooking worker. It is independent
// of the live scene; failed or superseded jobs cannot alter published collision.
class Cache {
    std::unordered_map<std::string,std::shared_ptr<const Part>> parts_;
    std::size_t bytes_=0;
    std::unordered_map<std::string,std::pair<std::uint64_t,std::uint64_t>> models_;
public:
    void clear(){parts_.clear();bytes_=0;}
    bool original(const std::string &project,const std::string &name,const Binding &binding,bool &matches,std::string &error){
        std::uint64_t stamp=0;if(!file_stamp(project,name,stamp,error))return false;
        const auto path=project+"/"+name,key=path+"#"+std::to_string(stamp);auto at=models_.find(key);
        if(at==models_.end()){
            std::ifstream in(std::filesystem::u8path(path),std::ios::binary);if(!in){error="Cannot read structural model revision: "+name;return false;}
            std::uint64_t size=0,hash=14695981039346656037ull;char buffer[65536];
            while(in){in.read(buffer,sizeof buffer);const auto count=in.gcount();size+=std::uint64_t(count);if(size>512u*1024u*1024u){error="Structural model revision exceeds its byte bound";return false;}hash=hash_bytes(buffer,std::size_t(count),hash);}
            if(!in.eof()){error="Cannot verify structural model revision: "+name;return false;}
            at=models_.emplace(key,std::make_pair(size,hash)).first;
        }
        matches=at->second.first==binding.model_bytes&&at->second.second==binding.model_hash;return true;
    }
    std::shared_ptr<const Part> part(const std::string &project,const std::string &name,std::string &error){
        std::uint64_t stamp=0;if(!file_stamp(project,name,stamp,error))return {};
        const auto key=project+"/"+name+"#"+std::to_string(stamp);const auto at=parts_.find(key);if(at!=parts_.end())return at->second;
        auto p=std::make_shared<Part>();if(!load_part(project,name,*p,error))return {};
        const auto bytes=p->vertices.size()*sizeof(world_physics::Vec3)+p->triangles.size()*12;
        if(bytes>geometry_bytes_limit){error="Structural source geometry exceeds its cache budget";return {};}
        if(bytes_>geometry_bytes_limit-bytes){clear();}
        parts_[key]=p;bytes_+=bytes;return p;
    }
    std::shared_ptr<const Part> visual(const std::string &project,const std::string &name,std::string &error){
        std::uint64_t stamp=0;if(!file_stamp(project,name,stamp,error))return {};
        const auto path=project+"/"+name,key=path+"#"+std::to_string(stamp);const auto at=parts_.find(key);if(at!=parts_.end())return at->second;
        const auto report=world_assets::inspect(path,true);if(!report.supported||report.geometry.empty()){error="Cannot cook structural source mesh "+name+": "+report.error;return {};}
        auto p=std::make_shared<Part>();p->vertices.reserve(report.geometry.size()*3);p->triangles.reserve(report.geometry.size());
        for(const auto &triangle:report.geometry){
            // Render meshes may retain collapsed UV seams or zero-area faces.
            // They have no physical surface and Jolt rejects them; discard
            // only the same sub-threshold area as physics validation does.
            const double ux=double(triangle.b[0])-triangle.a[0],uy=double(triangle.b[1])-triangle.a[1],uz=double(triangle.b[2])-triangle.a[2];
            const double vx=double(triangle.c[0])-triangle.a[0],vy=double(triangle.c[1])-triangle.a[1],vz=double(triangle.c[2])-triangle.a[2];
            if(std::hypot(std::hypot(uy*vz-uz*vy,uz*vx-ux*vz),ux*vy-uy*vx)<1.e-8)continue;
            const auto base=std::uint32_t(p->vertices.size());for(const auto *v:{&triangle.a,&triangle.b,&triangle.c})p->vertices.push_back({(*v)[0],(*v)[1],(*v)[2]});p->triangles.push_back({base,base+1,base+2});
        }
        if(p->triangles.empty()){error="Replacement structural mesh has no nondegenerate physical surface";return {};}
        const auto bytes=p->vertices.size()*sizeof(world_physics::Vec3)+p->triangles.size()*12;if(bytes>geometry_bytes_limit){error="Replacement structural geometry exceeds its cook budget";return {};}
        if(bytes_>geometry_bytes_limit-bytes){clear();}
        bytes_+=bytes;parts_[key]=p;return p;
    }
};
inline bool build(const Capture &capture,Geometry &geometry,std::string &error,Cache *reuse=nullptr){
    Cache local;Cache &cache=reuse?*reuse:local;Geometry next;next.fingerprint=capture.fingerprint;
    world_physics::Document document;unsigned vertices=0,triangles=0;world_physics::Id next_id=1;
    auto flush=[&]()->bool{if(document.bodies.empty())return true;if(!world_physics::validate(document,error))return false;if(next.documents.size()>=world_limit){error="Structural collision exceeds the bounded world partition budget";return false;}next.documents.push_back(std::move(document));document={};vertices=triangles=0;return true;};
    for(const auto &instance:capture.instances){
        std::vector<std::shared_ptr<const Part>> source;const auto binding=capture.catalog.find(instance.alias);
        if(binding!=capture.catalog.end()&&binding->second.empty)continue;
        bool original=false;if(binding!=capture.catalog.end()&&binding->second.model==instance.model&&!cache.original(capture.project,instance.model,binding->second,original,error))return false;
        if(original){for(const auto &part:binding->second.parts){auto p=cache.part(capture.project,part,error);if(!p)return false;source.push_back(std::move(p));}}
        else {auto p=cache.visual(capture.project,instance.model,error);if(!p)return false;source.push_back(std::move(p));}
        ++next.instances;const auto transform=vw_transform::matrix(instance.pose);
        for(const auto &part:source){
            world_physics::Body body;std::unordered_map<std::uint32_t,std::uint32_t> remap;remap.reserve(std::min<std::size_t>(part->vertices.size(),world_physics::vertex_limit));
            auto begin_body=[&]{body={};body.id=next_id++;body.name=("Structure "+instance.alias).substr(0,95);body.shape=world_physics::Shape::Mesh;body.motion=world_physics::Motion::Static;body.pose.position={instance.pose.x,instance.pose.y,instance.pose.z};remap.clear();};
            auto finish_body=[&]()->bool{if(body.triangles.empty())return true;vertices+=unsigned(body.vertices.size());next.vertices+=body.vertices.size();triangles+=unsigned(body.triangles.size());document.bodies.push_back(std::move(body));return true;};
            begin_body();
            for(const auto &triangle:part->triangles){
                // A valid source face can collapse below physical precision
                // after a small or nonuniform instance scale. Test its exact
                // transformed area before adding any vertices to the cook.
                vw_transform::V points[3];for(unsigned corner=0;corner<3;++corner){const auto &v=part->vertices[triangle[corner]];points[corner]=vw_transform::vector(transform,{v.x,v.y,v.z});}
                const double ux=points[1][0]-points[0][0],uy=points[1][1]-points[0][1],uz=points[1][2]-points[0][2];
                const double vx=points[2][0]-points[0][0],vy=points[2][1]-points[0][1],vz=points[2][2]-points[0][2];
                if(std::hypot(std::hypot(uy*vz-uz*vy,uz*vx-ux*vz),ux*vy-uy*vx)<1.e-8)continue;
                unsigned added=0;for(auto index:triangle)if(!remap.count(index))++added;
                if(document.bodies.size()>=world_physics::body_limit||vertices+body.vertices.size()+added>world_physics::vertex_limit||triangles+body.triangles.size()+1>world_physics::triangle_limit){if(!finish_body()||!flush())return false;begin_body();}
                std::array<std::uint32_t,3> t;
                for(unsigned corner=0;corner<3;++corner){const auto index=triangle[corner];auto at=remap.find(index);if(at==remap.end()){const auto &v=part->vertices[index];const auto transformed=vw_transform::vector(transform,{v.x,v.y,v.z});const world_physics::Vec3 local{transformed[0],transformed[1],transformed[2]};
                        for(double value:{local.x,local.y,local.z})if(!std::isfinite(value)||std::abs(value)>1.e6){error="Structural transform produces unbounded collision";return false;}
                        const auto ix=std::uint32_t(body.vertices.size());body.vertices.push_back(local);remap.emplace(index,ix);t[corner]=ix;
                        next.xmin=std::min(next.xmin,local.x+instance.pose.x);next.xmax=std::max(next.xmax,local.x+instance.pose.x);next.ymin=std::min(next.ymin,local.y+instance.pose.y);next.ymax=std::max(next.ymax,local.y+instance.pose.y);next.zmin=std::min(next.zmin,local.z+instance.pose.z);next.zmax=std::max(next.zmax,local.z+instance.pose.z);
                    }else t[corner]=at->second;}
                body.triangles.push_back(t);++next.triangles;
            }
            if(!finish_body())return false;
        }
    }
    if(!flush())return false;
    for(const auto &mesh:capture.procedural.meshes){
        if(next.documents.size()>=world_limit||!world_physics::validate(mesh.collision,error)){if(error.empty())error="Procedural structure exceeds collision partition budget";return false;}
        next.documents.push_back(mesh.collision);++next.instances;next.vertices+=mesh.vertices.size();next.triangles+=mesh.collision.bodies.front().triangles.size();
        next.xmin=std::min(next.xmin,mesh.bounds[0]);next.xmax=std::max(next.xmax,mesh.bounds[1]);next.ymin=std::min(next.ymin,mesh.bounds[2]);next.ymax=std::max(next.ymax,mesh.bounds[3]);next.zmin=std::min(next.zmin,mesh.bounds[4]);next.zmax=std::max(next.zmax,mesh.bounds[5]);
    }
    geometry=std::move(next);error.clear();return true;
}
}
#endif

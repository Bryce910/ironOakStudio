/* Actual model/bounds/color/shadow validation. Run from the engine root or
 * pass its absolute path. Default SDL offscreen; VALWOOD_TEST_NATIVE_GL=1
 * preserves the native display/driver. This is not a performance benchmark.
 */
#define GL_GLEXT_PROTOTYPES
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_scenery.h"
#include "engine/world_atlas.h"
#include "engine/world_models.h"
extern "C" {
#include "engine/texture_cache.h"
}
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <set>
#include <string>
#include <unistd.h>
#include <vector>

namespace {
constexpr int extent = 160, target_height = 90;
using Matrix = std::array<float, 16>;
std::string read(const std::string &path) {
    std::ifstream file(path); assert(file);
    std::ostringstream out; out << file.rdbuf(); return out.str();
}
GLuint shader(GLenum type, const std::string &path) {
    auto source = read(path); const char *p = source.c_str();
    GLuint result = glCreateShader(type); glShaderSource(result, 1, &p, nullptr); glCompileShader(result);
    GLint ok; glGetShaderiv(result, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[4096]; glGetShaderInfoLog(result, sizeof(log), nullptr, log); std::fprintf(stderr, "%s: %s\n", path.c_str(), log); }
    assert(ok); return result;
}
GLuint program(const std::string &root, const char *fragment) {
    GLuint vs = shader(GL_VERTEX_SHADER, root + "/shaders/world_foliage.vs.glsl");
    GLuint fs = shader(GL_FRAGMENT_SHADER, root + "/shaders/" + fragment);
    GLuint result = glCreateProgram(); glAttachShader(result, vs); glAttachShader(result, fs); glLinkProgram(result);
    GLint ok; glGetProgramiv(result, GL_LINK_STATUS, &ok); assert(ok);
    glUseProgram(result);glUniform1i(glGetUniformLocation(result,"uVillageSkyMask"),12);
    glUniform1i(glGetUniformLocation(result,"uLocalProbe0"),4);glUniform1i(glGetUniformLocation(result,"uLocalProbe1"),5);
    glUniform1i(glGetUniformLocation(result,"uLocalProbeCount"),0);glUniform1f(glGetUniformLocation(result,"uLocalIrradianceWeight"),0);
    glUniform1i(glGetUniformLocation(result,"uVillageSkyDirectional"),13);glUniform1i(glGetUniformLocation(result,"uVillageSkyUp"),14);
    glDeleteShader(vs); glDeleteShader(fs); return result;
}
void f(GLuint p, const char *name, float value) { glUniform1f(glGetUniformLocation(p, name), value); }
void i(GLuint p, const char *name, int value) { glUniform1i(glGetUniformLocation(p, name), value); }
void v3(GLuint p, const char *name, float x, float y, float z) { glUniform3f(glGetUniformLocation(p, name), x, y, z); }
Matrix view_projection(float yaw, float pitch) {
    float sy = std::sin(yaw), cy = std::cos(yaw), sp = std::sin(pitch), cp = std::cos(pitch);
    Matrix view = {cy, -sy * sp, -sy * cp, 0, 0, cp, -sp, 0, sy, cy * sp, cy * cp, 0, 0, 0, 0, 1};
    Matrix projection = {}, result = {};
    projection[5] = 1.f / std::tan(.54f);
    projection[0] = projection[5] * target_height / extent;
    projection[10] = (15000.f + .4f) / (.4f - 15000.f); projection[11] = -1;
    projection[14] = 2 * .4f * 15000.f / (.4f - 15000.f);
    for (int c = 0; c < 4; ++c) for (int row = 0; row < 4; ++row) for (int k = 0; k < 4; ++k)
        result[c * 4 + row] += projection[k * 4 + row] * view[c * 4 + k];
    return result;
}
GLuint constant_texture(float value) {
    GLuint texture; glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 1, 1, 0, GL_RED, GL_FLOAT, &value);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return texture;
}
void route_clearance(WorldScenery *scene,const std::string &json) {
    // Authoring data is the independent route oracle. Collision comes from
    // the loaded real models/compound shapes, never copied geometry math.
    const std::regex route(R"route("points_xz"\s*:\s*\[([\s\S]*?)\]\s*,\s*"half_width_m"\s*:\s*([-+.0-9eE]+))route");
    const std::regex point(R"point(\[\s*([-+.0-9eE]+)\s*,\s*([-+.0-9eE]+)\s*\])point");
    std::size_t paths=0,sweeps=0;double length_total=0;
    for(std::sregex_iterator it(json.begin(),json.end(),route),end;it!=end;++it) {
        auto body=(*it)[1].str();double half_width=std::stod((*it)[2]);
        assert(half_width>=1.4&&half_width<=2);
        std::vector<std::array<double,2>> points;
        for(std::sregex_iterator p(body.begin(),body.end(),point);p!=end;++p)
            points.push_back({std::stod((*p)[1]),std::stod((*p)[2])});
        assert(points.size()>=2);++paths;
        for(std::size_t segment=1;segment<points.size();segment++) {
            double dx=points[segment][0]-points[segment-1][0],dz=points[segment][1]-points[segment-1][1];
            double length=std::hypot(dx,dz),nx=-dz/length,nz=dx/length;assert(length>0);
            length_total+=length;int steps=int(std::ceil(length/.5));
            for(int lane=-1;lane<=1;lane++)for(int step=1;step<=steps;step++) {
                double offset=lane*(half_width-.37-.03),t0=double(step-1)/steps,t1=double(step)/steps;
                double ax=points[segment-1][0]+dx*t0+nx*offset,az=points[segment-1][1]+dz*t0+nz*offset;
                double bx=points[segment-1][0]+dx*t1+nx*offset,bz=points[segment-1][1]+dz*t1+nz*offset;
                float floor_a=world_atlas_height(ax,az),floor_b=world_atlas_height(bx,bz);
                bool clear=world_scenery_can_move(scene,ax,az,ax,az,floor_a,1.75f,.37f)&&
                    world_scenery_can_move(scene,bx,bz,bx,bz,floor_b,1.75f,.37f)&&
                    world_scenery_can_move(scene,ax,az,bx,bz,std::min(floor_a,floor_b),1.75f,.37f);
                if(!clear) {
                    std::fprintf(stderr,"Blocked authored route%zu segment%zu lane%d at %.4f %.4f -> %.4f %.4f\n",paths,segment,lane,ax,az,bx,bz);
                    auto stats=world_scenery_stats(scene);
                    for(std::size_t k=0;k<stats.source_instances;k++){WorldSceneryPlacement a;assert(world_scenery_placement(scene,k,&a));
                        if(std::hypot(a.x-ax,a.z-az)<12)std::fprintf(stderr,"  nearby %s at %.4f %.4f y%.4f scale%.3f\n",a.alias,a.x,a.z,a.y,a.scale);}
                }
                assert(clear);++sweeps;
            }
        }
    }
    assert(paths>=2&&length_total>250&&sweeps>1400);
    std::printf("Actual-model route clearance: %zu paths, %.3fm unique polylines, %zu continuous half-meter sweeps across3 capsule lanes\n",paths,length_total,sweeps);
}
void rainforest_clearance(WorldScenery *scene) {
    std::size_t count=0;const auto *points=world_atlas_rainforest_trail_points(&count);assert(points&&count==6);
    std::size_t sweeps=0;double length_total=0;
    for(std::size_t segment=1;segment<count;++segment) {
        const auto &a=points[segment-1],&b=points[segment];
        double dx=b.x-a.x,dz=b.z-a.z,length=std::hypot(dx,dz);assert(length>0);
        assert(a.half_width==1.6f&&b.half_width==1.6f);length_total+=length;
        double nx=-dz/length,nz=dx/length;int steps=int(std::ceil(length/.5));
        for(int lane=-1;lane<=1;++lane)for(int step=1;step<=steps;++step) {
            double offset=lane*(1.6-.37-.03),t0=double(step-1)/steps,t1=double(step)/steps;
            double ax=a.x+dx*t0+nx*offset,az=a.z+dz*t0+nz*offset;
            double bx=a.x+dx*t1+nx*offset,bz=a.z+dz*t1+nz*offset;
            float ya=world_atlas_height(ax,az),yb=world_atlas_height(bx,bz);
            bool clear=world_scenery_can_move(scene,ax,az,ax,az,ya,1.75f,.37f)&&
                world_scenery_can_move(scene,bx,bz,bx,bz,yb,1.75f,.37f)&&
                world_scenery_can_move(scene,ax,az,bx,bz,std::min(ya,yb),1.75f,.37f);
            if(!clear)std::fprintf(stderr,"Blocked rainforest route segment%zu lane%d %.4f %.4f -> %.4f %.4f\n",segment,lane,ax,az,bx,bz);
            assert(clear);++sweeps;
        }
    }
    assert(length_total>200&&sweeps>=1200);
    std::printf("Rainforest actual-model route: %.3fm, %zu continuous half-meter sweeps across3 .37m capsule lanes, zero obstructions\n",length_total,sweeps);
}
struct SourceRecords {
    std::size_t instances=0,trees=0,props=0,imported_buildings=0,imported_goods=0;std::set<std::string> aliases;
};
SourceRecords source_records(const std::string &path) {
    SourceRecords result;std::istringstream file(read(path));std::string line;
    while(std::getline(file,line)) {
        std::istringstream fields(line);std::string alias;if(!(fields>>alias)||alias[0]=='#')continue;
        result.instances++;result.aliases.insert(alias);
        if(alias=="pine"||alias=="fir"||alias=="broadleaf"||alias=="pine_young")result.trees++;else result.props++;
        result.imported_goods+=alias.rfind("purchased_goods_",0)==0;
        result.imported_buildings+=alias.rfind("purchased_house_",0)==0||alias.rfind("purchased_shop_",0)==0||alias=="purchased_castle";
    }
    return result;
}
void empty_cache() {
    auto cache = texture_cache_stats();
    assert(cache.entries == 0 && cache.references == 0);
}

void purchased_environment(const std::string &root,const std::string &models,GLuint color_program) {
    const std::string directory=root+"/projects/valwood_world/scenery/";
    const std::string additions=directory+"bep_environment_instances.txt";
    if(!std::filesystem::is_regular_file(additions))return;
    const char *names[]={"authored_instances.txt","starting_area_instances.txt","rainforest_instances.txt",
        "castle_village_instances.txt","tanthoon_bandit_camp_instances.txt","castle_streets_instances.txt",
        "bep_environment_instances.txt"};
    std::vector<std::string> storage;std::vector<const char*> paths;std::size_t count=0;
    for(const char *name:names){storage.push_back(directory+name);count+=source_records(storage.back()).instances;}
    const std::string finish=directory+"demo_finish_instances.txt";
    const bool has_finish=std::filesystem::is_regular_file(finish);
    if(has_finish){storage.push_back(finish);count+=source_records(finish).instances;}
    for(const auto &path:storage)paths.push_back(path.c_str());
    char error[1024]={};auto *scene=world_scenery_load_files(paths.data(),paths.size(),models.c_str(),error,sizeof(error));
    if(!scene)std::fprintf(stderr,"Purchased environment load: %s\n",error);
    assert(scene);const auto stats=world_scenery_stats(scene);const auto added=source_records(additions);
    assert(stats.source_files==(has_finish?8u:7u)&&stats.source_instances==count&&count<=8192);
    const auto source=world_scenery_source_stats(scene,6);
    assert(source.instances==added.instances&&source.aliases==18&&source.colliders>0);
    assert(added.instances>500&&added.instances<=1300);
    if(has_finish){const auto expected=source_records(finish);const auto last=world_scenery_source_stats(scene,7);
        assert(expected.instances>=250&&expected.instances<=1200&&expected.imported_buildings>=20);
        assert(last.instances==expected.instances&&last.aliases==expected.aliases.size()&&last.colliders>25);}
    rainforest_clearance(scene);
    // Use the committed full walking route as an independent oracle against
    // production compound colliders, including the enterable tavern shell.
    const auto manifest=read(directory+"castle_streets_manifest.json");
    const std::regex route(R"route("points_world_xz"\s*:\s*\[([\s\S]*?)\]\s*,\s*"sampled_step_m")route");
    const std::regex point(R"point(\[\s*([-+.0-9eE]+)\s*,\s*([-+.0-9eE]+)\s*\])point");
    std::smatch match;assert(std::regex_search(manifest,match,route));
    const auto body=match[1].str();std::vector<std::array<double,2>> points;
    for(std::sregex_iterator it(body.begin(),body.end(),point),end;it!=end;++it)
        points.push_back({std::stod((*it)[1]),std::stod((*it)[2])});
    assert(points.size()==30);std::size_t sweeps=0;
    for(std::size_t k=1;k<points.size();++k) {
        const auto &a=points[k-1],&b=points[k];const double dx=b[0]-a[0],dz=b[1]-a[1];
        const int steps=int(std::ceil(std::hypot(dx,dz)/.25));
        const double length=std::hypot(dx,dz),nx=length>0?-dz/length:0,nz=length>0?dx/length:0;
        for(int lane=-1;lane<=1;++lane)for(int step=0;step<steps;++step) {
            const double t=double(step)/steps,u=double(step+1)/steps;
            // Three adjacent capsule lanes verify a full 1.10m walking strip
            // even on the tight tavern/bar return, rather than its center only.
            const double offset=lane*.18;
            const double x=a[0]+dx*t+nx*offset,z=a[1]+dz*t+nz*offset;
            const double xx=a[0]+dx*u+nx*offset,zz=a[1]+dz*u+nz*offset;
            assert(world_scenery_can_move(scene,x,z,xx,zz,world_atlas_walk_height(x,z),1.75f,.37f));++sweeps;
        }
    }
    if(has_finish) {
        double ox,oz;world_atlas_village_origin(&ox,&oz);
        const auto finish_manifest=read(directory+"demo_finish_manifest.json");
        // The compact arcade turns between close frontages, then exits west
        // before its blocked vista. Read the committed independent authoring
        // polyline; test the complete public width through real colliders.
        const std::regex arcade_route(R"route("new_arcade_route_local_xz"\s*:\s*\[([\s\S]*?)\]\s*,\s*"new_arcade_route_half_width_m"\s*:\s*([-+.0-9eE]+))route");
        std::smatch arcade_match;assert(std::regex_search(finish_manifest,arcade_match,arcade_route));
        const auto arcade_body=arcade_match[1].str();const double arcade_half_width=std::stod(arcade_match[2]);
        std::vector<std::array<double,2>> arcade_points;
        for(std::sregex_iterator it(arcade_body.begin(),arcade_body.end(),point),end;it!=end;++it)
            arcade_points.push_back({std::stod((*it)[1]),std::stod((*it)[2])});
        assert(arcade_points.size()==9&&arcade_half_width==1.3);
        assert((arcade_points.front()==std::array<double,2>{{-95,36}}));
        assert((arcade_points.back()==std::array<double,2>{{-95,-20}}));
        std::size_t arcade_sweeps=0;double arcade_length=0;
        for(std::size_t segment=1;segment<arcade_points.size();++segment) {
            const auto &a=arcade_points[segment-1],&b=arcade_points[segment];
            const double dx=b[0]-a[0],dz=b[1]-a[1],length=std::hypot(dx,dz);assert(length>1);
            arcade_length+=length;const double nx=-dz/length,nz=dx/length;
            const int steps=int(std::ceil(length/.25));
            for(int lane=-1;lane<=1;++lane)for(int step=0;step<steps;++step) {
                const double offset=lane*(arcade_half_width-.37-.03),t=double(step)/steps,u=double(step+1)/steps;
                const double x=ox+a[0]+dx*t+nx*offset,z=oz+a[1]+dz*t+nz*offset;
                const double xx=ox+a[0]+dx*u+nx*offset,zz=oz+a[1]+dz*u+nz*offset;
                assert(world_scenery_can_move(scene,x,z,x,z,world_atlas_walk_height(x,z),1.75f,.37f));
                assert(world_scenery_can_move(scene,xx,zz,xx,zz,world_atlas_walk_height(xx,zz),1.75f,.37f));
                assert(world_scenery_can_move(scene,x,z,xx,zz,std::min(world_atlas_walk_height(x,z),world_atlas_walk_height(xx,zz)),1.75f,.37f));
                ++arcade_sweeps;
            }
        }
        assert(arcade_length>105&&arcade_length<115&&arcade_sweeps>1200);
        const auto camera_start=finish_manifest.find("\"inspection_cameras\"");assert(camera_start!=std::string::npos);
        const auto cameras=finish_manifest.substr(camera_start);
        const std::regex camera(R"camera("id"\s*:\s*"(castle-finished-[^"]+)"[\s\S]*?"world_xz"\s*:\s*\[\s*([-+.0-9eE]+)\s*,\s*([-+.0-9eE]+)\s*\])camera");
        std::size_t camera_count=0;
        for(std::sregex_iterator it(cameras.begin(),cameras.end(),camera),end;it!=end;++it){
            const double x=std::stod((*it)[2]),z=std::stod((*it)[3]);const float y=world_atlas_walk_height(x,z);
            for(const auto &offset:std::array<std::array<double,2>,5>{{{{0,0}},{{1,0}},{{-1,0}},{{0,1}},{{0,-1}}}})
                assert(world_scenery_can_move(scene,x,z,x+offset[0],z+offset[1],y,1.75f,.37f));
            ++camera_count;
        }
        assert(camera_count==3);
        std::printf("Demo finish: source 8/%zu additions, actual-bounds dogleg arcade %.3fm/%zu full-width three-lane sweeps and %zu inspection-camera neighborhoods PASS\n",source_records(finish).instances,arcade_length,arcade_sweeps,camera_count);
    }
    std::printf("Purchased environment: %zu sources/%zu records, %zu BEP additions/18 families, full 585m castle route %zu three-lane capsule sweeps PASS\n",paths.size(),count,added.instances,sweeps);
    world_scenery_free(scene);empty_cache();

    // New two-mesh trees must retain their mid geometry beyond the legacy
    // 265m billboard handoff. Check the actual selected/submitted meshes.
    for(const char *alias:{"bep_nature_oak","bep_nature_beech"}) {
        std::istringstream input(read(additions));std::string line,record;
        while(std::getline(input,line))if(line.rfind(std::string(alias)+" ",0)==0){record=line;break;}
        assert(!record.empty());auto path=std::filesystem::temp_directory_path()/("valwood-bep-lod-"+std::to_string(getpid())+".txt");
        {std::ofstream output(path);output<<record<<'\n';}
        scene=world_scenery_load(path.c_str(),models.c_str(),error,sizeof(error));assert(scene);
        WorldSceneryPlacement p;assert(world_scenery_placement(scene,0,&p));
        GLuint query;glGenQueries(1,&query);glEnable(GL_RASTERIZER_DISCARD);
        for(const double distance:{20.,80.,300.,1200.}) {
            world_scenery_update(scene,p.x,p.y+5,p.z+distance,0,0);
            const auto selected=world_scenery_stats(scene);
            assert((selected.color_instances>0)==(distance<1200));
            if(distance==300.)assert(selected.mid_trees==1&&selected.near_trees==0);
            glBeginQuery(GL_PRIMITIVES_GENERATED,query);
            world_scenery_draw(scene,color_program,p.x,p.y+5,p.z+distance);
            glEndQuery(GL_PRIMITIVES_GENERATED);GLuint primitives=0;glGetQueryObjectuiv(query,GL_QUERY_RESULT,&primitives);
            assert((primitives>0)==(distance<1200));
            GLint shader_lod=0;glGetUniformiv(color_program,glGetUniformLocation(color_program,"uLodLevel"),&shader_lod);
            assert(shader_lod==-1); // No legacy 265m fragment-stage tree handoff.
            GLfloat fade[2]={1,1},lod_range[3]={1,1,1};
            glGetUniformfv(color_program,glGetUniformLocation(color_program,"uSceneryDistanceFade"),fade);
            glGetUniformfv(color_program,glGetUniformLocation(color_program,"uPropLodRange"),lod_range);
            assert(fade[0]==0&&fade[1]==0&&lod_range[0]==0&&lod_range[1]==0&&lod_range[2]==0);
        }
        glDisable(GL_RASTERIZER_DISCARD);glDeleteQueries(1,&query);
        world_scenery_free(scene);empty_cache();std::filesystem::remove(path);
    }
    assert(glGetError()==GL_NO_ERROR);
    std::puts("Purchased two-mesh trees: near/overlap/300m mid/end selection, real primitive submissions, reset fade uniforms PASS");
}
std::size_t model_triangles(const std::string &path) {
    cgltf_options options={};cgltf_data *data=nullptr;
    assert(cgltf_parse_file(&options,path.c_str(),&data)==cgltf_result_success);
    std::size_t triangles=0;
    for(std::size_t node=0;node<data->nodes_count;++node)if(data->nodes[node].mesh)
        for(std::size_t p=0;p<data->nodes[node].mesh->primitives_count;++p) {
            const auto &primitive=data->nodes[node].mesh->primitives[p];
            assert(primitive.type==cgltf_primitive_type_triangles&&primitive.indices);
            triangles+=primitive.indices->count/3;
        }
    cgltf_free(data);assert(triangles);return triangles;
}
void shadow_proxies(const std::string &root,const std::string &models,GLuint color_program,GLuint shadow_program) {
    struct Family {const char *alias,*stem;bool tree;};
    const Family families[]={{"pine","cinematic_pine",true},{"fir","cinematic_fir",true},
        {"broadleaf","cinematic_broadleaf",true},{"pine_young","cinematic_pine_young",true},
        {"showcase_shrub","showcase_shrub",false},{"showcase_shrub_b","showcase_shrub_b",false}};
    auto path=std::filesystem::temp_directory_path()/("valwood-shadow-proxy-"+std::to_string(getpid())+".txt");
    GLuint query;glGenQueries(1,&query);glEnable(GL_RASTERIZER_DISCARD);
    for(const auto &family:families) {
        std::string line;
        for(const char *source:{"authored_instances.txt","starting_area_instances.txt","rainforest_instances.txt"}) {
            std::istringstream input(read(root+"/projects/valwood_world/scenery/"+source));std::string candidate;
            while(std::getline(input,candidate))if(candidate.rfind(std::string(family.alias)+" ",0)==0){line=candidate;break;}
            if(!line.empty())break;
        }
        assert(!line.empty());{std::ofstream output(path);output<<line<<'\n';}
        empty_cache();
        const auto near_path=models+"/"+family.stem+(family.tree?"_near.glb":".glb"),mid_path=models+"/"+family.stem+"_mid.glb";
        std::vector<WorldModelAsset*> inventory={world_model_load(near_path.c_str()),world_model_load(mid_path.c_str())};
        if(family.tree)inventory.push_back(world_model_load((models+"/"+family.stem+"_far.glb").c_str()));
        for(auto *asset:inventory)assert(asset);
        const auto shared=texture_cache_stats();assert(shared.entries&&shared.references);
        const std::size_t near_triangles=model_triangles(near_path),mid_triangles=model_triangles(mid_path);
        assert(mid_triangles<near_triangles);
        for(bool disabled:{false,true}) {
            if(disabled)setenv("VALWOOD_DISABLE_SHADOW_LOD","1",1);else unsetenv("VALWOOD_DISABLE_SHADOW_LOD");
            char error[512]={};auto *scene=world_scenery_load(path.c_str(),models.c_str(),error,sizeof(error));
            if(!scene)std::fprintf(stderr,"Shadow proxy fixture: %s\n",error);
            assert(scene&&world_scenery_stats(scene).shadow_proxy_groups==1);
            const auto retained=texture_cache_stats();
            assert(retained.entries==shared.entries&&retained.misses==shared.misses&&retained.references>shared.references);
            WorldSceneryPlacement placement;assert(world_scenery_placement(scene,0,&placement));
            const double x=placement.x,y=placement.y+1.75,z=placement.z+30;
            world_scenery_update(scene,x,y,z,0,0);
            assert(world_scenery_stats(scene).color_instances==1&&world_scenery_stats(scene).shadow_instances==1);
            auto primitives=[&](GLuint program,bool shadow) {
                glBeginQuery(GL_PRIMITIVES_GENERATED,query);world_scenery_draw(scene,program,x,y,z,shadow);
                glEndQuery(GL_PRIMITIVES_GENERATED);GLuint count=0;glGetQueryObjectuiv(query,GL_QUERY_RESULT,&count);return count;
            };
            assert(primitives(color_program,false)==near_triangles);
            assert(primitives(shadow_program,true)==(disabled?near_triangles:mid_triangles));
            world_scenery_update(scene,x,y,z,0,0);assert(!world_scenery_stats(scene).uploads_this_update);
            world_scenery_free(scene);const auto released=texture_cache_stats();
            assert(released.entries==shared.entries&&released.references==shared.references);
        }
        for(auto *asset:inventory)world_model_free(asset);
        empty_cache();std::printf("Shadow proxy %s: color%zu / shadow%zu primitives, original-mesh A/B and shared image ownership PASS\n",family.alias,near_triangles,mid_triangles);
    }
    unsetenv("VALWOOD_DISABLE_SHADOW_LOD");std::filesystem::remove(path);
    glDisable(GL_RASTERIZER_DISCARD);glDeleteQueries(1,&query);assert(glGetError()==GL_NO_ERROR);
}
void visible_prefix(const std::string &root, const std::string &models,
                    GLuint color_program, GLuint shadow_program) {
    // Reuse one real validated source prop. Its small bounds make the wide /
    // tall frustum expectations unambiguous, independently of tree crowns.
    auto path = std::filesystem::temp_directory_path() /
        ("valwood-scenery-visibility-" + std::to_string(getpid()) + ".txt");
    {
        std::istringstream records(read(root + "/projects/valwood_world/scenery/starting_area_instances.txt"));
        std::ofstream file(path); std::string line; bool found = false;
        while (std::getline(records, line)) if (line.rfind("starting_barrel ", 0) == 0) {
            file << line << '\n'; found = true; break;
        }
        assert(found);
    }
    char error[512] = {};
    WorldScenery *scene = world_scenery_load(path.c_str(), models.c_str(), error, sizeof(error));
    if (!scene) std::fprintf(stderr, "Visibility fixture load failed: %s\n", error);
    assert(scene); std::filesystem::remove(path);
    WorldSceneryPlacement placement; assert(world_scenery_placement(scene, 0, &placement));
    const double center_y = placement.y + placement.maximum[1] * placement.scale * .5;
    GLuint query; glGenQueries(1, &query); glEnable(GL_RASTERIZER_DISCARD);
    auto primitives = [&](GLuint program, double x, double y, double z, bool shadow) {
        glBeginQuery(GL_PRIMITIVES_GENERATED, query);
        world_scenery_draw(scene, program, x, y, z, shadow);
        glEndQuery(GL_PRIMITIVES_GENERATED);
        GLuint result = 0; glGetQueryObjectuiv(query, GL_QUERY_RESULT, &result);
        return result;
    };
    auto select = [&](double x, double y, double z, float pitch, float aspect, bool expected_visible,
                      float tan_vertical_half_fov = 0.f) {
        world_scenery_update(scene, x, y, z, 0, pitch, aspect, tan_vertical_half_fov);
        auto stats = world_scenery_stats(scene);
        assert(stats.props == 1 && stats.shadow_instances == 1);
        assert(stats.color_instances == std::size_t(expected_visible));
        GLuint color = primitives(color_program, x, y, z, false);
        GLuint shadow = primitives(shadow_program, x, y, z, true);
        assert((color > 0) == expected_visible && shadow > 0);
        if (expected_visible) assert(color == shadow);
        world_scenery_update(scene, x, y, z, 0, pitch, aspect, tan_vertical_half_fov);
        assert(!world_scenery_stats(scene).uploads_this_update);
    };
    // A behind-camera prop contributes no color primitives, yet remains a
    // shadow caster. Merely checking pixels would miss redundant submissions.
    select(placement.x, center_y, placement.z - 180, 0, 16.f / 9.f, false);
    select(placement.x, center_y, placement.z + 180, 0, 16.f / 9.f, true);
    // 1.5 horizontal slope: outside standard/tall, inside a genuinely wide
    // target. The vertical FOV must not widen with aspect.
    select(placement.x - 150, center_y, placement.z + 100, 0, 16.f / 9.f, false);
    select(placement.x - 150, center_y, placement.z + 100, 0, 3, true);
    select(placement.x - 150, center_y, placement.z + 100, 0, .6f, false);
    select(placement.x, center_y - 100, placement.z + 150, 0, 3, false);
    // Real geometry at 0.8 vertical slope is outside the normal square view
    // but inside a 90 degree cube face. Verify submissions, not merely pixels;
    // the cast-shadow set stays exactly the same in both projections.
    select(placement.x,center_y+80,placement.z+100,0,1,false);
    select(placement.x,center_y+80,placement.z+100,0,1,true,1);
    select(placement.x-80,center_y+80,placement.z+100,0,1,true,1);
    select(placement.x-80,center_y+80,placement.z+100,0,1,false);
    // The main view misses vertically, but its reflected camera sees the prop.
    // The placement's center is outside the main VP's upper plane by >20%.
    const double mirror_y = -center_y - 40;
    const Matrix main_vp = view_projection(0, .5f);
    double dy = center_y - mirror_y, dz = -180;
    double clip_y = main_vp[5] * dy + main_vp[9] * dz;
    double clip_w = main_vp[7] * dy + main_vp[11] * dz;
    assert(clip_y > 1.2 * clip_w);
    select(placement.x, mirror_y, placement.z + 180, .5f, 16.f / 9.f, true);
    glDisable(GL_RASTERIZER_DISCARD); glDeleteQueries(1, &query);
    world_scenery_free(scene);
    // A raised local reflection has a different camera than the sea mirror.
    // This real lantern is outside the old two-frustum selection (the CPU
    // negative control independently removes the pool frustum and must fail).
    const auto *pool=world_atlas_rainforest_pool();assert(pool);
    double u,v;world_atlas_world_to_uv(pool->x+30,pool->z,&u,&v);
    {std::ofstream file(path);file.precision(15);file<<"starting_lantern "<<u<<' '<<v<<" 0 1 0 3\n";}
    scene=world_scenery_load(path.c_str(),models.c_str(),error,sizeof(error));assert(scene);
    std::filesystem::remove(path);
    glEnable(GL_RASTERIZER_DISCARD);glGenQueries(1,&query);
    select(pool->x+30,pool->surface_y+1.75,pool->z+20,-.8f,16.f/9.f,true);
    glDisable(GL_RASTERIZER_DISCARD);glDeleteQueries(1,&query);
    world_scenery_free(scene); assert(glGetError() == GL_NO_ERROR);
    std::puts("Visible prefix: offscreen color primitives0 / shadow retained, wide+tall aspects, fixed vertical FOV, sea/raised-pool mirrored-only visibility, stable uploads PASS");
}
}

int main(int argc, char **argv) {
    std::string root = std::filesystem::absolute(argc > 1 ? argv[1] : ".").lexically_normal().string();
    std::string instances = root + "/projects/valwood_world/scenery/authored_instances.txt";
    std::string models = root + "/projects/valwood_world/models";
    assert(world_atlas_init());
    if (!std::getenv("VALWOOD_TEST_NATIVE_GL")) SDL_setenv("SDL_VIDEODRIVER", "offscreen", 1);
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window = SDL_CreateWindow("Authored scenery test", 0, 0, extent, target_height, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(window);
    SDL_GLContext context = SDL_GL_CreateContext(window); assert(context);
    std::printf("Authored scenery renderer: %s\n", glGetString(GL_RENDERER));

    // Force a real partial-load failure after two valid models; all shared
    // texture ownership must be released before retrying the complete file.
    auto temporary = std::filesystem::temp_directory_path() / ("valwood-scenery-gl-" + std::to_string(getpid()));
    std::filesystem::create_directory(temporary);
    for (const char *name : {"cinematic_pine_near.glb"})
        std::filesystem::create_symlink(std::filesystem::path(models) / name, temporary / name);
    auto one = temporary / "single.txt";
    { std::istringstream all(read(instances)); std::string line;
      std::ofstream out(one);
      while (std::getline(all, line)) if (line.rfind("pine ", 0) == 0) { out << line << '\n'; break; } }
    char error[1024] = {};
    assert(!world_scenery_load(one.c_str(), temporary.c_str(), error, sizeof(error)));
    assert(std::strstr(error,"shadow proxy")&&std::strstr(error,"pine_mid.glb"));empty_cache();
    std::filesystem::create_symlink(std::filesystem::path(models)/"cinematic_pine_mid.glb",temporary/"cinematic_pine_mid.glb");
    assert(!world_scenery_load(one.c_str(), temporary.c_str(), error, sizeof(error)));
    assert(std::strstr(error, "pine_far.glb")); empty_cache();
    std::filesystem::remove_all(temporary);

    WorldScenery *scene = world_scenery_load(instances.c_str(), models.c_str(), error, sizeof(error));
    if (!scene) std::fprintf(stderr, "Actual-bounds authoring validation failed: %s\n", error);
    assert(scene);
    auto source = world_scenery_stats(scene);
    assert(source.source_instances == 283 && source.loaded_groups == 21);
    // Independent denser terrain scan over the actual loaded model bounds.
    // A root-height-only check cannot detect a 92m wall floating downhill.
    float max_support_gap = -1.e30f, old_cliff_gap = -1.e30f;
    std::size_t rigid_count = 0;
    for (std::size_t k = 0; k < source.source_instances; ++k) {
        WorldSceneryPlacement a; assert(world_scenery_placement(scene, k, &a));
        if (!a.rigid) continue;
        ++rigid_count;
        int steps_x = std::max(2, int(std::ceil((a.maximum[0] - a.minimum[0]) * a.scale / .5)));
        int steps_z = std::max(2, int(std::ceil((a.maximum[2] - a.minimum[2]) * a.scale / .5)));
        double c = std::cos(a.yaw), s = std::sin(a.yaw);
        for (int iz = 0; iz <= steps_z; ++iz) for (int ix = 0; ix <= steps_x; ++ix) {
            double x = (a.minimum[0] + double(ix) / steps_x * (a.maximum[0] - a.minimum[0])) * a.scale;
            double z = (a.minimum[2] + double(iz) / steps_z * (a.maximum[2] - a.minimum[2])) * a.scale;
            float terrain = world_atlas_height(a.x + c * x + s * z, a.z - s * x + c * z);
            max_support_gap = std::max(max_support_gap, a.y - terrain);
            if (!std::strcmp(a.alias, "cliff"))
                old_cliff_gap = std::max(old_cliff_gap, world_atlas_height(a.x, a.z) - a.burial - terrain);
        }
    }
    assert(rigid_count == 89 && max_support_gap <= .015f && old_cliff_gap > 2.f);
    std::printf("Actual-bounds grounding: %zu props, maximum support gap %.3fm; old center-only cliff gap %.3fm\n", rigid_count, max_support_gap, old_cliff_gap);
    world_scenery_free(scene);empty_cache();
    std::string starting_instances=root+"/projects/valwood_world/scenery/starting_area_instances.txt";
    const char *source_paths[]={instances.c_str(),starting_instances.c_str()};
    scene=world_scenery_load_files(source_paths,2,models.c_str(),error,sizeof(error));
    if(!scene)std::fprintf(stderr,"Combined actual-model starting-area load failed: %s\n",error);
    assert(scene);auto combined=world_scenery_stats(scene);
    auto baseline_source=world_scenery_source_stats(scene,0),starting_source=world_scenery_source_stats(scene,1);
    assert(combined.source_files==2&&baseline_source.instances==283&&baseline_source.aliases==12);
    assert(starting_source.instances>=500&&starting_source.instances<=8192-baseline_source.instances&&starting_source.aliases>=14&&starting_source.aliases<=26);
    std::set<std::string> starting_families;
    for(std::size_t k=0;k<combined.source_instances;k++) {
        WorldSceneryPlacement p;assert(world_scenery_placement(scene,k,&p));
        if(p.source_file==1&&std::strncmp(p.alias,"starting_",9)==0)starting_families.insert(p.alias);
    }
    assert(starting_families.size()==14);
    assert(combined.source_instances==283+starting_source.instances&&combined.source_aliases==26&&combined.loaded_groups==39);
    assert(combined.source_colliders==baseline_source.colliders+starting_source.colliders&&starting_source.colliders>10);
    route_clearance(scene,read(root+"/projects/valwood_world/scenery/starting_area_layout.json"));
    std::printf("Combined starting scene: %zu baseline +%zu starting records, %zu families/%zu groups, %zu colliders\n",
        baseline_source.instances,starting_source.instances,combined.source_aliases,combined.loaded_groups,combined.source_colliders);
    // Keep the original two-source load isolated above. A third source reuses
    // its model families/groups, without changing either earlier source count.
    const std::string rainforest_instances=root+"/projects/valwood_world/scenery/rainforest_instances.txt";
    const bool has_rainforest=std::filesystem::is_regular_file(rainforest_instances);
    if(has_rainforest) {
        const auto previous=combined;const auto expected=source_records(rainforest_instances);
        assert(expected.instances>=200&&expected.instances<=8192-previous.source_instances);
        std::set<std::string> existing_aliases;
        for(std::size_t k=0;k<previous.source_instances;++k){WorldSceneryPlacement p;assert(world_scenery_placement(scene,k,&p));existing_aliases.insert(p.alias);}
        std::set<std::string> new_aliases;
        for(const auto &alias:expected.aliases)if(!existing_aliases.count(alias)){
            assert(alias=="showcase_roots"||alias=="showcase_stump"||alias=="showcase_shrub"||alias=="showcase_shrub_b"||alias=="showcase_sorrel");
            new_aliases.insert(alias);
        }
        assert(new_aliases.size()==5);
        world_scenery_free(scene);empty_cache();
        const char *all_sources[]={instances.c_str(),starting_instances.c_str(),rainforest_instances.c_str()};
        scene=world_scenery_load_files(all_sources,3,models.c_str(),error,sizeof(error));
        if(!scene)std::fprintf(stderr,"Combined actual-model rainforest load failed: %s\n",error);
        assert(scene);combined=world_scenery_stats(scene);auto rain=world_scenery_source_stats(scene,2);
        const auto baseline_again=world_scenery_source_stats(scene,0),starting_again=world_scenery_source_stats(scene,1);
        assert(combined.source_files==3&&combined.source_instances==previous.source_instances+expected.instances&&combined.shadow_proxy_groups==6);
        assert(baseline_again.instances==baseline_source.instances&&baseline_again.aliases==baseline_source.aliases&&baseline_again.colliders==baseline_source.colliders);
        assert(starting_again.instances==starting_source.instances&&starting_again.aliases==starting_source.aliases&&starting_again.colliders==starting_source.colliders);
        assert(rain.instances==expected.instances&&rain.aliases==expected.aliases.size()&&rain.trees==expected.trees&&rain.props==expected.props);
        assert(combined.source_aliases==previous.source_aliases+new_aliases.size()&&combined.loaded_groups==previous.loaded_groups+2*new_aliases.size());
        assert(combined.source_colliders==previous.source_colliders+rain.colliders&&rain.colliders>0);
        std::size_t actual_count=0;for(std::size_t k=0;k<combined.source_instances;++k){WorldSceneryPlacement p;assert(world_scenery_placement(scene,k,&p));actual_count+=p.source_file==2;}
        assert(actual_count==expected.instances);rainforest_clearance(scene);
        std::printf("Third rainforest source: %zu records (%zu trees/%zu props), %zu aliases, %zu colliders; total%zu records/shared%zu groups\n",rain.instances,rain.trees,rain.props,rain.aliases,rain.colliders,combined.source_instances,combined.loaded_groups);
    } else std::puts("Rainforest actual-model fixture: optional third source absent; original two-source checks retained");
    const std::string village_instances=root+"/projects/valwood_world/scenery/castle_village_instances.txt";
    const bool has_village=std::filesystem::is_regular_file(village_instances);
    if(has_village&&has_rainforest) {
        const auto prior=combined;const auto expected=source_records(village_instances);
        world_scenery_free(scene);empty_cache();
        const char *all[]={instances.c_str(),starting_instances.c_str(),rainforest_instances.c_str(),village_instances.c_str()};
        scene=world_scenery_load_files(all,4,models.c_str(),error,sizeof(error));
        if(!scene)std::fprintf(stderr,"Combined village load failed: %s\n",error);
        assert(scene);combined=world_scenery_stats(scene);
        const auto village=world_scenery_source_stats(scene,3);
        assert(combined.source_files==4&&combined.source_instances==prior.source_instances+expected.instances);
        assert(village.instances==expected.instances&&village.props==expected.props&&village.trees==expected.trees);
        assert(village.colliders>100&&combined.source_instances<=8192);
        assert(world_scenery_source_stats(scene,0).instances==baseline_source.instances);
        assert(world_scenery_source_stats(scene,1).instances==starting_source.instances);
        double ox,oz;world_atlas_village_origin(&ox,&oz);
        std::size_t imported_buildings=0,imported_goods=0;
        for(std::size_t k=0;k<combined.source_instances;++k) {
            WorldSceneryPlacement a;assert(world_scenery_placement(scene,k,&a));
            if(std::strncmp(a.alias,"purchased_goods_",16)==0) {
                // The low inn tasting table and supported delivery stacks span
                // two heights. Builder QA independently ray-checks each mesh.
                assert(a.foundation&&!a.rigid&&a.elevation>.49f&&a.elevation<1.55f);
                assert(a.maximum[1]>.05f&&a.maximum[1]<.3f);++imported_goods;
            } else if(std::strncmp(a.alias,"purchased_house_",16)==0||std::strncmp(a.alias,"purchased_shop_",15)==0||std::strcmp(a.alias,"purchased_castle")==0) {
                assert(a.foundation&&!a.rigid);++imported_buildings;
                // Test actual loaded bounds/rotation against a full crossing.
                // These closed imports must not inherit prototype door holes.
                const double c=std::cos(a.yaw),s=std::sin(a.yaw),z=(a.minimum[2]+a.maximum[2])*.5*a.scale;
                const double ax=(a.minimum[0]*a.scale-1),bx=(a.maximum[0]*a.scale+1);
                assert(!world_scenery_can_move(scene,a.x+c*ax+s*z,a.z-s*ax+c*z,
                    a.x+c*bx+s*z,a.z-s*bx+c*z,24,1.75f,.37f));
            }
        }
        // The detailed demo expands the existing district. Require every
        // committed source placement, while retaining the original content
        // floor instead of pinning the old layout's smaller population.
        assert(imported_buildings==expected.imported_buildings&&imported_goods==expected.imported_goods);
        assert(imported_buildings>=60&&imported_goods>=234);
        std::printf("Purchased scene: %zu actual-bounds closed imports, %zu grounded tabletop goods with intact measured sizes PASS\n",imported_buildings,imported_goods);
        constexpr double path[][2]={{0,220},{0,130},{0,55},{0,-50},{0,-88},{0,-50},
            {0,55},{-95,55},{-95,125},{0,125},{0,220}};
        size_t checks=0;
        for(size_t i=1;i<std::size(path);++i) {
            const auto &a=path[i-1],&b=path[i];
            const int steps=int(std::ceil(std::hypot(b[0]-a[0],b[1]-a[1])/.25));
            for(int j=0;j<steps;++j) {
                const double t=double(j)/steps,u=double(j+1)/steps;
                double x=ox+a[0]+(b[0]-a[0])*t,z=oz+a[1]+(b[1]-a[1])*t;
                double nx=ox+a[0]+(b[0]-a[0])*u,nz=oz+a[1]+(b[1]-a[1])*u;
                float h=world_atlas_walk_height(x,z);
                if(!world_scenery_can_move(scene,x,z,nx,nz,h,1.75f,.37f))
                    std::fprintf(stderr,"Village route blocked at relative %.3f %.3f\n",x-ox,z-oz);
                assert(world_scenery_can_move(scene,x,z,nx,nz,h,1.75f,.37f));
                assert(std::abs(world_atlas_walk_height(nx,nz)-h)<.05f);++checks;
            }
        }
        std::printf("Fourth village source: %zu records/%zu colliders; %zu actual-bounds walking sweeps pass\n",village.instances,village.colliders,checks);
        size_t alley_checks=0;
        for(int step=0;step<344;++step)for(double lane:{-.8,0.,.8}) {
            const double x=ox+61.75+lane,z=oz+51.-step*.25;
            const float h=world_atlas_walk_height(x,z);
            assert(world_scenery_can_move(scene,x,z,x,z-.25,h,1.75f,.37f));
            assert(std::abs(world_atlas_walk_height(x,z-.25)-h)<.05f);++alley_checks;
        }
        assert(alley_checks==1032);
        std::printf("Merchants Lane:86m/%zu three-lane walking sweeps pass against actual imported bounds\n",alley_checks);
    }
    GLuint color_program = program(root, "world_foliage.fs.glsl"), shadow_program = program(root, "world_foliage_shadow.fs.glsl");
    GLuint fbo, color, depth;
    glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenTextures(1, &color); glBindTexture(GL_TEXTURE_2D, color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, extent, target_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
    glGenTextures(1, &depth); glBindTexture(GL_TEXTURE_2D, depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, extent, target_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    GLuint lit_shadow = constant_texture(1), macro_height = constant_texture(0);
    struct View { const char *id; double u, v; float yaw, pitch; bool authored; };
    const View views[] = {{"forest", .511, .252, -.55f, .025f, true}, {"lake", .454, .180, -.90f, .025f, true},
                          {"ridge", .29760, .088, .58905f, .040f, false}, {"coast", .522, .92525, 1.96350f, -.080f, true},
        {"courtyard",1096.006794000/world_atlas_width()+.5,-14092.302401000/world_atlas_depth()+.5,-0.030853886f,-0.100000000f,true},
        {"workshop",1100.071228000/world_atlas_width()+.5,-14133.502891000/world_atlas_depth()+.5,0.038002604f,-0.100000000f,true},
        {"overlook",1095.996140960/world_atlas_width()+.5,-14165.021036000/world_atlas_depth()+.5,0.301966327f,-0.060000000f,true},
        {"grove",1019.319435000/world_atlas_width()+.5,-14148.507819000/world_atlas_depth()+.5,0.377295218f,-0.080000000f,true},
        {"rainforest",.235-15./world_atlas_width(),.670+95./world_atlas_depth(),.2f,-.04f,has_rainforest},
        {"castle-village",.487,.320+220./world_atlas_depth(),0.f,.035f,has_village},
        {"castle-market",.487,.320+50./world_atlas_depth(),0.f,.025f,has_village},
        {"castle-courtyard",.487,.320-70./world_atlas_depth(),0.f,.07f,has_village}};
    std::vector<unsigned char> pixels(extent * target_height * 4);
    std::vector<float> depths(extent * target_height);
    for (const auto &view : views) {
        double x, z; world_atlas_uv_to_world(view.u, view.v, &x, &z);
        double y = world_atlas_height(x, z) + 1.75;
        world_scenery_update(scene, x, y, z, view.yaw, view.pitch);
        auto stats = world_scenery_stats(scene);
        std::size_t submitted = stats.near_trees + stats.mid_trees + stats.far_trees + stats.props;
        assert((submitted > 0) == view.authored && stats.largest_batch <= 4096);
        Matrix vp = view_projection(view.yaw, view.pitch);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, extent, target_height);
        glDisable(GL_BLEND); glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST);
        glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDrawBuffer(GL_COLOR_ATTACHMENT0); glReadBuffer(GL_COLOR_ATTACHMENT0);
        glClearColor(0, 0, 0, 1); glClearDepth(1); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(color_program);
        glUniformMatrix4fv(glGetUniformLocation(color_program, "uVP"), 1, GL_FALSE, vp.data());
        glUniformMatrix4fv(glGetUniformLocation(color_program, "uLightVP"), 1, GL_FALSE, vp.data());
        v3(color_program, "uSun", .3f, .8f, -.4f); v3(color_program, "uSunColor", 1, .9f, .7f);
        v3(color_program, "uAmbient", .4f, .45f, .5f); v3(color_program, "uFog", .4f, .5f, .6f);
        f(color_program, "uHaze", .0001f); f(color_program, "uTime", 17); f(color_program, "uTemporalPhase", 0);
        f(color_program, "uPixelScale", target_height / (2 * std::tan(.54f)));
        glUniform2f(glGetUniformLocation(color_program, "uWorldSize"), float(world_atlas_width()), float(world_atlas_depth()));
        glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, lit_shadow); i(color_program, "uShadow", 7);
        glActiveTexture(GL_TEXTURE11); glBindTexture(GL_TEXTURE_2D, macro_height); i(color_program, "uMacroHeight", 11);
        world_scenery_draw(scene, color_program, x, y, z);
        glReadPixels(0, 0, extent, target_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        std::size_t colored = 0;
        for (std::size_t k = 0; k < pixels.size(); k += 4) if (pixels[k] || pixels[k + 1] || pixels[k + 2]) ++colored;
        if (view.authored) assert(colored > 3);
        glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE); glClear(GL_DEPTH_BUFFER_BIT);
        glUseProgram(shadow_program);
        glUniformMatrix4fv(glGetUniformLocation(shadow_program, "uVP"), 1, GL_FALSE, vp.data());
        f(shadow_program, "uTime", 17); f(shadow_program, "uTemporalPhase", 0);
        world_scenery_draw(scene, shadow_program, x, y, z, true);
        glReadPixels(0, 0, extent, target_height, GL_DEPTH_COMPONENT, GL_FLOAT, depths.data());
        std::size_t covered = 0;
        for (float value : depths) { assert(std::isfinite(value) && value >= 0 && value <= 1); if (value < .999999f) ++covered; }
        if (view.authored) assert(covered > 3);
        assert(glGetError() == GL_NO_ERROR);
        world_scenery_update(scene, x, y, z, view.yaw, view.pitch);
        assert(world_scenery_stats(scene).uploads_this_update == 0);
        std::printf("%s: %zu submitted, %zu color pixels, %zu shadow pixels\n", view.id, submitted, colored, covered);
    }
    visible_prefix(root, models, color_program, shadow_program);
    world_scenery_free(scene); empty_cache();
    shadow_proxies(root,models,color_program,shadow_program);
    purchased_environment(root,models,color_program);
    glDeleteProgram(color_program); glDeleteProgram(shadow_program);
    glDeleteTextures(1, &lit_shadow); glDeleteTextures(1, &macro_height);
    glDeleteTextures(1, &color); glDeleteTextures(1, &depth); glDeleteFramebuffers(1, &fbo);
    assert(glGetError() == GL_NO_ERROR);
    SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit(); world_atlas_shutdown();
    std::puts("PASS authored scenery GL: actual asset bounds and trail clearance, partial-load cleanup, isolated two-source scene and optional third rainforest/source route, nine destinations, color/shadow shaders, stable uploads, zero GL errors/cache owners");
}

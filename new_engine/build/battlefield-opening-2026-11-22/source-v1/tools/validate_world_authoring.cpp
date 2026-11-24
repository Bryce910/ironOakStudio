// Native CPU schema validation for packages. No SDL, GPU context or user saves.
#include "engine/world_studio.h"
#include "engine/world_prefab.h"
#include "engine/world_living.h"
#include "engine/world_micro_policy.h"
#include "engine/world_npc_memory.h"
#include "engine/world_npc_appearance.h"
#include "engine/world_cinematics.h"
#include "engine/world_regions.h"
#include "engine/world_physics.h"
#include "engine/world_gameplay.h"
#include "engine/world_runtime_components.h"
#include "engine/world_runtime_demo.h"
#include "engine/world_animation.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

// This private editor validator advertises landscape-v1 only when compiled
// against the expanded terrain and persisted ecology schema used by the editor.
static_assert(world_studio::tile_limit==64,"landscape-v1 requires 64 terrain tiles");
static_assert(WORLD_COVER_REGION_LIMIT==32,"landscape-v1 requires 32 ecology patches");
static_assert(world_weather_authoring::area_limit==16&&unsigned(world_entity::Kind::Weather)==9,"weather-authoring-v1 requires bounded durable weather areas");

static std::string quoted(const std::string &text){
    std::string out="\"";for(unsigned char c:text){if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c=='\n')out+="\\n";else if(c=='\r')out+="\\r";else if(c=='\t')out+="\\t";else if(c<32)out+='?';else out+=char(c);}return out+'"';
}
static bool local_regular(const std::filesystem::path &root,const std::filesystem::path &path,std::string &error){
    const auto relative=path.lexically_relative(root);if(relative.empty()||relative.is_absolute()){error="Authoring dependency leaves the project";return false;}
    std::error_code ec;auto at=root;
    for(const auto &part:relative){if(part==".."||part=="."||part.empty()){error="Authoring dependency contains traversal";return false;}at/=part;const auto status=std::filesystem::symlink_status(at,ec);if(ec||std::filesystem::is_symlink(status)){error="Authoring dependency is missing or traverses a symlink";return false;}}
    if(!std::filesystem::is_regular_file(at,ec)||ec){error="Authoring dependency is not a regular project file";return false;}return true;
}
// Runtime asset banks may be shared through project symlinks, just like the
// shipped rig/model checks below. Package inventory enforces their portable
// ownership and materializes them; local_regular remains for authored files.
static bool sound_dependencies(const std::filesystem::path &root,const world_studio::Document &document,std::string &error){
    for(const auto &sound:document.sounds){
        if(sound.asset.empty())continue; // Unassigned editor marker.
        if(!world_soundscape::asset_path(sound.asset)){error="Invalid placed Sound WAV path: "+sound.asset;return false;}
        std::error_code ec;const auto file=root/std::filesystem::u8path(sound.asset);
        if(!std::filesystem::is_regular_file(file,ec)||ec){error="Missing placed Sound WAV dependency: "+sound.asset;return false;}
    }
    return true;
}
// Weather has no external file dependency, but runtime sampling requires its
// authoritative stable entity binding (prefabs keep that graph separately).
static bool weather_authoring(const world_studio::Document &document,const world_entity::Graph &graph,std::string &error){
    if(!world_weather_authoring::valid(document.weather_global,error))return false;
    if(document.weather_areas.size()>world_weather_authoring::area_limit){error="Weather area budget exceeds16";return false;}
    if(!world_entity::validate(graph,&document,error))return false;
    for(unsigned i=0;i<document.weather_areas.size();++i){
        if(!world_weather_authoring::valid(document.weather_areas[i],error))return false;
        const auto found=std::find_if(graph.nodes.begin(),graph.nodes.end(),[&](const auto &node){return node.binding==world_entity::Binding{world_entity::Kind::Weather,i};});
        if(found==graph.nodes.end()){error="Weather area is missing its persistent entity binding";return false;}
    }return true;
}
static bool scene_entities(const world_studio::Document &document,std::vector<world_runtime::Entity> &out,std::string &error){
    if(!world_entity::validate(document.entities,&document,error))return false;
    std::vector<std::pair<world_entity::Id,world_entity::Transform>> transforms;
    if(!world_entity::world_transforms(document.entities,transforms,error))return false;
    for(const auto &entry:transforms){const auto *node=world_entity::find(document.entities,entry.first);if(!node){error="Persistent scene node disappeared during validation";return false;}
        bool active=true;for(const auto *parent=node;parent;parent=parent->parent?world_entity::find(document.entities,parent->parent):nullptr)active&=parent->visible;
        if(node->binding.kind==world_entity::Kind::Actor)active&=document.actors[node->binding.index].enabled;
        if(node->binding.kind==world_entity::Kind::Weather)active&=document.weather_areas[node->binding.index].enabled;
        if(node->binding.kind==world_entity::Kind::SourceModel)for(const auto &edit:document.overrides)if(edit.index==node->binding.index)active&=!edit.hidden;
        const auto &t=entry.second;const double half_angle=t.yaw*.008726646259971648;
        world_runtime::Entity entity;entity.id=entry.first;entity.pose.position={t.x,t.y,t.z};entity.pose.rotation={0,float(std::sin(half_angle)),0,float(std::cos(half_angle))};entity.scale=t.scale;entity.active=active;out.push_back(entity);
    }return true;
}
static bool atlas_project(const std::filesystem::path &root){
    std::ifstream file(root/"project.valwood");std::string line;std::size_t bytes=0;
    while(std::getline(file,line)&&((bytes+=line.size())<=65536)){const auto split=line.find('=');if(split==std::string::npos)continue;auto key=line.substr(0,split),value=line.substr(split+1);auto trim=[](std::string &s){const auto begin=s.find_first_not_of(" \t\r"),end=s.find_last_not_of(" \t\r");s=begin==std::string::npos?"":s.substr(begin,end-begin+1);};trim(key);trim(value);if(key=="terrain")return value=="valwood_atlas";}
    return false;
}
static bool knight_asset(const std::filesystem::path &path,std::string &error){
    world_animation::Asset asset;if(!world_animation::load_glb(path.u8string().c_str(),asset,error))return false;
    for(const char *name:{"Walking","Running","Skill_03"}){const auto count=std::count_if(asset.clips.begin(),asset.clips.end(),[&](const auto &c){return c.name==name&&c.duration>0;});if(count!=1){error="Shipped knight must contain exact unique Walking / Running / Skill_03 clips";return false;}}
    auto node=[&](const char *name){int found=-1;for(unsigned i=0;i<asset.skeleton.joints.size();i++)if(asset.skeleton.joints[i].name==name){if(found>=0)return -1;found=int(i);}return found;};
    for(const char *name:{"Hips","RightHand","LeftHand","Head"})if(node(name)<0){error="Shipped knight is missing an unambiguous authored root/socket joint";return false;}
    for(const auto &names:{std::array<const char *,3>{"LeftUpLeg","LeftLeg","LeftFoot"},std::array<const char *,3>{"RightUpLeg","RightLeg","RightFoot"}}){const int hip=node(names[0]),knee=node(names[1]),ankle=node(names[2]);if(hip<0||knee<0||ankle<0||asset.skeleton.joints[unsigned(knee)].parent!=hip||asset.skeleton.joints[unsigned(ankle)].parent!=knee){error="Shipped knight is missing its authored two-bone terrain IK chain";return false;}}
    return true;
}
static bool authoring_file(const std::filesystem::path &path){
    const auto extension=path.extension().u8string();
    return extension==".studio"||extension==".vwprefab"||extension==".living"||extension==".vct"||extension==".vwml"||extension==".vwmemory"||extension==".vwlooks"||extension==".vwminds"||extension==".regions"||extension==".physics"||extension==".gameplay"||extension==".components";
}
int main(int argc,char **argv){
    namespace fs=std::filesystem;
    if(argc!=3||std::string(argv[1])!="--project"){std::cout<<"{\"ok\":false,\"error\":\"Usage: valwood_project_validate --project ABSOLUTE_PROJECT\"}\n";return 2;}
    // Windows main argv follows the active CRT code page. Parse the original
    // UTF-16 command line so project paths preserve every Unicode character.
    fs::path requested;
#ifdef _WIN32
    int wide_count=0;wchar_t **wide=CommandLineToArgvW(GetCommandLineW(),&wide_count);
    if(!wide||wide_count!=3){if(wide)LocalFree(wide);std::cout<<"{\"ok\":false,\"error\":\"Cannot read Unicode project argument\"}\n";return 2;}
    requested=fs::path(wide[2]);LocalFree(wide);
#else
    requested=fs::u8path(argv[2]);
#endif
    std::string error;std::error_code ec;const fs::path root=fs::canonical(requested,ec);unsigned checked=0,components_checked=0,skeletal_checked=0,seed_entities=0;
    auto fail=[&](const std::string &file){std::cout<<"{\"ok\":false,\"file\":"<<quoted(file)<<",\"error\":"<<quoted(error)<<"}\n";return 1;};
    if(ec||!fs::is_directory(root)){error="Project directory unavailable";return fail(requested.u8string());}
    if(!local_regular(root,root/"project.valwood",error))return fail("project.valwood");
    world_studio::Document scene;const auto scene_path=root/"world.studio";
    if(fs::exists(scene_path,ec)&&(!local_regular(root,scene_path,error)||!world_studio::load(scene_path.u8string().c_str(),scene,error)))return fail("world.studio");
    std::vector<world_runtime::Entity> entities;if(!scene_entities(scene,entities,error))return fail("world.studio");
    const unsigned authored_entities=unsigned(entities.size());const bool atlas=atlas_project(root);
    world_living::World authored_living;bool have_living=false;
    const auto living_path=root/"world.living";if(fs::exists(living_path,ec)){if(!local_regular(root,living_path,error)||!authored_living.load(living_path.u8string(),error))return fail("world.living");have_living=true;}
    world_runtime::Configuration mind_configuration;
    if(atlas){
        // Exactly the same separate fixed registry as the viewer; no new IDs are
        // inserted into the user's Document or inferred from array indices.
        world_runtime_demo::Seed seed;if(!world_runtime_demo::build({},entities,authored_living,!have_living,0,seed,error))return fail("built-in runtime registry");
        seed_entities=unsigned(seed.markers.nodes.size());entities=seed.configuration.entities;mind_configuration=seed.configuration;mind_configuration.external_living=nullptr;mind_configuration.living=authored_living;
        if(!fs::exists(root/"world.components",ec)){world_runtime::Runtime runtime;if(!runtime.configure(seed.configuration,error))return fail("built-in runtime components");}
        const auto rig=root/"models/animations/white_knight_brown_armor.glb";
        if(!fs::is_regular_file(rig,ec)||!knight_asset(rig,error)){if(error.empty())error="Missing shipped live knight skeletal asset";return fail("models/animations/white_knight_brown_armor.glb");}skeletal_checked++;
        const auto crate=root/"models/purchased_village_crate_near.glb";if(!fs::is_regular_file(crate,ec)){error="Missing shipped live supply-crate model";return fail("models/purchased_village_crate_near.glb");}
        const auto shader=root/"shaders/world_skeletal.vs.glsl";if(!fs::is_regular_file(shader,ec)){error="Missing shipped skeletal vertex shader";return fail("shaders/world_skeletal.vs.glsl");}
        if(!fs::is_regular_file(root/"shaders/world_dynamic.vs.glsl",ec)){error="Missing shipped dynamic rigid-body vertex shader";return fail("shaders/world_dynamic.vs.glsl");}
    }
    auto validate=[&](const fs::path &path){
        const auto extension=path.extension().u8string();
        bool ok=true;
        if(authoring_file(path)&&!local_regular(root,path,error))return false;
        if(extension==".studio"){world_studio::Document d;ok=world_studio::load(path.u8string().c_str(),d,error)&&sound_dependencies(root,d,error)&&weather_authoring(d,d.entities,error);}
        else if(extension==".vwprefab"){world_prefab::Prefab d;ok=world_prefab::load(path.u8string().c_str(),d,error)&&sound_dependencies(root,d.content,error)&&weather_authoring(d.content,d.graph,error);}
        else if(extension==".living"){world_living::World d;ok=d.load(path.u8string(),error);}
        else if(extension==".vct"){world_cinematics::Document d;ok=world_cinematics::load(path.u8string().c_str(),d,error);}
        else if(extension==".vwml"){world_micro_policy::Model d;ok=world_micro_policy::load(path.u8string().c_str(),d,error);}
        else if(extension==".vwmemory"){world_npc_memory::Bank d;ok=d.load(path.u8string().c_str(),error);}
        else if(extension==".vwminds"){
            world_runtime::Configuration configuration=mind_configuration;
            if(fs::exists(root/"world.components",ec)){world_runtime::Document d;ok=world_runtime::load((root/"world.components").u8string().c_str(),d,error)&&world_runtime::load_project(root.u8string(),d,entities,0,configuration,error);}
            else if(!atlas){configuration.entities=entities;}
            world_runtime::Runtime runtime;if(ok)ok=runtime.configure(configuration,error)&&runtime.load_minds(path.u8string().c_str(),error);
        }
        else if(extension==".vwlooks"){
            std::vector<world_npc_appearance::Binding> bindings;ok=world_npc_appearance::load_bindings(path.u8string(),bindings,error);
            for(const auto &binding:bindings){if(!ok)break;fs::path memory=binding.file;if(memory.is_relative())memory=root/memory;world_npc_memory::Bank d;ok=local_regular(root,memory,error)&&d.load(memory.u8string().c_str(),error);}
        }
        else if(extension==".regions"){world_regions::Document d;ok=world_regions::load(path.u8string().c_str(),d,error);}
        else if(extension==".physics"){world_physics::Document d;ok=world_physics::load(path.u8string().c_str(),d,error);}
        else if(extension==".gameplay"){world_gameplay::Document d;ok=world_gameplay::load(path.u8string().c_str(),d,error);}
        else if(extension==".components"){
            world_runtime::Document d;world_runtime::Configuration configuration;
            ok=world_runtime::load(path.u8string().c_str(),d,error)&&world_runtime::load_project(root.u8string(),d,entities,0,configuration,error);
            if(ok){world_runtime::Runtime runtime;ok=runtime.configure(configuration,error);}
            if(ok)components_checked++;
        }
        else return true;
        checked++;return ok;
    };
    for(const char *name:{"world.studio","world.living","world.regions","world.physics","world.gameplay","world.components"}){const auto path=root/name;if(fs::exists(path,ec)&&!validate(path))return fail(name);}
    unsigned entries=0;
    for(const char *folder:{"prefabs","cinematics","ai","assets","models","scenery","ground","particles","audio","sounds","shaders","fonts","environments","texture_blocks","imports","textures","surfaces"}){
        const auto path=root/folder;if(!fs::exists(path,ec))continue;
        if(fs::is_symlink(fs::symlink_status(path,ec))){
            if(std::string(folder)=="prefabs"||std::string(folder)=="cinematics"||std::string(folder)=="ai"||std::string(folder)=="assets"){error="Authoring asset folders must be local directories";return fail(folder);}
            continue; // Trusted cooked banks are materialized by the inventory.
        }
        fs::recursive_directory_iterator at(path,fs::directory_options::none,ec),end;
        for(;!ec&&at!=end;at.increment(ec)){
            if(++entries>65536){error="Runtime asset directories exceed65536 entries";return fail(folder);}
            if(at->is_symlink(ec)&&authoring_file(at->path())){error="Production authoring assets must be local regular files";return fail(at->path().u8string());}
            if(at->is_regular_file(ec)&&!validate(at->path()))return fail(at->path().lexically_relative(root).u8string());
        }
        if(ec){error=ec.message();return fail(folder);}
    }
    std::cout<<"{\"ok\":true,\"checked_documents\":"<<checked<<",\"components_validated\":"<<components_checked<<",\"authored_entities\":"<<authored_entities<<",\"seed_entities\":"<<seed_entities<<",\"skeletal_assets_validated\":"<<skeletal_checked<<",\"capabilities\":[\"typed-components-v1\",\"scene-id-closure-v1\",\"skeletal-knight-v1\",\"landscape-v1\",\"weather-authoring-v1\"]}\n";return 0;
}

#ifndef VALWOOD_WORLD_STUDIO_H
#define VALWOOD_WORLD_STUDIO_H
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "engine/world_runtime_editors.h"
#include "engine/world_studio_cinematics.h"
#include "engine/world_shader_graph.h"
#include "engine/world_entity_graph.h"
#include "engine/world_surface_authoring.h"
#include "engine/world_authoring_limits.h"
#include "engine/world_ecology.h"
#include "engine/world_structure_splines.h"
#include "engine/world_soundscape.h"
#include "engine/world_weather_authoring.h"
namespace world_surface {struct TerrainField;}

// CPU document shared by the streamed-world tools. Runtime readers see only
// committed data; terrain workers must be joined before commit/load/undo.
namespace world_studio {
constexpr unsigned tile_side=65,tile_limit=64,placement_limit=1024,light_limit=24,stamp_limit=32;
using Ecology=WorldCoverRegion;
struct TerrainTile {int x=0,z=0;std::array<float,tile_side*tile_side> delta={};};
struct Placement {std::string alias;double x=0,z=0;float yaw=0,scale=1,burial=0,elevation=0,offset_y=0,pitch=0,roll=0,sx=1,sy=1,sz=1;bool wind_enabled=true;float wind_sway=1,wind_flutter=1;};
struct Override {unsigned index=0;double x=0,z=0;float yaw=0,scale=1;bool hidden=false;float offset_y=0,pitch=0,roll=0,sx=1,sy=1,sz=1;bool wind_enabled=true;float wind_sway=1,wind_flutter=1;};
// Source keys0..65535 retain the source signature; added keys start at65536.
// Legacy v1-v14 persisted group keys use16384 and are migrated on load.
// Erasing an added object must also remap group keys through erase_placement.
struct Group {std::string name;std::vector<unsigned> members;};
struct Asset {std::string alias,stem;unsigned lods=1,collision=1,vegetation=0;float distance=220,split=60,units=1;};
struct Environment {std::string path;float rotation=0,intensity=1,strength=1;bool enabled=false;};
struct Light {double x=0,y=0,z=0;float radius=8,power=4;std::array<float,3> color={1,.65f,.35f};float yaw=0,pitch=-45,cone=180;bool flicker=false;};
struct Stamp {double x=0,z=0;float radius=8,value=0;int kind=0;}; // vegetation suppression: kind0..3
struct Material {std::string alias;std::array<float,3> tint={1,1,1};float roughness=1,normal=1,uv=1;};
struct Decal {double x=0,y=0,z=0;std::array<float,3> size={2,2,2},color={.16f,.19f,.09f};float yaw=0,opacity=.5f,roughness=.9f;int kind=0;};
struct Spline {std::string alias;float spacing=3,scale=1,width=2;int kind=0;std::vector<std::array<double,2>> points;};
struct Look {float sharpness=.28f,sun=1,sky=1,exposure=1,hour=17.05f,bearing=0,cloud=.28f,rain=.48f,wind=.85f,fog=.62f;};
struct Document {world_entity::Graph entities;std::string source_signature;Look look;std::vector<TerrainTile> terrain;std::vector<Placement> placements;std::vector<Override> overrides;std::vector<Light> lights;std::vector<Stamp> stamps;std::vector<Ecology> ecology;std::vector<Material> materials;std::vector<Decal> decals;std::vector<Spline> splines;std::vector<Group> groups;std::vector<Asset> assets;Environment environment;std::vector<NavigationArea> navigation;std::vector<Actor> actors;std::vector<Particle> particles;std::vector<world_soundscape::Sound> sounds;world_weather_authoring::Global weather_global;std::vector<world_weather_authoring::Area> weather_areas;std::vector<Water> waters;std::vector<world_graph::Graph> graphs;world_surface::Data surfaces;std::shared_ptr<const world_surface::TerrainField> surface_field;std::vector<world_structure::Definition> structures;world_structure::Id next_structure_id=1;};
bool valid(const Document &,std::string &error);
bool encode(const Document &,std::string &text,std::string &error);
// Explicit Apply/Undo classification. No terrain copies or runtime work;
// ignores Sound records/nodes, groups exclusively owning them, and issued IDs.
bool sound_only_change(const Document &,const Document &);
// Sound + authored weather only; all other world data stays exact.
bool environment_only_change(const Document &,const Document &);
bool save(const char *path,const Document &,std::string &error);
bool load(const char *path,Document &,std::string &error);
float height_delta(const Document &,double x,double z);
float vegetation(const Document &,int kind,double x,double z);
// All-or-nothing brush: works on a draft, bounded64*128m editable tiles.
// mode0 raise,1 lower,2 flatten,3 smooth. base returns UNEDITED terrain.
bool brush(Document &,double x,double z,float radius,float amount,int mode,float target,
           float (*base)(double,double),std::string &error);
// One draft transaction: blend a 10m bank into the terrain, then carve a 6m
// basin. The water plane is below the bank; runtime still validates its rim.
bool carve_pond(Document &,double x,double z,float (*base)(double,double),std::string &error);
std::vector<Placement> scatter(const char *alias,double x,double z,float radius,unsigned count,float scale,std::uint32_t seed);
bool build_spline(const Spline &,std::vector<Placement> &,std::string &error);
// Sorted importance scores; ties must not black out a cluster at the8-light cap.
std::array<float,8> light_weights(const std::array<float,32> &,unsigned count);
std::array<float,8> light_weights(const std::array<float,64> &,unsigned count);
struct History {
    std::vector<Document> back,forward;
    void checkpoint(const Document &);bool undo(Document &);bool redo(Document &);
};
}
#endif

#ifndef VALWOOD_WORLD_SURFACE_AUTHORING_H
#define VALWOOD_WORLD_SURFACE_AUTHORING_H
#include <array>
#include <string>
#include <vector>
#include <cstdint>

// CPU authoring and derived fields. No GL, model loads or global mutations.
// The caller commits Data/Prepared together after all resources validate.
namespace world_surface {
constexpr unsigned image_limit=16,layer_limit=8,paint_limit=256,paint_tile_limit=64;
constexpr unsigned paint_side=65,paint_hash_size=128,projection_limit=64,wind_limit=16,ribbon_limit=16;
enum class ImageRole : unsigned {Color,Normal,ARM};
struct Image {std::string name,path;ImageRole role=ImageRole::Color;};
struct Layer {
    std::string name="Material";int color=-1,normal=-1,arm=-1;
    std::array<float,3> tint={1,1,1};float metres=1,roughness=.8f,metallic=0,normal_strength=1;bool flip_normal_y=false;
};
enum class Blend : unsigned {Constant,Noise,Height,Slope};
struct Binding {std::string alias;int first=-1,second=-1;Blend mode=Blend::Constant;float amount=1,scale=1,threshold=0,softness=.25f;};
struct Paint {double x=0,z=0;float radius=4,amount=.5f;int layer=0;bool erase=false;};
struct Projection {
    std::string name="Image decal";int layer=0;
    std::array<double,3> position={};std::array<float,3> size={2,1,2},rotation={};
    float opacity=1,feather=.15f,normal_cutoff=.1f;
};
struct Wind {
    std::string name="Wind";std::array<double,3> center={};
    std::array<float,3> radius={12,8,12};std::array<float,2> direction={1,0};
    float speed=1,gust=.3f,period=8;std::uint32_t seed=1;
};
enum class RibbonKind : unsigned {Road,River};
struct Point {double x=0,y=0,z=0;};
struct Ribbon {std::string name="Road";RibbonKind kind=RibbonKind::Road;int layer=-1;float width=3,shoulder=1,depth=1;bool smooth=true;std::vector<Point> points;};
struct Data {std::vector<Image> images;std::vector<Layer> layers;std::vector<Binding> bindings;std::vector<Paint> paint;std::vector<Projection> projections;std::vector<Wind> winds;std::vector<Ribbon> ribbons;};
bool validate(const Data &,std::string &error);
bool encode(const Data &,std::string &text,std::string &error);
bool decode(const std::string &text,Data &,std::string &error);
// Stage a completed background import without replacing an image changed by
// Undo/Redo while the job ran. Failure preserves both output and selection.
bool stage_image_import(const Data &,const Image &,int target,const Image *expected,
                        Data &out,int &selected,std::string &error);

using Weights=std::array<float,layer_limit>;
Weights paint_weights(const Data &,double x,double z);
// Paint has a fixed world sampling grid: seam copies are byte-identical.
struct PaintTile {int x=0,z=0;std::array<std::uint8_t,paint_side*paint_side*layer_limit> weights={};};
struct PaintField {std::vector<PaintTile> tiles;std::array<std::array<int,4>,paint_hash_size> lookup={};};
bool bake_paint(const Data &,PaintField &,std::string &error);
unsigned paint_hash(int x,int z);
// Local-space Y is projection depth; XZ map the imported image. Euler angles
// are degrees. Returns a feathered, normal-facing coverage and bounded UVs.
struct Projected {float u=0,v=0,coverage=0;};
Projected project(const Projection &,Point world,std::array<float,3> normal);
std::array<float,2> wind(const Data &,Point,double seconds,std::array<float,2> global);
struct RibbonSample {Point position;std::array<float,2> right={1,0};double distance=0;};
struct BakedRibbon {Ribbon source;std::vector<RibbonSample> samples;std::array<double,4> bounds={};};
bool bake_ribbon(const Ribbon &,BakedRibbon &,std::string &error);
struct RibbonHit {bool inside=false;double distance=0,along=0,height=0;};
RibbonHit nearest(const BakedRibbon &,double x,double z);
}
#endif

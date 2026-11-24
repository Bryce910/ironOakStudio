#ifndef VALWOOD_WORLD_ASSETS_H
#define VALWOOD_WORLD_ASSETS_H
#include "engine/world_studio_tools.h"
#include <array>
namespace world_assets {
struct Image {int width=0,height=0,channels=0;std::uint64_t bytes=0;};
struct Material {std::string name;int color=-1,normal=-1,arm=-1,emissive=-1;float normal_strength=1,roughness=1,metallic=0;bool masked=false,blended=false;};
struct Triangle {std::array<float,3> a,b,c;};
struct Report {
 bool supported=false;std::string error;std::vector<std::string> warnings;
 std::uint64_t file_bytes=0,vertices=0,triangles=0,reference_texture_bytes=0;
 unsigned skins=0,animations=0;world_studio::Vec minimum,maximum;
 std::vector<Image> images;std::vector<Material> materials;std::vector<Triangle> geometry;
};
// CPU-only, bounded embedded static GLB inspection; no GL or global registry mutation.
Report inspect(const std::string &path,bool geometry=false);
bool ray_mesh(const Report &,const world_studio::Ray &,const world_studio::Pose &,double &distance);
struct ImportResult {bool ok=false;std::string stem,error;Report report;};
ImportResult import_model(const std::string &cooker,const std::string &project,const std::string &alias,
                          const std::array<std::string,3> &sources,double units);
struct EnvironmentImport {bool ok=false;std::string path,error;};
EnvironmentImport import_environment(const std::string &project,const std::string &source);
}
#endif

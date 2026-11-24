#ifndef VALWOOD_WORLD_SURFACE_IMAGES_H
#define VALWOOD_WORLD_SURFACE_IMAGES_H
#include "engine/world_surface_authoring.h"
#include <array>
#include <vector>
namespace world_surface {
// Existing landscape fields occupy six 513px layers. Preserve them exactly.
// Each new image uses six more layers: four guttered 510px quadrants,
// a 510px half-resolution mip, then a small packed mip tail. All GPU reads
// use explicit LOD0, so original sky/roof fields retain their old filtering.
constexpr unsigned image_side=1020,array_side=513,image_layers=6,image_mips=10;
struct MipRect {unsigned x=0,y=0,side=0,layer=0;};
std::array<MipRect,image_mips> image_rects();
struct PackedImage {std::vector<std::uint8_t> rgba;ImageRole role=ImageRole::Color;};
bool pack_image(const std::uint8_t *rgba,unsigned width,unsigned height,ImageRole,PackedImage &,std::string &error);
bool pack_paint(const PaintField &,std::vector<std::uint8_t> &rgba,std::string &error);
// CPU equivalent of the GLSL wrapped bilinear/trilinear sampler, for tests.
std::array<float,4> sample_image(const PackedImage &,float u,float v,float lod);
}
#endif

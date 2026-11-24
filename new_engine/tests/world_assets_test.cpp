#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "engine/world_assets.h"
#include <cassert>
#include <cstdio>
int main(int argc,char **argv){
 assert(argc==2);auto report=world_assets::inspect(std::string(argv[1])+"/projects/valwood_world/models/starting_crate.glb",true);if(!report.supported)std::fprintf(stderr,"%s\n",report.error.c_str());assert(report.supported&&report.triangles>0&&report.geometry.size()==report.triangles&&report.minimum.y==0&&report.maximum.y>0&&!report.materials.empty());assert(!world_assets::inspect("/nonexistent/asset.glb").supported);
 world_assets::Report triangle;triangle.supported=true;triangle.geometry.push_back({{{-1,0,0}},{{1,0,0}},{{0,2,0}}});world_studio::Pose pose;pose.minimum={-1,0,-.01};pose.maximum={1,2,.01};pose.root={1000,20,500};pose.yaw=.3;pose.scale=2;double distance=0;
 assert(world_assets::ray_mesh(triangle,{{1000,21,490},{0,0,1}},pose,distance)&&distance>9&&distance<11);assert(!world_assets::ray_mesh(triangle,{{1001.8,23.9,490},{0,0,1}},pose,distance));
 std::printf("PASS asset diagnostics: real static GLB %llu triangles/%zu materials/%zu images, source channel metadata, bounds/base alignment and actual triangle ray hits\n",(unsigned long long)report.triangles,report.materials.size(),report.images.size());
}

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "engine/world_model_cpu.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
int main(int argc,char **argv){
    using namespace world_model_cpu;
    const std::string root=argc>1?argv[1]:".";
    const auto path=root+"/projects/valwood_world/models/cinematic_deadwood.glb";
    Options options;std::string error;
    auto asset=prepare(path.c_str(),options,error);assert(asset&&error.empty());
    Bounds exact;assert(bounds_only(path.c_str(),options,exact,error));for(int i=0;i<3;i++){assert(exact.minimum[i]==asset->minimum[i]);assert(exact.maximum[i]==asset->maximum[i]);}
    assert(!asset->meshes.empty()&&!asset->textures.empty());
    assert(asset->minimum[1]==0&&asset->maximum[1]>1&&asset->maximum[1]<1.2f);
    std::uint64_t geometry=0,textures=0;std::size_t triangles=0;
    for(const auto &m:asset->meshes){triangles+=m.indices.size()/3;geometry+=m.vertices.size()*sizeof(Vertex)+m.indices.size()*sizeof(std::uint32_t);for(const auto &v:m.vertices){assert(std::isfinite(v.p[0])&&std::isfinite(v.p[1])&&std::isfinite(v.p[2]));assert(v.p[1]>=-1e-5f);const float n=v.n[0]*v.n[0]+v.n[1]*v.n[1]+v.n[2]*v.n[2];assert(n>.99f&&n<1.01f);}}
    assert(triangles==6500);
    for(const auto &t:asset->textures){assert(!t.encoded.empty()&&!t.levels.empty());unsigned w=t.levels.front().width,h=t.levels.front().height;for(const auto &level:t.levels){assert(level.width==w&&level.height==h&&level.rgba.size()==std::size_t(w)*h*4);textures+=level.rgba.size();w=w>1?w/2:1;h=h>1?h/2:1;}assert(t.levels.back().width==1&&t.levels.back().height==1);}
    assert(geometry==asset->geometry_bytes&&textures==asset->texture_bytes&&asset->upload_bytes==geometry+textures&&asset->cpu_bytes>=asset->upload_bytes);
    Options tiny=options;tiny.cpu_bytes=1024;assert(!prepare(path.c_str(),tiny,error)&&error.find("budget")!=std::string::npos);
    assert(!bounds_only(path.c_str(),tiny,exact,error)&&error.find("budget")!=std::string::npos);
    std::atomic<bool> stopping{true};Options cancelled=options;cancelled.stopping=&stopping;assert(!prepare(path.c_str(),cancelled,error)&&error.find("cancelled")!=std::string::npos);
    std::atomic<std::uint64_t> generation{5};cancelled=options;cancelled.generation=&generation;cancelled.expected_generation=4;assert(!prepare(path.c_str(),cancelled,error));
    Options small=options;small.max_texture_size=4;assert(!prepare(path.c_str(),small,error)&&error.find("dimension")!=std::string::npos);
    assert(!prepare("/tmp/valwood-nonexistent-stream-model.glb",options,error));
    auto again=prepare(path.c_str(),options,error);assert(again&&again->meshes.size()==asset->meshes.size()&&again->textures.size()==asset->textures.size());
    for(unsigned i=0;i<again->meshes.size();i++){const auto &a=asset->meshes[i],&b=again->meshes[i];assert(a.vertices.size()==b.vertices.size()&&!std::memcmp(a.vertices.data(),b.vertices.data(),a.vertices.size()*sizeof(Vertex)));}
    std::printf("CPU model prepare: %zu triangles, %zu textures, %llu CPU bytes, %llu GPU bytes; bounds/mips/budgets/cancellation pass\n",triangles,asset->textures.size(),static_cast<unsigned long long>(asset->cpu_bytes),static_cast<unsigned long long>(asset->gpu_bytes));
}

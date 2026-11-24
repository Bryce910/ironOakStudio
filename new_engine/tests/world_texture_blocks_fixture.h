#ifndef VALWOOD_TEXTURE_BLOCKS_FIXTURE_H
#define VALWOOD_TEXTURE_BLOCKS_FIXTURE_H
#include "engine/world_texture_blocks_format.h"
#include <filesystem>
#include <fstream>
#include <sstream>
namespace texture_blocks_fixture {
inline std::vector<unsigned char> source(){return {137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,4,0,0,0,4,8,6,0,0,0,169,241,158,126,0,0,0,21,73,68,65,84,120,156,99,108,104,248,255,159,1,9,48,49,160,1,194,2,0,196,83,3,6,29,15,87,130,0,0,0,0,73,69,78,68,174,66,96,130};}
inline void u32(std::vector<unsigned char>&out,unsigned v){for(unsigned i=0;i<4;i++)out.push_back(static_cast<unsigned char>(v>>(i*8)));}
inline std::filesystem::path model(const std::filesystem::path &directory){
    std::filesystem::create_directories(directory/"texture_blocks");
    const auto png=source();const float vertices[]={0,0,0,1,0,0,0,0,1};
    std::vector<unsigned char> bin(sizeof(vertices));std::memcpy(bin.data(),vertices,sizeof(vertices));bin.insert(bin.end(),png.begin(),png.end());while(bin.size()%4)bin.push_back(0);
    std::ostringstream text;
    text<<R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"material":0}]}],"materials":[{"normalTexture":{"index":0},"pbrMetallicRoughness":{"metallicRoughnessTexture":{"index":0}}}],"textures":[{"source":0}],"images":[{"bufferView":1,"mimeType":"image/png"}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,0,1]}],"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":)"<<png.size()<<R"(}],"buffers":[{"byteLength":)"<<bin.size()<<"}]}";
    auto json=text.str();while(json.size()%4)json.push_back(' ');
    std::vector<unsigned char> bytes;u32(bytes,0x46546c67);u32(bytes,2);u32(bytes,unsigned(12+8+json.size()+8+bin.size()));u32(bytes,unsigned(json.size()));u32(bytes,0x4e4f534a);bytes.insert(bytes.end(),json.begin(),json.end());u32(bytes,unsigned(bin.size()));u32(bytes,0x004e4942);bytes.insert(bytes.end(),bin.begin(),bin.end());
    auto path=directory/"fixture.glb";std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char *>(bytes.data()),std::streamsize(bytes.size()));return path;
}
inline std::filesystem::path cache(const std::filesystem::path &directory){char name[40];const auto png=source();std::snprintf(name,sizeof(name),"%016llx.linear.vbc7",static_cast<unsigned long long>(world_texture_blocks::fingerprint(png.data(),png.size())));return directory/"texture_blocks"/name;}
// CPU admission fixtures use opaque recipe-valid synthetic blocks. Real GPU
// decoding and angle measurements are independently checked in the GL fixture.
inline world_texture_blocks::Image synthetic(unsigned format){
    world_texture_blocks::Image image;image.width=image.height=4;image.format=format;
    image.normal_max_degrees=format==world_texture_blocks::format_normal_bc7?1.f:0;image.normal_rms_degrees=format==world_texture_blocks::format_normal_bc7?.5f:0;
    for(unsigned side=4;;side/=2){world_texture_blocks::Level level;level.width=level.height=side;level.blocks.resize(16);level.blocks[0]=2;image.levels.push_back(std::move(level));if(side==1)break;}return image;
}
}
#endif

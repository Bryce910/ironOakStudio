#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "engine/world_model_cpu.h"
#include "tests/world_texture_blocks_fixture.h"
#include <iostream>
#include <stdexcept>
#include <unistd.h>
static unsigned checks=0;
static void require(bool okay,const std::string &message){checks++;if(!okay)throw std::runtime_error(message);}
int main(){
    const auto directory=std::filesystem::temp_directory_path()/ ("valwood-linear-cpu-"+std::to_string(getpid()));
    try{
        namespace bc=world_texture_blocks;namespace cpu=world_model_cpu;
        const auto path=texture_blocks_fixture::model(directory),cache=texture_blocks_fixture::cache(directory);const auto source=texture_blocks_fixture::source();
        cpu::Options options;options.bc7=true;std::string error;
        auto check=[&](bool normal_compressed,bool arm_compressed){
            auto asset=cpu::prepare(path.c_str(),options,error);require(bool(asset),"Model prepare: "+error);
            const auto &mesh=asset->meshes.at(0);const auto &normal=asset->textures.at(mesh.textures[1]),&arm=asset->textures.at(mesh.textures[2]);
            require(normal.compressed==normal_compressed&&arm.compressed==arm_compressed,"Role-specific normal/ARM admission");
            require(!normal.srgb&&!arm.srgb,"Linear data never receives sRGB sampling");
            if(options.bc7){require(mesh.textures[1]!=mesh.textures[2],"Same encoded image retains distinct role keys");require(normal.bc7_format==bc::format_normal_bc7&&arm.bc7_format==bc::format_linear_bc7,"Prepared texture preserves required certificate format");}
            for(const auto &t:asset->textures){unsigned side=4;for(const auto &level:t.levels){require(level.width==side&&level.height==side,"Full source dimensions and exact chain");require(level.rgba.size()==(t.compressed?16:side*side*4),"Block/RGBA storage is exact");side=std::max(1u,side/2);}require(t.levels.size()==3,"All mip levels retained");}
        };
        check(false,false);
        require(bc::write(cache.c_str(),source.data(),source.size(),texture_blocks_fixture::synthetic(bc::format_linear_bc7),error),"Write linear cache: "+error);check(false,true);
        require(bc::write(cache.c_str(),source.data(),source.size(),texture_blocks_fixture::synthetic(bc::format_normal_bc7),error),"Write certified normal cache: "+error);check(true,false);
        options.bc7=false;check(false,false);options.bc7=true;
        {std::ofstream out(cache,std::ios::binary|std::ios::trunc);out<<"corrupt";}check(false,false);
        std::filesystem::remove_all(directory);std::cout<<"Linear BC7 CPU PASS: "<<checks<<" checks; role isolation, angular certificate requirement, full mips and original fallback\n";
    }catch(const std::exception &e){std::filesystem::remove_all(directory);std::cerr<<e.what()<<'\n';return 1;}
}

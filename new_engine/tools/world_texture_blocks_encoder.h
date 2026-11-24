#ifndef VALWOOD_TEXTURE_BLOCKS_ENCODER_H
#define VALWOOD_TEXTURE_BLOCKS_ENCODER_H
#include "third_party/bc7enc16/bc7enc16.h"
#include "engine/world_texture_blocks_format.h"
#include <algorithm>
#include <array>
#include <mutex>
#include <string>
#include <vector>

namespace world_texture_blocks_encoder {
inline bool encode(const std::vector<std::uint8_t>&rgba,std::uint32_t width,std::uint32_t height,
                   world_texture_blocks::Level &out,std::string &error){
    if(!width||!height||width>16384||height>16384||std::uint64_t(width)*height>64u*1024u*1024u
       ||rgba.size()!=std::uint64_t(width)*height*4){error="Invalid bounded RGBA level";return false;}
    for(std::size_t i=3;i<rgba.size();i+=4)if(rgba[i]!=255){error="Opaque pixels required";return false;}
    static std::once_flag initialized;
    std::call_once(initialized,[]{bc7enc16_compress_block_init();});
    bc7enc16_compress_block_params params;
    bc7enc16_compress_block_params_init(&params);
    bc7enc16_compress_block_params_init_linear_weights(&params);
    params.m_uber_level=4;params.m_max_partitions_mode1=64;
    params.m_mode1_partition_estimation_filterbank=BC7ENC16_FALSE;
    world_texture_blocks::Level result;result.width=width;result.height=height;
    const std::uint32_t bx=(width+3)/4,by=(height+3)/4;
    result.blocks.resize(std::size_t(bx)*by*16);
    for(std::uint32_t y=0;y<by;y++)for(std::uint32_t x=0;x<bx;x++){
        std::array<std::uint8_t,64> tile={};
        for(unsigned iy=0;iy<4;iy++)for(unsigned ix=0;ix<4;ix++)for(unsigned c=0;c<4;c++)
            tile[(iy*4+ix)*4+c]=rgba[(std::size_t(std::min(y*4+iy,height-1))*width+std::min(x*4+ix,width-1))*4+c];
        auto *block=result.blocks.data()+(std::size_t(y)*bx+x)*16;
        if(bc7enc16_compress_block(block,tile.data(),&params)){error="Unexpected alpha block";return false;}
        // Mode6's RGB-only objective may choose alpha254 through shared pbits.
        // Force its alpha endpoints/pbits to255; RGB endpoint movement<=1.
        // Mode1 has implicit opaque alpha. GPU quality checks follow encoding.
        if((block[0]&0x7fu)==0x40u){block[6]|=0xfeu;block[7]=0xffu;block[8]|=1u;}
    }
    out=std::move(result);error.clear();return true;
}
}
#endif

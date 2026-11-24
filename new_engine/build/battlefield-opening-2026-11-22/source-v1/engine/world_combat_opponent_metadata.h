#ifndef VALWOOD_WORLD_COMBAT_OPPONENT_METADATA_H
#define VALWOOD_WORLD_COMBAT_OPPONENT_METADATA_H
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace world_combat_opponent_metadata {
inline constexpr unsigned pose_count=172;
// Actual held-weapon right-hand pivots in actor-local metres, exported from
// this species' posed source rig. Never infer them from human proportions.
using Pivots=std::array<std::array<float,3>,pose_count>;
inline bool read_pivots(FILE *file,Pivots &out){
    if(!file)return false;
    auto word=[&](std::uint32_t &v){unsigned char b[4];if(std::fread(b,1,4,file)!=4)return false;
        v=std::uint32_t(b[0])|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);return true;};
    char magic[8];std::uint32_t count=0;
    if(std::fread(magic,1,8,file)!=8||std::memcmp(magic,"VWGRP172",8)||!word(count)||count!=pose_count)return false;
    Pivots staged={};
    for(auto &point:staged)for(float &f:point){std::uint32_t bits=0;if(!word(bits))return false;std::memcpy(&f,&bits,4);
        if(!std::isfinite(f)||std::abs(f)>16)return false;}
    if(std::fgetc(file)!=EOF||std::ferror(file))return false;
    out=staged;return true;
}
inline bool valid_edge(const float a[3],const float b[3]){
    double squared=0;
    for(unsigned k=0;k<3;++k){if(!std::isfinite(a[k])||!std::isfinite(b[k])||std::abs(a[k])>16||std::abs(b[k])>16)return false;
        const double d=double(a[k])-b[k];squared+=d*d;}
    return squared>=.04*.04&&squared<=4.*4.;
}
}
#endif

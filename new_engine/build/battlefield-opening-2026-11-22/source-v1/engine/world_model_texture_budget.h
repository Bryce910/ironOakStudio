#ifndef VALWOOD_WORLD_MODEL_TEXTURE_BUDGET_H
#define VALWOOD_WORLD_MODEL_TEXTURE_BUDGET_H
#include "world_model_mip_math.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>
// Loading-only GPU texture budget. Original encoded images and material color
// semantics remain unchanged; cache identity includes the effective base edge.
namespace world_model_texture_budget {
constexpr unsigned policy_shift=24,policy_mask=31u<<policy_shift;
inline bool valid(unsigned edge){return edge==0||edge==512||edge==1024||edge==2048||edge==4096;}
inline unsigned flags(unsigned edge){return (edge==512?9u:edge==1024?10u:edge==2048?11u:edge==4096?12u:0u)<<policy_shift;}
inline unsigned edge(unsigned policy){const auto n=(policy&policy_mask)>>policy_shift;return n==9?512u:n==10?1024u:n==11?2048u:n==12?4096u:0u;}
inline void dimensions(unsigned &w,unsigned &h,unsigned maximum){if(maximum)while(w>maximum||h>maximum){w=std::max(1u,w/2);h=std::max(1u,h/2);}}
inline bool reduce(const unsigned char *pixels,unsigned width,unsigned height,bool srgb,unsigned maximum,std::vector<unsigned char>&out,unsigned &w,unsigned &h){
 if(!pixels||!width||!height||std::uint64_t(width)*height>64ull*1024*1024||!valid(maximum))return false;
 w=width;h=height;out.assign(pixels,pixels+std::size_t(w)*h*4);
 while(maximum&&(w>maximum||h>maximum)){
  const unsigned nw=std::max(1u,w/2),nh=std::max(1u,h/2);std::vector<unsigned char> next(std::size_t(nw)*nh*4);
  for(unsigned y=0;y<nh;++y)for(unsigned x=0;x<nw;++x){const unsigned x0=x*w/nw,x1=(x+1)*w/nw,y0=y*h/nh,y1=(y+1)*h/nh;
   for(unsigned channel=0;channel<4;++channel){float sum=0;unsigned samples=0;
    for(unsigned sy=y0;sy<y1;++sy)for(unsigned sx=x0;sx<x1;++sx){const auto v=out[(std::size_t(sy)*w+sx)*4+channel];sum+=srgb&&channel<3?world_model_mip_math::linear(v):float(v)/255.f;++samples;}
    sum/=samples;next[(std::size_t(y)*nw+x)*4+channel]=srgb&&channel<3?world_model_mip_math::encoded(sum):static_cast<unsigned char>(std::lround(sum*255));
   }
  }out=std::move(next);w=nw;h=nh;
 }return true;
}
}
#endif

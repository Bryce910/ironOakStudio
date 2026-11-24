#ifndef VALWOOD_WORLD_MODEL_PENDING_TEXTURES_H
#define VALWOOD_WORLD_MODEL_PENDING_TEXTURES_H
#include "engine/world_model_cpu.h"
#include "engine/world_texture_blocks_format.h"
#include "engine/world_model_texture_budget.h"
#include <algorithm>
#include <cstring>
namespace world_model_pending_textures {
inline TextureCachePolicy policy(const world_model_cpu::Texture&t,unsigned anisotropy){
    const unsigned format=t.bc7_format;
    const unsigned blocks=t.bc7_policy?((1u<<16)|(format==world_texture_blocks::format_srgb_bc7?0:format<<17)):0;
    return {t.srgb,t.min_filter,t.mag_filter,t.wrap_s,t.wrap_t,1u|(anisotropy>1?anisotropy<<8:0)|blocks|world_model_texture_budget::flags(t.texture_edge)};
}
inline bool same(const world_model_cpu::Texture&a,unsigned aa,const world_model_cpu::Texture&b,unsigned ba){
    const auto p=policy(a,aa),q=policy(b,ba);
    return p.srgb==q.srgb&&p.min_filter==q.min_filter&&p.mag_filter==q.mag_filter&&
        p.wrap_s==q.wrap_s&&p.wrap_t==q.wrap_t&&p.mip_policy==q.mip_policy&&
        a.fingerprint==b.fingerprint&&a.encoded.size()==b.encoded.size()&&
        (a.encoded.empty()||!std::memcmp(a.encoded.data(),b.encoded.data(),a.encoded.size()));
}
inline std::size_t bytes(const world_model_cpu::Texture&t){std::size_t result=0;for(const auto&l:t.levels)result+=l.rgba.size();return result;}
// Cache hits acquire exactly one reference for the corresponding model owner.
// Never replace a partially uploaded image; its full reservation remains until
// it completes. Future and not-yet-started shared maps need no duplicate upload.
template<class Stage> void refresh(Stage*s){
    if(!s||!s->asset)return;
    for(std::size_t i=s->texture;i<s->cpu->textures.size();++i){
        if(s->asset->texture_owners[i]||(i==s->texture&&s->image))continue;
        const auto&t=s->cpu->textures[i];const auto p=policy(t,s->anisotropy);
        const auto id=texture_cache_find_prepared(t.cache_key?t.cache_key->prepared:nullptr,&p);
        if(id){s->asset->texture_owners[i]=id;s->new_texture_bytes-=std::min(s->new_texture_bytes,bytes(t));}
    }
}
// Source identity includes exact encoded bytes and effective sampler/format,
// never just a hash. Visit in primary/shadow upload order and count one pending
// reservation per shared image, while each model keeps its own cache reference.
template<class Stage> std::size_t missing_union(Stage*primary,Stage*shadow){
    Stage*stages[]={primary,shadow};for(auto*s:stages)refresh(s);std::size_t total=0;
    for(unsigned bank=0;bank<2;++bank){auto*s=stages[bank];if(!s||!s->asset)continue;
        for(std::size_t i=s->texture;i<s->cpu->textures.size();++i){
            if(s->asset->texture_owners[i])continue;
            bool duplicate=false;
            for(unsigned before=0;before<=bank&&!duplicate;++before){auto*p=stages[before];if(!p||!p->asset)continue;
                const auto end=before==bank?i:p->cpu->textures.size();
                for(std::size_t j=p->texture;j<end;++j)if(!p->asset->texture_owners[j]&&
                    same(s->cpu->textures[i],s->anisotropy,p->cpu->textures[j],p->anisotropy)){duplicate=true;break;}
            }
            if(!duplicate)total+=bytes(s->cpu->textures[i]);
        }
    }return total;
}
}
#endif

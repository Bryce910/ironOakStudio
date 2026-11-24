#ifndef VALWOOD_WORLD_UNIFORM_CACHE_H
#define VALWOOD_WORLD_UNIFORM_CACHE_H

#include <cstddef>
#include "engine/world_program_epoch.h"
#include <functional>
#include <map>
#include <string>
#include <utility>

// Program-owned GL location cache with nonallocating C++17 lookup. Owned
// strings retain caller names, including stack-built array-element names;
// transparent std::less<> compares const char* directly on a cache hit.
// Backend queries and node/string allocations occur only on a cache miss.
class WorldUniformCache {
    std::map<std::string,int,std::less<>> entries_;
    unsigned program_=0;unsigned long long epoch_=0;
public:
    void reset(unsigned program=0){entries_.clear();program_=program;epoch_=world_program_epoch;}
    std::size_t size()const{return entries_.size();}
    template<class Query> int get(unsigned program,const char *name,Query &&query){
        if(!program||!name)return -1;
        if(program_!=program||epoch_!=world_program_epoch)reset(program);
        const auto found=entries_.find(name);
        if(found!=entries_.end())return found->second;
        const int location=std::forward<Query>(query)(name);
        // Negative locations are cached too; optional inactive uniforms must
        // not query the driver again every frame.
        return entries_.emplace(std::string(name),location).first->second;
    }
};

#endif

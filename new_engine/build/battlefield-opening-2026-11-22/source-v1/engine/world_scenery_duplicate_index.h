#ifndef VALWOOD_WORLD_SCENERY_DUPLICATE_INDEX_H
#define VALWOOD_WORLD_SCENERY_DUPLICATE_INDEX_H
#include <cmath>
#include <cstdint>
#include <map>
#include <tuple>
#include <vector>

// Load-only index for already validated, finite scenery roots. The final
// predicate deliberately retains the original float elevation arithmetic.
// 2cm cells safely cover its rounding error at the +/-128m offset bound;
// adjacent-cell probes therefore preserve the strict 1cm duplicate policy.
class WorldSceneryDuplicateIndex {
    struct Root {double x,z;float elevation,offset_y;};
    using Cell=std::tuple<unsigned,std::int64_t,std::int64_t,std::int64_t>;
    std::map<Cell,std::vector<Root>> cells;
    static std::int64_t cell(double value){return std::int64_t(std::floor(value/.02));}
public:
    bool insert(unsigned alias,double x,double z,float elevation,float offset_y){
        const auto cx=cell(x),cz=cell(z),cy=cell(double(elevation)+offset_y);
        for(int dx=-1;dx<=1;++dx)for(int dz=-1;dz<=1;++dz)for(int dy=-1;dy<=1;++dy){
            const auto found=cells.find({alias,cx+dx,cz+dz,cy+dy});
            if(found==cells.end())continue;
            for(const auto &old:found->second)
                if(std::abs(old.elevation+old.offset_y-elevation-offset_y)<.01&&
                   std::hypot(old.x-x,old.z-z)<.01)return false;
        }
        cells[{alias,cx,cz,cy}].push_back({x,z,elevation,offset_y});return true;
    }
};
#endif

// CPU-only regression for the exact corpse shadow-proxy opt-in. Link with
// function/data sections and --gc-sections: no renderer or game is started.
#include "engine/world_scenery.cpp"
#include <cassert>
#include <iostream>

int main(){
    const char *names[]={"battlefield_corpse_back","battlefield_corpse_side",
        "battlefield_corpse_other","battlefield_corpse_back_extra","nm_control","unrelated"};
    WorldSceneryDefinition definitions[6]{};
    for(unsigned i=0;i<6;i++){
        auto &d=definitions[i];d.name=names[i];d.stem=names[i];d.lods=2;
        d.collision=0;d.vegetation=0;d.distance=140;d.split=19;
    }
    char error[256]{};
    assert(world_scenery_configure(definitions,6,error,sizeof error));
    unsigned checks=0;
    for(unsigned i=0;i<6;i++){
        const auto found=std::find_if(aliases.begin(),aliases.end(),[&](const Alias &a){return std::strcmp(a.name,names[i])==0;});
        assert(found!=aliases.end());++checks;
        assert(found->cast_shadow==(i==0||i==1||i==4));++checks;
        assert(found->collision==Collision::None);++checks;
        assert(found->lods==2&&found->prop_lod_fade&&found->lod_split==19);++checks;
        assert(found->lod_split-4==15);++checks;
        for(float tier:{.65f,1.f,1.5f,2.f}){
            const float detail=scenery_alias_detail_scale(*found,tier);
            assert(detail==(i<2?tier:1.f));++checks;
            if(i<2){
                assert(std::abs((found->lod_split-4)*detail-15*tier)<1.e-5f);++checks;
                assert(std::abs((found->lod_split+4)*detail-23*tier)<1.e-5f);++checks;
            }
        }
        Placement placement{};placement.alias=unsigned(found-aliases.begin());
        placement.scale=1;placement.x=123;placement.z=456;
        Group group{};group.minimum[0]=group.minimum[2]=-1;
        group.maximum[0]=group.maximum[2]=1;group.maximum[1]=1;
        std::vector<Collider> colliders;
        placement_colliders(colliders,placement,group);
        assert(colliders.empty());++checks;
    }
    for(auto &d:definitions)d.lods=1;
    assert(world_scenery_configure(definitions,6,error,sizeof error));++checks;
    for(unsigned i=0;i<6;i++){
        const auto found=std::find_if(aliases.begin(),aliases.end(),[&](const Alias &a){return std::strcmp(a.name,names[i])==0;});
        assert(found!=aliases.end()&&!found->cast_shadow);++checks;
        assert(found->collision==Collision::None);++checks;
    }
    std::cout<<"PASS "<<checks<<" corpse policy checks; exact aliases, tiered 15-23m fade, unchanged empty colliders\n";
}

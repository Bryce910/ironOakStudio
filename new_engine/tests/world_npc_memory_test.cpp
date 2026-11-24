#include "engine/world_npc_memory.h"
#include "engine/world_npc_appearance.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <new>
static bool tracking=false;static unsigned allocations=0;
[[gnu::noinline]] void *operator new(std::size_t size){if(tracking)allocations++;if(void *p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](std::size_t size){return ::operator new(size);}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,std::size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p,std::size_t)noexcept{std::free(p);}
using namespace world_npc_memory;
static Document sample(){Document d;d.profile.npc_id="npc_test";d.profile.name="Test keeper";Memory parent;parent.id="branch";parent.title="A family";parent.tags={"family"};Memory child;child.id="event";child.parent_id="branch";child.title="A rescue";child.tags={"player"};child.importance=.9;child.emotion=.8;child.shareable=true;d.memories={parent,child};return d;}
int main(int argc,char **argv){
    std::string error;Bank b;
    if(argc==3&&std::string(argv[1])=="--appearance"){
        if(!b.load(argv[2],error)){std::cerr<<error<<'\n';return 1;}
        const auto &a=b.document().profile.appearance;
        for(const auto &v:{a.age_years,a.height_cm,a.weight_kg}){if(v)std::cout<<std::setprecision(17)<<*v;else std::cout<<"unset";std::cout<<'\n';}
        for(const auto *s:{&a.build,&a.skin_tone,&a.eye_color,&a.hair_color,&a.hair_style,&a.hair_texture,&a.hair_thickness,&a.hair_density,&a.facial_hair,&a.distinguishing_features})std::cout<<std::quoted(*s)<<'\n';
        return 0;
    }
    if(argc>=4&&std::string(argv[1])=="--query"){
        if(!b.load(argv[2],error)){std::cerr<<error<<'\n';return 1;}
        Query q;try{q.game_hour=std::stod(argv[3]);}catch(...){return 2;}
        for(int i=4;i<argc;i++){if(std::string_view(argv[i])=="--public"){q.shareable_only=true;continue;}if(q.count==query_tag_limit)return 2;q.tags[q.count++]=argv[i];}
        const auto r=b.recall(q);if(!r.valid)return 2;
        for(unsigned i=0;i<r.count;i++){const auto &h=r.hits[i];std::cout<<b.document().memories[h.index].id<<'\t'<<std::setprecision(17)<<h.score<<'\t'<<h.matched_tags<<'\n';}return 0;
    }
    auto d=sample();assert(valid(d,error)&&b.configure(d,error));Query q;q.tags[0]="family";q.count=1;
    assert(!b.document().profile.appearance.height_cm);
    auto physical=d;physical.profile.appearance.height_cm=174.5;physical.profile.appearance.age_years=0;physical.profile.appearance.hair_thickness="fine";physical.profile.appearance.hair_density="dense";assert(b.configure(physical,error));
    auto bad_physical=physical;bad_physical.profile.appearance.height_cm=0;assert(!b.configure(bad_physical,error));assert(*b.document().profile.appearance.height_cm==174.5);
    bad_physical=physical;bad_physical.profile.appearance.age_years=-1;assert(!valid(bad_physical,error));bad_physical=physical;bad_physical.profile.appearance.weight_kg=std::numeric_limits<double>::infinity();assert(!valid(bad_physical,error));
    bad_physical=physical;bad_physical.profile.appearance.hair_thickness="invented";assert(!valid(bad_physical,error));bad_physical=physical;bad_physical.profile.appearance.hair_density="invented";assert(!valid(bad_physical,error));
    bad_physical=physical;bad_physical.profile.appearance.distinguishing_features=std::string(513,'x');assert(!valid(bad_physical,error));assert(b.configure(d,error));
    auto result=b.recall(q);assert(result.valid&&result.count==2&&b.document().memories[result.hits[0].index].id=="event");
    q.tags[1]="family";q.count=2;const auto duplicated=b.recall(q);assert(duplicated.hits[0].score==result.hits[0].score&&duplicated.hits[0].matched_tags==1);
    q.shareable_only=true;assert(b.recall(q).count==1);q.shareable_only=false;
    auto invalid=d;invalid.memories[0].parent_id="event";assert(!b.configure(invalid,error));assert(b.document().memories[0].parent_id=="core");
    invalid=d;invalid.memories[1].parent_id="missing";assert(!valid(invalid,error));invalid=d;invalid.memories[1].id="branch";assert(!valid(invalid,error));
    invalid=d;invalid.profile.name=std::string("\xc0\xaf",2);assert(!valid(invalid,error));invalid=d;invalid.memories[0].confidence=std::numeric_limits<double>::quiet_NaN();assert(!valid(invalid,error));
    invalid=d;invalid.memories[0].tags={"bad tag"};assert(!valid(invalid,error));invalid=d;invalid.memories[0].tags={"a","a"};assert(!valid(invalid,error));
    auto blocked=d;blocked.memories[0].enabled=false;assert(b.configure(blocked,error)&&b.recall(q).count==0);
    blocked=d;blocked.memories[0].occurred_hour=10;assert(b.configure(blocked,error)&&b.recall(q).count==0);
    auto decayed=d;decayed.memories[1].half_life_hours=10;assert(b.configure(decayed,error));q.count=1;q.tags[0]="player";q.game_hour=0;const double first=b.recall(q).hits[0].score;q.game_hour=10;assert(std::abs(b.recall(q).hits[0].score-first*.5)<1e-12);
    q.game_hour=std::numeric_limits<double>::infinity();assert(!b.recall(q).valid);q.game_hour=0;q.count=17;assert(!b.recall(q).valid);q.count=1;q.tags[0]="Upper";assert(!b.recall(q).valid);
    auto big=sample();big.memories.clear();for(unsigned i=0;i<memory_limit;i++){Memory m;m.id="m_"+std::to_string(i);m.title="Memory";m.tags={"context"};big.memories.push_back(m);}assert(b.configure(big,error));
    q={};q.tags[0]="context";q.count=1;tracking=true;for(unsigned i=0;i<2000;i++){const auto r=b.recall(q);assert(r.valid&&r.count==8);}tracking=false;assert(allocations==0);
    big.memories.push_back(big.memories[0]);assert(!valid(big,error));
    auto chain=sample();chain.memories.clear();for(unsigned i=0;i<17;i++){Memory m;m.id="depth_"+std::to_string(i);m.title="Memory";if(i)m.parent_id="depth_"+std::to_string(i-1);chain.memories.push_back(m);}assert(!valid(chain,error));chain.memories.pop_back();assert(valid(chain,error));
    if(argc>=2){assert(b.load(argv[1],error));assert(b.document().profile.npc_id=="npc_mara_fenwick"&&b.document().memories.size()==7);if(argc>=3){assert(!b.load(argv[2],error));assert(b.document().profile.npc_id=="npc_mara_fenwick"&&b.document().memories.size()==7);}
        char folder[]="/tmp/valwood-profile-import-XXXXXX";assert(mkdtemp(folder));std::vector<world_npc_appearance::Binding> bindings={{16,argv[1]}};
        assert(world_npc_appearance::save_project_bindings(folder,bindings,error));assert(bindings[0].file.find("ai/memories/appearance_imports/")==0);const auto imported=(std::filesystem::path(folder)/bindings[0].file).string();assert(b.load(imported.c_str(),error)&&b.document().profile.npc_id=="npc_mara_fenwick");const auto saved=bindings[0].file;
        assert(world_npc_appearance::save_project_bindings(folder,bindings,error)&&bindings[0].file==saved);
        auto invalid_binding=bindings;invalid_binding.push_back({18,"missing.vwmemory"});assert(!world_npc_appearance::save_project_bindings(folder,invalid_binding,error));
        std::vector<world_npc_appearance::Binding> restored;assert(world_npc_appearance::load_bindings(std::string(folder)+"/ai/npc_appearances.vwlooks",restored,error)&&restored.size()==1&&restored[0].file==saved);std::filesystem::remove_all(folder);
    }
    std::cout<<"PASS NPC memory: atomic imports, tree/UTF-8/budget validation, inherited recall, privacy, future/disabled branches, decay, deterministic top8 and2000 full-bank recalls without allocations\n";
}

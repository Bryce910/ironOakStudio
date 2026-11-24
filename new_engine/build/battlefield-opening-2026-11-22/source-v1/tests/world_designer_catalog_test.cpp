#include "engine/world_designer_assets.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

using namespace world_designer_assets;
namespace {
unsigned checks=0;
void check_at(bool value,const char *expression,unsigned line){++checks;if(!value){std::fprintf(stderr,"FAIL line %u: %s\n",line,expression);std::abort();}}
#define check(value) check_at(bool(value),#value,__LINE__)
WorldSceneryAlias alias(const char *name,const char *stem=nullptr){
    WorldSceneryAlias a;a.name=name;a.stem=stem?stem:name;return a;
}
Record entry(const char *name){return record(0,alias(name));}
std::vector<WorldSceneryAlias> registry;
unsigned reads=0;
WorldSceneryAlias get(std::size_t index){++reads;return registry.at(index);}
void metadata(){
    struct Example {const char *alias,*label,*collection;Category category;};
    const Example examples[]={
        {"nm_city_13_sm_house_four__paint_8a3d3838bd","House Four (Paint 8a3d38)","Castle City",Category::Buildings},
        {"nm_city_21_sm_house_three__paint_21ce49a20d__mat_1067289f","House Three (Paint 21ce49 / Material 106728)","Castle City",Category::Buildings},
        {"nm_city_6_sm_castle_centre","Castle Centre","Castle City",Category::Buildings},
        {"nm_city_37_sm_wooden_stairs_01","Wooden Stairs 01","Castle City",Category::Buildings},
        {"nm_city_38_dressing__1__1","Dressing 1 1 (Chunk 38)","Castle City",Category::Props},
        {"nm_city_39_dressing__1_1","Dressing 1 1 (Chunk 39)","Castle City",Category::Props},
        {"nm_city_60_foliage__1__1","Foliage 1 1 (Chunk 60)","Castle City",Category::Nature},
        {"nm_scene_built_3_9_a_b","Architecture Chunk 3 9 A B","Grandstone",Category::Buildings},
        {"nm_scene_geology_5_2_a","Rock Formation 5 2 A","Grandstone",Category::Nature},
        {"nm_scene_backdrop_10_0","Mountain Backdrop 10 0","Grandstone",Category::Nature},
        {"bep_country_terrain_2_3","Terrain Chunk 2 3","Countryside",Category::Nature},
        {"bep_nature_oak","Oak","Countryside",Category::Nature},
        {"bep_nature_grass_long","Grass Long","Countryside",Category::Nature},
        {"bep_nature_bush_b","Bush B","Countryside",Category::Nature},
        {"bep_prop_bucket","Bucket","Countryside",Category::Props},
        {"bep_structure_storehouse","Storehouse","Countryside",Category::Buildings},
        {"starting_firepit","Firepit","Starter Set",Category::Props},
        {"showcase_roots","Roots","Woodland",Category::Nature},
        {"purchased_goods_chicken","Chicken","Village & Castle",Category::Props},
        {"purchased_torch_wall","Torch Wall","Village & Castle",Category::Props},
        {"castle_weapon_rack","Weapon Rack","Village & Castle",Category::Props},
        {"village_horseman","Horseman","Village & Castle",Category::Characters},
        {"nm_arrival_oak04","Oak 04","Apartment & Arrival",Category::Props},
        {"nm_opening_reference_finward_scene_m1_0_a","Apartment Furniture Chunk M 1 0 A","Apartment & Arrival",Category::Props},
        {"my_custom_thing__paint_xyz","My Custom Thing Paint Xyz","Core Library",Category::Props},
        {"knight_statue","Knight Statue","Core Library",Category::Props},
        {"fir","Fir","Core Library",Category::Nature}
    };
    for(const auto &e:examples){
        auto a=alias(e.alias,"imports/test/asset");auto r=record(19,a);
        check(r.index==19&&r.alias==e.alias&&r.stem==a.stem);
        check(r.label==e.label);check(r.collection==(std::string(e.collection)=="Core Library"?"Imported":e.collection));
        check(r.category==e.category&&!r.subcategory.empty());
        AliasPayload payload;std::string_view decoded;
        check(encode_payload(r.alias,payload)&&decode_payload(&payload,sizeof payload,decoded)&&decoded==e.alias);
        check(matches(r,r.alias)&&matches(r,r.label)&&matches(r,r.collection)&&matches(r,r.subcategory));
    }
    // Registry metadata outranks names for imported vegetation with opaque IDs.
    auto a=alias("nm_arrival_oak04");a.tree=true;a.vegetation=2;a.wind_response=true;
    auto oak=record(9,a);check(oak.category==Category::Nature&&oak.subcategory=="Trees");
    check(matches(oak,"TREES arrival canopy wind"));check(!matches(oak,"trees desert"));
    const auto wind_signature=signature(a);a.wind_response=false;check(signature(a)!=wind_signature);
    const auto grass=entry("bep_nature_grass_long");check(grass.subcategory=="Grass & Groundcover"&&matches(grass,"vegetation groundcover"));
    const auto bush=entry("bep_nature_bush_b");check(bush.subcategory=="Shrubs & Ferns"&&matches(bush,"bushes countryside"));
    check(entry("castle_weapon_rack").subcategory=="Tools & Weapons");
    check(entry("purchased_torch_wall").subcategory=="Lighting");
    check(entry("nm_scene_built_3_9_a_b").subcategory=="Architecture Pieces");
    check(entry("nm_city_boundary_rock_foundations").subcategory=="Rocks & Terrain");
    check(entry("nm_city_12_sm_house_five").subcategory=="Houses & Shops");
    check(entry("tree_stump").subcategory=="Roots & Deadwood");
    // Exact unknown technical strings are retained and searchable; no semantic
    // name is fabricated for arbitrary user imports or malformed suffixes.
    check(entry("a__paint_zzzzzz").label=="A Paint Zzzzzz");
    check(entry("a__paint_deadbeefnote").label=="A Paint Deadbeefnote");
    check(entry("nm_city_weird_thing").label=="Weird Thing");
    const auto empty=record(0,{});check(empty.alias.empty()&&empty.label.empty());
    // Preview identity follows the scenery registry, including named-near
    // rigid props. Neither LOD count nor semantic category guesses a filename.
    auto pine=alias("pine","cinematic_pine");pine.tree=true;pine.lods=3;
    check(preview_stem(pine)=="cinematic_pine_near");
    auto house=alias("purchased_house_a","purchased_village_house_a");house.named_near=true;house.lods=2;
    const auto house_record=record(4,house);
    check(house_record.preview_stem=="purchased_village_house_a_near"&&house_record.stem=="purchased_village_house_a"&&house_record.alias=="purchased_house_a");
    const auto original_signature=signature(house);house.named_near=false;check(signature(house)!=original_signature);
    check(preview_stem(house)=="purchased_village_house_a");
    house.lods=1;house.named_near=true;check(preview_stem(house)=="purchased_village_house_a_near");
    check(preview_stem({}).empty());
    check(matches(oak,"\t\r\n")&&matches(oak,"oak-04")&&matches(oak,"nm_arrival"));
}
void ordering(){
    std::vector<Record> records;
    for(const auto *name:{"house10","house2","house1","house001","house01","House2","house99999999999999999999999999999999","house100000000000000000000000000000000","tree2","tree10"})records.push_back(entry(name));
    for(const auto sort:{Sort::Name,Sort::Category,Sort::Alias}){
        for(const auto &a:records){check(!less(a,a,sort));for(const auto &b:records){
            check(!(less(a,b,sort)&&less(b,a,sort)));
            for(const auto &c:records)if(less(a,b,sort)&&less(b,c,sort))check(less(a,c,sort));
        }}
        auto sorted=records;std::sort(sorted.begin(),sorted.end(),[&](const auto &a,const auto &b){return less(a,b,sort);});
        check(std::is_sorted(sorted.begin(),sorted.end(),[&](const auto &a,const auto &b){return less(a,b,sort);}));
    }
    check(less(entry("house2"),entry("house10")));check(!less(entry("house10"),entry("house2")));
    auto r=entry("barrel");r.label="Same Label";auto s=entry("crate");s.label="same label";check(less(r,s)&&!less(s,r));
    auto high=entry("house99999999999999999999999999999999"),higher=entry("house100000000000000000000000000000000");check(less(high,higher));
    // A reordered presentation is an indirection only. The source index and
    // exact payload alias are never replaced by a display name or sort index.
    registry={alias("house10"),alias("house2"),alias("bep_nature_bush_b")};
    RegistryIndex index;std::vector<Record> cached;check(index.refresh(cached,1,registry.size(),get));
    auto indirection=std::vector<unsigned>{0,1,2};std::sort(indirection.begin(),indirection.end(),[&](auto a,auto b){return less(cached[a],cached[b]);});
    check(cached[0].alias=="house10"&&cached[1].alias=="house2"&&index.find(cached,"house2")==1);
    const auto before=reads;check(!index.refresh(cached,1,registry.size(),get)&&reads==before);
    registry[2]=alias("nm_scene_built_1_2");check(index.refresh(cached,2,registry.size(),get));
    check(index.find(cached,"bep_nature_bush_b")==-1&&cached[2].collection=="Grandstone"&&cached[2].category==Category::Buildings);
}
void fixture(const char *path){
    std::ifstream input(path);check(bool(input));std::string line;unsigned count=0;std::set<std::string> identities;
    std::vector<Record> records;
    while(std::getline(input,line)){
        std::istringstream fields(line);std::string kind,name,stem;fields>>kind;if(kind!="asset")continue;
        unsigned lods=0,collision=0,vegetation=0;float distance=0,split=0;
        check(bool(fields>>name>>stem>>lods>>collision>>vegetation>>distance>>split));
        auto a=alias(name.c_str(),stem.c_str());a.lods=lods;a.collision=collision;a.vegetation=vegetation;a.tree=vegetation==2;
        auto r=record(count++,a);check(identities.insert(r.alias).second);check(!r.label.empty()&&!r.subcategory.empty()&&!r.collection.empty());
        check(r.alias==name&&r.stem==stem&&r.index==count-1&&r.lods==lods&&r.collision==collision);
        check(r.preview_stem==stem+(a.tree?"_near":""));
        check(matches(r,name)&&matches(r,r.label)&&matches(r,r.collection));records.push_back(std::move(r));
    }
    check(count>0);std::sort(records.begin(),records.end(),[](const auto &a,const auto &b){return less(a,b);});
    check(std::is_sorted(records.begin(),records.end(),[](const auto &a,const auto &b){return less(a,b);}));
    std::printf("real catalog: %s, %u aliases retain identity and searchable metadata\n",path,count);
    const auto models=std::filesystem::path(path).parent_path()/"models";
    // The actual project files prove the old stem+.glb guess fails for the
    // representative native library, while exact registry policy succeeds.
    for(auto a:{alias("pine","cinematic_pine"),alias("purchased_house_a","purchased_village_house_a"),alias("bep_nature_fern","bep_nature_fern")}){
        a.tree=std::string_view(a.name)=="pine";a.named_near=!a.tree;
        check(std::filesystem::is_regular_file(models/(preview_stem(a)+".glb")));
        check(!std::filesystem::exists(models/(std::string(a.stem)+".glb")));
    }
    for(const auto &r:records)if(r.alias=="nm_scene_built_2_7"){
        check(r.preview_stem==r.stem&&std::filesystem::is_regular_file(models/(r.preview_stem+".glb")));
    }
}
}
int main(int argc,char **argv){
    metadata();ordering();for(int i=1;i<argc;++i)fixture(argv[i]);
    std::printf("designer catalog: %u semantic, identity, natural-order, registry and fixture checks passed\n",checks);return 0;
}

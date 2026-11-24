#include "world_designer_assets.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace world_designer_assets {
namespace {
unsigned char lower(unsigned char c){return c>='A'&&c<='Z'?unsigned(c-'A'+'a'):c;}
bool separator(unsigned char c){return c=='_'||c=='-'||c==' '||c=='\t'||c=='\r'||c=='\n';}
bool space(unsigned char c){return c==' '||c=='\t'||c=='\r'||c=='\n';}
bool equal(unsigned char a,unsigned char b){return lower(a)==lower(b)||(separator(a)&&separator(b));}
bool find(std::string_view text,std::string_view token){
    if(token.size()>text.size())return false;
    for(std::size_t i=0;i+token.size()<=text.size();++i){
        std::size_t j=0;while(j<token.size()&&equal(text[i+j],token[j]))++j;
        if(j==token.size())return true;
    }return false;
}
bool word(std::string_view text,std::string_view token){
    for(std::size_t i=0;i<text.size();){
        while(i<text.size()&&separator(text[i]))++i;
        auto end=i;while(end<text.size()&&!separator(text[end]))++end;
        if(end-i==token.size()&&find(text.substr(i,end-i),token))return true;
        i=end;
    }return false;
}
bool any(std::string_view text,std::initializer_list<const char*> tokens){
    for(auto token:tokens)if(word(text,token))return true;
    return false;
}
bool starts(std::string_view s,std::string_view prefix){return s.size()>=prefix.size()&&s.substr(0,prefix.size())==prefix;}
bool digit(unsigned char c){return c>='0'&&c<='9';}
bool hex(unsigned char c){return digit(c)||(lower(c)>='a'&&lower(c)<='f');}
// Importer prefixes and material suffixes have a verified meaning in the
// source cookers. Unknown names remain intact rather than guessing a model.
std::string display_name(std::string_view name){
    if(starts(name,"nm_scene_built_"))return "Architecture Chunk "+humanize(name.substr(15));
    if(starts(name,"nm_scene_geology_"))return "Rock Formation "+humanize(name.substr(17));
    if(starts(name,"nm_scene_backdrop_"))return "Mountain Backdrop "+humanize(name.substr(18));
    if(starts(name,"bep_country_terrain_"))return "Terrain Chunk "+humanize(name.substr(20));
    if(starts(name,"nm_opening_reference_finward_scene_"))return "Apartment Furniture Chunk "+humanize(name.substr(35));
    std::string ordinal;
    if(starts(name,"nm_city_")){
        auto end=std::size_t(8);while(end<name.size()&&digit(name[end]))++end;
        if(end>8&&end<name.size()&&name[end]=='_'){
            ordinal=std::string(name.substr(8,end-8));name.remove_prefix(end+1);
            if(starts(name,"sm_"))name.remove_prefix(3);
        }else name.remove_prefix(8);
    }else{
        for(auto prefix:{"bep_nature_","bep_prop_","bep_structure_","bep_opening_","nm_opening_reference_","nm_opening_","nm_scene_lava_","nm_arrival_","nm_journey_","starting_","showcase_","purchased_","village_","castle_"})
            if(starts(name,prefix)){name.remove_prefix(std::strlen(prefix));break;}
        for(auto prefix:{"goods_","prop_"})if(starts(name,prefix)){name.remove_prefix(std::strlen(prefix));break;}
    }
    std::string clean(name),variants;
    for(auto marker:{"__paint_","__mat_"}){
        auto pos=clean.find(marker);if(pos==std::string::npos)continue;
        auto begin=pos+std::strlen(marker),end=begin;while(end<clean.size()&&hex(clean[end]))++end;
        if(end-begin<6||(end<clean.size()&&clean[end]!='_'))continue;
        variants+=variants.empty()?" (":" / ";variants+=std::strcmp(marker,"__paint_")==0?"Paint ":"Material ";
        // Six hexadecimal digits retain a useful visual variant cue. The full
        // immutable identifier remains searchable and visible in the tooltip.
        variants+=clean.substr(begin,std::min<std::size_t>(6,end-begin));clean.erase(pos,end-pos);
    }
    if(!variants.empty())variants+=')';
    std::string result=humanize(clean);
    // City dressing/foliage are combined source chunks: their cell text may
    // normalize to the same words, so retain the import ordinal explicitly.
    if(!ordinal.empty()&&(starts(name,"dressing_")||starts(name,"foliage_")))result+=" (Chunk "+ordinal+")";
    return result+variants;
}
const char *collection(std::string_view name,std::string_view stem){
    if(starts(name,"nm_city_"))return "Castle City";
    if(starts(name,"nm_scene_"))return "Grandstone";
    if(starts(name,"nm_opening_")||starts(name,"nm_arrival_")||starts(name,"nm_journey_")||starts(name,"bep_opening_"))return "Apartment & Arrival";
    if(starts(name,"bep_"))return "Countryside";
    if(starts(name,"showcase_"))return "Woodland";
    if(starts(name,"starting_"))return "Starter Set";
    if(starts(name,"purchased_")||starts(name,"village_")||starts(name,"castle_")||starts(name,"demo_sign_"))return "Village & Castle";
    return starts(stem,"imports/")?"Imported":"Core Library";
}
std::string subcategory(const Record &r){
    const auto &name=r.alias;const auto has=[&](std::initializer_list<const char*> tokens){return any(name,tokens);};
    if(r.category==Category::Characters)return "Characters";
    if(r.category==Category::Buildings){
        if(has({"house","shop","home","inn","smithy","tavern","keep","barracks","storehouse","quartermaster","shelter"}))return "Houses & Shops";
        if(has({"wall","gate","drawbridge","fence"}))return "Walls & Gates";
        return "Architecture Pieces";
    }
    if(r.category==Category::Nature){
        if(has({"roots","stump","deadwood","logs"}))return "Roots & Deadwood";
        if(r.tree||r.vegetation==2||has({"tree","pine","fir","broadleaf","birch","oak","beech","palm"}))return "Trees";
        if(has({"grass","clover","reed","crop","wheat","corn"}))return "Grass & Groundcover";
        if(has({"flower","wildflower","celandine"}))return "Flowers";
        if(has({"fern","fern0","fern1","fern2","shrub","bush","sorrel"}))return "Shrubs & Ferns";
        if(starts(name,"nm_scene_backdrop_"))return "Mountain Backdrops";
        if(has({"terrain","ground","geology","rock","rock0","rock1","rock2","cliff","foundations"}))return "Rocks & Terrain";
        return "Foliage";
    }
    if(starts(name,"nm_opening_reference_finward_scene_"))return "Furniture";
    if(has({"lamp","lantern","torch","brazier"}))return "Lighting";
    if(has({"table","chair","bench","bed","bedroll","furniture"}))return "Furniture";
    if(has({"axe","hatchet","chisel","hammer","crowbar","firepoker","hoe","machete","pickaxe","pitchfork","scythe","shovel","weapon","sword","archery","training"}))return "Tools & Weapons";
    if(has({"bread","cheese","chicken","tankard","goods","stall"}))return "Market Props";
    if(has({"statue","monument","sign","banner","flag"}))return "Statues & Signs";
    if(has({"camp","firepit","crate","barrel","bucket","sack","satchel","chest","rope","cart"}))return "Camp & Supplies";
    return "Other Props";
}
int natural_compare(std::string_view a,std::string_view b){
    std::size_t i=0,j=0;
    while(i<a.size()&&j<b.size()){
        if(digit(a[i])&&digit(b[j])){
            auto ae=i,be=j;while(ae<a.size()&&digit(a[ae]))++ae;while(be<b.size()&&digit(b[be]))++be;
            auto az=i,bz=j;while(az<ae&&a[az]=='0')++az;while(bz<be&&b[bz]=='0')++bz;
            if(ae-az!=be-bz)return ae-az<be-bz?-1:1;
            for(auto k=std::size_t(0);k<ae-az;++k)if(a[az+k]!=b[bz+k])return a[az+k]<b[bz+k]?-1:1;
            i=ae;j=be;continue;
        }
        const auto ac=lower(a[i]),bc=lower(b[j]);if(ac!=bc)return ac<bc?-1:1;++i;++j;
    }
    return i<a.size()?1:(j<b.size()?-1:0);
}
}
const char *category_name(Category c){
    switch(c){case Category::Buildings:return "Buildings";case Category::Nature:return "Nature";
        case Category::Props:return "Props";case Category::Characters:return "Characters";default:return "All";}
}
const char *collision_name(unsigned c){
    switch(c){case 0:return "None";case 1:return "Box";case 2:return "Circle";
        case 3:return "Trunk";case 4:return "Specialized";default:return "Unknown";}
}
std::string humanize(std::string_view alias){
    std::string result;result.reserve(alias.size()+8);bool start=true;unsigned char previous=0;
    for(unsigned char c:alias){
        if(separator(c)){if(!result.empty()&&result.back()!=' ')result+=' ';start=true;previous=c;continue;}
        if(c>='0'&&c<='9'&&previous&&!(previous>='0'&&previous<='9')&&!separator(previous))result+=' ';
        result+=char(start&&c>='a'&&c<='z'?c-'a'+'A':c);start=false;previous=c;
    }
    if(!result.empty()&&result.back()==' ')result.pop_back();
    return result;
}
Category classify(const WorldSceneryAlias &a){
    const std::string_view name=a.name?a.name:"",stem=a.stem?a.stem:"";
    if(starts(name,"nm_scene_built_"))return Category::Buildings;
    if(starts(name,"nm_scene_geology_")||starts(name,"nm_scene_backdrop_"))return Category::Nature;
    if(a.tree||a.vegetation||any(name,{"nature","tree","pine","fir","broadleaf","birch","oak","beech","palm","fern","fern0","fern1","fern2","rock0","rock1","rock2","rock","cliff","roots","stump","deadwood","shrub","sorrel","crop","celandine","wildflower","grass","bush","foliage","terrain","ground"}))return Category::Nature;
    if(any(name,{"statue","monument","sign","dummy","target","torch"}))return Category::Props;
    if(any(name,{"horseman","knight","villager","npc","bandit","dragon","dog","cat","traveler","archer"}))return Category::Characters;
    // Use semantic nouns rather than 'castle'/'village' pack prefixes: a
    // castle weapon rack is still a prop, and a torch wall is not a building.
    if(starts(name,"nm_city_")&&word(name,"castle"))return Category::Buildings;
    if(any(name,{"house","shop","home","inn","smithy","tavern","keep","barracks","quartermaster","shelter","storehouse","drawbridge","gate","tower","platform","structure","stairs","stair","pillar","column","arch","roof","window","door","floor","buttress"})||
       (word(name,"wall")&&!word(name,"torch"))||any(stem,{"house","inn","smithy","keep"})||
       name=="castle"||(name.size()>7&&name.substr(name.size()-7)=="_castle"))return Category::Buildings;
    return Category::Props;
}
std::string preview_stem(const WorldSceneryAlias &a){
    std::string stem=a.stem?a.stem:"";
    if(!stem.empty()&&(a.tree||a.named_near))stem+="_near";
    return stem;
}
Record record(unsigned index,const WorldSceneryAlias &a){
    Record r;r.index=index;r.alias=a.name?a.name:"";r.label=display_name(r.alias);r.stem=a.stem?a.stem:"";
    r.preview_stem=preview_stem(a);
    r.category=classify(a);r.lods=a.lods;r.collision=a.collision;r.vegetation=a.vegetation;
    r.distance=a.distance;r.split=a.split;r.max_scale=a.max_scale;r.tree=a.tree;
    r.collection=collection(r.alias,r.stem);r.subcategory=subcategory(r);
    if(r.subcategory=="Trees")r.tags="tree trees canopy foliage vegetation";
    else if(r.subcategory=="Grass & Groundcover")r.tags="grass groundcover foliage vegetation";
    else if(r.subcategory=="Shrubs & Ferns")r.tags="shrub shrubs bush bushes fern foliage vegetation";
    else if(r.subcategory=="Flowers"||r.subcategory=="Foliage")r.tags="plants foliage vegetation";
    else if(r.subcategory=="Rocks & Terrain")r.tags="landscape ground stone rocks terrain";
    else if(r.subcategory=="Mountain Backdrops")r.tags="mountains background landscape";
    else if(r.category==Category::Buildings)r.tags="architecture building construction";
    else if(r.subcategory=="Lighting")r.tags="light lighting";
    else if(r.subcategory=="Furniture")r.tags="furniture furnishings";
    if(r.collision)r.tags+=" collision";
    if(a.wind_response)r.tags+=" wind";
    return r;
}
std::uint64_t signature(const WorldSceneryAlias &a){
    std::uint64_t hash=14695981039346656037ull;
    auto bytes=[&](const void *p,std::size_t n){auto s=static_cast<const unsigned char*>(p);while(n--){hash^=*s++;hash*=1099511628211ull;}};
    auto str=[&](const char *s){if(s)bytes(s,std::strlen(s));const unsigned char end=0;bytes(&end,1);};
    str(a.name);str(a.stem);bytes(&a.lods,sizeof a.lods);bytes(&a.collision,sizeof a.collision);
    bytes(&a.vegetation,sizeof a.vegetation);bytes(&a.tree,sizeof a.tree);bytes(&a.max_scale,sizeof a.max_scale);
    bytes(&a.distance,sizeof a.distance);bytes(&a.split,sizeof a.split);bytes(&a.named_near,sizeof a.named_near);bytes(&a.wind_response,sizeof a.wind_response);return hash;
}
bool RegistryIndex::refresh(std::vector<Record> &records,std::uint64_t generation,std::size_t count,Getter get){
    if(generation_==generation&&count_==count&&records.size()==count)return false;
    std::vector<Record> next;next.reserve(count);
    std::vector<unsigned> sorted;sorted.reserve(count);
    for(std::size_t i=0;i<count;++i){next.push_back(record(unsigned(i),get(i)));sorted.push_back(unsigned(i));}
    std::sort(sorted.begin(),sorted.end(),[&](unsigned a,unsigned b){return next[a].alias<next[b].alias;});
    records=std::move(next);sorted_=std::move(sorted);count_=count;generation_=generation;++rebuilds_;return true;
}
int RegistryIndex::find(const std::vector<Record> &records,std::string_view name)const{
    if(name.empty())return -1;
    const auto at=std::lower_bound(sorted_.begin(),sorted_.end(),name,[&](unsigned index,std::string_view alias){return records[index].alias.compare(alias)<0;});
    return at!=sorted_.end()&&records[*at].alias==name?int(*at):-1;
}
bool matches(const Record &r,std::string_view query){
    for(std::size_t i=0;i<query.size();){
        while(i<query.size()&&space(query[i]))++i;
        auto end=i;while(end<query.size()&&!space(query[end]))++end;
        if(end>i){const auto token=query.substr(i,end-i);if(!find(r.alias,token)&&!find(r.label,token)&&!find(r.stem,token)&&
            !find(category_name(r.category),token)&&!find(r.subcategory,token)&&!find(r.collection,token)&&!find(r.tags,token))return false;}
        i=end;
    }return true;
}
bool less(const Record &a,const Record &b,Sort sort){
    if(sort==Sort::Alias){const auto order=natural_compare(a.alias,b.alias);return order?order<0:a.alias<b.alias;}
    if(sort==Sort::Category){
        if(a.category!=b.category)return int(a.category)<int(b.category);
        const auto sub=natural_compare(a.subcategory,b.subcategory);if(sub)return sub<0;
    }
    const auto order=natural_compare(a.label,b.label);return order?order<0:a.alias<b.alias;
}
bool valid_alias(std::string_view alias){
    if(alias.empty()||alias.size()>=sizeof(AliasPayload::alias))return false;
    for(unsigned char c:alias)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;
    return true;
}
bool contains(const std::vector<std::string> &items,std::string_view alias){
    for(const auto &item:items)if(item==alias)return true;
    return false;
}
bool toggle_favorite(std::vector<std::string> &items,std::string_view alias){
    if(!valid_alias(alias))return false;
    const auto at=std::find(items.begin(),items.end(),alias);
    if(at!=items.end()){items.erase(at);return true;}
    if(items.size()>=favorite_limit)return false;
    items.emplace_back(alias);return true;
}
void remember(std::vector<std::string> &items,std::string_view alias){
    if(!valid_alias(alias))return;
    // Copy first: the caller may pass a string_view into this vector.
    const std::string value(alias);
    items.erase(std::remove(items.begin(),items.end(),value),items.end());
    items.insert(items.begin(),value);if(items.size()>recent_limit)items.resize(recent_limit);
}
void normalize_preferences(std::vector<std::string> &items,std::size_t limit){
    limit=std::min(limit,favorite_limit);std::size_t keep=0;
    for(std::size_t i=0;i<items.size();++i){
        if(!valid_alias(items[i]))continue;
        bool duplicate=false;for(std::size_t k=0;k<keep;++k)if(items[k]==items[i]){duplicate=true;break;}
        if(!duplicate&&keep<limit){if(keep!=i)items[keep]=std::move(items[i]);++keep;}
    }items.resize(keep);
}
bool encode_payload(std::string_view alias,AliasPayload &out){
    out={};if(!valid_alias(alias))return false;std::memcpy(out.alias,alias.data(),alias.size());return true;
}
bool decode_payload(const void *data,std::size_t bytes,std::string_view &out){
    out={};if(!data||bytes!=sizeof(AliasPayload))return false;
    const auto *chars=static_cast<const char*>(data);const auto *end=static_cast<const char*>(std::memchr(chars,0,bytes));
    if(!end)return false;
    const std::string_view value(chars,std::size_t(end-chars));if(!valid_alias(value))return false;out=value;return true;
}
bool placement_parameters(const WorldSceneryAlias &a,double x,double z,float yaw,float scale,float &out_yaw,float &out_scale){
    if(!a.name||!valid_alias(a.name)||!std::isfinite(x)||!std::isfinite(z)||std::abs(x)>50400||std::abs(z)>28400||
       !std::isfinite(a.max_scale)||a.max_scale<.15f||!std::isfinite(yaw)||!std::isfinite(scale))return false;
    out_yaw=std::remainder(yaw,360.f);out_scale=std::clamp(scale,.15f,std::min(a.max_scale,4.f));return true;
}
}

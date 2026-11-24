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
    if(a.tree||a.vegetation||any(name,{"nature","tree","pine","broadleaf","birch","oak","beech","palm","fern","fern0","fern1","fern2","rock0","rock1","rock2","rock","cliff","roots","stump","deadwood","shrub","sorrel","crop","celandine","wildflower"}))return Category::Nature;
    if(any(name,{"horseman","knight","villager","npc","bandit","dragon","dog","cat","traveler","archer"}))return Category::Characters;
    // Use semantic nouns rather than 'castle'/'village' pack prefixes: a
    // castle weapon rack is still a prop, and a torch wall is not a building.
    if(any(name,{"house","inn","smithy","tavern","keep","barracks","quartermaster","shelter","storehouse","drawbridge","gate","tower","platform","structure"})||
       (word(name,"wall")&&!word(name,"torch"))||any(stem,{"house","inn","smithy","keep"})||
       name=="castle"||(name.size()>7&&name.substr(name.size()-7)=="_castle"))return Category::Buildings;
    return Category::Props;
}
Record record(unsigned index,const WorldSceneryAlias &a){
    Record r;r.index=index;r.alias=a.name?a.name:"";r.label=humanize(r.alias);r.stem=a.stem?a.stem:"";
    r.category=classify(a);r.lods=a.lods;r.collision=a.collision;r.vegetation=a.vegetation;
    r.distance=a.distance;r.split=a.split;r.max_scale=a.max_scale;r.tree=a.tree;return r;
}
std::uint64_t signature(const WorldSceneryAlias &a){
    std::uint64_t hash=14695981039346656037ull;
    auto bytes=[&](const void *p,std::size_t n){auto s=static_cast<const unsigned char*>(p);while(n--){hash^=*s++;hash*=1099511628211ull;}};
    auto str=[&](const char *s){if(s)bytes(s,std::strlen(s));const unsigned char end=0;bytes(&end,1);};
    str(a.name);str(a.stem);bytes(&a.lods,sizeof a.lods);bytes(&a.collision,sizeof a.collision);
    bytes(&a.vegetation,sizeof a.vegetation);bytes(&a.tree,sizeof a.tree);bytes(&a.max_scale,sizeof a.max_scale);
    bytes(&a.distance,sizeof a.distance);bytes(&a.split,sizeof a.split);return hash;
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
        if(end>i){const auto token=query.substr(i,end-i);if(!find(r.alias,token)&&!find(r.label,token)&&!find(r.stem,token))return false;}
        i=end;
    }return true;
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

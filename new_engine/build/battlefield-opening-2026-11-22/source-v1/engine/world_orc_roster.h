#ifndef VALWOOD_WORLD_ORC_ROSTER_H
#define VALWOOD_WORLD_ORC_ROSTER_H
#include <algorithm>
#include <iomanip>
#include <istream>
#include <string>
#include <utility>
#include <vector>

// A roster advertises distinct owned character banks. A committed match uses
// one character's own body, weapon and anatomy; a menu draft is not a live swap.
namespace world_orc_roster {
struct Entry {
    std::string id,display_name,body_directory,anatomy_directory,materials_config;
};
struct Roster {
    std::string default_id;
    std::vector<Entry> entries;
    int index(const std::string &id)const {
        for(unsigned i=0;i<entries.size();++i)if(entries[i].id==id)return int(i);
        return -1;
    }
};
inline bool valid_id(const std::string &id){
    return !id.empty()&&id.size()<=48&&std::all_of(id.begin(),id.end(),[](unsigned char c){
        return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-';});
}
inline bool valid_path(const std::string &path){
    if(path.empty()||path.size()>200||path.front()=='/'||path.back()=='/')return false;
    std::size_t begin=0;
    for(std::size_t i=0;i<=path.size();++i){
        if(i==path.size()||path[i]=='/'){
            const auto part=path.substr(begin,i-begin);
            if(part.empty()||part=="."||part=="..")return false;
            begin=i+1;
        }else{const unsigned char c=path[i];
            if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return false;
        }
    }
    return true;
}
inline bool quoted(std::istream &input,std::string &value){
    input>>std::ws;
    return input.peek()=='"'&&bool(input>>std::quoted(value));
}
inline bool read(std::istream &input,Roster &out,std::string &error){
    Roster staged;std::string token;unsigned version=0;bool ended=false,has_default=false;
    auto fail=[&](const char *message){error=message;return false;};
    if(!(input>>token>>version)||token!="VALWOOD_ORC_ROSTER"||version!=1)return fail("Unsupported Orc roster.");
    while(input>>token){
        if(token=="end"){ended=true;break;}
        if(token=="default"){
            if(has_default||!(input>>staged.default_id)||!valid_id(staged.default_id))return fail("Invalid default Orc character.");
            has_default=true;
        }else if(token=="character"){
            Entry entry;
            if(staged.entries.size()>=32||!(input>>entry.id)||!valid_id(entry.id)||staged.index(entry.id)>=0||
               !quoted(input,entry.display_name)||entry.display_name.empty()||entry.display_name.size()>80||
               std::any_of(entry.display_name.begin(),entry.display_name.end(),[](unsigned char c){return c<32||c==127;})||
               !quoted(input,entry.body_directory)||!valid_path(entry.body_directory)||
               !quoted(input,entry.anatomy_directory)||!valid_path(entry.anatomy_directory)||
               !quoted(input,entry.materials_config)||!valid_path(entry.materials_config))return fail("Invalid Orc character entry.");
            staged.entries.push_back(std::move(entry));
        }else return fail("Unknown Orc roster field.");
    }
    if(!ended||!has_default||staged.entries.empty()||staged.index(staged.default_id)<0||(input>>token))return fail("Incomplete Orc roster.");
    out=std::move(staged);error.clear();return true;
}

// Never keep all character pose banks resident. A between-match switch releases
// the old bank before allocating the replacement. On a failed load, rebuild the
// previous bank and retain its identity. The caller validates the requested entry
// before this loading boundary and does not allocate/award a run until success.
template<class Bank,class Load,class Release>
bool switch_bank(Bank *&bank,std::string &loaded_id,const std::string &requested,
                 Load load,Release release,std::string &error){
    if(bank&&loaded_id==requested){error.clear();return true;}
    const std::string previous=loaded_id;
    if(bank)release(bank);
    bank=nullptr;loaded_id.clear();
    bank=load(requested,error);
    if(bank){loaded_id=requested;error.clear();return true;}
    const std::string failure=error.empty()?"The selected Orc character could not load.":error;
    if(!previous.empty()){
        std::string recovery;
        bank=load(previous,recovery);
        if(bank)loaded_id=previous;
        else error=failure+" The previous Orc character is also unavailable. Human fights remain available.";
    }
    if(bank||previous.empty())error=failure;
    return false;
}
}
#endif

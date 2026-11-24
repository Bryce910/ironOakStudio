#ifndef VALWOOD_WORLD_KNIGHT_ROSTER_H
#define VALWOOD_WORLD_KNIGHT_ROSTER_H
#include "world_platform.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <istream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// Cosmetic arena selection is deliberately independent of economic progress.
// A draft row is not a loaded character. The caller applies all weapon/FP
// appearances atomically before committing an arena run or remembering a row.
namespace world_knight_roster {
struct Entry {
    std::string id,display_name,description,portrait_path,appearance_config;
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
inline bool valid_text(const std::string &text,std::size_t limit,bool empty=false){
    return (empty||!text.empty())&&text.size()<=limit&&
        std::none_of(text.begin(),text.end(),[](unsigned char c){return c<32||c==127;});
}
inline bool quoted(std::istream &input,std::string &value){
    input>>std::ws;return input.peek()=='"'&&bool(input>>std::quoted(value));
}
inline bool read(std::istream &input,Roster &out,std::string &error){
    Roster staged;std::string token;unsigned version=0;bool ended=false,has_default=false;
    auto fail=[&](const char *message){error=message;return false;};
    if(!(input>>token>>version)||token!="VALWOOD_KNIGHT_ROSTER"||version!=1)return fail("Unsupported knight roster.");
    while(input>>token){
        if(token=="end"){ended=true;break;}
        if(token=="default"){
            if(has_default||!(input>>staged.default_id)||!valid_id(staged.default_id))return fail("Invalid default knight.");
            has_default=true;
        }else if(token=="character"){
            Entry entry;
            if(staged.entries.size()>=32||!(input>>entry.id)||!valid_id(entry.id)||staged.index(entry.id)>=0||
               !quoted(input,entry.display_name)||!valid_text(entry.display_name,80)||
               !quoted(input,entry.description)||!valid_text(entry.description,240,true)||
               !quoted(input,entry.portrait_path)||!valid_path(entry.portrait_path)||
               !quoted(input,entry.appearance_config)||!valid_path(entry.appearance_config))return fail("Invalid knight entry.");
            staged.entries.push_back(std::move(entry));
        }else return fail("Unknown knight roster field.");
    }
    if(!ended||!has_default||staged.entries.empty()||staged.index(staged.default_id)<0||(input>>token))return fail("Incomplete knight roster.");
    out=std::move(staged);error.clear();return true;
}
inline bool read_selection(std::istream &input,const Roster &roster,std::string &out,std::string &error){
    std::string magic,field,id,end,trailing;unsigned version=0;
    if(!(input>>magic>>version>>field>>id>>end)||magic!="VALWOOD_ARENA_PLAYER_CHARACTER"||version!=1||
       field!="player"||end!="end"||(input>>trailing)||!valid_id(id)||roster.index(id)<0){
        error="The saved knight choice is unavailable or invalid.";return false;
    }
    out=std::move(id);error.clear();return true;
}
enum class SelectionLoad {Missing,Loaded,Invalid,IoError};
inline SelectionLoad load_selection(const std::filesystem::path &path,const Roster &roster,std::string &out,std::string &error){
    std::error_code ec;
    if(!std::filesystem::exists(path,ec)){if(ec){error=ec.message();return SelectionLoad::IoError;}error.clear();return SelectionLoad::Missing;}
    const auto size=std::filesystem::file_size(path,ec);
    if(ec||size>1024){error="The saved knight choice could not be read.";return ec?SelectionLoad::IoError:SelectionLoad::Invalid;}
    std::ifstream file(path,std::ios::binary);std::string bytes(size,'\0');
    if(!file||!file.read(bytes.data(),size)){error="The saved knight choice could not be read.";return SelectionLoad::IoError;}
    std::istringstream input(bytes);return read_selection(input,roster,out,error)?SelectionLoad::Loaded:SelectionLoad::Invalid;
}
inline bool save_selection(const std::filesystem::path &path,const Roster &roster,const std::string &id,std::string &error){
    if(!valid_id(id)||roster.index(id)<0){error="Choose an available knight.";return false;}
    const std::string bytes="VALWOOD_ARENA_PLAYER_CHARACTER 1\nplayer "+id+"\nend\n";
    std::error_code ec;
    if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path(),ec);
    if(ec){error="Could not remember this knight for the next launch.";return false;}
    const auto pattern=path.u8string()+".tmp-XXXXXX";std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);
    const int fd=mkstemp(name.data());if(fd<0){error="Could not remember this knight for the next launch.";return false;}
    auto *file=fdopen(fd,"wb");bool okay=false;
    if(file){okay=std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size()&&std::fflush(file)==0&&fsync(fd)==0;if(std::fclose(file))okay=false;}
    else close(fd);
    if(okay)okay=vw_replace(name.data(),path.u8string().c_str())==0;
    if(!okay){unlink(name.data());error="Could not remember this knight for the next launch.";return false;}
    error.clear();return true;
}
}
#endif

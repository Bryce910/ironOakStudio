#ifndef VALWOOD_WORLD_ARENA_PROGRESS_H
#define VALWOOD_WORLD_ARENA_PROGRESS_H

#include "world_platform.h"
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

// The practice purse is deliberately independent of opening.progress. A run
// allocation and its eventual receipt each replace this entire small record.
namespace world_arena_progress {
struct Config {unsigned total=8,simultaneous=1;};
inline bool valid(Config c){return c.total>=1&&c.total<=8&&c.simultaneous>=1&&c.simultaneous<=c.total;}
inline bool operator==(Config a,Config b){return a.total==b.total&&a.simultaneous==b.simultaneous;}
inline bool operator!=(Config a,Config b){return !(a==b);}
constexpr std::uint64_t coins_per_opponent=10,maximum_runs=1000000000;
inline std::uint64_t payout(Config c){return valid(c)?coins_per_opponent*c.total:0;}
struct Progress {
    Config selection={};
    std::uint64_t balance=0,wins=0,opponents=0,next_run_id=1,last_settled_run_id=0,active_run_id=0;
    Config active_config={};
};
inline bool valid(const Progress &p){
    if(!valid(p.selection)||!valid(p.active_config)||p.next_run_id<1||p.next_run_id>maximum_runs+1||
       p.last_settled_run_id>=p.next_run_id||p.active_run_id>=p.next_run_id||
       p.wins>p.last_settled_run_id||p.opponents<p.wins||p.opponents>p.wins*8||
       p.balance!=p.opponents*coins_per_opponent)return false;
    if(!p.wins&&(p.opponents||p.balance||p.last_settled_run_id))return false;
    if(p.wins&&!p.last_settled_run_id)return false;
    if(p.active_run_id&&(p.active_run_id!=p.next_run_id-1||p.active_run_id<=p.last_settled_run_id))return false;
    return true;
}
inline std::uint64_t fingerprint(const std::string &text){
    std::uint64_t h=14695981039346656037ull;
    for(unsigned char c:text){h^=c;h*=1099511628211ull;}
    return h;
}
inline std::string payload(const Progress &p){
    std::ostringstream s;
    s<<"VALWOOD_ARENA_PROGRESS 1\nselection "<<p.selection.total<<' '<<p.selection.simultaneous
     <<"\nledger "<<p.balance<<' '<<p.wins<<' '<<p.opponents<<' '<<p.next_run_id<<' '<<p.last_settled_run_id
     <<"\nactive "<<p.active_run_id<<' '<<p.active_config.total<<' '<<p.active_config.simultaneous<<'\n';
    return s.str();
}
inline bool encode(const Progress &p,std::string &out,std::string &error){
    if(!valid(p)){error="The arena purse has inconsistent progress.";return false;}
    const auto body=payload(p);std::ostringstream s;s<<body<<"checksum "<<std::hex<<std::setw(16)<<std::setfill('0')<<fingerprint(body)<<'\n';
    out=s.str();error.clear();return true;
}
inline bool parse(const std::string &text,Progress &out,std::string &error){
    if(text.empty()||text.size()>1024){error="The arena purse is empty or too large.";return false;}
    // Canonical formatting also excludes negative unsigned values, duplicate
    // records, truncation, trailing fields and accidental concatenated saves.
    Progress p;std::istringstream s(text);std::string magic,a,b,c,check,extra;unsigned version=0;std::uint64_t hash=0;
    if(!(s>>magic>>version>>a>>p.selection.total>>p.selection.simultaneous>>b>>p.balance>>p.wins>>p.opponents>>p.next_run_id>>p.last_settled_run_id
         >>c>>p.active_run_id>>p.active_config.total>>p.active_config.simultaneous>>check>>std::hex>>hash)||
       magic!="VALWOOD_ARENA_PROGRESS"||version!=1||a!="selection"||b!="ledger"||c!="active"||check!="checksum"||
       (s>>extra)||!valid(p)||hash!=fingerprint(payload(p))){error="The arena purse could not be verified. The existing file was preserved.";return false;}
    std::string canonical,ignored;if(!encode(p,canonical,ignored)||canonical!=text){error="The arena purse format is invalid. The existing file was preserved.";return false;}
    out=p;error.clear();return true;
}
inline bool allocate(const Progress &current,Config config,Progress &out,std::uint64_t &run_id,std::string &error){
    if(!valid(current)||!valid(config)||current.next_run_id>maximum_runs){error="A new arena run could not be allocated.";return false;}
    Progress p=current;p.selection=config;p.active_config=config;p.active_run_id=p.next_run_id++;
    out=p;run_id=p.active_run_id;error.clear();return true;
}
enum class Settlement {Rejected,AlreadySaved,Ready};
inline Settlement settle(const Progress &current,std::uint64_t run_id,Config config,bool genuine_win,
                         Progress &out,std::uint64_t &award,std::string &error){
    award=0;
    if(!valid(current)||!valid(config)||!genuine_win||run_id==0){error="Only a completed arena victory can earn coins.";return Settlement::Rejected;}
    if(run_id==current.last_settled_run_id&&!current.active_run_id&&config==current.active_config){out=current;error.clear();return Settlement::AlreadySaved;}
    if(run_id!=current.active_run_id||config!=current.active_config){error="This victory does not match the active arena run.";return Settlement::Rejected;}
    Progress p=current;p.balance+=payout(config);++p.wins;p.opponents+=config.total;p.last_settled_run_id=run_id;p.active_run_id=0;
    if(!valid(p)){error="The arena reward could not be verified.";return Settlement::Rejected;}
    out=p;award=payout(config);error.clear();return Settlement::Ready;
}
enum class Load {Missing,Loaded,Invalid,IoError};
inline Load load(const std::filesystem::path &path,Progress &out,std::string &error){
    std::error_code ec;const bool exists=std::filesystem::exists(path,ec);
    if(ec){error="Could not check the arena purse: "+ec.message();return Load::IoError;}
    if(!exists){out=Progress{};error.clear();return Load::Missing;}
    if(!std::filesystem::is_regular_file(path,ec)||ec){error="The arena purse is not a readable file. It was preserved.";return Load::IoError;}
    const auto size=std::filesystem::file_size(path,ec);
    if(ec||size>1024){error="The arena purse could not be read safely. It was preserved.";return Load::Invalid;}
    std::ifstream file(path,std::ios::binary);std::string text(size,'\0');
    if(!file||!file.read(text.data(),std::streamsize(size))){error="The arena purse could not be read. It was preserved.";return Load::IoError;}
    return parse(text,out,error)?Load::Loaded:Load::Invalid;
}
inline bool save(const std::filesystem::path &path,const Progress &p,std::string &error){
    std::string bytes;if(!encode(p,bytes,error))return false;
    std::error_code ec;const auto parent=path.parent_path().empty()?std::filesystem::path("."):path.parent_path();
    std::filesystem::create_directories(parent,ec);if(ec){error="Could not create the arena save folder: "+ec.message();return false;}
    const std::string target=path.u8string(),pattern=target+".tmp-XXXXXX";std::vector<char> temp(pattern.begin(),pattern.end());temp.push_back(0);
    const int fd=mkstemp(temp.data());if(fd<0){error="Could not stage the arena purse. Check that the game folder is writable.";return false;}
    FILE *file=fdopen(fd,"wb");bool ok=false;
    if(file){ok=std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size()&&std::fflush(file)==0&&fsync(fd)==0;if(std::fclose(file)!=0)ok=false;}
    else close(fd);
    if(ok)ok=vw_replace(temp.data(),target.c_str())==0;
    if(!ok){unlink(temp.data());error="Could not save the arena purse. Your previously saved coins were preserved.";return false;}
    error.clear();return true;
}
// A modal handoff stays closed until the complete physical input is neutral.
// The host resets this on focus/menu transitions and observes real held state.
struct NeutralGate {
    bool armed=false;
    void reset(){armed=false;}
    bool observe(bool focused,bool held){if(!focused){armed=false;return false;}if(!held)armed=true;return armed;}
};
}
#endif

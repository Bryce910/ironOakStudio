#include "engine/world_designer_workspace.h"
#include "engine/world_designer_assets.h"
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <thread>
#include <unistd.h>

using namespace world_designer_workspace;
namespace {
struct TemporaryDirectory {
    std::string path;
    TemporaryDirectory(){char pattern[]="/tmp/valwood-designer-workspace-XXXXXX";const auto *result=mkdtemp(pattern);assert(result);path=result;}
    ~TemporaryDirectory(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}
};
std::string encoded(const Preferences &p){std::string result,error;assert(encode(p,result,error)&&error.empty());return result;}
Preferences sample(){
    Preferences p;p.layout.left=283.125f;p.layout.right=417.375f;p.layout.bottom=283.5f;
    p.tool=22;p.category=2;p.grid=false;p.favorites_only=true;p.ground_grid=true;
    p.favorites={"bep_nature_oak","purchased_house_a","pine_young"};p.recent={"pine_young","starting_barrel"};return p;
}
std::string read(const std::string &path){std::ifstream file(path,std::ios::binary);assert(file);return {std::istreambuf_iterator<char>(file),{}};}
void write(const std::string &path,const std::string &text){std::ofstream file(path,std::ios::binary|std::ios::trunc);file.write(text.data(),std::streamsize(text.size()));assert(file);}
void rejected(const std::string &text){
    Preferences preserved=sample();const auto before=encoded(preserved);std::string error;
    assert(!decode(text,preserved,error));assert(!error.empty());assert(encoded(preserved)==before);
}
void bad_encode(const Preferences &p){
    std::string text="unchanged",error;assert(!encode(p,text,error));assert(text=="unchanged"&&!error.empty());
}
void no_temporary_files(const std::string &folder){
    for(const auto &file:std::filesystem::directory_iterator(folder))assert(file.path().filename().string().find(".tmp-")==std::string::npos);
}
const std::string header="VALWOOD_DESIGNER_WORKSPACE 1\n260 340 224 2 0 1 0 0\n";
void codec(){
    const auto p=sample();const auto text=encoded(p);Preferences loaded;std::string error="previous error";
    assert(decode(text,loaded,error)&&error.empty()&&encoded(loaded)==text);
    assert(loaded.layout.top==72&&loaded.layout.status==24);
    Preferences defaults;assert(decode(encoded(defaults),loaded,error)&&encoded(loaded)==encoded(defaults));
    assert(decode(text+" \n\t\r",loaded,error)&&encoded(loaded)==text);
    rejected("");rejected("VALWOOD_DESIGNER_WORKSPACE 1");rejected("OTHER 1\n260 340 224 2 0 1 0 0\n");
    rejected("VALWOOD_DESIGNER_WORKSPACE 2\n260 340 224 2 0 1 0 0\n");
    rejected("VALWOOD_DESIGNER_WORKSPACE -1\n260 340 224 2 0 1 0 0\n");
    for(const char *bad:{"nan","NaN","inf","-inf","1e100"}){
        rejected("VALWOOD_DESIGNER_WORKSPACE 1\n"+std::string(bad)+" 340 224 2 0 1 0 0\n");
    }
    for(const char *bad:{"159 340 224 2 0 1 0 0","521 340 224 2 0 1 0 0","260 259 224 2 0 1 0 0",
        "260 641 224 2 0 1 0 0","260 340 111 2 0 1 0 0","260 340 481 2 0 1 0 0",
        "260 340 224 -1 0 1 0 0","260 340 224 25 0 1 0 0","260 340 224 2 -1 1 0 0",
        "260 340 224 2 5 1 0 0","260 340 224 2 0 2 0 0","260 340 224 2 0 1 0"}){
        rejected("VALWOOD_DESIGNER_WORKSPACE 1\n"+std::string(bad)+"\n");
    }
    rejected(header+"unknown \"pine\"\n");rejected(header+"favorite\n");
    rejected(header+"favorite \"unterminated\n");rejected(header+"favorite \"../pine\"\n");
    rejected(header+"recent \"has a space\"\n");rejected(header+"favorite \"\"\n");
    rejected(header+"favorite \""+std::string(96,'x')+"\"\n");
    rejected(header+"favorite \"pine\" trailing\n");
    rejected(header+std::string("favorite \"pi\0ne\"\n",17));
    Preferences invalid=sample();invalid.layout.left=std::numeric_limits<float>::quiet_NaN();bad_encode(invalid);
    invalid=sample();invalid.layout.right=std::numeric_limits<float>::infinity();bad_encode(invalid);
    invalid=sample();invalid.layout.top=71;bad_encode(invalid);invalid=sample();invalid.layout.status=25;bad_encode(invalid);
    invalid=sample();invalid.favorites={"bad/path"};bad_encode(invalid);invalid=sample();invalid.recent={""};bad_encode(invalid);
    std::string maximum=header;
    for(std::size_t i=0;i<world_designer_assets::favorite_limit;++i)maximum+="favorite \"f_"+std::to_string(i)+"\"\n";
    for(std::size_t i=0;i<world_designer_assets::recent_limit;++i)maximum+="recent \"r_"+std::to_string(i)+"\"\n";
    assert(decode(maximum,loaded,error));assert(loaded.favorites.size()==world_designer_assets::favorite_limit&&loaded.recent.size()==world_designer_assets::recent_limit);
    rejected(maximum+"favorite \"extra\"\n");rejected(maximum+"recent \"extra\"\n");
    invalid=loaded;invalid.favorites.push_back("extra");bad_encode(invalid);
    invalid=loaded;invalid.recent.push_back("extra");bad_encode(invalid);
    assert(decode(header+"favorite \"pine\"\nfavorite \"oak\"\nfavorite \"pine\"\nrecent \"house\"\nrecent \"house\"\nrecent \"pine\"\n",loaded,error));
    assert((loaded.favorites==std::vector<std::string>{"pine","oak"}));assert((loaded.recent==std::vector<std::string>{"house","pine"}));
    // The exact byte boundary is accepted; one byte more is atomically rejected.
    std::string boundary=header;boundary.resize(32768,' ');assert(decode(boundary,loaded,error));
    rejected(boundary+' ');
}
void files(){
    TemporaryDirectory folder;const auto file=folder.path+"/workspace";
    auto p=sample();std::string error;assert(save(file,p,error)&&error.empty());
    assert(read(file)==encoded(p));assert(std::filesystem::is_regular_file(file));no_temporary_files(folder.path);
    Preferences loaded;assert(load(file,loaded,error)&&encoded(loaded)==encoded(p));
    const auto preserved=encoded(p);
    // Invalid save never replaces the old file and never creates a temp file.
    auto invalid=p;invalid.tool=25;assert(!save(file,invalid,error)&&read(file)==preserved);no_temporary_files(folder.path);
    invalid=p;invalid.favorites.push_back("../bad");assert(!save(file,invalid,error)&&read(file)==preserved);no_temporary_files(folder.path);
    const auto directory=folder.path+"/directory";assert(std::filesystem::create_directory(directory));
    assert(!save(directory,p,error)&&std::filesystem::is_directory(directory));no_temporary_files(folder.path);
    assert(!save(folder.path+"/missing/target",p,error));no_temporary_files(folder.path);
    auto atomic_load_failure=[&](const std::string &name){loaded=p;assert(!load(name,loaded,error));assert(!error.empty()&&encoded(loaded)==preserved);};
    atomic_load_failure(folder.path+"/absent");
    write(file,header+"favorite \"../bad\"\n");atomic_load_failure(file);
    std::string boundary=header;boundary.resize(32768,' ');write(file,boundary);assert(load(file,loaded,error));
    write(file,boundary+' ');atomic_load_failure(file);
    // A large file with a valid prefix must be rejected without accepting its
    // prefix. The loader's read loop is bounded at 32 KiB + one read chunk.
    write(file,header+std::string(1024*1024,' '));atomic_load_failure(file);
    assert(save(file,p,error));
    // Atomic replacement also protects a reader of the old inode. A workspace
    // symlink is replaced in this same directory, never followed for writing.
    const auto outside=folder.path+"/original";write(outside,"original payload");
    assert(std::filesystem::remove(file));std::filesystem::create_symlink(outside,file);
    assert(save(file,p,error));assert(!std::filesystem::is_symlink(file));assert(read(outside)=="original payload");
    assert(read(file)==preserved);no_temporary_files(folder.path);
    // Readers must observe one complete revision during repeated replacements.
    auto other=p;other.tool=7;other.favorites={"fern0","purchased_house_b"};const auto text_a=encoded(p),text_b=encoded(other);
    std::atomic<bool> done{false},failed{false};std::atomic<unsigned> reads{0};
    std::thread reader([&]{while(!done.load()){Preferences snapshot;std::string detail;if(!load(file,snapshot,detail)){failed=true;break;}
        const auto text=encoded(snapshot);if(text!=text_a&&text!=text_b){failed=true;break;}++reads;}});
    while(reads==0&&!failed)std::this_thread::yield();
    for(unsigned i=0;i<40;++i)assert(save(file,i%2?p:other,error));
    done=true;reader.join();assert(!failed&&reads>0);no_temporary_files(folder.path);
}
void search(){
    const std::string name="Purchased Castle House A";
    assert(matches(name,"pUrChAsEd HOUSE"));assert(matches(name,"house CASTLE"));assert(matches(name,"  Castle   a  "));
    assert(!matches(name,"castle jungle"));assert(!matches(name,"ZZZ"));assert(matches(name,""));assert(matches(name,nullptr));
    assert(matches(name,"\tHOUSE\n castle\r"));assert(matches(name,"\t\r\n"));
    assert(matches("","")&&!matches("","castle"));
}
}
int main(){codec();files();search();std::puts("designer workspace: atomic codec/load rejection, 32 KiB bound, preference limits/normalization, same-directory save cleanup, concurrent readers, and case-insensitive AND search passed");}

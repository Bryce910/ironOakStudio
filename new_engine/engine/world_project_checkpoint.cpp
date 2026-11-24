#include "engine/world_platform.h"
#include "engine/world_project_checkpoint.h"
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <vector>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
namespace world_project {
namespace {
namespace fs=std::filesystem;
constexpr size_t file_limit=4u*1024*1024,total_limit=32u*1024*1024;
bool relative(const std::string &s){if(s.empty()||s.size()>512||s.find_first_of("\r\n\t\\")!=std::string::npos||s.find('\0')!=std::string::npos||fs::path(s).is_absolute())return false;for(const auto &p:fs::path(s))if(p==".."||p=="."||p.empty())return false;return true;}
bool path(const std::string &root,const std::string &name,fs::path &out,std::string &e){
    if(!relative(name)){e="Checkpoint path must be project relative without traversal";return false;}
    std::error_code ec;auto base=fs::canonical(root,ec);if(ec||!fs::is_directory(base)){e="Missing project directory";return false;}
    out=base;for(const auto &part:fs::path(name)){out/=part;auto status=fs::symlink_status(out,ec);if(ec&&ec!=std::errc::no_such_file_or_directory){e="Cannot inspect project path";return false;}ec.clear();if(fs::is_symlink(status)){e="Symlinks are not allowed in checkpoint paths";return false;}}
    return true;
}
uint64_t hash(const std::string &s){uint64_t h=14695981039346656037ull;for(unsigned char c:s){h^=c;h*=1099511628211ull;}return h;}
bool equal(const File &a,const File &b){return a.path==b.path&&a.exists==b.exists&&(!a.exists||a.bytes==b.bytes);}
bool get(const std::string &root,const std::string &name,File &f,std::string &e){
    fs::path p;if(!path(root,name,p,e))return false;std::error_code ec;f={name,"",false};if(!fs::exists(p,ec))return !ec;
    if(!fs::is_regular_file(p,ec)||ec||fs::file_size(p,ec)>file_limit||ec){e="Missing, oversized or nonregular authoring asset: "+name;return false;}
    std::ifstream in(p,std::ios::binary);f.bytes.assign(std::istreambuf_iterator<char>(in),{});if(!in||in.bad()||f.bytes.size()>file_limit){e="Cannot read authoring asset: "+name;return false;}f.exists=true;return true;
}
bool sync_directory(const fs::path &p){
#ifdef _WIN32
return vw_directory_barrier(p.u8string().c_str())==0;
#else
int fd=::vw_open(p.c_str(),O_RDONLY|O_DIRECTORY);if(fd<0)return false;bool ok=fsync(fd)==0;close(fd);return ok;
#endif
}
bool atomic(const fs::path &p,const std::string &bytes,std::string &e){
    std::error_code ec;fs::create_directories(p.parent_path(),ec);if(ec){e="Cannot create checkpoint directory";return false;}
    std::string pattern=p.string()+".tmp.XXXXXX";std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);int fd=mkstemp(name.data());if(fd<0){e="Cannot create checkpoint transaction";return false;}
    size_t done=0;bool ok=true;while(done<bytes.size()){auto n=write(fd,bytes.data()+done,bytes.size()-done);if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}done+=size_t(n);}if(ok)ok=fsync(fd)==0;if(close(fd))ok=false;if(ok)ok=vw_replace(name.data(),p.c_str())==0;if(ok)ok=sync_directory(p.parent_path());if(!ok){unlink(name.data());e="Checkpoint write failed; recovery journal retained";}return ok;
}
std::string encode(const Files &f){std::ostringstream o;o<<"VALWOOD_PROJECT 1 "<<f.size()<<'\n';for(const auto &a:f)o<<std::quoted(a.path)<<' '<<a.exists<<' '<<a.bytes.size()<<' '<<hash(a.bytes)<<'\n'<<a.bytes;return o.str();}
bool decode(const std::string &s,Files &out,std::string &e){
    if(s.size()>total_limit+128*1024){e="Oversized checkpoint";return false;}std::istringstream in(s);std::string magic;unsigned version=0,n=0;if(!(in>>magic>>version>>n)||magic!="VALWOOD_PROJECT"||version!=1||n>128||in.get()!='\n'){e="Unsupported checkpoint";return false;}Files f;
    for(unsigned i=0;i<n;i++){File a;int exists=0;size_t size=0;uint64_t sum=0;if(!(in>>std::quoted(a.path)>>exists>>size>>sum)||exists<0||exists>1||size>file_limit||in.get()!='\n'){e="Malformed checkpoint record";return false;}a.exists=exists;a.bytes.resize(size);if(size&&!in.read(&a.bytes[0],std::streamsize(size))){e="Truncated checkpoint";return false;}if(hash(a.bytes)!=sum){e="Checkpoint checksum mismatch";return false;}f.push_back(std::move(a));}
    if(in.peek()!=EOF||!validate(f,e)){if(e.empty())e="Trailing checkpoint data";return false;}out=std::move(f);return true;
}
bool raw(const fs::path &p,std::string &s,size_t limit,std::string &e){std::error_code ec;if(!fs::is_regular_file(p,ec)||ec||fs::file_size(p,ec)>limit||ec){e="Missing or oversized checkpoint file";return false;}std::ifstream in(p,std::ios::binary);s.assign(std::istreambuf_iterator<char>(in),{});if(!in||in.bad()){e="Cannot read checkpoint";return false;}return true;}
struct Lock {int fd=-1;~Lock(){if(fd>=0){flock(fd,LOCK_UN);close(fd);}}bool take(const fs::path &p,std::string &e){fd=::vw_open(p.c_str(),O_CREAT|O_RDWR|O_NOFOLLOW,0600);if(fd<0||flock(fd,LOCK_EX|LOCK_NB)){e="Another project save/recovery is active";return false;}return true;}};
bool stage_locked(const std::string &root,const std::string &name,const Files &after,const Files &expected,std::string &e){
    if(!validate(after,e)||!validate(expected,e)||after.size()!=expected.size()){if(e.empty())e="Checkpoint guard set mismatch";return false;}
    fs::path journal;if(!path(root,name+".journal",journal,e))return false;if(fs::exists(journal)){e="Recover the interrupted project save before saving again";return false;}
    for(size_t i=0;i<after.size();i++){if(after[i].path!=expected[i].path||after[i].path==name||after[i].path.rfind(name+".",0)==0){e="Checkpoint guard path mismatch or reserved path";return false;}File current;if(!get(root,after[i].path,current,e))return false;if(!equal(current,expected[i])){e="File changed outside this save: "+after[i].path;return false;}}
    const auto a=encode(expected),b=encode(after);return atomic(journal,std::to_string(a.size())+"\n"+a+b,e);
}
bool recover_locked(const std::string &root,const std::string &name,bool finish,std::string &e){
    fs::path j,c;if(!path(root,name+".journal",j,e)||!path(root,name,c,e))return false;std::string s;if(!raw(j,s,2*(total_limit+128*1024)+64,e))return false;const auto line=s.find('\n');size_t split=0;try{size_t used=0;split=std::stoull(s.substr(0,line),&used);if(used!=line)throw 0;}catch(...){e="Malformed checkpoint journal";return false;}if(line==std::string::npos||split>s.size()-line-1){e="Truncated checkpoint journal";return false;}Files before,after;if(!decode(s.substr(line+1,split),before,e)||!decode(s.substr(line+1+split),after,e)||before.size()!=after.size())return false;
    for(size_t i=0;i<before.size();i++){if(before[i].path!=after[i].path||before[i].path==name||before[i].path.rfind(name+".",0)==0){e="Invalid journal path set";return false;}File current;if(!get(root,before[i].path,current,e))return false;if(!equal(current,before[i])&&!equal(current,after[i])){e="Recovery conflict; preserve external edits before retrying: "+current.path;return false;}}
    for(const auto &f:finish?after:before){fs::path p;if(!path(root,f.path,p,e))return false;if(f.exists){if(!atomic(p,f.bytes,e))return false;}else{std::error_code ec;fs::remove(p,ec);if(ec){e="Cannot remove newly created recovery asset";return false;}if(fs::exists(p.parent_path())&&!sync_directory(p.parent_path())){e="Cannot sync recovery directory";return false;}}}
    if(finish){std::error_code ec;if(fs::exists(c,ec)){std::string previous;if(!raw(c,previous,total_limit+128*1024,e)||!atomic(c.string()+".previous",previous,e))return false;}if(!atomic(c,encode(after),e))return false;}
    if(unlink(j.c_str())||!sync_directory(j.parent_path())){e="Saved, but journal cleanup needs retry";return false;}return true;
}
}
bool validate(const Files &f,std::string &e){size_t total=0;std::set<std::string> paths;if(f.empty()||f.size()>128){e="Checkpoint needs 1–128 authoring assets";return false;}for(const auto &a:f){total+=a.bytes.size();if(!relative(a.path)||!paths.insert(a.path).second||a.bytes.size()>file_limit||total>total_limit||(!a.exists&&!a.bytes.empty())){e="Invalid, duplicate or oversized checkpoint asset";return false;}}return true;}
bool read(const std::string &root,const std::vector<std::string> &names,Files &out,std::string &e){Files f;for(const auto &n:names){File a;if(!get(root,n,a,e))return false;f.push_back(std::move(a));}if(!validate(f,e))return false;out=std::move(f);return true;}
bool load(const std::string &root,const std::string &name,Files &out,std::string &e){fs::path p;std::string s;return path(root,name,p,e)&&raw(p,s,total_limit+128*1024,e)&&decode(s,out,e);}
bool pending(const std::string &root,const std::string &name){fs::path p;std::string e;return path(root,name+".journal",p,e)&&fs::exists(p);}
bool stage(const std::string &root,const std::string &name,const Files &a,const Files &b,std::string &e){fs::path p;if(!path(root,name+".lock",p,e))return false;Lock lock;return lock.take(p,e)&&stage_locked(root,name,a,b,e);}
bool recover(const std::string &root,const std::string &name,bool finish,std::string &e){fs::path p;if(!path(root,name+".lock",p,e))return false;Lock lock;return lock.take(p,e)&&recover_locked(root,name,finish,e);}
bool commit(const std::string &root,const std::string &name,const Files &a,const Files &b,std::string &e){fs::path p;if(!path(root,name+".lock",p,e))return false;Lock lock;return lock.take(p,e)&&stage_locked(root,name,a,b,e)&&recover_locked(root,name,true,e);}
}

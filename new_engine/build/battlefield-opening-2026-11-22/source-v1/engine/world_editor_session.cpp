#include "engine/world_platform.h"
#include "engine/world_editor_session.h"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace world_editor {
namespace {
constexpr std::size_t payload_limit=16*1024*1024,envelope_limit=2*payload_limit+1024;
std::string digest(const std::string &bytes){
    // Content identity/integrity, not a cryptographic authentication boundary.
    std::uint64_t h=14695981039346656037ull;
    for(unsigned char c:bytes){h^=c;h*=1099511628211ull;}
    std::ostringstream out;out<<std::hex<<std::setfill('0')<<std::setw(16)<<h;return out.str();
}
bool read_bytes(const std::string &path,std::string &bytes,std::size_t limit,std::string &error,bool missing_ok=false){
    const int fd=::vw_open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0){if(missing_ok&&errno==ENOENT){bytes.clear();return true;}error="Cannot read recovery/base file: "+std::string(std::strerror(errno));return false;}
    struct stat st={};bool ok=fstat(fd,&st)==0&&S_ISREG(st.st_mode)&&st.st_size>=0&&std::uint64_t(st.st_size)<=limit;
    std::string next;if(ok){next.resize(std::size_t(st.st_size));std::size_t done=0;while(done<next.size()){const auto n=read(fd,next.data()+done,next.size()-done);if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}done+=std::size_t(n);}}
    if(close(fd)!=0)ok=false;
    if(!ok){error="Recovery/base file is unreadable, non-regular or oversized";return false;}bytes=std::move(next);return true;
}
bool file_hash(const std::string &path,std::string &hash,std::string &error){
    std::string bytes;if(!read_bytes(path,bytes,payload_limit,error,true))return false;
    // Empty and missing are different saved bases.
    struct stat st={};if(lstat(path.c_str(),&st)<0){if(errno==ENOENT){hash="missing";return true;}error="Cannot inspect saved base";return false;}
    hash=digest(bytes);return true;
}
struct Temporary {
    std::string path;int fd=-1;
    explicit Temporary(const std::string &prefix){std::vector<char> p(prefix.begin(),prefix.end());p.insert(p.end(),{'X','X','X','X','X','X',0});fd=mkstemp(p.data());if(fd>=0)path=p.data();}
    ~Temporary(){if(fd>=0)close(fd);if(!path.empty())unlink(path.c_str());}
};
bool write_fd(int fd,const std::string &bytes){
    std::size_t done=0;while(done<bytes.size()){auto n=write(fd,bytes.data()+done,bytes.size()-done);if(n<0&&errno==EINTR)continue;if(n<=0)return false;done+=std::size_t(n);}return fsync(fd)==0;
}
bool sync_parent(const std::string &path){
    const auto directory=std::filesystem::path(path).parent_path().string();
#ifdef _WIN32
    return vw_directory_barrier(directory.c_str())==0;
#else
    int fd=::vw_open(directory.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(fd<0)return false;const bool ok=fsync(fd)==0;close(fd);return ok;
#endif
}
bool atomic_bytes(const std::string &path,const std::string &bytes,std::string &error){
    Temporary transaction(path+".tmp.");if(transaction.fd<0||!write_fd(transaction.fd,bytes)){error="Cannot write/sync recovery transaction; previous generation preserved";return false;}
    if(close(transaction.fd)!=0){transaction.fd=-1;error="Cannot close recovery transaction";return false;}transaction.fd=-1;
    if(vw_replace(transaction.path.c_str(),path.c_str())!=0){error="Cannot publish recovery transaction; previous generation preserved";return false;}
    transaction.path.clear();if(!sync_parent(path)){error="Recovery published but directory durability could not be confirmed";return false;}return true;
}
// Reuse the production serializer/parser instead of maintaining a second schema.
// These temporary transactions occur only at startup/explicit actions or worker
// saves, never on passive frame ticks. They are unique and never the project.
bool encode(const std::string &prefix,const world_studio::Document &document,std::string &payload,std::string &error){
    if(!world_studio::valid(document,error))return false;
    Temporary scratch(prefix+".document.");if(scratch.fd<0){error="Cannot create document serialization transaction";return false;}
    close(scratch.fd);scratch.fd=-1;
    return world_studio::save(scratch.path.c_str(),document,error)&&read_bytes(scratch.path,payload,payload_limit,error);
}
bool decode(const std::string &prefix,const std::string &payload,world_studio::Document &document,std::string &error){
    Temporary scratch(prefix+".inspect.");if(scratch.fd<0||!write_fd(scratch.fd,payload)){error="Cannot stage recovery validation";return false;}
    close(scratch.fd);scratch.fd=-1;return world_studio::load(scratch.path.c_str(),document,error);
}
struct Envelope {
    std::uint64_t revision=0;std::string base,origin,applied_hash,draft_hash,applied,draft;
};
std::string pack(const Envelope &e){
    std::ostringstream o;o<<"VALWOOD_EDITOR_RECOVERY 1\n"<<e.revision<<' '<<e.base<<' '<<e.origin<<' '<<e.applied_hash<<' '<<e.draft_hash<<' '<<e.applied.size()<<' '<<e.draft.size()<<'\n'<<e.applied<<e.draft;return o.str();
}
bool hash_token(const std::string &s,bool missing=false){return (missing&&s=="missing")||(s.size()==16&&std::all_of(s.begin(),s.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}));}
bool unpack(const std::string &bytes,Envelope &e,std::string &error){
    const auto first=bytes.find('\n'),second=first==std::string::npos?first:bytes.find('\n',first+1);
    if(first==std::string::npos||second==std::string::npos||second>512||bytes.substr(0,first)!="VALWOOD_EDITOR_RECOVERY 1"){error="Invalid recovery header";return false;}
    std::istringstream in(bytes.substr(first+1,second-first-1));std::size_t a=0,d=0;std::string extra;
    if(!(in>>e.revision>>e.base>>e.origin>>e.applied_hash>>e.draft_hash>>a>>d)||(in>>extra)||!hash_token(e.base,true)||!hash_token(e.origin)||!hash_token(e.applied_hash)||!hash_token(e.draft_hash)||!a||!d||a>payload_limit||d>payload_limit||bytes.size()-second-1!=a+d){error="Invalid/incomplete recovery metadata";return false;}
    e.applied=bytes.substr(second+1,a);e.draft=bytes.substr(second+1+a,d);
    if(digest(e.applied)!=e.applied_hash||digest(e.draft)!=e.draft_hash){error="Recovery snapshot integrity mismatch";return false;}return true;
}
bool inspect(const std::string &path,Envelope &e,std::string &error){std::string bytes;return read_bytes(path,bytes,envelope_limit,error)&&unpack(bytes,e,error);}
SaveResult store(const std::string &project,const std::string &path,const std::string &base,const std::string &origin,
                 unsigned backups,std::uint64_t revision,const world_studio::Document &applied,const world_studio::Document &draft){
    SaveResult result;result.revision=revision;Envelope e;e.revision=revision;e.base=base;e.origin=origin;
    if(!encode(path,applied,e.applied,result.error)||!encode(path,draft,e.draft,result.error))return result;
    e.applied_hash=digest(e.applied);e.draft_hash=digest(e.draft);
    std::string current;if(!file_hash(project,current,result.error)||current!=base){if(result.error.empty())result.error="Saved base changed externally; recovery write deferred";return result;}
    // Keep current valid until the new complete transaction is published. Copy
    // bounded old generations atomically, so an interrupted rotation never
    // leaves a partial current file. Corrupt old files are never propagated.
    for(unsigned i=backups;i>0;i--){const auto source=i==1?path:path+"."+std::to_string(i-1);std::string old,ignored;Envelope checked;
        if(read_bytes(source,old,envelope_limit,ignored)&&unpack(old,checked,ignored))if(!atomic_bytes(path+"."+std::to_string(i),old,result.error))return result;
    }
    // Recheck at publication boundary; no per-frame hashing is involved.
    if(!file_hash(project,current,result.error)||current!=base){if(result.error.empty())result.error="Saved base changed during recovery transaction";return result;}
    result.ok=atomic_bytes(path,pack(e),result.error);return result;
}
}

Session::Session(Config config):config_(config){}
Session::~Session(){if(worker_.valid())worker_.wait();}
bool Session::open(const std::string &path,const world_studio::Document &base,double now,std::string &error){
    error.clear();if(opened_||!std::isfinite(now)||now<0||!std::isfinite(config_.debounce_seconds)||!std::isfinite(config_.maximum_interval_seconds)||config_.debounce_seconds<.1||config_.debounce_seconds>60||config_.maximum_interval_seconds<config_.debounce_seconds||config_.maximum_interval_seconds>300||config_.backup_count>4){error="Invalid/already open editor session";return false;}
    std::error_code ec;auto absolute=std::filesystem::absolute(path,ec).lexically_normal();if(ec||path.empty()||absolute.filename().empty()||!std::filesystem::is_directory(absolute.parent_path(),ec)){error="Editor project path requires an existing parent directory";return false;}
    const auto project=absolute.string(),recovery=project+".recovery";std::string file,payload;
    if(!file_hash(project,file,error)||!encode(recovery,base,payload,error))return false;
    project_path_=project;recovery_path_=recovery;base_file_hash_=file;base_document_hash_=digest(payload);applied_=base;opened_=true;last_clock_=now;return true;
}
bool Session::changed(double now){
    if(!save_allowed()||!std::isfinite(now)||now<last_clock_||stats_.revision==std::numeric_limits<std::uint64_t>::max())return false;
    last_clock_=last_change_=now;if(!stats_.dirty)first_dirty_=now;stats_.dirty=true;stats_.revision++;return true;
}
bool Session::applied(const world_studio::Document &document,std::string &error){
    error.clear();if(!opened_||!world_studio::valid(document,error)){if(error.empty())error="No editor session";return false;}applied_=document;applied_revision_++;return true;
}
bool Session::poll(std::string &error){
    error.clear();if(!worker_.valid()||worker_.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return true;
    SaveResult result;try{result=worker_.get();}catch(const std::exception &e){result.error="Recovery worker failed: "+std::string(e.what());}
    stats_.worker_running=false;
    if(!result.ok){stats_.failures++;next_attempt_=last_clock_+std::max(1.,config_.debounce_seconds);error=result.error;return false;}
    stats_.writes++;stats_.saved_revision=result.revision;if(result.revision==stats_.revision)stats_.dirty=false;stats_.recovery=Recovery::Available;return true;
}
bool Session::wait(std::string &error){if(worker_.valid())worker_.wait();return poll(error);}
bool Session::flush_recovery(const world_studio::Document &draft,double now,std::string &error){
    error.clear();
    if(!save_allowed()||!std::isfinite(now)||now<last_clock_){error="Stop Play and finish scene restoration before preserving recovery; use a valid editor clock";return false;}
    if(!world_studio::valid(draft,error))return false;
    last_clock_=now;
    // Explicit retry is allowed after an older worker failed. Its published
    // generation remains untouched until store has completed the replacement.
    std::string previous_error;wait(previous_error);
    SaveResult result;
    try{result=store(project_path_,recovery_path_,base_file_hash_,base_document_hash_,config_.backup_count,stats_.revision,applied_,draft);}
    catch(const std::exception &e){result.error="Recovery flush failed: "+std::string(e.what());}
    if(!result.ok){stats_.dirty=true;stats_.failures++;next_attempt_=now+std::max(1.,config_.debounce_seconds);error=result.error;return false;}
    stats_.writes++;stats_.saved_revision=result.revision;stats_.dirty=false;stats_.recovery=Recovery::Available;first_dirty_=now;next_attempt_=0;return true;
}
bool Session::tick(double now,const world_studio::Document &draft,bool allow_work,std::string &error){
    if(!poll(error))return false;
    if(!opened_||!std::isfinite(now)||now<last_clock_){error="Invalid editor autosave clock/session";return false;}last_clock_=now;
    if(!allow_work||stats_.mode!=Mode::Editing||!stats_.dirty||worker_.valid()||now<next_attempt_||(now-last_change_<config_.debounce_seconds&&now-first_dirty_<config_.maximum_interval_seconds))return true;
    if(!world_studio::valid(draft,error))return false;
    try {
        worker_=std::async(std::launch::async,store,project_path_,recovery_path_,base_file_hash_,base_document_hash_,config_.backup_count,stats_.revision,applied_,draft);
        stats_.worker_running=true;
        // This snapshot covers all earlier edits. A revision arriving while it
        // saves gets a new maximum-interval window, rather than inheriting the
        // expired timer and causing continuous saves during sustained editing.
        first_dirty_=now;
    }
    catch(const std::exception &e){error="Cannot start bounded recovery worker: "+std::string(e.what());return false;}return true;
}
Recovery Session::probe(std::string &error,unsigned generation){
    error.clear();if(!opened_){error="No editor session";return stats_.recovery=Recovery::Corrupt;}
    if(worker_.valid())return stats_.recovery=Recovery::Busy;
    if(generation>config_.backup_count){error="Recovery generation outside configured retention";return stats_.recovery=Recovery::Corrupt;}
    recovery_generation_=generation;const auto path=generation?recovery_path_+"."+std::to_string(generation):recovery_path_;
    struct stat st={};if(lstat(path.c_str(),&st)!=0&&errno==ENOENT)return stats_.recovery=Recovery::None;
    Envelope e;world_studio::Document a,d;std::string file;
    if(!inspect(path,e,error)||!decode(recovery_path_,e.applied,a,error)||!decode(recovery_path_,e.draft,d,error))return stats_.recovery=Recovery::Corrupt;
    if(!file_hash(project_path_,file,error))return stats_.recovery=Recovery::Corrupt;
    if(file!=base_file_hash_||e.base!=base_file_hash_||e.origin!=base_document_hash_){error="Recovery belongs to a different saved/applied base; preserved for manual inspection";return stats_.recovery=Recovery::Stale;}
    probed_applied_revision_=applied_revision_;return stats_.recovery=Recovery::Available;
}
bool Session::recover(world_studio::Document &draft,double now,std::string &error){
    error.clear();if(!save_allowed()||stats_.recovery!=Recovery::Available||applied_revision_!=probed_applied_revision_){error="Inspect recovery again before recovering into the current applied scene";return false;}
    if(!std::isfinite(now)||now<last_clock_){error="Invalid recovery clock";return false;}
    if(probe(error,recovery_generation_)!=Recovery::Available)return false;
    Envelope e;world_studio::Document next;std::string file;
    const auto path=recovery_generation_?recovery_path_+"."+std::to_string(recovery_generation_):recovery_path_;
    if(!inspect(path,e,error)||!decode(recovery_path_,e.draft,next,error)||!file_hash(project_path_,file,error))return false;
    if(e.base!=base_file_hash_||e.origin!=base_document_hash_||file!=base_file_hash_){error="Recovery base changed during inspection";return false;}
    draft=std::move(next);return changed(now);
}
bool Session::discard(std::string &error){
    error.clear();if(!save_allowed()||worker_.valid()){error="Finish pending recovery work and stop Play before discarding";return false;}
    for(unsigned i=0;i<=4;i++){const auto path=i?recovery_path_+"."+std::to_string(i):recovery_path_;if(unlink(path.c_str())!=0&&errno!=ENOENT){error="Could not remove a recovery generation";return false;}}
    if(!sync_parent(recovery_path_)){error="Could not sync recovery cleanup";return false;}stats_.recovery=Recovery::None;return true;
}
bool Session::clean_saved(const world_studio::Document &document,std::string &error){
    error.clear();if(!save_allowed()){error="Stop Play and choose Keep or Discard before saving authoring data";return false;}
    // A failed old worker cannot prevent acknowledging a later explicit save.
    std::string ignored;wait(ignored);std::string file,payload;
    if(!file_hash(project_path_,file,error)||file=="missing"||!encode(recovery_path_,document,payload,error)){if(error.empty())error="Explicit project save is not present";return false;}
    world_studio::Document disk;std::string canonical;if(!world_studio::load(project_path_.c_str(),disk,error)||!encode(recovery_path_,disk,canonical,error)||canonical!=payload){if(error.empty())error="Project on disk differs from explicit saved document";return false;}
    if(!discard(error))return false;
    base_file_hash_=file;base_document_hash_=digest(payload);stats_.dirty=false;stats_.saved_revision=stats_.revision;return true;
}
bool Session::play_begin(const world_studio::Document &draft,const world_studio::Document &applied,std::string &error){
    error.clear();if(!save_allowed()||!world_studio::valid(draft,error)||!world_studio::valid(applied,error)){if(error.empty())error="Play is already active or no editor session exists";return false;}
    play_draft_=draft;play_applied_=applied;stats_.mode=Mode::Playing;return true;
}
bool Session::request_stop(){if(stats_.mode!=Mode::Playing)return false;stats_.mode=Mode::StopPending;return true;}
bool Session::play_stop(StopChoice choice,world_studio::Document &draft,world_studio::Document &applied,double now,std::string &error){
    error.clear();if(stats_.mode!=Mode::StopPending||!std::isfinite(now)||now<last_clock_){error="Request Stop and explicitly choose Keep or Discard first";return false;}
    if(choice==StopChoice::KeepAuthoringChanges&&(!world_studio::valid(draft,error)||!world_studio::valid(applied,error)))return false;
    if(choice==StopChoice::DiscardPlayChanges){draft=play_draft_;applied=play_applied_;}
    applied_=applied;applied_revision_++;play_draft_={};play_applied_={};stats_.mode=Mode::Editing;
    // Keep is explicitly authoring-only. Runtime actor/physics state is reset by
    // the host's existing transactional Apply, never serialized by this module.
    if(choice==StopChoice::KeepAuthoringChanges)return changed(now);
    last_clock_=now;return true;
}
}

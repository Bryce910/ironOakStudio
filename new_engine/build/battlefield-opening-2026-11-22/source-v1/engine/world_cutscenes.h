#ifndef VALWOOD_WORLD_CUTSCENES_H
#define VALWOOD_WORLD_CUTSCENES_H

#include "world_platform.h"
#include "world_cinematics.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

// Authored world cutscenes wrap the existing .vct camera/audio timeline. Load,
// prepare and configure only at scene/editor boundaries. Runtime::tick performs
// no filesystem/media work, never moves the player and never awards quest credit.
namespace world_cutscenes {
constexpr unsigned sequence_limit=64,progress_limit=1024;
constexpr std::size_t document_byte_limit=128*1024,progress_byte_limit=32768;
constexpr double skip_hold_seconds=.8,maximum_step_seconds=.1;
using Vec3=world_cinematics::Vec3;
using Shape=world_cinematics::Shape;
enum class TriggerKind : unsigned {Manual,Area,Interact};
struct Trigger {
    TriggerKind kind=TriggerKind::Manual;Shape shape=Shape::Box;
    Vec3 center={},extent={3,2,3}; // Sphere: extent.x radius; Box: half extents.
    std::uint64_t target_id=0;double cooldown_seconds=1;
};
struct Sequence {
    std::uint64_t id=1;std::string name="Cutscene",timeline="cinematics/sequence.vct";
    bool enabled=true,once=true;int priority=0;
    bool letterbox=true,skip_allowed=true;Trigger trigger;
};
struct Document {std::vector<Sequence> sequences;};
struct PreparedSequence {
    Sequence sequence;world_cinematics::Document timeline;
    std::string timeline_path; // Canonical absolute source, useful to the host.
};
struct PreparedLibrary {Document document;std::vector<PreparedSequence> sequences;};
struct Progress {std::vector<std::uint64_t> completed_ids;};

namespace detail {
inline bool range(double x,double a,double b){return std::isfinite(x)&&x>=a&&x<=b;}
inline bool point(Vec3 p){return range(p.x,-1e8,1e8)&&range(p.y,-1e8,1e8)&&range(p.z,-1e8,1e8);}
inline bool title(const std::string &s){return !s.empty()&&s.size()<=95&&s.find_first_of("\r\n\t")==std::string::npos&&s.find('\0')==std::string::npos;}
inline bool relative_path(const std::string &s){
    if(s.empty()||s.size()>239||s.front()=='/'||s.back()=='/'||s.find_first_of("\\:\r\n\t")!=std::string::npos||s.find('\0')!=std::string::npos)return false;
    std::size_t begin=0;while(begin<s.size()){
        auto end=s.find('/',begin);if(end==std::string::npos)end=s.size();
        const auto part=s.substr(begin,end-begin);if(part.empty()||part.front()=='.')return false;
        begin=end+1;
    }return true;
}
inline bool uint64(const std::string &s,std::uint64_t &out){
    if(s.empty()||s.find_first_not_of("0123456789")!=std::string::npos)return false;
    std::uint64_t next=0;const auto r=std::from_chars(s.data(),s.data()+s.size(),next);
    if(r.ec!=std::errc()||r.ptr!=s.data()+s.size())return false;out=next;return true;
}
inline bool same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
inline bool same(const Sequence &a,const Sequence &b){
    return a.id==b.id&&a.name==b.name&&a.timeline==b.timeline&&a.enabled==b.enabled&&a.once==b.once&&a.priority==b.priority&&a.letterbox==b.letterbox&&a.skip_allowed==b.skip_allowed&&
        a.trigger.kind==b.trigger.kind&&a.trigger.shape==b.trigger.shape&&same(a.trigger.center,b.trigger.center)&&same(a.trigger.extent,b.trigger.extent)&&a.trigger.target_id==b.trigger.target_id&&a.trigger.cooldown_seconds==b.trigger.cooldown_seconds;
}
inline bool read(const char *path,std::size_t limit,std::string &out,std::string &error){
    if(!path||!*path){error="No cutscene file was selected.";return false;}
    std::error_code ec;const auto p=std::filesystem::u8path(path);
    if(!std::filesystem::is_regular_file(p,ec)||ec){error="Cutscene file is missing or not a regular file: "+std::string(path);return false;}
    const auto size=std::filesystem::file_size(p,ec);
    if(ec||size==0||size>limit){error="Cutscene file is empty or exceeds its budget.";return false;}
    std::ifstream f(p,std::ios::binary);std::string next(std::size_t(size),'\0');
    if(!f||!f.read(next.data(),std::streamsize(size))){error="Cutscene file could not be read.";return false;}
    out=std::move(next);return true;
}
inline bool atomic_write(const char *path,const std::string &bytes,std::string &error){
    if(!path||!*path){error="No cutscene save path was selected.";return false;}
    const auto target=std::filesystem::u8path(path),parent=target.parent_path().empty()?std::filesystem::path("."):target.parent_path();
    std::error_code ec;std::filesystem::create_directories(parent,ec);
    if(ec){error="Could not create cutscene save folder: "+ec.message();return false;}
    const auto target_string=target.u8string();const auto pattern=target_string+".tmp-XXXXXX";
    std::vector<char> temporary(pattern.begin(),pattern.end());temporary.push_back(0);
    const int fd=mkstemp(temporary.data());if(fd<0){error="Could not stage cutscene save; the previous file was preserved.";return false;}
    FILE *f=fdopen(fd,"wb");bool ok=false;
    if(f){ok=std::fwrite(bytes.data(),1,bytes.size(),f)==bytes.size()&&std::fflush(f)==0&&fsync(fd)==0;if(std::fclose(f)!=0)ok=false;}else close(fd);
    if(ok)ok=vw_replace(temporary.data(),target_string.c_str())==0;
    if(!ok){unlink(temporary.data());error="Could not replace cutscene save; the previous file was preserved.";return false;}
    error.clear();return true;
}
inline bool contained(const std::filesystem::path &root,const std::filesystem::path &p){
    auto a=root.begin(),b=p.begin();for(;a!=root.end();++a,++b)if(b==p.end()||*a!=*b)return false;return b!=p.end();
}
inline bool inside(const Trigger &t,Vec3 p){
    const double x=std::abs(p.x-t.center.x),y=std::abs(p.y-t.center.y),z=std::abs(p.z-t.center.z);
    if(t.shape==Shape::Sphere)return std::hypot(std::hypot(x,y),z)<=t.extent.x;
    return x<=t.extent.x&&y<=t.extent.y&&z<=t.extent.z;
}
inline std::uint64_t fingerprint(const std::string &s){std::uint64_t h=14695981039346656037ull;for(unsigned char c:s){h^=c;h*=1099511628211ull;}return h;}
}

inline bool validate(const Document &d,std::string &error){
    error.clear();auto fail=[&](const char *s){error=s;return false;};
    if(d.sequences.size()>sequence_limit)return fail("At most 64 world cutscenes may be authored.");
    for(std::size_t i=0;i<d.sequences.size();++i){const auto &s=d.sequences[i];const auto &t=s.trigger;
        if(!s.id||!detail::title(s.name)||!detail::relative_path(s.timeline)||std::filesystem::u8path(s.timeline).extension()!=".vct")return fail("Cutscene needs a unique nonzero ID, name and project-relative .vct timeline.");
        if(s.priority< -1000||s.priority>1000||unsigned(t.kind)>unsigned(TriggerKind::Interact)||unsigned(t.shape)>unsigned(Shape::Box)||!detail::point(t.center)||
            !detail::range(t.extent.x,.01,100000)||!detail::range(t.extent.y,.01,100000)||!detail::range(t.extent.z,.01,100000)||!detail::range(t.cooldown_seconds,0,3600))return fail("Cutscene trigger geometry, priority or cooldown is invalid.");
        if(t.kind==TriggerKind::Interact&&!t.target_id)return fail("Interaction cutscenes need a persistent target ID.");
        for(std::size_t j=0;j<i;++j)if(d.sequences[j].id==s.id)return fail("Duplicate cutscene ID.");
    }return true;
}
inline bool encode(const Document &d,std::string &out,std::string &error){
    if(!validate(d,error))return false;
    std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(17)<<"VALWOOD_CUTSCENES 1\n";
    for(const auto &q:d.sequences){const auto &t=q.trigger;
        s<<"sequence "<<q.id<<' '<<std::quoted(q.name)<<' '<<std::quoted(q.timeline)<<' '<<q.enabled<<' '<<q.once<<' '<<q.priority<<' '<<q.letterbox<<' '<<q.skip_allowed<<' '
         <<unsigned(t.kind)<<' '<<unsigned(t.shape)<<' '<<t.target_id<<' '<<t.center.x<<' '<<t.center.y<<' '<<t.center.z<<' '<<t.extent.x<<' '<<t.extent.y<<' '<<t.extent.z<<' '<<t.cooldown_seconds<<'\n';
    }s<<"end\n";out=s.str();error.clear();return true;
}
inline bool parse(const std::string &text,Document &out,std::string &error){
    auto fail=[&](const char *s){error=s;return false;};
    if(text.empty()||text.size()>document_byte_limit||text.find('\0')!=std::string::npos)return fail("Cutscene library is empty or exceeds its budget.");
    std::istringstream f(text);f.imbue(std::locale::classic());std::string line;
    if(!std::getline(f,line)||line!="VALWOOD_CUTSCENES 1")return fail("Unsupported cutscene library version.");
    Document next;bool ended=false;
    while(std::getline(f,line)){
        if(line.empty())continue;
        if(ended)return fail("Unexpected data after cutscene library end.");
        if(line=="end"){ended=true;continue;}
        std::istringstream row(line);row.imbue(std::locale::classic());std::string key,id,target,extra;Sequence s;unsigned kind=0,shape=0;int enabled=0,once=0,letterbox=0,skip=0;
        if(!(row>>key>>id>>std::quoted(s.name)>>std::quoted(s.timeline)>>enabled>>once>>s.priority>>letterbox>>skip>>kind>>shape>>target>>s.trigger.center.x>>s.trigger.center.y>>s.trigger.center.z>>s.trigger.extent.x>>s.trigger.extent.y>>s.trigger.extent.z>>s.trigger.cooldown_seconds)||
           key!="sequence"||(row>>extra)||!detail::uint64(id,s.id)||!detail::uint64(target,s.trigger.target_id)||enabled<0||enabled>1||once<0||once>1||letterbox<0||letterbox>1||skip<0||skip>1||kind>2||shape>1)return fail("Malformed cutscene sequence record.");
        s.enabled=enabled;s.once=once;s.letterbox=letterbox;s.skip_allowed=skip;s.trigger.kind=TriggerKind(kind);s.trigger.shape=Shape(shape);next.sequences.push_back(std::move(s));
        if(next.sequences.size()>sequence_limit)return fail("Cutscene library exceeds its sequence budget.");
    }
    if(!ended)return fail("Cutscene library was truncated.");
    if(!validate(next,error))return false;out=std::move(next);error.clear();return true;
}
inline bool load(const char *path,Document &out,std::string &error){std::string text;return detail::read(path,document_byte_limit,text,error)&&parse(text,out,error);}
inline bool save(const char *path,const Document &d,std::string &error){std::string bytes;return encode(d,bytes,error)&&detail::atomic_write(path,bytes,error);}

inline bool prepare(const char *project_directory,const Document &d,PreparedLibrary &out,std::string &error){
    if(!validate(d,error))return false;
    if(!project_directory||!*project_directory){error="No cutscene project folder was selected.";return false;}
    std::error_code ec;const auto root=std::filesystem::canonical(std::filesystem::u8path(project_directory),ec);
    if(ec||!std::filesystem::is_directory(root,ec)||ec){error="Cutscene project folder is unavailable.";return false;}
    PreparedLibrary next;next.document=d;next.sequences.reserve(d.sequences.size());
    for(const auto &s:d.sequences){
        const auto path=std::filesystem::canonical(root/std::filesystem::u8path(s.timeline),ec);
        if(ec||!detail::contained(root,path)){error="Cutscene timeline is missing or escapes its project: "+s.timeline;return false;}
        PreparedSequence item;item.sequence=s;item.timeline_path=path.u8string();
        if(!world_cinematics::load(item.timeline_path.c_str(),item.timeline,error)){error=s.name+": "+error;return false;}
        if(item.timeline.loop||item.timeline.camera.empty()){error=s.name+": a world cutscene needs a non-looping camera timeline.";return false;}
        next.sequences.push_back(std::move(item));
    }out=std::move(next);error.clear();return true;
}
inline bool validate(const PreparedLibrary &p,std::string &error){
    if(!validate(p.document,error))return false;
    if(p.sequences.size()!=p.document.sequences.size()){error="Prepared cutscene library does not match its document.";return false;}
    for(std::size_t i=0;i<p.sequences.size();++i){const auto &s=p.sequences[i];
        if(!detail::same(s.sequence,p.document.sequences[i])||s.timeline_path.empty()){error="Prepared cutscene metadata is stale.";return false;}
        if(!world_cinematics::validate(s.timeline,error))return false;
        if(s.timeline.loop||s.timeline.camera.empty()){error="Prepared cutscene needs a non-looping camera timeline.";return false;}
    }error.clear();return true;
}
inline bool validate(const Progress &p,std::string &error){
    if(p.completed_ids.size()>progress_limit){error="Cutscene completion record exceeds its budget.";return false;}
    std::uint64_t previous=0;for(auto id:p.completed_ids){if(!id||id<=previous){error="Cutscene completion IDs must be nonzero, unique and sorted.";return false;}previous=id;}
    error.clear();return true;
}
inline std::string progress_payload(const Progress &p){
    std::ostringstream s;s.imbue(std::locale::classic());s<<"VALWOOD_CUTSCENE_PROGRESS 1\ncompleted "<<p.completed_ids.size();for(auto id:p.completed_ids)s<<' '<<id;s<<'\n';return s.str();
}
inline bool encode_progress(const Progress &p,std::string &out,std::string &error){
    if(!validate(p,error))return false;const auto body=progress_payload(p);std::ostringstream s;s.imbue(std::locale::classic());
    s<<body<<"signature "<<std::hex<<std::setw(16)<<std::setfill('0')<<detail::fingerprint(body)<<'\n';out=s.str();error.clear();return true;
}
inline bool parse_progress(const std::string &text,Progress &out,std::string &error){
    auto fail=[&](){error="Cutscene progress signature or format is invalid; previous progress was preserved.";return false;};
    if(text.empty()||text.size()>progress_byte_limit)return fail();
    std::istringstream s(text);s.imbue(std::locale::classic());std::string key,magic,token,extra;unsigned version=0;std::size_t count=0;std::uint64_t signature=0;Progress next;
    if(!(s>>magic>>version>>key>>count)||magic!="VALWOOD_CUTSCENE_PROGRESS"||version!=1||key!="completed"||count>progress_limit)return fail();
    next.completed_ids.reserve(count);for(std::size_t i=0;i<count;++i){std::uint64_t id=0;if(!(s>>token)||!detail::uint64(token,id))return fail();next.completed_ids.push_back(id);}
    if(!(s>>key>>std::hex>>signature)||key!="signature"||(s>>extra)||!validate(next,error)||signature!=detail::fingerprint(progress_payload(next)))return fail();
    std::string canonical;if(!encode_progress(next,canonical,error)||canonical!=text)return fail();out=std::move(next);error.clear();return true;
}
inline bool load_progress(const char *path,Progress &out,std::string &error){std::string text;return detail::read(path,progress_byte_limit,text,error)&&parse_progress(text,out,error);}
inline bool save_progress(const char *path,const Progress &p,std::string &error){std::string bytes;return encode_progress(p,bytes,error)&&detail::atomic_write(path,bytes,error);}

struct Input {
    double dt=0;Vec3 position={};bool paused=false,focused=true,confirm=false;
    // All relevant physical controls released, not merely confirm=false.
    bool neutral=true,allow_triggers=true;
};
struct Frame {
    bool active=false,started=false,completed=false,skipped=false,release_controls=false,letterbox=false;
    std::uint64_t id=0;double time=0,duration=0,skip_progress=0;
    world_cinematics::CameraPose camera;const char *name=nullptr;
};
class Runtime {
public:
    // Invalid edits/reloads preserve the previous live library AND active take.
    // A valid reload interrupts without completing; an unseen once-take replays.
    bool configure(const PreparedLibrary &p,std::string &error){
        if(!validate(p,error)||!capacity_for(p,progress_,error))return false;PreparedLibrary next=p;
        library_=std::move(next);progress_.completed_ids.reserve(progress_limit);reset(true);error.clear();return true;
    }
    const PreparedSequence *find(std::uint64_t id)const {for(const auto &s:library_.sequences)if(s.sequence.id==id)return &s;return nullptr;}
    const PreparedSequence *active_sequence()const {return active_index_<library_.sequences.size()?&library_.sequences[active_index_]:nullptr;}
    const PreparedLibrary &library()const{return library_;}
    const Progress &progress()const{return progress_;}
    bool set_progress(const Progress &p,std::string &error){if(!validate(p,error)||!capacity_for(library_,p,error))return false;Progress next=p;next.completed_ids.reserve(progress_limit);progress_=std::move(next);error.clear();return true;}
    bool completed(std::uint64_t id)const{return std::binary_search(progress_.completed_ids.begin(),progress_.completed_ids.end(),id);}
    void reset(bool keep_progress=true){
        active_index_=sequence_limit;time_=clock_=skip_time_=0;inside_.fill(false);pending_.fill(false);next_allowed_.fill(0);
        skip_armed_=suspended_=awaiting_release_=false;
        if(!keep_progress)progress_.completed_ids.clear();
    }
    // Requests are considered together on the next allowed tick. No preemption;
    // highest priority wins, then lowest stable ID, regardless of authoring order.
    bool manual_start(std::uint64_t id){
        if(active_sequence()||awaiting_release_)return false;
        for(std::size_t i=0;i<library_.sequences.size();++i)if(library_.sequences[i].sequence.id==id&&eligible(i)&&library_.sequences[i].sequence.trigger.kind==TriggerKind::Manual){pending_[i]=true;return true;}return false;
    }
    bool interact(std::uint64_t target_id){
        if(!target_id||active_sequence()||awaiting_release_)return false;bool queued=false;
        for(std::size_t i=0;i<library_.sequences.size();++i){const auto &s=library_.sequences[i].sequence;
            if(s.trigger.kind==TriggerKind::Interact&&s.trigger.target_id==target_id&&eligible(i)){pending_[i]=true;queued=true;}}
        return queued;
    }
    // Returns false without modifying either runtime or output on invalid dt or
    // position. Large finite dt is bounded: a hitch cannot skip an entire take.
    bool tick(const Input &in,Frame &out){
        if(!std::isfinite(in.dt)||in.dt<0||!detail::point(in.position))return false;
        Frame f;const bool stopped=in.paused||!in.focused;
        if(stopped){if(active_sequence()){suspended_=true;sample(f);}out=f;return true;}
        const double dt=std::min(in.dt,maximum_step_seconds);clock_+=dt;
        if(suspended_){skip_armed_=false;skip_time_=0;suspended_=false;}
        if(awaiting_release_&&in.neutral&&!in.confirm)awaiting_release_=false;
        std::array<bool,sequence_limit> entered={};
        if(in.allow_triggers){for(std::size_t i=0;i<library_.sequences.size();++i){const auto &s=library_.sequences[i].sequence;
            if(s.trigger.kind==TriggerKind::Area){const bool now=detail::inside(s.trigger,in.position);entered[i]=now&&!inside_[i];inside_[i]=now;}}}
        if(!active_sequence()){
            std::size_t winner=sequence_limit;
            if(in.allow_triggers&&!awaiting_release_){for(std::size_t i=0;i<library_.sequences.size();++i)if((pending_[i]||entered[i])&&eligible(i)){
                const auto &s=library_.sequences[i].sequence;
                if(winner==sequence_limit||s.priority>library_.sequences[winner].sequence.priority||(s.priority==library_.sequences[winner].sequence.priority&&s.id<library_.sequences[winner].sequence.id))winner=i;
            }}
            // Explicit requests made while a higher-priority modal owns the host
            // remain queued. Once evaluated, losing triggers are not replayed.
            if(in.allow_triggers)pending_.fill(false);
            if(winner!=sequence_limit){active_index_=winner;time_=skip_time_=0;skip_armed_=false;sample(f);f.started=f.release_controls=true;out=f;return true;}
            out=f;return true;
        }
        if(!skip_armed_){if(in.neutral&&!in.confirm)skip_armed_=true;skip_time_=0;}
        else if(in.confirm&&active_sequence()->sequence.skip_allowed)skip_time_+=dt;else skip_time_=0;
        time_=std::min(time_+dt,active_sequence()->timeline.duration);
        if(time_+1e-9>=active_sequence()->timeline.duration)time_=active_sequence()->timeline.duration;
        sample(f);
        const bool skip=skip_armed_&&active_sequence()->sequence.skip_allowed&&skip_time_+1e-9>=skip_hold_seconds;
        if(skip||time_+1e-9>=active_sequence()->timeline.duration){
            const auto index=active_index_;const auto id=library_.sequences[index].sequence.id;
            if(!completed(id)&&progress_.completed_ids.size()<progress_limit)progress_.completed_ids.insert(std::lower_bound(progress_.completed_ids.begin(),progress_.completed_ids.end(),id),id);
            next_allowed_[index]=clock_+library_.sequences[index].sequence.trigger.cooldown_seconds;
            f.active=false;f.completed=true;f.skipped=skip;f.release_controls=true;
            active_index_=sequence_limit;time_=skip_time_=0;skip_armed_=false;awaiting_release_=true;
        }out=f;return true;
    }
private:
    static bool capacity_for(const PreparedLibrary &library,const Progress &p,std::string &error){
        std::size_t total=p.completed_ids.size();for(const auto &s:library.sequences)if(!std::binary_search(p.completed_ids.begin(),p.completed_ids.end(),s.sequence.id))++total;
        if(total>progress_limit){error="Cutscene completion history has no room for this library; existing progress was preserved.";return false;}return true;
    }
    bool eligible(std::size_t i)const {const auto &s=library_.sequences[i].sequence;return s.enabled&&(!s.once||!completed(s.id))&&clock_+1e-9>=next_allowed_[i];}
    void sample(Frame &f)const {
        const auto *p=active_sequence();if(!p)return;f.active=true;f.id=p->sequence.id;f.name=p->sequence.name.c_str();f.time=time_;f.duration=p->timeline.duration;f.letterbox=p->sequence.letterbox;
        f.skip_progress=p->sequence.skip_allowed?std::min(1.,skip_time_/skip_hold_seconds):0;world_cinematics::sample_camera(p->timeline,time_,f.camera);
    }
    PreparedLibrary library_;Progress progress_;std::array<bool,sequence_limit> inside_={},pending_={};
    std::array<double,sequence_limit> next_allowed_={};std::size_t active_index_=sequence_limit;
    double time_=0,clock_=0,skip_time_=0;bool skip_armed_=false,suspended_=false,awaiting_release_=false;
};
}
#endif

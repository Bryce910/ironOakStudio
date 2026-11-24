#ifndef VALWOOD_WORLD_PLAYTEST_SLICE_H
#define VALWOOD_WORLD_PLAYTEST_SLICE_H
// Bounded wayfinding only: never moves a player, performs IO during update,
// changes gameplay state, or claims that the route is collision-free.
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <istream>
#include <locale>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace world_playtest_slice {
constexpr std::size_t node_limit=256, byte_limit=65536, text_limit=96;
constexpr double height_tolerance=2.;
struct Spawn {double x=0,eye_y=1.75,z=0,yaw=0,pitch=0;};
struct Node {std::string label;double x=0,ground_y=0,z=0,radius=2.;bool visible=false;};
struct Path {std::string name;Spawn spawn;std::vector<Node> nodes;};
namespace detail {
inline bool fail(std::string &error,const char *message){error=message;return false;}
inline bool text(const std::string &s,bool required){
    if(s.size()>text_limit||(required&&s.empty()))return false;
    for(unsigned char c:s)if(c<32||c==127)return false;
    return true;
}
inline bool position(double x,double y,double z){
    return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z)&&
           std::abs(x)<=1000000.&&std::abs(z)<=1000000.&&std::abs(y)<=100000.;
}
inline bool end(std::istringstream &row){row>>std::ws;return row.eof();}
inline bool quoted(std::istringstream &row,std::string &out){
    row>>std::ws;return row.peek()=='"'&&bool(row>>std::quoted(out));
}
inline bool decimal(const std::string &token,std::uint64_t &out){
    if(token.empty())return false;
    std::uint64_t value=0;
    for(unsigned char c:token){
        if(c<'0'||c>'9'||value>(std::numeric_limits<std::uint64_t>::max()-(c-'0'))/10)return false;
        value=value*10+(c-'0');
    }
    out=value;return true;
}
}
inline bool validate(const Path &path,std::string &error){
    if(!detail::text(path.name,true))return detail::fail(error,"Invalid playtest name");
    const auto &s=path.spawn;
    if(!detail::position(s.x,s.eye_y,s.z)||!std::isfinite(s.yaw)||std::abs(s.yaw)>100000.||
       !std::isfinite(s.pitch)||std::abs(s.pitch)>1.5707963267948966)
        return detail::fail(error,"Invalid playtest spawn");
    if(path.nodes.empty()||path.nodes.size()>node_limit)return detail::fail(error,"Invalid playtest node count");
    for(const auto &p:path.nodes)
        if(!detail::text(p.label,p.visible)||!detail::position(p.x,p.ground_y,p.z)||
           !std::isfinite(p.radius)||p.radius<.25||p.radius>100.)
            return detail::fail(error,"Invalid playtest path node");
    error.clear();return true;
}
// Grammar (one record per line, optional blank lines, no comments):
// VALWOOD_PLAYTEST 1
// name "Route name"
// spawn x eyeY z yawRadians pitchRadians
// count N
// path "Label" x groundY z radius [checkpoint]
// The optional checkpoint word makes a node visible in the player's progress UI.
inline bool parse(std::istream &input,Path &out,std::string &error){
    Path staged;std::size_t bytes=0,records=0,count=0;std::string line;
    // Read bounded characters rather than allowing getline to allocate an
    // arbitrary malicious line before checking its length.
    auto next=[&]()->int{
        line.clear();char c;
        while(input.get(c)){
            if(++bytes>byte_limit)return -1;
            if(c=='\n')break;
            if(c=='\0'||line.size()>=1024)return -1;
            line+=c;
        }
        if(input.bad()||(input.fail()&&!input.eof()))return -1;
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        return line.empty()&&input.eof()?0:1;
    };
    for(;;){
        const int status=next();if(status<0)return detail::fail(error,"Playtest input exceeds bounds or is unreadable");
        if(!status)break;
        std::istringstream row(line);row.imbue(std::locale::classic());row>>std::ws;
        if(row.eof())continue;
        std::string tag;if(!(row>>tag))return detail::fail(error,"Malformed playtest record");
        if(records==0){
            unsigned version=0;if(tag!="VALWOOD_PLAYTEST"||!(row>>version)||version!=1||!detail::end(row))
                return detail::fail(error,"Invalid playtest header");
        }else if(records==1){
            if(tag!="name"||!detail::quoted(row,staged.name)||!detail::end(row))return detail::fail(error,"Invalid playtest name record");
        }else if(records==2){
            auto &s=staged.spawn;
            if(tag!="spawn"||!(row>>s.x>>s.eye_y>>s.z>>s.yaw>>s.pitch)||!detail::end(row))return detail::fail(error,"Invalid playtest spawn record");
        }else if(records==3){
            if(tag!="count"||!(row>>count)||!detail::end(row)||count<1||count>node_limit)return detail::fail(error,"Invalid playtest count record");
            staged.nodes.reserve(count);
        }else{
            if(staged.nodes.size()>=count)return detail::fail(error,"Extra playtest records");
            Node node;std::string flag;
            if(tag!="path"||!detail::quoted(row,node.label)||!(row>>node.x>>node.ground_y>>node.z>>node.radius))
                return detail::fail(error,"Invalid playtest path record");
            if(row>>flag){if(flag!="checkpoint"||!detail::end(row))return detail::fail(error,"Invalid playtest path flag");node.visible=true;}
            else if(!row.eof())return detail::fail(error,"Invalid playtest path tail");
            staged.nodes.push_back(std::move(node));
        }
        ++records;
    }
    if(records<4||staged.nodes.size()!=count)return detail::fail(error,"Incomplete playtest path");
    if(!validate(staged,error))return false;
    out=std::move(staged);error.clear();return true;
}
inline bool load(const char *filename,Path &out,std::string &error){
    if(!filename||!*filename)return detail::fail(error,"Missing playtest filename");
    std::ifstream input(filename,std::ios::binary);
    if(!input)return detail::fail(error,"Cannot open playtest route");
    return parse(input,out,error);
}
inline bool encode(const Path &path,std::string &out,std::string &error){
    if(!validate(path,error))return false;
    std::ostringstream stream;stream.imbue(std::locale::classic());stream<<std::setprecision(17);
    const auto &s=path.spawn;
    stream<<"VALWOOD_PLAYTEST 1\nname "<<std::quoted(path.name)<<"\nspawn "<<s.x<<' '<<s.eye_y<<' '<<s.z<<' '<<s.yaw<<' '<<s.pitch<<"\ncount "<<path.nodes.size()<<'\n';
    for(const auto &p:path.nodes)stream<<"path "<<std::quoted(p.label)<<' '<<p.x<<' '<<p.ground_y<<' '<<p.z<<' '<<p.radius<<(p.visible?" checkpoint":"")<<'\n';
    std::string staged=stream.str();
    if(staged.size()>byte_limit)return detail::fail(error,"Encoded playtest exceeds bounds");
    out=std::move(staged);error.clear();return true;
}
// Stable route identity, not a security/authentication digest. The canonical
// encoded coordinates/labels/flags all participate; no filesystem paths do.
inline std::uint64_t fingerprint(const Path &path){
    std::string bytes,error;if(!encode(path,bytes,error))return 0;
    std::uint64_t hash=14695981039346656037ull;
    for(unsigned char c:bytes){hash^=c;hash*=1099511628211ull;}
    return hash?hash:1;
}
struct Session {
    unsigned section=0;std::uint64_t fingerprint=0;std::size_t index=0;
    double x=0,eye_y=1.75,z=0,yaw=0,pitch=0;bool third_person=false;
};
inline bool validate_session(const Session &s,std::string &error){
    if(s.section>1||!s.fingerprint||s.index>node_limit||!detail::position(s.x,s.eye_y,s.z)||
       !std::isfinite(s.yaw)||std::abs(s.yaw)>100000.||!std::isfinite(s.pitch)||std::abs(s.pitch)>1.5707963267948966)
        return detail::fail(error,"Invalid playtest session");
    error.clear();return true;
}
inline bool parse_session(std::istream &input,Session &out,std::string &error){
    std::string bytes;char c;
    while(input.get(c)){
        if(c=='\0'||bytes.size()>=2048)return detail::fail(error,"Playtest session exceeds bounds");
        bytes+=c;
    }
    if(input.bad()||(input.fail()&&!input.eof()))return detail::fail(error,"Unreadable playtest session");
    std::istringstream row(bytes);row.imbue(std::locale::classic());
    Session staged;std::string tag,section_token,fingerprint_token,index_token,third_token;unsigned version=0;
    if(!(row>>tag>>version)||tag!="VALWOOD_PLAYTEST_SESSION"||version!=1||
       !(row>>tag>>section_token)||tag!="section"||
       !(row>>tag>>fingerprint_token)||tag!="fingerprint"||
       !(row>>tag>>index_token)||tag!="index"||
       !(row>>tag>>staged.x>>staged.eye_y>>staged.z>>staged.yaw>>staged.pitch>>third_token)||tag!="player"||!detail::end(row))
        return detail::fail(error,"Malformed playtest session");
    std::uint64_t section=0,index=0,third=0;
    if(!detail::decimal(section_token,section)||section>1||!detail::decimal(fingerprint_token,staged.fingerprint)||
       !detail::decimal(index_token,index)||index>node_limit||!detail::decimal(third_token,third)||third>1)
        return detail::fail(error,"Invalid playtest session integer");
    staged.section=unsigned(section);staged.index=std::size_t(index);
    staged.third_person=third!=0;
    if(!validate_session(staged,error))return false;
    out=staged;error.clear();return true;
}
inline bool encode_session(const Session &s,std::string &out,std::string &error){
    if(!validate_session(s,error))return false;
    std::ostringstream stream;stream.imbue(std::locale::classic());stream<<std::setprecision(17);
    stream<<"VALWOOD_PLAYTEST_SESSION 1\nsection "<<s.section<<"\nfingerprint "<<s.fingerprint<<"\nindex "<<s.index
          <<"\nplayer "<<s.x<<' '<<s.eye_y<<' '<<s.z<<' '<<s.yaw<<' '<<s.pitch<<' '<<(s.third_person?1:0)<<'\n';
    out=stream.str();error.clear();return true;
}
class State {
    Path path_;std::size_t index_=0,reached_=0,total_=0;bool started_=false;
public:
    bool start(const Path &path,std::string *error=nullptr){
        std::string message;if(!validate(path,message)){if(error)*error=message;return false;}
        Path copy=path;path_=std::move(copy);started_=true;total_=0;
        for(const auto &p:path_.nodes)if(p.visible)++total_;
        reset();if(error)error->clear();return true;
    }
    void reset(){index_=reached_=0;}
    bool restore_index(std::size_t index,std::string &error){
        if(!started_||index>path_.nodes.size())return detail::fail(error,"Invalid playtest progress index");
        std::size_t reached=0;for(std::size_t i=0;i<index;++i)if(path_.nodes[i].visible)++reached;
        index_=index;reached_=reached;error.clear();return true;
    }
    bool active()const{return started_;}
    bool complete()const{return started_&&index_==path_.nodes.size();}
    std::size_t index()const{return index_;}
    std::size_t reached()const{return reached_;}
    std::size_t total()const{return total_;}
    const Path &path()const{return path_;}
    const Spawn &spawn()const{return path_.spawn;}
    const Node *next()const{return started_&&index_<path_.nodes.size()?&path_.nodes[index_]:nullptr;}
    const char *getlabel()const{const auto *p=next();return p?p->label.c_str():"";}
    double distance(double x,double z)const{
        const auto *p=next();return p&&std::isfinite(x)&&std::isfinite(z)?std::hypot(x-p->x,z-p->z):0.;
    }
    bool update(double x,double feet_y,double z){
        if(!detail::position(x,feet_y,z))return false;
        const auto before=index_;
        while(const auto *p=next()){
            if(std::hypot(x-p->x,z-p->z)>p->radius||std::abs(feet_y-p->ground_y)>height_tolerance)break;
            if(p->visible)++reached_;
            ++index_;
        }
        return index_!=before;
    }
};
// Caller selects the section's Path before this check and commits player
// camera/settings only after it succeeds. State itself never reads/writes files.
inline bool restore_session(const Session &session,State &state,std::string &error){
    if(!validate_session(session,error))return false;
    if(!state.active()||session.fingerprint!=fingerprint(state.path()))return detail::fail(error,"Saved playtest route has changed");
    return state.restore_index(session.index,error);
}
}
#endif

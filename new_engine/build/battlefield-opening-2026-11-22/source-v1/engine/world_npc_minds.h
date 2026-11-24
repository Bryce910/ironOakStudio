#ifndef VALWOOD_WORLD_NPC_MINDS_H
#define VALWOOD_WORLD_NPC_MINDS_H
#include "engine/world_npc_memory.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

// Explicit scene bindings, authored recall and bounded witnessed experience.
// Configure/codec allocate at loading boundaries. Observe/recall/duty do not.
namespace world_npc_minds {
using Id=std::uint64_t;
constexpr unsigned mind_limit=32,experience_limit=256;
enum class Event : unsigned {Meeting,Choice,TrainingWon,TrainingLost,Report};
enum class Duty : unsigned {Schedule,Guard,Work,Rest};
struct Binding {Id entity=0;world_npc_memory::Document memory;std::vector<std::string> context;};
struct Experience {Id entity=0,key=0;Event kind=Event::Meeting;unsigned count=0;double first_hour=0,last_hour=0;};
struct State {std::uint64_t signature=0,sequence=0;unsigned count=0;std::array<Experience,experience_limit> values={};};
struct Recollection {const char *name=nullptr,*knowledge=nullptr,*recognition=nullptr;unsigned visits=0;};
inline std::uint64_t hash(std::string_view bytes){std::uint64_t h=14695981039346656037ull;for(unsigned char c:bytes)h=(h^c)*1099511628211ull;return h;}
class Minds {
    struct Mind {Id entity=0;world_npc_memory::Bank bank;std::vector<std::string> context;};
    std::vector<Mind> minds_;State state_;
    const Mind *find(Id id)const{for(const auto &m:minds_)if(m.entity==id)return &m;return nullptr;}
public:
    bool configure(const std::vector<Binding> &bindings,std::string &error){
        if(bindings.size()>mind_limit){error="At most 32 NPC minds are supported";return false;}
        std::vector<Mind> next;std::ostringstream signature;
        for(const auto &b:bindings){
            if(!b.entity||b.context.size()>world_npc_memory::query_tag_limit||std::any_of(next.begin(),next.end(),[&](const auto &m){return m.entity==b.entity;})){error="Invalid or duplicate NPC mind binding";return false;}
            Mind m;m.entity=b.entity;m.context=b.context;if(!m.bank.configure(b.memory,error))return false;
            world_npc_memory::Query q;for(const auto &tag:m.context)q.tags[q.count++]=tag;
            if(!m.bank.recall(q).valid){error="Invalid NPC conversation context tags";return false;}
            std::string encoded;if(!m.bank.encode(encoded,error))return false;
            signature<<b.entity<<' '<<std::quoted(encoded)<<' '<<m.context.size();for(const auto &s:m.context)signature<<' '<<std::quoted(s);
            next.push_back(std::move(m));
        }
        State initial;initial.signature=hash(signature.str());minds_=std::move(next);state_=initial;error.clear();return true;
    }
    bool bound(Id entity)const{return find(entity)!=nullptr;}
    bool observe(Id entity,Event kind,Id key,double hour){
        if(!find(entity)||unsigned(kind)>unsigned(Event::Report)||!std::isfinite(hour)||hour<0||hour>1e9||state_.sequence==UINT64_MAX)return false;
        for(unsigned i=0;i<state_.count;i++){auto &e=state_.values[i];if(e.entity==entity&&e.kind==kind&&e.key==key){if(hour<e.last_hour)return false;e.count=std::min(1000000u,e.count+1);e.last_hour=hour;state_.sequence++;return true;}}
        unsigned at=state_.count;if(at<experience_limit)state_.count++;
        else {at=0;for(unsigned i=1;i<experience_limit;i++)if(state_.values[i].last_hour<state_.values[at].last_hour)at=i;}
        state_.values[at]={entity,key,kind,1,hour,hour};state_.sequence++;return true;
    }
    unsigned count(Id entity,Event kind)const{unsigned n=0;for(unsigned i=0;i<state_.count;i++){const auto &e=state_.values[i];if(e.entity==entity&&e.kind==kind)n=std::min(1000000u,n+e.count);}return n;}
    Recollection recall(Id entity,double hour)const{
        Recollection out;const auto *m=find(entity);if(!m)return out;out.name=m->bank.document().profile.name.c_str();
        world_npc_memory::Query q;q.game_hour=hour;q.shareable_only=true;for(const auto &tag:m->context)q.tags[q.count++]=tag;
        const auto r=m->bank.recall(q);if(r.valid&&r.count)out.knowledge=m->bank.document().memories[r.hits[0].index].body.c_str();
        out.visits=count(entity,Event::Meeting);
        if(count(entity,Event::TrainingWon))out.recognition="I remember your sparring victory. You earned the watch's respect.";
        else if(count(entity,Event::Report))out.recognition="You brought word back from the trail. The camp remembers that help.";
        else if(count(entity,Event::TrainingLost))out.recognition="I remember your last round. Read the guard and take your time; you can try again.";
        else if(out.visits>1)out.recognition="I remember you. It is good to see you again.";
        return out;
    }
    Duty duty(Id entity,double hour)const{
        const auto *m=find(entity);if(!m)return Duty::Schedule;
        world_npc_memory::Query q;q.tags[0]="duty";q.count=1;q.game_hour=hour;const auto r=m->bank.recall(q);
        for(unsigned i=0;i<r.count;i++){const auto &hit=r.hits[i];const auto &memory=m->bank.document().memories[hit.index];if(memory.kind!=world_npc_memory::Kind::Goal||hit.score<.3)continue;
            for(const auto &tag:memory.tags){if(tag=="guard")return Duty::Guard;if(tag=="work")return Duty::Work;if(tag=="rest")return Duty::Rest;}}
        return Duty::Schedule;
    }
    const State &state()const{return state_;}
    bool restore(const State &state,std::string &error){
        if(state.signature!=state_.signature||state.count>experience_limit){error="NPC experience belongs to another memory configuration";return false;}
        std::uint64_t observations=0;
        for(unsigned i=0;i<state.count;i++){const auto &e=state.values[i];if(!find(e.entity)||unsigned(e.kind)>unsigned(Event::Report)||!e.count||e.count>1000000||!std::isfinite(e.first_hour)||!std::isfinite(e.last_hour)||e.first_hour<0||e.last_hour<e.first_hour||e.last_hour>1e9){error="Invalid witnessed NPC experience";return false;}observations+=e.count;for(unsigned j=0;j<i;j++){const auto &v=state.values[j];if(v.entity==e.entity&&v.kind==e.kind&&v.key==e.key){error="Duplicate witnessed NPC experience";return false;}}}
        if(observations>state.sequence){error="Invalid NPC observation sequence";return false;}state_=state;error.clear();return true;
    }
    bool encode(std::string &out,std::string &error)const{
        std::ostringstream o;o<<std::setprecision(17)<<"VALWOOD_NPC_EXPERIENCE 1 "<<state_.signature<<' '<<state_.sequence<<' '<<state_.count<<'\n';
        for(unsigned i=0;i<state_.count;i++){const auto &e=state_.values[i];o<<"SEEN "<<e.entity<<' '<<unsigned(e.kind)<<' '<<e.key<<' '<<e.count<<' '<<e.first_hour<<' '<<e.last_hour<<'\n';}o<<"END\n";out=o.str();error.clear();return true;
    }
    bool decode(const std::string &bytes,std::string &error){
        if(bytes.size()>65536||bytes.find('\0')!=std::string::npos){error="Invalid NPC experience bytes";return false;}
        std::istringstream in(bytes);std::string tag;unsigned version=0;State s;
        if(!(in>>tag>>version>>s.signature>>s.sequence>>s.count)||tag!="VALWOOD_NPC_EXPERIENCE"||version!=1||s.count>experience_limit){error="Invalid NPC experience header";return false;}
        for(unsigned i=0;i<s.count;i++){auto &e=s.values[i];unsigned kind=0;if(!(in>>tag>>e.entity>>kind>>e.key>>e.count>>e.first_hour>>e.last_hour)||tag!="SEEN"||kind>unsigned(Event::Report)){error="Invalid NPC experience record";return false;}e.kind=Event(kind);}
        if(!(in>>tag)||tag!="END"){error="Missing NPC experience end";return false;}in>>std::ws;if(!in.eof()){error="Trailing NPC experience data";return false;}return restore(s,error);
    }
};
}
#endif

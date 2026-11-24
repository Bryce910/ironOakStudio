#include "engine/world_npc_memory.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>

namespace world_npc_memory {
namespace {
bool identifier(std::string_view s,unsigned limit=48,bool tag=false){
    if(s.empty()||s.size()>limit)return false;
    auto lower=[](char c){return c>='a'&&c<='z';};auto digit=[](char c){return c>='0'&&c<='9';};
    if(!lower(s[0])&&!(tag&&digit(s[0])))return false;
    for(char c:s)if(!lower(c)&&!digit(c)&&c!='_'&&c!='-')return false;
    return true;
}
bool text(const std::string &s,unsigned limit,bool required=false){
    if(s.size()>limit||(required&&s.find_first_not_of(" \r\n\t")==std::string::npos))return false;
    for(std::size_t i=0;i<s.size();){
        const unsigned c=static_cast<unsigned char>(s[i++]);
        if(c<128){if((c<32&&c!=9&&c!=10&&c!=13)||c==127)return false;continue;}
        unsigned value=0,n=0,minimum=0;
        if(c>=0xc2&&c<=0xdf){value=c&31;n=1;minimum=0x80;}
        else if(c>=0xe0&&c<=0xef){value=c&15;n=2;minimum=0x800;}
        else if(c>=0xf0&&c<=0xf4){value=c&7;n=3;minimum=0x10000;}
        else return false;
        if(i+n>s.size())return false;
        for(unsigned j=0;j<n;j++){const unsigned b=static_cast<unsigned char>(s[i++]);if((b&0xc0)!=0x80)return false;value=(value<<6)|(b&63);}
        if(value<minimum||value>0x10ffff||(value>=0xd800&&value<=0xdfff))return false;
    }return true;
}
bool range(double v,double a,double b){return std::isfinite(v)&&v>=a&&v<=b;}
int index(const Document &d,const std::string &id){for(unsigned i=0;i<d.memories.size();i++)if(d.memories[i].id==id)return int(i);return -1;}
bool marker(std::istream &in,const char *value){std::string s;return bool(in>>s)&&s==value;}
}
const char *kind_name(Kind k){static const char *names[]={"Event","Relationship","Belief","Goal","Knowledge","Skill"};return unsigned(k)<6?names[unsigned(k)]:"Invalid";}
const char *source_name(Source k){static const char *names[]={"Authored","Witnessed","Hearsay"};return unsigned(k)<3?names[unsigned(k)]:"Invalid";}
bool valid(const Document &d,std::string &error){
    error.clear();auto fail=[&](const char *s){error=s;return false;};const auto &p=d.profile;
    if(!identifier(p.npc_id)||p.actor_id>1000000000||!text(p.name,64,true)||!text(p.role,64)||!text(p.faction,64)||!text(p.summary,2048)||p.traits.size()>8)return fail("Invalid memory profile, actor ID or UTF-8 text budget");
    for(const auto &t:p.traits)if(!text(t,48,true))return fail("Invalid personality trait");
    const auto &a=p.appearance;
    if((a.age_years&&!range(*a.age_years,0,10000))||(a.height_cm&&!range(*a.height_cm,1,1000))||(a.weight_kg&&!range(*a.weight_kg,.1,10000)))return fail("Invalid physical attribute measurements");
    for(const auto *s:{&a.build,&a.skin_tone,&a.eye_color,&a.hair_color,&a.hair_style,&a.hair_texture,&a.hair_thickness,&a.hair_density,&a.facial_hair})if(!text(*s,64))return fail("Invalid physical attribute text");
    if(!text(a.distinguishing_features,512))return fail("Invalid distinguishing features");
    if(a.hair_thickness!=""&&a.hair_thickness!="fine"&&a.hair_thickness!="medium"&&a.hair_thickness!="coarse")return fail("Invalid hair strand thickness");
    if(a.hair_density!=""&&a.hair_density!="bald"&&a.hair_density!="sparse"&&a.hair_density!="medium"&&a.hair_density!="dense")return fail("Invalid hair density");
    if(d.memories.size()>memory_limit)return fail("Memory bank exceeds256 nodes");
    for(unsigned i=0;i<d.memories.size();i++){
        const auto &m=d.memories[i];
        if(!identifier(m.id)||m.id=="core"||!identifier(m.parent_id)||!text(m.title,96,true)||!text(m.body,2048)||unsigned(m.kind)>5||unsigned(m.source)>2)return fail("Invalid memory ID, category, source or text");
        for(unsigned j=0;j<i;j++)if(m.id==d.memories[j].id)return fail("Duplicate memory ID");
        if(!range(m.importance,0,1)||!range(m.confidence,0,1)||!range(m.emotion,-1,1)||!range(m.occurred_hour,0,1e9)||!range(m.half_life_hours,0,1e9))return fail("Invalid importance/confidence/emotion or game-hour bounds");
        if(m.tags.size()>tag_limit)return fail("Too many memory trigger tags");
        for(unsigned j=0;j<m.tags.size();j++){if(!identifier(m.tags[j],32,true))return fail("Invalid lowercase trigger tag");for(unsigned k=0;k<j;k++)if(m.tags[j]==m.tags[k])return fail("Duplicate trigger tag");}
    }
    for(unsigned i=0;i<d.memories.size();i++){
        int at=int(i);std::array<bool,memory_limit> visited={};unsigned depth=0;
        while(at>=0){if(visited[unsigned(at)]||++depth>depth_limit)return fail("Cyclic memory graph or branch depth exceeds16");visited[unsigned(at)]=true;const auto &m=d.memories[unsigned(at)];if(m.parent_id=="core")break;at=index(d,m.parent_id);if(at<0)return fail("Missing memory parent");}
    }return true;
}
bool Bank::configure(const Document &d,std::string &error){
    if(!valid(d,error))return false;
    Document next=d;std::array<int,memory_limit> parents={};parents.fill(-1);
    for(unsigned i=0;i<next.memories.size();i++)if(next.memories[i].parent_id!="core")parents[i]=index(next,next.memories[i].parent_id);
    document_=std::move(next);parents_=parents;ready_=true;return true;
}
bool Bank::load(const char *path,std::string &error){
    error.clear();auto fail=[&](const char *s){error=s;return false;};
    if(!path||!*path)return fail("Empty memory file path");
    std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)return fail("Cannot open NPC memory file");
    const auto length=file.tellg();if(length<0||length>2*1024*1024)return fail("Memory file exceeds2MiB");
    std::string bytes(std::size_t(length),'\0');file.seekg(0);if(!bytes.empty()&&!file.read(bytes.data(),std::streamsize(bytes.size())))return fail("Cannot read NPC memory file");
    return decode(bytes,error);
}
bool Bank::decode(std::string_view bytes,std::string &error){
    error.clear();auto fail=[&](const char *s){error=s;return false;};
    if(bytes.size()>2*1024*1024||bytes.find('\0')!=std::string_view::npos)return fail("Invalid or oversized memory bytes");
    std::istringstream in{std::string(bytes)};unsigned version=0,n=0;Document next;auto &p=next.profile;
    if(!marker(in,"VALWOOD_NPC_MEMORY")||!(in>>version)||(version!=1&&version!=2)||!marker(in,"PROFILE")||!(in>>std::quoted(p.npc_id)>>p.actor_id>>std::quoted(p.name)>>std::quoted(p.role)>>std::quoted(p.faction)>>std::quoted(p.summary)))return fail("Invalid NPC memory header/profile");
    if(!marker(in,"TRAITS")||!(in>>n)||n>8)return fail("Invalid memory traits");
    p.traits.resize(n);for(auto &s:p.traits)if(!(in>>std::quoted(s)))return fail("Truncated personality trait");
    if(version==2){
        auto &a=p.appearance;double age=-1,height=-1,weight=-1;
        if(!marker(in,"APPEARANCE")||!(in>>age>>height>>weight>>std::quoted(a.build)>>std::quoted(a.skin_tone)>>std::quoted(a.eye_color)>>std::quoted(a.hair_color)>>std::quoted(a.hair_style)>>std::quoted(a.hair_texture)>>std::quoted(a.hair_thickness)>>std::quoted(a.hair_density)>>std::quoted(a.facial_hair)>>std::quoted(a.distinguishing_features)))return fail("Invalid or truncated physical attributes");
        if(age!=-1)a.age_years=age;
        if(height!=-1)a.height_cm=height;
        if(weight!=-1)a.weight_kg=weight;
    }
    if(!marker(in,"MEMORIES")||!(in>>n)||n>memory_limit)return fail("Invalid memory node count");
    next.memories.resize(n);
    for(auto &m:next.memories){
        unsigned kind=0,source=0,share=0,enabled=0,count=0;
        if(!marker(in,"MEMORY")||!(in>>std::quoted(m.id)>>std::quoted(m.parent_id)>>kind>>std::quoted(m.title)>>std::quoted(m.body)>>m.importance>>m.confidence>>m.emotion>>source>>share>>m.occurred_hour>>m.half_life_hours>>enabled>>count)||kind>5||source>2||share>1||enabled>1||count>tag_limit)return fail("Invalid or truncated memory node");
        m.kind=Kind(kind);m.source=Source(source);m.shareable=share!=0;m.enabled=enabled!=0;m.tags.resize(count);
        for(auto &s:m.tags)if(!(in>>std::quoted(s)))return fail("Truncated memory trigger");
    }
    if(!marker(in,"END"))return fail("Missing memory end marker");
    in>>std::ws;if(!in.eof())return fail("Unexpected trailing memory data");
    return configure(next,error);
}
bool Bank::encode(std::string &out,std::string &error) const{
    if(!ready_||!valid(document_,error)){if(error.empty())error="Memory bank is not configured";return false;}
    const auto &p=document_.profile;const auto &a=p.appearance;std::ostringstream o;o<<std::setprecision(17);
    o<<"VALWOOD_NPC_MEMORY 2\nPROFILE "<<std::quoted(p.npc_id)<<' '<<p.actor_id<<' '<<std::quoted(p.name)<<' '<<std::quoted(p.role)<<' '<<std::quoted(p.faction)<<' '<<std::quoted(p.summary)<<"\nTRAITS "<<p.traits.size();for(const auto &s:p.traits)o<<' '<<std::quoted(s);
    o<<"\nAPPEARANCE "<<a.age_years.value_or(-1)<<' '<<a.height_cm.value_or(-1)<<' '<<a.weight_kg.value_or(-1);
    for(const auto *s:{&a.build,&a.skin_tone,&a.eye_color,&a.hair_color,&a.hair_style,&a.hair_texture,&a.hair_thickness,&a.hair_density,&a.facial_hair,&a.distinguishing_features})o<<' '<<std::quoted(*s);
    o<<"\nMEMORIES "<<document_.memories.size()<<'\n';for(const auto &m:document_.memories){o<<"MEMORY "<<std::quoted(m.id)<<' '<<std::quoted(m.parent_id)<<' '<<unsigned(m.kind)<<' '<<std::quoted(m.title)<<' '<<std::quoted(m.body)<<' '<<m.importance<<' '<<m.confidence<<' '<<m.emotion<<' '<<unsigned(m.source)<<' '<<m.shareable<<' '<<m.occurred_hour<<' '<<m.half_life_hours<<' '<<m.enabled<<' '<<m.tags.size();for(const auto &s:m.tags)o<<' '<<std::quoted(s);o<<'\n';}o<<"END\n";out=o.str();error.clear();return true;
}
Result Bank::recall(const Query &q) const{
    Result out;if(!ready_||q.count>query_tag_limit||!range(q.game_hour,0,1e9))return out;
    std::array<std::string_view,query_tag_limit> tags={};unsigned count=0;
    for(unsigned i=0;i<q.count;i++){
        if(!identifier(q.tags[i],32,true))return out;
        bool duplicate=false;for(unsigned j=0;j<count;j++)duplicate|=tags[j]==q.tags[i];if(!duplicate)tags[count++]=q.tags[i];
    }out.valid=true;
    for(unsigned i=0;i<document_.memories.size();i++){
        const auto &m=document_.memories[i];if(m.occurred_hour>q.game_hour||(q.shareable_only&&!m.shareable))continue;
        std::array<bool,query_tag_limit> matched={};bool enabled=true;int at=int(i);
        while(at>=0){const auto &ancestor=document_.memories[unsigned(at)];enabled&=ancestor.enabled&&ancestor.occurred_hour<=q.game_hour;for(unsigned j=0;j<count;j++)for(const auto &tag:ancestor.tags)matched[j]=matched[j]||tag==tags[j];at=parents_[unsigned(at)];}
        if(!enabled)continue;
        unsigned hits=0;for(unsigned j=0;j<count;j++)hits+=matched[j]?1u:0u;if(count&&!hits)continue;
        const double relevance=count?std::min(1.,double(hits)/std::min(3u,count)):0.;
        const double decay=m.half_life_hours?std::exp2(-(q.game_hour-m.occurred_hour)/m.half_life_hours):1.;
        Hit candidate{i,hits,(.5*m.importance+.2*m.confidence+.1*std::abs(m.emotion)+.2*relevance)*decay,decay};unsigned place=0;
        while(place<out.count){const auto &other=out.hits[place];if(candidate.score>other.score||(candidate.score==other.score&&m.id<document_.memories[other.index].id))break;place++;}
        if(place>=recall_limit)continue;
        out.count=std::min(recall_limit,out.count+1);for(unsigned j=out.count-1;j>place;j--)out.hits[j]=out.hits[j-1];out.hits[place]=candidate;
    }return out;
}
}

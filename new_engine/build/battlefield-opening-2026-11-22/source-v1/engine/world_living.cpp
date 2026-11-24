#include "engine/world_platform.h"
#include "engine/world_living.h"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace world_living {
namespace {
constexpr std::uint64_t quantity_limit=1000000000000ull,tick_limit=1000000000000ull,wall_limit=1000000000000000ull;
bool clock_value(double seconds,std::uint64_t &milliseconds){
    if(!std::isfinite(seconds)||seconds<0||seconds>double(wall_limit)/1000.)return false;
    milliseconds=std::uint64_t(std::floor(seconds*1000.));return true;
}
bool label(const std::string &s){return !s.empty()&&s.size()<=64&&std::none_of(s.begin(),s.end(),[](unsigned char c){return c<32||c==127;});}
template<class T> int index(const std::vector<T> &v,Id id){for(unsigned i=0;i<v.size();i++)if(v[i].id==id)return int(i);return -1;}
template<class T> bool unique(const std::vector<T> &v){for(unsigned i=0;i<v.size();i++){if(!v[i].id||v[i].id>1000000000||!label(v[i].name))return false;for(unsigned j=0;j<i;j++)if(v[i].id==v[j].id)return false;}return true;}
std::uint64_t mixed(std::uint64_t x){x+=0x9e3779b97f4a7c15ull;x=(x^(x>>30))*0xbf58476d1ce4e5b9ull;x=(x^(x>>27))*0x94d049bb133111ebull;return x^(x>>31);}
bool contains(unsigned minute,unsigned start,unsigned end){return start<=end?(minute>=start&&minute<end):(minute>=start||minute<end);}
Activity scheduled(const Actor &a,unsigned minute){
    if(contains(minute,a.sleep_minute,a.wake_minute))return Activity::Sleep;
    if(contains(minute,a.meal_minute,(a.meal_minute+a.meal_duration)%1440))return Activity::Eat;
    if(a.commute_minutes&&(contains(minute,(a.work_start+1440-a.commute_minutes)%1440,a.work_start)||contains(minute,a.work_end,(a.work_end+a.commute_minutes)%1440)))return Activity::Commute;
    if(contains(minute,a.work_start,a.work_end))return a.guard?Activity::Guard:Activity::Work;
    return Activity::Leisure;
}
std::uint64_t saturation_add(std::uint64_t a,std::uint64_t b){return b>std::numeric_limits<std::uint64_t>::max()-a?std::numeric_limits<std::uint64_t>::max():a+b;}
std::string hash(const std::string &bytes){std::uint64_t h=14695981039346656037ull;for(unsigned char c:bytes){h^=c;h*=1099511628211ull;}std::ostringstream s;s<<std::hex<<std::setw(16)<<std::setfill('0')<<h;return s.str();}
bool write_all(int fd,const std::string &bytes){std::size_t n=0;while(n<bytes.size()){const auto k=write(fd,bytes.data()+n,bytes.size()-n);if(k<0&&errno==EINTR)continue;if(k<=0)return false;n+=std::size_t(k);}return fsync(fd)==0;}
bool atomic_save(const std::string &path,const std::string &bytes,std::string &error){
    if(path.empty()){error="Empty living-world path";return false;}
    std::string pattern=path+".tmp.XXXXXX";std::vector<char> temp(pattern.begin(),pattern.end());temp.push_back(0);const int fd=mkstemp(temp.data());
    if(fd<0){error="Cannot create living-world save transaction";return false;}
    bool ok=write_all(fd,bytes);if(close(fd)!=0)ok=false;if(ok)ok=vw_replace(temp.data(),path.c_str())==0;
    if(!ok){unlink(temp.data());error="Living-world save failed; previous file preserved";return false;}
    auto parent=std::filesystem::path(path).parent_path().string();if(parent.empty())parent=".";
#ifdef _WIN32
    return vw_directory_barrier(parent.c_str())==0;
#else
    const int dir=::vw_open(parent.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    const bool durable=dir>=0&&fsync(dir)==0;if(dir>=0)close(dir);if(!durable){error="Living-world save published but directory sync failed";return false;}return true;
#endif
}
bool tag(std::istream &in,const char *expected){std::string s;return bool(in>>s)&&s==expected;}
template<class A> bool read_array(std::istream &in,A &array){for(auto &v:array)if(!(in>>v))return false;return true;}
template<class A> void write_array(std::ostream &out,const A &array){for(const auto &v:array)out<<' '<<v;}
}

bool valid(const Definition &d,std::string &error){
    error.clear();auto fail=[&](const char *message){error=message;return false;};
    if(d.step_seconds<10||d.step_seconds>3600||d.start_minute>=1440||d.maximum_debt_seconds<3600||d.maximum_debt_seconds>30*86400)return fail("Invalid simulation timestep/start/debt budget");
    if(d.factions.size()>faction_limit||d.settlements.size()>settlement_limit||d.actors.size()>actor_limit||!unique(d.factions)||!unique(d.settlements)||!unique(d.actors))return fail("Invalid/duplicate world IDs, names or object budget");
    for(const auto &f:d.factions){if(f.allies.size()>faction_limit)return fail("Faction alliance budget exceeded");for(unsigned i=0;i<f.allies.size();i++){if(f.allies[i]==f.id||index(d.factions,f.allies[i])<0)return fail("Unknown/self faction alliance");for(unsigned j=0;j<i;j++)if(f.allies[i]==f.allies[j])return fail("Duplicate faction alliance");}}
    for(const auto &s:d.settlements){if(index(d.factions,s.faction)<0)return fail("Unknown settlement faction");for(unsigned r=0;r<resource_count;r++)if(s.stock[r]>s.capacity[r]||s.capacity[r]>quantity_limit||s.reserve[r]>quantity_limit)return fail("Settlement resource/capacity bounds exceeded");}
    for(const auto &a:d.actors){
        if(index(d.factions,a.faction)<0||index(d.settlements,a.home)<0||(a.trade_target&&index(d.settlements,a.trade_target)<0)||a.trade_target==a.home||unsigned(a.work_resource)>=resource_count||!a.work_amount||a.work_amount>1000||!a.ration||a.ration>10||a.cooldown_seconds<d.step_seconds||a.cooldown_seconds>86400||(a.allowed_actions&~all_actions))return fail("Invalid actor ownership/job/action/cooldown");
        if(a.wake_minute>=1440||a.work_start>=1440||a.work_end>=1440||a.sleep_minute>=1440||a.meal_minute>=1440||a.sleep_minute==a.wake_minute||a.work_start==a.work_end||!a.meal_duration||a.meal_duration>180||a.commute_minutes>120)return fail("Invalid NPC daily schedule");
    }
    return true;
}
const char *activity_name(Activity a){static const char *names[]={"Sleeping","Commuting","Working","Eating","On guard","Leisure"};return unsigned(a)<6?names[unsigned(a)]:"Invalid";}
const char *resource_name(Resource r){static const char *names[]={"Food","Wood","Ore","Coin"};return unsigned(r)<resource_count?names[unsigned(r)]:"Invalid";}
int World::settlement_index(Id id)const{return index(definition_.settlements,id);}
int World::actor_index(Id id)const{return index(definition_.actors,id);}
bool World::allied(Id a,Id b)const{
    if(a==b)return index(definition_.factions,a)>=0;
    const int ai=index(definition_.factions,a),bi=index(definition_.factions,b);if(ai<0||bi<0)return false;
    const auto &aa=definition_.factions[unsigned(ai)].allies,&bb=definition_.factions[unsigned(bi)].allies;
    return std::find(aa.begin(),aa.end(),b)!=aa.end()&&std::find(bb.begin(),bb.end(),a)!=bb.end();
}
unsigned World::minute()const{return unsigned((std::uint64_t(definition_.start_minute)*60+state_.tick*definition_.step_seconds)%86400/60);}
std::uint64_t World::day()const{return (std::uint64_t(definition_.start_minute)*60+state_.tick*definition_.step_seconds)/86400;}
Activity World::activity(unsigned i)const{return i<definition_.actors.size()?scheduled(definition_.actors[i],minute()):Activity::Leisure;}
void World::update_activities(){for(unsigned i=0;i<definition_.actors.size();i++)state_.actors[i].activity=activity(i);}
bool World::configure(const Definition &definition,double now,std::string &error){
    std::uint64_t wall=0;if(!valid(definition,error)||!clock_value(now,wall)){if(error.empty())error="Invalid initial wall clock";return false;}
    World next;next.definition_=definition;next.state_.clock_initialized=true;next.state_.last_wall_milliseconds=wall;
    for(unsigned i=0;i<definition.settlements.size();i++){const auto &s=definition.settlements[i];next.state_.settlements[i]={s.id,s.stock,s.reserve};}
    for(unsigned i=0;i<definition.actors.size();i++)next.state_.actors[i].id=definition.actors[i].id;
    next.update_activities();*this=std::move(next);return true;
}
bool World::observe_clock(double now,std::string &error){
    error.clear();std::uint64_t wall=0;if(!clock_value(now,wall)){error="Invalid/non-finite living-world clock";return false;}
    stats_.observed_clocks++;
    if(!state_.clock_initialized){state_.clock_initialized=true;state_.last_wall_milliseconds=wall;return true;}
    if(wall<state_.last_wall_milliseconds){stats_.rollback_clocks++;return true;} // retain high-water clock; never double-count a clock rollback
    const auto elapsed=wall-state_.last_wall_milliseconds;state_.last_wall_milliseconds=wall;
    if(!definition_.enabled)return true;
    const std::uint64_t cap=std::uint64_t(definition_.maximum_debt_seconds)*1000,room=cap-state_.debt_milliseconds,accepted=std::min(elapsed,room);
    state_.debt_milliseconds+=accepted;state_.dropped_milliseconds=saturation_add(state_.dropped_milliseconds,elapsed-accepted);return true;
}
bool World::rebase_clock(double now,std::string &error){
    error.clear();std::uint64_t wall=0;if(!clock_value(now,wall)){error="Invalid/non-finite living-world clock";return false;}
    state_.clock_initialized=true;state_.last_wall_milliseconds=std::max(state_.last_wall_milliseconds,wall);return true;
}
Proposal World::default_proposal(unsigned i)const{
    Proposal p;if(!definition_.enabled||i>=definition_.actors.size())return p;
    const auto &a=definition_.actors[i];const auto &as=state_.actors[i];const int si=settlement_index(a.home);if(si<0||state_.tick<as.next_action_tick||!allied(a.faction,definition_.settlements[unsigned(si)].faction))return p;
    const auto &s=state_.settlements[unsigned(si)];const auto act=activity(i);p.actor=a.id;p.source=a.home;p.resource=a.work_resource;
    if(act==Activity::Eat&&(a.allowed_actions&action_bit(Action::Consume))&&as.last_meal_day!=day()&&s.stock[0]>=a.ration){p.action=Action::Consume;p.resource=Resource::Food;p.amount=a.ration;}
    else if(act==Activity::Guard&&(a.allowed_actions&action_bit(Action::Patrol)))p.action=Action::Patrol;
    else if(act==Activity::Work){
        const unsigned r=unsigned(a.work_resource);
        if(a.trade_target&&(a.allowed_actions&action_bit(Action::Transfer))){const int ti=settlement_index(a.trade_target);if(ti>=0&&allied(a.faction,definition_.settlements[unsigned(ti)].faction)){p.action=Action::Transfer;p.target=a.trade_target;p.amount=std::min({std::uint64_t(a.work_amount),s.stock[r],definition_.settlements[unsigned(ti)].capacity[r]-state_.settlements[unsigned(ti)].stock[r]});}}
        else if(a.allowed_actions&action_bit(Action::Gather)){p.action=Action::Gather;p.amount=std::min({1+mixed(definition_.seed^std::uint64_t(a.id)*0x9e3779b97f4a7c15ull^state_.tick)%a.work_amount,s.reserve[r],definition_.settlements[unsigned(si)].capacity[r]-s.stock[r]});}
    }
    if(p.action!=Action::Patrol&&!p.amount)p.action=Action::None;
    return p;
}
bool World::propose(const Proposal &p,std::string &error){
    error.clear();auto reject=[&](const char *message){stats_.rejected_proposals++;error=message;return false;};
    if(!definition_.enabled)return reject("Living world is disabled");
    const int ai=actor_index(p.actor),si=settlement_index(p.source);if(ai<0||si<0||unsigned(p.resource)>=resource_count||unsigned(p.action)<1||unsigned(p.action)>4)return reject("Unknown actor/settlement/resource/action");
    const auto &a=definition_.actors[unsigned(ai)];auto &as=state_.actors[unsigned(ai)];auto &source=state_.settlements[unsigned(si)];const auto &source_def=definition_.settlements[unsigned(si)];const unsigned r=unsigned(p.resource);
    if(p.source!=a.home||!allied(a.faction,source_def.faction)||(a.allowed_actions&action_bit(p.action))==0)return reject("Actor ownership/action permission rejected");
    if(state_.tick<as.next_action_tick)return reject("Actor action cooldown is active");
    const auto act=activity(unsigned(ai));int target_index=-1;
    switch(p.action){
    case Action::Gather:
        if(act!=Activity::Work||p.target||p.resource!=a.work_resource||!p.amount||p.amount>a.work_amount||p.amount>source.reserve[r]||p.amount>source_def.capacity[r]-source.stock[r])return reject("Gather violates activity/reserve/capacity rules");
        break;
    case Action::Consume:
        if(act!=Activity::Eat||p.target||p.resource!=Resource::Food||p.amount!=a.ration||as.last_meal_day==day()||p.amount>source.stock[0])return reject("Meal violates activity/ration/stock rules");
        break;
    case Action::Transfer:
        target_index=settlement_index(p.target);
        if(act!=Activity::Work||!a.trade_target||p.target!=a.trade_target||target_index<0||p.resource!=a.work_resource||!p.amount||p.amount>a.work_amount||p.amount>source.stock[r]||!allied(a.faction,definition_.settlements[unsigned(target_index)].faction)||p.amount>definition_.settlements[unsigned(target_index)].capacity[r]-state_.settlements[unsigned(target_index)].stock[r])return reject("Transfer violates route/alliance/resource/capacity rules");
        break;
    case Action::Patrol:
        if(act!=Activity::Guard||p.target||p.amount)return reject("Patrol requires guard activity and no resource amount");
        break;
    default:return reject("No actionable policy proposal");
    }
    // Every check precedes mutation. Resources move between finite accounts;
    // food consumption has an explicit conserved sink, never negative stock.
    if(p.action==Action::Gather){source.reserve[r]-=p.amount;source.stock[r]+=p.amount;}
    else if(p.action==Action::Consume){source.stock[0]-=p.amount;state_.consumed[0]+=p.amount;as.last_meal_day=day();}
    else if(p.action==Action::Transfer){source.stock[r]-=p.amount;state_.settlements[unsigned(target_index)].stock[r]+=p.amount;}
    else as.patrols++;
    as.actions++;as.next_action_tick=state_.tick+(a.cooldown_seconds+definition_.step_seconds-1)/definition_.step_seconds;
    Event event;event.sequence=++state_.event_sequence;event.tick=state_.tick;event.proposal=p;
    const unsigned slot=(state_.event_start+state_.event_count)%event_limit;state_.events[slot]=event;
    if(state_.event_count<event_limit)state_.event_count++;else state_.event_start=(state_.event_start+1)%event_limit;
    return true;
}
ProcessResult World::process(unsigned budget,Policy policy,void *userdata){
    ProcessResult result;const auto step=std::uint64_t(definition_.step_seconds)*1000;
    if(definition_.enabled)for(unsigned n=0;n<std::min(budget,max_steps_per_call)&&state_.debt_milliseconds>=step;n++){
        if(state_.tick>=tick_limit){result.at_tick_limit=true;break;}
        state_.debt_milliseconds-=step;state_.tick++;update_activities();
        for(unsigned i=0;i<definition_.actors.size();i++){
            const auto &a=definition_.actors[i];if(state_.tick<state_.actors[i].next_action_tick)continue;
            Proposal p;bool supplied=false;
            if(policy&&i==(state_.tick-1)%definition_.actors.size()){
                const int home=settlement_index(a.home);Decision d{&definition_,&a,&state_.actors[i],home<0?nullptr:&state_.settlements[unsigned(home)],state_.tick,day(),mixed(definition_.seed^a.id^state_.tick),minute(),&state_};
                try{supplied=policy(d,p,userdata);}catch(...){stats_.policy_failures++;}
            }
            std::string ignored;if(supplied){if(p.actor==a.id&&propose(p,ignored))continue;stats_.policy_failures++;}
            p=default_proposal(i);if(p.action!=Action::None)propose(p,ignored);
        }
        result.steps++;stats_.processed_steps++;
    }
    result.pending_steps=step?state_.debt_milliseconds/step:0;return result;
}
bool World::advance(double now,unsigned budget,ProcessResult &result,std::string &error,Policy policy,void *userdata){if(!observe_clock(now,error))return false;result=process(budget,policy,userdata);return true;}
std::vector<Event> World::recent_events()const{std::vector<Event> out;out.reserve(state_.event_count);for(unsigned i=0;i<state_.event_count;i++)out.push_back(state_.events[(state_.event_start+i)%event_limit]);return out;}

bool World::valid_state(std::string &error)const{
    if(!valid(definition_,error))return false;
    auto fail=[&](const char *s){error=s;return false;};
    if((definition_.enabled&&!state_.clock_initialized)||(!state_.clock_initialized&&(state_.last_wall_milliseconds||state_.tick||state_.debt_milliseconds))||state_.last_wall_milliseconds>wall_limit||state_.debt_milliseconds>std::uint64_t(definition_.maximum_debt_seconds)*1000||state_.tick>tick_limit||state_.event_count!=std::min(std::uint64_t(event_limit),state_.event_sequence)||state_.event_start>=event_limit)return fail("Invalid simulation clock/debt/event bounds");
    for(unsigned r=1;r<resource_count;r++)if(state_.consumed[r])return fail("Only authored food rations can enter the consumption sink");
    Quantities initial={},accounted=state_.consumed;
    for(unsigned i=0;i<definition_.settlements.size();i++){const auto &d=definition_.settlements[i];const auto &s=state_.settlements[i];if(s.id!=d.id)return fail("Settlement state ID mismatch");for(unsigned r=0;r<resource_count;r++){if(s.stock[r]>d.capacity[r]||s.reserve[r]>d.reserve[r]||accounted[r]>quantity_limit*settlement_limit*2)return fail("Invalid settlement balances");initial[r]+=d.stock[r]+d.reserve[r];accounted[r]+=s.stock[r]+s.reserve[r];}}
    if(initial!=accounted)return fail("Resource conservation failed");
    std::uint64_t actions=0;for(unsigned i=0;i<definition_.actors.size();i++){const auto &a=state_.actors[i];const auto &d=definition_.actors[i];if(a.id!=d.id||a.activity!=activity(i)||a.next_action_tick>state_.tick+(d.cooldown_seconds+definition_.step_seconds-1)/definition_.step_seconds||(a.last_meal_day!=~std::uint64_t(0)&&a.last_meal_day>day())||a.actions>state_.tick+1||a.patrols>a.actions)return fail("Invalid NPC activity/cooldown/counter state");actions+=a.actions;}
    if(actions!=state_.event_sequence)return fail("NPC/event sequence mismatch");
    std::uint64_t previous_tick=0;std::array<std::uint64_t,actor_limit> last_action;last_action.fill(~std::uint64_t(0));
    for(unsigned i=0;i<state_.event_count;i++){
        const auto &e=state_.events[(state_.event_start+i)%event_limit];const auto &p=e.proposal;const int ai=actor_index(p.actor),si=settlement_index(p.source),ti=settlement_index(p.target);
        if(ai<0||si<0||unsigned(p.action)<1||unsigned(p.action)>4||unsigned(p.resource)>=resource_count||e.tick>state_.tick||(i&&e.tick<previous_tick)||e.sequence!=state_.event_sequence-state_.event_count+1+i)return fail("Invalid event identity/order");
        const auto &a=definition_.actors[unsigned(ai)];if(p.source!=a.home||!(a.allowed_actions&action_bit(p.action))||!allied(a.faction,definition_.settlements[unsigned(si)].faction))return fail("Invalid historical action ownership");
        const auto event_minute=unsigned((std::uint64_t(definition_.start_minute)*60+e.tick*definition_.step_seconds)%86400/60);const auto event_activity=scheduled(a,event_minute);
        if((p.action==Action::Consume&&event_activity!=Activity::Eat)||(p.action==Action::Patrol&&event_activity!=Activity::Guard)||((p.action==Action::Gather||p.action==Action::Transfer)&&event_activity!=Activity::Work))return fail("Historical action violates NPC schedule");
        const auto cooldown=(a.cooldown_seconds+definition_.step_seconds-1)/definition_.step_seconds;
        if(last_action[unsigned(ai)]!=~std::uint64_t(0)&&e.tick-last_action[unsigned(ai)]<cooldown)return fail("Historical action violates NPC cooldown");
        last_action[unsigned(ai)]=e.tick;
        if(p.action==Action::Transfer){if(ti<0||p.target!=a.trade_target||!allied(a.faction,definition_.settlements[unsigned(ti)].faction)||p.resource!=a.work_resource||!p.amount||p.amount>a.work_amount)return fail("Invalid historical transfer");}
        else if(p.target)return fail("Invalid historical target");
        if(p.action==Action::Gather&&(p.resource!=a.work_resource||!p.amount||p.amount>a.work_amount))return fail("Invalid historical gather");
        if(p.action==Action::Consume&&(p.resource!=Resource::Food||p.amount!=a.ration))return fail("Invalid historical meal");
        if(p.action==Action::Patrol&&p.amount)return fail("Invalid historical patrol");
        previous_tick=e.tick;
    }
    return true;
}
bool World::encode(std::string &out,std::string &error)const{
    error.clear();if(!valid_state(error))return false;std::ostringstream o;o<<std::setprecision(17);
    const auto &d=definition_;o<<"config "<<d.enabled<<' '<<d.seed<<' '<<d.step_seconds<<' '<<d.start_minute<<' '<<d.maximum_debt_seconds<<'\n';
    o<<"factions "<<d.factions.size()<<'\n';for(const auto &f:d.factions){o<<"faction "<<f.id<<' '<<std::quoted(f.name)<<' '<<f.allies.size();for(Id id:f.allies)o<<' '<<id;o<<'\n';}
    o<<"settlements "<<d.settlements.size()<<'\n';for(const auto &s:d.settlements){o<<"settlement "<<s.id<<' '<<s.faction<<' '<<std::quoted(s.name);write_array(o,s.stock);write_array(o,s.reserve);write_array(o,s.capacity);o<<'\n';}
    o<<"actors "<<d.actors.size()<<'\n';for(const auto &a:d.actors)o<<"actor "<<a.id<<' '<<a.faction<<' '<<a.home<<' '<<a.trade_target<<' '<<std::quoted(a.name)<<' '<<unsigned(a.work_resource)<<' '<<a.work_amount<<' '<<a.ration<<' '<<a.cooldown_seconds<<' '<<a.allowed_actions<<' '<<a.wake_minute<<' '<<a.work_start<<' '<<a.work_end<<' '<<a.sleep_minute<<' '<<a.meal_minute<<' '<<a.meal_duration<<' '<<a.commute_minutes<<' '<<a.guard<<'\n';
    const auto &s=state_;o<<"clock "<<s.clock_initialized<<' '<<s.last_wall_milliseconds<<' '<<s.debt_milliseconds<<' '<<s.tick<<' '<<s.dropped_milliseconds<<' '<<s.event_sequence;write_array(o,s.consumed);o<<'\n';
    o<<"settlement_states "<<d.settlements.size()<<'\n';for(unsigned i=0;i<d.settlements.size();i++){const auto &v=s.settlements[i];o<<"settlement_state "<<v.id;write_array(o,v.stock);write_array(o,v.reserve);o<<'\n';}
    o<<"actor_states "<<d.actors.size()<<'\n';for(unsigned i=0;i<d.actors.size();i++){const auto &v=s.actors[i];o<<"actor_state "<<v.id<<' '<<unsigned(v.activity)<<' '<<v.next_action_tick<<' '<<v.last_meal_day<<' '<<v.actions<<' '<<v.patrols<<'\n';}
    o<<"events "<<s.event_count<<'\n';for(unsigned i=0;i<s.event_count;i++){const auto &e=s.events[(s.event_start+i)%event_limit];const auto &p=e.proposal;o<<"event "<<e.sequence<<' '<<e.tick<<' '<<p.actor<<' '<<p.source<<' '<<p.target<<' '<<unsigned(p.action)<<' '<<unsigned(p.resource)<<' '<<p.amount<<'\n';}
    const auto body=o.str();out="VALWOOD_LIVING 1 "+hash(body)+"\n"+body;return true;
}
bool World::save(const std::string &path,std::string &error)const{std::string bytes;return encode(bytes,error)&&atomic_save(path,bytes,error);
}
bool World::load(const std::string &path,std::string &error){
    error.clear();std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file||file.tellg()<0||file.tellg()>1024*1024){error="Missing/oversized living-world save";return false;}
    const auto size=std::size_t(file.tellg());file.seekg(0);std::string bytes(size,'\0');if(!file.read(bytes.data(),std::streamsize(size))){error="Incomplete living-world read";return false;}
    return decode(bytes,error);
}
bool World::decode(const std::string &bytes,std::string &error){
    error.clear();if(bytes.size()>1024*1024||bytes.find('\0')!=std::string::npos){error="Invalid or oversized living state";return false;}
    const auto newline=bytes.find('\n');if(newline==std::string::npos||newline>128){error="Invalid living-world header";return false;}
    std::istringstream header(bytes.substr(0,newline));std::string magic,checksum,extra;unsigned version=0;
    if(!(header>>magic>>version>>checksum)||(header>>extra)||magic!="VALWOOD_LIVING"||version!=1||checksum!=hash(bytes.substr(newline+1))){error="Unsupported/corrupt living-world save";return false;}
    std::istringstream in(bytes.substr(newline+1));World next;auto &d=next.definition_;auto &s=next.state_;unsigned enabled=0,count=0;
    bool ok=tag(in,"config")&&bool(in>>enabled>>d.seed>>d.step_seconds>>d.start_minute>>d.maximum_debt_seconds)&&enabled<=1;d.enabled=enabled;
    ok=ok&&tag(in,"factions")&&bool(in>>count)&&count<=faction_limit;if(ok)d.factions.resize(count);
    for(auto &f:d.factions){unsigned allies=0;ok=ok&&tag(in,"faction")&&bool(in>>f.id>>std::quoted(f.name)>>allies)&&allies<=faction_limit;if(ok)f.allies.resize(allies);for(Id &id:f.allies)ok=ok&&bool(in>>id);}
    ok=ok&&tag(in,"settlements")&&bool(in>>count)&&count<=settlement_limit;if(ok)d.settlements.resize(count);
    for(auto &v:d.settlements)ok=ok&&tag(in,"settlement")&&bool(in>>v.id>>v.faction>>std::quoted(v.name))&&read_array(in,v.stock)&&read_array(in,v.reserve)&&read_array(in,v.capacity);
    ok=ok&&tag(in,"actors")&&bool(in>>count)&&count<=actor_limit;if(ok)d.actors.resize(count);
    for(auto &a:d.actors){unsigned resource=0,guard=0;ok=ok&&tag(in,"actor")&&bool(in>>a.id>>a.faction>>a.home>>a.trade_target>>std::quoted(a.name)>>resource>>a.work_amount>>a.ration>>a.cooldown_seconds>>a.allowed_actions>>a.wake_minute>>a.work_start>>a.work_end>>a.sleep_minute>>a.meal_minute>>a.meal_duration>>a.commute_minutes>>guard)&&resource<resource_count&&guard<=1;a.work_resource=Resource(resource);a.guard=guard;}
    unsigned initialized=0;ok=ok&&tag(in,"clock")&&bool(in>>initialized>>s.last_wall_milliseconds>>s.debt_milliseconds>>s.tick>>s.dropped_milliseconds>>s.event_sequence)&&initialized<=1&&read_array(in,s.consumed);s.clock_initialized=initialized;
    ok=ok&&tag(in,"settlement_states")&&bool(in>>count)&&count==d.settlements.size();
    for(unsigned i=0;i<d.settlements.size();i++){auto &v=s.settlements[i];ok=ok&&tag(in,"settlement_state")&&bool(in>>v.id)&&read_array(in,v.stock)&&read_array(in,v.reserve);}
    ok=ok&&tag(in,"actor_states")&&bool(in>>count)&&count==d.actors.size();
    for(unsigned i=0;i<d.actors.size();i++){auto &v=s.actors[i];unsigned activity=0;ok=ok&&tag(in,"actor_state")&&bool(in>>v.id>>activity>>v.next_action_tick>>v.last_meal_day>>v.actions>>v.patrols)&&activity<=unsigned(Activity::Leisure);v.activity=Activity(activity);}
    ok=ok&&tag(in,"events")&&bool(in>>count)&&count<=event_limit;if(ok)s.event_count=count;
    for(unsigned i=0;i<s.event_count;i++){auto &e=s.events[i];auto &p=e.proposal;unsigned action=0,resource=0;ok=ok&&tag(in,"event")&&bool(in>>e.sequence>>e.tick>>p.actor>>p.source>>p.target>>action>>resource>>p.amount)&&action<=unsigned(Action::Patrol)&&resource<resource_count;p.action=Action(action);p.resource=Resource(resource);}
    if(!ok||(in>>extra)||!next.valid_state(error)){if(error.empty())error="Malformed living-world save";return false;}*this=std::move(next);return true;
}
}

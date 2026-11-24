#include "engine/world_living.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

using namespace world_living;
namespace {
struct Files {std::string directory;Files(){char temp[]="/tmp/valwood-living-test-XXXXXX";const auto *path=mkdtemp(temp);assert(path);directory=path;}~Files(){std::filesystem::remove_all(directory);}std::string path(const char *name){return directory+"/"+name;}};
std::string read(const std::string &path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
void write(const std::string &path,const std::string &bytes){std::ofstream f(path,std::ios::binary);f<<bytes;assert(f.good());}
std::string checksummed(std::string body){std::uint64_t h=14695981039346656037ull;for(unsigned char c:body){h^=c;h*=1099511628211ull;}std::ostringstream out;out<<"VALWOOD_LIVING 1 "<<std::hex<<std::setw(16)<<std::setfill('0')<<h<<'\n'<<body;return out.str();}
std::string snapshot(World &w){Files f;std::string error;assert(w.save(f.path("state"),error));return read(f.path("state"));}
Definition authored(){
    Definition d;d.enabled=true;d.start_minute=480;d.seed=987654321;
    d.factions={{1,"Town",{2}},{2,"Forest allies",{1}},{3,"Bandits",{}}};
    Settlement town;town.id=10;town.faction=1;town.name="Castle farms";town.stock={50,10,5,20};town.reserve={10000,10000,10000,0};
    Settlement camp;camp.id=20;camp.faction=2;camp.name="Jungle camp";camp.stock={1,0,0,0};camp.reserve={200,100,0,0};
    Settlement bandits;bandits.id=30;bandits.faction=3;bandits.name="Old Tanthoon";bandits.stock={2,0,0,0};
    d.settlements={town,camp,bandits};
    Actor farmer;farmer.id=101;farmer.faction=1;farmer.home=10;farmer.name="Farmer";farmer.cooldown_seconds=600;farmer.work_amount=3;
    Actor wood=farmer;wood.id=102;wood.name="Woodworker";wood.work_resource=Resource::Wood;wood.cooldown_seconds=1200;
    Actor guard=farmer;guard.id=103;guard.name="Watch";guard.guard=true;guard.allowed_actions=action_bit(Action::Patrol)|action_bit(Action::Consume);guard.cooldown_seconds=3600;
    Actor trader=farmer;trader.id=104;trader.name="Provision trader";trader.trade_target=20;trader.cooldown_seconds=3600;trader.work_amount=2;
    d.actors={farmer,wood,guard,trader};return d;
}
void drain(World &w,unsigned budget){for(unsigned n=0;n<100000;n++){auto r=w.process(budget);if(!r.pending_steps)return;}assert(false);}
void conservation(const World &w){Quantities initial={},current=w.state().consumed;for(unsigned i=0;i<w.definition().settlements.size();i++){const auto &s=w.definition().settlements[i];for(unsigned r=0;r<resource_count;r++){initial[r]+=s.stock[r]+s.reserve[r];current[r]+=w.state().settlements[i].stock[r]+w.state().settlements[i].reserve[r];assert(w.state().settlements[i].stock[r]<=s.capacity[r]);}}assert(initial==current);}
void deterministic_batches(){
    std::string error;World a,b;const auto d=authored();assert(a.configure(d,1000,error));assert(b.configure(d,1000,error));assert(a.observe_clock(1000+3*86400,error));assert(b.observe_clock(1000+3*86400,error));drain(a,1);drain(b,64);assert(snapshot(a)==snapshot(b));assert(a.state().tick==4320);assert(a.state().event_count==event_limit);assert(a.state().event_sequence>event_limit);conservation(a);
    auto events=a.recent_events();assert(events.size()==event_limit);for(unsigned i=1;i<events.size();i++)assert(events[i].sequence==events[i-1].sequence+1);
    assert(a.state().actors[2].patrols>0&&a.state().consumed[0]>0); // guard and meal schedules actually run
    auto different=d;different.seed++;World c;assert(c.configure(different,1000,error));assert(c.observe_clock(1000+3*86400,error));drain(c,7);assert(c.state().settlements[0].stock!=a.state().settlements[0].stock);conservation(c);
}
void save_resume_offline(){
    Files files;std::string error;World uninterrupted,paused,resumed;auto d=authored();assert(uninterrupted.configure(d,5000,error));assert(paused.configure(d,5000,error));
    for(auto *w:{&uninterrupted,&paused}){assert(w->observe_clock(5120.5,error));assert(w->process(1).steps==1);assert(w->state().debt_milliseconds==60500);}
    assert(paused.save(files.path("living.save"),error));assert(resumed.load(files.path("living.save"),error));assert(snapshot(resumed)==snapshot(paused));
    assert(uninterrupted.observe_clock(5000+86400,error));assert(resumed.observe_clock(5000+86400,error));drain(uninterrupted,64);drain(resumed,5);assert(snapshot(uninterrupted)==snapshot(resumed));assert(resumed.state().debt_milliseconds==0);conservation(resumed);
    // Truncated/corrupt or unsupported saves cannot mutate a live world.
    const auto preserved=snapshot(resumed),valid=read(files.path("living.save"));auto bad=valid;bad.back()=bad.back()=='x'?'a':'x';write(files.path("bad"),bad);assert(!resumed.load(files.path("bad"),error));assert(snapshot(resumed)==preserved);
    write(files.path("bad"),"VALWOOD_LIVING 999 deadbeef\n");assert(!resumed.load(files.path("bad"),error));assert(snapshot(resumed)==preserved);
    write(files.path("living.save.tmp.abandoned"),"partial");assert(paused.load(files.path("living.save"),error));assert(snapshot(paused)!=preserved);
    const auto previous=read(files.path("living.save"));assert(std::filesystem::create_directory(files.path("blocked")));assert(!paused.save(files.path("blocked"),error));assert(read(files.path("living.save"))==previous);
}
void invalid_proposals_and_conservation(){
    std::string error;World w;auto d=authored();assert(w.configure(d,0,error));Proposal gather{101,10,0,Action::Gather,Resource::Food,3};const auto original=snapshot(w);
    for(unsigned kind=0;kind<8;kind++){auto p=gather;if(kind==0)p.actor=999;if(kind==1)p.source=999;if(kind==2)p.resource=Resource::Count;if(kind==3)p.amount=~std::uint64_t(0);if(kind==4)p.target=20;if(kind==5)p.action=Action::None;if(kind==6)p.resource=Resource::Wood;if(kind==7)p.action=Action::Consume;assert(!w.propose(p,error));assert(snapshot(w)==original);}
    assert(w.propose(gather,error));assert(w.state().settlements[0].stock[0]==53&&w.state().settlements[0].reserve[0]==9997);assert(!w.propose(gather,error));conservation(w);
    Proposal trade{104,10,20,Action::Transfer,Resource::Food,2};assert(w.propose(trade,error));assert(w.state().settlements[1].stock[0]==3);conservation(w);
    d.actors[3].trade_target=30;World hostile;assert(hostile.configure(d,0,error));trade.target=30;const auto before=snapshot(hostile);assert(!hostile.propose(trade,error));assert(snapshot(hostile)==before);
    d=authored();d.settlements[0].capacity[0]=d.settlements[0].stock[0];World full;assert(full.configure(d,0,error));assert(!full.propose(gather,error));
    d=authored();d.settlements[0].reserve[0]=0;assert(full.configure(d,0,error));assert(!full.propose(gather,error));
    d=authored();d.start_minute=720;World meal;assert(meal.configure(d,0,error));Proposal eat{101,10,0,Action::Consume,Resource::Food,1};assert(meal.propose(eat,error));assert(meal.state().consumed[0]==1);assert(meal.observe_clock(1200,error));drain(meal,20);assert(!meal.propose(eat,error));assert(meal.state().actors[0].last_meal_day==0);conservation(meal);
    d=authored();d.actors[0].allowed_actions=0;World forbidden;assert(forbidden.configure(d,0,error));assert(!forbidden.propose(gather,error));
}
struct PolicyState {unsigned calls=0;bool throws=false,wrong_actor=false;};
bool malicious(const Decision &d,Proposal &p,void *data){auto &s=*static_cast<PolicyState *>(data);s.calls++;if(s.throws)throw std::runtime_error("policy failure");p={s.wrong_actor?999u:d.actor->id,d.actor->home,0,Action::Gather,Resource::Food,~std::uint64_t(0)};return true;}
void policy_boundary(){
    auto d=authored();std::string error;World rules,untrusted,throwing;assert(rules.configure(d,0,error));assert(untrusted.configure(d,0,error));assert(throwing.configure(d,0,error));assert(rules.observe_clock(3600,error));assert(untrusted.observe_clock(3600,error));assert(throwing.observe_clock(3600,error));PolicyState bad,throws;throws.throws=true;
    auto r=rules.process(64);auto u=untrusted.process(64,malicious,&bad);auto t=throwing.process(64,malicious,&throws);assert(r.steps==60&&u.steps==60&&t.steps==60);assert(bad.calls<=u.steps&&throws.calls<=t.steps);assert(untrusted.stats().policy_failures==bad.calls&&throwing.stats().policy_failures==throws.calls);assert(snapshot(rules)==snapshot(untrusted));assert(snapshot(rules)==snapshot(throwing));
    World wrong;assert(wrong.configure(d,0,error));assert(wrong.observe_clock(3600,error));PolicyState ids;ids.wrong_actor=true;wrong.process(64,malicious,&ids);assert(snapshot(wrong)==snapshot(rules));
}
void clocks_and_debt(){
    std::string error;auto d=authored();d.step_seconds=10;d.maximum_debt_seconds=3600;World w;assert(w.configure(d,1000,error));assert(w.observe_clock(1000.5,error));assert(w.observe_clock(999,error));assert(w.state().last_wall_milliseconds==1000500&&w.state().debt_milliseconds==500);assert(w.stats().rollback_clocks==1);assert(w.observe_clock(1000.6,error));assert(w.state().debt_milliseconds==600);
    const auto before=snapshot(w);for(double bad:{-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN(),1e13}){assert(!w.observe_clock(bad,error));assert(snapshot(w)==before);}
    assert(w.observe_clock(1e12,error));assert(w.state().debt_milliseconds==3600000&&w.state().dropped_milliseconds>0);auto r=w.process(~0u);assert(r.steps==64&&r.pending_steps==296);const auto debt=w.state().debt_milliseconds;r=w.process(0);assert(r.steps==0&&w.state().debt_milliseconds==debt);drain(w,64);assert(w.state().tick==360);conservation(w);
    // Discarding an editor preview excludes its wall time, retaining the
    // original fractional debt and rejecting invalid rebases atomically.
    World preview;assert(preview.configure(d,1000,error));assert(preview.observe_clock(1000.5,error));
    const auto unchanged=preview.state().settlements[0].stock;
    assert(preview.rebase_clock(5000,error));assert(preview.state().debt_milliseconds==500);
    assert(preview.state().settlements[0].stock==unchanged);
    assert(preview.observe_clock(5010,error));assert(preview.state().debt_milliseconds==10500);
    const auto prior=snapshot(preview);assert(!preview.rebase_clock(std::numeric_limits<double>::quiet_NaN(),error));assert(snapshot(preview)==prior);
    // Disabled defaults perform no resource/NPC actions and accumulate no debt.
    Definition empty;World disabled;assert(disabled.configure(empty,0,error));ProcessResult idle;assert(disabled.advance(99999,64,idle,error));assert(!idle.steps&&!idle.pending_steps&&disabled.state().event_count==0);
    auto bad=d;bad.actors.push_back(bad.actors[0]);assert(!valid(bad,error));bad=d;bad.actors[0].home=777;assert(!valid(bad,error));bad=d;bad.step_seconds=0;assert(!valid(bad,error));bad=d;bad.settlements[0].stock[0]=~std::uint64_t(0);assert(!valid(bad,error));
}
void structural_validation_and_budgets(){
    Files files;std::string error;World w;auto d=authored();assert(w.configure(d,0,error));const auto original=snapshot(w);auto body=original.substr(original.find('\n')+1);
    auto balance=body;const auto pos=balance.find("settlement_state 10 50");assert(pos!=std::string::npos);balance.replace(pos,std::string("settlement_state 10 50").size(),"settlement_state 10 51");write(files.path("unbalanced"),checksummed(balance));assert(!w.load(files.path("unbalanced"),error));assert(snapshot(w)==original);
    auto overflow=body;const auto clock=overflow.find("clock 1 0 0 0 ");assert(clock!=std::string::npos);overflow.replace(clock,std::string("clock 1 0 0 0 ").size(),"clock 1 0 0 18446744073709551615 ");write(files.path("overflow"),checksummed(overflow));assert(!w.load(files.path("overflow"),error));assert(snapshot(w)==original);
    auto crowded=d;crowded.actors.clear();for(unsigned i=0;i<actor_limit;i++){auto a=d.actors[0];a.id=1000+i;a.name="Resident "+std::to_string(i);crowded.actors.push_back(a);}assert(w.configure(crowded,0,error));assert(w.observe_clock(3600,error));PolicyState policy;auto result=w.process(~0u,malicious,&policy);assert(result.steps==60&&policy.calls<=result.steps);assert(w.state().event_count<=event_limit);conservation(w);assert(w.save(files.path("crowd"),error));World loaded;assert(loaded.load(files.path("crowd"),error));assert(snapshot(loaded)==snapshot(w));
}
}
int main(){deterministic_batches();save_resume_offline();invalid_proposals_and_conservation();policy_boundary();clocks_and_debt();structural_validation_and_budgets();std::puts("world_living: deterministic step batching, persistent resume/fractional debt, conserved finite resources, daily NPC activities, strict policy/ID/cooldown/alliance gates, checksummed structural validation, bounded catchup/crowds/event ring and rollback/non-finite/overflow clocks passed");}

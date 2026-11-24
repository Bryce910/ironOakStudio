#include "engine/world_runtime_demo.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>
using namespace world_runtime_demo;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<" | "<<error<<'\n';return 1;}}while(0)
int main(){
    std::string error;world_living::World living;Sites sites;sites.castle_x=1000;sites.castle_z=2000;sites.jungle_x=-10000;sites.jungle_z=8000;sites.jungle_floor=120;Seed seed;
    CHECK(build(sites,{},living,true,1000,seed,error));world_runtime::Runtime runtime;CHECK(runtime.configure(seed.configuration,error));world_runtime::Frame frame;
    auto tick=[&](){return runtime.tick(1./60.,1000,{1000-42.5,24,2000-68},frame);};
    CHECK(runtime.quest(journey_id)==world_gameplay::Progress::Inactive);
    CHECK(runtime.enqueue({world_gameplay::EventKind::Interact,instructor_id}));CHECK(tick());CHECK(runtime.quest(journey_id)==world_gameplay::Progress::Active);CHECK(runtime.choices().count==2);
    auto recollection=runtime.recollection();CHECK(recollection.visits==1&&recollection.knowledge&&!recollection.recognition);CHECK(runtime.choose(1));CHECK(tick());
    CHECK(runtime.enqueue({world_gameplay::EventKind::Interact,instructor_id}));CHECK(tick());recollection=runtime.recollection();CHECK(recollection.visits==2&&recollection.recognition);CHECK(runtime.choose(0));CHECK(tick());
    bool training=false;for(unsigned i=0;i<frame.requests.count;i++)training|=frame.requests.values[i].kind==world_gameplay::CommandKind::StartTraining;CHECK(training);
    char folder[]="/tmp/valwood-journey-XXXXXX";CHECK(mkdtemp(folder));const std::filesystem::path root(folder);auto save=(root/"demo.progress").string();world_runtime::Runtime::Player player;player.feet={1020,24,1920};player.yaw=.7;player.pitch=-.2;player.third_person=false;
    CHECK(!runtime.save_progress(save.c_str(),player,error));CHECK(!std::filesystem::exists(save));
    CHECK(runtime.enqueue({world_gameplay::EventKind::EncounterFinished,0,training_encounter_id,0,0}));CHECK(tick());CHECK(runtime.counter(mastery_counter)==0);CHECK(runtime.enqueue({world_gameplay::EventKind::Interact,instructor_id}));CHECK(tick());CHECK(runtime.recollection().recognition&&std::string(runtime.recollection().recognition).find("last round")!=std::string::npos);CHECK(runtime.choose(0));CHECK(tick());
    CHECK(runtime.enqueue({world_gameplay::EventKind::EncounterFinished,0,training_encounter_id,0,1}));CHECK(tick());CHECK(runtime.counter(mastery_counter)==1);
    CHECK(runtime.enqueue({world_gameplay::EventKind::Interact,instructor_id}));CHECK(tick());CHECK(std::string(runtime.recollection().recognition).find("victory")!=std::string::npos);CHECK(runtime.choose(1));CHECK(tick());
    CHECK(runtime.enqueue({world_gameplay::EventKind::Interact,jungle_actor_first}));CHECK(tick());CHECK(runtime.choices().count==2);CHECK(runtime.choose(1));CHECK(tick());CHECK(runtime.counter(camp_counter)==1&&runtime.quest(journey_id)==world_gameplay::Progress::Active);
    CHECK(runtime.save_progress(save.c_str(),player,error));const auto observations=runtime.experiences()->sequence;
    CHECK(runtime.enqueue({world_gameplay::EventKind::Interact,instructor_id}));CHECK(tick());CHECK(runtime.choices().count==3);CHECK(runtime.choose(2));
    CHECK(runtime.enqueue({world_gameplay::EventKind::Interact,instructor_id}));CHECK(runtime.enqueue({world_gameplay::EventKind::DialogueChoice,0,730001,730011,2}));CHECK(tick());CHECK(runtime.quest(journey_id)==world_gameplay::Progress::Complete&&runtime.counter(coin_counter)==25&&runtime.counter(renown_counter)==1);
    for(unsigned i=0;i<3;i++){CHECK(runtime.enqueue({world_gameplay::EventKind::Interact,instructor_id}));CHECK(tick());CHECK(runtime.choices().count==2);CHECK(runtime.choose(1));CHECK(tick());}CHECK(runtime.counter(coin_counter)==25);
    world_runtime::Runtime::Player restored;CHECK(runtime.load_progress(save.c_str(),restored,2000,error));CHECK(runtime.quest(journey_id)==world_gameplay::Progress::Active&&runtime.counter(camp_counter)==1&&runtime.counter(coin_counter)==0);CHECK(runtime.experiences()->sequence==observations&&restored.feet.x==player.feet.x&&restored.yaw==player.yaw&&!restored.third_person);
    // A freshly configured process can resume; memory and system signatures
    // do not depend on pointers, allocation addresses or process lifetime.
    world_runtime::Runtime fresh;CHECK(fresh.configure(seed.configuration,error));CHECK(fresh.load_progress(save.c_str(),restored,2000,error));CHECK(fresh.experiences()->sequence==observations&&fresh.counter(mastery_counter)==1);
    // Saved queued choices survive, but the running encounter is never saved.
    CHECK(runtime.enqueue({world_gameplay::EventKind::Interact,instructor_id}));CHECK(tick());CHECK(runtime.choose(2));CHECK(runtime.save_progress(save.c_str(),player,error));CHECK(runtime.load_progress(save.c_str(),restored,2000,error));CHECK(tick());CHECK(runtime.counter(coin_counter)==25&&runtime.quest(journey_id)==world_gameplay::Progress::Complete);
    world_runtime::Snapshot checkpoint;CHECK(runtime.snapshot(checkpoint,error));auto before=runtime.experiences()->sequence;CHECK(runtime.enqueue({world_gameplay::EventKind::Interact,instructor_id}));CHECK(tick());CHECK(runtime.experiences()->sequence>before);CHECK(runtime.restore(checkpoint,2000,error));CHECK(runtime.experiences()->sequence==before);
    // Corruption/truncation cannot partially replace player, rewards, bodies,
    // external authoritative world, or witnessed memories.
    std::ifstream input(save,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(input)),{});auto corrupt=root/"bad.progress";std::ofstream out(corrupt,std::ios::binary);out<<bytes.substr(0,bytes.size()/2);out.close();restored.feet.x=777;
    CHECK(!runtime.load_progress(corrupt.c_str(),restored,3000,error));CHECK(restored.feet.x==777&&runtime.counter(coin_counter)==25&&runtime.experiences()->sequence==before);
    CHECK(symlink("demo.progress",(root/"linked.progress").c_str())==0);CHECK(!runtime.load_progress((root/"linked.progress").c_str(),restored,3000,error));CHECK(!runtime.save_progress((root/"linked.progress").c_str(),player,error));
    // Version two stores inventory in the same atomic checkpoint as earned
    // rewards. A corrupt ledger must not replace any runtime/player state.
    player.town.spent=26;CHECK(!runtime.save_progress(save.c_str(),player,error));
    player.town={11,2,1,7,1};CHECK(runtime.save_progress(save.c_str(),player,error));
    CHECK(fresh.load_progress(save.c_str(),restored,3000,error));CHECK(restored.town.spent==11&&restored.town.bread==2&&restored.town.apples==1&&restored.town.prepared==7&&restored.town.welcome==1);
    std::ifstream v2input(save,std::ios::binary);std::string v2((std::istreambuf_iterator<char>(v2input)),{});auto body=v2.substr(v2.find('\n')+1);
    auto write_version=[&](unsigned version,const std::string &data){std::ofstream f(corrupt,std::ios::binary);f<<"VALWOOD_PROGRESS "<<version<<' '<<world_npc_minds::hash(data)<<'\n'<<data;};
    auto position=body.find("TOWN ");CHECK(position!=std::string::npos);auto end=body.find('\n',position);auto legacy=body;legacy.erase(position,end-position+1);write_version(1,legacy);
    CHECK(fresh.load_progress(corrupt.c_str(),restored,3000,error));CHECK(restored.town.spent==0&&restored.town.bread==0&&restored.town.prepared==0&&fresh.counter(coin_counter)==25);
    auto invalid_town=body;invalid_town.replace(position,end-position,"TOWN 26 2 1 7 1");write_version(2,invalid_town);restored.feet.x=777;
    CHECK(!fresh.load_progress(corrupt.c_str(),restored,3000,error));CHECK(restored.feet.x==777&&fresh.counter(coin_counter)==25&&restored.town.spent==0);
    invalid_town=body;invalid_town.replace(position,end-position,"TOWN 11 9 1 7 1");write_version(2,invalid_town);CHECK(!fresh.load_progress(corrupt.c_str(),restored,3000,error));CHECK(restored.feet.x==777);
    auto profile=seed.configuration.minds[1].memory;profile.memories[0].shareable=false;profile.memories[0].body="Private secret";CHECK(runtime.bind_memory(jungle_actor_first,profile,error));CHECK(runtime.enqueue({world_gameplay::EventKind::Interact,jungle_actor_first}));CHECK(tick());CHECK(!runtime.recollection().knowledge);
    CHECK(!runtime.load_progress(save.c_str(),restored,3000,error));CHECK(runtime.counter(coin_counter)==25); // changed profile rejects older mind signature
    auto minds=(root/"world.vwminds").string();CHECK(runtime.save_minds(minds.c_str(),error));CHECK(fresh.load_minds(minds.c_str(),error));CHECK(fresh.enqueue({world_gameplay::EventKind::Interact,jungle_actor_first}));CHECK(fresh.tick(.016,2000,{},frame));CHECK(!fresh.recollection().knowledge);
    // A goal in the browser's native schema drives the actual motion target.
    auto goal=seed.configuration.minds[2].memory;goal.memories.back().tags={"duty","work"};CHECK(runtime.bind_memory(castle_actor_first,goal,error));CHECK(runtime.read_frame(frame));bool duty=false;for(unsigned i=0;i<frame.motion_count;i++)if(frame.motion[i].entity==castle_actor_first){duty=frame.motion[i].activity==world_living::Activity::Work;CHECK(frame.motion[i].destination.x==sites.castle_x+21);}CHECK(duty);
    world_npc_minds::Minds bounded;CHECK(bounded.configure(seed.configuration.minds,error));for(unsigned i=0;i<300;i++)CHECK(bounded.observe(instructor_id,world_npc_minds::Event::Choice,i,i));CHECK(bounded.state().count==256);CHECK(!bounded.observe(123,world_npc_minds::Event::Meeting,0,300));const auto state=bounded.state();auto invalid=state;invalid.values[0].last_hour=-1;CHECK(!bounded.restore(invalid,error));CHECK(bounded.state().sequence==state.sequence);
    std::filesystem::remove_all(root);std::cout<<"world_demo_journey: "<<checks<<" complete quest/retry/reward/no-farming/committed memory/private recall/duty/save-restart/queued choice/corrupt rollback/Play/portable binding/bounded experience checks\n";
}

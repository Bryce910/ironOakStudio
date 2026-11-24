#include "engine/world_platform.h"
#include "engine/world_runtime_components.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <thread>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace world_runtime {
namespace {
bool finite(double x,double maximum=1e8){return std::isfinite(x)&&std::abs(x)<=maximum;}
bool clock_ok(double x){return std::isfinite(x)&&x>=0&&x<=1e12;}
bool minutes_inside(unsigned minute,unsigned from,unsigned to){return from<=to?(minute>=from&&minute<to):(minute>=from||minute<to);}
bool vec(Vec p){return finite(p.x)&&finite(p.y)&&finite(p.z);}
bool pose(const Pose &p){const auto q=p.rotation;return vec(p.position)&&finite(q.x,1)&&finite(q.y,1)&&finite(q.z,1)&&finite(q.w,1)&&std::abs(double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w-1)<.02;}
bool relative_asset(const std::string &s,const char *extension){
    if(s.empty())return true;
    if(s.size()>511||s.find_first_of("\r\n\t")!=std::string::npos||s.find('\0')!=std::string::npos)return false;
    const std::filesystem::path p(s);if(p.is_absolute()||p.extension()!=extension)return false;
    for(const auto &part:p)if(part==".."||part=="."||part.empty())return false;
    return true;
}
bool asset_path(const std::filesystem::path &root,const std::string &s,std::string &out,std::string &error){
    std::error_code ec;auto p=root;for(const auto &part:std::filesystem::path(s)){p/=part;auto status=std::filesystem::symlink_status(p,ec);if(ec||std::filesystem::is_symlink(status)){error="Component asset is missing or traverses a symlink: "+s;return false;}}
    if(!std::filesystem::is_regular_file(p,ec)||ec){error="Component asset is not a regular project file: "+s;return false;}out=p.string();return true;
}
world_physics::Quaternion multiply(world_physics::Quaternion a,world_physics::Quaternion b){return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
Vec rotate(world_physics::Quaternion q,Vec p){const Vec t={2*(q.y*p.z-q.z*p.y),2*(q.z*p.x-q.x*p.z),2*(q.x*p.y-q.y*p.x)};return {p.x+q.w*t.x+q.y*t.z-q.z*t.y,p.y+q.w*t.y+q.z*t.x-q.x*t.z,p.z+q.w*t.z+q.x*t.y-q.y*t.x};}
Pose compose(const Pose &a,const Pose &b){auto r=rotate(a.rotation,b.position);return {{a.position.x+r.x,a.position.y+r.y,a.position.z+r.z},multiply(a.rotation,b.rotation)};}
template<class Items>int index(const Items &v,Id id){for(unsigned i=0;i<v.size();++i)if(v[i].id==id)return int(i);return -1;}
bool unsigned_id(std::istream &in,Id &id){std::string s;if(!(in>>s)||s.empty()||s.size()>20||!std::all_of(s.begin(),s.end(),[](char c){return c>='0'&&c<='9';}))return false;try{id=std::stoull(s);return id!=0;}catch(...){return false;}}
void write_pose(std::ostream &out,const Pose &p){out<<p.position.x<<' '<<p.position.y<<' '<<p.position.z<<' '<<p.rotation.x<<' '<<p.rotation.y<<' '<<p.rotation.z<<' '<<p.rotation.w;}
bool read_pose(std::istream &in,Pose &p){return bool(in>>p.position.x>>p.position.y>>p.position.z>>p.rotation.x>>p.rotation.y>>p.rotation.z>>p.rotation.w);}
std::string encode(const Document &d){std::ostringstream out;out<<std::setprecision(17)<<"VALWOOD_COMPONENTS 1\nassets "<<std::quoted(d.physics_asset)<<' '<<std::quoted(d.gameplay_asset)<<' '<<std::quoted(d.living_asset)<<'\n';for(const auto &b:d.physics){out<<"physics "<<b.entity<<' '<<b.body<<' ';write_pose(out,b.body_to_entity);out<<'\n';}for(const auto &b:d.living){out<<"living "<<b.entity<<' '<<b.actor<<' '<<b.speed<<' '<<b.arrival_radius;for(Vec p:{b.home,b.work,b.leisure,b.guard})out<<' '<<p.x<<' '<<p.y<<' '<<p.z;out<<'\n';}return out.str();}
std::uint64_t signature(const Configuration &c){
    std::uint64_t hash=14695981039346656037ull;auto bytes=[&](const void *data,std::size_t n){auto p=static_cast<const unsigned char *>(data);while(n--)hash=(hash^*p++)*1099511628211ull;};
    const auto text=encode(c.components);bytes(text.data(),text.size());
    for(const auto &e:c.entities){bytes(&e.id,sizeof e.id);bytes(&e.pose.position.x,sizeof(double));bytes(&e.pose.position.y,sizeof(double));bytes(&e.pose.position.z,sizeof(double));bytes(&e.pose.rotation.x,sizeof(float));bytes(&e.pose.rotation.y,sizeof(float));bytes(&e.pose.rotation.z,sizeof(float));bytes(&e.pose.rotation.w,sizeof(float));bytes(&e.scale,sizeof e.scale);bytes(&e.active,sizeof e.active);}
    bytes(&c.physics_enabled,sizeof c.physics_enabled);bytes(&c.gameplay_enabled,sizeof c.gameplay_enabled);bytes(&c.living_enabled,sizeof c.living_enabled);return hash;
}
bool atomic_write(const char *file,const std::string &text,std::string &error){
    if(!file||!*file){error="Missing components filename";return false;}struct stat info;if(lstat(file,&info)==0&&!S_ISREG(info.st_mode)){error="Components destination must be a regular file";return false;}
    std::string pattern=std::string(file)+".tmp.XXXXXX";std::vector<char> temp(pattern.begin(),pattern.end());temp.push_back(0);int fd=mkstemp(temp.data());if(fd<0){error="Cannot create components temporary file";return false;}
    std::size_t done=0;while(done<text.size()){const auto count=write(fd,text.data()+done,text.size()-done);if(count<=0)break;done+=std::size_t(count);}const bool synced=done==text.size()&&fsync(fd)==0;const bool closed=close(fd)==0;bool ok=synced&&closed&&vw_replace(temp.data(),file)==0;if(!ok){unlink(temp.data());error="Cannot atomically write components asset";}return ok;
}
}
bool validate(const Document &d,std::string &error){
    if(!relative_asset(d.physics_asset,".physics")||!relative_asset(d.gameplay_asset,".gameplay")||!relative_asset(d.living_asset,".living")){error="Component assets must use project-relative typed paths";return false;}
    if(d.physics.size()>world_physics::body_limit||d.living.size()>world_living::actor_limit){error="Component binding limit exceeded";return false;}
    for(unsigned i=0;i<d.physics.size();++i){const auto &b=d.physics[i];if(!b.entity||!b.body||!pose(b.body_to_entity)){error="Invalid physics scene binding";return false;}for(unsigned j=0;j<i;++j)if(d.physics[j].entity==b.entity||d.physics[j].body==b.body){error="Physics scene bindings must be one-to-one";return false;}}
    for(unsigned i=0;i<d.living.size();++i){const auto &b=d.living[i];if(!b.entity||!b.actor||!vec(b.home)||!vec(b.work)||!vec(b.leisure)||!vec(b.guard)||!finite(b.speed,10)||b.speed<=0||!finite(b.arrival_radius,100)||b.arrival_radius<0){error="Invalid living scene binding";return false;}for(unsigned j=0;j<i;++j)if(d.living[j].entity==b.entity||d.living[j].actor==b.actor){error="Living scene bindings must be one-to-one";return false;}for(const auto &p:d.physics)if(p.entity==b.entity){error="A scene entity cannot have competing physics and living motion owners";return false;}}
    error.clear();return true;
}
bool save(const char *path,const Document &d,std::string &error){return validate(d,error)&&atomic_write(path,encode(d),error);}
bool load(const char *path,Document &d,std::string &error){
    if(!path){error="Missing components filename";return false;}int fd=vw_open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0){error="Cannot open components asset";return false;}struct stat info;if(fstat(fd,&info)||!S_ISREG(info.st_mode)||info.st_size<0||info.st_size>128*1024){close(fd);error="Invalid or oversized components asset";return false;}
    std::string text(std::size_t(info.st_size),'\0');std::size_t done=0;while(done<text.size()){const auto n=read(fd,text.data()+done,text.size()-done);if(n<=0)break;done+=std::size_t(n);}close(fd);if(done!=text.size()||text.find('\0')!=std::string::npos){error="Truncated components asset";return false;}
    Document candidate;std::istringstream input(text);std::string line;bool header=false,assets=false;unsigned lines=0;
    while(std::getline(input,line)){if(++lines>world_physics::body_limit+world_living::actor_limit+2){error="Too many components records";return false;}std::istringstream in(line);std::string token;in>>token;
        if(!header){unsigned version=0;if(token!="VALWOOD_COMPONENTS"||!(in>>version)||version!=1){error="Invalid components header";return false;}header=true;}
        else if(token=="assets"&&!assets){if(!(in>>std::quoted(candidate.physics_asset)>>std::quoted(candidate.gameplay_asset)>>std::quoted(candidate.living_asset))){error="Invalid components paths";return false;}assets=true;}
        else if(token=="physics"){PhysicsBinding b;if(!unsigned_id(in,b.entity)||!unsigned_id(in,b.body)||!read_pose(in,b.body_to_entity)){error="Invalid physics binding record";return false;}candidate.physics.push_back(b);}
        else if(token=="living"){LivingBinding b;Id actor=0;if(!unsigned_id(in,b.entity)||!unsigned_id(in,actor)||actor>UINT32_MAX||!(in>>b.speed>>b.arrival_radius)){error="Invalid living binding record";return false;}b.actor=world_living::Id(actor);for(auto p:{&b.home,&b.work,&b.leisure,&b.guard})if(!(in>>p->x>>p->y>>p->z)){error="Invalid living target record";return false;}candidate.living.push_back(b);}
        else{error="Unknown or duplicated components record";return false;}in>>std::ws;if(!in.eof()){error="Trailing components record data";return false;}
    }if(!header||!assets||!validate(candidate,error)){if(error.empty())error="Missing components metadata";return false;}d=std::move(candidate);return true;
}
bool load_project(const std::string &root_path,const Document &d,const std::vector<Entity> &entities,double wall,Configuration &out,std::string &error){
    if(!validate(d,error)||!clock_ok(wall)){if(error.empty())error="Invalid component loading clock";return false;}std::error_code ec;const auto root=std::filesystem::canonical(root_path,ec);if(ec||!std::filesystem::is_directory(root,ec)){error="Invalid component project root";return false;}
    Configuration c;c.components=d;c.entities=entities;std::string path;
    if(!d.physics_asset.empty()){if(!asset_path(root,d.physics_asset,path,error)||!world_physics::load(path.c_str(),c.physics,error))return false;c.physics_enabled=true;}
    if(!d.gameplay_asset.empty()){if(!asset_path(root,d.gameplay_asset,path,error)||!world_gameplay::load(path.c_str(),c.gameplay,error))return false;c.gameplay_enabled=true;}
    if(!d.living_asset.empty()){world_living::World authored;if(!asset_path(root,d.living_asset,path,error)||!authored.load(path,error)||!c.living.configure(authored.definition(),wall,error))return false;c.living_enabled=true;}
    out=std::move(c);return true;
}
struct Runtime::Impl {
    Configuration configuration;std::thread::id owner=std::this_thread::get_id();
    std::unique_ptr<world_physics::World> physics;
    std::unique_ptr<world_gameplay::Runtime,void(*)(world_gameplay::Runtime *)> gameplay{nullptr,world_gameplay::destroy};
    world_npc_minds::Minds minds;
    world_living::World living;std::array<Entity,entity_limit> entities={};unsigned count=0;
    std::array<unsigned,world_physics::body_limit> physics_indices={};
    std::array<unsigned,world_living::actor_limit> living_indices={},living_actor_indices={};
    std::array<world_physics::Event,world_physics::event_limit> physics_events={};
    std::shared_ptr<const void> identity=std::make_shared<const unsigned char>(0);
    std::uint64_t signature=0;Stats stats;
    world_living::World &living_world(){return configuration.external_living?*configuration.external_living:living;}
    const world_living::World &living_world()const{return configuration.external_living?*configuration.external_living:living;}
    bool own()const{return owner==std::this_thread::get_id();}
    int entity_index(Id id)const{for(unsigned i=0;i<count;++i)if(entities[i].id==id)return int(i);return -1;}
    bool physics_owned(Id id)const{for(const auto &b:configuration.components.physics)if(b.entity==id)return true;return false;}
    bool living_bindings_valid()const{
        const auto &actors=living_world().definition().actors;
        for(unsigned i=0;i<configuration.components.living.size();++i)if(living_actor_indices[i]>=actors.size()||actors[living_actor_indices[i]].id!=configuration.components.living[i].actor)return false;
        return true;
    }
    double game_hour()const{return double(living_world().day())*24+living_world().minute()/60.;}
    mutable std::array<world_npc_minds::Duty,world_living::actor_limit> duties={};
    mutable double duty_hour=-1;
    bool publish(Frame &out)const{
        const auto &living=living_world();out.motion_count=0;
        if(!living_bindings_valid())return false;
        const auto hour=game_hour();if(hour!=duty_hour){for(unsigned i=0;i<configuration.components.living.size();i++)duties[i]=minds.duty(configuration.components.living[i].entity,hour);duty_hour=hour;}
        for(unsigned i=0;i<configuration.components.living.size();++i){const auto &b=configuration.components.living[i];if(!entities[living_indices[i]].active)continue;auto activity=living.activity(living_actor_indices[i]);if(activity!=world_living::Activity::Sleep&&activity!=world_living::Activity::Eat){if(duties[i]==world_npc_minds::Duty::Guard)activity=world_living::Activity::Guard;else if(duties[i]==world_npc_minds::Duty::Work)activity=world_living::Activity::Work;else if(duties[i]==world_npc_minds::Duty::Rest)activity=world_living::Activity::Leisure;}Vec target=b.leisure;switch(activity){case world_living::Activity::Sleep:case world_living::Activity::Eat:target=b.home;break;case world_living::Activity::Commute:{const auto &actor=living.definition().actors[living_actor_indices[i]];target=minutes_inside(living.minute(),actor.work_end,(actor.work_end+actor.commute_minutes)%1440)?b.home:b.work;break;}case world_living::Activity::Work:target=b.work;break;case world_living::Activity::Guard:target=b.guard;break;case world_living::Activity::Leisure:break;}out.motion[out.motion_count++]={b.entity,b.actor,activity,target,b.speed,b.arrival_radius};}
        out.entity_count=count;std::copy_n(entities.begin(),count,out.entities.begin());return true;
    }
    static bool exists(void *context,Id id){return static_cast<Impl *>(context)->entity_index(id)>=0;}
    static bool position(void *context,Id id,world_gameplay::Vec &out){auto &self=*static_cast<Impl *>(context);const int i=self.entity_index(id);if(i<0)return false;const auto p=self.entities[unsigned(i)].pose.position;out={p.x,p.y,p.z};return true;}
};
struct SnapshotData {
    std::uint64_t signature=0;std::shared_ptr<const void> identity;
    world_physics::Snapshot physics;world_gameplay::Snapshot gameplay;
    world_living::World living;std::array<Entity,entity_limit> entities={};unsigned count=0;
    Stats stats;world_npc_minds::State minds;
};
Runtime::Runtime()=default;Runtime::~Runtime()=default;
bool Runtime::configure(const Configuration &c,std::string &error){
    if(impl_&&!impl_->own()){error="Runtime components used outside their owner thread";return false;}
    if(!validate(c.components,error))return false;
    if(c.entities.size()>entity_limit){error="Runtime scene entity limit exceeded";return false;}
    auto candidate=std::make_unique<Impl>();candidate->configuration=c;candidate->count=unsigned(c.entities.size());candidate->signature=signature(c);
    for(unsigned i=0;i<c.entities.size();++i){const auto &e=c.entities[i];if(!e.id||!pose(e.pose)||!finite(e.scale,1e4)||e.scale<=0||index(c.entities,e.id)!=int(i)){error="Invalid or duplicate persistent runtime scene entity";return false;}candidate->entities[i]=e;}
    if(!c.physics_enabled&&!c.components.physics.empty()){error="Physics bindings require a configured physics document";return false;}
    if(!c.living_enabled&&!c.components.living.empty()){error="Living bindings require a configured living document";return false;}
    for(unsigned i=0;i<c.components.physics.size();++i){const auto &b=c.components.physics[i];const int e=candidate->entity_index(b.entity);if(e<0||index(c.physics.bodies,b.body)<0){error="Physics binding references a missing scene entity or body";return false;}candidate->physics_indices[i]=unsigned(e);}
    const auto &living=c.external_living?*c.external_living:c.living;
    for(unsigned i=0;i<c.components.living.size();++i){const auto &b=c.components.living[i];const int e=candidate->entity_index(b.entity),a=index(living.definition().actors,b.actor);if(e<0||a<0){error="Living binding references a missing scene entity or actor";return false;}candidate->living_indices[i]=unsigned(e);candidate->living_actor_indices[i]=unsigned(a);}
    if(!candidate->minds.configure(c.minds,error))return false;
    for(const auto &binding:c.minds){if(candidate->entity_index(binding.entity)<0){error="NPC memory references a missing scene entity";return false;}
        if(binding.memory.profile.actor_id){bool matched=false;for(const auto &living_binding:c.components.living)matched|=living_binding.entity==binding.entity&&living_binding.actor==binding.memory.profile.actor_id;if(!matched){error="NPC profile actor ID does not match the explicit living binding";return false;}}}
    if(c.gameplay_enabled){
        candidate->gameplay.reset(world_gameplay::create());if(!candidate->gameplay||!world_gameplay::configure(candidate->gameplay.get(),c.gameplay,error,{candidate.get(),Impl::exists,Impl::position,true},c.gameplay_budget))return false;
        const auto state=world_gameplay::snapshot(candidate->gameplay.get());for(unsigned i=0;i<state.entity_count;++i){const int e=candidate->entity_index(state.entities[i].id);candidate->entities[unsigned(e)].active=state.entities[i].value!=0&&c.entities[unsigned(e)].active;}
    }
    if(c.physics_enabled){candidate->physics=std::make_unique<world_physics::World>();if(!candidate->physics->configure(c.physics,error))return false;for(unsigned i=0;i<c.components.physics.size();++i){world_physics::BodyState state;if(!candidate->physics->body_state(c.components.physics[i].body,state)){error="Configured physics binding is unavailable";return false;}auto &entity=candidate->entities[candidate->physics_indices[i]];entity.pose=compose(state.pose,c.components.physics[i].body_to_entity);if(!candidate->physics->set_enabled(c.components.physics[i].body,entity.active)){error="Cannot apply initial physics activation";return false;}}}
    if(c.living_enabled){if(!world_living::valid(living.definition(),error))return false;candidate->living=c.living;}
    impl_=std::move(candidate);error.clear();return true;
}
bool Runtime::can_rebind_scene(const std::vector<Entity> &entities,std::string &error)const{
    if(!configured()){error="Cannot refresh an unconfigured runtime scene";return false;}
    if(entities.size()>entity_limit){error="Runtime scene entity limit exceeded";return false;}
    for(unsigned i=0;i<entities.size();++i){const auto &entity=entities[i];if(!entity.id||!pose(entity.pose)||!finite(entity.scale,1e4)||entity.scale<=0||index(entities,entity.id)!=int(i)){error="Invalid or duplicate persistent runtime scene entity";return false;}}
    const auto &old=*impl_;for(const auto &binding:old.configuration.components.physics){const int previous=old.entity_index(binding.entity),next=index(entities,binding.entity);if(next<0||previous<0||std::abs(entities[unsigned(next)].scale-old.entities[unsigned(previous)].scale)>1e-8){error="Referenced physics entity is missing or its scale changed; explicitly reconfigure its collider";return false;}}
    for(const auto &binding:old.configuration.components.living)if(index(entities,binding.entity)<0){error="A configured logical actor references a removed scene entity";return false;}
    if(old.configuration.gameplay_enabled)for(const auto &entity:old.configuration.gameplay.entities)if(index(entities,entity.id)<0){error="Configured gameplay references a removed scene entity";return false;}
    error.clear();return true;
}
bool Runtime::rebind_scene(const std::vector<Entity> &entities,std::string &error){
    if(!can_rebind_scene(entities,error))return false;
    const auto &old=*impl_;
    world_physics::Snapshot physics;world_gameplay::Snapshot gameplay;if(old.physics&&!old.physics->snapshot(physics)){error="Cannot preserve physics during graph refresh";return false;}if(old.gameplay)gameplay=world_gameplay::snapshot(old.gameplay.get());
    Configuration c=old.configuration;c.entities=entities;Runtime candidate;if(!candidate.configure(c,error))return false;auto &next=*candidate.impl_;
    if(next.physics&&!next.physics->restore(physics,error))return false;
    if(next.gameplay&&!world_gameplay::restore(next.gameplay.get(),gameplay,error))return false;
    next.living=old.living;next.stats=old.stats;if(!next.minds.restore(old.minds.state(),error))return false;
    for(unsigned i=0;i<next.count;++i){const int prior=old.entity_index(next.entities[i].id);if(prior<0)continue;bool moving=old.physics_owned(next.entities[i].id);for(const auto &binding:old.configuration.components.living)if(binding.entity==next.entities[i].id)moving=true;
        if(moving)next.entities[i].pose=old.entities[unsigned(prior)].pose;
        // The old visible flag is already authored-mask AND game-state. It
        // cannot recover the game flag when authoring unhides an entity.
        // Read that independent state from the preserved gameplay snapshot.
        if(next.gameplay)for(unsigned j=0;j<gameplay.entity_count;++j)if(gameplay.entities[j].id==next.entities[i].id)next.entities[i].active=c.entities[i].active&&gameplay.entities[j].value!=0;
    }
    if(next.physics)for(unsigned i=0;i<c.components.physics.size();++i)if(!next.physics->set_enabled(c.components.physics[i].body,next.entities[next.physics_indices[i]].active)){error="Cannot refresh physics activation";return false;}
    impl_=std::move(candidate.impl_);error.clear();return true;
}
bool Runtime::configured()const{return impl_&&impl_->own();}
bool Runtime::tick(double dt,double wall,Vec player,Frame &out){
    if(!configured()||!std::isfinite(dt)||dt<0||dt>.25||!clock_ok(wall)||!vec(player)||!impl_->living_bindings_valid())return false;
    auto &s=*impl_;out.entity_count=out.motion_count=out.physics_events=out.living_steps=0;out.requests.count=0;out.physics_event_data=nullptr;out.gameplay_result=world_gameplay::Result::NotConfigured;
    if(s.physics){if(!s.physics->step(dt))return false;for(unsigned i=0;i<s.configuration.components.physics.size();++i){world_physics::BodyState state;s.physics->body_state(s.configuration.components.physics[i].body,state);s.entities[s.physics_indices[i]].pose=compose(state.pose,s.configuration.components.physics[i].body_to_entity);}out.physics_events=s.physics->drain_events(s.physics_events.data(),unsigned(s.physics_events.size()));out.physics_event_data=s.physics_events.data();s.stats.physics_events+=out.physics_events;}
    auto &living=s.living_world();
    if(s.configuration.living_enabled&&s.configuration.drive_living_clock){std::string error;if(!living.observe_clock(wall,error))return false;const auto result=living.process(4);out.living_steps=result.steps;s.stats.living_pending_steps=result.pending_steps;}
    if(s.gameplay){out.gameplay_result=world_gameplay::tick(s.gameplay.get(),dt,{player.x,player.y,player.z},out.requests);if(out.gameplay_result==world_gameplay::Result::Ok){for(unsigned i=0;i<out.requests.count;++i){const auto &r=out.requests.values[i];if(r.kind==world_gameplay::CommandKind::ActivateEntity){const int e=s.entity_index(r.entity);if(e>=0){s.entities[unsigned(e)].active=r.value!=0&&s.configuration.entities[unsigned(e)].active;if(s.physics)for(const auto &b:s.configuration.components.physics)if(b.entity==r.entity)s.physics->set_enabled(b.body,s.entities[unsigned(e)].active);}}}}else ++s.stats.failed_gameplay_ticks;}
    if(out.gameplay_result==world_gameplay::Result::Ok){
        const double hour=s.game_hour();
        for(unsigned i=0;i<out.requests.count;i++){const auto &command=out.requests.values[i];
            if(command.kind==world_gameplay::CommandKind::InteractionCommitted)s.minds.observe(command.entity,world_npc_minds::Event::Meeting,0,hour);
            if(command.kind==world_gameplay::CommandKind::ChoiceCommitted)s.minds.observe(command.entity,world_npc_minds::Event::Choice,command.node^(Id(unsigned(command.value))<<56),hour);
            if(command.kind==world_gameplay::CommandKind::EncounterChanged&&(command.value==int(world_gameplay::Progress::Complete)||command.value==int(world_gameplay::Progress::Failed))){
                Entity arena;if(entity(command.entity,arena))for(const auto &mind:s.configuration.minds){Entity witness;if(entity(mind.entity,witness)&&std::hypot(witness.pose.position.x-arena.pose.position.x,witness.pose.position.z-arena.pose.position.z)<20)s.minds.observe(mind.entity,command.value==int(world_gameplay::Progress::Complete)?world_npc_minds::Event::TrainingWon:world_npc_minds::Event::TrainingLost,command.key,hour);}}
            if(command.kind==world_gameplay::CommandKind::QuestChanged&&command.value==int(world_gameplay::Progress::Complete))for(const auto &mind:s.configuration.minds){Entity witness;if(entity(mind.entity,witness)&&std::hypot(witness.pose.position.x-player.x,witness.pose.position.z-player.z)<6)s.minds.observe(mind.entity,world_npc_minds::Event::Report,command.key,hour);}
        }
    }
    if(!s.publish(out))return false;
    ++s.stats.frames;return true;
}
bool Runtime::read_frame(Frame &out)const{if(!configured())return false;out.requests.count=0;out.physics_events=out.living_steps=0;out.physics_event_data=nullptr;out.gameplay_result=world_gameplay::Result::NotConfigured;return impl_->publish(out);}
bool Runtime::entity(Id id,Entity &out)const{if(!configured())return false;const int i=impl_->entity_index(id);if(i<0)return false;out=impl_->entities[unsigned(i)];return true;}
bool Runtime::set_scene_pose(Id id,const Pose &p){if(!configured()||!pose(p)||impl_->physics_owned(id))return false;const int i=impl_->entity_index(id);if(i<0)return false;impl_->entities[unsigned(i)].pose=p;return true;}
bool Runtime::enqueue(world_gameplay::Event e){return configured()&&impl_->gameplay&&world_gameplay::enqueue(impl_->gameplay.get(),e);}
bool Runtime::interact_nearest(Vec player,double distance,Id &selected){if(!configured()||!impl_->gameplay||!vec(player)||!finite(distance,100)||distance<=0)return false;Id best=0;double squared=distance*distance;for(const auto &v:impl_->configuration.gameplay.entities){if(std::none_of(impl_->configuration.gameplay.rules.begin(),impl_->configuration.gameplay.rules.end(),[&](const auto &rule){return rule.event==world_gameplay::EventKind::Interact&&(!rule.entity||rule.entity==v.id);}))continue;const int i=impl_->entity_index(v.id);if(i<0||!impl_->entities[unsigned(i)].active)continue;const auto p=impl_->entities[unsigned(i)].pose.position;const double d=(p.x-player.x)*(p.x-player.x)+(p.y-player.y)*(p.y-player.y)+(p.z-player.z)*(p.z-player.z);if(d<squared||(d==squared&&(!best||v.id<best))){best=v.id;squared=d;}}if(!best||!enqueue({world_gameplay::EventKind::Interact,best}))return false;selected=best;return true;}
bool Runtime::choose(unsigned i){return configured()&&impl_->gameplay&&world_gameplay::choose(impl_->gameplay.get(),i);}
bool Runtime::close_dialogue(){return configured()&&impl_->gameplay&&world_gameplay::close_dialogue(impl_->gameplay.get());}
const world_gameplay::Line *Runtime::current_line()const{return configured()&&impl_->gameplay?world_gameplay::current_line(impl_->gameplay.get()):nullptr;}
world_gameplay::Choices Runtime::choices()const{return configured()&&impl_->gameplay?world_gameplay::choices(impl_->gameplay.get()):world_gameplay::Choices{};}
bool Runtime::impulse_body(world_physics::Id id,Vec impulse){return configured()&&impl_->physics&&impl_->physics->impulse(id,impulse);}
bool Runtime::teleport_body(world_physics::Id id,const Pose &p){return configured()&&impl_->physics&&impl_->physics->teleport(id,p);}
bool Runtime::door_target(world_physics::Id id,float angle){return configured()&&impl_->physics&&impl_->physics->door_target(id,angle);}
Id Runtime::scene_entity_for_body(world_physics::Id id)const{if(!configured())return 0;for(const auto &b:impl_->configuration.components.physics)if(b.body==id)return b.entity;return 0;}
bool Runtime::raycast(Vec origin,Vec displacement,world_physics::Hit &hit,Id &entity_id,world_physics::Id ignore,unsigned layer)const{if(!configured()||!impl_->physics||!impl_->physics->raycast(origin,displacement,hit,ignore,layer))return false;entity_id=0;if(hit.hit)for(const auto &b:impl_->configuration.components.physics)if(b.body==hit.body){entity_id=b.entity;break;}return true;}
bool Runtime::cast_capsule(Vec center,float radius,float half_height,Vec displacement,world_physics::Hit &hit,Id &entity_id,world_physics::Id ignore,unsigned layer)const{if(!configured()||!impl_->physics||!impl_->physics->cast_capsule(center,radius,half_height,displacement,hit,ignore,layer))return false;entity_id=hit.hit?scene_entity_for_body(hit.body):0;return true;}
bool Runtime::snapshot(Snapshot &out,std::string &error)const{if(!configured()){error="Runtime components are not configured on this thread";return false;}if(!impl_->living_bindings_valid()){error="Logical actor IDs or ordering changed; configure live component bindings before Play";return false;}auto next=std::make_shared<SnapshotData>();next->signature=impl_->signature;next->identity=impl_->identity;next->count=impl_->count;next->entities=impl_->entities;next->living=impl_->living_world();next->stats=impl_->stats;next->minds=impl_->minds.state();if(impl_->physics&&!impl_->physics->snapshot(next->physics)){error="Cannot snapshot configured physics scene";return false;}if(impl_->gameplay)next->gameplay=world_gameplay::snapshot(impl_->gameplay.get());out.data=std::move(next);error.clear();return true;}
bool Runtime::restore(const Snapshot &snapshot,double wall,std::string &error){
    if(!can_restore(snapshot,wall,error))return false;
    const auto &data=*snapshot.data;
    // Stage using the snapshot's logical definition. The external authoritative
    // world can have been reset since Play began and must not influence either
    // validation or mutation until the entire restoration has succeeded.
    auto configuration=impl_->configuration;auto *external=configuration.external_living;
    configuration.external_living=nullptr;configuration.living=data.living;
    Runtime candidate;if(!candidate.configure(configuration,error))return false;auto &s=*candidate.impl_;s.signature=impl_->signature;s.identity=impl_->identity;
    if(s.physics&&!s.physics->restore(data.physics,error))return false;
    if(s.gameplay&&!world_gameplay::restore(s.gameplay.get(),data.gameplay,error))return false;
    if(!s.minds.restore(data.minds,error))return false;
    s.living=data.living;if(s.configuration.living_enabled&&!s.living.rebase_clock(wall,error))return false;
    s.configuration.external_living=external;
    if(external&&s.configuration.living_enabled)*external=s.living;
    s.entities=data.entities;s.count=data.count;s.stats=data.stats;impl_=std::move(candidate.impl_);error.clear();return true;
}
bool Runtime::can_restore(const Snapshot &snapshot,double wall,std::string &error)const{
    if(!configured()||!snapshot.data||snapshot.data->signature!=impl_->signature||snapshot.data->identity!=impl_->identity||!clock_ok(wall)){error="Play snapshot does not belong to this configured scene or clock";return false;}
    // SnapshotData is immutable and can only be created by snapshot(). This is
    // a referential preflight, not a promise that a later allocation will work.
    error.clear();return true;
}
Stats Runtime::stats()const{if(!configured())return {};auto s=impl_->stats;if(impl_->physics)s.physics=impl_->physics->stats();if(impl_->configuration.living_enabled)s.living=impl_->living_world().stats();return s;}
#include "engine/world_runtime_progress.inc"

}

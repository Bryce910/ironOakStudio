// Included in world_runtime namespace after Runtime::Impl is complete.
namespace {
bool read_progress_file(const char *path,std::string &bytes,std::string &error){
    if(!path||!*path){error="Missing progress filename";return false;}
    int fd=vw_open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0){error="Cannot open progress file";return false;}
    struct stat info;const bool valid=fstat(fd,&info)==0&&S_ISREG(info.st_mode)&&info.st_size>=0&&info.st_size<=4*1024*1024;
    if(!valid){close(fd);error="Progress file is invalid or exceeds 4 MiB";return false;}
    bytes.assign(std::size_t(info.st_size),'\0');std::size_t done=0;while(done<bytes.size()){const auto n=read(fd,bytes.data()+done,bytes.size()-done);if(n<=0)break;done+=std::size_t(n);}close(fd);
    if(done!=bytes.size()||bytes.find('\0')!=std::string::npos){error="Truncated or invalid progress bytes";return false;}return true;
}
bool player_ok(const Runtime::Player &p){return vec(p.feet)&&finite(p.yaw,1e6)&&finite(p.pitch,1.49)&&world_town::valid(p.town);}
bool living_definition_equal(const world_living::World &a,const world_living::World &b,std::string &error){
    world_living::World x,y;std::string xs,ys;
    return x.configure(a.definition(),0,error)&&y.configure(b.definition(),0,error)&&x.encode(xs,error)&&y.encode(ys,error)&&xs==ys;
}
}
world_npc_minds::Recollection Runtime::recollection()const{
    if(!configured()||!impl_->gameplay)return {};
    const auto *line=current_line();if(!line)return {};
    for(const auto &d:impl_->configuration.gameplay.dialogues){for(const auto &l:d.lines)if(l.id==line->id)return impl_->minds.recall(d.speaker,impl_->game_hour());}
    return {};
}
const world_npc_minds::State *Runtime::experiences()const{return configured()?&impl_->minds.state():nullptr;}
world_gameplay::Progress Runtime::quest(Id id)const{return configured()&&impl_->gameplay?world_gameplay::quest(impl_->gameplay.get(),id):world_gameplay::Progress::Inactive;}
int Runtime::counter(Id id)const{return configured()&&impl_->gameplay?world_gameplay::counter(impl_->gameplay.get(),id):0;}
const world_gameplay::Document *Runtime::gameplay_document()const{return configured()&&impl_->gameplay?&impl_->configuration.gameplay:nullptr;}
bool Runtime::bind_memory(Id entity,const world_npc_memory::Document &document,std::string &error){
    if(!configured()||impl_->entity_index(entity)<0){error="Choose an existing persistent scene entity";return false;}
    auto bindings=impl_->configuration.minds;auto at=std::find_if(bindings.begin(),bindings.end(),[&](const auto &b){return b.entity==entity;});
    if(at==bindings.end()){world_npc_minds::Binding b;b.entity=entity;b.context={"greeting","player","trail","training"};bindings.push_back(std::move(b));at=std::prev(bindings.end());}
    if(document.profile.actor_id){bool matches=false;for(const auto &b:impl_->configuration.components.living)matches|=b.entity==entity&&b.actor==document.profile.actor_id;if(!matches){error="Profile actor ID must match this entity's living actor, or be zero for an explicit scene-only binding";return false;}}
    at->memory=document;world_npc_minds::Minds next;if(!next.configure(bindings,error))return false;
    auto state=impl_->minds.state();state.signature=next.state().signature;if(!next.restore(state,error))return false;
    impl_->minds=std::move(next);impl_->configuration.minds=std::move(bindings);impl_->identity=std::make_shared<const unsigned char>(0);impl_->duty_hour=-1;error.clear();return true;
}
bool Runtime::save_minds(const char *path,std::string &error)const{std::string bytes;return encode_minds(bytes,error)&&atomic_write(path,bytes,error);}
bool Runtime::encode_minds(std::string &bytes,std::string &error)const{
    if(!configured()){error="NPC minds are unavailable";return false;}
    std::ostringstream out;out<<"VALWOOD_NPC_MINDS 1 "<<impl_->configuration.minds.size()<<'\n';
    for(const auto &b:impl_->configuration.minds){world_npc_memory::Bank bank;std::string encoded;if(!bank.configure(b.memory,error)||!bank.encode(encoded,error))return false;out<<"BIND "<<b.entity<<' '<<b.context.size();for(const auto &s:b.context)out<<' '<<std::quoted(s);out<<' '<<std::quoted(encoded)<<'\n';}out<<"END\n";
    if(out.str().size()>4*1024*1024){error="NPC mind bindings exceed 4 MiB";return false;}bytes=out.str();return true;
}
bool Runtime::load_minds(const char *path,std::string &error){
    if(!configured()){error="NPC minds are unavailable";return false;}std::string bytes;if(!read_progress_file(path,bytes,error))return false;
    std::istringstream in(bytes);std::string tag;unsigned version=0,count=0;if(!(in>>tag>>version>>count)||tag!="VALWOOD_NPC_MINDS"||version!=1||count>world_npc_minds::mind_limit){error="Invalid NPC mind bindings header";return false;}
    std::vector<world_npc_minds::Binding> bindings;
    for(unsigned i=0;i<count;i++){world_npc_minds::Binding b;unsigned n=0;std::string encoded;if(!(in>>tag>>b.entity>>n)||tag!="BIND"||n>world_npc_memory::query_tag_limit||impl_->entity_index(b.entity)<0){error="Invalid NPC mind scene binding";return false;}b.context.resize(n);for(auto &s:b.context)if(!(in>>std::quoted(s))){error="Truncated NPC mind context";return false;}world_npc_memory::Bank bank;if(!(in>>std::quoted(encoded))||!bank.decode(encoded,error))return false;b.memory=bank.document();if(b.memory.profile.actor_id){bool match=false;for(const auto &lb:impl_->configuration.components.living)match|=lb.entity==b.entity&&lb.actor==b.memory.profile.actor_id;if(!match){error="NPC mind actor ID mismatch";return false;}}bindings.push_back(std::move(b));}
    if(!(in>>tag)||tag!="END"){error="Missing NPC minds end marker";return false;}in>>std::ws;if(!in.eof()){error="Trailing NPC mind bindings data";return false;}
    world_npc_minds::Minds next;if(!next.configure(bindings,error))return false;
    // Preserve observations for retained identities when an author changes a profile.
    auto state=impl_->minds.state();state.signature=next.state().signature;unsigned n=0;for(unsigned i=0;i<state.count;i++)if(next.bound(state.values[i].entity))state.values[n++]=state.values[i];state.count=n;
    if(!next.restore(state,error))return false;
    impl_->minds=std::move(next);impl_->configuration.minds=std::move(bindings);impl_->identity=std::make_shared<const unsigned char>(0);impl_->duty_hour=-1;error.clear();return true;
}
bool Runtime::save_progress(const char *path,const Player &player,std::string &error)const{
    if(!configured()||!player_ok(player)||player.town.spent>counter(721004)){error="Cannot save invalid player/runtime state";return false;}
    const auto &s=*impl_;std::string gameplay,living,minds;
    if(s.gameplay){const auto state=world_gameplay::snapshot(s.gameplay.get());for(unsigned i=0;i<state.encounter_count;i++)if(state.encounters[i].progress==world_gameplay::Progress::Active){error="Finish the active encounter before saving progress";return false;}if(!world_gameplay::encode_state(state,gameplay,error))return false;}
    if(s.configuration.living_enabled&&!s.living_world().encode(living,error))return false;
    if(!s.minds.encode(minds,error))return false;
    auto physics=std::make_unique<world_physics::Snapshot>();if(s.physics&&!s.physics->snapshot(*physics)){error="Cannot snapshot physics";return false;}
    std::ostringstream out;out<<std::setprecision(17)<<"SCENE "<<s.signature<<'\n'<<"PLAYER "<<player.feet.x<<' '<<player.feet.y<<' '<<player.feet.z<<' '<<player.yaw<<' '<<player.pitch<<' '<<player.third_person<<'\n';
    out<<"TOWN "<<player.town.spent<<' '<<player.town.bread<<' '<<player.town.apples<<' '<<player.town.prepared<<' '<<player.town.welcome<<'\n';
    out<<"GAMEPLAY "<<std::quoted(gameplay)<<"\nLIVING "<<std::quoted(living)<<"\nMINDS "<<std::quoted(minds)<<'\n'<<"ENTITIES "<<s.count<<'\n';
    for(unsigned i=0;i<s.count;i++){const auto &e=s.entities[i];out<<"ENTITY "<<e.id<<' ';write_pose(out,e.pose);out<<' '<<e.scale<<' '<<e.active<<'\n';}
    out<<"PHYSICS "<<physics->signature<<' '<<physics->count<<' '<<physics->door_count<<' '<<physics->accumulator<<'\n';
    for(unsigned i=0;i<physics->count;i++){const auto &b=physics->bodies[i];out<<"BODY "<<b.id<<' ';write_pose(out,b.pose);out<<' '<<b.linear_velocity.x<<' '<<b.linear_velocity.y<<' '<<b.linear_velocity.z<<' '<<b.angular_velocity.x<<' '<<b.angular_velocity.y<<' '<<b.angular_velocity.z<<' '<<b.active<<' '<<b.enabled<<'\n';}
    for(unsigned i=0;i<physics->door_count;i++){const auto &d=physics->doors[i];out<<"DOOR "<<d.body<<' '<<d.angle<<' '<<d.target<<'\n';}out<<"END\n";
    const auto body=out.str();if(body.size()>4*1024*1024-128){error="Progress exceeds 4 MiB";return false;}
    return atomic_write(path,"VALWOOD_PROGRESS 2 "+std::to_string(world_npc_minds::hash(body))+"\n"+body,error);
}
bool Runtime::load_progress(const char *path,Player &player,double wall,std::string &error){
    if(!configured()||!clock_ok(wall)){error="Invalid progress loading boundary";return false;}
    std::string bytes;if(!read_progress_file(path,bytes,error))return false;const auto newline=bytes.find('\n');if(newline==std::string::npos||newline>128){error="Missing progress header";return false;}
    std::istringstream header(bytes.substr(0,newline));std::string tag;unsigned version=0;std::uint64_t checksum=0;
    if(!(header>>tag>>version>>checksum)||tag!="VALWOOD_PROGRESS"||(version!=1&&version!=2)||checksum!=world_npc_minds::hash(bytes.substr(newline+1))){error="Unsupported or corrupt progress save";return false;}header>>std::ws;if(!header.eof()){error="Trailing progress header";return false;}
    std::istringstream in(bytes.substr(newline+1));std::uint64_t signature=0;Player p;unsigned third=0;
    if(!(in>>tag>>signature)||tag!="SCENE"||signature!=impl_->signature||!(in>>tag>>p.feet.x>>p.feet.y>>p.feet.z>>p.yaw>>p.pitch>>third)||tag!="PLAYER"||third>1||!player_ok(p)){error="Progress does not match this scene or player bounds";return false;}p.third_person=third!=0;
    if(version>=2&&(!(in>>tag>>p.town.spent>>p.town.bread>>p.town.apples>>p.town.prepared>>p.town.welcome)||tag!="TOWN"||!world_town::valid(p.town))){error="Invalid town inventory";return false;}
    std::string gameplay,living,minds;if(!(in>>tag>>std::quoted(gameplay))||tag!="GAMEPLAY"||!(in>>tag>>std::quoted(living))||tag!="LIVING"||!(in>>tag>>std::quoted(minds))||tag!="MINDS"){error="Missing progress sections";return false;}
    auto configuration=impl_->configuration;auto *external=configuration.external_living;configuration.external_living=nullptr;
    world_living::World saved_living;
    if(configuration.living_enabled){if(!saved_living.decode(living,error)||!living_definition_equal(saved_living,impl_->living_world(),error)){if(error.empty())error="Progress uses another living-world definition";return false;}configuration.living=saved_living;}
    else if(!living.empty()){error="Unexpected living state";return false;}
    Runtime candidate;if(!candidate.configure(configuration,error))return false;auto &next=*candidate.impl_;
    if(next.gameplay){world_gameplay::Snapshot state;if(!world_gameplay::decode_state(gameplay,state,error)||!world_gameplay::restore(next.gameplay.get(),state,error))return false;}
    else if(!gameplay.empty()){error="Unexpected gameplay state";return false;}
    if(!next.minds.decode(minds,error))return false;
    unsigned n=0;if(!(in>>tag>>n)||tag!="ENTITIES"||n!=next.count){error="Progress scene entity count mismatch";return false;}
    for(unsigned i=0;i<n;i++){Entity e;unsigned active=0;if(!(in>>tag>>e.id)||tag!="ENTITY"||!read_pose(in,e.pose)||!(in>>e.scale>>active)||active>1||!pose(e.pose)||e.id!=next.entities[i].id||e.scale!=next.entities[i].scale||(active&&!configuration.entities[i].active)){error="Invalid progress entity";return false;}e.active=active!=0;next.entities[i]=e;}
    auto physics=std::make_unique<world_physics::Snapshot>();if(!(in>>tag>>physics->signature>>physics->count>>physics->door_count>>physics->accumulator)||tag!="PHYSICS"||physics->count>world_physics::body_limit||physics->door_count>world_physics::door_limit){error="Invalid physics progress header";return false;}
    for(unsigned i=0;i<physics->count;i++){auto &b=physics->bodies[i];unsigned active=0,enabled=0;if(!(in>>tag>>b.id)||tag!="BODY"||!read_pose(in,b.pose)||!(in>>b.linear_velocity.x>>b.linear_velocity.y>>b.linear_velocity.z>>b.angular_velocity.x>>b.angular_velocity.y>>b.angular_velocity.z>>active>>enabled)||active>1||enabled>1){error="Invalid physics progress body";return false;}b.active=active!=0;b.enabled=enabled!=0;}
    for(unsigned i=0;i<physics->door_count;i++){auto &d=physics->doors[i];if(!(in>>tag>>d.body>>d.angle>>d.target)||tag!="DOOR"){error="Invalid door progress";return false;}}
    if(!(in>>tag)||tag!="END"){error="Missing progress end marker";return false;}in>>std::ws;if(!in.eof()){error="Trailing progress bytes";return false;}
    if(next.physics){if(!next.physics->restore(*physics,error))return false;
        // Physics is authoritative for bound render pivots and active flags.
        for(unsigned i=0;i<configuration.components.physics.size();i++){world_physics::BodyState b;const auto &binding=configuration.components.physics[i];if(!next.physics->body_state(binding.body,b))return false;const auto expected=compose(b.pose,binding.body_to_entity);auto &e=next.entities[next.physics_indices[i]];if(e.active!=b.enabled||std::abs(e.pose.position.x-expected.position.x)>1e-5||std::abs(e.pose.position.y-expected.position.y)>1e-5||std::abs(e.pose.position.z-expected.position.z)>1e-5){error="Saved physical entity differs from its collider";return false;}e.pose=expected;}}
    else if(physics->count||physics->door_count||physics->signature){error="Unexpected physics state";return false;}
    if(next.gameplay){const auto gs=world_gameplay::snapshot(next.gameplay.get());for(unsigned i=0;i<gs.entity_count;i++){const int at=next.entity_index(gs.entities[i].id);if(at<0||next.entities[unsigned(at)].active!=(gs.entities[i].value!=0&&configuration.entities[unsigned(at)].active)){error="Saved entity activation differs from gameplay";return false;}}}
    if(p.town.spent>candidate.counter(721004)){error="Town spending exceeds earned coin";return false;}
    // Offline progress is observed by the existing bounded scheduler after
    // commit, preserving saved time/debt without an unbounded loading loop.
    next.configuration.external_living=external;next.signature=impl_->signature;
    if(external&&configuration.living_enabled)*external=next.living;
    impl_=std::move(candidate.impl_);player=p;error.clear();return true;
}

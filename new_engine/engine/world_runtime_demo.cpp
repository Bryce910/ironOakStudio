#include "engine/world_runtime_demo.h"
#include "engine/world_presentation.h"
#include <algorithm>
#include <cmath>
namespace world_runtime_demo {
namespace {
using namespace world_runtime;
void marker(Seed &s,Id id,const char *name,Vec p,double yaw=0){
    world_entity::Node node;node.id=id;node.name=name;node.local={p.x,p.y,p.z,yaw,1};s.markers.nodes.push_back(node);
    Entity entity;entity.id=id;entity.pose.position=p;const double angle=yaw*.008726646259971648;entity.pose.rotation={0,float(std::sin(angle)),0,float(std::cos(angle))};s.configuration.entities.push_back(entity);
}
world_gameplay::Entity gameplay_entity(const Seed &s,Id id,const char *name){for(const auto &e:s.configuration.entities)if(e.id==id)return {id,name,true,{e.pose.position.x,e.pose.position.y,e.pose.position.z}};return {};}
world_living::Definition living_definition(){
    world_living::Definition d;d.enabled=true;d.seed=0x56414c57;d.start_minute=540;d.factions={{910001,"Valwood inhabitants",{}}};
    world_living::Settlement castle;castle.id=910001;castle.faction=910001;castle.name="Castle quarter";castle.stock={500,100,50,250};castle.reserve={5000,2000,1000,0};castle.capacity={2000,2000,2000,2000};auto camp=castle;camp.id=910002;camp.name="Wayfarers jungle camp";camp.stock={150,50,20,80};d.settlements={castle,camp};
    const char *names[]={"East watch knight","Courtyard watch knight","Gate watch knight","West watch knight","Wayfarers steward","Jungle trail scout"};
    for(unsigned i=0;i<6;i++){world_living::Actor a;a.id=920001+i;a.faction=910001;a.home=i<4?910001:910002;a.name=names[i];a.guard=i<4||i==5;a.work_resource=i<4?world_living::Resource::Ore:world_living::Resource::Food;d.actors.push_back(a);}return d;
}
}
bool build(const Sites &sites,const std::vector<world_runtime::Entity> &authored,world_living::World &external,bool initialize,double wall,Seed &out,std::string &error){
    const double values[]={sites.castle_x,sites.castle_z,sites.castle_floor,sites.jungle_x,sites.jungle_z,sites.jungle_floor,wall};for(double x:values)if(!std::isfinite(x)||std::abs(x)>1e12){error="Invalid runtime demo site/clock";return false;}if(wall<0){error="Invalid runtime demo clock";return false;}
    Seed s;s.configuration.entities=authored;auto &c=s.configuration;c.physics_enabled=c.gameplay_enabled=c.living_enabled=true;c.components.physics_asset="runtime-demo.physics";c.components.gameplay_asset="runtime-demo.gameplay";c.components.living_asset="runtime-demo.living";c.drive_living_clock=false;c.external_living=&external;
    const auto castle=[&](double x,double z){return Vec{sites.castle_x+x,sites.castle_floor,sites.castle_z+z};};const auto jungle=[&](double x,double z){return Vec{sites.jungle_x+x,sites.jungle_floor,sites.jungle_z+z};};
    marker(s,arena_id,"Castle training arena",castle(-49,-73));marker(s,instructor_id,"Master of arms (existing instructor)",castle(-42.5,-68),-90);marker(s,opponent_id,"Training opponent (existing knight)",castle(-47,-73),-90);
    const char *knight_names[]={"East watch knight","Courtyard watch knight","Gate watch knight","West watch knight"};
    const Vec roots[]={castle(18,-76),castle(6,-82),castle(-6,-76),castle(-18,-84),jungle(-5,4),jungle(6,5)};
    for(unsigned i=0;i<6;i++){const Id id=i<4?castle_actor_first+i:jungle_actor_first+i-4;const char *name=i<4?knight_names[i]:i==4?"Wayfarers steward":"Jungle trail scout";marker(s,id,name,roots[i]);s.visuals.push_back({id,i<4?VisualKind::CastleKnight:i==4?VisualKind::JungleSteward:VisualKind::JungleScout,name,"knight"});}
    // Purchased crate has a bottom-centred pivot and measured bounds. Offset
    // converts Jolt's centre pose to that exact mesh pivot, including rotation.
    constexpr double half_y=.4965408742427826;const Vec crates[]={castle(-24,-48),jungle(7,-4)};
    for(unsigned i=0;i<2;i++){const Id id=i?jungle_crate_id:castle_crate_id;auto root=crates[i];root.y+=.05;marker(s,id,i?"Jungle supply crate":"Castle training supply crate",root);s.visuals.push_back({id,VisualKind::Crate,i?"Jungle supply crate":"Castle supply crate","models/purchased_village_crate_near.glb"});
        world_physics::Body floor;floor.id=700001+i*2;floor.name=i?"Jungle crate support":"Castle crate support";floor.size={2,.25,2};floor.pose.position=crates[i];floor.pose.position.y-=.25;
        world_physics::Body crate;crate.id=floor.id+1;crate.name=i?"Jungle dynamic supply crate":"Castle dynamic supply crate";crate.motion=world_physics::Motion::Dynamic;crate.size={.505768358707428,half_y,.5058863759040833};crate.mass=20;crate.friction=.8f;crate.restitution=.02f;crate.pose.position=root;crate.pose.position.y+=half_y;c.physics.bodies.push_back(floor);c.physics.bodies.push_back(crate);PhysicsBinding binding;binding.entity=id;binding.body=crate.id;binding.body_to_entity.position.y=-half_y;c.components.physics.push_back(binding);
    }
    s.markers.next_id=id_base+0x100;
    for(const auto &m:s.markers.nodes)if(std::count_if(c.entities.begin(),c.entities.end(),[&](const auto &e){return e.id==m.id;})!=1){error="Authored scene conflicts with the reserved runtime demo identity namespace";return false;}
    if(c.entities.size()>entity_limit){error="Authored scene and built-in markers exceed the live entity capacity";return false;}
    if(!world_entity::validate(s.markers,nullptr,error)||!world_physics::validate(c.physics,error))return false;
    auto &g=c.gameplay;g.name="Valwood live systems demo";g.entities={gameplay_entity(s,arena_id,"Training arena"),gameplay_entity(s,instructor_id,"Master of arms"),gameplay_entity(s,opponent_id,"Training opponent"),gameplay_entity(s,jungle_actor_first,"Wayfarers steward")};g.flags={{720001,"Jungle camp visited",false},{720002,"Steward greeted",false}};
    world_gameplay::Encounter encounter;encounter.id=training_encounter_id;encounter.arena=arena_id;encounter.name="Castle knight sparring";encounter.training=true;encounter.actors={opponent_id};g.encounters={encounter};
    world_gameplay::Dialogue instructor;instructor.id=730001;instructor.speaker=instructor_id;instructor.entry=730011;instructor.name="Master of arms";world_gameplay::Line welcome;welcome.id=instructor.entry;welcome.text="A good knight reads the opponent. Choose your angle, keep your guard, and punish a missed swing. Ready for a sparring round?";world_gameplay::Choice begin;begin.text="Begin training";begin.actions={{world_gameplay::ActionKind::StartTraining,training_encounter_id,1}};world_gameplay::Choice later;later.text="I'll return later";welcome.choices={begin,later};instructor.lines={welcome};
    world_gameplay::Dialogue steward;steward.id=730002;steward.speaker=jungle_actor_first;steward.entry=730021;steward.name="Wayfarers steward";world_gameplay::Line greeting;greeting.id=steward.entry;greeting.text="Welcome to the wayfarers' clearing. The watch changes with the day, and our supplies are counted even while you are away. Keep to the trail in the mist.";world_gameplay::Choice thanks;thanks.text="Good to find a friendly fire";thanks.actions={{world_gameplay::ActionKind::SetFlag,720002,1}};greeting.choices={thanks};steward.lines={greeting};g.dialogues={instructor,steward};
    world_gameplay::Rule talk;talk.id=740001;talk.name="Speak to master of arms";talk.entity=instructor_id;talk.actions={{world_gameplay::ActionKind::Speak,instructor.id,1}};auto greet=talk;greet.id=740002;greet.name="Speak to jungle steward";greet.entity=jungle_actor_first;greet.actions={{world_gameplay::ActionKind::Speak,steward.id,1}};g.rules={talk,greet};
    world_gameplay::Trigger entered;entered.id=750001;entered.name="Wayfarers camp arrival";entered.shape=world_gameplay::Shape::Sphere;entered.center={sites.jungle_x,sites.jungle_floor,sites.jungle_z};entered.extent={14,14,14};g.triggers={entered};world_gameplay::Rule arrival;arrival.id=740003;arrival.name="Remember jungle camp arrival";arrival.event=world_gameplay::EventKind::TriggerEnter;arrival.key=entered.id;arrival.once=true;arrival.actions={{world_gameplay::ActionKind::SetFlag,720001,1}};g.rules.push_back(arrival);
    // A short complete demo arc. Training stays repeatable; the reward is
    // gated by the active quest and cannot be farmed by repeating dialogue.
    using CK=world_gameplay::ConditionKind;using AK=world_gameplay::ActionKind;using P=world_gameplay::Progress;
    g.counters={{mastery_counter,"Sparring mastered",0,0,1},{camp_counter,"Trail report received",0,0,1},{return_counter,"Report delivered",0,0,1},{coin_counter,"Purse",0,0,1000000},{renown_counter,"Watch reputation",0,0,1000000}};
    world_gameplay::Quest journey;journey.id=journey_id;journey.name="A place in the watch";
    journey.objectives={{761001,mastery_counter,"Win the three-round sparring test in the west court",1,world_gameplay::EventKind::Objective,0},{761002,camp_counter,"Speak to the jungle steward and collect the trail report",1,world_gameplay::EventKind::Objective,0},{761003,return_counter,"Return to the master of arms with the report",1,world_gameplay::EventKind::Objective,0}};g.quests={journey};
    world_gameplay::Rule accept;accept.id=740010;accept.name="Offer a place in the watch";accept.entity=instructor_id;accept.conditions={{CK::Quest,journey_id,world_gameplay::Compare::Equal,int(P::Inactive)}};accept.actions={{AK::SetQuest,journey_id,int(P::Active)}};g.rules.push_back(accept);
    world_gameplay::Rule mastery;mastery.id=740011;mastery.name="Remember successful sparring";mastery.event=world_gameplay::EventKind::EncounterFinished;mastery.key=training_encounter_id;mastery.conditions={{CK::EventValue,0,world_gameplay::Compare::Equal,1}};mastery.actions={{AK::SetCounter,mastery_counter,1}};g.rules.push_back(mastery);
    auto &instructor_line=g.dialogues[0].lines[0];instructor_line.text+=" The watch needs a steady hand: pass the test, check on our jungle wayfarers, then bring their report back to me.";
    world_gameplay::Choice report;report.text="Deliver the wayfarers' report (+25 coin, +1 watch reputation)";report.conditions={{CK::Quest,journey_id,world_gameplay::Compare::Equal,int(P::Active)},{CK::Counter,camp_counter,world_gameplay::Compare::AtLeast,1},{CK::Counter,return_counter,world_gameplay::Compare::Equal,0}};report.actions={{AK::SetCounter,return_counter,1},{AK::AddCounter,coin_counter,25},{AK::AddCounter,renown_counter,1}};instructor_line.choices.push_back(report);
    auto &steward_line=g.dialogues[1].lines[0];steward_line.text+=" If the master of arms has tested you, carry word that the creek is passable and our supplies are safe.";
    world_gameplay::Choice collect;collect.text="Collect the trail report for the master of arms";collect.conditions={{CK::Quest,journey_id,world_gameplay::Compare::Equal,int(P::Active)},{CK::Counter,mastery_counter,world_gameplay::Compare::AtLeast,1},{CK::Counter,camp_counter,world_gameplay::Compare::Equal,0}};collect.actions={{AK::SetCounter,camp_counter,1}};steward_line.choices.push_back(collect);
    if(!world_gameplay::validate(g,error))return false;
    world_living::World initial=external;if(initialize){if(!external.definition().actors.empty()||!external.definition().settlements.empty()||!external.definition().factions.empty()||external.state().tick){error="Refusing to replace an existing logical world with the demo definition";return false;}if(!initial.configure(living_definition(),wall,error))return false;s.initialized_living=true;}
    if(!world_living::valid(initial.definition(),error))return false;
    const auto &actors=initial.definition().actors;std::vector<unsigned> chosen;chosen.reserve(6);auto unique_name=[&](unsigned i){return std::count_if(actors.begin(),actors.end(),[&](const auto &a){return a.name==actors[i].name;})==1;};auto pick=[&](bool guard){for(unsigned i=0;i<actors.size();i++)if(actors[i].guard==guard&&unique_name(i)&&std::find(chosen.begin(),chosen.end(),i)==chosen.end()){chosen.push_back(i);return true;}for(unsigned i=0;i<actors.size();i++)if(unique_name(i)&&std::find(chosen.begin(),chosen.end(),i)==chosen.end()){chosen.push_back(i);return true;}return false;};
    for(unsigned i=0;i<6&&pick(i<4||i==5);i++){LivingBinding binding;binding.entity=i<4?castle_actor_first+i:jungle_actor_first+i-4;const auto &actor=actors[chosen.back()];binding.actor=actor.id;binding.home=roots[i];binding.work=roots[i];binding.leisure=roots[i];binding.guard=roots[i];if(i<4){binding.home.z+=2;binding.work.x+=3;binding.leisure.x-=2;binding.guard.z-=3;}else{binding.home.z+=2;binding.work.x+=i==4?-2:2;binding.leisure.z+=1;binding.guard.z-=3;}binding.speed=i<4?1.2:1.15;binding.arrival_radius=.4;c.components.living.push_back(binding);s.visuals[i].living_actor=actor.id;s.visuals[i].name=actor.name;if(auto *node=world_entity::find(s.markers,binding.entity))node->name=actor.name;}
    auto memory=[&](Id entity,const char *id,const char *name,const char *role,const char *body,std::vector<std::string> tags,bool guard=false){
        world_npc_minds::Binding binding;binding.entity=entity;binding.context=tags;auto &d=binding.memory;d.profile.npc_id=id;d.profile.name=name;d.profile.role=role;d.profile.faction="Valwood watch";d.profile.summary="A resident of Valwood who remembers the player's visits and witnessed actions.";
        for(const auto &lb:c.components.living)if(lb.entity==entity)d.profile.actor_id=lb.actor;
        world_npc_memory::Memory knowledge;knowledge.id="local_knowledge";knowledge.kind=world_npc_memory::Kind::Knowledge;knowledge.title="What I know";knowledge.body=body;knowledge.tags=tags;knowledge.shareable=true;d.memories.push_back(knowledge);
        if(guard){world_npc_memory::Memory duty;duty.id="watch_duty";duty.kind=world_npc_memory::Kind::Goal;duty.title="Keep watch";duty.body="Keep the courtyard watch post while awake.";duty.tags={"duty","guard"};duty.importance=.8;d.memories.push_back(duty);}
        c.minds.push_back(std::move(binding));
    };
    memory(instructor_id,"master_of_arms","Master of arms","Training instructor","A patient guard beats a hurried sword. Match the attack, parry late, and answer from an open angle.",{"training","player"});
    memory(jungle_actor_first,"wayfarers_steward","Wayfarers steward","Camp steward","The wet trail bends around the pool. Keep to the worn earth, watch the rocky bank, and leave the animals room.",{"trail","player"});
    memory(castle_actor_first,"east_watch","East watch knight","Castle watch","My post keeps the eastern approach clear.",{"greeting","watch"},true);
    if(!world_runtime::validate(c.components,error))return false;
    c.living=initial;
    s.status="Built-in live demo: four castle knights, two jungle residents, instructor/steward dialogue and two physical supply crates. "+std::to_string(c.components.living.size())+" residents use explicit bindings to the authoritative living world.";
    if(s.initialized_living)external=std::move(initial);
    out=std::move(s);error.clear();return true;
}
bool presentation(const Sites &sites,Seed &s,double crossing_floor,std::string &error){
    auto &g=s.configuration.gameplay;
    using CK=world_gameplay::ConditionKind;using AK=world_gameplay::ActionKind;
    using Compare=world_gameplay::Compare;
    const Id arena=id_base+0x40,enemy=id_base+0x41;
    if(!std::isfinite(crossing_floor)){error="Invalid surveyed crossing support";return false;}
    const Vec p{sites.jungle_x+world_presentation::crossing_offset_x,crossing_floor,sites.jungle_z+world_presentation::crossing_offset_z};
    marker(s,arena,"Wayfarers crossing",p);marker(s,enemy,"The oathbreaker",p);
    g.entities.push_back(gameplay_entity(s,arena,"Wayfarers crossing"));
    g.entities.push_back(gameplay_entity(s,enemy,"The oathbreaker"));
    g.counters.push_back({world_presentation::threat_counter,"Crossing secured",0,0,1});
    world_gameplay::Encounter encounter;encounter.id=world_presentation::threat_encounter;encounter.arena=arena;
    encounter.name="The broken oath";encounter.actors={enemy};g.encounters.push_back(encounter);
    world_gameplay::Rule begin;begin.id=740020;begin.name="Challenge the oathbreaker";begin.entity=arena;
    begin.conditions={{CK::Counter,mastery_counter,Compare::AtLeast,1},{CK::Counter,world_presentation::threat_counter,Compare::Equal,0}};
    begin.actions={{AK::StartEncounter,world_presentation::threat_encounter,1}};g.rules.push_back(begin);
    world_gameplay::Rule finish;finish.id=740021;finish.name="The crossing is safe";
    finish.event=world_gameplay::EventKind::EncounterFinished;finish.key=world_presentation::threat_encounter;
    finish.conditions={{CK::EventValue,0,Compare::Equal,1}};
    finish.actions={{AK::SetCounter,world_presentation::threat_counter,1}};g.rules.push_back(finish);
    auto &quest=g.quests.front();quest.name="A place in the watch";
    quest.objectives.insert(quest.objectives.begin()+1,{761004,world_presentation::threat_counter,"Clear the wayfarers' crossing",1,world_gameplay::EventKind::Objective,0});
    auto &captain=g.dialogues[0];captain.name="Master Corvin";
    captain.lines[0].text="The watch is a promise, not a uniform. Show me you can read a blade, then carry our seal to Warden Rowan in the Emerald Wood. A deserter has been troubling the crossing. Bring Rowan's report home, and there will be a place for you here.";
    captain.lines[0].choices[0].text="I'm ready. Begin the watch trial.";
    auto &warden=g.dialogues[1];warden.name="Warden Rowan";
    warden.lines[0].text="Corvin sent you? The deserter took our crossing, but you have given it back. The creek is open, our people are safe, and the stores will last the winter. Tell Corvin the wayfarers remember who stood with us.";
    auto &collect=warden.lines[0].choices.back();collect.text="Take Rowan's sealed report.";
    collect.conditions.push_back({CK::Counter,world_presentation::threat_counter,Compare::AtLeast,1});
    captain.lines[0].choices.back().text="Deliver Rowan's report and take the watch oath.";
    for(auto &e:s.configuration.entities){if(e.id==instructor_id)e.scale=1.07;if(e.id==jungle_actor_first)e.scale=.94;}
    for(auto &mind:s.configuration.minds){if(mind.entity==instructor_id)mind.memory.profile.name="Master Corvin";if(mind.entity==jungle_actor_first)mind.memory.profile.name="Warden Rowan";}
    s.markers.next_id=std::max(s.markers.next_id,id_base+0x100);
    return world_gameplay::validate(g,error);
}
}

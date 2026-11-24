#ifndef VALWOOD_WORLD_GAMEPLAY_H
#define VALWOOD_WORLD_GAMEPLAY_H
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace world_gameplay {
using Id=std::uint64_t;
constexpr unsigned entity_limit=256,value_limit=64,rule_limit=128,trigger_limit=64;
constexpr unsigned quest_limit=32,encounter_limit=16,dialogue_limit=16;
constexpr unsigned queue_limit=128,command_limit=128;
struct Vec {double x=0,y=0,z=0;};
enum class EventKind : unsigned {Interact,TriggerEnter,Kill,Objective,EncounterFinished,DialogueChoice};
enum class Progress : int {Inactive,Active,Complete,Failed};
enum class ConditionKind : unsigned {Flag,Counter,Quest,Encounter,EntityActive,CooldownReady,EventValue};
enum class Compare : unsigned {Equal,NotEqual,Less,AtLeast};
struct Condition {ConditionKind kind=ConditionKind::Flag;Id key=0;Compare compare=Compare::Equal;int value=1;};
enum class ActionKind : unsigned {SetFlag,SetCounter,AddCounter,Speak,ActivateEntity,StartEncounter,StartTraining,SetQuest,SetEncounter,SetCooldown};
struct Action {ActionKind kind=ActionKind::SetFlag;Id key=0;int value=1;};
struct Entity {Id id=0;std::string name;bool active=true;Vec position;};
struct Flag {Id id=0;std::string name;bool initial=false;};
struct Counter {Id id=0;std::string name;int initial=0,minimum=0,maximum=1000000;};
struct Objective {Id id=0,counter=0;std::string text;int required=1;
    EventKind event=EventKind::Objective;Id entity=0;};
// Each objective owns a distinct declared counter; encounter actors belong to
// one definition. Replaying an encounter revives its actors/reset defeat mask.
struct Quest {Id id=0;std::string name;std::vector<Objective> objectives;};
struct Encounter {Id id=0,arena=0;std::string name;bool training=false;std::vector<Id> actors;};
enum class Shape : unsigned {Sphere,Box};
// Center is an offset from entity's resolved position; with entity=0 it is
// world space. Missing position callback uses the declared entity position.
// This first pass supports translation attachments, not rotated/scaled boxes.
struct Trigger {Id id=0,entity=0;std::string name;Shape shape=Shape::Box;Vec center,extent={2,2,2};bool once=true;};
struct Choice {std::string text;Id next=0;std::vector<Condition> conditions;std::vector<Action> actions;};
struct Line {Id id=0;std::string text;std::vector<Choice> choices;};
struct Dialogue {Id id=0,speaker=0,entry=0;std::string name;std::vector<Line> lines;};
struct Rule {Id id=0;std::string name;EventKind event=EventKind::Interact;
    Id entity=0,key=0;bool once=false;double cooldown=0;
    std::vector<Condition> conditions;std::vector<Action> actions;};
struct Document {std::string name="Gameplay";std::vector<Entity> entities;std::vector<Flag> flags;
    std::vector<Counter> counters;std::vector<Quest> quests;std::vector<Encounter> encounters;
    std::vector<Trigger> triggers;std::vector<Dialogue> dialogues;std::vector<Rule> rules;};
struct Bindings {void *context=nullptr;bool(*exists)(void *,Id)=nullptr;bool(*position)(void *,Id,Vec &)=nullptr;bool report_committed=false;};
bool validate(const Document &,std::string &error,Bindings={});
bool save(const char *path,const Document &,std::string &error);
bool load(const char *path,Document &,std::string &error,Bindings={});
enum class CommandKind : unsigned {Speak,EndDialogue,ActivateEntity,StartEncounter,StartTraining,EncounterChanged,QuestChanged,InteractionCommitted,ChoiceCommitted};
// Text borrows the configured immutable document, valid until reconfigure or
// destruction. Host must consume commands before the next loading boundary.
struct Command {CommandKind kind=CommandKind::Speak;Id entity=0,key=0,node=0;int value=0;const char *text=nullptr;};
struct Commands {std::array<Command,command_limit> values={};unsigned count=0;};
struct Event {EventKind kind=EventKind::Interact;Id entity=0,key=0,node=0;int value=1;};
struct Value {Id id=0;int value=0;};
struct RuleState {Id id=0;bool fired=false;double cooldown=0;};
struct TriggerState {Id id=0;bool inside=false,fired=false;};
struct EncounterState {Id id=0;Progress progress=Progress::Inactive;std::uint16_t defeated=0;};
struct Snapshot {std::uint64_t signature=0;Id dialogue=0,line=0;
    unsigned entity_count=0,flag_count=0,counter_count=0,quest_count=0,encounter_count=0,rule_count=0,trigger_count=0;
    std::array<Value,entity_limit> entities={};std::array<Value,value_limit> flags={},counters={};
    std::array<Value,quest_limit> quests={};std::array<EncounterState,encounter_limit> encounters={};
    std::array<RuleState,rule_limit> rules={};std::array<TriggerState,trigger_limit> triggers={};
    unsigned queued_count=0;std::array<Event,queue_limit> queued={};};
bool encode_state(const Snapshot &,std::string &,std::string &error);
bool decode_state(const std::string &,Snapshot &,std::string &error);
bool save_state(const char *path,const Snapshot &,std::string &error);
bool load_state(const char *path,Snapshot &,std::string &error);
struct Budget {unsigned events=32,rule_checks=4096,actions=256,commands=128;};
struct Runtime;
Runtime *create();void destroy(Runtime *);
// Loading-only: validates all entity references against an optional host graph,
// copies the document and commits atomically. State restore is strict to the
// document signature; explicit migration is future work.
bool configure(Runtime *,const Document &,std::string &error,Bindings={},Budget={});
bool restore(Runtime *,const Snapshot &,std::string &error);
// Includes all accepted/deferred events in their consumption order.
Snapshot snapshot(const Runtime *);
// Owner thread only. Enqueue during input/physics, consume at frame boundary.
bool enqueue(Runtime *,Event);
bool choose(Runtime *,unsigned choice_index);
bool close_dialogue(Runtime *);
enum class Result {Ok,NotConfigured,InvalidInput,BudgetExceeded};
// Pure deterministic bounded state transaction. No allocations/IO/eval/network.
// Failed ticks preserve state, queue and output commands. Host commands need
// actual bindings to dialogue UI, entity flags and combat/training systems.
// Accepted input beyond this tick's event quota remains queued for later ticks.
Result tick(Runtime *,double dt,Vec player,Commands &);
struct Choices {std::array<unsigned,8> indices={};unsigned count=0;};
const Line *current_line(const Runtime *);
Choices choices(const Runtime *);
int flag(const Runtime *,Id);int counter(const Runtime *,Id);
Progress quest(const Runtime *,Id);Progress encounter(const Runtime *,Id);
}
#endif

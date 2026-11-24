#ifndef VALWOOD_WORLD_STUDIO_CINEMATICS_H
#define VALWOOD_WORLD_STUDIO_CINEMATICS_H
#include "engine/world_cutscenes.h"
#include "engine/world_project_checkpoint.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <fstream>
#include <cctype>
#include <map>
#include <sstream>
namespace world_studio_cinematics {
struct Entry {world_cutscenes::Sequence sequence;world_cinematics::Document timeline;};
struct State {std::vector<Entry> entries;std::uint64_t selected=0;};
inline world_cutscenes::Document library(const State &s){world_cutscenes::Document d;for(const auto &e:s.entries)d.sequences.push_back(e.sequence);return d;}
inline std::string fingerprint(const State &s){
 std::ostringstream o;o<<std::setprecision(17);std::string text,error;
 world_cutscenes::encode(library(s),text,error);o<<text;
 for(const auto &e:s.entries){const auto &d=e.timeline;o<<std::quoted(d.name)<<' '<<d.duration<<' '<<d.loop<<'\n';
  for(const auto &k:d.camera)o<<"k "<<k.time<<' '<<k.pose.position.x<<' '<<k.pose.position.y<<' '<<k.pose.position.z<<' '<<k.pose.rotation.x<<' '<<k.pose.rotation.y<<' '<<k.pose.rotation.z<<' '<<k.pose.rotation.w<<' '<<k.pose.fov_degrees<<' '<<int(k.curve)<<'\n';
  for(const auto &z:d.zones)o<<"z "<<z.id<<' '<<z.bus<<' '<<std::quoted(z.name)<<' '<<std::quoted(z.asset)<<' '<<int(z.shape)<<' '<<z.priority<<' '<<z.center.x<<' '<<z.center.y<<' '<<z.center.z<<' '<<z.extent.x<<' '<<z.extent.y<<' '<<z.extent.z<<' '<<z.blend<<' '<<z.gain<<' '<<z.dry<<' '<<z.reverb<<'\n';
  for(const auto &c:d.clips)o<<"c "<<c.id<<' '<<c.bus<<' '<<std::quoted(c.asset)<<' '<<c.start<<' '<<c.duration<<' '<<c.source_offset<<' '<<c.gain<<' '<<c.fade_in<<' '<<c.fade_out<<' '<<c.loop<<'\n';
  for(const auto &b:d.envelopes){o<<"b "<<b.bus<<'\n';for(const auto &k:b.keys)o<<k.time<<' '<<k.gain<<' '<<int(k.curve)<<'\n';}
 }return o.str();
}
inline bool valid(const State &s,std::string &error){
 if(!world_cutscenes::validate(library(s),error))return false;
 std::vector<std::string> files;
 for(const auto &e:s.entries){
  if(std::find(files.begin(),files.end(),e.sequence.timeline)!=files.end()){error="Each sequence needs its own timeline file; duplicate the timeline before linking it.";return false;}files.push_back(e.sequence.timeline);
  if(!world_cinematics::validate(e.timeline,error))return false;
  if(e.timeline.camera.empty()||e.timeline.loop){error="A published cutscene needs camera keys and a non-looping timeline.";return false;}
 }
 error.clear();return true;
}
struct Wave {double duration=0;unsigned rate=0;};
inline bool wave_file(const std::string &project,const std::string &asset,Wave &out,std::string &error){
 namespace fs=std::filesystem;const fs::path relative(asset);if(asset.empty()||relative.is_absolute()||asset.find('\\')!=std::string::npos||std::any_of(relative.begin(),relative.end(),[](const auto &p){return p=="..";})){error="Narration WAV must be a project-relative path.";return false;}
 std::string extension=relative.extension().string();std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return char(std::tolower(c));});if(extension!=".wav"){error="Import compressed audio first; narration playback requires a PCM WAV asset.";return false;}
 std::error_code ec;const auto root=fs::canonical(project,ec);if(ec){error="Project folder is unavailable.";return false;}const auto path=fs::canonical(root/relative,ec);if(ec||!fs::is_regular_file(path,ec)||ec){error="Narration WAV is missing: "+asset;return false;}const auto confined=path.lexically_relative(root);if(confined.empty()||confined.is_absolute()||std::any_of(confined.begin(),confined.end(),[](const auto&p){return p=="..";})){error="Narration WAV resolves outside this project.";return false;}
 const auto size=fs::file_size(path,ec);if(ec||size<44||size>64u*1024u*1024u){error="Narration WAV is empty, truncated or exceeds the64MiB source-file limit.";return false;}std::ifstream file(path,std::ios::binary);auto u16=[](const unsigned char*p){return unsigned(p[0])|(unsigned(p[1])<<8);};auto u32=[](const unsigned char*p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);};unsigned char header[12];if(!file.read(reinterpret_cast<char*>(header),12)||std::string(reinterpret_cast<char*>(header),4)!="RIFF"||std::string(reinterpret_cast<char*>(header+8),4)!="WAVE"||std::uint64_t(u32(header+4))+8!=size){error="Narration WAV has an invalidRIFF header.";return false;}
 bool format=false,data=false;unsigned rate=0,alignment=0;std::uint64_t samples=0,offset=12;
 while(offset+8<=size){unsigned char chunk[8];file.seekg(std::streamoff(offset));if(!file.read(reinterpret_cast<char*>(chunk),8)){error="Unreadable WAV chunk.";return false;}const auto count=u32(chunk+4);if(offset+8+count>size){error="Truncated WAV chunk.";return false;}const std::string tag(reinterpret_cast<char*>(chunk),4);
  if(tag=="fmt "){unsigned char f[16];if(format||count<16||!file.read(reinterpret_cast<char*>(f),16)){error="Invalid WAV format chunk.";return false;}const auto kind=u16(f),channels=u16(f+2),bits=u16(f+14);rate=u32(f+4);alignment=u16(f+12);if(!((kind==1&&bits==16)||(kind==3&&bits==32))||channels<1||channels>2||rate<8000||rate>96000||alignment!=channels*bits/8||u32(f+8)!=rate*alignment){error="WAV must be PCM16/float32, mono/stereo,8–96kHz.";return false;}format=true;}
  else if(tag=="data"){if(data||!count){error="Invalid WAV data chunk.";return false;}data=true;samples=count;}
  offset+=8+count+(count&1u);
 }
 if(!format||!data||!alignment||samples%alignment||offset!=size){error="WAV format/data chunks are incomplete or misaligned.";return false;}out={double(samples/alignment)/rate,rate};return true;
}
inline bool audio_preflight(const std::string &project,const State &state,std::string &error){
 std::map<std::string,Wave> assets;auto asset=[&](const std::string &name)->bool{if(assets.count(name))return true;if(assets.size()>=32){error="Published cinematics exceed the32 unique audio-asset limit.";return false;}Wave wave;if(!wave_file(project,name,wave,error))return false;assets.emplace(name,wave);return true;};
 for(const auto&e:state.entries){for(const auto&z:e.timeline.zones)if(!asset(z.asset))return false;for(const auto&c:e.timeline.clips){if(!asset(c.asset))return false;const auto &wave=assets.at(c.asset);if(c.source_offset>=wave.duration||(!c.loop&&c.source_offset+c.duration>wave.duration+1./wave.rate)){error="Narration cue extends beyond its WAV source. Adjust duration or source offset: "+c.asset;return false;}}}return true;
}
class Model {
 State state_;std::vector<State> back_,forward_;std::string saved_,project_;world_project::Files guards_;bool loaded_=false,gesture_=false,dirty_=false;
 std::uint64_t issued_=0;
 static constexpr const char *checkpoint="cinematics/world.vcutsproject";
 static bool same(const world_project::File &a,const world_project::File &b){return a.exists==b.exists&&(!a.exists||a.bytes==b.bytes);}
 public:
 const State &state()const{return state_;}bool loaded()const{return loaded_;}bool dirty()const{return loaded_&&dirty_;}
 bool can_undo()const{return !back_.empty();}bool can_redo()const{return !forward_.empty();}unsigned undo_count()const{return unsigned(back_.size());}
 const Entry *selected()const{auto i=std::find_if(state_.entries.begin(),state_.entries.end(),[&](const auto &e){return e.sequence.id==state_.selected;});return i==state_.entries.end()?nullptr:&*i;}
 void end_gesture(){gesture_=false;}
 bool select(std::uint64_t id){if(std::none_of(state_.entries.begin(),state_.entries.end(),[&](const auto&e){return e.sequence.id==id;}))return false;state_.selected=id;gesture_=false;return true;}
 bool edit(State next,std::string &error,bool merge=false){if(!valid(next,error))return false;const auto token=fingerprint(next);if(token==fingerprint(state_)){state_.selected=next.selected;return true;}if(!merge||!gesture_){if(back_.size()==32)back_.erase(back_.begin());back_.push_back(state_);}forward_.clear();state_=std::move(next);gesture_=merge;dirty_=token!=saved_;return true;}
 bool undo(){if(back_.empty())return false;forward_.push_back(state_);state_=std::move(back_.back());back_.pop_back();gesture_=false;dirty_=fingerprint(state_)!=saved_;return true;}
 bool redo(){if(forward_.empty())return false;if(back_.size()==32)back_.erase(back_.begin());back_.push_back(state_);state_=std::move(forward_.back());forward_.pop_back();gesture_=false;dirty_=fingerprint(state_)!=saved_;return true;}
 bool open(const std::string &project,bool discard,std::string &error){
  if(dirty()&&!discard){error="Cinematic edits are unsaved. Publish them or explicitly discard before reloading.";return false;}
  if(world_project::pending(project,checkpoint)){error="An interrupted cinematic publish needs recovery before editing.";return false;}
  State next;world_cutscenes::Document d;const auto path=std::filesystem::path(project)/"cinematics/world.vcuts";
  std::error_code ec;const bool exists=std::filesystem::exists(path,ec);if(ec){error=ec.message();return false;}
  if(exists){world_cutscenes::PreparedLibrary prepared;if(!world_cutscenes::load(path.u8string().c_str(),d,error)||!world_cutscenes::prepare(project.c_str(),d,prepared,error))return false;for(auto &p:prepared.sequences)next.entries.push_back({std::move(p.sequence),std::move(p.timeline)});}
  if(!valid(next,error))return false;std::vector<std::string> names={"cinematics/world.vcuts"};for(const auto &e:next.entries)names.push_back(e.sequence.timeline);
  world_project::Files guards;if(!world_project::read(project,names,guards,error))return false;
  if(!next.entries.empty())next.selected=next.entries.front().sequence.id;
  state_=std::move(next);project_=project;guards_=std::move(guards);back_.clear();forward_.clear();gesture_=false;saved_=fingerprint(state_);dirty_=false;loaded_=true;
  issued_=std::max(issued_,std::uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count()));for(const auto&e:state_.entries)issued_=std::max(issued_,e.sequence.id);error.clear();return true;
 }
 bool create(world_cinematics::CameraPose pose,std::string &error){
  if(!loaded_){error="Load the cinematic library first.";return false;}if(issued_==std::numeric_limits<std::uint64_t>::max()){error="No cinematic IDs remain.";return false;}State next=state_;Entry e;e.sequence.id=++issued_;e.sequence.name="Cutscene "+std::to_string(next.entries.size()+1);e.sequence.timeline="cinematics/sequence-"+std::to_string(e.sequence.id)+".vct";e.timeline.name=e.sequence.name;e.timeline.duration=20;e.timeline.camera.push_back({0,pose,world_cinematics::Curve::Smooth});next.selected=e.sequence.id;next.entries.push_back(e);return edit(std::move(next),error);
 }
 bool erase(std::uint64_t id,std::string &error){State next=state_;auto it=std::find_if(next.entries.begin(),next.entries.end(),[&](const auto&e){return e.sequence.id==id;});if(it==next.entries.end()){error="Select a sequence.";return false;}next.entries.erase(it);next.selected=next.entries.empty()?0:next.entries.front().sequence.id;return edit(std::move(next),error);}
 bool load_timeline(const std::string &relative,std::string &error){
  const auto *current=selected();if(!current){error="Select a sequence.";return false;}State next=state_;auto &entry=*std::find_if(next.entries.begin(),next.entries.end(),[&](const auto&e){return e.sequence.id==next.selected;});entry.sequence.timeline=relative;
  if(!world_cutscenes::validate(library(next),error))return false;
  world_cutscenes::PreparedLibrary prepared;if(!world_cutscenes::prepare(project_.c_str(),world_cutscenes::Document{{entry.sequence}},prepared,error))return false;
  entry.timeline=std::move(prepared.sequences[0].timeline);world_project::Files guard;if(!world_project::read(project_,{relative},guard,error))return false;
  if(!edit(std::move(next),error))return false;auto old=std::find_if(guards_.begin(),guards_.end(),[&](const auto&g){return g.path==relative;});if(old==guards_.end())guards_.push_back(std::move(guard[0]));else *old=std::move(guard[0]);return true;
 }
 bool publish(std::string &error){
  if(!loaded_||!valid(state_,error)){if(!loaded_)error="Load the cinematic library first.";return false;}
  if(world_project::pending(project_,checkpoint)){error="Recover the interrupted cinematic publish first.";return false;}
  if(!audio_preflight(project_,state_,error))return false;
  namespace fs=std::filesystem;std::error_code ec;const auto stage=fs::path(project_)/(".cinematic-stage-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  if(!fs::create_directory(stage,ec)){error="Cannot create cinematic staging folder: "+ec.message();return false;}
  struct Cleanup{fs::path p;~Cleanup(){std::error_code e;fs::remove_all(p,e);}}cleanup{stage};
  std::vector<std::string> names={"cinematics/world.vcuts"};for(const auto&e:state_.entries)names.push_back(e.sequence.timeline);
  world_project::Files before;if(!world_project::read(project_,names,before,error))return false;
  for(const auto &file:before){auto old=std::find_if(guards_.begin(),guards_.end(),[&](const auto&g){return g.path==file.path;});if(old!=guards_.end()){if(!same(*old,file)){error="Cinematic files changed outside the editor. Reload after preserving your draft.";return false;}}else if(file.exists){error="The timeline filename already exists. Load it explicitly or choose a new file.";return false;}}
  for(const auto&e:state_.entries){auto path=stage/e.sequence.timeline;fs::create_directories(path.parent_path(),ec);if(ec){error=ec.message();return false;}if(!world_cinematics::save(path.u8string().c_str(),e.timeline,error))return false;}
  fs::create_directories(stage/"cinematics",ec);if(ec){error=ec.message();return false;}
  if(!world_cutscenes::save((stage/"cinematics/world.vcuts").u8string().c_str(),library(state_),error))return false;
  world_cutscenes::PreparedLibrary prepared;if(!world_cutscenes::prepare(stage.u8string().c_str(),library(state_),prepared,error))return false;
  fs::create_directories(fs::path(project_)/"cinematics",ec);if(ec){error=ec.message();return false;}
  world_project::Files after;if(!world_project::read(stage.string(),names,after,error)||!world_project::commit(project_,checkpoint,after,before,error))return false;
  guards_=std::move(after);saved_=fingerprint(state_);dirty_=false;gesture_=false;error.clear();return true;
 }
 bool recover(const std::string &project,bool finish,std::string &error){if(dirty()){error="Preserve the cinematic draft before recovery.";return false;}return world_project::recover(project,checkpoint,finish,error)&&open(project,true,error);}
};
}
#endif

#include "engine/world_cinematics_audio.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <fstream>
#include <memory>
#include <limits>
#include <new>
namespace world_cinematics_audio {
namespace {
constexpr unsigned queue_size=16,delay_size=4096;
constexpr unsigned comb_count=4,allpass_count=2;
std::atomic<bool> adapter_owned{false};
static_assert(std::atomic<unsigned>::is_always_lock_free&&std::atomic<std::uint64_t>::is_always_lock_free&&std::atomic<float>::is_always_lock_free,"Audio callback requires lock-free scalar atomics");
bool range(double v,double lo,double hi){return std::isfinite(v)&&v>=lo&&v<=hi;}
struct Pcm {std::string key;std::vector<Uint8> data;std::uint64_t frames=0;};
struct Bank {Configuration config;std::array<Pcm,asset_limit> assets;unsigned count=0;std::size_t bytes=0,allocated=0;std::uint64_t generation=0;};
struct Desired {const Pcm *pcm=nullptr;std::uint64_t cursor=0,loop_start=0,remaining=0;
 unsigned category=0,id=0,bus=0;float dry=0,send=0,left=1,right=1;bool active=false,loop=false,seek=false;};
struct Command {enum Kind {Frame,Reset,Stop} kind=Frame;std::uint64_t generation=0;std::array<Desired,voice_limit> voices={};};
struct Delay {std::array<float,delay_size> ring={};unsigned length=1,index=0;float damp=0;};
struct Reverb {std::array<std::array<Delay,comb_count+allpass_count>,2> side;
 unsigned quiet=0,tail_frames=0;
 void initialize(int frequency){tail_frames=unsigned(frequency)*12;constexpr unsigned base[]={1116,1188,1277,1356,225,556};for(unsigned channel=0;channel<2;channel++)for(unsigned i=0;i<6;i++)side[channel][i].length=std::min(delay_size,unsigned(std::lround((base[i]+channel*23)*frequency/44100.)));}
 void clear(){quiet=0;for(auto &channel:side)for(auto &d:channel){d.ring.fill(0);d.index=0;d.damp=0;}}
 void sample(float input,float feedback,float damping,float &left,float &right){
 if(input!=0)quiet=tail_frames;else if(!quiet){left=right=0;return;}else if(--quiet==0){clear();left=right=0;return;}
 float result[2]={};for(unsigned channel=0;channel<2;channel++){
  float value=0;for(unsigned i=0;i<comb_count;i++){auto &d=side[channel][i];const float delayed=d.ring[d.index];d.damp=delayed*(1-damping)+d.damp*damping;if(std::abs(d.damp)<1e-20f)d.damp=0;d.ring[d.index]=input+d.damp*feedback;if(++d.index==d.length)d.index=0;value+=delayed*.25f;}
  for(unsigned i=comb_count;i<comb_count+allpass_count;i++){auto &d=side[channel][i];const float delayed=d.ring[d.index],next=delayed-value;const float stored=value+delayed*.5f;d.ring[d.index]=std::abs(stored)<1e-20f?0:stored;if(++d.index==d.length)d.index=0;value=next;}result[channel]=value;
 }left=result[0];right=result[1];}
};
struct Voice {Desired state;std::uint64_t cursor=0,remaining=0,rendered=0;float dry=0,send=0;bool active=false;};
struct AtomicVoice {std::atomic<unsigned> id{0},category{0},active{0};std::atomic<std::uint64_t> start{0},cursor{0},loop_start{0},rendered{0};std::atomic<float> dry{0},send{0};};
float read_sample(const Uint8 *p,Uint16 format){if(format==AUDIO_S16SYS){Sint16 v;std::memcpy(&v,p,2);return v/32768.f;}float v;std::memcpy(&v,p,4);return std::isfinite(v)?v:0;}
void write_sample(Uint8 *p,Uint16 format,float value){if(format==AUDIO_S16SYS){const auto v=Sint16(std::lround(std::clamp(value,-1.f,32767.f/32768.f)*32768.f));std::memcpy(p,&v,2);}else{value=std::clamp(value,-1.f,1.f);std::memcpy(p,&value,4);}}
const Pcm *asset(const Bank &b,const char *key){for(unsigned i=0;i<b.count;i++)if(b.assets[i].key==key)return &b.assets[i];return nullptr;}
unsigned u16(const Uint8 *b){return unsigned(b[0])|(unsigned(b[1])<<8);}
std::uint32_t u32(const Uint8 *b){return std::uint32_t(b[0])|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);}
}
struct Audio {
 Options options;int frequency=0,first=-1;Uint16 format=0;unsigned channels=0,sample_bytes=0,frame_bytes=0;
 std::array<Uint8,2048> silence={};Mix_Chunk carrier={};bool registered=false,owns_registration=false;
 std::unique_ptr<Bank> current,retired;std::uint64_t generation=0;std::array<unsigned,voice_limit> tracked={};bool force_seek=true;
 std::array<Command,queue_size> commands;std::atomic<unsigned> head{0},tail{0};
 std::array<Voice,voice_limit> voices;std::array<AtomicVoice,voice_limit> visible;
 std::array<Reverb,world_cinematics::bus_limit> reverb;
 std::atomic<std::uint64_t> acknowledged{0},callbacks{0},frames{0},starts{0},queue_full{0};float outside_gain=1;
};
namespace {
bool push(Audio &a,const Command &command){const unsigned h=a.head.load(std::memory_order_relaxed);if(h-a.tail.load(std::memory_order_acquire)>=queue_size){a.queue_full.fetch_add(1,std::memory_order_relaxed);return false;}a.commands[h%queue_size]=command;a.head.store(h+1,std::memory_order_release);return true;}
void silence_owned(Audio &a){for(auto &v:a.voices){v.active=false;v.state={};v.dry=v.send=0;}for(auto &r:a.reverb)r.clear();}
void process_commands(Audio &a){unsigned t=a.tail.load(std::memory_order_relaxed);const unsigned end=a.head.load(std::memory_order_acquire);for(;t!=end;t++){
 const auto &command=a.commands[t%queue_size];if(command.kind!=Command::Frame){silence_owned(a);if(command.kind==Command::Reset)a.acknowledged.store(command.generation,std::memory_order_release);continue;}
 for(unsigned i=0;i<a.channels;i++){auto &v=a.voices[i];const auto &d=command.voices[i];const bool start=d.active&&(!v.active||d.seek||v.state.id!=d.id||v.state.category!=d.category||v.state.pcm!=d.pcm);
  if(start){v.cursor=d.cursor;v.rendered=0;v.dry=v.send=0;a.visible[i].start.store(d.cursor,std::memory_order_relaxed);a.starts.fetch_add(1,std::memory_order_relaxed);}v.state=d;v.active=d.active;v.remaining=d.remaining;
 }
 }a.tail.store(t,std::memory_order_release);
}
void SDLCALL callback(int,void *stream,int length,void *context){
 auto &a=*static_cast<Audio *>(context);if(length<=0||unsigned(length)%a.frame_bytes)return;process_commands(a);auto *bytes=static_cast<Uint8 *>(stream);const unsigned count=unsigned(length)/a.frame_bytes;
 for(unsigned frame=0;frame<count;frame++){
  float left=0,right=0;std::array<float,world_cinematics::bus_limit> sends={};
  for(unsigned slot=0;slot<a.channels;slot++){auto &v=a.voices[slot];const auto &d=v.state;if(!v.active||!d.pcm)continue;
   if(v.remaining==0){v.active=false;continue;}if(v.cursor>=d.pcm->frames){if(d.loop&&d.loop_start<d.pcm->frames)v.cursor=d.loop_start+(v.cursor-d.loop_start)%(d.pcm->frames-d.loop_start);else{v.active=false;continue;}}
   const auto *pcm=d.pcm->data.data()+std::size_t(v.cursor)*a.frame_bytes;const float l=read_sample(pcm,a.format),r=read_sample(pcm+a.sample_bytes,a.format);v.dry+=(d.dry-v.dry)*(1.f/128.f);v.send+=(d.send-v.send)*(1.f/128.f);left+=l*v.dry*d.left;right+=r*v.dry*d.right;sends[d.bus]+=(l+r)*.5f*v.send;v.cursor++;v.rendered++;if(v.remaining!=std::numeric_limits<std::uint64_t>::max())v.remaining--;
  }
  for(unsigned bus=0;bus<world_cinematics::bus_limit;bus++){float l=0,r=0;a.reverb[bus].sample(sends[bus],a.options.reverb_feedback,a.options.reverb_damping,l,r);left+=l*a.options.reverb_wet;right+=r*a.options.reverb_wet;}
  left*=a.options.master;right*=a.options.master;if(left!=0||right!=0){auto *out=bytes+std::size_t(frame)*a.frame_bytes;write_sample(out,a.format,read_sample(out,a.format)+left);write_sample(out+a.sample_bytes,a.format,read_sample(out+a.sample_bytes,a.format)+right);}
 }
 for(unsigned i=0;i<a.channels;i++){const auto &v=a.voices[i];auto &s=a.visible[i];s.id.store(v.state.id,std::memory_order_relaxed);s.category.store(v.state.category,std::memory_order_relaxed);s.cursor.store(v.cursor,std::memory_order_relaxed);s.loop_start.store(v.state.loop_start,std::memory_order_relaxed);s.rendered.store(v.rendered,std::memory_order_relaxed);s.dry.store(v.state.dry,std::memory_order_relaxed);s.send.store(v.state.send,std::memory_order_relaxed);s.active.store(v.active,std::memory_order_release);}
 a.frames.fetch_add(count,std::memory_order_relaxed);a.callbacks.fetch_add(1,std::memory_order_release);
}
bool device(const Audio &a,std::string &e){int hz=0,ch=0;Uint16 fmt=0;if(!Mix_QuerySpec(&hz,&fmt,&ch)||hz!=a.frequency||fmt!=a.format||ch!=2){e="Cinematic audio device changed or closed; recreate adapter";return false;}return true;}
bool prepare(Audio &a,const Configuration &config,std::unique_ptr<Bank> &out,std::string &e){
 if(!device(a,e)||!world_cinematics::validate(config.document,e))return false;
 if(config.document.zones.size()>a.options.zone_voices||config.files.size()>asset_limit||config.outside_asset.size()>240||config.outside_bus>=world_cinematics::bus_limit||!range(config.outside_gain,0,4)){e="Zone count, file count or outside mix exceeds adapter limits";return false;}
 for(const auto &file:config.files)if(file.asset.empty()||file.asset.size()>240||file.path.empty()||file.path.size()>4096){e="Invalid bounded WAV asset key/path";return false;}
 for(const auto &c:config.document.clips){unsigned overlap=0;for(const auto &v:config.document.clips)if(v.start<=c.start&&v.start+v.duration>c.start)overlap++;if(overlap>a.options.cue_voices){e="Simultaneous cue count exceeds owned voices";return false;}}
 if(a.retired&&!collect(&a)){e="Previous audio bank is still awaiting the callback; retry loading later";return false;}
 auto b=std::make_unique<Bank>();b->config=config;b->generation=a.generation+1;if(b->generation==0){e="Audio generation exhausted";return false;}
 auto needed=[&](const std::string &key){if(key.empty())return true;for(unsigned i=0;i<b->count;i++)if(b->assets[i].key==key)return true;if(b->count>=asset_limit)return false;b->assets[b->count++].key=key;return true;};
 for(const auto &z:config.document.zones)if(!needed(z.asset)){e="More than32 decoded cinematic assets";return false;}
 for(const auto &c:config.document.clips)if(!needed(c.asset)){e="More than32 decoded cinematic assets";return false;}
 if(!needed(config.outside_asset)){e="Outside bed exceeds decoded asset count";return false;}out=std::move(b);return true;
}
bool finish(Audio &a,std::unique_ptr<Bank> b,std::string &e){
 for(const auto &clip:b->config.document.clips){const auto *p=asset(*b,clip.asset.c_str());if(!p||clip.source_offset*a.frequency>=p->frames){e="Cue source offset is outside its decoded PCM";return false;}}
 Command command;command.kind=Command::Reset;command.generation=b->generation;
 if(!push(a,command)){e="Audio command queue full; previous configuration preserved";return false;}
 a.retired=std::move(a.current);a.current=std::move(b);a.generation=a.current->generation;a.tracked.fill(0);a.force_seek=true;collect(&a);return true;
}
bool load_wav(Audio &a,const std::string &path,std::size_t budget,std::vector<Uint8> &out,std::string &e){
 std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f||f.tellg()<12||f.tellg()>std::streamoff(a.options.pcm_budget+65536)){e="Missing or oversized WAV: "+path;return false;}const std::uint64_t file_size=std::uint64_t(f.tellg());f.seekg(0);Uint8 riff[12];if(!f.read(reinterpret_cast<char *>(riff),12)||std::memcmp(riff,"RIFF",4)||std::memcmp(riff+8,"WAVE",4)){e="Expected RIFF PCM WAV: "+path;return false;}
 const std::uint64_t end=std::uint64_t(u32(riff+4))+8;if(end>file_size||end<12){e="Malformed WAV RIFF length";return false;}unsigned tag=0,channels=0,bits=0,rate=0,alignment=0;std::uint64_t data_offset=0,data_size=0;bool format_seen=false;
 for(std::uint64_t at=12;at+8<=end;){Uint8 h[8];f.seekg(std::streamoff(at));if(!f.read(reinterpret_cast<char *>(h),8)){e="Truncated WAV chunk";return false;}const std::uint64_t size=u32(h+4),next=at+8+size+(size&1);if(next>end){e="WAV chunk exceeds RIFF length";return false;}
  if(!std::memcmp(h,"fmt ",4)){Uint8 fmt[16];if(size<16||size>4096||format_seen||!f.read(reinterpret_cast<char *>(fmt),16)){e="Invalid/duplicate WAV format chunk";return false;}format_seen=true;tag=u16(fmt);channels=u16(fmt+2);rate=u32(fmt+4);alignment=u16(fmt+12);bits=u16(fmt+14);}
  if(!std::memcmp(h,"data",4)){if(data_offset){e="Multiple WAV data chunks unsupported";return false;}data_offset=at+8;data_size=size;}at=next;
 }
 if(!data_offset||data_size==0||channels<1||channels>2||rate<8000||rate>96000||!((tag==1&&bits==16)||(tag==3&&bits==32))||alignment!=channels*(bits/8)||data_size%alignment||data_size>budget){e="WAV must be bounded PCM16/float32 mono/stereo at8..96kHz";return false;}
 SDL_AudioCVT cvt={};if(SDL_BuildAudioCVT(&cvt,tag==1?AUDIO_S16LSB:AUDIO_F32LSB,Uint8(channels),int(rate),a.format,2,a.frequency)<0||cvt.len_mult<1||data_size>budget/std::size_t(cvt.len_mult)||data_size>std::uint64_t(std::numeric_limits<int>::max())){e="Converted WAV exceeds remaining PCM budget or format support";return false;}
 std::vector<Uint8> data(std::size_t(data_size)*std::size_t(cvt.len_mult));f.seekg(std::streamoff(data_offset));if(!f.read(reinterpret_cast<char *>(data.data()),std::streamsize(data_size))){e="Truncated WAV PCM";return false;}
 cvt.buf=data.data();cvt.len=int(data_size);if(cvt.needed&&SDL_ConvertAudio(&cvt)<0){e="SDL PCM conversion failed";return false;}const std::size_t bytes=cvt.needed?std::size_t(cvt.len_cvt):std::size_t(cvt.len);if(bytes==0||bytes>budget||bytes%a.frame_bytes){e="Converted PCM is not device-frame aligned";return false;}data.resize(bytes);out=std::move(data);return true;
}
void pan(const Audio &a,const world_cinematics::CameraPose &camera,world_cinematics::Vec3 position,float &l,float &r){l=r=1;if(!a.options.spatial_pan)return;const auto f=world_cinematics::forward(camera.rotation),u=world_cinematics::up(camera.rotation);const double rx=f.y*u.z-f.z*u.y,ry=f.z*u.x-f.x*u.z,rz=f.x*u.y-f.y*u.x,dx=position.x-camera.position.x,dy=position.y-camera.position.y,dz=position.z-camera.position.z,length=std::sqrt(dx*dx+dy*dy+dz*dz);if(length<.01)return;const double p=std::clamp((rx*dx+ry*dy+rz*dz)/length,-1.,1.)*.72;l=float(std::sqrt(1-p));r=float(std::sqrt(1+p));}
}
Audio *create(const Options &o,std::string &e){
 e.clear();if(o.zone_voices+o.cue_voices+1>voice_limit||o.zone_voices>voice_limit||o.cue_voices>voice_limit||o.pcm_budget<4096||o.pcm_budget>128u*1024u*1024u||!range(o.master,0,1)||!range(o.reverb_feedback,0,.92)||!range(o.reverb_damping,0,1)||!range(o.reverb_wet,0,1)||!range(o.cue_send,0,1)){e="Invalid bounded cinematic audio options";return nullptr;}
 auto a=std::unique_ptr<Audio>(new(std::nothrow)Audio);if(!a){e="Cannot allocate cinematic DSP buffers";return nullptr;}int ch=0;if(!Mix_QuerySpec(&a->frequency,&a->format,&ch)||ch!=2||(a->format!=AUDIO_S16SYS&&a->format!=AUDIO_F32SYS)||a->frequency<8000||a->frequency>96000){e="Cinematic reverb requires an open S16SYS/F32SYS stereo8..96kHz mixer";return nullptr;}
 bool expected=false;if(!adapter_owned.compare_exchange_strong(expected,true)){e="A cinematic audio adapter already owns this mixer";return nullptr;}a->owns_registration=true;
 a->options=o;a->channels=o.zone_voices+o.cue_voices+1;a->sample_bytes=unsigned(SDL_AUDIO_BITSIZE(a->format))/8;a->frame_bytes=a->sample_bytes*2;for(auto &r:a->reverb)r.initialize(a->frequency);
 a->first=Mix_AllocateChannels(-1);if(a->first<0||a->first+int(a->channels)>256||Mix_AllocateChannels(a->first+int(a->channels))!=a->first+int(a->channels)){e="Cannot append cinematic voice channels";adapter_owned.store(false);return nullptr;}
 a->carrier.allocated=0;a->carrier.abuf=a->silence.data();a->carrier.alen=unsigned(a->silence.size());a->carrier.volume=MIX_MAX_VOLUME;
 for(unsigned i=0;i<a->channels;i++){Mix_Volume(a->first+int(i),MIX_MAX_VOLUME);if(Mix_PlayChannel(a->first+int(i),&a->carrier,-1)<0){e="Cannot start cinematic channel carrier";destroy(a.release());return nullptr;}}
 if(!Mix_RegisterEffect(MIX_CHANNEL_POST,callback,nullptr,a.get())){e="Cannot register cinematic DSP effect";destroy(a.release());return nullptr;}a->registered=true;return a.release();
}
void destroy(Audio *a){if(!a)return;if(Mix_QuerySpec(nullptr,nullptr,nullptr)){if(a->registered)Mix_UnregisterEffect(MIX_CHANNEL_POST,callback);for(unsigned i=0;i<a->channels;i++)if(Mix_GetChunk(a->first+int(i))==&a->carrier)Mix_HaltChannel(a->first+int(i));if(a->first>=0&&Mix_AllocateChannels(-1)==a->first+int(a->channels))Mix_AllocateChannels(a->first);}if(a->owns_registration)adapter_owned.store(false);delete a;}
bool configure_pcm(Audio *a,const Configuration &config,const std::vector<PcmAsset> &assets,std::string &e){
 e.clear();if(!a){e="Missing cinematic audio adapter";return false;}try{std::unique_ptr<Bank> b;if(!prepare(*a,config,b,e))return false;if(assets.size()>asset_limit){e="Too many input PCM assets";return false;}
 for(unsigned i=0;i<b->count;i++){const PcmAsset *input=nullptr;for(const auto &p:assets)if(p.asset==b->assets[i].key){if(input){e="Duplicate decoded PCM key";return false;}input=&p;}if(!input||!input->data||input->bytes==0||input->bytes%a->frame_bytes||input->bytes>a->options.pcm_budget-b->allocated){e="Missing, unaligned or over-budget PCM: "+b->assets[i].key;return false;}auto &p=b->assets[i];const auto *data=static_cast<const Uint8 *>(input->data);p.data.assign(data,data+input->bytes);p.frames=p.data.size()/a->frame_bytes;b->bytes+=p.data.size();b->allocated+=p.data.capacity();if(b->allocated>a->options.pcm_budget){e="PCM storage capacity exceeds budget";return false;}}
 return finish(*a,std::move(b),e);}catch(const std::bad_alloc &){e="PCM preparation allocation failed; previous mix preserved";return false;}
}
bool configure_files(Audio *a,const Configuration &config,std::string &e){
 e.clear();if(!a){e="Missing cinematic audio adapter";return false;}try{std::unique_ptr<Bank> b;if(!prepare(*a,config,b,e))return false;if(config.files.size()>asset_limit){e="Too many WAV asset paths";return false;}
 for(unsigned i=0;i<b->count;i++){const FileAsset *input=nullptr;for(const auto &p:config.files)if(p.asset==b->assets[i].key){if(input){e="Duplicate WAV asset key";return false;}input=&p;}if(!input||input->path.empty()||input->path.size()>4096){e="Missing WAV path: "+b->assets[i].key;return false;}auto &p=b->assets[i];if(!load_wav(*a,input->path,a->options.pcm_budget-b->allocated,p.data,e))return false;p.frames=p.data.size()/a->frame_bytes;b->bytes+=p.data.size();b->allocated+=p.data.capacity();if(b->allocated>a->options.pcm_budget){e="Converted PCM storage exceeds budget";return false;}}
 return finish(*a,std::move(b),e);}catch(const std::bad_alloc &){e="WAV preparation allocation failed; previous mix preserved";return false;}
}
UpdateResult update(Audio *a,const world_cinematics::Step &step,const world_cinematics::CameraPose &camera){
 if(!a||!a->current)return UpdateResult::NotConfigured;
 const auto &b=*a->current;world_cinematics::Schedule schedule;world_cinematics::Mix mix;
 if(!world_cinematics::schedule(b.config.document,step,schedule)||!world_cinematics::sample_mix(b.config.document,camera.position,step.time,mix))return UpdateResult::InvalidFrame;
 double norm=0;for(double v:{camera.rotation.x,camera.rotation.y,camera.rotation.z,camera.rotation.w}){if(!std::isfinite(v))return UpdateResult::InvalidFrame;norm+=v*v;}if(std::abs(norm-1)>.02)return UpdateResult::InvalidFrame;
 Command command;command.generation=b.generation;auto tracked=a->tracked;const bool resync=a->force_seek||schedule.resync||step.wraps>0;
 if(step.paused)command.kind=Command::Stop;
 else{
  for(unsigned i=0;i<b.config.document.zones.size();i++){const auto &z=b.config.document.zones[i];auto &d=command.voices[i];d.pcm=asset(b,z.asset.c_str());if(!d.pcm)continue;d.id=z.id;d.category=1;d.bus=z.bus;d.active=true;d.loop=true;d.remaining=std::numeric_limits<std::uint64_t>::max();d.cursor=std::uint64_t(std::floor(step.time*a->frequency))%d.pcm->frames;d.seek=resync;pan(*a,camera,z.center,d.left,d.right);for(unsigned j=0;j<mix.zones.count;j++)if(mix.zones.voices[j].id==z.id){d.dry=mix.zones.voices[j].dry_gain;d.send=mix.zones.voices[j].reverb_send;break;}}
  const unsigned outside=a->options.zone_voices+a->options.cue_voices;if(!b.config.outside_asset.empty()){auto &d=command.voices[outside];d.pcm=asset(b,b.config.outside_asset.c_str());d.active=true;d.id=1;d.category=3;d.bus=b.config.outside_bus;d.loop=true;d.seek=resync;d.remaining=std::numeric_limits<std::uint64_t>::max();d.cursor=std::uint64_t(std::floor(step.time*a->frequency))%d.pcm->frames;d.dry=mix.zones.outside_gain*b.config.outside_gain*mix.buses[d.bus];}
  for(unsigned slot=0;slot<a->options.cue_voices;slot++)if(tracked[slot]&&std::none_of(mix.clips.begin(),mix.clips.begin()+mix.clip_count,[&](const auto &c){return c.id==tracked[slot];}))tracked[slot]=0;
  for(unsigned i=0;i<mix.clip_count;i++){const auto &cue=mix.clips[i];unsigned slot=0;for(;slot<a->options.cue_voices;slot++)if(tracked[slot]==cue.id)break;if(slot==a->options.cue_voices)for(slot=0;slot<a->options.cue_voices;slot++)if(!tracked[slot])break;if(slot==a->options.cue_voices)return UpdateResult::InvalidFrame;const bool fresh=tracked[slot]!=cue.id;tracked[slot]=cue.id;
   const auto at=std::find_if(b.config.document.clips.begin(),b.config.document.clips.end(),[&](const auto &c){return c.id==cue.id;});if(at==b.config.document.clips.end())return UpdateResult::InvalidFrame;auto &d=command.voices[a->options.zone_voices+slot];d.pcm=asset(b,cue.asset);d.id=cue.id;d.category=2;d.bus=cue.bus;d.active=true;d.loop=cue.loop;d.seek=resync||fresh;d.cursor=std::uint64_t(std::floor(cue.source_offset*a->frequency+1e-6));d.loop_start=std::uint64_t(std::floor(at->source_offset*a->frequency+1e-6));if(d.loop&&d.cursor>=d.pcm->frames)d.cursor=d.loop_start+(d.cursor-d.loop_start)%(d.pcm->frames-d.loop_start);d.remaining=std::uint64_t(std::ceil(std::max(0.,at->start+at->duration-step.time)*a->frequency));d.dry=cue.gain;d.send=cue.gain*a->options.cue_send;
  }
 }
 if(!push(*a,command))return UpdateResult::QueueFull;
 a->tracked=tracked;a->force_seek=step.paused;a->outside_gain=mix.zones.outside_gain;return UpdateResult::Ok;
}
bool stop_all(Audio *a){if(!a)return false;Command c;c.kind=Command::Stop;if(!push(*a,c))return false;a->tracked.fill(0);a->force_seek=true;return true;}
bool collect(Audio *a){if(!a)return false;if(a->retired&&a->acknowledged.load(std::memory_order_acquire)>=a->generation)a->retired.reset();return !a->retired;}
Stats stats(const Audio *a){Stats s;if(!a)return s;s.first_channel=a->first;s.channels=a->channels;s.frequency=a->frequency;s.format=a->format;s.generation=a->generation;s.acknowledged_generation=a->acknowledged.load(std::memory_order_acquire);s.callbacks=a->callbacks.load(std::memory_order_acquire);s.rendered_frames=a->frames.load(std::memory_order_relaxed);s.starts=a->starts.load(std::memory_order_relaxed);s.queue_full=a->queue_full.load(std::memory_order_relaxed);s.outside_gain=a->outside_gain;s.retired_bank=bool(a->retired);if(a->current){s.assets=a->current->count;s.decoded_bytes=a->current->bytes;s.allocated_pcm_bytes=a->current->allocated;}for(unsigned i=0;i<a->channels;i++)s.active_voices+=a->visible[i].active.load(std::memory_order_acquire);return s;}
bool voice(const Audio *a,unsigned slot,VoiceState &out){if(!a||slot>=a->channels)return false;const auto &v=a->visible[slot];VoiceState s;s.active=v.active.load(std::memory_order_acquire)!=0;s.id=v.id.load(std::memory_order_relaxed);s.category=v.category.load(std::memory_order_relaxed);s.start_frame=v.start.load(std::memory_order_relaxed);s.cursor_frame=v.cursor.load(std::memory_order_relaxed);s.loop_start_frame=v.loop_start.load(std::memory_order_relaxed);s.rendered_frames=v.rendered.load(std::memory_order_relaxed);s.dry_gain=v.dry.load(std::memory_order_relaxed);s.reverb_send=v.send.load(std::memory_order_relaxed);out=s;return true;}
}

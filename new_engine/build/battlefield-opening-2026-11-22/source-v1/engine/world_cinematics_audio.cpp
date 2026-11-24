#include "engine/world_cinematics_audio.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <fstream>
#include <filesystem>
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
struct Bank {Configuration config;std::array<Pcm,asset_limit> assets;unsigned count=0;std::size_t bytes=0,allocated=0;std::uint64_t generation=0;bool soundscape=false;};
struct Desired {const Pcm *pcm=nullptr;std::uint64_t cursor=0,loop_start=0,remaining=0;
 std::uint64_t id=0,cursor_clock=0;unsigned category=0,bus=0;float dry=0,send=0,left=1,right=1;bool active=false,loop=false,seek=false,clocked=false;};
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
constexpr unsigned world_state_limit=world_soundscape::sound_limit+soundscape_voice_limit;
struct WorldSound {
 std::uint64_t id=0,cursor=0;const Pcm *pcm=nullptr;
 float gain=0,target=0,from=0,left=1,right=1,fade_out=1;double ramp_time=0,ramp_duration=0;
 bool present=false,inside=false,playing=false,loop=true,virtualized=false;
};
struct Voice {Desired state;std::uint64_t cursor=0,remaining=0,rendered=0;float dry=0,send=0,pan_left=1,pan_right=1;bool active=false;};
struct AtomicVoice {std::atomic<std::uint64_t> id{0};std::atomic<unsigned> category{0},active{0};std::atomic<std::uint64_t> start{0},cursor{0},loop_start{0},rendered{0};std::atomic<float> dry{0},send{0};};
float read_sample(const Uint8 *p,Uint16 format){if(format==AUDIO_S16SYS){Sint16 v;std::memcpy(&v,p,2);return v/32768.f;}float v;std::memcpy(&v,p,4);return std::isfinite(v)?v:0;}
void write_sample(Uint8 *p,Uint16 format,float value){if(format==AUDIO_S16SYS){const auto v=Sint16(std::lround(std::clamp(value,-1.f,32767.f/32768.f)*32768.f));std::memcpy(p,&v,2);}else{value=std::clamp(value,-1.f,1.f);std::memcpy(p,&value,4);}}
const Pcm *asset(const Bank &b,const char *key){for(unsigned i=0;i<b.count;i++)if(b.assets[i].key==key)return &b.assets[i];return nullptr;}
unsigned u16(const Uint8 *b){return unsigned(b[0])|(unsigned(b[1])<<8);}
std::uint32_t u32(const Uint8 *b){return std::uint32_t(b[0])|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);}
}
struct Audio {
 Options options;int frequency=0,first=-1;Uint16 format=0;unsigned channels=0,sample_bytes=0,frame_bytes=0;
 std::array<Uint8,2048> silence={};Mix_Chunk carrier={};bool registered=false,owns_registration=false;
 std::unique_ptr<Bank> current,retired;std::uint64_t generation=0;std::array<unsigned,voice_limit> tracked={};bool force_seek=true,shared_library=false,playback_world=false;
 std::array<Command,queue_size> commands;std::atomic<unsigned> head{0},tail{0};
 std::array<Voice,voice_limit> voices;std::array<AtomicVoice,voice_limit> visible;
 std::array<Reverb,world_cinematics::bus_limit> reverb;
 std::array<WorldSound,world_state_limit> world={};std::array<std::uint64_t,soundscape_voice_limit> world_slots={};
 unsigned world_count=0,world_authored=0,world_virtualized=0;std::uint64_t world_clock=0,world_audition=0,world_audition_serial=0;bool world_allowed=false;
 std::atomic<std::uint64_t> acknowledged{0},callbacks{0},frames{0},starts{0},queue_full{0};float outside_gain=1;
};
namespace {
bool push(Audio &a,const Command &command){const unsigned h=a.head.load(std::memory_order_relaxed);if(h-a.tail.load(std::memory_order_acquire)>=queue_size){a.queue_full.fetch_add(1,std::memory_order_relaxed);return false;}a.commands[h%queue_size]=command;a.head.store(h+1,std::memory_order_release);return true;}
void silence_owned(Audio &a){for(auto &v:a.voices){v.active=false;v.state={};v.dry=v.send=0;}for(auto &r:a.reverb)r.clear();}
void process_commands(Audio &a){unsigned t=a.tail.load(std::memory_order_relaxed);const unsigned end=a.head.load(std::memory_order_acquire);for(;t!=end;t++){
 const auto &command=a.commands[t%queue_size];if(command.kind!=Command::Frame){silence_owned(a);if(command.kind==Command::Reset)a.acknowledged.store(command.generation,std::memory_order_release);continue;}
 for(unsigned i=0;i<a.channels;i++){auto &v=a.voices[i];const auto &d=command.voices[i];const bool start=d.active&&(!v.active||d.seek||v.state.id!=d.id||v.state.category!=d.category||v.state.pcm!=d.pcm);
  if(start){v.cursor=d.cursor;if(d.clocked){const auto now=a.frames.load(std::memory_order_relaxed);v.cursor+=now>=d.cursor_clock?now-d.cursor_clock:0;}v.rendered=0;v.dry=v.send=0;v.pan_left=d.left;v.pan_right=d.right;a.visible[i].start.store(d.cursor,std::memory_order_relaxed);a.starts.fetch_add(1,std::memory_order_relaxed);}v.state=d;v.active=d.active;v.remaining=d.remaining;
 }
 }a.tail.store(t,std::memory_order_release);
}
void SDLCALL callback(int,void *stream,int length,void *context){
 auto &a=*static_cast<Audio *>(context);if(length<=0||unsigned(length)%a.frame_bytes)return;process_commands(a);auto *bytes=static_cast<Uint8 *>(stream);const unsigned count=unsigned(length)/a.frame_bytes;
 for(unsigned frame=0;frame<count;frame++){
  float left=0,right=0;std::array<float,world_cinematics::bus_limit> sends={};
  for(unsigned slot=0;slot<a.channels;slot++){auto &v=a.voices[slot];const auto &d=v.state;if(!v.active||!d.pcm)continue;
   if(v.remaining==0){v.active=false;continue;}if(v.cursor>=d.pcm->frames){if(d.loop&&d.loop_start<d.pcm->frames)v.cursor=d.loop_start+(v.cursor-d.loop_start)%(d.pcm->frames-d.loop_start);else{v.active=false;continue;}}
   const auto *pcm=d.pcm->data.data()+std::size_t(v.cursor)*a.frame_bytes;const float l=read_sample(pcm,a.format),r=read_sample(pcm+a.sample_bytes,a.format);v.dry+=(d.dry-v.dry)*(1.f/128.f);v.send+=(d.send-v.send)*(1.f/128.f);if(d.category==4){v.pan_left+=(d.left-v.pan_left)*(1.f/128.f);v.pan_right+=(d.right-v.pan_right)*(1.f/128.f);}left+=l*v.dry*(d.category==4?v.pan_left:d.left);right+=r*v.dry*(d.category==4?v.pan_right:d.right);sends[d.bus]+=(l+r)*.5f*v.send;v.cursor++;v.rendered++;if(v.remaining!=std::numeric_limits<std::uint64_t>::max())v.remaining--;
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
 a.retired=std::move(a.current);a.current=std::move(b);a.generation=a.current->generation;a.tracked.fill(0);a.force_seek=true;a.shared_library=false;a.playback_world=a.current->soundscape;a.world={};a.world_slots={};a.world_count=a.world_authored=a.world_virtualized=0;a.world_clock=a.frames.load(std::memory_order_acquire);a.world_allowed=false;a.world_audition=a.world_audition_serial=0;a.outside_gain=1;collect(&a);return true;
}
bool load_wav(Audio &a,const std::string &path,std::size_t budget,std::vector<Uint8> &out,std::string &e){
 if(!world_soundscape::text(path,4096)){e="WAV path must be bounded valid UTF-8";return false;}
 std::ifstream f(std::filesystem::u8path(path),std::ios::binary|std::ios::ate);if(!f||f.tellg()<12||f.tellg()>std::streamoff(a.options.pcm_budget+65536)){e="Missing or oversized WAV: "+path;return false;}const std::uint64_t file_size=std::uint64_t(f.tellg());f.seekg(0);Uint8 riff[12];if(!f.read(reinterpret_cast<char *>(riff),12)||std::memcmp(riff,"RIFF",4)||std::memcmp(riff+8,"WAVE",4)){e="Expected RIFF PCM WAV: "+path;return false;}
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
namespace {
bool runtime_sound_valid(const world_soundscape::Sound &s){return world_soundscape::valid(s);}

void world_advance(WorldSound &s,std::uint64_t elapsed){
 if(!s.playing||!s.pcm||!s.pcm->frames)return;
 if(s.loop)s.cursor=(s.cursor+elapsed%s.pcm->frames)%s.pcm->frames;
 else{s.cursor+=std::min(elapsed,s.pcm->frames-std::min(s.cursor,s.pcm->frames));if(s.cursor>=s.pcm->frames)s.playing=false;}
}
void world_ramp(WorldSound &s,float target,double seconds,double dt){
 if(target!=s.target||(s.gain!=target&&seconds!=s.ramp_duration)){s.from=s.gain;s.target=target;s.ramp_time=0;s.ramp_duration=seconds;}
 s.ramp_time=std::min(s.ramp_duration,s.ramp_time+dt);
 s.gain=s.ramp_duration>0?float(s.from+(s.target-s.from)*(s.ramp_time/s.ramp_duration)):s.target;
 if(s.ramp_time>=s.ramp_duration)s.gain=s.target;
}
}
bool configure_soundscape_files(Audio *a,const SoundscapeConfiguration &config,std::string &e){
 e.clear();if(!a){e="Missing spatial audio adapter";return false;}
 try{
  if(!device(*a,e))return false;
  if(config.emitters.size()>world_soundscape::sound_limit||config.files.size()>asset_limit){e="World sounds allow64 emitters and32 unique WAV assets";return false;}
  for(unsigned i=0;i<config.emitters.size();++i){const auto &em=config.emitters[i];if(!range(em.condition_gain,0,1)){e="Invalid world sound weather condition gain";return false;}if(!em.id){e="World sound emitter ID must be nonzero";return false;}if(!world_soundscape::valid(em.sound,e))return false;for(unsigned j=0;j<i;++j)if(config.emitters[j].id==em.id){e="Duplicate world sound emitter ID";return false;}}
  for(unsigned i=0;i<config.files.size();++i){const auto &f=config.files[i];if(!world_soundscape::asset_path(f.asset)||!world_soundscape::text(f.path,4096)){e="World sound assets require bounded UTF-8 WAV paths";return false;}for(unsigned j=0;j<i;++j)if(config.files[j].asset==f.asset){e="Duplicate world sound file mapping";return false;}}
  if(a->retired&&!collect(a)){e="Previous decoded audio bank is awaiting the callback; retry preparation";return false;}
  auto b=std::make_unique<Bank>();b->soundscape=true;b->generation=a->generation+1;if(!b->generation){e="Audio generation exhausted";return false;}
  for(const auto &em:config.emitters){const auto &key=em.sound.asset;if(key.empty()||asset(*b,key.c_str()))continue;if(b->count==asset_limit){e="More than32 unique world sound assets";return false;}b->assets[b->count++].key=key;}
  const std::size_t budget=std::min(a->options.pcm_budget,default_pcm_budget);
  for(unsigned i=0;i<b->count;++i){auto &pcm=b->assets[i];const auto f=std::find_if(config.files.begin(),config.files.end(),[&](const auto &v){return v.asset==pcm.key;});if(f==config.files.end()){e="Missing prepared WAV mapping: "+pcm.key;return false;}
   if(!load_wav(*a,f->path,budget-b->allocated,pcm.data,e))return false;
   pcm.frames=pcm.data.size()/a->frame_bytes;b->bytes+=pcm.data.size();b->allocated+=pcm.data.capacity();if(b->allocated>budget){e="World sound decoded PCM exceeds64MiB budget";return false;}}
  return finish(*a,std::move(b),e);
 }catch(const std::bad_alloc &){e="World sound preparation allocation failed; previous mix preserved";return false;}
}
namespace {
UpdateResult update_soundscape_impl(Audio *a,const world_soundscape::Emitter *emitters,std::size_t count,const SoundscapeFrame &frame){
 if(count>world_soundscape::sound_limit||(count&&!emitters)||!range(frame.dt,0,1)||!range(frame.gain,0,1))return UpdateResult::InvalidFrame;
 const auto &camera=frame.listener;double norm=0;for(double v:{camera.rotation.x,camera.rotation.y,camera.rotation.z,camera.rotation.w}){if(!std::isfinite(v))return UpdateResult::InvalidFrame;norm+=v*v;}
 if(std::abs(norm-1)>.02||!range(camera.position.x,-1.e8,1.e8)||!range(camera.position.y,-1.e8,1.e8)||!range(camera.position.z,-1.e8,1.e8))return UpdateResult::InvalidFrame;
 const auto &bank=*a->current;
 for(unsigned i=0;i<count;++i){const auto &em=emitters[i];if(!em.id||!range(em.condition_gain,0,1)||!runtime_sound_valid(em.sound)||(!em.sound.asset.empty()&&!asset(bank,em.sound.asset.c_str())))return UpdateResult::InvalidFrame;for(unsigned j=0;j<i;++j)if(emitters[j].id==em.id)return UpdateResult::InvalidFrame;}
 const auto clock=a->frames.load(std::memory_order_acquire);const auto elapsed=a->world_allowed&&clock>=a->world_clock?clock-a->world_clock:0;
 const bool allowed=frame.focused&&!frame.paused&&!frame.loading&&!frame.muted;
 auto previous=a->world;for(unsigned i=0;i<a->world_count;++i)world_advance(previous[i],elapsed);
 std::array<WorldSound,world_state_limit> next={};unsigned next_count=0;
 const double dt=std::min(frame.dt,.25);
 const bool audition_restart=frame.audition_id&&(frame.audition_id!=a->world_audition||frame.audition_serial!=a->world_audition_serial);
 for(unsigned i=0;i<count;++i){const auto &em=emitters[i];const auto &s=em.sound;auto &v=next[next_count++];
  for(unsigned j=0;j<a->world_count;++j)if(previous[j].id==em.id){v=previous[j];break;}
  const auto *pcm=s.asset.empty()?nullptr:asset(bank,s.asset.c_str());if(v.pcm!=pcm)v=WorldSound{};
  v.id=em.id;v.pcm=pcm;v.present=true;v.loop=s.loop;v.fade_out=s.fade_out;v.virtualized=true;
  if(!allowed){v.gain=v.target=v.from=0;v.ramp_time=v.ramp_duration=0;continue;}
  const bool audition=frame.audition_id==em.id;const bool selected=(!frame.solo_id||frame.solo_id==em.id)&&(!frame.audition_id||audition);
  const float distance=audition?1.f:world_soundscape::attenuation(s,camera.position.x,camera.position.y,camera.position.z);
  const bool inside=(s.enabled||audition)&&pcm&&distance>0;const bool entry=inside&&!v.inside;
  if(inside&&((!v.playing&&s.loop)||(!s.loop&&entry)||(audition&&audition_restart))){if(!s.loop||!v.playing||(audition&&audition_restart)){v.cursor=0;v.gain=v.target=v.from=0;v.ramp_time=v.ramp_duration=0;}v.playing=true;}
  v.inside=inside;float left=1,right=1;if(s.spatial&&!audition)pan(*a,camera,{s.x,s.y,s.z},left,right);v.left=left;v.right=right;
  const float target=v.playing&&inside&&selected?s.volume*distance*frame.gain*(audition?1.f:em.condition_gain):0;
  world_ramp(v,target,target>v.gain?s.fade_in:s.fade_out,dt);
 }
 // At most16 formerly audible removed nodes may finish a fade, in addition to
 // the64 authored logical nodes. Deleted virtual nodes never consume tails.
 for(unsigned i=0;i<a->world_count&&next_count<world_state_limit;++i){auto old=previous[i];if(!old.id||!old.playing||old.gain<=0)continue;bool present=false;for(unsigned j=0;j<count;++j)present|=emitters[j].id==old.id;if(present)continue;
  bool was_voice=false;for(auto id:a->world_slots)was_voice|=id==old.id;if(!was_voice||!allowed)continue;old.present=false;old.inside=false;old.virtualized=true;if(allowed)world_ramp(old,0,old.fade_out,dt);if(old.gain>0)next[next_count++]=old;
 }
 Command command;command.generation=bank.generation;
 auto slots=a->world_slots;std::array<unsigned,soundscape_voice_limit> chosen={};unsigned chosen_count=0;
 if(!allowed){command.kind=Command::Stop;slots.fill(0);}
 else{
  std::array<unsigned,world_state_limit> rank={};unsigned candidates=0;for(unsigned i=0;i<next_count;++i)if(next[i].pcm&&next[i].playing&&next[i].gain>0)rank[candidates++]=i;
  auto score=[&](unsigned i){float gain=next[i].gain;for(auto id:a->world_slots)if(id==next[i].id){gain*=1.05f;break;}return gain;};
  std::sort(rank.begin(),rank.begin()+candidates,[&](unsigned l,unsigned r){const auto a_gain=score(l),b_gain=score(r);return a_gain!=b_gain?a_gain>b_gain:next[l].id<next[r].id;});
  chosen_count=std::min({candidates,soundscape_voice_limit,a->channels});for(unsigned i=0;i<chosen_count;++i)chosen[i]=rank[i];
  for(auto &id:slots){bool selected=false;for(unsigned i=0;i<chosen_count;++i)selected|=id==next[chosen[i]].id;if(!selected)id=0;}
  for(unsigned i=0;i<chosen_count;++i){auto &v=next[chosen[i]];unsigned slot=0;for(;slot<soundscape_voice_limit;++slot)if(slots[slot]==v.id)break;if(slot==soundscape_voice_limit)for(slot=0;slot<soundscape_voice_limit&&slots[slot];++slot);if(slot>=soundscape_voice_limit||slot>=a->channels)return UpdateResult::InvalidFrame;
   const bool fresh=a->world_slots[slot]!=v.id;slots[slot]=v.id;v.virtualized=false;auto &d=command.voices[slot];d.pcm=v.pcm;d.id=v.id;d.category=4;d.active=true;d.loop=v.loop;d.remaining=std::numeric_limits<std::uint64_t>::max();d.cursor=v.cursor;d.cursor_clock=clock;d.clocked=true;d.seek=a->force_seek||fresh||(frame.audition_id==v.id&&audition_restart);d.dry=v.gain;d.left=v.left;d.right=v.right;
  }
 }
 if(!push(*a,command))return UpdateResult::QueueFull;
 a->world=next;a->world_count=next_count;a->world_authored=unsigned(count);a->world_virtualized=0;for(unsigned i=0;i<next_count;++i)a->world_virtualized+=next[i].present&&next[i].playing&&next[i].gain>0&&next[i].virtualized;
 a->world_slots=slots;a->world_clock=clock;a->world_allowed=allowed;a->force_seek=!allowed;a->outside_gain=1;
 if(allowed){a->world_audition=frame.audition_id;a->world_audition_serial=frame.audition_serial;}
 a->playback_world=true;return UpdateResult::Ok;
}
}
UpdateResult update_soundscape(Audio *a,const world_soundscape::Emitter *emitters,std::size_t count,const SoundscapeFrame &frame){
 if(!a||!a->current||!a->current->soundscape)return UpdateResult::NotConfigured;
 return update_soundscape_impl(a,emitters,count,frame);
}
UpdateResult update_soundscape_prepared(Audio *a,const world_soundscape::Emitter *emitters,std::size_t count,const SoundscapeFrame &frame){
 if(!a||!a->current||!a->shared_library)return UpdateResult::NotConfigured;
 if(!a->playback_world&&!stop_all(a))return UpdateResult::QueueFull;
 return update_soundscape_impl(a,emitters,count,frame);
}
bool sound(const Audio *a,std::uint64_t id,SoundState &out){
 if(!a||!id||!a->current||!a->playback_world)return false;
 for(unsigned i=0;i<a->world_count;++i){const auto &v=a->world[i];if(v.id==id){out={v.id,v.cursor,v.gain,v.target,v.present,v.inside,v.playing,v.virtualized};return true;}}return false;
}

namespace {
UpdateResult update_document_impl(Audio *a,const world_cinematics::Document &document,const world_cinematics::Step &step,const world_cinematics::CameraPose &camera,float gain){
 if(!range(gain,0,1)||document.zones.size()>a->options.zone_voices)return UpdateResult::InvalidFrame;
 const auto &b=*a->current;world_cinematics::Schedule schedule;world_cinematics::Mix mix;
 if(!world_cinematics::schedule(document,step,schedule)||!world_cinematics::sample_mix(document,camera.position,step.time,mix))return UpdateResult::InvalidFrame;
 double norm=0;for(double v:{camera.rotation.x,camera.rotation.y,camera.rotation.z,camera.rotation.w}){if(!std::isfinite(v))return UpdateResult::InvalidFrame;norm+=v*v;}if(std::abs(norm-1)>.02)return UpdateResult::InvalidFrame;
 Command command;command.generation=b.generation;auto tracked=a->tracked;const bool resync=a->force_seek||schedule.resync||step.wraps>0;
 if(step.paused)command.kind=Command::Stop;
 else{
  for(unsigned i=0;i<document.zones.size();i++){const auto &z=document.zones[i];auto &d=command.voices[i];d.pcm=asset(b,z.asset.c_str());if(!d.pcm)return UpdateResult::InvalidFrame;d.id=z.id;d.category=1;d.bus=z.bus;d.active=true;d.loop=true;d.remaining=std::numeric_limits<std::uint64_t>::max();d.cursor=std::uint64_t(std::floor(step.time*a->frequency))%d.pcm->frames;d.seek=resync;pan(*a,camera,z.center,d.left,d.right);for(unsigned j=0;j<mix.zones.count;j++)if(mix.zones.voices[j].id==z.id){d.dry=mix.zones.voices[j].dry_gain;d.send=mix.zones.voices[j].reverb_send;break;}}
  const unsigned outside=a->options.zone_voices+a->options.cue_voices;if(!b.config.outside_asset.empty()){auto &d=command.voices[outside];d.pcm=asset(b,b.config.outside_asset.c_str());d.active=true;d.id=1;d.category=3;d.bus=b.config.outside_bus;d.loop=true;d.seek=resync;d.remaining=std::numeric_limits<std::uint64_t>::max();d.cursor=std::uint64_t(std::floor(step.time*a->frequency))%d.pcm->frames;d.dry=mix.zones.outside_gain*b.config.outside_gain*mix.buses[d.bus];}
  for(unsigned slot=0;slot<a->options.cue_voices;slot++)if(tracked[slot]&&std::none_of(mix.clips.begin(),mix.clips.begin()+mix.clip_count,[&](const auto &c){return c.id==tracked[slot];}))tracked[slot]=0;
  for(unsigned i=0;i<mix.clip_count;i++){const auto &cue=mix.clips[i];unsigned slot=0;for(;slot<a->options.cue_voices;slot++)if(tracked[slot]==cue.id)break;if(slot==a->options.cue_voices)for(slot=0;slot<a->options.cue_voices;slot++)if(!tracked[slot])break;if(slot==a->options.cue_voices)return UpdateResult::InvalidFrame;const bool fresh=tracked[slot]!=cue.id;tracked[slot]=cue.id;
   const auto at=std::find_if(document.clips.begin(),document.clips.end(),[&](const auto &c){return c.id==cue.id;});if(at==document.clips.end())return UpdateResult::InvalidFrame;auto &d=command.voices[a->options.zone_voices+slot];d.pcm=asset(b,cue.asset);if(!d.pcm||at->source_offset*a->frequency>=d.pcm->frames)return UpdateResult::InvalidFrame;d.id=cue.id;d.category=2;d.bus=cue.bus;d.active=true;d.loop=cue.loop;d.seek=resync||fresh;d.cursor=std::uint64_t(std::floor(cue.source_offset*a->frequency+1e-6));d.loop_start=std::uint64_t(std::floor(at->source_offset*a->frequency+1e-6));if(d.loop&&d.cursor>=d.pcm->frames)d.cursor=d.loop_start+(d.cursor-d.loop_start)%(d.pcm->frames-d.loop_start);d.remaining=std::uint64_t(std::ceil(std::max(0.,at->start+at->duration-step.time)*a->frequency));d.dry=cue.gain;d.send=cue.gain*a->options.cue_send;
  }
 }
 for(auto &voice:command.voices){voice.dry*=gain;voice.send*=gain;}
 if(!push(*a,command))return UpdateResult::QueueFull;
 a->tracked=tracked;a->force_seek=step.paused;a->outside_gain=mix.zones.outside_gain;a->playback_world=false;return UpdateResult::Ok;
}
}
UpdateResult update(Audio *a,const world_cinematics::Step &step,const world_cinematics::CameraPose &camera){
 if(!a||!a->current||a->current->soundscape)return UpdateResult::NotConfigured;
 return update_document_impl(a,a->current->config.document,step,camera,1);
}
bool library_ready(const Audio *a){return a&&a->current&&a->shared_library;}
bool configure_library_files(Audio *a,const std::vector<FileAsset>&files,std::string &error){
 if(files.size()>asset_limit){error="Published cutscenes and map sounds share at most32 WAV assets.";return false;}
 Configuration config;config.files=files;config.document.name="Prepared world cutscene audio";config.document.duration=std::max(1.,double(files.size()));
 for(unsigned i=0;i<files.size();++i){for(unsigned j=0;j<i;++j)if(files[j].asset==files[i].asset){error="Duplicate shared cutscene WAV key.";return false;}
  world_cinematics::Clip cue;cue.id=i+1;cue.asset=files[i].asset;cue.start=i;cue.duration=.01;cue.fade_in=cue.fade_out=0;config.document.clips.push_back(std::move(cue));}
 if(!configure_files(a,config,error))return false;
 a->shared_library=true;return true;
}
bool prepared_document_valid(const Audio *a,const world_cinematics::Document&d,std::string&error){
 if(!library_ready(a)||!world_cinematics::validate(d,error)){if(error.empty())error="Shared cutscene PCM is not prepared.";return false;}
 if(d.zones.size()>a->options.zone_voices){error="Cutscene ambience exceeds the owned zone voice count.";return false;}
 for(const auto&z:d.zones)if(!asset(*a->current,z.asset.c_str())){error="Cutscene ambience WAV is absent from the prepared library: "+z.asset;return false;}
 for(const auto&c:d.clips){const auto *p=asset(*a->current,c.asset.c_str());if(!p||c.source_offset*a->frequency>=p->frames){error="Cutscene cue is absent or starts outside its WAV: "+c.asset;return false;}
  unsigned overlap=0;for(const auto&v:d.clips)overlap+=v.start<=c.start&&v.start+v.duration>c.start;if(overlap>a->options.cue_voices){error="Cutscene cues exceed the owned voice count.";return false;}}
 error.clear();return true;
}
UpdateResult update_prepared_document(Audio *a,const world_cinematics::Document&d,const world_cinematics::Step&s,const world_cinematics::CameraPose&camera,float gain){
 if(!library_ready(a))return UpdateResult::NotConfigured;
 if(a->playback_world){if(!stop_all(a))return UpdateResult::QueueFull;a->world_allowed=false;}
 return update_document_impl(a,d,s,camera,gain);
}
bool stop_all(Audio *a){if(!a)return false;Command c;c.kind=Command::Stop;if(!push(*a,c))return false;a->tracked.fill(0);a->force_seek=true;return true;}
bool collect(Audio *a){if(!a)return false;if(a->retired&&a->acknowledged.load(std::memory_order_acquire)>=a->generation)a->retired.reset();return !a->retired;}
Stats stats(const Audio *a){Stats s;if(!a)return s;s.first_channel=a->first;s.channels=a->channels;s.frequency=a->frequency;s.format=a->format;s.generation=a->generation;s.acknowledged_generation=a->acknowledged.load(std::memory_order_acquire);s.callbacks=a->callbacks.load(std::memory_order_acquire);s.rendered_frames=a->frames.load(std::memory_order_relaxed);s.starts=a->starts.load(std::memory_order_relaxed);s.queue_full=a->queue_full.load(std::memory_order_relaxed);s.outside_gain=a->outside_gain;s.soundscape_mode=a->playback_world;s.authored_emitters=a->world_authored;s.virtualized_emitters=a->world_virtualized;s.soundscape_capacity=std::min(soundscape_voice_limit,a->channels);s.retired_bank=bool(a->retired);if(a->current){s.assets=a->current->count;s.decoded_bytes=a->current->bytes;s.allocated_pcm_bytes=a->current->allocated;}for(unsigned i=0;i<a->channels;i++)s.active_voices+=a->visible[i].active.load(std::memory_order_acquire);return s;}
bool voice(const Audio *a,unsigned slot,VoiceState &out){if(!a||slot>=a->channels)return false;const auto &v=a->visible[slot];VoiceState s;s.active=v.active.load(std::memory_order_acquire)!=0;s.id=v.id.load(std::memory_order_relaxed);s.category=v.category.load(std::memory_order_relaxed);s.start_frame=v.start.load(std::memory_order_relaxed);s.cursor_frame=v.cursor.load(std::memory_order_relaxed);s.loop_start_frame=v.loop_start.load(std::memory_order_relaxed);s.rendered_frames=v.rendered.load(std::memory_order_relaxed);s.dry_gain=v.dry.load(std::memory_order_relaxed);s.reverb_send=v.send.load(std::memory_order_relaxed);out=s;return true;}
}

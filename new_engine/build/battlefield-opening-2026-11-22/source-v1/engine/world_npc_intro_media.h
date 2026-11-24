#ifndef VALWOOD_WORLD_NPC_INTRO_MEDIA_H
#define VALWOOD_WORLD_NPC_INTRO_MEDIA_H
// Prepared local media only. No runtime child process, decoder plugin or GL
// calls. load/unload and transport calls belong to the main thread; a bounded
// worker decodes JPEG frames, and one owned mixer effect renders the voice.
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include "stb_image.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace world_npc_intro_media {
enum class Mode { None, Voice, Video };
// Adopts stb's allocation directly: no second full-frame memcpy/allocation.
struct Pixels {
    unsigned char *pointer=nullptr;std::size_t bytes=0;
    Pixels()=default;Pixels(const Pixels&)=delete;Pixels&operator=(const Pixels&)=delete;
    Pixels(Pixels&&p)noexcept:pointer(p.pointer),bytes(p.bytes){p.pointer=nullptr;p.bytes=0;}
    Pixels&operator=(Pixels&&p)noexcept{if(this!=&p){clear();pointer=p.pointer;bytes=p.bytes;p.pointer=nullptr;p.bytes=0;}return *this;}
    ~Pixels(){clear();}void clear(){if(pointer)stbi_image_free(pointer);pointer=nullptr;bytes=0;}
    unsigned char *data(){return pointer;}const unsigned char *data()const{return pointer;}
    std::size_t size()const{return bytes;}bool empty()const{return !pointer;}
};
struct Frame {unsigned width=0,height=0,index=0;double pts=0;Pixels rgba;};
struct Stats {
    int audio_channel=-1;std::size_t pcm_bytes=0,queued_frames=0,queued_rgba_bytes=0;
    std::uint64_t decoded_frames=0,audio_callbacks=0,audio_frames=0,audio_resyncs=0;
    double maximum_audio_drift_seconds=0;bool decoder_working=false,playing=false,paused=false;
};
struct Letterbox {float x=0,y=0,width=0,height=0;};
inline Letterbox letterbox(float width,float height,unsigned source_width,unsigned source_height){
    if(!(width>0&&height>0&&source_width&&source_height))return {};
    const float scale=std::min(width/float(source_width),height/float(source_height));
    const float w=source_width*scale,h=source_height*scale;return {(width-w)*.5f,(height-h)*.5f,w,h};
}
namespace detail {
inline bool relative(const std::string &s){
    if(s.empty()||s.size()>240||s.front()=='/'||s.find_first_of("\\:\r\n\t")!=std::string::npos)return false;
    std::size_t at=0;while(at<s.size()){auto end=s.find('/',at);if(end==std::string::npos)end=s.size();
        const auto part=s.substr(at,end-at);if(part.empty()||part=="."||part=="..")return false;at=end+1;}
    return s.back()!='/';
}
inline bool file(const std::filesystem::path &root,const std::string &name,std::uint64_t limit,std::filesystem::path &out){
    if(!relative(name))return false;
    std::error_code ec;auto current=root;
    const std::filesystem::path relative_path=std::filesystem::u8path(name);
    for(const auto &part:relative_path){current/=part;const auto status=std::filesystem::symlink_status(current,ec);if(ec||std::filesystem::is_symlink(status))return false;}
    if(!std::filesystem::is_regular_file(current,ec)||ec)return false;
    const auto size=std::filesystem::file_size(current,ec);if(ec||!size||size>limit)return false;out=current;return true;
}
inline bool utf8(const std::string&s){
    if(s.empty()||s.size()>2048)return false;
    for(std::size_t i=0;i<s.size();){const auto c=static_cast<unsigned char>(s[i]);
        if(c<32&&c!='\n')return false;
        if(c==127)return false;
        if(c<128){++i;continue;}
        unsigned count=0,code=0,min=0;
        if(c>=0xc2&&c<=0xdf){count=2;code=c&31;min=0x80;}else if(c>=0xe0&&c<=0xef){count=3;code=c&15;min=0x800;}else if(c>=0xf0&&c<=0xf4){count=4;code=c&7;min=0x10000;}else return false;
        if(i+count>s.size())return false;
        for(unsigned j=1;j<count;++j){const auto d=static_cast<unsigned char>(s[i+j]);if((d&0xc0)!=0x80)return false;code=(code<<6)|(d&63);}
        if(code<min||code>0x10ffff||(code>=0xd800&&code<=0xdfff)||(code>=0x80&&code<=0x9f))return false;
        i+=count;
    }return true;
}
}
class Media {
public:
    Media()=default;Media(const Media&)=delete;Media&operator=(const Media&)=delete;
    ~Media(){unload();}
    bool load(const char *project_directory,std::string &error){
        unload();error.clear();
        try{return load_impl(project_directory,error);}catch(const std::exception &e){error=std::string("NPC media preparation failed: ")+e.what();unload();return false;}
    }
    void play(double cinematic_time=0){
        if(!ready())return;
        // A mixer callback may run before the host's first update supplies its
        // persisted voice/master gain. Never emit a default-volume first buffer.
        audio_gain_.store(0.f);
        if(original_audio_&&channel_<0){std::string message;try{if(!activate_audio(message)){fail(message);return;}}catch(const std::exception&e){fail(e.what());return;}}
        playing_=true;paused_=false;last_time_=clamp_time(cinematic_time);last_allowed_=true;
        {std::lock_guard<std::mutex>lock(mutex_);served_=~0u;++generation_;}
        audio_playing_.store(true);audio_paused_.store(false);audio_seek(last_time_);request_frame(last_time_);
    }
    void seek(double cinematic_time){
        if(!ready())return;
        last_time_=clamp_time(cinematic_time);audio_seek(last_time_);
        {std::lock_guard<std::mutex>lock(mutex_);served_=~0u;++generation_;frames_.clear();}request_frame(last_time_);
    }
    void update(double cinematic_time,bool paused,float user_gain,bool allowed=true){
        if(!ready()||!playing_)return;
        if(!std::isfinite(cinematic_time)||!std::isfinite(user_gain)){fail("Invalid NPC media clock or gain");return;}
        const double time=clamp_time(cinematic_time);const bool silent=paused||!allowed;
        audio_gain_.store(std::clamp(user_gain,0.f,1.f));
        if(original_audio_&&!owns_channel()){fail("NPC media audio channel ownership changed");return;}
        // The callback advances freely between updates. Seek on explicit jumps,
        // resume or substantial measured drift, never on every rendered frame.
        const double cursor=double(audio_cursor_.load())/audio_rate_;
        if(original_audio_&&!silent&&!paused_&&last_allowed_){
            const double drift=std::abs(cursor-time);max_drift_=std::max(max_drift_,drift);
            if(time<last_time_-.002||time-last_time_>.5||drift>.080){audio_seek(time);++resyncs_;}
        }else if(original_audio_&&!silent&&(paused_||!last_allowed_))audio_seek(time);
        paused_=paused;last_allowed_=allowed;last_time_=time;
        audio_paused_.store(silent);audio_playing_.store(time<duration_);request_frame(time);
    }
    void stop(){playing_=false;paused_=false;audio_playing_.store(false);audio_paused_.store(true);}
    void unload(){
        stop();{std::lock_guard<std::mutex> lock(mutex_);quit_=true;}
        wake_.notify_all();if(worker_.joinable())worker_.join();
        release_audio();{std::lock_guard<std::mutex> lock(mutex_);frames_.clear();indices_.clear();error_.clear();working_=false;quit_=false;desired_=0;served_=~0u;}
        ready_=false;failed_=false;mode_=Mode::None;duration_=0;width_=height_=0;fps_=0;subtitles_.clear();pack_.clear();original_audio_=false;generation_=0;requested_time_=0;
        decoded_=0;max_drift_=0;resyncs_=0;last_time_=0;last_allowed_=true;
    }
    bool take_frame(Frame &out){
        if(!ready()||mode_!=Mode::Video)return false;
        std::lock_guard<std::mutex> lock(mutex_);
        auto found=frames_.end();
        for(auto item=frames_.begin();item!=frames_.end();++item)if(item->index<=desired_&&(served_==~0u||item->index>served_)&&requested_time_-item->pts<=.5&&(found==frames_.end()||item->index>found->index))found=item;
        if(found==frames_.end())return false;
        out=std::move(*found);served_=out.index;frames_.erase(found);
        frames_.erase(std::remove_if(frames_.begin(),frames_.end(),[&](const auto&f){return f.index<=served_;}),frames_.end());wake_.notify_one();return true;
    }
    Mode mode()const{return mode_;}bool ready()const{return ready_&&!failed_.load();}
    double duration()const{return duration_;}bool failed()const{return failed_.load();}
    bool has_audio()const{return original_audio_;}bool has_subtitles()const{return !subtitles_.empty();}
    const char *subtitle(double time)const{
        if(!std::isfinite(time))return nullptr;
        const auto found=std::upper_bound(subtitles_.begin(),subtitles_.end(),time,[](double t,const Cue&c){return t<c.start;});
        if(found==subtitles_.begin())return nullptr;
        const auto &cue=*(found-1);return time<cue.end?cue.text.c_str():nullptr;
    }
    std::string error()const{std::lock_guard<std::mutex> lock(mutex_);return error_;}
    Stats stats()const{
        Stats s;s.audio_channel=channel_;s.pcm_bytes=pcm_.size();s.decoded_frames=decoded_.load();s.audio_callbacks=callbacks_.load();s.audio_frames=audio_cursor_.load();s.audio_resyncs=resyncs_;s.maximum_audio_drift_seconds=max_drift_;s.playing=playing_;s.paused=paused_;
        std::lock_guard<std::mutex> lock(mutex_);s.queued_frames=frames_.size();s.decoder_working=working_;for(const auto &f:frames_)s.queued_rgba_bytes+=f.rgba.size();return s;
    }
#ifdef VALWOOD_NPC_INTRO_MEDIA_TESTING
    void testing_decode_delay(unsigned milliseconds){test_decode_delay_=std::min(milliseconds,1000u);}
#endif
private:
    struct Index {double pts=0;std::uint64_t offset=0;unsigned bytes=0;};
    struct Cue {double start=0,end=0;std::string text;};
    Mode mode_=Mode::None;double duration_=0,fps_=0,last_time_=0,max_drift_=0;unsigned width_=0,height_=0;
    bool ready_=false,playing_=false,paused_=false,last_allowed_=true,original_audio_=false;
    std::atomic<bool> failed_{false};mutable std::mutex mutex_;std::condition_variable wake_;std::thread worker_;
    std::vector<Index> indices_;std::deque<Frame> frames_;std::vector<Cue> subtitles_;std::filesystem::path pack_;
    unsigned desired_=0,served_=~0u;double requested_time_=0;std::uint64_t generation_=0;bool quit_=false,working_=false;std::string error_;std::atomic<std::uint64_t> decoded_{0};
#ifdef VALWOOD_NPC_INTRO_MEDIA_TESTING
    std::atomic<unsigned> test_decode_delay_{0};
#endif
    std::vector<Uint8> pcm_;std::array<Uint8,1024> silence_={};Mix_Chunk carrier_={};int channel_=-1,audio_rate_=0;Uint16 audio_format_=0;unsigned frame_bytes_=0;
    bool registered_=false;std::atomic<bool> audio_playing_{false},audio_paused_{true};std::atomic<float> audio_gain_{1};
    std::atomic<std::uint64_t> audio_target_{0},audio_serial_{0},audio_cursor_{0},callbacks_{0};
    std::uint64_t callback_serial_=0,callback_cursor_=0,resyncs_=0;
    double clamp_time(double value)const{return std::isfinite(value)?std::clamp(value,0.,duration_):0.;}
    void fail(const std::string &message){audio_playing_.store(false);failed_.store(true);std::lock_guard<std::mutex> lock(mutex_);error_=message;}
    static bool key(std::istream &in,const char *expected){std::string text;return bool(in>>text)&&text==expected;}
    bool load_impl(const char *project_directory,std::string &error){
        auto reject=[&](const char *reason){error=reason;unload();return false;};
        if(!project_directory||!*project_directory)return reject("NPC media project directory is missing");
        std::error_code ec;const auto project=std::filesystem::canonical(std::filesystem::u8path(project_directory),ec);
        if(ec||!std::filesystem::is_directory(project,ec))return reject("NPC media project directory is unavailable");
        std::filesystem::path config_path;if(!detail::file(project,"opening/npc_intro/media.cfg",4096,config_path))return reject("No prepared NPC intro media; using the authored scene");
        const auto root=config_path.parent_path();std::ifstream config(config_path);unsigned version=0,audio_present=0;std::string magic,mode,audio,index,pack,subtitles,extra;
        if(!(config>>magic>>version)||magic!="VALWOOD_NPC_INTRO_MEDIA"||version!=1||!key(config,"mode")||!(config>>mode)||
           !key(config,"duration")||!(config>>duration_)||!std::isfinite(duration_)||duration_<.1||duration_>300||
           !key(config,"audio")||!(config>>std::quoted(audio))||!key(config,"audio_present")||!(config>>audio_present)||audio_present>1||
           !key(config,"index")||!(config>>std::quoted(index))||!key(config,"pack")||!(config>>std::quoted(pack))||
           !key(config,"subtitles")||!(config>>std::quoted(subtitles))||!key(config,"end")||(config>>extra))return reject("Invalid prepared NPC media definition");
        if(mode=="video")mode_=Mode::Video;else if(mode=="voice")mode_=Mode::Voice;else return reject("Unknown NPC media mode");
        original_audio_=audio_present!=0;
        std::filesystem::path audio_path;if(!detail::file(root,audio,58u*1024u*1024u,audio_path))return reject("Prepared NPC voice is missing or unsafe");
        if(mode_==Mode::Video){
            std::filesystem::path index_path;if(!detail::file(root,index,2u*1024u*1024u,index_path)||!detail::file(root,pack,8ull*1024*1024*1024,pack_))return reject("Prepared NPC frames are missing or unsafe");
            std::ifstream input(index_path);unsigned numerator=0,denominator=0,count=0;std::uint64_t bytes=0,expected=0;
            if(!(input>>magic>>version)||magic!="VALWOOD_NPC_INTRO_FRAMES"||version!=1||!key(input,"size")||!(input>>width_>>height_)||!width_||!height_||width_>3840||height_>2160||
               !key(input,"fps")||!(input>>numerator>>denominator)||!numerator||!denominator||numerator>30000000||denominator>1000000||
               !key(input,"count")||!(input>>count)||!count||count>9000||!key(input,"pack_size")||!(input>>bytes))return reject("Invalid prepared NPC frame index");
            fps_=double(numerator)/denominator;
            if(fps_<=0||fps_>30||count>std::ceil(duration_*fps_)+1||bytes!=std::filesystem::file_size(pack_,ec)||ec)return reject("Prepared NPC frame timing/size mismatch");
            indices_.reserve(count);
            for(unsigned i=0;i<count;++i){unsigned number=~0u;Index entry;
                if(!key(input,"frame")||!(input>>number>>entry.pts>>entry.offset>>entry.bytes)||number!=i||!std::isfinite(entry.pts)||std::abs(entry.pts-double(i)/fps_)>1e-7||entry.pts>=duration_+1e-7||entry.offset!=expected||!entry.bytes||entry.bytes>32u*1024u*1024u||entry.offset>bytes||entry.bytes>bytes-entry.offset)return reject("Invalid NPC frame offset or timestamp");
                expected+=entry.bytes;indices_.push_back(entry);
            }
            if(expected!=bytes||!key(input,"end")||(input>>extra)||double(count)/fps_<duration_-1./fps_-1e-7)return reject("Incomplete NPC frame index");
            Frame initial;std::string decode_error;bool decoded=false;
            // stb's per-thread flip flag must never alter the host's image
            // loading convention. Even the loading-boundary preview uses its
            // own decoder thread, then transfers the single allocation.
            std::thread preload([&]{try{decoded=decode(0,initial,decode_error);}catch(const std::exception &e){decode_error=e.what();}});preload.join();
            if(!decoded)return reject(decode_error.c_str());
            frames_.push_back(std::move(initial));
        }else if(!index.empty()||!pack.empty())return reject("Voice-only NPC media cannot reference video frames");
        if(!subtitles.empty()){
            std::filesystem::path subtitle_path;if(!detail::file(root,subtitles,2u*1024u*1024u,subtitle_path))return reject("Prepared NPC subtitles are missing or unsafe");
            std::ifstream input(subtitle_path);if(!(input>>magic>>version)||magic!="VALWOOD_NPC_INTRO_SUBTITLES"||version!=1)return reject("Invalid NPC subtitles header");
            bool ended=false;while(input>>magic){if(magic=="end"){ended=true;break;}Cue cue;
                if(magic!="cue"||!(input>>cue.start>>cue.end>>std::quoted(cue.text))||!std::isfinite(cue.start)||!std::isfinite(cue.end)||cue.start<0||cue.end<=cue.start||cue.end>duration_+.001||(!subtitles_.empty()&&cue.start<subtitles_.back().end)||!detail::utf8(cue.text)||subtitles_.size()>=1000)return reject("Invalid NPC subtitle cue");
                subtitles_.push_back(std::move(cue));
            }if(!ended||(input>>extra)||subtitles_.empty())return reject("Incomplete NPC subtitles");
        }
        if(!prepare_audio(audio_path,error)){unload();return false;}
        ready_=true;failed_=false;desired_=0;served_=~0u;
        if(mode_==Mode::Video)worker_=std::thread([this]{worker_loop();});
        return true;
    }
    bool decode(unsigned number,Frame &out,std::string &error){
        if(number>=indices_.size()){error="NPC frame number is unavailable";return false;}
        const auto index=indices_[number];std::ifstream input(pack_,std::ios::binary);if(!input){error="NPC frame pack disappeared";return false;}
        std::vector<stbi_uc> encoded(index.bytes);input.seekg(std::streamoff(index.offset));if(!input.read(reinterpret_cast<char*>(encoded.data()),encoded.size())){error="Truncated NPC frame pack";return false;}
        if(encoded.size()<4||encoded[0]!=0xff||encoded[1]!=0xd8||encoded[encoded.size()-2]!=0xff||encoded.back()!=0xd9){error="Corrupt NPC JPEG boundaries";return false;}
        int width=0,height=0,channels=0;
        if(!stbi_info_from_memory(encoded.data(),int(encoded.size()),&width,&height,&channels)||width!=int(width_)||height!=int(height_)||channels<1||channels>4){error="NPC JPEG dimensions are corrupt";return false;}
        stbi_set_flip_vertically_on_load_thread(0);
        auto *pixels=stbi_load_from_memory(encoded.data(),int(encoded.size()),&width,&height,&channels,4);
        if(!pixels){error="NPC JPEG decode failed";return false;}
        out.width=unsigned(width);out.height=unsigned(height);out.index=number;out.pts=index.pts;out.rgba.pointer=pixels;out.rgba.bytes=std::size_t(width)*height*4;++decoded_;return true;
    }
    void request_frame(double time){
        if(mode_!=Mode::Video||indices_.empty())return;
        const unsigned desired=std::min<unsigned>(unsigned(indices_.size()-1),unsigned(std::max(0.,std::floor(time*fps_+1e-7))));
        {std::lock_guard<std::mutex> lock(mutex_);if(desired<desired_){served_=~0u;++generation_;frames_.clear();}desired_=desired;requested_time_=time;
            frames_.erase(std::remove_if(frames_.begin(),frames_.end(),[&](const auto&f){return f.index>desired_+2||requested_time_-f.pts>.5||(served_!=~0u&&f.index<=served_);}),frames_.end());
            if(served_!=desired_&&frames_.size()>=2&&std::none_of(frames_.begin(),frames_.end(),[&](const auto&f){return f.index==desired_;})){
                auto oldest=std::min_element(frames_.begin(),frames_.end(),[](const auto&a,const auto&b){return a.index<b.index;});frames_.erase(oldest);
            }
        }
        wake_.notify_one();
    }
    unsigned next_locked()const{
        if(frames_.size()>=2)return ~0u;
        for(unsigned number=desired_;number<indices_.size()&&number<=desired_+2;++number){
            if(number==served_)continue;
            bool found=false;for(const auto &f:frames_)if(f.index==number){found=true;break;}if(!found)return number;
        }return ~0u;
    }
    void worker_loop(){
        try{for(;;){unsigned number=~0u;std::uint64_t generation=0;
            {std::unique_lock<std::mutex> lock(mutex_);wake_.wait(lock,[&]{return quit_||failed_.load()||(number=next_locked())!=~0u;});if(quit_||failed_)return;working_=true;generation=generation_;}
            Frame frame;std::string message;const bool ok=decode(number,frame,message);
#ifdef VALWOOD_NPC_INTRO_MEDIA_TESTING
            if(test_decode_delay_.load())SDL_Delay(test_decode_delay_.load());
#endif
            {std::lock_guard<std::mutex> lock(mutex_);working_=false;
                if(!ok){error_=message;failed_=true;audio_playing_=false;return;}
                const bool fresh=requested_time_-frame.pts<=.5;
                if(generation==generation_&&fresh&&number<=desired_+2&&(served_==~0u||number>served_)){
                    if(frames_.size()>=2){auto oldest=std::min_element(frames_.begin(),frames_.end(),[](const auto&a,const auto&b){return a.index<b.index;});if(oldest->index<number)frames_.erase(oldest);}
                    if(frames_.size()<2)frames_.push_back(std::move(frame));
                }
            }
        }}catch(const std::exception &e){fail(std::string("NPC frame worker failed: ")+e.what());}
    }
    bool owns_channel()const{return channel_>=0&&Mix_QuerySpec(nullptr,nullptr,nullptr)&&Mix_GetChunk(channel_)==&carrier_&&Mix_Playing(channel_);}
    void audio_seek(double time){audio_target_.store(std::min<std::uint64_t>(pcm_.size()/std::max(1u,frame_bytes_),std::uint64_t(std::floor(time*audio_rate_))));audio_serial_.fetch_add(1);}
    static void SDLCALL effect(int,void *stream,int bytes,void *context){static_cast<Media*>(context)->render_audio(stream,bytes);}
    void render_audio(void *stream,int bytes){
        ++callbacks_;std::memset(stream,0,std::size_t(std::max(0,bytes)));
        const auto serial=audio_serial_.load();if(serial!=callback_serial_){callback_serial_=serial;callback_cursor_=audio_target_.load();audio_cursor_.store(callback_cursor_);}
        if(!audio_playing_.load()||audio_paused_.load()||!frame_bytes_||bytes<=0)return;
        const std::size_t total=pcm_.size()/frame_bytes_,count=std::min<std::size_t>(std::size_t(bytes)/frame_bytes_,total-std::min<std::size_t>(total,callback_cursor_));
        const float gain=audio_gain_.load();
        if(audio_format_==AUDIO_S16SYS){auto *output=static_cast<Sint16*>(stream);const auto *source=reinterpret_cast<const Sint16*>(pcm_.data())+callback_cursor_*2;for(std::size_t i=0;i<count*2;++i)output[i]=Sint16(std::lround(source[i]*gain));}
        else{auto *output=static_cast<float*>(stream);const auto *source=reinterpret_cast<const float*>(pcm_.data())+callback_cursor_*2;for(std::size_t i=0;i<count*2;++i)output[i]=source[i]*gain;}
        callback_cursor_+=count;audio_cursor_.store(callback_cursor_);
    }
    bool prepare_audio(const std::filesystem::path &path,std::string &error){
        std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file){error="Prepared NPC WAV cannot be read";return false;}
        const std::uint64_t file_size=std::uint64_t(file.tellg());file.seekg(0);std::array<Uint8,16> bytes={};
        auto u16=[](const Uint8*p){return unsigned(p[0])|(unsigned(p[1])<<8);};
        auto u32=[](const Uint8*p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);};
        if(!file.read(reinterpret_cast<char*>(bytes.data()),12)||std::memcmp(bytes.data(),"RIFF",4)||std::memcmp(bytes.data()+8,"WAVE",4)||std::uint64_t(u32(bytes.data()+4))+8!=file_size){error="Prepared NPC audio must be a complete RIFF WAV";return false;}
        bool format=false;std::uint64_t offset=0,size=0;
        for(std::uint64_t at=12;at+8<=file_size;){file.seekg(std::streamoff(at));if(!file.read(reinterpret_cast<char*>(bytes.data()),8)){error="Truncated NPC WAV chunk";return false;}
            const std::uint64_t count=u32(bytes.data()+4),next=at+8+count+(count&1);if(next>file_size){error="NPC WAV chunk exceeds file bounds";return false;}
            if(!std::memcmp(bytes.data(),"fmt ",4)){
                if(format||count<16||count>4096||!file.read(reinterpret_cast<char*>(bytes.data()),16)||u16(bytes.data())!=1||u16(bytes.data()+2)!=2||u32(bytes.data()+4)!=48000||u32(bytes.data()+8)!=192000||u16(bytes.data()+12)!=4||u16(bytes.data()+14)!=16){error="NPC WAV must be uncompressed48kHz PCM16 stereo";return false;}format=true;
            }else if(!std::memcmp(bytes.data(),"data",4)){if(offset){error="Multiple NPC WAV payloads are unsupported";return false;}offset=at+8;size=count;}
            at=next;
        }
        if(!format||!offset||!size||size>57600000||size%4||std::abs(double(size)/192000.-duration_)>1./48000.+1e-8){error="NPC WAV duration/PCM size does not match the clip";return false;}
        pcm_.resize(std::size_t(size));file.seekg(std::streamoff(offset));if(!file.read(reinterpret_cast<char*>(pcm_.data()),pcm_.size())){error="Truncated NPC WAV PCM";return false;}
        audio_rate_=48000;audio_format_=AUDIO_S16LSB;frame_bytes_=4;return true;
    }
    bool activate_audio(std::string &error){
        int channels=0;int rate=0;Uint16 format=0;
        if(!Mix_QuerySpec(&rate,&format,&channels)||channels!=2||(format!=AUDIO_S16SYS&&format!=AUDIO_F32SYS)||rate<8000||rate>96000){error="NPC media needs the existing stereo S16/F32 mixer";return false;}
        SDL_AudioCVT convert{};
        const auto bytes=pcm_.size();
        if(SDL_BuildAudioCVT(&convert,audio_format_,2,audio_rate_,format,2,rate)<0||convert.len_mult<1||std::uint64_t(bytes)*unsigned(convert.len_mult)>128ull*1024*1024){error="NPC PCM conversion exceeds its128MiB limit";return false;}
        pcm_.resize(std::size_t(bytes)*convert.len_mult);convert.buf=pcm_.data();convert.len=int(bytes);
        if(convert.needed&&SDL_ConvertAudio(&convert)<0){error="NPC PCM conversion failed";return false;}
        pcm_.resize(std::size_t(convert.needed?convert.len_cvt:convert.len));audio_rate_=rate;audio_format_=format;frame_bytes_=unsigned(SDL_AUDIO_BITSIZE(audio_format_)/8)*2;
        if(pcm_.empty()||pcm_.size()%frame_bytes_){error="NPC PCM conversion is unaligned";return false;}
        channel_=Mix_AllocateChannels(-1);if(channel_<0||channel_>=255||Mix_AllocateChannels(channel_+1)!=channel_+1){error="No dedicated NPC media channel is available";channel_=-1;return false;}
        carrier_.allocated=0;carrier_.abuf=silence_.data();carrier_.alen=Uint32(silence_.size());carrier_.volume=MIX_MAX_VOLUME;
        if(Mix_PlayChannel(channel_,&carrier_,-1)!=channel_){error="Cannot start the dedicated NPC audio carrier";return false;}
        Mix_Volume(channel_,MIX_MAX_VOLUME);
        if(!Mix_RegisterEffect(channel_,effect,nullptr,this)){error="Cannot register the dedicated NPC audio effect";return false;}registered_=true;return true;
    }
    void release_audio(){
        if(Mix_QuerySpec(nullptr,nullptr,nullptr)){
            const bool owned=owns_channel();
            if(registered_&&owned)Mix_UnregisterEffect(channel_,effect);
            if(owned)Mix_HaltChannel(channel_);
            if(owned&&channel_>=0&&Mix_AllocateChannels(-1)==channel_+1)Mix_AllocateChannels(channel_);
        }
        registered_=false;channel_=-1;pcm_.clear();pcm_.shrink_to_fit();frame_bytes_=0;audio_rate_=0;audio_format_=0;callbacks_=0;audio_cursor_=0;audio_target_=0;audio_serial_=0;callback_serial_=callback_cursor_=0;
    }
};
} // namespace world_npc_intro_media
#endif

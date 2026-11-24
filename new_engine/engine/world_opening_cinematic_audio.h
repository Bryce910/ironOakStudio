#ifndef VALWOOD_OPENING_CINEMATIC_AUDIO_H
#define VALWOOD_OPENING_CINEMATIC_AUDIO_H

// Original, deliberately restrained apartment Foley. All PCM is generated at
// loading time; the frame update only changes this adapter's own voice gains.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace world_opening_cinematic_audio {
enum class Cue : unsigned { Ambient, DoorClose, BagDown, ConsolePower, Count };
namespace detail {
constexpr double pi=3.14159265358979323846;
inline double hash(std::uint32_t n){
    n^=n>>16;n*=0x7feb352du;n^=n>>15;n*=0x846ca68bu;n^=n>>16;
    return double(n&0x00ffffffu)/8388607.5-1.;
}
inline double smooth(double x){x=std::clamp(x,0.,1.);return x*x*(3.-2.*x);}
inline double noise(double time,double hz,std::uint32_t seed){
    const double x=time*hz;const auto k=std::uint32_t(std::floor(x));
    const double f=smooth(x-std::floor(x));return hash(k+seed)*(1.-f)+hash(k+1+seed)*f;
}
inline double loop_noise(double time,unsigned cells,std::uint32_t seed){
    const double x=time*double(cells)/8.;const auto k=unsigned(std::floor(x));
    const double f=smooth(x-std::floor(x));
    return hash(k%cells+seed)*(1.-f)+hash((k+1)%cells+seed)*f;
}
inline double attack_decay(double time,double start,double duration,double decay){
    if(time<start||time>=start+duration)return 0.;
    const double age=time-start;
    return smooth(age/.003)*std::exp(-age*decay)*smooth((duration-age)/.025);
}
inline double duration(Cue cue){
    switch(cue){case Cue::Ambient:return 8.;case Cue::DoorClose:return .98;
    case Cue::BagDown:return .72;case Cue::ConsolePower:return .44;default:return 0.;}
}
inline float sample(Cue cue,double t){
    double v=0.;
    switch(cue){
    case Cue::Ambient:{
        // Integer cycles over eight seconds make both the hum and softened
        // noise seamless. Low, distant traffic never becomes a foreground cue.
        const double city=.42+.24*std::sin(2*pi*t/8.)+.15*std::sin(2*pi*3*t/8.);
        v=.046*std::sin(2*pi*60*t)+.018*std::sin(2*pi*120*t)+
            .008*std::sin(2*pi*180*t)+city*(.031*std::sin(2*pi*43*t)+
            .018*loop_noise(t,1024,971))+.012*loop_noise(t,4096,701);
        break;
    }
    case Cue::DoorClose:{
        // Muted key ring, handle/latch, hinge air, then the weight of a door.
        for(unsigned i=0;i<5;++i){
            const double start=.008+.037*i;
            const double e=attack_decay(t,start,.14,38.);
            v+=e*(.037*std::sin(2*pi*(1610.+231.*i)*(t-start))+
                .020*std::sin(2*pi*(2813.+113.*i)*(t-start)));
        }
        v+=.07*attack_decay(t,.265,.10,47.)*noise(t,8400,311);
        v+=.023*smooth((t-.12)/.08)*smooth((.53-t)/.14)*noise(t,1050,443);
        const double age=std::max(0.,t-.53);
        const double thud=attack_decay(t,.53,.41,11.);
        v+=thud*(.25*std::sin(2*pi*(66.*age-19.*age*age))+
            .08*noise(t,490,601));
        v+=.045*attack_decay(t,.565,.12,36.)*noise(t,7100,337);
        break;
    }
    case Cue::BagDown:{
        const double age=std::max(0.,t-.14);
        v+=attack_decay(t,.14,.47,12.)*(.19*std::sin(2*pi*77*age)+.12*noise(t,780,151));
        v+=.095*smooth((t-.02)/.06)*smooth((.57-t)/.21)*
            (noise(t,1900,919)-.55*noise(t,340,921));
        v+=.042*attack_decay(t,.33,.22,14.)*noise(t,3100,121);
        break;
    }
    case Cue::ConsolePower:{
        // Plastic controller/button contact and a modest electronic power
        // confirmation. No orchestral chord or magical transport sound here.
        v+=.075*attack_decay(t,.012,.075,62.)*noise(t,7400,811);
        const double age=std::max(0.,t-.09);
        const double e=smooth(age/.017)*smooth((.31-t)/.075);
        if(t>=.09&&t<.31)v+=e*(.078*std::sin(2*pi*(310*age+42*age*age))+
            .020*std::sin(2*pi*(620*age+84*age*age)));
        break;
    }
    default:break;
    }
    return float(std::clamp(v,-.85,.85));
}
inline std::vector<float> waveform(Cue cue,unsigned rate){
    if(rate<8000||rate>192000||cue>=Cue::Count)return {};
    std::vector<float> pcm(std::size_t(std::llround(duration(cue)*rate)));
    // Remove DC without disturbing the periodic ambient seam. Finite Foley
    // additionally receives a short safety taper at both ends.
    double dc=0.;for(std::size_t i=0;i<pcm.size();++i){pcm[i]=sample(cue,double(i)/rate);dc+=pcm[i];}
    if(!pcm.empty()){
        dc/=double(pcm.size());
        for(std::size_t i=0;i<pcm.size();++i){
            const double edge=std::min(double(i),double(pcm.size()-1-i))/double(rate);
            pcm[i]=float((pcm[i]-dc)*(cue==Cue::Ambient?1.:smooth(edge/.004)));
        }
    }
    return pcm;
}
} // namespace detail
} // namespace world_opening_cinematic_audio

#ifndef VALWOOD_OPENING_AUDIO_WAVEFORM_ONLY
#include <SDL2/SDL_mixer.h>

struct WorldOpeningCinematicAudio {
    using Cue=world_opening_cinematic_audio::Cue;
    struct Clip {std::vector<Uint8> pcm;Mix_Chunk *chunk=nullptr;};
    struct Voice {int channel=-1;Mix_Chunk *chunk=nullptr;float gain=1;bool ambient=false;int prior_volume=MIX_MAX_VOLUME;};
    std::array<Clip,unsigned(Cue::Count)> clips={};
    std::array<Voice,8> voices={};
    std::array<unsigned,4> cue_starts={};
    unsigned starts=0;bool ready=false,allowed=false;float level=0,user_gain=0;
    WorldOpeningCinematicAudio()=default;
    WorldOpeningCinematicAudio(const WorldOpeningCinematicAudio&)=delete;
    WorldOpeningCinematicAudio& operator=(const WorldOpeningCinematicAudio&)=delete;
    ~WorldOpeningCinematicAudio(){release();}

    bool init(){
        release();int rate=0,channels=0;Uint16 format=0;
        if(!Mix_QuerySpec(&rate,&format,&channels)||rate<8000||rate>192000||channels<1||channels>8)return false;
        for(unsigned i=0;i<clips.size();++i){
            auto mono=world_opening_cinematic_audio::detail::waveform(Cue(i),unsigned(rate));
            SDL_AudioCVT cvt={};
            if(mono.empty()||SDL_BuildAudioCVT(&cvt,AUDIO_F32SYS,1,rate,format,Uint8(channels),rate)<0){release();return false;}
            const std::size_t bytes=mono.size()*sizeof(float);
            if(bytes>std::size_t(std::numeric_limits<int>::max())||cvt.len_mult<1){release();return false;}
            auto &clip=clips[i];clip.pcm.resize(bytes*std::size_t(cvt.len_mult));
            std::memcpy(clip.pcm.data(),mono.data(),bytes);
            cvt.buf=clip.pcm.data();cvt.len=int(bytes);
            if(cvt.needed&&SDL_ConvertAudio(&cvt)<0){release();return false;}
            const auto converted=cvt.needed?cvt.len_cvt:cvt.len;
            if(converted<=0){release();return false;}
            clip.pcm.resize(std::size_t(converted));
            clip.chunk=Mix_QuickLoad_RAW(clip.pcm.data(),Uint32(converted));
            if(!clip.chunk){release();return false;}
            Mix_VolumeChunk(clip.chunk,MIX_MAX_VOLUME);
        }
        ready=true;return true;
    }
    static bool owns(const Voice &v){
        return v.channel>=0&&v.chunk&&Mix_GetChunk(v.channel)==v.chunk&&Mix_Playing(v.channel);
    }
    static void restore(const Voice &v){
        if(v.channel>=0&&v.chunk&&Mix_GetChunk(v.channel)==v.chunk)Mix_Volume(v.channel,v.prior_volume);
    }
    void retire(){for(auto &v:voices)if(v.channel>=0&&!owns(v)){restore(v);v=Voice{};}}
    bool start(Cue cue,float gain,bool ambient){
        if(!ready||cue>=Cue::Count||!std::isfinite(gain))return false;
        retire();auto slot=std::find_if(voices.begin(),voices.end(),[](const Voice &v){return v.channel<0;});
        if(slot==voices.end())return false;
        auto *chunk=clips[unsigned(cue)].chunk;
        // Never halt or replace somebody else's channel. Other adapters may
        // subsequently replace this free-channel voice; owns() detects that.
        const int channel=Mix_PlayChannel(-1,chunk,ambient?-1:0);
        if(channel<0)return false;
        *slot={channel,chunk,std::clamp(gain,0.f,1.f),ambient,Mix_Volume(channel,-1)};
        Mix_Volume(channel,int(std::lround(MIX_MAX_VOLUME*slot->gain*(ambient?level:user_gain))));
        ++starts;++cue_starts[unsigned(cue)];return true;
    }
    bool play(Cue cue,float gain=1.f){
        return allowed&&cue!=Cue::Ambient&&start(cue,gain,false);
    }
    // Call before cue dispatch. userGain is the existing user-controlled sound
    // gain, not a replacement global mixer volume. paused includes menus,
    // minimization, loss of input focus and destination-loading gates.
    void updateAmbient(float dt,bool apartment,bool paused,float userGain){
        if(!ready)return;
        user_gain=std::isfinite(userGain)?std::clamp(userGain,0.f,1.f):0.f;
        allowed=apartment&&!paused&&user_gain>0;
        if(user_gain<=0)level=0; // The user's zero-volume choice is immediate.
        retire();
        const float target=allowed?.20f*user_gain:0.f;
        const float blend=1.f-std::exp(-std::clamp(std::isfinite(dt)?dt:0.f,0.f,.1f)*1.9f);
        level+=(target-level)*blend;
        bool have_ambient=false;
        for(auto &v:voices)if(owns(v)){
            if(v.ambient)have_ambient=true;
            if(paused){Mix_Pause(v.channel);continue;}
            if(Mix_Paused(v.channel))Mix_Resume(v.channel);
            // One-shot Foley does not carry into Valois or play under menus.
            if(!apartment&&!v.ambient){Mix_HaltChannel(v.channel);restore(v);v=Voice{};continue;}
            Mix_Volume(v.channel,int(std::lround(MIX_MAX_VOLUME*v.gain*(v.ambient?level:user_gain))));
            if(v.ambient&&!allowed&&level<.0005f){Mix_HaltChannel(v.channel);restore(v);v=Voice{};have_ambient=false;}
        }
        if(allowed&&!have_ambient)start(Cue::Ambient,1,true);
    }
    void release(){
        const bool mixer=Mix_QuerySpec(nullptr,nullptr,nullptr)!=0;
        if(mixer)for(auto &v:voices){if(owns(v))Mix_HaltChannel(v.channel);restore(v);}
        for(auto &v:voices)v=Voice{};
        for(auto &clip:clips){if(clip.chunk)Mix_FreeChunk(clip.chunk);clip.chunk=nullptr;clip.pcm.clear();}
        ready=false;allowed=false;level=user_gain=0;
    }
};
#endif
#endif

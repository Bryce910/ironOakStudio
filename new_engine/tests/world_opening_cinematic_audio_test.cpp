#define VALWOOD_OPENING_AUDIO_WAVEFORM_ONLY
#include "engine/world_opening_cinematic_audio.h"
#include <iostream>
namespace audio=world_opening_cinematic_audio;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<'\n';return 1;}}while(0)
int main(){
    CHECK(audio::detail::waveform(audio::Cue::Ambient,0).empty());
    CHECK(audio::detail::waveform(audio::Cue::Count,44100).empty());
    for(unsigned rate:{8000u,44100u,48000u,96000u})for(unsigned i=0;i<unsigned(audio::Cue::Count);++i){
        const auto cue=audio::Cue(i);const auto wave=audio::detail::waveform(cue,rate);
        CHECK(wave.size()==std::size_t(std::llround(audio::detail::duration(cue)*rate)));
        CHECK(wave==audio::detail::waveform(cue,rate));
        double peak=0,dc=0,energy=0;
        for(float v:wave){CHECK(std::isfinite(v));peak=std::max(peak,std::abs(double(v)));dc+=v;energy+=double(v)*v;}
        dc/=wave.size();energy=std::sqrt(energy/wave.size());
        CHECK(peak<.45);CHECK(peak>.06);CHECK(std::abs(dc)<.0002);CHECK(energy>.006&&energy<.09);
        if(cue==audio::Cue::Ambient){
            // End-to-start changes are no larger than normal adjacent samples.
            double adjacent=0;for(std::size_t n=1;n<wave.size();++n)adjacent=std::max(adjacent,std::abs(double(wave[n]-wave[n-1])));
            CHECK(std::abs(wave.back()-wave.front())<=adjacent+1e-6);
            CHECK(std::abs(audio::detail::sample(cue,0)-audio::detail::sample(cue,8))<1e-6);
        }else{CHECK(wave.front()==0);CHECK(wave.back()==0);}
        std::cout<<"cue "<<i<<" rate "<<rate<<" samples "<<wave.size()<<" peak "<<peak<<" rms "<<energy<<" dc "<<dc<<'\n';
    }
    std::cout<<"opening cinematic audio: "<<checks<<" finite, bounded, deterministic and seamless-loop checks passed\n";
}

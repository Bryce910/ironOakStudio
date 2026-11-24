// SDL's real loading-only format conversion, with an in-memory mixer double.
// No audio device, window, GPU or game project is opened by this test.
#include "engine/world_opening_cinematic_audio.h"
#include <cstdlib>
#include <iostream>
namespace {
struct Channel {Mix_Chunk *chunk=nullptr;bool playing=false,paused=false;int volume=79;};
std::array<Channel,6> channels;
bool mixer_open=true;unsigned allocated=0,freed=0;Uint16 output_format=AUDIO_S16SYS;
Channel *channel(int i){return i>=0&&i<int(channels.size())?&channels[unsigned(i)]:nullptr;}
}
extern "C" {
int Mix_QuerySpec(int *rate,Uint16 *format,int *count){if(!mixer_open)return 0;if(rate)*rate=44100;if(format)*format=output_format;if(count)*count=2;return 1;}
Mix_Chunk *Mix_QuickLoad_RAW(Uint8 *mem,Uint32 len){auto *c=static_cast<Mix_Chunk*>(std::calloc(1,sizeof(Mix_Chunk)));c->abuf=mem;c->alen=len;c->volume=128;++allocated;return c;}
void Mix_FreeChunk(Mix_Chunk *chunk){++freed;std::free(chunk);}
int Mix_VolumeChunk(Mix_Chunk *chunk,int volume){const int old=chunk->volume;if(volume>=0)chunk->volume=Uint8(volume);return old;}
Mix_Chunk *Mix_GetChunk(int n){auto *c=channel(n);return c?c->chunk:nullptr;}
int Mix_Playing(int n){auto *c=channel(n);return c&&c->playing;}
int Mix_PlayChannel(int n,Mix_Chunk *chunk,int){
    if(n<0){for(unsigned i=0;i<channels.size();++i)if(!channels[i].playing){n=int(i);break;}}
    auto *c=channel(n);if(!c)return -1;c->chunk=chunk;c->playing=true;c->paused=false;return n;
}
int Mix_PlayChannelTimed(int n,Mix_Chunk *chunk,int loops,int){return Mix_PlayChannel(n,chunk,loops);}
int Mix_Volume(int n,int volume){auto *c=channel(n);if(!c)return -1;const int old=c->volume;if(volume>=0)c->volume=volume;return old;}
void Mix_Pause(int n){if(auto *c=channel(n))c->paused=true;}
void Mix_Resume(int n){if(auto *c=channel(n))c->paused=false;}
int Mix_Paused(int n){auto *c=channel(n);return c&&c->paused;}
int Mix_HaltChannel(int n){if(auto *c=channel(n)){c->playing=false;c->paused=false;}return 0;}
}
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<'\n';return 1;}}while(0)
int main(){
    using Audio=WorldOpeningCinematicAudio;using Cue=Audio::Cue;
    Mix_Chunk unrelated={};channels[0]={&unrelated,true,false,79};
    Audio audio;CHECK(audio.init());CHECK(allocated==4);CHECK(audio.ready);
    CHECK(!audio.play(Cue::DoorClose)); // No focus authorization yet.
    CHECK((audio.starts==0&&audio.cue_starts==std::array<unsigned,4>{}));
    audio.updateAmbient(.05f,true,false,.6f);
    CHECK(channels[1].playing);CHECK(channels[1].volume>0);
    CHECK(audio.play(Cue::DoorClose));CHECK(channels[2].playing);
    CHECK(audio.starts==2&&audio.cue_starts[0]==1&&audio.cue_starts[1]==1&&audio.cue_starts[2]==0&&audio.cue_starts[3]==0);
    CHECK(!audio.play(Cue::Ambient));
    CHECK(channels[0].chunk==&unrelated&&channels[0].playing&&channels[0].volume==79);
    audio.updateAmbient(.05f,true,true,.6f);
    CHECK(channels[1].paused&&channels[2].paused);CHECK(!audio.play(Cue::BagDown));
    CHECK(!channels[0].paused);
    audio.updateAmbient(.05f,true,false,.6f);CHECK(!channels[1].paused&&!channels[2].paused);
    // A fixed channel owner may replace our free-channel cue at any point.
    // Updating/pausing/releasing must not touch that replacement.
    channels[2]={&unrelated,true,false,91};
    audio.updateAmbient(.05f,true,true,.6f);CHECK(!channels[2].paused&&channels[2].volume==91);
    audio.updateAmbient(.05f,true,false,.6f);
    CHECK(audio.play(Cue::BagDown));CHECK(channels[3].playing);
    const auto *finished_chunk=channels[3].chunk;channels[3].playing=false;
    audio.updateAmbient(.05f,true,false,.6f);
    CHECK(channels[3].chunk==finished_chunk&&channels[3].volume==79); // Restored for the next user.
    CHECK(audio.play(Cue::ConsolePower));
    audio.updateAmbient(.05f,true,false,0);
    CHECK(!channels[1].playing);CHECK(channels[1].volume==79);CHECK(channels[3].volume==0);
    CHECK(!audio.play(Cue::ConsolePower));
    audio.updateAmbient(.05f,false,false,.6f);CHECK(!channels[3].playing&&channels[3].volume==79);
    audio.release();CHECK(!audio.ready);CHECK(freed==allocated);
    CHECK(channels[0].playing&&channels[0].volume==79);
    CHECK(channels[2].playing&&channels[2].volume==91);
    for(auto &c:channels)c={&unrelated,true,false,80};
    CHECK(audio.init());const auto previous_starts=audio.cue_starts;const unsigned previous_total=audio.starts;
    audio.updateAmbient(.05f,true,false,1);CHECK(!audio.play(Cue::BagDown));
    CHECK(audio.cue_starts==previous_starts&&audio.starts==previous_total);
    for(const auto &c:channels)CHECK(c.chunk==&unrelated&&c.playing&&c.volume==80);
    audio.release();CHECK(freed==allocated);
    // The same source converts to a float-output mixer without an audio sink.
    for(auto &c:channels)c=Channel{};
    output_format=AUDIO_F32SYS;
    CHECK(audio.init());CHECK(audio.clips[0].pcm.size()==352800*2*sizeof(float));
    mixer_open=false;audio.release();CHECK(freed==allocated);
    CHECK(!audio.init());
    std::cout<<"opening cinematic mixer: "<<checks<<" conversion, channel ownership, focus/pause, zero volume and teardown checks passed\n";
}

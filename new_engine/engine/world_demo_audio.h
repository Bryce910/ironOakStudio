#ifndef VALWOOD_WORLD_DEMO_AUDIO_H
#define VALWOOD_WORLD_DEMO_AUDIO_H

#include <cstddef>
#include <cstdint>

enum class WorldDemoAudioSurface : unsigned { Soil, Stone, Wood, Wet, Count };
enum class WorldDemoAudioBed : unsigned { Wind, Forest, Creek, Fire, Rain, Market, Tavern, Night, Count };
constexpr unsigned WORLD_DEMO_AUDIO_BEDS=8;
constexpr unsigned WORLD_DEMO_AUDIO_STEP_VARIANTS=3;
constexpr unsigned WORLD_DEMO_AUDIO_VOICES=12;
constexpr unsigned WORLD_DEMO_AUDIO_FIRE_CAPACITY=256;

struct WorldDemoAudioFrame {
    double x=0,y=0,z=0,yaw=0;
    float dt=0,rain=0,wind=.5f,hour=16.65f;
    // Set false while airborne/flying. Actual XZ displacement sets cadence;
    // blocked movement, mouse look and idle cannot generate footsteps.
    bool grounded=false,running=false,focused=true,interior=false,covered=false;
    // A home or other quiet room keeps roof attenuation and wood footsteps
    // without the tavern crowd bed. Existing interior callers retain that bed.
    bool quiet_interior=false;
};
struct WorldDemoAudioFire {
    double x=0,y=0,z=0;
    float radius=14,strength=1;
};
struct WorldDemoAudioStats {
    bool ready=false,owns_device=false,focused=true;
    char driver[48]={};
    unsigned loaded_beds=0,loaded_steps=0,voice_capacity=WORLD_DEMO_AUDIO_VOICES,active_voices=0;
    std::size_t decoded_bytes=0;
    std::uint64_t updates=0,steps=0,rejected_jumps=0,zone_samples=0;
    double walked_m=0;
    float gains[WORLD_DEMO_AUDIO_BEDS]={},targets[WORLD_DEMO_AUDIO_BEDS]={};
    std::uint64_t surface_steps[unsigned(WorldDemoAudioSurface::Count)]={};
    WorldDemoAudioSurface surface=WorldDemoAudioSurface::Soil;
};
struct WorldDemoAudio;

// Owner thread. Loads only20 small PCM clips once, then uses8 loop voices
// and4 fixed footstep voices. Opens SDL audio/mixer only if not already open;
// an unavailable device is optional and returns null without affecting graphics.
// Assets are generated locally by tools/build_demo_audio.py with provenance.
WorldDemoAudio *world_demo_audio_create(const char *directory="audio/demo",char *error=nullptr,std::size_t capacity=0);
// Copies bounded emitters. Share the actual flame origins; no phantom fire
// sound sources are invented. Invalid input leaves the old set unchanged.
bool world_demo_audio_set_fires(WorldDemoAudio *,const WorldDemoAudioFire *,std::size_t count);
// Owner-thread ambience bus; leaves footsteps and other mixer owners alone.
bool world_demo_audio_set_ambience_gain(WorldDemoAudio *,float gain);
// No file IO, model queries or heap allocations. Zone reads are cached at~7Hz;
// exact exponential gain smoothing prevents frame-rate-dependent crossfades.
// Focus loss mutes immediately and discards stride debt; regain fades in.
bool world_demo_audio_update(WorldDemoAudio *,const WorldDemoAudioFrame &);
WorldDemoAudioStats world_demo_audio_stats(const WorldDemoAudio *);
void world_demo_audio_free(WorldDemoAudio *);

#endif

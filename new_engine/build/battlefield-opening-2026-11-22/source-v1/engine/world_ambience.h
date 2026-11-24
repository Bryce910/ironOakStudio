#ifndef VALWOOD_WORLD_AMBIENCE_H
#define VALWOOD_WORLD_AMBIENCE_H
#include <cstdint>
/* AAA merge: world-mode ambience. Looping campfire (positional), creek
 * (nearest water, sampled around the listener), forest wind bed, daytime bird
 * calls / dusk crickets as randomly placed one-shots, and hero footsteps
 * triggered by skeletal foot-plant events. Volume follows distance; stereo
 * pan follows direction relative to the camera. Owns its own mixer voices. */
struct WorldAmbience;
struct WorldAmbienceFrame {
    float dt=0;double listener[3]={};float yaw=0;     // camera position / yaw (forward = (-sin, -cos))
    bool hearth=false;double hearth_pos[3]={};          // active campfire
    float hour=12.f;                                    // 0..24
    std::uint64_t foot_plants=0;bool hero_grounded=true;double hero_pos[3]={};
    float sfx_volume=1.f;                               // 0..1 user SFX volume
    // Scene context. indoor silences every outdoor bed and wildlife call
    // (including one-shots already sounding) and starts none until outdoors
    // again; paused/unfocused/loading mute and halt one-shots. hero_steps=false
    // hands hero footsteps to another single owner (WorldDemoAudio).
    bool indoor=false,paused=false,focused=true,loading=false,hero_steps=true;
};
WorldAmbience *world_ambience_create(const char *audio_dir,const char *sound_effects_dir);
void world_ambience_update(WorldAmbience *,const WorldAmbienceFrame &);
void world_ambience_free(WorldAmbience *);
#endif

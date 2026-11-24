#ifndef VALWOOD_WORLD_CINEMATICS_AUDIO_H
#define VALWOOD_WORLD_CINEMATICS_AUDIO_H
#include "engine/world_cinematics.h"
#include "engine/world_soundscape.h"
#include <cstdint>
namespace world_cinematics_audio {
constexpr unsigned voice_limit=32,asset_limit=32,soundscape_voice_limit=16;
constexpr std::size_t default_pcm_budget=64u*1024u*1024u;
struct Audio;
struct Options {unsigned zone_voices=8,cue_voices=16;
    std::size_t pcm_budget=default_pcm_budget;float master=.35f;
    float reverb_feedback=.72f,reverb_damping=.28f,reverb_wet=.3f,cue_send=.15f;
    bool spatial_pan=true;};
struct FileAsset {std::string asset,path;};
struct Configuration {world_cinematics::Document document;std::vector<FileAsset> files;
    std::string outside_asset;unsigned outside_bus=0;float outside_gain=.3f;};
// Already converted into Mix_QuerySpec's current stereo device format/rate.
// Data is copied during explicit loading; callers may release it afterwards.
struct SoundscapeConfiguration {std::vector<world_soundscape::Emitter> emitters;std::vector<FileAsset> files;};
struct SoundscapeFrame {
    world_cinematics::CameraPose listener;double dt=0;
    bool focused=true,paused=false,loading=false,muted=false;float gain=1;
    std::uint64_t solo_id=0,audition_id=0,audition_serial=0;
};
struct SoundState {std::uint64_t id=0,cursor_frame=0;float gain=0,target_gain=0;
    bool present=false,inside=false,playing=false,virtualized=false;};
struct PcmAsset {std::string asset;const void *data=nullptr;std::size_t bytes=0;};
enum class UpdateResult {Ok,NotConfigured,InvalidFrame,QueueFull};
struct Stats {int first_channel=-1;unsigned channels=0,assets=0,active_voices=0;
    int frequency=0;std::uint16_t format=0;std::size_t decoded_bytes=0,allocated_pcm_bytes=0;
    std::uint64_t generation=0,acknowledged_generation=0,callbacks=0,rendered_frames=0,starts=0,queue_full=0;
    float outside_gain=1;bool retired_bank=false,soundscape_mode=false;unsigned authored_emitters=0,virtualized_emitters=0,soundscape_capacity=0;};
struct VoiceState {bool active=false;std::uint64_t id=0;unsigned category=0;
    std::uint64_t start_frame=0,cursor_frame=0,loop_start_frame=0,rendered_frames=0;
    float dry_gain=0,reverb_send=0;};
// One adapter per mixer. Requires an already open mixer. Supports exactly S16SYS/F32SYS stereo at
// 8..96kHz. Does not open/close devices, change global volume or replace hooks.
Audio *create(const Options &,std::string &error);
void destroy(Audio *);
// Loading only. All validation/decoding happens before the atomic queued
// commit. A failed configure preserves the old document AND playing voices.
// File path supports bounded RIFF PCM16/float32 WAV (mono/stereo,8..96kHz),
// converted with SDL_AudioCVT; compressed codecs are deliberately unsupported.
bool configure_files(Audio *,const Configuration &,std::string &error);
bool configure_pcm(Audio *,const Configuration &,const std::vector<PcmAsset> &,std::string &error);
// Loading-boundary union bank for published cutscenes and placed world sounds.
// One shared <=32-asset/64MiB bank, no per-sequence PCM copies. The host validates
// immutable documents once, then changes views without decoding or allocation.
bool configure_library_files(Audio *,const std::vector<FileAsset> &,std::string &error);
bool prepared_document_valid(const Audio *,const world_cinematics::Document &,std::string &error);
bool library_ready(const Audio *);
UpdateResult update_prepared_document(Audio *,const world_cinematics::Document &,
    const world_cinematics::Step &,const world_cinematics::CameraPose &,float gain=1);
UpdateResult update_soundscape_prepared(Audio *,const world_soundscape::Emitter *,std::size_t,const SoundscapeFrame &);
// Alternative world mode on the SAME adapter. Explicit preparation decodes WAVs
// transactionally; failed loads preserve the previous mode and playing bank.
// Default adapter capacity25 permits16 audible world voices; custom smaller
// adapters use min(16,owned channels). Up to64 stable emitters share32 assets.
bool configure_soundscape_files(Audio *,const SoundscapeConfiguration &,std::string &error);
// No IO, allocations or locks. Edits to prepared asset positions/parameters take
// effect on this update. New asset paths require explicit preparation first.
// Focus/pause/loading/mute silence immediately and freeze logical cursors.
// Virtualized loops retain phase. Nonloops play once on entry; leaving then
// reentering retriggers. Audition isolates/centers a source irrespective of range;
// changing audition_serial retriggers it; audition also permits a disabled node.
// Weather condition_gain multiplies the fade target without changing spatial
// entry/one-shot identity; audition bypasses conditions.
// Solo retains world attenuation/pan and respects enabled.
UpdateResult update_soundscape(Audio *,const world_soundscape::Emitter *,std::size_t,const SoundscapeFrame &);
// Owner-thread diagnostic state (including virtualized voices), not callback state.
bool sound(const Audio *,std::uint64_t id,SoundState &);
// Main-thread update after the camera actually rendered has been determined.
// Samples authored zones/envelopes/cues; no file IO, decoding, heap allocation,
// audio lock or blocking wait. Pause/seek/wrap resynchronize only owned voices.
// Destination-active cues seek to elapsed source time. A cue wholly between
// render updates is obsolete and is not replayed late; this is not an audio
// look-ahead scheduler for guaranteed sub-frame one-shot delivery.
UpdateResult update(Audio *,const world_cinematics::Step &,const world_cinematics::CameraPose &rendered_camera);
bool stop_all(Audio *); // queued mute/reset of this adapter, not demo/music
// Loading-only reclamation. A previous decoded bank remains bounded/alive
// until the callback acknowledges the replacement generation.
bool collect(Audio *);
Stats stats(const Audio *);
bool voice(const Audio *,unsigned slot,VoiceState &);
// Integration: create after demo audio -> configure at loading -> update with
// timeline Step + rendered camera each frame -> destroy BEFORE mixer teardown.
// The adapter holds zone_voices+cue_voices+1 appended channels with permanent
// silent chunks, preventing Mix_PlayChannel(-1) from stealing its owned slots.
// One registered posteffect adds our voices/reverb without replacing existing
// Mix_SetPostMix or effects. PCM cursors address whole interleaved frames; cue
// loops run from authored source_offset to clip end (never wrap to PCM zero).
// Peak PCM at replacement is at most two banks, each <=pcm_budget. DSP uses
// fixed Schroeder rings; idle buses skip DSP, and tails retire after12 seconds
// without input. Tail buffers and command queue allocate only at create.
}
#endif

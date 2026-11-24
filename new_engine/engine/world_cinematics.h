#ifndef VALWOOD_WORLD_CINEMATICS_H
#define VALWOOD_WORLD_CINEMATICS_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// CPU authoring/runtime math only: no OpenGL, SDL, decoding or audio device.
// Validate/replace documents at a loading boundary. Sampling and scheduling a
// validated immutable document allocate nothing and never access files.
namespace world_cinematics {
constexpr unsigned camera_limit=256,zone_limit=64,clip_limit=128,bus_limit=8;
constexpr unsigned envelope_key_limit=64,event_limit=clip_limit*4+1;
constexpr std::size_t document_byte_limit=1024*1024;
enum class Curve : unsigned { Linear, Smooth, Hold };
enum class Shape : unsigned { Sphere, Box };
struct Vec3 {double x=0,y=0,z=0;};
struct Quaternion {double x=0,y=0,z=0,w=1;};
struct CameraPose {Vec3 position;Quaternion rotation;double fov_degrees=61.879441874;};
struct CameraKey {double time=0;CameraPose pose;Curve curve=Curve::Smooth;};
struct GainKey {double time=0;float gain=1;Curve curve=Curve::Linear;};
struct BusEnvelope {unsigned bus=0;std::vector<GainKey> keys;};
struct Zone {
    unsigned id=1,bus=0;std::string name="Ambience",asset;
    Shape shape=Shape::Box;int priority=0;Vec3 center,extent={8,4,8};
    // Sphere uses extent.x as radius; Box uses all half extents. Feather is
    // symmetric about the boundary: full inside -blend, zero outside +blend.
    double blend=3;float gain=1,dry=1,reverb=.2f;
};
struct Clip {
    unsigned id=1,bus=0;std::string asset;double start=0,duration=1,source_offset=0;
    float gain=1,fade_in=.05f,fade_out=.1f;bool loop=false;
};
struct Document {
    std::string name="Cinematic";double duration=10;bool loop=false;
    std::vector<CameraKey> camera;std::vector<Zone> zones;
    std::vector<Clip> clips;std::vector<BusEnvelope> envelopes;
};
bool validate(const Document &,std::string &error);
bool save(const char *path,const Document &,std::string &error);
// On every read/parse/validation failure, the previous document stays intact.
bool load(const char *path,Document &,std::string &error);

// Camera convention matches the world viewer: zero looks -Z, +yaw turns +X,
// +pitch looks up. Angles are radians; sampled FOV is vertical degrees.
Quaternion orientation(double yaw,double pitch,double roll=0);
Vec3 forward(Quaternion);
Vec3 up(Quaternion);
bool sample_camera(const Document &,double time,CameraPose &);
float sample_bus(const Document &,unsigned bus,double time);

struct ZoneVoice {
    unsigned id=0,bus=0;const char *asset=nullptr;
    float weight=0,gain=0,dry_gain=0,reverb_send=0;
};
struct BusSends {float dry_gain=0,reverb_send=0;};
struct ZoneMix {
    std::array<ZoneVoice,zone_limit> voices={};unsigned count=0;
    std::array<BusSends,bus_limit> buses={};float outside_gain=1;
};
// Highest priorities consume available *power*. Equal-priority overlaps share
// that power. sum(weight^2)+outside_gain^2=1; low priorities smoothly reappear
// at higher-priority boundaries. Sends are requested amplitudes, not DSP.
bool sample_zones(const Document &,Vec3 listener,double time,ZoneMix &);
struct ClipVoice {
    unsigned id=0,bus=0;const char *asset=nullptr;double source_offset=0;
    float gain=0;bool loop=false;
};
struct Mix {
    ZoneMix zones;std::array<ClipVoice,clip_limit> clips={};unsigned clip_count=0;
    std::array<float,bus_limit> buses={};
};
bool sample_mix(const Document &,Vec3 listener,double time,Mix &);

struct Step {
    double previous=0,time=0,elapsed=0;
    std::uint64_t previous_cycle=0,cycle=0,wraps=0;
    bool jumped=true,paused=false,finished=false;
};
class Transport {
public:
    bool configure(double duration,bool loop);
    bool advance(double absolute_monotonic_seconds,Step &);
    bool seek(double timeline_seconds); // finite input clamped to [0,duration]
    void pause(bool paused); // primes next tick; paused wall time never accrues
    void looping(bool loop);
    double time()const{return time_;}
    bool paused()const{return paused_;}
    bool loop()const{return loop_;}
    std::uint64_t cycle()const{return cycle_;}
private:
    double duration_=0,time_=0,last_clock_=0;
    std::uint64_t cycle_=0;
    bool loop_=false,paused_=false,seen_clock_=false,anchor_=false,jumped_=true;
};

enum class EventKind : unsigned { StopAll,Stop,Start };
struct Event {
    EventKind kind=EventKind::StopAll;unsigned id=0,bus=0;const char *asset=nullptr;
    double timeline_time=0,source_offset=0,step_offset=0;std::uint64_t cycle=0;
    float gain=0;bool loop=false;
};
struct Schedule {std::array<Event,event_limit> events={};unsigned count=0;bool resync=false;};
// Normal ticks retain chronological start/stop crossings, including one wrap.
// A scrub/pause or a >1-wrap jump stops owned voices then restores only voices
// active at the destination, avoiding a burst of obsolete one-shot sounds.
// Start offset includes elapsed cue time, allowing the host to seek decoded PCM.
// The host owns channels, pauses/starts/stops real clips and implements reverb.
bool schedule(const Document &,const Step &,Schedule &);
}
#endif

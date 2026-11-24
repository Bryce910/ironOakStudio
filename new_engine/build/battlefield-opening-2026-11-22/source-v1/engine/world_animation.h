#ifndef VALWOOD_WORLD_ANIMATION_H
#define VALWOOD_WORLD_ANIMATION_H
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// General CPU skeletal animation, independent of the current prebaked-pose
// renderer. Imported skin/mesh data and matrices are real; GPU skinning and an
// animation-editor integration are deliberately separate host responsibilities.
namespace world_animation {
// The four accepted weapon banks retain up to64 combat/equipment clips.
// Leave a bounded allowance for the same body's civilian swim/death captures.
constexpr unsigned node_limit=256,joint_limit=128,clip_limit=96,channel_limit=768,key_limit=16384,event_limit=64,batch_limit=256;
struct Vec3 {float x=0,y=0,z=0;};
struct Quat {float x=0,y=0,z=0,w=1;};
struct Transform {Vec3 translation;Quat rotation;Vec3 scale{1,1,1};};
struct Mat4 {std::array<float,16> m={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}; // column-major
struct Joint {std::string name;int parent=-1;Transform rest;};
struct Skeleton {std::vector<Joint> joints;std::vector<unsigned> palette;std::vector<Mat4> inverse_bind;unsigned mesh_node=0;};
enum class Path {Translation,Rotation,Scale};
enum class Interpolation {Linear,Step,CubicSpline};
struct Channel {unsigned node=0;Path path=Path::Translation;Interpolation interpolation=Interpolation::Linear;std::vector<float> times;std::vector<std::array<float,4>> values;};
struct ClipEvent {std::string name;double time=0;};
struct Clip {std::string name;double duration=0;std::vector<Channel> channels;std::vector<ClipEvent> events;};
struct SkinVertex {Vec3 position,normal{0,1,0};std::array<float,2> uv={};std::array<unsigned,4> joints={};std::array<float,4> weights={1,0,0,0};std::array<float,4> tangent={1,0,0,1};};
constexpr unsigned morph_limit=32,morph_vertex_limit=16384;
struct MorphDelta {unsigned vertex=0;Vec3 position,normal,tangent;};
struct MorphTarget {std::string name;std::vector<MorphDelta> deltas;};
struct Primitive {std::size_t first_index=0,index_count=0;int material=-1;bool has_normals=false,has_uv=false,has_tangents=false;};
struct SkinMesh {std::vector<SkinVertex> vertices;std::vector<std::uint32_t> indices;std::vector<Primitive> primitives;};
struct Image {std::vector<unsigned char> encoded;};
struct TextureMap {int image=-1;unsigned min_filter=9987,mag_filter=9729,wrap_s=10497,wrap_t=10497;};
struct Material {
    TextureMap base,normal,metallic_roughness,occlusion,emissive;
    std::array<float,4> base_factor={1,1,1,1};std::array<float,3> emissive_factor={0,0,0};
    float roughness=1,metallic=1,normal_scale=1,occlusion_strength=1,alpha_cutoff=0;
    bool double_sided=false;
    unsigned hair_material=0; // Owned source contract:0ordinary,1OrcMASK,2knightBLEND,3ThomasCards,4ThomasHdrpCards.
    bool organic_body=false; // Explicit owned head; separate eyes/mouth stay ordinary.
};
struct Asset {Skeleton skeleton;std::vector<Clip> clips;SkinMesh mesh;std::vector<Image> images;std::vector<Material> materials;std::vector<MorphTarget> morph_targets;std::string source_path;};
struct Pose {unsigned nodes=0,joints=0;std::array<Transform,node_limit> local;std::array<Mat4,node_limit> global;std::array<Mat4,joint_limit> skin;std::array<float,morph_limit> morph_weights={};};

bool validate(const Asset &,std::string &error);
// Embedded GLB2, first skin and one mesh node, triangle primitives, 4 influences,
// float TRS channels (LINEAR/STEP/CUBICSPLINE), and bounded named authored
// POSITION/NORMAL/TANGENT morphs (sparse or dense). Sparse base attributes,
// animated glTF weight channels, Draco/meshopt or >4 influences reject. No external buffer loading.
// Failed import leaves the caller's prior Asset untouched.
bool load_glb(const char *path,Asset &,std::string &error);
bool rest_pose(const Skeleton &,Pose &,std::string &error);
bool evaluate(const Skeleton &,Pose &,std::string &error);
// Assets must be validated after authoring/import. Sampling uses binary search
// and fixed Pose storage; no allocation or file IO on a successful sample.
bool sample(const Skeleton &,const Clip &,double seconds,bool loop,Pose &,std::string &error);
bool sample_batch(const Asset &,unsigned clip,const double *times,std::size_t count,bool loop,Pose *,std::string &error);
struct Blend {unsigned clip=0;double seconds=0;float weight=0;};
bool blend(const Asset &,const Blend *,unsigned count,bool loop,Pose &,std::string &error);
struct BlendPoint {float x=0,y=0;unsigned clip=0;};
struct Triangle {std::array<unsigned,3> points={};};
bool weights_1d(const std::vector<BlendPoint> &,float x,std::array<float,16> &,std::string &error);
// Authored triangulation, barycentric inside and nearest edge outside. This
// supports directional locomotion with continuous weights, not a label-only UI.
bool weights_2d(const std::vector<BlendPoint> &,const std::vector<Triangle> &,float x,float y,std::array<float,16> &,std::string &error);
struct EventHit {unsigned index=0;double absolute_time=0;};
struct EventBuffer {std::array<EventHit,event_limit> hits={};unsigned count=0;std::uint64_t dropped=0;};
// (previous,current] intervals, chronological including crossed loops. Initial
// time-zero events are explicit host startup actions. Bounded output/work even
// for many missed loops; dropped counts remain visible.
bool events_between(const Clip &,double previous,double current,bool loop,EventBuffer &,std::string &error);
struct Playback {unsigned clip=0,from_clip=0;double time=0,from_time=0,fade_duration=0,fade_elapsed=0;bool loop=true,fading=false,source_frozen=false;std::array<Transform,node_limit> frozen;};
bool transition(const Asset &,Playback &,unsigned target,double duration,std::string &error,bool restart=true);
bool advance(const Asset &,Playback &,double dt,Pose &,EventBuffer &,std::string &error);
struct Rigid {Vec3 translation;Quat rotation;};
bool root_motion(const Skeleton &,const Clip &,unsigned node,double previous,double current,bool loop,Rigid &,std::string &error);
bool strip_root_motion(const Skeleton &,unsigned node,Pose &,std::string &error,bool keep_vertical=true);
struct Socket {std::string joint;Transform local;};
bool socket_transform(const Skeleton &,const Pose &,const Socket &,Mat4 &,std::string &error);
struct RetargetMap {unsigned target_nodes=0;std::array<int,node_limit> source_for_target;float translation_scale=1;RetargetMap(){source_for_target.fill(-1);}};
bool retarget_map(const Skeleton &source,const Skeleton &target,RetargetMap &,std::string &error,float translation_scale=1);
bool retarget(const Skeleton &source,const Pose &,const Skeleton &target,const RetargetMap &,Pose &,std::string &error);
// Two-bone analytic IK in world/skeleton coordinates, preserving bone lengths.
// Bone scales must be uniform and positive along this chain. Terrain callback
// is optional and can reject a sample; no physics/GL/global state is consulted.
using TerrainHeight=bool (*)(double x,double z,double &height,void *userdata);
struct LegIK {unsigned hip=0,knee=0,ankle=0;Vec3 target,pole{0,0,1};float weight=1,sole_offset=0;};
bool two_bone_ik(const Skeleton &,Pose &,const LegIK &,std::string &error,TerrainHeight terrain=nullptr,void *userdata=nullptr);
// CPU reference deformation in the skinned mesh node's local space. Correct
// inverse-transpose normals for the blended skin matrix; useful for bake/tests.
bool morph_vertex(const Asset &,unsigned vertex,const Pose &,SkinVertex &,std::string &error);
bool deform_vertex(const SkinVertex &,const Pose &,Vec3 &position,Vec3 &normal,std::string &error);
Vec3 transform_point(const Mat4 &,Vec3);
}
#endif

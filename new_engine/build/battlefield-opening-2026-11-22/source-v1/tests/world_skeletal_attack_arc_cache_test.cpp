#include "engine/world_skeletal_attack_arc_cache.h"
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
using namespace world_animation;
using world_skeletal_attack_arc_cache::Inputs;
using world_skeletal_attack_arc_cache::compatible;
static unsigned checks=0,rejections=0;
static void require(bool value,const char*message){++checks;if(!value){std::cerr<<"FAIL "<<message<<'\n';std::exit(1);}}
static Asset fixture(unsigned body_parts,unsigned body_vertices){
 Asset a;a.skeleton.joints={{"root",-1,{}},{"hand_r",0,{}}};a.skeleton.palette={0,1};a.skeleton.inverse_bind.resize(2);a.skeleton.mesh_node=0;
 Clip clip;clip.name="Attack_Left";clip.duration=1;Channel channel;channel.node=1;channel.path=Path::Rotation;channel.times={0,1};channel.values={{{0,0,0,1}},{{.2f,0,0,.98f}}};clip.channels.push_back(channel);clip.events.push_back({"contact",.5});a.clips.push_back(clip);
 a.mesh.vertices.resize(body_vertices);a.mesh.primitives.resize(body_parts);
 for(unsigned n=0;n<4;++n){SkinVertex v;v.position={float(n),float(n*n),0};v.joints={1,0,0,0};a.mesh.vertices.push_back(v);}
 for(unsigned i:{0u,1u,2u,0u,2u,3u})a.mesh.indices.push_back(body_vertices+i);
 Primitive p;p.index_count=6;p.has_normals=p.has_uv=p.has_tangents=true;a.mesh.primitives.push_back(p);return a;
}
static Inputs inputs(const Asset&a,unsigned held){Inputs i;i.asset=&a;i.held_begin=held;i.pelvis=0;i.spine=i.hand=i.right_hand=1;return i;}
int main(){
 const Asset original=fixture(19,23),replacement=fixture(9,7);const auto left=inputs(original,19);
 require(compatible(left,left),"identical inputs accepted");require(compatible(left,inputs(replacement,9)),"body topology and absolute held offsets may differ");
 auto reject_asset=[&](const std::function<void(Asset&)>&change,const char*message){Asset b=replacement;change(b);require(!compatible(left,inputs(b,9)),message);++rejections;};
 reject_asset([](Asset&b){b.skeleton.joints[1].name="hand_changed";},"joint name differs");
 reject_asset([](Asset&b){b.skeleton.joints[1].parent=-1;},"joint parent differs");
 reject_asset([](Asset&b){b.skeleton.joints[1].rest.translation.x=std::nextafter(0.f,1.f);},"one-bit rest translation differs");
 reject_asset([](Asset&b){b.skeleton.joints[1].rest.rotation.w=std::nextafter(1.f,2.f);},"one-ulp rest rotation differs");
 reject_asset([](Asset&b){b.skeleton.joints[1].rest.scale.y=std::nextafter(1.f,2.f);},"one-ulp rest scale differs");
 reject_asset([](Asset&b){b.skeleton.joints[1].rest.translation.z=-0.f;},"signed zero transform differs");
 reject_asset([](Asset&b){std::swap(b.skeleton.palette[0],b.skeleton.palette[1]);},"palette differs");
 reject_asset([](Asset&b){b.skeleton.inverse_bind[1].m[12]=std::nextafter(0.f,1.f);},"inverse bind differs");
 reject_asset([](Asset&b){b.skeleton.mesh_node=1;},"mesh transform node differs");
 reject_asset([](Asset&b){b.clips[0].name="Attack_Right";},"clip name differs");
 reject_asset([](Asset&b){b.clips[0].duration=std::nextafter(1.,2.);},"one-ulp duration differs");
 reject_asset([](Asset&b){b.clips[0].channels[0].node=0;},"channel node differs");
 reject_asset([](Asset&b){b.clips[0].channels[0].path=Path::Translation;},"channel path differs");
 reject_asset([](Asset&b){b.clips[0].channels[0].interpolation=Interpolation::Step;},"channel interpolation differs");
 reject_asset([](Asset&b){b.clips[0].channels[0].times[1]=std::nextafter(1.f,2.f);},"one-ulp key time differs");
 reject_asset([](Asset&b){b.clips[0].channels[0].values[1][0]=std::nextafter(.2f,1.f);},"one-ulp key value differs");
 reject_asset([](Asset&b){b.clips[0].events[0].name="changed";},"clip event name differs");
 reject_asset([](Asset&b){b.clips[0].events[0].time=std::nextafter(.5,1.);},"clip event time differs");
 reject_asset([](Asset&b){b.clips.push_back(b.clips[0]);},"additional clip differs");
 reject_asset([](Asset&b){b.mesh.vertices[7].position.x=std::nextafter(0.f,1.f);},"held position differs");
 reject_asset([](Asset&b){b.mesh.vertices[7].normal.y=std::nextafter(1.f,2.f);},"held normal differs");
 reject_asset([](Asset&b){b.mesh.vertices[7].uv[1]=std::nextafter(0.f,1.f);},"held UV differs");
 reject_asset([](Asset&b){b.mesh.vertices[7].joints[0]=0;},"held skin joint differs");
 reject_asset([](Asset&b){b.mesh.vertices[7].weights[0]=std::nextafter(1.f,2.f);},"held skin weight differs");
 reject_asset([](Asset&b){b.mesh.vertices[7].tangent[3]=-1;},"held tangent differs");
 reject_asset([](Asset&b){b.mesh.indices[1]=9;},"held triangle topology differs");
 reject_asset([](Asset&b){b.mesh.vertices.push_back(b.mesh.vertices[7]);b.mesh.indices[3]=11;},"duplicate equal vertex may not change shared topology");
 reject_asset([](Asset&b){b.mesh.indices[1]=9999;},"invalid held index rejected");
 reject_asset([](Asset&b){b.mesh.primitives[9].first_index=std::numeric_limits<std::size_t>::max();},"invalid held range rejected");
 reject_asset([](Asset&b){b.mesh.primitives[9].index_count=std::numeric_limits<std::size_t>::max();},"overflowing held range rejected");
 reject_asset([](Asset&b){b.mesh.primitives[9].has_tangents=false;},"held attribute presence differs");
 reject_asset([](Asset&b){b.mesh.primitives.push_back(b.mesh.primitives.back());},"held primitive count differs");
 const auto right=inputs(replacement,9);
 auto reject_context=[&](const std::function<void(Inputs&)>&change,const char*message){auto changed=right;change(changed);require(!compatible(left,changed),message);++rejections;};
 reject_context([](Inputs&i){i.clips[10]=1;},"selected attack clip differs");reject_context([](Inputs&i){i.pelvis=1;},"pelvis index differs");
 reject_context([](Inputs&i){i.spine=0;},"spine index differs");reject_context([](Inputs&i){i.hand=0;},"socket index differs");
 reject_context([](Inputs&i){i.right_hand=0;},"wrist index differs");reject_context([](Inputs&i){i.grip_profile=1;},"weapon family differs");
 reject_context([](Inputs&i){i.stationary_cut_adaptation=true;},"source-family adaptation differs");reject_context([](Inputs&i){i.asset=nullptr;},"missing source rejected");
 reject_context([](Inputs&i){i.held_begin=10;},"no held gear rejected");
 Asset cosmetic=replacement;cosmetic.mesh.vertices[0].position.x=999;cosmetic.mesh.primitives[0].index_count=0;cosmetic.mesh.primitives[9].material=92;cosmetic.materials.resize(93);cosmetic.images.push_back({{1,2,3}});cosmetic.source_path="different-outfit.glb";
 require(compatible(left,inputs(cosmetic,9)),"body, materials, image bytes and filename do not affect combat table");
 // Index/vertex buffer reordering that preserves exact held incidence is safe.
 Asset reordered=replacement;std::swap(reordered.mesh.vertices[7],reordered.mesh.vertices[8]);for(auto&i:reordered.mesh.indices){if(i==7)i=8;else if(i==8)i=7;}
 require(compatible(left,inputs(reordered,9)),"equivalent held vertex relabeling accepted");
 std::cout<<"{\"passed\":true,\"gpu_used\":false,\"exact_input_assertions\":"<<checks<<",\"incompatible_inputs_rejected\":"<<rejections<<"}\n";
}

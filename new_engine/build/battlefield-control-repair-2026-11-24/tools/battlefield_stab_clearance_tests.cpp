// Actual purchased civilian/knight rig, skinned torso and visible steel line.
// CPU-only geometry evidence; renderer stubs cannot produce a visual pass.
#include "engine/world_battlefield_actors.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>
namespace world_animation_render {
Renderer*create(const world_animation::Asset&,unsigned,std::string&,std::uint64_t){return nullptr;}void destroy(Renderer*){}bool warm(Renderer*,unsigned,std::string&){return false;}bool upload(Renderer*,const world_animation::Pose*,const Instance*,std::size_t,const Bindings&,std::string&){return false;}bool draw(Renderer*,unsigned,const Bindings&,std::string&){return false;}Stats stats(const Renderer*){return{};}unsigned previous_palette(const Renderer*){return 0;}int previous_palette_offset(const Renderer*){return 0;}
}
using A=WorldBattlefieldActors;using V=world_animation::Vec3;
static V sub(V a,V b){return{a.x-b.x,a.y-b.y,a.z-b.z};}static float dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}static V cross(V a,V b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static V vec(const std::array<double,3>&a){return{float(a[0]),float(a[1]),float(a[2])};}
static float dist(V a,V b){return std::sqrt(dot(sub(a,b),sub(a,b)));}
static V world(V q,const world_animation_render::Instance&i){return{float(i.x+std::cos(i.yaw)*q.x+std::sin(i.yaw)*q.z),float(i.y+q.y),float(i.z-std::sin(i.yaw)*q.x+std::cos(i.yaw)*q.z)};}
static bool segment(V from,V to,V a,V b,V c){V d=sub(to,from),e=sub(b,a),f=sub(c,a),h=cross(d,f);double det=dot(e,h);if(std::abs(det)<1e-10)return false;double inv=1/det;V s=sub(from,a);double u=dot(s,h)*inv;if(u<0||u>1)return false;V q=cross(s,e);double v=dot(d,q)*inv;if(v<0||u+v>1)return false;double t=dot(f,q)*inv;return t>1e-4&&t<1-1e-4;}
static double line_distance(V a,V b,V p){V d=sub(b,a);double t=std::clamp(double(dot(sub(p,a),d)/dot(d,d)),0.,1.);return dist({a.x+float(t)*d.x,a.y+float(t)*d.y,a.z+float(t)*d.z},p);}
int main(int argc,char**argv){if(argc<3)return 2;const std::string n=argv[1];std::string e;A actors;
 if(!actors.load_cpu((n+"/motion/rigs/sword.glb").c_str(),(n+"/battlefield-motion/captures.glb").c_str(),(n+"/character-quality/High/story_hero/civilian/body.glb").c_str(),(n+"/arena-project/knights_roster/knight09/body.glb").c_str(),e)){std::cerr<<e;return 1;}
 const auto&asset=actors.asset(A::Role::Player);std::vector<bool> torso(asset.mesh.vertices.size());
 for(unsigned j=0;j<torso.size();++j){float w=0;const auto&v=asset.mesh.vertices[j];for(unsigned k=0;k<4;++k){auto&name=asset.skeleton.joints[asset.skeleton.palette[v.joints[k]]].name;if(name=="pelvis"||name.rfind("spine_",0)==0)w+=v.weights[k];}torso[j]=w>.50f;}
 std::vector<std::array<unsigned,3>> tris;std::set<unsigned> used;
 for(unsigned j=0;j<asset.mesh.indices.size();j+=3){std::array<unsigned,3> t{asset.mesh.indices[j],asset.mesh.indices[j+1],asset.mesh.indices[j+2]};if(torso[t[0]]&&torso[t[1]]&&torso[t[2]]){tris.push_back(t);used.insert(t.begin(),t.end());}}
 if(tris.size()<200){std::cerr<<"Missing real skinned torso triangles";return 2;}
 unsigned samples=0,intersections=0,intersection_frames=0;double max_step=0,max_offhand=0,max_wrist_blade_base=0,contact=100,recover_pop=100;V previous{};bool have=false;std::vector<V> vertices(asset.mesh.vertices.size());
 A::Input in;in.dt=1./240.;in.beat=A::Beat::Struggle;in.progress=1;in.struggle=1;in.seconds=7.5;
 if(!actors.update_cpu(in,e)){std::cerr<<e;return 2;}
 std::ofstream trace(std::string(argv[2])+".csv");trace<<"beat,progress,self_intersections,right_x,right_y,right_z,chest_x,chest_y,chest_z,victim_distance\n";
 for(auto beat:{A::Beat::Stab,A::Beat::Recover})for(unsigned k=0,steps=unsigned(std::ceil((beat==A::Beat::Stab?1.48:1.10)*240));k<=steps;++k){in.beat=beat;in.progress=double(k)/steps;in.seconds=in.progress*(beat==A::Beat::Stab?1.48:1.10);in.dt=(beat==A::Beat::Stab?1.48:1.10)/steps;if(!actors.update_cpu(in,e)){std::cerr<<e;return 2;}++samples;const auto&pose=actors.pose(A::Role::Player);const auto&inst=actors.instance(A::Role::Player);const auto&h=actors.anchors(A::Role::Player);const auto&v=actors.anchors(A::Role::Attacker);unsigned hits=0;
  for(auto j:used){V q,norm;if(!world_animation::deform_vertex(asset.mesh.vertices[j],pose,q,norm,e))return 2;vertices[j]=world(world_animation::transform_point(pose.global[asset.skeleton.mesh_node],q),inst);}
  for(const auto&t:tris)hits+=segment(vec(h.right_hand),vec(h.blade_tip),vertices[t[0]],vertices[t[1]],vertices[t[2]]);
  intersections+=hits;intersection_frames+=hits!=0;if(have){double step=dist(previous,vec(h.right_hand));if(beat==A::Beat::Recover&&k==0)recover_pop=step;else max_step=std::max(max_step,step);}previous=vec(h.right_hand);have=true;
  if(beat==A::Beat::Stab){max_offhand=std::max(max_offhand,double(dist(vec(h.right_hand),vec(h.left_hand))));max_wrist_blade_base=std::max(max_wrist_blade_base,double(dist(vec(h.right_hand),vec(h.blade_base))));}double target=line_distance(vec(h.blade_base),vec(h.blade_tip),vec(v.chest));
  trace<<unsigned(beat)<<','<<in.progress<<','<<hits<<','<<h.right_hand[0]<<','<<h.right_hand[1]<<','<<h.right_hand[2]<<','<<h.chest[0]<<','<<h.chest[1]<<','<<h.chest[2]<<','<<target<<'\n';
 }
 in.beat=A::Beat::Stab;in.progress=.64/1.48;in.seconds=.64;if(!actors.update_cpu(in,e))return 2;const auto&hit=actors.anchors(A::Role::Player);const auto&victim=actors.anchors(A::Role::Attacker);contact=line_distance(vec(hit.blade_base),vec(hit.blade_tip),vec(victim.chest));
 const double steel_length=dist(vec(hit.blade_base),vec(hit.blade_tip));
 const bool passed=intersections==0&&contact<.015&&recover_pop<.025&&max_step<.08&&max_offhand<.12&&max_wrist_blade_base<.09&&steel_length>.95&&steel_length<1.;
 std::ofstream out(argv[2]);out<<"{\n\"passed\":"<<(passed?"true":"false")<<",\n\"actual_rig_samples\":"<<samples<<",\n\"minimum_sampling_hz\":240,\n\"skinned_torso_triangles\":"<<tris.size()<<",\n\"self_intersections\":"<<intersections<<",\n\"self_intersection_frames\":"<<intersection_frames<<",\n\"contact_distance_m\":"<<contact<<",\n\"recover_hand_pop_m\":"<<recover_pop<<",\n\"max_hand_step_240hz_m\":"<<max_step<<",\n\"max_two_hand_gap_m\":"<<max_offhand<<",\n\"max_right_wrist_to_blade_base_m\":"<<max_wrist_blade_base<<",\n\"actual_source_blade_length_m\":"<<steel_length<<",\n\"rendering_tested\":false,\n\"native_visual_review_required\":true\n}\n";
 std::cout<<(passed?"PASS ":"FAIL ")<<samples<<" actual rig samples, torso triangles "<<tris.size()<<", self intersections "<<intersections<<" (frames "<<intersection_frames<<"), contact "<<contact<<", hand step "<<max_step<<", offhand "<<max_offhand<<", recover pop "<<recover_pop<<'\n';return passed?0:1;
}

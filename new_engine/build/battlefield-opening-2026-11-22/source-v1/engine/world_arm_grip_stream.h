#ifndef VALWOOD_WORLD_ARM_GRIP_STREAM_H
#define VALWOOD_WORLD_ARM_GRIP_STREAM_H
#include "world_arm_grip_ik_math.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>
// DOC-only candidate runtime. Loading may allocate; evaluation does not. The
// producer must certify the entire interval's mesh/Jacobian/seam behavior.
namespace world_arm_grip_stream {
static_assert(sizeof(unsigned)==4&&sizeof(float)==4&&sizeof(double)==8&&std::numeric_limits<float>::is_iec559&&std::numeric_limits<double>::is_iec559,"Grip metadata uses IEEE754 binary32/64 and32bit indices");
namespace ik=world_arm_grip_ik;
constexpr unsigned pose_count=172,pair_count=217,bank_count=3;
struct Layout {std::array<std::vector<unsigned>,2> views;};
struct NativePose {float rotation[4]={0,0,0,1},pivot_a[3]={},pivot_b[3]={};};
struct SparseOverride {unsigned index=0;float position[3]={},normal[3]={},tangent[3]={};};
enum class Status {Disabled,Applied,Rejected};
struct Output {const SparseOverride*vertices=nullptr;std::size_t count=0;};
struct Entry {unsigned joint=0;std::array<float,4> local={};};
struct Vertex {unsigned primitive=0,local=0,global=0,flags=0,first_entry=0,entry_count=0;float uv[2]={},sign=1;double cuff_gain=0;unsigned cuff_hand=0;};
struct Sample {std::array<float,9> pnt={};};
struct Residual {std::array<float,3> normal={},tangent={};};
struct Face {std::array<unsigned,3> vertices={};double tangent_factor1=0,tangent_factor2=0;};
struct Geometry {
    std::vector<unsigned> mesh_counts;
    std::vector<Vertex> vertices;
    std::vector<Entry> entries;
    std::vector<Face> faces;
    std::vector<Sample> samples;
    std::vector<Residual> residuals;
    std::vector<ik::Point> positions,geometric_normals,tangent_sums,normals,tangents;
    std::vector<SparseOverride> output;
};
struct Pair {bool accepted=false,fp_accepted=false;unsigned a=0,b=0;NativePose native;};
struct Bank {unsigned id=0;std::vector<ik::Pose> frames;std::array<Pair,pair_count> pairs={};std::array<Geometry,2> views;};
struct Database {std::vector<Bank> banks;std::size_t input_bytes=0,resident_bytes=0;};
inline std::array<unsigned,2> endpoints(unsigned slot){
    constexpr unsigned clips[][3]={{0,4,0},{4,8,1},{12,12,0},{24,12,0},{36,12,0},{48,12,0},{60,12,0},{72,4,0},{76,4,0},{80,4,0},{84,4,0},{88,4,0},{92,8,0},{100,8,0},{108,8,0},{116,8,0},{124,8,0},{132,8,0},{140,8,0},{148,24,0}};
    if(slot<172){for(const auto&c:clips)if(slot>=c[0]&&slot<c[0]+c[1])return {slot,c[0]+(c[2]?(slot-c[0]+1)%c[1]:std::min(slot-c[0]+1,c[1]-1))};}
    else if(slot<175)return {slot==172?0u:slot==173?4u:8u,slot==172?4u:0u};
    else if(slot<200){unsigned pair=slot-175;return {12+12*(pair/5)+8,12+12*(pair%5)};}
    else if(slot<205)return {0,12+12*(slot-200)};
    else if(slot<217){constexpr unsigned ends[]={23,35,47,59,71,99,107,115,123,131,139,147};return {ends[slot-205],0};}
    return {172,172};
}
namespace detail {
struct Reader {
    const unsigned char*data=nullptr;std::size_t size=0,at=0;bool okay=true;
    bool raw(void*out,std::size_t n){if(!okay||n>size-at){okay=false;return false;}std::memcpy(out,data+at,n);at+=n;return true;}
    unsigned u32(){unsigned char b[4]={};raw(b,4);return unsigned(b[0])|(unsigned(b[1])<<8)|(unsigned(b[2])<<16)|(unsigned(b[3])<<24);}
    float f32(){const std::uint32_t bits=u32();float value=0;std::memcpy(&value,&bits,4);if(!std::isfinite(value))okay=false;return value;}
    double f64(){unsigned char b[8]={};raw(b,8);std::uint64_t bits=0;for(unsigned i=0;i<8;++i)bits|=std::uint64_t(b[i])<<(8*i);double value=0;std::memcpy(&value,&bits,8);if(!std::isfinite(value))okay=false;return value;}
};
inline ik::Point residual(const std::array<float,3>&s){return {s[0],s[1],s[2]};}
inline ik::Point pnt(const Sample&s,unsigned start){return {s.pnt[start],s.pnt[start+1],s.pnt[start+2]};}
inline ik::Point geometric_normal(ik::Point n){if(ik::length(n)<1.e-8)return {0,1,0};ik::unit(n);return n;}
inline ik::Point tangent(ik::Point sum,const ik::Point&normal){sum=ik::sub(sum,ik::mul(normal,ik::dot(normal,sum)));if(ik::length(sum)<1.e-10)sum=ik::cross(normal,std::abs(normal[1])<.9?ik::Point{0,1,0}:ik::Point{1,0,0});if(!ik::unit(sum,1.e-12))return {1,0,0};return sum;}
inline void surface_sums(Geometry&g){
    std::fill(g.geometric_normals.begin(),g.geometric_normals.end(),ik::Point{});std::fill(g.tangent_sums.begin(),g.tangent_sums.end(),ik::Point{});
    for(const auto&face:g.faces){const auto e1=ik::sub(g.positions[face.vertices[1]],g.positions[face.vertices[0]]),e2=ik::sub(g.positions[face.vertices[2]],g.positions[face.vertices[0]]),cross=ik::cross(e1,e2);
        const ik::Point t=ik::add(ik::mul(e1,face.tangent_factor1),ik::mul(e2,face.tangent_factor2));
        for(unsigned vertex:face.vertices){g.geometric_normals[vertex]=ik::add(g.geometric_normals[vertex],cross);g.tangent_sums[vertex]=ik::add(g.tangent_sums[vertex],t);}
    }
}
inline bool native_valid(const NativePose&p){ik::Quaternion q;for(unsigned i=0;i<4;++i)q[i]=p.rotation[i];if(!ik::signed_delta(q))return false;for(unsigned i=0;i<3;++i)if(!std::isfinite(p.pivot_a[i])||!std::isfinite(p.pivot_b[i])||std::abs(p.pivot_a[i])>8||std::abs(p.pivot_b[i])>8)return false;return true;}
inline bool same_native(const NativePose&a,const NativePose&b){
    ik::Quaternion qa,qb;for(unsigned i=0;i<4;++i){qa[i]=a.rotation[i];qb[i]=b.rotation[i];}if(!ik::signed_delta(qa)||!ik::signed_delta(qb))return false;
    for(unsigned i=0;i<4;++i)if(std::abs(qa[i]-qb[i])>2.e-6)return false;
    for(unsigned i=0;i<3;++i)if(!std::isfinite(a.pivot_a[i])||!std::isfinite(a.pivot_b[i])||std::abs(a.pivot_a[i]-b.pivot_a[i])>2.e-5||std::abs(a.pivot_b[i]-b.pivot_b[i])>2.e-5)return false;
    return true;
}
inline ik::Interval interval(const Bank&b,unsigned a,unsigned z,const NativePose&native){ik::Interval in;in.a=b.frames[a];in.b=b.frames[z];for(unsigned i=0;i<4;++i)in.weapon_delta[i]=native.rotation[i];for(unsigned i=0;i<3;++i){in.pivot_a[i]=native.pivot_a[i];in.pivot_b[i]=native.pivot_b[i];}return in;}
}
// Expected layouts come from the actual already-loaded body/FP model assets.
// Runtime checks counts and UV/index bounds. Exact UV/topology/material identity
// is supplied separately by the sealed producer and original asset SHA proof.
// Load is transactional: on any failure the old database is left untouched.
inline bool load(const void*data,std::size_t bytes,const std::array<Layout,bank_count>&expected,Database&out,std::string&error){
    error.clear();auto reject=[&](const char*message){error=message;return false;};
    if(!data||bytes<16||bytes>256u*1024u*1024u)return reject("Grip metadata byte budget rejected");
    detail::Reader r{static_cast<const unsigned char*>(data),bytes};Database candidate;std::size_t memory=0;
    try{
        char magic[8];if(!r.raw(magic,8))return reject("Grip metadata header rejected");const unsigned version=r.u32();const bool legacy=std::memcmp(magic,"VWAG0001",8)==0&&version==1;if(!legacy&&(std::memcmp(magic,"VWAG0002",8)||version!=2))return reject("Grip metadata header rejected");const unsigned count=r.u32();if(!count||count>bank_count)return reject("Grip bank count rejected");candidate.banks.reserve(count);memory=std::size_t(count)*(sizeof(Bank)+pose_count*sizeof(ik::Pose));unsigned previous_id=0;
        for(unsigned bank_index=0;bank_index<count;++bank_index){Bank b;b.id=r.u32();if(b.id>=bank_count||(bank_index&&b.id<=previous_id)||r.u32()!=pose_count||r.u32()!=pair_count)return reject("Grip bank dimensions/order rejected");previous_id=b.id;b.frames.resize(pose_count);
            for(auto&pose:b.frames)for(auto&joint:pose)for(unsigned row=0;row<3;++row){for(double&v:joint.basis[row])v=r.f64();joint.position[row]=r.f64();}
            for(const auto&pose:b.frames)for(const auto&joint:pose){ik::Basis ignored;if(!ik::finite(joint.position)||ik::length(joint.position)>12||!ik::orthogonal(joint.basis,ignored))return reject("Grip source joint frame rejected");}
            for(unsigned slot=0;slot<pair_count;++slot){auto&p=b.pairs[slot];const unsigned enabled=r.u32();p.accepted=bool(enabled&1);p.fp_accepted=!legacy&&bool(enabled&2);p.a=r.u32();p.b=r.u32();for(float&v:p.native.rotation)v=r.f32();for(float&v:p.native.pivot_a)v=r.f32();for(float&v:p.native.pivot_b)v=r.f32();const auto e=endpoints(slot);if(enabled>(legacy?1u:3u)||p.a!=e[0]||p.b!=e[1]||!detail::native_valid(p.native))return reject("Grip pair/rigid contract rejected");
                if(p.accepted||p.fp_accepted){if(p.a==p.b)return reject("Grip static self pair must remain baseline");const auto in=detail::interval(b,p.a,p.b,p.native);if(ik::length(ik::sub(in.a[7].position,in.pivot_a))>2.e-5||ik::length(ik::sub(in.b[7].position,in.pivot_b))>2.e-5)return reject("Grip primary wrist/pivot mismatch");for(double t:{.25,.5,.75}){ik::Result result;if(!ik::solve(in,t,result))return reject("Certified grip pair fails anatomical gate");}}
            }
            for(unsigned view=0;view<2;++view){auto&g=b.views[view];const unsigned meshes=r.u32(),total=r.u32(),vertices=r.u32(),faces=r.u32();if(!meshes||meshes>128||meshes!=expected[b.id].views[view].size()||!total||total>100000||vertices>12000||faces>50000||(vertices==0)!=(faces==0))return reject("Grip sparse view dimensions rejected");
                const std::size_t estimated=std::size_t(vertices)*pose_count*(sizeof(Sample)+sizeof(Residual))+std::size_t(vertices)*(sizeof(Vertex)+4*sizeof(Entry)+5*sizeof(ik::Point)+sizeof(SparseOverride)+sizeof(unsigned))+std::size_t(faces)*sizeof(Face)+std::size_t(meshes)*2*sizeof(unsigned);if(estimated>512u*1024u*1024u-memory)return reject("Grip CPU resident budget rejected");memory+=estimated;
                g.mesh_counts.resize(meshes);std::vector<unsigned>offsets(meshes);unsigned sum=0;for(unsigned i=0;i<meshes;++i){offsets[i]=sum;g.mesh_counts[i]=r.u32();if(!g.mesh_counts[i]||g.mesh_counts[i]!=expected[b.id].views[view][i]||g.mesh_counts[i]>100000-sum)return reject("Grip actual model layout differs");sum+=g.mesh_counts[i];}if(sum!=total)return reject("Grip total model vertex count differs");
                g.vertices.resize(vertices);g.entries.reserve(std::size_t(vertices)*4);std::uint64_t previous=0;unsigned outputs=0;
                for(unsigned i=0;i<vertices;++i){auto&v=g.vertices[i];v.primitive=r.u32();v.local=r.u32();v.flags=r.u32();v.uv[0]=r.f32();v.uv[1]=r.f32();v.sign=r.f32();v.entry_count=r.u32();const std::uint64_t key=(std::uint64_t(v.primitive)<<32)|v.local;
                    if(v.primitive>=meshes||v.local>=g.mesh_counts[v.primitive]||(i&&key<=previous)||(v.flags!=0&&v.flags!=2&&v.flags!=3&&v.flags!=6)||std::abs(v.uv[0])>10000||std::abs(v.uv[1])>10000||(v.sign!=1&&v.sign!=-1)||v.entry_count>4||bool(v.entry_count)!=bool(v.flags&1))return reject("Grip sparse vertex correspondence rejected");
                    if(v.flags==6){if(legacy||view!=1||g.mesh_counts[v.primitive]!=582)return reject("Grip cuff mode requires original582vertex FP sleeve");const unsigned side=v.local/291,part=v.local%291,ring=part==289?0:part==290?16:part/17;v.cuff_gain=ik::smooth(double(ring)/16);v.cuff_hand=side==0?3:7;}
                    previous=key;v.global=offsets[v.primitive]+v.local;v.first_entry=unsigned(g.entries.size());float weight=0;unsigned previous_joint=0;
                    for(unsigned e=0;e<v.entry_count;++e){Entry entry;entry.joint=r.u32();for(float&value:entry.local)value=r.f32();if(entry.joint>=8||(e&&entry.joint<=previous_joint)||entry.local[3]<=0||entry.local[3]>1)return reject("Grip skin entry rejected");previous_joint=entry.joint;for(float value:entry.local)if(std::abs(value)>2048)return reject("Grip weighted local coordinate rejected");weight+=entry.local[3];g.entries.push_back(entry);}if(weight>1.00001f)return reject("Grip skin weight sum rejected");outputs+=bool(v.flags&2);
                }
                if(vertices&&!outputs)return reject("Grip sparse view has no output vertices");
                g.faces.resize(faces);std::vector<unsigned>incidence(vertices,0);for(auto&f:g.faces){for(unsigned&v:f.vertices)v=r.u32();if(f.vertices[0]>=vertices||f.vertices[1]>=vertices||f.vertices[2]>=vertices||f.vertices[0]==f.vertices[1]||f.vertices[0]==f.vertices[2]||f.vertices[1]==f.vertices[2]||g.vertices[f.vertices[0]].primitive!=g.vertices[f.vertices[1]].primitive||g.vertices[f.vertices[0]].primitive!=g.vertices[f.vertices[2]].primitive)return reject("Grip original triangle correspondence rejected");for(unsigned v:f.vertices)++incidence[v];const auto&a=g.vertices[f.vertices[0]],&b=g.vertices[f.vertices[1]],&c=g.vertices[f.vertices[2]];const double du1=double(b.uv[0])-a.uv[0],dv1=double(b.uv[1])-a.uv[1],du2=double(c.uv[0])-a.uv[0],dv2=double(c.uv[1])-a.uv[1],det=du1*dv2-dv1*du2;if(std::abs(det)>1.e-12){f.tangent_factor1=dv2/det;f.tangent_factor2=-dv1/det;}}for(unsigned i=0;i<vertices;++i)if((g.vertices[i].flags&2)&&!incidence[i])return reject("Grip output missing incident topology");
                g.samples.resize(std::size_t(pose_count)*vertices);for(auto&s:g.samples){for(float&value:s.pnt)value=r.f32();for(unsigned i=0;i<3;++i)if(std::abs(s.pnt[i])>8)return reject("Grip morph coordinate rejected");const double n=ik::length(detail::pnt(s,3)),t=ik::length(detail::pnt(s,6));if(n<.8||n>1.2||t<.8||t>1.2)return reject("Grip original PNT basis rejected");}
                if(!r.okay)return reject("Grip PNT payload truncated/nonfinite");
                g.residuals.resize(g.samples.size());g.positions.resize(vertices);g.geometric_normals.resize(vertices);g.tangent_sums.resize(vertices);g.normals.resize(vertices);g.tangents.resize(vertices);g.output.resize(outputs);
                for(unsigned pose=0;pose<pose_count;++pose){for(unsigned v=0;v<vertices;++v)g.positions[v]=detail::pnt(g.samples[std::size_t(pose)*vertices+v],0);detail::surface_sums(g);for(unsigned v=0;v<vertices;++v){const auto&sample=g.samples[std::size_t(pose)*vertices+v];const auto n=detail::geometric_normal(g.geometric_normals[v]),t=detail::tangent(g.tangent_sums[v],n);auto&res=g.residuals[std::size_t(pose)*vertices+v];const auto rn=ik::sub(detail::pnt(sample,3),n),rt=ik::sub(detail::pnt(sample,6),t);for(unsigned axis=0;axis<3;++axis){res.normal[axis]=float(rn[axis]);res.tangent[axis]=float(rt[axis]);}}}
            }
            if(!r.okay)return reject("Grip metadata truncated/nonfinite");
            for(const auto&pair:b.pairs)if((pair.accepted&&b.views[0].vertices.empty())||(pair.fp_accepted&&b.views[1].vertices.empty()))return reject("Grip enabled view has no sparse geometry");
            candidate.banks.push_back(std::move(b));
        }
        if(!r.okay||r.at!=bytes)return reject("Grip metadata truncated or unexpected tail");
        candidate.input_bytes=bytes;std::size_t resident=sizeof(Database)+candidate.banks.capacity()*sizeof(Bank);
        for(const auto&bank:candidate.banks){resident+=bank.frames.capacity()*sizeof(ik::Pose);for(const auto&g:bank.views){resident+=g.mesh_counts.capacity()*sizeof(unsigned)+g.vertices.capacity()*sizeof(Vertex)+g.entries.capacity()*sizeof(Entry)+g.faces.capacity()*sizeof(Face)+g.samples.capacity()*sizeof(Sample)+g.residuals.capacity()*sizeof(Residual)+g.positions.capacity()*sizeof(ik::Point)+g.geometric_normals.capacity()*sizeof(ik::Point)+g.tangent_sums.capacity()*sizeof(ik::Point)+g.normals.capacity()*sizeof(ik::Point)+g.tangents.capacity()*sizeof(ik::Point)+g.output.capacity()*sizeof(SparseOverride);}}
        if(resident>512u*1024u*1024u)return reject("Grip actual resident budget rejected");
        candidate.resident_bytes=resident;out=std::move(candidate);return true;
    }catch(...){return reject("Grip metadata allocation/format rejected");}
}
inline bool certified(const Database&db,unsigned bank,unsigned a,unsigned b,unsigned view=0){if(view>=2||a>=pose_count||b>=pose_count)return false;for(const auto&entry:db.banks)if(entry.id==bank){for(const auto&p:entry.pairs)if((view==0?p.accepted:p.fp_accepted)&&p.a==a&&p.b==b)return true;return false;}return false;}
// Output scratch belongs to db; root maps already-global indices directly into
// the existing absolute body/FP vertex override buffers. A rejected evaluation
// is a hard diagnostics failure; never treat it as permission to toggle a bad
// interval on/off during playback. Only offline-certified pairs may be enabled.
inline Status evaluate(Database&db,unsigned bank,unsigned view,unsigned a,unsigned b,float weight,const NativePose&native,Output&out){
    out={};if(bank>=bank_count||view>=2||a>=pose_count||b>=pose_count||!std::isfinite(weight)||weight<0||weight>1)return Status::Rejected;
    Bank*owner=nullptr;const Pair*selected=nullptr;for(auto&entry:db.banks)if(entry.id==bank){owner=&entry;for(const auto&p:entry.pairs)if((view==0?p.accepted:p.fp_accepted)&&p.a==a&&p.b==b){selected=&p;break;}break;}
    if(!owner||!selected)return Status::Disabled;
    auto&g=owner->views[view];if(g.vertices.empty())return Status::Disabled;if(!detail::same_native(native,selected->native))return Status::Rejected;
    const unsigned count=unsigned(g.vertices.size());const std::size_t start_a=std::size_t(a)*count,start_b=std::size_t(b)*count;
    if(weight==0||weight==1){const std::size_t start=weight==0?start_a:start_b;unsigned o=0;for(unsigned i=0;i<count;++i)if(g.vertices[i].flags&2){auto&v=g.output[o++];v.index=g.vertices[i].global;const auto&s=g.samples[start+i].pnt;std::copy_n(s.data(),3,v.position);std::copy_n(s.data()+3,3,v.normal);std::copy_n(s.data()+6,3,v.tangent);}out={g.output.data(),g.output.size()};return Status::Applied;}
    ik::Result solved;const auto in=detail::interval(*owner,a,b,native);if(!ik::solve(in,weight,solved))return Status::Rejected;
    const double t=weight;ik::Pose changes;for(unsigned joint=0;joint<8;++joint){for(unsigned row=0;row<3;++row)changes[joint].basis[row]=ik::sub(solved.pose[joint].basis[row],ik::lerp(in.a[joint].basis[row],in.b[joint].basis[row],t));changes[joint].position=ik::sub(solved.pose[joint].position,ik::lerp(in.a[joint].position,in.b[joint].position,t));}
    std::array<ik::Basis,2> inverse_hand_a,inverse_hand_b;
    if(view==1)for(unsigned side=0;side<2;++side){const unsigned hand=side==0?3:7;if(!ik::inverse(in.a[hand].basis,inverse_hand_a[side])||!ik::inverse(in.b[hand].basis,inverse_hand_b[side]))return Status::Rejected;}
    for(unsigned i=0;i<count;++i){const auto&vertex=g.vertices[i];g.positions[i]=ik::lerp(detail::pnt(g.samples[start_a+i],0),detail::pnt(g.samples[start_b+i],0),t);
        if(vertex.flags==6){if(vertex.cuff_gain>0){const unsigned hand=vertex.cuff_hand,side=hand==3?0:1;const ik::Point reach={.25,0,.65};const auto la=ik::apply(inverse_hand_a[side],ik::sub(ik::sub(detail::pnt(g.samples[start_a+i],0),reach),in.a[hand].position)),lb=ik::apply(inverse_hand_b[side],ik::sub(ik::sub(detail::pnt(g.samples[start_b+i],0),reach),in.b[hand].position));const auto target=ik::add(ik::add(ik::apply(solved.pose[hand].basis,ik::lerp(la,lb,t)),solved.pose[hand].position),reach);g.positions[i]=ik::lerp(g.positions[i],target,vertex.cuff_gain);}continue;}
        if(!(vertex.flags&1))continue;
        ik::Point delta={};for(unsigned e=0;e<vertex.entry_count;++e){const auto&entry=g.entries[vertex.first_entry+e];const auto&change=changes[entry.joint];const ik::Point local={entry.local[0],entry.local[1],entry.local[2]};delta=ik::add(delta,ik::add(ik::apply(change.basis,local),ik::mul(change.position,entry.local[3])));}g.positions[i]=ik::add(g.positions[i],delta);}
    // Surfacebasis follows the actualfloat32positions uploadedtorendering.
    for(auto&point:g.positions)for(double&coordinate:point)coordinate=double(float(coordinate));
    detail::surface_sums(g);for(unsigned i=0;i<count;++i){if(!(g.vertices[i].flags&2))continue;const auto&ra=g.residuals[start_a+i],&rb=g.residuals[start_b+i];g.normals[i]=ik::add(detail::geometric_normal(g.geometric_normals[i]),ik::lerp(detail::residual(ra.normal),detail::residual(rb.normal),t));if(!ik::unit(g.normals[i]))return Status::Rejected;g.tangents[i]=ik::add(detail::tangent(g.tangent_sums[i],g.normals[i]),ik::lerp(detail::residual(ra.tangent),detail::residual(rb.tangent),t));g.tangents[i]=ik::sub(g.tangents[i],ik::mul(g.normals[i],ik::dot(g.normals[i],g.tangents[i])));if(!ik::unit(g.tangents[i]))return Status::Rejected;}
    unsigned o=0;for(unsigned i=0;i<count;++i)if(g.vertices[i].flags&2){auto&v=g.output[o++];v.index=g.vertices[i].global;for(unsigned axis=0;axis<3;++axis){v.position[axis]=float(g.positions[i][axis]);v.normal[axis]=float(g.normals[i][axis]);v.tangent[axis]=float(g.tangents[i][axis]);if(!std::isfinite(v.position[axis])||std::abs(v.position[axis])>8)return Status::Rejected;}}
    out={g.output.data(),g.output.size()};return Status::Applied;
}
}
#endif

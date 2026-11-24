#ifndef VALWOOD_WORLD_COMBAT_POSE_PACKET_H
#define VALWOOD_WORLD_COMBAT_POSE_PACKET_H
// Immutable exact posed PNT, memory mapped once. Only requested actor poses
// enter CPU staging/GPU buffers; no source weights are truncated or retargeted.
#include "engine/world_models.h"
#include "engine/world_rigid_weapon_pose.h"
#include <array>
#include <vector>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <limits>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif
namespace world_combat_pose_packet {
using Point=world_rigid_weapon_pose::Point;
struct Mesh {unsigned count=0,offset=0,flags=0;std::uint64_t layout=0;std::array<unsigned,3> triangle={};std::vector<unsigned> indices;};
inline bool basis(WorldModelVertexOverride &v,Point n,Point t){
 double normal[3]={n[0],n[1],n[2]},tangent[3]={t[0],t[1],t[2]},nn=0;
 for(auto x:normal)nn+=x*x;if(!(nn>1.e-14)||!std::isfinite(nn))return false;nn=std::sqrt(nn);for(auto &x:normal)x/=nn;double dot=0;
 for(unsigned k=0;k<3;++k)dot+=normal[k]*tangent[k];double tt=0;for(unsigned k=0;k<3;++k){tangent[k]-=normal[k]*dot;tt+=tangent[k]*tangent[k];}
 if(!(tt>1.e-14)){const unsigned axis=std::abs(normal[1])<.9?1:0;tangent[0]=axis==1?normal[2]:0;tangent[1]=axis==0?-normal[2]:0;tangent[2]=axis==1?-normal[0]:normal[1];tt=0;for(auto x:tangent)tt+=x*x;}
 if(!(tt>1.e-14)||!std::isfinite(tt))return false;tt=std::sqrt(tt);for(unsigned k=0;k<3;++k){v.normal[k]=float(normal[k]);v.tangent[k]=float(tangent[k]/tt);}return true;
}
class Packet {
 const unsigned char *data_=nullptr;std::size_t bytes_=0,start_=0;
#ifdef _WIN32
 HANDLE file_=INVALID_HANDLE_VALUE,map_=nullptr;
#else
 int file_=-1;
#endif
 unsigned poses_=0,vertices_=0;std::vector<Mesh> meshes_;
 static unsigned word(const unsigned char *p){return unsigned(p[0])|(unsigned(p[1])<<8)|(unsigned(p[2])<<16)|(unsigned(p[3])<<24);}
 public:
 Packet()=default;Packet(const Packet&)=delete;Packet&operator=(const Packet&)=delete;~Packet(){close();}
 void close(){
#ifdef _WIN32
  if(data_)UnmapViewOfFile(data_);if(map_)CloseHandle(map_);if(file_!=INVALID_HANDLE_VALUE)CloseHandle(file_);file_=INVALID_HANDLE_VALUE;map_=nullptr;
#else
  if(data_)munmap(const_cast<unsigned char*>(data_),bytes_);if(file_>=0)::close(file_);file_=-1;
#endif
  data_=nullptr;bytes_=start_=0;poses_=vertices_=0;meshes_.clear();
 }
 bool load(const char *path){close();if(!path||!*path)return false;
#ifdef _WIN32
  file_=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);LARGE_INTEGER size={};
  if(file_==INVALID_HANDLE_VALUE||!GetFileSizeEx(file_,&size)||size.QuadPart<=0||std::uint64_t(size.QuadPart)>1073741824ull){close();return false;}bytes_=std::size_t(size.QuadPart);
  map_=CreateFileMappingA(file_,nullptr,PAGE_READONLY,0,0,nullptr);if(map_)data_=static_cast<const unsigned char*>(MapViewOfFile(map_,FILE_MAP_READ,0,0,0));
#else
  file_=::open(path,O_RDONLY|O_CLOEXEC);struct stat s={};if(file_<0||fstat(file_,&s)||s.st_size<=0||std::uint64_t(s.st_size)>1073741824ull){close();return false;}bytes_=std::size_t(s.st_size);
  void *p=mmap(nullptr,bytes_,PROT_READ,MAP_PRIVATE,file_,0);if(p!=MAP_FAILED)data_=static_cast<const unsigned char*>(p);
#endif
  if(!data_||bytes_<24||std::memcmp(data_,"VWPNT002",8)||word(data_+8)!=2){close();return false;}
  poses_=word(data_+12);const unsigned count=word(data_+16);vertices_=word(data_+20);
  if(poses_<172||poses_>256||!count||count>32||!vertices_||vertices_>100000){close();return false;}
  start_=24+std::size_t(count)*32;if(start_>bytes_){close();return false;}
  unsigned offset=0;for(unsigned m=0;m<count;++m){const auto *p=data_+24+m*32;Mesh mesh;mesh.count=word(p);mesh.flags=word(p+4);mesh.layout=std::uint64_t(word(p+8))|(std::uint64_t(word(p+12))<<32);mesh.offset=offset;
   for(unsigned k=0;k<3;++k)mesh.triangle[k]=word(p+16+k*4);
   if(!mesh.count||mesh.count>vertices_-offset||mesh.flags>1){close();return false;}
   const unsigned ni=word(p+28);if((mesh.flags?ni<3||ni%3||ni>320000:ni!=0)||std::size_t(ni)*4>bytes_-start_){close();return false;}mesh.indices.resize(ni);for(unsigned i=0;i<ni;++i){mesh.indices[i]=word(data_+start_+i*4);if(mesh.indices[i]>=mesh.count){close();return false;}}start_+=std::size_t(ni)*4;
   for(auto i:mesh.triangle)if(i>=mesh.count){close();return false;}offset+=mesh.count;meshes_.push_back(mesh);
  }if(offset!=vertices_||std::uint64_t(poses_)*vertices_*36!=bytes_-start_){close();return false;}return true;
 }
 unsigned pose_count()const{return poses_;}unsigned vertices()const{return vertices_;}std::size_t mapped_bytes()const{return bytes_;}const std::vector<Mesh>&meshes()const{return meshes_;}
 const float *vertex(unsigned pose,unsigned id)const{if(pose>=poses_||id>=vertices_)return nullptr;return reinterpret_cast<const float*>(data_+start_+(std::size_t(pose)*vertices_+id)*36);}
 Point position(unsigned pose,unsigned id)const{const auto *p=vertex(pose,id);return p?Point{p[0],p[1],p[2]}:Point{};}
 std::array<unsigned,3> frame(const Mesh &mesh,unsigned pose)const{
  if(mesh.indices.empty())return mesh.triangle;const unsigned x=mesh.indices[0];unsigned y=x,z=x;float far=0,area=0;
  const auto origin=position(pose,mesh.offset+x);for(unsigned id:mesh.indices){const auto v=world_rigid_weapon_pose::sub(position(pose,mesh.offset+id),origin);const float n=world_rigid_weapon_pose::dot(v,v);if(n>far){far=n;y=id;}}
  const auto edge=world_rigid_weapon_pose::sub(position(pose,mesh.offset+y),origin);for(unsigned id:mesh.indices){const auto v=world_rigid_weapon_pose::cross(edge,world_rigid_weapon_pose::sub(position(pose,mesh.offset+id),origin));const float n=world_rigid_weapon_pose::dot(v,v);if(n>area){area=n;z=id;}}
  return {x,y,z};
 }
 bool rigid(unsigned a,unsigned b,const float pivot_a[3],const float pivot_b[3],std::vector<WorldModelRigidWeaponPose>&out,WorldModelRigidWeaponPose *primary=nullptr)const{
  if(a>=poses_||b>=poses_)return false;out.assign(meshes_.size(),{});bool found=false;
  for(unsigned m=0;m<meshes_.size();++m){const auto &mesh=meshes_[m];if(!(mesh.flags&1))continue;auto &r=out[m];std::array<Point,3>x,y;
   const auto triangle=frame(mesh,a);for(unsigned k=0;k<3;++k){x[k]=position(a,mesh.offset+triangle[k]);y[k]=position(b,mesh.offset+triangle[k]);}
   if(!world_rigid_weapon_pose::rotation(x,y,r.rotation))return false;std::copy_n(pivot_a,3,r.pivot_a);std::copy_n(pivot_b,3,r.pivot_b);r.enabled=true;
   for(unsigned i=0;i<mesh.count;++i){auto v=position(a,mesh.offset+i),target=position(b,mesh.offset+i);for(auto f:v)if(!std::isfinite(f))return false;const auto q=world_rigid_weapon_pose::interpolate(v,pivot_a,pivot_b,r.rotation,1);const auto d=world_rigid_weapon_pose::sub(q,target);if(world_rigid_weapon_pose::dot(d,d)>.002f*.002f)return false;}
   if(!found&&primary)*primary=r;found=true;
  }return found;
 }
 bool raw_pose(unsigned pose,WorldModelVertexOverride*out,std::size_t count)const{
  if(!out||pose>=poses_||count!=vertices_)return false;for(unsigned i=0;i<vertices_;++i){const auto *p=vertex(pose,i);for(unsigned k=0;k<9;++k)if(!std::isfinite(p[k]))return false;auto &v=out[i];v={};v.weight=1;std::copy_n(p,3,v.position);std::copy_n(p+3,3,v.normal);std::copy_n(p+6,3,v.tangent);}return true;
 }
 bool sample(unsigned a,unsigned b,float w,const std::vector<WorldModelRigidWeaponPose>&rigid,WorldModelVertexOverride*out,std::size_t count)const{
  if(!out||count!=vertices_||a>=poses_||b>=poses_||!std::isfinite(w)||w<0||w>1||rigid.size()!=meshes_.size())return false;
  for(unsigned m=0;m<meshes_.size();++m){const auto &mesh=meshes_[m];const auto &r=rigid[m];for(unsigned i=0;i<mesh.count;++i){const unsigned id=mesh.offset+i;const auto *x=vertex(a,id),*y=vertex(b,id);auto &v=out[id];v={};v.weight=1;
   for(unsigned k=0;k<9;++k)if(!std::isfinite(x[k])||!std::isfinite(y[k]))return false;
   Point n,t;if(r.enabled){const auto q=world_rigid_weapon_pose::interpolate({x[0],x[1],x[2]},r.pivot_a,r.pivot_b,r.rotation,w);std::copy(q.begin(),q.end(),v.position);
    float rot[4]={r.rotation[0]*w,r.rotation[1]*w,r.rotation[2]*w,1+(r.rotation[3]-1)*w};float norm=0;for(auto f:rot)norm+=f*f;if(!(norm>1e-14f))return false;norm=std::sqrt(norm);for(auto &f:rot)f/=norm;
    n=world_rigid_weapon_pose::rotate({x[3],x[4],x[5]},rot);t=world_rigid_weapon_pose::rotate({x[6],x[7],x[8]},rot);
   }else{for(unsigned k=0;k<3;++k){v.position[k]=x[k]+(y[k]-x[k])*w;n[k]=x[k+3]+(y[k+3]-x[k+3])*w;t[k]=x[k+6]+(y[k+6]-x[k+6])*w;}}
   if(!r.enabled&&(w==0||w==1)){const auto *z=w==0?x:y;std::copy_n(z,3,v.position);std::copy_n(z+3,3,v.normal);std::copy_n(z+6,3,v.tangent);}else if(!basis(v,n,t))return false;
  }}return true;
 }
 bool bridge(unsigned a,unsigned b,float w,const std::vector<WorldModelRigidWeaponPose>&rigid,const WorldModelVertexOverride*frozen,float alpha,WorldModelVertexOverride*out,std::size_t count)const{
  if(!frozen||!out||frozen==out||!std::isfinite(alpha)||alpha<0||alpha>1||count!=vertices_)return false;
  if(alpha==0){std::copy_n(frozen,count,out);return true;}if(!sample(a,b,w,rigid,out,count))return false;if(alpha==1)return true;
  for(unsigned m=0;m<meshes_.size();++m){const auto &mesh=meshes_[m];const auto &r=rigid[m];float rotation[4]={},from[3]={},to[3]={},q[4]={};
   if(r.enabled){std::array<Point,3> f,target,base;const auto triangle=frame(mesh,a);for(unsigned k=0;k<3;++k){const unsigned id=mesh.offset+triangle[k];f[k]={frozen[id].position[0],frozen[id].position[1],frozen[id].position[2]};target[k]={out[id].position[0],out[id].position[1],out[id].position[2]};base[k]=position(a,id);}
    float base_to_frozen[4];if(!world_rigid_weapon_pose::rotation(f,target,rotation)||!world_rigid_weapon_pose::rotation(base,f,base_to_frozen))return false;
    const auto arm=world_rigid_weapon_pose::rotate({r.pivot_a[0]-base[0][0],r.pivot_a[1]-base[0][1],r.pivot_a[2]-base[0][2]},base_to_frozen);for(unsigned k=0;k<3;++k){from[k]=f[0][k]+arm[k];to[k]=r.pivot_a[k]+(r.pivot_b[k]-r.pivot_a[k])*w;q[k]=rotation[k]*alpha;}q[3]=1+(rotation[3]-1)*alpha;float n=0;for(auto v:q)n+=v*v;if(!(n>1e-14f))return false;n=std::sqrt(n);for(auto &v:q)v/=n;
   }
   for(unsigned i=0;i<mesh.count;++i){const unsigned id=mesh.offset+i;auto &v=out[id];const auto &f=frozen[id];Point n,t;
    if(!r.enabled){for(unsigned k=0;k<3;++k){v.position[k]=f.position[k]+(v.position[k]-f.position[k])*alpha;n[k]=f.normal[k]+(v.normal[k]-f.normal[k])*alpha;t[k]=f.tangent[k]+(v.tangent[k]-f.tangent[k])*alpha;}}
    else{const Point fp={f.position[0],f.position[1],f.position[2]},fn={f.normal[0],f.normal[1],f.normal[2]},ft={f.tangent[0],f.tangent[1],f.tangent[2]};const auto pos=world_rigid_weapon_pose::interpolate(fp,from,to,rotation,alpha),end=world_rigid_weapon_pose::interpolate(fp,from,to,rotation,1),nn=world_rigid_weapon_pose::rotate(fn,q),ne=world_rigid_weapon_pose::rotate(fn,rotation),tt=world_rigid_weapon_pose::rotate(ft,q),te=world_rigid_weapon_pose::rotate(ft,rotation);float error=0;
     for(unsigned k=0;k<3;++k){const float residual=v.position[k]-end[k];error+=residual*residual;v.position[k]=pos[k]+alpha*residual;n[k]=nn[k]+alpha*(v.normal[k]-ne[k]);t[k]=tt[k]+alpha*(v.tangent[k]-te[k]);}if(error>.002f*.002f)return false;
    }
    if(world_rigid_weapon_pose::dot(n,n)<=1e-12f)n={f.normal[0],f.normal[1],f.normal[2]};if(!basis(v,n,t))return false;
   }
  }return true;
 }
};
}
#endif

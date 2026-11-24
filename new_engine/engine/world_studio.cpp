#include "engine/world_platform.h"
#include "engine/world_studio.h"
#include "engine/world_transform_math.h"
#include "engine/world_torch_flames.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
namespace world_studio {
namespace {
bool range(double v,double lo,double hi){return std::isfinite(v)&&v>=lo&&v<=hi;}
bool name(const std::string &s){return !s.empty()&&s.size()<96&&std::all_of(s.begin(),s.end(),[](char c){return(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-';});}
bool relative(const std::string &s){return !s.empty()&&s.size()<240&&s[0]!='/'&&s.find("..") == std::string::npos&&std::all_of(s.begin(),s.end(),[](char c){return(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='/'||c=='.';});}
bool xyz(double x,double y,double z){return range(x,-50400,50400)&&range(z,-28400,28400)&&range(y,-2000,12000);}
float weight(double x,double z,double cx,double cz,float r){const double d=std::hypot(x-cx,z-cz)/r;if(d>=1)return 0;const float t=float(1-d*d);return t*t;}
}
static bool valid_fields(const Document &d,std::string &e,bool complete=true){
 e.clear();auto fail=[&](const char *s){e=s;return false;};const auto &l=d.look;
 if(!world_surface::validate(d.surfaces,e))return false;
 if(!d.source_signature.empty()&&(d.source_signature.size()!=16||!std::all_of(d.source_signature.begin(),d.source_signature.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');})))return fail("Invalid source scene identity");
 if(!range(l.sharpness,0,.65)||!range(l.sun,.25,2)||!range(l.sky,.25,2)||!range(l.exposure,.5,1.75)||!range(l.hour,5.5,20)||!range(l.bearing,-180,180)||!range(l.cloud,0,1)||!range(l.rain,0,1)||!range(l.wind,0,1.5)||!range(l.fog,0,1.5))return fail("Invalid lighting/material look");
 if(d.terrain.size()>tile_limit||d.placements.size()>placement_limit||d.overrides.size()>512||d.lights.size()>light_limit||d.stamps.size()>stamp_limit||d.materials.size()>64||d.decals.size()>32||d.splines.size()>16||d.groups.size()>32||d.assets.size()>WORLD_CUSTOM_ASSET_LIMIT)return fail("Document budget exceeded");
 for(size_t i=0;i<d.terrain.size();i++){const auto &t=d.terrain[i];if(!range(t.x,-394,393)||!range(t.z,-222,221))return fail("Terrain tile outside atlas");for(size_t j=0;j<i;j++)if(t.x==d.terrain[j].x&&t.z==d.terrain[j].z)return fail("Duplicate terrain tile");for(float v:t.delta)if(!range(v,-32,32))return fail("Invalid terrain offset");}
 for(const auto &p:d.placements)if(!name(p.alias)||!xyz(p.x,0,p.z)||!range(p.yaw,-360,360)||!range(p.scale,.15,4)||!range(p.burial,0,2)||!range(p.elevation,0,3)||!range(p.offset_y,-WORLD_OBJECT_OFFSET_LIMIT,WORLD_OBJECT_OFFSET_LIMIT)||!vw_transform::extra_valid(p.pitch,p.roll,p.sx,p.sy,p.sz))return fail("Invalid placement");
 for(size_t i=0;i<d.overrides.size();i++){const auto &p=d.overrides[i];if(p.index>=8192||!xyz(p.x,0,p.z)||!range(p.yaw,-360,360)||!range(p.scale,.15,4)||!range(p.offset_y,-WORLD_OBJECT_OFFSET_LIMIT,WORLD_OBJECT_OFFSET_LIMIT)||!vw_transform::extra_valid(p.pitch,p.roll,p.sx,p.sy,p.sz))return fail("Invalid source override");for(size_t j=0;j<i;j++)if(p.index==d.overrides[j].index)return fail("Duplicate source override");}
 for(const auto &p:d.lights){if(!xyz(p.x,p.y,p.z)||!range(p.radius,.5,40)||!range(p.power,0,40)||!range(p.yaw,-180,180)||!range(p.pitch,-90,90)||!range(p.cone,5,180))return fail("Invalid light");for(float c:p.color)if(!range(c,0,1))return fail("Invalid light color");}
 for(const auto &p:d.stamps)if(!xyz(p.x,0,p.z)||!range(p.radius,.5,64)||!range(p.value,0,1)||p.kind<0||p.kind>3)return fail("Invalid foliage mask");
 for(size_t i=0;i<d.materials.size();i++){const auto &p=d.materials[i];if(!name(p.alias)||!range(p.roughness,.25,2)||!range(p.normal,0,2)||!range(p.uv,.25,8))return fail("Invalid material");for(float c:p.tint)if(!range(c,0,2))return fail("Invalid material tint");for(size_t j=0;j<i;j++)if(p.alias==d.materials[j].alias)return fail("Duplicate material override");}
 for(const auto &p:d.decals){if(!xyz(p.x,p.y,p.z)||!range(p.yaw,-180,180)||!range(p.opacity,0,1)||!range(p.roughness,0,1)||p.kind<0||p.kind>3)return fail("Invalid decal");for(float v:p.size)if(!range(v,.1,32))return fail("Invalid decal box");for(float v:p.color)if(!range(v,0,1))return fail("Invalid decal color");}
 for(const auto &p:d.splines){if(!name(p.alias)||p.points.size()>32||p.kind<0||p.kind>1||!range(p.spacing,.5,40)||!range(p.width,.5,32)||!range(p.scale,.15,4))return fail("Invalid spline");for(auto v:p.points)if(!xyz(v[0],0,v[1]))return fail("Invalid spline point");}
 for(const auto &g:d.groups){if(g.name.empty()||g.name.size()>64||g.name.find_first_of("\r\n\t")!=std::string::npos||g.members.empty()||g.members.size()>128)return fail("Invalid object group");for(size_t i=0;i<g.members.size();i++){unsigned k=g.members[i];if(k>=8192+placement_limit)return fail("Invalid group object key");for(size_t j=0;j<i;j++)if(k==g.members[j])return fail("Duplicate group object");}}
 for(size_t i=0;i<d.assets.size();i++){const auto &a=d.assets[i];if(!name(a.alias)||!relative(a.stem)||a.stem.find('.')!=std::string::npos||a.lods<1||a.lods>3||a.collision>4||a.vegetation>2||!range(a.distance,30,2000)||!range(a.split,10,500)||a.split+4>=a.distance||!range(a.units,.001,1000)||(a.vegetation==2&&a.lods<2)||(a.vegetation!=2&&a.lods>2))return fail("Invalid asset definition");for(size_t j=0;j<i;j++)if(d.assets[j].alias==a.alias)return fail("Duplicate asset definition");}
 if(complete){
 if(d.navigation.size()>4||d.actors.size()>32||d.particles.size()>16||d.waters.size()>8||d.graphs.size()>8)return fail("Runtime editor budget exceeded");
 unsigned cells=0;for(const auto &n:d.navigation){if(n.name.empty()||n.name.size()>64||n.name.find_first_of("\r\n\t")!=std::string::npos||!xyz(n.x,0,n.z)||!range(n.span,8,192)||!range(n.cell,1,4)||!range(n.radius,.2,.8)||!range(n.slope,.1,1)||std::ceil(n.span/n.cell)>96)return fail("Invalid local navigation area");unsigned side=unsigned(std::ceil(n.span/n.cell));cells+=side*side;}if(cells>32768)return fail("Navigation bake exceeds32768 cells");
 for(size_t i=0;i<d.actors.size();i++){const auto &a=d.actors[i];if(a.name.empty()||a.name.size()>64||a.name.find_first_of("\r\n\t")!=std::string::npos||a.source< -1||a.source>=140||a.navigation< -1||a.navigation>=int(d.navigation.size())||a.appearance>=10||a.points.empty()||a.points.size()>32||!range(a.speed,.3,2.5)||!range(a.dwell,.4,15)||!range(a.yaw,-180,180))return fail("Invalid actor/route");for(auto point:a.points)if(!xyz(point[0],0,point[1]))return fail("Actor point outside world");if(a.source>=0)for(size_t j=0;j<i;j++)if(d.actors[j].source==a.source)return fail("Duplicate resident override");}
 for(size_t i=0;i<d.particles.size();i++){const auto &p=d.particles[i];if(p.source< -1||p.source>=64+int(WORLD_TORCH_FLAME_CAPACITY)||p.kind>4||!xyz(p.x,p.y,p.z)||!range(p.radius,.12,15)||!range(p.height,.15,30)||!range(p.power,0,4)||!range(p.warmth,0,1)||!range(p.rate,.25,2)||!range(p.lifetime,.25,4)||!range(p.size,.25,2)||!range(p.wind,0,2)||!range(p.light,0,20)||!range(p.ceiling,p.y+.15,12000)||(p.kind==2&&(p.radius>2||p.height>4||p.ceiling<p.y+p.height*3.2)))return fail("Invalid particle emitter/ceiling");if(p.source>=0)for(size_t j=0;j<i;j++)if(d.particles[j].source==p.source)return fail("Duplicate particle source override");for(float v:p.tint)if(!range(v,0,1))return fail("Invalid VFX tint");if(!range(p.glow,0,8))return fail("Invalid VFX glow");}
 for(size_t i=0;i<d.waters.size();i++){const auto &w=d.waters[i];if(w.source>3||!xyz(w.x,w.level,w.z)||!range(w.radius_x,1,32)||!range(w.radius_z,1,32)||!range(w.roughness,.5,2)||!range(w.waves,0,2)||!range(w.flow,0,2)||!range(w.foam,0,2))return fail("Invalid water body");for(float v:w.tint)if(!range(v,.1,2))return fail("Invalid water tint");for(float v:w.absorption)if(!range(v,.25,3))return fail("Invalid water absorption");if(w.source<3)for(size_t j=0;j<i;j++)if(d.waters[j].source==w.source)return fail("Duplicate fixed water override");}
 for(size_t i=0;i<d.graphs.size();i++){if(!world_graph::validate(d.graphs[i],e))return false;for(size_t j=0;j<i;j++)if(d.graphs[i].alias==d.graphs[j].alias)return fail("Duplicate graph material target");const auto &graph=d.graphs[i];for(const auto &node:graph.nodes)if(world_graph::texture_op(node.op)&&node.value[3]>=d.surfaces.images.size())return fail("Graph texture refers to a missing surface image");if((graph.alias=="__terrain__"||graph.alias.rfind("__water_",0)==0)&&graph.outputs[5]>=0)return fail("Terrain/water graph vertex motion is unsupported; use sculpt/water controls");}
 }
 const auto &env=d.environment;if((!env.path.empty()&&(!relative(env.path)||env.path.rfind("environments/",0)!=0))||(env.enabled&&env.path.empty())||!range(env.rotation,-180,180)||!range(env.intensity,0,8)||!range(env.strength,0,1))return fail("Invalid environment definition");
 return true;
}
// Every shared edge has one value, including the unedited zero exterior.
// Validate after a complete load, not while neighboring records are pending.
bool valid(const Document &d,std::string &e){
 if(!valid_fields(d,e))return false;
 if(!world_entity::validate(d.entities,&d,e))return false;
 for(const auto &g:d.groups)for(unsigned k:g.members)if(k>=8192+d.placements.size()){e="Missing group object";return false;}
 for(const auto &t:d.terrain){
  const TerrainTile *left=nullptr,*right=nullptr,*top=nullptr,*bottom=nullptr;
  for(const auto &other:d.terrain){if(other.z==t.z){if(other.x==t.x-1)left=&other;if(other.x==t.x+1)right=&other;}if(other.x==t.x){if(other.z==t.z-1)top=&other;if(other.z==t.z+1)bottom=&other;}}
  for(unsigned i=0;i<65;i++){
   const float expected[]={left?left->delta[i*65+64]:0,right?right->delta[i*65]:0,top?top->delta[64*65+i]:0,bottom?bottom->delta[i]:0};
   const float actual[]={t.delta[i*65],t.delta[i*65+64],t.delta[i],t.delta[64*65+i]};
   for(unsigned side=0;side<4;side++)if(std::abs(expected[side]-actual[side])>.00001f){e="Terrain tile edges do not match; previous document preserved";return false;}
  }
 }return true;
}
float height_delta(const Document &d,double x,double z){
 if(d.terrain.empty()||!std::isfinite(x)||!std::isfinite(z)||std::abs(x)>50400||std::abs(z)>28400)return 0;
 const int tx=int(std::floor(x/128)),tz=int(std::floor(z/128));
 for(const auto &t:d.terrain)if(t.x==tx&&t.z==tz){double u=(x-tx*128)/2,v=(z-tz*128)/2;int ix=std::clamp(int(u),0,63),iz=std::clamp(int(v),0,63);float a=float(u-ix),b=float(v-iz);const auto &p=t.delta;// Match world_tiles.c's a-c-b / b-c-d diagonal. Bilinear
 // sampling would introduce a second surface between rendered vertices.
 const float p00=p[iz*65+ix],p10=p[iz*65+ix+1],p01=p[(iz+1)*65+ix],p11=p[(iz+1)*65+ix+1];
 return a+b<=1?p00+(p10-p00)*a+(p01-p00)*b:p11+(p01-p11)*(1-a)+(p10-p11)*(1-b);}
 return 0;
}
float vegetation(const Document &d,int kind,double x,double z){float result=1;for(const auto &p:d.stamps)if(p.kind==kind)result+=(p.value-result)*weight(x,z,p.x,p.z,p.radius);return result;}
bool brush(Document &d,double x,double z,float radius,float amount,int mode,float target,float(*base)(double,double),std::string &e){
 if(!base||!xyz(x,0,z)||!range(radius,2,32)||!range(amount,0,4)||mode<0||mode>3||!std::isfinite(target)){e="Invalid brush";return false;}
 Document next=d;
 for(int tz=int(std::floor((z-radius)/128));tz<=int(std::floor((z+radius)/128));tz++)for(int tx=int(std::floor((x-radius)/128));tx<=int(std::floor((x+radius)/128));tx++){
  auto it=std::find_if(next.terrain.begin(),next.terrain.end(),[&](const auto &t){return t.x==tx&&t.z==tz;});
  if(it==next.terrain.end()){if(next.terrain.size()==tile_limit){e="16 editable terrain tiles reached; undo or reduce the edited area";return false;}next.terrain.push_back({});it=next.terrain.end()-1;it->x=tx;it->z=tz;}
  for(int j=0;j<65;j++)for(int i=0;i<65;i++){
   const double px=tx*128.+i*2,pz=tz*128.+j*2;const float w=weight(px,pz,x,z,radius);if(w<=0)continue;
   const float old=height_delta(d,px,pz),raw=base(px,pz);float value=old;
   if(mode<2)value+=amount*w*(mode==0?1:-1);
   else if(mode==2)value+=(target-raw-old)*std::min(1.f,amount*w);
   else{float sum=0;for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++)sum+=base(px+dx*2,pz+dz*2)+height_delta(d,px+dx*2,pz+dz*2);value+=(sum/9-raw-old)*std::min(1.f,amount*w);}
   it->delta[j*65+i]=std::clamp(value,-32.f,32.f);
  }
 }
 if(!valid(next,e))return false;
 d=std::move(next);return true;
}
bool carve_pond(Document &d,double x,double z,float(*base)(double,double),std::string &e){
 if(!base||d.waters.size()>=8){e="Water body budget exceeded or missing terrain";return false;}
 const float bank=base(x,z)+height_delta(d,x,z);Document next=d;
 if(!brush(next,x,z,10,4,2,bank,base,e)||!brush(next,x,z,6,1.5,1,0,base,e))return false;
 Water w;w.source=3;w.x=x;w.z=z;w.level=bank-.4;w.radius_x=w.radius_z=5.04f;next.waters.push_back(w);
 if(!valid(next,e))return false;
 d=std::move(next);return true;
}
template<size_t N> std::array<float,8> weights(const std::array<float,N> &scores,unsigned count){
 std::array<float,8> out={};if(count>N)return out;
 const unsigned used=std::min(count,8u);const float cutoff=count>8?scores[8]:0;float sum=0;
 for(unsigned i=0;i<used;i++){
  if(!std::isfinite(scores[i])||scores[i]<=0)continue;
  if(count<=8)out[i]=1;
  else{float t=std::clamp((scores[i]-cutoff)/std::max(scores[i],.00001f)/.15f,0.f,1.f);out[i]=t*t*(3-2*t)+.015f;}
  sum+=out[i];
 }
 if(count>8&&sum>0)for(unsigned i=0;i<used;i++)out[i]=std::min(1.f,out[i]*used/sum);
 return out;
}
std::array<float,8> light_weights(const std::array<float,32> &s,unsigned n){return weights(s,n);}
std::array<float,8> light_weights(const std::array<float,64> &s,unsigned n){return weights(s,n);}
std::vector<Placement> scatter(const char *alias,double x,double z,float radius,unsigned count,float scale,std::uint32_t seed){
 std::vector<Placement> result;if(!alias||!name(alias)||!xyz(x,0,z)||!range(radius,.5,64)||count>64||!range(scale,.15,4))return result;
 std::uint32_t phase=seed+0x9e3779b9u;phase^=phase>>16;phase*=0x7feb352du;phase^=phase>>15;phase*=0x846ca68bu;phase^=phase>>16;
 result.reserve(count);for(unsigned i=0;i<count;i++){const double a=i*2.399963229728653+double(phase)*(6.283185307179586/4294967296.),r=radius*std::sqrt((i+.5)/count);Placement p;p.alias=alias;p.x=x+std::cos(a)*r;p.z=z+std::sin(a)*r;p.yaw=float(std::remainder(a*57.2957795,360.));p.scale=scale;result.push_back(p);}return result;
}
bool build_spline(const Spline &s,std::vector<Placement> &out,std::string &e){
 out.clear();Document test;test.splines.push_back(s);if(!valid(test,e)||s.points.size()<2){e="Spline needs at least two points";return false;}
 std::vector<std::array<double,2>> points=s.points;
 if(s.kind==1){points.clear();points.push_back(s.points.front());for(size_t i=0;i+1<s.points.size();i++){
     const auto p0=s.points[i?i-1:i],p1=s.points[i],p2=s.points[i+1],p3=s.points[std::min(i+2,s.points.size()-1)];
     const unsigned steps=unsigned(std::clamp(std::ceil(std::hypot(p2[0]-p1[0],p2[1]-p1[1])/2),8.,64.));
     for(unsigned j=1;j<=steps;j++){double t=double(j)/steps,t2=t*t,t3=t2*t;std::array<double,2> p;for(int k=0;k<2;k++)p[k]=.5*((2*p1[k])+(-p0[k]+p2[k])*t+(2*p0[k]-5*p1[k]+4*p2[k]-p3[k])*t2+(-p0[k]+3*p1[k]-3*p2[k]+p3[k])*t3);points.push_back(p);}
 }}
 double remaining=0;for(size_t i=1;i<points.size();i++){auto a=points[i-1],b=points[i];double dx=b[0]-a[0],dz=b[1]-a[1],length=std::hypot(dx,dz);if(length<.001)continue;
  for(double d=remaining;d<length;d+=s.spacing){if(out.size()==placement_limit){e="Spline exceeds1024 placements";out.clear();return false;}Placement p;p.alias=s.alias;p.x=a[0]+dx*d/length;p.z=a[1]+dz*d/length;p.yaw=float(std::atan2(dx,dz)*57.2957795);p.scale=s.scale;out.push_back(p);}
  double last=remaining;if(last<length)last+=std::ceil((length-last)/s.spacing)*s.spacing;remaining=last-length;
 }if(out.empty()){e="Spline needs two distinct points";return false;}return true;
}
void History::checkpoint(const Document &d){if(back.size()==32)back.erase(back.begin());back.push_back(d);forward.clear();}
bool History::undo(Document &d){if(back.empty())return false;forward.push_back(d);const auto issued=d.entities.next_id;d=std::move(back.back());d.entities.next_id=std::max(d.entities.next_id,issued);back.pop_back();return true;}
bool History::redo(Document &d){if(forward.empty())return false;back.push_back(d);const auto issued=d.entities.next_id;d=std::move(forward.back());d.entities.next_id=std::max(d.entities.next_id,issued);forward.pop_back();return true;}
bool encode(const Document &d,std::string &text,std::string &e){
 if(!valid(d,e))return false;
 // Keep the canonical bytes of legacy scenes stable: recovery envelopes bind
 // their saved base to this encoding. Use version seven only for new VFX data.
 const bool extended_vfx=std::any_of(d.particles.begin(),d.particles.end(),[](const Particle &p){return p.kind>2||p.glow!=0||p.tint!=std::array<float,3>{1,1,1};});
 std::ostringstream o;o<<std::setprecision(17)<<"VALWOOD_STUDIO "<<(extended_vfx?7:6)<<'\n';const auto &l=d.look;
 if(!d.source_signature.empty())o<<"source "<<d.source_signature<<'\n';
 o<<"look "<<l.sharpness<<' '<<l.sun<<' '<<l.sky<<' '<<l.exposure<<' '<<l.hour<<' '<<l.bearing<<' '<<l.cloud<<' '<<l.rain<<' '<<l.wind<<' '<<l.fog<<'\n';
 for(const auto &t:d.terrain){o<<"terrain "<<t.x<<' '<<t.z;for(float v:t.delta)o<<' '<<v;o<<'\n';}
 for(const auto &p:d.placements)o<<"prop "<<p.alias<<' '<<p.x<<' '<<p.z<<' '<<p.yaw<<' '<<p.scale<<' '<<p.burial<<' '<<p.elevation<<' '<<p.offset_y<<' '<<p.pitch<<' '<<p.roll<<' '<<p.sx<<' '<<p.sy<<' '<<p.sz<<'\n';
 for(const auto &p:d.overrides)o<<"override "<<p.index<<' '<<p.x<<' '<<p.z<<' '<<p.yaw<<' '<<p.scale<<' '<<p.hidden<<' '<<p.offset_y<<' '<<p.pitch<<' '<<p.roll<<' '<<p.sx<<' '<<p.sy<<' '<<p.sz<<'\n';
 for(const auto &p:d.lights)o<<"light "<<p.x<<' '<<p.y<<' '<<p.z<<' '<<p.radius<<' '<<p.power<<' '<<p.color[0]<<' '<<p.color[1]<<' '<<p.color[2]<<' '<<p.yaw<<' '<<p.pitch<<' '<<p.cone<<' '<<p.flicker<<'\n';
 for(const auto &p:d.stamps)o<<"foliage "<<p.x<<' '<<p.z<<' '<<p.radius<<' '<<p.value<<' '<<p.kind<<'\n';
 for(const auto &p:d.materials)o<<"material "<<p.alias<<' '<<p.tint[0]<<' '<<p.tint[1]<<' '<<p.tint[2]<<' '<<p.roughness<<' '<<p.normal<<' '<<p.uv<<'\n';
 for(const auto &p:d.decals)o<<"decal "<<p.x<<' '<<p.y<<' '<<p.z<<' '<<p.size[0]<<' '<<p.size[1]<<' '<<p.size[2]<<' '<<p.color[0]<<' '<<p.color[1]<<' '<<p.color[2]<<' '<<p.yaw<<' '<<p.opacity<<' '<<p.roughness<<' '<<p.kind<<'\n';
 for(const auto &p:d.splines){o<<"spline "<<p.alias<<' '<<p.spacing<<' '<<p.scale<<' '<<p.width<<' '<<p.kind<<' '<<p.points.size();for(auto v:p.points)o<<' '<<v[0]<<' '<<v[1];o<<'\n';}
 for(const auto &g:d.groups){o<<"group "<<std::quoted(g.name)<<' '<<g.members.size();for(unsigned k:g.members)o<<' '<<k;o<<'\n';}
 for(const auto &a:d.assets)o<<"asset "<<a.alias<<' '<<a.stem<<' '<<a.lods<<' '<<a.collision<<' '<<a.vegetation<<' '<<a.distance<<' '<<a.split<<' '<<a.units<<'\n';
 if(!d.environment.path.empty()){const auto &v=d.environment;o<<"environment "<<v.path<<' '<<v.rotation<<' '<<v.intensity<<' '<<v.strength<<' '<<v.enabled<<'\n';}
 for(const auto &n:d.navigation)o<<"navigation "<<std::quoted(n.name)<<' '<<n.x<<' '<<n.z<<' '<<n.span<<' '<<n.cell<<' '<<n.radius<<' '<<n.slope<<'\n';
 for(const auto &a:d.actors){o<<"actor "<<std::quoted(a.name)<<' '<<a.source<<' '<<a.navigation<<' '<<a.appearance<<' '<<a.enabled<<' '<<a.loop<<' '<<a.speed<<' '<<a.dwell<<' '<<a.yaw<<' '<<a.points.size();for(auto p:a.points)o<<' '<<p[0]<<' '<<p[1];o<<'\n';}
 for(const auto &p:d.particles){o<<"particle "<<p.source<<' '<<p.kind<<' '<<p.enabled<<' '<<p.x<<' '<<p.y<<' '<<p.z<<' '<<p.ceiling<<' '<<p.radius<<' '<<p.height<<' '<<p.power<<' '<<p.warmth<<' '<<p.rate<<' '<<p.lifetime<<' '<<p.size<<' '<<p.wind<<' '<<p.light<<' '<<p.seed;if(extended_vfx)o<<' '<<p.tint[0]<<' '<<p.tint[1]<<' '<<p.tint[2]<<' '<<p.glow;o<<'\n';}
 for(const auto &w:d.waters){o<<"water "<<w.source<<' '<<w.enabled<<' '<<w.x<<' '<<w.z<<' '<<w.level<<' '<<w.radius_x<<' '<<w.radius_z;for(float v:w.tint)o<<' '<<v;for(float v:w.absorption)o<<' '<<v;o<<' '<<w.roughness<<' '<<w.waves<<' '<<w.flow<<' '<<w.foam<<'\n';}
 for(const auto &g:d.graphs){o<<"graph "<<g.alias<<' '<<g.output_x<<' '<<g.output_y;for(int v:g.outputs)o<<' '<<v;o<<' '<<g.nodes.size()<<'\n';for(const auto &n:g.nodes){o<<"node "<<g.alias<<' '<<n.id<<' '<<unsigned(n.op);for(int v:n.inputs)o<<' '<<v;for(float v:n.value)o<<' '<<v;o<<' '<<n.x<<' '<<n.y<<'\n';}}
 std::string metadata;if(!world_entity::encode(d.entities,metadata,e))return false;std::istringstream entity_lines(metadata);std::string entity_line;while(std::getline(entity_lines,entity_line))o<<"entities "<<std::quoted(entity_line)<<'\n';
 std::string surface_metadata;if(!world_surface::encode(d.surfaces,surface_metadata,e))return false;std::istringstream surface_lines(surface_metadata);std::string surface_line;while(std::getline(surface_lines,surface_line))o<<"surfaces "<<std::quoted(surface_line)<<'\n';
 text=o.str();return true;
}
bool save(const char *path,const Document &d,std::string &e){
 if(!path||!*path)return false;
 std::string text;if(!encode(d,text,e))return false;
 std::string temp=std::string(path)+".tmp.XXXXXX";std::vector<char> name(temp.begin(),temp.end());name.push_back(0);int fd=mkstemp(name.data());if(fd<0){e="Cannot create save transaction";return false;}
 size_t done=0;bool ok=true;while(done<text.size()){ssize_t n=write(fd,text.data()+done,text.size()-done);if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}done+=size_t(n);}if(ok)ok=fsync(fd)==0;int closed=close(fd);ok=ok&&closed==0;if(ok)ok=vw_replace(name.data(),path)==0;if(!ok){unlink(name.data());e="Save failed; previous file preserved";}return ok;
}
bool load(const char *path,Document &d,std::string &e){
 e.clear();std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f){e="No saved studio document";return false;}if(f.tellg()<0||f.tellg()>4*1024*1024){e="Oversized studio document";return false;}f.seekg(0);std::string magic;int version=0;if(!(f>>magic>>version)||magic!="VALWOOD_STUDIO"||(version<1||version>7)){e="Unsupported studio document";return false;}
 std::string line;std::getline(f,line);Document next;std::vector<unsigned> graph_counts;std::string entity_metadata;unsigned entity_records=0,surface_records=0;std::string surface_metadata;bool have_look=false,have_source=false,have_environment=false;
 while(std::getline(f,line)){
  if(line.size()>100000){e="Oversized studio record";return false;}std::istringstream in(line);std::string kind,extra;if(!(in>>kind))continue;bool ok=true;
  if(kind=="source"){ok=!have_source&&bool(in>>next.source_signature);have_source=true;}
  else if(kind=="look"){auto &l=next.look;ok=!have_look&&bool(in>>l.sharpness>>l.sun>>l.sky>>l.exposure>>l.hour>>l.bearing>>l.cloud>>l.rain>>l.wind>>l.fog);have_look=true;}
  else if(kind=="terrain"){TerrainTile p;ok=bool(in>>p.x>>p.z);for(float &v:p.delta)ok=ok&&bool(in>>v);next.terrain.push_back(p);}
  else if(kind=="prop"){Placement p;ok=bool(in>>p.alias>>p.x>>p.z>>p.yaw>>p.scale>>p.burial>>p.elevation);if(version>=2)ok=ok&&bool(in>>p.offset_y);if(version>=6)ok=ok&&bool(in>>p.pitch>>p.roll>>p.sx>>p.sy>>p.sz);next.placements.push_back(p);}
  else if(kind=="override"){Override p;int hidden=0;ok=bool(in>>p.index>>p.x>>p.z>>p.yaw>>p.scale>>hidden)&&hidden>=0&&hidden<=1;p.hidden=ok&&hidden;if(version>=2)ok=ok&&bool(in>>p.offset_y);if(version>=6)ok=ok&&bool(in>>p.pitch>>p.roll>>p.sx>>p.sy>>p.sz);next.overrides.push_back(p);}
  else if(kind=="light"){Light p;int flicker=0;ok=bool(in>>p.x>>p.y>>p.z>>p.radius>>p.power>>p.color[0]>>p.color[1]>>p.color[2]>>p.yaw>>p.pitch>>p.cone>>flicker)&&flicker>=0&&flicker<=1;p.flicker=ok&&flicker;next.lights.push_back(p);}
  else if(kind=="foliage"){Stamp p;ok=bool(in>>p.x>>p.z>>p.radius>>p.value>>p.kind);next.stamps.push_back(p);}
  else if(kind=="material"){Material p;ok=bool(in>>p.alias>>p.tint[0]>>p.tint[1]>>p.tint[2]>>p.roughness>>p.normal>>p.uv);next.materials.push_back(p);}
  else if(kind=="decal"){Decal p;ok=bool(in>>p.x>>p.y>>p.z>>p.size[0]>>p.size[1]>>p.size[2]>>p.color[0]>>p.color[1]>>p.color[2]>>p.yaw>>p.opacity>>p.roughness>>p.kind);next.decals.push_back(p);}
  else if(kind=="spline"){Spline p;unsigned n=0;ok=bool(in>>p.alias>>p.spacing>>p.scale>>p.width>>p.kind>>n)&&n<=32;for(unsigned i=0;i<n&&ok;i++){std::array<double,2> v;ok=bool(in>>v[0]>>v[1]);p.points.push_back(v);}next.splines.push_back(p);}
  else if(kind=="group"&&version>=2){Group g;unsigned n=0;ok=bool(in>>std::quoted(g.name)>>n)&&n<=128;for(unsigned i=0;i<n&&ok;i++){unsigned key=0;ok=bool(in>>key);g.members.push_back(key);}next.groups.push_back(g);}
  else if(kind=="asset"&&version>=2){Asset a;ok=bool(in>>a.alias>>a.stem>>a.lods>>a.collision>>a.vegetation>>a.distance>>a.split>>a.units);next.assets.push_back(a);}
  else if(kind=="environment"&&version>=2){auto &v=next.environment;int enabled=0;ok=!have_environment&&bool(in>>v.path>>v.rotation>>v.intensity>>v.strength>>enabled)&&enabled>=0&&enabled<=1;have_environment=true;v.enabled=enabled==1;}
  else if(kind=="navigation"&&version>=3){NavigationArea n;ok=bool(in>>std::quoted(n.name)>>n.x>>n.z>>n.span>>n.cell>>n.radius>>n.slope)&&next.navigation.size()<4;next.navigation.push_back(n);}
  else if(kind=="actor"&&version>=3){Actor a;int enabled=0,loop=0;unsigned count=0;ok=bool(in>>std::quoted(a.name)>>a.source>>a.navigation>>a.appearance>>enabled>>loop>>a.speed>>a.dwell>>a.yaw>>count)&&count<=32&&enabled>=0&&enabled<=1&&loop>=0&&loop<=1&&next.actors.size()<32;a.enabled=enabled;a.loop=loop;for(unsigned i=0;i<count&&ok;i++){std::array<double,2> p;ok=bool(in>>p[0]>>p[1]);a.points.push_back(p);}next.actors.push_back(a);}
  else if(kind=="particle"&&version>=3){Particle p;int enabled=0;ok=bool(in>>p.source>>p.kind>>enabled>>p.x>>p.y>>p.z>>p.ceiling>>p.radius>>p.height>>p.power>>p.warmth>>p.rate>>p.lifetime>>p.size>>p.wind>>p.light>>p.seed)&&enabled>=0&&enabled<=1&&next.particles.size()<16;p.enabled=enabled;if(version>=7){for(float &v:p.tint)ok=ok&&bool(in>>v);ok=ok&&bool(in>>p.glow);}next.particles.push_back(p);}
  else if(kind=="water"&&version>=3){Water w;int enabled=0;ok=bool(in>>w.source>>enabled>>w.x>>w.z>>w.level>>w.radius_x>>w.radius_z)&&enabled>=0&&enabled<=1&&next.waters.size()<8;w.enabled=enabled;for(float &v:w.tint)ok=ok&&bool(in>>v);for(float &v:w.absorption)ok=ok&&bool(in>>v);ok=ok&&bool(in>>w.roughness>>w.waves>>w.flow>>w.foam);next.waters.push_back(w);}
  else if(kind=="graph"&&version>=3){world_graph::Graph g;unsigned count=0;ok=bool(in>>g.alias>>g.output_x>>g.output_y)&&next.graphs.size()<8;for(int &v:g.outputs)ok=ok&&bool(in>>v);ok=ok&&bool(in>>count)&&count<=32;for(const auto &other:next.graphs)if(other.alias==g.alias)ok=false;next.graphs.push_back(g);graph_counts.push_back(count);}
  else if(kind=="node"&&version>=3){std::string alias;world_graph::Node n;unsigned op=0;ok=bool(in>>alias>>n.id>>op)&&op<=unsigned(world_graph::Op::TextureAlpha);n.op=world_graph::Op(op);for(int &v:n.inputs)ok=ok&&bool(in>>v);for(float &v:n.value)ok=ok&&bool(in>>v);ok=ok&&bool(in>>n.x>>n.y);auto at=std::find_if(next.graphs.begin(),next.graphs.end(),[&](const auto &g){return g.alias==alias;});if(at==next.graphs.end()||at->nodes.size()>=32)ok=false;else at->nodes.push_back(n);}
  else if(kind=="surfaces"&&version>=5){std::string record;ok=bool(in>>std::quoted(record))&&surface_records++<1024&&record.size()<=8192;surface_metadata+=record+'\n';if(surface_metadata.size()>512*1024)ok=false;}
  else if(kind=="entities"&&version>=4){std::string record;ok=bool(in>>std::quoted(record))&&entity_records++<=world_entity::node_limit;entity_metadata+=record+'\n';}
  else ok=false;
  if(!ok||(in>>extra)||!valid_fields(next,e,false)){if(e.empty())e="Malformed studio record";return false;}
 }
 for(unsigned i=0;i<next.graphs.size();i++)if(next.graphs[i].nodes.size()!=graph_counts[i]){e="Incomplete graph";return false;}
 if(version>=4&&entity_metadata.empty()){e="Studio entity metadata is missing";return false;}
 if(version>=4&&!world_entity::decode(entity_metadata,next.entities,e))return false;
 if(version>=5&&!world_surface::decode(surface_metadata,next.surfaces,e))return false;
 if(!have_look||f.bad()||!valid(next,e)){if(e.empty())e="Incomplete studio document";return false;}d=std::move(next);return true;
}
}

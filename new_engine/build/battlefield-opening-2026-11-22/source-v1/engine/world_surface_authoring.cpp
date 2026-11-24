#include "engine/world_surface_authoring.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
namespace world_surface {
namespace {
bool range(double x,double a,double b){return std::isfinite(x)&&x>=a&&x<=b;}
bool label(const std::string &s){return !s.empty()&&s.size()<=64&&std::none_of(s.begin(),s.end(),[](unsigned char c){return c<32||c==127;});}
bool token(const std::string &s){return !s.empty()&&s.size()<96&&std::all_of(s.begin(),s.end(),[](char c){return(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-';});}
bool location(Point p){return range(p.x,-50400,50400)&&range(p.z,-28400,28400)&&range(p.y,-2000,12000);}
bool relative(const std::string &s){return s.rfind("surfaces/",0)==0&&s.size()<240&&s.find("..") == std::string::npos&&std::all_of(s.begin(),s.end(),[](char c){return(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='/'||c=='.';});}
double smooth(double x){x=std::clamp(x,0.,1.);return x*x*(3-2*x);}
std::array<double,3> rotate(std::array<double,3> p,const std::array<float,3> &degrees,bool inverse){
 auto axis=[&](unsigned k,double angle){unsigned a=(k+1)%3,b=(k+2)%3;double c=std::cos(angle),s=std::sin(angle),u=p[a],v=p[b];p[a]=c*u-s*v;p[b]=s*u+c*v;};
 if(inverse){for(int k=2;k>=0;k--)axis(unsigned(k),-degrees[unsigned(k)]*.017453292519943295);}
 else for(unsigned k=0;k<3;k++)axis(k,degrees[k]*.017453292519943295);
 return p;
}
}
bool validate(const Data &d,std::string &e){
 e.clear();auto fail=[&](const char *s){e=s;return false;};
 if(d.images.size()>image_limit||d.layers.size()>layer_limit||d.bindings.size()>64||d.paint.size()>paint_limit||d.projections.size()>projection_limit||d.winds.size()>wind_limit||d.ribbons.size()>ribbon_limit)return fail("Surface authoring budget exceeded");
 std::set<std::string> names;
 for(const auto &im:d.images){if(!label(im.name)||!relative(im.path)||unsigned(im.role)>2||!names.insert(im.name).second)return fail("Invalid or duplicate surface image");}
 names.clear();for(const auto &l:d.layers){if(!label(l.name)||!names.insert(l.name).second||!range(l.metres,.05,64)||!range(l.roughness,.04,1)||!range(l.metallic,0,1)||!range(l.normal_strength,0,2))return fail("Invalid material layer");for(float v:l.tint)if(!range(v,0,2))return fail("Invalid material tint");const int maps[]={l.color,l.normal,l.arm};for(unsigned k=0;k<3;k++)if(maps[k]<-1||maps[k]>=int(d.images.size())||(maps[k]>=0&&unsigned(d.images[unsigned(maps[k])].role)!=k))return fail("Material image is missing or has the wrong color space/role");}
 auto layer=[&](int i,bool source=false){return i>=(source?-1:0)&&i<int(d.layers.size());};
 names.clear();for(const auto &b:d.bindings)if(!token(b.alias)||!names.insert(b.alias).second||!layer(b.first,true)||!layer(b.second,true)||(b.first<0&&b.second<0)||unsigned(b.mode)>3||!range(b.amount,0,1)||!range(b.scale,.01,64)||!range(b.threshold,-2000,12000)||!range(b.softness,.001,100))return fail("Invalid material binding");
 for(const auto &p:d.paint)if(!location({p.x,0,p.z})||!range(p.radius,2,64)||!range(p.amount,0,1)||(!p.erase&&!layer(p.layer)))return fail("Invalid terrain paint stroke");
 for(const auto &p:d.projections){if(!label(p.name)||!layer(p.layer)||!location({p.position[0],p.position[1],p.position[2]})||!range(p.opacity,0,1)||!range(p.feather,.01,.5)||!range(p.normal_cutoff,-1,.99))return fail("Invalid image projection");for(float v:p.size)if(!range(v,.05,64))return fail("Invalid projection size");for(float v:p.rotation)if(!range(v,-360,360))return fail("Invalid projection rotation");}
 for(const auto &w:d.winds){if(!label(w.name)||!location({w.center[0],w.center[1],w.center[2]})||!range(w.speed,0,5)||!range(w.gust,0,1)||!range(w.period,.5,64)||!range(w.direction[0],-1,1)||!range(w.direction[1],-1,1)||std::hypot(w.direction[0],w.direction[1])<.01)return fail("Invalid wind volume");for(float v:w.radius)if(!range(v,.5,256))return fail("Invalid wind radius");}
 for(const auto &r:d.ribbons){if(!label(r.name)||unsigned(r.kind)>1||!layer(r.layer,true)||!range(r.width,.5,32)||!range(r.shoulder,.1,12)||!range(r.depth,.1,8)||r.points.size()<2||r.points.size()>32)return fail("Invalid road or river spline");double length=0;for(unsigned i=0;i<r.points.size();i++){if(!location(r.points[i]))return fail("Spline point outside world");if(i){double span=std::hypot(r.points[i].x-r.points[i-1].x,r.points[i].z-r.points[i-1].z);if(span<.25||span>256)return fail("Spline points require 0.25 to 256 m spacing");length+=span;}}if(length>1024)return fail("Split splines longer than 1024 m");}
 return true;
}
Weights paint_weights(const Data &d,double x,double z){
 Weights out={};if(!std::isfinite(x)||!std::isfinite(z))return out;
 for(const auto &p:d.paint){if(!range(p.radius,2,64)||!range(p.amount,0,1))continue;double q=std::hypot(x-p.x,z-p.z)/p.radius;if(!std::isfinite(q)||q>=1)continue;float a=float((1-q*q)*(1-q*q))*p.amount;for(auto &w:out)w*=1-a;if(!p.erase&&p.layer>=0&&p.layer<int(layer_limit))out[unsigned(p.layer)]+=a;}
 return out;
}
unsigned paint_hash(int x,int z){std::uint32_t a=std::uint32_t(x)*0x8da6b343u,b=std::uint32_t(z)*0xd8163841u;return (a^b)&(paint_hash_size-1);}
bool bake_paint(const Data &d,PaintField &out,std::string &e){
 if(!validate(d,e))return false;
 PaintField next;std::set<std::pair<int,int>> keys;
 std::vector<BakedRibbon> splines;
 for(const auto &r:d.ribbons)if(r.layer>=0){
  BakedRibbon spline;if(!bake_ribbon(r,spline,e))return false;
  for(int z=int(std::floor(spline.bounds[1]/128));z<=int(std::floor(spline.bounds[3]/128));z++)for(int x=int(std::floor(spline.bounds[0]/128));x<=int(std::floor(spline.bounds[2]/128));x++)keys.insert({x,z});
  if(keys.size()>paint_tile_limit){e="Spline material paint exceeds64local tiles";return false;}splines.push_back(std::move(spline));
 }
 // Riverbeds cover ground beneath crossings; bridge decks are independent geometry.
 std::stable_sort(splines.begin(),splines.end(),[](const auto &a,const auto &b){return unsigned(a.source.kind)<unsigned(b.source.kind);});
 for(const auto &p:d.paint)for(int z=int(std::floor((p.z-p.radius)/128));z<=int(std::floor((p.z+p.radius)/128));z++)for(int x=int(std::floor((p.x-p.radius)/128));x<=int(std::floor((p.x+p.radius)/128));x++){keys.insert({x,z});if(keys.size()>paint_tile_limit){e="Terrain paint exceeds 64 local tiles; reduce or clear strokes";return false;}}
 for(auto &entry:next.lookup)entry={0,0,-1,0};
 for(auto [x,z]:keys){PaintTile tile;tile.x=x;tile.z=z;for(unsigned iz=0;iz<paint_side;iz++)for(unsigned ix=0;ix<paint_side;ix++){
  const double wx=x*128.+ix*2,wz=z*128.+iz*2;auto weights=paint_weights(d,wx,wz);
  for(const auto &spline:splines){if(wx<spline.bounds[0]||wz<spline.bounds[1]||wx>spline.bounds[2]||wz>spline.bounds[3])continue;auto hit=nearest(spline,wx,wz);float a=float(1-smooth((hit.distance-spline.source.width*.5)/spline.source.shoulder));for(float &w:weights)w*=1-a;weights[unsigned(spline.source.layer)]+=a;}
  unsigned total=0;for(unsigned k=0;k<layer_limit;k++){auto v=unsigned(std::floor(std::clamp(weights[k],0.f,1.f)*255));tile.weights[(iz*paint_side+ix)*layer_limit+k]=std::uint8_t(v);total+=v;}if(total>255){e="Paint weight normalization failed";return false;}
 }
  unsigned at=paint_hash(x,z),probes=0;while(next.lookup[at][2]>=0&&probes<paint_hash_size){at=(at+1)&(paint_hash_size-1);probes++;}if(probes>=paint_hash_size){e="Paint lookup budget exceeded";return false;}next.lookup[at]={x,z,int(next.tiles.size()),0};next.tiles.push_back(std::move(tile));}
 out=std::move(next);e.clear();return true;
}
Projected project(const Projection &p,Point w,std::array<float,3> normal){
 for(float v:p.size)if(!range(v,.05,64))return {};
 if(!location(w))return {};
 double norm=std::hypot(std::hypot(normal[0],normal[1]),normal[2]);if(!std::isfinite(norm)||norm<1e-8)return {};
 for(float &v:normal)v=float(v/norm);
 auto q=rotate({w.x-p.position[0],w.y-p.position[1],w.z-p.position[2]},p.rotation,true);for(unsigned i=0;i<3;i++)q[i]/=p.size[i]*.5;double edge=std::max({std::abs(q[0]),std::abs(q[1]),std::abs(q[2])});if(edge>=1)return {};
 auto axis=rotate({0,1,0},p.rotation,false);double facing=axis[0]*normal[0]+axis[1]*normal[1]+axis[2]*normal[2];double face=smooth((facing-p.normal_cutoff)/std::max(.01,1.-p.normal_cutoff));return {float(q[0]*.5+.5),float(q[2]*.5+.5),float((1-smooth((edge-1+p.feather)/p.feather))*face*p.opacity)};
}
std::array<float,2> wind(const Data &d,Point p,double seconds,std::array<float,2> base){
 if(!std::isfinite(seconds)||!location(p))return base;
 for(const auto &w:d.winds){double n=std::hypot(w.direction[0],w.direction[1]);if(n<.01||!range(w.period,.5,64))continue;double q=0;const double v[]={p.x,p.y,p.z};for(unsigned k=0;k<3;k++){if(!range(w.radius[k],.5,256)){q=2;break;}double x=(v[k]-w.center[k])/w.radius[k];q+=x*x;}if(q>=1)continue;double weight=1-smooth(std::sqrt(q)),phase=std::remainder(seconds,w.period)*6.283185307179586/w.period+(w.seed%65536)*.0000958738;double speed=w.speed*(1+w.gust*.5*(std::sin(phase)+.35*std::sin(phase*2)));for(unsigned k=0;k<2;k++)base[k]+=float(weight*speed*w.direction[k]/n);}
 double speed=std::hypot(base[0],base[1]);if(speed>5)for(float &v:base)v=float(v*5/speed);return base;
}
bool bake_ribbon(const Ribbon &r,BakedRibbon &out,std::string &e){
 if(r.layer<-1||r.layer>=int(layer_limit)){e="Invalid spline material layer";return false;}
 Data check;check.ribbons.push_back(r);
 if(r.layer>=0)check.layers.resize(unsigned(r.layer)+1);
 for(unsigned i=0;i<check.layers.size();i++)check.layers[i].name="Layer "+std::to_string(i);
 if(!validate(check,e))return false;
 BakedRibbon next;next.source=r;next.bounds={1e30,1e30,-1e30,-1e30};
 auto append=[&](Point p){if(!next.samples.empty()&&std::hypot(p.x-next.samples.back().position.x,p.z-next.samples.back().position.z)<.01)return;double distance=next.samples.empty()?0:next.samples.back().distance+std::hypot(p.x-next.samples.back().position.x,p.z-next.samples.back().position.z);next.samples.push_back({p,{1,0},distance});};append(r.points[0]);
 for(unsigned i=0;i+1<r.points.size();i++){auto p0=r.points[i?i-1:i],p1=r.points[i],p2=r.points[i+1],p3=r.points[std::min(i+2,unsigned(r.points.size()-1))];unsigned count=unsigned(std::max(1.,std::ceil(std::hypot(p2.x-p1.x,p2.z-p1.z)/2)));for(unsigned k=1;k<=count;k++){double t=double(k)/count;auto component=[&](double a,double b,double c,double z){return r.smooth?.5*((2*b)+(-a+c)*t+(2*a-5*b+4*c-z)*t*t+(-a+3*b-3*c+z)*t*t*t):b+(c-b)*t;};append({component(p0.x,p1.x,p2.x,p3.x),component(p0.y,p1.y,p2.y,p3.y),component(p0.z,p1.z,p2.z,p3.z)});}}
 if(next.samples.size()<2||next.samples.size()>1024){e="Spline sample budget exceeded";return false;}
 for(const auto &s:next.samples)if(!location(s.position)){e="Spline overshoots world bounds";return false;}
 if(next.samples.back().distance>1152){e="Spline overshoots length budget";return false;}
 for(unsigned i=0;i<next.samples.size();i++){auto &s=next.samples[i];const auto &a=next.samples[i?i-1:i].position,&b=next.samples[std::min(i+1,unsigned(next.samples.size()-1))].position;double length=std::hypot(b.x-a.x,b.z-a.z);if(length<.001){e="Spline contains a collapsed turn";return false;}s.right={float((b.z-a.z)/length),float(-(b.x-a.x)/length)};double margin=r.width*.5+r.shoulder;next.bounds[0]=std::min(next.bounds[0],s.position.x-margin);next.bounds[1]=std::min(next.bounds[1],s.position.z-margin);next.bounds[2]=std::max(next.bounds[2],s.position.x+margin);next.bounds[3]=std::max(next.bounds[3],s.position.z+margin);}
 out=std::move(next);e.clear();return true;
}
RibbonHit nearest(const BakedRibbon &r,double x,double z){
 RibbonHit out;if(!std::isfinite(x)||!std::isfinite(z)||x<r.bounds[0]||z<r.bounds[1]||x>r.bounds[2]||z>r.bounds[3])return out;
 double best=std::numeric_limits<double>::infinity();
 for(unsigned i=1;i<r.samples.size();i++){
  auto a=r.samples[i-1].position,b=r.samples[i].position;
  double dx=b.x-a.x,dz=b.z-a.z,n=dx*dx+dz*dz;
  if(n<1e-12)continue;
  double t=std::clamp(((x-a.x)*dx+(z-a.z)*dz)/n,0.,1.);
  double distance=std::hypot(x-a.x-dx*t,z-a.z-dz*t);
  if(distance<best){best=distance;out={distance<r.source.width*.5,distance,r.samples[i-1].distance+t*std::sqrt(n),a.y+(b.y-a.y)*t};}
 }
 return out;
}
bool stage_image_import(const Data &d,const Image &image,int target,const Image *expected,Data &out,int &selected,std::string &e){
 if(target< -1){e="Invalid texture import target";return false;}
 Data next=d;Image staged=image;int pick=target;
 if(target>=0){
  if(!expected||unsigned(target)>=d.images.size()||d.images[unsigned(target)].name!=expected->name||d.images[unsigned(target)].path!=expected->path||d.images[unsigned(target)].role!=expected->role){e="The selected texture changed while importing. Select it and reimport again.";return false;}
  if(staged.role!=expected->role){e="Texture reimport must preserve its material role";return false;}
  staged.name=expected->name;next.images[unsigned(target)]=std::move(staged);
 }else{
  if(d.images.size()>=image_limit){e="Image library is full; no document entry was added.";return false;}
  for(unsigned i=0;i<=image_limit;i++){
   staged.name=image.name+(i?" "+std::to_string(i+1):"");
   if(std::none_of(d.images.begin(),d.images.end(),[&](const auto &v){return v.name==staged.name;}))break;
  }
  pick=int(next.images.size());next.images.push_back(std::move(staged));
 }
 if(!validate(next,e))return false;
 out=std::move(next);selected=pick;e.clear();return true;
}
bool encode(const Data &d,std::string &text,std::string &e){
 if(!validate(d,e))return false;
 std::ostringstream s;s<<std::setprecision(17)<<"VALWOOD_SURFACES 2\n";
 for(const auto &v:d.images)s<<"image "<<std::quoted(v.name)<<' '<<std::quoted(v.path)<<' '<<unsigned(v.role)<<'\n';
 for(const auto &v:d.layers){s<<"layer "<<std::quoted(v.name)<<' '<<v.color<<' '<<v.normal<<' '<<v.arm;for(auto x:v.tint)s<<' '<<x;s<<' '<<v.metres<<' '<<v.roughness<<' '<<v.metallic<<' '<<v.normal_strength<<' '<<v.flip_normal_y<<'\n';}
 for(const auto &v:d.bindings)s<<"bind "<<v.alias<<' '<<v.first<<' '<<v.second<<' '<<unsigned(v.mode)<<' '<<v.amount<<' '<<v.scale<<' '<<v.threshold<<' '<<v.softness<<'\n';
 for(const auto &v:d.paint)s<<"paint "<<v.x<<' '<<v.z<<' '<<v.radius<<' '<<v.amount<<' '<<v.layer<<' '<<v.erase<<'\n';
 for(const auto &v:d.projections){s<<"project "<<std::quoted(v.name)<<' '<<v.layer;for(auto x:v.position)s<<' '<<x;for(auto x:v.size)s<<' '<<x;for(auto x:v.rotation)s<<' '<<x;s<<' '<<v.opacity<<' '<<v.feather<<' '<<v.normal_cutoff<<'\n';}
 for(const auto &v:d.winds){s<<"wind "<<std::quoted(v.name);for(auto x:v.center)s<<' '<<x;for(auto x:v.radius)s<<' '<<x;for(auto x:v.direction)s<<' '<<x;s<<' '<<v.speed<<' '<<v.gust<<' '<<v.period<<' '<<v.seed<<'\n';}
 for(const auto &v:d.ribbons){s<<"ribbon "<<std::quoted(v.name)<<' '<<unsigned(v.kind)<<' '<<v.layer<<' '<<v.width<<' '<<v.shoulder<<' '<<v.depth<<' '<<v.smooth<<' '<<v.points.size();for(auto p:v.points)s<<' '<<p.x<<' '<<p.y<<' '<<p.z;s<<'\n';}
 text=s.str();e.clear();return true;
}
bool decode(const std::string &text,Data &out,std::string &e){
 if(text.size()>512*1024){e="Oversized surface document";return false;}std::istringstream f(text);std::string magic,line;unsigned version=0;if(!(f>>magic>>version)||magic!="VALWOOD_SURFACES"||(version<1||version>2)){e="Unsupported surface document";return false;}std::getline(f,line);if(line.find_first_not_of(" \t\r")!=std::string::npos){e="Malformed surface header";return false;}Data d;unsigned records=0;
 while(std::getline(f,line)){if(line.size()>8192||++records>1024){e="Surface record budget exceeded";return false;}std::istringstream s(line);std::string kind,extra;if(!(s>>kind))continue;bool ok=false;
  if(kind=="image"){Image v;unsigned role=0;ok=bool(s>>std::quoted(v.name)>>std::quoted(v.path)>>role);v.role=ImageRole(role);d.images.push_back(v);}
  else if(kind=="layer"){Layer v;ok=bool(s>>std::quoted(v.name)>>v.color>>v.normal>>v.arm>>v.tint[0]>>v.tint[1]>>v.tint[2]>>v.metres>>v.roughness>>v.metallic>>v.normal_strength);if(version>=2){int flip=0;ok=ok&&bool(s>>flip)&&flip>=0&&flip<=1;v.flip_normal_y=flip==1;}d.layers.push_back(v);}
  else if(kind=="bind"){Binding v;unsigned mode=0;ok=bool(s>>v.alias>>v.first>>v.second>>mode>>v.amount>>v.scale>>v.threshold>>v.softness);v.mode=Blend(mode);d.bindings.push_back(v);}
  else if(kind=="paint"){Paint v;int erase=0;ok=bool(s>>v.x>>v.z>>v.radius>>v.amount>>v.layer>>erase)&&erase>=0&&erase<=1;v.erase=erase==1;d.paint.push_back(v);}
  else if(kind=="project"){Projection v;ok=bool(s>>std::quoted(v.name)>>v.layer>>v.position[0]>>v.position[1]>>v.position[2]>>v.size[0]>>v.size[1]>>v.size[2]>>v.rotation[0]>>v.rotation[1]>>v.rotation[2]>>v.opacity>>v.feather>>v.normal_cutoff);d.projections.push_back(v);}
  else if(kind=="wind"){Wind v;ok=bool(s>>std::quoted(v.name)>>v.center[0]>>v.center[1]>>v.center[2]>>v.radius[0]>>v.radius[1]>>v.radius[2]>>v.direction[0]>>v.direction[1]>>v.speed>>v.gust>>v.period>>v.seed);d.winds.push_back(v);}
  else if(kind=="ribbon"){Ribbon v;unsigned type=0,count=0;int smooth=0;ok=bool(s>>std::quoted(v.name)>>type>>v.layer>>v.width>>v.shoulder>>v.depth>>smooth>>count)&&count<=32&&smooth>=0&&smooth<=1;v.kind=RibbonKind(type);v.smooth=smooth==1;for(unsigned i=0;i<count&&ok;i++){Point p;ok=bool(s>>p.x>>p.y>>p.z);v.points.push_back(p);}d.ribbons.push_back(v);}
  if(!ok||(s>>extra)){e="Malformed surface record: "+kind;return false;}
  if(d.images.size()>image_limit||d.layers.size()>layer_limit||d.bindings.size()>64||d.paint.size()>paint_limit||d.projections.size()>projection_limit||d.winds.size()>wind_limit||d.ribbons.size()>ribbon_limit){e="Surface authoring budget exceeded";return false;}
 }
 if(!validate(d,e))return false;
 out=std::move(d);e.clear();return true;
}
}

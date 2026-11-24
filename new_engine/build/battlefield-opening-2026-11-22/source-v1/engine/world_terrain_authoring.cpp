#include "engine/world_platform.h"
#include "engine/world_terrain_authoring.h"
#include "engine/world_surface_field.h"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <set>
#include <unistd.h>
namespace world_terrain_authoring {
namespace {
constexpr unsigned node_budget=70000;
constexpr double spacing=2.,degrees=57.29577951308232;
bool range(double x,double lo,double hi){return std::isfinite(x)&&x>=lo&&x<=hi;}
bool position(double x,double z){return range(x,-50400,50400)&&range(z,-28400,28400);}
double smooth(double x){x=std::clamp(x,0.,1.);return x*x*(3-2*x);}
bool region_ok(const Region &r,std::string &e){
 if(r.columns<2||r.rows<2||r.columns>1025||r.rows>1025||std::uint64_t(r.columns)*r.rows>node_budget||!position(r.x,r.z)||!position(r.x+(r.columns-1)*spacing,r.z+(r.rows-1)*spacing)||std::abs(std::remainder(r.x,spacing))>1e-7||std::abs(std::remainder(r.z,spacing))>1e-7){e="Terrain region must fit the global2m lattice, atlas and70000-node budget";return false;}
 return true;
}
bool height(const world_studio::Document &d,Source s,double x,double z,double &value,std::string &e){
 if(!s.base||!position(x,z)){e="Missing terrain base sampler or sample outside atlas";return false;}
 const double base=s.base(x,z);value=base+world_studio::height_delta(d,x,z);
 if(!range(base,-2000,12000)||!std::isfinite(value)){e="Non-finite or unsupported base height";return false;}
 return true;
}
bool map_ok(const Heightmap &m,std::string &e){
 if(!region_ok(m.region,e))return false;
 if(!range(m.minimum,-2000,12000)||!range(m.maximum,-2000,12000)||m.maximum-m.minimum<.01||m.samples.size()!=std::size_t(m.region.columns)*m.region.rows){e="Invalid normalized16-bit heightmap range or samples";return false;}
 return true;
}
struct Grid {Region region;std::vector<double> original,values;};
bool read_grid(const world_studio::Document &d,const Region &r,Source s,Grid &g,std::string &e){
 if(!world_studio::valid(d,e)||!region_ok(r,e))return false;
 Grid next;next.region=r;next.original.resize(std::size_t(r.columns)*r.rows);
 for(unsigned j=0;j<r.rows;j++)for(unsigned i=0;i<r.columns;i++)if(!height(d,s,r.x+i*spacing,r.z+j*spacing,next.original[j*r.columns+i],e))return false;
 next.values=next.original;g=std::move(next);return true;
}
double border_weight(const Region &r,unsigned x,unsigned z,float border){return smooth(std::min({x*spacing,z*spacing,(r.columns-1-x)*spacing,(r.rows-1-z)*spacing})/border);}
bool prepare_tiles(world_studio::Document &d,const Region &r,std::string &e){
 const int x0=int(std::floor(r.x/128)),z0=int(std::floor(r.z/128));
 const int x1=int(std::ceil((r.x+(r.columns-1)*spacing)/128))-1,z1=int(std::ceil((r.z+(r.rows-1)*spacing)/128))-1;
 for(int z=z0;z<=z1;z++)for(int x=x0;x<=x1;x++){
  if(std::none_of(d.terrain.begin(),d.terrain.end(),[&](const auto &t){return t.x==x&&t.z==z;})){
   if(d.terrain.size()>=world_studio::tile_limit){e="Operation exceeds the "+std::to_string(world_studio::tile_limit)+" editable 128m terrain tiles";return false;}
   world_studio::TerrainTile t;t.x=x;t.z=z;d.terrain.push_back(t);
  }
 }
 return true;
}
bool publish(world_studio::Document &d,const Grid &g,Source source,Report &report,std::string &e){
 Report result;const auto &r=g.region;std::vector<float> delta(g.values.size());
 for(unsigned z=0;z<r.rows;z++)for(unsigned x=0;x<r.columns;x++){
  const auto i=z*r.columns+x;const double px=r.x+x*spacing,pz=r.z+z*spacing,base=source.base(px,pz),value=g.values[i];
  if(!std::isfinite(value)||!range(base,-2000,12000)){e="Terrain algorithm or base sampler produced a non-finite/out-of-range sample";return false;}
  const double unbounded=value-base;delta[i]=float(std::clamp(unbounded,-32.,32.));if(std::abs(unbounded-delta[i])>1e-5)result.clamped_nodes++;
  const double final=base+delta[i],change=std::abs(final-g.original[i]);result.height_sum_before+=g.original[i];result.height_sum_after+=final;
  if(change>1e-5){if(source.protected_point&&source.protected_point(px,pz,source.context)){e="Terrain operation overlaps a protected foundation, water or navigation region";return false;}result.changed_nodes++;result.largest_change=std::max(result.largest_change,change);}
 }
 if(!result.changed_nodes){report=result;return true;}
 auto next=d;if(!prepare_tiles(next,r,e))return false;result.added_tiles=unsigned(next.terrain.size()-d.terrain.size());
 // Write every duplicate edge sample, including previously existing neighbors.
 for(auto &tile:next.terrain)for(unsigned z=0;z<65;z++)for(unsigned x=0;x<65;x++){
  const double px=tile.x*128.+x*spacing,pz=tile.z*128.+z*spacing;const double gx=(px-r.x)/spacing,gz=(pz-r.z)/spacing;
  if(gx>=0&&gz>=0&&gx<r.columns&&gz<r.rows)tile.delta[z*65+x]=delta[std::size_t(gz)*r.columns+std::size_t(gx)];
 }
 if(!world_studio::valid(next,e))return false;
 d=std::move(next);report=result;return true;
}
std::uint32_t hash(std::uint32_t v){v^=v>>16;v*=0x7feb352du;v^=v>>15;v*=0x846ca68bu;return v^(v>>16);}
double landscape_noise(double x,double z,double scale,std::uint32_t seed){
 const double gx=x/scale,gz=z/scale;const int ix=int(std::floor(gx)),iz=int(std::floor(gz));
 const double u=smooth(gx-ix),v=smooth(gz-iz);
 auto value=[&](int px,int pz){return double(hash(seed^hash(std::uint32_t(px))^hash(std::uint32_t(pz)+0x9e3779b9u)))/4294967295.;};
 const double a=value(ix,iz)*(1-u)+value(ix+1,iz)*u,b=value(ix,iz+1)*(1-u)+value(ix+1,iz+1)*u;
 return a*(1-v)+b*v;
}
struct RiverGround {const world_studio::Document *document;Source source;};
float river_ground(double x,double z,void *opaque){
 const auto &context=*static_cast<RiverGround *>(opaque);double result=0;std::string error;
 return height(*context.document,context.source,x,z,result,error)?float(result):std::numeric_limits<float>::quiet_NaN();
}
bool eligible(const world_studio::Document &d,Source source,double x,double z,double radius,const Biome &b,bool &accepted,std::string &e){
 accepted=false;const double reach=radius+2.9;
 for(double pz=std::floor((z-reach)/2)*2;pz<=z+reach;pz+=2)for(double px=std::floor((x-reach)/2)*2;px<=x+reach;px+=2){
  if(std::hypot(px-x,pz-z)>reach)continue;
  double h=0,l=0,r=0,t=0,bt=0;
  if(!height(d,source,px,pz,h,e)||!height(d,source,px-2,pz,l,e)||!height(d,source,px+2,pz,r,e)||!height(d,source,px,pz-2,t,e)||!height(d,source,px,pz+2,bt,e))return false;
  const double slope=std::atan(std::hypot((r-l)/4,(bt-t)/4))*degrees;
  if(h<b.minimum_height||h>b.maximum_height||slope<b.minimum_slope||slope>b.maximum_slope||(source.protected_point&&source.protected_point(px,pz,source.context)))return true;
 }
 accepted=true;return true;
}
bool corridor_masks(world_studio::Document &d,const Corridor &c,unsigned &added,std::string &e){
 added=0;if(!c.vegetation_layers)return true;const double radius=c.half_width+c.bank_width*.5,step=std::max(2.,radius*1.4);double carry=0;
 std::vector<std::array<double,2>> roots;
 for(size_t i=1;i<c.points.size();i++){
  const auto &a=c.points[i-1],&b=c.points[i];const double dx=b.x-a.x,dz=b.z-a.z,length=std::hypot(dx,dz);
  for(double t=carry;t<length;t+=step)roots.push_back({a.x+dx*t/length,a.z+dz*t/length});
  double last=carry;if(last<length)last+=std::ceil((length-last)/step)*step;carry=last-length;
 }
 const auto &end=c.points.back();if(roots.empty()||std::hypot(roots.back()[0]-end.x,roots.back()[1]-end.z)>step*.5)roots.push_back({end.x,end.z});
 unsigned layers=0;for(unsigned i=0;i<4;i++)if(c.vegetation_layers&(1u<<i))layers++;
 if(roots.size()*layers+d.stamps.size()>world_studio::stamp_limit){e="Corridor vegetation masks exceed32 strokes; shorten the corridor or use fewer layers";return false;}
 for(const auto &p:roots)for(unsigned kind=0;kind<4;kind++)if(c.vegetation_layers&(1u<<kind)){world_studio::Stamp stamp;stamp.x=p[0];stamp.z=p[1];stamp.radius=float(radius);stamp.value=c.vegetation_density;stamp.kind=int(kind);d.stamps.push_back(stamp);added++;}
 return true;
}
}
bool sample_heightmap(const world_studio::Document &d,const Region &r,float minimum,float maximum,Source source,Heightmap &out,std::string &e){
 e.clear();Heightmap result;result.region=r;result.minimum=minimum;result.maximum=maximum;
 if(!region_ok(r,e)||!range(minimum,-2000,12000)||!range(maximum,-2000,12000)||maximum-minimum<.01||!world_studio::valid(d,e)){if(e.empty())e="Choose a valid heightmap metre range";return false;}
 result.samples.resize(std::size_t(r.columns)*r.rows);
 for(unsigned z=0;z<r.rows;z++)for(unsigned x=0;x<r.columns;x++){double h=0;if(!height(d,source,r.x+x*spacing,r.z+z*spacing,h,e))return false;if(h<minimum||h>maximum){e="Sample exceeds heightmap metre range; widen export range";return false;}result.samples[z*r.columns+x]=std::uint16_t(std::llround((h-minimum)*65535./(maximum-minimum)));}
 out=std::move(result);return true;
}
bool save_heightmap(const char *path,const Heightmap &m,std::string &e){
 e.clear();if(!path||!*path||!map_ok(m,e))return false;std::ostringstream h;h<<std::setprecision(17)<<"P5\n# VALWOOD_HEIGHTMAP 1 "<<m.region.x<<' '<<m.region.z<<' '<<m.minimum<<' '<<m.maximum<<"\n"<<m.region.columns<<' '<<m.region.rows<<"\n65535\n";
 std::string bytes=h.str();bytes.reserve(bytes.size()+m.samples.size()*2);for(auto v:m.samples){bytes.push_back(char(v>>8));bytes.push_back(char(v&255));}
 std::string pattern=std::string(path)+".tmp.XXXXXX";std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);int fd=mkstemp(name.data());if(fd<0){e="Cannot create heightmap save transaction";return false;}bool ok=true;size_t done=0;
 while(done<bytes.size()){ssize_t n=write(fd,bytes.data()+done,bytes.size()-done);if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}done+=size_t(n);}if(ok)ok=fsync(fd)==0;if(close(fd)!=0)ok=false;if(ok)ok=vw_replace(name.data(),path)==0;if(!ok){unlink(name.data());e="Heightmap save failed; previous file preserved";}return ok;
}
bool load_heightmap(const char *path,Heightmap &out,std::string &e){
 e.clear();if(!path||!*path){e="Missing heightmap path";return false;}std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f||f.tellg()<0||f.tellg()>std::streamoff(node_budget*2+1024)){e="Missing or oversized heightmap";return false;}f.seekg(0);
 std::string magic,meta,size,max;Heightmap m;if(!std::getline(f,magic)||!std::getline(f,meta)||!std::getline(f,size)||!std::getline(f,max)||magic!="P5"||max!="65535"||meta.size()>512||size.size()>64){e="Expected metadata-bearing16-bit Valwood PGM P5";return false;}
 std::istringstream metadata(meta),dimensions(size);std::string hashmark,tag,extra;unsigned version=0;if(!(metadata>>hashmark>>tag>>version>>m.region.x>>m.region.z>>m.minimum>>m.maximum)||(metadata>>extra)||hashmark!="#"||tag!="VALWOOD_HEIGHTMAP"||version!=1||!(dimensions>>m.region.columns>>m.region.rows)||(dimensions>>extra)||!region_ok(m.region,e)){if(e.empty())e="Malformed heightmap metadata";return false;}
 m.samples.resize(std::size_t(m.region.columns)*m.region.rows);for(auto &v:m.samples){unsigned char bytes[2];if(!f.read(reinterpret_cast<char *>(bytes),2)){e="Truncated16-bit heightmap pixels";return false;}v=std::uint16_t((unsigned(bytes[0])<<8)|bytes[1]);}char trailing=0;if(f.get(trailing)||!map_ok(m,e)){if(e.empty())e="Trailing heightmap bytes";return false;}out=std::move(m);return true;
}
bool import_heightmap(world_studio::Document &d,const Heightmap &m,const Import &o,Source source,Report &out,std::string &e){
 e.clear();if(!map_ok(m,e)||!range(o.strength,0,1)||!range(o.border,2,64)){if(e.empty())e="Invalid heightmap blend settings";return false;}Grid g;if(!read_grid(d,m.region,source,g,e))return false;
 for(unsigned z=0;z<m.region.rows;z++)for(unsigned x=0;x<m.region.columns;x++){const auto i=z*m.region.columns+x;const double h=m.minimum+double(m.samples[i])*(m.maximum-m.minimum)/65535.;g.values[i]+=o.strength*border_weight(m.region,x,z,o.border)*(h-g.original[i]);}
 return publish(d,g,source,out,e);
}
bool erode(world_studio::Document &d,const Erosion &o,Source source,Report &out,std::string &e){
 e.clear();if(o.iterations<1||o.iterations>256||!range(o.rain,0,.5)||!range(o.evaporation,0,1)||!range(o.capacity,0,12)||!range(o.erosion,0,1)||!range(o.deposition,0,1)||!range(o.thermal,0,1)||!range(o.talus,0,4)||!range(o.border,2,64)){e="Invalid bounded erosion parameters";return false;}Grid g;if(!read_grid(d,o.region,source,g,e))return false;
 const auto &r=o.region;const size_t count=g.values.size();std::vector<double> water(count),sediment(count),next_water(count),next_sediment(count),velocity(count),slope(count),change(count);
 auto interior=[&](unsigned x,unsigned z){return x>0&&z>0&&x+1<r.columns&&z+1<r.rows;};
 for(unsigned iteration=0;iteration<o.iterations;iteration++){
  for(unsigned z=1;z+1<r.rows;z++)for(unsigned x=1;x+1<r.columns;x++){const auto i=z*r.columns+x;water[i]+=o.rain*(.9+.2*(hash(o.seed^std::uint32_t(i)^hash(iteration))/4294967295.));}
  next_water=water;next_sediment=sediment;std::fill(velocity.begin(),velocity.end(),0);std::fill(slope.begin(),slope.end(),0);
  for(unsigned z=1;z+1<r.rows;z++)for(unsigned x=1;x+1<r.columns;x++){
   const auto i=z*r.columns+x;const unsigned nx[]={x-1,x+1,x,x},nz[]={z,z,z-1,z+1};double head[4]={},sum=0;
   for(unsigned k=0;k<4;k++)if(interior(nx[k],nz[k])){const auto j=nz[k]*r.columns+nx[k];head[k]=std::max(0.,g.values[i]+water[i]-g.values[j]-water[j]);sum+=head[k];slope[i]=std::max(slope[i],std::abs(g.values[i]-g.values[j])/spacing);}
   if(sum<=1e-12||water[i]<=1e-12)continue;
   const double flow=std::min(water[i],sum*.25),carry=sediment[i]*flow/water[i];velocity[i]=flow/water[i];next_water[i]-=flow;next_sediment[i]-=carry;
   for(unsigned k=0;k<4;k++)if(head[k]>0){const auto j=nz[k]*r.columns+nx[k];next_water[j]+=flow*head[k]/sum;next_sediment[j]+=carry*head[k]/sum;}
  }
  for(unsigned z=1;z+1<r.rows;z++)for(unsigned x=1;x+1<r.columns;x++){
   const auto i=z*r.columns+x;water[i]=std::max(0.,next_water[i])*(1-o.evaporation);sediment[i]=std::max(0.,next_sediment[i]);const double capacity=o.capacity*velocity[i]*std::max(.01,slope[i])*water[i];
   if(sediment[i]>capacity){const double deposited=(sediment[i]-capacity)*o.deposition;g.values[i]+=deposited;sediment[i]-=deposited;}else{const double removed=std::min(.15,(capacity-sediment[i])*o.erosion);g.values[i]-=removed;sediment[i]+=removed;}
  }
  if(o.thermal>0){std::fill(change.begin(),change.end(),0);for(unsigned z=1;z+1<r.rows;z++)for(unsigned x=1;x+1<r.columns;x++){
    const auto i=z*r.columns+x;for(auto offset:std::array<std::array<unsigned,2>,2>{{{{1,0}},{{0,1}}}}){const unsigned xx=x+offset[0],zz=z+offset[1];if(!interior(xx,zz))continue;const auto j=zz*r.columns+xx;const double difference=g.values[i]-g.values[j],excess=std::abs(difference)-o.talus*spacing;if(excess<=0)continue;const double amount=std::min(.25,excess*o.thermal*.125)*(difference>0?1:-1);change[i]-=amount;change[j]+=amount;}
   }for(size_t i=0;i<count;i++)g.values[i]+=change[i];}
 }
 for(unsigned z=0;z<r.rows;z++)for(unsigned x=0;x<r.columns;x++){const auto i=z*r.columns+x;g.values[i]+=sediment[i];g.values[i]=g.original[i]+(g.values[i]-g.original[i])*border_weight(r,x,z,o.border);}
 return publish(d,g,source,out,e);
}
bool paint_biome(world_studio::Document &d,const Biome &b,Source source,Report &out,std::string &e){
 e.clear();if(!world_studio::valid(d,e)||!position(b.x-b.radius-5,b.z-b.radius-5)||!position(b.x+b.radius+5,b.z+b.radius+5)||!range(b.radius,2,64)||!range(b.density,0,1)||b.layer<0||b.layer>3||b.divisions<1||b.divisions>5||!range(b.minimum_height,-2000,12000)||!range(b.maximum_height,b.minimum_height,12000)||!range(b.minimum_slope,0,90)||!range(b.maximum_slope,b.minimum_slope,90)){if(e.empty())e="Invalid biome brush or terrain constraints";return false;}
 const double cell=2*b.radius/b.divisions,radius=cell*.48;if(radius<.5){e="Biome brush subdivisions are too small for foliage masks";return false;}auto next=d;Report result;
 for(unsigned z=0;z<b.divisions;z++)for(unsigned x=0;x<b.divisions;x++){
  const double px=b.x-b.radius+(x+.5)*cell,pz=b.z-b.radius+(z+.5)*cell;if(std::hypot(px-b.x,pz-b.z)+radius>b.radius+1e-6)continue;bool accepted=false;if(!eligible(d,source,px,pz,radius,b,accepted,e))return false;if(!accepted)continue;
  if(next.stamps.size()>=world_studio::stamp_limit){e="Biome paint exceeds32 persistent foliage masks";return false;}world_studio::Stamp stamp;stamp.x=px;stamp.z=pz;stamp.radius=float(radius);stamp.value=b.density;stamp.kind=b.layer;next.stamps.push_back(stamp);result.added_masks++;
 }
 if(!result.added_masks){e="No brush footprint satisfies the height/slope/protection constraints";return false;}if(!world_studio::valid(next,e))return false;d=std::move(next);out=result;return true;
}
bool sculpt_corridor(world_studio::Document &d,const Corridor &c,Source source,Report &out,std::string &e){
 e.clear();if(c.points.size()<2||c.points.size()>32||(c.kind!=CorridorKind::Road&&c.kind!=CorridorKind::River)||!range(c.half_width,.5,16)||!range(c.bank_width,2,32)||!range(c.depth,0,16)||!range(c.maximum_grade,0,1)||c.vegetation_layers>15||!range(c.vegetation_density,0,1)){e="Invalid road/river corridor settings";return false;}
 double minx=std::numeric_limits<double>::infinity(),minz=minx,maxx=-minx,maxz=-minx;
 for(size_t i=0;i<c.points.size();i++){const auto &p=c.points[i];if(!position(p.x,p.z)||!range(p.height,-2000,12000)){e="Invalid corridor control point";return false;}minx=std::min(minx,p.x);minz=std::min(minz,p.z);maxx=std::max(maxx,p.x);maxz=std::max(maxz,p.z);if(i){const auto &a=c.points[i-1];const double length=std::hypot(p.x-a.x,p.z-a.z);if(length<.5||std::abs(p.height-a.height)>c.maximum_grade*length+1e-6||(c.kind==CorridorKind::River&&p.height>a.height+1e-6)){e="Corridor has coincident points, excessive grade or an uphill river segment";return false;}}}
 const double outer=c.half_width+c.bank_width;minx=std::floor((minx-outer-2)/2)*2;minz=std::floor((minz-outer-2)/2)*2;maxx=std::ceil((maxx+outer+2)/2)*2;maxz=std::ceil((maxz+outer+2)/2)*2;
 Region region{minx,minz,unsigned((maxx-minx)/2)+1,unsigned((maxz-minz)/2)+1};Grid g;if(!read_grid(d,region,source,g,e))return false;
 for(unsigned z=0;z<region.rows;z++)for(unsigned x=0;x<region.columns;x++){
  const auto index=z*region.columns+x;const double px=minx+x*2,pz=minz+z*2;double best=outer+1,grade=0;
  for(size_t i=1;i<c.points.size();i++){const auto &a=c.points[i-1],&b=c.points[i];const double dx=b.x-a.x,dz=b.z-a.z,t=std::clamp(((px-a.x)*dx+(pz-a.z)*dz)/(dx*dx+dz*dz),0.,1.),distance=std::hypot(px-a.x-dx*t,pz-a.z-dz*t);if(distance<best){best=distance;grade=a.height+(b.height-a.height)*t;}}
  if(best>=outer)continue;
  if(source.protected_point&&source.protected_point(px,pz,source.context)){e="Road/river banks or vegetation overlap a protected region";return false;}
  const double weight=best<=c.half_width?1:smooth(1-(best-c.half_width)/c.bank_width);double desired=grade;if(c.kind==CorridorKind::River&&best<c.half_width)desired-=c.depth*(1-(best/c.half_width)*(best/c.half_width));g.values[index]+=weight*(desired-g.original[index]);
 }
 auto next=d;unsigned masks=0;if(!corridor_masks(next,c,masks,e))return false;Report result;if(!publish(next,g,source,result,e)||!world_studio::valid(next,e))return false;result.added_masks=masks;d=std::move(next);out=result;return true;
}
bool simple_brush(world_studio::Document &d,double cx,double cz,float radius,float strength,
                  SimpleBrushMode mode,double target,std::uint32_t seed,Source source,Report &out,std::string &e){
 e.clear();
 if(!range(radius,8,256)||!range(strength,.1,8)||unsigned(mode)>unsigned(SimpleBrushMode::RollingHills)||
    !range(target,-2000,12000)||!position(cx-radius,cz-radius)||!position(cx+radius,cz+radius)){
  e="Choose an 8–256m brush, .1–8 strength and a valid terrain position/height";return false;
 }
 const double minx=std::floor((cx-radius)/spacing)*spacing,minz=std::floor((cz-radius)/spacing)*spacing;
 const double maxx=std::ceil((cx+radius)/spacing)*spacing,maxz=std::ceil((cz+radius)/spacing)*spacing;
 Region region{minx,minz,unsigned(std::llround((maxx-minx)/spacing))+1,unsigned(std::llround((maxz-minz)/spacing))+1};
 Grid grid;if(!read_grid(d,region,source,grid,e))return false;unsigned protected_nodes=0;
 const double blend=1-std::exp(-double(strength)*.5);
 for(unsigned z=0;z<region.rows;z++)for(unsigned x=0;x<region.columns;x++){
  const auto i=z*region.columns+x;const double px=minx+x*spacing,pz=minz+z*spacing,distance=std::hypot(px-cx,pz-cz);
  if(distance>=radius)continue;
  const double weight=smooth(1-distance/radius);
  if(source.protected_point&&source.protected_point(px,pz,source.context)){protected_nodes++;continue;}
  if(mode==SimpleBrushMode::Raise)grid.values[i]+=strength*weight;
  else if(mode==SimpleBrushMode::Lower)grid.values[i]-=strength*weight;
  else if(mode==SimpleBrushMode::Flatten)grid.values[i]+=(target-grid.original[i])*blend*weight;
  else if(mode==SimpleBrushMode::Smooth){
   double sum=0;unsigned count=0;
   for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){
    const int xx=int(x)+dx,zz=int(z)+dz;if(xx<0||zz<0||xx>=int(region.columns)||zz>=int(region.rows))continue;
    sum+=grid.original[unsigned(zz)*region.columns+unsigned(xx)];count++;
   }
   grid.values[i]+=(sum/count-grid.original[i])*blend*weight;
  }else{
   // World-anchored frequencies and seeds keep adjacent strokes coherent;
   // three octaves add broad hills, intermediate ridges and gentle variation.
   const double noise=.65*landscape_noise(px,pz,96,seed)+.25*landscape_noise(px,pz,40,seed^0xa511e9b3u)+.10*landscape_noise(px,pz,16,seed^0x63d83595u);
   grid.values[i]+=strength*(noise*2-1)*weight;
  }
 }
 auto next=d;Report result;if(!publish(next,grid,source,result,e))return false;
 if(!next.surfaces.ribbons.empty()){
  RiverGround ground{&next,source};auto field=std::make_shared<world_surface::TerrainField>();
  if(!world_surface::bake_terrain(next.surfaces,river_ground,&ground,*field,e)||!combined_height_valid(next,*field,e))return false;
  next.surface_field=std::move(field);
 }else next.surface_field.reset();
 result.protected_nodes=protected_nodes;d=std::move(next);out=result;return true;
}
bool simple_pond(world_studio::Document &d,double cx,double cz,float radius,float depth,
                 Source source,Report &out,std::string &e){
 e.clear();
 if(!range(radius,6,32)||!range(depth,.5,6)||!source.base){
  e="Choose a pond/lake diameter of 12–64m and depth .5–6m";return false;
 }
 if(!world_studio::valid(d,e))return false;
 if(d.waters.size()>=8){e="All 8 water edit slots are used. Remove a water edit before creating another pond or lake";return false;}
 const double fringe=2.9,bank=std::max(4.,double(radius)*.25),outer=radius+bank,reach=outer+fringe;
 if(!position(cx-reach,cz-reach)||!position(cx+reach,cz+reach)){e="The pond/lake and its banks must stay inside the world bounds";return false;}
 // A center test alone misses a plant standing between lattice nodes or a
 // road outside the water ellipse but inside its graded banks. The caller's
 // protection sampler must cover the complete prospective ground footprint.
 if(source.protected_point&&source.protected_point(cx,cz,source.context)){e="Pond/lake overlaps a protected building, object, water or route; choose open ground";return false;}
 for(const auto &w:d.waters)if(w.source==3&&w.enabled&&std::hypot(cx-w.x,cz-w.z)<=reach+std::max(w.radius_x,w.radius_z)){
  e="Pond/lake banks overlap an existing pond or lake; move the new center farther away";return false;
 }
 // Sculpting beneath any authored corridor changes its derived field. Keep
 // both road grades and river banks untouched, including interpolation reach.
 for(const auto &r:d.surfaces.ribbons){world_surface::BakedRibbon path;if(!world_surface::bake_ribbon(r,path,e))return false;
  const double clearance=reach+r.width*.5+r.shoulder+fringe;
  for(unsigned i=1;i<path.samples.size();i++){const auto &a=path.samples[i-1].position,&b=path.samples[i].position;
   const double dx=b.x-a.x,dz=b.z-a.z,length2=dx*dx+dz*dz,t=length2>1e-12?std::clamp(((cx-a.x)*dx+(cz-a.z)*dz)/length2,0.,1.):0;
   if(std::hypot(cx-a.x-dx*t,cz-a.z-dz*t)<=clearance){e="Pond/lake banks overlap an existing road or river; move the center farther away";return false;}}}
 const double minx=std::floor((cx-reach)/spacing)*spacing,minz=std::floor((cz-reach)/spacing)*spacing;
 const double maxx=std::ceil((cx+reach)/spacing)*spacing,maxz=std::ceil((cz+reach)/spacing)*spacing;
 Region region{minx,minz,unsigned(std::llround((maxx-minx)/spacing))+1,unsigned(std::llround((maxz-minz)/spacing))+1};
 Grid grid;if(!read_grid(d,region,source,grid,e))return false;
 double center=0;if(!height(d,source,cx,cz,center,e))return false;
 const double freeboard=.30,level=center-freeboard,crest=level+freeboard,bed=level-depth;
 if(!range(level,.1,11990)){e="Choose dry ground above sea level for a pond/lake";return false;}
 // Every triangle supporting a point on the water cutoff is at most
 // sqrt(8)m from it. A 2.9m dry annulus therefore encloses the FULL ellipse,
 // rather than merely the 32 validation samples or a few cardinal points.
 const double inner=radius-fringe,flat=inner*.45,rim_outer=radius+fringe;
 for(unsigned z=0;z<region.rows;z++)for(unsigned x=0;x<region.columns;x++){
  const auto i=z*region.columns+x;const double px=minx+x*spacing,pz=minz+z*spacing,distance=std::hypot(px-cx,pz-cz);
  if(distance>reach)continue;
  if(source.protected_point&&source.protected_point(px,pz,source.context)){e="Pond/lake banks overlap a protected building, object, water or route; choose open ground";return false;}
  if(distance>=outer)continue;
  const double dry=std::max(grid.original[i],crest);
  if(distance<inner){const double blend=smooth((distance-flat)/(inner-flat));grid.values[i]=bed+(dry-bed)*blend;}
  else if(distance<=rim_outer)grid.values[i]=dry;
  else grid.values[i]=grid.original[i]+(dry-grid.original[i])*smooth((outer-distance)/(outer-rim_outer));
 }
 auto next=d;Report result;if(!publish(next,grid,source,result,e))return false;
 if(result.clamped_nodes){e="Pond/lake grading exceeds the 32m terrain limit on this slope; choose flatter ground or a smaller lake";return false;}
 if(!next.surfaces.ribbons.empty()){
  RiverGround ground{&next,source};auto field=std::make_shared<world_surface::TerrainField>();
  if(!world_surface::bake_terrain(next.surfaces,river_ground,&ground,*field,e)||!combined_height_valid(next,*field,e))return false;
  next.surface_field=std::move(field);
 }else next.surface_field.reset();
 auto final_height=[&](double x,double z){return double(source.base(x,z))+world_studio::height_delta(next,x,z)+(next.surface_field?world_surface::terrain_offset(*next.surface_field,x,z):0);};
 if(!std::isfinite(final_height(cx,cz))||level-final_height(cx,cz)<.08){e="This pond/lake cannot reach a submerged center on the ground grid; choose flatter ground";return false;}
 for(unsigned i=0;i<256;i++){const double a=i*6.283185307179586/256,h=final_height(cx+std::cos(a)*radius,cz+std::sin(a)*radius);
  if(!std::isfinite(h)||h<level+.035){e="This pond/lake cannot form a closed dry bank here; choose flatter ground";return false;}}
 world_studio::Water water;water.source=3;water.x=cx;water.z=cz;water.level=level;water.radius_x=water.radius_z=radius;
 // Neutral freshwater preserves the basin's natural earth/stone color at
 // shallow banks instead of baking a cyan swimming-pool grade into the style.
 water.tint={.92f,.98f,.94f};water.absorption={1.8f,2.f,2.2f};water.roughness=1.05f;water.waves=.55f;water.foam=.25f;
 next.waters.push_back(water);
 if(!world_studio::valid(next,e))return false;
 d=std::move(next);out=result;return true;
}
bool simple_river(world_studio::Document &d,const std::vector<Point> &points,
                  float width,float banks,float depth,Source source,Report &out,std::string &e){
 e.clear();if(!world_studio::valid(d,e))return false;
 if(points.size()<2||points.size()>32||!range(width,4,32)||!range(banks,2,12)||!range(depth,.5,4)||!source.base){
  e="Draw 2–32 river points; width 4–32m, banks 2–12m and depth .5–4m";return false;
 }
 if(d.surfaces.ribbons.size()>=world_surface::ribbon_limit){e="16 road/river paths reached; remove a path before adding a river";return false;}
 std::vector<double> distances(points.size());double length=0;
 for(unsigned i=0;i<points.size();i++){
  const auto &p=points[i];if(!position(p.x,p.z)||!range(p.height,-2000,12000)){e="River point is outside the supported world/height bounds";return false;}
  if(i){const double segment=std::hypot(p.x-points[i-1].x,p.z-points[i-1].z);if(!range(segment,.25,256)){e="River points must be .25–256m apart";return false;}length+=segment;distances[i]=length;}
 }
 if(length>1024){e="Split rivers longer than 1024m into separate paths";return false;}
 world_surface::Ribbon ribbon;ribbon.name="River "+std::to_string(d.surfaces.ribbons.size()+1);ribbon.kind=world_surface::RibbonKind::River;
 ribbon.width=width;ribbon.shoulder=banks;ribbon.depth=depth;ribbon.layer=-1;ribbon.smooth=false;
 const double first=points.front().height,last=std::min(points.back().height,first-length*.001);
 if(!range(last,-2000,12000)){e="River downhill profile leaves the supported height bounds";return false;}
 for(unsigned i=0;i<points.size();i++)ribbon.points.push_back({points[i].x,first+(last-first)*distances[i]/length,points[i].z});
 world_surface::BakedRibbon baked;if(!world_surface::bake_ribbon(ribbon,baked,e))return false;
 if(!position(baked.bounds[0],baked.bounds[1])||!position(baked.bounds[2],baked.bounds[3])){e="River and its banks must stay inside the world bounds";return false;}
 auto next=d;next.surfaces.ribbons.push_back(std::move(ribbon));
 RiverGround ground{&d,source};world_surface::TerrainField before,after;
 if(!world_surface::bake_terrain(d.surfaces,river_ground,&ground,before,e)||!world_surface::bake_terrain(next.surfaces,river_ground,&ground,after,e))return false;
 if(!combined_height_valid(next,after,e))return false;
 Report result;result.added_ribbons=1;std::set<std::pair<int,int>> visited;
 for(const auto &tile:after.tiles)for(unsigned z=0;z<65;z++)for(unsigned x=0;x<65;x++){
  const int gx=tile.x*64+int(x),gz=tile.z*64+int(z);if(!visited.insert({gx,gz}).second)continue;
  const double px=gx*spacing,pz=gz*spacing,old_offset=world_surface::terrain_offset(before,px,pz),new_offset=world_surface::terrain_offset(after,px,pz);
  const double change=std::abs(new_offset-old_offset);if(change<=1e-5)continue;
  if(source.protected_point&&source.protected_point(px,pz,source.context)){e="River banks overlap a protected building, water or navigation area; move the route";return false;}
  double h=0;if(!height(d,source,px,pz,h,e))return false;
  result.changed_nodes++;result.largest_change=std::max(result.largest_change,change);result.height_sum_before+=h+old_offset;result.height_sum_after+=h+new_offset;
 }
 if(!world_studio::valid(next,e))return false;
 next.surface_field=std::make_shared<world_surface::TerrainField>(std::move(after));d=std::move(next);out=result;return true;
}
bool combined_height_valid(const world_studio::Document &d,const world_surface::TerrainField &field,std::string &e,const std::vector<std::pair<int,int>> *changed_tiles){
 e.clear();
 for(const auto &tile:field.tiles){
  if(changed_tiles&&!std::any_of(changed_tiles->begin(),changed_tiles->end(),[&](const auto &changed){return std::abs(changed.first-tile.x)<=1&&std::abs(changed.second-tile.z)<=1;}))continue;
  std::array<float,65*65> sculpt={};
  for(unsigned z=0;z<65;z++)for(unsigned x=0;x<65;x++){
   const auto i=z*65+x;sculpt[i]=world_studio::height_delta(d,tile.x*128.+x*spacing,tile.z*128.+z*spacing);
   const float total=sculpt[i]+tile.delta[i];
   if(!std::isfinite(total)||std::abs(total)>32){e="Combined sculpt and road/river displacement exceeds 32m; lower the terrain change or adjust the path height";return false;}
  }
  // A triangular sculpt plus bilinear ribbon field is a saddle within each
  // triangle, so extrema occur on its edges. Outer cell edges are linear;
  // the shared u+v=1 diagonal can contain one quadratic extremum.
  for(unsigned z=0;z<64;z++)for(unsigned x=0;x<64;x++){
   const auto i=z*65+x;const double f00=tile.delta[i],f10=tile.delta[i+1],f01=tile.delta[i+65],f11=tile.delta[i+66];
   const double a=f10+f01-f00-f11,b=double(sculpt[i+1])-sculpt[i+65]+f00+f11-2*f01,c=double(sculpt[i+65])+f01;
   if(std::abs(a)<1e-12)continue;
   const double u=-b/(2*a);if(u<=0||u>=1)continue;
   const double total=(a*u+b)*u+c;
   if(!std::isfinite(total)||std::abs(total)>32){e="Combined sculpt and road/river displacement exceeds 32m between terrain samples; reduce the terrain or path change";return false;}
  }
 }
 return true;
}
bool combined_height_valid(const world_studio::Document &d,const world_surface::TerrainField &field,std::string &e){return combined_height_valid(d,field,e,nullptr);}
}

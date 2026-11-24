#include "engine/world_platform.h"
#include "engine/world_terrain_authoring.h"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
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
   if(d.terrain.size()>=world_studio::tile_limit){e="Operation exceeds the sixteen128m editable terrain tiles";return false;}
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
}

#include "engine/world_studio.h"
#include "engine/world_atlas.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
namespace {
double smooth(double a,double b,double x){x=std::clamp((x-a)/(b-a),0.,1.);return x*x*(3-2*x);}
struct Drift{double x,z,height,width,length;};
// Broad, asymmetric deposits aligned to one prevailing mountain wind. These
// are real 2m terrain samples, shared by rendering, walking and boot prints.
constexpr Drift drifts[]={
 {12,35,1.15,5,13},{-13,56,1.45,5,19},{14,83,1.25,6,18},
 {-15,112,1.55,6,18},{20,132,1.65,7,21},{-27,32,1.8,8,17},
 {32,59,2.35,10,24},{-35,90,2.7,11,28},{46,111,2.8,12,24},
 {-52,145,3.2,13,29},{70,46,3.6,14,26},{-76,65,3.8,16,34},
 {85,155,4.1,17,35},{-105,122,4.4,18,32},{116,78,4.8,19,38},
 {-123,195,5.1,22,41},{125,222,5.5,23,46},{-53,234,3.5,18,40},
 {40,205,2.7,14,30},{-24,184,2.2,12,28},{-93,27,2.7,16,24}};
double relief(double x,double z){
    double h=.12+.09*std::sin(x*.13+z*.045)+.07*std::sin(z*.17-x*.04);
    for(const auto &d:drifts){
        const double dx=x-d.x,dz=z-d.z;
        double u=(dx*.94+dz*.342)/d.width;
        // Long windward ramp, shorter lee. No repeated sine-wave dunes.
        u/=u<0?1.5:.85;
        const double v=(-dx*.342+dz*.94)/d.length,q=u*u+v*v;
        if(q<1)h+=d.height*std::pow(1-q,3);
    }
    const double corridor=1-smooth(2.3,6.4,std::abs(x));
    const double packed=.035+.012*std::sin(z*.11);
    h=h*(1-corridor)+packed*corridor;
    // Furniture and resident stations retain stable, gently feathered pads.
    for(double sx:{-10.5,10.5})for(double sz:{22.,48.,78.})
        h*=smooth(1.7,4.,std::hypot(x-sx,z-sz));
    for(double sx:{-27.,27.})for(double sz:{22.,50.,82.})
        h*=smooth(1.1,3.,std::hypot(x-sx,z-sz));
    h*=smooth(1.7,4.,std::hypot(x-23,z-36));
    // Flat frozen plunge pool; its snow shore blends into the same collision
    // height field. No visual ice plane is placed above a different floor.
    h*=smooth(.72,1.4,std::hypot((x-63)/13,(z-32)/10));
    return h*smooth(13,21,z);
}
}
int main(int argc,char **argv){
    if(argc!=3||!world_atlas_init())return 2;
    world_studio::Document doc;std::string error;
    if(!world_studio::load(argv[1],doc,error)){std::fprintf(stderr,"%s\n",error.c_str());return 1;}
    constexpr double x=-43344.,z=(.077-.5)*100800.*2818./5009.;
    const float level=world_atlas_base_height(x,z);
    // Keep the full hall/mountain base level, then feather an irregular snow
    // basin over 15 tiles (the Studio budget is 16). Shared outer edges are 0.
    doc.terrain.clear();
    for(int tz=-190;tz<=-186;++tz)
      for(int tx=-340;tx<=-338;++tx){
        world_studio::TerrainTile tile;tile.x=tx;tile.z=tz;
        for(unsigned j=0;j<65;j++)for(unsigned i=0;i<65;i++){
            double px=tx*128.+i*2.,pz=tz*128.+j*2.;
            const double lx=px-x,lz=pz-z;
            double w=(1-smooth(133,173,std::abs(lx)))*
                smooth(-300,-240,lz)*(1-smooth(250,298,lz));
            tile.delta[j*65+i]=float((level+relief(lx,lz)-world_atlas_base_height(px,pz))*w);
        }
        doc.terrain.push_back(tile);
      }
    if(!world_studio::save(argv[2],doc,error)){std::fprintf(stderr,"%s\n",error.c_str());return 1;}
    std::printf("Grandstone saved: %zu placements, %zu terrain tiles, level %.5f\n",doc.placements.size(),doc.terrain.size(),level);
}

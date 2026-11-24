#include "engine/world_studio.h"
#include "engine/world_cover.h"
#include "engine/world_atlas.h"
#include "engine/world_editor_session.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

static unsigned checks=0;
static void check(bool ok,const char *what){++checks;if(!ok){std::fprintf(stderr,"FAIL %u: %s\n",checks,what);std::abort();}}
struct Snapshot {double x=0,z=0;std::vector<WorldModelInstance> roots;};
using Snapshots=std::array<Snapshot,4>;
static Snapshots build(WorldCover *cover,double x,double z,size_t budget=4096){
    check(world_cover_update(cover,x,z),"start cover generation");
    Snapshots out;unsigned fields=0;
    for(unsigned calls=0;calls<200000;++calls){
        auto done=world_cover_pump(cover,budget);
        check(done<=std::min(budget,size_t(4096)),"bounded pump");
        WorldCoverField field={};
        while(world_cover_poll(cover,&field)){
            auto &s=out[field.kind];s.x=field.anchor_x;s.z=field.anchor_z;
            s.roots.assign(field.instances,field.instances+field.count);++fields;
        }
        WorldCoverMetrics m={};world_cover_metrics(cover,&m);
        if(!m.near_building&&!m.tree_building){check(fields==4,"publish all four fields");return out;}
    }
    check(false,"generation terminates");return out;
}
static bool equal(const Snapshots &a,const Snapshots &b){
    for(unsigned k=0;k<4;++k)if(a[k].x!=b[k].x||a[k].z!=b[k].z||a[k].roots.size()!=b[k].roots.size()||
        (!a[k].roots.empty()&&std::memcmp(a[k].roots.data(),b[k].roots.data(),a[k].roots.size()*sizeof(WorldModelInstance))))return false;
    return true;
}
static void terrain(const char *path,bool steep){
    std::ofstream f(path,std::ios::binary);const uint32_t dims[]={2,2};
    const double bounds[]={24000,9000,4000,2000};
    const float heights[]={steep?-1900.f:100.f,steep?6100.f:100.f,steep?-1900.f:100.f,steep?6100.f:100.f};
    f.write("VWHGT1\0\0",8);f.write(reinterpret_cast<const char*>(dims),sizeof dims);
    f.write(reinterpret_cast<const char*>(bounds),sizeof bounds);f.write(reinterpret_cast<const char*>(heights),sizeof heights);f.close();
    char error[256]={};check(world_atlas_load_height_region(path,error,sizeof error)==1,"install actual authored test terrain");
}
static bool protected_half(int,double x,double,const void *data){return x<*static_cast<const double*>(data);}
static bool flooded(double,double,float *level,float *depth,const void *){if(level)*level=101;if(depth)*depth=1;return true;}
static size_t total(const Snapshots &s){size_t n=0;for(const auto &v:s)n+=v.roots.size();return n;}
static std::string read(const char *path){std::ifstream f(path,std::ios::binary);std::ostringstream o;o<<f.rdbuf();return o.str();}

int main(){
    using namespace world_studio;
    std::string error,legacy;
    Document d;check(encode(d,legacy,error)&&legacy.rfind("VALWOOD_STUDIO 6\n",0)==0,"empty ecology preserves v6");
    d.particles.push_back({});d.particles.back().kind=3;
    std::string vfx;check(encode(d,vfx,error)&&vfx.rfind("VALWOOD_STUDIO 7\n",0)==0,"extended VFX preserves v7");
    WorldCoverRegion r;r.x=25000;r.z=10000;r.radius=64;r.seed=4294967295u;r.max_slope=.1f;
    for(float &density:r.density)density=1;
    d.ecology.push_back(r);d.particles.back().kind=0;
    std::string encoded;check(encode(d,encoded,error)&&encoded.rfind("VALWOOD_STUDIO 8\n",0)==0,"ecology opts into v8");
    char path[]="/tmp/valwood-ecology-test-XXXXXX";int fd=mkstemp(path);check(fd>=0,"temporary document");close(fd);
    check(save(path,d,error),"save ecology");Document loaded;
    check(load(path,loaded,error)&&loaded.ecology.size()==1&&world_ecology::same(loaded.ecology[0],r),"ecology exact roundtrip");
    check(loaded.particles[0].tint==std::array<float,3>({1,1,1})&&loaded.particles[0].glow==0,"v8 retains default particle extension");
    std::string again;check(encode(loaded,again,error)&&again==encoded,"stable reencoding");
    History history;history.checkpoint(loaded);loaded.ecology[0].density[1]=0;
    check(history.undo(loaded)&&loaded.ecology[0].density[1]==1,"ecology undo");
    check(history.redo(loaded)&&loaded.ecology[0].density[1]==0,"ecology redo");
    for(unsigned bad=0;bad<6;++bad){
        auto invalid=d;
        if(bad==0)invalid.ecology[0].radius=257;
        if(bad==1)invalid.ecology[0].density[0]=std::numeric_limits<float>::quiet_NaN();
        if(bad==2)invalid.ecology[0].density[3]=-1;
        if(bad==3)invalid.ecology[0].preset=3;
        if(bad==4)invalid.ecology[0].max_slope=86;
        if(bad==5)invalid.ecology[0].x=50399;
        check(!save(path,invalid,error)&&read(path)==encoded,"invalid ecology cannot clobber saved world");
    }
    auto malformed=[&](std::string bytes){std::ofstream f(path,std::ios::binary);f<<bytes;f.close();auto before=loaded.ecology;bool ok=load(path,loaded,error);check(!ok&&loaded.ecology.size()==before.size()&&world_ecology::same(loaded.ecology[0],before[0]),"malformed load preserves previous document");};
    for(const char *seed:{"-1","4294967296","999999999999999999999"}){auto text=encoded;auto at=text.find("4294967295");check(at!=std::string::npos,"seed fixture");text.replace(at,10,seed);malformed(text);}
    {auto text=encoded;text.replace(15,1,"7");malformed(text);}
    {auto invalid=d;invalid.ecology.resize(33,r);check(!valid(invalid,error),"32 region capacity enforced");}
    {Document large;large.terrain.resize(64);for(unsigned i=0;i<64;++i){auto &t=large.terrain[i];t.x=int(i%8)*2;t.z=int(i/8)*2;
        for(unsigned z=1;z<64;++z)for(unsigned x=1;x<64;++x){unsigned h=(i*73856093u)^((z*65+x)*19349663u);t.delta[z*65+x]=float(double(h)*(31./4294967296.));}}
        std::string bytes;check(encode(large,bytes,error)&&bytes.size()>4*1024*1024,"64 detailed tiles exceed former 4MiB bound");
        check(save(path,large,error)&&load(path,loaded,error)&&loaded.terrain.size()==64,"64 detailed tiles persist under bounded 16MiB schema");
        {world_editor::Session session;check(session.open(path,large,0,error),"session opens actual >4MiB base");
         auto draft=large;draft.ecology.push_back(r);check(session.changed(.1)&&session.flush_recovery(draft,.2,error),"large v8 recovery publishes bounded immutable snapshot");
         check(read(path)==bytes,"recovery leaves saved large base unchanged");
         world_editor::Session restarted;check(restarted.open(path,large,0,error)&&restarted.probe(error)==world_editor::Recovery::Available,"large recovery restart inspects exact base");
         Document recovered;check(restarted.recover(recovered,1,error)&&recovered.terrain.size()==64&&recovered.ecology.size()==1&&world_ecology::same(recovered.ecology[0],r),"large draft and ecology survive recovery");
         check(restarted.discard(error),"large recovery safely discarded after test");}
        large.terrain.push_back({});check(!valid(large,error),"65th terrain tile rejected");}
    check(save(path,d,error)&&load(path,loaded,error),"restore ecology fixture");
    malformed(encoded+std::string(400001,' ')+"\n");
    {std::ofstream f(path,std::ios::binary);f.seekp(16*1024*1024);f.put('x');f.close();check(!load(path,loaded,error),"16MiB document bound enforced");}
    unlink(path);

    check(world_atlas_init(),"atlas init");terrain(path,false);
    WorldCover *a=world_cover_init(),*b=world_cover_init();check(a&&b,"cover allocation");
    check(world_cover_set_regions(a,&r,1,true,nullptr,nullptr)&&world_cover_set_regions(b,&r,1,true,nullptr,nullptr),"copy immutable authored regions");
    auto original=r;r.radius=2;for(float &density:r.density)density=0;
    auto roots=build(a,25000,10000);auto repeat=build(b,25000,10000,37);
    check(equal(roots,repeat),"copied seed, positions and pump chunking deterministic");
    const size_t caps[]={WORLD_COVER_GRASS_CAP,WORLD_COVER_TREE_CAP,WORLD_COVER_ROCK_CAP,WORLD_COVER_FERN_CAP};
    for(unsigned k=0;k<4;++k){check(!roots[k].roots.empty(),"each painted layer adds real roots");check(roots[k].roots.size()<=caps[k],"painted layer respects existing cap");
        for(const auto &root:roots[k].roots){double x=roots[k].x+root.x,z=roots[k].z+root.z;check(std::hypot(x-original.x,z-original.z)<original.radius+.001&&std::abs(root.y-100)<.001,"roots stay in circle on committed terrain");}}
    check(roots[1].roots.size()>100,"8m authored tree grid exceeds coarse legacy density");
    auto changed=original;changed.seed--;
    check(world_cover_set_regions(b,&changed,1,true,nullptr,nullptr),"change layout seed");auto reseeded=build(b,25000,10000);check(!equal(roots,reseeded),"new seed changes actual rooted layout");
    check(world_cover_set_regions(b,&original,1,true,nullptr,nullptr),"restore original seed");
    auto shifted=build(b,25128,10000);
    check(shifted[1].roots.size()==roots[1].roots.size(),"tree circle count stable across 128m anchor change");
    for(const auto &root:roots[1].roots){bool found=false;for(const auto &next:shifted[1].roots){
        if(root.yaw==next.yaw&&root.scale==next.scale&&std::abs(roots[1].x+root.x-shifted[1].x-next.x)<.001&&std::abs(roots[1].z+root.z-shifted[1].z-next.z)<.001){found=true;break;}}
        check(found,"authored trees remain rooted across LOD anchor shifts");}
    std::array<WorldCoverRegion,3> forests={original,original,original};
    for(unsigned i=0;i<forests.size();++i){forests[i].x+=i*600;forests[i].radius=256;for(float &v:forests[i].density)v=0;forests[i].density[1]=1;}
    check(world_cover_set_regions(b,forests.data(),forests.size(),true,nullptr,nullptr),"three dense forest regions");
    auto saturated=build(b,25600,10000);check(saturated[1].roots.size()==WORLD_COVER_TREE_CAP,"authored forest overflow retains existing 7000 root cap");
    check(!world_cover_set_regions(a,nullptr,1,true,nullptr,nullptr),"invalid null region input rejected");
    auto bad=original;bad.radius=999;check(!world_cover_set_regions(a,&bad,1,true,nullptr,nullptr),"invalid configuration atomic");
    world_cover_invalidate(a);repeat=build(a,25000,10000);check(equal(roots,repeat),"rejected configuration preserves old copy");
    std::array<WorldCoverRegion,2> overlap={original,original};overlap[1].radius=20;for(float &density:overlap[1].density)density=0;
    check(world_cover_set_regions(b,overlap.data(),2,true,nullptr,nullptr),"last circle clears all layers");auto cleared=build(b,25000,10000);
    for(const auto &field:cleared)for(const auto &root:field.roots)check(std::hypot(field.x+root.x-25000,field.z+root.z-10000)>=19.999,"zero density overlap leaves real clearing");
    double threshold=25000;check(world_cover_set_regions(b,&original,1,true,protected_half,&threshold),"protected point callback");auto protected_roots=build(b,25000,10000);
    check(total(protected_roots)>0,"unprotected half still populated");
    for(const auto &field:protected_roots)for(const auto &root:field.roots)check(field.x+root.x>=25000-.001,"protected half empty");
    world_atlas_set_water_edit(flooded,nullptr);world_cover_invalidate(a);auto wet=build(a,25000,10000);check(total(wet)==0,"paint cannot grow underwater");world_atlas_set_water_edit(nullptr,nullptr);
    terrain(path,true);world_cover_invalidate(a);auto slopes=build(a,25000,10000);check(total(slopes)==0,"steep terrain excluded by authored slope");terrain(path,false);
    world_cover_set_filter(a,[](int,double,double,const void*)->float{return 0;},nullptr);auto masked=build(a,25000,10000);check(total(masked)==0,"legacy density masks still suppress painted roots");
    world_cover_set_filter(a,nullptr,nullptr);check(world_cover_set_regions(a,nullptr,0,true,nullptr,nullptr),"empty explicit-only mode");auto empty=build(a,25000,10000);check(total(empty)==0,"empty explicit-only scene does not spawn global forest");
    std::array<WorldCoverRegion,32> covered;covered.fill(original);
    check(world_cover_set_regions(b,covered.data(),covered.size(),true,nullptr,nullptr),"32 fully overlapping regions");
    auto superseded=build(b,25000,10000);check(equal(roots,superseded),"fully superseded regions preserve exact last-seed roots");
    check(!world_cover_set_regions(b,covered.data(),33,true,nullptr,nullptr),"cover API rejects 33 regions before reading array");
    double forest_x=0,forest_z=0;world_atlas_uv_to_world(.52,.24,&forest_x,&forest_z);
    auto clear=original;clear.x=forest_x;clear.z=forest_z;clear.radius=24;for(float &density:clear.density)density=0;
    check(world_cover_set_regions(b,&clear,1,false,nullptr,nullptr),"clear patch within legacy biome mode");
    auto legacy_clear=build(b,forest_x,forest_z);check(total(legacy_clear)>1000,"legacy growth remains outside explicit clearing");
    for(const auto &field:legacy_clear)for(const auto &root:field.roots)
        check(std::hypot(field.x+root.x-forest_x,field.z+root.z-forest_z)>=23.999,"painted zero clears baseline roots too");
    check(world_cover_region_preset(overlap.data(),2,25000,10000)==int(overlap[1].preset)&&world_cover_region_preset(overlap.data(),2,0,0)==-1,"species preset query shares overlap semantics");
    world_cover_shutdown(a);world_cover_shutdown(b);world_atlas_set_height_edit(nullptr,nullptr);world_atlas_shutdown();unlink(path);
    std::printf("Ecology: %u assertions passed; generated grass=%zu trees=%zu rocks=%zu ferns=%zu.\n",checks,roots[0].roots.size(),roots[1].roots.size(),roots[2].roots.size(),roots[3].roots.size());
}

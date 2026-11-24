#include "engine/world_program_epoch.h"
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "engine/world_stronghold.h"
#include "engine/world_stronghold_data.h"
#include "engine/world_stronghold_visibility.h"
#include "engine/world_models.h"
#include "engine/world_atlas.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <new>
#include <vector>
namespace {
constexpr unsigned families=5,frames=12,total=families*frames,capacity=32;
static_assert(std::size(stronghold_stations)<=capacity);
struct Batch {WorldModelPoseBlend *gpu=nullptr;std::array<WorldModelInstance,capacity> next={},old={};std::array<float,capacity> weights={},old_weights={};unsigned count=0,old_count=0;};
struct Actor {double x=0,z=0;float y=0,yaw=0,scale=1,blend=0;unsigned pose=0;bool walking=false;};
struct Station {unsigned family=0;double x=0,y=0,z=0,yaw=0,scale=1,phase=0,end_x=0,end_y=0,end_z=0;bool patrol=false,absolute_y=false;};
struct Uniforms {unsigned program=0;unsigned long long epoch=0;int offset=-1,anchor=-1,grass=-1,vegetation=-1,lod=-1,wind=-1,height=-1,reactive=-1,actor_lighting=-1,prop=-1;};
bool load_stations(std::vector<Station> &stations,std::string &error){
    FILE *file=std::fopen("showcase.residents","r");
    if(!file){if(errno==ENOENT)return true;error="Cannot read showcase.residents";return false;}
    std::vector<Station> candidate;char line[512];unsigned row=0,records=0;bool ok=true;
    while(std::fgets(line,sizeof line,file)){
        ++row;const char *text=line;while(*text==' '||*text=='\t'||*text=='\r'||*text=='\n')++text;
        if(!std::strchr(line,'\n')&&!std::feof(file)){ok=false;break;}
        if(!*text||*text=='#')continue;
        Station s;unsigned patrol=0;int consumed=0;
        if(++records>capacity||std::sscanf(text,"%u %lf %lf %lf %lf %lf %lf %lf %lf %lf %u %n",
            &s.family,&s.x,&s.y,&s.z,&s.yaw,&s.scale,&s.phase,&s.end_x,&s.end_y,&s.end_z,&patrol,&consumed)!=11){ok=false;break;}
        const char *tail=text+consumed;while(*tail==' '||*tail=='\t'||*tail=='\r'||*tail=='\n')++tail;
        const double values[]={s.x,s.y,s.z,s.yaw,s.scale,s.phase,s.end_x,s.end_y,s.end_z};
        bool finite=true;for(double value:values)finite=finite&&std::isfinite(value);
        if((*tail&&*tail!='#')||!finite||s.family>=families||patrol>1||s.scale<.5||s.scale>2||
           std::abs(s.x)>1e8||std::abs(s.z)>1e8||std::abs(s.end_x)>1e8||std::abs(s.end_z)>1e8||
           std::abs(s.y)>12000||std::abs(s.end_y)>12000||std::abs(s.yaw)>100||std::abs(s.phase)>1e6||
           (patrol&&std::hypot(s.end_x-s.x,s.end_z-s.z)<.001)){ok=false;break;}
        s.patrol=patrol!=0;s.absolute_y=true;
        if(s.family!=1)candidate.push_back(s); // Broddi source armor remains disabled.
    }
    if(std::ferror(file))ok=false;
    std::fclose(file);
    if(!ok||candidate.empty()){
        error="Rejected showcase.residents at line "+std::to_string(row)+": expected at most32 finite resident rows";return false;
    }
    stations=std::move(candidate);return true;
}
}
struct WorldStronghold {
    struct VisibilityBounds {float center_y=0,radius=0;bool valid=false;};
    std::array<WorldModelAsset *,total> assets={};std::array<Batch,total*3> batches={};std::array<Actor,capacity> actors={};
    std::array<VisibilityBounds,families> visibility={};
    bool frustum_enabled=std::getenv("VALWOOD_QA_DISABLE_DWARF_FRUSTUM")==nullptr;
    std::vector<Station> stations;
    std::array<Uniforms,4> uniforms={};unsigned next_uniform=0;WorldStrongholdStats stats;
    double ax=std::floor(stronghold_x/128)*128,az=std::floor(stronghold_z/128)*128;
    Uniforms &locations(unsigned program){
        for(auto &u:uniforms)if(u.program==program&&u.epoch==world_program_epoch)return u;
        auto &u=uniforms[next_uniform++%uniforms.size()];u.program=program;u.epoch=world_program_epoch;
#define U(a,b) u.a=glGetUniformLocation(program,b)
        U(offset,"uOffset");U(anchor,"uWorldAnchor");U(grass,"uGrass");U(vegetation,"uVegetation");U(lod,"uLodLevel");U(wind,"uWind");U(height,"uTreeHeight");U(reactive,"uActorReactive");U(prop,"uPropLodRange");
        U(actor_lighting,"uActorLighting");
#undef U
        return u;
    }
};
WorldStronghold *world_stronghold_load(const char *directory,std::string &error){
    error.clear();if(!directory||!*directory){error="Missing dwarf pose directory";return nullptr;}
    auto *w=new(std::nothrow)WorldStronghold;if(!w){error="Dwarf allocation failed";return nullptr;}
    for(const auto &s:stronghold_stations){Station t;t.family=s.family;t.x=s.x;t.z=s.z;t.yaw=s.yaw;t.scale=s.scale;t.phase=s.phase;t.end_x=s.end_x;t.end_z=s.end_z;t.patrol=s.patrol;w->stations.push_back(t);}
    if(!load_stations(w->stations,error)){world_stronghold_free(w);return nullptr;}
    const char *names[]={"king","broddi","agvid","bedvar","orme"};
    for(unsigned f=0;f<families;f++)for(unsigned mode=0;mode<2;mode++){
        if(f==1)continue; // Supplied Broddi armor needs source transform repair.
        unsigned count=mode?8:4,base=f*frames+(mode?4:0);
        for(unsigned i=0;i<count;i++){
            char path[1200];int n=std::snprintf(path,sizeof path,"%s/dwarf_%s_%s_%02u.glb",directory,names[f],mode?"walk":"idle",i);
            if(n<0||std::size_t(n)>=sizeof path||!(w->assets[base+i]=world_model_load_pose(path,WORLD_MODEL_POSE_BLEND_VERSION))){error=std::string("Dwarf pose failed: ")+path;world_stronghold_free(w);return nullptr;}
        }
        for(unsigned i=0;i<count;i++)for(unsigned pass=0;pass<3;pass++){
            auto &b=w->batches[pass*total+base+i];b.gpu=world_model_pose_blend_create(capacity,WORLD_MODEL_POSE_BLEND_VERSION);
            if(!b.gpu||!world_model_pose_blend_set_targets(b.gpu,w->assets[base+i],w->assets[base+(i+1)%count])){error="Incompatible dwarf animation poses";world_stronghold_free(w);return nullptr;}
        }
    }
    // One bound encloses every idle/walk target in each family. Convex pose
    // interpolation cannot escape this union. Radial XZ extent covers any yaw.
    for(unsigned f=0;f<families;f++){
        if(f==1)continue;
        float lo[3]={INFINITY,INFINITY,INFINITY},hi[3]={-INFINITY,-INFINITY,-INFINITY};bool valid=true;
        for(unsigned i=0;i<frames;i++){
            float a[3],b[3];
            if(!world_model_bounds(w->assets[f*frames+i],a,b)){valid=false;break;}
            for(unsigned j=0;j<3;j++){
                valid=valid&&std::isfinite(a[j])&&std::isfinite(b[j])&&a[j]<=b[j];
                lo[j]=std::min(lo[j],a[j]);hi[j]=std::max(hi[j],b[j]);
            }
        }
        if(valid){
            const float rx=std::max(std::abs(lo[0]),std::abs(hi[0])),rz=std::max(std::abs(lo[2]),std::abs(hi[2])),ry=(hi[1]-lo[1])*.5f;
            w->visibility[f]={(lo[1]+hi[1])*.5f,std::sqrt(rx*rx+ry*ry+rz*rz)+.05f,true};
        }
    }
    w->stats.poses=total-frames;world_stronghold_update(w,0);return w;
}
void world_stronghold_update(WorldStronghold *w,double seconds){
    if(!w||!std::isfinite(seconds)||seconds<0||seconds>1e8)return;
    w->stats.updates++;w->stats.actors=w->stats.walking=0;
    for(const auto &s:w->stations){
        Actor a;a.x=s.x;a.z=s.z;a.yaw=s.yaw;a.scale=s.scale;double phase=seconds/2.0+s.phase;
        double fixed_y=s.y;
        if(s.patrol){
            const double dx=s.end_x-s.x,dz=s.end_z-s.z,length=std::hypot(dx,dz),travel=length/.75;
            const double t=std::fmod(seconds+s.phase*(travel*2+4),travel*2+4);
            double u=0;bool back=false;
            if(t<2)u=0;else if(t<travel+2){u=(t-2)/travel;a.walking=true;}
            else if(t<travel+4)u=1;else{u=1-(t-travel-4)/travel;a.walking=true;back=true;}
            a.x+=dx*u;a.z+=dz*u;a.yaw=float(std::atan2(back?-dx:dx,back?-dz:dz));
            fixed_y+=(s.end_y-s.y)*u;
            phase=(seconds*.75/.9)+s.phase;
        }
        phase-=std::floor(phase);const unsigned count=a.walking?8:4;const double f=phase*count;const unsigned i=unsigned(f);
        a.pose=s.family*frames+(a.walking?4:0)+i;a.blend=float(f-i);a.y=s.absolute_y?float(fixed_y):world_atlas_height(a.x,a.z)+.015f;
        if(!std::isfinite(a.y)){w->stats.failures++;continue;}
        w->actors[w->stats.actors++]=a;if(a.walking)w->stats.walking++;
    }
}
void world_stronghold_draw(WorldStronghold *w,unsigned program,double x,double y,double z,unsigned pass,const float *view_projection){
    if(!w||!program||pass>=3)return;
    const unsigned start=pass*total;for(unsigned i=start;i<start+total;i++)w->batches[i].count=0;
    unsigned visible=0;
    for(unsigned i=0;i<w->stats.actors;i++){
        const auto &a=w->actors[i];if(std::hypot(a.x-x,a.z-z)>230)continue;
        const auto &bounds=w->visibility[a.pose/frames];
        if(w->frustum_enabled&&bounds.valid&&view_projection){
            if(pass==0)++w->stats.frustum_tests;
            if(world_stronghold_sphere_outside_clip(view_projection,a.x-x,
                    double(a.y)-y+bounds.center_y*a.scale,a.z-z,bounds.radius*a.scale)){
                if(pass==0)++w->stats.frustum_culled;
                continue;
            }
        }
        auto &b=w->batches[start+a.pose];b.next[b.count]={float(a.x-w->ax),a.y,float(a.z-w->az),a.yaw,a.scale};b.weights[b.count++]=a.blend;visible++;
    }
    auto &u=w->locations(program);glUseProgram(program);
    glUniform1i(u.actor_lighting,1);
    glUniform3f(u.offset,float(w->ax-x),float(-y),float(w->az-z));glUniform2f(u.anchor,float(w->ax),float(w->az));glUniform1i(u.grass,0);glUniform1i(u.vegetation,0);glUniform1i(u.lod,-1);glUniform1f(u.wind,0);glUniform1f(u.height,1);glUniform1f(u.reactive,pass==2?0.f:.85f);glUniform3f(u.prop,215,15,1);
    for(unsigned i=start;i<start+total;i++){
        auto &b=w->batches[i];if(!b.gpu)continue;
        if(b.count!=b.old_count||std::memcmp(b.next.data(),b.old.data(),b.count*sizeof(WorldModelInstance))||std::memcmp(b.weights.data(),b.old_weights.data(),b.count*sizeof(float))){
            if(!world_model_pose_blend_upload(b.gpu,b.next.data(),b.weights.data(),b.count)){w->stats.failures++;continue;}
            std::copy_n(b.next.begin(),b.count,b.old.begin());std::copy_n(b.weights.begin(),b.count,b.old_weights.begin());b.old_count=b.count;w->stats.uploads++;
        }
        if(b.old_count)world_model_pose_blend_draw(b.gpu,program);
    }
    glUniform1f(u.reactive,0);glUniform1i(u.actor_lighting,0);glUniform3f(u.prop,0,0,0);
    if(pass==0){w->stats.visible=visible;w->stats.peak_visible=std::max(w->stats.peak_visible,visible);}
}
bool world_stronghold_can_move(const WorldStronghold *w,double from_x,double from_z,double x,double z,double feet,float radius){
    if(!w)return true;
    for(unsigned i=0;i<w->stats.actors;i++){
        const auto &a=w->actors[i];if(feet>a.y+1.5*a.scale||feet+1.75<a.y)continue;
        const double d=std::hypot(x-a.x,z-a.z),old=std::hypot(from_x-a.x,from_z-a.z);
        if(d<radius+.4*a.scale&&d<old-.00001)return false;
    }return true;
}
WorldStrongholdStats world_stronghold_stats(const WorldStronghold *w){return w?w->stats:WorldStrongholdStats{};}
void world_stronghold_free(WorldStronghold *w){if(!w)return;for(auto &b:w->batches)if(b.gpu)world_model_pose_blend_free(b.gpu);for(auto *a:w->assets)if(a)world_model_free(a);delete w;}

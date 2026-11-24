#include <cstdlib>
#include "engine/world_program_epoch.h"
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "engine/world_road_traffic.h"
#include "engine/world_road_traffic_core.h"
#include "engine/world_models.h"
#include "engine/world_model_pose_transform.h"
#include "cgltf.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <new>
#include <sstream>
#include <string>
#include <vector>
namespace {
#include "engine/world_village_contact_data.inc"
using world_road_traffic_core::Core;
using world_road_traffic_core::Profile;
constexpr unsigned bank_count=6,max_poses=20,max_pairs=23,passes=3,capacity=WORLD_ROAD_TRAFFIC_ACTORS;
void error_text(char *out,std::size_t cap,const std::string &s){if(out&&cap)std::snprintf(out,cap,"%s",s.c_str());}
bool trailing(std::istringstream &row){std::string extra;return !(row>>extra);}
bool relative(const std::string&s){if(s.empty()||s.size()>512||s.find('\\')!=std::string::npos)return false;auto p=std::filesystem::u8path(s);if(p.is_absolute())return false;for(const auto&c:p)if(c==".."||c==".")return false;return p.extension()==".glb";}
struct Manifest {
    std::array<std::string,20> mounted={};std::array<std::string,12> puller={};
    unsigned mw=0,mi=0,pw=0,pi=0,axles=0;double mounted_stride=0,mounted_seconds=0,puller_stride=0,puller_seconds=0,wheel_radius=0;
    std::string body,wheel;std::array<double,3> offset={},wheel_axis={};std::array<std::array<double,3>,4> axle={};
};
bool manifest_load(const char *file,Manifest&m,std::string&error){
    std::ifstream in(file);if(!in){error="Cannot open road life asset manifest";return false;}std::string line;if(!std::getline(in,line)||line!="VALWOOD_ROAD_LIFE_ASSETS 1"){error="Unsupported road life asset manifest";return false;}
    Manifest next;bool ended=false,have_m=false,have_p=false,have_off=false,have_radius=false,have_axis=false;unsigned lines=0;
    while(std::getline(in,line)){if(++lines>100||line.size()>1024){error="Road life asset manifest exceeds its limit";return false;}if(line.empty()||line[0]=='#')continue;std::istringstream row(line);std::string key;row>>key;bool ok=false;
        if(key=="end"){ok=trailing(row);ended=true;}
        else if(key=="mounted"||key=="puller"){std::string a,b;double stride=0,seconds=0;bool&have=key=="mounted"?have_m:have_p;ok=!have&&bool(row>>a>>stride>>b>>seconds)&&a=="stride"&&b=="seconds"&&std::isfinite(stride)&&stride>=.25&&stride<=12&&std::isfinite(seconds)&&seconds>=.2&&seconds<=20&&trailing(row);if(ok){have=true;if(key=="mounted"){next.mounted_stride=stride;next.mounted_seconds=seconds;}else{next.puller_stride=stride;next.puller_seconds=seconds;}}}
        else if(key=="mounted_walk"||key=="mounted_idle"||key=="puller_walk"||key=="puller_idle"){std::string p;ok=bool(row>>std::quoted(p))&&relative(p)&&trailing(row);if(ok){unsigned *n=nullptr,base=0,limit=0;std::string *dest=nullptr;if(key=="mounted_walk"){n=&next.mw;limit=16;dest=next.mounted.data();}if(key=="mounted_idle"){n=&next.mi;limit=4;base=16;dest=next.mounted.data();}if(key=="puller_walk"){n=&next.pw;limit=8;dest=next.puller.data();}if(key=="puller_idle"){n=&next.pi;limit=4;base=8;dest=next.puller.data();}ok=*n<limit;if(ok)dest[base+(*n)++]=p;}}
        else if(key=="cart_body"||key=="cart_wheel"){auto&dest=key=="cart_body"?next.body:next.wheel;ok=dest.empty()&&bool(row>>std::quoted(dest))&&relative(dest)&&trailing(row);}
        else if(key=="cart_offset"||key=="wheel_axis"){auto&v=key=="cart_offset"?next.offset:next.wheel_axis;bool&have=key=="cart_offset"?have_off:have_axis;ok=!have&&bool(row>>v[0]>>v[1]>>v[2])&&trailing(row);for(double x:v)ok=ok&&std::isfinite(x)&&std::abs(x)<10;if(ok)have=true;}
        else if(key=="cart_axle"){ok=next.axles<next.axle.size();if(ok){auto&v=next.axle[next.axles];ok=bool(row>>v[0]>>v[1]>>v[2])&&trailing(row);for(double x:v)ok=ok&&std::isfinite(x)&&std::abs(x)<10;if(ok)++next.axles;}}
        else if(key=="wheel_radius"){ok=!have_radius&&bool(row>>next.wheel_radius)&&std::isfinite(next.wheel_radius)&&next.wheel_radius>=.15&&next.wheel_radius<=2&&trailing(row);if(ok)have_radius=true;}
        if(!ok){error="Malformed road life manifest field: "+key;return false;}if(ended)break;
    }
    while(std::getline(in,line))if(!line.empty()&&line[0]!='#'){error="Trailing road life manifest records";return false;}
    if(!ended||!have_m||!have_p||!have_off||!have_radius||!have_axis||next.mw!=16||next.mi!=4||next.pw!=8||next.pi!=4||next.axles!=2||next.body.empty()||next.wheel.empty()||std::abs(next.wheel_axis[0]-1)>1e-6||std::abs(next.wheel_axis[1])>1e-6||std::abs(next.wheel_axis[2])>1e-6){error="Incomplete road life manifest or unsupported wheel axis";return false;}
    m=std::move(next);return true;
}
// Loading-only source point capture. Use the same node transform convention
// as world_models; baked pose banks contain no live skins. No CPU geometry is
// retained once the conservative swept footprint has been derived.
bool shape_points(const std::filesystem::path&path,std::vector<std::array<double,3>>&out,const std::array<double,3>&offset={}){
    cgltf_options options={};cgltf_data*data=nullptr;if(cgltf_parse_file(&options,path.u8string().c_str(),&data)!=cgltf_result_success)return false;
    bool ok=cgltf_load_buffers(&options,data,path.u8string().c_str())==cgltf_result_success;
    if(ok)for(std::size_t ni=0;ni<data->nodes_count&&ok;++ni){const auto&node=data->nodes[ni];if(!node.mesh)continue;float m[16];cgltf_node_transform_world(&node,m);
        for(std::size_t pi=0;pi<node.mesh->primitives_count&&ok;++pi){const auto&primitive=node.mesh->primitives[pi];const cgltf_accessor*pos=nullptr;for(std::size_t ai=0;ai<primitive.attributes_count;++ai)if(primitive.attributes[ai].type==cgltf_attribute_type_position)pos=primitive.attributes[ai].data;
            if(!pos||pos->count>100000){ok=false;break;}for(std::size_t vi=0;vi<pos->count;++vi){float p[3];if(!cgltf_accessor_read_float(pos,vi,p,3)){ok=false;break;}std::array<double,3>v={m[0]*p[0]+m[4]*p[1]+m[8]*p[2]+m[12]+offset[0],m[1]*p[0]+m[5]*p[1]+m[9]*p[2]+m[13]+offset[1],m[2]*p[0]+m[6]*p[1]+m[10]*p[2]+m[14]+offset[2]};for(double x:v)if(!std::isfinite(x)||std::abs(x)>20)ok=false;if(!ok)break;out.push_back(v);}
        }
    }cgltf_free(data);return ok;
}
void fit_profile(Profile&f,const std::vector<std::array<double,3>>&points){
    double width=0;for(const auto&p:points)width=std::max(width,std::abs(p[0]));f.radius=std::ceil((width+.015)/.05)*.05;
    double axis_front=0,axis_rear=0,height=0;
    for(const auto&p:points){double span=std::sqrt(std::max(0.,f.radius*f.radius-p[0]*p[0]));
        auto include=[&](double angle){double c=std::cos(angle),s=std::sin(angle),y=c*p[1]-s*p[2],z=s*p[1]+c*p[2];axis_front=std::max(axis_front,z-span);axis_rear=std::max(axis_rear,-z-span);height=std::max(height,y);};
        include(-.16);include(.16);include(0);for(double critical:{std::atan2(p[1],p[2]),std::atan2(p[1],p[2])+world_road_traffic_core::pi,std::atan2(-p[2],p[1])}){critical=std::remainder(critical,2*world_road_traffic_core::pi);if(critical>=-.16&&critical<=.16)include(critical);}
    }
    f.front=std::ceil((axis_front+f.radius+.01)*100)/100;f.rear=std::ceil((axis_rear+f.radius+.01)*100)/100;f.height=std::ceil((height+.015)*100)/100;
}
struct Batch {WorldModelPoseBlend *gpu=nullptr;std::array<WorldModelInstance,capacity> instances={};std::array<float,capacity> weights={};unsigned count=0,old_count=0;std::array<WorldModelInstance,capacity> old={};std::array<float,capacity> old_weights={};};
struct Pair {unsigned a=0,b=0;int contact=-1;std::array<Batch,passes> pass={};};
struct Bank {unsigned walk=8,idle=4,count=0,pair_count=0;std::array<WorldModelAsset*,max_poses> poses={};std::array<Pair,max_pairs> pairs={};float radius=2;};
struct Uniforms{unsigned program=0;unsigned long long epoch=0;int offset=-1,anchor=-1,grass=-1,vegetation=-1,lod=-1,wind=-1,height=-1,reactive=-1,prop=-1,graph=-1,surface=-1,tint=-1,normal_uv=-1;};
bool sphere_visible(const float*m,double x,double y,double z,double r){if(!m)return true;for(unsigned axis=0;axis<3;++axis)for(unsigned side=0;side<2;++side){double sign=side?1:-1,a=m[3]+sign*m[axis],b=m[7]+sign*m[4+axis],c=m[11]+sign*m[8+axis],d=m[15]+sign*m[12+axis],n=std::sqrt(a*a+b*b+c*c);if(!std::isfinite(n)||n<1e-9)return true;if(a*x+b*y+c*z+d<-(r+.10)*n)return false;}return true;}
std::array<double,3> local_point(const WorldRoadTrafficActor&a,const std::array<double,3>&p){double c=std::cos(a.yaw),s=std::sin(a.yaw),cp=std::cos(a.pitch),sp=std::sin(a.pitch),y=cp*p[1]-sp*p[2],z=sp*p[1]+cp*p[2];return {a.x+c*p[0]+s*z,a.y+y,a.z-s*p[0]+c*z};}
}
struct WorldRoadTraffic {
    Core core;Manifest manifest;std::array<Bank,bank_count> banks={};WorldModelAsset *cart_body=nullptr,*cart_wheel=nullptr;
    std::array<WorldModelInstanceBatch*,passes> cart_batches={},wheel_batches={};std::array<Uniforms,4> uniforms={};unsigned next_uniform=0;
    std::array<std::array<WorldModelInstance,3>,passes> previous_carts={};
    std::array<std::array<WorldModelInstance,6>,passes> previous_wheels={};
    std::array<unsigned,passes> previous_cart_counts={},previous_wheel_counts={};
    std::array<std::array<std::array<double,3>,2>,capacity> wheel_previous={};std::array<std::array<double,2>,capacity> wheel_distances={};std::array<bool,capacity> wheel_initialized={};
    WorldRoadTrafficStats render_stats;double anchor_x=0,anchor_z=0;bool ready=false;
    Uniforms&locations(unsigned p){for(auto&u:uniforms)if(u.program==p&&u.epoch==world_program_epoch)return u;auto&u=uniforms[next_uniform++%uniforms.size()];u.program=p;u.epoch=world_program_epoch;u.offset=glGetUniformLocation(p,"uOffset");u.anchor=glGetUniformLocation(p,"uWorldAnchor");u.grass=glGetUniformLocation(p,"uGrass");u.vegetation=glGetUniformLocation(p,"uVegetation");u.lod=glGetUniformLocation(p,"uLodLevel");u.wind=glGetUniformLocation(p,"uWind");u.height=glGetUniformLocation(p,"uTreeHeight");u.reactive=glGetUniformLocation(p,"uActorReactive");u.prop=glGetUniformLocation(p,"uPropLodRange");u.graph=glGetUniformLocation(p,"uStudioGraphIndex");u.surface=glGetUniformLocation(p,"uSurfaceBinding");u.tint=glGetUniformLocation(p,"uStudioTintRough");u.normal_uv=glGetUniformLocation(p,"uStudioNormalUV");return u;}
};
bool world_road_traffic_config_load(const char *file,WorldRoadTrafficConfig &config,char *error,std::size_t cap){
    if(error&&cap)error[0]=0;std::ifstream in(file?file:"");std::string line;if(!in||!std::getline(in,line)||line!="VALWOOD_ROAD_TRAFFIC 1"){error_text(error,cap,"Cannot read supported road traffic settings");return false;}WorldRoadTrafficConfig next;unsigned points=0,seen=0,rows=0;bool end=false;
    while(std::getline(in,line)){if(++rows>128||line.size()>1024){error_text(error,cap,"Road traffic settings row too long");return false;}if(line.empty()||line[0]=='#')continue;std::istringstream row(line);std::string key;row>>key;bool ok=false;
        if(key=="point"){ok=points<6;if(ok){auto&p=next.points[points++];ok=bool(row>>p.x>>p.y>>p.z)&&trailing(row);}}
        else if(key=="trim"){ok=!(seen&1)&&bool(row>>next.start_trim>>next.end_trim)&&trailing(row);seen|=1;}
        else if(key=="lanes"){ok=!(seen&2)&&bool(row>>next.lane_offset>>next.road_half_width)&&trailing(row);seen|=2;}
        else if(key=="camp"||key=="town"||key=="encounter"||key=="satchel"){unsigned bit=key=="camp"?4:key=="town"?8:key=="encounter"?16:32;auto&p=key=="camp"?next.camp:key=="town"?next.town:key=="encounter"?next.encounter:next.satchel;double&r=key=="camp"?next.camp_radius:key=="town"?next.town_radius:key=="encounter"?next.encounter_radius:next.satchel_radius;ok=!(seen&bit)&&bool(row>>p.x>>p.y>>p.z>>r)&&trailing(row);seen|=bit;}
        else if(key=="end"){ok=trailing(row);end=true;}
        if(!ok){error_text(error,cap,"Malformed road traffic settings: "+key);return false;}if(end)break;
    }
    while(std::getline(in,line))if(!line.empty()&&line[0]!='#'){error_text(error,cap,"Trailing road traffic settings");return false;}
    world_road_traffic_core::Route test;if(!end||points!=6||seen!=63||!test.build(next)){error_text(error,cap,"Incomplete or unsafe road traffic settings");return false;}config=next;return true;
}
void world_road_traffic_free(WorldRoadTraffic*p){if(!p)return;for(auto&b:p->banks){for(auto&pair:b.pairs)for(auto&pass:pair.pass)if(pass.gpu)world_model_pose_blend_free(pass.gpu);for(auto*a:b.poses)if(a)world_model_free(a);}for(auto*b:p->cart_batches)if(b)world_model_instance_batch_free(b);for(auto*b:p->wheel_batches)if(b)world_model_instance_batch_free(b);if(p->cart_body)world_model_free(p->cart_body);if(p->cart_wheel)world_model_free(p->cart_wheel);delete p;}
WorldRoadTraffic *world_road_traffic_load(const char *models,const char *manifest_file,const WorldRoadTrafficConfig&config,char *error,std::size_t cap){
    if(error&&cap)error[0]=0;if(!models||!manifest_file){error_text(error,cap,"Missing road traffic paths");return nullptr;}auto*p=new(std::nothrow)WorldRoadTraffic;if(!p){error_text(error,cap,"Road traffic allocation failed");return nullptr;}
    auto fail=[&](const std::string&s)->WorldRoadTraffic*{error_text(error,cap,s);world_road_traffic_free(p);return nullptr;};std::string why;if(!p->core.route.build(config))return fail("Invalid road traffic route");if(!manifest_load(manifest_file,p->manifest,why))return fail(why);
    std::array<std::vector<std::array<double,3>>,bank_count> source_shapes;
    const auto root=std::filesystem::u8path(manifest_file).parent_path();const char*stem[]={"purchased_character_traveler","village_character_travelerwoman","purchased_character_craftsman","purchased_character_wayfarer"};const double stride[]={.9565,1.1464363315,.9596,.9811};
    for(unsigned bank=0;bank<bank_count;++bank){auto&b=p->banks[bank];b.walk=bank==4?16:8;b.count=b.walk+b.idle;float lo[3]={1e9f,1e9f,1e9f},hi[3]={-1e9f,-1e9f,-1e9f};
        for(unsigned i=0;i<b.count;++i){std::filesystem::path path;if(bank<4){char file[128];std::snprintf(file,sizeof(file),"%s_%s_%02u.glb",stem[bank],i<b.walk?"walk":"idle",i<b.walk?i:i-b.walk);path=std::filesystem::u8path(std::getenv("VALWOOD_CIVILIAN_MODELS")?std::getenv("VALWOOD_CIVILIAN_MODELS"):models)/file;}else path=root/std::filesystem::u8path(bank==4?p->manifest.mounted[i]:p->manifest.puller[i]);
            b.poses[i]=bank<4?world_model_load_pose(path.u8string().c_str(),WORLD_MODEL_POSE_BLEND_VERSION):world_model_load_pose_origin(path.u8string().c_str(),WORLD_MODEL_POSE_BLEND_VERSION);if(!b.poses[i])return fail("Road traffic cannot load pose: "+path.u8string());if(!shape_points(path,source_shapes[bank]))return fail("Road traffic source footprint capture failed");float a[3],z[3];if(!world_model_bounds(b.poses[i],a,z))return fail("Road traffic missing source bounds");for(unsigned k=0;k<3;++k){lo[k]=std::min(lo[k],a[k]);hi[k]=std::max(hi[k],z[k]);}p->render_stats.source_geometry_bytes+=world_model_geometry_bytes(b.poses[i]);++p->render_stats.pose_assets;
        }
        auto pair=[&](unsigned a,unsigned z,int contact=-1){if(b.pair_count>=max_pairs)return false;auto&v=b.pairs[b.pair_count++];v.a=a;v.b=z;v.contact=contact;for(auto&pass:v.pass){pass.gpu=world_model_pose_blend_create(capacity,WORLD_MODEL_POSE_BLEND_VERSION);if(!pass.gpu||!world_model_pose_blend_set_targets(pass.gpu,b.poses[a],b.poses[z])||!world_model_pose_blend_enable_transform(pass.gpu))return false;}return true;};
        const int walk_group[]={20,36,52,64},idle_group[]={28,44,60,72},transition_group[]={79,82,85,88};
        for(unsigned i=0;i<b.walk;++i)if(!pair(i,(i+1)%b.walk,bank<4?walk_group[bank]+int(i):-1))return fail("Road traffic incompatible walking topology");
        for(unsigned i=0;i<b.idle;++i)if(!pair(b.walk+i,b.walk+(i+1)%b.idle,bank<4?idle_group[bank]+int(i):-1))return fail("Road traffic incompatible idle topology");
        if(!pair(b.walk,0,bank<4?transition_group[bank]:-1)||!pair(0,b.walk,bank<4?transition_group[bank]+1:-1)||!pair(b.walk/2,b.walk,bank<4?transition_group[bank]+2:-1))return fail("Road traffic incompatible start/stop topology");
        auto&f=p->core.profiles[bank];f.radius=std::max(.25,double(std::max(std::abs(lo[0]),std::abs(hi[0]))));f.front=std::max(f.radius,double(hi[2]));f.rear=std::max(f.radius,-double(lo[2]));f.height=hi[1]-std::min(0.f,lo[1]);f.stride=bank<4?stride[bank]:bank==4?p->manifest.mounted_stride:p->manifest.puller_stride;f.speed=bank<4?1.02+.06*bank:bank==4?1.18:.88;b.radius=std::sqrt(std::max(lo[0]*lo[0],hi[0]*hi[0])+std::max(lo[2]*lo[2],hi[2]*hi[2]))+hi[1];
    }
    p->cart_body=world_model_load_pose_origin((root/std::filesystem::u8path(p->manifest.body)).u8string().c_str(),WORLD_MODEL_POSE_BLEND_VERSION);p->cart_wheel=world_model_load_pose_origin((root/std::filesystem::u8path(p->manifest.wheel)).u8string().c_str(),WORLD_MODEL_POSE_BLEND_VERSION);if(!p->cart_body||!p->cart_wheel)return fail("Road traffic cart geometry failed");
    if(!shape_points(root/std::filesystem::u8path(p->manifest.body),source_shapes[5],p->manifest.offset))return fail("Road cart footprint capture failed");
    for(unsigned j=0;j<2;++j){auto offset=p->manifest.axle[j];for(unsigned k=0;k<3;++k)offset[k]+=p->manifest.offset[k];if(!shape_points(root/std::filesystem::u8path(p->manifest.wheel),source_shapes[5],offset))return fail("Road wheel footprint capture failed");}
    for(unsigned bank=0;bank<bank_count;++bank){if(source_shapes[bank].empty())return fail("Empty traffic source footprint");fit_profile(p->core.profiles[bank],source_shapes[bank]);if(bank==5)p->banks[bank].radius=float(p->core.profiles[bank].front+p->core.profiles[bank].rear+p->core.profiles[bank].height);}
    for(unsigned i=0;i<passes;++i){p->cart_batches[i]=world_model_instance_batch_create(p->cart_body,3);p->wheel_batches[i]=world_model_instance_batch_create(p->cart_wheel,6);if(!p->cart_batches[i]||!p->wheel_batches[i])return fail("Road traffic cart batch failed");}p->render_stats.source_geometry_bytes+=world_model_geometry_bytes(p->cart_body)+world_model_geometry_bytes(p->cart_wheel);p->render_stats.pose_assets+=2;
    p->anchor_x=std::floor(config.points[0].x/128)*128;p->anchor_z=std::floor(config.points[0].z/128)*128;p->ready=true;return p;
}
bool world_road_traffic_validate_route(WorldRoadTraffic*p,const WorldRoadTrafficQuery&q,char*error,std::size_t cap){
    if(error&&cap)error[0]=0;
    if(!p||!p->ready){error_text(error,cap,"Road traffic assets are not ready");return false;}
    if(!p->core.validate(q)){
        const auto &c=p->core;char message[384];
        std::snprintf(message,sizeof message,"%s; bank%u station%.2f at %.3f %.3f %.3f",c.validation_reason,c.validation_bank,c.validation_station,c.validation_point.x,c.validation_point.y,c.validation_point.z);
        error_text(error,cap,message);return false;
    }
    return true;
}
void world_road_traffic_update(WorldRoadTraffic*p,const WorldRoadTrafficFrame&f){
    if(!p||!p->ready)return;double old_clock=p->core.clock;p->core.update(f);
    if(!p->core.initialized)return;
    for(unsigned i=0;i<capacity;++i){auto&a=p->core.actors[i].view;if(a.kind!=WorldRoadTrafficKind::Pullcart)continue;
        for(unsigned j=0;j<2;++j){auto q=p->manifest.axle[j];for(unsigned k=0;k<3;++k)q[k]+=p->manifest.offset[k];auto now=local_point(a,q);
            if(p->wheel_initialized[i]&&p->core.clock>old_clock){auto&old=p->wheel_previous[i][j];double dx=now[0]-old[0],dy=now[1]-old[1],dz=now[2]-old[2];double sign=dx*std::sin(a.yaw)+dz*std::cos(a.yaw)>=0?1:-1;p->wheel_distances[i][j]+=sign*std::sqrt(dx*dx+dy*dy+dz*dz);}
            p->wheel_previous[i][j]=now;a.wheel_angles[j]=std::remainder(p->wheel_distances[i][j]/p->manifest.wheel_radius,2*world_road_traffic_core::pi);
        }p->wheel_initialized[i]=true;a.wheel_angle=a.wheel_angles[0];
    }
}
bool world_road_traffic_blocks_segment(const WorldRoadTraffic*p,double ax,double az,double bx,double bz,double feet,double height,double radius){return p&&p->core.blocks(ax,az,bx,bz,feet,height,radius);}
bool world_road_traffic_camera_clear(const WorldRoadTraffic*p,double ax,double ay,double az,double bx,double by,double bz,float radius,float half_height){return !p||p->core.camera_clear(ax,ay,az,bx,by,bz,radius,half_height);}
unsigned world_road_traffic_count(const WorldRoadTraffic*p){return p&&p->core.initialized?capacity:0;}
bool world_road_traffic_actor(const WorldRoadTraffic*p,unsigned i,WorldRoadTrafficActor*out){if(!p||!out||i>=world_road_traffic_count(p))return false;*out=p->core.actors[i].view;return true;}
WorldRoadTrafficStats world_road_traffic_stats(const WorldRoadTraffic*p){if(!p)return {};auto s=p->core.stats;s.pose_assets=p->render_stats.pose_assets;s.source_geometry_bytes=p->render_stats.source_geometry_bytes;s.uploads=p->render_stats.uploads;s.draws=p->render_stats.draws;s.rigid_uploads=p->render_stats.rigid_uploads;s.rigid_uploads_skipped=p->render_stats.rigid_uploads_skipped;s.visible=p->render_stats.visible;s.draw_groups=p->render_stats.draw_groups;if(p->core.initialized)for(auto&a:p->core.actors){const auto&v=a.view;if(!v.active)continue;++s.actors;s.walkers+=v.kind==WorldRoadTrafficKind::Walker;s.riders+=v.kind==WorldRoadTrafficKind::Rider;s.pullcarts+=v.kind==WorldRoadTrafficKind::Pullcart;s.moving+=v.moving;s.yielding+=v.yielding;s.barrier_waiting+=v.barrier_wait;}return s;}
void world_road_traffic_draw(WorldRoadTraffic*p,unsigned program,double x,double y,double z,float yaw,float pitch,float aspect,unsigned pass,const float*vp){
    if(!p||!p->ready||!p->core.enabled||!p->core.validated||!p->core.initialized||!program||pass>=passes)return;for(double a:{x,y,z,double(yaw),double(pitch),double(aspect)})if(!std::isfinite(a))return;
    for(auto&b:p->banks)for(unsigned j=0;j<b.pair_count;++j)b.pairs[j].pass[pass].count=0;std::array<WorldModelInstance,3> carts={};std::array<WorldModelInstance,6>wheels={};unsigned nc=0,nw=0,visible=0;
    for(const auto&state:p->core.actors){const auto&a=state.view;if(!a.active)continue;unsigned bank=a.kind==WorldRoadTrafficKind::Walker?a.appearance:a.kind==WorldRoadTrafficKind::Rider?4:5;auto&b=p->banks[bank];double range=pass==2?245:205;if(std::hypot(a.x-x,a.z-z)>range)continue;if(!sphere_visible(vp,a.x-x,a.y+b.radius*.35-y,a.z-z,b.radius))continue;
        Pair*pair=nullptr;bool reverse=false;for(unsigned j=0;j<b.pair_count&&!pair;++j)if(b.pairs[j].a==a.pose_a&&b.pairs[j].b==a.pose_b)pair=&b.pairs[j];if(!pair)for(unsigned j=0;j<b.pair_count&&!pair;++j)if(b.pairs[j].b==a.pose_a&&b.pairs[j].a==a.pose_b){pair=&b.pairs[j];reverse=true;}if(!pair){++p->core.stats.failures;continue;}
        auto&batch=pair->pass[pass];float weight=float(reverse?1-a.pose_weight:a.pose_weight),sole=0;if(pair->contact>=0){float f=std::clamp(weight,0.f,1.f)*32;unsigned k=std::min(31u,unsigned(f));float t=f-k;sole=-(world_village_contact_curves[pair->contact][k]*(1-t)+world_village_contact_curves[pair->contact][k+1]*t);}
        batch.instances[batch.count]={float(a.x-p->anchor_x),float(a.y)+sole,float(a.z-p->anchor_z),float(a.yaw),1,float(a.pitch)};batch.weights[batch.count++]=weight;++visible;
        if(a.kind==WorldRoadTrafficKind::Pullcart){auto pos=local_point(a,p->manifest.offset);carts[nc++]={float(pos[0]-p->anchor_x),float(pos[1]),float(pos[2]-p->anchor_z),float(a.yaw),1,float(a.pitch)};for(unsigned j=0;j<p->manifest.axles;++j){auto q=p->manifest.axle[j];for(unsigned k=0;k<3;++k)q[k]+=p->manifest.offset[k];pos=local_point(a,q);wheels[nw++]={float(pos[0]-p->anchor_x),float(pos[1]),float(pos[2]-p->anchor_z),float(a.yaw),1,float(a.pitch+a.wheel_angles[j])};}}
    }
    glUseProgram(program);auto&u=p->locations(program);glUniform3f(u.offset,float(p->anchor_x-x),float(-y),float(p->anchor_z-z));glUniform2f(u.anchor,float(p->anchor_x),float(p->anchor_z));glUniform1i(u.grass,0);glUniform1i(u.vegetation,0);glUniform1i(u.lod,-1);glUniform1f(u.wind,0);glUniform1f(u.height,2);glUniform1f(u.reactive,pass==2?0:.85f);glUniform3f(u.prop,0,0,0);glUniform1i(u.graph,-1);glUniform1i(u.surface,-1);glUniform4f(u.tint,1,1,1,1);glUniform2f(u.normal_uv,1,1);unsigned groups=0;
    for(auto&b:p->banks)for(unsigned j=0;j<b.pair_count;++j){auto&batch=b.pairs[j].pass[pass];bool changed=batch.count!=batch.old_count||std::memcmp(batch.instances.data(),batch.old.data(),batch.count*sizeof(WorldModelInstance))||std::memcmp(batch.weights.data(),batch.old_weights.data(),batch.count*sizeof(float));if(changed){if(!world_model_pose_blend_upload(batch.gpu,batch.instances.data(),batch.weights.data(),batch.count)){++p->core.stats.failures;continue;}std::copy_n(batch.instances.begin(),batch.count,batch.old.begin());std::copy_n(batch.weights.begin(),batch.count,batch.old_weights.begin());batch.old_count=batch.count;++p->render_stats.uploads;}if(batch.count){world_model_pose_blend_draw(batch.gpu,program);++groups;++p->render_stats.draws;}}
    // Each rendering pass owns its buffer and last successful payload. Empty
    // and paused batches can retain GPU state; moving carts/wheels still upload
    // every changed transform, including camera-dependent visibility changes.
    auto rigid=[&](WorldModelInstanceBatch *gpu,const auto &instances,unsigned count,auto &old,unsigned &old_count){
        const bool changed=count!=old_count||std::memcmp(instances.data(),old.data(),count*sizeof(WorldModelInstance));
        if(changed){
            if(!world_model_instance_batch_upload(gpu,instances.data(),count)){++p->core.stats.failures;return;}
            std::copy_n(instances.begin(),count,old.begin());old_count=count;++p->render_stats.rigid_uploads;
        }else ++p->render_stats.rigid_uploads_skipped;
        if(count){world_model_instance_batch_draw(gpu,program);++groups;++p->render_stats.draws;}
    };
    rigid(p->cart_batches[pass],carts,nc,p->previous_carts[pass],p->previous_cart_counts[pass]);
    rigid(p->wheel_batches[pass],wheels,nw,p->previous_wheels[pass],p->previous_wheel_counts[pass]);
    glUniform1f(u.reactive,0);if(pass==0){p->render_stats.visible=visible;p->render_stats.draw_groups=groups;}
}

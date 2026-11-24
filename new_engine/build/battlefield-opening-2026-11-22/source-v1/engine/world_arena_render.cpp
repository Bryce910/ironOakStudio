#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_arena_render.h"
#include "engine/world_models.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#include <vector>
namespace {
constexpr unsigned dynamic_capacity=12000,arrow_capacity=32;
struct P {float x=0,y=0,z=0;};
P operator+(P a,P b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
P operator-(P a,P b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
P operator*(P a,float f){return {a.x*f,a.y*f,a.z*f};}
P cross(P a,P b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
P normal(P a){const float l=std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);return l>.00001f?a*(1/l):P{0,1,0};}
// Attributes4/5/6 match world_foliage: offsetXYZ/scale, yaw, fade. Keep their
// identity values in the vertex stream so the portable GL loader needs no
// glVertexAttrib*f constant-attribute imports on Windows.
struct V {P p,n;float uv[2],tangent[4]={1,0,0,1},paint=0;float instance[4]={0,0,0,1},yaw=0,fade=1;};
struct Mesh {GLuint vao=0,vbo=0,texture=0;unsigned count=0;float color[4]={1,1,1,1};float roughness=.9f,metallic=0;};
using Vertices=std::vector<V>;
void vertex(Vertices &out,P p,P n,float u,float v){V a;a.p=p;a.n=n;a.uv[0]=u;a.uv[1]=v;out.push_back(a);}
void quad(Vertices &out,P a,P b,P c,P d,float tile=1){const auto n=normal(cross(b-a,c-a));vertex(out,a,n,0,0);vertex(out,b,n,tile,0);vertex(out,c,n,tile,tile);vertex(out,a,n,0,0);vertex(out,c,n,tile,tile);vertex(out,d,n,0,tile);}
void box(Vertices &out,P c,P h){
 P a{c.x-h.x,c.y-h.y,c.z-h.z},b{c.x+h.x,c.y-h.y,c.z-h.z},d{c.x-h.x,c.y+h.y,c.z-h.z},e{c.x-h.x,c.y-h.y,c.z+h.z};
 P cc{b.x,d.y,a.z},f{b.x,a.y,e.z},g{b.x,d.y,e.z},hh{a.x,d.y,e.z};
 quad(out,a,d,cc,b);quad(out,e,f,g,hh);quad(out,a,e,hh,d);quad(out,b,cc,g,f);quad(out,d,hh,g,cc);quad(out,a,b,f,e);
}
void tube(Vertices &out,P a,P b,float radius,unsigned sides=8){
 const P axis=normal(b-a),u=normal(cross(axis,std::abs(axis.y)>.9f?P{1,0,0}:P{0,1,0})),v=cross(axis,u);
 for(unsigned i=0;i<sides;++i){const float t=float(i)*6.28318530718f/sides,q=float(i+1)*6.28318530718f/sides;
  const P r=(u*std::cos(t)+v*std::sin(t))*radius,s=(u*std::cos(q)+v*std::sin(q))*radius;
  quad(out,a+r,a+s,b+s,b+r);vertex(out,a,axis*-1,0,0);vertex(out,a+s,axis*-1,1,0);vertex(out,a+r,axis*-1,1,1);
  vertex(out,b,axis,0,0);vertex(out,b+r,axis,1,0);vertex(out,b+s,axis,1,1);
 }
}
unsigned hash(unsigned a){a^=a>>16;a*=0x7feb352d;a^=a>>15;a*=0x846ca68b;return a^(a>>16);}
GLuint texture(bool wood){
 // Optional owned ground image for dressed arenas. Keep legacy pads and wood
 // unchanged. SDL owns decoder pixels; the existing mesh owns the GL texture.
 if(!wood){
  SDL_Surface *source=SDL_LoadBMP("scenery/arena_floor.bmp");
  if(source){
   SDL_Surface *image=source->w>0&&source->h>0&&source->w<=2048&&source->h<=2048?SDL_ConvertSurfaceFormat(source,SDL_PIXELFORMAT_RGBA32,0):nullptr;
   SDL_FreeSurface(source);
   if(image){
    GLint alignment=4;glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);glPixelStorei(GL_UNPACK_ALIGNMENT,4);
    GLuint id=0;glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);
    glTexImage2D(GL_TEXTURE_2D,0,GL_SRGB8_ALPHA8,image->w,image->h,0,GL_RGBA,GL_UNSIGNED_BYTE,image->pixels);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);glGenerateMipmap(GL_TEXTURE_2D);
    glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);std::printf("[Arena ground] Owned dressed-earth texture %dx%d\n",image->w,image->h);SDL_FreeSurface(image);return id;
   }
   std::fprintf(stderr,"[Arena ground] Invalid optional image; legacy ground retained\n");
  }
 }
 std::array<unsigned char,128*128*3> data={};
 for(unsigned y=0;y<128;++y)for(unsigned x=0;x<128;++x){
  const float noise=float(hash(y*128+x)&255)/255.f;
  const float variation=wood?.72f+.18f*std::sin(x*.9f+std::sin(y*.07f)*1.2f)+noise*.20f:
      .72f+noise*.24f+.04f*std::sin(x*.16f+y*.08f);
  const float rgb[3]={wood?.34f:.51f,wood?.21f:.40f,wood?.115f:.275f};
  for(unsigned c=0;c<3;++c)data[(y*128+x)*3+c]=static_cast<unsigned char>(std::clamp(rgb[c]*variation*255,0.f,255.f));
 }
 GLuint id=0;glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);glTexImage2D(GL_TEXTURE_2D,0,GL_SRGB8,128,128,0,GL_RGB,GL_UNSIGNED_BYTE,data.data());
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);glGenerateMipmap(GL_TEXTURE_2D);return id;
}
void setup(Mesh &m,const Vertices &v,bool dynamic=false){
 glGenVertexArrays(1,&m.vao);glGenBuffers(1,&m.vbo);glBindVertexArray(m.vao);glBindBuffer(GL_ARRAY_BUFFER,m.vbo);
 glBufferData(GL_ARRAY_BUFFER,(dynamic?dynamic_capacity:v.size())*sizeof(V),dynamic?nullptr:v.data(),dynamic?GL_DYNAMIC_DRAW:GL_STATIC_DRAW);
 const unsigned locations[]={0,1,2,3,7,4,5,6};const int sizes[]={3,3,2,4,1,4,1,1};
 const std::size_t offsets[]={offsetof(V,p),offsetof(V,n),offsetof(V,uv),offsetof(V,tangent),offsetof(V,paint),offsetof(V,instance),offsetof(V,yaw),offsetof(V,fade)};
 for(unsigned i=0;i<8;++i){glEnableVertexAttribArray(locations[i]);glVertexAttribPointer(locations[i],sizes[i],GL_FLOAT,GL_FALSE,sizeof(V),reinterpret_cast<void*>(offsets[i]));}
 glBindVertexArray(0);m.count=unsigned(v.size());
}
void integer(GLuint p,const char *n,int value){glUniform1i(glGetUniformLocation(p,n),value);}
void scalar(GLuint p,const char *n,float value){glUniform1f(glGetUniformLocation(p,n),value);}
void draw(Mesh &m,unsigned program){
 if(!m.count)return;
 integer(program,"uHasBase",m.texture!=0);integer(program,"uHasNormal",0);integer(program,"uHasARM",0);integer(program,"uHasEmissive",0);
 integer(program,"uBase",0);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,m.texture);
 glUniform4fv(glGetUniformLocation(program,"uBaseFactor"),1,m.color);scalar(program,"uRoughness",m.roughness);scalar(program,"uMetallic",m.metallic);scalar(program,"uAlphaCutoff",0);
 glBindVertexArray(m.vao);glDrawArrays(GL_TRIANGLES,0,m.count);
}
void uniforms(unsigned p,double x,double y,double z,double ox,double oy,double oz){
 glUseProgram(p);glUniform3f(glGetUniformLocation(p,"uOffset"),float(ox-x),float(oy-y),float(oz-z));glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),float(ox),float(oz));
 for(const char *n:{"uGrass","uVegetation","uInstanceFade","uPoseBlendEnabled","uCharacterEnabled","uBakedLightEnabled","uSourcePaintEnabled","uFullTransform","uFacadeMode","uRootedGrassWind"})integer(p,n,0);
 integer(p,"uLodLevel",-1);scalar(p,"uWind",0);scalar(p,"uActorReactive",0);glUniform3f(glGetUniformLocation(p,"uPropLodRange"),0,0,0);
}
bool finite(const WorldArenaBowPose &b){return std::isfinite(b.x)&&std::isfinite(b.y)&&std::isfinite(b.z)&&std::isfinite(b.yaw)&&std::isfinite(b.pitch)&&std::isfinite(b.draw);}
}
struct WorldArenaRender {
 WorldArenaRenderConfig config;std::array<Mesh,5> static_meshes={};std::array<Mesh,3> bow_meshes={};Mesh arrows;
 std::array<Vertices,3> bow_vertices;Vertices arrow_vertices;WorldArenaRenderStats stats;
 bool bow_visible=false,bow_first=false;unsigned equipment=340;
 WorldModelAsset *dressed_floor=nullptr,*dressed_boundary=nullptr;
 std::array<std::array<WorldModelAsset*,3>,2> bow_assets={};std::array<std::array<WorldModelPoseBlend*,2>,2> bow_blends={};
 float sockets[2][3][2][3]={};unsigned bow_slot=0;bool bow_loaded=false;
};
WorldArenaRender *world_arena_render_create(const WorldArenaRenderConfig &c,char *error,std::size_t capacity){
 if(error&&capacity)error[0]=0;
 if(!SDL_GL_GetCurrentContext()||!std::isfinite(c.x)||!std::isfinite(c.ground_y)||!std::isfinite(c.z)||!std::isfinite(c.half_x)||!std::isfinite(c.half_z)||c.half_x<5||c.half_x>40||c.half_z<5||c.half_z>40)return nullptr;
 auto *p=new(std::nothrow)WorldArenaRender;if(!p)return nullptr;p->config=c;std::array<Vertices,5> v;
 const float hx=c.half_x,hz=c.half_z;
 // Shallow dressed sand pad; coarse border timbers conceal its thin edge.
 quad(v[0],{-hx,.012f,-hz},{-hx,.012f,hz},{hx,.012f,hz},{hx,.012f,-hz},8);
 for(unsigned side=0;side<4;++side){
  const bool eastwest=side<2;const float fixed=(side%2?1.f:-1.f)*(eastwest?hx:hz),half=eastwest?hz:hx;
  const unsigned bays=unsigned(std::ceil(half*2/2.5f));
  for(unsigned i=0;i<=bays;++i){const float at=-half+half*2*i/bays;if(side==2&&std::abs(at)<1.7f)continue;
   P a=eastwest?P{fixed,.65f,at}:P{at,.65f,fixed};box(v[1],a,{.105f,.72f,.105f});
   box(v[2],a+P{0,.28f,0},{.112f,.035f,.112f});
   if(i<bays){const float end=-half+half*2*(i+1)/bays;if(side==2&&(at<1.7f&&end>-1.7f))continue;
    for(float height:{.45f,.95f}){P center=eastwest?P{fixed,height,(at+end)*.5f}:P{(at+end)*.5f,height,fixed};
     box(v[1],center,eastwest?P{.045f,.055f,(end-at)*.5f}:P{(end-at)*.5f,.055f,.045f});}
   }
  }
 }
 for(float sign:{-1.f,1.f}){
  box(v[1],{sign*1.9f,1.5f,hz},{.16f,1.5f,.16f});box(v[2],{sign*1.9f,2.45f,hz},{.18f,.06f,.18f});
  const float x=sign*1.9f;
  quad(v[sign<0?3:4],{x,2.7f,hz},{x+sign*.85f,2.7f,hz},{x+sign*.85f,1.45f,hz},{x,1.65f,hz});
 }
 // An inner rail and segmented corner barrels give the waiting rank a clear edge.
 for(float sign:{-1.f,1.f})for(unsigned i=0;i<3;++i){const float z=-hz+1.1f+i*.65f;tube(v[1],{sign*(hx-1),0,z},{sign*(hx-1),.70f,z},.24f,10);}
 for(unsigned i=0;i<5;++i){setup(p->static_meshes[i],v[i]);p->stats.vertices+=unsigned(v[i].size());}
 // Optional authored PBR floor. Match the exact physics rectangle before
 // replacing the fallback; never layer two coplanar surfaces. Loading only.
 if(FILE *file=std::fopen("scenery/arena_floor.glb","rb")){
  std::fclose(file);auto *asset=world_model_load("scenery/arena_floor.glb");
  float lo[3]={},hi[3]={};const WorldModelInstance placement{0,.012f,0,0,1};
  if(asset&&world_model_bounds(asset,lo,hi)&&std::abs(lo[0]+hx)<.001f&&std::abs(hi[0]-hx)<.001f&&
     std::abs(lo[2]+hz)<.001f&&std::abs(hi[2]-hz)<.001f&&std::abs(lo[1])<.001f&&std::abs(hi[1])<.001f&&
     world_model_upload_instances(asset,&placement,1)){
   p->dressed_floor=asset;std::printf("[Arena ground] Authored PBR floor; physics dimensions preserved %.1fx%.1f\n",hx*2,hz*2);
  }else{if(asset)world_model_free(asset);std::fprintf(stderr,"[Arena ground] Invalid PBR floor; legacy ground retained\n");}
 }
 if(FILE *file=std::fopen("scenery/arena_boundary.glb","rb")){
  std::fclose(file);auto *asset=world_model_load("scenery/arena_boundary.glb");float lo[3]={},hi[3]={};
  const WorldModelInstance placement{0,0,0,0,1};
  if(asset&&world_model_bounds(asset,lo,hi)&&lo[0]>=-hx-.5f&&hi[0]<=hx+.5f&&lo[2]>=-hz-.5f&&hi[2]<=hz+.5f&&
     std::abs(lo[1])<.001f&&hi[1]<=4&&world_model_upload_instances(asset,&placement,1)){
   p->dressed_boundary=asset;std::printf("[Arena ground] PBR timber boundary; existing collision retained\n");
  }else{if(asset)world_model_free(asset);std::fprintf(stderr,"[Arena ground] Invalid PBR boundary; legacy rail retained\n");}
 }
 if(!p->dressed_floor)p->static_meshes[0].texture=texture(false);
 p->static_meshes[1].texture=texture(true);
 const float metal[4]={.105f,.12f,.14f,1},blue[4]={.035f,.09f,.22f,1},red[4]={.31f,.045f,.035f,1};
 std::copy_n(metal,4,p->static_meshes[2].color);p->static_meshes[2].metallic=.65f;p->static_meshes[2].roughness=.65f;
 std::copy_n(blue,4,p->static_meshes[3].color);std::copy_n(red,4,p->static_meshes[4].color);
 for(unsigned i=0;i<3;++i){p->bow_vertices[i].reserve(dynamic_capacity);setup(p->bow_meshes[i],{},true);}
 p->arrow_vertices.reserve(dynamic_capacity);setup(p->arrows,{},true);
 p->bow_meshes[0].texture=p->static_meshes[1].texture;const float string[4]={.64f,.52f,.32f,1};std::copy_n(string,4,p->bow_meshes[1].color);
 std::copy_n(metal,4,p->bow_meshes[2].color);p->bow_meshes[2].metallic=.7f;p->bow_meshes[2].roughness=.4f;
 const float arrow[4]={.43f,.27f,.12f,1};std::copy_n(arrow,4,p->arrows.color);
 if(glGetError()!=GL_NO_ERROR){if(error&&capacity)std::snprintf(error,capacity,"Could not create bounded arena geometry");world_arena_render_free(p);return nullptr;}return p;
}
bool world_arena_render_load_bow(WorldArenaRender *p,const char *directory,const float sockets[2][3][2][3],char *error,std::size_t capacity){
 if(error&&capacity)error[0]=0;
 if(!p||!directory||!*directory||!sockets||p->bow_loaded)return false;
 for(unsigned kind=0;kind<2;++kind)for(unsigned frame=0;frame<3;++frame)for(unsigned grip=0;grip<2;++grip)for(unsigned axis=0;axis<3;++axis)
  if(!std::isfinite(sockets[kind][frame][grip][axis])||std::abs(sockets[kind][frame][grip][axis])>4)return false;
 std::array<std::array<WorldModelAsset*,3>,2> assets={};std::array<std::array<WorldModelPoseBlend*,2>,2> blends={};
 bool ok=true;for(unsigned kind=0;kind<2&&ok;++kind){
  for(unsigned i=0;i<3&&ok;++i){char path[1400];const int n=std::snprintf(path,sizeof path,"%s/bow_%s_%03u.glb",directory,kind?"arms":"body",i);
   if(n<0||std::size_t(n)>=sizeof path)ok=false;
   else{assets[kind][i]=kind?world_model_load_pose_origin(path,WORLD_MODEL_POSE_BLEND_VERSION):world_model_load_pose(path,WORLD_MODEL_POSE_BLEND_VERSION);ok=assets[kind][i]!=nullptr;}}
  for(unsigned i=0;i<2&&ok;++i){blends[kind][i]=world_model_pose_blend_create(1,WORLD_MODEL_POSE_BLEND_VERSION);ok=blends[kind][i]&&world_model_pose_blend_set_targets(blends[kind][i],assets[kind][i],assets[kind][i+1]);}
 }
 if(!ok){for(auto &kind:blends)for(auto *b:kind)if(b)world_model_pose_blend_free(b);for(auto &kind:assets)for(auto *a:kind)if(a)world_model_free(a);
  if(error&&capacity)std::snprintf(error,capacity,"Owned bow poses missing or incompatible");
  return false;}
 p->bow_assets=assets;p->bow_blends=blends;std::memcpy(p->sockets,sockets,sizeof p->sockets);p->bow_loaded=true;p->stats.bow_pose_assets=6;return true;
}
bool world_arena_render_update(WorldArenaRender *p,const WorldArenaBowPose &bow,const WorldArenaRenderArrow *arrows,unsigned count){
 if(!p||!finite(bow)||count>arrow_capacity||(count&&!arrows))return false;
 for(unsigned i=0;i<count;++i)for(unsigned k=0;k<3;++k)if(!std::isfinite(arrows[i].base[k])||!std::isfinite(arrows[i].tip[k]))return false;
 for(auto &v:p->bow_vertices)v.clear();
 p->arrow_vertices.clear();p->bow_visible=bow.visible;p->bow_first=bow.first_person;
 if(bow.visible){
  const float cy=std::cos(bow.yaw),sy=std::sin(bow.yaw),cp=std::cos(bow.pitch),sp=std::sin(bow.pitch);
  const P right{cy,0,sy},forward{sy*cp,sp,-cy*cp},up{-sy*sp,cp,cy*sp};
  const P origin{float(bow.x-p->config.x),float(bow.y-p->config.ground_y),float(bow.z-p->config.z)};
  const P center=bow.first_person?P{-.26f,-.18f,.72f}:P{-.32f,1.3f,.64f};
  auto point=[&](P q){return origin+right*q.x+up*q.y+forward*q.z;};
  P grip=point(center+P{0,0,.19f}),nock=point(center+P{0,0,-.36f*std::clamp(bow.draw,0.f,1.f)}),axis=forward,bow_up=up;
  if(p->bow_loaded){
   const float d=std::clamp(bow.draw,0.f,1.f);const unsigned slot=d<.5f?0:1;const float weight=d<.5f?d*2:(d-.5f)*2;p->bow_slot=slot;
   const float yaw=3.14159265359f-bow.yaw,cs=std::cos(yaw),sn=std::sin(yaw);
   // Keep both source hand sockets safely ahead of the first-person near
   // plane. The same transform places the owned arms and their bow/string;
   // the torso/third-person pose retains its exact authored placement.
   const P body_origin=origin,arms_origin=origin-right*.30f+P{0,-1.40f,0}+P{sy,0,-cy}*.35f;
   const unsigned kind=bow.first_person?1:0;const float scale=kind?.8f:1.f;const P base=kind?arms_origin:body_origin;
   auto held=[&](unsigned hand){P q;float *values=&q.x;for(unsigned k=0;k<3;++k)values[k]=p->sockets[kind][slot][hand][k]+(p->sockets[kind][slot+1][hand][k]-p->sockets[kind][slot][hand][k])*weight;
    return base+P{cs*q.x+sn*q.z,q.y,-sn*q.x+cs*q.z}*scale;};
   grip=held(0);nock=held(1);axis=normal(grip-nock);bow_up={0,1,0};
   for(unsigned k=0;k<2;++k){const P base_position=k?arms_origin:body_origin;WorldModelInstance instance={base_position.x,base_position.y,base_position.z,yaw,k?.8f:1.f};
    if(!world_model_pose_blend_upload(p->bow_blends[k][slot],&instance,&weight,1)){++p->stats.failures;return false;}}
  }
  P previous;for(unsigned i=0;i<=20;++i){const float t=-1.f+2.f*i/20;const P q=grip+bow_up*(t*.64f)-axis*(.19f*t*t);
   if(i)tube(p->bow_vertices[0],previous,q,.018f,6);
   previous=q;}
  tube(p->bow_vertices[1],grip-bow_up*.64f-axis*.19f,nock,.0025f,4);
  tube(p->bow_vertices[1],nock,grip+bow_up*.64f-axis*.19f,.0025f,4);
  tube(p->bow_vertices[0],grip-bow_up*.075f,grip+bow_up*.075f,.026f,8);
  P tip=grip+axis*.40f;tube(p->bow_vertices[1],nock,tip,.004f,5);tube(p->bow_vertices[2],tip,tip+axis*.075f,.013f,4);
  const P feather=normal(cross(axis,bow_up));for(float side:{-1.f,1.f})quad(p->bow_vertices[1],nock+axis*.035f,nock+feather*(side*.025f)+axis*.055f,nock+feather*(side*.025f)+axis*.16f,nock+axis*.18f);
 }
 unsigned active=0;for(unsigned i=0;i<count;++i)if(arrows[i].visible){
  P a{float(arrows[i].base[0]-p->config.x),float(arrows[i].base[1]-p->config.ground_y),float(arrows[i].base[2]-p->config.z)};
  P b{float(arrows[i].tip[0]-p->config.x),float(arrows[i].tip[1]-p->config.ground_y),float(arrows[i].tip[2]-p->config.z)};
  if(std::abs(a.x)>500||std::abs(a.y)>500||std::abs(a.z)>500||std::abs(b.x)>500||std::abs(b.y)>500||std::abs(b.z)>500)continue;
  tube(p->arrow_vertices,a,b,.008f,5);++active;
 }
 auto upload=[&](Mesh &m,const Vertices &v){if(v.size()>dynamic_capacity)return false;glBindBuffer(GL_ARRAY_BUFFER,m.vbo);if(!v.empty())glBufferSubData(GL_ARRAY_BUFFER,0,v.size()*sizeof(V),v.data());m.count=unsigned(v.size());++p->stats.uploads;return true;};
 for(unsigned i=0;i<3;++i)if(!upload(p->bow_meshes[i],p->bow_vertices[i]))return false;
 if(!upload(p->arrows,p->arrow_vertices))return false;
 p->stats.active_arrows=active;return true;
}
void world_arena_render_draw(WorldArenaRender *p,unsigned program,double x,double y,double z,bool){if(!p||!program)return;uniforms(program,x,y,z,p->config.x,p->config.ground_y,p->config.z);if(p->dressed_floor)world_model_draw(p->dressed_floor,program);if(p->dressed_boundary)world_model_draw(p->dressed_boundary,program);for(unsigned i=0;i<p->static_meshes.size();++i){if((i==0&&p->dressed_floor)||((i==1||i==2)&&p->dressed_boundary))continue;draw(p->static_meshes[i],program);}++p->stats.draws;}
void world_arena_render_draw_bow(WorldArenaRender *p,unsigned program,double x,double y,double z,bool shadow){
 if(!p||!program)return;
 uniforms(program,x,y,z,p->config.x,p->config.ground_y,p->config.z);
 if(p->bow_visible&&(!shadow||!p->bow_first)){for(auto &m:p->bow_meshes)draw(m,program);++p->stats.bow_draws;}
}
void world_arena_render_draw_arrows(WorldArenaRender *p,unsigned program,double x,double y,double z,bool){
 if(!p||!program)return;
 uniforms(program,x,y,z,p->config.x,p->config.ground_y,p->config.z);
 if(p->arrows.count){draw(p->arrows,program);++p->stats.arrow_draws;}
}
void world_arena_render_draw_bow_actor(WorldArenaRender *p,unsigned program,double x,double y,double z,bool first_person,bool shadow){
 if(!p||!program||!p->bow_visible||!p->bow_loaded||first_person!=p->bow_first||(shadow&&first_person))return;
 uniforms(program,x,y,z,p->config.x,p->config.ground_y,p->config.z);world_model_pose_blend_draw_equipped(p->bow_blends[first_person?1:0][p->bow_slot],program,p->equipment,1);
}
WorldArenaRenderStats world_arena_render_stats(const WorldArenaRender *p){return p?p->stats:WorldArenaRenderStats{};}
void world_arena_render_equipment(WorldArenaRender *p,unsigned mask){if(p&&mask<=511)p->equipment=mask;}
unsigned world_arena_render_equipment(const WorldArenaRender *p){return p?p->equipment:0;}
void world_arena_render_free(WorldArenaRender *p){if(!p)return;if(p->dressed_floor)world_model_free(p->dressed_floor);if(p->dressed_boundary)world_model_free(p->dressed_boundary);for(auto &kind:p->bow_blends)for(auto *b:kind)if(b)world_model_pose_blend_free(b);for(auto &kind:p->bow_assets)for(auto *a:kind)if(a)world_model_free(a);for(auto &m:p->static_meshes){if(m.texture)glDeleteTextures(1,&m.texture);glDeleteBuffers(1,&m.vbo);glDeleteVertexArrays(1,&m.vao);}for(auto &m:p->bow_meshes){glDeleteBuffers(1,&m.vbo);glDeleteVertexArrays(1,&m.vao);}glDeleteBuffers(1,&p->arrows.vbo);glDeleteVertexArrays(1,&p->arrows.vao);delete p;}

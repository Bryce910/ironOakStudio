#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "stb_image.h"
#include "gl_helpers.h"
#include "engine/world_road_traffic.h"
extern "C" {
#include "engine/texture_cache.h"
}
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
static void APIENTRY debug(GLenum,GLenum,GLuint,GLenum,GLsizei,const GLchar*message,const void*){std::fprintf(stderr,"DEBUG %s\n",message);}
static unsigned checks=0;
static void need(bool b,const char*s){++checks;if(!b){std::fprintf(stderr,"FAIL %s (%s)\n",s,SDL_GetError());std::exit(1);}}
static double grade=0;static bool ground(double,double z,double reference,double&h,const void*){h=grade?12.+.1*std::sin((z-5172)*.2):reference;return true;}
static bool clear(double,double,double,double,double,double,double,const void*){return true;}
struct V{double x,y,z;};static V sub(V a,V b){return {a.x-b.x,a.y-b.y,a.z-b.z};}static double dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}static V unit(V v){double n=std::sqrt(dot(v,v));return {v.x/n,v.y/n,v.z/n};}static V cross(V a,V b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static std::array<float,16> vp(V relative_target){V f=unit(relative_target),r=unit(cross(f,{0,1,0})),u=cross(r,f);std::array<float,16>view={float(r.x),float(u.x),float(-f.x),0,float(r.y),float(u.y),float(-f.y),0,float(r.z),float(u.z),float(-f.z),0,0,0,0,1};float sy=1/std::tan(.54),sx=sy*360.f/640.f,n=.1f,far=250;std::array<float,16>p={sx,0,0,0,0,sy,0,0,0,0,-(far+n)/(far-n),-1,0,0,-2*far*n/(far-n),0},out={};for(unsigned c=0;c<4;c++)for(unsigned row=0;row<4;row++)for(unsigned k=0;k<4;k++)out[c*4+row]+=p[k*4+row]*view[c*4+k];return out;}
static unsigned program(const std::string&root,const char*fragment){auto vs=glsl_load((root+"/shaders/world_foliage.vs.glsl").c_str()),fs=glsl_load((root+"/shaders/"+fragment).c_str());need(vs&&fs,"actual shader files");std::string frag(fs);frag.insert(frag.find('\n')+1,"#define VALWOOD_QA_DISABLE_TEXTURE_GATHER 1\n");unsigned p=GLH_program(vs,frag.c_str());free(vs);free(fs);int linked=0;glGetProgramiv(p,GL_LINK_STATUS,&linked);need(linked,"actual shader link");return p;}
static void save(const std::string&path){std::vector<unsigned char>rgb(640*360*3);glReadPixels(0,0,640,360,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());FILE*f=fopen(path.c_str(),"wb");need(f,"image write");fprintf(f,"P6\n640 360\n255\n");for(int y=359;y>=0;y--)fwrite(rgb.data()+y*640*3,1,640*3,f);fclose(f);}

int main(int argc,char**argv){
 need(argc==5,"source models manifest output");need(SDL_Init(SDL_INIT_VIDEO)==0,"SDL");
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
 auto*w=SDL_CreateWindow("Isolated current-road-animation review",0,0,640,360,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);need(w,"window");auto ctx=SDL_GL_CreateContext(w);need(ctx,"context");
 unsigned color=program(argv[1],"world_foliage.fs.glsl");char error[1024]={};WorldRoadTrafficConfig cfg;cfg.lane_offset=1.30;
 auto*p=world_road_traffic_load(argv[2],argv[3],cfg,error,sizeof(error));if(!p)std::fprintf(stderr,"%s\n",error);need(p,"current road assets");need(world_road_traffic_validate_route(p,{ground,clear,nullptr},error,sizeof(error)),"synthetic supported flat route");
 WorldRoadTrafficFrame frame;frame.query={ground,clear,nullptr};frame.player_x=-11988;frame.player_feet_y=11.8;frame.player_z=5172;world_road_traffic_update(p,frame);frame.dt=1./60;
 for(unsigned i=0;i<90;++i)world_road_traffic_update(p,frame);
 glViewport(0,0,640,360);glEnable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glUseProgram(color);
 const char*samplers[]={"uBase","uNormal","uARM","uShadow","uMacroHeight","uEmissive","uEnvironmentCube","uEnvironmentBRDF","uLocalProbe0","uLocalProbe1"};
 for(unsigned si=0;si<10;++si)glUniform1i(glGetUniformLocation(color,samplers[si]),si==6?15:si==8?13:si==9?14:si);
 glUniform1i(glGetUniformLocation(color,"uVillageSkyMask"),11);glUniform1i(glGetUniformLocation(color,"uVillageSkyDirectional"),12);glUniform1i(glGetUniformLocation(color,"uVillageSkyUp"),12);glUniform1i(glGetUniformLocation(color,"uLocalLightShadow"),13);
 GLH_vec3(color,"uSun",.3,.9,.2);GLH_vec3(color,"uSunColor",2,1.9,1.7);GLH_vec3(color,"uAmbient",.5,.5,.5);GLH_vec3(color,"uSkyLight",.5,.55,.6);GLH_vec3(color,"uGroundLight",.25,.22,.2);glUniform2f(glGetUniformLocation(color,"uWorldSize"),100800,56800);
 for(unsigned id:{0u,1u,3u,5u,24u,28u}){
  for(unsigned n=0;n<60;++n){
   world_road_traffic_update(p,frame);WorldRoadTrafficActor a;need(world_road_traffic_actor(p,id,&a),"actor");
   const double distance=id<24?3.2:6.;double ex=a.x+std::cos(a.yaw)*distance-std::sin(a.yaw)*distance*.45,ey=a.y+(id<24?1.2:1.9),ez=a.z-std::sin(a.yaw)*distance-std::cos(a.yaw)*distance*.45;
   auto matrix=vp({a.x-ex,a.y+(id<24?.9:1.35)-ey,a.z-ez});GLH_mat4(color,"uVP",matrix.data());
   glClearColor(.12,.16,.2,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);world_road_traffic_draw(p,color,ex,ey,ez,0,0,640.f/360,0,matrix.data());glFinish();need(glGetError()==GL_NO_ERROR,"GL0");
   char name[200];std::snprintf(name,sizeof(name),"/actor-%02u-frame-%03u.ppm",id,n);if(n%15==0)save(std::string(argv[4])+name);
  }
 }
 world_road_traffic_free(p);glDeleteProgram(color);SDL_GL_DeleteContext(ctx);SDL_DestroyWindow(w);SDL_Quit();printf("PASS %u checks; current production objects; isolated synthetic terrain only\n",checks);
}

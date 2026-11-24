#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_studio.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
bool studio_wind_features=true;
struct Program {GLuint id;GLint at(const char *n){return glGetUniformLocation(id,n);}void i(const char *n,int x){glUniform1i(at(n),x);}};
struct ExactProductionWindMethods {world_surface::Data data;double wind_time=0;
    static void particle_wind(void *userdata,double x,double y,double z,double time,float out[2]){
        const auto &data=*static_cast<const world_surface::Data *>(userdata);auto v=world_surface::wind(data,{x,y,z},time,{out[0],out[1]});out[0]=v[0];out[1]=v[1];
    }
    void bind_wind(Program &p)const{
        if(!studio_wind_features)return;
        std::array<float,64> centers={},radii={},directions={};
        for(unsigned i=0;i<data.winds.size();i++){
            const auto &w=data.winds[i];for(unsigned k=0;k<3;k++){centers[i*4+k]=float(w.center[k]);radii[i*4+k]=w.radius[k];}
            const double phase=std::remainder(wind_time,w.period)*6.283185307179586/w.period+(w.seed%65536)*.0000958738;
            centers[i*4+3]=float(w.speed*(1+w.gust*.5*(std::sin(phase)+.35*std::sin(phase*2))));double length=std::hypot(w.direction[0],w.direction[1]);directions[i*4]=float(w.direction[0]/length);directions[i*4+1]=float(w.direction[1]/length);
        }
        p.i("uSurfaceWindCount",int(data.winds.size()));if(!data.winds.empty()){glUniform4fv(p.at("uSurfaceWindCenter[0]"),data.winds.size(),centers.data());glUniform4fv(p.at("uSurfaceWindRadius[0]"),data.winds.size(),radii.data());glUniform4fv(p.at("uSurfaceWindDirection[0]"),data.winds.size(),directions.data());}
    }
};
std::string read(const char *p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
GLuint compile(GLenum stage,const std::string &text){GLuint s=glCreateShader(stage);const char *p=text.c_str();glShaderSource(s,1,&p,nullptr);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){char msg[4096];glGetShaderInfoLog(s,sizeof(msg),nullptr,msg);std::cerr<<msg;std::exit(2);}return s;}
int main(int argc,char **argv){if(argc!=4)return 2;world_studio::Document base,doc;std::string e;if(!world_studio::load(argv[1],base,e)||!world_studio::load(argv[2],doc,e)){std::cerr<<e;return 1;}if(doc.surfaces.winds.size()!=base.surfaces.winds.size()+2){std::cerr<<"expected two new winds";return 1;}
std::string encoded;if(!world_studio::encode(doc,encoded,e)){std::cerr<<e;return 1;}
struct Point{const char *name;double x,y,z;};const Point points[]={
{"fire_source",-11988,12.4950001907,5172},{"smoke_mid",-11988,17.245,5172},{"smoke_top",-11988,22,5172},
{"camp_ground",-11988,11.8,5172},{"camp_canopy",-11988,32,5172},
{"overlap_canopy",-11971.5,27,5302.5},{"road_128m",-11965,12,5300},{"road_208m",-11965,14,5380},{"road_300m",-11960,14,5472},
{"road_taper_348m",-11960,14,5520},{"out_of_zone",-11500,12,5172}};
std::cout<<std::setprecision(10)<<"{\n\"passed\":true,\"baseline_wind_count\":"<<base.surfaces.winds.size()<<",\"candidate_wind_count\":"<<doc.surfaces.winds.size()<<",\"duration_seconds\":240,\"hz\":60,\"points\":[\n";
ExactProductionWindMethods production;production.data=doc.surfaces;double particle_error=0;
for(unsigned k=0;k<sizeof(points)/sizeof(*points);++k){auto p=points[k];double minimum=1e30,maximum=-1e30,mean=0;for(unsigned i=0;i<=14400;++i){double t=i/60.;auto w=world_surface::wind(doc.surfaces,{p.x,p.y,p.z},t,{0,0});float q[2]={0,0};production.particle_wind(&production.data,p.x,p.y,p.z,t,q);particle_error=std::max(particle_error,std::max(std::abs(double(w[0]-q[0])),std::abs(double(w[1]-q[1]))));double speed=std::hypot(w[0],w[1]);minimum=std::min(minimum,speed);maximum=std::max(maximum,speed);mean+=speed;}if(k)std::cout<<",\n";std::cout<<"{\"name\":"<<std::quoted(p.name)<<",\"position\":["<<p.x<<','<<p.y<<','<<p.z<<"],\"min_mps\":"<<minimum<<",\"max_mps\":"<<maximum<<",\"mean_mps\":"<<mean/14401<<'}';}
SDL_setenv("SDL_VIDEODRIVER","offscreen",1);SDL_setenv("LIBGL_ALWAYS_SOFTWARE","1",1);if(SDL_Init(SDL_INIT_VIDEO)){std::cerr<<SDL_GetError();return 1;}SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);auto *win=SDL_CreateWindow("wind proof",0,0,16,16,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);auto ctx=SDL_GL_CreateContext(win);const char *renderer=(const char*)glGetString(GL_RENDERER);if(!renderer||!std::strstr(renderer,"llvmpipe")){std::cerr<<"software guard";return 1;}
GLuint shader=compile(GL_VERTEX_SHADER,std::string("#version 330 core\nlayout(location=0)in vec3 position;out vec2 result;\n")+read(argv[3])+"\nvoid main(){result=studioWind(position,vec2(0));gl_Position=vec4(0,0,0,1);}\n");
GLuint prog=glCreateProgram();glAttachShader(prog,shader);const char *varying="result";glTransformFeedbackVaryings(prog,1,&varying,GL_INTERLEAVED_ATTRIBS);glLinkProgram(prog);GLint linked=0;glGetProgramiv(prog,GL_LINK_STATUS,&linked);if(!linked){char msg[4096];glGetProgramInfoLog(prog,sizeof(msg),nullptr,msg);std::cerr<<msg;return 1;}glUseProgram(prog);Program program{prog};GLuint vao=0,vbo=0,tf=0;glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);float xyz[3]={};glBufferData(GL_ARRAY_BUFFER,sizeof(xyz),xyz,GL_DYNAMIC_DRAW);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,nullptr);glEnableVertexAttribArray(0);glGenBuffers(1,&tf);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,tf);glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,2*sizeof(float),nullptr,GL_DYNAMIC_READ);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,tf);glEnable(GL_RASTERIZER_DISCARD);
double gpu_error=0;unsigned gpu_cases=0;for(auto p:points)for(unsigned i=0;i<120;++i){double t=i*2.003;production.wind_time=t;production.bind_wind(program);xyz[0]=float(p.x);xyz[1]=float(p.y);xyz[2]=float(p.z);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(xyz),xyz);glBeginTransformFeedback(GL_POINTS);glDrawArrays(GL_POINTS,0,1);glEndTransformFeedback();float result[2]={};glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,tf);glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,sizeof(result),result);auto expected=world_surface::wind(doc.surfaces,{double(xyz[0]),double(xyz[1]),double(xyz[2])},t,{0,0});for(unsigned j=0;j<2;j++){if(!std::isfinite(result[j]))return 1;gpu_error=std::max(gpu_error,std::abs(double(result[j]-expected[j])));}++gpu_cases;}
GLenum error=glGetError();std::cout<<"\n],\"particle_callback_max_difference_mps\":"<<particle_error<<",\"foliage_glsl_max_difference_mps\":"<<gpu_error<<",\"foliage_glsl_cases\":"<<gpu_cases<<",\"gl_errors\":"<<error<<",\"renderer\":"<<std::quoted(renderer)<<"}\n";return particle_error==0&&gpu_error<2e-6&&error==0?0:1;}

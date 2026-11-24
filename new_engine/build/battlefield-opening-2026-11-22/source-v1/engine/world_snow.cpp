#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_mixer.h>
#include "engine/world_snow.h"
#include "engine/world_nature_grandstone.h"
#include "engine/world_atlas.h"
#include "engine/world_program_epoch.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>
namespace {
constexpr unsigned print_cap=384,powder_cap=128;
struct Instance {float x=0,y=0,z=0,angle=0,born=0,size=1,side=0,extra=0;};
struct Powder {double x=0,y=0,z=0;float vx=0,vy=0,vz=0,born=-100,size=0;};
GLuint compile(GLenum kind,const std::string &path,std::string &error){
    std::ifstream in(path);std::ostringstream s;s<<in.rdbuf();auto source=s.str();if(!in||source.empty()){error="Missing snow shader: "+path;return 0;}
    GLuint id=glCreateShader(kind);const char *p=source.c_str();glShaderSource(id,1,&p,nullptr);glCompileShader(id);GLint ok=0;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[2048]={};glGetShaderInfoLog(id,sizeof log,nullptr,log);error=log;glDeleteShader(id);return 0;}return id;
}
float random(unsigned seed){seed^=seed>>16;seed*=0x7feb352d;seed^=seed>>15;seed*=0x846ca68b;seed^=seed>>16;return float(seed>>8)/16777216.f;}
}
struct WorldSnow {
    GLuint program=0,vao=0,buffer=0;GLint vp=-1,offset=-1,right=-1,up=-1,time_location=-1,mode=-1,sky=-1,sun=-1;
    std::array<Instance,print_cap> prints={};std::array<Powder,powder_cap> particles={};std::array<Instance,powder_cap> powder_gpu={};
    unsigned next_print=0,count=0,next_powder=0;double last_x=0,last_z=0,last_time=0,distance=0,now=0,ax=0,az=0;bool initialized=false;WorldSnowStats stats;
    Mix_Chunk *step=nullptr;std::array<int,2> channels={-1,-1};unsigned next_channel=0;
    static constexpr unsigned imprint_side=1024;static constexpr double imprint_width=32.;
    GLuint imprint=0;std::vector<float> heights;double imprint_x=0,imprint_z=0,imprint_time=-1e6;
    unsigned imprint_created=0;bool terrain_imprints=false,imprint_empty=true;
    struct Binding {unsigned program=0;unsigned long long epoch=0;GLint ready=-1,field=-1,bounds=-1;};std::array<Binding,4> bindings={};unsigned next_binding=0;
};
WorldSnow *world_snow_create(const char *shaders,const char *sound,std::string &error){
    error.clear();auto *w=new WorldSnow;
    GLuint vs=compile(GL_VERTEX_SHADER,std::string(shaders)+"/world_snow.vs.glsl",error),fs=compile(GL_FRAGMENT_SHADER,std::string(shaders)+"/world_snow.fs.glsl",error);
    if(!vs||!fs){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);world_snow_free(w);return nullptr;}
    w->program=glCreateProgram();glAttachShader(w->program,vs);glAttachShader(w->program,fs);glLinkProgram(w->program);glDeleteShader(vs);glDeleteShader(fs);GLint ok=0;glGetProgramiv(w->program,GL_LINK_STATUS,&ok);
    if(!ok){error="Snow shader link failed";world_snow_free(w);return nullptr;}
#define U(member,name) w->member=glGetUniformLocation(w->program,name)
    U(vp,"uVP");U(offset,"uOffset");U(right,"uRight");U(up,"uUp");U(time_location,"uTime");U(mode,"uMode");U(sky,"uSky");U(sun,"uSun");
#undef U
    glGenVertexArrays(1,&w->vao);glBindVertexArray(w->vao);glGenBuffers(1,&w->buffer);glBindBuffer(GL_ARRAY_BUFFER,w->buffer);glBufferData(GL_ARRAY_BUFFER,sizeof(w->prints),nullptr,GL_STREAM_DRAW);
    for(unsigned i=0;i<2;i++){glEnableVertexAttribArray(i);glVertexAttribPointer(i,4,GL_FLOAT,GL_FALSE,sizeof(Instance),reinterpret_cast<void*>(i*4*sizeof(float)));glVertexAttribDivisor(i,1);}
    glBindVertexArray(0);
    if(Mix_QuerySpec(nullptr,nullptr,nullptr))w->step=Mix_LoadWAV(sound);
    w->stats.audio_ready=w->step!=nullptr;return w;
}
namespace {
float smooth_edge(float a,float b,float x){x=std::clamp((x-a)/(b-a),0.f,1.f);return x*x*(3-2*x);}
void rebuild_imprints(WorldSnow &w,double x,double z){
    const double ox=std::floor(x/4)*4-16,oz=std::floor(z/4)*4-16;
    if(w.imprint&&w.imprint_empty&&!w.stats.footprints){w.imprint_x=ox;w.imprint_z=oz;return;}
    if(w.imprint_created==w.stats.created&&ox==w.imprint_x&&oz==w.imprint_z&&w.now-w.imprint_time<.25)return;
    GLint active=0;glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE0);
    if(!w.imprint){
        glGenTextures(1,&w.imprint);glBindTexture(GL_TEXTURE_2D,w.imprint);
        glTexImage2D(GL_TEXTURE_2D,0,GL_R16F,w.imprint_side,w.imprint_side,0,GL_RED,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        w.heights.resize(w.imprint_side*w.imprint_side);
    }
    w.imprint_x=ox;w.imprint_z=oz;w.imprint_time=w.now;w.imprint_created=w.stats.created;
    std::fill(w.heights.begin(),w.heights.end(),0.f);constexpr double pixel=w.imprint_width/w.imprint_side;
    for(unsigned i=0;i<w.count;i++){
        const auto &p=w.prints[i];const double age=w.now-p.born;if(age<0||age>=180)continue;
        const double px=w.ax+p.x,pz=w.az+p.z;const float fade=1-smooth_edge(90,180,float(age));
        const int ix=int(std::floor((px-ox)/pixel)),iz=int(std::floor((pz-oz)/pixel));
        const float c=std::cos(p.angle),s=std::sin(p.angle);
        for(int j=std::max(0,iz-9);j<=std::min(int(w.imprint_side)-1,iz+9);j++)for(int k=std::max(0,ix-9);k<=std::min(int(w.imprint_side)-1,ix+9);k++){
            const float dx=float(ox+(k+.5)*pixel-px),dz=float(oz+(j+.5)*pixel-pz);
            const float u=dx*c+dz*s,v=dx*s-dz*c;
            const float width=.083f+.014f*smooth_edge(-.05f,.12f,v)-.014f*std::exp(-v*v/.002f);
            const float qx=std::abs(u)-width+.035f,qy=std::abs(v)-.17f+.035f;
            const float d=std::hypot(std::max(qx,0.f),std::max(qy,0.f))+std::min(std::max(qx,qy),0.f)-.035f;
            const float inside=1-smooth_edge(-.025f,.018f,d);
            const float tread=.0025f*(.5f+.5f*std::cos(v*115))*inside;
            const float rim=.010f*std::exp(-std::pow((d-.025f)/.025f,2));
            const float h=(-.038f*inside+tread+rim)*fade;float &old=w.heights[unsigned(j)*w.imprint_side+unsigned(k)];
            old=h<0?std::min(old,h):old<0?old:std::max(old,h);
        }
    }
    w.stats.depressed_texels=0;w.stats.deepest_imprint=0;w.imprint_empty=true;
    for(float v:w.heights){w.stats.depressed_texels+=v<-.002f;w.stats.deepest_imprint=std::max(w.stats.deepest_imprint,-v);w.imprint_empty=w.imprint_empty&&v==0;}
    glBindTexture(GL_TEXTURE_2D,w.imprint);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,w.imprint_side,w.imprint_side,GL_RED,GL_FLOAT,w.heights.data());glActiveTexture(GLenum(active));w.stats.imprint_updates++;
}
}
void world_snow_update(WorldSnow *w,double time,double x,double feet,double z,bool grounded,double district_x,double district_z){
    if(!w||!std::isfinite(time)||!std::isfinite(x)||!std::isfinite(z)||!std::isfinite(feet))return;
    w->now=time;
    if(!w->initialized){w->last_x=x;w->last_z=z;w->last_time=time;w->ax=std::floor(district_x/128)*128;w->az=std::floor(district_z/128)*128;w->initialized=true;}
    const double dx=x-w->last_x,dz=z-w->last_z,travel=std::hypot(dx,dz),dt=std::max(.0001,time-w->last_time);
    const bool snow=grounded&&z>district_z+14&&z<district_z+500&&std::abs(x-district_x)<500&&!world_snow_frozen_basin(x-district_x,z-district_z)&&world_nature_grandstone_roof(x-district_x,z-district_z)<=0&&std::abs(feet-world_atlas_height(x,z))<.18;
    if(snow&&travel>.00001&&travel<2&&dt<.3){
        w->distance+=travel;const float speed=float(travel/dt),stride=speed>4.1f?.76f:.53f;
        if(w->distance>=stride){
            w->distance=std::fmod(w->distance,stride);const bool left=(w->stats.created%2)==0;const float side=left?-1.f:1.f,angle=float(std::atan2(dx,-dz));
            const double px=x+std::cos(angle)*side*.145,pz=z+std::sin(angle)*side*.145;const float py=world_atlas_height(px,pz)+.03f;
            const float slope_x=world_atlas_height(px+.25,pz)-world_atlas_height(px-.25,pz);
            const float slope_z=world_atlas_height(px,pz+.25)-world_atlas_height(px,pz-.25);
            auto &p=w->prints[w->next_print];p={float(px-w->ax),py,float(pz-w->az),angle,float(time),slope_x*2,side,slope_z*2};w->next_print=(w->next_print+1)%print_cap;w->count=std::min(w->count+1,print_cap);
            w->stats.created++;if(left)w->stats.left++;else w->stats.right++;
            for(unsigned i=0;i<6;i++){
                const unsigned seed=w->stats.created*17+i;auto &a=w->particles[w->next_powder];w->next_powder=(w->next_powder+1)%powder_cap;
                const float lateral=(random(seed)-.5f)*1.15f;
                a={px,py+.025,pz,float(dx/travel)*speed*.12f+std::cos(angle)*lateral,.45f+random(seed+57)*.75f,float(dz/travel)*speed*.12f+std::sin(angle)*lateral,float(time),.065f+random(seed+71)*.09f};w->stats.puffs++;
            }
            if(w->step){const int ch=Mix_PlayChannel(-1,w->step,0);if(ch>=0){Mix_Volume(ch,speed>4.1f?44:32);w->channels[w->next_channel++%2]=ch;w->stats.audio_steps++;}}
        }
    }else if(!snow||travel>=2)w->distance=0;
    w->last_x=x;w->last_z=z;w->last_time=time;w->stats.footprints=0;w->stats.powder=0;
    for(unsigned i=0;i<w->count;i++)if(time-w->prints[i].born<180)w->stats.footprints++;
    for(const auto &a:w->particles)if(time-a.born>=0&&time-a.born<.95)w->stats.powder++;
    rebuild_imprints(*w,x,z);
}
void world_snow_bind_terrain(WorldSnow *w,unsigned program){
    if(!w||!program)return;
    WorldSnow::Binding *binding=nullptr;
    for(auto &b:w->bindings)if(b.program==program&&b.epoch==world_program_epoch){binding=&b;break;}
    if(!binding){binding=&w->bindings[w->next_binding++%w->bindings.size()];binding->program=program;binding->epoch=world_program_epoch;binding->ready=glGetUniformLocation(program,"uSnowImprintsReady");binding->field=glGetUniformLocation(program,"uSnowImprints");binding->bounds=glGetUniformLocation(program,"uSnowImprintBounds");}
    if(binding->ready<0)return;
    glUniform1i(binding->ready,w->imprint!=0);glUniform1i(binding->field,16);
    glUniform4f(binding->bounds,float(w->imprint_x-w->ax),float(w->imprint_z-w->az),float(w->ax),float(w->az));
    glActiveTexture(GL_TEXTURE0+16);glBindTexture(GL_TEXTURE_2D,w->imprint);w->terrain_imprints=true;
}
void world_snow_draw(WorldSnow *w,const float *vp,double x,double y,double z,const float *right,const float *up,const float *sky,const float *sun){
    if(!w||!w->initialized||!vp)return;
    glUseProgram(w->program);glUniformMatrix4fv(w->vp,1,GL_FALSE,vp);glUniform3f(w->offset,float(w->ax-x),float(-y),float(w->az-z));glUniform3fv(w->right,1,right);glUniform3fv(w->up,1,up);glUniform3fv(w->sky,1,sky);glUniform3fv(w->sun,1,sun);glUniform1f(w->time_location,float(w->now));
    glBindVertexArray(w->vao);glBindBuffer(GL_ARRAY_BUFFER,w->buffer);glDrawBuffer(GL_COLOR_ATTACHMENT0);glEnable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ZERO,GL_ONE_MINUS_SRC_ALPHA);
    if(w->count&&!w->terrain_imprints){glUniform1i(w->mode,0);glBufferSubData(GL_ARRAY_BUFFER,0,w->count*sizeof(Instance),w->prints.data());glDrawArraysInstanced(GL_TRIANGLES,0,6,w->count);w->stats.draws++;w->stats.uploads++;}
    unsigned n=0;
    for(const auto &a:w->particles){
        const float age=float(w->now-a.born);if(age<0||age>=.95)continue;
        auto &p=w->powder_gpu[n++];p={float(a.x+a.vx*age-w->ax),float(a.y+a.vy*age-.65*age*age),float(a.z+a.vz*age-w->az),0,a.born,a.size*(1+age*2),0,0};
    }
    if(n){glUniform1i(w->mode,1);glBufferSubData(GL_ARRAY_BUFFER,0,n*sizeof(Instance),w->powder_gpu.data());glDrawArraysInstanced(GL_TRIANGLES,0,6,n);w->stats.draws++;w->stats.uploads++;}
    glDisable(GL_BLEND);glDepthMask(GL_TRUE);glEnable(GL_CULL_FACE);glBindVertexArray(0);
}
WorldSnowStats world_snow_stats(const WorldSnow *w){return w?w->stats:WorldSnowStats{};}
void world_snow_free(WorldSnow *w){if(!w)return;for(int ch:w->channels)if(ch>=0&&Mix_GetChunk(ch)==w->step)Mix_HaltChannel(ch);if(w->step)Mix_FreeChunk(w->step);if(w->imprint)glDeleteTextures(1,&w->imprint);if(w->buffer)glDeleteBuffers(1,&w->buffer);if(w->vao)glDeleteVertexArrays(1,&w->vao);if(w->program)glDeleteProgram(w->program);delete w;}

#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_weather.h"
#include "engine/world_atlas.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <new>
#include <sstream>
#include <string>

namespace {
constexpr unsigned side=65,rain_cap=4096,splash_cap=768;
constexpr double cache_span=192,cache_step=3;
GLuint shader(GLenum type,const std::string &path){
    std::ifstream input(path,std::ios::binary);
    if(!input){std::fprintf(stderr,"[Weather] Cannot read shader: %s\n",path.c_str());return 0;}
    std::ostringstream stream;stream<<input.rdbuf();std::string source=stream.str();
    if(source.empty()||source.size()>1024*1024){
        std::fprintf(stderr,"[Weather] Empty or oversized shader: %s (%zu bytes)\n",path.c_str(),source.size());return 0;
    }
    GLuint id=glCreateShader(type);const char *p=source.c_str();glShaderSource(id,1,&p,nullptr);glCompileShader(id);
    GLint ok=0;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[4096]={};glGetShaderInfoLog(id,sizeof(log),nullptr,log);std::fprintf(stderr,"[Weather] %s: %s\n",path.c_str(),log);glDeleteShader(id);return 0;}
    return id;
}
struct Program {
    GLuint id=0;
    GLint resolution=-1,half_size=-1,near_far=-1,tan_fov=-1,jitter=-1,right=-1,up=-1,forward=-1,camera=-1;
    GLint sun=-1,sun_color=-1,fog=-1,sky=-1,ground=-1,light_vp=-1,world_size=-1,time=-1,rain=-1,fog_strength=-1,wind=-1;
    GLint local_bounds=-1,has_terrain=-1,has_macro=-1,has_shadow=-1,mode=-1,rain_cell=-1,rain_origin=-1,splash_cell=-1,splash_origin=-1;
    GLint pool_bounds=-1,pool_level=-1,has_cover=-1;
    bool init(const std::string &vertex,const std::string &fragment){
        GLuint vs=shader(GL_VERTEX_SHADER,vertex),fs=shader(GL_FRAGMENT_SHADER,fragment);
        if(!vs||!fs){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);return false;}
        id=glCreateProgram();glAttachShader(id,vs);glAttachShader(id,fs);glLinkProgram(id);glDeleteShader(vs);glDeleteShader(fs);
        GLint ok=0;glGetProgramiv(id,GL_LINK_STATUS,&ok);
        if(!ok){char log[4096]={};glGetProgramInfoLog(id,sizeof(log),nullptr,log);std::fprintf(stderr,"[Weather] link: %s\n",log);return false;}
#define U(member,name) member=glGetUniformLocation(id,name)
        U(resolution,"uResolution");U(half_size,"uHalfSize");U(near_far,"uNearFar");U(tan_fov,"uTan");U(jitter,"uJitterNDC");
        U(right,"uRight");U(up,"uUp");U(forward,"uForward");U(camera,"uCamera");U(sun,"uSun");U(sun_color,"uSunColor");
        U(fog,"uFogColor");U(sky,"uSkyLight");U(ground,"uGroundLight");U(light_vp,"uLightVP");U(world_size,"uWorldSize");
        U(time,"uTime");U(rain,"uRain");U(fog_strength,"uFogStrength");U(wind,"uWind");U(local_bounds,"uLocalBounds");
        U(has_terrain,"uHasTerrain");U(has_macro,"uHasMacro");U(has_shadow,"uHasShadow");U(mode,"uMode");
        U(rain_cell,"uRainCell");U(rain_origin,"uRainOrigin");U(splash_cell,"uSplashCell");U(splash_origin,"uSplashOrigin");
        U(pool_bounds,"uPoolBounds");U(pool_level,"uPoolLevel");
        U(has_cover,"uHasCover");
#undef U
        glUseProgram(id);
        const char *samplers[]={"uDepth","uColor","uShadow","uMacroHeight","uTerrain","uFogField"};
        for(int i=0;i<6;i++)glUniform1i(glGetUniformLocation(id,samplers[i]),i);
        return true;
    }
    void frame(const WorldWeatherFrame &f,int hw,int hh,bool cached,double cx,double cz){
        glUseProgram(id);glUniform2f(resolution,float(f.width),float(f.height));glUniform2f(half_size,float(hw),float(hh));
        glUniform2f(near_far,f.near_plane,f.far_plane);glUniform1f(tan_fov,f.tan_half_fov);glUniform2fv(jitter,1,f.jitter_ndc);
        glUniform3fv(right,1,f.right);glUniform3fv(up,1,f.up);glUniform3fv(forward,1,f.forward);
        glUniform3f(camera,float(f.camera[0]),float(f.camera[1]),float(f.camera[2]));
        glUniform3fv(sun,1,f.sun);glUniform3fv(sun_color,1,f.sun_color);glUniform3fv(fog,1,f.fog);
        glUniform3fv(sky,1,f.sky_light);glUniform3fv(ground,1,f.ground_light);glUniformMatrix4fv(light_vp,1,GL_FALSE,f.light_vp);
        glUniform2fv(world_size,1,f.world_size);glUniform1f(time,f.time);glUniform1f(rain,std::clamp(f.intensity,0.f,1.f));
        glUniform1f(fog_strength,std::clamp(f.fog_strength,0.f,1.5f));glUniform2fv(wind,1,f.wind_xz);
        glUniform4fv(pool_bounds,1,f.pool_bounds);glUniform1f(pool_level,f.pool_level);
        glUniform4f(local_bounds,float(cx),float(cz),float(1/cache_span),float(1/cache_span));
        glUniform1i(has_terrain,cached);glUniform1i(has_macro,f.macro_height!=0);glUniform1i(has_shadow,f.shadow!=0);
        glUniform1i(has_cover,f.cover_height!=nullptr);
        const double drift_x=double(f.wind_xz[0])*f.time,drift_z=double(f.wind_xz[1])*f.time;
        double rx=std::floor((f.camera[0]-drift_x)/6)-8,rz=std::floor((f.camera[2]-drift_z)/6)-8;
        double sx=std::floor(f.camera[0]/3)-16,sz=std::floor(f.camera[2]/3)-12;
        glUniform2i(rain_cell,GLint(rx),GLint(rz));glUniform2f(rain_origin,float(rx*6+drift_x-f.camera[0]),float(rz*6+drift_z-f.camera[2]));
        glUniform2i(splash_cell,GLint(sx),GLint(sz));glUniform2f(splash_origin,float(sx*3-f.camera[0]),float(sz*3-f.camera[2]));
    }
};
void bind(unsigned unit,GLuint texture){glActiveTexture(GL_TEXTURE0+unit);glBindTexture(GL_TEXTURE_2D,texture);}
bool active(const WorldWeatherFrame &f){return f.intensity>0||f.fog_strength>0;}
bool camera_valid(const WorldWeatherFrame &f){
    for(double a:f.camera)if(!std::isfinite(a)||std::abs(a)>1.e8)return false;
    return std::isfinite(f.intensity)&&std::isfinite(f.fog_strength);
}
bool valid(const WorldWeatherFrame &f){
    if(!camera_valid(f)||!f.color||!f.depth||f.width<1||f.height<1||f.width>8192||f.height>8192||
       std::uint64_t(f.width)*f.height>4096ull*4096ull||!std::isfinite(f.near_plane)||!std::isfinite(f.far_plane)||
       f.near_plane<=0||f.far_plane<=f.near_plane||!std::isfinite(f.tan_half_fov)||f.tan_half_fov<=0||f.tan_half_fov>4||
       !std::isfinite(f.time)||std::abs(f.time)>1.e7f)return false;
    const float *vectors[]={f.right,f.up,f.forward,f.sun,f.sun_color,f.fog,f.sky_light,f.ground_light};
    for(const float *p:vectors)for(int i=0;i<3;i++)if(!std::isfinite(p[i]))return false;
    const float *basis[]={f.right,f.up,f.forward};
    for(int i=0;i<3;i++){
        float length=0;for(int k=0;k<3;k++)length+=basis[i][k]*basis[i][k];
        if(std::abs(length-1)>.05f)return false;
        for(int j=i+1;j<3;j++){
            float dot=0;for(int k=0;k<3;k++)dot+=basis[i][k]*basis[j][k];if(std::abs(dot)>.05f)return false;
        }
    }
    for(float a:f.light_vp)if(!std::isfinite(a))return false;
    for(float a:f.jitter_ndc)if(!std::isfinite(a)||std::abs(a)>.1f)return false;
    for(float a:f.world_size)if(!std::isfinite(a)||a<=0)return false;
    for(float a:f.wind_xz)if(!std::isfinite(a)||std::abs(a)>30)return false;
    for(float a:f.pool_bounds)if(!std::isfinite(a))return false;
    if(!std::isfinite(f.pool_level))return false;
    return true;
}
}
struct WorldWeather {
    Program fog,compose,particles;
    GLuint vao=0,fbo[2]={},texture[2]={},terrain=0;
    std::array<float,side*side*2> heights={};
    double cache_x=0,cache_z=0,pending_x=0,pending_z=0;
    unsigned next_row=0;bool building=false,ready=false;
    float (*height_callback)(double,double,void*)=nullptr;void *height_userdata=nullptr;
    float (*cover_callback)(double,double,void*)=nullptr;void *cover_userdata=nullptr;
    WorldWeatherStats stats;
    void release_targets(){
        glDeleteFramebuffers(2,fbo);glDeleteTextures(2,texture);
        for(int i=0;i<2;i++)fbo[i]=texture[i]=0;
        stats.width=stats.height=stats.half_width=stats.half_height=0;stats.bytes=terrain?side*side*8:0;
    }
    bool resize(int w,int h){
        if(stats.width==w&&stats.height==h)return true;
        release_targets();int hw=(w+1)/2,hh=(h+1)/2;
        glGenFramebuffers(2,fbo);glGenTextures(2,texture);
        for(int i=0;i<2;i++){
            glBindFramebuffer(GL_FRAMEBUFFER,fbo[i]);bind(5,texture[i]);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,i?w:hw,i?h:hh,0,GL_RGBA,GL_FLOAT,nullptr);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture[i],0);
            if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){release_targets();return false;}
        }
        if(!terrain){
            glGenTextures(1,&terrain);bind(4,terrain);glTexImage2D(GL_TEXTURE_2D,0,GL_RG32F,side,side,0,GL_RG,GL_FLOAT,nullptr);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        }
        stats.width=w;stats.height=h;stats.half_width=hw;stats.half_height=hh;
        stats.bytes=(std::uint64_t(w)*h+std::uint64_t(hw)*hh)*8+side*side*8;stats.resizes++;return true;
    }
};
WorldWeather *world_weather_create(const char *directory){
    if(!directory||!directory[0]||!SDL_GL_GetCurrentContext())return nullptr;
    auto *w=new(std::nothrow) WorldWeather;if(!w)return nullptr;
    const std::string root=std::string(directory)+"/world_weather_";
    if(!w->fog.init(root+"fullscreen.vs.glsl",root+"fog.fs.glsl")||
       !w->compose.init(root+"fullscreen.vs.glsl",root+"compose.fs.glsl")||
       !w->particles.init(root+"particles.vs.glsl",root+"particles.fs.glsl")){
        world_weather_free(w);return nullptr;
    }
    glGenVertexArrays(1,&w->vao);return w;
}
void world_weather_invalidate_surface(WorldWeather *w){if(!w)return;w->stats.terrain_cached=false;w->building=w->ready=false;w->next_row=0;}
void world_weather_update(WorldWeather *w,const WorldWeatherFrame &f,unsigned budget){
    if(!w)return;
    w->stats.terrain_rows_this_update=0;
    if(!camera_valid(f)||!active(f)||!budget)return;
    budget=std::min(budget,8u);
    const double x=std::floor(f.camera[0]/32)*32-cache_span*.5,z=std::floor(f.camera[2]/32)*32-cache_span*.5;
    bool callback_changed=w->height_callback!=f.surface_height||w->height_userdata!=f.height_userdata||
                          w->cover_callback!=f.cover_height||w->cover_userdata!=f.cover_userdata;
    if(callback_changed){w->stats.terrain_cached=false;w->ready=false;}
    if(!callback_changed&&!w->building&&!w->ready&&w->stats.terrain_cached&&x==w->cache_x&&z==w->cache_z)return;
    if(callback_changed||(!w->building&&!w->ready)||(x!=w->pending_x||z!=w->pending_z)){
        w->pending_x=x;w->pending_z=z;w->next_row=0;w->building=true;w->ready=false;
        w->height_callback=f.surface_height;w->height_userdata=f.height_userdata;
        w->cover_callback=f.cover_height;w->cover_userdata=f.cover_userdata;
    }
    if(!w->building)return;
    while(budget--&&w->next_row<side){
        for(unsigned ix=0;ix<side;ix++){
            const double wx=x+ix*cache_step,wz=z+w->next_row*cache_step;
            float height=f.surface_height?f.surface_height(wx,wz,f.height_userdata):world_atlas_height(wx,wz);
            if(!std::isfinite(height))height=world_atlas_height(wx,wz);
            height=std::isfinite(height)?std::clamp(height,-1000.f,6000.f):0;
            float cap=height;
            if(f.cover_height){
                const float cover=f.cover_height(wx,wz,f.cover_userdata);
                if(std::isfinite(cover))cap=std::max(height,std::clamp(cover,-1000.f,6000.f));
                w->stats.cover_queries++;
            }
            const unsigned offset=(w->next_row*side+ix)*2;
            w->heights[offset]=height;w->heights[offset+1]=cap;
            w->stats.height_queries++;
        }
        w->next_row++;w->stats.terrain_rows_this_update++;
    }
    if(w->next_row==side){w->building=false;w->ready=true;}
}
unsigned world_weather_resolve(WorldWeather *w,const WorldWeatherFrame &f){
    if(!w)return f.color;
    w->stats.rain_instances=w->stats.splash_instances=0;
    if(!active(f)||!valid(f))return f.color;
    for(GLuint owned:{w->texture[0],w->texture[1],w->terrain})
        if(owned&&(f.color==owned||f.depth==owned||f.shadow==owned||f.macro_height==owned))return f.color;
    glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_FALSE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    if(!w->resize(f.width,f.height))return f.color;
    bind(4,w->terrain);
    if(w->ready){
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,side,side,GL_RG,GL_FLOAT,w->heights.data());
        w->cache_x=w->pending_x;w->cache_z=w->pending_z;w->stats.terrain_cached=true;w->ready=false;w->stats.cache_generations++;
    }
    bind(0,f.depth);bind(1,f.color);bind(2,f.shadow);bind(3,f.macro_height);glBindVertexArray(w->vao);
    if(f.fog_strength>0){
        glBindFramebuffer(GL_FRAMEBUFFER,w->fbo[0]);glViewport(0,0,w->stats.half_width,w->stats.half_height);
        w->fog.frame(f,w->stats.half_width,w->stats.half_height,w->stats.terrain_cached,w->cache_x,w->cache_z);glDrawArrays(GL_TRIANGLES,0,3);
    }
    bind(5,w->texture[0]);glBindFramebuffer(GL_FRAMEBUFFER,w->fbo[1]);glViewport(0,0,f.width,f.height);
    w->compose.frame(f,w->stats.half_width,w->stats.half_height,w->stats.terrain_cached,w->cache_x,w->cache_z);glDrawArrays(GL_TRIANGLES,0,3);
    if(f.intensity>0){
        w->particles.frame(f,w->stats.half_width,w->stats.half_height,w->stats.terrain_cached,w->cache_x,w->cache_z);
        glEnable(GL_BLEND);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
        // Alpha is temporal stability, not transparency. Even a faint moving
        // particle must invalidate history; untouched pixels retain the scene.
        glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ZERO,GL_ZERO);
        glUniform1i(w->particles.mode,0);glDrawArraysInstanced(GL_TRIANGLES,0,6,rain_cap);
        glUniform1i(w->particles.mode,1);glDrawArraysInstanced(GL_TRIANGLES,0,18,splash_cap);
        w->stats.rain_instances=rain_cap;w->stats.splash_instances=splash_cap;glDisable(GL_BLEND);
    }
    w->stats.resolves++;return w->texture[1];
}
WorldWeatherStats world_weather_stats(const WorldWeather *w){return w?w->stats:WorldWeatherStats{};}
unsigned world_weather_fog_field(const WorldWeather *w){return w?w->texture[0]:0;}
void world_weather_free(WorldWeather *w){
    if(!w)return;
    w->release_targets();glDeleteTextures(1,&w->terrain);glDeleteVertexArrays(1,&w->vao);
    glDeleteProgram(w->fog.id);glDeleteProgram(w->compose.id);glDeleteProgram(w->particles.id);delete w;
}

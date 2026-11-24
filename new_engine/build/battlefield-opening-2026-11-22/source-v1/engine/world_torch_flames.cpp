#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_torch_flames.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <new>
#include <sstream>
#include <string>
#include <vector>
#include <cerrno>
#include <cstring>

namespace {
constexpr unsigned fire_layers=4,ember_layers=8,layers=fire_layers+ember_layers;
constexpr double height_factor=3.2;
using Instance=std::array<float,8>;
struct EffectBounds {double center_y=0,horizontal=0,radius=0,max_y=0;};
EffectBounds effect_bounds(const WorldTorchFlameEmitter &e,const WorldTorchFlameFrame &f) {
    const double wind=std::min(3.,std::hypot(double(f.wind_xz[0]),double(f.wind_xz[1])));
    EffectBounds b;
    b.center_y=e.y+height_factor*.5*e.height;b.max_y=e.y+height_factor*e.height;
    b.horizontal=2.*e.width+.12*e.height+wind*.65*e.height+.05;
    b.radius=std::hypot(b.horizontal,height_factor*.5*e.height);return b;
}
bool emitters_valid(const WorldTorchFlameEmitter *emitters,std::size_t count) {
    if(count>WORLD_TORCH_FLAME_CAPACITY||(count&&!emitters))return false;
    for(std::size_t i=0;i<count;i++) {
        const auto &e=emitters[i];
        if(!std::isfinite(e.x)||!std::isfinite(e.y)||!std::isfinite(e.z)||
           std::abs(e.x)>1.e8||std::abs(e.y)>1.e6||std::abs(e.z)>1.e8||
           !std::isfinite(e.height)||e.height<.05f||e.height>4.f||
           !std::isfinite(e.width)||e.width<.02f||e.width>2.f||
           !std::isfinite(e.intensity)||e.intensity<0.f||e.intensity>8.f)return false;
    }
    return true;
}
bool frame_valid(const WorldTorchFlameFrame &f) {
    if(!f.framebuffer||f.width<1||f.height<1||f.width>8192||f.height>8192||
       std::uint64_t(f.width)*f.height>4096ull*4096ull||!std::isfinite(f.time)||
       !std::isfinite(f.haze)||f.haze<0||f.haze>1||!std::isfinite(f.clip_height)||
       !std::isfinite(f.tan_half_fov)||f.tan_half_fov<.05f||f.tan_half_fov>4)return false;
    for(double c:f.camera)if(!std::isfinite(c)||std::abs(c)>1.e8)return false;
    bool nonzero=false;
    for(float a:f.vp){if(!std::isfinite(a))return false;nonzero=nonzero||a!=0;}
    if(!nonzero)return false;
    for(float a:f.wind_xz)if(!std::isfinite(a)||std::abs(a)>30.f)return false;
    const float *basis[]={f.right,f.up,f.forward};
    for(unsigned i=0;i<3;i++) {
        double length=0;
        for(unsigned k=0;k<3;k++){if(!std::isfinite(basis[i][k]))return false;length+=basis[i][k]*basis[i][k];}
        if(std::abs(length-1.)>.03)return false;
        for(unsigned j=0;j<i;j++) {
            double dot=0;for(unsigned k=0;k<3;k++)dot+=basis[i][k]*basis[j][k];
            if(std::abs(dot)>.03)return false;
        }
    }
    return true;
}
GLuint compile(GLenum type,const std::string &path) {
    std::ifstream file(path,std::ios::binary);
    if(!file){std::fprintf(stderr,"[TorchFlames] Cannot read %s\n",path.c_str());return 0;}
    std::ostringstream out;out<<file.rdbuf();const std::string source=out.str();
    if(source.empty()||source.size()>1024*1024)return 0;
    GLuint id=glCreateShader(type);const char *text=source.c_str();glShaderSource(id,1,&text,nullptr);glCompileShader(id);
    GLint ok=0;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[4096]={};glGetShaderInfoLog(id,sizeof(log),nullptr,log);std::fprintf(stderr,"[TorchFlames] %s: %s\n",path.c_str(),log);glDeleteShader(id);return 0;}
    return id;
}
std::size_t prepare(const std::array<WorldTorchFlameEmitter,WORLD_TORCH_FLAME_CAPACITY> &emitters,
                    std::size_t count,const WorldTorchFlameFrame &f,
                    std::array<Instance,WORLD_TORCH_FLAME_CAPACITY> &staging) {
    const double tv=f.tan_half_fov,th=tv*f.width/f.height;
    const double ph=std::hypot(1.,th),pv=std::hypot(1.,tv);
    std::size_t visible=0;
    for(std::size_t i=0;i<count;i++) {
        const auto &e=emitters[i];if(e.intensity<=0)continue;
        const double x=e.x-f.camera[0],y=e.y-f.camera[1],z=e.z-f.camera[2];
        const auto bounds=effect_bounds(e,f);
        const double center_y=bounds.center_y-f.camera[1],radius=bounds.radius;
        if(x*x+center_y*center_y+z*z>(300.+radius)*(300.+radius))continue;
        if(f.clip_below&&bounds.max_y<f.clip_height)continue;
        const double along=x*f.forward[0]+center_y*f.forward[1]+z*f.forward[2];
        const double across=x*f.right[0]+center_y*f.right[1]+z*f.right[2];
        const double up=x*f.up[0]+center_y*f.up[1]+z*f.up[2];
        if(along< -radius||std::abs(across)>std::max(0.,along)*th+radius*ph||
           std::abs(up)>std::max(0.,along)*tv+radius*pv)continue;
        staging[visible++]={float(x),float(y),float(z),e.height,e.width,e.intensity,
            world_torch_seed_phase(e.seed),world_torch_flicker(f.time,e.seed)};
    }
    return visible;
}
}
struct WorldTorchFlames {
    GLuint program=0,vao=0,instances=0,sprites=0;double preview_time=-1;
    GLint sprite_sampler=-1,use_sprites=-1;
    GLint vp=-1,right=-1,forward=-1,time=-1,pixel_scale=-1,eye_y=-1,haze=-1,clip=-1,clip_height=-1,wind=-1;
    std::array<WorldTorchFlameEmitter,WORLD_TORCH_FLAME_CAPACITY> emitters={};
    std::array<Instance,WORLD_TORCH_FLAME_CAPACITY> staging={};
    std::size_t count=0;
    WorldTorchFlameStats stats;
};
WorldTorchFlames *world_torch_flames_create(const char *directory) {
    if(!directory||!*directory||!SDL_GL_GetCurrentContext())return nullptr;
    auto *flames=new(std::nothrow) WorldTorchFlames;if(!flames)return nullptr;
    const std::string prefix=std::string(directory)+"/world_torch_flame";
    GLuint vs=compile(GL_VERTEX_SHADER,prefix+".vs.glsl"),fs=compile(GL_FRAGMENT_SHADER,prefix+".fs.glsl");
    if(!vs||!fs){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);delete flames;return nullptr;}
    flames->program=glCreateProgram();glAttachShader(flames->program,vs);glAttachShader(flames->program,fs);
    glLinkProgram(flames->program);glDeleteShader(vs);glDeleteShader(fs);
    GLint linked=0;glGetProgramiv(flames->program,GL_LINK_STATUS,&linked);
    if(!linked){char log[4096]={};glGetProgramInfoLog(flames->program,sizeof(log),nullptr,log);std::fprintf(stderr,"[TorchFlames] Link: %s\n",log);world_torch_flames_free(flames);return nullptr;}
#define U(member,name) flames->member=glGetUniformLocation(flames->program,name)
    U(vp,"uVP");U(right,"uRight");U(forward,"uForward");U(time,"uTimePhase");
    U(pixel_scale,"uPixelScale");U(eye_y,"uEyeY");U(haze,"uHaze");U(clip,"uClipBelow");U(clip_height,"uClipHeight");
    U(wind,"uWind");U(sprite_sampler,"uSprites");U(use_sprites,"uUseSprites");
#undef U
    glGenVertexArrays(1,&flames->vao);glGenBuffers(1,&flames->instances);
    glBindVertexArray(flames->vao);glBindBuffer(GL_ARRAY_BUFFER,flames->instances);
    glBufferData(GL_ARRAY_BUFFER,sizeof(flames->staging),nullptr,GL_STREAM_DRAW);
    for(unsigned i=0;i<2;i++) {
        glEnableVertexAttribArray(i);glVertexAttribPointer(i,4,GL_FLOAT,GL_FALSE,sizeof(Instance),reinterpret_cast<void*>(std::uintptr_t(i*4*sizeof(float))));
        glVertexAttribDivisor(i,layers);
    }
    glBindVertexArray(0);flames->stats.gpu_buffer_bytes=sizeof(flames->staging);return flames;
}
bool world_torch_flames_load_sprites(WorldTorchFlames *p,const char *path) {
    if(!p||!path)return false;
    std::ifstream file(path,std::ios::binary);std::array<std::uint32_t,4> h={};
    file.read(reinterpret_cast<char*>(h.data()),sizeof(h));
    if(!file||h!=std::array<std::uint32_t,4>{0x31584656u,128,256,144})return false;
    std::vector<unsigned char> pixels(size_t(h[1])*h[2]*h[3]*4);
    file.read(reinterpret_cast<char*>(pixels.data()),pixels.size());
    if(!file||file.peek()!=std::char_traits<char>::eof())return false;
    GLuint texture=0;glGenTextures(1,&texture);if(!texture)return false;
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D_ARRAY,texture);
    glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_SRGB8_ALPHA8,h[1],h[2],h[3],0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    if(glGetError()!=GL_NO_ERROR){glDeleteTextures(1,&texture);return false;}
    glDeleteTextures(1,&p->sprites);p->sprites=texture;p->stats.purchased_sprites=true;
    p->stats.sprite_bytes=0;for(unsigned w=128,hg=256;;w=std::max(1u,w/2),hg=std::max(1u,hg/2)) {
        p->stats.sprite_bytes+=std::uint64_t(w)*hg*144*4;if(w==1&&hg==1)break;
    }
    return true;
}
bool world_torch_flames_validate_emitters(const WorldTorchFlameEmitter *e,std::size_t n){return emitters_valid(e,n);}
WorldTorchFlameFileStatus world_torch_flames_read_file(const char *path,
        std::vector<WorldTorchFlameEmitter> &output,std::string &error,std::size_t maximum){
    error.clear();
    if(!path||!*path||maximum>WORLD_TORCH_FLAME_CAPACITY){error="Invalid source flame path/capacity";return WorldTorchFlameFileStatus::Invalid;}
    FILE *file=std::fopen(path,"r");
    if(!file){if(errno==ENOENT)return WorldTorchFlameFileStatus::Missing;error="Cannot open source flame file";return WorldTorchFlameFileStatus::Invalid;}
    std::vector<WorldTorchFlameEmitter> candidate;candidate.reserve(maximum);
    char line[512];unsigned row=0;bool header=false,ok=true;
    while(std::fgets(line,sizeof line,file)){
        ++row;if(row>4096||(!std::strchr(line,'\n')&&!std::feof(file))){ok=false;break;}
        const char *text=line;while(*text==' '||*text=='\t'||*text=='\r'||*text=='\n')++text;
        if(!*text||*text=='#')continue;
        std::istringstream input(text);std::string trailing;
        if(!header){std::string magic;unsigned version=0;if(!(input>>magic>>version)||magic!="VALWOOD_SOURCE_FLAMES"||version!=1||(input>>trailing)){ok=false;break;}header=true;continue;}
        WorldTorchFlameEmitter emitter;unsigned long long seed=0;
        if(candidate.size()>=maximum||!(input>>emitter.x>>emitter.y>>emitter.z>>emitter.height>>emitter.width>>emitter.intensity>>seed)||(input>>trailing)||seed>UINT32_MAX){ok=false;break;}
        emitter.seed=std::uint32_t(seed);
        if(!emitters_valid(&emitter,1)||std::abs(emitter.x)>50400||std::abs(emitter.z)>28400||emitter.y< -2000||emitter.y>12000){ok=false;break;}
        candidate.push_back(emitter);
    }
    if(std::ferror(file))ok=false;
    std::fclose(file);
    if(!header||!ok){error="Rejected source flame file at line "+std::to_string(row);return WorldTorchFlameFileStatus::Invalid;}
    output=std::move(candidate);return WorldTorchFlameFileStatus::Loaded;
}
bool world_torch_flames_set_emitters(WorldTorchFlames *flames,const WorldTorchFlameEmitter *emitters,std::size_t count) {
    if(!flames||!emitters_valid(emitters,count))return false;
    if(count)std::copy_n(emitters,count,flames->emitters.begin());
    flames->count=count;flames->stats.emitters=unsigned(count);flames->stats.visible_emitters=flames->stats.flame_slices=flames->stats.sparks=0;
    return true;
}
void world_torch_flames_preview_time(WorldTorchFlames *p,double time){if(p&&std::isfinite(time))p->preview_time=time;}
bool world_torch_flames_draw(WorldTorchFlames *flames,const WorldTorchFlameFrame &input) {
    auto f=input;if(flames&&flames->preview_time>=0)f.time=flames->preview_time;
    if(!flames||!frame_valid(f))return false;
    flames->stats.visible_emitters=flames->stats.flame_slices=flames->stats.sparks=0;
    const std::size_t count=prepare(flames->emitters,flames->count,f,flames->staging);
    if(!count)return true;
    glBindFramebuffer(GL_FRAMEBUFFER,f.framebuffer);glViewport(0,0,f.width,f.height);glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glUseProgram(flames->program);glBindVertexArray(flames->vao);glBindBuffer(GL_ARRAY_BUFFER,flames->instances);
    glBufferSubData(GL_ARRAY_BUFFER,0,count*sizeof(Instance),flames->staging.data());
    glUniformMatrix4fv(flames->vp,1,GL_FALSE,f.vp);glUniform3fv(flames->right,1,f.right);glUniform3fv(flames->forward,1,f.forward);
    glUniform1f(flames->time,float(std::remainder(f.time,64.)*(6.2831853071795864769/64.)));
    glUniform1f(flames->pixel_scale,float(f.height)/(2*f.tan_half_fov));glUniform1f(flames->eye_y,float(f.camera[1]));
    glUniform1f(flames->haze,f.haze);glUniform1i(flames->clip,f.clip_below);glUniform1f(flames->clip_height,f.clip_height);
    const float wind_scale=3.f/std::max(3.f,std::hypot(f.wind_xz[0],f.wind_xz[1]));
    glUniform2f(flames->wind,f.wind_xz[0]*wind_scale,f.wind_xz[1]*wind_scale);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D_ARRAY,flames->sprites);
    glUniform1i(flames->sprite_sampler,0);glUniform1i(flames->use_sprites,flames->sprites!=0);
    glEnable(GL_BLEND);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
    glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ZERO,GL_ONE_MINUS_SRC_ALPHA);
    glDrawArraysInstanced(GL_TRIANGLES,0,6,GLsizei(count*layers));glDisable(GL_BLEND);
    flames->stats.visible_emitters=unsigned(count);flames->stats.flame_slices=unsigned(count)*fire_layers;flames->stats.sparks=unsigned(count)*ember_layers;
    flames->stats.draws++;flames->stats.instance_uploads++;return true;
}
WorldTorchFlameStats world_torch_flames_stats(const WorldTorchFlames *flames){return flames?flames->stats:WorldTorchFlameStats{};}
void world_torch_flames_free(WorldTorchFlames *flames) {
    if(!flames)return;
    glUseProgram(0);glBindVertexArray(0);
    if(flames->sprites)glDeleteTextures(1,&flames->sprites);
    if(flames->instances)glDeleteBuffers(1,&flames->instances);
    if(flames->vao)glDeleteVertexArrays(1,&flames->vao);
    if(flames->program)glDeleteProgram(flames->program);
    delete flames;
}

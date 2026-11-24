#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_dragon_breath.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <new>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr unsigned flame_count=48,core_count=12,smoke_count=28,ember_count=24;
constexpr unsigned per_emitter=flame_count+core_count+smoke_count+ember_count;
static_assert(per_emitter*WORLD_DRAGON_BREATH_EMITTER_CAPACITY==WORLD_DRAGON_BREATH_PARTICLE_CAPACITY,"fixed breath budget");
constexpr double tau=6.2831853071795864769;
using Instance=std::array<float,16>;
using Staging=std::array<Instance,WORLD_DRAGON_BREATH_PARTICLE_CAPACITY>;
struct Prepared {unsigned visible=0,smoke=0,fire=0;};
float random01(std::uint32_t n) {
    n^=n>>16;n*=0x7feb352du;n^=n>>15;n*=0x846ca68bu;n^=n>>16;
    return float(n>>8)*(1.f/16777216.f);
}
float signal(double time,std::uint32_t seed) {
    const double phase=std::remainder(time,64.)*(tau/64.),s=random01(seed)*tau;
    return float(1.+.065*std::sin(phase*67.+s)+.035*std::sin(phase*113.+s*1.7));
}
bool emitters_valid(const WorldDragonBreathEmitter *e,std::size_t count) {
    if(count>WORLD_DRAGON_BREATH_EMITTER_CAPACITY||(count&&!e))return false;
    for(std::size_t i=0;i<count;i++) {
        for(double p:e[i].position)if(!std::isfinite(p)||std::abs(p)>1.e8)return false;
        double d=0;for(float v:e[i].direction){if(!std::isfinite(v))return false;d+=v*v;}
        if(std::abs(d-1.)>.025||!std::isfinite(e[i].strength)||e[i].strength<0||e[i].strength>1||
           !std::isfinite(e[i].length)||e[i].length<2||e[i].length>36||
           !std::isfinite(e[i].radius)||e[i].radius<.2f||e[i].radius>4||unsigned(e[i].variant)>1)return false;
    }
    return true;
}
bool frame_valid(const WorldDragonBreathFrame &f) {
    if(!f.color||!f.depth||f.color==f.depth||f.width<1||f.height<1||f.width>8192||f.height>8192||
       std::uint64_t(f.width)*f.height>4096ull*4096ull||!std::isfinite(f.time)||
       !std::isfinite(f.near_plane)||!std::isfinite(f.far_plane)||f.near_plane<=0||f.far_plane<=f.near_plane||
       f.far_plane>1000000||!std::isfinite(f.tan_half_fov)||f.tan_half_fov<.05f||f.tan_half_fov>4||
       !std::isfinite(f.haze)||f.haze<0||f.haze>1)return false;
    for(double p:f.camera)if(!std::isfinite(p)||std::abs(p)>1.e8)return false;
    bool nonzero=false;for(float v:f.vp){if(!std::isfinite(v))return false;nonzero=nonzero||v!=0;}
    if(!nonzero)return false;
    const float *basis[]={f.right,f.up,f.forward};
    for(unsigned i=0;i<3;i++) {
        double length=0;for(unsigned k=0;k<3;k++){const float v=basis[i][k];if(!std::isfinite(v))return false;length+=v*v;}
        if(std::abs(length-1.)>.03)return false;
        for(unsigned j=0;j<i;j++){double dot=0;for(unsigned k=0;k<3;k++)dot+=basis[i][k]*basis[j][k];if(std::abs(dot)>.03)return false;}
    }
    for(float v:f.fog)if(!std::isfinite(v)||v<0||v>16)return false;
    for(float v:f.wind_xz)if(!std::isfinite(v)||std::abs(v)>30)return false;
    return true;
}
bool visible(const WorldDragonBreathEmitter &e,const WorldDragonBreathFrame &f) {
    const double radius=e.length*.55+e.radius*3.+6.;
    const double p[3]={e.position[0]-f.camera[0]+e.direction[0]*e.length*.5,
        e.position[1]-f.camera[1]+e.direction[1]*e.length*.5+1.,
        e.position[2]-f.camera[2]+e.direction[2]*e.length*.5};
    if(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]>(500.+radius)*(500.+radius))return false;
    double along=0,x=0,y=0;for(unsigned k=0;k<3;k++){along+=p[k]*f.forward[k];x+=p[k]*f.right[k];y+=p[k]*f.up[k];}
    const double tv=f.tan_half_fov,th=tv*f.width/f.height;
    return along>=-radius&&std::abs(x)<=std::max(0.,along)*th+radius*std::hypot(1.,th)&&
        std::abs(y)<=std::max(0.,along)*tv+radius*std::hypot(1.,tv);
}
Prepared prepare(const std::array<WorldDragonBreathEmitter,WORLD_DRAGON_BREATH_EMITTER_CAPACITY> &emitters,
                 std::size_t count,const WorldDragonBreathFrame &f,Staging &staging) {
    Prepared out;std::array<Instance,WORLD_DRAGON_BREATH_EMITTER_CAPACITY*(flame_count+core_count+ember_count)> fire={};
    const double t=std::remainder(f.time,64.)/64.;
    const float wind_scale=3.f/std::max(3.f,std::hypot(f.wind_xz[0],f.wind_xz[1]));
    for(std::size_t ei=0;ei<count;ei++) {
        const auto &e=emitters[ei];if(e.strength<=.0001f||!visible(e,f))continue;out.visible++;
        // A stable basis around the authored jet direction is camera-independent.
        float tangent[3]={-e.direction[2],0,e.direction[0]};
        float norm=std::hypot(tangent[0],tangent[2]);
        if(norm<.001f){tangent[0]=1;tangent[2]=0;norm=1;}
        for(float &v:tangent)v/=norm;
        const float bitangent[3]={e.direction[1]*tangent[2]-e.direction[2]*tangent[1],
            e.direction[2]*tangent[0]-e.direction[0]*tangent[2],e.direction[0]*tangent[1]-e.direction[1]*tangent[0]};
        const float flicker=signal(f.time,e.seed);
        for(unsigned n=0;n<per_emitter;n++) {
            const bool smoke=n>=flame_count+core_count&&n<flame_count+core_count+smoke_count;
            const bool core=n>=flame_count&&n<flame_count+core_count;
            const bool ember=n>=flame_count+core_count+smoke_count;
            const unsigned local=smoke?n-flame_count-core_count:core?n-flame_count:ember?n-flame_count-core_count-smoke_count:n;
            const std::uint32_t seed=e.seed^((n+1)*0x9e3779b9u);
            const float phase=random01(seed)*float(tau),r=random01(seed^0x8123ab7fu);
            const int cycles=smoke?19:ember?47:core?41:37;
            // Stratified flame slices maintain overlap along the whole jet.
            // Independent random ages previously bunched into bright beads
            // separated by gaps, despite having enough total billboards.
            const double phase_age=core?(double(local)+.5)/core_count:
                !smoke&&!ember?(double(local)+.5)/flame_count:random01(seed^0x217fa37bu);
            const double age0=core?phase_age:t*cycles+phase_age;
            const float age=float(age0-std::floor(age0));
            const float life=std::min(1.f,age*12.f)*std::min(1.f,(1.f-age)*5.f);
            const float advance=smoke?.28f+age*.76f:age;
            const float cone=e.radius*(.08f+advance*.92f)*(smoke?.9f:core?.16f:ember?.7f:.50f);
            const float angle=phase+age*(smoke?1.7f:2.2f);
            const float radial=cone*(.25f+.75f*r);
            const float rise=age*age*(smoke?2.0f:ember?1.4f:.7f);
            Instance instance={};
            for(unsigned k=0;k<3;k++)instance[k]=float(e.position[k]-f.camera[k])+e.direction[k]*e.length*advance+
                (tangent[k]*std::cos(angle)+bitangent[k]*std::sin(angle))*radial;
            instance[0]+=f.wind_xz[0]*wind_scale*age*age*(smoke?.75f:.25f);
            instance[1]+=rise;instance[2]+=f.wind_xz[1]*wind_scale*age*age*(smoke?.75f:.25f);
            float width=core?(.07f+e.radius*(.035f+age*.08f)):smoke?(.4f+e.radius*(.4f+age*.9f)):
                ember?(.018f+.018f*r):(.15f+e.radius*(.09f+age*.40f));
            float half_length=core?(e.length/core_count*.8f+.15f):smoke?width:ember?width*(2.2f+2.f*r):(.55f+e.length*.05f);
            // Avoid subpixel bright spark flicker without unbounded expansion.
            float energy=1;
            if(ember) {
                const float view_depth=instance[0]*f.forward[0]+instance[1]*f.forward[1]+instance[2]*f.forward[2];
                const float filtered=std::max(width,std::min(width*2.5f,view_depth*2*f.tan_half_fov/f.height*.50f));
                energy=width*width/(filtered*filtered);half_length*=filtered/width;width=filtered;
            }
            instance[3]=width;
            for(unsigned k=0;k<3;k++)instance[4+k]=e.direction[k];
            instance[7]=half_length;instance[8]=age;
            instance[9]=e.strength*life*(smoke?.085f:core?.10f:ember?.35f:.14f);
            instance[10]=float(unsigned(e.variant));instance[11]=smoke?2.f:core?1.f:ember?3.f:0.f;
            instance[12]=phase;
            double sprite=t*(72.*21.)+r*72.+local*1.8125;
            sprite-=std::floor(sprite/72.)*72.;instance[13]=float(sprite);
            instance[14]=smoke?1.15f:ember?.08f:.45f;instance[15]=flicker*energy;
            if(smoke)staging[out.smoke++]=instance;else fire[out.fire++]=instance;
        }
    }
    // Depth rather than radial distance correctly orders smoke when looking
    // along a bank. Introsort uses fixed stack storage and performs no heap work.
    std::sort(staging.begin(),staging.begin()+out.smoke,[&](const Instance &a,const Instance &b){
        return a[0]*f.forward[0]+a[1]*f.forward[1]+a[2]*f.forward[2]>
               b[0]*f.forward[0]+b[1]*f.forward[1]+b[2]*f.forward[2];
    });
    std::copy_n(fire.begin(),out.fire,staging.begin()+out.smoke);return out;
}
GLuint compile(GLenum kind,const std::string &path) {
    std::ifstream file(path,std::ios::binary);if(!file)return 0;
    std::ostringstream stream;stream<<file.rdbuf();const std::string source=stream.str();if(source.empty()||source.size()>1024*1024)return 0;
    GLuint shader=glCreateShader(kind);const char *text=source.c_str();glShaderSource(shader,1,&text,nullptr);glCompileShader(shader);
    GLint good=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&good);
    if(!good){char log[4096]={};glGetShaderInfoLog(shader,sizeof(log),nullptr,log);std::fprintf(stderr,"[DragonBreath] %s: %s\n",path.c_str(),log);glDeleteShader(shader);return 0;}
    return shader;
}
struct State {
    GLint fbo=0,viewport[4]={},program=0,vao=0,array=0,active=0,texture0=0,texture1=0,texture2=0;
    GLint src_rgb=0,dst_rgb=0,src_alpha=0,dst_alpha=0,eq_rgb=0,eq_alpha=0,polygon[2]={};
    GLboolean depth_mask=GL_TRUE,color_mask[4]={};
    std::array<GLboolean,7> enabled={};
    static constexpr GLenum caps[7]={GL_BLEND,GL_DEPTH_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_FRAMEBUFFER_SRGB,GL_RASTERIZER_DISCARD};
    State() {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&fbo);glGetIntegerv(GL_VIEWPORT,viewport);
        glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&array);
        glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture0);
        glActiveTexture(GL_TEXTURE1);glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY,&texture1);
        glActiveTexture(GL_TEXTURE2);glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture2);
        glGetIntegerv(GL_BLEND_SRC_RGB,&src_rgb);glGetIntegerv(GL_BLEND_DST_RGB,&dst_rgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA,&src_alpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&dst_alpha);
        glGetIntegerv(GL_BLEND_EQUATION_RGB,&eq_rgb);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&eq_alpha);
        glGetBooleanv(GL_DEPTH_WRITEMASK,&depth_mask);glGetBooleanv(GL_COLOR_WRITEMASK,color_mask);glGetIntegerv(GL_POLYGON_MODE,polygon);
        for(unsigned i=0;i<enabled.size();i++)enabled[i]=glIsEnabled(caps[i]);
    }
    ~State() {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(fbo));glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
        glUseProgram(GLuint(program));glBindVertexArray(GLuint(vao));glBindBuffer(GL_ARRAY_BUFFER,GLuint(array));
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,GLuint(texture0));glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D_ARRAY,GLuint(texture1));
        glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,GLuint(texture2));glActiveTexture(GLenum(active));
        glBlendFuncSeparate(GLenum(src_rgb),GLenum(dst_rgb),GLenum(src_alpha),GLenum(dst_alpha));
        glBlendEquationSeparate(GLenum(eq_rgb),GLenum(eq_alpha));glDepthMask(depth_mask);glColorMask(color_mask[0],color_mask[1],color_mask[2],color_mask[3]);
        glPolygonMode(GL_FRONT_AND_BACK,GLenum(polygon[0]));
        for(unsigned i=0;i<enabled.size();i++){if(enabled[i])glEnable(caps[i]);else glDisable(caps[i]);}
    }
};
bool load_texture(const char *path,GLuint &destination,bool array,std::uint64_t &bytes) {
    if(!path)return false;
    std::ifstream file(path,std::ios::binary);std::array<std::uint32_t,4> h={};file.read(reinterpret_cast<char*>(h.data()),sizeof(h));
    if(!file||h!=std::array<std::uint32_t,4>{0x31584656u,128,array?256u:128u,array?144u:1u})return false;
    std::vector<unsigned char> pixels(std::size_t(h[1])*h[2]*h[3]*4);
    file.read(reinterpret_cast<char*>(pixels.data()),pixels.size());if(!file||file.peek()!=std::char_traits<char>::eof())return false;
    GLuint texture=0;glGenTextures(1,&texture);if(!texture)return false;
    const GLenum target=array?GL_TEXTURE_2D_ARRAY:GL_TEXTURE_2D;
    glActiveTexture(GL_TEXTURE0);glBindTexture(target,texture);
    // Dragon flames use only the72 torch frames; the campfire's second72
    // frames are not uploaded. This halves the additional resident art cost.
    const unsigned layers=array?72u:1u;
    if(array)glTexImage3D(target,0,GL_SRGB8_ALPHA8,h[1],h[2],layers,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    else glTexImage2D(target,0,GL_RGBA8,h[1],h[2],0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    glTexParameteri(target,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(target,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(target,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(target,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glGenerateMipmap(target);
    if(glGetError()!=GL_NO_ERROR){glDeleteTextures(1,&texture);return false;}
    glDeleteTextures(1,&destination);destination=texture;bytes=0;
    for(unsigned w=h[1],height=h[2];;w=std::max(1u,w/2),height=std::max(1u,height/2)){
        bytes+=std::uint64_t(w)*height*layers*4;if(w==1&&height==1)break;
    }
    return true;
}
}
struct WorldDragonBreath {
    GLuint program=0,vao=0,buffer=0,fbo=0,fire=0,smoke=0,attached=0;
    GLint vp=-1,right=-1,up=-1,forward=-1,resolution=-1,near_far=-1,time=-1,haze=-1,fog=-1,use_fire=-1,use_smoke=-1;
    std::array<WorldDragonBreathEmitter,WORLD_DRAGON_BREATH_EMITTER_CAPACITY> emitters={};
    Staging staging={};std::size_t count=0;WorldDragonBreathStats stats;
};
WorldDragonBreath *world_dragon_breath_create(const char *directory) {
    if(!directory||!*directory||!SDL_GL_GetCurrentContext())return nullptr;
    auto *p=new(std::nothrow) WorldDragonBreath;if(!p)return nullptr;
    const std::string prefix=std::string(directory)+"/world_dragon_breath";
    GLuint vs=compile(GL_VERTEX_SHADER,prefix+".vs.glsl"),fs=compile(GL_FRAGMENT_SHADER,prefix+".fs.glsl");
    if(!vs||!fs){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);delete p;return nullptr;}
    p->program=glCreateProgram();glAttachShader(p->program,vs);glAttachShader(p->program,fs);glLinkProgram(p->program);glDeleteShader(vs);glDeleteShader(fs);
    GLint linked=0;glGetProgramiv(p->program,GL_LINK_STATUS,&linked);
    if(!linked){char log[4096]={};glGetProgramInfoLog(p->program,sizeof(log),nullptr,log);std::fprintf(stderr,"[DragonBreath] Link: %s\n",log);world_dragon_breath_free(p);return nullptr;}
#define U(member,name) p->member=glGetUniformLocation(p->program,name)
    U(vp,"uVP");U(right,"uRight");U(up,"uUp");U(forward,"uForward");U(resolution,"uResolution");
    U(near_far,"uNearFar");U(time,"uTimePhase");U(haze,"uHaze");U(fog,"uFog");U(use_fire,"uUseFire");U(use_smoke,"uUseSmoke");
#undef U
    glUseProgram(p->program);glUniform1i(glGetUniformLocation(p->program,"uDepth"),0);
    glUniform1i(glGetUniformLocation(p->program,"uFire"),1);glUniform1i(glGetUniformLocation(p->program,"uSmoke"),2);
    glGenVertexArrays(1,&p->vao);glGenBuffers(1,&p->buffer);glGenFramebuffers(1,&p->fbo);
    if(!p->vao||!p->buffer||!p->fbo){world_dragon_breath_free(p);return nullptr;}
    glBindVertexArray(p->vao);glBindBuffer(GL_ARRAY_BUFFER,p->buffer);glBufferData(GL_ARRAY_BUFFER,sizeof(p->staging),nullptr,GL_STREAM_DRAW);
    for(unsigned i=0;i<4;i++){
        glEnableVertexAttribArray(i);glVertexAttribPointer(i,4,GL_FLOAT,GL_FALSE,sizeof(Instance),reinterpret_cast<void*>(std::uintptr_t(i*4*sizeof(float))));glVertexAttribDivisor(i,1);
    }
    glBindVertexArray(0);p->stats.gpu_buffer_bytes=sizeof(p->staging);return p;
}
bool world_dragon_breath_load_fire(WorldDragonBreath *p,const char *path) {
    if(!p||!load_texture(path,p->fire,true,p->stats.sprite_bytes))return false;
    p->stats.purchased_fire=true;return true;
}
bool world_dragon_breath_load_smoke(WorldDragonBreath *p,const char *path) {
    if(!p||!load_texture(path,p->smoke,false,p->stats.smoke_texture_bytes))return false;
    p->stats.purchased_smoke=true;return true;
}
bool world_dragon_breath_set_emitters(WorldDragonBreath *p,const WorldDragonBreathEmitter *emitters,std::size_t count) {
    if(!p||!emitters_valid(emitters,count))return false;
    if(count)std::copy_n(emitters,count,p->emitters.begin());
    p->count=count;p->stats.emitters=unsigned(count);return true;
}
std::size_t world_dragon_breath_lights(const WorldDragonBreath *p,double time,WorldDragonBreathLight *out,std::size_t capacity) {
    if(!p||!std::isfinite(time)||(!out&&capacity))return 0;
    std::size_t count=0;
    for(std::size_t i=0;i<p->count&&count<capacity;i++) {
        const auto &e=p->emitters[i];if(e.strength<=.0001f)continue;auto &light=out[count++];
        for(unsigned k=0;k<3;k++)light.position[k]=e.position[k]+e.direction[k]*std::min(2.5f,e.length*.20f);
        const bool cold=e.variant==WorldDragonBreathVariant::Frost;
        light.color[0]=cold?.24f:1.f;light.color[1]=cold?.67f:.29f;light.color[2]=cold?1.f:.04f;
        light.strength=e.strength*signal(time,e.seed)*2.2f;light.radius=std::min(18.f,8.f+e.length*.35f);
    }
    return count;
}
bool world_dragon_breath_draw(WorldDragonBreath *p,const WorldDragonBreathFrame &f) {
    if(!p||!frame_valid(f)){if(p)p->stats.failures++;return false;}
    p->stats.visible_emitters=p->stats.flames=p->stats.cores=p->stats.smoke=p->stats.embers=0;
    const auto prepared=prepare(p->emitters,p->count,f,p->staging);if(!prepared.visible)return true;
    State saved;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,p->fbo);
    if(p->attached!=f.color) {
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,f.color,0);glDrawBuffer(GL_COLOR_ATTACHMENT0);
        if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){p->attached=0;p->stats.failures++;return false;}
        p->attached=f.color;
    }
    glViewport(0,0,f.width,f.height);glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_FRAMEBUFFER_SRGB);glDisable(GL_RASTERIZER_DISCARD);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glUseProgram(p->program);glBindVertexArray(p->vao);glBindBuffer(GL_ARRAY_BUFFER,p->buffer);
    glBufferSubData(GL_ARRAY_BUFFER,0,(prepared.smoke+prepared.fire)*sizeof(Instance),p->staging.data());
    glUniformMatrix4fv(p->vp,1,GL_FALSE,f.vp);glUniform3fv(p->right,1,f.right);glUniform3fv(p->up,1,f.up);glUniform3fv(p->forward,1,f.forward);
    glUniform2f(p->resolution,float(f.width),float(f.height));glUniform2f(p->near_far,f.near_plane,f.far_plane);
    glUniform1f(p->time,float(std::remainder(f.time,64.)*(tau/64.)));glUniform1f(p->haze,f.haze);glUniform3fv(p->fog,1,f.fog);
    glUniform1i(p->use_fire,p->fire!=0);glUniform1i(p->use_smoke,p->smoke!=0);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,f.depth);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D_ARRAY,p->fire);
    glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,p->smoke);
    // Alpha is temporal-confidence metadata, not accumulated coverage. The
    // .4–.99 band belongs to static foliage reconstruction; a weak moving jet
    // must not inherit that tag from the opaque/foliage background beneath it.
    // Only surviving fragments write reactive0; RGB smoke coverage is intact.
    glEnable(GL_BLEND);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);glBlendFuncSeparate(GL_ONE,GL_ONE_MINUS_SRC_ALPHA,GL_ZERO,GL_ZERO);
    glDrawArraysInstanced(GL_TRIANGLES,0,6,GLsizei(prepared.smoke));
    // Base-instance is not GL3.3 core. Repoint the4 attributes inside the same
    // fixed VBO; no second upload, allocation, or per-particle draw is needed.
    for(unsigned i=0;i<4;i++)glVertexAttribPointer(i,4,GL_FLOAT,GL_FALSE,sizeof(Instance),
        reinterpret_cast<void*>(std::uintptr_t(prepared.smoke*sizeof(Instance)+i*4*sizeof(float))));
    glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ZERO,GL_ZERO);glDrawArraysInstanced(GL_TRIANGLES,0,6,GLsizei(prepared.fire));
    for(unsigned i=0;i<4;i++)glVertexAttribPointer(i,4,GL_FLOAT,GL_FALSE,sizeof(Instance),reinterpret_cast<void*>(std::uintptr_t(i*4*sizeof(float))));
    p->stats.visible_emitters=prepared.visible;p->stats.flames=prepared.visible*flame_count;p->stats.cores=prepared.visible*core_count;
    p->stats.smoke=prepared.smoke;p->stats.embers=prepared.visible*ember_count;p->stats.draws+=2;p->stats.instance_uploads++;return true;
}
bool world_dragon_breath_prepare(WorldDragonBreath *p,const WorldDragonBreathFrame &f,bool loading) {
    if(!p)return false;
    if(!loading||!frame_valid(f)){p->stats.warmup_failures++;return false;}
    // Preserve even a zero-strength authored source and every live counter;
    // these synthetic examples cannot enter the runtime emitter/light set.
    const auto authored=p->emitters;const std::size_t count=p->count;const auto stats=p->stats;
    std::array<WorldDragonBreathEmitter,2> examples={};
    for(unsigned i=0;i<examples.size();i++) {
        auto &e=examples[i];e.strength=1;e.length=18;e.radius=1.8f;e.seed=97+i*317;
        e.variant=i?WorldDragonBreathVariant::Frost:WorldDragonBreathVariant::Fire;
        for(unsigned k=0;k<3;k++) {
            e.position[k]=f.camera[k]+f.forward[k]*8.+f.right[k]*(i?2.:-2.);
            e.direction[k]=f.forward[k]*.8f+f.right[k]*(i?-.6f:.6f);
        }
        // Valid input camera bases permit a small orthonormal tolerance;
        // normalize the temporary jet to the tighter emitter contract.
        const float norm=std::sqrt(e.direction[0]*e.direction[0]+e.direction[1]*e.direction[1]+e.direction[2]*e.direction[2]);
        for(float &v:e.direction)v/=norm;
    }
    const GLenum prior_error=glGetError();
    bool ok=prior_error==GL_NO_ERROR&&world_dragon_breath_set_emitters(p,examples.data(),examples.size())&&world_dragon_breath_draw(p,f);
    const auto performed=p->stats.draws-stats.draws;
    const GLenum error=glGetError();ok=ok&&error==GL_NO_ERROR;
    p->emitters=authored;p->count=count;p->stats=stats;p->stats.warmups++;p->stats.warmup_draws+=performed;
    if(!ok) {
        p->stats.warmup_failures++;
        std::fprintf(stderr,"[DragonBreath] Loading preparation FAILED, priorGL0x%x GL0x%x\n",unsigned(prior_error),unsigned(error));
    }
    return ok;
}
WorldDragonBreathStats world_dragon_breath_stats(const WorldDragonBreath *p){return p?p->stats:WorldDragonBreathStats{};}
void world_dragon_breath_free(WorldDragonBreath *p) {
    if(!p)return;
    if(p->fire)glDeleteTextures(1,&p->fire);
    if(p->smoke)glDeleteTextures(1,&p->smoke);
    if(p->buffer)glDeleteBuffers(1,&p->buffer);
    if(p->vao)glDeleteVertexArrays(1,&p->vao);
    if(p->fbo)glDeleteFramebuffers(1,&p->fbo);
    if(p->program)glDeleteProgram(p->program);
    delete p;
}

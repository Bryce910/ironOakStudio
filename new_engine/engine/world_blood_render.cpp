#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_blood_render.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr std::size_t maximum_bank_bytes=192u*1024u*1024u;
constexpr std::uint64_t maximum_geometry_bytes=96ull*1024*1024,maximum_texture_bytes=64ull*1024*1024;
constexpr const char *package_sha="78a37065dcaf5ef2e103beb05ad85708680a4b35ff777ab9f907a470fdbe7bdf";
struct Variant {
    GLuint vao=0,vbo=0,ebo=0,mask=0;
    unsigned vertices=0,indices=0,frames=0;
    float duration=0,color[4]={},size[2]={};
};
struct Cursor {
    const std::vector<unsigned char> &bytes;std::size_t offset=0;
    const unsigned char *take(std::size_t count){
        if(offset>bytes.size()||count>bytes.size()-offset)throw std::runtime_error("Truncated blood bank");
        const auto *p=bytes.data()+offset;offset+=count;return p;
    }
    std::uint32_t integer(){const auto *p=take(4);return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);}
    float real(){const auto bits=integer();float value;std::memcpy(&value,&bits,4);if(!std::isfinite(value))throw std::runtime_error("Nonfinite blood bank value");return value;}
};
std::vector<unsigned char> read_file(const std::string &path,std::size_t limit){
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file)throw std::runtime_error("Cannot open blood bank/provenance");
    const auto size=file.tellg();
    if(size<=0||std::uint64_t(size)>limit)throw std::runtime_error("Blood file exceeds loading budget");
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size),0);
    file.seekg(0);if(!file.read(reinterpret_cast<char*>(bytes.data()),size))throw std::runtime_error("Cannot read blood bank/provenance");return bytes;
}
GLuint shader(GLenum type,const char *path){
    std::ifstream file(path);if(!file)throw std::runtime_error("Cannot open blood shader");
    std::ostringstream stream;stream<<file.rdbuf();const auto source=stream.str();const char *text=source.c_str();
    const GLuint result=glCreateShader(type);glShaderSource(result,1,&text,nullptr);glCompileShader(result);
    GLint ok=0;glGetShaderiv(result,GL_COMPILE_STATUS,&ok);
    if(!ok){char message[2048]={};glGetShaderInfoLog(result,sizeof message,nullptr,message);glDeleteShader(result);throw std::runtime_error(std::string("Blood shader: ")+message);}return result;
}
bool finite_point(world_blood_effects::Point p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<1e8&&std::abs(p.y)<1e7&&std::abs(p.z)<1e8;}
bool warm(WorldBloodRenderer *);
}

struct WorldBloodRenderer {
    GLuint program=0,fbo=0,quad=0,self_depth=0;
    int width=0,height=0;
    GLint vp=-1,origin=-1,rotation=-1,size=-1,weight=-1,scale=-1,mode=-1,depth=-1,mask=-1,resolution=-1,near_far=-1;
    GLint sun=-1,sun_color=-1,ambient=-1,color=-1,fade=-1,mask_scale=-1,pool_diameter=-1,age=-1,seed=-1,floor_y=-1;
    std::array<Variant,8> variants={};unsigned count=0;
    struct History {std::uint64_t serial=0;unsigned frame=0;};
    std::array<History,world_blood_effects::mesh_capacity> history={};
    WorldBloodStats stats;
};

WorldBloodRenderer *world_blood_create(const char *bank,char *error,std::size_t capacity){
    if(error&&capacity)error[0]=0;
    auto fail=[&](const char *text){if(error&&capacity)std::snprintf(error,capacity,"%s",text);};
    if(!SDL_GL_GetCurrentContext()){fail("Blood renderer requires a current OpenGL context");return nullptr;}
    if(!bank||!*bank||std::strlen(bank)>4096){fail("Invalid blood bank directory");return nullptr;}
    auto *p=new(std::nothrow)WorldBloodRenderer;
    if(!p){fail("Cannot allocate blood renderer");return nullptr;}
    GLuint vertex=0,fragment=0;
    try {
        const std::string directory(bank);
        const auto provenance=read_file(directory+"/provenance.json",256*1024);
        const std::string metadata(provenance.begin(),provenance.end());
        auto field=[&](const char *key){
            const auto start=metadata.find(std::string("\"")+key+"\"");
            if(start==std::string::npos)return std::string{};
            const auto colon=metadata.find(':',start);if(colon==std::string::npos)return std::string{};
            const auto quote=metadata.find_first_not_of(" \r\n\t",colon+1);
            if(quote==std::string::npos||metadata[quote]!='\"')return std::string{};
            const auto end=metadata.find('\"',quote+1);return end==std::string::npos?std::string{}:metadata.substr(quote+1,end-quote-1);
        };
        const auto bank_sha=field("bank_sha256");
        if(field("schema")!="valwood.volumetric-blood-provenance"||field("source_archive_sha256")!=package_sha||bank_sha.size()!=64||
           !std::all_of(bank_sha.begin(),bank_sha.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}))throw std::runtime_error("Blood purchase provenance mismatch");
        const auto bytes=read_file(directory+"/blood.bank",maximum_bank_bytes);Cursor in{bytes};
        if(std::memcmp(in.take(8),"VWBLOOD1",8)||in.integer()!=1)throw std::runtime_error("Unsupported blood bank version");
        p->count=in.integer();if(!p->count||p->count>p->variants.size())throw std::runtime_error("Invalid blood variant count");
        for(unsigned v=0;v<p->count;++v){
            auto &a=p->variants[v];a.vertices=in.integer();a.indices=in.integer();a.frames=in.integer();
            const auto width=in.integer(),height=in.integer();a.duration=in.real();
            if(!a.vertices||a.vertices>50000||!a.indices||a.indices>150000||a.indices%3||a.frames<2||a.frames>256||
               !width||width>2048||!height||height>2048||a.duration<1.f/240||a.duration>30)throw std::runtime_error("Blood variant exceeds source budgets");
            for(auto &c:a.color){c=in.real();if(c<0||c>4)throw std::runtime_error("Invalid blood source tint");}
            for(auto &s:a.size){s=in.real();if(s<.02||s>16)throw std::runtime_error("Invalid blood stain dimensions");}
            const std::uint64_t vertex_bytes=std::uint64_t(a.vertices)*a.frames*6*sizeof(float),index_bytes=std::uint64_t(a.indices)*4;
            const std::uint64_t texture_bytes=std::uint64_t(width)*height*4;
            if(vertex_bytes+index_bytes>maximum_geometry_bytes-p->stats.geometry_bytes||texture_bytes>maximum_texture_bytes-p->stats.texture_bytes)throw std::runtime_error("Blood resident data exceeds memory budget");
            std::vector<std::uint32_t> indices(a.indices);for(auto &index:indices){index=in.integer();if(index>=a.vertices)throw std::runtime_error("Blood triangle index out of bounds");}
            std::vector<float> vertices(std::size_t(a.vertices)*a.frames*6);
            for(std::size_t i=0;i<vertices.size();++i){vertices[i]=in.real();if(std::abs(vertices[i])>(i%6<3?32.f:4.f))throw std::runtime_error("Blood vertex bounds/normal invalid");}
            const auto *rgba=in.take(std::size_t(texture_bytes));
            glGenVertexArrays(1,&a.vao);glGenBuffers(1,&a.vbo);glGenBuffers(1,&a.ebo);glGenTextures(1,&a.mask);
            glBindVertexArray(a.vao);glBindBuffer(GL_ARRAY_BUFFER,a.vbo);glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(vertex_bytes),vertices.data(),GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,a.ebo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,GLsizeiptr(index_bytes),indices.data(),GL_STATIC_DRAW);
            for(unsigned attribute=0;attribute<4;++attribute){glEnableVertexAttribArray(attribute);glVertexAttribPointer(attribute,3,GL_FLOAT,GL_FALSE,6*sizeof(float),reinterpret_cast<void*>(std::uintptr_t((attribute%2)*3*sizeof(float))));}
            glBindTexture(GL_TEXTURE_2D,a.mask);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,GLsizei(width),GLsizei(height),0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            p->stats.geometry_bytes+=vertex_bytes+index_bytes;p->stats.texture_bytes+=texture_bytes;
            p->stats.source_vertices+=a.vertices;p->stats.triangles+=a.indices/3;p->stats.source_frames+=a.frames;p->stats.max_duration=std::max(p->stats.max_duration,double(a.duration));
        }
        if(in.offset!=bytes.size())throw std::runtime_error("Trailing blood bank bytes");
        vertex=shader(GL_VERTEX_SHADER,"shaders/world_blood.vs.glsl");fragment=shader(GL_FRAGMENT_SHADER,"shaders/world_blood.fs.glsl");
        p->program=glCreateProgram();glAttachShader(p->program,vertex);glAttachShader(p->program,fragment);glLinkProgram(p->program);
        glDeleteShader(vertex);vertex=0;glDeleteShader(fragment);fragment=0;
        GLint linked=0;glGetProgramiv(p->program,GL_LINK_STATUS,&linked);if(!linked)throw std::runtime_error("Cannot link blood shader program");
        auto uniform=[&](const char *name){return glGetUniformLocation(p->program,name);};
        p->vp=uniform("uVP");p->origin=uniform("uOrigin");p->rotation=uniform("uRotation");p->size=uniform("uSize");p->weight=uniform("uWeight");p->scale=uniform("uScale");p->mode=uniform("uMode");p->depth=uniform("uDepth");p->mask=uniform("uMask");p->resolution=uniform("uResolution");p->near_far=uniform("uNearFar");p->sun=uniform("uSun");p->sun_color=uniform("uSunColor");p->ambient=uniform("uAmbient");p->color=uniform("uColor");p->fade=uniform("uFade");
        p->mask_scale=uniform("uMaskScale");p->pool_diameter=uniform("uPoolDiameter");p->age=uniform("uAge");p->seed=uniform("uSeed");
        p->floor_y=uniform("uFloorY");
        glGenFramebuffers(1,&p->fbo);glGenVertexArrays(1,&p->quad);glBindVertexArray(0);
        if(!p->program||!p->fbo||!p->quad||glGetError()!=GL_NO_ERROR)throw std::runtime_error("Blood GPU upload failed");
        p->stats.loaded_variants=p->count;p->stats.provenance_loaded=true;
        if(!warm(p))throw std::runtime_error("Blood shader warmup failed");
        return p;
    }catch(const std::exception &e){if(vertex)glDeleteShader(vertex);if(fragment)glDeleteShader(fragment);fail(e.what());world_blood_free(p);return nullptr;}
    catch(...){if(vertex)glDeleteShader(vertex);if(fragment)glDeleteShader(fragment);fail("Unknown blood loading failure");world_blood_free(p);return nullptr;}
}
world_blood_effects::Config world_blood_config(const WorldBloodRenderer *p){
    world_blood_effects::Config config;config.stain_duration=24;config.persistent_stains=true;
    if(!p)return config;
    config.mesh_duration=0;config.variant_count=p->count;
    for(unsigned i=0;i<p->count;++i){
        const double duration=(p->variants[i].duration-world_blood_effects::source_start_seconds)/world_blood_effects::playback_rate;
        config.variant_durations[i]=std::max(1./240.,duration);config.mesh_duration=std::max(config.mesh_duration,config.variant_durations[i]);
    }
    return config;
}
bool world_blood_prepare(WorldBloodRenderer *p,int width,int height){
    if(!p)return false;
    if(width<1||height<1||width>8192||height>8192){++p->stats.failures;return false;}
    if(p->self_depth&&width==p->width&&height==p->height)return true;
    GLuint next=0;glGenRenderbuffers(1,&next);glBindRenderbuffer(GL_RENDERBUFFER,next);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);
    if(!next||glGetError()!=GL_NO_ERROR){if(next)glDeleteRenderbuffers(1,&next);++p->stats.failures;return false;}
    glBindFramebuffer(GL_FRAMEBUFFER,p->fbo);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,next);
    if(p->self_depth)glDeleteRenderbuffers(1,&p->self_depth);
    p->self_depth=next;p->width=width;p->height=height;glBindRenderbuffer(GL_RENDERBUFFER,0);glBindFramebuffer(GL_FRAMEBUFFER,0);return true;
}

bool world_blood_draw(WorldBloodRenderer *p,const world_blood_effects::System &system,const WorldBloodFrame &frame){
    if(!p)return false;
    p->stats.active_meshes=p->stats.active_stains=0;
    p->stats.minimum_stain_fade=1;p->stats.oldest_stain_age=0;p->stats.persistent_stains_drawn=0;
    auto fail=[&](){++p->stats.failures;return false;};const auto &f=frame.target;
    if(!f.color||!f.depth||f.color==f.depth||f.width<1||f.height<1||f.width>8192||f.height>8192||
       !std::isfinite(f.near_plane)||!std::isfinite(f.far_plane)||f.near_plane<.01||f.far_plane<=f.near_plane||!finite_point(f.camera)||!std::isfinite(frame.scale)||frame.scale<.02||frame.scale>2)return fail();
    for(float v:f.vp)if(!std::isfinite(v))return fail();
    for(unsigned i=0;i<3;++i)if(!std::isfinite(frame.sun[i])||!std::isfinite(frame.sun_color[i])||!std::isfinite(frame.ambient[i])||frame.sun_color[i]<0||frame.ambient[i]<0||frame.sun_color[i]>64||frame.ambient[i]>64)return fail();
    const double sun_length=std::hypot(frame.sun[0],frame.sun[1],frame.sun[2]);if(!std::isfinite(sun_length)||sun_length<1e-8)return fail();
    if(!p->self_depth||p->width!=f.width||p->height!=f.height)return fail();
    const auto stage=system.stage();if(!stage.mesh_count&&!stage.stain_count)return true;
    glBindFramebuffer(GL_FRAMEBUFFER,p->fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,f.color,0);glDrawBuffer(GL_COLOR_ATTACHMENT0);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,0,0);return fail();}
    glViewport(0,0,f.width,f.height);glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glEnable(GL_BLEND);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);glBlendFuncSeparate(GL_ONE,GL_ONE_MINUS_SRC_ALPHA,GL_ZERO,GL_ZERO);
    glUseProgram(p->program);glUniformMatrix4fv(p->vp,1,GL_FALSE,f.vp);glUniform2f(p->resolution,float(f.width),float(f.height));glUniform2f(p->near_far,f.near_plane,f.far_plane);glUniform1f(p->scale,frame.scale);
    glUniform3f(p->sun,float(frame.sun[0]/sun_length),float(frame.sun[1]/sun_length),float(frame.sun[2]/sun_length));glUniform3fv(p->sun_color,1,frame.sun_color);glUniform3fv(p->ambient,1,frame.ambient);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,f.depth);glUniform1i(p->depth,0);glUniform1i(p->mask,1);
    auto place=[&](world_blood_effects::Point point,double yaw){glUniform3f(p->origin,float(point.x-f.camera.x),float(point.y-f.camera.y),float(point.z-f.camera.z));glUniform2f(p->rotation,float(std::cos(yaw)),float(std::sin(yaw)));};
    // The stain pool remains across duels and is independent of mesh playback.
    glUniform1i(p->mode,1);glUniform1f(p->scale,1);glBindVertexArray(p->quad);
    for(unsigned i=0;i<stage.stain_count;++i){const auto &s=stage.stains[i];const auto &v=p->variants[(s.serial-1)%p->count];
        const float strength=std::sqrt(s.strength),diameter=s.finisher?std::clamp(.85f*strength,.85f,1.1f):std::clamp(.52f*strength,.4f,.7f);
        const float mask_x=v.size[0]*frame.scale*.70f*strength,mask_z=v.size[1]*frame.scale*.70f*strength;
        const float size_x=std::max(mask_x,diameter*1.5f),size_z=std::max(mask_z,diameter*1.5f);
        place(s.position,s.yaw);glUniform2f(p->size,size_x,size_z);glUniform2f(p->mask_scale,size_x/mask_x,size_z/mask_z);
        glUniform1f(p->pool_diameter,diameter);glUniform1f(p->age,float(s.age));glUniform1f(p->seed,float((s.hit_id*2654435761u)%65536u)*.001f);glUniform4fv(p->color,1,v.color);
        const float remaining=s.persistent?1.f:float(std::clamp((s.duration-s.age)/std::min(3.,s.duration),0.,1.));glUniform1f(p->fade,remaining);
        p->stats.minimum_stain_fade=std::min(p->stats.minimum_stain_fade,remaining);p->stats.oldest_stain_age=std::max(p->stats.oldest_stain_age,s.age);p->stats.persistent_stains_drawn+=s.persistent;
        glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,v.mask);glDrawArrays(GL_TRIANGLES,0,6);++p->stats.stain_draws;++p->stats.active_stains;p->stats.drawn_triangles+=2;
    }
    // Match the purchased fluid's Cull Back / ZWrite On behavior. The private
    // depth attachment gives real self-occlusion without sampling a texture
    // attached to this framebuffer or modifying the caller's opaque depth.
    glDepthMask(GL_TRUE);glClearDepth(1);glClear(GL_DEPTH_BUFFER_BIT);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glFrontFace(GL_CCW);
    std::array<std::pair<double,unsigned>,world_blood_effects::mesh_capacity> order={};
    for(unsigned i=0;i<stage.mesh_count;++i){const auto &s=stage.meshes[i];const double x=s.position.x-f.camera.x,y=s.position.y-f.camera.y,z=s.position.z-f.camera.z;order[i]={-(x*x+y*y+z*z),i};}
    for(unsigned i=1;i<stage.mesh_count;++i)for(unsigned j=i;j&&order[j]<order[j-1];--j)std::swap(order[j],order[j-1]);
    glUniform1i(p->mode,0);glUniform1f(p->fade,1);
    for(unsigned n=0;n<stage.mesh_count;++n){const auto &s=stage.meshes[order[n].second];const auto &v=p->variants[((s.hit_id-1)+s.lobe)%p->count];
        if(s.age<s.delay)continue;
        // Compress the actual authored liquid trajectory; lifecycle clip lengths
        // use exactly this mapping so completed fluid leaves no occupied slots.
        const double source_age=(s.age-s.delay)*world_blood_effects::playback_rate+world_blood_effects::source_start_seconds;if(source_age>=v.duration)continue;
        const double sample=std::clamp(source_age/double(v.duration),0.,1.)*(v.frames-1);const unsigned a=unsigned(sample),b=std::min(a+1,v.frames-1);
        const std::size_t stride=std::size_t(v.vertices)*6*sizeof(float);glBindVertexArray(v.vao);glBindBuffer(GL_ARRAY_BUFFER,v.vbo);
        for(unsigned attribute=0;attribute<4;++attribute){const std::size_t offset=(attribute<2?a:b)*stride+(attribute%2)*3*sizeof(float);glVertexAttribPointer(attribute,3,GL_FLOAT,GL_FALSE,6*sizeof(float),reinterpret_cast<void*>(std::uintptr_t(offset)));}
        const double yaw=s.yaw+(s.lobe?((s.hit_id&1)?1.0471975512:-1.0471975512):0);
        place(s.position,yaw);glUniform1f(p->scale,frame.scale*std::sqrt(s.strength)*(s.lobe?.78f:1.f));
        glUniform1f(p->floor_y,float(s.floor_y-f.camera.y));
        glUniform1f(p->weight,float(sample-a));glUniform4fv(p->color,1,v.color);glDrawElements(GL_TRIANGLES,GLsizei(v.indices),GL_UNSIGNED_INT,nullptr);
        ++p->stats.mesh_draws;++p->stats.animation_frames;++p->stats.active_meshes;p->stats.drawn_triangles+=v.indices/3;p->stats.last_frame=a;p->stats.last_serial=s.serial;
        if(s.lobe)++p->stats.secondary_draws;
        if(s.finisher)++p->stats.finisher_draws;
        auto history=std::find_if(p->history.begin(),p->history.end(),[&](const WorldBloodRenderer::History &h){return h.serial==s.serial;});
        if(history!=p->history.end()){if(history->frame!=a)++p->stats.frame_transitions;history->frame=a;}
        else{history=std::min_element(p->history.begin(),p->history.end(),[](const WorldBloodRenderer::History &a,const WorldBloodRenderer::History &b){return a.serial<b.serial;});*history={s.serial,a};}
    }
    ++p->stats.draws;p->stats.peak_meshes=std::max(p->stats.peak_meshes,p->stats.active_meshes);p->stats.peak_stains=std::max(p->stats.peak_stains,p->stats.active_stains);
    glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glBindVertexArray(0);glActiveTexture(GL_TEXTURE0);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,0,0);return true;
}
namespace {
bool warm(WorldBloodRenderer *p){
    constexpr unsigned width=64,height=64;std::array<float,width*height*4> color={};std::array<float,width*height> depth={};depth.fill(1);
    GLuint textures[2]={};glGenTextures(2,textures);
    for(unsigned i=0;i<2;++i){glBindTexture(GL_TEXTURE_2D,textures[i]);glTexImage2D(GL_TEXTURE_2D,0,i?GL_R32F:GL_RGBA16F,width,height,0,i?GL_RED:GL_RGBA,GL_FLOAT,i?depth.data():color.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);}
    world_blood_effects::System effects;effects.configure(world_blood_config(p));world_blood_effects::Input input;
    input.contacts[1]={{0,0,-4},{0,0,1},-1,true};input.outgoing_hits=p->count;effects.update(input,0);effects.update(input,.25);
    WorldBloodFrame frame;auto &f=frame.target;f.color=textures[0];f.depth=textures[1];f.width=width;f.height=height;f.near_plane=.4f;f.far_plane=100;f.vp[0]=f.vp[5]=1.5f;f.vp[10]=-100.4f/99.6f;f.vp[11]=-1;f.vp[14]=-80.f/99.6f;
    const bool ok=world_blood_prepare(p,width,height)&&world_blood_draw(p,effects,frame);glFinish();glDeleteTextures(2,textures);glBindFramebuffer(GL_FRAMEBUFFER,0);
    const auto source=p->stats;p->stats={};p->stats.loaded_variants=source.loaded_variants;p->stats.source_frames=source.source_frames;p->stats.source_vertices=source.source_vertices;p->stats.triangles=source.triangles;p->stats.geometry_bytes=source.geometry_bytes;p->stats.texture_bytes=source.texture_bytes;p->stats.max_duration=source.max_duration;p->stats.provenance_loaded=source.provenance_loaded;p->history={};
    return ok&&glGetError()==GL_NO_ERROR;
}
}
WorldBloodStats world_blood_stats(const WorldBloodRenderer *p){return p?p->stats:WorldBloodStats{};}
void world_blood_free(WorldBloodRenderer *p){
    if(!p)return;
    for(auto &v:p->variants){if(v.vao)glDeleteVertexArrays(1,&v.vao);if(v.vbo)glDeleteBuffers(1,&v.vbo);if(v.ebo)glDeleteBuffers(1,&v.ebo);if(v.mask)glDeleteTextures(1,&v.mask);}
    if(p->quad)glDeleteVertexArrays(1,&p->quad);
    if(p->self_depth)glDeleteRenderbuffers(1,&p->self_depth);
    if(p->fbo)glDeleteFramebuffers(1,&p->fbo);
    if(p->program)glDeleteProgram(p->program);
    delete p;
}

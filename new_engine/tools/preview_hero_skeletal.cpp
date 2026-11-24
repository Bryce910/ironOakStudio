// Isolated asset preview. The real engine importer, sampler and GPU skinning
// are used; the neutral preview lighting is deliberately not game lighting.
#define GL_GLEXT_PROTOTYPES
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "engine/world_animation.h"
#include "engine/world_animation_render.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#include <png.h>
extern "C" {
#include "engine/texture_cache.h"
}
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace wa = world_animation;
namespace ar = world_animation_render;
constexpr float pi = 3.14159265358979323846f;
struct Options {
    std::string asset, clip, capture, capture_directory, shader_directory;
    int frames=0, width=1000, height=1000, capture_every=1;
    double phase=0, fps=60;
    float yaw=.38f, pitch=.12f, zoom=1;
    bool hidden=false, visible=false, paused=false, help=false, native_units=false;
};
double number(const std::string &s, double low, double high, const char *name) {
    char *end=nullptr;
    const double value=std::strtod(s.c_str(), &end);
    if(end==s.c_str() || *end || !std::isfinite(value) || value<low || value>high)
        throw std::runtime_error(std::string(name)+" outside allowed range");
    return value;
}
int integer(const std::string &s, int low, int high, const char *name) {
    const double value=number(s,low,high,name);
    if(std::floor(value)!=value) throw std::runtime_error(std::string(name)+" requires an integer");
    return int(value);
}
Options options(int argc, char **argv) {
    Options o;
    for(int i=1;i<argc;i++) {
        const std::string arg=argv[i];
        auto next=[&]() { if(i+1>=argc)throw std::runtime_error("Missing value for "+arg); return std::string(argv[++i]); };
        if(arg=="--help")o.help=true;
        else if(arg=="--clip")o.clip=next();
        else if(arg=="--phase")o.phase=number(next(),0,1,"--phase");
        else if(arg=="--frames")o.frames=integer(next(),1,1000000,"--frames");
        else if(arg=="--fps")o.fps=number(next(),1,240,"--fps");
        else if(arg=="--width")o.width=integer(next(),128,4096,"--width");
        else if(arg=="--height")o.height=integer(next(),128,4096,"--height");
        else if(arg=="--yaw")o.yaw=float(number(next(),-3600,3600,"--yaw"))*pi/180;
        else if(arg=="--pitch")o.pitch=float(number(next(),-70,80,"--pitch"))*pi/180;
        else if(arg=="--zoom")o.zoom=float(number(next(),.2,10,"--zoom"));
        else if(arg=="--capture")o.capture=next();
        else if(arg=="--capture-dir")o.capture_directory=next();
        else if(arg=="--capture-every")o.capture_every=integer(next(),1,1000000,"--capture-every");
        else if(arg=="--shaders")o.shader_directory=next();
        else if(arg=="--hidden")o.hidden=true;
        else if(arg=="--visible")o.visible=true;
        else if(arg=="--paused")o.paused=true;
        else if(arg=="--native-units")o.native_units=true;
        else if(!arg.empty()&&arg.front()=='-')throw std::runtime_error("Unknown option: "+arg);
        else if(o.asset.empty())o.asset=arg;
        else throw std::runtime_error("Only one asset can be previewed");
    }
    if(o.hidden&&o.visible)throw std::runtime_error("Choose either --hidden or --visible");
    if(o.hidden&&!o.frames)throw std::runtime_error("--hidden requires bounded --frames");
    if(o.frames&&!o.visible)o.hidden=true;
    if(!o.capture.empty()&&!o.frames)throw std::runtime_error("--capture requires bounded --frames");
    if(!o.capture_directory.empty()&&!o.frames)throw std::runtime_error("--capture-dir requires bounded --frames");
    if(o.capture_every!=1&&o.capture_directory.empty())throw std::runtime_error("--capture-every requires --capture-dir");
    if(!o.help&&o.asset.empty())throw std::runtime_error("Missing ASSET.glb; use --help");
    return o;
}
std::string read(const std::filesystem::path &path) {
    std::ifstream f(path,std::ios::binary);
    if(!f)throw std::runtime_error("Cannot read shader: "+path.string());
    return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
}
std::filesystem::path shaders(const Options &o, const char *exe) {
    if(!o.shader_directory.empty())return o.shader_directory;
    std::vector<std::filesystem::path> candidates={"new_engine/shaders","shaders"};
    std::error_code ec;
    auto binary=std::filesystem::canonical(exe,ec);
    if(!ec)candidates.push_back(binary.parent_path()/"../../shaders");
    for(const auto &p:candidates)if(std::filesystem::is_regular_file(p/"world_skeletal.vs.glsl"))return p;
    throw std::runtime_error("Cannot find shader directory; pass --shaders new_engine/shaders");
}
GLuint compile(GLenum stage, const std::string &source) {
    GLuint shader=glCreateShader(stage);
    const char *data=source.c_str();glShaderSource(shader,1,&data,nullptr);glCompileShader(shader);
    GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok) {
        GLint length=0;glGetShaderiv(shader,GL_INFO_LOG_LENGTH,&length);
        std::string log(std::size_t(std::max(1,length)),'\0');glGetShaderInfoLog(shader,length,nullptr,log.data());
        glDeleteShader(shader);throw std::runtime_error("Shader compile: "+log);
    }
    return shader;
}
GLuint program(const std::string &vs, const std::string &fs) {
    GLuint vert=compile(GL_VERTEX_SHADER,vs),frag=0,p=0;
    try {
        frag=compile(GL_FRAGMENT_SHADER,fs);p=glCreateProgram();
        glAttachShader(p,vert);glAttachShader(p,frag);glLinkProgram(p);
        GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);
        if(!ok) {
            GLint length=0;glGetProgramiv(p,GL_INFO_LOG_LENGTH,&length);
            std::string log(std::size_t(std::max(1,length)),'\0');glGetProgramInfoLog(p,length,nullptr,log.data());
            throw std::runtime_error("Program link: "+log);
        }
    } catch(...) {if(p)glDeleteProgram(p);if(frag)glDeleteShader(frag);glDeleteShader(vert);throw;}
    glDeleteShader(vert);glDeleteShader(frag);return p;
}
void gl_check(const char *where) {
    const GLenum e=glGetError();if(e!=GL_NO_ERROR) {
        std::ostringstream out;out<<where<<": GL error 0x"<<std::hex<<e;throw std::runtime_error(out.str());
    }
}
wa::Vec3 add(wa::Vec3 a,wa::Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
wa::Vec3 sub(wa::Vec3 a,wa::Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
wa::Vec3 mul(wa::Vec3 a,float b){return {a.x*b,a.y*b,a.z*b};}
float dot(wa::Vec3 a,wa::Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
wa::Vec3 cross(wa::Vec3 a,wa::Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
wa::Vec3 unit(wa::Vec3 a){const float l=std::sqrt(dot(a,a));if(l<1e-8f)throw std::runtime_error("Degenerate camera basis");return mul(a,1/l);}
wa::Mat4 product(const wa::Mat4 &a,const wa::Mat4 &b) {
    wa::Mat4 out;out.m.fill(0);
    for(unsigned c=0;c<4;c++)for(unsigned r=0;r<4;r++)for(unsigned k=0;k<4;k++)out.m[c*4+r]+=a.m[k*4+r]*b.m[c*4+k];
    return out;
}
wa::Mat4 perspective(float aspect) {
    const float f=1/std::tan(38*pi/360),near=.02f,far=250.f;
    wa::Mat4 m;m.m.fill(0);m.m[0]=f/aspect;m.m[5]=f;m.m[10]=(far+near)/(near-far);m.m[11]=-1;m.m[14]=2*far*near/(near-far);return m;
}
wa::Mat4 look(wa::Vec3 eye,wa::Vec3 target) {
    const auto f=unit(sub(target,eye)),s=unit(cross(f,{0,1,0})),u=cross(s,f);
    wa::Mat4 m;m.m={s.x,u.x,-f.x,0,s.y,u.y,-f.y,0,s.z,u.z,-f.z,0,-dot(s,eye),-dot(u,eye),dot(f,eye),1};return m;
}
struct Bounds {
    wa::Vec3 low{INFINITY,INFINITY,INFINITY},high{-INFINITY,-INFINITY,-INFINITY};
    void add(wa::Vec3 v) {
        if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))throw std::runtime_error("Non-finite deformed position");
        low={std::min(low.x,v.x),std::min(low.y,v.y),std::min(low.z,v.z)};
        high={std::max(high.x,v.x),std::max(high.y,v.y),std::max(high.z,v.z)};
    }
};
void pose_for(const wa::Asset &asset,int clip,double time,wa::Pose &pose) {
    std::string error;
    const bool ok=clip<0?wa::rest_pose(asset.skeleton,pose,error):wa::sample(asset.skeleton,asset.clips[unsigned(clip)],time,true,pose,error);
    if(!ok)throw std::runtime_error("Pose sampling: "+error);
}
Bounds bounds_for(const wa::Asset &asset,int clip) {
    Bounds bounds;wa::Pose pose;std::string error;
    const int samples=clip<0?1:25;
    for(int i=0;i<samples;i++) {
        pose_for(asset,clip,clip<0?0:asset.clips[unsigned(clip)].duration*i/samples,pose);
        for(const auto &v:asset.mesh.vertices) {
            wa::Vec3 p,n;if(!wa::deform_vertex(v,pose,p,n,error))throw std::runtime_error("Bounds deformation: "+error);
            bounds.add(wa::transform_point(pose.global[asset.skeleton.mesh_node],p));
        }
    }
    return bounds;
}
void uniform_matrix(GLuint p,const char *name,const wa::Mat4 &m){glUniformMatrix4fv(glGetUniformLocation(p,name),1,GL_FALSE,m.m.data());}
void uniform3(GLuint p,const char *name,wa::Vec3 v){glUniform3f(glGetUniformLocation(p,name),v.x,v.y,v.z);}
void capture(const std::string &path,int width,int height) {
    if(std::filesystem::path(path).extension()!=".png")throw std::runtime_error("--capture path must end in .png");
    std::vector<unsigned char> pixels(std::size_t(width)*height*4),flipped(pixels.size());
    glFinish();glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);
    glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());gl_check("PNG pixel readback");
    const std::size_t stride=std::size_t(width)*4;
    for(int y=0;y<height;y++)std::copy_n(pixels.data()+std::size_t(height-y-1)*stride,stride,flipped.data()+std::size_t(y)*stride);
    png_image image{};image.version=PNG_IMAGE_VERSION;image.width=unsigned(width);image.height=unsigned(height);image.format=PNG_FORMAT_RGBA;
    if(!png_image_write_to_file(&image,path.c_str(),0,flipped.data(),0,nullptr)) {
        const std::string error=image.message;png_image_free(&image);throw std::runtime_error("PNG write: "+error);
    }
    png_image_free(&image);std::cout<<"capture="<<std::filesystem::absolute(path)<<'\n';
}
struct Resources {
    SDL_Window *window=nullptr;SDL_GLContext context=nullptr;
    ar::Renderer *renderer=nullptr;GLuint hero=0,ground=0,ground_vao=0,ground_vbo=0;
    ~Resources(){
        if(context&&SDL_GL_GetCurrentContext()==context){ar::destroy(renderer);if(hero)glDeleteProgram(hero);if(ground)glDeleteProgram(ground);glDeleteVertexArrays(1,&ground_vao);glDeleteBuffers(1,&ground_vbo);texture_cache_shutdown();}
        if(context)SDL_GL_DeleteContext(context);
        if(window)SDL_DestroyWindow(window);
        SDL_Quit();
    }
};
const char *ground_vertex=R"GLSL(#version 330 core
layout(location=0)in vec3 aPos;uniform mat4 uVP;out vec3 vPos;
void main(){vPos=aPos;gl_Position=uVP*vec4(aPos,1.);})GLSL";
const char *ground_fragment=R"GLSL(#version 330 core
in vec3 vPos;out vec4 color;
void main(){vec2 q=vPos.xz/.25;vec2 d=abs(fract(q-.5)-.5)/max(fwidth(q),vec2(.0001));
float grid=1.-min(min(d.x,d.y),1.);float fade=exp(-dot(vPos.xz,vPos.xz)*.018);
vec3 linear=mix(vec3(.065,.073,.085),vec3(.11,.12,.13),fade)-grid*.012*fade;
color=vec4(pow(max(linear,vec3(0.)),vec3(1./2.2)),1.);})GLSL";

int run(const Options &o,const char *exe) {
    wa::Asset asset;std::string error;
    if(!wa::load_glb(o.asset.c_str(),asset,error)||!wa::validate(asset,error))throw std::runtime_error("Asset import: "+error);
    if(asset.mesh.vertices.empty()||asset.mesh.indices.empty())throw std::runtime_error("Asset has no skinned triangles");
    int clip=asset.clips.empty()?-1:0;
    if(!o.clip.empty()) {
        if(o.clip=="rest")clip=-1;
        else {
            auto found=std::find_if(asset.clips.begin(),asset.clips.end(),[&](const wa::Clip &c){return c.name==o.clip;});
            if(found!=asset.clips.end())clip=int(found-asset.clips.begin());
            else clip=integer(o.clip,0,int(asset.clips.size())-1,"--clip (name/index)");
        }
    }
    std::cout<<"asset="<<std::filesystem::absolute(o.asset)<<"\nvertices="<<asset.mesh.vertices.size()<<" triangles="<<asset.mesh.indices.size()/3<<" joints="<<asset.skeleton.palette.size()<<" materials="<<asset.materials.size()<<" images="<<asset.images.size()<<'\n';
    for(unsigned i=0;i<asset.clips.size();i++)std::cout<<"clip["<<i<<"]="<<asset.clips[i].name<<" duration="<<asset.clips[i].duration<<'\n';
    std::cout<<"lighting=neutral standalone preview; source material maps/factors retained\n"
             <<"controls=left/right drag: orbit; wheel: zoom; Space: pause; [ / ]: previous/next clip; Left/Right: step 1/60s; R: rest pose; Home: reset camera; Esc: quit\n";
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0)throw std::runtime_error(std::string("SDL init: ")+SDL_GetError());
    Resources r;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
    r.window=SDL_CreateWindow("Valwood skeletal asset preview | neutral preview lighting",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,o.width,o.height,SDL_WINDOW_OPENGL|(o.hidden?SDL_WINDOW_HIDDEN:SDL_WINDOW_RESIZABLE));
    if(!r.window)throw std::runtime_error(std::string("SDL GL window: ")+SDL_GetError());
    r.context=SDL_GL_CreateContext(r.window);if(!r.context)throw std::runtime_error(std::string("OpenGL 3.3 context: ")+SDL_GetError());
    if(SDL_GL_MakeCurrent(r.window,r.context)!=0)throw std::runtime_error(std::string("Make current: ")+SDL_GetError());
    GLint major=0,minor=0;glGetIntegerv(GL_MAJOR_VERSION,&major);glGetIntegerv(GL_MINOR_VERSION,&minor);
    if(major<3||(major==3&&minor<3))throw std::runtime_error("OpenGL 3.3 or later is required");
    std::cout<<"GL_vendor="<<glGetString(GL_VENDOR)<<"\nGL_renderer="<<glGetString(GL_RENDERER)<<"\nGL_version="<<glGetString(GL_VERSION)<<'\n';
    SDL_GL_SetSwapInterval(o.frames?0:1);
    const auto shader_path=shaders(o,exe);
    r.hero=program(read(shader_path/"world_skeletal.vs.glsl"),read(shader_path/"hero_preview.fs.glsl"));
    r.ground=program(ground_vertex,ground_fragment);
    r.renderer=ar::create(asset,1,error);if(!r.renderer)throw std::runtime_error("GPU asset upload: "+error);
    if(!ar::warm(r.renderer,r.hero,error))throw std::runtime_error("GPU skin contract: "+error);
    const float ground_vertices[]={-35,-.008f,-35,35,-.008f,-35,35,-.008f,35,-35,-.008f,-35,35,-.008f,35,-35,-.008f,35};
    glGenVertexArrays(1,&r.ground_vao);glGenBuffers(1,&r.ground_vbo);glBindVertexArray(r.ground_vao);glBindBuffer(GL_ARRAY_BUFFER,r.ground_vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(ground_vertices),ground_vertices,GL_STATIC_DRAW);glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),nullptr);glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);
    glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glDisable(GL_BLEND);glDisable(GL_FRAMEBUFFER_SRGB);
    const auto restore=ar::capture_bindings();
    ar::Instance instance;float base_distance=3.4f;wa::Vec3 target{0,.9f,0};
    auto fit=[&]() {
        if(o.native_units) {
            instance.scale=1;instance.x=instance.y=instance.z=0;base_distance=4.3f;
            std::cout<<"selected_clip="<<(clip<0?"rest":asset.clips[unsigned(clip)].name)<<" preview_scale=1 origin=unchanged native_units=true\n";
            return;
        }
        const auto bounds=bounds_for(asset,clip);const float height=bounds.high.y-bounds.low.y;
        if(!std::isfinite(height)||height<1e-5f)throw std::runtime_error("Cannot frame zero-height asset");
        instance.scale=1.8f/height;
        if(instance.scale>100)throw std::runtime_error("Asset too small for preview renderer instance scale; fix source units");
        instance.x=-(bounds.high.x+bounds.low.x)*.5f*instance.scale;
        instance.y=-bounds.low.y*instance.scale;instance.z=-(bounds.high.z+bounds.low.z)*.5f*instance.scale;
        const float span=std::max({1.8f,(bounds.high.x-bounds.low.x)*instance.scale,(bounds.high.z-bounds.low.z)*instance.scale});
        base_distance=span*1.85f;
        std::cout<<"selected_clip="<<(clip<0?"rest":asset.clips[unsigned(clip)].name)<<" source_height="<<height<<" preview_scale="<<instance.scale<<'\n';
    };
    fit();
    if(!o.capture_directory.empty())std::filesystem::create_directories(o.capture_directory);
    float yaw=o.yaw,pitch=o.pitch,zoom=o.zoom;bool paused=o.paused,running=true;
    double time=clip<0?0:o.phase*asset.clips[unsigned(clip)].duration;
    auto previous=std::chrono::steady_clock::now();int rendered=0;wa::Pose pose;
    while(running&&(!o.frames||rendered<o.frames)) {
        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            if(event.type==SDL_QUIT)running=false;
            else if(event.type==SDL_MOUSEWHEEL)zoom=std::clamp(zoom*std::exp(-event.wheel.y*.12f),.2f,10.f);
            else if(event.type==SDL_MOUSEMOTION&&(event.motion.state&(SDL_BUTTON_LMASK|SDL_BUTTON_RMASK))) {
                yaw-=event.motion.xrel*.006f;pitch=std::clamp(pitch+event.motion.yrel*.006f,-70*pi/180,80*pi/180);
            } else if(event.type==SDL_KEYDOWN&&!event.key.repeat) {
                const auto key=event.key.keysym.sym;
                if(key==SDLK_ESCAPE)running=false;
                else if(key==SDLK_SPACE)paused=!paused;
                else if(key==SDLK_HOME){yaw=o.yaw;pitch=o.pitch;zoom=o.zoom;}
                else if(key==SDLK_LEFT||key==SDLK_RIGHT){paused=true;time=std::max(0.,time+(key==SDLK_LEFT?-1.:1.)/60);}
                else if(key==SDLK_r){clip=-1;time=0;fit();}
                else if((key==SDLK_RIGHTBRACKET||key==SDLK_LEFTBRACKET)&&!asset.clips.empty()) {
                    const int count=int(asset.clips.size());clip=(clip+(key==SDLK_RIGHTBRACKET?1:count-1)+count)%count;
                    time=0;fit();
                }
            }
        }
        if(!running)break;
        const auto now=std::chrono::steady_clock::now();
        const double dt=o.frames?1/o.fps:std::min(.1,std::chrono::duration<double>(now-previous).count());previous=now;
        if(rendered&&!paused)time+=dt;
        pose_for(asset,clip,time,pose);
        if(!ar::upload(r.renderer,&pose,&instance,1,restore,error))throw std::runtime_error("GPU skin upload: "+error);
        int width=0,height=0;SDL_GL_GetDrawableSize(r.window,&width,&height);
        if(width<1||height<1){SDL_Delay(10);continue;}
        glViewport(0,0,width,height);glClearColor(.285f,.31f,.345f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        const float distance=base_distance*zoom*std::max(1.f,float(height)/width);
        const auto eye=add(target,{std::sin(yaw)*std::cos(pitch)*distance,std::sin(pitch)*distance,std::cos(yaw)*std::cos(pitch)*distance});
        const auto vp=product(perspective(float(width)/height),look(eye,target));
        glDisable(GL_CULL_FACE);glUseProgram(r.ground);uniform_matrix(r.ground,"uVP",vp);glBindVertexArray(r.ground_vao);glDrawArrays(GL_TRIANGLES,0,6);glBindVertexArray(0);glEnable(GL_CULL_FACE);
        glUseProgram(r.hero);uniform_matrix(r.hero,"uVP",vp);uniform3(r.hero,"uEye",eye);uniform3(r.hero,"uOffset",{});
        if(!ar::draw(r.renderer,r.hero,restore,error))throw std::runtime_error("GPU skinned draw: "+error);
        gl_check("Preview frame");
        if(!o.capture_directory.empty()&&rendered%o.capture_every==0) {
            std::ostringstream filename;filename<<"frame-"<<std::setw(6)<<std::setfill('0')<<rendered/o.capture_every<<".png";
            capture((std::filesystem::path(o.capture_directory)/filename.str()).string(),width,height);
        }
        if(o.frames&&rendered==o.frames-1&&!o.capture.empty())capture(o.capture,width,height);
        SDL_GL_SwapWindow(r.window);++rendered;
        if(!o.hidden) {
            std::ostringstream title;title<<"Valwood asset preview | "<<(clip<0?"REST":asset.clips[unsigned(clip)].name)<<" | "<<std::fixed<<std::setprecision(2)<<time<<"s | "<<(paused?"PAUSED":"PLAY")<<" | neutral preview lighting";
            SDL_SetWindowTitle(r.window,title.str().c_str());
        }
    }
    const auto stats=ar::stats(r.renderer);
    std::cout<<"rendered_frames="<<rendered<<" final_seconds="<<time<<" GPU_draws="<<stats.draws<<" GPU_uploads="<<stats.uploads<<" GPU_failures="<<stats.failures<<" texture_reference_bytes="<<stats.reference_texture_bytes<<'\n';
    return 0;
}
} // namespace
int main(int argc,char **argv) {
    try {
        const auto o=options(argc,argv);
        if(o.help) {
            std::cout<<"Usage: preview_hero_skeletal ASSET.glb [--clip NAME|INDEX|rest] [--phase 0..1]\n"
                <<" [--frames N] [--fps 60] [--capture OUTPUT.png] [--visible|--hidden] [--paused]\n"
                <<" [--capture-dir DIRECTORY] [--capture-every N]\n"
                <<" [--width 1000] [--height 1000] [--yaw DEGREES] [--pitch DEGREES] [--zoom 1] [--native-units]\n"
                <<" [--shaders DIRECTORY]\nBounded runs are hidden and use fixed time steps; capture is the last frame.\n"
                <<"Actual source skeletal animation/materials, standalone neutral preview lighting.\n";return 0;
        }
        return run(o,argv[0]);
    } catch(const std::exception &e){std::cerr<<"preview_hero_skeletal: "<<e.what()<<'\n';return 1;}
}

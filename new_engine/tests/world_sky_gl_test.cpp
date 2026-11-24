/* Actual sky-module correctness fixture. Software/offscreen by default;
 * VALWOOD_TEST_NATIVE_GL=1 preserves a caller-selected native context.
 * This verifies bounded resources and rendering invariants, not FPS or art. */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_sky.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>
#include <unistd.h>

namespace {std::uint64_t uniform_queries=0;}
// Exercise the real driver while detecting any accidental per-draw location
// queries in the module. SDL resolves the context entry point independently
// of this linked fixture wrapper.
extern "C" GLint APIENTRY glGetUniformLocation(GLuint program,const GLchar *name) {
    using Function=GLint (APIENTRY *)(GLuint,const GLchar *);
    static const auto actual=reinterpret_cast<Function>(SDL_GL_GetProcAddress("glGetUniformLocation"));
    assert(actual&&actual!=glGetUniformLocation);uniform_queries++;
    return actual(program,name);
}

namespace {
using Pixel=std::array<float,4>;
struct Target {
    unsigned fbo=0,color=0,depth=0;int w,h;
    Target(int width,int height):w(width),h(height) {
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,w,h,0,GL_RGBA,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
        glGenTextures(1,&depth);glBindTexture(GL_TEXTURE_2D,depth);
        glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,w,h,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depth,0);
        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
        glDepthMask(GL_TRUE);glClearDepth(.37);glClear(GL_DEPTH_BUFFER_BIT);
    }
    ~Target(){glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&color);glDeleteTextures(1,&depth);}
};
std::vector<Pixel> read(const Target &t) {
    glBindFramebuffer(GL_FRAMEBUFFER,t.fbo);std::vector<Pixel> result(t.w*t.h);
    glReadPixels(0,0,t.w,t.h,GL_RGBA,GL_FLOAT,result.data());
    for(const auto &p:result) {
        for(float f:p)assert(std::isfinite(f));
        assert(p[0]>=0&&p[1]>=0&&p[2]>=0&&p[3]>=.67f&&p[3]<=1.001f);
    }
    assert(glGetError()==GL_NO_ERROR);return result;
}
float difference(const std::vector<Pixel> &a,const std::vector<Pixel> &b) {
    assert(a.size()==b.size());double sum=0;
    for(unsigned i=0;i<a.size();++i)for(unsigned k=0;k<3;++k)sum+=std::abs(a[i][k]-b[i][k]);
    return float(sum/(a.size()*3));
}
void check_overcast(WorldSky *sky,const Target &output) {
    WorldSkyFrame view;
    view.eye[1]=200;view.forward[1]=.6f;view.forward[2]=.8f;
    view.up[1]=.8f;view.up[2]=-.6f;
    view.sun[0]=0;view.sun[1]=.6f;view.sun[2]=.8f;
    view.sun_color[0]=8;view.sun_color[1]=6;view.sun_color[2]=4;
    view.fog[0]=.186f;view.fog[1]=.224f;view.fog[2]=.215f;
    view.sky_light[0]=.15f;view.sky_light[1]=.17f;view.sky_light[2]=.16f;
    // Deliberately retain a saturated blue input zenith. The overcast sheet
    // must cover blue gaps and the horizon even without expensive clouds.
    view.sky_zenith[0]=.04f;view.sky_zenith[1]=.12f;view.sky_zenith[2]=.26f;
    view.cloud=.96f;view.volumetric_clouds=false;
    auto draw=[&](const WorldSkyFrame &frame) {
        assert(world_sky_draw(sky,frame,output.fbo,output.w,output.h));return read(output);
    };
    const auto sheet=draw(view);
    float gray_spread=0,maximum_sheet=0;
    for(const auto &p:sheet) {
        const float low=std::min({p[0],p[1],p[2]}),high=std::max({p[0],p[1],p[2]});
        gray_spread=std::max(gray_spread,(high-low)/std::max(high,.0001f));
        maximum_sheet=std::max(maximum_sheet,high);
        assert(std::abs(p[3]-1.f)<1e-6f);
    }
    assert(gray_spread<.065f&&maximum_sheet<.24f);
    auto horizon=view;
    horizon.forward[1]=.04f;horizon.forward[2]=std::sqrt(1.f-.04f*.04f);
    horizon.up[1]=horizon.forward[2];horizon.up[2]=-.04f;
    const auto horizon_sheet=draw(horizon);
    for(const auto &p:horizon_sheet) {
        const float low=std::min({p[0],p[1],p[2]}),high=std::max({p[0],p[1],p[2]});
        assert((high-low)/std::max(high,.0001f)<.065f&&high<.24f);
    }
    auto no_sun=view;std::fill(std::begin(no_sun.sun_color),std::end(no_sun.sun_color),0.f);
    const float sheet_solar_error=difference(sheet,draw(no_sun));assert(sheet_solar_error==0);
    auto disc_off=view;disc_off.sun_disc=false;
    assert(difference(sheet,draw(disc_off))==0);
    view.volumetric_clouds=true;no_sun.volumetric_clouds=true;
    const auto volume=draw(view);
    const float volume_solar_error=difference(volume,draw(no_sun));assert(volume_solar_error==0);
    assert(difference(volume,sheet)>.001f);
    auto ordinary_sun=view;ordinary_sun.volumetric_clouds=false;ordinary_sun.cloud=.28f;
    const auto visible_sun=draw(ordinary_sun);
    ordinary_sun.sun_disc=false;
    assert(difference(visible_sun,draw(ordinary_sun))>.0001f);
    float ordinary_peak=0;for(const auto &p:visible_sun)ordinary_peak=std::max(ordinary_peak,p[0]);
    assert(ordinary_peak>maximum_sheet+1.f);
    // The normal weather range is unchanged. With raymarching disabled, .28
    // and the last non-overcast value .65 equal the original clear atmosphere.
    no_sun.volumetric_clouds=false;no_sun.cloud=0;const auto clear=draw(no_sun);
    no_sun.cloud=.28f;assert(difference(clear,draw(no_sun))==0);
    no_sun.cloud=.65f;assert(difference(clear,draw(no_sun))==0);
    no_sun.cloud=.95f;const auto full=draw(no_sun);
    no_sun.cloud=.8f;const auto middle=draw(no_sun);
    float midpoint_error=0;
    for(size_t i=0;i<middle.size();i++)for(unsigned k=0;k<3;k++)
        midpoint_error=std::max(midpoint_error,std::abs(middle[i][k]-(clear[i][k]+full[i][k])*.5f));
    assert(midpoint_error<.0003f&&difference(clear,middle)>.005f&&difference(middle,full)>.005f);
    no_sun.cloud=.649f;const auto lower=draw(no_sun);
    no_sun.cloud=.651f;const float lower_edge_error=difference(lower,draw(no_sun));
    no_sun.cloud=.949f;const auto upper=draw(no_sun);
    no_sun.cloud=.951f;const float upper_edge_error=difference(upper,draw(no_sun));
    assert(lower_edge_error<.0001f&&upper_edge_error<.0001f);
    std::printf("Overcast sky: gray spread=%g peak=%g ordinary sun peak=%g; sun leak cheap=%g volume=%g; midpoint=%g transition edges=%g/%g\n",
        gray_spread,maximum_sheet,ordinary_peak,sheet_solar_error,volume_solar_error,midpoint_error,lower_edge_error,upper_edge_error);
}
Pixel linear(const std::vector<Pixel> &source,int w,int h,float u,float v) {
    float x=u*w-.5f,y=v*h-.5f;int ix=int(std::floor(x)),iy=int(std::floor(y));float fx=x-ix,fy=y-iy;Pixel result={};
    for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx) {
        const Pixel &p=source[std::clamp(iy+dy,0,h-1)*w+std::clamp(ix+dx,0,w-1)];
        for(int k=0;k<4;++k)result[k]+=p[k]*(dx?fx:1-fx)*(dy?fy:1-fy);
    }
    return result;
}
void check_tent(GLuint texture,const Target &destination,const std::vector<Pixel> &actual,int w,int h) {
    glBindTexture(GL_TEXTURE_2D,texture);std::vector<Pixel> source(w*h);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,source.data());
    float error=0,control=0;
    for(int y=0;y<destination.h;++y)for(int x=0;x<destination.w;++x) {
        float u=(x+.5f)/destination.w,v=(y+.5f)/destination.h;Pixel expected={},single=linear(source,w,h,u,v);
        for(int dy:{-1,1})for(int dx:{-1,1}) {
            Pixel tap=linear(source,w,h,u+dx*.5f/w,v+dy*.5f/h);
            for(int k=0;k<4;++k)expected[k]+=tap[k]*.25f;
        }
        for(int k=0;k<4;++k) {
            error=std::max(error,std::abs(expected[k]-actual[y*destination.w+x][k]));
            control=std::max(control,std::abs(expected[k]-single[k]));
        }
    }
    assert(error<.003f&&control>.01f);
    std::printf("Sky upsample: four-tap reference error=%g, old-single-tap control=%g\n",error,control);
}
GLuint texture_binding(GLenum name) {GLint id=0;glGetIntegerv(name,&id);return GLuint(id);}
void ppm(const char *path,const Target &t,const std::vector<Pixel> &pixels) {
    std::ofstream output(path,std::ios::binary);output<<"P6\n"<<t.w<<' '<<t.h<<"\n255\n";
    for(int y=t.h-1;y>=0;--y)for(int x=0;x<t.w;++x)for(int k=0;k<3;++k) {
        float v=pixels[y*t.w+x][k];v=1-std::exp(-v*1.2f);
        output.put(static_cast<char>(std::clamp(std::pow(v,1/2.2f)*255.f,0.f,255.f)));
    }
}
std::string source_file(const char *path){std::ifstream f(path);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
void check_dead_work_equivalence(const char *saved_reference){
    const auto optimized_source=source_file("shaders/world_sky.fs.glsl");
    std::string reference_source;
    if(saved_reference)reference_source=source_file(saved_reference);
    else {
        // Preserve the pre-optimization blocks as an independent reference.
        // This keeps the regression repeatable without requiring a /tmp file;
        // release QA can additionally supply its actual frozen pre-edit copy.
        reference_source=optimized_source;
        const auto replace=[&](const char *first,const char *last,const char *original){
            const auto begin=reference_source.find(first),end=reference_source.find(last,begin);
            assert(begin!=std::string::npos&&end!=std::string::npos);
            reference_source.replace(begin,end+std::char_traits<char>::length(last)-begin,original);
        };
        replace("// Begin full-overcast dead-work guard.","// End full-overcast dead-work guard.",R"GLSL(
    vec3 sky = atmosphere(ray, sun, toward_sun, max(zenith, vec3(0)), solar_visibility);
#ifdef VALWOOD_HDR_ENVIRONMENT
    if(uEnvironmentStrength>0.){vec3 d=vec3(uEnvironmentRotation.x*ray.x-uEnvironmentRotation.y*ray.z,ray.y,uEnvironmentRotation.y*ray.x+uEnvironmentRotation.x*ray.z);sky=mix(sky,max(textureLod(uEnvironmentSky,d,0.).rgb,vec3(0))*uEnvironmentIntensity,uEnvironmentStrength);}
#endif
    if (overcast > 0.0) {
        const vec3 luminance = vec3(.2126, .7152, .0722);
        vec3 gray_zenith = max(dot(zenith, luminance), 0.0) * vec3(.94, 1.0, .98);
        vec3 gray_horizon = max(dot(uFog, luminance), 0.0) * vec3(.94, 1.0, .98);
        vec3 sheet = mix(gray_horizon, gray_zenith, pow(max(ray.y, 0.0), .42));
        sky = mix(sky, sheet, overcast);
    }
)GLSL");
        replace("// Begin zero-solar phase guard.","// End zero-solar phase guard.",R"GLSL(
                float phase_light = .80 * hg(toward_sun, .55) + .20 * hg(toward_sun, -.20);
                float sun_up = smoothstep(-.08, .08, sun.y);
)GLSL");
    }
    const auto loop=[](const std::string &s){
        const auto start=s.find("for (int i = 0; i < 12; ++i)"),finish=s.find("float aerial",start);
        assert(start!=std::string::npos&&finish!=std::string::npos);return s.substr(start,finish-start);
    };
    // Every density, sunward light step, opacity weight and early-exit threshold
    // stays byte-identical. Only work multiplied by exact zero moved outside.
    assert(loop(reference_source)==loop(optimized_source));
    char reference_path[]="/tmp/valwood-sky-equivalence-XXXXXX";const int fd=mkstemp(reference_path);assert(fd>=0);close(fd);
    {std::ofstream f(reference_path);f<<reference_source;assert(f.good());}
    WorldSky *before=world_sky_create("shaders/world_sky.vs.glsl",reference_path),*after=world_sky_create();unlink(reference_path);assert(before&&after);
    const auto queries=uniform_queries;
    GLuint environment=0;glActiveTexture(GL_TEXTURE1);glGenTextures(1,&environment);glBindTexture(GL_TEXTURE_CUBE_MAP,environment);
    for(unsigned face=0;face<6;face++){
        std::array<float,4*4*3> pixels;
        for(unsigned y=0;y<4;y++)for(unsigned x=0;x<4;x++){
            const auto at=(y*4+x)*3;pixels[at]=.12f+.07f*face+.04f*x;pixels[at+1]=.09f+.03f*face+.02f*y;pixels[at+2]=.20f+.06f*face+.01f*(x+y);
        }
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,0,GL_RGB16F,4,4,0,GL_RGB,GL_FLOAT,pixels.data());
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    for(GLenum axis:{GL_TEXTURE_WRAP_S,GL_TEXTURE_WRAP_T,GL_TEXTURE_WRAP_R})glTexParameteri(GL_TEXTURE_CUBE_MAP,axis,GL_CLAMP_TO_EDGE);
    Target output(48,32);unsigned cases=0,max_byte_error=0;float max_rgb_error=0,max_stability_error=0,hdr_signal=0;bool volume_has_signal=false;
    for(float cloud:{0.f,.28f,.8f,.9499f,.95f,.9501f,.96f,1.f})for(float height:{200.f,2699.f,3300.f,4500.f})for(unsigned heading=0;heading<3;heading++)for(float time:{0.f,173.25f})for(bool volume:{false,true}){
        WorldSkyFrame frame;frame.eye[0]=4000;frame.eye[1]=height;frame.eye[2]=-2300;frame.cloud=cloud;frame.time=time;frame.volumetric_clouds=volume;
        const float yaw=heading*1.7f,pitch=heading==0?.04f:heading==1?.65f:-.40f,c=std::cos(pitch),s=std::sin(pitch),sy=std::sin(yaw),cy=std::cos(yaw);
        frame.forward[0]=sy*c;frame.forward[1]=s;frame.forward[2]=cy*c;frame.right[0]=cy;frame.right[1]=0;frame.right[2]=-sy;frame.up[0]=-sy*s;frame.up[1]=c;frame.up[2]=-cy*s;
        frame.sun[0]=.3f;frame.sun[1]=.8f;frame.sun[2]=.519615f;frame.sun_color[0]=1.15f;frame.sun_color[1]=1.06f;frame.sun_color[2]=.9f;
        frame.fog[0]=.186f;frame.fog[1]=.224f;frame.fog[2]=.215f;frame.environment_rotation=27;frame.environment_intensity=.85f;
        std::vector<Pixel> no_hdr;
        for(bool hdr:{false,true}){
            frame.environment_texture=hdr?environment:0;frame.environment_strength=hdr?.72f:0.f;
            assert(world_sky_draw(before,frame,output.fbo,output.w,output.h));const auto old=read(output);
            assert(world_sky_draw(after,frame,output.fbo,output.w,output.h));const auto current=read(output);cases++;
            for(unsigned i=0;i<old.size();i++){
                for(unsigned k=0;k<3;k++){
                    max_rgb_error=std::max(max_rgb_error,std::abs(old[i][k]-current[i][k]));
                    const int a=int(std::lround(std::clamp(old[i][k],0.f,1.f)*255)),b=int(std::lround(std::clamp(current[i][k],0.f,1.f)*255));
                    max_byte_error=std::max(max_byte_error,unsigned(std::abs(a-b)));
                }
                max_stability_error=std::max(max_stability_error,std::abs(old[i][3]-current[i][3]));
                if(volume&&old[i][3]<.99f)volume_has_signal=true;
            }
            if(!hdr)no_hdr=current;else hdr_signal=std::max(hdr_signal,difference(no_hdr,current));
        }
    }
    assert(cases==768&&max_rgb_error<=1.f/1024&&max_byte_error<=1&&max_stability_error<=1.e-6f&&hdr_signal>.005f&&volume_has_signal);
    assert(world_sky_stats(before).view_steps==12&&world_sky_stats(before).light_steps==2&&world_sky_stats(after).view_steps==12&&world_sky_stats(after).light_steps==2);
    assert(uniform_queries==queries);
    std::printf("Sky pre/post equivalence: %u cases, max linear RGB=%g, max RGB8=%u byte, max stability=%g, HDR signal=%g; identical12-view/2-conditional-light-step loop\n",cases,max_rgb_error,max_byte_error,max_stability_error,hdr_signal);
    world_sky_free(before);world_sky_free(after);glDeleteTextures(1,&environment);assert(!glIsTexture(environment)&&glGetError()==GL_NO_ERROR);
}
}
int main(int argc,char **argv) {
    if(argc>1)assert(chdir(argv[1])==0);
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL")){SDL_setenv("SDL_VIDEODRIVER","offscreen",1);SDL_setenv("LIBGL_ALWAYS_SOFTWARE","1",1);}
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window=SDL_CreateWindow("Volumetric sky correctness",0,0,192,128,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);
    SDL_GLContext context=SDL_GL_CreateContext(window);assert(context);
    std::printf("Sky renderer: %s\n",glGetString(GL_RENDERER));
    assert(!world_sky_create("/missing/vertex","/missing/fragment"));
    WorldSky *sky=world_sky_create();assert(sky);
    const auto cached_uniform_queries=uniform_queries;assert(cached_uniform_queries>=19);
    assert(world_sky_stats(sky).noise_bytes==1198372&&world_sky_stats(sky).noise_uploads==1);
    assert(!world_sky_draw(nullptr,{},0,96,64));
    {
        Target output(193,129),reflection(96,64),probe(128,128);WorldSkyFrame frame;
        assert(frame.volumetric_clouds);
        frame.eye[1]=200;frame.forward[1]=.4f;frame.forward[2]=.916515f;
        frame.up[1]=.916515f;frame.up[2]=-.4f;
        frame.sun[0]=.3f;frame.sun[1]=.8f;frame.sun[2]=.519615f;
        frame.sun_color[0]=1.15f;frame.sun_color[1]=1.06f;frame.sun_color[2]=.9f;
        frame.sky_light[0]=.28f;frame.sky_light[1]=.32f;frame.sky_light[2]=.42f;
        frame.cloud=0;
        glEnable(GL_SCISSOR_TEST);glScissor(0,0,1,1);glEnable(GL_BLEND);glEnable(GL_CULL_FACE);glEnable(GL_DEPTH_TEST);
        assert(world_sky_draw(sky,frame,output.fbo,output.w,output.h));
        const GLuint first=texture_binding(GL_TEXTURE_BINDING_2D),noise=texture_binding(GL_TEXTURE_BINDING_3D);
        assert(first&&noise&&first!=output.color);
        const auto clear=read(output);
        GLint read_fbo=0,draw_fbo=0,viewport[4];glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read_fbo);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw_fbo);glGetIntegerv(GL_VIEWPORT,viewport);
        assert(GLuint(read_fbo)==output.fbo&&GLuint(draw_fbo)==output.fbo&&viewport[2]==output.w&&viewport[3]==output.h);
        assert(!glIsEnabled(GL_SCISSOR_TEST)&&!glIsEnabled(GL_BLEND)&&!glIsEnabled(GL_CULL_FACE)&&!glIsEnabled(GL_DEPTH_TEST));
        GLboolean mask=GL_TRUE;glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);assert(!mask);
        std::vector<float> depth(output.w*output.h);glBindTexture(GL_TEXTURE_2D,output.depth);glGetTexImage(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,GL_FLOAT,depth.data());
        for(float d:depth)assert(std::abs(d-.37f)<1e-6f);
        auto stats=world_sky_stats(sky);assert(stats.width[0]==97&&stats.height[0]==65&&stats.target_allocations==1&&stats.target_resizes==0);
        frame.time=100;frame.eye[0]=8000;frame.eye[2]=-4000;
        assert(world_sky_draw(sky,frame,output.fbo,output.w,output.h));assert(difference(clear,read(output))==0);
        frame.time=0;frame.eye[0]=frame.eye[2]=0;frame.cloud=.8f;
        assert(world_sky_draw(sky,frame,output.fbo,output.w,output.h));const auto cloudy=read(output);
        check_tent(first,output,cloudy,97,65);
        assert(difference(clear,cloudy)>.008f);
        float minimum_alpha=1;for(const auto &p:cloudy)minimum_alpha=std::min(minimum_alpha,p[3]);assert(minimum_alpha<.96f);
        ppm("/tmp/valwood-sky-volume.ppm",output,cloudy);
        // Clouds off preserves weather and lighting, but bypasses raymarching.
        // Dense overcast retains its inexpensive sheet at both view sizes;
        // ordinary .28 weather still equals the old clear-atmosphere fallback.
        frame.volumetric_clouds=false;
        assert(world_sky_draw(sky,frame,output.fbo,output.w,output.h));
        const auto disabled=read(output);assert(difference(clear,disabled)>.005f);
        for(const auto &p:disabled)assert(std::abs(p[3]-1.f)<1e-6f);
        assert(frame.cloud==.8f&&frame.sun_color[0]==1.15f&&frame.sky_light[0]==.28f);
        assert(texture_binding(GL_TEXTURE_BINDING_2D)==first);
        auto ordinary=frame;ordinary.cloud=.28f;
        assert(world_sky_draw(sky,ordinary,output.fbo,output.w,output.h));
        assert(difference(clear,read(output))<1e-6f);
        assert(world_sky_draw(sky,frame,reflection.fbo,reflection.w,reflection.h,1));
        const auto disabled_reflection=read(reflection);
        auto clear_reflection=frame;clear_reflection.volumetric_clouds=true;clear_reflection.cloud=0;
        assert(world_sky_draw(sky,clear_reflection,reflection.fbo,reflection.w,reflection.h,1));
        const auto reflection_reference=read(reflection);
        assert(difference(disabled_reflection,reflection_reference)>.005f);
        assert(world_sky_draw(sky,ordinary,reflection.fbo,reflection.w,reflection.h,1));
        assert(difference(reflection_reference,read(reflection))<1e-6f);
        frame.volumetric_clouds=true;
        assert(world_sky_draw(sky,frame,output.fbo,output.w,output.h));
        assert(difference(cloudy,read(output))==0);
        stats=world_sky_stats(sky);
        assert(stats.target_allocations==2&&stats.target_resizes==0&&stats.noise_uploads==1);
        assert(uniform_queries==cached_uniform_queries);
        check_overcast(sky,output);
        frame.eye[0]-=100*(.0014f/.00042f);frame.eye[2]-=100*(-.00055f/.00042f);frame.time=100;
        assert(world_sky_draw(sky,frame,output.fbo,output.w,output.h));
        float anchored_error=difference(cloudy,read(output));assert(anchored_error<.00025f);
        frame.time=0;frame.eye[0]=1200;frame.eye[2]=0;
        assert(world_sky_draw(sky,frame,output.fbo,output.w,output.h));float parallax=difference(cloudy,read(output));assert(parallax>.005f);
        frame.eye[0]=0;frame.eye[1]=4500;frame.forward[1]=1;frame.forward[2]=0;frame.up[1]=0;frame.up[2]=1;
        assert(world_sky_draw(sky,frame,output.fbo,output.w,output.h));const auto above=read(output);
        frame.cloud=0;assert(world_sky_draw(sky,frame,output.fbo,output.w,output.h));assert(difference(above,read(output))<1e-6f);
        frame.cloud=1;frame.eye[1]=3300;assert(world_sky_draw(sky,frame,output.fbo,output.w,output.h));(void)read(output);
        frame.eye[1]=50000;frame.forward[1]=-1;frame.forward[2]=0;frame.up[1]=0;frame.up[2]=1;
        assert(world_sky_draw(sky,frame,output.fbo,output.w,output.h));(void)read(output);
        frame.sun_color[0]=frame.sun_color[1]=frame.sun_color[2]=0;frame.sky_light[0]=frame.sky_light[1]=frame.sky_light[2]=0;
        frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=0;frame.time=1e6;frame.sun[0]=frame.sun[1]=frame.sun[2]=0;
        assert(world_sky_draw(sky,frame,output.fbo,output.w,output.h));(void)read(output);
        assert(world_sky_draw(sky,frame,reflection.fbo,reflection.w,reflection.h,1));const GLuint second=texture_binding(GL_TEXTURE_BINDING_2D);
        assert(second!=first&&second!=reflection.color);(void)read(reflection);
        stats=world_sky_stats(sky);assert(stats.target_allocations==2&&stats.target_resizes==0&&stats.noise_uploads==1);
        assert(stats.target_bytes==std::uint64_t(97*65+48*32)*8);
        assert(world_sky_draw(sky,frame,output.fbo,output.w,output.h));assert(texture_binding(GL_TEXTURE_BINDING_2D)==first);
        assert(world_sky_draw(sky,frame,reflection.fbo,reflection.w,reflection.h,1));assert(texture_binding(GL_TEXTURE_BINDING_2D)==second);
        assert(!world_sky_draw(sky,frame,output.fbo,0,64));assert(!world_sky_draw(sky,frame,output.fbo,64,64,WORLD_SKY_VIEW_COUNT));
        frame.time=std::numeric_limits<float>::quiet_NaN();assert(!world_sky_draw(sky,frame,output.fbo,64,64));frame.time=0;
        assert(world_sky_stats(sky).target_allocations==2);
        // Local cubemap captures own a third tiny cloud target. Six faces and
        // interleaved main/water draws must retain every original allocation,
        // rather than resizing the water sky to64² and back during walking.
        assert(world_sky_draw(sky,frame,reflection.fbo,reflection.w,reflection.h,WORLD_SKY_WATER_VIEW));
        const auto water_before=read(reflection);
        auto probe_frame=frame;probe_frame.sun_disc=false;
        assert(world_sky_draw(sky,probe_frame,probe.fbo,128,128,WORLD_SKY_PROBE_VIEW));
        const GLuint third=texture_binding(GL_TEXTURE_BINDING_2D);assert(third!=first&&third!=second&&third!=probe.color);
        stats=world_sky_stats(sky);assert(stats.target_allocations==3&&stats.target_resizes==0);
        assert(stats.width[0]==97&&stats.height[0]==65&&stats.width[1]==48&&stats.height[1]==32);
        assert(stats.width[2]==64&&stats.height[2]==64&&stats.target_bytes==std::uint64_t(97*65+48*32+64*64)*8);
        const auto probe_reference=read(probe);
        for(unsigned face=0;face<6;face++) {
            assert(world_sky_draw(sky,frame,output.fbo,output.w,output.h,WORLD_SKY_MAIN_VIEW));assert(texture_binding(GL_TEXTURE_BINDING_2D)==first);
            assert(world_sky_draw(sky,frame,reflection.fbo,reflection.w,reflection.h,WORLD_SKY_WATER_VIEW));
            assert(texture_binding(GL_TEXTURE_BINDING_2D)==second&&difference(water_before,read(reflection))==0);
            assert(world_sky_draw(sky,probe_frame,probe.fbo,probe.w,probe.h,WORLD_SKY_PROBE_VIEW));
            assert(texture_binding(GL_TEXTURE_BINDING_2D)==third&&difference(probe_reference,read(probe))==0);
        }
        stats=world_sky_stats(sky);assert(stats.target_allocations==3&&stats.target_resizes==0&&stats.noise_uploads==1);
        assert(uniform_queries==cached_uniform_queries);
        assert(!world_sky_draw(sky,probe_frame,probe.fbo,8193,128,WORLD_SKY_PROBE_VIEW));
        assert(!world_sky_draw(sky,probe_frame,probe.fbo,8192,8192,WORLD_SKY_PROBE_VIEW));
        assert(world_sky_draw(sky,frame,output.fbo,191,127));const GLuint resized=texture_binding(GL_TEXTURE_BINDING_2D);
        assert(resized!=first&&!glIsTexture(first));stats=world_sky_stats(sky);assert(stats.target_allocations==4&&stats.target_resizes==1);
        assert(glIsTexture(third)&&stats.width[2]==64&&stats.height[2]==64);
        std::printf("Sky invariant errors: wind=%g parallax=%g; target=%llu noise=%llu bytes; allocations=%llu resize=%llu\n",anchored_error,parallax,
            static_cast<unsigned long long>(stats.target_bytes),static_cast<unsigned long long>(stats.noise_bytes),
            static_cast<unsigned long long>(stats.target_allocations),static_cast<unsigned long long>(stats.target_resizes));
        if(std::getenv("VALWOOD_SKY_PREVIEW")) {
            Target preview(768,432);WorldSkyFrame view;
            view.eye[1]=200;view.forward[1]=.38f;view.forward[2]=.924986f;view.up[1]=.924986f;view.up[2]=-.38f;
            view.sun[0]=.4f;view.sun[1]=.5f;view.sun[2]=.768115f;view.sun_color[0]=1.15f;view.sun_color[1]=1.06f;view.sun_color[2]=.9f;
            view.sky_light[0]=.28f;view.sky_light[1]=.32f;view.sky_light[2]=.42f;
            for(int i=0;i<3;++i) {
                view.cloud=i==0?.28f:(i==1?.55f:.8f);
                assert(world_sky_draw(sky,view,preview.fbo,preview.w,preview.h));
                char path[128];std::snprintf(path,sizeof(path),"/tmp/valwood-sky-preview-%d.ppm",i);ppm(path,preview,read(preview));
            }
        }
        world_sky_free(sky);sky=nullptr;
        assert(!glIsTexture(noise)&&!glIsTexture(second)&&!glIsTexture(third)&&!glIsTexture(resized));assert(glGetError()==GL_NO_ERROR);
    }
    assert(uniform_queries==cached_uniform_queries);
    check_dead_work_equivalence(argc>2?argv[2]:nullptr);
    world_sky_free(nullptr);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    std::puts("world_sky_gl_test: PASS (actual volume, overcast/no-sun-leak/smooth-transition/horizon/cheap-fallback, clouds-off main/reflection/default/cached-locations, finite/clear/wind/parallax/high-altitude/night/resource/state/cleanup)");
}

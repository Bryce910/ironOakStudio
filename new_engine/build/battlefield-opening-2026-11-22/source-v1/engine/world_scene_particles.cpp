#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_scene_particles.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <new>
#include <sstream>
#include <string>
#include <vector>

struct WorldSceneParticleInstance {std::array<float,4> center_size,motion,style,tint;};
namespace {
constexpr double tau=6.2831853071795864769;
using Instance=WorldSceneParticleInstance;
static_assert(sizeof(Instance)==64,"Particle instance stays64bytes");
struct Visible {std::size_t index;unsigned requested;double distance;};
std::uint32_t hash(std::uint32_t n){n^=n>>16;n*=0x7feb352du;n^=n>>15;n*=0x846ca68bu;n^=n>>16;return n;}
double unit(std::uint32_t n){return double(hash(n)>>8)/16777216.;}
double smooth(double a,double b,double x){x=std::max(0.,std::min(1.,(x-a)/(b-a)));return x*x*(3.-2.*x);}
bool size_valid(int w,int h){return w>0&&h>0&&w<=8192&&h<=8192&&std::uint64_t(w)*h<=4096ull*4096ull;}
bool emitters_valid(const WorldSceneParticleEmitter *e,std::size_t n) {
    if(n>WORLD_SCENE_PARTICLE_EMITTER_CAPACITY||(n&&!e))return false;
    for(std::size_t i=0;i<n;i++) {
        const auto &a=e[i];
        for(double v:a.position)if(!std::isfinite(v)||std::abs(v)>1.e8)return false;
        if(unsigned(a.kind)>unsigned(WorldSceneParticleKind::Fireflies))return false;
        if(!std::isfinite(a.radius)||a.radius<.12f||a.radius>15.f||!std::isfinite(a.height)||a.height<.15f||a.height>30.f||
           !std::isfinite(a.strength)||a.strength<0||a.strength>4||!std::isfinite(a.warmth)||a.warmth<0||a.warmth>1||
           !std::isfinite(a.ceiling_y)||a.ceiling_y<a.position[1]+.15||a.ceiling_y>1.e8)return false;
        for(float v:a.tint)if(!std::isfinite(v)||v<0||v>1)return false;
        if(!std::isfinite(a.glow)||a.glow<0||a.glow>8)return false;
        for(float v:{a.rate,a.lifetime,a.size,a.wind})if(!std::isfinite(v)||v<0||v>4)return false;
        if(a.rate<.25||a.rate>2||a.lifetime<.25||a.size<.25||a.size>2||a.wind>2)return false;
    }
    return true;
}
bool frame_valid(const WorldSceneParticlesFrame &f) {
    if(!f.color||!f.depth||f.color==f.depth||!size_valid(f.width,f.height)||!std::isfinite(f.time)||std::abs(f.time)>1.e15||
       !std::isfinite(f.near_plane)||!std::isfinite(f.far_plane)||f.near_plane<.01||f.far_plane<=f.near_plane||f.far_plane>1.e7||
       !std::isfinite(f.haze)||f.haze<0||f.haze>1||f.smoke_budget>512||f.ambient_budget>512||f.smoke_budget+f.ambient_budget>512||
       !std::isfinite(f.smoke_distance)||f.smoke_distance<10||f.smoke_distance>300||
       !std::isfinite(f.life_distance)||f.life_distance<5||f.life_distance>180||
       !std::isfinite(f.hour)||f.hour<0||f.hour>24||!std::isfinite(f.tan_half_fov)||f.tan_half_fov<.05||f.tan_half_fov>4)return false;
    bool nonzero=false;
    for(float v:f.vp){if(!std::isfinite(v))return false;nonzero=nonzero||v!=0;}
    if(!nonzero)return false;
    for(double v:f.camera)if(!std::isfinite(v)||std::abs(v)>1.e8)return false;
    for(float v:f.light_vp)if(!std::isfinite(v))return false;
    for(float v:f.wind_xz)if(!std::isfinite(v)||std::abs(v)>30)return false;
    const float *basis[]={f.right,f.up,f.forward};
    for(unsigned i=0;i<3;i++) {
        double length=0;
        for(unsigned k=0;k<3;k++){if(!std::isfinite(basis[i][k]))return false;length+=double(basis[i][k])*basis[i][k];}
        if(std::abs(length-1.)>.03)return false;
        for(unsigned j=0;j<i;j++){double dot=0;for(unsigned k=0;k<3;k++)dot+=double(basis[i][k])*basis[j][k];if(std::abs(dot)>.03)return false;}
    }
    double sun_length=0;for(float v:f.sun){if(!std::isfinite(v))return false;sun_length+=double(v)*v;}
    if(std::abs(sun_length-1.)>.03)return false;
    const float *colors[]={f.sun_color,f.sky_light,f.ground_light,f.fog};
    for(const auto *c:colors)for(int k=0;k<3;k++)if(!std::isfinite(c[k])||c[k]<0||c[k]>64)return false;
    return true;
}
// Pure bounded staging is shared by production and the CPU/GL fixtures.
// An emitter's radius includes every rotated sprite corner, even at maximum
// wind. Smoke is transported by the wind sampled over its actual age, rather
// than rotating an entire old plume whenever today's gust changes direction.
std::size_t stage(const std::array<WorldSceneParticleEmitter,WORLD_SCENE_PARTICLE_EMITTER_CAPACITY> &emitters,
                  std::size_t count,const WorldSceneParticlesFrame &f,
                  std::array<Instance,WORLD_SCENE_PARTICLE_CAPACITY> &out,unsigned &visible_count) {
    std::array<Visible,WORLD_SCENE_PARTICLE_EMITTER_CAPACITY> visible={};
    std::size_t n=0;unsigned requested_totals[2]={};
    const double tv=f.tan_half_fov,th=tv*f.width/f.height,ph=std::hypot(1.,th),pv=std::hypot(1.,tv);
    const double dusk=std::max(smooth(17.5,20.,f.hour),1.-smooth(4.5,7.,f.hour));
    for(std::size_t i=0;i<count;i++) {
        const auto &e=emitters[i];if(e.strength<=0)continue;
        const bool smoke=e.kind==WorldSceneParticleKind::Smoke;
        if(e.kind==WorldSceneParticleKind::Fireflies&&dusk<=.001)continue;
        const unsigned budget=smoke?f.smoke_budget:f.ambient_budget;
        if(!budget)continue;
        const double height=std::min(double(e.height),e.ceiling_y-e.position[1]);
        const double x=e.position[0]-f.camera[0],y=e.position[1]+height*.5-f.camera[1],z=e.position[2]-f.camera[2];
        const double radius=std::hypot(double(e.radius),height*.5),distance=std::sqrt(x*x+y*y+z*z);
        const double limit=smoke?f.smoke_distance:std::min(double(f.life_distance),e.kind==WorldSceneParticleKind::Dust?45.:180.);
        if(distance>limit+radius)continue;
        const double along=x*f.forward[0]+y*f.forward[1]+z*f.forward[2];
        const double across=x*f.right[0]+y*f.right[1]+z*f.right[2],up=x*f.up[0]+y*f.up[1]+z*f.up[2];
        if(along+radius<f.near_plane||along-radius>f.far_plane||std::abs(across)>std::max(0.,along)*th+radius*ph||
           std::abs(up)>std::max(0.,along)*tv+radius*pv)continue;
        const unsigned detail=smoke?std::clamp(budget/5u,10u,48u):std::clamp(budget/4u,6u,28u);
        const unsigned requested=std::max(1u,unsigned(std::lround(detail*e.rate)));
        visible[n++]={i,requested,distance};requested_totals[smoke?0:1]+=requested;
    }
    visible_count=unsigned(n);if(!n)return 0;
    // Stable nearest-first order only breaks rounding ties. Both classes have
    // independent quotas, so a city skyline cannot starve the local camp.
    std::sort(visible.begin(),visible.begin()+n,[](const Visible &a,const Visible &b){return a.distance!=b.distance?a.distance<b.distance:a.index<b.index;});
    const unsigned budgets[2]={f.smoke_budget,f.ambient_budget};
    unsigned assigned[2]={},prefix[2]={};
    std::size_t used=0;
    for(std::size_t v=0;v<n;v++) {
        const auto &item=visible[v];const auto &e=emitters[item.index];
        const bool smoke=e.kind==WorldSceneParticleKind::Smoke;
        const unsigned category=smoke?0:1;
        prefix[category]+=item.requested;
        const unsigned end=unsigned(std::min<double>(prefix[category],std::floor(double(prefix[category])*budgets[category]/std::max(1u,requested_totals[category]))));
        const unsigned particles=end-assigned[category];assigned[category]=end;
        const double height=std::min(double(e.height),e.ceiling_y-e.position[1]);
        const double phase=std::remainder(f.time,64.)/64.;
        const double limit=smoke?f.smoke_distance:std::min(double(f.life_distance),e.kind==WorldSceneParticleKind::Dust?45.:180.);
        const double distance_fade=1.-smooth(limit*.72,limit,item.distance);
        for(unsigned p=0;p<particles&&used<out.size();p++) {
            const std::uint32_t seed=hash(e.seed^(p*0x9e3779b9u));
            const double cycles=std::max(1.,std::round((smoke?8.+double(e.seed%3):4.+double(seed%7))/e.lifetime));
            const double life_seconds=64./cycles;
            const double offset=smoke?unit(e.seed+1)+double(p)/std::max(1u,particles)+unit(seed+1)*.004:unit(seed+1);
            const double t=phase*cycles+offset,age=t-std::floor(t);
            const double angle=unit(seed+2)*tau,turn=phase*tau*double(1+seed%3);
            double half_size=0,energy=1,x=0,y=0,z=0;
            if(smoke) {
                half_size=std::min({double(e.radius)*(.055+.29*std::pow(age,.9))*e.size,height*.12,double(e.radius)*.36});
                const double diagonal=half_size*std::sqrt(2.);
                // Three trapezoidal samples of the existing local wind field
                // integrate the puff's trajectory. No retained particles,
                // heap allocation or per-puff terrain/physics query is needed.
                const double elapsed=age*life_seconds;
                double wind_x=0,wind_z=0;
                for(unsigned tap=0;tap<3;++tap) {
                    float wind[2]={f.wind_xz[0],f.wind_xz[1]};
                    const double sample_time=f.time-elapsed+elapsed*.5*tap;
                    if(f.local_wind)f.local_wind(f.wind_userdata,e.position[0],e.position[1]+height*age*.5*tap,e.position[2],sample_time,wind);
                    const double weight=tap==1?.5:.25;
                    const double speed=std::hypot(double(wind[0]),double(wind[1]));
                    if(std::isfinite(speed)) {const double clamp=3./std::max(3.,speed);wind_x+=wind[0]*clamp*weight;wind_z+=wind[1]*clamp*weight;}
                }
                const double spread=e.radius*(.035+.14*std::pow(age,1.4));
                const double sway=e.radius*.11*age*age;
                x=std::cos(angle+age*1.8)*spread+wind_x*elapsed*.30*e.wind+sway*std::sin(turn+age*3.+e.seed);
                z=std::sin(angle+age*1.8)*spread+wind_z*elapsed*.30*e.wind+sway*std::cos(turn+age*2.7+e.seed*.7);
                const double travel=std::hypot(x,z),available=std::max(0.,double(e.radius)-diagonal-.001);
                if(travel>.00001) {const double bounded=available*std::tanh(travel/std::max(available,.0001))/travel;x*=bounded;z*=bounded;}
                y=diagonal+(height-2.*diagonal)*age;
            } else if(e.kind==WorldSceneParticleKind::Dust||e.kind==WorldSceneParticleKind::Fireflies) {
                const bool fly=e.kind==WorldSceneParticleKind::Fireflies;
                const double radial=std::sqrt(unit(seed+3))*e.radius*.72;
                float wind[2]={f.wind_xz[0],f.wind_xz[1]};
                if(f.local_wind)f.local_wind(f.wind_userdata,e.position[0],e.position[1],e.position[2],f.time,wind);
                const double drift=std::min(e.radius*.08,std::hypot(double(wind[0]),double(wind[1]))*.04)*e.wind;
                x=std::cos(angle+std::sin(turn+seed%17)*.13)*radial+drift*std::sin(turn+angle);
                z=std::sin(angle+std::sin(turn+seed%17)*.13)*radial+drift*std::cos(turn+angle);
                const double physical=(fly?.009+.005*unit(seed+4):.003+.006*unit(seed+4))*e.size;
                const double center_y=height*(.10+.80*unit(seed+5))+(fly?.08:.025)*height*std::sin(turn+angle);
                const double along=(e.position[0]+x-f.camera[0])*f.forward[0]+(e.position[1]+center_y-f.camera[1])*f.forward[1]+(e.position[2]+z-f.camera[2])*f.forward[2];
                const double pixel_half=std::max(.0,along)*2*f.tan_half_fov/f.height*.52;
                half_size=std::min(std::min(physical*4.,std::max(physical,pixel_half)),std::min(double(e.radius)*.08,height*.04));
                energy=physical*physical/(half_size*half_size);
                if(fly)energy*=dusk*(.30+.70*std::pow(.5+.5*std::sin(turn*2.+angle),3.));
                const double diagonal=half_size*std::sqrt(2.);
                y=std::max(diagonal,std::min(height-diagonal,center_y));
            } else if(e.kind==WorldSceneParticleKind::Leaves) {
                half_size=std::min({double(e.radius)*.018,height*.04,(.025+.028*unit(seed+4))*e.size});
                const double diagonal=half_size*std::sqrt(2.);
                const double radial=(e.radius-diagonal)*.72*std::sqrt(unit(seed+3));
                float wind[2]={f.wind_xz[0],f.wind_xz[1]};
                if(f.local_wind)f.local_wind(f.wind_userdata,e.position[0],e.position[1],e.position[2],f.time,wind);
                const double breeze=std::hypot(double(wind[0]),double(wind[1]));
                const double lean=e.radius*.16*std::tanh(breeze*.35)*e.wind;
                x=std::cos(angle)*radial+std::sin(turn+age*9.)*half_size*2.+(breeze>.001?wind[0]/breeze*lean*(age-.5):0.);
                z=std::sin(angle)*radial+std::cos(turn*2.+age*11.)*half_size*2.+(breeze>.001?wind[1]/breeze*lean*(age-.5):0.);
                const double travel=std::hypot(x,z),available=e.radius-diagonal-.001;
                if(travel>available){x*=available/travel;z*=available/travel;}
                y=diagonal+(height-2.*diagonal)*(1.-age);
            } else {
                const bool spark=e.kind==WorldSceneParticleKind::Sparks;
                half_size=std::min({double(e.radius)*.10,height*.07,(spark?.018:.065)*e.size*(.6+.7*unit(seed+4))});
                const double radial=e.radius*(spark?.45:.60)*std::sqrt(unit(seed+3));
                x=std::cos(angle+turn*(spark?.2:1.))*radial;z=std::sin(angle+turn*(spark?.2:1.))*radial;
                const double diagonal=half_size*std::sqrt(2.);
                y=diagonal+(height-2.*diagonal)*(spark?age:(.5+.38*std::sin(turn+angle)));
            }
            const bool leaf=e.kind==WorldSceneParticleKind::Leaves,fly=e.kind==WorldSceneParticleKind::Fireflies;
            const double opacity=smooth(0.,smoke?.045:.12,age)*(1.-smooth(smoke?.72:.80,1.,age))*e.strength*distance_fade*(smoke?.42:leaf?.88:fly?.62:.18);
            if(opacity*energy<.0001)continue;
            Instance a;
            a.center_size={float(e.position[0]+x-f.camera[0]),float(e.position[1]+y-f.camera[1]),float(e.position[2]+z-f.camera[2]),float(half_size)};
            a.motion={float(angle+turn),float(age),float(opacity),float(unit(seed+6)*16.)};
            const double along=double(a.center_size[0])*f.forward[0]+double(a.center_size[1])*f.forward[1]+double(a.center_size[2])*f.forward[2];
            a.style={float(unsigned(e.kind)),e.warmth,float(energy),float(along)};
            a.tint={e.tint[0],e.tint[1],e.tint[2],e.glow};out[used++]=a;
        }
    }
    std::sort(out.begin(),out.begin()+used,[](const Instance &a,const Instance &b){if(a.style[3]!=b.style[3])return a.style[3]>b.style[3];return a.motion[3]<b.motion[3];});
    return used;
}
GLuint compile(GLenum type,const std::string &path) {
    std::ifstream file(path,std::ios::binary);if(!file){std::fprintf(stderr,"[SceneParticles] Cannot read %s\n",path.c_str());return 0;}
    std::ostringstream stream;stream<<file.rdbuf();const std::string source=stream.str();
    if(source.empty()||source.size()>1024*1024)return 0;
    GLuint result=glCreateShader(type);if(!result)return 0;
    const char *p=source.c_str();glShaderSource(result,1,&p,nullptr);glCompileShader(result);
    GLint ok=0;glGetShaderiv(result,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[4096]={};glGetShaderInfoLog(result,sizeof(log),nullptr,log);std::fprintf(stderr,"[SceneParticles] %s: %s\n",path.c_str(),log);glDeleteShader(result);return 0;}
    return result;
}
GLuint program(const std::string &vs_path,const std::string &fs_path) {
    GLuint vs=compile(GL_VERTEX_SHADER,vs_path),fs=compile(GL_FRAGMENT_SHADER,fs_path);
    if(!vs||!fs){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);return 0;}
    GLuint result=glCreateProgram();
    if(!result){glDeleteShader(vs);glDeleteShader(fs);return 0;}
    glAttachShader(result,vs);glAttachShader(result,fs);glLinkProgram(result);glDeleteShader(vs);glDeleteShader(fs);
    GLint ok=0;glGetProgramiv(result,GL_LINK_STATUS,&ok);
    if(!ok){char log[4096]={};glGetProgramInfoLog(result,sizeof(log),nullptr,log);std::fprintf(stderr,"[SceneParticles] link: %s\n",log);glDeleteProgram(result);return 0;}
    return result;
}
void bind(unsigned unit,GLuint texture){glActiveTexture(GL_TEXTURE0+unit);glBindTexture(GL_TEXTURE_2D,texture);}
}
struct WorldSceneParticles {
    GLuint program=0,copy=0,vao=0,buffer=0,fbo=0,color=0,smoke_mask=0;
    GLint smoke_sampler=-1,use_smoke=-1;
    GLint copy_color=-1,vp=-1,right=-1,up=-1,forward=-1,resolution=-1,near_far=-1,depth=-1,shadow=-1;
    GLint light_vp=-1,has_shadow=-1,sun=-1,sun_color=-1,sky=-1,ground=-1,fog=-1,time=-1,haze=-1;
    std::array<WorldSceneParticleEmitter,WORLD_SCENE_PARTICLE_EMITTER_CAPACITY> emitters={};
    std::array<WorldSceneParticleEmitter,8> environment={};
    std::array<Instance,WORLD_SCENE_PARTICLE_CAPACITY> staging={};
    std::size_t count=0,environment_count=0;double preview_time=-1;WorldSceneParticlesStats stats;
};
WorldSceneParticles *world_scene_particles_create(const char *directory) {
    if(!directory||!*directory||!SDL_GL_GetCurrentContext())return nullptr;
    auto *p=new(std::nothrow) WorldSceneParticles;if(!p)return nullptr;
    const std::string prefix=std::string(directory)+"/";
    p->program=program(prefix+"world_scene_particles.vs.glsl",prefix+"world_scene_particles.fs.glsl");
    p->copy=program(prefix+"world_post.vs.glsl",prefix+"world_scene_particles_copy.fs.glsl");
    if(!p->program||!p->copy){world_scene_particles_free(p);return nullptr;}
#define U(member,name) p->member=glGetUniformLocation(p->program,name)
    U(smoke_sampler,"uSmokeMask");U(use_smoke,"uUseSmokeMask");U(vp,"uVP");U(right,"uRight");U(up,"uUp");U(forward,"uForward");U(resolution,"uResolution");U(near_far,"uNearFar");
    U(depth,"uDepth");U(shadow,"uShadow");U(light_vp,"uLightVP");U(has_shadow,"uHasShadow");U(sun,"uSun");U(sun_color,"uSunColor");
    U(sky,"uSkyLight");U(ground,"uGroundLight");U(fog,"uFog");U(time,"uTimePhase");U(haze,"uHaze");
#undef U
    p->copy_color=glGetUniformLocation(p->copy,"uColor");
    glGenVertexArrays(1,&p->vao);glGenBuffers(1,&p->buffer);
    if(!p->vao||!p->buffer){world_scene_particles_free(p);return nullptr;}
    glBindVertexArray(p->vao);glBindBuffer(GL_ARRAY_BUFFER,p->buffer);glBufferData(GL_ARRAY_BUFFER,sizeof(p->staging),nullptr,GL_STREAM_DRAW);
    for(unsigned i=0;i<4;i++) {
        glEnableVertexAttribArray(i);glVertexAttribPointer(i,4,GL_FLOAT,GL_FALSE,sizeof(Instance),reinterpret_cast<void*>(std::uintptr_t(i*4*sizeof(float))));
        glVertexAttribDivisor(i,1);
    }
    glBindVertexArray(0);p->stats.gpu_buffer_bytes=sizeof(p->staging);return p;
}
std::size_t world_scene_particles_camp_life(double x,double ground_y,double z,
        WorldSceneParticleEmitter *out,std::size_t capacity) {
    if(!out||capacity<3||!std::isfinite(x)||!std::isfinite(ground_y)||!std::isfinite(z)||
       std::abs(x)>1.e8-20||std::abs(z)>1.e8-20||std::abs(ground_y)>1.e6)return 0;
    std::array<WorldSceneParticleEmitter,3> a{};
    a[0]={{x-9,ground_y+.07,z+5},WorldSceneParticleKind::Leaves,12,8,.55f,.15f,908641,ground_y+8.1};
    a[0].rate=.55f;a[0].lifetime=1.1f;a[0].wind=1.2f;a[0].tint[0]=.55f;a[0].tint[1]=.43f;a[0].tint[2]=.22f;
    a[1]={{x+4,ground_y+.3,z-8},WorldSceneParticleKind::Dust,10,5,.65f,.1f,908642,ground_y+5.4};
    a[1].rate=.7f;a[1].tint[0]=.9f;a[1].tint[1]=.85f;a[1].tint[2]=.73f;
    a[2]={{x-11,ground_y+.18,z+12},WorldSceneParticleKind::Fireflies,9,2.4f,.7f,0,908643,ground_y+2.7};
    a[2].rate=.6f;a[2].tint[0]=.9f;a[2].tint[1]=1;a[2].tint[2]=.35f;a[2].glow=3;
    if(!emitters_valid(a.data(),a.size()))return 0;
    std::copy(a.begin(),a.end(),out);return a.size();
}
bool world_scene_particles_load_smoke(WorldSceneParticles *p,const char *path) {
    if(!p||!path)return false;
    std::ifstream file(path,std::ios::binary);std::array<std::uint32_t,4> h={};
    file.read(reinterpret_cast<char*>(h.data()),sizeof(h));
    if(!file||h!=std::array<std::uint32_t,4>{0x31584656u,128,128,1})return false;
    std::array<unsigned char,128*128*4> pixels={};file.read(reinterpret_cast<char*>(pixels.data()),pixels.size());
    if(!file||file.peek()!=std::char_traits<char>::eof())return false;
    GLuint t=0;glGenTextures(1,&t);if(!t)return false;bind(0,t);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,128,128,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D);
    if(glGetError()!=GL_NO_ERROR){glDeleteTextures(1,&t);return false;}
    glDeleteTextures(1,&p->smoke_mask);p->smoke_mask=t;p->stats.purchased_smoke=true;p->stats.smoke_texture_bytes=87380;return true;
}
bool world_scene_particles_validate_emitters(const WorldSceneParticleEmitter *e,std::size_t n){return emitters_valid(e,n);}
bool world_scene_particles_set_emitters(WorldSceneParticles *p,const WorldSceneParticleEmitter *emitters,std::size_t count) {
    if(!p||!emitters_valid(emitters,count))return false;
    if(count)std::copy_n(emitters,count,p->emitters.begin());
    p->count=count;p->stats.emitters=unsigned(count);return true;
}
bool world_scene_particles_set_environment_emitters(WorldSceneParticles *p,const WorldSceneParticleEmitter *emitters,std::size_t count) {
    if(!p||count>8||!emitters_valid(emitters,count))return false;
    if(count)std::copy_n(emitters,count,p->environment.begin());
    p->environment_count=count;return true;
}
bool world_scene_particles_prepare(WorldSceneParticles *p,int w,int h) {
    if(!p)return false;
    if(!size_valid(w,h)){p->stats.failures++;return false;}
    if(w==p->stats.width&&h==p->stats.height)return true;
    GLuint texture=0,fbo=0;glGenTextures(1,&texture);glGenFramebuffers(1,&fbo);
    if(!texture||!fbo){glDeleteTextures(1,&texture);glDeleteFramebuffers(1,&fbo);p->stats.failures++;return false;}
    bind(0,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,w,h,0,GL_RGBA,GL_FLOAT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){glDeleteTextures(1,&texture);glDeleteFramebuffers(1,&fbo);p->stats.failures++;return false;}
    glDeleteTextures(1,&p->color);glDeleteFramebuffers(1,&p->fbo);p->color=texture;p->fbo=fbo;
    p->stats.width=w;p->stats.height=h;p->stats.target_bytes=std::uint64_t(w)*h*8;p->stats.allocations++;return true;
}
void world_scene_particles_preview_time(WorldSceneParticles *p,double time){if(p&&std::isfinite(time))p->preview_time=time;}
unsigned world_scene_particles_resolve(WorldSceneParticles *p,const WorldSceneParticlesFrame &input) {
    auto f=input;if(p&&p->preview_time>=0)f.time=p->preview_time;
    if(!p)return f.color;
    p->stats.visible_emitters=p->stats.smoke_instances=p->stats.dust_instances=0;
    if(!frame_valid(f)||f.color==p->color||f.depth==p->color||f.shadow==p->color){p->stats.failures++;return f.color;}
    auto merged=p->emitters;
    const auto extra=std::min(p->environment_count,merged.size()-p->count);
    std::copy_n(p->environment.begin(),extra,merged.begin()+p->count);
    unsigned visible=0;const auto count=stage(merged,p->count+extra,f,p->staging,visible);
    if(!count)return f.color;
    if(!world_scene_particles_prepare(p,f.width,f.height))return f.color;
    glBindFramebuffer(GL_FRAMEBUFFER,p->fbo);glDrawBuffer(GL_COLOR_ATTACHMENT0);glViewport(0,0,f.width,f.height);
    glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);
    glDepthMask(GL_FALSE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glBindVertexArray(p->vao);glUseProgram(p->copy);bind(0,f.color);glUniform1i(p->copy_color,0);glDrawArrays(GL_TRIANGLES,0,3);
    glUseProgram(p->program);glBindBuffer(GL_ARRAY_BUFFER,p->buffer);glBufferSubData(GL_ARRAY_BUFFER,0,count*sizeof(Instance),p->staging.data());
    bind(0,f.depth);bind(1,f.shadow);bind(2,p->smoke_mask);
    glUniform1i(p->smoke_sampler,2);glUniform1i(p->use_smoke,p->smoke_mask!=0);glUniform1i(p->depth,0);glUniform1i(p->shadow,1);glUniform1i(p->has_shadow,f.shadow!=0);
    glUniformMatrix4fv(p->vp,1,GL_FALSE,f.vp);glUniformMatrix4fv(p->light_vp,1,GL_FALSE,f.light_vp);
    glUniform3fv(p->right,1,f.right);glUniform3fv(p->up,1,f.up);glUniform3fv(p->forward,1,f.forward);
    glUniform2f(p->resolution,float(f.width),float(f.height));glUniform2f(p->near_far,f.near_plane,f.far_plane);
    glUniform3fv(p->sun,1,f.sun);glUniform3fv(p->sun_color,1,f.sun_color);glUniform3fv(p->sky,1,f.sky_light);
    glUniform3fv(p->ground,1,f.ground_light);glUniform3fv(p->fog,1,f.fog);
    glUniform1f(p->time,float(std::remainder(f.time,64.)*(tau/64.)));glUniform1f(p->haze,f.haze);
    glEnable(GL_BLEND);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);glBlendFuncSeparate(GL_ONE,GL_ONE_MINUS_SRC_ALPHA,GL_ZERO,GL_ZERO);
    glDrawArraysInstanced(GL_TRIANGLES,0,6,GLsizei(count));glDisable(GL_BLEND);glActiveTexture(GL_TEXTURE0);
    p->stats.visible_emitters=visible;
    for(std::size_t i=0;i<count;i++)if(p->staging[i].style[0]<.5)p->stats.smoke_instances++;else p->stats.dust_instances++;
    p->stats.resolves++;p->stats.draws++;p->stats.instance_uploads++;return p->color;
}
WorldSceneParticlesStats world_scene_particles_stats(const WorldSceneParticles *p){return p?p->stats:WorldSceneParticlesStats{};}
bool world_scene_particles_warm(WorldSceneParticles *p,const WorldSceneParticlesFrame &f) {
    if(!p)return false;
    const auto saved=p->emitters[0];const auto count=p->count,environment_count=p->environment_count;
    const auto preview=p->preview_time;const auto stats=p->stats;
    WorldSceneParticleEmitter fixture;
    for(unsigned k=0;k<3;k++)fixture.position[k]=f.camera[k]+f.forward[k]*8.;
    fixture.position[1]-=1.;fixture.ceiling_y=fixture.position[1]+6;
    fixture.radius=.6f;fixture.height=4;fixture.strength=.8f;
    p->emitters[0]=fixture;p->count=1;p->environment_count=0;p->preview_time=-1;
    const bool ready=world_scene_particles_resolve(p,f)!=f.color;
    p->emitters[0]=saved;p->count=count;p->environment_count=environment_count;p->preview_time=preview;
    p->stats.emitters=stats.emitters;p->stats.visible_emitters=stats.visible_emitters;
    p->stats.smoke_instances=stats.smoke_instances;p->stats.dust_instances=stats.dust_instances;
    return ready;
}
void world_scene_particles_free(WorldSceneParticles *p) {
    if(!p)return;
    // A bound program remains a live GL object after deletion until detached.
    glUseProgram(0);glBindVertexArray(0);
    glDeleteTextures(1,&p->smoke_mask);glDeleteTextures(1,&p->color);glDeleteFramebuffers(1,&p->fbo);glDeleteBuffers(1,&p->buffer);glDeleteVertexArrays(1,&p->vao);
    glDeleteProgram(p->program);glDeleteProgram(p->copy);delete p;
}

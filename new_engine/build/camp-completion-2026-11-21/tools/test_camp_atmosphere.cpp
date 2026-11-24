// Standalone fixture includes the implementation to inspect its fixed staging
// and GL ownership without adding diagnostic API to the production renderer.
#include "../source-v1/engine/world_scene_particles.cpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

namespace {
WorldSceneParticlesFrame frame(int w=256,int h=128) {
    WorldSceneParticlesFrame f;f.width=w;f.height=h;f.color=1;f.depth=2;f.time=22;
    const float q=1.f/f.tan_half_fov,n=f.near_plane,z=f.far_plane;
    f.vp[0]=q*float(h)/w;f.vp[5]=q;f.vp[10]=-(z+n)/(z-n);f.vp[11]=-1;f.vp[14]=-2*z*n/(z-n);
    return f;
}
void cpu() {
    auto f=frame();assert(frame_valid(f));
    std::array<WorldSceneParticleEmitter,WORLD_SCENE_PARTICLE_EMITTER_CAPACITY> es={};
    for(unsigned i=0;i<es.size();i++) {
        auto &e=es[i];e.position[0]=(int(i%6)-3)*.2;e.position[1]=-1;e.position[2]=-6.-(i/6)*.2;
        e.radius=1.5;e.height=3;e.ceiling_y=2;e.seed=8911+i;e.kind=i%2?WorldSceneParticleKind::Dust:WorldSceneParticleKind::Smoke;
    }
    assert(emitters_valid(es.data(),es.size()));
    std::array<Instance,WORLD_SCENE_PARTICLE_CAPACITY> a={},b={};unsigned visible=0,vb=0;
    const auto na=stage(es,es.size(),f,a,visible);assert(na>350&&na<=512&&visible==48);
    auto same=f;same.time+=64;
    const auto nb=stage(es,es.size(),same,b,vb);assert(na==nb&&visible==vb&&std::memcmp(a.data(),b.data(),na*sizeof(Instance))==0);
    for(std::size_t i=1;i<na;i++)assert(a[i-1].style[3]>=a[i].style[3]);
    auto shifted=es;same=f;
    const double shift[3]={1000000,25000,-1000000};
    for(int k=0;k<3;k++)same.camera[k]+=shift[k];
    for(auto &e:shifted){for(int k=0;k<3;k++)e.position[k]+=shift[k];e.ceiling_y+=shift[1];}
    const auto nc=stage(shifted,shifted.size(),same,b,vb);assert(na==nc);
    for(std::size_t i=0;i<na;i++)for(unsigned k=0;k<4;k++)assert(std::abs(a[i].center_size[k]-b[i].center_size[k])<.000001f);
    // Exercise every orientation, wind extreme and age: all four actual quad
    // corners must remain inside the authored cylinder and ceiling volume.
    unsigned bounds_checks=0;
    for(unsigned mode=0;mode<6;mode++) {
        auto e=es[0];e.kind=WorldSceneParticleKind(mode);e.tint[0]=.2;e.tint[1]=.6;e.glow=4;
        std::array<WorldSceneParticleEmitter,WORLD_SCENE_PARTICLE_EMITTER_CAPACITY> one={};one[0]=e;
        for(unsigned t=0;t<1200;t++) {
            auto ff=f;ff.time=t*.073;ff.hour=21;ff.wind_xz[0]=30;ff.wind_xz[1]=-30;
            const auto nn=stage(one,1,ff,a,visible);assert(nn<=96);
            for(std::size_t j=0;j<nn;j++) {
                const auto &p=a[j];assert(p.tint[0]==e.tint[0]&&p.tint[1]==e.tint[1]&&p.tint[3]==e.glow);const double c=std::cos(p.motion[0]),s=std::sin(p.motion[0]);
                for(int x:{-1,1})for(int y:{-1,1}) {
                    const double px=p.center_size[0]+(c*x-s*y)*p.center_size[3]-e.position[0];
                    const double py=p.center_size[1]+(s*x+c*y)*p.center_size[3];
                    const double pz=p.center_size[2]-e.position[2];
                    assert(std::hypot(px,pz)<=e.radius+.00001&&py>=e.position[1]-.00001&&py<=e.ceiling_y+.00001);
                    bounds_checks++;
                }
            }
        }
    }
    for(unsigned kind=0;kind<2;kind++)for(double d:{14.,18.,30.,50.,70.,90.,160.}) {
        auto one=es;one[0].kind=kind?WorldSceneParticleKind::Dust:WorldSceneParticleKind::Smoke;
        auto before=f,after=f;before.camera[2]=d-6.-.001;after.camera[2]=d-6.+.001;
        const auto n0=stage(one,1,before,a,visible),n1=stage(one,1,after,b,vb);
        double opacity0=0,opacity1=0;
        for(size_t k=0;k<n0;k++)opacity0+=a[k].motion[2];
        for(size_t k=0;k<n1;k++)opacity1+=b[k].motion[2];
        assert(std::abs(opacity0-opacity1)<.003);
    }
    // Separate quality quotas, time of day and bounded instantaneous
    // advection are verified against the exact production staging path.
    std::array<WorldSceneParticleEmitter,3> life{};
    assert(world_scene_particles_camp_life(0,-1,-8,life.data(),3)==3);
    assert(!world_scene_particles_camp_life(0,-1,-8,life.data(),2));
    auto one=es;one[0]=es[0];one[0].position[2]=-8;one[0].rate=1.4;
    std::size_t previous=0;
    for(auto budget:{48u,96u,160u,224u}) {
        auto q=frame();q.smoke_budget=budget;q.ambient_budget=24;
        auto num=stage(one,1,q,a,visible);assert(num>previous&&num<=budget);previous=num;
    }
    one[0].kind=WorldSceneParticleKind::Fireflies;
    auto day=frame();day.hour=12;assert(stage(one,1,day,a,visible)==0);
    auto night=day;night.hour=21;assert(stage(one,1,night,a,visible)>0);
    one[0].kind=WorldSceneParticleKind::Smoke;
    auto calm=frame();calm.smoke_budget=160;calm.ambient_budget=96;calm.time=23;
    auto breeze=calm;breeze.wind_xz[0]=2.;
    const auto still=stage(one,1,calm,a,visible),moving=stage(one,1,breeze,b,vb);
    assert(still==moving&&still>0);double travel=0;
    for(size_t i=0;i<still;++i)travel+=b[i].center_size[0]-a[i].center_size[0];
    assert(travel>still*.15);
    // Invalid quality inputs are rejected before GL or staging work.
    auto bad_quality=frame();bad_quality.smoke_budget=513;assert(!frame_valid(bad_quality));
    bad_quality=frame();bad_quality.hour=25;assert(!frame_valid(bad_quality));
    std::cout<<"QUALITY PASS: tiered separate quotas, above-source smoke advection, day/night fireflies and source helper\n";
    f=frame();f.forward[0]=std::numeric_limits<float>::quiet_NaN();assert(!frame_valid(f));
    f=frame();f.vp[0]=std::numeric_limits<float>::infinity();assert(!frame_valid(f));
    f=frame();f.time=std::numeric_limits<double>::quiet_NaN();assert(!frame_valid(f));
    auto invalid=es;invalid[0].ceiling_y=-5;assert(!emitters_valid(invalid.data(),invalid.size()));
    assert(!emitters_valid(nullptr,1)&&!emitters_valid(es.data(),49)&&emitters_valid(nullptr,0));
    auto off=es;for(auto &e:off)e.position[2]=1000;f=frame();assert(stage(off,off.size(),f,a,visible)==0&&visible==0);
    std::cout<<"CPU PASS: fixed capacity, sorted deterministic64s/rebase, finite rejection, "<<bounds_checks<<" quad-corner volume witnesses\n";
}
GLuint texture(GLenum format,int w,int h,const float *data) {
    GLuint t=0;glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);glTexImage2D(GL_TEXTURE_2D,0,format,w,h,0,format==GL_R32F?GL_RED:GL_RGBA,GL_FLOAT,data);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);return t;
}
std::vector<float> read(GLuint t,int w,int h) {
    std::vector<float> p(std::size_t(w)*h*4);glBindTexture(GL_TEXTURE_2D,t);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,p.data());return p;
}
float encoded(const WorldSceneParticlesFrame &f,float z){return f.far_plane/(f.far_plane-f.near_plane)-f.far_plane*f.near_plane/((f.far_plane-f.near_plane)*z);}
void gl() {
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window=SDL_CreateWindow("Scene particle fixture",0,0,256,128,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);
    auto context=SDL_GL_CreateContext(window);assert(context);
    auto f=frame();const int w=f.width,h=f.height;std::vector<float> input(std::size_t(w)*h*4),depth(std::size_t(w)*h,encoded(f,100));
    for(std::size_t i=0;i<input.size();i+=4){input[i]=.2f;input[i+1]=.3f;input[i+2]=.4f;input[i+3]=(i/4)%2?.995f:.71f;}
    f.color=texture(GL_RGBA16F,w,h,input.data());f.depth=texture(GL_R32F,w,h,depth.data());const auto original=read(f.color,w,h);
    auto *p=world_scene_particles_create();assert(p);assert(world_scene_particles_stats(p).target_bytes==0);
    if(std::getenv("VALWOOD_TEST_PURCHASED_PARTICLES")) {
        assert(world_scene_particles_load_smoke(p,"particles/purchased_smoke.vfx"));
        assert(!world_scene_particles_load_smoke(p,"particles/missing.vfx"));
        assert(p->stats.purchased_smoke&&p->stats.smoke_texture_bytes==87380);
    }
    assert(world_scene_particles_resolve(p,f)==f.color&&p->stats.allocations==0&&p->stats.draws==0);
    WorldSceneParticleEmitter e;e.position[1]=-1;e.position[2]=-8;e.radius=2;e.height=3;e.ceiling_y=2;e.seed=527;
    assert(world_scene_particles_set_emitters(p,&e,1));auto bad=e;bad.strength=-1;assert(!world_scene_particles_set_emitters(p,&bad,1));assert(p->emitters[0].strength==e.strength);
    assert(world_scene_particles_prepare(p,w,h));const GLuint owned=p->color,owned_fbo=p->fbo,owned_buffer=p->buffer,owned_vao=p->vao,owned_program=p->program,owned_copy=p->copy;
    assert(world_scene_particles_prepare(p,w,h)&&p->stats.allocations==1&&p->stats.target_bytes==std::uint64_t(w)*h*8);
    assert(!world_scene_particles_prepare(p,-1,h)&&p->color==owned&&glIsTexture(owned));
    const auto output=world_scene_particles_resolve(p,f);assert(output==owned&&output!=f.color&&output!=f.depth);
    const auto far=read(output,w,h);unsigned changed=0,untouched=0;
    for(std::size_t i=0;i<far.size();i+=4) {
        if(far[i+3]==0){changed++;assert(std::isfinite(far[i])&&std::isfinite(far[i+1])&&std::isfinite(far[i+2]));}
        else{untouched++;assert(std::memcmp(&far[i],&original[i],4*sizeof(float))==0);}
    }
    assert(changed>80&&untouched>1000);assert(read(f.color,w,h)==original);
    const auto feedback=f;f.color=output;assert(world_scene_particles_resolve(p,f)==output);f=feedback;
    std::fill(depth.begin(),depth.end(),encoded(f,4));glBindTexture(GL_TEXTURE_2D,f.depth);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,w,h,GL_RED,GL_FLOAT,depth.data());
    assert(world_scene_particles_resolve(p,f)==output);assert(read(output,w,h)==original);
    // Intersection metric must fade as the opaque plane approaches the smoke.
    // Compare RGB coverage, because temporal metadata is intentionally binary.
    auto coverage=[&](float z){std::fill(depth.begin(),depth.end(),encoded(f,z));glBindTexture(GL_TEXTURE_2D,f.depth);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,w,h,GL_RED,GL_FLOAT,depth.data());world_scene_particles_resolve(p,f);const auto image=read(output,w,h);double sum=0;for(std::size_t i=0;i<image.size();i+=4)for(unsigned k=0;k<3;k++)sum+=std::abs(image[i+k]-original[i+k]);return sum;};
    const auto full=coverage(100),soft=coverage(8.2f),hidden=coverage(4);
    assert(full>0&&soft>0&&soft<full&&hidden==0);
    assert(p->stats.allocations==1&&p->stats.gpu_buffer_bytes==32768);
    // New colored HDR effects reach the actual GPU and respect the preview clock.
    std::fill(depth.begin(),depth.end(),encoded(f,100));glBindTexture(GL_TEXTURE_2D,f.depth);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,w,h,GL_RED,GL_FLOAT,depth.data());
    auto glow=e;glow.tint[0]=.05f;glow.tint[1]=.2f;glow.tint[2]=1;glow.glow=6;glow.strength=3;glow.size=2;
    for(auto kind:{WorldSceneParticleKind::Sparks,WorldSceneParticleKind::Wisps}){
        glow.kind=kind;assert(world_scene_particles_set_emitters(p,&glow,1));world_scene_particles_preview_time(p,13);world_scene_particles_resolve(p,f);const auto held=read(output,w,h);
        double red=0,blue=0;for(size_t i=0;i<held.size();i+=4){red+=held[i]-original[i];blue+=held[i+2]-original[i+2];}assert(blue>red+.01);
        auto later=f;later.time+=17;world_scene_particles_resolve(p,later);assert(read(output,w,h)==held);
        world_scene_particles_preview_time(p,-1);world_scene_particles_resolve(p,later);assert(read(output,w,h)!=held);
    }
    // Engine-owned life survives a Studio authored-set replacement and has
    // its own real shader silhouettes. Daytime fireflies contribute no pixels.
    WorldSceneParticleEmitter leaf=e;leaf.kind=WorldSceneParticleKind::Leaves;leaf.strength=1;leaf.size=2;
    assert(world_scene_particles_set_emitters(p,nullptr,0));
    assert(world_scene_particles_set_environment_emitters(p,&leaf,1));
    const auto leaf_output=world_scene_particles_resolve(p,f);assert(leaf_output==output);
    assert(read(output,w,h)!=original);
    auto invalid_leaf=leaf;invalid_leaf.radius=-1;
    assert(!world_scene_particles_set_environment_emitters(p,&invalid_leaf,1)&&p->environment_count==1);
    auto fly=leaf;fly.kind=WorldSceneParticleKind::Fireflies;fly.glow=3;fly.tint[0]=.7;fly.tint[1]=1;fly.tint[2]=.1;
    assert(world_scene_particles_set_environment_emitters(p,&fly,1));f.hour=12;
    assert(world_scene_particles_resolve(p,f)==f.color);
    f.hour=21;assert(world_scene_particles_resolve(p,f)==output&&read(output,w,h)!=original);
    const auto env=p->environment[0];
    assert(world_scene_particles_warm(p,f)&&p->environment_count==1&&!std::memcmp(&env,&p->environment[0],sizeof env));
    assert(world_scene_particles_set_environment_emitters(p,nullptr,0));f.hour=12;
    std::cout<<"GL PASS ambient life: opaque leaf silhouettes and night-only fireflies survive authoredset replacement, atomicinvalidupdate and warmup restoration\n";
    // Loading cannot rely on a particular authored chimney remaining visible.
    assert(world_scene_particles_set_emitters(p,nullptr,0));
    assert(world_scene_particles_warm(p,f)&&p->count==0&&p->stats.emitters==0);
    assert(world_scene_particles_resolve(p,f)==f.color);
    auto distant=e;distant.position[2]=5000;
    assert(world_scene_particles_set_emitters(p,&distant,1));world_scene_particles_preview_time(p,13);
    assert(world_scene_particles_warm(p,f)&&p->count==1&&p->stats.emitters==1&&p->preview_time==13);
    assert(!std::memcmp(&p->emitters[0],&distant,sizeof distant));
    assert(world_scene_particles_resolve(p,f)==f.color);
    auto invalid=f;invalid.color=0;assert(!world_scene_particles_warm(p,invalid));
    assert(p->count==1&&p->preview_time==13&&!std::memcmp(&p->emitters[0],&distant,sizeof distant));
    std::cout<<"GL PASS loading warm-up: empty/distant authored emitters, clock and source preserved, invalid frame rejected\n";
    assert(glGetError()==GL_NO_ERROR);world_scene_particles_free(p);
    assert(!glIsTexture(owned)&&!glIsFramebuffer(owned_fbo)&&!glIsBuffer(owned_buffer)&&!glIsVertexArray(owned_vao)&&!glIsProgram(owned_program)&&!glIsProgram(owned_copy));
    glDeleteTextures(1,&f.color);glDeleteTextures(1,&f.depth);assert(glGetError()==GL_NO_ERROR);
    std::cout<<"GL PASS: "<<changed<<" moving pixels, "<<untouched<<" bit-exact untouched pixels, occlusion/soft fade "<<full<<"/"<<soft<<"/"<<hidden<<", feedback rejection, stable reuse, owners0\n";
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
}
}
int main(int argc,char **argv){cpu();if(argc>1&&std::string(argv[1])=="--gl")gl();}

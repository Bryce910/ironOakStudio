/* Synthetic startup capture and binding contracts, not a native FPS claim. */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_local_probes.h"
#include "engine/world_program_epoch.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <new>
#include <sstream>
#include <string>

static std::atomic<unsigned> allocations{0};
// Driver compiler/queue workers may allocate concurrently on native GL. The
// module's hot-path contract concerns its invoking render thread, so measure
// that thread strictly inside bind calls, with a process-wide diagnostic too.
static thread_local bool track_caller_allocations=false;
static thread_local unsigned caller_allocations=0;
void *operator new(std::size_t n) {
    allocations++;
    if(track_caller_allocations)caller_allocations++;
    if(void *p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
void *operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void *p) noexcept {std::free(p);}
void operator delete[](void *p) noexcept {std::free(p);}
void operator delete(void *p,std::size_t) noexcept {std::free(p);}
void operator delete[](void *p,std::size_t) noexcept {std::free(p);}
namespace {
using Pixel=std::array<float,4>;
constexpr Pixel colors[3]={{{.4f,.2f,.1f,1}},{{.1f,.3f,.5f,1}},{{.2f,.4f,.6f,1}}};
GLuint shader(GLenum type,const char *source) {
    const GLuint s=glCreateShader(type);glShaderSource(s,1,&source,nullptr);glCompileShader(s);GLint okay=0;glGetShaderiv(s,GL_COMPILE_STATUS,&okay);
    if(!okay){char log[8192]={};glGetShaderInfoLog(s,sizeof(log),nullptr,log);std::fprintf(stderr,"%s\n",log);}assert(okay);return s;
}
GLuint program(bool cubes) {
    const char *vs="#version 330 core\nvoid main(){vec2 q=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(q*2.-1.,0,1);}";
    const std::string fs=std::string(R"GLSL(#version 330 core
uniform vec3 uLocalIrradiance[9];uniform float uLocalIrradianceWeight;
uniform vec4 uLocalIrradianceBounds;uniform vec2 uLocalIrradianceHeights;
uniform vec3 uLocalIrradianceSecondary[9];uniform float uLocalIrradianceSecondaryWeight;
uniform vec4 uLocalIrradianceSecondaryBounds;uniform vec2 uLocalIrradianceSecondaryHeights;
out vec4 color;
)GLSL")+(cubes?R"GLSL(
uniform samplerCube uLocalProbe0,uLocalProbe1;
uniform int uLocalProbeCount;
uniform vec3 uLocalProbeCenter[2],uLocalProbeBoxMin[2],uLocalProbeBoxMax[2];
uniform vec2 uLocalProbeWeights;uniform float uLocalProbeMip,uLocalProbeLightScale;
)GLSL":"")+R"GLSL(
void main(){
    float metadata=uLocalIrradianceWeight+dot(uLocalIrradianceBounds,vec4(1))+dot(uLocalIrradianceHeights,vec2(1));
    metadata+=uLocalIrradianceSecondaryWeight+dot(uLocalIrradianceSecondaryBounds,vec4(1))+dot(uLocalIrradianceSecondaryHeights,vec2(1));
    for(int i=0;i<9;i++)metadata+=dot(uLocalIrradiance[i]+uLocalIrradianceSecondary[i],vec3(1));
)GLSL"+(cubes?R"GLSL(
    for(int i=0;i<2;i++)metadata+=dot(uLocalProbeCenter[i]+uLocalProbeBoxMin[i]+uLocalProbeBoxMax[i],vec3(1));
    vec3 reflected=vec3(0);
    if(uLocalProbeCount>0)reflected+=textureLod(uLocalProbe0,vec3(1,.2,.1),uLocalProbeMip).rgb*uLocalProbeWeights.x;
    if(uLocalProbeCount>1)reflected+=textureLod(uLocalProbe1,vec3(-.1,.2,1),uLocalProbeMip).rgb*uLocalProbeWeights.y;
    color=vec4(reflected*uLocalProbeLightScale,metadata);
)GLSL":"color=vec4(vec3(uLocalIrradianceWeight),metadata);\n")+"}\n";
    const GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs.c_str()),p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);
    GLint okay=0;glGetProgramiv(p,GL_LINK_STATUS,&okay);assert(okay);glDeleteShader(v);glDeleteShader(f);return p;
}
float uniform(GLuint p,const char *name) {float f=-100;glGetUniformfv(p,glGetUniformLocation(p,name),&f);return f;}
int count(GLuint p) {GLint n=-1;glGetUniformiv(p,glGetUniformLocation(p,"uLocalProbeCount"),&n);return n;}
std::array<GLint,2> cube_bindings(bool complete,int side=16) {
    GLint active=0;glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
    std::array<GLint,2> textures={};
    for(unsigned slot=0;slot<2;slot++) {
        glActiveTexture(GL_TEXTURE4+slot);glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP,&textures[slot]);
        if(!complete){assert(textures[slot]==0);continue;}
        assert(textures[slot]>0&&glIsTexture(GLuint(textures[slot])));
        GLint filter=0,max_level=0;glGetTexParameteriv(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,&filter);
        glGetTexParameteriv(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAX_LEVEL,&max_level);
        assert(filter==GL_LINEAR_MIPMAP_LINEAR&&max_level==int(std::log2(side)));
        for(int level=0,n=side;n;level++,n/=2)for(unsigned face=0;face<6;face++) {
            GLint width=0,height=0,format=0;const GLenum target=GL_TEXTURE_CUBE_MAP_POSITIVE_X+face;
            glGetTexLevelParameteriv(target,level,GL_TEXTURE_WIDTH,&width);
            glGetTexLevelParameteriv(target,level,GL_TEXTURE_HEIGHT,&height);
            glGetTexLevelParameteriv(target,level,GL_TEXTURE_INTERNAL_FORMAT,&format);
            assert(width==n&&height==n&&format==GL_RGB16F);
        }
    }
    glActiveTexture(GLenum(active));return textures;
}
struct Fixture {WorldLocalProbes *module=nullptr;GLuint p=0,fbo=0,depth=0;unsigned faces=0;};
bool capture(const WorldLocalProbeCapture &f,void *data) {
    auto &test=*static_cast<Fixture *>(data);assert(f.side==16&&f.index<3&&f.face<6);
    float forward[3],up[3],right[3];assert(world_local_probe_face_basis(f.face,forward,up,right));
    for(int k=0;k<3;k++)assert(forward[k]==f.forward[k]&&up[k]==f.up[k]&&right[k]==f.right[k]);
    assert(f.eye[1]==2&&f.eye[2]==0);assert(f.eye[0]==(f.index==0?-2:(f.index==1?2:0)));
    test.fbo=f.framebuffer;GLint depth=0;glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,&depth);test.depth=GLuint(depth);
    glUseProgram(test.p);assert(world_local_probes_bind(test.module,test.p,f.eye));
    assert(count(test.p)==0&&uniform(test.p,"uLocalIrradianceWeight")==0&&uniform(test.p,"uLocalIrradianceSecondaryWeight")==0); // capture cannot feed back
    (void)cube_bindings(false); // Neither sampler can retain a capture target.
    glClearColor(colors[f.index][0],colors[f.index][1],colors[f.index][2],1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);test.faces++;
    glBindFramebuffer(GL_FRAMEBUFFER,0); // Module must rebind before its readback.
    return true;
}
bool failed_capture(const WorldLocalProbeCapture &,void *) {return false;}
Pixel draw(GLuint target,int side=8) {
    glBindFramebuffer(GL_FRAMEBUFFER,target);glViewport(0,0,side,side);glDrawArrays(GL_TRIANGLES,0,3);Pixel p;
    glReadPixels(4,4,1,1,GL_RGBA,GL_FLOAT,p.data());for(float f:p)assert(std::isfinite(f));return p;
}
std::string function(const std::string &source,const char *name) {
    std::size_t search=0;
    while(true){const auto begin=source.find(name,search);assert(begin!=std::string::npos);
        auto end=source.find('{',begin);const auto semicolon=source.find(';',begin);
        // The production shaders may forward-declare this helper for an HDR
        // function above its definition. Never capture the next function's body.
        if(semicolon!=std::string::npos&&(end==std::string::npos||semicolon<end)){search=semicolon+1;continue;}
        assert(end!=std::string::npos);unsigned depth=1;
        for(++end;end<source.size()&&depth;end++){if(source[end]=='{')depth++;else if(source[end]=='}')depth--;}
        assert(!depth);return source.substr(begin,end-begin)+"\n";
    }
}
GLuint diffuse_program(const char *path) {
    std::ifstream file(path);assert(file);std::ostringstream text;text<<file.rdbuf();const auto source=text.str();
    const std::string fs=std::string(R"GLSL(#version 330 core
uniform vec3 uLocalIrradiance[9],uLocalIrradianceSecondary[9];
uniform float uLocalIrradianceWeight,uLocalIrradianceSecondaryWeight;
uniform vec4 uLocalIrradianceBounds,uLocalIrradianceSecondaryBounds;
uniform vec2 uLocalIrradianceHeights,uLocalIrradianceSecondaryHeights;
uniform vec3 tWorld;out vec4 color;
)GLSL")+function(source,"float local_sh_weight(")+function(source,"vec3 local_sh9(")+function(source,"vec3 local_diffuse(")+
        "void main(){color=vec4(local_diffuse(tWorld,vec3(0,1,0),vec3(.25)),1);}";
    const char *vs="#version 330 core\nvoid main(){vec2 q=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(q*2.-1.,0,1);}";
    const GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs.c_str()),p=glCreateProgram();
    glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);GLint okay=0;glGetProgramiv(p,GL_LINK_STATUS,&okay);assert(okay);
    glDeleteShader(v);glDeleteShader(f);return p;
}
bool diffuse_capture(const WorldLocalProbeCapture &frame,void *) {
    assert(frame.index<2);const auto &c=colors[frame.index];glClearColor(c[0],c[1],c[2],1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);return true;
}
void diffuse_continuity(GLuint target) {
    // Two17m-wide domains overlap10m. The camera crosses their exact distance
    // bisector while each fragment belongs to only one box. A dominant-box SH
    // switch would instantly erase local fill on one side of that bisector.
    WorldLocalProbeDef defs[2];
    for(unsigned i=0;i<2;i++) {
        defs[i].position[0]=i?2:-2;defs[i].position[1]=2;
        defs[i].box_min[0]=i?-5:-12;defs[i].box_max[0]=i?12:5;
        defs[i].box_min[1]=-5;defs[i].box_max[1]=10;defs[i].box_min[2]=-10;defs[i].box_max[2]=10;
    }
    auto *module=world_local_probes_create(defs,2,16);assert(module&&world_local_probes_capture(module,diffuse_capture,nullptr));
    float worst_step=0,negative_jump=0;
    for(const char *source:{"shaders/world_terrain.fs.glsl","shaders/world_foliage.fs.glsl"}) {
        const GLuint p=diffuse_program(source);glUseProgram(p);assert(world_local_probes_register_program(module,p));
        for(float fragment_x:{-8.f,8.f}) {
            glUniform3f(glGetUniformLocation(p,"tWorld"),fragment_x,2,0);
            Pixel previous={};
            for(int station=0;station<=100;station++) {
                const double camera[3]={-.1+station*.002,2,0};assert(world_local_probes_bind(module,p,camera));
                const float total=uniform(p,"uLocalIrradianceWeight")+uniform(p,"uLocalIrradianceSecondaryWeight");assert(total<=.220001f&&total>.219999f);
                const Pixel actual=draw(target);
                if(station)for(int c=0;c<3;c++)worst_step=std::max(worst_step,std::abs(actual[c]-previous[c]));
                previous=actual;
            }
        }
        glUniform3f(glGetUniformLocation(p,"tWorld"),-8,2,0);
        Pixel wrong[2];
        for(unsigned side=0;side<2;side++) {
            const double camera[3]={side?.001:-.001,2,0};assert(world_local_probes_bind(module,p,camera));
            glUniform1f(glGetUniformLocation(p,"uLocalIrradianceSecondaryWeight"),0);wrong[side]=draw(target);
        }
        for(int c=0;c<3;c++)negative_jump=std::max(negative_jump,std::abs(wrong[0][c]-wrong[1][c]));
        const double outside[3]={100,2,0};assert(world_local_probes_bind(module,p,outside));
        assert(uniform(p,"uLocalIrradianceWeight")==0&&uniform(p,"uLocalIrradianceSecondaryWeight")==0);
        const auto unlit=draw(target);for(int c=0;c<3;c++)assert(std::abs(unlit[c]-.25f)<1.e-7);
        glDeleteProgram(p);
    }
    assert(worst_step<.0001f&&negative_jump>.005f);world_local_probes_free(module);
    std::printf("PASS production terrain+foliage SH bisector: 10m overlap, fragments inside onlyonebox, max2mm camera step%.8f; zero-secondary negativecontrol jump%.6f; bothoutside weights exact0\n",worst_step,negative_jump);
}
}
int main() {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    assert(function("vec3 wanted(vec3 n);\nvec3 other(){return vec3(1);}\nvec3 wanted(vec3 n){return n;}\n","vec3 wanted(")=="vec3 wanted(vec3 n){return n;}\n");
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL")){setenv("SDL_VIDEODRIVER","offscreen",1);setenv("LIBGL_ALWAYS_SOFTWARE","1",1);}
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto *window=SDL_CreateWindow("Local probe contracts",0,0,32,32,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);auto context=SDL_GL_CreateContext(window);assert(context);
    std::printf("Local probe renderer: %s\n",glGetString(GL_RENDERER));
    GLuint vao=0;glGenVertexArrays(1,&vao);glBindVertexArray(vao);
    GLuint cube_sentinel=0,pbo=0;glActiveTexture(GL_TEXTURE0);glGenTextures(1,&cube_sentinel);glBindTexture(GL_TEXTURE_CUBE_MAP,cube_sentinel);
    glGenBuffers(1,&pbo);glBindBuffer(GL_PIXEL_PACK_BUFFER,pbo);glBufferData(GL_PIXEL_PACK_BUFFER,1,nullptr,GL_STATIC_DRAW);
    glPixelStorei(GL_PACK_ALIGNMENT,8);glPixelStorei(GL_PACK_ROW_LENGTH,17);glPixelStorei(GL_PACK_SKIP_PIXELS,3);glPixelStorei(GL_PACK_SKIP_ROWS,2);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,pbo);glPixelStorei(GL_UNPACK_ALIGNMENT,8);glPixelStorei(GL_UNPACK_ROW_LENGTH,19);glPixelStorei(GL_UNPACK_SKIP_PIXELS,5);
    glActiveTexture(GL_TEXTURE7);glViewport(3,4,17,19);
    WorldLocalProbeDef defs[3];
    for(unsigned i=0;i<3;i++){defs[i].position[0]=i==0?-2:(i==1?2:0);defs[i].position[1]=2;
        const float lo[3]={-10,-5,-10},hi[3]={10,10,10};std::copy_n(lo,3,defs[i].box_min);std::copy_n(hi,3,defs[i].box_max);}
    defs[2].interior=true;defs[2].box_min[0]=defs[2].box_min[2]=-2;defs[2].box_max[0]=defs[2].box_max[2]=2;
    defs[2].box_min[1]=0;defs[2].box_max[1]=5;
    Fixture fixture;fixture.module=world_local_probes_create(defs,3,16);assert(fixture.module);fixture.p=program(true);
    glUseProgram(fixture.p);assert(world_local_probes_register_program(fixture.module,fixture.p));
    const double unbaked_eye[3]={0,2,0};assert(world_local_probes_bind(fixture.module,fixture.p,unbaked_eye));
    assert(count(fixture.p)==0);(void)cube_bindings(false);glActiveTexture(GL_TEXTURE7);
    assert(world_local_probes_capture(fixture.module,capture,&fixture));assert(fixture.faces==18);
    GLint active=0,viewport[4]={},pack=0,align=0,row=0,skip=0;
    glGetIntegerv(GL_ACTIVE_TEXTURE,&active);assert(active==GL_TEXTURE7);glGetIntegerv(GL_VIEWPORT,viewport);assert(viewport[0]==3&&viewport[1]==4&&viewport[2]==17&&viewport[3]==19);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&pack);glGetIntegerv(GL_PACK_ALIGNMENT,&align);glGetIntegerv(GL_PACK_ROW_LENGTH,&row);glGetIntegerv(GL_PACK_SKIP_PIXELS,&skip);
    assert(pack==GLint(pbo)&&align==8&&row==17&&skip==3);
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&pack);glGetIntegerv(GL_UNPACK_ROW_LENGTH,&row);glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&skip);assert(pack==GLint(pbo)&&row==19&&skip==5);
    glActiveTexture(GL_TEXTURE0);GLint cube=0;glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP,&cube);assert(cube==GLint(cube_sentinel));
    glBindBuffer(GL_PIXEL_PACK_BUFFER,0);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glPixelStorei(GL_PACK_ALIGNMENT,1);glPixelStorei(GL_PACK_ROW_LENGTH,0);glPixelStorei(GL_PACK_SKIP_PIXELS,0);glPixelStorei(GL_PACK_SKIP_ROWS,0);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
    const auto baked=world_local_probes_stats(fixture.module);assert(baked.ready==3&&baked.count==3&&baked.capture_faces==18&&baked.readbacks==18&&baked.bytes<8*1024*1024);
    GLuint target=0,color=0;glGenFramebuffers(1,&target);glBindFramebuffer(GL_FRAMEBUFFER,target);glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,8,8,0,GL_RGBA,GL_FLOAT,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
    double camera[3]={-4,2,0};glUseProgram(fixture.p);assert(world_local_probes_bind(fixture.module,fixture.p,camera));assert(count(fixture.p)==2);
    float weights[2];glGetUniformfv(fixture.p,glGetUniformLocation(fixture.p,"uLocalProbeWeights"),weights);assert(std::abs(weights[0]+weights[1]-1)<1.e-6);
    assert(std::abs(uniform(fixture.p,"uLocalIrradianceWeight")+uniform(fixture.p,"uLocalIrradianceSecondaryWeight")-.22f)<1.e-6);
    const Pixel mixed=draw(target);for(int c=0;c<3;c++)assert(std::abs(mixed[c]-(colors[0][c]*weights[0]+colors[1][c]*weights[1]))<.001f);
    const auto paired=cube_bindings(true);assert(paired[0]!=paired[1]);
    GLint textures[3]={paired[0],paired[1],0};
    camera[0]=0;assert(world_local_probes_bind(fixture.module,fixture.p,camera));assert(count(fixture.p)==1);assert(std::abs(uniform(fixture.p,"uLocalIrradianceWeight")-.4f)<1.e-6);
    const Pixel inside=draw(target);for(int c=0;c<3;c++)assert(std::abs(inside[c]-colors[2][c])<.001f);
    const auto single=cube_bindings(true);assert(single[0]==single[1]);textures[2]=single[0];
    camera[0]=100;assert(world_local_probes_bind(fixture.module,fixture.p,camera));assert(count(fixture.p)==0&&uniform(fixture.p,"uLocalIrradianceWeight")==0&&uniform(fixture.p,"uLocalIrradianceSecondaryWeight")==0);
    const auto absent=cube_bindings(true);assert(absent[0]==textures[0]&&absent[1]==textures[0]);
    const Pixel outside=draw(target);assert(outside[0]==0&&outside[1]==0&&outside[2]==0&&outside[3]==0);
    camera[0]=9.9;assert(world_local_probes_bind(fixture.module,fixture.p,camera));glGetUniformfv(fixture.p,glGetUniformLocation(fixture.p,"uLocalProbeWeights"),weights);assert(weights[0]+weights[1]>.001f&&weights[0]+weights[1]<.02f);
    camera[0]=NAN;assert(!world_local_probes_bind(fixture.module,fixture.p,camera));assert(count(fixture.p)==0);camera[0]=0;
    assert(world_local_probes_bind(fixture.module,fixture.p,camera,false));assert(count(fixture.p)==0);
    const auto disabled_bindings=cube_bindings(true);assert(disabled_bindings==absent);
    const Pixel disabled=draw(target);assert(disabled==outside); // Bit-exact legacy disabled output.
    assert(!world_local_probes_capture(fixture.module,capture,&fixture));assert(world_local_probes_stats(fixture.module).ready==3);
    const GLuint terrain=program(false);glUseProgram(terrain);assert(world_local_probes_register_program(fixture.module,terrain));
    assert(world_local_probes_bind(fixture.module,terrain,camera));assert(std::abs(uniform(terrain,"uLocalIrradianceWeight")-.4f)<1.e-6);
    std::array<GLuint,7> replacements={};
    for(unsigned i=0;i<replacements.size();i++){replacements[i]=program(false);assert(world_local_probes_register_program(fixture.module,replacements[i])==(i<6));}
    glUseProgram(fixture.p);camera[0]=-4;
    // Exercise the complete reflected draw, not just deferred uniform calls,
    // before measuring bindings. Readback/finish completes the warmup work.
    for(int i=0;i<32;i++){assert(world_local_probes_bind(fixture.module,fixture.p,camera));(void)draw(target);}
    glFinish();
    // Positive allocation control proves the caller-local guard is active.
    using Allocate=void *(*)(std::size_t);using Deallocate=void (*)(void *)noexcept;
    Allocate volatile allocate=static_cast<Allocate>(::operator new);
    Deallocate volatile deallocate=static_cast<Deallocate>(::operator delete);
    track_caller_allocations=true;const unsigned control=caller_allocations;
    void *control_allocation=allocate(7);deallocate(control_allocation);
    track_caller_allocations=false;assert(caller_allocations==control+1);
    const unsigned before=caller_allocations,process_before=allocations.load();
    track_caller_allocations=true;
    for(int i=0;i<200;i++)assert(world_local_probes_bind(fixture.module,fixture.p,camera));
    track_caller_allocations=false;
    const unsigned process_during_binds=allocations.load()-process_before;
    assert(caller_allocations==before);
    for(unsigned epoch=0;epoch<7;epoch++){++world_program_epoch;glUseProgram(fixture.p);assert(world_local_probes_register_program(fixture.module,fixture.p));assert(world_local_probes_bind(fixture.module,fixture.p,camera));glUseProgram(terrain);assert(world_local_probes_register_program(fixture.module,terrain));assert(world_local_probes_bind(fixture.module,terrain,camera));}
    const auto final=world_local_probes_stats(fixture.module);assert(final.readbacks==18&&final.capture_faces==18&&final.bind_calls>200);
    world_local_probes_free(fixture.module);for(int id:textures)assert(id&&!glIsTexture(GLuint(id)));assert(!glIsFramebuffer(fixture.fbo)&&!glIsRenderbuffer(fixture.depth));
    diffuse_continuity(target);
    auto *failed=world_local_probes_create(defs,1,4);assert(failed);assert(!world_local_probes_capture(failed,failed_capture,nullptr));const auto bad=world_local_probes_stats(failed);
    assert(bad.ready==0&&bad.failures==1&&bad.readbacks==0);world_local_probes_free(failed);
    assert(glGetError()==GL_NO_ERROR);for(auto p:replacements)glDeleteProgram(p);glDeleteProgram(terrain);glDeleteProgram(fixture.p);glDeleteTextures(1,&cube_sentinel);glDeleteTextures(1,&color);
    glDeleteFramebuffers(1,&target);glDeleteBuffers(1,&pbo);glDeleteVertexArrays(1,&vao);assert(glGetError()==GL_NO_ERROR);
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    std::printf("PASS local probes GL:18 actual HDRcapture/readbacks, fullmips/sampleblend, interior priority/1.5m cameraedgefade, capture-feedback disabled withzero cube bindings, completeRGB16F fullmip cube bindings forsingle/outside/disabled slots, bit-exactdisabled output, finite/outside/count0 controls, SH-onlyprogram/eight-program bounded cache with seven replacement epochs, hostile PBO/pixelstore and FBO/viewport restoration,32 complete draw warmups,200 steadybinds0calling-thread C++allocations with positive control, callback failure/ownedGLteardown; logical bytes%llu; concurrent process allocations%u\n",static_cast<unsigned long long>(baked.bytes),process_during_binds);
}

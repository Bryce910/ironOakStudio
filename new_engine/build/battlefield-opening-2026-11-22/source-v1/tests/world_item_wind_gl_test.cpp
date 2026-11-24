// Actual production VS + color/shadow FS transform-feedback fixture.
// Compile with the private source include path, SDL2 flags and -lGL; pass the
// absolute render-quality-2026-11-06 lane path as the sole runtime argument.
// This fixture is prepared for the serialized GPU lane; it is not a screenshot test.
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include "stb_image.h"
#include "gl_helpers.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
struct Output {float position[3],normal[3],tangent[3],speed,wind[2];};
constexpr unsigned count=6;
std::string current_variant;
int current_kind=0;
void check(const char *stage,GLuint p=0,float time=0) {
    bool okay=true;
    for(GLenum error=glGetError();error!=GL_NO_ERROR;error=glGetError()) {
        std::fprintf(stderr,"Grass GL fixture: variant=%s kind=%d program=%u time=%.6f stage=%s error=0x%04x\n",
            current_variant.c_str(),current_kind,p,time,stage,unsigned(error));okay=false;
    }
    assert(okay);
}
bool is_sampler(GLenum type) {
    switch(type) {
    case GL_SAMPLER_1D:case GL_SAMPLER_2D:case GL_SAMPLER_3D:case GL_SAMPLER_CUBE:
    case GL_SAMPLER_1D_SHADOW:case GL_SAMPLER_2D_SHADOW:case GL_SAMPLER_CUBE_SHADOW:
    case GL_SAMPLER_1D_ARRAY:case GL_SAMPLER_2D_ARRAY:case GL_SAMPLER_1D_ARRAY_SHADOW:case GL_SAMPLER_2D_ARRAY_SHADOW:
    case GL_SAMPLER_2D_RECT:case GL_SAMPLER_2D_RECT_SHADOW:case GL_SAMPLER_BUFFER:
    case GL_SAMPLER_2D_MULTISAMPLE:case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_INT_SAMPLER_1D:case GL_INT_SAMPLER_2D:case GL_INT_SAMPLER_3D:case GL_INT_SAMPLER_CUBE:
    case GL_INT_SAMPLER_1D_ARRAY:case GL_INT_SAMPLER_2D_ARRAY:case GL_INT_SAMPLER_2D_RECT:case GL_INT_SAMPLER_BUFFER:
    case GL_INT_SAMPLER_2D_MULTISAMPLE:case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_1D:case GL_UNSIGNED_INT_SAMPLER_2D:case GL_UNSIGNED_INT_SAMPLER_3D:case GL_UNSIGNED_INT_SAMPLER_CUBE:
    case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:case GL_UNSIGNED_INT_SAMPLER_2D_RECT:case GL_UNSIGNED_INT_SAMPLER_BUFFER:
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:return true;
    default:return false;
    }
}
GLuint program(const std::string &root,const char *fragment) {
    char *vs=glsl_load((root+"/world_foliage.vs.glsl").c_str());
    char *fs=glsl_load((root+"/"+fragment).c_str());assert(vs&&fs);
    std::string v(vs),f(fs);v.insert(v.find('\n')+1,"#define VALWOOD_LOCAL_WIND 1\n");f.insert(f.find('\n')+1,"#define VALWOOD_LOCAL_WIND 1\n");
    const auto vertex=glh_compile(GL_VERTEX_SHADER,v.c_str()),pixel=glh_compile(GL_FRAGMENT_SHADER,f.c_str());
    std::free(vs);std::free(fs);
    GLuint p=glCreateProgram();glAttachShader(p,vertex);glAttachShader(p,pixel);
    const char *outputs[]={"vPos","vNormal","vTangent","vRootedGrassWindSpeed","vItemWind"};
    glTransformFeedbackVaryings(p,5,outputs,GL_INTERLEAVED_ATTRIBS);glLinkProgram(p);
    GLint okay=0;glGetProgramiv(p,GL_LINK_STATUS,&okay);
    if(!okay){char log[4096];glGetProgramInfoLog(p,sizeof(log),nullptr,log);std::fprintf(stderr,"%s/%s: %s\n",root.c_str(),fragment,log);}
    assert(okay);glDeleteShader(vertex);glDeleteShader(pixel);glUseProgram(p);
    // Different sampler types cannot share a texture unit, even with discarded
    // rasterization. Contents are irrelevant to transform feedback.
    GLint uniforms=0,max_units=0;glGetProgramiv(p,GL_ACTIVE_UNIFORMS,&uniforms);glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,&max_units);
    std::vector<GLenum> sampler_types;
    for(GLint i=0;i<uniforms;i++){
        char name[256];GLsizei length=0;GLint size=0;GLenum type=0;
        glGetActiveUniform(p,GLuint(i),sizeof(name),&length,&size,&type,name);
        if(!is_sampler(type))continue;
        auto found=std::find(sampler_types.begin(),sampler_types.end(),type);
        const GLint unit=GLint(found-sampler_types.begin());
        if(found==sampler_types.end())sampler_types.push_back(type);
        assert(unit<max_units);std::vector<GLint> units(std::size_t(size),unit);
        glUniform1iv(glGetUniformLocation(p,name),size,units.data());
    }
    check("sampler-assignment",p);glValidateProgram(p);glGetProgramiv(p,GL_VALIDATE_STATUS,&okay);
    if(!okay){char log[4096];glGetProgramInfoLog(p,sizeof(log),nullptr,log);std::fprintf(stderr,"Grass fixture validation %s/%s: %s\n",root.c_str(),fragment,log);}
    assert(okay);check("program-validation",p);
    return p;
}
void f(GLuint p,const char *key,float value){glUniform1f(glGetUniformLocation(p,key),value);}
void i(GLuint p,const char *key,int value){glUniform1i(glGetUniformLocation(p,key),value);}
void setup(GLuint p,int vegetation) {
    glUseProgram(p);i(p,"uFullTransform",1);i(p,"uRootedGrassWind",1);i(p,"uVegetation",vegetation);
    i(p,"uPoseBlendEnabled",0);i(p,"uColorCoverageCull",0);i(p,"uInstanceFade",0);
    i(p,"uItemWindEnabled",1);i(p,"uSourcePaintEnabled",0);i(p,"uBakedLightEnabled",0);
    f(p,"uWind",vegetation==0?0.f:.45f);f(p,"uTreeHeight",.71f);
    glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),-1408,-10368);
    glUniform2f(glGetUniformLocation(p,"uWeatherWind"),1.4f,.7f);
    glUniform3f(glGetUniformLocation(p,"uOffset"),0,0,0);
    glUniform4f(glGetUniformLocation(p,"uGrassInteractor"),0,0,0,0);
    i(p,"uSurfaceWindCount",1);glUniform4f(glGetUniformLocation(p,"uSurfaceWindCenter[0]"),-1310.25f,0,-10206.5f,2.f);
    glUniform4f(glGetUniformLocation(p,"uSurfaceWindRadius[0]"),32.f,12.f,32.f,0);
    glUniform4f(glGetUniformLocation(p,"uSurfaceWindDirection[0]"),1.f,.3f,0,0);
    const float identity[]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    glUniformMatrix4fv(glGetUniformLocation(p,"uVP"),1,GL_FALSE,identity);
}
std::array<Output,count> sample(GLuint p,float time,GLuint feedback) {
    check("before-sample",p,time);
    glUseProgram(p);f(p,"uTime",time);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,feedback);
    check("sample-bind",p,time);glBeginTransformFeedback(GL_POINTS);check("begin-feedback",p,time);
    glDrawArrays(GL_POINTS,0,count);check("draw-feedback",p,time);glEndTransformFeedback();check("end-feedback",p,time);
    std::array<Output,count> output;glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,sizeof(output),output.data());
    check("feedback-readback",p,time);return output;
}
float distance(const float *a,const float *b){float q=0;for(unsigned k=0;k<3;k++)q+=(a[k]-b[k])*(a[k]-b[k]);return std::sqrt(q);}
float dot(const float *a,const float *b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
}


int main(int argc,char **argv) {
    assert(argc==2);const std::string lane=argv[1];
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto *window=SDL_CreateWindow("Private item wind shader fixture",0,0,32,32,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);
    auto context=SDL_GL_CreateContext(window);assert(context);std::printf("Renderer: %s\n",glGetString(GL_RENDERER));
    GLuint vao=0,positions=0,feedback=0;glGenVertexArrays(1,&vao);glBindVertexArray(vao);
    const float vertices[]={-.8f,0,-.6f,.8f,0,.6f,-.4f,.355f,-.3f,.4f,.355f,.3f,-.1f,.71f,0,.1f,.71f,0};
    glGenBuffers(1,&positions);glBindBuffer(GL_ARRAY_BUFFER,positions);glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,nullptr);
    glVertexAttrib3f(1,0,0,1);glVertexAttrib2f(2,.5f,.5f);glVertexAttrib4f(3,1,0,0,1);
    glVertexAttrib4f(4,97.75f,0,161.5f,.72f);glVertexAttrib1f(5,.73f);glVertexAttrib3f(6,1,1,1);
    glVertexAttrib2f(14,.4f,-.3f);glVertexAttrib3f(15,.8f,1.3f,1.7f);
    glGenBuffers(1,&feedback);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,feedback);glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,sizeof(Output)*count,nullptr,GL_STREAM_READ);
    glEnable(GL_RASTERIZER_DISCARD);unsigned cases=0,defaults=0,contacts=0;
    check("fixture-setup");
    for(const char *variant:{"source-v1/shaders","reference-project/shaders"}) {
        current_variant=variant;const auto color=program(lane+"/"+variant,"world_foliage.fs.glsl"),shadow=program(lane+"/"+variant,"world_foliage_shadow.fs.glsl");
        for(int kind:{0,1,2,3,5,6}) {
            current_kind=kind;setup(color,kind);setup(shadow,kind);
            for(auto factors:{std::array<float,2>{0,0},std::array<float,2>{1,1},std::array<float,2>{0,1},std::array<float,2>{1,0},std::array<float,2>{1.75f,.25f},std::array<float,2>{2,2}}) {
                glVertexAttrib3f(6,1,factors[0],factors[1]);const auto first=sample(color,0,feedback);float span=0;
                for(unsigned tick=0;tick<30;tick++) {
                    const float time=tick*.137f;const auto a=sample(color,time,feedback),b=sample(shadow,time,feedback);
                    for(unsigned v=0;v<count;v++) {
                        for(unsigned k=0;k<3;k++)assert(std::isfinite(a[v].position[k])&&std::isfinite(a[v].normal[k])&&std::isfinite(a[v].tangent[k]));
                        assert(distance(a[v].position,b[v].position)<.00001f&&distance(a[v].normal,b[v].normal)<.00001f);
                        assert(std::abs(dot(a[v].normal,a[v].tangent))<.00001f&&std::abs(dot(a[v].normal,a[v].normal)-1)<.00001f);
                        assert(a[v].wind[0]==(kind==0?0.f:.45f*factors[0])&&a[v].wind[1]==(kind==0?0.f:.45f*factors[1]));
                        if(v<2&&(kind==2||kind==3||kind==5))assert(distance(a[v].position,first[v].position)==0&&a[v].speed==0);
                        else span=std::max(span,distance(a[v].position,first[v].position));
                    }
                    ++cases;
                }
                if(kind==0||(factors[0]==0&&(factors[1]==0||kind==6)))assert(span==0);
                else assert(span>.0001f);
            }
            // Disabled item scope ignores arbitrary/absent attribute channels,
            // preserving ordinary procedural/pose rendering exactly.
            for(unsigned tick=0;tick<20;tick++){
                const float time=tick*.19f;glUseProgram(color);i(color,"uItemWindEnabled",0);glVertexAttrib3f(6,1,0,0);const auto legacy=sample(color,time,feedback);
                glUseProgram(color);i(color,"uItemWindEnabled",1);glVertexAttrib3f(6,1,1,1);const auto normal=sample(color,time,feedback);
                assert(!std::memcmp(legacy.data(),normal.data(),sizeof(legacy)));++defaults;
            }
            if(kind==2||kind==3||kind==5){
                glVertexAttrib3f(6,1,0,0);const auto off=sample(color,1.3f,feedback);
                glUseProgram(color);glUniform4f(glGetUniformLocation(color,"uGrassInteractor"),-1310.25f,-10206.5f,0,2.f);
                const auto contact=sample(color,1.3f,feedback);float motion=0;
                for(unsigned v=2;v<count;v++)motion=std::max(motion,distance(off[v].position,contact[v].position));
                assert(motion>.001f);++contacts;
            }
        }
        glDeleteProgram(color);glDeleteProgram(shadow);
    }
    std::printf("PASS actual production item wind: %u animated color/shadow pairs, %u byte-exact default/scope cases, %u wind-off player contacts; static architecture, independent channels, local gusts, full TRS, fixed roots and matched tangent bases; two shader roots\n",cases,defaults,contacts);
    glDeleteBuffers(1,&feedback);glDeleteBuffers(1,&positions);glDeleteVertexArrays(1,&vao);
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
}

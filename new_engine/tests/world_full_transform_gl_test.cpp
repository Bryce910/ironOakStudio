#define GL_GLEXT_PROTOTYPES
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "engine/world_models.h"
#include "engine/world_model_cpu.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_transform_math.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
// Transform feedback forbids even a redundant glUseProgram. Keep the real
// asset upload/material/VAO/draw path, suppress only its redundant bind while
// inspecting feedback with that same verified program already active.
static bool model_feedback=false;
extern "C" void __real_glUseProgram(GLuint);
extern "C" void __wrap_glUseProgram(GLuint program){if(model_feedback){GLint active=0;glGetIntegerv(GL_CURRENT_PROGRAM,&active);assert(GLuint(active)==program);}else __real_glUseProgram(program);}
int main(){
    if(!getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto *window=SDL_CreateWindow("Valwood full transform correctness",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);auto context=SDL_GL_CreateContext(window);assert(context);std::cout<<"Renderer: "<<glGetString(GL_RENDERER)<<'\n';
    std::ifstream in("shaders/world_foliage.vs.glsl");std::string source((std::istreambuf_iterator<char>(in)),{});const char *text=source.c_str();GLuint shader=glCreateShader(GL_VERTEX_SHADER);glShaderSource(shader,1,&text,nullptr);glCompileShader(shader);GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);if(!ok){char log[4096];glGetShaderInfoLog(shader,sizeof log,nullptr,log);std::cerr<<log;}assert(ok);GLuint program=glCreateProgram();glAttachShader(program,shader);const char *outputs[]={"vPos","vNormal","vTangent"};glTransformFeedbackVaryings(program,3,outputs,GL_INTERLEAVED_ATTRIBS);glLinkProgram(program);glGetProgramiv(program,GL_LINK_STATUS,&ok);assert(ok);glUseProgram(program);
    const float identity[]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};glUniformMatrix4fv(glGetUniformLocation(program,"uVP"),1,GL_FALSE,identity);GLuint vao=0,buffer=0;glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&buffer);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer);glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,9*sizeof(float),nullptr,GL_STREAM_READ);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer);glEnable(GL_RASTERIZER_DISCARD);
    unsigned cases=0;for(bool enabled:{false,true})for(int yaw=-150;yaw<=150;yaw+=30)for(int pitch=-60;pitch<=60;pitch+=30){
        world_entity::Transform t={4,3,-2,double(yaw),1.2,enabled?double(pitch):0,enabled?27.:0,enabled?1.8:1,enabled?.6:1,enabled?1.3:1};
        glVertexAttrib3f(0,.7f,1.2f,-.4f);glVertexAttrib3f(1,.6f,.8f,0);glVertexAttrib4f(3,.8f,-.6f,0,1);glVertexAttrib4f(4,float(t.x),float(t.y),float(t.z),float(t.scale));glVertexAttrib1f(5,float(t.yaw*.017453292519943295));glVertexAttrib2f(14,float(t.pitch*.017453292519943295),float(t.roll*.017453292519943295));glVertexAttrib3f(15,float(t.sx),float(t.sy),float(t.sz));glUniform1i(glGetUniformLocation(program,"uFullTransform"),enabled);
        glBeginTransformFeedback(GL_POINTS);glDrawArrays(GL_POINTS,0,1);glEndTransformFeedback();float actual[9];glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,sizeof actual,actual);
        const auto m=vw_transform::matrix(t),inverse=vw_transform::inverse(m);const auto expected=vw_transform::point(m,{.7,1.2,-.4});double normal[3]={};for(unsigned i=0;i<3;i++)normal[i]=inverse.a[0][i]*.6+inverse.a[1][i]*.8;double length=std::sqrt(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);for(unsigned i=0;i<3;i++){assert(std::abs(actual[i]-expected[i])<3e-5);assert(std::abs(actual[i+3]-normal[i]/length)<3e-5);}assert(std::abs(actual[3]*actual[6]+actual[4]*actual[7]+actual[5]*actual[8])<2e-5);assert(glGetError()==GL_NO_ERROR);cases++;
    }
    // Exercise the actual optional asset stream, including a real static
    // model's VAO attributes and upload/reset/invalid-input transaction.
    std::string error;const char *asset_path="projects/valwood_world/models/starting_crate.glb";auto cpu=world_model_cpu::prepare(asset_path,{},error);assert(cpu);auto *asset=world_model_load(asset_path);assert(asset);size_t vertices=0;for(const auto &mesh:cpu->meshes)vertices+=mesh.indices.size();glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer);glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,GLsizeiptr(vertices*9*sizeof(float)),nullptr,GL_STREAM_READ);
    for(bool full:{true,false}){
        world_entity::Transform t={4,3,-2,32,1.2,full?23.:0,full?-17.:0,full?1.4:1,full?.7:1,full?1.1:1};WorldModelInstance instance={float(t.x),float(t.y),float(t.z),float(t.yaw*.017453292519943295),float(t.scale),float(t.pitch*.017453292519943295),float(t.roll*.017453292519943295),float(t.sx-1),float(t.sy-1),float(t.sz-1)};assert(world_model_upload_instances(asset,&instance,1));auto rejected=instance;rejected.axis_x=-1;assert(!world_model_upload_instances(asset,&rejected,1)&&world_model_instance_count(asset)==1);
        glBeginTransformFeedback(GL_TRIANGLES);model_feedback=true;world_model_draw(asset,program);model_feedback=false;glEndTransformFeedback();std::vector<float> actual(vertices*9);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer);glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,GLsizeiptr(actual.size()*sizeof(float)),actual.data());size_t vertex=0;const auto matrix=vw_transform::matrix(t);for(const auto &mesh:cpu->meshes)for(auto index:mesh.indices){const auto &v=mesh.vertices[index];const auto expected=vw_transform::point(matrix,{v.p[0],v.p[1],v.p[2]});for(unsigned j=0;j<3;j++)assert(std::abs(actual[vertex*9+j]-expected[j])<4e-5);vertex++;}GLint reset=1;glGetUniformiv(program,glGetUniformLocation(program,"uFullTransform"),&reset);assert(reset==0&&glGetError()==GL_NO_ERROR);
    }
    world_model_free(asset);assert(texture_cache_stats().references==0);std::cout<<"PASS actual static GLB transform buffer: "<<vertices<<" indexed vertices, extended/legacy reset and invalid-scale preservation\n";
    glDisable(GL_RASTERIZER_DISCARD);glDeleteBuffers(1,&buffer);glDeleteVertexArrays(1,&vao);glDeleteProgram(program);glDeleteShader(shader);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();std::cout<<"PASS production foliage/color/shadow vertex path: "<<cases<<" full/legacy TRS positions, inverse-transpose normals, tangent orthogonality, GL0\n";
}

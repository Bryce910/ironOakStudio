/* Source-identical instance-stream correctness, not a timing benchmark.
 * Link with --wrap=glBufferData --wrap=glBufferSubData
 * --wrap=glDrawElementsInstanced --wrap=glUseProgram --wrap=glBindBuffer
 * --wrap=glGetError. Native GL runs are owned
 * by the central acceptance runner; default is SDL's software offscreen GL.
 */
#define GL_GLEXT_PROTOTYPES
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <SDL2/SDL.h>
#include "gl_helpers.h"
#include "engine/world_models.h"
extern "C" {
#include "engine/texture_cache.h"
}
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <new>
#include <sstream>
#include <string>
#include <unistd.h>

namespace {
struct GPUInstance {float x,y,z,scale,yaw,fade;};
using Sample=std::array<float,6>;
static_assert(sizeof(GPUInstance)==24&&sizeof(Sample)==24,"Exact GL instance/feedback strides");
struct UploadTrace {unsigned data_calls=0,sub_calls=0;GLuint buffer=0;GLsizeiptr allocation=0,live=0;GLenum usage=0;bool null_data=false;};
struct DrawTrace {GLuint buffer=0,vao=0;GLsizei count=0;unsigned calls=0;bool pose=false;};
UploadTrace upload_trace;
DrawTrace draw_trace;
bool observe_upload=false,observe_draw=false,capturing=false;
// Measure the calling module's C++ work only. Driver workers have their own
// TLS flags; C++ allocation inside an actual GL entry point is excluded below.
thread_local bool track_allocations=false;
thread_local unsigned allocations=0,driver_call_depth=0,driver_allocations=0;
struct DriverCallScope {
    bool previous=track_allocations;
    DriverCallScope(){track_allocations=false;driver_call_depth++;}
    ~DriverCallScope(){driver_call_depth--;track_allocations=previous;}
};
GLuint active_program=0;
}
[[gnu::noinline]] void *operator new(std::size_t size){
    if(track_allocations)allocations++;
    if(driver_call_depth)driver_allocations++;
    if(void *memory=std::malloc(size?size:1))return memory;
    throw std::bad_alloc();
}
[[gnu::noinline]] void operator delete(void *memory) noexcept{std::free(memory);}
[[gnu::noinline]] void operator delete(void *memory,std::size_t) noexcept{std::free(memory);}

extern "C" void __real_glBufferData(GLenum,GLsizeiptr,const void *,GLenum);
extern "C" void __wrap_glBufferData(GLenum target,GLsizeiptr size,const void *data,GLenum usage){
    DriverCallScope driver;
    if(observe_upload){
        assert(target==GL_ARRAY_BUFFER);GLint buffer=0;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&buffer);assert(buffer>0);
        assert(!upload_trace.buffer||upload_trace.buffer==GLuint(buffer));
        upload_trace.buffer=GLuint(buffer);upload_trace.data_calls++;upload_trace.allocation=size;
        upload_trace.null_data=data==nullptr;upload_trace.usage=usage;
    }
    __real_glBufferData(target,size,data,usage);
}
extern "C" void __real_glBufferSubData(GLenum,GLintptr,GLsizeiptr,const void *);
extern "C" void __wrap_glBufferSubData(GLenum target,GLintptr offset,GLsizeiptr size,const void *data){
    DriverCallScope driver;
    if(observe_upload){
        assert(target==GL_ARRAY_BUFFER&&offset==0&&data&&size>0);GLint buffer=0;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&buffer);
        assert(upload_trace.buffer==GLuint(buffer));upload_trace.sub_calls++;upload_trace.live=size;
    }
    __real_glBufferSubData(target,offset,size,data);
}
extern "C" void __real_glDrawElementsInstanced(GLenum,GLsizei,GLenum,const void *,GLsizei);
extern "C" void __wrap_glDrawElementsInstanced(GLenum mode,GLsizei indices,GLenum type,const void *offset,GLsizei count){
    DriverCallScope driver;
    if(observe_draw){
        assert(mode==GL_TRIANGLES&&indices==3&&type==GL_UNSIGNED_INT&&offset==nullptr&&count==draw_trace.count);
        GLint vao=0;glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);assert(vao>0);
        if(draw_trace.vao)assert(draw_trace.vao==GLuint(vao));else draw_trace.vao=GLuint(vao);
        for(unsigned attribute:{4u,5u,draw_trace.pose?11u:6u}){
            GLint value=0;glGetVertexAttribiv(attribute,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&value);assert(GLuint(value)==draw_trace.buffer);
            glGetVertexAttribiv(attribute,GL_VERTEX_ATTRIB_ARRAY_DIVISOR,&value);assert(value==1);
            glGetVertexAttribiv(attribute,GL_VERTEX_ATTRIB_ARRAY_STRIDE,&value);assert(value==24);
            glGetVertexAttribiv(attribute,GL_VERTEX_ATTRIB_ARRAY_TYPE,&value);assert(value==GL_FLOAT);
            void *pointer=nullptr;glGetVertexAttribPointerv(attribute,GL_VERTEX_ATTRIB_ARRAY_POINTER,&pointer);
            assert(reinterpret_cast<std::uintptr_t>(pointer)==(attribute==4?0u:attribute==5?16u:20u));
        }
        draw_trace.calls++;
    }
    __real_glDrawElementsInstanced(mode,indices,type,offset,count);
}
extern "C" void __real_glUseProgram(GLuint);
extern "C" void __wrap_glUseProgram(GLuint program){
    DriverCallScope driver;
    if(capturing){assert(program==active_program);return;}
    __real_glUseProgram(program);
}
extern "C" void __real_glBindBuffer(GLenum,GLuint);
extern "C" void __wrap_glBindBuffer(GLenum target,GLuint buffer){DriverCallScope driver;__real_glBindBuffer(target,buffer);}
extern "C" GLenum __real_glGetError(void);
extern "C" GLenum __wrap_glGetError(void){DriverCallScope driver;return __real_glGetError();}

namespace {
constexpr std::array<std::size_t,24> counts={1,2,15,16,17,31,32,33,63,64,65,127,128,129,255,256,257,4095,4096,2,0,17,1,33};
std::string fixture(const char *directory,const char *name,bool next){
    const std::array<float,24> vertices={next?.25f:0.f,0,0, 0,0,1, 0,0,
        next?1.25f:1.f,0,0, 0,0,1, 1,0,
        next?.25f:0.f,1,0, 0,0,1, 0,1};
    const std::array<std::uint32_t,3> indices={0,1,2};
    constexpr unsigned vertex_bytes=sizeof(vertices),index_bytes=sizeof(indices),bytes=vertex_bytes+index_bytes;
    std::ostringstream json;
    json<<R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":)"<<bytes<<R"(}],"bufferViews":[{"buffer":0,"byteLength":)"<<vertex_bytes
        <<R"(,"byteStride":32},{"buffer":0,"byteOffset":)"<<vertex_bytes<<R"(,"byteLength":)"<<index_bytes<<R"(}],"accessors":[)";
    json<<R"({"bufferView":0,"byteOffset":0,"componentType":5126,"count":3,"type":"VEC3","min":[)"<<(next?.25:0)
        <<R"(,0,0],"max":[)"<<(next?1.25:1)<<R"(,1,0]},)";
    json<<R"({"bufferView":0,"byteOffset":12,"componentType":5126,"count":3,"type":"VEC3"},)"
        <<R"({"bufferView":0,"byteOffset":24,"componentType":5126,"count":3,"type":"VEC2"},)"
        <<R"({"bufferView":1,"componentType":5125,"count":3,"type":"SCALAR"}],)"
        <<R"("meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"indices":3}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";
    std::string text=json.str();while(text.size()%4)text+=' ';
    const std::array<std::uint32_t,5> header={0x46546c67u,2u,unsigned(12+8+text.size()+8+bytes),unsigned(text.size()),0x4e4f534au};
    const std::array<std::uint32_t,2> chunk={bytes,0x004e4942u};
    const std::string path=std::string(directory)+"/"+name+".glb";std::ofstream stream(path,std::ios::binary);assert(stream);
    stream.write(reinterpret_cast<const char *>(header.data()),sizeof(header));stream.write(text.data(),std::streamsize(text.size()));
    stream.write(reinterpret_cast<const char *>(chunk.data()),sizeof(chunk));stream.write(reinterpret_cast<const char *>(vertices.data()),sizeof(vertices));
    stream.write(reinterpret_cast<const char *>(indices.data()),sizeof(indices));assert(stream);return path;
}
GLuint program(){
    const char *vs=R"(#version 330 core
layout(location=0) in vec3 aPosition;layout(location=4) in vec4 aInstance;
layout(location=5) in float aYaw;layout(location=6) in float aFade;
layout(location=8) in vec3 aNext;layout(location=11) in float aWeight;
uniform bool uPoseBlendEnabled;uniform bool uInstanceFade;
out vec4 observed;out vec2 phase;
void main(){float w=uPoseBlendEnabled?aWeight:0.;vec3 p=mix(aPosition,aNext,w);
float c=cos(aYaw),s=sin(aYaw);p=vec3(c*p.x+s*p.z,p.y,-s*p.x+c*p.z)*aInstance.w+aInstance.xyz;
observed=vec4(p,uInstanceFade?aFade:1.);phase=vec2(w,aYaw);gl_Position=vec4(p,1.);})";
    const char *fs=R"(#version 330 core
out vec4 color;void main(){color=vec4(1.);})";
    const GLuint p=GLH_program(vs,fs);assert(p);
    const char *outputs[]={"observed","phase"};glTransformFeedbackVaryings(p,2,outputs,GL_INTERLEAVED_ATTRIBS);glLinkProgram(p);
    GLint linked=0;glGetProgramiv(p,GL_LINK_STATUS,&linked);assert(linked);return p;
}
std::size_t bucket(std::size_t n){std::size_t result=16;while(result<n)result*=2;return result;}
void begin_upload(){upload_trace={};observe_upload=true;allocations=0;track_allocations=true;}
void end_upload(){track_allocations=false;observe_upload=false;assert(allocations==0);}
void check_allocation_guard(){
    // An allocation in module scope must remain observable. Calling operator
    // new directly avoids compiler elision of a matching new/delete expression.
    allocations=0;track_allocations=true;
    void *witness=::operator new(64);
    track_allocations=false;::operator delete(witness);assert(allocations==1);
    // Nested driver exclusion must restore the armed caller on return.
    allocations=0;track_allocations=true;
    {DriverCallScope driver;void *inside=::operator new(32);::operator delete(inside);assert(!track_allocations);}
    assert(track_allocations&&allocations==0);track_allocations=false;
    std::puts("PASS instance allocation guard: positive caller allocation detected; scoped GL/driver exclusion restores caller tracking; guard state is thread-local");
}
void check_upload(bool exact,std::size_t count,std::size_t &high_water,GLuint &buffer){
    if(!count){assert(upload_trace.data_calls==0&&upload_trace.sub_calls==0);return;}
    assert(upload_trace.data_calls==1);
    if(buffer)assert(upload_trace.buffer==buffer);else buffer=upload_trace.buffer;
    if(exact){assert(!upload_trace.null_data&&upload_trace.sub_calls==0&&upload_trace.usage==GL_DYNAMIC_DRAW);high_water=count;}
    else{high_water=std::max(high_water,bucket(count));assert(upload_trace.null_data&&upload_trace.sub_calls==1&&upload_trace.usage==GL_STREAM_DRAW);assert(upload_trace.live==GLsizeiptr(count*24));}
    assert(upload_trace.allocation==GLsizeiptr(high_water*24));
    GLint binding=99;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&binding);assert(binding==0);
}
void buffer_contents(GLuint buffer,std::size_t bytes,GLenum usage,const std::array<WorldModelInstance,4096> &instances,
                     const std::array<float,4096> &values,std::size_t count,bool extra){
    assert(buffer&&bytes<=4096*24);glBindBuffer(GL_COPY_READ_BUFFER,buffer);
    GLint64 actual=0;glGetBufferParameteri64v(GL_COPY_READ_BUFFER,GL_BUFFER_SIZE,&actual);assert(actual==GLint64(bytes));
    GLint actual_usage=0;glGetBufferParameteriv(GL_COPY_READ_BUFFER,GL_BUFFER_USAGE,&actual_usage);assert(GLenum(actual_usage)==usage);
    std::array<GPUInstance,4096> readback={};
    if(count)glGetBufferSubData(GL_COPY_READ_BUFFER,0,GLsizeiptr(count*24),readback.data());
    for(std::size_t i=0;i<count;i++){
        const auto &p=instances[i];const GPUInstance expected={p.x,p.y,p.z,p.scale,p.yaw,extra?values[i]:1.f};
        assert(std::memcmp(&readback[i],&expected,sizeof(expected))==0);
    }
    glBindBuffer(GL_COPY_READ_BUFFER,0);assert(glGetError()==GL_NO_ERROR);
}
void append_hash(std::uint64_t &hash,const void *data,std::size_t size){
    const auto *bytes=static_cast<const unsigned char *>(data);for(std::size_t i=0;i<size;i++){hash^=bytes[i];hash*=1099511628211ull;}
}
void draw_feedback(WorldModelAsset *asset,WorldModelPoseBlend *batch,GLuint p,GLuint feedback,GLuint instance_buffer,
                   std::size_t count,const std::array<WorldModelInstance,4096> &instances,const std::array<float,4096> &values,
                   bool fading,GLuint &stable_vao,std::uint64_t &hash){
    draw_trace={instance_buffer,stable_vao,GLsizei(count),0,batch!=nullptr};observe_draw=true;
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,feedback);glUseProgram(p);active_program=p;
    glEnable(GL_RASTERIZER_DISCARD);capturing=true;glBeginTransformFeedback(GL_TRIANGLES);
    if(batch)world_model_pose_blend_draw(batch,p);else world_model_draw(asset,p);
    glEndTransformFeedback();capturing=false;glDisable(GL_RASTERIZER_DISCARD);observe_draw=false;
    assert(draw_trace.calls==(count?1u:0u));if(count)stable_vao=draw_trace.vao;
    std::array<Sample,4096*3> samples={};glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,feedback);
    if(count)glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,GLsizeiptr(count*3*sizeof(Sample)),samples.data());
    const std::array<std::array<double,3>,3> vertices={{{0,0,0},{1,0,0},{0,1,0}}};
    for(std::size_t i=0;i<count;i++)for(unsigned v=0;v<3;v++){
        const auto &a=instances[i];const double weight=batch?values[i]:0,x=vertices[v][0]+weight*.25,y=vertices[v][1];
        const std::array<double,6> expected={a.x+std::cos(a.yaw)*x*a.scale,a.y+y*a.scale,
            a.z-std::sin(a.yaw)*x*a.scale,fading?values[i]:1.f,weight,a.yaw};
        for(unsigned k=0;k<6;k++)assert(std::isfinite(samples[i*3+v][k])&&std::abs(samples[i*3+v][k]-expected[k])<.0001);
    }
    append_hash(hash,samples.data(),count*3*sizeof(Sample));
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,0);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,0);assert(glGetError()==GL_NO_ERROR);
}
void check_production_default(const std::string &one,const std::string &two){
    assert(unsetenv("VALWOOD_QA_EXACT_INSTANCE_BUFFERS")==0);
    auto *asset=world_model_load(one.c_str());auto *a=world_model_load_pose(one.c_str(),1);auto *b=world_model_load_pose(two.c_str(),1);
    auto *batch=world_model_pose_blend_create(17,1);assert(asset&&a&&b&&batch&&world_model_pose_blend_set_targets(batch,a,b));
    // The unset production default must be exact for both stream types, and
    // stay latched even if a diagnostic override changes after construction.
    SDL_setenv("VALWOOD_QA_EXACT_INSTANCE_BUFFERS","0",1);
    std::array<WorldModelInstance,4096> instances={};std::array<float,4096> values={};
    for(std::size_t i=0;i<17;i++){instances[i]={float(i),0,0,0,1};values[i]=.5f;}
    GLuint model_buffer=0,pose_buffer=0;std::size_t model_capacity=0,pose_capacity=0;
    begin_upload();assert(world_model_upload_instances(asset,instances.data(),17));end_upload();
    check_upload(true,17,model_capacity,model_buffer);
    buffer_contents(model_buffer,17*24,GL_DYNAMIC_DRAW,instances,values,17,false);
    begin_upload();assert(world_model_upload_fading_instances(asset,instances.data(),values.data(),17));end_upload();
    check_upload(true,17,model_capacity,model_buffer);
    buffer_contents(model_buffer,17*24,GL_DYNAMIC_DRAW,instances,values,17,true);
    begin_upload();assert(world_model_pose_blend_upload(batch,instances.data(),values.data(),17));end_upload();
    check_upload(true,17,pose_capacity,pose_buffer);
    buffer_contents(pose_buffer,17*24,GL_DYNAMIC_DRAW,instances,values,17,true);
    world_model_pose_blend_free(batch);world_model_free(b);world_model_free(a);world_model_free(asset);
    std::puts("PASS production default with override unset: static/fading/pose exact17*24-byte GL_DYNAMIC_DRAW stores, actual live-byte readbacks, latched policy, zero caller-module upload allocations");
}
std::uint64_t exercise(const std::string &one,const std::string &two,bool exact,GLuint p,GLuint feedback){
    SDL_setenv("VALWOOD_QA_EXACT_INSTANCE_BUFFERS",exact?"1":"0",1);
    auto *asset=world_model_load(one.c_str());auto *a=world_model_load_pose(one.c_str(),1);auto *b=world_model_load_pose(two.c_str(),1);
    auto *batch=world_model_pose_blend_create(4096,1);assert(asset&&a&&b&&batch&&world_model_pose_blend_set_targets(batch,a,b));
    // Runtime getenv changes must never change an existing stream's policy.
    SDL_setenv("VALWOOD_QA_EXACT_INSTANCE_BUFFERS",exact?"0":"1",1);
    std::array<WorldModelInstance,4096> instances={};std::array<float,4096> values={};
    for(std::size_t i=0;i<instances.size();i++){
        instances[i]={float(i)*.017f,float(i%7)*.01f,-float(i)*.005f,float(i%11)*.14f,1.f+float(i%9)*.02f};values[i]=float(i%17)/16.f;
    }
    GLuint model_buffer=0,pose_buffer=0,model_vao=0,pose_vao=0;std::size_t model_capacity=0,pose_capacity=0;
    std::uint64_t hash=1469598103934665603ull;
    for(std::size_t count:counts){
        begin_upload();assert(world_model_upload_instances(asset,instances.data(),count));end_upload();
        check_upload(exact,count,model_capacity,model_buffer);assert(world_model_instance_count(asset)==count);
        buffer_contents(model_buffer,model_capacity*24,exact?GL_DYNAMIC_DRAW:GL_STREAM_DRAW,instances,values,count,false);
        draw_feedback(asset,nullptr,p,feedback,model_buffer,count,instances,values,false,model_vao,hash);
        begin_upload();assert(world_model_upload_fading_instances(asset,instances.data(),values.data(),count));end_upload();
        check_upload(exact,count,model_capacity,model_buffer);buffer_contents(model_buffer,model_capacity*24,exact?GL_DYNAMIC_DRAW:GL_STREAM_DRAW,instances,values,count,true);
        draw_feedback(asset,nullptr,p,feedback,model_buffer,count,instances,values,true,model_vao,hash);
        begin_upload();assert(world_model_pose_blend_upload(batch,instances.data(),values.data(),count));end_upload();
        check_upload(exact,count,pose_capacity,pose_buffer);buffer_contents(pose_buffer,pose_capacity*24,exact?GL_DYNAMIC_DRAW:GL_STREAM_DRAW,instances,values,count,true);
        draw_feedback(nullptr,batch,p,feedback,pose_buffer,count,instances,values,false,pose_vao,hash);
    }
    constexpr std::size_t retained=counts.back();GLuint sentinel=0;glGenBuffers(1,&sentinel);glBindBuffer(GL_ARRAY_BUFFER,sentinel);
    instances.back().scale=std::numeric_limits<float>::quiet_NaN();begin_upload();assert(!world_model_upload_instances(asset,instances.data(),4096));end_upload();
    assert(!upload_trace.data_calls&&!upload_trace.sub_calls&&world_model_instance_count(asset)==retained);
    instances.back().scale=1;values.back()=std::numeric_limits<float>::infinity();
    begin_upload();assert(!world_model_upload_fading_instances(asset,instances.data(),values.data(),4096));end_upload();assert(!upload_trace.data_calls&&!upload_trace.sub_calls);
    begin_upload();assert(!world_model_pose_blend_upload(batch,instances.data(),values.data(),4096));end_upload();assert(!upload_trace.data_calls&&!upload_trace.sub_calls);
    GLint binding=0;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&binding);assert(GLuint(binding)==sentinel);
    buffer_contents(model_buffer,model_capacity*24,exact?GL_DYNAMIC_DRAW:GL_STREAM_DRAW,instances,values,retained,true);
    buffer_contents(pose_buffer,pose_capacity*24,exact?GL_DYNAMIC_DRAW:GL_STREAM_DRAW,instances,values,retained,true);
    // The failing ordinary upload must also preserve active fade mode/count.
    draw_feedback(asset,nullptr,p,feedback,model_buffer,retained,instances,values,true,model_vao,hash);
    draw_feedback(nullptr,batch,p,feedback,pose_buffer,retained,instances,values,false,pose_vao,hash);
    glBindBuffer(GL_ARRAY_BUFFER,0);glDeleteBuffers(1,&sentinel);
    world_model_pose_blend_free(batch);world_model_free(b);world_model_free(a);world_model_free(asset);
    // GPU storage may round above a declared batch limit; validation may not.
    SDL_setenv("VALWOOD_QA_EXACT_INSTANCE_BUFFERS",exact?"1":"0",1);batch=world_model_pose_blend_create(17,1);assert(batch);
    begin_upload();assert(world_model_pose_blend_upload(batch,instances.data(),values.data(),17));end_upload();
    std::size_t small_capacity=0;GLuint small_buffer=0;check_upload(exact,17,small_capacity,small_buffer);
    begin_upload();assert(!world_model_pose_blend_upload(batch,instances.data(),values.data(),18));end_upload();assert(!upload_trace.data_calls&&!upload_trace.sub_calls);
    buffer_contents(small_buffer,small_capacity*24,exact?GL_DYNAMIC_DRAW:GL_STREAM_DRAW,instances,values,17,true);world_model_pose_blend_free(batch);
    std::printf("PASS %s instance buffers: static/fading/pose counts1..4096, clear/refill/shrink, stable VAO/buffer IDs, actual buffer-size/usage/live-byte readbacks, analytical GPU geometry, late-invalid transaction, exact declared capacity, zero C++ upload allocations; hash=%llu\n",exact?"exact diagnostic":"orphaned high-water",(unsigned long long)hash);
    return hash;
}
}
int main(){
    check_allocation_guard();
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto *window=SDL_CreateWindow("Instance stream correctness",0,0,32,32,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);
    const auto context=SDL_GL_CreateContext(window);assert(context);std::printf("Instance stream renderer: %s\n",glGetString(GL_RENDERER));
    char directory[]="/tmp/valwood-instance-stream-XXXXXX";assert(mkdtemp(directory));
    const auto first=fixture(directory,"pose-a",false),next=fixture(directory,"pose-b",true);const auto p=program();
    GLuint feedback=0;glGenBuffers(1,&feedback);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,feedback);glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,4096*3*sizeof(Sample),nullptr,GL_STATIC_READ);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,0);
    check_production_default(first,next);
    const auto streaming=exercise(first,next,false,p,feedback),exact=exercise(first,next,true,p,feedback);assert(streaming==exact);
    assert(!texture_cache_stats().entries&&!texture_cache_stats().references);texture_cache_shutdown();
    glDeleteBuffers(1,&feedback);glDeleteProgram(p);assert(glGetError()==GL_NO_ERROR);
    assert(unlink(first.c_str())==0&&unlink(next.c_str())==0&&rmdir(directory)==0);
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    std::printf("PASS instance streaming: exact and orphaned policies yield byte-identical transformed positions/fades/weights across all cases; caller-module upload allocations0; excluded same-thread driver/control allocations%u; performance benefit requires isolated native timing\n",driver_allocations);
}

// Requires one isolated GL context. This test never constitutes an FPS result.
#define main world_animation_cpu_fixture_main
#include "tests/world_animation_test.cpp"
#undef main
#define GL_GLEXT_PROTOTYPES
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#include "gl_helpers.h"
#include "engine/world_animation_render.h"
#include "engine/world_program_epoch.h"
#include <algorithm>
extern "C" {
#include "engine/texture_cache.h"
}

namespace ar=world_animation_render;
static bool hot=false;static unsigned hot_lookups=0,hot_queries=0,hot_allocations=0;
extern "C" GLint __real_glGetUniformLocation(GLuint,const GLchar *);
extern "C" GLint __wrap_glGetUniformLocation(GLuint p,const GLchar *name){if(hot)hot_lookups++;return __real_glGetUniformLocation(p,name);}
extern "C" void __real_glGetIntegerv(GLenum,GLint *);
extern "C" void __wrap_glGetIntegerv(GLenum p,GLint *v){if(hot)hot_queries++;__real_glGetIntegerv(p,v);}
extern "C" void __real_glBufferData(GLenum,GLsizeiptr,const void *,GLenum);
extern "C" void __wrap_glBufferData(GLenum a,GLsizeiptr b,const void *c,GLenum d){if(hot)hot_allocations++;__real_glBufferData(a,b,c,d);}
namespace {
std::string shader(const char *path){std::ifstream f(path);assert(f);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
void integer(GLuint p,const char *name,int value){glUniform1i(glGetUniformLocation(p,name),value);}
void vec3(GLuint p,const char *name,float x,float y,float z){glUniform3f(glGetUniformLocation(p,name),x,y,z);}
void scalar(GLuint p,const char *name,float value){glUniform1f(glGetUniformLocation(p,name),value);}
GLuint program(const char *fragment,bool vertex_graph=false){
 auto vs=shader("shaders/world_skeletal.vs.glsl");const auto fs=shader(fragment);
 if(vertex_graph){
  const char *marker="// VALWOOD_GRAPH_VERTEX";const auto at=vs.find(marker);assert(at!=std::string::npos);
  // Same generated graph function signature, bounds and absolute-world inputs
  // as Program::load; this fixture independently measures its geometric effect.
  vs.replace(at,std::strlen(marker),"uniform int uStudioGraphIndex;\nvec3 studioGraphVertex(vec2 uv,vec3 world,float time){if(uStudioGraphIndex!=2)return vec3(0.);return clamp(vec3(world.x*.000001+.025*sin(time),world.y*.01,world.z*.000001+.02*sin(time)),vec3(-.15),vec3(.15));}\n");
  vs.insert(vs.find('\n')+1,"#define VALWOOD_STUDIO_GRAPH 1\n");
 }
 GLuint p=GLH_program(vs.c_str(),fs.c_str());assert(p);const char *varyings[]={"vPos","vNormal","vTangent","vBitangent"};glTransformFeedbackVaryings(p,4,varyings,GL_INTERLEAVED_ATTRIBS);glLinkProgram(p);GLint linked=0;glGetProgramiv(p,GL_LINK_STATUS,&linked);assert(linked);glUseProgram(p);
 integer(p,"uShadow",7);integer(p,"uMacroHeight",11);integer(p,"uLocalProbe0",4);integer(p,"uLocalProbe1",5);integer(p,"uEnvironmentBRDF",6);integer(p,"uFacadeAux0",8);integer(p,"uFacadeAux1",9);integer(p,"uFacadeAux2",10);integer(p,"uVillageSkyMask",12);integer(p,"uVillageSkyDirectional",13);integer(p,"uVillageSkyUp",14);integer(p,"uEnvironmentCube",15);
 Mat4 identity;glUniformMatrix4fv(glGetUniformLocation(p,"uVP"),1,GL_FALSE,identity.m.data());glUniformMatrix4fv(glGetUniformLocation(p,"uLightVP"),1,GL_FALSE,identity.m.data());vec3(p,"uSun",0,0,1);return p;
}
bool same(const ar::Bindings &a,const ar::Bindings &b){return a.vao==b.vao&&a.array_buffer==b.array_buffer&&a.texture_buffer==b.texture_buffer&&a.active_texture==b.active_texture&&a.textures==b.textures&&a.samplers==b.samplers&&a.palette_texture==b.palette_texture&&a.palette_sampler==b.palette_sampler&&a.cull_face==b.cull_face;}
using Surface=std::array<float,12>;
std::vector<Surface> capture(ar::Renderer *renderer,GLuint p,std::size_t count,int graph=-1){
 std::vector<Surface> result(count);GLuint feedback=0;glGenBuffers(1,&feedback);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,feedback);glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,GLsizeiptr(count*sizeof(Surface)),nullptr,GL_DYNAMIC_READ);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,feedback);glUseProgram(p);auto bindings=ar::capture_bindings();bindings.studio_graph_index=graph;std::string error;
 glEnable(GL_RASTERIZER_DISCARD);glBeginTransformFeedback(GL_TRIANGLES);hot=true;assert(ar::draw(renderer,p,bindings,error));hot=false;glEndTransformFeedback();glDisable(GL_RASTERIZER_DISCARD);assert(same(bindings,ar::capture_bindings()));glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,GLsizeiptr(count*sizeof(Surface)),result.data());glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,0);glDeleteBuffers(1,&feedback);assert(glGetError()==GL_NO_ERROR);return result;
}
Vec3 placed(Vec3 p,const ar::Instance &i,bool direction=false){const float c=std::cos(i.yaw),s=std::sin(i.yaw);Vec3 v{c*p.x+s*p.z,p.y,-s*p.x+c*p.z};if(!direction)v={v.x*i.scale+i.x,v.y*i.scale+i.y,v.z*i.scale+i.z};return v;}
void deformation_and_transactions(const std::array<GLuint,3> &programs){
 auto asset=imported();std::string error;auto *renderer=ar::create(asset,2,error);assert(renderer);for(auto p:programs)assert(ar::warm(renderer,p,error));std::array<Pose,2> poses;assert(sample(asset.skeleton,asset.clips[0],1,false,poses[0],error));assert(rest_pose(asset.skeleton,poses[1],error));std::array<ar::Instance,2> instances={ar::Instance{},ar::Instance{2,.5f,-1,.5f,1.57079632679f}};
 const auto bindings=ar::capture_bindings();hot=true;assert(ar::upload(renderer,poses.data(),instances.data(),2,bindings,error));hot=false;assert(same(bindings,ar::capture_bindings()));auto surface=capture(renderer,programs[0],6);
 for(unsigned actor=0;actor<2;actor++)for(unsigned v=0;v<3;v++){const auto index=asset.mesh.indices[v];Vec3 p,n;assert(deform_vertex(asset.mesh.vertices[index],poses[actor],p,n,error));p=transform_point(poses[actor].global[asset.skeleton.mesh_node],p);p=placed(p,instances[actor]);n=placed(n,instances[actor],true);const auto &gpu=surface[actor*3+v];vector_near({gpu[0],gpu[1],gpu[2]},p);vector_near({gpu[3],gpu[4],gpu[5]},n);for(unsigned at:{3u,6u,9u})assert(near(gpu[at]*gpu[at]+gpu[at+1]*gpu[at+1]+gpu[at+2]*gpu[at+2],1));assert(near(gpu[3]*gpu[6]+gpu[4]*gpu[7]+gpu[5]*gpu[8],0));}
 for(unsigned pass=1;pass<3;pass++)assert(capture(renderer,programs[pass],6)==surface);
 const auto before=ar::stats(renderer);instances[0].scale=0;hot=true;assert(!ar::upload(renderer,poses.data(),instances.data(),2,bindings,error));hot=false;instances[0].scale=1;assert(same(bindings,ar::capture_bindings()));assert(ar::stats(renderer).uploads==before.uploads);assert(capture(renderer,programs[0],6)==surface);
 poses[0].skin[0].m[0]=std::numeric_limits<float>::quiet_NaN();hot=true;assert(!ar::upload(renderer,poses.data(),instances.data(),2,bindings,error));assert(!ar::upload(renderer,poses.data(),instances.data(),3,bindings,error));hot=false;assert(same(bindings,ar::capture_bindings()));assert(capture(renderer,programs[0],6)==surface);
 world_program_epoch++;glUseProgram(programs[0]);assert(!ar::draw(renderer,programs[0],bindings,error));assert(ar::warm(renderer,programs[0],error));assert(capture(renderer,programs[0],6)==surface);
 assert(ar::stats(renderer).palette_bytes==2*2*2*sizeof(Mat4));ar::destroy(renderer);assert(texture_cache_stats().references==0&&texture_cache_stats().entries==0);
}
Asset textured_asset(){Files files;GLB fixture;fixture.save(files.path("material.glb"),false,false,false,true);Asset asset;std::string error;assert(load_glb(files.path("material.glb").c_str(),asset,error));assert(asset.images.size()==3&&asset.materials.size()==1);return asset;}
std::array<unsigned char,4> texel(GLuint texture,GLint expected){glBindTexture(GL_TEXTURE_2D,texture);GLint format=0,w=0,h=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&format);
 // The existing shared linear uploader requests unsized GL_RGBA. Mesa drivers
 // may report that enum or its canonical GL_RGBA8; verify actual component
 // widths before accepting the unsized spelling. sRGB remains a strict role.
 if(format==GL_RGBA&&expected==GL_RGBA8){for(auto component:{GL_TEXTURE_RED_SIZE,GL_TEXTURE_GREEN_SIZE,GL_TEXTURE_BLUE_SIZE,GL_TEXTURE_ALPHA_SIZE}){GLint bits=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,0,component,&bits);assert(bits==8);}format=GL_RGBA8;}
 if(format!=expected){std::fprintf(stderr,"Texture internal format 0x%x; expected 0x%x\n",unsigned(format),unsigned(expected));}
 assert(format==expected);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&w);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&h);std::vector<unsigned char> data(std::size_t(w)*h*4);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,data.data());return {data[0],data[1],data[2],data[3]};}
void texture_roles_and_sharing(){
 auto asset=textured_asset();std::string error;const auto saved=ar::capture_bindings();auto *a=ar::create(asset,1,error),*b=ar::create(asset,1,error);assert(a&&b);assert(same(saved,ar::capture_bindings()));ar::MaterialTextures aa,bb;assert(ar::material_textures(a,0,aa)&&ar::material_textures(b,0,bb));assert(aa.maps==bb.maps&&aa.maps[0]==aa.maps[3]);texel(aa.maps[0],GL_SRGB8_ALPHA8);texel(aa.maps[1],GL_RGBA8);auto arm=texel(aa.maps[2],GL_RGBA8);assert(arm[0]==200&&arm[1]==128&&arm[2]==64);
 const auto cache=texture_cache_stats();assert(cache.entries>=6&&cache.references>=14);auto too_big=ar::create(asset,1,error,1);assert(!too_big&&texture_cache_stats().references==cache.references);auto bad=asset;bad.images[0].encoded={1,2,3,4};assert(!ar::create(bad,1,error)&&texture_cache_stats().references==cache.references);
 auto no_ao=asset;no_ao.materials[0].occlusion.image=-1;auto *derived=ar::create(no_ao,1,error);assert(derived);ar::MaterialTextures maps;assert(ar::material_textures(derived,0,maps));arm=texel(maps.maps[2],GL_RGBA8);assert(arm[0]==255&&arm[1]==128&&arm[2]==64);ar::destroy(derived);
 auto ao_only=asset;ao_only.materials[0].metallic_roughness.image=-1;ao_only.materials[0].occlusion_strength=.5f;derived=ar::create(ao_only,1,error);assert(derived&&ar::material_textures(derived,0,maps));arm=texel(maps.maps[2],GL_RGBA8);assert(arm[0]==228&&arm[1]==255&&arm[2]==255);ar::destroy(derived);
 auto mixed_role=asset;mixed_role.materials[0].normal=mixed_role.materials[0].base;derived=ar::create(mixed_role,1,error);assert(derived&&ar::material_textures(derived,0,maps));assert(maps.maps[0]!=maps.maps[1]);texel(maps.maps[0],GL_SRGB8_ALPHA8);texel(maps.maps[1],GL_RGBA8);ar::destroy(derived);
 ar::destroy(a);assert(glIsTexture(bb.maps[0]));ar::destroy(b);assert(texture_cache_stats().references==0&&texture_cache_stats().entries==0);
}
Asset quad(){
 auto source=textured_asset();source.skeleton.joints={{"Root",-1,{}},{"Mesh",-1,{}}};source.skeleton.mesh_node=1;source.skeleton.palette={0};source.skeleton.inverse_bind={Mat4{}};source.clips.clear();source.mesh.vertices.resize(4);const Vec3 points[]={{-.8f,-.8f,0},{.8f,-.8f,0},{.8f,.8f,0},{-.8f,.8f,0}};const std::array<float,2> uv[]={{0,0},{1,0},{1,1},{0,1}};for(unsigned i=0;i<4;i++){auto &v=source.mesh.vertices[i];v.position=points[i];v.normal={0,0,1};v.uv=uv[i];v.tangent={1,0,0,1};v.joints={0,0,0,0};v.weights={1,0,0,0};}source.mesh.indices={0,1,2,0,2,3};source.mesh.primitives={{0,6,0,true,true,true}};return source;
}
void actual_alpha_depth_and_srgb(const std::array<GLuint,3> &programs){
 auto asset=quad();std::string error;assert(validate(asset,error));auto *r=ar::create(asset,1,error);assert(r);for(auto p:programs)assert(ar::warm(r,p,error));Pose pose;assert(rest_pose(asset.skeleton,pose,error));ar::Instance instance;auto bindings=ar::capture_bindings();assert(ar::upload(r,&pose,&instance,1,bindings,error));
 GLuint fbo=0,color=0,depth=0;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,64,64,0,GL_RGBA,GL_FLOAT,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);glGenTextures(1,&depth);glBindTexture(GL_TEXTURE_2D,depth);glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,64,64,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depth,0);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);glViewport(0,0,64,64);glDisable(GL_BLEND);glDisable(GL_FRAMEBUFFER_SRGB);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDepthFunc(GL_LESS);glDisable(GL_CULL_FACE);
 Mat4 vp;vp.m[10]=.1f;std::array<float,2> depths={};
 for(unsigned pass:{0u,2u}){const auto p=programs[pass];glUseProgram(p);glUniformMatrix4fv(glGetUniformLocation(p,"uVP"),1,GL_FALSE,vp.m.data());vec3(p,"uOffset",0,0,-3);vec3(p,"uSunColor",0,0,0);vec3(p,"uAmbient",0,0,0);vec3(p,"uSkyLight",0,0,0);vec3(p,"uGroundLight",0,0,0);scalar(p,"uHaze",0);scalar(p,"uWetness",0);glClearColor(0,0,0,0);glClearDepth(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);bindings=ar::capture_bindings();assert(ar::draw(r,p,bindings,error));assert(same(bindings,ar::capture_bindings()));float left=0,right=0;glReadPixels(20,32,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&left);glReadPixels(44,32,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&right);assert(near(left,1)&&right<.99f);depths[pass==0?0:1]=right;if(pass==0){std::array<float,4> pixel;glReadPixels(44,32,1,1,GL_RGBA,GL_FLOAT,pixel.data());const float srgb128=std::pow((128/255.f+.055f)/1.055f,2.4f),srgb64=std::pow((64/255.f+.055f)/1.055f,2.4f);assert(near(pixel[0],srgb128*.2f,.002f)&&near(pixel[1],srgb64*.1f,.002f));assert(pixel[3]>.1f&&pixel[3]<.2f);}}
 assert(near(depths[0],depths[1]));glBindFramebuffer(GL_FRAMEBUFFER,0);glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&color);glDeleteTextures(1,&depth);ar::destroy(r);assert(texture_cache_stats().references==0);
}
void bounded_program_replacement_cache(){
 std::array<GLuint,10> replacements;for(auto &p:replacements)p=program("shaders/world_foliage_shadow.fs.glsl");
 const auto asset=imported();std::string error;auto *r=ar::create(asset,1,error);assert(r);const auto bindings=ar::capture_bindings();
 for(unsigned i=0;i<8;i++)assert(ar::warm(r,replacements[i],error));
 assert(!ar::warm(r,replacements[8],error)); // Eight live programs remain bounded.
 for(unsigned round=0;round<7;round++){
  world_program_epoch++;const auto a=replacements[(round*2)%10],b=replacements[(round*2+1)%10];
  glUseProgram(a);hot=true;assert(!ar::draw(r,a,bindings,error));hot=false;
  assert(ar::warm(r,a,error)&&ar::warm(r,b,error));
  for(auto p:{a,b}){glUseProgram(p);hot=true;assert(ar::draw(r,p,bindings,error));hot=false;}
 }
 ar::destroy(r);glUseProgram(0);for(auto p:replacements)glDeleteProgram(p);assert(texture_cache_stats().references==0);
}
void authored_graph_request_and_reset(){
 const auto vertex=shader("shaders/world_skeletal.vs.glsl");
 const char *fragment="#version 330 core\nuniform int uStudioGraphIndex;out vec4 color;void main(){color=vec4(float(uStudioGraphIndex+1)/8.,.25,.5,1.);}";
 const GLuint p=GLH_program(vertex.c_str(),fragment);assert(p);glUseProgram(p);Mat4 identity;glUniformMatrix4fv(glGetUniformLocation(p,"uVP"),1,GL_FALSE,identity.m.data());
 const GLint graph=glGetUniformLocation(p,"uStudioGraphIndex");assert(graph>=0);
 auto asset=quad();std::string error;auto *r=ar::create(asset,1,error);assert(r&&ar::warm(r,p,error));Pose pose;assert(rest_pose(asset.skeleton,pose,error));ar::Instance instance;
 GLuint fbo=0,texture=0;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,16,16,0,GL_RGBA,GL_FLOAT,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);glViewport(0,0,16,16);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
 auto bindings=ar::capture_bindings();assert(ar::upload(r,&pose,&instance,1,bindings,error));
 for(int requested:{-1,3,7}){
  bindings.studio_graph_index=requested;glUniform1i(graph,5);glClearColor(-1,-1,-1,-1);glClear(GL_COLOR_BUFFER_BIT);
  hot=true;assert(ar::draw(r,p,bindings,error));hot=false;
  std::array<float,4> pixel;glReadPixels(8,8,1,1,GL_RGBA,GL_FLOAT,pixel.data());assert(near(pixel[0],float(requested+1)/8.));
  GLint restored=0;glGetUniformiv(p,graph,&restored);assert(restored== -1&&same(bindings,ar::capture_bindings()));
 }
 const auto draws=ar::stats(r).draws;
 for(int invalid:{-2,8}){bindings.studio_graph_index=invalid;glUniform1i(graph,5);hot=true;assert(!ar::draw(r,p,bindings,error));hot=false;GLint preserved=0;glGetUniformiv(p,graph,&preserved);assert(preserved==5&&ar::stats(r).draws==draws&&same(bindings,ar::capture_bindings()));}
 bindings.studio_graph_index=3;assert(ar::upload(r,nullptr,nullptr,0,bindings,error));assert(ar::draw(r,p,bindings,error));GLint reset=0;glGetUniformiv(p,graph,&reset);assert(reset== -1);
 glBindFramebuffer(GL_FRAMEBUFFER,0);glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&texture);ar::destroy(r);glUseProgram(0);glDeleteProgram(p);assert(texture_cache_stats().references==0&&glGetError()==GL_NO_ERROR);
}
void authored_vertex_displacement(){
 const std::array<GLuint,3> programs={program("shaders/world_foliage.fs.glsl",true),program("shaders/world_foliage.fs.glsl",true),program("shaders/world_foliage_shadow.fs.glsl",true)};
 auto asset=imported();std::string error;auto *r=ar::create(asset,1,error);assert(r);for(auto p:programs)assert(ar::warm(r,p,error));Pose pose;assert(sample(asset.skeleton,asset.clips[0],.7,false,pose,error));
 ar::Instance instance{.25f,.5f,-.2f,.8f,.3f};const auto bindings=ar::capture_bindings();assert(ar::upload(r,&pose,&instance,1,bindings,error));
 for(float time:{.83f,2.1f}){
  std::vector<Surface> first;
  for(unsigned pass=0;pass<programs.size();pass++){
   const auto p=programs[pass];glUseProgram(p);scalar(p,"uTime",time);glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),40000,-22000);vec3(p,"uOffset",float(pass*7),-float(pass*3),float(pass*5));
   const auto plain=capture(r,p,3),displaced=capture(r,p,3,2);
   for(unsigned v=0;v<3;v++){
    const float wx=plain[v][0]+40000,wz=plain[v][2]-22000;
    const float offsets[]={std::clamp(wx*.000001f+.025f*std::sin(time),-.15f,.15f),std::clamp(plain[v][1]*.01f,-.15f,.15f),std::clamp(wz*.000001f+.02f*std::sin(time),-.15f,.15f)};
    for(unsigned axis=0;axis<3;axis++)assert(near(displaced[v][axis],plain[v][axis]+offsets[axis],2.e-5));
    // Geometry moves once in world metres after the .8 instance scale. The
    // prior skin-normal/tangent frame is preserved by this bounded graph hook.
    for(unsigned axis=3;axis<12;axis++)assert(displaced[v][axis]==plain[v][axis]);
   }
   if(pass==0)first=displaced;else assert(displaced==first);
   GLint reset=0;glGetUniformiv(p,glGetUniformLocation(p,"uStudioGraphIndex"),&reset);assert(reset== -1);
  }
 }
 ar::destroy(r);glUseProgram(0);for(auto p:programs)glDeleteProgram(p);assert(texture_cache_stats().references==0&&glGetError()==GL_NO_ERROR);
}
}
int main(int argc,char **argv){
 std::setvbuf(stdout,nullptr,_IONBF,0);
 if(argc>1)assert(chdir(argv[1])==0);
 if(!std::getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
 assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);auto *window=SDL_CreateWindow("skeletal renderer contract",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);auto context=SDL_GL_CreateContext(window);assert(context);std::printf("Skeletal GL renderer: %s\n",glGetString(GL_RENDERER));
 const std::array<GLuint,3> programs={program("shaders/world_foliage.fs.glsl"),program("shaders/world_foliage.fs.glsl"),program("shaders/world_foliage_shadow.fs.glsl")};deformation_and_transactions(programs);texture_roles_and_sharing();actual_alpha_depth_and_srgb(programs);bounded_program_replacement_cache();authored_graph_request_and_reset();authored_vertex_displacement();assert(hot_lookups==0&&hot_queries==0&&hot_allocations==0);assert(glGetError()==GL_NO_ERROR);for(auto p:programs)glDeleteProgram(p);texture_cache_shutdown();assert(texture_cache_stats().entries==0);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();std::puts("world_animation_render: actual GPU skinning/shared pass geometry, alpha/depth/sRGB PBR, cache ownership, atomic invalid batches, bounded shader replacement cache, authored graph raster/reset/rejection and world-space vertex displacement across color/reflection/depth, binding restore and no hot lookup/query/reallocation passed");return 0;
}

/* Actual cat/dog pose/material import and shared color/shadow draws.
 * Software by default; root may opt into native with VALWOOD_TEST_NATIVE_GL=1.
 */
#define GL_GLEXT_PROTOTYPES
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_village_population.h"
#include "engine/world_village_pets.h"
#include "engine/world_atlas.h"
#include "engine/world_scenery.h"
#include "engine/world_models.h"
extern "C" {
#include "engine/texture_cache.h"
}
#include <array>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>
namespace {
constexpr int width=256,height=160;
using Matrix=std::array<float,16>;
using Pixel=std::array<float,4>;
std::string read(const std::filesystem::path &path){std::ifstream f(path);assert(f);std::ostringstream s;s<<f.rdbuf();return s.str();}
GLuint shader(GLenum type,const std::filesystem::path &path){
    auto source=read(path);const char *p=source.c_str();GLuint id=glCreateShader(type);glShaderSource(id,1,&p,nullptr);glCompileShader(id);
    GLint ok=0;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);if(!ok){char log[4096]={};glGetShaderInfoLog(id,sizeof(log),nullptr,log);std::fprintf(stderr,"%s: %s\n",path.c_str(),log);}assert(ok);return id;
}
GLuint program(const std::filesystem::path &root,const char *fragment){
    GLuint v=shader(GL_VERTEX_SHADER,root/"shaders/world_foliage.vs.glsl"),f=shader(GL_FRAGMENT_SHADER,root/"shaders"/fragment);
    GLuint p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);assert(ok);
    glUseProgram(p);glUniform1i(glGetUniformLocation(p,"uVillageSkyMask"),12);
    glUniform1i(glGetUniformLocation(p,"uLocalProbe0"),4);glUniform1i(glGetUniformLocation(p,"uLocalProbe1"),5);
    glUniform1i(glGetUniformLocation(p,"uLocalProbeCount"),0);glUniform1f(glGetUniformLocation(p,"uLocalIrradianceWeight"),0);
    glUniform1i(glGetUniformLocation(p,"uVillageSkyDirectional"),13);glUniform1i(glGetUniformLocation(p,"uVillageSkyUp"),14);
    glDeleteShader(v);glDeleteShader(f);return p;
}
Matrix vp(float yaw,float pitch){
    float sy=std::sin(yaw),cy=std::cos(yaw),sp=std::sin(pitch),cp=std::cos(pitch);
    Matrix v={cy,-sy*sp,-sy*cp,0,0,cp,-sp,0,sy,cy*sp,cy*cp,0,0,0,0,1},p={},r={};
    p[5]=1/std::tan(.54f);p[0]=p[5]*float(height)/width;p[10]=(15000.f+.4f)/(.4f-15000.f);p[11]=-1;p[14]=2*.4f*15000.f/(.4f-15000.f);
    for(int c=0;c<4;c++)for(int row=0;row<4;row++)for(int k=0;k<4;k++)r[c*4+row]+=p[k*4+row]*v[c*4+k];
    return r;
}
void f(GLuint p,const char *n,float a){glUniform1f(glGetUniformLocation(p,n),a);}
void v3(GLuint p,const char *n,float x,float y,float z){glUniform3f(glGetUniformLocation(p,n),x,y,z);}
void empty(){auto a=texture_cache_stats();assert(!a.entries&&!a.references);}
unsigned unexpected_upload(const void *,std::size_t,const TextureCachePolicy *){assert(!"Expected an already managed image");return 0;}
void unexpected_delete(unsigned){assert(!"Probe must not own the cached image's final reference");}
struct ImageKey {
    std::vector<unsigned char> encoded;
    TextureCachePolicy policy;
};
struct PoseContracts {
    std::vector<ImageKey> images;
    std::size_t references=0,source_bytes=0,rgba_mip_bytes=0,poses=0,max_triangles=0,max_primitives=0;
};
bool same_policy(const TextureCachePolicy &a,const TextureCachePolicy &b){
    return a.srgb==b.srgb&&a.min_filter==b.min_filter&&a.mag_filter==b.mag_filter&&a.wrap_s==b.wrap_s&&a.wrap_t==b.wrap_t&&a.mip_policy==b.mip_policy;
}
void image_contract(PoseContracts &contract,const cgltf_texture_view &view,bool srgb,bool opaque_material=false){
    if(!view.texture)return;
    assert(view.texcoord==0&&!view.has_transform);
    const auto *texture=view.texture;const auto *image=texture->image;
    assert(image&&image->buffer_view&&!image->uri);
    const auto *bytes=cgltf_buffer_view_data(image->buffer_view);const std::size_t length=image->buffer_view->size;
    assert(bytes&&length>0&&length<=64u*1024u*1024u);
    int width=0,height=0,channels=0;
    assert(stbi_info_from_memory(bytes,int(length),&width,&height,&channels));
    assert(width>0&&height>0&&width<=2048&&height<=2048);
    TextureCachePolicy policy={srgb,GL_LINEAR_MIPMAP_LINEAR,GL_LINEAR,GL_REPEAT,GL_REPEAT,1};
    if(texture->sampler){
        const auto &sampler=*texture->sampler;
        if(sampler.min_filter)policy.min_filter=unsigned(sampler.min_filter);
        if(sampler.mag_filter)policy.mag_filter=unsigned(sampler.mag_filter);
        if(sampler.wrap_s)policy.wrap_s=unsigned(sampler.wrap_s);
        if(sampler.wrap_t)policy.wrap_t=unsigned(sampler.wrap_t);
    }
    // Match the loader creation policy; actual support/QA gating is applied
    // after context creation when probing the existing managed entry.
    if(srgb&&opaque_material)policy.mip_policy|=1u<<16;
    contract.references++;
    for(const auto &key:contract.images)
        if(same_policy(key.policy,policy)&&key.encoded.size()==length&&!std::memcmp(key.encoded.data(),bytes,length))return;
    contract.images.push_back({std::vector<unsigned char>(bytes,bytes+length),policy});
    contract.source_bytes+=length;
    // RGBA8 is a conservative image-memory estimate. Count each effective
    // cache key, including color-space/sampler distinctions, and every mip.
    for(;;){contract.rgba_mip_bytes+=std::size_t(width)*std::size_t(height)*4;if(width==1&&height==1)break;width=std::max(1,width/2);height=std::max(1,height/2);}
    assert(contract.rgba_mip_bytes<=224u*1024u*1024u);
}
PoseContracts pose_contracts(const std::filesystem::path &models){
    PoseContracts contract;
    for(const char *species:{"cat","dog"})for(const char *mode:{"walk","idle","rest"})for(unsigned frame=0;frame<(!std::strcmp(mode,"walk")?8u:4u);frame++){
        char name[128];std::snprintf(name,sizeof(name),"village_pet_%s_%s_%02u.glb",species,mode,frame);
        auto path=models/name;cgltf_options options={};cgltf_data *data=nullptr;
        assert(cgltf_parse_file(&options,path.c_str(),&data)==cgltf_result_success);
        assert(cgltf_load_buffers(&options,data,path.c_str())==cgltf_result_success&&cgltf_validate(data)==cgltf_result_success);
        assert(!data->skins_count&&!data->animations_count&&data->meshes_count==1&&data->meshes[0].primitives_count==1);
        const auto &p=data->meshes[0].primitives[0];assert(p.type==cgltf_primitive_type_triangles&&p.indices&&p.material);
        const auto &m=*p.material;assert(m.has_pbr_metallic_roughness&&m.pbr_metallic_roughness.metallic_factor==0&&m.pbr_metallic_roughness.roughness_factor>.8f);
        image_contract(contract,m.pbr_metallic_roughness.base_color_texture,true,m.alpha_mode==cgltf_alpha_mode_opaque);image_contract(contract,m.normal_texture,false);
        const cgltf_accessor *positions=nullptr;for(std::size_t i=0;i<p.attributes_count;i++)if(p.attributes[i].type==cgltf_attribute_type_position)positions=p.attributes[i].data;
        assert(positions&&positions->has_min&&positions->has_max&&std::abs(positions->min[1])<.0001f&&positions->max[1]>.3&&positions->max[1]<.95);
        const auto triangles=p.indices->count/3;assert(triangles>1000&&triangles<=9000);
        contract.max_triangles=std::max(contract.max_triangles,triangles);contract.max_primitives=1;contract.poses++;cgltf_free(data);
    }
    assert(contract.poses==32&&contract.images.size()==3&&contract.references==48&&contract.rgba_mip_bytes<=12u*1024u*1024u);
    return contract;
}
}
int main(int argc,char **argv){
    auto root=std::filesystem::absolute(argc>1?argv[1]:".");auto models=root/"projects/valwood_world/models";
    const auto contracts=pose_contracts(models);
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window=SDL_CreateWindow("Village pets test",0,0,width,height,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);auto context=SDL_GL_CreateContext(window);assert(context);
    std::printf("Village pets renderer: %s\n",glGetString(GL_RENDERER));assert(world_atlas_init());
    auto temporary=std::filesystem::temp_directory_path()/("valwood-pets-gl-"+std::to_string(getpid()));std::filesystem::create_directory(temporary);
    for(const char *name:{"village_pet_cat_walk_00.glb","village_pet_cat_walk_01.glb"})std::filesystem::create_symlink(models/name,temporary/name);
    char error[1024]={};assert(!world_village_pets_load(temporary.c_str(),error,sizeof(error))&&std::strstr(error,"walk_02"));empty();std::filesystem::remove_all(temporary);
    // Load actual world scenery before pets, exercising shared source-image
    // ownership alongside the village's full architecture maps.
    std::array<std::string,4> scenery_paths;
    std::array<const char *,4> scenery_files;
    const char *source_names[]={"authored_instances.txt","starting_area_instances.txt","rainforest_instances.txt","castle_village_instances.txt"};
    for(std::size_t i=0;i<scenery_paths.size();++i){scenery_paths[i]=(root/"projects/valwood_world/scenery"/source_names[i]).string();scenery_files[i]=scenery_paths[i].c_str();}
    auto *scenery=world_scenery_load_files(scenery_files.data(),scenery_files.size(),models.c_str(),error,sizeof(error));
    if(!scenery)std::fprintf(stderr,"Integrated scenery load failed: %s\n",error);
    assert(scenery&&world_scenery_stats(scenery).source_files==4);
    const auto architecture_cache=texture_cache_stats();
    std::fprintf(stderr,"Full scenery cache: %zu entries, %.2fMiB source, %zu unmanaged uploads\n",architecture_cache.entries,double(architecture_cache.source_bytes)/(1024*1024),architecture_cache.unmanaged_uploads);
    assert(architecture_cache.entries>100&&architecture_cache.unmanaged_uploads==0);
    assert(architecture_cache.source_bytes+contracts.source_bytes<=TEXTURE_CACHE_SOURCE_BUDGET_BYTES);
    auto *population=world_village_pets_load(models.c_str(),error,sizeof(error));if(!population)std::fprintf(stderr,"%s\n",error);assert(population);
    auto stats=world_village_pets_stats(population);assert(stats.pets==12&&stats.cats==6&&stats.dogs==6&&stats.loaded_pose_assets==32);
    // The two species and every pose share exactly three source-image policies.
    auto cache=texture_cache_stats();assert(contracts.poses==32&&contracts.images.size()==3);
    assert(cache.entries<=architecture_cache.entries+contracts.images.size());
    // Every loaded pose also owns three shared neutral sampler references.
    assert(cache.references==architecture_cache.references+contracts.references+
        contracts.poses*WORLD_MODEL_NEUTRAL_TEXTURE_REFS);
    assert(cache.source_bytes<=architecture_cache.source_bytes+contracts.source_bytes&&cache.source_bytes<=TEXTURE_CACHE_SOURCE_BUDGET_BYTES);
    assert(cache.unmanaged_uploads==0&&cache.misses-architecture_cache.misses<=contracts.images.size());
    assert(cache.hits-architecture_cache.hits>=contracts.references-contracts.images.size());
    for(const auto &key:contracts.images){
        TextureCachePolicy effective=key.policy;
        const char *disabled=SDL_getenv("VALWOOD_QA_DISABLE_OPAQUE_BC7");
        if(!SDL_GL_ExtensionSupported("GL_ARB_texture_compression_bptc")||(disabled&&!std::strcmp(disabled,"1")))effective.mip_policy&=~(1u<<16);
        if(SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic")||SDL_GL_ExtensionSupported("GL_ARB_texture_filter_anisotropic")){
            float maximum=1;glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,&maximum);
            if(std::isfinite(maximum)&&maximum>=2)effective.mip_policy|=unsigned(std::min(4.f,maximum))<<8;
        }
        unsigned id=texture_cache_acquire(key.encoded.data(),key.encoded.size(),&effective,unexpected_upload,unexpected_delete);
        assert(id&&texture_cache_release(id));
    }
    std::printf("Integrated scenery+pets cache: %zu scenery entries/%zu pose keys; %.2fMiB encoded source of768MiB; zero unmanaged uploads PASS\n",architecture_cache.entries,contracts.images.size(),double(cache.source_bytes)/(1024*1024));
    GLuint color_program=program(root,"world_foliage.fs.glsl"),shadow_program=program(root,"world_foliage_shadow.fs.glsl");
    GLuint framebuffer,color,depth,neutral[2];glGenFramebuffers(1,&framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
    glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,width,height,0,GL_RGBA,GL_FLOAT,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
    glGenTextures(1,&depth);glBindTexture(GL_TEXTURE_2D,depth);glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,width,height,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depth,0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);glGenTextures(2,neutral);
    for(int i=0;i<2;i++){float value=i?24.f:1.f;glActiveTexture(GL_TEXTURE7+i*4);glBindTexture(GL_TEXTURE_2D,neutral[i]);glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,1,1,0,GL_RED,GL_FLOAT,&value);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);}
    double origin_x,origin_z;world_atlas_village_origin(&origin_x,&origin_z);
    std::vector<Pixel> pixels(width*height),previous;std::vector<float> depths(width*height),color_depths(width*height);
    std::size_t colored_total=0,shadow_total=0,fractional_poses=0;double animation_delta=0;
    for(int frame=0;frame<3;frame++){
        // Adjacent display frames must animate between baked keys, not merely
        // demonstrate a different pose after a whole eight-key step.
        double x=origin_x+22,z=origin_z+50.5,y=24.7;float time=17+frame/60.f;
        world_village_pets_update(population,x,y,z,time,1.f/60);stats=world_village_pets_stats(population);assert(stats.visible>0&&stats.draw_groups>0&&stats.draw_groups<=world_village_pet_pose_count);
        for(std::size_t i=0;i<world_village_pet_count;i++){WorldVillagePet resident;assert(world_village_pets_pet(population,i,&resident));fractional_poses+=resident.pose_blend>.001f&&resident.pose_blend<.999f;}
        auto matrix=vp(0,-.12f);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glViewport(0,0,width,height);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glColorMask(1,1,1,1);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);glReadBuffer(GL_COLOR_ATTACHMENT0);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glUseProgram(color_program);glUniformMatrix4fv(glGetUniformLocation(color_program,"uVP"),1,GL_FALSE,matrix.data());glUniformMatrix4fv(glGetUniformLocation(color_program,"uLightVP"),1,GL_FALSE,matrix.data());
        v3(color_program,"uSun",.3f,.8f,.2f);v3(color_program,"uSunColor",1,.9f,.8f);v3(color_program,"uAmbient",.35f,.4f,.45f);v3(color_program,"uFog",.4f,.5f,.6f);
        glUniform1i(glGetUniformLocation(color_program,"uShadow"),7);glUniform1i(glGetUniformLocation(color_program,"uMacroHeight"),11);glUniform2f(glGetUniformLocation(color_program,"uWorldSize"),float(world_atlas_width()),float(world_atlas_depth()));f(color_program,"uTime",time);
        world_village_pets_draw(population,color_program,x,y,z);
        float reactive=-1;glGetUniformfv(color_program,glGetUniformLocation(color_program,"uActorReactive"),&reactive);assert(reactive==0);
        glReadPixels(0,0,width,height,GL_RGBA,GL_FLOAT,pixels.data());std::size_t colored=0;
        for(auto pixel:pixels){for(float c:pixel)assert(std::isfinite(c));if(pixel[0]||pixel[1]||pixel[2]){colored++;assert(std::abs(pixel[3]-.15f)<.002f);}}
        assert(colored>10);colored_total+=colored;
        glReadPixels(0,0,width,height,GL_DEPTH_COMPONENT,GL_FLOAT,color_depths.data());
        if(!previous.empty())for(std::size_t i=0;i<pixels.size();i++)for(int c=0;c<3;c++)animation_delta+=std::abs(pixels[i][c]-previous[i][c]);
        previous=pixels;
        glUseProgram(shadow_program);glUniformMatrix4fv(glGetUniformLocation(shadow_program,"uVP"),1,GL_FALSE,matrix.data());glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);glClear(GL_DEPTH_BUFFER_BIT);
        world_village_pets_draw(population,shadow_program,x,y,z,true);glReadPixels(0,0,width,height,GL_DEPTH_COMPONENT,GL_FLOAT,depths.data());
        for(std::size_t i=0;i<depths.size();i++){const float d=depths[i];assert(std::isfinite(d)&&d>=0&&d<=1);if(d<.999999f)shadow_total++;assert(std::abs(d-color_depths[i])<.000002f);}
        auto uploads=world_village_pets_stats(population).uploads;
        world_village_pets_draw(population,color_program,x,-y,z);world_village_pets_update(population,x,y,z,time,1.f/60);
        assert(world_village_pets_stats(population).uploads==uploads&&glGetError()==GL_NO_ERROR);
    }
    assert(animation_delta>.01&&shadow_total>10&&fractional_poses>5);
    world_village_pets_free(population);
    auto released=texture_cache_stats();assert(released.entries==architecture_cache.entries&&released.references==architecture_cache.references&&released.source_bytes==architecture_cache.source_bytes);
    world_scenery_free(scenery);empty();glDeleteProgram(color_program);glDeleteProgram(shadow_program);glDeleteTextures(2,neutral);glDeleteTextures(1,&color);glDeleteTextures(1,&depth);glDeleteFramebuffers(1,&framebuffer);
    world_atlas_shutdown();assert(glGetError()==GL_NO_ERROR);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    std::printf("PASS village pets GL: %zu actual poses/%zu shared effective image keys, partial-load cleanup,12pets/2species,%zu fractional pose samples across60Hz frames, animated image delta%.3f, %zu color/%zu shadow pixels with matching depths, reactivealpha.15/reset, mirrored reuse/stableuploads andzero texture owners\n",contracts.poses,contracts.images.size(),fractional_poses,animation_delta,colored_total,shadow_total);
}

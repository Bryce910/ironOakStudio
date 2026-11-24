/* Actual downloaded-character pose/material import and population color/shadow draws.
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
#include "tests/world_village_population_runtime_cases.h"
namespace {
constexpr int width=256,height=160;
using Matrix=std::array<float,16>;
using Pixel=std::array<float,4>;
std::string read(const std::filesystem::path &path){std::ifstream f(path);assert(f);std::ostringstream s;s<<f.rdbuf();return s.str();}
GLuint shader(GLenum type,const std::filesystem::path &path,bool character=false){
    auto source=read(path);if(character)source.insert(source.find('\n')+1,"#define VALWOOD_CHARACTER_APPEARANCE 1\n");const char *p=source.c_str();GLuint id=glCreateShader(type);glShaderSource(id,1,&p,nullptr);glCompileShader(id);
    GLint ok=0;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);if(!ok){char log[4096]={};glGetShaderInfoLog(id,sizeof(log),nullptr,log);std::fprintf(stderr,"%s: %s\n",path.c_str(),log);}assert(ok);return id;
}
GLuint program(const std::filesystem::path &root,const char *fragment,bool character=false){
    GLuint v=shader(GL_VERTEX_SHADER,root/"shaders/world_foliage.vs.glsl",character),f=shader(GL_FRAGMENT_SHADER,root/"shaders"/fragment,character);
    GLuint p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);assert(ok);
    glUseProgram(p);glUniform1i(glGetUniformLocation(p,"uVillageSkyMask"),12);
    glUniform1i(glGetUniformLocation(p,"uLocalProbe0"),4);glUniform1i(glGetUniformLocation(p,"uLocalProbe1"),5);
    glUniform1i(glGetUniformLocation(p,"uLocalProbeCount"),0);glUniform1f(glGetUniformLocation(p,"uLocalIrradianceWeight"),0);
    glUniform1i(glGetUniformLocation(p,"uVillageSkyDirectional"),13);glUniform1i(glGetUniformLocation(p,"uVillageSkyUp"),14);
    glDeleteShader(v);glDeleteShader(f);return p;
}
// Test/loading only: compare variants under every identical active uniform,
// including source samplers, arrays, weather and lighting. No production draw
// uses introspection or uniform copying.
void copy_uniforms(GLuint from,GLuint to){
    GLint count=0;glGetProgramiv(from,GL_ACTIVE_UNIFORMS,&count);glUseProgram(to);
    for(GLint i=0;i<count;i++){
        char name[256];GLint size=0;GLenum type=0;glGetActiveUniform(from,GLuint(i),sizeof name,nullptr,&size,&type,name);
        const GLint source=glGetUniformLocation(from,name),target=glGetUniformLocation(to,name);if(source<0||target<0)continue;
        // Driver glGetUniform reads one array element at a time.
        const auto array=std::string(name).find("[0]");
        for(GLint element=0;element<size;element++){
            std::string field=name;if(array!=std::string::npos)field.replace(array,3,"["+std::to_string(element)+"]");
            const GLint a=glGetUniformLocation(from,field.c_str()),b=glGetUniformLocation(to,field.c_str());if(a<0||b<0)continue;
            GLfloat f[16]={};GLint n[4]={};
            if(type==GL_FLOAT||type==GL_FLOAT_VEC2||type==GL_FLOAT_VEC3||type==GL_FLOAT_VEC4||type==GL_FLOAT_MAT2||type==GL_FLOAT_MAT3||type==GL_FLOAT_MAT4){
                glGetUniformfv(from,a,f);
                if(type==GL_FLOAT)glUniform1fv(b,1,f);else if(type==GL_FLOAT_VEC2)glUniform2fv(b,1,f);else if(type==GL_FLOAT_VEC3)glUniform3fv(b,1,f);else if(type==GL_FLOAT_VEC4)glUniform4fv(b,1,f);else if(type==GL_FLOAT_MAT2)glUniformMatrix2fv(b,1,GL_FALSE,f);else if(type==GL_FLOAT_MAT3)glUniformMatrix3fv(b,1,GL_FALSE,f);else glUniformMatrix4fv(b,1,GL_FALSE,f);
            }else{
                assert(type==GL_INT||type==GL_BOOL||type==GL_SAMPLER_2D||type==GL_SAMPLER_3D||type==GL_SAMPLER_CUBE||type==GL_SAMPLER_2D_ARRAY||type==GL_SAMPLER_BUFFER||type==GL_SAMPLER_2D_SHADOW);glGetUniformiv(from,a,n);glUniform1iv(b,1,n);
            }
        }
    }
    assert(glGetError()==GL_NO_ERROR);
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
    // Original213.31MiB plus the explicit16MiB seated source atlas budget.
    // Retain the former224MiB allowance and append only that measured budget.
    assert(contract.rgba_mip_bytes<=(224u+16u)*1024u*1024u);
}
bool prefix(const char *name,const char *value){return name&&!std::strncmp(name,value,std::strlen(value));}
PoseContracts pose_contracts(const std::filesystem::path &models){
    PoseContracts contract;
    for(const auto &appearance:world_village_pose_archetypes){
      const bool source_seated=!std::strcmp(appearance.name,"seated");
      const bool source_female=!std::strcmp(appearance.file_prefix,"village_character")&&
          (!std::strcmp(appearance.name,"traderwoman")||!std::strcmp(appearance.name,"travelerwoman"));
      std::size_t appearance_poses=0;
      for(unsigned mode=0;mode<2;mode++){
        const unsigned count=mode?appearance.idle_count:appearance.walk_count;
        for(unsigned frame=0;frame<count;frame++){
            char name[160];std::snprintf(name,sizeof(name),"%s_%s_%s_%02u.glb",appearance.file_prefix,appearance.name,mode?"idle":"walk",frame);
            auto path=models/name;cgltf_options options={};cgltf_data *data=nullptr;
            assert(cgltf_parse_file(&options,path.c_str(),&data)==cgltf_result_success);
            assert(!data->skins_count&&!data->animations_count&&data->meshes_count>0);
            assert(cgltf_load_buffers(&options,data,path.c_str())==cgltf_result_success);
            assert(cgltf_validate(data)==cgltf_result_success);
            for(std::size_t mesh=0;mesh<data->meshes_count;mesh++){
                if(data->meshes[mesh].name)assert(!std::strstr(data->meshes[mesh].name,"Icosphere"));
                for(std::size_t primitive=0;primitive<data->meshes[mesh].primitives_count;primitive++)
                    assert(data->meshes[mesh].primitives[primitive].material);
            }
            // Clothing, skin and hair may be separate source meshes. Measure
            // their tagged union, excluding equipment and any rig widgets.
            float low=100,high=-100;std::size_t body_vertices=0,triangles=0,primitives=0;
            for(std::size_t n=0;n<data->nodes_count;n++)if(data->nodes[n].mesh){
                float m[16];cgltf_node_transform_world(&data->nodes[n],m);
                for(float value:m)assert(std::isfinite(value));
                const auto &mesh=*data->nodes[n].mesh;
                const bool body=prefix(data->nodes[n].name,"Body_")||prefix(mesh.name,"Body_");
                const bool gear=prefix(data->nodes[n].name,"Gear_")||prefix(mesh.name,"Gear_");
                assert(body!=gear);
                if(data->nodes[n].name)assert(!std::strstr(data->nodes[n].name,"Icosphere"));
                for(std::size_t pi=0;pi<mesh.primitives_count;pi++){
                    const auto &primitive=mesh.primitives[pi];assert(primitive.type==cgltf_primitive_type_triangles&&!primitive.has_draco_mesh_compression);
                    const cgltf_accessor *positions=nullptr;
                    for(std::size_t ai=0;ai<primitive.attributes_count;ai++)if(primitive.attributes[ai].type==cgltf_attribute_type_position)positions=primitive.attributes[ai].data;
                    assert(positions&&positions->count);
                    const auto index_count=primitive.indices?primitive.indices->count:positions->count;assert(index_count>0&&index_count%3==0);
                    triangles+=index_count/3;primitives++;
                    const auto &material=*primitive.material;
                    image_contract(contract,material.pbr_metallic_roughness.base_color_texture,true,material.alpha_mode==cgltf_alpha_mode_opaque);
                    image_contract(contract,material.normal_texture,false);
                    image_contract(contract,material.pbr_metallic_roughness.metallic_roughness_texture,false);
                    if(!body)continue;
                    for(std::size_t v=0;v<positions->count;v++){
                        float p[3];assert(cgltf_accessor_read_float(positions,v,p,3));
                        for(float value:p)assert(std::isfinite(value));
                        const float y=m[1]*p[0]+m[5]*p[1]+m[9]*p[2]+m[13];assert(std::isfinite(y));low=std::min(low,y);high=std::max(high,y);body_vertices++;
                    }
                }
            }
            assert(body_vertices&&low>=-.0001f&&low<=.035f&&high-low>=(source_seated?1.55f:1.68f)&&high-low<=(source_seated?1.65f:1.92f));
            assert(triangles>0&&triangles<=20000&&primitives>0&&primitives<=8);
            if(source_female){
                // The real source character keeps cloth/skin, metal, alpha
                // hair, opaque eyes and alpha lashes in stable primitives.
                // Do not let an old diffuse-only placeholder satisfy this
                // fixture merely because it still has the right pose names.
                assert(triangles==5181&&primitives==5&&data->materials_count==5);
                assert(data->materials[0].normal_texture.texture&&data->materials[0].occlusion_texture.texture);
                assert(data->materials[0].pbr_metallic_roughness.metallic_roughness_texture.texture);
                assert(data->materials[0].alpha_mode==cgltf_alpha_mode_opaque);
                assert(data->materials[0].pbr_metallic_roughness.metallic_factor==0);
                assert(data->materials[1].pbr_metallic_roughness.metallic_factor>.6f&&data->materials[1].pbr_metallic_roughness.metallic_factor<.8f);
                assert(data->materials[3].alpha_mode==cgltf_alpha_mode_opaque);
                assert(data->materials[3].normal_texture.texture&&data->materials[3].pbr_metallic_roughness.roughness_factor==.25f);
                for(std::size_t i:{2u,4u}){
                    const auto &material=data->materials[i];
                    assert(material.alpha_mode==cgltf_alpha_mode_mask&&std::fabs(material.alpha_cutoff-.4f)<1e-6f);
                    assert(material.double_sided&&material.pbr_metallic_roughness.base_color_texture.texture);
                }
            }
            contract.max_triangles=std::max(contract.max_triangles,triangles);contract.max_primitives=std::max(contract.max_primitives,primitives);
            cgltf_free(data);contract.poses++;appearance_poses++;
        }
      }
      assert(appearance_poses==appearance.walk_count+appearance.idle_count&&appearance_poses>0);
    }
    assert(contract.poses==world_village_pose_group_count&&!contract.images.empty()&&contract.references>contract.images.size());
    std::printf("Pose contracts:%zu tagged multi-mesh bodies across10 appearances; no rig display geometry/skins/animations; soles0..35mm, standingheight1.68..1.92m/seated1.55..1.65m, maximum%zu triangles/%zu primitives; %zu effective image keys, %.2fMiB estimated RGBA8+mips PASS\n",contract.poses,contract.max_triangles,contract.max_primitives,contract.images.size(),double(contract.rgba_mip_bytes)/(1024*1024));
    return contract;
}
}
int main(int argc,char **argv){
    auto root=std::filesystem::absolute(argc>1?argv[1]:".");auto models=root/"projects/valwood_world/models";
    const auto contracts=pose_contracts(models);
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window=SDL_CreateWindow("Village population test",0,0,width,height,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);auto context=SDL_GL_CreateContext(window);assert(context);
    std::printf("Village population renderer: %s\n",glGetString(GL_RENDERER));assert(world_atlas_init());
    auto temporary=std::filesystem::temp_directory_path()/("valwood-population-gl-"+std::to_string(getpid()));std::filesystem::create_directory(temporary);
    for(const char *name:{"village_character_knight_walk_00.glb","village_character_knight_walk_01.glb"})std::filesystem::create_symlink(models/name,temporary/name);
    char error[1024]={};assert(!world_village_population_load(temporary.c_str(),error,sizeof(error))&&std::strstr(error,"walk_02"));empty();std::filesystem::remove_all(temporary);
    // Match the real viewer's load order: all world scenery, including the full
    // architecture maps, precedes every animated pose. A population-only test
    // missed the old 256 MiB source-cache exhaustion and unmanaged GPU copies.
    std::array<std::string,6> scenery_paths;
    std::array<const char *,6> scenery_files;
    const char *source_names[]={"authored_instances.txt","starting_area_instances.txt","rainforest_instances.txt","castle_village_instances.txt","tanthoon_bandit_camp_instances.txt","castle_streets_instances.txt"};
    for(std::size_t i=0;i<scenery_paths.size();++i){scenery_paths[i]=(root/"projects/valwood_world/scenery"/source_names[i]).string();scenery_files[i]=scenery_paths[i].c_str();}
    auto *scenery=world_scenery_load_files(scenery_files.data(),scenery_files.size(),models.c_str(),error,sizeof(error));
    if(!scenery)std::fprintf(stderr,"Integrated scenery load failed: %s\n",error);
    assert(scenery&&world_scenery_stats(scenery).source_files==6);
    const auto architecture_cache=texture_cache_stats();
    std::fprintf(stderr,"Full scenery cache: %zu entries, %.2fMiB source, %zu unmanaged uploads\n",architecture_cache.entries,double(architecture_cache.source_bytes)/(1024*1024),architecture_cache.unmanaged_uploads);
    assert(architecture_cache.entries>100&&architecture_cache.unmanaged_uploads==0);
    assert(architecture_cache.source_bytes+contracts.source_bytes<=TEXTURE_CACHE_SOURCE_BUDGET_BYTES);
    auto *population=world_village_population_load(models.c_str(),error,sizeof(error));if(!population)std::fprintf(stderr,"%s\n",error);assert(population);
    auto stats=world_village_population_stats(population);assert(stats.residents==world_village_resident_count&&stats.loaded_pose_assets==world_village_pose_group_count&&stats.archers==16&&stats.traders==20&&stats.knights==16&&stats.travelers==76);
    // Two old female diffuse atlases are replaced by seven shared source-PBR
    // image policies, used by both roles and every one of their sixteen poses.
    auto cache=texture_cache_stats();assert(contracts.poses==80&&contracts.images.size()==48);
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
    std::printf("Integrated scenery+population cache: %zu scenery entries/%zu pose keys; %.2fMiB encoded source of768MiB; zero unmanaged uploads PASS\n",architecture_cache.entries,contracts.images.size(),double(cache.source_bytes)/(1024*1024));
    GLuint color_program=program(root,"world_foliage.fs.glsl"),shadow_program=program(root,"world_foliage_shadow.fs.glsl");
    GLuint framebuffer,color,depth,neutral[2];glGenFramebuffers(1,&framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
    glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,width,height,0,GL_RGBA,GL_FLOAT,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
    glGenTextures(1,&depth);glBindTexture(GL_TEXTURE_2D,depth);glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,width,height,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depth,0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);glGenTextures(2,neutral);
    for(int i=0;i<2;i++){float value=i?24.f:1.f;glActiveTexture(GL_TEXTURE7+i*4);glBindTexture(GL_TEXTURE_2D,neutral[i]);glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,1,1,0,GL_RED,GL_FLOAT,&value);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);}
    double origin_x,origin_z;world_atlas_village_origin(&origin_x,&origin_z);
    std::vector<Pixel> pixels(width*height),previous,culled_pixels(width*height);std::vector<float> depths(width*height),color_depths(width*height),culled_depths(width*height);
    std::size_t colored_total=0,shadow_total=0,fractional_poses=0;double animation_delta=0;
    for(int frame=0;frame<6;frame++){
        // Adjacent display frames must animate between baked keys, not merely
        // demonstrate a different pose after a whole eight-key step.
        double x=origin_x,z=origin_z-65,y=25.75;float view_yaw=0;
        // Isolate the new opaque seated source in profile. A front view also
        // contains existing masked merchant hair: color intentionally uses
        // alpha antialiasing while sun shadows use a hard cutoff, so those
        // background pixels are not an exact geometry-depth reference.
        if(frame>=3){x=origin_x-146.4;z=origin_z-79.8;y=25.4;view_yaw=-1.57079632679f;}
        float time=17+(frame%3)/60.f;
        world_village_population_update(population,x,y,z,time,1.f/60);stats=world_village_population_stats(population);assert(stats.visible>0&&stats.draw_groups>0&&stats.draw_groups<=world_village_pose_group_count);
        for(std::size_t i=0;i<world_village_resident_count;i++){WorldVillageResident resident;assert(world_village_population_resident(population,i,&resident));fractional_poses+=resident.pose_blend>.001f&&resident.pose_blend<.999f;}
        auto matrix=vp(view_yaw,-.06f);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glViewport(0,0,width,height);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glColorMask(1,1,1,1);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);glReadBuffer(GL_COLOR_ATTACHMENT0);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glUseProgram(color_program);glUniformMatrix4fv(glGetUniformLocation(color_program,"uVP"),1,GL_FALSE,matrix.data());glUniformMatrix4fv(glGetUniformLocation(color_program,"uLightVP"),1,GL_FALSE,matrix.data());
        v3(color_program,"uSun",.3f,.8f,.2f);v3(color_program,"uSunColor",1,.9f,.8f);v3(color_program,"uAmbient",.35f,.4f,.45f);v3(color_program,"uFog",.4f,.5f,.6f);
        glUniform1i(glGetUniformLocation(color_program,"uShadow"),7);glUniform1i(glGetUniformLocation(color_program,"uMacroHeight"),11);glUniform2f(glGetUniformLocation(color_program,"uWorldSize"),float(world_atlas_width()),float(world_atlas_depth()));f(color_program,"uTime",time);
        world_village_population_draw(population,color_program,x,y,z);
        float reactive=-1;glGetUniformfv(color_program,glGetUniformLocation(color_program,"uActorReactive"),&reactive);assert(reactive==0);
        glReadPixels(0,0,width,height,GL_RGBA,GL_FLOAT,pixels.data());std::size_t colored=0;
        for(auto pixel:pixels){for(float c:pixel)assert(std::isfinite(c));if(pixel[0]||pixel[1]||pixel[2]){colored++;assert(std::abs(pixel[3]-.15f)<.002f);}}
        assert(colored>10);colored_total+=colored;
        glReadPixels(0,0,width,height,GL_DEPTH_COMPONENT,GL_FLOAT,color_depths.data());
        // Keep full source detail, material ordering and every shadow caster:
        // only discard whole batches proven outside the actual color VP.
        const auto before_cull=world_village_population_stats(population);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        world_village_population_draw_frustum(population,color_program,x,y,z,matrix.data());
        glReadPixels(0,0,width,height,GL_RGBA,GL_FLOAT,culled_pixels.data());
        glReadPixels(0,0,width,height,GL_DEPTH_COMPONENT,GL_FLOAT,culled_depths.data());
        assert(std::memcmp(pixels.data(),culled_pixels.data(),pixels.size()*sizeof(Pixel))==0);
        assert(std::memcmp(color_depths.data(),culled_depths.data(),color_depths.size()*sizeof(float))==0);
        const auto after_cull=world_village_population_stats(population);
        assert(after_cull.frustum_draw_calls==before_cull.frustum_draw_calls+1&&after_cull.uploads==before_cull.uploads);
        assert((after_cull.frustum_submitted_batches-before_cull.frustum_submitted_batches)+(after_cull.frustum_culled_batches-before_cull.frustum_culled_batches)==stats.draw_groups);
        if(!previous.empty()&&frame!=3)for(std::size_t i=0;i<pixels.size();i++)for(int c=0;c<3;c++)animation_delta+=std::abs(pixels[i][c]-previous[i][c]);
        previous=pixels;
        glUseProgram(shadow_program);glUniformMatrix4fv(glGetUniformLocation(shadow_program,"uVP"),1,GL_FALSE,matrix.data());glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);glClear(GL_DEPTH_BUFFER_BIT);
        world_village_population_draw(population,shadow_program,x,y,z,true);glReadPixels(0,0,width,height,GL_DEPTH_COMPONENT,GL_FLOAT,depths.data());
        for(std::size_t i=0;i<depths.size();i++){
            const float d=depths[i];assert(std::isfinite(d)&&d>=0&&d<=1);
            if(d<.999999f)shadow_total++;
            if(std::abs(d-color_depths[i])>=.000002f)std::fprintf(stderr,"Depth mismatch frame%d pixel%zu: shadow%.9f/color%.9f\n",frame,i,d,color_depths[i]);
            assert(std::abs(d-color_depths[i])<.000002f);
        }
        auto uploads=world_village_population_stats(population).uploads;
        // Reflection gets its own mirrored camera and actual VP. Compare the
        // entire target, even where the mirror clips a batch through a side or
        // near plane; the camera used by update must not leak into this test.
        auto reflected=vp(view_yaw,.06f);glUseProgram(color_program);
        glUniformMatrix4fv(glGetUniformLocation(color_program,"uVP"),1,GL_FALSE,reflected.data());
        glDrawBuffer(GL_COLOR_ATTACHMENT0);glReadBuffer(GL_COLOR_ATTACHMENT0);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        world_village_population_draw(population,color_program,x,-y,z);
        glReadPixels(0,0,width,height,GL_RGBA,GL_FLOAT,culled_pixels.data());
        glReadPixels(0,0,width,height,GL_DEPTH_COMPONENT,GL_FLOAT,culled_depths.data());
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        world_village_population_draw_frustum(population,color_program,x,-y,z,reflected.data());
        glReadPixels(0,0,width,height,GL_RGBA,GL_FLOAT,pixels.data());
        glReadPixels(0,0,width,height,GL_DEPTH_COMPONENT,GL_FLOAT,depths.data());
        assert(std::memcmp(pixels.data(),culled_pixels.data(),pixels.size()*sizeof(Pixel))==0);
        assert(std::memcmp(depths.data(),culled_depths.data(),depths.size()*sizeof(float))==0);
        world_village_population_update(population,x,y,z,time,1.f/60);
        assert(world_village_population_stats(population).uploads==uploads&&glGetError()==GL_NO_ERROR);
    }
    assert(animation_delta>1&&shadow_total>10&&fractional_poses>100);
    const auto frustum_stats=world_village_population_stats(population);
    assert(frustum_stats.frustum_draw_calls==12&&frustum_stats.frustum_culled_batches>0&&frustum_stats.frustum_submitted_batches>0);
    world_village_population_update(population,origin_x,25.75,origin_z-65,17,0);
    const auto visible_before_replace=world_village_population_stats(population).visible;
    assert(visible_before_replace>0&&!world_village_population_runtime_active(population,world_village_resident_count,false));
    for(unsigned i=0;i<world_village_resident_count;i++)assert(world_village_population_runtime_active(population,i,false));
    world_village_population_update(population,origin_x,25.75,origin_z-65,17,0);
    assert(world_village_population_stats(population).visible==0&&world_village_population_stats(population).draw_groups==0);
    for(unsigned i=0;i<world_village_resident_count;i++)assert(world_village_population_runtime_active(population,i,true));
    world_village_population_update(population,origin_x,25.75,origin_z-65,17,0);
    assert(world_village_population_stats(population).visible==visible_before_replace);
    std::printf("PASS population actual-GL frustum:12 main/reflected passes bit-exact RGBA/depth versus full draws; %llu whole batches culled/%llu submitted; shadows unchanged\n",static_cast<unsigned long long>(frustum_stats.frustum_culled_batches),static_cast<unsigned long long>(frustum_stats.frustum_submitted_batches));
    const auto before_runtime=texture_cache_stats();check_authored_population_runtime(population,origin_x,origin_z);const auto after_runtime=texture_cache_stats();assert(before_runtime.entries==after_runtime.entries&&before_runtime.references==after_runtime.references&&before_runtime.misses==after_runtime.misses);
    // An authored root elevated above its original navigation point must use
    // the exact same committed runtime pose in real color and shadow geometry.
    WorldVillageActorConfig controlled;controlled.source=0;controlled.appearance=0;controlled.count=1;controlled.points[0]={origin_x+60,24,origin_z+60};assert(world_village_population_configure(population,&controlled,1));
    for(unsigned i=1;i<world_village_resident_count;i++)assert(world_village_population_runtime_active(population,i,false));
    WorldVillageActorRuntime runtime{origin_x,28.25,origin_z-65,.2f,.75f,true};assert(world_village_population_actor_runtime(population,0,&runtime));
    const double rx=runtime.x,ry=runtime.y+1.1,rz=runtime.z+5;world_village_population_update(population,rx,ry,rz,103,1.f/60);assert(world_village_population_stats(population).visible==1);
    auto runtime_matrix=vp(0,-.06f);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glViewport(0,0,width,height);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glColorMask(1,1,1,1);
    glUseProgram(color_program);glUniformMatrix4fv(glGetUniformLocation(color_program,"uVP"),1,GL_FALSE,runtime_matrix.data());glUniformMatrix4fv(glGetUniformLocation(color_program,"uLightVP"),1,GL_FALSE,runtime_matrix.data());
    glDrawBuffer(GL_COLOR_ATTACHMENT0);glReadBuffer(GL_COLOR_ATTACHMENT0);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);world_village_population_draw(population,color_program,rx,ry,rz);glReadPixels(0,0,width,height,GL_DEPTH_COMPONENT,GL_FLOAT,color_depths.data());
    std::size_t controlled_pixels=0;for(float value:color_depths)controlled_pixels+=value<.999999f;assert(controlled_pixels>10);
    glUseProgram(shadow_program);glUniformMatrix4fv(glGetUniformLocation(shadow_program,"uVP"),1,GL_FALSE,runtime_matrix.data());glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);glClear(GL_DEPTH_BUFFER_BIT);world_village_population_draw(population,shadow_program,rx,ry,rz,true);glReadPixels(0,0,width,height,GL_DEPTH_COMPONENT,GL_FLOAT,depths.data());
    for(std::size_t i=0;i<depths.size();i++)assert(std::abs(depths[i]-color_depths[i])<.000002f);
    runtime.active=false;assert(world_village_population_actor_runtime(population,0,&runtime));world_village_population_update(population,rx,ry,rz,103,0);assert(world_village_population_stats(population).visible==0&&world_village_population_stats(population).draw_groups==0);
    WorldVillageResident suppressed;assert(!world_village_population_resident(population,0,&suppressed));glClear(GL_DEPTH_BUFFER_BIT);world_village_population_draw(population,shadow_program,rx,ry,rz,true);glReadPixels(0,0,width,height,GL_DEPTH_COMPONENT,GL_FLOAT,depths.data());for(float value:depths)assert(value==1);
    for(unsigned i=0;i<world_village_resident_count;i++)assert(world_village_population_runtime_active(population,i,true));
    assert(world_village_population_configure(population,nullptr,0));
    std::printf("PASS authored runtime real GL: exact elevated root, %zu matching color/shadow pixels, inactive geometry/query suppression and unchanged texture ownership\n",controlled_pixels);
    // One supported hair-card NPC, isolated from the crowd. Color-only edits
    // must preserve exact geometry; rooted height/width change both passes;
    // baldness must remove the same hair from color and depth/shadow rendering.
    const auto appearance_cache=texture_cache_stats();
    const GLuint character_program=program(root,"world_foliage.fs.glsl",true),character_shadow=program(root,"world_foliage_shadow.fs.glsl",true);
    for(const auto p:{color_program,shadow_program}){assert(glGetUniformLocation(p,"uCharacterRegion")==-1&&glGetUniformLocation(p,"uCharacterEnabled")==-1);assert(glGetAttribLocation(p,"aCharacterData")==-1&&glGetAttribLocation(p,"aCharacterHair")==-1);}
    assert(glGetUniformLocation(character_program,"uCharacterRegion")>=0&&glGetAttribLocation(character_program,"aCharacterData")>=0);
    copy_uniforms(color_program,character_program);copy_uniforms(shadow_program,character_shadow);
    {
        auto *a=world_model_load_pose((models/"village_character_travelerwoman_idle_00.glb").c_str(),1),*b=world_model_load_pose((models/"village_character_travelerwoman_idle_01.glb").c_str(),1);assert(a&&b);
        auto *batch=world_model_pose_blend_create(1,1);assert(batch&&world_model_pose_blend_set_targets(batch,a,b)&&world_model_pose_blend_enable_character(batch));
        WorldModelInstance instance={0,0,0,0,1};float weight=.5f;auto look=world_model_character_default();
        for(unsigned i=0;i<16;i++){instance.x=float(i);assert(world_model_pose_blend_upload_character(batch,&instance,&weight,&look,1));}assert(world_model_pose_blend_character_uploads(batch)==0);
        look.width=1.18f;assert(world_model_pose_blend_upload_character(batch,&instance,&weight,&look,1));assert(world_model_pose_blend_character_uploads(batch)==1);
        for(unsigned i=0;i<16;i++){instance.x=float(i);assert(world_model_pose_blend_upload_character(batch,&instance,&weight,&look,1));}assert(world_model_pose_blend_character_uploads(batch)==1);
        const auto custom=look;look=world_model_character_default();assert(world_model_pose_blend_upload_character(batch,&instance,&weight,&look,1));look=custom;assert(world_model_pose_blend_upload_character(batch,&instance,&weight,&look,1));assert(world_model_pose_blend_character_uploads(batch)==1);
        world_model_pose_blend_free(batch);world_model_free(a);world_model_free(b);
    }
    controlled.appearance=5;assert(world_village_population_configure(population,&controlled,1));
    for(unsigned i=1;i<world_village_resident_count;i++)assert(world_village_population_runtime_active(population,i,false));
    runtime={origin_x,24,origin_z-65,.3f,0,true};assert(world_village_population_actor_runtime(population,0,&runtime));
    WorldVillageAppearanceInfo look_info;assert(world_village_population_appearance_info(population,0,&look_info));assert(look_info.capabilities&WORLD_MODEL_CHARACTER_MASKED_HAIR);
    const double eye_y=25.2,eye_z=origin_z-60;
    auto render_appearance=[&](bool shadow,bool variant=true){
        world_village_population_update(population,origin_x,eye_y,eye_z,103,0);
        const auto p=shadow?(variant?character_shadow:shadow_program):(variant?character_program:color_program);glUseProgram(p);glUniformMatrix4fv(glGetUniformLocation(p,"uVP"),1,GL_FALSE,runtime_matrix.data());
        glDrawBuffer(shadow?GL_NONE:GL_COLOR_ATTACHMENT0);glReadBuffer(shadow?GL_NONE:GL_COLOR_ATTACHMENT0);glClear(GL_DEPTH_BUFFER_BIT|(shadow?0:GL_COLOR_BUFFER_BIT));
        world_village_population_draw(population,p,origin_x,eye_y,eye_z,shadow);
        if(!shadow)glReadPixels(0,0,width,height,GL_RGBA,GL_FLOAT,pixels.data());
        glReadPixels(0,0,width,height,GL_DEPTH_COMPONENT,GL_FLOAT,depths.data());
        if(variant){GLint region=-1,enabled=-1;glGetUniformiv(p,glGetUniformLocation(p,"uCharacterRegion"),&region);glGetUniformiv(p,glGetUniformLocation(p,"uCharacterEnabled"),&enabled);assert(region==0&&enabled==0);}
    };
    const auto save_appearance=[&](const char *name){if(const char *folder=std::getenv("VALWOOD_QA_APPEARANCE_EVIDENCE")){std::filesystem::create_directories(folder);std::ofstream file(std::filesystem::path(folder)/(std::string(name)+".ppm"),std::ios::binary);file<<"P6\n"<<width<<' '<<height<<"\n255\n";for(int y=height-1;y>=0;y--)for(int x=0;x<width;x++)for(unsigned c=0;c<3;c++){float v=std::clamp(pixels[y*width+x][c],0.f,1.f);const unsigned char byte=static_cast<unsigned char>(std::lround(std::pow(v,1.f/2.2f)*255));file.write(reinterpret_cast<const char *>(&byte),1);}}};
    render_appearance(false,false);const auto original_pixels=pixels;const auto original_depth=depths;
    render_appearance(false,true);assert(pixels==original_pixels&&depths==original_depth);save_appearance("original");
    render_appearance(true,false);const auto generic_shadow=depths;render_appearance(true,true);assert(depths==generic_shadow);
    auto character=world_model_character_default();character.color_strength=1;character.hair_tint[0]=.8f;character.hair_tint[1]=.02f;character.hair_tint[2]=.005f;
    assert(world_village_population_appearance(population,0,1,&character));render_appearance(false);assert(depths==original_depth);
    unsigned changed_pixels=0;for(unsigned i=0;i<pixels.size();i++)if(std::abs(pixels[i][0]-original_pixels[i][0])+std::abs(pixels[i][1]-original_pixels[i][1])+std::abs(pixels[i][2]-original_pixels[i][2])>.0001f)changed_pixels++;assert(changed_pixels>2);save_appearance("hair-color");
    character.width=1.18f;assert(world_village_population_appearance(population,0,1.22f,&character));render_appearance(false);assert(depths!=original_depth);save_appearance("height-build");
    render_appearance(true);const auto hair_shadow=depths;
    character.hide_hair=1;assert(world_village_population_appearance(population,0,1.22f,&character));render_appearance(false);const auto bald_pixels=pixels;save_appearance("bald");render_appearance(true);assert(depths!=hair_shadow);
    unsigned removed=0;for(unsigned i=0;i<depths.size();i++)removed+=depths[i]>hair_shadow[i]+.000001f;assert(removed>2);
    character.hide_hair=0;character.alpha_exponent=1.2f;character.cutoff_offset=.13f;assert(world_village_population_appearance(population,0,1.22f,&character));render_appearance(false);assert(pixels!=bald_pixels);render_appearance(true);assert(depths!=hair_shadow);
    assert(world_village_population_appearance(population,0,1,nullptr));render_appearance(false);assert(depths==original_depth&&pixels==original_pixels);
    const auto appearance_after=texture_cache_stats();assert(appearance_cache.entries==appearance_after.entries&&appearance_cache.references==appearance_after.references&&appearance_cache.misses==appearance_after.misses);
    std::printf("PASS appearance actual GL: %u hair-only color pixels with bit-exact depth; rooted height/build, matching all-pass baldness (%u depth pixels removed), masked density/thickness, exact clear/restore and zero additional textures\n",changed_pixels,removed);
    std::puts("PASS appearance shader variants: generic character inputs/uniforms absent, neutral color/depth and shadow bit-exact, custom/neutral/cleared state restored; neutral uploads zero and unchanged custom uploads bypassed");
    world_village_population_free(population);
    auto released=texture_cache_stats();assert(released.entries==architecture_cache.entries&&released.references==architecture_cache.references&&released.source_bytes==architecture_cache.source_bytes);
    world_scenery_free(scenery);empty();glDeleteProgram(color_program);glDeleteProgram(shadow_program);glDeleteProgram(character_program);glDeleteProgram(character_shadow);glDeleteTextures(2,neutral);glDeleteTextures(1,&color);glDeleteTextures(1,&depth);glDeleteFramebuffers(1,&framebuffer);
    world_atlas_shutdown();assert(glGetError()==GL_NO_ERROR);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    std::printf("PASS village population GL: %zu actual poses/%zu shared effective image keys, partial-load cleanup,140roles/10appearances,%zu fractional pose samples across60Hz frames, animated image delta%.3f, %zu color/%zu shadow pixels with matching depths, reactivealpha.15/reset, mirrored reuse/stableuploads andzero texture owners\n",contracts.poses,contracts.images.size(),fractional_poses,animation_delta,colored_total,shadow_total);
}

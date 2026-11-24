/* Actual purchased torch material/import correctness. Software OpenGL by
 * default; these assertions are not a native-GPU performance measurement.
 * Link with world_models.cpp + texture_cache.c, without main.c, and
 * -Wl,--wrap=glDrawElementsInstanced to inspect material state during draws.
 */
#define GL_GLEXT_PROTOTYPES
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <SDL2/SDL.h>
#include "gl_helpers.h"
#include "engine/world_models.h"
#include "engine/world_texture_blocks_format.h"
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
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

namespace {
constexpr int side=256;
using Pixel=std::array<float,4>;
struct Observation {GLint sampler=-1,has=-1;std::array<float,3> factor={};};
GLuint observed_program=0;
std::vector<Observation> observed;
struct Key {std::vector<unsigned char> bytes;TextureCachePolicy policy;};
struct Contract {std::array<Key,4> keys;std::array<bool,4> compressed={};std::size_t triangles=0;};
struct Fixture {const char *alias;std::array<float,3> socket;float span;};
const Fixture fixtures[]={
    {"hand",{0,2.3f,0},.85f},{"cup",{0,1.83f,0},.85f},
    {"pole",{0,5.94f,0},1.1f},{"wall",{-.00288539f,2,.67683959f},1.25f},
    {"brazier",{-.00478515f,.54112417f,-.00025332f},1.7f}
};
bool same_policy(const TextureCachePolicy &a,const TextureCachePolicy &b){
    return a.srgb==b.srgb&&a.min_filter==b.min_filter&&a.mag_filter==b.mag_filter&&
        a.wrap_s==b.wrap_s&&a.wrap_t==b.wrap_t&&a.mip_policy==b.mip_policy;
}
TextureCachePolicy policy(const cgltf_texture *texture,bool srgb){
    TextureCachePolicy p={srgb,GL_LINEAR_MIPMAP_LINEAR,GL_LINEAR,GL_REPEAT,GL_REPEAT,1};
    if(texture->sampler){
        const auto &s=*texture->sampler;
        if(s.min_filter)p.min_filter=unsigned(s.min_filter);
        if(s.mag_filter)p.mag_filter=unsigned(s.mag_filter);
        if(s.wrap_s)p.wrap_s=unsigned(s.wrap_s);
        if(s.wrap_t)p.wrap_t=unsigned(s.wrap_t);
    }
    if(SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic")||SDL_GL_ExtensionSupported("GL_ARB_texture_filter_anisotropic")){
        float maximum=1;glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,&maximum);
        if(std::isfinite(maximum)&&maximum>=2)p.mip_policy|=unsigned(std::min(4.f,maximum))<<8;
    }
    const char *disabled=SDL_getenv("VALWOOD_QA_DISABLE_OPAQUE_BC7");
    if(srgb&&SDL_GL_ExtensionSupported("GL_ARB_texture_compression_bptc")&&!(disabled&&!std::strcmp(disabled,"1")))p.mip_policy|=1u<<16;
    return p;
}
Contract inspect(const std::filesystem::path &path){
    cgltf_options options={};cgltf_data *data=nullptr;
    assert(cgltf_parse_file(&options,path.c_str(),&data)==cgltf_result_success);
    assert(cgltf_load_buffers(&options,data,path.c_str())==cgltf_result_success);
    assert(cgltf_validate(data)==cgltf_result_success);
    assert(data->file_type==cgltf_file_type_glb&&data->materials_count==1&&data->meshes_count==1);
    assert(data->images_count==4&&!data->skins_count&&!data->animations_count);
    assert(data->meshes[0].primitives_count==1);
    const auto &primitive=data->meshes[0].primitives[0];
    assert(primitive.type==cgltf_primitive_type_triangles&&primitive.indices);
    assert(primitive.indices->count>0&&primitive.indices->count%3==0);
    const auto &m=data->materials[0];
    assert(m.has_pbr_metallic_roughness&&m.alpha_mode==cgltf_alpha_mode_opaque);
    for(float v:m.emissive_factor)assert(v==1);
    const cgltf_texture_view *views[]={&m.pbr_metallic_roughness.base_color_texture,&m.normal_texture,
        &m.pbr_metallic_roughness.metallic_roughness_texture,&m.emissive_texture};
    Contract result;result.triangles=primitive.indices->count/3;
    for(unsigned i=0;i<4;i++){
        const auto &view=*views[i];assert(view.texture&&view.texcoord==0&&!view.has_transform);
        const auto *image=view.texture->image;assert(image&&!image->uri&&image->buffer_view);
        const auto *bytes=cgltf_buffer_view_data(image->buffer_view);assert(bytes);
        int w=0,h=0,c=0;assert(stbi_info_from_memory(bytes,int(image->buffer_view->size),&w,&h,&c));
        assert(w==1024&&h==1024);
        result.keys[i]={std::vector<unsigned char>(bytes,bytes+image->buffer_view->size),policy(view.texture,i==0||i==3)};
        if(result.keys[i].policy.mip_policy&(1u<<16)){
            char leaf[32];std::snprintf(leaf,sizeof(leaf),"%016llx.vbc7",static_cast<unsigned long long>(world_texture_blocks::fingerprint(bytes,image->buffer_view->size)));
            world_texture_blocks::Image cached;std::string error;
            result.compressed[i]=world_texture_blocks::read((path.parent_path().parent_path()/"texture_blocks"/leaf).c_str(),bytes,image->buffer_view->size,cached,error)
                &&cached.width==unsigned(w)&&cached.height==unsigned(h);
        }
    }
    cgltf_free(data);return result;
}
GLuint program(){
    const char *vs=R"GLSL(#version 330 core
layout(location=0)in vec3 aPosition;layout(location=2)in vec2 aUV;
layout(location=4)in vec4 aInstance;layout(location=5)in float aYaw;
layout(location=8)in vec3 aPosePosition;layout(location=11)in float aPoseWeight;
uniform bool uPoseBlendEnabled;
uniform vec3 uCentre;uniform float uScale,uPitch;out vec2 vUV;
void main(){
 float c=cos(aYaw),s=sin(aYaw);
 vec3 p=(uPoseBlendEnabled?mix(aPosition,aPosePosition,aPoseWeight):aPosition)*aInstance.w;
 p=vec3(p.x*c+p.z*s,p.y,-p.x*s+p.z*c)+aInstance.xyz-uCentre;
 float cp=cos(uPitch),sp=sin(uPitch);
 vec3 q=vec3(p.x,cp*p.y-sp*p.z,-sp*p.y-cp*p.z);
 gl_Position=vec4(q.xy*uScale,q.z*.1,1);vUV=aUV;
})GLSL";
    const char *fs=R"GLSL(#version 330 core
in vec2 vUV;out vec4 fragColor;
uniform sampler2D uEmissive;uniform bool uHasEmissive;uniform vec3 uEmissiveFactor;
uniform sampler2D uBase,uNormal,uARM;uniform bool tInspectMaterialSamplers;
void main(){
 // Keep all four material sampler locations active for the format/ownership
 // audit below. Ordinary fixture pixels still contain emission alone.
 if(tInspectMaterialSamplers){fragColor=texture(uBase,vUV)+texture(uNormal,vUV)+texture(uARM,vUV);return;}
 // Deliberately no ambient, sun, basecolor or artificial brightness. Only
 // imported glTF emission can produce nonblack pixels in this fixture.
 vec3 source=uHasEmissive?texture(uEmissive,vUV).rgb:vec3(1);
 fragColor=vec4(source*uEmissiveFactor,1);
})GLSL";
    GLuint p=GLH_program(vs,fs);GLint linked=0;glGetProgramiv(p,GL_LINK_STATUS,&linked);assert(linked);
    for(const char *name:{"uEmissive","uHasEmissive","uEmissiveFactor"})assert(glGetUniformLocation(p,name)>=0);
    return p;
}
void configure(GLuint p,const std::array<float,3> &centre,float span,float pitch=.65f){
    glUseProgram(p);glUniform3fv(glGetUniformLocation(p,"uCentre"),1,centre.data());
    glUniform1f(glGetUniformLocation(p,"uScale"),1.8f/span);
    glUniform1f(glGetUniformLocation(p,"uPitch"),pitch);
}
std::vector<Pixel> pixels(){
    std::vector<Pixel> result(side*side);glReadPixels(0,0,side,side,GL_RGBA,GL_FLOAT,result.data());
    for(const auto &p:result)for(float value:p)assert(std::isfinite(value));
    assert(glGetError()==GL_NO_ERROR);return result;
}
std::vector<Pixel> draw(WorldModelAsset *asset,GLuint p){
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    const auto before=observed.size();world_model_draw(asset,p);assert(observed.size()==before+1);return pixels();
}
double energy(const std::vector<Pixel> &image){
    double value=0;for(const auto &p:image)for(int c=0;c<3;c++)value+=p[c];return value;
}
std::size_t lit(const std::vector<Pixel> &image){
    return std::count_if(image.begin(),image.end(),[](const Pixel &p){return p[0]>1e-5f||p[1]>1e-5f||p[2]>1e-5f;});
}
void uniform_contract(GLuint p,const std::array<float,3> &expected,bool map){
    assert(!observed.empty());const auto &during=observed.back();
    assert(during.sampler==3&&during.has==int(map));
    for(int i=0;i<3;i++)assert(std::fabs(during.factor[i]-expected[i])<1e-6f);
    // Material observations come from the draw call itself. Returning to the
    // caller must reset emission so following procedural dressing stays dark.
    GLint has=-1;float factor[3]={};glGetUniformiv(p,glGetUniformLocation(p,"uHasEmissive"),&has);
    glGetUniformfv(p,glGetUniformLocation(p,"uEmissiveFactor"),factor);
    assert(has==0&&factor[0]==0&&factor[1]==0&&factor[2]==0);
}
GLuint bound(unsigned unit){
    glActiveTexture(GL_TEXTURE0+unit);GLint id=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&id);return GLuint(id);
}
unsigned unexpected_upload(const void *,std::size_t,const TextureCachePolicy *){
    assert(!"Expected source bytes/policy to reuse the actual loaded GPU texture");return 0;
}
void unexpected_delete(unsigned){assert(!"The cache probe cannot own the final texture reference");}
std::vector<unsigned char> read_bytes(const std::filesystem::path &path){
    std::ifstream f(path,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
}
void write_bytes(const std::filesystem::path &path,const std::vector<unsigned char> &bytes){
    std::ofstream f(path,std::ios::binary);assert(f);f.write(reinterpret_cast<const char *>(bytes.data()),std::streamsize(bytes.size()));assert(f);
}
void write_glb(const std::filesystem::path &path,std::string text,std::vector<unsigned char> binary){
    while(text.size()%4)text.push_back(' ');
    while(binary.size()%4)binary.push_back(0);
    const uint32_t head[]={0x46546c67,2,uint32_t(28+text.size()+binary.size()),uint32_t(text.size()),0x4e4f534a};
    const uint32_t chunk[]={uint32_t(binary.size()),0x004e4942};
    std::ofstream f(path,std::ios::binary);assert(f);f.write(reinterpret_cast<const char *>(head),sizeof(head));
    f.write(text.data(),std::streamsize(text.size()));f.write(reinterpret_cast<const char *>(chunk),sizeof(chunk));
    f.write(reinterpret_cast<const char *>(binary.data()),std::streamsize(binary.size()));assert(f);
}
std::filesystem::path synthetic(const std::filesystem::path &dir,const char *name,const Key *map,
    const std::array<float,3> &factor,float strength,float u=.5f,float v=.5f){
    const float positions[]={-.8f,0,0,.8f,0,0,0,1.2f,0};
    const float normals[]={0,0,1,0,0,1,0,0,1},uv[]={u,v,u,v,u,v};const uint16_t indices[]={0,1,2};
    std::vector<unsigned char> binary;
    auto append=[&](const void *p,std::size_t n){while(binary.size()%4)binary.push_back(0);auto offset=binary.size();
        const auto *b=static_cast<const unsigned char *>(p);binary.insert(binary.end(),b,b+n);return offset;};
    const auto p=append(positions,sizeof(positions)),n=append(normals,sizeof(normals)),t=append(uv,sizeof(uv)),i=append(indices,sizeof(indices));
    const auto image=map?append(map->bytes.data(),map->bytes.size()):0;
    std::ostringstream s;s<<"{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0}],\"buffers\":[{\"byteLength\":"<<binary.size()<<"}],\"bufferViews\":[";
    const std::pair<std::size_t,std::size_t> views[]={{p,sizeof(positions)},{n,sizeof(normals)},{t,sizeof(uv)},{i,sizeof(indices)},{image,map?map->bytes.size():0}};
    for(unsigned j=0;j<(map?5u:4u);j++){if(j)s<<',';s<<"{\"buffer\":0,\"byteOffset\":"<<views[j].first<<",\"byteLength\":"<<views[j].second<<'}';}
    s<<"],\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\",\"min\":[-0.8,0,0],\"max\":[0.8,1.2,0]},{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},{\"bufferView\":2,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"},{\"bufferView\":3,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],";
    if(map)s<<"\"images\":[{\"bufferView\":4,\"mimeType\":\"image/png\"}],\"samplers\":[{\"minFilter\":9987,\"magFilter\":9729,\"wrapS\":10497,\"wrapT\":10497}],\"textures\":[{\"source\":0,\"sampler\":0}],";
    s<<"\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0,0,0,1]},\"emissiveFactor\":["<<factor[0]<<','<<factor[1]<<','<<factor[2]<<']';
    if(map)s<<",\"emissiveTexture\":{\"index\":0}";
    if(strength!=1)s<<",\"extensions\":{\"KHR_materials_emissive_strength\":{\"emissiveStrength\":"<<strength<<"}}";
    s<<"}],\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1,\"TEXCOORD_0\":2},\"indices\":3,\"material\":0}]}]";
    if(strength!=1)s<<",\"extensionsUsed\":[\"KHR_materials_emissive_strength\"]";
    s<<'}';auto path=dir/(std::string(name)+".glb");write_glb(path,s.str(),std::move(binary));return path;
}
void partial_failure(const std::filesystem::path &original,const std::filesystem::path &dir){
    // Leave valid base/normal/ORM maps in place, then fail the fourth image.
    // This reaches material acquisition after three owned texture references.
    cgltf_options options={};cgltf_data *data=nullptr;
    assert(cgltf_parse_file(&options,original.c_str(),&data)==cgltf_result_success);
    assert(cgltf_load_buffers(&options,data,original.c_str())==cgltf_result_success);
    const auto *view=data->materials[0].emissive_texture.texture->image->buffer_view;
    const std::size_t offset=view->offset;cgltf_free(data);
    auto bytes=read_bytes(original);uint32_t length=0;std::memcpy(&length,bytes.data()+12,4);
    const std::size_t binary=20+std::size_t(length)+8;assert(binary+offset+16<bytes.size());
    std::fill(bytes.begin()+std::ptrdiff_t(binary+offset),bytes.begin()+std::ptrdiff_t(binary+offset+16),0);
    const auto path=dir/"bad-emissive.png.glb";write_bytes(path,bytes);
    const auto before=texture_cache_stats();assert(!world_model_load(path.c_str()));
    const auto after=texture_cache_stats();
    assert(after.entries==before.entries&&after.references==before.references&&after.source_bytes==before.source_bytes);
    assert(after.hits>=before.hits+3);assert(glGetError()==GL_NO_ERROR);
}
float linear(unsigned char c){const float x=c/255.f;return x<=.04045f?x/12.92f:std::pow((x+.055f)/1.055f,2.4f);}
void synthetic_contracts(const Key &emissive,GLuint p,const std::filesystem::path &dir){
    int w=0,h=0,c=0;auto *rgba=stbi_load_from_memory(emissive.bytes.data(),int(emissive.bytes.size()),&w,&h,&c,4);assert(rgba);
    std::size_t bright=0;unsigned maximum=0;
    for(int i=0;i<w*h;i++){unsigned value=rgba[4*i]+rgba[4*i+1]+rgba[4*i+2];if(value>maximum){maximum=value;bright=std::size_t(i);}}
    assert(maximum>100);const float u=(float(bright%w)+.5f)/w,v=(float(bright/w)+.5f)/h;
    std::array<float,3> source={linear(rgba[4*bright]),linear(rgba[4*bright+1]),linear(rgba[4*bright+2])};stbi_image_free(rgba);
    const auto a_path=synthetic(dir,"strength",&emissive,{.1f,.2f,.3f},2,u,v);
    const auto b_path=synthetic(dir,"same-effective-emission",&emissive,{.2f,.4f,.6f},1,u,v);
    const auto c_path=synthetic(dir,"different-emission",&emissive,{.1f,.2f,.3f},1,u,v);
    const auto no_path=synthetic(dir,"no-emission",nullptr,{0,0,0},1);
    const auto factor_path=synthetic(dir,"factor-only",nullptr,{.15f,.3f,.45f},2);
    auto *a=world_model_load_pose(a_path.c_str(),1),*b=world_model_load_pose(b_path.c_str(),1),*different=world_model_load_pose(c_path.c_str(),1);
    auto *none=world_model_load(no_path.c_str()),*factor=world_model_load(factor_path.c_str());assert(a&&b&&different&&none&&factor);
    const WorldModelInstance instance={0,0,0,0,1};
    for(auto *asset:{a,b,different,none,factor})assert(world_model_upload_instances(asset,&instance,1));
    configure(p,{0,.6f,0},1.6f,0);
    auto image=draw(a,p);uniform_contract(p,{.2f,.4f,.6f},true);assert(lit(image)>1000);
    const auto centre=image[(side/2)*side+side/2];
    for(int j=0;j<3;j++)assert(std::fabs(centre[j]-source[j]*(.2f*(j+1)))<.006f);
    image=draw(none,p);uniform_contract(p,{0,0,0},false);assert(energy(image)==0&&lit(image)==0);
    image=draw(factor,p);uniform_contract(p,{.3f,.6f,.9f},false);
    for(int j=0;j<3;j++)assert(std::fabs(image[(side/2)*side+side/2][j]-(.3f*(j+1)))<.0005f);
    auto *batch=world_model_pose_blend_create(1,1);assert(batch);
    assert(world_model_pose_blend_set_targets(batch,a,b));const float weight=.5f;
    assert(world_model_pose_blend_upload(batch,&instance,&weight,1));
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);world_model_pose_blend_draw(batch,p);const auto baseline=pixels();
    assert(lit(baseline)>1000&&energy(baseline)>0);uniform_contract(p,{.2f,.4f,.6f},true);
    assert(!world_model_pose_blend_set_targets(batch,a,different));
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);world_model_pose_blend_draw(batch,p);assert(pixels()==baseline);
    world_model_pose_blend_free(batch);for(auto *asset:{a,b,different,none,factor})world_model_free(asset);
    std::puts("PASS generic emission: sRGB sample times factor/KHR strength, factor-only emission, exact black/reset for ordinary material; compatible pose emission preserved and mismatched emission rejected transactionally");
}
}

extern "C" void __real_glDrawElementsInstanced(GLenum,GLsizei,GLenum,const void *,GLsizei);
extern "C" void __wrap_glDrawElementsInstanced(GLenum mode,GLsizei count,GLenum type,const void *indices,GLsizei instances){
    GLint current=0;glGetIntegerv(GL_CURRENT_PROGRAM,&current);
    if(GLuint(current)==observed_program){
        Observation o;glGetUniformiv(observed_program,glGetUniformLocation(observed_program,"uEmissive"),&o.sampler);
        glGetUniformiv(observed_program,glGetUniformLocation(observed_program,"uHasEmissive"),&o.has);
        glGetUniformfv(observed_program,glGetUniformLocation(observed_program,"uEmissiveFactor"),o.factor.data());
        observed.push_back(o);
    }
    __real_glDrawElementsInstanced(mode,count,type,indices,instances);
}

int main(int argc,char **argv){
    assert(argc<=2);auto root=std::filesystem::absolute(argc>1?argv[1]:".");
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto *window=SDL_CreateWindow("Torch model material correctness",0,0,side,side,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);
    auto context=SDL_GL_CreateContext(window);assert(context);
    std::printf("Torch material correctness renderer: %s (no FPS claim)\n",glGetString(GL_RENDERER));
    GLuint fbo=0,color=0,depth=0;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,side,side,0,GL_RGBA,GL_FLOAT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
    glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,side,side);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);glReadBuffer(GL_COLOR_ATTACHMENT0);glViewport(0,0,side,side);
    glEnable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_FRAMEBUFFER_SRGB);
    const GLuint p=program();observed_program=p;const auto initial=texture_cache_stats();assert(!initial.entries&&!initial.references);
    std::vector<WorldModelAsset *> assets;Contract first;std::array<GLuint,4> texture_ids={};
    const auto models=root/"projects/valwood_world/models";std::size_t total_lit=0;
    for(const auto &fixture:fixtures)for(const char *lod:{"near","mid"}){
        const auto path=models/(std::string("purchased_torch_")+fixture.alias+'_'+lod+".glb");const auto contract=inspect(path);
        if(assets.empty())first=contract;
        else for(unsigned i=0;i<4;i++)assert(same_policy(contract.keys[i].policy,first.keys[i].policy)&&contract.keys[i].bytes==first.keys[i].bytes);
        assert(contract.triangles>0&&contract.triangles<=(std::strcmp(lod,"near")==0?6500u:1850u));
        auto *asset=world_model_load(path.c_str());assert(asset);assets.push_back(asset);
        const WorldModelInstance instance={0,0,0,0,1};assert(world_model_upload_instances(asset,&instance,1));
        configure(p,fixture.socket,fixture.span);const auto image=draw(asset,p);uniform_contract(p,{1,1,1},true);
        const auto count=lit(image);const auto light=energy(image);
        std::printf("Torch %s %s: %zu tris, %zu fuel pixels, emission-only energy %.6f\n",fixture.alias,lod,contract.triangles,count,light);
        assert(count>1&&light>.001);total_lit+=count;
        for(unsigned i=0;i<4;i++){
            const GLuint id=bound(i);assert(id);if(assets.size()==1)texture_ids[i]=id;else assert(id==texture_ids[i]);
            GLint format=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&format);
            GLint width=0,height=0,compressed=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&width);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&height);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_COMPRESSED,&compressed);
            assert(width==1024&&height==1024&&bool(compressed)==contract.compressed[i]);
            if(i==0||i==3)assert(format==(contract.compressed[i]?GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM:GL_SRGB8_ALPHA8));else assert(format==GL_RGBA8||format==GL_RGBA);
        }
        const auto cache=texture_cache_stats();assert(cache.entries==4+WORLD_MODEL_NEUTRAL_TEXTURE_REFS&&cache.references==assets.size()*(4+WORLD_MODEL_NEUTRAL_TEXTURE_REFS)&&cache.unmanaged_uploads==0);
    }
    assert(assets.size()==10&&total_lit>50);
    for(unsigned i=0;i<4;i++){
        const auto &key=first.keys[i];const auto texture=texture_cache_acquire(key.bytes.data(),key.bytes.size(),&key.policy,unexpected_upload,unexpected_delete);
        assert(texture==texture_ids[i]);assert(texture_cache_release(texture));
    }
    const auto dir=std::filesystem::temp_directory_path()/("valwood-torch-models-"+std::to_string(getpid()));assert(std::filesystem::create_directory(dir));
    partial_failure(models/"purchased_torch_hand_near.glb",dir);
    synthetic_contracts(first.keys[3],p,dir);
    const auto loaded=texture_cache_stats();assert(loaded.entries==4+WORLD_MODEL_NEUTRAL_TEXTURE_REFS&&loaded.references==40+assets.size()*WORLD_MODEL_NEUTRAL_TEXTURE_REFS&&loaded.unmanaged_uploads==0);
    for(auto *asset:assets)world_model_free(asset);
    const auto released=texture_cache_stats();assert(!released.entries&&!released.references&&!released.source_bytes);
    texture_cache_shutdown();std::filesystem::remove_all(dir);
    glDeleteProgram(p);glDeleteRenderbuffers(1,&depth);glDeleteTextures(1,&color);glDeleteFramebuffers(1,&fbo);
    assert(glGetError()==GL_NO_ERROR);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    std::puts("PASS ten actual purchased torch GLBs: four exact shared PBR image keys/40 source references plus three neutral keys/30 asset references, emissive sRGB unit3, visible glowing fuel with no ambient; late bad-emissive load cleans up; zero remaining texture owners");
}

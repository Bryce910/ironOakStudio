#include "engine/world_program_epoch.h"
#define GL_GLEXT_PROTOTYPES
#include "engine/world_models.h"
#include "engine/world_model_pose_storage.h"
#include "engine/world_model_material_batch.h"
#include "engine/world_model_pose_transform.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#include "cgltf.h"
#include "stb_image.h"
#include "gl_helpers.h"
#include "engine/world_model_texture_blocks.h"
#include "engine/world_model_texture_budget.h"
#include "engine/world_character_textures.h"
#include "engine/world_model_facade.h"
#include "engine/world_model_cpu.h"
#include "engine/world_model_pending_textures.h"
#include "engine/world_model_equipment.h"
#include "engine/world_model_variants.h"
#include "engine/world_model_orc_organic.h"
#include "engine/world_model_hair.h"
#include "engine/world_model_skin.h"
#include "engine/world_model_image_bytes.h"
#include "engine/world_model_baked_light.h"
#include "engine/world_model_source_paint.h"
#include "engine/world_model_lava_heat.h"
#include "engine/world_rigid_weapon_pose.h"
extern "C" {
#include "engine/texture_cache.h"
}
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <climits>
#include <memory>
#include <stdexcept>
#include <limits>
#include <vector>
#include <set>
#include <string_view>
#include <sys/stat.h>

namespace {
thread_local void (*load_observer)(void *)=nullptr;
thread_local void *load_observer_user=nullptr;
thread_local unsigned depth_material_program=0;
thread_local WorldModelDepthMaterialStats depth_material_stats={};
constexpr size_t max_meshes=128, max_vertices=1000000, max_indices=3000000;
constexpr size_t max_instances=4096, max_file_bytes=128u*1024u*1024u;
using Vertex=world_model_cpu::Vertex;
// Pose assets do not carry baked building irradiance. Keep their exact float
// PNT/UV/paint prefix, without uploading four permanently unused light values.
constexpr GLsizei pose_vertex_stride=13*sizeof(float);
static_assert(offsetof(Vertex,baked_light)==pose_vertex_stride,"Pose prefix must remain exact");
struct SharedPoseIndices {
    std::uint64_t hash=0;GLuint buffer=0;std::size_t references=0;
    std::vector<std::uint32_t> indices;
};
thread_local std::vector<SharedPoseIndices> shared_pose_indices;
thread_local std::size_t pose_index_hits=0,pose_index_saved=0,pose_vertex_saved=0;
static std::uint64_t pose_index_hash(const std::vector<std::uint32_t>&indices){
    std::uint64_t h=14695981039346656037ull;
    for(auto v:indices){h^=v;h*=1099511628211ull;}return h;
}
static bool compact_pose_geometry(){return std::getenv("VALWOOD_QA_LEGACY_POSE_STORAGE")==nullptr;}
static void release_pose_indices(GLuint buffer){
    for(auto i=shared_pose_indices.begin();i!=shared_pose_indices.end();++i)if(i->buffer==buffer){
        if(!--i->references){glDeleteBuffers(1,&i->buffer);shared_pose_indices.erase(i);}return;
    }
}
using world_model_materials::Facade;
using world_model_materials::facade_parameters;
struct GPUInstance {float x,y,z,scale,yaw,fade,wind_sway,wind_flutter;};
static_assert(sizeof(GPUInstance)==8*sizeof(float),"Instance fade and wind must retain the packed8-float layout");
struct InstanceStream {
    size_t capacity=0;
    bool exact=true;
};
static bool exact_instance_buffers(){
    const char *value=std::getenv("VALWOOD_QA_EXACT_INSTANCE_BUFFERS");
    // Native RX5300 full-route A/B found severe stalls with explicit orphan +
    // subdata. Keep the established dynamic upload as the production default;
    // value0 retains the experimental path for isolated driver comparisons.
    return !value||std::strcmp(value,"0")!=0;
}
static bool stream_instances(GLuint buffer,const GPUInstance *data,size_t count,InstanceStream &stream){
    // Input and CPU staging have already been validated by the caller. Keep
    // the same object referenced by every existing VAO; only its backing store
    // may be replaced. The optional high-water policy is diagnostic only;
    // measured native performance, rather than allocation theory, picks default.
    size_t capacity=count;
    if(!stream.exact){
        capacity=16;
        while(capacity<count)capacity*=2;
        capacity=std::max(capacity,stream.capacity);
    }
    glBindBuffer(GL_ARRAY_BUFFER,buffer);
    if(stream.exact){
        // Production default, latched once at asset/batch creation.
        glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(count*sizeof(GPUInstance)),data,GL_DYNAMIC_DRAW);
    }else{
        glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(capacity*sizeof(GPUInstance)),nullptr,GL_STREAM_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER,0,GLsizeiptr(count*sizeof(GPUInstance)),data);
    }
    glBindBuffer(GL_ARRAY_BUFFER,0);
    if(glGetError()!=GL_NO_ERROR)return false;
    stream.capacity=capacity;
    return true;
}
std::uint64_t rigid_weapon_draws=0;
struct RigidWeaponPart {WorldModelRigidWeaponPose pose={};unsigned first_index=0,index_count=0;};
struct RigidWeaponMesh {std::array<RigidWeaponPart,2> parts={};unsigned count=0;};
struct PoseKey {float uv[2],handedness,paint;};
#include "engine/world_model_pose_pager.inc"
struct Mesh {
    GLuint vao=0,vbo=0,ebo=0,base=0,normal=0,arm=0,emissive=0,facade_aux[3]={0,0,0};
    GLuint detail_samplers[4]={0,0,0,0};
    Facade facade;
    GLsizei indices=0;
    GLsizei vertex_stride=sizeof(Vertex);
    bool shared_indices=false;
    PosePageOwner pose_page={nullptr,pose_page_release};
    float factor[4]={1,1,1,1},roughness=1,metallic=0,cutoff=0,normal_scale=1;
    float emissive_factor[3]={0,0,0};
    float winter[3]={0,0,0};
    float leaf_transmission[4]={0,0,0,-1};
    bool baked_light=false,source_paint=false,support_weapon=false,skin_material=false,organic_body=false,held_weapon=false;
    unsigned character_region=0,equipment_mask=0,variant_mask=0,hair_material=0;
    std::vector<PoseKey> pose_keys;
    std::vector<uint32_t> pose_indices;
    // Loading-only geometry for an explicitly owned, untagged weapon primitive.
    // Released once all preallocated pose pairs have been certified.
    std::vector<world_rigid_weapon_pose::Point> rigid_source;
    std::vector<std::array<float,3>> cut_support_xyz;
    std::vector<std::array<float,6>> cut_basis_nt;
};
struct CookedMesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    const cgltf_material *material=nullptr;
    Facade facade;
    bool baked_light=false,source_paint=false,organic_body=false;
    unsigned hair_material=0;
};
struct Locations {
    GLint base=-1,normal=-1,arm=-1,has_base=-1,has_normal=-1,has_arm=-1;
    GLint emissive=-1,has_emissive=-1,emissive_factor=-1;
    GLint normal_scale=-1;
    GLint winter=-1,leaf_transmission=-1;
    GLint factor=-1,roughness=-1,metallic=-1,cutoff=-1,instance_fade=-1,full_transform=-1;
    GLint facade_mode=-1,facade_scale=-1,facade_aux[3]={-1,-1,-1};
    GLint pose_blend=-1,vertex_override=-1;
    GLint rigid_weapon=-1,rigid_rotation=-1,rigid_pivot_a=-1,rigid_pivot_b=-1;
    GLint baked_light=-1,source_paint=-1;
    GLint anatomy=-1,anatomy_mirror=-1,anatomy_caps=-1,anatomy_mask=-1,anatomy_keep=-1,anatomy_instance=-1,anatomy_yaw_weight=-1,anatomy_pivot=-1,anatomy_rotation=-1,anatomy_cap_side=-1;
    GLint character_enabled=-1,character_region=-1,item_wind=-1,skin_material=-1,organic_body=-1,hair_material=-1;
};
struct ProgramLocations {unsigned long long epoch=0;GLuint program=0;Locations uniforms;};
static bool finite3(const float *v){return std::isfinite(v[0])&&std::isfinite(v[1])&&std::isfinite(v[2]);}
static void normalize(float *v){
    float len=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    if(len>1e-12f){for(int i=0;i<3;i++)v[i]/=len;}
    else {v[0]=0;v[1]=1;v[2]=0;}
}
static const cgltf_accessor *attribute(const cgltf_primitive &p,cgltf_attribute_type type,int index=0){
    for(size_t i=0;i<p.attributes_count;i++)
        if(p.attributes[i].type==type&&p.attributes[i].index==index)return p.attributes[i].data;
    return nullptr;
}
static bool in_scene(const cgltf_data *data,const cgltf_node *node){
    if(!data->scene&&data->scenes_count==0)return true;
    const cgltf_scene *scene=data->scene?data->scene:&data->scenes[0];
    for(size_t depth=0;node&&depth<=data->nodes_count;depth++,node=node->parent)
        for(size_t i=0;i<scene->nodes_count;i++)if(scene->nodes[i]==node)return true;
    return false;
}
static void transform_direction(const float *m,const float *v,float *out){
    for(int i=0;i<3;i++)out[i]=m[i]*v[0]+m[4+i]*v[1]+m[8+i]*v[2];
}
static bool normal_matrix(const float *m,float *n,float &sign){
    const float a=m[0],b=m[4],c=m[8],d=m[1],e=m[5],f=m[9],g=m[2],h=m[6],i=m[10];
    const float determinant=a*(e*i-f*h)+b*(f*g-d*i)+c*(d*h-e*g);
    if(!std::isfinite(determinant)||std::fabs(determinant)<1e-12f)return false;
    const float cofactor[9]={e*i-f*h,f*g-d*i,d*h-e*g,c*h-b*i,a*i-c*g,b*g-a*h,b*f-c*e,c*d-a*f,a*e-b*d};
    for(int k=0;k<9;k++)n[k]=cofactor[k]/determinant;
    sign=determinant<0?-1.0f:1.0f;return true;
}
static void transform_normal(const float *m,const float *v,float *out){
    for(int i=0;i<3;i++)out[i]=m[i*3]*v[0]+m[i*3+1]*v[1]+m[i*3+2]*v[2];
    normalize(out);
}
static void complete_basis(CookedMesh &mesh,bool need_normals,bool need_tangents,
                           bool stable_pose_basis=false,float mirror_sign=1){
    std::vector<std::array<float,3>> bitangents;
    if(need_tangents)bitangents.resize(mesh.vertices.size());
    std::vector<float> dominant_uv,uv_handedness;
    std::vector<unsigned char> uv_signs;
    if(need_tangents&&stable_pose_basis){
        dominant_uv.resize(mesh.vertices.size());uv_handedness.resize(mesh.vertices.size(),mirror_sign);
        uv_signs.resize(mesh.vertices.size());
    }
    for(size_t i=0;i<mesh.indices.size();i+=3){
        Vertex &a=mesh.vertices[mesh.indices[i]],&b=mesh.vertices[mesh.indices[i+1]],&c=mesh.vertices[mesh.indices[i+2]];
        float e1[3],e2[3];for(int k=0;k<3;k++){e1[k]=b.p[k]-a.p[k];e2[k]=c.p[k]-a.p[k];}
        if(need_normals){
            const float n[3]={e1[1]*e2[2]-e1[2]*e2[1],e1[2]*e2[0]-e1[0]*e2[2],e1[0]*e2[1]-e1[1]*e2[0]};
            for(Vertex *v:{&a,&b,&c})for(int k=0;k<3;k++)v->n[k]+=n[k];
        }
        if(need_tangents){
            float du1=b.uv[0]-a.uv[0],dv1=b.uv[1]-a.uv[1],du2=c.uv[0]-a.uv[0],dv2=c.uv[1]-a.uv[1];
            float determinant=du1*dv2-dv1*du2;
            if(std::fabs(determinant)>1e-12f){
                for(int corner=0;corner<3;corner++){
                    const uint32_t index=mesh.indices[i+(size_t)corner];
                    if(stable_pose_basis){
                        uv_signs[index]|=determinant<0?2:1;
                        // Final oriented indices already contain node mirror
                        // correction. Fixed UV area, not a deformed folded
                        // face's agreement with a smooth normal, owns parity.
                        if(std::fabs(determinant)>dominant_uv[index]){
                            dominant_uv[index]=std::fabs(determinant);uv_handedness[index]=determinant<0?-1.f:1.f;
                        }
                    }
                    for(int k=0;k<3;k++){
                        mesh.vertices[index].tangent[k]+=(e1[k]*dv2-e2[k]*dv1)/determinant;
                        bitangents[index][k]+=(e2[k]*du1-e1[k]*du2)/determinant;
                    }
                }
            }
        }
    }
    size_t repaired=0,mixed=0,degenerate=0;
    for(size_t index=0;index<mesh.vertices.size();index++){
        Vertex &v=mesh.vertices[index];
        normalize(v.n);
        float dot=v.n[0]*v.tangent[0]+v.n[1]*v.tangent[1]+v.n[2]*v.tangent[2];
        for(int k=0;k<3;k++)v.tangent[k]-=v.n[k]*dot;
        if(v.tangent[0]*v.tangent[0]+v.tangent[1]*v.tangent[1]+v.tangent[2]*v.tangent[2]<1e-12f){
            if(std::fabs(v.n[1])<.95f){v.tangent[0]=v.n[2];v.tangent[1]=0;v.tangent[2]=-v.n[0];}
            else {v.tangent[0]=1;v.tangent[1]=v.tangent[2]=0;}
        }
        normalize(v.tangent);
        if(need_tangents){
            const float cross[3]={v.n[1]*v.tangent[2]-v.n[2]*v.tangent[1],
                v.n[2]*v.tangent[0]-v.n[0]*v.tangent[2],v.n[0]*v.tangent[1]-v.n[1]*v.tangent[0]};
            float handedness=0;for(int k=0;k<3;k++)handedness+=cross[k]*bitangents[index][k];
            v.tangent[3]=handedness<0?-1.0f:1.0f;
            if(stable_pose_basis){
                repaired+=v.tangent[3]!=uv_handedness[index];mixed+=uv_signs[index]==3;degenerate+=uv_signs[index]==0;
                v.tangent[3]=uv_handedness[index];
            }
        }
    }
    if(need_tangents&&stable_pose_basis)
        std::printf("[PoseBasis] %zu vertices: fixed-UV parity changes%zu, mixed-UV%zu, degenerate-UV%zu\n",mesh.vertices.size(),repaired,mixed,degenerate);
}
static unsigned upload_texture(const void *bytes,size_t length,const TextureCachePolicy *policy){
    const auto max_edge=world_model_texture_budget::edge(policy->mip_policy);
    GLuint texture=(policy->mip_policy&world_model_texture_blocks::policy_bit)?
        world_model_texture_blocks::upload(bytes,length,world_model_texture_blocks::policy_format(policy->mip_policy),max_edge):0;
    if(!texture&&max_edge){
        int width=0,height=0,channels=0;auto *pixels=stbi_load_from_memory(static_cast<const stbi_uc*>(bytes),int(length),&width,&height,&channels,4);
        if(!pixels)return 0;auto *prepared=glh_prepare_rgba_for_mips(pixels,width,height,4);std::vector<unsigned char> reduced;unsigned w=0,h=0;
        if(prepared){stbi_image_free(pixels);pixels=nullptr;}
        bool okay=false;try{okay=world_model_texture_budget::reduce(prepared?prepared:pixels,unsigned(width),unsigned(height),policy->srgb,max_edge,reduced,w,h);}catch(...){std::free(prepared);stbi_image_free(pixels);return 0;}
        std::free(prepared);stbi_image_free(pixels);if(!okay)return 0;
        world_model_texture_blocks::UnpackScope unpack;
        texture=policy->srgb?GLH_texture_pixels_srgb(reduced.data(),int(w),int(h),4):GLH_texture_pixels(reduced.data(),int(w),int(h),4);
    }
    if(!texture&&!max_edge)texture=policy->srgb?GLH_texture_mem_srgb(bytes,(int)length):GLH_texture_mem(bytes,(int)length);
    if(!texture)return 0;
    glBindTexture(GL_TEXTURE_2D,texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,(GLint)policy->min_filter);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,(GLint)policy->mag_filter);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,(GLint)policy->wrap_s);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,(GLint)policy->wrap_t);
    const unsigned anisotropy=(policy->mip_policy>>8)&0xffu;
    if(anisotropy>1)glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_ANISOTROPY_EXT,float(anisotropy));
    if(glGetError()!=GL_NO_ERROR){glDeleteTextures(1,&texture);return 0;}
    return texture;
}
static void delete_texture(unsigned texture){GLuint id=texture;glDeleteTextures(1,&id);}
// Cache keys describe tiny raw neutral texels, distinct from encoded images.
// Every asset owns its references; no context-dependent GL name is static.
static unsigned upload_neutral(const void *encoded,size_t length,const TextureCachePolicy *policy){
    const auto *bytes=static_cast<const unsigned char *>(encoded);
    if(length!=8||std::memcmp(bytes,"VWN\1",4))return 0;
    GLint unpack[7]={};
    const GLenum names[]={GL_PIXEL_UNPACK_BUFFER_BINDING,GL_UNPACK_ALIGNMENT,GL_UNPACK_ROW_LENGTH,
        GL_UNPACK_IMAGE_HEIGHT,GL_UNPACK_SKIP_PIXELS,GL_UNPACK_SKIP_ROWS,GL_UNPACK_SKIP_IMAGES};
    for(unsigned i=0;i<7;i++)glGetIntegerv(names[i],&unpack[i]);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    for(unsigned i=2;i<7;i++)glPixelStorei(names[i],0);
    GLuint texture=0;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    glTexImage2D(GL_TEXTURE_2D,0,policy->srgb?GL_SRGB8_ALPHA8:GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,bytes+4);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_BASE_LEVEL,0);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,0);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GLint(policy->min_filter));
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GLint(policy->mag_filter));
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GLint(policy->wrap_s));glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GLint(policy->wrap_t));
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,GLuint(unpack[0]));
    for(unsigned i=1;i<7;i++)glPixelStorei(names[i],unpack[i]);
    if(glGetError()!=GL_NO_ERROR){glDeleteTextures(1,&texture);return 0;}
    return texture;
}
static GLuint neutral_texture(unsigned role){
    static const unsigned char white[8]={'V','W','N',1,255,255,255,255};
    static const unsigned char normal[8]={'V','W','N',1,128,128,255,255};
    const TextureCachePolicy policy={role==0,GL_LINEAR_MIPMAP_LINEAR,GL_LINEAR,GL_REPEAT,GL_REPEAT,1};
    const auto result=texture_cache_acquire(role==2?normal:white,8,&policy,upload_neutral,delete_texture);if(result)texture_cache_track_gpu_bytes(result,4);return result;
}
// Sampling quality is separate from image ownership. Layered building
// materials can use8x filtering without duplicating full-resolution textures
// already shared with4x foliage/props. These tiny sampler objects live with
// the material and never allocate or query state during walking.
static GLuint facade_sampler(const cgltf_texture_view &view){
    if(!view.texture||!(SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic")||SDL_GL_ExtensionSupported("GL_ARB_texture_filter_anisotropic")))return 0;
    float maximum=1;glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,&maximum);if(!std::isfinite(maximum)||maximum<=4)return 0;
    GLuint id=0;glGenSamplers(1,&id);if(!id)return 0;
    const auto *s=view.texture->sampler;
    glSamplerParameteri(id,GL_TEXTURE_MIN_FILTER,s&&s->min_filter?GLint(s->min_filter):GL_LINEAR_MIPMAP_LINEAR);
    glSamplerParameteri(id,GL_TEXTURE_MAG_FILTER,s&&s->mag_filter?GLint(s->mag_filter):GL_LINEAR);
    glSamplerParameteri(id,GL_TEXTURE_WRAP_S,s&&s->wrap_s?GLint(s->wrap_s):GL_REPEAT);
    glSamplerParameteri(id,GL_TEXTURE_WRAP_T,s&&s->wrap_t?GLint(s->wrap_t):GL_REPEAT);
    glSamplerParameterf(id,GL_TEXTURE_MAX_ANISOTROPY_EXT,std::min(8.f,maximum));return id;
}
#include "engine/world_model_material_batch.inc"
static GLuint material_texture(const char *model_path,const cgltf_texture_view &view,bool srgb,bool opaque_material=false,unsigned data_format=0,bool *verified_hand=nullptr,unsigned max_texture_edge=0){
    if(!view.texture)return 0;
    const cgltf_texture *texture=view.texture;
    const cgltf_image *image=texture->image;
    if(view.texcoord!=0||view.has_transform)return 0;
    world_model_image_bytes::Image source;std::string source_error;
    if(!world_model_image_bytes::read(image,model_path,world_model_image_bytes::maximum_bytes,source,source_error))throw std::runtime_error(source_error);
    const unsigned character_quality=world_character_textures::loading_quality<world_character_textures::profile_count?world_character_textures::loading_quality:world_character_textures::quality;
    const auto *source_sampler=texture->sampler;
    const std::array<unsigned,11> batch_key={unsigned(srgb),unsigned(opaque_material),data_format,unsigned(verified_hand!=nullptr),max_texture_edge,character_quality,
        source_sampler?unsigned(source_sampler->min_filter):0,source_sampler?unsigned(source_sampler->mag_filter):0,
        source_sampler?unsigned(source_sampler->wrap_s):0,source_sampler?unsigned(source_sampler->wrap_t):0,world_character_textures::loading_quality};
    if(const auto cached=material_batch_find(batch_key,source.data(),source.length,verified_hand))return cached;
    const std::uint64_t hand_source_hash=verified_hand?world_character_textures::fingerprint(source.data(),source.length):0;
    std::vector<unsigned char> character_image;
    world_character_textures::LoadingQuality character_role(world_character_textures::model_profile(character_quality,srgb));
    if(!world_character_textures::replace(source.data(),source.length,character_image,source_error))throw std::runtime_error(source_error);
    const size_t length=character_image.empty()?source.length:character_image.size();const void *bytes=character_image.empty()?source.data():character_image.data();
    // Loading-only, explicit hand-material contract. The source atlas was
    // visually checked (SHA256 6b701002b7fc81c44e49608fa7e4d2838af20200075f505c89c31e08cfd97cc6).
    // Length plus its existing encoded-byte fingerprint also rejects a
    // designer replacing the named hand material with another image.
    if(verified_hand)*verified_hand=world_character_textures::hand_response(character_quality,hand_source_hash,source.length,world_character_textures::fingerprint(bytes,length),length);
    int width=0,height=0,channels=0,max_size=0;
    if(!stbi_info_from_memory((const stbi_uc*)bytes,(int)length,&width,&height,&channels))return 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE,&max_size);
    if(width<=0||height<=0||width>max_size||height>max_size||(uint64_t)width*(uint64_t)height>64u*1024u*1024u)return 0;
    const cgltf_sampler *sampler=texture->sampler;
    TextureCachePolicy policy={srgb,GL_LINEAR_MIPMAP_LINEAR,GL_LINEAR,GL_REPEAT,GL_REPEAT,1};
    if(sampler){
        if(sampler->min_filter)policy.min_filter=(unsigned)sampler->min_filter;
        if(sampler->mag_filter)policy.mag_filter=(unsigned)sampler->mag_filter;
        if(sampler->wrap_s)policy.wrap_s=(unsigned)sampler->wrap_s;
        if(sampler->wrap_t)policy.wrap_t=(unsigned)sampler->wrap_t;
    }
    if(SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic")||
       SDL_GL_ExtensionSupported("GL_ARB_texture_filter_anisotropic")) {
        float maximum=1;glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,&maximum);
        if(std::isfinite(maximum)&&maximum>=2) {
            const unsigned anisotropy=unsigned(std::min(4.f,maximum));
            // Include effective sampler quality in the shared cache key. An
            // older isotropic loader must not reuse this ID, or vice versa.
            // Low eight bits retain the existing generated-mip policy.
            policy.mip_policy|=anisotropy<<8;
        }
    }
    if(world_model_texture_blocks::available(srgb,opaque_material,data_format))policy.mip_policy|=world_model_texture_blocks::policy_flags(srgb?world_texture_blocks::format_srgb_bc7:data_format);
    if(max_texture_edge&&(unsigned(width)>max_texture_edge||unsigned(height)>max_texture_edge))policy.mip_policy|=world_model_texture_budget::flags(max_texture_edge);
    const auto before=world_model_texture_blocks::counters().block_bytes;
    const unsigned result=texture_cache_acquire(bytes,length,&policy,upload_texture,delete_texture);
    unsigned effective_w=unsigned(width),effective_h=unsigned(height);world_model_texture_budget::dimensions(effective_w,effective_h,world_model_texture_budget::edge(policy.mip_policy));
    std::size_t gpu_bytes=0;for(int w=int(effective_w),h=int(effective_h);;w=std::max(1,w/2),h=std::max(1,h/2)){gpu_bytes+=std::size_t(w)*h*4;if(w==1&&h==1)break;}
    const auto compressed=world_model_texture_blocks::counters().block_bytes-before;
    if(result)texture_cache_track_gpu_bytes(result,compressed?std::size_t(compressed):gpu_bytes);
    material_batch_remember(batch_key,source.data(),source.length,result,verified_hand&&*verified_hand);
    return result;
}
static void release_texture(GLuint &texture){
    material_batch_clear();
    if(texture&&!texture_cache_release(texture)){texture_cache_forget_gpu_bytes(texture);glDeleteTextures(1,&texture);}
    texture=0;
}
static Locations locations(GLuint program){
    Locations l;
    l.base=glGetUniformLocation(program,"uBase");l.normal=glGetUniformLocation(program,"uNormal");l.arm=glGetUniformLocation(program,"uARM");
    l.has_base=glGetUniformLocation(program,"uHasBase");l.has_normal=glGetUniformLocation(program,"uHasNormal");l.has_arm=glGetUniformLocation(program,"uHasARM");
    l.emissive=glGetUniformLocation(program,"uEmissive");l.has_emissive=glGetUniformLocation(program,"uHasEmissive");l.emissive_factor=glGetUniformLocation(program,"uEmissiveFactor");
    l.normal_scale=glGetUniformLocation(program,"uNormalScale");
    l.winter=glGetUniformLocation(program,"uWinterSurface");
    l.leaf_transmission=glGetUniformLocation(program,"uLeafTransmission");
    l.factor=glGetUniformLocation(program,"uBaseFactor");l.roughness=glGetUniformLocation(program,"uRoughness");l.metallic=glGetUniformLocation(program,"uMetallic");l.cutoff=glGetUniformLocation(program,"uAlphaCutoff");
    l.instance_fade=glGetUniformLocation(program,"uInstanceFade");l.full_transform=glGetUniformLocation(program,"uFullTransform");
    l.facade_mode=glGetUniformLocation(program,"uFacadeMode");l.facade_scale=glGetUniformLocation(program,"uFacadeUVScale");
    l.facade_aux[0]=glGetUniformLocation(program,"uFacadeAux0");l.facade_aux[1]=glGetUniformLocation(program,"uFacadeAux1");l.facade_aux[2]=glGetUniformLocation(program,"uFacadeAux2");
    l.pose_blend=glGetUniformLocation(program,"uPoseBlendEnabled");
    l.vertex_override=glGetUniformLocation(program,"uVertexOverrideEnabled");
    l.rigid_weapon=glGetUniformLocation(program,"uRigidWeaponPose");l.rigid_rotation=glGetUniformLocation(program,"uRigidWeaponRotation");
    l.rigid_pivot_a=glGetUniformLocation(program,"uRigidWeaponPivotA");l.rigid_pivot_b=glGetUniformLocation(program,"uRigidWeaponPivotB");
    l.anatomy=glGetUniformLocation(program,"uAnatomyEnabled");l.anatomy_mirror=glGetUniformLocation(program,"uAnatomyMirror");l.anatomy_caps=glGetUniformLocation(program,"uAnatomyCaps");
    l.anatomy_mask=glGetUniformLocation(program,"uAnatomyMask");l.anatomy_keep=glGetUniformLocation(program,"uAnatomyKeep");l.anatomy_instance=glGetUniformLocation(program,"uAnatomyInstance");
    l.anatomy_yaw_weight=glGetUniformLocation(program,"uAnatomyYawWeight");l.anatomy_pivot=glGetUniformLocation(program,"uAnatomyPivot");l.anatomy_rotation=glGetUniformLocation(program,"uAnatomyRotation");l.anatomy_cap_side=glGetUniformLocation(program,"uAnatomyCapSide");
    l.baked_light=glGetUniformLocation(program,"uBakedLightEnabled");
    l.source_paint=glGetUniformLocation(program,"uSourcePaintEnabled");
    l.item_wind=glGetUniformLocation(program,"uItemWindEnabled");
    l.character_enabled=glGetUniformLocation(program,"uCharacterEnabled");l.character_region=glGetUniformLocation(program,"uCharacterRegion");
    l.skin_material=glGetUniformLocation(program,"uSkinMaterial");
    l.organic_body=glGetUniformLocation(program,"uOrganicBody");
    l.hair_material=glGetUniformLocation(program,"uHairMaterial");
    return l;
}
}

struct WorldModelAsset {
    std::size_t references=1;
    std::vector<Mesh> meshes;
    std::vector<GPUInstance> staging;
    GLuint instance_buffer=0,transform_buffer=0;
    std::vector<std::array<float,5>> transforms;bool full_transform=false;
    std::array<GLuint,WORLD_MODEL_NEUTRAL_TEXTURE_REFS> neutral={}; // color white, linear ARM white, linear flat normal
    InstanceStream stream;
    GLsizei count=0;
    bool has_fades=false;
    bool pose_asset=false;
    bool paged_pose=false;
    bool has_support_grip=false;float support_grip[3]={0,0,0};
    float minimum[3]={0,0,0},maximum[3]={0,0,0};
    /* Color and shadow programs alternate each frame. Keep their locations
       together so drawing a second pass does not repeat driver queries. The
       caller keeps linked programs unchanged for the asset's lifetime. */
    std::array<ProgramLocations,4> programs;
    size_t next_program=0;
    bool staged_materials=false;
    std::size_t geometry_bytes=0;
    std::vector<GLuint> texture_owners;
};

struct WorldModelAnatomyStream {std::vector<GLuint> buffers;std::vector<unsigned> counts;std::vector<std::uint64_t> keys;std::vector<std::array<float,8>> weights;std::vector<std::size_t> offsets;};
struct WorldModelPoseBlend {
    WorldModelAsset *a=nullptr,*b=nullptr;
    WorldModelVertexStream *vertex_stream=nullptr;
    WorldModelAnatomyStream *anatomy=nullptr;
    std::vector<RigidWeaponMesh> rigid_weapons;
    std::vector<GLuint> vaos,subset_buffers;
    std::vector<GLsizei> subset_counts;
    std::vector<GPUInstance> staging;
    GLuint instances=0,transform_buffer=0;
    std::vector<std::array<float,5>> transforms;
    bool full_transform=false,pose_streams_attached=false;
    InstanceStream stream;
    size_t capacity=0;
    GLsizei count=0;
    GLuint character_buffer=0;bool character_active=false;
    std::vector<WorldModelCharacterLook> character_snapshot;
    std::uint64_t character_uploads=0;
};

struct WorldModelInstanceBatch {
    WorldModelAsset *asset=nullptr;
    std::vector<GLuint> vaos;
    std::vector<GPUInstance> staging;
    std::vector<std::array<float,5>> transforms;
    GLuint instances=0,transform_buffer=0;
    InstanceStream stream;
    size_t capacity=0;
    GLsizei count=0;
    bool full_transform=false;
};

// Pin the entire A/B pair before making room, so a large endpoint cannot
// evict another endpoint needed by this draw. No allocation or GL name changes.
static bool prepare_pose_geometry(WorldModelAsset *a,WorldModelAsset *b=nullptr){
    if((!a||!a->paged_pose)&&(!b||!b->paged_pose))return true;
    GLint old_buffer=0;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&old_buffer);
    auto pins=[](WorldModelAsset *asset,int delta){if(asset)for(auto &m:asset->meshes)if(m.pose_page){if(delta>0)++m.pose_page->pins;else --m.pose_page->pins;}};
    pins(a,1);pins(b,1);
    bool okay=true;
    for(auto *asset:{a,b})if(asset)for(auto &m:asset->meshes)if(m.pose_page)okay=pose_page_ready(*m.pose_page)&&okay;
    pins(a,-1);pins(b,-1);glBindBuffer(GL_ARRAY_BUFFER,GLuint(old_buffer));return okay;
}

#include "engine/world_model_vertex_stream.inc"

extern "C" void world_model_free(WorldModelAsset *asset){
    if(!asset)return;
    if(--asset->references)return;
    for(Mesh &mesh:asset->meshes){
        mesh.pose_page.reset();
        glDeleteSamplers(4,mesh.detail_samplers);
        glDeleteVertexArrays(1,&mesh.vao);glDeleteBuffers(1,&mesh.vbo);
        if(mesh.shared_indices)release_pose_indices(mesh.ebo);else glDeleteBuffers(1,&mesh.ebo);
        if(!asset->staged_materials){release_texture(mesh.base);release_texture(mesh.normal);release_texture(mesh.arm);
            release_texture(mesh.emissive);for(GLuint &texture:mesh.facade_aux)release_texture(texture);}
    }
    for(GLuint &texture:asset->texture_owners)release_texture(texture);
    for(GLuint &texture:asset->neutral)release_texture(texture);
    glDeleteBuffers(1,&asset->transform_buffer);glDeleteBuffers(1,&asset->instance_buffer);delete asset;
}

extern "C" void world_model_instance_batch_free(WorldModelInstanceBatch *batch){
    if(!batch)return;
    if(!batch->vaos.empty())glDeleteVertexArrays((GLsizei)batch->vaos.size(),batch->vaos.data());
    glDeleteBuffers(1,&batch->instances);glDeleteBuffers(1,&batch->transform_buffer);
    world_model_free(batch->asset);delete batch;
}

extern "C" WorldModelInstanceBatch *world_model_instance_batch_create(WorldModelAsset *asset,size_t capacity){
    if(!asset||!capacity||capacity>max_instances)return nullptr;
    WorldModelInstanceBatch *batch=nullptr;GLint previous_vao=0,previous_buffer=0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&previous_vao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&previous_buffer);
    try{
        batch=new WorldModelInstanceBatch;batch->asset=asset;++asset->references;
        batch->capacity=capacity;batch->stream.exact=exact_instance_buffers();
        batch->staging.reserve(capacity);batch->transforms.reserve(capacity);batch->vaos.resize(asset->meshes.size());
        glGenBuffers(1,&batch->instances);glGenBuffers(1,&batch->transform_buffer);
        if(!batch->instances||!batch->transform_buffer)throw "Static batch buffer creation failed";
        glBindBuffer(GL_ARRAY_BUFFER,batch->transform_buffer);glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(capacity*5*sizeof(float)),nullptr,GL_DYNAMIC_DRAW);
        glGenVertexArrays((GLsizei)batch->vaos.size(),batch->vaos.data());
        for(size_t k=0;k<batch->vaos.size();++k){
            if(!batch->vaos[k])throw "Static batch VAO creation failed";
            glBindVertexArray(batch->vaos[k]);glBindBuffer(GL_ARRAY_BUFFER,asset->meshes[k].vbo);
            const GLH_Attrib source[]={{0,3,GL_FLOAT,0,offsetof(Vertex,p)},{1,3,GL_FLOAT,0,offsetof(Vertex,n)},
                {2,2,GL_FLOAT,0,offsetof(Vertex,uv)},{3,4,GL_FLOAT,0,offsetof(Vertex,tangent)},{7,1,GL_FLOAT,0,offsetof(Vertex,facade_paint)}};
            for(const auto &v:source){glEnableVertexAttribArray(v.loc);glVertexAttribPointer(v.loc,v.n,v.type,v.norm,asset->meshes[k].vertex_stride,(void*)v.offset);}
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,asset->meshes[k].ebo);glBindBuffer(GL_ARRAY_BUFFER,batch->instances);
            glEnableVertexAttribArray(4);glVertexAttribPointer(4,4,GL_FLOAT,GL_FALSE,sizeof(GPUInstance),(void*)offsetof(GPUInstance,x));glVertexAttribDivisor(4,1);
            glEnableVertexAttribArray(5);glVertexAttribPointer(5,1,GL_FLOAT,GL_FALSE,sizeof(GPUInstance),(void*)offsetof(GPUInstance,yaw));glVertexAttribDivisor(5,1);
            glEnableVertexAttribArray(6);glVertexAttribPointer(6,3,GL_FLOAT,GL_FALSE,sizeof(GPUInstance),(void*)offsetof(GPUInstance,fade));glVertexAttribDivisor(6,1);
            glBindBuffer(GL_ARRAY_BUFFER,batch->transform_buffer);
            glEnableVertexAttribArray(14);glVertexAttribPointer(14,2,GL_FLOAT,GL_FALSE,5*sizeof(float),nullptr);glVertexAttribDivisor(14,1);
            glEnableVertexAttribArray(15);glVertexAttribPointer(15,3,GL_FLOAT,GL_FALSE,5*sizeof(float),reinterpret_cast<void *>(2*sizeof(float)));glVertexAttribDivisor(15,1);
        }
        glBindVertexArray(GLuint(previous_vao));glBindBuffer(GL_ARRAY_BUFFER,GLuint(previous_buffer));
        if(glGetError()!=GL_NO_ERROR)throw "Static batch GL creation failed";
        return batch;
    }catch(...){glBindVertexArray(GLuint(previous_vao));glBindBuffer(GL_ARRAY_BUFFER,GLuint(previous_buffer));world_model_instance_batch_free(batch);return nullptr;}
}

extern "C" bool world_model_instance_batch_upload(WorldModelInstanceBatch *batch,const WorldModelInstance *instances,size_t count){
    if(!batch||count>batch->capacity||(count&&!instances))return false;
    bool full=false;
    for(size_t i=0;i<count;++i){const auto &p=instances[i];
        if(!finite3(&p.x)||!std::isfinite(p.yaw)||!std::isfinite(p.scale)||p.scale<=0||!std::isfinite(p.pitch)||!std::isfinite(p.roll)||
           !std::isfinite(p.axis_x)||!std::isfinite(p.axis_y)||!std::isfinite(p.axis_z)||p.axis_x<=-1||p.axis_y<=-1||p.axis_z<=-1||!std::isfinite(p.wind_sway)||p.wind_sway<0||p.wind_sway>2||!std::isfinite(p.wind_flutter)||p.wind_flutter<0||p.wind_flutter>2)return false;
        full|=p.pitch!=0||p.roll!=0||p.axis_x!=0||p.axis_y!=0||p.axis_z!=0;
    }
    if(!count){batch->count=0;batch->full_transform=false;return true;}
    batch->staging.clear();batch->transforms.clear();
    for(size_t i=0;i<count;++i){const auto &p=instances[i];batch->staging.push_back({p.x,p.y,p.z,p.scale,p.yaw,1.f,p.wind_sway,p.wind_flutter});if(full)batch->transforms.push_back({p.pitch,p.roll,1+p.axis_x,1+p.axis_y,1+p.axis_z});}
    if(full){GLint previous=0;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&previous);glBindBuffer(GL_ARRAY_BUFFER,batch->transform_buffer);glBufferSubData(GL_ARRAY_BUFFER,0,GLsizeiptr(count*5*sizeof(float)),batch->transforms.data());glBindBuffer(GL_ARRAY_BUFFER,GLuint(previous));if(glGetError()!=GL_NO_ERROR)return false;}
    if(!stream_instances(batch->instances,batch->staging.data(),count,batch->stream))return false;
    batch->count=GLsizei(count);batch->full_transform=full;return true;
}

static WorldModelAsset *load_model(const char *path,bool pose_asset,bool preserve_origin=false,bool retain_support=false,bool retain_pnt=false,unsigned max_texture_edge=0){
    struct stat info;
    if(!path||stat(path,&info)||info.st_size<20||(uint64_t)info.st_size>max_file_bytes)return nullptr;
    cgltf_options options={};cgltf_data *data=nullptr;WorldModelAsset *asset=nullptr;
    try {
        world_model_texture_blocks::DirectoryScope texture_blocks(path);
        if(cgltf_parse_file(&options,path,&data)!=cgltf_result_success)throw "Invalid glTF data";
        if(data->file_type!=cgltf_file_type_glb||data->nodes_count>512||data->images_count>128||data->skins_count)
            throw "Expected bounded static GLB";
        size_t declared_bytes=0;
        for(size_t bi=0;bi<data->buffers_count;bi++){
            if(data->buffers[bi].uri||data->buffers[bi].size>max_file_bytes-declared_bytes)
                throw "Expected bounded embedded buffers";
            declared_bytes+=data->buffers[bi].size;
        }
        if(cgltf_load_buffers(&options,data,path)!=cgltf_result_success||
           cgltf_validate(data)!=cgltf_result_success)throw "Invalid glTF buffers";
        /* World-transform traversal must not encounter cyclic parent chains. */
        for(size_t ni=0;ni<data->nodes_count;ni++){
            const cgltf_node *n=&data->nodes[ni];size_t depth=0;
            while(n&&depth++<=data->nodes_count)n=n->parent;
            if(n)throw "Cyclic node parents";
        }
        // The owned roster includes one original 103,181-triangle character.
        // Keep the ordinary pose budget, with a small explicit index allowance
        // for that exact authored roster marker; vertex/file limits still apply.
        size_t pose_max_indices=300000;
        if(pose_asset)for(size_t mi=0;mi<data->materials_count;++mi)
            if(data->materials[mi].name&&std::string_view(data->materials[mi].name)=="Owned Unity Orc roster weapon")pose_max_indices=320000;
        std::vector<CookedMesh> cooked;size_t vertex_count=0,index_count=0;
        float minimum[3]={std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max()};
        float maximum[3]={-minimum[0],-minimum[1],-minimum[2]};
        for(size_t ni=0;ni<data->nodes_count;ni++){
            const cgltf_node *node=&data->nodes[ni];
            if(!node->mesh||!in_scene(data,node))continue;
            float matrix[16],normal_transform[9],sign;
            cgltf_node_transform_world(node,matrix);
            for(float value:matrix)if(!std::isfinite(value))throw "Non-finite node transform";
            if(!normal_matrix(matrix,normal_transform,sign))throw "Singular node transform";
            for(size_t pi=0;pi<node->mesh->primitives_count;pi++){
                const cgltf_primitive &primitive=node->mesh->primitives[pi];
                if(primitive.type!=cgltf_primitive_type_triangles||primitive.has_draco_mesh_compression)throw "Unsupported primitive";
                if(cooked.size()>=max_meshes)throw "Mesh budget exceeded";
                const cgltf_accessor *position=attribute(primitive,cgltf_attribute_type_position);
                const cgltf_accessor *normal=attribute(primitive,cgltf_attribute_type_normal);
                const cgltf_accessor *uv=attribute(primitive,cgltf_attribute_type_texcoord);
                const cgltf_accessor *tangent=attribute(primitive,cgltf_attribute_type_tangent);
                const cgltf_accessor *paint=nullptr;
                for(size_t ai=0;ai<primitive.attributes_count;ai++)
                    if(primitive.attributes[ai].name&&!std::strcmp(primitive.attributes[ai].name,"_VALWOOD_PAINT"))paint=primitive.attributes[ai].data;
                if(!position||!position->count)throw "Missing positions";
                const auto *baked_light=world_model_baked_light::accessor(primitive,position->count);
                const auto *lava_heat=world_model_lava_heat::accessor(primitive,position->count);
                const auto *source_paint=world_model_source_paint::accessor(primitive,position->count);
                size_t count=primitive.indices?primitive.indices->count:position->count;
                if(!count||count%3||position->count>max_vertices-vertex_count||count>max_indices-index_count)throw "Geometry budget exceeded";
                if(pose_asset&&(position->count>100000-vertex_count||count>pose_max_indices-index_count))throw "Pose validation budget exceeded";
                vertex_count+=position->count;index_count+=count;
                CookedMesh mesh;mesh.baked_light=baked_light!=nullptr;mesh.source_paint=source_paint!=nullptr;mesh.material=primitive.material;mesh.facade=facade_parameters(primitive.material,data->textures_count);
                mesh.organic_body=world_model_orc_organic::classify(path,pose_asset,primitive)||world_model_skin::posed(path,pose_asset,primitive);
                mesh.hair_material=world_model_hair::posed(path,pose_asset,primitive);
                if(mesh.facade.mode&&(!uv||!tangent||(mesh.facade.mode!=5&&!source_paint&&(!paint||paint->count!=position->count||paint->type!=cgltf_type_scalar))))throw "Missing facade UV/paint/tangent stream";
                if((mesh.facade.mode==3||mesh.facade.mode==4)&&!source_paint)throw "Missing source RGB paint for city material";
                mesh.vertices.resize(position->count);mesh.indices.resize(count);
                for(size_t vi=0;vi<position->count;vi++){
                    Vertex &v=mesh.vertices[vi];float p[4]={0,0,0,1};
                    if(!cgltf_accessor_read_float(position,vi,p,3)||!finite3(p))throw "Invalid position";
                    transform_direction(matrix,p,v.p);for(int k=0;k<3;k++)v.p[k]+=matrix[12+k];
                    if(!finite3(v.p))throw "Invalid transformed position";
                    for(int k=0;k<3;k++){minimum[k]=std::min(minimum[k],v.p[k]);maximum[k]=std::max(maximum[k],v.p[k]);}
                    if(normal){if(!cgltf_accessor_read_float(normal,vi,p,3)||!finite3(p))throw "Invalid normal";transform_normal(normal_transform,p,v.n);}
                    if(uv){if(!cgltf_accessor_read_float(uv,vi,v.uv,2)||!std::isfinite(v.uv[0])||!std::isfinite(v.uv[1]))throw "Invalid UV";}
                    if(tangent){
                        if(!cgltf_accessor_read_float(tangent,vi,p,4)||!finite3(p)||!std::isfinite(p[3]))throw "Invalid tangent";
                        transform_direction(matrix,p,v.tangent);v.tangent[3]=p[3]*sign;
                    }else v.tangent[3]=1;
                    if(mesh.facade.mode&&paint&&(!cgltf_accessor_read_float(paint,vi,&v.facade_paint,1)||
                        !std::isfinite(v.facade_paint)||v.facade_paint<0||v.facade_paint>1))throw "Invalid facade paint";
                    world_model_baked_light::read(baked_light,vi,v.baked_light);
                    world_model_lava_heat::read(lava_heat,vi,v.facade_paint);
                    world_model_source_paint::read(source_paint,vi,v.baked_light,v.facade_paint);
                }
                for(size_t ii=0;ii<count;ii++){
                    size_t index=primitive.indices?cgltf_accessor_read_index(primitive.indices,ii):ii;
                    if(index>=position->count)throw "Index outside vertex stream";
                    mesh.indices[ii]=(uint32_t)index;
                }
                /* Mirrored nodes preserve outward winding after baking. */
                if(sign<0)for(size_t ii=0;ii<count;ii+=3)std::swap(mesh.indices[ii+1],mesh.indices[ii+2]);
                complete_basis(mesh,!normal,!tangent,pose_asset,sign);
                cooked.push_back(std::move(mesh));
            }
        }
        if(cooked.empty())throw "No triangle geometry";
        if(pose_asset&&(vertex_count>100000||index_count>pose_max_indices))throw "Pose validation budget exceeded";
        // Tagged combat unions are authored in the already accepted rig's
        // displayed coordinate space. A different boot silhouette must never
        // translate the unchanged weapon away from its physical sockets.
        if(pose_asset)for(const auto &source:cooked){
            unsigned appearance=0;world_model_variants::strip(source.material?source.material->name:nullptr,appearance);
            if(appearance)preserve_origin=true;
        }
        asset=new WorldModelAsset;asset->meshes.reserve(cooked.size());asset->staging.reserve(max_instances);
        asset->stream.exact=exact_instance_buffers();
        for(unsigned role=0;role<asset->neutral.size();role++)if(!(asset->neutral[role]=neutral_texture(role)))throw "Neutral sampler upload failed";
        asset->pose_asset=pose_asset;
        for(int k=0;k<3;k++){asset->minimum[k]=minimum[k];asset->maximum[k]=maximum[k];}
        if(!preserve_origin){asset->minimum[1]=0;asset->maximum[1]-=minimum[1];}
        glGenBuffers(1,&asset->instance_buffer);
        for(CookedMesh &source:cooked){
            asset->geometry_bytes+=source.vertices.size()*sizeof(Vertex)+source.indices.size()*sizeof(uint32_t);
            if(!preserve_origin)for(Vertex &v:source.vertices)v.p[1]-=minimum[1];
            asset->meshes.emplace_back();Mesh &mesh=asset->meshes.back();
            mesh.indices=(GLsizei)source.indices.size();
            if(retain_support&&pose_asset){mesh.cut_support_xyz.reserve(source.vertices.size());for(const auto &v:source.vertices)mesh.cut_support_xyz.push_back({v.p[0],v.p[1],v.p[2]});}
            if(retain_pnt&&pose_asset){mesh.cut_basis_nt.reserve(source.vertices.size());for(const auto &v:source.vertices)mesh.cut_basis_nt.push_back({v.n[0],v.n[1],v.n[2],v.tangent[0],v.tangent[1],v.tangent[2]});}
            if(pose_asset){
                mesh.pose_indices=source.indices;mesh.pose_keys.reserve(source.vertices.size());
                for(const Vertex &v:source.vertices)mesh.pose_keys.push_back({{v.uv[0],v.uv[1]},v.tangent[3],v.facade_paint});
            }
            mesh.facade=source.facade;
            mesh.baked_light=source.baked_light;
            mesh.source_paint=source.source_paint;
            mesh.organic_body=source.organic_body;
            mesh.hair_material=source.hair_material;
            const cgltf_material *material=source.material;
            const char *equipment_name=world_model_variants::strip(material?material->name:nullptr,mesh.variant_mask);
            mesh.equipment_mask=world_model_equipment::mask(equipment_name);
            mesh.support_weapon=material&&material->name&&(std::string_view(material->name)=="Shield hand-grip dark leather"||std::string_view(material->name)=="Shield grip forged rivets");
            if(pose_asset&&material&&material->name&&std::string_view(material->name)=="Shield hand-grip dark leather"&&!source.vertices.empty()){
                for(const auto &v:source.vertices)for(unsigned k=0;k<3;++k)asset->support_grip[k]+=v.p[k]/float(source.vertices.size());
                asset->has_support_grip=true;
            }
            if(pose_asset&&!mesh.equipment_mask&&material&&material->name&&source.vertices.size()<=16384&&
               (std::string_view(material->name).find("Owned knight")==0||std::string_view(material->name)=="Owned PowerOrc14 axe"||std::string_view(material->name)=="Owned Unity Orc Gnur weapon"||std::string_view(material->name)=="Owned Unity Orc roster weapon"||std::string_view(material->name)=="Owned Unity Knight roster weapon"||std::string_view(material->name)=="CC0 Poly Haven weathered steel and ash axe"||
                std::string_view(material->name)=="Oiled ash wood"||std::string_view(material->name)=="Forged steel"||std::string_view(material->name)=="Dark leather binding"||mesh.support_weapon)){
                mesh.held_weapon=true;
                mesh.rigid_source.reserve(source.vertices.size());
                for(const auto &v:source.vertices)mesh.rigid_source.push_back({v.p[0],v.p[1],v.p[2]});
            }
            if(material){
                world_model_materials::winter_parameters(material,mesh.winter);
                world_model_materials::leaf_transmission_parameters(material,mesh.leaf_transmission);
                // Explicit contracts for owned character materials. Combined
                // skin/clothing atlases and arbitrary scenery are never tinted.
                if(pose_asset&&material->name){const std::string_view name=material->name;
                    if(name=="Sintel source alpha hair"||name=="Purchased_hair01"||name=="Purchased_hair02"||name=="Purchased_hair03"||name=="Purchased_hair05")mesh.character_region=1;
                }
                const cgltf_pbr_metallic_roughness &pbr=material->pbr_metallic_roughness;
                std::copy(pbr.base_color_factor,pbr.base_color_factor+4,mesh.factor);
                mesh.roughness=pbr.roughness_factor;mesh.metallic=pbr.metallic_factor;
                // OPAQUE ignores texture/factor alpha (it can hold utility
                // data, e.g. the purchased thatch). Only MASK clips coverage.
                // BLEND retains its prior bounded cutout fallback; this path
                // does not implement sorted translucent model rendering.
                mesh.cutoff=material->alpha_mode==cgltf_alpha_mode_mask?material->alpha_cutoff:
                    material->alpha_mode==cgltf_alpha_mode_blend?.4f:0.f;
                const bool opaque_material=material->alpha_mode==cgltf_alpha_mode_opaque;
                const bool known_hand=mesh.facade.mode==0&&material->name&&std::string_view(material->name)=="VALWOOD_EQUIPMENT:32 NPC_Characters_04_Hands";
                mesh.base=material_texture(path,pbr.base_color_texture,true,opaque_material,0,known_hand?&mesh.skin_material:nullptr,max_texture_edge);
                if(opaque_material&&mesh.facade.mode>0)mesh.detail_samplers[0]=facade_sampler(pbr.base_color_texture);
                const bool packed=world_model_materials::facade_packed_layers(mesh.facade);
                mesh.normal=material_texture(path,material->normal_texture,false,false,packed?0:world_texture_blocks::format_normal_bc7,nullptr,max_texture_edge);
                mesh.normal_scale=material->normal_texture.scale;
                if(!std::isfinite(mesh.normal_scale)||mesh.normal_scale<0||mesh.normal_scale>8)throw "Invalid normal strength";
                mesh.arm=material_texture(path,pbr.metallic_roughness_texture,packed,packed&&opaque_material,packed?0:world_texture_blocks::format_linear_bc7,nullptr,max_texture_edge);
                const float emission_strength=material->has_emissive_strength?material->emissive_strength.emissive_strength:1.f;
                if(!std::isfinite(emission_strength)||emission_strength<0||emission_strength>10000)throw "Invalid emissive strength";
                for(int j=0;j<3;j++) {
                    mesh.emissive_factor[j]=material->emissive_factor[j]*emission_strength;
                    if(!std::isfinite(mesh.emissive_factor[j])||mesh.emissive_factor[j]<0)throw "Invalid emissive factor";
                }
                if(material->emissive_texture.texture) {
                    mesh.emissive=material_texture(path,material->emissive_texture,true,opaque_material,0,nullptr,max_texture_edge);
                    if(!mesh.emissive)throw "Unsupported or failed emissive image";
                }
                for(int j=0;j<3;j++)if(mesh.facade.aux[j]>=0){
                    cgltf_texture_view view={};view.texture=&data->textures[mesh.facade.aux[j]];
                    mesh.facade_aux[j]=material_texture(path,view,world_model_materials::facade_srgb_aux(mesh.facade,j),
                        opaque_material&&world_model_materials::facade_color_aux(mesh.facade,j),0,nullptr,max_texture_edge);
                    if(!mesh.facade_aux[j])throw "Failed facade source image";
                    if(opaque_material&&world_model_materials::facade_color_aux(mesh.facade,j))mesh.detail_samplers[j+1]=facade_sampler(view);
                }
                if((pbr.base_color_texture.texture&&!mesh.base)||(material->normal_texture.texture&&!mesh.normal)||
                   (pbr.metallic_roughness_texture.texture&&!mesh.arm))throw "Unsupported or failed material image";
                for(float value:mesh.factor)if(!std::isfinite(value))throw "Invalid base factor";
            }
            const GLH_Attrib attributes[]={{0,3,GL_FLOAT,0,offsetof(Vertex,p)},
                {1,3,GL_FLOAT,0,offsetof(Vertex,n)},{2,2,GL_FLOAT,0,offsetof(Vertex,uv)},
                {3,4,GL_FLOAT,0,offsetof(Vertex,tangent)},{7,1,GL_FLOAT,0,offsetof(Vertex,facade_paint)},
                {12,4,GL_FLOAT,0,offsetof(Vertex,baked_light)}};
            std::vector<float> packed;
            const bool pack=pose_asset&&!source.baked_light&&compact_pose_geometry();
            if(pack){
                mesh.vertex_stride=pose_vertex_stride;packed.resize(source.vertices.size()*13);
                for(std::size_t i=0;i<source.vertices.size();++i)std::memcpy(packed.data()+i*13,&source.vertices[i],pose_vertex_stride);
                const auto saved=source.vertices.size()*(sizeof(Vertex)-pose_vertex_stride);
                asset->geometry_bytes-=saved;pose_vertex_saved+=saved;
            }
            const auto hash=pose_asset&&compact_pose_geometry()?pose_index_hash(source.indices):0;
            SharedPoseIndices *shared=nullptr;
            if(pose_asset&&compact_pose_geometry())for(auto &entry:shared_pose_indices)
                if(entry.hash==hash&&entry.indices==source.indices){shared=&entry;break;}
            const void *vertex_data=pack?static_cast<const void*>(packed.data()):static_cast<const void*>(source.vertices.data());
            const auto vertex_bytes=source.vertices.size()*mesh.vertex_stride;
            if(pack)mesh.pose_page=pose_page_store(vertex_data,vertex_bytes);
            mesh.vao=GLH_mesh_full(mesh.pose_page?nullptr:vertex_data,
                mesh.pose_page?0:vertex_bytes,shared?nullptr:source.indices.data(),shared?0:source.indices.size()*sizeof(uint32_t),
                mesh.vertex_stride,attributes,pack?5:6,&mesh.vbo,&mesh.ebo);
            if(mesh.pose_page){mesh.pose_page->buffer=mesh.vbo;asset->paged_pose=true;}
            if(shared){
                glBindVertexArray(mesh.vao);glDeleteBuffers(1,&mesh.ebo);mesh.ebo=shared->buffer;
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,mesh.ebo);++shared->references;mesh.shared_indices=true;
                ++pose_index_hits;pose_index_saved+=source.indices.size()*sizeof(uint32_t);
            }else if(pose_asset&&compact_pose_geometry()){
                shared_pose_indices.push_back({hash,mesh.ebo,1,source.indices});mesh.shared_indices=true;
            }
            glBindVertexArray(mesh.vao);glBindBuffer(GL_ARRAY_BUFFER,asset->instance_buffer);
            glEnableVertexAttribArray(4);glVertexAttribPointer(4,4,GL_FLOAT,GL_FALSE,sizeof(GPUInstance),(void*)offsetof(GPUInstance,x));glVertexAttribDivisor(4,1);
            glEnableVertexAttribArray(5);glVertexAttribPointer(5,1,GL_FLOAT,GL_FALSE,sizeof(GPUInstance),(void*)offsetof(GPUInstance,yaw));glVertexAttribDivisor(5,1);
            glEnableVertexAttribArray(6);glVertexAttribPointer(6,3,GL_FLOAT,GL_FALSE,sizeof(GPUInstance),(void*)offsetof(GPUInstance,fade));glVertexAttribDivisor(6,1);
        }
        glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);
        if(glGetError()!=GL_NO_ERROR)throw "OpenGL upload failed";
        cgltf_free(data);
        std::printf("[WorldModels] %s: %zu meshes, %zu triangles, height %.2f m\n",path,asset->meshes.size(),index_count/3,asset->maximum[1]);
        if(load_observer)load_observer(load_observer_user);
        return asset;
    }catch(const char *message){std::fprintf(stderr,"[WorldModels] %s: %s\n",path,message);}
    catch(const std::exception &error){std::fprintf(stderr,"[WorldModels] %s: %s\n",path,error.what());}
    if(data)cgltf_free(data);
    world_model_free(asset);
    return nullptr;
}

extern "C" void world_model_set_load_observer(void (*observer)(void *),void *user){load_observer=observer;load_observer_user=user;}
extern "C" WorldModelAsset *world_model_load(const char *path){return load_model(path,false);}
extern "C" WorldModelAsset *world_model_load_budgeted(const char *path,unsigned edge){return world_model_texture_budget::valid(edge)?load_model(path,false,false,false,false,edge):nullptr;}
struct WorldModelStaged {
    std::shared_ptr<const world_model_cpu::Asset> cpu;
    WorldModelAsset *asset=nullptr;
    unsigned anisotropy=1,facade_anisotropy=1;std::size_t texture=0,level=0,row=0,mesh=0,offset=0,new_texture_bytes=0;
    GLuint image=0;bool image_allocated=false,mesh_allocated=false,index_stream=false,complete=false,failed=false;
};
namespace {
TextureCachePolicy staged_policy(const world_model_cpu::Texture &t,unsigned anisotropy){return world_model_pending_textures::policy(t,anisotropy);}
void staged_sampler(const world_model_cpu::Texture &t,unsigned anisotropy){glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GLint(t.min_filter));glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GLint(t.mag_filter));glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GLint(t.wrap_s));glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GLint(t.wrap_t));if(anisotropy>1)glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_ANISOTROPY_EXT,float(anisotropy));}
void staged_mesh_attributes(Mesh &mesh,GLuint instances){
    glBindVertexArray(mesh.vao);glBindBuffer(GL_ARRAY_BUFFER,mesh.vbo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,mesh.ebo);
    const GLH_Attrib attributes[]={{0,3,GL_FLOAT,0,offsetof(Vertex,p)},{1,3,GL_FLOAT,0,offsetof(Vertex,n)},{2,2,GL_FLOAT,0,offsetof(Vertex,uv)},{3,4,GL_FLOAT,0,offsetof(Vertex,tangent)},{7,1,GL_FLOAT,0,offsetof(Vertex,facade_paint)},{12,4,GL_FLOAT,0,offsetof(Vertex,baked_light)}};
    for(const auto &a:attributes){glEnableVertexAttribArray(a.loc);glVertexAttribPointer(a.loc,a.n,a.type,a.norm,sizeof(Vertex),reinterpret_cast<void *>(a.offset));}
    glBindBuffer(GL_ARRAY_BUFFER,instances);glEnableVertexAttribArray(4);glVertexAttribPointer(4,4,GL_FLOAT,GL_FALSE,sizeof(GPUInstance),reinterpret_cast<void *>(offsetof(GPUInstance,x)));glVertexAttribDivisor(4,1);glEnableVertexAttribArray(5);glVertexAttribPointer(5,1,GL_FLOAT,GL_FALSE,sizeof(GPUInstance),reinterpret_cast<void *>(offsetof(GPUInstance,yaw)));glVertexAttribDivisor(5,1);glEnableVertexAttribArray(6);glVertexAttribPointer(6,3,GL_FLOAT,GL_FALSE,sizeof(GPUInstance),reinterpret_cast<void *>(offsetof(GPUInstance,fade)));glVertexAttribDivisor(6,1);
}
}
WorldModelStaged *world_model_stage(std::shared_ptr<const world_model_cpu::Asset> cpu,unsigned anisotropy,unsigned facade_anisotropy){
    if(!cpu||cpu->meshes.empty()||cpu->meshes.size()>128||cpu->textures.size()>256||anisotropy>16||facade_anisotropy>16)return nullptr;
    WorldModelStaged *stage=nullptr;
    try{
        static_assert(sizeof(Vertex)==sizeof(world_model_cpu::Vertex),"CPU/GPU vertex layouts must agree");
        stage=new WorldModelStaged;stage->cpu=std::move(cpu);stage->anisotropy=std::max(1u,anisotropy);stage->facade_anisotropy=std::max(1u,facade_anisotropy);stage->asset=new WorldModelAsset;auto &a=*stage->asset;a.staged_materials=true;a.geometry_bytes=stage->cpu->geometry_bytes;a.stream.exact=exact_instance_buffers();a.staging.reserve(max_instances);a.meshes.resize(stage->cpu->meshes.size());a.texture_owners.resize(stage->cpu->textures.size());std::copy(stage->cpu->minimum,stage->cpu->minimum+3,a.minimum);std::copy(stage->cpu->maximum,stage->cpu->maximum+3,a.maximum);
        for(unsigned role=0;role<a.neutral.size();role++){
            const auto key=stage->cpu->neutral[role];if(!key||!key->prepared)throw "Missing prepared neutral key";
            const TextureCachePolicy policy={role==0,GL_LINEAR_MIPMAP_LINEAR,GL_LINEAR,GL_REPEAT,GL_REPEAT,1};a.neutral[role]=texture_cache_find_prepared(key->prepared,&policy);
            if(!a.neutral[role]){static const unsigned char white[8]={'V','W','N',1,255,255,255,255},flat[8]={'V','W','N',1,128,128,255,255};const GLuint id=upload_neutral(role==2?flat:white,8,&policy);if(!id)throw "Neutral sampler upload failed";a.neutral[role]=texture_cache_adopt_prepared(key->take(),&policy,id,delete_texture);if(!a.neutral[role])throw "Neutral sampler adoption failed";texture_cache_track_gpu_bytes(a.neutral[role],4);}
        }
        glGenBuffers(1,&a.instance_buffer);
        for(unsigned i=0;i<a.texture_owners.size();i++){const auto &t=stage->cpu->textures[i];const auto policy=staged_policy(t,stage->anisotropy);a.texture_owners[i]=texture_cache_find_prepared(t.cache_key?t.cache_key->prepared:nullptr,&policy);if(!a.texture_owners[i])for(const auto &l:t.levels)stage->new_texture_bytes+=l.rgba.size();}
        if(glGetError()!=GL_NO_ERROR)throw "Staged model initialization failed";
        return stage;
    }catch(...){world_model_stage_free(stage);return nullptr;}
}
bool world_model_stage_step(WorldModelStaged *s,std::size_t budget,std::size_t *consumed,bool *complete){
    if(consumed)*consumed=0;
    if(complete)*complete=false;
    if(!s||s->failed||!s->asset||!consumed||!complete||!budget)return false;
    if(s->complete){*complete=true;return true;}
    std::size_t used=0;glActiveTexture(GL_TEXTURE0);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_IMAGE_HEIGHT,0);glPixelStorei(GL_UNPACK_SKIP_IMAGES,0);
    world_model_pending_textures::refresh(s);
    while(s->texture<s->cpu->textures.size()&&used<budget){
        // A prior texture in this same primary/shadow transaction may just have
        // become shared. Refresh before beginning another image allocation.
        if(!s->image)world_model_pending_textures::refresh(s);
        if(s->asset->texture_owners[s->texture]){s->texture++;continue;}
        const auto &t=s->cpu->textures[s->texture];if(t.levels.empty()){s->failed=true;break;}
        if(!s->image){glGenTextures(1,&s->image);glBindTexture(GL_TEXTURE_2D,s->image);staged_sampler(t,s->anisotropy);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_BASE_LEVEL,0);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,GLint(t.levels.size()-1));}else glBindTexture(GL_TEXTURE_2D,s->image);
        const auto &l=t.levels[s->level];const std::size_t rows=t.compressed?(l.height+3)/4:l.height;const std::size_t stride=t.compressed?std::size_t((l.width+3)/4)*16:std::size_t(l.width)*4;
        if(stride>budget-used)break;
        if(!s->image_allocated){if(t.compressed)glCompressedTexImage2D(GL_TEXTURE_2D,GLint(s->level),t.srgb?GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM:GL_COMPRESSED_RGBA_BPTC_UNORM,GLsizei(l.width),GLsizei(l.height),0,GLsizei(l.rgba.size()),nullptr);else glTexImage2D(GL_TEXTURE_2D,GLint(s->level),t.srgb?GL_SRGB8_ALPHA8:GL_RGBA8,GLsizei(l.width),GLsizei(l.height),0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);s->image_allocated=true;}
        const std::size_t count=std::min(rows-s->row,(budget-used)/stride),bytes=count*stride;
        if(t.compressed){const unsigned y=unsigned(s->row)*4,h=std::min(l.height-y,unsigned(count)*4);glCompressedTexSubImage2D(GL_TEXTURE_2D,GLint(s->level),0,GLint(y),GLsizei(l.width),GLsizei(h),t.srgb?GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM:GL_COMPRESSED_RGBA_BPTC_UNORM,GLsizei(bytes),l.rgba.data()+s->row*stride);}else glTexSubImage2D(GL_TEXTURE_2D,GLint(s->level),0,GLint(s->row),GLsizei(l.width),GLsizei(count),GL_RGBA,GL_UNSIGNED_BYTE,l.rgba.data()+s->row*stride);
        used+=bytes;s->row+=count;
        if(s->row==rows){s->row=0;s->level++;s->image_allocated=false;if(s->level==t.levels.size()){const auto policy=staged_policy(t,s->anisotropy);auto *key=t.cache_key?t.cache_key->take():nullptr;if(!key){s->failed=true;break;}const GLuint image=texture_cache_adopt_prepared(key,&policy,s->image,delete_texture);if(!image){s->failed=true;break;}std::size_t size=0;for(const auto &m:t.levels)size+=m.rgba.size();texture_cache_track_gpu_bytes(image,size);s->new_texture_bytes-=std::min(s->new_texture_bytes,size);s->asset->texture_owners[s->texture]=image;s->image=0;s->level=0;s->texture++;}}
    }
    while(!s->failed&&s->texture==s->cpu->textures.size()&&s->mesh<s->cpu->meshes.size()&&used<budget){
        const auto &source=s->cpu->meshes[s->mesh];auto &mesh=s->asset->meshes[s->mesh];
        mesh.baked_light=source.baked_light;
        mesh.source_paint=source.source_paint;mesh.equipment_mask=source.equipment_mask;
        mesh.organic_body=false; // Staged scenery never opts into owned pose metadata.
        mesh.hair_material=0;
        if(!s->mesh_allocated){glGenVertexArrays(1,&mesh.vao);glGenBuffers(1,&mesh.vbo);glGenBuffers(1,&mesh.ebo);glBindVertexArray(mesh.vao);glBindBuffer(GL_ARRAY_BUFFER,mesh.vbo);glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(source.vertices.size()*sizeof(Vertex)),nullptr,GL_STATIC_DRAW);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,mesh.ebo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,GLsizeiptr(source.indices.size()*sizeof(std::uint32_t)),nullptr,GL_STATIC_DRAW);s->mesh_allocated=true;}
        const std::size_t size=s->index_stream?source.indices.size()*sizeof(std::uint32_t):source.vertices.size()*sizeof(Vertex);const auto *data=s->index_stream?reinterpret_cast<const unsigned char *>(source.indices.data()):reinterpret_cast<const unsigned char *>(source.vertices.data());const std::size_t bytes=std::min(size-s->offset,budget-used);glBindVertexArray(mesh.vao);glBindBuffer(s->index_stream?GL_ELEMENT_ARRAY_BUFFER:GL_ARRAY_BUFFER,s->index_stream?mesh.ebo:mesh.vbo);glBufferSubData(s->index_stream?GL_ELEMENT_ARRAY_BUFFER:GL_ARRAY_BUFFER,GLintptr(s->offset),GLsizeiptr(bytes),data+s->offset);used+=bytes;s->offset+=bytes;
        if(s->offset==size){s->offset=0;if(!s->index_stream)s->index_stream=true;else{mesh.indices=GLsizei(source.indices.size());std::copy(source.factor,source.factor+4,mesh.factor);std::copy(source.emissive_factor,source.emissive_factor+3,mesh.emissive_factor);mesh.roughness=source.roughness;mesh.metallic=source.metallic;mesh.cutoff=source.cutoff;mesh.normal_scale=source.normal_scale;std::copy(source.winter,source.winter+3,mesh.winter);std::copy(source.leaf_transmission,source.leaf_transmission+4,mesh.leaf_transmission);mesh.facade.mode=source.facade.mode;std::copy(source.facade.aux,source.facade.aux+3,mesh.facade.aux);std::copy(source.facade.scale,source.facade.scale+3,mesh.facade.scale);GLuint *slots[]={&mesh.base,&mesh.normal,&mesh.arm,&mesh.emissive,&mesh.facade_aux[0],&mesh.facade_aux[1],&mesh.facade_aux[2]};for(unsigned i=0;i<7;i++)if(source.textures[i]>=0)*slots[i]=s->asset->texture_owners[unsigned(source.textures[i])];for(unsigned i=0;i<4;i++)if(source.detail_sampler[i]){const int index=source.textures[i?i+3:0];if(index>=0){const auto &t=s->cpu->textures[unsigned(index)];glGenSamplers(1,&mesh.detail_samplers[i]);glSamplerParameteri(mesh.detail_samplers[i],GL_TEXTURE_MIN_FILTER,GLint(t.min_filter));glSamplerParameteri(mesh.detail_samplers[i],GL_TEXTURE_MAG_FILTER,GLint(t.mag_filter));glSamplerParameteri(mesh.detail_samplers[i],GL_TEXTURE_WRAP_S,GLint(t.wrap_s));glSamplerParameteri(mesh.detail_samplers[i],GL_TEXTURE_WRAP_T,GLint(t.wrap_t));if(s->facade_anisotropy>1)glSamplerParameterf(mesh.detail_samplers[i],GL_TEXTURE_MAX_ANISOTROPY_EXT,float(s->facade_anisotropy));}}staged_mesh_attributes(mesh,s->asset->instance_buffer);s->mesh++;s->mesh_allocated=s->index_stream=false;}}
    }
    glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);glBindTexture(GL_TEXTURE_2D,0);glPixelStorei(GL_UNPACK_ALIGNMENT,4);
    if(glGetError()!=GL_NO_ERROR)s->failed=true;
    *consumed=used;s->complete=!s->failed&&s->texture==s->cpu->textures.size()&&s->mesh==s->cpu->meshes.size();*complete=s->complete;return !s->failed;
}
WorldModelAsset *world_model_stage_take(WorldModelStaged *s){if(!s||!s->complete||s->failed)return nullptr;auto *a=s->asset;s->asset=nullptr;return a;}
void world_model_stage_free(WorldModelStaged *s){if(!s)return;if(s->image)glDeleteTextures(1,&s->image);world_model_free(s->asset);delete s;}
std::size_t world_model_geometry_bytes(const WorldModelAsset *asset){return asset?asset->geometry_bytes:0;}
extern "C" unsigned world_model_pose_variant_mask(const WorldModelAsset *asset){
    unsigned mask=0;if(asset)for(const auto &mesh:asset->meshes)mask|=mesh.variant_mask;return mask;
}
std::size_t world_model_staged_new_texture_bytes(const WorldModelStaged *s){return s?s->new_texture_bytes:0;}
std::size_t world_model_staged_new_texture_bytes_union(WorldModelStaged *primary,WorldModelStaged *shadow){return world_model_pending_textures::missing_union(primary,shadow);}
bool world_model_warm(WorldModelAsset *asset,unsigned program){if(!asset||!program)return false;for(const auto &e:asset->programs)if(e.program==program&&e.epoch==world_program_epoch)return true;auto &e=asset->programs[asset->next_program];e.program=program;e.epoch=world_program_epoch;e.uniforms=locations(program);asset->next_program=(asset->next_program+1)%asset->programs.size();return true;}
extern "C" WorldModelTextureBlockStats world_model_texture_block_stats(void){return world_model_texture_blocks::counters();}
extern "C" WorldModelDepthMaterialStats world_model_depth_material_stats(void){return depth_material_stats;}
extern "C" WorldModelPoseStorageStats world_model_pose_storage_stats(void){return {pose_vertex_saved,pose_index_saved,pose_index_hits,shared_pose_indices.size()};}
extern "C" void world_model_material_batch_begin(void){if(!material_batch_depth)material_batch_clear();++material_batch_depth;}
extern "C" void world_model_material_batch_end(void){if(material_batch_depth&&!--material_batch_depth)material_batch_clear();}
extern "C" uint64_t world_model_material_batch_hits(void){return material_batch_hits;}
extern "C" WorldModelPoseStreamStats world_model_pose_stream_stats(void){const auto &g=pose_pager;return {g.pages,g.end,g.resident,g.peak,pose_cache_budget(),g.uploads,g.hits,g.evictions,g.failures};}
extern "C" unsigned world_model_depth_material_program(unsigned program){
    // An explicit diagnostic A/B preserves all shaders, images and geometry.
    static const bool disabled=[](){const char *p=std::getenv("VALWOOD_QA_DISABLE_DEPTH_MATERIALS");return p&&!std::strcmp(p,"1");}();
    const unsigned previous=depth_material_program;
    depth_material_program=disabled?0:program;return previous;
}
extern "C" WorldModelAsset *world_model_load_pose(const char *path,unsigned version){
    return version==WORLD_MODEL_POSE_BLEND_VERSION?load_model(path,true):nullptr;
}
extern "C" WorldModelAsset *world_model_load_pose_origin(const char *path,unsigned version){
    if(version!=WORLD_MODEL_POSE_BLEND_VERSION)return nullptr;
    return load_model(path,true,true);
}
extern "C" WorldModelAsset *world_model_load_pose_support(const char *path,unsigned version,bool preserve_origin){return version==WORLD_MODEL_POSE_BLEND_VERSION?load_model(path,true,preserve_origin,true):nullptr;}
extern "C" WorldModelAsset *world_model_load_pose_support_pnt(const char *path,unsigned version,bool preserve_origin){return version==WORLD_MODEL_POSE_BLEND_VERSION?load_model(path,true,preserve_origin,true,true):nullptr;}
extern "C" WorldModelAsset *world_model_load_pose_support_pnt_budgeted(const char *path,unsigned version,bool preserve_origin,unsigned max_texture_edge){return version==WORLD_MODEL_POSE_BLEND_VERSION&&world_model_texture_budget::valid(max_texture_edge)?load_model(path,true,preserve_origin,true,true,max_texture_edge):nullptr;}
extern "C" size_t world_model_pose_support_bytes(const WorldModelAsset *a){std::size_t n=0;if(a)for(const auto &m:a->meshes)n+=m.cut_support_xyz.size()*sizeof(std::array<float,3>);return n;}
extern "C" size_t world_model_pose_vertices(const WorldModelAsset *a){std::size_t n=0;if(a)for(const auto &m:a->meshes)n+=m.pose_keys.size();return n;}
extern "C" size_t world_model_pose_pnt_bytes(const WorldModelAsset *a){std::size_t n=0;if(a)for(const auto &m:a->meshes)n+=m.cut_support_xyz.capacity()*sizeof(std::array<float,3>)+m.cut_basis_nt.capacity()*sizeof(std::array<float,6>);return n;}




namespace {
std::uint64_t anatomy_key(const Mesh &m){
    std::uint64_t h=14695981039346656037ull;
    auto bytes=[&](const void *p,std::size_t n){const auto *b=static_cast<const unsigned char*>(p);for(std::size_t i=0;i<n;++i)h=(h^b[i])*1099511628211ull;};
    for(const auto &k:m.pose_keys)bytes(k.uv,sizeof k.uv);
    bytes(m.pose_indices.data(),m.pose_indices.size()*sizeof(std::uint32_t));return h;
}
}
extern "C" void world_model_anatomy_stream_free(WorldModelAnatomyStream *p){if(p){if(!p->buffers.empty())glDeleteBuffers(GLsizei(p->buffers.size()),p->buffers.data());delete p;}}
extern "C" WorldModelAnatomyStream *world_model_anatomy_stream_create(const WorldModelAsset *a,const unsigned *counts,const uint64_t *keys,const float *w,size_t n){
    if(!a||!a->pose_asset||!counts||!keys||!w||n!=a->meshes.size()||n>32)return nullptr;
    std::size_t total=0;for(std::size_t m=0;m<n;++m){if(counts[m]!=a->meshes[m].pose_keys.size()||keys[m]!=anatomy_key(a->meshes[m]))return nullptr;total+=counts[m];}
    if(total>100000)return nullptr;for(std::size_t i=0;i<total*8;++i)if(!std::isfinite(w[i])||w[i]<0||w[i]>1)return nullptr;
    auto *p=new(std::nothrow)WorldModelAnatomyStream;if(!p)return nullptr;
    try{p->counts.assign(counts,counts+n);p->keys.assign(keys,keys+n);p->buffers.resize(n);p->weights.resize(total);std::memcpy(p->weights.data(),w,total*8*sizeof(float));std::size_t offset=0;for(std::size_t m=0;m<n;++m){p->offsets.push_back(offset);offset+=counts[m];}}catch(...){delete p;return nullptr;}
    GLint previous=0;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&previous);glGenBuffers(GLsizei(n),p->buffers.data());
    std::size_t at=0;for(std::size_t m=0;m<n;++m){glBindBuffer(GL_ARRAY_BUFFER,p->buffers[m]);glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(counts[m]*8*sizeof(float)),w+at*8,GL_STATIC_DRAW);at+=counts[m];}
    glBindBuffer(GL_ARRAY_BUFFER,GLuint(previous));if(glGetError()!=GL_NO_ERROR){world_model_anatomy_stream_free(p);return nullptr;}return p;
}
extern "C" bool world_model_pose_blend_attach_anatomy(WorldModelPoseBlend *b,WorldModelAnatomyStream *p){
    if(!b||!b->a||!b->b||!p||b->full_transform||b->character_active||b->character_buffer||p->buffers.size()!=b->vaos.size())return false;
    for(std::size_t m=0;m<p->buffers.size();++m)if(p->counts[m]!=b->a->meshes[m].pose_keys.size()||p->counts[m]!=b->b->meshes[m].pose_keys.size()||p->keys[m]!=anatomy_key(b->a->meshes[m])||p->keys[m]!=anatomy_key(b->b->meshes[m]))return false;
    GLint vao=0,buffer=0;glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&buffer);
    for(std::size_t m=0;m<p->buffers.size();++m){glBindVertexArray(b->vaos[m]);glBindBuffer(GL_ARRAY_BUFFER,p->buffers[m]);for(unsigned j=0;j<2;++j){glEnableVertexAttribArray(12+j);glVertexAttribPointer(12+j,4,GL_FLOAT,GL_FALSE,8*sizeof(float),reinterpret_cast<const void*>(std::uintptr_t(j*4*sizeof(float))));glVertexAttribDivisor(12+j,0);}}
    glBindVertexArray(GLuint(vao));glBindBuffer(GL_ARRAY_BUFFER,GLuint(buffer));if(glGetError()!=GL_NO_ERROR)return false;b->anatomy=p;return true;
}

extern "C" void world_model_pose_blend_free(WorldModelPoseBlend *batch){
    if(!batch)return;
    if(!batch->subset_buffers.empty())glDeleteBuffers((GLsizei)batch->subset_buffers.size(),batch->subset_buffers.data());
    if(!batch->vaos.empty())glDeleteVertexArrays((GLsizei)batch->vaos.size(),batch->vaos.data());
    glDeleteBuffers(1,&batch->character_buffer);glDeleteBuffers(1,&batch->transform_buffer);glDeleteBuffers(1,&batch->instances);delete batch;
}

extern "C" WorldModelPoseBlend *world_model_pose_blend_create(size_t capacity,unsigned version){
    if(version!=WORLD_MODEL_POSE_BLEND_VERSION||!capacity||capacity>max_instances)return nullptr;
    WorldModelPoseBlend *batch=nullptr;
    try {
        batch=new WorldModelPoseBlend;batch->capacity=capacity;
        batch->stream.exact=exact_instance_buffers();
        batch->staging.reserve(capacity);batch->vaos.reserve(max_meshes);
        glGenBuffers(1,&batch->instances);
        if(!batch->instances||glGetError()!=GL_NO_ERROR)throw "Pose instance buffer creation failed";
        return batch;
    }catch(...){world_model_pose_blend_free(batch);return nullptr;}
}

namespace {
bool same_pose_material(const Mesh &a,const Mesh &b){
    return a.base==b.base&&a.normal==b.normal&&a.arm==b.arm&&a.emissive==b.emissive&&
        std::equal(a.emissive_factor,a.emissive_factor+3,b.emissive_factor)&&std::equal(a.winter,a.winter+3,b.winter)&&
        std::equal(a.leaf_transmission,a.leaf_transmission+4,b.leaf_transmission)&&
        std::equal(a.facade_aux,a.facade_aux+3,b.facade_aux)&&
        a.equipment_mask==b.equipment_mask&&a.variant_mask==b.variant_mask&&a.character_region==b.character_region&&a.skin_material==b.skin_material&&a.organic_body==b.organic_body&&a.hair_material==b.hair_material&&a.held_weapon==b.held_weapon&&a.facade.mode==b.facade.mode&&std::equal(a.facade.scale,a.facade.scale+3,b.facade.scale)&&
        std::equal(a.factor,a.factor+4,b.factor)&&a.roughness==b.roughness&&a.normal_scale==b.normal_scale&&
        a.metallic==b.metallic&&a.cutoff==b.cutoff;
}
bool compatible_poses(const WorldModelAsset *a,const WorldModelAsset *b){
    if(!a||!b||!a->pose_asset||!b->pose_asset||a->meshes.size()!=b->meshes.size())return false;
    static_assert(sizeof(PoseKey)==4*sizeof(float),"Pose keys have no padding");
    for(size_t k=0;k<a->meshes.size();k++){
        const Mesh &x=a->meshes[k],&y=b->meshes[k];
        if(x.pose_keys.empty()||x.pose_keys.size()!=y.pose_keys.size()||x.pose_indices!=y.pose_indices||!same_pose_material(x,y)){
            std::fprintf(stderr,"[PoseBlend] Incompatible mesh%zu counts%zu/%zu indices%d materials%d\n",k,x.pose_keys.size(),y.pose_keys.size(),int(x.pose_indices==y.pose_indices),int(same_pose_material(x,y)));return false;
        }
        if(std::memcmp(x.pose_keys.data(),y.pose_keys.data(),x.pose_keys.size()*sizeof(PoseKey))){
            for(size_t v=0;v<x.pose_keys.size();v++)if(std::memcmp(&x.pose_keys[v],&y.pose_keys[v],sizeof(PoseKey))){
                const auto &p=x.pose_keys[v],&q=y.pose_keys[v];std::fprintf(stderr,"[PoseBlend] Incompatible mesh%zu vertex%zu UV(%g,%g)/(%g,%g) handed%g/%g paint%g/%g\n",k,v,p.uv[0],p.uv[1],q.uv[0],q.uv[1],p.handedness,q.handedness,p.paint,q.paint);break;
            }return false;
        }
    }
    return true;
}
}

extern "C" bool world_model_pose_blend_set_targets(WorldModelPoseBlend *batch,WorldModelAsset *a,WorldModelAsset *b){
    if(!batch||!compatible_poses(a,b))return false;
    if(batch->a==a&&batch->b==b)return true;
    if(batch->character_buffer||batch->anatomy||batch->full_transform||!batch->subset_buffers.empty())return false; // configured once at loading boundary
    std::vector<GLuint> next;
    try {next.resize(a->meshes.size());}catch(...){return false;}
    GLint previous_vao=0,previous_buffer=0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&previous_vao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&previous_buffer);
    glGenVertexArrays((GLsizei)next.size(),next.data());
    for(size_t k=0;k<next.size();k++){
        glBindVertexArray(next[k]);glBindBuffer(GL_ARRAY_BUFFER,a->meshes[k].vbo);
        const GLH_Attrib source[]={{0,3,GL_FLOAT,0,offsetof(Vertex,p)},{1,3,GL_FLOAT,0,offsetof(Vertex,n)},
            {2,2,GL_FLOAT,0,offsetof(Vertex,uv)},{3,4,GL_FLOAT,0,offsetof(Vertex,tangent)},{7,1,GL_FLOAT,0,offsetof(Vertex,facade_paint)}};
        for(const auto &v:source){glEnableVertexAttribArray(v.loc);glVertexAttribPointer(v.loc,v.n,v.type,v.norm,a->meshes[k].vertex_stride,(void*)v.offset);}
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,a->meshes[k].ebo);
        glBindBuffer(GL_ARRAY_BUFFER,b->meshes[k].vbo);
        for(const auto &v:std::array<GLH_Attrib,3>{{{8,3,GL_FLOAT,0,offsetof(Vertex,p)},{9,3,GL_FLOAT,0,offsetof(Vertex,n)},{10,4,GL_FLOAT,0,offsetof(Vertex,tangent)}}}){
            glEnableVertexAttribArray(v.loc);glVertexAttribPointer(v.loc,v.n,v.type,v.norm,b->meshes[k].vertex_stride,(void*)v.offset);
        }
        glBindBuffer(GL_ARRAY_BUFFER,batch->instances);
        glEnableVertexAttribArray(4);glVertexAttribPointer(4,4,GL_FLOAT,GL_FALSE,sizeof(GPUInstance),(void*)offsetof(GPUInstance,x));glVertexAttribDivisor(4,1);
        glEnableVertexAttribArray(5);glVertexAttribPointer(5,1,GL_FLOAT,GL_FALSE,sizeof(GPUInstance),(void*)offsetof(GPUInstance,yaw));glVertexAttribDivisor(5,1);
        glEnableVertexAttribArray(11);glVertexAttribPointer(11,1,GL_FLOAT,GL_FALSE,sizeof(GPUInstance),(void*)offsetof(GPUInstance,fade));glVertexAttribDivisor(11,1);
    }
    glBindVertexArray(GLuint(previous_vao));glBindBuffer(GL_ARRAY_BUFFER,GLuint(previous_buffer));
    if(glGetError()!=GL_NO_ERROR){glDeleteVertexArrays((GLsizei)next.size(),next.data());return false;}
    if(!batch->vaos.empty())glDeleteVertexArrays((GLsizei)batch->vaos.size(),batch->vaos.data());
    batch->vaos.swap(next);batch->a=a;batch->b=b;batch->rigid_weapons.clear();return true;
}

namespace {
bool certify_weapon_part(const Mesh &ma,const Mesh &mb,unsigned first,unsigned count,const float pivot_a[3],const float pivot_b[3],RigidWeaponPart &out){
    const auto &a=ma.rigid_source,&b=mb.rigid_source;const auto &indices=ma.pose_indices;
    if(a.size()<3||a.size()!=b.size()||!count||first>indices.size()||count>indices.size()-first)return false;
    const unsigned x=indices[first];if(x>=a.size())return false;
    unsigned y=x,z=x;float far=0,area=0;
    // The arms bake retains some unused source vertices. Only indexed geometry
    // can be drawn, so those unrelated vertices must not define a weapon frame.
    for(unsigned at=first;at<first+count;++at){const unsigned i=indices[at];if(i>=a.size())return false;const auto v=world_rigid_weapon_pose::sub(a[i],a[x]);const float d=world_rigid_weapon_pose::dot(v,v);if(d>far){far=d;y=i;}}
    const auto edge=world_rigid_weapon_pose::sub(a[y],a[x]);
    for(unsigned at=first;at<first+count;++at){const unsigned i=indices[at];const auto v=world_rigid_weapon_pose::cross(edge,world_rigid_weapon_pose::sub(a[i],a[x]));const float d=world_rigid_weapon_pose::dot(v,v);if(d>area){area=d;z=i;}}
    WorldModelRigidWeaponPose pose={};
    if(!world_rigid_weapon_pose::rotation({a[x],a[y],a[z]},{b[x],b[y],b[z]},pose.rotation))return false;
    std::copy_n(pivot_a,3,pose.pivot_a);std::copy_n(pivot_b,3,pose.pivot_b);
    for(unsigned at=first;at<first+count;++at){const unsigned i=indices[at];
        const auto q=world_rigid_weapon_pose::interpolate(a[i],pivot_a,pivot_b,pose.rotation,1);
        const auto d=world_rigid_weapon_pose::sub(q,b[i]);if(world_rigid_weapon_pose::dot(d,d)>.002f*.002f)return false;
    }
    pose.enabled=true;out={pose,first,count};return true;
}
}
extern "C" bool world_model_pose_blend_rigid_weapon(WorldModelPoseBlend *batch,const float pivot_a[3],const float pivot_b[3],WorldModelRigidWeaponPose *result){
    if(result)*result={};
    if(!batch||!batch->a||!batch->b||batch->full_transform||!finite3(pivot_a)||!finite3(pivot_b))return false;
    std::vector<RigidWeaponMesh> accepted;
    try{accepted.resize(batch->a->meshes.size());}catch(...){return false;}
    bool any=false;
    for(unsigned mesh=0;mesh<accepted.size();++mesh){
        const auto &ma=batch->a->meshes[mesh],&mb=batch->b->meshes[mesh];auto &part=accepted[mesh];
        if(ma.rigid_source.empty()||mb.rigid_source.empty())continue;
        const auto *pa=ma.support_weapon&&batch->a->has_support_grip?batch->a->support_grip:pivot_a;
        const auto *pb=ma.support_weapon&&batch->b->has_support_grip?batch->b->support_grip:pivot_b;
        if(certify_weapon_part(ma,mb,0,unsigned(ma.pose_indices.size()),pa,pb,part.parts[0]))part.count=1;
        else if(mesh==0&&ma.pose_indices.size()==2952&&(ma.rigid_source.size()==786||ma.rigid_source.size()==821)&&batch->a->has_support_grip&&batch->b->has_support_grip){
            // Explicit owned sword/shield topology: first596 triangles are the
            // shield, remaining388 the sword. Strict pose index correspondence,
            // disjoint vertices and full rigid certification guard this split.
            constexpr unsigned shield=1788;bool disjoint=true;std::array<bool,821> used={};
            for(unsigned at=0;at<shield;++at)used[ma.pose_indices[at]]=true;
            for(unsigned at=shield;at<ma.pose_indices.size();++at)if(used[ma.pose_indices[at]])disjoint=false;
            if(disjoint&&certify_weapon_part(ma,mb,0,shield,batch->a->support_grip,batch->b->support_grip,part.parts[0])&&
               certify_weapon_part(ma,mb,shield,unsigned(ma.pose_indices.size())-shield,pivot_a,pivot_b,part.parts[1]))part.count=2;
        }
        if(!part.count)continue;
        // Socket/trail queries describe the sword (last part), while the shield
        // rotates about its independently authored left-hand leather grip.
        if(result&&!result->enabled&&!ma.support_weapon)*result=part.parts[part.count-1].pose;any=true;
    }
    if(any)batch->rigid_weapons.swap(accepted);return any;
}
extern "C" bool world_model_pose_blend_share_rigid(WorldModelPoseBlend *to,const WorldModelPoseBlend *from){
    if(!to||!from||to->full_transform||to->a!=from->a||to->b!=from->b||from->rigid_weapons.empty())return false;
    try{to->rigid_weapons=from->rigid_weapons;}catch(...){return false;}return true;
}
extern "C" void world_model_pose_release_rigid_source(WorldModelAsset *asset){
    if(asset)for(auto &m:asset->meshes)std::vector<world_rigid_weapon_pose::Point>().swap(m.rigid_source);
}

extern "C" bool world_model_pose_blend_enable_transform(WorldModelPoseBlend *b){
    if(!b||!b->a||!b->b||b->vaos.empty()||b->vertex_stream||b->pose_streams_attached||b->anatomy||b->character_buffer||!b->rigid_weapons.empty()||!b->subset_buffers.empty())return false;
    if(b->full_transform)return true;
    if(b->count)return false;
    try{b->transforms.reserve(b->capacity);}catch(...){return false;}
    GLint vao=0,buffer=0;glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&buffer);
    GLuint next=0;glGenBuffers(1,&next);glBindBuffer(GL_ARRAY_BUFFER,next);
    glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(b->capacity*5*sizeof(float)),nullptr,GL_DYNAMIC_DRAW);
    if(!next||glGetError()!=GL_NO_ERROR){glBindBuffer(GL_ARRAY_BUFFER,GLuint(buffer));glDeleteBuffers(1,&next);return false;}
    for(auto v:b->vaos){glBindVertexArray(v);
        glEnableVertexAttribArray(14);glVertexAttribPointer(14,2,GL_FLOAT,GL_FALSE,5*sizeof(float),nullptr);glVertexAttribDivisor(14,1);
        glEnableVertexAttribArray(15);glVertexAttribPointer(15,3,GL_FLOAT,GL_FALSE,5*sizeof(float),reinterpret_cast<void*>(2*sizeof(float)));glVertexAttribDivisor(15,1);
    }
    glBindVertexArray(GLuint(vao));glBindBuffer(GL_ARRAY_BUFFER,GLuint(buffer));
    if(glGetError()!=GL_NO_ERROR){for(auto v:b->vaos){glBindVertexArray(v);glDisableVertexAttribArray(14);glDisableVertexAttribArray(15);}glBindVertexArray(GLuint(vao));glDeleteBuffers(1,&next);return false;}
    b->transform_buffer=next;b->full_transform=true;return true;
}

extern "C" bool world_model_pose_blend_upload(WorldModelPoseBlend *batch,const WorldModelInstance *instances,const float *weights,size_t count){
    if(!batch||count>batch->capacity||(count&&(!instances||!weights)))return false;
    for(size_t k=0;k<count;k++){
        const auto &p=instances[k];
        if(!finite3(&p.x)||!std::isfinite(p.yaw)||!std::isfinite(p.scale)||p.scale<=0||
           !std::isfinite(weights[k])||weights[k]<0||weights[k]>1||!std::isfinite(p.wind_sway)||p.wind_sway<0||p.wind_sway>2||!std::isfinite(p.wind_flutter)||p.wind_flutter<0||p.wind_flutter>2)return false;
        if(batch->full_transform&&(!std::isfinite(p.pitch)||!std::isfinite(p.roll)||!std::isfinite(p.axis_x)||!std::isfinite(p.axis_y)||!std::isfinite(p.axis_z)||p.axis_x<=-1||p.axis_y<=-1||p.axis_z<=-1))return false;
    }
    if(!count){batch->count=0;batch->character_active=false;return true;}
    // Stage both endpoints before render passes. This also covers the next
    // source animation key; color/shadows then share the same ready buffers.
    if(!prepare_pose_geometry(batch->a,batch->b))return false;
    batch->staging.clear();
    for(size_t k=0;k<count;k++){
        const auto &p=instances[k];batch->staging.push_back({p.x,p.y,p.z,p.scale,p.yaw,weights[k],1.f,1.f});
    }
    if(!stream_instances(batch->instances,batch->staging.data(),count,batch->stream))return false;
    if(batch->full_transform){
        batch->transforms.clear();for(size_t k=0;k<count;++k){const auto &p=instances[k];batch->transforms.push_back({p.pitch,p.roll,1+p.axis_x,1+p.axis_y,1+p.axis_z});}
        GLint previous=0;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&previous);glBindBuffer(GL_ARRAY_BUFFER,batch->transform_buffer);
        glBufferSubData(GL_ARRAY_BUFFER,0,GLsizeiptr(count*5*sizeof(float)),batch->transforms.data());glBindBuffer(GL_ARRAY_BUFFER,GLuint(previous));
        if(glGetError()!=GL_NO_ERROR)return false;
    }
    batch->count=(GLsizei)count;batch->character_active=false;return true;
}

extern "C" WorldModelCharacterLook world_model_character_default(void){return {1,0,1,0,{1,1,1},0};}
extern "C" unsigned world_model_character_capabilities(const WorldModelAsset *asset){
    unsigned flags=0;if(asset)for(const auto &m:asset->meshes)if(m.character_region==1){flags|=WORLD_MODEL_CHARACTER_HAIR;if(m.cutoff>0)flags|=WORLD_MODEL_CHARACTER_MASKED_HAIR;}return flags;
}
extern "C" bool world_model_pose_blend_enable_character(WorldModelPoseBlend *b){
    if(!b||b->vaos.empty())return false;
    if(b->anatomy||b->full_transform)return false;
    if(b->character_buffer)return true;
    try{b->character_snapshot.reserve(b->capacity);}catch(...){return false;}
    GLint vao=0,buffer=0;glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&buffer);
    glGenBuffers(1,&b->character_buffer);glBindBuffer(GL_ARRAY_BUFFER,b->character_buffer);
    glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(b->capacity*sizeof(WorldModelCharacterLook)),nullptr,GL_DYNAMIC_DRAW);
    for(auto v:b->vaos){glBindVertexArray(v);for(unsigned i=0;i<2;i++){glEnableVertexAttribArray(12+i);glVertexAttribPointer(12+i,4,GL_FLOAT,GL_FALSE,sizeof(WorldModelCharacterLook),(void*)(std::size_t(i)*4*sizeof(float)));glVertexAttribDivisor(12+i,1);}}
    glBindVertexArray(GLuint(vao));glBindBuffer(GL_ARRAY_BUFFER,GLuint(buffer));return b->character_buffer&&glGetError()==GL_NO_ERROR;
}
extern "C" bool world_model_pose_blend_upload_character(WorldModelPoseBlend *b,const WorldModelInstance *instances,const float *weights,const WorldModelCharacterLook *looks,size_t count){
    if(!b||!b->character_buffer||count>b->capacity||(count&&(!looks||!instances||!weights)))return false;
    bool active=false;
    for(size_t i=0;i<count;i++){const auto &a=looks[i];
        if(!std::isfinite(a.width)||a.width<.65f||a.width>1.45f||!std::isfinite(a.cutoff_offset)||std::abs(a.cutoff_offset)>.3f||!std::isfinite(a.alpha_exponent)||a.alpha_exponent<.5f||a.alpha_exponent>2||!std::isfinite(a.color_strength)||a.color_strength<0||a.color_strength>1||!finite3(a.hair_tint)||!std::isfinite(a.hide_hair)||a.hide_hair<0||a.hide_hair>1)return false;
        for(float c:a.hair_tint)if(c<0||c>1)return false;
        active|=a.width!=1||a.cutoff_offset!=0||a.alpha_exponent!=1||a.color_strength!=0||a.hide_hair!=0;
    }
    if(!world_model_pose_blend_upload(b,instances,weights,count))return false;
    // Default crowd looks need only the original placement/pose upload. Do not
    // stream redundant appearance data or enable its shader branch each frame.
    if(!active)return true;
    const bool changed=count>b->character_snapshot.size()||std::memcmp(looks,b->character_snapshot.data(),count*sizeof(WorldModelCharacterLook))!=0;
    if(count&&changed){
        glBindBuffer(GL_ARRAY_BUFFER,b->character_buffer);glBufferSubData(GL_ARRAY_BUFFER,0,GLsizeiptr(count*sizeof(WorldModelCharacterLook)),looks);glBindBuffer(GL_ARRAY_BUFFER,0);if(glGetError()!=GL_NO_ERROR)return false;
        b->character_snapshot.assign(looks,looks+count);b->character_uploads++;
    }
    b->character_active=true;return true;
}
extern "C" uint64_t world_model_pose_blend_character_uploads(const WorldModelPoseBlend *b){return b?b->character_uploads:0;}

namespace {
bool upload_instances(WorldModelAsset *asset,const WorldModelInstance *instances,const float *fades,size_t count,bool fading){
    if(!asset||count>max_instances||(count&&(!instances||(fading&&!fades))))return false;
    // Validate the whole transaction before changing staging, GL or count.
    for(size_t i=0;i<count;i++){
        const WorldModelInstance &p=instances[i];
        if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||!std::isfinite(p.yaw)||!std::isfinite(p.scale)||p.scale<=0||!std::isfinite(p.pitch)||!std::isfinite(p.roll)||!std::isfinite(p.axis_x)||!std::isfinite(p.axis_y)||!std::isfinite(p.axis_z)||p.axis_x<=-1||p.axis_y<=-1||p.axis_z<=-1||!std::isfinite(p.wind_sway)||p.wind_sway<0||p.wind_sway>2||!std::isfinite(p.wind_flutter)||p.wind_flutter<0||p.wind_flutter>2)return false;
        if(fading&&(!std::isfinite(fades[i])||fades[i]<0||fades[i]>1))return false;
    }
    bool extended=false;for(size_t i=0;i<count;i++){const auto &p=instances[i];extended|=p.pitch!=0||p.roll!=0||p.axis_x!=0||p.axis_y!=0||p.axis_z!=0;}
    if(extended){
        if(!asset->transform_buffer){asset->transforms.reserve(max_instances);glGenBuffers(1,&asset->transform_buffer);if(!asset->transform_buffer)return false;for(const auto &mesh:asset->meshes){glBindVertexArray(mesh.vao);glBindBuffer(GL_ARRAY_BUFFER,asset->transform_buffer);glEnableVertexAttribArray(14);glVertexAttribPointer(14,2,GL_FLOAT,GL_FALSE,5*sizeof(float),nullptr);glVertexAttribDivisor(14,1);glEnableVertexAttribArray(15);glVertexAttribPointer(15,3,GL_FLOAT,GL_FALSE,5*sizeof(float),reinterpret_cast<void *>(2*sizeof(float)));glVertexAttribDivisor(15,1);}glBindVertexArray(0);}
        asset->transforms.clear();for(size_t i=0;i<count;i++){const auto &p=instances[i];asset->transforms.push_back({p.pitch,p.roll,1+p.axis_x,1+p.axis_y,1+p.axis_z});}
        GLint previous=0;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&previous);glBindBuffer(GL_ARRAY_BUFFER,asset->transform_buffer);glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(count*5*sizeof(float)),asset->transforms.data(),GL_DYNAMIC_DRAW);glBindBuffer(GL_ARRAY_BUFFER,GLuint(previous));
    }
    asset->staging.clear();
    if(!count){asset->count=0;asset->has_fades=false;return true;}
    // The full bounded capacity is reserved at load, including the first
    // upload. Visible-prefix reordering can refill this every frame safely.
    for(size_t i=0;i<count;i++){
        const WorldModelInstance &p=instances[i];asset->staging.push_back({p.x,p.y,p.z,p.scale,p.yaw,fading?fades[i]:1.f,p.wind_sway,p.wind_flutter});
    }
    if(!stream_instances(asset->instance_buffer,asset->staging.data(),count,asset->stream))return false;
    asset->count=(GLsizei)count;asset->has_fades=fading;asset->full_transform=extended;return true;
}
}

extern "C" bool world_model_upload_instances(WorldModelAsset *asset,const WorldModelInstance *instances,size_t count){
    return upload_instances(asset,instances,nullptr,count,false);
}
extern "C" bool world_model_upload_fading_instances(WorldModelAsset *asset,const WorldModelInstance *instances,const float *fades,size_t count){
    return upload_instances(asset,instances,fades,count,true);
}

extern "C" void world_model_draw(WorldModelAsset *asset,unsigned program){
    world_model_draw_instances(asset,program,asset?(size_t)asset->count:0);
}

#include "engine/world_model_pose_pnt.inc"
namespace {
unsigned anatomy_source_part(unsigned part,bool mirror){return mirror?(part==3?4:part==4?3:part==5?6:part==6?5:part==7?8:part==8?7:part):part;}
bool anatomy_cap_selected(std::size_t mesh,const WorldModelAnatomyDraw &d,float &side){
 const unsigned actual=anatomy_source_part(unsigned(mesh)+1,d.mirrored),keep=d.keep_part;
 const bool descendant=keep==2?actual<=6:keep==3?actual==5:keep==4?actual==6:false;
 if((keep&&actual!=keep&&!(descendant&&(d.removed&(1u<<(actual-1)))))||(!keep&&!(d.removed&(1u<<(actual-1)))))return false;
 side=keep&&actual==keep?-1.f:1.f;return true;
}
struct SupportVertex {float x=0,y=0,z=0;std::array<float,8> regions={};};
}
static bool capture_support_pnt3(const WorldModelPoseBlend *b,const WorldModelAnatomyDraw *d,const WorldModelVertexOverride *posed,size_t vertices,WorldModelSupportPoint3Fn emit,void *ctx){
 if(!b||!b->a||!b->b||!d||!emit||(!d->caps&&!b->anatomy)||!std::isfinite(d->weight)||d->weight<0||d->weight>1||d->keep_part>8||d->removed>255||!world_model_variants::valid_selection(d->variant_mask))return false;
 if(posed&&vertices!=world_model_pose_vertices(b->a))return false;
 const float w=d->weight;std::size_t emitted=0,offset=0;constexpr std::size_t limit=240000;
 for(std::size_t m=0;m<b->a->meshes.size();++m){const auto &a=b->a->meshes[m],&z=b->b->meshes[m];
  const size_t base=offset;offset+=a.cut_support_xyz.size();
  if(a.cut_support_xyz.empty()||a.cut_support_xyz.size()!=z.cut_support_xyz.size())return false;
  float side=1;if(d->caps&&!anatomy_cap_selected(m,*d,side))continue;if(!d->caps&&d->hide_weapon&&a.held_weapon)continue;
  if(!world_model_equipment::visible(a.equipment_mask,world_model_equipment::standard))continue;
  if(!world_model_variants::visible(a.variant_mask,d->variant_mask))continue;
  if(d->caps){for(auto id:a.pose_indices){if(id>=a.cut_support_xyz.size()||++emitted>limit)return false;const auto &x=a.cut_support_xyz[id],&y=z.cut_support_xyz[id];const auto *q=posed?posed[base+id].position:nullptr;if(!emit(q?q[0]:x[0]+(y[0]-x[0])*w,q?q[1]:x[1]+(y[1]-x[1])*w,q?q[2]:x[2]+(y[2]-x[2])*w,ctx))return false;}continue;}
  if(m>=b->anatomy->offsets.size())return false;
  for(std::size_t tri=0;tri<a.pose_indices.size();tri+=3){std::array<SupportVertex,12> polygon={},scratch={};unsigned count=3;
   for(unsigned i=0;i<3;++i){const auto id=a.pose_indices[tri+i];if(id>=a.cut_support_xyz.size())return false;const auto &x=a.cut_support_xyz[id],&y=z.cut_support_xyz[id];auto &v=polygon[i];const auto *q=posed?posed[base+id].position:nullptr;v.x=q?q[0]:x[0]+(y[0]-x[0])*w;v.y=q?q[1]:x[1]+(y[1]-x[1])*w;v.z=q?q[2]:x[2]+(y[2]-x[2])*w;
    const auto &weights=b->anatomy->weights[b->anatomy->offsets[m]+id];for(unsigned part=1;part<=8;++part)v.regions[part-1]=weights[anatomy_source_part(part,d->mirrored)-1];
   }
   for(unsigned part=1;part<=8&&count;++part){const bool keep=d->keep_part==part;if(!keep&&!(d->removed&(1u<<(part-1))))continue;unsigned next=0;
    auto distance=[&](const SupportVertex &v){return keep?v.regions[part-1]-.5f:.5f-v.regions[part-1];};
    for(unsigned i=0;i<count;++i){const auto &x=polygon[i],&y=polygon[(i+1)%count];const float dx=distance(x),dy=distance(y);if(dx>=0){if(next>=scratch.size())return false;scratch[next++]=x;}
     if((dx>=0)!=(dy>=0)){if(next>=scratch.size())return false;const float t=dx/(dx-dy);auto &v=scratch[next++];v.x=x.x+(y.x-x.x)*t;v.y=x.y+(y.y-x.y)*t;v.z=x.z+(y.z-x.z)*t;for(unsigned k=0;k<8;++k)v.regions[k]=x.regions[k]+(y.regions[k]-x.regions[k])*t;}
    }count=next;polygon=scratch;
   }
   if(count<3)continue;for(unsigned i=0;i<count;++i)if(++emitted>limit||!emit(polygon[i].x,polygon[i].y,polygon[i].z,ctx))return false;
  }
 }
 return true;
}

extern "C" bool world_model_pose_blend_capture_support3(const WorldModelPoseBlend *b,const WorldModelAnatomyDraw *d,WorldModelSupportPoint3Fn emit,void *ctx){return capture_support_pnt3(b,d,nullptr,0,emit,ctx);}
extern "C" bool world_model_pose_blend_capture_support_pnt3(const WorldModelPoseBlend *b,const WorldModelAnatomyDraw *d,const WorldModelVertexOverride *posed,size_t vertices,WorldModelSupportPoint3Fn emit,void *ctx){return posed&&capture_support_pnt3(b,d,posed,vertices,emit,ctx);}
extern "C" bool world_model_pose_blend_capture_support(const WorldModelPoseBlend *b,const WorldModelAnatomyDraw *d,WorldModelSupportPointFn emit,void *ctx){
 if(!emit)return false;struct Adapter{WorldModelSupportPointFn callback;void *context;} adapter={emit,ctx};
 return world_model_pose_blend_capture_support3(b,d,[](float,float y,float z,void *p){auto &a=*static_cast<Adapter*>(p);return a.callback(y,z,a.context);},&adapter);
}

static void draw_geometry(WorldModelAsset *asset,unsigned program,GLsizei count,
                          const std::vector<GLuint> *pose_vaos,bool fading,bool character=false,
                          const std::vector<GLuint> *static_vaos=nullptr,bool static_full_transform=false,unsigned equipment=world_model_equipment::standard,bool vertex_override=false,const std::vector<RigidWeaponMesh> *rigid=nullptr,const WorldModelAnatomyDraw *anatomy=nullptr,const std::vector<GLsizei> *subset_counts=nullptr,WorldModelAsset *pose_target=nullptr){
    if(!asset||!program||!count||!prepare_pose_geometry(asset,pose_target))return;
    const Locations *cached=nullptr;
    for(const ProgramLocations &entry:asset->programs)
        if(entry.program==program&&entry.epoch==world_program_epoch){cached=&entry.uniforms;break;}
    if(!cached){
        ProgramLocations &entry=asset->programs[asset->next_program];
        entry.program=program;entry.epoch=world_program_epoch;entry.uniforms=locations(program);cached=&entry.uniforms;
        asset->next_program=(asset->next_program+1)%asset->programs.size();
    }
    const Locations &u=*cached;
    if(pose_vaos&&u.pose_blend<0)return;
    const bool depth_only=program==depth_material_program;
    glUseProgram(program);
    glUniform1i(u.base,0);glUniform1i(u.normal,1);glUniform1i(u.arm,2);
    glUniform1i(u.emissive,3);
    glUniform1i(u.instance_fade,fading);glUniform1i(u.pose_blend,pose_vaos!=nullptr);
    if(u.vertex_override>=0)glUniform1i(u.vertex_override,vertex_override&&pose_vaos);
    if(u.full_transform>=0)glUniform1i(u.full_transform,(static_vaos||pose_vaos)?static_full_transform:asset->full_transform);
    if(u.character_enabled>=0)glUniform1i(u.character_enabled,character);
    if((pose_vaos||character)&&u.item_wind>=0)glUniform1i(u.item_wind,0);
    for(int j=0;j<3;j++)glUniform1i(u.facade_aux[j],8+j);
    GLuint bound_samplers[4]={0,0,0,0};const unsigned sampler_units[4]={0,8,9,10};
    GLint old_front=GL_CCW;std::array<GLboolean,8> old_clip={};
    if(anatomy){
        if(u.anatomy<0||!pose_vaos)return;
        glUniform1i(u.anatomy,1);glUniform1i(u.anatomy_mirror,anatomy->mirrored);glUniform1i(u.anatomy_caps,anatomy->caps);glUniform1i(u.anatomy_mask,int(anatomy->removed));glUniform1i(u.anatomy_keep,int(anatomy->keep_part));
        glUniform4f(u.anatomy_instance,anatomy->x,anatomy->y,anatomy->z,anatomy->scale);glUniform2f(u.anatomy_yaw_weight,anatomy->yaw,anatomy->weight);
        glUniform3fv(u.anatomy_pivot,1,anatomy->pivot);glUniform4fv(u.anatomy_rotation,1,anatomy->rotation);glUniform1f(u.anatomy_cap_side,anatomy->cap_side);
        glGetIntegerv(GL_FRONT_FACE,&old_front);glFrontFace((anatomy->mirrored!=(anatomy->caps&&anatomy->cap_side<0))?GL_CW:GL_CCW);
        for(unsigned j=0;j<8;++j){old_clip[j]=glIsEnabled(GL_CLIP_DISTANCE0+j);if(!anatomy->caps)glEnable(GL_CLIP_DISTANCE0+j);}
    }
    size_t mesh_index=0;
    // Every model draw leaves this opt-in shader branch false. Track only
    // transitions within this call: static meshes need no redundant writes.
    bool rigid_enabled=false;
    for(const Mesh &mesh:asset->meshes){
        const GLsizei element_count=subset_counts&&mesh_index<subset_counts->size()?(*subset_counts)[mesh_index]:mesh.indices;
        if(!element_count){++mesh_index;continue;}
        if(anatomy){
            if(anatomy->caps){float side=1;if(!anatomy_cap_selected(mesh_index,*anatomy,side)){++mesh_index;continue;}glUniform1f(u.anatomy_cap_side,side);glFrontFace((anatomy->mirrored!=(side<0))?GL_CW:GL_CCW);
            }else if(anatomy->hide_weapon&&mesh.held_weapon){++mesh_index;continue;}
        }
        if(!world_model_equipment::visible(mesh.equipment_mask,equipment)||!world_model_variants::visible(mesh.variant_mask,anatomy?anatomy->variant_mask:0)){++mesh_index;continue;}
        const auto *rigid_mesh=pose_vaos&&rigid&&mesh_index<rigid->size()?&(*rigid)[mesh_index]:nullptr;
        const bool use_rigid=rigid_mesh&&rigid_mesh->count;
        if(use_rigid!=rigid_enabled){if(u.rigid_weapon>=0)glUniform1i(u.rigid_weapon,use_rigid);rigid_enabled=use_rigid;}
        glUniform1i(u.baked_light,mesh.baked_light&&!pose_vaos&&!character);
        glUniform1i(u.source_paint,mesh.source_paint&&!pose_vaos&&!character);
        if(u.character_region>=0)glUniform1i(u.character_region,character?GLint(mesh.character_region):0);
        if(u.skin_material>=0)glUniform1i(u.skin_material,!depth_only&&mesh.skin_material);
        if(u.organic_body>=0)glUniform1i(u.organic_body,!depth_only&&mesh.organic_body);
        // Source-blend hair needs its opacity contract in shadow draws too.
        if(u.hair_material>=0)glUniform1i(u.hair_material,GLint(mesh.hair_material));
        // Uniform branches cannot remove a sampler's BO from driver validation.
        // Do not make an opaque wall's color atlas resident just to draw depth.
        // Cutout coverage, LOD/fade, geometry and all original color draws remain
        // identical; the shader already returns before reading this texel.
        const bool neutral_depth=depth_only&&mesh.cutoff<=0.f;
        for(unsigned j=0;j<4;j++){const GLuint next=depth_only?0:mesh.detail_samplers[j];if(next!=bound_samplers[j]){glBindSampler(sampler_units[j],next);bound_samplers[j]=next;}}
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,!neutral_depth&&mesh.base?mesh.base:asset->neutral[0]);
        if(depth_only){if(neutral_depth)depth_material_stats.opaque_draws++;else depth_material_stats.masked_draws++;}
        if(u.normal>=0){glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,mesh.normal?mesh.normal:asset->neutral[2]);}
        if(u.arm>=0){glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,mesh.arm?mesh.arm:asset->neutral[1]);}
        if(u.emissive>=0){glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,mesh.emissive?mesh.emissive:asset->neutral[0]);}
        glUniform1i(u.has_emissive,mesh.emissive!=0);glUniform3fv(u.emissive_factor,1,mesh.emissive_factor);
        glUniform1i(u.facade_mode,mesh.facade.mode);glUniform3fv(u.facade_scale,1,mesh.facade.scale);
        for(int j=0;j<3;j++)if(u.facade_aux[j]>=0){
            const GLuint fallback=asset->neutral[mesh.facade.mode==1||j==2?0:(j==0?2:1)];
            glActiveTexture(GL_TEXTURE8+j);glBindTexture(GL_TEXTURE_2D,mesh.facade_aux[j]?mesh.facade_aux[j]:fallback);
        }
        glUniform1i(u.has_base,mesh.base!=0);glUniform1i(u.has_normal,mesh.normal!=0);glUniform1i(u.has_arm,mesh.arm!=0);
        glUniform1f(u.normal_scale,mesh.normal_scale);
        glUniform3fv(u.winter,1,mesh.winter);
        glUniform4fv(u.leaf_transmission,1,mesh.leaf_transmission);
        glUniform4fv(u.factor,1,mesh.factor);glUniform1f(u.roughness,mesh.roughness);glUniform1f(u.metallic,mesh.metallic);glUniform1f(u.cutoff,mesh.cutoff);
        glBindVertexArray(pose_vaos?(*pose_vaos)[mesh_index]:(static_vaos?(*static_vaos)[mesh_index]:mesh.vao));
        if(rigid_mesh&&rigid_mesh->count){
            for(unsigned j=0;j<rigid_mesh->count;++j){const auto &part=rigid_mesh->parts[j];
                glUniform4fv(u.rigid_rotation,1,part.pose.rotation);glUniform3fv(u.rigid_pivot_a,1,part.pose.pivot_a);glUniform3fv(u.rigid_pivot_b,1,part.pose.pivot_b);
                glDrawElementsInstanced(GL_TRIANGLES,GLsizei(part.index_count),GL_UNSIGNED_INT,reinterpret_cast<const void*>(std::uintptr_t(part.first_index)*sizeof(std::uint32_t)),count);
                if(u.rigid_weapon>=0)++rigid_weapon_draws;
            }
        }else glDrawElementsInstanced(GL_TRIANGLES,element_count,GL_UNSIGNED_INT,nullptr,count);
        mesh_index++;
    }
    for(unsigned j=0;j<4;j++)if(bound_samplers[j])glBindSampler(sampler_units[j],0);
    // Procedural dressing can use this program after imported models, even
    // in the following frame. Never leak a source layer into those draws.
    glUniform1i(u.facade_mode,0);
    glUniform1i(u.baked_light,0);
    glUniform1i(u.source_paint,0);
    glUniform3f(u.winter,0,0,0);
    glUniform4f(u.leaf_transmission,0,0,0,-1);
    glUniform1i(u.pose_blend,0);if(u.full_transform>=0)glUniform1i(u.full_transform,0);
    if(u.vertex_override>=0)glUniform1i(u.vertex_override,0);
    if(rigid_enabled&&u.rigid_weapon>=0)glUniform1i(u.rigid_weapon,0);
    if(anatomy){glUniform1i(u.anatomy,0);glFrontFace(GLenum(old_front));for(unsigned j=0;j<8;++j)if(!old_clip[j])glDisable(GL_CLIP_DISTANCE0+j);}
    if(u.character_enabled>=0)glUniform1i(u.character_enabled,0);
    if(u.item_wind>=0)glUniform1i(u.item_wind,0);
    if(u.character_region>=0)glUniform1i(u.character_region,0);
    if(u.skin_material>=0)glUniform1i(u.skin_material,0);
    if(u.organic_body>=0)glUniform1i(u.organic_body,0);
    if(u.hair_material>=0)glUniform1i(u.hair_material,0);
    glUniform1i(u.has_emissive,0);glUniform3f(u.emissive_factor,0,0,0);
    glBindVertexArray(0);
}

extern "C" void world_model_draw_instances(WorldModelAsset *asset,unsigned program,size_t prefix_count){
    if(!asset)return;
    draw_geometry(asset,program,(GLsizei)std::min(prefix_count,(size_t)asset->count),nullptr,asset->has_fades);
}
extern "C" void world_model_instance_batch_draw(WorldModelInstanceBatch *batch,unsigned program){
    if(batch&&batch->asset)draw_geometry(batch->asset,program,batch->count,nullptr,false,false,&batch->vaos,batch->full_transform);
}
extern "C" void world_model_pose_blend_draw_instances(WorldModelPoseBlend *batch,unsigned program,size_t prefix_count){
    if(!batch||!batch->a||!batch->b||batch->vaos.empty())return;
    draw_geometry(batch->a,program,(GLsizei)std::min(prefix_count,(size_t)batch->count),&batch->vaos,false,batch->character_active,nullptr,batch->full_transform,world_model_equipment::standard,false,&batch->rigid_weapons,nullptr,batch->subset_counts.empty()?nullptr:&batch->subset_counts,batch->b);
}
extern "C" void world_model_pose_blend_draw_equipped(WorldModelPoseBlend *batch,unsigned program,unsigned mask,size_t prefix_count){
    if(!batch||!batch->a||!batch->b||batch->vaos.empty()||mask>world_model_equipment::all)return;
    draw_geometry(batch->a,program,(GLsizei)std::min(prefix_count,(size_t)batch->count),&batch->vaos,false,batch->character_active,nullptr,batch->full_transform,mask,false,&batch->rigid_weapons,nullptr,batch->subset_counts.empty()?nullptr:&batch->subset_counts,batch->b);
}
extern "C" void world_model_pose_blend_draw_equipped_deformed(WorldModelPoseBlend *batch,unsigned program,unsigned mask,size_t prefix_count){
    if(!batch||!batch->a||!batch->b||batch->vaos.empty()||mask>world_model_equipment::all)return;
    draw_geometry(batch->a,program,(GLsizei)std::min(prefix_count,(size_t)batch->count),&batch->vaos,false,batch->character_active,nullptr,false,mask,batch->vertex_stream!=nullptr,&batch->rigid_weapons,nullptr,batch->subset_counts.empty()?nullptr:&batch->subset_counts,batch->b);
}
extern "C" void world_model_pose_blend_draw_anatomy(WorldModelPoseBlend *b,unsigned program,const WorldModelAnatomyDraw *d){
    if(!b||!b->a||!d||(!d->caps&&!b->anatomy)||d->keep_part>8||d->removed>255||!std::isfinite(d->weight)||d->weight<0||d->weight>1||!std::isfinite(d->scale)||d->scale<=0||!world_model_variants::valid_selection(d->variant_mask))return;
    draw_geometry(b->a,program,1,&b->vaos,false,false,nullptr,false,world_model_equipment::standard,false,&b->rigid_weapons,d,b->subset_counts.empty()?nullptr:&b->subset_counts,b->b);
}
extern "C" void world_model_pose_blend_draw_anatomy_deformed(WorldModelPoseBlend *b,unsigned program,const WorldModelAnatomyDraw *d){
    if(!b||!b->a||!b->vertex_stream||!d||(!d->caps&&!b->anatomy)||d->keep_part>8||d->removed>255||!std::isfinite(d->weight)||d->weight<0||d->weight>1||!std::isfinite(d->scale)||d->scale<=0||!world_model_variants::valid_selection(d->variant_mask))return;
    draw_geometry(b->a,program,1,&b->vaos,false,false,nullptr,false,world_model_equipment::standard,true,&b->rigid_weapons,d,b->subset_counts.empty()?nullptr:&b->subset_counts,b->b);
}
extern "C" void world_model_pose_blend_draw(WorldModelPoseBlend *batch,unsigned program){
    world_model_pose_blend_draw_instances(batch,program,batch?(size_t)batch->count:0);
}

extern "C" uint64_t world_model_rigid_weapon_draws(void){return rigid_weapon_draws;}

extern "C" bool world_model_bounds(const WorldModelAsset *asset,float minimum[3],float maximum[3]){
    if(!asset)return false;
    if(minimum)std::copy(asset->minimum,asset->minimum+3,minimum);
    if(maximum)std::copy(asset->maximum,asset->maximum+3,maximum);
    return true;
}
extern "C" bool world_model_plain_materials(const WorldModelAsset *asset){
    if(!asset||asset->pose_asset)return false;
    return std::all_of(asset->meshes.begin(),asset->meshes.end(),
        [](const Mesh &mesh){return mesh.facade.mode==0;});
}
extern "C" size_t world_model_instance_count(const WorldModelAsset *asset){return asset?(size_t)asset->count:0;}

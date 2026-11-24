#include "engine/world_model_cpu.h"
#include "cgltf.h"
#include "stb_image.h"
#include "engine/world_model_facade.h"
#include "engine/world_model_baked_light.h"
#include "engine/world_model_source_paint.h"
#include "engine/world_model_lava_heat.h"
#include "engine/world_texture_blocks_format.h"
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <sys/stat.h>
namespace world_model_cpu {
bool Options::cancelled()const{return(stopping&&stopping->load(std::memory_order_acquire))||(generation&&generation->load(std::memory_order_acquire)!=expected_generation);}
namespace {
constexpr std::uint64_t max_file_bytes=128ull*1024*1024;
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
static void complete_basis(Mesh &mesh,bool need_normals,bool need_tangents,
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
    (void)repaired;(void)mixed;(void)degenerate;
}
struct Budget {
    std::uint64_t used=0,limit=0;
    void take(std::uint64_t bytes){if(bytes>limit-used)throw "CPU preparation budget exceeded";used+=bytes;}
    void release(std::uint64_t bytes){if(bytes>used)throw "CPU preparation reservation underflow";used-=bytes;}
};
std::uint64_t hash(const void *data,std::size_t bytes){const auto *p=static_cast<const unsigned char *>(data);std::uint64_t h=1469598103934665603ull;while(bytes--){h^=*p++;h*=1099511628211ull;}return h;}
float linear(unsigned char value){const float x=value/255.f;return x<=.04045f?x/12.92f:std::pow((x+.055f)/1.055f,2.4f);}
unsigned char encoded(float value){const float x=value<=.0031308f?12.92f*value:1.055f*std::pow(value,1.f/2.4f)-.055f;return static_cast<unsigned char>(std::lround(std::max(0.f,std::min(1.f,x))*255));}
void mipmaps(Texture &texture,const Options &options){
    while(texture.levels.back().width>1||texture.levels.back().height>1){
        if(options.cancelled())throw "Model preparation cancelled";
        const Level &from=texture.levels.back();Level to;to.width=std::max(1u,from.width/2);to.height=std::max(1u,from.height/2);to.rgba.resize(std::size_t(to.width)*to.height*4);
        for(unsigned y=0;y<to.height;y++)for(unsigned x=0;x<to.width;x++){
            // Area averaging includes the final column/row of odd images.
            const unsigned x0=x*from.width/to.width,x1=(x+1)*from.width/to.width;
            const unsigned y0=y*from.height/to.height,y1=(y+1)*from.height/to.height;
            for(unsigned c=0;c<4;c++){float sum=0;unsigned count=0;for(unsigned sy=y0;sy<y1;sy++)for(unsigned sx=x0;sx<x1;sx++){const auto v=from.rgba[(std::size_t(sy)*from.width+sx)*4+c];sum+=(texture.srgb&&c<3)?linear(v):v/255.f;count++;}sum/=count;to.rgba[(std::size_t(y)*to.width+x)*4+c]=(texture.srgb&&c<3)?encoded(sum):static_cast<unsigned char>(std::lround(sum*255));}
        }
        texture.levels.push_back(std::move(to));
    }
}
thread_local std::string block_directory;
int texture(Asset &asset,const cgltf_texture_view &view,bool srgb,bool opaque,Budget &budget,const Options &options,unsigned data_format=0){
    if(!view.texture)return -1;
    const auto *image=view.texture->image;
    if(!image||!image->buffer_view||view.texcoord!=0||view.has_transform||image->uri)throw "Expected embedded UV0 material image";
    const auto *bytes=cgltf_buffer_view_data(image->buffer_view);const auto length=image->buffer_view->size;
    if(!bytes||length<4||length>64ull*1024*1024)throw "Invalid material image length";
    Texture item;item.srgb=srgb;item.opaque=opaque;item.bc7_policy=options.bc7&&((srgb&&opaque)||(!srgb&&world_texture_blocks::linear_format(data_format)));item.bc7_format=item.bc7_policy?(srgb?world_texture_blocks::format_srgb_bc7:data_format):0;
    if(const auto *sampler=view.texture->sampler){if(sampler->min_filter)item.min_filter=sampler->min_filter;if(sampler->mag_filter)item.mag_filter=sampler->mag_filter;if(sampler->wrap_s)item.wrap_s=sampler->wrap_s;if(sampler->wrap_t)item.wrap_t=sampler->wrap_t;}
    item.fingerprint=hash(bytes,length);
    for(unsigned i=0;i<asset.textures.size();i++){const auto &t=asset.textures[i];if(t.fingerprint==item.fingerprint&&t.srgb==srgb&&t.opaque==opaque&&t.bc7_format==item.bc7_format&&t.min_filter==item.min_filter&&t.mag_filter==item.mag_filter&&t.wrap_s==item.wrap_s&&t.wrap_t==item.wrap_t&&t.encoded.size()==length&&!std::memcmp(t.encoded.data(),bytes,length))return int(i);}
    int width=0,height=0,channels=0;if(!stbi_info_from_memory(bytes,int(length),&width,&height,&channels)||width<=0||height<=0||unsigned(width)>options.max_texture_size||unsigned(height)>options.max_texture_size||std::uint64_t(width)*height>64ull*1024*1024)throw "Material image dimension budget exceeded";
    if(item.bc7_policy){
        char leaf[40];std::snprintf(leaf,sizeof(leaf),srgb?"%016llx.vbc7":"%016llx.linear.vbc7",static_cast<unsigned long long>(world_texture_blocks::fingerprint(bytes,length)));
        world_texture_blocks::Image blocks;std::string ignored;
        const auto cache=std::filesystem::path(block_directory)/leaf;
        std::error_code ec;const auto cache_bytes=std::filesystem::file_size(cache,ec);
        if(!ec&&cache_bytes<=(budget.limit-budget.used)/2&&world_texture_blocks::read(cache.u8string().c_str(),bytes,length,blocks,ignored,item.bc7_format)&&blocks.width==unsigned(width)&&blocks.height==unsigned(height)&&!blocks.levels.empty()&&blocks.levels.front().rmse<=world_texture_blocks::base_rmse_max){
            std::uint64_t block_bytes=0;for(const auto &level:blocks.levels)block_bytes+=level.blocks.size();budget.take(length*2+block_bytes);item.encoded.assign(bytes,bytes+length);item.cache_key=std::make_shared<CacheKey>();item.cache_key->prepared=texture_cache_prepare(bytes,length);if(!item.cache_key->prepared)throw "Texture cache key allocation failed";item.compressed=true;item.levels.reserve(blocks.levels.size());for(auto &level:blocks.levels)item.levels.push_back({level.width,level.height,std::move(level.blocks)});asset.texture_bytes+=block_bytes;asset.textures.push_back(std::move(item));return int(asset.textures.size()-1);
        }
    }
    std::uint64_t mip_bytes=0;for(unsigned w=unsigned(width),h=unsigned(height);;w=std::max(1u,w/2),h=std::max(1u,h/2)){mip_bytes+=std::uint64_t(w)*h*4;if(w==1&&h==1)break;}
    // Reserve the retained chain, encoded key and a conservative decoder scratch
    // image before decoding. Ordinary loaders retain their conservative total;
    // large source batches may reuse scratch only after the decoder is freed.
    budget.take(length*2+mip_bytes+std::uint64_t(width)*height*8);
    if(options.cancelled())throw "Model preparation cancelled";
    unsigned char *pixels=stbi_load_from_memory(bytes,int(length),&width,&height,&channels,4);if(!pixels)throw "Material image decode failed";
    std::unique_ptr<unsigned char,void(*)(void*)> owned(pixels,stbi_image_free);
    Level first;first.width=unsigned(width);first.height=unsigned(height);first.rgba.assign(pixels,pixels+std::size_t(width)*height*4);
    item.encoded.assign(bytes,bytes+length);item.cache_key=std::make_shared<CacheKey>();item.cache_key->prepared=texture_cache_prepare(bytes,length);if(!item.cache_key->prepared)throw "Texture cache key allocation failed";
    item.levels.reserve(16);item.levels.push_back(std::move(first));owned.reset();
    if(options.reuse_decoder_scratch)budget.release(std::uint64_t(width)*height*8);
    mipmaps(item,options);
    asset.texture_bytes+=mip_bytes;asset.textures.push_back(std::move(item));return int(asset.textures.size()-1);
}
}
bool bounds_only(const char *path,const Options &options,Bounds &result,std::string &error){
    error.clear();cgltf_data *data=nullptr;
    try{
        struct stat info={};if(!path||stat(path,&info)||info.st_size<20||std::uint64_t(info.st_size)>max_file_bytes)throw "Invalid static GLB file size";
        if(std::uint64_t(info.st_size)>options.cpu_bytes/2)throw "CPU bounds preparation budget exceeded";
        if(options.cancelled())throw "Model preparation cancelled";
        cgltf_options parser={};if(cgltf_parse_file(&parser,path,&data)!=cgltf_result_success)throw "Invalid glTF data";
        if(data->file_type!=cgltf_file_type_glb||data->nodes_count>512||data->images_count>128||data->skins_count)throw "Expected bounded static GLB";
        std::size_t declared=0;for(std::size_t i=0;i<data->buffers_count;i++){if(data->buffers[i].uri||data->buffers[i].size>max_file_bytes-declared)throw "Expected bounded embedded buffers";declared+=data->buffers[i].size;}
        if(cgltf_load_buffers(&parser,data,path)!=cgltf_result_success||cgltf_validate(data)!=cgltf_result_success)throw "Invalid glTF buffers";
        for(std::size_t i=0;i<data->nodes_count;i++){const auto *node=&data->nodes[i];std::size_t depth=0;while(node&&depth++<=data->nodes_count)node=node->parent;if(node)throw "Cyclic node parents";}
        Bounds bounds;for(int i=0;i<3;i++){bounds.minimum[i]=std::numeric_limits<float>::max();bounds.maximum[i]=-bounds.minimum[i];}
        std::size_t vertices=0,indices=0,meshes=0;
        for(std::size_t ni=0;ni<data->nodes_count;ni++){
            const auto *node=&data->nodes[ni];if(!node->mesh||!in_scene(data,node))continue;float matrix[16],normal[9],sign;cgltf_node_transform_world(node,matrix);for(float v:matrix)if(!std::isfinite(v))throw "Non-finite node transform";if(!normal_matrix(matrix,normal,sign))throw "Singular node transform";
            for(std::size_t pi=0;pi<node->mesh->primitives_count;pi++){
                const auto &primitive=node->mesh->primitives[pi];const auto *position=attribute(primitive,cgltf_attribute_type_position);if(primitive.type!=cgltf_primitive_type_triangles||primitive.has_draco_mesh_compression||!position||!position->count||++meshes>128)throw "Unsupported primitive or mesh budget";const auto count=primitive.indices?primitive.indices->count:position->count;
                if(!count||count%3||position->count>1000000-vertices||count>3000000-indices)throw "Geometry budget exceeded";
                vertices+=position->count;indices+=count;
                for(std::size_t vi=0;vi<position->count;vi++){if((vi&1023)==0&&options.cancelled())throw "Model preparation cancelled";float p[3],world[3];if(!cgltf_accessor_read_float(position,vi,p,3)||!finite3(p))throw "Invalid position";transform_direction(matrix,p,world);for(int k=0;k<3;k++)world[k]+=matrix[12+k];if(!finite3(world))throw "Invalid transformed position";for(int k=0;k<3;k++){bounds.minimum[k]=std::min(bounds.minimum[k],world[k]);bounds.maximum[k]=std::max(bounds.maximum[k],world[k]);}}
            }
        }
        if(!meshes)throw "No triangle geometry";
        bounds.maximum[1]-=bounds.minimum[1];bounds.minimum[1]=0;cgltf_free(data);result=bounds;return true;
    }catch(const char *message){error=message;}catch(const std::exception &e){error=e.what();}catch(...){error="Unknown bounds preparation failure";}
    if(data)cgltf_free(data);
    return false;
}
std::shared_ptr<const Asset> prepare(const char *path,const Options &options,std::string &error){
    error.clear();cgltf_data *data=nullptr;
    try{
        struct stat info={};if(!path||stat(path,&info)||info.st_size<20||std::uint64_t(info.st_size)>max_file_bytes)throw "Invalid static GLB file size";
        if(options.cpu_bytes<1024||options.max_texture_size==0||options.max_texture_size>32768)throw "Invalid model preparation options";
        Budget budget{0,options.cpu_bytes};budget.take(std::uint64_t(info.st_size)*2+sizeof(Asset)+128*sizeof(Mesh));
        auto parent=std::filesystem::path(path).parent_path();if(parent.filename()=="models")parent=parent.parent_path();block_directory=(parent/"texture_blocks").string();
        if(options.cancelled())throw "Model preparation cancelled";
        cgltf_options parser={};if(cgltf_parse_file(&parser,path,&data)!=cgltf_result_success)throw "Invalid glTF data";
        if(data->file_type!=cgltf_file_type_glb||data->nodes_count>512||data->images_count>128||data->textures_count>256||data->skins_count)throw "Expected bounded static GLB";
        std::size_t declared_bytes=0;for(std::size_t i=0;i<data->buffers_count;i++){if(data->buffers[i].uri||data->buffers[i].size>max_file_bytes-declared_bytes)throw "Expected bounded embedded buffers";declared_bytes+=data->buffers[i].size;}
        if(cgltf_load_buffers(&parser,data,path)!=cgltf_result_success||cgltf_validate(data)!=cgltf_result_success)throw "Invalid glTF buffers";
        for(std::size_t i=0;i<data->nodes_count;i++){const cgltf_node *node=&data->nodes[i];std::size_t depth=0;while(node&&depth++<=data->nodes_count)node=node->parent;if(node)throw "Cyclic node parents";}
        auto asset=std::make_shared<Asset>();asset->meshes.reserve(128);asset->textures.reserve(128);std::size_t vertex_count=0,index_count=0;
        const unsigned char white[8]={'V','W','N',1,255,255,255,255},flat[8]={'V','W','N',1,128,128,255,255};for(unsigned i=0;i<3;i++){asset->neutral[i]=std::make_shared<CacheKey>();asset->neutral[i]->prepared=texture_cache_prepare(i==2?flat:white,8);if(!asset->neutral[i]->prepared)throw "Neutral key allocation failed";}
        float minimum[3]={std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max()},maximum[3]={-minimum[0],-minimum[1],-minimum[2]};
        for(std::size_t ni=0;ni<data->nodes_count;ni++){
            const auto *node=&data->nodes[ni];if(!node->mesh||!in_scene(data,node))continue;
            if(options.cancelled())throw "Model preparation cancelled";
            float matrix[16],normal_transform[9],sign;cgltf_node_transform_world(node,matrix);for(float v:matrix)if(!std::isfinite(v))throw "Non-finite node transform";if(!normal_matrix(matrix,normal_transform,sign))throw "Singular node transform";
            for(std::size_t pi=0;pi<node->mesh->primitives_count;pi++){
                const auto &primitive=node->mesh->primitives[pi];if(primitive.type!=cgltf_primitive_type_triangles||primitive.has_draco_mesh_compression||asset->meshes.size()>=128)throw "Unsupported primitive or mesh budget";
                const auto *position=attribute(primitive,cgltf_attribute_type_position),*normal=attribute(primitive,cgltf_attribute_type_normal),*uv=attribute(primitive,cgltf_attribute_type_texcoord),*tangent=attribute(primitive,cgltf_attribute_type_tangent);const cgltf_accessor *paint=nullptr;
                for(std::size_t ai=0;ai<primitive.attributes_count;ai++)if(primitive.attributes[ai].name&&!std::strcmp(primitive.attributes[ai].name,"_VALWOOD_PAINT"))paint=primitive.attributes[ai].data;
                if(!position||!position->count)throw "Missing positions";
                const auto *baked_light=world_model_baked_light::accessor(primitive,position->count);
                const auto *lava_heat=world_model_lava_heat::accessor(primitive,position->count);
                const auto *source_paint=world_model_source_paint::accessor(primitive,position->count);
                const std::size_t count=primitive.indices?primitive.indices->count:position->count;
                if(!count||count%3||position->count>1000000-vertex_count||count>3000000-index_count)throw "Geometry budget exceeded";
                vertex_count+=position->count;index_count+=count;
                budget.take(position->count*sizeof(Vertex)+count*sizeof(std::uint32_t)+(tangent?0:position->count*sizeof(std::array<float,3>)));
                Mesh mesh;mesh.baked_light=baked_light!=nullptr;mesh.source_paint=source_paint!=nullptr;const auto facade=world_model_materials::facade_parameters(primitive.material,data->textures_count);mesh.facade.mode=facade.mode;std::copy(facade.aux,facade.aux+3,mesh.facade.aux);std::copy(facade.scale,facade.scale+3,mesh.facade.scale);
                if(mesh.facade.mode&&(!uv||!tangent||(mesh.facade.mode!=5&&!source_paint&&(!paint||paint->count!=position->count||paint->type!=cgltf_type_scalar))))throw "Missing facade UV/paint/tangent stream";
                if((mesh.facade.mode==3||mesh.facade.mode==4)&&!source_paint)throw "Missing source RGB paint for city material";
                mesh.vertices.resize(position->count);mesh.indices.resize(count);
                for(std::size_t vi=0;vi<position->count;vi++){
                    if((vi&1023)==0&&options.cancelled())throw "Model preparation cancelled";
                    Vertex &v=mesh.vertices[vi];float p[4]={0,0,0,1};if(!cgltf_accessor_read_float(position,vi,p,3)||!finite3(p))throw "Invalid position";transform_direction(matrix,p,v.p);for(int k=0;k<3;k++)v.p[k]+=matrix[12+k];if(!finite3(v.p))throw "Invalid transformed position";for(int k=0;k<3;k++){minimum[k]=std::min(minimum[k],v.p[k]);maximum[k]=std::max(maximum[k],v.p[k]);}
                    if(normal){if(!cgltf_accessor_read_float(normal,vi,p,3)||!finite3(p))throw "Invalid normal";transform_normal(normal_transform,p,v.n);}if(uv&&(!cgltf_accessor_read_float(uv,vi,v.uv,2)||!std::isfinite(v.uv[0])||!std::isfinite(v.uv[1])))throw "Invalid UV";
                    if(tangent){if(!cgltf_accessor_read_float(tangent,vi,p,4)||!finite3(p)||!std::isfinite(p[3]))throw "Invalid tangent";transform_direction(matrix,p,v.tangent);v.tangent[3]=p[3]*sign;}else v.tangent[3]=1;
                    if(mesh.facade.mode&&paint&&(!cgltf_accessor_read_float(paint,vi,&v.facade_paint,1)||!std::isfinite(v.facade_paint)||v.facade_paint<0||v.facade_paint>1))throw "Invalid facade paint";
                    world_model_baked_light::read(baked_light,vi,v.baked_light);
                    world_model_lava_heat::read(lava_heat,vi,v.facade_paint);
                    world_model_source_paint::read(source_paint,vi,v.baked_light,v.facade_paint);
                }
                for(std::size_t ii=0;ii<count;ii++){const auto index=primitive.indices?cgltf_accessor_read_index(primitive.indices,ii):ii;if(index>=position->count)throw "Index outside vertex stream";mesh.indices[ii]=std::uint32_t(index);}if(sign<0)for(std::size_t ii=0;ii<count;ii+=3)std::swap(mesh.indices[ii+1],mesh.indices[ii+2]);complete_basis(mesh,!normal,!tangent);
                if(const auto *material=primitive.material){world_model_materials::winter_parameters(material,mesh.winter);const auto &pbr=material->pbr_metallic_roughness;const bool opaque=material->alpha_mode==cgltf_alpha_mode_opaque;std::copy(pbr.base_color_factor,pbr.base_color_factor+4,mesh.factor);for(float v:mesh.factor)if(!std::isfinite(v))throw "Invalid base factor";mesh.roughness=pbr.roughness_factor;mesh.metallic=pbr.metallic_factor;mesh.cutoff=material->alpha_mode==cgltf_alpha_mode_mask?material->alpha_cutoff:(material->alpha_mode==cgltf_alpha_mode_blend?.4f:0.f);mesh.normal_scale=material->normal_texture.scale;if(!std::isfinite(mesh.normal_scale)||mesh.normal_scale<0||mesh.normal_scale>8)throw "Invalid normal strength";
                    const bool packed=world_model_materials::facade_packed_layers(facade);
                    mesh.textures[0]=texture(*asset,pbr.base_color_texture,true,opaque,budget,options);mesh.textures[1]=texture(*asset,material->normal_texture,false,false,budget,options,packed?0:world_texture_blocks::format_normal_bc7);mesh.textures[2]=texture(*asset,pbr.metallic_roughness_texture,packed,packed&&opaque,budget,options,packed?0:world_texture_blocks::format_linear_bc7);mesh.textures[3]=texture(*asset,material->emissive_texture,true,opaque,budget,options);mesh.detail_sampler[0]=opaque&&facade.mode>0&&pbr.base_color_texture.texture;
                    const float strength=material->has_emissive_strength?material->emissive_strength.emissive_strength:1.f;if(!std::isfinite(strength)||strength<0||strength>10000)throw "Invalid emissive strength";for(int j=0;j<3;j++){mesh.emissive_factor[j]=material->emissive_factor[j]*strength;if(!std::isfinite(mesh.emissive_factor[j])||mesh.emissive_factor[j]<0)throw "Invalid emissive factor";if(facade.aux[j]>=0){cgltf_texture_view view={};view.texture=&data->textures[facade.aux[j]];const bool color=world_model_materials::facade_color_aux(facade,j);mesh.textures[4+j]=texture(*asset,view,world_model_materials::facade_srgb_aux(facade,j),opaque&&color,budget,options);mesh.detail_sampler[j+1]=opaque&&color;}}
                }
                asset->geometry_bytes+=mesh.vertices.size()*sizeof(Vertex)+mesh.indices.size()*sizeof(std::uint32_t);asset->meshes.push_back(std::move(mesh));
            }
        }
        if(asset->meshes.empty())throw "No triangle geometry";
        for(auto &mesh:asset->meshes)for(auto &v:mesh.vertices)v.p[1]-=minimum[1];
        for(int k=0;k<3;k++){asset->minimum[k]=minimum[k];asset->maximum[k]=maximum[k];}
        asset->minimum[1]=0;asset->maximum[1]-=minimum[1];
        asset->cpu_bytes=sizeof(Asset)+asset->meshes.capacity()*sizeof(Mesh)+asset->textures.capacity()*sizeof(Texture);for(const auto &m:asset->meshes)asset->cpu_bytes+=m.vertices.capacity()*sizeof(Vertex)+m.indices.capacity()*sizeof(std::uint32_t);for(const auto &t:asset->textures){asset->cpu_bytes+=t.encoded.capacity()+t.encoded.size()+sizeof(CacheKey)+t.levels.capacity()*sizeof(Level);for(const auto &l:t.levels)asset->cpu_bytes+=l.rgba.capacity();}
        asset->gpu_bytes=asset->upload_bytes=asset->geometry_bytes+asset->texture_bytes;cgltf_free(data);return asset;
    }catch(const char *message){error=message;}catch(const std::exception &e){error=e.what();}catch(...){error="Unknown model preparation failure";}
    if(data)cgltf_free(data);
    return {};
}
}

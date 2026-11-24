#ifndef VALWOOD_WORLD_WEATHER_DRIPS_INC
#define VALWOOD_WORLD_WEATHER_DRIPS_INC
// Global-scope implementation include. Uses the already-linked cgltf/stb
// declarations: no duplicate implementation macros, GL calls or frame IO.
#include "engine/world_weather.h"
#include "engine/world_scenery.h"
#include "engine/world_transform_math.h"
#include "engine/world_model_image_bytes.h"
#include "cgltf.h"
#include "stb_image.h"
#include <filesystem>
#include <map>
#include <memory>
#include <cstring>
#include <cctype>
#include <limits>
#include <climits>
#include <fstream>
namespace world_weather_drips {
using Ground=float(*)(double,double,void*);
using V=std::array<double,3>;
struct LocalPoint {V p{};unsigned kind=0,seed=1;};
struct Asset {std::string stem;bool near=false,tree=false;std::array<LocalPoint,64>points{};unsigned count=0;};
struct Binding {std::array<char,128>alias{};unsigned asset=0;};
struct Stats {std::uint64_t encoded_bytes=0,refreshes=0,triangles_tested=0,alpha_rejected=0,occluded=0;unsigned assets=0,unsupported=0,leaf_anchors=0,roof_anchors=0;};
struct Cache {
    std::array<WorldWeatherDrip,64>points{};unsigned count=0;
    std::array<Asset,8>assets{};unsigned asset_count=0;
    std::array<Binding,1024>bindings{};unsigned binding_count=0;
    double last_x=1.e30,last_z=1.e30;const WorldScenery*scene=nullptr;
    std::uint64_t activity_revision=0;
    Stats stats{};
};
inline unsigned hash(unsigned x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
inline bool word(const char*name,const char*needle){if(!name)return false;const auto n=std::strlen(needle),len=std::strlen(name);for(std::size_t i=0;i+n<=len;++i){bool yes=true;for(std::size_t j=0;j<n;++j)yes&=std::tolower(static_cast<unsigned char>(name[i+j]))==needle[j];if(yes)return true;}return false;}
inline bool roof_name(const char*n){return word(n,"roof")||word(n,"eave")||word(n,"shingle")||word(n,"tile")||word(n,"tent")||word(n,"canvas");}
inline bool candidate(const WorldSceneryPlacement&p){return p.tree||p.foundation||roof_name(p.alias)||word(p.alias,"house")||word(p.alias,"building")||word(p.alias,"tavern");}
inline bool in_scene(const cgltf_data*d,const cgltf_node*n){if(!d->scene&&!d->scenes_count)return true;const auto*s=d->scene?d->scene:&d->scenes[0];for(std::size_t depth=0;n&&depth<=d->nodes_count;++depth,n=n->parent)for(std::size_t i=0;i<s->nodes_count;++i)if(s->nodes[i]==n)return true;return false;}
inline const cgltf_accessor*attribute(const cgltf_primitive&p,cgltf_attribute_type type){for(std::size_t i=0;i<p.attributes_count;++i)if(p.attributes[i].type==type&&p.attributes[i].index==0)return p.attributes[i].data;return nullptr;}
inline V point(const float*m,const cgltf_accessor*a,std::size_t i){float v[3]={};if(!cgltf_accessor_read_float(a,i,v,3))return {NAN,NAN,NAN};V p{};for(unsigned k=0;k<3;++k)p[k]=m[k]*v[0]+m[k+4]*v[1]+m[k+8]*v[2]+m[k+12];return p;}
inline bool finite(V p){return std::isfinite(p[0])&&std::isfinite(p[1])&&std::isfinite(p[2])&&std::abs(p[0])<1.e7&&std::abs(p[1])<1.e7&&std::abs(p[2])<1.e7;}
struct Alpha {
    std::unique_ptr<unsigned char,void(*)(void*)> pixels{nullptr,stbi_image_free};int w=0,h=0,wrap_s=10497,wrap_t=10497;float cutoff=.5f,factor=1;bool opaque=true;
    bool load(const cgltf_material*m,const std::string&path,std::uint64_t&budget,Stats&stats){
        if(!m||m->alpha_mode==cgltf_alpha_mode_opaque)return true;
        opaque=false;factor=m->pbr_metallic_roughness.base_color_factor[3];cutoff=m->alpha_mode==cgltf_alpha_mode_mask?m->alpha_cutoff:.8f;
        const auto&view=m->pbr_metallic_roughness.base_color_texture;
        if(!view.texture||!view.texture->image||view.texcoord||view.has_transform)return false;
        world_model_image_bytes::Image image;std::string error;if(!world_model_image_bytes::read(view.texture->image,path.c_str(),budget,image,error))return false;
        if(image.external()){if(image.length>budget)return false;budget-=image.length;stats.encoded_bytes+=image.length;}
        int c=0;if(image.length>INT_MAX||!stbi_info_from_memory(image.data(),int(image.length),&w,&h,&c)||w<=0||h<=0||w>4096||h>4096||std::uint64_t(w)*h*4>64ull*1024*1024)return false;
        pixels.reset(stbi_load_from_memory(image.data(),int(image.length),&w,&h,&c,4));if(!pixels)return false;
        if(view.texture->sampler){wrap_s=int(view.texture->sampler->wrap_s);wrap_t=int(view.texture->sampler->wrap_t);}return true;
    }
    static int texel(int x,int side,int wrap){if(wrap==33071)return std::clamp(x,0,side-1);if(wrap==33648){int p=x%(2*side);if(p<0)p+=2*side;return p<side?p:2*side-1-p;}x%=side;return x<0?x+side:x;}
    bool visible(const cgltf_accessor*uv,const std::array<std::size_t,3>&ix,const std::array<double,3>&b)const{
        if(opaque)return true;if(!pixels||!uv)return false;double u=0,v=0;for(unsigned i=0;i<3;++i){float t[2]={};if(!cgltf_accessor_read_float(uv,ix[i],t,2)||!std::isfinite(t[0])||!std::isfinite(t[1]))return false;u+=b[i]*t[0];v+=b[i]*t[1];}
        // The entire bilinear footprint must survive the same material alpha
        // cutoff. This deliberately omits atlas edges and transparent holes.
        int x=int(std::floor(u*w-.5)),y=int(std::floor(v*h-.5));for(int j=0;j<2;++j)for(int i=0;i<2;++i){auto a=pixels.get()[(texel(y+j,h,wrap_t)*w+texel(x+i,w,wrap_s))*4+3];if(float(a)/255.f*factor<cutoff+.025f)return false;}return true;
    }
};
struct Edge {V a{},b{},inside{};unsigned count=0,seed=0;bool visible=false;};
using EdgeKey=std::array<std::int64_t,6>;
inline EdgeKey edge_key(V a,V b){if(b<a)std::swap(a,b);EdgeKey k{};for(unsigned i=0;i<3;++i){k[i]=std::llround(a[i]*100000.);k[i+3]=std::llround(b[i]*100000.);}return k;}
// Large owned houses embed ~80MiB of unrelated wall textures. Read only their
// bounded JSON/geometry views and a required alpha image, never the whole BIN.
// cgltf's documented view.data override retains exact accessor offsets/strides.
struct GeometryReader {
    std::ifstream file;std::vector<unsigned char>json;
    std::uint64_t file_size=0,bin_offset=0,bin_size=0,used=0;
    std::uint64_t&budget;Stats&stats;
    GeometryReader(std::uint64_t&b,Stats&s):budget(b),stats(s){}
    bool read(std::uint64_t offset,std::size_t size,void*target){
        if(offset>file_size||size>file_size-offset||size>budget||size>64ull*1024*1024-used)return false;
        file.clear();file.seekg(std::streamoff(offset));file.read(static_cast<char*>(target),std::streamsize(size));if(!file)return false;used+=size;budget-=size;stats.encoded_bytes+=size;return true;
    }
    bool parse(const std::string&path,cgltf_data*&data){
        file_size=std::filesystem::file_size(std::filesystem::u8path(path));if(file_size<28||file_size>512ull*1024*1024)return false;
        file.open(std::filesystem::u8path(path),std::ios::binary);if(!file)return false;
        std::array<std::uint32_t,5>head{};if(!read(0,sizeof(head),head.data())||head[0]!=0x46546c67||head[1]!=2||head[2]!=file_size||head[4]!=0x4e4f534a||head[3]>4*1024*1024||head[3]>file_size-28)return false;
        json.resize(head[3]);if(!read(20,json.size(),json.data()))return false;std::array<std::uint32_t,2>bin{};if(!read(20+json.size(),sizeof(bin),bin.data())||bin[1]!=0x004e4942)return false;
        bin_offset=28+json.size();bin_size=bin[0];if(bin_size!=file_size-bin_offset)return false;
        cgltf_options options{};options.type=cgltf_file_type_gltf;if(cgltf_parse(&options,json.data(),json.size(),&data)!=cgltf_result_success)return false;
        return true;
    }
    bool view(cgltf_buffer_view*v){
        if(!v)return true;if(v->data)return true;if(!v->buffer||v->buffer->uri||v->offset>bin_size||v->size>bin_size-v->offset||v->has_meshopt_compression)return false;
        void*p=std::malloc(v->size?v->size:1);if(!p)return false;if(!read(bin_offset+v->offset,v->size,p)){std::free(p);return false;}v->data=p;return true;
    }
};
inline bool load_asset(Asset&out,const std::string&path,std::uint64_t&budget,Stats&stats){
    try {
        GeometryReader reader(budget,stats);cgltf_data*raw=nullptr;if(!reader.parse(path,raw))return false;
        std::unique_ptr<cgltf_data,void(*)(cgltf_data*)>data(raw,cgltf_free);
        if(data->nodes_count>8192||data->buffers_count!=1||data->buffers[0].uri||data->buffers[0].size>reader.bin_size||cgltf_validate(data.get())!=cgltf_result_success)return false;
        for(std::size_t mi=0;mi<data->meshes_count;++mi)for(std::size_t pi=0;pi<data->meshes[mi].primitives_count;++pi){const auto&p=data->meshes[mi].primitives[pi];for(auto type:{cgltf_attribute_type_position,cgltf_attribute_type_texcoord}){const auto*a=attribute(p,type);if(a&&(a->is_sparse||!reader.view(a->buffer_view)))return false;}if(p.indices&&(p.indices->is_sparse||!reader.view(p.indices->buffer_view)))return false;}
        V minimum={1.e30,1.e30,1.e30},maximum={-1.e30,-1.e30,-1.e30};std::size_t vertices=0;
        for(std::size_t ni=0;ni<data->nodes_count;++ni){const auto*n=&data->nodes[ni];const auto*parent=n;std::size_t depth=0;while(parent&&depth++<=data->nodes_count)parent=parent->parent;if(parent)return false;if(!n->mesh||!in_scene(data.get(),n))continue;float m[16];cgltf_node_transform_world(n,m);for(float v:m)if(!std::isfinite(v))return false;for(std::size_t pi=0;pi<n->mesh->primitives_count;++pi){const auto&p=n->mesh->primitives[pi];const auto*a=attribute(p,cgltf_attribute_type_position);if(!a)continue;vertices+=a->count;if(vertices>1000000)return false;for(std::size_t i=0;i<a->count;++i){const V v=point(m,a,i);if(!finite(v))return false;for(unsigned k=0;k<3;++k){minimum[k]=std::min(minimum[k],v[k]);maximum[k]=std::max(maximum[k],v[k]);}}}}
        if(minimum[1]>maximum[1])return false;
        std::array<LocalPoint,32>leaves{};unsigned leaf_count=0,leaf_seen=0;std::map<EdgeKey,Edge>edges;bool roof_budget=false;
        for(std::size_t ni=0;ni<data->nodes_count;++ni){const auto*n=&data->nodes[ni];if(!n->mesh||!in_scene(data.get(),n))continue;float m[16];cgltf_node_transform_world(n,m);
            for(std::size_t pi=0;pi<n->mesh->primitives_count;++pi){const auto&p=n->mesh->primitives[pi];if(p.type!=cgltf_primitive_type_triangles||p.has_draco_mesh_compression)continue;const auto*pos=attribute(p,cgltf_attribute_type_position);const auto*uv=attribute(p,cgltf_attribute_type_texcoord);if(!pos)continue;const std::size_t count=p.indices?p.indices->count:pos->count;if(count%3)continue;const auto*mat=p.material;
                const bool leaf=out.tree&&mat&&mat->alpha_mode!=cgltf_alpha_mode_opaque;
                const bool roof=!out.tree&&(roof_name(mat?mat->name:nullptr)||roof_name(n->name)||roof_name(n->mesh->name)||roof_name(out.stem.c_str()));
                if(!leaf&&!roof)continue;Alpha alpha;
                if(mat&&mat->alpha_mode!=cgltf_alpha_mode_opaque){const auto*t=mat->pbr_metallic_roughness.base_color_texture.texture;if(t&&t->image&&t->image->buffer_view&&!reader.view(t->image->buffer_view))continue;}
                auto image_budget=std::min<std::uint64_t>(budget,64ull*1024*1024-reader.used);const auto before=image_budget;const bool alpha_ok=alpha.load(mat,path,image_budget,stats);const auto spent=before-image_budget;reader.used+=spent;budget-=spent;if(!alpha_ok)continue;
                const std::size_t triangles=count/3;if(roof&&(triangles>40000||edges.size()+count>120000)){roof_budget=true;continue;}
                const std::size_t stride=leaf?std::max<std::size_t>(1,triangles/4096):1;
                for(std::size_t ti=0;ti<triangles;ti+=stride){std::array<std::size_t,3>ix;std::array<V,3>v;bool okay=true;for(unsigned k=0;k<3;++k){ix[k]=p.indices?cgltf_accessor_read_index(p.indices,ti*3+k):ti*3+k;if(ix[k]>=pos->count){okay=false;break;}v[k]=point(m,pos,ix[k]);okay&=finite(v[k]);}if(!okay)continue;++stats.triangles_tested;
                    const V a={v[1][0]-v[0][0],v[1][1]-v[0][1],v[1][2]-v[0][2]},b={v[2][0]-v[0][0],v[2][1]-v[0][1],v[2][2]-v[0][2]};V normal={a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};const double length=std::sqrt(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);if(length<1.e-10)continue;const double determinant=m[0]*(m[5]*m[10]-m[6]*m[9])-m[4]*(m[1]*m[10]-m[2]*m[9])+m[8]*(m[1]*m[6]-m[2]*m[5]);normal[1]*=determinant<0?-1:1;
                    if(leaf){if((mat->double_sided?std::abs(normal[1]): -normal[1])/length<.15)continue;const std::array<double,3>bary={.23,.31,.46};if(!alpha.visible(uv,ix,bary)){++stats.alpha_rejected;continue;}LocalPoint q;for(unsigned k=0;k<3;++k)q.p[k]=v[0][k]*bary[0]+v[1][k]*bary[1]+v[2][k]*bary[2];q.p[1]-=minimum[1];q.kind=0;q.seed=hash(unsigned(ti)+unsigned(ni)*113+unsigned(pi)*337);++leaf_seen;if(leaf_count<32)leaves[leaf_count++]=q;else{const unsigned at=hash(q.seed)%leaf_seen;if(at<32)leaves[at]=q;}continue;}
                    if(normal[1]/length<.15)continue;for(unsigned edge=0;edge<3;++edge){const unsigned next=(edge+1)%3;const auto key=edge_key(v[edge],v[next]);auto&entry=edges[key];if(++entry.count>1)continue;entry.a=v[edge];entry.b=v[next];std::array<double,3>bary={.02/3,.02/3,.02/3};bary[edge]+=.49;bary[next]+=.49;for(unsigned k=0;k<3;++k)entry.inside[k]=v[0][k]*bary[0]+v[1][k]*bary[1]+v[2][k]*bary[2];entry.visible=alpha.visible(uv,ix,bary);entry.seed=hash(unsigned(ti)+unsigned(ni)*113+unsigned(pi)*337+edge);}
                }
            }
        }
        out.count=0;for(unsigned i=0;i<leaf_count;++i)out.points[out.count++]=leaves[i];
        if(!roof_budget){std::array<LocalPoint,32>roof{};unsigned seen=0,size=0;for(const auto&e:edges){const auto&v=e.second;if(v.count!=1||!v.visible||v.inside[1]<minimum[1]+.45*(maximum[1]-minimum[1]))continue;LocalPoint q;q.p=v.inside;q.p[1]-=minimum[1];q.kind=1;q.seed=v.seed;++seen;if(size<32)roof[size++]=q;else{unsigned at=hash(q.seed)%seen;if(at<32)roof[at]=q;}}for(unsigned i=0;i<size&&out.count<64;++i)out.points[out.count++]=roof[i];}
        // Reject sources covered by real opaque triangles of this asset. This
        // is a startup vertical-ray query, never a bounding-box roof estimate.
        // Masked canopy self-overlap is intentionally allowed: it is a drip
        // source from a wet leaf, not a claim of direct sky transmission.
        std::array<bool,64>blocked{};std::size_t tested=0;
        for(std::size_t ni=0;ni<data->nodes_count;++ni){const auto*n=&data->nodes[ni];if(!n->mesh||!in_scene(data.get(),n))continue;float m[16];cgltf_node_transform_world(n,m);for(std::size_t pi=0;pi<n->mesh->primitives_count;++pi){const auto&primitive=n->mesh->primitives[pi];if(primitive.type!=cgltf_primitive_type_triangles||(primitive.material&&primitive.material->alpha_mode!=cgltf_alpha_mode_opaque))continue;const auto*pos=attribute(primitive,cgltf_attribute_type_position);if(!pos)continue;const auto count=primitive.indices?primitive.indices->count:pos->count;tested+=count/3;if(tested>100000)return false;
            for(std::size_t ti=0;ti+2<count;ti+=3){std::array<V,3>v;bool okay=true;for(unsigned k=0;k<3;++k){auto ix=primitive.indices?cgltf_accessor_read_index(primitive.indices,ti+k):ti+k;if(ix>=pos->count){okay=false;break;}v[k]=point(m,pos,ix);okay&=finite(v[k]);}if(!okay)continue;const double ax=v[1][0]-v[0][0],az=v[1][2]-v[0][2],bx=v[2][0]-v[0][0],bz=v[2][2]-v[0][2],det=ax*bz-az*bx;if(std::abs(det)<1.e-10)continue;
                for(unsigned q=0;q<out.count;++q){if(blocked[q])continue;const auto&p=out.points[q].p;const double x=p[0]-v[0][0],z=p[2]-v[0][2],u=(x*bz-z*bx)/det,w=(ax*z-az*x)/det;if(u<-.00001||w<-.00001||u+w>1.00001)continue;const double y=v[0][1]+u*(v[1][1]-v[0][1])+w*(v[2][1]-v[0][1]);if(y>p[1]+minimum[1]+.045)blocked[q]=true;}
            }
        }}
        unsigned kept=0;for(unsigned i=0;i<out.count;++i)if(!blocked[i])out.points[kept++]=out.points[i];else ++stats.occluded;out.count=kept;
        return out.count>0;
    }catch(...){return false;}
}
inline void refresh(Cache&cache,const WorldScenery*scene,double cx,double cz,Ground ground,void*userdata,bool force=false){
    if(!std::isfinite(cx)||!std::isfinite(cz))return;
    const auto revision=world_scenery_activity_revision(scene);
    if(!force&&cache.scene==scene&&cache.activity_revision==revision&&std::hypot(cx-cache.last_x,cz-cache.last_z)<8)return;
    cache.activity_revision=revision;
    cache.scene=scene;cache.last_x=cx;cache.last_z=cz;cache.count=0;cache.stats.leaf_anchors=cache.stats.roof_anchors=0;++cache.stats.refreshes;
    std::array<double,64>distance{};const auto records=world_scenery_stats(scene).source_instances;
    for(std::size_t i=0;i<records;++i){WorldSceneryPlacement p;if(!world_scenery_placement_active(scene,i)||!world_scenery_placement(scene,i,&p)||!p.alias)continue;const Asset*asset=nullptr;for(unsigned b=0;b<cache.binding_count;++b)if(!std::strcmp(p.alias,cache.bindings[b].alias.data())){asset=&cache.assets[cache.bindings[b].asset];break;}if(!asset||std::hypot(p.x-cx,p.z-cz)>80)continue;
        const auto transform=vw_transform::matrix({p.x,p.y,p.z,p.yaw*57.29577951308232,p.scale,p.pitch*57.29577951308232,p.roll*57.29577951308232,p.sx,p.sy,p.sz});unsigned emitted=0;
        for(unsigned q=0;q<asset->count&&emitted<4;++q){const auto&local=asset->points[(q+hash(unsigned(i))%asset->count)%asset->count];const auto world=vw_transform::point(transform,local.p);const double d=std::hypot(world[0]-cx,world[2]-cz);if(d>=24||!finite(world))continue;const float floor=ground?ground(world[0],world[2],userdata):0;if(!std::isfinite(floor)||world[1]<floor+.20||world[1]>floor+35)continue;
            WorldWeatherDrip drip;drip.x=world[0];drip.y=world[1]-.012;drip.z=world[2];drip.ground_y=floor;drip.exposure=1;drip.kind=local.kind;drip.seed=hash(local.seed^unsigned(i)*977u);unsigned at=cache.count;
            if(at<64)++cache.count;else{at=0;for(unsigned j=1;j<64;++j)if(distance[j]>distance[at])at=j;if(distance[at]<=d)continue;}cache.points[at]=drip;distance[at]=d;++emitted;
        }
    }
    for(unsigned i=0;i<cache.count;++i)if(cache.points[i].kind)++cache.stats.roof_anchors;else ++cache.stats.leaf_anchors;
}
inline bool prepare(Cache&cache,const WorldScenery*scene,const char*models,double cx,double cz,Ground ground,void*userdata){
    cache=Cache{};if(!scene||!models||!std::isfinite(cx)||!std::isfinite(cz))return false;
    struct Candidate{WorldSceneryPlacement p;WorldSceneryAlias definition;double distance=0;};std::array<Candidate,128>candidates{};unsigned count=0;
    for(std::size_t i=0;i<world_scenery_stats(scene).source_instances;++i){WorldSceneryPlacement p;if(!world_scenery_placement_active(scene,i)||!world_scenery_placement(scene,i,&p)||!candidate(p))continue;const double distance=std::hypot(p.x-cx,p.z-cz);if(distance>80)continue;WorldSceneryAlias def;for(std::size_t j=0;j<world_scenery_alias_count();++j){auto a=world_scenery_alias(j);if(a.name&&!std::strcmp(a.name,p.alias)){def=a;break;}}if(!def.stem)continue;unsigned at=count;if(count<128)++count;else{at=0;for(unsigned j=1;j<128;++j)if(candidates[j].distance>candidates[at].distance)at=j;if(candidates[at].distance<=distance)continue;}candidates[at]={p,def,distance};}
    std::sort(candidates.begin(),candidates.begin()+count,[](const Candidate&a,const Candidate&b){return a.distance<b.distance;});std::uint64_t budget=256ull*1024*1024;
    std::array<std::string,8>attempted;unsigned attempts=0;
    for(unsigned i=0;i<count&&attempts<8;++i){const auto&def=candidates[i].definition;const bool near=def.tree||def.named_near;const std::string key=std::string(def.stem)+(near?"_near.glb":".glb");bool have=false;for(unsigned j=0;j<attempts;++j)have|=attempted[j]==key;if(have)continue;attempted[attempts++]=key;Asset asset;asset.stem=def.stem;asset.near=near;asset.tree=def.tree;const auto path=(std::filesystem::u8path(models)/std::filesystem::u8path(key)).u8string();if(load_asset(asset,path,budget,cache.stats))cache.assets[cache.asset_count++]=std::move(asset);else ++cache.stats.unsupported;}
    for(std::size_t i=0;i<world_scenery_alias_count()&&cache.binding_count<cache.bindings.size();++i){const auto def=world_scenery_alias(i);if(!def.name||!def.stem||std::strlen(def.name)>=128)continue;for(unsigned a=0;a<cache.asset_count;++a)if(cache.assets[a].stem==def.stem&&cache.assets[a].near==(def.tree||def.named_near)){auto&binding=cache.bindings[cache.binding_count++];std::strcpy(binding.alias.data(),def.name);binding.asset=a;break;}}
    cache.stats.assets=cache.asset_count;refresh(cache,scene,cx,cz,ground,userdata,true);return true;
}
}
#endif

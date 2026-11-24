#include "engine/world_platform.h"
#include "engine/world_transform_math.h"
#include "engine/world_assets.h"
#include "cgltf.h"
#include "stb_image.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#ifndef _WIN32
#include <spawn.h>
#include <sys/wait.h>
#endif
#include <unistd.h>
extern char **environ;
namespace world_assets {
namespace {
bool active(const cgltf_data *data,const cgltf_node *node){
 if(!data->scene&&data->scenes_count==0)return true;
 const auto *scene=data->scene?data->scene:&data->scenes[0];
 for(size_t depth=0;node&&depth<=data->nodes_count;depth++,node=node->parent)for(size_t i=0;i<scene->nodes_count;i++)if(scene->nodes[i]==node)return true;
 return false;
}
const cgltf_accessor *position(const cgltf_primitive &p){for(size_t i=0;i<p.attributes_count;i++)if(p.attributes[i].type==cgltf_attribute_type_position)return p.attributes[i].data;return nullptr;}
int image_index(const cgltf_data *data,const cgltf_texture_view &v){return v.texture&&v.texture->image?int(v.texture->image-data->images):-1;}
bool view_ok(const cgltf_texture_view &v){return !v.texture||(!v.has_transform&&v.texcoord==0&&v.texture->image&&v.texture->image->buffer_view);}
}
Report inspect(const std::string &path,bool geometry){
 Report r;cgltf_data *raw=nullptr;cgltf_options options={};
 try{
  r.file_bytes=std::filesystem::file_size(path);if(r.file_bytes>128u*1024u*1024u)throw "GLB exceeds128 MiB";
  if(cgltf_parse_file(&options,path.c_str(),&raw)!=cgltf_result_success)throw "Cannot parse GLB";
  std::unique_ptr<cgltf_data,decltype(&cgltf_free)> data(raw,cgltf_free);raw=nullptr;
  r.skins=unsigned(data->skins_count);r.animations=unsigned(data->animations_count);
  if(data->file_type!=cgltf_file_type_glb||data->nodes_count>512||data->images_count>128||data->skins_count)throw "Static embedded GLB required; skinned rigs use the character pipeline";
  size_t bytes=0;for(size_t i=0;i<data->buffers_count;i++){auto &b=data->buffers[i];if(b.uri||b.size>128u*1024u*1024u-bytes)throw "External or oversized buffer";bytes+=b.size;}
  if(cgltf_load_buffers(&options,data.get(),path.c_str())!=cgltf_result_success||cgltf_validate(data.get())!=cgltf_result_success)throw "Invalid buffer/accessor data";
  for(size_t i=0;i<data->images_count;i++){const auto &im=data->images[i];if(!im.buffer_view||im.uri)throw "Embed images in the GLB before importing";const auto n=im.buffer_view->size;const auto *p=cgltf_buffer_view_data(im.buffer_view);Image image;image.bytes=n;if(!p||n>128u*1024u*1024u||!stbi_info_from_memory(p,int(n),&image.width,&image.height,&image.channels)||image.width>8192||image.height>8192||image.width<=0||image.height<=0||std::uint64_t(image.width)*image.height>32u*1024u*1024u)throw "Unsupported, oversized or corrupt embedded image";
   r.reference_texture_bytes+=std::uint64_t(image.width)*image.height*4*4/3;r.images.push_back(image);}
  for(size_t i=0;i<data->materials_count;i++){const auto &m=data->materials[i];const auto &p=m.pbr_metallic_roughness;if(!view_ok(p.base_color_texture)||!view_ok(p.metallic_roughness_texture)||!view_ok(m.normal_texture)||!view_ok(m.emissive_texture))throw "UV0 embedded maps required; bake texture transforms/UV sets before import";
   Material a;a.name=m.name?m.name:std::string("Material ")+std::to_string(i+1);a.color=image_index(data.get(),p.base_color_texture);a.normal=image_index(data.get(),m.normal_texture);a.arm=image_index(data.get(),p.metallic_roughness_texture);a.emissive=image_index(data.get(),m.emissive_texture);a.normal_strength=m.normal_texture.scale;a.roughness=p.roughness_factor;a.metallic=p.metallic_factor;a.masked=m.alpha_mode==cgltf_alpha_mode_mask;a.blended=m.alpha_mode==cgltf_alpha_mode_blend;r.materials.push_back(a);
   if(a.blended)r.warnings.push_back(a.name+": BLEND renders as cutout; sorted glass is unavailable");
   if(a.normal<0)r.warnings.push_back(a.name+": no normal map (flat surface is valid)");
   if(m.occlusion_texture.texture&&image_index(data.get(),m.occlusion_texture)!=a.arm)r.warnings.push_back(a.name+": AO uses a separate image; renderer expects packed red-AO/green-roughness/blue-metallic");}
  r.minimum={1e30,1e30,1e30};r.maximum={-1e30,-1e30,-1e30};size_t primitives=0;
  for(size_t n=0;n<data->nodes_count;n++){const auto &node=data->nodes[n];if(!node.mesh||!active(data.get(),&node))continue;float matrix[16];cgltf_node_transform_world(&node,matrix);
   for(size_t j=0;j<node.mesh->primitives_count;j++){const auto &p=node.mesh->primitives[j];const auto *a=position(p);const size_t indices=p.indices?p.indices->count:(a?a->count:0);if(!a||p.type!=cgltf_primitive_type_triangles||p.has_draco_mesh_compression||indices%3||++primitives>128)throw "Triangle primitives with readable positions are required";
    r.vertices+=a->count;r.triangles+=indices/3;if(r.vertices>1000000||r.triangles>1000000)throw "Static vertex/triangle budget exceeded";
    std::vector<std::array<float,3>> vertices(a->count);
    for(size_t k=0;k<a->count;k++){float v[3];if(!cgltf_accessor_read_float(a,k,v,3))throw "Invalid position accessor";auto &q=vertices[k];for(int axis=0;axis<3;axis++){q[axis]=matrix[axis]*v[0]+matrix[4+axis]*v[1]+matrix[8+axis]*v[2]+matrix[12+axis];if(!std::isfinite(q[axis]))throw "Non-finite geometry";}r.minimum.x=std::min(r.minimum.x,double(q[0]));r.minimum.y=std::min(r.minimum.y,double(q[1]));r.minimum.z=std::min(r.minimum.z,double(q[2]));r.maximum.x=std::max(r.maximum.x,double(q[0]));r.maximum.y=std::max(r.maximum.y,double(q[1]));r.maximum.z=std::max(r.maximum.z,double(q[2]));}
    if(geometry)for(size_t k=0;k<indices;k+=3){size_t ix[3];for(int c=0;c<3;c++){ix[c]=p.indices?cgltf_accessor_read_index(p.indices,k+c):k+c;if(ix[c]>=vertices.size())throw "Index outside vertex array";}r.geometry.push_back({vertices[ix[0]],vertices[ix[1]],vertices[ix[2]]});}
   }
  }
  if(!r.triangles)throw "No active triangle geometry";
  const double base=r.minimum.y;r.maximum.y-=base;r.minimum.y=0;for(auto &t:r.geometry)for(auto *v:{&t.a,&t.b,&t.c})(*v)[1]-=float(base);
  if(r.animations)r.warnings.push_back("Node animation is not played by static scenery");
  if(r.triangles>100000)r.warnings.push_back("High triangle count: supply a reduced LOD before scattering");
  if(r.reference_texture_bytes>128u*1024u*1024u)r.warnings.push_back("Large texture cost: reference RGBA mip estimate exceeds128 MiB");
  r.supported=true;
 }catch(const char *e){r.error=e;}catch(const std::exception &e){r.error=e.what();}if(raw)cgltf_free(raw);return r;
}
bool ray_mesh(const Report &report,const world_studio::Ray &ray,const world_studio::Pose &pose,double &distance){
 double box=0;if(!report.supported||report.geometry.empty()||!world_studio::ray_bounds(ray,pose,box))return false;
 const auto inverse=vw_transform::inverse(vw_transform::matrix(vw_transform::pose(pose)));
 auto local=[&](world_studio::Vec v){const auto q=vw_transform::vector(inverse,{v.x,v.y,v.z});return world_studio::Vec{q[0],q[1],q[2]};};
 const auto o=local({ray.origin.x-pose.root.x,ray.origin.y-pose.root.y,ray.origin.z-pose.root.z}),d=local(ray.direction);double best=std::numeric_limits<double>::infinity();
 auto sub=[](world_studio::Vec a,world_studio::Vec b){return world_studio::Vec{a.x-b.x,a.y-b.y,a.z-b.z};};auto cross=[](world_studio::Vec a,world_studio::Vec b){return world_studio::Vec{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};};auto dot=[](world_studio::Vec a,world_studio::Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;};
 for(const auto &t:report.geometry){world_studio::Vec a{t.a[0],t.a[1],t.a[2]},b{t.b[0],t.b[1],t.b[2]},cc{t.c[0],t.c[1],t.c[2]};auto e1=sub(b,a),e2=sub(cc,a),p=cross(d,e2);double det=dot(e1,p);if(std::abs(det)<1e-12)continue;auto v=sub(o,a);double u=dot(v,p)/det;if(u<0||u>1)continue;auto q=cross(v,e1);double w=dot(d,q)/det;if(w<0||u+w>1)continue;double hit=dot(e2,q)/det;if(hit>=0)best=std::min(best,hit);}
 if(!std::isfinite(best))return false;
 distance=best;return true;
}
ImportResult import_model(const std::string &cooker,const std::string &project,const std::string &alias,const std::array<std::string,3> &sources,double units){
 ImportResult out;out.report=inspect(sources[0]);if(!out.report.supported){out.error=out.report.error;return out;}
 for(unsigned i=1;i<3;i++)if(!sources[i].empty()){auto report=inspect(sources[i]);if(!report.supported){out.error="LOD"+std::to_string(i)+": "+report.error;return out;}if(report.triangles>out.report.triangles)out.report.warnings.push_back("A distant LOD has more triangles than LOD0");}
 std::string filename=vw_temp_pattern("valwood-import-XXXXXX");int fd=mkstemp(filename.data());if(fd<0){out.error="Cannot create importer result";return out;}close(fd);const char *file=filename.c_str();
 struct Cleanup{const char *p;~Cleanup(){unlink(p);}} cleanup{file};
 std::vector<std::string> values={"python3",cooker,"--project",project,"--alias",alias,"--units",std::to_string(units),"--result",file,"--near",sources[0]};if(!sources[1].empty()){values.push_back("--mid");values.push_back(sources[1]);}if(!sources[2].empty()){values.push_back("--far");values.push_back(sources[2]);}
#ifdef _WIN32
 values[0]=vw_python_executable();std::string diagnostics;int status=vw_run_process(values,diagnostics,out.error);const bool exited_ok=status==0;
#else
 std::vector<char*> args;for(auto &v:values)args.push_back(v.data());args.push_back(nullptr);pid_t pid=0;int status=0;if(posix_spawnp(&pid,"python3",nullptr,nullptr,args.data(),environ)!=0){out.error="Cannot start Python asset cooker";return out;}
 while(waitpid(pid,&status,0)<0){if(errno!=EINTR){out.error="Cannot wait for asset cooker";return out;}}
 const bool exited_ok=WIFEXITED(status)&&WEXITSTATUS(status)==0;
#endif
 std::ifstream result(file);std::string line;std::getline(result,line);if(!exited_ok||line.rfind("OK ",0)!=0){out.error=line.empty()?"Asset cooker failed":line;return out;}out.stem=line.substr(3);out.report=inspect(project+"/models/"+out.stem+".glb");if(!out.report.supported){out.error=out.report.error;return out;}out.ok=true;return out;
}
EnvironmentImport import_environment(const std::string &project,const std::string &source){
 EnvironmentImport result;
 try{
  auto size=std::filesystem::file_size(source);if(size>64u*1024u*1024u)throw "HDR exceeds64 MiB";int w=0,h=0,c=0;if(!stbi_is_hdr(source.c_str())||!stbi_info(source.c_str(),&w,&h,&c)||w!=h*2||w<2||h<2||w>4096||h>2048)throw "Use a2:1 Radiance HDR panorama, up to4096x2048";
  std::unique_ptr<float,decltype(&stbi_image_free)> pixels(stbi_loadf(source.c_str(),&w,&h,&c,3),stbi_image_free);if(!pixels)throw "Cannot decode HDR";for(size_t i=0;i<size_t(w)*h*3;i++)if(!std::isfinite(pixels.get()[i])||pixels.get()[i]<0||pixels.get()[i]>60000)throw "Invalid or excessive HDR radiance";pixels.reset();
  std::ifstream input(source,std::ios::binary);std::vector<char> bytes(size);if(!input.read(bytes.data(),std::streamsize(size)))throw "Cannot read HDR source";uint64_t hash=14695981039346656037ull;for(unsigned char b:bytes){hash^=b;hash*=1099511628211ull;}char name[40];std::snprintf(name,sizeof name,"sky_%016llx.hdr",(unsigned long long)hash);
  auto root=std::filesystem::canonical(project),directory=std::filesystem::weakly_canonical(root/"environments");if(directory.generic_string().rfind(root.generic_string()+"/",0)!=0)throw "Environment folder escapes project";std::filesystem::create_directories(directory);auto path=directory/name;
  if(std::filesystem::exists(path)){std::ifstream old(path,std::ios::binary|std::ios::ate);if(!old||old.tellg()!=std::streamoff(size))throw "Existing HDR revision differs";old.seekg(0);std::vector<char> previous(size);if(!old.read(previous.data(),std::streamsize(size))||previous!=bytes)throw "Existing HDR revision differs";}
  else{auto pattern=(directory/".import-XXXXXX").string();std::vector<char> temp(pattern.begin(),pattern.end());temp.push_back(0);int fd=mkstemp(temp.data());if(fd<0)throw "Cannot create HDR transaction";size_t done=0;bool ok=true;while(done<bytes.size()){auto n=write(fd,bytes.data()+done,bytes.size()-done);if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}done+=size_t(n);}if(ok)ok=fsync(fd)==0;int closed=close(fd);ok=ok&&closed==0;if(ok)ok=link(temp.data(),path.c_str())==0;unlink(temp.data());if(!ok)throw "HDR save transaction failed";}
  result.path="environments/"+std::string(name);result.ok=true;
 }catch(const char *e){result.error=e;}catch(const std::exception &e){result.error=e.what();}return result;
}

}

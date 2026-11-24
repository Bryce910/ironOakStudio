// CPU-only purchased FBX extraction. No renderer, source mutations or centering.
#include "../ufbx.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>
namespace fs=std::filesystem;
static std::string quote(const std::string&s){std::string o="\"";for(unsigned char c:s){if(c=='"'||c=='\\')o+='\\'; if(c>=32)o+=c;}return o+'"';}
static ufbx_vec3 normalized(ufbx_vec3 v){double n=sqrt(v.x*v.x+v.y*v.y+v.z*v.z);if(n>1e-20){v.x/=n;v.y/=n;v.z/=n;}return v;}
static ufbx_vec3 cross(ufbx_vec3 a,ufbx_vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static double dot(ufbx_vec3 a,ufbx_vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
int main(int argc,char**argv){
 if(argc<3){fprintf(stderr,"usage: export_village_source_fbx INPUT_DIR OUTPUT_DIR\n");return 2;}
 fs::create_directories(argv[2]);std::vector<fs::path>files;for(auto&e:fs::directory_iterator(argv[1]))if(e.path().extension()==".FBX"||e.path().extension()==".fbx")files.push_back(e.path());std::sort(files.begin(),files.end());
 std::ofstream manifest(fs::path(argv[2])/"meshes.json");manifest<<"{\"schema\":\"village-fbx-raw-18-v1\",\"coordinate\":\"Unreal centimetres X-forward Y-right Z-up\",\"vertex_stride\":18,\"meshes\":[";bool first=true;size_t totaltri=0;
 for(auto &path:files){ufbx_load_opts opts={};opts.load_external_files=false;opts.generate_missing_normals=true;opts.target_axes=ufbx_axes_left_handed_z_up;opts.target_unit_meters=.01;ufbx_error err;ufbx_scene*s=ufbx_load_file(path.c_str(),&opts,&err);if(!s){fprintf(stderr,"%s: %s\n",path.c_str(),err.description.data);return 1;}
  size_t ordinal=0;for(auto*node:s->nodes){if(!node->mesh)continue;auto*m=node->mesh;std::string stem=path.stem().string()+"__"+std::to_string(ordinal++);std::vector<float>vertices;std::map<int,std::vector<uint32_t>>parts;std::vector<uint32_t>tri(std::max(size_t(3),m->max_face_triangles*3));
   // inverse transpose of affine matrix is the exact normal transform.
   ufbx_matrix normal=ufbx_matrix_for_normals(&node->geometry_to_world);
   for(size_t fi=0;fi<m->faces.count;++fi){auto face=m->faces.data[fi];size_t nt=ufbx_triangulate_face(tri.data(),tri.size(),m,face);int slot=m->face_material.count?int(m->face_material.data[fi]):0;
    for(size_t ti=0;ti<nt*3;++ti){uint32_t ix=tri[ti];ufbx_vec3 p=ufbx_transform_position(&node->geometry_to_world,ufbx_get_vertex_vec3(&m->vertex_position,ix));ufbx_vec3 n=normalized(ufbx_transform_direction(&normal,ufbx_get_vertex_vec3(&m->vertex_normal,ix)));
     ufbx_vec2 uv={0,0},uv1={0,0};if(m->uv_sets.count)uv=ufbx_get_vertex_vec2(&m->uv_sets.data[0].vertex_uv,ix);if(m->uv_sets.count>1)uv1=ufbx_get_vertex_vec2(&m->uv_sets.data[1].vertex_uv,ix);
     ufbx_vec3 t={1,0,0},b={0,1,0};if(m->vertex_tangent.exists)t=normalized(ufbx_transform_direction(&node->geometry_to_world,ufbx_get_vertex_vec3(&m->vertex_tangent,ix)));if(m->vertex_bitangent.exists)b=normalized(ufbx_transform_direction(&node->geometry_to_world,ufbx_get_vertex_vec3(&m->vertex_bitangent,ix)));
     ufbx_vec4 c={1,1,1,1};if(m->vertex_color.exists)c=ufbx_get_vertex_vec4(&m->vertex_color,ix);float data[]={float(p.x),float(p.y),float(p.z),float(n.x),float(n.y),float(n.z),float(uv.x),float(uv.y),float(uv1.x),float(uv1.y),float(t.x),float(t.y),float(t.z),float(dot(cross(n,t),b)<0?-1:1),float(c.x),float(c.y),float(c.z),float(c.w)};
     parts[slot].push_back(uint32_t(vertices.size()/18));vertices.insert(vertices.end(),data,data+18);
    }
   }
   std::ofstream vf(fs::path(argv[2])/(stem+".v18.f32"),std::ios::binary);vf.write((char*)vertices.data(),vertices.size()*4);if(!first)manifest<<',';first=false;
   manifest<<"{\"name\":"<<quote(node->name.data)<<",\"mesh_name\":"<<quote(m->name.data)<<",\"source_file\":"<<quote(path.string())<<",\"vertices\":"<<quote(stem+".v18.f32")<<",\"vertex_count\":"<<vertices.size()/18<<",\"uv_sets\":"<<m->uv_sets.count<<",\"colors\":"<<(m->vertex_color.exists?"true":"false")<<",\"tangents\":"<<(m->vertex_tangent.exists?"true":"false")<<",\"parts\":[";bool fp=true;
   for(auto &part:parts){std::string iname=stem+".slot"+std::to_string(part.first)+".i32";std::ofstream f(fs::path(argv[2])/iname,std::ios::binary);f.write((char*)part.second.data(),part.second.size()*4);if(!fp)manifest<<',';fp=false;std::string material=part.first<int(node->materials.count)?node->materials.data[part.first]->name.data:"";manifest<<"{\"slot\":"<<part.first<<",\"material\":"<<quote(material)<<",\"indices\":"<<quote(iname)<<",\"index_count\":"<<part.second.size()<<'}';totaltri+=part.second.size()/3;}
   manifest<<"]}";
  }ufbx_free_scene(s);
 }
 manifest<<"],\"triangles\":"<<totaltri<<"}\n";fprintf(stderr,"Extracted %zu FBXs, %zu triangles\n",files.size(),totaltri);return 0;
}

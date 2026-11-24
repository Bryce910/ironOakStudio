// Separate offline tool. The game never links the encoder or creates caches.
#define GL_GLEXT_PROTOTYPES
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "gl_helpers.h"
#include "tools/world_texture_blocks_encoder.h"
#include "engine/world_model_facade.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs=std::filesystem;
using Clock=std::chrono::steady_clock;
struct Record {
    std::string source,material,file,status;
    std::uint32_t width=0,height=0;
    std::uint64_t rgba_bytes=0,block_bytes=0;
    double seconds=0;
    bool normal=false;
    double normal_max_degrees=0,normal_rms_degrees=0;
    std::vector<world_texture_blocks::Level> levels;
};
static std::string quote(const std::string &text){
    std::ostringstream out;out<<'"';
    for(unsigned char c:text){if(c=='"'||c=='\\')out<<'\\'<<char(c);else if(c<32)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<unsigned(c)<<std::dec;else out<<char(c);}
    out<<'"';return out.str();
}
static bool quality(const std::vector<std::uint8_t>&a,const std::vector<std::uint8_t>&b,world_texture_blocks::Level &level,bool linear,bool normal,double &normal_max,double &normal_rms){
    double squares=0,angle_squares=0;std::array<double,3> bias={};unsigned alpha=0,maximum=0;
    for(std::size_t p=0;p<a.size();p+=4){
        for(unsigned c=0;c<3;c++){const int d=int(b[p+c])-int(a[p+c]);squares+=double(d)*d;bias[c]+=d;maximum=std::max(maximum,unsigned(std::abs(d)));}
        alpha=std::max(alpha,unsigned(std::abs(int(b[p+3])-255)));
        if(normal){
            double dot=0,aa=0,bb=0;
            for(unsigned c=0;c<3;c++){const double x=double(a[p+c])*2/255-1,y=double(b[p+c])*2/255-1;dot+=x*y;aa+=x*x;bb+=y*y;}
            if(aa<.04||bb<.04)return false;
            const double angle=std::acos(std::clamp(dot/std::sqrt(aa*bb),-1.,1.))*180/3.141592653589793;
            normal_max=std::max(normal_max,angle);angle_squares+=angle*angle;
        }
    }
    const double pixels=a.size()/4;
    level.rmse=float(std::sqrt(squares/(pixels*3)));level.alpha_max=alpha;
    level.max_error=linear?maximum:0;
    bool okay=level.rmse<10&&alpha==0;
    if(normal){normal_rms=std::max(normal_rms,std::sqrt(angle_squares/pixels));okay&=normal_max<=world_texture_blocks::normal_angle_max&&normal_rms<=world_texture_blocks::normal_rms_max;}
    for(unsigned c=0;c<3;c++){level.bias[c]=float(bias[c]/pixels);okay&=std::abs(level.bias[c])<3;}
    if(linear)okay&=world_texture_blocks::detail::linear_quality(level.rmse,level.bias,maximum);
    return okay;
}
static bool cook(const void *encoded,std::size_t length,world_texture_blocks::Image &image,std::string &reason,bool linear,bool normal,double &normal_max,double &normal_rms){
    image={};
    int width=0,height=0,channels=0;
    if(length<4||length>64u*1024u*1024u||!stbi_info_from_memory(static_cast<const stbi_uc*>(encoded),int(length),&width,&height,&channels)
       ||width<=0||height<=0||width>16384||height>16384||std::uint64_t(width)*height>64u*1024u*1024u){reason="invalid_image";return false;}
    auto *pixels=stbi_load_from_memory(static_cast<const stbi_uc*>(encoded),int(length),&width,&height,&channels,4);
    if(!pixels){reason="decode_failed";return false;}
    bool opaque=true;for(std::size_t i=3;i<std::size_t(width)*height*4;i+=4)if(pixels[i]!=255){opaque=false;break;}
    if(!opaque&&linear){stbi_image_free(pixels);reason="nonopaque_alpha_retained";return false;}
    // This color path is called only for OPAQUE materials and explicitly
    // identified facade RGB slots. Their source alpha is utility data and is
    // never used for coverage. Keep every source RGB byte; do not reject a
    // whole roof atlas because its unused alpha is zero. MASK/BLEND never enter.
    GLuint reference=linear?GLH_texture_pixels(pixels,width,height,4):GLH_texture_pixels_srgb(pixels,width,height,4);stbi_image_free(pixels);
    if(!reference||glGetError()!=GL_NO_ERROR){if(reference)glDeleteTextures(1,&reference);reason="reference_upload_failed";return false;}
    GLuint compressed=0;glGenTextures(1,&compressed);image.width=unsigned(width);image.height=unsigned(height);
    image.format=normal?world_texture_blocks::format_normal_bc7:(linear?world_texture_blocks::format_linear_bc7:world_texture_blocks::format_srgb_bc7);
    const GLenum gpu_format=linear?GL_COMPRESSED_RGBA_BPTC_UNORM:GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;
    bool okay=true;
    for(unsigned i=0,w=unsigned(width),h=unsigned(height);;i++,w=std::max(1u,w/2),h=std::max(1u,h/2)){
        std::vector<std::uint8_t> before(std::size_t(w)*h*4),after(before.size());
        glBindTexture(GL_TEXTURE_2D,reference);glGetTexImage(GL_TEXTURE_2D,GLint(i),GL_RGBA,GL_UNSIGNED_BYTE,before.data());
        // Preserve production's alpha-edge RGB preparation and its exact native
        // mip chain first. Clearing utility alpha before GLH would bypass that
        // preparation and silently change RGB. Only the unused result alpha is
        // normalized after readback; every original RGB mip byte stays intact.
        if(!linear)for(std::size_t p=3;p<before.size();p+=4)before[p]=255;
        world_texture_blocks::Level level;
        if(!world_texture_blocks_encoder::encode(before,w,h,level,reason)){okay=false;break;}
        glBindTexture(GL_TEXTURE_2D,compressed);
        glCompressedTexImage2D(GL_TEXTURE_2D,GLint(i),gpu_format,GLsizei(w),GLsizei(h),0,GLsizei(level.blocks.size()),level.blocks.data());
        GLint flag=0,bytes=0,format=0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D,GLint(i),GL_TEXTURE_COMPRESSED,&flag);
        glGetTexLevelParameteriv(GL_TEXTURE_2D,GLint(i),GL_TEXTURE_COMPRESSED_IMAGE_SIZE,&bytes);
        glGetTexLevelParameteriv(GL_TEXTURE_2D,GLint(i),GL_TEXTURE_INTERNAL_FORMAT,&format);
        if(glGetError()!=GL_NO_ERROR||!flag||unsigned(format)!=gpu_format||bytes<0||std::size_t(bytes)!=level.blocks.size()){
            reason="compressed_upload_failed";okay=false;break;
        }
        glGetTexImage(GL_TEXTURE_2D,GLint(i),GL_RGBA,GL_UNSIGNED_BYTE,after.data());
        if(glGetError()!=GL_NO_ERROR||!quality(before,after,level,linear,normal,normal_max,normal_rms)||(i==0&&level.rmse>world_texture_blocks::base_rmse_max)){
            reason="quality_rejected_original_retained";okay=false;
            std::printf("  Reject mip%u %ux%u rmse%.5f bias[%.5f %.5f %.5f] alpha%u\n",i,w,h,level.rmse,level.bias[0],level.bias[1],level.bias[2],level.alpha_max);
            break;
        }
        image.levels.push_back(std::move(level));if(w==1&&h==1)break;
    }
    glDeleteTextures(1,&compressed);glDeleteTextures(1,&reference);
    if(!okay){image={};return false;}
    image.normal_max_degrees=float(normal_max);image.normal_rms_degrees=float(normal_rms);
    reason.clear();return true;
}
static void report(const fs::path &path,const std::vector<Record>&rows,bool linear){
    std::ofstream out(path);if(!out)throw std::runtime_error("Cannot write report");
    std::uint64_t reference=0,blocks=0;unsigned accepted=0,rejected=0;
    for(const auto &r:rows)if(r.status=="cooked"||r.status=="existing_validated"){reference+=r.rgba_bytes;blocks+=r.block_bytes;accepted++;}else rejected++;
    out<<"{\n  \"schema_version\":1,\"encoder\":\"bc7enc16\",\"revision\":\"b37c4bb65af406ab2a1071f2a312bdbe112e4f53\",\n"
       <<"  \"recipe\":1,\"rgb_metric\":\"equal RGB\",\"uber\":4,\"partitions\":64,\"filterbank\":false,\"opaque_mode6_alpha_finalization\":true,\n"
       <<"  \"source_images_changed\":false,\"full_resolution_and_original_reference_mips\":true,\"quality_guard\":{\"every_mip_rmse_below\":10,\"every_channel_abs_bias_below\":3,\"alpha_max\":0,\"base_psnr_min_db\":38},\n"
       <<"  \"accepted\":"<<accepted<<",\"retained_original\":"<<rejected<<",\"rgba_reference_bytes\":"<<reference<<",\"bc7_bytes\":"<<blocks<<",\"saved_gpu_bytes\":"<<(reference-blocks)<<",\n  \"images\":[\n";
    bool first=true;
    for(const auto&r:rows){if(!first)out<<",\n";first=false;
        out<<"    {\"source\":"<<quote(r.source)<<",\"material\":"<<quote(r.material)<<",\"file\":"<<quote(r.file)<<",\"status\":"<<quote(r.status)<<",\"width\":"<<r.width<<",\"height\":"<<r.height<<",\"rgba_bytes\":"<<r.rgba_bytes<<",\"block_bytes\":"<<r.block_bytes<<",\"seconds\":"<<r.seconds<<",\"normal_map\":"<<(r.normal?"true":"false")<<",\"normal_max_degrees\":"<<r.normal_max_degrees<<",\"normal_rms_degrees\":"<<r.normal_rms_degrees<<",\"levels\":[";
        bool initial=true;for(const auto&l:r.levels){if(!initial)out<<',';initial=false;out<<"{\"width\":"<<l.width<<",\"height\":"<<l.height<<",\"bytes\":"<<l.blocks.size()<<",\"rmse\":"<<l.rmse<<",\"bias\":["<<l.bias[0]<<','<<l.bias[1]<<','<<l.bias[2]<<"],\"alpha_max\":"<<l.alpha_max<<",\"max_error\":"<<l.max_error<<'}';}out<<"]}";
    }
    out<<"\n  ],\n  \"linear_data\": "<<(linear?"true":"false")<<", \"linear_rmse_max\": 1.5, \"linear_channel_max_error\": 6, \"normal_max_degrees\": 2, \"normal_rms_degrees_max\": 0.75\n}\n";if(!out)throw std::runtime_error("Report write failed");
}
static cgltf_data *load_model(const fs::path &file){
            if(fs::file_size(file)>128u*1024u*1024u)throw std::runtime_error("GLB exceeds loader file budget");
            cgltf_options options={};cgltf_data *data=nullptr;
            if(cgltf_parse_file(&options,file.c_str(),&data)!=cgltf_result_success)throw std::runtime_error("Invalid GLB: "+file.string());
            if(data->file_type!=cgltf_file_type_glb||data->nodes_count>512||data->images_count>128||data->skins_count)throw std::runtime_error("Expected bounded static GLB");
            std::size_t declared=0;
            for(std::size_t bi=0;bi<data->buffers_count;bi++){
                if(data->buffers[bi].uri||data->buffers[bi].size>128u*1024u*1024u-declared)throw std::runtime_error("Expected bounded embedded buffers");
                declared+=data->buffers[bi].size;
            }
            if(cgltf_load_buffers(&options,data,file.c_str())!=cgltf_result_success||cgltf_validate(data)!=cgltf_result_success)throw std::runtime_error("Invalid embedded source");
    return data;
}
int main(int argc,char**argv){
    try {
        fs::path models="projects/valwood_world/models",output;std::vector<fs::path> files;bool force=false,linear=false;
        for(int i=1;i<argc;i++){
            const std::string arg=argv[i];
            if(arg=="--models"&&i+1<argc)models=argv[++i];else if(arg=="--output"&&i+1<argc)output=argv[++i];
            else if(arg=="--model"&&i+1<argc)files.emplace_back(argv[++i]);else if(arg=="--force")force=true;else if(arg=="--linear")linear=true;
            else if(arg=="--help"){std::puts("Offline opaque sRGB BC7 cooker: --models DIR --output DIR [--model GLB ...] [--force] [--linear]. Requires isolated GL3.3+BPTC for exact reference mips and decoded quality checks. Never run during FPS acceptance.");return 0;}
            else throw std::runtime_error("Unknown/incomplete argument: "+arg);
        }
        if(output.empty())output=models.parent_path()/"texture_blocks";
        fs::create_directories(output);
        if(files.empty())for(const auto &item:fs::directory_iterator(models))if(item.is_regular_file()&&item.path().extension()==".glb")files.push_back(item.path());
        std::sort(files.begin(),files.end());if(files.empty())throw std::runtime_error("No GLB inputs");
        if(!SDL_getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
        if(SDL_Init(SDL_INIT_VIDEO))throw std::runtime_error(SDL_GetError());
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
        auto *window=SDL_CreateWindow("Offline Valwood texture cooker",0,0,32,32,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);if(!window)throw std::runtime_error(SDL_GetError());
        auto context=SDL_GL_CreateContext(window);if(!context)throw std::runtime_error(SDL_GetError());
        if(!SDL_GL_ExtensionSupported("GL_ARB_texture_compression_bptc"))throw std::runtime_error("BPTC unsupported: originals retained; no cache created");
        std::setvbuf(stdout,nullptr,_IOLBF,0);std::printf("Offline texture cooker renderer:%s models%zu\n",glGetString(GL_RENDERER),files.size());
        // A fingerprint collision only adds the stricter normal guard; exact
        // source bytes remain authoritative for cache identity below.
        std::set<std::uint64_t> normal_sources;
        if(linear)for(const auto &file:files){
            auto *data=load_model(file);
            for(std::size_t i=0;i<data->materials_count;i++){
                const auto *texture=data->materials[i].normal_texture.texture;
                if(!texture||!texture->image||!texture->image->buffer_view)continue;
                const auto *v=texture->image->buffer_view;
                const auto *bytes=cgltf_buffer_view_data(v);
                if(bytes&&v->size>=4&&v->size<=64u*1024u*1024u)normal_sources.insert(world_texture_blocks::fingerprint(bytes,v->size));
            }
            cgltf_free(data);
        }
        std::map<std::uint64_t,std::vector<std::vector<std::uint8_t>>> seen;std::size_t seen_bytes=0;std::vector<Record> records;
        for(const auto &file:files){
            cgltf_data *data=load_model(file);
            for(std::size_t mi=0;mi<data->materials_count;mi++){
                const auto &mat=data->materials[mi];if(!linear&&mat.alpha_mode!=cgltf_alpha_mode_opaque)continue;
                std::vector<const cgltf_texture*> textures;
                if(linear){if(mat.normal_texture.texture)textures.push_back(mat.normal_texture.texture);if(mat.pbr_metallic_roughness.metallic_roughness_texture.texture)textures.push_back(mat.pbr_metallic_roughness.metallic_roughness_texture.texture);if(mat.occlusion_texture.texture)textures.push_back(mat.occlusion_texture.texture);}
                else if(mat.pbr_metallic_roughness.base_color_texture.texture)textures.push_back(mat.pbr_metallic_roughness.base_color_texture.texture);
                if(!linear&&mat.emissive_texture.texture)textures.push_back(mat.emissive_texture.texture);
                if(!linear){
                    const auto facade=world_model_materials::facade_parameters(&mat,data->textures_count);
                    for(int j=0;j<3;j++)if(facade.aux[j]>=0&&world_model_materials::facade_color_aux(facade,j))
                        textures.push_back(data->textures+facade.aux[j]);
                }
                // All facade mask/normal/ARM auxiliaries remain exact RGBA8.
                for(const auto *texture:textures){
                    const auto *image=texture->image;if(!image||!image->buffer_view)continue;
                    const auto length=image->buffer_view->size;const auto *encoded=cgltf_buffer_view_data(image->buffer_view);
                    if(!encoded||length<4||length>64u*1024u*1024u)throw std::runtime_error("Image outside loader budget");
                    const auto hash=world_texture_blocks::fingerprint(encoded,length);auto &bucket=seen[hash];bool duplicate=false;
                    for(const auto &prior:bucket)if(prior.size()==length&&!std::memcmp(prior.data(),encoded,length)){duplicate=true;break;}
                    if(duplicate)continue;
                    if(length>768u*1024u*1024u-seen_bytes)throw std::runtime_error("Unique source comparison budget exceeded");
                    bucket.emplace_back(encoded,encoded+length);seen_bytes+=length;
                    char leaf[40];std::snprintf(leaf,sizeof(leaf),linear?"%016llx.linear.vbc7":"%016llx.vbc7",static_cast<unsigned long long>(hash));const auto destination=output/leaf;
                    Record record;record.source=file.string();record.material=mat.name?mat.name:"unnamed";record.file=leaf;record.normal=linear&&normal_sources.count(hash)>0;
                    const auto format=record.normal?world_texture_blocks::format_normal_bc7:(linear?world_texture_blocks::format_linear_bc7:world_texture_blocks::format_srgb_bc7);
                    const auto start=Clock::now();world_texture_blocks::Image cooked;std::string error;
                    int source_width=0,source_height=0,source_channels=0;
                    const bool source_dimensions=stbi_info_from_memory(encoded,int(length),&source_width,&source_height,&source_channels)&&source_width>0&&source_height>0;
                    if(!force&&source_dimensions&&world_texture_blocks::read(destination.c_str(),encoded,length,cooked,error,format)
                       &&cooked.width==unsigned(source_width)&&cooked.height==unsigned(source_height))record.status="existing_validated";
                    else if(cook(encoded,length,cooked,error,linear,record.normal,record.normal_max_degrees,record.normal_rms_degrees)){
                        if(!world_texture_blocks::write(destination.c_str(),encoded,length,cooked,error))throw std::runtime_error("Cache write: "+error);
                        world_texture_blocks::Image verified;if(!world_texture_blocks::read(destination.c_str(),encoded,length,verified,error,format))throw std::runtime_error("Cache selfcheck: "+error);
                        record.status="cooked";
                    }else {record.status=error;std::error_code ec;fs::remove(destination,ec);}
                    if(record.status=="existing_validated"){record.normal_max_degrees=cooked.normal_max_degrees;record.normal_rms_degrees=cooked.normal_rms_degrees;}
                    record.width=cooked.width;record.height=cooked.height;
                    for(const auto &level:cooked.levels){record.rgba_bytes+=std::uint64_t(level.width)*level.height*4;record.block_bytes+=level.blocks.size();}
                    record.levels=std::move(cooked.levels);record.seconds=std::chrono::duration<double>(Clock::now()-start).count();records.push_back(std::move(record));
                    const auto &last=records.back();std::printf("Texture%zu %s %s %ux%u %.3fs saved%.3fMiB\n",records.size(),last.file.c_str(),last.status.c_str(),last.width,last.height,last.seconds,double(last.rgba_bytes-last.block_bytes)/1048576.);
                    report(output/(linear?"linear-manifest.json":"manifest.json"),records,linear);
                }
            }
            cgltf_free(data);
        }
        report(output/(linear?"linear-manifest.json":"manifest.json"),records,linear);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
    }catch(const std::exception &e){std::fprintf(stderr,"Offline texture cooker: %s\n",e.what());return 1;}
}

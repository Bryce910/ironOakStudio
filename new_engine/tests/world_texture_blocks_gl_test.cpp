// Actual full-resolution, full-mip decode quality and transactional rejection.
#define GL_GLEXT_PROTOTYPES
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "engine/world_model_facade.h"
#undef STB_IMAGE_IMPLEMENTATION
#include "gl_helpers.h"
#include "engine/world_model_texture_blocks.h"
#include <SDL2/SDL.h>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <unistd.h>
extern "C" {
#include "engine/texture_cache.h"
void __real_glCompressedTexImage2D(GLenum,GLint,GLenum,GLsizei,GLsizei,GLint,GLsizei,const void*);
void __real_glTexImage2D(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*);
}
static bool inject_failure=false,waiting_for_original=false;
static unsigned injected_failures=0,original_fallbacks=0;
static GLuint original_fallback_texture=0;
static GLsizei failed_width=0,failed_height=0;
extern "C" void __wrap_glCompressedTexImage2D(GLenum target,GLint level,GLenum format,GLsizei w,GLsizei h,GLint border,GLsizei bytes,const void*data){
    if(inject_failure){
        inject_failure=false;waiting_for_original=true;injected_failures++;
        failed_width=w;failed_height=h;
        // Actual invalid driver upload. The stale-error regression used to
        // queue a second INVALID_OPERATION through the size query afterward.
        __real_glCompressedTexImage2D(target,level,0,w,h,border,bytes,data);return;
    }
    __real_glCompressedTexImage2D(target,level,format,w,h,border,bytes,data);
}
extern "C" void __wrap_glTexImage2D(GLenum target,GLint level,GLint format,GLsizei w,GLsizei h,GLint border,GLenum pixels,GLenum type,const void*data){
    __real_glTexImage2D(target,level,format,w,h,border,pixels,type,data);
    if(waiting_for_original&&level==0&&format==GL_SRGB8_ALPHA8&&w==failed_width&&h==failed_height){
        waiting_for_original=false;original_fallbacks++;GLint bound=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&bound);original_fallback_texture=GLuint(bound);
    }
}
namespace fs=std::filesystem;
static unsigned checks=0,images=0,mips=0;
static void require(bool okay,int line=__builtin_LINE()){checks++;if(!okay){std::fprintf(stderr,"Texture block check %u failed at line%d\n",checks,line);std::abort();}}
static void compare(const std::vector<unsigned char>&reference,const std::vector<unsigned char>&decoded,bool base){
    double sq=0;std::array<double,3> bias={};
    for(size_t p=0;p<reference.size();p+=4){for(unsigned c=0;c<3;c++){const int d=int(decoded[p+c])-reference[p+c];sq+=double(d)*d;bias[c]+=d;}require(decoded[p+3]==255);}
    const double pixels=reference.size()/4;const double rmse=std::sqrt(sq/(pixels*3));
    if(rmse>=10||(base&&rmse>world_texture_blocks::base_rmse_max))std::fprintf(stderr,"RGB quality failure: mip%u rmse%.6f base%d bias[%.6f %.6f %.6f]\n",mips,rmse,int(base),bias[0]/pixels,bias[1]/pixels,bias[2]/pixels);
    require(rmse<10);if(base)require(rmse<=world_texture_blocks::base_rmse_max);
    for(double b:bias)require(std::abs(b/pixels)<3);
    std::printf(" mip %u rmse %.6f PSNR %.3f bias[%.6f %.6f %.6f] alpha255\n",mips,rmse,rmse?20*std::log10(255/rmse):999,bias[0]/pixels,bias[1]/pixels,bias[2]/pixels);
}
int main(){
    if(!SDL_getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    require(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto*window=SDL_CreateWindow("Texture block quality fixture",0,0,32,32,SDL_WINDOW_HIDDEN|SDL_WINDOW_OPENGL);require(window);
    auto context=SDL_GL_CreateContext(window);require(context);std::printf("Renderer %s\n",glGetString(GL_RENDERER));
    require(!world_model_texture_blocks::available(false,true));require(!world_model_texture_blocks::available(true,false));
    SDL_setenv("VALWOOD_QA_DISABLE_OPAQUE_BC7","1",1);require(!world_model_texture_blocks::available(true,true));SDL_setenv("VALWOOD_QA_DISABLE_OPAQUE_BC7","0",1);
    if(!world_model_texture_blocks::available(true,true)){std::puts("BPTC unsupported; exact original fallback verified, no compressed quality claim");SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;}
    const char* names[]={"purchased_village_house_a_near.glb","cinematic_pine_near.glb","bep_nature_oak_near.glb","bep_nature_fern_near.glb",
        "purchased_village_house_d_near.glb","castle_training_knight_drill_00.glb"};
    std::set<std::uint64_t> seen;bool rejection_tested=false;std::uint64_t reference_bytes=0,block_bytes=0;unsigned utility_alpha_images=0,auxiliary_images=0,retained_originals=0;
    for(const char*name:names){
        const fs::path path=fs::path("projects/valwood_world/models")/name;const auto size=fs::file_size(path);const auto stamp=fs::last_write_time(path);
        cgltf_options options={};cgltf_data*data=nullptr;require(cgltf_parse_file(&options,path.c_str(),&data)==cgltf_result_success);require(cgltf_load_buffers(&options,data,path.c_str())==cgltf_result_success);
        world_model_texture_blocks::DirectoryScope scope(path.c_str());
        for(size_t mi=0;mi<data->materials_count;mi++){
            const auto&mat=data->materials[mi];if(mat.alpha_mode!=cgltf_alpha_mode_opaque)continue;
            std::vector<const cgltf_texture*> textures={mat.pbr_metallic_roughness.base_color_texture.texture,mat.emissive_texture.texture};
            const auto facade=world_model_materials::facade_parameters(&mat,data->textures_count);
            for(int j=0;j<3;j++)if(facade.aux[j]>=0&&world_model_materials::facade_color_aux(facade,j))textures.push_back(data->textures+facade.aux[j]);
            for(auto*texture:textures){
                if(!texture||!texture->image||!texture->image->buffer_view)continue;
                const auto*view=texture->image->buffer_view;const auto*encoded=cgltf_buffer_view_data(view);const size_t length=view->size;
                const auto hash=world_texture_blocks::fingerprint(encoded,length);if(!seen.insert(hash).second)continue;
                int w=0,h=0,ch=0;auto*pixels=stbi_load_from_memory(encoded,int(length),&w,&h,&ch,4);require(pixels);bool opaque=true;
                for(size_t p=3;p<size_t(w)*h*4;p+=4)opaque&=pixels[p]==255;
                GLuint original=GLH_texture_pixels_srgb(pixels,w,h,4);stbi_image_free(pixels);require(original);
                char leaf[32];std::snprintf(leaf,sizeof(leaf),"%016llx.vbc7",static_cast<unsigned long long>(hash));
                const auto cache_path=fs::path(world_model_texture_blocks::directory())/leaf;
                if(!fs::exists(cache_path)){
                    // A rejected precision candidate retains the original. Its
                    // absence must be a clean miss, never a corrupt upload or
                    // mutation of the current original-texture binding.
                    const auto before=world_model_texture_blocks::counters();
                    require(world_model_texture_blocks::upload(encoded,length)==0);
                    const auto after=world_model_texture_blocks::counters();
                    require(after.cache_misses==before.cache_misses+1&&after.rejected_cache==before.rejected_cache&&after.upload_failures==before.upload_failures);
                    GLint bound=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&bound);require(GLuint(bound)==original);
                    glDeleteTextures(1,&original);retained_originals++;continue;
                }
                world_texture_blocks::Image cache;std::string error;require(world_texture_blocks::read(cache_path.c_str(),encoded,length,cache,error));
                if(!opaque)utility_alpha_images++;
                if(texture!=mat.pbr_metallic_roughness.base_color_texture.texture&&texture!=mat.emissive_texture.texture)auxiliary_images++;
                GLuint sentinel=0;glGenBuffers(1,&sentinel);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,sentinel);glBufferData(GL_PIXEL_UNPACK_BUFFER,16,nullptr,GL_STATIC_DRAW);glPixelStorei(GL_UNPACK_ALIGNMENT,8);glPixelStorei(GL_UNPACK_ROW_LENGTH,7);
                const auto before=world_model_texture_blocks::counters();GLuint compressed=world_model_texture_blocks::upload(encoded,length);require(compressed);
                GLint bound=0,alignment=0,row_length=0;glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&bound);glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);glGetIntegerv(GL_UNPACK_ROW_LENGTH,&row_length);
                require(bound==GLint(sentinel)&&alignment==8&&row_length==7);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glPixelStorei(GL_UNPACK_ALIGNMENT,4);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glDeleteBuffers(1,&sentinel);
                require(world_model_texture_blocks::counters().cached_textures==before.cached_textures+1);
                for(size_t level=0;level<cache.levels.size();level++){
                    const auto&l=cache.levels[level];std::vector<unsigned char>a(size_t(l.width)*l.height*4),b(a.size());
                    glBindTexture(GL_TEXTURE_2D,original);glGetTexImage(GL_TEXTURE_2D,GLint(level),GL_RGBA,GL_UNSIGNED_BYTE,a.data());
                    glBindTexture(GL_TEXTURE_2D,compressed);GLint width=0,height=0,format=0,flag=0,bytes=0;
                    glGetTexLevelParameteriv(GL_TEXTURE_2D,GLint(level),GL_TEXTURE_WIDTH,&width);glGetTexLevelParameteriv(GL_TEXTURE_2D,GLint(level),GL_TEXTURE_HEIGHT,&height);glGetTexLevelParameteriv(GL_TEXTURE_2D,GLint(level),GL_TEXTURE_INTERNAL_FORMAT,&format);glGetTexLevelParameteriv(GL_TEXTURE_2D,GLint(level),GL_TEXTURE_COMPRESSED,&flag);glGetTexLevelParameteriv(GL_TEXTURE_2D,GLint(level),GL_TEXTURE_COMPRESSED_IMAGE_SIZE,&bytes);
                    require(width==int(l.width)&&height==int(l.height)&&flag&&format==GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM&&bytes==int(l.blocks.size()));
                    glGetTexImage(GL_TEXTURE_2D,GLint(level),GL_RGBA,GL_UNSIGNED_BYTE,b.data());require(glGetError()==GL_NO_ERROR);compare(a,b,level==0);mips++;reference_bytes+=a.size();block_bytes+=l.blocks.size();
                }
                if(!rejection_tested&&cache.levels.size()>1){
                    const auto temp=fs::temp_directory_path()/("valwood-cache-rejection-"+std::to_string(getpid()));fs::create_directories(temp);
                    const auto production=world_model_texture_blocks::directory();world_model_texture_blocks::directory()=temp.string();const auto badpath=temp/leaf;
                    // A checksum-valid cache retaining exactly the original encoded
                    // source, but deliberately discarding the original base mip.
                    auto resized=cache;resized.levels.erase(resized.levels.begin());resized.width=resized.levels.front().width;resized.height=resized.levels.front().height;resized.levels.front().rmse=0;
                    require(world_texture_blocks::write(badpath.c_str(),encoded,length,resized,error));world_texture_blocks::Image parsed;require(world_texture_blocks::read(badpath.c_str(),encoded,length,parsed,error));
                    glBindTexture(GL_TEXTURE_2D,original);const auto rejected=world_model_texture_blocks::counters().rejected_cache;
                    require(!world_model_texture_blocks::upload(encoded,length));glGetIntegerv(GL_TEXTURE_BINDING_2D,&bound);require(bound==GLint(original));require(world_model_texture_blocks::counters().rejected_cache==rejected+1);
                    require(world_texture_blocks::write(badpath.c_str(),encoded,length,cache,error));std::fstream file(badpath,std::ios::in|std::ios::out|std::ios::binary);file.seekp(-1,std::ios::end);file.put('\0');file.close();
                    // Deliberately corrupt the protected CRC field irrespective of
                    // whether the last compressed byte happened to already be0.
                    std::fstream crc(badpath,std::ios::in|std::ios::out|std::ios::binary);crc.seekg(56);char byte=0;crc.get(byte);crc.seekp(56);crc.put(char(byte^1));crc.close();
                    require(!world_model_texture_blocks::upload(encoded,length));glGetIntegerv(GL_TEXTURE_BINDING_2D,&bound);require(bound==GLint(original));
                    fs::remove(badpath);const auto missing=world_model_texture_blocks::counters().cache_misses;require(!world_model_texture_blocks::upload(encoded,length));require(world_model_texture_blocks::counters().cache_misses==missing+1);
                    world_model_texture_blocks::directory()=production;fs::remove(temp);rejection_tested=true;
                }
                glDeleteTextures(1,&compressed);glDeleteTextures(1,&original);images++;
            }
        }
        cgltf_free(data);require(fs::file_size(path)==size&&fs::last_write_time(path)==stamp);
    }
    // Exercise the actual production parent upload callback, not a copied
    // fallback: one failed compressed level must load the original RGBA image.
    const auto failures_before=world_model_texture_blocks::counters().upload_failures;
    inject_failure=true;
    auto*asset=world_model_load("projects/valwood_world/models/purchased_village_house_a_near.glb");
    require(asset&&injected_failures==1&&!inject_failure&&!waiting_for_original&&original_fallbacks==1);
    require(world_model_texture_blocks::counters().upload_failures==failures_before+1);
    require(glGetError()==GL_NO_ERROR&&original_fallback_texture&&glIsTexture(original_fallback_texture));
    glBindTexture(GL_TEXTURE_2D,original_fallback_texture);GLint format=0,compressed=1,width=0,height=0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&format);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_COMPRESSED,&compressed);
    glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&width);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&height);
    require(format==GL_SRGB8_ALPHA8&&!compressed&&width==failed_width&&height==failed_height&&glGetError()==GL_NO_ERROR);
    world_model_free(asset);require(!glIsTexture(original_fallback_texture)&&texture_cache_stats().entries==0);texture_cache_shutdown();
    std::puts("Production loader controlled compressed upload failure: full original RGBA8 fallback succeeded, upload_failures+1, no stale GL error, ownership released");
    require(images>=5&&mips>=50&&rejection_tested);require(reference_bytes>block_bytes&&glGetError()==GL_NO_ERROR);
    require(utility_alpha_images>0&&auxiliary_images>0&&retained_originals<=2);
    std::printf("Opaque utility-alpha images checked against original RGB mip chains: %u; facade colors:%u; rejected originals retained:%u\n",utility_alpha_images,auxiliary_images,retained_originals);
    std::printf("PASS %u images/%u full mips, %u checks, RGBA%llu BC7%llu bytes, resized/corrupt/missing rejection; source metadata unchanged\n",images,mips,checks,(unsigned long long)reference_bytes,(unsigned long long)block_bytes);
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
}

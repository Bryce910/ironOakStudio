#ifndef VALWOOD_WORLD_TEXTURE_BLOCKS_FORMAT_H
#define VALWOOD_WORLD_TEXTURE_BLOCKS_FORMAT_H
/* CPU-only, bounded opaque sRGB/linear BC7 cache. The filename fingerprint is only a
 * lookup key: every read compares the complete original encoded source bytes.
 * CRC64-ECMA detects accidental corruption, not an adversarial signature.
 * Quality fields certify the offline encoder's full-mip comparison; runtime
 * never encodes or changes image quality. alpha_max is max |alpha-255|, hence0.
 * All integers/IEEE754 float bits are explicitly little-endian on disk.
 */
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>
namespace world_texture_blocks {
inline constexpr std::uint32_t version=1,encoder_recipe=1,format_srgb_bc7=1,format_linear_bc7=2,format_normal_bc7=3;
inline constexpr float normal_angle_max=2.f,normal_rms_max=.75f;
inline bool linear_format(std::uint32_t format){return format==format_linear_bc7||format==format_normal_bc7;}
inline bool supported_format(std::uint32_t format){return format==format_srgb_bc7||linear_format(format);}
inline constexpr float linear_rmse_max=1.5f;
inline constexpr std::uint32_t linear_error_max=6;
inline constexpr std::size_t header_bytes=64,level_descriptor_bytes=40;
inline constexpr std::size_t max_encoded_bytes=64u*1024u*1024u,max_file_bytes=192u*1024u*1024u;
inline constexpr std::uint64_t max_pixels=64u*1024u*1024u;
inline constexpr std::uint32_t max_dimension=16384,max_levels=15;
// Nearest float32 to 255*10^(-38/20): compare float certificates against the
// same float bound in the cooker and parser, avoiding decimal-policy drift.
inline constexpr float base_rmse_max=3.2102598f;
struct Level {
    std::uint32_t width=0,height=0;
    std::vector<std::uint8_t> blocks;
    float rmse=0,bias[3]={};
    std::uint32_t alpha_max=0;
    std::uint32_t max_error=0;
};
struct Image {std::uint32_t width=0,height=0;std::vector<Level> levels;std::uint32_t format=format_srgb_bc7;float normal_max_degrees=0,normal_rms_degrees=0;};
namespace detail {
inline constexpr std::array<std::uint8_t,8> magic={'V','W','B','C','7',0,'\r','\n'};
inline bool fail(std::string &error,const char *message) noexcept {try{error=message;}catch(...){}return false;}
inline bool source_valid(const void *data,std::size_t length){return data&&length&&length<=max_encoded_bytes;}
inline bool dimensions(std::uint32_t w,std::uint32_t h){return w&&h&&w<=max_dimension&&h<=max_dimension&&std::uint64_t(w)*h<=max_pixels;}
inline unsigned chain_levels(std::uint32_t w,std::uint32_t h){unsigned n=1;while(w>1||h>1){w=std::max(1u,w/2);h=std::max(1u,h/2);n++;}return n;}
inline std::uint64_t block_bytes(std::uint32_t w,std::uint32_t h){return std::uint64_t((w+3)/4)*((h+3)/4)*16;}
inline bool quality(float rmse,const float bias[3],std::uint32_t alpha_max){
    if(!std::isfinite(rmse)||rmse<0||rmse>=10||alpha_max)return false;
    for(unsigned c=0;c<3;c++)if(!std::isfinite(bias[c])||std::abs(bias[c])>=3)return false;
    return true;
}
inline bool linear_quality(float rmse,const float bias[3],std::uint32_t maximum){
    if(!std::isfinite(rmse)||rmse<0||rmse>linear_rmse_max||maximum>linear_error_max)return false;
    for(unsigned i=0;i<3;i++)if(!std::isfinite(bias[i])||std::abs(bias[i])>1.5f)return false;
    return true;
}
inline bool block_modes(const std::uint8_t *bytes,std::size_t length){
    // Recipe1 emits mode1 (RGB-only, implicitly opaque) or mode6. Mode6 has
    // seven unary mode bits, then 7-bit R/G/B/A endpoint pairs and two P bits.
    // Alpha endpoints bits49..62 plus P bits63..64 must all be1, guaranteeing
    // alpha255 for every index, independently of the quality certificate.
    for(std::size_t offset=0;offset<length;offset+=16){
        const auto *p=bytes+offset;
        if((p[0]&3)==2)continue;
        if((p[0]&0x7f)!=0x40||(p[6]&0xfe)!=0xfe||p[7]!=0xff||(p[8]&1)!=1)return false;
    }
    return true;
}
inline std::uint32_t get32(const std::uint8_t *p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);}
inline std::uint64_t get64(const std::uint8_t *p){return get32(p)|(std::uint64_t(get32(p+4))<<32);}
inline float get_float(const std::uint8_t *p){const auto bits=get32(p);float value=0;std::memcpy(&value,&bits,4);return value;}
inline void put32(std::uint8_t *p,std::uint32_t n){for(unsigned i=0;i<4;i++)p[i]=std::uint8_t(n>>(i*8));}
inline void put64(std::uint8_t *p,std::uint64_t n){for(unsigned i=0;i<8;i++)p[i]=std::uint8_t(n>>(i*8));}
inline void put_float(std::uint8_t *p,float f){std::uint32_t bits=0;std::memcpy(&bits,&f,4);put32(p,bits);}
inline constexpr std::array<std::uint64_t,256> make_crc_table(){
    std::array<std::uint64_t,256> table={};
    for(std::size_t i=0;i<table.size();i++){
        std::uint64_t crc=std::uint64_t(i)<<56;
        for(unsigned bit=0;bit<8;bit++)crc=(crc<<1)^((crc>>63)?0x42f0e1eba9ea3693ull:0);
        table[i]=crc;
    }return table;
}
inline constexpr auto crc_table=make_crc_table();
inline std::uint64_t crc64(const std::uint8_t *bytes,std::size_t length,bool file){
    std::uint64_t crc=0;
    for(std::size_t i=0;i<length;i++){
        const auto byte=file&&i>=56&&i<64?0:bytes[i];
        crc=(crc<<8)^crc_table[std::uint8_t((crc>>56)^byte)];
    }return crc;
}
// Normal format stores conservative 1/1024 degree upper bounds in flags:
// bit0 opaque, bit1 angular certificate, bits2..13 maximum, bits14..25
// maximum per-mip RMS; bits26..31 reserved. Existing formats keep flags1.
inline bool normal_quality(float maximum,float rms){return std::isfinite(maximum)&&std::isfinite(rms)&&maximum>=0&&rms>=0&&maximum<=normal_angle_max&&rms<=normal_rms_max;}
inline std::uint32_t image_flags(const Image &image){
    if(image.format!=format_normal_bc7)return 1;
    return 3u|(std::uint32_t(std::ceil(double(image.normal_max_degrees)*1024))<<2)|(std::uint32_t(std::ceil(double(image.normal_rms_degrees)*1024))<<14);
}
inline bool valid_flags(std::uint32_t format,std::uint32_t flags){
    return format==format_normal_bc7?((flags&3u)==3u&&!(flags&0xfc000000u)&&((flags>>2)&4095u)<=2048u&&((flags>>14)&4095u)<=768u):flags==1;
}
inline bool validate_image(const Image &image,std::string &error){
    if(!supported_format(image.format))return fail(error,"Unsupported image format");
    if(image.format==format_normal_bc7&&!normal_quality(image.normal_max_degrees,image.normal_rms_degrees))return fail(error,"Normal angular precision certificate failed");
    if(!dimensions(image.width,image.height))return fail(error,"Cache dimensions exceed budget");
    const auto count=chain_levels(image.width,image.height);
    if(count>max_levels||image.levels.size()!=count)return fail(error,"Cache must contain the exact full mip chain");
    if(image.levels.front().rmse>base_rmse_max)return fail(error,"Base mip falls below recipe PSNR38 requirement");
    auto w=image.width,h=image.height;
    for(const auto &l:image.levels){
        if(l.width!=w||l.height!=h||l.blocks.size()!=block_bytes(w,h))return fail(error,"Invalid mip dimensions or block length");
        if(!quality(l.rmse,l.bias,l.alpha_max))return fail(error,"Mip quality or opaque-alpha certificate failed");
        if(linear_format(image.format)&&!linear_quality(l.rmse,l.bias,l.max_error))return fail(error,"Linear data precision certificate failed");
        if(image.format==format_srgb_bc7&&l.max_error)return fail(error,"Reserved color certificate must be zero");
        if(!block_modes(l.blocks.data(),l.blocks.size()))return fail(error,"BC7 block violates opaque encoder recipe");
        w=std::max(1u,w/2);h=std::max(1u,h/2);
    }return true;
}
}
static_assert(sizeof(float)==4&&std::numeric_limits<float>::is_iec559,"Cache requires IEEE754 float32");
inline std::uint64_t fingerprint(const void *encoded,std::size_t length){
    if(!encoded&&length)return 0;
    std::uint64_t hash=14695981039346656037ull;const auto *p=static_cast<const std::uint8_t *>(encoded);
    for(std::size_t i=0;i<length;i++){hash^=p[i];hash*=1099511628211ull;}return hash;
}
/* Exposed for deterministic cooker/test diagnostics. file_checksum treats the
 * stored checksum field [56,64) as zero, covering all other file bytes. */
inline std::uint64_t checksum(const void *bytes,std::size_t length){return bytes?detail::crc64(static_cast<const std::uint8_t *>(bytes),length,false):0;}
inline std::uint64_t file_checksum(const void *bytes,std::size_t length){return bytes?detail::crc64(static_cast<const std::uint8_t *>(bytes),length,true):0;}
inline bool serialize(const void *encoded,std::size_t length,const Image &image,
                      std::vector<std::uint8_t> &output,std::string &error){
    error.clear();
    if(!detail::source_valid(encoded,length))return detail::fail(error,"Invalid or oversized original encoded source");
    if(!detail::validate_image(image,error))return false;
    std::uint64_t total=header_bytes+image.levels.size()*level_descriptor_bytes+length;
    for(const auto &l:image.levels)total+=l.blocks.size();
    if(total>max_file_bytes)return detail::fail(error,"Cache file exceeds budget");
    try{
        std::vector<std::uint8_t> next(std::size_t(total),0);std::copy(detail::magic.begin(),detail::magic.end(),next.begin());
        detail::put32(next.data()+8,version);detail::put32(next.data()+12,encoder_recipe);detail::put32(next.data()+16,image.format);
        detail::put32(next.data()+20,header_bytes);detail::put32(next.data()+24,image.width);detail::put32(next.data()+28,image.height);
        detail::put32(next.data()+32,std::uint32_t(image.levels.size()));detail::put32(next.data()+36,detail::image_flags(image));
        detail::put64(next.data()+40,length);detail::put64(next.data()+48,total);
        std::size_t at=header_bytes;
        for(const auto &l:image.levels){
            auto *p=next.data()+at;detail::put32(p,l.width);detail::put32(p+4,l.height);detail::put64(p+8,l.blocks.size());
            detail::put_float(p+16,l.rmse);for(unsigned c=0;c<3;c++)detail::put_float(p+20+c*4,l.bias[c]);detail::put32(p+32,l.alpha_max);
            detail::put32(p+36,l.max_error);
            at+=level_descriptor_bytes;
        }
        std::memcpy(next.data()+at,encoded,length);at+=length;
        for(const auto &l:image.levels){std::copy(l.blocks.begin(),l.blocks.end(),next.begin()+std::ptrdiff_t(at));at+=l.blocks.size();}
        detail::put64(next.data()+56,file_checksum(next.data(),next.size()));output=std::move(next);return true;
    }catch(...){return detail::fail(error,"Cache serialization allocation failed");}
}
/* Parse compares original source identity before allocating output block data.
 * Every failure preserves the caller's previous Image, including allocation or
 * I/O errors. No casts into unaligned/untrusted byte storage are used. */
inline bool parse(const void *file,std::size_t size,const void *encoded,std::size_t length,
                  Image &output,std::string &error,std::uint32_t expected_format=format_srgb_bc7){
    error.clear();
    if(!file||size<header_bytes||size>max_file_bytes)return detail::fail(error,"Invalid or oversized cache file");
    if(!detail::source_valid(encoded,length))return detail::fail(error,"Invalid or oversized original encoded source");
    const auto *bytes=static_cast<const std::uint8_t *>(file);
    if(std::memcmp(bytes,detail::magic.data(),detail::magic.size()))return detail::fail(error,"Invalid cache magic");
    if(!supported_format(expected_format)||detail::get32(bytes+8)!=version||detail::get32(bytes+12)!=encoder_recipe||detail::get32(bytes+16)!=expected_format||
       detail::get32(bytes+20)!=header_bytes||!detail::valid_flags(expected_format,detail::get32(bytes+36)))return detail::fail(error,"Unsupported cache version, recipe, format or flags");
    const auto width=detail::get32(bytes+24),height=detail::get32(bytes+28),count=detail::get32(bytes+32);
    if(!detail::dimensions(width,height))return detail::fail(error,"Cache dimensions exceed budget");
    if(!count||count>max_levels||count!=detail::chain_levels(width,height))return detail::fail(error,"Cache must contain the exact full mip chain");
    if(detail::get64(bytes+40)!=length||detail::get64(bytes+48)!=size)return detail::fail(error,"Cache source/file length mismatch");
    const std::size_t source_at=header_bytes+std::size_t(count)*level_descriptor_bytes;
    if(source_at>size||length>size-source_at)return detail::fail(error,"Truncated cache source or mip table");
    if(std::memcmp(bytes+source_at,encoded,length))return detail::fail(error,"Original encoded source identity mismatch");
    std::uint64_t expected=std::uint64_t(source_at)+length;auto w=width,h=height;
    for(std::uint32_t n=0;n<count;n++){
        const auto *p=bytes+header_bytes+std::size_t(n)*level_descriptor_bytes;
        if(detail::get32(p)!=w||detail::get32(p+4)!=h||detail::get64(p+8)!=detail::block_bytes(w,h)||(expected_format==format_srgb_bc7&&detail::get32(p+36)))
            return detail::fail(error,"Invalid mip dimensions, block length or reserved field");
        float bias[3];for(unsigned c=0;c<3;c++)bias[c]=detail::get_float(p+20+c*4);
        if(!detail::quality(detail::get_float(p+16),bias,detail::get32(p+32)))return detail::fail(error,"Mip quality or opaque-alpha certificate failed");
        if(linear_format(expected_format)&&!detail::linear_quality(detail::get_float(p+16),bias,detail::get32(p+36)))return detail::fail(error,"Linear data precision certificate failed");
        if(!n&&detail::get_float(p+16)>base_rmse_max)return detail::fail(error,"Base mip falls below recipe PSNR38 requirement");
        expected+=detail::block_bytes(w,h);w=std::max(1u,w/2);h=std::max(1u,h/2);
    }
    if(expected!=size)return detail::fail(error,"Truncated cache blocks or trailing bytes");
    if(detail::get64(bytes+56)!=file_checksum(bytes,size))return detail::fail(error,"Cache checksum mismatch");
    std::size_t at=source_at+length;w=width;h=height;
    for(std::uint32_t n=0;n<count;n++){
        const auto block_size=std::size_t(detail::block_bytes(w,h));
        if(!detail::block_modes(bytes+at,block_size))return detail::fail(error,"BC7 block violates opaque encoder recipe");
        at+=block_size;w=std::max(1u,w/2);h=std::max(1u,h/2);
    }
    try{
        Image next;next.width=width;next.height=height;next.format=expected_format;
        if(expected_format==format_normal_bc7){const auto flags=detail::get32(bytes+36);next.normal_max_degrees=float((flags>>2)&4095u)/1024;next.normal_rms_degrees=float((flags>>14)&4095u)/1024;}
        next.levels.reserve(count);at=source_at+length;
        for(std::uint32_t n=0;n<count;n++){
            const auto *p=bytes+header_bytes+std::size_t(n)*level_descriptor_bytes;Level l;l.width=detail::get32(p);l.height=detail::get32(p+4);
            l.rmse=detail::get_float(p+16);for(unsigned c=0;c<3;c++)l.bias[c]=detail::get_float(p+20+c*4);l.alpha_max=detail::get32(p+32);
            l.max_error=detail::get32(p+36);
            const auto block_size=std::size_t(detail::get64(p+8));l.blocks.assign(bytes+at,bytes+at+block_size);at+=block_size;next.levels.push_back(std::move(l));
        }
        output=std::move(next);return true;
    }catch(...){return detail::fail(error,"Cache parsing allocation failed");}
}
inline bool read(const char *path,const void *encoded,std::size_t length,Image &output,std::string &error,std::uint32_t expected_format=format_srgb_bc7){
    error.clear();
    if(!path||!*path||!detail::source_valid(encoded,length))return detail::fail(error,"Invalid cache path or encoded source");
    try{
        std::ifstream input(path,std::ios::binary|std::ios::ate);if(!input)return detail::fail(error,"Cache file unavailable");
        const auto end=input.tellg();if(end<std::streamoff(header_bytes)||end>std::streamoff(max_file_bytes))return detail::fail(error,"Invalid or oversized cache file");
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));input.seekg(0);
        if(!input.read(reinterpret_cast<char *>(bytes.data()),std::streamsize(bytes.size())))return detail::fail(error,"Truncated cache file read");
        if(input.peek()!=std::char_traits<char>::eof())return detail::fail(error,"Cache grew or contains trailing bytes");
        return parse(bytes.data(),bytes.size(),encoded,length,output,error,expected_format);
    }catch(...){return detail::fail(error,"Cache read allocation or I/O failed");}
}
/* Commit a complete file by sibling-temp + rename. Failed writes remove their
 * private temporary and preserve an existing destination. No runtime hot draw
 * path calls this; the offline cooker alone writes cache files. */
inline bool write(const char *path,const void *encoded,std::size_t length,const Image &image,std::string &error){
    error.clear();if(!path||!*path)return detail::fail(error,"Invalid cache output path");
    std::vector<std::uint8_t> bytes;if(!serialize(encoded,length,image,bytes,error))return false;
    try{
        static std::atomic<std::uint64_t> sequence{0};std::string temporary;FILE *file=nullptr;
        for(unsigned attempt=0;attempt<16&&!file;attempt++){
            const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
            temporary=std::string(path)+".tmp."+std::to_string(stamp)+"."+std::to_string(sequence.fetch_add(1));
            file=std::fopen(temporary.c_str(),"wbx");
        }
        if(!file)return detail::fail(error,"Cannot create cache temporary file");
        bool okay=std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size();if(std::fflush(file)!=0)okay=false;
        if(std::fclose(file)!=0)okay=false;
        if(!okay){std::remove(temporary.c_str());return detail::fail(error,"Cache temporary write failed");}
        if(std::rename(temporary.c_str(),path)!=0){std::remove(temporary.c_str());return detail::fail(error,"Cache atomic rename failed");}
        return true;
    }catch(...){return detail::fail(error,"Cache write allocation failed");}
}
}
#endif

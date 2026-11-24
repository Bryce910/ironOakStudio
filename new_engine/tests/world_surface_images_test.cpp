#include "engine/world_surface_images.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace world_surface;
int main(){
 std::string error;PackedImage im;std::vector<std::uint8_t> pixels(16*16*4,255);
 for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++)for(unsigned k=0;k<3;k++)pixels[(y*16+x)*4+k]=((x+y)%2)*255;
 assert(pack_image(pixels.data(),16,16,ImageRole::Color,im,error));
 auto p=sample_image(im,.2,.3,9);assert(std::abs(p[0]-.5)<.012&&p[3]==1);
 for(float lod:{0.f,.2f,1.f,1.5f,2.f,7.f,9.f})for(float v:{0.f,.49f,.5f,.99f}){
  auto a=sample_image(im,.5f-1e-6f,v,lod),b=sample_image(im,.5f+1e-6f,v,lod);
  auto c=sample_image(im,-1e-6f,v,lod),d=sample_image(im,1.f-1e-6f,v,lod);
  for(unsigned k=0;k<4;k++){assert(std::abs(a[k]-b[k])<.001);assert(std::abs(c[k]-d[k])<.001);}
 }
 auto rects=image_rects();for(unsigned i=1;i<image_mips;i++){auto r=rects[i];assert(r.x>=1&&r.y>=1&&r.x+r.side<array_side&&r.y+r.side<array_side);for(unsigned j=1;j<i;j++){auto s=rects[j];if(r.layer!=s.layer)continue;assert(r.x+r.side+1<=s.x-1||s.x+s.side+1<=r.x-1||r.y+r.side+1<=s.y-1||s.y+s.side+1<=r.y-1);}}
 const auto old=im.rgba.size();assert(!pack_image(nullptr,16,16,ImageRole::Color,im,error)&&im.rgba.size()==old);
 pixels.assign(16*16*4,255);for(unsigned i=0;i<256;i++){pixels[i*4]=128;pixels[i*4+1]=128;pixels[i*4+2]=255;}
 assert(pack_image(pixels.data(),16,16,ImageRole::Normal,im,error));p=sample_image(im,.5,.5,4.3f);assert(p[2]>.99&&std::abs(p[0]-.5)<.005);
 for(unsigned i=0;i<256;i++){pixels[i*4]=(i%2)?204:51;pixels[i*4+1]=128;pixels[i*4+2]=230;}
 assert(pack_image(pixels.data(),16,16,ImageRole::Normal,im,error));p=sample_image(im,.5,.5,9);assert(p[2]>.89&&p[2]<.92); // Renormalizing the mean would incorrectly restore a sharp specular lobe.
 std::puts("PASS surface images: wrapped quadrant seams, mip tail packing, linear-light color reduction, normal variance preservation and invalid-input rollback");
}

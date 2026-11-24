#include "engine/world_surface_images.h"
#include <algorithm>
#include <cmath>
namespace world_surface {
namespace {
using Pixel=std::array<float,4>;
float linear(float x){return x<=.04045f?x/12.92f:std::pow((x+.055f)/1.055f,2.4f);}
float srgb(float x){return x<=.0031308f?12.92f*x:1.055f*std::pow(x,1.f/2.4f)-.055f;}
int wrap(int x,int size){return (x%size+size)%size;}
Pixel read(const std::uint8_t *data,unsigned width,unsigned height,int x,int y,ImageRole role){
 const auto *p=data+(size_t(wrap(y,int(height)))*width+wrap(x,int(width)))*4;Pixel out;
 for(unsigned k=0;k<4;k++)out[k]=p[k]/255.f;
 if(role==ImageRole::Color)for(unsigned k=0;k<3;k++)out[k]=linear(out[k]);
 return out;
}
Pixel interpolate(Pixel a,Pixel b,float t){for(unsigned k=0;k<4;k++)a[k]+=(b[k]-a[k])*t;return a;}
Pixel bilinear(const std::uint8_t *data,unsigned width,unsigned height,double x,double y,ImageRole role){
 int ix=int(std::floor(x)),iy=int(std::floor(y));float tx=float(x-ix),ty=float(y-iy);
 return interpolate(interpolate(read(data,width,height,ix,iy,role),read(data,width,height,ix+1,iy,role),tx),interpolate(read(data,width,height,ix,iy+1,role),read(data,width,height,ix+1,iy+1,role),tx),ty);
}
void encode(Pixel p,ImageRole role,std::uint8_t *out){
 // Preserve the mean normal length: the shader converts lost high-frequency
 // variance into roughness before normalizing the shading direction.
 if(role==ImageRole::Color)for(unsigned k=0;k<3;k++)p[k]=srgb(std::clamp(p[k],0.f,1.f));
 for(unsigned k=0;k<4;k++)out[k]=std::uint8_t(std::lround(std::clamp(p[k],0.f,1.f)*255));
}
void blit(PackedImage &out,const std::vector<std::uint8_t> &level,unsigned width,unsigned x,unsigned y,unsigned side,MipRect rect){
 for(int z=-1;z<=int(side);z++)for(int u=-1;u<=int(side);u++){
  const auto *src=level.data()+(size_t(wrap(int(y)+z,int(width)))*width+wrap(int(x)+u,int(width)))*4;
  auto *dst=out.rgba.data()+((size_t(rect.layer)*array_side+rect.y+z)*array_side+rect.x+u)*4;
  std::copy_n(src,4,dst);
 }
}
}
std::array<MipRect,image_mips> image_rects(){
 std::array<MipRect,image_mips> out={};out[0]={1,1,510,0};out[1]={1,1,510,4};
 unsigned x=0,y=0,row=0,side=255;
 for(unsigned i=2;i<image_mips;i++){
  const unsigned box=side+2;if(x+box>array_side){x=0;y+=row;row=0;}
  out[i]={x+1,y+1,side,5};x+=box;row=std::max(row,box);side=std::max(1u,side/2);
 }
 return out;
}
bool pack_paint(const PaintField &field,std::vector<std::uint8_t> &out,std::string &error){
 if(field.tiles.size()>paint_tile_limit){error="Invalid paint tile budget";return false;}
 if(field.tiles.empty()){out.clear();error.clear();return true;}
 std::vector<std::uint8_t> pixels(size_t(array_side)*array_side*4*4);
 for(unsigned i=0;i<field.tiles.size();i++){
  const auto &t=field.tiles[i];unsigned bank=i/32,local=i%32,x0=(local%7)*paint_side,y0=(local/7)*paint_side;
  if(t.x< -32768||t.x>32767||t.z< -32768||t.z>32767){error="Invalid paint tile coordinate";return false;}
  for(unsigned z=0;z<paint_side;z++)for(unsigned x=0;x<paint_side;x++)for(unsigned k=0;k<layer_limit;k++)pixels[((size_t(bank*2+k/4)*array_side+y0+z)*array_side+x0+x)*4+k%4]=t.weights[(z*paint_side+x)*layer_limit+k];
 }
 for(unsigned i=0;i<paint_hash_size;i++){
  const auto &h=field.lookup[i];auto *p=pixels.data()+(size_t(512)*array_side+i*2)*4;
  if(h[2]<0)continue;
  if(unsigned(h[2])>=field.tiles.size()||field.tiles[unsigned(h[2])].x!=h[0]||field.tiles[unsigned(h[2])].z!=h[1]){error="Invalid paint hash entry";return false;}
  unsigned x=unsigned(h[0]+32768),z=unsigned(h[1]+32768);p[0]=x&255;p[1]=(x>>8)&255;p[2]=z&255;p[3]=(z>>8)&255;p[4]=std::uint8_t(h[2]+1);
 }
 out=std::move(pixels);error.clear();return true;
}
bool pack_image(const std::uint8_t *source,unsigned width,unsigned height,ImageRole role,PackedImage &out,std::string &error){
 if(!source||width<1||height<1||width>8192||height>8192||unsigned(role)>2){error="Invalid surface image or role (maximum8192px)";return false;}
 PackedImage next;next.role=role;next.rgba.resize(size_t(image_layers)*array_side*array_side*4);
 std::vector<std::uint8_t> level(size_t(image_side)*image_side*4);
 for(unsigned y=0;y<image_side;y++)for(unsigned x=0;x<image_side;x++)encode(bilinear(source,width,height,(x+.5)*width/image_side-.5,(y+.5)*height/image_side-.5,role),role,level.data()+(size_t(y)*image_side+x)*4);
 auto rects=image_rects();
 for(unsigned z=0;z<2;z++)for(unsigned x=0;x<2;x++)blit(next,level,image_side,x*510,z*510,510,{1,1,510,z*2+x});
 unsigned side=image_side;
 for(unsigned mip=1;mip<image_mips;mip++){
  unsigned size=std::max(1u,side/2);std::vector<std::uint8_t> down(size_t(size)*size*4);
  for(unsigned y=0;y<size;y++)for(unsigned x=0;x<size;x++){
   // Exact area average includes the last row/column for odd mip sizes.
   const double x0=double(x)*side/size,x1=double(x+1)*side/size,y0=double(y)*side/size,y1=double(y+1)*side/size;
   Pixel mean={};double weight=0;
   for(int yy=int(y0);yy<int(std::ceil(y1));yy++)for(int xx=int(x0);xx<int(std::ceil(x1));xx++){
    double a=(std::min(x1,double(xx+1))-std::max(x0,double(xx)))*(std::min(y1,double(yy+1))-std::max(y0,double(yy)));
    auto p=read(level.data(),side,side,xx,yy,role);for(unsigned k=0;k<4;k++)mean[k]+=float(p[k]*a);weight+=a;
   }
   for(float &v:mean)v=float(v/weight);
   encode(mean,role,down.data()+(size_t(y)*size+x)*4);
  }
  level.swap(down);side=size;blit(next,level,side,0,0,side,rects[mip]);
 }
 out=std::move(next);error.clear();return true;
}
std::array<float,4> sample_image(const PackedImage &im,float u,float v,float lod){
 if(im.rgba.size()!=size_t(image_layers)*array_side*array_side*4||!std::isfinite(u)||!std::isfinite(v)||!std::isfinite(lod))return {};
 auto rects=image_rects();u-=std::floor(u);v-=std::floor(v);lod=std::clamp(lod,0.f,float(image_mips-1));
 auto sample=[&](unsigned mip){auto rect=rects[mip];double x,y;
  if(mip==0){unsigned tx=unsigned(u*2),ty=unsigned(v*2);rect.layer=ty*2+tx;x=u*image_side-tx*510-.5;y=v*image_side-ty*510-.5;}
  else{x=u*rect.side-.5;y=v*rect.side-.5;}
  const auto *data=im.rgba.data()+size_t(rect.layer)*array_side*array_side*4;
  // GPU linear RGBA8 sampling decodes color after sampling, consistently.
  auto result=bilinear(data,array_side,array_side,rect.x+x,rect.y+y,ImageRole::ARM);
  if(im.role==ImageRole::Color)for(unsigned k=0;k<3;k++)result[k]=linear(result[k]);
  return result;
 };
 unsigned first=unsigned(lod);auto result=interpolate(sample(first),sample(std::min(first+1,image_mips-1)),lod-first);
 return result;
}
}

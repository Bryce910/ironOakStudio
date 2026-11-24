#include <algorithm>
#include <cmath>
// Bake the supplied secondary UVs into a shared primary-UV material atlas.
// First coverage wins: overlapping copies of a construction piece share grain.
extern "C" void nature_uv_raster(const float *vertices,const int *indices,int count,int size,float *map){
 for(int t=0;t<count;t+=3){
  const float *a=vertices+indices[t]*14,*b=vertices+indices[t+1]*14,*c=vertices+indices[t+2]*14;
  float minx=std::min({a[6],b[6],c[6]}),maxx=std::max({a[6],b[6],c[6]});
  float miny=std::min({a[7],b[7],c[7]}),maxy=std::max({a[7],b[7],c[7]});
  if(maxx-minx>16||maxy-miny>16)continue;
  float determinant=(b[7]-c[7])*(a[6]-c[6])+(c[6]-b[6])*(a[7]-c[7]);
  if(std::abs(determinant)<1e-12f)continue;
  for(int ox=int(std::floor(minx));ox<=int(std::floor(maxx));ox++)for(int oy=int(std::floor(miny));oy<=int(std::floor(maxy));oy++){
   int x0=std::max(0,int(std::floor((minx-ox)*size))),x1=std::min(size-1,int(std::ceil((maxx-ox)*size)));
   int y0=std::max(0,int(std::floor((1-maxy+oy)*size))),y1=std::min(size-1,int(std::ceil((1-miny+oy)*size)));
   for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
    float *dst=map+(y*size+x)*2;if(std::isfinite(dst[0]))continue;
    float u=(x+.5f)/size+ox,v=1-(y+.5f)/size+oy;
    float wa=((b[7]-c[7])*(u-c[6])+(c[6]-b[6])*(v-c[7]))/determinant;
    float wb=((c[7]-a[7])*(u-c[6])+(a[6]-c[6])*(v-c[7]))/determinant,wc=1-wa-wb;
    if(wa<-.0001f||wb<-.0001f||wc<-.0001f)continue;
    dst[0]=a[8]*wa+b[8]*wb+c[8]*wc;dst[1]=a[9]*wa+b[9]*wb+c[9]*wc;
   }
  }
 }
}

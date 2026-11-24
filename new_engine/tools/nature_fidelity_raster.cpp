#include <algorithm>
#include <cmath>

// Static bake helper, isolated from the shipping material cooker. The input is
// NativeCitadelExport's p/n/UV0/UV3/color stream. The output is UV3 plus the
// tangent-plane transform from UV3 into UV0. First coverage wins as in the
// baseline cooker; UV-overlap ambiguity remains explicitly reported.
namespace {
struct V { float x,y,z; };
V operator+(V a,V b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
V operator-(V a,V b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
V operator*(V a,float b){return {a.x*b,a.y*b,a.z*b};}
float dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
V cross(V a,V b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
V norm(V a){return a*(1.f/std::sqrt(std::max(dot(a,a),1e-20f)));}
V xyz(const float *a,int o=0){return {a[o],a[o+1],a[o+2]};}
bool derivatives(const float *a,const float *b,const float *c,int uv,V &u,V &v){
 const float du=b[uv]-a[uv],dv=b[uv+1]-a[uv+1],eu=c[uv]-a[uv],ev=c[uv+1]-a[uv+1];
 const float d=du*ev-dv*eu;if(std::abs(d)<1e-12f)return false;
 u=((xyz(b)-xyz(a))*ev-(xyz(c)-xyz(a))*dv)*(1.f/d);
 v=((xyz(c)-xyz(a))*du-(xyz(b)-xyz(a))*eu)*(1.f/d);return true;
}
void basis(V n,V du,V dv,V &t,V &b){
 t=norm(du-n*dot(n,du));b=cross(n,t);if(dot(b,dv)<0)b=b*-1;
}
}
extern "C" void nature_fidelity_raster(const float *vertices,const int *indices,int count,int size,float *map){
 for(int k=0;k<count;k+=3){
  const float *a=vertices+indices[k]*14,*b=vertices+indices[k+1]*14,*c=vertices+indices[k+2]*14;
  const float minx=std::min({a[6],b[6],c[6]}),maxx=std::max({a[6],b[6],c[6]});
  const float miny=std::min({a[7],b[7],c[7]}),maxy=std::max({a[7],b[7],c[7]});
  if(maxx-minx>16||maxy-miny>16)continue;
  const float d=(b[7]-c[7])*(a[6]-c[6])+(c[6]-b[6])*(a[7]-c[7]);
  if(std::abs(d)<1e-12f)continue;
  V du0,dv0,du3,dv3;if(!derivatives(a,b,c,6,du0,dv0))continue;
  if(!derivatives(a,b,c,8,du3,dv3)){du3=du0;dv3=dv0;}
  for(int ox=int(std::floor(minx));ox<=int(std::floor(maxx));ox++)for(int oy=int(std::floor(miny));oy<=int(std::floor(maxy));oy++){
   const int x0=std::max(0,int(std::floor((minx-ox)*size))),x1=std::min(size-1,int(std::ceil((maxx-ox)*size)));
   const int y0=std::max(0,int(std::floor((1-maxy+oy)*size))),y1=std::min(size-1,int(std::ceil((1-miny+oy)*size)));
   for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
    float *dst=map+(y*size+x)*6;if(std::isfinite(dst[0]))continue;
    const float u=(x+.5f)/size+ox,v=1-(y+.5f)/size+oy;
    const float wa=((b[7]-c[7])*(u-c[6])+(c[6]-b[6])*(v-c[7]))/d;
    const float wb=((c[7]-a[7])*(u-c[6])+(a[6]-c[6])*(v-c[7]))/d,wc=1-wa-wb;
    if(wa<-.0001f||wb<-.0001f||wc<-.0001f)continue;
    dst[0]=a[8]*wa+b[8]*wb+c[8]*wc;dst[1]=a[9]*wa+b[9]*wb+c[9]*wc;
    V n=norm(xyz(a,3)*wa+xyz(b,3)*wb+xyz(c,3)*wc),t0,b0,t3,b3;
    basis(n,du0,dv0,t0,b0);basis(n,du3,dv3,t3,b3);
    dst[2]=dot(t3,t0);dst[3]=dot(b3,t0);dst[4]=dot(t3,b0);dst[5]=dot(b3,b0);
   }
  }
 }
}

#include <algorithm>
#include <cmath>
struct P {float x,y,z;};
static float segment(float x,float z,const P &a,const P &b){float dx=b.x-a.x,dz=b.z-a.z;float t=std::clamp(((x-a.x)*dx+(z-a.z)*dz)/std::max(dx*dx+dz*dz,1e-12f),0.f,1.f);return std::hypot(x-a.x-dx*t,z-a.z-dz*t);}
extern "C" void nature_collision_raster(const float *p,const int *idx,int count,float xmin,float zmin,float cell,int width,int depth,float low,float high,unsigned char *out){
 for(int i=0;i<count;i+=3){P a[8],b[8];for(int j=0;j<3;j++){const float *v=p+idx[i+j]*3;a[j]={v[0],v[1],v[2]};}
  float nx=(a[1].y-a[0].y)*(a[2].z-a[0].z)-(a[1].z-a[0].z)*(a[2].y-a[0].y),ny=(a[1].z-a[0].z)*(a[2].x-a[0].x)-(a[1].x-a[0].x)*(a[2].z-a[0].z),nz=(a[1].x-a[0].x)*(a[2].y-a[0].y)-(a[1].y-a[0].y)*(a[2].x-a[0].x);
  if(std::abs(ny)>.75f*std::sqrt(nx*nx+ny*ny+nz*nz))continue;
  int n=3;
  for(int side=0;side<2;side++){
   int next=0;float plane=side?high:low;for(int k=0;k<n;k++){
    P v=a[k],w=a[(k+1)%n];bool inside=side?v.y<=plane:v.y>=plane,other=side?w.y<=plane:w.y>=plane;
    if(inside)b[next++]=v;if(inside!=other){float t=(plane-v.y)/(w.y-v.y);b[next++]={v.x+t*(w.x-v.x),plane,v.z+t*(w.z-v.z)};}
   }n=next;for(int k=0;k<n;k++)a[k]=b[k];if(!n)break;
  }if(n<2)continue;
  float x0=a[0].x,x1=x0,z0=a[0].z,z1=z0;for(int k=1;k<n;k++){x0=std::min(x0,a[k].x);x1=std::max(x1,a[k].x);z0=std::min(z0,a[k].z);z1=std::max(z1,a[k].z);}
  int ax=std::max(0,int(std::floor((x0-xmin)/cell))-1),bx=std::min(width-1,int(std::floor((x1-xmin)/cell))+1),az=std::max(0,int(std::floor((z0-zmin)/cell))-1),bz=std::min(depth-1,int(std::floor((z1-zmin)/cell))+1);
  for(int z=az;z<=bz;z++)for(int x=ax;x<=bx;x++){
   float px=xmin+(x+.5f)*cell,pz=zmin+(z+.5f)*cell;bool hit=false,inside=false;
   for(int k=0;k<n;k++){P v=a[k],w=a[(k+1)%n];if(segment(px,pz,v,w)<cell*.71f)hit=true;if((v.z>pz)!=(w.z>pz)&&px<(w.x-v.x)*(pz-v.z)/(w.z-v.z)+v.x)inside=!inside;}
   if(hit||inside)out[z*width+x]=1;
  }
 }
}

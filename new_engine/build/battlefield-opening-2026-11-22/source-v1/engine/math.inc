#ifndef ENGINE_MATH_INC
#define ENGINE_MATH_INC

#include <math.h>
#include "ufbx.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct { float x,y,z; } Vec3;
typedef struct { float m[16]; } Mat4;

static inline float clampf(float v,float lo,float hi){return v<lo?lo:(v>hi?hi:v);}
static inline float vec3_len(Vec3 v){return sqrtf(v.x*v.x+v.y*v.y+v.z*v.z);}
static inline Vec3  vec3_norm(Vec3 v){float l=vec3_len(v);if(l<1e-6f)return (Vec3){0,0,0};return (Vec3){v.x/l,v.y/l,v.z/l};}
static inline Vec3  vec3_sub(Vec3 a,Vec3 b){return (Vec3){a.x-b.x,a.y-b.y,a.z-b.z};}
static inline Vec3  vec3_add(Vec3 a,Vec3 b){return (Vec3){a.x+b.x,a.y+b.y,a.z+b.z};}
static inline Vec3  vec3_scale(Vec3 v,float s){return (Vec3){v.x*s,v.y*s,v.z*s};}
static inline float vec3_dist(Vec3 a,Vec3 b){return vec3_len(vec3_sub(a,b));}
static inline Vec3  vec3_cross(Vec3 a,Vec3 b){return (Vec3){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static inline float vec3_dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}

static Mat4 mat4_identity(void){Mat4 m={0};m.m[0]=m.m[5]=m.m[10]=m.m[15]=1;return m;}
static Mat4 mat4_mul(Mat4 a,Mat4 b){Mat4 c={0};for(int r=0;r<4;r++)for(int col=0;col<4;col++)for(int k=0;k<4;k++)c.m[col*4+r]+=a.m[k*4+r]*b.m[col*4+k];return c;}
static Mat4 mat4_inverse_affine(Mat4 m){
    float a=m.m[0], b=m.m[4], c=m.m[8];
    float d=m.m[1], e=m.m[5], f=m.m[9];
    float g=m.m[2], h=m.m[6], i=m.m[10];
    float det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
    if(fabsf(det)<1e-8f)return mat4_identity();
    float inv_det=1.0f/det;
    Mat4 out=mat4_identity();
    out.m[0]=(e*i-f*h)*inv_det;
    out.m[4]=(c*h-b*i)*inv_det;
    out.m[8]=(b*f-c*e)*inv_det;
    out.m[1]=(f*g-d*i)*inv_det;
    out.m[5]=(a*i-c*g)*inv_det;
    out.m[9]=(c*d-a*f)*inv_det;
    out.m[2]=(d*h-e*g)*inv_det;
    out.m[6]=(b*g-a*h)*inv_det;
    out.m[10]=(a*e-b*d)*inv_det;
    float tx=m.m[12],ty=m.m[13],tz=m.m[14];
    out.m[12]=-(out.m[0]*tx+out.m[4]*ty+out.m[8]*tz);
    out.m[13]=-(out.m[1]*tx+out.m[5]*ty+out.m[9]*tz);
    out.m[14]=-(out.m[2]*tx+out.m[6]*ty+out.m[10]*tz);
    return out;
}
static Mat4 mat4_translate(float x,float y,float z){Mat4 m=mat4_identity();m.m[12]=x;m.m[13]=y;m.m[14]=z;return m;}
static Mat4 mat4_scale_u(float s){Mat4 m=mat4_identity();m.m[0]=m.m[5]=m.m[10]=s;return m;}
static Mat4 mat4_rot_y(float r){Mat4 m=mat4_identity();float c=cosf(r),s=sinf(r);m.m[0]=c;m.m[2]=s;m.m[8]=-s;m.m[10]=c;return m;}
static Mat4 mat4_rot_x(float r){Mat4 m=mat4_identity();float c=cosf(r),s=sinf(r);m.m[5]=c;m.m[6]=-s;m.m[9]=s;m.m[10]=c;return m;}
static Mat4 mat4_rot_z(float r){Mat4 m=mat4_identity();float c=cosf(r),s=sinf(r);m.m[0]=c;m.m[1]=s;m.m[4]=-s;m.m[5]=c;return m;}
static Mat4 mat4_scale_xyz(float x,float y,float z){Mat4 m=mat4_identity();m.m[0]=x;m.m[5]=y;m.m[10]=z;return m;}
static Mat4 mat4_persp(float fov,float asp,float zn,float zf){float f=1.0f/tanf(fov*0.5f*(float)M_PI/180.0f);Mat4 m={0};m.m[0]=f/asp;m.m[5]=f;m.m[10]=(zf+zn)/(zn-zf);m.m[11]=-1.0f;m.m[14]=(2.0f*zf*zn)/(zn-zf);return m;}
static Mat4 mat4_lookat(Vec3 eye,Vec3 tgt,Vec3 up){Vec3 f=vec3_norm(vec3_sub(tgt,eye));Vec3 r=vec3_norm(vec3_cross(f,up));Vec3 u=vec3_cross(r,f);Mat4 m={0};m.m[0]=r.x;m.m[4]=r.y;m.m[8]=r.z;m.m[12]=-(r.x*eye.x+r.y*eye.y+r.z*eye.z);m.m[1]=u.x;m.m[5]=u.y;m.m[9]=u.z;m.m[13]=-(u.x*eye.x+u.y*eye.y+u.z*eye.z);m.m[2]=-f.x;m.m[6]=-f.y;m.m[10]=-f.z;m.m[14]=(f.x*eye.x+f.y*eye.y+f.z*eye.z);m.m[15]=1;return m;}
static Mat4 mat4_ortho(float l,float r,float b,float t,float n,float f){Mat4 m=mat4_identity();m.m[0]=2.0f/(r-l);m.m[5]=2.0f/(t-b);m.m[10]=-2.0f/(f-n);m.m[12]=-(r+l)/(r-l);m.m[13]=-(t+b)/(t-b);m.m[14]=-(f+n)/(f-n);return m;}
static Mat4 mat4_from_ufbx(const ufbx_matrix *u){Mat4 m={0};m.m[0]=(float)u->m00;m.m[1]=(float)u->m10;m.m[2]=(float)u->m20;m.m[4]=(float)u->m01;m.m[5]=(float)u->m11;m.m[6]=(float)u->m21;m.m[8]=(float)u->m02;m.m[9]=(float)u->m12;m.m[10]=(float)u->m22;m.m[12]=(float)u->m03;m.m[13]=(float)u->m13;m.m[14]=(float)u->m23;m.m[15]=1;return m;}

static bool world_to_ndc(Vec3 wp,Mat4 proj,Mat4 view,float *sx,float *sy){
    float x=view.m[0]*wp.x+view.m[4]*wp.y+view.m[8]*wp.z+view.m[12];
    float y=view.m[1]*wp.x+view.m[5]*wp.y+view.m[9]*wp.z+view.m[13];
    float z=view.m[2]*wp.x+view.m[6]*wp.y+view.m[10]*wp.z+view.m[14];
    float w=view.m[3]*wp.x+view.m[7]*wp.y+view.m[11]*wp.z+view.m[15];
    float cx=proj.m[0]*x+proj.m[4]*y+proj.m[8]*z+proj.m[12]*w;
    float cy=proj.m[1]*x+proj.m[5]*y+proj.m[9]*z+proj.m[13]*w;
    float cw=proj.m[3]*x+proj.m[7]*y+proj.m[11]*z+proj.m[15]*w;
    if(cw<=0.001f) return false;
    *sx=cx/cw; *sy=cy/cw; return true;
}

#endif

/*
 * RPG Engine - ufbx + SDL2 + OpenGL 3.3
 * Build: gcc -std=c99 -O2 -I. main.c ufbx.c -o rpg -lSDL2 -lGL -lm
 * Usage: ./rpg character.glb [village.glb]
 * Controls: WASD Move | SPACE Jump | F/LClick Attack | RightDrag Camera | Scroll Zoom | 1-5 Abilities
 * Assets: skybox.png (optional equirectangular panorama, falls back to gradient)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <SDL2/SDL.h>
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#include "ufbx.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#include "stb_image.h"
#include "gl_helpers.h"

/* ── Constants ─────────────────────────────────────────────── */
#define MAX_BONES        128
#define MAX_WEIGHTS      4
#define MAX_MESHES       64
#define WIN_W            1280
#define WIN_H            720
#define MAX_ENEMIES      8
#define MAX_QUESTS       8
#define MAX_OBJECTIVES   4
#define HOTBAR_SLOTS     5
#define MAX_PARTICLES    1024
#define MAX_EMITTERS     8
#define MAX_POINT_LIGHTS 6
#define MAX_COLL_BOXES   256
#define GRAVITY         -18.0f
#define PLAYER_SPEED     5.0f
#define PLAYER_JUMP      9.0f
#define ATTACK_RANGE     2.2f
#define ATTACK_DAMAGE   25.0f
#define ENEMY_SPEED      2.8f
#define ENEMY_CHASE_R   12.0f
#define ENEMY_ATTACK_R   1.8f
#define ENEMY_DAMAGE    12.0f
#define TERRAIN_COLS    128
#define TERRAIN_ROWS    128
#define TERRAIN_CELL    1.0f
#define WATER_LEVEL     0.0f
#define SHADOW_SIZE     1024
#define DAY_DURATION    120.0f  /* seconds for one full day/night cycle */

/* ── Math ───────────────────────────────────────────────────── */
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

static Mat4 mat4_identity(void){Mat4 m={0};m.m[0]=m.m[5]=m.m[10]=m.m[15]=1;return m;}
static Mat4 mat4_mul(Mat4 a,Mat4 b){Mat4 c={0};for(int r=0;r<4;r++)for(int col=0;col<4;col++)for(int k=0;k<4;k++)c.m[col*4+r]+=a.m[k*4+r]*b.m[col*4+k];return c;}
static Mat4 mat4_translate(float x,float y,float z){Mat4 m=mat4_identity();m.m[12]=x;m.m[13]=y;m.m[14]=z;return m;}
static Mat4 mat4_scale_u(float s){Mat4 m=mat4_identity();m.m[0]=m.m[5]=m.m[10]=s;return m;}
static Mat4 mat4_rot_y(float r){Mat4 m=mat4_identity();float c=cosf(r),s=sinf(r);m.m[0]=c;m.m[2]=s;m.m[8]=-s;m.m[10]=c;return m;}
static Mat4 mat4_persp(float fov,float asp,float zn,float zf){float f=1.0f/tanf(fov*0.5f*(float)M_PI/180.0f);Mat4 m={0};m.m[0]=f/asp;m.m[5]=f;m.m[10]=(zf+zn)/(zn-zf);m.m[11]=-1.0f;m.m[14]=(2.0f*zf*zn)/(zn-zf);return m;}
static Mat4 mat4_lookat(Vec3 eye,Vec3 tgt,Vec3 up){Vec3 f=vec3_norm(vec3_sub(tgt,eye));Vec3 r=vec3_norm(vec3_cross(f,up));Vec3 u=vec3_cross(r,f);Mat4 m={0};m.m[0]=r.x;m.m[4]=r.y;m.m[8]=r.z;m.m[12]=-(r.x*eye.x+r.y*eye.y+r.z*eye.z);m.m[1]=u.x;m.m[5]=u.y;m.m[9]=u.z;m.m[13]=-(u.x*eye.x+u.y*eye.y+u.z*eye.z);m.m[2]=-f.x;m.m[6]=-f.y;m.m[10]=-f.z;m.m[14]=(f.x*eye.x+f.y*eye.y+f.z*eye.z);m.m[15]=1;return m;}
static Mat4 mat4_ortho(float l,float r,float b,float t,float n,float f){Mat4 m=mat4_identity();m.m[0]=2.0f/(r-l);m.m[5]=2.0f/(t-b);m.m[10]=-2.0f/(f-n);m.m[12]=-(r+l)/(r-l);m.m[13]=-(t+b)/(t-b);m.m[14]=-(f+n)/(f-n);return m;}
static inline float vec3_dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static Mat4 mat4_from_ufbx(const ufbx_matrix *u){Mat4 m={0};m.m[0]=(float)u->m00;m.m[1]=(float)u->m10;m.m[2]=(float)u->m20;m.m[4]=(float)u->m01;m.m[5]=(float)u->m11;m.m[6]=(float)u->m21;m.m[8]=(float)u->m02;m.m[9]=(float)u->m12;m.m[10]=(float)u->m22;m.m[12]=(float)u->m03;m.m[13]=(float)u->m13;m.m[14]=(float)u->m23;m.m[15]=1;return m;}

static bool world_to_ndc(Vec3 wp,Mat4 proj,Mat4 view,float *sx,float *sy){
    float x=view.m[0]*wp.x+view.m[4]*wp.y+view.m[8]*wp.z+view.m[12];
    float y=view.m[1]*wp.x+view.m[5]*wp.y+view.m[9]*wp.z+view.m[13];
    float z=view.m[2]*wp.x+view.m[6]*wp.y+view.m[10]*wp.z+view.m[14];
    float w=view.m[3]*wp.x+view.m[7]*wp.y+view.m[11]*wp.z+view.m[15];
    float cx=proj.m[0]*x+proj.m[4]*y+proj.m[8]*z+proj.m[12]*w;
    float cy=proj.m[1]*x+proj.m[5]*y+proj.m[9]*z+proj.m[13]*w;
    float cw=proj.m[3]*x+proj.m[7]*y+proj.m[11]*z+proj.m[15]*w;
    if(cw<=0.001f)return false;*sx=cx/cw;*sy=cy/cw;return true;
}


/* ── Lighting ───────────────────────────────────────────────── */
#define LIGHTING_UNIFORMS \
    "uniform vec3 uSunDir,uSunColor,uAmbient;\n" \
    "uniform int uNumLights;\n" \
    "uniform vec3 uLightPos[6],uLightColor[6];\n" \
    "uniform float uLightRadius[6];\n" \
    "uniform sampler2D uShadowMap;\n" \
    "uniform mat4 uLightVP;\n"

#define LIGHTING_CALC \
    "    vec4 lsFrag=uLightVP*vec4(vPos,1.0);\n" \
    "    vec3 projC=lsFrag.xyz/lsFrag.w*0.5+0.5;\n" \
    "    float shadow=0.0;\n" \
    "    if(projC.z<=1.0&&projC.x>0.0&&projC.x<1.0&&projC.y>0.0&&projC.y<1.0){\n" \
    "        float bias=max(0.006*(1.0-dot(n,-uSunDir)),0.002);\n" \
    "        vec2 ts=1.0/vec2(textureSize(uShadowMap,0));\n" \
    "        for(int xi=-1;xi<=1;xi++)for(int yi=-1;yi<=1;yi++){\n" \
    "            float sd=texture(uShadowMap,projC.xy+vec2(float(xi),float(yi))*ts).r;\n" \
    "            shadow+=(projC.z-bias>sd)?0.4:0.0;}\n" \
    "        shadow/=9.0;}\n" \
    "    float diff=max(dot(n,-uSunDir),0.0)*(1.0-shadow);\n" \
    "    vec3 col=base*(uAmbient+uSunColor*diff);\n" \
    "    for(int i=0;i<uNumLights;i++){\n" \
    "        vec3 lv=uLightPos[i]-vPos;float d=length(lv);\n" \
    "        float att=1.0-clamp(d/uLightRadius[i],0,1);att*=att;\n" \
    "        col+=base*uLightColor[i]*max(dot(n,normalize(lv)),0.0)*att;\n" \
    "    }\n"

typedef struct {
    Vec3 pos,color;
    float radius,intensity,flicker_speed,flicker_phase;
    bool flicker;
} PointLight;
typedef struct {
    Vec3 sun_dir,sun_color,ambient;
    PointLight points[MAX_POINT_LIGHTS];
    int num_points;
    GLuint shadow_tex;  /* bound to slot 7 in every lit draw call */
    Mat4   light_vp;    /* light-space view-projection for shadow map */
    float  day_frac;    /* 0..1 through the day cycle */
    float  sun_elev;    /* -1..1, positive = sun above horizon */
} LightingSystem;

static void lighting_init(LightingSystem *ls){
    memset(ls,0,sizeof(*ls));
    ls->sun_dir=vec3_norm((Vec3){-0.5f,-1.0f,-0.3f});
    ls->sun_color=(Vec3){1.0f,0.95f,0.85f};
    ls->ambient=(Vec3){0.18f,0.18f,0.22f};
    ls->light_vp=mat4_identity();
    /* 1×1 white depth texture so the sampler is never unbound */
    float one=1.0f;
    glGenTextures(1,&ls->shadow_tex);
    glBindTexture(GL_TEXTURE_2D,ls->shadow_tex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,1,1,0,GL_DEPTH_COMPONENT,GL_FLOAT,&one);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
}
static int lighting_add_point(LightingSystem *ls,Vec3 pos,Vec3 col,float rad,float intensity){
    if(ls->num_points>=MAX_POINT_LIGHTS)return -1;
    int i=ls->num_points++;
    ls->points[i]=(PointLight){pos,col,rad,intensity,0,0,false};
    return i;
}
static void lighting_update(LightingSystem *ls,float t){
    /* ── Day/Night cycle ──────────────────────────────────────── */
    ls->day_frac=fmodf(t,DAY_DURATION)/DAY_DURATION;
    /* sun_angle: 0=dawn, PI/2=noon, PI=dusk, 3PI/2=midnight */
    float sun_angle=ls->day_frac*2.0f*(float)M_PI-(float)M_PI*0.5f;
    float elev=sinf(sun_angle);          /* positive = above horizon */
    float azim=cosf(sun_angle);
    ls->sun_elev=elev;
    /* sun_dir points FROM sun TOWARD scene (matches shader dot(n,-uSunDir)) */
    ls->sun_dir=vec3_norm((Vec3){-0.35f,-elev,-azim*0.6f});

    if(elev>0.0f){
        /* Day: interpolate noon white ↔ dawn/dusk orange */
        float dawn_t=clampf(1.0f-elev*4.0f,0,1);
        ls->sun_color=(Vec3){
            1.0f*(1-dawn_t)+1.0f*dawn_t,
            0.95f*(1-dawn_t)+0.55f*dawn_t,
            0.85f*(1-dawn_t)+0.15f*dawn_t};
        float day_t=clampf(elev,0,1);
        ls->ambient=(Vec3){0.05f+0.13f*day_t, 0.05f+0.13f*day_t, 0.08f+0.14f*day_t};
    } else {
        /* Night: dim moonlight */
        ls->sun_color=(Vec3){0.04f,0.04f,0.09f};
        ls->ambient=(Vec3){0.02f,0.02f,0.05f};
    }

    /* ── Point light flicker ──────────────────────────────────── */
    for(int i=0;i<ls->num_points;i++){
        PointLight *p=&ls->points[i];if(!p->flicker)continue;
        p->intensity=0.85f+0.15f*sinf(t*p->flicker_speed+p->flicker_phase)
                         +0.08f*sinf(t*p->flicker_speed*2.3f+p->flicker_phase+1.1f);
    }
}
static void lighting_push(LightingSystem *ls,GLuint prog){
    glUseProgram(prog);
    GLH_vec3(prog,"uSunDir",ls->sun_dir.x,ls->sun_dir.y,ls->sun_dir.z);
    GLH_vec3(prog,"uSunColor",ls->sun_color.x,ls->sun_color.y,ls->sun_color.z);
    GLH_vec3(prog,"uAmbient",ls->ambient.x,ls->ambient.y,ls->ambient.z);
    GLH_int(prog,"uNumLights",ls->num_points);
    float lpos[MAX_POINT_LIGHTS*3],lcol[MAX_POINT_LIGHTS*3],lrad[MAX_POINT_LIGHTS];
    for(int i=0;i<ls->num_points;i++){
        PointLight *p=&ls->points[i];
        lpos[i*3+0]=p->pos.x;lpos[i*3+1]=p->pos.y;lpos[i*3+2]=p->pos.z;
        lcol[i*3+0]=p->color.x*p->intensity;lcol[i*3+1]=p->color.y*p->intensity;lcol[i*3+2]=p->color.z*p->intensity;
        lrad[i]=p->radius;
    }
    if(ls->num_points>0){
        GLH_vec3_array(prog,"uLightPos",ls->num_points,lpos);
        GLH_vec3_array(prog,"uLightColor",ls->num_points,lcol);
        GLH_float_array(prog,"uLightRadius",ls->num_points,lrad);
    }
    /* Shadow map always bound to slot 7 */
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D,ls->shadow_tex);
    GLH_int(prog,"uShadowMap",7);
    GLH_mat4(prog,"uLightVP",ls->light_vp.m);
}


/* ── Terrain ────────────────────────────────────────────────── */
typedef struct{float pos[3],normal[3],color[3];}TVert;
typedef struct{
    GLuint land_vao,water_vao;int land_ic;
    GLuint land_prog,water_prog;
    /* Splat textures: 0=sand 1=grass 2=dirt 3=rock 4=snow 5=lava */
    GLuint tex[6];
    bool   has_tex[6];
}Terrain;

/* Load a tileable texture from file; on failure generate a solid-color 4x4 fallback */
static GLuint terrain_tex_load(const char *path, float r,float g,float b){
    int w,h,ch;
    uint8_t *px=stbi_load(path,&w,&h,&ch,4);
    GLuint id;glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);
    if(px){
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,px);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        stbi_image_free(px);
        printf("[Terrain] Loaded %s\n",path);
        return id;
    }
    /* fallback solid color */
    uint8_t fb[4]={(uint8_t)(r*255),(uint8_t)(g*255),(uint8_t)(b*255),255};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,fb);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    printf("[Terrain] '%s' not found, using solid fallback\n",path);
    return id;
}

static float terrain_h(float wx,float wz){
    float d=sqrtf(wx*wx+wz*wz);
    float flat_r=30.0f,blend_r=50.0f;
    float h=sinf(wx*0.08f)*cosf(wz*0.11f)*4.0f
           +sinf(wx*0.21f+1.3f)*sinf(wz*0.17f+0.9f)*2.0f
           +sinf(wx*0.43f+2.1f)*cosf(wz*0.37f+1.7f)*1.0f
           +sinf(wx*0.89f+0.7f)*sinf(wz*0.71f+3.1f)*0.5f
           +sinf(wx*1.73f+3.3f)*cosf(wz*1.57f+2.7f)*0.2f;
    if(d<flat_r)return 0.5f;  /* above WATER_LEVEL=0 so no z-fighting with water plane */
    if(d<blend_r){float t=(d-flat_r)/(blend_r-flat_r);return 0.5f+h*t*t;}
    return h;
}
static Vec3 terrain_color(float h){
    if(h<WATER_LEVEL)      return (Vec3){0.12f,0.28f,0.62f};
    if(h<WATER_LEVEL+0.3f) return (Vec3){0.76f,0.70f,0.50f};
    if(h<WATER_LEVEL+3.5f) return (Vec3){0.25f,0.52f,0.18f};
    if(h<WATER_LEVEL+5.5f) return (Vec3){0.50f,0.48f,0.42f};
    return                        (Vec3){0.90f,0.90f,0.95f};
}

static void terrain_build(Terrain *t){
    int cols=TERRAIN_COLS,rows=TERRAIN_ROWS;float cell=TERRAIN_CELL;
    float ox=-(cols*cell)*0.5f,oz=-(rows*cell)*0.5f;
    int vc=(cols+1)*(rows+1),ic=cols*rows*6;
    TVert *verts=malloc((size_t)vc*sizeof(TVert));
    uint32_t *inds=malloc((size_t)ic*sizeof(uint32_t));
    for(int z=0;z<=rows;z++)for(int x=0;x<=cols;x++){
        float wx=ox+x*cell,wz=oz+z*cell,wy=terrain_h(wx,wz);
        TVert *v=&verts[z*(cols+1)+x];
        v->pos[0]=wx;v->pos[1]=wy<WATER_LEVEL?WATER_LEVEL-0.01f:wy;v->pos[2]=wz;
        float hL=terrain_h(wx-0.1f,wz),hR=terrain_h(wx+0.1f,wz);
        float hD=terrain_h(wx,wz-0.1f),hU=terrain_h(wx,wz+0.1f);
        Vec3 n=vec3_norm((Vec3){hL-hR,0.2f,hD-hU});
        v->normal[0]=n.x;v->normal[1]=n.y;v->normal[2]=n.z;
        Vec3 c=terrain_color(wy);v->color[0]=c.x;v->color[1]=c.y;v->color[2]=c.z;
    }
    int idx=0;
    for(int z=0;z<rows;z++)for(int x=0;x<cols;x++){
        uint32_t tl=(uint32_t)(z*(cols+1)+x),tr=tl+1,bl=tl+(uint32_t)(cols+1),br=bl+1;
        inds[idx++]=tl;inds[idx++]=bl;inds[idx++]=tr;
        inds[idx++]=tr;inds[idx++]=bl;inds[idx++]=br;
    }
    t->land_ic=ic;
    GLH_Attrib a[]={{0,3,GL_FLOAT,GL_FALSE,offsetof(TVert,pos)},{1,3,GL_FLOAT,GL_FALSE,offsetof(TVert,normal)},{2,3,GL_FLOAT,GL_FALSE,offsetof(TVert,color)}};
    t->land_vao=GLH_mesh(verts,(size_t)vc*sizeof(TVert),inds,(size_t)ic*sizeof(uint32_t),sizeof(TVert),a,3);
    free(verts);free(inds);
    float W=cols*cell*2.0f,D=rows*cell*2.0f;
    float wv[]={-W,WATER_LEVEL,-D,0,1,0,0.08f,0.25f,0.55f,W,WATER_LEVEL,-D,0,1,0,0.08f,0.25f,0.55f,W,WATER_LEVEL,D,0,1,0,0.08f,0.25f,0.55f,-W,WATER_LEVEL,D,0,1,0,0.08f,0.25f,0.55f};
    uint32_t wi[]={0,1,2,0,2,3};
    t->water_vao=GLH_mesh(wv,sizeof(wv),wi,sizeof(wi),9*sizeof(float),a,3);
    t->land_prog=GLH_program(
        /* ── Vertex ── */
        "#version 330 core\n"
        "layout(location=0)in vec3 aPos;\nlayout(location=1)in vec3 aNormal;\nlayout(location=2)in vec3 aColor;\n"
        "uniform mat4 uProj,uView;\nout vec3 vNormal,vPos,vColor;\n"
        "void main(){gl_Position=uProj*uView*vec4(aPos,1);vNormal=aNormal;vPos=aPos;vColor=aColor;}\n",
        /* ── Fragment ── */
        "#version 330 core\n"
        "in vec3 vNormal,vPos,vColor;\n"
        "uniform float uTime;\n"
        "uniform sampler2D uTexSand,uTexGrass,uTexDirt,uTexRock,uTexSnow,uTexLava;\n"
        LIGHTING_UNIFORMS
        "out vec4 FragColor;\n"

        /* Sample a texture tiled at given scale, returns RGB */
        "vec3 stex(sampler2D s,float sc){return texture(s,vPos.xz*sc).rgb;}\n"

        "void main(){\n"
        "    vec3 n=normalize(vNormal);\n"
        "    float h=vPos.y;\n"
        "    float slope=clamp(1.0-n.y,0.0,1.0);\n"

        /* Trig-based detail variation (same as before, keeps look when no textures) */
        "    float nd=sin(vPos.x*4.10+vPos.z*3.70)*0.50\n"
        "            +sin(vPos.x*11.3-vPos.z*8.90)*0.25\n"
        "            +cos(vPos.x*5.70-vPos.z*4.30)*0.50\n"
        "            +cos(vPos.x*9.10+vPos.z*12.7)*0.25;\n"
        "    float det=nd*0.04;\n"

        /* Sample each splat texture at a comfortable tiling scale */
        "    vec3 c_sand =stex(uTexSand, 0.18)*(1.0+det);\n"
        "    vec3 c_grass=stex(uTexGrass,0.25)*(1.0+det*1.5);\n"
        "    vec3 c_dirt =stex(uTexDirt, 0.22)*(1.0+det);\n"
        "    vec3 c_rock =stex(uTexRock, 0.20)*(1.0+det);\n"
        "    vec3 c_snow =stex(uTexSnow, 0.15)*(1.0+det*0.2);\n"

        /* Height / slope blending weights */
        "    float t_grass=clamp((h-0.30)/1.20,0.0,1.0);\n"
        "    float t_dirt =clamp(slope*2.5-0.4,0.0,1.0);\n"
        "    float t_rock =clamp((h-3.00)/1.80,0.0,1.0);\n"
        "    float t_snow =clamp((h-4.00)/1.20,0.0,1.0);\n"

        /* Layer biomes */
        "    vec3 base=c_sand;\n"
        "    base=mix(base,c_grass,t_grass);\n"
        "    base=mix(base,c_dirt, t_dirt);\n"
        "    base=mix(base,c_rock, t_rock);\n"
        "    float snow_flat=clamp((1.0-slope*4.0),0.0,1.0);\n"
        "    float snow_mix =t_snow*mix(0.4,1.0,snow_flat);\n"
        "    float snow_patch=clamp((sin(vPos.x*0.4+vPos.z*0.6)*0.5+0.5)*2.0-0.6,0.0,1.0)\n"
        "                    *clamp((h-3.0)/1.5,0.0,1.0);\n"
        "    base=mix(base,c_snow,clamp(snow_mix+snow_patch*0.35,0.0,1.0));\n"

        /* Lava zones */
        "    float dist=length(vPos.xz);\n"
        "    float lnoise=sin(vPos.x*0.31+1.70)*cos(vPos.z*0.27+0.90)\n"
        "                +sin(vPos.x*0.53-vPos.z*0.41)*0.50\n"
        "                +cos(vPos.x*0.19+vPos.z*0.63)*0.30;\n"
        "    float lava_mask=clamp((lnoise-0.55)*4.0,0.0,1.0)\n"
        "                   *clamp((dist-52.0)/12.0,0.0,1.0)\n"
        "                   *clamp((2.0-h)/2.0,0.0,1.0);\n"
        "    float pulse=sin(uTime*1.8+vPos.x*0.5+vPos.z*0.4)*0.5+0.5;\n"
        "    float crack=clamp(sin(vPos.x*3.1+vPos.z*2.3+uTime*0.3)*3.0,0.0,1.0);\n"
        /* Blend lava texture with animated glow */
        "    vec3 c_lava_tex=stex(uTexLava,0.12);\n"
        "    vec3 c_lava=mix(c_lava_tex*0.3,c_lava_tex+vec3(0.6,0.1,0.0),pulse*crack);\n"
        "    base=mix(base,c_lava,lava_mask);\n"

        LIGHTING_CALC
        "    col+=c_lava*lava_mask*(0.5+pulse*0.8);\n"
        "    FragColor=vec4(col,1.0);\n"
        "}\n");

    /* Load splat textures — drop PNG files next to the executable.
       Fallback solid colours are used automatically if files are missing. */
    t->tex[0]=terrain_tex_load("terrain_sand.png",  0.76f,0.70f,0.50f);
    t->tex[1]=terrain_tex_load("terrain_grass.png", 0.20f,0.50f,0.14f);
    t->tex[2]=terrain_tex_load("terrain_dirt.png",  0.38f,0.28f,0.18f);
    t->tex[3]=terrain_tex_load("terrain_rock.png",  0.50f,0.46f,0.40f);
    t->tex[4]=terrain_tex_load("terrain_snow.png",  0.90f,0.93f,0.98f);
    t->tex[5]=terrain_tex_load("terrain_lava.png",  0.80f,0.20f,0.02f);
    t->water_prog=GLH_program(
        "#version 330 core\n"
        "layout(location=0)in vec3 aPos;\nlayout(location=1)in vec3 aNormal;\nlayout(location=2)in vec3 aColor;\n"
        "uniform mat4 uProj,uView;\nuniform float uTime;\nout vec3 vNormal,vPos;\n"
        "void main(){\n"
        "    vec3 p=aPos;\n"
        "    p.y+=sin(p.x*0.4+uTime*1.2)*0.08+sin(p.z*0.3+uTime*0.9)*0.06;\n"
        "    gl_Position=uProj*uView*vec4(p,1);vNormal=aNormal;vPos=p;}\n",
        "#version 330 core\nin vec3 vNormal,vPos;\n"
        LIGHTING_UNIFORMS
        "uniform vec3 uCamPos;\nout vec4 FragColor;\nvoid main(){\n"
        "    vec3 n=normalize(vNormal),base=vec3(0.06,0.22,0.52);\n"
        LIGHTING_CALC
        "    vec3 view=normalize(uCamPos-vPos),refl=reflect(uSunDir,n);\n"
        "    col+=vec3(0.8)*pow(max(dot(refl,view),0.0),64.0)*0.5;\n"
        "    FragColor=vec4(col,0.82);}\n");
    printf("[Terrain] %dx%d built\n",cols,rows);
}
static void terrain_draw(Terrain *t,LightingSystem *ls,Mat4 proj,Mat4 view,Vec3 cam,float time){
    lighting_push(ls,t->land_prog);GLH_begin(t->land_prog,proj.m,view.m);
    GLH_float(t->land_prog,"uTime",time);
    /* Bind splat textures to slots 0-5 (slot 7 = shadow map, reserved) */
    const char *names[]={"uTexSand","uTexGrass","uTexDirt","uTexRock","uTexSnow","uTexLava"};
    for(int i=0;i<6;i++){
        glActiveTexture(GL_TEXTURE0+i);
        glBindTexture(GL_TEXTURE_2D,t->tex[i]);
        GLH_int(t->land_prog,names[i],i);
    }
    GLH_draw_indexed(t->land_vao,t->land_ic);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    lighting_push(ls,t->water_prog);GLH_begin(t->water_prog,proj.m,view.m);
    GLH_float(t->water_prog,"uTime",time);GLH_vec3(t->water_prog,"uCamPos",cam.x,cam.y,cam.z);
    GLH_draw_indexed(t->water_vao,6);
    glDepthMask(GL_TRUE);glDisable(GL_BLEND);
}


/* ── Shadow Map ─────────────────────────────────────────────── */
typedef struct{GLuint fbo,tex,prog_depth,prog_skin_depth;}ShadowMap;

static void shadow_init(ShadowMap *sm,LightingSystem *ls){
    glGenTextures(1,&sm->tex);
    glBindTexture(GL_TEXTURE_2D,sm->tex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,SHADOW_SIZE,SHADOW_SIZE,0,GL_DEPTH_COMPONENT,GL_FLOAT,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
    float bc[]={1,1,1,1};glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,bc);
    glGenFramebuffers(1,&sm->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER,sm->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,sm->tex,0);
    glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    sm->prog_depth=GLH_program(
        "#version 330 core\nlayout(location=0)in vec3 aPos;\nuniform mat4 uLightVP,uModel;\nvoid main(){gl_Position=uLightVP*uModel*vec4(aPos,1);}\n",
        "#version 330 core\nvoid main(){}\n");
    sm->prog_skin_depth=GLH_program(
        "#version 330 core\n"
        "layout(location=0)in vec3 aPos;\nlayout(location=3)in vec4 aWeights;\nlayout(location=4)in ivec4 aBones;\n"
        "uniform mat4 uLightVP,uModel,uBones[128];\n"
        "void main(){\n"
        "  mat4 sk=aWeights.x*uBones[aBones.x]+aWeights.y*uBones[aBones.y]+aWeights.z*uBones[aBones.z]+aWeights.w*uBones[aBones.w];\n"
        "  if(dot(aWeights,vec4(1))<0.001)sk=mat4(1);\n"
        "  gl_Position=uLightVP*uModel*sk*vec4(aPos,1);}\n",
        "#version 330 core\nvoid main(){}\n");
    ls->shadow_tex=sm->tex;
    printf("[Shadow] %dx%d shadow map created\n",SHADOW_SIZE,SHADOW_SIZE);
}

static void shadow_begin(ShadowMap *sm,LightingSystem *ls,Vec3 center){
    /* Use the live sun direction from the lighting system.
       ls->sun_dir points FROM sun TO scene, so negate for "scene→sun". */
    Vec3 sun_up={-ls->sun_dir.x,-ls->sun_dir.y,-ls->sun_dir.z};
    /* Clamp Y so the shadow frustum never collapses at night */
    if(sun_up.y<0.15f)sun_up.y=0.15f;
    sun_up=vec3_norm(sun_up);
    Vec3 lp=vec3_add(center,vec3_scale(sun_up,70.0f));
    Mat4 lv=mat4_lookat(lp,center,(Vec3){0,1,0});
    Mat4 lproj=mat4_ortho(-70,70,-70,70,1.0f,180.0f);
    ls->light_vp=mat4_mul(lproj,lv);
    glBindFramebuffer(GL_FRAMEBUFFER,sm->fbo);
    glViewport(0,0,SHADOW_SIZE,SHADOW_SIZE);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(2.0f,4.0f);
    glCullFace(GL_FRONT);
}
static void shadow_end(ShadowMap *sm,int win_w,int win_h){
    (void)sm;
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glViewport(0,0,win_w,win_h);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glCullFace(GL_BACK);
}

/* Render terrain into shadow map (no model matrix – already in world space) */
static void shadow_terrain(ShadowMap *sm,LightingSystem *ls,Terrain *t){
    glUseProgram(sm->prog_depth);
    Mat4 id=mat4_identity();
    glUniformMatrix4fv(glGetUniformLocation(sm->prog_depth,"uLightVP"),1,GL_FALSE,ls->light_vp.m);
    glUniformMatrix4fv(glGetUniformLocation(sm->prog_depth,"uModel"),1,GL_FALSE,id.m);
    glBindVertexArray(t->land_vao);glDrawElements(GL_TRIANGLES,t->land_ic,GL_UNSIGNED_INT,0);
    glBindVertexArray(0);
}


/* ── Collision ──────────────────────────────────────────────── */
typedef struct{Vec3 min,max;}AABB;
typedef struct{AABB boxes[MAX_COLL_BOXES];int count;}CollWorld;
static void coll_add(CollWorld *cw,ufbx_mesh *mesh,ufbx_node *node){
    if(cw->count>=MAX_COLL_BOXES)return;
    AABB b;b.min=(Vec3){1e9f,1e9f,1e9f};b.max=(Vec3){-1e9f,-1e9f,-1e9f};
    for(size_t vi=0;vi<mesh->num_vertices;vi++){
        ufbx_vec3 lp=mesh->vertices.data[vi];ufbx_vec3 wp=ufbx_transform_position(&node->geometry_to_world,lp);
        float x=(float)wp.x,y=(float)wp.y,z=(float)wp.z;
        if(x<b.min.x)b.min.x=x;if(x>b.max.x)b.max.x=x;
        if(y<b.min.y)b.min.y=y;if(y>b.max.y)b.max.y=y;
        if(z<b.min.z)b.min.z=z;if(z>b.max.z)b.max.z=z;
    }
    if(vec3_len(vec3_sub(b.max,b.min))>0.1f)cw->boxes[cw->count++]=b;
}
static void coll_resolve(CollWorld *cw,Vec3 *pos,float radius){
    for(int i=0;i<cw->count;i++){
        AABB *b=&cw->boxes[i];
        float cx=clampf(pos->x,b->min.x,b->max.x),cy=clampf(pos->y,b->min.y,b->max.y),cz=clampf(pos->z,b->min.z,b->max.z);
        float dx=pos->x-cx,dy=pos->y-cy,dz=pos->z-cz;
        float d2=dx*dx+dy*dy+dz*dz;
        if(d2<radius*radius){float d=sqrtf(d2);if(d<1e-5f){pos->x+=radius;continue;}float pen=radius-d;pos->x+=(dx/d)*pen;pos->z+=(dz/d)*pen;}
    }
}


/* ── GPU Mesh / Scene ───────────────────────────────────────── */
typedef struct{float pos[3],normal[3],uv[2];float weights[MAX_WEIGHTS];int32_t bones[MAX_WEIGHTS];}Vertex;
typedef struct{GLuint vao;GLsizei index_count;int32_t cluster_map[MAX_BONES],num_gpu_bones;Mat4 geometry_to_world;uint32_t node_typed_id;GLuint texture;}GpuMesh;
typedef struct{ufbx_scene *scene;GpuMesh meshes[MAX_MESHES];int num_meshes;ufbx_skin_deformer *skins[MAX_MESHES];float model_scale,cx,cy,cz,ground_offset;}Scene;
typedef struct{int idle,walk,run,attack,attack2,jump,death,roll,block,hit;}AnimMap;

static int find_anim(ufbx_scene *sc,const char *kw){
    if(!sc)return -1;
    for(size_t i=0;i<sc->anim_stacks.count;i++){
        const char *n=sc->anim_stacks.data[i]->name.data;
        for(size_t si=0;n[si];si++){size_t ki=0;while(kw[ki]&&tolower((unsigned char)n[si+ki])==tolower((unsigned char)kw[ki]))ki++;if(!kw[ki])return (int)i;}
    }
    return -1;
}
static void animmap_init(AnimMap *am,ufbx_scene *sc){
    am->idle   =find_anim(sc,"idle");
    am->walk   =find_anim(sc,"walk");
    am->run    =find_anim(sc,"run");    if(am->run<0)    am->run   =find_anim(sc,"sprint");
    am->attack =find_anim(sc,"attack"); if(am->attack<0) am->attack=find_anim(sc,"slash");
    am->attack2=find_anim(sc,"attack2");if(am->attack2<0)am->attack2=find_anim(sc,"kick");
    am->jump   =find_anim(sc,"jump");
    am->death  =find_anim(sc,"death");  if(am->death<0)  am->death =find_anim(sc,"die");
    am->roll   =find_anim(sc,"roll");   if(am->roll<0)   am->roll  =find_anim(sc,"dodge");
    am->block  =find_anim(sc,"block");  if(am->block<0)  am->block =find_anim(sc,"guard");
    am->hit    =find_anim(sc,"hit");    if(am->hit<0)    am->hit   =find_anim(sc,"hurt");
    /* fallbacks */
    if(am->idle<0)   am->idle=0;
    if(am->walk<0)   am->walk=am->idle;
    if(am->run<0)    am->run=am->walk;
    if(am->attack<0) am->attack=am->idle;
    if(am->attack2<0)am->attack2=am->attack;
    if(am->jump<0)   am->jump=am->idle;
    if(am->death<0)  am->death=am->idle;
    if(am->roll<0)   am->roll=am->idle;
    if(am->block<0)  am->block=am->idle;
    if(am->hit<0)    am->hit=am->idle;
}

static GLuint get_mesh_tex(ufbx_mesh *mesh,const char *path){
    for(size_t mi=0;mi<mesh->materials.count;mi++){
        ufbx_material *mat=mesh->materials.data[mi];if(!mat)continue;
        ufbx_texture *tex=mat->pbr.base_color.texture;if(!tex)tex=mat->fbx.diffuse_color.texture;if(!tex)continue;
        if(tex->content.size>4){GLuint id=GLH_texture_mem(tex->content.data,(int)tex->content.size);if(id)return id;}
        if(tex->relative_filename.length>0){
            char full[1024];const char *s=strrchr(path,'/'),*b2=strrchr(path,'\\');const char *last=s>b2?s:b2;
            if(last)snprintf(full,sizeof(full),"%.*s%s",(int)(last-path+1),path,tex->relative_filename.data);
            else snprintf(full,sizeof(full),"%s",tex->relative_filename.data);
            GLuint id=GLH_texture_file(full);if(id)return id;
        }
    }
    return 0;
}

static bool build_gpu_mesh(GpuMesh *gm,ufbx_skin_deformer **skin_out,ufbx_mesh *mesh,ufbx_node *node,const char *path){
    ufbx_skin_deformer *skin=mesh->skin_deformers.count>0?mesh->skin_deformers.data[0]:NULL;
    *skin_out=skin;
    int32_t c2g[MAX_BONES];memset(c2g,-1,sizeof(c2g));gm->num_gpu_bones=0;
    if(skin){size_t nc=skin->clusters.count<MAX_BONES?skin->clusters.count:MAX_BONES;for(size_t ci=0;ci<nc;ci++){c2g[ci]=(int32_t)ci;gm->cluster_map[ci]=(int32_t)ci;}gm->num_gpu_bones=(int32_t)nc;}
    size_t ntri=mesh->max_face_triangles*3;uint32_t *tri=malloc(ntri*sizeof(uint32_t));if(!tri)return false;
    Vertex *verts=NULL;uint32_t *inds=NULL;size_t vc=0,ic=0,vcap=4096,icap=4096;
    verts=malloc(vcap*sizeof(Vertex));inds=malloc(icap*sizeof(uint32_t));if(!verts||!inds){free(tri);return false;}
    for(size_t fi=0;fi<mesh->num_faces;fi++){
        ufbx_face face=mesh->faces.data[fi];size_t nt=ufbx_triangulate_face(tri,ntri,mesh,face);
        for(size_t ti=0;ti<nt*3;ti++){
            uint32_t ix=tri[ti],vi=mesh->vertex_indices.data[ix];Vertex v={0};
            ufbx_vec3 p=ufbx_get_vertex_vec3(&mesh->vertex_position,ix);v.pos[0]=(float)p.x;v.pos[1]=(float)p.y;v.pos[2]=(float)p.z;
            if(mesh->vertex_normal.exists){ufbx_vec3 n=ufbx_get_vertex_vec3(&mesh->vertex_normal,ix);v.normal[0]=(float)n.x;v.normal[1]=(float)n.y;v.normal[2]=(float)n.z;}else v.normal[1]=1.0f;
            if(mesh->vertex_uv.exists){ufbx_vec2 uv=ufbx_get_vertex_vec2(&mesh->vertex_uv,ix);v.uv[0]=(float)uv.x;v.uv[1]=(float)uv.y;}
            if(skin){ufbx_skin_vertex sv=skin->vertices.data[vi];size_t nw=sv.num_weights<MAX_WEIGHTS?sv.num_weights:MAX_WEIGHTS;float tw=0;
                for(size_t wi=0;wi<nw;wi++){ufbx_skin_weight sw=skin->weights.data[sv.weight_begin+wi];int32_t gb=(sw.cluster_index<MAX_BONES)?c2g[sw.cluster_index]:-1;if(gb<0)continue;v.bones[wi]=gb;v.weights[wi]=(float)sw.weight;tw+=(float)sw.weight;}
                if(tw>0.001f)for(int wi=0;wi<MAX_WEIGHTS;wi++)v.weights[wi]/=tw;}
            if(vc>=vcap){vcap*=2;verts=realloc(verts,vcap*sizeof(Vertex));}
            if(ic>=icap){icap*=2;inds=realloc(inds,icap*sizeof(uint32_t));}
            inds[ic++]=(uint32_t)vc;verts[vc++]=v;
        }
    }
    free(tri);
    GLH_Attrib attrs[]={{0,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,pos)},{1,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,normal)},{2,2,GL_FLOAT,GL_FALSE,offsetof(Vertex,uv)},{3,4,GL_FLOAT,GL_FALSE,offsetof(Vertex,weights)},{4,4,GL_INT,0,offsetof(Vertex,bones)}};
    gm->vao=GLH_mesh(verts,(size_t)vc*sizeof(Vertex),inds,(size_t)ic*sizeof(uint32_t),sizeof(Vertex),attrs,5);
    gm->index_count=(GLsizei)ic;gm->geometry_to_world=mat4_from_ufbx(&node->geometry_to_world);gm->node_typed_id=node->typed_id;gm->texture=get_mesh_tex(mesh,path);
    free(verts);free(inds);return true;
}

static bool scene_load(Scene *s,const char *path){
    ufbx_load_opts opts={0};opts.generate_missing_normals=true;opts.target_axes=ufbx_axes_right_handed_y_up;opts.target_unit_meters=1.0f;
    ufbx_error err;s->scene=ufbx_load_file(path,&opts,&err);
    if(!s->scene){fprintf(stderr,"Load '%s': %s\n",path,err.description.data);return false;}
    s->num_meshes=0;
    for(size_t ni=0;ni<s->scene->nodes.count&&s->num_meshes<MAX_MESHES;ni++){ufbx_node *nd=s->scene->nodes.data[ni];if(!nd->mesh)continue;int mi=s->num_meshes++;if(!build_gpu_mesh(&s->meshes[mi],&s->skins[mi],nd->mesh,nd,path))s->num_meshes--;}
    float xmin=1e9f,ymin=1e9f,zmin=1e9f,xmax=-1e9f,ymax=-1e9f,zmax=-1e9f;
    for(size_t ni=0;ni<s->scene->nodes.count;ni++){ufbx_node *nd=s->scene->nodes.data[ni];if(!nd->mesh)continue;
        for(size_t vi=0;vi<nd->mesh->num_vertices;vi++){ufbx_vec3 lp=nd->mesh->vertices.data[vi];ufbx_vec3 p=ufbx_transform_position(&nd->geometry_to_world,lp);
            float x=(float)p.x,y=(float)p.y,z=(float)p.z;if(x<xmin)xmin=x;if(x>xmax)xmax=x;if(y<ymin)ymin=y;if(y>ymax)ymax=y;if(z<zmin)zmin=z;if(z>zmax)zmax=z;}}
    s->cx=(xmin+xmax)*0.5f;s->cy=(ymin+ymax)*0.5f;s->cz=(zmin+zmax)*0.5f;
    float md=fmaxf(xmax-xmin,fmaxf(ymax-ymin,zmax-zmin));if(md<0.0001f)md=1.0f;
    s->model_scale=2.0f/md;s->ground_offset=-(ymin-s->cy)*s->model_scale;
    printf("[Scene] %s: %d meshes\n",path,s->num_meshes);return true;
}
static void scene_free(Scene *s){if(s->scene){ufbx_free_scene(s->scene);s->scene=NULL;}for(int i=0;i<s->num_meshes;i++){glDeleteVertexArrays(1,&s->meshes[i].vao);if(s->meshes[i].texture)glDeleteTextures(1,&s->meshes[i].texture);}}
/* Returns the actual last keyframe time in an animation stack by scanning
   all curves.  Falls back to stk->time_end if no keys are found.
   This fixes the case where the FBX stack duration is longer than the
   actual animation cycle (e.g. walk = 2 s inside a 6 s stack). */
static double anim_stack_real_end(ufbx_anim_stack *stk){
    double end=stk->time_begin;
    for(size_t li=0;li<stk->layers.count;li++){
        ufbx_anim_layer *layer=stk->layers.data[li];
        for(size_t vi=0;vi<layer->anim_values.count;vi++){
            ufbx_anim_value *av=layer->anim_values.data[vi];
            for(int k=0;k<3;k++){
                ufbx_anim_curve *curve=av->curves[k];
                if(!curve||curve->keyframes.count==0)continue;
                double kt=curve->keyframes.data[curve->keyframes.count-1].time;
                if(kt>end)end=kt;
            }
        }
    }
    /* If we found no keys at all, fall back to the stack's declared end */
    if(end<=stk->time_begin)end=stk->time_end;
    return end;
}

static void compute_bones(Mat4 *out,int nb,const GpuMesh *gm,ufbx_skin_deformer *sk,ufbx_scene *ev){
    for(int i=0;i<nb;i++)out[i]=mat4_identity();if(!sk||!ev)return;
    uint32_t tid=sk->typed_id;if(tid>=ev->skin_deformers.count)return;
    ufbx_skin_deformer *es=ev->skin_deformers.data[tid];size_t nc=es->clusters.count<(size_t)nb?es->clusters.count:(size_t)nb;
    for(size_t ci=0;ci<nc;ci++){ufbx_skin_cluster *cl=es->clusters.data[ci];int gi=gm->cluster_map[ci];if(gi<0||gi>=nb||!cl->bone_node)continue;
        ufbx_matrix sm=ufbx_matrix_mul(&cl->bone_node->node_to_world,&cl->geometry_to_bone);out[gi]=mat4_from_ufbx(&sm);}
}


/* ── Skinned Shader ─────────────────────────────────────────── */
static const char *VERT_SKIN=
"#version 330 core\n"
"layout(location=0)in vec3 aPos;\nlayout(location=1)in vec3 aNormal;\n"
"layout(location=2)in vec2 aUV;\nlayout(location=3)in vec4 aWeights;\nlayout(location=4)in ivec4 aBones;\n"
"uniform mat4 uProj,uView,uModel,uBones[128];\n"
"out vec3 vNormal,vFragPos;out vec2 vUV;\n"
"void main(){\n"
"    mat4 skin=aWeights.x*uBones[aBones.x]+aWeights.y*uBones[aBones.y]+aWeights.z*uBones[aBones.z]+aWeights.w*uBones[aBones.w];\n"
"    if(dot(aWeights,vec4(1))<0.001)skin=mat4(1);\n"
"    vec4 world=uModel*skin*vec4(aPos,1);\n"
"    gl_Position=uProj*uView*world;vFragPos=world.xyz;\n"
"    vNormal=normalize(mat3(transpose(inverse(uModel*skin)))*aNormal);vUV=aUV;}\n";

static const char *FRAG_SKIN=
"#version 330 core\n"
"in vec3 vNormal,vFragPos;in vec2 vUV;\n"
"uniform sampler2D uAlbedo;uniform bool uHasTexture;\n"
"uniform vec3 uBaseColor;uniform float uFlash;\n"
LIGHTING_UNIFORMS
"out vec4 FragColor;\nvoid main(){\n"
"    vec3 n=normalize(vNormal);\n"
"    vec3 base=uHasTexture?texture(uAlbedo,vUV).rgb:uBaseColor;\n"
"    vec3 vPos=vFragPos;\n"
LIGHTING_CALC
"    col=mix(col,vec3(1.0,0.1,0.1),uFlash);\n"
"    FragColor=vec4(col,1.0);}\n";

typedef struct{GLuint prog;GLint loc_model,loc_bones;}SkinProg;
static void skinprog_init(SkinProg *sp){sp->prog=GLH_program(VERT_SKIN,FRAG_SKIN);sp->loc_model=glGetUniformLocation(sp->prog,"uModel");sp->loc_bones=glGetUniformLocation(sp->prog,"uBones[0]");}

static Mat4 entity_mat(Vec3 pos,float yaw,float dy,float scale,float cx,float cy,float cz){
    return mat4_mul(mat4_translate(pos.x,pos.y+dy,pos.z),mat4_mul(mat4_rot_y(yaw),mat4_mul(mat4_scale_u(scale),mat4_translate(-cx,-cy,-cz))));
}

static void draw_scene(SkinProg *sp,Scene *sc,Mat4 wm,int ai,double at,float flash,Mat4 proj,Mat4 view,LightingSystem *ls){
    lighting_push(ls,sp->prog);GLH_begin(sp->prog,proj.m,view.m);GLH_float(sp->prog,"uFlash",flash);
    ufbx_scene *eval=NULL;
    if(sc->scene->anim_stacks.count>0&&ai>=0){int idx=ai<(int)sc->scene->anim_stacks.count?ai:(int)sc->scene->anim_stacks.count-1;ufbx_anim_stack *stk=sc->scene->anim_stacks.data[idx];double real_end=anim_stack_real_end(stk);double dur=real_end-stk->time_begin;double t=dur>0?fmod(at,dur):0;eval=ufbx_evaluate_scene(sc->scene,stk->anim,stk->time_begin+t,NULL,NULL);}
    Mat4 bone_mats[MAX_BONES];
    for(int mi=0;mi<sc->num_meshes;mi++){GpuMesh *gm=&sc->meshes[mi];Mat4 mesh_mat;
        if(sc->skins[mi])mesh_mat=wm;
        else{ufbx_matrix g2w;if(eval&&gm->node_typed_id<eval->nodes.count)g2w=eval->nodes.data[gm->node_typed_id]->geometry_to_world;else memcpy(&g2w,&gm->geometry_to_world,sizeof(ufbx_matrix));mesh_mat=mat4_mul(wm,mat4_from_ufbx(&g2w));}
        glUniformMatrix4fv(sp->loc_model,1,GL_FALSE,mesh_mat.m);
        if(gm->texture){GLH_bind_texture(sp->prog,"uAlbedo",gm->texture,0);GLH_bool(sp->prog,"uHasTexture",1);}
        else{GLH_bool(sp->prog,"uHasTexture",0);GLH_vec3(sp->prog,"uBaseColor",0.75f,0.75f,0.8f);}
        for(int i=0;i<MAX_BONES;i++)bone_mats[i]=mat4_identity();
        compute_bones(bone_mats,gm->num_gpu_bones,gm,sc->skins[mi],eval);
        glUniformMatrix4fv(sp->loc_bones,MAX_BONES,GL_FALSE,bone_mats[0].m);
        glBindVertexArray(gm->vao);glDrawElements(GL_TRIANGLES,gm->index_count,GL_UNSIGNED_INT,0);}
    if(eval)ufbx_free_scene(eval);
}


/* ── Village ────────────────────────────────────────────────── */
typedef struct{Scene scene;CollWorld coll;GLuint prog;}Village;
static bool village_load(Village *v,const char *path){
    if(!scene_load(&v->scene,path))return false;
    for(size_t ni=0;ni<v->scene.scene->nodes.count;ni++){ufbx_node *nd=v->scene.scene->nodes.data[ni];if(nd->mesh)coll_add(&v->coll,nd->mesh,nd);}
    printf("[Village] %d collision boxes\n",v->coll.count);
    v->prog=GLH_program(
        "#version 330 core\n"
        "layout(location=0)in vec3 aPos;\nlayout(location=1)in vec3 aNormal;\nlayout(location=2)in vec2 aUV;\n"
        "layout(location=3)in vec4 aWeights;\nlayout(location=4)in ivec4 aBones;\n"
        "uniform mat4 uProj,uView,uModel;\nout vec3 vNormal,vPos;out vec2 vUV;\n"
        "void main(){vec4 w=uModel*vec4(aPos,1);gl_Position=uProj*uView*w;vNormal=normalize(mat3(transpose(inverse(uModel)))*aNormal);vPos=w.xyz;vUV=aUV;}\n",
        "#version 330 core\nin vec3 vNormal,vPos;in vec2 vUV;\n"
        "uniform sampler2D uAlbedo;uniform bool uHasTexture;uniform vec3 uBaseColor;\n"
        LIGHTING_UNIFORMS
        "out vec4 FragColor;\nvoid main(){\n"
        "    vec3 n=normalize(vNormal),base=uHasTexture?texture(uAlbedo,vUV).rgb:uBaseColor;\n"
        LIGHTING_CALC
        "    FragColor=vec4(col,1.0);}\n");
    return true;
}
static void village_draw(Village *v,LightingSystem *ls,Mat4 proj,Mat4 view){
    lighting_push(ls,v->prog);GLH_begin(v->prog,proj.m,view.m);
    float vs=v->scene.model_scale*20.0f;Mat4 world=mat4_scale_u(vs);
    glUniformMatrix4fv(glGetUniformLocation(v->prog,"uModel"),1,GL_FALSE,world.m);
    for(int mi=0;mi<v->scene.num_meshes;mi++){GpuMesh *gm=&v->scene.meshes[mi];
        if(gm->texture){GLH_bind_texture(v->prog,"uAlbedo",gm->texture,0);GLH_bool(v->prog,"uHasTexture",1);}
        else{GLH_bool(v->prog,"uHasTexture",0);GLH_vec3(v->prog,"uBaseColor",0.7f,0.65f,0.55f);}
        glBindVertexArray(gm->vao);glDrawElements(GL_TRIANGLES,gm->index_count,GL_UNSIGNED_INT,0);}
}

/* Render static village mesh into shadow map */
static void shadow_village(ShadowMap *sm,LightingSystem *ls,Village *v){
    glUseProgram(sm->prog_depth);
    glUniformMatrix4fv(glGetUniformLocation(sm->prog_depth,"uLightVP"),1,GL_FALSE,ls->light_vp.m);
    float vs=v->scene.model_scale*20.0f;Mat4 world=mat4_scale_u(vs);
    glUniformMatrix4fv(glGetUniformLocation(sm->prog_depth,"uModel"),1,GL_FALSE,world.m);
    for(int mi=0;mi<v->scene.num_meshes;mi++){glBindVertexArray(v->scene.meshes[mi].vao);glDrawElements(GL_TRIANGLES,v->scene.meshes[mi].index_count,GL_UNSIGNED_INT,0);}
    glBindVertexArray(0);
}
/* Render skinned character into shadow map */
static void shadow_scene(ShadowMap *sm,LightingSystem *ls,Scene *sc,Mat4 wm,int ai,double at){
    glUseProgram(sm->prog_skin_depth);
    GLint loc_lvp=glGetUniformLocation(sm->prog_skin_depth,"uLightVP");
    GLint loc_mod=glGetUniformLocation(sm->prog_skin_depth,"uModel");
    GLint loc_bon=glGetUniformLocation(sm->prog_skin_depth,"uBones[0]");
    glUniformMatrix4fv(loc_lvp,1,GL_FALSE,ls->light_vp.m);
    ufbx_scene *eval=NULL;
    if(sc->scene->anim_stacks.count>0&&ai>=0){
        int idx=ai<(int)sc->scene->anim_stacks.count?ai:(int)sc->scene->anim_stacks.count-1;
        ufbx_anim_stack *stk=sc->scene->anim_stacks.data[idx];
        double real_end=anim_stack_real_end(stk);
        double dur=real_end-stk->time_begin;double t=dur>0?fmod(at,dur):0;
        eval=ufbx_evaluate_scene(sc->scene,stk->anim,stk->time_begin+t,NULL,NULL);
    }
    Mat4 bone_mats[MAX_BONES];
    for(int mi=0;mi<sc->num_meshes;mi++){
        GpuMesh *gm=&sc->meshes[mi];Mat4 mm;
        if(sc->skins[mi])mm=wm;
        else{ufbx_matrix g2w;if(eval&&gm->node_typed_id<eval->nodes.count)g2w=eval->nodes.data[gm->node_typed_id]->geometry_to_world;else memcpy(&g2w,&gm->geometry_to_world,sizeof(ufbx_matrix));mm=mat4_mul(wm,mat4_from_ufbx(&g2w));}
        glUniformMatrix4fv(loc_mod,1,GL_FALSE,mm.m);
        for(int i=0;i<MAX_BONES;i++)bone_mats[i]=mat4_identity();
        compute_bones(bone_mats,gm->num_gpu_bones,gm,sc->skins[mi],eval);
        glUniformMatrix4fv(loc_bon,MAX_BONES,GL_FALSE,bone_mats[0].m);
        glBindVertexArray(gm->vao);glDrawElements(GL_TRIANGLES,gm->index_count,GL_UNSIGNED_INT,0);
    }
    if(eval)ufbx_free_scene(eval);
    glBindVertexArray(0);
}

/* ── Skybox ─────────────────────────────────────────────────── */
typedef struct{GLuint prog,vao,tex;bool has_tex;}Skybox;
static void skybox_init(Skybox *sky,const char *img){
    float verts[]={-1,1,-1,-1,-1,-1,1,-1,-1,1,-1,-1,1,1,-1,-1,1,-1,-1,-1,1,-1,-1,-1,-1,1,-1,-1,1,-1,-1,1,1,-1,-1,1,1,-1,-1,1,-1,1,1,1,1,1,1,1,1,1,-1,1,-1,-1,-1,-1,1,-1,1,1,1,1,1,1,1,1,1,-1,1,-1,-1,1,-1,1,-1,1,1,-1,1,1,1,1,1,1,-1,1,1,-1,1,-1,-1,-1,-1,-1,-1,1,1,-1,-1,1,-1,-1,-1,-1,1,1,-1,1};
    GLH_Attrib a[]={{0,3,GL_FLOAT,GL_FALSE,0}};sky->vao=GLH_mesh(verts,sizeof(verts),NULL,0,12,a,1);
    sky->tex=0;sky->has_tex=false;
    if(img){int w,h,ch;uint8_t *px=stbi_load(img,&w,&h,&ch,0);if(px){sky->tex=GLH_texture_pixels(px,w,h,ch);stbi_image_free(px);sky->has_tex=true;printf("[Skybox] Loaded %s\n",img);}else printf("[Skybox] '%s' not found, using procedural sky\n",img);}
    sky->prog=GLH_program(
        "#version 330 core\nlayout(location=0)in vec3 aPos;\nuniform mat4 uProj,uView;\nout vec3 vDir;\n"
        "void main(){vDir=aPos;mat4 v=uView;v[3]=vec4(0,0,0,1);vec4 p=uProj*v*vec4(aPos,1);gl_Position=p.xyww;}\n",
        "#version 330 core\n#define PI 3.14159265\nin vec3 vDir;\n"
        "uniform sampler2D uSkyTex;uniform bool uHasTex;\n"
        "uniform vec3 uSunDir;\n"   /* scene→sun, normalized */
        "uniform float uSunElev;\n" /* -1..1 */
        "out vec4 F;\n"
        "void main(){\n"
        "    vec3 d=normalize(vDir);\n"
        "    if(uHasTex){\n"
        "        float u=0.5+atan(d.z,d.x)/(2.0*PI);\n"
        "        float v=0.5-asin(clamp(d.y,-1,1))/PI;\n"
        "        F=texture(uSkyTex,vec2(u,v));return;}\n"
        /* procedural sky */
        "    float h=clamp(d.y,0.0,1.0);\n"
        "    float day_t=clamp(uSunElev*3.0+0.3,0.0,1.0);\n"
        "    float dawn_t=clamp(1.0-abs(uSunElev)*5.0,0.0,1.0);\n"
        /* zenith & horizon colours */
        "    vec3 night_z=vec3(0.01,0.01,0.06), night_h=vec3(0.03,0.03,0.10);\n"
        "    vec3 day_z  =vec3(0.10,0.35,0.85), day_h  =vec3(0.55,0.75,0.95);\n"
        "    vec3 dawn_h =vec3(0.95,0.45,0.10);\n"
        "    vec3 zenith =mix(night_z,day_z,day_t);\n"
        "    vec3 horiz  =mix(night_h,mix(day_h,dawn_h,dawn_t*0.9),day_t+dawn_t*0.4);\n"
        "    float sky_t =clamp(h*2.5,0.0,1.0);\n"
        "    vec3 sky=mix(horiz,zenith,sky_t);\n"
        /* sun glow + disk */
        "    if(uSunElev>-0.15){\n"
        "        float sd=dot(d,uSunDir);\n"
        "        float glow=clamp(pow(max(sd,0.0),6.0)*0.6,0.0,1.0);\n"
        "        float disk=sd>0.9995?1.0:0.0;\n"
        "        vec3 sc=mix(vec3(1.0,0.55,0.15),vec3(1.0,0.97,0.88),clamp(uSunElev*3.0,0,1));\n"
        "        sky+=sc*glow*clamp(uSunElev+0.2,0.0,1.0);\n"
        "        sky=mix(sky,sc*1.8,disk);}\n"
        /* moon + glow */
        "    if(uSunElev<0.1){\n"
        "        vec3 md=-uSunDir;\n"
        "        float msd=dot(d,md);\n"
        "        float mglow=clamp(pow(max(msd,0.0),20.0)*0.2,0.0,1.0);\n"
        "        float mdisk=msd>0.9998?1.0:0.0;\n"
        "        float nt=clamp(-uSunElev*2.0,0.0,1.0);\n"
        "        sky+=vec3(0.85,0.88,0.95)*mdisk+vec3(0.3,0.33,0.45)*mglow*nt;}\n"
        "    F=vec4(sky,1.0);}\n");
}
static void skybox_draw(Skybox *sky,Mat4 proj,Mat4 view,LightingSystem *ls){
    glDepthFunc(GL_LEQUAL);glDepthMask(GL_FALSE);GLH_begin(sky->prog,proj.m,view.m);
    /* pass sun direction (scene→sun = negated ls->sun_dir) */
    Vec3 sun_up={-ls->sun_dir.x,-ls->sun_dir.y,-ls->sun_dir.z};
    GLH_vec3(sky->prog,"uSunDir",sun_up.x,sun_up.y,sun_up.z);
    GLH_float(sky->prog,"uSunElev",ls->sun_elev);
    if(sky->has_tex){GLH_bind_texture(sky->prog,"uSkyTex",sky->tex,0);GLH_bool(sky->prog,"uHasTex",1);}else GLH_bool(sky->prog,"uHasTex",0);
    GLH_draw(sky->vao,GL_TRIANGLES,36);glDepthMask(GL_TRUE);glDepthFunc(GL_LESS);
}


/* ── Particles ──────────────────────────────────────────────── */
typedef struct{Vec3 pos,vel;float life,max_life,size,r,g,b,a;bool active;}Particle;
typedef struct{Vec3 pos;float rate,accum,r,g,b,size,speed;bool active;}Emitter;
typedef struct{Particle particles[MAX_PARTICLES];Emitter emitters[MAX_EMITTERS];int pcount,ecount;GLuint prog,vao,vbo;}ParticleSystem;

static void ps_init(ParticleSystem *ps){
    memset(ps,0,sizeof(*ps));
    ps->prog=GLH_program(
        "#version 330 core\nlayout(location=0)in vec3 aPos;\nlayout(location=1)in vec4 aColor;\nlayout(location=2)in float aSize;\nuniform mat4 uProj,uView;\nout vec4 vColor;\nvoid main(){gl_Position=uProj*uView*vec4(aPos,1);gl_PointSize=aSize*200.0/gl_Position.w;vColor=aColor;}\n",
        "#version 330 core\nin vec4 vColor;out vec4 F;\nvoid main(){vec2 uv=gl_PointCoord*2.0-1.0;float d=dot(uv,uv);if(d>1.0)discard;F=vec4(vColor.rgb,vColor.a*(1.0-d));}\n");
    GLH_Attrib a[]={{0,3,GL_FLOAT,GL_FALSE,0},{1,4,GL_FLOAT,GL_FALSE,12},{2,1,GL_FLOAT,GL_FALSE,28}};
    ps->vbo=GLH_vbo(MAX_PARTICLES*32);ps->vao=GLH_mesh_dynamic(MAX_PARTICLES*32,32,a,3);
    glBindVertexArray(ps->vao);glBindBuffer(GL_ARRAY_BUFFER,ps->vbo);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,32,(void*)0);glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,32,(void*)12);glVertexAttribPointer(2,1,GL_FLOAT,GL_FALSE,32,(void*)28);
    glBindVertexArray(0);
}
static void ps_spawn(ParticleSystem *ps,Vec3 pos,Vec3 vel,float life,float size,float r,float g,float b){
    for(int i=0;i<MAX_PARTICLES;i++){Particle *p=&ps->particles[i];if(p->active)continue;p->pos=pos;p->vel=vel;p->life=p->max_life=life;p->size=size;p->r=r;p->g=g;p->b=b;p->a=1;p->active=true;if(i>=ps->pcount)ps->pcount=i+1;return;}
}
static void ps_burst(ParticleSystem *ps,Vec3 pos,float r,float g,float b){
    for(int i=0;i<16;i++){float a=(float)i/16.0f*2.0f*(float)M_PI;Vec3 vel={cosf(a)*2.5f,3.0f+(float)rand()/RAND_MAX*2.0f,sinf(a)*2.5f};ps_spawn(ps,pos,vel,0.6f+(float)rand()/RAND_MAX*0.4f,0.12f+(float)rand()/RAND_MAX*0.08f,r,g,b);}
}
static int ps_add_emitter(ParticleSystem *ps,Vec3 pos,float rate,float r,float g,float b,float size,float speed){
    if(ps->ecount>=MAX_EMITTERS)return -1;int i=ps->ecount++;ps->emitters[i]=(Emitter){pos,rate,0,r,g,b,size,speed,true};return i;
}
static void ps_update(ParticleSystem *ps,float dt){
    for(int e=0;e<ps->ecount;e++){Emitter *em=&ps->emitters[e];if(!em->active)continue;em->accum+=dt;float inv=1.0f/em->rate;
        while(em->accum>=inv){em->accum-=inv;float ax=(float)(rand()%100-50)*0.01f,az=(float)(rand()%100-50)*0.01f;Vec3 vel={ax*em->speed,em->speed*(0.8f+(float)rand()/RAND_MAX*0.4f),az*em->speed};ps_spawn(ps,em->pos,vel,0.5f+(float)rand()/RAND_MAX*0.5f,em->size*(0.8f+(float)rand()/RAND_MAX*0.4f),em->r,em->g,em->b);}}
    for(int i=0;i<ps->pcount;i++){Particle *p=&ps->particles[i];if(!p->active)continue;p->vel.y+=GRAVITY*0.05f*dt;p->pos=vec3_add(p->pos,vec3_scale(p->vel,dt));p->life-=dt;p->a=p->life/p->max_life;p->size*=0.98f;if(p->life<=0)p->active=false;}
}
static void ps_draw(ParticleSystem *ps,Mat4 proj,Mat4 view){
    float buf[MAX_PARTICLES*8];int n=0;
    for(int i=0;i<ps->pcount;i++){Particle *p=&ps->particles[i];if(!p->active)continue;buf[n*8+0]=p->pos.x;buf[n*8+1]=p->pos.y;buf[n*8+2]=p->pos.z;buf[n*8+3]=p->r;buf[n*8+4]=p->g;buf[n*8+5]=p->b;buf[n*8+6]=p->a;buf[n*8+7]=p->size;n++;}
    if(!n)return;
    GLH_vbo_update(ps->vbo,buf,(size_t)n*8*sizeof(float));
    glBindVertexArray(ps->vao);glBindBuffer(GL_ARRAY_BUFFER,ps->vbo);GLH_begin(ps->prog,proj.m,view.m);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE);glDepthMask(GL_FALSE);glEnable(GL_PROGRAM_POINT_SIZE);
    glDrawArrays(GL_POINTS,0,n);glDepthMask(GL_TRUE);glDisable(GL_BLEND);
}

/* ── Campfire ───────────────────────────────────────────────── */
typedef struct{Vec3 pos;int fire_em,smoke_em,spark_em,light_idx;}Campfire;
static Campfire campfire_create(ParticleSystem *ps,LightingSystem *ls,Vec3 pos){
    Campfire cf;cf.pos=pos;
    cf.fire_em =ps_add_emitter(ps,pos,40,1.00f,0.50f,0.05f,0.15f,2.5f);
    cf.smoke_em=ps_add_emitter(ps,(Vec3){pos.x,pos.y+0.4f,pos.z},8,0.30f,0.30f,0.30f,0.20f,0.8f);
    cf.spark_em=ps_add_emitter(ps,pos,15,1.00f,0.90f,0.30f,0.06f,4.0f);
    Vec3 fc={1.0f,0.55f,0.15f};cf.light_idx=lighting_add_point(ls,pos,fc,12.0f,1.0f);
    if(cf.light_idx>=0){ls->points[cf.light_idx].flicker=true;ls->points[cf.light_idx].flicker_speed=7.0f;ls->points[cf.light_idx].flicker_phase=0.3f;}
    printf("[Campfire] (%.1f,%.1f,%.1f)\n",(double)pos.x,(double)pos.y,(double)pos.z);return cf;
}


/* ── Quest / Ability / Physics / Camera ─────────────────────── */
typedef enum{OBJ_KILL,OBJ_REACH}ObjType;
typedef struct{ObjType type;char desc[64];int current,required;bool done;Vec3 loc;float radius;}Objective;
typedef struct{int id;char name[64];char desc[128];Objective objs[MAX_OBJECTIVES];int obj_count,gold,xp;enum{Q_INACTIVE,Q_ACTIVE,Q_DONE}status;}Quest;
typedef struct{Quest quests[MAX_QUESTS];int count,next_id,player_gold,player_xp,player_level;}QuestSystem;

static void quest_init(QuestSystem *qs){memset(qs,0,sizeof(*qs));qs->next_id=1;qs->player_level=1;}
static Quest *quest_add(QuestSystem *qs,const char *name,const char *desc){if(qs->count>=MAX_QUESTS)return NULL;Quest *q=&qs->quests[qs->count++];memset(q,0,sizeof(*q));q->id=qs->next_id++;strncpy(q->name,name,63);strncpy(q->desc,desc,127);q->status=Q_INACTIVE;return q;}
static void quest_kill_obj(Quest *q,const char *d,int n){if(!q||q->obj_count>=MAX_OBJECTIVES)return;Objective *o=&q->objs[q->obj_count++];memset(o,0,sizeof(*o));o->type=OBJ_KILL;strncpy(o->desc,d,63);o->required=n;}
static void quest_reach_obj(Quest *q,const char *d,Vec3 loc,float r){if(!q||q->obj_count>=MAX_OBJECTIVES)return;Objective *o=&q->objs[q->obj_count++];memset(o,0,sizeof(*o));o->type=OBJ_REACH;strncpy(o->desc,d,63);o->required=1;o->loc=loc;o->radius=r;}
static void quest_start(QuestSystem *qs,int id){for(int i=0;i<qs->count;i++)if(qs->quests[i].id==id){qs->quests[i].status=Q_ACTIVE;return;}}
static void quest_notify_kill(QuestSystem *qs){for(int i=0;i<qs->count;i++){Quest *q=&qs->quests[i];if(q->status!=Q_ACTIVE)continue;for(int j=0;j<q->obj_count;j++){Objective *o=&q->objs[j];if(o->type==OBJ_KILL&&!o->done){o->current++;if(o->current>=o->required)o->done=true;}}}}
static void quest_update(QuestSystem *qs,Vec3 pp){
    for(int i=0;i<qs->count;i++){Quest *q=&qs->quests[i];if(q->status!=Q_ACTIVE)continue;
        for(int j=0;j<q->obj_count;j++){Objective *o=&q->objs[j];if(o->type==OBJ_REACH&&!o->done&&vec3_dist(pp,o->loc)<=o->radius)o->done=true;}
        bool all=true;for(int j=0;j<q->obj_count;j++)if(!q->objs[j].done){all=false;break;}
        if(all){q->status=Q_DONE;qs->player_gold+=q->gold;qs->player_xp+=q->xp;printf("[Quest] '%s' complete!\n",q->name);}}
}

typedef struct{char name[32];float cost,cd,cd_timer;bool ready;}Ability;
typedef struct{Ability slots[HOTBAR_SLOTS];float mana,max_mana,mp_regen,stam,max_stam,sp_regen;}AbilitySystem;
static void ability_init(AbilitySystem *as){memset(as,0,sizeof(*as));as->mana=as->max_mana=100;as->mp_regen=3;as->stam=as->max_stam=100;as->sp_regen=8;const char *n[]={"Fireball","Heal","Dash","Shield","Meteor"};float c[]={20,25,0,15,40},cd[]={3,8,4,20,15};for(int i=0;i<HOTBAR_SLOTS;i++){strncpy(as->slots[i].name,n[i],31);as->slots[i].cost=c[i];as->slots[i].cd=cd[i];as->slots[i].ready=true;}}
static void ability_update(AbilitySystem *as,float dt){as->mana=clampf(as->mana+as->mp_regen*dt,0,as->max_mana);as->stam=clampf(as->stam+as->sp_regen*dt,0,as->max_stam);for(int i=0;i<HOTBAR_SLOTS;i++){Ability *ab=&as->slots[i];if(!ab->ready){ab->cd_timer-=dt;if(ab->cd_timer<=0){ab->ready=true;ab->cd_timer=0;}}}}
static bool ability_use(AbilitySystem *as,int slot){if(slot<0||slot>=HOTBAR_SLOTS)return false;Ability *ab=&as->slots[slot];if(!ab->ready||as->mana<ab->cost)return false;as->mana-=ab->cost;ab->ready=false;ab->cd_timer=ab->cd;printf("[Ability] %s\n",ab->name);return true;}

/* ── Hit Detection (capsule vs capsule) ─────────────────────── */
/* Squared distance between closest points on two line segments */
static float seg_seg_dist2(Vec3 p0,Vec3 p1,Vec3 q0,Vec3 q1){
    Vec3 d1=vec3_sub(p1,p0),d2=vec3_sub(q1,q0),r=vec3_sub(p0,q0);
    float a=vec3_dot(d1,d1),e=vec3_dot(d2,d2),f=vec3_dot(d2,r);
    float s,t;
    if(a<1e-6f&&e<1e-6f){Vec3 d=vec3_sub(p0,q0);return vec3_dot(d,d);}
    if(a<1e-6f){t=clampf(f/e,0,1);s=0;}
    else{float c=vec3_dot(d1,r);
        if(e<1e-6f){s=clampf(-c/a,0,1);t=0;}
        else{float b=vec3_dot(d1,d2),denom=a*e-b*b;
            s=denom>1e-6f?clampf((b*f-c*e)/denom,0,1):0;
            t=clampf((b*s+f)/e,0,1);
            s=clampf((b*t-c)/a,0,1);
            t=clampf((b*s+f)/e,0,1);}}
    Vec3 cl=vec3_sub(vec3_add(p0,vec3_scale(d1,s)),vec3_add(q0,vec3_scale(d2,t)));
    return vec3_dot(cl,cl);
}
/* True if two capsules overlap */
static bool capsule_capsule(Vec3 p0,Vec3 p1,float r0,Vec3 q0,Vec3 q1,float r1){
    float r=r0+r1;return seg_seg_dist2(p0,p1,q0,q1)<r*r;
}

typedef struct{Vec3 pos,vel;float yaw;bool on_ground;float height,radius;}PhysBody;
static void phys_update(PhysBody *b,float dt,float gy){if(!b->on_ground)b->vel.y+=GRAVITY*dt;b->pos=vec3_add(b->pos,vec3_scale(b->vel,dt));if(b->pos.y<=gy){b->pos.y=gy;if(b->vel.y<0)b->vel.y=0;b->on_ground=true;}else b->on_ground=false;}

typedef enum{PS_IDLE,PS_WALK,PS_RUN,PS_ATTACK,PS_ATTACK2,PS_JUMP,PS_ROLL,PS_BLOCK,PS_HIT,PS_DEAD}PlayerState;
typedef struct{PhysBody phys;PlayerState state;float health,max_health;double anim_time;float anim_speed;int anim_idx,prev_anim_idx;float attack_timer;bool attack_hit;float hit_flash;float roll_timer;float hit_timer;bool blocking;}Player;
typedef enum{ES_IDLE,ES_CHASE,ES_ATTACK,ES_STAGGER,ES_DEAD}EnemyState;
typedef struct{PhysBody phys;EnemyState state;float health,max_health;double anim_time;float anim_speed;int anim_idx;float attack_timer,attack_cooldown,stagger_timer;bool active;}Enemy;

typedef struct{float yaw,pitch,dist;Vec3 pos,target;}Camera3P;
static void cam_update(Camera3P *c,Vec3 follow,float dt){Vec3 want=vec3_add(follow,(Vec3){0,1.3f,0});c->target=vec3_add(c->target,vec3_scale(vec3_sub(want,c->target),10.0f*dt));c->pos.x=c->target.x+c->dist*cosf(c->pitch)*sinf(c->yaw);c->pos.y=c->target.y+c->dist*sinf(c->pitch);c->pos.z=c->target.z+c->dist*cosf(c->pitch)*cosf(c->yaw);}
/* FIX: W = move away from camera (into scene) */
static Vec3 cam_forward(Camera3P *c){return vec3_norm((Vec3){-sinf(c->yaw),0,-cosf(c->yaw)});}
static Vec3 cam_right(Camera3P *c){Vec3 f=cam_forward(c);return (Vec3){-f.z,0,f.x};}


/* ── Line Renderer (debug wireframes) ───────────────────────── */
#define LINE_MAX_VERTS 8192   /* x,y,z,r,g,b per vert */
static float g_line_buf[LINE_MAX_VERTS*6];
static int   g_line_count=0;
typedef struct{GLuint prog,vao,vbo;}LineRenderer;
static void lr_init(LineRenderer *lr){
    lr->prog=GLH_program(
        "#version 330 core\nlayout(location=0)in vec3 aPos;\nlayout(location=1)in vec3 aColor;\nuniform mat4 uProj,uView;\nout vec3 vColor;\nvoid main(){gl_Position=uProj*uView*vec4(aPos,1);vColor=aColor;}\n",
        "#version 330 core\nin vec3 vColor;out vec4 F;\nvoid main(){F=vec4(vColor,1.0);}\n");
    GLH_Attrib a[]={{0,3,GL_FLOAT,GL_FALSE,0},{1,3,GL_FLOAT,GL_FALSE,12}};
    lr->vbo=GLH_vbo(LINE_MAX_VERTS*6*sizeof(float));
    lr->vao=GLH_mesh_dynamic(LINE_MAX_VERTS*6*sizeof(float),24,a,2);
    glBindVertexArray(lr->vao);glBindBuffer(GL_ARRAY_BUFFER,lr->vbo);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,24,(void*)0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,24,(void*)12);
    glBindVertexArray(0);
}
static void lr_push(Vec3 a,Vec3 b,float r,float g,float bl){
    if(g_line_count+2>LINE_MAX_VERTS)return;
    float *p=g_line_buf+g_line_count*6;
    p[0]=a.x;p[1]=a.y;p[2]=a.z;p[3]=r;p[4]=g;p[5]=bl;
    p[6]=b.x;p[7]=b.y;p[8]=b.z;p[9]=r;p[10]=g;p[11]=bl;
    g_line_count+=2;
}
static void lr_box(AABB *box,float r,float g,float b){
    Vec3 mn=box->min,mx=box->max;
    Vec3 c[8]={
        {mn.x,mn.y,mn.z},{mx.x,mn.y,mn.z},{mx.x,mn.y,mx.z},{mn.x,mn.y,mx.z},
        {mn.x,mx.y,mn.z},{mx.x,mx.y,mn.z},{mx.x,mx.y,mx.z},{mn.x,mx.y,mx.z}
    };
    int edges[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
    for(int i=0;i<12;i++)lr_push(c[edges[i][0]],c[edges[i][1]],r,g,b);
}
/* Draw a capsule approximation: 2 rings + 4 spine lines */
static void lr_capsule(Vec3 base,float height,float radius,float r,float g,float b){
    Vec3 top={base.x,base.y+height,base.z};
    int segs=12;
    for(int i=0;i<segs;i++){
        float a0=(float)i/(float)segs*2.0f*(float)M_PI;
        float a1=(float)(i+1)/(float)segs*2.0f*(float)M_PI;
        Vec3 p0b={base.x+cosf(a0)*radius,base.y,base.z+sinf(a0)*radius};
        Vec3 p1b={base.x+cosf(a1)*radius,base.y,base.z+sinf(a1)*radius};
        Vec3 p0t={top.x+cosf(a0)*radius, top.y, top.z+sinf(a0)*radius};
        Vec3 p1t={top.x+cosf(a1)*radius, top.y, top.z+sinf(a1)*radius};
        lr_push(p0b,p1b,r,g,b);
        lr_push(p0t,p1t,r,g,b);
        if(i%3==0)lr_push(p0b,p0t,r,g,b); /* vertical stripes */
    }
}
/* Axis cross at a world position */
static void lr_axis(Vec3 pos,float sz){
    lr_push(pos,(Vec3){pos.x+sz,pos.y,pos.z},1,0,0);
    lr_push(pos,(Vec3){pos.x,pos.y+sz,pos.z},0,1,0);
    lr_push(pos,(Vec3){pos.x,pos.y,pos.z+sz},0,0,1);
}
static void lr_flush(LineRenderer *lr,Mat4 proj,Mat4 view){
    if(!g_line_count)return;
    GLH_vbo_update(lr->vbo,g_line_buf,(size_t)g_line_count*6*sizeof(float));
    glUseProgram(lr->prog);GLH_mat4(lr->prog,"uProj",proj.m);GLH_mat4(lr->prog,"uView",view.m);
    glBindVertexArray(lr->vao);glBindBuffer(GL_ARRAY_BUFFER,lr->vbo);
    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_LINES,0,g_line_count);
    glEnable(GL_DEPTH_TEST);glBindVertexArray(0);
    g_line_count=0;
}

/* ── Debug overlay ───────────────────────────────────────────── */
static const char *player_state_name(PlayerState s){
    switch(s){
    case PS_IDLE:   return "IDLE";
    case PS_WALK:   return "WALK";
    case PS_RUN:    return "RUN";
    case PS_ATTACK: return "ATTACK";
    case PS_ATTACK2:return "ATTACK2";
    case PS_JUMP:   return "JUMP";
    case PS_ROLL:   return "ROLL";
    case PS_BLOCK:  return "BLOCK";
    case PS_HIT:    return "HIT";
    case PS_DEAD:   return "DEAD";
    default:        return "?";
    }
}
static const char *enemy_state_name(EnemyState s){
    switch(s){case ES_IDLE:return "IDLE";case ES_CHASE:return "CHASE";case ES_ATTACK:return "ATTACK";case ES_STAGGER:return "STAGGER";case ES_DEAD:return "DEAD";default:return "?";}
}

typedef struct{
    float fps;
    float frame_ms;
    float fps_accum;
    int   fps_frames;
    float fps_timer;
} PerfCounter;
static void perf_update(PerfCounter *pc,float dt){
    pc->frame_ms=dt*1000.0f;
    pc->fps_accum+=1.0f/dt;
    pc->fps_frames++;
    pc->fps_timer+=dt;
    if(pc->fps_timer>=0.5f){
        pc->fps=pc->fps_accum/(float)pc->fps_frames;
        pc->fps_accum=0;pc->fps_frames=0;pc->fps_timer=0;
    }
}




/* 8×8 bitmap font for ASCII 32-127.  Row 0 = top, bit7 = left pixel. */
static const uint8_t g_font[96][8]={
/*32 ' '*/{0,0,0,0,0,0,0,0},           /*33 '!'*/{0x10,0x10,0x10,0x10,0x10,0,0x10,0},
/*34 '"'*/{0x28,0x28,0x28,0,0,0,0,0},  /*35 '#'*/{0x28,0x28,0x7C,0x28,0x7C,0x28,0x28,0},
/*36 '$'*/{0x10,0x3C,0x50,0x38,0x14,0x78,0x10,0},/*37 '%'*/{0x62,0x64,8,0x10,0x26,0x46,0,0},
/*38 '&'*/{0x30,0x48,0x50,0x20,0x54,0x48,0x34,0},/*39'\'' */{0x10,0x10,0x20,0,0,0,0,0},
/*40 '('*/{8,0x10,0x20,0x20,0x20,0x10,8,0},      /*41 ')'*/{0x20,0x10,8,8,8,0x10,0x20,0},
/*42 '*'*/{0,0x28,0x10,0x7C,0x10,0x28,0,0},      /*43 '+'*/{0,0x10,0x10,0x7C,0x10,0x10,0,0},
/*44 ','*/{0,0,0,0,0x18,0x18,0x10,0x20},          /*45 '-'*/{0,0,0,0x7C,0,0,0,0},
/*46 '.'*/{0,0,0,0,0,0x18,0x18,0},               /*47 '/'*/{4,8,8,0x10,0x20,0x40,0x40,0},
/*48 '0'*/{0x38,0x44,0x4C,0x54,0x64,0x44,0x38,0},/*49 '1'*/{0x10,0x30,0x10,0x10,0x10,0x10,0x38,0},
/*50 '2'*/{0x38,0x44,4,0x18,0x20,0x44,0x7C,0},   /*51 '3'*/{0x38,0x44,4,0x18,4,0x44,0x38,0},
/*52 '4'*/{8,0x18,0x28,0x48,0x7C,8,8,0},         /*53 '5'*/{0x7C,0x40,0x78,4,4,0x44,0x38,0},
/*54 '6'*/{0x18,0x20,0x40,0x78,0x44,0x44,0x38,0},/*55 '7'*/{0x7C,4,8,0x10,0x20,0x20,0x20,0},
/*56 '8'*/{0x38,0x44,0x44,0x38,0x44,0x44,0x38,0},/*57 '9'*/{0x38,0x44,0x44,0x3C,4,8,0x30,0},
/*58 ':'*/{0,0x18,0x18,0,0x18,0x18,0,0},         /*59 ';'*/{0,0x18,0x18,0,0x18,0x18,0x10,0x20},
/*60 '<'*/{8,0x10,0x20,0x40,0x20,0x10,8,0},      /*61 '='*/{0,0,0x7C,0,0x7C,0,0,0},
/*62 '>'*/{0x20,0x10,8,4,8,0x10,0x20,0},         /*63 '?'*/{0x38,0x44,4,0x18,0x10,0,0x10,0},
/*64 '@'*/{0x38,0x44,0x5C,0x54,0x5C,0x40,0x38,0},/*65 'A'*/{0x10,0x28,0x44,0x7C,0x44,0x44,0x44,0},
/*66 'B'*/{0x78,0x44,0x44,0x78,0x44,0x44,0x78,0},/*67 'C'*/{0x38,0x44,0x40,0x40,0x40,0x44,0x38,0},
/*68 'D'*/{0x70,0x48,0x44,0x44,0x44,0x48,0x70,0},/*69 'E'*/{0x7C,0x40,0x40,0x78,0x40,0x40,0x7C,0},
/*70 'F'*/{0x7C,0x40,0x40,0x78,0x40,0x40,0x40,0},/*71 'G'*/{0x38,0x44,0x40,0x5C,0x44,0x44,0x38,0},
/*72 'H'*/{0x44,0x44,0x44,0x7C,0x44,0x44,0x44,0},/*73 'I'*/{0x38,0x10,0x10,0x10,0x10,0x10,0x38,0},
/*74 'J'*/{0x1C,8,8,8,8,0x48,0x30,0},            /*75 'K'*/{0x44,0x48,0x50,0x60,0x50,0x48,0x44,0},
/*76 'L'*/{0x40,0x40,0x40,0x40,0x40,0x40,0x7C,0},/*77 'M'*/{0x44,0x6C,0x54,0x54,0x44,0x44,0x44,0},
/*78 'N'*/{0x44,0x64,0x54,0x4C,0x44,0x44,0x44,0},/*79 'O'*/{0x38,0x44,0x44,0x44,0x44,0x44,0x38,0},
/*80 'P'*/{0x78,0x44,0x44,0x78,0x40,0x40,0x40,0},/*81 'Q'*/{0x38,0x44,0x44,0x44,0x54,0x48,0x34,0},
/*82 'R'*/{0x78,0x44,0x44,0x78,0x50,0x48,0x44,0},/*83 'S'*/{0x38,0x44,0x40,0x38,4,0x44,0x38,0},
/*84 'T'*/{0x7C,0x10,0x10,0x10,0x10,0x10,0x10,0},/*85 'U'*/{0x44,0x44,0x44,0x44,0x44,0x44,0x38,0},
/*86 'V'*/{0x44,0x44,0x44,0x44,0x28,0x28,0x10,0},/*87 'W'*/{0x44,0x44,0x54,0x54,0x54,0x6C,0x44,0},
/*88 'X'*/{0x44,0x44,0x28,0x10,0x28,0x44,0x44,0},/*89 'Y'*/{0x44,0x44,0x28,0x10,0x10,0x10,0x10,0},
/*90 'Z'*/{0x7C,4,8,0x10,0x20,0x40,0x7C,0},      /*91 '['*/{0x38,0x20,0x20,0x20,0x20,0x20,0x38,0},
/*92'\\'*/{0x40,0x40,0x20,0x10,8,4,4,0},          /*93 ']'*/{0x38,8,8,8,8,8,0x38,0},
/*94 '^'*/{0x10,0x28,0x44,0,0,0,0,0},             /*95 '_'*/{0,0,0,0,0,0,0,0x7C},
/*96 '`'*/{0x20,0x10,0,0,0,0,0,0},                /*97 'a'*/{0,0,0x38,4,0x3C,0x44,0x3C,0},
/*98 'b'*/{0x40,0x40,0x78,0x44,0x44,0x44,0x78,0}, /*99 'c'*/{0,0,0x38,0x44,0x40,0x44,0x38,0},
/*100'd'*/{4,4,0x3C,0x44,0x44,0x44,0x3C,0},       /*101'e'*/{0,0,0x38,0x44,0x7C,0x40,0x38,0},
/*102'f'*/{0x18,0x24,0x20,0x70,0x20,0x20,0x20,0}, /*103'g'*/{0,0,0x3C,0x44,0x44,0x3C,4,0x38},
/*104'h'*/{0x40,0x40,0x58,0x64,0x44,0x44,0x44,0}, /*105'i'*/{0x10,0,0x30,0x10,0x10,0x10,0x38,0},
/*106'j'*/{8,0,0x18,8,8,0x48,0x30,0},             /*107'k'*/{0x40,0x40,0x48,0x50,0x60,0x50,0x48,0},
/*108'l'*/{0x30,0x10,0x10,0x10,0x10,0x10,0x38,0}, /*109'm'*/{0,0,0x68,0x54,0x54,0x44,0x44,0},
/*110'n'*/{0,0,0x58,0x64,0x44,0x44,0x44,0},       /*111'o'*/{0,0,0x38,0x44,0x44,0x44,0x38,0},
/*112'p'*/{0,0,0x78,0x44,0x44,0x78,0x40,0x40},    /*113'q'*/{0,0,0x3C,0x44,0x44,0x3C,4,4},
/*114'r'*/{0,0,0x58,0x64,0x40,0x40,0x40,0},       /*115's'*/{0,0,0x3C,0x40,0x38,4,0x78,0},
/*116't'*/{0x20,0x20,0x7C,0x20,0x20,0x24,0x18,0}, /*117'u'*/{0,0,0x44,0x44,0x44,0x4C,0x34,0},
/*118'v'*/{0,0,0x44,0x44,0x44,0x28,0x10,0},       /*119'w'*/{0,0,0x44,0x54,0x54,0x6C,0x44,0},
/*120'x'*/{0,0,0x44,0x28,0x10,0x28,0x44,0},       /*121'y'*/{0,0,0x44,0x44,0x44,0x3C,4,0x38},
/*122'z'*/{0,0,0x7C,8,0x10,0x20,0x7C,0},          /*123'{'*/{8,0x10,0x10,0x20,0x10,0x10,8,0},
/*124'|'*/{0x10,0x10,0x10,0,0x10,0x10,0x10,0},    /*125'}'*/{0x20,0x10,0x10,8,0x10,0x10,0x20,0},
/*126'~'*/{0,0,0x32,0x4C,0,0,0,0},                /*127 DEL*/{0x7C,0x44,0x44,0x44,0x44,0x44,0x7C,0},
};

#define HUD_MAX_VERTS 32768   /* 6 floats per vertex: x,y,r,g,b,a */
static float g_hud_vbuf[HUD_MAX_VERTS*6];  /* static — lives in BSS, not stack */
typedef struct{
    GLuint prog,vao,vbo;
    int win_w,win_h;
    int vcount;
}HUD;
static void hud_init(HUD *h,int w,int hp){
    h->win_w=w;h->win_h=hp;h->vcount=0;
    h->prog=GLH_program(
        "#version 330 core\nlayout(location=0)in vec2 aPos;\nlayout(location=1)in vec4 aColor;\nuniform mat4 uOrtho;\nout vec4 vColor;\nvoid main(){gl_Position=uOrtho*vec4(aPos,0,1);vColor=aColor;}\n",
        "#version 330 core\nin vec4 vColor;out vec4 F;\nvoid main(){F=vColor;}\n");
    GLH_Attrib a[]={{0,2,GL_FLOAT,GL_FALSE,0},{1,4,GL_FLOAT,GL_FALSE,8}};
    h->vbo=GLH_vbo(HUD_MAX_VERTS*6*sizeof(float));
    h->vao=GLH_mesh_dynamic(HUD_MAX_VERTS*6*sizeof(float),24,a,2);    glBindVertexArray(h->vao);glBindBuffer(GL_ARRAY_BUFFER,h->vbo);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,24,(void*)0);
    glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,24,(void*)8);
    glBindVertexArray(0);
}
static void hud_resize(HUD *h,int w,int hp){h->win_w=w;h->win_h=hp;}
static void hud_rect(HUD *h,float x,float y,float w,float ht,float r,float g,float b,float a){
    if(h->vcount+6>HUD_MAX_VERTS)return;
    float *v=g_hud_vbuf+h->vcount*6;
    float q[][6]={{x,y,r,g,b,a},{x+w,y,r,g,b,a},{x+w,y+ht,r,g,b,a},{x,y,r,g,b,a},{x+w,y+ht,r,g,b,a},{x,y+ht,r,g,b,a}};
    memcpy(v,q,sizeof(q));h->vcount+=6;
}
static void hud_flush(HUD *h){
    if(!h->vcount)return;
    float fw=(float)h->win_w,fh=(float)h->win_h;
    Mat4 o=mat4_identity();o.m[0]=2.0f/fw;o.m[5]=-2.0f/fh;o.m[12]=-1.0f;o.m[13]=1.0f;
    GLH_vbo_update(h->vbo,g_hud_vbuf,(size_t)h->vcount*6*sizeof(float));
    glUseProgram(h->prog);GLH_mat4(h->prog,"uOrtho",o.m);
    glBindVertexArray(h->vao);glBindBuffer(GL_ARRAY_BUFFER,h->vbo);
    glDisable(GL_DEPTH_TEST);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES,0,h->vcount);
    glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glBindVertexArray(0);
    h->vcount=0;
}
static void hud_bar(HUD *h,float x,float y,float w,float ht,float pct,float fr,float fg,float fb,float br,float bg,float bb){
    hud_rect(h,x,y,w,ht,br,bg,bb,0.75f);hud_rect(h,x,y,w*clampf(pct,0,1),ht,fr,fg,fb,0.9f);
}
static void hud_char(HUD *h,float x,float y,float sc,char c,float r,float g,float b,float a){
    int idx=(int)(unsigned char)c-32;if(idx<0||idx>=96)idx=31;/*'?'*/
    for(int row=0;row<8;row++){uint8_t bits=g_font[idx][row];for(int col=0;col<8;col++){if(bits&(0x80u>>col))hud_rect(h,x+col*sc,y+row*sc,sc,sc,r,g,b,a);}}
}
static void hud_str(HUD *h,float x,float y,float sc,const char *s,float r,float g,float b,float a){
    float cx=x;for(int i=0;s[i];i++){if(s[i]=='\n'){y+=9*sc;cx=x;continue;}hud_char(h,cx,y,sc,s[i],r,g,b,a);cx+=9*sc;}
}
/* Minimap: 160×160 top-down overview, bottom-right corner */
static void minimap_draw(HUD *h,Vec3 ppos,float pyaw,Enemy *enemies,int ne,QuestSystem *qs,Vec3 campfire_pos){
    const float MS=160.0f,MX=(float)h->win_w-MS-10.0f,MY=(float)h->win_h-MS-10.0f;
    const float WR=MS/3.0f; /* world units shown = 53 */
    hud_rect(h,MX-2,MY-2,MS+4,MS+4,0,0,0,0.85f);
    /* Terrain color grid (4px cells = 40×40 = 1600 quads) */
    int cells=40;float cell_px=MS/cells,cell_wu=WR*2.0f/cells;
    for(int gz=0;gz<cells;gz++)for(int gx=0;gx<cells;gx++){
        float wx=ppos.x+(gx-cells/2+0.5f)*cell_wu;
        float wz=ppos.z+(gz-cells/2+0.5f)*cell_wu;
        Vec3 tc=terrain_color(terrain_h(wx,wz));
        hud_rect(h,MX+gx*cell_px,MY+gz*cell_px,cell_px+0.5f,cell_px+0.5f,tc.x*0.7f,tc.y*0.7f,tc.z*0.7f,0.9f);
    }
    /* Helper macros: world→minimap pixel */
    #define W2MX(wx) (MX+((wx)-ppos.x+WR)/WR*0.5f*MS)
    #define W2MZ(wz) (MY+((wz)-ppos.z+WR)/WR*0.5f*MS)
    /* Quest reach markers (yellow diamond) */
    for(int i=0;i<qs->count;i++){Quest *q=&qs->quests[i];if(q->status!=Q_ACTIVE)continue;
        for(int j=0;j<q->obj_count;j++){Objective *o=&q->objs[j];if(o->type!=OBJ_REACH||o->done)continue;
            float ox=W2MX(o->loc.x),oz=W2MZ(o->loc.z);hud_rect(h,ox-5,oz-5,10,10,1,1,0,0.9f);}}
    /* Campfire (orange) */
    {float cx=W2MX(campfire_pos.x),cz=W2MZ(campfire_pos.z);hud_rect(h,cx-4,cz-4,8,8,1.0f,0.45f,0.1f,0.95f);}
    /* Enemies (red) */
    for(int i=0;i<ne;i++){Enemy *e=&enemies[i];if(!e->active||e->state==ES_DEAD)continue;
        float ex=W2MX(e->phys.pos.x),ez=W2MZ(e->phys.pos.z);
        if(ex>=MX&&ex<MX+MS&&ez>=MY&&ez<MY+MS)hud_rect(h,ex-4,ez-4,8,8,0.9f,0.1f,0.1f,0.95f);}
    /* Player dot + direction arrow */
    hud_rect(h,MX+MS/2-4,MY+MS/2-4,8,8,1,1,1,1);
    {float ax=MX+MS/2-sinf(pyaw)*10,az=MY+MS/2+cosf(pyaw)*10;hud_rect(h,ax-2,az-2,4,4,0.7f,0.7f,1,0.9f);}
    /* Border */
    hud_rect(h,MX,MY,MS,1,0.6f,0.6f,0.6f,0.8f);hud_rect(h,MX,MY+MS-1,MS,1,0.6f,0.6f,0.6f,0.8f);
    hud_rect(h,MX,MY,1,MS,0.6f,0.6f,0.6f,0.8f);hud_rect(h,MX+MS-1,MY,1,MS,0.6f,0.6f,0.6f,0.8f);
    /* "MAP" label */
    hud_str(h,MX+MS/2-13,MY-12,1.0f,"MAP",0.8f,0.8f,0.8f,0.8f);
    #undef W2MX
    #undef W2MZ
}
static void hud_draw(HUD *h,float hp,float mhp,float mp,float mmp,float sp,float msp,
                     AbilitySystem *as,QuestSystem *qs,Enemy *enemies,int ne,
                     Mat4 proj,Mat4 view,Player *player,Vec3 campfire_pos){
    float W=(float)h->win_w,H=(float)h->win_h;
    /* HP / MP / Stamina bars */
    float bx=20,by=H-90;
    hud_str(h,bx,by-10,1.0f,"HP",0.9f,0.3f,0.3f,0.9f);
    hud_bar(h,bx+22,by,   220,16,hp/mhp, 0.9f,0.15f,0.15f,0.10f,0.05f,0.05f);
    hud_str(h,bx,by+10,1.0f,"MP",0.3f,0.5f,0.9f,0.9f);
    hud_bar(h,bx+22,by+20,220,16,mp/mmp, 0.15f,0.4f,0.9f, 0.05f,0.10f,0.20f);
    hud_str(h,bx,by+30,1.0f,"SP",0.3f,0.9f,0.3f,0.9f);
    hud_bar(h,bx+22,by+40,220,16,sp/msp, 0.15f,0.8f,0.2f, 0.05f,0.20f,0.05f);
    /* Hotbar */
    float sw=48,sg=4,hbt=HOTBAR_SLOTS*(sw+sg)-sg,hbx=(W-hbt)*0.5f,hby=H-60;
    for(int i=0;i<HOTBAR_SLOTS;i++){
        Ability *ab=&as->slots[i];float sx=hbx+i*(sw+sg);
        float cdp=ab->ready?1.0f:(ab->cd>0?1.0f-ab->cd_timer/ab->cd:1.0f);
        hud_rect(h,sx,hby,sw,sw,0.08f,0.08f,0.12f,0.75f);
        hud_rect(h,sx,hby,sw,sw*cdp,0.3f,0.3f,0.6f,0.75f);
        char key[3];snprintf(key,3,"%d",i+1);hud_str(h,sx+2,hby+2,1.0f,key,0.8f,0.8f,0.8f,0.85f);
        if(!ab->ready){char rem[6];snprintf(rem,6,"%.0f",ab->cd_timer+0.5f);hud_str(h,sx+sw/2-4,hby+sw/2-4,1.0f,rem,1,0.6f,0,0.9f);}
        else hud_str(h,sx+2,hby+sw-10,1.0f,ab->name,0.9f,0.9f,0.9f,0.75f);
    }
    /* Quest log panel (top-right) – now shows titles + descriptions */
    float qx=W-270,qy=20,cy=qy;
    /* Measure height first */
    float panel_h=8;
    for(int i=0;i<qs->count;i++){Quest *q=&qs->quests[i];if(q->status==Q_INACTIVE)continue;
        panel_h+=14;for(int j=0;j<q->obj_count;j++)panel_h+=12;}
    hud_rect(h,qx-6,qy-4,274,panel_h,0,0,0,0.55f);
    /* Header */
    hud_str(h,qx,cy,1.5f,"QUESTS",0.8f,0.7f,0.3f,0.95f);cy+=14;
    for(int i=0;i<qs->count;i++){
        Quest *q=&qs->quests[i];if(q->status==Q_INACTIVE)continue;
        float nr=q->status==Q_DONE?0.3f:1.0f,ng=q->status==Q_DONE?0.9f:0.85f,nb=q->status==Q_DONE?0.3f:0.1f;
        hud_str(h,qx,cy,1.5f,q->name,nr,ng,nb,0.95f);cy+=14;
        for(int j=0;j<q->obj_count;j++){
            Objective *o=&q->objs[j];
            char buf[96];
            if(o->type==OBJ_KILL) snprintf(buf,sizeof(buf),"%s %d/%d",o->desc,o->current,o->required);
            else snprintf(buf,sizeof(buf),"%s%s",o->desc,o->done?" [done]":"");
            float ocr=o->done?0.35f:0.65f,ocg=o->done?0.9f:0.65f;
            hud_str(h,qx+4,cy,1.0f,buf,ocr,ocg,0.35f,0.85f);cy+=12;
        }
        cy+=2;
    }
    /* Enemy health bars (world-space → screen) */
    for(int i=0;i<ne;i++){
        Enemy *e=&enemies[i];if(!e->active||e->state==ES_DEAD)continue;
        Vec3 head={e->phys.pos.x,e->phys.pos.y+e->phys.height+0.25f,e->phys.pos.z};
        float sx,sy;if(!world_to_ndc(head,proj,view,&sx,&sy))continue;
        float px=(sx*0.5f+0.5f)*W,py=(1.0f-(sy*0.5f+0.5f))*H;
        hud_bar(h,px-30,py-10,60,6,e->health/e->max_health,0.9f,0.1f,0.1f,0.15f,0.05f,0.05f);
    }
    /* Crosshair */
    hud_rect(h,W/2-14,H/2-1,28,2,1,1,1,0.7f);hud_rect(h,W/2-1,H/2-14,2,28,1,1,1,0.7f);
    /* Minimap */
    minimap_draw(h,player->phys.pos,player->phys.yaw,enemies,ne,qs,campfire_pos);
    hud_flush(h);
}

static void debug_draw(
    HUD *hud, LineRenderer *lr,
    PerfCounter *perf,
    Player *player, Enemy *enemies, int ne,
    CollWorld *cw,
    Scene *char_scene,
    Camera3P *cam,
    AnimMap *amap,
    int win_w, int win_h,
    Mat4 proj, Mat4 view)
{
    float W=(float)win_w, H=(float)win_h;
    float sc=1.5f;  /* text scale */
    float lh=sc*10.0f; /* line height */
    float x=8.0f, y=8.0f;
    char buf[128];

    /* ── Semi-transparent background panel ── */
    hud_rect(hud,0,0,340,H,0,0,0,0.45f);

    /* ── FPS / frame time ── */
    snprintf(buf,sizeof(buf),"FPS: %.1f  (%.2f ms)",(double)perf->fps,(double)perf->frame_ms);
    hud_str(hud,x,y,sc,buf,1.0f,1.0f,0.2f,1.0f); y+=lh+2;

    /* ── Player ── */
    hud_str(hud,x,y,sc,"--- PLAYER ---",0.6f,0.8f,1.0f,1.0f); y+=lh+2;
    snprintf(buf,sizeof(buf),"Pos: %.2f  %.2f  %.2f",
             (double)player->phys.pos.x,(double)player->phys.pos.y,(double)player->phys.pos.z);
    hud_str(hud,x,y,sc,buf,0.9f,0.9f,0.9f,1.0f); y+=lh;
    snprintf(buf,sizeof(buf),"Vel: %.2f  %.2f  %.2f",
             (double)player->phys.vel.x,(double)player->phys.vel.y,(double)player->phys.vel.z);
    hud_str(hud,x,y,sc,buf,0.9f,0.9f,0.9f,1.0f); y+=lh;
    snprintf(buf,sizeof(buf),"Yaw: %.1f  OnGnd: %s",
             (double)(player->phys.yaw*180.0f/(float)M_PI), player->phys.on_ground?"YES":"NO");
    hud_str(hud,x,y,sc,buf,0.9f,0.9f,0.9f,1.0f); y+=lh;
    snprintf(buf,sizeof(buf),"State: %s  HP: %.0f/%.0f",
             player_state_name(player->state),(double)player->health,(double)player->max_health);
    hud_str(hud,x,y,sc,buf,0.4f,1.0f,0.5f,1.0f); y+=lh;

    /* ── Animation ── */
    hud_str(hud,x,y,sc,"--- ANIMATION ---",0.6f,0.8f,1.0f,1.0f); y+=lh+2;
    const char *anim_names[]={"idle","walk","run","attack","attack2","jump","death","roll","block","hit"};
    int anim_indices[]={amap->idle,amap->walk,amap->run,amap->attack,amap->attack2,amap->jump,amap->death,amap->roll,amap->block,amap->hit};
    int num_mapped=10;
    const char *cur_anim_name="?";
    for(int i=0;i<num_mapped;i++)if(anim_indices[i]==player->anim_idx){cur_anim_name=anim_names[i];break;}
    float anim_dur=0;
    if(char_scene->scene&&player->anim_idx>=0&&player->anim_idx<(int)char_scene->scene->anim_stacks.count){
        ufbx_anim_stack *stk=char_scene->scene->anim_stacks.data[player->anim_idx];
        anim_dur=(float)(anim_stack_real_end(stk)-stk->time_begin);
    }
    snprintf(buf,sizeof(buf),"Clip: [%d] %s",player->anim_idx,cur_anim_name);
    hud_str(hud,x,y,sc,buf,1.0f,0.8f,0.4f,1.0f); y+=lh;
    snprintf(buf,sizeof(buf),"Time: %.3f / %.3f",(double)player->anim_time,(double)anim_dur);
    hud_str(hud,x,y,sc,buf,1.0f,0.8f,0.4f,1.0f); y+=lh;
    /* Mini progress bar for anim playback */
    if(anim_dur>0){
        hud_rect(hud,x,y,200,6,0.2f,0.2f,0.2f,0.8f);
        hud_rect(hud,x,y,200.0f*clampf((float)(player->anim_time/anim_dur),0,1),6,1.0f,0.6f,0.1f,0.9f);
        y+=10;
    }
    snprintf(buf,sizeof(buf),"Stacks in FBX: %d",(int)(char_scene->scene?char_scene->scene->anim_stacks.count:0));
    hud_str(hud,x,y,sc,buf,0.7f,0.7f,0.7f,1.0f); y+=lh;

    /* ── Camera ── */
    hud_str(hud,x,y,sc,"--- CAMERA ---",0.6f,0.8f,1.0f,1.0f); y+=lh+2;
    snprintf(buf,sizeof(buf),"Pos: %.1f %.1f %.1f",(double)cam->pos.x,(double)cam->pos.y,(double)cam->pos.z);
    hud_str(hud,x,y,sc,buf,0.9f,0.9f,0.9f,1.0f); y+=lh;
    snprintf(buf,sizeof(buf),"Yaw:%.1f  Pitch:%.1f  Dist:%.1f",
             (double)(cam->yaw*180.0f/(float)M_PI),(double)(cam->pitch*180.0f/(float)M_PI),(double)cam->dist);
    hud_str(hud,x,y,sc,buf,0.9f,0.9f,0.9f,1.0f); y+=lh;

    /* ── Collision ── */
    hud_str(hud,x,y,sc,"--- COLLISION ---",0.6f,0.8f,1.0f,1.0f); y+=lh+2;
    snprintf(buf,sizeof(buf),"AABB boxes: %d",cw?cw->count:0);
    hud_str(hud,x,y,sc,buf,0.9f,0.9f,0.9f,1.0f); y+=lh;
    snprintf(buf,sizeof(buf),"Player r=%.2f h=%.2f",
             (double)player->phys.radius,(double)player->phys.height);
    hud_str(hud,x,y,sc,buf,0.9f,0.9f,0.9f,1.0f); y+=lh;

    /* ── Enemies ── */
    hud_str(hud,x,y,sc,"--- ENEMIES ---",0.6f,0.8f,1.0f,1.0f); y+=lh+2;
    int alive=0;
    for(int i=0;i<ne;i++){
        Enemy *e=&enemies[i];if(!e->active)continue;
        float dist=vec3_dist(e->phys.pos,player->phys.pos);
        snprintf(buf,sizeof(buf),"[%d] %s  HP:%.0f  d:%.1f",
                 i,enemy_state_name(e->state),(double)e->health,(double)dist);
        float ec=e->state==ES_DEAD?0.4f:1.0f;
        hud_str(hud,x,y,sc,buf,ec,e->state==ES_DEAD?0.4f:0.3f,0.3f,1.0f); y+=lh;
        if(e->state!=ES_DEAD)alive++;
    }
    snprintf(buf,sizeof(buf),"Active: %d / %d",alive,ne);
    hud_str(hud,x,y,sc,buf,0.7f,0.7f,0.7f,1.0f); y+=lh;

    /* ── 3D wireframes ── */
    /* Player capsule – cyan */
    lr_capsule(player->phys.pos,player->phys.height,player->phys.radius,0,1,1);
    lr_axis(player->phys.pos,0.5f);
    /* Attack capsule when attacking – yellow */
    if(player->state==PS_ATTACK||player->state==PS_ATTACK2){
        Vec3 ad={-sinf(player->phys.yaw),0,cosf(player->phys.yaw)};
        Vec3 atk0=vec3_add(player->phys.pos,(Vec3){0,0.5f,0});
        Vec3 atk1=vec3_add(atk0,vec3_scale(ad,ATTACK_RANGE));
        lr_push(atk0,atk1,1,1,0);
        lr_capsule(atk0,0.1f,0.55f,1,1,0);
        lr_capsule(atk1,0.1f,0.55f,1,0.5f,0);
    }
    /* Enemy capsules – red / grey for dead */
    for(int i=0;i<ne;i++){
        Enemy *e=&enemies[i];if(!e->active)continue;
        float r=e->state==ES_DEAD?0.4f:0.9f,g=0.2f,b=0.2f;
        lr_capsule(e->phys.pos,e->phys.height,e->phys.radius,r,g,b);
        lr_axis(e->phys.pos,0.3f);
    }
    /* Collision AABBs – green (only nearest 32 to avoid spam) */
    if(cw){for(int i=0;i<cw->count&&i<32;i++){
        float dist=vec3_dist(player->phys.pos,(Vec3){
            (cw->boxes[i].min.x+cw->boxes[i].max.x)*0.5f,
            player->phys.pos.y,
            (cw->boxes[i].min.z+cw->boxes[i].max.z)*0.5f});
        float t=clampf(1.0f-dist/20.0f,0,1);
        lr_box(&cw->boxes[i],0.1f+0.4f*t,0.8f,0.1f);
    }}

    lr_flush(lr,proj,view);
    hud_flush(hud);
}


/* ══════════════════════════════════════════════════════════════
   MAIN
   ══════════════════════════════════════════════════════════════ */
int main(int argc,char **argv){
    if(argc<2){printf("Usage: %s <character.glb> [village.glb]\n",argv[0]);return 1;}
    const char *char_path=argv[1],*village_path=argc>=3?argv[2]:NULL;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,1);SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,4);
    SDL_Window *win=SDL_CreateWindow("RPG Engine",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,WIN_W,WIN_H,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    SDL_GLContext ctx=SDL_GL_CreateContext(win);
    SDL_GL_SetSwapInterval(1);glEnable(GL_DEPTH_TEST);glEnable(GL_MULTISAMPLE);srand((unsigned)time(NULL));

    Scene char_scene={0};if(!scene_load(&char_scene,char_path)){SDL_Quit();return 1;}
    AnimMap player_amap,enemy_amap;animmap_init(&player_amap,char_scene.scene);enemy_amap=player_amap;

    Village village;memset(&village,0,sizeof(village));bool has_village=false;
    if(village_path)has_village=village_load(&village,village_path);

    SkinProg   skin;     skinprog_init(&skin);
    Skybox     skybox;   skybox_init(&skybox,"skybox.png");
    HUD        hud;      hud_init(&hud,WIN_W,WIN_H);
    ParticleSystem ps;   ps_init(&ps);
    LightingSystem ls;   lighting_init(&ls);
    QuestSystem    qs;   quest_init(&qs);
    AbilitySystem  as;   ability_init(&as);
    Terrain        terrain; terrain_build(&terrain);
    ShadowMap      sm;   shadow_init(&sm,&ls);
    LineRenderer   lr;   lr_init(&lr);
    PerfCounter    perf; memset(&perf,0,sizeof(perf));

    Vec3 cf_pos={0.0f,terrain_h(0,0)+0.1f,0.0f};
    Campfire campfire=campfire_create(&ps,&ls,cf_pos);(void)campfire;

    /* Lava pool ambient lights – positions chosen to sit inside lava noise zones */
    {Vec3 lc={1.0f,0.25f,0.0f};
     lighting_add_point(&ls,(Vec3){ 70,terrain_h( 70,-65)+0.3f,-65},lc,18.0f,2.5f);
     lighting_add_point(&ls,(Vec3){-80,terrain_h(-80, 70)+0.3f, 70},lc,18.0f,2.5f);
     lighting_add_point(&ls,(Vec3){ 60,terrain_h( 60, 80)+0.3f, 80},lc,14.0f,2.0f);}
    Quest *q1=quest_add(&qs,"Dark Knight","Defeat the enemy warriors.");quest_kill_obj(q1,"Slay 3 enemies",3);q1->gold=100;q1->xp=50;quest_start(&qs,q1->id);
    Quest *q2=quest_add(&qs,"Find the Campfire","Locate the camp.");quest_reach_obj(q2,"Reach campfire",cf_pos,3.0f);q2->xp=75;quest_start(&qs,q2->id);

    /* Compute real durations for attack animations so the timer matches the clip */
    float attack_dur=0.8f,attack2_dur=0.8f,hit_dur=0.5f,death_dur=2.0f;
    {ufbx_scene *sc=char_scene.scene;
     if(player_amap.attack>=0&&player_amap.attack<(int)sc->anim_stacks.count){ufbx_anim_stack *stk=sc->anim_stacks.data[player_amap.attack];attack_dur=(float)(anim_stack_real_end(stk)-stk->time_begin);}
     if(player_amap.attack2>=0&&player_amap.attack2<(int)sc->anim_stacks.count){ufbx_anim_stack *stk=sc->anim_stacks.data[player_amap.attack2];attack2_dur=(float)(anim_stack_real_end(stk)-stk->time_begin);}
     if(player_amap.hit>=0&&player_amap.hit<(int)sc->anim_stacks.count){ufbx_anim_stack *stk=sc->anim_stacks.data[player_amap.hit];hit_dur=(float)(anim_stack_real_end(stk)-stk->time_begin);}
     if(player_amap.death>=0&&player_amap.death<(int)sc->anim_stacks.count){ufbx_anim_stack *stk=sc->anim_stacks.data[player_amap.death];death_dur=(float)(anim_stack_real_end(stk)-stk->time_begin);}
     printf("[Anim] attack=%.3fs  attack2=%.3fs  hit=%.3fs  death=%.3fs\n",(double)attack_dur,(double)attack2_dur,(double)hit_dur,(double)death_dur);}

    Player player={0};player.phys.pos=(Vec3){5,0,5};player.phys.height=1.8f;player.phys.radius=0.4f;
    player.health=player.max_health=100.0f;player.anim_idx=player_amap.idle;player.prev_anim_idx=player_amap.idle;player.anim_speed=1.0f;
    player.phys.pos.y=terrain_h(5,5);

    Enemy enemies[MAX_ENEMIES];memset(enemies,0,sizeof(enemies));int ne=3;
    Vec3 esp[]={{8,0,4},{-6,0,9},{12,0,-5}};
    for(int i=0;i<ne;i++){enemies[i].phys.pos=esp[i];enemies[i].phys.pos.y=terrain_h(esp[i].x,esp[i].z);enemies[i].phys.height=1.8f;enemies[i].phys.radius=0.4f;enemies[i].health=enemies[i].max_health=80.0f;enemies[i].anim_idx=enemy_amap.idle;enemies[i].anim_speed=1.0f;enemies[i].attack_cooldown=1.5f;enemies[i].active=true;}

    Camera3P cam={0};cam.yaw=0.3f;cam.pitch=0.4f;cam.dist=6.0f;cam.target=player.phys.pos;
    bool cam_drag=false;int cam_lx=0,cam_ly=0;
    bool show_debug=false;
    float total_time=0;int win_w=WIN_W,win_h=WIN_H;
    float time_scale=1.0f;
    Uint64 last=SDL_GetPerformanceCounter();bool running=true;

    while(running){
        Uint64 now=SDL_GetPerformanceCounter();
        float dt=clampf((float)(now-last)/(float)SDL_GetPerformanceFrequency(),0,0.05f);
        last=now;
        /* Hold T to fast-forward the day/night cycle (20×) */
        {const uint8_t *k=SDL_GetKeyboardState(NULL);time_scale=k[SDL_SCANCODE_T]?20.0f:1.0f;}
        total_time+=dt*time_scale;

        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            switch(ev.type){
            case SDL_QUIT:running=false;break;
            case SDL_KEYDOWN:
                switch(ev.key.keysym.sym){
                case SDLK_ESCAPE:running=false;break;
                case SDLK_F3:show_debug=!show_debug;break;
                /* Respawn */
                case SDLK_RETURN:case SDLK_KP_ENTER:
                    if(player.state==PS_DEAD){
                        player.health=player.max_health;
                        player.state=PS_IDLE;player.anim_idx=player_amap.idle;
                        player.anim_time=0;player.hit_flash=0;
                        player.phys.pos=(Vec3){5,terrain_h(5,5),5};
                        player.phys.vel=(Vec3){0,0,0};
                    }break;
                case SDLK_1:ability_use(&as,0);break;case SDLK_2:ability_use(&as,1);break;
                case SDLK_3:ability_use(&as,2);break;case SDLK_4:ability_use(&as,3);break;case SDLK_5:ability_use(&as,4);break;
                case SDLK_f:if(player.state!=PS_DEAD&&player.state!=PS_ATTACK&&player.state!=PS_ATTACK2){player.state=PS_ATTACK;player.anim_idx=player_amap.attack;player.attack_timer=0;player.attack_hit=false;player.anim_time=0;}break;
                case SDLK_g:if(player.state!=PS_DEAD&&player.state!=PS_ATTACK&&player.state!=PS_ATTACK2){player.state=PS_ATTACK2;player.anim_idx=player_amap.attack2;player.attack_timer=0;player.attack_hit=false;player.anim_time=0;}break;
                case SDLK_q:if(player.state!=PS_DEAD&&player.state!=PS_ROLL&&player.phys.on_ground){player.state=PS_ROLL;player.anim_idx=player_amap.roll;player.roll_timer=0;player.anim_time=0;}break;
                }break;
            case SDL_MOUSEBUTTONDOWN:
                if(ev.button.button==SDL_BUTTON_RIGHT){cam_drag=true;cam_lx=ev.button.x;cam_ly=ev.button.y;}
                if(ev.button.button==SDL_BUTTON_LEFT&&player.state!=PS_DEAD&&player.state!=PS_ATTACK&&player.state!=PS_ATTACK2){player.state=PS_ATTACK;player.anim_idx=player_amap.attack;player.attack_timer=0;player.attack_hit=false;player.anim_time=0;}
                if(ev.button.button==SDL_BUTTON_MIDDLE&&player.state!=PS_DEAD&&player.state!=PS_ATTACK&&player.state!=PS_ATTACK2){player.state=PS_ATTACK2;player.anim_idx=player_amap.attack2;player.attack_timer=0;player.attack_hit=false;player.anim_time=0;}
                break;
            case SDL_MOUSEBUTTONUP:if(ev.button.button==SDL_BUTTON_RIGHT)cam_drag=false;break;
            case SDL_MOUSEMOTION:if(cam_drag){cam.yaw-=(ev.motion.x-cam_lx)*0.005f;cam.pitch-=(ev.motion.y-cam_ly)*0.005f;cam.pitch=clampf(cam.pitch,0.05f,1.3f);cam_lx=ev.motion.x;cam_ly=ev.motion.y;}break;
            case SDL_MOUSEWHEEL:cam.dist-=ev.wheel.y*0.5f;cam.dist=clampf(cam.dist,2.0f,25.0f);break;
            case SDL_WINDOWEVENT:if(ev.window.event==SDL_WINDOWEVENT_RESIZED){win_w=ev.window.data1;win_h=ev.window.data2;glViewport(0,0,win_w,win_h);hud_resize(&hud,win_w,win_h);}break;
            }
        }

        /* Player movement */
        if(player.state!=PS_DEAD){
            const uint8_t *keys=SDL_GetKeyboardState(NULL);
            Vec3 fwd=cam_forward(&cam),rgt=cam_right(&cam),move={0,0,0};
            if(keys[SDL_SCANCODE_W])move=vec3_add(move,fwd);
            if(keys[SDL_SCANCODE_S])move=vec3_sub(move,fwd);
            if(keys[SDL_SCANCODE_D])move=vec3_add(move,rgt);
            if(keys[SDL_SCANCODE_A])move=vec3_sub(move,rgt);
            bool moving=vec3_len(move)>0.01f;
            if(moving){move=vec3_scale(vec3_norm(move),PLAYER_SPEED);player.phys.yaw=atan2f(move.x,-move.z)+(float)M_PI;}
            player.phys.vel.x=moving?move.x:player.phys.vel.x*0.8f;
            player.phys.vel.z=moving?move.z:player.phys.vel.z*0.8f;
            if(keys[SDL_SCANCODE_SPACE]&&player.phys.on_ground&&player.state!=PS_ATTACK&&player.state!=PS_ATTACK2&&player.state!=PS_ROLL){player.phys.vel.y=PLAYER_JUMP;player.state=PS_JUMP;player.anim_idx=player_amap.jump;player.anim_time=0;}
            bool sprinting=keys[SDL_SCANCODE_LSHIFT]&&moving;
            if(sprinting){move=vec3_scale(vec3_norm(move),PLAYER_SPEED*1.7f);player.phys.vel.x=move.x;player.phys.vel.z=move.z;}
            player.blocking=keys[SDL_SCANCODE_R]&&player.state!=PS_DEAD&&player.state!=PS_ATTACK&&player.state!=PS_ATTACK2;
            if(player.state==PS_ATTACK||player.state==PS_ATTACK2){
                player.attack_timer+=dt;
                float cur_atk_dur=(player.state==PS_ATTACK)?attack_dur:attack2_dur;
                /* Hit window = 20%–50% of the clip duration */
                float hit_start=cur_atk_dur*0.20f;
                float hit_end  =cur_atk_dur*0.50f;
                if(!player.attack_hit&&player.attack_timer>hit_start&&player.attack_timer<hit_end){
                    Vec3 ad={-sinf(player.phys.yaw),0,cosf(player.phys.yaw)};
                    Vec3 atk0=vec3_add(player.phys.pos,(Vec3){0,0.5f,0});
                    Vec3 atk1=vec3_add(atk0,vec3_scale(ad,ATTACK_RANGE));
                    for(int i=0;i<ne;i++){if(!enemies[i].active||enemies[i].state==ES_DEAD)continue;
                        Vec3 eb=enemies[i].phys.pos;
                        Vec3 et=vec3_add(eb,(Vec3){0,enemies[i].phys.height,0});
                        if(capsule_capsule(atk0,atk1,0.55f,eb,et,enemies[i].phys.radius)){
                            enemies[i].health-=ATTACK_DAMAGE;enemies[i].state=ES_STAGGER;enemies[i].stagger_timer=0.3f;
                            ps_burst(&ps,enemies[i].phys.pos,1.0f,0.3f,0.1f);
                            if(enemies[i].health<=0){enemies[i].health=0;enemies[i].state=ES_DEAD;enemies[i].anim_idx=enemy_amap.death;enemies[i].anim_time=0;quest_notify_kill(&qs);ps_burst(&ps,enemies[i].phys.pos,0.8f,0.1f,0.1f);}
                            player.attack_hit=true;break;
                        }
                    }
                }
                if(player.attack_timer>=cur_atk_dur){player.anim_idx=player_amap.idle;player.state=PS_IDLE;}
            }else if(player.state==PS_ROLL){
                player.roll_timer+=dt;
                Vec3 rd={-sinf(player.phys.yaw),0,cosf(player.phys.yaw)};player.phys.vel.x=rd.x*PLAYER_SPEED*1.5f;player.phys.vel.z=rd.z*PLAYER_SPEED*1.5f;
                if(player.roll_timer>0.6f){player.anim_idx=player_amap.idle;player.state=PS_IDLE;}
            }else if(player.state==PS_HIT){
                player.hit_timer-=dt;
                /* lock velocity during stagger */
                player.phys.vel.x*=0.85f;player.phys.vel.z*=0.85f;
                if(player.hit_timer<=0.0f){player.state=PS_IDLE;player.anim_idx=player_amap.idle;}
            }else if(player.state==PS_JUMP){if(player.phys.on_ground){player.anim_idx=player_amap.idle;player.state=PS_IDLE;}}
            else if(player.blocking){player.anim_idx=player_amap.block;player.state=PS_BLOCK;}
            else{
                if(moving){player.state=sprinting?PS_RUN:PS_WALK;player.anim_idx=sprinting?player_amap.run:player_amap.walk;}
                else{player.state=PS_IDLE;player.anim_idx=player_amap.idle;}
            }
            /* KEY FIX: reset anim_time whenever the active animation changes */
            if(player.anim_idx!=player.prev_anim_idx){player.anim_time=0.0f;}
            player.prev_anim_idx=player.anim_idx;
        }
        /* Clamp death animation – play once then freeze on last frame */
        if(player.state==PS_DEAD&&player.anim_time>(double)death_dur)
            player.anim_time=(double)death_dur;

        player.hit_flash=clampf(player.hit_flash-dt*3.0f,0,1);
        float pg=terrain_h(player.phys.pos.x,player.phys.pos.z);
        phys_update(&player.phys,dt,pg);
        if(has_village)coll_resolve(&village.coll,&player.phys.pos,player.phys.radius);
        player.anim_time += dt * player.anim_speed;

        /* Enemy AI */
        for(int i=0;i<ne;i++){Enemy *e=&enemies[i];if(!e->active)continue;
            if(e->state==ES_DEAD){e->anim_time+=dt;continue;}
            if(e->state==ES_STAGGER){e->stagger_timer-=dt;if(e->stagger_timer<=0)e->state=ES_CHASE;e->anim_time+=dt;continue;}
            e->attack_timer=clampf(e->attack_timer-dt,0,999);
            float dist=vec3_dist(e->phys.pos,player.phys.pos);
            if(dist>ENEMY_CHASE_R){e->state=ES_IDLE;e->anim_idx=enemy_amap.idle;e->phys.vel.x=e->phys.vel.z=0;}
            else if(dist>ENEMY_ATTACK_R){e->state=ES_CHASE;e->anim_idx=enemy_amap.walk;Vec3 dir=vec3_norm(vec3_sub(player.phys.pos,e->phys.pos));e->phys.vel.x=dir.x*ENEMY_SPEED;e->phys.vel.z=dir.z*ENEMY_SPEED;e->phys.yaw=atan2f(dir.x,-dir.z)+(float)M_PI;}
            else{e->phys.vel.x=e->phys.vel.z=0;e->state=ES_ATTACK;e->anim_idx=enemy_amap.attack;
                if(e->attack_timer<=0&&player.state!=PS_DEAD&&player.state!=PS_HIT){player.health-=ENEMY_DAMAGE;player.hit_flash=1.0f;e->attack_timer=e->attack_cooldown;ps_burst(&ps,player.phys.pos,1,0,0);
                    if(player.health<=0){player.health=0;player.state=PS_DEAD;player.anim_idx=player_amap.death;player.anim_time=0;}
                    else{player.state=PS_HIT;player.anim_idx=player_amap.hit;player.anim_time=0;player.hit_timer=hit_dur;}}}
            float eg=terrain_h(e->phys.pos.x,e->phys.pos.z);phys_update(&e->phys,dt,eg);
            e->anim_time += dt * e->anim_speed;
        }

        ability_update(&as,dt);quest_update(&qs,player.phys.pos);lighting_update(&ls,total_time);ps_update(&ps,dt);cam_update(&cam,player.phys.pos,dt);
        perf_update(&perf,dt);

        /* Render */
        float asp=(float)win_w/(float)win_h;
        Mat4 proj=mat4_persp(60.0f,asp,0.1f,500.0f);
        Mat4 view=mat4_lookat(cam.pos,cam.target,(Vec3){0,1,0});

        /* ── Shadow pass ──────────────────────────────────────── */
        shadow_begin(&sm,&ls,player.phys.pos);
        shadow_terrain(&sm,&ls,&terrain);
        if(has_village)shadow_village(&sm,&ls,&village);
        {Mat4 wm=entity_mat(player.phys.pos,player.phys.yaw,char_scene.ground_offset,char_scene.model_scale,char_scene.cx,char_scene.cy,char_scene.cz);shadow_scene(&sm,&ls,&char_scene,wm,player.anim_idx,player.anim_time);}
        for(int i=0;i<ne;i++){Enemy *e=&enemies[i];if(!e->active)continue;
            Mat4 wm=entity_mat(e->phys.pos,e->phys.yaw,char_scene.ground_offset,char_scene.model_scale,char_scene.cx,char_scene.cy,char_scene.cz);
            shadow_scene(&sm,&ls,&char_scene,wm,e->anim_idx,e->anim_time);}
        shadow_end(&sm,win_w,win_h);

        /* ── Main render pass ─────────────────────────────────── */
        /* Clear colour tracks the sky horizon so no seams at the edges */
        {float day_t=clampf(ls.sun_elev*3.0f+0.3f,0,1);
         float dawn_t=clampf(1.0f-fabsf(ls.sun_elev)*5.0f,0,1);
         float r=0.03f+(0.52f*day_t+0.92f*dawn_t*0.4f)*(day_t+dawn_t*0.4f);
         float g=0.03f+(0.72f*day_t+0.41f*dawn_t*0.4f)*(day_t+dawn_t*0.4f);
         float b=0.10f+(0.85f*day_t)*(day_t);
         glClearColor(r,g,b,1.0f);}
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        skybox_draw(&skybox,proj,view,&ls);
        terrain_draw(&terrain,&ls,proj,view,cam.pos,total_time);
        if(has_village)village_draw(&village,&ls,proj,view);

        /* Player */
        {Mat4 wm=entity_mat(player.phys.pos,player.phys.yaw,char_scene.ground_offset,char_scene.model_scale,char_scene.cx,char_scene.cy,char_scene.cz);draw_scene(&skin,&char_scene,wm,player.anim_idx,player.anim_time,player.hit_flash,proj,view,&ls);}

        /* Enemies (darker) */
        for(int i=0;i<ne;i++){Enemy *e=&enemies[i];if(!e->active)continue;
            GLH_vec3(skin.prog,"uBaseColor",0.35f,0.35f,0.42f);
            Mat4 wm=entity_mat(e->phys.pos,e->phys.yaw,char_scene.ground_offset,char_scene.model_scale,char_scene.cx,char_scene.cy,char_scene.cz);
            draw_scene(&skin,&char_scene,wm,e->anim_idx,e->anim_time,e->state==ES_STAGGER?0.4f:0.0f,proj,view,&ls);}

        ps_draw(&ps,proj,view);
        hud_draw(&hud,player.health,player.max_health,as.mana,as.max_mana,as.stam,as.max_stam,&as,&qs,enemies,ne,proj,view,&player,cf_pos);
        if(show_debug)debug_draw(&hud,&lr,&perf,&player,enemies,ne,has_village?&village.coll:NULL,&char_scene,&cam,&player_amap,win_w,win_h,proj,view);

        /* ── Death screen ─────────────────────────────────────── */
        if(player.state==PS_DEAD){
            float cx=(float)win_w*0.5f,cy=(float)win_h*0.38f;
            hud_rect(&hud,0,0,(float)win_w,(float)win_h,0,0,0,0.5f);
            hud_str(&hud,cx-72,cy,    3.5f,"YOU DIED",0.85f,0.05f,0.05f,1.0f);
            hud_str(&hud,cx-108,cy+52,1.6f,"ENTER to Respawn",0.90f,0.90f,0.90f,0.85f);
            hud_flush(&hud);
        }

        char title[256];snprintf(title,sizeof(title),"RPG | HP:%.0f | MP:%.0f | Gold:%d XP:%d",(double)player.health,(double)as.mana,qs.player_gold,qs.player_xp);
        SDL_SetWindowTitle(win,title);SDL_GL_SwapWindow(win);
    }

    scene_free(&char_scene);if(has_village)scene_free(&village.scene);
    SDL_GL_DeleteContext(ctx);SDL_DestroyWindow(win);SDL_Quit();return 0;
}

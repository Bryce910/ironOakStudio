/*
 * RPG Engine - ufbx + SDL2 + OpenGL 3.3
 * Build: gcc -std=c99 -O2 -I. main.c ufbx.c -o rpg -lSDL2 -lSDL2_mixer -lGL -lm
 * Usage: ./rpg character.fbx [village.fbx]
 * Controls: WASD Move | SPACE Jump | F/LClick Attack | RightDrag Camera | Scroll Zoom | 1-5 Abilities | E Interact | H Potion | I Inventory | P Weather | T FastTime
 * Audio:  Drop .wav files next to the exe (see AudioSystem for filenames). All optional.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>  /* mkdir */
#if defined(_WIN32)
#  include <process.h>  /* _execv */
#else
#  include <unistd.h>   /* execvp */
#endif
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#include "ufbx.h"
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#include "stb_image.h"
#include "gl_helpers.h"

/* Upload pre-decoded albedo pixels as sRGB so models shade correctly */
static GLuint GLH_texture_rgba_srgb(void *px,int w,int h){
    if(!px||w<=0||h<=0)return 0;
    GLuint id=0;glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);
    glTexImage2D(GL_TEXTURE_2D,0,GL_SRGB8_ALPHA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,px);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    return id;
}

/* ── Constants ─────────────────────────────────────────────── */
#define MAX_BONES        128
#define MAX_WEIGHTS      4
#define MAX_MESHES       64
#define MAX_GLTF_TEX_CACHE 256
#define WIN_W            1280
#define WIN_H            720
#define MAX_ENEMIES      128
#define MAX_QUESTS       8
#define MAX_OBJECTIVES   4
#define HOTBAR_SLOTS     5
#define MAX_PARTICLES    2048
#define MAX_EMITTERS     48
#define MAX_POINT_LIGHTS 32
#define MAX_COLL_BOXES   256
#define GRAVITY         -18.0f
#define PLAYER_SPEED     9.0f          /* fast enough to traverse 5km world */
#define PLAYER_JUMP      9.0f
#define ATTACK_RANGE     2.2f
#define ATTACK_DAMAGE   25.0f
#define ENEMY_SPEED      2.8f
#define ENEMY_CHASE_R   14.0f
#define ENEMY_ATTACK_R   1.8f
#define ENEMY_DAMAGE    12.0f
#define TERRAIN_COLS    512            /* 512×512 grid */
#define TERRAIN_ROWS    512
#define TERRAIN_CELL    9.765625f      /* 5000/512 → ~5km×5km world */
#define WORLD_HALF      2500.0f
/* ── Flow-field navmesh ── */
#define NAV_W           256
#define NAV_H           256
#define NAV_CELL        (5000.0f / (float)NAV_W)
#define NAV_BLOCKED     0xFF
/* ── Post-processing ── */
#define BLOOM_THRESH    1.0f
#define BLOOM_STRENGTH  0.45f
/* ── Options / config (declared early — used by audio + postfx) ── */
typedef enum{DIFF_EASY=0,DIFF_NORMAL,DIFF_HARD}Difficulty;
typedef struct{
    int  master_vol;  /* 0-100 */
    int  music_vol;   /* 0-100 */
    int  sfx_vol;     /* 0-100 */
    bool fullscreen;
    bool show_fps;
    bool vsync;
    bool bloom;
    int  difficulty;
    int  res_w,res_h;
}Options;
static Options g_options={100,70,80,false,false,true,true,DIFF_NORMAL,1280,720};
static void options_save(const Options *o){
    FILE *f=fopen("config.ini","w");if(!f)return;
    fprintf(f,"master_vol=%d\n",o->master_vol);
    fprintf(f,"music_vol=%d\n",o->music_vol);
    fprintf(f,"sfx_vol=%d\n",o->sfx_vol);
    fprintf(f,"fullscreen=%d\n",(int)o->fullscreen);
    fprintf(f,"show_fps=%d\n",(int)o->show_fps);
    fprintf(f,"vsync=%d\n",(int)o->vsync);
    fprintf(f,"bloom=%d\n",(int)o->bloom);
    fprintf(f,"difficulty=%d\n",o->difficulty);
    fprintf(f,"res_w=%d\n",o->res_w);
    fprintf(f,"res_h=%d\n",o->res_h);
    fclose(f);printf("[Config] Saved config.ini\n");
}
static void options_load(Options *o){
    FILE *f=fopen("config.ini","r");if(!f)return;
    char line[128];
    while(fgets(line,sizeof(line),f)){
        int v;
        if(sscanf(line,"master_vol=%d",&v)==1) o->master_vol=v;
        if(sscanf(line,"music_vol=%d",&v)==1)  o->music_vol=v;
        if(sscanf(line,"sfx_vol=%d",&v)==1)    o->sfx_vol=v;
        if(sscanf(line,"fullscreen=%d",&v)==1) o->fullscreen=(bool)v;
        if(sscanf(line,"show_fps=%d",&v)==1)   o->show_fps=(bool)v;
        if(sscanf(line,"vsync=%d",&v)==1)      o->vsync=(bool)v;
        if(sscanf(line,"bloom=%d",&v)==1)      o->bloom=(bool)v;
        if(sscanf(line,"difficulty=%d",&v)==1) o->difficulty=v;
        if(sscanf(line,"res_w=%d",&v)==1)      o->res_w=v;
        if(sscanf(line,"res_h=%d",&v)==1)      o->res_h=v;
    }
    fclose(f);printf("[Config] Loaded config.ini\n");
}
#define WATER_LEVEL     0.0f
#define SHADOW_SIZE     2048           /* larger shadow map for big world */
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
static Mat4 mat4_rot_x(float r){Mat4 m=mat4_identity();float c=cosf(r),s=sinf(r);m.m[5]=c;m.m[6]=-s;m.m[9]=s;m.m[10]=c;return m;}
static Mat4 mat4_rot_z(float r){Mat4 m=mat4_identity();float c=cosf(r),s=sinf(r);m.m[0]=c;m.m[1]=s;m.m[4]=-s;m.m[5]=c;return m;}
static Mat4 mat4_scale_xyz(float x,float y,float z){Mat4 m=mat4_identity();m.m[0]=x;m.m[5]=y;m.m[10]=z;return m;}
static Mat4 mat4_persp(float fov,float asp,float zn,float zf){float f=1.0f/tanf(fov*0.5f*(float)M_PI/180.0f);Mat4 m={0};m.m[0]=f/asp;m.m[5]=f;m.m[10]=(zf+zn)/(zn-zf);m.m[11]=-1.0f;m.m[14]=(2.0f*zf*zn)/(zn-zf);return m;}
/* mat4_inv_persp removed — SSAO uses analytical linear depth instead */
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
    if(cw<=0.001f) return false;
    *sx=cx/cw; *sy=cy/cw; return true;
}


/* ── Lighting ───────────────────────────────────────────────── */
#define LIGHTING_UNIFORMS \
    "uniform vec3 uSunDir,uSunColor,uAmbient;\n" \
    "uniform float uSunElev;\n" \
    "uniform int uNumLights;\n" \
    "uniform vec3 uLightPos[32],uLightColor[32];\n" \
    "uniform float uLightRadius[32];\n" \
    "uniform sampler2D uShadowMap;\n" \
    "uniform sampler2D uShadowMapWide;\n" \
    "uniform mat4 uLightVP;\n" \
    "uniform mat4 uLightVPWide;\n"

#define LIGHTING_CALC \
    /* sunFade: shadows fade out at low sun angles (dawn/dusk)             */ \
    "    float sunFade=clamp(uSunElev*10.0,0.0,1.0);\n" \
    "    float shadow=0.0;\n" \
    "    float NdotL=dot(shadow_n,-uSunDir);\n" \
    /* Back-facing surface is fully in shadow — no need to sample map */    \
    "    if(NdotL<=0.0){ shadow=1.0*sunFade; }\n" \
    "    else if(sunFade>0.0){\n" \
    /* Normal offset proportional to slope: steeper=larger offset needed */ \
    "    float slopeBias=clamp(1.0-NdotL,0.0,1.0);\n" \
    "    vec3 shadowOff=shadow_n*(1.5+slopeBias*3.5);\n" \
    "    vec4 lsFrag=uLightVP*vec4(vPos+shadowOff,1.0);\n" \
    "    vec3 projC=lsFrag.xyz/lsFrag.w*0.5+0.5;\n" \
    /* Poisson disk hoisted outside if/else so both cascades can use it */ \
    "    vec2 pd[12]=vec2[12](\n" \
    "        vec2(-0.326,-0.406),vec2(-0.840,-0.074),\n" \
    "        vec2(-0.696, 0.457),vec2(-0.203, 0.621),\n" \
    "        vec2( 0.962,-0.195),vec2( 0.473,-0.480),\n" \
    "        vec2( 0.519, 0.767),vec2( 0.185,-0.893),\n" \
    "        vec2( 0.507, 0.064),vec2( 0.896, 0.412),\n" \
    "        vec2(-0.322,-0.933),vec2(-0.792,-0.598));\n" \
    "    if(projC.z<=1.0&&projC.x>0.02&&projC.x<0.98&&projC.y>0.02&&projC.y<0.98){\n" \
    "        vec2 ts=1.0/vec2(textureSize(uShadowMap,0));\n" \
    "        float pcf=0.0; float bias=0.0015+slopeBias*0.0045;\n" \
    "        for(int i=0;i<12;i++){\n" \
    "            float sd=texture(uShadowMap,projC.xy+pd[i]*ts*2.5).r;\n" \
    "            pcf+=(projC.z-bias>sd)?1.0:0.0;}\n" \
    "        shadow=clamp(pcf/12.0,0.0,0.65)*sunFade;\n" \
    "    } else {\n" \
    "        vec3 shadowOffW=shadow_n*(3.0+slopeBias*6.0);\n" \
    "        vec4 lsW=uLightVPWide*vec4(vPos+shadowOffW,1.0);\n" \
    "        vec3 pW=lsW.xyz/lsW.w*0.5+0.5;\n" \
    "        if(pW.z<=1.0&&pW.x>0.0&&pW.x<1.0&&pW.y>0.0&&pW.y<1.0){\n" \
    "            vec2 tw=1.0/vec2(textureSize(uShadowMapWide,0));\n" \
    "            float pcfw=0.0; float biasW=0.0030+slopeBias*0.0060;\n" \
    "            for(int i=0;i<12;i++){\n" \
    "                float sd=texture(uShadowMapWide,pW.xy+pd[i]*tw*3.0).r;\n" \
    "                pcfw+=(pW.z-biasW>sd)?1.0:0.0;}\n" \
    "            shadow=clamp(pcfw/12.0,0.0,0.55)*sunFade*0.7;\n" \
    "        }\n" \
    "    }\n" \
    "    }\n" \
    "    float diff=max(dot(n,-uSunDir),0.0)*(1.0-shadow);\n" \
    "    vec3 col=base*(uAmbient+uSunColor*diff);\n" \
    "    for(int i=0;i<uNumLights;i++){\n" \
    "        vec3 lv=uLightPos[i]-vPos;float d=length(lv);\n" \
    "        float att=1.0-clamp(d/uLightRadius[i],0,1);att*=att;\n" \
    "        col+=base*uLightColor[i]*max(dot(n,normalize(lv)),0.0)*att;\n" \
    "}\n"
typedef struct {
    Vec3 pos,color;
    float radius,intensity,flicker_speed,flicker_phase;
    bool flicker;
} PointLight;
typedef struct {
    Vec3 sun_dir,sun_color,ambient;
    PointLight points[MAX_POINT_LIGHTS];
    int num_points;
    GLuint shadow_tex;       /* near cascade, slot 7 */
    GLuint shadow_tex_wide;  /* wide cascade, slot 6 */
    Mat4   light_vp;         /* near cascade VP */
    Mat4   light_vp_wide;    /* wide cascade VP */
    float  day_frac;    /* 0..1 through the day cycle */
    float  sun_elev;    /* -1..1, positive = sun above horizon */
} LightingSystem;

static void lighting_init(LightingSystem *ls){
    memset(ls,0,sizeof(*ls));
    ls->sun_dir=vec3_norm((Vec3){-0.5f,-1.0f,-0.3f});
    ls->sun_color=(Vec3){1.0f,0.95f,0.85f};
    ls->ambient=(Vec3){0.12f,0.12f,0.15f};
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
        ls->ambient=(Vec3){0.04f+0.09f*day_t, 0.04f+0.09f*day_t, 0.06f+0.10f*day_t};
    } else {
        /* Night: dim moonlight */
        ls->sun_color=(Vec3){0.04f,0.04f,0.09f};
        ls->ambient=(Vec3){0.09f,0.09f,0.13f};
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
    glUniform1f(glGetUniformLocation(prog,"uSunElev"),ls->sun_elev);
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
    /* Near cascade — slot 7, wide cascade — slot 6 */
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D,ls->shadow_tex);
    GLH_int(prog,"uShadowMap",7);
    GLH_mat4(prog,"uLightVP",ls->light_vp.m);
    if(ls->shadow_tex_wide){
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D,ls->shadow_tex_wide);
        GLH_int(prog,"uShadowMapWide",6);
        GLH_mat4(prog,"uLightVPWide",ls->light_vp_wide.m);
    }
}


/* ── Terrain ────────────────────────────────────────────────── */
#define MAX_WATER_ZONES  8
typedef struct{float pos[3],normal[3],color[3];}TVert;

/* Chunked LOD terrain
   512×512 grid split into TCNX×TCNZ chunks of TCSZ×TCSZ cells each.
   3 LOD levels per chunk: step=1 (full), step=2, step=4.
   Draw distance per LOD controlled by TC_DIST_* constants.        */
#define TCNX       16          /* chunks along X */
#define TCNZ       16          /* chunks along Z */
#define TCSZ       32          /* cells per chunk side (512/16) */
#define TC_LODS     3
#define TC_DIST_0  250.0f      /* LOD0 (full detail) within this dist */
#define TC_DIST_1  700.0f      /* LOD1 (half)        within this dist */
#define TC_DIST_2  1800.0f     /* LOD2 (quarter)     within this dist */

typedef struct{
    GLuint vao[TC_LODS];
    int    ic [TC_LODS];
    float  cx,cz;  /* world-space chunk centre */
}TerrainChunk;

typedef struct{
    TerrainChunk chunks[TCNX*TCNZ];
    GLuint land_prog,water_prog;
    GLuint water_vao;
    GLuint river_vaos[5];
    int    river_ic[5];
    GLuint zone_vaos[MAX_WATER_ZONES];  /* per-zone elevated water quads */
    GLuint water_detail_tex;
    GLuint worn_mask_tex;
    GLuint tex[6];       /* albedo: sand grass dirt rock snow lava */
    bool   has_tex[6];
    GLuint nrm[5];       /* normal maps: sand grass dirt rock snow */
    GLuint rgh[5];       /* roughness/AO: sand grass dirt rock snow */
    bool   has_nrm;      /* true if all 5 normal maps loaded */
    bool   has_rgh;      /* true if all 5 roughness maps loaded */
    bool   has_water_detail;
    GLuint hmap_tex;
    int    last_tris;
}Terrain;

/* Load a tileable texture; returns true in *loaded_out if the file was found.
   On failure a 1×1 solid-color fallback is generated so the shader never gets
   an unbound sampler.                                                          */
static GLuint terrain_tex_load2(const char *path, float r,float g,float b, bool *loaded_out){
    const char *src=path;
    if(access(src,F_OK)!=0){
        const char *base=strrchr(path,'/');
        if(base&&base[1]&&access(base+1,F_OK)==0) src=base+1;
    }
    int w,h,ch;
    uint8_t *px=stbi_load(src,&w,&h,&ch,4);
    GLuint id;glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);
    if(px){
        /* Normal/roughness maps are linear data — use GL_RGBA.
           Albedo textures are sRGB — callers use terrain_tex_load_srgb instead. */
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,px);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        /* anisotropic filtering — up to 16× if GPU supports it */
        float maxAniso=1.0f;
        glGetFloatv(0x84FF/*GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT*/,&maxAniso);
        if(maxAniso>1.0f)
            glTexParameterf(GL_TEXTURE_2D,0x84FE/*GL_TEXTURE_MAX_ANISOTROPY_EXT*/,
                            maxAniso>16.0f?16.0f:maxAniso);
        stbi_image_free(px);
        printf("[Terrain] Loaded %s (%dx%d)\n",src,w,h);
        if(loaded_out)*loaded_out=true;
        return id;
    }
    uint8_t fb[4]={(uint8_t)(r*255),(uint8_t)(g*255),(uint8_t)(b*255),255};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,fb);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    printf("[Terrain] '%s' not found — using flat fallback\n",path);
    if(loaded_out)*loaded_out=false;
    return id;
}
/* sRGB albedo loader — GPU converts sRGB→linear at sample time for correct lighting */
static GLuint terrain_tex_load_srgb(const char *path, float r,float g,float b){
    const char *src=path;
    if(access(src,F_OK)!=0){
        const char *base=strrchr(path,'/');
        if(base&&base[1]&&access(base+1,F_OK)==0) src=base+1;
    }
    int w,h,ch;
    uint8_t *px=stbi_load(src,&w,&h,&ch,4);
    GLuint id;glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);
    if(px){
        glTexImage2D(GL_TEXTURE_2D,0,GL_SRGB8_ALPHA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,px);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        float maxAniso=1.0f;
        glGetFloatv(0x84FF,&maxAniso);
        if(maxAniso>1.0f)
            glTexParameterf(GL_TEXTURE_2D,0x84FE,maxAniso>16.0f?16.0f:maxAniso);
        stbi_image_free(px);
        printf("[Terrain] Loaded sRGB %s (%dx%d)\n",src,w,h);
        return id;
    }
    uint8_t fb[4]={(uint8_t)(r*255),(uint8_t)(g*255),(uint8_t)(b*255),255};
    glTexImage2D(GL_TEXTURE_2D,0,GL_SRGB8_ALPHA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,fb);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    printf("[Terrain] '%s' not found — using sRGB fallback\n",path);
    return id;
}
/* Convenience wrapper (keeps old call-sites working) */
static GLuint terrain_tex_load(const char *path, float r,float g,float b){
    return terrain_tex_load2(path,r,g,b,NULL);
}

/* ── World map geography ──────────────────────────────────────
   World is 5000×5000 units centered at origin.
   u = (wx+2500)/5000   0=West  1=East
   v = (wz+2500)/5000   0=North 1=South  (game -Z = map North)

   Regions (from map):
     NW  Great Mashito Mountains  – high peaks 80-160
     NE  The Wallows              – scrubland 10-25
     N   Valwood forest + lake    – hills 8-20, lake at ~(0.43,0.22)
     CL  Old Tanthoon             – forested lowlands 4-14
     CE  Estuay/river confluence  – river flats 1-6
     SE  Kerone Stomplands        – open plains 3-10
     S   Crosswind Coast          – sandy coast 0-4
     SE  Coastal sea edge         – below water
   Rivers carved as smooth valleys through the landscape.
   ── ─────────────────────────────────────────────────────── */

/* Smooth falloff: 1 inside radius r, fades to 0 at r*2 */
static float __attribute__((unused)) region_weight(float px,float pz,float cx,float cz,float r){
    float d=sqrtf((px-cx)*(px-cx)+(pz-cz)*(pz-cz));
    return clampf(1.0f-d/r,0.0f,1.0f);
}
static float rw2(float px,float pz,float cx,float cz,float r){/* squared falloff, smoother */
    float d=sqrtf((px-cx)*(px-cx)+(pz-cz)*(pz-cz));
    float t=clampf(1.0f-d/r,0.0f,1.0f);return t*t;
}
/* Distance from point to line segment, returns 0..1 where 0=on line */
static float dist_to_segment(float px,float pz,float ax,float az,float bx,float bz){
    float abx=bx-ax,abz=bz-az,len2=abx*abx+abz*abz;
    if(len2<0.001f)return sqrtf((px-ax)*(px-ax)+(pz-az)*(pz-az));
    float t=clampf(((px-ax)*abx+(pz-az)*abz)/len2,0,1);
    float nx=ax+t*abx-px,nz=az+t*abz-pz;
    return sqrtf(nx*nx+nz*nz);
}
/* Multi-segment river: returns distance to nearest segment */
static float river_dist(float wx,float wz,const float *pts,int n){
    float best=1e9f;
    for(int i=0;i<n-2;i+=2)best=fminf(best,dist_to_segment(wx,wz,pts[i],pts[i+1],pts[i+2],pts[i+3]));
    return best;
}
#define BUILTIN_RIVER_COUNT 5
#define BUILTIN_RIVER_SURFACE_OFFSET 0.12f
static const float g_river_main[]={
    -1800,-2100, -1400,-1800, -900,-1400, -500,-900,
    -200,-400,    100, 100,   300, 600,   400,1400,  350,2400};
static const float g_river_east[]={
    100,100,  400,-100, 800,-200, 1300,0, 1800,300, 2300,800};
static const float g_river_tant[]={
    -2000,-600, -1600,-200, -1200,200, -800,600, -500,1000, -200,1400};
static const float g_river_lake[]={
    -150,-1550, -80,-1200, 0,-900, -100,-600};
static const float g_river_mirror[]={
    -900,-350, -600,-100, -200,100, 100,100};
typedef struct{
    const float *pts;
    int count;
} BuiltinRiverDef;
static const BuiltinRiverDef g_builtin_rivers[BUILTIN_RIVER_COUNT]={
    {g_river_main,   (int)(sizeof(g_river_main)/sizeof(float))},
    {g_river_east,   (int)(sizeof(g_river_east)/sizeof(float))},
    {g_river_tant,   (int)(sizeof(g_river_tant)/sizeof(float))},
    {g_river_lake,   (int)(sizeof(g_river_lake)/sizeof(float))},
    {g_river_mirror, (int)(sizeof(g_river_mirror)/sizeof(float))},
};
#define BUILTIN_RIVER_RENDER_HALF_WIDTH 54.0f

static float builtin_river_dist(float wx,float wz){
    float best=1e9f;
    for(int i=0;i<BUILTIN_RIVER_COUNT;i++)
        best=fminf(best,river_dist(wx,wz,g_builtin_rivers[i].pts,g_builtin_rivers[i].count));
    return best;
}
static float criver_dist(float wx,float wz);
static bool water_surface_info(float wx,float wz,float *out_level,int *out_preset);
static bool find_nearby_dry_ground(Vec3 origin,float max_radius,Vec3 *out_pos);
static float grass_mask_sample(float wx,float wz);
static bool grass_mask_base_allowed(float wx,float wz);
static float worn_path_sample(float wx,float wz);

/* ── Water Style System ─────────────────────────────────────────
   Global style applied to the entire water plane.
   Cycle presets with W key in the F4 river editor.
   Custom tweaks can be set directly on g_wstyle.               */
typedef struct {
    const char *name;
    float r,g,b;      /* base water colour */
    float wave_amp;   /* 0 = flat/ice,  1 = normal ocean */
    float wave_speed; /* 0.2 = slow, 1.0 = default, 2.0 = fast */
    float opacity;    /* 0 = invisible, 1 = solid */
    float spec_pow;   /* specular exponent: 8 rough → 128 glassy */
    float spec_str;   /* specular strength */
    float ice;        /* 0 = liquid, 1 = ice crack overlay */
    float depth_fade; /* transparency depth scale */
} WaterStyleDef;

/* Named presets — add more freely */
static const WaterStyleDef WATER_PRESETS[]={
/*  name              R     G     B    amp  spd   opac  pow   str   ice   df  */
{"Deep Ocean",      0.05f,0.19f,0.50f, 1.0f,1.0f, 0.88f, 64.f,0.60f,0.0f,1.0f},
{"Clear Shallow",   0.05f,0.38f,0.42f, 0.6f,0.8f, 0.50f, 32.f,0.40f,0.0f,0.4f},
{"Dark River",      0.03f,0.10f,0.22f, 1.2f,1.4f, 0.92f, 96.f,0.50f,0.0f,1.2f},
{"Murky Swamp",     0.08f,0.16f,0.07f, 0.4f,0.3f, 0.96f, 12.f,0.15f,0.0f,1.5f},
{"Ice Lake",        0.65f,0.80f,0.92f, 0.05f,0.1f,0.82f,128.f,0.90f,1.0f,0.6f},
{"Lava Flow",       0.72f,0.18f,0.02f, 1.8f,2.0f, 0.96f, 16.f,0.30f,0.0f,1.8f},
{"Emerald Pool",    0.04f,0.42f,0.28f, 0.5f,0.6f, 0.70f, 48.f,0.55f,0.0f,0.5f},
{"Arctic Melt",     0.40f,0.65f,0.78f, 0.3f,0.4f, 0.60f, 80.f,0.70f,0.4f,0.7f},
};
#define WATER_PRESET_COUNT (int)(sizeof(WATER_PRESETS)/sizeof(WATER_PRESETS[0]))

/* Runtime mutable style — starts as Deep Ocean, modified by preset or user */
typedef struct {
    float r,g,b;
    float wave_amp,wave_speed;
    float opacity,spec_pow,spec_str;
    float ice,depth_fade;
    int   preset_idx; /* which preset is active (-1 = custom) */
} WaterStyle;

static WaterStyle g_wstyle;

static void wstyle_apply_preset(int idx){
    if(idx<0||idx>=WATER_PRESET_COUNT)return;
    const WaterStyleDef *p=&WATER_PRESETS[idx];
    g_wstyle.r=p->r; g_wstyle.g=p->g; g_wstyle.b=p->b;
    g_wstyle.wave_amp=p->wave_amp; g_wstyle.wave_speed=p->wave_speed;
    g_wstyle.opacity=p->opacity;
    g_wstyle.spec_pow=p->spec_pow; g_wstyle.spec_str=p->spec_str;
    g_wstyle.ice=p->ice; g_wstyle.depth_fade=p->depth_fade;
    g_wstyle.preset_idx=idx;
    printf("[WaterStyle] -> %s\n",p->name);
}

static void wstyle_init(void){
    wstyle_apply_preset(0); /* Deep Ocean default */
}

/* Save / load preset index to river_custom.txt alongside river data */
/* (called from criver_save / criver_load which already open the file) */

/* ── End water style system ──────────────────────────────────── */

/* ── Water Zone System ──────────────────────────────────────────
   Up to 8 independent water bodies, each with its own style.
   Outside all zones the global g_wstyle (Deep Ocean) applies.
   Zones blend smoothly at their edges (10% falloff band).
   Edit in-game with F2.                                         */
#define WZONE_SAVE_FILE  "water_zones.txt"

typedef struct {
    float cx,cz;       /* world-space centre            */
    float radius;      /* influence radius               */
    float level;       /* water surface Y (world units) */
    int   preset_idx;  /* WaterStyleDef index            */
    bool  active;
} WaterZone;

static WaterZone g_wzones[MAX_WATER_ZONES];
static int       g_wzone_count=0;

static void wzone_save(void){
    FILE *f=fopen(WZONE_SAVE_FILE,"w");
    if(!f)return;
    fprintf(f,"# Water zones — %d entries\n",g_wzone_count);
    for(int i=0;i<g_wzone_count;i++){
        WaterZone *z=&g_wzones[i];
        if(!z->active)continue;
        fprintf(f,"%.1f %.1f %.1f %d %.2f\n",
                (double)z->cx,(double)z->cz,
                (double)z->radius,z->preset_idx,
                (double)z->level);
    }
    fclose(f);
    printf("[WZone] Saved %d zones to %s\n",g_wzone_count,WZONE_SAVE_FILE);
}

static void wzone_load(void){
    FILE *f=fopen(WZONE_SAVE_FILE,"r");
    if(!f)return;
    g_wzone_count=0;
    char line[64];
    while(fgets(line,sizeof(line),f)&&g_wzone_count<MAX_WATER_ZONES){
        if(line[0]=='#')continue;
        float cx,cz,rad,lv=WATER_LEVEL; int pi;
        int nf=sscanf(line,"%f %f %f %d %f",&cx,&cz,&rad,&pi,&lv);
        if(nf>=4){
            WaterZone *z=&g_wzones[g_wzone_count++];
            z->cx=cx;z->cz=cz;z->radius=rad;
            z->preset_idx=pi>=0&&pi<WATER_PRESET_COUNT?pi:0;
            z->level=(nf>=5)?lv:WATER_LEVEL;  /* backward compat */
            z->active=true;
        }
    }
    fclose(f);
    printf("[WZone] Loaded %d zones from %s\n",g_wzone_count,WZONE_SAVE_FILE);
}

/* ── wzone_local_level: returns water surface Y at world XZ ──
   Checks all zones sorted by proximity weight. Returns the level
   of the dominant zone, or WATER_LEVEL if outside all zones.    */
static float wzone_local_level(float wx, float wz){
    float best_t = 0.0f;
    float best_level = WATER_LEVEL;
    for(int zi=0;zi<g_wzone_count;zi++){
        WaterZone *z=&g_wzones[zi];
        if(!z->active)continue;
        float dx=wx-z->cx, dz2=wz-z->cz;
        float d=sqrtf(dx*dx+dz2*dz2);
        if(d>=z->radius)continue;
        float t=clampf(1.0f-(d/z->radius),0.0f,1.0f);
        t=t*t*(3.0f-2.0f*t); /* smoothstep */
        if(t>best_t){ best_t=t; best_level=z->level; }
    }
    return best_level;
}

/* ── End water zone system ───────────────────────────────────── */

/* ── wzone_rebuild_vaos: (re)build per-zone elevated water quads ──
   Called after any zone add/remove/resize/level-change.
   Zones at WATER_LEVEL don't need their own quad — the global ocean
   plane already covers them.  Zones at any other level get a quad
   centred at (cx, level+0.03, cz) spanning radius*1.15 in XZ.         */
static void wzone_rebuild_vaos(Terrain *t){
    /* Free any existing zone VAOs */
    for(int i=0;i<MAX_WATER_ZONES;i++){
        if(t->zone_vaos[i]){ glDeleteVertexArrays(1,&t->zone_vaos[i]); t->zone_vaos[i]=0; }
    }
    GLH_Attrib wa[]={{0,3,GL_FLOAT,GL_FALSE,0},{1,3,GL_FLOAT,GL_FALSE,12},{2,3,GL_FLOAT,GL_FALSE,24}};
    for(int zi=0;zi<g_wzone_count&&zi<MAX_WATER_ZONES;zi++){
        WaterZone *z=&g_wzones[zi];
        if(!z->active)continue;
        /* Skip zones sitting at global sea level — ocean quad covers them */
        float y  = z->level + 0.03f;
        float ext = z->radius * 1.15f;   /* slightly beyond radius for clean shore */
        float cx=z->cx, cz=z->cz;
        float verts[4*9]={
            cx-ext, y, cz-ext,  0,1,0,  0.08f,0.25f,0.55f,
            cx+ext, y, cz-ext,  0,1,0,  0.08f,0.25f,0.55f,
            cx+ext, y, cz+ext,  0,1,0,  0.08f,0.25f,0.55f,
            cx-ext, y, cz+ext,  0,1,0,  0.08f,0.25f,0.55f,
        };
        uint32_t idx[]={0,1,2,0,2,3};
        t->zone_vaos[zi]=GLH_mesh(verts,sizeof(verts),idx,sizeof(idx),
                                   9*sizeof(float),wa,3);
    }
}

/* ── Forward declarations (functions defined later but called above) ── */
/* CustomRiver and g_criver — defined here, used in terrain_draw above */
typedef struct { float pts[128*2]; int count; bool dirty; float width; } CustomRiver;
static CustomRiver g_criver;
static float  terrain_h(float wx, float wz);
static GLuint terrain_build_chunk_lod(float ox, float oz, int step, int *out_ic);

/* ═══════════════════════════════════════════════════════════════
   COBBLESTONE PATH SYSTEM  (F1 editor)
   Stores a spline of XZ points + half-width.  Builds a ribbon
   mesh that hugs terrain_h.  Fragment shader draws a procedural
   brick-offset cobblestone pattern with per-stone colour variation.
   ═══════════════════════════════════════════════════════════════ */
#define CPATH_MAX_PTS   128
#define CPATH_TILE_SZ   1.5f     /* cobblestone tile size in world units */
#define CPATH_SAVE_FILE "paths_custom.txt"

typedef struct {
    float  pts[CPATH_MAX_PTS*2]; /* interleaved X,Z pairs */
    int    count;
    float  width;                /* half-width in world units */
    bool   dirty;
} CustomPath;

typedef struct {
    GLuint vao,vbo,ebo;
    int    idx_count;
    bool   valid;
} PathMesh;

static CustomPath g_cpath;
static PathMesh   g_pmesh;
static GLuint     g_path_prog=0;
static GLuint     g_path_tex=0,g_path_nrm=0,g_path_rgh=0;
static bool       g_path_has_tex=false,g_path_has_nrm=false,g_path_has_rgh=false;
static bool       g_path_mat_init=false;

static float cpath_dist(float wx,float wz){
    if(g_cpath.count<2) return 1e9f;
    return river_dist(wx,wz,g_cpath.pts,g_cpath.count*2);
}

static void path_material_init(void){
    if(g_path_mat_init) return;
    g_path_mat_init=true;
    {
        static const char *const albedo_candidates[]={
            "ground/cobblestone_path.png",
            "ground/path_cobble.png",
            "ground/cobblestone_diff.png",
            "cobblestone_path.png",
            "path_cobble.png",
            "cobblestone_diff.png"
        };
        bool found=false;
        for(size_t i=0;i<sizeof(albedo_candidates)/sizeof(albedo_candidates[0]);i++){
            const char *p=albedo_candidates[i];
            const char *base=strrchr(p,'/');
            if(access(p,F_OK)==0 || (base&&access(base+1,F_OK)==0)){
                g_path_tex=terrain_tex_load_srgb(p,0.46f,0.44f,0.41f);
                found=true;
                break;
            }
        }
        if(!found) g_path_tex=terrain_tex_load_srgb(albedo_candidates[0],0.46f,0.44f,0.41f);
        g_path_has_tex=found;
    }
    {
        static const char *const normal_candidates[]={
            "ground/cobblestone_path_nor.png",
            "ground/path_cobble_nor.png",
            "ground/cobblestone_nor.png",
            "cobblestone_path_nor.png",
            "path_cobble_nor.png",
            "cobblestone_nor.png"
        };
        bool found=false;
        for(size_t i=0;i<sizeof(normal_candidates)/sizeof(normal_candidates[0]);i++){
            const char *p=normal_candidates[i];
            const char *base=strrchr(p,'/');
            if(access(p,F_OK)==0 || (base&&access(base+1,F_OK)==0)){
                g_path_nrm=terrain_tex_load2(p,0.50f,0.50f,1.00f,&g_path_has_nrm);
                found=true;
                break;
            }
        }
        if(!found) g_path_nrm=terrain_tex_load2(normal_candidates[0],0.50f,0.50f,1.00f,&g_path_has_nrm);
    }
    {
        static const char *const rough_candidates[]={
            "ground/cobblestone_path_rgh.png",
            "ground/path_cobble_rgh.png",
            "ground/cobblestone_rgh.png",
            "cobblestone_path_rgh.png",
            "path_cobble_rgh.png",
            "cobblestone_rgh.png"
        };
        bool found=false;
        for(size_t i=0;i<sizeof(rough_candidates)/sizeof(rough_candidates[0]);i++){
            const char *p=rough_candidates[i];
            const char *base=strrchr(p,'/');
            if(access(p,F_OK)==0 || (base&&access(base+1,F_OK)==0)){
                g_path_rgh=terrain_tex_load2(p,0.88f,1.00f,0.00f,&g_path_has_rgh);
                found=true;
                break;
            }
        }
        if(!found) g_path_rgh=terrain_tex_load2(rough_candidates[0],0.88f,1.00f,0.00f,&g_path_has_rgh);
    }
    printf("[PathMat] albedo:%s normal:%s rough:%s\n",
           g_path_has_tex?"loaded":"fallback",
           g_path_has_nrm?"loaded":"fallback",
           g_path_has_rgh?"loaded":"fallback");
}

/* Initialise shader once */
static void path_prog_init(void){
    if(g_path_prog) return;
    path_material_init();
    g_path_prog=GLH_program(
        /* ── Vertex ── */
        "#version 330 core\n"
        "layout(location=0)in vec3 aPos;\n"
        "layout(location=1)in vec2 aUV;\n"
        "uniform mat4 uProj,uView;\n"
        "out vec3 vPos; out vec2 vUV;\n"
        "void main(){\n"
        "    gl_Position=uProj*uView*vec4(aPos,1.0);\n"
        "    vPos=aPos; vUV=aUV;}\n",
        /* ── Fragment: procedural cobblestone ── */
        "#version 330 core\n"
        "in vec3 vPos; in vec2 vUV;\n"
        LIGHTING_UNIFORMS
        "uniform vec3 uCamPos,uFogColor;\n"
        "uniform float uFogDensity;\n"
        "uniform sampler2D uPathTex,uPathNrm,uPathRgh;\n"
        "uniform bool uHasPathTex,uHasPathNrm,uHasPathRgh;\n"
        "out vec4 FragColor;\n"
        "void main(){\n"
        /* vUV.x = 0..width_tiles across, vUV.y = dist/tile along path */
        "    vec2 uv = vUV;\n"
        "    vec3 base;\n"
        "    vec3 n=normalize(vec3(0.0,1.0,0.0));\n"
        "    float rough=0.88;\n"
        "    if(uHasPathTex){\n"
        "        base=texture(uPathTex,uv).rgb;\n"
        "    }else{\n"
        "        float row = floor(uv.y);\n"
        "        uv.x += mod(row, 2.0)*0.5;\n"
        "        vec2 sid = floor(uv);\n"
        "        float v=fract(sin(dot(sid,vec2(127.1,311.7)))*43758.5);\n"
        "        vec2 t=fract(uv);\n"
        "        float m=0.10;\n"
        "        float s=step(m,t.x)*(1.0-step(1.0-m,t.x))\n"
        "               *step(m,t.y)*(1.0-step(1.0-m,t.y));\n"
        "        vec2 e=smoothstep(0.0,m*1.5,t)*smoothstep(1.0,1.0-m*1.5,t);\n"
        "        float edge=e.x*e.y;\n"
        "        vec3 stone=vec3(0.46+v*0.14,0.44+v*0.11,0.41+v*0.10)*mix(0.75,1.0,edge);\n"
        "        vec3 mortar=vec3(0.24,0.23,0.21);\n"
        "        base=mix(mortar,stone,s);\n"
        "    }\n"
        "    if(uHasPathNrm){\n"
        "        vec3 tn=texture(uPathNrm,vUV).xyz*2.0-1.0;\n"
        "        n=normalize(vec3(tn.x*0.65,max(tn.z,0.35),tn.y*0.65));\n"
        "    }\n"
        "    if(uHasPathRgh){ rough=clamp(texture(uPathRgh,vUV).g,0.04,1.0); }\n"
        "    vec3 shadow_n=n;\n"
        LIGHTING_CALC
        "    vec3 view=normalize(uCamPos-vPos);\n"
        "    vec3 refl=reflect(-normalize(uSunDir),n);\n"
        "    col+=vec3(0.22)*pow(max(dot(refl,view),0.0),mix(40.0,8.0,rough))*clamp(1.0-rough,0.0,1.0)*0.45;\n"
        "    float fogFac=exp(-uFogDensity*length(vPos-uCamPos));\n"
        "    col=mix(uFogColor,col,clamp(fogFac,0.0,1.0));\n"
        "    FragColor=vec4(col,1.0);\n"
        "}\n");
}

/* Rebuild VAO ribbon mesh from current g_cpath points */
static void path_rebuild_mesh(void){
    path_prog_init();
    g_pmesh.valid=false; g_pmesh.idx_count=0;
    if(g_cpath.count<2) return;
    int N=g_cpath.count;
    float hw=g_cpath.width>0.5f?g_cpath.width:4.0f;
    /* 2 verts per control point, 6 indices per segment quad */
    float   *vd=malloc((size_t)N*2*5*sizeof(float));
    uint32_t *id=malloc((size_t)(N-1)*6*sizeof(uint32_t));
    if(!vd||!id){free(vd);free(id);return;}

    float dist=0.0f;
    for(int i=0;i<N;i++){
        float x=g_cpath.pts[i*2], z=g_cpath.pts[i*2+1];
        /* tangent */
        float tx,tz;
        if(i==0)          {tx=g_cpath.pts[2]-g_cpath.pts[0];   tz=g_cpath.pts[3]-g_cpath.pts[1];}
        else if(i==N-1)   {tx=g_cpath.pts[i*2]-g_cpath.pts[(i-1)*2]; tz=g_cpath.pts[i*2+1]-g_cpath.pts[(i-1)*2+1];}
        else              {tx=g_cpath.pts[(i+1)*2]-g_cpath.pts[(i-1)*2]; tz=g_cpath.pts[(i+1)*2+1]-g_cpath.pts[(i-1)*2+1];}
        float tlen=sqrtf(tx*tx+tz*tz);
        if(tlen>0.001f){tx/=tlen;tz/=tlen;}
        /* perpendicular (left) */
        float px=-tz, pz=tx;
        /* accumulated distance for UV.y */
        if(i>0){float dx=x-g_cpath.pts[(i-1)*2],dz=z-g_cpath.pts[(i-1)*2+1];dist+=sqrtf(dx*dx+dz*dz);}
        float uvy=dist/CPATH_TILE_SZ;
        float width_tiles=hw*2.0f/CPATH_TILE_SZ; /* total width in tiles */
        /* left vertex */
        float lx=x-px*hw, lz=z-pz*hw, ly=terrain_h(lx,lz)+0.06f;
        float *vL=&vd[(i*2)*5];
        vL[0]=lx; vL[1]=ly; vL[2]=lz; vL[3]=0.0f; vL[4]=uvy;
        /* right vertex */
        float rx=x+px*hw, rz=z+pz*hw, ry=terrain_h(rx,rz)+0.06f;
        float *vR=&vd[(i*2+1)*5];
        vR[0]=rx; vR[1]=ry; vR[2]=rz; vR[3]=width_tiles; vR[4]=uvy;
        /* quad indices */
        if(i<N-1){
            uint32_t *q=&id[i*6];
            q[0]=(uint32_t)(i*2);q[1]=(uint32_t)(i*2+1);q[2]=(uint32_t)((i+1)*2);
            q[3]=(uint32_t)(i*2+1);q[4]=(uint32_t)((i+1)*2+1);q[5]=(uint32_t)((i+1)*2);
        }
    }
    if(!g_pmesh.vao){glGenVertexArrays(1,&g_pmesh.vao);glGenBuffers(1,&g_pmesh.vbo);glGenBuffers(1,&g_pmesh.ebo);}
    glBindVertexArray(g_pmesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER,g_pmesh.vbo);
    glBufferData(GL_ARRAY_BUFFER,(GLsizeiptr)(N*2*5*sizeof(float)),vd,GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)(3*sizeof(float)));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,g_pmesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,(GLsizeiptr)((N-1)*6*sizeof(uint32_t)),id,GL_STATIC_DRAW);
    glBindVertexArray(0);
    g_pmesh.idx_count=(N-1)*6;
    g_pmesh.valid=true;
    free(vd);free(id);
}

/* Draw the path ribbon */
static void path_draw(LightingSystem *ls,Mat4 proj,Mat4 view,Vec3 cam_pos,float fog_density,Vec3 fog_col){
    if(!g_pmesh.valid||g_pmesh.idx_count<=0)return;
    path_prog_init();
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f,-1.0f); /* pull toward camera so it sits above terrain */
    lighting_push(ls,g_path_prog);
    GLH_begin(g_path_prog,proj.m,view.m);
    GLH_vec3(g_path_prog,"uCamPos",cam_pos.x,cam_pos.y,cam_pos.z);
    GLH_float(g_path_prog,"uFogDensity",fog_density);
    GLH_vec3(g_path_prog,"uFogColor",fog_col.x,fog_col.y,fog_col.z);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,g_path_tex);
    GLH_int(g_path_prog,"uPathTex",0);
    GLH_bool(g_path_prog,"uHasPathTex",g_path_has_tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,g_path_nrm);
    GLH_int(g_path_prog,"uPathNrm",1);
    GLH_bool(g_path_prog,"uHasPathNrm",g_path_has_nrm);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D,g_path_rgh);
    GLH_int(g_path_prog,"uPathRgh",2);
    GLH_bool(g_path_prog,"uHasPathRgh",g_path_has_rgh);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(g_pmesh.vao);
    glDrawElements(GL_TRIANGLES,g_pmesh.idx_count,GL_UNSIGNED_INT,0);
    glBindVertexArray(0);
    glPolygonOffset(0.0f,0.0f);
    glDisable(GL_POLYGON_OFFSET_FILL);
}

/* Save to file + print C array */
static void cpath_save(void){
    FILE *f=fopen(CPATH_SAVE_FILE,"w");
    if(f){
        fprintf(f,"# Cobblestone path — %d points\n",g_cpath.count);
        fprintf(f,"# width %.2f\n",g_cpath.width);
        for(int i=0;i<g_cpath.count;i++)
            fprintf(f,"%.2f %.2f\n",(double)g_cpath.pts[i*2],(double)g_cpath.pts[i*2+1]);
        fclose(f);
    }
    printf("[PathEd] Saved %d points. C array:\n",g_cpath.count);
    for(int i=0;i<g_cpath.count;i++)
        printf("    %.2ff,%.2ff,\n",(double)g_cpath.pts[i*2],(double)g_cpath.pts[i*2+1]);
}

/* Load from file */
static void cpath_load(void){
    FILE *f=fopen(CPATH_SAVE_FILE,"r");
    if(!f)return;
    char line[64]; g_cpath.count=0;
    while(fgets(line,sizeof(line),f)&&g_cpath.count<CPATH_MAX_PTS){
        if(line[0]=='#'){
            float w; if(sscanf(line,"# width %f",&w)==1) g_cpath.width=w;
            continue;
        }
        float x,z;
        if(sscanf(line,"%f %f",&x,&z)==2){
            g_cpath.pts[g_cpath.count*2]=x;
            g_cpath.pts[g_cpath.count*2+1]=z;
            g_cpath.count++;
        }
    }
    fclose(f);
    if(g_cpath.width<=0.5f) g_cpath.width=4.0f;
    g_cpath.dirty=true;
    printf("[PathEd] Loaded %d points from %s\n",g_cpath.count,CPATH_SAVE_FILE);
}

/* ── End path system (draw/HUD functions follow HUD/LineRenderer typedefs) ─ */

/* Frustum plane extraction from combined ProjView matrix (column-major) */
static void frustum_extract(float pl[6][4],Mat4 pv){
    float *m=pv.m;
    /* Planes from rows of clip matrix: left/right/bottom/top/near/far */
    pl[0][0]=m[3]+m[0];pl[0][1]=m[7]+m[4];pl[0][2]=m[11]+m[8]; pl[0][3]=m[15]+m[12];
    pl[1][0]=m[3]-m[0];pl[1][1]=m[7]-m[4];pl[1][2]=m[11]-m[8]; pl[1][3]=m[15]-m[12];
    pl[2][0]=m[3]+m[1];pl[2][1]=m[7]+m[5];pl[2][2]=m[11]+m[9]; pl[2][3]=m[15]+m[13];
    pl[3][0]=m[3]-m[1];pl[3][1]=m[7]-m[5];pl[3][2]=m[11]-m[9]; pl[3][3]=m[15]-m[13];
    pl[4][0]=m[3]+m[2];pl[4][1]=m[7]+m[6];pl[4][2]=m[11]+m[10];pl[4][3]=m[15]+m[14];
    pl[5][0]=m[3]-m[2];pl[5][1]=m[7]-m[6];pl[5][2]=m[11]-m[10];pl[5][3]=m[15]-m[14];
    for(int i=0;i<6;i++){
        float len=sqrtf(pl[i][0]*pl[i][0]+pl[i][1]*pl[i][1]+pl[i][2]*pl[i][2]);
        if(len>1e-6f){pl[i][0]/=len;pl[i][1]/=len;pl[i][2]/=len;pl[i][3]/=len;}
    }
}
/* Returns true if sphere is (possibly) visible — false = definitely culled */
static bool frustum_sphere(float pl[6][4],float cx,float cy,float cz,float r){
    for(int i=0;i<6;i++)
        if(pl[i][0]*cx+pl[i][1]*cy+pl[i][2]*cz+pl[i][3]<-r)return false;
    return true;
}

/* Rebuild all chunks whose bounding box overlaps a world-space circle.
   Called after the custom river changes — only dirty chunks get re-uploaded. */
static void terrain_rebuild_near(Terrain *t, float wx, float wz, float radius){
    float chunk_w=TCSZ*TERRAIN_CELL;
    int lod_steps[TC_LODS]={1,2,4};
    for(int i=0;i<TCNX*TCNZ;i++){
        TerrainChunk *ch=&t->chunks[i];
        float dx=fabsf(ch->cx-wx), dz=fabsf(ch->cz-wz);
        /* chunk half-size: chunk_w/2 + radius */
        float half=chunk_w*0.5f+radius;
        if(dx>half||dz>half)continue;
        /* delete old VAOs */
        glDeleteVertexArrays(TC_LODS,ch->vao);
        /* rebuild each LOD */
        float ox=ch->cx-chunk_w*0.5f, oo=ch->cz-chunk_w*0.5f;
        for(int l=0;l<TC_LODS;l++)
            ch->vao[l]=terrain_build_chunk_lod(ox,oo,lod_steps[l],&ch->ic[l]);
    }
}

/* Full world rebuild — used when many points change at once */
static void terrain_rebuild_all(Terrain *t){
    float chunk_w=TCSZ*TERRAIN_CELL;
    int lod_steps[TC_LODS]={1,2,4};
    float world_ox=-(TERRAIN_COLS*TERRAIN_CELL)*0.5f;
    float world_oz=-(TERRAIN_ROWS*TERRAIN_CELL)*0.5f;
    for(int cz=0;cz<TCNZ;cz++)for(int cx=0;cx<TCNX;cx++){
        TerrainChunk *ch=&t->chunks[cz*TCNX+cx];
        glDeleteVertexArrays(TC_LODS,ch->vao);
        float ox=world_ox+(float)cx*chunk_w;
        float oz=world_oz+(float)cz*chunk_w;
        for(int l=0;l<TC_LODS;l++)
            ch->vao[l]=terrain_build_chunk_lod(ox,oz,lod_steps[l],&ch->ic[l]);
    }
}

static void terrain_draw(Terrain *t,LightingSystem *ls,Mat4 proj,Mat4 view,Vec3 cam,
                         float time,float snow_accum,float wetness,float fog_density,Vec3 fog_color){
    lighting_push(ls,t->land_prog);
    GLH_begin(t->land_prog,proj.m,view.m);
    GLH_float(t->land_prog,"uTime",time);
    GLH_float(t->land_prog,"uSnowAccum",snow_accum);
    GLH_float(t->land_prog,"uWetness",wetness);
    GLH_float(t->land_prog,"uFogDensity",fog_density);
    GLH_vec3(t->land_prog,"uFogColor",fog_color.x,fog_color.y,fog_color.z);
    GLH_vec3(t->land_prog,"uCamPos",cam.x,cam.y,cam.z);
    /* albedo: units 0-5 */
    const char *names[]={"uTexSand","uTexGrass","uTexDirt","uTexRock","uTexSnow","uTexLava"};
    for(int i=0;i<6;i++){
        glActiveTexture(GL_TEXTURE0+i);
        glBindTexture(GL_TEXTURE_2D,t->tex[i]);
        GLH_int(t->land_prog,names[i],i);
    }
    /* normal maps: units 6-10 */
    const char *nnames[]={"uNrmSand","uNrmGrass","uNrmDirt","uNrmRock","uNrmSnow"};
    for(int i=0;i<5;i++){
        glActiveTexture(GL_TEXTURE6+i);
        glBindTexture(GL_TEXTURE_2D,t->nrm[i]);
        GLH_int(t->land_prog,nnames[i],6+i);
    }
    /* roughness/AO: units 11-15 */
    const char *rnames[]={"uRghSand","uRghGrass","uRghDirt","uRghRock","uRghSnow"};
    for(int i=0;i<5;i++){
        glActiveTexture(GL_TEXTURE11+i);
        glBindTexture(GL_TEXTURE_2D,t->rgh[i]);
        GLH_int(t->land_prog,rnames[i],11+i);
    }
    GLH_bool(t->land_prog,"uHasNrm",t->has_nrm);
    GLH_bool(t->land_prog,"uHasRgh",t->has_rgh);
    glActiveTexture(GL_TEXTURE16);
    glBindTexture(GL_TEXTURE_2D,t->worn_mask_tex);
    GLH_int(t->land_prog,"uWearMask",16);
    GLH_vec2(t->land_prog,"uWorldMin",-WORLD_HALF,-WORLD_HALF);
    GLH_vec2(t->land_prog,"uWorldSize",WORLD_HALF*2.0f,WORLD_HALF*2.0f);
    glActiveTexture(GL_TEXTURE0);
    float chunk_r=TCSZ*TERRAIN_CELL*0.75f;(void)chunk_r;
    int tris=0;
    for(int i=0;i<TCNX*TCNZ;i++){
        TerrainChunk *ch=&t->chunks[i];
        float dx=ch->cx-cam.x, dz=ch->cz-cam.z;
        float dist=sqrtf(dx*dx+dz*dz);
        int lod;
        if     (dist<TC_DIST_0)lod=0;
        else if(dist<TC_DIST_1)lod=1;
        else if(dist<TC_DIST_2)lod=2;
        else continue;
        GLH_draw_indexed(ch->vao[lod],ch->ic[lod]);
        tris+=ch->ic[lod]/3;
    }
    t->last_tris=tris;

    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-2.0f,-16.0f);
    lighting_push(ls,t->water_prog);
    GLH_begin(t->water_prog,proj.m,view.m);
    GLH_float(t->water_prog,"uTime",time);
    GLH_vec3(t->water_prog,"uCamPos",cam.x,cam.y,cam.z);
    GLH_float(t->water_prog,"uFogDensity",fog_density);
    GLH_vec3(t->water_prog,"uFogColor",fog_color.x,fog_color.y,fog_color.z);
    /* water style uniforms */
    GLH_vec3(t->water_prog,"uWaterColor",g_wstyle.r,g_wstyle.g,g_wstyle.b);
    GLH_float(t->water_prog,"uWaveAmp",g_wstyle.wave_amp);
    GLH_float(t->water_prog,"uWaveSpeed",g_wstyle.wave_speed);
    GLH_float(t->water_prog,"uOpacity",g_wstyle.opacity);
    GLH_float(t->water_prog,"uSpecPow",g_wstyle.spec_pow);
    GLH_float(t->water_prog,"uSpecStr",g_wstyle.spec_str);
    GLH_float(t->water_prog,"uIce",g_wstyle.ice);
    GLH_float(t->water_prog,"uDepthFade",g_wstyle.depth_fade);
    glActiveTexture(GL_TEXTURE9);
    glBindTexture(GL_TEXTURE_2D,t->hmap_tex);
    GLH_int(t->water_prog,"uTerrainHMap",9);
    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D,t->water_detail_tex);
    GLH_int(t->water_prog,"uDetailTex",10);
    GLH_bool(t->water_prog,"uHasDetailTex",t->has_water_detail);
    GLH_vec2(t->water_prog,"uWorldMin",-WORLD_HALF,-WORLD_HALF);
    GLH_vec2(t->water_prog,"uWorldSize",WORLD_HALF*2.0f,WORLD_HALF*2.0f);
    glActiveTexture(GL_TEXTURE0);
    {
        const WaterStyleDef *rp=&WATER_PRESETS[2];
        GLH_vec3(t->water_prog,"uWaterColor",rp->r,rp->g,rp->b);
        GLH_float(t->water_prog,"uWaveAmp",rp->wave_amp);
        GLH_float(t->water_prog,"uWaveSpeed",rp->wave_speed);
        GLH_float(t->water_prog,"uOpacity",1.0f);
        GLH_float(t->water_prog,"uSpecPow",rp->spec_pow);
        GLH_float(t->water_prog,"uSpecStr",rp->spec_str);
        GLH_float(t->water_prog,"uIce",rp->ice);
        GLH_float(t->water_prog,"uDepthFade",rp->depth_fade);
        GLH_int(t->water_prog,"uZoneCount",0);
        for(int ri=0;ri<BUILTIN_RIVER_COUNT;ri++){
            if(t->river_vaos[ri]&&t->river_ic[ri]>0)
                GLH_draw_indexed(t->river_vaos[ri],t->river_ic[ri]);
        }
        GLH_vec3(t->water_prog,"uWaterColor",g_wstyle.r,g_wstyle.g,g_wstyle.b);
        GLH_float(t->water_prog,"uWaveAmp",g_wstyle.wave_amp);
        GLH_float(t->water_prog,"uWaveSpeed",g_wstyle.wave_speed);
        GLH_float(t->water_prog,"uOpacity",g_wstyle.opacity);
        GLH_float(t->water_prog,"uSpecPow",g_wstyle.spec_pow);
        GLH_float(t->water_prog,"uSpecStr",g_wstyle.spec_str);
        GLH_float(t->water_prog,"uIce",g_wstyle.ice);
        GLH_float(t->water_prog,"uDepthFade",g_wstyle.depth_fade);
    }
    /* zone arrays */
    {
        int zc=0;
        float zpos[MAX_WATER_ZONES*2];
        float zrad[MAX_WATER_ZONES];
        float zcol[MAX_WATER_ZONES*3];
        float zwa[MAX_WATER_ZONES],zws[MAX_WATER_ZONES],zop[MAX_WATER_ZONES];
        float zsp[MAX_WATER_ZONES],zss[MAX_WATER_ZONES],zice[MAX_WATER_ZONES],zdf[MAX_WATER_ZONES];
        for(int zi=0;zi<g_wzone_count&&zc<MAX_WATER_ZONES;zi++){
            WaterZone *z=&g_wzones[zi];
            if(!z->active)continue;
            const WaterStyleDef *p=&WATER_PRESETS[z->preset_idx];
            zpos[zc*2]=z->cx; zpos[zc*2+1]=z->cz;
            zrad[zc]=z->radius;
            zcol[zc*3]=p->r; zcol[zc*3+1]=p->g; zcol[zc*3+2]=p->b;
            zwa[zc]=p->wave_amp; zws[zc]=p->wave_speed; zop[zc]=p->opacity;
            zsp[zc]=p->spec_pow; zss[zc]=p->spec_str; zice[zc]=p->ice; zdf[zc]=p->depth_fade;
            zc++;
        }
        GLH_int(t->water_prog,"uZoneCount",zc);
        if(zc>0){
            /* upload as float2 array for positions */
            glUniform2fv(glGetUniformLocation(t->water_prog,"uZonePos"),zc,zpos);
            GLH_float_array(t->water_prog,"uZoneRadius",zc,zrad);
            GLH_vec3_array(t->water_prog,"uZoneColor",zc,zcol);
            GLH_float_array(t->water_prog,"uZoneWaveAmp",zc,zwa);
            GLH_float_array(t->water_prog,"uZoneWaveSpeed",zc,zws);
            GLH_float_array(t->water_prog,"uZoneOpacity",zc,zop);
            GLH_float_array(t->water_prog,"uZoneSpecPow",zc,zsp);
            GLH_float_array(t->water_prog,"uZoneSpecStr",zc,zss);
            GLH_float_array(t->water_prog,"uZoneIce",zc,zice);
            GLH_float_array(t->water_prog,"uZoneDepthFade",zc,zdf);
        }
    }
    /* Only draw the global ocean quad if at least one sea-level water zone exists.
       Without this gate, any terrain sculpted below y=0 reveals the ocean plane
       even though the player never placed any water there. */
    {
        bool has_sea_zone=false;
        for(int _si=0;_si<g_wzone_count;_si++){
            if(g_wzones[_si].active&&fabsf(g_wzones[_si].level-WATER_LEVEL)<0.05f)
                { has_sea_zone=true; break; }
        }
        if(!has_sea_zone && g_criver.count>=2) has_sea_zone=true;
        if(has_sea_zone){
            GLH_draw_indexed(t->water_vao,6);
        }
    }

    /* ── Per-zone elevated water quads ─────────────────────────
       Each zone at a non-zero level gets its own quad rendered
       with that zone's style.  The global uniforms (uWaterColor
       etc.) are overridden per zone; zone count is set to 1 so
       the shader's zone lookup still applies a gentle edge blend. */
    for(int _zi=0;_zi<g_wzone_count&&_zi<MAX_WATER_ZONES;_zi++){
        WaterZone *_z=&g_wzones[_zi];
        if(!_z->active||!t->zone_vaos[_zi])continue;
        /* all zones get their own quad now that the global ocean is removed */
        const WaterStyleDef *_p=&WATER_PRESETS[_z->preset_idx];
        /* Push this zone's style as global so it's visible outside the 90% blend radius */
        GLH_vec3(t->water_prog,"uWaterColor",_p->r,_p->g,_p->b);
        GLH_float(t->water_prog,"uWaveAmp",  _p->wave_amp);
        GLH_float(t->water_prog,"uWaveSpeed",_p->wave_speed);
        GLH_float(t->water_prog,"uOpacity",  _p->opacity);
        GLH_float(t->water_prog,"uSpecPow",  _p->spec_pow);
        GLH_float(t->water_prog,"uSpecStr",  _p->spec_str);
        GLH_float(t->water_prog,"uIce",      _p->ice);
        GLH_float(t->water_prog,"uDepthFade",_p->depth_fade);
        /* For per-zone quads the global fallback (outside radius) must be
           fully transparent — otherwise the square quad bleeds water over
           surrounding terrain beyond the circular zone boundary. */
        GLH_float(t->water_prog,"uOpacity", 0.0f);
        /* Single-zone array with large radius for edge blend */
        GLH_int(t->water_prog,"uZoneCount",1);
        float _zpos[2]={_z->cx,_z->cz};
        float _zrad[1]={_z->radius};
        float _zcol[3]={_p->r,_p->g,_p->b};
        float _zwa[1]={_p->wave_amp},_zws[1]={_p->wave_speed},_zop[1]={_p->opacity};
        float _zsp[1]={_p->spec_pow},_zss[1]={_p->spec_str},_zice[1]={_p->ice},_zdf[1]={_p->depth_fade};
        glUniform2fv(glGetUniformLocation(t->water_prog,"uZonePos"),1,_zpos);
        GLH_float_array(t->water_prog,"uZoneRadius",1,_zrad);
        GLH_vec3_array(t->water_prog,"uZoneColor",1,_zcol);
        GLH_float_array(t->water_prog,"uZoneWaveAmp",1,_zwa);
        GLH_float_array(t->water_prog,"uZoneWaveSpeed",1,_zws);
        GLH_float_array(t->water_prog,"uZoneOpacity",1,_zop);
        GLH_float_array(t->water_prog,"uZoneSpecPow",1,_zsp);
        GLH_float_array(t->water_prog,"uZoneSpecStr",1,_zss);
        GLH_float_array(t->water_prog,"uZoneIce",1,_zice);
        GLH_float_array(t->water_prog,"uZoneDepthFade",1,_zdf);
        GLH_draw_indexed(t->zone_vaos[_zi],6);
    }

    glDisable(GL_POLYGON_OFFSET_FILL);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);glDisable(GL_BLEND);
}


/* ── Grass System ───────────────────────────────────────────── */
/* Instanced grass blades: cross-quad billboards that sway with wind
   and bend away from the player as they walk through.
   Grass is only spawned in the grass height band (WATER_LEVEL+0.5 ~ +5.5)
   on sufficiently flat terrain.  The instance buffer is rebuilt whenever
   the player moves more than GRASS_REBUILD_DIST units.               */
#define GRASS_RADIUS        48.0f    /* ring radius around player        */
#define GRASS_STEP           0.65f   /* spacing between blade origins    */
#define GRASS_MAX_BLADES    30000    /* hard upper limit                 */
#define GRASS_REBUILD_DIST   5.0f    /* rebuild when player moves this far */

typedef struct { float x,y,z,yaw,scale; } GrassInst;
typedef struct {
    GLuint prog, vao, base_vbo, base_ebo, inst_vbo;
    GrassInst *insts;
    int count;
    Vec3 last_center;
    GLuint tex;
} GrassSystem;

static void grass_init(GrassSystem *gs){
    memset(gs,0,sizeof(*gs));
    gs->insts = malloc(GRASS_MAX_BLADES*sizeof(GrassInst));
    gs->count = 0;
    gs->last_center = (Vec3){1e9f,0,1e9f};

    /* Base mesh: THREE quads at 0°, 60°, 120° — triple cross.
       With 3 quads 60° apart you never see a fully edge-on blade,
       eliminating the "flat rectangle" look from any camera angle.
       Each quad: 4 verts (x y z  u v), y=0 root, y=1 tip.
       The vertex shader tapers width → 0 at the tip, so the
       blade silhouette is a pointed triangle, not a rectangle.    */
    float bv[]={
        /* Quad A – along X axis (yaw=0°) */
        -0.5f,0.0f, 0.0f,  0.0f,1.0f,
         0.5f,0.0f, 0.0f,  1.0f,1.0f,
         0.5f,1.0f, 0.0f,  1.0f,0.0f,
        -0.5f,1.0f, 0.0f,  0.0f,0.0f,
        /* Quad B – rotated 60° */
        -0.4330f,0.0f,-0.2500f,  0.0f,1.0f,
         0.4330f,0.0f, 0.2500f,  1.0f,1.0f,
         0.4330f,1.0f, 0.2500f,  1.0f,0.0f,
        -0.4330f,1.0f,-0.2500f,  0.0f,0.0f,
        /* Quad C – rotated 120° */
        -0.4330f,0.0f, 0.2500f,  0.0f,1.0f,
         0.4330f,0.0f,-0.2500f,  1.0f,1.0f,
         0.4330f,1.0f,-0.2500f,  1.0f,0.0f,
        -0.4330f,1.0f, 0.2500f,  0.0f,0.0f,
    };
    uint16_t bi[]={
        0,1,2, 0,2,3,    /* quad A */
        4,5,6, 4,6,7,    /* quad B */
        8,9,10, 8,10,11  /* quad C */
    };

    glGenVertexArrays(1,&gs->vao);
    glBindVertexArray(gs->vao);

    glGenBuffers(1,&gs->base_vbo);
    glBindBuffer(GL_ARRAY_BUFFER,gs->base_vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(bv),bv,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)(3*sizeof(float)));

    glGenBuffers(1,&gs->inst_vbo);
    glBindBuffer(GL_ARRAY_BUFFER,gs->inst_vbo);
    glBufferData(GL_ARRAY_BUFFER,GRASS_MAX_BLADES*sizeof(GrassInst),NULL,GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(GrassInst),(void*)offsetof(GrassInst,x));
    glVertexAttribDivisor(2,1);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3,1,GL_FLOAT,GL_FALSE,sizeof(GrassInst),(void*)offsetof(GrassInst,yaw));
    glVertexAttribDivisor(3,1);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4,1,GL_FLOAT,GL_FALSE,sizeof(GrassInst),(void*)offsetof(GrassInst,scale));
    glVertexAttribDivisor(4,1);

    glGenBuffers(1,&gs->base_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,gs->base_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(bi),bi,GL_STATIC_DRAW);

    glBindVertexArray(0);

    gs->prog = GLH_program(
        /* ── VERTEX ── */
        "#version 330 core\n"
        "layout(location=0)in vec3 aPos;\n"
        "layout(location=1)in vec2 aUV;\n"
        "layout(location=2)in vec3 iPos;\n"
        "layout(location=3)in float iYaw;\n"
        "layout(location=4)in float iScale;\n"
        "uniform mat4 uProj,uView;\n"
        "uniform float uTime;\n"
        "uniform vec3 uPlayerPos,uCamPos;\n"
        "uniform vec2 uWindDir;\n"
        "uniform float uWindStr;\n"
        "out vec2 vUV;\n"
        "out float vTip;\n"
        "out float vAlpha;\n"        /* combined fade factor               */
        "void main(){\n"
        "    float tip=clamp(aPos.y,0.0,1.0);\n"

        /* ── Taper: width shrinks to 0 at the tip → pointed blade ── */
        "    float taper=mix(1.0,0.0,tip*tip);\n"

        /* ── Rotate the whole blade by iYaw ── */
        "    float cy=cos(iYaw),sy=sin(iYaw);\n"
        "    vec3 p=vec3(\n"
        "        aPos.x*taper*cy+aPos.z*taper*sy,\n"
        "        aPos.y,\n"
        "       -aPos.x*taper*sy+aPos.z*taper*cy);\n"

        /* ── Scale: thin width, taller height ── */
        "    p*=vec3(iScale*0.22, iScale*1.1, iScale*0.22);\n"

        /* ── Wind sway (tip moves most) ── */
        "    float turb=sin(uTime*2.1+iPos.x*0.11+iPos.z*0.09)*0.13\n"
        "              +sin(uTime*3.7+iPos.x*0.06-iPos.z*0.14)*0.07;\n"
        "    float wstr=(uWindStr*0.6+turb)*iScale;\n"
        "    p.x+=uWindDir.x*wstr*tip;\n"
        "    p.z+=uWindDir.y*wstr*tip;\n"

        /* ── Player push ── */
        "    vec2 d2=iPos.xz-uPlayerPos.xz;\n"
        "    float pd=length(d2);\n"
        "    float push=clamp(1.0-(pd/2.8),0.0,1.0);\n"
        "    push=push*push*iScale*2.0;\n"
        "    if(pd>0.01)p.xz+=normalize(d2)*push*tip;\n"

        "    vec4 worldPos=vec4(p+iPos,1.0);\n"
        "    gl_Position=uProj*uView*worldPos;\n"
        "    vUV=aUV;\n"
        "    vTip=tip;\n"

        /* ── Alpha: three fade effects ──────────────────────────────
           1. Distance fade: smooth falloff at radius edge
           2. Player proximity: ghost out blades you're inside
           3. View-angle fade: edge-on blades go transparent         */

        /* 1. Distance from player to blade origin (not world pos) */
        "    float distXZ=length(iPos.xz-uPlayerPos.xz);\n"
        "    float distFade=clamp(1.0-(distXZ-30.0)/10.0,0.0,1.0);\n"

        /* 2. Blades very close to player ghost out */
        "    float proxFade=clamp((pd-0.9)/1.0,0.0,1.0);\n"

        /* 3. View-angle: dot of blade's local X (its face normal) with
              view direction.  Near 0 = edge-on → fade transparent.
              Blade face normal after yaw rotate is (-sy, 0, cy).      */
        "    vec3 viewDir=normalize(worldPos.xyz - uCamPos);\n"
        "    vec3 bladeNorm=vec3(-sy,0.0,cy);\n"
        "    float edgeDot=abs(dot(bladeNorm,viewDir));\n"
        "    float angleFade=clamp((edgeDot-0.08)/0.18,0.0,1.0);\n"

        "    vAlpha=distFade*proxFade*angleFade;\n"
        "}\n",
        /* ── FRAGMENT ── */
        "#version 330 core\n"
        "in vec2 vUV;\n"
        "in float vTip;\n"
        "in float vAlpha;\n"
        "uniform sampler2D uGrassTex;\n"
        "uniform vec3 uSunColor,uAmbient;\n"
        "out vec4 FragColor;\n"
        "void main(){\n"
        "    vec4 c=texture(uGrassTex,vUV);\n"
        /* Procedural blade shape when no texture: alpha = pointed silhouette */
        "    float bx=abs(vUV.x*2.0-1.0);          /* 0 at centre, 1 at edge */\n"
        "    float shapeMask=clamp(1.0-bx/(1.0-vTip*0.92+0.08),0.0,1.0);\n"
        "    c.a*=shapeMask;\n"
        "    if(c.a*vAlpha<0.08)discard;\n"
        "    vec3 col=c.rgb*mix(0.40,1.0,vTip);\n"
        "    col*=(uAmbient+uSunColor*0.55);\n"
        "    FragColor=vec4(col,c.a*vAlpha);\n"
        "}\n");

    gs->tex = terrain_tex_load("grass_blade.png", 0.22f,0.55f,0.10f);
}

/* Rebuild the instance buffer.  Called whenever the player moves
   more than GRASS_REBUILD_DIST units from the last rebuild centre. */
/* Comparator for back-to-front sort — needs camera pos passed via a static */
static Vec3 g_grass_sort_cam;
static int grass_sort_cmp(const void *a, const void *b){
    const GrassInst *ga=(const GrassInst*)a;
    const GrassInst *gb=(const GrassInst*)b;
    float dxa=ga->x-g_grass_sort_cam.x, dza=ga->z-g_grass_sort_cam.z;
    float dxb=gb->x-g_grass_sort_cam.x, dzb=gb->z-g_grass_sort_cam.z;
    float da2=dxa*dxa+dza*dza, db2=dxb*dxb+dzb*dzb;
    /* Far-first (back to front) */
    return (da2>db2)?-1:(da2<db2)?1:0;
}

static void grass_update(GrassSystem *gs, Vec3 player){
    float dx=player.x-gs->last_center.x, dz=player.z-gs->last_center.z;
    if(dx*dx+dz*dz < GRASS_REBUILD_DIST*GRASS_REBUILD_DIST) return;
    gs->last_center=player;
    gs->count=0;

    float r=GRASS_RADIUS;
    float step=GRASS_STEP;
    for(float zz=-r; zz<=r && gs->count<GRASS_MAX_BLADES; zz+=step){
        for(float xx=-r; xx<=r && gs->count<GRASS_MAX_BLADES; xx+=step){
            if(xx*xx+zz*zz > r*r) continue;
            float jx=sinf(xx*13.7f+zz*7.3f)*step*0.30f;
            float jz=cosf(xx*5.1f+zz*11.9f)*step*0.30f;
            float wx=player.x+xx+jx;
            float wz=player.z+zz+jz;
            float seed=wx*127.1f+wz*311.7f;
            if(!grass_mask_base_allowed(wx,wz)) continue;
            float density=grass_mask_sample(wx,wz);
            if(density<0.10f) continue;
            density=clampf((density-0.10f)/0.90f,0.0f,1.0f);
            if(fmodf(fabsf(seed*0.0173f),1.0f) > density) continue;
            float h=terrain_h(wx,wz);
            GrassInst *g=&gs->insts[gs->count++];
            g->x=wx; g->y=h; g->z=wz;
            g->yaw  = fmodf(fabsf(seed),6.2832f);
            g->scale=(0.10f+0.18f*fmodf(fabsf(seed*0.0137f),1.0f))*(0.55f+(1.15f-0.55f)*density);
        }
    }

    /* Sort back-to-front so alpha-blended blades composite correctly.
       Sort key = squared distance from player (camera position at rebuild time). */
    g_grass_sort_cam=player;
    qsort(gs->insts, gs->count, sizeof(GrassInst), grass_sort_cmp);

    glBindBuffer(GL_ARRAY_BUFFER,gs->inst_vbo);
    glBufferSubData(GL_ARRAY_BUFFER,0,gs->count*sizeof(GrassInst),gs->insts);
    glBindBuffer(GL_ARRAY_BUFFER,0);
}

static void grass_draw(GrassSystem *gs,LightingSystem *ls,
                       Mat4 proj,Mat4 view,Vec3 player,Vec3 cam_pos,float time,
                       float wind_x,float wind_z,float wind_str){
    if(gs->count==0) return;

    /* ── KEY FIX: depth mask OFF while drawing alpha-blended billboards.
       With depth mask ON, transparent pixels write depth → their triangle
       edge blocks everything behind it → the "diagonal streak" artifact.
       With depth mask OFF blades composite correctly via the sorted order. */
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    /* Alpha-to-coverage: uses the MSAA sample mask for crisp silhouette
       edges without the hard-discard staircase look.                     */
    glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);

    glUseProgram(gs->prog);
    glUniformMatrix4fv(glGetUniformLocation(gs->prog,"uProj"),1,GL_FALSE,proj.m);
    glUniformMatrix4fv(glGetUniformLocation(gs->prog,"uView"),1,GL_FALSE,view.m);
    glUniform1f(glGetUniformLocation(gs->prog,"uTime"),time);
    glUniform3f(glGetUniformLocation(gs->prog,"uPlayerPos"),player.x,player.y,player.z);
    glUniform3f(glGetUniformLocation(gs->prog,"uCamPos"),cam_pos.x,cam_pos.y,cam_pos.z);
    glUniform2f(glGetUniformLocation(gs->prog,"uWindDir"),wind_x,wind_z);
    glUniform1f(glGetUniformLocation(gs->prog,"uWindStr"),wind_str);
    glUniform3f(glGetUniformLocation(gs->prog,"uSunColor"),
                ls->sun_color.x,ls->sun_color.y,ls->sun_color.z);
    glUniform3f(glGetUniformLocation(gs->prog,"uAmbient"),
                ls->ambient.x,ls->ambient.y,ls->ambient.z);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,gs->tex);
    glUniform1i(glGetUniformLocation(gs->prog,"uGrassTex"),0);

    glBindVertexArray(gs->vao);
    glDrawElementsInstanced(GL_TRIANGLES,18,GL_UNSIGNED_SHORT,0,gs->count);
    glBindVertexArray(0);

    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);   /* restore depth writes for all subsequent geometry */
}

static void grass_free(GrassSystem *gs){
    free(gs->insts);
    glDeleteVertexArrays(1,&gs->vao);
    glDeleteBuffers(1,&gs->base_vbo);
    glDeleteBuffers(1,&gs->base_ebo);
    glDeleteBuffers(1,&gs->inst_vbo);
    glDeleteTextures(1,&gs->tex);
}

/* Helper macro to stringify a #define number into a string literal */
#ifndef STRINGIZE
#define STRINGIZE_(x) #x
#define STRINGIZE(x) STRINGIZE_(x)
#endif

/* ── Puddle System ──────────────────────────────────────────────
   Instanced flat reflective quads on low/flat terrain around the
   player.  Appear as wetness rises; removed when drying out.
   Each puddle is a single quad lying in the XZ plane.             */
#define PUDDLE_MAX      300
#define PUDDLE_RADIUS   55.0f
#define PUDDLE_STEP      5.5f

typedef struct { float x,y,z,size,yaw; } PuddleInst;
typedef struct {
    GLuint prog,vao,base_vbo,base_ebo,inst_vbo;
    PuddleInst insts[PUDDLE_MAX];
    int count;
    Vec3 last_center;
} PuddleSystem;

static void puddle_init(PuddleSystem *ps){
    memset(ps,0,sizeof(*ps));
    ps->last_center=(Vec3){1e9f,0,1e9f};

    /* Flat quad vertices: x,z in [-0.5,0.5], y=0 */
    float bv[]={ -0.5f,0,0.5f, 0.5f,0,0.5f, 0.5f,0,-0.5f, -0.5f,0,-0.5f };
    uint16_t bi[]={0,1,2, 0,2,3};

    glGenVertexArrays(1,&ps->vao);
    glBindVertexArray(ps->vao);

    glGenBuffers(1,&ps->base_vbo);
    glBindBuffer(GL_ARRAY_BUFFER,ps->base_vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(bv),bv,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);

    glGenBuffers(1,&ps->inst_vbo);
    glBindBuffer(GL_ARRAY_BUFFER,ps->inst_vbo);
    glBufferData(GL_ARRAY_BUFFER,PUDDLE_MAX*sizeof(PuddleInst),NULL,GL_DYNAMIC_DRAW);
    /* loc 1: world XYZ */
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(PuddleInst),(void*)offsetof(PuddleInst,x));
    glVertexAttribDivisor(1,1);
    /* loc 2: size */
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,1,GL_FLOAT,GL_FALSE,sizeof(PuddleInst),(void*)offsetof(PuddleInst,size));
    glVertexAttribDivisor(2,1);
    /* loc 3: yaw (random rotation so puddles look irregular) */
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3,1,GL_FLOAT,GL_FALSE,sizeof(PuddleInst),(void*)offsetof(PuddleInst,yaw));
    glVertexAttribDivisor(3,1);

    glGenBuffers(1,&ps->base_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ps->base_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(bi),bi,GL_STATIC_DRAW);
    glBindVertexArray(0);

    ps->prog=GLH_program(
        "#version 330 core\n"
        "layout(location=0)in vec3 aPos;\n"
        "layout(location=1)in vec3 iWorldPos;\n"
        "layout(location=2)in float iSize;\n"
        "layout(location=3)in float iYaw;\n"
        "uniform mat4 uProj,uView;\n"
        "out vec3 vWorldPos;\n"
        "void main(){\n"
        "    float cy=cos(iYaw),sy=sin(iYaw);\n"
        "    vec3 p=vec3(aPos.x*cy-aPos.z*sy, 0.0, aPos.x*sy+aPos.z*cy)*iSize+iWorldPos;\n"
        "    vWorldPos=p;\n"
        "    gl_Position=uProj*uView*vec4(p,1.0);\n"
        "}\n",
        "#version 330 core\n"
        "in vec3 vWorldPos;\n"
        "uniform vec3 uSunColor,uAmbient,uSunDir,uCamPos;\n"
        "uniform float uTime,uWetness;\n"
        "out vec4 FragColor;\n"
        "void main(){\n"
        /* ripple normal from animated sin waves */
        "    float rx=sin(vWorldPos.x*2.1+uTime*1.4)*0.04+sin(vWorldPos.z*1.7+uTime*1.1)*0.03;\n"
        "    float rz=sin(vWorldPos.z*2.3+uTime*1.2)*0.04+sin(vWorldPos.x*1.9+uTime*0.9)*0.03;\n"
        "    vec3 n=normalize(vec3(rx,1.0,rz));\n"
        "    vec3 base=vec3(0.04,0.10,0.22);\n"
        "    float diff=max(dot(n,-uSunDir),0.0);\n"
        "    vec3 col=base*(uAmbient+uSunColor*diff*0.5);\n"
        "    vec3 view=normalize(uCamPos-vWorldPos);\n"
        "    vec3 refl=reflect(uSunDir,n);\n"
        "    col+=vec3(0.9)*pow(max(dot(refl,view),0.0),64.0)*0.7;\n"
        "    FragColor=vec4(col,uWetness*0.75);\n"
        "}\n");
}

static void puddle_update(PuddleSystem *ps,Vec3 player,float wetness){
    if(wetness<0.15f){ps->count=0;return;}
    float dx=player.x-ps->last_center.x, dz=player.z-ps->last_center.z;
    if(dx*dx+dz*dz < 10.0f*10.0f) return;
    ps->last_center=player;
    ps->count=0;
    float r=PUDDLE_RADIUS, step=PUDDLE_STEP;
    for(float zz=-r;zz<=r&&ps->count<PUDDLE_MAX;zz+=step){
        for(float xx=-r;xx<=r&&ps->count<PUDDLE_MAX;xx+=step){
            if(xx*xx+zz*zz>r*r) continue;
            float jx=sinf(xx*17.3f+zz*9.1f)*step*0.45f;
            float jz=cosf(xx*6.7f+zz*13.3f)*step*0.45f;
            float wx=player.x+xx+jx, wz=player.z+zz+jz;
            float h=terrain_h(wx,wz);
            if(h<WATER_LEVEL+0.05f||h>WATER_LEVEL+3.0f) continue; /* low flat only */
            if(builtin_river_dist(wx,wz)<BUILTIN_RIVER_RENDER_HALF_WIDTH+8.0f) continue;
            if(criver_dist(wx,wz)<fmaxf(g_criver.width,18.0f)+8.0f) continue;
            if(water_surface_info(wx,wz,NULL,NULL)) continue;
            /* skip steep terrain */
            float hL=terrain_h(wx-0.5f,wz),hR=terrain_h(wx+0.5f,wz);
            float hD=terrain_h(wx,wz-0.5f),hU=terrain_h(wx,wz+0.5f);
            Vec3 n=vec3_norm((Vec3){hL-hR,1.0f,hD-hU});
            if(n.y<0.90f) continue;
            /* deterministic: only some cells get a puddle */
            float seed=wx*91.3f+wz*173.7f;
            if(fmodf(fabsf(seed),1.0f)>wetness*0.55f) continue;
            PuddleInst *p=&ps->insts[ps->count++];
            p->x=wx; p->y=h+0.02f; p->z=wz;
            p->size=1.5f+2.5f*fmodf(fabsf(seed*0.023f),1.0f);
            p->yaw=fmodf(fabsf(seed),6.2832f);
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER,ps->inst_vbo);
    glBufferSubData(GL_ARRAY_BUFFER,0,ps->count*sizeof(PuddleInst),ps->insts);
}

static void puddle_draw(PuddleSystem *ps,LightingSystem *ls,
                        Mat4 proj,Mat4 view,Vec3 cam,float time,float wetness){
    if(ps->count==0||wetness<0.15f) return;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glUseProgram(ps->prog);
    glUniformMatrix4fv(glGetUniformLocation(ps->prog,"uProj"),1,GL_FALSE,proj.m);
    glUniformMatrix4fv(glGetUniformLocation(ps->prog,"uView"),1,GL_FALSE,view.m);
    glUniform1f(glGetUniformLocation(ps->prog,"uTime"),time);
    glUniform1f(glGetUniformLocation(ps->prog,"uWetness"),wetness);
    glUniform3f(glGetUniformLocation(ps->prog,"uSunColor"),ls->sun_color.x,ls->sun_color.y,ls->sun_color.z);
    glUniform3f(glGetUniformLocation(ps->prog,"uAmbient"),ls->ambient.x,ls->ambient.y,ls->ambient.z);
    glUniform3f(glGetUniformLocation(ps->prog,"uSunDir"),ls->sun_dir.x,ls->sun_dir.y,ls->sun_dir.z);
    glUniform3f(glGetUniformLocation(ps->prog,"uCamPos"),cam.x,cam.y,cam.z);
    glBindVertexArray(ps->vao);
    glDrawElementsInstanced(GL_TRIANGLES,6,GL_UNSIGNED_SHORT,0,ps->count);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

static void puddle_free(PuddleSystem *ps){
    glDeleteVertexArrays(1,&ps->vao);
    glDeleteBuffers(1,&ps->base_vbo);
    glDeleteBuffers(1,&ps->base_ebo);
    glDeleteBuffers(1,&ps->inst_vbo);
}

/* ── Footprint System ───────────────────────────────────────────
   Ring buffer of up to 200 decals stamped into snowy terrain as
   the player walks.  Each decal is a flat quad lying in XZ.
   Fades out over FOOTPRINT_LIFE seconds.                         */
#define FOOTPRINT_MAX  200
#define FOOTPRINT_LIFE 10.0f
#define FOOTPRINT_DIST  0.9f   /* min walk distance between prints */

typedef struct { float x,y,z,yaw,age; } Footprint;
typedef struct {
    GLuint prog,vao,base_vbo,base_ebo,inst_vbo;
    Footprint fp[FOOTPRINT_MAX];
    int head,count;           /* ring buffer */
    float dist_accum;         /* distance walked since last print */
    Vec3  last_pos;
} FootprintSystem;

/* Per-instance data sent to GPU: world XYZ, yaw, alpha */
typedef struct { float x,y,z,yaw,alpha; } FpInst;

static void footprint_init(FootprintSystem *fs){
    memset(fs,0,sizeof(*fs));
    fs->last_pos=(Vec3){1e9f,0,1e9f};

    float bv[]={ -0.22f,0,0.45f, 0.22f,0,0.45f, 0.22f,0,-0.45f, -0.22f,0,-0.45f };
    uint16_t bi[]={0,1,2, 0,2,3};

    glGenVertexArrays(1,&fs->vao);
    glBindVertexArray(fs->vao);

    glGenBuffers(1,&fs->base_vbo);
    glBindBuffer(GL_ARRAY_BUFFER,fs->base_vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(bv),bv,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);

    glGenBuffers(1,&fs->inst_vbo);
    glBindBuffer(GL_ARRAY_BUFFER,fs->inst_vbo);
    glBufferData(GL_ARRAY_BUFFER,FOOTPRINT_MAX*sizeof(FpInst),NULL,GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(1); /* world XYZ */
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(FpInst),(void*)0);
    glVertexAttribDivisor(1,1);
    glEnableVertexAttribArray(2); /* yaw */
    glVertexAttribPointer(2,1,GL_FLOAT,GL_FALSE,sizeof(FpInst),(void*)(3*sizeof(float)));
    glVertexAttribDivisor(2,1);
    glEnableVertexAttribArray(3); /* alpha */
    glVertexAttribPointer(3,1,GL_FLOAT,GL_FALSE,sizeof(FpInst),(void*)(4*sizeof(float)));
    glVertexAttribDivisor(3,1);

    glGenBuffers(1,&fs->base_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,fs->base_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(bi),bi,GL_STATIC_DRAW);
    glBindVertexArray(0);

    fs->prog=GLH_program(
        "#version 330 core\n"
        "layout(location=0)in vec3 aPos;\n"
        "layout(location=1)in vec3 iWorld;\n"
        "layout(location=2)in float iYaw;\n"
        "layout(location=3)in float iAlpha;\n"
        "uniform mat4 uProj,uView;\n"
        "out float vAlpha;\n"
        "void main(){\n"
        "    float cy=cos(iYaw),sy=sin(iYaw);\n"
        "    vec3 p=vec3(aPos.x*cy-aPos.z*sy, 0.005, aPos.x*sy+aPos.z*cy)+iWorld;\n"
        "    gl_Position=uProj*uView*vec4(p,1.0);\n"
        "    vAlpha=iAlpha;\n"
        "}\n",
        "#version 330 core\n"
        "in float vAlpha;\n"
        "out vec4 FragColor;\n"
        "void main(){\n"
        /* dark indentation shadow */
        "    FragColor=vec4(0.12,0.14,0.18, vAlpha*0.55);\n"
        "}\n");
}

static void footprint_update(FootprintSystem *fs,Vec3 player,float dt,float snow_accum){
    /* Age all existing prints */
    for(int i=0;i<FOOTPRINT_MAX;i++) if(fs->fp[i].age>0) fs->fp[i].age-=dt;

    if(snow_accum<0.45f) return;
    if(fs->last_pos.x>1e8f){fs->last_pos=player;return;}

    float ddx=player.x-fs->last_pos.x, ddz=player.z-fs->last_pos.z;
    float moved=sqrtf(ddx*ddx+ddz*ddz);
    fs->dist_accum+=moved;
    fs->last_pos=player;

    if(fs->dist_accum<FOOTPRINT_DIST) return;
    fs->dist_accum=0.0f;

    float h=terrain_h(player.x,player.z);
    if(h<WATER_LEVEL+0.3f||h>WATER_LEVEL+7.0f) return;

    /* stamp */
    Footprint *fp=&fs->fp[fs->head];
    fp->x=player.x; fp->y=h+0.01f; fp->z=player.z;
    fp->yaw=player.y; /* reuse y field for yaw — set below */
    /* compute walk direction from last movement */
    fp->yaw=atan2f(ddx,ddz);
    fp->age=FOOTPRINT_LIFE;
    fs->head=(fs->head+1)%FOOTPRINT_MAX;
    if(fs->count<FOOTPRINT_MAX) fs->count++;
}

static void footprint_draw(FootprintSystem *fs,Mat4 proj,Mat4 view){
    if(fs->count==0) return;

    /* Pack live prints into a temp inst buffer */
    static FpInst buf[FOOTPRINT_MAX];
    int n=0;
    for(int i=0;i<FOOTPRINT_MAX;i++){
        Footprint *fp=&fs->fp[i];
        if(fp->age<=0) continue;
        buf[n].x=fp->x; buf[n].y=fp->y; buf[n].z=fp->z;
        buf[n].yaw=fp->yaw;
        buf[n].alpha=clampf(fp->age/FOOTPRINT_LIFE,0.0f,1.0f);
        n++;
    }
    if(n==0) return;

    glBindBuffer(GL_ARRAY_BUFFER,fs->inst_vbo);
    glBufferSubData(GL_ARRAY_BUFFER,0,n*sizeof(FpInst),buf);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f,-1.0f); /* prevent z-fighting with terrain */

    glUseProgram(fs->prog);
    glUniformMatrix4fv(glGetUniformLocation(fs->prog,"uProj"),1,GL_FALSE,proj.m);
    glUniformMatrix4fv(glGetUniformLocation(fs->prog,"uView"),1,GL_FALSE,view.m);
    glBindVertexArray(fs->vao);
    glDrawElementsInstanced(GL_TRIANGLES,6,GL_UNSIGNED_SHORT,0,n);
    glBindVertexArray(0);

    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

static void footprint_free(FootprintSystem *fs){
    glDeleteVertexArrays(1,&fs->vao);
    glDeleteBuffers(1,&fs->base_vbo);
    glDeleteBuffers(1,&fs->base_ebo);
    glDeleteBuffers(1,&fs->inst_vbo);
}

/* ── Wind System ────────────────────────────────────────────────
   A slowly evolving world wind that drives:
     • grass blade lean direction + magnitude
     • snow / rain particle drift
     • terrain snow accumulation (snow weather drives buildup)

   Direction rotates smoothly; strength breathes between calm and
   strong; occasional gusts spike the strength for a few seconds. */
typedef struct {
    float dir_angle;        /* current wind heading, radians            */
    float target_angle;     /* angle we're drifting toward              */
    float strength;         /* 0..1  smoothed base strength             */
    float target_strength;
    float gust;             /* 0..1  short pulse added on top           */
    float gust_timer;
    float gust_cooldown;
    float change_timer;     /* seconds until next target re-roll        */
    /* Derived (recomputed each update) */
    float wx, wz;           /* unit XZ direction vector                 */
    float total;            /* combined strength+gust, clamped 0..1     */
    /* Snow accumulation: 0=no extra snow, 1=full global blizzard       */
    float snow_accum;
} WindSystem;

static void wind_init(WindSystem *w){
    memset(w,0,sizeof(*w));
    w->dir_angle      = (float)(rand()%628)/100.0f;
    w->target_angle   = w->dir_angle;
    w->strength       = 0.25f;
    w->target_strength= 0.25f;
    w->gust_cooldown  = 8.0f + (float)(rand()%60)/10.0f;
}

static void wind_update(WindSystem *w, float dt, float total_time){
    /* ── Pick new target direction/strength periodically ── */
    w->change_timer -= dt;
    if(w->change_timer <= 0.0f){
        w->change_timer = 12.0f + (float)(rand()%200)/10.0f;
        /* Turn by up to ±60° */
        w->target_angle = w->dir_angle + ((float)(rand()%1200)-600.0f)*(float)M_PI/1800.0f;
        w->target_strength = 0.1f + (float)(rand()%100)/100.0f*0.7f;
    }

    /* ── Smooth toward targets ── */
    float dir_diff = w->target_angle - w->dir_angle;
    /* wrap diff to [-PI, PI] */
    while(dir_diff >  (float)M_PI) dir_diff -= 2.0f*(float)M_PI;
    while(dir_diff < -(float)M_PI) dir_diff += 2.0f*(float)M_PI;
    w->dir_angle  += dir_diff  * dt * 0.4f;
    w->strength   += (w->target_strength - w->strength) * dt * 0.3f;

    /* ── Gust spike ── */
    w->gust_cooldown -= dt;
    if(w->gust_cooldown <= 0.0f){
        w->gust = 0.3f + (float)(rand()%70)/100.0f;
        w->gust_timer    = 1.5f + (float)(rand()%30)/10.0f;
        w->gust_cooldown = 15.0f + (float)(rand()%120)/10.0f;
    }
    if(w->gust_timer > 0.0f){
        w->gust_timer -= dt;
        if(w->gust_timer <= 0.0f) w->gust = 0.0f;
    }
    w->gust = clampf(w->gust - dt*0.25f, 0.0f, 1.0f);  /* decay */

    w->total = clampf(w->strength + w->gust, 0.0f, 1.0f);
    w->wx    = cosf(w->dir_angle);
    w->wz    = sinf(w->dir_angle);

    /* Snow accumulation is weather-driven in the main loop.
       Do not ramp it from total play time or the whole world
       gradually washes out even on clear/rainy days. */
    (void)total_time;
}

/* ── Water Interaction ──────────────────────────────────────────
   A high-resolution disc mesh (~80×80 grid) that sits just above
   WATER_LEVEL and follows the player.  The vertex shader adds:

     1. Background swell  – same sin-wave motion as the base water plane
     2. Player depression – a smooth bowl that sinks under the player's feet
     3. Ring ripple       – an outward-travelling wave front generated from
                            the player's movement velocity; the faster you
                            walk, the taller the ring.

   The disc is drawn with additive-ish blending ON TOP of the existing
   semi-transparent water quad, so both layers coexist.  It is only
   rendered when the player is within WINT_ACTIVE_H of WATER_LEVEL.   */

#define WINT_SEGS        80       /* grid subdivisions per axis           */
#define WINT_RADIUS      22.0f    /* world-unit radius of the disc        */
#define WINT_ACTIVE_H    1.8f     /* height range above water to be active*/

typedef struct {
    GLuint prog,vao,vbo,ebo;
    int    ic;
    /* ripple ring state */
    float  ring_age;      /* seconds since last big splash              */
    float  ring_radius;   /* current ring expansion radius              */
    float  ring_strength; /* decays over time                          */
    Vec3   prev_player;   /* to compute velocity                       */
} WaterInteraction;

static void wint_init(WaterInteraction *wi){
    memset(wi,0,sizeof(*wi));
    wi->prev_player=(Vec3){1e9f,0,1e9f};

    /* Build a flat WINT_SEGS×WINT_SEGS grid in [-1..1]×[-1..1].
       The vertex shader scales it to WINT_RADIUS and translates to
       the player's XZ.  We store only (u,v) → world coords come from
       uPlayerPos + scaled (u,v).                                      */
    int vsz=WINT_SEGS+1;
    int vc=vsz*vsz;
    int ic=WINT_SEGS*WINT_SEGS*6;
    float   *verts=malloc((size_t)vc *2*sizeof(float));
    uint32_t *inds =malloc((size_t)ic *sizeof(uint32_t));

    for(int z=0;z<vsz;z++)for(int x=0;x<vsz;x++){
        int i=z*vsz+x;
        verts[i*2+0]=(float)x/(float)WINT_SEGS; /* u: 0..1 */
        verts[i*2+1]=(float)z/(float)WINT_SEGS; /* v: 0..1 */
    }
    int idx=0;
    for(int z=0;z<WINT_SEGS;z++)for(int x=0;x<WINT_SEGS;x++){
        uint32_t tl=(uint32_t)(z*vsz+x),tr=tl+1,
                 bl=tl+(uint32_t)vsz,   br=bl+1;
        inds[idx++]=tl;inds[idx++]=bl;inds[idx++]=tr;
        inds[idx++]=tr;inds[idx++]=bl;inds[idx++]=br;
    }
    wi->ic=ic;

    glGenVertexArrays(1,&wi->vao);
    glBindVertexArray(wi->vao);

    glGenBuffers(1,&wi->vbo);
    glBindBuffer(GL_ARRAY_BUFFER,wi->vbo);
    glBufferData(GL_ARRAY_BUFFER,(size_t)vc*2*sizeof(float),verts,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);

    glGenBuffers(1,&wi->ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,wi->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,(size_t)ic*sizeof(uint32_t),inds,GL_STATIC_DRAW);

    glBindVertexArray(0);
    free(verts); free(inds);

    wi->prog=GLH_program(
        /* ── VERTEX ───────────────────────────────────────────── */
        "#version 330 core\n"
        "layout(location=0)in vec2 aUV;\n"   /* 0..1 grid coords */
        "uniform mat4 uProj,uView;\n"
        "uniform float uTime,uRingR,uRingStr,uWaterLevel;\n"
        "uniform vec3 uPlayerPos;\n"
        "out vec3 vPos;\n"
        "out float vFoam;\n"
        "void main(){\n"
        /* map uv 0..1 → local offset -1..1 scaled by radius */
        "    vec2 local=(aUV*2.0-1.0)*" STRINGIZE(WINT_RADIUS) ";\n"
        "    vec3 p=vec3(uPlayerPos.x+local.x, uWaterLevel, uPlayerPos.z+local.y);\n"
        /* ── 1. background swell (same phase as base water) ── */
        "    p.y+=sin(p.x*0.4+uTime*1.2)*0.08+sin(p.z*0.3+uTime*0.9)*0.06;\n"
        /* ── 2. player depression: smooth bowl ── */
        "    float pd=length(local);\n"
        "    float depr=exp(-pd*pd*0.18)*0.16;\n"
        "    p.y-=depr;\n"
        /* ── 3. outward ring ripple ── */
        "    float rdist=abs(pd-uRingR);\n"
        "    float ring=exp(-rdist*rdist*3.5)*uRingStr;\n"
        "    p.y+=ring;\n"
        /* foam is stronger at ring crest and edge of depression */
        "    vFoam=clamp(ring*2.0+depr*0.8,0.0,1.0);\n"
        "    vPos=p;\n"
        "    gl_Position=uProj*uView*vec4(p,1.0);\n"
        "}\n",
        /* ── FRAGMENT ─────────────────────────────────────────── */
        "#version 330 core\n"
        "in vec3 vPos;\n"
        "in float vFoam;\n"
        "uniform float uTime;\n"
        "uniform vec3 uSunColor,uAmbient,uCamPos,uSunDir;\n"
        "out vec4 FragColor;\n"
        "void main(){\n"
        /* fake normal from neighbour derivatives */
        "    vec3 dx=dFdx(vPos), dz=dFdy(vPos);\n"
        "    vec3 n=normalize(cross(dz,dx));\n"
        "    if(n.y<0.0)n=-n;\n"
        /* water base colour */
        "    vec3 base=vec3(0.06,0.22,0.52);\n"
        /* simple diffuse + specular */
        "    float diff=max(dot(n,-uSunDir),0.0);\n"
        "    vec3 col=base*(uAmbient+uSunColor*diff*0.9);\n"
        "    vec3 view=normalize(uCamPos-vPos);\n"
        "    vec3 refl=reflect(uSunDir,n);\n"
        "    col+=vec3(0.9)*pow(max(dot(refl,view),0.0),48.0)*0.6;\n"
        /* foam brightens the ring crest */
        "    col=mix(col,vec3(0.88,0.93,0.98),vFoam*0.35);\n"
        /* fade disc edges so it blends seamlessly with the base water */
        "    float edgeDist=length(vPos.xz-uCamPos.xz);\n"  /* reuse cam xz as player xz */
        "    float edge=clamp(1.0-edgeDist/" STRINGIZE(WINT_RADIUS) ",0.0,1.0);\n"
        "    float alpha=clamp(edge*4.0,0.0,1.0)*(0.18+vFoam*0.18);\n"
        "    FragColor=vec4(col,alpha);\n"
        "}\n");
}

static void wint_update(WaterInteraction *wi, Vec3 player, float dt){
    /* velocity magnitude */
    float dx=player.x-wi->prev_player.x, dz=player.z-wi->prev_player.z;
    float speed=sqrtf(dx*dx+dz*dz)/(dt>0.0001f?dt:0.0001f);
    wi->prev_player=player;

    /* spawn / refresh ring when moving fast enough in water */
    float _wlv=wzone_local_level(player.x,player.z);
    bool in_water=(player.y <= _wlv+WINT_ACTIVE_H);
    if(in_water && speed>1.5f){
        float impact=clampf(speed/PLAYER_SPEED,0.0f,1.0f);
        if(wi->ring_radius>WINT_RADIUS*0.6f || wi->ring_strength<0.05f){
            /* restart a new ring */
            wi->ring_radius=0.3f;
            wi->ring_strength=0.18f+0.28f*impact;
            wi->ring_age=0.0f;
        }
    }

    /* advance ring outward */
    wi->ring_age+=dt;
    wi->ring_radius+=dt*7.5f;                           /* expansion speed */
    wi->ring_strength*=expf(-dt*1.8f);                  /* decay            */
    if(wi->ring_radius>WINT_RADIUS) wi->ring_strength=0.0f;
}

static void wint_draw(WaterInteraction *wi,LightingSystem *ls,
                      Mat4 proj,Mat4 view,Vec3 player,Vec3 cam,float time){
    float _wlvd=wzone_local_level(player.x,player.z);
    if(player.y > _wlvd+WINT_ACTIVE_H) return;
    if(wi->ring_strength<0.02f) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glUseProgram(wi->prog);
    glUniformMatrix4fv(glGetUniformLocation(wi->prog,"uProj"),1,GL_FALSE,proj.m);
    glUniformMatrix4fv(glGetUniformLocation(wi->prog,"uView"),1,GL_FALSE,view.m);
    glUniform1f(glGetUniformLocation(wi->prog,"uTime"),time);
    glUniform3f(glGetUniformLocation(wi->prog,"uPlayerPos"),player.x,player.y,player.z);
    glUniform1f(glGetUniformLocation(wi->prog,"uWaterLevel"),_wlvd);
    glUniform1f(glGetUniformLocation(wi->prog,"uRingR"),wi->ring_radius);
    glUniform1f(glGetUniformLocation(wi->prog,"uRingStr"),wi->ring_strength);
    glUniform3f(glGetUniformLocation(wi->prog,"uSunColor"),
                ls->sun_color.x,ls->sun_color.y,ls->sun_color.z);
    glUniform3f(glGetUniformLocation(wi->prog,"uAmbient"),
                ls->ambient.x,ls->ambient.y,ls->ambient.z);
    glUniform3f(glGetUniformLocation(wi->prog,"uSunDir"),
                ls->sun_dir.x,ls->sun_dir.y,ls->sun_dir.z);
    glUniform3f(glGetUniformLocation(wi->prog,"uCamPos"),cam.x,cam.y,cam.z);

    glBindVertexArray(wi->vao);
    glDrawElements(GL_TRIANGLES,wi->ic,GL_UNSIGNED_INT,0);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

static void wint_free(WaterInteraction *wi){
    glDeleteVertexArrays(1,&wi->vao);
    glDeleteBuffers(1,&wi->vbo);
    glDeleteBuffers(1,&wi->ebo);
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
        "  if(dot(aWeights,vec4(1))<0.05)sk=mat4(1);\n"
        "  gl_Position=uLightVP*uModel*sk*vec4(aPos,1);}\n",
        "#version 330 core\nvoid main(){}\n");
    ls->shadow_tex=sm->tex;
    printf("[Shadow] %dx%d shadow map created\n",SHADOW_SIZE,SHADOW_SIZE);
}

/* range  = half-extent of the ortho frustum (world units)
   height = how far above center the light sits             */
static void shadow_begin_range(ShadowMap *sm,LightingSystem *ls,Vec3 center,
                               float range,float height,Mat4 *out_vp){
    Vec3 sun_up={-ls->sun_dir.x,-ls->sun_dir.y,-ls->sun_dir.z};
    if(sun_up.y<0.15f)sun_up.y=0.15f;
    sun_up=vec3_norm(sun_up);
    Vec3 lp=vec3_add(center,vec3_scale(sun_up,height));
    Mat4 lv=mat4_lookat(lp,center,(Vec3){0,1,0});
    Mat4 lproj=mat4_ortho(-range,range,-range,range,1.0f,height*2.2f+50.0f);
    Mat4 lvp=mat4_mul(lproj,lv);
    /* Texel snapping: project world origin through lightVP, measure sub-texel
       drift, nudge ortho translation to cancel it.  Without this the shadow
       texel grid swims as the player moves -> circular acne pattern. */
    {
        float sz=(float)SHADOW_SIZE;
        float ox=lvp.m[0]*0.0f+lvp.m[4]*0.0f+lvp.m[8]*0.0f+lvp.m[12];
        float oy=lvp.m[1]*0.0f+lvp.m[5]*0.0f+lvp.m[9]*0.0f+lvp.m[13];
        float tx=ox*0.5f*sz; float ty=oy*0.5f*sz;
        float rx=tx-floorf(tx); float ry=ty-floorf(ty);
        lvp.m[12]+=-rx*(2.0f/sz);
        lvp.m[13]+=-ry*(2.0f/sz);
    }
    *out_vp=lvp;
    glBindFramebuffer(GL_FRAMEBUFFER,sm->fbo);
    glViewport(0,0,SHADOW_SIZE,SHADOW_SIZE);
    glClear(GL_DEPTH_BUFFER_BIT);
    /* Increased offset: slope-scale 3→4, constant 6→8 to kill shadow acne
       on flat terrain with low-angle sun without visible Peter Panning.    */
    glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(4.0f,8.0f);
    glCullFace(GL_FRONT);
}
/* Convenience wrappers */
static void shadow_begin(ShadowMap *sm,LightingSystem *ls,Vec3 center){
    /* Near cascade: ±80u at 2048px = 0.078u/texel — very high res, kills acne */
    shadow_begin_range(sm,ls,center,80.0f,120.0f,&ls->light_vp);
}
static void shadow_begin_wide(ShadowMap *sm,LightingSystem *ls,Vec3 center){
    /* Wide cascade: ±550u, covers far terrain */
    shadow_begin_range(sm,ls,center,550.0f,600.0f,&ls->light_vp_wide);
    ls->shadow_tex_wide=sm->tex;
}
static void shadow_end(ShadowMap *sm,int win_w,int win_h){
    (void)sm;
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glViewport(0,0,win_w,win_h);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glCullFace(GL_BACK);
}

/* Render terrain into shadow map — draw all visible chunks at LOD1 */
static void shadow_terrain(ShadowMap *sm,LightingSystem *ls,Terrain *t){
    glUseProgram(sm->prog_depth);
    Mat4 id=mat4_identity();
    glUniformMatrix4fv(glGetUniformLocation(sm->prog_depth,"uLightVP"),1,GL_FALSE,ls->light_vp.m);
    glUniformMatrix4fv(glGetUniformLocation(sm->prog_depth,"uModel"),1,GL_FALSE,id.m);
    /* Use LOD1 (half-res) for shadow map — good quality, lower cost */
    for(int i=0;i<TCNX*TCNZ;i++){
        TerrainChunk *ch=&t->chunks[i];
        glBindVertexArray(ch->vao[1]);
        glDrawElements(GL_TRIANGLES,ch->ic[1],GL_UNSIGNED_INT,0);
    }
    glBindVertexArray(0);
}


/* ── Collision ──────────────────────────────────────────────── */
typedef struct{Vec3 min,max;}AABB;
typedef struct{AABB boxes[MAX_COLL_BOXES];int count;}CollWorld; /* kept for terrain */

/* Per-triangle mesh collision for village buildings */
#define MAX_VILLAGE_TRIS 131072
typedef struct{Vec3 a,b,c;Vec3 n;}CTri; /* collision triangle + face normal */
typedef struct{CTri *tris;int count,cap;}TriMesh;
static Vec3 mat4_transform_point(Mat4 m, Vec3 v);
static Mat4 mat4_from_cgltf(const cgltf_float *m16);
static const cgltf_attribute *gltf_find_attribute(const cgltf_primitive *prim,cgltf_attribute_type type,int index);

static void trimesh_init(TriMesh *tm){tm->tris=NULL;tm->count=0;tm->cap=0;}
static void trimesh_free(TriMesh *tm){free(tm->tris);tm->tris=NULL;tm->count=0;tm->cap=0;}

/* Add a triangle in LOCAL space. The village draw/coll transform is applied later. */
static void trimesh_add(TriMesh *tm,Vec3 a,Vec3 b,Vec3 c){
    if(tm->count>=tm->cap){
        tm->cap=tm->cap?tm->cap*2:4096;
        tm->tris=realloc(tm->tris,tm->cap*sizeof(CTri));
    }
    CTri *t=&tm->tris[tm->count++];
    t->a=a;t->b=b;t->c=c;
    /* Face normal */
    Vec3 ab=vec3_sub(b,a),ac=vec3_sub(c,a);
    t->n=vec3_norm((Vec3){ab.y*ac.z-ab.z*ac.y, ab.z*ac.x-ab.x*ac.z, ab.x*ac.y-ab.y*ac.x});
}

/* Build TriMesh from an FBX scene (local vertex_position, triangulated) */
static void trimesh_build(TriMesh *tm,ufbx_scene *scene){
    trimesh_init(tm);
    size_t ntri_buf=64;uint32_t *tri=malloc(ntri_buf*sizeof(uint32_t));
    for(size_t ni=0;ni<scene->nodes.count;ni++){
        ufbx_node *nd=scene->nodes.data[ni];if(!nd->mesh)continue;
        ufbx_mesh *mesh=nd->mesh;
        /* Apply geometry_to_world so axis conversion (Z-up→Y-up) is baked in */
        ufbx_matrix g2w=nd->geometry_to_world;
        if(mesh->max_face_triangles*3>ntri_buf){ntri_buf=mesh->max_face_triangles*3;tri=realloc(tri,ntri_buf*sizeof(uint32_t));}
        for(size_t fi=0;fi<mesh->num_faces;fi++){
            ufbx_face face=mesh->faces.data[fi];
            size_t nt=ufbx_triangulate_face(tri,ntri_buf,mesh,face);
            for(size_t ti=0;ti<nt;ti++){
                uint32_t i0=tri[ti*3],i1=tri[ti*3+1],i2=tri[ti*3+2];
                ufbx_vec3 lp0=ufbx_get_vertex_vec3(&mesh->vertex_position,i0);
                ufbx_vec3 lp1=ufbx_get_vertex_vec3(&mesh->vertex_position,i1);
                ufbx_vec3 lp2=ufbx_get_vertex_vec3(&mesh->vertex_position,i2);
                ufbx_vec3 p0=ufbx_transform_position(&g2w,lp0);
                ufbx_vec3 p1=ufbx_transform_position(&g2w,lp1);
                ufbx_vec3 p2=ufbx_transform_position(&g2w,lp2);
                trimesh_add(tm,(Vec3){(float)p0.x,(float)p0.y,(float)p0.z},
                               (Vec3){(float)p1.x,(float)p1.y,(float)p1.z},
                               (Vec3){(float)p2.x,(float)p2.y,(float)p2.z});
            }
        }
    }
    free(tri);
    printf("[TriMesh] %d triangles\n",tm->count);
}

static void trimesh_build_gltf(TriMesh *tm,cgltf_data *data){
    trimesh_init(tm);
    if(!data)return;
    for(size_t ni=0;ni<data->nodes_count;ni++){
        cgltf_node *nd=&data->nodes[ni];
        if(!nd->mesh)continue;
        cgltf_float world[16];
        cgltf_node_transform_world(nd,world);
        Mat4 wm=mat4_from_cgltf(world);
        for(size_t pi=0;pi<nd->mesh->primitives_count;pi++){
            cgltf_primitive *prim=&nd->mesh->primitives[pi];
            const cgltf_attribute *pos_attr=gltf_find_attribute(prim,cgltf_attribute_type_position,0);
            if(!pos_attr||!pos_attr->data)continue;
            size_t icount=prim->indices?prim->indices->count:pos_attr->data->count;
            for(size_t i=0;i+2<icount;i+=3){
                uint32_t i0=prim->indices?(uint32_t)cgltf_accessor_read_index(prim->indices,i):(uint32_t)i;
                uint32_t i1=prim->indices?(uint32_t)cgltf_accessor_read_index(prim->indices,i+1):(uint32_t)(i+1);
                uint32_t i2=prim->indices?(uint32_t)cgltf_accessor_read_index(prim->indices,i+2):(uint32_t)(i+2);
                cgltf_float p0[3]={0},p1[3]={0},p2[3]={0};
                cgltf_accessor_read_float(pos_attr->data,i0,p0,3);
                cgltf_accessor_read_float(pos_attr->data,i1,p1,3);
                cgltf_accessor_read_float(pos_attr->data,i2,p2,3);
                trimesh_add(tm,
                    mat4_transform_point(wm,(Vec3){(float)p0[0],(float)p0[1],(float)p0[2]}),
                    mat4_transform_point(wm,(Vec3){(float)p1[0],(float)p1[1],(float)p1[2]}),
                    mat4_transform_point(wm,(Vec3){(float)p2[0],(float)p2[1],(float)p2[2]}));
            }
        }
    }
    printf("[TriMesh] %d triangles\n",tm->count);
}

/* Apply village transform (Y-flip + translate) to all triangles in place */
static Vec3 mat4_transform_point(Mat4 m, Vec3 v){
    return (Vec3){
        m.m[0]*v.x+m.m[4]*v.y+m.m[8]*v.z+m.m[12],
        m.m[1]*v.x+m.m[5]*v.y+m.m[9]*v.z+m.m[13],
        m.m[2]*v.x+m.m[6]*v.y+m.m[10]*v.z+m.m[14]
    };
}
static void trimesh_apply_transform(TriMesh *tm, Mat4 world){
    /* Apply prop_world matrix directly — geometry_to_world already baked
       into verts by trimesh_build, so just transform to world space.    */
    for(int i=0;i<tm->count;i++){
        CTri *t=&tm->tris[i];
        t->a=mat4_transform_point(world,t->a);
        t->b=mat4_transform_point(world,t->b);
        t->c=mat4_transform_point(world,t->c);
        Vec3 ab=vec3_sub(t->b,t->a),ac=vec3_sub(t->c,t->a);
        t->n=vec3_norm((Vec3){ab.y*ac.z-ab.z*ac.y, ab.z*ac.x-ab.x*ac.z, ab.x*ac.y-ab.y*ac.x});
    }
}

/* Closest point on line segment to point P (2D XZ) */
static float seg_dist_xz(float px,float pz,float ax,float az,float bx,float bz,float *nx,float *nz){
    float abx=bx-ax,abz=bz-az,len2=abx*abx+abz*abz;
    if(len2<1e-6f){*nx=px-ax;*nz=pz-az;return sqrtf(*nx**nx+*nz**nz);}
    float t=clampf(((px-ax)*abx+(pz-az)*abz)/len2,0,1);
    *nx=px-(ax+t*abx); *nz=pz-(az+t*abz);
    float d=sqrtf(*nx**nx+*nz**nz);
    return d;
}

/* Resolve player cylinder against all triangles in the mesh */
static bool trimesh_resolve(TriMesh *tm,Vec3 *pos,float radius,float height){
    float py=pos->y, ptop=py+height;

    /* ── Pass 1: floor/ramp triangles — snap player on top ───────
       For upward-facing tris (ny > 0.7), if the player's foot is
       slightly below the surface and their XZ is inside the tri,
       push them up onto it (like terrain_h does for the ground).  */
    float best_floor_y=-1e9f;
    bool  on_floor=false;
    for(int i=0;i<tm->count;i++){
        CTri *t=&tm->tris[i];
        if(t->n.y<0.3f)continue; /* upward-facing — 0.3 catches stairs/ramps */
        float tmax=fmaxf(t->a.y,fmaxf(t->b.y,t->c.y));
        /* Player foot must be near or below the triangle top */
        if(py>tmax+1.5f)continue;
        float tmin=fminf(t->a.y,fminf(t->b.y,t->c.y));
        if(py<tmin-2.0f)continue;
        /* XZ point-in-triangle test */
        float px=pos->x,pz=pos->z;
        float d0=(px-t->a.x)*(t->b.z-t->a.z)-(pz-t->a.z)*(t->b.x-t->a.x);
        float d1=(px-t->b.x)*(t->c.z-t->b.z)-(pz-t->b.z)*(t->c.x-t->b.x);
        float d2=(px-t->c.x)*(t->a.z-t->c.z)-(pz-t->c.z)*(t->a.x-t->c.x);
        if(!((d0>=0&&d1>=0&&d2>=0)||(d0<=0&&d1<=0&&d2<=0)))continue;
        /* Interpolate Y on the triangle plane at (px,pz) */
        float floor_y;
        if(fabsf(t->n.y)>1e-4f){
            float D=t->n.x*t->a.x+t->n.y*t->a.y+t->n.z*t->a.z;
            floor_y=(D-t->n.x*px-t->n.z*pz)/t->n.y;
        } else {
            floor_y=tmax;
        }
        if(floor_y>best_floor_y){best_floor_y=floor_y;on_floor=true;}
    }
    if(on_floor&&py<best_floor_y+1.5f&&py>best_floor_y-2.0f){
        pos->y=best_floor_y;
    }

    /* ── Pass 2: wall triangles — XZ push-out ────────────────── */
    py=pos->y; ptop=py+height;
    for(int i=0;i<tm->count;i++){
        CTri *t=&tm->tris[i];
        if(t->n.y>0.3f||t->n.y<-0.3f)continue; /* skip floors/ramps/ceilings */
        float tmin=fminf(t->a.y,fminf(t->b.y,t->c.y));
        float tmax=fmaxf(t->a.y,fmaxf(t->b.y,t->c.y));
        if(py>tmax+0.05f||ptop<tmin-0.05f)continue;
        float px=pos->x,pz=pos->z;
        float verts[3][2]={{t->a.x,t->a.z},{t->b.x,t->b.z},{t->c.x,t->c.z}};
        float d0=(px-verts[0][0])*(verts[1][1]-verts[0][1])-(pz-verts[0][1])*(verts[1][0]-verts[0][0]);
        float d1=(px-verts[1][0])*(verts[2][1]-verts[1][1])-(pz-verts[1][1])*(verts[2][0]-verts[1][0]);
        float d2=(px-verts[2][0])*(verts[0][1]-verts[2][1])-(pz-verts[2][1])*(verts[0][0]-verts[2][0]);
        bool inside=(d0>=0&&d1>=0&&d2>=0)||(d0<=0&&d1<=0&&d2<=0);
        if(inside){
            float nl=sqrtf(t->n.x*t->n.x+t->n.z*t->n.z);
            if(nl>0.01f){pos->x+=t->n.x/nl*radius;pos->z+=t->n.z/nl*radius;}
            continue;
        }
        for(int e=0;e<3;e++){
            float ax=verts[e][0],az=verts[e][1],bx=verts[(e+1)%3][0],bz=verts[(e+1)%3][1];
            float nx,nz; float d=seg_dist_xz(px,pz,ax,az,bx,bz,&nx,&nz);
            if(d<radius&&d>1e-5f){float pen=radius-d;pos->x+=nx/d*pen;pos->z+=nz/d*pen;}
        }
    }
    return on_floor;
}

/* CollWorld kept for debug_draw signature compatibility */


/* ── GPU Mesh / Scene ───────────────────────────────────────── */
typedef struct{float pos[3],normal[3],uv[2];float weights[MAX_WEIGHTS];int32_t bones[MAX_WEIGHTS];float tangent[3],bitangent[3];}Vertex;
typedef struct{
    GLuint vao;
    GLsizei index_count;
    int32_t cluster_map[MAX_BONES],num_gpu_bones;
    Mat4 geometry_to_world;
    uint32_t node_typed_id;
    GLuint texture,normal_tex,mr_tex,emissive_tex;
    float mat_color[4],emissive[3],metallic,roughness,alpha_cutoff;
    bool has_pbr;
}GpuMesh;
typedef enum{SCENE_FORMAT_NONE=0,SCENE_FORMAT_UFBX,SCENE_FORMAT_GLTF}SceneFormat;
typedef struct{float x,y,z,w;}Quat;
typedef struct{Vec3 t,s;Quat r;}GltfNodePose;
typedef struct{
    int joint_count;
    int joint_nodes[MAX_BONES];
    Mat4 inverse_bind[MAX_BONES];
}GltfSkinInfo;
typedef struct{
    const cgltf_image *image;
    bool srgb;
    GLuint tex;
}GltfTexCacheEntry;
typedef struct{
    ufbx_scene *scene;
    cgltf_data *gltf;
    SceneFormat format;
    GpuMesh meshes[MAX_MESHES];
    int num_meshes;
    ufbx_skin_deformer *skins[MAX_MESHES];
    int gltf_mesh_skin[MAX_MESHES];
    int gltf_node_count;
    GltfNodePose *gltf_bind_pose;
    Mat4 *gltf_rest_world;
    GltfNodePose *gltf_eval_pose;
    Mat4 *gltf_eval_world;
    uint8_t *gltf_eval_mark;
    GltfSkinInfo *gltf_skins;
    int gltf_skin_count;
    GltfTexCacheEntry *gltf_tex_cache;
    int gltf_tex_cache_count;
    int gltf_tex_cache_cap;
    float model_scale,cx,cy,cz,ground_offset,ymin_raw,ymax_raw;
}Scene;
typedef struct{int idle,walk,run,attack,attack2,jump,death,roll,block,hit;}AnimMap;

typedef struct {
    ufbx_vec3 *normals;
    uint32_t *indices;
    size_t num_normals;
} MeshNormals;

static bool path_contains_ci(const char *path,const char *needle);
static void path_normalize_slashes(char *path);
static bool path_has_extension(const char *path);
static bool path_has_extension_ci(const char *path,const char *ext);
static bool scene_has_data(const Scene *s);
static int scene_anim_count(const Scene *s);
static float scene_anim_duration(const Scene *s,int anim_index);
static int scene_find_anim(const Scene *s,const char *kw);
static Mat4 mat4_from_cgltf(const cgltf_float *m16);
static const cgltf_attribute *gltf_find_attribute(const cgltf_primitive *prim,cgltf_attribute_type type,int index);
static double anim_stack_real_end(ufbx_anim_stack *stk);
static float gltf_anim_duration(const cgltf_animation *anim);

static bool path_has_extension_ci(const char *path,const char *ext){
    if(!path||!ext)return false;
    const char *dot=strrchr(path,'.');
    if(!dot)return false;
    while(*dot&&*ext){
        if(tolower((unsigned char)*dot)!=tolower((unsigned char)*ext))return false;
        dot++;ext++;
    }
    return *dot==0&&*ext==0;
}

static bool scene_has_data(const Scene *s){
    return s&&s->format!=SCENE_FORMAT_NONE&&s->num_meshes>0;
}

static int scene_anim_count(const Scene *s){
    if(!s)return 0;
    if(s->format==SCENE_FORMAT_UFBX&&s->scene)return (int)s->scene->anim_stacks.count;
    if(s->format==SCENE_FORMAT_GLTF&&s->gltf)return (int)s->gltf->animations_count;
    return 0;
}

static float scene_anim_duration(const Scene *s,int anim_index){
    if(!s||anim_index<0)return 0.0f;
    if(s->format==SCENE_FORMAT_UFBX&&s->scene&&anim_index<(int)s->scene->anim_stacks.count){
        ufbx_anim_stack *stk=s->scene->anim_stacks.data[anim_index];
        return (float)(anim_stack_real_end(stk)-stk->time_begin);
    }
    if(s->format==SCENE_FORMAT_GLTF&&s->gltf&&anim_index<(int)s->gltf->animations_count){
        return gltf_anim_duration(&s->gltf->animations[anim_index]);
    }
    return 0.0f;
}

static int scene_find_anim(const Scene *s,const char *kw){
    if(!s||!kw||!kw[0])return -1;
    if(s->format==SCENE_FORMAT_UFBX&&s->scene){
        for(size_t i=0;i<s->scene->anim_stacks.count;i++){
            const char *n=s->scene->anim_stacks.data[i]->name.data;
            for(size_t si=0;n[si];si++){size_t ki=0;while(kw[ki]&&tolower((unsigned char)n[si+ki])==tolower((unsigned char)kw[ki]))ki++;if(!kw[ki])return (int)i;}
        }
        return -1;
    }
    if(s->format==SCENE_FORMAT_GLTF&&s->gltf){
        for(size_t i=0;i<s->gltf->animations_count;i++){
            const char *n=s->gltf->animations[i].name?s->gltf->animations[i].name:"";
            for(size_t si=0;n[si];si++){size_t ki=0;while(kw[ki]&&tolower((unsigned char)n[si+ki])==tolower((unsigned char)kw[ki]))ki++;if(!kw[ki])return (int)i;}
        }
    }
    return -1;
}

static MeshNormals mesh_normals_build(ufbx_mesh *mesh){
    MeshNormals mn={0};
    if(mesh->num_indices==0)return mn;
    ufbx_topo_edge *topo=malloc(mesh->num_indices*sizeof(ufbx_topo_edge));
    uint32_t *normal_indices=malloc(mesh->num_indices*sizeof(uint32_t));
    if(!topo||!normal_indices){free(topo);free(normal_indices);return mn;}
    ufbx_compute_topology(mesh,topo,mesh->num_indices);
    size_t num_normals=ufbx_generate_normal_mapping(mesh,topo,mesh->num_indices,normal_indices,mesh->num_indices,false);
    free(topo);
    if(num_normals==0){free(normal_indices);return mn;}
    ufbx_vec3 *normals=malloc(num_normals*sizeof(ufbx_vec3));
    if(!normals){free(normal_indices);return mn;}
    ufbx_compute_normals(mesh,&mesh->vertex_position,normal_indices,mesh->num_indices,normals,num_normals);
    mn.normals=normals;
    mn.indices=normal_indices;
    mn.num_normals=num_normals;
    return mn;
}

static inline ufbx_vec3 mesh_normal_get(const MeshNormals *mn, ufbx_mesh *mesh, uint32_t ix){
    if(mn->normals&&mn->indices&&ix<mesh->num_indices){
        uint32_t ni=mn->indices[ix];
        if(ni<mn->num_normals)return mn->normals[ni];
    }
    if(mesh->vertex_normal.exists)return ufbx_get_vertex_vec3(&mesh->vertex_normal,ix);
    return (ufbx_vec3){0.0,1.0,0.0};
}

static void mesh_normals_free(MeshNormals *mn){
    free(mn->normals);
    free(mn->indices);
    memset(mn,0,sizeof(*mn));
}

static float uv_stream_score(const ufbx_mesh *mesh,const ufbx_vertex_vec2 *uvs){
    if(!mesh||!uvs||!uvs->exists||mesh->num_indices==0)return -1.0f;
    float umin=1e30f,vmin=1e30f,umax=-1e30f,vmax=-1e30f;
    for(size_t ix=0;ix<mesh->num_indices;ix++){
        ufbx_vec2 uv=ufbx_get_vertex_vec2(uvs,ix);
        if((float)uv.x<umin)umin=(float)uv.x;
        if((float)uv.y<vmin)vmin=(float)uv.y;
        if((float)uv.x>umax)umax=(float)uv.x;
        if((float)uv.y>vmax)vmax=(float)uv.y;
    }
    float du=umax-umin,dv=vmax-vmin;
    if(du<1e-5f&&dv<1e-5f)return 0.0f;
    return du*dv;
}

static const ufbx_uv_set *mesh_preferred_uv_set(const ufbx_mesh *mesh){
    if(!mesh)return NULL;
    const ufbx_uv_set *best=NULL;
    float best_score=-1.0f;
    for(size_t i=0;i<mesh->uv_sets.count;i++){
        const ufbx_uv_set *uv=&mesh->uv_sets.data[i];
        float score=uv_stream_score(mesh,&uv->vertex_uv);
        if(score<=0.0f)continue;
        if(path_contains_ci(uv->name.data,"UVMap"))score+=1000.0f;
        if(score>best_score){best=uv;best_score=score;}
    }
    return best;
}

static const ufbx_vertex_vec2 *mesh_preferred_uv(const ufbx_mesh *mesh){
    if(mesh&&mesh->vertex_uv.exists&&uv_stream_score(mesh,&mesh->vertex_uv)>0.0f)return &mesh->vertex_uv;
    const ufbx_uv_set *set=mesh_preferred_uv_set(mesh);
    if(set)return &set->vertex_uv;
    return NULL;
}

static const ufbx_vertex_vec3 *mesh_preferred_tangent(const ufbx_mesh *mesh){
    if(mesh&&mesh->vertex_tangent.exists&&uv_stream_score(mesh,&mesh->vertex_uv)>0.0f)return &mesh->vertex_tangent;
    const ufbx_uv_set *set=mesh_preferred_uv_set(mesh);
    if(set&&set->vertex_tangent.exists)return &set->vertex_tangent;
    return NULL;
}

static const ufbx_vertex_vec3 *mesh_preferred_bitangent(const ufbx_mesh *mesh){
    if(mesh&&mesh->vertex_bitangent.exists&&uv_stream_score(mesh,&mesh->vertex_uv)>0.0f)return &mesh->vertex_bitangent;
    const ufbx_uv_set *set=mesh_preferred_uv_set(mesh);
    if(set&&set->vertex_bitangent.exists)return &set->vertex_bitangent;
    return NULL;
}

static const char *mesh_preferred_uv_name(const ufbx_mesh *mesh){
    if(!mesh)return "<none>";
    if(mesh->vertex_uv.exists&&uv_stream_score(mesh,&mesh->vertex_uv)>0.0f)return "<vertex_uv>";
    const ufbx_uv_set *set=mesh_preferred_uv_set(mesh);
    if(set)return set->name.data;
    return "<none>";
}

static bool texture_path_pick_candidate(char *dst,size_t cap,const char *path){
    char candidate[1024];
    const char *exts[]={"png","jpg","jpeg","tga","bmp",NULL};
    if(!dst||cap==0||!path||!path[0])return false;
    dst[0]=0;
    snprintf(candidate,sizeof(candidate),"%s",path);
    path_normalize_slashes(candidate);
    if(access(candidate,F_OK)==0){snprintf(dst,cap,"%s",candidate);return true;}
    const char *base=strrchr(candidate,'/');
    if(base&&base[1]&&access(base+1,F_OK)==0){snprintf(dst,cap,"%s",base+1);return true;}
    if(path_has_extension(candidate))return false;
    for(int ei=0;exts[ei];ei++){
        char try_path[1100];
        snprintf(try_path,sizeof(try_path),"%s.%s",candidate,exts[ei]);
        if(access(try_path,F_OK)==0){snprintf(dst,cap,"%s",try_path);return true;}
        if(base&&base[1]){
            char try_base[600];
            snprintf(try_base,sizeof(try_base),"%s.%s",base+1,exts[ei]);
            if(access(try_base,F_OK)==0){snprintf(dst,cap,"%s",try_base);return true;}
        }
    }
    return false;
}

/* Forward declarations — defined after GPU mesh code below */
static bool scene_load(Scene *s, const char *path);
static void scene_free(Scene *s);
/* Fixed logical animation slot indices — AnimMap fields always equal these */
#define ANIM_IDLE    0
#define ANIM_WALK    1
#define ANIM_RUN     2
#define ANIM_ATTACK  3
#define ANIM_ATTACK2 4
#define ANIM_JUMP    5
#define ANIM_DEATH   6
#define ANIM_ROLL    7
#define ANIM_BLOCK   8
#define ANIM_HIT     9
#define ANIM_SLOTS   10

/* One resolved animation: which scene file + which stack within it */
typedef struct { Scene *scene; int stack; } AnimEntry;

/* A character model: base mesh/skeleton + per-slot animation resolution.
   Works for BOTH single-FBX (multiple stacks) AND multi-FBX (one anim per file). */
#define CHARMODEL_MAX_EXTRA 12   /* max separately-loaded animation FBX files */
typedef struct {
    Scene      base;                      /* mesh, skeleton, scale info */
    AnimEntry  anims[ANIM_SLOTS];         /* resolved [scene, stack] per slot */
    Scene      extra[CHARMODEL_MAX_EXTRA];/* storage for extra per-anim FBX files */
    int        extra_count;
    bool       owns_base;                 /* false = base is a shallow copy, don't free */
} CharModel;

static void charmodel_init(CharModel *cm, const char *base_fbx){
    memset(cm,0,sizeof(*cm));
    cm->owns_base=true;
    if(!scene_load(&cm->base,base_fbx)){
        fprintf(stderr,"[CharModel] Failed to load base: %s\n",base_fbx);
        return;
    }
    /* Slot names for keyword search AND per-file fallback suffix */
    static const char *slot_kws[ANIM_SLOTS]={
        "idle","walk","run","attack","attack2","jump","death","roll","block","hit"
    };
    /* Build base stem for per-file probing: strip extension */
    char stem[512];
    strncpy(stem,base_fbx,sizeof(stem)-1);stem[sizeof(stem)-1]=0;
    char *dot=strrchr(stem,'.');if(dot)*dot=0;   /* e.g. "models/knight" */

    /* First pass: fill from base scene stacks */
    for(int s=0;s<ANIM_SLOTS;s++){
        int idx=scene_find_anim(&cm->base,slot_kws[s]);
        /* attack2 fallback keywords */
        if(idx<0&&s==ANIM_ATTACK2)idx=scene_find_anim(&cm->base,"kick");
        if(idx<0&&s==ANIM_ATTACK2)idx=scene_find_anim(&cm->base,"slash2");
        /* run fallback */
        if(idx<0&&s==ANIM_RUN)    idx=scene_find_anim(&cm->base,"sprint");
        /* death fallback */
        if(idx<0&&s==ANIM_DEATH)  idx=scene_find_anim(&cm->base,"die");
        /* roll fallback */
        if(idx<0&&s==ANIM_ROLL)   idx=scene_find_anim(&cm->base,"dodge");
        /* block fallback */
        if(idx<0&&s==ANIM_BLOCK)  idx=scene_find_anim(&cm->base,"guard");
        /* hit fallback */
        if(idx<0&&s==ANIM_HIT)    idx=scene_find_anim(&cm->base,"hurt");

        if(idx>=0){
            cm->anims[s].scene=&cm->base;
            cm->anims[s].stack=idx;
        }
        /* else: will probe for separate file in second pass */
    }

    /* Second pass: for any slot still missing, probe {stem}_{slotname}.fbx
       only for ufbx assets. glTF keeps all clips in the base scene. */
    if(cm->base.format==SCENE_FORMAT_UFBX){
        for(int s=0;s<ANIM_SLOTS;s++){
            if(cm->anims[s].scene)continue; /* already resolved */
            if(cm->extra_count>=CHARMODEL_MAX_EXTRA)break;
            char probe[600];
            snprintf(probe,sizeof(probe),"%s_%s.fbx",stem,slot_kws[s]);
            if(scene_load(&cm->extra[cm->extra_count],probe)){
                cm->anims[s].scene=&cm->extra[cm->extra_count];
                cm->anims[s].stack=0;  /* single-stack per-anim FBX */
                cm->extra_count++;
                printf("[CharModel] Loaded separate anim: %s → slot %d\n",probe,s);
            }
        }
    }

    /* Third pass: fill remaining missing slots with fallback chain */
    /* idle → 0th stack guaranteed */
    if(!cm->anims[ANIM_IDLE].scene){
        cm->anims[ANIM_IDLE].scene=&cm->base;
        cm->anims[ANIM_IDLE].stack=scene_anim_count(&cm->base)>0?0:-1;
    }
    for(int s=0;s<ANIM_SLOTS;s++){
        if(!cm->anims[s].scene){
            /* Fallback chain */
            int fb=ANIM_IDLE;
            if(s==ANIM_WALK||s==ANIM_RUN)  fb=ANIM_IDLE;
            if(s==ANIM_ATTACK2)             fb=ANIM_ATTACK;
            if(s==ANIM_JUMP||s==ANIM_ROLL)  fb=ANIM_IDLE;
            if(s==ANIM_BLOCK)               fb=ANIM_IDLE;
            if(s==ANIM_HIT)                 fb=ANIM_IDLE;
            if(s==ANIM_DEATH)               fb=ANIM_IDLE;
            cm->anims[s]=cm->anims[fb];
        }
    }
}

/* Shallow-copy another model's base (for fallback enemy reuse of player mesh) */
static void charmodel_share_base(CharModel *dst, CharModel *src){
    memset(dst,0,sizeof(*dst));
    dst->base=src->base;
    dst->owns_base=false;
    /* Copy all anim entries pointing at src's base/extras — they stay valid */
    for(int s=0;s<ANIM_SLOTS;s++) dst->anims[s]=src->anims[s];
}

static void charmodel_free(CharModel *cm){
    if(cm->owns_base) scene_free(&cm->base);
    for(int i=0;i<cm->extra_count;i++) scene_free(&cm->extra[i]);
    memset(cm,0,sizeof(*cm));
}

static void path_normalize_slashes(char *path){
    if(!path)return;
    for(;*path;path++) if(*path=='\\') *path='/';
}

static bool path_has_extension(const char *path){
    if(!path||!path[0])return false;
    const char *slash1=strrchr(path,'/');
    const char *slash2=strrchr(path,'\\');
    const char *slash=slash1>slash2?slash1:slash2;
    const char *dot=strrchr(path,'.');
    return dot&&(!slash||dot>slash)&&dot[1];
}

static void ufbx_string_copy(char *dst,size_t cap,ufbx_string str){
    if(!dst||cap==0)return;
    size_t n=str.length<cap-1?str.length:cap-1;
    memcpy(dst,str.data,n);
    dst[n]=0;
}

static void texture_path_resolve(char *dst,size_t cap,const char *model_path,ufbx_string rel){
    char rel_path[512]={0};
    char model_dir[1024]={0};
    const char *slash;
    if(!dst||cap==0)return;
    dst[0]=0;
    if(rel.length==0)return;

    ufbx_string_copy(rel_path,sizeof(rel_path),rel);
    path_normalize_slashes(rel_path);
    if(rel_path[0]=='/'||(rel_path[0]&&rel_path[1]==':')){
        snprintf(dst,cap,"%s",rel_path);
        return;
    }

    snprintf(model_dir,sizeof(model_dir),"%s",model_path?model_path:"");
    path_normalize_slashes(model_dir);
    slash=strrchr(model_dir,'/');
    if(slash){
        size_t dir_len=(size_t)(slash-model_dir+1);
        snprintf(dst,cap,"%.*s%s",(int)dir_len,model_dir,rel_path);
    }else{
        snprintf(dst,cap,"%s",rel_path);
    }
}

static GLuint texture_file_try_candidates(const char *path){
    char candidate[1024];
    const char *exts[]={"png","jpg","jpeg","tga","bmp",NULL};
    GLuint id=0;
    if(!path||!path[0])return 0;

    snprintf(candidate,sizeof(candidate),"%s",path);
    path_normalize_slashes(candidate);
    if(access(candidate,F_OK)==0){
        id=GLH_texture_file_srgb(candidate);
        if(id)return id;
    }

    const char *base=strrchr(candidate,'/');
    if(base&&base[1]&&access(base+1,F_OK)==0){
        id=GLH_texture_file_srgb(base+1);
        if(id)return id;
    }

    if(path_has_extension(candidate))return 0;

    for(int ei=0;exts[ei];ei++){
        char try_path[1100];
        snprintf(try_path,sizeof(try_path),"%s.%s",candidate,exts[ei]);
        if(access(try_path,F_OK)==0){
            id=GLH_texture_file_srgb(try_path);
            if(id)return id;
        }
        if(base&&base[1]){
            char try_base[600];
            snprintf(try_base,sizeof(try_base),"%s.%s",base+1,exts[ei]);
            if(access(try_base,F_OK)==0){
                id=GLH_texture_file_srgb(try_base);
                if(id)return id;
            }
        }
    }
    return 0;
}

static GLuint texture_file_try_candidates_mode(const char *path,bool srgb){
    char candidate[1024];
    const char *exts[]={"png","jpg","jpeg","tga","bmp",NULL};
    GLuint id=0;
    if(!path||!path[0])return 0;

    snprintf(candidate,sizeof(candidate),"%s",path);
    path_normalize_slashes(candidate);
    if(access(candidate,F_OK)==0){
        id=srgb?GLH_texture_file_srgb(candidate):GLH_texture_file(candidate);
        if(id)return id;
    }

    const char *base=strrchr(candidate,'/');
    if(base&&base[1]&&access(base+1,F_OK)==0){
        id=srgb?GLH_texture_file_srgb(base+1):GLH_texture_file(base+1);
        if(id)return id;
    }

    if(path_has_extension(candidate))return 0;

    for(int ei=0;exts[ei];ei++){
        char try_path[1100];
        snprintf(try_path,sizeof(try_path),"%s.%s",candidate,exts[ei]);
        if(access(try_path,F_OK)==0){
            id=srgb?GLH_texture_file_srgb(try_path):GLH_texture_file(try_path);
            if(id)return id;
        }
        if(base&&base[1]){
            char try_base[600];
            snprintf(try_base,sizeof(try_base),"%s.%s",base+1,exts[ei]);
            if(access(try_base,F_OK)==0){
                id=srgb?GLH_texture_file_srgb(try_base):GLH_texture_file(try_base);
                if(id)return id;
            }
        }
    }
    return 0;
}

static bool path_contains_ci(const char *path,const char *needle){
    if(!path||!needle||!*needle)return false;
    size_t nlen=strlen(needle);
    for(const char *p=path;*p;p++){
        size_t i=0;
        while(i<nlen&&p[i]&&tolower((unsigned char)p[i])==tolower((unsigned char)needle[i]))i++;
        if(i==nlen)return true;
    }
    return false;
}

static const char *path_basename_ptr(const char *path){
    if(!path)return "";
    const char *s1=strrchr(path,'/');
    const char *s2=strrchr(path,'\\');
    const char *s=s1>s2?s1:s2;
    return s?s+1:path;
}

static bool is_direct_knight_asset(const char *path){
    const char *base=path_basename_ptr(path);
    return path_contains_ci(base,"knight.fbx")||path_contains_ci(base,"knight (copy).fbx");
}

static bool is_knight_copy_asset(const char *path){
    const char *base=path_basename_ptr(path);
    return path_contains_ci(base,"knight (copy).fbx");
}

static GLuint try_texture_candidates(const char *const *candidates,size_t count){
    for(size_t i=0;i<count;i++){
        GLuint id=texture_file_try_candidates(candidates[i]);
        if(id)return id;
    }
    return 0;
}

static void ufbx_string_to_cstr(char *dst,size_t cap,ufbx_string str){
    if(!dst||cap==0)return;
    size_t n=str.length<cap-1?str.length:cap-1;
    memcpy(dst,str.data,n);
    dst[n]=0;
}

static ufbx_material *mesh_material_at(ufbx_mesh *mesh,ufbx_node *node,size_t index){
    if(node&&index<node->materials.count){
        ufbx_material *mat=node->materials.data[index];
        if(mat)return mat;
    }
    if(mesh&&index<mesh->materials.count)return mesh->materials.data[index];
    return NULL;
}

static GLuint get_knight_override_tex(ufbx_node *node){
    char node_name[256]={0};
    const char *const plates[]={
        "textures/knight_plates_BaseColor.png",
        "models/characters/knight_3/textures(4)/textures/knight_plates_BaseColor.png",
    };
    const char *const leather[]={
        "textures/knight_leather_BaseColor.png",
        "models/characters/knight_3/textures(4)/textures/knight_leather_BaseColor.png",
    };
    const char *const chainmail[]={
        "textures/knight_chainmail_BaseColor.png",
        "models/characters/knight_3/textures(4)/textures/knight_chainmail_BaseColor.png",
    };
    const char *const belts[]={
        "models/characters/knight_3/textures(4)/textures/belts_belts_BaseColor.png",
    };
    const char *const sword[]={
        "models/characters/knight_3/textures(4)/textures/ZWEIHANDER_ZWEIHANDER_BaseColor.png",
        "models/characters/knight_3/cav_textures/weapons/sword_long_Flattened_Diffuse_COLOR.jpg",
    };
    const char *const shield[]={
        "models/characters/knight_3/cav_textures/weapons/Crest_shield_Crest_shield_BaseColor.png",
    };
    ufbx_string_copy(node_name,sizeof(node_name),node->name);
    if(path_contains_ci(node_name,"sword")||path_contains_ci(node_name,"zweihander")) return try_texture_candidates(sword,sizeof(sword)/sizeof(sword[0]));
    if(path_contains_ci(node_name,"shield")||path_contains_ci(node_name,"crest")) return try_texture_candidates(shield,sizeof(shield)/sizeof(shield[0]));
    if(path_contains_ci(node_name,"belt")) return try_texture_candidates(belts,sizeof(belts)/sizeof(belts[0]));
    if(path_contains_ci(node_name,"leather")) return try_texture_candidates(leather,sizeof(leather)/sizeof(leather[0]));
    if(path_contains_ci(node_name,"chain")) return try_texture_candidates(chainmail,sizeof(chainmail)/sizeof(chainmail[0]));
    return try_texture_candidates(plates,sizeof(plates)/sizeof(plates[0]));
}

static GLuint get_character_override_tex(ufbx_node *node,ufbx_material *mat,const char *path){
    char node_name[256]={0},mat_name[256]={0},base_name[256]={0};
    const char *base=path_basename_ptr(path);
    if(node)ufbx_string_to_cstr(node_name,sizeof(node_name),node->name);
    if(mat) ufbx_string_to_cstr(mat_name,sizeof(mat_name),mat->name);
    snprintf(base_name,sizeof(base_name),"%s",base);

    if(path_contains_ci(base_name,"npc.fbx")){
        const char *const npc_tex[]={"knight-tex.jpg"};
        return try_texture_candidates(npc_tex,sizeof(npc_tex)/sizeof(npc_tex[0]));
    }

    if(is_direct_knight_asset(path)&&!is_knight_copy_asset(path)){
        const char *const sword[]={"textures/ZWEIHANDER_ZWEIHANDER_BaseColor.png","models/characters/knight_3/cav_textures/weapons/sword_long_Flattened_Diffuse_COLOR.jpg"};
        const char *const belts[]={"textures/belts_belts_BaseColor.png","models/characters/knight_3/textures(4)/textures/belts_belts_BaseColor.png"};
        const char *const leather[]={"textures/knight_leather_BaseColor.png","models/characters/knight_3/textures(4)/textures/knight_leather_BaseColor.png"};
        const char *const chainmail[]={"textures/knight_chainmail_BaseColor.png","models/characters/knight_3/textures(4)/textures/knight_chainmail_BaseColor.png"};
        const char *const plates[]={"textures/knight_plates_BaseColor.png","models/characters/knight_3/textures(4)/textures/knight_plates_BaseColor.png"};
        if(path_contains_ci(node_name,"sword")||path_contains_ci(node_name,"zweihander")||path_contains_ci(mat_name,"zweihander")) return try_texture_candidates(sword,sizeof(sword)/sizeof(sword[0]));
        if(path_contains_ci(node_name,"belt")||path_contains_ci(mat_name,"belt")) return try_texture_candidates(belts,sizeof(belts)/sizeof(belts[0]));
        if(path_contains_ci(node_name,"leather")||path_contains_ci(mat_name,"leather")) return try_texture_candidates(leather,sizeof(leather)/sizeof(leather[0]));
        if(path_contains_ci(node_name,"chain")||path_contains_ci(mat_name,"chain")||path_contains_ci(mat_name,"head")) return try_texture_candidates(chainmail,sizeof(chainmail)/sizeof(chainmail[0]));
        return try_texture_candidates(plates,sizeof(plates)/sizeof(plates[0]));
    }

    if(path_contains_ci(base_name,"dwarf_")){
        char dwarf_id[64]="";
        if(path_contains_ci(base_name,"dwarf_broddi")) snprintf(dwarf_id,sizeof(dwarf_id),"Dwarf_Broddi");
        else if(path_contains_ci(base_name,"dwarf_king")) snprintf(dwarf_id,sizeof(dwarf_id),"Dwarf_King");
        else if(path_contains_ci(base_name,"dwarf_agvid")) snprintf(dwarf_id,sizeof(dwarf_id),"Dwarf_Agvid");
        else if(path_contains_ci(base_name,"dwarf_bedvar")) snprintf(dwarf_id,sizeof(dwarf_id),"Dwarf_Bedvar");
        if(dwarf_id[0]){
            const char *slot="Body";
            if(path_contains_ci(node_name,"weapon")||path_contains_ci(node_name,"forge")||path_contains_ci(mat_name,"weapon")) slot="Weapon";
            else if(path_contains_ci(node_name,"details")||path_contains_ci(mat_name,"details")) slot="Details";
            else if(path_contains_ci(node_name,"beard")||path_contains_ci(node_name,"mustache")||path_contains_ci(mat_name,"beard")) slot="Beard";
            else if(path_contains_ci(node_name,"head")||path_contains_ci(node_name,"hair")||path_contains_ci(node_name,"eyes")||
                    path_contains_ci(node_name,"eyebrow")||path_contains_ci(node_name,"jaw")||path_contains_ci(mat_name,"head")||
                    path_contains_ci(mat_name,"hair")) slot="Head";
            char tex_name[160];
            char tex_path1[256],tex_path2[256];
            snprintf(tex_name,sizeof(tex_name),"%s_%s_Albedo.tga",dwarf_id,slot);
            snprintf(tex_path1,sizeof(tex_path1),"textures/%s",tex_name);
            snprintf(tex_path2,sizeof(tex_path2),"models/characters/dawrves/textures/%s",tex_name);
            const char *cands[]={tex_path1,tex_path2};
            return try_texture_candidates(cands,sizeof(cands)/sizeof(cands[0]));
        }
    }
    return 0;
}

static bool resolve_mesh_texture_debug_path(ufbx_mesh *mesh,ufbx_node *node,const char *path,size_t material_index,char *resolved,size_t resolved_cap){
    if(!resolved||resolved_cap==0)return false;
    resolved[0]=0;
    if(is_direct_knight_asset(path)){
        char node_name[256]={0};
        ufbx_string_copy(node_name,sizeof(node_name),node->name);
        const char *guess=
            (path_contains_ci(node_name,"sword")||path_contains_ci(node_name,"zweihander")) ? "models/characters/knight_3/textures(4)/textures/ZWEIHANDER_ZWEIHANDER_BaseColor.png" :
            (path_contains_ci(node_name,"shield")||path_contains_ci(node_name,"crest")) ? "models/characters/knight_3/cav_textures/weapons/Crest_shield_Crest_shield_BaseColor.png" :
            path_contains_ci(node_name,"belt") ? "models/characters/knight_3/textures(4)/textures/belts_belts_BaseColor.png" :
            path_contains_ci(node_name,"leather") ? "textures/knight_leather_BaseColor.png" :
            path_contains_ci(node_name,"chain") ? "textures/knight_chainmail_BaseColor.png" :
            "textures/knight_plates_BaseColor.png";
        if(texture_path_pick_candidate(resolved,resolved_cap,guess)) return true;
    }
    if(mesh&&mesh->materials.count>0){
        ufbx_material *mat=mesh_material_at(mesh,node,material_index);
        if(mat){
            ufbx_texture *tex=mat->pbr.base_color.texture;if(!tex)tex=mat->fbx.diffuse_color.texture;
            if(tex){
                if(tex->relative_filename.length>0){
                    char full[1024];
                    texture_path_resolve(full,sizeof(full),path,tex->relative_filename);
                    if(texture_path_pick_candidate(resolved,resolved_cap,full)) return true;
                }else if(tex->content.size>4){
                    snprintf(resolved,resolved_cap,"<embedded>");
                    return true;
                }
            }
        }
    }
    return false;
}

static void debug_print_mesh_binding(ufbx_mesh *mesh,ufbx_node *node,const char *path,const ufbx_vertex_vec2 *uv_stream){
    if(!(path_contains_ci(path,"knight")||path_contains_ci(path,"platemail")||path_contains_ci(path,"character")))return;
    double umin=1e30,vmin=1e30,umax=-1e30,vmax=-1e30;
    if(uv_stream&&uv_stream->exists){
        for(size_t ix=0;ix<mesh->num_indices;ix++){
            ufbx_vec2 uv=ufbx_get_vertex_vec2(uv_stream,ix);
            if(uv.x<umin)umin=uv.x; if(uv.y<vmin)vmin=uv.y;
            if(uv.x>umax)umax=uv.x; if(uv.y>vmax)vmax=uv.y;
        }
    }else{
        umin=vmin=umax=vmax=0.0;
    }
    char resolved[1024]={0};
    const char *chosen="<none>";
    int ch=0;
    bool alpha=false;
    if(resolve_mesh_texture_debug_path(mesh,node,path,0,resolved,sizeof(resolved))) chosen=resolved;
    if(chosen[0]&&chosen[0]!='<'){
        int w=0,h=0;
        if(stbi_info(chosen,&w,&h,&ch)) alpha=(ch==4);
    }
    printf("[MeshDebug] model='%s' node='%s' uv='%s' uv_range=[%.3f..%.3f, %.3f..%.3f] tex='%s' ch=%d alpha=%s mats=%zu uv_sets=%zu\n",
           path,node->name.data,mesh_preferred_uv_name(mesh),umin,umax,vmin,vmax,chosen,ch,alpha?"yes":"no",mesh->materials.count,mesh->uv_sets.count);
}

/* AnimMap now always maps to fixed indices 0-9 */
static void animmap_init(AnimMap *am, ufbx_scene *sc){
    (void)sc; /* no longer needed — CharModel handles resolution */
    am->idle=ANIM_IDLE; am->walk=ANIM_WALK; am->run=ANIM_RUN;
    am->attack=ANIM_ATTACK; am->attack2=ANIM_ATTACK2; am->jump=ANIM_JUMP;
    am->death=ANIM_DEATH; am->roll=ANIM_ROLL; am->block=ANIM_BLOCK;
    am->hit=ANIM_HIT;
}


static GLuint get_mesh_tex(ufbx_mesh *mesh,ufbx_node *node,const char *path,size_t material_index){
    ufbx_material *mat=mesh_material_at(mesh,node,material_index);
    if(mat){
        ufbx_texture *tex=mat->pbr.base_color.texture;if(!tex)tex=mat->fbx.diffuse_color.texture;
        if(tex){
            if(tex->content.size>4){GLuint id=GLH_texture_mem_srgb(tex->content.data,(int)tex->content.size);if(id)return id;}
            if(tex->relative_filename.length>0){
                char full[1024];
                texture_path_resolve(full,sizeof(full),path,tex->relative_filename);
                GLuint id=texture_file_try_candidates(full);if(id)return id;
            }
        }
    }
    return get_character_override_tex(node,mat,path);
}

static GLuint load_texture_from_map(const ufbx_material_map *map,const char *path,bool srgb){
    if(!map||!map->texture_enabled||!map->texture)return 0;
    ufbx_texture *tex=map->texture;
    if(tex->content.size>4){
        return srgb ? GLH_texture_mem_srgb(tex->content.data,(int)tex->content.size)
                    : GLH_texture_mem(tex->content.data,(int)tex->content.size);
    }
    if(tex->relative_filename.length>0){
        char full[1024];
        texture_path_resolve(full,sizeof(full),path,tex->relative_filename);
        return texture_file_try_candidates_mode(full,srgb);
    }
    return 0;
}

static void gpu_mesh_apply_pbr(GpuMesh *gm,ufbx_material *mat,const char *path){
    gm->mat_color[0]=0.75f;gm->mat_color[1]=0.72f;gm->mat_color[2]=0.68f;gm->mat_color[3]=1.0f;
    gm->emissive[0]=gm->emissive[1]=gm->emissive[2]=0.0f;
    gm->metallic=0.0f;
    gm->roughness=0.85f;
    gm->alpha_cutoff=0.33f;
    gm->has_pbr=false;
    if(!mat)return;

    gm->normal_tex=load_texture_from_map(&mat->pbr.normal_map,path,false);
    gm->mr_tex=load_texture_from_map(&mat->pbr.roughness,path,false);
    if(!gm->mr_tex)gm->mr_tex=load_texture_from_map(&mat->pbr.metalness,path,false);
    gm->emissive_tex=load_texture_from_map(&mat->pbr.emission_color,path,true);

    if(mat->pbr.base_color.value_components>=4){
        gm->mat_color[0]=(float)mat->pbr.base_color.value_vec4.x;
        gm->mat_color[1]=(float)mat->pbr.base_color.value_vec4.y;
        gm->mat_color[2]=(float)mat->pbr.base_color.value_vec4.z;
        gm->mat_color[3]=(float)mat->pbr.base_color.value_vec4.w;
    }else{
        ufbx_vec3 c=mat->pbr.base_color.value_vec3;
        if(c.x+c.y+c.z<0.01f)c=mat->fbx.diffuse_color.value_vec3;
        if(c.x+c.y+c.z>0.01f){
            gm->mat_color[0]=(float)c.x;
            gm->mat_color[1]=(float)c.y;
            gm->mat_color[2]=(float)c.z;
        }
    }
    if(mat->pbr.emission_color.value_components>=3){
        gm->emissive[0]=(float)mat->pbr.emission_color.value_vec3.x;
        gm->emissive[1]=(float)mat->pbr.emission_color.value_vec3.y;
        gm->emissive[2]=(float)mat->pbr.emission_color.value_vec3.z;
    }
    if(mat->pbr.metalness.has_value)gm->metallic=clampf((float)mat->pbr.metalness.value_real,0.0f,1.0f);
    if(mat->pbr.roughness.has_value)gm->roughness=clampf((float)mat->pbr.roughness.value_real,0.04f,1.0f);
    if(mat->pbr.opacity.has_value){
        gm->mat_color[3]=clampf((float)mat->pbr.opacity.value_real,0.0f,1.0f);
    }
    gm->has_pbr=(gm->normal_tex||gm->mr_tex||gm->emissive_tex||mat->pbr.base_color.has_value||mat->pbr.metalness.has_value||mat->pbr.roughness.has_value);
}

static Mat4 mat4_from_cgltf(const cgltf_float *m16){
    Mat4 m=mat4_identity();
    if(!m16)return m;
    for(int i=0;i<16;i++)m.m[i]=(float)m16[i];
    return m;
}

static Quat quat_identity(void){return (Quat){0,0,0,1};}
static Quat quat_norm(Quat q){float l=sqrtf(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);if(l<1e-8f)return quat_identity();float il=1.0f/l;return (Quat){q.x*il,q.y*il,q.z*il,q.w*il};}
static Quat quat_nlerp(Quat a,Quat b,float t){
    float dot=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
    if(dot<0.0f){b.x=-b.x;b.y=-b.y;b.z=-b.z;b.w=-b.w;}
    Quat q={a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t};
    return quat_norm(q);
}
static Mat4 mat4_from_quat(Quat q){
    q=quat_norm(q);
    float xx=q.x*q.x,yy=q.y*q.y,zz=q.z*q.z;
    float xy=q.x*q.y,xz=q.x*q.z,yz=q.y*q.z;
    float wx=q.w*q.x,wy=q.w*q.y,wz=q.w*q.z;
    Mat4 m=mat4_identity();
    m.m[0]=1.0f-2.0f*(yy+zz); m.m[4]=2.0f*(xy-wz);     m.m[8]=2.0f*(xz+wy);
    m.m[1]=2.0f*(xy+wz);     m.m[5]=1.0f-2.0f*(xx+zz); m.m[9]=2.0f*(yz-wx);
    m.m[2]=2.0f*(xz-wy);     m.m[6]=2.0f*(yz+wx);     m.m[10]=1.0f-2.0f*(xx+yy);
    return m;
}
static Mat4 mat4_from_trs(Vec3 t,Quat r,Vec3 s){
    Mat4 m=mat4_from_quat(r);
    m.m[0]*=s.x;m.m[1]*=s.x;m.m[2]*=s.x;
    m.m[4]*=s.y;m.m[5]*=s.y;m.m[6]*=s.y;
    m.m[8]*=s.z;m.m[9]*=s.z;m.m[10]*=s.z;
    m.m[12]=t.x;m.m[13]=t.y;m.m[14]=t.z;
    return m;
}
static GltfNodePose gltf_node_bind_pose(const cgltf_node *node){
    GltfNodePose p={0};
    p.t=(Vec3){0,0,0};
    p.s=(Vec3){1,1,1};
    p.r=quat_identity();
    if(node->has_translation)p.t=(Vec3){(float)node->translation[0],(float)node->translation[1],(float)node->translation[2]};
    if(node->has_scale)p.s=(Vec3){(float)node->scale[0],(float)node->scale[1],(float)node->scale[2]};
    if(node->has_rotation)p.r=(Quat){(float)node->rotation[0],(float)node->rotation[1],(float)node->rotation[2],(float)node->rotation[3]};
    if(node->has_matrix){
        cgltf_float m[16]; for(int i=0;i<16;i++)m[i]=node->matrix[i];
        p.t=(Vec3){(float)m[12],(float)m[13],(float)m[14]};
    }
    return p;
}
static int gltf_node_index(const Scene *s,const cgltf_node *node){
    if(!s||!s->gltf||!node)return -1;
    return (int)(node-s->gltf->nodes);
}
static float gltf_anim_duration(const cgltf_animation *anim){
    float end=0.0f;
    if(!anim)return 0.0f;
    for(size_t ci=0;ci<anim->channels_count;ci++){
        cgltf_animation_sampler *sam=anim->channels[ci].sampler;
        if(!sam||!sam->input||sam->input->count==0)continue;
        cgltf_float v=0.0f;
        cgltf_accessor_read_float(sam->input,sam->input->count-1,&v,1);
        if(v>end)end=(float)v;
    }
    return end;
}
static void gltf_sample_channel_pose(const cgltf_animation_channel *ch,float t,GltfNodePose *pose){
    if(!ch||!ch->sampler||!ch->sampler->input||!ch->sampler->output||!pose)return;
    cgltf_accessor *tin=ch->sampler->input,*tout=ch->sampler->output;
    if(tin->count==0||tout->count==0)return;
    size_t count=tin->count;
    size_t i0=0,i1=0;
    cgltf_float tt=0.0f;
    cgltf_accessor_read_float(tin,0,&tt,1);
    if(t<=tt){i0=i1=0;}
    else{
        i0=count-1;i1=count-1;
        for(size_t i=1;i<count;i++){
            cgltf_float cur=0.0f;
            cgltf_accessor_read_float(tin,i,&cur,1);
            if(t<=cur){i0=i-1;i1=i;break;}
        }
    }
    cgltf_float t0=0.0f,t1=0.0f;
    cgltf_accessor_read_float(tin,i0,&t0,1);
    cgltf_accessor_read_float(tin,i1,&t1,1);
    float a=(i0==i1||ch->sampler->interpolation==cgltf_interpolation_type_step||fabsf((float)(t1-t0))<1e-6f)?0.0f:clampf((t-(float)t0)/((float)t1-(float)t0),0.0f,1.0f);
    if(ch->sampler->interpolation==cgltf_interpolation_type_cubic_spline)a=0.0f;
    if(ch->target_path==cgltf_animation_path_type_translation||ch->target_path==cgltf_animation_path_type_scale){
        cgltf_float va[3]={0},vb[3]={0};
        cgltf_accessor_read_float(tout,i0,va,3);cgltf_accessor_read_float(tout,i1,vb,3);
        Vec3 out={(float)(va[0]+(vb[0]-va[0])*a),(float)(va[1]+(vb[1]-va[1])*a),(float)(va[2]+(vb[2]-va[2])*a)};
        if(ch->target_path==cgltf_animation_path_type_translation)pose->t=out; else pose->s=out;
    }else if(ch->target_path==cgltf_animation_path_type_rotation){
        cgltf_float va[4]={0,0,0,1},vb[4]={0,0,0,1};
        cgltf_accessor_read_float(tout,i0,va,4);cgltf_accessor_read_float(tout,i1,vb,4);
        pose->r=quat_nlerp((Quat){(float)va[0],(float)va[1],(float)va[2],(float)va[3]},(Quat){(float)vb[0],(float)vb[1],(float)vb[2],(float)vb[3]},a);
    }
}
static Mat4 gltf_eval_world_recursive(Scene *s,int node_index){
    if(node_index<0||node_index>=s->gltf_node_count)return mat4_identity();
    if(s->gltf_eval_mark[node_index]==2)return s->gltf_eval_world[node_index];
    if(s->gltf_eval_mark[node_index]==1)return s->gltf_eval_world[node_index];
    s->gltf_eval_mark[node_index]=1;
    cgltf_node *node=&s->gltf->nodes[node_index];
    Mat4 local=mat4_from_trs(s->gltf_eval_pose[node_index].t,s->gltf_eval_pose[node_index].r,s->gltf_eval_pose[node_index].s);
    if(node->parent){
        int parent_index=gltf_node_index(s,node->parent);
        s->gltf_eval_world[node_index]=mat4_mul(gltf_eval_world_recursive(s,parent_index),local);
    }else{
        s->gltf_eval_world[node_index]=local;
    }
    s->gltf_eval_mark[node_index]=2;
    return s->gltf_eval_world[node_index];
}
static void scene_gltf_evaluate(Scene *s,int anim_index,double at){
    if(!s||s->format!=SCENE_FORMAT_GLTF||!s->gltf||s->gltf_node_count<=0)return;
    for(int i=0;i<s->gltf_node_count;i++)s->gltf_eval_pose[i]=s->gltf_bind_pose[i];
    if(anim_index>=0&&anim_index<(int)s->gltf->animations_count){
        cgltf_animation *anim=&s->gltf->animations[anim_index];
        float dur=gltf_anim_duration(anim);
        float t=dur>0.0f?fmodf((float)at,dur):0.0f;
        for(size_t ci=0;ci<anim->channels_count;ci++){
            cgltf_animation_channel *ch=&anim->channels[ci];
            if(!ch->target_node||ch->target_path==cgltf_animation_path_type_weights)continue;
            int node_index=gltf_node_index(s,ch->target_node);
            if(node_index<0||node_index>=s->gltf_node_count)continue;
            gltf_sample_channel_pose(ch,t,&s->gltf_eval_pose[node_index]);
        }
    }
    memset(s->gltf_eval_mark,0,(size_t)s->gltf_node_count*sizeof(*s->gltf_eval_mark));
    for(int i=0;i<s->gltf_node_count;i++)gltf_eval_world_recursive(s,i);
}
static void compute_bones_gltf(Mat4 *out,int nb,const GpuMesh *gm,const Scene *sc){
    for(int i=0;i<nb;i++)out[i]=mat4_identity();
    if(!sc||sc->format!=SCENE_FORMAT_GLTF||!sc->gltf_skins)return;
    int skin_index=gm->cluster_map[0];
    if(skin_index<0||skin_index>=sc->gltf_skin_count)return;
    const GltfSkinInfo *skin=&sc->gltf_skins[skin_index];
    int count=skin->joint_count<nb?skin->joint_count:nb;
    for(int i=0;i<count;i++){
        int node_index=skin->joint_nodes[i];
        if(node_index<0||node_index>=sc->gltf_node_count)continue;
        out[i]=mat4_mul(sc->gltf_eval_world[node_index],skin->inverse_bind[i]);
    }
}

static const cgltf_attribute *gltf_find_attribute(const cgltf_primitive *prim,cgltf_attribute_type type,int index){
    if(!prim)return NULL;
    for(size_t i=0;i<prim->attributes_count;i++){
        const cgltf_attribute *attr=&prim->attributes[i];
        if(attr->type==type&&attr->index==index&&attr->data)return attr;
    }
    return NULL;
}

static void model_dir_from_path(char *dst,size_t cap,const char *path){
    if(!dst||cap==0)return;
    dst[0]=0;
    if(!path||!path[0])return;
    snprintf(dst,cap,"%s",path);
    path_normalize_slashes(dst);
    char *slash=strrchr(dst,'/');
    if(slash)slash[1]=0;
    else dst[0]=0;
}

static bool gltf_texture_info_ok_memory(const void *data,int size,const char *label){
    if(!data||size<=4)return false;
    int w=0,h=0,ch=0;
    if(!stbi_info_from_memory((const stbi_uc*)data,size,&w,&h,&ch))return true;
    if(w<=0||h<=0)return false;
    if(w>4096||h>4096||(int64_t)w*(int64_t)h>16777216LL){
        fprintf(stderr,"[glTF] Skipping oversized embedded texture %s (%dx%d)\n",label?label:"<embedded>",w,h);
        return false;
    }
    return true;
}

static bool gltf_texture_info_ok_file(const char *path){
    if(!path||!path[0])return false;
    int w=0,h=0,ch=0;
    if(!stbi_info(path,&w,&h,&ch))return true;
    if(w<=0||h<=0)return false;
    if(w>4096||h>4096||(int64_t)w*(int64_t)h>16777216LL){
        fprintf(stderr,"[glTF] Skipping oversized texture file %s (%dx%d)\n",path,w,h);
        return false;
    }
    return true;
}

static const cgltf_image *gltf_texture_image_ptr(const cgltf_texture *tex){
    if(!tex)return NULL;
    if(tex->image)return tex->image;
    if(tex->has_basisu&&tex->basisu_image)return tex->basisu_image;
    if(tex->has_webp&&tex->webp_image)return tex->webp_image;
    return NULL;
}

static GLuint gltf_texture_from_image(const cgltf_image *image,const char *path,bool srgb){
    if(!image)return 0;
    if(image->buffer_view&&image->buffer_view->buffer&&image->buffer_view->buffer->data){
        const void *data=cgltf_buffer_view_data(image->buffer_view);
        int size=(int)image->buffer_view->size;
        if(data&&size>4&&gltf_texture_info_ok_memory(data,size,image->name))return srgb?GLH_texture_mem_srgb(data,size):GLH_texture_mem(data,size);
    }
    if(image->uri&&image->uri[0]){
        if(!strncmp(image->uri,"data:",5)){
            const char *base64=strstr(image->uri,"base64,");
            if(base64){
                void *decoded=NULL;
                cgltf_size encoded_size=(cgltf_size)strlen(base64+7);
                cgltf_size decoded_size=encoded_size*3/4+4;
                cgltf_options opts={0};
                if(cgltf_load_buffer_base64(&opts,encoded_size,base64+7,&decoded)==cgltf_result_success&&decoded){
                    GLuint id=0;
                    if(gltf_texture_info_ok_memory(decoded,(int)decoded_size,image->name))id=srgb?GLH_texture_mem_srgb(decoded,(int)decoded_size):GLH_texture_mem(decoded,(int)decoded_size);
                    free(decoded);
                    return id;
                }
            }
            return 0;
        }
        char full[1024],dir[1024];
        model_dir_from_path(dir,sizeof(dir),path);
        snprintf(full,sizeof(full),"%s%s",dir,image->uri);
        path_normalize_slashes(full);
        if(!gltf_texture_info_ok_file(full))return 0;
        return texture_file_try_candidates_mode(full,srgb);
    }
    return 0;
}

static GLuint gltf_texture_from_view_cached(Scene *s,const cgltf_texture_view *view,const char *path,bool srgb){
    if(!view||!view->texture)return 0;
    const cgltf_image *image=gltf_texture_image_ptr(view->texture);
    if(image&&s){
        for(int i=0;i<s->gltf_tex_cache_count;i++){
            GltfTexCacheEntry *entry=&s->gltf_tex_cache[i];
            if(entry->image==image&&entry->srgb==srgb)return entry->tex;
        }
    }
    GLuint id=gltf_texture_from_image(image,path,srgb);
    if(id&&image&&s&&s->gltf_tex_cache&&s->gltf_tex_cache_count<s->gltf_tex_cache_cap){
        GltfTexCacheEntry *entry=&s->gltf_tex_cache[s->gltf_tex_cache_count++];
        entry->image=image;
        entry->srgb=srgb;
        entry->tex=id;
    }
    return id;
}

static bool build_gpu_mesh_gltf(Scene *s,GpuMesh *gm,const cgltf_primitive *prim,const cgltf_node *node,int node_index,int skin_index,const char *path){
    const cgltf_attribute *pos_attr=gltf_find_attribute(prim,cgltf_attribute_type_position,0);
    if(!pos_attr||!pos_attr->data||pos_attr->data->count==0)return false;
    const cgltf_attribute *nrm_attr=gltf_find_attribute(prim,cgltf_attribute_type_normal,0);
    const cgltf_attribute *uv_attr =gltf_find_attribute(prim,cgltf_attribute_type_texcoord,0);
    const cgltf_attribute *tan_attr=gltf_find_attribute(prim,cgltf_attribute_type_tangent,0);
    const cgltf_attribute *jnt_attr=gltf_find_attribute(prim,cgltf_attribute_type_joints,0);
    const cgltf_attribute *wgt_attr=gltf_find_attribute(prim,cgltf_attribute_type_weights,0);
    size_t vcount=pos_attr->data->count;
    size_t icount=prim->indices?prim->indices->count:vcount;
    if(vcount==0||icount==0)return false;
    Vertex *verts=calloc(vcount,sizeof(Vertex));
    uint32_t *inds=malloc(icount*sizeof(uint32_t));
    if(!verts||!inds){free(verts);free(inds);return false;}
    for(size_t i=0;i<vcount;i++){
        cgltf_float fv[4]={0,0,0,1};
        cgltf_uint uv[4]={0};
        Vertex *v=&verts[i];
        cgltf_accessor_read_float(pos_attr->data,i,fv,3);v->pos[0]=(float)fv[0];v->pos[1]=(float)fv[1];v->pos[2]=(float)fv[2];
        if(nrm_attr&&cgltf_accessor_read_float(nrm_attr->data,i,fv,3)){v->normal[0]=(float)fv[0];v->normal[1]=(float)fv[1];v->normal[2]=(float)fv[2];}
        else v->normal[1]=1.0f;
        if(uv_attr&&cgltf_accessor_read_float(uv_attr->data,i,fv,2)){v->uv[0]=(float)fv[0];v->uv[1]=(float)fv[1];}
        if(tan_attr&&cgltf_accessor_read_float(tan_attr->data,i,fv,4)){
            v->tangent[0]=(float)fv[0];v->tangent[1]=(float)fv[1];v->tangent[2]=(float)fv[2];
            Vec3 nn={v->normal[0],v->normal[1],v->normal[2]};
            Vec3 tt={v->tangent[0],v->tangent[1],v->tangent[2]};
            Vec3 bb=vec3_scale(vec3_norm(vec3_cross(nn,tt)),fv[3]<0.0f?-1.0f:1.0f);
            v->bitangent[0]=bb.x;v->bitangent[1]=bb.y;v->bitangent[2]=bb.z;
        }else{
            Vec3 nn={v->normal[0],v->normal[1],v->normal[2]};
            Vec3 tt=fabsf(nn.y)<0.99f?(Vec3){0,1,0}:(Vec3){1,0,0};
            tt=vec3_norm(vec3_sub(tt,vec3_scale(nn,vec3_dot(tt,nn))));
            Vec3 bb=vec3_norm(vec3_cross(nn,tt));
            v->tangent[0]=tt.x;v->tangent[1]=tt.y;v->tangent[2]=tt.z;
            v->bitangent[0]=bb.x;v->bitangent[1]=bb.y;v->bitangent[2]=bb.z;
        }
        if(jnt_attr&&wgt_attr){
            cgltf_accessor_read_uint(jnt_attr->data,i,uv,4);
            cgltf_accessor_read_float(wgt_attr->data,i,fv,4);
            float tw=0.0f;
            for(int k=0;k<MAX_WEIGHTS;k++){
                v->bones[k]=(int32_t)uv[k];
                v->weights[k]=(float)fv[k];
                tw+=v->weights[k];
            }
            if(tw>0.001f)for(int k=0;k<MAX_WEIGHTS;k++)v->weights[k]/=tw;
        }
    }
    for(size_t i=0;i<icount;i++)inds[i]=prim->indices?(uint32_t)cgltf_accessor_read_index(prim->indices,i):(uint32_t)i;
    GLH_Attrib attrs[]={{0,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,pos)},{1,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,normal)},{2,2,GL_FLOAT,GL_FALSE,offsetof(Vertex,uv)},{3,4,GL_FLOAT,GL_FALSE,offsetof(Vertex,weights)},{4,4,GL_INT,0,offsetof(Vertex,bones)},{5,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,tangent)},{6,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,bitangent)}};
    gm->vao=GLH_mesh(verts,vcount*sizeof(Vertex),inds,icount*sizeof(uint32_t),sizeof(Vertex),attrs,7);
    gm->index_count=(GLsizei)icount;
    cgltf_float world[16];
    cgltf_node_transform_world(node,world);
    gm->geometry_to_world=mat4_from_cgltf(world);
    gm->node_typed_id=(uint32_t)node_index;
    gm->cluster_map[0]=skin_index;
    gm->texture=0;
    gm->normal_tex=gm->mr_tex=gm->emissive_tex=0;
    gm->mat_color[0]=gm->mat_color[1]=gm->mat_color[2]=1.0f;gm->mat_color[3]=1.0f;
    gm->emissive[0]=gm->emissive[1]=gm->emissive[2]=0.0f;
    gm->metallic=1.0f;gm->roughness=1.0f;gm->alpha_cutoff=0.5f;gm->has_pbr=false;
    if(prim->material){
        const cgltf_material *mat=prim->material;
        gm->texture=gltf_texture_from_view_cached(s,&mat->pbr_metallic_roughness.base_color_texture,path,true);
        gm->normal_tex=gltf_texture_from_view_cached(s,&mat->normal_texture,path,false);
        gm->mr_tex=gltf_texture_from_view_cached(s,&mat->pbr_metallic_roughness.metallic_roughness_texture,path,false);
        gm->emissive_tex=gltf_texture_from_view_cached(s,&mat->emissive_texture,path,true);
        if(mat->has_pbr_metallic_roughness){
            gm->mat_color[0]=(float)mat->pbr_metallic_roughness.base_color_factor[0];
            gm->mat_color[1]=(float)mat->pbr_metallic_roughness.base_color_factor[1];
            gm->mat_color[2]=(float)mat->pbr_metallic_roughness.base_color_factor[2];
            gm->mat_color[3]=(float)mat->pbr_metallic_roughness.base_color_factor[3];
            gm->metallic=(float)mat->pbr_metallic_roughness.metallic_factor;
            gm->roughness=clampf((float)mat->pbr_metallic_roughness.roughness_factor,0.04f,1.0f);
        }
        gm->emissive[0]=(float)mat->emissive_factor[0];
        gm->emissive[1]=(float)mat->emissive_factor[1];
        gm->emissive[2]=(float)mat->emissive_factor[2];
        if(mat->alpha_mode==cgltf_alpha_mode_mask)gm->alpha_cutoff=mat->alpha_cutoff>0.0f?(float)mat->alpha_cutoff:0.5f;
        else if(mat->alpha_mode==cgltf_alpha_mode_blend)gm->alpha_cutoff=0.0f;
        gm->has_pbr=(gm->texture||gm->normal_tex||gm->mr_tex||gm->emissive_tex||mat->has_pbr_metallic_roughness);
    }
    if(jnt_attr&&wgt_attr){
        int max_bone=0;
        for(size_t i=0;i<vcount;i++)for(int k=0;k<MAX_WEIGHTS;k++)if(verts[i].weights[k]>0.0f&&verts[i].bones[k]>max_bone)max_bone=verts[i].bones[k];
        gm->num_gpu_bones=max_bone+1;
        if(gm->num_gpu_bones>MAX_BONES)gm->num_gpu_bones=MAX_BONES;
    }
    free(verts);
    free(inds);
    return true;
}

static bool scene_load_gltf(Scene *s,const char *path){
    cgltf_options opts={0};
    cgltf_result res=cgltf_parse_file(&opts,path,&s->gltf);
    if(res!=cgltf_result_success||!s->gltf){
        fprintf(stderr,"Load '%s': cgltf parse failed (%d)\n",path,(int)res);
        return false;
    }
    res=cgltf_load_buffers(&opts,s->gltf,path);
    if(res!=cgltf_result_success){
        fprintf(stderr,"Load '%s': cgltf buffers failed (%d)\n",path,(int)res);
        cgltf_free(s->gltf);s->gltf=NULL;
        return false;
    }
    s->format=SCENE_FORMAT_GLTF;
    s->gltf_node_count=(int)s->gltf->nodes_count;
    if(s->gltf_node_count>0){
        s->gltf_bind_pose=calloc((size_t)s->gltf_node_count,sizeof(*s->gltf_bind_pose));
        s->gltf_rest_world=calloc((size_t)s->gltf_node_count,sizeof(*s->gltf_rest_world));
        s->gltf_eval_pose=calloc((size_t)s->gltf_node_count,sizeof(*s->gltf_eval_pose));
        s->gltf_eval_world=calloc((size_t)s->gltf_node_count,sizeof(*s->gltf_eval_world));
        s->gltf_eval_mark=calloc((size_t)s->gltf_node_count,sizeof(*s->gltf_eval_mark));
        if(!s->gltf_bind_pose||!s->gltf_rest_world||!s->gltf_eval_pose||!s->gltf_eval_world||!s->gltf_eval_mark){
            fprintf(stderr,"Load '%s': glTF runtime alloc failed\n",path);
            return false;
        }
        for(int i=0;i<s->gltf_node_count;i++){
            s->gltf_bind_pose[i]=gltf_node_bind_pose(&s->gltf->nodes[i]);
            cgltf_float world[16];
            cgltf_node_transform_world(&s->gltf->nodes[i],world);
            s->gltf_rest_world[i]=mat4_from_cgltf(world);
            s->gltf_eval_pose[i]=s->gltf_bind_pose[i];
            s->gltf_eval_world[i]=s->gltf_rest_world[i];
        }
    }
    s->gltf_skin_count=(int)s->gltf->skins_count;
    if(s->gltf_skin_count>0){
        s->gltf_skins=calloc((size_t)s->gltf_skin_count,sizeof(*s->gltf_skins));
        if(!s->gltf_skins){
            fprintf(stderr,"Load '%s': glTF skin alloc failed\n",path);
            return false;
        }
        for(int si=0;si<s->gltf_skin_count;si++){
            cgltf_skin *skin=&s->gltf->skins[si];
            GltfSkinInfo *dst=&s->gltf_skins[si];
            dst->joint_count=(int)(skin->joints_count<(cgltf_size)MAX_BONES?skin->joints_count:(cgltf_size)MAX_BONES);
            for(int ji=0;ji<dst->joint_count;ji++){
                dst->joint_nodes[ji]=gltf_node_index(s,skin->joints[ji]);
                dst->inverse_bind[ji]=mat4_identity();
                if(skin->inverse_bind_matrices&&ji<(int)skin->inverse_bind_matrices->count){
                    cgltf_float m[16]={0};
                    if(cgltf_accessor_read_float(skin->inverse_bind_matrices,(cgltf_size)ji,m,16))dst->inverse_bind[ji]=mat4_from_cgltf(m);
                }
            }
        }
    }
    if(s->gltf->images_count>0){
        cgltf_size want=s->gltf->images_count*2;
        if(want>(cgltf_size)MAX_GLTF_TEX_CACHE)want=MAX_GLTF_TEX_CACHE;
        s->gltf_tex_cache=calloc((size_t)want,sizeof(*s->gltf_tex_cache));
        if(!s->gltf_tex_cache&&want>0){
            fprintf(stderr,"Load '%s': glTF texture cache alloc failed\n",path);
            return false;
        }
        s->gltf_tex_cache_cap=(int)want;
    }
    for(int i=0;i<MAX_MESHES;i++)s->gltf_mesh_skin[i]=-1;
    s->num_meshes=0;
    float xmin=1e9f,ymin=1e9f,zmin=1e9f,xmax=-1e9f,ymax=-1e9f,zmax=-1e9f;
    float ymin_local=1e9f,ymax_local=-1e9f;
    for(size_t ni=0;ni<s->gltf->nodes_count&&s->num_meshes<MAX_MESHES;ni++){
        cgltf_node *nd=&s->gltf->nodes[ni];
        if(!nd->mesh)continue;
        cgltf_float world[16];
        cgltf_node_transform_world(nd,world);
        Mat4 wm=mat4_from_cgltf(world);
        int node_index=(int)ni;
        int skin_index=-1;
        if(nd->skin)skin_index=(int)(nd->skin-s->gltf->skins);
        for(size_t pi=0;pi<nd->mesh->primitives_count&&s->num_meshes<MAX_MESHES;pi++){
            if(build_gpu_mesh_gltf(s,&s->meshes[s->num_meshes],&nd->mesh->primitives[pi],nd,node_index,skin_index,path)){
                s->gltf_mesh_skin[s->num_meshes]=skin_index;
                s->num_meshes++;
            }
        }
        for(size_t pi=0;pi<nd->mesh->primitives_count;pi++){
            const cgltf_attribute *pos_attr=gltf_find_attribute(&nd->mesh->primitives[pi],cgltf_attribute_type_position,0);
            if(!pos_attr||!pos_attr->data)continue;
            for(size_t vi=0;vi<pos_attr->data->count;vi++){
                cgltf_float p[3]={0};
                if(!cgltf_accessor_read_float(pos_attr->data,vi,p,3))continue;
                float x=wm.m[0]*p[0]+wm.m[4]*p[1]+wm.m[8]*p[2]+wm.m[12];
                float y=wm.m[1]*p[0]+wm.m[5]*p[1]+wm.m[9]*p[2]+wm.m[13];
                float z=wm.m[2]*p[0]+wm.m[6]*p[1]+wm.m[10]*p[2]+wm.m[14];
                if(x<xmin)xmin=x;if(x>xmax)xmax=x;
                if(y<ymin)ymin=y;if(y>ymax)ymax=y;
                if(z<zmin)zmin=z;if(z>zmax)zmax=z;
                if(p[1]<ymin_local)ymin_local=p[1];
                if(p[1]>ymax_local)ymax_local=p[1];
            }
        }
    }
    if(s->num_meshes==0){cgltf_free(s->gltf);s->gltf=NULL;s->format=SCENE_FORMAT_NONE;return false;}
    s->cx=(xmin+xmax)*0.5f;s->cy=(ymin+ymax)*0.5f;s->cz=(zmin+zmax)*0.5f;
    s->ymin_raw=ymin_local;s->ymax_raw=ymax_local;
    float md=fmaxf(xmax-xmin,fmaxf(ymax-ymin,zmax-zmin));if(md<0.0001f)md=1.0f;
    s->model_scale=2.0f/md;s->ground_offset=-(ymin-s->cy)*s->model_scale;
    printf("[Scene] %s: %d glTF meshes\n",path,s->num_meshes);
    return true;
}

static bool build_gpu_mesh(GpuMesh *gm,ufbx_skin_deformer **skin_out,ufbx_mesh *mesh,ufbx_node *node,const char *path,size_t material_index){
    ufbx_skin_deformer *skin=mesh->skin_deformers.count>0?mesh->skin_deformers.data[0]:NULL;
    MeshNormals mesh_normals=mesh_normals_build(mesh);
    const ufbx_vertex_vec2 *uv_stream=mesh_preferred_uv(mesh);
    const ufbx_vertex_vec3 *tan_stream=mesh_preferred_tangent(mesh);
    const ufbx_vertex_vec3 *bitan_stream=mesh_preferred_bitangent(mesh);
    debug_print_mesh_binding(mesh,node,path,uv_stream);
    *skin_out=skin;
    int32_t c2g[MAX_BONES];memset(c2g,-1,sizeof(c2g));gm->num_gpu_bones=0;
    if(skin){size_t nc=skin->clusters.count<MAX_BONES?skin->clusters.count:MAX_BONES;for(size_t ci=0;ci<nc;ci++){c2g[ci]=(int32_t)ci;gm->cluster_map[ci]=(int32_t)ci;}gm->num_gpu_bones=(int32_t)nc;}
    size_t ntri=mesh->max_face_triangles*3;uint32_t *tri=malloc(ntri*sizeof(uint32_t));if(!tri){mesh_normals_free(&mesh_normals);return false;}
    Vertex *verts=NULL;uint32_t *inds=NULL;size_t vc=0,ic=0,vcap=4096,icap=4096;
    verts=malloc(vcap*sizeof(Vertex));inds=malloc(icap*sizeof(uint32_t));if(!verts||!inds){free(tri);mesh_normals_free(&mesh_normals);return false;}
    for(size_t fi=0;fi<mesh->num_faces;fi++){
        if(mesh->materials.count>1&&mesh->face_material.count==mesh->num_faces){
            uint32_t face_mat=mesh->face_material.data[fi];
            if(face_mat!=material_index)continue;
        }
        ufbx_face face=mesh->faces.data[fi];size_t nt=ufbx_triangulate_face(tri,ntri,mesh,face);
        for(size_t ti=0;ti<nt*3;ti++){
            uint32_t ix=tri[ti],vi=mesh->vertex_indices.data[ix];Vertex v={0};
            ufbx_vec3 p=ufbx_get_vertex_vec3(&mesh->vertex_position,ix);v.pos[0]=(float)p.x;v.pos[1]=(float)p.y;v.pos[2]=(float)p.z;
            ufbx_vec3 n=mesh_normal_get(&mesh_normals,mesh,ix);v.normal[0]=(float)n.x;v.normal[1]=(float)n.y;v.normal[2]=(float)n.z;
            if(uv_stream&&uv_stream->exists){ufbx_vec2 uv=ufbx_get_vertex_vec2(uv_stream,ix);v.uv[0]=(float)uv.x;v.uv[1]=(float)uv.y;}
            if(tan_stream&&tan_stream->exists){
                ufbx_vec3 t=ufbx_get_vertex_vec3(tan_stream,ix);
                v.tangent[0]=(float)t.x;v.tangent[1]=(float)t.y;v.tangent[2]=(float)t.z;
            }
            if(bitan_stream&&bitan_stream->exists){
                ufbx_vec3 b=ufbx_get_vertex_vec3(bitan_stream,ix);
                v.bitangent[0]=(float)b.x;v.bitangent[1]=(float)b.y;v.bitangent[2]=(float)b.z;
            }else{
                Vec3 nn={v.normal[0],v.normal[1],v.normal[2]};
                Vec3 tt={v.tangent[0],v.tangent[1],v.tangent[2]};
                if(fabsf(tt.x)+fabsf(tt.y)+fabsf(tt.z)<1e-5f){
                    tt=fabsf(nn.y)<0.99f?(Vec3){0,1,0}:(Vec3){1,0,0};
                    tt=vec3_norm(vec3_sub(tt,vec3_scale(nn,vec3_dot(tt,nn))));
                    v.tangent[0]=tt.x;v.tangent[1]=tt.y;v.tangent[2]=tt.z;
                }
                Vec3 bb=vec3_norm(vec3_cross(nn,tt));
                v.bitangent[0]=bb.x;v.bitangent[1]=bb.y;v.bitangent[2]=bb.z;
            }
            if(skin){
                /* Guard: skin->vertices may be smaller than mesh vertex count
                   if the FBX has helper/proxy geometry sharing the skin deformer */
                if(vi < skin->vertices.count){
                    ufbx_skin_vertex sv=skin->vertices.data[vi];
                    size_t nw=sv.num_weights<MAX_WEIGHTS?sv.num_weights:MAX_WEIGHTS;
                    float tw=0;
                    int out=0; /* pack valid weights densely into slots 0..3 */
                    for(size_t wi=0;wi<nw&&out<MAX_WEIGHTS;wi++){
                        ufbx_skin_weight sw=skin->weights.data[sv.weight_begin+wi];
                        if(sw.cluster_index>=(size_t)MAX_BONES) continue;
                        int32_t gb=c2g[sw.cluster_index];
                        if(gb<0||gb>=gm->num_gpu_bones) continue;
                        v.bones[out]=gb;
                        v.weights[out]=(float)sw.weight;
                        tw+=(float)sw.weight;
                        out++;
                    }
                    /* Normalize surviving weights; zero-weight verts use identity
                       matrix via the shader's dot(aWeights)<0.05 fallback          */
                    if(tw>0.001f){for(int wi=0;wi<MAX_WEIGHTS;wi++)v.weights[wi]/=tw;}
                    else{for(int wi=0;wi<MAX_WEIGHTS;wi++){v.weights[wi]=0;v.bones[wi]=0;}}
                }
                /* else: no skin data for this vertex → weights stay 0 → identity  */
            }
            if(vc>=vcap){vcap*=2;verts=realloc(verts,vcap*sizeof(Vertex));}
            if(ic>=icap){icap*=2;inds=realloc(inds,icap*sizeof(uint32_t));}
            inds[ic++]=(uint32_t)vc;verts[vc++]=v;
        }
    }
    free(tri);
    if(ic==0||vc==0){
        mesh_normals_free(&mesh_normals);
        free(verts);free(inds);
        return false;
    }
    GLH_Attrib attrs[]={{0,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,pos)},{1,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,normal)},{2,2,GL_FLOAT,GL_FALSE,offsetof(Vertex,uv)},{3,4,GL_FLOAT,GL_FALSE,offsetof(Vertex,weights)},{4,4,GL_INT,0,offsetof(Vertex,bones)},{5,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,tangent)},{6,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,bitangent)}};
    gm->vao=GLH_mesh(verts,(size_t)vc*sizeof(Vertex),inds,(size_t)ic*sizeof(uint32_t),sizeof(Vertex),attrs,7);
    gm->index_count=(GLsizei)ic;gm->geometry_to_world=mat4_from_ufbx(&node->geometry_to_world);gm->node_typed_id=node->typed_id;gm->texture=get_mesh_tex(mesh,node,path,material_index);
    {
        ufbx_material *mat=mesh_material_at(mesh,node,material_index);
        gpu_mesh_apply_pbr(gm,mat,path);
    }
    mesh_normals_free(&mesh_normals);
    free(verts);free(inds);return true;
}

static bool scene_load(Scene *s,const char *path){
    memset(s,0,sizeof(*s));
    if(path_has_extension_ci(path,".gltf")||path_has_extension_ci(path,".glb"))return scene_load_gltf(s,path);
    ufbx_load_opts opts={0};opts.generate_missing_normals=true;opts.target_axes=ufbx_axes_right_handed_y_up;opts.target_unit_meters=1.0f;opts.use_blender_pbr_material=true;opts.normalize_tangents=true;
    ufbx_error err;s->scene=ufbx_load_file(path,&opts,&err);
    if(!s->scene){fprintf(stderr,"Load '%s': %s\n",path,err.description.data);return false;}
    s->format=SCENE_FORMAT_UFBX;
    s->num_meshes=0;
    for(size_t ni=0;ni<s->scene->nodes.count&&s->num_meshes<MAX_MESHES;ni++){
        ufbx_node *nd=s->scene->nodes.data[ni];
        if(!nd->mesh)continue;
        size_t mat_count=nd->mesh->materials.count;
        bool split_by_material=mat_count>1&&nd->mesh->face_material.count==nd->mesh->num_faces;
        size_t passes=split_by_material?mat_count:1;
        for(size_t part=0;part<passes&&s->num_meshes<MAX_MESHES;part++){
            int mi=s->num_meshes++;
            if(!build_gpu_mesh(&s->meshes[mi],&s->skins[mi],nd->mesh,nd,path,split_by_material?part:0))s->num_meshes--;
        }
    }
    float xmin=1e9f,ymin=1e9f,zmin=1e9f,xmax=-1e9f,ymax=-1e9f,zmax=-1e9f;
    for(size_t ni=0;ni<s->scene->nodes.count;ni++){ufbx_node *nd=s->scene->nodes.data[ni];if(!nd->mesh)continue;
        for(size_t vi=0;vi<nd->mesh->num_vertices;vi++){ufbx_vec3 lp=nd->mesh->vertices.data[vi];ufbx_vec3 p=ufbx_transform_position(&nd->geometry_to_world,lp);
            float x=(float)p.x,y=(float)p.y,z=(float)p.z;if(x<xmin)xmin=x;if(x>xmax)xmax=x;if(y<ymin)ymin=y;if(y>ymax)ymax=y;if(z<zmin)zmin=z;if(z>zmax)zmax=z;}}
    /* Also compute local-space Y bounds (matching build_gpu_mesh vertex storage) for village placement */
    float ymin_local=1e9f,ymax_local=-1e9f;
    for(size_t ni=0;ni<s->scene->nodes.count;ni++){ufbx_node *nd=s->scene->nodes.data[ni];if(!nd->mesh)continue;
        for(size_t fi=0;fi<nd->mesh->num_faces;fi++){ufbx_face face=nd->mesh->faces.data[fi];
            for(size_t i=face.index_begin;i<face.index_begin+face.num_indices;i++){
                float y=(float)ufbx_get_vertex_vec3(&nd->mesh->vertex_position,i).y;
                if(y<ymin_local)ymin_local=y;if(y>ymax_local)ymax_local=y;}}}
    s->ymin_raw=ymin_local; s->ymax_raw=ymax_local;
    printf("[Scene] %s: local_ymin=%.3f local_ymax=%.3f world_ymin=%.3f world_ymax=%.3f\n",
           path,(double)ymin_local,(double)ymax_local,(double)ymin,(double)ymax);
    s->cx=(xmin+xmax)*0.5f;s->cy=(ymin+ymax)*0.5f;s->cz=(zmin+zmax)*0.5f;
    s->ymin_raw=ymin; s->ymax_raw=ymax;
    float md=fmaxf(xmax-xmin,fmaxf(ymax-ymin,zmax-zmin));if(md<0.0001f)md=1.0f;
    s->model_scale=2.0f/md;s->ground_offset=-(ymin-s->cy)*s->model_scale;
    printf("[Scene] %s: %d meshes\n",path,s->num_meshes);return true;
}
static void scene_free(Scene *s){
    if(s->scene){ufbx_free_scene(s->scene);s->scene=NULL;}
    if(s->gltf){cgltf_free(s->gltf);s->gltf=NULL;}
    free(s->gltf_bind_pose);free(s->gltf_rest_world);free(s->gltf_eval_pose);free(s->gltf_eval_world);free(s->gltf_eval_mark);free(s->gltf_skins);free(s->gltf_tex_cache);
    {
        GLuint seen[MAX_MESHES*4];
        int seen_count=0;
        for(int i=0;i<s->num_meshes;i++){
            glDeleteVertexArrays(1,&s->meshes[i].vao);
            GLuint tex_ids[4]={s->meshes[i].texture,s->meshes[i].normal_tex,s->meshes[i].mr_tex,s->meshes[i].emissive_tex};
            for(int ti=0;ti<4;ti++){
                GLuint tex=tex_ids[ti];
                if(!tex)continue;
                bool dup=false;
                for(int si=0;si<seen_count;si++)if(seen[si]==tex){dup=true;break;}
                if(dup)continue;
                if(seen_count<(int)(sizeof(seen)/sizeof(seen[0])))seen[seen_count++]=tex;
                glDeleteTextures(1,&tex);
            }
        }
    }
    memset(s,0,sizeof(*s));
}
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
"layout(location=2)in vec2 aUV;\nlayout(location=3)in vec4 aWeights;\nlayout(location=4)in ivec4 aBones;\nlayout(location=5)in vec3 aTangent;\nlayout(location=6)in vec3 aBitangent;\n"
"uniform mat4 uProj,uView,uModel,uBones[128];\n"
"out vec3 vNormal,vFragPos,vTangent,vBitangent;out vec2 vUV;\n"
"void main(){\n"
"    mat4 skin=aWeights.x*uBones[aBones.x]+aWeights.y*uBones[aBones.y]+aWeights.z*uBones[aBones.z]+aWeights.w*uBones[aBones.w];\n"
"    if(dot(aWeights,vec4(1))<0.05)skin=mat4(1);\n"
"    vec3 skinnedNormal=mat3(skin)*aNormal;\n"
"    vec3 skinnedTangent=mat3(skin)*aTangent;\n"
"    vec3 skinnedBitangent=mat3(skin)*aBitangent;\n"
"    vec4 world=uModel*skin*vec4(aPos,1);\n"
"    gl_Position=uProj*uView*world;vFragPos=world.xyz;\n"
"    mat3 nmat=mat3(transpose(inverse(uModel)));\n"
"    vNormal=normalize(nmat*skinnedNormal);vTangent=normalize(nmat*skinnedTangent);vBitangent=normalize(nmat*skinnedBitangent);vUV=aUV;}\n";

static const char *FRAG_SKIN=
"#version 330 core\n"
"in vec3 vNormal,vFragPos,vTangent,vBitangent;in vec2 vUV;\n"
"uniform sampler2D uAlbedo,uNormalTex,uMRTex,uEmissiveTex;\n"
"uniform bool uHasTexture,uHasNormalTex,uHasMRTex,uHasEmissiveTex;\n"
"uniform vec4 uBaseColor;uniform vec3 uEmissiveFactor,uCamPos;uniform float uMetallic,uRoughness,uAlphaCutoff,uFlash;\n"
 LIGHTING_UNIFORMS
"out vec4 FragColor;\nvoid main(){\n"
"    vec3 n=normalize(vNormal);\n"
"    if(uHasNormalTex){ vec3 tn=texture(uNormalTex,vUV).xyz*2.0-1.0; n=normalize(mat3(normalize(vTangent),normalize(vBitangent),normalize(vNormal))*tn); }\n"
"    vec4 texSample=uHasTexture?texture(uAlbedo,vUV):vec4(1.0);\n"
"    vec4 baseSample=texSample*uBaseColor;\n"
"    if(baseSample.a<uAlphaCutoff)discard;\n"
"    vec3 base=baseSample.rgb;\n"
"    float roughness=uRoughness,metallic=uMetallic;\n"
"    if(uHasMRTex){ vec3 mr=texture(uMRTex,vUV).rgb; roughness=clamp(roughness*mr.g,0.04,1.0); metallic=clamp(metallic*mr.b,0.0,1.0); }\n"
"    vec3 emissive=uEmissiveFactor*(uHasEmissiveTex?texture(uEmissiveTex,vUV).rgb:vec3(1.0));\n"
"    vec3 diffColor=mix(base,vec3(0.0),metallic);\n"
"    vec3 specColor=mix(vec3(0.04),base,metallic);\n"
"    vec3 vPos=vFragPos;\n"
"    vec3 view=normalize(uCamPos-vPos);\n"
"    float hemi=clamp(n.y*0.5+0.5,0.0,1.0);\n"
"    vec3 skyTint=mix(vec3(0.19,0.19,0.20),vec3(0.48,0.49,0.52),hemi);\n"
"    float fres=pow(clamp(1.0-max(dot(n,view),0.0),0.0,1.0),5.0);\n"
"    float diff=max(dot(n,-uSunDir),0.0);\n"
"    vec3 sunHalf=normalize(view-uSunDir);\n"
"    float sunSpec=pow(max(dot(n,sunHalf),0.0),mix(96.0,8.0,roughness));\n"
"    vec3 envDiff=diffColor*skyTint*(0.27+0.30*(1.0-metallic));\n"
"    vec3 envSpec=specColor*skyTint*(0.28+0.40*fres)*(1.0-roughness*0.48);\n"
"    vec3 col=diffColor*(uAmbient+uSunColor*diff)+specColor*uSunColor*sunSpec*(1.0-roughness*0.65)+envDiff+envSpec;\n"
"    for(int i=0;i<uNumLights;i++){\n"
"        vec3 lv=uLightPos[i]-vPos;float d=length(lv);\n"
"        float att=1.0-clamp(d/uLightRadius[i],0,1);att*=att;\n"
"        vec3 ldir=normalize(lv); vec3 h=normalize(view+ldir);\n"
"        float ldiff=max(dot(n,ldir),0.0);\n"
"        float lspec=pow(max(dot(n,h),0.0),mix(96.0,8.0,roughness));\n"
"        col+=diffColor*uLightColor[i]*ldiff*att;\n"
"        col+=specColor*uLightColor[i]*lspec*att*(1.0-roughness*0.65);\n"
"    }\n"
"    col+=emissive;\n"
"    col=mix(col,vec3(1.0,0.1,0.1),uFlash);\n"
"    FragColor=vec4(col,baseSample.a);}\n";

typedef struct{GLuint prog;GLint loc_model,loc_bones;}SkinProg;
static void skinprog_init(SkinProg *sp){sp->prog=GLH_program(VERT_SKIN,FRAG_SKIN);sp->loc_model=glGetUniformLocation(sp->prog,"uModel");sp->loc_bones=glGetUniformLocation(sp->prog,"uBones[0]");}

static Mat4 entity_mat(Vec3 pos,float yaw,float dy,float scale,float cx,float cy,float cz){
    return mat4_mul(mat4_translate(pos.x,pos.y+dy,pos.z),mat4_mul(mat4_rot_y(yaw),mat4_mul(mat4_scale_u(scale),mat4_translate(-cx,-cy,-cz))));
}

static void draw_scene(SkinProg *sp,Scene *sc,Mat4 wm,int ai,double at,float flash,Mat4 proj,Mat4 view,Vec3 cam_pos,LightingSystem *ls){
    if(!scene_has_data(sc))return; /* no mesh loaded — skip silently */
    lighting_push(ls,sp->prog);GLH_begin(sp->prog,proj.m,view.m);GLH_float(sp->prog,"uFlash",flash);GLH_vec3(sp->prog,"uCamPos",cam_pos.x,cam_pos.y,cam_pos.z);
    ufbx_scene *eval=NULL;
    if(sc->format==SCENE_FORMAT_UFBX&&sc->scene&&sc->scene->anim_stacks.count>0&&ai>=0){int idx=ai<(int)sc->scene->anim_stacks.count?ai:(int)sc->scene->anim_stacks.count-1;ufbx_anim_stack *stk=sc->scene->anim_stacks.data[idx];double real_end=anim_stack_real_end(stk);double dur=real_end-stk->time_begin;double t=dur>0?fmod(at,dur):0;eval=ufbx_evaluate_scene(sc->scene,stk->anim,stk->time_begin+t,NULL,NULL);}
    else if(sc->format==SCENE_FORMAT_GLTF)scene_gltf_evaluate(sc,ai,at);
    Mat4 bone_mats[MAX_BONES];
    for(int mi=0;mi<sc->num_meshes;mi++){GpuMesh *gm=&sc->meshes[mi];Mat4 mesh_mat;
        if(sc->format==SCENE_FORMAT_UFBX&&sc->skins[mi])mesh_mat=wm;
        else if(sc->format==SCENE_FORMAT_GLTF&&gm->num_gpu_bones>0)mesh_mat=wm;
        else if(eval&&sc->format==SCENE_FORMAT_UFBX&&gm->node_typed_id<eval->nodes.count){ufbx_matrix g2w=eval->nodes.data[gm->node_typed_id]->geometry_to_world;mesh_mat=mat4_mul(wm,mat4_from_ufbx(&g2w));}
        else if(sc->format==SCENE_FORMAT_GLTF&&sc->gltf_eval_world&&gm->node_typed_id<(uint32_t)sc->gltf_node_count)mesh_mat=mat4_mul(wm,sc->gltf_eval_world[gm->node_typed_id]);
        else mesh_mat=mat4_mul(wm,gm->geometry_to_world);
        glUniformMatrix4fv(sp->loc_model,1,GL_FALSE,mesh_mat.m);
        if(gm->texture){GLH_bind_texture(sp->prog,"uAlbedo",gm->texture,0);GLH_bool(sp->prog,"uHasTexture",1);}
        else GLH_bool(sp->prog,"uHasTexture",0);
        if(gm->normal_tex){GLH_bind_texture(sp->prog,"uNormalTex",gm->normal_tex,1);GLH_bool(sp->prog,"uHasNormalTex",1);} else GLH_bool(sp->prog,"uHasNormalTex",0);
        if(gm->mr_tex){GLH_bind_texture(sp->prog,"uMRTex",gm->mr_tex,2);GLH_bool(sp->prog,"uHasMRTex",1);} else GLH_bool(sp->prog,"uHasMRTex",0);
        if(gm->emissive_tex){GLH_bind_texture(sp->prog,"uEmissiveTex",gm->emissive_tex,3);GLH_bool(sp->prog,"uHasEmissiveTex",1);} else GLH_bool(sp->prog,"uHasEmissiveTex",0);
        GLH_vec4(sp->prog,"uBaseColor",gm->mat_color[0],gm->mat_color[1],gm->mat_color[2],gm->mat_color[3]);
        GLH_vec3(sp->prog,"uEmissiveFactor",gm->emissive[0],gm->emissive[1],gm->emissive[2]);
        GLH_float(sp->prog,"uMetallic",gm->metallic);
        GLH_float(sp->prog,"uRoughness",gm->roughness);
        GLH_float(sp->prog,"uAlphaCutoff",gm->alpha_cutoff);
        for(int i=0;i<MAX_BONES;i++)bone_mats[i]=mat4_identity();
        if(sc->format==SCENE_FORMAT_GLTF)compute_bones_gltf(bone_mats,gm->num_gpu_bones,gm,sc);
        else compute_bones(bone_mats,gm->num_gpu_bones,gm,sc->skins[mi],eval);
        glUniformMatrix4fv(sp->loc_bones,MAX_BONES,GL_FALSE,bone_mats[0].m);
        glBindVertexArray(gm->vao);glDrawElements(GL_TRIANGLES,gm->index_count,GL_UNSIGNED_INT,0);}
    if(eval)ufbx_free_scene(eval);
}

/* Draw using a CharModel — resolves correct scene+stack per anim slot */
static void draw_charmodel(SkinProg *sp,CharModel *cm,Mat4 wm,int anim_slot,double at,float flash,Mat4 proj,Mat4 view,Vec3 cam_pos,LightingSystem *ls){
    if(!cm||!scene_has_data(&cm->base))return; /* model not loaded — skip */
    if(anim_slot<0||anim_slot>=ANIM_SLOTS)anim_slot=ANIM_IDLE;
    AnimEntry *ae=&cm->anims[anim_slot];
    Scene *sc=ae->scene?ae->scene:&cm->base;
    draw_scene(sp,sc,wm,ae->scene?ae->stack:-1,at,flash,proj,view,cam_pos,ls);
}
/*
   USAGE in main() - place props before the game loop:

   // Auto-snap Y to terrain, auto AABB:
   prop_place(&g_props,"models/house.fbx",  -1910,0,-1795, 0,  PROP_COLL_MESH, 0,0, 0,0);

   // No collision (fires, barrels, decorations):
   prop_place(&g_props,"models/fire.fbx",   -1900,0,-1800, 45, PROP_COLL_NONE, 0,0, 0,0);

   // Manual AABB (cx,cz=center offset from origin; hw,hd=half width/depth):
   prop_place(&g_props,"models/tower.fbx",  -1890,0,-1810, 0,  PROP_COLL_AABB, 0,0, 3.0,3.0);

   // Per-triangle collision (stairs, arches, complex shapes):
   prop_place(&g_props,"models/steps.fbx",  -1895,0,-1795, 90, PROP_COLL_MESH, 0,0, 0,0);

   COLLISION MODES:
     PROP_COLL_NONE  - no collision (fires, crates, decorations)
     PROP_COLL_AABB  - fast box collision (most buildings)
     PROP_COLL_MESH  - per-triangle (stairs, arches, railings)

   ROTATION: degrees around Y axis
   Y=0 auto-snaps to terrain height
*/
#define MAX_PROPS 512
typedef enum{PROP_COLL_NONE=0,PROP_COLL_AABB,PROP_COLL_MESH}PropCollType;

/* ── Streaming states ────────────────────────────────────────── */
typedef enum{
    PROP_UNLOADED=0,  /* definition only, zero GPU memory      */
    PROP_QUEUED,      /* in background parse queue              */
    PROP_READY,       /* CPU data assembled, awaiting GL upload */
    PROP_LOADED       /* fully on GPU, renders normally         */
}PropStreamState;

/* CPU-side mesh data — produced by background thread, consumed by
   main thread for GL upload.  No GL calls touch this struct.    */
typedef struct{
    Vertex    *verts;
    uint32_t  *inds;
    size_t     vc,ic;
    float      mat_color[3];
    /* Embedded texture: either raw bytes OR a file path */
    void      *tex_mem;   /* stb_image-decoded RGBA pixels, or NULL */
    int        tex_w,tex_h;
    char       tex_path[512]; /* fallback file path if tex_mem==NULL  */
    Mat4       geometry_to_world;
    uint32_t   node_typed_id;
    int32_t    cluster_map[MAX_BONES];
    int32_t    num_gpu_bones;
}CpuMeshData;

static void gltf_texture_to_cpu(const cgltf_texture_view *view,const char *path,CpuMeshData *cm){
    if(!view||!view->texture||!cm)return;
    const cgltf_texture *tex=view->texture;
    const cgltf_image *image=tex->image?tex->image:(tex->has_basisu?tex->basisu_image:(tex->has_webp?tex->webp_image:NULL));
    if(!image)return;
    if(image->buffer_view&&image->buffer_view->buffer&&image->buffer_view->buffer->data){
        const void *data=cgltf_buffer_view_data(image->buffer_view);
        int size=(int)image->buffer_view->size,w=0,h=0,ch=0;
        unsigned char *px=stbi_load_from_memory(data,size,&w,&h,&ch,4);
        if(px){cm->tex_mem=px;cm->tex_w=w;cm->tex_h=h;return;}
    }
    if(image->uri&&image->uri[0]&&strncmp(image->uri,"data:",5)){
        char dir[1024],full[1024];
        model_dir_from_path(dir,sizeof(dir),path);
        snprintf(full,sizeof(full),"%s%s",dir,image->uri);
        path_normalize_slashes(full);
        snprintf(cm->tex_path,sizeof(cm->tex_path),"%s",full);
    }
}

static CpuMeshData cpu_mesh_build_gltf(const cgltf_primitive *prim,const cgltf_node *node,const char *path){
    CpuMeshData cm={0};
    const cgltf_attribute *pos_attr=gltf_find_attribute(prim,cgltf_attribute_type_position,0);
    const cgltf_attribute *nrm_attr=gltf_find_attribute(prim,cgltf_attribute_type_normal,0);
    const cgltf_attribute *uv_attr =gltf_find_attribute(prim,cgltf_attribute_type_texcoord,0);
    if(!pos_attr||!pos_attr->data)return cm;
    size_t vcount=pos_attr->data->count;
    size_t icount=prim->indices?prim->indices->count:vcount;
    cm.verts=calloc(vcount,sizeof(Vertex));
    cm.inds=malloc(icount*sizeof(uint32_t));
    if(!cm.verts||!cm.inds){free(cm.verts);free(cm.inds);memset(&cm,0,sizeof(cm));return cm;}
    for(size_t i=0;i<vcount;i++){
        cgltf_float fv[4]={0,0,0,1};
        Vertex *v=&cm.verts[i];
        cgltf_accessor_read_float(pos_attr->data,i,fv,3);v->pos[0]=(float)fv[0];v->pos[1]=(float)fv[1];v->pos[2]=(float)fv[2];
        if(nrm_attr&&cgltf_accessor_read_float(nrm_attr->data,i,fv,3)){v->normal[0]=(float)fv[0];v->normal[1]=(float)fv[1];v->normal[2]=(float)fv[2];}
        else v->normal[1]=1.0f;
        if(uv_attr&&cgltf_accessor_read_float(uv_attr->data,i,fv,2)){v->uv[0]=(float)fv[0];v->uv[1]=(float)fv[1];}
    }
    for(size_t i=0;i<icount;i++)cm.inds[i]=prim->indices?(uint32_t)cgltf_accessor_read_index(prim->indices,i):(uint32_t)i;
    cm.vc=vcount;cm.ic=icount;
    cgltf_float world[16];cgltf_node_transform_world(node,world);
    cm.geometry_to_world=mat4_from_cgltf(world);
    cm.node_typed_id=(uint32_t)(node-node);
    cm.mat_color[0]=cm.mat_color[1]=cm.mat_color[2]=1.0f;
    if(prim->material){
        const cgltf_material *mat=prim->material;
        gltf_texture_to_cpu(&mat->pbr_metallic_roughness.base_color_texture,path,&cm);
        if(mat->has_pbr_metallic_roughness){
            cm.mat_color[0]=(float)mat->pbr_metallic_roughness.base_color_factor[0];
            cm.mat_color[1]=(float)mat->pbr_metallic_roughness.base_color_factor[1];
            cm.mat_color[2]=(float)mat->pbr_metallic_roughness.base_color_factor[2];
        }
    }
    return cm;
}

/* One completed parse job — holds all meshes for one prop */
typedef struct{
    CpuMeshData meshes[MAX_MESHES];
    int         num_meshes;
    float       ymin;
    AABB        aabb;
    TriMesh     trimesh;
    int         prop_idx;   /* index into PropList  */
    bool        valid;
}StreamJob;

typedef struct{
    GpuMesh  meshes[MAX_MESHES];int num_meshes;
    Vec3     pos;
    float    rot_x,rot_y,rot_z;
    float    scale;
    float    ymin;
    PropCollType coll_type;
    /* AABB / trimesh rebuild params */
    float    aabb_cx,aabb_cz,aabb_hw,aabb_hd;
    AABB     aabb;
    TriMesh  trimesh;
    char     path[256];
    PropStreamState stream_state;
    StreamJob *ready_job;  /* non-NULL when PROP_READY, main thread uploads then clears */
    bool     active;       /* slot in use (even when unloaded) */
}Prop;

typedef struct{Prop list[MAX_PROPS];int count;int selected;}PropList;
static GLuint g_prop_prog=0;
static PropList g_props;

/* ── Background streaming thread ────────────────────────────────
   One worker thread.  Main thread pushes prop indices to load;
   worker parses FBX + assembles CPU data; main thread picks up
   completed jobs and does the GL upload (max STREAM_GPU_PER_FRAME
   per frame to avoid stutter).                                   */
#define STREAM_LOAD_DIST     200.0f  /* start loading when within 200 units  */
#define STREAM_UNLOAD_DIST   280.0f  /* free GPU when beyond 280 units       */
#define STREAM_GPU_PER_FRAME 2       /* max GL uploads per frame (keep <5ms) */
#define STREAM_QUEUE_CAP     64      /* room for more simultaneous requests  */

typedef struct{
    /* Request queue: main → worker */
    int       req[STREAM_QUEUE_CAP];
    int       req_head,req_tail,req_count;
    /* Ready queue: worker → main */
    StreamJob *ready[STREAM_QUEUE_CAP];
    int       rdy_head,rdy_tail,rdy_count;
    SDL_mutex *mutex;
    SDL_sem   *work_sem;   /* signalled when request added   */
    SDL_Thread *thread;
    bool       quit;
    PropList  *props;      /* shared read-only reference     */
}StreamSystem;

static StreamSystem g_stream;

/* Background thread: parse FBX on CPU, no GL calls */
static CpuMeshData cpu_mesh_build(ufbx_mesh *mesh,ufbx_node *node,const char *path){
    CpuMeshData cm={0};
    ufbx_skin_deformer *skin=mesh->skin_deformers.count>0?mesh->skin_deformers.data[0]:NULL;
    MeshNormals mesh_normals=mesh_normals_build(mesh);
    const ufbx_vertex_vec2 *uv_stream=mesh_preferred_uv(mesh);
    int32_t c2g[MAX_BONES];memset(c2g,-1,sizeof(c2g));
    cm.num_gpu_bones=0;
    if(skin){
        size_t nc=skin->clusters.count<MAX_BONES?skin->clusters.count:MAX_BONES;
        for(size_t ci=0;ci<nc;ci++){c2g[ci]=(int32_t)ci;cm.cluster_map[ci]=(int32_t)ci;}
        cm.num_gpu_bones=(int32_t)nc;
    }
    size_t ntri=mesh->max_face_triangles*3;
    uint32_t *tri=malloc(ntri*sizeof(uint32_t));if(!tri){mesh_normals_free(&mesh_normals);return cm;}
    size_t vcap=4096,icap=4096;
    cm.verts=malloc(vcap*sizeof(Vertex));
    cm.inds =malloc(icap*sizeof(uint32_t));
    if(!cm.verts||!cm.inds){free(tri);free(cm.verts);free(cm.inds);mesh_normals_free(&mesh_normals);memset(&cm,0,sizeof(cm));return cm;}
    for(size_t fi=0;fi<mesh->num_faces;fi++){
        ufbx_face face=mesh->faces.data[fi];
        size_t nt=ufbx_triangulate_face(tri,ntri,mesh,face);
        for(size_t ti=0;ti<nt*3;ti++){
            uint32_t ix=tri[ti];uint32_t vi=mesh->vertex_indices.data[ix];Vertex v={0};
            ufbx_vec3 p=ufbx_get_vertex_vec3(&mesh->vertex_position,ix);
            v.pos[0]=(float)p.x;v.pos[1]=(float)p.y;v.pos[2]=(float)p.z;
            ufbx_vec3 n=mesh_normal_get(&mesh_normals,mesh,ix);
            v.normal[0]=(float)n.x;v.normal[1]=(float)n.y;v.normal[2]=(float)n.z;
            if(uv_stream&&uv_stream->exists){ufbx_vec2 uv=ufbx_get_vertex_vec2(uv_stream,ix);v.uv[0]=(float)uv.x;v.uv[1]=(float)uv.y;}
            if(skin&&vi<skin->vertices.count){
                ufbx_skin_vertex sv=skin->vertices.data[vi];
                size_t nw=sv.num_weights<MAX_WEIGHTS?sv.num_weights:MAX_WEIGHTS;
                float tw=0;int out=0;
                for(size_t wi=0;wi<nw&&out<MAX_WEIGHTS;wi++){
                    ufbx_skin_weight sw=skin->weights.data[sv.weight_begin+wi];
                    if(sw.cluster_index>=(size_t)MAX_BONES)continue;
                    int32_t gb=c2g[sw.cluster_index];if(gb<0||gb>=cm.num_gpu_bones)continue;
                    v.bones[out]=gb;v.weights[out]=(float)sw.weight;tw+=(float)sw.weight;out++;
                }
                if(tw>0.001f){for(int wi=0;wi<MAX_WEIGHTS;wi++)v.weights[wi]/=tw;}
                else{for(int wi=0;wi<MAX_WEIGHTS;wi++){v.weights[wi]=0;v.bones[wi]=0;}}
            }
            if(cm.vc>=vcap){vcap*=2;cm.verts=realloc(cm.verts,vcap*sizeof(Vertex));}
            if(cm.ic>=icap){icap*=2;cm.inds=realloc(cm.inds,icap*sizeof(uint32_t));}
            cm.inds[cm.ic++]=(uint32_t)cm.vc;cm.verts[cm.vc++]=v;
        }
    }
    free(tri);
    cm.geometry_to_world=mat4_from_ufbx(&node->geometry_to_world);
    cm.node_typed_id=node->typed_id;
    /* Material color */
    cm.mat_color[0]=0.75f;cm.mat_color[1]=0.72f;cm.mat_color[2]=0.68f;
    for(size_t mi=0;mi<mesh->materials.count;mi++){
        ufbx_material *mat=mesh->materials.data[mi];if(!mat)continue;
        /* Try to extract embedded texture pixels (stb_image — no GL) */
        ufbx_texture *tex=mat->pbr.base_color.texture;if(!tex)tex=mat->fbx.diffuse_color.texture;
        if(tex){
            if(tex->content.size>4){
                int w,h,ch;
                unsigned char *px=stbi_load_from_memory(tex->content.data,(int)tex->content.size,&w,&h,&ch,4);
                if(px){cm.tex_mem=px;cm.tex_w=w;cm.tex_h=h;}
            }
            if(!cm.tex_mem&&tex->relative_filename.length>0){
                texture_path_resolve(cm.tex_path,sizeof(cm.tex_path),path,tex->relative_filename);
            }
        }
        ufbx_vec3 c=mat->pbr.base_color.value_vec3;
        if(c.x+c.y+c.z<0.01f)c=mat->fbx.diffuse_color.value_vec3;
        if(c.x+c.y+c.z>0.01f){
            float br=((float)c.x+(float)c.y+(float)c.z)/3.0f;
            float sc2=(br<0.25f)?0.25f/br:1.0f;
            cm.mat_color[0]=clampf((float)c.x*sc2,0,1);
            cm.mat_color[1]=clampf((float)c.y*sc2,0,1);
            cm.mat_color[2]=clampf((float)c.z*sc2,0,1);
            break;
        }
    }
    mesh_normals_free(&mesh_normals);
    return cm;
}

static int stream_worker_thread(void *arg){
    StreamSystem *ss=(StreamSystem*)arg;
    while(1){
        SDL_SemWait(ss->work_sem);
        if(ss->quit)break;

        /* Dequeue one request */
        SDL_LockMutex(ss->mutex);
        if(ss->req_count==0){SDL_UnlockMutex(ss->mutex);continue;}
        int pidx=ss->req[ss->req_head%STREAM_QUEUE_CAP];
        ss->req_head++;ss->req_count--;
        SDL_UnlockMutex(ss->mutex);

        Prop *p=&ss->props->list[pidx];
        StreamJob *job=calloc(1,sizeof(StreamJob));
        if(!job)continue;
        job->prop_idx=pidx;

        if(path_has_extension_ci(p->path,".gltf")||path_has_extension_ci(p->path,".glb")){
            cgltf_options gopts={0};
            cgltf_data *gltf=NULL;
            cgltf_result gres=cgltf_parse_file(&gopts,p->path,&gltf);
            if(gres!=cgltf_result_success||!gltf||cgltf_load_buffers(&gopts,gltf,p->path)!=cgltf_result_success){
                fprintf(stderr,"[Stream] Load '%s': cgltf failed\n",p->path);
                SDL_LockMutex(ss->mutex);
                if(p->stream_state==PROP_QUEUED)p->stream_state=PROP_UNLOADED;
                SDL_UnlockMutex(ss->mutex);
                if(gltf)cgltf_free(gltf);
                free(job);continue;
            }
            float ymn=1e9f;
            for(size_t ni=0;ni<gltf->nodes_count;ni++){
                cgltf_node *nd=&gltf->nodes[ni];if(!nd->mesh)continue;
                cgltf_float world[16];cgltf_node_transform_world(nd,world);Mat4 wm=mat4_from_cgltf(world);
                for(size_t pi=0;pi<nd->mesh->primitives_count;pi++){
                    const cgltf_attribute *pos_attr=gltf_find_attribute(&nd->mesh->primitives[pi],cgltf_attribute_type_position,0);
                    if(!pos_attr||!pos_attr->data)continue;
                    for(size_t vi=0;vi<pos_attr->data->count;vi++){
                        cgltf_float pv[3]={0};cgltf_accessor_read_float(pos_attr->data,vi,pv,3);
                        float y=wm.m[1]*pv[0]+wm.m[5]*pv[1]+wm.m[9]*pv[2]+wm.m[13];
                        if(y<ymn)ymn=y;
                    }
                    if(job->num_meshes<MAX_MESHES){
                        job->meshes[job->num_meshes]=cpu_mesh_build_gltf(&nd->mesh->primitives[pi],nd,p->path);
                        if(job->meshes[job->num_meshes].verts)job->num_meshes++;
                    }
                }
            }
            job->ymin=ymn;
            if(p->coll_type==PROP_COLL_MESH){
                trimesh_build_gltf(&job->trimesh,gltf);
                float d2r=(float)3.14159265f/180.0f;
                float lift=-job->ymin*p->scale;
                Mat4 liftM=mat4_translate(0,lift,0);
                Mat4 sc2=mat4_scale_xyz(p->scale,p->scale,p->scale);
                Mat4 rot=mat4_mul(mat4_rot_y(p->rot_y*d2r),mat4_mul(mat4_rot_x(p->rot_x*d2r),mat4_rot_z(p->rot_z*d2r)));
                Mat4 trans=mat4_translate(p->pos.x,p->pos.y,p->pos.z);
                Mat4 wmat=mat4_mul(trans,mat4_mul(sc2,mat4_mul(rot,liftM)));
                trimesh_apply_transform(&job->trimesh,wmat);
            }
            cgltf_free(gltf);
        }else{
            ufbx_load_opts opts={0};
            opts.generate_missing_normals=true;
            opts.target_axes=ufbx_axes_right_handed_y_up;
            opts.target_unit_meters=1.0f;
            ufbx_error err;
            ufbx_scene *sc=ufbx_load_file(p->path,&opts,&err);
            if(!sc){
                fprintf(stderr,"[Stream] Load '%s': %s\n",p->path,err.description.data);
                /* Mark back to unloaded so it can be retried */
                SDL_LockMutex(ss->mutex);
                if(p->stream_state==PROP_QUEUED)p->stream_state=PROP_UNLOADED;
                SDL_UnlockMutex(ss->mutex);
                free(job);continue;
            }

            /* Compute Y bounds for ground-snap — transform through geometry_to_world
               so axis conversion is applied (raw .y is Z in Blender Z-up space)  */
            float ymn=1e9f,ymx=-1e9f;
            for(size_t ni=0;ni<sc->nodes.count;ni++){
                ufbx_node *nd=sc->nodes.data[ni];if(!nd->mesh)continue;
                for(size_t fi=0;fi<nd->mesh->num_faces;fi++){
                    ufbx_face face=nd->mesh->faces.data[fi];
                    for(size_t i=face.index_begin;i<face.index_begin+face.num_indices;i++){
                        ufbx_vec3 lp=ufbx_get_vertex_vec3(&nd->mesh->vertex_position,i);
                        ufbx_vec3 wp=ufbx_transform_position(&nd->geometry_to_world,lp);
                        float y=(float)wp.y;
                        if(y<ymn)ymn=y;if(y>ymx)ymx=y;
                    }
                }
            }
            job->ymin=ymn;

            /* Build CPU meshes */
            for(size_t ni=0;ni<sc->nodes.count&&job->num_meshes<MAX_MESHES;ni++){
                ufbx_node *nd=sc->nodes.data[ni];if(!nd->mesh)continue;
                job->meshes[job->num_meshes]=cpu_mesh_build(nd->mesh,nd,p->path);
                if(job->meshes[job->num_meshes].verts)job->num_meshes++;
            }

            if(p->coll_type==PROP_COLL_MESH){
                trimesh_build(&job->trimesh,sc);
                /* Build world matrix matching prop_world(p) with job->ymin */
                {
                    float d2r=(float)3.14159265f/180.0f;
                    float lift=-job->ymin*p->scale;
                    Mat4 liftM=mat4_translate(0,lift,0);
                    Mat4 sc2=mat4_scale_xyz(p->scale,p->scale,p->scale);
                    Mat4 rot=mat4_mul(mat4_rot_y(p->rot_y*d2r),mat4_mul(mat4_rot_x(p->rot_x*d2r),mat4_rot_z(p->rot_z*d2r)));
                    Mat4 trans=mat4_translate(p->pos.x,p->pos.y,p->pos.z);
                    Mat4 wmat=mat4_mul(trans,mat4_mul(sc2,mat4_mul(rot,liftM)));
                    trimesh_apply_transform(&job->trimesh,wmat);
                }
            }

            ufbx_free_scene(sc);
        }
        job->valid=true;

        /* Push to ready queue */
        SDL_LockMutex(ss->mutex);
        if(ss->rdy_count<STREAM_QUEUE_CAP){
            ss->ready[ss->rdy_tail%STREAM_QUEUE_CAP]=job;
            ss->rdy_tail++;ss->rdy_count++;
        } else {
            /* Ready queue full — shouldn't happen, but free safely */
            for(int i=0;i<job->num_meshes;i++){free(job->meshes[i].verts);free(job->meshes[i].inds);if(job->meshes[i].tex_mem)stbi_image_free(job->meshes[i].tex_mem);}
            free(job);
        }
        SDL_UnlockMutex(ss->mutex);
    }
    return 0;
}

static void stream_init(StreamSystem *ss, PropList *pl){
    memset(ss,0,sizeof(*ss));
    ss->props=pl;
    ss->mutex=SDL_CreateMutex();
    ss->work_sem=SDL_CreateSemaphore(0);
    ss->thread=SDL_CreateThread(stream_worker_thread,"PropStream",ss);
}

static void stream_shutdown(StreamSystem *ss){
    ss->quit=true;
    SDL_SemPost(ss->work_sem);
    int ret;SDL_WaitThread(ss->thread,&ret);
    SDL_DestroyMutex(ss->mutex);
    SDL_DestroySemaphore(ss->work_sem);
}

/* Queue a prop for background loading */
static void stream_request(StreamSystem *ss, int prop_idx){
    SDL_LockMutex(ss->mutex);
    if(ss->req_count<STREAM_QUEUE_CAP){
        ss->req[ss->req_tail%STREAM_QUEUE_CAP]=prop_idx;
        ss->req_tail++;ss->req_count++;
        ss->props->list[prop_idx].stream_state=PROP_QUEUED;
        SDL_SemPost(ss->work_sem);
    }
    SDL_UnlockMutex(ss->mutex);
}

static Mat4 prop_world(Prop *p); /* forward decl — defined below */
/* Upload one completed job to GPU — MAIN THREAD ONLY */
static void stream_gpu_upload(StreamJob *job){
    Prop *p=&g_stream.props->list[job->prop_idx];
    p->num_meshes=0;
    p->ymin=job->ymin;
    p->trimesh=job->trimesh; /* transfer ownership */

    /* Compute AABB here — p->ymin is now valid, and CPU verts still live.
       Uses prop_world(p) * cm->geometry_to_world, same as props_draw.    */
    if(p->coll_type==PROP_COLL_AABB){
        if(p->aabb_hw>0.0f){
            /* Manual AABB override */
            float ymx_raw=job->ymin; /* ymin is min; compute height from mesh */
            (void)ymx_raw;
            /* Just use the stored manual AABB params */
            p->aabb=job->aabb; /* set earlier in streaming thread for manual case */
        } else {
            float wxmn=1e9f,wymn=1e9f,wzmn=1e9f,wxmx=-1e9f,wymx=-1e9f,wzmx=-1e9f;
            Mat4 world=prop_world(p);
            for(int mi=0;mi<job->num_meshes;mi++){
                CpuMeshData *cm=&job->meshes[mi];
                Mat4 M=mat4_mul(world,cm->geometry_to_world);
                for(size_t vi=0;vi<cm->vc;vi++){
                    float vx=cm->verts[vi].pos[0];
                    float vy=cm->verts[vi].pos[1];
                    float vz=cm->verts[vi].pos[2];
                    float tx=M.m[0]*vx+M.m[4]*vy+M.m[8]*vz+M.m[12];
                    float ty=M.m[1]*vx+M.m[5]*vy+M.m[9]*vz+M.m[13];
                    float tz=M.m[2]*vx+M.m[6]*vy+M.m[10]*vz+M.m[14];
                    if(tx<wxmn)wxmn=tx;if(tx>wxmx)wxmx=tx;
                    if(ty<wymn)wymn=ty;if(ty>wymx)wymx=ty;
                    if(tz<wzmn)wzmn=tz;if(tz>wzmx)wzmx=tz;
                }
            }
            p->aabb.min=(Vec3){wxmn,wymn,wzmn};
            p->aabb.max=(Vec3){wxmx,wymx,wzmx};
        }
    }

    for(int i=0;i<job->num_meshes;i++){
        CpuMeshData *cm=&job->meshes[i];
        GpuMesh *gm=&p->meshes[p->num_meshes];
        memset(gm,0,sizeof(*gm));
        /* Upload vertices + indices */
        GLH_Attrib attrs[]={
            {0,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,pos)},
            {1,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,normal)},
            {2,2,GL_FLOAT,GL_FALSE,offsetof(Vertex,uv)},
            {3,4,GL_FLOAT,GL_FALSE,offsetof(Vertex,weights)},
            {4,4,GL_INT,0,offsetof(Vertex,bones)}
        };
        gm->vao=GLH_mesh(cm->verts,cm->vc*sizeof(Vertex),
                         cm->inds, cm->ic*sizeof(uint32_t),
                         sizeof(Vertex),attrs,5);
        gm->index_count=(GLsizei)cm->ic;
        gm->geometry_to_world=cm->geometry_to_world;
        gm->node_typed_id=cm->node_typed_id;
        memcpy(gm->cluster_map,cm->cluster_map,sizeof(gm->cluster_map));
        gm->num_gpu_bones=cm->num_gpu_bones;
        gm->mat_color[0]=cm->mat_color[0];
        gm->mat_color[1]=cm->mat_color[1];
        gm->mat_color[2]=cm->mat_color[2];
        /* Texture — GL calls, must be main thread */
        if(cm->tex_mem){
            gm->texture=GLH_texture_rgba_srgb(cm->tex_mem,cm->tex_w,cm->tex_h);
            stbi_image_free(cm->tex_mem);cm->tex_mem=NULL;
        } else if(cm->tex_path[0]){
            gm->texture=texture_file_try_candidates(cm->tex_path);
        }
        /* Free CPU data */
        free(cm->verts);cm->verts=NULL;
        free(cm->inds); cm->inds=NULL;
        p->num_meshes++;
    }
    p->stream_state=PROP_LOADED;
    free(job);
}

/* Free GPU data for one prop, keep definition for future reload */
static void prop_unload_gpu(Prop *p){
    for(int mi=0;mi<p->num_meshes;mi++){
        glDeleteVertexArrays(1,&p->meshes[mi].vao);
        if(p->meshes[mi].texture)glDeleteTextures(1,&p->meshes[mi].texture);
    }
    trimesh_free(&p->trimesh);
    p->num_meshes=0;
    p->stream_state=PROP_UNLOADED;
}

/* Per-frame streaming update — call from main loop */
static void props_stream_update(PropList *pl, Vec3 cam){
    /* 1. Drain the ready queue — upload to GPU (max STREAM_GPU_PER_FRAME) */
    int uploads=0;
    while(uploads<STREAM_GPU_PER_FRAME){
        StreamJob *job=NULL;
        SDL_LockMutex(g_stream.mutex);
        if(g_stream.rdy_count>0){
            job=g_stream.ready[g_stream.rdy_head%STREAM_QUEUE_CAP];
            g_stream.rdy_head++;g_stream.rdy_count--;
        }
        SDL_UnlockMutex(g_stream.mutex);
        if(!job)break;
        stream_gpu_upload(job);
        uploads++;
    }

    /* 2. Scan all registered props — load/unload based on distance */
    for(int i=0;i<pl->count;i++){
        Prop *p=&pl->list[i];
        if(!p->active)continue;
        float dx=p->pos.x-cam.x, dz=p->pos.z-cam.z;
        float d2=dx*dx+dz*dz;

        if(d2<=STREAM_LOAD_DIST*STREAM_LOAD_DIST){
            /* Want loaded */
            if(p->stream_state==PROP_UNLOADED)
                stream_request(&g_stream,i);
        } else if(d2>STREAM_UNLOAD_DIST*STREAM_UNLOAD_DIST){
            /* Want unloaded */
            if(p->stream_state==PROP_LOADED)
                prop_unload_gpu(p);
            /* If QUEUED — it will finish loading and sit READY/LOADED briefly
               before the next frame unloads it.  Small waste but safe. */
        }
    }
}


static GLuint prop_make_prog(void){
    return GLH_program(
        "#version 330 core\n"
        "layout(location=0)in vec3 aPos;\n"
        "layout(location=1)in vec3 aNormal;\n"
        "layout(location=2)in vec2 aUV;\n"
        "layout(location=3)in vec4 aWeights;\n"
        "layout(location=4)in ivec4 aBones;\n"
        "uniform mat4 uProj,uView,uModel;\n"
        "out vec3 vNormal,vPos;out vec2 vUV;\n"
        "void main(){vec4 w=uModel*vec4(aPos,1);gl_Position=uProj*uView*w;"
        "vNormal=normalize(mat3(transpose(inverse(uModel)))*aNormal);"
        "vPos=w.xyz;vUV=aUV;}\n",
        "#version 330 core\nin vec3 vNormal,vPos;in vec2 vUV;\n"
        "uniform sampler2D uAlbedo;uniform bool uHasTexture;uniform vec3 uBaseColor;\n"
        LIGHTING_UNIFORMS
        "out vec4 FragColor;\nvoid main(){\n"
        /* Full RGBA sample so foliage/leaf alpha masks work */
        "vec4 texSample=uHasTexture?texture(uAlbedo,vUV):vec4(uBaseColor,1.0);\n"
        /* Discard transparent leaf edges - stops solid-square artefacts */
        "if(texSample.a<0.15)discard;\n"
        /* Flip normal on back-faces so double-sided leaves light correctly */
        "vec3 n=normalize(gl_FrontFacing?vNormal:-vNormal);\n"
        "vec3 base=texSample.rgb;\n"
        "vec3 shadow_n=n;\n"
        LIGHTING_CALC
        "FragColor=vec4(col,1.0);}\n");
}

static Mat4 prop_world(Prop *p){
    float d2r=(float)M_PI/180.0f;
    /* Ground-snap: lift model up by -ymin*scale so its lowest vertex
       sits at pos.y. ufbx converts to Y-up so no flip needed.       */
    Mat4 lift =mat4_translate(0,-p->ymin*p->scale,0);
    Mat4 sc   =mat4_scale_xyz(p->scale,p->scale,p->scale);
    Mat4 rx   =mat4_rot_x(p->rot_x*d2r);
    Mat4 ry   =mat4_rot_y(p->rot_y*d2r);
    Mat4 rz   =mat4_rot_z(p->rot_z*d2r);
    Mat4 rot  =mat4_mul(ry,mat4_mul(rx,rz));
    Mat4 trans=mat4_translate(p->pos.x,p->pos.y,p->pos.z);
    return mat4_mul(trans,mat4_mul(sc,mat4_mul(rot,lift)));
}

/* Register a prop definition — no loading, no GPU work.
   The streaming system loads it when the player gets close. */
static bool prop_register(PropList *pl,const char *fbx_path,
                       float wx,float wy,float wz,
                       float rx,float ry,float rz,float scale,
                       PropCollType coll,
                       float aabb_cx,float aabb_cz,
                       float aabb_hw,float aabb_hd){
    if(pl->count>=MAX_PROPS){fprintf(stderr,"[Prop] MAX_PROPS reached\n");return false;}
    Prop *p=&pl->list[pl->count];memset(p,0,sizeof(Prop));
    strncpy(p->path,fbx_path,255);
    p->rot_x=rx;p->rot_y=ry;p->rot_z=rz;
    p->scale=(scale<=0.0f)?1.0f:scale;
    p->pos.x=wx;p->pos.y=(wy==0.0f)?terrain_h(wx,wz):wy;p->pos.z=wz;
    p->coll_type=coll;
    p->aabb_cx=aabb_cx;p->aabb_cz=aabb_cz;
    p->aabb_hw=aabb_hw;p->aabb_hd=aabb_hd;
    p->stream_state=PROP_UNLOADED;
    p->active=true;
    pl->count++;
    return true;
}

/* Legacy prop_place: immediately loads and uploads to GPU.
   Only used for props that MUST be present at game start
   (e.g., collision-critical objects near spawn).           */
static bool prop_place(PropList *pl,const char *fbx_path,
                       float wx,float wy,float wz,
                       float rx,float ry,float rz,float scale,
                       PropCollType coll,
                       float aabb_cx,float aabb_cz,
                       float aabb_hw,float aabb_hd){
    if(pl->count>=MAX_PROPS){fprintf(stderr,"[Prop] MAX_PROPS reached\n");return false;}
    Prop *p=&pl->list[pl->count];memset(p,0,sizeof(Prop));
    strncpy(p->path,fbx_path,255);
    if(path_has_extension_ci(fbx_path,".gltf")||path_has_extension_ci(fbx_path,".glb")){
        Scene tmp={0};
        if(!scene_load(&tmp,fbx_path)){fprintf(stderr,"[Prop] Load '%s': glTF failed\n",fbx_path);return false;}
        p->num_meshes=tmp.num_meshes;
        for(int i=0;i<tmp.num_meshes;i++)p->meshes[i]=tmp.meshes[i];
        tmp.num_meshes=0;
        p->ymin=tmp.ymin_raw;
        p->rot_x=rx; p->rot_y=ry; p->rot_z=rz;
        p->scale=(scale<=0.0f)?1.0f:scale;
        p->pos.x=wx; p->pos.y=(wy==0.0f)?terrain_h(wx,wz):wy; p->pos.z=wz;
        p->coll_type=coll;
        if(coll==PROP_COLL_AABB){
            if(aabb_hw>0.0f){
                float h=(tmp.ymax_raw-tmp.ymin_raw)*p->scale;
                p->aabb.min=(Vec3){wx+aabb_cx-aabb_hw,p->pos.y,wz+aabb_cz-aabb_hd};
                p->aabb.max=(Vec3){wx+aabb_cx+aabb_hw,p->pos.y+h,wz+aabb_cz+aabb_hd};
            }else{
                float wxmn=1e9f,wymn=1e9f,wzmn=1e9f,wxmx=-1e9f,wymx=-1e9f,wzmx=-1e9f;
                Mat4 world=prop_world(p);
                for(size_t ni=0;ni<tmp.gltf->nodes_count;ni++){
                    cgltf_node *nd=&tmp.gltf->nodes[ni];if(!nd->mesh)continue;
                    cgltf_float nworld[16];cgltf_node_transform_world(nd,nworld);Mat4 g2w=mat4_from_cgltf(nworld);
                    Mat4 M=mat4_mul(world,g2w);
                    for(size_t pi=0;pi<nd->mesh->primitives_count;pi++){
                        const cgltf_attribute *pos_attr=gltf_find_attribute(&nd->mesh->primitives[pi],cgltf_attribute_type_position,0);
                        if(!pos_attr||!pos_attr->data)continue;
                        for(size_t vi=0;vi<pos_attr->data->count;vi++){
                            cgltf_float lp[3]={0};cgltf_accessor_read_float(pos_attr->data,vi,lp,3);
                            float tx=M.m[0]*lp[0]+M.m[4]*lp[1]+M.m[8]*lp[2]+M.m[12];
                            float ty=M.m[1]*lp[0]+M.m[5]*lp[1]+M.m[9]*lp[2]+M.m[13];
                            float tz=M.m[2]*lp[0]+M.m[6]*lp[1]+M.m[10]*lp[2]+M.m[14];
                            if(tx<wxmn)wxmn=tx;if(tx>wxmx)wxmx=tx;
                            if(ty<wymn)wymn=ty;if(ty>wymx)wymx=ty;
                            if(tz<wzmn)wzmn=tz;if(tz>wzmx)wzmx=tz;
                        }
                    }
                }
                p->aabb.min=(Vec3){wxmn,wymn,wzmn};p->aabb.max=(Vec3){wxmx,wymx,wzmx};
            }
        }else if(coll==PROP_COLL_MESH){
            trimesh_build_gltf(&p->trimesh,tmp.gltf);
            trimesh_apply_transform(&p->trimesh,prop_world(p));
        }
        p->active=true;
        p->stream_state=PROP_LOADED;
        scene_free(&tmp);
        pl->count++;
        printf("[Prop] '%s' at (%.1f,%.1f,%.1f) rot=(%.1f,%.1f,%.1f) scale=%.2f coll=%d\n",
               fbx_path,(double)wx,(double)p->pos.y,(double)wz,(double)rx,(double)ry,(double)rz,(double)p->scale,coll);
        return true;
    }
    ufbx_load_opts opts={0};opts.generate_missing_normals=true;
    opts.target_axes=ufbx_axes_right_handed_y_up;opts.target_unit_meters=1.0f;
    ufbx_error err;ufbx_scene *sc=ufbx_load_file(fbx_path,&opts,&err);
    if(!sc){fprintf(stderr,"[Prop] Load '%s': %s\n",fbx_path,err.description.data);return false;}
    /* GPU meshes */
    ufbx_skin_deformer *ds=NULL;
    for(size_t ni=0;ni<sc->nodes.count&&p->num_meshes<MAX_MESHES;ni++){
        ufbx_node *nd=sc->nodes.data[ni];if(!nd->mesh)continue;
        size_t mat_count=nd->mesh->materials.count;
        bool split_by_material=mat_count>1&&nd->mesh->face_material.count==nd->mesh->num_faces;
        size_t passes=split_by_material?mat_count:1;
        for(size_t part=0;part<passes&&p->num_meshes<MAX_MESHES;part++){
            if(build_gpu_mesh(&p->meshes[p->num_meshes],&ds,nd->mesh,nd,fbx_path,split_by_material?part:0))p->num_meshes++;
        }
    }
    /* Local Y bounds for Y-flip (needed for render transform) */
    float ymn=1e9f,ymx=-1e9f;
    for(size_t ni=0;ni<sc->nodes.count;ni++){
        ufbx_node *nd=sc->nodes.data[ni];if(!nd->mesh)continue;
        for(size_t fi=0;fi<nd->mesh->num_faces;fi++){
            ufbx_face face=nd->mesh->faces.data[fi];
            for(size_t i=face.index_begin;i<face.index_begin+face.num_indices;i++){
                ufbx_vec3 lp=ufbx_get_vertex_vec3(&nd->mesh->vertex_position,i);
                ufbx_vec3 wp=ufbx_transform_position(&nd->geometry_to_world,lp);
                float y=(float)wp.y;
                if(y<ymn)ymn=y;if(y>ymx)ymx=y;
            }
        }
    }
    p->ymin=ymn;
    p->rot_x=rx; p->rot_y=ry; p->rot_z=rz;
    p->scale=(scale<=0.0f)?1.0f:scale;
    p->pos.x=wx; p->pos.y=(wy==0.0f)?terrain_h(wx,wz):wy; p->pos.z=wz;
    p->coll_type=coll;
    if(coll==PROP_COLL_AABB){
        if(aabb_hw>0.0f){
            /* Manual override */
            float h=(-ymn+ymx)*p->scale;
            p->aabb.min=(Vec3){wx+aabb_cx-aabb_hw, p->pos.y,           wz+aabb_cz-aabb_hd};
            p->aabb.max=(Vec3){wx+aabb_cx+aabb_hw, p->pos.y+h,         wz+aabb_cz+aabb_hd};
        } else {
            /* Use prop_world(p) * nd->geometry_to_world per node — same as props_draw */
            Mat4 world=prop_world(p);
            float wxmn=1e9f,wymn=1e9f,wzmn=1e9f,wxmx=-1e9f,wymx=-1e9f,wzmx=-1e9f;
            for(size_t ni=0;ni<sc->nodes.count;ni++){
                ufbx_node *nd=sc->nodes.data[ni];if(!nd->mesh)continue;
                Mat4 g2w=mat4_from_ufbx(&nd->geometry_to_world);
                Mat4 M=mat4_mul(world,g2w);
                for(size_t fi=0;fi<nd->mesh->num_faces;fi++){
                    ufbx_face face=nd->mesh->faces.data[fi];
                    for(size_t i=face.index_begin;i<face.index_begin+face.num_indices;i++){
                        ufbx_vec3 lp=ufbx_get_vertex_vec3(&nd->mesh->vertex_position,i);
                        float vx=(float)lp.x,vy=(float)lp.y,vz=(float)lp.z;
                        float tx=M.m[0]*vx+M.m[4]*vy+M.m[8]*vz+M.m[12];
                        float ty=M.m[1]*vx+M.m[5]*vy+M.m[9]*vz+M.m[13];
                        float tz=M.m[2]*vx+M.m[6]*vy+M.m[10]*vz+M.m[14];
                        if(tx<wxmn)wxmn=tx;if(tx>wxmx)wxmx=tx;
                        if(ty<wymn)wymn=ty;if(ty>wymx)wymx=ty;
                        if(tz<wzmn)wzmn=tz;if(tz>wzmx)wzmx=tz;
                    }
                }
            }
            p->aabb.min=(Vec3){wxmn,wymn,wzmn};
            p->aabb.max=(Vec3){wxmx,wymx,wzmx};
        }
    } else if(coll==PROP_COLL_MESH){
        trimesh_build(&p->trimesh,sc);
        trimesh_apply_transform(&p->trimesh,prop_world(p));
    }
    p->active=true;
    p->stream_state=PROP_LOADED;  /* already on GPU — don't re-stream */
    ufbx_free_scene(sc);
    pl->count++;
    printf("[Prop] '%s' at (%.1f,%.1f,%.1f) rot=(%.1f,%.1f,%.1f) scale=%.2f coll=%d\n",
           fbx_path,(double)wx,(double)p->pos.y,(double)wz,
           (double)rx,(double)ry,(double)rz,(double)p->scale,coll);
    return true;
}

static void props_draw(PropList *pl,LightingSystem *ls,Mat4 proj,Mat4 view,Vec3 cam){
    if(!pl->count||!g_prop_prog)return;
    Mat4 pv=mat4_mul(proj,view);
    float planes[6][4];
    frustum_extract(planes,pv);
    /* Disable backface culling: foliage/leaf cards on FBX models are single-sided
       quads that must be visible from both faces. Alpha-discard in the shader
       handles the leaf outlines. Restored at end of function. */
    glDisable(GL_CULL_FACE);
    lighting_push(ls,g_prop_prog);GLH_begin(g_prop_prog,proj.m,view.m);
    for(int pi=0;pi<pl->count;pi++){
        Prop *p=&pl->list[pi];
        if(!p->active||p->stream_state!=PROP_LOADED)continue;
        float dx=p->pos.x-cam.x, dz=p->pos.z-cam.z;
        float d2=dx*dx+dz*dz;
        /* LOD cull: scale-dependent draw distance */
        float lod_r = (p->scale<1.0f)?150.0f:(p->scale<2.5f)?350.0f:600.0f;
        if(d2>lod_r*lod_r)continue;
        if(!frustum_sphere(planes,p->pos.x,p->pos.y,p->pos.z,30.0f*p->scale))continue;
        Mat4 world=prop_world(p);
        for(int mi=0;mi<p->num_meshes;mi++){GpuMesh *gm=&p->meshes[mi];
            /* Apply per-mesh geometry_to_world (contains ufbx axis conversion) */
            Mat4 mesh_mat=mat4_mul(world,gm->geometry_to_world);
            glUniformMatrix4fv(glGetUniformLocation(g_prop_prog,"uModel"),1,GL_FALSE,mesh_mat.m);
            if(gm->texture){GLH_bind_texture(g_prop_prog,"uAlbedo",gm->texture,0);GLH_bool(g_prop_prog,"uHasTexture",1);}
            else{GLH_bool(g_prop_prog,"uHasTexture",0);GLH_vec3(g_prop_prog,"uBaseColor",gm->mat_color[0],gm->mat_color[1],gm->mat_color[2]);}
            glBindVertexArray(gm->vao);glDrawElements(GL_TRIANGLES,gm->index_count,GL_UNSIGNED_INT,0);}
    }
    glEnable(GL_CULL_FACE);
}
static void props_shadow(PropList *pl,ShadowMap *sm,LightingSystem *ls,Vec3 cam){
    if(!pl->count)return;
    glUseProgram(sm->prog_depth);
    glUniformMatrix4fv(glGetUniformLocation(sm->prog_depth,"uLightVP"),1,GL_FALSE,ls->light_vp.m);
    for(int pi=0;pi<pl->count;pi++){Prop *p=&pl->list[pi];
        if(!p->active||p->stream_state!=PROP_LOADED)continue;
        float dx=p->pos.x-cam.x,dz=p->pos.z-cam.z;
        if(dx*dx+dz*dz>200.0f*200.0f)continue;
        Mat4 world=prop_world(p);
        for(int mi=0;mi<p->num_meshes;mi++){
            Mat4 mesh_mat=mat4_mul(world,p->meshes[mi].geometry_to_world);
            glUniformMatrix4fv(glGetUniformLocation(sm->prog_depth,"uModel"),1,GL_FALSE,mesh_mat.m);
            glBindVertexArray(p->meshes[mi].vao);glDrawElements(GL_TRIANGLES,p->meshes[mi].index_count,GL_UNSIGNED_INT,0);}
    }glBindVertexArray(0);
}
static void props_free(PropList *pl){
    for(int pi=0;pi<pl->count;pi++){Prop *p=&pl->list[pi];
        if(!p->active)continue;
        if(p->stream_state==PROP_LOADED){
            for(int mi=0;mi<p->num_meshes;mi++){glDeleteVertexArrays(1,&p->meshes[mi].vao);if(p->meshes[mi].texture)glDeleteTextures(1,&p->meshes[mi].texture);}
            trimesh_free(&p->trimesh);
        }
    }
}

/* ══════════════════════════════════════════════════════════════
   FLOW-FIELD NAVMESH
   256×256 grid over the 5km×5km world.  BFS from player each
   time the player steps into a new grid cell.  Obstacles come
   from prop AABBs.  Enemies sample the grid for chase direction.
   ══════════════════════════════════════════════════════════════ */
typedef struct{
    uint8_t dir[NAV_H][NAV_W]; /* 0-7 = move dir, NAV_BLOCKED = wall/unvisited */
    int     pcx, pcz;          /* last player cell that triggered rebuild */
    bool    dirty;
}NavGrid;
static NavGrid g_nav;
static bool    g_nav_ready=false;

/* 8-direction deltas: E NE N NW W SW S SE */
static const int NAV_DX[8]={1,1,0,-1,-1,-1, 0, 1};
static const int NAV_DZ[8]={0,1,1, 1, 0,-1,-1,-1};
/* Opposite direction index */
static const uint8_t NAV_OPP[8]={4,5,6,7,0,1,2,3};

static int nav_world_to_cx(float wx){ int c=(int)((wx+WORLD_HALF)/NAV_CELL); return c<0?0:c>=NAV_W?NAV_W-1:c; }
static int nav_world_to_cz(float wz){ int c=(int)((wz+WORLD_HALF)/NAV_CELL); return c<0?0:c>=NAV_H?NAV_H-1:c; }

/* Mark cells blocked by prop AABBs */
/* BFS flow-field rebuild — runs on main thread only, ~1ms for 256×256 */
static void nav_rebuild(Vec3 player_pos, PropList *pl){
    /* Two-step: build obstacle map, then BFS for flow directions.
       g_nav.dir stores the outgoing direction (0-7) toward player,
       or NAV_BLOCKED for walls.  Unvisited open cells are 0xFE     */
    static uint8_t obs[NAV_H][NAV_W];          /* obstacle map */
    memset(obs, 0, sizeof(obs));

    /* Mark obstacles from prop AABBs */
    for(int pi=0;pi<pl->count;pi++){
        Prop *p=&pl->list[pi];
        if(!p->active||p->stream_state!=PROP_LOADED||p->coll_type==PROP_COLL_NONE)continue;
        float mnx=p->aabb.min.x-NAV_CELL, mxx=p->aabb.max.x+NAV_CELL;
        float mnz=p->aabb.min.z-NAV_CELL, mxz=p->aabb.max.z+NAV_CELL;
        int x0=nav_world_to_cx(mnx),x1=nav_world_to_cx(mxx);
        int z0=nav_world_to_cz(mnz),z1=nav_world_to_cz(mxz);
        for(int zz=z0;zz<=z1;zz++) for(int xx=x0;xx<=x1;xx++) obs[zz][xx]=1;
    }

    /* Initialise direction map: walls=NAV_BLOCKED, open=0xFE (unvisited) */
    for(int z=0;z<NAV_H;z++)
        for(int x=0;x<NAV_W;x++)
            g_nav.dir[z][x]=obs[z][x]?NAV_BLOCKED:0xFE;

    /* BFS from player cell outward — stores "direction back to player" */
    static uint16_t qx[NAV_W*NAV_H], qz2[NAV_W*NAV_H];
    int head=0, tail=0;
    int sx=nav_world_to_cx(player_pos.x);
    int sz=nav_world_to_cz(player_pos.z);
    /* Force player cell open even if inside a prop */
    g_nav.dir[sz][sx]=0; /* sentinel: 'arrived' */
    qx[tail]=(uint16_t)sx; qz2[tail]=(uint16_t)sz;
    tail=(tail+1)%(NAV_W*NAV_H);

    while(head!=tail){
        int cx=(int)qx[head], cz=(int)qz2[head];
        head=(head+1)%(NAV_W*NAV_H);
        for(int d=0;d<8;d++){
            int nx=cx+NAV_DX[d], nz=cz+NAV_DZ[d];
            if(nx<0||nx>=NAV_W||nz<0||nz>=NAV_H) continue;
            if(g_nav.dir[nz][nx]!=0xFE) continue; /* already visited or wall */
            /* Direction stored = which way this cell should move to reach player */
            g_nav.dir[nz][nx]=NAV_OPP[d];
            qx[tail]=(uint16_t)nx; qz2[tail]=(uint16_t)nz;
            tail=(tail+1)%(NAV_W*NAV_H);
        }
    }
    /* Any cell still 0xFE is unreachable — mark blocked */
    for(int z=0;z<NAV_H;z++)
        for(int x=0;x<NAV_W;x++)
            if(g_nav.dir[z][x]==0xFE) g_nav.dir[z][x]=NAV_BLOCKED;

    g_nav.pcx=sx; g_nav.pcz=sz;
    g_nav.dirty=false;
    g_nav_ready=true;
}

/* Sample flow field — returns move direction for an entity at pos.
   Falls back to direct-to-player dir if no path found.            */
static Vec3 nav_sample(Vec3 pos, Vec3 player_pos){
    if(!g_nav_ready){
        Vec3 d=vec3_sub(player_pos,pos); float l=vec3_len(d);
        return l>0.01f?vec3_scale(d,1.0f/l):(Vec3){0,0,0};
    }
    int cx=nav_world_to_cx(pos.x);
    int cz=nav_world_to_cz(pos.z);
    uint8_t dir=g_nav.dir[cz][cx];
    if(dir==NAV_BLOCKED||dir>=8){
        /* No path — try neighbours or direct fallback */
        Vec3 d=vec3_sub(player_pos,pos); float l=vec3_len(d);
        return l>0.01f?vec3_scale(d,1.0f/l):(Vec3){0,0,0};
    }
    /* Blend toward flow direction (smooth steering) */
    Vec3 flow={(float)NAV_DX[dir],0,(float)NAV_DZ[dir]};
    /* Weight by how close the flow points toward actual player */
    Vec3 direct=vec3_sub(player_pos,pos);
    float dl=vec3_len(direct);
    if(dl<0.01f)return flow;
    direct=vec3_scale(direct,1.0f/dl);
    /* Blend: close to player → direct aim, far → flow field */
    float blend=clampf(1.0f - dl/80.0f, 0.0f, 1.0f);
    Vec3 result={(flow.x*(1.0f-blend)+direct.x*blend),
                  0,
                  (flow.z*(1.0f-blend)+direct.z*blend)};
    float rl=sqrtf(result.x*result.x+result.z*result.z);
    return rl>0.001f?(Vec3){result.x/rl,0,result.z/rl}:(Vec3){0,0,0};
}

/* ══════════════════════════════════════════════════════════════
   POST-PROCESSING STACK
   HDR FBO → bright-pass → gaussian bloom → tonemap + composite
   ══════════════════════════════════════════════════════════════ */
typedef struct{
    GLuint hdr_fbo;
    GLuint hdr_color;       /* GL_R11F_G11F_B10F HDR colour */
    GLuint hdr_depth_tex;   /* GL_DEPTH_COMPONENT24 as texture (SSAO reads it) */
    /* Bloom: two quarter-res FBOs for ping-pong blur */
    GLuint bloom_fbo[2];
    GLuint bloom_tex[2];
    /* SSAO: half-res occlusion + blur */
    GLuint ssao_fbo,  ssao_tex;
    GLuint ssao_blur_fbo, ssao_blur_tex;
    /* Shaders */
    GLuint bright_prog;
    GLuint blur_h_prog;
    GLuint blur_v_prog;
    GLuint tonemap_prog;
    GLuint ssao_prog;
    GLuint ssao_blur_prog;
    /* Fullscreen quad */
    GLuint quad_vao, quad_vbo;
    int    width, height;
    float  cam_near, cam_far;     /* updated each frame for SSAO */
    float  cam_aspect;            /* win_w/win_h */
    bool   ready;
}PostFX;
static PostFX g_pfx;

/* ── Planar Water Reflection ──────────────────────────────── */
typedef struct{
    GLuint fbo, tex;
    int    w, h;
    bool   ready;
}WaterReflect;
static WaterReflect g_reflect;

static void reflect_init(WaterReflect *wr, int w, int h){
    wr->w=w/2; wr->h=h/2;
    glGenFramebuffers(1,&wr->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER,wr->fbo);
    glGenTextures(1,&wr->tex);
    glBindTexture(GL_TEXTURE_2D,wr->tex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,wr->w,wr->h,0,GL_RGB,GL_UNSIGNED_BYTE,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,wr->tex,0);
    /* Depth renderbuffer for reflection pass */
    GLuint rbo; glGenRenderbuffers(1,&rbo);
    glBindRenderbuffer(GL_RENDERBUFFER,rbo);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,wr->w,wr->h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,rbo);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    wr->ready=true;
    printf("[Reflect] %dx%d reflection FBO created\n",wr->w,wr->h);
}

static GLuint pfx_make_shader(const char *vs, const char *fs){
    GLuint v=glCreateShader(GL_VERTEX_SHADER);glShaderSource(v,1,&vs,NULL);glCompileShader(v);
    GLuint f=glCreateShader(GL_FRAGMENT_SHADER);glShaderSource(f,1,&fs,NULL);glCompileShader(f);
    GLuint p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);
    glDeleteShader(v);glDeleteShader(f);return p;
}

static void pfx_make_fbo(GLuint *fbo, GLuint *tex, int w, int h){
    glGenFramebuffers(1,fbo); glBindFramebuffer(GL_FRAMEBUFFER,*fbo);
    glGenTextures(1,tex); glBindTexture(GL_TEXTURE_2D,*tex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_R11F_G11F_B10F,w,h,0,GL_RGB,GL_FLOAT,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,*tex,0);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
}

static void pfx_init(PostFX *pfx, int w, int h){
    pfx->width=w; pfx->height=h;
    /* HDR FBO */
    glGenFramebuffers(1,&pfx->hdr_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER,pfx->hdr_fbo);
    glGenTextures(1,&pfx->hdr_color);
    glBindTexture(GL_TEXTURE_2D,pfx->hdr_color);
    glTexImage2D(GL_TEXTURE_2D,0,GL_R11F_G11F_B10F,w,h,0,GL_RGB,GL_FLOAT,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,pfx->hdr_color,0);
    /* Depth as texture so SSAO can sample it */
    glGenTextures(1,&pfx->hdr_depth_tex);
    glBindTexture(GL_TEXTURE_2D,pfx->hdr_depth_tex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,w,h,0,GL_DEPTH_COMPONENT,GL_FLOAT,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,pfx->hdr_depth_tex,0);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    /* Bloom FBOs at quarter resolution */
    int bw=w/4<1?1:w/4, bh=h/4<1?1:h/4;
    pfx_make_fbo(&pfx->bloom_fbo[0],&pfx->bloom_tex[0],bw,bh);
    pfx_make_fbo(&pfx->bloom_fbo[1],&pfx->bloom_tex[1],bw,bh);
    /* SSAO half-res R8 FBOs */
    int sw=w/2<1?1:w/2, sh=h/2<1?1:h/2;
    {
        GLuint *fbo=&pfx->ssao_fbo, *tex=&pfx->ssao_tex;
        glGenFramebuffers(1,fbo);glBindFramebuffer(GL_FRAMEBUFFER,*fbo);
        glGenTextures(1,tex);glBindTexture(GL_TEXTURE_2D,*tex);
        glTexImage2D(GL_TEXTURE_2D,0,GL_R8,sw,sh,0,GL_RED,GL_UNSIGNED_BYTE,NULL);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,*tex,0);
        glBindFramebuffer(GL_FRAMEBUFFER,0);
    }
    {
        GLuint *fbo=&pfx->ssao_blur_fbo, *tex=&pfx->ssao_blur_tex;
        glGenFramebuffers(1,fbo);glBindFramebuffer(GL_FRAMEBUFFER,*fbo);
        glGenTextures(1,tex);glBindTexture(GL_TEXTURE_2D,*tex);
        glTexImage2D(GL_TEXTURE_2D,0,GL_R8,sw,sh,0,GL_RED,GL_UNSIGNED_BYTE,NULL);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,*tex,0);
        glBindFramebuffer(GL_FRAMEBUFFER,0);
    }
    /* Quad */
    float q[]={-1,-1, 1,-1, 1,1, -1,-1, 1,1, -1,1};
    glGenVertexArrays(1,&pfx->quad_vao); glGenBuffers(1,&pfx->quad_vbo);
    glBindVertexArray(pfx->quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER,pfx->quad_vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(q),q,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,8,0);
    glBindVertexArray(0);
    /* ── Shaders ── */
    const char *quad_vs=
        "#version 330 core\n"
        "layout(location=0)in vec2 aPos;\n"
        "out vec2 vUV;\n"
        "void main(){vUV=aPos*0.5+0.5;gl_Position=vec4(aPos,0,1);}\n";
    /* Bright-pass: extract pixels above luminance threshold */
    const char *bright_fs=
        "#version 330 core\n"
        "in vec2 vUV;\n"
        "uniform sampler2D uHDR;\n"
        "out vec3 FragColor;\n"
        "void main(){\n"
        "    vec3 c=texture(uHDR,vUV).rgb;\n"
        "    float lum=dot(c,vec3(0.2126,0.7152,0.0722));\n"
        "    FragColor=(lum>1.0)?c-1.0:vec3(0.0);\n"
        "}\n";
    /* Horizontal gaussian blur */
    const char *blur_h_fs=
        "#version 330 core\n"
        "in vec2 vUV;\n"
        "uniform sampler2D uTex;\n"
        "uniform vec2 uTexelSize;\n"
        "out vec3 FragColor;\n"
        "void main(){\n"
        "    float w[5]=float[](0.227027,0.194595,0.121622,0.054054,0.016216);\n"
        "    vec3 c=texture(uTex,vUV).rgb*w[0];\n"
        "    for(int i=1;i<5;i++){\n"
        "        c+=texture(uTex,vUV+vec2(uTexelSize.x*i,0)).rgb*w[i];\n"
        "        c+=texture(uTex,vUV-vec2(uTexelSize.x*i,0)).rgb*w[i];\n"
        "    }\n"
        "    FragColor=c;\n"
        "}\n";
    /* Vertical gaussian blur */
    const char *blur_v_fs=
        "#version 330 core\n"
        "in vec2 vUV;\n"
        "uniform sampler2D uTex;\n"
        "uniform vec2 uTexelSize;\n"
        "out vec3 FragColor;\n"
        "void main(){\n"
        "    float w[5]=float[](0.227027,0.194595,0.121622,0.054054,0.016216);\n"
        "    vec3 c=texture(uTex,vUV).rgb*w[0];\n"
        "    for(int i=1;i<5;i++){\n"
        "        c+=texture(uTex,vUV+vec2(0,uTexelSize.y*i)).rgb*w[i];\n"
        "        c+=texture(uTex,vUV-vec2(0,uTexelSize.y*i)).rgb*w[i];\n"
        "    }\n"
        "    FragColor=c;\n"
        "}\n";
    /* Tonemap + bloom + SSAO composite */
    const char *tonemap_fs=
        "#version 330 core\n"
        "in vec2 vUV;\n"
        "uniform sampler2D uHDR;\n"
        "uniform sampler2D uBloom;\n"
        "uniform sampler2D uSSAO;\n"
        "uniform float uBloomStr;\n"
        "uniform float uExposure;\n"
        "uniform float uSSAOStr;\n"
        "out vec4 FragColor;\n"
        "void main(){\n"
        "    vec3 hdr=texture(uHDR,vUV).rgb;\n"
        "    float ao=texture(uSSAO,vUV).r;\n"
        "    /* Apply SSAO to darken ambient areas */\n"
        "    float aoFactor=mix(1.0,ao,uSSAOStr);\n"
        "    hdr *= aoFactor;\n"
        "    hdr += texture(uBloom,vUV).rgb * uBloomStr;\n"
        "    hdr *= uExposure;\n"
        "    vec3 mapped = hdr / (hdr + vec3(1.0));\n"
        "    mapped = mix(mapped, mapped*mapped*(3.0-2.0*mapped), 0.25);\n"
        "    mapped = pow(clamp(mapped,0.0,1.0), vec3(1.0/2.2));\n"
        "    FragColor = vec4(mapped, 1.0);\n"
        "}\n";
    pfx->bright_prog  = pfx_make_shader(quad_vs, bright_fs);
    pfx->blur_h_prog  = pfx_make_shader(quad_vs, blur_h_fs);
    pfx->blur_v_prog  = pfx_make_shader(quad_vs, blur_v_fs);
    pfx->tonemap_prog = pfx_make_shader(quad_vs, tonemap_fs);

    /* ── SSAO shader ─────────────────────────────────────────────
       View-space position reconstructed analytically from linear
       depth + camera FOV, avoiding the inv_proj path entirely.
       For 60° vertical FOV: tanHalfFovY = tan(30°) ≈ 0.5774       */
    const char *ssao_fs=
        "#version 330 core\n"
        "in vec2 vUV;\n"
        "uniform sampler2D uDepth;\n"
        "uniform float uNear,uFar,uAspect;\n"
        "out float FragColor;\n"

        /* Linearise NDC depth to positive view-space distance */
        "float linZ(float d){\n"
        "    float nz=d*2.0-1.0;\n"
        "    return 2.0*uNear*uFar/(uFar+uNear-nz*(uFar-uNear));\n"
        "}\n"

        /* Reconstruct view-space pos from UV and linear depth.\n"
        "   OpenGL view space: camera looks down -Z, so z_vs=-linZ. */
        "vec3 vp(vec2 uv){\n"
        "    float d=texture(uDepth,uv).r;\n"
        "    if(d>=0.9999) return vec3(0,0,-uFar); /* sky/far plane */\n"
        "    float lz=linZ(d);\n"
        "    const float TAN_HALF_FOV=0.57735027; /* tan(30 deg) */\n"
        "    vec2 nxy=uv*2.0-1.0;\n"
        "    return vec3(nxy.x*lz*TAN_HALF_FOV*uAspect,\n"
        "                nxy.y*lz*TAN_HALF_FOV,\n"
        "                -lz); }\n"

        "void main(){\n"
        "    vec3 pos=vp(vUV);\n"
        "    /* Bail out on sky fragments */\n"
        "    if(pos.z<-uFar*0.98){ FragColor=1.0; return; }\n"
        "    /* View-space normal from centred differences */\n"
        "    vec2 ts=1.0/vec2(textureSize(uDepth,0));\n"
        "    vec3 px=vp(vUV+vec2(ts.x,0.0));\n"
        "    vec3 py=vp(vUV+vec2(0.0,ts.y));\n"
        "    vec3 n=normalize(cross(px-pos, py-pos));\n"
        "    /* Pseudo-random per-pixel rotation */\n"
        "    float rnd=fract(sin(dot(vUV,vec2(127.1,311.7)))*43758.5);\n"
        "    float cr=cos(rnd*6.2832),sr=sin(rnd*6.2832);\n"
        "    /* Radius in world units, clamped so it doesn't exceed scene */\n"
        "    float radius=clamp(1.2/abs(pos.z),0.002,0.06);\n"
        "    float ao=0.0;\n"
        "    vec2 kern[12]=vec2[12](\n"
        "        vec2( 0.5381, 0.1857),vec2( 0.1379, 0.2486),\n"
        "        vec2( 0.3372, 0.5679),vec2(-0.6999,-0.0451),\n"
        "        vec2( 0.0690,-0.1598),vec2( 0.7555, 0.2203),\n"
        "        vec2(-0.6868, 0.3818),vec2(-0.1312, 0.6315),\n"
        "        vec2(-0.2296,-0.7265),vec2( 0.5991,-0.4739),\n"
        "        vec2( 0.4171,-0.2306),vec2(-0.3420,-0.5568));\n"
        "    for(int i=0;i<12;i++){\n"
        "        vec2 k=vec2(kern[i].x*cr-kern[i].y*sr,\n"
        "                    kern[i].x*sr+kern[i].y*cr);\n"
        "        vec2 suv=clamp(vUV+k*radius, vec2(0.001), vec2(0.999));\n"
        "        vec3 sp=vp(suv);\n"
        "        if(sp.z<-uFar*0.98) continue; /* sky sample — skip */\n"
        "        vec3 sd=sp-pos;\n"
        "        float len=length(sd);\n"
        "        if(len<0.0001) continue;\n"
        "        float ang=max(dot(n,sd/len)-0.02,0.0);\n"
        "        /* Range check: fade out occlusion beyond ~2m */\n"
        "        float rng=1.0-smoothstep(0.0,2.0,len);\n"
        "        ao+=ang*rng;\n"
        "    }\n"
        "    float occ=clamp(ao/12.0*2.2,0.0,0.80);\n"
        "    FragColor=1.0-occ;\n"
        "}\n";

    /* SSAO blur: simple 4x4 box blur to smooth the half-res occlusion */
    const char *ssao_blur_fs=
        "#version 330 core\n"
        "in vec2 vUV;\n"
        "uniform sampler2D uAO;\n"
        "out float FragColor;\n"
        "void main(){\n"
        "    vec2 ts=1.0/vec2(textureSize(uAO,0));\n"
        "    float ao=0.0;\n"
        "    for(int x=-2;x<=2;x++)for(int y=-2;y<=2;y++)\n"
        "        ao+=texture(uAO,vUV+vec2(float(x),float(y))*ts).r;\n"
        "    FragColor=ao/25.0;\n"
        "}\n";

    pfx->ssao_prog      = pfx_make_shader(quad_vs, ssao_fs);
    pfx->ssao_blur_prog = pfx_make_shader(quad_vs, ssao_blur_fs);

    pfx->ready=true;
    printf("[PostFX] Init %dx%d, bloom %dx%d, SSAO %dx%d\n",w,h,bw,bh,w/2,h/2);
}

static void pfx_resize(PostFX *pfx, int w, int h){
    if(!pfx->ready||(pfx->width==w&&pfx->height==h))return;
    pfx->width=w; pfx->height=h;
    glBindTexture(GL_TEXTURE_2D,pfx->hdr_color);
    glTexImage2D(GL_TEXTURE_2D,0,GL_R11F_G11F_B10F,w,h,0,GL_RGB,GL_FLOAT,NULL);
    glBindTexture(GL_TEXTURE_2D,pfx->hdr_depth_tex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,w,h,0,GL_DEPTH_COMPONENT,GL_FLOAT,NULL);
    int bw=w/4<1?1:w/4, bh=h/4<1?1:h/4;
    for(int i=0;i<2;i++){
        glBindTexture(GL_TEXTURE_2D,pfx->bloom_tex[i]);
        glTexImage2D(GL_TEXTURE_2D,0,GL_R11F_G11F_B10F,bw,bh,0,GL_RGB,GL_FLOAT,NULL);
    }
    int sw=w/2<1?1:w/2, sh=h/2<1?1:h/2;
    glBindTexture(GL_TEXTURE_2D,pfx->ssao_tex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_R8,sw,sh,0,GL_RED,GL_UNSIGNED_BYTE,NULL);
    glBindTexture(GL_TEXTURE_2D,pfx->ssao_blur_tex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_R8,sw,sh,0,GL_RED,GL_UNSIGNED_BYTE,NULL);
    glBindTexture(GL_TEXTURE_2D,0);
}

static void pfx_run_quad(PostFX *pfx){ glBindVertexArray(pfx->quad_vao);glDrawArrays(GL_TRIANGLES,0,6);glBindVertexArray(0); }

/* Run the full post-processing pipeline.
   Call AFTER all 3D rendering, BEFORE HUD rendering.
   Reads from hdr_fbo, writes to default framebuffer (FB 0).       */
static void pfx_composite(PostFX *pfx, int win_w, int win_h){
    if(!pfx->ready)return;
    int bw=win_w/4<1?1:win_w/4, bh=win_h/4<1?1:win_h/4;
    int sw=win_w/2<1?1:win_w/2, sh=win_h/2<1?1:win_h/2;
    glDisable(GL_DEPTH_TEST);

    /* 1. SSAO pass — half res, linear depth reconstruction */
    glBindFramebuffer(GL_FRAMEBUFFER,pfx->ssao_fbo);
    glViewport(0,0,sw,sh);
    glClearColor(1,1,1,1); glClear(GL_COLOR_BUFFER_BIT); /* white=no occlusion */
    glUseProgram(pfx->ssao_prog);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,pfx->hdr_depth_tex);
    glUniform1i(glGetUniformLocation(pfx->ssao_prog,"uDepth"),0);
    glUniform1f(glGetUniformLocation(pfx->ssao_prog,"uNear"),pfx->cam_near);
    glUniform1f(glGetUniformLocation(pfx->ssao_prog,"uFar"),pfx->cam_far);
    glUniform1f(glGetUniformLocation(pfx->ssao_prog,"uAspect"),pfx->cam_aspect);
    pfx_run_quad(pfx);

    /* 2. SSAO blur */
    glBindFramebuffer(GL_FRAMEBUFFER,pfx->ssao_blur_fbo);
    glClearColor(1,1,1,1); glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(pfx->ssao_blur_prog);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,pfx->ssao_tex);
    glUniform1i(glGetUniformLocation(pfx->ssao_blur_prog,"uAO"),0);
    pfx_run_quad(pfx);

    /* 3. Bloom bright pass: HDR → bloom[0] */
    glBindFramebuffer(GL_FRAMEBUFFER,pfx->bloom_fbo[0]);
    glViewport(0,0,bw,bh);
    glUseProgram(pfx->bright_prog);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,pfx->hdr_color);
    glUniform1i(glGetUniformLocation(pfx->bright_prog,"uHDR"),0);
    pfx_run_quad(pfx);

    /* 4. Bloom blur H → V × 2 */
    glBindFramebuffer(GL_FRAMEBUFFER,pfx->bloom_fbo[1]);
    glUseProgram(pfx->blur_h_prog);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,pfx->bloom_tex[0]);
    glUniform1i(glGetUniformLocation(pfx->blur_h_prog,"uTex"),0);
    glUniform2f(glGetUniformLocation(pfx->blur_h_prog,"uTexelSize"),1.0f/bw,0);
    pfx_run_quad(pfx);

    glBindFramebuffer(GL_FRAMEBUFFER,pfx->bloom_fbo[0]);
    glUseProgram(pfx->blur_v_prog);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,pfx->bloom_tex[1]);
    glUniform1i(glGetUniformLocation(pfx->blur_v_prog,"uTex"),0);
    glUniform2f(glGetUniformLocation(pfx->blur_v_prog,"uTexelSize"),0,1.0f/bh);
    pfx_run_quad(pfx);

    glBindFramebuffer(GL_FRAMEBUFFER,pfx->bloom_fbo[1]);
    glUseProgram(pfx->blur_h_prog);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,pfx->bloom_tex[0]);
    glUniform2f(glGetUniformLocation(pfx->blur_h_prog,"uTexelSize"),1.0f/bw,0);
    pfx_run_quad(pfx);

    glBindFramebuffer(GL_FRAMEBUFFER,pfx->bloom_fbo[0]);
    glUseProgram(pfx->blur_v_prog);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,pfx->bloom_tex[1]);
    glUniform2f(glGetUniformLocation(pfx->blur_v_prog,"uTexelSize"),0,1.0f/bh);
    pfx_run_quad(pfx);

    /* 5. Tonemap + SSAO + bloom composite → default FB */
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glViewport(0,0,win_w,win_h);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(pfx->tonemap_prog);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,pfx->hdr_color);
    glUniform1i(glGetUniformLocation(pfx->tonemap_prog,"uHDR"),0);
    glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,pfx->bloom_tex[0]);
    glUniform1i(glGetUniformLocation(pfx->tonemap_prog,"uBloom"),1);
    glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,pfx->ssao_blur_tex);
    glUniform1i(glGetUniformLocation(pfx->tonemap_prog,"uSSAO"),2);
    glUniform1f(glGetUniformLocation(pfx->tonemap_prog,"uBloomStr"),
                g_options.bloom ? BLOOM_STRENGTH : 0.0f);
    glUniform1f(glGetUniformLocation(pfx->tonemap_prog,"uExposure"),1.0f);
    glUniform1f(glGetUniformLocation(pfx->tonemap_prog,"uSSAOStr"),0.45f);
    pfx_run_quad(pfx);

    glEnable(GL_DEPTH_TEST);
    glUseProgram(0);
}



/* Draw AABB wireframes and trimesh triangles for all props (F3 mode) */

static void shadow_scene(ShadowMap *sm,LightingSystem *ls,Scene *sc,Mat4 wm,int ai,double at){
    if(!scene_has_data(sc))return; /* no mesh loaded — skip silently */
    glUseProgram(sm->prog_skin_depth);
    GLint loc_lvp=glGetUniformLocation(sm->prog_skin_depth,"uLightVP");
    GLint loc_mod=glGetUniformLocation(sm->prog_skin_depth,"uModel");
    GLint loc_bon=glGetUniformLocation(sm->prog_skin_depth,"uBones[0]");
    glUniformMatrix4fv(loc_lvp,1,GL_FALSE,ls->light_vp.m);
    ufbx_scene *eval=NULL;
    if(sc->format==SCENE_FORMAT_UFBX&&sc->scene&&sc->scene->anim_stacks.count>0&&ai>=0){
        int idx=ai<(int)sc->scene->anim_stacks.count?ai:(int)sc->scene->anim_stacks.count-1;
        ufbx_anim_stack *stk=sc->scene->anim_stacks.data[idx];
        double real_end=anim_stack_real_end(stk);
        double dur=real_end-stk->time_begin;double t=dur>0?fmod(at,dur):0;
        eval=ufbx_evaluate_scene(sc->scene,stk->anim,stk->time_begin+t,NULL,NULL);
    } else if(sc->format==SCENE_FORMAT_GLTF)scene_gltf_evaluate(sc,ai,at);
    Mat4 bone_mats[MAX_BONES];
    for(int mi=0;mi<sc->num_meshes;mi++){
        GpuMesh *gm=&sc->meshes[mi];Mat4 mm;
        if(sc->format==SCENE_FORMAT_UFBX&&sc->skins[mi])mm=wm;
        else if(sc->format==SCENE_FORMAT_GLTF&&gm->num_gpu_bones>0)mm=wm;
        else if(eval&&sc->format==SCENE_FORMAT_UFBX&&gm->node_typed_id<eval->nodes.count){ufbx_matrix g2w=eval->nodes.data[gm->node_typed_id]->geometry_to_world;mm=mat4_mul(wm,mat4_from_ufbx(&g2w));}
        else if(sc->format==SCENE_FORMAT_GLTF&&sc->gltf_eval_world&&gm->node_typed_id<(uint32_t)sc->gltf_node_count)mm=mat4_mul(wm,sc->gltf_eval_world[gm->node_typed_id]);
        else mm=mat4_mul(wm,gm->geometry_to_world);
        glUniformMatrix4fv(loc_mod,1,GL_FALSE,mm.m);
        for(int i=0;i<MAX_BONES;i++)bone_mats[i]=mat4_identity();
        if(sc->format==SCENE_FORMAT_GLTF)compute_bones_gltf(bone_mats,gm->num_gpu_bones,gm,sc);
        else compute_bones(bone_mats,gm->num_gpu_bones,gm,sc->skins[mi],eval);
        glUniformMatrix4fv(loc_bon,MAX_BONES,GL_FALSE,bone_mats[0].m);
        glBindVertexArray(gm->vao);glDrawElements(GL_TRIANGLES,gm->index_count,GL_UNSIGNED_INT,0);
    }
    if(eval)ufbx_free_scene(eval);
    glBindVertexArray(0);
}

static void shadow_charmodel(ShadowMap *sm,LightingSystem *ls,CharModel *cm,Mat4 wm,int anim_slot,double at){
    if(!cm||!scene_has_data(&cm->base))return; /* model not loaded — skip */
    if(anim_slot<0||anim_slot>=ANIM_SLOTS)anim_slot=ANIM_IDLE;
    AnimEntry *ae=&cm->anims[anim_slot];
    Scene *sc=ae->scene?ae->scene:&cm->base;
    shadow_scene(sm,ls,sc,wm,ae->scene?ae->stack:-1,at);
}

/* ── Cached-eval variants ─────────────────────────────────────────
   Cache the ufbx_scene* eval pointer (evaluated once per throttle
   interval). Both shadow and draw passes call compute_bones per
   mesh normally — fixing the per-mesh bone index bug — but only
   one ufbx_evaluate_scene call happens per throttle window.        */

static void draw_scene_cached(SkinProg *sp, Scene *sc, Mat4 wm,
                              ufbx_scene *eval, float flash, Vec3 cam_pos,
                              Mat4 proj, Mat4 view, LightingSystem *ls){
    if(!scene_has_data(sc))return;
    lighting_push(ls,sp->prog);
    GLH_begin(sp->prog,proj.m,view.m);
    GLH_float(sp->prog,"uFlash",flash);
    GLH_vec3(sp->prog,"uCamPos",cam_pos.x,cam_pos.y,cam_pos.z);
    Mat4 bone_mats[MAX_BONES];
    for(int mi=0;mi<sc->num_meshes;mi++){
        GpuMesh *gm=&sc->meshes[mi];
        Mat4 mesh_mat;
        if(sc->format==SCENE_FORMAT_UFBX&&sc->skins[mi]) mesh_mat=wm;
        else if(sc->format==SCENE_FORMAT_GLTF&&gm->num_gpu_bones>0) mesh_mat=wm;
        else if(eval&&sc->format==SCENE_FORMAT_UFBX&&gm->node_typed_id<eval->nodes.count){
            ufbx_matrix g2w=eval->nodes.data[gm->node_typed_id]->geometry_to_world;
            mesh_mat=mat4_mul(wm,mat4_from_ufbx(&g2w));
        }else if(sc->format==SCENE_FORMAT_GLTF&&sc->gltf_eval_world&&gm->node_typed_id<(uint32_t)sc->gltf_node_count) mesh_mat=mat4_mul(wm,sc->gltf_eval_world[gm->node_typed_id]);
        else mesh_mat=mat4_mul(wm,gm->geometry_to_world);
        glUniformMatrix4fv(sp->loc_model,1,GL_FALSE,mesh_mat.m);
        if(gm->texture){GLH_bind_texture(sp->prog,"uAlbedo",gm->texture,0);GLH_bool(sp->prog,"uHasTexture",1);}
        else GLH_bool(sp->prog,"uHasTexture",0);
        if(gm->normal_tex){GLH_bind_texture(sp->prog,"uNormalTex",gm->normal_tex,1);GLH_bool(sp->prog,"uHasNormalTex",1);} else GLH_bool(sp->prog,"uHasNormalTex",0);
        if(gm->mr_tex){GLH_bind_texture(sp->prog,"uMRTex",gm->mr_tex,2);GLH_bool(sp->prog,"uHasMRTex",1);} else GLH_bool(sp->prog,"uHasMRTex",0);
        if(gm->emissive_tex){GLH_bind_texture(sp->prog,"uEmissiveTex",gm->emissive_tex,3);GLH_bool(sp->prog,"uHasEmissiveTex",1);} else GLH_bool(sp->prog,"uHasEmissiveTex",0);
        GLH_vec4(sp->prog,"uBaseColor",gm->mat_color[0],gm->mat_color[1],gm->mat_color[2],gm->mat_color[3]);
        GLH_vec3(sp->prog,"uEmissiveFactor",gm->emissive[0],gm->emissive[1],gm->emissive[2]);
        GLH_float(sp->prog,"uMetallic",gm->metallic);
        GLH_float(sp->prog,"uRoughness",gm->roughness);
        GLH_float(sp->prog,"uAlphaCutoff",gm->alpha_cutoff);
        for(int i=0;i<MAX_BONES;i++) bone_mats[i]=mat4_identity();
        if(sc->format==SCENE_FORMAT_GLTF)compute_bones_gltf(bone_mats,gm->num_gpu_bones,gm,sc);
        else compute_bones(bone_mats,gm->num_gpu_bones,gm,sc->skins[mi],eval);
        glUniformMatrix4fv(sp->loc_bones,MAX_BONES,GL_FALSE,bone_mats[0].m);
        glBindVertexArray(gm->vao);
        glDrawElements(GL_TRIANGLES,gm->index_count,GL_UNSIGNED_INT,0);
    }
}

static void shadow_scene_cached(ShadowMap *sm, LightingSystem *ls,
                                Scene *sc, Mat4 wm, ufbx_scene *eval){
    if(!scene_has_data(sc))return;
    glUseProgram(sm->prog_skin_depth);
    GLint loc_lvp=glGetUniformLocation(sm->prog_skin_depth,"uLightVP");
    GLint loc_mod=glGetUniformLocation(sm->prog_skin_depth,"uModel");
    GLint loc_bon=glGetUniformLocation(sm->prog_skin_depth,"uBones[0]");
    glUniformMatrix4fv(loc_lvp,1,GL_FALSE,ls->light_vp.m);
    Mat4 bone_mats[MAX_BONES];
    for(int mi=0;mi<sc->num_meshes;mi++){
        GpuMesh *gm=&sc->meshes[mi]; Mat4 mm;
        if(sc->format==SCENE_FORMAT_UFBX&&sc->skins[mi]) mm=wm;
        else if(sc->format==SCENE_FORMAT_GLTF&&gm->num_gpu_bones>0) mm=wm;
        else if(eval&&sc->format==SCENE_FORMAT_UFBX&&gm->node_typed_id<eval->nodes.count){
            ufbx_matrix g2w=eval->nodes.data[gm->node_typed_id]->geometry_to_world;
            mm=mat4_mul(wm,mat4_from_ufbx(&g2w));
        }else if(sc->format==SCENE_FORMAT_GLTF&&sc->gltf_eval_world&&gm->node_typed_id<(uint32_t)sc->gltf_node_count) mm=mat4_mul(wm,sc->gltf_eval_world[gm->node_typed_id]);
        else mm=mat4_mul(wm,gm->geometry_to_world);
        glUniformMatrix4fv(loc_mod,1,GL_FALSE,mm.m);
        for(int i=0;i<MAX_BONES;i++) bone_mats[i]=mat4_identity();
        if(sc->format==SCENE_FORMAT_GLTF)compute_bones_gltf(bone_mats,gm->num_gpu_bones,gm,sc);
        else compute_bones(bone_mats,gm->num_gpu_bones,gm,sc->skins[mi],eval);
        glUniformMatrix4fv(loc_bon,MAX_BONES,GL_FALSE,bone_mats[0].m);
        glBindVertexArray(gm->vao);
        glDrawElements(GL_TRIANGLES,gm->index_count,GL_UNSIGNED_INT,0);
    }
    glBindVertexArray(0);
}

/* Refresh cached eval for one enemy (Fix 1+3).
   Throttle by distance — every 1/2/3 frames for near/mid/far.
   Caller frees e->anim_eval after all rendering.                    */
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
        "uniform vec3 uSunDir;\n"
        "uniform float uSunElev,uOvercast;\n"
        "out vec4 F;\n"
        "void main(){\n"
        "    vec3 d=normalize(vDir);\n"
        "    if(uHasTex){\n"
        "        float u=0.5+atan(d.z,d.x)/(2.0*PI);\n"
        "        float v=0.5-asin(clamp(d.y,-1,1))/PI;\n"
        "        vec4 tc=texture(uSkyTex,vec2(u,v));\n"
        "        F=vec4(mix(tc.rgb,vec3(0.55,0.57,0.60),uOvercast),1.0);return;}\n"
        "    float h=clamp(d.y,0.0,1.0);\n"
        "    float day_t=clamp(uSunElev*3.0+0.3,0.0,1.0);\n"
        "    float dawn_t=clamp(1.0-abs(uSunElev)*5.0,0.0,1.0);\n"
        "    vec3 night_z=vec3(0.01,0.01,0.06), night_h=vec3(0.03,0.03,0.10);\n"
        "    vec3 day_z  =vec3(0.10,0.35,0.85), day_h  =vec3(0.55,0.75,0.95);\n"
        "    vec3 dawn_h =vec3(0.95,0.45,0.10);\n"
        "    vec3 zenith =mix(night_z,day_z,day_t);\n"
        "    vec3 horiz  =mix(night_h,mix(day_h,dawn_h,dawn_t*0.9),day_t+dawn_t*0.4);\n"
        "    float sky_t =clamp(h*2.5,0.0,1.0);\n"
        "    vec3 sky=mix(horiz,zenith,sky_t);\n"
        "    if(uSunElev>-0.15){\n"
        "        float sd=dot(d,uSunDir);\n"
        /* sun disk and glow fade out as overcast rises */
        "        float glow=clamp(pow(max(sd,0.0),6.0)*0.6,0.0,1.0)*(1.0-uOvercast);\n"
        "        float disk=(sd>0.9995&&uOvercast<0.3)?1.0:0.0;\n"
        "        vec3 sc=mix(vec3(1.0,0.55,0.15),vec3(1.0,0.97,0.88),clamp(uSunElev*3.0,0,1));\n"
        "        sky+=sc*glow*clamp(uSunElev+0.2,0.0,1.0);\n"
        "        sky=mix(sky,sc*1.8,disk);}\n"
        "    if(uSunElev<0.1){\n"
        "        vec3 md=-uSunDir;\n"
        "        float msd=dot(d,md);\n"
        "        float mglow=clamp(pow(max(msd,0.0),20.0)*0.2,0.0,1.0)*(1.0-uOvercast);\n"
        "        float mdisk=(msd>0.9998&&uOvercast<0.3)?1.0:0.0;\n"
        "        float nt=clamp(-uSunElev*2.0,0.0,1.0);\n"
        "        sky+=vec3(0.85,0.88,0.95)*mdisk+vec3(0.3,0.33,0.45)*mglow*nt;}\n"
        /* blend toward flat overcast grey */
        "    vec3 overcast_col=mix(vec3(0.38,0.40,0.42),vec3(0.55,0.57,0.60),h);\n"
        "    sky=mix(sky,overcast_col,uOvercast);\n"
        "    F=vec4(sky,1.0);}\n");
}
static void skybox_draw(Skybox *sky,Mat4 proj,Mat4 view,LightingSystem *ls,float overcast){
    glDepthFunc(GL_LEQUAL);glDepthMask(GL_FALSE);GLH_begin(sky->prog,proj.m,view.m);
    Vec3 sun_up={-ls->sun_dir.x,-ls->sun_dir.y,-ls->sun_dir.z};
    GLH_vec3(sky->prog,"uSunDir",sun_up.x,sun_up.y,sun_up.z);
    GLH_float(sky->prog,"uSunElev",ls->sun_elev);
    GLH_float(sky->prog,"uOvercast",overcast);
    if(sky->has_tex){GLH_bind_texture(sky->prog,"uSkyTex",sky->tex,0);GLH_bool(sky->prog,"uHasTex",1);}else GLH_bool(sky->prog,"uHasTex",0);
    GLH_draw(sky->vao,GL_TRIANGLES,36);glDepthMask(GL_TRUE);glDepthFunc(GL_LESS);
}


/* ── Particles ──────────────────────────────────────────────── */
typedef enum{PT_NORMAL=0,PT_FIRE=1,PT_MAGIC=2,PT_DUST=3}ParticleType;
typedef struct{Vec3 pos,vel;float life,max_life,size,r,g,b,a;
    ParticleType type; int frame; bool active;}Particle;
typedef struct{Vec3 pos;float rate,accum,r,g,b,size,speed;bool active;ParticleType spawn_type;}Emitter;
typedef struct{Particle particles[MAX_PARTICLES];Emitter emitters[MAX_EMITTERS];
    int pcount,ecount;
    GLuint prog;       /* normal soft-circle GL_POINTS */
    GLuint fire_prog;  /* animated 4×4 sprite sheet fire */
    GLuint magic_prog; /* large additive ring glow */
    GLuint vao,vbo;
    float  total_time; /* accumulated time for frame animation */
}ParticleSystem;

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

    /* ── Fire sprite shader ─────────────────────────────────
       Animated 4×4 sprite-sheet (16 frames) — frame cycles based
       on uTime. Warm hue shift from orange→yellow as particle ages. */
    ps->fire_prog=GLH_program(
        "#version 330 core\n"
        "layout(location=0)in vec3 aPos;\n"
        "layout(location=1)in vec4 aColor;\n"
        "layout(location=2)in float aSize;\n"
        "uniform mat4 uProj,uView;\n"
        "out vec4 vColor;\n"
        "out float vAge;\n"
        "void main(){\n"
        "    gl_Position=uProj*uView*vec4(aPos,1);\n"
        "    gl_PointSize=aSize*280.0/gl_Position.w;\n"
        "    vColor=aColor; vAge=aColor.a;}\n",
        "#version 330 core\n"
        "in vec4 vColor;in float vAge;\n"
        "uniform float uTime;\n"
        "out vec4 F;\n"
        "void main(){\n"
        /* Sprite sheet 4x4: pick frame from gl_PointCoord */
        "    int frame=int(uTime*12.0)%16;\n"
        "    float fx=float(frame%4)/4.0, fy=float(frame/4)/4.0;\n"
        "    vec2 suv=gl_PointCoord/4.0+vec2(fx,fy);\n"
        /* Soft circle within the sprite cell */
        "    vec2 uv=fract(gl_PointCoord*4.0)*2.0-1.0;\n"
        "    float d=dot(uv,uv);\n"
        "    if(d>1.0) discard;\n"
        /* Warm hue: orange core → yellow tip, darkens as age fades */
        "    float age=vAge;\n"
        "    vec3 col=mix(vec3(1.0,0.2,0.0),vec3(1.0,0.85,0.1),age)*vColor.rgb*1.4;\n"
        "    float a=age*(1.0-d*0.6);\n"
        "    F=vec4(col, a*0.85);}\n");

    /* ── Magic glow shader ──────────────────────────────────
       Large additive ring: bright core + coloured outer ring.    */
    ps->magic_prog=GLH_program(
        "#version 330 core\n"
        "layout(location=0)in vec3 aPos;\n"
        "layout(location=1)in vec4 aColor;\n"
        "layout(location=2)in float aSize;\n"
        "uniform mat4 uProj,uView;\n"
        "out vec4 vColor;\n"
        "void main(){\n"
        "    gl_Position=uProj*uView*vec4(aPos,1);\n"
        "    gl_PointSize=aSize*350.0/gl_Position.w;\n"
        "    vColor=aColor;}\n",
        "#version 330 core\n"
        "in vec4 vColor;\n"
        "out vec4 F;\n"
        "void main(){\n"
        "    vec2 uv=gl_PointCoord*2.0-1.0;\n"
        "    float d=dot(uv,uv);\n"
        "    if(d>1.0) discard;\n"
        /* Ring: bright thin ring + soft core */
        "    float ring=exp(-pow((d-0.5)*8.0,2.0))*0.9;\n"
        "    float core=exp(-d*6.0)*0.6;\n"
        "    float glow=clamp(ring+core,0.0,1.0);\n"
        "    F=vec4(vColor.rgb*1.8+0.2, glow*vColor.a);}\n");
}
static void ps_spawn_typed(ParticleSystem *ps,Vec3 pos,Vec3 vel,float life,float size,
                            float r,float g,float b,ParticleType type){
    for(int i=0;i<MAX_PARTICLES;i++){Particle *p=&ps->particles[i];if(p->active)continue;
        p->pos=pos;p->vel=vel;p->life=p->max_life=life;
        p->size=size;p->r=r;p->g=g;p->b=b;p->a=1;
        p->type=type;p->frame=0;p->active=true;
        if(i>=ps->pcount)ps->pcount=i+1;return;}
}
static void ps_spawn(ParticleSystem *ps,Vec3 pos,Vec3 vel,float life,float size,float r,float g,float b){
    ps_spawn_typed(ps,pos,vel,life,size,r,g,b,PT_NORMAL);
}

/* Fire burst: 20 upward particles with warm colour + low gravity pull */
static void ps_fire_burst(ParticleSystem *ps,Vec3 base,float intensity){
    for(int i=0;i<20;i++){
        float ang=(float)rand()/RAND_MAX*2.0f*3.14159f;
        float spd=(0.4f+(float)rand()/RAND_MAX*0.6f)*intensity;
        Vec3 vel={cosf(ang)*spd*0.3f,
                  1.2f+(float)rand()/RAND_MAX*2.0f*intensity,
                  sinf(ang)*spd*0.3f};
        float life=0.5f+(float)rand()/RAND_MAX*0.7f;
        float sz  =0.10f+(float)rand()/RAND_MAX*0.10f;
        /* Orange-red: r=1, g=0.3-0.6, b=0 */
        float gr=0.3f+(float)rand()/RAND_MAX*0.3f;
        ps_spawn_typed(ps,base,vel,life,sz,1.0f,gr,0.0f,PT_FIRE);
    }
}

/* Magic hit burst: ring of glowing particles + small core shockwave */
static void ps_magic_burst(ParticleSystem *ps,Vec3 pos,float cr,float cg,float cb){
    /* Outer ring */
    for(int i=0;i<12;i++){
        float ang=(float)i/12.0f*2.0f*3.14159f;
        Vec3 vel={cosf(ang)*3.5f,0.5f+(float)rand()/RAND_MAX,sinf(ang)*3.5f};
        ps_spawn_typed(ps,pos,vel,0.35f,0.20f,cr,cg,cb,PT_MAGIC);
    }
    /* Bright core flash */
    for(int i=0;i<6;i++){
        float ang=(float)rand()/RAND_MAX*2.0f*3.14159f;
        Vec3 vel={cosf(ang)*1.0f,2.0f,sinf(ang)*1.0f};
        ps_spawn_typed(ps,pos,vel,0.20f,0.30f,1.0f,1.0f,1.0f,PT_MAGIC);
    }
}

/* Dust puff: soft grey particles drifting sideways — footsteps, landings */
static void ps_dust_puff(ParticleSystem *ps,Vec3 pos,int count){
    for(int i=0;i<count;i++){
        float ang=(float)rand()/RAND_MAX*2.0f*3.14159f;
        float spd=0.3f+(float)rand()/RAND_MAX*0.7f;
        Vec3 vel={cosf(ang)*spd,0.3f+(float)rand()/RAND_MAX*0.5f,sinf(ang)*spd};
        float grey=0.55f+(float)rand()/RAND_MAX*0.25f;
        ps_spawn_typed(ps,pos,vel,0.6f+(float)rand()/RAND_MAX*0.5f,
                       0.12f+(float)rand()/RAND_MAX*0.08f,grey,grey,grey,PT_DUST);
    }
}
static void ps_burst(ParticleSystem *ps,Vec3 pos,float r,float g,float b){
    for(int i=0;i<16;i++){float a=(float)i/16.0f*2.0f*(float)M_PI;Vec3 vel={cosf(a)*2.5f,3.0f+(float)rand()/RAND_MAX*2.0f,sinf(a)*2.5f};ps_spawn(ps,pos,vel,0.6f+(float)rand()/RAND_MAX*0.4f,0.12f+(float)rand()/RAND_MAX*0.08f,r,g,b);}
}

/* Blood splatter — dark red droplets spraying up and outward from pos.
   chest_h: approximate torso height offset above pos.y (e.g. 1.0 for enemies).
   count: number of particles (20 for a kill, 10 for a hit).              */
static void ps_blood(ParticleSystem *ps, Vec3 pos, float chest_h, int count){
    Vec3 origin={pos.x, pos.y+chest_h, pos.z};
    for(int i=0;i<count;i++){
        /* Random outward direction with strong upward bias */
        float yaw=(float)rand()/RAND_MAX*2.0f*(float)M_PI;
        float spd=3.0f+(float)rand()/RAND_MAX*5.0f;
        float vy =2.5f+(float)rand()/RAND_MAX*4.0f;
        Vec3 vel={cosf(yaw)*spd, vy, sinf(yaw)*spd};
        /* Vary hue: dark red → crimson */
        float rr=0.55f+(float)rand()/RAND_MAX*0.30f;
        float rg=0.00f+(float)rand()/RAND_MAX*0.08f;
        float rb=0.00f+(float)rand()/RAND_MAX*0.04f;
        float life=0.4f+(float)rand()/RAND_MAX*0.5f;
        float sz =0.10f+(float)rand()/RAND_MAX*0.12f;
        ps_spawn(ps,origin,vel,life,sz,rr,rg,rb);
    }
}
static int ps_add_emitter(ParticleSystem *ps,Vec3 pos,float rate,float r,float g,float b,float size,float speed){
    if(ps->ecount>=MAX_EMITTERS)return -1;
    int i=ps->ecount++;
    Emitter *em=&ps->emitters[i];
    *em=(Emitter){pos,rate,0,r,g,b,size,speed,true,PT_NORMAL};
    return i;
}
static int ps_add_emitter_typed(ParticleSystem *ps,Vec3 pos,float rate,float r,float g,float b,float size,float speed,ParticleType type){
    int i=ps_add_emitter(ps,pos,rate,r,g,b,size,speed);
    if(i>=0) ps->emitters[i].spawn_type=type;
    return i;
}
static void ps_update(ParticleSystem *ps,float dt){
    ps->total_time+=dt;
    for(int e=0;e<ps->ecount;e++){Emitter *em=&ps->emitters[e];if(!em->active)continue;em->accum+=dt;float inv=1.0f/em->rate;
        while(em->accum>=inv){em->accum-=inv;float ax=(float)(rand()%100-50)*0.01f,az=(float)(rand()%100-50)*0.01f;Vec3 vel={ax*em->speed,em->speed*(0.8f+(float)rand()/RAND_MAX*0.4f),az*em->speed};
            ps_spawn_typed(ps,em->pos,vel,0.5f+(float)rand()/RAND_MAX*0.5f,em->size*(0.8f+(float)rand()/RAND_MAX*0.4f),em->r,em->g,em->b,em->spawn_type);}}
    for(int i=0;i<ps->pcount;i++){Particle *p=&ps->particles[i];if(!p->active)continue;
        /* Fire: less gravity drag, more upward buoyancy */
        float grav=(p->type==PT_FIRE)?0.01f:0.05f;
        p->vel.y+=GRAVITY*grav*dt;
        if(p->type==PT_FIRE)p->vel.y+=1.8f*dt; /* buoyancy */
        p->pos=vec3_add(p->pos,vec3_scale(p->vel,dt));
        p->life-=dt;p->a=p->life/p->max_life;
        /* Dust grows slightly, fire shrinks faster */
        if(p->type==PT_DUST) p->size*=(1.0f+0.6f*dt);
        else p->size*=(p->type==PT_FIRE)?(1.0f-1.2f*dt):(1.0f-0.2f*dt);
        if(p->life<=0)p->active=false;}
    while(ps->pcount>0&&!ps->particles[ps->pcount-1].active) ps->pcount--;
}
static void ps_draw(ParticleSystem *ps,Mat4 proj,Mat4 view){
    /* We make 4 passes in type order to batch by shader.
       All use the same VAO/VBO — rebuild buffer each pass. */
    glEnable(GL_BLEND); glDepthMask(GL_FALSE); glEnable(GL_PROGRAM_POINT_SIZE);

    /* Helper macro: build buffer for a given type, draw if non-empty */
    float buf[MAX_PARTICLES*8];
#define PS_DRAW_TYPE(PROG, TYPE, BLEND_DST) do { \
    int n=0; \
    for(int i=0;i<ps->pcount;i++){Particle *p=&ps->particles[i]; \
        if(!p->active||p->type!=(TYPE))continue; \
        buf[n*8+0]=p->pos.x;buf[n*8+1]=p->pos.y;buf[n*8+2]=p->pos.z; \
        buf[n*8+3]=p->r;buf[n*8+4]=p->g;buf[n*8+5]=p->b; \
        buf[n*8+6]=p->a;buf[n*8+7]=p->size;n++;} \
    if(n>0){ \
        GLH_vbo_update(ps->vbo,buf,(size_t)n*8*sizeof(float)); \
        glBindVertexArray(ps->vao);glBindBuffer(GL_ARRAY_BUFFER,ps->vbo); \
        GLH_begin((PROG),proj.m,view.m); \
        glBlendFunc(GL_SRC_ALPHA,(BLEND_DST)); \
        glDrawArrays(GL_POINTS,0,n);} \
} while(0)

    /* Normal particles — soft additive */
    PS_DRAW_TYPE(ps->prog,      PT_NORMAL, GL_ONE);
    /* Dust — soft over-blended, non-additive */
    PS_DRAW_TYPE(ps->prog,      PT_DUST,   GL_ONE_MINUS_SRC_ALPHA);
    /* Fire — animated warm additive */
    glUseProgram(ps->fire_prog);
    glUniform1f(glGetUniformLocation(ps->fire_prog,"uTime"),ps->total_time);
    PS_DRAW_TYPE(ps->fire_prog, PT_FIRE,   GL_ONE);
    /* Magic — large ring glow, additive */
    PS_DRAW_TYPE(ps->magic_prog,PT_MAGIC,  GL_ONE);

#undef PS_DRAW_TYPE
    glDepthMask(GL_TRUE); glDisable(GL_BLEND); glBindVertexArray(0);
}

/* ── Weather System ─────────────────────────────────────────── */
#define WEATHER_MAX  2048
typedef enum{WX_NONE,WX_RAIN,WX_SNOW}WeatherType;
typedef struct{float x,y,z,vx,vy,vz,life,max_life,size,r,g,b;}WxParticle;
typedef struct{
    WxParticle parts[WEATHER_MAX];
    int count;
    GLuint prog,vao,vbo;
    WeatherType type;
    bool enabled;
    float accum;
}WeatherSystem;

static void wx_init(WeatherSystem *wx){
    memset(wx,0,sizeof(*wx));
    wx->enabled=true;
    wx->prog=GLH_program(
        "#version 330 core\n"
        "layout(location=0)in vec3 aPos;\nlayout(location=1)in vec4 aColor;\nlayout(location=2)in float aSize;\n"
        "uniform mat4 uProj,uView;\nout vec4 vColor;\n"
        "void main(){gl_Position=uProj*uView*vec4(aPos,1);\n"
        "    gl_PointSize=aSize*600.0/gl_Position.w;vColor=aColor;}\n",
        "#version 330 core\nin vec4 vColor;out vec4 F;\n"
        "void main(){\n"
        /* Rain: tall thin streak; Snow: soft round disk */
        "    vec2 uv=gl_PointCoord*2.0-1.0;\n"
        "    float d=dot(uv,uv);\n"
        "    if(d>1.0)discard;\n"
        "    F=vec4(vColor.rgb,vColor.a*(1.0-d*0.5));}\n");
    GLH_Attrib a[]={{0,3,GL_FLOAT,GL_FALSE,0},{1,4,GL_FLOAT,GL_FALSE,12},{2,1,GL_FLOAT,GL_FALSE,28}};
    wx->vbo=GLH_vbo(WEATHER_MAX*32);
    wx->vao=GLH_mesh_dynamic(WEATHER_MAX*32,32,a,3);
    glBindVertexArray(wx->vao);glBindBuffer(GL_ARRAY_BUFFER,wx->vbo);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,32,(void*)0);
    glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,32,(void*)12);
    glVertexAttribPointer(2,1,GL_FLOAT,GL_FALSE,32,(void*)28);
    glBindVertexArray(0);
}

static void wx_update(WeatherSystem *wx,Vec3 cam_pos,float player_h,float dt,
                      WindSystem *wind,float total_time){
    if(!wx->enabled){wx->count=0;return;}
    (void)total_time;

    WeatherType want=WX_NONE;
    if(player_h>=50.0f)      want=WX_SNOW;
    else if(player_h>=1.0f)  want=WX_RAIN;
    wx->type=want;
    if(want==WX_NONE){wx->count=0;return;}

    /* Spawn rate scales up as the blizzard builds */
    float snow_rate = 90.0f + 280.0f * wind->snow_accum;
    float spawn_rate = (want==WX_RAIN) ? 220.0f : snow_rate;
    /* Spawn ring grows in blizzard */
    float spawn_r    = 28.0f + 30.0f * wind->snow_accum;
    float spawn_h    = 18.0f;

    wx->accum+=dt;
    float inv=1.0f/spawn_rate;
    while(wx->accum>=inv && wx->count<WEATHER_MAX){
        wx->accum-=inv;
        WxParticle *p=&wx->parts[wx->count++];
        float angle=(float)(rand()%1000)/1000.0f*2.0f*(float)M_PI;
        float rad  =(float)(rand()%1000)/1000.0f*spawn_r;
        p->x=cam_pos.x+cosf(angle)*rad;
        p->z=cam_pos.z+sinf(angle)*rad;
        p->y=cam_pos.y+spawn_h+(float)(rand()%100)/100.0f*4.0f;
        if(want==WX_RAIN){
            /* Rain blown by wind */
            float ws=wind->total*0.6f;
            p->vx=wind->wx*ws+(float)(rand()%100-50)*0.008f;
            p->vy=-18.0f-(float)(rand()%100)/100.0f*6.0f;
            p->vz=wind->wz*ws+(float)(rand()%100-50)*0.008f;
            p->max_life=p->life=(spawn_h+4.0f)/fabsf(p->vy);
            p->size=0.18f+(float)(rand()%100)/100.0f*0.12f;
            p->r=0.55f;p->g=0.70f;p->b=0.90f;
        }else{
            /* Snow: drift hard with wind; bigger flakes in blizzard */
            float ws=wind->total*2.8f;
            p->vx=wind->wx*ws+(float)(rand()%100-50)*0.025f;
            p->vy=-0.6f-(float)(rand()%100)/100.0f*0.8f - wind->snow_accum*0.6f;
            p->vz=wind->wz*ws+(float)(rand()%100-50)*0.025f;
            p->max_life=p->life=(spawn_h+4.0f)/fabsf(p->vy);
            float sz=0.28f+(float)(rand()%100)/100.0f*0.18f;
            /* flakes swell in heavy blizzard */
            sz*=1.0f+wind->snow_accum*0.8f;
            p->size=sz;
            p->r=0.92f;p->g=0.95f;p->b=1.00f;
        }
    }

    /* Update existing particles */
    int alive=0;
    for(int i=0;i<wx->count;i++){
        WxParticle *p=&wx->parts[i];
        p->x+=p->vx*dt; p->y+=p->vy*dt; p->z+=p->vz*dt;
        p->life-=dt;
        if(p->life<=0||p->y<terrain_h(p->x,p->z)-0.5f)continue;
        wx->parts[alive++]=*p;
    }
    wx->count=alive;
}

static void wx_draw(WeatherSystem *wx,Mat4 proj,Mat4 view){
    if(!wx->enabled||wx->count==0)return;
    /* Pack into VBO */
    float *buf=malloc((size_t)wx->count*32);
    if(!buf)return;
    for(int i=0;i<wx->count;i++){
        WxParticle *p=&wx->parts[i];
        float alpha=(p->life/p->max_life)*0.75f;
        float *v=buf+i*8;
        v[0]=p->x;v[1]=p->y;v[2]=p->z;
        v[3]=p->r;v[4]=p->g;v[5]=p->b;v[6]=alpha;v[7]=p->size;
    }
    GLH_vbo_update(wx->vbo,buf,(size_t)wx->count*8*sizeof(float));
    free(buf);
    glBindVertexArray(wx->vao);glBindBuffer(GL_ARRAY_BUFFER,wx->vbo);
    GLH_begin(wx->prog,proj.m,view.m);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);glEnable(GL_PROGRAM_POINT_SIZE);
    glDrawArrays(GL_POINTS,0,wx->count);
    glDepthMask(GL_TRUE);glDisable(GL_BLEND);
}
typedef struct{Vec3 pos;int fire_em,smoke_em,spark_em,light_idx;}Campfire;
static Campfire campfire_create(ParticleSystem *ps,LightingSystem *ls,Vec3 pos){
    Campfire cf;cf.pos=pos;
    /* Fire emitter: use generic emitter but spawn typed PT_FIRE particles */
    cf.fire_em =ps_add_emitter_typed(ps,pos,35,1.00f,0.50f,0.05f,0.14f,2.2f,PT_FIRE);
    /* Smoke: soft grey PT_DUST, rises slowly */
    cf.smoke_em=ps_add_emitter_typed(ps,(Vec3){pos.x,pos.y+0.5f,pos.z},6,0.32f,0.30f,0.28f,0.22f,0.7f,PT_DUST);
    /* Sparks: tiny bright PT_NORMAL */
    cf.spark_em=ps_add_emitter(ps,pos,18,1.00f,0.92f,0.30f,0.05f,4.5f);
    Vec3 fc={1.0f,0.55f,0.15f};cf.light_idx=lighting_add_point(ls,pos,fc,13.0f,1.0f);
    if(cf.light_idx>=0){ls->points[cf.light_idx].flicker=true;ls->points[cf.light_idx].flicker_speed=7.0f;ls->points[cf.light_idx].flicker_phase=0.3f;}
    printf("[Campfire] (%.1f,%.1f,%.1f)\n",(double)pos.x,(double)pos.y,(double)pos.z);return cf;
}


/* ── Quest / Ability / Physics / Camera ─────────────────────── */
typedef enum{OBJ_KILL,OBJ_REACH}ObjType;
typedef struct{ObjType type;char desc[64];int current,required;bool done;Vec3 loc;float radius;int etype_filter;/*-1=any enemy, else EnemyType*/}Objective;
typedef struct{int id;char name[64];char desc[128];Objective objs[MAX_OBJECTIVES];int obj_count,gold,xp;enum{Q_INACTIVE,Q_ACTIVE,Q_DONE}status;}Quest;
typedef struct{Quest quests[MAX_QUESTS];int count,next_id,player_gold,player_xp,player_level;}QuestSystem;

static void quest_init(QuestSystem *qs){memset(qs,0,sizeof(*qs));qs->next_id=1;qs->player_level=1;}

static Quest *quest_add(QuestSystem *qs,const char *name,const char *desc){if(qs->count>=MAX_QUESTS)return NULL;Quest *q=&qs->quests[qs->count++];memset(q,0,sizeof(*q));q->id=qs->next_id++;strncpy(q->name,name,63);strncpy(q->desc,desc,127);q->status=Q_INACTIVE;return q;}
static void quest_kill_obj(Quest *q,const char *d,int n){if(!q||q->obj_count>=MAX_OBJECTIVES)return;Objective *o=&q->objs[q->obj_count++];memset(o,0,sizeof(*o));o->type=OBJ_KILL;strncpy(o->desc,d,63);o->required=n;o->etype_filter=-1;}
static void quest_kill_obj_type(Quest *q,const char *d,int n,int etype){quest_kill_obj(q,d,n);if(q->obj_count>0)q->objs[q->obj_count-1].etype_filter=etype;}
static void quest_reach_obj(Quest *q,const char *d,Vec3 loc,float r){if(!q||q->obj_count>=MAX_OBJECTIVES)return;Objective *o=&q->objs[q->obj_count++];memset(o,0,sizeof(*o));o->type=OBJ_REACH;strncpy(o->desc,d,63);o->required=1;o->loc=loc;o->radius=r;o->etype_filter=-1;}
static void quest_start(QuestSystem *qs,int id){for(int i=0;i<qs->count;i++)if(qs->quests[i].id==id){qs->quests[i].status=Q_ACTIVE;return;}}
/* etype = the EnemyType just killed.  Only increments objectives whose
   etype_filter matches (-1 = any), so two simultaneous kill quests
   targeting different enemy types advance independently.               */
static void quest_notify_kill(QuestSystem *qs,int etype){
    for(int i=0;i<qs->count;i++){
        Quest *q=&qs->quests[i];if(q->status!=Q_ACTIVE)continue;
        for(int j=0;j<q->obj_count;j++){
            Objective *o=&q->objs[j];
            if(o->type!=OBJ_KILL||o->done)continue;
            if(o->etype_filter!=-1&&o->etype_filter!=etype)continue;
            o->current++;
            if(o->current>=o->required)o->done=true;
        }
    }
}
static void quest_update(QuestSystem *qs,Vec3 pp){
    for(int i=0;i<qs->count;i++){Quest *q=&qs->quests[i];if(q->status!=Q_ACTIVE)continue;
        for(int j=0;j<q->obj_count;j++){Objective *o=&q->objs[j];if(o->type==OBJ_REACH&&!o->done&&vec3_dist(pp,o->loc)<=o->radius)o->done=true;}
        bool all=true;for(int j=0;j<q->obj_count;j++)if(!q->objs[j].done){all=false;break;}
        if(all){q->status=Q_DONE;qs->player_gold+=q->gold;qs->player_xp+=q->xp;printf("[Quest] '%s' complete!\n",q->name);}}
}

typedef struct{char name[32];float cost,cd,cd_timer;bool ready;}Ability;
typedef struct{Ability slots[HOTBAR_SLOTS];float mana,max_mana,mp_regen,stam,max_stam,sp_regen,stam_drain_timer;}AbilitySystem;
static void ability_init(AbilitySystem *as){memset(as,0,sizeof(*as));as->mana=as->max_mana=100;as->mp_regen=3;as->stam=as->max_stam=100;as->sp_regen=8;const char *n[]={"Fireball","Heal","Dash","Shield","Meteor"};float c[]={20,25,0,15,40},cd[]={3,8,4,20,15};for(int i=0;i<HOTBAR_SLOTS;i++){strncpy(as->slots[i].name,n[i],31);as->slots[i].cost=c[i];as->slots[i].cd=cd[i];as->slots[i].ready=true;}}
static void ability_update(AbilitySystem *as,float dt){
    as->mana=clampf(as->mana+as->mp_regen*dt,0,as->max_mana);
    /* Stam regens only after 1.5s of no drain */
    if(as->stam_drain_timer>0.0f) as->stam_drain_timer-=dt;
    else as->stam=clampf(as->stam+as->sp_regen*dt,0,as->max_stam);
    for(int i=0;i<HOTBAR_SLOTS;i++){Ability *ab=&as->slots[i];if(!ab->ready){ab->cd_timer-=dt;if(ab->cd_timer<=0){ab->ready=true;ab->cd_timer=0;}}}
}
/* Call this whenever stamina is consumed to reset the regen delay */
static void stam_consume(AbilitySystem *as,float amount){
    as->stam=clampf(as->stam-amount,0,as->max_stam);
    as->stam_drain_timer=1.5f;
}
static void stam_drain(AbilitySystem *as){
    /* Mark draining this frame — delays regen (called when sprint/block active) */
    as->stam_drain_timer=1.5f;
}
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

typedef struct PhysBody{Vec3 pos,vel;float yaw;bool on_ground;float height,radius;}PhysBody;
static void phys_update(PhysBody *b,float dt,float gy){if(!b->on_ground)b->vel.y+=GRAVITY*dt;b->pos=vec3_add(b->pos,vec3_scale(b->vel,dt));if(b->pos.y<=gy){b->pos.y=gy;if(b->vel.y<0)b->vel.y=0;b->on_ground=true;}else b->on_ground=false;}

static void props_resolve(PropList *pl,PhysBody *body,float radius){
    for(int pi=0;pi<pl->count;pi++){
        Prop *p=&pl->list[pi];
        if(!p->active||p->stream_state!=PROP_LOADED)continue;
        if(p->coll_type==PROP_COLL_AABB){
            AABB *b=&p->aabb;
            float py=body->pos.y,ptop=py+body->height;
            if(py>b->max.y+0.05f||ptop<b->min.y-0.05f)continue;
            if(body->pos.x+radius<b->min.x||body->pos.x-radius>b->max.x)continue;
            if(body->pos.z+radius<b->min.z||body->pos.z-radius>b->max.z)continue;
            /* Land on top */
            bool over_box=(body->pos.x>b->min.x-radius&&body->pos.x<b->max.x+radius&&
                           body->pos.z>b->min.z-radius&&body->pos.z<b->max.z+radius);
            if(over_box&&py<=b->max.y+0.25f&&py>=b->max.y-0.5f&&body->vel.y<=0.0f){
                body->pos.y=b->max.y; body->vel.y=0.0f; body->on_ground=true; continue;
            }
            /* Side push — minimum separation axis */
            float left=body->pos.x+radius-b->min.x, right=b->max.x-(body->pos.x-radius);
            float front=body->pos.z+radius-b->min.z, back=b->max.z-(body->pos.z-radius);
            if(left<0||right<0||front<0||back<0)continue;
            float ox=left<right?-left:right;
            float oz=front<back?-front:back;
            if(fabsf(ox)<fabsf(oz))body->pos.x+=ox; else body->pos.z+=oz;
        } else if(p->coll_type==PROP_COLL_MESH){
            if(trimesh_resolve(&p->trimesh,&body->pos,radius,body->height)){
                if(body->vel.y<=0.0f){ body->vel.y=0.0f; body->on_ground=true; }
            }
        }
    }
}

typedef enum{PS_IDLE,PS_WALK,PS_RUN,PS_ATTACK,PS_ATTACK2,PS_JUMP,PS_ROLL,PS_BLOCK,PS_HIT,PS_DEAD,PS_SWIM}PlayerState;
typedef struct{PhysBody phys;PlayerState state;float health,max_health;double anim_time;float anim_speed;int anim_idx,prev_anim_idx;float attack_timer;bool attack_hit;float hit_flash;float roll_timer;float hit_timer;bool blocking;float shield_timer;float dash_iframes;float ooc_timer;/*out-of-combat seconds before regen kicks in*/}Player;
typedef enum{ES_IDLE,ES_CHASE,ES_ATTACK,ES_STAGGER,ES_DEAD}EnemyState;
/* ── Projectiles (Fireball etc) ──────────────────────────────── */
#define MAX_PROJECTILES 32
typedef enum{PROJ_FIREBALL=0,PROJ_ARROW}ProjType;
typedef enum{PROJ_OWNER_PLAYER=0,PROJ_OWNER_ENEMY}ProjOwner;
typedef struct{
    Vec3  pos,vel;
    float life,radius,damage;
    ProjType  type;
    ProjOwner owner;
    bool  active;
}Projectile;
typedef struct{Projectile p[MAX_PROJECTILES];int count;}ProjList;

static void proj_spawn(ProjList *pl,Vec3 pos,Vec3 dir,float speed,float dmg,ProjType type,ProjOwner owner){
    for(int i=0;i<MAX_PROJECTILES;i++){
        Projectile *p=&pl->p[i];if(p->active)continue;
        p->pos=pos;
        p->vel=(Vec3){dir.x*speed,dir.y*speed,dir.z*speed};
        p->life=(type==PROJ_ARROW)?3.0f:5.0f;
        p->radius=(type==PROJ_ARROW)?0.18f:0.4f;
        p->damage=dmg;p->type=type;p->owner=owner;p->active=true;
        if(i>=pl->count)pl->count=i+1;
        return;
    }
}

/* ── Enemy Types ─────────────────────────────────────────────── */
typedef enum{
    ETYPE_KNIGHT=0,   /* standard melee — the existing enemy */
    ETYPE_ARCHER,     /* ranged — fires projectiles, keeps distance */
    ETYPE_BRUTE,      /* heavy — slow, high HP/damage, large */
    ETYPE_SHADE,      /* fast — low HP, very quick, small */
    ETYPE_BOSS,       /* The Lich King — 3-phase boss */
    ETYPE_COUNT
}EnemyType;

typedef struct{
    const char *name;
    const char *fbx;      /* FBX filename — NULL falls back to char_scene */
    float hp;
    float damage;
    float speed;
    float chase_r;
    float attack_r;
    float attack_cd;
    float scale;
    float cr,cg,cb;   /* tint color */
    float height,radius;
    bool  ranged;
    int   xp;         /* XP awarded on kill */
}EnemyDef;

static const EnemyDef EDEF[ETYPE_COUNT]={
/*  name       fbx                   hp    dmg  spd   chaseR  atkR   atkCD  scale  r     g     b     h     rad  ranged  xp */
  {"Knight",  "enemy_knight.fbx",   80,   12,  2.8f, 14.0f,  1.8f,  1.5f,  1.0f,  0.35f,0.35f,0.42f, 1.8f,0.4f, false, 25},
  {"Archer",  "enemy_archer.fbx",   55,    8,  2.4f, 20.0f,  8.0f,  2.5f,  0.95f, 0.25f,0.45f,0.25f, 1.8f,0.4f, true,  20},
  {"Brute",   "enemy_brute.fbx",   200,   25,  1.6f, 12.0f,  2.2f,  2.2f,  1.35f, 0.55f,0.20f,0.20f, 2.4f,0.6f, false, 75},
  {"Shade",   "enemy_shade.fbx",    40,    8,  5.0f, 18.0f,  1.4f,  0.8f,  0.82f, 0.15f,0.15f,0.35f, 1.6f,0.35f,false, 15},
  {"Lich King","boss.fbx",         800,   35,  2.2f, 40.0f,  2.5f,  1.8f,  1.8f,  0.05f,0.80f,0.85f, 3.0f,0.7f, false, 500},
};

typedef struct{
    PhysBody phys;
    EnemyState state;
    EnemyType  etype;
    float health,max_health;
    double anim_time;
    float  anim_speed;
    int    anim_idx;
    float  attack_timer,attack_cooldown,stagger_timer;
    float  growl_timer;   /* random growl cooldown */
    bool   active;
    /* Respawn */
    Vec3   spawn_pos;       /* original spawn position                  */
    float  respawn_timer;   /* counts down after death; 0 = ready       */
    bool   boss_no_respawn; /* boss never respawns                       */
    /* Boss-only fields */
    int    boss_phase;      /* 1/2/3 — updated as HP drops */
    float  summon_timer;    /* cooldown before next minion summon */
    float  shake_timer;     /* screen shake remaining */
    bool   phase_flash;     /* plays transition flash */
    float  phase_flash_t;
    /* ── Performance: cached animation eval ─────────────────────
       ufbx_evaluate_scene is expensive. We evaluate once per
       throttle interval and cache the ufbx_scene* result.
       Both shadow and draw passes compute_bones from it normally,
       then it is freed after all enemy rendering is done.         */
    ufbx_scene *anim_eval;        /* cached evaluated pose (or NULL) */
    int         anim_throttle;    /* frame counter for LOD throttle  */
    int         anim_eval_frame;  /* frame number of last evaluation */
}Enemy;

static void enemy_refresh_eval(Enemy *e, CharModel *cm, float dist, int frame){
    int skip_rate = (dist < 30.0f) ? 1 : (dist < 60.0f) ? 2 : 3;
    if(e->anim_eval && (frame - e->anim_eval_frame) < skip_rate)
        return; /* reuse existing eval */

    if(e->anim_eval){ ufbx_free_scene(e->anim_eval); e->anim_eval=NULL; }
    if(!cm||!scene_has_data(&cm->base)) return;

    int slot=e->anim_idx;
    if(slot<0||slot>=ANIM_SLOTS) slot=ANIM_IDLE;
    AnimEntry *ae=&cm->anims[slot];
    Scene *sc=ae->scene?ae->scene:&cm->base;
    if(!sc)return;
    if(sc->format==SCENE_FORMAT_GLTF){
        scene_gltf_evaluate(sc,ae->scene?ae->stack:-1,e->anim_time);
        e->anim_eval_frame=frame;
        return;
    }
    if(sc->format!=SCENE_FORMAT_UFBX||!sc->scene) return;

    int ai=ae->scene?ae->stack:-1;
    if(sc->scene->anim_stacks.count>0&&ai>=0){
        int idx=ai<(int)sc->scene->anim_stacks.count?ai:(int)sc->scene->anim_stacks.count-1;
        ufbx_anim_stack *stk=sc->scene->anim_stacks.data[idx];
        double real_end=anim_stack_real_end(stk);
        double dur=real_end-stk->time_begin;
        double t=dur>0?fmod(e->anim_time,dur):0;
        e->anim_eval=ufbx_evaluate_scene(sc->scene,stk->anim,stk->time_begin+t,NULL,NULL);
    }
    e->anim_eval_frame=frame;
}

static void enemy_spawn(Enemy *e,EnemyType type,Vec3 pos,AnimMap *amap){
    memset(e,0,sizeof(*e));
    const EnemyDef *d=&EDEF[type];
    Vec3 dry=pos;
    find_nearby_dry_ground(pos,22.0f,&dry);
    e->etype=type;
    e->phys.pos=dry; e->phys.pos.y=terrain_h(dry.x,dry.z)+0.1f;
    e->phys.height=d->height; e->phys.radius=d->radius;
    e->health=e->max_health=d->hp;
    e->anim_idx=amap->idle; e->anim_speed=1.0f;
    e->attack_cooldown=d->attack_cd;
    e->growl_timer=2.0f+(float)rand()/RAND_MAX*4.0f;
    e->spawn_pos=e->phys.pos;
    if(type==ETYPE_BOSS){e->boss_phase=1;e->summon_timer=8.0f;e->boss_no_respawn=true;}
    e->active=true;
}

/* forward decls — audio defined later */
/* forward sound defines needed before audio system is declared */
#define SND_FIREBALL_HIT 8
#define SND_HIT          3
static void audio_play(int snd,int vol);

static void proj_update(ProjList *pl,float dt,ParticleSystem *ps,
                        Enemy *enemies,int ne,Player *player){
    for(int i=0;i<pl->count;i++){
        Projectile *p=&pl->p[i];if(!p->active)continue;
        p->vel.y-=(p->type==PROJ_ARROW?9.8f:4.0f)*dt;
        p->pos=vec3_add(p->pos,vec3_scale(p->vel,dt));
        p->life-=dt;
        if(p->life<=0){p->active=false;continue;}
        if(p->type==PROJ_FIREBALL){
            for(int k=0;k<3;k++){
                Vec3 off={(((float)rand()/RAND_MAX)-0.5f)*0.2f,
                          (((float)rand()/RAND_MAX)-0.5f)*0.2f,
                          (((float)rand()/RAND_MAX)-0.5f)*0.2f};
                ps_spawn(ps,vec3_add(p->pos,off),(Vec3){off.x*2,0.5f+off.y,off.z*2},
                         0.4f,0.3f+((float)rand()/RAND_MAX)*0.2f,
                         1.0f,0.4f+((float)rand()/RAND_MAX)*0.3f,0.0f);
            }
        }
        float th=terrain_h(p->pos.x,p->pos.z);
        if(p->pos.y<=th){
            if(p->type==PROJ_FIREBALL){ps_burst(ps,p->pos,1.0f,0.5f,0.0f);audio_play(SND_FIREBALL_HIT,85);}
            p->active=false;continue;
        }
        if(p->owner==PROJ_OWNER_PLAYER){
            for(int ei=0;ei<ne;ei++){
                Enemy *e=&enemies[ei];if(!e->active||e->health<=0)continue;
                float dx=p->pos.x-e->phys.pos.x,dz=p->pos.z-e->phys.pos.z,dy=p->pos.y-(e->phys.pos.y+1.0f);
                if(dx*dx+dy*dy+dz*dz<(p->radius+e->phys.radius)*(p->radius+e->phys.radius)){
                    e->health-=p->damage;ps_burst(ps,p->pos,1.0f,0.4f,0.0f);audio_play(SND_FIREBALL_HIT,85);p->active=false;break;
                }
            }
        } else {
            if(player->state!=PS_DEAD){
                float dx=p->pos.x-player->phys.pos.x;
                float dz=p->pos.z-player->phys.pos.z;
                float dy=p->pos.y-(player->phys.pos.y+0.9f);
                float r2=(p->radius+player->phys.radius)*(p->radius+player->phys.radius);
                if(dx*dx+dy*dy+dz*dz<r2){
                    if(player->dash_iframes<=0){
                        float shield_reduce=(player->shield_timer>0)?0.75f:0.0f;
                        float block_reduce =(player->blocking)      ?0.60f:0.0f;
                        float total_reduce=clampf(shield_reduce+block_reduce,0,0.90f);
                        player->health-=p->damage*(1.0f-total_reduce);
                        player->hit_flash=1.0f;
                        ps_blood(ps,player->phys.pos,0.9f,8);
                        audio_play(SND_HIT,90);
                        if(player->health<=0){player->health=0;player->state=PS_DEAD;}
                    }
                    p->active=false;
                }
            }
        }
    }
}
static void lr_box(AABB *box,float r,float g,float b); /* forward decl */

typedef struct{float yaw,pitch,dist;Vec3 pos,target;}Camera3P;
static void cam_update(Camera3P *c,Vec3 follow,float dt){
    Vec3 want=vec3_add(follow,(Vec3){0,1.3f,0});
    c->target=vec3_add(c->target,vec3_scale(vec3_sub(want,c->target),10.0f*dt));
    c->pos.x=c->target.x+c->dist*cosf(c->pitch)*sinf(c->yaw);
    c->pos.y=c->target.y+c->dist*sinf(c->pitch);
    c->pos.z=c->target.z+c->dist*cosf(c->pitch)*cosf(c->yaw);
    /* Never let the camera clip below the terrain or water surface */
    float floor_y=terrain_h(c->pos.x,c->pos.z)+0.5f;
    float water_y=wzone_local_level(c->pos.x,c->pos.z)+0.35f;
    float min_y=floor_y>water_y?floor_y:water_y;
    if(c->pos.y<min_y) c->pos.y=min_y;
}
/* FIX: W = move away from camera (into scene) */
static Vec3 cam_forward(Camera3P *c){return vec3_norm((Vec3){-sinf(c->yaw),0,-cosf(c->yaw)});}
static Vec3 cam_right(Camera3P *c){Vec3 f=cam_forward(c);return (Vec3){-f.z,0,f.x};}


/* ══════════════════════════════════════════════════════════════
   WATERFALL SYSTEM
   Each waterfall is a vertical curtain mesh placed by the user
   in the F2 editor (Tab = toggle zone/waterfall sub-mode).
   The curtain uses its own animated GLSL shader; splash and mist
   particles come from the shared ParticleSystem.
   Saved/loaded via waterfalls.txt.
   ══════════════════════════════════════════════════════════════ */
#define MAX_WATERFALLS   16
#define WF_MESH_COLS      8   /* subdivisions across width */
#define WF_MESH_ROWS     24   /* subdivisions down height  */
#define WF_FILE          "waterfalls.txt"

typedef struct {
    Vec3  pos;        /* world position of the TOP-CENTRE of the falls */
    float yaw;        /* rotation around Y axis (radians)              */
    float width;      /* horizontal spread in world units              */
    float height;     /* vertical drop in world units                  */
    int   mist_em;    /* index into ParticleSystem.emitters, -1=none   */
    bool  active;
} Waterfall;

typedef struct {
    Waterfall   wf[MAX_WATERFALLS];
    int         count;
    GLuint      prog;
    GLuint      vao, vbo, ebo;
    int         icount;   /* number of indices in the unit-curtain mesh */
} WaterfallSystem;

static WaterfallSystem g_wfalls;

/* ── Waterfall save/load ─────────────────────────────────────── */
static void wf_save(void){
    FILE *f=fopen(WF_FILE,"w");
    if(!f)return;
    fprintf(f,"# waterfalls  x y z yaw width height\n");
    for(int i=0;i<g_wfalls.count;i++){
        Waterfall *w=&g_wfalls.wf[i];
        if(!w->active)continue;
        fprintf(f,"%.3f %.3f %.3f %.4f %.2f %.2f\n",
                (double)w->pos.x,(double)w->pos.y,(double)w->pos.z,
                (double)w->yaw,(double)w->width,(double)w->height);
    }
    fclose(f);
    printf("[WFalls] Saved %d waterfalls to %s\n",g_wfalls.count,WF_FILE);
}

/* forward-declared — defined after ps_add_emitter */
static void wf_load(void);   /* defined after wf_add */

/* ── Add one waterfall (allocates emitters) ──────────────────── */
static int wf_add(Vec3 pos, float yaw, float width, float height){
    if(g_wfalls.count>=MAX_WATERFALLS)return -1;
    int i=g_wfalls.count++;
    Waterfall *w=&g_wfalls.wf[i];
    w->pos=pos; w->yaw=yaw; w->width=width; w->height=height;
    w->active=true; w->mist_em=-1;
    return i;
}

/* ── GL init: build unit-curtain mesh + compile shader ───────── */
static void wf_init(void){
    memset(&g_wfalls,0,sizeof(g_wfalls));

    /* Unit curtain: x in [-0.5, 0.5], y in [0, -1]
       We pass uWidth/uHeight uniforms and scale in VS. */
    int cols=WF_MESH_COLS, rows=WF_MESH_ROWS;
    int vc=(cols+1)*(rows+1);
    int ic=cols*rows*6;
    float *verts=malloc((size_t)vc*5*sizeof(float)); /* x y u v heightFrac */
    uint32_t *idx=malloc((size_t)ic*sizeof(uint32_t));

    for(int r=0;r<=rows;r++){
        float fv=(float)r/(float)rows;          /* 0=top, 1=bottom */
        float y =-fv;                            /* local y: 0..−1  */
        for(int c=0;c<=cols;c++){
            float fu=(float)c/(float)cols;       /* 0=left, 1=right */
            float x =fu-0.5f;                    /* local x: −0.5..0.5 */
            int vi=r*(cols+1)+c;
            verts[vi*5+0]=x;
            verts[vi*5+1]=y;
            verts[vi*5+2]=fu;          /* u */
            verts[vi*5+3]=fv;          /* v (0=top 1=bot) */
            verts[vi*5+4]=fv;          /* heightFrac (same as v) */
        }
    }
    int ii=0;
    for(int r=0;r<rows;r++) for(int c=0;c<cols;c++){
        uint32_t tl=(uint32_t)(r*(cols+1)+c);
        uint32_t tr=tl+1, bl=tl+(uint32_t)(cols+1), br=bl+1;
        idx[ii++]=tl;idx[ii++]=bl;idx[ii++]=tr;
        idx[ii++]=tr;idx[ii++]=bl;idx[ii++]=br;
    }
    g_wfalls.icount=ii;

    /* VAO/VBO/EBO */
    glGenVertexArrays(1,&g_wfalls.vao);
    glGenBuffers(1,&g_wfalls.vbo);
    glGenBuffers(1,&g_wfalls.ebo);
    glBindVertexArray(g_wfalls.vao);
    glBindBuffer(GL_ARRAY_BUFFER,g_wfalls.vbo);
    glBufferData(GL_ARRAY_BUFFER,(GLsizeiptr)((size_t)vc*5*sizeof(float)),verts,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,g_wfalls.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,(GLsizeiptr)((size_t)ic*sizeof(uint32_t)),idx,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)(2*sizeof(float)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2,1,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)(4*sizeof(float)));
    glBindVertexArray(0);
    free(verts); free(idx);

    /* Shader */
    g_wfalls.prog=GLH_program(
        /* ── Vertex ── */
        "#version 330 core\n"
        "layout(location=0)in vec2 aXY;\n"          /* local -0.5..0.5, 0..-1 */
        "layout(location=1)in vec2 aUV;\n"
        "layout(location=2)in float aHF;\n"          /* 0=top 1=bottom */
        "uniform mat4 uProj,uView,uModel;\n"
        "uniform float uTime,uWidth,uHeight;\n"
        "out vec2  vUV;\n"
        "out float vHF;\n"
        "out vec3  vWorldPos;\n"
        "void main(){\n"
        /* Turbulence grows quadratically toward base */
        "    float turb=aHF*aHF;\n"
        "    float tx=sin(aUV.x*6.28+uTime*2.1)*0.04*turb\n"
        "            +sin(aUV.x*13.7+uTime*3.4)*0.02*turb;\n"
        "    float tz=cos(aUV.x*9.1 +uTime*1.8)*0.03*turb;\n"
        "    vec3 local=vec3((aXY.x+tx)*uWidth, aXY.y*uHeight, tz*uWidth*0.15);\n"
        "    vec4 world=uModel*vec4(local,1.0);\n"
        "    gl_Position=uProj*uView*world;\n"
        "    vUV=aUV; vHF=aHF; vWorldPos=world.xyz;\n"
        "}\n",
        /* ── Fragment ── */
        "#version 330 core\n"
        "in vec2  vUV;\n"
        "in float vHF;\n"
        "in vec3  vWorldPos;\n"
        "uniform float uTime;\n"
        "out vec4 FragColor;\n"
        /* hash-based value noise for streaks */
        "float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}\n"
        "float vnoise(vec2 p){\n"
        "    vec2 i=floor(p);vec2 f=fract(p);\n"
        "    vec2 u=f*f*(3.0-2.0*f);\n"
        "    return mix(mix(hash(i),hash(i+vec2(1,0)),u.x),\n"
        "               mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),u.x),u.y);\n"
        "}\n"
        "void main(){\n"
        /* Scrolling UV: fast downward flow */
        "    vec2 flowUV=vec2(vUV.x*4.0, vUV.y*6.0 - uTime*3.5);\n"
        /* Two octaves of streak noise */
        "    float s =vnoise(flowUV);\n"
        "    float s2=vnoise(flowUV*2.1+vec2(3.7,1.3));\n"
        "    float streak=mix(s,s2,0.4);\n"
        "    streak=pow(streak,1.8);\n"
        /* Horizontal edge fade */
        "    float ex=vUV.x*2.0-1.0;\n"
        "    float edge=1.0-ex*ex;\n"
        "    edge=edge*edge;\n"
        /* Foam band at the base */
        "    float foam=exp(-vHF*8.0)*0.6;\n"
        /* Core water colour: deep blue-white */
        "    vec3 deepCol =vec3(0.55,0.75,0.95);\n"
        "    vec3 foamCol =vec3(0.90,0.95,1.00);\n"
        "    vec3 col=mix(deepCol,foamCol,streak*0.5+foam);\n"
        /* Brighten highlights */
        "    col+=vec3(0.3)*pow(streak,3.0);\n"
        /* Alpha: translucent body, opaque foam, fade at edges */
        "    float alpha=(0.35+streak*0.35+foam*0.5)*edge;\n"
        "    alpha=clamp(alpha,0.0,0.92);\n"
        "    FragColor=vec4(col,alpha);\n"
        "}\n"
    );
}

/* Called once after ps is initialised to wire up mist emitters */
static void wf_apply_emitters(ParticleSystem *ps){
    for(int i=0;i<g_wfalls.count;i++){
        Waterfall *w=&g_wfalls.wf[i];
        if(!w->active||w->mist_em>=0)continue;
        Vec3 base={w->pos.x, w->pos.y-w->height+0.5f, w->pos.z};
        /* Mist: slow, white, small, low rate */
        w->mist_em=ps_add_emitter(ps,base,6,0.88f,0.93f,1.00f,0.14f,0.9f);
    }
}

static void wf_load(void){
    FILE *f=fopen(WF_FILE,"r");
    if(!f)return;
    char line[128];
    g_wfalls.count=0;
    while(fgets(line,sizeof(line),f)&&g_wfalls.count<MAX_WATERFALLS){
        if(line[0]=='#')continue;
        float x,y,z,yaw,w,h;
        if(sscanf(line,"%f %f %f %f %f %f",&x,&y,&z,&yaw,&w,&h)==6){
            wf_add((Vec3){x,y,z},yaw,w,h);
        }
    }
    fclose(f);
    printf("[WFalls] Loaded %d waterfalls from %s\n",g_wfalls.count,WF_FILE);
}

/* ── Per-frame update: spawn splash particles ───────────────── */
static void wf_update(WaterfallSystem *wfs, ParticleSystem *ps, float dt){
    (void)dt;
    for(int i=0;i<wfs->count;i++){
        Waterfall *w=&wfs->wf[i];
        if(!w->active)continue;
        /* Splash at base: small burst every few frames */
        if((rand()%4)==0){
            float cy=cosf(w->yaw), sy=sinf(w->yaw);
            /* random point across width */
            float fx=(float)(rand()%100-50)*0.01f*w->width*0.45f;
            Vec3 base={w->pos.x + cy*0.0f - sy*fx,
                       w->pos.y - w->height + 0.1f,
                       w->pos.z + sy*0.0f + cy*fx};
            /* Outward + slightly upward velocity */
            float vx=(float)(rand()%100-50)*0.03f;
            float vz=(float)(rand()%100-50)*0.03f;
            float vy=0.8f+(float)rand()/RAND_MAX*1.8f;
            Vec3 vel={vx*3.0f, vy, vz*3.0f};
            ps_spawn(ps,base,vel,0.4f+(float)rand()/RAND_MAX*0.5f,
                     0.07f+(float)rand()/RAND_MAX*0.06f,
                     0.80f,0.90f,1.00f);
        }
        /* Update mist emitter position (base may have moved if user edited) */
        if(w->mist_em>=0 && w->mist_em<ps->ecount){
            ps->emitters[w->mist_em].pos=(Vec3){
                w->pos.x, w->pos.y-w->height+0.6f, w->pos.z};
        }
    }
}

/* ── mat4_translate / mat4_rotate helpers (used in wf_draw) ─── */
static Mat4 wf_model_matrix(Waterfall *w){
    float cy=cosf(w->yaw), sy=sinf(w->yaw);
    /* Column-major OpenGL matrix: rotation around Y then translate */
    Mat4 m;
    m.m[ 0]= cy; m.m[ 4]=0; m.m[ 8]= sy; m.m[12]=w->pos.x;
    m.m[ 1]=  0; m.m[ 5]=1; m.m[ 9]=  0; m.m[13]=w->pos.y;
    m.m[ 2]=-sy; m.m[ 6]=0; m.m[10]= cy; m.m[14]=w->pos.z;
    m.m[ 3]=  0; m.m[ 7]=0; m.m[11]=  0; m.m[15]=1;
    return m;
}

/* ── Render all active waterfalls ───────────────────────────── */
static void wf_draw(WaterfallSystem *wfs, Mat4 proj, Mat4 view, float time){
    if(!wfs->count)return;
    glUseProgram(wfs->prog);
    glUniformMatrix4fv(glGetUniformLocation(wfs->prog,"uProj"),1,GL_FALSE,proj.m);
    glUniformMatrix4fv(glGetUniformLocation(wfs->prog,"uView"),1,GL_FALSE,view.m);
    glUniform1f(glGetUniformLocation(wfs->prog,"uTime"),time);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(wfs->vao);
    int uModel =glGetUniformLocation(wfs->prog,"uModel");
    int uWidth =glGetUniformLocation(wfs->prog,"uWidth");
    int uHeight=glGetUniformLocation(wfs->prog,"uHeight");
    for(int i=0;i<wfs->count;i++){
        Waterfall *w=&wfs->wf[i];
        if(!w->active)continue;
        Mat4 model=wf_model_matrix(w);
        glUniformMatrix4fv(uModel,1,GL_FALSE,model.m);
        glUniform1f(uWidth, w->width);
        glUniform1f(uHeight,w->height);
        glDrawElements(GL_TRIANGLES,wfs->icount,GL_UNSIGNED_INT,0);
    }
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}

/* ── End waterfall system ──────────────────────────────────── */


/* ── Custom River Editor ────────────────────────────────────────
   Points placed by the user at runtime. terrain_h samples this
   list the same way it samples hardcoded rivers. Chunks that
   overlap the edited area are rebuilt on the GPU.               */
#define CRIVER_MAX_PTS  128
#define CRIVER_WIDTH    80.0f   /* river half-width in world units */
#define CRIVER_DEPTH    5.0f    /* carve deep enough to reach below WATER_LEVEL */

/* CustomRiver typedef and g_criver declared earlier as forward declarations */

/* Returns distance to nearest segment of the custom river, 1e9 if empty */
static float criver_dist(float wx,float wz){
    if(g_criver.count<2)return 1e9f;
    return river_dist(wx,wz,g_criver.pts,g_criver.count*2);
}

static bool water_surface_info(float wx,float wz,float *out_level,int *out_preset){
    float best_t=0.0f;
    float best_level=WATER_LEVEL;
    int best_preset=(g_wstyle.preset_idx>=0)?g_wstyle.preset_idx:0;
    bool found=false;
    for(int zi=0;zi<g_wzone_count;zi++){
        WaterZone *z=&g_wzones[zi];
        if(!z->active)continue;
        float dx=wx-z->cx, dz=wz-z->cz;
        float d=sqrtf(dx*dx+dz*dz);
        if(d>=z->radius)continue;
        float t=clampf(1.0f-(d/z->radius),0.0f,1.0f);
        t=t*t*(3.0f-2.0f*t);
        if(t>best_t){
            best_t=t;
            best_level=z->level;
            best_preset=z->preset_idx;
            found=true;
        }
    }
    {
        float rdist=builtin_river_dist(wx,wz);
        if(rdist<BUILTIN_RIVER_RENDER_HALF_WIDTH){
            float t=clampf(1.0f-rdist/BUILTIN_RIVER_RENDER_HALF_WIDTH,0.0f,1.0f);
            t=t*t*(3.0f-2.0f*t);
            if(t>best_t){
                best_t=t;
                best_level=WATER_LEVEL;
                best_preset=2; /* Dark River */
                found=true;
            }
        }
    }
    {
        float rr=fmaxf(g_criver.width*0.92f,18.0f);
        float rdist=criver_dist(wx,wz);
        if(rdist<rr){
            float t=clampf(1.0f-rdist/rr,0.0f,1.0f);
            t=t*t*(3.0f-2.0f*t);
            if(t>best_t){
                best_t=t;
                best_level=WATER_LEVEL;
                best_preset=(g_wstyle.preset_idx>=0)?g_wstyle.preset_idx:0;
                found=true;
            }
        }
    }
    if(found){
        if(out_level) *out_level=best_level;
        if(out_preset) *out_preset=best_preset;
    }
    return found;
}

static bool point_is_surface_water(float wx,float wz){
    float wl=WATER_LEVEL;
    if(!water_surface_info(wx,wz,&wl,NULL)) return false;
    return terrain_h(wx,wz) <= wl + 0.15f;
}

static bool find_nearby_dry_ground(Vec3 origin,float max_radius,Vec3 *out_pos){
    if(!point_is_surface_water(origin.x,origin.z)){
        if(out_pos) *out_pos=(Vec3){origin.x,terrain_h(origin.x,origin.z),origin.z};
        return true;
    }
    for(int ring=1;ring<=8;ring++){
        float r=max_radius*((float)ring/8.0f);
        int samples=8+ring*6;
        for(int i=0;i<samples;i++){
            float a=((float)i/(float)samples)*(float)(2.0*M_PI);
            float wx=origin.x+cosf(a)*r;
            float wz=origin.z+sinf(a)*r;
            if(point_is_surface_water(wx,wz)) continue;
            if(out_pos) *out_pos=(Vec3){wx,terrain_h(wx,wz),wz};
            return true;
        }
    }
    return false;
}

/* ══════════════════════════════════════════════════════════════
   GRASS PAINT MASK  (used by F9 grass mode)
   512×512 byte density map.  0 = none, 255 = full density.
   The first paint seeds from the current procedural grass rules so
   editing starts from the existing look instead of wiping the world.
   ══════════════════════════════════════════════════════════════ */
#define GRASS_MASK_RES   512
#define GRASS_MASK_FILE  "grass_mask.bin"
typedef struct{ uint8_t d[GRASS_MASK_RES*GRASS_MASK_RES]; } GrassMask;
static GrassMask g_grass_mask;
static bool g_grass_mask_active=false;
static bool g_grass_mask_seeded=false;

static float grass_mask_sample(float wx,float wz){
    if(!g_grass_mask_active) return 1.0f;
    float su=(wx+WORLD_HALF)/(WORLD_HALF*2.0f)*(float)(GRASS_MASK_RES-1);
    float sv=(wz+WORLD_HALF)/(WORLD_HALF*2.0f)*(float)(GRASS_MASK_RES-1);
    su=clampf(su,0.0f,(float)(GRASS_MASK_RES-1));
    sv=clampf(sv,0.0f,(float)(GRASS_MASK_RES-1));
    int ix=(int)su, iz=(int)sv;
    float fx=su-(float)ix, fz=sv-(float)iz;
    int ix2=ix<GRASS_MASK_RES-1?ix+1:ix;
    int iz2=iz<GRASS_MASK_RES-1?iz+1:iz;
    float v00=(float)g_grass_mask.d[iz *GRASS_MASK_RES+ix ]*(1.0f/255.0f);
    float v10=(float)g_grass_mask.d[iz *GRASS_MASK_RES+ix2]*(1.0f/255.0f);
    float v01=(float)g_grass_mask.d[iz2*GRASS_MASK_RES+ix ]*(1.0f/255.0f);
    float v11=(float)g_grass_mask.d[iz2*GRASS_MASK_RES+ix2]*(1.0f/255.0f);
    return v00*(1.0f-fx)*(1.0f-fz)+v10*fx*(1.0f-fz)+v01*(1.0f-fx)*fz+v11*fx*fz;
}

static bool grass_mask_base_allowed(float wx,float wz){
    float h=terrain_h(wx,wz);
    if(h < WATER_LEVEL+0.25f || h > WATER_LEVEL+7.2f) return false;
    if(point_is_surface_water(wx,wz)) return false;
    if(cpath_dist(wx,wz) < g_cpath.width + 1.2f) return false;
    if(worn_path_sample(wx,wz) > 0.22f) return false;
    float hL=terrain_h(wx-0.5f,wz), hR=terrain_h(wx+0.5f,wz);
    float hD=terrain_h(wx,wz-0.5f), hU=terrain_h(wx,wz+0.5f);
    Vec3 n=vec3_norm((Vec3){hL-hR,1.0f,hD-hU});
    return n.y >= 0.74f;
}

static void grass_mask_seed_from_terrain(void){
    float cell=(WORLD_HALF*2.0f)/(float)(GRASS_MASK_RES-1);
    for(int iz=0;iz<GRASS_MASK_RES;iz++){
        for(int ix=0;ix<GRASS_MASK_RES;ix++){
            float wx=(float)ix*cell-WORLD_HALF;
            float wz=(float)iz*cell-WORLD_HALF;
            g_grass_mask.d[iz*GRASS_MASK_RES+ix]=grass_mask_base_allowed(wx,wz)?176:0;
        }
    }
    g_grass_mask_active=true;
    g_grass_mask_seeded=true;
    printf("[GrassPaint] Seeded mask from procedural grass\n");
}

static void grass_mask_save(void){
    FILE *f=fopen(GRASS_MASK_FILE,"wb");
    if(f){
        fwrite(g_grass_mask.d,sizeof(g_grass_mask.d),1,f);
        fclose(f);
        printf("[GrassPaint] Saved to %s\n",GRASS_MASK_FILE);
    }
}

static void grass_mask_load(void){
    FILE *f=fopen(GRASS_MASK_FILE,"rb");
    if(!f) return;
    if(fread(g_grass_mask.d,sizeof(g_grass_mask.d),1,f)==1){
        g_grass_mask_active=true;
        g_grass_mask_seeded=true;
        printf("[GrassPaint] Loaded from %s\n",GRASS_MASK_FILE);
    }
    fclose(f);
}

static void grass_mask_brush(float cx,float cz,float radius,float strength,bool erase){
    if(!g_grass_mask_seeded) grass_mask_seed_from_terrain();
    float inv_w=(float)(GRASS_MASK_RES-1)/(WORLD_HALF*2.0f);
    float inv_w2=(WORLD_HALF*2.0f)/(float)(GRASS_MASK_RES-1);
    float mcx=(cx+WORLD_HALF)*inv_w;
    float mcz=(cz+WORLD_HALF)*inv_w;
    float mr=radius*inv_w;
    int x0=(int)(mcx-mr-1); if(x0<0)x0=0;
    int x1=(int)(mcx+mr+1); if(x1>=GRASS_MASK_RES)x1=GRASS_MASK_RES-1;
    int z0=(int)(mcz-mr-1); if(z0<0)z0=0;
    int z1=(int)(mcz+mr+1); if(z1>=GRASS_MASK_RES)z1=GRASS_MASK_RES-1;
    for(int iz=z0;iz<=z1;iz++){
        for(int ix=x0;ix<=x1;ix++){
            float dx=(float)ix-mcx, dz=(float)iz-mcz;
            float d=sqrtf(dx*dx+dz*dz);
            if(d>mr) continue;
            float t=d/mr;
            float w=(1.0f-t*t*(3.0f-2.0f*t))*strength*4.0f;
            int idx=iz*GRASS_MASK_RES+ix;
            float cur=(float)g_grass_mask.d[idx];
            if(erase){
                cur-=255.0f*w;
            }else{
                float wx2=(float)ix*inv_w2-WORLD_HALF;
                float wz2=(float)iz*inv_w2-WORLD_HALF;
                if(!grass_mask_base_allowed(wx2,wz2)) continue;
                cur+=255.0f*w;
            }
            cur=clampf(cur,0.0f,255.0f);
            g_grass_mask.d[idx]=(uint8_t)(cur+0.5f);
        }
    }
    g_grass_mask_active=true;
}

/* ══════════════════════════════════════════════════════════════
   WORN PATH MASK  (subtle dirt/traffic hints)
   0 = none, 255 = strongest worn path.
   ══════════════════════════════════════════════════════════════ */
#define WORN_MASK_RES   512
#define WORN_MASK_FILE  "worn_path_mask.bin"
typedef struct{ uint8_t d[WORN_MASK_RES*WORN_MASK_RES]; } WornMask;
static WornMask g_worn_mask;
static bool g_worn_mask_active=false;

static float worn_path_sample(float wx,float wz){
    if(!g_worn_mask_active) return 0.0f;
    float su=(wx+WORLD_HALF)/(WORLD_HALF*2.0f)*(float)(WORN_MASK_RES-1);
    float sv=(wz+WORLD_HALF)/(WORLD_HALF*2.0f)*(float)(WORN_MASK_RES-1);
    su=clampf(su,0.0f,(float)(WORN_MASK_RES-1));
    sv=clampf(sv,0.0f,(float)(WORN_MASK_RES-1));
    int ix=(int)su, iz=(int)sv;
    float fx=su-(float)ix, fz=sv-(float)iz;
    int ix2=ix<WORN_MASK_RES-1?ix+1:ix;
    int iz2=iz<WORN_MASK_RES-1?iz+1:iz;
    float v00=(float)g_worn_mask.d[iz *WORN_MASK_RES+ix ]*(1.0f/255.0f);
    float v10=(float)g_worn_mask.d[iz *WORN_MASK_RES+ix2]*(1.0f/255.0f);
    float v01=(float)g_worn_mask.d[iz2*WORN_MASK_RES+ix ]*(1.0f/255.0f);
    float v11=(float)g_worn_mask.d[iz2*WORN_MASK_RES+ix2]*(1.0f/255.0f);
    return v00*(1.0f-fx)*(1.0f-fz)+v10*fx*(1.0f-fz)+v01*(1.0f-fx)*fz+v11*fx*fz;
}

static void worn_mask_upload(Terrain *t){
    if(!t->worn_mask_tex) glGenTextures(1,&t->worn_mask_tex);
    glBindTexture(GL_TEXTURE_2D,t->worn_mask_tex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_R8,WORN_MASK_RES,WORN_MASK_RES,0,GL_RED,GL_UNSIGNED_BYTE,g_worn_mask.d);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
}

static void worn_mask_save(void){
    FILE *f=fopen(WORN_MASK_FILE,"wb");
    if(f){
        fwrite(g_worn_mask.d,sizeof(g_worn_mask.d),1,f);
        fclose(f);
        printf("[WornPath] Saved to %s\n",WORN_MASK_FILE);
    }
}

static void worn_mask_load(Terrain *t){
    FILE *f=fopen(WORN_MASK_FILE,"rb");
    if(!f){
        worn_mask_upload(t);
        return;
    }
    if(fread(g_worn_mask.d,sizeof(g_worn_mask.d),1,f)==1){
        g_worn_mask_active=true;
        printf("[WornPath] Loaded from %s\n",WORN_MASK_FILE);
    }
    fclose(f);
    worn_mask_upload(t);
}

static void worn_mask_brush(Terrain *t,float cx,float cz,float radius,float strength,bool erase){
    float inv_w=(float)(WORN_MASK_RES-1)/(WORLD_HALF*2.0f);
    float mcx=(cx+WORLD_HALF)*inv_w;
    float mcz=(cz+WORLD_HALF)*inv_w;
    float mr=radius*inv_w;
    int x0=(int)(mcx-mr-1); if(x0<0)x0=0;
    int x1=(int)(mcx+mr+1); if(x1>=WORN_MASK_RES)x1=WORN_MASK_RES-1;
    int z0=(int)(mcz-mr-1); if(z0<0)z0=0;
    int z1=(int)(mcz+mr+1); if(z1>=WORN_MASK_RES)z1=WORN_MASK_RES-1;
    for(int iz=z0;iz<=z1;iz++){
        for(int ix=x0;ix<=x1;ix++){
            float dx=(float)ix-mcx, dz=(float)iz-mcz;
            float d=sqrtf(dx*dx+dz*dz);
            if(d>mr) continue;
            float t0=d/mr;
            float w=(1.0f-t0*t0*(3.0f-2.0f*t0))*strength*3.0f;
            int idx=iz*WORN_MASK_RES+ix;
            float cur=(float)g_worn_mask.d[idx];
            cur += erase ? -255.0f*w : 255.0f*w;
            g_worn_mask.d[idx]=(uint8_t)(clampf(cur,0.0f,255.0f)+0.5f);
        }
    }
    g_worn_mask_active=true;
    worn_mask_upload(t);
}

/* Save river points to file and print C array to stdout */
static void criver_save(void){
    FILE *f=fopen("river_custom.txt","w");
    if(f){
        fprintf(f,"# Custom river — %d points\n",g_criver.count);
        fprintf(f,"# width %.1f\n",g_criver.width);
        fprintf(f,"# water_preset %d\n",g_wstyle.preset_idx);
        for(int i=0;i<g_criver.count;i++)
            fprintf(f,"%.1f %.1f\n",
                    (double)g_criver.pts[i*2],
                    (double)g_criver.pts[i*2+1]);
        fclose(f);
    }
    /* Print as C array so you can paste into the hardcoded rivers */
    printf("[RiverEd] C array:\nstatic const float river_custom[]={\n    ");
    for(int i=0;i<g_criver.count;i++){
        printf("%.0ff,%.0ff",
               (double)g_criver.pts[i*2],
               (double)g_criver.pts[i*2+1]);
        if(i<g_criver.count-1)printf(", ");
        if((i+1)%4==0&&i<g_criver.count-1)printf("\n    ");
    }
    printf("};\n");
    printf("[RiverEd] Saved %d points to river_custom.txt\n",g_criver.count);
}

/* Load river points from file (called at startup if file exists) */
static void criver_load(void){
    FILE *f=fopen("river_custom.txt","r");
    if(!f)return;
    char line[64];
    g_criver.count=0;
    while(fgets(line,sizeof(line),f)&&g_criver.count<CRIVER_MAX_PTS){
        if(line[0]=='#'){
            float w; if(sscanf(line,"# width %f",&w)==1) g_criver.width=w;
            int pi; if(sscanf(line,"# water_preset %d",&pi)==1) wstyle_apply_preset(pi);
            continue;
        }
        float x,z;
        if(sscanf(line,"%f %f",&x,&z)==2){
            g_criver.pts[g_criver.count*2  ]=x;
            g_criver.pts[g_criver.count*2+1]=z;
            g_criver.count++;
        }
    }
    fclose(f);
    if(g_criver.width<=0.0f) g_criver.width=CRIVER_WIDTH; /* default if not in file */
    g_criver.dirty=(g_criver.count>0);
    printf("[RiverEd] Loaded %d points from river_custom.txt\n",g_criver.count);
}


/* ══════════════════════════════════════════════════════════════
   TERRAIN SCULPT MAP  (F9 editor)
   512×512 float delta array — bilinear sampled in terrain_h.
   Saved/loaded as a raw binary so no precision is lost.
   ══════════════════════════════════════════════════════════════ */
#define SCULPT_RES   512
#define SCULPT_FILE  "terrain_sculpt.bin"

typedef struct {
    float d[SCULPT_RES * SCULPT_RES];  /* height deltas, world-space Y */
} SculptMap;

static SculptMap g_sculpt;            /* zero-initialised = no sculpt */
static bool      g_sculpt_active = false; /* true once any cell is non-zero */

static float sculpt_sample(float wx, float wz){
    if(!g_sculpt_active) return 0.0f;
    float su = (wx + WORLD_HALF) / (WORLD_HALF * 2.0f) * (float)(SCULPT_RES - 1);
    float sv = (wz + WORLD_HALF) / (WORLD_HALF * 2.0f) * (float)(SCULPT_RES - 1);
    su = clampf(su, 0.0f, (float)(SCULPT_RES - 1));
    sv = clampf(sv, 0.0f, (float)(SCULPT_RES - 1));
    int ix = (int)su, iz = (int)sv;
    float fx = su - (float)ix, fz = sv - (float)iz;
    int ix2 = ix < SCULPT_RES - 1 ? ix + 1 : ix;
    int iz2 = iz < SCULPT_RES - 1 ? iz + 1 : iz;
    float v00 = g_sculpt.d[iz  * SCULPT_RES + ix ];
    float v10 = g_sculpt.d[iz  * SCULPT_RES + ix2];
    float v01 = g_sculpt.d[iz2 * SCULPT_RES + ix ];
    float v11 = g_sculpt.d[iz2 * SCULPT_RES + ix2];
    return v00*(1.0f-fx)*(1.0f-fz) + v10*fx*(1.0f-fz)
         + v01*(1.0f-fx)*fz        + v11*fx*fz;
}

static void sculpt_save(void){
    FILE *f = fopen(SCULPT_FILE, "wb");
    if(f){ fwrite(g_sculpt.d, sizeof(g_sculpt.d), 1, f); fclose(f);
        printf("[SculptEd] Saved to %s\n", SCULPT_FILE); }
}

static void sculpt_load(void){
    FILE *f = fopen(SCULPT_FILE, "rb");
    if(f){ fread(g_sculpt.d, sizeof(g_sculpt.d), 1, f); fclose(f);
        g_sculpt_active = true;
        printf("[SculptEd] Loaded from %s\n", SCULPT_FILE); }
}

/* terrain_h_base: procedural height WITHOUT sculpt delta (forward decl) */
static float terrain_h_base(float wx, float wz);

/* Apply one brush stroke — called every frame while LMB held.
   mode: 0=raise  1=lower  2=flatten  3=smooth
   flatten_abs: target absolute world-space height for flatten mode  */
static void sculpt_brush(float cx, float cz, float radius, float strength,
                         int mode, float flatten_abs){
    g_sculpt_active = true; /* mark map dirty on any brush stroke */
    float inv_w  = (float)(SCULPT_RES - 1) / (WORLD_HALF * 2.0f);
    float inv_w2 = (WORLD_HALF * 2.0f) / (float)(SCULPT_RES - 1); /* cell → world */
    float sc_cx = (cx + WORLD_HALF) * inv_w;
    float sc_cz = (cz + WORLD_HALF) * inv_w;
    float sc_r  = radius * inv_w;
    int x0 = (int)(sc_cx - sc_r - 1);  x0 = x0 < 0 ? 0 : x0;
    int x1 = (int)(sc_cx + sc_r + 1);  x1 = x1 >= SCULPT_RES ? SCULPT_RES-1 : x1;
    int z0 = (int)(sc_cz - sc_r - 1);  z0 = z0 < 0 ? 0 : z0;
    int z1 = (int)(sc_cz + sc_r + 1);  z1 = z1 >= SCULPT_RES ? SCULPT_RES-1 : z1;

    for(int iz = z0; iz <= z1; iz++){
        for(int ix = x0; ix <= x1; ix++){
            float dx = (float)ix - sc_cx, dz = (float)iz - sc_cz;
            float d  = sqrtf(dx*dx + dz*dz);
            if(d > sc_r) continue;
            float t = d / sc_r;
            float w = (1.0f - t*t*(3.0f - 2.0f*t)) * strength;
            float *cell = &g_sculpt.d[iz * SCULPT_RES + ix];
            if(mode == 0)      *cell += w;
            else if(mode == 1) *cell -= w;
            else if(mode == 2){
                /* Flatten: compute what delta is needed so that
                   base_h(cell) + delta = flatten_abs              */
                float wx2 = (float)ix * inv_w2 - WORLD_HALF;
                float wz2 = (float)iz * inv_w2 - WORLD_HALF;
                float base = terrain_h_base(wx2, wz2);
                float need = flatten_abs - base; /* required delta */
                *cell += (need - *cell) * w;
            }
            else if(mode == 3){
                float sum = 0.0f; int cnt = 0;
                for(int nz=-1;nz<=1;nz++) for(int nx=-1;nx<=1;nx++){
                    int ni=iz+nz, nxi=ix+nx;
                    if(ni<0||ni>=SCULPT_RES||nxi<0||nxi>=SCULPT_RES)continue;
                    sum += g_sculpt.d[ni*SCULPT_RES+nxi]; cnt++;
                }
                *cell += (sum/(float)cnt - *cell) * w;
            }
            else if(mode == 4){
                /* Cliff: inner half raised hard, outer half cut sharp.
                   Creates a vertical wall at the brush edge.            */
                float edge = t > 0.5f ? 1.0f : 0.0f; /* step at 50% radius */
                if(edge == 0.0f) *cell += strength * 8.0f; /* raise inner  */
                else             *cell -= strength * 2.0f; /* cut outer    */
            }
            else if(mode == 5){
                /* Erase: pull delta back toward 0 regardless of sign.
                   RMB eraser — works against any mode including cliff. */
                *cell -= *cell * w * 3.0f;
                if(fabsf(*cell) < 0.01f) *cell = 0.0f;
            }
        }
    }
}

/* Layered noise using trig – no external library needed */
static float tnoise(float x,float z,float s){
    return (sinf(x*s)*cosf(z*s*0.71f)
           +sinf(x*s*1.6f+1.3f)*cosf(z*s*2.1f+0.7f)*0.5f
           +sinf(x*s*3.7f+2.9f)*cosf(z*s*2.9f+1.8f)*0.25f)*0.57f;
}

/* Village flat zone table – all 15 settlements                  */
/* Format: wx, wz, flat_radius, blend_extra, target_height       */
static const float VILLAGES[][5]={
    /* Great Mashito Mountains */
    {-2050, -2200,  40, 60,  28.0f}, /* Brandalhall  */
    {-1900, -1800, 160, 240,   3.5f}, /* Callicut  (spawn – wide flat, near sea level) */
    {-2150, -1050,  30, 50,  12.0f}, /* Rustpatch    */
    /* Valwood / North */
    { -100, -2000,  35, 55,   8.0f}, /* Ulasse        */
    {  600, -1700,  30, 50,  10.0f}, /* Baldurne      */
    { 2050, -2150,  35, 55,  18.0f}, /* Gorshett Keep */
    {   50,  -800,  50, 80,   6.0f}, /* Valdoa (castle) */
    /* Old Tanthoon / Center-left */
    {-1200,  -500,  30, 50,   5.0f}, /* Ishome        */
    { -650,   500,  30, 50,   4.0f}, /* Seldonne      */
    /* Center & East */
    {  350,    50, 220,320,   3.2f}, /* Estuay (starter flatland) */
    { 1450,  -250,  50, 80,   5.0f}, /* Kerone City   */
    /* South */
    { 1100,   650,  30, 50,   4.0f}, /* Etarma        */
    {  350,  1100,  25, 45,   3.0f}, /* Fallaway      */
    {-1100,  1150,  25, 45,   3.5f}, /* Sclay         */
    { 2000,  1650,  40, 65,   2.0f}, /* Bellweather   */
};
#define N_VILLAGES 15
#define START_VILLAGE_INDEX 9 /* Estuay */

static float terrain_h(float wx, float wz){
    float u=(wx+WORLD_HALF)/(WORLD_HALF*2.0f);
    float v=(wz+WORLD_HALF)/(WORLD_HALF*2.0f);

#define SS(t) ((t)*(t)*(3.0f-2.0f*(t)))
#define SSC(x,lo,hi) SS(clampf(((x)-(lo))/((hi)-(lo)),0,1))

    float macro = tnoise(wx,wz,0.0025f)*10.0f
                + tnoise(wx,wz,0.007f) * 3.5f
                + tnoise(wx,wz,0.018f) * 1.2f;

    float mw = SSC(u, 0.42f, 0.06f)
             * SSC(v, 0.52f, 0.04f);
    float mtn_detail = tnoise(wx,wz,0.005f)*0.5f + tnoise(wx,wz,0.012f)*0.3f;
    float mtn_h = mw * (0.6f + mtn_detail) * 155.0f;

    float fw = SSC(u, 0.55f, 0.18f)
             * SSC(v, 0.60f, 0.08f)
             * (1.0f - mw*0.85f);
    float foot_h = fw * (0.35f + tnoise(wx,wz,0.008f)*0.4f) * 32.0f;

    float vw = rw2(wx,wz,-200,-1600,1300) * (1.0f - mw*0.9f);
    float valw_h = vw * (0.55f + tnoise(wx,wz,0.018f)*0.3f) * 14.0f;

    float pw_u = SSC(u, 0.22f, 0.45f) * SSC(u, 0.88f, 0.62f);
    float pw_v = SSC(v, 0.18f, 0.38f) * SSC(v, 0.78f, 0.58f);
    float plain_w = pw_u * pw_v * (1.0f - mw*0.9f) * (1.0f - fw*0.7f);
    float plain_h = plain_w * (2.5f + tnoise(wx,wz,0.006f)*2.0f);

    float sw = SSC(u, 0.50f, 0.68f) * SSC(u, 1.0f, 0.80f)
             * SSC(v, 0.32f, 0.48f) * SSC(v, 0.92f, 0.75f);
    float stomp_h = sw * (1.8f + tnoise(wx,wz,0.010f)*3.5f);

    float wallw = SSC(u, 0.68f, 0.82f) * SSC(v, 0.38f, 0.08f);
    float wall_h = wallw * (0.45f + tnoise(wx,wz,0.018f)*0.4f) * 20.0f;

    float coast_pull = SSC(v, 0.68f, 0.84f) * 5.0f;
    float sea_pull = SSC(u, 0.84f, 0.95f) * SSC(v, 0.72f, 0.88f) * 12.0f;

    float h = mtn_h + foot_h + valw_h + plain_h + stomp_h + wall_h
            + macro - coast_pull - sea_pull;
    h = fmaxf(h, -5.0f);

    /* Lakes */
    {   float d=sqrtf((wx+150)*(wx+150)+(wz+1550)*(wz+1550));
        float t=clampf(1.0f-d/420.0f,0,1); t=t*t*(3-2*t)*t;
        h = h*(1.0f-t*0.97f) - 1.2f*t; }
    {   float d=sqrtf((wx-900)*(wx-900)+(wz+350)*(wz+350));
        float t=clampf(1.0f-d/320.0f,0,1); t=t*t*(3-2*t)*t;
        h = h*(1.0f-t*0.97f) - 1.0f*t; }
    {   float d=sqrtf((wx+700)*(wx+700)+(wz+200)*(wz+200));
        float t=clampf(1.0f-d/260.0f,0,1); t=t*t*(3-2*t)*t;
        h = h*(1.0f-t*0.96f) - 0.8f*t; }
    {   float d=sqrtf((wx+1600)*(wx+1600)+(wz+1200)*(wz+1200));
        float t=clampf(1.0f-d/180.0f,0,1); t=t*t*(3-2*t)*t;
        float target=18.0f;
        h = h*(1.0f-t*0.92f) + target*t*0.92f; }

    /* Rivers */
    float rdist=builtin_river_dist(wx,wz);

    if(rdist<100.0f){
        float rt=clampf(1.0f-rdist/100.0f,0,1); rt=rt*rt*rt;
        float target=WATER_LEVEL-CRIVER_DEPTH*rt;
        h=h*(1.0f-rt)+target*rt;
    }

    /* Water zone carving */
    for(int zi=0;zi<g_wzone_count;zi++){
        WaterZone *z=&g_wzones[zi];
        if(!z->active)continue;
        float dx=wx-z->cx, dz2=wz-z->cz;
        float d=sqrtf(dx*dx+dz2*dz2);
        if(d<z->radius){
            float t=clampf(1.0f-d/z->radius,0,1);
            t=t*t*(3.0f-2.0f*t);
            float target=z->level-3.0f*t;
            if(target<h) h=h*(1.0f-t)+target*t;
        }
    }

#undef SS
#undef SSC
    h += sculpt_sample(wx, wz);
    return h;
}
/* terrain_h without sculpt delta — used by flatten brush */
static float terrain_h_base(float wx, float wz){
    return terrain_h(wx, wz) - sculpt_sample(wx, wz);
}

static Vec3 terrain_color(float h){
    if(h<WATER_LEVEL)      return (Vec3){0.12f,0.28f,0.62f};
    if(h<WATER_LEVEL+0.3f) return (Vec3){0.76f,0.70f,0.50f};
    if(h<WATER_LEVEL+3.5f) return (Vec3){0.25f,0.52f,0.18f};
    if(h<WATER_LEVEL+5.5f) return (Vec3){0.50f,0.48f,0.42f};
    return                        (Vec3){0.90f,0.90f,0.95f};
}

/* Build one chunk's VAO for a given LOD step (step=1,2,4) */
static void terrain_rebake_hmap(Terrain *t); /* defined after terrain_build */
static GLuint terrain_build_chunk_lod(float ox,float oz,int step,int *out_ic){
    /* Shared vertex grid for this chunk at native res, sampled every `step` cells */
    int gsz=TCSZ/step;  /* grid quads per side at this LOD */
    int vc=(gsz+1)*(gsz+1);
    int ic=gsz*gsz*6;
    TVert    *verts=malloc((size_t)vc *sizeof(TVert));
    uint32_t *inds =malloc((size_t)ic *sizeof(uint32_t));
    float cell=TERRAIN_CELL*(float)step;
    for(int z=0;z<=gsz;z++)for(int x=0;x<=gsz;x++){
        float wx=ox+(float)x*cell, wz=oz+(float)z*cell;
        float wy=terrain_h(wx,wz);
        TVert *v=&verts[z*(gsz+1)+x];
        v->pos[0]=wx; v->pos[1]=wy; v->pos[2]=wz;
        float s=TERRAIN_CELL*0.5f;
        float hL=terrain_h(wx-s,wz),hR=terrain_h(wx+s,wz);
        float hD=terrain_h(wx,wz-s),hU=terrain_h(wx,wz+s);
        Vec3 n=vec3_norm((Vec3){hL-hR,2.0f*s,hD-hU});
        v->normal[0]=n.x;v->normal[1]=n.y;v->normal[2]=n.z;
        Vec3 c=terrain_color(wy);v->color[0]=c.x;v->color[1]=c.y;v->color[2]=c.z;
    }
    int idx=0;
    for(int z=0;z<gsz;z++)for(int x=0;x<gsz;x++){
        uint32_t tl=(uint32_t)(z*(gsz+1)+x),tr=tl+1,
                 bl=tl+(uint32_t)(gsz+1),   br=bl+1;
        inds[idx++]=tl;inds[idx++]=bl;inds[idx++]=tr;
        inds[idx++]=tr;inds[idx++]=bl;inds[idx++]=br;
    }
    GLH_Attrib a[]={
        {0,3,GL_FLOAT,GL_FALSE,offsetof(TVert,pos)},
        {1,3,GL_FLOAT,GL_FALSE,offsetof(TVert,normal)},
        {2,3,GL_FLOAT,GL_FALSE,offsetof(TVert,color)}};
    GLuint vao=GLH_mesh(verts,(size_t)vc*sizeof(TVert),
                        inds, (size_t)ic*sizeof(uint32_t),
                        sizeof(TVert),a,3);
    free(verts);free(inds);
    *out_ic=ic;
    return vao;
}

static GLuint river_build_ribbon_vao(const float *pts,int n,float half_width,float y,int *out_ic){
    int pc=n/2;
    if(pc<2){ if(out_ic) *out_ic=0; return 0; }
    int segc=pc-1;
    int vc=segc*4;
    int ic=segc*6;
    TVert *verts=malloc((size_t)vc*sizeof(TVert));
    uint32_t *inds=malloc((size_t)ic*sizeof(uint32_t));
    if(!verts||!inds){
        free(verts);
        free(inds);
        if(out_ic) *out_ic=0;
        return 0;
    }
    for(int i=0;i<segc;i++){
        float ax=pts[i*2], ay=y, az=pts[i*2+1];
        float bx=pts[(i+1)*2], by=y, bz=pts[(i+1)*2+1];
        float dx=bx-ax, dz=bz-az;
        float len=sqrtf(dx*dx+dz*dz);
        if(len<0.001f){ dx=1.0f; dz=0.0f; len=1.0f; }
        dx/=len; dz/=len;
        float rx=-dz*half_width, rz=dx*half_width;
        int vi=i*4;
        verts[vi+0]=(TVert){{ax-rx,ay,az-rz},{0,1,0},{0.08f,0.25f,0.55f}};
        verts[vi+1]=(TVert){{ax+rx,ay,az+rz},{0,1,0},{0.08f,0.25f,0.55f}};
        verts[vi+2]=(TVert){{bx-rx,by,bz-rz},{0,1,0},{0.08f,0.25f,0.55f}};
        verts[vi+3]=(TVert){{bx+rx,by,bz+rz},{0,1,0},{0.08f,0.25f,0.55f}};
        {
            int ii=i*6;
            uint32_t a=(uint32_t)vi;
            uint32_t b=(uint32_t)(vi+1);
            uint32_t c=(uint32_t)(vi+2);
            uint32_t d=(uint32_t)(vi+3);
            inds[ii+0]=a; inds[ii+1]=c; inds[ii+2]=b;
            inds[ii+3]=b; inds[ii+4]=c; inds[ii+5]=d;
        }
    }
    GLH_Attrib a[]={
        {0,3,GL_FLOAT,GL_FALSE,offsetof(TVert,pos)},
        {1,3,GL_FLOAT,GL_FALSE,offsetof(TVert,normal)},
        {2,3,GL_FLOAT,GL_FALSE,offsetof(TVert,color)}
    };
    GLuint vao=GLH_mesh(verts,(size_t)vc*sizeof(TVert),
                        inds,(size_t)ic*sizeof(uint32_t),
                        sizeof(TVert),a,3);
    free(verts);
    free(inds);
    if(out_ic) *out_ic=ic;
    return vao;
}

static void terrain_build(Terrain *t){
    memset(t,0,sizeof(*t));
    float world_ox=-(TERRAIN_COLS*TERRAIN_CELL)*0.5f;
    float world_oz=-(TERRAIN_ROWS*TERRAIN_CELL)*0.5f;
    float chunk_w =TCSZ*TERRAIN_CELL;
    int lod_steps[TC_LODS]={1,2,4};
    printf("[Terrain] Building %dx%d chunks × %d LODs…\n",TCNX,TCNZ,TC_LODS);
    for(int cz=0;cz<TCNZ;cz++)for(int cx=0;cx<TCNX;cx++){
        TerrainChunk *ch=&t->chunks[cz*TCNX+cx];
        float ox=world_ox+(float)cx*chunk_w;
        float oz=world_oz+(float)cz*chunk_w;
        ch->cx=ox+chunk_w*0.5f;
        ch->cz=oz+chunk_w*0.5f;
        for(int l=0;l<TC_LODS;l++)
            ch->vao[l]=terrain_build_chunk_lod(ox,oz,lod_steps[l],&ch->ic[l]);
    }
    /* Water plane */
    float W=TERRAIN_COLS*TERRAIN_CELL, D=TERRAIN_ROWS*TERRAIN_CELL;
    float wv[]={-W,WATER_LEVEL+0.03f,-D,0,1,0,0.08f,0.25f,0.55f,
                 W,WATER_LEVEL+0.03f,-D,0,1,0,0.08f,0.25f,0.55f,
                 W,WATER_LEVEL+0.03f, D,0,1,0,0.08f,0.25f,0.55f,
                -W,WATER_LEVEL+0.03f, D,0,1,0,0.08f,0.25f,0.55f};
    uint32_t wi[]={0,1,2,0,2,3};
    GLH_Attrib wa[]={{0,3,GL_FLOAT,GL_FALSE,0},{1,3,GL_FLOAT,GL_FALSE,12},{2,3,GL_FLOAT,GL_FALSE,24}};
    t->water_vao=GLH_mesh(wv,sizeof(wv),wi,sizeof(wi),9*sizeof(float),wa,3);
    for(int ri=0;ri<BUILTIN_RIVER_COUNT;ri++){
        t->river_vaos[ri]=river_build_ribbon_vao(
            g_builtin_rivers[ri].pts,
            g_builtin_rivers[ri].count,
            BUILTIN_RIVER_RENDER_HALF_WIDTH,
            WATER_LEVEL+BUILTIN_RIVER_SURFACE_OFFSET,
            &t->river_ic[ri]);
    }
    t->land_prog=GLH_program(
        /* ── Vertex ── */
        "#version 330 core\n"
        "layout(location=0)in vec3 aPos;\n"
        "layout(location=1)in vec3 aNormal;\n"
        "layout(location=2)in vec3 aColor;\n"
        "uniform mat4 uProj,uView;\n"
        "out vec3 vNormal,vPos,vColor;\n"
        /* Build TBN from terrain normal — we know up is roughly Y so we can
           derive tangent as the X-axis projected perpendicular to normal      */
        "out vec3 vTangent,vBitangent;\n"
        "void main(){\n"
        "    gl_Position=uProj*uView*vec4(aPos,1);\n"
        "    vNormal=aNormal;vPos=aPos;vColor=aColor;\n"
        "    vec3 up=abs(aNormal.y)>0.9?vec3(1,0,0):vec3(0,1,0);\n"
        "    vTangent=normalize(cross(up,aNormal));\n"
        "    vBitangent=normalize(cross(aNormal,vTangent));\n"
        "}\n",
        /* ── Fragment ── */
        "#version 330 core\n"
        "in vec3 vNormal,vPos,vColor;\n"
        "in vec3 vTangent,vBitangent;\n"
        "uniform float uTime,uSnowAccum,uWetness,uFogDensity;\n"
        "uniform vec3 uCamPos,uFogColor;\n"
        "uniform vec2 uWorldMin,uWorldSize;\n"
        /* albedo maps (6 biomes) */
        "uniform sampler2D uTexSand,uTexGrass,uTexDirt,uTexRock,uTexSnow,uTexLava;\n"
        "uniform sampler2D uWearMask;\n"
        /* normal maps — same slots offset by 6 */
        "uniform sampler2D uNrmSand,uNrmGrass,uNrmDirt,uNrmRock,uNrmSnow;\n"
        /* roughness/AO packed: R=roughness G=AO */
        "uniform sampler2D uRghSand,uRghGrass,uRghDirt,uRghRock,uRghSnow;\n"
        /* flags so we gracefully skip missing maps */
        "uniform bool uHasNrm,uHasRgh;\n"
        LIGHTING_UNIFORMS
        "out vec4 FragColor;\n"

        /* sample albedo at two scales and blend for detail/macro variation */
        "vec3 stex(sampler2D s,float sc){\n"
        "    return texture(s,vPos.xz*sc).rgb*0.7\n"
        "          +texture(s,vPos.xz*sc*0.22).rgb*0.3;\n"
        "}\n"
        /* unpack normal map into world-space normal */
        "vec3 snrm(sampler2D s,float sc){\n"
        "    vec3 t=texture(s,vPos.xz*sc).rgb*2.0-1.0;\n"
        "    return normalize(mat3(vTangent,vBitangent,vNormal)*t);\n"
        "}\n"
        "void main(){\n"
        "    float h=vPos.y;\n"
        "    vec3 geo_n=normalize(vNormal);\n"
        "    float slope=clamp(1.0-geo_n.y,0.0,1.0);\n"

        /* biome blend weights */
        "    float t_grass=clamp((h-0.50)/3.0,0.0,1.0);\n"
        "    float t_dirt =clamp(slope*2.5-0.4,0.0,1.0);\n"
        "    float snow_base=mix(55.0,16.0,uSnowAccum);\n"
        "    float t_rock =clamp((h-18.0)/12.0,0.0,1.0);\n"
        "    float t_snow =clamp((h-snow_base)/20.0,0.0,1.0);\n"
        "    float snow_flat=clamp((1.0-slope*4.0),0.0,1.0);\n"
        "    float snow_mix=t_snow*mix(0.4,1.0,snow_flat);\n"
        "    float snow_patch=clamp((sin(vPos.x*0.008+vPos.z*0.012)*0.5+0.5)*2.0-0.6,0.0,1.0)\n"
        "                    *clamp((h-40.0)/15.0,0.0,1.0);\n"
        "    float global_cover=uSnowAccum*snow_flat*0.28;\n"
        "    float snow_w=clamp(snow_mix+snow_patch*0.35+global_cover,0.0,1.0);\n"

        /* ── albedo blend ── */
        "    vec3 c_sand =stex(uTexSand, 0.18);\n"
        "    vec3 c_grass=stex(uTexGrass,0.25);\n"
        "    vec3 c_dirt =stex(uTexDirt, 0.22);\n"
        "    vec3 c_rock =stex(uTexRock, 0.20);\n"
        "    vec3 c_snow =stex(uTexSnow, 0.15);\n"
        "    vec3 base=c_sand;\n"
        "    base=mix(base,c_grass,t_grass);\n"
        "    base=mix(base,c_dirt, t_dirt);\n"
        "    base=mix(base,c_rock, t_rock);\n"
        "    base=mix(base,c_snow, snow_w);\n"
        "    vec2 wearUV=clamp((vPos.xz-uWorldMin)/uWorldSize,vec2(0.0),vec2(1.0));\n"
        "    float wear=texture(uWearMask,wearUV).r;\n"
        "    wear=smoothstep(0.10,0.85,wear);\n"
        "    vec3 wornCol=mix(c_dirt*0.82+vec3(0.03,0.02,0.01),c_sand*0.78,clamp(h/8.0,0.0,1.0));\n"
        "    base=mix(base,wornCol,wear*0.78);\n"

        /* ── normal map blend (only if textures loaded) ── */
        "    vec3 n=geo_n;\n"
        "    if(uHasNrm){\n"
        "        vec3 n_sand =snrm(uNrmSand, 0.18);\n"
        "        vec3 n_grass=snrm(uNrmGrass,0.25);\n"
        "        vec3 n_dirt =snrm(uNrmDirt, 0.22);\n"
        "        vec3 n_rock =snrm(uNrmRock, 0.20);\n"
        "        vec3 n_snow =snrm(uNrmSnow, 0.15);\n"
        "        n=n_sand;\n"
        "        n=normalize(mix(n,n_grass,t_grass));\n"
        "        n=normalize(mix(n,n_dirt, t_dirt));\n"
        "        n=normalize(mix(n,n_rock, t_rock));\n"
        "        n=normalize(mix(n,n_snow, snow_w));\n"
        "    }\n"

        /* ── roughness/AO blend ── */
        "    float roughness=0.85,ao=1.0;\n"
        "    if(uHasRgh){\n"
        "        vec2 rg_sand =texture(uRghSand, vPos.xz*0.18).rg;\n"
        "        vec2 rg_grass=texture(uRghGrass,vPos.xz*0.25).rg;\n"
        "        vec2 rg_dirt =texture(uRghDirt, vPos.xz*0.22).rg;\n"
        "        vec2 rg_rock =texture(uRghRock, vPos.xz*0.20).rg;\n"
        "        vec2 rg_snow =texture(uRghSnow, vPos.xz*0.15).rg;\n"
        "        vec2 rg=rg_sand;\n"
        "        rg=mix(rg,rg_grass,t_grass);\n"
        "        rg=mix(rg,rg_dirt, t_dirt);\n"
        "        rg=mix(rg,rg_rock, t_rock);\n"
        "        rg=mix(rg,rg_snow, snow_w);\n"
        "        rg.g=mix(rg.g,0.96,wear*0.65);\n"
        "        rg.r=mix(rg.r,1.0,wear*0.20);\n"
        "        roughness=rg.g; ao=rg.r;\n"
        "    }\n"

        /* ── wet darkening ── */
        "    float wet_flat=snow_flat*(1.0-clamp(snow_mix*2.0,0.0,1.0));\n"
        "    base*=1.0-uWetness*wet_flat*0.38;\n"

        /* ── lava ── */
        "    float dist=length(vPos.xz);\n"
        "    float lnoise=sin(vPos.x*0.006+1.70)*cos(vPos.z*0.0054+0.90)\n"
        "                +sin(vPos.x*0.0106-vPos.z*0.0082)*0.50+cos(vPos.x*0.0038+vPos.z*0.0126)*0.30;\n"
        "    float lava_mask=clamp((lnoise-0.55)*4.0,0.0,1.0)*clamp((dist-600.0)/200.0,0.0,1.0)*clamp(1.0-h/8.0,0.0,1.0);\n"
        "    vec3 c_lava=vec3(0.9,0.25,0.02);float pulse=sin(uTime*2.2+vPos.x*0.05+vPos.z*0.04)*0.5+0.5;\n"
        "    base=mix(base,c_lava+vec3(pulse*0.15,0,0),lava_mask);\n"

        /* ── lighting with AO ── */
        "    base*=ao;\n"
        "    vec3 shadow_n=geo_n;\n"
        "    float sunFade=clamp(uSunElev*10.0,0.0,1.0);\n"
        "    float terrainShadowFade=smoothstep(140.0,300.0,length(vPos.xz-uCamPos.xz));\n"
        "    float shadow=0.0;\n"
        "    float NdotL=dot(shadow_n,-uSunDir);\n"
        "    if(NdotL<=0.0){ shadow=1.0*sunFade; }\n"
        "    else if(sunFade>0.0){\n"
        "    float slopeBias=clamp(1.0-NdotL,0.0,1.0);\n"
        "    vec3 shadowOff=shadow_n*(0.5+slopeBias*1.5);\n"
        "    vec4 lsFrag=uLightVP*vec4(vPos+shadowOff,1.0);\n"
        "    vec3 projC=lsFrag.xyz/lsFrag.w*0.5+0.5;\n"
        "    vec2 pd[12]=vec2[12](\n"
        "        vec2(-0.326,-0.406),vec2(-0.840,-0.074),\n"
        "        vec2(-0.696, 0.457),vec2(-0.203, 0.621),\n"
        "        vec2( 0.962,-0.195),vec2( 0.473,-0.480),\n"
        "        vec2( 0.519, 0.767),vec2( 0.185,-0.893),\n"
        "        vec2( 0.507, 0.064),vec2( 0.896, 0.412),\n"
        "        vec2(-0.322,-0.933),vec2(-0.792,-0.598));\n"
        "    if(projC.z<=1.0&&projC.x>0.02&&projC.x<0.98&&projC.y>0.02&&projC.y<0.98){\n"
        "        vec2 ts=1.0/vec2(textureSize(uShadowMap,0));\n"
        "        float pcf=0.0; float bias=0.0001;\n"
        "        for(int i=0;i<12;i++){\n"
        "            float sd=texture(uShadowMap,projC.xy+pd[i]*ts*2.5).r;\n"
        "            pcf+=(projC.z-bias>sd)?1.0:0.0;}\n"
        "        shadow=clamp(pcf/12.0,0.0,0.65)*sunFade;\n"
        "    } else {\n"
        "        vec3 shadowOffW=shadow_n*(1.5+slopeBias*2.5);\n"
        "        vec4 lsW=uLightVPWide*vec4(vPos+shadowOffW,1.0);\n"
        "        vec3 pW=lsW.xyz/lsW.w*0.5+0.5;\n"
        "        if(pW.z<=1.0&&pW.x>0.0&&pW.x<1.0&&pW.y>0.0&&pW.y<1.0){\n"
        "            vec2 tw=1.0/vec2(textureSize(uShadowMapWide,0));\n"
        "            float pcfw=0.0; float biasW=0.0002;\n"
        "            for(int i=0;i<12;i++){\n"
        "                float sd=texture(uShadowMapWide,pW.xy+pd[i]*tw*3.0).r;\n"
        "                pcfw+=(pW.z-biasW>sd)?1.0:0.0;}\n"
        "            shadow=clamp(pcfw/12.0,0.0,0.55)*sunFade*0.7;\n"
        "        }\n"
        "    }\n"
        "    }\n"
        "    shadow*=terrainShadowFade;\n"
        "    float diff=max(dot(n,-uSunDir),0.0)*(1.0-shadow);\n"
        "    vec3 col=base*(uAmbient+uSunColor*diff);\n"
        "    for(int i=0;i<uNumLights;i++){\n"
        "        vec3 lv=uLightPos[i]-vPos;float d=length(lv);\n"
        "        float att=1.0-clamp(d/uLightRadius[i],0,1);att*=att;\n"
        "        col+=base*uLightColor[i]*max(dot(n,normalize(lv)),0.0)*att;\n"
        "    }\n"
        "    col+=c_lava*lava_mask*(0.5+pulse*0.8);\n"

        /* ── wet specular — roughness-gated ── */
        "    if(uWetness>0.01){\n"
        "        float gloss=clamp((1.0-roughness)*(1.0+uWetness),0.0,1.0);\n"
        "        vec3 view=normalize(uCamPos-vPos);\n"
        "        vec3 refl=reflect(uSunDir,n);\n"
        "        float spec=pow(max(dot(refl,view),0.0),mix(8.0,64.0,gloss));\n"
        "        col+=vec3(0.7)*spec*uWetness*wet_flat*gloss;\n"
        "    }\n"

        /* ── fog ── */
        "    float fogDist=length(vPos-uCamPos);\n"
        "    float fogFac=exp(-uFogDensity*fogDist);\n"
        "    col=mix(uFogColor,col,clamp(fogFac,0.0,1.0));\n"
        "    FragColor=vec4(col,1.0);\n"
        "}\n");
    t->water_prog=GLH_program(
        /* Vertex */
        "#version 330 core\n"
        "layout(location=0)in vec3 aPos;\nlayout(location=1)in vec3 aNormal;\nlayout(location=2)in vec3 aColor;\n"
        "uniform mat4 uProj,uView;\n"
        "out vec3 vPos;\nout vec2 vOrigXZ;\n"
        "void main(){\n"
        "    gl_Position=uProj*uView*vec4(aPos,1.0);\n"
        "    vPos=aPos;vOrigXZ=aPos.xz;}\n",
        /* Fragment — zone-aware parameterised water */
        "#version 330 core\nin vec3 vPos;\nin vec2 vOrigXZ;\n"
        LIGHTING_UNIFORMS
        "uniform vec3  uCamPos,uFogColor;\n"
        "uniform float uFogDensity,uTime;\n"
        "uniform sampler2D uReflectTex;\n"
        "uniform sampler2D uTerrainHMap;\n"
        "uniform sampler2D uDetailTex;\n"
        "uniform vec2  uScreenSize;\n"
        "uniform vec2  uWorldMin,uWorldSize;\n"
        "uniform bool  uHasReflect;\n"
        "uniform bool  uHasDetailTex;\n"
        /* global style (fallback outside all zones) */
        "uniform vec3  uWaterColor;\n"
        "uniform float uWaveAmp,uWaveSpeed,uOpacity,uSpecPow,uSpecStr,uIce,uDepthFade;\n"
        /* zone array — up to 8 zones */
        "uniform int   uZoneCount;\n"
        "uniform vec2  uZonePos[8];\n"
        "uniform float uZoneRadius[8];\n"
        "uniform vec3  uZoneColor[8];\n"
        "uniform float uZoneWaveAmp[8],uZoneWaveSpeed[8],uZoneOpacity[8];\n"
        "uniform float uZoneSpecPow[8],uZoneSpecStr[8],uZoneIce[8],uZoneDepthFade[8];\n"
        "out vec4 FragColor;\n"
        "void main(){\n"
        /* ── find dominant zone ── */
        "    vec3  wc  =uWaterColor;\n"
        "    float wa  =uWaveAmp,   ws=uWaveSpeed;\n"
        "    float op  =uOpacity,   sp=uSpecPow,   ss=uSpecStr;\n"
        "    float ice =uIce,       df=uDepthFade;\n"
        "    float best=0.0;\n"
        "    for(int zi=0;zi<uZoneCount;zi++){\n"
        "        float d=length(vOrigXZ-uZonePos[zi]);\n"
        "        float r=uZoneRadius[zi];\n"
        "        if(d>=r)continue;\n"
        /* smooth blend: full inside 90% of radius, fade over last 10% */
        "        float t=clamp(1.0-(d/r-0.9)*10.0,0.0,1.0);\n"
        "        if(t<=best)continue;\n"
        "        best=t;\n"
        "        wc =mix(uWaterColor,  uZoneColor[zi],   t);\n"
        "        wa =mix(uWaveAmp,     uZoneWaveAmp[zi], t);\n"
        "        ws =mix(uWaveSpeed,   uZoneWaveSpeed[zi],t);\n"
        "        op =mix(uOpacity,     uZoneOpacity[zi], t);\n"
        "        sp =mix(uSpecPow,     uZoneSpecPow[zi], t);\n"
        "        ss =mix(uSpecStr,     uZoneSpecStr[zi], t);\n"
        "        ice=mix(uIce,         uZoneIce[zi],     t);\n"
        "        df =mix(uDepthFade,   uZoneDepthFade[zi],t);\n"
        "    }\n"
        "    vec2 huv=clamp((vOrigXZ-uWorldMin)/uWorldSize,vec2(0.0),vec2(1.0));\n"
        "    float terrainH=texture(uTerrainHMap,huv).r;\n"
        "    float waterDepth=max(vPos.y-terrainH+0.12,0.0);\n"
        "    float shoreFade=1.0;\n"
        /* ── wave normals ── */
        "    float nx=sin(vOrigXZ.x*0.12+uTime*0.45*ws)*wa*0.08\n"
        "            +sin(vOrigXZ.y*0.10-uTime*0.28*ws)*wa*0.05;\n"
        "    float nz=cos(vOrigXZ.x*0.09-uTime*0.34*ws)*wa*0.07\n"
        "            +cos(vOrigXZ.y*0.13+uTime*0.41*ws)*wa*0.04;\n"
        "    vec3 n=normalize(vec3(nx,1.0,nz));\n"
        /* ── ice crack voronoi ── */
        "    float crack=0.0;\n"
        "    if(ice>0.01){\n"
        "        vec2 cell=floor(vOrigXZ*0.08);\n"
        "        vec2 f   =fract(vOrigXZ*0.08);\n"
        "        float md=1.0;\n"
        "        for(int cy=-1;cy<=1;cy++)for(int cx=-1;cx<=1;cx++){\n"
        "            vec2 nb=vec2(float(cx),float(cy));\n"
        "            vec2 h2=fract(sin(vec2(dot(cell+nb,vec2(127.1,311.7)),dot(cell+nb,vec2(269.5,183.3))))*43758.5)*0.5+0.25;\n"
        "            md=min(md,length(f-nb-h2));\n"
        "        }\n"
        "        crack=clamp(1.0-md*6.0,0.0,1.0);\n"
        "    }\n"
        "    vec3 shallowTint=wc*1.18+vec3(0.03,0.05,0.02);\n"
        "    vec3 base=mix(shallowTint,wc,clamp(waterDepth*2.0,0.0,1.0));\n"
        "    float detail=0.5+0.5*sin(vOrigXZ.x*0.045+uTime*0.12*ws)*sin(vOrigXZ.y*0.038-uTime*0.10*ws);\n"
        "    if(uHasDetailTex){\n"
        "        vec2 duv0=vOrigXZ*0.0065+vec2(uTime*0.010*ws,-uTime*0.008*ws);\n"
        "        vec2 duv1=vOrigXZ.yx*0.0040+vec2(-uTime*0.006*ws,uTime*0.007*ws);\n"
        "        float td0=dot(texture(uDetailTex,duv0).rgb,vec3(0.3333));\n"
        "        float td1=dot(texture(uDetailTex,duv1).rgb,vec3(0.3333));\n"
        "        detail=mix(detail,td0*0.65+td1*0.35,0.65);\n"
        "    }\n"
        "    base*=mix(0.97,1.03,detail);\n"
        "    base=mix(base, wc*1.6+vec3(0.05), crack*ice);\n"
        "    vec3 shadow_n=n;\n"
        LIGHTING_CALC
        "    vec3 view=normalize(uCamPos-vPos);\n"
        "    float fresnel=pow(1.0-max(dot(view,n),0.0),2.5);\n"
        "    col=mix(col, wc*1.10+uFogColor*0.08, fresnel*0.22);\n"
        "    vec3 refl=reflect(-normalize(uSunDir),n);\n"
        "    col+=vec3(0.55,0.58,0.62)*pow(max(dot(refl,view),0.0),max(sp*0.18,10.0))*ss*0.22;\n"
        "    float distFade=clamp(length(vPos-uCamPos)*0.006*df,0.0,1.0);\n"
        "    float alpha=max(op*mix(0.94,1.0,distFade),0.92);\n"
        "    float fogFac=exp(-uFogDensity*length(vPos-uCamPos)*0.10);\n"
        "    vec3 foggedWater=mix(wc*0.85 + uFogColor*0.15,col,clamp(fogFac,0.0,1.0));\n"
        "    col=foggedWater;\n"
        "    FragColor=vec4(col,alpha);}\n");
    t->tex[0]=terrain_tex_load_srgb("ground/terrain_sand.png",  0.76f,0.70f,0.50f);
    t->tex[1]=terrain_tex_load_srgb("ground/terrain_grass.png", 0.20f,0.50f,0.14f);
    t->tex[2]=terrain_tex_load_srgb("ground/terrain_dirt.png",  0.38f,0.28f,0.18f);
    t->tex[3]=terrain_tex_load_srgb("ground/terrain_rock.png",  0.50f,0.46f,0.40f);
    t->tex[4]=terrain_tex_load_srgb("ground/terrain_snow.png",  0.90f,0.93f,0.98f);
    t->tex[5]=terrain_tex_load_srgb("ground/terrain_lava.png",  0.80f,0.20f,0.02f);

    /* Normal maps — flat blue (0.5,0.5,1) fallback */
    bool nrm_ok[5];
    t->nrm[0]=terrain_tex_load2("ground/terrain_sand_nor.png",  0.50f,0.50f,1.00f,&nrm_ok[0]);
    t->nrm[1]=terrain_tex_load2("ground/terrain_grass_nor.png", 0.50f,0.50f,1.00f,&nrm_ok[1]);
    t->nrm[2]=terrain_tex_load2("ground/terrain_dirt_nor.png",  0.50f,0.50f,1.00f,&nrm_ok[2]);
    t->nrm[3]=terrain_tex_load2("ground/terrain_rock_nor.png",  0.50f,0.50f,1.00f,&nrm_ok[3]);
    t->nrm[4]=terrain_tex_load2("ground/terrain_snow_nor.png",  0.50f,0.50f,1.00f,&nrm_ok[4]);
    t->has_nrm=nrm_ok[1]; /* enable as long as grass normal exists; others use flat fallback */

    /* Roughness/AO maps — ARM format: R=AO G=Roughness B=Metallic; fallback rough=0.85 ao=1.0 */
    bool rgh_ok[5];
    t->rgh[0]=terrain_tex_load2("ground/terrain_sand_rgh.png",  0.85f,1.00f,0.00f,&rgh_ok[0]);
    t->rgh[1]=terrain_tex_load2("ground/terrain_grass_rgh.png", 0.80f,1.00f,0.00f,&rgh_ok[1]);
    t->rgh[2]=terrain_tex_load2("ground/terrain_dirt_rgh.png",  0.90f,1.00f,0.00f,&rgh_ok[2]);
    t->rgh[3]=terrain_tex_load2("ground/terrain_rock_rgh.png",  0.75f,1.00f,0.00f,&rgh_ok[3]);
    t->rgh[4]=terrain_tex_load2("ground/terrain_snow_rgh.png",  0.60f,1.00f,0.00f,&rgh_ok[4]);
    t->has_rgh=rgh_ok[1]; /* enable as long as grass roughness exists */
    t->water_detail_tex=terrain_tex_load2("terrain_water.png",0.50f,0.50f,0.50f,&t->has_water_detail);
    memset(g_worn_mask.d,0,sizeof(g_worn_mask.d));
    worn_mask_upload(t);

    printf("[Terrain] PBR normal maps: %s\n", t->has_nrm?"ALL LOADED":"using fallbacks");
    printf("[Terrain] PBR roughness maps: %s\n",t->has_rgh?"ALL LOADED":"using fallbacks");
    printf("[Water] detail texture: %s\n",t->has_water_detail?"loaded":"fallback");

    /* ── Bake 512×512 R32F heightmap for water occlusion ───────
       Each texel stores terrain height with a very small safety bias.
       Using an aggressive max-filter here over-culls large parts of the
       water plane, making water appear only in deep pockets.      */
    terrain_rebake_hmap(t);
    printf("[Terrain] %dx%d chunks built\n",TCNX,TCNZ);
}

/* Rebake the R32F heightmap texture — call after any terrain_h change
   (custom river edits, etc.) so the water discard shader stays correct. */
#define HMAP_SZ 512
static void terrain_rebake_hmap(Terrain *t){
    float *hmap = malloc(HMAP_SZ*HMAP_SZ*sizeof(float));
    if(!hmap)return;
    float cell = 5000.0f/(float)HMAP_SZ;
    for(int zi=0;zi<HMAP_SZ;zi++){
        for(int xi=0;xi<HMAP_SZ;xi++){
            float wx=-2500.0f+(xi+0.5f)*cell;
            float wz=-2500.0f+(zi+0.5f)*cell;
            float h0=terrain_h(wx,wz);
            hmap[zi*HMAP_SZ+xi]=h0+0.03f;
        }
    }
    if(!t->hmap_tex) glGenTextures(1,&t->hmap_tex);
    glBindTexture(GL_TEXTURE_2D,t->hmap_tex);
    /* glSubImage if already allocated, else full upload */
    glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,HMAP_SZ,HMAP_SZ,0,GL_RED,GL_FLOAT,hmap);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    free(hmap);
    printf("[Terrain] Heightmap rebaked\n");
}



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

/* Projectile draw — always rendered (arrows visible as directional lines) */
static void proj_draw(ProjList *pl,LineRenderer *lr,Mat4 proj,Mat4 view){
    for(int i=0;i<pl->count;i++){
        Projectile *p=&pl->p[i];if(!p->active)continue;
        if(p->type==PROJ_ARROW){
            float spd=sqrtf(p->vel.x*p->vel.x+p->vel.y*p->vel.y+p->vel.z*p->vel.z);
            if(spd>0.001f){
                float len=0.55f;
                Vec3 d={(p->vel.x/spd)*len,(p->vel.y/spd)*len,(p->vel.z/spd)*len};
                Vec3 tail={p->pos.x-d.x,p->pos.y-d.y,p->pos.z-d.z};
                Vec3 tip ={p->pos.x+d.x*0.3f,p->pos.y+d.y*0.3f,p->pos.z+d.z*0.3f};
                lr_push(tail,tip, 0.60f,0.42f,0.18f);
                lr_push(tip,p->pos, 0.80f,0.70f,0.50f);
            }
        } else {
            AABB b;b.min=(Vec3){p->pos.x-p->radius,p->pos.y-p->radius,p->pos.z-p->radius};
            b.max=(Vec3){p->pos.x+p->radius,p->pos.y+p->radius,p->pos.z+p->radius};
            lr_box(&b,1.0f,0.4f,0.0f);
        }
    }
    lr_flush(lr,proj,view);
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
    case PS_SWIM:   return "SWIM";
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
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);          /* ensure depth mask never blocks HUD draw */
    glDisable(GL_POLYGON_OFFSET_FILL);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES,0,h->vcount);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    /* do NOT restore cull face here — game runs with it off by default */
    glBindVertexArray(0);
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
    const float WR=600.0f; /* world units shown – ~1.2km view radius */
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

/* ── Full-screen World Map (M key) — Parchment Style ─────────── */

/* ── HUD for waterfall editor sub-mode ──────────────────────── */
static void wf_editor_hud(HUD *h, WaterfallSystem *wfs, int sel,
                           int win_w, int win_h){
    float W=(float)win_w;
    float pw=320, lh=20, pad=8;
    float px=W-pw-6, py=8;
    int rows=6+(sel>=0?3:0);
    hud_rect(h,px-4,py-4,pw+8,(float)rows*lh+pad*2+8, 0,0,0,0.75f);
    hud_str(h,px,py,1.4f,"[F2 » WATERFALL EDITOR]",0.6f,0.85f,1.0f,1.0f); py+=lh+4;
    hud_str(h,px,py,1.2f,"LMB=Place  RMB=Delete",0.8f,0.8f,0.6f,1.0f); py+=lh;
    hud_str(h,px,py,1.2f,"Wheel=Height  Shift+Wheel=Width",0.8f,0.8f,0.6f,1.0f); py+=lh;
    hud_str(h,px,py,1.2f,"R/F=Rotate  S=Save  Tab=Zones",0.8f,0.8f,0.6f,1.0f); py+=lh;
    char cbuf[64]; snprintf(cbuf,sizeof(cbuf),"Waterfalls: %d / %d",
                             wfs->count,MAX_WATERFALLS);
    hud_str(h,px,py,1.2f,cbuf,0.7f,0.9f,0.7f,1.0f); py+=lh;
    if(sel>=0&&sel<wfs->count){
        Waterfall *w=&wfs->wf[sel];
        py+=4;
        char wb[64]; snprintf(wb,sizeof(wb),"W:%.1f  H:%.1f  Yaw:%.0f°",
                               (double)w->width,(double)w->height,
                               (double)(w->yaw*180.0f/3.14159f));
        hud_str(h,px,py,1.2f,wb,1.0f,1.0f,0.5f,1.0f); py+=lh;
        char pb[64]; snprintf(pb,sizeof(pb),"Pos:(%.0f,%.0f,%.0f)",
                               (double)w->pos.x,(double)w->pos.y,(double)w->pos.z);
        hud_str(h,px,py,1.2f,pb,0.7f,0.7f,0.9f,1.0f); py+=lh;
        char ib[32]; snprintf(ib,sizeof(ib),"[Selected: %d]",sel);
        hud_str(h,px,py,1.2f,ib,0.4f,1.0f,0.4f,1.0f);
    }
    hud_flush(h);
}

/* ── Draw preview marker for selected waterfall ─────────────── */
static void wf_editor_draw(WaterfallSystem *wfs, LineRenderer *lr,
                            Mat4 proj, Mat4 view, int sel){
    for(int i=0;i<wfs->count;i++){
        Waterfall *w=&wfs->wf[i];
        if(!w->active)continue;
        float cy=cosf(w->yaw),sy=sinf(w->yaw);
        float hw=w->width*0.5f;
        /* Top edge */
        /* Left/right are perpendicular to facing direction.
           Facing = (sin(yaw), 0, cos(yaw)).  Right = (cos(yaw), 0, -sin(yaw)) */
        Vec3 right={cy,0,-sy};
        Vec3 TL={w->pos.x-right.x*hw, w->pos.y,              w->pos.z-right.z*hw};
        Vec3 TR={w->pos.x+right.x*hw, w->pos.y,              w->pos.z+right.z*hw};
        Vec3 BL={w->pos.x-right.x*hw, w->pos.y-w->height,    w->pos.z-right.z*hw};
        Vec3 BR={w->pos.x+right.x*hw, w->pos.y-w->height,    w->pos.z+right.z*hw};
        float r=i==sel?1.0f:0.4f, g=i==sel?1.0f:0.7f, b=i==sel?0.4f:1.0f;
        lr_push(TL,TR,r,g,b); lr_push(BL,BR,r,g,b);
        lr_push(TL,BL,r,g,b); lr_push(TR,BR,r,g,b);
        /* Centre top marker */
        Vec3 up={w->pos.x,w->pos.y+1.5f,w->pos.z};
        lr_push(w->pos,up,r,g,b);
    }
    lr_flush(lr,proj,view);
}

/* Forward declarations — NpcDef/NpcSystem defined later */

typedef struct{char msg[64];float timer;}SaveNotify;
static void save_notify(SaveNotify *n,const char *msg); /* defined in save section */
#define NPC_MAX          16
#define NPC_LINES_MAX     8
#define NPC_LINE_LEN     80
#define NPC_INTERACT_R   3.5f   /* interact radius */

#define NPC_MODEL_MAX 8   /* distinct NPC models */
typedef struct NpcDef_s{
    Vec3       pos;
    float      yaw;
    char       name[32];
    char       lines[NPC_LINES_MAX][NPC_LINE_LEN];
    int        line_count;
    double     anim_time;
    int        anim_idx;
    /* Quest offer */
    int        quest_id;
    bool       quest_offered;
    /* Merchant */
    bool       is_merchant;
    int        shop_id;
    /* Which model to render (index into g_npc_models[]) */
    int        model_idx;
    /* Branching dialogue tree root (-1 = use flat lines) */
    int        dlg_root_id;
    int        dlg_cur_node;  /* active node during conversation */
}NpcDef;

typedef enum{NPC_STATE_CHAT,NPC_STATE_QUEST_OFFER,NPC_STATE_SHOP}NpcTalkState;

typedef struct NpcSystem_s{
    NpcDef  npcs[NPC_MAX];
    int     count;
    int     active_npc;
    int     active_line;
    bool    open;
    NpcTalkState talk_state; /* chat or quest accept/decline screen */
}NpcSystem;

typedef struct{ bool open; float scroll_x,scroll_y,zoom; }MapScreen;

/* Convert a terrain RGB into sepia ink tones for parchment look */
static Vec3 sepia_terrain(Vec3 tc, float h){
    /* Blend original color toward warm sepia palette */
    float lum=tc.x*0.299f+tc.y*0.587f+tc.z*0.114f;
    /* Sepia base: warm ochre-brown range */
    float sr=clampf(lum*0.98f+0.10f,0,1);
    float sg=clampf(lum*0.82f+0.06f,0,1);
    float sb=clampf(lum*0.55f+0.02f,0,1);
    /* Blend: low areas more sepia, high peaks keep a little cool */
    float t=clampf(h/30.0f,0,1); /* 0=flat=full sepia, 1=peaks=mix in original */
    return (Vec3){sr*(1-t)+tc.x*t*0.7f, sg*(1-t)+tc.y*t*0.7f, sb*(1-t)+tc.z*t*0.6f};
}

/* Draw a compass rose centred at (cx,cy) with radius r */
static void map_compass(HUD *h,float cx,float cy,float r){
    /* N/S/E/W arms */
    hud_rect(h,cx-r*0.10f,cy-r,r*0.20f,r*2.0f, 0.15f,0.10f,0.06f,0.85f);
    hud_rect(h,cx-r,cy-r*0.10f,r*2.0f,r*0.20f, 0.15f,0.10f,0.06f,0.85f);
    /* Diagonal arms (thinner) */
    float d=r*0.65f;
    float t=r*0.10f;
    hud_rect(h,cx-d-t,cy-d-t,d*2+t*2,t*2, 0.20f,0.14f,0.08f,0.6f);
    hud_rect(h,cx-t,cy-d-t,t*2,d*2+t*2, 0.20f,0.14f,0.08f,0.6f);
    /* Centre jewel */
    hud_rect(h,cx-r*0.14f,cy-r*0.14f,r*0.28f,r*0.28f, 0.55f,0.30f,0.10f,1.0f);
    hud_rect(h,cx-r*0.07f,cy-r*0.07f,r*0.14f,r*0.14f, 0.85f,0.65f,0.20f,1.0f);
    /* Cardinal labels */
    float lsc=1.3f;
    hud_str(h,cx-4,cy-r-14,lsc,"N", 0.15f,0.10f,0.06f,1.0f);
    hud_str(h,cx-4,cy+r+2, lsc,"S", 0.15f,0.10f,0.06f,0.9f);
    hud_str(h,cx+r+3,cy-4, lsc,"E", 0.15f,0.10f,0.06f,0.9f);
    hud_str(h,cx-r-9,cy-4, lsc,"W", 0.15f,0.10f,0.06f,0.9f);
}

/* Draw a town icon (cross + outer diamond) at (tx,ty) with size s */
static void map_town_icon(HUD *h,float tx,float ty,float s,bool is_city,
                           float ir,float ig,float ib){
    float ink_r=0.20f,ink_g=0.13f,ink_b=0.06f;
    /* Outer diamond outline */
    hud_rect(h,tx-s,   ty-s*0.35f,s*2, s*0.70f, ink_r,ink_g,ink_b,0.75f);
    hud_rect(h,tx-s*0.35f,ty-s,   s*0.70f,s*2, ink_r,ink_g,ink_b,0.75f);
    /* Filled centre diamond */
    hud_rect(h,tx-s*0.5f,ty-s*0.5f,s,s, ir,ig,ib,1.0f);
    if(is_city){
        /* Extra ring for cities */
        hud_rect(h,tx-s*0.65f,ty-s*0.65f,s*1.3f,s*0.3f, ink_r,ink_g,ink_b,0.5f);
        hud_rect(h,tx-s*0.65f,ty+s*0.35f,s*1.3f,s*0.3f, ink_r,ink_g,ink_b,0.5f);
        hud_rect(h,tx-s*0.65f,ty-s*0.65f,s*0.3f,s*1.3f, ink_r,ink_g,ink_b,0.5f);
        hud_rect(h,tx+s*0.35f,ty-s*0.65f,s*0.3f,s*1.3f, ink_r,ink_g,ink_b,0.5f);
    }
}

/* Forward decls for functions called before definition */

static void map_draw(MapScreen *ms,HUD *h,int win_w,int win_h,
                     Vec3 player_pos,float player_yaw,
                     QuestSystem *qs,Enemy *enemies,int ne,
                     NpcSystem *npcs){
    if(!ms->open)return;
    float W=(float)win_w,H=(float)win_h;

    /* ── Full-screen parchment background ── */
    /* Base parchment */
    hud_rect(h,0,0,W,H, 0.75f,0.62f,0.40f,1.0f);
    /* Subtle grain — layered semi-transparent rects simulating paper texture */
    for(int i=0;i<12;i++){
        float gx=(float)((i*137+11)%((int)W));
        float gy=(float)((i*79 +37)%((int)H));
        float gw=W*0.3f+(float)((i*53)%((int)(W*0.4f)));
        float gh=H*0.04f+(float)((i*31)%((int)(H*0.08f)));
        hud_rect(h,gx,gy,gw,gh, 0.70f,0.58f,0.36f,0.12f);
    }
    /* Vignette — darker edges */
    hud_rect(h,0,0,W,H*0.06f, 0.35f,0.25f,0.12f,0.55f);
    hud_rect(h,0,H*0.94f,W,H*0.06f, 0.35f,0.25f,0.12f,0.55f);
    hud_rect(h,0,0,W*0.05f,H, 0.35f,0.25f,0.12f,0.55f);
    hud_rect(h,W*0.95f,0,W*0.05f,H, 0.35f,0.25f,0.12f,0.55f);
    /* Corner darkening */
    for(int c=0;c<4;c++){
        float cx=(c%2==0)?0:W-W*0.15f;
        float cy=(c<2)?0:H-H*0.15f;
        hud_rect(h,cx,cy,W*0.15f,H*0.15f, 0.30f,0.20f,0.10f,0.35f);
    }

    /* ── Map panel bounds ── */
    float pw=W*0.78f,ph=H*0.76f,px=(W-pw)*0.5f,py=H*0.13f;

    /* ── Ornate border ── */
    float bthk=6.0f;
    /* Outer frame */
    hud_rect(h,px-bthk*3,py-bthk*3,pw+bthk*6,bthk,       0.28f,0.18f,0.08f,1.0f);
    hud_rect(h,px-bthk*3,py+ph+bthk*2,pw+bthk*6,bthk,    0.28f,0.18f,0.08f,1.0f);
    hud_rect(h,px-bthk*3,py-bthk*3,bthk,ph+bthk*6,        0.28f,0.18f,0.08f,1.0f);
    hud_rect(h,px+pw+bthk*2,py-bthk*3,bthk,ph+bthk*6,     0.28f,0.18f,0.08f,1.0f);
    /* Inner frame (thinner) */
    hud_rect(h,px-bthk,py-bthk,pw+bthk*2,bthk*0.5f,       0.42f,0.28f,0.12f,0.9f);
    hud_rect(h,px-bthk,py+ph+bthk*0.5f,pw+bthk*2,bthk*0.5f,0.42f,0.28f,0.12f,0.9f);
    hud_rect(h,px-bthk,py-bthk,bthk*0.5f,ph+bthk*2,        0.42f,0.28f,0.12f,0.9f);
    hud_rect(h,px+pw+bthk*0.5f,py-bthk,bthk*0.5f,ph+bthk*2,0.42f,0.28f,0.12f,0.9f);
    /* Corner squares */
    float cs=16;
    for(int c=0;c<4;c++){
        float cx=(c%2==0)?px-bthk*3:px+pw+bthk*2;
        float cy=(c<2)?py-bthk*3:py+ph+bthk*2;
        hud_rect(h,cx-2,cy-2,cs+4,cs+4, 0.22f,0.14f,0.06f,1.0f);
        hud_rect(h,cx,cy,cs,cs,          0.58f,0.40f,0.18f,1.0f);
        hud_rect(h,cx+3,cy+3,cs-6,cs-6, 0.75f,0.55f,0.22f,1.0f);
    }

    /* ── Terrain grid — sepia-toned ── */
    int cells=80;
    float zoom=ms->zoom>0?ms->zoom:1.0f;
    float cell_w=pw/(float)cells, cell_h=ph/(float)cells;
    float world_span=WORLD_HALF*2.0f/zoom;
    float view_ox=player_pos.x+ms->scroll_x-world_span*0.5f;
    float view_oz=player_pos.z+ms->scroll_y-world_span*0.5f;

    for(int gz=0;gz<cells;gz++) for(int gx=0;gx<cells;gx++){
        float wx=view_ox+(gx+0.5f)*(world_span/cells);
        float wz=view_oz+(gz+0.5f)*(world_span/cells);
        float th=terrain_h(wx,wz);
        Vec3 tc=terrain_color(th);
        Vec3 sc=sepia_terrain(tc,th);
        /* Height shading — ridges slightly darker ink */
        float shade=clampf(0.70f+th*0.010f,0.65f,1.05f);
        hud_rect(h,px+gx*cell_w,py+gz*cell_h,cell_w+0.8f,cell_h+0.8f,
                 sc.x*shade,sc.y*shade,sc.z*shade,0.95f);
    }

    /* ── Faint cartographic grid lines ── */
    {
        int grid_divs=8;
        float gstep_w=pw/(float)grid_divs;
        float gstep_h=ph/(float)grid_divs;
        for(int i=1;i<grid_divs;i++){
            hud_rect(h,px+i*gstep_w,py,1,ph, 0.28f,0.18f,0.08f,0.15f);
            hud_rect(h,px,py+i*gstep_h,pw,1, 0.28f,0.18f,0.08f,0.15f);
        }
    }

    /* World→map pixel helpers */
    #define M_WX(wx) (px+((wx)-view_ox)/world_span*pw)
    #define M_WZ(wz) (py+((wz)-view_oz)/world_span*ph)
    #define M_CLAMP(v,lo,hi) ((v)<(lo)?(lo):(v)>(hi)?(hi):(v))
    #define M_VIS(x,z) ((x)>=px-12&&(x)<=px+pw+12&&(z)>=py-12&&(z)<=py+ph+12)

    /* ── Biome region labels (ink, italic-ish via offset shadow) ── */
    {
        struct{float wx,wz;const char*name;float r,g,b;}blbls[]={
            {-1600,-2200,"Lava Fields",     0.48f,0.15f,0.05f},
            { -800,-1600,"Mashito Peaks",   0.22f,0.25f,0.32f},
            {  200, -300,"Great Plains",    0.24f,0.34f,0.16f},
            { -600,  500,"Ancient Forest",  0.18f,0.28f,0.14f},
            { -400,-2100,"Snow Wastes",     0.30f,0.32f,0.38f},
        };
        for(int i=0;i<5;i++){
            float bx=M_WX(blbls[i].wx),bz=M_WZ(blbls[i].wz);
            if(!M_VIS(bx,bz))continue;
            float lsc=1.1f,ltw=lsc*8.0f*(float)strlen(blbls[i].name);
            /* Ink shadow */
            hud_str(h,bx-ltw*0.5f+1,bz+1,lsc,blbls[i].name,0.0f,0.0f,0.0f,0.25f);
            hud_str(h,bx-ltw*0.5f,bz,lsc,blbls[i].name,
                    blbls[i].r,blbls[i].g,blbls[i].b,0.70f);
        }
    }

    /* ── Village / city icons ── */
    {
        static const char *vnames[N_VILLAGES]={
            "Brandalhall","Callicut","Rustpatch","Ulasse","Baldurne",
            "Gorshett Keep","Valdoa","Ishome","Seldonne","Estuay",
            "Kerone City","Etarma","Fallaway","Sclay","Bellweather"
        };
        /* City size: large radius > 40 = city */
        for(int i=0;i<N_VILLAGES;i++){
            float vx=M_WX(VILLAGES[i][0]),vz=M_WZ(VILLAGES[i][1]);
            if(!M_VIS(vx,vz))continue;
            bool is_city=(VILLAGES[i][2]>=40);
            float s=is_city?7.0f:5.0f;
            /* Ink colour: dark sepia */
            map_town_icon(h,vx,vz,s,is_city, 0.65f,0.42f,0.12f);
            /* Name — ink with shadow */
            float ny=(i%2==0)?vz-s-11:vz+s+3;
            float nsc=0.95f,ntw=nsc*8.0f*(float)strlen(vnames[i]);
            float nx2=M_CLAMP(vx-ntw*0.5f,px,px+pw-ntw);
            float nz2=M_CLAMP(ny,py,py+ph-11);
            hud_str(h,nx2+1,nz2+1,nsc,vnames[i], 0.0f,0.0f,0.0f,0.35f); /* shadow */
            hud_str(h,nx2,nz2,nsc,vnames[i],      0.22f,0.14f,0.06f,0.95f);
        }
    }

    /* ── Merchant / NPC markers ── */
    if(npcs){
        for(int i=0;i<npcs->count;i++){
            NpcDef *n=&npcs->npcs[i];
            float nx=M_WX(n->pos.x),nz=M_WZ(n->pos.z);
            if(!M_VIS(nx,nz))continue;
            if(n->is_merchant){
                /* Gold coin icon */
                hud_rect(h,nx-6,nz-6,12,12, 0.18f,0.12f,0.05f,0.85f);
                hud_rect(h,nx-4,nz-4,8,8,   0.80f,0.60f,0.10f,1.0f);
                hud_str(h,nx-2,nz-5,1.0f,"$", 0.18f,0.12f,0.04f,1.0f);
            } else {
                hud_rect(h,nx-5,nz-5,10,10, 0.18f,0.12f,0.05f,0.8f);
                hud_rect(h,nx-3,nz-3,6,6,   0.30f,0.55f,0.20f,0.9f);
            }
            float nsc2=0.85f,ntw2=nsc2*8.0f*(float)strlen(n->name);
            hud_str(h,nx-ntw2*0.5f+1,nz+7+1,nsc2,n->name, 0.0f,0.0f,0.0f,0.25f);
            hud_str(h,nx-ntw2*0.5f,nz+7,    nsc2,n->name, 0.22f,0.38f,0.14f,0.85f);
        }
    }

    /* ── Quest objective markers (crossed quill icon) ── */
    for(int i=0;i<qs->count;i++){
        Quest *q=&qs->quests[i]; if(q->status!=Q_ACTIVE)continue;
        for(int j=0;j<q->obj_count;j++){
            Objective *o=&q->objs[j]; if(o->type!=OBJ_REACH||o->done)continue;
            float qx=M_WX(o->loc.x),qz=M_WZ(o->loc.z);
            if(!M_VIS(qx,qz))continue;
            /* Diamond star in ink gold */
            hud_rect(h,qx-7,qz-2,14,4,  0.55f,0.40f,0.08f,0.95f);
            hud_rect(h,qx-2,qz-7,4,14,  0.55f,0.40f,0.08f,0.95f);
            hud_rect(h,qx-4,qz-4,3,3,   0.75f,0.60f,0.12f,1.0f);
            hud_rect(h,qx+2,qz-4,3,3,   0.75f,0.60f,0.12f,1.0f);
            hud_rect(h,qx-4,qz+2,3,3,   0.75f,0.60f,0.12f,1.0f);
            hud_rect(h,qx+2,qz+2,3,3,   0.75f,0.60f,0.12f,1.0f);
        }
    }

    /* ── Enemy blips (small red X) ── */
    for(int i=0;i<ne;i++){
        Enemy *e=&enemies[i]; if(!e->active||e->state==ES_DEAD)continue;
        float ex=M_WX(e->phys.pos.x),ez=M_WZ(e->phys.pos.z);
        if(!M_VIS(ex,ez))continue;
        hud_rect(h,ex-4,ez-1,8,2, 0.55f,0.10f,0.06f,0.80f);
        hud_rect(h,ex-1,ez-4,2,8, 0.55f,0.10f,0.06f,0.80f);
    }

    /* ── Player position — ink blot + arrow ── */
    {
        float plx=M_WX(player_pos.x),plz=M_WZ(player_pos.z);
        /* Ink blot shadow */
        hud_rect(h,plx-7,plz-7,14,14, 0.0f,0.0f,0.0f,0.30f);
        /* Outer ring */
        hud_rect(h,plx-7,plz-7,14,14, 0.18f,0.12f,0.05f,0.85f);
        /* Inner fill — deep blue ink */
        hud_rect(h,plx-4,plz-4,8,8,  0.10f,0.20f,0.55f,1.0f);
        /* Direction arrow */
        float ax=plx-sinf(player_yaw)*11,az=plz+cosf(player_yaw)*11;
        hud_rect(h,ax-3,az-3,6,6, 0.18f,0.12f,0.05f,0.9f);
        hud_rect(h,ax-2,az-2,4,4, 0.55f,0.65f,1.0f,0.95f);
    }

    #undef M_WX
    #undef M_WZ
    #undef M_CLAMP
    #undef M_VIS

    /* ── Compass rose — bottom right of map ── */
    map_compass(h, px+pw-38, py+ph-38, 22);

    /* ── Title scroll banner ── */
    {
        float tw2=420,th2=38,tx2=(W-tw2)*0.5f,ty2=py-55;
        /* Banner background */
        hud_rect(h,tx2-8,ty2-4,tw2+16,th2+8, 0.30f,0.18f,0.07f,0.85f);
        hud_rect(h,tx2,  ty2,  tw2,   th2,    0.72f,0.56f,0.32f,0.95f);
        /* Decorative side tabs */
        hud_rect(h,tx2-22,ty2+6,22,th2-12, 0.30f,0.18f,0.07f,0.80f);
        hud_rect(h,tx2+tw2,ty2+6,22,th2-12,0.30f,0.18f,0.07f,0.80f);
        /* Title text */
        const char *ttl="World Map — The Realm of Mashito";
        float tsc=1.8f,ttw=tsc*8.0f*(float)strlen(ttl);
        hud_str(h,tx2+(tw2-ttw)*0.5f+1,ty2+8+1,tsc,ttl, 0.0f,0.0f,0.0f,0.30f);
        hud_str(h,tx2+(tw2-ttw)*0.5f,  ty2+8,  tsc,ttl, 0.22f,0.12f,0.05f,1.0f);
        /* Zoom indicator */
        char zbuf[24];snprintf(zbuf,sizeof(zbuf),"x%.1f",zoom);
        hud_str(h,px+pw-35,py-22,1.2f,zbuf, 0.30f,0.18f,0.07f,0.8f);
    }

    /* ── Legend — small parchment scroll inset ── */
    {
        float lx=px,ly=py+ph+18;
        float lw=520,lh=30;
        hud_rect(h,lx-4,ly-4,lw+8,lh+8, 0.30f,0.18f,0.07f,0.75f);
        hud_rect(h,lx,ly,lw,lh,           0.72f,0.58f,0.36f,0.92f);
        float ls2=1.1f,iy=ly+5;
        /* Town icon sample */
        map_town_icon(h,lx+8,iy+8,4,false, 0.65f,0.42f,0.12f);
        hud_str(h,lx+18,iy,ls2,"Town", 0.22f,0.14f,0.06f,0.9f);
        /* City icon sample */
        map_town_icon(h,lx+80,iy+8,5,true, 0.65f,0.42f,0.12f);
        hud_str(h,lx+92,iy,ls2,"City", 0.22f,0.14f,0.06f,0.9f);
        /* Shop */
        hud_rect(h,lx+150,iy+2,10,10, 0.80f,0.60f,0.10f,1.0f);
        hud_str(h,lx+150,iy+2,1.0f,"$", 0.18f,0.12f,0.04f,1.0f);
        hud_str(h,lx+164,iy,ls2,"Shop", 0.22f,0.14f,0.06f,0.9f);
        /* NPC */
        hud_rect(h,lx+224,iy+2,8,8, 0.30f,0.55f,0.20f,0.9f);
        hud_str(h,lx+236,iy,ls2,"NPC", 0.22f,0.14f,0.06f,0.9f);
        /* Quest */
        hud_rect(h,lx+282,iy+3,8,2, 0.55f,0.40f,0.08f,0.95f);
        hud_rect(h,lx+285,iy,2,8,   0.55f,0.40f,0.08f,0.95f);
        hud_str(h,lx+295,iy,ls2,"Quest", 0.22f,0.14f,0.06f,0.9f);
        /* Player */
        hud_rect(h,lx+358,iy+2,8,8, 0.10f,0.20f,0.55f,1.0f);
        hud_str(h,lx+370,iy,ls2,"You", 0.22f,0.14f,0.06f,0.9f);
        /* Controls hint */
        hud_str(h,W-320,ly+7,ls2,"Drag: pan   +/- Scroll: zoom   M/ESC: close",
                0.32f,0.20f,0.10f,0.65f);
    }

    hud_flush(h);
}
/* forward decls — LevelSystem defined after Inventory */
typedef struct{float flash,banner,dmg_bonus,hp_bonus,mp_bonus,regen_bonus;}LevelSystem;
static void level_draw(LevelSystem *lvl,QuestSystem *qs,HUD *h,float bx,float by,float W,float H);

static void hud_draw(HUD *h,float hp,float mhp,float mp,float mmp,float sp,float msp,
                     AbilitySystem *as,QuestSystem *qs,Enemy *enemies,int ne,
                     Mat4 proj,Mat4 view,Player *player,Vec3 campfire_pos,
                     LevelSystem *lvl){
    float W=(float)h->win_w,H=(float)h->win_h;
    /* HP / MP / Stamina bars */
    float bx=20,by=H-100;
    hud_str(h,bx,by-10,1.0f,"HP",0.9f,0.3f,0.3f,0.9f);
    hud_bar(h,bx+22,by,   220,16,hp/mhp, 0.9f,0.15f,0.15f,0.10f,0.05f,0.05f);
    hud_str(h,bx,by+20,1.0f,"MP",0.3f,0.5f,0.9f,0.9f);
    hud_bar(h,bx+22,by+20,220,16,mp/mmp, 0.15f,0.4f,0.9f, 0.05f,0.10f,0.20f);
    hud_str(h,bx,by+40,1.0f,"SP",sp<5.0f?1.0f:0.3f,sp<5.0f?0.2f:0.9f,sp<5.0f?0.1f:0.3f,0.9f);
    {float sfrac=sp/msp;float sr=sfrac<0.3f?0.9f:0.15f,sg2=sfrac<0.3f?0.2f:0.8f,sb2=sfrac<0.3f?0.1f:0.2f;
    hud_bar(h,bx+22,by+40,220,16,sfrac,sr,sg2,sb2,0.05f,0.20f,0.05f);}
    /* XP bar + level */
    level_draw(lvl,qs,h,bx,by,W,H);
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

/* ── Prop editor debug helpers (need lr_*, HUD, Prop all defined) ── */
static void props_debug_draw(PropList *pl,int selected){
    for(int pi=0;pi<pl->count;pi++){
        Prop *p=&pl->list[pi];if(!p->active)continue;
        bool sel=(pi==selected);
        float r=sel?1.0f:0.2f,g=sel?1.0f:0.8f,b=sel?0.0f:0.2f;
        lr_axis(p->pos,sel?2.0f:0.8f);
        if(p->coll_type==PROP_COLL_AABB){
            lr_box(&p->aabb,r,g,b);
        } else if(p->coll_type==PROP_COLL_MESH){
            int lim=p->trimesh.count<512?p->trimesh.count:512;
            for(int ti=0;ti<lim;ti++){
                CTri *t=&p->trimesh.tris[ti];
                lr_push(t->a,t->b,r,g,b);lr_push(t->b,t->c,r,g,b);lr_push(t->c,t->a,r,g,b);
            }
        }
    }
}
static void props_editor_hud(HUD *h,PropList *pl,int sel,int win_w,int win_h){
    if(!pl->count){hud_str(h,10,(float)win_h-60,1.4f,"No props loaded",1,1,0,1);return;}
    Prop *p=&pl->list[sel];char buf[512];
    float y=(float)win_h-20;float lh=18;
    hud_rect(h,0,y-lh*8.0f-4,(float)win_w,lh*8.0f+8,0,0,0,0.65f);
    hud_str(h,8,y-=lh,1.3f,"[F3 PROP EDITOR]  TAB=next  R/V=RotY  T/B=RotX  Y/N=RotZ  =/- Scale  Arrows=XZ  PgUp/Dn=Y",1.0f,0.9f,0.2f,1);
    snprintf(buf,sizeof(buf),"Prop %d/%d: %s",sel+1,pl->count,p->path);
    hud_str(h,8,y-=lh,1.3f,buf,0.8f,1.0f,0.8f,1);
    snprintf(buf,sizeof(buf),"  pos  x=%.2f  y=%.2f  z=%.2f",(double)p->pos.x,(double)p->pos.y,(double)p->pos.z);
    hud_str(h,8,y-=lh,1.3f,buf,0.9f,0.9f,0.9f,1);
    snprintf(buf,sizeof(buf),"  rot  rx=%.1f  ry=%.1f  rz=%.1f",(double)p->rot_x,(double)p->rot_y,(double)p->rot_z);
    hud_str(h,8,y-=lh,1.3f,buf,0.9f,0.9f,0.9f,1);
    snprintf(buf,sizeof(buf),"  scale=%.3f",(double)p->scale);
    hud_str(h,8,y-=lh,1.3f,buf,0.9f,0.9f,0.9f,1);
    snprintf(buf,sizeof(buf),"  >> prop_place(&g_props,\"%s\", %.2f,%.2f,%.2f, %.1f,%.1f,%.1f, %.3f, PROP_COLL_AABB, 0,0,0,0);",
             p->path,(double)p->pos.x,(double)p->pos.y,(double)p->pos.z,
             (double)p->rot_x,(double)p->rot_y,(double)p->rot_z,(double)p->scale);
    hud_str(h,8,y-=lh,1.15f,buf,0.3f,1.0f,0.3f,1);
}

/* ── River Editor ───────────────────────────────────────────────
   F4 toggles mode. Left-click places a point, right-click removes
   the last one. S saves. Z undoes the last point. C clears all.
   Points are connected as a spline and immediately carved into
   the terrain (affected chunks rebuilt on GPU).                  */

/* Cast a ray from screen pixel (mx,my) into the world and find where
   it intersects the terrain by binary search along the ray.          */
static bool river_screen_to_world(int mx,int my,int win_w,int win_h,
                                  Mat4 proj,Mat4 view,Vec3 *out){
    /* NDC */
    float nx=(2.0f*mx/(float)win_w)-1.0f;
    float ny=1.0f-(2.0f*my/(float)win_h);

    /* Unproject two depths to get ray direction */
    /* Simpler: extract ray from view matrix directly */
    /* Ray origin = camera world position (from inverse view) */
    Mat4 iv; /* inverse view: just the transpose for rotation + negated translation */
    {
        float *m=view.m;
        float tx=-(m[0]*m[12]+m[1]*m[13]+m[2]*m[14]);
        float ty=-(m[4]*m[12]+m[5]*m[13]+m[6]*m[14]);
        float tz=-(m[8]*m[12]+m[9]*m[13]+m[10]*m[14]);
        iv.m[0]=m[0];iv.m[1]=m[4];iv.m[2]=m[8]; iv.m[3]=0;
        iv.m[4]=m[1];iv.m[5]=m[5];iv.m[6]=m[9]; iv.m[7]=0;
        iv.m[8]=m[2];iv.m[9]=m[6];iv.m[10]=m[10];iv.m[11]=0;
        iv.m[12]=tx; iv.m[13]=ty; iv.m[14]=tz;   iv.m[15]=1;
    }
    Vec3 ro={iv.m[12],iv.m[13],iv.m[14]};

    /* Inverse projection: unproject clip-space direction */
    float fov_y_half=atanf(1.0f/proj.m[5]);
    float aspect=proj.m[5]/proj.m[0];
    float rdx=nx*tanf(fov_y_half)*aspect;
    float rdy=ny*tanf(fov_y_half);
    /* Ray in view space, transform to world */
    Vec3 rd_view={rdx,rdy,-1.0f};
    Vec3 rd={
        iv.m[0]*rd_view.x+iv.m[4]*rd_view.y+iv.m[8]*rd_view.z,
        iv.m[1]*rd_view.x+iv.m[5]*rd_view.y+iv.m[9]*rd_view.z,
        iv.m[2]*rd_view.x+iv.m[6]*rd_view.y+iv.m[10]*rd_view.z
    };
    float rlen=sqrtf(rd.x*rd.x+rd.y*rd.y+rd.z*rd.z);
    if(rlen<0.0001f)return false;
    rd.x/=rlen;rd.y/=rlen;rd.z/=rlen;

    /* Binary search along ray for terrain intersection */
    float tlo=0.5f,thi=800.0f;
    /* early check: does ray hit terrain at all? */
    Vec3 hi_p={ro.x+rd.x*thi,ro.y+rd.y*thi,ro.z+rd.z*thi};
    float th=terrain_h(hi_p.x,hi_p.z);
    if(hi_p.y>th+50.0f){thi=800.0f;} /* extend search */

    for(int iter=0;iter<32;iter++){
        float tmid=(tlo+thi)*0.5f;
        Vec3 p={ro.x+rd.x*tmid,ro.y+rd.y*tmid,ro.z+rd.z*tmid};
        float h=terrain_h(p.x,p.z);
        if(p.y<h) thi=tmid; else tlo=tmid;
    }
    float t=(tlo+thi)*0.5f;
    out->x=ro.x+rd.x*t;
    out->y=ro.y+rd.y*t;
    out->z=ro.z+rd.z*t;
    return true;
}

/* ── Path editor HUD + spline overlay ───────────────────────── */
static void path_editor_hud(HUD *h,int win_w,int win_h){
    float W=(float)win_w, lh=18.0f, by=(float)win_h-8.0f;
    hud_rect(h,0,by-lh*4-4,W,lh*4+8, 0,0,0,0.70f);
    hud_str(h,8,by-=lh,1.3f,
        "[F1 PATH EDITOR]  LMB=place  RMB=remove last  Ctrl+Wheel=Width  S=save  C=clear  Z=undo",
        1.0f,0.85f,0.3f,1.0f);
    char buf[128];
    snprintf(buf,sizeof(buf),"Points: %d / %d   Half-width: %.1f wu   Tile: %.1f wu",
             g_cpath.count,CPATH_MAX_PTS,(double)g_cpath.width,(double)CPATH_TILE_SZ);
    hud_str(h,8,by-=lh,1.3f,buf,0.9f,0.9f,0.9f,1.0f);
    if(g_cpath.count>0){
        snprintf(buf,sizeof(buf),"Last: (%.0f, %.0f)",
                 (double)g_cpath.pts[(g_cpath.count-1)*2],
                 (double)g_cpath.pts[(g_cpath.count-1)*2+1]);
        hud_str(h,8,by-=lh,1.3f,buf,0.7f,1.0f,0.7f,1.0f);
    } else {
        hud_str(h,8,by-=lh,1.3f,"No points — click terrain to start",0.7f,0.7f,0.7f,1.0f);
    }
    hud_str(h,8,by-=lh,1.15f,
        "TIP: S exports point array to console. Ctrl+Wheel adjusts path width live.",
        0.5f,0.8f,1.0f,0.9f);
    hud_flush(h);
}

static void path_editor_draw(LineRenderer *lr,Mat4 proj,Mat4 view){
    if(g_cpath.count<1)return;
    for(int i=0;i<g_cpath.count-1;i++){
        float ax=g_cpath.pts[i*2],   az=g_cpath.pts[i*2+1];
        float bx=g_cpath.pts[(i+1)*2],bz=g_cpath.pts[(i+1)*2+1];
        float ay=terrain_h(ax,az)+0.2f, by2=terrain_h(bx,bz)+0.2f;
        lr_push((Vec3){ax,ay,az},(Vec3){bx,by2,bz}, 1.0f,0.85f,0.3f);
    }
    for(int i=0;i<g_cpath.count;i++){
        float x=g_cpath.pts[i*2],z=g_cpath.pts[i*2+1];
        float y=terrain_h(x,z)+0.25f;
        float cr=0.6f;
        lr_push((Vec3){x-cr,y,z},(Vec3){x+cr,y,z},1.0f,0.85f,0.3f);
        lr_push((Vec3){x,y,z-cr},(Vec3){x,y,z+cr},1.0f,0.85f,0.3f);
    }
    lr_flush(lr,proj,view);
}

/* ── Water zone editor HUD + overlay ────────────────────────── */
static void wzone_editor_hud(HUD *h,int win_w,int win_h,int sel){
    float W=(float)win_w,lh=18.0f,by=(float)win_h-8.0f;
    int rows=4;
    hud_rect(h,0,by-lh*(float)rows-4,W,lh*(float)rows+8,0,0,0,0.70f);
    hud_str(h,8,by-=lh,1.3f,
        "[F2 WATER ZONE EDITOR]  LMB=place zone  RMB=remove nearest  Ctrl+Wheel=Radius  [ ]=Level  W=cycle style  S=save  C=clear",
        0.3f,0.8f,1.0f,1.0f);
    char buf[128];
    snprintf(buf,sizeof(buf),"Zones: %d / %d   Selected: %d",
             g_wzone_count,MAX_WATER_ZONES,sel);
    hud_str(h,8,by-=lh,1.3f,buf,0.9f,0.9f,0.9f,1.0f);
    if(sel>=0&&sel<g_wzone_count){
        WaterZone *z=&g_wzones[sel];
        const char *pn=z->preset_idx>=0&&z->preset_idx<WATER_PRESET_COUNT
                       ?WATER_PRESETS[z->preset_idx].name:"?";
        snprintf(buf,sizeof(buf),"  Style: %-16s  Radius: %.0f  Level: %.1f  Pos: (%.0f, %.0f)",
                 pn,(double)z->radius,(double)z->level,(double)z->cx,(double)z->cz);
        hud_str(h,8,by-=lh,1.3f,buf,1.0f,0.9f,0.3f,1.0f);
    } else {
        hud_str(h,8,by-=lh,1.3f,"Click water to place a zone",0.7f,0.7f,0.7f,1.0f);
    }
    hud_str(h,8,by-=lh,1.15f,
        "TIP: Each zone overrides the global water style in its radius. Blend is smooth at edge.",
        0.5f,0.8f,1.0f,0.9f);
    hud_flush(h);
}

static void wzone_editor_draw(LineRenderer *lr,Mat4 proj,Mat4 view,int sel){
    for(int i=0;i<g_wzone_count;i++){
        WaterZone *z=&g_wzones[i];
        if(!z->active)continue;
        float y=z->level+0.5f;
        bool is_sel=(i==sel);
        float r=is_sel?1.0f:0.3f, g2=is_sel?0.9f:0.7f, b=is_sel?0.2f:1.0f;
        /* draw circle approximation (32 segments) */
        int segs=32;
        for(int s=0;s<segs;s++){
            float a0=(float)s/(float)segs*2.0f*3.14159f;
            float a1=(float)(s+1)/(float)segs*2.0f*3.14159f;
            lr_push((Vec3){z->cx+cosf(a0)*z->radius,y,z->cz+sinf(a0)*z->radius},
                    (Vec3){z->cx+cosf(a1)*z->radius,y,z->cz+sinf(a1)*z->radius},
                    r,g2,b);
        }
        /* centre cross */
        float cr=3.0f;
        lr_push((Vec3){z->cx-cr,y,z->cz},(Vec3){z->cx+cr,y,z->cz},r,g2,b);
        lr_push((Vec3){z->cx,y,z->cz-cr},(Vec3){z->cx,y,z->cz+cr},r,g2,b);
    }
    lr_flush(lr,proj,view);
}

static void river_editor_hud(HUD *h, int win_w, int win_h){
    float W=(float)win_w;
    float lh=18.0f;
    float by=(float)win_h-8.0f;
    int rows=5;
    hud_rect(h,0,by-lh*(float)rows-4,W,lh*(float)rows+8,0,0,0,0.70f);
    hud_str(h,8,by-=lh,1.3f,
        "[F4 RIVER EDITOR]  LMB=place point  RMB=remove last  Z=undo  C=clear  S=save  Ctrl+Wheel=Width  W=water style",
        1.0f,0.7f,0.2f,1.0f);
    char buf[128];
    snprintf(buf,sizeof(buf),"Points: %d / %d   Width: %.0f   Depth: %.1f",
             g_criver.count,CRIVER_MAX_PTS,
             (double)g_criver.width,(double)CRIVER_DEPTH);
    hud_str(h,8,by-=lh,1.3f,buf,0.9f,0.9f,0.9f,1.0f);
    /* Water style */
    const char *sname=(g_wstyle.preset_idx>=0&&g_wstyle.preset_idx<WATER_PRESET_COUNT)
                      ?WATER_PRESETS[g_wstyle.preset_idx].name:"Custom";
    snprintf(buf,sizeof(buf),"Water Style: %s  [W to cycle]",sname);
    hud_str(h,8,by-=lh,1.3f,buf,0.3f,0.8f,1.0f,1.0f);
    if(g_criver.count>0){
        snprintf(buf,sizeof(buf),"Last point: (%.0f, %.0f)",
                 (double)g_criver.pts[(g_criver.count-1)*2],
                 (double)g_criver.pts[(g_criver.count-1)*2+1]);
        hud_str(h,8,by-=lh,1.3f,buf,0.7f,1.0f,0.7f,1.0f);
    } else {
        hud_str(h,8,by-=lh,1.3f,"No points — click terrain to start",
                0.7f,0.7f,0.7f,1.0f);
    }
    hud_str(h,8,by-=lh,1.15f,
        "TIP: Place points along the river path. S exports C array to console for hardcoding.",
        0.5f,0.8f,1.0f,0.9f);
}

/* Draw river path as a line strip in the world */
static void river_editor_draw(LineRenderer *lr, Mat4 proj, Mat4 view){
    if(g_criver.count<1)return;
    /* Draw each point as a cross + connect segments */
    for(int i=0;i<g_criver.count;i++){
        float px=g_criver.pts[i*2], pz=g_criver.pts[i*2+1];
        float py=terrain_h(px,pz)+0.3f;
        float sz=3.0f;
        lr_push((Vec3){px-sz,py,pz},(Vec3){px+sz,py,pz}, 1,0.5f,0);
        lr_push((Vec3){px,py,pz-sz},(Vec3){px,py,pz+sz}, 1,0.5f,0);
        /* dot marker */
        lr_push((Vec3){px,py-0.1f,pz},(Vec3){px,py+2.0f,pz}, 1,1,0);
        /* connect to next */
        if(i<g_criver.count-1){
            float nx=g_criver.pts[(i+1)*2], nz=g_criver.pts[(i+1)*2+1];
            float ny=terrain_h(nx,nz)+0.3f;
            lr_push((Vec3){px,py,pz},(Vec3){nx,ny,nz}, 0.2f,0.8f,1.0f);
        }
    }
    lr_flush(lr,proj,view);
}

/* ── Prop Placer Editor (F5) ────────────────────────────────────
   A palette of FBX model names on the left; left-click places a
   prop at the terrain surface, right-click deletes the nearest.
   Ghost preview follows the cursor.  S saves to props_custom.txt
   and prints the C prop_register array.                          */

#define PROP_PALETTE_MAX  64
#define PROP_PLACER_FILE  "props_custom.txt"

typedef struct {
    /* Palette — list of model filenames the user can cycle through */
    char   palette[PROP_PALETTE_MAX][256];
    int    palette_count;
    int    palette_sel;   /* currently chosen model index          */
    int    palette_scroll;/* top visible row                       */

    /* Ghost state */
    bool   has_ghost;
    Vec3   ghost_pos;

    /* Placement modifiers */
    float  place_ry;      /* Y-rotation for next placement (degrees) */
    float  place_scale;   /* scale for next placement                */
    bool   snap_y;        /* true = snap Y to terrain                */

    /* Mouse state */
    bool   lmb_prev, rmb_prev;
    int    wheel;         /* accumulated scroll this frame           */
} PropPlacer;

static PropPlacer g_placer;
typedef struct ForestSystem ForestSystem;

/* Scan models/ directory for .fbx files and populate palette */
static void placer_scan_models(PropPlacer *pp){
    pp->palette_count=0;
#if defined(_WIN32)
    WIN32_FIND_DATAA fd;
    HANDLE h=FindFirstFileA("models\\*.fbx",&fd);
    if(h==INVALID_HANDLE_VALUE)return;
    do{
        if(pp->palette_count>=PROP_PALETTE_MAX)break;
        snprintf(pp->palette[pp->palette_count],256,"models/%s",fd.cFileName);
        pp->palette_count++;
    }while(FindNextFileA(h,&fd));
    FindClose(h);
#else
    /* POSIX */
    #include <dirent.h>
    DIR *d=opendir("models");
    if(!d){
        /* No models/ dir — seed with some common names so editor isn't empty */
        const char *defaults[]={"models/village/House_FBX/House_1/House_1.fbx","models/village/Tree_FBX/Birch1.fbx",
            "models/barrel/barrel.fbx","models/village/Benches/Crate.fbx","models/torch.fbx",
            "models/well.fbx","models/village/Fence/fence_1.fbx","models/tower.fbx",NULL};
        for(int i=0;defaults[i]&&pp->palette_count<PROP_PALETTE_MAX;i++){
            strncpy(pp->palette[pp->palette_count],defaults[i],255);
            pp->palette_count++;
        }
        return;
    }
    struct dirent *ent;
    while((ent=readdir(d))!=NULL&&pp->palette_count<PROP_PALETTE_MAX){
        const char *n=ent->d_name;
        size_t l=strlen(n);
        if(l>4&&strcmp(n+l-4,".fbx")==0){
            snprintf(pp->palette[pp->palette_count],256,"models/%s",n);
            pp->palette_count++;
        }
    }
    closedir(d);
#endif
    if(pp->palette_count==0){
        /* ultimate fallback */
        strncpy(pp->palette[0],"models/house.fbx",255);
        pp->palette_count=1;
    }
    printf("[PropPlacer] Found %d models in palette\n",pp->palette_count);
}

static void placer_init(PropPlacer *pp){
    memset(pp,0,sizeof(*pp));
    pp->place_scale=1.0f;
    pp->snap_y=true;
    placer_scan_models(pp);
}

/* Save placed props to file + print C array */
static void placer_save(PropList *pl){
    FILE *f=fopen(PROP_PLACER_FILE,"w");
    if(f){
        fprintf(f,"# Props placed in editor — %d total\n",pl->count);
        /* Only save props that were placed via the editor (path present, active) */
        int saved=0;
        for(int i=0;i<pl->count;i++){
            Prop *p=&pl->list[i];
            if(!p->active)continue;
            fprintf(f,"%s %.3f %.3f %.3f %.3f %.3f %.3f %.3f %d\n",
                    p->path,
                    (double)p->pos.x,(double)p->pos.y,(double)p->pos.z,
                    (double)p->rot_x,(double)p->rot_y,(double)p->rot_z,
                    (double)p->scale,(int)p->coll_type);
            saved++;
        }
        fclose(f);
        printf("[PropPlacer] Saved %d props to %s\n",saved,PROP_PLACER_FILE);
    }
    /* Also print C array */
    printf("[PropPlacer] C array:\n");
    for(int i=0;i<pl->count;i++){
        Prop *p=&pl->list[i];
        if(!p->active)continue;
        printf("    prop_register(&g_props,\"%s\", %.2ff,%.2ff,%.2ff, 0,%.1ff,0, %.3ff, %s,0,0,0,0);\n",
               p->path,
               (double)p->pos.x,(double)p->pos.y,(double)p->pos.z,
               (double)p->rot_y,(double)p->scale,
               p->coll_type==PROP_COLL_MESH?"PROP_COLL_MESH":
               p->coll_type==PROP_COLL_AABB?"PROP_COLL_AABB":"PROP_COLL_NONE");
    }
}

/* Load previously saved props at startup */
static void placer_load(PropList *pl){
    FILE *f=fopen(PROP_PLACER_FILE,"r");
    if(!f)return;
    char line[512];int loaded=0;
    while(fgets(line,sizeof(line),f)){
        if(line[0]=='#')continue;
        char path[256]; float x,y,z,rx,ry,rz,sc; int coll;
        if(sscanf(line,"%255s %f %f %f %f %f %f %f %d",
                  path,&x,&y,&z,&rx,&ry,&rz,&sc,&coll)==9){
            prop_register(pl,path,x,y,z,rx,ry,rz,sc,(PropCollType)coll,0,0,0,0);
            loaded++;
        }
    }
    fclose(f);
    printf("[PropPlacer] Loaded %d props from %s\n",loaded,PROP_PLACER_FILE);
}

/* Draw ghost (wireframe box) at cursor hit position */
static void placer_draw_ghost(LineRenderer *lr, Vec3 pos, float scale,
                               Mat4 proj, Mat4 view){
    float h=scale*2.0f, r=scale*0.6f;
    Vec3 p=pos;
    /* Bottom rect */
    lr_push((Vec3){p.x-r,p.y,p.z-r},(Vec3){p.x+r,p.y,p.z-r}, 0.2f,1.0f,0.4f);
    lr_push((Vec3){p.x+r,p.y,p.z-r},(Vec3){p.x+r,p.y,p.z+r}, 0.2f,1.0f,0.4f);
    lr_push((Vec3){p.x+r,p.y,p.z+r},(Vec3){p.x-r,p.y,p.z+r}, 0.2f,1.0f,0.4f);
    lr_push((Vec3){p.x-r,p.y,p.z+r},(Vec3){p.x-r,p.y,p.z-r}, 0.2f,1.0f,0.4f);
    /* Top rect */
    lr_push((Vec3){p.x-r,p.y+h,p.z-r},(Vec3){p.x+r,p.y+h,p.z-r}, 0.2f,1.0f,0.4f);
    lr_push((Vec3){p.x+r,p.y+h,p.z-r},(Vec3){p.x+r,p.y+h,p.z+r}, 0.2f,1.0f,0.4f);
    lr_push((Vec3){p.x+r,p.y+h,p.z+r},(Vec3){p.x-r,p.y+h,p.z+r}, 0.2f,1.0f,0.4f);
    lr_push((Vec3){p.x-r,p.y+h,p.z+r},(Vec3){p.x-r,p.y+h,p.z-r}, 0.2f,1.0f,0.4f);
    /* Verticals */
    lr_push((Vec3){p.x-r,p.y,p.z-r},(Vec3){p.x-r,p.y+h,p.z-r}, 0.2f,1.0f,0.4f);
    lr_push((Vec3){p.x+r,p.y,p.z-r},(Vec3){p.x+r,p.y+h,p.z-r}, 0.2f,1.0f,0.4f);
    lr_push((Vec3){p.x+r,p.y,p.z+r},(Vec3){p.x+r,p.y+h,p.z+r}, 0.2f,1.0f,0.4f);
    lr_push((Vec3){p.x-r,p.y,p.z+r},(Vec3){p.x-r,p.y+h,p.z+r}, 0.2f,1.0f,0.4f);
    /* Yaw indicator spike */
    float yrad=g_placer.place_ry*(float)M_PI/180.0f;
    Vec3 tip={p.x+sinf(yrad)*r*1.6f, p.y+h*0.5f, p.z+cosf(yrad)*r*1.6f};
    lr_push((Vec3){p.x,p.y+h*0.5f,p.z}, tip, 1.0f,1.0f,0.0f);
    lr_flush(lr,proj,view);
}

/* Palette + status HUD */
static void placer_hud(HUD *h, PropPlacer *pp, int win_w, int win_h){
    float W=(float)win_w, H=(float)win_h;
    float lh=17.0f, pw=260.0f, px=8.0f;
    int vis=12; /* visible rows */

    /* Left palette panel */
    float py=H*0.08f;
    hud_rect(h,px-4,py-4,pw+8,lh*(float)vis+8, 0,0,0,0.72f);

    /* Title */
    hud_str(h,px,py-lh,1.25f,"[F8 PROP PLACER]", 1.0f,0.7f,0.1f,1.0f);

    for(int i=0;i<vis&&(pp->palette_scroll+i)<pp->palette_count;i++){
        int idx=pp->palette_scroll+i;
        bool sel=(idx==pp->palette_sel);
        float ry2=py+(float)i*lh;
        if(sel) hud_rect(h,px-2,ry2-1,pw+4,lh+1, 0.3f,0.6f,0.15f,0.85f);
        /* Show just the filename, not the full path */
        const char *full=pp->palette[idx];
        const char *name=strrchr(full,'/');
        name=name?name+1:full;
        char label[64]; strncpy(label,name,63);
        /* strip .fbx */
        char *dot=strrchr(label,'.');if(dot)*dot='\0';
        hud_str(h,px+4,ry2,1.2f,label,
                sel?1.0f:0.75f, sel?1.0f:0.75f, sel?0.4f:0.75f, 1.0f);
    }
    /* Scroll indicator */
    if(pp->palette_count>vis){
        char sb[32];snprintf(sb,sizeof(sb),"%d/%d",pp->palette_sel+1,pp->palette_count);
        hud_str(h,px,py+(float)vis*lh+2,1.1f,sb,0.6f,0.6f,0.6f,1.0f);
    }

    /* Bottom status bar */
    float by=H-8.0f; float blh=17.0f;
    int rows=4;
    hud_rect(h,0,by-blh*(float)rows-4,W,blh*(float)rows+8, 0,0,0,0.68f);
    char buf[256];
    const char *sel_name=pp->palette_count?pp->palette[pp->palette_sel]:"(none)";
    const char *sn=strrchr(sel_name,'/');sn=sn?sn+1:sel_name;
    snprintf(buf,sizeof(buf),"Model: %s   Scale: %.2f   RotY: %.0f°   SnapY: %s",
             sn,(double)pp->place_scale,(double)pp->place_ry,pp->snap_y?"ON":"OFF");
    hud_str(h,8,by-=blh,1.3f,buf, 0.9f,0.9f,0.6f,1.0f);
    hud_str(h,8,by-=blh,1.2f,
        "LMB=Place  RMB=Delete nearest  Wheel=Browse palette  Ctrl+Wheel=Scale",
        0.8f,0.8f,0.8f,1.0f);
    hud_str(h,8,by-=blh,1.2f,
        "[=RotY+15  ]=RotY-15  G=SnapY toggle  DEL=Remove sel  S=Save",
        0.8f,0.8f,0.8f,1.0f);
    hud_str(h,8,by-=blh,1.15f,
        "TIP: S exports prop_register() C array to console for hardcoding.",
        0.5f,0.8f,1.0f,0.9f);

    hud_flush(h);
}

/* ═══════════════════════════════════════════════════════════════
   SPAWN EDITOR  (F12)
   Click to place/move enemy spawn points, choose type from palette,
   save to "spawns_custom.txt" which is loaded at startup.
   ═══════════════════════════════════════════════════════════════ */
#define SPAWN_EDITOR_FILE  "spawns_custom.txt"
#define MAX_SPAWN_POINTS   64

typedef struct {
    Vec3      pos;
    EnemyType etype;
    bool      active;
} SpawnPoint;

typedef struct {
    SpawnPoint points[MAX_SPAWN_POINTS];
    int        count;
} SpawnList;

static SpawnList g_spawns;

/* Save spawn list to file and print C array */
static void spawns_save(SpawnList *sl){
    FILE *f=fopen(SPAWN_EDITOR_FILE,"w");
    if(f){
        fprintf(f,"# Enemy spawn points — %d total\n",sl->count);
        for(int i=0;i<sl->count;i++){
            SpawnPoint *sp=&sl->points[i];
            if(!sp->active)continue;
            fprintf(f,"%d %.3f %.3f %.3f\n",
                    (int)sp->etype,
                    (double)sp->pos.x,(double)sp->pos.y,(double)sp->pos.z);
        }
        fclose(f);
        int n=0;for(int i=0;i<sl->count;i++)if(sl->points[i].active)n++;
        printf("[SpawnEd] Saved %d spawn points to %s\n",n,SPAWN_EDITOR_FILE);
    }
    /* Print C array for hardcoding */
    printf("[SpawnEd] C array:\n");
    for(int i=0;i<sl->count;i++){
        SpawnPoint *sp=&sl->points[i];
        if(!sp->active)continue;
        const char *tname="ETYPE_KNIGHT";
        switch(sp->etype){
            case ETYPE_ARCHER: tname="ETYPE_ARCHER"; break;
            case ETYPE_BRUTE:  tname="ETYPE_BRUTE";  break;
            case ETYPE_SHADE:  tname="ETYPE_SHADE";  break;
            case ETYPE_BOSS:   tname="ETYPE_BOSS";   break;
            default: break;
        }
        printf("    enemy_spawn(&enemies[i++],%s,(Vec3){%.2ff,0,%.2ff},&enemy_amaps[%s]);\n",
               tname,(double)sp->pos.x,(double)sp->pos.z,tname);
    }
}

/* Load spawn list from file — called at startup, overwrites enemies[] */
static void spawns_load(SpawnList *sl){
    memset(sl,0,sizeof(*sl));
    FILE *f=fopen(SPAWN_EDITOR_FILE,"r");
    if(!f)return;
    char line[256]; int loaded=0;
    while(fgets(line,sizeof(line),f)&&sl->count<MAX_SPAWN_POINTS){
        if(line[0]=='#')continue;
        int et; float x,y,z;
        if(sscanf(line,"%d %f %f %f",&et,&x,&y,&z)==4){
            SpawnPoint *sp=&sl->points[sl->count++];
            sp->etype=(EnemyType)et;
            sp->pos=(Vec3){x,y,z};
            sp->active=true;
            loaded++;
        }
    }
    fclose(f);
    printf("[SpawnEd] Loaded %d spawn points from %s\n",loaded,SPAWN_EDITOR_FILE);
}

/* Apply loaded spawn points to the live enemies array.
   Appends after whatever enemies are already in the array (*ne).
   Called after spawns_load at startup.                          */
static void spawns_apply(SpawnList *sl, Enemy *enemies, int *ne,
                         AnimMap enemy_amaps[]){
    int n=*ne; /* start after hardcoded enemies */
    for(int i=0;i<sl->count&&n<MAX_ENEMIES;i++){
        SpawnPoint *sp=&sl->points[i];
        if(!sp->active)continue;
        enemy_spawn(&enemies[n],sp->etype,sp->pos,&enemy_amaps[sp->etype]);
        n++;
    }
    int added=n-*ne;
    *ne=n;
    printf("[SpawnEd] Applied %d custom enemy spawns (total: %d)\n",added,n);
}

/* Draw the spawn editor HUD */
static void spawned_hud(HUD *h, SpawnList *sl, EnemyType sel_type,
                        int hovered_idx, int win_w, int win_h){
    float W=(float)win_w, H=(float)win_h;
    float pw=200, lh=18;
    float px=W-pw-6, py=8;

    /* Right-side panel background */
    hud_rect(h,px-4,py-4,pw+8,(float)(ETYPE_COUNT+2)*lh+12, 0,0,0,0.72f);
    hud_str(h,px,py,1.3f,"[F12 SPAWN EDITOR]",1.0f,0.5f,0.1f,1.0f); py+=lh+4;

    /* Type palette */
    for(int t=0;t<ETYPE_COUNT;t++){
        bool active=(t==(int)sel_type);
        if(active) hud_rect(h,px-2,py-1,pw+4,lh+2, 0.8f,0.4f,0.0f,0.85f);
        /* Count how many spawns of this type */
        int cnt=0;for(int i=0;i<sl->count;i++)
            if(sl->points[i].active&&sl->points[i].etype==(EnemyType)t)cnt++;
        char buf[64];
        snprintf(buf,sizeof(buf),"%d  %s  (%d)",t+1,EDEF[t].name,cnt);
        float r=EDEF[t].cr,g2=EDEF[t].cg,b=EDEF[t].cb;
        hud_str(h,px+4,py,1.2f,buf, active?1.0f:r, active?1.0f:g2, active?0.4f:b, 1.0f);
        py+=lh;
    }
    py+=4;

    /* Spawn count */
    int total=0;for(int i=0;i<sl->count;i++)if(sl->points[i].active)total++;
    char cnt_buf[64];snprintf(cnt_buf,sizeof(cnt_buf),"Total: %d / %d",total,MAX_SPAWN_POINTS);
    hud_str(h,px+4,py,1.15f,cnt_buf,0.7f,0.7f,0.7f,1.0f);

    /* Bottom status bar */
    float by=H-8.0f,blh=17.0f;
    hud_rect(h,0,by-blh*3-4,W,blh*3+8, 0,0,0,0.68f);
    hud_str(h,8,by-=blh,1.3f,
        "LMB=Place spawn  RMB=Delete nearest  1-5=Select type  S=Save",
        0.9f,0.9f,0.6f,1.0f);
    if(hovered_idx>=0){
        char hbuf[64];
        snprintf(hbuf,sizeof(hbuf),"Hover: spawn %d (%s)",
                 hovered_idx,EDEF[sl->points[hovered_idx].etype].name);
        hud_str(h,8,by-=blh,1.2f,hbuf,0.6f,1.0f,0.6f,1.0f);
    } else {
        hud_str(h,8,by-=blh,1.2f,"Hover over a marker to highlight it",
                0.5f,0.5f,0.5f,1.0f);
    }
    hud_str(h,8,by-=blh,1.15f,
        "TIP: S exports enemy_spawn() C array to console for hardcoding.",
        0.5f,0.8f,1.0f,0.9f);

    hud_flush(h);
}


/* ══════════════════════════════════════════════════════════════
   NPC SPAWN EDITOR  (F11)
   LMB = place NPC, RMB = delete nearest, 1-4 = select type
   S = save to npcs_custom.txt, auto-saves on place/delete
   ══════════════════════════════════════════════════════════════ */
#define NPC_SPAWN_FILE    "npcs_custom.txt"
#define MAX_NPC_SPAWNS    64
#define NPC_WP_MAX  8    /* max waypoints per NPC */
enum{ SHOP_GENERAL=0, SHOP_WEAPONS, SHOP_ARMOR, SHOP_ALCHEMY, SHOP_PROFILE_COUNT };
static const char *SHOP_PROFILE_NAMES[SHOP_PROFILE_COUNT]={
    "General","Weapons","Armor","Alchemy"
};

typedef struct {
    Vec3  pos;
    float yaw;        /* facing direction (radians) */
    int   model_idx;  /* index into g_npc_models[] */
    char  name[32];
    bool  active;
    /* Patrol waypoints — empty means NPC stands still */
    Vec3  waypoints[NPC_WP_MAX];
    int   wp_count;
    float patrol_speed; /* 0 = default 1.4 */
    float patrol_wait;  /* pause at each wp, 0 = default 2.0 */
    /* Dialogue lines */
    char  dialogue[NPC_LINES_MAX][NPC_LINE_LEN];
    int   dialogue_count;
    bool  is_merchant;
    int   shop_id;
} NpcSpawnPoint;

typedef struct {
    NpcSpawnPoint points[MAX_NPC_SPAWNS];
    int           count;
} NpcSpawnList;

static NpcSpawnList g_npc_spawns;

/* Names/colours per model slot for editor display */
static const char *NPC_TYPE_NAMES[NPC_MODEL_MAX]={
    "Villager","Merchant","Guard","Elder","Type4","Type5","Type6","Type7"
};
static const float NPC_TYPE_COLORS[NPC_MODEL_MAX][3]={
    {0.8f,0.8f,0.4f}, /* villager — yellow  */
    {0.2f,0.9f,0.3f}, /* merchant — green   */
    {0.3f,0.5f,1.0f}, /* guard    — blue    */
    {0.9f,0.3f,0.9f}, /* elder    — purple  */
    {0.8f,0.5f,0.2f},{0.4f,0.9f,0.9f},{0.9f,0.4f,0.4f},{0.7f,0.7f,0.7f}
};

static void npc_spawns_save(NpcSpawnList *sl){
    FILE *f=fopen(NPC_SPAWN_FILE,"w");
    if(f){
        fprintf(f,"# NPC spawn points\n");
        fprintf(f,"# n model_idx x y z yaw name\n");
        fprintf(f,"# w x y z   (waypoint lines follow each n line)\n");
        for(int i=0;i<sl->count;i++){
            NpcSpawnPoint *sp=&sl->points[i];
            if(!sp->active)continue;
            fprintf(f,"n %d %.3f %.3f %.3f %.3f %.2f %.2f %s\n",
                    sp->model_idx,
                    (double)sp->pos.x,(double)sp->pos.y,(double)sp->pos.z,
                    (double)sp->yaw,
                    (double)(sp->patrol_speed>0?sp->patrol_speed:1.4f),
                    (double)(sp->patrol_wait >0?sp->patrol_wait :2.0f),
                    sp->name);
            for(int wi=0;wi<sp->wp_count;wi++)
                fprintf(f,"w %.3f %.3f %.3f\n",
                        (double)sp->waypoints[wi].x,
                        (double)sp->waypoints[wi].y,
                        (double)sp->waypoints[wi].z);
            if(sp->is_merchant) fprintf(f,"m\n");
            if(sp->shop_id>0) fprintf(f,"s %d\n",sp->shop_id);
            for(int di=0;di<sp->dialogue_count;di++)
                fprintf(f,"d %s\n",sp->dialogue[di]);
        }
        fclose(f);
        int n=0;for(int i=0;i<sl->count;i++)if(sl->points[i].active)n++;
        printf("[NpcEd] Saved %d NPC spawns to %s\n",n,NPC_SPAWN_FILE);
    }
}

static void npc_spawns_load(NpcSpawnList *sl){
    memset(sl,0,sizeof(*sl));
    FILE *f=fopen(NPC_SPAWN_FILE,"r");
    if(!f)return;
    char line[256]; int loaded=0;
    NpcSpawnPoint *cur=NULL;
    while(fgets(line,sizeof(line),f)&&sl->count<MAX_NPC_SPAWNS){
        if(line[0]=='#')continue;
        if(line[0]=='n'){
            /* NPC line: n model_idx x y z yaw speed wait name */
            int mi; float x,y,z,yaw,spd=1.4f,wait=2.0f; char name[32]="NPC";
            if(sscanf(line+2,"%d %f %f %f %f %f %f %31s",
                      &mi,&x,&y,&z,&yaw,&spd,&wait,name)>=5){
                cur=&sl->points[sl->count++];
                memset(cur,0,sizeof(*cur));
                cur->model_idx=mi; cur->pos=(Vec3){x,y,z}; cur->yaw=yaw;
                cur->patrol_speed=spd; cur->patrol_wait=wait;
                strncpy(cur->name,name,31); cur->name[31]=0;
                cur->active=true; loaded++;
            }
        } else if(line[0]=='w'&&cur&&cur->wp_count<NPC_WP_MAX){
            /* Waypoint line: w x y z */
            float wx,wy,wz;
            if(sscanf(line+2,"%f %f %f",&wx,&wy,&wz)==3)
                cur->waypoints[cur->wp_count++]=(Vec3){wx,wy,wz};
        } else if(line[0]=='d'&&cur&&cur->dialogue_count<NPC_LINES_MAX){
            /* Dialogue line: d <text> */
            char *txt=line+2;
            int tlen=(int)strlen(txt);
            while(tlen>0&&(txt[tlen-1]=='\n'||txt[tlen-1]=='\r'))txt[--tlen]=0;
            if(tlen>0){
                strncpy(cur->dialogue[cur->dialogue_count],txt,NPC_LINE_LEN-1);
                cur->dialogue[cur->dialogue_count][NPC_LINE_LEN-1]=0;
                cur->dialogue_count++;
            }
        } else if(line[0]=='m'&&cur){
            cur->is_merchant=true;
        } else if(line[0]=='s'&&cur){
            int shop_id=0;
            if(sscanf(line+2,"%d",&shop_id)==1)
                cur->shop_id=(shop_id>=0&&shop_id<SHOP_PROFILE_COUNT)?shop_id:SHOP_GENERAL;
        } else if(line[0]!='\n'){
            /* Legacy format: model_idx x y z yaw name */
            int mi; float x,y,z,yaw; char name[32]="NPC";
            if(sscanf(line,"%d %f %f %f %f %31s",&mi,&x,&y,&z,&yaw,name)>=5){
                cur=&sl->points[sl->count++];
                memset(cur,0,sizeof(*cur));
                cur->model_idx=mi; cur->pos=(Vec3){x,y,z}; cur->yaw=yaw;
                cur->patrol_speed=1.4f; cur->patrol_wait=2.0f;
                strncpy(cur->name,name,31); cur->name[31]=0;
                cur->active=true; loaded++;
            }
        }
    }
    fclose(f);
    printf("[NpcEd] Loaded %d NPC spawns from %s\n",loaded,NPC_SPAWN_FILE);
}

/* Forward declarations -- npc_add/patrol defined later in NPC system */
static int  npc_add(NpcSystem *ns, Vec3 pos, float yaw, const char *name);
static void npc_patrol_add(int npc_idx, float speed, float wait_secs);
static void npc_add_line(NpcSystem *ns, int idx, const char *line);
static void npc_patrol_waypoint(int npc_idx, Vec3 wp);

static void npc_spawns_apply(NpcSpawnList *sl, NpcSystem *ns){
    for(int i=0;i<sl->count&&ns->count<NPC_MAX;i++){
        NpcSpawnPoint *sp=&sl->points[i];
        if(!sp->active)continue;
        int idx=npc_add(ns,sp->pos,sp->yaw,sp->name);
        ns->npcs[idx].model_idx=sp->model_idx;
        ns->npcs[idx].anim_idx=0;
        ns->npcs[idx].is_merchant=sp->is_merchant;
        ns->npcs[idx].shop_id=sp->shop_id;
        for(int di=0;di<sp->dialogue_count;di++)
            npc_add_line(ns,idx,sp->dialogue[di]);
        /* Set up patrol if waypoints are defined */
        if(sp->wp_count>=2){
            float spd=sp->patrol_speed>0?sp->patrol_speed:1.4f;
            float wait=sp->patrol_wait >0?sp->patrol_wait :2.0f;
            npc_patrol_add(idx,spd,wait);
            for(int wi=0;wi<sp->wp_count;wi++)
                npc_patrol_waypoint(idx,sp->waypoints[wi]);
        }
    }
    printf("[NpcEd] Applied %d NPC spawns\n",sl->count);
}

static void npc_spawns_draw_markers(NpcSpawnList *sl, LineRenderer *lr,
                                    HUD *h, int sel_type, int hov_idx,
                                    Mat4 proj, Mat4 view, int win_w, int win_h){
    for(int i=0;i<sl->count;i++){
        NpcSpawnPoint *sp=&sl->points[i];
        if(!sp->active)continue;
        bool hov=(i==hov_idx);
        int mi=sp->model_idx; if(mi<0||mi>=NPC_MODEL_MAX)mi=0;
        float r=NPC_TYPE_COLORS[mi][0],g2=NPC_TYPE_COLORS[mi][1],b=NPC_TYPE_COLORS[mi][2];
        if(hov){r=1.0f;g2=1.0f;b=0.2f;}
        float sz=hov?0.9f:(mi==sel_type?0.7f:0.5f);
        Vec3 p=sp->pos; float top=sz*1.6f;
        /* Diamond marker */
        Vec3 pk={p.x,p.y+top,p.z}, bk={p.x,p.y-sz*0.4f,p.z};
        Vec3 pn={p.x,p.y+sz*0.4f,p.z-sz}, ps={p.x,p.y+sz*0.4f,p.z+sz};
        Vec3 pw={p.x-sz,p.y+sz*0.4f,p.z}, pe={p.x+sz,p.y+sz*0.4f,p.z};
        /* Direction spike showing yaw */
        Vec3 fwd={p.x+sinf(sp->yaw)*sz*1.5f,p.y+sz*0.4f,p.z+cosf(sp->yaw)*sz*1.5f};
        lr_push(pk,pn,r,g2,b);lr_push(pk,ps,r,g2,b);
        lr_push(pk,pw,r,g2,b);lr_push(pk,pe,r,g2,b);
        lr_push(bk,pn,r,g2,b);lr_push(bk,ps,r,g2,b);
        lr_push(bk,pw,r,g2,b);lr_push(bk,pe,r,g2,b);
        lr_push(pn,pw,r,g2,b);lr_push(pw,ps,r,g2,b);
        lr_push(ps,pe,r,g2,b);lr_push(pe,pn,r,g2,b);
        lr_push((Vec3){p.x,p.y+sz*0.4f,p.z},fwd,1.0f,1.0f,1.0f); /* facing arrow */
        float ndcx,ndcy;
        if(world_to_ndc((Vec3){pk.x,pk.y+0.3f,pk.z},proj,view,&ndcx,&ndcy)){
            float sx=(ndcx*0.5f+0.5f)*(float)win_w;
            float sy=(-ndcy*0.5f+0.5f)*(float)win_h;
            hud_str(h,sx+4,sy,1.1f,sp->name,r,g2,b,hov?1.0f:0.75f);
        }
    }
    lr_flush(lr,proj,view);
}

static void npc_spawns_hud(HUD *h, NpcSpawnList *sl, int sel_type,
                            int hov_idx, int win_w, int win_h){
    float W=(float)win_w, H=(float)win_h;
    float pw=220, lh=18;
    float px=W-pw-6, py=8;
    int nshown=4; /* show first 4 model slots */
    hud_rect(h,px-4,py-4,pw+8,(float)(nshown+3)*lh+12,0,0,0,0.72f);
    hud_str(h,px,py,1.3f,"[F11 NPC EDITOR]",0.4f,1.0f,0.5f,1.0f); py+=lh+4;
    for(int t=0;t<nshown;t++){
        bool active=(t==sel_type);
        if(active)hud_rect(h,px-2,py-1,pw+4,lh+2,0.1f,0.5f,0.2f,0.85f);
        int cnt=0;for(int i=0;i<sl->count;i++)
            if(sl->points[i].active&&sl->points[i].model_idx==t)cnt++;
        char buf[64];snprintf(buf,sizeof(buf),"%d  %s  (%d)",t+1,NPC_TYPE_NAMES[t],cnt);
        float r=NPC_TYPE_COLORS[t][0],g2=NPC_TYPE_COLORS[t][1],b=NPC_TYPE_COLORS[t][2];
        hud_str(h,px+4,py,1.2f,buf,active?1.0f:r,active?1.0f:g2,active?0.4f:b,1.0f);
        py+=lh;
    }
    py+=4;
    int total=0;for(int i=0;i<sl->count;i++)if(sl->points[i].active)total++;
    char cnt_buf[64];snprintf(cnt_buf,sizeof(cnt_buf),"Total: %d / %d",total,MAX_NPC_SPAWNS);
    hud_str(h,px+4,py,1.15f,cnt_buf,0.7f,0.7f,0.7f,1.0f);
    float by=H-8.0f,blh=17.0f;
    hud_rect(h,0,by-blh*3-4,W,blh*3+8,0,0,0,0.68f);
    if(hov_idx>=0){
        hud_str(h,8,by-=blh,1.3f,
            "LMB=Place  RMB=Delete  1-4=Type  Wheel=Rotate  P=Set Waypoints  S=Save",
            0.9f,0.9f,0.6f,1.0f);
    } else {
        hud_str(h,8,by-=blh,1.3f,
            "LMB=Place NPC  RMB=Delete  1-4=Type  Wheel=Rotate  Hover+P=Waypoints  S=Save",
            0.9f,0.9f,0.6f,1.0f);
    }
    if(hov_idx>=0){
        NpcSpawnPoint *sp=&sl->points[hov_idx];
        char hbuf[64];snprintf(hbuf,sizeof(hbuf),"Hover: %s (model %d)",sp->name,sp->model_idx);
        hud_str(h,8,by-=blh,1.2f,hbuf,0.6f,1.0f,0.6f,1.0f);
    } else {
        hud_str(h,8,by-=blh,1.2f,"Hover over a marker to highlight it",0.5f,0.5f,0.5f,1.0f);
    }
    hud_str(h,8,by-=blh,1.15f,"White spike = NPC facing direction.",0.5f,0.8f,1.0f,0.9f);
    hud_flush(h);
}

static void npc_spawns_hud_dialogue(HUD *h, NpcSpawnList *sl, int dlg_npc,
                                     int sel, const char *buf, int win_w, int win_h){
    if(dlg_npc<0||dlg_npc>=sl->count)return;
    NpcSpawnPoint *sp=&sl->points[dlg_npc];
    float W=(float)win_w, H=(float)win_h;
    float pw=580, lh=22, pad=10;
    float px=W*0.5f-pw*0.5f;
    float rows=(float)(NPC_LINES_MAX+4);
    float ph=rows*lh+pad*2+8;
    float py=H*0.5f-ph*0.5f;

    /* Background panel */
    hud_rect(h,px-4,py-4,pw+8,ph+8, 0.0f,0.0f,0.0f,0.88f);
    hud_rect(h,px,py,pw,ph,          0.04f,0.04f,0.06f,0.92f);
    hud_rect(h,px,py-4,pw,3,         0.3f,0.6f,1.0f,1.0f);
    hud_rect(h,px,py+ph+1,pw,3,      0.3f,0.6f,1.0f,1.0f);

    /* Title */
    char title[64]; snprintf(title,sizeof(title),"DIALOGUE: %s",sp->name);
    hud_str(h,px+pad,py+pad,1.5f,title,0.4f,0.75f,1.0f,1.0f);

    /* Merchant toggle */
    const char *merch_lbl=sp->is_merchant?"[M] Merchant: YES":"[M] Merchant: NO";
    hud_str(h,px+pw-160,py+pad,1.2f,merch_lbl,
            sp->is_merchant?0.3f:0.5f, sp->is_merchant?1.0f:0.5f, 0.3f, 1.0f);
    if(sp->is_merchant){
        char shop_lbl[64];
        snprintf(shop_lbl,sizeof(shop_lbl),"[ / ] Shop: %s",SHOP_PROFILE_NAMES[sp->shop_id]);
        hud_str(h,px+pw-190,py+pad+18,1.1f,shop_lbl, 0.7f,0.9f,1.0f,1.0f);
    }

    float cy=py+pad+lh+4;

    /* Existing lines */
    for(int i=0;i<NPC_LINES_MAX;i++){
        bool has_line=(i<sp->dialogue_count);
        bool is_sel=(i==sel);
        if(is_sel) hud_rect(h,px+pad-2,cy-2,pw-pad*2+4,lh+2, 0.1f,0.2f,0.4f,0.85f);
        char row[128];
        if(has_line){
            snprintf(row,sizeof(row),"%d. %s",i+1,sp->dialogue[i]);
            hud_str(h,px+pad+4,cy,1.3f,row,
                    is_sel?1.0f:0.85f, is_sel?1.0f:0.85f, is_sel?0.4f:0.85f, 1.0f);
        } else if(i==sp->dialogue_count){
            /* Empty slot — show input cursor here */
            hud_str(h,px+pad+4,cy,1.3f,"(new line...)",0.35f,0.35f,0.35f,0.7f);
        }
        cy+=lh;
    }

    /* Text input area */
    cy+=4;
    hud_rect(h,px+pad,cy,pw-pad*2,lh+6, 0.0f,0.05f,0.1f,0.92f);
    hud_rect(h,px+pad,cy+lh+6,pw-pad*2,2, 0.3f,0.6f,1.0f,0.8f);
    char input_disp[NPC_LINE_LEN+8];
    snprintf(input_disp,sizeof(input_disp),"> %s_",buf);
    hud_str(h,px+pad+6,cy+4,1.3f,input_disp,0.6f,0.9f,1.0f,1.0f);
    cy+=lh+12;

    /* Controls hint */
    hud_str(h,px+pad,cy,1.1f,
        "Enter=Confirm  Up/Down=Select line  Del=Delete  Esc=Cancel  M=Merchant  [ / ]=Shop",
        0.55f,0.55f,0.55f,0.9f);

    hud_flush(h);
}

static void npc_spawns_hud_wpmode(HUD *h, NpcSpawnList *sl, int wp_npc, int win_w, int win_h){
    if(wp_npc<0||wp_npc>=sl->count)return;
    NpcSpawnPoint *sp=&sl->points[wp_npc];
    float W=(float)win_w;
    hud_rect(h,0,0,W,28,0,0.15f,0,0.85f);
    char buf[128];
    snprintf(buf,sizeof(buf),
        "[WAYPOINT MODE] NPC: %s  |  %d waypoints  |  LMB=Add  Backspace=Undo  P=Done",
        sp->name,sp->wp_count);
    hud_str(h,8,6,1.3f,buf,0.2f,1.0f,0.3f,1.0f);
    hud_flush(h);
}

/* Draw spawn point markers in world-space (coloured diamond wireframe + label) */
static void spawned_draw_markers(SpawnList *sl, LineRenderer *lr,
                                 HUD *h, EnemyType sel_type, int hovered_idx,
                                 Mat4 proj, Mat4 view,
                                 int win_w, int win_h){
    for(int i=0;i<sl->count;i++){
        SpawnPoint *sp=&sl->points[i];
        if(!sp->active)continue;
        bool hov=(i==hovered_idx);
        float r=EDEF[sp->etype].cr, g2=EDEF[sp->etype].cg, b=EDEF[sp->etype].cb;
        if(hov){r=1.0f;g2=1.0f;b=0.2f;} /* highlight hovered in yellow */
        bool sel=(sp->etype==sel_type);
        float sz=hov?0.9f:sel?0.7f:0.5f;
        Vec3 p=sp->pos;
        float top=sz*1.6f;
        /* Diamond: 4 lines from top-peak to 4 equatorial points, 4 lines bottom */
        Vec3 pk ={p.x,    p.y+top,p.z};
        Vec3 bk ={p.x,    p.y-sz*0.4f,p.z};
        Vec3 n  ={p.x,    p.y+sz*0.4f,p.z-sz};
        Vec3 s2 ={p.x,    p.y+sz*0.4f,p.z+sz};
        Vec3 w  ={p.x-sz, p.y+sz*0.4f,p.z};
        Vec3 e  ={p.x+sz, p.y+sz*0.4f,p.z};
        lr_push(pk,n, r,g2,b); lr_push(pk,s2,r,g2,b);
        lr_push(pk,w, r,g2,b); lr_push(pk,e, r,g2,b);
        lr_push(bk,n, r,g2,b); lr_push(bk,s2,r,g2,b);
        lr_push(bk,w, r,g2,b); lr_push(bk,e, r,g2,b);
        lr_push(n,w,  r,g2,b); lr_push(w,s2, r,g2,b);
        lr_push(s2,e, r,g2,b); lr_push(e,n,  r,g2,b);
        /* World→screen label */
        float ndcx,ndcy;
        if(world_to_ndc((Vec3){pk.x,pk.y+0.3f,pk.z},proj,view,&ndcx,&ndcy)){
            float sx=(ndcx*0.5f+0.5f)*(float)win_w;
            float sy=(-ndcy*0.5f+0.5f)*(float)win_h;
            char lbl[32];snprintf(lbl,sizeof(lbl),"%s",EDEF[sp->etype].name);
            hud_str(h,sx+4,sy,1.1f,lbl,r,g2,b,hov?1.0f:0.75f);
        }
    }
    lr_flush(lr,proj,view);
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
    Mat4 proj, Mat4 view,
    Terrain *terrain)
{
    (void)win_w;
    float H=(float)win_h;
    float sc=1.5f;  /* text scale */
    float lh=sc*10.0f; /* line height */
    float x=8.0f, y=8.0f;
    char buf[128];

    /* ── Semi-transparent background panel ── */
    hud_rect(hud,0,0,340,H,0,0,0,0.45f);

    /* ── FPS / frame time ── */
    snprintf(buf,sizeof(buf),"FPS: %.1f  (%.2f ms)",(double)perf->fps,(double)perf->frame_ms);
    hud_str(hud,x,y,sc,buf,1.0f,1.0f,0.2f,1.0f); y+=lh+2;
    /* Terrain LOD triangle count */
    snprintf(buf,sizeof(buf),"Terrain tris: %d",terrain->last_tris);
    hud_str(hud,x,y,sc,buf,0.6f,1.0f,0.6f,1.0f); y+=lh+2;

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
    float anim_dur=scene_anim_duration(char_scene,player->anim_idx);
    if(anim_dur<=0.0f&&scene_anim_count(char_scene)>0)anim_dur=scene_anim_duration(char_scene,0);
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
    snprintf(buf,sizeof(buf),"Clips in Scene: %d",scene_anim_count(char_scene));
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


/* ── Audio System ───────────────────────────────────────────── */
/*  Drop these WAV files next to the executable (all optional):
 *   snd_step.wav      footstep
 *   snd_run.wav       running footstep (falls back to snd_step.wav)
 *   snd_attack.wav    sword swing
 *   snd_hit.wav       player takes damage
 *   snd_death.wav     player dies
 *   snd_enemy_die.wav enemy death
 *   snd_jump.wav      jump
 *   snd_ambient.wav   looping ambient (wind/birds)  */
/* ── Audio ──────────────────────────────────────────────────── */
#define SND_STEP        0
#define SND_RUN         1
#define SND_ATTACK      2
#define SND_HIT         3
#define SND_DEATH       4
#define SND_ENEMY_DIE   5
#define SND_JUMP        6
#define SND_FIREBALL    7
/* SND_FIREBALL_HIT 8 — forward declared above */
#define SND_HEAL        9
#define SND_MENU_CLICK  10
#define SND_LEVEL_UP    11
#define SND_ENEMY_GROWL 12
#define SND_ENEMY_ATTACK 13
/* Ambient one-shot SFX */
#define SND_AMB_WIND    14
#define SND_AMB_BIRD    15
#define SND_AMB_CRICKET 16
#define SND_AMB_LAVA    17
#define SND_AMB_CROW    18
#define SND_AMB_RIVER   19   /* babbling brook — plains/forest */
#define SND_AMB_THUNDER 20   /* distant thunder — mountains */
#define SND_AMB_OWL     21   /* night only */
#define SND_LOOP_RAIN   22   /* rain loop  */
#define SND_LOOP_NIGHT  23   /* cricket/night loop */
#define SND_DASH        24   /* ability 3: dash whoosh */
#define SND_SHIELD      25   /* ability 4: shield pop  */
#define SND_METEOR      26   /* ability 5: explosion   */
#define SND_ANIMAL_0    27   /* generic ambient animal call slot 0 */
#define SND_ANIMAL_1    28
#define SND_ANIMAL_2    29
#define SND_WF_LOOP     30   /* waterfall roar — looping proximity */
#define SND_COUNT       31

#define AMBILOOP_WIND_CH  28   /* SDL_mixer channel reserved for looping wind */
#define AMBILOOP_LAVA_CH  29   /* SDL_mixer channel reserved for looping lava rumble */
#define AMBILOOP_RAIN_CH  30   /* rain loop */
#define AMBILOOP_NITE_CH  31   /* night ambience loop (crickets/frogs) */
#define AMBILOOP_WF_CH    32   /* waterfall roar loop — proximity faded */

/* Music tracks */
#define MUS_EXPLORE      0
#define MUS_COMBAT       1
#define MUS_MENU         2
#define MUS_BIOME_MTN    3   /* mountain / snow wind */
#define MUS_BIOME_FOREST 4   /* forest / woodland */
#define MUS_BIOME_LAVA   5   /* volcanic ominous */
#define MUS_BIOME_PLAINS 6   /* open plains */
#define MUS_COUNT        7

/* Biome type — used for music + ambient SFX selection */
typedef enum{ BIOME_PLAINS=0,BIOME_FOREST,BIOME_MOUNTAIN,BIOME_LAVA,BIOME_SNOW,BIOME_COUNT }BiomeType;

/* Returns biome for a world position. Needs terrain_h() forward-declared below. */
static BiomeType get_biome(Vec3 pos){
    float h=terrain_h(pos.x,pos.z);
    float ld=sqrtf((pos.x+1600.0f)*(pos.x+1600.0f)+(pos.z+2200.0f)*(pos.z+2200.0f));
    if(ld<700.0f)  return BIOME_LAVA;
    if(h>45.0f)    return BIOME_SNOW;
    if(h>18.0f)    return BIOME_MOUNTAIN;
    if(pos.x<-300.0f&&pos.z>-300.0f&&pos.z<1300.0f) return BIOME_FOREST;
    return BIOME_PLAINS;
}
static const int BIOME_MUSIC[BIOME_COUNT]={
    MUS_BIOME_PLAINS,MUS_BIOME_FOREST,MUS_BIOME_MTN,MUS_BIOME_LAVA,MUS_BIOME_MTN
};
typedef struct{int snd;float min_interval;float weight;}AmbEntry;

/* DAY ambient tables [biome][slot] */
static const AmbEntry BIOME_AMB_DAY[BIOME_COUNT][5]={
    /* PLAINS  */ {{SND_AMB_BIRD,  7.f,.45f},{SND_AMB_RIVER,15.f,.25f},{SND_AMB_WIND, 8.f,.15f},{SND_AMB_CROW,16.f,.10f},{SND_AMB_CRICKET,25.f,.05f}},
    /* FOREST  */ {{SND_AMB_BIRD,  4.f,.50f},{SND_AMB_CRICKET, 8.f,.20f},{SND_AMB_RIVER,12.f,.20f},{SND_AMB_CROW, 9.f,.10f},{-1,0,0}},
    /* MOUNTAIN*/ {{SND_AMB_WIND,  5.f,.55f},{SND_AMB_THUNDER,30.f,.15f},{SND_AMB_CROW,10.f,.20f},{SND_AMB_BIRD,18.f,.10f},{-1,0,0}},
    /* LAVA    */ {{SND_AMB_WIND,  4.f,.60f},{SND_AMB_CROW,   12.f,.25f},{SND_AMB_THUNDER,20.f,.15f},{-1,0,0},{-1,0,0}},
    /* SNOW    */ {{SND_AMB_WIND,  4.f,.70f},{SND_AMB_CROW,   14.f,.20f},{SND_AMB_BIRD,20.f,.10f},{-1,0,0},{-1,0,0}},
};
/* NIGHT ambient tables — quieter, crickets + owls dominate */
static const AmbEntry BIOME_AMB_NIGHT[BIOME_COUNT][5]={
    /* PLAINS  */ {{SND_AMB_CRICKET,5.f,.50f},{SND_AMB_OWL,20.f,.25f},{SND_AMB_WIND,12.f,.15f},{SND_AMB_CROW,22.f,.10f},{-1,0,0}},
    /* FOREST  */ {{SND_AMB_CRICKET,4.f,.55f},{SND_AMB_OWL,14.f,.30f},{SND_AMB_WIND,10.f,.10f},{SND_AMB_CROW,18.f,.05f},{-1,0,0}},
    /* MOUNTAIN*/ {{SND_AMB_WIND,  4.f,.65f},{SND_AMB_OWL, 20.f,.20f},{SND_AMB_THUNDER,25.f,.15f},{-1,0,0},{-1,0,0}},
    /* LAVA    */ {{SND_AMB_WIND,  4.f,.70f},{SND_AMB_THUNDER,18.f,.30f},{-1,0,0},{-1,0,0},{-1,0,0}},
    /* SNOW    */ {{SND_AMB_WIND,  3.f,.80f},{SND_AMB_OWL, 25.f,.20f},{-1,0,0},{-1,0,0},{-1,0,0}},
};

typedef struct{
    Mix_Chunk *chunks[SND_COUNT];
    Mix_Music *music[MUS_COUNT];
    bool   ok;
    float  step_timer;
    int    cur_music;
    bool   in_combat;
    float  combat_timer;
    float  lava_vol;         /* 0..1 lava proximity */
    float  wind_vol;         /* 0..1 wind loop volume target */
    float  amb_accum;
    float  amb_timers[5];    /* per-slot cooldown (5 slots now) */
    BiomeType cur_biome;
    float  biome_change_timer;
    bool   is_night;
    /* Looping ambient: wind on ch 28, lava on ch 29 */
    bool   wind_loop_playing;
    bool   lava_loop_playing;
    bool   rain_loop_playing;
    bool   nite_loop_playing;
    float  rain_vol;            /* 0..1 rain loop volume */
    float  nite_vol;            /* 0..1 night loop volume */
    float  wf_vol;              /* 0..1 waterfall proximity volume */
    bool   wf_loop_playing;
    float  thunder_timer;       /* countdown to next thunder clap */
    int    music_vol;        /* 0-100, from options */
    int    sfx_vol;          /* 0-100, from options */
}AudioSystem;

static AudioSystem g_audio;

static void audio_init(void){
    memset(&g_audio,0,sizeof(g_audio));
    if(Mix_OpenAudio(44100,MIX_DEFAULT_FORMAT,2,2048)<0){
        printf("[Audio] SDL_mixer init failed: %s\n",Mix_GetError());return;}
    Mix_AllocateChannels(33);
    g_audio.ok=true;
    g_audio.cur_music=-1;

    /* SFX — all optional, silent if missing */
    const char *sfx[SND_COUNT]={
        "sound_effects/snd_step.wav",       /* SND_STEP        */
        "sound_effects/snd_run.wav",        /* SND_RUN         */
        "sound_effects/snd_attack.wav",     /* SND_ATTACK      */
        "sound_effects/snd_hit.wav",        /* SND_HIT         */
        "sound_effects/snd_death.wav",      /* SND_DEATH       */
        "sound_effects/snd_enemy_die.wav",  /* SND_ENEMY_DIE   */
        "sound_effects/snd_jump.wav",       /* SND_JUMP        */
        "sound_effects/snd_fireball.wav",   /* SND_FIREBALL    */
        "sound_effects/snd_fireball_hit.wav",/* SND_FIREBALL_HIT*/
        "sound_effects/snd_heal.wav",       /* SND_HEAL        */
        "sound_effects/snd_menu_click.wav", /* SND_MENU_CLICK  */
        "sound_effects/snd_level_up.wav",   /* SND_LEVEL_UP    */
        "sound_effects/snd_growl.wav",      /* SND_ENEMY_GROWL */
        "sound_effects/snd_enemy_atk.wav",  /* SND_ENEMY_ATTACK*/
        "sound_effects/amb_wind.wav",       /* SND_AMB_WIND    */
        "sound_effects/amb_bird.wav",       /* SND_AMB_BIRD    */
        "sound_effects/amb_cricket.wav",    /* SND_AMB_CRICKET */
        "sound_effects/amb_lava.wav",       /* SND_AMB_LAVA    */
        "sound_effects/amb_crow.wav",       /* SND_AMB_CROW    */
        "sound_effects/amb_river.wav",      /* SND_AMB_RIVER   */
        "sound_effects/amb_thunder.wav",    /* SND_AMB_THUNDER */
        "sound_effects/amb_owl.wav",        /* SND_AMB_OWL     */
        "sound_effects/amb_loop_rain.wav",  /* SND_LOOP_RAIN   */
        "sound_effects/amb_loop_night.wav", /* SND_LOOP_NIGHT  */
        "sound_effects/snd_dash.wav",       /* SND_DASH        */
        "sound_effects/snd_shield.wav",     /* SND_SHIELD      */
        "sound_effects/snd_meteor.wav",     /* SND_METEOR      */
        "sound_effects/snd_animal_0.wav",   /* SND_ANIMAL_0    */
        "sound_effects/snd_animal_1.wav",   /* SND_ANIMAL_1    */
        "sound_effects/snd_animal_2.wav",   /* SND_ANIMAL_2    */
        "sound_effects/amb_waterfall.wav",   /* SND_WF_LOOP     */
    };
    for(int i=0;i<SND_COUNT;i++){
        g_audio.chunks[i]=Mix_LoadWAV(sfx[i]);
        if(g_audio.chunks[i]) printf("[Audio] SFX: %s\n",sfx[i]);
        else                   printf("[Audio] optional SFX not found: %s\n",sfx[i]);
    }
    /* Fallbacks */
    if(!g_audio.chunks[SND_RUN]&&g_audio.chunks[SND_STEP])
        g_audio.chunks[SND_RUN]=g_audio.chunks[SND_STEP];
    if(!g_audio.chunks[SND_FIREBALL_HIT]&&g_audio.chunks[SND_HIT])
        g_audio.chunks[SND_FIREBALL_HIT]=g_audio.chunks[SND_HIT];
    if(!g_audio.chunks[SND_ENEMY_ATTACK]&&g_audio.chunks[SND_ATTACK])
        g_audio.chunks[SND_ENEMY_ATTACK]=g_audio.chunks[SND_ATTACK];
    /* Ability sound fallbacks */
    if(!g_audio.chunks[SND_DASH]&&g_audio.chunks[SND_JUMP])
        g_audio.chunks[SND_DASH]=g_audio.chunks[SND_JUMP];
    if(!g_audio.chunks[SND_SHIELD]&&g_audio.chunks[SND_HIT])
        g_audio.chunks[SND_SHIELD]=g_audio.chunks[SND_HIT];
    if(!g_audio.chunks[SND_METEOR]&&g_audio.chunks[SND_FIREBALL])
        g_audio.chunks[SND_METEOR]=g_audio.chunks[SND_FIREBALL];
    /* Ambient fallbacks */
    if(!g_audio.chunks[SND_AMB_CROW]&&g_audio.chunks[SND_AMB_BIRD])
        g_audio.chunks[SND_AMB_CROW]=g_audio.chunks[SND_AMB_BIRD];
    if(!g_audio.chunks[SND_AMB_OWL]&&g_audio.chunks[SND_AMB_CROW])
        g_audio.chunks[SND_AMB_OWL]=g_audio.chunks[SND_AMB_CROW];
    if(!g_audio.chunks[SND_AMB_RIVER]&&g_audio.chunks[SND_AMB_WIND])
        g_audio.chunks[SND_AMB_RIVER]=g_audio.chunks[SND_AMB_WIND];
    if(!g_audio.chunks[SND_AMB_THUNDER]&&g_audio.chunks[SND_AMB_WIND])
        g_audio.chunks[SND_AMB_THUNDER]=g_audio.chunks[SND_AMB_WIND];
    /* Loop fallbacks */
    if(!g_audio.chunks[SND_LOOP_RAIN]&&g_audio.chunks[SND_AMB_RIVER])
        g_audio.chunks[SND_LOOP_RAIN]=g_audio.chunks[SND_AMB_RIVER];
    if(!g_audio.chunks[SND_LOOP_NIGHT]&&g_audio.chunks[SND_AMB_CRICKET])
        g_audio.chunks[SND_LOOP_NIGHT]=g_audio.chunks[SND_AMB_CRICKET];
    if(!g_audio.chunks[SND_WF_LOOP]&&g_audio.chunks[SND_AMB_RIVER])
        g_audio.chunks[SND_WF_LOOP]=g_audio.chunks[SND_AMB_RIVER];
    /* Init thunder timer */
    g_audio.thunder_timer=20.0f+(float)(rand()%20);
    /* Default volumes from options */
    g_audio.music_vol=70; g_audio.sfx_vol=80;

    /* Music tracks — streamed */
    const char *mus[MUS_COUNT]={
        "background_music/mus_explore.ogg",        /* MUS_EXPLORE       */
        "background_music/mus_combat.ogg",         /* MUS_COMBAT        */
        "background_music/mus_menu.ogg",           /* MUS_MENU          */
        "background_music/mus_biome_mountain.ogg", /* MUS_BIOME_MTN     */
        "background_music/mus_biome_forest.ogg",   /* MUS_BIOME_FOREST  */
        "background_music/mus_biome_lava.ogg",     /* MUS_BIOME_LAVA    */
        "background_music/mus_biome_plains.ogg",   /* MUS_BIOME_PLAINS  */
    };
    for(int i=0;i<MUS_COUNT;i++){
        g_audio.music[i]=Mix_LoadMUS(mus[i]);
        if(!g_audio.music[i]){
            /* Try .wav fallback */
            char wav[64];snprintf(wav,sizeof(wav),"%.*s.wav",(int)(strlen(mus[i])-4),mus[i]);
            g_audio.music[i]=Mix_LoadMUS(wav);
        }
        if(g_audio.music[i]) printf("[Audio] Music: %s\n",mus[i]);
        else                  printf("[Audio] optional music not found: %s\n",mus[i]);
    }
}

static void audio_play(int snd,int vol){
    if(!g_audio.ok||snd<0||snd>=SND_COUNT||!g_audio.chunks[snd])return;
    int sv=(vol*g_audio.sfx_vol*g_options.master_vol)/(100*100);
    Mix_VolumeChunk(g_audio.chunks[snd],sv);
    Mix_PlayChannel(-1,g_audio.chunks[snd],0);
}

/* Count actively-playing non-music SDL channels */
static int audio_active_channels(void){
    int n=0;
    for(int c=0;c<32;c++) if(Mix_Playing(c)) n++;
    return n;
}

static void audio_play_3d(int snd,Vec3 src,Vec3 listener,int max_vol,float max_dist){
    if(!g_audio.ok||snd<0||snd>=SND_COUNT||!g_audio.chunks[snd])return;
    float d=vec3_dist(src,listener);
    if(d>max_dist)return;
    int vol=(int)(max_vol*(1.0f-d/max_dist));
    if(vol<4)return;
    /* Hard channel cap: drop low-volume sounds under heavy load */
    if(audio_active_channels()>=20&&vol<30)return;
    if(audio_active_channels()>=26&&vol<55)return;
    audio_play(snd,vol);
}

static int audio_music_mix_volume(float mod){
    float clamped_mod=clampf(mod,0.0f,1.0f);
    int base=(g_audio.music_vol*g_options.master_vol*MIX_MAX_VOLUME)/(100*100);
    return (int)(base*clamped_mod);
}

static void audio_play_music(int track,int fade_ms){
    if(!g_audio.ok||track<0||track>=MUS_COUNT)return;
    if(g_audio.cur_music==track)return;
    if(!g_audio.music[track]){g_audio.cur_music=track;return;}
    if(fade_ms>0) Mix_FadeOutMusic(fade_ms);
    else          Mix_HaltMusic();
    Mix_FadeInMusic(g_audio.music[track],-1,fade_ms>0?fade_ms:0);
    Mix_VolumeMusic(audio_music_mix_volume(1.0f));
    g_audio.cur_music=track;
    printf("[Audio] Music -> track %d\n",track);
}

/* Apply option volumes immediately (call from options page) */
static void audio_set_volumes(int music_vol,int sfx_vol){
    g_audio.music_vol=music_vol; g_audio.sfx_vol=sfx_vol;
    if(g_audio.ok) Mix_VolumeMusic(audio_music_mix_volume(1.0f));
}

static void audio_footstep(bool running,float dt){
    if(!g_audio.ok)return;
    float interval=running?0.30f:0.48f;
    g_audio.step_timer-=dt;
    if(g_audio.step_timer<=0){
        g_audio.step_timer=interval;
        audio_play(running?SND_RUN:SND_STEP,65+(rand()%15));
    }
}

/* Call once per frame — handles music transitions, biome music, ambient SFX */
static void audio_update(float dt,Vec3 player_pos,bool enemies_nearby,float day_frac,bool is_raining){
    if(!g_audio.ok)return;

    /* ── Apply options volumes live ── */
    Mix_VolumeMusic(audio_music_mix_volume(1.0f));

    /* ── Day/night determination (day_frac 0..1, 0.25=dawn, 0.75=dusk) ── */
    bool is_night=(day_frac<0.20f||day_frac>0.75f);
    /* Smooth night fraction for crossfading (0=day, 1=night) */
    float night_f=clampf(day_frac<0.5f?(0.20f-day_frac)/0.10f:(day_frac-0.75f)/0.10f,0,1);
    g_audio.is_night=is_night;

    /* ── Combat music ── */
    if(enemies_nearby){
        g_audio.combat_timer=6.0f;
        if(!g_audio.in_combat){ g_audio.in_combat=true; audio_play_music(MUS_COMBAT,1500); }
    } else {
        g_audio.combat_timer-=dt;
        if(g_audio.combat_timer<0&&g_audio.in_combat){
            g_audio.in_combat=false;
            audio_play_music(BIOME_MUSIC[g_audio.cur_biome],3000);
        }
    }

    /* ── Biome music (out of combat) ── */
    if(!g_audio.in_combat){
        g_audio.biome_change_timer-=dt;
        if(g_audio.biome_change_timer<=0){
            g_audio.biome_change_timer=4.0f;
            BiomeType b=get_biome(player_pos);
            if(b!=g_audio.cur_biome){
                g_audio.cur_biome=b;
                int track=BIOME_MUSIC[b];
                if(!g_audio.music[track]) track=MUS_EXPLORE;
                audio_play_music(track,3000);
            }
        }
    }

    /* ── Lava proximity ── */
    float lava_cx=-1600,lava_cz=-2200;
    float ld=sqrtf((player_pos.x-lava_cx)*(player_pos.x-lava_cx)+
                   (player_pos.z-lava_cz)*(player_pos.z-lava_cz));
    float lava_target=clampf(1.0f-(ld/600.0f),0,1);
    g_audio.lava_vol+=(lava_target-g_audio.lava_vol)*dt*0.5f;

    /* ── Looping wind channel (biome MOUNTAIN/SNOW/LAVA) ── */
    float wind_target=0.0f;
    if(g_audio.cur_biome==BIOME_MOUNTAIN||g_audio.cur_biome==BIOME_SNOW)
        wind_target=0.55f+(is_night?0.1f:0.0f);
    else if(g_audio.cur_biome==BIOME_LAVA)
        wind_target=0.35f;
    g_audio.wind_vol+=(wind_target-g_audio.wind_vol)*dt*0.3f;
    if(g_audio.chunks[SND_AMB_WIND]){
        int wv=(int)(g_audio.wind_vol*(g_audio.sfx_vol*MIX_MAX_VOLUME/100));
        if(wind_target>0.05f&&!g_audio.wind_loop_playing){
            Mix_Volume(AMBILOOP_WIND_CH,wv);
            Mix_PlayChannel(AMBILOOP_WIND_CH,g_audio.chunks[SND_AMB_WIND],-1);
            g_audio.wind_loop_playing=true;
        } else if(g_audio.wind_loop_playing){
            if(wind_target<0.02f){Mix_HaltChannel(AMBILOOP_WIND_CH);g_audio.wind_loop_playing=false;}
            else Mix_Volume(AMBILOOP_WIND_CH,wv);
        }
    }

    /* ── Looping lava channel ── */
    if(g_audio.chunks[SND_AMB_LAVA]){
        int lv=(int)(g_audio.lava_vol*(g_audio.sfx_vol*MIX_MAX_VOLUME/100)*0.7f);
        if(g_audio.lava_vol>0.05f&&!g_audio.lava_loop_playing){
            Mix_Volume(AMBILOOP_LAVA_CH,lv);
            Mix_PlayChannel(AMBILOOP_LAVA_CH,g_audio.chunks[SND_AMB_LAVA],-1);
            g_audio.lava_loop_playing=true;
        } else if(g_audio.lava_loop_playing){
            if(g_audio.lava_vol<0.02f){Mix_HaltChannel(AMBILOOP_LAVA_CH);g_audio.lava_loop_playing=false;}
            else Mix_Volume(AMBILOOP_LAVA_CH,lv);
        }
    }

    /* ── Looping rain channel ── */
    float rain_target=is_raining?0.75f:0.0f;
    g_audio.rain_vol+=(rain_target-g_audio.rain_vol)*dt*1.2f;
    if(g_audio.chunks[SND_LOOP_RAIN]){
        int rv=(int)(g_audio.rain_vol*(g_audio.sfx_vol*MIX_MAX_VOLUME/100));
        if(rain_target>0.05f&&!g_audio.rain_loop_playing){
            Mix_Volume(AMBILOOP_RAIN_CH,rv);
            Mix_PlayChannel(AMBILOOP_RAIN_CH,g_audio.chunks[SND_LOOP_RAIN],-1);
            g_audio.rain_loop_playing=true;
        } else if(g_audio.rain_loop_playing){
            if(rain_target<0.02f){Mix_HaltChannel(AMBILOOP_RAIN_CH);g_audio.rain_loop_playing=false;}
            else Mix_Volume(AMBILOOP_RAIN_CH,rv);
        }
    }

    /* ── Looping night ambience (crickets/frogs) ── */
    float nite_target=0.0f;
    if(g_audio.cur_biome!=BIOME_LAVA&&g_audio.cur_biome!=BIOME_SNOW&&!is_raining)
        nite_target=night_f*0.55f;
    g_audio.nite_vol+=(nite_target-g_audio.nite_vol)*dt*0.5f;
    if(g_audio.chunks[SND_LOOP_NIGHT]){
        int nv=(int)(g_audio.nite_vol*(g_audio.sfx_vol*MIX_MAX_VOLUME/100));
        if(nite_target>0.05f&&!g_audio.nite_loop_playing){
            Mix_Volume(AMBILOOP_NITE_CH,nv);
            Mix_PlayChannel(AMBILOOP_NITE_CH,g_audio.chunks[SND_LOOP_NIGHT],-1);
            g_audio.nite_loop_playing=true;
        } else if(g_audio.nite_loop_playing){
            if(nite_target<0.02f){Mix_HaltChannel(AMBILOOP_NITE_CH);g_audio.nite_loop_playing=false;}
            else Mix_Volume(AMBILOOP_NITE_CH,nv);
        }
    }

    /* ── Thunder during rain ── */
    if(is_raining){
        g_audio.thunder_timer-=dt;
        if(g_audio.thunder_timer<=0){
            audio_play(SND_AMB_THUNDER,50+(rand()%30));
            g_audio.thunder_timer=12.0f+(float)(rand()%28); /* 12-40s between claps */
        }
    } else {
        if(g_audio.thunder_timer<15.0f) g_audio.thunder_timer=15.0f; /* don't fire right after rain stops */
    }

    /* ── Waterfall proximity audio ────────────────────────────
       Find the nearest active waterfall, compute a proximity
       volume, and smoothly fade a dedicated looping channel.
       Audible range: 120 world units, full volume at ≤15 u.    */
    {
        float wf_best=1e9f;
        for(int _wi=0;_wi<g_wfalls.count;_wi++){
            Waterfall *_w=&g_wfalls.wf[_wi];
            if(!_w->active)continue;
            /* Measure to base of falls (loudest point) */
            Vec3 _base={_w->pos.x, _w->pos.y-_w->height, _w->pos.z};
            float _d=vec3_dist(player_pos,_base);
            /* Larger falls = louder: scale by sqrt(height*width) */
            float _scale=sqrtf(_w->height*_w->width)*0.12f;
            _scale=clampf(_scale,0.3f,2.0f);
            float _eff=_d/_scale;  /* effective distance */
            if(_eff<wf_best) wf_best=_eff;
        }
        float wf_target=0.0f;
        if(wf_best<120.0f) wf_target=clampf(1.0f-(wf_best-15.0f)/105.0f,0.0f,1.0f);
        wf_target=wf_target*wf_target; /* squared falloff — more natural */
        g_audio.wf_vol+=(wf_target-g_audio.wf_vol)*dt*1.8f; /* fast enough to track movement */
        if(g_audio.chunks[SND_WF_LOOP]){
            int wfv=(int)(g_audio.wf_vol*(g_audio.sfx_vol*MIX_MAX_VOLUME/100)*0.85f);
            if(g_audio.wf_vol>0.04f&&!g_audio.wf_loop_playing){
                Mix_Volume(AMBILOOP_WF_CH,wfv);
                Mix_PlayChannel(AMBILOOP_WF_CH,g_audio.chunks[SND_WF_LOOP],-1);
                g_audio.wf_loop_playing=true;
            } else if(g_audio.wf_loop_playing){
                if(g_audio.wf_vol<0.02f){
                    Mix_HaltChannel(AMBILOOP_WF_CH);
                    g_audio.wf_loop_playing=false;
                } else {
                    Mix_Volume(AMBILOOP_WF_CH,wfv);
                }
            }
        }
    }

    /* Music volume modulation near lava + waterfall (wf slightly ducks music) */
    if(!g_audio.in_combat)
        Mix_VolumeMusic(audio_music_mix_volume((1.0f-g_audio.lava_vol*0.4f)
                                               *(1.0f-g_audio.wf_vol*0.15f)));

    /* ── Ambient one-shot SFX (day/night tables) ── */
    for(int i=0;i<5;i++) if(g_audio.amb_timers[i]>0) g_audio.amb_timers[i]-=dt;

    g_audio.amb_accum+=dt;
    float trigger_interval=is_raining?14.0f:(is_night?8.0f:6.0f);
    if(g_audio.amb_accum>=trigger_interval*(0.7f+(float)(rand()%100)/300.0f)){
        g_audio.amb_accum=0;
        BiomeType b=g_audio.cur_biome;
        const AmbEntry *table=is_night?BIOME_AMB_NIGHT[b]:BIOME_AMB_DAY[b];
        float roll=(float)(rand()%1000)/1000.0f,cum=0;
        for(int i=0;i<5;i++){
            const AmbEntry *ae=&table[i];
            if(ae->snd<0) break;
            cum+=ae->weight;
            if(roll<=cum&&g_audio.amb_timers[i]<=0){
                /* Don't play looping sounds as one-shots */
                if(ae->snd!=SND_AMB_WIND&&ae->snd!=SND_AMB_LAVA){
                    /* Suppress birds during rain */
                    if(is_raining&&ae->snd==SND_AMB_BIRD) break;
                    int vol=(int)((35+(rand()%25))*g_audio.sfx_vol/100);
                    audio_play(ae->snd,vol);
                    g_audio.amb_timers[i]=ae->min_interval;
                }
                break;
            }
        }
    }
}

static void audio_shutdown(void){
    if(!g_audio.ok)return;
    Mix_HaltChannel(-1);  /* halt all channels */
    Mix_HaltMusic();
    for(int i=0;i<SND_COUNT;i++)if(g_audio.chunks[i])Mix_FreeChunk(g_audio.chunks[i]);
    for(int i=0;i<MUS_COUNT;i++)if(g_audio.music[i])Mix_FreeMusic(g_audio.music[i]);
    Mix_CloseAudio();
}



typedef enum{
    ITEM_NONE=0,
    ITEM_POTION_HP,     /* consumable — restores health */
    ITEM_POTION_MP,     /* consumable — restores mana */
    ITEM_POTION_FULL,   /* consumable — restores both */
    ITEM_SWORD_IRON,    /* weapon — +5 damage */
    ITEM_SWORD_STEEL,   /* weapon — +12 damage */
    ITEM_SWORD_MAGIC,   /* weapon — +22 damage */
    ITEM_HELM_LEATHER,  /* armor — +10 max hp */
    ITEM_HELM_IRON,     /* armor — +25 max hp */
    ITEM_CHEST_LEATHER, /* armor — +15 max hp, -10% dmg */
    ITEM_CHEST_IRON,    /* armor — +35 max hp, -20% dmg */
    ITEM_BOOTS_LEATHER, /* boots — +15% move speed */
    ITEM_FISH_RAW,      /* common fish */
    ITEM_FISH_TROUT,    /* uncommon fish */
    ITEM_FISH_GOLDEN,   /* rare fish */
    ITEM_GOLD_COIN,     /* currency — auto-collected */
    ITEM_COUNT
}ItemType;

#define SHOP_SELL_FACTOR 0.5f  /* sell back at 50% */

/* ── Bird System ────────────────────────────────────────────────
   Two-tier LOD — same pattern as the forest system:
   LOD0 (< BIRD_LOD_DIST): animated FBX via skinning shader
   LOD1 (>= BIRD_LOD_DIST): camera-facing billboard quad
   Both tiers fall back gracefully when assets are missing.       */
#define BIRD_MAX       64
#define BIRD_LOD_DIST  20.0f    /* FBX within this distance        */
#define BIRD_CULL_DIST 220.0f   /* skip entirely beyond this       */

typedef enum { BIRD_CIRCLE, BIRD_FIGURE8, BIRD_WANDER } BirdPattern;

typedef struct {
    Vec3   center;
    float  radius;
    float  altitude;
    float  speed;
    float  phase;
    float  wing_t;      /* billboard flap timer / FBX anim time */
    float  yaw;         /* heading — updated each frame         */
    BirdPattern pattern;
    bool   active;
} Bird;

typedef struct {
    Bird      birds[BIRD_MAX];
    int       count;
    /* LOD1 billboard */
    GLuint    bill_vao, bill_prog, bill_tex;
    /* LOD0 FBX */
    CharModel mesh_model;
    bool      has_fbx;
} BirdSystem;

/* Helper: compute bird world pos + heading yaw from pattern at given phase */
static Vec3 bird_pos_yaw(const Bird *b, float *out_yaw){
    Vec3 pos; float yaw=0;
    if(b->pattern==BIRD_FIGURE8){
        float t=b->phase;
        pos.x=b->center.x+sinf(t)*b->radius;
        pos.z=b->center.z+sinf(t*2.0f)*b->radius*0.5f;
        float vx=cosf(t)*b->radius;
        float vz=cosf(t*2.0f)*b->radius;
        yaw=atan2f(vx,vz);
    } else if(b->pattern==BIRD_WANDER){
        pos.x=b->center.x+sinf(b->phase*0.7f)*b->radius
                         +sinf(b->phase*1.3f)*b->radius*0.3f;
        pos.z=b->center.z+cosf(b->phase*0.8f)*b->radius
                         +cosf(b->phase*1.1f)*b->radius*0.35f;
        float vx=cosf(b->phase*0.7f)*b->radius*0.7f+cosf(b->phase*1.3f)*b->radius*0.39f;
        float vz=-sinf(b->phase*0.8f)*b->radius*0.8f-sinf(b->phase*1.1f)*b->radius*0.385f;
        yaw=atan2f(vx,vz);
    } else { /* BIRD_CIRCLE */
        pos.x=b->center.x+cosf(b->phase)*b->radius;
        pos.z=b->center.z+sinf(b->phase)*b->radius;
        yaw=b->phase+(float)M_PI*0.5f;
    }
    pos.y=b->center.y+b->altitude+sinf(b->phase*2.3f)*1.5f;
    if(out_yaw)*out_yaw=yaw;
    return pos;
}

static void bird_system_init(BirdSystem *bs,
                             const char *fbx_path,
                             const char *sprite_path){
    memset(bs,0,sizeof(*bs));

    /* LOD1 billboard shader */
    bs->bill_prog=GLH_program(
        "#version 330 core\n"
        "layout(location=0)in vec3 aPos;\n"
        "layout(location=1)in vec2 aUV;\n"
        "uniform mat4 uProj,uView;\n"
        "uniform vec3 uBirdPos;\n"
        "uniform float uScale,uBank;\n"
        "uniform float uFrame;\n"
        "out vec2 vUV;\n"
        "void main(){\n"
        "    vec3 right=vec3(uView[0][0],uView[1][0],uView[2][0]);\n"
        "    vec3 up   =vec3(uView[0][1],uView[1][1],uView[2][1]);\n"
        "    float cb=cos(uBank),sb=sin(uBank);\n"
        "    vec3 r2=right*cb+up*sb;\n"
        "    vec3 u2=-right*sb+up*cb;\n"
        "    vec3 wpos=uBirdPos+r2*aPos.x*uScale+u2*aPos.y*uScale;\n"
        "    gl_Position=uProj*uView*vec4(wpos,1.0);\n"
        "    vUV=vec2(aUV.x*0.5+uFrame*0.5,aUV.y);\n"
        "}\n",
        "#version 330 core\n"
        "in vec2 vUV;\n"
        "uniform sampler2D uTex;\n"
        "uniform bool uHasTex;\n"
        "uniform float uFade;\n"
        "out vec4 FragColor;\n"
        "void main(){\n"
        "    if(uHasTex){\n"
        "        vec4 c=texture(uTex,vUV);\n"
        "        if(c.a<0.2)discard;\n"
        "        FragColor=vec4(c.rgb,c.a*uFade);\n"
        "    } else {\n"
        "        vec2 uv=vUV*2.0-1.0;\n"
        "        float wing=abs(uv.x)-abs(uv.y)*1.5;\n"
        "        if(wing>0.0||abs(uv.y)>0.25)discard;\n"
        "        FragColor=vec4(0.08,0.06,0.06,uFade);\n"
        "    }\n"
        "}\n");

    float verts[]={-0.5f,-0.3f,0,0,1, 0.5f,-0.3f,0,1,1,
                    0.5f, 0.3f,0,1,0, -0.5f,0.3f,0,0,0};
    uint32_t idx[]={0,1,2,0,2,3};
    GLH_Attrib a[]={{0,3,GL_FLOAT,GL_FALSE,0},{1,2,GL_FLOAT,GL_FALSE,12}};
    bs->bill_vao=GLH_mesh(verts,sizeof(verts),idx,sizeof(idx),5*sizeof(float),a,2);
    bs->bill_tex=terrain_tex_load(sprite_path,0,0,0);

    /* LOD0 FBX — optional, falls back to billboard-only if missing */
    if(fbx_path&&fbx_path[0]){
        charmodel_init(&bs->mesh_model,fbx_path);
        bs->has_fbx=(bs->mesh_model.base.scene!=NULL);
        if(bs->has_fbx)
            printf("[Birds] Loaded FBX LOD0: %s\n",fbx_path);
        else
            printf("[Birds] FBX not found (%s) — billboard-only mode\n",fbx_path);
    }
}

static int bird_add(BirdSystem *bs, Vec3 center, float radius, float altitude,
                    float speed, BirdPattern pattern){
    if(bs->count>=BIRD_MAX)return -1;
    int i=bs->count++;
    Bird *b=&bs->birds[i];
    b->center=center; b->radius=radius; b->altitude=altitude;
    b->speed=speed; b->phase=(float)i*1.3f;
    b->pattern=pattern; b->active=true;
    b->wing_t=(float)i*0.4f;
    return i;
}

static void bird_system_update(BirdSystem *bs, float dt){
    for(int i=0;i<bs->count;i++){
        Bird *b=&bs->birds[i];
        if(!b->active)continue;
        b->phase+=b->speed*dt;
        b->wing_t+=dt*3.5f;
        /* keep heading current for LOD0 world matrix */
        bird_pos_yaw(b,&b->yaw);
    }
}

static void bird_system_draw(BirdSystem *bs, SkinProg *sp,
                             Mat4 proj, Mat4 view,
                             Vec3 cam, LightingSystem *ls){
    if(!bs->count)return;

    /* ── LOD0: skinned FBX ── */
    if(bs->has_fbx){
        for(int i=0;i<bs->count;i++){
            Bird *b=&bs->birds[i];
            if(!b->active)continue;
            Vec3 pos=bird_pos_yaw(b,NULL);
            float dx=pos.x-cam.x,dz=pos.z-cam.z;
            float d2=dx*dx+dz*dz;
            if(d2>BIRD_LOD_DIST*BIRD_LOD_DIST)continue;
            /* build world matrix from flight pos + heading */
            Mat4 wm=entity_mat(pos,b->yaw,
                               bs->mesh_model.base.ground_offset,
                               bs->mesh_model.base.model_scale,
                               bs->mesh_model.base.cx,
                               bs->mesh_model.base.cy,
                               bs->mesh_model.base.cz);
            /* use wing_t as anim time; ANIM_IDLE is slot 0 (or walk=1 for flap) */
            draw_charmodel(sp,&bs->mesh_model,wm,
                           ANIM_WALK,(double)b->wing_t,
                           0.0f,proj,view,cam,ls);
        }
    }

    /* ── LOD1: billboard ── */
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glUseProgram(bs->bill_prog);
    glUniformMatrix4fv(glGetUniformLocation(bs->bill_prog,"uProj"),1,GL_FALSE,proj.m);
    glUniformMatrix4fv(glGetUniformLocation(bs->bill_prog,"uView"),1,GL_FALSE,view.m);
    int has_tex=(bs->bill_tex!=0);
    if(has_tex){glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,bs->bill_tex);
                glUniform1i(glGetUniformLocation(bs->bill_prog,"uTex"),0);}
    glUniform1i(glGetUniformLocation(bs->bill_prog,"uHasTex"),has_tex);

    glBindVertexArray(bs->bill_vao);
    GLint lpos =glGetUniformLocation(bs->bill_prog,"uBirdPos");
    GLint lsc  =glGetUniformLocation(bs->bill_prog,"uScale");
    GLint lbank=glGetUniformLocation(bs->bill_prog,"uBank");
    GLint lfr  =glGetUniformLocation(bs->bill_prog,"uFrame");
    GLint lfade=glGetUniformLocation(bs->bill_prog,"uFade");

    for(int i=0;i<bs->count;i++){
        Bird *b=&bs->birds[i];
        if(!b->active)continue;

        float yaw; Vec3 pos=bird_pos_yaw(b,&yaw);
        float dx=pos.x-cam.x,dz=pos.z-cam.z;
        float d2=dx*dx+dz*dz;
        if(d2>BIRD_CULL_DIST*BIRD_CULL_DIST)continue;
        float d=sqrtf(d2);

        /* skip billboard range that LOD0 FBX covers */
        if(bs->has_fbx&&d<BIRD_LOD_DIST)continue;

        /* banking angle from pattern */
        float bank=0;
        if(b->pattern==BIRD_FIGURE8){
            float vx=cosf(b->phase)*b->radius;
            float vz=cosf(b->phase*2.0f)*b->radius;
            bank=-atan2f(vx,fabsf(vz)+0.001f)*0.4f;
        } else if(b->pattern==BIRD_WANDER){
            bank=sinf(b->phase*1.5f)*0.2f;
        } else {
            bank=-0.25f;
        }

        /* fade in from LOD0 crossover, fade out at cull distance */
        float fade=clampf((d-BIRD_LOD_DIST)/8.0f,0.0f,1.0f)
                  *clampf((BIRD_CULL_DIST-d)/30.0f,0.0f,1.0f);

        float scale=0.6f+d*0.004f;
        float frame=(sinf(b->wing_t)>0.0f)?0.0f:1.0f;

        glUniform3f(lpos,pos.x,pos.y,pos.z);
        glUniform1f(lsc,scale);
        glUniform1f(lbank,bank);
        glUniform1f(lfr,frame);
        glUniform1f(lfade,fade);
        glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,0);
    }
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}


/* ── NPC Patrol System ──────────────────────────────────────────
   Extends NpcDef with optional waypoint patrol routes.
   NPCs walk between waypoints, pause briefly at each, then move on.
   Integrates with the existing NpcSystem — just call npc_patrol_add
   after npc_add and call npc_patrol_update each frame.            */

typedef struct {
    Vec3  waypoints[NPC_WP_MAX];
    int   wp_count;
    int   wp_cur;
    float wait_timer;   /* pause at waypoint */
    float wait_dur;     /* seconds to pause  */
    float walk_speed;
    bool  patrolling;
} NpcPatrol;

#define NPC_PATROL_MAX NPC_MAX
static NpcPatrol g_npc_patrols[NPC_PATROL_MAX];

static void npc_patrol_add(int npc_idx, float speed, float wait_secs){
    if(npc_idx<0||npc_idx>=NPC_PATROL_MAX)return;
    NpcPatrol *p=&g_npc_patrols[npc_idx];
    memset(p,0,sizeof(*p));
    p->walk_speed=speed;
    p->wait_dur=wait_secs;
    p->patrolling=true;
}

static void npc_patrol_waypoint(int npc_idx, Vec3 wp){
    if(npc_idx<0||npc_idx>=NPC_PATROL_MAX)return;
    NpcPatrol *p=&g_npc_patrols[npc_idx];
    if(p->wp_count>=NPC_WP_MAX)return;
    p->waypoints[p->wp_count++]=wp;
}

#define ANIM_WALK 1
#define ANIM_IDLE 0

static void npc_patrol_update(NpcSystem *ns, float dt){
    /* Don't move NPCs who are being talked to */
    for(int i=0;i<ns->count;i++){
        if(ns->open&&ns->active_npc==i)continue;
        NpcPatrol *p=&g_npc_patrols[i];
        if(!p->patrolling||p->wp_count<2)continue;
        NpcDef *n=&ns->npcs[i];

        if(p->wait_timer>0){
            p->wait_timer-=dt;
            n->anim_idx=ANIM_IDLE;
            continue;
        }

        Vec3 target=p->waypoints[p->wp_cur];
        float dx=target.x-n->pos.x, dz=target.z-n->pos.z;
        float dist=sqrtf(dx*dx+dz*dz);

        if(dist<0.5f){
            /* reached waypoint — pause then advance */
            p->wp_cur=(p->wp_cur+1)%p->wp_count;
            p->wait_timer=p->wait_dur*(0.7f+(float)(rand()%100)/300.0f);
            n->anim_idx=ANIM_IDLE;
        } else {
            /* walk toward waypoint */
            float step=p->walk_speed*dt;
            if(step>dist)step=dist;
            n->pos.x+=dx/dist*step;
            n->pos.z+=dz/dist*step;
            n->pos.y=terrain_h(n->pos.x,n->pos.z)+0.05f;
            n->yaw=atan2f(dx,dz);
            n->anim_idx=ANIM_WALK;
            n->anim_time+=dt;
        }
    }
}

/* ── Forest System ──────────────────────────────────────────────
   Two-tier LOD forest.
   LOD0 (< FOREST_MESH_DIST): full FBX prop — uses existing PropList
   LOD1 (>= FOREST_MESH_DIST): single camera-facing billboard quad
   Each tree is registered once; the system decides at draw time.   */
#define FOREST_MAX       512
#define FOREST_MESH_DIST  80.0f   /* switch to billboard beyond this */
#define FOREST_EDITOR_FILE "forest_custom.txt"

typedef struct {
    Vec3   pos;
    float  scale;
    float  yaw;
    int    prop_idx;    /* index into g_props, -1 if not spawned yet  */
    bool   active;
} ForestTree;

typedef struct ForestSystem {
    ForestTree trees[FOREST_MAX];
    int        count;
    /* Billboard VAO */
    GLuint     bill_vao;
    GLuint     bill_prog;
    GLuint     bill_tex;       /* normal tree billboard sprite             */
    GLuint     bill_tex_snow;  /* snow-capped variant, used above snowline */
    /* FBX path to use for close trees */
    char       fbx_path[128];
} ForestSystem;

static ForestSystem g_forest;
static BirdSystem   g_birds;

/* ═══════════════════════════════════════════════════════════════
   ANIMAL SYSTEM
   Ground-clamped herds with graze/wander/flee states.
   Each AnimalHerd has one FBX model shared across all members.
   ═══════════════════════════════════════════════════════════════ */
#define ANIMAL_MAX_HERDS   8
#define ANIMAL_MAX_PER_HERD 16
#define ANIMAL_LOD_DIST    80.0f   /* FBX → billboard crossover  */
#define ANIMAL_CULL_DIST  200.0f   /* beyond this: not drawn     */
#define ANIMAL_FLEE_DIST   12.0f   /* player distance that triggers flee */

typedef enum {
    AS_GRAZE=0,   /* standing/eating, slow head bob */
    AS_WANDER,    /* walking toward next wander target */
    AS_FLEE,      /* running away from player */
    AS_IDLE,      /* standing still briefly */
    AS_CHASE,     /* hostile: running toward player */
    AS_ATTACK,    /* hostile: biting/lunging at player */
} AnimalState;

typedef struct {
    Vec3        pos;
    float       yaw;          /* heading in radians */
    AnimalState state;
    float       state_timer;  /* seconds until next state change */
    Vec3        wander_target;/* current destination when wandering */
    float       anim_time;
    float       attack_timer; /* cooldown between hits */
    bool        active;
} Animal;

typedef struct {
    Animal      animals[ANIMAL_MAX_PER_HERD];
    int         count;
    CharModel   model;
    bool        has_fbx;
    /* herd home — animals wander within home_radius of home */
    Vec3        home;
    float       home_radius;
    /* species parameters */
    float       walk_speed;   /* m/s wandering */
    float       flee_speed;   /* m/s fleeing    */
    float       scale;        /* render scale   */
    /* hostile parameters (hostile=false → passive/fleeing) */
    bool        hostile;
    float       aggro_dist;   /* distance to start chasing  */
    float       attack_dist;  /* distance to start hitting  */
    float       attack_damage;/* HP per hit                 */
    float       attack_cd;    /* seconds between hits       */
    float       chase_speed;  /* m/s while chasing          */
    /* billboard fallback (simple coloured quad) */
    GLuint      bill_vao, bill_prog;
    float       bill_r,bill_g,bill_b; /* tint for fallback billboard */
    char        name[32];
} AnimalHerd;

typedef struct {
    AnimalHerd herds[ANIMAL_MAX_HERDS];
    int        count;
    float      pending_damage; /* accumulated player damage this frame */
} AnimalSystem;

static AnimalSystem g_animals;

/* ── Init one herd ──────────────────────────────────────────── */
static void animal_herd_init(AnimalHerd *h,
                             const char *fbx_path,
                             const char *species_name,
                             Vec3 home, float home_radius,
                             int num_animals,
                             float walk_speed, float flee_speed,
                             float scale,
                             float bill_r, float bill_g, float bill_b){
    memset(h,0,sizeof(*h));
    strncpy(h->name,species_name,31);
    h->home=home; h->home_radius=home_radius;
    h->walk_speed=walk_speed; h->flee_speed=flee_speed;
    h->scale=scale;
    h->bill_r=bill_r; h->bill_g=bill_g; h->bill_b=bill_b;

    /* Load FBX */
    charmodel_init(&h->model, fbx_path);
    h->has_fbx=(h->model.base.scene!=NULL);
    printf("[Animal] %s: FBX %s\n", species_name,
           h->has_fbx?"loaded":"not found (billboard fallback)");

    /* Billboard fallback shader — simple coloured quad facing camera */
    h->bill_prog=GLH_program(
        "#version 330 core\n"
        "layout(location=0)in vec3 aPos;\n"
        "layout(location=1)in vec2 aUV;\n"
        "uniform mat4 uProj,uView;\n"
        "uniform vec3 uPos;\n"
        "uniform float uScale;\n"
        "out vec2 vUV;\n"
        "void main(){\n"
        "    vec3 right=vec3(uView[0][0],uView[1][0],uView[2][0]);\n"
        "    vec3 up   =vec3(0,1,0);\n"  /* world-up billboard — no tilt */
        "    vec3 wpos=uPos+right*aPos.x*uScale+up*aPos.y*uScale;\n"
        "    gl_Position=uProj*uView*vec4(wpos,1.0);\n"
        "    vUV=aUV;}\n",
        "#version 330 core\n"
        "in vec2 vUV;\n"
        "uniform vec3 uColor;\n"
        "uniform float uFade;\n"
        "out vec4 FragColor;\n"
        "void main(){\n"
        "    vec2 uv=vUV*2.0-1.0;\n"
        "    if(length(uv)>1.0)discard;\n"   /* rough oval silhouette */
        "    FragColor=vec4(uColor,uFade);}\n");

    float verts[]={-0.5f,0.0f,0,0,0, 0.5f,0.0f,0,1,0,
                    0.5f,1.0f,0,1,1, -0.5f,1.0f,0,0,1};
    uint32_t idx[]={0,1,2,0,2,3};
    GLH_Attrib a[]={{0,3,GL_FLOAT,GL_FALSE,0},{1,2,GL_FLOAT,GL_FALSE,12}};
    h->bill_vao=GLH_mesh(verts,sizeof(verts),idx,sizeof(idx),
                         5*sizeof(float),a,2);

    /* Spawn animals scattered around home */
    int n=num_animals<ANIMAL_MAX_PER_HERD?num_animals:ANIMAL_MAX_PER_HERD;
    h->count=n;
    for(int i=0;i<n;i++){
        Animal *a2=&h->animals[i];
        float ang=(float)i/(float)n*2.0f*(float)M_PI;
        float r=home_radius*0.5f*(0.4f+(float)rand()/RAND_MAX*0.6f);
        a2->pos.x=home.x+cosf(ang)*r;
        a2->pos.z=home.z+sinf(ang)*r;
        a2->pos.y=terrain_h(a2->pos.x,a2->pos.z);
        a2->yaw=ang+(float)M_PI;
        a2->state=AS_GRAZE;
        a2->state_timer=2.0f+(float)rand()/RAND_MAX*4.0f;
        a2->wander_target=a2->pos;
        a2->anim_time=(float)i*0.37f; /* stagger animations */
        a2->active=true;
    }
}

/* ── Add a herd to the system ─────────────────────────────── */
static AnimalHerd* animal_add_herd(AnimalSystem *as,
                                   const char *fbx,
                                   const char *name,
                                   Vec3 home, float radius,
                                   int count,
                                   float walk_spd, float flee_spd,
                                   float scale,
                                   float r,float g,float b){
    if(as->count>=ANIMAL_MAX_HERDS)return NULL;
    AnimalHerd *h=&as->herds[as->count++];
    animal_herd_init(h,fbx,name,home,radius,count,
                     walk_spd,flee_spd,scale,r,g,b);
    return h;
}

/* ── Make a herd hostile ──────────────────────────────────── */
static void animal_set_hostile(AnimalHerd *h,
                                float aggro_dist,
                                float attack_dist,
                                float chase_speed,
                                float damage,
                                float attack_cd){
    if(!h)return;
    h->hostile=true;
    h->aggro_dist=aggro_dist;
    h->attack_dist=attack_dist;
    h->chase_speed=chase_speed;
    h->attack_damage=damage;
    h->attack_cd=attack_cd;
}

/* ── Per-frame update ─────────────────────────────────────── */
static void animal_system_update(AnimalSystem *as, float dt, Vec3 player_pos){
    as->pending_damage=0.0f;
    for(int hi=0;hi<as->count;hi++){
        AnimalHerd *h=&as->herds[hi];
        for(int i=0;i<h->count;i++){
            Animal *a=&h->animals[i];
            if(!a->active)continue;

            /* Always clamp to terrain */
            a->pos.y=terrain_h(a->pos.x,a->pos.z);

            float pdx=a->pos.x-player_pos.x;
            float pdz=a->pos.z-player_pos.z;
            float pdist=sqrtf(pdx*pdx+pdz*pdz);

            /* tick attack cooldown */
            if(a->attack_timer>0) a->attack_timer-=dt;

            /* ── Aggro / flee trigger ── */
            if(h->hostile){
                /* Hostile: aggro when player enters aggro_dist */
                if(pdist<h->aggro_dist &&
                   a->state!=AS_CHASE && a->state!=AS_ATTACK){
                    a->state=AS_CHASE;
                    a->state_timer=8.0f; /* chase for up to 8s */
                }
            } else {
                /* Passive: flee when player too close */
                if(pdist<ANIMAL_FLEE_DIST &&
                   a->state!=AS_FLEE){
                    a->state=AS_FLEE;
                    a->state_timer=4.0f+((float)rand()/RAND_MAX)*2.0f;
                }
            }

            a->state_timer-=dt;

            switch(a->state){

            /* ── HOSTILE STATES ─────────────────────────── */
            case AS_CHASE:{
                float nx=(pdist>0.001f)?(-pdx/pdist):0.0f;
                float nz=(pdist>0.001f)?(-pdz/pdist):0.0f;
                /* Move toward player */
                a->pos.x+=nx*h->chase_speed*dt;
                a->pos.z+=nz*h->chase_speed*dt;
                a->yaw=atan2f(nx,-nz)+(float)M_PI;
                a->anim_time+=dt*2.0f;
                /* Switch to attack when close enough */
                if(pdist<h->attack_dist){
                    a->state=AS_ATTACK;
                    a->state_timer=1.2f;
                }
                /* Give up chase if timer expires or player too far */
                if(a->state_timer<=0||pdist>h->aggro_dist*1.8f){
                    a->state=AS_WANDER;
                    a->state_timer=3.0f;
                    float ang=((float)rand()/RAND_MAX)*2.0f*(float)M_PI;
                    float rv=((float)rand()/RAND_MAX)*h->home_radius;
                    a->wander_target.x=h->home.x+cosf(ang)*rv;
                    a->wander_target.z=h->home.z+sinf(ang)*rv;
                }
            } break;

            case AS_ATTACK:{
                /* Face player, lunge slightly */
                float nx=(pdist>0.001f)?(-pdx/pdist):0.0f;
                float nz=(pdist>0.001f)?(-pdz/pdist):0.0f;
                a->yaw=atan2f(nx,-nz)+(float)M_PI;
                /* Lunge forward during attack */
                a->pos.x+=nx*h->chase_speed*0.4f*dt;
                a->pos.z+=nz*h->chase_speed*0.4f*dt;
                a->anim_time+=dt*1.8f; /* attack anim */
                /* Deal damage on cooldown */
                if(a->attack_timer<=0 && pdist<h->attack_dist*1.2f){
                    as->pending_damage+=h->attack_damage;
                    a->attack_timer=h->attack_cd;
                }
                /* Return to chase if player moves away */
                if(pdist>h->attack_dist*1.5f){
                    a->state=AS_CHASE;
                    a->state_timer=5.0f;
                } else if(a->state_timer<=0){
                    /* Brief pause then re-enter chase */
                    a->state=AS_CHASE;
                    a->state_timer=4.0f;
                }
            } break;

            /* ── PASSIVE STATES ─────────────────────────── */
            case AS_FLEE:{
                float fx=(pdist>0.001f)?(pdx/pdist):1.0f;
                float fz=(pdist>0.001f)?(pdz/pdist):0.0f;
                a->pos.x+=fx*h->flee_speed*dt;
                a->pos.z+=fz*h->flee_speed*dt;
                a->yaw=atan2f(fx,-fz)+(float)M_PI;
                /* Clamp to home radius */
                float hx=a->pos.x-h->home.x, hz=a->pos.z-h->home.z;
                float hd=sqrtf(hx*hx+hz*hz);
                if(hd>h->home_radius){
                    a->pos.x=h->home.x+(hx/hd)*h->home_radius;
                    a->pos.z=h->home.z+(hz/hd)*h->home_radius;
                }
                a->anim_time+=dt*2.2f;
                if(a->state_timer<=0||pdist>ANIMAL_FLEE_DIST*2.5f){
                    a->state=AS_GRAZE;
                    a->state_timer=3.0f+((float)rand()/RAND_MAX)*5.0f;
                }
            } break;

            case AS_WANDER:{
                float tx=a->wander_target.x-a->pos.x;
                float tz=a->wander_target.z-a->pos.z;
                float td=sqrtf(tx*tx+tz*tz);
                if(td<0.6f||a->state_timer<=0){
                    a->state=AS_GRAZE;
                    a->state_timer=2.0f+((float)rand()/RAND_MAX)*6.0f;
                } else {
                    float nx=tx/td, nz=tz/td;
                    a->pos.x+=nx*h->walk_speed*dt;
                    a->pos.z+=nz*h->walk_speed*dt;
                    a->yaw=atan2f(nx,-nz)+(float)M_PI;
                    a->anim_time+=dt*1.2f;
                }
            } break;

            case AS_IDLE:
                a->anim_time+=dt*0.3f;
                if(a->state_timer<=0){
                    a->state=(((int)(a->anim_time*10))%3==0)?AS_WANDER:AS_GRAZE;
                    a->state_timer=2.0f+((float)rand()/RAND_MAX)*4.0f;
                    if(a->state==AS_WANDER){
                        float ang=((float)rand()/RAND_MAX)*2.0f*(float)M_PI;
                        float rv=((float)rand()/RAND_MAX)*h->home_radius;
                        a->wander_target.x=h->home.x+cosf(ang)*rv;
                        a->wander_target.z=h->home.z+sinf(ang)*rv;
                    }
                }
                break;

            case AS_GRAZE:
            default:
                a->anim_time+=dt*0.5f;
                if(a->state_timer<=0){
                    float roll=(float)rand()/RAND_MAX;
                    if(roll<0.40f){
                        a->state=AS_WANDER;
                        a->state_timer=3.0f+((float)rand()/RAND_MAX)*5.0f;
                        float ang=((float)rand()/RAND_MAX)*2.0f*(float)M_PI;
                        float rv=((float)rand()/RAND_MAX)*h->home_radius;
                        a->wander_target.x=h->home.x+cosf(ang)*rv;
                        a->wander_target.z=h->home.z+sinf(ang)*rv;
                    } else if(roll<0.60f){
                        a->state=AS_IDLE;
                        a->state_timer=1.5f+((float)rand()/RAND_MAX)*3.0f;
                    } else {
                        a->state_timer=3.0f+((float)rand()/RAND_MAX)*6.0f;
                    }
                }
                break;
            }
        }
    }
}

/* ── Pick animation slot based on state ─────────────────────── */
static int animal_anim_slot(AnimalState s){
    switch(s){
    case AS_FLEE:
    case AS_CHASE:   return ANIM_RUN;
    case AS_WANDER:  return ANIM_WALK;
    case AS_ATTACK:  return ANIM_ATTACK;
    default:         return ANIM_IDLE;
    }
}

/* ── Draw all herds ─────────────────────────────────────────── */
static void animal_system_draw(AnimalSystem *as,
                               SkinProg *sp,
                               Mat4 proj, Mat4 view,
                               Vec3 cam, LightingSystem *ls){
    for(int hi=0;hi<as->count;hi++){
        AnimalHerd *h=&as->herds[hi];

        /* ── LOD0: skinned FBX ── */
        if(h->has_fbx){
            for(int i=0;i<h->count;i++){
                Animal *a=&h->animals[i];
                if(!a->active)continue;
                float dx=a->pos.x-cam.x, dz=a->pos.z-cam.z;
                float d2=dx*dx+dz*dz;
                if(d2>ANIMAL_LOD_DIST*ANIMAL_LOD_DIST)continue;
                Mat4 wm=entity_mat(a->pos,a->yaw,
                                   h->model.base.ground_offset,
                                   h->scale*h->model.base.model_scale,
                                   h->model.base.cx,
                                   h->model.base.cy,
                                   h->model.base.cz);
                draw_charmodel(sp,&h->model,wm,
                               animal_anim_slot(a->state),
                               (double)a->anim_time,
                               0.0f,proj,view,cam,ls);
            }
        }

        /* ── LOD1: billboard fallback ── */
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);glDepthMask(GL_FALSE);
        glUseProgram(h->bill_prog);
        glUniformMatrix4fv(glGetUniformLocation(h->bill_prog,"uProj"),1,GL_FALSE,proj.m);
        glUniformMatrix4fv(glGetUniformLocation(h->bill_prog,"uView"),1,GL_FALSE,view.m);
        glUniform3f(glGetUniformLocation(h->bill_prog,"uColor"),h->bill_r,h->bill_g,h->bill_b);
        glBindVertexArray(h->bill_vao);

        for(int i=0;i<h->count;i++){
            Animal *a=&h->animals[i];
            if(!a->active)continue;
            float dx=a->pos.x-cam.x, dz=a->pos.z-cam.z;
            float d2=dx*dx+dz*dz;
            if(d2>ANIMAL_CULL_DIST*ANIMAL_CULL_DIST)continue;
            float d=sqrtf(d2);
            if(h->has_fbx&&d<ANIMAL_LOD_DIST)continue;
            float fade=clampf((d-ANIMAL_LOD_DIST)/15.0f,0.0f,1.0f)
                      *clampf((ANIMAL_CULL_DIST-d)/40.0f,0.0f,1.0f);
            glUniform3f(glGetUniformLocation(h->bill_prog,"uPos"),a->pos.x,a->pos.y,a->pos.z);
            glUniform1f(glGetUniformLocation(h->bill_prog,"uScale"),h->scale*1.2f);
            glUniform1f(glGetUniformLocation(h->bill_prog,"uFade"),fade);
            glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,0);
        }
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
        glDisable(GL_BLEND);
    }
}
/* ═══════════════════════════════════════════════════════════════
   END ANIMAL SYSTEM
   ═══════════════════════════════════════════════════════════════ */

static void forest_init(ForestSystem *fs, const char *tree_fbx, const char *billboard_png){
    memset(fs,0,sizeof(*fs));
    strncpy(fs->fbx_path,tree_fbx,127);

    fs->bill_prog=GLH_program(
        "#version 330 core\n"
        "layout(location=0)in vec3 aPos;\n"
        "layout(location=1)in vec2 aUV;\n"
        "uniform mat4 uProj,uView;\n"
        "uniform vec3 uTreePos,uCamPos;\n"
        "uniform float uScale;\n"
        "out vec2 vUV;\n"
        "out float vFade;\n"
        "void main(){\n"
        /* Y-axis billboard: use camera right, keep world up */
        "    vec3 right=normalize(vec3(uView[0][0],uView[1][0],uView[2][0]));\n"
        "    vec3 up=vec3(0,1,0);\n"
        "    vec3 wpos=uTreePos + right*aPos.x*uScale + up*aPos.y*uScale;\n"
        "    gl_Position=uProj*uView*vec4(wpos,1.0);\n"
        "    vUV=aUV;\n"
        /* fade in beyond mesh LOD distance; uCamPos is actual world camera pos */
        "    float dist=length(uTreePos-uCamPos);\n"
        "    vFade=clamp((dist-60.0)/30.0,0.0,1.0);\n"
        "}\n",
        "#version 330 core\n"
        "in vec2 vUV;\nin float vFade;\n"
        "uniform sampler2D uTex;\n"
        "uniform bool uHasTex;\n"
        "uniform float uSnowTint;\n"   /* 0=normal, 1=fully snow-tinted */
        "uniform vec3 uSunColor,uAmbient;\n"
        "out vec4 FragColor;\n"
        "void main(){\n"
        "    vec4 c;\n"
        "    if(uHasTex){\n"
        "        c=texture(uTex,vUV);\n"
        "        if(c.a<0.15)discard;\n"
        "        /* Tint toward white/grey when snow PNG not available */\n"
        "        c.rgb=mix(c.rgb,vec3(0.88,0.93,1.0),uSnowTint*0.72);\n"
        "    } else {\n"
        /* procedural green oval fallback, with snow tint on crown */
        "        vec2 uv=vUV*2.0-1.0;\n"
        "        float trunk=clamp(1.0-abs(uv.x)*8.0,0.0,1.0)*clamp(uv.y+1.0,0.0,1.0)*clamp(-uv.y+0.1,0.0,1.0);\n"
        "        float crown=clamp(1.0-length(uv+vec2(0,0.2))*1.4,0.0,1.0);\n"
        "        if(trunk+crown<0.05)discard;\n"
        "        vec3 base_c=mix(vec3(0.22,0.14,0.06),vec3(0.12,0.38,0.08),crown);\n"
        "        base_c=mix(base_c,vec3(0.88,0.93,1.0),uSnowTint*crown);\n"
        "        c=vec4(base_c,1.0);\n"
        "    }\n"
        "    vec3 lit=c.rgb*(uAmbient+uSunColor*0.6);\n"
        "    FragColor=vec4(lit,c.a*vFade);\n"
        "}\n");

    /* Quad: roots at Y=0, top at Y=1, ±0.5 wide */
    float qv[]={-0.5f,0,0, 0,1,  0.5f,0,0, 1,1,  0.5f,1,0, 1,0,  -0.5f,1,0, 0,0};
    uint32_t qi[]={0,1,2,0,2,3};
    GLH_Attrib qa[]={{0,3,GL_FLOAT,GL_FALSE,0},{1,2,GL_FLOAT,GL_FALSE,12}};
    fs->bill_vao=GLH_mesh(qv,sizeof(qv),qi,sizeof(qi),5*sizeof(float),qa,2);
    fs->bill_tex=terrain_tex_load(billboard_png,0,0,0);
    /* Snow variant — optional, falls back to normal sprite if not present */
    fs->bill_tex_snow=terrain_tex_load("tree_billboard_snow.png",0,0,0);
    if(fs->bill_tex_snow)
        printf("[Forest] Loaded tree_billboard_snow.png\n");
    else
        printf("[Forest] tree_billboard_snow.png not found — will tint normal sprite\n");
}

static void forest_add_tree(ForestSystem *fs, Vec3 pos, float scale, float yaw){
    if(fs->count>=FOREST_MAX)return;
    Vec3 dry=pos;
    if(!find_nearby_dry_ground(pos,18.0f,&dry)) return;
    ForestTree *t=&fs->trees[fs->count++];
    t->pos=dry; t->scale=scale; t->yaw=yaw;
    t->prop_idx=-1; t->active=true;
}

/* Place a ring of trees — convenience helper */
static void forest_ring(ForestSystem *fs, Vec3 center, float radius,
                        int count, float scale_min, float scale_max){
    for(int i=0;i<count;i++){
        float a=(float)i/(float)count*(float)(2*M_PI);
        float r=radius*(0.85f+(float)(rand()%100)*0.003f);
        float jx=((float)(rand()%100)-50)*0.01f*radius*0.15f;
        float jz=((float)(rand()%100)-50)*0.01f*radius*0.15f;
        Vec3 p={center.x+cosf(a)*r+jx, 0,
                center.z+sinf(a)*r+jz};
        p.y=terrain_h(p.x,p.z);
        float sc=scale_min+(scale_max-scale_min)*(float)(rand()%100)/100.0f;
        float yaw=(float)(rand()%628)/100.0f;
        forest_add_tree(fs,p,sc,yaw);
    }
}

/* Fill a rectangle with random trees */
static void forest_fill_rect(ForestSystem *fs,
                              float x0,float z0,float x1,float z1,
                              int count, float scale_min,float scale_max){
    for(int i=0;i<count&&fs->count<FOREST_MAX;i++){
        float px=x0+(x1-x0)*(float)(rand()%1000)/1000.0f;
        float pz=z0+(z1-z0)*(float)(rand()%1000)/1000.0f;
        Vec3 p={px,terrain_h(px,pz),pz};
        float sc=scale_min+(scale_max-scale_min)*(float)(rand()%100)/100.0f;
        float yaw=(float)(rand()%628)/100.0f;
        forest_add_tree(fs,p,sc,yaw);
    }
}

static void forest_remove_tree(ForestSystem *fs, PropList *props, int idx){
    if(idx<0||idx>=fs->count)return;
    ForestTree *t=&fs->trees[idx];
    if(t->prop_idx>=0&&props&&t->prop_idx<props->count){
        Prop *p=&props->list[t->prop_idx];
        if(p->active){
            if(p->stream_state==PROP_LOADED) prop_unload_gpu(p);
            p->active=false;
        }
    }
    fs->trees[idx]=fs->trees[fs->count-1];
    fs->count--;
}

static void forest_clear(ForestSystem *fs, PropList *props){
    while(fs->count>0) forest_remove_tree(fs,props,fs->count-1);
}

static bool forest_is_too_close(ForestSystem *fs,float x,float z,float min_dist,int ignore_idx){
    float md2=min_dist*min_dist;
    for(int i=0;i<fs->count;i++){
        if(i==ignore_idx)continue;
        ForestTree *t=&fs->trees[i];
        if(!t->active)continue;
        float dx=t->pos.x-x,dz=t->pos.z-z;
        if(dx*dx+dz*dz<md2) return true;
    }
    return false;
}

static bool forest_is_suitable_ground(float x,float z){
    float h=terrain_h(x,z);
    if(h<-1.0f) return false;
    float hx=terrain_h(x+2.0f,z)-terrain_h(x-2.0f,z);
    float hz=terrain_h(x,z+2.0f)-terrain_h(x,z-2.0f);
    float slope=sqrtf(hx*hx+hz*hz);
    return slope<5.5f;
}

static void forest_brush_paint(ForestSystem *fs,Vec3 center,float radius,int density,
                               float scale_min,float scale_max){
    for(int i=0;i<density&&fs->count<FOREST_MAX;i++){
        float ang=((float)rand()/(float)RAND_MAX)*(float)(2*M_PI);
        float rad=sqrtf((float)rand()/(float)RAND_MAX)*radius;
        float px=center.x+cosf(ang)*rad;
        float pz=center.z+sinf(ang)*rad;
        if(!forest_is_suitable_ground(px,pz)) continue;
        if(forest_is_too_close(fs,px,pz,6.0f,-1)) continue;
        float py=terrain_h(px,pz);
        float sc=scale_min+((float)rand()/(float)RAND_MAX)*(scale_max-scale_min);
        float yaw=((float)rand()/(float)RAND_MAX)*(float)(2*M_PI);
        forest_add_tree(fs,(Vec3){px,py,pz},sc,yaw);
    }
}

static void forest_brush_erase(ForestSystem *fs,PropList *props,Vec3 center,float radius){
    float r2=radius*radius;
    for(int i=fs->count-1;i>=0;i--){
        ForestTree *t=&fs->trees[i];
        float dx=t->pos.x-center.x,dz=t->pos.z-center.z;
        if(dx*dx+dz*dz<=r2) forest_remove_tree(fs,props,i);
    }
}

static void forest_save(ForestSystem *fs){
    FILE *f=fopen(FOREST_EDITOR_FILE,"w");
    if(!f)return;
    fprintf(f,"# x y z scale yaw\n");
    int saved=0;
    for(int i=0;i<fs->count;i++){
        ForestTree *t=&fs->trees[i];
        if(!t->active)continue;
        fprintf(f,"%.3f %.3f %.3f %.3f %.3f\n",
                t->pos.x,t->pos.y,t->pos.z,t->scale,t->yaw);
        saved++;
    }
    fclose(f);
    printf("[ForestEd] Saved %d trees to %s\n",saved,FOREST_EDITOR_FILE);
}

static bool forest_load(ForestSystem *fs,PropList *props){
    FILE *f=fopen(FOREST_EDITOR_FILE,"r");
    if(!f)return false;
    forest_clear(fs,props);
    char line[256];
    int loaded=0;
    while(fgets(line,sizeof(line),f)&&fs->count<FOREST_MAX){
        if(line[0]=='#')continue;
        float x,y,z,scale,yaw;
        if(sscanf(line,"%f %f %f %f %f",&x,&y,&z,&scale,&yaw)==5){
            if(forest_is_too_close(fs,x,z,2.0f,-1)) continue;
            forest_add_tree(fs,(Vec3){x,y,z},scale,yaw);
            loaded++;
        }
    }
    fclose(f);
    printf("[ForestEd] Loaded %d trees from %s\n",loaded,FOREST_EDITOR_FILE);
    return true;
}

static void forest_draw(ForestSystem *fs, PropList *props,
                        LightingSystem *ls, Mat4 proj, Mat4 view, Vec3 cam,
                        float snow_accum){
    /* Camera world pos from inverse view */
    Vec3 cam_w=cam;

    /* Sort not needed — alpha-blend handled by depth test + alpha discard */
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);

    glUseProgram(fs->bill_prog);
    glUniformMatrix4fv(glGetUniformLocation(fs->bill_prog,"uProj"),1,GL_FALSE,proj.m);
    glUniformMatrix4fv(glGetUniformLocation(fs->bill_prog,"uView"),1,GL_FALSE,view.m);
    glUniform3f(glGetUniformLocation(fs->bill_prog,"uCamPos"),cam.x,cam.y,cam.z);
    glUniform3f(glGetUniformLocation(fs->bill_prog,"uSunColor"),
                ls->sun_color.x,ls->sun_color.y,ls->sun_color.z);
    glUniform3f(glGetUniformLocation(fs->bill_prog,"uAmbient"),
                ls->ambient.x,ls->ambient.y,ls->ambient.z);
    if(fs->bill_tex){
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,fs->bill_tex);
        glUniform1i(glGetUniformLocation(fs->bill_prog,"uTex"),0);
        glUniform1i(glGetUniformLocation(fs->bill_prog,"uHasTex"),1);
    } else {
        glUniform1i(glGetUniformLocation(fs->bill_prog,"uHasTex"),0);
    }
    /* Snow baseline matches terrain shader: mix(55,-4,snow_accum) */
    float snow_base = 55.0f + ((-4.0f)-55.0f)*snow_accum; /* = mix(55,-4,t) */
    float snow_fade = 20.0f;   /* blend zone height */
    GLint loc_snow = glGetUniformLocation(fs->bill_prog,"uSnowTint");
    GLint loc_pos  = glGetUniformLocation(fs->bill_prog,"uTreePos");
    GLint loc_sc   = glGetUniformLocation(fs->bill_prog,"uScale");
    glBindVertexArray(fs->bill_vao);

    /* Track currently bound texture to avoid redundant rebinds */
    GLuint cur_tex = fs->bill_tex;

    for(int i=0;i<fs->count;i++){
        ForestTree *t=&fs->trees[i];
        if(!t->active)continue;

        float dx=t->pos.x-cam_w.x, dz=t->pos.z-cam_w.z;
        float dist2=dx*dx+dz*dz;

        if(dist2 < FOREST_MESH_DIST*FOREST_MESH_DIST){
            /* LOD0: ensure FBX prop is registered (streaming system loads it async) */
            if(t->prop_idx<0){
                t->prop_idx=props->count;
                prop_register(props,fs->fbx_path,
                           t->pos.x,t->pos.y,t->pos.z,
                           0,t->yaw*57.295f,0,
                           t->scale,
                           PROP_COLL_NONE,
                           0,0,0,0);
            }
        } else {
            /* LOD1: draw billboard */
            if(dist2 > 600.0f*600.0f) continue;

            /* Determine if this tree is in the snow zone */
            float elev    = t->pos.y;
            float snow_t  = clampf((elev - snow_base) / snow_fade, 0.0f, 1.0f);
            bool  is_snow = (snow_t > 0.05f);

            /* Swap texture only when crossing normal↔snow boundary */
            GLuint want_tex = (is_snow && fs->bill_tex_snow) ? fs->bill_tex_snow : fs->bill_tex;
            if(want_tex && want_tex != cur_tex){
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D,want_tex);
                cur_tex = want_tex;
                glUniform1i(glGetUniformLocation(fs->bill_prog,"uHasTex"), 1);
            } else if(!want_tex && cur_tex){
                glUniform1i(glGetUniformLocation(fs->bill_prog,"uHasTex"), 0);
                cur_tex = 0;
            }

            /* Snow tint: used when no snow PNG (blends toward white) */
            float tint = (is_snow && !fs->bill_tex_snow) ? snow_t : 0.0f;
            glUniform1f(loc_snow, tint);

            float sc=t->scale*5.5f;
            glUniform3f(loc_pos,t->pos.x,t->pos.y,t->pos.z);
            glUniform1f(loc_sc,sc);
            glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,0);
        }
    }
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}

static void forest_hud(HUD *h, ForestSystem *fs, float brush_radius, int brush_density,
                       int hovered_idx, int win_w, int win_h){
    float W=(float)win_w, H=(float)win_h;
    float pw=260.0f, lh=18.0f, px=W-pw-6.0f, py=8.0f;
    hud_rect(h,px-4,py-4,pw+8,lh*5+12, 0,0,0,0.72f);
    hud_str(h,px,py,1.3f,"[F10 FOREST BRUSH]",0.3f,0.95f,0.45f,1.0f); py+=lh+4.0f;

    char buf[96];
    snprintf(buf,sizeof(buf),"Trees: %d / %d",fs->count,FOREST_MAX);
    hud_str(h,px+4,py,1.2f,buf,0.9f,0.9f,0.8f,1.0f); py+=lh;
    snprintf(buf,sizeof(buf),"Radius: %.0f",brush_radius);
    hud_str(h,px+4,py,1.2f,buf,0.9f,0.9f,0.8f,1.0f); py+=lh;
    snprintf(buf,sizeof(buf),"Density: %d",brush_density);
    hud_str(h,px+4,py,1.2f,buf,0.9f,0.9f,0.8f,1.0f); py+=lh;
    if(hovered_idx>=0){
        snprintf(buf,sizeof(buf),"Hover: tree %d",hovered_idx);
        hud_str(h,px+4,py,1.2f,buf,0.6f,1.0f,0.6f,1.0f);
    } else {
        hud_str(h,px+4,py,1.2f,"Hover: none",0.5f,0.5f,0.5f,1.0f);
    }

    float by=H-8.0f,blh=17.0f;
    hud_rect(h,0,by-blh*3-4,W,blh*3+8, 0,0,0,0.68f);
    hud_str(h,8,by-=blh,1.3f,"LMB=Paint trees  RMB=Erase trees  S=Save",
            0.9f,0.9f,0.6f,1.0f);
    hud_str(h,8,by-=blh,1.2f,"Wheel=Radius  Shift+Wheel=Density",
            0.8f,0.8f,0.8f,1.0f);
    hud_str(h,8,by-=blh,1.15f,"Uses the dedicated forest LOD system, not generic props.",
            0.5f,0.8f,1.0f,0.9f);

    hud_flush(h);
}

/* ── Shop UI ──────────────────────────────────────────────────── */
#define SHOP_STOCK_MAX 12
typedef struct{
    ItemType items[SHOP_STOCK_MAX];
    int      stock[SHOP_STOCK_MAX]; /* -1 = unlimited */
    int      count;
}ShopDef;

typedef struct ShopUI_s{
    bool  open;
    int   tab;         /* 0=buy  1=sell */
    int   hover;
    float msg_timer;
    char  msg[64];
}ShopUI;


static void shop_def_init(ShopDef *def,int shop_id){
    memset(def,0,sizeof(*def));
    #define STOCK(item,qty) do{ def->items[def->count]=item; def->stock[def->count++]=qty; }while(0)
    switch(shop_id){
        case SHOP_WEAPONS:
            STOCK(ITEM_SWORD_IRON,  -1);
            STOCK(ITEM_SWORD_STEEL,  1);
            STOCK(ITEM_POTION_HP,   -1);
            break;
        case SHOP_ARMOR:
            STOCK(ITEM_HELM_LEATHER, -1);
            STOCK(ITEM_HELM_IRON,     1);
            STOCK(ITEM_CHEST_LEATHER,-1);
            STOCK(ITEM_CHEST_IRON,    1);
            STOCK(ITEM_BOOTS_LEATHER,-1);
            break;
        case SHOP_ALCHEMY:
            STOCK(ITEM_POTION_HP,   -1);
            STOCK(ITEM_POTION_MP,   -1);
            STOCK(ITEM_POTION_FULL, -1);
            break;
        case SHOP_GENERAL:
        default:
            STOCK(ITEM_POTION_HP,   -1);
            STOCK(ITEM_POTION_MP,   -1);
            STOCK(ITEM_POTION_FULL,  3);
            STOCK(ITEM_SWORD_IRON,   2);
            STOCK(ITEM_HELM_LEATHER,-1);
            STOCK(ITEM_CHEST_LEATHER,-1);
            STOCK(ITEM_BOOTS_LEATHER,-1);
            break;
    }
    #undef STOCK
}

static int g_active_shop_id=SHOP_GENERAL;
static void shop_open(ShopUI *ui,int shop_id){
    ui->open=true; ui->tab=0; ui->hover=-1; ui->msg_timer=0;
    g_active_shop_id=(shop_id>=0&&shop_id<SHOP_PROFILE_COUNT)?shop_id:SHOP_GENERAL;
}
static ShopUI  g_shop;
static ShopDef g_shop_defs[SHOP_PROFILE_COUNT];


/* ══════════════════════════════════════════════════════════════
   BRANCHING DIALOGUE SYSTEM
   Each DialogueNode has up to 4 player-selectable choices.
   Special next_node values:
      -1 = close dialogue
      -2 = trigger quest offer
      -3 = open merchant shop
   Set NpcDef.dlg_root_id to the root node index.
   If dlg_root_id < 0 the old flat-lines system is used.
   ══════════════════════════════════════════════════════════════ */
#define DLG_MAX_NODES   512
#define DLG_MAX_CHOICES   4
#define DLG_CLOSE        -1
#define DLG_QUEST_OFFER  -2
#define DLG_OPEN_SHOP    -3

typedef struct{
    char text[256];         /* NPC speech text */
    char speaker[32];       /* speaker name override ("" = use NPC name) */
    int  choice_next[DLG_MAX_CHOICES]; /* destination node, or DLG_CLOSE etc */
    char choice_label[DLG_MAX_CHOICES][64];
    int  num_choices;       /* 0 = just "Continue >" auto-advance, requires exactly 1 choice_next[0] */
}DialogueNode;

static DialogueNode g_dlg_nodes[DLG_MAX_NODES];
static int          g_dlg_count = 0;

/* Add a node; returns its index */
static int dlg_node(const char *text, const char *speaker){
    if(g_dlg_count>=DLG_MAX_NODES)return -1;
    int idx=g_dlg_count++;
    DialogueNode *n=&g_dlg_nodes[idx];
    memset(n,0,sizeof(*n));
    strncpy(n->text,text,255);
    if(speaker)strncpy(n->speaker,speaker,31);
    n->num_choices=0;
    return idx;
}
/* Add a choice to an existing node */
static void dlg_choice(int node_idx, const char *label, int next_node){
    if(node_idx<0||node_idx>=g_dlg_count)return;
    DialogueNode *n=&g_dlg_nodes[node_idx];
    if(n->num_choices>=DLG_MAX_CHOICES)return;
    int c=n->num_choices++;
    strncpy(n->choice_label[c],label,63);
    n->choice_next[c]=next_node;
}
/* Convenience: single-choice node that just continues (no branch) */
static int dlg_line(const char *text, const char *speaker, int next_node){
    int idx=dlg_node(text,speaker);
    dlg_choice(idx,"Continue",next_node);
    return idx;
}

static void npc_init(NpcSystem *ns){
    memset(ns,0,sizeof(*ns));
    ns->active_npc=-1;
    for(int i=0;i<NPC_MAX;i++){ns->npcs[i].quest_id=-1;ns->npcs[i].dlg_root_id=-1;ns->npcs[i].dlg_cur_node=-1;}
}
static int npc_add(NpcSystem *ns,Vec3 pos,float yaw,const char *name){
    if(ns->count>=NPC_MAX)return -1;
    int i=ns->count++;
    ns->npcs[i].pos=pos;
    ns->npcs[i].yaw=yaw;
    ns->npcs[i].anim_idx=0;
    ns->npcs[i].anim_time=0;
    ns->npcs[i].quest_id=-1;
    ns->npcs[i].dlg_root_id=-1;
    ns->npcs[i].dlg_cur_node=-1;
    ns->npcs[i].quest_offered=false;
    strncpy(ns->npcs[i].name,name,31);
    return i;
}

/* Attach a quest to an NPC — call after quest_add() */
static void npc_attach_quest(NpcSystem *ns,int npc_idx,int quest_id){
    if(npc_idx<0||npc_idx>=ns->count)return;
    ns->npcs[npc_idx].quest_id=quest_id;
}
static void npc_add_line(NpcSystem *ns,int idx,const char *line){
    if(idx<0||idx>=ns->count)return;
    NpcDef *n=&ns->npcs[idx];
    if(n->line_count>=NPC_LINES_MAX)return;
    strncpy(n->lines[n->line_count++],line,NPC_LINE_LEN-1);
}

/* Returns index of nearest NPC within interact range, or -1 */
static int npc_nearest(NpcSystem *ns,Vec3 player_pos){
    float best=NPC_INTERACT_R*NPC_INTERACT_R;int found=-1;
    for(int i=0;i<ns->count;i++){
        float d2=vec3_dist(ns->npcs[i].pos,player_pos);
        d2=d2*d2;
        if(d2<best){best=d2;found=i;}
    }
    return found;
}

/* Press E / choice selection: open, advance, or follow branch */
static void npc_interact(NpcSystem *ns,Vec3 player_pos,QuestSystem *qs){
    if(ns->open){
        if(ns->talk_state==NPC_STATE_QUEST_OFFER)return;
        if(ns->talk_state==NPC_STATE_SHOP)return;
        NpcDef *n=&ns->npcs[ns->active_npc];
        /* Branching tree mode */
        if(n->dlg_root_id>=0){
            if(n->dlg_cur_node<0){ns->open=false;ns->active_npc=-1;return;}
            DialogueNode *dn=&g_dlg_nodes[n->dlg_cur_node];
            /* If only 1 choice (auto-continue), follow it immediately on E */
            if(dn->num_choices==1){
                int next=dn->choice_next[0];
                if(next==DLG_CLOSE){ns->open=false;ns->active_npc=-1;n->dlg_cur_node=n->dlg_root_id;return;}
                if(next==DLG_QUEST_OFFER){ns->talk_state=NPC_STATE_QUEST_OFFER;return;}
                if(next==DLG_OPEN_SHOP){ns->talk_state=NPC_STATE_SHOP;shop_open(&g_shop,n->shop_id);return;}
                n->dlg_cur_node=next;
            }
            /* Multi-choice: E selects choice 0 as default */
            else if(dn->num_choices>1){
                int next=dn->choice_next[0];
                if(next==DLG_CLOSE){ns->open=false;ns->active_npc=-1;n->dlg_cur_node=n->dlg_root_id;return;}
                if(next==DLG_QUEST_OFFER){ns->talk_state=NPC_STATE_QUEST_OFFER;return;}
                if(next==DLG_OPEN_SHOP){ns->talk_state=NPC_STATE_SHOP;shop_open(&g_shop,n->shop_id);return;}
                n->dlg_cur_node=next;
            }
            return;
        }
        /* Flat lines fallback */
        ns->active_line++;
        if(ns->active_line>=n->line_count){
            if(n->quest_id>=0&&!n->quest_offered){
                for(int i=0;i<qs->count;i++){
                    if(qs->quests[i].id==n->quest_id&&qs->quests[i].status==Q_INACTIVE){
                        ns->talk_state=NPC_STATE_QUEST_OFFER;
                        return;
                    }
                }
            }
            if(n->is_merchant){ns->talk_state=NPC_STATE_SHOP;shop_open(&g_shop,n->shop_id);return;}
            ns->open=false;ns->active_npc=-1;ns->active_line=0;
        }
        return;
    }
    int idx=npc_nearest(ns,player_pos);
    if(idx<0)return;
    ns->active_npc=idx;ns->active_line=0;ns->open=true;
    NpcDef *n=&ns->npcs[idx];
    if(n->dlg_root_id>=0){
        n->dlg_cur_node=n->dlg_root_id;
        ns->talk_state=NPC_STATE_CHAT;
        return;
    }
    if(n->is_merchant&&n->line_count==0){
        ns->talk_state=NPC_STATE_SHOP;
        shop_open(&g_shop,n->shop_id);
    } else {
        ns->talk_state=NPC_STATE_CHAT;
    }
}

/* Select a specific dialogue choice by index (called from UI) */
static void npc_dlg_pick(NpcSystem *ns, int choice_idx, QuestSystem *qs){
    if(!ns->open||ns->active_npc<0)return;
    NpcDef *n=&ns->npcs[ns->active_npc];
    if(n->dlg_cur_node<0||n->dlg_cur_node>=g_dlg_count)return;
    DialogueNode *dn=&g_dlg_nodes[n->dlg_cur_node];
    if(choice_idx<0||choice_idx>=dn->num_choices)return;
    int next=dn->choice_next[choice_idx];
    if(next==DLG_CLOSE){ns->open=false;ns->active_npc=-1;n->dlg_cur_node=n->dlg_root_id;return;}
    if(next==DLG_QUEST_OFFER){ns->talk_state=NPC_STATE_QUEST_OFFER;return;}
    if(next==DLG_OPEN_SHOP){ns->talk_state=NPC_STATE_SHOP;shop_open(&g_shop,n->shop_id);return;}
    n->dlg_cur_node=next;
}

/* Accept quest from active NPC */
static void npc_quest_accept(NpcSystem *ns,QuestSystem *qs,SaveNotify *note){
    if(!ns->open||ns->talk_state!=NPC_STATE_QUEST_OFFER||ns->active_npc<0)return;
    NpcDef *n=&ns->npcs[ns->active_npc];
    quest_start(qs,n->quest_id);
    n->quest_offered=true;
    for(int i=0;i<qs->count;i++){
        if(qs->quests[i].id==n->quest_id){
            char buf[80];snprintf(buf,sizeof(buf),"Quest accepted: %s",qs->quests[i].name);
            save_notify(note,buf);break;
        }
    }
    ns->open=false;ns->active_npc=-1;ns->active_line=0;
    ns->talk_state=NPC_STATE_CHAT;
}

/* Decline quest */
static void npc_quest_decline(NpcSystem *ns){
    if(!ns->open||ns->talk_state!=NPC_STATE_QUEST_OFFER)return;
    ns->open=false;ns->active_npc=-1;ns->active_line=0;
    ns->talk_state=NPC_STATE_CHAT;
}

/* Draw chat box or quest offer panel */
static void npc_draw(NpcSystem *ns,HUD *h,int win_w,int win_h,QuestSystem *qs){
    if(!ns->open||ns->active_npc<0)return;
    NpcDef *n=&ns->npcs[ns->active_npc];

    /* ── Branching dialogue UI ── */
    if(n->dlg_root_id>=0 && ns->talk_state==NPC_STATE_CHAT){
        if(n->dlg_cur_node<0||n->dlg_cur_node>=g_dlg_count)return;
        DialogueNode *dn=&g_dlg_nodes[n->dlg_cur_node];
        const char *spk=dn->speaker[0]?dn->speaker:n->name;
        float W=(float)win_w, H=(float)win_h;
        float pw=700.0f;
        float px=W*0.5f-pw*0.5f;
        /* Height: base panel + 28px per choice */
        float ch_h=(float)dn->num_choices*28.0f;
        float ph=110.0f+ch_h;
        float py=H-ph-70.0f;
        /* Panel background */
        hud_rect(h,px-4,py-4,pw+8,ph+8, 0.05f,0.10f,0.04f,0.96f);
        hud_rect(h,px,  py,  pw,  ph,   0.02f,0.07f,0.02f,0.92f);
        /* Gold speaker name bar */
        hud_rect(h,px,py,pw,22, 0.08f,0.12f,0.02f,1.0f);
        hud_str(h,px+8,py+4,1.5f,spk, 1.0f,0.85f,0.2f,1.0f);
        /* Speech text (wrap at ~80 chars) */
        float ty=py+30.0f;
        const char *txt=dn->text;
        char line[82]; int li=0;
        for(int ci=0;;ci++){
            char c=txt[ci];
            if(c=='\0'||c=='\n'||li>=80){
                line[li]='\0';
                if(li>0)hud_str(h,px+8,ty,1.4f,line, 0.92f,0.92f,0.88f,1.0f);
                ty+=18.0f; li=0;
                if(c=='\0')break;
            } else { line[li++]=c; }
        }
        /* Choices */
        int dmx2,dmy2; SDL_GetMouseState(&dmx2,&dmy2);
        bool lmb_down=(SDL_GetMouseState(NULL,NULL)&SDL_BUTTON(1))!=0;
        float cy2=py+ph-ch_h-4.0f;
        for(int ci=0;ci<dn->num_choices;ci++){
            bool over=(dmx2>=px&&dmx2<=px+pw&&dmy2>=cy2&&dmy2<=cy2+24);
            if(over)hud_rect(h,px,cy2,pw,24, 0.15f,0.25f,0.08f,0.85f);
            char lbl[72]; snprintf(lbl,sizeof(lbl),"> %s",dn->choice_label[ci]);
            hud_str(h,px+12,cy2+5,1.35f,lbl,
                    over?1.0f:0.75f, over?0.9f:0.80f, over?0.3f:0.65f,1.0f);
            if(over&&lmb_down){
                /* Will be acted on next frame via npc_dlg_pick */
            }
            cy2+=28.0f;
        }
        return;
    }

    float bw=600.0f,bh=90.0f;
    float bx=(float)win_w*0.5f-bw*0.5f;
    float by=(float)win_h-bh-80.0f;

    if(ns->talk_state==NPC_STATE_QUEST_OFFER){
        /* Find quest */
        Quest *q=NULL;
        for(int i=0;i<qs->count;i++)
            if(qs->quests[i].id==n->quest_id){q=&qs->quests[i];break;}
        if(!q){ns->open=false;return;}

        /* Larger panel for quest info */
        float qbh=160.0f;
        float qby=(float)win_h-qbh-80.0f;
        hud_rect(h,bx-4,qby-4,bw+8,qbh+8,    0.05f,0.08f,0.03f,0.95f);
        hud_rect(h,bx,  qby,  bw,  qbh,       0.03f,0.06f,0.02f,0.92f);
        hud_rect(h,bx,qby-4,     bw,3,         0.4f,0.9f,0.2f,1.0f);
        hud_rect(h,bx,qby+qbh+1, bw,3,         0.4f,0.9f,0.2f,1.0f);

        /* Quest icon ! */
        hud_str(h,bx+12,qby+8, 2.2f,"!",      0.9f,0.8f,0.1f,1.0f);
        hud_str(h,bx+32,qby+8, 1.8f,"NEW QUEST",0.9f,0.8f,0.1f,1.0f);
        hud_str(h,bx+12,qby+34,1.8f,q->name,  0.95f,0.95f,0.90f,1.0f);
        hud_str(h,bx+12,qby+56,1.4f,q->desc,  0.80f,0.80f,0.75f,0.9f);

        /* Objectives preview */
        float oy=qby+76;
        for(int j=0;j<q->obj_count&&j<2;j++){
            char obuf[80];snprintf(obuf,sizeof(obuf),"  - %s",q->objs[j].desc);
            hud_str(h,bx+12,oy,1.3f,obuf, 0.7f,0.85f,0.7f,0.85f);
            oy+=16;
        }
        /* Reward */
        char rbuf[64];snprintf(rbuf,sizeof(rbuf),"Reward: %d Gold  %d XP",q->gold,q->xp);
        hud_str(h,bx+12,qby+qbh-30,1.3f,rbuf, 0.9f,0.75f,0.2f,0.9f);

        /* Accept / Decline buttons */
        float btnw=110,btnh=26,btny=qby+qbh-28;
        hud_rect(h,bx+bw-240,btny,btnw,btnh, 0.1f,0.4f,0.1f,0.9f);
        hud_str(h,bx+bw-232,btny+6,1.5f,"[Y] Accept", 0.3f,1.0f,0.3f,1.0f);
        hud_rect(h,bx+bw-120,btny,btnw,btnh, 0.4f,0.1f,0.1f,0.9f);
        hud_str(h,bx+bw-112,btny+6,1.5f,"[N] Decline",1.0f,0.3f,0.3f,1.0f);

        hud_flush(h);
        return;
    }

    /* Normal chat */
    if(ns->active_line>=n->line_count)return;

    hud_rect(h,bx-4,by-4,bw+8,bh+8,   0.08f,0.06f,0.02f,0.92f);
    hud_rect(h,bx,  by,  bw,  bh,     0.05f,0.04f,0.01f,0.88f);
    hud_rect(h,bx,by-4,     bw,3,     0.80f,0.65f,0.10f,1.0f);
    hud_rect(h,bx,by+bh+1,  bw,3,     0.80f,0.65f,0.10f,1.0f);
    hud_str(h,bx+12,by+8,1.8f,n->name,0.95f,0.80f,0.20f,1.0f);

    const char *line=n->lines[ns->active_line];
    char row[2][64]={{0},{0}};
    int len=(int)strlen(line);
    if(len<=55){
        strncpy(row[0],line,63);
    } else {
        int cut=54;
        while(cut>0&&line[cut]!=' ')cut--;
        if(cut==0)cut=54;
        strncpy(row[0],line,cut);row[0][cut]=0;
        strncpy(row[1],line+cut+1,63);
    }
    hud_str(h,bx+12,by+30,1.6f,row[0],0.95f,0.95f,0.90f,1.0f);
    if(row[1][0])hud_str(h,bx+12,by+50,1.6f,row[1],0.95f,0.95f,0.90f,1.0f);

    /* Show whether E will lead to a quest offer next */
    int remaining=n->line_count-ns->active_line-1;
    bool has_quest=(n->quest_id>=0&&!n->quest_offered);
    const char *prompt=remaining>0?"[E] Next":(has_quest?"[E] View Quest":"[E] Close");
    hud_str(h,bx+bw-130,by+bh-18,1.4f,prompt,0.60f,0.60f,0.60f,0.85f);

    hud_flush(h);
}

static void npc_draw_hints(NpcSystem *ns,HUD *h,Vec3 player_pos,
                            Mat4 proj,Mat4 view,int win_w,int win_h,
                            QuestSystem *qs){
    for(int i=0;i<ns->count;i++){
        float dist=vec3_dist(ns->npcs[i].pos,player_pos);
        NpcDef *n=&ns->npcs[i];

        /* Check if NPC has an available quest */
        bool has_quest=false;
        if(n->quest_id>=0&&!n->quest_offered){
            for(int qi=0;qi<qs->count;qi++)
                if(qs->quests[qi].id==n->quest_id&&qs->quests[qi].status==Q_INACTIVE)
                    {has_quest=true;break;}
        }

        /* Show ! icon always when NPC has quest, talk hint only when close */
        float show_r=has_quest?40.0f:NPC_INTERACT_R*1.5f;
        if(dist>show_r)continue;

        /* Project world pos to screen */
        Vec3 wp={n->pos.x,n->pos.y+2.4f,n->pos.z};
        float *p=proj.m,*v=view.m;
        float vx=v[0]*wp.x+v[4]*wp.y+v[8]*wp.z+v[12];
        float vy=v[1]*wp.x+v[5]*wp.y+v[9]*wp.z+v[13];
        float vz=v[2]*wp.x+v[6]*wp.y+v[10]*wp.z+v[14];
        float vw=v[3]*wp.x+v[7]*wp.y+v[11]*wp.z+v[15];
        float clip[4];
        clip[0]=p[0]*vx+p[4]*vy+p[8]*vz+p[12]*vw;
        clip[1]=p[1]*vx+p[5]*vy+p[9]*vz+p[13]*vw;
        clip[3]=p[3]*vx+p[7]*vy+p[11]*vz+p[15]*vw;
        if(clip[3]<=0)continue;
        float sx=(clip[0]/clip[3]*0.5f+0.5f)*(float)win_w;
        float sy=(1.0f-clip[1]/clip[3]*0.5f-0.5f)*(float)win_h;

        if(has_quest){
            /* Pulsing yellow ! above head — always visible within range */
            float alpha=clampf(1.0f-(dist/show_r)*0.5f,0.4f,1.0f);
            hud_rect(h,sx-10,sy-28,22,26, 0.0f,0.0f,0.0f,0.6f*alpha);
            hud_str(h,sx-6,sy-26,2.5f,"!", 1.0f,0.85f,0.0f,alpha);
        }

        if(dist<NPC_INTERACT_R*1.5f){
            char buf[64];snprintf(buf,sizeof(buf),"%s",n->name);
            hud_str(h,sx-40,sy-16+(!has_quest?-12:0),1.5f,buf,0.95f,0.80f,0.20f,0.95f);
            const char *hint=has_quest?"[E] Talk  (Quest available)":"[E] Talk";
            hud_str(h,sx-50,sy+(!has_quest?-12:0),1.3f,hint,0.85f,0.85f,0.85f,0.85f);
        }
    }
    hud_flush(h);
}

/* ══════════════════════════════════════════════════════════════
   MAIN
   ══════════════════════════════════════════════════════════════ */
/* ── Splash screen image system ─────────────────────────────────
   Place JPEG/PNG images at:
       assets/splash/splash0.jpg  … splash4.jpg   (up to 5 images)
   Each load stage advances to the next image.
   Falls back to a plain dark background if no images are found.   */
#define SPLASH_MAX 5
typedef struct {
    GLuint prog;
    GLuint vao, vbo;
    GLuint tex[SPLASH_MAX];
    int    count;           /* number of valid textures loaded */
} SplashScreen;

static SplashScreen g_splash;
static bool         g_splash_ready = false;

/* ── Gamepad / controller ── */
static SDL_GameController *g_pad = NULL;
static bool                g_pad_a_prev  = false;
static bool                g_pad_x_prev  = false;
static bool                g_pad_y_prev  = false;
static bool                g_pad_b_prev  = false;
static bool                g_pad_start_prev = false;
static bool                g_pad_back_prev  = false;

static void gamepad_try_open(void){
    for(int i=0;i<SDL_NumJoysticks();i++){
        if(SDL_IsGameController(i)){
            g_pad=SDL_GameControllerOpen(i);
            if(g_pad){printf("[Pad] Opened: %s\n",SDL_GameControllerName(g_pad));return;}
        }
    }
}
static float pad_axis(SDL_GameControllerAxis a, float dead){
    if(!g_pad)return 0.0f;
    float v=SDL_GameControllerGetAxis(g_pad,a)/32767.0f;
    return fabsf(v)>dead?v:0.0f;
}
static bool pad_btn(SDL_GameControllerButton b){
    return g_pad&&SDL_GameControllerGetButton(g_pad,b)!=0;
}

static void splash_init(void){
    /* ── Fullscreen-quad shader ── */
    const char *vsrc =
        "#version 330 core\n"
        "layout(location=0)in vec2 aPos;\n"
        "out vec2 vUV;\n"
        "void main(){\n"
        "    vUV = vec2(aPos.x*0.5+0.5, 0.5-aPos.y*0.5);\n"
        "    gl_Position=vec4(aPos,0.0,1.0);\n"
        "}\n";
    const char *fsrc =
        "#version 330 core\n"
        "in vec2 vUV;\n"
        "uniform sampler2D uTex;\n"
        "uniform float uFade;\n"    /* 0=black, 1=full */
        "out vec4 FragColor;\n"
        "void main(){\n"
        "    vec4 c=texture(uTex,vUV);\n"
        "    /* slight vignette */\n"
        "    vec2 uv=vUV*2.0-1.0;\n"
        "    float vig=1.0-dot(uv*0.5,uv*0.5);\n"
        "    vig=clamp(vig,0.0,1.0);\n"
        "    c.rgb*=mix(0.5,1.0,vig);\n"
        "    FragColor=vec4(c.rgb*uFade, 1.0);\n"
        "}\n";
    GLuint vs=glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs,1,&vsrc,NULL);glCompileShader(vs);
    GLuint fs=glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs,1,&fsrc,NULL);glCompileShader(fs);
    g_splash.prog=glCreateProgram();
    glAttachShader(g_splash.prog,vs);glAttachShader(g_splash.prog,fs);
    glLinkProgram(g_splash.prog);
    glDeleteShader(vs);glDeleteShader(fs);

    /* ── Fullscreen quad (NDC -1..1) ── */
    float q[]={-1,-1, 1,-1, 1,1, -1,-1, 1,1, -1,1};
    glGenVertexArrays(1,&g_splash.vao);glGenBuffers(1,&g_splash.vbo);
    glBindVertexArray(g_splash.vao);
    glBindBuffer(GL_ARRAY_BUFFER,g_splash.vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(q),q,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,8,0);
    glBindVertexArray(0);

    /* ── Load splash images ── */
    g_splash.count=0;
    for(int i=0;i<SPLASH_MAX;i++){
        char path[128];
        snprintf(path,sizeof(path),"assets/splash/splash%d.jpg",i);
        int w,h,ch;
        uint8_t *px=stbi_load(path,&w,&h,&ch,3);
        if(!px){
            /* also try .png */
            snprintf(path,sizeof(path),"assets/splash/splash%d.png",i);
            px=stbi_load(path,&w,&h,&ch,3);
        }
        if(!px) break;
        GLuint tid;glGenTextures(1,&tid);glBindTexture(GL_TEXTURE_2D,tid);
        glTexImage2D(GL_TEXTURE_2D,0,GL_SRGB8,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,px);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        stbi_image_free(px);
        g_splash.tex[g_splash.count++]=tid;
        printf("[Splash] Loaded %s (%dx%d)\n",path,w,h);
    }
    g_splash_ready=true;
}

/* Draw the splash background for loading stage done/total */
static void splash_draw_bg(int done, int total){
    if(!g_splash_ready||g_splash.count==0) return;
    /* Pick image: advance one image per equal slice of the bar */
    int idx=0;
    if(g_splash.count>1&&total>0){
        idx=(int)((float)done/(float)total*(float)g_splash.count);
        if(idx>=g_splash.count) idx=g_splash.count-1;
    }
    glDisable(GL_DEPTH_TEST);
    glUseProgram(g_splash.prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,g_splash.tex[idx]);
    glUniform1i(glGetUniformLocation(g_splash.prog,"uTex"),0);
    glUniform1f(glGetUniformLocation(g_splash.prog,"uFade"),1.0f);
    glBindVertexArray(g_splash.vao);
    glDrawArrays(GL_TRIANGLES,0,6);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

/* ── Menu / Loading screen ───────────────────────────────────── */
typedef enum{GAME_MENU=0,GAME_LOADING,GAME_PLAYING}GamePhase;

/* Returns true if (mx,my) is inside the button rect */
static bool menu_btn(HUD *h,float x,float y,float w,float ht,
                     const char *label,float sc,
                     int mx,int my,bool hover,bool *clicked){
    bool over=(mx>=x&&mx<=x+w&&my>=y&&my<=y+ht);
    if(hover&&over){hud_rect(h,x-2,y-2,w+4,ht+4, 1.0f,0.85f,0.2f,1.0f);}  /* gold outline */
    float br=over?0.25f:0.12f, bg=over?0.15f:0.08f, bb=over?0.05f:0.03f;
    hud_rect(h,x,y,w,ht, br,bg,bb,0.92f);
    /* Centre label */
    float tw=sc*8.0f*(float)strlen(label);
    hud_str(h,x+(w-tw)*0.5f,y+(ht-sc*10.0f)*0.5f,sc,label,1.0f,0.9f,0.6f,1.0f);
    if(clicked)*clicked=(over&&SDL_GetMouseState(NULL,NULL)&SDL_BUTTON(1));
    return over;
}



/* Step-by-step loading — call between each asset load to show progress */
static void loading_render(HUD *h,int win_w,int win_h,
                           const char *step,int done,int total){
    h->vcount=0;
    float W=(float)win_w,H=(float)win_h;
    float pct=total>0?(float)done/(float)total:0.0f;

    /* ── Background: splash image or plain dark fill ── */
    if(g_splash_ready&&g_splash.count>0){
        splash_draw_bg(done,total);
    } else {
        /* No splash images — dark gradient fallback */
        hud_rect(h,0,0,W,H, 0.04f,0.03f,0.08f,1.0f);
        hud_flush(h); h->vcount=0;
    }

    /* ── Bottom overlay panel (translucent dark strip) ── */
    float panel_h = H*0.30f;
    float panel_y = H - panel_h;
    hud_rect(h,0,panel_y,W,panel_h,  0.0f,0.0f,0.0f,0.72f);

    /* Title — large, gold, above the panel */
    float tsc=4.0f;
    const char *title="LOADING...";
    float tw=tsc*8.0f*(float)strlen(title);
    hud_str(h,(W-tw)*0.5f, panel_y - tsc*12.0f - 8.0f,
            tsc,title, 1.0f,0.82f,0.22f,1.0f);

    /* Step label */
    float ssc=1.6f;
    float stw=ssc*8.0f*(float)strlen(step);
    hud_str(h,(W-stw)*0.5f, panel_y+18.0f, ssc,step, 0.85f,0.85f,0.85f,1.0f);

    /* Progress bar */
    float bx=W*0.15f, by=panel_y+panel_h*0.42f;
    float bw=W*0.70f, bh=20.0f;
    hud_rect(h,bx,by,bw,bh, 0.08f,0.08f,0.08f,1.0f);
    if(pct>0.0f){
        /* gradient fill: dark orange → gold */
        float t=pct;
        float fr=0.55f+0.25f*t, fg=0.35f+0.25f*t, fb=0.05f;
        hud_rect(h,bx,by,bw*pct,bh, fr,fg,fb,1.0f);
    }
    /* Bar border */
    hud_rect(h,bx,       by,       bw, 2,    0.6f,0.45f,0.05f,1.0f);
    hud_rect(h,bx,       by+bh-2,  bw, 2,    0.6f,0.45f,0.05f,1.0f);
    hud_rect(h,bx,       by,       2,  bh,   0.6f,0.45f,0.05f,1.0f);
    hud_rect(h,bx+bw-2,  by,       2,  bh,   0.6f,0.45f,0.05f,1.0f);

    /* Percent text below bar */
    char pct_buf[16]; snprintf(pct_buf,sizeof(pct_buf),"%d%%",(int)(pct*100.0f));
    float ptw=ssc*8.0f*(float)strlen(pct_buf);
    hud_str(h,(W-ptw)*0.5f, by+bh+10.0f, ssc,pct_buf, 1.0f,0.90f,0.50f,1.0f);

    /* Tiny bottom-right hint */
    const char *hint="Press any key after loading";
    float hsc=1.1f, htw=hsc*8.0f*(float)strlen(hint);
    hud_str(h, W-htw-16.0f, H-hsc*12.0f-8.0f, hsc,hint, 0.5f,0.5f,0.5f,0.8f);

    hud_flush(h);
}

/* Helper: render one loading frame and swap buffers */
#define LOAD_STEP(h,win,step,done,total) do{ \
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT); \
    loading_render((h),(win##_w),(win##_h),(step),(done),(total)); \
    SDL_GL_SwapWindow(win); \
    SDL_PumpEvents(); \
}while(0)

/* ── Inventory System ───────────────────────────────────────── */
static void save_notify(SaveNotify *n,const char *msg); /* forward decl — defined in save section */
#define INV_SLOTS      24   /* 6×4 grid */
#define INV_EQUIP_SLOTS 4   /* weapon, helmet, chest, boots */
#define INV_STACK_MAX  99


typedef enum{
    ICAT_NONE,ICAT_CONSUMABLE,ICAT_WEAPON,ICAT_HELM,ICAT_CHEST,ICAT_BOOTS,ICAT_MISC
}ItemCat;

typedef struct{
    const char *name;
    const char *desc;
    ItemCat     cat;
    float       hp_restore;   /* consumable */
    float       mp_restore;
    float       dmg_bonus;    /* weapon */
    float       hp_bonus;     /* armor */
    float       dmg_reduce;   /* armor 0-1 */
    float       spd_bonus;    /* boots multiplier */
    float       drop_chance;  /* 0-1 per enemy type */
    float       r,g,b;        /* icon color */
}ItemDef;

static const ItemDef IDEF[ITEM_COUNT]={
/* NONE         */ {"",           "",                        ICAT_NONE,      0,  0,  0,  0,  0,    0,    0,    0,0,0},
/* POTION_HP    */ {"HP Potion",  "Restores 40 health",      ICAT_CONSUMABLE,40, 0,  0,  0,  0,    0,    0.55f,0.9f,0.1f,0.1f},
/* POTION_MP    */ {"MP Potion",  "Restores 40 mana",        ICAT_CONSUMABLE,0,  40, 0,  0,  0,    0,    0.45f,0.1f,0.2f,0.9f},
/* POTION_FULL  */ {"Full Potion","Restores HP and MP",      ICAT_CONSUMABLE,60, 60, 0,  0,  0,    0,    0.25f,0.8f,0.1f,0.8f},
/* SWORD_IRON   */ {"Iron Sword", "+5 attack damage",        ICAT_WEAPON,    0,  0,  5,  0,  0,    0,    0.35f,0.7f,0.7f,0.75f},
/* SWORD_STEEL  */ {"Steel Sword","+12 attack damage",       ICAT_WEAPON,    0,  0,  12, 0,  0,    0,    0.20f,0.8f,0.85f,0.9f},
/* SWORD_MAGIC  */ {"Magic Sword","+22 damage, glows",       ICAT_WEAPON,    0,  0,  22, 0,  0,    0,    0.15f,0.3f,0.7f,1.0f},
/* HELM_LEATHER */ {"Leather Helm","+10 max HP",             ICAT_HELM,      0,  0,  0,  10, 0,    0,    0.30f,0.6f,0.45f,0.25f},
/* HELM_IRON    */ {"Iron Helm",  "+25 max HP",              ICAT_HELM,      0,  0,  0,  25, 0,    0,    0.15f,0.7f,0.7f,0.7f},
/* CHEST_LEATHER*/ {"Leather Chest","+15 HP, -10% damage",  ICAT_CHEST,     0,  0,  0,  15, 0.10f,0,    0.30f,0.55f,0.4f,0.2f},
/* CHEST_IRON   */ {"Iron Chest", "+35 HP, -20% damage",    ICAT_CHEST,     0,  0,  0,  35, 0.20f,0,    0.15f,0.65f,0.65f,0.65f},
/* BOOTS_LEATHER*/ {"Leather Boots","+15% move speed",      ICAT_BOOTS,     0,  0,  0,  0,  0,    0.15f,0.30f,0.5f,0.35f,0.2f},
/* FISH_RAW     */ {"Raw Fish",   "Common freshwater catch", ICAT_MISC,     0,  0,  0,  0,  0,    0,    0.40f,0.82f,0.92f,0.95f},
/* FISH_TROUT   */ {"River Trout","Uncommon clean-water fish",ICAT_MISC,    0,  0,  0,  0,  0,    0,    0.55f,0.92f,0.78f,0.55f},
/* FISH_GOLDEN  */ {"Golden Koi", "Rare shimmering catch",   ICAT_MISC,      0,  0,  0,  0,  0,    0,    0.95f,0.82f,0.28f,0.10f},
/* GOLD_COIN    */ {"Gold",       "Currency",                ICAT_CONSUMABLE,0,  0,  0,  0,  0,    0,    1.0f, 1.0f,0.8f,0.1f},
};

typedef struct{ItemType type;int count;}InvSlot;

typedef struct{
    InvSlot slots[INV_SLOTS];
    ItemType equipped[INV_EQUIP_SLOTS]; /* 0=weapon 1=helm 2=chest 3=boots */
    bool open;
    int  hover;   /* hovered slot index, -1=none */
    int  selected;/* selected slot for use/equip */
    /* Derived stats (recomputed on equip change) */
    float dmg_bonus;
    float hp_bonus;
    float dmg_reduce;
    float spd_bonus;
}Inventory;
static bool inv_add(Inventory *inv,ItemType type,int count);        /* forward */
static void inv_recalc(Inventory *inv,Player *pl,float base_max_hp); /* forward */
static const int ITEM_PRICE[ITEM_COUNT]={
    0,   /* NONE         */
    15,  /* POTION_HP    */
    15,  /* POTION_MP    */
    30,  /* POTION_FULL  */
    50,  /* SWORD_IRON   */
    150, /* SWORD_STEEL  */
    400, /* SWORD_MAGIC  */
    40,  /* HELM_LEATHER */
    100, /* HELM_IRON    */
    60,  /* CHEST_LEATHER*/
    180, /* CHEST_IRON   */
    45,  /* BOOTS_LEATHER*/
    8,   /* FISH_RAW     */
    18,  /* FISH_TROUT   */
    80,  /* FISH_GOLDEN  */
    0,   /* GOLD_COIN    */
};
static void shop_close(ShopUI *ui){ ui->open=false; }

static void shop_notify(ShopUI *ui,const char *m){
    strncpy(ui->msg,m,sizeof(ui->msg)-1); ui->msg[sizeof(ui->msg)-1]=0;
    ui->msg_timer=2.5f;
}

static void shop_draw(ShopUI *ui,HUD *h,int win_w,int win_h,int mx,int my,float dt,
                       ShopDef *def,Inventory *inv,QuestSystem *qs,
                       Player *pl,float base_max_hp){
    if(!ui->open)return;
    float W=(float)win_w,H=(float)win_h;
    hud_rect(h,0,0,W,H, 0,0,0,0.6f);

    float pw=620,ph=400,px=(W-pw)*0.5f,py=(H-ph)*0.5f;
    hud_rect(h,px,py,pw,ph, 0.05f,0.04f,0.02f,0.97f);
    hud_rect(h,px,py,pw,3,          0.7f,0.5f,0.1f,1);
    hud_rect(h,px,py+ph-3,pw,3,     0.7f,0.5f,0.1f,1);
    hud_rect(h,px,py,3,ph,          0.7f,0.5f,0.1f,1);
    hud_rect(h,px+pw-3,py,3,ph,     0.7f,0.5f,0.1f,1);

    /* Title */
    const char *ttl="MERCHANT";
    float tsc=2.2f,ttw=tsc*8.0f*(float)strlen(ttl);
    hud_str(h,px+(pw-ttw)*0.5f,py+10,tsc,ttl, 1.0f,0.85f,0.2f,1.0f);
    char gbuf[32];snprintf(gbuf,sizeof(gbuf),"Gold: %d",qs->player_gold);
    hud_str(h,px+pw-160,py+14,1.5f,gbuf, 1.0f,0.9f,0.2f,1.0f);

    /* Tabs */
    float tbx=px+pw*0.25f,tby=py+40,tbw=100,tbh=28;
    bool buy_over=(mx>=tbx&&mx<=tbx+tbw&&my>=tby&&my<=tby+tbh);
    bool sel_over=(mx>=tbx+tbw+8&&mx<=tbx+tbw*2+8&&my>=tby&&my<=tby+tbh);
    hud_rect(h,tbx,tby,tbw,tbh,   ui->tab==0?0.25f:0.12f, ui->tab==0?0.18f:0.08f, 0.05f,1.0f);
    hud_str(h,tbx+20,tby+6,1.6f,"BUY",  ui->tab==0?1.0f:0.6f, ui->tab==0?0.9f:0.55f, 0.3f,1.0f);
    hud_rect(h,tbx+tbw+8,tby,tbw,tbh, ui->tab==1?0.25f:0.12f, ui->tab==1?0.18f:0.08f, 0.05f,1.0f);
    hud_str(h,tbx+tbw+22,tby+6,1.6f,"SELL", ui->tab==1?1.0f:0.6f, ui->tab==1?0.9f:0.55f, 0.3f,1.0f);
    hud_rect(h,px+20,tby+tbh+4,pw-40,2, 0.4f,0.3f,0.05f,0.7f);

    /* Item list */
    float lx=px+24,ly=tby+tbh+14,lw=pw-48,lh=34,lgap=4;
    int item_count=0;
    bool clicked=(SDL_GetMouseState(NULL,NULL)&SDL_BUTTON(1))!=0;

    if(ui->tab==0){
        /* BUY — list shop stock */
        for(int i=0;i<def->count;i++){
            ItemType it=def->items[i];
            if(it==ITEM_NONE)continue;
            float iy=ly+(lh+lgap)*item_count;
            if(iy+lh>py+ph-50)break;
            bool over=(mx>=lx&&mx<=lx+lw&&my>=iy&&my<=iy+lh);
            if(over)hud_rect(h,lx-2,iy-2,lw+4,lh+4, 0.7f,0.55f,0.1f,0.5f);
            hud_rect(h,lx,iy,lw,lh, over?0.20f:0.10f,0.08f,0.04f,0.95f);
            /* Color swatch */
            hud_rect(h,lx+4,iy+6,16,16, IDEF[it].r,IDEF[it].g,IDEF[it].b,1.0f);
            hud_str(h,lx+26,iy+8,1.5f,IDEF[it].name, 1.0f,0.95f,0.8f,1.0f);
            hud_str(h,lx+200,iy+8,1.3f,IDEF[it].desc, 0.7f,0.7f,0.65f,0.85f);
            char pbuf[24];snprintf(pbuf,sizeof(pbuf),"%dg",ITEM_PRICE[it]);
            float ptw=1.5f*8.0f*(float)strlen(pbuf);
            hud_str(h,lx+lw-ptw-8,iy+8,1.5f,pbuf, 1.0f,0.85f,0.2f,1.0f);
            if(over&&clicked&&ui->hover!=i+100){
                ui->hover=i+100; /* debounce */
                if(qs->player_gold>=ITEM_PRICE[it]){
                    if(inv_add(inv,it,1)){
                        qs->player_gold-=ITEM_PRICE[it];
                        inv_recalc(inv,pl,base_max_hp);
                        char m[64];snprintf(m,sizeof(m),"Bought %s",IDEF[it].name);
                        shop_notify(ui,m);audio_play(SND_MENU_CLICK,80);
                    } else shop_notify(ui,"Inventory full!");
                } else shop_notify(ui,"Not enough gold!");
            } else if(!clicked) ui->hover=-1;
            item_count++;
        }
    } else {
        /* SELL — list player inventory */
        for(int i=0;i<INV_SLOTS;i++){
            InvSlot *s=&inv->slots[i];
            if(s->type==ITEM_NONE||s->type==ITEM_GOLD_COIN)continue;
            float iy=ly+(lh+lgap)*item_count;
            if(iy+lh>py+ph-50)break;
            bool over=(mx>=lx&&mx<=lx+lw&&my>=iy&&my<=iy+lh);
            if(over)hud_rect(h,lx-2,iy-2,lw+4,lh+4, 0.7f,0.55f,0.1f,0.5f);
            hud_rect(h,lx,iy,lw,lh, over?0.20f:0.10f,0.08f,0.04f,0.95f);
            hud_rect(h,lx+4,iy+6,16,16, IDEF[s->type].r,IDEF[s->type].g,IDEF[s->type].b,1.0f);
            char nbuf[48];snprintf(nbuf,sizeof(nbuf),"%s x%d",IDEF[s->type].name,s->count);
            hud_str(h,lx+26,iy+8,1.5f,nbuf, 1.0f,0.95f,0.8f,1.0f);
            int sell_price=(int)(ITEM_PRICE[s->type]*SHOP_SELL_FACTOR);
            char pbuf[24];snprintf(pbuf,sizeof(pbuf),"%dg each",sell_price);
            float ptw=1.4f*8.0f*(float)strlen(pbuf);
            hud_str(h,lx+lw-ptw-8,iy+8,1.4f,pbuf, 0.8f,0.75f,0.2f,0.9f);
            if(over&&clicked&&ui->hover!=i){
                ui->hover=i;
                qs->player_gold+=sell_price;
                s->count--;if(s->count<=0){s->type=ITEM_NONE;s->count=0;}
                inv_recalc(inv,pl,base_max_hp);
                char m[64];snprintf(m,sizeof(m),"Sold for %dg",sell_price);
                shop_notify(ui,m);audio_play(SND_MENU_CLICK,80);
            } else if(!clicked) ui->hover=-1;
            item_count++;
        }
        if(item_count==0)
            hud_str(h,lx,ly+20,1.5f,"Nothing to sell", 0.5f,0.5f,0.5f,0.8f);
    }

    /* Message */
    if(ui->msg_timer>0){
        ui->msg_timer-=dt;
        float alpha=ui->msg_timer>0.5f?1.0f:ui->msg_timer*2.0f;
        float mtw=1.5f*8.0f*(float)strlen(ui->msg);
        hud_str(h,px+(pw-mtw)*0.5f,py+ph-36,1.5f,ui->msg, 1.0f,0.9f,0.3f,alpha);
    }

    hud_str(h,px+pw*0.5f-60,py+ph-18,1.2f,"ESC to close shop", 0.45f,0.45f,0.45f,0.6f);
    hud_flush(h);
    (void)buy_over;(void)sel_over;
}


static void inv_init(Inventory *inv){
    memset(inv,0,sizeof(*inv));
    inv->hover=-1;inv->selected=-1;
    for(int i=0;i<INV_EQUIP_SLOTS;i++)inv->equipped[i]=ITEM_NONE;
}

/* Add item — returns false if inventory full */
static bool inv_add(Inventory *inv,ItemType type,int count){
    if(type==ITEM_NONE)return true;
    /* Try to stack */
    for(int i=0;i<INV_SLOTS;i++){
        if(inv->slots[i].type==type&&inv->slots[i].count<INV_STACK_MAX){
            inv->slots[i].count+=count;
            if(inv->slots[i].count>INV_STACK_MAX)inv->slots[i].count=INV_STACK_MAX;
            return true;
        }
    }
    /* Find empty slot */
    for(int i=0;i<INV_SLOTS;i++){
        if(inv->slots[i].type==ITEM_NONE){
            inv->slots[i].type=type;inv->slots[i].count=count;return true;
        }
    }
    return false; /* full */
}

/* Recompute derived stats from equipped items */
static void inv_recalc(Inventory *inv,Player *pl,float base_max_hp){
    inv->dmg_bonus=0;inv->hp_bonus=0;inv->dmg_reduce=0;inv->spd_bonus=0;
    for(int i=0;i<INV_EQUIP_SLOTS;i++){
        if(inv->equipped[i]==ITEM_NONE)continue;
        const ItemDef *d=&IDEF[inv->equipped[i]];
        inv->dmg_bonus  +=d->dmg_bonus;
        inv->hp_bonus   +=d->hp_bonus;
        inv->dmg_reduce +=d->dmg_reduce;
        inv->spd_bonus  +=d->spd_bonus;
    }
    pl->max_health=base_max_hp+inv->hp_bonus;
    if(pl->health>pl->max_health)pl->health=pl->max_health;
}

/* Use or equip item in slot i */
static void inv_use(Inventory *inv,int i,Player *pl,AbilitySystem *as,
                    QuestSystem *qs,float base_max_hp,SaveNotify *note){
    if(i<0||i>=INV_SLOTS)return;
    InvSlot *s=&inv->slots[i];
    if(s->type==ITEM_NONE)return;
    const ItemDef *d=&IDEF[s->type];

    if(d->cat==ICAT_CONSUMABLE){
        if(s->type==ITEM_GOLD_COIN){
            qs->player_gold+=s->count;s->type=ITEM_NONE;s->count=0;
            save_notify(note,"Gold collected!");return;
        }
        float old_hp=pl->health,old_mp=as->mana;
        pl->health=clampf(pl->health+d->hp_restore,0,pl->max_health);
        as->mana  =clampf(as->mana  +d->mp_restore,0,as->max_mana);
        if(pl->health!=old_hp||as->mana!=old_mp){
            s->count--;if(s->count<=0){s->type=ITEM_NONE;s->count=0;}
            save_notify(note,d->name);
        }
    } else {
        /* Equip/unequip */
        int slot=-1;
        if(d->cat==ICAT_WEAPON)slot=0;
        else if(d->cat==ICAT_HELM)slot=1;
        else if(d->cat==ICAT_CHEST)slot=2;
        else if(d->cat==ICAT_BOOTS)slot=3;
        if(slot<0)return;
        ItemType prev=inv->equipped[slot];
        if(prev==s->type){ /* unequip */
            inv->equipped[slot]=ITEM_NONE;
            save_notify(note,"Unequipped");
        } else {
            if(prev!=ITEM_NONE)inv_add(inv,prev,1); /* return old item */
            inv->equipped[slot]=s->type;
            s->count--;if(s->count<=0){s->type=ITEM_NONE;s->count=0;}
            char buf[64];snprintf(buf,sizeof(buf),"Equipped %s",d->name);
            save_notify(note,buf);
        }
        inv_recalc(inv,pl,base_max_hp);
    }
}

/* Quick-use: finds the first HP (or full) potion in inventory and uses it.
   Called from the H hotkey. Returns true if a potion was consumed.        */
static bool quick_use_potion(Inventory *inv,Player *pl,AbilitySystem *as,
                             QuestSystem *qs,float base_max_hp,SaveNotify *note){
    /* Prefer POTION_FULL, then POTION_HP */
    int best=-1;
    ItemType prefer[]={ITEM_POTION_FULL,ITEM_POTION_HP};
    for(int p=0;p<2&&best<0;p++)
        for(int i=0;i<INV_SLOTS;i++)
            if(inv->slots[i].type==prefer[p]&&inv->slots[i].count>0){best=i;break;}
    if(best<0)return false;
    float old=pl->health;
    inv_use(inv,best,pl,as,qs,base_max_hp,note);
    return pl->health!=old;
}

/* Drop table — called on enemy death */
/* ── World Items (loot on the ground) ───────────────────────── */
#define AUTO_PICKUP    0      /* 0 = press E to pick up, 1 = walk over auto-collects */
#define MAX_WORLD_ITEMS 64
#define PICKUP_RADIUS   1.8f  /* how close player must be */

typedef struct{
    ItemType type;
    int      count;
    Vec3     pos;
    bool     active;
    float    bob;   /* phase for floating animation */
}WorldItem;

typedef struct{WorldItem items[MAX_WORLD_ITEMS];int count;}WorldItemList;

static void wi_init(WorldItemList *wl){memset(wl,0,sizeof(*wl));}

static void wi_spawn(WorldItemList *wl,ItemType type,int count,Vec3 pos){
    if(type==ITEM_NONE)return;
    /* Try existing inactive slot */
    for(int i=0;i<MAX_WORLD_ITEMS;i++){
        if(!wl->items[i].active){
            wl->items[i].type=type;wl->items[i].count=count;
            wl->items[i].pos=pos;wl->items[i].active=true;
            wl->items[i].bob=(float)rand()/RAND_MAX*(float)(2*M_PI);
            if(wl->count<=i)wl->count=i+1;
            return;
        }
    }
}

/* Update + auto-pickup or E-prompt. Returns item picked up or ITEM_NONE */
static void wi_update(WorldItemList *wl,float dt,Vec3 player_pos,
                      Inventory *inv,QuestSystem *qs,SaveNotify *note,
                      bool press_e){
    for(int i=0;i<wl->count;i++){
        WorldItem *w=&wl->items[i];if(!w->active)continue;
        w->bob+=dt*2.5f;
        float dist=vec3_dist(w->pos,player_pos);
        bool in_range=(dist<PICKUP_RADIUS);
        bool pick=(in_range&&(AUTO_PICKUP||press_e));
        if(!pick)continue;
        /* Gold is always auto-collected */
        if(w->type==ITEM_GOLD_COIN||AUTO_PICKUP||press_e){
            if(w->type==ITEM_GOLD_COIN){
                qs->player_gold+=w->count;
                char buf[32];snprintf(buf,sizeof(buf),"+%d Gold",w->count);
                save_notify(note,buf);
            } else {
                if(inv_add(inv,w->type,w->count)){
                    char buf[48];snprintf(buf,sizeof(buf),"Picked up: %s",IDEF[w->type].name);
                    save_notify(note,buf);
                } else {
                    save_notify(note,"Inventory full!");
                    continue; /* don't consume item */
                }
            }
            w->active=false;
        }
    }
}

/* Draw world item icons + pickup prompt as HUD-space billboards */
static void wi_draw_hud(WorldItemList *wl,HUD *h,Vec3 player_pos,
                         Mat4 proj,Mat4 view,int win_w,int win_h,float total_time){
    (void)total_time;
    for(int i=0;i<wl->count;i++){
        WorldItem *w=&wl->items[i];if(!w->active)continue;
        float dist=vec3_dist(w->pos,player_pos);
        if(dist>12.0f)continue; /* cull far items */

        /* Project world pos to screen */
        Vec3 wp={w->pos.x,w->pos.y+0.6f+sinf(w->bob)*0.15f,w->pos.z};
        /* Manual MVP */
        float cx=proj.m[0]*(view.m[0]*wp.x+view.m[4]*wp.y+view.m[8]*wp.z+view.m[12])
                +proj.m[8]*(view.m[2]*wp.x+view.m[6]*wp.y+view.m[10]*wp.z+view.m[14]);
        float cy=proj.m[5]*(view.m[1]*wp.x+view.m[5]*wp.y+view.m[9]*wp.z+view.m[13])
                +proj.m[9]*(view.m[2]*wp.x+view.m[6]*wp.y+view.m[10]*wp.z+view.m[14]);
        float cw=        -(view.m[2]*wp.x+view.m[6]*wp.y+view.m[10]*wp.z+view.m[14]);
        if(cw<0.1f)continue; /* behind camera */
        /* Simple projection */
        float vx[4]={0};
        /* Use full MVP inline */
        float vx0=view.m[0]*wp.x+view.m[4]*wp.y+view.m[8]*wp.z+view.m[12];
        float vy0=view.m[1]*wp.x+view.m[5]*wp.y+view.m[9]*wp.z+view.m[13];
        float vz0=view.m[2]*wp.x+view.m[6]*wp.y+view.m[10]*wp.z+view.m[14];
        float vw0=1.0f;(void)vx0;(void)vy0;(void)vx;(void)cx;(void)cy;(void)cw;
        float px2=proj.m[0]*vx0+proj.m[12];
        float py2=proj.m[5]*vy0+proj.m[13];
        float pw2=-vz0;
        if(pw2<0.1f)continue;
        float sx2=( px2/pw2*0.5f+0.5f)*(float)win_w;
        float sy2=(-py2/pw2*0.5f+0.5f)*(float)win_h;(void)vz0;(void)vw0;

        const ItemDef *d=&IDEF[w->type];
        float alpha=clampf(1.0f-dist/12.0f,0,1);

        /* Icon box */
        float bsz=22;
        hud_rect(h,sx2-bsz*0.5f,sy2-bsz*0.5f,bsz,bsz, d->r,d->g,d->b,0.85f*alpha);
        char letter[2]={(char)d->name[0],0};
        hud_str(h,sx2-bsz*0.25f,sy2-bsz*0.35f,1.2f,letter,1,1,1,alpha);

        /* Pickup prompt */
        if(dist<PICKUP_RADIUS){
            if(!AUTO_PICKUP){
                hud_str(h,sx2-20,sy2-bsz-14,1.2f,"[E] Pick up",0.9f,0.9f,0.3f,1.0f);
            }
            hud_str(h,sx2-((float)strlen(d->name)*4.8f),sy2+bsz*0.5f+2,
                    1.1f,d->name,1.0f,0.9f,0.6f,alpha);
        }
    }
}

static void inv_drop(WorldItemList *wl,EnemyType etype,Vec3 pos,
                     QuestSystem *qs,SaveNotify *note){
    /* Gold always auto-collected immediately */
    int gold=5+(rand()%10);
    if(etype==ETYPE_BRUTE)gold+=15+(rand()%20);
    qs->player_gold+=gold;
    char gbuf[32];snprintf(gbuf,sizeof(gbuf),"+%d Gold",gold);
    save_notify(note,gbuf);

    /* Item drops spawn in world at enemy position */
    typedef struct{ItemType t;float chance;}Roll;
    static const Roll rolls_knight[]={
        {ITEM_POTION_HP,0.35f},{ITEM_SWORD_IRON,0.12f},{ITEM_CHEST_LEATHER,0.06f},{ITEM_NONE,0}};
    static const Roll rolls_archer[]={
        {ITEM_POTION_MP,0.35f},{ITEM_POTION_HP,0.20f},{ITEM_HELM_LEATHER,0.08f},{ITEM_NONE,0}};
    static const Roll rolls_brute[]={
        {ITEM_POTION_FULL,0.40f},{ITEM_SWORD_STEEL,0.15f},{ITEM_CHEST_IRON,0.10f},
        {ITEM_HELM_IRON,0.10f},{ITEM_SWORD_MAGIC,0.03f},{ITEM_NONE,0}};
    static const Roll rolls_shade[]={
        {ITEM_POTION_MP,0.30f},{ITEM_BOOTS_LEATHER,0.15f},{ITEM_POTION_HP,0.20f},{ITEM_NONE,0}};
    const Roll *tbl=
        etype==ETYPE_KNIGHT?rolls_knight:
        etype==ETYPE_ARCHER?rolls_archer:
        etype==ETYPE_BRUTE ?rolls_brute :rolls_shade;
    for(int r=0;tbl[r].t!=ITEM_NONE;r++){
        if((float)rand()/RAND_MAX<tbl[r].chance){
            wi_spawn(wl,tbl[r].t,1,pos);
            break;
        }
    }
}



/* Draw inventory panel (I key) */
static void inv_draw(Inventory *inv,HUD *h,int win_w,int win_h,int mx,int my){
    if(!inv->open)return;
    float W=(float)win_w,H=(float)win_h;
    int   cols=6,rows=4;
    float cell=52,pad=6;
    float pw=cols*(cell+pad)+pad+120; /* extra for equip panel */
    float ph=rows*(cell+pad)+pad+60;
    float px=(W-pw)*0.5f,py=(H-ph)*0.5f;

    /* Background */
    hud_rect(h,px-4,py-4,pw+8,ph+8, 0.15f,0.10f,0.08f,1.0f);
    hud_rect(h,px,py,pw,ph,          0.07f,0.05f,0.04f,0.95f);
    hud_str(h,px+8,py+8,1.8f,"INVENTORY", 0.9f,0.75f,0.3f,1.0f);
    hud_str(h,px+8,py+26,1.1f,"[I] Close  [Click] Use/Equip",0.5f,0.5f,0.5f,0.8f);

    /* Item grid */
    float gx=px+pad,gy=py+46;
    for(int i=0;i<INV_SLOTS;i++){
        int col=i%cols,row=i/cols;
        float sx=gx+col*(cell+pad),sy=gy+row*(cell+pad);
        bool hov=(mx>=sx&&mx<=sx+cell&&my>=sy&&my<=sy+cell);
        bool sel=(inv->selected==i);
        /* Slot background */
        hud_rect(h,sx,sy,cell,cell, 0.12f,0.10f,0.08f,0.9f);
        if(hov) hud_rect(h,sx,sy,cell,cell, 0.3f,0.25f,0.1f,0.5f);
        if(sel) hud_rect(h,sx-2,sy-2,cell+4,cell+4, 0.9f,0.75f,0.2f,0.8f);
        /* Border */
        hud_rect(h,sx,sy,cell,1,     0.3f,0.25f,0.15f,0.7f);
        hud_rect(h,sx,sy+cell-1,cell,1,0.3f,0.25f,0.15f,0.7f);
        hud_rect(h,sx,sy,1,cell,     0.3f,0.25f,0.15f,0.7f);
        hud_rect(h,sx+cell-1,sy,1,cell,0.3f,0.25f,0.15f,0.7f);
        /* Item icon — filled square + letter */
        InvSlot *s=&inv->slots[i];
        if(s->type!=ITEM_NONE){
            const ItemDef *d=&IDEF[s->type];
            float ic=cell*0.55f,io=(cell-ic)*0.5f;
            hud_rect(h,sx+io,sy+io,ic,ic, d->r,d->g,d->b,0.85f);
            /* First letter of name */
            char letter[2]={(char)d->name[0],0};
            hud_str(h,sx+io+ic*0.25f,sy+io+ic*0.15f,1.3f,letter,1,1,1,0.9f);
            /* Stack count */
            if(s->count>1){
                char cnt[4];snprintf(cnt,sizeof(cnt),"%d",s->count);
                hud_str(h,sx+cell-8-(int)strlen(cnt)*6,sy+cell-12,1.1f,cnt,1,1,0.5f,1);
            }
        }
    }

    /* Equipment panel — right side */
    float ex=gx+cols*(cell+pad)+4,ey=gy;
    const char *eslots[]={"WPN","HLM","BST","BOT"};
    for(int i=0;i<INV_EQUIP_SLOTS;i++){
        float sy=ey+i*(cell+pad);
        hud_str(h,ex,sy+cell*0.35f,1.1f,eslots[i],0.6f,0.6f,0.5f,0.8f);
        float ex2=ex+32;
        hud_rect(h,ex2,sy,cell,cell, 0.10f,0.08f,0.06f,0.9f);
        hud_rect(h,ex2,sy,cell,1,    0.5f,0.4f,0.2f,0.7f);
        hud_rect(h,ex2,sy+cell-1,cell,1,0.5f,0.4f,0.2f,0.7f);
        hud_rect(h,ex2,sy,1,cell,    0.5f,0.4f,0.2f,0.7f);
        hud_rect(h,ex2+cell-1,sy,1,cell,0.5f,0.4f,0.2f,0.7f);
        if(inv->equipped[i]!=ITEM_NONE){
            const ItemDef *d=&IDEF[inv->equipped[i]];
            float ic=cell*0.55f,io=(cell-ic)*0.5f;
            hud_rect(h,ex2+io,sy+io,ic,ic, d->r,d->g,d->b,0.9f);
            char letter[2]={(char)d->name[0],0};
            hud_str(h,ex2+io+ic*0.25f,sy+io+ic*0.15f,1.3f,letter,1,1,1,0.9f);
        }
    }

    /* Tooltip for hovered slot */
    for(int i=0;i<INV_SLOTS;i++){
        int col=i%cols,row=i/cols;
        float sx=gx+col*(cell+pad),sy=gy+row*(cell+pad);
        if(mx>=sx&&mx<=sx+cell&&my>=sy&&my<=sy+cell&&inv->slots[i].type!=ITEM_NONE){
            const ItemDef *d=&IDEF[inv->slots[i].type];
            float tw=1.3f*8*(float)strlen(d->desc)+16;
            float tx=sx+cell+4,ty=sy;
            if(tx+tw>W)tx=sx-tw-4;
            hud_rect(h,tx,ty,tw,34, 0,0,0,0.85f);
            hud_str(h,tx+6,ty+4, 1.3f,d->name, 1.0f,0.85f,0.4f,1.0f);
            hud_str(h,tx+6,ty+18,1.1f,d->desc, 0.8f,0.8f,0.8f,0.9f);
            break;
        }
    }
}

/* Handle mouse click on inventory */
static void inv_click(Inventory *inv,int mx,int my,int win_w,int win_h,
                      Player *pl,AbilitySystem *as,QuestSystem *qs,
                      float base_max_hp,SaveNotify *note){
    if(!inv->open)return;
    float W=(float)win_w,H=(float)win_h;
    int   cols=6;
    float cell=52,pad=6;
    float pw=cols*(cell+pad)+pad+120;
    float ph=4*(cell+pad)+pad+60;
    float px=(W-pw)*0.5f,py=(H-ph)*0.5f;
    float gx=px+pad,gy=py+46;
    for(int i=0;i<INV_SLOTS;i++){
        int col=i%cols,row=i/cols;
        float sx=gx+col*(cell+pad),sy=gy+row*(cell+pad);
        if(mx>=sx&&mx<=sx+cell&&my>=sy&&my<=sy+cell){
            inv_use(inv,i,pl,as,qs,base_max_hp,note);
            return;
        }
    }
}


/* ── Level System ────────────────────────────────────────────── */
/* XP needed to reach level N+1 from level N */
static int level_xp_required(int level){
    /* 100, 250, 450, 700, 1000, 1350 … quadratic growth */
    return 100 + (level-1)*150 + (level-1)*(level-1)*25;
}

/* LevelSystem typedef forward-declared near hud_draw */

static void level_init(LevelSystem *ls){memset(ls,0,sizeof(*ls));}

/* Call each frame after xp changes — returns true if levelled up */
static bool level_update(LevelSystem *lvl,QuestSystem *qs,
                         Player *pl,AbilitySystem *as,
                         float base_max_hp,Inventory *inv,
                         SaveNotify *note,float dt){
    lvl->flash =clampf(lvl->flash -dt*2.0f,0,1);
    lvl->banner=clampf(lvl->banner-dt,     0,4);
    bool levelled=false;
    while(qs->player_xp>=level_xp_required(qs->player_level)){
        qs->player_xp-=level_xp_required(qs->player_level);
        qs->player_level++;
        /* Stat gains per level */
        lvl->dmg_bonus  +=3.0f;
        lvl->hp_bonus   +=20.0f;
        lvl->mp_bonus   +=15.0f;
        lvl->regen_bonus+=0.5f;
        /* Apply immediately */
        as->max_mana  =100.0f+lvl->mp_bonus;
        as->mp_regen  =3.0f +lvl->regen_bonus;
        as->sp_regen  =8.0f +lvl->regen_bonus;
        inv_recalc(inv,pl,base_max_hp+lvl->hp_bonus);
        pl->health=pl->max_health; /* full heal on level up */
        as->mana  =as->max_mana;
        lvl->flash=1.0f;
        lvl->banner=3.5f;
        char buf[48];snprintf(buf,sizeof(buf),"Level Up!  Now level %d",qs->player_level);
        save_notify(note,buf);
        audio_play(SND_LEVEL_UP,110);
        printf("[Level] Now level %d  dmg+%.0f  hp+%.0f\n",
               qs->player_level,lvl->dmg_bonus,lvl->hp_bonus);
        levelled=true;
    }
    return levelled;
}

/* Draw XP bar + level-up banner */
static void level_draw(LevelSystem *lvl,QuestSystem *qs,HUD *h,
                       float bx,float by,float W,float H){
    int needed=level_xp_required(qs->player_level);
    float pct=(needed>0)?(float)qs->player_xp/(float)needed:1.0f;
    /* XP bar just below stamina bar */
    hud_str(h,bx,by+50,1.0f,"XP",0.6f,0.4f,0.9f,0.9f);
    hud_bar(h,bx+22,by+60,220,10,pct, 0.6f,0.3f,0.9f,0.05f,0.05f,0.10f);
    /* Level badge */
    char lvlbuf[12];snprintf(lvlbuf,sizeof(lvlbuf),"Lv.%d",qs->player_level);
    hud_str(h,bx+248,by+57,1.2f,lvlbuf,0.7f,0.5f,1.0f,0.95f);

    /* Level-up flash overlay */
    if(lvl->flash>0)
        hud_rect(h,0,0,W,H, 0.7f,0.6f,1.0f,lvl->flash*0.35f);

    /* "LEVEL UP!" banner */
    if(lvl->banner>0){
        float a=clampf(lvl->banner*0.8f,0,1);
        float tw=4.5f*8.0f*10.0f; /* approx width of "LEVEL UP!" at scale 4.5 */
        hud_rect(h,W*0.5f-tw*0.5f-12,H*0.35f-8,tw+24,52, 0,0,0,0.65f*a);
        hud_str(h,W*0.5f-tw*0.5f,H*0.35f,4.5f,"LEVEL UP!", 0.8f,0.5f,1.0f,a);
        char lvlbuf2[32];snprintf(lvlbuf2,sizeof(lvlbuf2),"Now level %d",qs->player_level);
        hud_str(h,W*0.5f-60,H*0.35f+42,1.6f,lvlbuf2,0.9f,0.85f,1.0f,a*0.9f);
    }
}

/* ── Fishing System ─────────────────────────────────────────── */
typedef struct{
    bool  active;
    bool  near_water;
    float water_level;
    Vec3  spot;
    float cast_anim;
    float cursor;
    float cursor_vel;
    float target;
    float target_w;
    float retarget_timer;
    int   misses;
    int   xp;
    int   level;
    float banner;
    ItemType last_catch;
}FishingSystem;

static int fishing_xp_required(int level){
    return 2 + (level-1)*2;
}

static void fishing_init(FishingSystem *fs){
    memset(fs,0,sizeof(*fs));
    fs->level=1;
    fs->cursor_vel=0.75f;
    fs->target=0.5f;
    fs->target_w=0.20f;
    fs->spot=(Vec3){0,0,0};
    fs->last_catch=ITEM_FISH_RAW;
}

static bool fishing_find_water(Vec3 pos,float *out_level,Vec3 *out_spot){
    static const float offs[][2]={
        { 0.0f, 2.0f},{ 1.5f, 1.5f},{ 2.2f, 0.0f},{ 1.5f,-1.5f},
        { 0.0f,-2.2f},{-1.5f,-1.5f},{-2.2f, 0.0f},{-1.5f, 1.5f},
        { 0.0f, 3.5f},{ 2.5f, 2.5f},{ 3.5f, 0.0f},{-2.5f, 2.5f}
    };
    float best=1e9f;
    bool found=false;
    for(size_t i=0;i<sizeof(offs)/sizeof(offs[0]);i++){
        float sx=pos.x+offs[i][0], sz=pos.z+offs[i][1];
        float wl=WATER_LEVEL;
        if(!water_surface_info(sx,sz,&wl,NULL)) continue;
        float th=terrain_h(sx,sz);
        if(th>wl+0.05f) continue;
        float dx=sx-pos.x, dz=sz-pos.z;
        float d2=dx*dx+dz*dz;
        if(d2<best){
            best=d2;
            found=true;
            if(out_level) *out_level=wl;
            if(out_spot)  *out_spot =(Vec3){sx,wl,sz};
        }
    }
    return found;
}

static void fishing_retarget(FishingSystem *fs){
    float lvl_t=clampf((float)(fs->level-1)/9.0f,0.0f,1.0f);
    fs->target=0.18f + (float)rand()/(float)RAND_MAX*0.64f;
    fs->target_w=0.18f - lvl_t*0.06f + ((float)rand()/(float)RAND_MAX)*0.03f;
    fs->target_w=clampf(fs->target_w,0.10f,0.22f);
    fs->retarget_timer=0.85f + (float)rand()/(float)RAND_MAX*1.0f;
    fs->cursor_vel=0.85f + (float)rand()/(float)RAND_MAX*(0.55f+lvl_t*0.30f);
}

static void fishing_start(FishingSystem *fs,Vec3 spot,float water_level){
    fs->active=true;
    fs->spot=spot;
    fs->water_level=water_level;
    fs->cast_anim=0.42f;
    fs->cursor=0.12f;
    fs->misses=0;
    fs->banner=0.0f;
    fishing_retarget(fs);
}

static void fishing_stop(FishingSystem *fs){
    fs->active=false;
}

static void fishing_update(FishingSystem *fs,float dt){
    if(!fs->active) return;
    fs->cast_anim=clampf(fs->cast_anim-dt,0.0f,1.0f);
    fs->cursor += fs->cursor_vel*dt;
    if(fs->cursor>=1.0f){ fs->cursor=1.0f; fs->cursor_vel=-fabsf(fs->cursor_vel); }
    if(fs->cursor<=0.0f){ fs->cursor=0.0f; fs->cursor_vel= fabsf(fs->cursor_vel); }
    fs->retarget_timer-=dt;
    if(fs->retarget_timer<=0.0f) fishing_retarget(fs);
    fs->banner=clampf(fs->banner-dt,0.0f,3.0f);
}

static int fishing_water_preset(Vec3 pos){
    int best_idx=g_wstyle.preset_idx>=0?g_wstyle.preset_idx:0;
    water_surface_info(pos.x,pos.z,NULL,&best_idx);
    return best_idx;
}

static ItemType fishing_choose_catch(FishingSystem *fs,Vec3 pos){
    float r=(float)rand()/(float)RAND_MAX;
    float skill_t=clampf((float)(fs->level-1)/12.0f,0.0f,1.0f);
    int preset=fishing_water_preset(pos);
    BiomeType biome=get_biome(pos);

    if(preset==6 || preset==1){
        if(r < 0.08f + skill_t*0.10f) return ITEM_FISH_GOLDEN;
        if(r < 0.42f + skill_t*0.10f) return ITEM_FISH_TROUT;
        return ITEM_FISH_RAW;
    }
    if(preset==2 || biome==BIOME_FOREST){
        if(r < 0.12f + skill_t*0.08f) return ITEM_FISH_GOLDEN;
        if(r < 0.62f + skill_t*0.12f) return ITEM_FISH_TROUT;
        return ITEM_FISH_RAW;
    }
    if(biome==BIOME_MOUNTAIN || biome==BIOME_SNOW){
        if(r < 0.70f + skill_t*0.12f) return ITEM_FISH_TROUT;
        if(r < 0.78f + skill_t*0.08f) return ITEM_FISH_GOLDEN;
        return ITEM_FISH_RAW;
    }
    if(preset==0){
        if(r < 0.05f + skill_t*0.08f) return ITEM_FISH_GOLDEN;
        if(r < 0.35f + skill_t*0.12f) return ITEM_FISH_TROUT;
        return ITEM_FISH_RAW;
    }
    if(r < 0.16f + skill_t*0.08f) return ITEM_FISH_GOLDEN;
    if(r < 0.55f + skill_t*0.12f) return ITEM_FISH_TROUT;
    return ITEM_FISH_RAW;
}

static void fishing_gain(FishingSystem *fs,Inventory *inv,SaveNotify *note){
    ItemType catch_type=fishing_choose_catch(fs,fs->spot);
    fs->last_catch=catch_type;
    inv_add(inv,catch_type,1);
    fs->xp++;
    fs->banner=1.6f;
    { char buf[80]; snprintf(buf,sizeof(buf),"Caught %s",IDEF[catch_type].name); save_notify(note,buf); }
    while(fs->xp>=fishing_xp_required(fs->level)){
        fs->xp-=fishing_xp_required(fs->level);
        fs->level++;
        fs->banner=3.2f;
        char buf[64];
        snprintf(buf,sizeof(buf),"Fishing level up! Now level %d",fs->level);
        save_notify(note,buf);
    }
}

static void fishing_world_draw(FishingSystem *fs,LineRenderer *lr,Mat4 proj,Mat4 view,Vec3 player){
    if(!fs->active) return;
    Vec3 rod={player.x,player.y+1.25f,player.z};
    Vec3 bob={fs->spot.x,fs->water_level+0.06f,fs->spot.z};
    float pulse=0.06f+sinf((float)SDL_GetTicks()*0.010f)*0.015f;
    lr_push(rod,bob,0.92f,0.92f,0.86f);
    lr_push((Vec3){bob.x-pulse,bob.y,bob.z},(Vec3){bob.x+pulse,bob.y,bob.z},1.0f,0.35f,0.30f);
    lr_push((Vec3){bob.x,bob.y,bob.z-pulse},(Vec3){bob.x,bob.y,bob.z+pulse},1.0f,0.35f,0.30f);
    lr_push((Vec3){bob.x,bob.y-0.10f,bob.z},(Vec3){bob.x,bob.y+0.10f,bob.z},1.0f,0.55f,0.40f);
    lr_flush(lr,proj,view);
}

static void fishing_draw(FishingSystem *fs,HUD *h,int win_w,int win_h){
    if(fs->near_water&&!fs->active){
        const char *msg="[E] Start Fishing";
        float w=1.7f*8.0f*(float)strlen(msg);
        float x=(float)win_w*0.5f-w*0.5f;
        float y=(float)win_h*0.68f;
        hud_rect(h,x-14,y-8,w+28,28,0,0,0,0.68f);
        hud_str(h,x,y,1.7f,msg,0.78f,0.92f,1.0f,0.96f);
    }
    if(!fs->active) return;

    float W=420.0f,H=132.0f;
    float x=(float)win_w*0.5f-W*0.5f;
    float y=(float)win_h*0.64f;
    float bar_x=x+28.0f, bar_y=y+52.0f, bar_w=W-56.0f, bar_h=24.0f;
    float zone_x=bar_x + (fs->target-fs->target_w*0.5f)*bar_w;
    float zone_w=fs->target_w*bar_w;
    float cur_x=bar_x + fs->cursor*bar_w;
    char lvl[48];
    char miss[32];
    snprintf(lvl,sizeof(lvl),"Fishing Lv.%d  XP %d/%d",
             fs->level,fs->xp,fishing_xp_required(fs->level));
    snprintf(miss,sizeof(miss),"Misses: %d / 3",fs->misses);

    hud_rect(h,x,y,W,H,0.02f,0.04f,0.08f,0.82f);
    hud_rect(h,bar_x,bar_y,bar_w,bar_h,0.10f,0.12f,0.18f,0.95f);
    hud_rect(h,zone_x,bar_y,zone_w,bar_h,0.18f,0.65f,0.78f,0.95f);
    hud_rect(h,cur_x-3.0f,bar_y-5.0f,6.0f,bar_h+10.0f,1.0f,0.97f,0.72f,0.98f);
    {
        float fx=zone_x+zone_w*0.5f, fy=bar_y-15.0f;
        hud_rect(h,fx-8.0f,fy-3.0f,14.0f,6.0f,0.40f,0.82f,0.92f,0.95f);
        hud_rect(h,fx+4.0f,fy-6.0f,6.0f,12.0f,0.40f,0.82f,0.92f,0.95f);
        hud_rect(h,fx-12.0f,fy-2.0f,5.0f,2.0f,0.40f,0.82f,0.92f,0.95f);
        hud_rect(h,fx-12.0f,fy+1.0f,5.0f,2.0f,0.40f,0.82f,0.92f,0.95f);
        hud_rect(h,fx+1.0f,fy-1.0f,2.0f,2.0f,0.06f,0.12f,0.18f,0.95f);
    }
    hud_str(h,x+22.0f,y+14.0f,1.7f,"Fishing",0.88f,0.95f,1.0f,1.0f);
    hud_str(h,x+22.0f,y+96.0f,1.25f,"Press SPACE when the marker is over the fish",0.78f,0.82f,0.90f,0.95f);
    hud_str(h,x+210.0f,y+16.0f,1.2f,lvl,0.72f,0.88f,0.96f,0.95f);
    hud_str(h,x+22.0f,y+32.0f,1.15f,miss,1.0f,0.72f,0.60f,0.95f);

    if(fs->banner>0.01f){
        float a=clampf(fs->banner/1.2f,0.0f,1.0f);
        hud_str(h,x+W*0.5f-78.0f,y-24.0f,1.45f,IDEF[fs->last_catch].name,1.0f,0.92f,0.55f,a);
    }
}

#define SAVE_SLOTS     3
#define SAVE_MAGIC     0x4D505247  /* "MRPG" */
#define SAVE_VERSION   5
#define SAVE_DIR       "saves"

/* Everything that gets written to disk */
typedef struct{
    uint32_t magic,version;
    /* Player */
    float    px,py,pz,pyaw;
    float    health,max_health;
    /* Resources */
    float    mana,max_mana,stam,max_stam;
    /* Ability cooldowns */
    float    ab_cd_timer[HOTBAR_SLOTS];
    bool     ab_ready[HOTBAR_SLOTS];
    /* Progression */
    int      gold,xp,level;
    /* Quests */
    int      quest_count;
    struct{
        int  id;
        int  status;           /* 0=inactive 1=active 2=done */
        int  obj_current[MAX_OBJECTIVES];
        bool obj_done[MAX_OBJECTIVES];
    }quests[MAX_QUESTS];
    /* Enemies — just alive/dead + position so world stays consistent */
    int      ne;
    struct{
        bool     active;
        int      etype;
        float    hx,hy,hz;
        float    health;
        int      state;       /* ES_DEAD etc */
    }enemies[MAX_ENEMIES];
    /* World time */
    float    world_time;
    /* Inventory */
    struct{ int type; int count; }inv_slots[INV_SLOTS];
    int      inv_equipped[INV_EQUIP_SLOTS];
    /* Level system bonuses */
    float    lvl_dmg_bonus,lvl_hp_bonus,lvl_mp_bonus,lvl_regen_bonus;
    /* Padding for future fields */
    uint8_t  _pad[128];
}SaveData;

static bool save_mkdir(void){
#ifdef _WIN32
    _mkdir(SAVE_DIR);
#else
    mkdir(SAVE_DIR,0755);
#endif
    return true;
}

static void save_path(char *buf,size_t sz,int slot){
    snprintf(buf,sz,SAVE_DIR"/slot%d.sav",slot);
}

/* Format world_time (seconds) as HH:MM */
static void fmt_playtime(float t,char *buf,int bufsz){
    int h=(int)(t/3600),m=(int)(t/60)%60;
    snprintf(buf,bufsz,"%dh %02dm",h,m);
}

/* Draw one save slot panel. Returns true if clicked. */
static bool slot_panel(HUD *h,float x,float y,float w,float ht,
                        int slot_idx,int mx,int my,bool *clicked){
    char path[64];save_path(path,sizeof(path),slot_idx);
    FILE *f=fopen(path,"rb");
    bool has_save=false; int level=0; float world_time=0;float hp=0;
    if(f){
        SaveData sd;
        if(fread(&sd,sizeof(sd),1,f)==1&&sd.magic==SAVE_MAGIC&&sd.version==SAVE_VERSION){
            has_save=true;level=(int)sd.level;world_time=sd.world_time;hp=sd.health;
        }
        fclose(f);
    }
    bool over=(mx>=x&&mx<=x+w&&my>=y&&my<=y+ht);
    /* Outer glow on hover */
    if(over)hud_rect(h,x-3,y-3,w+6,ht+6, 0.8f,0.65f,0.1f,0.7f);
    /* Panel BG */
    float bg=over?0.18f:0.10f;
    hud_rect(h,x,y,w,ht, bg,bg*0.7f,bg*0.4f,0.92f);
    /* Border */
    float bc=has_save?0.6f:0.3f;
    hud_rect(h,x,y,w,2,     bc,bc*0.7f,0.1f,1);
    hud_rect(h,x,y+ht-2,w,2,bc,bc*0.7f,0.1f,1);
    hud_rect(h,x,y,2,ht,    bc,bc*0.7f,0.1f,1);
    hud_rect(h,x+w-2,y,2,ht,bc,bc*0.7f,0.1f,1);
    /* Slot label */
    char slabel[16];snprintf(slabel,sizeof(slabel),"SLOT %d",slot_idx+1);
    float lsc=1.6f,ltw=lsc*8.0f*(float)strlen(slabel);
    hud_str(h,x+(w-ltw)*0.5f,y+10,lsc,slabel,1.0f,0.85f,0.3f,1.0f);
    if(has_save){
        /* Level */
        char lbuf[32];snprintf(lbuf,sizeof(lbuf),"Level %d",level);
        float lbsc=2.2f,lbtw=lbsc*8.0f*(float)strlen(lbuf);
        hud_str(h,x+(w-lbtw)*0.5f,y+34,lbsc,lbuf, 1.0f,0.95f,0.8f,1.0f);
        /* HP */
        char hbuf[32];snprintf(hbuf,sizeof(hbuf),"HP: %.0f",hp);
        float hsc=1.3f,htw=hsc*8.0f*(float)strlen(hbuf);
        hud_str(h,x+(w-htw)*0.5f,y+60,hsc,hbuf, 0.8f,0.9f,0.7f,0.9f);
        /* Playtime */
        char tbuf[32];fmt_playtime(world_time,tbuf,sizeof(tbuf));
        float tsc2=1.3f,ttw=tsc2*8.0f*(float)strlen(tbuf);
        hud_str(h,x+(w-ttw)*0.5f,y+78,tsc2,tbuf, 0.7f,0.75f,0.8f,0.85f);
        /* Load label */
        const char *act="CONTINUE";
        float asc=1.5f,atw=asc*8.0f*(float)strlen(act);
        hud_str(h,x+(w-atw)*0.5f,y+ht-22,asc,act, 0.9f,0.8f,0.3f,over?1.0f:0.7f);
    } else {
        const char *empty="— Empty —";
        float esc=1.5f,etw=esc*8.0f*(float)strlen(empty);
        hud_str(h,x+(w-etw)*0.5f,y+ht*0.38f,esc,empty, 0.5f,0.5f,0.5f,0.7f);
        const char *ns="NEW GAME";
        float nsc=1.5f,ntw=nsc*8.0f*(float)strlen(ns);
        hud_str(h,x+(w-ntw)*0.5f,y+ht-22,nsc,ns, 0.7f,0.9f,0.4f,over?1.0f:0.6f);
    }
    if(clicked)*clicked=(over&&SDL_GetMouseState(NULL,NULL)&SDL_BUTTON(1));
    return over;
}

/* ── Menu page enum ──────────────────────────────────────────── */
/* ═══════════════════════════════════════════════════════════════
   PAUSE MENU
   ═══════════════════════════════════════════════════════════════ */
typedef enum{ PM_NONE, PM_ROOT, PM_SAVE_SLOTS, PM_OPTIONS }PauseState;

typedef struct{
    PauseState state;
    bool       lmb_prev;
    int        active_slider;
}PauseMenu;

static PauseMenu g_pause={PM_NONE,false,-1};

/* Returns true if a click was consumed this frame */

static bool game_save(int slot,Player *pl,QuestSystem *qs,AbilitySystem *as,
                      Enemy *enemies,int ne,float world_time,Inventory *inv,
                      LevelSystem *lvl); /* forward decl */

/* Forward declaration — defined later in the menu section */
static void page_options(HUD *h,float W,float H,int mx,int my,bool clicked,
                         Options *opt,bool held,int *active_slider,
                         bool *back,bool *credits);

static bool pause_draw(PauseMenu *pm,HUD *h,int win_w,int win_h,
                       /* game refs for save/quit: */
                       Player *player,QuestSystem *qs,AbilitySystem *as,
                       Enemy *enemies,int ne,float world_time,Inventory *inv,
                       LevelSystem *lvl,SaveNotify *note,
                       bool *want_resume,bool *want_quit_menu,bool *want_quit_game){
    if(pm->state==PM_NONE)return false;
    float W=(float)win_w,H=(float)win_h;
    int mx,my; bool lmb=(SDL_GetMouseState(&mx,&my)&SDL_BUTTON(1))!=0;
    bool clicked=lmb&&!pm->lmb_prev;
    pm->lmb_prev=lmb;
    if(!lmb) pm->active_slider=-1;

    /* Dark translucent overlay */
    hud_rect(h,0,0,W,H, 0,0,0,0.60f);

    float pw=320,ph=0,px=(W-pw)*0.5f;
    float bw=pw-40,bh=44,bx=px+20;
    float gap=12;

    if(pm->state==PM_ROOT){
        ph=5*bh+4*gap+80; float py=(H-ph)*0.5f;
        /* Panel */
        hud_rect(h,px,py,pw,ph, 0.06f,0.04f,0.02f,0.95f);
        hud_rect(h,px,py,pw,4,  0.8f,0.6f,0.1f,1); /* top gold bar */

        const char *title="PAUSED";
        float tsc=2.4f,tw=tsc*8.0f*(float)strlen(title);
        hud_str(h,px+(pw-tw)*0.5f,py+14,tsc,title,1.0f,0.85f,0.2f,1.0f);

        float by_=py+56;
        bool c=false;
        /* RESUME */
        menu_btn(h,bx,by_,bw,bh,"RESUME",1.8f,mx,my,true,&c);
        if(c&&clicked){pm->state=PM_NONE;*want_resume=true;audio_play(SND_MENU_CLICK,70);}
        by_+=bh+gap;
        /* SAVE GAME */
        menu_btn(h,bx,by_,bw,bh,"SAVE GAME",1.8f,mx,my,true,&c);
        if(c&&clicked){pm->state=PM_SAVE_SLOTS;audio_play(SND_MENU_CLICK,70);}
        by_+=bh+gap;
        /* OPTIONS */
        menu_btn(h,bx,by_,bw,bh,"OPTIONS",1.8f,mx,my,true,&c);
        if(c&&clicked){ pm->state=PM_OPTIONS; audio_play(SND_MENU_CLICK,70); }
        by_+=bh+gap;
        /* QUIT TO MENU */
        menu_btn(h,bx,by_,bw,bh,"QUIT TO MENU",1.8f,mx,my,true,&c);
        if(c&&clicked){pm->state=PM_NONE;*want_quit_menu=true;options_save(&g_options);audio_play(SND_MENU_CLICK,70);}
        by_+=bh+gap;
        /* QUIT GAME */
        menu_btn(h,bx,by_,bw,bh,"QUIT GAME",1.8f,mx,my,true,&c);
        if(c&&clicked){pm->state=PM_NONE;*want_quit_game=true;options_save(&g_options);audio_play(SND_MENU_CLICK,70);}

    } else if(pm->state==PM_SAVE_SLOTS){
        ph=3*bh+2*gap+100; float py=(H-ph)*0.5f;
        hud_rect(h,px,py,pw,ph, 0.06f,0.04f,0.02f,0.95f);
        hud_rect(h,px,py,pw,4,  0.8f,0.6f,0.1f,1);

        const char *title="SAVE TO SLOT";
        float tsc=1.8f,tw=tsc*8.0f*(float)strlen(title);
        hud_str(h,px+(pw-tw)*0.5f,py+14,tsc,title,1.0f,0.85f,0.2f,1.0f);

        float by_=py+52;
        for(int s=0;s<3;s++){
            char lbl[32];snprintf(lbl,sizeof(lbl),"SLOT %d",s+1);
            bool c2=false;
            menu_btn(h,bx,by_,bw,bh,lbl,1.8f,mx,my,true,&c2);
            if(c2&&clicked){
                game_save(s,player,qs,as,enemies,ne,world_time,inv,lvl);
                char nbuf[32];snprintf(nbuf,sizeof(nbuf),"Saved — Slot %d",s+1);
                save_notify(note,nbuf);
                audio_play(SND_MENU_CLICK,80);
                pm->state=PM_ROOT;
            }
            by_+=bh+gap;
        }
        /* BACK */
        bool cb=false;
        hud_str(h,px+10,by_+14,1.2f,"< BACK",0.7f,0.6f,0.3f,0.85f);
        bool over_back=(mx>=px&&mx<=px+80&&my>=by_+8&&my<=by_+8+20);
        if(over_back&&clicked){pm->state=PM_ROOT;audio_play(SND_MENU_CLICK,70);}
        (void)cb;

    } else if(pm->state==PM_OPTIONS){
        /* ── In-game settings overlay ─── */
        /* Reuse page_options but drawn over a dim overlay.
           page_options calls hud_flush internally so we call it here
           and then handle back/save ourselves.                         */
        bool back_opt=false, credits_ign=false;
        /* We need a window handle to apply fullscreen/vsync — forward
           declare it via SDL_GL_GetCurrentWindow().                    */
        Options opt_prev=g_options;
        page_options(h,(float)win_w,(float)win_h,mx,my,clicked,
                     &g_options,lmb,&pm->active_slider,&back_opt,&credits_ign);
        /* Apply audio live */
        audio_set_volumes(g_options.music_vol,g_options.sfx_vol);
        SDL_GL_SetSwapInterval(g_options.vsync?1:0);
        if(g_options.fullscreen!=opt_prev.fullscreen)
            SDL_SetWindowFullscreen(SDL_GL_GetCurrentWindow(),
                g_options.fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0);
        if(clicked&&back_opt){
            options_save(&g_options);
            pm->state=PM_ROOT;
            audio_play(SND_MENU_CLICK,70);
        }
        return true; /* page_options already flushed hud */
    }

    hud_flush(h);
    return true;
}

/* ═══════════════════════════════════════════════════════════════
   INTERIOR SPACES
   ═══════════════════════════════════════════════════════════════ */
#define INTERIOR_MAX      6
#define INTERIOR_PROP_MAX 12

typedef struct{
    Vec3  offset;   /* relative to room center */
    float yaw,scale;
    const char *path;
}IntProp;

typedef struct{
    const char *name;       /* location label shown on enter */
    Vec3  center;           /* world-space room centre */
    float hw,hh,hd;         /* half-extents: width, height, depth */
    /* Visual palette */
    float wall_r,wall_g,wall_b;
    float floor_r,floor_g,floor_b;
    float ceil_r, ceil_g, ceil_b;
    /* Door: world-space position of the entrance */
    Vec3  door;
    /* Interior ambient light colour */
    float amb_r,amb_g,amb_b;
    /* Props placed inside */
    IntProp props[INTERIOR_PROP_MAX];
    int prop_count;
}InteriorDef;

/* GL resources for the room box */
typedef struct{
    GLuint prog,vao,vbo;
    int vcount;
}RoomRenderer;

static InteriorDef g_interiors[INTERIOR_MAX];
static int         g_interior_count=0;
static int         g_interior_idx=-1; /* -1 = outside */
static RoomRenderer g_room_rend;

/* Add an interior — returns index */
static int interior_add(const char *name,Vec3 center,float hw,float hh,float hd,
                        float wr,float wg,float wb,
                        float fr,float fg,float fb,
                        float cr,float cg,float cb,
                        Vec3 door,
                        float ar,float ag,float ab){
    if(g_interior_count>=INTERIOR_MAX)return -1;
    InteriorDef *d=&g_interiors[g_interior_count];
    d->name=name; d->center=center;
    d->hw=hw; d->hh=hh; d->hd=hd;
    d->wall_r=wr; d->wall_g=wg; d->wall_b=wb;
    d->floor_r=fr; d->floor_g=fg; d->floor_b=fb;
    d->ceil_r=cr;  d->ceil_g=cg;  d->ceil_b=cb;
    d->door=door;
    d->amb_r=ar; d->amb_g=ag; d->amb_b=ab;
    d->prop_count=0;
    return g_interior_count++;
}
static void interior_add_prop(int idx,Vec3 offset,float yaw,float scale,const char *path){
    if(idx<0||idx>=g_interior_count)return;
    InteriorDef *d=&g_interiors[idx];
    if(d->prop_count>=INTERIOR_PROP_MAX)return;
    IntProp *p=&d->props[d->prop_count++];
    p->offset=offset; p->yaw=yaw; p->scale=scale; p->path=path;
}

/* Build vertex buffer: 6 faces of box, each a quad (2 triangles).
   Vertex layout: x,y,z,nx,ny,nz,r,g,b (9 floats) */
static void room_renderer_init(RoomRenderer *rr){
    const char *vs=
        "#version 330 core\n"
        "layout(location=0)in vec3 aPos;\n"
        "layout(location=1)in vec3 aNorm;\n"
        "layout(location=2)in vec3 aColor;\n"
        "uniform mat4 uProj,uView;\n"
        "out vec3 vColor;out vec3 vNorm;out vec3 vPos;\n"
        "void main(){\n"
        "  gl_Position=uProj*uView*vec4(aPos,1.0);\n"
        "  vPos=aPos;vNorm=aNorm;vColor=aColor;\n"
        "}\n";
    const char *fs=
        "#version 330 core\n"
        "in vec3 vColor;in vec3 vNorm;in vec3 vPos;\n"
        "uniform vec3 uLightDir;uniform vec3 uAmb;\n"
        "out vec4 F;\n"
        "void main(){\n"
        "  float d=max(dot(normalize(vNorm),normalize(-uLightDir)),0.0);\n"
        "  vec3 col=vColor*(uAmb+d*0.55);\n"
        "  /* Torchlight flicker — fake with distance from centre */\n"
        "  float dist=length(vPos);\n"
        "  col=min(col*1.05,vec3(1.0));\n"
        "  F=vec4(col,1.0);\n"
        "}\n";
    rr->prog=GLH_program(vs,fs);
    glGenVertexArrays(1,&rr->vao);
    glGenBuffers(1,&rr->vbo);
    glBindVertexArray(rr->vao);
    glBindBuffer(GL_ARRAY_BUFFER,rr->vbo);
    /* 6 faces * 6 verts (2 tris) * 9 floats = 324 floats max */
    glBufferData(GL_ARRAY_BUFFER,324*sizeof(float),NULL,GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,36,(void*)0);
    glEnableVertexAttribArray(1);glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,36,(void*)12);
    glEnableVertexAttribArray(2);glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,36,(void*)24);
    glBindVertexArray(0);
    rr->vcount=0;
}

/* Upload room geometry for a given interior def */
static void room_upload(RoomRenderer *rr,const InteriorDef *d){
    float cx=d->center.x,cy=d->center.y,cz=d->center.z;
    float hw=d->hw,hh=d->hh,hd=d->hd;
    /* 6 faces: floor, ceil, N, S, E, W */
    /* Each face: 6 verts (2 tris), format: x,y,z, nx,ny,nz, r,g,b */
    float verts[6*6*9];
    int vi=0;
    #define V(px,py,pz,nx,ny,nz,r,g,b) \
        verts[vi++]=(px);verts[vi++]=(py);verts[vi++]=(pz); \
        verts[vi++]=(nx);verts[vi++]=(ny);verts[vi++]=(nz); \
        verts[vi++]=(r); verts[vi++]=(g); verts[vi++]=(b)
    #define QUAD(ax,ay,az, bx,by,bz, cx2,cy2,cz2, dx,dy,dz, nx,ny,nz, r,g,b) \
        V(ax,ay,az,nx,ny,nz,r,g,b);V(bx,by,bz,nx,ny,nz,r,g,b);V(cx2,cy2,cz2,nx,ny,nz,r,g,b); \
        V(ax,ay,az,nx,ny,nz,r,g,b);V(cx2,cy2,cz2,nx,ny,nz,r,g,b);V(dx,dy,dz,nx,ny,nz,r,g,b)
    float x0=cx-hw,x1=cx+hw,y0=cy-hh,y1=cy+hh,z0=cz-hd,z1=cz+hd;
    float wr=d->wall_r,wg=d->wall_g,wb=d->wall_b;
    float fr=d->floor_r,fg=d->floor_g,fb=d->floor_b;
    float cr=d->ceil_r, cg=d->ceil_g, cb=d->ceil_b;
    /* Floor */
    QUAD(x0,y0,z0, x1,y0,z0, x1,y0,z1, x0,y0,z1, 0,1,0, fr,fg,fb);
    /* Ceiling */
    QUAD(x0,y1,z1, x1,y1,z1, x1,y1,z0, x0,y1,z0, 0,-1,0, cr,cg,cb);
    /* North wall (z=z0, facing +z) */
    QUAD(x0,y0,z0, x0,y1,z0, x1,y1,z0, x1,y0,z0, 0,0,1, wr,wg,wb);
    /* South wall (z=z1, facing -z) */
    QUAD(x1,y0,z1, x1,y1,z1, x0,y1,z1, x0,y0,z1, 0,0,-1, wr,wg,wb);
    /* East wall (x=x1, facing -x) */
    QUAD(x1,y0,z0, x1,y1,z0, x1,y1,z1, x1,y0,z1, -1,0,0, wr*0.85f,wg*0.85f,wb*0.85f);
    /* West wall (x=x0, facing +x) */
    QUAD(x0,y0,z1, x0,y1,z1, x0,y1,z0, x0,y0,z0, 1,0,0, wr*0.85f,wg*0.85f,wb*0.85f);
    #undef V
    #undef QUAD
    glBindVertexArray(rr->vao);
    glBindBuffer(GL_ARRAY_BUFFER,rr->vbo);
    glBufferSubData(GL_ARRAY_BUFFER,0,vi*sizeof(float),verts);
    glBindVertexArray(0);
    rr->vcount=vi/9; /* 9 floats per vertex */
}

static void room_draw(RoomRenderer *rr,const InteriorDef *d,Mat4 proj,Mat4 view){
    if(!rr->prog)return;
    room_upload(rr,d);
    glUseProgram(rr->prog);
    GLH_mat4(rr->prog,"uProj",proj.m); GLH_mat4(rr->prog,"uView",view.m);
    /* Fake torchlight from above-centre */
    GLH_vec3(rr->prog,"uLightDir", 0.2f,-1.0f,0.3f);
    GLH_vec3(rr->prog,"uAmb", d->amb_r,d->amb_g,d->amb_b);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(rr->vao);
    glDrawArrays(GL_TRIANGLES,0,rr->vcount);
    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
}

/* Check if player is inside any interior AABB; update g_interior_idx */
static float g_interior_enter_timer=0; /* fades in banner on entry */
static void interior_check(Vec3 player_pos){
    int prev=g_interior_idx;
    g_interior_idx=-1;
    for(int i=0;i<g_interior_count;i++){
        InteriorDef *d=&g_interiors[i];
        if(fabsf(player_pos.x-d->center.x)<d->hw&&
           player_pos.y>d->center.y-d->hh&&player_pos.y<d->center.y+d->hh+1.0f&&
           fabsf(player_pos.z-d->center.z)<d->hd){
            g_interior_idx=i; break;
        }
    }
    if(g_interior_idx!=prev&&g_interior_idx>=0){
        g_interior_enter_timer=3.0f;
        printf("[Interior] Entered: %s\n",g_interiors[g_interior_idx].name);
    }
    if(g_interior_idx!=prev&&g_interior_idx<0&&prev>=0)
        printf("[Interior] Exited: %s\n",g_interiors[prev].name);
}

/* Define the world's interiors — called once during setup */
static void interiors_setup(void){
    /* ── 1. Valdoa Great Hall — castle interior ── */
    /* Valdoa is at world (50, -800). Floor at ~terrain_h(50,-800)+0.1 ≈ 6.1 */
    float vy=terrain_h(50,-800)+0.1f;
    int vi=interior_add("Valdoa Great Hall",
        (Vec3){50,vy+5,-800},    /* center (raised 5 above floor) */
        12,6,18,                  /* hw=12, hh=6, hd=18 */
        0.40f,0.32f,0.22f,       /* walls: warm stone */
        0.28f,0.22f,0.14f,       /* floor: dark stone */
        0.22f,0.18f,0.12f,       /* ceiling: dark beam */
        (Vec3){50,vy,-800+18},   /* door: south face */
        0.35f,0.22f,0.08f);      /* amber torch ambient */
    interior_add_prop(vi,(Vec3){ 0,0, 8},0,1.2f,"throne.fbx");
    interior_add_prop(vi,(Vec3){-8,0, 2},0,0.9f,"torch_wall.fbx");
    interior_add_prop(vi,(Vec3){ 8,0, 2},(float)M_PI,0.9f,"torch_wall.fbx");
    interior_add_prop(vi,(Vec3){-6,0,-4},0,0.8f,"barrel.fbx");
    interior_add_prop(vi,(Vec3){ 6,0,-4},0,0.8f,"crate.fbx");

    /* ── 2. Kerone City Tavern ── */
    /* Kerone City at (1450, -250). Floor ~terrain_h(1450,-250)+0.1 ≈ 5.1 */
    float ky=terrain_h(1450,-250)+0.1f;
    int ki=interior_add("The Rusty Flagon — Tavern",
        (Vec3){1450,ky+4,-250},  /* center */
        9,4.5f,12,                /* hw, hh, hd */
        0.48f,0.35f,0.20f,       /* walls: plank wood */
        0.32f,0.24f,0.14f,       /* floor: worn wood */
        0.26f,0.19f,0.10f,       /* ceiling: low beam */
        (Vec3){1450,ky,-250+12}, /* door: south */
        0.40f,0.25f,0.08f);      /* warm candlelight */
    interior_add_prop(ki,(Vec3){-5,0, 4},0,    0.9f,"table.fbx");
    interior_add_prop(ki,(Vec3){ 5,0, 4},0,    0.9f,"table.fbx");
    interior_add_prop(ki,(Vec3){ 0,0, 6},(float)M_PI,1.0f,"counter.fbx");
    interior_add_prop(ki,(Vec3){-6,0,-2},0,    0.7f,"barrel.fbx");
    interior_add_prop(ki,(Vec3){ 6,0,-2},0,    0.7f,"barrel.fbx");
    interior_add_prop(ki,(Vec3){-3,0, 2},0,    0.8f,"chair.fbx");
    interior_add_prop(ki,(Vec3){ 3,0, 2},(float)M_PI,0.8f,"chair.fbx");

    /* ── 3. Mountain Dungeon Entrance ── */
    /* Placed near Brandalhall (-2050,-2200). Floor ~terrain_h(-2050,-2200) ≈ 28 */
    float dy=terrain_h(-2050,-2200)+0.1f;
    int di=interior_add("Dungeon of Mashito",
        (Vec3){-2050,dy+4,-2200}, /* center */
        10,5,14,                   /* hw, hh, hd */
        0.25f,0.22f,0.20f,        /* walls: damp cave stone */
        0.18f,0.16f,0.14f,        /* floor: dark wet stone */
        0.15f,0.13f,0.12f,        /* ceiling: rough rock */
        (Vec3){-2050,dy,-2200+14},/* door: south */
        0.15f,0.12f,0.08f);       /* dim, eerie amber */
    interior_add_prop(di,(Vec3){-7,0, 5},0,    0.9f,"torch_wall.fbx");
    interior_add_prop(di,(Vec3){ 7,0, 5},(float)M_PI,0.9f,"torch_wall.fbx");
    interior_add_prop(di,(Vec3){ 0,0,-8},0,    1.1f,"chest.fbx");
    interior_add_prop(di,(Vec3){-5,0,-3},0,    0.7f,"barrel.fbx");
    interior_add_prop(di,(Vec3){ 4,0,-3},0,    0.8f,"crate.fbx");
}

/* Draw the current interior and show location banner */
static void interior_draw_room(RoomRenderer *rr,HUD *h,int win_w,int win_h,
                                Mat4 proj,Mat4 view){
    if(g_interior_idx<0)return;
    InteriorDef *d=&g_interiors[g_interior_idx];
    room_draw(rr,d,proj,view);

    /* Location banner — fades out after 3s */
    if(g_interior_enter_timer>0){
        float alpha=clampf(g_interior_enter_timer/1.5f,0,1);
        float W=(float)win_w;
        float tsc=2.0f,tw=tsc*8.0f*(float)strlen(d->name);
        /* Dark backing */
        hud_rect(h,W*0.5f-tw*0.5f-12,(float)win_h*0.20f-6,tw+24,tsc*10+8,
                 0,0,0,0.55f*alpha);
        hud_str(h,W*0.5f-tw*0.5f,(float)win_h*0.20f,tsc,d->name,
                1.0f,0.85f,0.35f,alpha);
        hud_flush(h);
    }
}

typedef enum{ MPAGE_MAIN, MPAGE_LOAD, MPAGE_OPTIONS, MPAGE_CHARSELECT, MPAGE_CREDITS }MenuPage;

/* ── Main menu background ─────────────────────────────────────
   Drop a wide panoramic image at assets/menu_bg.jpg (or .png)
   to display it as the menu backdrop. Falls back to the          
   original gradient if the file is not found.                    */
static GLuint g_menu_bg_tex = 0;
static bool   g_menu_bg_tried = false;

static void menu_bg_load(void){
    if(g_menu_bg_tried) return;
    g_menu_bg_tried=true;
    int w,h,ch;
    uint8_t *px=stbi_load("assets/menu_bg.jpg",&w,&h,&ch,3);
    if(!px) px=stbi_load("assets/menu_bg.png",&w,&h,&ch,3);
    if(!px){ printf("[Menu] No assets/menu_bg.jpg found — using gradient\n"); return; }
    glGenTextures(1,&g_menu_bg_tex);
    glBindTexture(GL_TEXTURE_2D,g_menu_bg_tex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_SRGB8,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,px);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    stbi_image_free(px);
    printf("[Menu] Loaded menu background %dx%d\n",w,h);
}

/* ── Shared background ───────────────────────────────────────── */
static void menu_bg(HUD *h,float W,float H){
    menu_bg_load();
    if(g_menu_bg_tex){
        /* Draw background image via splash system (reuses quad+shader) */
        if(g_splash_ready){
            glDisable(GL_DEPTH_TEST);
            glUseProgram(g_splash.prog);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D,g_menu_bg_tex);
            glUniform1i(glGetUniformLocation(g_splash.prog,"uTex"),0);
            glUniform1f(glGetUniformLocation(g_splash.prog,"uFade"),1.0f);
            glBindVertexArray(g_splash.vao);
            glDrawArrays(GL_TRIANGLES,0,6);
            glBindVertexArray(0);
            glEnable(GL_DEPTH_TEST);
        }
        /* Semi-transparent top strip for title legibility */
        hud_rect(h,0,0,W,H*0.30f, 0.0f,0.0f,0.0f,0.55f);
        /* Bottom vignette strip for button area */
        hud_rect(h,0,H*0.70f,W,H*0.30f, 0.0f,0.0f,0.0f,0.50f);
    } else {
        /* Gradient fallback */
        hud_rect(h,0,0,W,H,          0.04f,0.03f,0.06f,1.0f);
        hud_rect(h,0,H*0.5f,W,H*0.5f,0.07f,0.04f,0.02f,0.7f);
    }
    hud_rect(h,0,H-6,W,6, 0.7f,0.5f,0.1f,1.0f);
    hud_rect(h,0,0,   W,6, 0.7f,0.5f,0.1f,1.0f);
    /* Title */
    float tsc=5.0f;
    const char *title="LORDS OF MASHITO";
    float tw=tsc*8.0f*(float)strlen(title);
    /* Drop shadow */
    hud_str(h,(W-tw)*0.5f+3,H*0.08f+3,tsc,title, 0.0f,0.0f,0.0f,0.7f);
    hud_str(h,(W-tw)*0.5f,  H*0.08f,  tsc,title, 1.0f,0.80f,0.2f,1.0f);
    const char *sub="An Epic RPG Adventure";
    float ssc=1.6f,stw=ssc*8.0f*(float)strlen(sub);
    hud_str(h,(W-stw)*0.5f,H*0.19f,ssc,sub, 0.9f,0.8f,0.55f,0.9f);
}

static void studio_splash_draw(HUD *h,float W,float H,float alpha){
    const char *studio="IRON OAK GAMES";
    const char *tag="A small studio forging dark worlds";
    const char *hint="PRESS ANY KEY";
    float a=clampf(alpha,0.0f,1.0f);
    float title_sc=3.6f;
    float tag_sc=1.0f;
    float hint_sc=0.95f;
    float tw=title_sc*8.0f*(float)strlen(studio);
    float gw=tag_sc*8.0f*(float)strlen(tag);
    float hw=hint_sc*8.0f*(float)strlen(hint);
    float cx=W*0.5f;
    float cy=H*0.47f;

    hud_rect(h,0,0,W,H, 0.0f,0.0f,0.0f,a);
    hud_rect(h,cx-94.0f,cy-122.0f,188.0f,2.0f, 0.92f,0.92f,0.92f,0.70f*a);
    hud_rect(h,cx-58.0f,cy-112.0f,116.0f,116.0f, 0.0f,0.0f,0.0f,0.0f);
    hud_rect(h,cx-30.0f,cy-92.0f,60.0f,60.0f, 0.96f,0.96f,0.96f,0.10f*a);
    hud_rect(h,cx-8.0f,cy-102.0f,16.0f,72.0f, 0.92f,0.92f,0.92f,0.88f*a);
    hud_rect(h,cx-34.0f,cy-66.0f,68.0f,12.0f, 0.92f,0.92f,0.92f,0.88f*a);
    hud_rect(h,cx-44.0f,cy-42.0f,88.0f,14.0f, 0.92f,0.92f,0.92f,0.88f*a);
    hud_rect(h,cx-16.0f,cy-118.0f,32.0f,14.0f, 0.92f,0.92f,0.92f,0.88f*a);
    hud_rect(h,cx-74.0f,cy-10.0f,148.0f,2.0f, 0.92f,0.92f,0.92f,0.40f*a);

    hud_str(h,cx-tw*0.5f+2.0f,cy+18.0f+2.0f,title_sc,studio, 0.0f,0.0f,0.0f,0.55f*a);
    hud_str(h,cx-tw*0.5f,cy+18.0f,title_sc,studio, 0.96f,0.96f,0.96f,1.0f*a);
    hud_str(h,cx-gw*0.5f,cy+54.0f,tag_sc,tag, 0.70f,0.70f,0.70f,0.78f*a);
    hud_str(h,cx-hw*0.5f,cy+108.0f,hint_sc,hint, 0.62f,0.62f,0.62f,0.72f*a);
}


/* Slider helper — returns new value */
static int menu_slider(HUD *h,float x,float y,float w,const char *label,
                        int val,int mx,int my,bool clicked,bool held,
                        int slider_id,int *active_slider,int *drag_val){
    float lsc=1.4f,ltw=lsc*8.0f*(float)strlen(label);
    hud_str(h,x,y,lsc,label, 0.9f,0.85f,0.6f,1.0f);
    float tx=x+ltw+12,tw2=w-ltw-12,th=18;
    /* Track */
    hud_rect(h,tx,y+4,tw2,th-8, 0.12f,0.10f,0.07f,1.0f);
    /* Fill */
    float frac=val/100.0f;
    hud_rect(h,tx,y+4,tw2*frac,th-8, 0.7f,0.5f,0.1f,0.9f);
    /* Thumb */
    float tx2=tx+tw2*frac-6;
    bool over=(mx>=tx&&mx<=tx+tw2&&my>=y&&my<=y+th);
    if(clicked&&over&&active_slider) *active_slider=slider_id;
    hud_rect(h,tx2,y,12,th, over?1.0f:0.85f,over?0.9f:0.75f,0.3f,1.0f);
    /* Drag */
    if(held&&active_slider&&*active_slider==slider_id&&mx>=tx&&mx<=tx+tw2)
        val=(int)(((float)(mx-tx)/tw2)*100.0f);
    val=val<0?0:val>100?100:val;
    if(drag_val)*drag_val=val;
    /* Value label */
    char vbuf[8];snprintf(vbuf,sizeof(vbuf),"%d",val);
    hud_str(h,tx+tw2+8,y,lsc,vbuf, 0.8f,0.8f,0.8f,1.0f);
    return val;
}

/* ── MAIN PAGE ───────────────────────────────────────────────── */
static void page_main(HUD *h,float W,float H,int mx,int my,
                       bool *start,bool *load,bool *opts,bool *quit){
    menu_bg(h,W,H);
    float bw=260,bh=50,bx=(W-bw)*0.5f,by=H*0.33f,gap=16;
    menu_btn(h,bx,by,           bw,bh,"START GAME",2.3f,mx,my,true,start);
    menu_btn(h,bx,by+bh+gap,    bw,bh,"LOAD GAME", 2.3f,mx,my,true,load);
    menu_btn(h,bx,by+(bh+gap)*2,bw,bh,"OPTIONS",   2.3f,mx,my,true,opts);
    menu_btn(h,bx,by+(bh+gap)*3,bw,bh,"QUIT",      2.3f,mx,my,true,quit);
    hud_str(h,10,H-18,1.2f,"v0.1  |  WASD Move  |  LClick/F Attack  |  1-5 Abilities",
            0.45f,0.45f,0.45f,0.6f);
    hud_flush(h);
}

/* ── LOAD PAGE ───────────────────────────────────────────────── */
static void page_load(HUD *h,float W,float H,int mx,int my,
                       int *chosen_slot,bool *back){
    menu_bg(h,W,H);
    /* Header */
    const char *hdr="SELECT SAVE SLOT";
    float hsc=2.0f,htw=hsc*8.0f*(float)strlen(hdr);
    hud_str(h,(W-htw)*0.5f,H*0.25f,hsc,hdr, 1.0f,0.85f,0.3f,1.0f);
    /* Divider */
    hud_rect(h,W*0.15f,H*0.25f+26,W*0.7f,2, 0.6f,0.45f,0.1f,0.6f);

    float pw=(W*0.74f)/3.0f-14,ph=150,gap=18;
    float total_w=pw*3+gap*2,sx=(W-total_w)*0.5f,sy=H*0.31f;
    for(int i=0;i<3;i++){
        bool sc=false;
        slot_panel(h,sx+(pw+gap)*i,sy,pw,ph,i,mx,my,&sc);
        if(sc&&chosen_slot)*chosen_slot=i;
    }
    /* Back button */
    float bbw=160,bbh=40,bbx=W*0.5f-bbw*0.5f;
    menu_btn(h,bbx,H*0.80f,bbw,bbh,"BACK",2.0f,mx,my,true,back);
    hud_flush(h);
}

/* Helper: checkbox + label, returns true when hovered */
static bool menu_check(HUD *h,float x,float y,const char *label,bool val,int mx,int my){
    float s=22;
    bool over=(mx>=(int)x&&mx<=(int)(x+s)&&my>=(int)y&&my<=(int)(y+s));
    hud_rect(h,x,y,s,s, 0.12f,0.10f,0.07f,1.0f);
    if(val) hud_rect(h,x+3,y+3,s-6,s-6, 0.7f,0.5f,0.1f,1.0f);
    if(over) hud_rect(h,x-2,y-2,s+4,s+4, 0.8f,0.65f,0.1f,0.4f);
    hud_str(h,x+s+10,y+3,1.4f,label, 0.9f,0.85f,0.6f,1.0f);
    return over;
}

/* Helper: three difficulty buttons, returns new selection */
static int menu_difficulty(HUD *h,float x,float y,float bw,int cur,int mx,int my,bool clicked){
    const char *lbl[3]={"EASY","NORMAL","HARD"};
    float gap=10; int result=cur;
    for(int i=0;i<3;i++){
        float bx=x+(bw+gap)*(float)i;
        bool sel=(cur==i),over=(mx>=(int)bx&&mx<=(int)(bx+bw)&&my>=(int)y&&my<=(int)(y+28));
        if(sel) hud_rect(h,bx-2,y-2,bw+4,32, 0.7f,0.5f,0.1f,1.0f);
        hud_rect(h,bx,y,bw,28, sel?0.55f:(over?0.25f:0.10f),sel?0.35f:(over?0.18f:0.08f),sel?0.05f:0.03f,0.95f);
        float tw=1.4f*8.0f*(float)strlen(lbl[i]);
        hud_str(h,bx+(bw-tw)*0.5f,y+6,1.4f,lbl[i], sel?1.0f:0.8f,sel?0.9f:0.75f,sel?0.3f:0.55f,1.0f);
        if(clicked&&over) result=i;
    }
    return result;
}

/* ── OPTIONS PAGE ────────────────────────────────────────────── */
static void page_options(HUD *h,float W,float H,int mx,int my,bool clicked,
                          Options *opt,bool held,int *active_slider,
                          bool *back,bool *credits){
    menu_bg(h,W,H);
    const char *hdr="SETTINGS";
    float hsc=2.5f,htw=hsc*8.0f*(float)strlen(hdr);
    hud_str(h,(W-htw)*0.5f,H*0.17f,hsc,hdr, 1.0f,0.85f,0.3f,1.0f);
    hud_rect(h,W*0.15f,H*0.17f+30,W*0.7f,2, 0.6f,0.45f,0.1f,0.6f);

    float lx=W*0.13f, rx=W*0.54f, sw=W*0.32f;
    float y=H*0.29f, sg=44;

    /* LEFT: audio */
    hud_str(h,lx,y-20,1.3f,"AUDIO", 0.7f,0.6f,0.3f,0.8f);
    menu_slider(h,lx,y,      sw,"Master ",opt->master_vol,mx,my,clicked,held,0,active_slider,&opt->master_vol);
    menu_slider(h,lx,y+sg,   sw,"Music  ",opt->music_vol, mx,my,clicked,held,1,active_slider,&opt->music_vol);
    menu_slider(h,lx,y+sg*2, sw,"SFX    ",opt->sfx_vol,  mx,my,clicked,held,2,active_slider,&opt->sfx_vol);

    /* LEFT: display toggles */
    float ty=y+sg*3+14;
    hud_str(h,lx,ty-18,1.3f,"DISPLAY", 0.7f,0.6f,0.3f,0.8f);
    if(menu_check(h,lx,    ty,   "Fullscreen",opt->fullscreen,mx,my)&&clicked) opt->fullscreen=!opt->fullscreen;
    if(menu_check(h,lx,    ty+32,"VSync",     opt->vsync,     mx,my)&&clicked) opt->vsync=!opt->vsync;
    if(menu_check(h,lx,    ty+64,"Show FPS",  opt->show_fps,  mx,my)&&clicked) opt->show_fps=!opt->show_fps;
    if(menu_check(h,lx,    ty+96,"Bloom",     opt->bloom,     mx,my)&&clicked) opt->bloom=!opt->bloom;

    /* RIGHT: difficulty */
    hud_str(h,rx,y-20,1.3f,"GAMEPLAY", 0.7f,0.6f,0.3f,0.8f);
    hud_str(h,rx,y+4,1.35f,"Difficulty:", 0.85f,0.80f,0.55f,1.0f);
    opt->difficulty=menu_difficulty(h,rx,y+28,82,opt->difficulty,mx,my,clicked);

    /* RIGHT: resolution */
    hud_str(h,rx,y+sg*2-4,1.3f,"RESOLUTION", 0.7f,0.6f,0.3f,0.8f);
    static const int RW[]={1280,1600,1920,2560};
    static const int RH[]={720, 900, 1080,1440};
    static const char *RL[]={"1280 x 720","1600 x 900","1920 x 1080","2560 x 1440"};
    for(int ri=0;ri<4;ri++){
        float ry2=y+sg*2+16+(float)ri*28;
        bool sel=(opt->res_w==RW[ri]&&opt->res_h==RH[ri]);
        bool over=(mx>=(int)rx&&mx<=(int)(rx+200)&&my>=(int)ry2&&my<=(int)(ry2+24));
        if(sel) hud_rect(h,rx-2,ry2-1,204,26, 0.55f,0.35f,0.05f,0.9f);
        else if(over) hud_rect(h,rx-2,ry2-1,204,26, 0.18f,0.14f,0.05f,0.9f);
        hud_str(h,rx+8,ry2+4,1.3f,RL[ri], sel?1.0f:0.75f,sel?0.85f:0.7f,sel?0.25f:0.45f,1.0f);
        if(clicked&&over){opt->res_w=RW[ri];opt->res_h=RH[ri];}
    }

    /* Bottom buttons */
    float bby=H*0.87f,bbh=42,bbw2=155,gap2=20;
    float bbx0=W*0.5f-(bbw2*2+gap2)*0.5f;
    menu_btn(h,bbx0,         bby,bbw2,bbh,"CREDITS",1.8f,mx,my,true,credits);
    menu_btn(h,bbx0+bbw2+gap2,bby,bbw2,bbh,"BACK",   1.8f,mx,my,true,back);
    hud_flush(h);
}


/* ── CREDITS PAGE ──────────────────────────────────────────────── */
static void page_credits(HUD *h,float W,float H,int mx,int my,bool *back){
    menu_bg(h,W,H);
    const char *hdr="CREDITS";
    float hsc=2.5f,htw=hsc*8.0f*(float)strlen(hdr);
    hud_str(h,(W-htw)*0.5f,H*0.10f,hsc,hdr, 1.0f,0.85f,0.3f,1.0f);
    hud_rect(h,W*0.15f,H*0.10f+30,W*0.7f,2, 0.6f,0.45f,0.1f,0.6f);

    /* Entries: role  name  (for three columns) */
    static const char *entries[][2]={
        {"Game Design & Programming",  "Bryce"},
        {"Engine",                     "C99 / SDL2 / OpenGL 3.3"},
        {"3D Animation",               "ufbx  (MIT License)"},
        {"Image Loading",              "stb_image  (Public Domain)"},
        {"Audio",                      "SDL2_mixer  (zlib License)"},
        {"Special Thanks",             "The open-source community"},
        {NULL,NULL}
    };

    float ey=H*0.22f, row=34;
    float lx=W*0.18f, rx=W*0.52f;
    /* Column headers */
    hud_str(h,lx,ey,1.3f,"ROLE",  0.65f,0.55f,0.25f,0.9f);
    hud_str(h,rx,ey,1.3f,"NAME",  0.65f,0.55f,0.25f,0.9f);
    hud_rect(h,lx,ey+18,W*0.65f,1, 0.45f,0.35f,0.08f,0.5f);
    ey+=28;
    for(int i=0;entries[i][0];i++){
        bool alt=(i&1);
        if(alt) hud_rect(h,lx-8,ey-3,W*0.66f,row, 0.06f,0.04f,0.02f,0.4f);
        hud_str(h,lx,ey+6,1.35f,entries[i][0], 0.85f,0.80f,0.60f,1.0f);
        hud_str(h,rx,ey+6,1.35f,entries[i][1], 1.00f,0.90f,0.50f,1.0f);
        ey+=row;
    }

    /* Library license notice */
    hud_str(h,lx,ey+16,1.2f,
            "SDL2: https://libsdl.org  |  ufbx: https://github.com/ufbx/ufbx",
            0.5f,0.5f,0.5f,0.7f);
    hud_str(h,lx,ey+34,1.2f,
            "stb_image: https://github.com/nothings/stb",
            0.5f,0.5f,0.5f,0.7f);

    /* Copyright */
    float csc=1.3f;
    const char *copy="Copyright 2025  All Rights Reserved";
    float ctw=csc*8.0f*(float)strlen(copy);
    hud_str(h,(W-ctw)*0.5f,H*0.88f,csc,copy, 0.55f,0.50f,0.35f,0.8f);

    float bbw=160,bbh=40,bbx=W*0.5f-80;
    menu_btn(h,bbx,H*0.92f,bbw,bbh,"BACK",2.0f,mx,my,true,back);
    hud_flush(h);
}

/* Legacy wrapper — no longer used for main loop but kept for any stray refs */
static void menu_render(HUD *h,int win_w,int win_h,int mx,int my,
                        bool *play_clicked,bool *quit_clicked){
    bool load=false,opts=false;
    page_main(h,(float)win_w,(float)win_h,mx,my,play_clicked,&load,&opts,quit_clicked);
}

/* ── CHARACTER SELECT PAGE ──────────────────────────────────────
   Shows up to 4 character cards after "Start Game".
   Each card has: name, class, lore blurb, stat bars, colour panel.
   Optional portrait: drop "portrait_<id>.png" next to the exe.   */

#define CHARSEL_MAX 4

typedef struct {
    const char *name;
    const char *class_name;
    const char *lore;        /* one short line */
    const char *fbx_path;    /* passed straight to charmodel_init  */
    /* Colour used for the portrait panel when no portrait PNG */
    float r,g,b;
    /* Approximate stats 0–10 (display only) */
    int   str,agi,mag,end;
    /* Optional portrait texture (loaded lazily) */
    GLuint portrait_tex;
    bool   portrait_tried;
} CharDef;

/* ── Define your characters here ─────────────────────────────── */
static CharDef g_chardefs[CHARSEL_MAX]={
    {
        "Aldric","Warrior",
        "A seasoned knight who survived the siege of Valdoa.",
        "models/character_warrior.fbx",
        0.55f,0.25f,0.10f,   /* dark orange */
        9,5,2,8,
        0,false
    },
    {
        "Lyra","Rogue",
        "She moves like shadow. Nobody sees her coming.",
        "models/character_rogue.fbx",
        0.15f,0.35f,0.20f,   /* dark green */
        5,9,4,6,
        0,false
    },
    {
        "Soren","Mage",
        "Exiled from the Arcane College. Power without limits.",
        "models/character_mage.fbx",
        0.10f,0.15f,0.45f,   /* deep blue */
        3,5,10,4,
        0,false
    },
    {
        "Brann","Ranger",
        "Mountain born. Arrow never misses at distance.",
        "models/character_ranger.fbx",
        0.30f,0.28f,0.12f,   /* earthy brown */
        6,8,3,7,
        0,false
    },
};
static int g_charsel_count=4;  /* how many to show; reduce if fewer FBX ready */

/* Try to load portrait_<idx>.png once */
static GLuint charsel_portrait(int idx){
    if(idx<0||idx>=CHARSEL_MAX)return 0;
    CharDef *c=&g_chardefs[idx];
    if(!c->portrait_tried){
        char path[64];
        snprintf(path,sizeof(path),"portrait_%d.png",idx);
        c->portrait_tex=terrain_tex_load(path,0,0,0);
        c->portrait_tried=true;
    }
    return c->portrait_tex;
}

/* Draw a single stat bar with label */
static void charsel_stat(HUD *h,float x,float y,float w,
                          const char *label,int val){
    float sc=1.15f;
    hud_str(h,x,y,sc,label, 0.85f,0.80f,0.55f,1.0f);
    float bx=x+48,bw=w-50,bh=9;
    hud_rect(h,bx,y+2,bw,bh,       0.10f,0.08f,0.05f,1.0f);
    hud_rect(h,bx,y+2,bw*(val/10.0f),bh, 0.75f,0.55f,0.10f,0.95f);
}

static void page_charselect(HUD *h,float W,float H,int mx,int my,bool clicked,
                             int *hovered,int *selected,bool *back){
    menu_bg(h,W,H);

    /* Header */
    const char *hdr="CHOOSE YOUR HERO";
    float hsc=2.5f,htw=hsc*8.0f*(float)strlen(hdr);
    hud_str(h,(W-htw)*0.5f, H*0.07f, hsc, hdr, 1.0f,0.85f,0.3f,1.0f);
    hud_rect(h,W*0.15f,H*0.07f+28, W*0.70f,2, 0.6f,0.45f,0.1f,0.5f);

    int n=g_charsel_count<CHARSEL_MAX?g_charsel_count:CHARSEL_MAX;
    float card_w=160.0f, card_h=320.0f;
    float total_w=n*card_w+(n-1)*18.0f;
    float cx=(W-total_w)*0.5f;
    float cy=H*0.16f;

    /* hovered is persistent across frames (set by caller) — only update when
       cursor is actually over a card this frame, leave alone otherwise so
       the SELECT HERO button stays visible when cursor moves down to it. */
    int this_frame_hover=-1;
    for(int i=0;i<n;i++){
        CharDef *cd=&g_chardefs[i];
        float x=cx+i*(card_w+18.0f);
        float y=cy;

        bool over=(mx>=x&&mx<=x+card_w&&my>=y&&my<=y+card_h);
        if(over) this_frame_hover=i;

        /* Card shadow */
        hud_rect(h,x+4,y+4,card_w,card_h, 0,0,0,0.5f);

        /* Card bg — brighter on hover */
        bool hi=(i==*hovered);
        float cbr=hi?0.20f:0.12f, cbg=hi?0.14f:0.08f, cbb=hi?0.08f:0.04f;
        hud_rect(h,x,y,card_w,card_h, cbr,cbg,cbb,0.95f);

        /* Gold outline on hover */
        if(hi){
            hud_rect(h,x-2,y-2,card_w+4,2,       0.9f,0.7f,0.1f,1.0f);
            hud_rect(h,x-2,y+card_h,card_w+4,2,  0.9f,0.7f,0.1f,1.0f);
            hud_rect(h,x-2,y,2,card_h,           0.9f,0.7f,0.1f,1.0f);
            hud_rect(h,x+card_w,y,2,card_h,      0.9f,0.7f,0.1f,1.0f);
        }

        /* Portrait panel */
        float ph=card_h*0.42f;
        GLuint ptex=charsel_portrait(i);
        if(ptex){
            hud_rect(h,x+4,y+4,card_w-8,ph, 1,1,1,1);
        }
        hud_rect(h,x+4,y+4,card_w-8,ph, cd->r,cd->g,cd->b, ptex?0.3f:0.85f);

        /* Silhouette */
        if(!ptex){
            float mx2=x+card_w*0.5f, ty=y+8, by2=y+ph;
            float hw=card_w*0.18f;
            hud_rect(h,mx2-hw,ty+ph*0.30f, hw*2,2, 0,0,0,0.25f);
            hud_rect(h,mx2-8,ty+ph*0.08f,16,16, cd->r*1.6f,cd->g*1.6f,cd->b*1.6f,0.5f);
            (void)by2;
        }

        /* Class banner */
        hud_rect(h,x+4,y+4+ph,card_w-8,16, 0,0,0,0.55f);
        float csc=1.2f,ctw=csc*8.0f*(float)strlen(cd->class_name);
        hud_str(h,x+(card_w-ctw)*0.5f, y+6+ph, csc, cd->class_name,
                cd->r*1.5f+0.3f, cd->g*1.5f+0.3f, cd->b*1.5f+0.3f, 1.0f);

        /* Name */
        float nsc=1.7f,ntw=nsc*8.0f*(float)strlen(cd->name);
        hud_str(h,x+(card_w-ntw)*0.5f, y+ph+24, nsc, cd->name, 1.0f,0.92f,0.6f,1.0f);

        /* Lore */
        float lsc=0.95f;
        {
            const char *l=cd->lore; int len=(int)strlen(l);
            int lw=22;
            float ly2=y+ph+46;
            for(int c2=0;c2<len;c2+=lw){
                char tmp[32]={0};
                int take=len-c2>lw?lw:len-c2;
                strncpy(tmp,l+c2,(size_t)take);
                hud_str(h,x+6,ly2,lsc,tmp, 0.80f,0.76f,0.60f,0.9f);
                ly2+=lsc*12.0f;
            }
        }

        /* Stat bars */
        float sy2=y+card_h-95;
        charsel_stat(h,x+8,sy2,card_w-16,"STR",cd->str);  sy2+=16;
        charsel_stat(h,x+8,sy2,card_w-16,"AGI",cd->agi);  sy2+=16;
        charsel_stat(h,x+8,sy2,card_w-16,"MAG",cd->mag);  sy2+=16;
        charsel_stat(h,x+8,sy2,card_w-16,"END",cd->end);

        /* Click on card directly selects it */
        if(over&&clicked) *selected=i;
    }

    /* Update persistent hover only when cursor is actually over a card */
    if(this_frame_hover>=0) *hovered=this_frame_hover;

    /* SELECT HERO button — visible whenever a card has been hovered/selected */
    float bbw=200,bbh=44;
    float btn_y=cy+card_h+28;
    if(*hovered>=0){
        bool conf_over=(mx>=(W-bbw)*0.5f&&mx<=(W-bbw)*0.5f+bbw&&my>=btn_y&&my<=btn_y+bbh);
        menu_btn(h,(W-bbw)*0.5f,btn_y,bbw,bbh,"SELECT HERO",2.0f,mx,my,true,NULL);
        if(conf_over&&clicked) *selected=*hovered;
    } else {
        hud_rect(h,(W-bbw)*0.5f,btn_y,bbw,bbh, 0.10f,0.06f,0.03f,0.6f);
        float lsc2=2.0f,ltw=lsc2*8.0f*(float)strlen("SELECT HERO");
        hud_str(h,(W-bbw)*0.5f+(bbw-ltw)*0.5f,btn_y+(bbh-lsc2*10)*0.5f,
                lsc2,"SELECT HERO", 0.45f,0.40f,0.25f,0.7f);
    }
    menu_btn(h,(W-bbw)*0.5f-110,btn_y,90,bbh,"BACK",1.8f,mx,my,true,back);

    hud_flush(h);
}

/* ── In-game Save/Load UI ───────────────────────────────────── */
typedef struct{
    bool open;
    int  hover_slot;    /* -1=none */
    float anim;         /* 0..1 slide-in */
}SaveUI;

static void saveui_draw(SaveUI *ui,HUD *h,int win_w,int win_h,int mx,int my,
                        int *save_slot,int *load_slot){
    *save_slot=-1;*load_slot=-1;
    if(!ui->open)return;
    float W=(float)win_w,H=(float)win_h;
    /* Dark overlay */
    hud_rect(h,0,0,W,H, 0,0,0,0.6f);
    /* Panel */
    float pw=580,ph=320,px=(W-pw)*0.5f,py=(H-ph)*0.5f;
    hud_rect(h,px,py,pw,ph, 0.06f,0.04f,0.02f,0.97f);
    /* Gold border */
    hud_rect(h,px,py,pw,3,     0.7f,0.5f,0.1f,1);
    hud_rect(h,px,py+ph-3,pw,3,0.7f,0.5f,0.1f,1);
    hud_rect(h,px,py,3,ph,     0.7f,0.5f,0.1f,1);
    hud_rect(h,px+pw-3,py,3,ph,0.7f,0.5f,0.1f,1);
    /* Title */
    const char *ttl="SAVE / LOAD";
    float tsc=2.2f,ttw=tsc*8.0f*(float)strlen(ttl);
    hud_str(h,px+(pw-ttw)*0.5f,py+12,tsc,ttl, 1.0f,0.85f,0.2f,1.0f);
    /* Divider */
    hud_rect(h,px+20,py+42,pw-40,2, 0.5f,0.35f,0.05f,0.8f);
    /* 3 slots */
    float sw=(pw-60)/3.0f,sh=180,sx2=px+20,sy2=py+56;
    for(int i=0;i<3;i++){
        float cx=sx2+(sw+10)*i;
        /* Panel */
        bool over=(mx>=cx&&mx<=cx+sw&&my>=sy2&&my<=sy2+sh);
        if(over)hud_rect(h,cx-2,sy2-2,sw+4,sh+4, 0.7f,0.55f,0.1f,0.7f);
        hud_rect(h,cx,sy2,sw,sh, over?0.18f:0.10f,0.08f,0.04f,0.95f);
        /* Slot info */
        char path[64];save_path(path,sizeof(path),i);
        FILE *f=fopen(path,"rb");
        bool has=false;int lvl=0;float wt=0,hp2=0;
        if(f){SaveData sd;if(fread(&sd,sizeof(sd),1,f)==1&&sd.magic==SAVE_MAGIC&&sd.version==SAVE_VERSION){has=true;lvl=(int)sd.level;wt=sd.world_time;hp2=sd.health;}fclose(f);}
        char slabel[16];snprintf(slabel,sizeof(slabel),"SLOT %d",i+1);
        float lsc2=1.5f,ltw2=lsc2*8.0f*(float)strlen(slabel);
        hud_str(h,cx+(sw-ltw2)*0.5f,sy2+8,lsc2,slabel, 1.0f,0.85f,0.3f,1.0f);
        if(has){
            char lb[24];snprintf(lb,sizeof(lb),"Level %d",lvl);
            float lsc3=1.9f,ltw3=lsc3*8.0f*(float)strlen(lb);
            hud_str(h,cx+(sw-ltw3)*0.5f,sy2+30,lsc3,lb, 1.0f,0.95f,0.8f,1.0f);
            char hb[24];snprintf(hb,sizeof(hb),"HP: %.0f",hp2);
            hud_str(h,cx+8,sy2+60,1.2f,hb, 0.8f,0.9f,0.7f,0.9f);
            char tb[24];fmt_playtime(wt,tb,sizeof(tb));
            hud_str(h,cx+8,sy2+78,1.2f,tb, 0.7f,0.75f,0.8f,0.85f);
        } else {
            hud_str(h,cx+8,sy2+44,1.3f,"— Empty —", 0.5f,0.5f,0.5f,0.7f);
        }
        /* Save button */
        bool save_over=(mx>=cx+4&&mx<=cx+sw/2-6&&my>=sy2+sh-38&&my<=sy2+sh-6);
        hud_rect(h,cx+4,sy2+sh-38,sw/2-8,32, save_over?0.3f:0.15f,save_over?0.2f:0.1f,0.05f,0.95f);
        hud_str(h,cx+10,sy2+sh-28,1.3f,"SAVE", save_over?1.0f:0.8f,save_over?0.9f:0.7f,0.3f,1.0f);
        /* Load button */
        bool load_over=(mx>=cx+sw/2+2&&mx<=cx+sw-4&&my>=sy2+sh-38&&my<=sy2+sh-6);
        hud_rect(h,cx+sw/2+2,sy2+sh-38,sw/2-6,32, load_over?0.1f:0.05f,load_over?0.3f:0.15f,load_over?0.4f:0.2f,0.95f);
        hud_str(h,cx+sw/2+8,sy2+sh-28,1.3f,"LOAD", load_over?0.7f:0.5f,load_over?0.95f:0.75f,load_over?1.0f:0.8f,1.0f);
        if(save_over&&SDL_GetMouseState(NULL,NULL)&SDL_BUTTON(1))*save_slot=i;
        if(load_over&&SDL_GetMouseState(NULL,NULL)&SDL_BUTTON(1))*load_slot=i;
    }
    /* Close hint */
    hud_str(h,px+(pw-160)*0.5f,py+ph-22,1.3f,"TAB or ESC to close", 0.5f,0.5f,0.5f,0.7f);
}

/* Returns true if slot has a valid save */
static bool __attribute__((unused)) save_probe(int slot,float *out_time,int *out_level){
    char path[64];save_path(path,sizeof(path),slot);
    FILE *f=fopen(path,"rb");if(!f)return false;
    SaveData sd;
    bool ok=(fread(&sd,sizeof(sd),1,f)==1&&sd.magic==SAVE_MAGIC&&sd.version==SAVE_VERSION);
    fclose(f);
    if(ok){if(out_time)*out_time=sd.world_time;if(out_level)*out_level=sd.level;}
    return ok;
}

static bool game_save(int slot,Player *pl,QuestSystem *qs,AbilitySystem *as,
                      Enemy *enemies,int ne,float world_time,Inventory *inv,
                      LevelSystem *lvl){
    save_mkdir();
    char path[64];save_path(path,sizeof(path),slot);
    SaveData sd;memset(&sd,0,sizeof(sd));
    sd.magic=SAVE_MAGIC;sd.version=SAVE_VERSION;
    /* Player */
    sd.px=pl->phys.pos.x;sd.py=pl->phys.pos.y;sd.pz=pl->phys.pos.z;
    sd.pyaw=pl->phys.yaw;
    sd.health=pl->health;sd.max_health=pl->max_health;
    /* Resources */
    sd.mana=as->mana;sd.max_mana=as->max_mana;
    sd.stam=as->stam;sd.max_stam=as->max_stam;
    for(int i=0;i<HOTBAR_SLOTS;i++){
        sd.ab_cd_timer[i]=as->slots[i].cd_timer;
        sd.ab_ready[i]=as->slots[i].ready;
    }
    /* Progression */
    sd.gold=qs->player_gold;sd.xp=qs->player_xp;sd.level=qs->player_level;
    /* Quests */
    sd.quest_count=qs->count;
    for(int i=0;i<qs->count&&i<MAX_QUESTS;i++){
        sd.quests[i].id=qs->quests[i].id;
        sd.quests[i].status=(int)qs->quests[i].status;
        for(int j=0;j<qs->quests[i].obj_count&&j<MAX_OBJECTIVES;j++){
            sd.quests[i].obj_current[j]=qs->quests[i].objs[j].current;
            sd.quests[i].obj_done[j]=qs->quests[i].objs[j].done;
        }
    }
    /* Enemies */
    sd.ne=ne;
    for(int i=0;i<ne&&i<MAX_ENEMIES;i++){
        sd.enemies[i].active=enemies[i].active;
        sd.enemies[i].etype=(int)enemies[i].etype;
        sd.enemies[i].hx=enemies[i].phys.pos.x;
        sd.enemies[i].hy=enemies[i].phys.pos.y;
        sd.enemies[i].hz=enemies[i].phys.pos.z;
        sd.enemies[i].health=enemies[i].health;
        sd.enemies[i].state=(int)enemies[i].state;
    }
    sd.world_time=world_time;
    /* Inventory */
    for(int i=0;i<INV_SLOTS;i++){sd.inv_slots[i].type=(int)inv->slots[i].type;sd.inv_slots[i].count=inv->slots[i].count;}
    for(int i=0;i<INV_EQUIP_SLOTS;i++)sd.inv_equipped[i]=(int)inv->equipped[i];
    sd.lvl_dmg_bonus=lvl->dmg_bonus;sd.lvl_hp_bonus=lvl->hp_bonus;
    sd.lvl_mp_bonus=lvl->mp_bonus;sd.lvl_regen_bonus=lvl->regen_bonus;
    FILE *f=fopen(path,"wb");if(!f){printf("[Save] Cannot write %s\n",path);return false;}
    bool ok=fwrite(&sd,sizeof(sd),1,f)==1;
    fclose(f);
    printf("[Save] Slot %d saved to %s\n",slot,path);
    return ok;
}

static bool game_load(int slot,Player *pl,QuestSystem *qs,AbilitySystem *as,
                      Enemy *enemies,int ne,float *world_time,AnimMap *amap,
                      AnimMap *enemy_amaps,
                      Inventory *inv,float base_max_hp,LevelSystem *lvl){
    char path[64];save_path(path,sizeof(path),slot);
    FILE *f=fopen(path,"rb");if(!f){printf("[Load] %s not found\n",path);return false;}
    SaveData sd;
    if(fread(&sd,sizeof(sd),1,f)!=1||sd.magic!=SAVE_MAGIC||sd.version!=SAVE_VERSION){
        fclose(f);printf("[Load] Invalid save file %s\n",path);return false;
    }
    fclose(f);
    /* Player */
    pl->phys.pos=(Vec3){sd.px,sd.py,sd.pz};pl->phys.yaw=sd.pyaw;
    pl->phys.vel=(Vec3){0,0,0};pl->phys.on_ground=false;
    pl->health=sd.health;pl->max_health=sd.max_health;
    pl->state=PS_IDLE;pl->anim_idx=amap->idle;pl->anim_time=0;
    /* Resources */
    as->mana=sd.mana;as->max_mana=sd.max_mana;
    as->stam=sd.stam;as->max_stam=sd.max_stam;as->stam_drain_timer=0.0f;
    for(int i=0;i<HOTBAR_SLOTS;i++){
        as->slots[i].cd_timer=sd.ab_cd_timer[i];
        as->slots[i].ready=sd.ab_ready[i];
    }
    /* Progression */
    qs->player_gold=sd.gold;qs->player_xp=sd.xp;qs->player_level=sd.level;
    /* Quest progress — match by id */
    for(int si=0;si<sd.quest_count&&si<MAX_QUESTS;si++){
        for(int qi=0;qi<qs->count;qi++){
            if(qs->quests[qi].id!=sd.quests[si].id)continue;
            qs->quests[qi].status=sd.quests[si].status;
            for(int j=0;j<qs->quests[qi].obj_count&&j<MAX_OBJECTIVES;j++){
                qs->quests[qi].objs[j].current=sd.quests[si].obj_current[j];
                qs->quests[qi].objs[j].done   =sd.quests[si].obj_done[j];
            }
            break;
        }
    }
    /* Enemies */
    for(int i=0;i<ne&&i<sd.ne&&i<MAX_ENEMIES;i++){
        enemies[i].active=sd.enemies[i].active;
        enemies[i].health=sd.enemies[i].health;
        enemies[i].phys.pos=(Vec3){sd.enemies[i].hx,sd.enemies[i].hy,sd.enemies[i].hz};
        enemies[i].state=(EnemyState)sd.enemies[i].state;
        enemies[i].etype=(EnemyType)sd.enemies[i].etype;
        /* Use the correct AnimMap for this enemy type */
        AnimMap *em=&enemy_amaps[enemies[i].etype];
        enemies[i].anim_idx=(enemies[i].state==ES_DEAD)?em->death:em->idle;
    }
    *world_time=sd.world_time;
    /* Inventory */
    for(int i=0;i<INV_SLOTS;i++){inv->slots[i].type=(ItemType)sd.inv_slots[i].type;inv->slots[i].count=sd.inv_slots[i].count;}
    for(int i=0;i<INV_EQUIP_SLOTS;i++)inv->equipped[i]=(ItemType)sd.inv_equipped[i];
    inv_recalc(inv,pl,base_max_hp);
    lvl->dmg_bonus=sd.lvl_dmg_bonus;lvl->hp_bonus=sd.lvl_hp_bonus;
    lvl->mp_bonus=sd.lvl_mp_bonus;lvl->regen_bonus=sd.lvl_regen_bonus;
    as->max_mana=100.0f+lvl->mp_bonus;as->mp_regen=3.0f+lvl->regen_bonus;as->sp_regen=8.0f+lvl->regen_bonus;
    inv_recalc(inv,pl,base_max_hp+lvl->hp_bonus);
    printf("[Load] Slot %d loaded from %s\n",slot,path);
    return true;
}

/* On-screen save/load feedback */
static void save_notify(SaveNotify *n,const char *msg){
    strncpy(n->msg,msg,sizeof(n->msg)-1);n->msg[sizeof(n->msg)-1]=0;n->timer=2.5f;
}
static void save_notify_draw(SaveNotify *n,HUD *h,int win_w,int win_h,float dt){
    if(n->timer<=0)return;
    n->timer-=dt;
    float a=clampf(n->timer*2.0f,0,1);
    float tw=1.6f*8.0f*(float)strlen(n->msg);
    float x=(float)win_w*0.5f-tw*0.5f,y=(float)win_h*0.12f;
    hud_rect(h,x-8,y-4,tw+16,24, 0,0,0,0.6f*a);
    hud_str(h,x,y,1.6f,n->msg, 0.3f,1.0f,0.4f,a);
}

/* ── Boss System ─────────────────────────────────────────────── */
static float g_screen_shake=0.0f;   /* current shake magnitude, decays each frame */

/* Draw the boss health bar at top of screen */
/* ── Death Screen ─────────────────────────────────────────────
   Drawn whenever player.state==PS_DEAD.  Fades in over 1.5s,
   shows "YOU DIED", a stat summary, and a single respawn button.
   Returns true when the player clicks Respawn (caller resets).  */
static bool death_screen_draw(HUD *h, float *fade_t, float dt,
                              int win_w, int win_h,
                              float hp_max, int gold, int level,
                              int mx, int my){
    float W=(float)win_w, H=(float)win_h;

    /* Fade in from black — 1.5s */
    *fade_t=clampf(*fade_t+dt/1.5f, 0.0f, 1.0f);
    float a=*fade_t;

    /* Dark blood-red vignette overlay */
    hud_rect(h,0,0,W,H, 0.04f,0.0f,0.0f, a*0.82f);

    if(a<0.25f){hud_flush(h);return false;} /* wait before showing text */

    float ta=clampf((a-0.25f)/0.75f,0,1); /* text alpha lags vignette */

    /* "YOU DIED" */
    const char *title="YOU DIED";
    float tsc=5.0f, tw=tsc*8.0f*(float)strlen(title);
    float ty=H*0.32f;
    /* Shadow */
    hud_str(h,W*0.5f-tw*0.5f+3,ty+3,tsc,title, 0.0f,0.0f,0.0f,ta*0.8f);
    /* Main — dark crimson */
    hud_str(h,W*0.5f-tw*0.5f,ty,tsc,title, 0.75f,0.04f,0.04f,ta);

    /* Stat summary */
    char buf[128];
    float sy=ty+tsc*12.0f+24.0f, ssc=1.6f, slh=ssc*12.0f+4.0f;
    snprintf(buf,sizeof(buf),"Level %d   Max HP %.0f   Gold %d",level,(double)hp_max,gold);
    float sw=ssc*8.0f*(float)strlen(buf);
    hud_str(h,W*0.5f-sw*0.5f,sy,ssc,buf, 0.75f,0.60f,0.60f,ta*0.85f);
    sy+=slh*2.0f;

    /* Respawn button — only show once fully faded in */
    if(ta>=0.98f){
        float bw=240,bh=48,bx=W*0.5f-bw*0.5f,by=sy+8;
        bool hov=(mx>=bx&&mx<=bx+bw&&my>=by&&my<=by+bh);
        hud_rect(h,bx,by,bw,bh, hov?0.55f:0.35f,0.05f,0.05f,0.9f);
        hud_rect(h,bx,by,bw,2,  0.85f,0.2f,0.2f,1.0f); /* top border */
        const char *rlbl="RESPAWN AT CAMPFIRE";
        float rw=1.5f*8.0f*(float)strlen(rlbl);
        hud_str(h,bx+(bw-rw)*0.5f,by+(bh-14)*0.5f,1.5f,rlbl,
                1.0f,0.8f,0.8f,1.0f);

        bool lmb=(SDL_GetMouseState(NULL,NULL)&SDL_BUTTON(1))!=0;
        if(lmb&&hov){hud_flush(h);return true;}
    }

    hud_flush(h);
    return false;
}

static void boss_hud_draw(HUD *h,Enemy *boss,int win_w,int win_h){
    if(!boss||!boss->active||boss->state==ES_DEAD)return;
    if(boss->etype!=ETYPE_BOSS||boss->boss_phase<1)return; /* not a real boss */
    float W=(float)win_w, H=(float)win_h;
    float bw=W*0.5f, bh=18.0f;
    float bx=W*0.25f, by=18.0f;
    float frac=clampf(boss->health/boss->max_health,0,1);

    /* Background */
    hud_rect(h,bx-2,by-2,bw+4,bh+4, 0,0,0,0.75f);
    /* Health fill — color shifts by phase */
    float r=0.05f,g2=0.75f,b2=0.85f;
    if(boss->boss_phase==2){r=0.6f;g2=0.1f;b2=0.8f;}
    if(boss->boss_phase==3){r=0.9f;g2=0.05f;b2=0.05f;}
    hud_rect(h,bx,by,bw*frac,bh, r,g2,b2,0.9f);
    /* Boss name */
    const char *phase_str[]={"","  [ENRAGED]","  [BERSERK]"};
    char buf[64];snprintf(buf,sizeof(buf),"Lich King%s",phase_str[boss->boss_phase-1]);
    float tw=strlen(buf)*6.0f*1.4f;
    hud_str(h,W*0.5f-tw*0.5f,by-14.0f,1.4f,buf, r+0.1f,g2+0.1f,b2+0.1f,1.0f);
    /* Phase flash overlay */
    if(boss->phase_flash&&boss->phase_flash_t>0){
        float a=boss->phase_flash_t*0.45f;
        hud_rect(h,0,0,W,H, r,g2*0.3f,b2*0.3f,a);
    }
    (void)H;
}

int main(int argc,char **argv){
    /* char_path is set either from argv (dev override) or from character select */
    char char_path_buf[256]="models/character_warrior.fbx";
    const char *char_path=char_path_buf;
    if(argc>=2){
        /* Dev/CLI override: skip character select and load this FBX directly */
        strncpy(char_path_buf,argv[1],sizeof(char_path_buf)-1);
    }

    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO);
    audio_init();
    audio_set_volumes(g_options.music_vol,g_options.sfx_vol);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,1);SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,4);
    SDL_Window *win=SDL_CreateWindow("Lords of Mashito",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,WIN_W,WIN_H,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    SDL_GLContext ctx=SDL_GL_CreateContext(win);
    SDL_GL_SetSwapInterval(1);glEnable(GL_DEPTH_TEST);glEnable(GL_MULTISAMPLE);
    glClearColor(0.04f,0.03f,0.06f,1.0f);
    srand((unsigned)time(NULL));

    /* Init HUD early — it's just a shader + VBO, no assets needed */
    int win_w=WIN_W,win_h=WIN_H;
    HUD hud; hud_init(&hud,win_w,win_h);
    splash_init(); /* load assets/splash/splash0..4.jpg for loading screen */
    gamepad_try_open(); /* open first connected controller if any */
    pfx_init(&g_pfx, win_w, win_h); /* HDR FBO + bloom + tonemap + SSAO */
    reflect_init(&g_reflect, win_w, win_h); /* planar water reflection */
    bool show_startup_splash=true;

menu_start:
    /* ── MAIN MENU ─────────────────────────────────────────────── */
    options_load(&g_options); /* load persisted settings */
    audio_set_volumes(g_options.music_vol,g_options.sfx_vol);
    SDL_GL_SetSwapInterval(g_options.vsync?1:0);
    if(g_options.fullscreen) SDL_SetWindowFullscreen(win,SDL_WINDOW_FULLSCREEN_DESKTOP);
    if(show_startup_splash){
        float splash_t=0.0f;
        const float splash_fade_in=0.45f;
        const float splash_hold=1.60f;
        const float splash_fade_out=0.65f;
        const float splash_total=splash_fade_in+splash_hold+splash_fade_out;
        Uint64 splash_last=SDL_GetPerformanceCounter();
        bool splash_done=false;

        while(!splash_done){
            SDL_Event ev;
            bool skip=false;
            while(SDL_PollEvent(&ev)){
                if(ev.type==SDL_QUIT){SDL_Quit();return 0;}
                if(ev.type==SDL_WINDOWEVENT&&ev.window.event==SDL_WINDOWEVENT_RESIZED){
                    win_w=ev.window.data1;win_h=ev.window.data2;
                    hud_resize(&hud,win_w,win_h);glViewport(0,0,win_w,win_h);
                }
                if(ev.type==SDL_KEYDOWN||ev.type==SDL_MOUSEBUTTONDOWN||ev.type==SDL_CONTROLLERBUTTONDOWN)
                    skip=true;
            }

            Uint64 splash_now=SDL_GetPerformanceCounter();
            float dt=clampf((float)(splash_now-splash_last)/(float)SDL_GetPerformanceFrequency(),0,0.05f);
            splash_last=splash_now;
            if(skip&&splash_t<(splash_fade_in+splash_hold)) splash_t=splash_fade_in+splash_hold;
            splash_t+=dt;

            float overlay_alpha=1.0f;
            if(splash_t<splash_fade_in) overlay_alpha=splash_t/splash_fade_in;
            else if(splash_t<(splash_fade_in+splash_hold)) overlay_alpha=1.0f;
            else overlay_alpha=1.0f-(splash_t-(splash_fade_in+splash_hold))/splash_fade_out;
            overlay_alpha=clampf(overlay_alpha,0.0f,1.0f);

            float W=(float)win_w,H=(float)win_h;
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            studio_splash_draw(&hud,W,H,overlay_alpha);
            hud_flush(&hud);
            SDL_GL_SwapWindow(win);

            if(splash_t>=splash_total) splash_done=true;
        }
        show_startup_splash=false;
    }
    audio_play_music(MUS_MENU,0);
    int menu_load_slot=-1; /* which slot to load after menu (-1 = new game) */
    int menu_char_sel=-1;  /* which character was chosen (-1 = not yet)     */
    (void)menu_char_sel;
    {
        MenuPage page=MPAGE_MAIN;
        bool running_menu=true;
        bool lmb_prev=false;
        bool slider_held=false;
        int active_slider=-1;
        int  charsel_hover=-1;

        while(running_menu){
            SDL_Event ev;
            while(SDL_PollEvent(&ev)){
                if(ev.type==SDL_QUIT){SDL_Quit();return 0;}
                if(ev.type==SDL_WINDOWEVENT&&ev.window.event==SDL_WINDOWEVENT_RESIZED){
                    win_w=ev.window.data1;win_h=ev.window.data2;
                    hud_resize(&hud,win_w,win_h);glViewport(0,0,win_w,win_h);
                }
                if(ev.type==SDL_KEYDOWN&&ev.key.keysym.sym==SDLK_ESCAPE){
                    if(page!=MPAGE_MAIN)page=MPAGE_MAIN;
                    else{SDL_Quit();return 0;}
                }
            }
            int mx,my;
            bool lmb=(SDL_GetMouseState(&mx,&my)&SDL_BUTTON(1))!=0;
            bool clicked=lmb&&!lmb_prev; /* rising edge */
            slider_held=lmb;
            if(!slider_held) active_slider=-1;

            float W=(float)win_w,H=(float)win_h;
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

            if(page==MPAGE_MAIN){
                bool start=false,load=false,opts=false,quit=false;
                page_main(&hud,W,H,mx,my,&start,&load,&opts,&quit);
                if(clicked){
                    if(start){
                        /* If argv override was given, skip char select */
                        if(argc>=2){menu_load_slot=-1;running_menu=false;}
                        else page=MPAGE_CHARSELECT;
                    }
                    if(load) page=MPAGE_LOAD;
                    if(opts) page=MPAGE_OPTIONS;
                    if(quit){SDL_Quit();return 0;}
                }
            } else if(page==MPAGE_CHARSELECT){
                int sel=-1; bool back=false;
                page_charselect(&hud,W,H,mx,my,clicked,&charsel_hover,&sel,&back);
                if(back&&clicked){ page=MPAGE_MAIN; }
                if(sel>=0){
                    menu_char_sel=sel;
                    strncpy(char_path_buf,g_chardefs[sel].fbx_path,sizeof(char_path_buf)-1);
                    menu_load_slot=-1;
                    running_menu=false;
                    audio_play(SND_MENU_CLICK,90);
                }
            } else if(page==MPAGE_LOAD){
                int chosen=-1;bool back=false;
                page_load(&hud,W,H,mx,my,&chosen,&back);
                if(clicked){
                    if(chosen>=0){menu_load_slot=chosen;running_menu=false;}
                    if(back)page=MPAGE_MAIN;
                }
            } else if(page==MPAGE_OPTIONS){
                bool back=false,credits=false;
                page_options(&hud,W,H,mx,my,clicked,&g_options,slider_held,&active_slider,&back,&credits);
                /* Apply all settings live */
                audio_set_volumes(g_options.music_vol,g_options.sfx_vol);
                SDL_GL_SetSwapInterval(g_options.vsync?1:0);
                if(clicked){
                    if(g_options.fullscreen)
                        SDL_SetWindowFullscreen(win,SDL_WINDOW_FULLSCREEN_DESKTOP);
                    else
                        SDL_SetWindowFullscreen(win,0);
                    if(back){options_save(&g_options);page=MPAGE_MAIN;}
                    if(credits) page=MPAGE_CREDITS;
                    /* Apply resolution when not fullscreen */
                    if(!g_options.fullscreen){
                        SDL_SetWindowSize(win,g_options.res_w,g_options.res_h);
                        win_w=g_options.res_w; win_h=g_options.res_h;
                        hud_resize(&hud,win_w,win_h);
                        glViewport(0,0,win_w,win_h);
                    }
                }
            } else if(page==MPAGE_CREDITS){
                bool back=false;
                page_credits(&hud,W,H,mx,my,&back);
                if(clicked&&back) page=MPAGE_OPTIONS;
            }

            SDL_GL_SwapWindow(win);
            lmb_prev=lmb;
        }
    }

    /* ── LOADING SCREEN + ASSET LOADS ─────────────────────────── */
    int load_done=0, load_total=8;

    LOAD_STEP(&hud,win,"Loading character...",load_done,load_total);
    CharModel char_model; charmodel_init(&char_model,char_path);
    Scene *char_scene_ptr=&char_model.base; /* kept for debug_draw compat */
    AnimMap player_amap; animmap_init(&player_amap,NULL);
    LOAD_STEP(&hud,win,"Loading character...",++load_done,load_total);

    /* ── Enemy meshes — CharModel per type, fallback shares player base ── */
    CharModel enemy_models[ETYPE_COUNT];
    AnimMap   enemy_amaps[ETYPE_COUNT];
    bool      enemy_model_loaded[ETYPE_COUNT];
    memset(enemy_models,0,sizeof(enemy_models));
    memset(enemy_model_loaded,0,sizeof(enemy_model_loaded));
    for(int t=0;t<ETYPE_COUNT;t++) animmap_init(&enemy_amaps[t],NULL);
    /* Enemy CharModels are lazy-loaded on first encounter to avoid
       loading all 5 FBX files at startup. See lazy_load_enemy_model(). */

    LOAD_STEP(&hud,win,"Loading props...",load_done,load_total);
    g_prop_prog=prop_make_prog();
    /* g_props is a static global — already zeroed, but be explicit */
    memset(&g_props,0,sizeof(g_props));
    /* ── ADD YOUR PROPS HERE ─────────────────────────────────────────────
       prop_place(&g_props,"models/house.fbx",  -1910,0,-1795,  0,0,0,1.0, PROP_COLL_MESH, 0,0, 0,0);
       prop_place(&g_props,"models/fire.fbx",   -1900,0,-1800,  0,0,0,1.0, PROP_COLL_NONE, 0,0, 0,0);
       prop_place(&g_props,"models/tower.fbx",  -1890,0,-1810,  0,0,0,1.0, PROP_COLL_MESH, 0,0, 0,0);
       prop_place(&g_props,"models/steps.fbx",  -1895,0,-1793, 90,0,0,1.0, PROP_COLL_MESH, 0,0, 0,0);
       args: path, wx,wy,wz, rx,ry,rz, scale, coll,  cx,cz, hw,hd
       wy=0 auto-snaps to terrain. hw=0 auto-computes AABB.
    ─────────────────────────────────────────────────────────────────── */
    LOAD_STEP(&hud,win,"Loading props...",++load_done,load_total);

    LOAD_STEP(&hud,win,"Loading NPCs...",load_done,load_total);
    /* ── NPC model array — add paths here to register new NPC types ── */
    static const char *npc_model_paths[NPC_MODEL_MAX]={
        "npc.fbx",           /* 0 — default villager  */
        "npc_merchant.fbx",  /* 1 — merchant          */
        "npc_guard.fbx",     /* 2 — guard             */
        "npc_elder.fbx",     /* 3 — elder/quest giver */
        NULL,NULL,NULL,NULL  /* 4-7 — unused slots    */
    };
    CharModel g_npc_models[NPC_MODEL_MAX]; memset(g_npc_models,0,sizeof(g_npc_models));
    bool      g_npc_model_ok[NPC_MODEL_MAX]={0};
    for(int _mi=0;_mi<NPC_MODEL_MAX&&npc_model_paths[_mi];_mi++){
        charmodel_init(&g_npc_models[_mi],npc_model_paths[_mi]);
        if(g_npc_models[_mi].base.scene){
            g_npc_model_ok[_mi]=true;
            printf("[NPC] model[%d] loaded: %s\n",_mi,npc_model_paths[_mi]);
        } else {
            /* Fall back to model 0 at draw time if this slot failed */
            printf("[NPC] model[%d] not found: %s (will fallback)\n",_mi,npc_model_paths[_mi]);
        }
    }
    bool has_npc_model=g_npc_model_ok[0];
    LOAD_STEP(&hud,win,"Loading NPCs...",++load_done,load_total);

    LOAD_STEP(&hud,win,"Building terrain...",load_done,load_total);
    Terrain terrain; terrain_build(&terrain);
    /* Load any previously saved river — rebuilds terrain chunks if found */
    criver_load();
    memset(&g_sculpt,0,sizeof(g_sculpt));
    if(menu_load_slot>=0){
        sculpt_load();
    } else {
        g_sculpt_active=false;
        printf("[SculptEd] New game: ignoring %s\n", SCULPT_FILE);
    }
    grass_mask_load();
    worn_mask_load(&terrain);
    /* Rebuild terrain with sculpt deltas applied — must happen after sculpt_load */
    terrain_rebuild_all(&terrain);
    terrain_rebake_hmap(&terrain);
    wf_init();
    wf_load();
    if(g_criver.width<=0.0f) g_criver.width=CRIVER_WIDTH;
    wstyle_init();
    wzone_load();
    wzone_rebuild_vaos(&terrain);
    if(g_wzone_count>0) terrain_rebuild_all(&terrain);
    /* Load cobblestone paths */
    memset(&g_cpath,0,sizeof(g_cpath));
    g_cpath.width=4.0f;
    cpath_load();
    if(g_cpath.count>=2) path_rebuild_mesh();
    placer_load(&g_props);
    /* Override collision type for props that must be walkable mesh — overrides
       any stale AABB value saved in props_custom.txt                          */
    {
        const char *mesh_props[]={"house.fbx","steps.fbx","tower.fbx","house_large.fbx","House_1.fbx",NULL};
        for(int pi=0;pi<g_props.count;pi++){
            Prop *p=&g_props.list[pi];
            for(int mi=0;mesh_props[mi];mi++){
                if(strstr(p->path,mesh_props[mi])){
                    p->coll_type=PROP_COLL_MESH; break;
                }
            }
        }
    }
    stream_init(&g_stream,&g_props);  /* start background thread AFTER coll types are set */
    if(g_criver.count>0) terrain_rebuild_all(&terrain);
    GrassSystem g_grass; grass_init(&g_grass);
    WaterInteraction g_wint; wint_init(&g_wint);
    LOAD_STEP(&hud,win,"Building terrain...",++load_done,load_total);

    LOAD_STEP(&hud,win,"Initialising systems...",load_done,load_total);
    SkinProg   skin;     skinprog_init(&skin);
    Skybox     skybox;   skybox_init(&skybox,"skybox.png");
    ParticleSystem ps;   ps_init(&ps);
    wf_apply_emitters(&ps);
    WeatherSystem  wx;   wx_init(&wx);
    WindSystem     g_wind; wind_init(&g_wind);
    PuddleSystem   g_puddles; puddle_init(&g_puddles);
    FootprintSystem g_footprints; footprint_init(&g_footprints);
    /* Atmospheric state — updated each frame, drives fog/wet/overcast */
    float g_wetness=0.0f, g_fog_density=0.0005f, g_overcast=0.0f;
    LightingSystem ls;   lighting_init(&ls);
    QuestSystem    qs;   quest_init(&qs);
    AbilitySystem  as;   ability_init(&as);
    ShadowMap      sm;   shadow_init(&sm,&ls);
        /* Wide cascade shadow map — 1024px, covers far terrain */
        ShadowMap      sm_wide;
        memset(&sm_wide,0,sizeof(sm_wide));
        {
            glGenFramebuffers(1,&sm_wide.fbo);
            glGenTextures(1,&sm_wide.tex);
            glBindTexture(GL_TEXTURE_2D,sm_wide.tex);
            glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,1024,1024,0,GL_DEPTH_COMPONENT,GL_FLOAT,NULL);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_MODE,GL_NONE);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
            float border[]={1,1,1,1};
            glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,border);
            glBindFramebuffer(GL_FRAMEBUFFER,sm_wide.fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,sm_wide.tex,0);
            glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);
            glBindFramebuffer(GL_FRAMEBUFFER,0);
            sm_wide.prog_depth=sm.prog_depth;
            sm_wide.prog_skin_depth=sm.prog_skin_depth;
            printf("[Shadow] Wide cascade 1024x1024 created\n");
        }
        ls.shadow_tex_wide=sm_wide.tex;
        ls.light_vp_wide=mat4_identity();
    LineRenderer   lr;   lr_init(&lr);
    PerfCounter    perf; memset(&perf,0,sizeof(perf));
    LOAD_STEP(&hud,win,"Initialising systems...",++load_done,load_total);

    LOAD_STEP(&hud,win,"Placing world objects...",load_done,load_total);

    #if 0
    /* ── Village prop population ───────────────────────────────
       Prop FBX files expected next to executable:
         house.fbx      — standard dwelling
         house_large.fbx — larger building (tavern/hall)
         fence.fbx      — fence segment (~2u long)
         barrel.fbx     — decorative barrel
         crate.fbx      — decorative crate
         torch.fbx      — wall torch / post torch
         well.fbx       — village well centrepiece
         tent.fbx       — market stall / tent
         tower.fbx      — guard tower
       Missing files are skipped silently.
    ──────────────────────────────────────────────────────────── */
    #define VP(path,wx,wy,wz,ry,sc,coll) \
        prop_register(&g_props,(path),(wx),(wy),(wz), 0,(ry),0,(sc),(coll),0,0,0,0)
    #define VPH(path,wx,wz,ry,sc,coll) \
        VP((path),(wx),0,(wz),(ry),(sc),(coll))   /* wy=0 auto-snaps to terrain */

    /* Helper: place a ring of fence segments around a centre */
    #define FENCE_RING(cx,cz,rad,n,fname) do{ \
        for(int _fi=0;_fi<(n);_fi++){ \
            float _a=(float)_fi/(float)(n)*(float)(2*M_PI); \
            float _fx=(cx)+cosf(_a)*(rad), _fz=(cz)+sinf(_a)*(rad); \
            float _ry=(_a*(180.0f/(float)M_PI)); \
            VPH((fname),_fx,_fz,_ry,1.0f,PROP_COLL_NONE); \
        } \
    }while(0)

    /* place_village: populates one settlement with props.
       vi = index into VILLAGES table.
       Density and layout scale with flat_radius.            */
    for(int vi=0;vi<N_VILLAGES;vi++){
        if(vi==START_VILLAGE_INDEX)continue; /* starter village is hand-placed below */
        float vx=(float)VILLAGES[vi][0];
        float vz=(float)VILLAGES[vi][1];
        float vr=(float)VILLAGES[vi][2]; /* flat radius */
        float vy_base=(float)VILLAGES[vi][4];
        (void)vy_base;

        /* Seed per-village RNG so layout is deterministic */
        unsigned int vseed=(unsigned int)(vi*7919u+(unsigned int)fabsf(vx)*31u+(unsigned int)fabsf(vz)*17u);
        #define VRAND() (vseed=vseed*1664525u+1013904223u, (float)(vseed>>16)/(float)0xFFFF)

        bool is_callicut =(vi==1);
        bool is_valdoa   =(vi==6);
        bool is_city     =(vi==10); /* Kerone City — large flat radius */

        /* 1. Central well or fire pit */
        if(is_callicut){
            VPH("well.fbx",    vx,    vz,    0,   1.0f, PROP_COLL_AABB);
        } else if(is_valdoa){
            VPH("tower.fbx",   vx,    vz,    0,   2.0f, PROP_COLL_AABB);
            VPH("tower.fbx",   vx+20, vz,   45,   2.0f, PROP_COLL_AABB);
            VPH("tower.fbx",   vx-20, vz,  -45,   2.0f, PROP_COLL_AABB);
        } else {
            VPH("well.fbx",    vx,    vz,    0,   1.0f, PROP_COLL_AABB);
        }

        /* 2. Main hall / tavern — one per village, offset slightly */
        float hall_r=vr*0.4f;
        float hall_angle=VRAND()*(float)(2*M_PI);
        float hx=vx+cosf(hall_angle)*hall_r;
        float hz=vz+sinf(hall_angle)*hall_r;
        VPH(is_city||is_valdoa?"house_large.fbx":"house_large.fbx",
            hx, hz, VRAND()*360.0f, 1.0f, PROP_COLL_MESH);

        /* 3. Houses — scatter inside flat radius */
        int n_houses = is_city?8 : is_valdoa?4 : (int)(vr/8.0f)+2;
        n_houses = n_houses>12?12:n_houses;
        for(int h=0;h<n_houses;h++){
            float ang=VRAND()*(float)(2*M_PI);
            float dist=VRAND()*vr*0.85f+4.0f;
            float hpx=vx+cosf(ang)*dist;
            float hpz=vz+sinf(ang)*dist;
            float rot=VRAND()*360.0f;
            float sc=0.9f+VRAND()*0.3f;
            VPH("models/village/House_FBX/House_1/House_1.fbx", hpx, hpz, rot, sc, PROP_COLL_MESH);
        }

        /* 4. Barrels and crates — clustered near houses */
        int n_barrels=is_city?12:(int)(vr/10.0f)+2;
        for(int b=0;b<n_barrels;b++){
            float ang=VRAND()*(float)(2*M_PI);
            float dist=VRAND()*vr*0.7f+3.0f;
            VPH("barrel.fbx",
                vx+cosf(ang)*dist, vz+sinf(ang)*dist,
                VRAND()*360.0f, 0.8f+VRAND()*0.4f, PROP_COLL_NONE);
            if(VRAND()>0.5f)
                VPH("crate.fbx",
                    vx+cosf(ang)*dist+VRAND()*2-1,
                    vz+sinf(ang)*dist+VRAND()*2-1,
                    VRAND()*360.0f, 0.9f, PROP_COLL_NONE);
        }

        /* 5. Torches — ring around perimeter */
        int n_torches=is_city?12:(int)(vr/7.0f)+3;
        for(int t=0;t<n_torches;t++){
            float ang=(float)t/(float)n_torches*(float)(2*M_PI)+VRAND()*0.3f;
            float dist=vr*0.75f+VRAND()*6.0f;
            VPH("models/torch/torch.fbx",
                vx+cosf(ang)*dist, vz+sinf(ang)*dist,
                ang*(180.0f/(float)M_PI), 1.0f, PROP_COLL_NONE);
        }

        /* 6. Guard towers at city/castle corners */
        if(is_city||is_valdoa){
            float tr=vr*0.9f;
            for(int t=0;t<4;t++){
                float ang=(float)t*((float)M_PI*0.5f)+(float)M_PI*0.25f;
                VPH("tower.fbx",
                    vx+cosf(ang)*tr, vz+sinf(ang)*tr,
                    ang*(180.0f/(float)M_PI), 1.0f, PROP_COLL_AABB);
            }
        }

        /* 7. Market tents — one or two per medium+ village */
        if(vr>=30.0f){
            int n_tents=is_city?4:1+(int)(VRAND()*2);
            for(int t=0;t<n_tents;t++){
                float ang=VRAND()*(float)(2*M_PI);
                float dist=VRAND()*vr*0.5f+5.0f;
                VPH("tent.fbx",
                    vx+cosf(ang)*dist, vz+sinf(ang)*dist,
                    VRAND()*360.0f, 1.0f, PROP_COLL_NONE);
            }
        }

        #undef VRAND
    }
    #undef VP
    #undef VPH
    #undef FENCE_RING

    printf("[Village] Placed %d props across %d settlements\n",g_props.count,N_VILLAGES);
    #endif

    /* Torch lights — warm orange glow at starter village torches */
    {Vec3 torch_col={1.0f,0.55f,0.15f};
     float _cvx=(float)VILLAGES[START_VILLAGE_INDEX][0], _cvz=(float)VILLAGES[START_VILLAGE_INDEX][1];
     float tr=VILLAGES[START_VILLAGE_INDEX][2]*0.75f;
     int n_tl=6;
     for(int t=0;t<n_tl;t++){
         float ang=(float)t/(float)n_tl*(float)(2*M_PI);
         float tx=_cvx+cosf(ang)*tr, tz=_cvz+sinf(ang)*tr;
         lighting_add_point(&ls,(Vec3){tx,terrain_h(tx,tz)+2.0f,tz},torch_col,14.0f,1.2f);
     }
    }

    /* ════════════════════════════════════════════════════════════
       STARTER VILLAGE — HAND-PLACED STARTING AREA
       Use the F8 prop editor in-game to find XZ coordinates,
       then hardcode them here.  cf_wx/cf_wz is the village centre.
       ════════════════════════════════════════════════════════════ */
    float cf_wx=(float)VILLAGES[START_VILLAGE_INDEX][0], cf_wz=(float)VILLAGES[START_VILLAGE_INDEX][1];

    /* Campfire — anchor point, defines respawn */
    Vec3 cf_pos={cf_wx, terrain_h(cf_wx,cf_wz)+0.3f, cf_wz};
    Campfire campfire=campfire_create(&ps,&ls,cf_pos);(void)campfire;

    /* ── PROPS — place manually using prop_register or placer_load ──
       Format: prop_register(&g_props, path, x,y,z, rx,ry,rz, scale, coll, cx,cz,hw,hd)
       y=0 auto-snaps to terrain.  Example:
         prop_register(&g_props,"models/village/House_FBX/House_1/House_1.fbx",
                       cf_wx+10, 0, cf_wz+5,  0,45,0, 1.0f, PROP_COLL_MESH, 0,0,0,0);
    ───────────────────────────────────────────────────────────────── */

    /* ── NPCS ───────────────────────────────────────────────────── */
    NpcSystem npcs; npc_init(&npcs);
    for(int si=0;si<SHOP_PROFILE_COUNT;si++) shop_def_init(&g_shop_defs[si],si);

    /* ── Add your NPCs here.  Pattern:
         int idx = npc_add(&npcs, (Vec3){x, terrain_h(x,z)+0.1f, z}, yaw_radians, "Name");
         npcs.npcs[idx].model_idx = 0;   // which model from npc_model_paths[]
         npc_add_line(&npcs, idx, "Dialogue line 1.");
         npc_add_line(&npcs, idx, "Dialogue line 2.");
         npcs.npcs[idx].is_merchant = true;  // optional: opens shop
         npc_attach_quest(&npcs, idx, quest_id); // optional: attach quest
    ─────────────────────────────────────────────────────────────── */

    /* ── QUESTS — define before attaching to NPCs ───────────────── */
    /* Example:
         Quest *q1 = quest_add(&qs, "Quest Name", "Quest description.");
         quest_kill_obj(q1, "Kill 3 enemies", 3);
         q1->gold = 100; q1->xp = 50;
         npc_attach_quest(&npcs, npc_idx, q1->id);
    ─────────────────────────────────────────────────────────────── */

    /* ── NPC PATROLS — add after NPCs ───────────────────────────── */
    /* Example (npc index 0, speed 1.5, wait 2s at each waypoint):
         npc_patrol_add(0, 1.5f, 2.0f);
         npc_patrol_waypoint(0, (Vec3){cf_wx+10, terrain_h(cf_wx+10,cf_wz)+0.05f, cf_wz});
         npc_patrol_waypoint(0, (Vec3){cf_wx-10, terrain_h(cf_wx-10,cf_wz)+0.05f, cf_wz});
    ─────────────────────────────────────────────────────────────── */

    /* Load NPC spawns saved by editor */
    memset(&g_npc_spawns,0,sizeof(g_npc_spawns));
    npc_spawns_load(&g_npc_spawns);
    npc_spawns_apply(&g_npc_spawns,&npcs);
    for(int i=0;i<npcs.count;i++)npcs.npcs[i].anim_idx=0;

    /* ── Birds ─────────────────────────────────────────────────── */
    bird_system_init(&g_birds,"models/bird.fbx","bird_sprite.png");

    /* ── Animal herds ───────────────────────────────────────── */
    memset(&g_animals,0,sizeof(g_animals));
    /* Grass animals — graze on the plain south of spawn */
    animal_add_herd(&g_animals,
        "models/grass_animal.fbx", "Grazer",
        (Vec3){50.0f,0,80.0f}, /* home centre */
        60.0f,                  /* home radius  */
        8,                      /* herd size    */
        1.4f, 5.5f,             /* walk / flee speed */
        1.0f,                   /* scale        */
        0.55f,0.42f,0.18f);     /* billboard tint (tan) */
    /* Flock circling high above the village */
    {Vec3 c={cf_wx, terrain_h(cf_wx,cf_wz)+30.0f, cf_wz};
     for(int i=0;i<6;i++)
         bird_add(&g_birds,c,30.0f+i*4.0f,5.0f+i*2.0f,
                  0.35f+i*0.03f, BIRD_CIRCLE);
    }
    /* Pair doing lazy figure-8 over the pond */
    {Vec3 c={cf_wx-100, terrain_h(cf_wx-100,cf_wz-100)+18.0f, cf_wz-100};
     bird_add(&g_birds,c,22.0f,0,0.28f,BIRD_FIGURE8);
     bird_add(&g_birds,c,18.0f,3.0f,0.32f,BIRD_FIGURE8);
    }
    /* Lone wanderer drifting toward the forest edge */
    {Vec3 c={cf_wx+150, terrain_h(cf_wx+150,cf_wz+50)+24.0f, cf_wz+50};
     bird_add(&g_birds,c,50.0f,0,0.18f,BIRD_WANDER);
    }

    /* ── Animals ────────────────────────────────────────────────── */
    memset(&g_animals,0,sizeof(g_animals));
    animal_add_herd(&g_animals,"models/grass_animal.fbx","Grazer",
        (Vec3){cf_wx+80,0,cf_wz+60},60.0f,8, 1.4f,5.5f,1.0f, 0.55f,0.42f,0.18f);
    animal_add_herd(&g_animals,"models/deer.fbx","Deer",
        (Vec3){cf_wx+120,0,cf_wz-80},50.0f,5, 2.2f,7.0f,1.0f, 0.65f,0.45f,0.25f);
    animal_add_herd(&g_animals,"models/sheep.fbx","Sheep",
        (Vec3){cf_wx-60,0,cf_wz+50},40.0f,6, 1.2f,4.5f,0.9f, 0.85f,0.82f,0.75f);
    /* Hostile — wolf pack in the deep forest */
    {AnimalHerd *wolves=animal_add_herd(&g_animals,"models/wolf.fbx","Wolf",
        (Vec3){cf_wx+280,0,cf_wz-120},70.0f,4, 3.5f,6.0f,1.0f, 0.25f,0.22f,0.20f);
     animal_set_hostile(wolves, 16.0f,1.8f,6.5f, 8.0f,1.4f);}
    /* Hostile — boars near the forest fringe */
    {AnimalHerd *boars=animal_add_herd(&g_animals,"models/boar.fbx","Boar",
        (Vec3){cf_wx+180,0,cf_wz-60},50.0f,3, 2.0f,5.0f,1.0f, 0.38f,0.28f,0.18f);
     animal_set_hostile(boars, 10.0f,1.6f,5.0f, 12.0f,2.0f);}

    /* ── Forest ─────────────────────────────────────────────────── */
    /* Forest runs along the north-east edge of the spawn flat area.
       Place tree.fbx + tree_billboard.png next to the executable.
       The system works fine without them — billboards use a procedural
       green-oval fallback and close meshes skip rendering.            */
    forest_init(&g_forest,"models/village/Tree_FBX/Birch1.fbx","tree_billboard.png");
    /* Dense belt north of village: X -1700..-1600, Z -2000..-1600 */
    forest_fill_rect(&g_forest, cf_wx+80,cf_wz-250, cf_wx+300,cf_wz+150,
                     180, 1.0f, 2.2f);
    /* Thinner fringe east — transition from plains to forest */
    forest_fill_rect(&g_forest, cf_wx+300,cf_wz-150, cf_wx+420,cf_wz+100,
                     60, 0.9f, 1.8f);
    /* Small copse west of the pond for cover */
    forest_ring(&g_forest,(Vec3){cf_wx-140,0,cf_wz-80},40.0f,14,1.0f,1.6f);
    forest_load(&g_forest,&g_props); /* replaces defaults when authored forest exists */

    /* Lava ambient lights – deep in the mountains */
    {Vec3 lc={1.0f,0.25f,0.0f};
     float lx=-1400,lz=-2300;lighting_add_point(&ls,(Vec3){lx,terrain_h(lx,lz)+2.0f,lz},lc,120.0f,3.0f);
     lx=-1700;lz=-2050;      lighting_add_point(&ls,(Vec3){lx,terrain_h(lx,lz)+2.0f,lz},lc,100.0f,2.5f);}

    /* Find Valdoa quest — auto-started (exploration, no NPC needed) */
    Quest *q2=quest_add(&qs,"Find Valdoa","Reach the castle at Valdoa.");quest_reach_obj(q2,"Reach Valdoa",(Vec3){50,0,-800},80.0f);q2->xp=150;quest_start(&qs,q2->id);

    /* Compute real durations by resolving through CharModel anim entries */
    float attack_dur=0.8f,attack2_dur=0.8f,hit_dur=0.5f,death_dur=2.0f;
    {
        /* Helper lambda via block: get duration for a slot */
        #define ANIM_DUR(slot) ({ \
            AnimEntry *_ae=&char_model.anims[slot]; \
            float _d=0.5f; \
            if(_ae->scene&&_ae->stack>=0){ _d=scene_anim_duration(_ae->scene,_ae->stack); if(_d<=0.0f)_d=0.5f; } \
            _d; })
        attack_dur =ANIM_DUR(ANIM_ATTACK);
        attack2_dur=ANIM_DUR(ANIM_ATTACK2);
        hit_dur    =ANIM_DUR(ANIM_HIT);
        death_dur  =ANIM_DUR(ANIM_DEATH);
        #undef ANIM_DUR
        printf("[Anim] attack=%.3fs  attack2=%.3fs  hit=%.3fs  death=%.3fs\n",
               (double)attack_dur,(double)attack2_dur,(double)hit_dur,(double)death_dur);
    }

    Player player={0};
    /* Spawn at Callicut village center (flat starter area) */
    player.phys.pos=(Vec3){cf_wx,0,cf_wz};
    player.phys.height=1.8f;player.phys.radius=0.4f;
    player.health=player.max_health=100.0f;player.anim_idx=player_amap.idle;player.prev_anim_idx=player_amap.idle;player.anim_speed=1.0f;
    player.phys.pos.y=terrain_h(player.phys.pos.x,player.phys.pos.z)+0.1f;

    Enemy enemies[MAX_ENEMIES];memset(enemies,0,sizeof(enemies));int ne=0;

    Camera3P cam={0};cam.yaw=0.3f;cam.pitch=0.4f;cam.dist=10.0f;cam.target=player.phys.pos;
    bool cam_drag=false;int cam_lx=0,cam_ly=0;
    bool show_debug=false;
    /* Prop editor state (F3 mode) */
    bool prop_edit=false;
    int  prop_sel=0;
    /* River editor state (F4 mode) */
    bool river_edit=false;
    bool river_lmb_prev=false, river_rmb_prev=false;
    /* Path editor state (F1 mode) */
    bool path_edit=false;
    bool path_lmb_prev=false, path_rmb_prev=false;
    /* Water zone editor state (F2 mode) */
    bool wzone_edit=false;
    bool wzone_lmb_prev=false;
    int  wzone_sel=-1;   /* index of currently selected zone (-1=none) */
    /* Waterfall editor sub-mode (Tab while in F2) */
    bool wf_edit_mode=false;  /* false=water zones, true=waterfalls */
    int  wf_sel=-1;           /* selected waterfall index */
    bool wf_lmb_prev=false;
    float wf_place_width=12.0f, wf_place_height=18.0f, wf_place_yaw=0.0f;
    /* Prop placer state (F5 mode) */
    bool prop_place_mode=false;
    bool forest_edit=false;
    bool spawn_edit_mode=false;  /* F12 spawn editor */
    EnemyType spawn_sel_type=ETYPE_KNIGHT; /* currently chosen enemy type */
    bool npc_edit_mode=false;     /* F11 NPC editor */
    int  npc_sel_type=0;          /* selected NPC model index */
    bool npc_ed_lmb_prev=false, npc_ed_rmb_prev=false;
    float npc_ed_yaw=0.0f;        /* current placement yaw */
    /* ── Terrain sculpt editor (F9) ─────────────────────────── */
    bool sculpt_edit=false;
    int  sculpt_mode=0;          /* 0=raise 1=lower 2=flatten 3=smooth */
    float sculpt_radius=30.0f;   /* brush radius in world units */
    float sculpt_strength=0.04f; /* delta per frame while held */
    float sculpt_flatten_h=0.0f; /* target height for flatten mode */
    bool sculpt_lmb_prev=false;
    static float sculpt_undo[SCULPT_RES*SCULPT_RES]; /* one-level undo */
    bool sculpt_has_undo=false;
    static uint8_t grass_mask_undo[GRASS_MASK_RES*GRASS_MASK_RES];
    bool grass_mask_has_undo=false;
    static uint8_t worn_mask_undo[WORN_MASK_RES*WORN_MASK_RES];
    bool worn_mask_has_undo=false;
    int  npc_ed_wp_npc=-1;        /* index of NPC being given waypoints, -1=none */
    bool npc_ed_wp_mode=false;    /* P toggles waypoint placement mode */
    int  npc_ed_dlg_npc=-1;       /* NPC being edited for dialogue */
    bool npc_ed_dlg_mode=false;   /* D toggles dialogue editor mode */
    char npc_ed_dlg_buf[NPC_LINE_LEN]={0}; /* current text input buffer */
    int  npc_ed_dlg_len=0;        /* length of text in buffer */
    int  npc_ed_dlg_sel=-1;       /* selected line index (-1=new line) */
    bool spawn_lmb_prev=false, spawn_rmb_prev=false;
    float forest_brush_radius=36.0f;
    int   forest_brush_density=3;
    bool fishing_space_prev=false;
    placer_init(&g_placer);
    /* Spawn editor — load custom spawn points if file exists, apply to enemies */
    memset(&g_spawns,0,sizeof(g_spawns));
    spawns_load(&g_spawns);
    if(g_spawns.count>0) spawns_apply(&g_spawns,enemies,&ne,enemy_amaps);
    /* Find boss pointer — search array since spawns_apply may reorder slots */
    Enemy *boss=NULL;
    for(int i=0;i<ne;i++) if(enemies[i].etype==ETYPE_BOSS&&enemies[i].active){boss=&enemies[i];break;}
    SaveNotify save_note;memset(&save_note,0,sizeof(save_note));
    SaveUI save_ui;memset(&save_ui,0,sizeof(save_ui));
    MapScreen map_screen;memset(&map_screen,0,sizeof(map_screen));map_screen.zoom=1.0f;
    /* Pause menu */
    memset(&g_pause,0,sizeof(g_pause));
    /* Room renderer + interiors */
    room_renderer_init(&g_room_rend);
    interiors_setup();
    Inventory  inv;inv_init(&inv);
    /* Starting gear — iron sword + a handful of health potions */
    inv_add(&inv,ITEM_SWORD_IRON,1);
    inv_add(&inv,ITEM_POTION_HP, 3);
    WorldItemList world_items;wi_init(&world_items);
    /* ── Fixed world loot ─────────────────────────────────────────
       Scattered around the world so exploration is rewarded.
       Positions are relative to cf_wx/cf_wz (Callicut campfire).
       All Y values auto-snap via terrain_h at spawn time.        */
    {
        /* Near the spawn village — easy early loot */
        wi_spawn(&world_items,ITEM_POTION_HP,  2,(Vec3){cf_wx+45,  terrain_h(cf_wx+45, cf_wz+18)+0.3f,   cf_wz+18});
        wi_spawn(&world_items,ITEM_GOLD_COIN,  8,(Vec3){cf_wx-30,  terrain_h(cf_wx-30, cf_wz-12)+0.3f,   cf_wz-12});
        wi_spawn(&world_items,ITEM_HELM_LEATHER,1,(Vec3){cf_wx+12, terrain_h(cf_wx+12, cf_wz+55)+0.3f,   cf_wz+55});

        /* Forest edge — mid loot */
        wi_spawn(&world_items,ITEM_POTION_HP,  3,(Vec3){cf_wx+150, terrain_h(cf_wx+150,cf_wz-60)+0.3f,   cf_wz-60});
        wi_spawn(&world_items,ITEM_CHEST_LEATHER,1,(Vec3){cf_wx+200,terrain_h(cf_wx+200,cf_wz+30)+0.3f,  cf_wz+30});
        wi_spawn(&world_items,ITEM_GOLD_COIN, 15,(Vec3){cf_wx+90,  terrain_h(cf_wx+90, cf_wz-180)+0.3f,  cf_wz-180});

        /* Mountain pass — harder area, better rewards */
        wi_spawn(&world_items,ITEM_SWORD_STEEL, 1,(Vec3){cf_wx-80, terrain_h(cf_wx-80, cf_wz-300)+0.3f,  cf_wz-300});
        wi_spawn(&world_items,ITEM_POTION_FULL, 2,(Vec3){cf_wx+40, terrain_h(cf_wx+40, cf_wz-400)+0.3f,  cf_wz-400});
        wi_spawn(&world_items,ITEM_BOOTS_LEATHER,1,(Vec3){cf_wx-200,terrain_h(cf_wx-200,cf_wz-250)+0.3f,  cf_wz-250});
        wi_spawn(&world_items,ITEM_GOLD_COIN,  30,(Vec3){cf_wx+120,terrain_h(cf_wx+120,cf_wz-500)+0.3f,  cf_wz-500});

        /* Near Valdoa — endgame area */
        wi_spawn(&world_items,ITEM_CHEST_IRON,  1,(Vec3){70,       terrain_h(70,  -750)+0.3f,             -750});
        wi_spawn(&world_items,ITEM_HELM_IRON,   1,(Vec3){30,       terrain_h(30,  -820)+0.3f,             -820});
        wi_spawn(&world_items,ITEM_POTION_FULL, 3,(Vec3){100,      terrain_h(100, -780)+0.3f,             -780});
    }
    LevelSystem lvlsys;level_init(&lvlsys);
    FishingSystem fishing; fishing_init(&fishing);
    float base_max_hp=100.0f; /* base before armor bonuses */
    float prop_step_rot=5.0f;   /* degrees per keypress */
    float prop_step_scale=0.05f;
    /* Projectile system */
    ProjList projs;memset(&projs,0,sizeof(projs));
    float total_time=0;
    int   frame_count=0;
    float time_scale=1.0f;
    audio_play_music(MUS_EXPLORE,2000); /* fade in explore music as game starts */
    /* Auto-load the slot chosen on the main menu (if it has a save) */
    if(menu_load_slot>=0){
        if(game_load(menu_load_slot,&player,&qs,&as,enemies,ne,&total_time,&player_amap,enemy_amaps,&inv,base_max_hp,&lvlsys)){
            char nb[48];snprintf(nb,sizeof(nb),"Loaded — Slot %d",menu_load_slot+1);
            save_notify(&save_note,nb);
            inv_recalc(&inv,&player,base_max_hp); /* apply equipped item bonuses */
        }
        /* else: slot was empty, just start new game in that slot */
    }
    Uint64 last=SDL_GetPerformanceCounter();bool running=true;
    bool restart_to_menu=false; /* set when user picks Return to Menu */
    float death_fade=0.0f;      /* 0..1 fade-in timer for death screen */
    bool campfire_menu_open=false; /* E near campfire opens rest menu */
    bool campfire_lmb_prev=false;
    float intro_fade=1.0f;      /* 1=black, fades to 0 over ~1.5s on game start */

    while(running){
        Uint64 now=SDL_GetPerformanceCounter();
        float dt=clampf((float)(now-last)/(float)SDL_GetPerformanceFrequency(),0,0.05f);
        last=now;
        /* Hold T to fast-forward the day/night cycle (20×) */
        {const uint8_t *k=SDL_GetKeyboardState(NULL);time_scale=k[SDL_SCANCODE_T]?20.0f:1.0f;}
        total_time+=dt*time_scale;
        frame_count++;

        bool npc_dialogue_capture=(npc_ed_dlg_mode&&npc_ed_dlg_npc>=0);
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            switch(ev.type){
            case SDL_QUIT:running=false;break;
            case SDL_TEXTINPUT:
                if(npc_ed_dlg_mode&&npc_ed_dlg_npc>=0){
                    for(int ci=0;ev.text.text[ci]&&npc_ed_dlg_len<NPC_LINE_LEN-1;ci++)
                        npc_ed_dlg_buf[npc_ed_dlg_len++]=ev.text.text[ci];
                    npc_ed_dlg_buf[npc_ed_dlg_len]=0;
                }
                break;
            case SDL_KEYDOWN:
                /* ── Dialogue editor intercepts keys when active ── */
                if(npc_ed_dlg_mode&&npc_ed_dlg_npc>=0){
                    NpcSpawnPoint *dsp=&g_npc_spawns.points[npc_ed_dlg_npc];
                    SDL_Keycode dk=ev.key.keysym.sym;
                    if(dk==SDLK_BACKSPACE&&npc_ed_dlg_len>0){
                        npc_ed_dlg_buf[--npc_ed_dlg_len]=0;
                    } else if(dk==SDLK_RETURN||dk==SDLK_KP_ENTER){
                        if(npc_ed_dlg_len>0){
                            if(npc_ed_dlg_sel>=0&&npc_ed_dlg_sel<dsp->dialogue_count)
                                strncpy(dsp->dialogue[npc_ed_dlg_sel],npc_ed_dlg_buf,NPC_LINE_LEN-1);
                            else if(dsp->dialogue_count<NPC_LINES_MAX)
                                strncpy(dsp->dialogue[dsp->dialogue_count++],npc_ed_dlg_buf,NPC_LINE_LEN-1);
                            npc_ed_dlg_buf[0]=0; npc_ed_dlg_len=0; npc_ed_dlg_sel=-1;
                            npc_spawns_save(&g_npc_spawns);
                            /* Sync dialogue to live NPC: find by position match */
                            for(int ni=0;ni<npcs.count;ni++){
                                float dx=npcs.npcs[ni].pos.x-dsp->pos.x;
                                float dz=npcs.npcs[ni].pos.z-dsp->pos.z;
                                if(dx*dx+dz*dz<1.0f){
                                    npcs.npcs[ni].line_count=0;
                                    for(int di=0;di<dsp->dialogue_count;di++)
                                        npc_add_line(&npcs,ni,dsp->dialogue[di]);
                                    break;
                                }
                            }
                        }
                    } else if(dk==SDLK_ESCAPE){
                        npc_ed_dlg_mode=false;
                        npc_ed_dlg_npc=-1;
                        npc_ed_dlg_buf[0]=0; npc_ed_dlg_len=0; npc_ed_dlg_sel=-1;
                        SDL_StopTextInput();
                        printf("[NpcEd] Dialogue mode OFF\n");
                    } else if(dk==SDLK_DELETE&&npc_ed_dlg_sel>=0&&npc_ed_dlg_sel<dsp->dialogue_count){
                        for(int di=npc_ed_dlg_sel;di<dsp->dialogue_count-1;di++)
                            strncpy(dsp->dialogue[di],dsp->dialogue[di+1],NPC_LINE_LEN-1);
                        dsp->dialogue_count--; npc_ed_dlg_sel=-1;
                        npc_ed_dlg_buf[0]=0; npc_ed_dlg_len=0;
                        npc_spawns_save(&g_npc_spawns);
                        /* Sync to live NPC */
                        for(int ni=0;ni<npcs.count;ni++){
                            float dx=npcs.npcs[ni].pos.x-dsp->pos.x;
                            float dz=npcs.npcs[ni].pos.z-dsp->pos.z;
                            if(dx*dx+dz*dz<1.0f){
                                npcs.npcs[ni].line_count=0;
                                for(int di=0;di<dsp->dialogue_count;di++)
                                    npc_add_line(&npcs,ni,dsp->dialogue[di]);
                                break;
                            }
                        }
                    } else if(dk==SDLK_UP){
                        if(npc_ed_dlg_sel==-1&&dsp->dialogue_count>0)
                            npc_ed_dlg_sel=dsp->dialogue_count-1;
                        else if(npc_ed_dlg_sel>0)
                            npc_ed_dlg_sel--;
                        if(npc_ed_dlg_sel>=0){
                            strncpy(npc_ed_dlg_buf,dsp->dialogue[npc_ed_dlg_sel],NPC_LINE_LEN-1);
                            npc_ed_dlg_len=(int)strlen(npc_ed_dlg_buf);
                        }
                    } else if(dk==SDLK_DOWN){
                        if(npc_ed_dlg_sel<dsp->dialogue_count-1){
                            npc_ed_dlg_sel++;
                            strncpy(npc_ed_dlg_buf,dsp->dialogue[npc_ed_dlg_sel],NPC_LINE_LEN-1);
                            npc_ed_dlg_len=(int)strlen(npc_ed_dlg_buf);
                        } else { npc_ed_dlg_sel=-1; npc_ed_dlg_buf[0]=0; npc_ed_dlg_len=0; }
                    } else if(dk==SDLK_m){
                        dsp->is_merchant=!dsp->is_merchant;
                        npc_spawns_save(&g_npc_spawns);
                    }
                    break; /* consume key — don't fall through to game hotkeys */
                }
                switch(ev.key.keysym.sym){
                case SDLK_ESCAPE:
                    if(save_ui.open){save_ui.open=false;audio_play(SND_MENU_CLICK,70);break;}
                    if(map_screen.open){map_screen.open=false;break;}
                    if(inv.open){inv.open=false;break;}
                    if(g_shop.open){shop_close(&g_shop);break;}
                    if(npcs.open){npcs.open=false;npcs.active_npc=-1;break;}
                    /* Toggle pause menu */
                    if(g_pause.state==PM_NONE){
                        g_pause.state=PM_ROOT;
                        g_pause.lmb_prev=true; /* suppress click-through */
                        audio_play(SND_MENU_CLICK,70);
                    } else {
                        g_pause.state=PM_NONE;
                        audio_play(SND_MENU_CLICK,70);
                    }
                    break;
                case SDLK_m:
                    if(!g_shop.open&&!inv.open&&!save_ui.open){
                        map_screen.open=!map_screen.open;
                        if(map_screen.open){map_screen.scroll_x=0;map_screen.scroll_y=0;map_screen.zoom=1.0f;}
                        audio_play(SND_MENU_CLICK,70);
                    } break;
                case SDLK_z:
                    if(river_edit&&g_criver.count>0){
                        g_criver.count--;
                        g_criver.dirty=true;
                        terrain_rebuild_all(&terrain);
                        printf("[RiverEd] Undo — %d points\n",g_criver.count);
                    } else if(path_edit&&g_cpath.count>0){
                        g_cpath.count--;
                        if(g_cpath.count>=2) path_rebuild_mesh();
                        else { g_pmesh.valid=false; }
                        printf("[PathEd] Undo — %d points\n",g_cpath.count);
                    } break;
                case SDLK_c:
                    if(river_edit){
                        g_criver.count=0; g_criver.dirty=false;
                        terrain_rebuild_all(&terrain);
                        printf("[RiverEd] Cleared all points\n");
                    } else if(path_edit){
                        g_cpath.count=0; g_pmesh.valid=false;
                        printf("[PathEd] Cleared all points\n");
                    } else if(wzone_edit){
                        if(wf_edit_mode){
                            g_wfalls.count=0; wf_sel=-1;
                            printf("[WFalls] Cleared\n");
                        } else {
                            g_wzone_count=0; wzone_sel=-1;
                            wzone_rebuild_vaos(&terrain);
                        }
                        printf("[WZoneEd] Cleared all zones\n");
                    } break;
                case SDLK_s:
                    if(river_edit){ criver_save(); terrain_rebuild_all(&terrain); terrain_rebake_hmap(&terrain); printf("[RiverEd] Saved + heightmap baked\n"); }
                    else if(path_edit){ cpath_save(); }
                    else if(wzone_edit){
                        if(wf_edit_mode) wf_save();
                        else { wzone_save(); terrain_rebuild_all(&terrain); terrain_rebake_hmap(&terrain); printf("[WZoneEd] Saved + heightmap baked\n"); }
                    }
                    else if(prop_place_mode){ placer_save(&g_props); }
                    else if(forest_edit){ forest_save(&g_forest); }
                    else if(spawn_edit_mode){ spawns_save(&g_spawns); }
                    break;
                case SDLK_w:
                    /* In river editor — cycle global water style presets */
                    if(river_edit){
                        int next=(g_wstyle.preset_idx+1)%WATER_PRESET_COUNT;
                        wstyle_apply_preset(next);
                    }
                    /* In water zone editor — cycle selected zone's preset */
                    else if(wzone_edit&&wzone_sel>=0&&wzone_sel<g_wzone_count){
                        g_wzones[wzone_sel].preset_idx=
                            (g_wzones[wzone_sel].preset_idx+1)%WATER_PRESET_COUNT;
                        printf("[WZoneEd] Zone %d -> %s\n",wzone_sel,
                               WATER_PRESETS[g_wzones[wzone_sel].preset_idx].name);
                    } break;
                case SDLK_LEFTBRACKET:
                    if(wzone_edit&&!wf_edit_mode&&wzone_sel>=0){
                        g_wzones[wzone_sel].level-=1.0f;
                        terrain_rebuild_all(&terrain);
                        wzone_rebuild_vaos(&terrain);wzone_save();
                        printf("[WZoneEd] Zone %d level: %.1f\n",
                               wzone_sel,(double)g_wzones[wzone_sel].level);
                    } else if(prop_place_mode){
                        g_placer.place_ry=fmodf(g_placer.place_ry+15.0f,360.0f);
                    } break;
                case SDLK_RIGHTBRACKET:
                    if(wzone_edit&&!wf_edit_mode&&wzone_sel>=0){
                        g_wzones[wzone_sel].level+=1.0f;
                        terrain_rebuild_all(&terrain);
                        wzone_rebuild_vaos(&terrain);wzone_save();
                        printf("[WZoneEd] Zone %d level: %.1f\n",
                               wzone_sel,(double)g_wzones[wzone_sel].level);
                    } else if(prop_place_mode){
                        g_placer.place_ry=fmodf(g_placer.place_ry-15.0f+360.0f,360.0f);
                    } break;
                case SDLK_DELETE:
                    if(prop_place_mode){
                        /* Remove the closest prop to the camera */
                        float best=1e9f; int bi=-1;
                        for(int i=0;i<g_props.count;i++){
                            Prop *p=&g_props.list[i];if(!p->active)continue;
                            float dx=p->pos.x-cam.pos.x,dz=p->pos.z-cam.pos.z;
                            float d=dx*dx+dz*dz;if(d<best){best=d;bi=i;}
                        }
                        if(bi>=0){
                            Prop *p=&g_props.list[bi];
                            if(p->stream_state==PROP_LOADED)prop_unload_gpu(p);
                            p->active=false;
                            printf("[PropPlacer] Deleted prop %d (%s)\n",bi,p->path);
                        }
                    } break;
                case SDLK_F5:game_save(0,&player,&qs,&as,enemies,ne,total_time,&inv,&lvlsys);save_notify(&save_note,"Saved — Slot 1");audio_play(SND_MENU_CLICK,80);break;
                case SDLK_F6:game_save(1,&player,&qs,&as,enemies,ne,total_time,&inv,&lvlsys);save_notify(&save_note,"Saved — Slot 2");audio_play(SND_MENU_CLICK,80);break;
                case SDLK_F7:game_save(2,&player,&qs,&as,enemies,ne,total_time,&inv,&lvlsys);save_notify(&save_note,"Saved — Slot 3");audio_play(SND_MENU_CLICK,80);break;
                case SDLK_F9:
                    sculpt_edit=!sculpt_edit;
                    spawn_edit_mode=false;npc_edit_mode=false;prop_edit=false;
                    show_debug=false;river_edit=false;prop_place_mode=false;forest_edit=false;
                    path_edit=false;wzone_edit=false;
                    SDL_SetRelativeMouseMode(sculpt_edit?SDL_FALSE:SDL_TRUE);
                    printf("[SculptEd] %s  mode=%d  radius=%.0f\n",
                           sculpt_edit?"ON":"OFF",sculpt_mode,(double)sculpt_radius);
                    break;
                case SDLK_F10:
                    forest_edit=!forest_edit;
                    spawn_edit_mode=false;npc_edit_mode=false;prop_edit=false;
                    show_debug=false;river_edit=false;prop_place_mode=false;sculpt_edit=false;
                    path_edit=false;wzone_edit=false;
                    SDL_SetRelativeMouseMode(forest_edit?SDL_FALSE:SDL_TRUE);
                    printf("[ForestEd] %s — %d trees\n",forest_edit?"ON":"OFF",g_forest.count);
                    break;
                case SDLK_i: inv.open=!inv.open;audio_play(SND_MENU_CLICK,70);break;
                case SDLK_h: /* Quick-use HP potion */
                    if(player.state!=PS_DEAD)
                        quick_use_potion(&inv,&player,&as,&qs,base_max_hp,&save_note);
                    break;
                case SDLK_y:
                    if(npcs.open&&npcs.talk_state==NPC_STATE_QUEST_OFFER){npc_quest_accept(&npcs,&qs,&save_note);audio_play(SND_MENU_CLICK,80);}
                    else if(prop_edit&&g_props.count){g_props.list[prop_sel].rot_z+=prop_step_rot;}
                    break;
                case SDLK_n:
                    if(npcs.open&&npcs.talk_state==NPC_STATE_QUEST_OFFER){npc_quest_decline(&npcs);audio_play(SND_MENU_CLICK,70);}
                    else if(prop_edit&&g_props.count){g_props.list[prop_sel].rot_z-=prop_step_rot;}
                    break;
                case SDLK_F3:
                    show_debug=!show_debug;
                    prop_edit=show_debug;
                    river_edit=false;path_edit=false;wzone_edit=false;
                    if(prop_edit&&g_props.count>0){printf("\n[PropEdit] Selected prop 0: %s\n",g_props.list[0].path);}
                    break;
                case SDLK_F1:
                    path_edit=!path_edit;
                    river_edit=false;prop_edit=false;show_debug=false;prop_place_mode=false;spawn_edit_mode=false;wzone_edit=false;
                    SDL_SetRelativeMouseMode(path_edit?SDL_FALSE:SDL_TRUE);
                    printf("[PathEd] %s — %d points, width=%.1f\n",path_edit?"ON":"OFF",g_cpath.count,(double)g_cpath.width);
                    break;
                case SDLK_F2:
                    wzone_edit=!wzone_edit;
                    river_edit=false;prop_edit=false;show_debug=false;prop_place_mode=false;spawn_edit_mode=false;path_edit=false;
                    SDL_SetRelativeMouseMode(wzone_edit?SDL_FALSE:SDL_TRUE);
                    printf("[WZoneEd] %s — %d zones\n",wzone_edit?"ON":"OFF",g_wzone_count);
                    break;
                case SDLK_F4:
                    river_edit=!river_edit;
                    prop_edit=false;show_debug=false;prop_place_mode=false;path_edit=false;wzone_edit=false;
                    if(river_edit) printf("[RiverEd] ON — %d points loaded\n",g_criver.count);
                    else           printf("[RiverEd] OFF\n");
                    SDL_SetRelativeMouseMode(river_edit?SDL_FALSE:SDL_TRUE);
                    break;
                case SDLK_F8:
                    prop_place_mode=!prop_place_mode;
                    prop_edit=false;show_debug=false;river_edit=false;spawn_edit_mode=false;path_edit=false;wzone_edit=false;forest_edit=false;
                    SDL_SetRelativeMouseMode(prop_place_mode?SDL_FALSE:SDL_TRUE);
                    printf("[PropPlacer] %s\n",prop_place_mode?"ON":"OFF");
                    break;
                case SDLK_F11:
                    npc_edit_mode=!npc_edit_mode;
                    spawn_edit_mode=false;prop_edit=false;show_debug=false;river_edit=false;prop_place_mode=false;path_edit=false;wzone_edit=false;forest_edit=false;
                    SDL_SetRelativeMouseMode(npc_edit_mode?SDL_FALSE:SDL_TRUE);
                    printf("[NpcEd] %s — %d points\n",npc_edit_mode?"ON":"OFF",g_npc_spawns.count);
                    break;
                case SDLK_F12:
                    spawn_edit_mode=!spawn_edit_mode;
                    npc_edit_mode=false;prop_edit=false;show_debug=false;river_edit=false;prop_place_mode=false;path_edit=false;wzone_edit=false;forest_edit=false;
                    SDL_SetRelativeMouseMode(spawn_edit_mode?SDL_FALSE:SDL_TRUE);
                    printf("[SpawnEd] %s — %d points\n",spawn_edit_mode?"ON":"OFF",g_spawns.count);
                    break;
                case SDLK_TAB:
                    if(wzone_edit){
                        wf_edit_mode=!wf_edit_mode; wf_sel=-1;
                        printf("[F2] Sub-mode: %s\n",wf_edit_mode?"Waterfalls":"Water Zones");
                        break;
                    }
                    if(prop_edit&&g_props.count>0){
                        prop_sel=(prop_sel+1)%g_props.count;
                        printf("[PropEdit] prop %d: %s rx=%.1f ry=%.1f rz=%.1f scale=%.3f\n",
                               prop_sel,g_props.list[prop_sel].path,
                               (double)g_props.list[prop_sel].rot_x,
                               (double)g_props.list[prop_sel].rot_y,
                               (double)g_props.list[prop_sel].rot_z,
                               (double)g_props.list[prop_sel].scale);
                    } else {
                        save_ui.open=!save_ui.open;audio_play(SND_MENU_CLICK,70);
                    } break;
                case SDLK_r:
                    if(wzone_edit&&wf_edit_mode&&wf_sel>=0)
                        {g_wfalls.wf[wf_sel].yaw+=0.1745f;wf_save();}
                    else if(prop_edit&&g_props.count)
                        g_props.list[prop_sel].rot_y+=prop_step_rot;
                    break;
                case SDLK_v: if(prop_edit&&g_props.count){g_props.list[prop_sel].rot_y-=prop_step_rot;}break;
                case SDLK_t: if(prop_edit&&g_props.count){g_props.list[prop_sel].rot_x+=prop_step_rot;}break;
                case SDLK_b: if(prop_edit&&g_props.count){g_props.list[prop_sel].rot_x-=prop_step_rot;}break;
                /* Y/N handled above (quest accept/decline + prop rot_z) */
                case SDLK_EQUALS:case SDLK_PLUS:
                    if(map_screen.open){map_screen.zoom=clampf(map_screen.zoom*1.25f,0.5f,8.0f);}
                    else if(prop_edit&&g_props.count){g_props.list[prop_sel].scale+=prop_step_scale;
                        if(g_props.list[prop_sel].scale<0.01f)g_props.list[prop_sel].scale=0.01f;}break;
                case SDLK_MINUS:
                    if(map_screen.open){map_screen.zoom=clampf(map_screen.zoom*0.80f,0.5f,8.0f);}
                    else if(prop_edit&&g_props.count){g_props.list[prop_sel].scale-=prop_step_scale;
                        if(g_props.list[prop_sel].scale<0.01f)g_props.list[prop_sel].scale=0.01f;}break;
                case SDLK_PAGEUP:   if(prop_edit&&g_props.count){g_props.list[prop_sel].pos.y+=0.5f;}break;
                case SDLK_PAGEDOWN: if(prop_edit&&g_props.count){g_props.list[prop_sel].pos.y-=0.5f;}break;
                case SDLK_UP:    if(prop_edit&&g_props.count){Vec3 fw=cam_forward(&cam);g_props.list[prop_sel].pos.x+=fw.x*0.5f;g_props.list[prop_sel].pos.z+=fw.z*0.5f;}break;
                case SDLK_DOWN:  if(prop_edit&&g_props.count){Vec3 fw=cam_forward(&cam);g_props.list[prop_sel].pos.x-=fw.x*0.5f;g_props.list[prop_sel].pos.z-=fw.z*0.5f;}break;
                case SDLK_LEFT:  if(prop_edit&&g_props.count){Vec3 ri=cam_right(&cam);g_props.list[prop_sel].pos.x-=ri.x*0.5f;g_props.list[prop_sel].pos.z-=ri.z*0.5f;}break;
                case SDLK_RIGHT: if(prop_edit&&g_props.count){Vec3 ri=cam_right(&cam);g_props.list[prop_sel].pos.x+=ri.x*0.5f;g_props.list[prop_sel].pos.z+=ri.z*0.5f;}break;
                case SDLK_p:
                    if(npc_edit_mode){
                        /* handled in editor block below */
                    } else { wx.enabled=!wx.enabled; }
                    break;
                case SDLK_d:
                    if(npc_edit_mode){
                        /* Handled in editor block — toggle dialogue editor on hovered NPC */
                    }
                    break;
                case SDLK_e:
                    if(player.state!=PS_DEAD){
                        float cf_dist=vec3_dist(player.phys.pos,cf_pos);
                        if(cf_dist<3.5f){
                            campfire_menu_open=!campfire_menu_open;
                            campfire_lmb_prev=true; /* suppress click-through */
                        } else if(fishing.active){
                            fishing_stop(&fishing);
                        } else if(fishing.near_water&&player.phys.on_ground&&player.state!=PS_SWIM){
                            fishing_start(&fishing,fishing.spot,fishing.water_level);
                        } else {
                            npc_interact(&npcs,player.phys.pos,&qs);
                        }
                    }
                    break;
                /* Respawn */
                case SDLK_RETURN:case SDLK_KP_ENTER:
                    if(player.state==PS_DEAD){
                        player.health=player.max_health;
                        player.state=PS_IDLE;player.anim_idx=player_amap.idle;
                        player.anim_time=0;player.hit_flash=0;
                        player.phys.pos=(Vec3){cf_wx,terrain_h(cf_wx,cf_wz)+0.1f,cf_wz};
                        player.phys.vel=(Vec3){0,0,0};
                        death_fade=0.0f;
                        player.ooc_timer=6.0f;
                    }break;
                case SDLK_1: /* Fireball — shoot forward / spawn type 1 */
                    if(sculpt_edit){sculpt_mode=0;printf("[SculptEd] RAISE\n");break;}
                    if(npc_edit_mode){npc_sel_type=0;break;}
                    if(spawn_edit_mode){spawn_sel_type=ETYPE_KNIGHT;break;}
                    if(ability_use(&as,0)&&player.state!=PS_DEAD){
                        Vec3 fw=cam_forward(&cam);
                        Vec3 spawn={player.phys.pos.x+fw.x*0.8f,
                                    player.phys.pos.y+1.2f,
                                    player.phys.pos.z+fw.z*0.8f};
                        proj_spawn(&projs,spawn,vec3_norm((Vec3){fw.x,0.08f,fw.z}),22.0f,35.0f,PROJ_FIREBALL,PROJ_OWNER_PLAYER);
                        ps_fire_burst(&ps,spawn,1.2f);          /* fire launch flash */
                        ps_magic_burst(&ps,spawn,1.0f,0.4f,0.0f); /* orange ring */
                        audio_play(SND_FIREBALL,90);
                    }break;
                case SDLK_2: /* Heal — restore 40 HP / spawn type 2 */
                    if(sculpt_edit){sculpt_mode=1;printf("[SculptEd] LOWER\n");break;}
                    if(npc_edit_mode){npc_sel_type=1;break;}
                    if(spawn_edit_mode){spawn_sel_type=ETYPE_ARCHER;break;}
                    if(ability_use(&as,1)&&player.state!=PS_DEAD){
                        float amt=40.0f;
                        player.health=clampf(player.health+amt,0,player.max_health);
                        /* Green magic burst */
                        ps_magic_burst(&ps,player.phys.pos,0.2f,1.0f,0.3f);
                        ps_magic_burst(&ps,(Vec3){player.phys.pos.x,player.phys.pos.y+1.0f,player.phys.pos.z},0.3f,0.5f,1.0f);
                        save_notify(&save_note,"+40 HP (Heal)");
                        audio_play(SND_HEAL,90);
                    }break;
                case SDLK_3: /* Dash — lunge forward / spawn type 3 */
                    if(sculpt_edit){sculpt_mode=2;printf("[SculptEd] FLATTEN\n");break;}
                    if(npc_edit_mode){npc_sel_type=2;break;}
                    if(spawn_edit_mode){spawn_sel_type=ETYPE_BRUTE;break;}
                    if(ability_use(&as,2)&&player.state!=PS_DEAD&&player.state!=PS_ROLL){
                        Vec3 fw=cam_forward(&cam);
                        player.phys.vel.x=fw.x*18.0f;
                        player.phys.vel.z=fw.z*18.0f;
                        player.phys.vel.y=2.5f;
                        player.dash_iframes=0.4f;  /* 0.4s invincibility */
                        player.state=PS_ROLL;       /* reuse roll anim */
                        player.anim_idx=player_amap.roll;
                        player.roll_timer=0;player.anim_time=0;
                        ps_dust_puff(&ps,player.phys.pos,10); /* dash dust */
                        audio_play(SND_DASH,90);
                    }break;
                case SDLK_4: /* Shield — 4s damage reduction / spawn type 4 */
                    if(sculpt_edit){sculpt_mode=3;printf("[SculptEd] SMOOTH\n");break;}
                    if(npc_edit_mode){npc_sel_type=3;break;}
                    if(spawn_edit_mode){spawn_sel_type=ETYPE_SHADE;break;}
                    if(ability_use(&as,3)&&player.state!=PS_DEAD){
                        player.shield_timer=4.0f;
                        /* Blue magic ring */
                        ps_magic_burst(&ps,player.phys.pos,0.2f,0.4f,1.0f);
                        ps_magic_burst(&ps,(Vec3){player.phys.pos.x,player.phys.pos.y+0.8f,player.phys.pos.z},0.3f,0.1f,0.8f);
                        save_notify(&save_note,"Shield activated! (4s)");
                        audio_play(SND_SHIELD,95);
                    }break;
                case SDLK_5: /* Meteor — AoE blast / spawn type 5 */
                    if(sculpt_edit){sculpt_mode=4;printf("[SculptEd] CLIFF\n");break;}
                    if(spawn_edit_mode){spawn_sel_type=ETYPE_BOSS;break;}
                    if(ability_use(&as,4)&&player.state!=PS_DEAD){
                        /* Instant AoE: damage + knockback all enemies within 30u */
                        int hits=0;
                        for(int mi=0;mi<ne;mi++){
                            Enemy *me=&enemies[mi];
                            if(!me->active||me->state==ES_DEAD)continue;
                            float md=vec3_dist(me->phys.pos,player.phys.pos);
                            if(md>30.0f)continue;
                            float falloff=1.0f-md/30.0f;
                            me->health-=80.0f*falloff;
                            /* Knockback away from player */
                            Vec3 kdir=vec3_norm(vec3_sub(me->phys.pos,player.phys.pos));
                            me->phys.vel.x+=kdir.x*12.0f*falloff;
                            me->phys.vel.y+=4.0f*falloff;
                            me->phys.vel.z+=kdir.z*12.0f*falloff;
                            me->state=ES_STAGGER;me->stagger_timer=0.6f;
                            ps_burst(&ps,me->phys.pos,1.0f,0.8f,0.2f);
                            hits++;
                        }
                        /* Big impact burst at player feet */
                        for(int bi=0;bi<5;bi++)ps_burst(&ps,
                            (Vec3){player.phys.pos.x+((float)rand()/RAND_MAX-0.5f)*8,
                                   player.phys.pos.y,
                                   player.phys.pos.z+((float)rand()/RAND_MAX-0.5f)*8},
                            1.2f,1.0f,0.1f);
                        g_screen_shake=0.5f;
                        char mbuf[48];snprintf(mbuf,sizeof(mbuf),"Meteor! Hit %d enemies",hits);
                        save_notify(&save_note,mbuf);
                        audio_play(SND_METEOR,118);
                    }break;
                case SDLK_6:
                    if(sculpt_edit){sculpt_mode=5;printf("[SculptEd] GRASS PAINT\n");break;}
                    break;
                case SDLK_7:
                    if(sculpt_edit){sculpt_mode=6;printf("[SculptEd] WORN PATH\n");break;}
                    break;
                case SDLK_f:
                    if(wzone_edit&&wf_edit_mode&&wf_sel>=0)
                        {g_wfalls.wf[wf_sel].yaw-=0.1745f;wf_save();break;}
                    if(player.state!=PS_DEAD&&player.state!=PS_ATTACK&&player.state!=PS_ATTACK2){player.state=PS_ATTACK;player.anim_idx=player_amap.attack;player.attack_timer=0;player.attack_hit=false;player.anim_time=0;audio_play(SND_ATTACK,100);}break;
                case SDLK_g:
                    if(prop_place_mode){
                        g_placer.snap_y=!g_placer.snap_y;
                        printf("[PropPlacer] SnapY %s\n",g_placer.snap_y?"ON":"OFF");
                    } else if(player.state!=PS_DEAD&&player.state!=PS_ATTACK&&player.state!=PS_ATTACK2){
                        player.state=PS_ATTACK2;player.anim_idx=player_amap.attack2;
                        player.attack_timer=0;player.attack_hit=false;player.anim_time=0;
                        audio_play(SND_ATTACK,100);
                    } break;
                case SDLK_q:if(player.state!=PS_DEAD&&player.state!=PS_ROLL&&player.state!=PS_SWIM&&player.phys.on_ground&&as.stam>=20.0f){
                    player.state=PS_ROLL;player.anim_idx=player_amap.roll;player.roll_timer=0;player.anim_time=0;player.dash_iframes=0.5f;
                    stam_consume(&as,20.0f);
                }break;
                }break;
            case SDL_MOUSEBUTTONDOWN:
                if(ev.button.button==SDL_BUTTON_LEFT&&inv.open){
                    int imx=ev.button.x,imy=ev.button.y;
                    inv_click(&inv,imx,imy,win_w,win_h,&player,&as,&qs,base_max_hp,&save_note);
                    break;
                }
                if(ev.button.button==SDL_BUTTON_RIGHT){cam_drag=true;cam_lx=ev.button.x;cam_ly=ev.button.y;}
                /* Dialogue choice click */
                if(ev.button.button==SDL_BUTTON_LEFT&&npcs.open&&npcs.active_npc>=0&&npcs.talk_state==NPC_STATE_CHAT){
                    NpcDef *_nc=&npcs.npcs[npcs.active_npc];
                    if(_nc->dlg_root_id>=0&&_nc->dlg_cur_node>=0&&_nc->dlg_cur_node<g_dlg_count){
                        DialogueNode *_dn2=&g_dlg_nodes[_nc->dlg_cur_node];
                        if(_dn2->num_choices>1){
                            float _pw2=700.0f,_px2=(float)win_w*0.5f-350.0f;
                            float _ch2=(float)_dn2->num_choices*28.0f;
                            float _ph2=110.0f+_ch2;
                            float _py2=(float)win_h-_ph2-70.0f;
                            float _cy2=_py2+_ph2-_ch2-4.0f;
                            for(int _ci2=0;_ci2<_dn2->num_choices;_ci2++,_cy2+=28.0f){
                                if(ev.button.x>=_px2&&ev.button.x<=_px2+_pw2&&
                                   ev.button.y>=_cy2&&ev.button.y<=_cy2+24){
                                    npc_dlg_pick(&npcs,_ci2,&qs);break;
                                }
                            }
                        }
                    }
                }
                if(ev.button.button==SDL_BUTTON_LEFT&&!npc_dialogue_capture&&!inv.open&&!prop_place_mode&&!forest_edit&&player.state!=PS_DEAD&&player.state!=PS_ATTACK&&player.state!=PS_ATTACK2){player.state=PS_ATTACK;player.anim_idx=player_amap.attack;player.attack_timer=0;player.attack_hit=false;player.anim_time=0;audio_play(SND_ATTACK,100);}
                if(ev.button.button==SDL_BUTTON_MIDDLE&&!npc_dialogue_capture&&!forest_edit&&player.state!=PS_DEAD&&player.state!=PS_ATTACK&&player.state!=PS_ATTACK2){player.state=PS_ATTACK2;player.anim_idx=player_amap.attack2;player.attack_timer=0;player.attack_hit=false;player.anim_time=0;audio_play(SND_ATTACK,100);}
                break;
            case SDL_MOUSEBUTTONUP:if(ev.button.button==SDL_BUTTON_RIGHT)cam_drag=false;break;
            case SDL_MOUSEMOTION:
                if(map_screen.open&&(SDL_GetMouseState(NULL,NULL)&SDL_BUTTON(1))){
                    /* drag scrolls the map view */
                    float scale=WORLD_HALF*2.0f/map_screen.zoom/(float)win_w;
                    map_screen.scroll_x-=ev.motion.xrel*scale;
                    map_screen.scroll_y-=ev.motion.yrel*scale;
                } else if(cam_drag){
                    cam.yaw-=(ev.motion.x-cam_lx)*0.005f;
                    cam.pitch-=(ev.motion.y-cam_ly)*0.005f;
                    cam.pitch=clampf(cam.pitch,0.05f,1.3f);
                    cam_lx=ev.motion.x;cam_ly=ev.motion.y;
                } break;
            case SDL_MOUSEWHEEL:
                if(wzone_edit&&wf_edit_mode&&wf_sel>=0){
                    Waterfall *_wfw=&g_wfalls.wf[wf_sel];
                    const Uint8 *_kwf=SDL_GetKeyboardState(NULL);
                    if(_kwf[SDL_SCANCODE_LSHIFT]||_kwf[SDL_SCANCODE_RSHIFT]){
                        _wfw->width=clampf(_wfw->width+ev.wheel.y*0.5f,1.0f,80.0f);
                        printf("[WFalls] Width: %.1f\n",(double)_wfw->width);
                    } else {
                        _wfw->height=clampf(_wfw->height+ev.wheel.y*1.0f,2.0f,200.0f);
                        printf("[WFalls] Height: %.1f\n",(double)_wfw->height);
                    }
                    /* re-wire mist emitter to new base position */
                    if(_wfw->mist_em>=0&&_wfw->mist_em<ps.ecount)
                        ps.emitters[_wfw->mist_em].pos=(Vec3){
                            _wfw->pos.x,_wfw->pos.y-_wfw->height+0.6f,_wfw->pos.z};
                    wf_save();
                } else if(sculpt_edit){
                    const Uint8 *ksw=SDL_GetKeyboardState(NULL);
                    if(ksw[SDL_SCANCODE_LSHIFT]||ksw[SDL_SCANCODE_RSHIFT]){
                        sculpt_strength=clampf(sculpt_strength+ev.wheel.y*0.005f,0.002f,0.5f);
                        printf("[SculptEd] Strength: %.3f\n",(double)sculpt_strength);
                    } else {
                        sculpt_radius=clampf(sculpt_radius+ev.wheel.y*2.0f,3.0f,300.0f);
                        printf("[SculptEd] Radius: %.0f\n",(double)sculpt_radius);
                    }
                } else if(forest_edit){
                    const Uint8 *ksw=SDL_GetKeyboardState(NULL);
                    if(ksw[SDL_SCANCODE_LSHIFT]||ksw[SDL_SCANCODE_RSHIFT]){
                        forest_brush_density+=ev.wheel.y;
                        if(forest_brush_density<1) forest_brush_density=1;
                        if(forest_brush_density>12) forest_brush_density=12;
                        printf("[ForestEd] Density: %d\n",forest_brush_density);
                    } else {
                        forest_brush_radius=clampf(forest_brush_radius+ev.wheel.y*2.0f,6.0f,160.0f);
                        printf("[ForestEd] Radius: %.0f\n",(double)forest_brush_radius);
                    }
                } else if(wzone_edit&&wf_edit_mode&&wf_sel<0){
                    const Uint8 *_kwfp=SDL_GetKeyboardState(NULL);
                    if(_kwfp[SDL_SCANCODE_LSHIFT]||_kwfp[SDL_SCANCODE_RSHIFT])
                        wf_place_width=clampf(wf_place_width+ev.wheel.y*0.5f,1.0f,80.0f);
                    else
                        wf_place_height=clampf(wf_place_height+ev.wheel.y*1.0f,2.0f,200.0f);
                } else if(npc_edit_mode){
                    npc_ed_yaw=fmodf(npc_ed_yaw+(float)ev.wheel.y*0.2f,(float)(2*M_PI));
                } else if(wzone_edit&&wzone_sel>=0){
                    const Uint8 *ksw=SDL_GetKeyboardState(NULL);
                    if(ksw[SDL_SCANCODE_LCTRL]||ksw[SDL_SCANCODE_RCTRL]){
                        g_wzones[wzone_sel].radius=clampf(
                            g_wzones[wzone_sel].radius+ev.wheel.y*5.0f,10.0f,1000.0f);
                        terrain_rebuild_all(&terrain);
                        wzone_rebuild_vaos(&terrain);
                        printf("[WZoneEd] Zone %d radius: %.0f\n",
                               wzone_sel,(double)g_wzones[wzone_sel].radius);
                    }
                } else if(path_edit){
                    const Uint8 *ksp=SDL_GetKeyboardState(NULL);
                    if(ksp[SDL_SCANCODE_LCTRL]||ksp[SDL_SCANCODE_RCTRL]){
                        g_cpath.width=clampf(g_cpath.width+ev.wheel.y*0.5f,0.5f,40.0f);
                        if(g_cpath.count>=2) path_rebuild_mesh();
                        printf("[PathEd] Width: %.1f\n",(double)g_cpath.width);
                    }
                } else if(river_edit){
                    const Uint8 *ks2=SDL_GetKeyboardState(NULL);
                    if(ks2[SDL_SCANCODE_LCTRL]||ks2[SDL_SCANCODE_RCTRL]){
                        g_criver.width=clampf(g_criver.width+ev.wheel.y*5.0f,10.0f,400.0f);
                        /* Rebuild all chunks near existing river points */
                        for(int ri=0;ri<g_criver.count;ri++)
                            terrain_rebuild_near(&terrain,
                                g_criver.pts[ri*2],g_criver.pts[ri*2+1],
                                g_criver.width+TERRAIN_CELL*4);
                        printf("[RiverEd] Width: %.0f\n",(double)g_criver.width);
                    }
                } else if(prop_place_mode){
                    /* Ctrl+wheel = scale, plain wheel = palette scroll */
                    const Uint8 *ks=SDL_GetKeyboardState(NULL);
                    if(ks[SDL_SCANCODE_LCTRL]||ks[SDL_SCANCODE_RCTRL]){
                        g_placer.place_scale=clampf(g_placer.place_scale+ev.wheel.y*0.05f,0.05f,10.0f);
                    } else {
                        g_placer.palette_sel=clampf((float)(g_placer.palette_sel-ev.wheel.y),
                                                    0,(float)(g_placer.palette_count-1));
                        /* keep scroll window centred on selection */
                        int vis=12;
                        if(g_placer.palette_sel<g_placer.palette_scroll)
                            g_placer.palette_scroll=g_placer.palette_sel;
                        if(g_placer.palette_sel>=g_placer.palette_scroll+vis)
                            g_placer.palette_scroll=g_placer.palette_sel-vis+1;
                    }
                } else if(map_screen.open){
                    map_screen.zoom*=(ev.wheel.y>0)?1.25f:0.80f;
                    map_screen.zoom=clampf(map_screen.zoom,0.5f,8.0f);
                } else {
                    cam.dist-=ev.wheel.y*0.8f;cam.dist=clampf(cam.dist,3.0f,60.0f);
                } break;
            case SDL_WINDOWEVENT:if(ev.window.event==SDL_WINDOWEVENT_RESIZED){
                win_w=ev.window.data1;win_h=ev.window.data2;
                glViewport(0,0,win_w,win_h);
                hud_resize(&hud,win_w,win_h);
                pfx_resize(&g_pfx,win_w,win_h);
            }break;
            case SDL_CONTROLLERDEVICEADDED:
                if(!g_pad)gamepad_try_open();
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                if(g_pad&&SDL_GameControllerGetJoystick(g_pad)==
                   SDL_JoystickFromInstanceID(ev.cdevice.which)){
                    SDL_GameControllerClose(g_pad);g_pad=NULL;
                    printf("[Pad] Disconnected\n");
                    gamepad_try_open();
                }
                break;
            }
        }
        npc_dialogue_capture=(npc_ed_dlg_mode&&npc_ed_dlg_npc>=0);
        if(npc_dialogue_capture) time_scale=1.0f;
        /* ── Simulation (paused when pause menu open) ────────── */
        bool is_paused=(g_pause.state!=PM_NONE);
        /* ── Gamepad: handle discrete button events ───────── */
        if(g_pad&&!is_paused&&!npc_dialogue_capture&&player.state!=PS_DEAD){
            bool pa=pad_btn(SDL_CONTROLLER_BUTTON_A);
            bool px2=pad_btn(SDL_CONTROLLER_BUTTON_X);
            bool py2=pad_btn(SDL_CONTROLLER_BUTTON_Y);
            bool pb=pad_btn(SDL_CONTROLLER_BUTTON_B);
            bool pstart=pad_btn(SDL_CONTROLLER_BUTTON_START);
            bool pback=pad_btn(SDL_CONTROLLER_BUTTON_BACK);
            /* A = jump */
            if(pa&&!g_pad_a_prev&&player.phys.on_ground&&
               player.state!=PS_ATTACK&&player.state!=PS_ATTACK2&&
               player.state!=PS_ROLL&&player.state!=PS_SWIM){
                player.phys.vel.y=PLAYER_JUMP;player.state=PS_JUMP;
                player.anim_idx=player_amap.jump;player.anim_time=0;
                audio_play(SND_JUMP,90);
            }
            /* X = attack */
            if(px2&&!g_pad_x_prev&&player.state!=PS_ATTACK&&player.state!=PS_ATTACK2){
                player.state=PS_ATTACK;player.anim_idx=player_amap.attack;
                player.attack_timer=0;player.attack_hit=false;player.anim_time=0;
                audio_play(SND_ATTACK,100);
            }
            /* Y = attack2 */
            if(py2&&!g_pad_y_prev&&player.state!=PS_ATTACK&&player.state!=PS_ATTACK2){
                player.state=PS_ATTACK2;player.anim_idx=player_amap.attack2;
                player.attack_timer=0;player.attack_hit=false;player.anim_time=0;
                audio_play(SND_ATTACK,100);
            }
            /* B = roll */
            if(pb&&!g_pad_b_prev&&player.state!=PS_ROLL&&
               player.state!=PS_ATTACK&&player.state!=PS_ATTACK2&&
               player.phys.on_ground&&as.stam>=20.0f){
                player.state=PS_ROLL;player.anim_idx=player_amap.roll;
                player.roll_timer=0;player.anim_time=0;player.dash_iframes=0.5f;
            }
            /* Start = pause */
            if(pstart&&!g_pad_start_prev){
                if(g_pause.state==PM_NONE) g_pause.state=PM_ROOT;
                else g_pause.state=PM_NONE;
            }
            /* Back = inventory */
            if(pback&&!g_pad_back_prev) inv.open=!inv.open;
            g_pad_a_prev=pa; g_pad_x_prev=px2; g_pad_y_prev=py2;
            g_pad_b_prev=pb; g_pad_start_prev=pstart; g_pad_back_prev=pback;
        }
        if(!is_paused){

        /* Player movement */
        bool fishing_space_pressed=false;
        if(player.state!=PS_DEAD){
            const uint8_t *keys=SDL_GetKeyboardState(NULL);
            Vec3 fwd=cam_forward(&cam),rgt=cam_right(&cam),move={0,0,0};
            fishing_space_pressed=!npc_dialogue_capture&&keys[SDL_SCANCODE_SPACE]&&!fishing_space_prev;
            fishing_space_prev=!npc_dialogue_capture&&keys[SDL_SCANCODE_SPACE];
            if(!fishing.active&&!npc_dialogue_capture){
                if(keys[SDL_SCANCODE_W])move=vec3_add(move,fwd);
                if(keys[SDL_SCANCODE_S])move=vec3_sub(move,fwd);
                if(keys[SDL_SCANCODE_D])move=vec3_add(move,rgt);
                if(keys[SDL_SCANCODE_A])move=vec3_sub(move,rgt);
            }
            /* Gamepad left stick */
            if(g_pad&&!fishing.active&&!npc_dialogue_capture){
                float lx=pad_axis(SDL_CONTROLLER_AXIS_LEFTX,0.2f);
                float ly=pad_axis(SDL_CONTROLLER_AXIS_LEFTY,0.2f);
                if(fabsf(lx)>0.01f)move=vec3_add(move,vec3_scale(rgt,lx));
                if(fabsf(ly)>0.01f)move=vec3_sub(move,vec3_scale(fwd,ly));
                /* Right stick: rotate camera */
                float rx=pad_axis(SDL_CONTROLLER_AXIS_RIGHTX,0.2f);
                float ry=pad_axis(SDL_CONTROLLER_AXIS_RIGHTY,0.2f);
                cam.yaw  -= rx*dt*2.5f;
                cam.pitch = clampf(cam.pitch - ry*dt*2.0f, 0.05f, 1.3f);
                /* LB = block */
                if(pad_btn(SDL_CONTROLLER_BUTTON_LEFTSHOULDER))
                    player.blocking=true;
                /* RT = ability 0 */
                if(pad_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,0.5f)>0.0f)
                    ability_use(&as,0); /* RT = ability slot 0 */
            }
            bool moving=vec3_len(move)>0.01f;
            if(moving){float spd=PLAYER_SPEED*(1.0f+inv.spd_bonus);move=vec3_scale(vec3_norm(move),spd);player.phys.yaw=atan2f(move.x,-move.z)+(float)M_PI;}
            player.phys.vel.x=moving?move.x:player.phys.vel.x*0.8f;
            player.phys.vel.z=moving?move.z:player.phys.vel.z*0.8f;
            if(fishing.active){
                player.phys.vel.x*=0.25f;
                player.phys.vel.z*=0.25f;
            }
            if(!npc_dialogue_capture&&!fishing.active&&keys[SDL_SCANCODE_SPACE]&&player.phys.on_ground&&player.state!=PS_ATTACK&&player.state!=PS_ATTACK2&&player.state!=PS_ROLL&&player.state!=PS_SWIM){player.phys.vel.y=PLAYER_JUMP;player.state=PS_JUMP;player.anim_idx=player_amap.jump;player.anim_time=0;audio_play(SND_JUMP,90);}
            bool sprinting=!npc_dialogue_capture&&!fishing.active&&keys[SDL_SCANCODE_LSHIFT]&&moving&&as.stam>0.0f&&player.state!=PS_SWIM;
            if(sprinting){
                float spd=PLAYER_SPEED*(1.0f+inv.spd_bonus);
                move=vec3_scale(vec3_norm(move),spd*1.7f);
                player.phys.vel.x=move.x;player.phys.vel.z=move.z;
                as.stam=clampf(as.stam-15.0f*dt,0,as.max_stam);
                stam_drain(&as);
            }
            player.blocking=!npc_dialogue_capture&&!fishing.active&&keys[SDL_SCANCODE_R]&&player.state!=PS_DEAD&&player.state!=PS_ATTACK&&player.state!=PS_ATTACK2&&as.stam>0.0f;
            if(player.blocking){as.stam=clampf(as.stam-8.0f*dt,0,as.max_stam);stam_drain(&as);}
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
                            enemies[i].health-=(ATTACK_DAMAGE+inv.dmg_bonus+lvlsys.dmg_bonus);enemies[i].state=ES_STAGGER;enemies[i].stagger_timer=0.3f;
                            ps_blood(&ps,enemies[i].phys.pos,EDEF[enemies[i].etype].height*0.55f,10);
                            ps_burst(&ps,enemies[i].phys.pos,1.0f,0.3f,0.1f);
                            if(enemies[i].health<=0){
                                enemies[i].health=0;enemies[i].state=ES_DEAD;
                                enemies[i].anim_idx=enemy_amaps[enemies[i].etype].death;
                                enemies[i].anim_time=0;
                                quest_notify_kill(&qs,(int)enemies[i].etype);
                                ps_burst(&ps,enemies[i].phys.pos,0.8f,0.1f,0.1f);
                                ps_blood(&ps,enemies[i].phys.pos,EDEF[enemies[i].etype].height*0.6f,22);
                                audio_play(SND_ENEMY_DIE,85);
                                inv_drop(&world_items,enemies[i].etype,enemies[i].phys.pos,&qs,&save_note);
                                /* Award kill XP */
                                int kxp=EDEF[enemies[i].etype].xp;
                                qs.player_xp+=kxp;
                                if(enemies[i].etype==ETYPE_BOSS){
                                    qs.player_gold+=200;
                                    save_notify(&save_note,"THE LICH KING IS DEFEATED! +500 XP  +200 Gold");
                                    g_screen_shake=1.0f;
                                    /* Huge death burst */
                                    for(int b=0;b<6;b++)ps_burst(&ps,enemies[i].phys.pos,1.5f,0.1f,1.0f);
                                } else {
                                    char xpbuf[32];snprintf(xpbuf,sizeof(xpbuf),"+%d XP (%s)",kxp,EDEF[enemies[i].etype].name);
                                    save_notify(&save_note,xpbuf);
                                    enemies[i].respawn_timer=60.0f; /* respawn after 60s */
                                }
                            }
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
            else if(player.state!=PS_SWIM){
                if(moving){player.state=sprinting?PS_RUN:PS_WALK;player.anim_idx=sprinting?player_amap.run:player_amap.walk;
                    audio_footstep(sprinting,dt);}
                else{player.state=PS_IDLE;player.anim_idx=player_amap.idle;g_audio.step_timer=0;}
            }
            /* KEY FIX: reset anim_time whenever the active animation changes */
            if(player.anim_idx!=player.prev_anim_idx){player.anim_time=0.0f;}
            player.prev_anim_idx=player.anim_idx;
        }
        /* Clamp death animation – play once then freeze on last frame */
        if(player.state==PS_DEAD&&player.anim_time>(double)death_dur)
            player.anim_time=(double)death_dur;

        player.hit_flash=clampf(player.hit_flash-dt*3.0f,0,1);
        player.shield_timer=clampf(player.shield_timer-dt,0,999);
        player.dash_iframes=clampf(player.dash_iframes-dt,0,999);

        /* ── Out-of-combat health regen ────────────────────────────
           ooc_timer resets to 6s whenever the player takes damage.
           After 6s with no hits, HP slowly ticks up (5 HP/s base,
           boosted by regen_bonus from leveling).                   */
        if(player.state==PS_HIT||player.hit_flash>0.5f)
            player.ooc_timer=6.0f;  /* reset on being hit */
        else
            player.ooc_timer=clampf(player.ooc_timer-dt,0.0f,6.0f);
        if(player.ooc_timer<=0.0f&&player.state!=PS_DEAD&&player.health<player.max_health){
            float regen_rate=5.0f+lvlsys.regen_bonus;
            player.health=clampf(player.health+regen_rate*dt,0,player.max_health);
        }
        float pg=terrain_h(player.phys.pos.x,player.phys.pos.z);
        fishing.near_water=false;

        /* ── Water detection ────────────────────────────────────────
           Player is swimming when their feet are below water level
           AND the terrain beneath them is also underwater (real water,
           not just standing on shallow shore).                        */
        float local_wlevel = WATER_LEVEL;
        bool has_surface_water = water_surface_info(player.phys.pos.x,player.phys.pos.z,&local_wlevel,NULL);
        bool in_water = has_surface_water
                      && (player.phys.pos.y <= local_wlevel + 0.35f)
                      && (pg <= local_wlevel + 0.2f)
                      && player.state != PS_DEAD;

        if(in_water){
            /* ── Swim physics — replace normal gravity/ground logic ── */
            float swim_surface = local_wlevel - 0.4f; /* player chest at surface */
            float depth = swim_surface - player.phys.pos.y;
            /* Buoyancy: push toward swim_surface */
            float buoy = clampf(depth * 6.0f, -8.0f, 12.0f);
            player.phys.vel.y += buoy * dt;
            /* Space = actively swim upward */
            if(SDL_GetKeyboardState(NULL)[SDL_SCANCODE_SPACE])
                player.phys.vel.y = clampf(player.phys.vel.y + 8.0f*dt, -4.0f, 3.5f);
            /* Water drag */
            player.phys.vel.x *= (1.0f - 3.5f*dt);
            player.phys.vel.z *= (1.0f - 3.5f*dt);
            player.phys.vel.y *= (1.0f - 2.5f*dt);
            /* Swim speed (slower than walking) */
            const uint8_t *ks=SDL_GetKeyboardState(NULL);
            Vec3 sfwd=cam_forward(&cam),srgt=cam_right(&cam),smove={0,0,0};
            if(ks[SDL_SCANCODE_W])smove=vec3_add(smove,sfwd);
            if(ks[SDL_SCANCODE_S])smove=vec3_sub(smove,sfwd);
            if(ks[SDL_SCANCODE_D])smove=vec3_add(smove,srgt);
            if(ks[SDL_SCANCODE_A])smove=vec3_sub(smove,srgt);
            smove.y=0; /* horizontal only */
            if(vec3_len(smove)>0.01f){
                smove=vec3_scale(vec3_norm(smove),PLAYER_SPEED*0.55f);
                player.phys.vel.x=smove.x;
                player.phys.vel.z=smove.z;
                player.phys.yaw=atan2f(smove.x,-smove.z)+(float)M_PI;
            }
            /* Clamp depth — can't go below terrain even underwater */
            if(player.phys.pos.y < pg + 0.05f){
                player.phys.pos.y = pg + 0.05f;
                if(player.phys.vel.y < 0) player.phys.vel.y = 0;
            }
            /* Clamp above surface */
            if(player.phys.pos.y > local_wlevel + 0.05f){
                player.phys.pos.y = local_wlevel + 0.05f;
                if(player.phys.vel.y > 0) player.phys.vel.y = 0;
            }
            player.phys.pos = vec3_add(player.phys.pos,
                                       vec3_scale(player.phys.vel, dt));
            player.phys.on_ground = false;
            /* State + animation */
            bool swim_moving = vec3_len((Vec3){player.phys.vel.x,0,player.phys.vel.z})>0.3f;
            player.state = PS_SWIM;
            player.anim_idx = swim_moving ? player_amap.walk : player_amap.idle;
            player.anim_speed = swim_moving ? 1.4f : 0.6f;
            /* Slight stamina drain while swimming */
            as.stam = clampf(as.stam - 4.0f*dt, 0, as.max_stam);
            stam_drain(&as);
            fishing_stop(&fishing);
        } else {
            /* ── Normal land physics ─────────────────────────────── */
            /* If we just left water, give a small exit boost */
            if(player.state == PS_SWIM){
                player.state = PS_IDLE;
                player.phys.vel.y = clampf(player.phys.vel.y, 0, 4.0f);
            }
            phys_update(&player.phys,dt,pg);
            {
                float fish_wl=WATER_LEVEL;
                Vec3  fish_spot={0,0,0};
                bool can_fish = player.phys.on_ground
                             && player.state!=PS_ATTACK
                             && player.state!=PS_ATTACK2
                             && player.state!=PS_ROLL
                             && fishing_find_water(player.phys.pos,&fish_wl,&fish_spot)
                             && fabsf(pg-fish_wl)<2.0f;
                fishing.near_water=can_fish;
                fishing.water_level=fish_wl;
                fishing.spot=fish_spot;
                if(fishing.active&&!can_fish){
                    fishing_stop(&fishing);
                }else if(fishing.active){
                    fishing_update(&fishing,dt);
                    if(fishing_space_pressed){
                        float catch_half=fishing.target_w*0.5f;
                        float d=fabsf(fishing.cursor-fishing.target);
                        if(d<=catch_half){
                            fishing_gain(&fishing,&inv,&save_note);
                            fishing.misses=0;
                            fishing_retarget(&fishing);
                        }else{
                            fishing.misses++;
                            if(fishing.misses>=3){
                                save_notify(&save_note,"The fish got away");
                                fishing_stop(&fishing);
                            } else {
                                char mbuf[48];
                                snprintf(mbuf,sizeof(mbuf),"Missed the bite (%d/3)",fishing.misses);
                                save_notify(&save_note,mbuf);
                            }
                            fishing.banner=0.5f;
                        }
                    }
                    player.phys.vel.x*=0.15f;
                    player.phys.vel.z*=0.15f;
                    player.anim_idx=(fishing.cast_anim>0.0f)?player_amap.attack2:player_amap.idle;
                    player.state=PS_IDLE;
                }
            }
        }
        /* Clamp to world edges so terrain_h never gets out-of-range coords */
        player.phys.pos.x=clampf(player.phys.pos.x,-WORLD_HALF+5.0f,WORLD_HALF-5.0f);
        player.phys.pos.z=clampf(player.phys.pos.z,-WORLD_HALF+5.0f,WORLD_HALF-5.0f);
        props_stream_update(&g_props,cam.pos);  /* load/unload props by distance */
        props_resolve(&g_props,&player.phys,player.phys.radius);
        player.anim_time += dt * player.anim_speed;

        /* ── Flow-field rebuild (player moved to new cell?) ── */
        {
            int pcx=nav_world_to_cx(player.phys.pos.x);
            int pcz=nav_world_to_cz(player.phys.pos.z);
            if(!g_nav_ready||pcx!=g_nav.pcx||pcz!=g_nav.pcz)
                nav_rebuild(player.phys.pos,&g_props);
        }
        /* Enemy AI */
        bool any_enemy_nearby=false;
        for(int i=0;i<ne;i++){Enemy *e=&enemies[i];if(!e->active)continue;
            {float _pdx=e->phys.pos.x-player.phys.pos.x,_pdz=e->phys.pos.z-player.phys.pos.z;
             if(_pdx*_pdx+_pdz*_pdz > 200.0f*200.0f) continue;}
            const EnemyDef *d=&EDEF[e->etype];
            if(e->state==ES_DEAD){
                e->anim_time+=dt;
                if(!e->boss_no_respawn&&e->respawn_timer>0.0f){
                    e->respawn_timer-=dt;
                    if(e->respawn_timer<=0.0f){
                        /* Only respawn if player is far enough away */
                        float rd=vec3_dist(e->spawn_pos,player.phys.pos);
                        if(rd>30.0f){
                            enemy_spawn(e,e->etype,e->spawn_pos,&enemy_amaps[e->etype]);
                            e->respawn_timer=0.0f;
                        } else {
                            e->respawn_timer=5.0f; /* check again in 5s */
                        }
                    }
                }
                continue;
            }
            /* Single sqrt for cull + all AI distance checks */
            float dist=vec3_dist(e->phys.pos,player.phys.pos);
            if(dist>120.0f){e->anim_time+=dt*e->anim_speed;continue;}
            if(e->state==ES_STAGGER){e->stagger_timer-=dt;if(e->stagger_timer<=0)e->state=ES_CHASE;e->anim_time+=dt;continue;}
            e->attack_timer=clampf(e->attack_timer-dt,0,999);

            /* Random growl */
            e->growl_timer-=dt;
            if(e->growl_timer<=0){
                e->growl_timer=4.0f+(float)rand()/RAND_MAX*6.0f;
                audio_play_3d(SND_ENEMY_GROWL,e->phys.pos,player.phys.pos,80,20.0f);
            }

            if(dist<d->chase_r)any_enemy_nearby=true;

            if(dist>d->chase_r){
                e->state=ES_IDLE;e->anim_idx=enemy_amaps[e->etype].idle;
                e->phys.vel.x=e->phys.vel.z=0;
            } else if(d->ranged){
                /* Archers: strafe at attack_r, shoot projectiles */
                Vec3 dir=vec3_norm(vec3_sub(player.phys.pos,e->phys.pos));
                if(dist>d->attack_r+1.0f){
                    e->state=ES_CHASE;e->anim_idx=enemy_amaps[e->etype].walk;
                    Vec3 navdir=nav_sample(e->phys.pos,player.phys.pos);
                    e->phys.vel.x=navdir.x*d->speed;e->phys.vel.z=navdir.z*d->speed;
                } else if(dist<d->attack_r-1.0f){
                    /* Back off to preferred range */
                    e->state=ES_CHASE;e->anim_idx=enemy_amaps[e->etype].walk;
                    e->phys.vel.x=-dir.x*d->speed*0.7f;e->phys.vel.z=-dir.z*d->speed*0.7f;
                } else {
                    e->phys.vel.x=e->phys.vel.z=0;
                    e->state=ES_ATTACK;e->anim_idx=enemy_amaps[e->etype].attack;
                    if(e->attack_timer<=0&&player.state!=PS_DEAD){
                        /* Shoot arrow/projectile at player */
                        Vec3 aim=vec3_norm(vec3_sub(player.phys.pos,(Vec3){e->phys.pos.x,e->phys.pos.y+1.0f,e->phys.pos.z}));
                        Vec3 spawn={e->phys.pos.x+aim.x*0.8f,e->phys.pos.y+1.3f,e->phys.pos.z+aim.z*0.8f};
                        proj_spawn(&projs,spawn,aim,18.0f,d->damage,PROJ_ARROW,PROJ_OWNER_ENEMY);
                        e->attack_timer=e->attack_cooldown;
                        audio_play_3d(SND_ENEMY_ATTACK,e->phys.pos,player.phys.pos,80,20.0f);
                    }
                }
                e->phys.yaw=atan2f(dir.x,-dir.z)+(float)M_PI;
            } else if(dist>d->attack_r){
                e->state=ES_CHASE;e->anim_idx=enemy_amaps[e->etype].walk;
                /* Flow-field pathfinding — avoids walls and props */
                Vec3 dir=nav_sample(e->phys.pos,player.phys.pos);
                e->phys.vel.x=dir.x*d->speed;e->phys.vel.z=dir.z*d->speed;
                e->phys.yaw=atan2f(dir.x,-dir.z)+(float)M_PI;
            } else {
                e->phys.vel.x=e->phys.vel.z=0;e->state=ES_ATTACK;e->anim_idx=enemy_amaps[e->etype].attack;
                if(e->attack_timer<=0&&player.state!=PS_DEAD&&player.state!=PS_HIT){
                    /* Skip damage if dash i-frames active */
                    if(player.dash_iframes>0){}
                    else{
                        float shield_reduce=(player.shield_timer>0)?0.75f:0.0f; /* shield ability blocks 75% */
                        float block_reduce =(player.blocking)      ?0.60f:0.0f; /* R-block blocks 60% */
                        float total_reduce=clampf(inv.dmg_reduce+shield_reduce+block_reduce,0,0.90f);
                        player.health-=d->damage*(1.0f-total_reduce);
                        player.hit_flash=1.0f;
                        e->attack_timer=e->attack_cooldown;
                        ps_burst(&ps,player.phys.pos,1,0,0);
                        audio_play(SND_HIT,90);
                        audio_play_3d(SND_ENEMY_ATTACK,e->phys.pos,player.phys.pos,85,10.0f);
                        if(player.health<=0){
                            player.health=0;player.state=PS_DEAD;
                            death_fade=0.0f; /* start death screen fade */
                            player.anim_idx=player_amap.death;player.anim_time=0;
                            ps_blood(&ps,player.phys.pos,1.0f,24); /* big splatter on death */
                            audio_play(SND_DEATH,110);
                        } else if(player.blocking){
                            /* Block absorbs stagger — stay in block stance */
                            player.hit_flash=0.3f; /* subtle flash */
                            audio_play(SND_HIT,60);
                        } else {
                            player.state=PS_HIT;player.anim_idx=player_amap.hit;
                            player.anim_time=0;player.hit_timer=hit_dur;
                            /* Knock player away from attacker */
                            Vec3 kb=vec3_norm((Vec3){player.phys.pos.x-e->phys.pos.x,0,player.phys.pos.z-e->phys.pos.z});
                            player.phys.vel.x=kb.x*7.0f;
                            player.phys.vel.z=kb.z*7.0f;
                            player.phys.vel.y=2.5f;
                        }
                    }
                }  /* end if(e->attack_timer) */
            }
            /* ── BOSS AI ─────────────────────────────────────── */
            if(e->etype==ETYPE_BOSS&&e->state!=ES_DEAD){
                float hp_frac=e->health/e->max_health;
                /* Phase transitions */
                int new_phase=(hp_frac>0.60f)?1:(hp_frac>0.30f)?2:3;
                if(new_phase>e->boss_phase){
                    e->boss_phase=new_phase;
                    e->phase_flash=true;e->phase_flash_t=1.0f;
                    g_screen_shake=0.6f;
                    /* Big particle burst on phase change */
                    ps_burst(&ps,e->phys.pos,1.2f,0.2f,1.0f);
                    ps_burst(&ps,e->phys.pos,0.8f,0.0f,0.8f);
                    save_notify(&save_note,(new_phase==2)?"The Lich King ENRAGES!":"THE LICH KING BERSERKS!");
                    audio_play(SND_ENEMY_GROWL,110);
                }
                if(e->phase_flash_t>0){e->phase_flash_t-=dt*1.5f;if(e->phase_flash_t<=0)e->phase_flash=false;}

                /* Phase 2+: shoot homing projectiles in addition to melee */
                if(e->boss_phase>=2&&e->attack_timer<=0&&dist<30.0f&&player.state!=PS_DEAD){
                    Vec3 aim=vec3_norm(vec3_sub(player.phys.pos,(Vec3){e->phys.pos.x,e->phys.pos.y+1.5f,e->phys.pos.z}));
                    Vec3 sp={e->phys.pos.x,e->phys.pos.y+1.5f,e->phys.pos.z};
                    proj_spawn(&projs,sp,aim,10.0f,d->damage*0.6f,PROJ_FIREBALL,PROJ_OWNER_ENEMY);
                    if(e->boss_phase==3){
                        /* Phase 3: 3-shot spread */
                        float a=0.35f;
                        Vec3 left ={(aim.x*cosf(a)-aim.z*sinf(a)),aim.y,(aim.x*sinf(a)+aim.z*cosf(a))};
                        Vec3 right={(aim.x*cosf(-a)-aim.z*sinf(-a)),aim.y,(aim.x*sinf(-a)+aim.z*cosf(-a))};
                        proj_spawn(&projs,sp,vec3_norm(left), 12.0f,d->damage*0.5f,PROJ_FIREBALL,PROJ_OWNER_ENEMY);
                        proj_spawn(&projs,sp,vec3_norm(right),12.0f,d->damage*0.5f,PROJ_FIREBALL,PROJ_OWNER_ENEMY);
                    }
                    e->attack_timer=e->attack_cooldown*(e->boss_phase==3?0.55f:0.80f);
                    audio_play_3d(SND_FIREBALL,e->phys.pos,player.phys.pos,95,30.0f);
                }
                /* Phase 2: summon Knights periodically */
                if(e->boss_phase>=2&&ne<MAX_ENEMIES){
                    e->summon_timer-=dt;
                    if(e->summon_timer<=0){
                        e->summon_timer=(e->boss_phase==3)?8.0f:14.0f;
                        /* Find first inactive slot */
                        for(int si=0;si<MAX_ENEMIES;si++){
                            if(!enemies[si].active){
                                float ang=(float)rand()/(float)RAND_MAX*(float)(2*M_PI);
                                Vec3 spos={e->phys.pos.x+cosf(ang)*5.0f,0,e->phys.pos.z+sinf(ang)*5.0f};
                                enemy_spawn(&enemies[si],ETYPE_KNIGHT,spos,&enemy_amaps[ETYPE_KNIGHT]);
                                ps_burst(&ps,spos,0.5f,0.3f,0.4f);
                                save_notify(&save_note,"The Lich King summons a Knight!");
                                break;
                            }
                        }
                    }
                }
                /* Phase 3: speed boost */
                if(e->boss_phase==3){
                    float spd3=d->speed*2.0f;
                    if(dist>d->attack_r){
                        Vec3 dir=vec3_norm(vec3_sub(player.phys.pos,e->phys.pos));
                        e->phys.vel.x=dir.x*spd3;e->phys.vel.z=dir.z*spd3;
                    }
                }
                /* Screen shake when boss hits player */
                if(e->state==ES_ATTACK&&e->attack_timer==e->attack_cooldown)
                    g_screen_shake=clampf(g_screen_shake+0.25f,0,0.8f);
            }
            if(e->state!=ES_IDLE||dist<30.0f){
                float eg=terrain_h(e->phys.pos.x,e->phys.pos.z);
                phys_update(&e->phys,dt,eg);
            }
            e->anim_time+=dt*e->anim_speed;
        }
        /* ── Entity-entity separation (prevent stacking) ─────
           O(n²) but with cheap squared-distance early-out.      */
        for(int si=0;si<ne;si++){
            Enemy *ea=&enemies[si];
            if(!ea->active||ea->state==ES_DEAD)continue;
            for(int sj=si+1;sj<ne;sj++){
                Enemy *eb=&enemies[sj];
                if(!eb->active||eb->state==ES_DEAD)continue;
                float sdx=ea->phys.pos.x-eb->phys.pos.x;
                float sdz=ea->phys.pos.z-eb->phys.pos.z;
                float sd2=sdx*sdx+sdz*sdz;
                float min_sep=ea->phys.radius+eb->phys.radius+0.3f;
                if(sd2<min_sep*min_sep&&sd2>0.0001f){
                    float sd=sqrtf(sd2);
                    float push=(min_sep-sd)/min_sep*5.0f;
                    float nx=sdx/sd, nz=sdz/sd;
                    ea->phys.vel.x+=nx*push;
                    ea->phys.vel.z+=nz*push;
                    eb->phys.vel.x-=nx*push;
                    eb->phys.vel.z-=nz*push;
                }
            }
        }
        audio_update(dt,player.phys.pos,any_enemy_nearby,ls.day_frac,wx.type==WX_RAIN);

        ability_update(&as,dt);quest_update(&qs,player.phys.pos);
        level_update(&lvlsys,&qs,&player,&as,base_max_hp,&inv,&save_note,dt);
        lighting_update(&ls,total_time);ps_update(&ps,dt);
    wf_update(&g_wfalls,&ps,dt);
        wind_update(&g_wind,dt,total_time);
        /* ── Atmospheric state update ──────────────────────────── */
        {   bool is_raining=(wx.type==WX_RAIN);
            bool is_snowing=(wx.type==WX_SNOW);
            bool any_precip=is_raining||is_snowing;
            /* Snow build-up should reflect current weather, not elapsed session time. */
            float snow_target=is_snowing?(0.35f+g_wind.total*0.45f):0.0f;
            g_wind.snow_accum+=(snow_target-g_wind.snow_accum)*dt*(is_snowing?0.18f:0.10f);
            g_wind.snow_accum=clampf(g_wind.snow_accum,0.0f,1.0f);
            /* wetness: ramp up in rain, dry slowly */
            float wet_target=is_raining?1.0f:0.0f;
            g_wetness+=(wet_target-g_wetness)*dt*(is_raining?0.12f:0.04f);
            g_wetness=clampf(g_wetness,0.0f,1.0f);
            /* fog: base 0.0005; rises in rain, spikes in blizzard */
            float fog_target=0.0005f;
            if(is_raining) fog_target=0.0022f;
            if(is_snowing) fog_target=0.0012f+g_wind.snow_accum*0.008f;
            g_fog_density+=(fog_target-g_fog_density)*dt*0.8f;
            /* overcast: builds in precip, clears slowly */
            float ovc_target=any_precip?(is_raining?0.85f:0.6f+g_wind.snow_accum*0.35f):0.0f;
            g_overcast+=(ovc_target-g_overcast)*dt*(any_precip?0.15f:0.05f);
            g_overcast=clampf(g_overcast,0.0f,1.0f);
        }
        puddle_update(&g_puddles,player.phys.pos,g_wetness);
        footprint_update(&g_footprints,player.phys.pos,dt,g_wind.snow_accum);
        grass_update(&g_grass,player.phys.pos);
        wint_update(&g_wint,player.phys.pos,dt);
        proj_update(&projs,dt,&ps,enemies,ne,&player);
        {const uint8_t *ks=SDL_GetKeyboardState(NULL);
         bool press_e=!npc_dialogue_capture&&ks[SDL_SCANCODE_E]&&!inv.open;
         wi_update(&world_items,dt,player.phys.pos,&inv,&qs,&save_note,press_e);}
        wx_update(&wx,cam.pos,player.phys.pos.y,dt,&g_wind,total_time);
        cam_update(&cam,player.phys.pos,dt);
        perf_update(&perf,dt);
        /* Advance NPC idle animations */
        if(has_npc_model)for(int i=0;i<npcs.count;i++)npcs.npcs[i].anim_time+=dt;
        npc_patrol_update(&npcs,dt);   /* move patrolling NPCs along waypoints */
        bird_system_update(&g_birds,dt); /* advance bird flight phases + wing flap */
        animal_system_update(&g_animals,dt,player.phys.pos);
        /* Apply animal attack damage */
        if(g_animals.pending_damage>0&&player.state!=PS_DEAD&&player.dash_iframes<=0){
            float shield_reduce=(player.shield_timer>0)?0.75f:0.0f;
            float block_reduce =(player.blocking)      ?0.60f:0.0f;
            float total_reduce=clampf(shield_reduce+block_reduce,0,0.90f);
            player.health=clampf(player.health-g_animals.pending_damage*(1.0f-total_reduce),0,player.max_health);
            player.hit_flash=0.7f;
            ps_blood(&ps,player.phys.pos,0.7f,8);
            audio_play(SND_HIT,80);
            if(player.health<=0){
                player.health=0;player.state=PS_DEAD;
                death_fade=0.0f;
                player.anim_idx=player_amap.death;player.anim_time=0;
                ps_blood(&ps,player.phys.pos,1.0f,24);
                audio_play(SND_DEATH,110);
            }
        }

        /* Interior containment check */
        interior_check(player.phys.pos);
        g_interior_enter_timer=clampf(g_interior_enter_timer-dt,0,4.0f);

        } /* end !is_paused */

        /* Render */
        float asp=(float)win_w/(float)win_h;
        Mat4 proj=mat4_persp(60.0f,asp,0.5f,7000.0f);
        /* Update SSAO camera params each frame */
        if(g_pfx.ready){
            g_pfx.cam_near=0.5f; g_pfx.cam_far=7000.0f;
            g_pfx.cam_aspect=asp;
        }
        g_screen_shake=clampf(g_screen_shake-dt*3.0f,0,1.0f);
        Vec3 shake_off={0,0,0};
        if(g_screen_shake>0.001f){
            shake_off.x=((float)rand()/RAND_MAX-0.5f)*g_screen_shake*0.8f;
            shake_off.y=((float)rand()/RAND_MAX-0.5f)*g_screen_shake*0.5f;
        }
        Vec3 shaken_cam={cam.pos.x+shake_off.x,cam.pos.y+shake_off.y,cam.pos.z+shake_off.z};
        Mat4 view=mat4_lookat(shaken_cam,cam.target,(Vec3){0,1,0});

        /* ── Shadow pass ──────────────────────────────────────── */
        /* ── Lazy-load enemy CharModels ──────────────────────────
           For each active enemy within render range, ensure its
           CharModel is loaded. First encounter with a new type will
           cause a one-frame parse+GL-upload stall, but that's far
           better than loading all 5 FBX files at startup.          */
        #define ENSURE_ENEMY_MODEL(etype) do{ \
            if(!enemy_model_loaded[(etype)]){ \
                const char *_fbx=EDEF[(etype)].fbx; \
                if(_fbx){ \
                    charmodel_init(&enemy_models[(etype)],_fbx); \
                    if(!scene_has_data(&enemy_models[(etype)].base)) \
                        charmodel_share_base(&enemy_models[(etype)],&char_model); \
                } else { \
                    charmodel_share_base(&enemy_models[(etype)],&char_model); \
                } \
                enemy_model_loaded[(etype)]=true; \
                printf("[Enemy] Lazy-loaded model: %s\n",EDEF[(etype)].name); \
            } \
        }while(0)

        for(int i=0;i<ne;i++){
            Enemy *e=&enemies[i]; if(!e->active)continue;
            float _dd=vec3_dist(e->phys.pos,cam.pos);
            if(_dd<130.0f) ENSURE_ENEMY_MODEL(e->etype);
        }

        shadow_begin(&sm,&ls,player.phys.pos);
        shadow_terrain(&sm,&ls,&terrain);
        props_shadow(&g_props,&sm,&ls,cam.pos);
        {Mat4 wm=entity_mat(player.phys.pos,player.phys.yaw,char_model.base.ground_offset,char_model.base.model_scale,char_model.base.cx,char_model.base.cy,char_model.base.cz);shadow_charmodel(&sm,&ls,&char_model,wm,player.anim_idx,player.anim_time);}
        for(int i=0;i<ne;i++){Enemy *e=&enemies[i];if(!e->active)continue;
            float _sd=vec3_dist(e->phys.pos,player.phys.pos);if(_sd>80.0f)continue;
            if(!enemy_model_loaded[e->etype])continue;
            CharModel *em=&enemy_models[e->etype];
            enemy_refresh_eval(e,em,_sd,frame_count);
            if(!e->anim_eval&&_sd>2.0f)continue;
            const EnemyDef *d=&EDEF[e->etype];
            float sc2=em->base.model_scale*d->scale;
            Mat4 wm=entity_mat(e->phys.pos,e->phys.yaw,em->base.ground_offset,sc2,em->base.cx,em->base.cy,em->base.cz);
            if(_sd<40.0f){
                int slot=e->anim_idx; if(slot<0||slot>=ANIM_SLOTS)slot=0;
                AnimEntry *ae=&em->anims[slot];
                Scene *sc3=ae->scene?ae->scene:&em->base;
                shadow_scene_cached(&sm,&ls,sc3,wm,e->anim_eval);
            }
        }
        /* NPC shadows */
        if(has_npc_model)for(int i=0;i<npcs.count;i++){NpcDef *n=&npcs.npcs[i];
            float _nd=vec3_dist(n->pos,player.phys.pos);if(_nd>60.0f)continue;
            int _mi=n->model_idx; if(_mi<0||_mi>=NPC_MODEL_MAX||!g_npc_model_ok[_mi])_mi=0;
            CharModel *_cm=&g_npc_models[_mi];
            Mat4 wm=entity_mat(n->pos,n->yaw,_cm->base.ground_offset,_cm->base.model_scale,_cm->base.cx,_cm->base.cy,_cm->base.cz);
            shadow_charmodel(&sm,&ls,_cm,wm,n->anim_idx,n->anim_time);}
        shadow_end(&sm,win_w,win_h);

        /* Wide cascade — terrain + large props only, every 3 frames */
        if(frame_count%3==0){
            shadow_begin_wide(&sm_wide,&ls,player.phys.pos);
            glViewport(0,0,1024,1024);
            shadow_terrain(&sm_wide,&ls,&terrain);
            props_shadow(&g_props,&sm_wide,&ls,cam.pos);
            glBindFramebuffer(GL_FRAMEBUFFER,0);
            glViewport(0,0,win_w,win_h);
            glDisable(GL_POLYGON_OFFSET_FILL);
            glCullFace(GL_BACK);
            ls.shadow_tex_wide=sm_wide.tex;
        }

        /* ── Main render pass ─────────────────────────────────── */
        /* Render 3D scene to HDR FBO; tonemap+bloom applied after */
        if(g_pfx.ready) glBindFramebuffer(GL_FRAMEBUFFER,g_pfx.hdr_fbo);
        /* Clear colour tracks the sky horizon; blends toward overcast grey */
        {float day_t=clampf(ls.sun_elev*3.0f+0.3f,0,1);
         float dawn_t=clampf(1.0f-fabsf(ls.sun_elev)*5.0f,0,1);
         float r=0.03f+(0.52f*day_t+0.92f*dawn_t*0.4f)*(day_t+dawn_t*0.4f);
         float g=0.03f+(0.72f*day_t+0.41f*dawn_t*0.4f)*(day_t+dawn_t*0.4f);
         float b=0.10f+(0.85f*day_t)*(day_t);
         r=r+(0.52f-r)*g_overcast; g=g+(0.54f-g)*g_overcast; b=b+(0.56f-b)*g_overcast;
         glClearColor(r,g,b,1.0f);}
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        /* Fog colour = slightly desaturated sky horizon */
        Vec3 fog_col;
        {float day_t=clampf(ls.sun_elev*3.0f+0.3f,0,1);
         fog_col=(Vec3){0.52f+0.18f*day_t*(1.0f-g_overcast),
                        0.54f+0.16f*day_t*(1.0f-g_overcast),
                        0.56f+0.20f*day_t*(1.0f-g_overcast)};}

        /* ── Water reflection pre-pass (every 2nd frame, half-res) ──
           Renders skybox + terrain land + props with camera reflected
           about y=WATER_LEVEL into g_reflect.fbo.  The result is
           sampled in the water fragment shader for Fresnel reflection. */
        bool do_reflect = g_reflect.ready && g_interior_idx<0 && (frame_count%2==0);
        if(do_reflect){
            float wl=WATER_LEVEL;
            Vec3 rcam={(cam.pos.x+shake_off.x),
                       2.0f*wl-(cam.pos.y+shake_off.y),
                       (cam.pos.z+shake_off.z)};
            Vec3 rtgt={cam.target.x, 2.0f*wl-cam.target.y, cam.target.z};
            /* up vector flipped because camera is inverted */
            Mat4 rview=mat4_lookat(rcam,rtgt,(Vec3){0,-1,0});
            glBindFramebuffer(GL_FRAMEBUFFER,g_reflect.fbo);
            glViewport(0,0,g_reflect.w,g_reflect.h);
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            /* Flip culling — reflected geometry has reversed winding */
            glCullFace(GL_FRONT);
            skybox_draw(&skybox,proj,rview,&ls,g_overcast);
            terrain_draw(&terrain,&ls,proj,rview,rcam,total_time,
                         g_wind.snow_accum,g_wetness,g_fog_density,fog_col);
            props_draw(&g_props,&ls,proj,rview,rcam);
            forest_draw(&g_forest,&g_props,&ls,proj,rview,rcam,g_wind.snow_accum);
            glCullFace(GL_BACK);
            /* CRITICAL: restore HDR FBO so the main scene writes depth
               into hdr_depth_tex (which SSAO reads).  Binding to 0 here
               was causing SSAO to read stale depth → full-screen flicker. */
            if(g_pfx.ready) glBindFramebuffer(GL_FRAMEBUFFER,g_pfx.hdr_fbo);
            else             glBindFramebuffer(GL_FRAMEBUFFER,0);
            glViewport(0,0,win_w,win_h);
        }
        /* Upload reflect tex to water shader */
        {
            GLuint wp=terrain.water_prog;
            glUseProgram(wp);
            if(do_reflect){
                glActiveTexture(GL_TEXTURE8);
                glBindTexture(GL_TEXTURE_2D,g_reflect.tex);
                glUniform1i(glGetUniformLocation(wp,"uReflectTex"),8);
                glUniform1i(glGetUniformLocation(wp,"uHasReflect"),1);
            } else {
                glUniform1i(glGetUniformLocation(wp,"uHasReflect"),0);
            }
            glUniform2f(glGetUniformLocation(wp,"uScreenSize"),(float)win_w,(float)win_h);
            glUseProgram(0);
        }

        skybox_draw(&skybox,proj,view,&ls,g_overcast);
        /* Skip outdoor rendering when inside */
        if(g_interior_idx<0){
            terrain_draw(&terrain,&ls,proj,view,cam.pos,total_time,
                         g_wind.snow_accum,g_wetness,g_fog_density,fog_col);
            path_draw(&ls,proj,view,cam.pos,g_fog_density,fog_col);
            props_draw(&g_props,&ls,proj,view,cam.pos);
            puddle_draw(&g_puddles,&ls,proj,view,cam.pos,total_time,g_wetness);
            footprint_draw(&g_footprints,proj,view);
            grass_draw(&g_grass,&ls,proj,view,player.phys.pos,cam.pos,total_time,
                       g_wind.wx,g_wind.wz,g_wind.total);
            wint_draw(&g_wint,&ls,proj,view,player.phys.pos,cam.pos,total_time);
            forest_draw(&g_forest,&g_props,&ls,proj,view,cam.pos,g_wind.snow_accum);
            bird_system_draw(&g_birds,&skin,proj,view,cam.pos,&ls);
            animal_system_draw(&g_animals,&skin,proj,view,cam.pos,&ls);
        } else {
            interior_draw_room(&g_room_rend,&hud,win_w,win_h,proj,view);
        }

        /* Player */
        {Mat4 wm=entity_mat(player.phys.pos,player.phys.yaw,char_model.base.ground_offset,char_model.base.model_scale,char_model.base.cx,char_model.base.cy,char_model.base.cz);draw_charmodel(&skin,&char_model,wm,player.anim_idx,player.anim_time,player.hit_flash,proj,view,cam.pos,&ls);}

        /* Enemies — Fix 1+2+3: cached pose, distance LOD tiers, throttled eval
           LOD0 < 40u  : full skin + shadow
           LOD1 40-80u : full skin, no shadow
           LOD2 80-110u: billboard (coloured oval, no skeleton eval)
           >110u        : culled                                               */
        Mat4 pv2=mat4_mul(proj,view);float epl[6][4];frustum_extract(epl,pv2);
        {
        for(int i=0;i<ne;i++){Enemy *e=&enemies[i];if(!e->active)continue;
            if(!enemy_model_loaded[e->etype])continue;
            float _ed=vec3_dist(e->phys.pos,cam.pos);
            if(_ed>110.0f)continue;
            if(!frustum_sphere(epl,e->phys.pos.x,e->phys.pos.y,e->phys.pos.z,3.0f))continue;
            const EnemyDef *d=&EDEF[e->etype];
            CharModel *em=&enemy_models[e->etype];

            if(_ed>80.0f){
                /* LOD2: simple coloured billboard — zero skeleton cost */
                /* Reuse animal billboard prog if available, else skip */
                /* Just draw a debug box placeholder at low res */
                /* (billboard prog would require setup — skip for now, just cull) */
                continue;
            }

            /* LOD0/1: full skinned draw — eval already refreshed by shadow pass
               or refresh now if shadow pass was skipped (distance > 40)       */
            enemy_refresh_eval(e,em,_ed,frame_count);

            float sc2=em->base.model_scale*d->scale;
            Mat4 wm=entity_mat(e->phys.pos,e->phys.yaw,em->base.ground_offset,sc2,em->base.cx,em->base.cy,em->base.cz);
            GLH_vec3(skin.prog,"uBaseColor",d->cr,d->cg,d->cb);
            int slot=e->anim_idx; if(slot<0||slot>=ANIM_SLOTS)slot=0;
            AnimEntry *ae=&em->anims[slot];
            Scene *sc3=ae->scene?ae->scene:&em->base;
            draw_scene_cached(&skin,sc3,wm,e->anim_eval,
                              e->state==ES_STAGGER?0.4f:0.0f,cam.pos,proj,view,&ls);
        }}

        /* NPCs */
        if(has_npc_model){
            GLH_vec3(skin.prog,"uBaseColor",0.85f,0.80f,0.72f); /* friendly tint */
            for(int i=0;i<npcs.count;i++){NpcDef *n=&npcs.npcs[i];
                float _nd2=vec3_dist(n->pos,cam.pos);
                if(_nd2>90.0f) continue; /* distance cull */
                if(!frustum_sphere(epl,n->pos.x,n->pos.y,n->pos.z,2.0f)) continue;
                int _mi=n->model_idx; if(_mi<0||_mi>=NPC_MODEL_MAX||!g_npc_model_ok[_mi])_mi=0;
                CharModel *_cm=&g_npc_models[_mi];
                Mat4 wm=entity_mat(n->pos,n->yaw,_cm->base.ground_offset,_cm->base.model_scale,_cm->base.cx,_cm->base.cy,_cm->base.cz);
                draw_charmodel(&skin,_cm,wm,n->anim_idx,n->anim_time,0.0f,proj,view,cam.pos,&ls);}
        }

        ps_draw(&ps,proj,view);
        if(g_interior_idx<0)wf_draw(&g_wfalls,proj,view,total_time);
        wx_draw(&wx,proj,view);
        if(g_interior_idx<0)fishing_world_draw(&fishing,&lr,proj,view,player.phys.pos);
        /* ── Post-processing: tonemap HDR → default FB ─────── */
        if(g_pfx.ready){
            pfx_composite(&g_pfx,win_w,win_h);
        }
        hud_draw(&hud,player.health,player.max_health,as.mana,as.max_mana,as.stam,as.max_stam,&as,&qs,enemies,ne,proj,view,&player,cf_pos,&lvlsys);
        boss_hud_draw(&hud,boss,win_w,win_h);hud_flush(&hud);

        /* ── Death screen ────────────────────────────────────────── */
        if(player.state==PS_DEAD){
            int dmx,dmy;SDL_GetMouseState(&dmx,&dmy);
            bool do_respawn=death_screen_draw(&hud,&death_fade,dt,
                                              win_w,win_h,
                                              player.max_health,
                                              qs.player_gold,qs.player_level,
                                              dmx,dmy);
            if(do_respawn){
                player.health=player.max_health;
                player.state=PS_IDLE;player.anim_idx=player_amap.idle;
                player.anim_time=0;player.hit_flash=0;
                player.phys.pos=(Vec3){cf_wx,terrain_h(cf_wx,cf_wz)+0.1f,cf_wz};
                player.phys.vel=(Vec3){0,0,0};
                death_fade=0.0f;
                player.ooc_timer=6.0f; /* brief grace before regen starts */
            }
        }

        /* ── Campfire proximity hint ─────────────────────────────── */
        if(player.state!=PS_DEAD&&!campfire_menu_open){
            float cfd=vec3_dist(player.phys.pos,cf_pos);
            if(cfd<6.0f){
                float alpha=clampf(1.0f-(cfd-3.0f)/3.0f,0.3f,1.0f);
                const char *hint="[E] Rest at Campfire";
                float hw=1.4f*8.0f*(float)strlen(hint);
                hud_rect(&hud,(float)win_w*0.5f-hw*0.5f-8,(float)win_h*0.62f-4,hw+16,22, 0,0,0,0.6f*alpha);
                hud_str(&hud,(float)win_w*0.5f-hw*0.5f,(float)win_h*0.62f,1.4f,hint, 1.0f,0.75f,0.3f,alpha);
                hud_flush(&hud);
            }
        }

        /* ── Campfire rest menu ───────────────────────────────────── */
        if(campfire_menu_open&&player.state!=PS_DEAD){
            float W=(float)win_w,H=(float)win_h;
            int cmx,cmy;Uint32 cmb=SDL_GetMouseState(&cmx,&cmy);
            bool lmb_now=(cmb&SDL_BUTTON(1))!=0;
            bool clicked=lmb_now&&!campfire_lmb_prev;
            campfire_lmb_prev=lmb_now;

            /* Dim overlay */
            hud_rect(&hud,0,0,W,H, 0.0f,0.0f,0.0f,0.55f);

            float pw=300,bw=260,bh=46,gap=10;
            float ph=4*bh+3*gap+80;
            float px=(W-pw)*0.5f, py=(H-ph)*0.5f;

            /* Panel */
            hud_rect(&hud,px,py,pw,ph, 0.07f,0.04f,0.01f,0.96f);
            hud_rect(&hud,px,py,pw,3,  1.0f,0.45f,0.1f,1.0f);

            /* Title */
            const char *title="CAMPFIRE";
            float tsc=2.2f,tw=tsc*8.0f*(float)strlen(title);
            hud_str(&hud,px+(pw-tw)*0.5f,py+12,tsc,title, 1.0f,0.55f,0.15f,1.0f);

            float bx=px+(pw-bw)*0.5f,by=py+56;
            bool hov,c=false;

            /* REST — restore full HP/MP, advance time to morning */
            hov=(cmx>=bx&&cmx<=bx+bw&&cmy>=by&&cmy<=by+bh);
            hud_rect(&hud,bx,by,bw,bh, hov?0.25f:0.12f,hov?0.15f:0.08f,hov?0.06f:0.03f,0.92f);
            hud_rect(&hud,bx,by,bw,2,  0.8f,0.4f,0.1f,1.0f);
            {const char *l="REST (Restore HP & MP)";float lw=1.5f*8.0f*(float)strlen(l);
             hud_str(&hud,bx+(bw-lw)*0.5f,by+(bh-14)*0.5f,1.5f,l,1.0f,0.9f,0.7f,1.0f);}
            if(hov&&clicked){
                player.health=player.max_health;
                as.mana=as.max_mana;
                /* Skip time to morning (day_frac ~ 0.28 = early day) */
                /* lighting_update reads total_time: advance it to next morning */
                float cur_frac=fmodf(total_time,DAY_DURATION)/DAY_DURATION;
                float target_frac=0.28f;
                float skip=(target_frac>cur_frac)
                    ?(target_frac-cur_frac)*DAY_DURATION
                    :(1.0f-(cur_frac-target_frac))*DAY_DURATION;
                total_time+=skip;
                save_notify(&save_note,"Rested — HP and MP restored. Morning.");
                audio_play(SND_MENU_CLICK,80);
                campfire_menu_open=false;
            }
            by+=bh+gap;

            /* SAVE */
            hov=(cmx>=bx&&cmx<=bx+bw&&cmy>=by&&cmy<=by+bh);
            hud_rect(&hud,bx,by,bw,bh, hov?0.25f:0.12f,hov?0.18f:0.09f,hov?0.08f:0.04f,0.92f);
            hud_rect(&hud,bx,by,bw,2,  0.8f,0.4f,0.1f,1.0f);
            {const char *l="SAVE GAME";float lw=1.5f*8.0f*(float)strlen(l);
             hud_str(&hud,bx+(bw-lw)*0.5f,by+(bh-14)*0.5f,1.5f,l,1.0f,0.9f,0.7f,1.0f);}
            if(hov&&clicked){
                game_save(0,&player,&qs,&as,enemies,ne,total_time,&inv,&lvlsys);
                save_notify(&save_note,"Saved at campfire.");
                audio_play(SND_MENU_CLICK,80);
            }
            by+=bh+gap;

            /* SET RESPAWN */
            hov=(cmx>=bx&&cmx<=bx+bw&&cmy>=by&&cmy<=by+bh);
            hud_rect(&hud,bx,by,bw,bh, hov?0.25f:0.12f,hov?0.18f:0.09f,hov?0.08f:0.04f,0.92f);
            hud_rect(&hud,bx,by,bw,2,  0.8f,0.4f,0.1f,1.0f);
            {const char *l="SET AS RESPAWN POINT";float lw=1.5f*8.0f*(float)strlen(l);
             hud_str(&hud,bx+(bw-lw)*0.5f,by+(bh-14)*0.5f,1.5f,l,1.0f,0.9f,0.7f,1.0f);}
            if(hov&&clicked){
                /* respawn coords are always campfire — this just confirms it */
                save_notify(&save_note,"Respawn point set to this campfire.");
                audio_play(SND_MENU_CLICK,80);
            }
            by+=bh+gap;

            /* CLOSE */
            hov=(cmx>=bx&&cmx<=bx+bw&&cmy>=by&&cmy<=by+bh);
            hud_rect(&hud,bx,by,bw,bh, hov?0.20f:0.10f,hov?0.08f:0.04f,hov?0.04f:0.02f,0.92f);
            hud_rect(&hud,bx,by,bw,2,  0.5f,0.2f,0.05f,1.0f);
            {const char *l="LEAVE";float lw=1.5f*8.0f*(float)strlen(l);
             hud_str(&hud,bx+(bw-lw)*0.5f,by+(bh-14)*0.5f,1.5f,l,0.8f,0.7f,0.6f,1.0f);}
            if(hov&&clicked){campfire_menu_open=false;audio_play(SND_MENU_CLICK,70);}

            hud_flush(&hud);
            /* Close on Escape */
            if(SDL_GetKeyboardState(NULL)[SDL_SCANCODE_ESCAPE]){campfire_menu_open=false;}
        }
        if(save_ui.open){
            int smx,smy;SDL_GetMouseState(&smx,&smy);
            int sv_slot=-1,ld_slot=-1;
            saveui_draw(&save_ui,&hud,win_w,win_h,smx,smy,&sv_slot,&ld_slot);
            hud_flush(&hud);
            /* Handle clicks on mouse-up via event (sampled below in next frame)
               Use a static to debounce — set flag, clear on release */
            static bool saveui_lmb_prev=false;
            bool saveui_lmb=(SDL_GetMouseState(NULL,NULL)&SDL_BUTTON(1))!=0;
            if(!saveui_lmb&&saveui_lmb_prev){ /* on release */
                if(sv_slot>=0){
                    game_save(sv_slot,&player,&qs,&as,enemies,ne,total_time,&inv,&lvlsys);
                    char nb[48];snprintf(nb,sizeof(nb),"Saved — Slot %d",sv_slot+1);
                    save_notify(&save_note,nb);audio_play(SND_MENU_CLICK,80);
                }
                if(ld_slot>=0){
                    if(game_load(ld_slot,&player,&qs,&as,enemies,ne,&total_time,&player_amap,enemy_amaps,&inv,base_max_hp,&lvlsys)){
                        char nb[48];snprintf(nb,sizeof(nb),"Loaded — Slot %d",ld_slot+1);
                        save_notify(&save_note,nb);audio_play(SND_MENU_CLICK,80);
                        inv_recalc(&inv,&player,base_max_hp); /* apply equipped item bonuses */
                        save_ui.open=false; /* auto-close after load */
                    } else {
                        save_notify(&save_note,"Slot empty — nothing to load");
                    }
                }
            }
            saveui_lmb_prev=saveui_lmb;
        }
        if(show_debug){
            debug_draw(&hud,&lr,&perf,&player,enemies,ne,NULL,char_scene_ptr,&cam,&player_amap,win_w,win_h,proj,view,&terrain);
            if(prop_edit&&g_props.count>0){
                props_debug_draw(&g_props,prop_sel);
                props_editor_hud(&hud,&g_props,prop_sel,win_w,win_h);
                hud_flush(&hud);
            }
        }
        proj_draw(&projs,&lr,proj,view); /* always render projectiles */
        /* ── Water Zone Editor (F2) ───────────────────────────── */
        if(wzone_edit){
            /* ── WATERFALL SUB-MODE ── */
            if(wf_edit_mode){
                int wmx2,wmy2;
                Uint32 wmb2=SDL_GetMouseState(&wmx2,&wmy2);
                bool lmb2=(wmb2&SDL_BUTTON(1))!=0;
                bool rmb2=(wmb2&SDL_BUTTON(3))!=0;
                Vec3 wfhit; bool wfgot=false;
                wfgot=river_screen_to_world(wmx2,wmy2,win_w,win_h,proj,view,&wfhit);
                if(wfgot) wfhit.y=terrain_h(wfhit.x,wfhit.z);

                if(lmb2&&!wf_lmb_prev&&wfgot){
                    /* Select if near existing */
                    float best2=12.0f; int besti2=-1;
                    for(int wi=0;wi<g_wfalls.count;wi++){
                        Waterfall *_w=&g_wfalls.wf[wi];
                        if(!_w->active)continue;
                        float dd=sqrtf((_w->pos.x-wfhit.x)*(_w->pos.x-wfhit.x)+
                                       (_w->pos.z-wfhit.z)*(_w->pos.z-wfhit.z));
                        if(dd<best2){best2=dd;besti2=wi;}
                    }
                    if(besti2>=0){ wf_sel=besti2; }
                    else if(g_wfalls.count<MAX_WATERFALLS){
                        /* Place new waterfall at cursor, top at terrain surface */
                        int ni=wf_add(wfhit,wf_place_yaw,wf_place_width,wf_place_height);
                        if(ni>=0){
                            wf_sel=ni;
                            /* Allocate mist emitter */
                            Vec3 base={wfhit.x,wfhit.y-wf_place_height+0.6f,wfhit.z};
                            g_wfalls.wf[ni].mist_em=ps_add_emitter(&ps,base,
                                6,0.88f,0.93f,1.0f,0.14f,0.9f);
                            wf_save();
                            printf("[WFalls] Placed waterfall %d at (%.0f,%.0f,%.0f) W=%.1f H=%.1f\n", ni,
                                   (double)wfhit.x,(double)wfhit.y,(double)wfhit.z,
                                   (double)wf_place_width,(double)wf_place_height);
                        }
                    }
                }
                /* RMB: delete nearest */
                if(rmb2&&wfgot&&g_wfalls.count>0){
                    float best3=20.0f; int bi3=-1;
                    for(int wi=0;wi<g_wfalls.count;wi++){
                        Waterfall *_w=&g_wfalls.wf[wi];
                        if(!_w->active)continue;
                        float dd=sqrtf((_w->pos.x-wfhit.x)*(_w->pos.x-wfhit.x)+
                                       (_w->pos.z-wfhit.z)*(_w->pos.z-wfhit.z));
                        if(dd<best3){best3=dd;bi3=wi;}
                    }
                    if(bi3>=0){
                        /* Deactivate mist emitter */
                        int em=g_wfalls.wf[bi3].mist_em;
                        if(em>=0&&em<ps.ecount) ps.emitters[em].active=false;
                        /* Shift array down */
                        for(int wi=bi3;wi<g_wfalls.count-1;wi++)
                            g_wfalls.wf[wi]=g_wfalls.wf[wi+1];
                        g_wfalls.count--;
                        if(wf_sel>=g_wfalls.count)wf_sel=g_wfalls.count-1;
                        wf_save();
                        printf("[WFalls] Deleted -- %d remaining\n",g_wfalls.count);
                    }
                }
                wf_lmb_prev=lmb2;

                /* Place-mode preview: show where cursor is */
                if(wfgot&&wf_sel<0){
                    float cy2=cosf(wf_place_yaw),sy2=sinf(wf_place_yaw);
                    Vec3 right2={cy2,0,-sy2};
                    float hw2=wf_place_width*0.5f;
                    Vec3 TL2={wfhit.x-right2.x*hw2,wfhit.y,             wfhit.z-right2.z*hw2};
                    Vec3 TR2={wfhit.x+right2.x*hw2,wfhit.y,             wfhit.z+right2.z*hw2};
                    Vec3 BL2={wfhit.x-right2.x*hw2,wfhit.y-wf_place_height,wfhit.z-right2.z*hw2};
                    Vec3 BR2={wfhit.x+right2.x*hw2,wfhit.y-wf_place_height,wfhit.z+right2.z*hw2};
                    lr_push(TL2,TR2,0.4f,1.0f,0.6f);lr_push(BL2,BR2,0.4f,1.0f,0.6f);
                    lr_push(TL2,BL2,0.4f,1.0f,0.6f);lr_push(TR2,BR2,0.4f,1.0f,0.6f);
                    lr_flush(&lr,proj,view);
                }
                wf_editor_draw(&g_wfalls,&lr,proj,view,wf_sel);
                wf_editor_hud(&hud,&g_wfalls,wf_sel,win_w,win_h);

            } else {
            /* ── WATER ZONE SUB-MODE (original) ── */

            int wmx,wmy;
            Uint32 wmb=SDL_GetMouseState(&wmx,&wmy);
            bool lmb_now=(wmb&SDL_BUTTON(1))!=0;
            bool rmb_now=(wmb&SDL_BUTTON(3))!=0;

            /* LMB: place new zone or select existing */
            if(lmb_now&&!wzone_lmb_prev){
                Vec3 hit;
                if(river_screen_to_world(wmx,wmy,win_w,win_h,proj,view,&hit)){
                    /* Select nearest existing if within 20u */
                    float best=20.0f; int besti=-1;
                    for(int zi=0;zi<g_wzone_count;zi++){
                        float d=sqrtf((g_wzones[zi].cx-hit.x)*(g_wzones[zi].cx-hit.x)+
                                      (g_wzones[zi].cz-hit.z)*(g_wzones[zi].cz-hit.z));
                        if(d<best){best=d;besti=zi;}
                    }
                    if(besti>=0){ wzone_sel=besti; }
                    else if(g_wzone_count<MAX_WATER_ZONES){
                        WaterZone *z=&g_wzones[g_wzone_count];
                        z->cx=hit.x; z->cz=hit.z;
                        z->radius=80.0f;
                        z->preset_idx=1; /* Clear Shallow default */
                        z->level=terrain_h(hit.x,hit.z); /* start at terrain surface */
                        z->active=true;
                        wzone_sel=g_wzone_count;
                        g_wzone_count++;
                        terrain_rebuild_all(&terrain);
                        wzone_rebuild_vaos(&terrain);
                        printf("[WZoneEd] Zone %d placed at (%.0f, %.0f) — %s\n",
                               wzone_sel,(double)hit.x,(double)hit.z,
                               WATER_PRESETS[z->preset_idx].name);
                    }
                }
            }
            /* RMB: remove nearest zone */
            if(rmb_now&&g_wzone_count>0){
                Vec3 hit;
                if(river_screen_to_world(wmx,wmy,win_w,win_h,proj,view,&hit)){
                    float best=1e9f; int besti=-1;
                    for(int zi=0;zi<g_wzone_count;zi++){
                        float d=sqrtf((g_wzones[zi].cx-hit.x)*(g_wzones[zi].cx-hit.x)+
                                      (g_wzones[zi].cz-hit.z)*(g_wzones[zi].cz-hit.z));
                        if(d<best){best=d;besti=zi;}
                    }
                    if(besti>=0){
                        /* Shift array down */
                        for(int zi=besti;zi<g_wzone_count-1;zi++)
                            g_wzones[zi]=g_wzones[zi+1];
                        g_wzone_count--;
                        if(wzone_sel>=g_wzone_count)wzone_sel=g_wzone_count-1;
                        terrain_rebuild_all(&terrain);
                        wzone_rebuild_vaos(&terrain);
                        printf("[WZoneEd] Removed zone — %d remaining\n",g_wzone_count);
                    }
                }
            }
            wzone_lmb_prev=lmb_now;

            wzone_editor_draw(&lr,proj,view,wzone_sel);
            wzone_editor_hud(&hud,win_w,win_h,wzone_sel);
            } /* end water zone sub-mode else */
        }

        /* ── Path Editor (F1) ─────────────────────────────────── */
        if(path_edit){
            int pmx2,pmy2;
            Uint32 pmb2=SDL_GetMouseState(&pmx2,&pmy2);
            bool lmb_now=(pmb2&SDL_BUTTON(1))!=0;
            bool rmb_now=(pmb2&SDL_BUTTON(3))!=0;

            /* Left click: place point */
            if(lmb_now&&!path_lmb_prev){
                Vec3 hit;
                if(river_screen_to_world(pmx2,pmy2,win_w,win_h,proj,view,&hit)
                   &&g_cpath.count<CPATH_MAX_PTS){
                    g_cpath.pts[g_cpath.count*2  ]=hit.x;
                    g_cpath.pts[g_cpath.count*2+1]=hit.z;
                    g_cpath.count++;
                    if(g_cpath.count>=2) path_rebuild_mesh();
                    printf("[PathEd] Point %d: (%.0f, %.0f)\n",g_cpath.count,(double)hit.x,(double)hit.z);
                }
            }
            /* Right click: remove last point */
            if(rmb_now&&!path_rmb_prev&&g_cpath.count>0){
                g_cpath.count--;
                if(g_cpath.count>=2) path_rebuild_mesh();
                else g_pmesh.valid=false;
                printf("[PathEd] Removed last point — %d remaining\n",g_cpath.count);
            }
            path_lmb_prev=lmb_now;
            path_rmb_prev=rmb_now;

            path_editor_draw(&lr,proj,view);
            path_editor_hud(&hud,win_w,win_h);
        }
        /* ── River editor ── */
        if(river_edit){
            int rmx,rmy;
            Uint32 rmb=SDL_GetMouseState(&rmx,&rmy);
            bool lmb_now=(rmb&SDL_BUTTON(1))!=0;
            bool rmb_now=(rmb&SDL_BUTTON(3))!=0;

            /* Left click: place point */
            if(lmb_now&&!river_lmb_prev){
                Vec3 hit;
                if(river_screen_to_world(rmx,rmy,win_w,win_h,proj,view,&hit)
                   &&g_criver.count<CRIVER_MAX_PTS){
                    g_criver.pts[g_criver.count*2  ]=hit.x;
                    g_criver.pts[g_criver.count*2+1]=hit.z;
                    g_criver.count++;
                    /* Only rebuild chunks near the new point */
                    terrain_rebuild_near(&terrain,hit.x,hit.z,g_criver.width+TERRAIN_CELL*4);
                    printf("[RiverEd] Point %d: (%.0f, %.0f)\n",
                           g_criver.count,
                           (double)hit.x,(double)hit.z);
                }
            }
            /* Right click: remove last point */
            if(rmb_now&&!river_rmb_prev&&g_criver.count>0){
                float lx=g_criver.pts[(g_criver.count-1)*2];
                float lz=g_criver.pts[(g_criver.count-1)*2+1];
                g_criver.count--;
                terrain_rebuild_near(&terrain,lx,lz,g_criver.width+TERRAIN_CELL*4);
                printf("[RiverEd] Removed last point — %d remaining\n",g_criver.count);
            }
            river_lmb_prev=lmb_now;
            river_rmb_prev=rmb_now;

            river_editor_draw(&lr,proj,view);
            river_editor_hud(&hud,win_w,win_h);
            hud_flush(&hud);
        }
        /* ── Prop Placer (F5) ── */
        if(prop_place_mode){
            int pmx,pmy;
            Uint32 pmb=SDL_GetMouseState(&pmx,&pmy);
            bool lmb_now=(pmb&SDL_BUTTON(1))!=0;
            bool rmb_now=(pmb&SDL_BUTTON(3))!=0;

            /* Raycast cursor to terrain every frame for ghost + placement */
            Vec3 hit; bool got_hit=false;
            got_hit=river_screen_to_world(pmx,pmy,win_w,win_h,proj,view,&hit);
            if(got_hit){
                if(g_placer.snap_y) hit.y=terrain_h(hit.x,hit.z);
                g_placer.ghost_pos=hit;
                g_placer.has_ghost=true;
            }

            /* Left click — place prop */
            if(lmb_now&&!g_placer.lmb_prev&&got_hit&&g_placer.palette_count>0){
                const char *path=g_placer.palette[g_placer.palette_sel];
                float wy=g_placer.snap_y?0.0f:hit.y; /* 0 = auto-snap in prop_register */
                int idx=g_props.count;
                if(prop_register(&g_props,path,
                                 hit.x,wy,hit.z,
                                 0,g_placer.place_ry,0,
                                 g_placer.place_scale,
                                 PROP_COLL_AABB,0,0,0,0)){
                    /* Immediately request streaming so it appears fast */
                    stream_request(&g_stream,idx);
                    printf("[PropPlacer] Placed '%s' at (%.1f,%.1f,%.1f) ry=%.0f sc=%.2f\n",
                           path,(double)hit.x,(double)hit.y,(double)hit.z,
                           (double)g_placer.place_ry,(double)g_placer.place_scale);
                }
            }

            /* Right click — delete nearest prop to cursor hit */
            if(rmb_now&&!g_placer.rmb_prev&&got_hit){
                float best=30.0f*30.0f; int bi=-1;
                for(int i=0;i<g_props.count;i++){
                    Prop *p=&g_props.list[i];if(!p->active)continue;
                    float dx=p->pos.x-hit.x,dz=p->pos.z-hit.z;
                    float d2=dx*dx+dz*dz;
                    if(d2<best){best=d2;bi=i;}
                }
                if(bi>=0){
                    Prop *p=&g_props.list[bi];
                    if(p->stream_state==PROP_LOADED) prop_unload_gpu(p);
                    p->active=false;
                    printf("[PropPlacer] Deleted prop %d (%s)\n",bi,p->path);
                }
            }

            g_placer.lmb_prev=lmb_now;
            g_placer.rmb_prev=rmb_now;

            /* Draw ghost wireframe box at cursor */
            if(g_placer.has_ghost)
                placer_draw_ghost(&lr,g_placer.ghost_pos,g_placer.place_scale,proj,view);

            placer_hud(&hud,&g_placer,win_w,win_h);
        }

        /* ── Forest Brush (F10) ─────────────────────────────── */
        if(forest_edit){
            int fmx,fmy;
            Uint32 fmb=SDL_GetMouseState(&fmx,&fmy);
            bool lmb_now=(fmb&SDL_BUTTON(1))!=0;
            bool rmb_now=(fmb&SDL_BUTTON(3))!=0;

            Vec3 hit; bool got_hit=false;
            got_hit=river_screen_to_world(fmx,fmy,win_w,win_h,proj,view,&hit);
            if(got_hit) hit.y=terrain_h(hit.x,hit.z);

            int hov_idx=-1;
            if(got_hit){
                float best=8.0f*8.0f;
                for(int i=0;i<g_forest.count;i++){
                    ForestTree *t=&g_forest.trees[i];
                    float dx=t->pos.x-hit.x,dz=t->pos.z-hit.z;
                    float d2=dx*dx+dz*dz;
                    if(d2<best){best=d2;hov_idx=i;}
                }
            }

            if(lmb_now&&got_hit){
                forest_brush_paint(&g_forest,hit,forest_brush_radius,forest_brush_density,0.9f,1.8f);
            }
            if(rmb_now&&got_hit){
                forest_brush_erase(&g_forest,&g_props,hit,forest_brush_radius);
            }

            if(got_hit){
                int segs=32;
                for(int si=0;si<segs;si++){
                    float a0=(float)si/(float)segs*(float)(2*M_PI);
                    float a1=(float)(si+1)/(float)segs*(float)(2*M_PI);
                    float x0=hit.x+cosf(a0)*forest_brush_radius;
                    float z0=hit.z+sinf(a0)*forest_brush_radius;
                    float x1=hit.x+cosf(a1)*forest_brush_radius;
                    float z1=hit.z+sinf(a1)*forest_brush_radius;
                    Vec3 pa={x0,terrain_h(x0,z0)+0.2f,z0};
                    Vec3 pb={x1,terrain_h(x1,z1)+0.2f,z1};
                    lr_push(pa,pb,0.25f,0.95f,0.45f);
                }
            }
            if(hov_idx>=0){
                ForestTree *t=&g_forest.trees[hov_idx];
                float h=t->scale*6.0f;
                float r=t->scale*1.2f;
                lr_push((Vec3){t->pos.x-r,t->pos.y,t->pos.z-r},(Vec3){t->pos.x+r,t->pos.y,t->pos.z-r}, 0.8f,1.0f,0.3f);
                lr_push((Vec3){t->pos.x+r,t->pos.y,t->pos.z-r},(Vec3){t->pos.x+r,t->pos.y,t->pos.z+r}, 0.8f,1.0f,0.3f);
                lr_push((Vec3){t->pos.x+r,t->pos.y,t->pos.z+r},(Vec3){t->pos.x-r,t->pos.y,t->pos.z+r}, 0.8f,1.0f,0.3f);
                lr_push((Vec3){t->pos.x-r,t->pos.y,t->pos.z+r},(Vec3){t->pos.x-r,t->pos.y,t->pos.z-r}, 0.8f,1.0f,0.3f);
                lr_push((Vec3){t->pos.x,t->pos.y,t->pos.z},(Vec3){t->pos.x,t->pos.y+h,t->pos.z}, 0.8f,1.0f,0.3f);
            }
            lr_flush(&lr,proj,view);
            forest_hud(&hud,&g_forest,forest_brush_radius,forest_brush_density,hov_idx,win_w,win_h);
        }

        /* ── Spawn editor (F12) ──────────────────────────────── */
        if(spawn_edit_mode){
            int smx,smy;
            Uint32 smb=SDL_GetMouseState(&smx,&smy);
            bool lmb_now=(smb&SDL_BUTTON(1))!=0;
            bool rmb_now=(smb&SDL_BUTTON(3))!=0;

            Vec3 hit; bool got_hit=false;
            got_hit=river_screen_to_world(smx,smy,win_w,win_h,proj,view,&hit);
            if(got_hit) hit.y=terrain_h(hit.x,hit.z);

            /* Find hovered spawn point (nearest to cursor within 6 units) */
            int hov_idx=-1;
            if(got_hit){
                float best=6.0f*6.0f;
                for(int i=0;i<g_spawns.count;i++){
                    SpawnPoint *sp=&g_spawns.points[i];
                    if(!sp->active)continue;
                    float dx=sp->pos.x-hit.x,dz=sp->pos.z-hit.z;
                    float d2=dx*dx+dz*dz;
                    if(d2<best){best=d2;hov_idx=i;}
                }
            }

            /* LMB — place new spawn */
            if(lmb_now&&!spawn_lmb_prev&&got_hit&&g_spawns.count<MAX_SPAWN_POINTS){
                SpawnPoint *sp=&g_spawns.points[g_spawns.count++];
                sp->pos=hit; sp->etype=spawn_sel_type; sp->active=true;
                /* Immediately spawn the enemy in the live world */
                for(int fi=0;fi<MAX_ENEMIES;fi++){
                    if(!enemies[fi].active||enemies[fi].state==ES_DEAD){
                        enemy_spawn(&enemies[fi],spawn_sel_type,hit,&enemy_amaps[spawn_sel_type]);
                        if(fi>=ne) ne=fi+1;
                        break;
                    }
                }
                spawns_save(&g_spawns); /* auto-save so it persists on next load */
                printf("[SpawnEd] Placed %s at (%.1f,%.1f,%.1f) — auto-saved\n",
                       EDEF[spawn_sel_type].name,(double)hit.x,(double)hit.y,(double)hit.z);
            }

            /* RMB — delete hovered spawn */
            if(rmb_now&&!spawn_rmb_prev&&hov_idx>=0){
                printf("[SpawnEd] Deleted spawn %d (%s)\n",
                       hov_idx,EDEF[g_spawns.points[hov_idx].etype].name);
                g_spawns.points[hov_idx].active=false;
                /* Compact array */
                int w=0;
                for(int i=0;i<g_spawns.count;i++)
                    if(g_spawns.points[i].active) g_spawns.points[w++]=g_spawns.points[i];
                g_spawns.count=w;
                spawns_save(&g_spawns); /* auto-save deletion too */
            }

            spawn_lmb_prev=lmb_now;
            spawn_rmb_prev=rmb_now;

            /* Draw markers and HUD */
            spawned_draw_markers(&g_spawns,&lr,&hud,spawn_sel_type,hov_idx,
                                 proj,view,win_w,win_h);
            spawned_hud(&hud,&g_spawns,spawn_sel_type,hov_idx,win_w,win_h);
        }
        /* ── NPC Spawn Editor ──────────────────────────────────── */
        /* ══════════════════════════════════════════════════════════
           TERRAIN SCULPT EDITOR  (F9)
           1=Raise 2=Lower 3=Flatten 4=Smooth
           Wheel=Radius  Shift+Wheel=Strength  S=Save  Z=Undo
           ══════════════════════════════════════════════════════════ */
        if(sculpt_edit){
            int smx,smy;
            Uint32 smb=SDL_GetMouseState(&smx,&smy);
            bool lmb=(smb&SDL_BUTTON(1))!=0;
            bool rmb=(smb&SDL_BUTTON(3))!=0;

            /* Ray-cast cursor onto terrain */
            Vec3 hit; bool got_hit=false;
            got_hit=river_screen_to_world(smx,smy,win_w,win_h,proj,view,&hit);
            if(got_hit) hit.y=terrain_h(hit.x,hit.z);

            /* S = save, Z = undo */
            const Uint8 *sks=SDL_GetKeyboardState(NULL);
            static bool sk_s_prev=false, sk_z_prev=false;
            bool sk_s=sks[SDL_SCANCODE_S]!=0;
            bool sk_z=sks[SDL_SCANCODE_Z]!=0;
            if(sk_s&&!sk_s_prev){
                if(sculpt_mode==5){
                    grass_mask_save();
                    g_grass.last_center=(Vec3){1e9f,0,1e9f};
                } else if(sculpt_mode==6){
                    worn_mask_save();
                    g_grass.last_center=(Vec3){1e9f,0,1e9f};
                } else {
                    sculpt_save();
                    terrain_rebake_hmap(&terrain);
                    printf("[SculptEd] Heightmap rebaked\n");
                }
            }
            if(sk_z&&!sk_z_prev){
                if(sculpt_mode==5&&grass_mask_has_undo){
                    memcpy(g_grass_mask.d,grass_mask_undo,sizeof(g_grass_mask.d));
                    g_grass_mask_active=true;
                    g_grass_mask_seeded=true;
                    grass_mask_has_undo=false;
                    g_grass.last_center=(Vec3){1e9f,0,1e9f};
                    printf("[GrassPaint] Undo applied\n");
                } else if(sculpt_mode==6&&worn_mask_has_undo){
                    memcpy(g_worn_mask.d,worn_mask_undo,sizeof(g_worn_mask.d));
                    g_worn_mask_active=true;
                    worn_mask_upload(&terrain);
                    worn_mask_has_undo=false;
                    g_grass.last_center=(Vec3){1e9f,0,1e9f};
                    printf("[WornPath] Undo applied\n");
                } else if(sculpt_mode<5&&sculpt_has_undo){
                    memcpy(g_sculpt.d, sculpt_undo, sizeof(g_sculpt.d));
                    terrain_rebuild_all(&terrain);
                    terrain_rebake_hmap(&terrain);
                    sculpt_has_undo=false;
                    printf("[SculptEd] Undo applied\n");
                }
            }
            sk_s_prev=sk_s; sk_z_prev=sk_z;

            /* LMB = apply brush, RMB = erase (lower toward zero) */
            if((lmb||rmb)&&got_hit){
                /* Save undo snapshot on first press */
                if(!sculpt_lmb_prev){
                    if(sculpt_mode==5){
                        if(!g_grass_mask_seeded) grass_mask_seed_from_terrain();
                        memcpy(grass_mask_undo,g_grass_mask.d,sizeof(g_grass_mask.d));
                        grass_mask_has_undo=true;
                    } else if(sculpt_mode==6){
                        memcpy(worn_mask_undo,g_worn_mask.d,sizeof(g_worn_mask.d));
                        worn_mask_has_undo=true;
                    } else {
                        memcpy(sculpt_undo, g_sculpt.d, sizeof(g_sculpt.d));
                        sculpt_has_undo=true;
                        /* Capture flatten target at stroke start */
                        if(sculpt_mode==2)
                            sculpt_flatten_h=terrain_h(hit.x,hit.z); /* absolute target */
                    }
                }
                if(sculpt_mode==5){
                    grass_mask_brush(hit.x,hit.z,sculpt_radius,sculpt_strength,rmb);
                    g_grass.last_center=(Vec3){1e9f,0,1e9f};
                } else if(sculpt_mode==6){
                    worn_mask_brush(&terrain,hit.x,hit.z,sculpt_radius,sculpt_strength,rmb);
                    g_grass.last_center=(Vec3){1e9f,0,1e9f};
                } else {
                    int apply_mode = rmb ? 5 : sculpt_mode; /* RMB = erase toward zero */
                    float apply_str = rmb ? sculpt_strength : sculpt_strength;
                    sculpt_brush(hit.x, hit.z, sculpt_radius, apply_str,
                                 apply_mode, sculpt_flatten_h);
                    terrain_rebuild_near(&terrain, hit.x, hit.z,
                                         sculpt_radius + TERRAIN_CELL*4);
                    /* heightmap rebaked on S (save) only, not every stroke */
                }
            }
            sculpt_lmb_prev=(lmb||rmb);

            /* Draw brush circle preview */
            if(got_hit){
                int segs=32;
                for(int si=0;si<segs;si++){
                    float a0=(float)si/(float)segs*(float)(2*M_PI);
                    float a1=(float)(si+1)/(float)segs*(float)(2*M_PI);
                    float x0=hit.x+cosf(a0)*sculpt_radius;
                    float z0=hit.z+sinf(a0)*sculpt_radius;
                    float x1=hit.x+cosf(a1)*sculpt_radius;
                    float z1=hit.z+sinf(a1)*sculpt_radius;
                    Vec3 pa={x0,terrain_h(x0,z0)+0.15f,z0};
                    Vec3 pb={x1,terrain_h(x1,z1)+0.15f,z1};
                    float br=sculpt_mode==0?0.3f:sculpt_mode==1?1.0f:sculpt_mode==2?1.0f:sculpt_mode==4?1.0f:sculpt_mode==5?0.20f:sculpt_mode==6?0.72f:0.3f;
                    float bg=sculpt_mode==0?1.0f:sculpt_mode==1?0.3f:sculpt_mode==2?0.7f:sculpt_mode==4?0.6f:sculpt_mode==5?0.95f:sculpt_mode==6?0.56f:0.7f;
                    float bb=sculpt_mode==0?0.3f:sculpt_mode==1?0.3f:sculpt_mode==2?0.3f:sculpt_mode==4?0.2f:sculpt_mode==5?0.25f:sculpt_mode==6?0.24f:1.0f;
                    lr_push(pa,pb,br,bg,bb);
                }
                lr_flush(&lr,proj,view);
            }

            /* HUD */
            {
                static const char *mode_names[]={"RAISE","LOWER","FLATTEN","SMOOTH","CLIFF","GRASS","WORN"};
                static const float mode_r[]={0.3f,1.0f,1.0f,0.3f,1.0f,0.20f,0.72f};
                static const float mode_g[]={1.0f,0.3f,0.7f,0.7f,0.6f,0.95f,0.56f};
                static const float mode_b[]={0.3f,0.3f,0.3f,1.0f,0.2f,0.25f,0.24f};
                float W=(float)win_w, H=(float)win_h;
                float pw=290, lh=20, pad=10;
                float px=W-pw-6, py=8;
                hud_rect(&hud,px-4,py-4,pw+8,lh*8+pad*2+8, 0,0,0,0.75f);
                hud_str(&hud,px,py,1.4f,"[F9 TERRAIN EDITOR]",0.4f,0.9f,1.0f,1.0f); py+=lh+4;
                /* Mode buttons */
                for(int mi=0;mi<7;mi++){
                    bool active=(mi==sculpt_mode);
                    if(active) hud_rect(&hud,px-2,py-1,pw+4,lh+2,0.1f,0.2f,0.3f,0.9f);
                    char mbuf[32]; snprintf(mbuf,sizeof(mbuf),"%d  %s",mi+1,mode_names[mi]);
                    hud_str(&hud,px+4,py,1.2f,mbuf,
                            active?1.0f:mode_r[mi],active?1.0f:mode_g[mi],
                            active?0.4f:mode_b[mi],1.0f);
                    py+=lh;
                }
                py+=4;
                char rbuf[64]; snprintf(rbuf,sizeof(rbuf),"Radius:   %.0f u",
                                        (double)sculpt_radius);
                hud_str(&hud,px,py,1.2f,rbuf,0.8f,0.8f,0.5f,1.0f); py+=lh;
                char sbuf[64]; snprintf(sbuf,sizeof(sbuf),"Strength: %.3f",
                                        (double)sculpt_strength);
                hud_str(&hud,px,py,1.2f,sbuf,0.8f,0.8f,0.5f,1.0f);
                /* Bottom hint bar */
                float by=H-8.0f, blh=17.0f;
                hud_rect(&hud,0,by-blh*2-4,W,blh*2+8,0,0,0,0.68f);
                hud_str(&hud,8,by-=blh,1.3f,
                    "LMB=Paint  RMB=Erase  1-7=Mode  Wheel=Radius  Shift+Wheel=Strength  S=Save  Z=Undo",
                    0.9f,0.9f,0.6f,1.0f);
                if(sculpt_mode==5)
                    hud_str(&hud,8,by-=blh,1.2f,"[Mode 6 paints grass density mask]",0.5f,0.95f,0.55f,1.0f);
                else if(sculpt_mode==6)
                    hud_str(&hud,8,by-=blh,1.2f,"[Mode 7 paints subtle worn path hints]",0.85f,0.72f,0.45f,1.0f);
                else if(sculpt_has_undo)
                    hud_str(&hud,8,by-=blh,1.2f,"[Z to undo last stroke]",0.6f,0.8f,0.6f,1.0f);
                if(sculpt_mode==5&&grass_mask_has_undo)
                    hud_str(&hud,8,by-=blh,1.2f,"[Z to undo grass stroke]",0.6f,0.8f,0.6f,1.0f);
                if(sculpt_mode==6&&worn_mask_has_undo)
                    hud_str(&hud,8,by-=blh,1.2f,"[Z to undo worn-path stroke]",0.6f,0.8f,0.6f,1.0f);
                hud_flush(&hud);
            }
        }

        if(npc_edit_mode){
            int smx,smy;
            Uint32 smb=SDL_GetMouseState(&smx,&smy);
            bool lmb_now=(smb&SDL_BUTTON(1))!=0;
            bool rmb_now=(smb&SDL_BUTTON(3))!=0;

            Vec3 hit; bool got_hit=false;
            got_hit=river_screen_to_world(smx,smy,win_w,win_h,proj,view,&hit);
            if(got_hit) hit.y=terrain_h(hit.x,hit.z);

            /* Find hovered NPC spawn point */
            int hov_idx=-1;
            if(got_hit){
                float best=6.0f*6.0f;
                for(int i=0;i<g_npc_spawns.count;i++){
                    NpcSpawnPoint *sp=&g_npc_spawns.points[i];
                    if(!sp->active)continue;
                    float dx=sp->pos.x-hit.x,dz=sp->pos.z-hit.z;
                    float d2=dx*dx+dz*dz;
                    if(d2<best){best=d2;hov_idx=i;}
                }
            }

            /* ── P key — toggle waypoint mode on hovered NPC ── */
            const Uint8 *npc_ks=SDL_GetKeyboardState(NULL);
            static bool npc_p_prev=false;
            bool npc_p_now=npc_ks[SDL_SCANCODE_P]!=0;
            if(npc_p_now&&!npc_p_prev){
                if(!npc_ed_wp_mode&&hov_idx>=0){
                    npc_ed_wp_mode=true;
                    npc_ed_wp_npc=hov_idx;
                    printf("[NpcEd] Waypoint mode ON for NPC %d (%s)\n",
                           hov_idx,g_npc_spawns.points[hov_idx].name);
                } else {
                    npc_ed_wp_mode=false;
                    if(npc_ed_wp_npc>=0){
                        /* Apply patrol live to matching NPC in npcs system */
                        NpcSpawnPoint *wsp=&g_npc_spawns.points[npc_ed_wp_npc];
                        if(wsp->wp_count>=2){
                            /* find the live npc by position match */
                            for(int ni=0;ni<npcs.count;ni++){
                                float dx=npcs.npcs[ni].pos.x-wsp->pos.x;
                                float dz=npcs.npcs[ni].pos.z-wsp->pos.z;
                                if(dx*dx+dz*dz<1.0f){
                                    float spd=wsp->patrol_speed>0?wsp->patrol_speed:1.4f;
                                    float wt =wsp->patrol_wait >0?wsp->patrol_wait :2.0f;
                                    npc_patrol_add(ni,spd,wt);
                                    for(int wi=0;wi<wsp->wp_count;wi++)
                                        npc_patrol_waypoint(ni,wsp->waypoints[wi]);
                                    break;
                                }
                            }
                        }
                        npc_spawns_save(&g_npc_spawns);
                        printf("[NpcEd] Waypoint mode OFF — %d waypoints saved\n",
                               g_npc_spawns.points[npc_ed_wp_npc].wp_count);
                    }
                    npc_ed_wp_npc=-1;
                }
            }
            npc_p_prev=npc_p_now;

            /* ── D key — toggle dialogue editor on hovered NPC ── */
            static bool npc_d_prev=false;
            bool npc_d_now=npc_ks[SDL_SCANCODE_D]!=0;
            if(npc_d_now&&!npc_d_prev&&!npc_ed_wp_mode&&!npc_ed_dlg_mode){
                if(!npc_ed_dlg_mode&&hov_idx>=0){
                    npc_ed_dlg_mode=true;
                    npc_ed_dlg_npc=hov_idx;
                    npc_ed_dlg_buf[0]=0; npc_ed_dlg_len=0; npc_ed_dlg_sel=-1;
                    SDL_StartTextInput();
                    printf("[NpcEd] Dialogue mode ON for NPC %d (%s)\n",
                           hov_idx,g_npc_spawns.points[hov_idx].name);
                }
            }
            npc_d_prev=npc_d_now;

            /* ── [ / ] — cycle merchant shop profile on hovered NPC ── */
            static bool npc_lb_prev=false, npc_rb_prev=false;
            bool npc_lb_now=npc_ks[SDL_SCANCODE_LEFTBRACKET]!=0;
            bool npc_rb_now=npc_ks[SDL_SCANCODE_RIGHTBRACKET]!=0;
            if(hov_idx>=0&&!npc_ed_wp_mode){
                NpcSpawnPoint *sp=&g_npc_spawns.points[hov_idx];
                if(sp->is_merchant){
                    if(npc_lb_now&&!npc_lb_prev){
                        sp->shop_id=(sp->shop_id+SHOP_PROFILE_COUNT-1)%SHOP_PROFILE_COUNT;
                        npc_spawns_save(&g_npc_spawns);
                        for(int ni=0;ni<npcs.count;ni++){
                            float dx=npcs.npcs[ni].pos.x-sp->pos.x;
                            float dz=npcs.npcs[ni].pos.z-sp->pos.z;
                            if(dx*dx+dz*dz<1.0f){npcs.npcs[ni].shop_id=sp->shop_id;break;}
                        }
                    }
                    if(npc_rb_now&&!npc_rb_prev){
                        sp->shop_id=(sp->shop_id+1)%SHOP_PROFILE_COUNT;
                        npc_spawns_save(&g_npc_spawns);
                        for(int ni=0;ni<npcs.count;ni++){
                            float dx=npcs.npcs[ni].pos.x-sp->pos.x;
                            float dz=npcs.npcs[ni].pos.z-sp->pos.z;
                            if(dx*dx+dz*dz<1.0f){npcs.npcs[ni].shop_id=sp->shop_id;break;}
                        }
                    }
                }
            }
            npc_lb_prev=npc_lb_now;
            npc_rb_prev=npc_rb_now;

            /* ── Backspace — remove last waypoint in wp mode ── */
            static bool npc_bs_prev=false;
            bool npc_bs_now=npc_ks[SDL_SCANCODE_BACKSPACE]!=0;
            if(npc_bs_now&&!npc_bs_prev&&npc_ed_wp_mode&&npc_ed_wp_npc>=0){
                NpcSpawnPoint *wsp=&g_npc_spawns.points[npc_ed_wp_npc];
                if(wsp->wp_count>0){
                    wsp->wp_count--;
                    printf("[NpcEd] Removed last waypoint (%d remain)\n",wsp->wp_count);
                }
            }
            npc_bs_prev=npc_bs_now;

            if(npc_ed_wp_mode&&npc_ed_wp_npc>=0){
                /* ── WAYPOINT MODE: LMB adds waypoint, RMB exits ── */
                NpcSpawnPoint *wsp=&g_npc_spawns.points[npc_ed_wp_npc];
                if(lmb_now&&!npc_ed_lmb_prev&&got_hit&&wsp->wp_count<NPC_WP_MAX){
                    Vec3 wp=hit; wp.y=terrain_h(wp.x,wp.z)+0.05f;
                    wsp->waypoints[wsp->wp_count++]=wp;
                    printf("[NpcEd] Added waypoint %d at (%.1f,%.1f)\n",
                           wsp->wp_count,(double)wp.x,(double)wp.z);
                }
                /* Draw waypoint path lines */
                if(wsp->wp_count>0){
                    for(int wi=0;wi<wsp->wp_count;wi++){
                        Vec3 a=wsp->waypoints[wi];
                        Vec3 b=wsp->waypoints[(wi+1)%wsp->wp_count];
                        lr_push(a,b,0.2f,1.0f,0.2f);
                        /* Small cross at each waypoint */
                        Vec3 xl={a.x-0.5f,a.y,a.z},xr={a.x+0.5f,a.y,a.z};
                        Vec3 zf={a.x,a.y,a.z-0.5f},zb={a.x,a.y,a.z+0.5f};
                        lr_push(xl,xr,0.2f,1.0f,0.2f);
                        lr_push(zf,zb,0.2f,1.0f,0.2f);
                    }
                    /* Line from last wp to cursor */
                    if(got_hit) lr_push(wsp->waypoints[wsp->wp_count-1],hit,0.5f,1.0f,0.5f);
                }
                lr_flush(&lr,proj,view);
            } else {
                /* ── NORMAL MODE: LMB place NPC, RMB delete ── */
                if(lmb_now&&!npc_ed_lmb_prev&&got_hit&&g_npc_spawns.count<MAX_NPC_SPAWNS&&npcs.count<NPC_MAX){
                    NpcSpawnPoint *sp=&g_npc_spawns.points[g_npc_spawns.count++];
                    memset(sp,0,sizeof(*sp));
                    sp->pos=hit; sp->yaw=npc_ed_yaw;
                    sp->model_idx=npc_sel_type; sp->active=true;
                    sp->patrol_speed=1.4f; sp->patrol_wait=2.0f;
                    strncpy(sp->name,NPC_TYPE_NAMES[npc_sel_type],31);
                    int idx=npc_add(&npcs,hit,npc_ed_yaw,NPC_TYPE_NAMES[npc_sel_type]);
                    npcs.npcs[idx].model_idx=npc_sel_type;
                    npcs.npcs[idx].anim_idx=0;
                    npc_spawns_save(&g_npc_spawns);
                    printf("[NpcEd] Placed %s at (%.1f,%.1f) yaw=%.2f\n",
                           NPC_TYPE_NAMES[npc_sel_type],(double)hit.x,(double)hit.z,(double)npc_ed_yaw);
                }
                if(rmb_now&&!npc_ed_rmb_prev&&hov_idx>=0){
                    printf("[NpcEd] Deleted NPC %d (%s)\n",
                           hov_idx,g_npc_spawns.points[hov_idx].name);
                    g_npc_spawns.points[hov_idx].active=false;
                    int w=0;
                    for(int i=0;i<g_npc_spawns.count;i++)
                        if(g_npc_spawns.points[i].active)
                            g_npc_spawns.points[w++]=g_npc_spawns.points[i];
                    g_npc_spawns.count=w;
                    npc_spawns_save(&g_npc_spawns);
                }
            }

            npc_ed_lmb_prev=lmb_now;
            npc_ed_rmb_prev=rmb_now;

            npc_spawns_draw_markers(&g_npc_spawns,&lr,&hud,npc_sel_type,hov_idx,
                                    proj,view,win_w,win_h);
            npc_spawns_hud(&hud,&g_npc_spawns,npc_sel_type,hov_idx,win_w,win_h);
            if(npc_ed_wp_mode)
                npc_spawns_hud_wpmode(&hud,&g_npc_spawns,npc_ed_wp_npc,win_w,win_h);
            if(npc_ed_dlg_mode)
                npc_spawns_hud_dialogue(&hud,&g_npc_spawns,npc_ed_dlg_npc,
                                        npc_ed_dlg_sel,npc_ed_dlg_buf,win_w,win_h);
        }
        /* NPC interaction hints + chat box */
        if(!npcs.open)npc_draw_hints(&npcs,&hud,player.phys.pos,proj,view,win_w,win_h,&qs);
        /* Map screen — drawn before shop/npc so they can overlay */
        if(map_screen.open)
            map_draw(&map_screen,&hud,win_w,win_h,player.phys.pos,player.phys.yaw,&qs,enemies,ne,&npcs);

        /* Shop — drawn when open */
        if(g_shop.open){
            int smx2,smy2;SDL_GetMouseState(&smx2,&smy2);
            shop_draw(&g_shop,&hud,win_w,win_h,smx2,smy2,dt,&g_shop_defs[g_active_shop_id],&inv,&qs,&player,base_max_hp);
        }

        npc_draw(&npcs,&hud,win_w,win_h,&qs);

        /* World item icons + pickup prompts */
        wi_draw_hud(&world_items,&hud,player.phys.pos,proj,view,win_w,win_h,total_time);
        fishing_draw(&fishing,&hud,win_w,win_h);
        /* Save/load notification toast */
        save_notify_draw(&save_note,&hud,win_w,win_h,dt);
        /* Inventory panel */
        {int imx,imy;SDL_GetMouseState(&imx,&imy);
         inv_draw(&inv,&hud,win_w,win_h,imx,imy);}
        /* Subtle F-key hint at bottom */
        hud_str(&hud,8,(float)win_h-18,1.1f,"TAB Save/Load  I Inventory  M Map  E Pickup  Q Roll  R Block",0.4f,0.4f,0.4f,0.5f);
        hud_flush(&hud);

        /* ── Shield active overlay ────────────────────────────── */
        if(player.shield_timer>0){
            float pulse=0.5f+0.5f*sinf(total_time*8.0f);
            float a=0.08f+0.05f*pulse;
            hud_rect(&hud,0,0,(float)win_w,(float)win_h, 0.1f,0.3f,0.9f,a);
            /* Shield timer bar along bottom edge */
            float max_shield=4.0f;
            float sfrac=clampf(player.shield_timer/max_shield,0,1);
            hud_rect(&hud,0,(float)win_h-5,(float)win_w*sfrac,5, 0.2f,0.5f,1.0f,0.85f);
            hud_flush(&hud);
        }

        /* ── Death screen ─────────────────────────────────────── */
        if(player.state==PS_DEAD){
            float cx=(float)win_w*0.5f,cy=(float)win_h*0.38f;
            hud_rect(&hud,0,0,(float)win_w,(float)win_h,0,0,0,0.5f);
            hud_str(&hud,cx-72,cy,    3.5f,"YOU DIED",0.85f,0.05f,0.05f,1.0f);
            hud_str(&hud,cx-108,cy+52,1.6f,"ENTER to Respawn",0.90f,0.90f,0.90f,0.85f);
            hud_flush(&hud);
        }

        /* ── Pause menu ───────────────────────────────────────── */
        {
            bool want_resume=false,want_quit_menu=false,want_quit_game=false;
            pause_draw(&g_pause,&hud,win_w,win_h,
                       &player,&qs,&as,enemies,ne,total_time,&inv,&lvlsys,&save_note,
                       &want_resume,&want_quit_menu,&want_quit_game);
            if(want_quit_game) running=false;
            if(want_quit_menu){ running=false; restart_to_menu=true; }
        }
        /* ── Intro fade-in (black → transparent over 1.5s) ── */
        if(intro_fade>0.0f){
            intro_fade=clampf(intro_fade-dt/1.5f,0.0f,1.0f);
            glDisable(GL_DEPTH_TEST);
            hud_rect(&hud,0,0,(float)win_w,(float)win_h, 0,0,0,intro_fade);
            hud_flush(&hud);
            glEnable(GL_DEPTH_TEST);
        }
        char title[256];snprintf(title,sizeof(title),"RPG | HP:%.0f | MP:%.0f | Gold:%d XP:%d",(double)player.health,(double)as.mana,qs.player_gold,qs.player_xp);
        SDL_SetWindowTitle(win,title);SDL_GL_SwapWindow(win);
    }

    charmodel_free(&char_model);
    grass_free(&g_grass);
    #undef ENSURE_ENEMY_MODEL
    wint_free(&g_wint);
    puddle_free(&g_puddles);
    footprint_free(&g_footprints);
    /* Only free models that were actually lazy-loaded */
    for(int t=0;t<ETYPE_COUNT;t++)
        if(enemy_model_loaded[t]&&enemy_models[t].owns_base) charmodel_free(&enemy_models[t]);
    for(int _mi=0;_mi<NPC_MODEL_MAX;_mi++)if(g_npc_model_ok[_mi])charmodel_free(&g_npc_models[_mi]);
    stream_shutdown(&g_stream);  /* stop worker thread before freeing props it references */
    props_free(&g_props);
    if(restart_to_menu){
        goto menu_start;
    }
    audio_shutdown();
    SDL_GL_DeleteContext(ctx);SDL_DestroyWindow(win);SDL_Quit();
    return 0;
}

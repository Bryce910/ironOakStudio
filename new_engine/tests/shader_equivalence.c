/* Render the historical and current shaders against identical inputs.
 * Run from the engine root. Offscreen rendering validates output, not speed.
 */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "stb_image.h"
#include "gl_helpers.h"
#define SIDE 48
static const char *vs=
"#version 330 core\n"
"uniform vec3 uTestPos,uTestNormal;\n"
"out vec3 vNormal,vPos,vColor,vTangent,vBitangent;out vec2 vUV;\n"
"void main(){vec2 q[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));vec2 p=q[gl_VertexID];gl_Position=vec4(p,0,1);vPos=uTestPos+vec3(p.x*4,0,p.y*4);vNormal=uTestNormal;vColor=vec3(1);vTangent=vec3(1,0,0);vBitangent=vec3(0,0,1);vUV=p*.5+.5;}\n";
static unsigned rng=1977;
static float rand01(void){rng=rng*1664525u+1013904223u;return (float)((rng>>8)&0xffffff)/16777216.0f;}
static GLuint prog(const char *path){char *src=glsl_load(path);assert(src);GLuint p=GLH_program(vs,src);free(src);GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);assert(ok);return p;}
static void set3(GLuint p,const char *n,float x,float y,float z){glUniform3f(glGetUniformLocation(p,n),x,y,z);}
static void set1(GLuint p,const char *n,float x){glUniform1f(glGetUniformLocation(p,n),x);}
static void seti(GLuint p,const char *n,int x){glUniform1i(glGetUniformLocation(p,n),x);}
static void uniforms(GLuint p,int which,int is_static){
 glUseProgram(p);
 float y=(which%4==0)?1.2f:(which%4==1)?7:(which%4==2)?30:65;
 float x=(which%3)*14.0f,z=(which%5)*13.0f;
 set3(p,"uTestPos",x,y,z);
 float nx=which%4==2?.7f:0,ny=which%4==2?.7141428f:1;
 set3(p,"uTestNormal",nx,ny,0);
 float distance=(which/4)%3==0?20:((which/4)%3==1?239.99f:260);
 set3(p,"uCamPos",x,y+distance,z);
 set3(p,"uSunDir",.45f,(which%2)? .65f:-.65f,.6f);
 set3(p,"uSunColor",1,.78f,.56f);set3(p,"uAmbient",.12f,.14f,.18f);
 set1(p,"uSnowAccum",which%4==3?.5f:0);set1(p,"uWetness",which%3==1?.6f:0);
 set1(p,"uFogDensity",.001f);set3(p,"uFogColor",.15f,.2f,.3f);
 set3(p,"uCampfirePos",-60,0,-60);
 glUniform2f(glGetUniformLocation(p,"uWorldMin"),-250,-250);glUniform2f(glGetUniformLocation(p,"uWorldSize"),500,500);
 seti(p,"uHasNrm",1);seti(p,"uHasRgh",1);seti(p,"uToonShading",which>=24);set1(p,"uToonBands",4);
 seti(p,"uNumLights",1);set3(p,"uLightPos[0]",x,y+4,z+2);set3(p,"uLightColor[0]",.8f,.35f,.12f);set1(p,"uLightRadius[0]",15);
 glUniform4f(glGetUniformLocation(p,"uLightParams[0]"),0,0,0,0);
 float light[16]={.004f,0,0,0,0,.004f,0,0,0,0,.004f,0,0,0,0,1};
 glUniformMatrix4fv(glGetUniformLocation(p,"uLightVP"),1,0,light);glUniformMatrix4fv(glGetUniformLocation(p,"uLightVPWide"),1,0,light);
 const char *names[]={"uTexSand","uTexGrass","uTexDirt","uTexRock","uTexSnow","uGrassCoverage","uNrmSand","uNrmGrass","uNrmDirt","uNrmRock","uNrmSnow","uRghSand","uRghGrass","uRghDirt","uRghRock","uRghSnow","uWearMask","uFarmMask","uShadowMapWide","uShadowMap"};
 for(int i=0;i<20;i++)seti(p,names[i],i);
 if(is_static){
  seti(p,"uAlbedo",1);seti(p,"uNormal",7);seti(p,"uMR",12);seti(p,"uEmissive",3);
  seti(p,"uHasTexture",1);seti(p,"uHasNormal",which%3==1);seti(p,"uHasMR",1);seti(p,"uHasEmissive",0);
  glUniform4f(glGetUniformLocation(p,"uBaseColor"),.6f,.8f,.4f,1);
  set1(p,"uAlphaCutoff",which%3==2?.05f:0);set1(p,"uMetallic",.2f);set1(p,"uRoughness",.7f);
 }
}

/* Read the actual runtime C string rather than copying the optimized filter
 * into this test. This keeps an implementation regression visible here. */
static char *runtime_ssao_blur_source(void){
    FILE *file=fopen("main.c","rb");assert(file);
    assert(fseek(file,0,SEEK_END)==0);long size=ftell(file);assert(size>0);
    rewind(file);char *source=malloc((size_t)size+1);assert(source);
    assert(fread(source,1,(size_t)size,file)==(size_t)size);source[size]='\0';fclose(file);
    const char *marker="const char *ssao_blur_fs=";
    char *p=strstr(source,marker);assert(p);p+=strlen(marker);
    char *shader=malloc((size_t)size+1);assert(shader);size_t length=0;
    for(;;){
        while(isspace((unsigned char)*p))p++;
        if(p[0]=='/'&&p[1]=='*'){
            p=strstr(p+2,"*/");assert(p);p+=2;continue;
        }
        if(p[0]=='/'&&p[1]=='/'){
            p=strchr(p+2,'\n');assert(p);continue;
        }
        if(*p==';')break;
        assert(*p=='"');p++;
        while(*p!='"'){
            assert(*p);char c=*p++;
            if(c=='\\'){
                c=*p++;assert(c);
                if(c=='n')c='\n';
                else if(c=='r')c='\r';
                else if(c=='t')c='\t';
                else assert(c=='\\'||c=='"');
            }
            shader[length++]=c;
        }
        p++;
    }
    shader[length]='\0';free(source);return shader;
}

static void test_ssao_blur(GLuint fbo,GLuint output){
    GLuint before=prog("tests/fixtures/shaders/ssao_blur_25tap.before.fs.glsl");
    char *runtime=runtime_ssao_blur_source();GLuint after=GLH_program(vs,runtime);free(runtime);
    GLint linked=0;glGetProgramiv(after,GL_LINK_STATUS,&linked);assert(linked);
    const int sizes[][2]={{1,1},{1,7},{2,3},{13,9},{48,48},{63,37},{257,139}};
    GLuint input;glGenTextures(1,&input);glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,input);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_PACK_ALIGNMENT,1);
    double largest_error[2]={0,0};unsigned largest_code_error=0;int cases=0;
    for(size_t size=0;size<sizeof(sizes)/sizeof(sizes[0]);size++){
        int width=sizes[size][0],height=sizes[size][1];size_t count=(size_t)width*height;
        unsigned char *values=malloc(count),*quantized_before=malloc(count),*quantized_after=malloc(count);
        float *a=malloc(count*sizeof(float)),*b=malloc(count*sizeof(float));
        assert(values&&quantized_before&&quantized_after&&a&&b);
        for(int float_source=0;float_source<2;float_source++)for(int pattern=0;pattern<6;pattern++){
            for(int y=0;y<height;y++)for(int x=0;x<width;x++){
                unsigned char value=(unsigned char)(rand01()*256.0f);
                if(pattern==1)value=((x+y)%2)?0:255;
                else if(pattern==2)value=(x==0||y==0||x==width-1||y==height-1)?255:0;
                else if(pattern==3)value=(unsigned char)((x*23+y*41)%256);
                else if(pattern==4)value=0;
                else if(pattern==5)value=255;
                values[(size_t)y*width+x]=value;
            }
            glBindTexture(GL_TEXTURE_2D,input);
            glTexImage2D(GL_TEXTURE_2D,0,float_source?GL_R32F:GL_R8,width,height,0,GL_RED,GL_UNSIGNED_BYTE,values);
            for(int quantized=0;quantized<2;quantized++){
                glBindTexture(GL_TEXTURE_2D,output);
                glTexImage2D(GL_TEXTURE_2D,0,quantized?GL_R8:GL_R32F,width,height,0,GL_RED,GL_FLOAT,NULL);
                glBindTexture(GL_TEXTURE_2D,input);
                glBindFramebuffer(GL_FRAMEBUFFER,fbo);
                assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
                glViewport(0,0,width,height);
                glUseProgram(before);seti(before,"uAO",0);glDrawArrays(GL_TRIANGLES,0,3);
                glReadPixels(0,0,width,height,GL_RED,quantized?GL_UNSIGNED_BYTE:GL_FLOAT,quantized?(void*)quantized_before:(void*)a);
                glUseProgram(after);seti(after,"uAO",0);glDrawArrays(GL_TRIANGLES,0,3);
                glReadPixels(0,0,width,height,GL_RED,quantized?GL_UNSIGNED_BYTE:GL_FLOAT,quantized?(void*)quantized_after:(void*)b);
                for(size_t i=0;i<count;i++){
                    if(quantized){
                        unsigned delta=(unsigned)abs((int)quantized_before[i]-(int)quantized_after[i]);
                        if(delta>largest_code_error)largest_code_error=delta;
                        /* Even floating input can straddle an R8 rounding
                           boundary after reassociation on a different GPU.
                           Float-output comparisons below keep the tighter
                           mathematical tolerance; quantized output allows
                           only one code value for either source format. */
                        if(delta>1u){
                            fprintf(stderr,"SSAO R8 mismatch at %dx%d, source %s, pattern %d, pixel %zu: %u vs %u\n",width,height,float_source?"float":"R8",pattern,i,quantized_before[i],quantized_after[i]);
                            exit(1);
                        }
                    }else{
                        assert(isfinite(a[i])&&isfinite(b[i]));double error=fabs(a[i]-b[i]);
                        if(error>largest_error[float_source])largest_error[float_source]=error;
                        // Fixed-point R8 filtering may round each paired sample.
                        // Floating input checks the mathematical equivalence; R8
                        // permits at most one normalized color code of error.
                        double tolerance=float_source?2e-5:1.0/255.0;
                        if(error>tolerance){fprintf(stderr,"SSAO float mismatch at %dx%d, source %s, pattern %d, pixel %zu: %.9g\n",width,height,float_source?"float":"R8",pattern,i,error);exit(1);}
                    }
                }
                cases++;
            }
        }
        free(values);free(quantized_before);free(quantized_after);free(a);free(b);
    }
    assert(glGetError()==GL_NO_ERROR);
    printf("PASS: %d SSAO blur image cases, max difference float source %.9g / R8 source %.9g; R8 output max %u code value(s)\n",cases,largest_error[1],largest_error[0],largest_code_error);
    glDeleteTextures(1,&input);glDeleteProgram(before);glDeleteProgram(after);
}
int main(void){
 if(!SDL_getenv("VALWOOD_TEST_NATIVE_GL")) SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
 assert(!SDL_Init(SDL_INIT_VIDEO));
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
 SDL_Window *w=SDL_CreateWindow("shader reference",0,0,SIDE,SIDE,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(w);SDL_GLContext c=SDL_GL_CreateContext(w);assert(c);
 GLuint old[2]={prog("tests/fixtures/shaders/terrain_land.before.fs.glsl"),prog("tests/fixtures/shaders/static_pbr.before.fs.glsl")};
 GLuint now[2]={prog("shaders/terrain_land.fs.glsl"),prog("shaders/static_pbr.fs.glsl")};
 GLuint output,fbo;glGenTextures(1,&output);glBindTexture(GL_TEXTURE_2D,output);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,SIDE,SIDE,0,GL_RGBA,GL_FLOAT,NULL);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,output,0);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
 GLuint textures[20];glGenTextures(20,textures);
 float pixels[32*32*4];
 for(int i=0;i<20;i++){
  for(int k=0;k<32*32;k++){for(int ch=0;ch<3;ch++)pixels[k*4+ch]=.1f+rand01()*.8f;pixels[k*4+3]=1;
   if(i>=6&&i<=10){pixels[k*4]=.45f+rand01()*.1f;pixels[k*4+1]=.45f+rand01()*.1f;pixels[k*4+2]=.8f+rand01()*.2f;}
   if(i==5){pixels[k*4]=.9f;pixels[k*4+1]=pixels[k*4+2]=0;}
   if(i==16||i==17){pixels[k*4]=.05f;pixels[k*4+1]=0;}
  }
  glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,textures[i]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,32,32,0,GL_RGBA,GL_FLOAT,pixels);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);glGenerateMipmap(GL_TEXTURE_2D);
 }
 GLuint vao;glGenVertexArrays(1,&vao);glBindVertexArray(vao);glViewport(0,0,SIDE,SIDE);
 float a[SIDE*SIDE*4],b[SIDE*SIDE*4];double maxerr=0;int cases=0;
 for(int type=0;type<2;type++)for(int which=0;which<36;which++){
  uniforms(old[type],which,type);glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,3);glReadPixels(0,0,SIDE,SIDE,GL_RGBA,GL_FLOAT,a);
  uniforms(now[type],which,type);glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,3);glReadPixels(0,0,SIDE,SIDE,GL_RGBA,GL_FLOAT,b);
  double e=0;for(int k=0;k<SIDE*SIDE*4;k++){assert(isfinite(a[k])&&isfinite(b[k]));double d=fabs(a[k]-b[k]);if(d>e)e=d;}
  if(e>maxerr)maxerr=e;
  if(e>2e-5){fprintf(stderr,"Case type=%d idx=%d differs: %.8g\n",type,which,e);return 1;}
  cases++;
 }
 assert(glGetError()==GL_NO_ERROR);
 printf("PASS: %d terrain/static PBR image cases, maximum linear RGBA difference %.9g\n",cases,maxerr);
 glDeleteProgram(old[0]);glDeleteProgram(old[1]);glDeleteProgram(now[0]);glDeleteProgram(now[1]);
 glDeleteTextures(20,textures);
 test_ssao_blur(fbo,output);
 printf("Shader equivalence renderer: %s\n",glGetString(GL_RENDERER));
 glDeleteVertexArrays(1,&vao);glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&output);
 SDL_GL_DeleteContext(c);SDL_DestroyWindow(w);SDL_Quit();return 0;
}

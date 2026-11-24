#define _GNU_SOURCE
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <dlfcn.h>
#include <time.h>
#include <stdio.h>
#include <stdatomic.h>
static _Atomic unsigned reports=0;
static double tick(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static void report(const char*n,double t){double ms=(tick()-t)*1000;if(ms>50&&atomic_fetch_add(&reports,1)<80)fprintf(stderr,"[Native API timing] %s %.3fms\n",n,ms);}
#define TRACE_VOID(name,args,call) void name args {static void(*real)args;if(!real)real=dlsym(RTLD_NEXT,#name);double t=tick();real call;report(#name,t);}
TRACE_VOID(SDL_GetWindowPosition,(SDL_Window*w,int*x,int*y),(w,x,y))
TRACE_VOID(SDL_GetWindowSize,(SDL_Window*w,int*x,int*y),(w,x,y))
TRACE_VOID(SDL_GL_GetDrawableSize,(SDL_Window*w,int*x,int*y),(w,x,y))
TRACE_VOID(SDL_PumpEvents,(void),())
TRACE_VOID(glGetIntegerv,(GLenum p,GLint*v),(p,v))
Uint32 SDL_GetGlobalMouseState(int*x,int*y){static Uint32(*real)(int*,int*);if(!real)real=dlsym(RTLD_NEXT,"SDL_GetGlobalMouseState");double t=tick();Uint32 r=real(x,y);report("SDL_GetGlobalMouseState",t);return r;}
int SDL_NumJoysticks(void){static int(*real)(void);if(!real)real=dlsym(RTLD_NEXT,"SDL_NumJoysticks");double t=tick();int r=real();report("SDL_NumJoysticks",t);return r;}
SDL_GameController* SDL_GameControllerOpen(int i){static SDL_GameController*(*real)(int);if(!real)real=dlsym(RTLD_NEXT,"SDL_GameControllerOpen");double t=tick();SDL_GameController*r=real(i);report("SDL_GameControllerOpen",t);return r;}

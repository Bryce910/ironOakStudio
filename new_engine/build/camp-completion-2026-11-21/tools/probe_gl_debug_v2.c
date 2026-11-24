
#define _GNU_SOURCE
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <stdio.h>
#include <stdint.h>
static unsigned messages;
static void APIENTRY report(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar*message,const void*userdata){
 (void)source;(void)length;(void)userdata;
 if(type!=GL_DEBUG_TYPE_ERROR&&severity!=GL_DEBUG_SEVERITY_HIGH)return;
 if(messages++>=32)return;
 fprintf(stderr,"[GL probe] type=0x%x severity=0x%x id=%u: %s\n",type,severity,id,message);
 GLint program=0;glGetIntegerv(GL_CURRENT_PROGRAM,&program);fprintf(stderr,"[GL probe] current_program=%d\n",program);
 if(program){GLint uniforms=0;glGetProgramiv((GLuint)program,GL_ACTIVE_UNIFORMS,&uniforms);for(GLint index=0;index<uniforms;index++){char name[256]={};GLsizei length=0;GLint size=0;GLenum type=0;glGetActiveUniform((GLuint)program,(GLuint)index,sizeof(name),&length,&size,&type,name);GLint location=glGetUniformLocation((GLuint)program,name);if(location<=121&&location+size>121)fprintf(stderr,"[GL probe] uniform121 name=%s base=%d size=%d type=0x%x\n",name,location,size,type);}}
 void*stack[24];int count=backtrace(stack,24);
 for(int i=0;i<count;i++){Dl_info info;if(dladdr(stack[i],&info)&&info.dli_fname)fprintf(stderr,"[GL probe] %s+0x%lx %s\n",info.dli_fname,(unsigned long)((uintptr_t)stack[i]-(uintptr_t)info.dli_fbase),info.dli_sname?info.dli_sname:"");}
 fflush(stderr);
}
SDL_GLContext SDL_GL_CreateContext(SDL_Window*window){
 SDL_GLContext(*original)(SDL_Window*)=dlsym(RTLD_NEXT,"SDL_GL_CreateContext");
 SDL_GLContext result=original(window);
 if(result&&SDL_GL_ExtensionSupported("GL_KHR_debug")){
  PFNGLDEBUGMESSAGECALLBACKPROC callback=(PFNGLDEBUGMESSAGECALLBACKPROC)SDL_GL_GetProcAddress("glDebugMessageCallback");
  if(callback){glEnable(GL_DEBUG_OUTPUT);glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);callback(report,0);fprintf(stderr,"[GL probe] Synchronous KHR_debug installed (first32 errors); no error state drained\n");}
 }
 return result;
}

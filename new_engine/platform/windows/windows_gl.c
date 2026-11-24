#include "platform/windows/windows_gl.h"
#include <SDL2/SDL.h>
#include <stdio.h>
PFNGLACTIVETEXTUREPROC vw_glActiveTexture;
PFNGLATTACHSHADERPROC vw_glAttachShader;
PFNGLBINDBUFFERPROC vw_glBindBuffer;
PFNGLBINDFRAMEBUFFERPROC vw_glBindFramebuffer;
PFNGLBINDRENDERBUFFERPROC vw_glBindRenderbuffer;
PFNGLBINDSAMPLERPROC vw_glBindSampler;
PFNGLBINDVERTEXARRAYPROC vw_glBindVertexArray;
PFNGLBLENDEQUATIONPROC vw_glBlendEquation;
PFNGLBLENDEQUATIONSEPARATEPROC vw_glBlendEquationSeparate;
PFNGLBLENDFUNCSEPARATEPROC vw_glBlendFuncSeparate;
PFNGLBLITFRAMEBUFFERPROC vw_glBlitFramebuffer;
PFNGLBUFFERDATAPROC vw_glBufferData;
PFNGLBUFFERSUBDATAPROC vw_glBufferSubData;
PFNGLCHECKFRAMEBUFFERSTATUSPROC vw_glCheckFramebufferStatus;
PFNGLCLEARBUFFERFVPROC vw_glClearBufferfv;
PFNGLCLIPCONTROLPROC vw_glClipControl;
PFNGLCOMPILESHADERPROC vw_glCompileShader;
PFNGLCOMPRESSEDTEXIMAGE2DPROC vw_glCompressedTexImage2D;
PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC vw_glCompressedTexSubImage2D;
PFNGLCREATEPROGRAMPROC vw_glCreateProgram;
PFNGLCREATESHADERPROC vw_glCreateShader;
PFNGLDELETEBUFFERSPROC vw_glDeleteBuffers;
PFNGLDELETEFRAMEBUFFERSPROC vw_glDeleteFramebuffers;
PFNGLDELETEPROGRAMPROC vw_glDeleteProgram;
PFNGLDELETEQUERIESPROC vw_glDeleteQueries;
PFNGLDELETERENDERBUFFERSPROC vw_glDeleteRenderbuffers;
PFNGLDELETESAMPLERSPROC vw_glDeleteSamplers;
PFNGLDELETESHADERPROC vw_glDeleteShader;
PFNGLDELETEVERTEXARRAYSPROC vw_glDeleteVertexArrays;
PFNGLDETACHSHADERPROC vw_glDetachShader;
PFNGLDISABLEVERTEXATTRIBARRAYPROC vw_glDisableVertexAttribArray;
PFNGLDRAWARRAYSINSTANCEDPROC vw_glDrawArraysInstanced;
PFNGLDRAWBUFFERSPROC vw_glDrawBuffers;
PFNGLDRAWELEMENTSBASEVERTEXPROC vw_glDrawElementsBaseVertex;
PFNGLDRAWELEMENTSINSTANCEDPROC vw_glDrawElementsInstanced;
PFNGLENABLEVERTEXATTRIBARRAYPROC vw_glEnableVertexAttribArray;
PFNGLFRAMEBUFFERRENDERBUFFERPROC vw_glFramebufferRenderbuffer;
PFNGLFRAMEBUFFERTEXTURE2DPROC vw_glFramebufferTexture2D;
PFNGLGENBUFFERSPROC vw_glGenBuffers;
PFNGLGENFRAMEBUFFERSPROC vw_glGenFramebuffers;
PFNGLGENQUERIESPROC vw_glGenQueries;
PFNGLGENRENDERBUFFERSPROC vw_glGenRenderbuffers;
PFNGLGENSAMPLERSPROC vw_glGenSamplers;
PFNGLGENVERTEXARRAYSPROC vw_glGenVertexArrays;
PFNGLGENERATEMIPMAPPROC vw_glGenerateMipmap;
PFNGLGETATTRIBLOCATIONPROC vw_glGetAttribLocation;
PFNGLGETBUFFERPARAMETERIVPROC vw_glGetBufferParameteriv;
PFNGLGETINTEGERI_VPROC vw_glGetIntegeri_v;
PFNGLGETPROGRAMINFOLOGPROC vw_glGetProgramInfoLog;
PFNGLGETPROGRAMIVPROC vw_glGetProgramiv;
PFNGLGETQUERYOBJECTIVPROC vw_glGetQueryObjectiv;
PFNGLGETQUERYOBJECTUI64VPROC vw_glGetQueryObjectui64v;
PFNGLGETSHADERINFOLOGPROC vw_glGetShaderInfoLog;
PFNGLGETSHADERIVPROC vw_glGetShaderiv;
PFNGLGETSTRINGIPROC vw_glGetStringi;
PFNGLGETUNIFORMLOCATIONPROC vw_glGetUniformLocation;
PFNGLGETVERTEXATTRIBPOINTERVPROC vw_glGetVertexAttribPointerv;
PFNGLGETVERTEXATTRIBIVPROC vw_glGetVertexAttribiv;
PFNGLISBUFFERPROC vw_glIsBuffer;
PFNGLISPROGRAMPROC vw_glIsProgram;
PFNGLISRENDERBUFFERPROC vw_glIsRenderbuffer;
PFNGLISVERTEXARRAYPROC vw_glIsVertexArray;
PFNGLLINKPROGRAMPROC vw_glLinkProgram;
PFNGLQUERYCOUNTERPROC vw_glQueryCounter;
PFNGLRENDERBUFFERSTORAGEPROC vw_glRenderbufferStorage;
PFNGLSAMPLERPARAMETERFPROC vw_glSamplerParameterf;
PFNGLSAMPLERPARAMETERIPROC vw_glSamplerParameteri;
PFNGLSHADERSOURCEPROC vw_glShaderSource;
PFNGLTEXBUFFERPROC vw_glTexBuffer;
PFNGLTEXIMAGE3DPROC vw_glTexImage3D;
PFNGLTEXSUBIMAGE3DPROC vw_glTexSubImage3D;
PFNGLUNIFORM1FPROC vw_glUniform1f;
PFNGLUNIFORM1FVPROC vw_glUniform1fv;
PFNGLUNIFORM1IPROC vw_glUniform1i;
PFNGLUNIFORM1IVPROC vw_glUniform1iv;
PFNGLUNIFORM2FPROC vw_glUniform2f;
PFNGLUNIFORM2FVPROC vw_glUniform2fv;
PFNGLUNIFORM2IPROC vw_glUniform2i;
PFNGLUNIFORM3FPROC vw_glUniform3f;
PFNGLUNIFORM3FVPROC vw_glUniform3fv;
PFNGLUNIFORM4FPROC vw_glUniform4f;
PFNGLUNIFORM4FVPROC vw_glUniform4fv;
PFNGLUNIFORM4IVPROC vw_glUniform4iv;
PFNGLUNIFORMMATRIX3FVPROC vw_glUniformMatrix3fv;
PFNGLUNIFORMMATRIX4FVPROC vw_glUniformMatrix4fv;
PFNGLUSEPROGRAMPROC vw_glUseProgram;
PFNGLVERTEXATTRIBDIVISORPROC vw_glVertexAttribDivisor;
PFNGLVERTEXATTRIBIPOINTERPROC vw_glVertexAttribIPointer;
PFNGLVERTEXATTRIBPOINTERPROC vw_glVertexAttribPointer;
int vw_windows_gl_init(void){
int ok=1;
vw_glActiveTexture=(PFNGLACTIVETEXTUREPROC)SDL_GL_GetProcAddress("glActiveTexture");
if(!vw_glActiveTexture){fprintf(stderr,"[Windows GL] Missing glActiveTexture\n");ok=0;}
vw_glAttachShader=(PFNGLATTACHSHADERPROC)SDL_GL_GetProcAddress("glAttachShader");
if(!vw_glAttachShader){fprintf(stderr,"[Windows GL] Missing glAttachShader\n");ok=0;}
vw_glBindBuffer=(PFNGLBINDBUFFERPROC)SDL_GL_GetProcAddress("glBindBuffer");
if(!vw_glBindBuffer){fprintf(stderr,"[Windows GL] Missing glBindBuffer\n");ok=0;}
vw_glBindFramebuffer=(PFNGLBINDFRAMEBUFFERPROC)SDL_GL_GetProcAddress("glBindFramebuffer");
if(!vw_glBindFramebuffer){fprintf(stderr,"[Windows GL] Missing glBindFramebuffer\n");ok=0;}
vw_glBindRenderbuffer=(PFNGLBINDRENDERBUFFERPROC)SDL_GL_GetProcAddress("glBindRenderbuffer");
if(!vw_glBindRenderbuffer){fprintf(stderr,"[Windows GL] Missing glBindRenderbuffer\n");ok=0;}
vw_glBindSampler=(PFNGLBINDSAMPLERPROC)SDL_GL_GetProcAddress("glBindSampler");
if(!vw_glBindSampler){fprintf(stderr,"[Windows GL] Missing glBindSampler\n");ok=0;}
vw_glBindVertexArray=(PFNGLBINDVERTEXARRAYPROC)SDL_GL_GetProcAddress("glBindVertexArray");
if(!vw_glBindVertexArray){fprintf(stderr,"[Windows GL] Missing glBindVertexArray\n");ok=0;}
vw_glBlendEquation=(PFNGLBLENDEQUATIONPROC)SDL_GL_GetProcAddress("glBlendEquation");
if(!vw_glBlendEquation){fprintf(stderr,"[Windows GL] Missing glBlendEquation\n");ok=0;}
vw_glBlendEquationSeparate=(PFNGLBLENDEQUATIONSEPARATEPROC)SDL_GL_GetProcAddress("glBlendEquationSeparate");
if(!vw_glBlendEquationSeparate){fprintf(stderr,"[Windows GL] Missing glBlendEquationSeparate\n");ok=0;}
vw_glBlendFuncSeparate=(PFNGLBLENDFUNCSEPARATEPROC)SDL_GL_GetProcAddress("glBlendFuncSeparate");
if(!vw_glBlendFuncSeparate){fprintf(stderr,"[Windows GL] Missing glBlendFuncSeparate\n");ok=0;}
vw_glBlitFramebuffer=(PFNGLBLITFRAMEBUFFERPROC)SDL_GL_GetProcAddress("glBlitFramebuffer");
if(!vw_glBlitFramebuffer){fprintf(stderr,"[Windows GL] Missing glBlitFramebuffer\n");ok=0;}
vw_glBufferData=(PFNGLBUFFERDATAPROC)SDL_GL_GetProcAddress("glBufferData");
if(!vw_glBufferData){fprintf(stderr,"[Windows GL] Missing glBufferData\n");ok=0;}
vw_glBufferSubData=(PFNGLBUFFERSUBDATAPROC)SDL_GL_GetProcAddress("glBufferSubData");
if(!vw_glBufferSubData){fprintf(stderr,"[Windows GL] Missing glBufferSubData\n");ok=0;}
vw_glCheckFramebufferStatus=(PFNGLCHECKFRAMEBUFFERSTATUSPROC)SDL_GL_GetProcAddress("glCheckFramebufferStatus");
if(!vw_glCheckFramebufferStatus){fprintf(stderr,"[Windows GL] Missing glCheckFramebufferStatus\n");ok=0;}
vw_glClearBufferfv=(PFNGLCLEARBUFFERFVPROC)SDL_GL_GetProcAddress("glClearBufferfv");
if(!vw_glClearBufferfv){fprintf(stderr,"[Windows GL] Missing glClearBufferfv\n");ok=0;}
vw_glClipControl=(PFNGLCLIPCONTROLPROC)SDL_GL_GetProcAddress("glClipControl");
vw_glCompileShader=(PFNGLCOMPILESHADERPROC)SDL_GL_GetProcAddress("glCompileShader");
if(!vw_glCompileShader){fprintf(stderr,"[Windows GL] Missing glCompileShader\n");ok=0;}
vw_glCompressedTexImage2D=(PFNGLCOMPRESSEDTEXIMAGE2DPROC)SDL_GL_GetProcAddress("glCompressedTexImage2D");
if(!vw_glCompressedTexImage2D){fprintf(stderr,"[Windows GL] Missing glCompressedTexImage2D\n");ok=0;}
vw_glCompressedTexSubImage2D=(PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC)SDL_GL_GetProcAddress("glCompressedTexSubImage2D");
if(!vw_glCompressedTexSubImage2D){fprintf(stderr,"[Windows GL] Missing glCompressedTexSubImage2D\n");ok=0;}
vw_glCreateProgram=(PFNGLCREATEPROGRAMPROC)SDL_GL_GetProcAddress("glCreateProgram");
if(!vw_glCreateProgram){fprintf(stderr,"[Windows GL] Missing glCreateProgram\n");ok=0;}
vw_glCreateShader=(PFNGLCREATESHADERPROC)SDL_GL_GetProcAddress("glCreateShader");
if(!vw_glCreateShader){fprintf(stderr,"[Windows GL] Missing glCreateShader\n");ok=0;}
vw_glDeleteBuffers=(PFNGLDELETEBUFFERSPROC)SDL_GL_GetProcAddress("glDeleteBuffers");
if(!vw_glDeleteBuffers){fprintf(stderr,"[Windows GL] Missing glDeleteBuffers\n");ok=0;}
vw_glDeleteFramebuffers=(PFNGLDELETEFRAMEBUFFERSPROC)SDL_GL_GetProcAddress("glDeleteFramebuffers");
if(!vw_glDeleteFramebuffers){fprintf(stderr,"[Windows GL] Missing glDeleteFramebuffers\n");ok=0;}
vw_glDeleteProgram=(PFNGLDELETEPROGRAMPROC)SDL_GL_GetProcAddress("glDeleteProgram");
if(!vw_glDeleteProgram){fprintf(stderr,"[Windows GL] Missing glDeleteProgram\n");ok=0;}
vw_glDeleteQueries=(PFNGLDELETEQUERIESPROC)SDL_GL_GetProcAddress("glDeleteQueries");
if(!vw_glDeleteQueries){fprintf(stderr,"[Windows GL] Missing glDeleteQueries\n");ok=0;}
vw_glDeleteRenderbuffers=(PFNGLDELETERENDERBUFFERSPROC)SDL_GL_GetProcAddress("glDeleteRenderbuffers");
if(!vw_glDeleteRenderbuffers){fprintf(stderr,"[Windows GL] Missing glDeleteRenderbuffers\n");ok=0;}
vw_glDeleteSamplers=(PFNGLDELETESAMPLERSPROC)SDL_GL_GetProcAddress("glDeleteSamplers");
if(!vw_glDeleteSamplers){fprintf(stderr,"[Windows GL] Missing glDeleteSamplers\n");ok=0;}
vw_glDeleteShader=(PFNGLDELETESHADERPROC)SDL_GL_GetProcAddress("glDeleteShader");
if(!vw_glDeleteShader){fprintf(stderr,"[Windows GL] Missing glDeleteShader\n");ok=0;}
vw_glDeleteVertexArrays=(PFNGLDELETEVERTEXARRAYSPROC)SDL_GL_GetProcAddress("glDeleteVertexArrays");
if(!vw_glDeleteVertexArrays){fprintf(stderr,"[Windows GL] Missing glDeleteVertexArrays\n");ok=0;}
vw_glDetachShader=(PFNGLDETACHSHADERPROC)SDL_GL_GetProcAddress("glDetachShader");
if(!vw_glDetachShader){fprintf(stderr,"[Windows GL] Missing glDetachShader\n");ok=0;}
vw_glDisableVertexAttribArray=(PFNGLDISABLEVERTEXATTRIBARRAYPROC)SDL_GL_GetProcAddress("glDisableVertexAttribArray");
if(!vw_glDisableVertexAttribArray){fprintf(stderr,"[Windows GL] Missing glDisableVertexAttribArray\n");ok=0;}
vw_glDrawArraysInstanced=(PFNGLDRAWARRAYSINSTANCEDPROC)SDL_GL_GetProcAddress("glDrawArraysInstanced");
if(!vw_glDrawArraysInstanced){fprintf(stderr,"[Windows GL] Missing glDrawArraysInstanced\n");ok=0;}
vw_glDrawBuffers=(PFNGLDRAWBUFFERSPROC)SDL_GL_GetProcAddress("glDrawBuffers");
if(!vw_glDrawBuffers){fprintf(stderr,"[Windows GL] Missing glDrawBuffers\n");ok=0;}
vw_glDrawElementsBaseVertex=(PFNGLDRAWELEMENTSBASEVERTEXPROC)SDL_GL_GetProcAddress("glDrawElementsBaseVertex");
if(!vw_glDrawElementsBaseVertex){fprintf(stderr,"[Windows GL] Missing glDrawElementsBaseVertex\n");ok=0;}
vw_glDrawElementsInstanced=(PFNGLDRAWELEMENTSINSTANCEDPROC)SDL_GL_GetProcAddress("glDrawElementsInstanced");
if(!vw_glDrawElementsInstanced){fprintf(stderr,"[Windows GL] Missing glDrawElementsInstanced\n");ok=0;}
vw_glEnableVertexAttribArray=(PFNGLENABLEVERTEXATTRIBARRAYPROC)SDL_GL_GetProcAddress("glEnableVertexAttribArray");
if(!vw_glEnableVertexAttribArray){fprintf(stderr,"[Windows GL] Missing glEnableVertexAttribArray\n");ok=0;}
vw_glFramebufferRenderbuffer=(PFNGLFRAMEBUFFERRENDERBUFFERPROC)SDL_GL_GetProcAddress("glFramebufferRenderbuffer");
if(!vw_glFramebufferRenderbuffer){fprintf(stderr,"[Windows GL] Missing glFramebufferRenderbuffer\n");ok=0;}
vw_glFramebufferTexture2D=(PFNGLFRAMEBUFFERTEXTURE2DPROC)SDL_GL_GetProcAddress("glFramebufferTexture2D");
if(!vw_glFramebufferTexture2D){fprintf(stderr,"[Windows GL] Missing glFramebufferTexture2D\n");ok=0;}
vw_glGenBuffers=(PFNGLGENBUFFERSPROC)SDL_GL_GetProcAddress("glGenBuffers");
if(!vw_glGenBuffers){fprintf(stderr,"[Windows GL] Missing glGenBuffers\n");ok=0;}
vw_glGenFramebuffers=(PFNGLGENFRAMEBUFFERSPROC)SDL_GL_GetProcAddress("glGenFramebuffers");
if(!vw_glGenFramebuffers){fprintf(stderr,"[Windows GL] Missing glGenFramebuffers\n");ok=0;}
vw_glGenQueries=(PFNGLGENQUERIESPROC)SDL_GL_GetProcAddress("glGenQueries");
if(!vw_glGenQueries){fprintf(stderr,"[Windows GL] Missing glGenQueries\n");ok=0;}
vw_glGenRenderbuffers=(PFNGLGENRENDERBUFFERSPROC)SDL_GL_GetProcAddress("glGenRenderbuffers");
if(!vw_glGenRenderbuffers){fprintf(stderr,"[Windows GL] Missing glGenRenderbuffers\n");ok=0;}
vw_glGenSamplers=(PFNGLGENSAMPLERSPROC)SDL_GL_GetProcAddress("glGenSamplers");
if(!vw_glGenSamplers){fprintf(stderr,"[Windows GL] Missing glGenSamplers\n");ok=0;}
vw_glGenVertexArrays=(PFNGLGENVERTEXARRAYSPROC)SDL_GL_GetProcAddress("glGenVertexArrays");
if(!vw_glGenVertexArrays){fprintf(stderr,"[Windows GL] Missing glGenVertexArrays\n");ok=0;}
vw_glGenerateMipmap=(PFNGLGENERATEMIPMAPPROC)SDL_GL_GetProcAddress("glGenerateMipmap");
if(!vw_glGenerateMipmap){fprintf(stderr,"[Windows GL] Missing glGenerateMipmap\n");ok=0;}
vw_glGetAttribLocation=(PFNGLGETATTRIBLOCATIONPROC)SDL_GL_GetProcAddress("glGetAttribLocation");
if(!vw_glGetAttribLocation){fprintf(stderr,"[Windows GL] Missing glGetAttribLocation\n");ok=0;}
vw_glGetBufferParameteriv=(PFNGLGETBUFFERPARAMETERIVPROC)SDL_GL_GetProcAddress("glGetBufferParameteriv");
if(!vw_glGetBufferParameteriv){fprintf(stderr,"[Windows GL] Missing glGetBufferParameteriv\n");ok=0;}
vw_glGetIntegeri_v=(PFNGLGETINTEGERI_VPROC)SDL_GL_GetProcAddress("glGetIntegeri_v");
if(!vw_glGetIntegeri_v){fprintf(stderr,"[Windows GL] Missing glGetIntegeri_v\n");ok=0;}
vw_glGetProgramInfoLog=(PFNGLGETPROGRAMINFOLOGPROC)SDL_GL_GetProcAddress("glGetProgramInfoLog");
if(!vw_glGetProgramInfoLog){fprintf(stderr,"[Windows GL] Missing glGetProgramInfoLog\n");ok=0;}
vw_glGetProgramiv=(PFNGLGETPROGRAMIVPROC)SDL_GL_GetProcAddress("glGetProgramiv");
if(!vw_glGetProgramiv){fprintf(stderr,"[Windows GL] Missing glGetProgramiv\n");ok=0;}
vw_glGetQueryObjectiv=(PFNGLGETQUERYOBJECTIVPROC)SDL_GL_GetProcAddress("glGetQueryObjectiv");
if(!vw_glGetQueryObjectiv){fprintf(stderr,"[Windows GL] Missing glGetQueryObjectiv\n");ok=0;}
vw_glGetQueryObjectui64v=(PFNGLGETQUERYOBJECTUI64VPROC)SDL_GL_GetProcAddress("glGetQueryObjectui64v");
if(!vw_glGetQueryObjectui64v){fprintf(stderr,"[Windows GL] Missing glGetQueryObjectui64v\n");ok=0;}
vw_glGetShaderInfoLog=(PFNGLGETSHADERINFOLOGPROC)SDL_GL_GetProcAddress("glGetShaderInfoLog");
if(!vw_glGetShaderInfoLog){fprintf(stderr,"[Windows GL] Missing glGetShaderInfoLog\n");ok=0;}
vw_glGetShaderiv=(PFNGLGETSHADERIVPROC)SDL_GL_GetProcAddress("glGetShaderiv");
if(!vw_glGetShaderiv){fprintf(stderr,"[Windows GL] Missing glGetShaderiv\n");ok=0;}
vw_glGetStringi=(PFNGLGETSTRINGIPROC)SDL_GL_GetProcAddress("glGetStringi");
if(!vw_glGetStringi){fprintf(stderr,"[Windows GL] Missing glGetStringi\n");ok=0;}
vw_glGetUniformLocation=(PFNGLGETUNIFORMLOCATIONPROC)SDL_GL_GetProcAddress("glGetUniformLocation");
if(!vw_glGetUniformLocation){fprintf(stderr,"[Windows GL] Missing glGetUniformLocation\n");ok=0;}
vw_glGetVertexAttribPointerv=(PFNGLGETVERTEXATTRIBPOINTERVPROC)SDL_GL_GetProcAddress("glGetVertexAttribPointerv");
if(!vw_glGetVertexAttribPointerv){fprintf(stderr,"[Windows GL] Missing glGetVertexAttribPointerv\n");ok=0;}
vw_glGetVertexAttribiv=(PFNGLGETVERTEXATTRIBIVPROC)SDL_GL_GetProcAddress("glGetVertexAttribiv");
if(!vw_glGetVertexAttribiv){fprintf(stderr,"[Windows GL] Missing glGetVertexAttribiv\n");ok=0;}
vw_glIsBuffer=(PFNGLISBUFFERPROC)SDL_GL_GetProcAddress("glIsBuffer");
if(!vw_glIsBuffer){fprintf(stderr,"[Windows GL] Missing glIsBuffer\n");ok=0;}
vw_glIsProgram=(PFNGLISPROGRAMPROC)SDL_GL_GetProcAddress("glIsProgram");
if(!vw_glIsProgram){fprintf(stderr,"[Windows GL] Missing glIsProgram\n");ok=0;}
vw_glIsRenderbuffer=(PFNGLISRENDERBUFFERPROC)SDL_GL_GetProcAddress("glIsRenderbuffer");
if(!vw_glIsRenderbuffer){fprintf(stderr,"[Windows GL] Missing glIsRenderbuffer\n");ok=0;}
vw_glIsVertexArray=(PFNGLISVERTEXARRAYPROC)SDL_GL_GetProcAddress("glIsVertexArray");
if(!vw_glIsVertexArray){fprintf(stderr,"[Windows GL] Missing glIsVertexArray\n");ok=0;}
vw_glLinkProgram=(PFNGLLINKPROGRAMPROC)SDL_GL_GetProcAddress("glLinkProgram");
if(!vw_glLinkProgram){fprintf(stderr,"[Windows GL] Missing glLinkProgram\n");ok=0;}
vw_glQueryCounter=(PFNGLQUERYCOUNTERPROC)SDL_GL_GetProcAddress("glQueryCounter");
if(!vw_glQueryCounter){fprintf(stderr,"[Windows GL] Missing glQueryCounter\n");ok=0;}
vw_glRenderbufferStorage=(PFNGLRENDERBUFFERSTORAGEPROC)SDL_GL_GetProcAddress("glRenderbufferStorage");
if(!vw_glRenderbufferStorage){fprintf(stderr,"[Windows GL] Missing glRenderbufferStorage\n");ok=0;}
vw_glSamplerParameterf=(PFNGLSAMPLERPARAMETERFPROC)SDL_GL_GetProcAddress("glSamplerParameterf");
if(!vw_glSamplerParameterf){fprintf(stderr,"[Windows GL] Missing glSamplerParameterf\n");ok=0;}
vw_glSamplerParameteri=(PFNGLSAMPLERPARAMETERIPROC)SDL_GL_GetProcAddress("glSamplerParameteri");
if(!vw_glSamplerParameteri){fprintf(stderr,"[Windows GL] Missing glSamplerParameteri\n");ok=0;}
vw_glShaderSource=(PFNGLSHADERSOURCEPROC)SDL_GL_GetProcAddress("glShaderSource");
if(!vw_glShaderSource){fprintf(stderr,"[Windows GL] Missing glShaderSource\n");ok=0;}
vw_glTexBuffer=(PFNGLTEXBUFFERPROC)SDL_GL_GetProcAddress("glTexBuffer");
if(!vw_glTexBuffer){fprintf(stderr,"[Windows GL] Missing glTexBuffer\n");ok=0;}
vw_glTexImage3D=(PFNGLTEXIMAGE3DPROC)SDL_GL_GetProcAddress("glTexImage3D");
if(!vw_glTexImage3D){fprintf(stderr,"[Windows GL] Missing glTexImage3D\n");ok=0;}
vw_glTexSubImage3D=(PFNGLTEXSUBIMAGE3DPROC)SDL_GL_GetProcAddress("glTexSubImage3D");
if(!vw_glTexSubImage3D){fprintf(stderr,"[Windows GL] Missing glTexSubImage3D\n");ok=0;}
vw_glUniform1f=(PFNGLUNIFORM1FPROC)SDL_GL_GetProcAddress("glUniform1f");
if(!vw_glUniform1f){fprintf(stderr,"[Windows GL] Missing glUniform1f\n");ok=0;}
vw_glUniform1fv=(PFNGLUNIFORM1FVPROC)SDL_GL_GetProcAddress("glUniform1fv");
if(!vw_glUniform1fv){fprintf(stderr,"[Windows GL] Missing glUniform1fv\n");ok=0;}
vw_glUniform1i=(PFNGLUNIFORM1IPROC)SDL_GL_GetProcAddress("glUniform1i");
if(!vw_glUniform1i){fprintf(stderr,"[Windows GL] Missing glUniform1i\n");ok=0;}
vw_glUniform1iv=(PFNGLUNIFORM1IVPROC)SDL_GL_GetProcAddress("glUniform1iv");
if(!vw_glUniform1iv){fprintf(stderr,"[Windows GL] Missing glUniform1iv\n");ok=0;}
vw_glUniform2f=(PFNGLUNIFORM2FPROC)SDL_GL_GetProcAddress("glUniform2f");
if(!vw_glUniform2f){fprintf(stderr,"[Windows GL] Missing glUniform2f\n");ok=0;}
vw_glUniform2fv=(PFNGLUNIFORM2FVPROC)SDL_GL_GetProcAddress("glUniform2fv");
if(!vw_glUniform2fv){fprintf(stderr,"[Windows GL] Missing glUniform2fv\n");ok=0;}
vw_glUniform2i=(PFNGLUNIFORM2IPROC)SDL_GL_GetProcAddress("glUniform2i");
if(!vw_glUniform2i){fprintf(stderr,"[Windows GL] Missing glUniform2i\n");ok=0;}
vw_glUniform3f=(PFNGLUNIFORM3FPROC)SDL_GL_GetProcAddress("glUniform3f");
if(!vw_glUniform3f){fprintf(stderr,"[Windows GL] Missing glUniform3f\n");ok=0;}
vw_glUniform3fv=(PFNGLUNIFORM3FVPROC)SDL_GL_GetProcAddress("glUniform3fv");
if(!vw_glUniform3fv){fprintf(stderr,"[Windows GL] Missing glUniform3fv\n");ok=0;}
vw_glUniform4f=(PFNGLUNIFORM4FPROC)SDL_GL_GetProcAddress("glUniform4f");
if(!vw_glUniform4f){fprintf(stderr,"[Windows GL] Missing glUniform4f\n");ok=0;}
vw_glUniform4fv=(PFNGLUNIFORM4FVPROC)SDL_GL_GetProcAddress("glUniform4fv");
if(!vw_glUniform4fv){fprintf(stderr,"[Windows GL] Missing glUniform4fv\n");ok=0;}
vw_glUniform4iv=(PFNGLUNIFORM4IVPROC)SDL_GL_GetProcAddress("glUniform4iv");
if(!vw_glUniform4iv){fprintf(stderr,"[Windows GL] Missing glUniform4iv\n");ok=0;}
vw_glUniformMatrix3fv=(PFNGLUNIFORMMATRIX3FVPROC)SDL_GL_GetProcAddress("glUniformMatrix3fv");
if(!vw_glUniformMatrix3fv){fprintf(stderr,"[Windows GL] Missing glUniformMatrix3fv\n");ok=0;}
vw_glUniformMatrix4fv=(PFNGLUNIFORMMATRIX4FVPROC)SDL_GL_GetProcAddress("glUniformMatrix4fv");
if(!vw_glUniformMatrix4fv){fprintf(stderr,"[Windows GL] Missing glUniformMatrix4fv\n");ok=0;}
vw_glUseProgram=(PFNGLUSEPROGRAMPROC)SDL_GL_GetProcAddress("glUseProgram");
if(!vw_glUseProgram){fprintf(stderr,"[Windows GL] Missing glUseProgram\n");ok=0;}
vw_glVertexAttribDivisor=(PFNGLVERTEXATTRIBDIVISORPROC)SDL_GL_GetProcAddress("glVertexAttribDivisor");
if(!vw_glVertexAttribDivisor){fprintf(stderr,"[Windows GL] Missing glVertexAttribDivisor\n");ok=0;}
vw_glVertexAttribIPointer=(PFNGLVERTEXATTRIBIPOINTERPROC)SDL_GL_GetProcAddress("glVertexAttribIPointer");
if(!vw_glVertexAttribIPointer){fprintf(stderr,"[Windows GL] Missing glVertexAttribIPointer\n");ok=0;}
vw_glVertexAttribPointer=(PFNGLVERTEXATTRIBPOINTERPROC)SDL_GL_GetProcAddress("glVertexAttribPointer");
if(!vw_glVertexAttribPointer){fprintf(stderr,"[Windows GL] Missing glVertexAttribPointer\n");ok=0;}
return ok;
}

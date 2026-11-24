#!/usr/bin/env python3
"""Build the custom Valwood editor for Windows x64 using project-local SDKs."""
import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def write_changed(path, text):
    if not path.exists() or path.read_text() != text:
        path.write_text(text)


def generate_loader():
    sdk = ROOT/'build/windows/sdl/SDL2-2.32.10/x86_64-w64-mingw32/include/SDL2'
    gl11 = (ROOT/'build/windows/toolchain/usr/x86_64-w64-mingw32/include/GL/gl.h').read_text()
    types = (sdk/'SDL_opengl_glext.h').read_text()
    names = set()
    for p in [ROOT/'main.c', ROOT/'gl_helpers.h', *list((ROOT/'engine').glob('*')), ROOT/'third_party/imgui/backends/imgui_impl_opengl3.cpp']:
        if p.is_file() and p.suffix in ('.c','.cpp','.h','.inc'):
            names.update(re.findall(r'\b(gl[A-Z]\w*)\s*\(',p.read_text(errors='replace')))
    core = set(re.findall(r'\b(gl[A-Z]\w*)\s*\(',gl11))
    names = sorted(n for n in names-core if 'PFN'+n.upper()+'PROC' in types)
    header = ['/* Generated from actual engine calls by build_windows_editor.py. */',
        '#ifndef VALWOOD_WINDOWS_GL_H', '#define VALWOOD_WINDOWS_GL_H',
        '#ifdef GL_GLEXT_PROTOTYPES', '#undef GL_GLEXT_PROTOTYPES', '#endif',
        '#include <SDL2/SDL_opengl.h>', '#include <SDL2/SDL_opengl_glext.h>',
        '#ifdef __cplusplus', 'extern "C" {', '#endif', 'int vw_windows_gl_init(void);']
    source = ['#include "platform/windows/windows_gl.h"', '#include <SDL2/SDL.h>', '#include <stdio.h>']
    for name in names:
        kind = 'PFN'+name.upper()+'PROC'
        header += [f'extern {kind} vw_{name};',f'#define {name} vw_{name}']
        source += [f'{kind} vw_{name};']
    header += ['#ifdef __cplusplus','}','#endif','#endif','']
    source += ['int vw_windows_gl_init(void){','int ok=1;']
    for name in names:
        source += [f'vw_{name}=({"PFN"+name.upper()+"PROC"})SDL_GL_GetProcAddress("{name}");']
        if name != 'glClipControl':
            source += [f'if(!vw_{name}){{fprintf(stderr,"[Windows GL] Missing {name}\\n");ok=0;}}']
    source += ['return ok;','}','']
    write_changed(ROOT/'platform/windows/windows_gl.h', '\n'.join(header))
    write_changed(ROOT/'platform/windows/windows_gl.c', '\n'.join(source))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jobs',type=int,default=3)
    parser.add_argument('--configure-only',action='store_true')
    args=parser.parse_args()
    if not 1<=args.jobs<=8:parser.error('jobs must be 1..8')
    generate_loader()
    sources=subprocess.check_output(['make','-s','--eval=print-windows-sources: ; @echo $(SRCS) $(CPPSRCS)','print-windows-sources'],cwd=ROOT,text=True).split()
    generated=ROOT/'build/windows/sources.cmake'
    write_changed(generated, 'set(VALWOOD_SOURCES\n'+''.join('  "${VALWOOD_ROOT}/'+s+'"\n' for s in sources)+')\n')
    root=ROOT/'build/windows/toolchain/usr'
    toolchain=ROOT/'build/windows/toolchain.cmake'
    write_changed(toolchain, '\n'.join([
        'set(CMAKE_SYSTEM_NAME Windows)', 'set(CMAKE_SYSTEM_PROCESSOR AMD64)',
        f'set(CMAKE_C_COMPILER "{root}/bin/x86_64-w64-mingw32-gcc-posix")',
        f'set(CMAKE_CXX_COMPILER "{root}/bin/x86_64-w64-mingw32-g++-posix")',
        f'set(CMAKE_RC_COMPILER "{root}/bin/x86_64-w64-mingw32-windres")',
        f'set(CMAKE_FIND_ROOT_PATH "{root}/x86_64-w64-mingw32")',
        'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)',
        'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)',
        'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)', '']))
    output=ROOT/'build/windows/editor'
    subprocess.run(['cmake','-S',str(ROOT/'platform/windows'),'-B',str(output),
        '-DCMAKE_TOOLCHAIN_FILE='+str(toolchain),'-DCMAKE_BUILD_TYPE=Release'],check=True)
    if not args.configure_only:
        subprocess.run(['cmake','--build',str(output),'--parallel',str(args.jobs)],check=True)


if __name__=='__main__':main()

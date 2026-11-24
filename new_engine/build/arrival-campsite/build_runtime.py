#!/usr/bin/env python3
"""Build isolated arrival-campsite runtimes from the accepted opening link recipes."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
BUILD = Path(__file__).resolve().parent
ACCEPTED = ROOT / 'build/opening-finward-reference'
SOURCE = BUILD / 'runtime-source'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')


def run(command, cwd, log):
    begin = time.monotonic()
    with log.open('w') as handle:
        subprocess.run(command, cwd=cwd, stdout=handle, stderr=subprocess.STDOUT, check=True)
    return dict(command=command, cwd=str(cwd), log=str(log), seconds=time.monotonic()-begin)


def compile_command(original, file, output, dependency):
    command = [arg.replace(str(ACCEPTED/'runtime-source'), str(SOURCE)) for arg in original]
    # The accepted Windows recipe may already reference its QA-neutral copy.
    command = [arg.replace(str(ACCEPTED/'windows-qa-neutral-recovery-v2/runtime-source'), str(SOURCE)) for arg in command]
    command[command.index('-c')+1] = str(SOURCE/'engine'/file)
    command[command.index('-o')+1] = str(output)
    command[command.index('-MF')+1] = str(dependency)
    return command


def main():
    for name in ['world_opening_arrival_camp.h', 'world_opening_prologue.h',
                 'world_opening_prologue_runtime.inc', 'world_trail_camp.h',
                 'world_trail_camp.cpp', 'world_viewer.cpp']:
        assert (SOURCE/'engine'/name).is_file(), name
    assert not (BUILD/'BUILD_REVIEW.json').exists(), 'Frozen build already exists; use another isolated directory.'
    native_base = next(row['command'] for row in json.loads((ACCEPTED/'runtime-compile.json').read_text())['results'] if row['file']=='world_viewer.cpp')
    windows_owner = json.loads((ACCEPTED/'windows-qa-neutral-recovery-v2/BUILD_REVIEW.json').read_text())
    native_link = json.loads((ACCEPTED/'runtime-link.json').read_text())
    windows_link = list(windows_owner['link_command'])
    native_viewer = str(ACCEPTED/'runtime-objects/world_viewer.cpp.o')
    windows_viewer = str(ACCEPTED/'windows-qa-neutral-recovery-v2/objects/world_viewer.cpp.obj')
    native_trail = 'engine/world_trail_camp.o'
    windows_trail = str(ROOT/'build/windows/editor/CMakeFiles/rpg.dir'/str(ROOT/'engine/world_trail_camp.cpp').lstrip('/'))+'.obj'
    assert native_link.count(native_viewer)==native_link.count(native_trail)==1
    assert windows_link.count(windows_viewer)==windows_link.count(windows_trail)==1, windows_trail
    accepted = [ACCEPTED/'bin/rpg', ACCEPTED/'bin/rpg.exe',
                ACCEPTED/'windows-qa-neutral-recovery-v2/bin/rpg.exe', ACCEPTED/'RUNTIME_REVIEW.json']
    frozen = {str(p):sha(p) for p in accepted}
    for links, cwd in [(native_link,ROOT),(windows_link,Path(windows_owner['link_cwd']))]:
        for arg in links:
            if arg.endswith(('.o','.obj','.res','.a','.rsp')):
                path = Path(arg) if Path(arg).is_absolute() else cwd/arg
                if path.is_file(): frozen[str(path)] = sha(path)
    source_files = {str(p.relative_to(ROOT)):sha(p) for p in sorted((SOURCE/'engine').iterdir()) if p.is_file()}
    record = dict(schema='valwood.arrival-campsite.runtime', version=1, compiled=False,
                  native_gpu_validation=False, wine_validation=False,
                  source_files=source_files, accepted_inputs_sha256=frozen,
                  accepted_inputs_unchanged=False,
                  contract={'optional_settings':'opening/arrival-camp.settings','actors':3,
                            'poses':12,'three_render_passes':True,'walk_assets_loaded':False,
                            'qa_neutral_guard':'same accepted V2 actual-walk-only SDL guard on both platforms',
                            'existing_jungle_camp':'unchanged default behavior',
                            'fire_audio':'existing source flame emitter drives positional fire ambience'},
                  scope='Private candidate only. Actual native/Wine story, interaction, collision and image acceptance remain root-owned.')
    write(BUILD/'BUILD_REVIEW.json', record)

    def platform(name, base, links, cwd, old_viewer, old_trail, suffix, binary):
        objdir = BUILD/f'{name}-objects'; objdir.mkdir(exist_ok=True)
        result=[]
        for file in ['world_viewer.cpp','world_trail_camp.cpp']:
            command=compile_command(base,file,objdir/(file+suffix),objdir/(file+'.d'))
            result.append(run(command,ROOT,objdir/(file+'.log')))
        links=list(links)
        links[links.index(old_viewer)]=str(objdir/('world_viewer.cpp'+suffix))
        links[links.index(old_trail)]=str(objdir/('world_trail_camp.cpp'+suffix))
        links[links.index('-o')+1]=str(BUILD/'bin'/binary)
        return dict(compile=result,link=run(links,cwd,objdir/'link.log'))

    with ThreadPoolExecutor(max_workers=2) as pool:
        native=pool.submit(platform,'native',native_base,native_link,ROOT,native_viewer,native_trail,'.o','rpg')
        windows=pool.submit(platform,'windows',windows_owner['compile_command'],windows_link,
                            Path(windows_owner['link_cwd']),windows_viewer,windows_trail,'.obj','rpg.exe')
        record['native']=native.result();record['windows']=windows.result()
    for name in ['SDL2.dll','SDL2_mixer.dll','SDL2_ttf.dll','libwinpthread-1.dll']:
        shutil.copyfile(ACCEPTED/'bin'/name,BUILD/'bin'/name)
    for path,digest in frozen.items(): assert sha(path)==digest,path
    assert all(sha(ROOT/path)==digest for path,digest in source_files.items())
    record.update(compiled=True,accepted_inputs_unchanged=True,
                  binaries={str(p.relative_to(ROOT)):dict(sha256=sha(p),bytes=p.stat().st_size)
                            for p in sorted((BUILD/'bin').iterdir()) if p.is_file()})
    write(BUILD/'BUILD_REVIEW.json',record)
    print(json.dumps(dict(compiled=True,review=str(BUILD/'BUILD_REVIEW.json'),
                          review_sha256=sha(BUILD/'BUILD_REVIEW.json'),binaries=record['binaries']),indent=2),flush=True)


if __name__=='__main__':
    main()

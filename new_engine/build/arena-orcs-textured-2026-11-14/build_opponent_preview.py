#!/usr/bin/env python3
"""Build the isolated production-opponent animation renderer; native Linux only."""
from pathlib import Path
import hashlib
import importlib.util
import json
import shlex
import subprocess
import sys

sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
REVIEW=HERE/'opponent-render-review'
def sha(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def main():
    REVIEW.mkdir(exist_ok=True)
    spec=importlib.util.spec_from_file_location('opponent_builder',HERE/'build_native_candidate.py')
    builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
    plan=builder.plan()
    units=['engine/world_models.cpp','engine/world_combat_motion.cpp','engine/world_training_combat.cpp',
           'engine/world_animation.cpp','engine/world_animation_render.cpp','engine/texture_cache.c']
    rows={row['logical_source']:row for row in plan['units']}
    for name in units:builder.compile_one(rows[name])
    flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','sdl2','libpng'],text=True))
    source=HERE/'preview_opponents.cpp';binary=REVIEW/'preview_opponents'
    command=[builder.CXX,'-std=c++17','-O2','-I'+str(HERE/'source-v1'),*flags,str(source),
             *[rows[name]['target'] for name in units],'-o',str(binary),'-lSDL2','-lGL','-lpng','-pthread','-lm']
    before={str(source):sha(source)}
    for name in units:
        row=rows[name];record=json.loads(Path(row['target']).with_suffix('.json').read_text())
        for path,expected in record['actual_dependency_sha256'].items():
            if sha(path)!=expected:raise RuntimeError('Stale preview object dependency: '+path)
        before[row['target']]=sha(row['target'])
    with (REVIEW/'preview-build.log').open('w') as handle:
        subprocess.run(command,stdout=handle,stderr=subprocess.STDOUT,check=True)
    if before!={path:sha(path) for path in before}:raise RuntimeError('Preview inputs changed while building')
    result=dict(schema='valwood.opponent-animation-preview.build.v1',compiled=True,binary=str(binary),
                binary_sha256=sha(binary),inputs=before,command=command,native_only=True,
                actual_orc_asset_acceptance=False,scripted_animation_fixture=True)
    (REVIEW/'PREVIEW_BUILD.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'compiled':True,'binary':str(binary),'sha256':result['binary_sha256']}))
if __name__=='__main__':main()

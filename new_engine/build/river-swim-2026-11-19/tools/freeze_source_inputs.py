#!/usr/bin/env python3
"""Stage the sole owned shader derivative after the root's explicit source freeze.

This edits only this unpublished candidate. It does not build, publish or touch LIVE.
"""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import os
import shutil
import sys
import time

sys.dont_write_bytecode=True
N=Path(__file__).resolve().parents[1]
B=N.with_name('scenery-forest-2026-11-19')

def need(ok,message):
    if not ok:raise RuntimeError(message)

def sha(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def inventory(root):
    return {str(p.relative_to(root)):sha(p) for p in sorted(root.rglob('*'))
            if p.is_file() and '__pycache__' not in p.parts and p.suffix not in ('.pyc','.pyo')}

def write(path,value):
    temporary=path.with_suffix(path.suffix+'.freeze-'+str(os.getpid())+'.tmp')
    need(not temporary.exists(),'Unexpected temporary '+str(temporary))
    try:
        temporary.write_text(json.dumps(value,indent=2)+'\n');os.replace(temporary,path)
    finally:temporary.unlink(missing_ok=True)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--root-source-frozen',action='store_true',required=True)
    a=ap.parse_args()
    need(a.root_source_frozen,'Root must explicitly authorize the frozen source')
    need(not (N/'UNIFIED_READY.json').exists() and not (N/'PUBLICATION.json').exists(),'Published generation is immutable')
    spec=importlib.util.spec_from_file_location('river_swim_freeze_checker',N/'tools/check_unified.py')
    c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)
    need(c.APPROVED_BINARY_SHA is None,'Do not change already approved release inputs')
    need(not (N/'source-v1').is_symlink() and not (N/'shared/shaders').is_symlink(),'Source and shader bank must be owned')
    source,baseline=inventory(N/'source-v1'),inventory(B/'source-v1')
    need(set(source)==set(baseline),'Source inventory differs from the protected baseline')
    changed={p for p in source if source[p]!=baseline[p]}
    need(changed==c.EXPECTED_SOURCE_CHANGES,'Unexpected source changes '+repr(sorted(changed)))
    need(baseline==json.loads((N/'SOURCE_ORIGIN.json').read_text())['baseline_sha256'],'Original source inventory changed')
    shaders,base_shaders=inventory(N/'shared/shaders'),inventory(B/'shared/shaders')
    need(set(shaders)==set(base_shaders),'Shader inventory changed')
    need({p for p in shaders if shaders[p]!=base_shaders[p]}<=c.EXPECTED_SHADER_CHANGES,'An unrelated shared shader changed')
    for p in c.EXPECTED_SHADER_CHANGES:
        target=N/'shared/shaders'/p
        need(target.is_file() and not target.is_symlink() and not (N/'source-v1/shaders'/p).is_symlink(),'Water fragment must be owned')
        need(base_shaders[p]==baseline['shaders/'+p],'Baseline shared/source shader mismatch')
    archive=N/'review'/('before-water-render-fix-'+str(time.time_ns()))
    archive.mkdir()
    for rel in ('review/SOURCE_CHANGE_SCOPE.json','review/SHADER_CHANGE_SCOPE.json','review/NATIVE_BUILD_REVIEW.json',
                'review/RIVER_SWIM_CPU_REVIEW.json','runtime/ORIGIN_BUILD.json','runtime-native-v1/READY.json',
                'runtime-native-v1/BUILD_PLAN.json','CACHE_SEED_REVIEW.json','runtime/rpg','shared/shaders/world_water.fs.glsl'):
        p=N/rel
        if p.is_file():
            target=archive/rel;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target)
    for p in c.EXPECTED_SHADER_CHANGES:
        target=N/'shared/shaders'/p;temporary=target.with_suffix(target.suffix+'.freeze-'+str(os.getpid())+'.tmp')
        need(not temporary.exists(),'Unexpected shader temporary')
        try:shutil.copy2(N/'source-v1/shaders'/p,temporary);os.replace(temporary,target)
        finally:temporary.unlink(missing_ok=True)
    need(inventory(N/'source-v1')==source,'Source changed during final freeze')
    source_scope=dict(schema='valwood.river-swim.source-change-scope.v1',reviewed=True,root_freeze_authorized=True,
                      source_changes=sorted(changed),recompiled_units=['engine/world_viewer.cpp'],
                      baseline_sha256={p:baseline[p] for p in sorted(changed)},
                      candidate_sha256={p:source[p] for p in sorted(changed)})
    shader_scope=dict(schema='valwood.river-swim.shader-change-scope.v1',reviewed=True,root_freeze_authorized=True,
                      shader_changes=sorted(c.EXPECTED_SHADER_CHANGES),
                      baseline_sha256={p:base_shaders[p] for p in sorted(c.EXPECTED_SHADER_CHANGES)},
                      candidate_sha256={p:source['shaders/'+p] for p in sorted(c.EXPECTED_SHADER_CHANGES)},
                      source_derivative=True,shared_bank_owned=True)
    write(N/'review/SOURCE_CHANGE_SCOPE.json',source_scope);write(N/'review/SHADER_CHANGE_SCOPE.json',shader_scope)
    c.source_scope();c.shader_scope();c.scene_scope()
    report=dict(schema='valwood.river-swim.frozen-source-inputs.v1',passed=True,source_changes=sorted(changed),
                shared_shader_changes=shader_scope['shader_changes'],intermediate_build_archive=str(archive),
                source_scope_sha256=sha(N/'review/SOURCE_CHANGE_SCOPE.json'),
                shader_scope_sha256=sha(N/'review/SHADER_CHANGE_SCOPE.json'),
                native_build_invoked=False,gpu_used=False,LIVE_changed=False,release_approved=False)
    write(N/'review/FROZEN_SOURCE_INPUTS.json',report);print(json.dumps(report,indent=2))

if __name__=='__main__':main()

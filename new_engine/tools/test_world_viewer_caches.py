#!/usr/bin/env python3
"""Compile the real viewer cache structs against deterministic CPU-only stubs."""
from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
source=(ROOT/'engine/world_viewer.cpp').read_text()
def extract(name):
 start=source.index('    struct '+name+' {');depth=0
 for index in range(source.index('{',start),len(source)):
  depth+=(source[index]=='{')-(source[index]=='}')
  if depth==0:return source[start:index+2]
 raise AssertionError(name)
terrain=extract('TerrainGPU').replace('struct TerrainGPU {','struct TerrainGPU {\nsize_t hierarchy_visits=0,hierarchy_builds=0;').replace('void visit(int index) {','void visit(int index) { ++hierarchy_visits;').replace('draws.clear();','++hierarchy_builds;draws.clear();')
legacy=terrain.replace('struct TerrainGPU {','struct LegacyTerrainGPU {').replace('if(!hierarchy_dirty)return;','(void)hierarchy_dirty;').replace('if(hierarchy_dirty)desired.assign','desired.assign')
with tempfile.TemporaryDirectory(prefix='valwood-viewer-caches-') as directory:
 p=Path(directory);(p/'viewer_cache_classes.inc').write_text(terrain+'\n'+legacy+'\n'+extract('HorizonForest')+'\n')
 command=['c++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-I'+str(ROOT),'-I'+str(p),str(ROOT/'tests/world_viewer_caches_test.cpp'),'-o',str(p/'test')]
 subprocess.run(command,check=True);subprocess.run([str(p/'test')],check=True)

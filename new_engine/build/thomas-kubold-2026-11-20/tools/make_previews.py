"""Build bounded inspection assets using the same body and captured motion."""
from pathlib import Path
import copy,importlib.util,sys
N=Path(__file__).resolve().parents[1];E=N.parents[1]
sys.path.insert(0,str(E/'tools'))
import finalize_modular_knight_glb as glb
spec=importlib.util.spec_from_file_location('compact_body',E/'build/plain-father-2026-11-18/tools/build_story_hero.py')
compact=importlib.util.module_from_spec(spec);spec.loader.exec_module(compact)
for outfit in ['civilian','armored']:
 d,b,_=glb.load(N/f'assets/story_hero/{outfit}/body.glb');source,sb,_=glb.load(N/'motion/rigs/sword.glb')
 vo=len(d['bufferViews']);ao=len(d['accessors']);b.extend(b'\0'*(-len(b)%4));offset=len(b);b.extend(sb)
 for view in source['bufferViews']:
  v=copy.deepcopy(view);v['byteOffset']=v.get('byteOffset',0)+offset;d['bufferViews'].append(v)
 for accessor in source['accessors']:
  a=copy.deepcopy(accessor);a['bufferView']+=vo;d['accessors'].append(a)
 d['animations']=[]
 for clip in source['animations']:
  if clip['name'] not in ['Story_UnarmedIdle','Story_UnarmedWalkFull','Story_UnarmedRunFull']:continue
  a=copy.deepcopy(clip)
  for sampler in a['samplers']:sampler['input']+=ao;sampler['output']+=ao
  d['animations'].append(a)
 d,b=compact.compact(d,b)
 output=N/'review'/('thomas-preview.glb' if outfit=='civilian' else 'armored-preview.glb')
 output.write_bytes(glb.encode(d,b));print(output,output.stat().st_size)

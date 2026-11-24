#!/usr/bin/env python3
"""Read purchased TerrainData's exact alpha-layer order without loading Unity."""
from pathlib import Path
import sys,json,re,shutil
ROOT=Path(__file__).resolve().parents[1];PRIVATE=ROOT.parent/'.local-tools/unity-source';DATA=ROOT/'assets/purchased/nature_castle'
sys.path.insert(0,str(PRIVATE/'pythondeps'))
import UnityPy,yaml
inv=json.loads((DATA/'INVENTORY.json').read_text());by={e['guid']:e for e in inv['entries']}
env=UnityPy.load(str(PRIVATE/'showcase-source/Terrain Castle.asset'));obj=next(o for o in env.objects if o.type.name=='TerrainData');terrain=obj.read_typetree();database=terrain['m_SplatDatabase'];layers=[]
for slot,ref in enumerate(database['m_TerrainLayers']):
 external=obj.assets_file.externals[ref['m_FileID']-1]
 guid=''.join(f'{b:02x}'[::-1]for b in external.guid);entry=by[guid];path=DATA/'source'/entry['path']
 text=re.sub(r'^%.*\n','',path.read_text(),flags=re.M);text=re.sub(r'^--- !u!\d+ &-?\d+','---',text,flags=re.M)
 d=next(x['TerrainLayer']for x in yaml.load_all(text,Loader=yaml.CSafeLoader)if x)
 textures={}
 for role,key in [('base','m_DiffuseTexture'),('normal','m_NormalMapTexture'),('mask','m_MaskMapTexture')]:
  g=d[key].get('guid');textures[role]={'guid':g,'path':by[g]['path'],'absolute_path':str(DATA/'source'/by[g]['path'])}if g in by else None
 layers.append({'alpha_channel':slot,'guid':guid,'source':entry['path'],'name':d['m_Name'],'textures':textures,'tile_size':d['m_TileSize'],'tile_offset':d['m_TileOffset'],'normal_scale':d['m_NormalScale'],'metallic':d['m_Metallic'],'smoothness':d['m_Smoothness'],'diffuse_remap_min':d['m_DiffuseRemapMin'],'diffuse_remap_max':d['m_DiffuseRemapMax'],'mask_remap_min':d['m_MaskMapRemapMin'],'mask_remap_max':d['m_MaskMapRemapMax'],'raw':d})
 # Stage these explicit binary dependencies and their texture closure for future export runs.
 for p in [path,Path(str(path)+'.meta')]+[Path(t['absolute_path'])for t in textures.values()if t]+[Path(str(t['absolute_path'])+'.meta')for t in textures.values()if t]:
  if p.exists():
   out=PRIVATE/'asset-bridge'/p.relative_to(DATA/'source');out.parent.mkdir(parents=True,exist_ok=True)
   if not out.exists():shutil.copy2(p,out)
report={'source_terrain':str(PRIVATE/'showcase-source/Terrain Castle.asset'),'verified_from':'TerrainData.m_SplatDatabase.m_TerrainLayers PPtr fileID -> SerializedFile.externals GUID, nibble-swapped byte encoding','alpha_binary':str(PRIVATE/'showcase-native/terrain/00-alpha.f32'),'alpha_layout':'float32 little-endian [z][x][layer], row0 UnityZ0, rows increasingUnityZ','width':database['m_AlphamapResolution'],'height':database['m_AlphamapResolution'],'layers':layers}
out=ROOT/'docs/stronghold/2026-11-01/showcase/terrain-layers.json';out.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'output':str(out),'order':[l['name']for l in layers]},indent=2))

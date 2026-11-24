#!/usr/bin/env python3
"""Create a private, data-only copy of the publisher's exact authored demo.

Restores original prefab FBX meshes in place of expensive inline vertex-painted
copies; keeps bespoke scene meshes that have no prefab source. The purchased
archive and original extracted scene are never edited. No purchased code runs.
"""
from pathlib import Path
import json,re,shutil,gzip,tarfile,os
ROOT=Path(__file__).resolve().parents[1];DATA=ROOT/'assets/purchased/nature_castle';PRIVATE=ROOT.parent/'.local-tools/unity-source';BRIDGE=PRIVATE/'asset-bridge'
SOURCE=PRIVATE/'showcase-source/Demo Castle.unity'
OUTPUT=BRIDGE/'Assets/ShowcaseSource/PublisherCastle.unity'
GUID=re.compile(rb'guid:\s*([a-fA-F0-9]{32})')
TEXT={'.prefab','.mat','.asset','.shader','.cginc','.hlsl','.shadersubgraph','.terrainlayer','.lighting','.unity','.playable'}
ALLOWED=TEXT|{'.fbx','.png','.tga','.jpg','.jpeg','.tif','.tiff','.exr','.hdr','.psd'}

def docs(path):
 block=[];kind=None;ident=0
 with path.open('rb')as f:
  for l in f:
   if l.startswith(b'--- !u!'):
    if block:yield kind,ident,b''.join(block)
    m=re.match(rb'--- !u!(\d+) &(\d+)',l);kind=int(m[1]);ident=int(m[2]);block=[l]
   else:block.append(l)
  if block:yield kind,ident,b''.join(block)

def lighten():
 layout=json.loads((ROOT/'docs/stronghold/2026-11-01/showcase/publisher-scene-layout.json').read_text())
 keep={int(x['m_Mesh']['fileID'])for x in layout['renderers']if x['kind']==33 and x.get('m_Mesh',{}).get('fileID')and not x['m_Mesh'].get('guid')}
 OUTPUT.parent.mkdir(parents=True,exist_ok=True);removed=0;overrides=0;deps=set()
 with OUTPUT.open('wb')as f:
  for kind,ident,b in docs(SOURCE):
   if kind==43 and ident not in keep:removed+=1;continue
   if kind==43:
    # Editor-only read access; raw scene geometry bytes remain unchanged.
    b=b.replace(b'  m_IsReadable: 0\n',b'  m_IsReadable: 1\n')
   if kind==1001:
    # Each modification occupies one complete list item, including target/value.
    b,n=re.subn(rb'    - target: [^\n]*\n      propertyPath: m_Mesh\n      value: [^\n]*\n      objectReference: [^\n]*\n',b'',b);overrides+=n
   f.write(b);deps.update(x.decode().lower()for x in GUID.findall(b))
 meta=OUTPUT.with_suffix('.unity.meta');meta.write_text('fileFormatVersion: 2\nguid: 5369b6883a294e6f932f9d46b77c1dad\nDefaultImporter:\n  externalObjects: {}\n  userData: \n  assetBundleName: \n  assetBundleVariant: \n')
 return deps,{'inline_meshes_omitted':removed,'prefab_mesh_overrides_restored':overrides,'bespoke_meshes_retained':len(keep),'scene_bytes':OUTPUT.stat().st_size}

def main():
 inv=json.loads((DATA/'INVENTORY.json').read_text());by={x['guid']:x for x in inv['entries']};deps,stats=lighten();seen=set();pending=list(deps);missing={};unresolved=set()
 # Gather closure from source copies or, when needed, extracted small support data.
 def src(e):
  p=DATA/'source'/e['path']
  if p.exists():return p
  q=PRIVATE/'showcase-source'/Path(e['path']).name
  return q if q.exists()else p
 while pending:
  guid=pending.pop()
  if guid in seen:continue
  seen.add(guid);e=by.get(guid)
  if not e:unresolved.add(guid);continue
  if Path(e['path']).suffix.lower()not in ALLOWED:continue
  p=src(e)
  if not p.exists():missing[guid]=e;continue
  for p2 in [p,Path(str(p)+'.meta')]:
   if not p2.exists()or(p2==p and p.suffix.lower()not in TEXT):continue
   pending.extend(x.decode().lower()for x in GUID.findall(p2.read_bytes()))
 # Missing non-code support files are extracted once, then include their closure.
 if missing:
  target={e[k]['member']:(DATA/'source'/(e['path']+(''if k=='asset'else'.meta')))for e in missing.values()for k in ['asset','asset.meta']if k in e}
  with gzip.open(inv['package'],'rb')as decoded,tarfile.open(fileobj=decoded,mode='r|')as tar:
   for m in tar:
    out=target.get(m.name)
    if out is None or out.exists():continue
    out.parent.mkdir(parents=True,exist_ok=True)
    with tar.extractfile(m)as s,out.open('wb')as d:shutil.copyfileobj(s,d,4*1024*1024)
  # Restart closure after support-data extraction, bounded by very few .assets.
  return main()
 staged=[]
 for guid in sorted(seen):
  e=by.get(guid)
  if not e or Path(e['path']).suffix.lower()not in ALLOWED:continue
  p=src(e);dest=BRIDGE/e['path'];dest.parent.mkdir(parents=True,exist_ok=True)
  if not dest.exists():
   # Imported binary source remains immutable; editable metadata is copied.
   if p.suffix.lower()in {'.fbx','.png','.tga','.jpg','.jpeg','.tif','.tiff','.exr','.hdr','.psd'}:os.link(p,dest)
   else:shutil.copy2(p,dest)
  meta=Path(str(p)+'.meta');outmeta=Path(str(dest)+'.meta')
  if meta.exists()and not outmeta.exists():shutil.copy2(meta,outmeta)
  staged.append(e['path'])
 config={'scene':'Assets/ShowcaseSource/PublisherCastle.unity','output':str(PRIVATE/'showcase-native'),'chunk_size':64,'groups':[],'export_terrain':True,'lod_mid_index':2,'original_scene':str(SOURCE),'source_mesh_paint_preserved':False,'restored_source_fbx_geometry':True}
 (PRIVATE/'showcase-selection.json').write_text(json.dumps(config,indent=2)+'\n')
 report={'source':str(SOURCE),'staged_scene':str(OUTPUT),'source_preserved':True,'purchased_scripts_staged':0,**stats,'staged_dependency_count':len(staged),'unresolved_guids':sorted(unresolved),'staged_dependencies':staged}
 out=ROOT/'docs/stronghold/2026-11-01/showcase/staging.json';out.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items()if k not in{'staged_dependencies','unresolved_guids'}},indent=2))
if __name__=='__main__':main()

#!/usr/bin/env python3
"""Install the private eight-layer native terrain shader and GPU manifest."""
import argparse
import json
from pathlib import Path
import shutil
import hashlib
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]

def copy_unchanged(source,target):
 source=Path(source)
 if target.is_file()and source.stat().st_size==target.stat().st_size and hashlib.sha256(source.read_bytes()).digest()==hashlib.sha256(target.read_bytes()).digest():return
 shutil.copyfile(source,target)

def main():
 parser=argparse.ArgumentParser()
 parser.add_argument('--project',type=Path,default=ROOT/'projects/valwood_showcase')
 parser.add_argument('--materials',type=Path,default=ROOT/'docs/stronghold/2026-11-01/showcase/terrain-materials/manifest.json')
 parser.add_argument('--origin-x',type=float,default=-43344.)
 parser.add_argument('--origin-z',type=float,default=-23987.824156518269)
 parser.add_argument('--size',type=float,default=1000.)
 parser.add_argument('--holes',type=Path,default=ROOT.parent/'.local-tools/unity-source/showcase-native/terrain/00-holes.u8')
 args=parser.parse_args()
 if not(args.project/'shaders').is_dir():raise SystemExit('Create the isolated project/shaders before patching terrain')
 base=(ROOT/'projects/valwood_visual/shaders/world_terrain.fs.glsl').read_text()
 head=base.split('void main() {',1)[0]
 body=(ROOT/'tools/templates/world_showcase_terrain.glsl').read_text()
 (args.project/'shaders/world_terrain.fs.glsl').write_text(head+body)
 material=json.loads(args.materials.read_text());assert len(material['layers'])==8
 target=args.project/'ground/showcase_splat';target.mkdir(parents=True,exist_ok=True)
 for i in range(2):copy_unchanged(material[f'alpha{i}'],target/f'alpha{i}.png')
 if args.holes.is_file():
  holes=np.fromfile(args.holes,np.uint8);side=int(round(len(holes)**.5))
  if side*side!=len(holes)or side<4 or side>2048 or not np.isin(holes,[0,1]).all():raise ValueError('Invalid source TerrainData holes')
  # Keep raw UnityZ row order, as with alpha maps. The native uploader flips
  # exactly once so GLv0 is sourceZmax / engineZmin; nearest cells stay2K.
  Image.fromarray(holes.reshape(side,side)*255).save(target/'holes.png')
 xmin,zmin=args.origin_x-args.size/2,args.origin_z-args.size/2
 rows=['VWSPLAT1',f'bounds {xmin:.12f} {zmin:.12f} {args.size:.12f} {args.size:.12f}','weights "alpha0.png" "alpha1.png" 1']
 for i,layer in enumerate(material['layers']):
  for channel in ['base','normal','arm']:copy_unchanged(layer[channel],target/f'{i}_{channel}.png')
  sx,sz=map(float,layer['tile_size']);ox,oz=map(float,layer['tile_offset'])
  # Source terrain is [0,size] in UnityXZ, centered into the native world
  # with Z mirrored. Standard material PNGs flip exactly once at upload.
  values=[1/sx,-1/sz,ox/sx,(args.size+oz)/sz,float(layer['normal_scale'])]
  rows.append(f'layer "{i}_base.png" "{i}_normal.png" "{i}_arm.png" '+' '.join(f'{v:.12g}'for v in values))
 (target/'terrain.txt').write_text('\n'.join(rows)+'\n')
 (target/'source.json').write_text(json.dumps(material,indent=2)+'\n')
 print(json.dumps({'shader':str(args.project/'shaders/world_terrain.fs.glsl'),'manifest':str(target/'terrain.txt'),'layers':8,'arrays':3,'texture_units':[0,1,2],'bounds':[xmin,zmin,args.size,args.size]},indent=2))

if __name__=='__main__':main()

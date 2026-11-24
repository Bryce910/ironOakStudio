#!/usr/bin/env python3
"""Reject the visible blue-padding regression in the actual seated-pose GLBs."""
import io
import json
from pathlib import Path
import sys

import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from audit_village_pose_compatibility import glb,values

models=ROOT/'projects/valwood_world/models'
manifest=json.loads((models/'village_seated_manifest.json').read_text())
assert manifest['source_uv_layers']=={'Body':'UVmap','Head':'UVMap','Hair':'UVMap'}
reference=None
for frame in range(4):
    j,b=glb(models/f'village_character_seated_idle_{frame:02}.glb')
    p=j['meshes'][0]['primitives'][0]
    uv=values(j,b,p['attributes']['TEXCOORD_0'])
    indices=values(j,b,p['indices'])
    if reference is None:reference=(uv,indices)
    else:assert np.array_equal(uv,reference[0]) and np.array_equal(indices,reference[1])
    texture=j['materials'][0]['pbrMetallicRoughness']['baseColorTexture']['index']
    image=j['images'][j['textures'][texture]['source']]
    view=j['bufferViews'][image['bufferView']]
    offset=view.get('byteOffset',0)
    pixels=np.asarray(Image.open(io.BytesIO(b[offset:offset+view['byteLength']])).convert('RGB'))
    assert pixels.shape==(1024,1024,3)
    body=uv[(uv[:,0]<.5)&(uv[:,1]>.5)]
    assert len(body)==7344
    xy=np.clip((body*1024).astype(int),0,1023);c=pixels[xy[:,1],xy[:,0]]
    padding=float(np.mean((c[:,2]>217)&(c[:,0]<166)&(c[:,1]<166)))
    # Source seams touch padding at a few corners. The bad map covers96.6%.
    assert padding<.06,(frame,padding)
print('PASS actual seated textures: all4 keys use fabric/skin rather than blue padding; exact adjacent-key UV/index correspondence')

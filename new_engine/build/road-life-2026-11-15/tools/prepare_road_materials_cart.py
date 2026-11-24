#!/usr/bin/env python3
from pathlib import Path
import copy, io, json, math
import numpy as np
from PIL import Image, ImageOps
from road_asset_common import *

TEXTURES=OUT/'textures';TEXTURES.mkdir(exist_ok=True)
MODELS=OUT/'models';MODELS.mkdir(exist_ok=True)

def pack(label,path,green=False):
    d,b=read_glb(path);g={k:copy.deepcopy(d[k]) for k in ('materials','images','textures','samplers') if k in d}
    normal_images={d['textures'][m['normalTexture']['index']]['source'] for m in d['materials'] if 'normalTexture'in m}
    rows=[]
    for i,im in enumerate(g.get('images',[])):
        v=d['bufferViews'][im['bufferView']];data=b[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']]
        src=Image.open(io.BytesIO(data));image=src.copy()
        image.thumbnail((2048,2048) if label=='mounted' and i not in normal_images else (1024,1024),Image.Resampling.LANCZOS)
        if green and i in normal_images:
            image=image.convert('RGB');arr=np.asarray(image).copy();arr[:,:,1]=255-arr[:,:,1];image=Image.fromarray(arr)
        mem=io.BytesIO();image.save(mem,format='PNG');encoded=mem.getvalue();digest=hashlib.sha256(encoded).hexdigest();target=TEXTURES/(digest+'.png');target.write_bytes(encoded)
        g['images'][i]={'mimeType':'image/png','source_path':str(target)}
        rows.append({'source_glb':str(Path(path).resolve()),'source_glb_sha256':sha(path),'image_index':i,'source_image_sha256':hashlib.sha256(data).hexdigest(),'source_dimensions':list(src.size),'dimensions':list(image.size),'output':str(target),'sha256':digest,'normal_green_inverted':green and i in normal_images})
    result={'gltf':g,'images':rows,'source_glb':str(Path(path).resolve()),'source_glb_sha256':sha(path)}
    (OUT/(label+'_materials.json')).write_text(json.dumps(result,indent=2)+'\n');return result

mounted=pack('mounted',ROOT/'projects/valwood_world/models/village_horseman_mid.glb',True)
puller=pack('puller',ROOT/'projects/valwood_world/models/purchased_character_wayfarer_walk_00.glb',True)
source=ROOT/'assets/purchased/medieval_village/publisher-source/models'
cart=pack('cart',source/'SM_Wooden_Cart_near.glb')
wheelpack=pack('wheel',source/'SM_Wooden_Cart_Wheel_near.glb')

def geometry(path):
    d,b=read_glb(path);out=[]
    for q in d['meshes'][0]['primitives']:
        attrs=q['attributes'];p=accessor(d,b,attrs['POSITION']);n=accessor(d,b,attrs['NORMAL']);uv=accessor(d,b,attrs['TEXCOORD_0']);ix=accessor(d,b,q['indices']).reshape(-1,3)
        area=np.linalg.norm(np.cross(p[ix[:,1]]-p[ix[:,0]],p[ix[:,2]]-p[ix[:,0]]),axis=1)
        ix=ix[area>1e-10]
        if len(ix):
            item={'p':p,'n':n,'uv':uv,'ix':ix,'material':q['material']}
            for key,semantic in [('uv1','TEXCOORD_1'),('paint','_VALWOOD_SOURCE_PAINT')]:
                if semantic in attrs:item[key]=accessor(d,b,attrs[semantic])
            out.append(item)
    return out

def components(q):
    _,inv=np.unique(np.round(q['p'],5),axis=0,return_inverse=True);par=np.arange(max(inv)+1)
    def find(i):
        while par[i]!=i:par[i]=par[par[i]];i=par[i]
        return i
    for a,b,c in inv[q['ix']]:
        a,b,c=find(a),find(b),find(c);par[b]=a;par[c]=a
    groups={}
    for i,v in enumerate(inv[q['ix'][:,0]]):groups.setdefault(find(v),[]).append(i)
    return list(groups.values())

# Preserve the original publisher cart, separating its disconnected real wheels
# from chassis/shafts. Its parked shaft tips are lifted about the actual axle.
qs=geometry(source/'SM_Wooden_Cart_near.glb');removed=[];kept=[];shaft_tips=[]
for q in qs:
    keep=[]
    for ids in components(q):
        pts=q['p'][q['ix'][ids].ravel()];lo=pts.min(0);hi=pts.max(0);center=(lo+hi)*.5
        is_wheel=abs(center[2])>.70 and lo[0]>-.84 and hi[0]<.84 and lo[1]>=-.001 and hi[1]<1.61 and hi[2]-lo[2]<.4
        if is_wheel:removed.append({'material':q['material'],'triangles':len(ids),'bounds':[lo.tolist(),hi.tolist()]})
        else:
            keep.extend(ids)
            if hi[0]>2.5 and hi[1]-lo[1]>1.0:
                tips=np.unique(pts[pts[:,0]>hi[0]-.06],axis=0);shaft_tips.append(tips.mean(0))
    if keep:
        q['ix']=q['ix'][keep]
        used,remap=np.unique(q['ix'],return_inverse=True)
        for k in ('p','n','uv','uv1','paint'):
            if k in q:q[k]=q[k][used]
        q['ix']=remap.reshape(-1,3);kept.append(q)
assert len(shaft_tips)==2 and len(removed)>15,(shaft_tips,len(removed))
scale=.55;pitch_degrees=37;angle=math.radians(pitch_degrees);co,si=math.cos(angle),math.sin(angle)
pitch=np.array([[co,-si,0],[si,co,0],[0,0,1.]])
forward=np.array([[0.,0,-1],[0,1,0],[1,0,0]])
rotation=forward@pitch;pivot=np.array([0.,.797804415,0.])
def transform(p):return ((p-pivot)@rotation.T+pivot)*scale
for q in kept:
    q['p']=transform(q['p']);q['n']=q['n']@rotation.T;q['t']=tangents(q['p'],q['n'],q['uv'],q['ix'])
body=write_glb(MODELS/'cart_body.glb',kept,cart,{'owned_source':'SM_Wooden_Cart','parked_to_pulled_pitch_degrees':pitch_degrees,'scale':scale})

# The publisher's separate wheel is re-centered on its measured round rim, not
# the export's bottom-on-floor pivot. Local X is its true rotation axis.
wqs=geometry(source/'SM_Wooden_Cart_Wheel_near.glb')
wp=np.concatenate([q['p']for q in wqs]);rim_center=np.array([(wp[:,0].min()+wp[:,0].max())*.5,(wp[:,1].min()+wp[:,1].max())*.5,0.])
wheel_rotation=forward
for q in wqs:
    q['p']=(q['p']-rim_center)@wheel_rotation.T*scale;q['n']=q['n']@wheel_rotation.T;q['t']=tangents(q['p'],q['n'],q['uv'],q['ix'])
wheel=write_glb(MODELS/'cart_wheel.glb',wqs,wheelpack,{'owned_source':'SM_Wooden_Cart_Wheel','measured_source_axle':rim_center.tolist(),'scale':scale})
grips=transform(np.array(shaft_tips));offset=np.array([-grips[:,0].mean(),0.,-.27-grips[:,2].mean()]);grips+=offset
axles=[[.82*scale,.797804415*scale,0.],[-.87*scale,.797804415*scale,0.]]
radius=float((wp[:,1].max()-wp[:,1].min())*.5*scale)
full_lo=min(body['bounds'][0][0],min(x[0] for x in axles)+wheel['bounds'][0][0])
full_hi=max(body['bounds'][1][0],max(x[0] for x in axles)+wheel['bounds'][1][0])
full_width=full_hi-full_lo
assert full_width<1.45 and np.all((grips[:,1]>.85)&(grips[:,1]<1.05)),(full_width,grips)
paint_proof=[]
for label,primitives in [('body',kept),('wheel',wqs)]:
    for i,q in enumerate(primitives):
        if 'paint' in q:
            assert np.max(np.abs(q['paint']*255-np.round(q['paint']*255)))<.00051
            paint_proof.append({'asset':label,'primitive':i,'source_paint_vertices':len(q['paint']),'source_rgb8_sha256':hashlib.sha256(q['paint'].astype('<f4').tobytes()).hexdigest(),'uv1_sha256':hashlib.sha256(q['uv1'].astype('<f4').tobytes()).hexdigest()})
manifest={'body':body,'wheel':wheel,'cart_local_offset':offset.tolist(),'axles_local':axles,'wheel_radius_m':radius,'wheel_axis':[1.,0.,0.],'body_forward':'+Z','grips_local':grips.tolist(),'width_m':full_width,'source_scale':scale,'source_parked_pitch_degrees':pitch_degrees,'source_removed_wheel_components':removed,'source_removed_triangles':sum(x['triangles']for x in removed),'source_paths':{str(source/'SM_Wooden_Cart_near.glb'):sha(source/'SM_Wooden_Cart_near.glb'),str(source/'SM_Wooden_Cart_Wheel_near.glb'):sha(source/'SM_Wooden_Cart_Wheel_near.glb')},'source_wheel_center':rim_center.tolist(),'wheel_pivot_baked_at_zero':True,'original_material_graph_preserved':True,'source_paint_preservation':paint_proof}
(OUT/'CART_ASSETS.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps({'passed':True,'cart':manifest,'materials_frozen':True}))

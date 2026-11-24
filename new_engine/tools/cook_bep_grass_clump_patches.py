#!/usr/bin/env python3
"""Dense source-photo grass patches, replacing sparse individual blade strips.

One owned BEP CGrass_01 full-clump alpha texture is reused by both aliases.
HIGH has12 tufts ×3 bent crossed cards (144 triangles); MID retains all12
tuft sites with1 card each (48 triangles). Neutral AO: Unity terrain-detail
vertex alpha is wind/fade, not TreeCreator AO. Prior catalogs/projects stay
immutable; each completed model replaces only its own new temporary file.
"""
from pathlib import Path
import copy,json,hashlib,struct
import numpy as np
from PIL import Image
from scipy.ndimage import distance_transform_edt
from cook_bep_reforged_vegetation import Cook,ROOT,SOURCE,normalize,sha
from cook_bep_countryside_materials import linear,srgb,resize

BASE=ROOT/'assets/purchased/bep_reforged/countryside-fern-binding'
OUT=ROOT/'assets/purchased/bep_reforged/countryside-grass-clumps'
GUID='4cacb6d81ae591f43a0421451adf40a3'

def card_geometry(alias,planes,bounds):
    rng=np.random.default_rng(481 if alias.endswith('long') else 829);positions=[];normals=[];tangents=[];uv=[];indices=[];roots=[]
    for i in range(12):
        center=np.array([(i%4-1.5)*.74+rng.uniform(-.09,.09),0,(i//4-1)*.87+rng.uniform(-.09,.09)])
        angle=rng.uniform(-np.pi,np.pi);height=rng.uniform(.48,.72);width=rng.uniform(.72,.95);bend=rng.uniform(-.08,.08,2);roots.append(center.tolist())
        for plane in range(3):
            theta=angle+plane*np.pi/3;right=np.array([np.cos(theta),0,np.sin(theta)]);face=np.cross(right,[0,1,0]);normal=normalize(np.array([face+np.array([0,.55,0])]))[0]
            if plane>=planes:continue
            offset=len(positions)
            for level,v in [(0.,1.),(.55,.45),(1.,0.)]:
                shift=np.array([bend[0],0,bend[1]])*level*level
                for side,u in [(-.5,0.),(.5,1.)]:
                    positions.append(center+right*width*side+np.array([0,height*level,0])+shift);normals.append(normal);tangents.append([*right,-1.]);uv.append([u,v])
            indices.extend((np.array([[0,1,2],[1,3,2],[2,3,4],[3,5,4]])+offset).tolist())
    v=dict(position=np.asarray(positions),normal=np.asarray(normals),tangent=np.asarray(tangents),uv0=np.asarray(uv),uv1=np.zeros((len(positions),2)),color=np.ones((len(positions),4)))
    # Both tiers use the HIGH transform so MID roots remain exactly in place.
    if planes==3:
        lo=v['position'].min(0);hi=v['position'].max(0);targetlo,targethi=np.asarray(bounds);scale=(targethi-targetlo)/np.maximum(hi-lo,1e-9);shift=targetlo-lo*scale
        card_geometry.transforms[alias]=(scale,shift)
    scale,shift=card_geometry.transforms[alias];v['position']=v['position']*scale+shift;v['normal']=normalize(v['normal']/scale);v['tangent'][:,:3]=normalize(v['tangent'][:,:3]*scale)
    # Neutral color is not emitted as source-paint: no wind-alpha/AO union.
    return {k:np.asarray(a,np.float32)for k,a in v.items()},np.asarray(indices,np.uint32),(np.asarray(roots)*scale+shift).tolist()
card_geometry.transforms={}

def main():
    c=Cook(OUT);original=json.loads((BASE/'catalog.json').read_text());catalog=copy.deepcopy(original)
    source=c.source_file(GUID);im=Image.open(source).convert('RGBA');a=np.asarray(im,np.float32)/255;alpha=a[:,:,3];cutoff=.35;size=(512,512)
    rgb=linear(a[:,:,:3]);newalpha=np.clip(resize(alpha[:,:,None],size)[:,:,0],0,1);rgb=resize(rgb*alpha[:,:,None],size)/np.maximum(newalpha[:,:,None],1e-6)
    valid=newalpha>.02;nearest=distance_transform_edt(~valid,return_distances=False,return_indices=True);rgb[~valid]=rgb[nearest[0][~valid],nearest[1][~valid]]
    coverage=float((alpha>=cutoff).mean());lo,hi=.25,4.
    for _ in range(24):
        factor=(lo+hi)*.5
        if float((newalpha*factor>=cutoff).mean())<coverage:lo=factor
        else:hi=factor
    newalpha=np.clip(newalpha*(lo+hi)*.5,0,1);base=c.image(np.dstack((srgb(np.clip(rgb,0,1)),newalpha)),'base',512)
    row=dict(name='BEP_CGrass_full_clump',base=base,normal=None,arm=None,roughness=1.,metallic=0.,factor=[.78,1.,.78,1.],alphaMode='MASK',alphaCutoff=cutoff,doubleSided=True,facade_mode=0,source_paint=False,
        source_texture_guid=GUID,new_dressing_tint_linear=[.78,1.,.78],source_vertex_alpha_interpretation='Terrain detail grass uses wind/fade alpha; no TreeCreator vertex AO copied',
        source_scope='Owned original full-clump photograph on newly composed dense crossed cards; not claimed source mesh or author layout')
    c.rows['BEP_CGrass_full_clump']=row;evidence=[]
    for asset in catalog['assets']:
        alias=asset['native']['alias']
        if not alias.startswith('bep_nature_grass_'):continue
        files=[];stats=[];roots=[]
        for lod,planes in enumerate([3,1]):
            v,ix,tufts=card_geometry(alias,planes,asset['bounds']);path=OUT/'models'/(alias+('_near'if lod==0 else'_mid')+'.glb');temp=path.with_suffix('.glb.tmp')
            stats.append(c.emit(temp,v,[(ix,'BEP_CGrass_full_clump')],asset['pivot_offset_native'][1],source_ao=False));temp.replace(path);files.append(str(path.resolve()));roots.append(tufts)
        assert np.array_equal(np.asarray(roots[0]),np.asarray(roots[1]))
        oldbounds=asset['bounds'];asset.update(files=files,stats=stats,source_guids=[GUID,GUID],source_geometry='New12-tuft patch: whole-clump alpha photo; HIGH3cards/tuft, MID1card/tuft, all root sites retained',lod_limitations=[])
        asset['material_binding']='New source-photo grass dressing, neutral AO and coherent green material tint; one shared512RGBA page.'
        evidence.append(dict(alias=alias,original_bounds=oldbounds,high_bounds=stats[0]['bounds'],pivot_offset_native=asset['pivot_offset_native'],tuft_sites_native=roots[0],MID_keeps_every_tuft=True,high_triangles=stats[0]['triangles'],mid_triangles=stats[1]['triangles']))
        print('GRASS READY',alias,[s['triangles']for s in stats],flush=True)
    before={a['name']:a for a in original['assets']if not a['native']['alias'].startswith('bep_nature_grass_')};after={a['name']:a for a in catalog['assets']if not a['native']['alias'].startswith('bep_nature_grass_')};assert before==after
    # The old8MiB grass family was unique to these two aliases. One shared
    #512RGBA+mips page replaces it; other source foliage pages stay identical.
    catalog['source_image_pages_RGBA8_mips_MiB']=original['source_image_pages_RGBA8_mips_MiB']-8+4/3
    catalog['texture_budget_RGBA8_mips_MiB']=original['texture_budget_RGBA8_mips_MiB']-8+4/3
    catalog['revision']='Dense whole-clump source-photo grass cards; fern wrapper correction retained; other6 source assets unchanged'
    (OUT/'catalog.json').write_text(json.dumps(catalog,indent=2,allow_nan=False)+'\n');(OUT/'materials.json').write_text(json.dumps(c.rows,indent=2)+'\n')
    audit=dict(source_guid=GUID,source_path=c.inventory[GUID]['path'],source_sha256=sha(source),source_size=list(im.size),prepared_size=list(size),original_mask_coverage=coverage,prepared_mask_coverage=float((newalpha>=cutoff).mean()),
        source_shader_evidence='Unity TerrainEngine.cginc WavingGrassVert uses alpha as wave strength; TreeCreatorLeavesOptimized separately labels alpha AO. New cards omit source-paint/AO entirely.',
        source_shader_links=['https://raw.githubusercontent.com/TwoTailsGames/Unity-Built-in-Shaders/master/CGIncludes/TerrainEngine.cginc','https://raw.githubusercontent.com/TwoTailsGames/Unity-Built-in-Shaders/master/DefaultResourcesExtra/Nature/TreeCreator/TreeCreatorLeavesOptimized.shader'],
        material=row,geometry=evidence,other6_asset_records_unchanged=True,shared_texture_RGBA8_mips_MiB=4/3,net_texture_GPU_saved_MiB=8-4/3,no_shader_project_or_lighting_changes=True)
    (OUT/'source-audit.json').write_text(json.dumps(audit,indent=2,allow_nan=False)+'\n');print('FULL8 CATALOG',OUT/'catalog.json',sha(OUT/'catalog.json'),flush=True)

if __name__=='__main__':main()

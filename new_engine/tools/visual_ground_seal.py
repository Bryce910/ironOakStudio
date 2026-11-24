#!/usr/bin/env python3
"""Seal pre-existing submillimetre town ownership cracks under the top surfaces.

Append a two-centimetre overlap on either side of the exact source paving rim
to the adjacent countryside tiles. The new triangles remain inside each
tile's existing AABB, below both existing tops. No old vertex/index, model
datum, placement, field or collider changes. Existing shared soil is reused.
"""
from pathlib import Path
import argparse,copy,json
import numpy as np
from visual_ground_finish import ROOT,GLB,append_view,write_glb,sha
from city_ground_ownership import CityGroundOwnership,shapely
from countryside_contact import TerrainSurfaces


def accessor(doc,binary,array,kind,target=34962,integer=False):
    a=np.ascontiguousarray(array,dtype='<u4'if integer else'<f4')
    binary,view=append_view(doc,binary,a.tobytes(),target=target)
    i=len(doc['accessors']);doc['accessors'].append(dict(bufferView=view,componentType=5125 if integer else 5126,count=len(a),type=kind,
        min=np.min(a,axis=0).reshape(-1).tolist(),max=np.max(a,axis=0).reshape(-1).tolist()))
    return binary,i


def build(project,output,width=.02,below=.002):
    project=Path(project).resolve();output=Path(output).resolve()
    if output.exists():raise ValueError('Use a fresh immutable candidate output')
    output.mkdir(parents=True);catalog_path=ROOT/'assets/purchased/bep_reforged/city-ground-final/catalog.json';data=json.loads(catalog_path.read_text())
    origin=np.asarray(json.loads((project/'city_showcase.json').read_text())['origin'])
    boundary=CityGroundOwnership(data,ROOT/'assets/purchased/bep_reforged/city-ground-support/source-ground-footprint.wkb').polygon.boundary
    strip=boundary.buffer(width,cap_style='square',join_style='mitre')
    assets={a['name']:a for a in data['assets']};town=copy.deepcopy(assets[data['ground_asset']]);town['name']='BEP_Countryside_Terrain_SourceGround'
    missing=lambda p:np.full(np.asarray(p).shape[:-1],np.nan)
    town_sampler=TerrainSurfaces(dict(assets=[town],placements=[dict(p,asset=town['name'])for p in data['placements']if p['asset']==data['ground_asset']]),origin,missing)
    country_sampler=TerrainSurfaces(data,origin,missing)
    alias_registry={s[1]:s[2]for s in[line.split()for line in(project/'world.studio').read_text().splitlines()]if len(s)>3 and s[0]=='asset'}
    models=[];replacements=[]
    for name in data['countryside']['terrain_assets']:
        asset=assets[name];alias=asset['native']['alias'];anchor=np.asarray(asset['stats'][0]['anchor']);low=anchor-[55,57.5];high=anchor+[55,57.5]
        owned=shapely.intersection(strip,shapely.box(*low,*high))
        if owned.is_empty:continue
        faces=shapely.get_parts(shapely.constrained_delaunay_triangles(owned))
        faces=faces[shapely.area(faces)>1e-12]
        points=np.concatenate([np.asarray(t.exterior.coords)[:3]for t in faces])
        unique,inverse=np.unique(points,axis=0,return_inverse=True);indices=inverse.reshape(-1,3)
        area=np.cross(unique[indices[:,1]]-unique[indices[:,0]],unique[indices[:,2]]-unique[indices[:,0]])
        reverse=area>0;indices[reverse]=indices[reverse][:,[0,2,1]]
        for suffix,label,lod in [('.glb','near',0),('_mid.glb','mid',1)]:
            prefix=alias_registry[alias];relative=Path('models')/(prefix+suffix);source=project/relative;old=GLB(source);doc=copy.deepcopy(old.doc);binary=old.binary
            # Probe both sides of each rim point. Every new vertex is at most
            # 2cm from that rim; the extra3mm straddle covers export rounding.
            offsets=np.array([[x,z]for x in[-.003,0,.003]for z in[-.003,0,.003]])
            samples=(unique[:,None,:]+offsets[None]).reshape(-1,2)+origin[[0,2]]
            a=town_sampler.sample(samples,lod).reshape(len(unique),-1);b=country_sampler.sample(samples,lod).reshape(len(unique),-1)
            values=np.concatenate((a,b),axis=1);height=np.min(np.where(np.isfinite(values),values,np.inf),axis=1)-below
            if not np.isfinite(height).all():raise ValueError(('Uncovered rim source samples',alias,label))
            datum=float(origin[1]+asset['pivot_offset_native'][1]);position=np.column_stack((unique[:,0]-anchor[0],height-datum,unique[:,1]-anchor[1])).astype('<f4')
            old_positions=np.concatenate([old.read(p['attributes']['POSITION'])for m in old.doc['meshes']for p in m['primitives']]);bounds=np.stack((old_positions.min(0),old_positions.max(0)))
            if np.any(position<bounds[0]-1e-7)or np.any(position>bounds[1]+1e-7):raise ValueError(('Seal extends existing AABB',alias,label,position.min(0),position.max(0),bounds))
            source_material=next((i for i,m in enumerate(doc['materials'])if m['name']=='earthpath'),None)
            if source_material is None:source_material=next((i for i,m in enumerate(doc['materials'])if m['name']=='grassground'),None)
            if source_material is None:raise ValueError('Missing existing shared soil material')
            params=doc['materials'][source_material]['extensions']['VALWOOD_materials_facade']['parameters']
            if params[0:2]!=[1,2]or params[7]!=7.875:raise ValueError('Expected accepted blended ground material')
            normal=np.tile([0.,1.,0.],(len(unique),1));tangent=np.tile([1.,0.,0.,-1.],(len(unique),1));uv=unique/4.;paint=np.ones((len(unique),1))
            attrs={}
            for key,array,kind in [('POSITION',position,'VEC3'),('NORMAL',normal,'VEC3'),('TANGENT',tangent,'VEC4'),('TEXCOORD_0',uv,'VEC2'),('_VALWOOD_PAINT',paint,'SCALAR')]:
                binary,attrs[key]=accessor(doc,binary,array,kind)
            binary,ix=accessor(doc,binary,indices.reshape(-1,1),'SCALAR',34963,True)
            doc['meshes'][0]['primitives'].append(dict(attributes=attrs,indices=ix,material=source_material))
            dest=output/'models'/alias/('asset_'+label+'.glb');write_glb(dest,doc,binary);new=GLB(dest)
            new_positions=np.concatenate([new.read(p['attributes']['POSITION'])for m in new.doc['meshes']for p in m['primitives']]);new_bounds=np.stack((new_positions.min(0),new_positions.max(0)))
            checks=dict(original_BIN_prefix_exact=new.binary[:len(old.binary)]==old.binary,original_primitives_exact=new.doc['meshes'][0]['primitives'][:-1]==old.doc['meshes'][0]['primitives'],AABB_and_raw_minimum_Y_exact=np.array_equal(new_bounds,bounds),nodes_and_scenes_exact=all(new.doc[k]==old.doc[k]for k in ['nodes','scenes']),material_textures_and_factors_exact=new.doc['materials']==old.doc['materials']and new.doc['images']==old.doc['images'])
            if not all(checks.values()):raise ValueError(('Seal geometry audit failed',checks))
            row=dict(alias=alias,lod=label,source=str(source),source_sha256=sha(source),output=str(dest),sha256=sha(dest),new_vertices=len(unique),new_triangles=len(indices),new_geometry_bytes=len(unique)*68+len(indices)*12,seal_projected_area_m2=float(owned.area),bounds=bounds.tolist(),checks=checks,
                new_world_Y_range=[float(height.min()),float(height.max())],maximum_nearby_top_height_difference_m=float(np.nanmax(abs(a[:,4]-b[:,4]))) if np.any(np.isfinite(a[:,4])&np.isfinite(b[:,4])) else None)
            models.append(row);replacements.append(dict(relative_path=relative.as_posix(),candidate=str(dest),sha256=sha(dest)))
            if label=='near':replacements.append(dict(relative_path=(Path('models')/(prefix+'_near.glb')).as_posix(),candidate=str(dest),sha256=sha(dest)))
    report=dict(schema=1,status='UNDER_SURFACE_OWNERSHIP_SEAL_CANDIDATE',source_project=str(project),source_catalog=str(catalog_path),source_catalog_sha256=sha(catalog_path),width_either_side_m=width,lower_than_nearby_existing_tops_m=below,source_paving_perimeter_m=float(boundary.length),source_paving_projected_area_m2=float(strip.area),models=models,replacements=replacements,
        all_four_source_paving_rims_and_hole_boundaries=True,source_geometry_and_indices_and_fields_and_physics_unchanged=True,model_AABBs_and_normalized_placement_datums_unchanged=True,new_underside_strips_only=True,source_project_modified=False,GPU_visual_verification_pending=True)
    (output/'SEAL_REVIEW.json').write_text(json.dumps(report,indent=2)+'\n');return report


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--project',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);args=ap.parse_args();r=build(args.project,args.output)
    print(json.dumps(dict(status=r['status'],models=len(r['models']),triangles=sum(x['new_triangles']for x in r['models'])),indent=2))


if __name__=='__main__':main()

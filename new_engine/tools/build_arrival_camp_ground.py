#!/usr/bin/env python3
"""Stage append-only scalar soil paint for the first-arrival campsite.

Reads the accepted v14 terrain; never edits projects or source model banks.
Existing materials/images/geometry/indices/accessors remain byte-exact. Only
eligible _VALWOOD_PAINT bindings change to an appended bounded float accessor.
"""
from pathlib import Path
import argparse
import copy
import json
import numpy as np

from visual_ground_finish import GLB, append_view, write_glb, sha, segment_distance, MARKER
from build_arrival_camp_assets import ROOT, CENTER, DEFAULT_PROJECT

DEFAULT_OUTPUT=ROOT/'docs/arrival-camp/2026-11-04/ground'


def smooth_falloff(distance, core, feather):
    t=np.clip((distance-core)/feather,0,1)
    return 1-t*t*(3-2*t)


def worn_weight(world, segments):
    p=np.asarray(world,float)-CENTER[[0,2]]
    # The central5.2m is entirely worn. A1.2m shoulder fades at the broad
    # perimeter; modest fixed world-space edge noise avoids a geometric stamp.
    radii=np.array([7.3,7.7])
    ellipse=(np.linalg.norm(p/radii,axis=1)-1)*min(radii)
    edge=.11*np.sin(world[:,0]*1.11+world[:,1]*.87)+.065*np.sin(world[:,0]*.49-world[:,1]*1.73)
    camp=smooth_falloff(ellipse+edge,-1.2,1.2)
    # The guy ropes and rear sleeping corner extend diagonally beyond the
    # ellipse. A softly rounded shelter footprint joins the central clearing.
    low=np.array([-5.95,-5.8]);high=np.array([-1.2,-2.2])
    exterior=np.maximum(np.maximum(low-p,p-high),0)
    tent=smooth_falloff(np.linalg.norm(exterior,axis=1)+edge*.5,0,.8)
    lane=smooth_falloff(segment_distance(world,segments)+edge*.65,1.45,1.2)
    return np.maximum.reduce([camp,tent,lane]).clip(0,1).astype('<f4')


def stage(project, output):
    if output.exists():
        raise ValueError('Use a fresh output directory; candidates remain immutable')
    output.mkdir(parents=True)
    camp=json.loads((ROOT/'docs/arrival-camp/2026-11-04/art/ART_CANDIDATE.json').read_text())
    points=np.asarray(camp['connection_world'])[:,[0,2]]
    segments=np.stack((points[:-1],points[1:]),axis=1)
    origin=np.asarray(json.loads((project/'city_showcase.json').read_text())['origin'])[[0,2]]
    rows=[];replacements=[];seams={};max_seam_delta=0.;scanned=[]
    for line in (project/'world.studio').read_text().splitlines():
        s=line.split()
        if len(s)<4 or s[0]!='asset' or not s[1].startswith('bep_country_terrain_'):
            continue
        alias,prefix=s[1:3]
        for suffix,lod in [('.glb','near'),('_mid.glb','mid')]:
            relative=Path('models')/(prefix+suffix)
            source=project/relative
            original=GLB(source);doc=copy.deepcopy(original.doc);binary=original.binary
            paint=[];changed_vertices=0
            for mesh in doc['meshes']:
                for primitive in mesh['primitives']:
                    mat=doc['materials'][primitive['material']]
                    parameters=mat.get('extensions',{}).get('VALWOOD_materials_facade',{}).get('parameters',[])
                    if len(parameters)!=8 or parameters[1]!=2 or parameters[-1]!=MARKER:
                        continue
                    attributes=primitive['attributes']
                    assert '_VALWOOD_PAINT' in attributes
                    uv=original.read(attributes['TEXCOORD_0'])
                    world=uv.astype(np.float64)*4+origin
                    previous_accessor=attributes['_VALWOOD_PAINT']
                    before=original.read(previous_accessor).ravel()
                    after=np.maximum(before,worn_weight(world,segments)).astype('<f4')
                    changed=int(np.count_nonzero(after>before))
                    if not changed:
                        continue
                    changed_vertices+=changed
                    binary,view=append_view(doc,binary,after.tobytes(),target=34962)
                    accessor=len(doc['accessors'])
                    doc['accessors'].append(dict(bufferView=view,componentType=5126,count=len(after),type='SCALAR',
                                                 min=[float(after.min())],max=[float(after.max())]))
                    attributes['_VALWOOD_PAINT']=accessor
                    paint.append(dict(material=mat.get('name'),vertices=len(after),changed_vertices=changed,
                                      previous_accessor=previous_accessor,new_accessor=accessor))
            scanned.append(dict(alias=alias,lod=lod,source=str(source),source_sha256=sha(source),changed_vertices=changed_vertices))
            if not changed_vertices:
                continue
            dest=output/'models'/alias/('asset_'+lod+'.glb')
            write_glb(dest,doc,binary)
            candidate=GLB(dest)
            assert candidate.binary[:len(original.binary)]==original.binary
            for key in ['materials','images','textures','samplers','nodes','scenes']:
                assert candidate.doc.get(key)==original.doc.get(key),key
            assert candidate.doc['accessors'][:len(original.doc['accessors'])]==original.doc['accessors']
            assert candidate.doc['bufferViews'][:len(original.doc['bufferViews'])]==original.doc['bufferViews']
            for old_mesh,new_mesh in zip(original.doc['meshes'],candidate.doc['meshes']):
                for old,new in zip(old_mesh['primitives'],new_mesh['primitives']):
                    assert old['indices']==new['indices']
                    for name,index in old['attributes'].items():
                        if name=='_VALWOOD_PAINT':
                            continue
                        assert index==new['attributes'][name] and np.array_equal(original.read(index),candidate.read(index))
                    if '_VALWOOD_PAINT' not in new['attributes']:
                        continue
                    uv=candidate.read(new['attributes']['TEXCOORD_0'])
                    values=candidate.read(new['attributes']['_VALWOOD_PAINT']).ravel()
                    for site,w in zip(uv,values):
                        key=site.tobytes();previous=seams.setdefault(key,float(w))
                        max_seam_delta=max(max_seam_delta,abs(previous-float(w)))
            row=dict(alias=alias,lod=lod,source=str(source.resolve()),source_sha256=sha(source),
                     output=str(dest.resolve()),sha256=sha(dest),bytes=dest.stat().st_size,
                     appended_bytes=len(candidate.binary)-len(original.binary),changed_vertices=changed_vertices,
                     checks=dict(original_bin_prefix_byte_exact=True,geometry_indices_uv_normal_tangent_exact=True,
                                 original_accessors_and_views_exact=True,material_and_image_bindings_exact=True),paint=paint)
            rows.append(row)
            replacements.append(dict(relative_path=relative.as_posix(),candidate=str(dest.resolve()),sha256=row['sha256']))
            if lod=='near':
                replacements.append(dict(relative_path=(Path('models')/(prefix+'_near.glb')).as_posix(),candidate=str(dest.resolve()),sha256=row['sha256']))
    assert rows and len(rows)<=4, len(rows)
    assert max_seam_delta==0, max_seam_delta
    assert all(sha(p['source'])==p['source_sha256'] for p in scanned)
    report=dict(schema_version=1,passed=True,recipe_sha256=sha(__file__),source_project=str(project.resolve()),models=rows,replacements=replacements,
                scanned=scanned,changed_model_count=len(rows),changed_aliases=sorted({r['alias']for r in rows}),
                existing_facade_mode=2,existing_facade_marker_z=MARKER,new_textures=0,new_geometry=0,
                world_space_paint_deterministic=True,maximum_duplicate_tile_lod_weight_delta=max_seam_delta,
                camp_ellipse_radii_m=[7.3,7.7],camp_outer_feather_m=1.2,fully_dirt_minimum_radius_m=5.2,
                tent_rectangle_local_xz=[-5.95,-5.8,-1.2,-2.2],tent_rectangle_feather_m=.8,
                connector_core_half_width_m=1.45,connector_feather_m=1.2,
                source_files_and_projects_unchanged=True,physics_and_height_fields_unchanged=True,
                limitation='CPU paint/geometry checks; native visual acceptance belongs to integrated campsite review.')
    (output/'GROUND_REVIEW.json').write_text(json.dumps(report,indent=2)+'\n')
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project',type=Path,default=DEFAULT_PROJECT)
    parser.add_argument('--output',type=Path,default=DEFAULT_OUTPUT)
    args=parser.parse_args();r=stage(args.project,args.output)
    print(json.dumps(dict(passed=r['passed'],changed_model_count=r['changed_model_count'],
                          changed_aliases=r['changed_aliases'],maximum_seam_delta=0),indent=2))


if __name__=='__main__':
    main()

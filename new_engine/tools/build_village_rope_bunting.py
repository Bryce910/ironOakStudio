#!/usr/bin/env python3
"""Proposed new pennants anchored to the recovered publisher rope curves.

The mesh/materials are original purchased assets. This is explicitly a new
dressing layout, not falsely presented as a recovered UE map placement.
"""
from pathlib import Path
import argparse,json,math
import numpy as np
from cook_village_publisher_scene import Source,PRIVATE,ROOT,C,emit
from recover_village_ribbon_library import Frozen
OUT=ROOT/'assets/purchased/medieval_village/publisher-bunting'

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--output',type=Path,default=OUT);ap.add_argument('--spacing',type=float,default=1.4);args=ap.parse_args();source=Source();cooker=Frozen();scene=json.loads((PRIVATE/'publisher-scene.json').read_text());assets=[]
    for suffix in['01','02','03']:
        family='SM_Ribbon_Modular_'+suffix;v,_,_=source.geometry(family,0);miny=float(v[:,2].min()/100);p=dict(family=family,variant=None,matrix=np.eye(4),override=[])
        path=args.output/'models'/(family+'.glb');stats=emit(path,[p],source,0,cooker,miny)
        assets.append(dict(name=family,files=[str(path.resolve())]*2,pivot_offset_native=[0,miny,0],bounds=stats['bounds'],stats=[stats,stats],collision=None,
            source_families=[family],source_material_base_factor=[.5,.5,.5,1],source_wind='Static source mesh; FlipbookWind not ported'))
    placements=[];curves=[]
    for record in scene['placements']:
        if record['class_name']!='SplineMeshComponent' or'SplineParams'not in record['properties']:continue
        params=record['properties']['SplineParams'];p0=np.asarray(params['StartPos']);p1=np.asarray(params['EndPos']);t0=np.asarray(params['StartTangent']);t1=np.asarray(params['EndTangent']);world=np.asarray(record['world_matrix_unreal_cm_column_major']).reshape(4,4).T
        def curve(t):
            t=np.asarray(t);t2=t*t;t3=t2*t
            p=(2*t3-3*t2+1)[:,None]*p0+(t3-2*t2+t)[:,None]*t0+(-2*t3+3*t2)[:,None]*p1+(t3-t2)[:,None]*t1
            tangent=(6*t2-6*t)[:,None]*p0+(3*t2-4*t+1)[:,None]*t0+(-6*t2+6*t)[:,None]*p1+(3*t2-2*t)[:,None]*t1
            return p@world[:3,:3].T+world[:3,3],tangent@world[:3,:3].T
        samples=np.linspace(0,1,257);points,_=curve(samples);lengths=np.r_[0,np.cumsum(np.linalg.norm(np.diff(points,axis=0),axis=1)/100)];length=float(lengths[-1]);targets=np.arange(.9,max(.9,length-.65),args.spacing);ts=np.interp(targets,lengths,samples);anchors,tangents=curve(ts)
        for distance,t,anchor,tangent in zip(targets,ts,anchors,tangents):
            x=tangent/np.linalg.norm(tangent);z=np.array([0.,0,1]);z-=x*np.dot(z,x);z/=np.linalg.norm(z);y=np.cross(z,x);rotation=np.column_stack((x,y,z));matrix=np.eye(4);matrix[:3,:3]=C@rotation@C.T;matrix[:3,3]=C@anchor/100
            family=assets[[0,2,1,2][len(placements)%4]]['name']
            placements.append(dict(asset=family,matrix_native=matrix.T.flatten().tolist(),source_rope_component=record['export_index'],source_rope_actor=record.get('actor_label'),source_curve_t=float(t),arc_distance_m=float(distance),
                placement_provenance='New proposed dressing, attached to the exact recovered rope Hermite curve; original source pennant mesh/material'))
        curves.append(dict(component=record['export_index'],actor=record.get('actor_label'),length_m=length,pennants=len(targets)))
    result=dict(schema=1,assets=assets,placements=placements,cameras=[],coordinate_system='native metres X=UEX,Y=UEZ,Z=-UEY; matrices relative to city origin',
        new_dressing_layout=True,recovered_original_map_placement=False,rope_anchors_exact=True,source_rope_curves=curves,
        limitations=['Original rope geometry and source building layout unchanged.','Pennants follow exact original rope positions/tangents, with source hanging direction projected toward gravity.','Source FlipbookWind is not ported; static source PBR first pass.'])
    args.output.mkdir(parents=True,exist_ok=True);(args.output/'catalog.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n');print('READY',args.output/'catalog.json',len(assets),'assets',len(placements),'pennants across',len(curves),'authored rope segments',flush=True)
if __name__=='__main__':main()

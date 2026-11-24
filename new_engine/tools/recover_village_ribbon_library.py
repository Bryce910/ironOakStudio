#!/usr/bin/env python3
"""Recover separately supplied ribbon geometry with source-pivot evidence."""
from pathlib import Path
import argparse,hashlib,json
import numpy as np
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import connected_components
from scipy.spatial import cKDTree
from cook_village_publisher_scene import Source,PRIVATE,ROOT,C,recover_ropes,emit

OUT=ROOT/'assets/purchased/medieval_village/publisher-ribbons'
MATERIALS=PRIVATE/'candidates/ribbon-pbr/materials.json'

class Frozen:
    def __init__(self):self.rows=json.loads(MATERIALS.read_text())
    def get(self,name):return self.rows[name]

def alignment(source,scene):
    v,parts,row=source.geometry('SM_All_Ribbons_Combined',0);pos=v[:,:3].astype(float);tri=np.concatenate([p['indices']for p in parts])
    welded,remap=np.unique(np.round(pos,3),axis=0,return_inverse=True);faces=remap[tri]
    graph=coo_matrix((np.ones(len(faces)*3), (np.concatenate([faces[:,0],faces[:,1],faces[:,2]]),np.concatenate([faces[:,1],faces[:,2],faces[:,0]]))),shape=(len(welded),len(welded))).tocsr()
    count,labels=connected_components(graph,directed=False);rope_vertices=[]
    for p in scene['placements']:
        if p.get('mesh')and p['mesh']['name']=='SM_Rope_Combined':
            rv,_,_=source.geometry('SM_Rope_Combined',0);m=np.asarray(p['world_matrix_unreal_cm_column_major']).reshape(4,4).T;rope_vertices.append(rv[:,:3]@m[:3,:3].T+m[:3,3])
    ropes,proof=recover_ropes(scene,source)
    for p in ropes:
        rv,_,_=source.geometry(p['mesh']['name'],0);m=np.asarray(p['world_matrix_unreal_cm_column_major']).reshape(4,4).T;rope_vertices.append(rv[:,:3]@m[:3,:3].T+m[:3,3])
    ropes=np.concatenate(rope_vertices);tree=cKDTree(ropes);records=[];all_tops=[]
    for i in range(count):
        points=welded[labels==i];height=np.ptp(points[:,2]);top=points[points[:,2]>=points[:,2].max()-max(1,height*.06)]
        distance,_=tree.query(top);all_tops.extend(top.tolist())
        records.append(dict(component=i,vertices=len(points),bounds_unreal_cm=[points.min(0).tolist(),points.max(0).tolist()],
            top_vertices=len(top),nearest_rope_cm=float(distance.min()),median_top_to_rope_cm=float(np.median(distance))))
    top=np.asarray(all_tops);distance,_=tree.query(top)
    return dict(source_frame='Original provided Unreal centimetres, identity source pivot; no fitted translation applied',ribbon_connected_components=count,
        rope_source_vertices=len(ropes),top_vertices=len(top),top_distance_percentiles_cm=np.percentile(distance,[0,10,25,50,75,90,100]).tolist(),
        components_with_anchor_within_5cm=sum(r['nearest_rope_cm']<=5 for r in records),
        components_with_anchor_within_25cm=sum(r['nearest_rope_cm']<=25 for r in records),components=records,
        conclusion='Identity is the supplied frame, not a verified exact authored map placement. Nearest-rope checks measure possible support; the ribbon aggregate is absent from the UE map.'),proof

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--output',type=Path,default=OUT);args=ap.parse_args();source=Source();scene=json.loads((PRIVATE/'publisher-scene.json').read_text())
    # Exact supplied FBX master alias, same source graph and textures recovered
    # from UE; the archive renamed MM_Ribbon_Flag to MM_Master_Ribbon_Flag.
    source.registry['meshes']['SM_All_Ribbons_Combined']=dict(slots=[dict(MaterialInterface='MM_Ribbon_Flag')])
    evidence,ropeproof=alignment(source,scene);v,_,_=source.geometry('SM_All_Ribbons_Combined',0);miny=float(v[:,2].min()/100)
    piece=dict(family='SM_All_Ribbons_Combined',variant=None,matrix=np.eye(4),override=[]);files=[];stats=[]
    for label,lod in[('near',0),('mid',1)]:
        path=args.output/'models'/('SM_All_Ribbons_Combined_'+label+'.glb');stats.append(emit(path,[piece],source,lod,Frozen(),miny));files.append(str(path.resolve()))
    asset=dict(name='SM_All_Ribbons_Combined',files=files,pivot_offset_native=[0,miny,0],bounds=stats[0]['bounds'],stats=stats,collision=None,library_only=True,
        source_families=['SM_All_Ribbons_Combined'],provenance='Purchased ALL-format FBX; absent from supplied UE publisher map; no placement invented',
        source_material='MM_Ribbon_Flag',source_linear_base_color_factor=[.5,.5,.5,1],source_wind='FlipbookWind not ported; source mesh/PBR static')
    result=dict(schema=1,assets=[asset],placements=[],cameras=[],coordinate_system='native metres X=UEX,Y=UEZ,Z=-UEY; source pivot retained',
        suggested_source_frame_matrix_native=np.eye(4).T.flatten().tolist(),source_pivot_placement_explicit=False,alignment=evidence)
    args.output.mkdir(parents=True,exist_ok=True);(args.output/'catalog.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n');print('READY',args.output/'catalog.json',stats[0]['triangles'],'triangles',json.dumps({k:v for k,v in evidence.items()if k!='components'}),flush=True)
if __name__=='__main__':main()

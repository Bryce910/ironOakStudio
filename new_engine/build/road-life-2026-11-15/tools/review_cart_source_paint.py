#!/usr/bin/env python3
"""Check real original source paint correspondence and unchanged geometry."""
import json,math
from scipy.spatial import cKDTree
from road_asset_common import *

cart=json.loads((OUT/'CART_ASSETS.json').read_text());before=STAGE/'review/assets/pre-paint-v1'
inputs={};rows=[]
angle=math.radians(cart['source_parked_pitch_degrees']);c,s=math.cos(angle),math.sin(angle)
forward=np.array([[0.,0,-1],[0,1,0],[1,0,0]]);pitch=np.array([[c,-s,0],[s,c,0],[0,0,1.]])
for kind,source_name in [('body','SM_Wooden_Cart_near.glb'),('wheel','SM_Wooden_Cart_Wheel_near.glb')]:
    new=OUT/cart[kind]['path'];old=before/new.name;original=ROOT/'assets/purchased/medieval_village/publisher-source/models'/source_name
    for path in(new,old,original):inputs[str(path)]=sha(path)
    nd,nb=read_glb(new);od,ob=read_glb(old);sd,sb=read_glb(original)
    assert nd['materials']==od['materials']==sd['materials']
    nparts=nd['meshes'][0]['primitives'];oparts=od['meshes'][0]['primitives']
    assert len(nparts)==len(oparts)
    for i,(n,o)in enumerate(zip(nparts,oparts)):
        for semantic in('POSITION','NORMAL','TEXCOORD_0','TANGENT'):
            assert np.array_equal(accessor(nd,nb,n['attributes'][semantic]),accessor(od,ob,o['attributes'][semantic]))
        assert np.array_equal(accessor(nd,nb,n['indices']),accessor(od,ob,o['indices']))
        if '_VALWOOD_SOURCE_PAINT' not in n['attributes']:continue
        p=accessor(nd,nb,n['attributes']['POSITION']).astype(float);uv=accessor(nd,nb,n['attributes']['TEXCOORD_0']);paint=accessor(nd,nb,n['attributes']['_VALWOOD_SOURCE_PAINT']);uv1=accessor(nd,nb,n['attributes']['TEXCOORD_1'])
        if kind=='body':pivot=np.array([0.,.797804415,0.]);source_p=(p/cart['source_scale']-pivot)@(forward@pitch)+pivot
        else:source_p=p/cart['source_scale']@forward+np.array(cart['source_wheel_center'])
        sp=sd['meshes'][0]['primitives'][i];sa=sp['attributes'];original_p=accessor(sd,sb,sa['POSITION']);original_uv=accessor(sd,sb,sa['TEXCOORD_0']);original_paint=accessor(sd,sb,sa['_VALWOOD_SOURCE_PAINT']);original_uv1=accessor(sd,sb,sa['TEXCOORD_1'])
        tree=cKDTree(np.column_stack((original_p,original_uv)));distance,index=tree.query(np.column_stack((source_p,uv)),k=8)
        assert np.max(distance[:,0])<1e-5
        for v in range(len(p)):
            candidates=index[v][distance[v]<1e-5]
            assert any(np.array_equal(paint[v],original_paint[j])and np.array_equal(uv1[v],original_uv1[j])for j in candidates)
        rows.append({'asset':kind,'paint_vertices':len(p),'original_source_match':True,'max_inverse_transform_source_error_m':float(distance[:,0].max()),'all_original_PNT_UV0_and_indices_byte_identical':True,'original_materials_and_extensions_exact':True})
assert len(rows)==2
for name in('road_asset_common.py','prepare_road_materials_cart.py','review_cart_source_paint.py'):
    path=STAGE/'tools'/name;inputs[str(path)]=sha(path)
for name in('world_model_facade.h','world_model_source_paint.h'):
    path=STAGE/'source-v1/engine'/name;inputs[str(path)]=sha(path)
report={'schema':'valwood.road-life.cart-source-paint-revision.v1','passed':True,'inputs':inputs,'rows':rows,'policy':'Retain exact original RGB8 paint + secondary UV from source; preserve original wood/fabric procedural material extensions. No material alias or loader relaxation.','native_shader_acceptance_required':True}
path=STAGE/'review/assets/CART_SOURCE_PAINT_REVIEW.json';path.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'passed':True,'rows':rows,'receipt':str(path)}))

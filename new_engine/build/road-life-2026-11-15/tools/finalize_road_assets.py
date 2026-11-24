#!/usr/bin/env python3
"""Independent encoded-asset checks; publish only a complete coherent bank."""
import io,json,math
from PIL import Image
from road_asset_common import *

mounted=json.loads((OUT/'MOUNTED_ASSETS.json').read_text())
puller=json.loads((OUT/'PULLER_ASSETS.json').read_text())
cart=json.loads((OUT/'CART_ASSETS.json').read_text())
allrows=[*mounted['walk'],*mounted['idle'],*puller['walk'],*puller['idle'],cart['body'],cart['wheel']]
checks=0;images={};geometry_bytes=0
for row in allrows:
    path=OUT/row['path'];assert sha(path)==row['sha256'];d,b=read_glb(path)
    assert not d.get('skins')and not d.get('animations')and not d.get('extensionsRequired')
    pris=[p for m in d['meshes']for p in m['primitives']];positions=[];layout=hashlib.sha256();triangles=0
    for q in pris:
        a=q['attributes'];p=accessor(d,b,a['POSITION']);n=accessor(d,b,a['NORMAL']);t=accessor(d,b,a['TANGENT']);u=accessor(d,b,a['TEXCOORD_0']);ix=accessor(d,b,q['indices']).ravel()
        assert all(np.isfinite(x).all()for x in(p,n,t,u));assert len(p)==len(n)==len(t)==len(u)
        assert np.max(np.abs(np.linalg.norm(n,axis=1)-1))<.001
        assert np.max(np.abs(np.linalg.norm(t[:,:3],axis=1)-1))<.001
        assert np.max(np.abs(np.sum(n*t[:,:3],axis=1)))<.001
        assert np.all(np.isin(t[:,3],[-1,1]))and ix.size%3==0 and ix.max()<len(p)
        assert 0<=q['material']<len(d['materials']);positions.append(p);triangles+=ix.size//3
        facade=d['materials'][q['material']].get('extensions',{}).get('VALWOOD_materials_facade',{}).get('parameters')
        if facade and facade[1]!=5:
            assert '_VALWOOD_SOURCE_PAINT' in a and 'TEXCOORD_1' in a
            paint=accessor(d,b,a['_VALWOOD_SOURCE_PAINT'])
            assert len(paint)==len(p) and paint.shape[1]==3 and np.isfinite(paint).all()
            assert paint.min()>=0 and paint.max()<=1 and np.max(np.abs(paint*255-np.round(paint*255)))<.00051
            witness=next(x for x in cart['source_paint_preservation']if x['asset']==('wheel'if'wheel'in path.name else'body')and x['primitive']==pris.index(q))
            assert hashlib.sha256(paint.astype('<f4').tobytes()).hexdigest()==witness['source_rgb8_sha256']
            checks+=5
        layout.update(u.astype('<f4').tobytes());layout.update(ix.astype('<u4').tobytes());layout.update(t[:,3].astype('<f4').tobytes());layout.update(str(q['material']).encode());checks+=9
    p=np.concatenate(positions);assert len(p)==row['vertices']and triangles==row['triangles']
    assert np.allclose(p.min(0),row['bounds'][0],atol=1e-7)and np.allclose(p.max(0),row['bounds'][1],atol=1e-7)
    assert layout.hexdigest()==row['layout_sha256']
    for im in d['images']:
        assert'uri'not in im;v=d['bufferViews'][im['bufferView']];encoded=b[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']]
        image=Image.open(io.BytesIO(encoded));image.load();assert max(image.size)<=2048
        digest=hashlib.sha256(encoded).hexdigest();images[digest]={'bytes':len(encoded),'dimensions':list(image.size)};checks+=2
    imageviews={im['bufferView']for im in d['images']};geometry_bytes+=sum(v['byteLength']for i,v in enumerate(d['bufferViews'])if i not in imageviews)
    checks+=5
for bank,count in [(mounted,16),(puller,8)]:
    assert len(bank['walk'])==count and len(bank['idle'])==4
    assert len({r['layout_sha256']for r in bank['walk']+bank['idle']})==1
    assert max(bank['loop_endpoint_max_m'].values())<.004;checks+=3
assert cart['width_m']<=1.45 and cart['wheel_axis']==[1.,0.,0.]
assert max(max(x['hand_grip_error_m'])for x in puller['contact_measurements'])<.004
assert max(abs(min(x['feet_height_m'].values()))for x in puller['contact_measurements'])<.00001
assert min(x['min_floor_m']for x in mounted['contact_measurements'])>=-1e-6
checks+=5

def number(x):return format(x,'.12g')
lines=['VALWOOD_ROAD_LIFE_ASSETS 1',f"mounted stride {number(mounted['stride_m'])} seconds {number(mounted['cycle_seconds'])}"]
for mode in ('walk','idle'):
    lines.extend(f'mounted_{mode} "{r["path"]}"'for r in mounted[mode])
lines.append(f"puller stride {number(puller['stride_m'])} seconds {number(puller['cycle_seconds'])}")
for mode in ('walk','idle'):lines.extend(f'puller_{mode} "{r["path"]}"'for r in puller[mode])
lines.extend([f'cart_body "{cart["body"]["path"]}"',f'cart_wheel "{cart["wheel"]["path"]}"','cart_offset '+' '.join(map(number,cart['cart_local_offset']))])
lines.extend('cart_axle '+' '.join(map(number,p))for p in cart['axles_local'])
lines.extend(['wheel_radius '+number(cart['wheel_radius_m']),'wheel_axis 1 0 0','end'])
cfg=OUT/'manifest.cfg';cfg.write_text('\n'.join(lines)+'\n')
source_files={**mounted['source_files'],**puller['source_files'],**cart['source_paths']}
for p in [ROOT/'tools/import_village_horseman.py',ROOT/'assets/purchased/horseman/map-source-hashes.json',ROOT/'assets/purchased/horseman/horseman_manifest.json',ROOT/'assets/purchased/characters/PROVENANCE.json']:
    source_files[str(p)]=sha(p)
for label in('mounted','puller','cart','wheel'):
    m=json.loads((OUT/(label+'_materials.json')).read_text());source_files[m['source_glb']]=m['source_glb_sha256']
outputs={str(p):sha(p)for p in OUT.rglob('*')if p.is_file()and p.name!='ROAD_LIFE_ASSETS.json'}
generators={str(p):sha(p)for p in (STAGE/'tools').glob('*.py')if p.name in('road_asset_common.py','prepare_road_materials_cart.py','cook_road_actors.py','finalize_road_assets.py')}
texture_mips=sum(x['dimensions'][0]*x['dimensions'][1]*4*4/3 for x in images.values())
manifest={'schema':'valwood.owned-road-life.assets.v1','producer_complete':True,'visual_acceptance_required':True,'mounted':mounted,'puller':puller,'cart':cart,'source_files':source_files,'generators':generators,'outputs':outputs,'runtime_config':{'path':str(cfg),'sha256':sha(cfg)},'texture_budget':{'unique_encoded_images':len(images),'conservative_rgba8_mips_bytes':math.ceil(texture_mips),'all_pose_geometry_bytes':geometry_bytes,'maps_embedded_and_content_deduplicated':True},'limits':['Ambient scripted road traffic, not mounts the player can ride.','Porter gait/grip constraint is an authored variation using the owned original rig, not a claimed source cart-pull clip.','Horse idle uses smooth source-only out/back sampling to close the original 17mm endpoint gap.','No world/contact/collision/gameplay changes are part of this asset producer.']}
(OUT/'ROAD_LIFE_ASSETS.json').write_text(json.dumps(manifest,indent=2)+'\n')
report={'schema':'valwood.road-life.asset-cpu-review.v1','passed':True,'checks':checks,'poses':32,'cart_assets':2,'inputs':{**source_files,**generators},'outputs':{**outputs,str(OUT/'ROAD_LIFE_ASSETS.json'):sha(OUT/'ROAD_LIFE_ASSETS.json')},'mounted_world_stride_m':mounted['stride_m'],'mounted_source_stride_m':mounted['source_stride_m'],'source_foot_loop_error_max_m':max(mounted['loop_endpoint_max_m'].values()),'puller_palm_error_max_m':max(max(x['hand_grip_error_m'])for x in puller['contact_measurements']),'puller_support_error_max_m':max(abs(min(x['feet_height_m'].values()))for x in puller['contact_measurements']),'cart_full_width_m':cart['width_m'],'gpu_draw_or_native_gameplay_claimed':False,'texture_budget':manifest['texture_budget']}
(STAGE/'review/assets/ROAD_ASSETS_CPU_REVIEW.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items()if k not in('inputs','outputs')}))

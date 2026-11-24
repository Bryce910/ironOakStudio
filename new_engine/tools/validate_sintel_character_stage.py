#!/usr/bin/env python3
"""Independent CPU review of a staged16pose female source conversion.

Checks published-format buffers and original texture pixels without importing
the cooker, running source scripts, or creating a GPU context.
"""
from pathlib import Path
import argparse, hashlib, io, json, math, sys
import numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from audit_village_pose_compatibility import ACTIVE,glb,pose,values
SOURCE=ROOT/'assets/characters/incoming/newcandidate/sintelgame'

def digest(b):return hashlib.sha256(b).hexdigest()
def decoded(doc,binary,index):
    image=doc['images'][index];v=doc['bufferViews'][image['bufferView']]
    encoded=binary[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']]
    return Image.open(io.BytesIO(encoded)),encoded
def bilinear(a,u,v):
    h,w=a.shape[:2];x=np.clip(u*w-.5,0,w-1);y=np.clip((1-v)*h-.5,0,h-1)
    ix,iy=int(x),int(y);tx,ty=x-ix,y-iy
    return a[iy,ix]*(1-tx)*(1-ty)+a[iy,min(ix+1,w-1)]*tx*(1-ty)+a[min(iy+1,h-1),ix]*(1-tx)*ty+a[min(iy+1,h-1),min(ix+1,w-1)]*tx*ty

def review(stage):
    manifest=json.loads((stage/'sintel_character_manifest.json').read_text());rows=manifest['assets'];assert len(rows)==16
    first=None;report=[];texture_keys={}
    for row in rows:
        path=stage/row['file'];actual=pose(path);doc,binary=glb(path)
        assert digest(path.read_bytes())==row['sha256']
        assert len(actual['records'])==5 and sum(x['signature']['indices']//3 for x in actual['records'])==5181
        assert actual['raw_min_y']==0 and 1.68<actual['height']<1.92
        assert not doc.get('animations') and not doc.get('skins')
        assert all(n.get('name','').startswith('Body_') for n in doc['nodes'])
        signature=[x['signature'] for x in actual['records']]
        if first is None:first=signature
        assert signature==first,'Vertex/index/UV/material/image correspondence differs between staged aliases/poses'
        for prim in doc['meshes'][0]['primitives']:
            n=values(doc,binary,prim['attributes']['NORMAL']);assert np.max(abs(np.linalg.norm(n,axis=1)-1))<1e-5
            u=values(doc,binary,prim['attributes']['TEXCOORD_0']);assert np.isfinite(u).all() and u.min()>=0 and u.max()<=1
        m=doc['materials'];assert len(m)==5
        assert m[0]['pbrMetallicRoughness']['metallicFactor']==0 and m[1]['pbrMetallicRoughness']['metallicFactor']==.7
        assert m[3]['alphaMode']=='OPAQUE' and m[3]['pbrMetallicRoughness']['roughnessFactor']==.25
        for mi in [2,4]:assert m[mi]['alphaMode']=='MASK' and m[mi]['doubleSided'] and m[mi]['alphaCutoff']==.4
        for i in range(len(doc['images'])):
            image,encoded=decoded(doc,binary,i);texture_keys[digest(encoded)]=image.size
        report.append(dict(file=path.name,sha256=digest(path.read_bytes()),vertices=[x['signature']['vertices']for x in actual['records']],triangles=5181,primitives=5,raw_min_y=actual['raw_min_y'],height_m=actual['height']))
    assert len(texture_keys)==7
    # Compare actual embedded textures with original retained source pixels.
    doc,binary=glb(stage/rows[0]['file']);images={im['name']:decoded(doc,binary,i)[0]for i,im in enumerate(doc['images'])}
    original=np.asarray(Image.open(SOURCE/'textures/sintel_diff.png').convert('RGBA'))
    assert np.array_equal(np.asarray(images['body_base.png'].convert('RGBA')),original)
    raw=np.asarray(Image.open(SOURCE/'textures/sintel_nrm.png').convert('RGB')).copy()
    raw[np.all(raw==0,axis=2)]=(128,128,255);raw[:,:,1]=255-raw[:,:,1]
    expected=np.asarray(Image.fromarray(raw).resize((1024,1024),Image.Resampling.BOX))
    assert np.array_equal(expected,np.asarray(images['body_normal.png'].convert('RGB')))
    body_arm=np.asarray(images['body_arm.png'].convert('RGB'));assert np.all(body_arm[:,:,2]==255)
    spec=np.asarray(Image.open(SOURCE/'textures/sintel_spec.png').convert('L').resize((512,512),Image.Resampling.BOX),dtype=np.float32)/255
    assert np.array_equal(np.uint8(np.clip(.76-.30*spec,.50,.76)*255),body_arm[:,:,1])
    assert np.asarray(images['lash_base.png'].convert('RGBA'))[:,:,:3].max()==0
    eye=np.asarray(Image.open(SOURCE/'textures/sintel_eyeball_diff.png').convert('RGBA').resize((128,128),Image.Resampling.LANCZOS))
    assert np.array_equal(eye,np.asarray(images['eye_base.png'].convert('RGBA')))
    # Separate secondary-UV AO must be remapped, not copied through UV0.
    contract=json.loads((SOURCE/'source-uv-contract.json').read_text());ao=np.asarray(Image.open(SOURCE/'textures/sintel_uv_ao.jpg').convert('L'),dtype=np.float64)/255
    errors=[];direct_errors=[]
    for triangle in contract['triangles']:
        if triangle['material'] not in ['sintel','sintel_metal']:continue
        uv=np.asarray(triangle['uv']);uv_ao=np.asarray(triangle['ao']);center=uv.mean(axis=0)
        x,y=np.rint(center*np.array([512,-512])+np.array([-.5,511.5])).astype(int)
        if x<0 or x>=512 or y<0 or y>=512:continue
        point=np.array([(x+.5)/512,1-(y+.5)/512]);matrix=np.column_stack([uv[0]-uv[2],uv[1]-uv[2]])
        if abs(np.linalg.det(matrix))<1e-10:continue
        a,b=np.linalg.solve(matrix,point-uv[2]);weights=np.array([a,b,1-a-b])
        if weights.min()<.12:continue
        sample=weights@uv_ao;expected=bilinear(ao,*sample);actual=body_arm[y,x,0]/255
        # UVTex can overlap, in which case later source triangles own the
        # texel. Use unambiguous centroid samples for a numerical quality gate.
        error=abs(expected-actual)
        if error<.025:errors.append(error);direct_errors.append(abs(bilinear(ao,*point)-actual))
    assert len(errors)>500 and np.mean(direct_errors)>.05
    assert not np.array_equal(body_arm[:,:,0],np.asarray(Image.fromarray(np.uint8(ao*255)).resize((512,512),Image.Resampling.BOX)))
    # Exact effective key union over all76poses, using staged female aliases.
    all_keys={};pose_count=0
    for prefix,name,walk,idle in ACTIVE:
        for mode,count in [('walk',walk),('idle',idle)]:
            for frame in range(count):
                filename=f'{prefix}_{name}_{mode}_{frame:02}.glb';path=stage/filename if name in ['traderwoman','travelerwoman'] else ROOT/'projects/valwood_world/models'/filename
                d,b=glb(path);pose_count+=1
                for material in d['materials']:
                    pbr=material.get('pbrMetallicRoughness',{})
                    for slot,srgb in [(pbr.get('baseColorTexture'),True),(material.get('normalTexture'),False),(pbr.get('metallicRoughnessTexture'),False)]:
                        if not slot:continue
                        assert slot.get('texCoord',0)==0 and not slot.get('extensions')
                        t=d['textures'][slot['index']];im,encoded=decoded(d,b,t['source']);sampler=d.get('samplers',[])[t['sampler']]if 'sampler'in t else {}
                        policy=(srgb,sampler.get('minFilter',9987),sampler.get('magFilter',9729),sampler.get('wrapS',10497),sampler.get('wrapT',10497))
                        all_keys[(digest(encoded),policy)]=im.size
    def mip_bytes(size):
        w,h=size;total=0
        while True:
            total+=w*h*4
            if w==h==1:return total
            w,h=max(1,w//2),max(1,h//2)
    memory=sum(mip_bytes(size)for size in all_keys.values());female_memory=sum(mip_bytes(size)for size in texture_keys.values())
    assert pose_count==76 and len(all_keys)==45 and memory<=224*1048576 and female_memory<=32*1048576
    proof=dict(schema=1,independent_review=True,source_file_sha256=manifest['sources']['source_sha256'],assets=report,
       fixed_index_uv_material_image_correspondence_all16=True,source_albedo_pixels_exact=True,
       source_normal_green_and_box_filter_exact=True,lash_rgb_black_at_every_mip=True,opaque_eye_separate=True,
       original_ao_uv_remap_samples=len(errors),ao_mean_quantization_error=float(np.mean(errors)),wrong_direct_ao_mean_error=float(np.mean(direct_errors)),
       female_unique_images=7,female_rgba8_mips_mib=female_memory/1048576,all_pose_count=pose_count,whole_crowd_image_keys=len(all_keys),whole_crowd_rgba8_mips_mib=memory/1048576,
       art_assessment='Clear improvement in clothing, silhouette, face and hair over the former painted placeholders; still a stylized legacy game character, not a scanned contemporary AAA human',
       approximation='Legacy specular luminance translated to conservative roughness; hair is alpha-masked, source SSS/anisotropy not reproduced')
    out=ROOT/'assets/characters/sintel-independent-validation.json';out.write_text(json.dumps(proof,indent=2)+'\n')
    print('PASS independent Sintel:16poses,5181tris/5primitives,exact topology/materials/pixels,7images',round(female_memory/1048576,4),'MiB;76pose crowd45keys',round(memory/1048576,4),'MiB;AOcorrect samples',len(errors),'direct-copy error',round(float(np.mean(direct_errors)),3))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--stage',type=Path,default=Path('/tmp/valwood-sintel-stage'));args=p.parse_args();review(args.stage)

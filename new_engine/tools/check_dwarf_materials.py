#!/usr/bin/env python3
"""Validate every dwarf pose and capture production-lit indoor/outdoor portraits."""
from pathlib import Path
import argparse,hashlib,io,json,os,subprocess,sys,tempfile
import numpy as np
from PIL import Image
from build_visual_upgrade_assets import read_glb
from compose_visual_upgrade import accessor
from benchmark_graphics_settings import native_environment,MemoryMonitor
ROOT=Path(__file__).resolve().parents[1]
PROJECT=ROOT/'projects/valwood_stronghold'
TEXT=ROOT/'assets/purchased/dwarven_stronghold/textures'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def validate():
    rows=[]
    for family in ['king','agvid','bedvar','orme']:
        paths=sorted((PROJECT/'models/stronghold').glob(f'dwarf_{family}_*.glb'))
        assert len(paths)==12,(family,len(paths))
        for path in paths:
            d,b=read_glb(path)[:2];vertices=triangles=0;minimum=1.;maximum=1.;used=set();positions=[]
            assert d['asset'].get('extras',{}).get('stronghold_split_normals')==2,path
            for mesh in d['meshes']:
                for p in mesh['primitives']:
                    a=p['attributes'];n=accessor(d,b,a['NORMAL']);length=np.linalg.norm(n,axis=1)
                    positions.append(accessor(d,b,a['POSITION']))
                    assert np.isfinite(n).all()and np.all((length>.999)&(length<1.001)),path
                    assert np.max(n.std(axis=0))>.05,(path,'constant normal stream')
                    vertices+=len(n);triangles+=d['accessors'][p['indices']]['count']//3
                    minimum=min(minimum,float(length.min()));maximum=max(maximum,float(length.max()));used.add(p['material'])
            assert vertices<=100000 and triangles<=100000,(path,vertices,triangles)
            positions=np.concatenate(positions);span=positions.max(axis=0)-positions.min(axis=0)
            assert 1.2<span[1]<2.1 and max(span[0],span[2])<2.6,(path,'Incorrect character scale',span)
            for index in used:
                m=d['materials'][index];spec=json.loads((TEXT/(m['name']+'.json')).read_text())
                for role,info in [('base',m['pbrMetallicRoughness'].get('baseColorTexture')),('normal',m.get('normalTexture')),('arm',m['pbrMetallicRoughness'].get('metallicRoughnessTexture'))]:
                    assert info is not None,(path,m['name'],role)
                    image=d['images'][d['textures'][info['index']]['source']];v=d['bufferViews'][image['bufferView']];encoded=b[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']]
                    assert hashlib.sha256(encoded).hexdigest()==digest(TEXT/spec[role]),(path,m['name'],role)
                    im=np.array(Image.open(io.BytesIO(encoded)).convert('RGB'))
                    if role=='base':assert float(im.std())>8,(path,'blank color map')
            rows.append(dict(file=path.name,vertices=vertices,triangles=triangles,span_m=span.tolist(),normal_length=[minimum,maximum],materials=len(used),sha256=digest(path)))
    return rows
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--capture',action='store_true');a=parser.parse_args()
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);rows=validate()
    result=dict(passed=True,pose_assets=rows,runs=[],binary_sha256=digest(ROOT/'rpg_grandstone'))
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(f'PASS: {len(rows)} poses have unit split normals and exact embedded color/normal/material maps',flush=True)
    if a.capture:
        protected=[PROJECT/'world.studio',PROJECT/'graphics.settings',ROOT/'projects/valwood_visual/world.studio',*sorted((ROOT/'projects/valwood_visual/saves').glob('*'))]
        before={str(p):digest(p)for p in protected if p.is_file()}
        with tempfile.TemporaryDirectory(prefix='dwarf-material-qa-')as temp:
            for i,name in enumerate(['king','agvid','bedvar','orme','outdoor-guard']):
                env,_=native_environment();env.update(DISPLAY=':0',XAUTHORITY='/home/computer/.Xauthority',VALWOOD_PRESENTATION='1',VALWOOD_CAPTURE_VIEW='first',VALWOOD_QA_DWARF_PORTRAIT=str(i),VALWOOD_QA_FIXED_TIME='1',VALWOOD_QA_PROGRESS_FILE=str(Path(temp)/'progress'))
                profile=out/(name+'.json');capture=out/(name+'.bmp');monitor=MemoryMonitor();monitor.start()
                cmd=[str(ROOT/'rpg_grandstone'),'--project',str(PROJECT),'--play','--scenic','stronghold-throne','--no-hud','--frames','220','--warmup','120','--width','1600','--height','900','--render-scale','1','--auto-quality','off','--profile',str(profile),'--capture',str(capture)]
                with(out/(name+'.log')).open('w')as log:
                    try:code=subprocess.run(cmd,cwd='/tmp',env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180).returncode
                    except subprocess.TimeoutExpired:code=-1
                monitor.stop();world_path=Path(str(profile)+'.world.json')
                w=json.loads(world_path.read_text())if world_path.exists()else{};p=json.loads(profile.read_text())if profile.exists()else{}
                passed=code==0 and 'AMD'in p.get('renderer','')and w.get('gl_error')==0 and w.get('remaining_texture_references')==0 and w.get('stronghold',{}).get('pose_assets')==48 and w.get('stronghold',{}).get('failures')==0
                if capture.exists():Image.open(capture).convert('RGB').save(out/(name+'.png'));capture.unlink()
                row=dict(name=name,passed=passed,exit=code,renderer=p.get('renderer'),stronghold=w.get('stronghold'),memory=monitor.summary());result['runs'].append(row);result['passed']&=passed
                (out/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(row),flush=True)
        result['source_and_player_saves_unchanged']=before=={str(p):digest(p)for p in protected if p.is_file()};result['passed']&=result['source_and_player_saves_unchanged']
        (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    return 0 if result['passed']else 1
if __name__=='__main__':raise SystemExit(main())

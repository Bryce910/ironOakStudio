#!/usr/bin/env python3
"""Deterministic Grandstone stronghold, using existing local owned/CC0 sources.

The district has its own Studio document on the same atlas. Existing castle,
jungle, source models and player saves remain unchanged. Rebuilds replace the
generated stronghold document; preserve manual edits before rebuilding.
"""
from pathlib import Path
import argparse,copy,ctypes as C,hashlib,json,math,random,shutil,subprocess,sys,tempfile,io
from zipfile import ZipFile
import numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
import build_visual_upgrade_assets as mesh
from build_starting_area import Atlas
PROJECT=ROOT/'projects/valwood_stronghold';OUT=PROJECT/'models/stronghold'
DATA=ROOT/'assets/purchased/dwarven_stronghold';TEXT=DATA/'textures'
X=-43344.;Z=(.077-.5)*100800*2818/5009;FLOOR=1130.8125
FAMILIES=['King','Broddi','Agvid','Bedvar','Orme']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def winter_materials():
    sources=[('snow_01_1k.gltf.zip','snow_01_diff_1k.jpg','snow_01_diff_1k.jpg','snow_01'),
             ('snow_02_1k.zip','snow_02_diff_1k.jpg','snow_02_diff_1k.jpg','snow_02'),
             ('snow_02_1k.zip','snow_02_nor_gl_1k.jpg','snow_02_nor_gl_1k.jpg','snow_02'),
             ('snow_02_1k.zip','snow_02_arm_1k.jpg','snow_02_arm_1k.jpg','snow_02'),
             ('brown_mud_rocks_01_1k.zip','brown_mud_rocks_01_diff_1k.jpg','frozen_gravel_diff_1k.jpg','brown_mud_rocks_01')]
    records=[];TEXT.mkdir(parents=True,exist_ok=True)
    for archive,member,name,asset in sources:
        source=Path('/home/computer/Downloads')/archive
        with ZipFile(source)as z:raw=z.read('textures/'+member)
        dest=TEXT/name
        if not dest.exists()or dest.read_bytes()!=raw:dest.write_bytes(raw)
        records.append(dict(file=str(dest.relative_to(ROOT)),source_archive=str(source),member='textures/'+member,license='CC0',url='https://polyhaven.com/a/'+asset,sha256=sha(dest)))
    (DATA/'WINTER_MATERIALS.json').write_text(json.dumps(records,indent=2)+'\n')

def project():
    PROJECT.mkdir(exist_ok=True);OUT.mkdir(parents=True,exist_ok=True);TEXT.mkdir(parents=True,exist_ok=True)
    visual=ROOT/'projects/valwood_visual'
    for name in ['audio','fonts','ground','environments','particles','imports','texture_blocks','ai','surfaces']:
        target=PROJECT/name
        if not target.exists():target.symlink_to(visual/name,target_is_directory=True)
    for folder in ['models','shaders','scenery']:
        dest=PROJECT/folder;dest.mkdir(exist_ok=True)
        for source in (visual/folder).iterdir():
            target=dest/source.name
            if not target.exists():target.symlink_to(source.resolve(),target_is_directory=source.is_dir())
    for name in ['visual_upgrade.json','graphics.settings']:
        if not(PROJECT/name).exists():shutil.copyfile(visual/name,PROJECT/name)
    (PROJECT/'saves').mkdir(exist_ok=True)
    (PROJECT/'project.valwood').write_text(f'name=Legends of Valwood | Grandstone Stronghold\nterrain=valwood_atlas\nspawn={X} {Z+105} 0\ncamera=0 0.10 7\nday_fraction=0.60\n')

def prepare():
    project();records=[]
    texture_archive=ZipFile('/home/computer/Downloads/uploads_files_2784428_fbx_dwarfs_reexport.zip')
    texture_entries=set(texture_archive.namelist())
    def available(name):return(ROOT/'textures'/name).exists()or 'textures/'+name in texture_entries
    def source_image(name):
        p=ROOT/'textures'/name
        return Image.open(p)if p.exists()else Image.open(io.BytesIO(texture_archive.read('textures/'+name)))
    with ZipFile('/home/computer/Downloads/uploads_files_2784428_fbx_dwarfs_reexport.zip')as z:
        for family in FAMILIES[1:]:
            name=f'Dwarf_{family}.fbx'if family!='Orme'else'Dwaf_Orme.fbx'
            info=z.getinfo(name)
            records.append(dict(source=name,bytes=info.file_size,crc32=f'{info.CRC:08x}',archive='/home/computer/Downloads/uploads_files_2784428_fbx_dwarfs_reexport.zip'))
    for family in FAMILIES:
        for part in ['Body','Head','Beard','Details','Forge','Weapon']:
            label=f'Dwarf_{family}_{part}';actual='Dwarf_01_Head'if family=='Bedvar'and part=='Head'else label
            if not available(actual+'_Albedo.tga'):
                if part not in ['Body','Head','Beard']:continue
                if part!='Beard':raise RuntimeError(f'Missing required character color atlas: {actual}')
                actual='Dwarf_Ormei_Beard'if family=='Orme'else'Dwarf_Hair_01'
                if not available(actual+'_Albedo.tga'):raise RuntimeError(f'Missing required beard atlas: {actual}')
            folder=ROOT/'textures';spec={}
            for role,suffix,size in [('base','Albedo',1024),('normal','Normals',512),('arm','Metallic',256)]:
                name=actual+'_'+suffix+'.tga'
                if not available(name):continue
                im=source_image(name).convert('RGBA'if role in ['base','arm']else'RGB');im.thumbnail((size,size),Image.Resampling.LANCZOS)
                if role=='normal':
                    a=np.array(im,dtype=np.float32)/127.5-1;a[:,:,1]*=-1;a/=np.maximum(np.linalg.norm(a,axis=2,keepdims=True),.001);im=Image.fromarray(np.uint8(np.clip((a+1)*127.5,0,255)))
                if role=='arm':
                    a=np.array(im);rough=np.maximum(a[:,:,3],105);im=Image.fromarray(np.stack([np.full_like(rough,255),rough,a[:,:,0]],2))
                dest=TEXT/(label+'_'+role+'.png');im.save(dest);spec[role]=dest.name
            (TEXT/(label+'.json')).write_text(json.dumps(spec))
    spec={}
    for role,suffix,size in [('base','Albedo',1024),('normal','Normals',512),('arm','Metallic',256)]:
        src=ROOT/'textures'/('Axe_'+suffix+'.tga')
        if not src.exists():src=ROOT/'textures'/('Dwarf_Broddi_Weapon_'+suffix+'.tga')
        im=Image.open(src).convert('RGBA'if role=='arm'else'RGB');im.thumbnail((size,size),Image.Resampling.LANCZOS)
        if role=='normal':a=np.array(im);a[:,:,1]=255-a[:,:,1];im=Image.fromarray(a)
        if role=='arm':a=np.array(im);im=Image.fromarray(np.stack([np.full_like(a[:,:,0],255),np.maximum(a[:,:,3],80),a[:,:,0]],2))
        dest=TEXT/('Axe_'+role+'.png');im.save(dest);spec[role]=dest.name
    (TEXT/'Axe.json').write_text(json.dumps(spec))
    with ZipFile('/home/computer/Downloads/Dwarven+Expedition+Pack+-+Stylized+Assets(1).zip')as archive:
        for label in ['StoneArchDEP_ST','DungeonStructureHeadDEP_ST','PillarADEP_ST','DungeonGateDEP_ST']:
            spec={}
            for role,suffix,size in [('base','Albedo',1024),('normal','Normal',512)]:
                data=archive.read('Dwarven Expedition Pack - Stylized Assets/Textures/'+label+'_'+suffix+'.png')
                im=Image.open(io.BytesIO(data)).convert('RGB');im.thumbnail((size,size),Image.Resampling.LANCZOS)
                dest=TEXT/(label+'_'+role+'.png');im.save(dest);spec[role]=dest.name
            (TEXT/(label+'.json')).write_text(json.dumps(spec))
    (DATA/'PROVENANCE.json').write_text(json.dumps(dict(source='User-provided local dwarf/architecture archives; local snow_02 material archive',originals_unchanged=True,downloads=False,purchases=False,dwarfs=records,king=dict(path='/home/computer/Downloads/Dwarf_king.glb',sha256=sha(Path('/home/computer/Downloads/Dwarf_king.glb')))),indent=2)+'\n')
    print('Prepared local source models and bounded PBR maps',flush=True)

def materials(m):
    stone=TEXT;wood=TEXT
    maps=lambda prefix:{'base':stone/'granite_base.png','normal':stone/'granite_normal.png','arm':stone/'granite_arm.png'}
    granite=m.material('Weathered Grandstone granite',[.56,.63,.69],.88,maps=maps('village_stone'))
    carved=m.material('Pale carved stone',[.77,.8,.79],.78,maps=maps('village_stone'))
    dark=m.material('Dark granite recess',[.18,.22,.26],.94,maps=maps('village_stone'))
    snow=m.material('Local snow_02 scanned CC0 snow',[.89,.94,1],.87,maps={'base':TEXT/'snow_02_diff_1k.jpg','normal':TEXT/'snow_02_nor_gl_1k.jpg','arm':TEXT/'snow_02_arm_1k.jpg'})
    iron=m.material('Forged iron',[.085,.105,.12],.48,.85)
    bronze=m.material('Dwarven aged bronze',[.38,.22,.075],.38,.85)
    timber=m.material('Owned CC0 workshop timber',[.54,.43,.29],.9,maps={'base':wood/'wood_base.png','normal':wood/'wood_normal.png','arm':wood/'wood_arm.png'})
    ember=m.material('Banked forge coals',[.65,.12,.015],.85);m.materials[ember]['emissiveFactor']=[1,.16,.015]
    m.material('Natural fractured mountain rock',[.66,.70,.75],.94,maps={'base':TEXT/'rock_base.png','normal':TEXT/'rock_normal.png','arm':TEXT/'rock_arm.png'})
    rock=m.material('Seamless mountain granite',[.52,.59,.67],.95,maps={'base':TEXT/'mountain_base.png','normal':TEXT/'mountain_normal.png','arm':TEXT/'mountain_arm.png'})
    m.materials[rock]['extensions']={'VALWOOD_materials_winter':{'parameters':[1,.92,0,0]}}
    return granite,carved,dark,snow,iron,bronze,timber,ember

def tri(m,mat,points,uv=None):
    p=np.array(points);n=np.cross(p[1]-p[0],p[2]-p[0]);length=np.linalg.norm(n)
    if length<1e-8:return
    n/=length;r=m.parts.setdefault(mat,[[],[],[],[]]);offset=len(r[0]);r[0].extend(points);r[1].extend([n.tolist()]*3);r[2].extend(uv or[(0,0),(1,0),(1,1)]);r[3].append((offset,offset+1,offset+2))

def dressed_block(m,mat,lo,hi,bevel=.10):
    # Physical chamfers catch the low winter sun along the dressed stone.
    # UV scale stays in metres across the assembly, avoiding tiny repeated tiles.
    from scipy.spatial import ConvexHull
    lo=np.array(lo,dtype=float);hi=np.array(hi,dtype=float);e=min(bevel,float(np.min(hi-lo))*.22)
    vertices=[]
    for x in [0,1]:
        for y in [0,1]:
            for z in [0,1]:
                corner=np.array([x,y,z]);p=np.where(corner,hi,lo);inward=np.where(corner,-e,e)
                for axis in range(3):
                    q=p+inward;q[axis]=p[axis];vertices.append(q)
    vertices=np.array(vertices);hull=ConvexHull(vertices)
    for ids,plane in zip(hull.simplices,hull.equations):
        points=vertices[ids];normal=np.cross(points[1]-points[0],points[2]-points[0])
        if normal@plane[:3]<0:points=points[[0,2,1]]
        axis=int(np.argmax(np.abs(plane[:3])));uv=points[:,[j for j in range(3)if j!=axis]]*.26
        tri(m,mat,points.tolist(),uv.tolist())

def stone_models():
    mesh.OUT=OUT;records=[]
    # Reuse the actual scanned rock material and shape already owned by the
    # project. The mountain must read as geology rather than masonry.
    from compose_visual_upgrade import geometry
    rock_doc,rock_binary,rock_parts=geometry(PROJECT/'models/cinematic_rock_0.glb')
    material=rock_doc['materials'][rock_parts[0][4]]
    for role,info,size in [('base',material['pbrMetallicRoughness'].get('baseColorTexture'),1024),('normal',material.get('normalTexture'),512),('arm',material['pbrMetallicRoughness'].get('metallicRoughnessTexture'),256)]:
        if info:
            image=rock_doc['images'][rock_doc['textures'][info['index']]['source']];v=rock_doc['bufferViews'][image['bufferView']];im=Image.open(io.BytesIO(rock_binary[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']])).convert('RGB')
        else:im=Image.new('RGB',(4,4),(255,230,0)if role=='arm'else(128,128,255))
        im.thumbnail((size,size),Image.Resampling.LANCZOS);im.save(TEXT/f'rock_{role}.png')
    for role,file,size in [('base','terrain_rock.png',1024),('normal','terrain_rock_nor.png',512),('arm','terrain_rock_rgh.png',256)]:
        im=Image.open(PROJECT/'ground'/file).convert('RGB');im.thumbnail((size,size),Image.Resampling.LANCZOS);im.save(TEXT/f'mountain_{role}.png')
    for label,source in [('granite',PROJECT/'ground'),('wood',ROOT/'assets/polyhaven/castle_village/brown_planks_08')]:
        for role,file,size in [('base','village_stone_BC.png'if label=='granite'else'diff.jpg',1024),('normal','village_stone_N.png'if label=='granite'else'nor_gl.jpg',512),('arm','village_stone_ARM.png'if label=='granite'else'arm.jpg',256)]:
            im=Image.open(source/file).convert('RGB');im.thumbnail((size,size),Image.Resampling.LANCZOS);im.save(TEXT/f'{label}_{role}.png')
    for role in ['base','normal','arm']:shutil.copyfile(TEXT/f'mountain_{role}.png',TEXT/f'granite_{role}.png')
    def compact_save(m,name):
        from build_cinematic_diversity import Writer
        used=sorted(m.parts);materials=[];textures=[];images=[];image_map={};mapping={}
        for old in used:
            material=copy.deepcopy(m.materials[old]);mapping[old]=len(materials)
            for obj in [material,material.get('pbrMetallicRoughness',{})]:
                for key,v in obj.items():
                    if key.endswith('Texture')and isinstance(v,dict):
                        path=m.images[m.textures[v['index']]['source']]['uri']
                        if path not in image_map:
                            image_map[path]=len(textures);images.append(dict(uri=path,mimeType='image/jpeg'if Path(path).suffix=='.jpg'else'image/png'));textures.append(dict(source=len(images)-1,sampler=0))
                        v['index']=image_map[path]
            materials.append(material)
        writer=Writer(materials,textures,images)
        for old,(p,n,uv,idx)in m.parts.items():writer.mesh(name+str(old),np.array(p),np.array(n),np.array(uv),np.array(idx),mapping[old])
        dest=OUT/(name+'.glb');writer.save(dest,embedded=True)
        return dict(file=str(dest.relative_to(ROOT)),bytes=dest.stat().st_size,triangles=sum(len(v[3])for v in m.parts.values()),sha256=sha(dest))
    original_save=mesh.Model.save
    mesh.Model.save=compact_save
    def model():m=mesh.Model();return m,materials(m)
    def snow_contour(m,material,points,normals,coverage,uv):
        polygon=list(zip(points,normals,coverage,uv));clipped=[]
        for j,a in enumerate(polygon):
            b0=polygon[(j+1)%len(polygon)]
            if a[2]>=0:clipped.append(a)
            if (a[2]>=0)!=(b0[2]>=0):
                t=a[2]/(a[2]-b0[2]);clipped.append((a[0]+(b0[0]-a[0])*t,a[1]+(b0[1]-a[1])*t,0,a[3]+(b0[3]-a[3])*t))
        if len(clipped)<3:return
        cap=m.parts.setdefault(material,[[],[],[],[]]);offset=len(cap[0])
        for p,n,weight,uv0 in clipped:
            n=n/max(np.linalg.norm(n),1e-8);p=p+n*(.008+min(.065,weight*.13))
            cap[0].append(p.tolist());cap[1].append(n.tolist());cap[2].append(uv0.tolist())
        cap[3].extend((offset,offset+j,offset+j+1)for j in range(1,len(clipped)-1))
    m,(g,c,d,s,i,b,w,e)=model()
    # An actual open gate frame. No central face or collision spans its opening.
    for side in [-1,1]:
        for j in range(12):
            y=j*1.7;dressed_block(m,g,(side*13-2.9,y,-3),(side*13+2.9,y+1.65,3),.14)
        for yy in [0,5.2,14,20.4]:dressed_block(m,c,(side*13-3.4,yy,-3.5),(side*13+3.4,yy+.7,3.5),.12)
        # Deep front panels and proud stone borders frame the bronze inlay.
        for yy in [6.3,14.9]:
            m.box(d,(side*13-1.75,yy,3.01),(side*13+1.75,yy+3.7,3.10))
            for xx in [-1.9,1.9]:dressed_block(m,c,(side*13+xx-.15,yy-.12,3.03),(side*13+xx+.15,yy+3.82,3.36),.07)
            for h in [yy-.15,yy+3.7]:dressed_block(m,c,(side*13-2.05,h,3.03),(side*13+2.05,h+.22,3.36),.05)
        for yy in [3,7,11,15]:
            # Bronze inlay chevrons cut a readable vertical rhythm into the piers.
            for a in [-1,1]:m.box(b,(side*13+a*.60-.12,yy,3.04),(side*13+a*.60+.12,yy+1.35,3.14),a*.40,(side*13,yy,3.1))
    for j in range(3):
        for k in range(7):
            x=-16.4+k*32.8/7
            dressed_block(m,c,(x+.025,20.8+j*.9,-3.8),(x+32.8/7-.025,21.6+j*.9,3.8),.11)
    m.box(d,(-10.1,19.5,-2),(10.1,21.0,1.6));m.box(b,(-9.9,20.0,1.65),(9.9,20.16,1.78))
    for x in [-13,0,13]:m.box(s,(x-3.6,23.5,-3.9),(x+3.6,23.78,3.9))
    records.append(m.save('grand_gate'))
    m,(g,c,d,s,i,b,w,e)=model();m.box(g,(-2.85,0,-2.9),(2.85,20.3,2.9));records.append(m.save('gate_pier'))
    m,(g,c,d,s,i,b,w,e)=model();m.box(g,(-3,0,-1.4),(3,4,1.4));m.box(s,(-3,4,-1.5),(3,4.25,1.5));records.append(m.save('mountain_foundation'))
    # Each door leaf opens along the side of the passage, leaving the center free.
    m,(g,c,d,s,i,b,w,e)=model();m.box(i,(-.35,0,-.4),(.35,17,7.8))
    for z in np.arange(0,7.8,.85):m.box(d,(-.38,.3,z),(.38,16.7,z+.72))
    for yy in [1.0,4.8,9.2,14.4]:m.box(b,(-.43,yy,-.3),(.43,yy+.28,8.0))
    for zz in [1,3,5,7]:m.box(b,(-.44,6,zz-.18),(.44,11,zz+.18))
    records.append(m.save('gate_leaf'))
    m,(g,c,d,s,i,b,w,e)=model();m.box(g,(-1.75,0,-1.75),(1.75,1,1.75));m.box(c,(-1.45,1,-1.45),(1.45,1.7,1.45));m.box(g,(-1.1,1.7,-1.1),(1.1,13,1.1));m.box(c,(-1.5,13,-1.5),(1.5,14,1.5));m.box(b,(-1.15,4,-1.15),(1.15,4.18,1.15));m.box(b,(-1.15,10,-1.15),(1.15,10.18,1.15));records.append(m.save('hall_column'))
    m,(g,c,d,s,i,b,w,e)=model();m.box(g,(-3,0,-1.5),(3,15,1.5));m.box(c,(-3,0,-1.55),(3,.6,1.55));m.box(c,(-3,13.8,-1.6),(3,14.5,1.6));records.append(m.save('hall_wall'))
    m,(g,c,d,s,i,b,w,e)=model();m.box(g,(-3,0,-3),(3,.015,3));records.append(m.save('floor_tile'))
    m,(g,c,d,s,i,b,w,e)=model();m.box(g,(-3,0,-3),(3,.25,3));m.box(c,(-3,.25,-3),(3,.32,-2.8));m.box(c,(-3,.25,2.8),(3,.32,3));m.box(s,(-2.95,.25,-2.8),(2.95,.31,2.8));records.append(m.save('approach_tile'))
    m,(g,c,d,s,i,b,w,e)=model();m.box(g,(-32,0,-6),(32,1,6));
    for x in range(-30,31,6):m.box(c,(x-2.6,-1.4,-6.2),(x+2.6,0,6.2))
    records.append(m.save('hall_beam'))
    m,(g,c,d,s,i,b,w,e)=model();m.box(g,(-2,0,-1.8),(2,.42,1.8));m.box(c,(-1.7,.42,-1.5),(1.7,.84,1.5));m.box(g,(-1.4,.84,-1.2),(1.4,1.15,1.2));m.box(d,(-.78,1.15,-.6),(.78,1.6,.55));m.box(c,(-.95,1.15,-.95),(.95,3.4,-.65));m.box(b,(-1.05,3.35,-1.0),(1.05,3.60,-.6));
    for x in [-1,1]:m.box(c,(x*.88-.15,1.2,-.6),(x*.88+.15,2.1,.65))
    records.append(m.save('throne'))
    m,(g,c,d,s,i,b,w,e)=model();m.box(g,(-1.8,0,-1.5),(1.8,1.1,1.5));m.box(d,(-1.6,1.1,-1.3),(1.6,1.25,1.3));m.box(e,(-1.3,1.24,-1),(1.3,1.27,1));
    for x in [-1,1]:m.box(g,(x*1.6-.2,1.1,-1.5),(x*1.6+.2,4.2,1.5))
    m.box(g,(-1.8,3.4,-1.5),(1.8,4.2,1.5));m.box(g,(-.8,4.2,-.8),(.8,9,.8));records.append(m.save('forge'))
    m,(g,c,d,s,i,b,w,e)=model();m.box(w,(-.38,0,-.38),(.38,.85,.38));m.box(i,(-.5,.85,-.25),(.65,1.05,.25));m.box(i,(-.30,1.05,-.28),(.35,1.34,.28));m.box(i,(-.75,1.18,-.15),(-.3,1.33,.15));records.append(m.save('anvil'))
    m,(g,c,d,s,i,b,w,e)=model();m.box(w,(-2,.78,-.75),(2,.95,.75));
    for x in [-1.6,1.6]:
        for z in [-.55,.55]:m.box(w,(x-.1,0,z-.1),(x+.1,.78,z+.1))
    records.append(m.save('workbench'))
    m,(g,c,d,s,i,b,w,e)=model();m.box(g,(-.7,0,-.7),(.7,.35,.7));m.box(i,(-.18,.35,-.18),(.18,2.0,.18));m.box(i,(-.6,2,-.6),(.6,2.2,.6));m.box(e,(-.45,2.19,-.45),(.45,2.25,.45));records.append(m.save('brazier'))
    m,(g,c,d,s,i,b,w,e)=model();m.box(b,(-.09,0,-.09),(.09,7,.09));cloth=m.material('Grandstone blue wool',[.025,.10,.20],.96);m.box(cloth,(0,3.3,-.02),(2.1,6.6,.02));m.box(b,(.9,4.1,.03),(1.15,5.8,.06));m.box(b,(.35,5.35,.03),(1.7,5.6,.06));records.append(m.save('banner'))
    m,(g,c,d,s,i,b,w,e)=model()
    rock_low=np.min(np.concatenate([p[0]for p in rock_parts]),axis=0);rock_high=np.max(np.concatenate([p[0]for p in rock_parts]),axis=0);rock_center=(rock_low+rock_high)*.5;rock_center[1]=rock_low[1]
    factor=5.5/max(rock_high[0]-rock_low[0],rock_high[2]-rock_low[2])
    for pos,normal,uv,idx,_ in rock_parts:
        pos=(pos-rock_center)*factor
        record=m.parts.setdefault(8,[[],[],[],[]]);offset=len(record[0]);record[0].extend(pos.tolist());record[1].extend(normal.tolist());record[2].extend(uv.tolist());record[3].extend((idx+offset).tolist())
        # Clip at an interpolated accumulation contour. Selecting complete
        # triangles produced conspicuous white shards on the old rock caps.
        coverage=normal[:,1]-.38
        coverage=np.minimum(coverage,(pos[:,1]-.45)*.7)
        cap=m.parts.setdefault(s,[[],[],[],[]])
        for face in idx:
            polygon=[(pos[j],normal[j],coverage[j])for j in face];clipped=[]
            for j,a in enumerate(polygon):
                b0=polygon[(j+1)%3]
                if a[2]>=0:clipped.append(a)
                if (a[2]>=0)!=(b0[2]>=0):
                    t=a[2]/(a[2]-b0[2]);clipped.append((a[0]+(b0[0]-a[0])*t,a[1]+(b0[1]-a[1])*t,0))
            if len(clipped)<3:continue
            offset=len(cap[0])
            for p,n,weight in clipped:
                n=n/max(np.linalg.norm(n),1e-8);p=p+n*(.008+min(.085,weight*.15))
                cap[0].append(p.tolist());cap[1].append(n.tolist());cap[2].append([p[0]*.38,-p[2]*.38])
            cap[3].extend((offset,offset+j,offset+j+1)for j in range(1,len(clipped)-1))
    records.append(m.save('snow_boulder'))
    # The mountain is a hollow shell above the traversable hall, with an open
    # front portal. The heightfield stays the collision/walking floor beneath it.
    m,(g,c,d,s,i,b,w,e)=model();xs=np.unique(np.r_[np.linspace(-180,180,181),-15.,-9.,-5.2,5.2,9.,15.]);zs=np.linspace(-220,12,65)
    def top(x,z):
        q=max(0,1-(x/195)**2-((z+83)/178)**2)
        ridge=103*q**.72+15*math.sin(x*.048+z*.018)*q+8*math.sin(x*.12-z*.035)*q
        # The entrance lies in a receding ridge, rather than at the bottom of
        # an unbroken hundred-metre wall. Scanned buttresses own its near face.
        front=53+12*math.exp(-((x+74)/36)**2)+16*math.exp(-((x-96)/42)**2)+3*math.sin(x*.09)
        t=max(0,min(1,(-z+12)/105));t=t*t*(3-2*t)
        ridge=front*(1-t)+ridge*t
        return max(18,ridge)
    def stone_noise(x,y):
        ix,iy=math.floor(x),math.floor(y);fx,fy=x-ix,y-iy
        # Coherent fractured strata with angular rather than inflated profiles.
        def value(a,b):
            k=(a*374761393+b*668265263)&0xffffffff;k=((k^(k>>13))*1274126177)&0xffffffff
            return ((k^(k>>16))&0xffffff)/16777215
        return (value(ix,iy)*(1-fx)+value(ix+1,iy)*fx)*(1-fy)+(value(ix,iy+1)*(1-fx)+value(ix+1,iy+1)*fx)*fy
    def front_top_z(x):return -8.+8.*stone_noise(x*.037,7.)
    def roof_point(x,z):
        t=max(0,min(1,(z+65)/77))
        return np.array((x,top(x,z),z+(front_top_z(x)-12)*t*t))
    grid=[[roof_point(float(x),float(z)).tolist()for x in xs]for z in zs]
    for j in range(len(zs)-1):
        for k in range(len(xs)-1):
            points=[grid[j][k],grid[j+1][k],grid[j+1][k+1],grid[j][k+1]]
            normal=np.cross(np.array(points[1])-points[0],np.array(points[2])-points[0]);normal/=np.linalg.norm(normal)
            uv=np.array([(p[0]*.15,p[2]*.15)for p in points]);m.quad(9,points,uv.tolist())
            normals=[]
            for ix,iz in [(k,j),(k,j+1),(k+1,j+1),(k+1,j)]:
                px,pz=float(xs[ix]),float(zs[iz])
                dx=roof_point(px+.1,pz)-roof_point(px-.1,pz);dz=roof_point(px,pz+.1)-roof_point(px,pz-.1)
                n=np.cross(dz,dx);normals.append(n/np.linalg.norm(n))
            normals=np.array(normals);m.parts[9][1][-4:]=normals.tolist()
    def front_bottom(xx):
        side=abs(xx)
        if side<=5.2:return 26.4-side*3.4/5.2
        if side<=9:return 23.-(side-5.2)*5.6/3.8
        return max(0.,17.4*(15-side)/6)
    def front(xx,t):
        bottom=front_bottom(xx)
        y=bottom+(top(xx,12)-bottom)*t;edge=max(.32,min(1,(abs(xx)-23)/25))
        fracture=2*abs(stone_noise(xx*.043+y*.016,y*.022)-.5)
        relief=(34*(fracture-.48)+9*(stone_noise(xx*.13-y*.08,y*.10)-.5)+3*(stone_noise(xx*.43,y*.34)-.5))
        # A receding face, with faulted ribs and broken strata, has physical
        # depth under side light instead of a vertical, softly dented sheet.
        strata=(abs(((y+xx*.13)/8.7)%1*2-1)-.5)*3.8
        z=22*(1-t)+front_top_z(xx)*t+edge*(relief+strata)*math.sin(math.pi*t)
        # A continuous rock face supports the scan ledges. Keep the carved
        # facade ahead of it and retain a real recessed channel for the ice.
        z=min(z,27)if abs(xx)<32 else z
        ice=(1-min(1,max(0,(abs(xx-63)-7)/7)))**2
        ice*=min(1,max(0,(43-y)/8))
        z=z*(1-ice)+9*ice
        return np.array((xx,y,z))
    for k in range(len(xs)-1):
        a,bx=float(xs[k]),float(xs[k+1]);p=grid[-1][k];q=grid[-1][k+1]
        for j in range(48):
            points=[];normals=[]
            for xx,t in [(a,j/48),(bx,j/48),(bx,(j+1)/48),(a,(j+1)/48)]:
                points.append(front(xx,t))
                dx=front(xx+.02,t)-front(xx-.02,t)
                dt=front(xx,t+.0002)-front(xx,t-.0002)
                n=np.cross(dx,dt);n/=max(np.linalg.norm(n),1e-8);normals.append(n)
            for ids in [(0,1,2),(0,2,3)]:
                face=np.array([points[i]for i in ids]);n=np.cross(face[1]-face[0],face[2]-face[0]);n/=max(np.linalg.norm(n),1e-8)
                uv=np.array([(v[0]*.12,v[1]*.12)for v in face]);ns=np.array([normals[i]for i in ids])
                ns=ns*.48+n*.52;ns/=np.maximum(np.linalg.norm(ns,axis=1,keepdims=True),1e-8)
                tri(m,9,face.tolist(),uv.tolist());m.parts[9][1][-3:]=ns.tolist()
    for edge in [grid[0],[row[0]for row in grid],[row[-1]for row in grid]]:
        for a,bx in zip(edge,edge[1:]):
            points=[(a[0],0,a[2]),(bx[0],0,bx[2]),bx,a]
            m.quad(9,points,[(v[0]*.12+v[2]*.12,v[1]*.12)for v in points])
            m.quad(9,list(reversed(points)),[(v[0]*.12+v[2]*.12,v[1]*.12)for v in reversed(points)])
    # Interior ceiling and back: inward-facing surfaces block sunlight/sky.
    m.quad(d,[(34,18,-125),(34,18,10),(-34,18,10),(-34,18,-125)],[(0,0),(0,35),(18,35),(18,0)])
    m.quad(d,[(34,0,-125),(34,18,-125),(-34,18,-125),(-34,0,-125)],[(0,0),(0,8),(25,8),(25,0)])
    records.append(m.save('mountain_shell'))
    (PROJECT/'assets').mkdir(exist_ok=True)
    (PROJECT/'assets/geometry.json').write_text(json.dumps(records,indent=2)+'\n')
    mesh.Model.save=original_save
    print('Built',len(records),'measured stronghold assemblies',flush=True)

def shaders():
    def write(name,text):
        dest=PROJECT/'shaders'/name
        if dest.is_symlink():dest.unlink()
        dest.write_text(text)
    source=(ROOT/'projects/valwood_visual/shaders/world_terrain.fs.glsl').read_text()
    needle='snow *= smoothstep(.30, .76, N.y + (patch - .5) * .23);'
    source=source.replace(needle,needle+f'\n    float grandstone=1.-smoothstep(1800.,2800.,length(vPos.xz-vec2({X},{Z})));\n    float grandstoneHall=step(abs(vPos.x-({X})),32.5)*step({Z-121},vPos.z)*step(vPos.z,{Z+12});\n    snow=max(snow,grandstone*smoothstep(.26,.78,N.y))*(1.-grandstoneHall);')
    marker='float surfaceMetal=0.;studioSurface('
    flooring='if(grandstoneHall>.5){vec2 tile=fract((vPos.xz-vec2(%s,%s))*.5);float seam=1.-smoothstep(.012,.035,min(min(tile.x,1.-tile.x),min(tile.y,1.-tile.y)));base=texture(uRock,ground_uv*.7).rgb*vec3(.60,.66,.72);base*=mix(1.,.43,seam);roughness=.86;ao=mix(.98,.83,seam);}\n    '%(X,Z)
    source=source.replace(marker,flooring+marker)
    source=source.replace('void main() {',(ROOT/'shaders/stronghold_snow_surface.glsl').read_text()+'\nvoid main() {\n    gl_FragDepth=gl_FragCoord.z;')
    source=source.replace('FragColor = vec4(mix(color, aerial_color, fog_amount), 1);','FragColor = vec4(mix(color, aerial_color, fog_amount),1.-snow_imprint_reactive*.75);')
    source=source.replace(marker,'stronghold_snow_surface(vPos,geometric_normal,distance_to_eye,base,roughness,ao,N);\n    '+marker)
    # Reuse the bounded grass texture slots for scanned snow in this private
    # district. Other projects retain their original ground maps and shaders.
    ground=PROJECT/'ground'
    if ground.is_symlink():
        original=ground.resolve();ground.unlink();ground.mkdir()
        for p in original.iterdir():(ground/p.name).symlink_to(p,target_is_directory=p.is_dir())
    for src,name in [('snow_02_diff_1k.jpg','terrain_grass.png'),('snow_02_nor_gl_1k.jpg','terrain_grass_nor.png'),('snow_02_arm_1k.jpg','terrain_grass_rgh.png'),('snow_01_diff_1k.jpg','terrain_sand.png'),('frozen_gravel_diff_1k.jpg','terrain_dirt.png')]:
        dest=ground/name
        if dest.is_symlink():dest.unlink()
        Image.open(TEXT/src).convert('RGB').save(dest)
    write('world_terrain.fs.glsl',source)
    source=(ROOT/'shaders/world_foliage.fs.glsl').read_text()
    source=source.replace('uniform vec4 uBaseFactor;','uniform vec4 uBaseFactor;\nuniform vec3 uWinterSurface=vec3(0);')
    source=source.replace('void main() {',(ROOT/'shaders/stronghold_model_winter.glsl').read_text()+'\nvoid main() {')
    marker='float rough=clamp(uRoughness*arm.g*uStudioTintRough.w,.25,1.),metal=clamp(uMetallic*arm.b,0.,1.);'
    source=source.replace(marker,marker+'\n    winter_material(vec3(vPos.x+uWorldAnchor.x,vPos.y,vPos.z+uWorldAnchor.y),geometricN,N,base.rgb,rough,metal,ao);')
    source=source.replace('vec3(.035+.005*clamp(uWetness,0.,1.))','vec3(mix(.035+.005*clamp(uWetness,0.,1.),.018,uWinterSurface.y))')
    marker='vec3 color=indirect+brdf(base.rgb,metal,rough,N,V,uSun)*sunlight*3.14159265*shadow;'
    source=source.replace(marker,marker+'''\n    if(uWinterSurface.y>0.) {
        float edge=pow(1.-max(dot(N,V),0.),2.);
        float wrap=max((dot(-N,uSun)+.6)/1.6,0.);
        vec3 absorption=exp(-vec3(1.35,.48,.24)*uWinterSurface.z);
        color+=base.rgb*absorption*(skyLight*.32+sunlight*wrap*.22*shadow)*uWinterSurface.y;
        color+=skyLight*absorption*edge*.12*uWinterSurface.y;
    }''')
    source=source.replace('float fog=max(1.-exp(-distance*density),smoothstep(10500.,14900.,distance));','float fog=max(1.-exp(-distance*density),smoothstep(10500.,14900.,distance));\n    fog=max(fog,1.-exp(-max(distance-170.,0.)*.00055));')
    source=source.replace('mix(color,uFog,fogAmount)','mix(color,mix(uFog,uSkyLight*1.9,.50),fogAmount)')
    write('world_foliage.fs.glsl',source)
    # The existing bounded weather pass becomes slow, round drifting snow in
    # this project only. Same terrain/depth/roof rejection, no rain splashes.
    source=(ROOT/'shaders/world_weather_particles.vs.glsl').read_text()
    source=source.replace('15.5+random(seed+7u)*5.0','1.15+random(seed+7u)*1.1').replace('.23+random(seed+4u)*.37','.025+random(seed+4u)*.055')
    source=source.replace('float physical_width=.008+random(seed+6u)*.007;','float physical_width=half_length;')
    source=source.replace('center+=across*(corner.x*half_width)+velocity*(corner.y*half_length);','center.x+=sin(uTime*.65+float(lane)+random(seed)*6.28)*.6;\n        center+=uRight*(corner.x*half_width)+uUp*(corner.y*half_length);\n        vOpacity*=2.;')
    source=source.replace('} else {\n        ivec2 cell=ivec2(int(id%32u)', '} else {\n        gl_Position=vec4(2,2,2,1);return;\n        ivec2 cell=ivec2(int(id%32u)')
    write('world_weather_particles.vs.glsl',source)
    source=(ROOT/'shaders/world_weather_particles.fs.glsl').read_text()
    start=source.index('        float x=vParticleUV.x,y=vParticleUV.y;');end=source.index('    } else if(vMode==1)',start)
    source=source[:start]+'        shape=1.-smoothstep(.25,1.,length(vParticleUV));\n'+source[end:]
    source=source.replace('vec3 color=mix(uGroundLight,uSkyLight,.92)*.95+vec3(.022);','vec3 color=vec3(.70,.81,.94)+uSkyLight*.3;')
    write('world_weather_particles.fs.glsl',source)
    source=(ROOT/'shaders/world_foliage.vs.glsl').read_text()
    guard=f'\n    vec2 winterRoot=aInstance.xz+uWorldAnchor;\n    if((uVegetation==2||uVegetation==3||uVegetation==5)&&length(winterRoot-vec2({X},{Z}))<540.){{gl_Position=vec4(2,2,2,1);return;}}\n'
    source=source.replace('void main() {','void main() {'+guard)
    write('world_foliage.vs.glsl',source)
    for name in ['world_snow.vs.glsl','world_snow.fs.glsl']:write(name,(ROOT/'shaders'/name).read_text())
    # Original short granular snow foley, generated offline; no external media.
    import wave
    audio=PROJECT/'audio'
    if audio.is_symlink():
        original=audio.resolve();audio.unlink();audio.mkdir()
        for p in original.iterdir():(audio/p.name).symlink_to(p,target_is_directory=p.is_dir())
    rate=22050;rng=np.random.default_rng(94831);t=np.arange(int(rate*.30))/rate
    noise=rng.normal(0,1,len(t));low=np.convolve(noise,np.ones(13)/13,mode='same')
    grain=np.maximum(0,np.sin(t*math.tau*74))**7
    sound=(low*.75+noise*.09)*(np.exp(-t*19)*(1-np.exp(-t*140)))+noise*.06*grain*np.exp(-t*12)
    sound/=max(1.,np.max(np.abs(sound))*1.1)
    with wave.open(str(audio/'snow_step.wav'),'wb')as f:f.setparams((1,2,rate,0,'NONE','not compressed'));f.writeframes(np.int16(sound*23000).tobytes())

def layout():
    rows=['VALWOOD_STUDIO 7','look .18 1.02 1.04 .96 8.35 28 .20 .24 .38 .30'];props=[];actors=[]
    rows += [line for line in (ROOT/'projects/valwood_visual/world.studio').read_text().splitlines()if line.startswith('asset ')]
    def asset(alias,stem,collision=0,distance=350):rows.append(f'asset {alias} stronghold/{stem} 1 {collision} 0 {distance} 60 1')
    for name in ['grand_gate','gate_pier','mountain_foundation','gate_leaf','hall_column','hall_wall','floor_tile','approach_tile','hall_beam','throne','forge','anvil','workbench','brazier','banner','snow_boulder','mountain_shell','source_arch','source_head','source_pillar','winter_pine','winter_sapling','winter_trunk','frozen_waterfall']:
        asset('gs_'+name,name,1 if name in ['gate_pier','mountain_foundation','gate_leaf','hall_column','hall_wall','forge','anvil','workbench','brazier','throne','snow_boulder']else 0,1200 if name=='mountain_shell'else 430)
    rows=[r.replace('asset gs_winter_trunk stronghold/winter_trunk 1 0','asset gs_winter_trunk stronghold/winter_trunk 1 1').replace('asset gs_frozen_waterfall stronghold/frozen_waterfall 1 0','asset gs_frozen_waterfall stronghold/frozen_waterfall 1 1')for r in rows]
    # Proper tree LODs retain canopy shadows and alpha-tested foliage lighting.
    rows=[r.replace('asset gs_winter_pine stronghold/winter_pine 1 0 0 430 60','asset gs_winter_pine stronghold/winter_pine 2 0 2 330 65').replace('asset gs_winter_sapling stronghold/winter_sapling 1 0 0 430 60','asset gs_winter_sapling stronghold/winter_sapling 2 0 2 175 45')for r in rows]
    rows=[r.replace('asset gs_hall_wall stronghold/hall_wall ','asset gs_hall_wall stronghold/dressed_hall_wall ').replace('asset gs_floor_tile stronghold/floor_tile ','asset gs_floor_tile stronghold/dressed_floor ').replace('asset gs_banner stronghold/banner ','asset gs_banner stronghold/clan_banner ').replace('asset gs_gate_pier stronghold/gate_pier ','asset gs_gate_pier stronghold/mountain_gate_pier ')for r in rows]
    for name in ['cliff_layer_a','cliff_layer_b','cliff_layer_c']:asset('gs_'+name,name,1,700)
    asset('gs_mountain_gate','mountain_gate',0,700)
    for side in ['left','right']:asset('gs_mountain_gate_wing_'+side,'mountain_gate_wing_'+side,1,700)
    rows.append('asset gs_alpine_backdrop stronghold/alpine_backdrop 2 0 0 1800 450 1')
    for name in ['winter_sedge','winter_talus','winter_deadfall','winter_cut_stump']:asset('gs_'+name,name,1 if name in ['winter_deadfall','winter_cut_stump']else 0,145 if name in ['winter_sedge','winter_talus']else 240)
    def add(name,x,z,yaw=0,scale=1,y=0,**kw):
        props.append(dict(alias='gs_'+name,x=X+x,z=Z+z,yaw=yaw,scale=scale,offset_y=y,**kw));rows.append(f'prop gs_{name} {X+x:.9f} {Z+z:.9f} {yaw} {scale} 0 0 {y} {kw.get("pitch",0)} {kw.get("roll",0)} {kw.get("sx",1)} {kw.get("sy",1)} {kw.get("sz",1)}')
    add('mountain_shell',0,0);add('mountain_gate',0,26)
    add('alpine_backdrop',0,0)
    for x in [-12.9,12.9]:add('gate_pier',x,26)
    add('mountain_gate_wing_left',-22.5,26);add('mountain_gate_wing_right',22.5,26)
    for x in list(range(-174,-17,6))+list(range(18,175,6)):add('mountain_foundation',x,22)
    for x in [-180,180]:
        for z in range(-218,11,6):add('mountain_foundation',x,z,90)
    for x in range(-174,175,6):add('mountain_foundation',x,-220)
    add('gate_leaf',-8.8,1);add('gate_leaf',8.8,1,180)
    for z in range(-111,10,6):
        for x in [-33,33]:add('hall_wall',x,z,90)
    for x in range(-30,31,6):add('hall_wall',x,-122)
    for z in [-19,-43,-67,-91,-115]:
        for x in [-21,21]:add('hall_column',x,z)
        add('hall_beam',0,z,y=14)
    for z in range(-117,12,6):
        for x in range(-30,31,6):add('floor_tile',x,z,y=.01)
    # The approach is sculpted continuous snow; floating square tile overlays
    # would conceal its relief and clip through the new terrain banks.
    for x in [-27,27]:
        for z in [22,50,82]:add('banner',x,z,90 if x<0 else -90)
    for x in [-10.5,10.5]:
        for z in [48,78]:add('brazier',x,z)
    for x in [-28,28]:
        for z in [-18,-66,-106]:add('brazier',x,z)
    add('throne',0,-112,scale=1.6)
    for x,z in [(-27,-37),(-27,-83),(27,-37),(27,-83)]:
        add('forge',x,z,yaw=90 if x<0 else-90);add('anvil',x*.79,z+5)
        add('workbench',x*.83,z-7,yaw=90)
    rng=random.Random(91551)
    for _ in range(62):
        x=rng.uniform(-145,145);z=rng.uniform(15,150)
        if abs(x)<16 or ((x-63)/21)**2+((z-32)/20)**2<1:continue
        add('snow_boulder',x,z,rng.randrange(360),rng.uniform(.65,2.5),y=-.15)
    for x,z in [(-25,18),(25,18),(-46,5),(46,5),(-60,-20),(60,-20)]:add('snow_boulder',x,z,scale=3.8)
    add('frozen_waterfall',63,20,y=-.08)
    for x,z,scale in [(54,19,1.1),(73,20,1.3)]:add('snow_boulder',x,z,scale=scale,y=-.22)
    # Large, overlapping physical strata. Their original scan UVs and normals
    # remain intact; layer placement breaks the straight mountain silhouette.
    for side in [-1,1]:
        for j in range(5):
            if side>0 and j==1:continue
            x=side*(36+j*34)
            # Nonuniform scan placements use conservative world AABBs for
            # collision. Leave capsule clearance along the whole ice shore.
            if j==0:
                add('cliff_layer_a',side*50,23 if side>0 else 28,yaw=0 if side>0 else 9,scale=3.7,y=25 if side>0 else-1.6,sx=2.,sy=3.3,sz=1.7)
            else:add('cliff_layer_a',x,42+(j%3)*2.1,yaw=side*(-9+(j%3)*8),scale=3.7+(j%3)*.14,y=-1.6,sx=1.85+(j%3)*.12,sy=3.05+(j%3)*.29,sz=1.7,pitch=(j%3-1)*3.2,roll=side*(j%2*5-2.5))
        for j in range(6):
            if side>0 and j==1:continue
            x=side*(40 if j==0 else 30+j*30)
            add('cliff_layer_b',x,40-(j%3)*1.8,yaw=side*(8+(j%4)*7),scale=3.6+(j%3)*.15,y=1+(j%3)*2,sx=1.8,sy=2.2,sz=1.25)
        # Upper scans sit in the receding ridge, with their bases buried in
        # its surface; isolated ledges against a blank wall looked suspended.
        for j in range(4):
            x=side*(42+j*40)
            if side>0 and j==0:continue
            add('cliff_layer_c',x,-3-(j%3)*2,yaw=side*(-12+(j%3)*11),scale=3.4+(j%3)*.2,y=39+(j%4)*3,sx=1.4,sy=1.2,sz=1.2,pitch=(j%3-1)*6.5,roll=side*(j%2*7-3.5))
    # A carved opening in the right-hand rock mass feeds the frozen cascade.
    for x,z,yy,scale,yaw in [(48,34,0,3.3,-17),(80,33,0,3.6,22),(49,29,19,2.8,-12),(78,29,20,3.0,15),(63,23,33,2.8,4)]:
        add('cliff_layer_a'if yy>15 else'cliff_layer_b',x,z,yaw,scale,y=yy,sy=2.4 if yy<15 else 1.8)
    # Wind-scoured fragments and dead vegetation give the near snow a scale
    # reference. They remain below ankle height or are placed off clear routes.
    detail_rng=random.Random(103332)
    for j in range(46):
        z=detail_rng.uniform(30,145);side=-1 if j%2 else 1;x=side*detail_rng.uniform(5.8,10.)
        if any(math.hypot(x-a,z-b)<3.6 for a in [-10.5,10.5]for b in [22,48,78]):continue
        add('winter_talus',x,z,detail_rng.randrange(360),detail_rng.uniform(.6,1.15),y=-.045)
        if j%2:add('winter_sedge',x+side*1.4,z+1.2,detail_rng.randrange(360),detail_rng.uniform(.6,1.2),y=-.10)
    for _ in range(52):
        x=detail_rng.uniform(-108,108);z=detail_rng.uniform(60,225)
        if abs(x)<13:continue
        add('winter_sedge',x,z,detail_rng.randrange(360),detail_rng.uniform(.65,1.5),y=-.12)
    for x,z,yaw,scale in [(-23,116,26,.8),(32,111,137,.9),(-47,143,74,.7),(45,173,26,1),(-81,204,112,.7),(97,194,47,.8)]:
        add('winter_deadfall',x,z,yaw,scale,y=-.13)
    for x,z in [(-21,89),(39,128),(-65,162),(61,201),(-94,219)]:add('winter_cut_stump',x,z,detail_rng.randrange(360),detail_rng.uniform(.65,1.1),y=-.14)
    # Winter canopy and understory use existing photographed conifers, with
    # independent bounded texture tiers. Narrow trunk collision leaves the
    # overhanging canopy traversable. Keep both walking rehearsals clear.
    drift_route=[(0,155),(-16,164),(-40,178),(-60,192),(-40,213),(-10,205),(0,180),(0,155)]
    def segment_distance(x,z,a,b):
        dx,dz=b[0]-a[0],b[1]-a[1];t=max(0,min(1,((x-a[0])*dx+(z-a[1])*dz)/max(dx*dx+dz*dz,.001)))
        return math.hypot(x-a[0]-dx*t,z-a[1]-dz*t)
    winter_rng=random.Random(80142);trees=[]
    for _ in range(1000):
        if len(trees)>=30:break
        x=winter_rng.uniform(-128,128);z=winter_rng.uniform(55,238)
        if abs(x)<21 or ((x-63)/25)**2+((z-32)/35)**2<1:continue
        if any(segment_distance(x,z,a,b)<4 for a,b in zip(drift_route,drift_route[1:])):continue
        if any(math.hypot(x-a,z-b)<9 for a,b in trees):continue
        if any(math.hypot(X+x-p['x'],Z+z-p['z'])<p['scale']*3.1+1.5 for p in props if p['alias']=='gs_snow_boulder'):continue
        trees.append((x,z));scale=winter_rng.uniform(.48,.77);yaw=winter_rng.randrange(360)
        add('winter_pine',x,z,yaw,scale,y=-.12);add('winter_trunk',x,z,yaw,scale)
    for x,z in [(22,60),(-35,45),(35,50),(-43,90),(-24,76),(24,78)]:
        if any(math.hypot(x-a,z-b)<7 for a,b in trees):continue
        trees.append((x,z));scale=winter_rng.uniform(.48,.64);yaw=winter_rng.randrange(360)
        add('winter_pine',x,z,yaw,scale,y=-.12);add('winter_trunk',x,z,yaw,scale)
    for x,z in trees[::2]:
        for _ in range(2):
            dx,dz=winter_rng.uniform(-5,5),winter_rng.uniform(-5,5)
            if abs(x+dx)<19:continue
            if any(segment_distance(x+dx,z+dz,a,b)<3 for a,b in zip(drift_route,drift_route[1:])):continue
            scale=winter_rng.uniform(.85,1.5);yaw=winter_rng.randrange(360)
            add('winter_sapling',x+dx,z+dz,yaw,scale,y=-.05)
            add('winter_trunk',x+dx,z+dz,yaw,scale*.32)
    # Keep natural cover off the snow forecourt and out of the roofed hall.
    for kind in range(4):
        for x,z in [(0,-95),(0,-35),(0,25),(0,85),(-50,10),(50,10),(0,140)]:rows.append(f'foliage {X+x} {Z+z} 64 0 {kind}')
    for x,z in [(-10.5,48),(10.5,48),(-28,-18),(28,-18),(-28,-66),(28,-66),(-28,-106),(28,-106)]:
        rows.append(f'light {X+x} {FLOOR+2.8} {Z+z} 18 5 1 .50 .17 0 -45 180 1')
    for x in [-18.4,18.4]:rows.append(f'light {X+x} {FLOOR+14.4} {Z+31.6} 18 8 1 .64 .30 0 -45 180 0')
    for x,z in [(-27,-37),(27,-37),(-27,-83),(27,-83)]:rows.append(f'light {X+x} {FLOOR+2.1} {Z+z} 20 7 1 .37 .10 0 -30 180 1')
    for x,z in [(0,-35),(0,-78),(0,-109)]:rows.append(f'light {X+x} {FLOOR+9} {Z+z} 35 3 .37 .56 1 0 -70 180 0')
    for j,(x,z)in enumerate([(-28,-18),(28,-18),(-28,-66),(28,-66),(-28,-106),(28,-106)]):
        rows.append(f'particle -1 2 1 {X+x} {FLOOR+2.2} {Z+z} {FLOOR+14} .42 1.5 .75 .6 1 1 .9 .2 1 {813+j} 1 .56 .16 .5')
    # Stations and compact patrols leave a continuous clear central avenue.
    def actor(family,x,z,yaw=0,path=None,role='resident',scale=1):
        if family=='Broddi':family='Bedvar'
        actors.append(dict(family=FAMILIES.index(family),x=X+x,z=Z+z,yaw=math.radians(yaw),path=[[X+a,Z+b]for a,b in(path or[])],role=role,scale=scale,phase=len(actors)*.173))
    actor('King',0,-108,0,role='king',scale=1.1)
    for x in [-7,7]:actor('Broddi',x,16,0,role='gate guard')
    for x in [-6,6]:actor('Agvid',x,-103,0,role='honor guard')
    for family,x,z in [('Bedvar',-19.8,-32),('Broddi',19.8,-32),('Orme',-19.8,-78),('Bedvar',19.8,-78)]:actor(family,x,z,90 if x<0 else-90,role='forge worker')
    for family,x,z,yaw in [('Orme',-13,-54,70),('Agvid',-11,-52,-110),('Bedvar',12,-61,-70),('Orme',14,-59,110),('Broddi',-12,-91,80),('Bedvar',-9,-90,-100)]:actor(family,x,z,yaw)
    actor('Broddi',-15,-15,path=[(-15,-15),(-15,-92)],role='hall patrol')
    actor('Agvid',15,-92,path=[(15,-92),(15,-15)],role='hall patrol')
    actor('Orme',-4,65,path=[(-4,65),(-4,29)],role='arriving resident')
    actor('Bedvar',17,-47,path=[(17,-47),(17,-74)],role='supplies runner')
    actor('Agvid',23,36,220,role='forecourt guard')
    route=[(0,105),(0,65),(0,25),(0,-12),(0,-40),(5,-54),(5,-89),(0,-100),(-5,-89),(-5,-54),(0,-40),(0,-12),(0,25),(0,65),(0,105)]
    manifest=dict(version=2,name='Grandstone | Brandalhall Dwarf Stronghold',origin=[X,Z],floor=FLOOR,bounds=[[-180,-220],[180,298]],props=props,actors=actors,route=[[X+x,Z+z]for x,z in route],winter=dict(pines=len(trees),frozen_waterfall=[63,20],frozen_basin=dict(center=[63,32],radii=[13,10]),drift_route=drift_route),gate_clearance=dict(width_m=18,height_m=16.8,peak_height_m=25.8,throat_depth_m=20.4,crest_height_m=47),hall=dict(width_m=64,length_m=135,height_m=18))
    rows+=['entities "VALWOOD_ENTITIES 4 1 0"','surfaces "VALWOOD_SURFACES 2"']
    (PROJECT/'stronghold.json').write_text(json.dumps(manifest,indent=2)+'\n');(PROJECT/'assets/staged.studio').write_text('\n'.join(rows)+'\n')
    # Static data is generated from the same public layout as native QA.
    header=['#ifndef VALWOOD_STRONGHOLD_DATA_H','#define VALWOOD_STRONGHOLD_DATA_H',f'inline constexpr double stronghold_x={X},stronghold_z={Z:.12f};','struct StrongholdStation {unsigned family;double x,z;float yaw,scale,phase;double end_x,end_z;bool patrol;};','inline constexpr StrongholdStation stronghold_stations[]={']
    for a in actors:
        end=a['path'][-1]if a['path']else[a['x'],a['z']]
        header.append('{%d,%.12f,%.12f,%.8ff,%.4ff,%.5ff,%.12f,%.12f,%s},'%(a['family'],a['x'],a['z'],a['yaw'],a['scale'],a['phase'],*end,'true'if a['path']else'false'))
    header+=['};','inline constexpr double stronghold_route[][2]={']+[f'{{{X+x:.12f},{Z+z:.12f}}},'for x,z in route]+['};','#endif']
    header_path=ROOT/'engine/world_stronghold_data.h';text='\n'.join(header)+'\n'
    if not header_path.exists()or header_path.read_text()!=text:header_path.write_text(text)
    with tempfile.TemporaryDirectory(prefix='grandstone-author-')as temp:
        exe=Path(temp)/'author'
        subprocess.run(['c++','-std=c++17','-O2','-I',str(ROOT),*[str(ROOT/f)for f in ['tools/author_dwarf_stronghold.cpp','engine/world_studio.cpp','engine/world_surface_authoring.cpp','engine/world_entity_graph.cpp','engine/world_shader_graph.cpp','engine/world_surface_field.cpp','engine/world_atlas.o']],'-lm','-o',str(exe)],check=True)
        subprocess.run([str(exe),str(PROJECT/'assets/staged.studio'),str(PROJECT/'world.studio')],check=True)
    print('Authored',len(props),'editable placements and',len(actors),'dwarf residents',flush=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--prepare-only',action='store_true');a=parser.parse_args()
    if a.prepare_only:prepare()
    else:
        project();winter_materials();stone_models()
        from build_stronghold_winter_details import waterfall as old_trunk
        old_trunk() # retained narrow trunk proxy; new ice replaces its output
        from build_stronghold_entrance_art import build as entrance_art
        from build_stronghold_mountain_finish import build as mountain_finish
        entrance_art();mountain_finish();shaders();layout()

#!/usr/bin/env python3
"""Private campsite art revision. No installed project/source asset mutations."""
from pathlib import Path
import argparse,copy,json,math,sys
import numpy as np
import build_arrival_camp_assets as base
from build_opening_prologue import Glb, furniture, read_glb, array
from build_village_showcase_project import decompose, matrix
from countryside_contact import root_points, fit_plane, errors

ROOT=base.ROOT
OUT=ROOT/'docs/arrival-redesign/2026-11-04/art-v2'
TEX=OUT/'textures'
PROJECT=ROOT/'build/opening-finward-reference/v15/project'
BARK=ROOT/'assets/polyhaven/cinematic/pine_tree_01/textures'
FABRIC=ROOT/'assets/polyhaven/milestone/fabric_pattern_07/textures'
T=None
EXTRA=[]

class SmallGlb(Glb):
    def texture(self, raw, size=512):return super().texture(raw,min(size,512))

def ground(x,z):return float(T.sample(np.array([[base.CENTER[0]+x,base.CENTER[2]+z]]))[0])-base.CENTER[1]

def filtered_furniture(g,path,*args,**kwargs):
    # Boards pretending to be blankets and a pale stacked-log prop are replaced
    # below with folded cloth and bark. Keep original provenance/bounds records.
    if 'bedroll' in path.name or 'purchased_prop_logs' in path.name:
        return furniture(Glb(),path,*args,**kwargs)
    return furniture(g,path,*args,**kwargs)

def custom_dressing(g,unused):
    lod=unused.lod
    m=base.Mesh(lod)
    names=['Wood','DarkWood','Canvas','CanvasPatch','Rope','Ember','Bark','Endgrain','BedRed','BedBlue','BedOlive']
    m.parts={name:[[],[],[],[]] for name in names}
    gy=ground(0,0);axis_y=gy+.98
    # A short fuel cross, not a tower of white-hot billboard flame.
    for i,a in enumerate([-.40,.55,1.63,2.48]):
        d=np.array([math.cos(a),0,math.sin(a)])
        m.round_beam('DarkWood',-d*.47+[0,gy+.12,0],d*.47+[0,gy+.145,0],.075)
    for i in range(11 if lod=='near' else 6):
        a=i*2.39996;r=.10+.036*(i%5)
        m.sphere('Ember',[math.cos(a)*r,gy+.17,math.sin(a)*r],[.035,.018,.026],8,4)
    for sign in [-1,1]:
        x=sign*.94;h=ground(x,0)
        m.round_beam('Bark',[x,h-.08,0],[x,axis_y+.04,0],.052)
        for dz in [-.12,.12]:m.round_beam('Bark',[x,axis_y-.12,0],[x,axis_y+.14,dz],.032)
    m.round_beam('DarkWood',[-1.11,axis_y,0],[1.18,axis_y,0],.025)
    m.round_beam('Bark',[1.18,axis_y,0],[1.18,axis_y-.17,0],.031)
    m.round_beam('Bark',[1.18,axis_y-.17,0],[1.39,axis_y-.17,0],.038)

    # Irregular split stumps with flared bark and cut grain, within the same
    # measured stool proxies. No polygonal barrel taper or decorative hoops.
    ns=28 if lod=='near'else 14
    for si,(x,z) in enumerate([(-2.1,.2),(2.15,.25),(-1.0,-4.35)]):
        h=ground(x,z)
        profile=[(-.035,.205),(.035,.242),(.16,.224),(.31,.211),(.412,.225)]
        for (y,r),(yy,rr) in zip(profile,profile[1:]):
            for j in range(ns):
                a,b=j*math.tau/ns,(j+1)*math.tau/ns
                def q(t,y,r):
                    r*=1+.035*math.sin(t*5+si)+.018*math.cos(t*9)
                    return [x+r*math.cos(t),h+y,z+r*math.sin(t)]
                qv=[q(a,y,r),q(b,y,r),q(b,yy,rr),q(a,yy,rr)]
                m.face('Bark',qv,[math.cos((a+b)/2),0,math.sin((a+b)/2)],uv=[[j/ns*2,y*2],[(j+1)/ns*2,y*2],[(j+1)/ns*2,yy*2],[j/ns*2,yy*2]])
        for j in range(ns):
            a,b=j*math.tau/ns,(j+1)*math.tau/ns
            q=lambda t,r:[x+math.cos(t)*r,h+.416+.003*math.sin(t*3+si),z+math.sin(t)*r]
            m.face('Endgrain',[[x,h+.416,z],q(a,.225),q(b,.225)],[0,1,0])
        # Hairline end-grain rings and two natural drying splits.
        if lod=='near':
            for r in [.061,.103,.155,.191]:
                for j in range(ns):
                    a,b=j*math.tau/ns,(j+1)*math.tau/ns
                    q=lambda t,rr:[x+math.cos(t)*rr,h+.421,z+math.sin(t)*rr]
                    m.face('Wood',[q(a,r),q(b,r),q(b,r+.002),q(a,r+.002)],[0,1,0])
            for a in [.45+si,3.82-si*.3]:
                d=np.array([math.cos(a),0,math.sin(a)]);c=np.array([x,h+.422,z]);side=np.array([-d[2],0,d[0]])*.003
                m.face('DarkWood',[c+d*.08-side,c+d*.224-side,c+d*.224+side,c+d*.08+side],[0,1,0])

    tx,tz=-3.55,-4.1;half_w,half_d=1.7,1.45;ridge=ground(tx,tz)+1.83
    for z in [-half_d,half_d]:
        for x in [-half_w,half_w]:
            xx,zz=tx+x,tz+z
            m.round_beam('Bark',[xx,ground(xx,zz)-.045,zz],[tx,ridge,zz],.035)
    m.round_beam('Bark',[tx,ridge,tz-half_d-.10],[tx,ridge,tz+half_d+.1],.045)
    # Cross-seam folds run down stretched cloth, and longitudinal sag remains
    # visible in silhouette. Retain the same roof silhouette in MID.
    nx,nz=16,16
    def point(sign,u,v):
        x=tx+sign*half_w*u;z=tz-half_d+v*2*half_d
        edge=ground(tx+sign*half_w,z)+.04
        y=ridge*(1-u)+edge*u-.165*math.sin(math.pi*u)
        y+=.090*math.sin(v*math.tau*4+.2)*math.sin(math.pi*u)
        y-=.035*math.sin(math.pi*v)*u
        return np.array([x,y,z])
    for sign in [-1,1]:
        for i in range(nx):
            for k in range(nz):
                a,b=i/nx,(i+1)/nx;c,d=k/nz,(k+1)/nz
                q=[point(sign,a,c),point(sign,b,c),point(sign,b,d),point(sign,a,d)]
                uv=[[a*2,c*3],[b*2,c*3],[b*2,d*3],[a*2,d*3]]
                m.face('Canvas',q,[sign,1,0],uv=uv)
                m.face('Canvas',[p-[0,.008,0] for p in q[::-1]],[-sign,-1,0],uv=uv[::-1])
        # Reinforced hems and a repaired cloth patch on each slope.
        for v in [0.,.5,1.]:
            for i in range(nx):
                a,b=i/nx,(i+1)/nx;da=.005
                q=[point(sign,a,max(0,v-da)),point(sign,b,max(0,v-da)),point(sign,b,min(1,v+da)),point(sign,a,min(1,v+da))]
                m.face('CanvasPatch',[p+[0,.003,0]for p in q],[sign,1,0])
        for i in range(4):
            a,b=.30+i*.032,.30+(i+1)*.032
            q=[point(sign,a,.28),point(sign,b,.28),point(sign,b,.45),point(sign,a,.45)]
            m.face('CanvasPatch',[p+[0,.006,0]for p in q],[sign,1,0])
        for z in [-half_d,half_d]:
            edge=[tx+sign*half_w,ground(tx+sign*half_w,tz+z)+.04,tz+z]
            peg=[tx+sign*(half_w+.65),ground(tx+sign*(half_w+.65),tz+z+.12)+.04,tz+z+.12]
            m.round_beam('Rope',edge,peg,.010,6)
            m.round_beam('Bark',[peg[0],peg[1]-.09,peg[2]],[peg[0]+.04,peg[1]+.10,peg[2]],.022,6)
    # Rear is also folded cloth, never a blank white triangular billboard.
    for sign in [-1,1]:
        for i in range(12):
            a,b=i/12,(i+1)/12
            q=[[tx,ground(tx,tz-half_d)+.03,tz-half_d],[tx+sign*half_w*b,ground(tx+sign*half_w*b,tz-half_d)+.04,tz-half_d],point(sign,b,0),point(sign,a,0)]
            m.face('Canvas',q,[0,0,-1]);m.face('Canvas',q[::-1],[0,0,1])
    # Soft sleeping pads, a draped blanket and a rolled head end, with a small
    # variation in color and folds instead of three identical flat wood boards.
    for bi,x in enumerate([tx-.92,tx,tx+.92]):
        z=tz+.10;h=ground(x,z)+.008;mat=['BedOlive','BedRed','BedBlue'][bi]
        for i in range(8):
            for j in range(20):
                a,b=-.35+i*.7/8,-.35+(i+1)*.7/8;c,d=-.92+j*1.84/20,-.92+(j+1)*1.84/20
                def q(xx,zz):return [x+xx,h+.055+.025*math.sin((xx+.35)/.7*math.pi)+.011*math.cos(zz*9+bi)*math.sin((xx+.35)/.7*math.pi),z+zz]
                m.face(mat,[q(a,c),q(b,c),q(b,d),q(a,d)],[0,1,0],uv=[[(a+.35)*2,(c+.92)*2],[(b+.35)*2,(c+.92)*2],[(b+.35)*2,(d+.92)*2],[(a+.35)*2,(d+.92)*2]])
        # The folded over hem and rolled pillow expose actual cloth thickness.
        for xx in [-.35,.35]:m.round_beam(mat,[x+xx,h+.045,z-.9],[x+xx,h+.045,z+.9],.024,8)
        m.round_beam(mat,[x-.30,h+.12,z-.71],[x+.30,h+.12,z-.71],.105,20 if lod=='near'else 12)
        for xx in [-.22,.22]:m.round_beam('Rope',[x+xx,h+.12,z-.825],[x+xx,h+.12,z-.6],.009,6)
    # Barked spare fuel has horizontal recognizable log ends, below knee height.
    for k,(x,z) in enumerate([(-2.80,-1.95),(-2.55,-1.95),(-2.68,-1.95)]):
        y=ground(x,z)+.09+(k==2)*.17
        m.round_beam('Bark',[x,y,z-.45],[x,y,z+.45],.09,14 if lod=='near'else 8)
        for zz in [z-.451,z+.451]:
            for j in range(14):
                a,b=j*math.tau/14,(j+1)*math.tau/14
                m.face('Endgrain',[[x,y,zz],[x+.088*math.cos(a),y+.088*math.sin(a),zz],[x+.088*math.cos(b),y+.088*math.sin(b),zz]],[0,0,1 if zz>z else -1])
    # A small tied sack stays safely behind the canvas, outside every camera
    # and traversal lane. Deliberate camp ownership rather than more crates.
    rec=furniture(g,base.SOURCE/f'purchased_prop_sack_{lod}.glb',-4.85,-4.9,width=.40,y=ground(-4.85,-4.9),yaw=19,roughness=.98)
    if lod=='near':EXTRA.append(dict(id='tied_provision_sack',**rec))
    mats={}
    mats['Wood']=g.material('Dry endgrain rings',[.36,.24,.13],.98,base=base.MAPS/'wood_base.png')
    mats['DarkWood']=g.material('Charcoal and dry splits',[.10,.075,.05],1,base=base.MAPS/'wood_base.png')
    mats['Bark']=g.material('Weathered peeled camp branches',[.90,.86,.76],.99,base=base.MAPS/'wood_base.png',normal=BARK/'pine_tree_01_bark_nor_gl_2k.jpg')
    mats['Endgrain']=g.material('Pale exposed cut timber',[.70,.56,.35],.98,base=base.MAPS/'wood_base.png',normal=base.MAPS/'wood_normal.png')
    mats['Canvas']=g.material('Weathered photographed burlap canvas',[1.,1.,.95],1,base=TEX/'canvas_base.png',normal=TEX/'canvas_normal.png')
    mats['CanvasPatch']=g.material('Hand repaired burlap seams',[.77,.73,.62],1,base=TEX/'canvas_base.png',normal=TEX/'canvas_normal.png')
    mats['Rope']=g.material('Hemp rope',[.27,.22,.14],1,base=base.MAPS/'cloth_base.png')
    mats['Ember']=g.material('Low live coals',[.035,.012,.003],1,emissive=[.18,.018,.0015])
    mats['BedRed']=g.material('Worn wool blanket',[.50,.41,.30],1,base=FABRIC/'fabric_pattern_07_col_1_1k.jpg',normal=FABRIC/'fabric_pattern_07_nor_gl_1k.jpg')
    for key,rgb in [('BedBlue',[.16,.23,.25]),('BedOlive',[.24,.27,.16])]:mats[key]=g.material('Woven '+key+' blanket',rgb,1,base=base.MAPS/'cloth_base.png',normal=FABRIC/'fabric_pattern_07_nor_gl_1k.jpg')
    for name,(p,n,uv,ix)in m.parts.items():
        if ix:g.mesh(p,n,uv,ix,mats[name])


def surround(project,output):
    meta=json.loads((project/'city_showcase.json').read_text());cat=json.loads(Path(meta['active_geometry_catalog']).read_text())
    assets={a.get('native',{}).get('alias'):a for a in cat['assets'] if a.get('native',{}).get('alias','').startswith('bep_nature_')}
    grass=json.loads((ROOT/'assets/purchased/bep_reforged/countryside-grass-clumps/source-audit.json').read_text());sites={a['alias']:a['tuft_sites_native'] for a in grass['geometry']}
    roots={alias:root_points(a,sites)for alias,a in assets.items()}
    origin=np.asarray(meta['origin'])
    # Authored asymmetric woodland edge. Background overlaps naturally, with
    # paired canopy height and shrub mass; the southeast road opening stays lit.
    sitespec=[]
    for x,z,s,yaw,typ in [(-7,-14,.70,28,'oak'),(-13,-6,.66,147,'oak'),(-17,4,.40,267,'beech'),(-7,14,.58,17,'oak'),(11,12,.68,138,'oak'),(16,-5,.60,237,'oak'),(24,8,.36,45,'beech'),(9,-19,.40,192,'beech'),(-5,-30,.75,329,'oak'),(-29,-4,.80,92,'oak'),(-14,24,.42,240,'beech'),(25,24,.70,51,'oak')]:sitespec.append(('bep_nature_'+typ,x,z,s,yaw,'canopy'))
    for x,z,s,yaw,typ in [(-7,-7,.19,18,'a'),(-8.5,-2.2,.22,135,'b'),(-8.1,3,.20,202,'a'),(-6.3,7.7,.20,310,'a'),(-2.0,10.5,.17,81,'b'),(6.6,9.5,.20,195,'a'),(9.2,7.7,.17,33,'b'),(11,-.6,.19,208,'a'),(6.6,-7.8,.18,105,'a'),(2.6,-8.2,.17,296,'b'),(-1.2,-8.2,.19,163,'a'),(-4.1,-9.1,.17,57,'b'),(-12.5,-13,.19,44,'a'),(14,18,.20,91,'a')]:sitespec.append(('bep_nature_bush_'+typ,x,z,s,yaw,'understory'))
    for x,z,s,yaw in [(-7.8,-.7,.33,18),(-6.9,5.6,.26,85),(-.7,-7,.26,156),(7,-5.5,.29,252),(6.3,10.1,.30,304),(-10,-9,.31,113)]:sitespec.append(('bep_nature_rock',x,z,s,yaw,'buried_rock'))
    for x,z,s,yaw in [(-6.4,.8,.40,17),(-6,3,.38,110),(-4.3,5.8,.35,256),(-3.3,7.1,.40,6),(-7.7,-4.4,.38,94),(-7,-7.3,.42,194),(-3.3,-7.2,.36,27),(-.3,-6.1,.36,215),(2.5,-6.1,.34,80),(4.6,-5.8,.37,333),(6.1,-6.6,.40,25),(8.3,8.0,.34,185),(7.0,10.3,.38,241),(1.1,9.1,.33,139)]:sitespec.append(('bep_nature_fern',x,z,s,yaw,'fern_cluster'))
    # Three low tufts per vegetated finger, with space between masses. Twelve
    # authored source tufts per patch keep this at~60 rather than hundreds draws.
    fingers=[(-5.6,1.2),(-5.5,3.7),(-4.0,6.2),(-1.8,8.0),(2.0,8.2),(5.8,8.3),(-6.7,-1.8),(-7.3,-6.9),(-4.4,-7.0),(-1.1,-5.9),(2.6,-6.3),(5.7,-6.8)]
    for i,(x,z) in enumerate(fingers):
        for j,(dx,dz) in enumerate([(-.35,.15),(.35,-.2),(0,.65)]):sitespec.append(('bep_nature_grass_'+('long'if(i+j)%2 else'broad'),x+dx,z+dz,.34+.04*((i+j)%3),(i*73+j*91)%360,'low_grass_finger'))
    for x,z,s,yaw in [(-5,3,.42,83),(-3.4,6.1,.43,19),(-2.2,7.7,.40,157),(.8,8.3,.40,248),(-6.4,-.8,.41,43),(-1.3,-5.7,.36,284),(2.9,-6.7,.42,331),(6,-6.0,.40,71)]:sitespec.append(('bep_nature_clover',x,z,s,yaw,'low_clover'))
    camp=json.loads((base.OUT/'ART_CANDIDATE.json').read_text());route=np.asarray(camp['connection_world'])[:,[0,2]]-base.CENTER[[0,2]]
    qa=np.array([[-2.15,1.8],[2.2,1.8],[3.6,1.0],[3.6,-2.0],[1.9,-2.0],[1.9,-2.9],[1.9,-2.0],[3.6,-2.0],[3.6,3.5],[0,5.2]])
    segments=np.concatenate([np.stack((route[:-1],route[1:]),axis=1),np.stack((qa[:-1],qa[1:]),axis=1)])
    def dist(p):
        a=segments[:,0];d=segments[:,1]-a;q=a+np.clip(((p-a)*d).sum(1)/(d*d).sum(1),0,1)[:,None]*d
        return float(np.linalg.norm(p-q,axis=1).min())
    rows=[];reviews=[];triangles=0;rejected=[]
    for i,(alias,x,z,s,yaw,purpose)in enumerate(sitespec):
        a=assets[alias];r=roots[alias];m=np.eye(4);angle=math.radians(yaw);c,si=math.cos(angle),math.sin(angle);m[:3,:3]=np.array([[c,0,si],[0,1,0],[-si,0,c]])*s
        m[:3,3]=[base.CENTER[0]+x-origin[0],base.CENTER[1]+ground(x,z)-origin[1],base.CENTER[2]+z-origin[2]]
        if purpose.startswith('low_')or purpose=='fern_cluster':m=fit_plane(m,r,T,np.array([s,s,s]))
        else:
            p=dict(matrix_native=m.T.ravel().tolist());e=errors(p,r,T)[0]
            m[1,3]-=max(float(e.max() if purpose=='canopy' else np.percentile(e,90))+.025,0)
        # Validate the actual rooted footprint, rather than only center points.
        p=r@m[:3,:3].T+m[:3,3]+origin
        rootdist=dist(np.array([x,z]))
        horizontal=float(np.max(np.linalg.norm((r@m[:3,:3].T)[:,[0,2]],axis=1)))
        # Grass cards have photographed transparent margins; give traversal a
        # clear75cm plus rooted footprint, and NPC shoulders at least60cm.
        if rootdist-horizontal<.75:
            rejected.append(dict(alias=alias,local_xz=[x,z],purpose=purpose,lane_clearance=rootdist-horizontal));continue
        pp=copy.copy(m);pp[:3,3]+=m[:3,:3]@np.asarray(a['pivot_offset_native'])+origin
        yy,pi,ro,sc,ex=decompose(pp);wx,wy,wz=pp[:3,3]
        row=f'{alias} {wx/100800+.5:.17g} {wz/(100800*2818/5009)+.5:.17g} {yy:.12g} {sc:.12g} 0 0 0 {pi:.12g} {ro:.12g} '+' '.join(format(float(v),'.12g')for v in ex)+f' {wy:.12g}'
        rows.append(row);e=errors(dict(matrix_native=m.T.ravel().tolist()),r,T)
        reviews.append(dict(alias=alias,purpose=purpose,local_xz=[x,z],world_root=[float(base.CENTER[0]+x),float(m[1,3]+origin[1]),float(base.CENTER[2]+z)],scale=s,source_yaw_degrees=yaw,lane_root_clearance_m=rootdist-horizontal,near_root_min=float(e[0].min()),near_root_max=float(e[0].max()),mid_root_min=float(e[1].min()),mid_root_max=float(e[1].max()),matrix_root=m.T.ravel().tolist(),row=row))
        triangles+=a['stats'][0]['triangles']
    (output/'surrounding_instances.txt').write_text('# Authored arrival woodland edge; append to authored_instances.txt. Existing asset aliases and source pivots.\n'+'\n'.join(rows)+'\n')
    return dict(passed=True,source_project=str(project),new_instances=len(rows),new_triangles_high=triangles,new_vegetation_texture_bytes=0,cleared_qa_and_connection_lanes=True,minimum_lane_root_clearance_m=min(r['lane_root_clearance_m']for r in reviews),unchanged_terrain_geometry=True,fallback_queries=T.fallback_queries,records=reviews,rejected_for_clearance=rejected)


def prepare_textures(output):
    # Crop only the owned sack atlas' photographed cloth region, away from
    # transparent edges or props. This is a PBR UV atlas extraction, not a new
    # generated image. Preserve matched normal UV coverage.
    import io
    from PIL import Image
    tex=output/'textures';tex.mkdir(parents=True,exist_ok=True)
    path=base.SOURCE/'purchased_prop_sack_near.glb';d,b=read_glb(path)
    for name,i in [('base',1),('normal',0)]:
        im=d['images'][d['textures'][i]['source']];v=d['bufferViews'][im['bufferView']]
        q=Image.open(io.BytesIO(b[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']])).convert('RGB')
        box=tuple(round(v*s)for v,s in zip([.15,.22,.64,.365],[q.width,q.height,q.width,q.height]))
        q=q.crop(box);q.thumbnail((512,512),Image.Resampling.LANCZOS);q.save(tex/('canvas_'+name+'.png'))
    (tex/'PROVENANCE.json').write_text(json.dumps(dict(source=str(path),source_sha256=base.sha(path),crop_uv=[.15,.22,.64,.365],base_source_texture=1,normal_source_texture=0,purpose='Real owned sack burlap photograph reused for campsite canvas',artificial_paint=False),indent=2)+'\n')


def stage(project,output):
    global T,TEX
    TEX=output/'textures'
    assert not(output/'ART_CANDIDATE.json').exists(),'Use a fresh immutable candidate output'
    prepare_textures(output)
    T,_=base.terrain(project)
    base.Glb=SmallGlb;base.furniture=filtered_furniture;base.append_mesh=custom_dressing
    report=base.make(project,output)
    report['scope']='Woodland-edge first-arrival art redesign, not installed or GPU accepted'
    report['extra_props']=EXTRA
    report['unchanged_actor_sockets']=True;report['unchanged_collision_proxies']=False
    report['single_proxy_change']='north_seat moved tolocal(-1,-4.35) toavoidchicken/stool silhouette alignment; physicsbody205 only'
    for b in report['physics']:
        if b['id']=='north_seat':b['center_world']=(base.CENTER+[-1.,ground(-1.,-4.35)+.22,-4.35]).tolist()
    report['materials_revision']='512px existing CC0 photographed bark and woven fabric; modeled cloth folds/repairs, cut stumps, rolled blankets'
    report['fire'].update(height_m=.48,width_m=.32,recommended_emitter_intensity=.55,recommended_light_power=1.1)
    # Trim the source platter and garnish below the real roast. Keep source
    # photographed chicken above the cut; no remesh or synthetic substitute.
    roast_review=[]
    for lod in ['near','mid']:
        path=output/'models'/f'arrival_roast_{lod}.glb';d,b=read_glb(path);g=SmallGlb()
        # Import original generated material/image bindings byte-for-byte by
        # parsing through furniture, then filter the already scaled mesh.
        furniture(g,path,0,0,width=.52,y=0,roughness=.9)
        before=0;after=0;allpos=[]
        for mat,parts in g.geometry.items():
            new=[]
            for pos,n,uv,ix in parts:
                tri=ix.reshape(-1,3);before+=len(tri)
                keep=np.min(pos[tri,1],axis=1)>.065
                tri=tri[keep];after+=len(tri)
                used=np.unique(tri);remap=np.full(len(pos),-1);remap[used]=np.arange(len(used));new.append((pos[used],n[used],uv[used],remap[tri].reshape(-1)));allpos.append(pos[used])
            g.geometry[mat]=new
        p=np.concatenate(allpos);lo=p.min(0);hi=p.max(0);center=(hi+lo)/2
        # Preserve world spit center and put the logical root at exactly Y0.
        shift=np.array([-center[0],-lo[1],-center[2]])
        for mat,parts in g.geometry.items():g.geometry[mat]=[(p+shift,n,u,ix)for p,n,u,ix in parts]
        path.write_bytes(g.finish());row=base.inspect(path,lod)
        report['models']=[row if Path(old['path'])==path else old for old in report['models']]
        roast_review.append(dict(lod=lod,source_triangles=before,retained_chicken_triangles=after,platter_triangles_removed=before-after,cut_local_y=.065,used_height=float(hi[1]-lo[1])))
        if lod=='near':
            half=float(hi[1]-lo[1])/2;report['roast']['pivot_local_m']=[0,half,0];report['roast']['instance_bottom_world_y']=report['roast']['center_world_m'][1]-half
    # The small roast stays on the exact HIGH envelope in both tiers. This
    # avoids the source MID losing a wing after the platter is removed.
    import shutil
    shutil.copyfile(output/'models/arrival_roast_near.glb',output/'models/arrival_roast_mid.glb')
    mid=base.inspect(output/'models/arrival_roast_mid.glb','mid')
    report['models']=[mid if Path(old['path']).name=='arrival_roast_mid.glb'else old for old in report['models']]
    report['roast']['mid_policy']='Exact trimmed HIGH reused: small1836-triangle hero cooking prop keeps silhouette and axis'
    report['roast']['platter_removal']=roast_review
    vegetation=surround(project,output)
    (output/'SURROUNDINGS_REVIEW.json').write_text(json.dumps(vegetation,indent=2)+'\n')
    report['surroundings']={k:v for k,v in vegetation.items()if k not in ['records']}
    report['recipe_sha256']=base.sha(__file__)
    report['canvas_atlas_provenance']=str(output/'textures/PROVENANCE.json')
    for path in [BARK/'pine_tree_01_bark_diff_2k.jpg',BARK/'pine_tree_01_bark_nor_gl_2k.jpg',BARK/'pine_tree_01_bark_arm_2k.jpg',FABRIC/'fabric_pattern_07_col_1_1k.jpg',FABRIC/'fabric_pattern_07_nor_gl_1k.jpg']:
        report['source_files'][str(path.relative_to(ROOT))]=base.sha(path)
    (output/'ART_CANDIDATE.json').write_text(json.dumps(report,indent=2)+'\n')
    return report

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--project',type=Path,default=PROJECT);p.add_argument('--output',type=Path,default=OUT);a=p.parse_args();r=stage(a.project,a.output)
    print(json.dumps(dict(models=r['models'],surroundings=r['surroundings'],roast=r['roast']),indent=2))

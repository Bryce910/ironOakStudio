#!/usr/bin/env python3
"""Compare shared terrain shapes, protected routes and existing rigid supports."""
import ctypes as C
import json
import math
from pathlib import Path
import subprocess
import tempfile
from build_starting_area import Atlas
from build_demo_finish import Author, WIDTH, DEPTH
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'docs/demo-completion/2026-10-29'
ORIGIN=(-26712.,9640.496705929329)


def main():
    with tempfile.TemporaryDirectory(prefix='valwood-landscape-') as tmp:
        tmp=Path(tmp)
        funcs=[]
        for label,header in [('before',OUT/'before/engine/world_demo_contours.h'),('after',ROOT/'engine/world_demo_contours.h')]:
            source=tmp/(label+'.c');source.write_text('#include "'+str(header)+'"\ndouble height(double x,double z){return world_demo_contour_delta(x,z);}\n')
            lib=tmp/(label+'.so');subprocess.run(['cc','-O2','-shared','-fPIC',str(source),'-lm','-o',str(lib)],check=True)
            api=C.CDLL(str(lib));api.height.argtypes=[C.c_double,C.c_double];api.height.restype=C.c_double;funcs.append(api.height)
        before,after=funcs;atlas=Atlas(tmp);author=Author(atlas)
        changed=0;maximum=0.;protected=0
        for iz in range(-380,221):
            for ix in range(-164,165):
                x,z=ix*.5,iz*.5;delta=after(x,z)-before(x,z);maximum=max(maximum,abs(delta));changed+=abs(delta)>1e-8
                if -10<=x<=34 and 13<=z<=47:
                    assert delta==0;protected+=1
        records=[];violations=[]
        for path in sorted((ROOT/'projects/valwood_world/scenery').glob('*_instances.txt')):
            for number,line in enumerate(path.read_text().splitlines(),1):
                t=line.split('#',1)[0].split()
                if not t:continue
                x,z=(float(t[1])-.5)*WIDTH,(float(t[2])-.5)*DEPTH;lx,lz=x-ORIGIN[0],z-ORIGIN[1]
                if not(-76<lx<82 and -80<lz<107):continue
                model=author.model(t[0]);kind=model['collision']
                if kind=='None':continue
                scale=float(t[4]);yaw=math.radians(float(t[3]));c,s=math.cos(yaw),math.sin(yaw);lo,hi=model['bounds_min'],model['bounds_max']
                if kind=='Trunk':lo=[-.65,0,-.65];hi=[.65,0,.65]
                old=[];new=[]
                for zz in range(5):
                    for xx in range(5):
                        px=(lo[0]+(hi[0]-lo[0])*xx/4)*scale;pz=(lo[2]+(hi[2]-lo[2])*zz/4)*scale
                        wx,wz=x+c*px+s*pz,z-s*px+c*pz;h=atlas.sample(wx,wz).height;delta=after(wx-ORIGIN[0],wz-ORIGIN[1])-before(wx-ORIGIN[0],wz-ORIGIN[1]);old.append(h-delta);new.append(h)
                prior=max(old)-min(old);current=max(new)-min(new)
                row=dict(source=path.name,line=number,alias=t[0],world_xz=[x,z],before_relief=prior,after_relief=current,added_relief=current-prior)
                records.append(row)
                if current-prior>.16:violations.append(row)
        report=dict(passed=not violations,changed_grid_samples=changed,grid_samples=601*329,maximum_height_change_m=maximum,protected_camp_samples=protected,rigid_supports_checked=len(records),footprint_samples=len(records)*25,support_relief_growth_limit_m=.16,violations=violations,supports=records,terrain_new_texture_images=0,terrain_new_draw_calls=0)
        (OUT/'landscape-support.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k not in ('supports','violations')},indent=2))
        if violations:print(json.dumps(violations,indent=2))
        raise SystemExit(0 if report['passed'] else 1)


if __name__=='__main__':main()

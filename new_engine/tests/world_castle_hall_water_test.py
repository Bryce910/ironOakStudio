#!/usr/bin/env python3
"""Bounded CPU/source proof for the receiving-hall hidden-moat classification."""
from pathlib import Path
import argparse,ctypes,hashlib,itertools,json,math,re,subprocess,sys,tempfile,unittest
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from build_starting_area import glb_bounds

CAMERA_BOX=[-10.5,10.5,-152.5,-137.5,24.5,30.5]
EXPECTED_SOURCE={
    'village_keep':'a3663dd92c6ab4cf3e9913ce62b16a72a1d772e7f705e6c38edccb9274bad30b',
    'village_keep_mid':'fa1a576baa8d381d7c5fd3ff1764b4762e2f5228a3df9f6725384f4805eb2fcd',
}

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def ray_first_hits(triangles,origins,targets):
    """Double-sided actual source triangles, matching the scenery color pass."""
    a=triangles[:,0];e1=triangles[:,1]-a;e2=triangles[:,2]-a
    first=[]
    for start in range(0,len(origins),32):
        eye=origins[start:start+32];direction=targets[start:start+32]-eye
        h=np.cross(direction[:,None,:],e2[None,:,:]);det=np.einsum('ti,nti->nt',e1,h)
        valid=np.abs(det)>1e-10;inv=np.divide(1,det,out=np.zeros_like(det),where=valid)
        s=eye[:,None,:]-a[None,:,:];u=np.einsum('nti,nti->nt',s,h)*inv
        q=np.cross(s,e1[None,:,:]);v=np.einsum('ni,nti->nt',direction,q)*inv
        t=np.einsum('ti,nti->nt',e2,q)*inv
        good=valid&(u>=-1e-7)&(v>=-1e-7)&(u+v<=1+1e-7)&(t>1e-7)&(t<1-1e-7)
        first.extend(np.min(np.where(good,t,np.inf),axis=1))
    return np.asarray(first)

def point_segment_distance(point,a,b):
    dx,dz=b[0]-a[0],b[1]-a[1]
    t=max(0,min(1,((point[0]-a[0])*dx+(point[1]-a[1])*dz)/(dx*dx+dz*dz)))
    return math.hypot(point[0]-a[0]-t*dx,point[1]-a[1]-t*dz)

class CastleHallWaterTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary=tempfile.TemporaryDirectory(prefix='valwood-hall-water-')
        work=Path(cls.temporary.name);source=work/'probe.cpp';library=work/'probe.so'
        source.write_text('#include "engine/world_castle_visibility.h"\nextern "C" int enclosed(double x,double y,double z){return world_castle_hall_enclosed_eye(x,y,z);}\n')
        subprocess.run(['c++','-std=c++17','-O2','-shared','-fPIC','-I',str(ROOT),str(source),'-o',str(library)],check=True,capture_output=True,text=True)
        cls.library=ctypes.CDLL(str(library));cls.enclosed=cls.library.enclosed
        cls.enclosed.argtypes=[ctypes.c_double]*3;cls.enclosed.restype=ctypes.c_int
        cls.proof=dict(schema_version=1,camera_box_local_xz_absolute_y=CAMERA_BOX,
            helper='world_castle_hall_enclosed_eye',header_sha256=sha(ROOT/'engine/world_castle_visibility.h'),
            keep_source_placement=dict(x=0,z=-145,yaw=math.pi,scale=1,burial=.005),
            scope='Near-moat occlusion only; finite source-ray regression plus source closure review, not a general exterior-water occluder')

    @classmethod
    def tearDownClass(cls):cls.temporary.cleanup()

    def test_native_helper_accepts_only_guarded_actual_eye_box(self):
        count=0
        for x,y,z in itertools.product([-10.5,0,10.5],[24.5,25.765,30.5],[-152.5,-145,-137.5]):
            self.assertTrue(self.enclosed(x,y,z));count+=1
        self.proof['native_inside_cases']=count

    def test_native_helper_retains_doorway_exterior_roof_and_other_views(self):
        cases=[(-10.500001,25.765,-145),(10.500001,25.765,-145),(0,24.499999,-145),(0,30.500001,-145),
            (0,25.765,-152.500001),(0,25.765,-137.499999),(0,25.765,-136.4),(0,25.765,-136),
            (0,25.765,-130),(-13,25.765,225),(224.5,25.765,-112),(0,25.765,35),(0,60,-145),
            (0,25.765,-80),(11.59,25.765,-145)]
        for eye in cases:
            with self.subTest(eye=eye):self.assertFalse(self.enclosed(*eye))
        for axis in range(3):
            for invalid in [math.nan,math.inf,-math.inf]:
                eye=[0,25.765,-145];eye[axis]=invalid
                self.assertFalse(self.enclosed(*eye));cases.append(eye)
        self.proof['native_retained_outside_invalid_cases']=len(cases)

    def test_actual_keep_placement_and_source_hashes_remain_exact(self):
        scene=json.loads((ROOT/'projects/valwood_world/scenery/castle_village_manifest.json').read_text())
        keep=next(row for row in scene['placements']if row['label']=='royal_keep')
        for key,value in self.proof['keep_source_placement'].items():self.assertEqual(keep[key],value)
        for model,expected in EXPECTED_SOURCE.items():
            self.assertEqual(sha(ROOT/'projects/valwood_world/models'/(model+'.glb')),expected)
        self.proof['source_hashes_checked']=EXPECTED_SOURCE

    def test_only_north_moat_run_passes_existing_distance_gate(self):
        path=ROOT/'engine/world_castle_creek.h';source=path.read_text()
        body=re.search(r'world_castle_creek_points\[\]=\{(.*?)\};',source,re.S)[1]
        points=[tuple(map(float,p))for p in re.findall(r'\{\s*([-\d.]+),\s*([-\d.]+),\s*([-\d.]+)\}',body)]
        self.assertEqual(len(points),22)
        corners=list(itertools.product(CAMERA_BOX[:2],CAMERA_BOX[2:4]))
        def box_distance(p):
            return math.hypot(max(CAMERA_BOX[0]-p[0],0,p[0]-CAMERA_BOX[1]),max(CAMERA_BOX[2]-p[1],0,p[1]-CAMERA_BOX[3]))
        distances=[]
        for i,(a,b)in enumerate(zip(points,points[1:])):
            distance=min(box_distance(a),box_distance(b),*(point_segment_distance(c,a,b)for c in corners))
            gate=120+max(a[2],b[2])+4
            distances.append(dict(segment=i,minimum_camera_box_distance_m=distance,existing_visibility_gate_m=gate,eligible=distance<=gate))
        self.assertEqual([d['segment']for d in distances if d['eligible']],[0])
        self.assertEqual(points[:2],[(-205,-219,3.8),(205,-219,3.8)])
        minimum=min(d['minimum_camera_box_distance_m']for d in distances if not d['eligible'])
        self.assertGreater(minimum,205)
        self.proof.update(creek_source_sha256=sha(path),eligible_moat_segments=[distances[0]],minimum_excluded_segment_distance_m=minimum)

    def test_near_and_mid_actual_source_block_eligible_water_directions(self):
        xmin,xmax,zmin,zmax,ymin,ymax=CAMERA_BOX
        cameras=list(itertools.product([xmin,0,xmax],[ymin,25.995,26.75,27.495,ymax],[zmin,(zmin+zmax)/2,zmax]))
        # Include shutter-aligned eyes, not only the room's outer corners.
        cameras.extend((x,26.75,-145)for x in[-8,-4,0,4,8])
        water=np.asarray(list(itertools.product(np.linspace(-212.8,212.8,65),[22.9],[-226.8,-219,-211.2])))
        origins=np.repeat(np.asarray(cameras),len(water),axis=0);targets=np.tile(water,(len(cameras),1))
        results=[]
        for model in EXPECTED_SOURCE:
            _,_,_,source=glb_bounds(ROOT/'projects/valwood_world/models'/(model+'.glb'),True)
            source=np.asarray(source)*[-1,1,-1]+[0,23.995,-145]
            lo,hi=source.min(1),source.max(1)
            relevant=(lo[:,1]<=30.51)&(hi[:,1]>=22.89)&(lo[:,0]<=12.3)&(hi[:,0]>=-12.3)&(lo[:,2]<=-135.7)&(hi[:,2]>=-154.3)
            tri=source[relevant];first=ray_first_hits(tri,origins,targets)
            self.assertTrue(np.isfinite(first).all(),f'{model}: source has an unobstructed moat ray')
            # Positive controls remove the REAL opaque north lancet backs.
            # The same five rays then escape; the proof is not floor-only.
            caps=np.all(np.abs(tri[:,:,2]+153.68)<1e-5,axis=1)&(tri[:,:,1].min(1)>=25.99)&(tri[:,:,1].max(1)<=27.50)
            self.assertGreater(np.count_nonzero(caps),0)
            eyes=np.asarray([(x,26.75,-145)for x in[-8,-4,0,4,8]])
            goals=np.asarray([(x,22.9,-219)for x in[-8,-4,0,4,8]])
            shutter_hits=ray_first_hits(tri,eyes,goals);removed=ray_first_hits(tri[~caps],eyes,goals)
            self.assertTrue(np.isfinite(shutter_hits).all())
            self.assertTrue(np.isinf(removed).all(),'Removing actual shutter caps must expose the north-window rays')
            results.append(dict(model=model,sha256=EXPECTED_SOURCE[model],relevant_actual_triangles=len(tri),
                camera_stations=len(cameras),north_capsule_targets=len(water),actual_source_rays=len(origins),misses=0,
                min_hit_segment_fraction=float(first.min()),max_hit_segment_fraction=float(first.max()),
                removed_opaque_shutter_positive_controls=5,actual_low_shutter_cap_triangles=int(caps.sum())))
        self.proof['actual_source']=results
        self.proof['source_closure_review']='North/side walls have real opaque recessed timber lancet backs and stone intrados; low room floor is closed. All eligible targets are north of the room; no eligible ray exits the real south entrance.'

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'docs/performance/torchpack/hall-water-proof.json')
    args=parser.parse_args()
    result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(CastleHallWaterTest))
    if result.wasSuccessful():
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(CastleHallWaterTest.proof,indent=2)+'\n')
        print('Saved',args.output)
    raise SystemExit(not result.wasSuccessful())

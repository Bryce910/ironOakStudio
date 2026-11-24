#!/usr/bin/env python3
"""Source transform/ground raster invariants independent of purchased art."""
import sys,unittest,math,json,tempfile
from unittest.mock import patch
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import numpy as np
from build_village_showcase_project import decompose,raster_height,solar,geometry_aliases,replace_asset_definitions,asset_definition,preflight_scenery_scales,FILES
class CityProject(unittest.TestCase):
    def test_full_trs_and_pivot(self):
        yaw,pitch,roll=map(math.radians,[117,-24,33]);cy,sy,cx,sx,cz,sz=math.cos(yaw),math.sin(yaw),math.cos(pitch),math.sin(pitch),math.cos(roll),math.sin(roll)
        r=np.array([[cy*cz+sy*sx*sz,-cy*sz+sy*sx*cz,sy*cx],[cx*sz,cx*cz,-sx],[-sy*cz+cy*sx*sz,sy*sz+cy*sx*cz,cy*cx]])
        m=np.eye(4);m[:3,:3]=r*np.array([2,3,4]);m[:3,3]=[14,8,-19]
        y,p,z,scale,extra=decompose(m)
        self.assertTrue(np.allclose([y,p,z],[117,-24,33]))
        self.assertTrue(np.allclose(scale*extra,[2,3,4]))
        # Grounding correction is transformed through rotated/scaled local Y.
        offset=np.array([0,-2,0]);corrected=m.copy();corrected[:3,3]+=m[:3,:3]@offset
        vertex=np.array([1,4,-3]);self.assertTrue(np.allclose(corrected[:3,:3]@(vertex-offset)+corrected[:3,3],m[:3,:3]@vertex+m[:3,3]))
        for invalid in [np.diag([-1,1,1,1]),np.array([[1,.2,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1]])]:
            with self.assertRaises(ValueError):decompose(invalid)
    def test_source_ground_corners_slope_and_coverage(self):
        vertices=np.array([[0,10,0],[4,14,0],[0,18,8],[4,22,8]],float)
        h,mask,origin,step=raster_height(vertices,np.array([[0,1,2],[1,3,2]]),9)
        yy,xx=np.mgrid[:9,:9]
        self.assertTrue(mask.all());self.assertTrue(np.allclose(h,10+xx*.5+yy))
        self.assertTrue(np.allclose(origin,[0,0]));self.assertTrue(np.allclose(step,[.5,1]))
        h,mask,_,_=raster_height(vertices,np.array([[0,1,2]]),9)
        self.assertTrue(np.isfinite(h).all());self.assertFalse(mask[-1,-1]);self.assertTrue(mask[0,0])
    def test_recovered_sun_direction(self):
        hour,bearing=solar();a=(hour-6)/12*math.pi;b=math.radians(bearing)
        raw=np.array([.65*math.cos(a),math.sin(a),.35]);sun=np.array([raw[0]*math.cos(b)-raw[2]*math.sin(b),raw[1],raw[0]*math.sin(b)+raw[2]*math.cos(b)]);sun/=np.linalg.norm(sun)
        p,y=map(math.radians,[-32.754127502441406,115.20697784423828]);expected=[-math.cos(p)*math.cos(y),-math.sin(p),math.cos(p)*math.sin(y)]
        self.assertTrue(np.allclose(sun,expected,atol=1e-10))
    def test_incremental_geometry_preserves_runtime_overrides(self):
        import build_village_showcase_project as builder
        with tempfile.TemporaryDirectory()as folder:
            root=Path(folder);project=root/'city';project.mkdir()
            for name in ['models/imports','ground','scenery','shaders','qa']:(project/name).mkdir(parents=True)
            source=root/'source.glb';source.write_bytes(b'frozen-source-fixture')
            def asset(name):return dict(name=name,files=[str(source)]*2,pivot_offset_native=[0,0,0])
            old=root/'old.json';old.write_text(json.dumps(dict(assets=[asset('Ground'),asset('House')])))
            metadata=dict(active_geometry_catalog=str(old))
            (project/'city_showcase.json').write_text(json.dumps(metadata))
            studio='VALWOOD_STUDIO 7\nlook 0 1.7 1.3 1 15 25 .2 0 .3 .08\nasset nm_city_0_ground old/ground 2 0 0 2000 35 1\nasset nm_city_1_house old/house 2 0 0 2000 35 1\nentities "keep edited practical lights"\nsurfaces "keep authored data"\n'
            (project/'world.studio').write_text(studio)
            protected=['project.valwood','city.scene','city.sky','showcase.views','shaders/world_foliage.vs.glsl','shaders/world_foliage.fs.glsl','qa/publisher_stations.route','qa/publisher-cameras.json']
            for name in protected:(project/name).write_text('edited '+name+'\n')
            assets={name:asset(name)for name in ['AddedCastle','Ground','House']}
            aliases=geometry_aliases(project,assets,True)
            self.assertEqual(aliases,dict(AddedCastle='nm_city_2_addedcastle',Ground='nm_city_0_ground',House='nm_city_1_house'))
            m=np.eye(4).reshape(-1,order='F').tolist()
            catalog=root/'new.json';catalog.write_text(json.dumps(dict(assets=list(assets.values()),placements=[dict(asset='Ground',matrix_native=m)],ground_asset='Ground',cameras=[])))
            with patch.object(sys,'argv',['build','--geometry-only','--catalog',str(catalog),'--project',str(project)]),patch.object(builder,'skeleton',side_effect=AssertionError('Must not reset installed shader/look')),patch.object(builder,'cook',side_effect=lambda project,alias,*a,**kw:'imports/'+alias),patch.object(builder,'source_ground',return_value={}),patch.object(builder,'physics',return_value={'walking_route_verified':False}),patch('builtins.print'):
                builder.main()
            for name in protected:self.assertEqual((project/name).read_text(),'edited '+name+'\n')
            updated=(project/'world.studio').read_text()
            self.assertEqual([s for s in studio.splitlines()if not s.startswith('asset ')],[s for s in updated.splitlines()if not s.startswith('asset ')])
            self.assertIn('asset nm_city_2_addedcastle ',updated)
            installed=json.loads((project/'city_showcase.json').read_text())
            self.assertEqual(installed['asset_aliases'],aliases)
            backup=Path(installed['previous_geometry_backup'])
            self.assertEqual((backup/'world.studio').read_text(),studio)
            for name in protected:self.assertEqual((backup/name).read_text(),'edited '+name+'\n')
            # The next update must keep the appended alias even though it sorts first.
            self.assertEqual(geometry_aliases(project,assets,True),aliases)
    def test_definition_replacement_requires_installed_format(self):
        for text in ['', 'VALWOOD_STUDIO 6\nasset x y\n','VALWOOD_STUDIO 7\nlook x\n']:
            with self.assertRaises(ValueError):replace_asset_definitions(text,['asset next next'])
    def test_native_vegetation_alias_and_lod_contract(self):
        assets={'Tree':{'native':dict(alias='bep_nature_oak',vegetation=2,collision=3,distance=850,split=80)},'Grass':{'native':dict(alias='bep_nature_grass_long',vegetation=1,collision=0,distance=105,split=28)}}
        aliases=geometry_aliases(Path('/unused'),assets,False)
        self.assertEqual(aliases,dict(Tree='bep_nature_oak',Grass='bep_nature_grass_long'))
        self.assertEqual(asset_definition(aliases['Tree'],'imports/oak',assets['Tree']),'asset bep_nature_oak imports/oak 2 3 2 850 80 1')
        self.assertEqual(asset_definition('grass','imports/grass',assets['Grass']),'asset grass imports/grass 2 0 1 105 28 1')
        self.assertEqual(asset_definition('old','imports/old',{}),'asset old imports/old 2 0 0 2000 35 1')
        for invalid in [dict(vegetation=3),dict(vegetation=True),dict(collision=8),dict(distance=float('nan')),dict(distance=40,split=36),dict(split=2)]:
            with self.assertRaises(ValueError):asset_definition('x','imports/x',dict(native=invalid))
        with self.assertRaises(ValueError):geometry_aliases(Path('/unused'),dict(a=dict(native=dict(alias='same')),b=dict(native=dict(alias='same'))),False)
    def test_native_uniform_scale_boundary_and_installed_rows(self):
        for value in [.15,.16,1,4]:
            m=np.diag([value,value,value,1.]);self.assertEqual(decompose(m)[3],value)
        for value in [.05,.12,.149999,4.0001,8]:
            with self.assertRaises(ValueError):decompose(np.diag([value,value,value,1.]))
        with tempfile.TemporaryDirectory()as folder:
            p=Path(folder);(p/'scenery').mkdir();(p/'world.studio').write_text('VALWOOD_STUDIO 7\nasset bush imports/bush 2 0 1 180 45 1\nasset tree imports/tree 2 3 2 850 80 1\n')
            for name in FILES:(p/'scenery'/f'{name}_instances.txt').write_text('# source rows\n')
            path=p/'scenery'/f'{FILES[0]}_instances.txt'
            path.write_text('bush .5 .5 0 .12 0\n')
            with self.assertRaisesRegex(ValueError,'rejected 1 rows'):preflight_scenery_scales(p)
            path.write_text('bush .5 .5 0 .15 0\ntree .5 .5 0 2 0\n');self.assertEqual(preflight_scenery_scales(p)['rows'],2)
            path.write_text('tree .5 .5 0 2.01 0\n')
            with self.assertRaises(ValueError):preflight_scenery_scales(p)
    def test_installed_city_native_scale_preflight(self):
        project=Path(__file__).resolve().parents[1]/'projects/valwood_city_showcase'
        marker=project/'city_showcase.json'
        if not marker.is_file():self.skipTest('Purchased private city project is not installed')
        result=preflight_scenery_scales(project)
        self.assertEqual(result['rows'],json.loads(marker.read_text())['instances'])
        self.assertEqual(result['failures'],0)
if __name__=='__main__':unittest.main()

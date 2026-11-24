#!/usr/bin/env python3
"""Source-measured tool contacts, preserved town and complete watch union."""
from pathlib import Path
import copy,json,math,sys,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import build_village_reinforcements as layer
from build_castle_village import Author,ALIASES,MODELS
from build_village_demo_dressing import shape
from check_castle_demo import reinforcements_report

class VillageReinforcementsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        baseline=json.loads((layer.BASELINE/'castle_village_manifest.json').read_text())
        cls.author=Author(copy.deepcopy(baseline));cls.author.records=copy.deepcopy(baseline['placements'])
        cls.author.bounds={a:m['bounds_y_up']for a,m in baseline['models'].items()}
        ALIASES.update({a:Path(m['file']).stem for a,m in baseline['models'].items()})
        cls.metadata=layer.apply(cls.author)
        cls.author.layout['demo_views']+=copy.deepcopy(layer.VIEWS)

    def clone(self):
        a=copy.copy(self.author);a.records=copy.deepcopy(self.author.records);a.layout=copy.deepcopy(self.author.layout)
        a._reinforcements_metadata=copy.deepcopy(self.metadata);a._reinforcements_expected=copy.deepcopy(self.author._reinforcements_expected)
        return a

    def test_actual_tool_types_contacts_all8pose_union_and_exact_baseline(self):
        p=layer.validate(self.author)
        self.assertEqual((p['instances'],p['tools'],p['tool_types'],p['mounted_guards'],p['animated_watch_guards']),(38,30,12,2,4))
        self.assertEqual((p['tabletop_supported_tools'],p['source_support_rays'],p['fence_supported_tools']),(16,80,14))
        self.assertEqual((p['existing_records_preserved'],p['existing_roads_preserved'],p['source_clear_inspection_cameras']),(2640,24,3))
        self.assertGreater(p['minimum_watch_source_route_clearance_m'],6)
        self.assertTrue(p['all8pose_phases_clear']and p['no_duplicate_animated_guard_scenery'])
        self.assertEqual(self.author.records[:2640],self.author._reinforcements_baseline)
        self.assertEqual(self.author.layout['roads'],self.author._reinforcements_roads)
        before=copy.deepcopy(self.author.records);self.assertEqual(layer.apply(self.author),self.metadata);self.assertEqual(before,self.author.records)

    def test_gate_faces_city_and_preserves_asymmetric_horse_footprint(self):
        guards=[r for r in self.author._reinforcements_expected if r['alias']=='village_horseman']
        self.assertEqual(len(guards),2)
        lo,hi=self.author.model('village_horseman')
        self.assertGreater(abs(lo[2]+hi[2]),.6)
        for guard in guards:
            self.assertEqual(guard['yaw'],math.pi);self.assertEqual(guard['guard_role'],'static_mounted_gate_watch')
            b=shape(self.author,guard)
            self.assertAlmostEqual(b[10],guard['z']-hi[2]);self.assertAlmostEqual(b[11],guard['z']-lo[2])
        self.assertEqual([s['phase']for s in layer.STATIONS],[0,.27,.53,.79])
        self.assertEqual([s['yaw']for s in layer.STATIONS],[0,0,math.pi,math.pi])

    def test_forged_recipe_cannot_move_tool_or_furniture_into_public_avenue(self):
        a=self.clone();r=a.records[2640];expected=a._reinforcements_expected[0]
        r['x']=expected['x']=0;r['z']=expected['z']=177
        with self.assertRaisesRegex(AssertionError,'obstructs road'):layer.validate(a)

    def test_support_floating_fence_contact_and_watch_metadata_rejected(self):
        for change in ['floating','support','fence','phase','union','view','route','source']:
            with self.subTest(change=change):
                a=self.clone();m=a._reinforcements_metadata
                if change=='floating':
                    label=m['surface_supports'][0]['label']
                    for records in [a.records,a._reinforcements_expected]:next(r for r in records if r['label']==label)['elevation']+=.1
                elif change=='support':m['surface_supports'][0]['actual_surface_samples_m'][0]+=.1
                elif change=='fence':m['lean_supports'][0]['parent_source_contact_xyz'][2]+=.1
                elif change=='phase':m['watch_stations'][3]['phase']=.1
                elif change=='union':m['watch_union_bounds_yup'][1][0]-=.3
                elif change=='view':m['views'][0]['position_xz'][0]=1
                elif change=='route':a.layout['demo_route_xz'][0][0]+=.1
                else:m['source_manifests']['tools']['sha256']='0'*64
                with self.assertRaises(AssertionError):layer.validate(a)

    def test_source_proved_attachment_only_narrows_earlier_parent_bbox_pass(self):
        view=layer.earlier_expansion_view(self.author)
        omitted=set(r['label']for r in self.author.records)-set(r['label']for r in view.records)
        self.assertEqual(omitted,{'village_reinforcement_armory_hammer'})
        self.assertEqual(view.records[:2640],self.author.records[:2640])
        a=self.clone();a._reinforcements_metadata['surface_supports'][-1]['actual_surface_samples_m'][0]+=.1
        with self.assertRaises(AssertionError):layer.earlier_expansion_view(a)

    def test_checker_reproduces_sources_and_rejects_forged_metadata(self):
        metadata=copy.deepcopy(self.metadata);metadata['generated_include_sha256']=layer.sha(layer.ROOT/metadata['generated_include'])
        places={r['label']:r for r in self.author.records}
        models={a:dict(file=ALIASES[a]+'.glb',sha256=layer.sha(MODELS/(ALIASES[a]+'.glb')),bounds_y_up=self.author.model(a))
            for a in {r['alias']for r in self.author.records}}
        def check(m=metadata,p=places,models=models):
            return reinforcements_report(m,p,models,roads=self.author.layout['roads'],demo_route=self.author.layout['demo_route_xz'],views=self.author.layout['demo_views'])
        self.assertTrue(check()['actual_pose_union_reproduced'])
        for change in ['missing','removed','prefix','recipe','bounds','contact','watch','role']:
            with self.subTest(change=change):
                m,p,mods=(copy.deepcopy(v)for v in [metadata,places,models])
                if change=='missing':m=None
                elif change=='removed':del p[m['labels'][-1]]
                elif change=='prefix':p['village_farm_corn_crate']['x']+=.1
                elif change=='recipe':m['recipe_sha256']='0'*64
                elif change=='bounds':mods['village_horseman']['bounds_y_up'][1][2]-=.3
                elif change=='contact':m['lean_supports'][0]['contact_gap_m']=.3
                elif change=='watch':m['watch_stations'][0]['phase']=.8
                else:p[m['labels'][-1]]['guard_role']='animated'
                with self.assertRaises(ValueError):check(m,p,mods)

    def test_actual_terrain_grounding_and_all_camera_footings(self):
        p=layer.test(self.author)
        self.assertTrue(p['idempotent']);self.assertGreaterEqual(p['actual_terrain_rays'],145)

if __name__=='__main__':unittest.main(verbosity=2)

#!/usr/bin/env python3
"""Actual purchased-source torch contacts, fixed sockets and light leak guards."""
from pathlib import Path
import copy,json,math,re,sys,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import build_village_torches as torch
from build_castle_village import Author,PATH

class VillageTorchesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.author=Author(json.loads(PATH.read_text()));cls.author.compose();cls.meta=torch.apply(cls.author)

    def test_source_contacts_routes_and_source_socket_emitters(self):
        proof=torch.validate(self.author)
        self.assertEqual((proof['instances'],proof['wall_torches'],proof['pole_torches'],proof['braziers']),(22,14,6,2))
        self.assertEqual(proof['actual_wall_contact_samples'],168)
        self.assertEqual(self.meta['existing_records_preserved'],2437)

    def test_scene_text_precision_preserves_measured_contacts_and_sockets(self):
        actual=copy.copy(self.author);actual.records=copy.deepcopy(self.author.records)
        for record in actual.records:
            for key in ('x','z','yaw','scale','burial','elevation'):
                record[key]=float(format(record[key],'.12g'))
        self.assertTrue(torch.validate(actual)['all_emitters_source_socket_exact'])

    def test_hall_and_wall_light_planes_cannot_be_disabled(self):
        for change in ['hall','normal','socket','seed']:
            with self.subTest(change=change):
                bad=copy.copy(self.author);bad._village_torches_metadata=copy.deepcopy(self.meta)
                e=next(e for e in bad._village_torches_metadata['emitters']if e['zone']=='hall')
                if change=='hall':e['zone']='outdoor';e['clip_bounds']=torch.OUTDOOR
                elif change=='normal':e['normal_xyz']=[0,0,0];e['plane_d_local']=1
                elif change=='socket':e['position_local_xyz'][1]+=.05
                else:e['seed']+=1
                with self.assertRaises(AssertionError):torch.validate(bad)

    def test_bracket_cannot_float_away_from_real_source_contact(self):
        bad=copy.copy(self.author);bad.records=copy.deepcopy(self.author.records)
        proof=self.meta['wall_mounts'][0]
        record=next(r for r in bad.records if r['label']==proof['label'])
        record['x']+=proof['normal_xz'][0]*.08;record['z']+=proof['normal_xz'][1]*.08
        # Keep the flame coupled to the moved body: the independent real-wall
        # contact gate must still reject a floating assembly.
        bad._village_torches_metadata=copy.deepcopy(self.meta)
        emitter=next(e for e in bad._village_torches_metadata['emitters']if e['label']==proof['label'])
        _,assets=torch.inventory();emitter.update(torch.emit(record,assets['wall'],proof['normal_xz'],'outdoor'))
        with self.assertRaisesRegex(AssertionError,'Backplate detached'):torch.validate(bad)

    def test_generated_runtime_include_preserves_all17_fields(self):
        source=torch.include(self.meta)
        rows=re.findall(r'\{([^{}]+)\}, // (village_torch_\w+)',source)
        self.assertEqual(len(rows),22)
        for (body,label),e in zip(rows,self.meta['emitters']):
            self.assertEqual(label,e['label'])
            values=[float(v.strip().removesuffix('f').removesuffix('u'))for v in body.split(',')]
            expected=[*e['position_local_xyz'],e['height'],e['width'],e['intensity'],e['seed'],*e['normal_xyz'],e['plane_d_local'],*e['clip_bounds']]
            self.assertEqual(len(values),17)
            for actual,target in zip(values,expected):self.assertAlmostEqual(actual,target,delta=1e-8*max(1,abs(target)))

    def test_wall_fade_plane_is12cm_behind_actual_mount(self):
        records={r['label']:r for r in self.author.records}
        for e in self.meta['emitters']:
            if not any(e['normal_xyz']):
                self.assertEqual(e['plane_d_local'],1)
                continue
            r=records[e['label']];nx,ny,nz=e['normal_xyz']
            value=nx*r['x']+ny*(24+r['elevation'])+nz*r['z']+e['plane_d_local']
            self.assertAlmostEqual(value,.12,places=8)
            self.assertGreater(nx*e['position_local_xyz'][0]+ny*e['position_local_xyz'][1]+nz*e['position_local_xyz'][2]+e['plane_d_local'],.1)

    def test_idempotent_layer_and_preserved_prefix(self):
        original=copy.deepcopy(self.author.records)
        self.assertEqual(torch.apply(self.author),self.meta)
        self.assertEqual(self.author.records,original)
        bad=copy.copy(self.author);bad.records=copy.deepcopy(original);bad.records[0]['x']+=.02
        with self.assertRaisesRegex(AssertionError,'Existing castle changed'):torch.validate(bad)

if __name__=='__main__':unittest.main(verbosity=2)

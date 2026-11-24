#!/usr/bin/env python3
"""Virtual watch grass patches and actual all-pose boot contacts stay local."""
from pathlib import Path
import hashlib,json,math,re,sys,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from build_castle_village import ROOT,watch_ground_rectangles

class VillageWatchGroundTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.scene=json.loads((ROOT/'projects/valwood_world/scenery/castle_village_manifest.json').read_text())
        cls.meta=cls.scene['village_reinforcements'];cls.rects=watch_ground_rectangles(cls.meta)
        cls.ground=json.loads((ROOT/'projects/valwood_world/scenery/village_ground_history_manifest.json').read_text())

    def test_exact_previous_terrain_prefix_and_only_four_virtual_rectangles_appended(self):
        before=(ROOT/'docs/performance/village-reinforcements/terrain-before-watch-patches.inc').read_text()
        actual=(ROOT/'engine/world_village_layout.inc').read_text()
        self.assertTrue(actual.startswith(before.rsplit('};',1)[0]));self.assertEqual(actual.count('\n')-before.count('\n'),4)
        rows=re.findall(r'\{([^{}]+)\}',actual.split('static const VillageRect village_rects[]={',1)[1].split('};',1)[0])[-4:]
        for row,expected in zip(rows,self.rects):
            values=list(map(float,row.split(',')));self.assertEqual(len(values),8)
            for value,target in zip(values,expected['values']):self.assertAlmostEqual(value,target,places=8)

    def test_rotated_asymmetric_pose_union_gets_exact_quarter_metre_grass_margin(self):
        low,high=self.meta['watch_union_bounds_yup']
        for row in self.rects:
            x,z,hx,hz,c,s,_,_=row['values'];station=row['station']
            self.assertAlmostEqual(hx,(high[0]-low[0])*.5+.25);self.assertAlmostEqual(hz,(high[2]-low[2])*.5+.25)
            for px in [low[0],high[0]]:
                for pz in [low[2],high[2]]:
                    wx=station['x']+c*px+s*pz;wz=station['z']-s*px+c*pz
                    localx=c*(wx-x)-s*(wz-z);localz=s*(wx-x)+c*(wz-z)
                    self.assertAlmostEqual(hx-abs(localx),.25);self.assertAlmostEqual(hz-abs(localz),.25)

    def test_all_four_worn_posts_use_actual_boot_sections_from_all_eight_poses(self):
        rows=self.ground['virtual_watch_contacts'];self.assertEqual(len(rows),4)
        hashes=self.ground['watch_pose_model_sha256'];self.assertEqual(len(hashes),8)
        for file,digest in hashes.items():self.assertEqual(hashlib.sha256((ROOT/'projects/valwood_world/models'/file).read_bytes()).hexdigest(),digest)
        for row,rect in zip(rows,self.rects):
            self.assertEqual(row['grass_clear_rectangle'],rect);self.assertEqual(row['source_pose_count'],8)
            self.assertTrue(row['virtual_runtime_station']);self.assertEqual(row['source_height_m'],.1)
            self.assertGreater(row['section_segments'],0);self.assertEqual(row['source_pose_sha256'],hashes)
        self.assertEqual(self.ground['scene_manifest_sha256'],hashlib.sha256((ROOT/'projects/valwood_world/scenery/castle_village_manifest.json').read_bytes()).hexdigest())
        self.assertEqual(len(self.scene['placements']),2678)
        self.assertFalse(any(r['label'].startswith('village_watch_station_')for r in self.scene['placements']))
        new=[r for r in self.ground['grounded_sections']if r['label'].startswith(('village_reinforcement_','village_watch_station_'))]
        self.assertEqual(len(new),12)

if __name__=='__main__':unittest.main(verbosity=2)

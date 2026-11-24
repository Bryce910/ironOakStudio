#!/usr/bin/env python3
"""Actual crop bounds, existing town preservation and walkable farm headlands."""
from pathlib import Path
import copy,json,math,sys,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import build_village_farms as farm
from build_castle_village import Author,PATH

class VillageFarmsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.author=Author(json.loads(PATH.read_text()));cls.author.compose();cls.metadata=farm.apply(cls.author)

    def clone(self):
        author=copy.copy(self.author);author.records=copy.deepcopy(self.author.records)
        author.layout=copy.deepcopy(self.author.layout);author._village_farms_metadata=copy.deepcopy(self.metadata)
        author._village_farms_expected=copy.deepcopy(self.author._village_farms_expected)
        return author

    def test_real_models_plots_and_every_source_wind_envelope_fit(self):
        proof=farm.validate(self.author)
        self.assertEqual((proof['instances'],proof['corn_clusters'],proof['wheat_clusters'],proof['fence_sections']),(181,60,60,44))
        self.assertEqual(proof['existing_records_preserved'],2459)
        self.assertEqual((proof['corn_stalks'],proof['wheat_stems']),(1200,21120))
        self.assertEqual(proof['inspection_walk_length_m'],176)
        self.assertGreater(proof['minimum_old_source_clearance_from_new_paths_m'],2)
        self.assertTrue(proof['full_source_and_wind_clear'])

    def test_rows_align_with_cultivated_soil_and_do_not_randomly_rotate(self):
        rows=farm.crop_specs();self.assertEqual(len(rows),120)
        for plot in farm.PLOTS:
            selected=[r for r in rows if r['farm_plot']==plot['id']]
            self.assertEqual(len(selected),60)
            self.assertEqual({r['yaw']for r in selected},{0,math.pi})
            self.assertEqual(len({r['x']for r in selected}),6)
            self.assertEqual(len({r['z']for r in selected}),10)
        soil=self.metadata['cultivated_ground']
        self.assertEqual([(p['crop'],p['row_spacing_m'],p['row_phase_x_m'])for p in soil],[('corn',1,.5),('wheat',.25,.125)])

    def test_entire_original_records_and_roads_remain_an_exact_prefix(self):
        self.assertEqual(self.author.records[:2459],self.author._village_farms_baseline)
        old=self.author._village_farms_before_roads
        self.assertEqual(self.author.layout['roads'][:len(old)],old)
        before=copy.deepcopy(self.author.records)
        self.assertEqual(farm.apply(self.author),self.metadata)
        self.assertEqual(self.author.records,before)
        bad=self.clone();bad.records[0]['x']+=.1
        with self.assertRaisesRegex(AssertionError,'Existing town'):farm.validate(bad)

    def test_crop_cannot_obstruct_the_inspection_aisle_even_with_forged_recipe(self):
        bad=self.clone();record=next(r for r in bad.records if r['label']=='village_farm_corn_5_0')
        expected=next(r for r in bad._village_farms_expected if r['label']==record['label'])
        record['x']=expected['x']=141
        with self.assertRaisesRegex(AssertionError,'Public road obstructed'):farm.validate(bad)

    def test_supported_cart_cargo_and_tools_have_real_surface_contacts(self):
        proof=farm.validate(self.author)
        self.assertEqual((proof['supported_items'],proof['source_support_rays']),(5,25))
        bad=self.clone();bad._village_farms_metadata['surface_supports'][0]['actual_surface_samples_m'][0]+=.1
        with self.assertRaises(AssertionError):farm.validate(bad)

    def test_metadata_cannot_disable_soil_wind_or_walk_clearance(self):
        for field in ['soil','wind','walk','roads','views']:
            with self.subTest(field=field):
                bad=self.clone();m=bad._village_farms_metadata
                if field=='soil':m['cultivated_ground'][1]['row_phase_x_m']=.25
                elif field=='wind':m['wind_horizontal_guard_m']=0
                elif field=='walk':m['walking_route_xz'][1][0]+=1
                elif field=='roads':bad.layout['roads'][-1]['half_width']=.1
                else:m['views'][0]['position_xz']=[0,0]
                with self.assertRaises(AssertionError):farm.validate(bad)

    def test_actual_terrain_support_covers_plots_and_all_ground_details(self):
        proof=farm.test(self.author)
        self.assertGreater(proof['actual_terrain_rays'],3000)
        self.assertTrue(proof['idempotent'])

if __name__=='__main__':unittest.main(verbosity=2)

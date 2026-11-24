#!/usr/bin/env python3
"""CPU regressions for the actual habitat validator and generated source data."""
from pathlib import Path
import copy
import importlib.util
import math
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import build_wildlife_habitats as habitat


class FlatAtlas:
    def height(self, x, z):
        return 24.

    def wet(self, x, z):
        return False


def environment(atlas=None):
    env = habitat.Environment.__new__(habitat.Environment)
    env.atlas = atlas or FlatAtlas()
    env.obstacles, env.roads, env.crops = [], [], []
    return env


def animal(x=0., z=0., route=2.):
    return dict(label='fixture', species=0, habitat=0, x=x, z=z,
                yaw=0., route_radius=route, speed=.5 if route else 0., phase=.4)


class HabitatValidationTests(unittest.TestCase):
    species = [dict(radius=.7, height=1.5, bound_radius=1.2)]

    def test_flat_circle_and_exact_runtime_square(self):
        proof = habitat.validate_actor(animal(), self.species, environment())
        self.assertEqual(proof['contact_spread_m'], 0)
        self.assertEqual(proof['runtime_3x3_max_spread_m'], 0)
        self.assertGreater(proof['support_samples'], 100)

    def test_complete_circle_hits_obstacle_between_cardinal_samples(self):
        env = environment()
        env.obstacles = [dict(x=math.sqrt(2), z=math.sqrt(2), yaw=.37,
                              low=[-.04, -.05], high=[.04, .05])]
        with self.assertRaisesRegex(AssertionError, 'source/road/crop'):
            habitat.validate_actor(animal(), self.species, env)

    def test_off_circle_center_chord_also_stays_clear(self):
        env = environment()
        env.obstacles = [dict(x=0., z=0., yaw=0., low=[-.1, -.1], high=[.1, .1])]
        with self.assertRaisesRegex(AssertionError, 'source/road/crop'):
            habitat.validate_actor(animal(), self.species, env)

    def test_huge_source_obb_cannot_escape_broadphase(self):
        env = environment()
        env.obstacles = [dict(x=160., z=0., yaw=0., low=[-162., -2.], high=[162., 2.])]
        with self.assertRaisesRegex(AssertionError, 'source/road/crop'):
            habitat.validate_actor(animal(route=0), self.species, env)

    def test_public_road_and_crop_boundaries(self):
        for feature in ('roads', 'crops'):
            env = environment()
            if feature == 'roads':
                env.roads = [dict(a=[-5., 2.], b=[5., 2.], width=.8)]
            else:
                env.crops = [dict(x=0., z=2., yaw=0., low=[-2., -.1], high=[2., .1])]
            with self.subTest(feature=feature), self.assertRaisesRegex(AssertionError, 'source/road/crop'):
                habitat.validate_actor(animal(), self.species, env)

    def test_contact_square_rejects_slope(self):
        class Slope(FlatAtlas):
            def height(self, x, z):
                return 24 + .06 * x
        with self.assertRaisesRegex(AssertionError, 'support/water'):
            habitat.validate_actor(animal(route=0), self.species, environment(Slope()))

    def test_wet_body_boundary_even_when_soles_dry(self):
        class Shore(FlatAtlas):
            def wet(self, x, z):
                return x > 1.1
        with self.assertRaisesRegex(AssertionError, 'wet full-body'):
            habitat.validate_actor(animal(route=0), self.species, environment(Shore()))

    def test_swept_actor_disks_cannot_interpenetrate(self):
        with self.assertRaisesRegex(AssertionError, 'swept disks overlap'):
            habitat.validate_actor(animal(), self.species, environment(), [animal(x=5.)])

    def test_saved_source_arrays_and_motion_budget(self):
        data = habitat.read(habitat.REPORT)
        self.assertEqual(len(data['species']), 8)
        self.assertEqual([sum(a['habitat'] == i for a in data['actors']) for i in range(3)], [12, 6, 4])
        self.assertEqual(habitat.includes(data), (habitat.DATA.read_text(), habitat.VIEWS.read_text()))
        for a in data['actors']:
            self.assertLess(a['proof']['runtime_3x3_max_spread_m'], .05)
            self.assertGreater(min(a['proof'][k] for k in ('road_margin_m', 'crop_margin_m', 'obstacle_margin_m')), 0)
            if a['route_radius'] and a['species'] in (1, 2):
                self.assertGreaterEqual(a['route_radius'], 2.)
            if a['route_radius'] and a['species'] == 3 and a['habitat'] != 2:
                self.assertGreaterEqual(a['route_radius'], 1.5)
        for index, h in enumerate(data['habitats']):
            self.assertGreaterEqual(h['enter_radius'], h['draw_radius'] + h['actor_extent_m'] + 12)
            self.assertGreaterEqual(h['exit_radius'], h['enter_radius'] + 20)
        self.assertEqual(len(data['inspection_cameras']), 3)
        self.assertIn('1.75f', habitat.VIEWS.read_text())


if __name__ == '__main__':
    unittest.main(verbosity=2)

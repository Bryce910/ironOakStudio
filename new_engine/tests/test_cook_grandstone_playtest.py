"""Collider import checks for source LODs and complete staircase regions."""
import importlib.util
from pathlib import Path
import unittest
import numpy as np

SPEC = importlib.util.spec_from_file_location(
    "grandstone_cooker", Path(__file__).parents[1] / "tools/cook_grandstone_playtest.py")
COOK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COOK)


class SourcePolicyTest(unittest.TestCase):
    def source(self, rows):
        source = object.__new__(COOK.Source)
        source.doc = {"placements": rows}
        return source

    def placement(self, name, mask, group="Temple_Complex", x=0):
        return {"name": name, "hierarchy": group + "/" + name,
                "group": group, "lod_mask": mask,
                "matrix": [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, 0, 0, 1]}

    def test_mask3_has_one_collision_lod(self):
        # Source export contains a single object in both bands. It must not
        # create two differently tessellated overlapping physical surfaces.
        rows = [self.placement("wall_LOD0", 3), self.placement("wall_LOD1", 3)]
        self.assertEqual([r["name"] for r in self.source(rows).selected()], ["wall_LOD1"])

    def test_walk_surface_keeps_detailed_geometry(self):
        rows = [self.placement("Stone_stairs_LOD0", 1),
                self.placement("Stone_stairs_LOD2", 2)]
        self.assertEqual([r["name"] for r in self.source(rows).selected()], ["Stone_stairs_LOD0"])

    def test_solid_gate_retained_cosmetic_overlay_omitted(self):
        rows = [self.placement("Wood_planks_gate_LOD0", 3),
                self.placement("Forged_hinge_LOD0", 1),
                self.placement("Brazier_01_LOD2", 2)]
        self.assertEqual({r["name"] for r in self.source(rows).selected()},
                         {"Wood_planks_gate_LOD0", "Brazier_01_LOD2"})

    def test_stair_region_keeps_last_piece_without_unrelated_geometry(self):
        source = self.source([])
        vertices = np.zeros((3, 16), dtype=float)
        vertices[:, :3] = [[426, 31, 487], [428, 31, 487], [427, 31, 490]]
        vertices[:, 4] = 1
        source.mesh = lambda key: (vertices, np.array([[0, 2, 1]]))
        rows = []
        for group in ["Stairs", "Unrelated"]:
            p = self.placement("Fence Generator/Prefab_stone_stairs_01_spline_66/Stone_stairs_LOD0", 1, group)
            p.update(mesh="fixture", bounds_min=dict(x=426, y=31, z=487),
                     bounds_max=dict(x=428, y=31, z=490))
            rows.append(p)
        v, faces, included = source.sector(rows, COOK.PROFILES["approach"])
        self.assertEqual(len(faces), 1)
        self.assertEqual(len(included), 1)
        self.assertTrue(included[0]["hierarchy"].startswith("Stairs/"))
        self.assertGreater(float(v[:, 0].max()), 427)


if __name__ == "__main__":
    unittest.main()

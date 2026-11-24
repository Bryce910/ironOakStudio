#!/usr/bin/env python3
"""CPU-only native package schema/scene identity closure; no SDL/GPU contexts."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ENGINE = Path(__file__).resolve().parents[1]
VALIDATOR = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else ENGINE / "tools/valwood_project_validate"


class NativeAuthoringValidatorTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="valwood-authoring-validator-")
        self.project = Path(self.temp.name)
        (self.project / "project.valwood").write_text("name=CPU fixture\nterrain=fixture\n")
        (self.project / "assets").mkdir()
        self.scene(1001)
        self.gameplay(1001)
        self.components()

    def tearDown(self):
        self.temp.cleanup()

    def scene(self, identity):
        records = [f"VALWOOD_ENTITIES 3 {identity + 1} 1",
                   f'{identity} 0 "Scene marker" 0 0 0 0 0 0 1 "" 1 0']
        (self.project / "world.studio").write_text("VALWOOD_STUDIO 4\nlook .28 1 1 1 17 0 .2 0 1 .6\n" +
            "".join("entities " + json.dumps(record) + "\n" for record in records))

    def gameplay(self, identity):
        (self.project / "assets/scene.gameplay").write_text(
            f'VALWOOD_GAMEPLAY 1\nname "Scene rules"\nentity {identity} "Actor" 1 0 0 0\n')

    def components(self, physics="", gameplay="assets/scene.gameplay", living="", binding=""):
        (self.project / "world.components").write_text('VALWOOD_COMPONENTS 1\nassets ' +
            " ".join(json.dumps(value) for value in (physics, gameplay, living)) + "\n" + binding)

    def run_validator(self, ok):
        result = subprocess.run([str(VALIDATOR), "--project", str(self.project)],
                                text=True, capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 0 if ok else 1, result.stdout + result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["ok"], ok)
        return report

    def test_valid_typed_components_preserve_sources(self):
        before = {p: p.read_bytes() for p in self.project.rglob("*") if p.is_file()}
        result = self.run_validator(True)
        self.assertEqual(result["components_validated"], 1)
        self.assertEqual(result["authored_entities"], 1)
        self.assertIn("scene-id-closure-v1", result["capabilities"])
        self.assertEqual(before, {p: p.read_bytes() for p in self.project.rglob("*") if p.is_file()})

    def test_missing_persistent_scene_entity_rejected(self):
        self.gameplay(1002)
        result = self.run_validator(False)
        self.assertIn("host scene", result["error"])
        self.assertEqual(result["file"], "world.components")

    def test_full_uint64_scene_identity_preserved(self):
        identity = 0xfffffffffffffff0
        self.scene(identity)
        self.gameplay(identity)
        self.run_validator(True)

    def test_missing_physics_body_rejected(self):
        text = "VALWOOD_PHYSICS 1\ngravity 0 -9.81 0\n" + "".join(f"layer {i} 255\n" for i in range(8))
        (self.project / "export.physics").write_text(text)
        self.components(physics="export.physics", binding="physics 1001 7 0 0 0 0 0 0 1\n")
        self.assertIn("missing scene entity or body", self.run_validator(False)["error"])

    def test_missing_traversal_and_symlink_dependencies_rejected(self):
        for reference in ("assets/missing.gameplay", "../outside.gameplay"):
            self.components(gameplay=reference)
            self.run_validator(False)
        (self.project / "linked.gameplay").symlink_to("assets/scene.gameplay")
        self.components(gameplay="linked.gameplay")
        self.assertIn("symlink", self.run_validator(False)["error"])

    def test_authored_id_cannot_shadow_separate_seed_registry(self):
        (self.project / "project.valwood").write_text("name=Atlas fixture\nterrain=valwood_atlas\n")
        self.scene(0x56414c5700000001)
        self.assertIn("reserved runtime demo identity namespace", self.run_validator(False)["error"])

    def test_registered_seed_ids_work_but_range_guesses_do_not(self):
        (self.project / "project.valwood").write_text("name=Atlas fixture\nterrain=valwood_atlas\n")
        for name in ("models/animations/white_knight_brown_armor.glb",
                     "models/purchased_village_crate_near.glb", "shaders/world_skeletal.vs.glsl", "shaders/world_dynamic.vs.glsl"):
            path = self.project / name
            path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ENGINE / "projects/valwood_world" / name, path)
        self.gameplay(0x56414c5700000001)
        result = self.run_validator(True)
        self.assertEqual(result["seed_entities"], 11)
        self.assertEqual(result["skeletal_assets_validated"], 1)
        self.gameplay(0x56414c57000000ff)
        self.assertIn("host scene", self.run_validator(False)["error"])


if __name__ == "__main__":
    if not VALIDATOR.is_file():
        raise SystemExit("Build/pass the CPU native authoring validator before this test")
    unittest.main()

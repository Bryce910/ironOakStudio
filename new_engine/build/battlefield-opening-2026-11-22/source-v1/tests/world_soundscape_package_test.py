"""Placed sound references must survive package inventory and artist returns."""
from pathlib import Path
import importlib.util
import struct
import sys
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("world_audio_asset_database", SOURCE / "tools/world_asset_database.py")
db = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = db
spec.loader.exec_module(db)


class SoundPackageTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="valwood-sound-package-")
        self.engine = Path(self.temp.name) / "engine"
        self.project = self.engine / "projects/map"
        (self.project / "sounds/imported").mkdir(parents=True)
        (self.project / "project.valwood").write_text("name=Sound Map\n")
        self.clip = "sounds/imported/Rivière stream.WAV"
        pcm = struct.pack("<16h", *range(16))
        wav = b"RIFF" + struct.pack("<I", 36 + len(pcm)) + b"WAVEfmt " + struct.pack("<IHHIIHH", 16, 1, 1, 48000, 96000, 2, 16) + b"data" + struct.pack("<I", len(pcm)) + pcm
        (self.project / self.clip).write_bytes(wav)

    def tearDown(self):
        self.temp.cleanup()

    def write(self, asset=None, version=13, switches="1 1 1", suffix=""):
        asset = self.clip if asset is None else asset
        (self.project / "world.studio").write_text(
            f'VALWOOD_STUDIO {version}\nsound "Brook near camp" "{asset}" {switches} 100 2 -300 .6 2 20 .5 1{suffix}\n', encoding="utf-8")

    def inventory(self):
        return db.ProjectInventory(self.project, self.engine).build()

    def test_unicode_space_clip_and_exact_dependency_are_retained(self):
        self.write()
        files = {row["path"]: row for row in self.inventory()["files"]}
        self.assertIn(self.clip, files)
        self.assertEqual(files["world.studio"]["dependency_paths"], [self.clip])
        self.assertIn("sounds", db.RUNTIME_DIRS)

    def test_missing_placed_clip_is_not_a_silent_package_omission(self):
        self.write("sounds/imported/missing.wav")
        with self.assertRaises(db.ValidationError):
            self.inventory()

    def test_disabled_sounds_still_retain_their_audio(self):
        self.write(switches="0 1 1")
        row = next(row for row in self.inventory()["files"] if row["path"] == "world.studio")
        self.assertEqual(row["dependency_paths"], [self.clip])

    def test_unassigned_marker_has_no_fake_file_dependency(self):
        self.write("")
        row = next(row for row in self.inventory()["files"] if row["path"] == "world.studio")
        self.assertEqual(row["dependency_paths"], [])

    def test_reference_traversal_absolute_private_and_compressed_rejected(self):
        for ref in ("../outside.wav", "/tmp/private.wav", "C:/outside.wav", "sounds//bad.wav", "sounds/./bad.wav", "sounds/.private/voice.wav", "sounds/voice.mp3", "sounds/voice.ogg"):
            with self.subTest(reference=ref):
                self.write(ref)
                with self.assertRaises(db.ValidationError):
                    self.inventory()

    def test_sound_record_cannot_be_smuggled_into_legacy_version(self):
        self.write(version=12)
        with self.assertRaises(db.ValidationError):
            self.inventory()

    def test_record_shape_and_boolean_switches_are_checked(self):
        for switches, suffix in (("2 1 1", ""), ("1 -1 1", ""), ("1 1 3", ""), ("1 1 1", " trailing")):
            with self.subTest(switches=switches, suffix=suffix):
                self.write(switches=switches, suffix=suffix)
                with self.assertRaises(db.ValidationError):
                    self.inventory()

    def test_all_existing_studio_versions_remain_inventory_compatible(self):
        for version in range(1, 13):
            with self.subTest(version=version):
                (self.project / "world.studio").write_text(f"VALWOOD_STUDIO {version}\n")
                self.inventory()

    def test_sound_prefab_keeps_clip_dependency_and_new_entity_codec(self):
        content = f'VALWOOD_STUDIO 13\nsound "Stream" "{self.clip}" 1 1 1 0 0 0 .5 1 15 .2 .7\n'.encode("utf-8")
        graph = b"VALWOOD_ENTITIES 5 1 0\n"
        wrapper = b'VALWOOD_PREFAB 1\n"Stream bundle"\n0 0 0 0 1 1 0\n' + f"{len(content)} {len(graph)}\n".encode() + content + graph
        folder = self.project / "prefabs"
        folder.mkdir()
        (folder / "stream.vwprefab").write_bytes(wrapper)
        files = {row["path"]: row for row in self.inventory()["files"]}
        self.assertEqual(files["prefabs/stream.vwprefab"]["dependency_paths"], [self.clip])


if __name__ == "__main__":
    unittest.main()

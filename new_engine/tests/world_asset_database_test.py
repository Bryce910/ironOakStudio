#!/usr/bin/env python3
"""CPU-only project inventory, identity and unsafe-reference contracts."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from world_asset_database import (ProjectInventory, ValidationError, canonical_json,
                                  main, write_new_json, open_regular_nofollow)


def glb(document=None):
    raw = json.dumps(document or {"asset": {"version": "2.0"}}).encode()
    raw += b" " * (-len(raw) % 4)
    return struct.pack("<IIIII", 0x46546C67, 2, 20 + len(raw), len(raw), 0x4E4F534A) + raw


def fixture(root):
    engine = root / "engine"
    project = engine / "projects/demo"
    project.mkdir(parents=True)
    (project / "project.valwood").write_text("# test\nname=Fixture World\nterrain=valwood_atlas\n")
    (project / "world.studio").write_text("VALWOOD_STUDIO 3\nlook .28 1 1 1 17 0 .2 0 1 .6\n")
    (project / "graphics.settings").write_text("scale=.7\n")
    for name in ("models", "shaders", "ground", "particles", "audio", "fonts", "scenery", "environments"):
        (project / name).mkdir()
    (project / "models/prop.glb").write_bytes(glb())
    (project / "ground/dirt.png").write_bytes(b"texturefixture")
    (project / "particles/fire.vfx").write_bytes(b"VFX1fixture")
    (project / "audio/wind.wav").write_bytes(b"RIFFfixture")
    (project / "fonts/face.ttf").write_bytes(b"fontfixture")
    (project / "scenery/fixtures_instances.txt").write_text("test_prop 0 0 0 1 0 0\n")
    (project / "models/source_manifest.json").write_text(json.dumps({
        "source": "/home/private/Downloads/my-model.blend",
        "license": "Purchased; original rights retained", "recipe": 3,
        "runtime_dependencies": ["prop.glb"]}))
    (engine / "shaders").mkdir()
    (engine / "shaders/test.fs.glsl").write_text("void main(){}\n")
    (project / "shaders/test.fs.glsl").symlink_to("../../../shaders/test.fs.glsl")
    (engine / "engine").mkdir()
    (engine / "engine/world_scenery.cpp").write_text('{"test_prop", "prop", false, 0, 100, 0, 1, Grounding::Root, Collision::None},\n')
    return engine, project


class AssetDatabaseTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="valwood-db-test-")
        self.root = Path(self.temp.name)
        self.engine, self.project = fixture(self.root)

    def tearDown(self):
        self.temp.cleanup()

    def database(self):
        return ProjectInventory(self.project, self.engine).build()

    def test_expanded_jungle_marker_survives_package_and_return(self):
        from world_artist_handoff import authoring_path
        (self.project/'jungle_expansion.json').write_text('{"area_m2":98700}')
        files={r['path'] for r in self.database()['files']}
        self.assertIn('jungle_expansion.json', files)
        self.assertTrue(authoring_path('jungle_expansion.json'))

    def test_source_hash_reuse_checks_identity_and_change_stamps(self):
        target=self.project/'ground/dirt.png';first=target.read_bytes()
        linked=self.project/'ground/linked.png';linked.symlink_to(target)
        inventory=ProjectInventory(self.project,self.engine);read_bytes=0
        class Observed:
            def __init__(self,path):self.stream=open_regular_nofollow(path)
            def __enter__(self):return self
            def __exit__(self,*args):return self.stream.__exit__(*args)
            def fileno(self):return self.stream.fileno()
            def read(self,*args):
                nonlocal read_bytes
                value=self.stream.read(*args);read_bytes+=len(value);return value
        with mock.patch('world_asset_database.open_regular_nofollow',side_effect=Observed):
            inventory.add('ground/first.png',target)
            inventory.add('ground/linked.png',linked)
            self.assertEqual(read_bytes,len(first))
            # Same inode and size after an in-place edit still requires a new
            # digest; the old Source snapshot must reject the changed bytes.
            changed=b'x'*len(first);target.write_bytes(changed)
            inventory.add('ground/changed.png',target)
            self.assertEqual(read_bytes,2*len(first))
        self.assertEqual(inventory.sources['ground/first.png'].source_hash,hashlib.sha256(first).hexdigest())
        self.assertEqual(inventory.sources['ground/changed.png'].source_hash,hashlib.sha256(changed).hexdigest())
        with self.assertRaisesRegex(ValidationError,'Source changed'):
            inventory.sources['ground/first.png'].read_verified()

    def test_purchased_city_markers_fields_and_visible_sky_are_dependencies(self):
        for name in ('city_showcase.json','publisher_showcase.json'):
            (self.project/name).write_text('{}')
        for name in ('city.scene','city.physics','showcase.views'):
            (self.project/name).write_text('runtime authored data\n')
        def field(name, values):
            (self.project/name).write_bytes(struct.pack('<8sIIdddd',b'VWHGT1\0\0',2,2,10,20,1,1)+struct.pack('<4f',*values))
        for name in ('ground/showcase.height','ground/countryside.height','ground/showcase.roofs'):
            field(name,[1,2,3,4])
        for name in ('ground/city.coverage','ground/countryside.coverage'):
            field(name,[1,0,1,0])
        (self.project/'city.sky').write_text('VALWOOD_CITY_SKY 1\npanorama "environments/coast.hdr"\nmaterial 1.5 2.5 -.17\ncube 256\n')
        (self.project/'environments/coast.hdr').write_bytes(b'Radiance fixture')
        files={r['path']:r for r in self.database()['files']}
        for name in ('city_showcase.json','publisher_showcase.json','city.scene','city.sky','city.physics','showcase.views',
                     'ground/showcase.height','ground/city.coverage','ground/countryside.height','ground/countryside.coverage','ground/showcase.roofs'):
            self.assertIn(name,files)
        self.assertIn('environments/coast.hdr',files['city.sky']['dependency_paths'])
        self.assertIn('ground/showcase.height',files['city_showcase.json']['dependency_paths'])
        (self.project/'environments/coast.hdr').unlink()
        with self.assertRaisesRegex(ValidationError,'Missing runtime dependency'):self.database()

    def test_stronghold_practicals_residents_and_malformed_fields(self):
        for name in ('stronghold.json','publisher_showcase.json','showcase.flames.json'):
            (self.project/name).write_text('{}')
        for name in ('showcase.views','showcase.flames','showcase.residents'):
            (self.project/name).write_text('runtime authored data\n')
        files={r['path']for r in self.database()['files']}
        self.assertTrue({'stronghold.json','publisher_showcase.json','showcase.flames.json','showcase.views','showcase.flames','showcase.residents'}<=files)
        target=self.project/'ground/showcase.height';target.write_bytes(b'truncated')
        with self.assertRaisesRegex(ValidationError,'Truncated authored height region'):self.database()
        target.write_bytes(struct.pack('<8sIIdddd',b'VWHGT1\0\0',2,2,0,0,1,1)+struct.pack('<4f',0,0,float('nan'),0))
        with self.assertRaisesRegex(ValidationError,'Non-finite authored height region'):self.database()

    def test_linked_named_sibling_authoring_banks_are_materialized(self):
        sibling=self.engine/'projects/source'
        for bank,name,data in [('ai','manifest.json',b'{}'),('surfaces','road.png',b'image'),('texture_blocks','opaque.vbc7',b'cache')]:
            path=sibling/bank;path.mkdir(parents=True)
            (path/name).write_bytes(data)
            (self.project/bank).symlink_to(path,target_is_directory=True)
        files={r['path']:r for r in self.database()['files']}
        for name in ('ai/manifest.json','surfaces/road.png','texture_blocks/opaque.vbc7'):
            self.assertIn(name,files)
            self.assertTrue(files[name]['symlink_materialized'])
        (sibling/'visual_upgrade.json').write_text('{}')
        (self.project/'visual_upgrade.json').symlink_to(sibling/'visual_upgrade.json')
        self.assertIn('visual_upgrade.json',{r['path']for r in self.database()['files']})

    def test_opt_in_managed_revision_closure_preserves_prefabs_and_cross_revisions(self):
        revisions={key:f'models/imports/{key}/{key*24}'for key in ('a','b','c','d')}
        for prefix in revisions.values():
            path=self.project/prefix;path.mkdir(parents=True)
            (path/'asset.glb').write_bytes(glb())
            (path/'recipe.json').write_text('{}')
        (self.project/'world.studio').write_text('VALWOOD_STUDIO 4\nasset main '+revisions['a'][7:]+'/asset 1 0 0 220 60 1\n')
        def prefab(path,target):
            content=('VALWOOD_STUDIO 4\nasset extra '+revisions[target][7:]+'/asset 1 0 0 220 60 1\n').encode()
            graph=b'VALWOOD_ENTITIES 2 1 0\n'
            header=f'VALWOOD_PREFAB 1\n"Bundle"\n0 0 0 0 1 1 0\n{len(content)} {len(graph)}\n'.encode()
            path.write_bytes(header+content+graph)
        (self.project/'prefabs').mkdir();prefab(self.project/'prefabs/keep-c.vwprefab','c')
        prefab(self.project/revisions['a']/'keep-d.vwprefab','d')
        full={r['path']for r in self.database()['files']}
        selected=ProjectInventory(self.project,self.engine,active_imports_only=True).build()
        active={r['path']for r in selected['files']}
        self.assertIn(revisions['b']+'/asset.glb',full)
        self.assertNotIn(revisions['b']+'/asset.glb',active)
        for key in ('a','c','d'):
            self.assertIn(revisions[key]+'/asset.glb',active)
            self.assertIn(revisions[key]+'/recipe.json',active)
        self.assertIn('models/prop.glb',active)  # unreferenced compiled runtime bank stays
        self.assertEqual(selected['managed_revision_selection']['retained_revisions'],3)

    def test_nested_cache_selection_runs_after_revision_closure_only_when_requested(self):
        prefixes={key:f'models/imports/{key}/{key*24}'for key in ('a','b')}
        for prefix in prefixes.values():
            path=self.project/prefix;(path/'texture_blocks').mkdir(parents=True)
            (path/'asset.glb').write_bytes(glb())
            (path/'texture_blocks/unused.vbc7').write_bytes(b'cachefixture')
        (self.project/'world.studio').write_text('VALWOOD_STUDIO 4\nasset main '+prefixes['a'][7:]+'/asset 1 0 0 220 60 1\n')
        cache=prefixes['a']+'/texture_blocks/unused.vbc7'
        def select(sources,deps):
            self.assertIn(prefixes['a']+'/asset.glb',sources)
            self.assertFalse(any(name.startswith(prefixes['b']+'/')for name in sources))
            self.assertIn(prefixes['a']+'/asset.glb',deps['world.studio'])
            return {cache}
        with mock.patch('world_package_cache_selection.select_unused_revision_caches',side_effect=select) as selector:
            self.assertIn(cache,{row['path']for row in self.database()['files']})
            selector.assert_not_called()
            selected=ProjectInventory(self.project,self.engine,active_imports_only=True).build()
        self.assertNotIn(cache,{row['path']for row in selected['files']})
        self.assertEqual(selected['managed_revision_selection']['excluded_texture_cache_files'],1)
        self.assertEqual(selected['managed_revision_selection']['excluded_texture_cache_bytes'],12)

    def test_cache_selection_rejects_unknown_paths(self):
        with mock.patch('world_package_cache_selection.select_unused_revision_caches',return_value={'missing.vbc7'}):
            with self.assertRaisesRegex(ValidationError,'unknown runtime path'):
                ProjectInventory(self.project,self.engine,active_imports_only=True).build()

    def test_production_assets_and_dependencies(self):
        (self.project / "ai").mkdir()
        (self.project / "ai/economic.vwml").write_text("VALWOOD_MICRO_POLICY 1 18 24 5\n" + "0 " * 581 + "\n")
        (self.project / "cinematics").mkdir()
        (self.project / "cinematics/demo.vct").write_text('VALWOOD_CINEMATICS 1\ntimeline "Demo" 10 0\nzone 1 "Wind" "audio/wind.wav" 0 1 0 0 0 0 8 4 8 3 .2 1 .1\nclip 1 "audio/wind.wav" 0 0 1 0 .2 .1 .1 0\n')
        (self.project / "prefabs").mkdir()
        content = b"VALWOOD_STUDIO 4\nlook .28 1 1 1 17 0 .2 0 1 .6\nasset prop prop 1 1 0 220 60 1\n"
        graph = b"VALWOOD_ENTITIES 2 1 0\n"
        header = f'VALWOOD_PREFAB 1\n"Bundle"\n0 0 0 0 1 1 0\n{len(content)} {len(graph)}\n'.encode()
        (self.project / "prefabs/prop.vwprefab").write_bytes(header + content + graph)
        (self.project / "saves").mkdir()
        (self.project / "saves/world.living").write_text("private player progress stays outside build inputs")
        database = self.database()
        files = {item["path"]: item for item in database["files"]}
        self.assertIn("ai/economic.vwml", files)
        self.assertNotIn("saves/world.living", files)
        self.assertEqual(files["cinematics/demo.vct"]["dependencies"], [files["audio/wind.wav"]["asset_id"]])
        self.assertEqual(files["prefabs/prop.vwprefab"]["dependencies"], [files["models/prop.glb"]["asset_id"]])
        (self.project / "ai/economic.vwml").write_text("VALWOOD_MICRO_POLICY 1 18 24 5\n" + "nan " * 581)
        with self.assertRaisesRegex(ValidationError, "Invalid micro-policy"):
            self.database()

    def test_missing_cinematic_audio_and_truncated_prefab(self):
        (self.project / "cinematics").mkdir()
        file = self.project / "cinematics/missing.vct"
        file.write_text('VALWOOD_CINEMATICS 1\nclip 1 "audio/missing.wav" 0 0 1 0 .2 .1 .1 0\n')
        with self.assertRaises(ValidationError):
            self.database()
        file.unlink()
        (self.project / "prefabs").mkdir()
        (self.project / "prefabs/bad.vwprefab").write_bytes(b'VALWOOD_PREFAB 1\n"Bundle"\n0 0 0 0 1 1 0\n100 100\nshort')
        with self.assertRaisesRegex(ValidationError, "Truncated"):
            self.database()

    def test_npc_appearance_bindings_are_packaged_with_profile_dependencies(self):
        (self.project / "ai/memories").mkdir(parents=True)
        (self.project / "ai/memories/npc.vwmemory").write_text("VALWOOD_NPC_MEMORY 2\n")
        bindings = self.project / "ai/npc_appearances.vwlooks"
        bindings.write_text('VALWOOD_NPC_LOOKS 1 1\nBIND 16 "ai/memories/npc.vwmemory"\nEND\n')
        data = self.database()
        files = {item["path"]: item for item in data["files"]}
        self.assertIn("ai/npc_appearances.vwlooks", files)
        self.assertEqual(files["ai/npc_appearances.vwlooks"]["dependencies"], [files["ai/memories/npc.vwmemory"]["asset_id"]])
        for text in ['VALWOOD_NPC_LOOKS 1 33\nEND\n', 'VALWOOD_NPC_LOOKS 1 1\nBIND 140 "ai/memories/npc.vwmemory"\nEND\n', 'VALWOOD_NPC_LOOKS 1 2\nBIND 16 "ai/memories/npc.vwmemory"\nBIND 16 "ai/memories/npc.vwmemory"\nEND\n', 'VALWOOD_NPC_LOOKS 1 1\nBIND 16 "../private.vwmemory"\nEND\n', 'VALWOOD_NPC_LOOKS 1 1\nBIND 16 "ai/missing.vwmemory"\nEND\n']:
            bindings.write_text(text)
            with self.assertRaises(ValidationError):
                self.database()

    def test_live_minds_embed_profiles_without_developer_file_dependencies(self):
        (self.project / "ai").mkdir()
        target = self.project / "ai/npc_minds.vwminds"
        profile = 'VALWOOD_NPC_MEMORY 2\nPROFILE "resident" 0 "Resident" "" "" ""\n'
        # std::quoted escapes the embedded quotes, retaining actual newlines.
        quoted = '"' + profile.replace('\\', '\\\\').replace('"', '\\"') + '"'
        target.write_text('VALWOOD_NPC_MINDS 1 1\nBIND 123 1 "player" ' + quoted + '\nEND\n')
        files = {item["path"]: item for item in self.database()["files"]}
        self.assertIn("ai/npc_minds.vwminds", files)
        self.assertEqual(files["ai/npc_minds.vwminds"]["dependencies"], [])
        for text in ['VALWOOD_NPC_MINDS 1 33\nEND\n', 'VALWOOD_NPC_MINDS 1 1\nBIND 0 0 "VALWOOD_NPC_MEMORY 2\n"\nEND\n', 'VALWOOD_NPC_MINDS 1 1\nBIND 12 17 "ignored"\nEND\n']:
            target.write_text(text)
            with self.assertRaises(ValidationError):
                self.database()

    def test_gameplay_definition_included_but_progress_stays_private(self):
        definition = b'VALWOOD_GAMEPLAY 1\nname "Castle training"\n'
        (self.project / "world.gameplay").write_bytes(definition)
        (self.project / "assets").mkdir()
        (self.project / "assets/quest.gameplay").write_bytes(definition)
        (self.project / "saves").mkdir()
        (self.project / "saves/world.gameplay.state").write_text("private player progress")
        (self.project / "saves/world.gameplay").write_bytes(definition)
        (self.project / "world.gameplay.state").write_text("private player progress")
        database = self.database()
        files = {item["path"]: item for item in database["files"]}
        self.assertIn("world.gameplay", files)
        self.assertIn("assets/quest.gameplay", files)
        self.assertEqual(files["world.gameplay"]["dependency_paths"], [])
        self.assertNotIn("saves/world.gameplay.state", files)
        self.assertNotIn("saves/world.gameplay", files)
        self.assertNotIn("world.gameplay.state", files)
        self.assertEqual((self.project / "world.gameplay").read_bytes(), definition)

    def test_gameplay_header_and_size_rejected(self):
        for definition in (b"VALWOOD_GAMEPLAY 2\nname Test\n", b"VALWOOD_GAMEPLAY_STATE 2 123\n",
                           b"VALWOOD_GAMEPLAY", b'VALWOOD_GAMEPLAY 1\nname "\xff"\n',
                           b"VALWOOD_GAMEPLAY 1\n" + b"x" * (2 * 1024 * 1024)):
            with self.subTest(prefix=definition[:32]):
                (self.project / "world.gameplay").write_bytes(definition)
                with self.assertRaisesRegex(ValidationError, "gameplay"):
                    self.database()

    def test_components_gather_only_typed_explicit_root_exports(self):
        (self.project / "assets").mkdir()
        (self.project / "runtime-demo.physics").write_text("VALWOOD_PHYSICS 1\n")
        (self.project / "assets/scene.gameplay").write_text('VALWOOD_GAMEPLAY 1\nname "Scene"\n')
        (self.project / "world.components").write_text('VALWOOD_COMPONENTS 1\nassets "runtime-demo.physics" "assets/scene.gameplay" ""\nphysics 18446744073709551614 7 0 0 0 0 0 0 1\n')
        (self.project / "unreferenced.physics").write_text("Not gathered implicitly")
        before = (self.project / "world.components").read_bytes()
        files = {item["path"]: item for item in self.database()["files"]}
        self.assertIn("world.components", files)
        self.assertIn("runtime-demo.physics", files)
        self.assertNotIn("unreferenced.physics", files)
        self.assertEqual(files["world.components"]["dependency_paths"],
                         ["assets/scene.gameplay", "runtime-demo.physics"])
        self.assertEqual((self.project / "world.components").read_bytes(), before)

    def test_components_invalid_paths_ids_and_records_rejected(self):
        for text in ('VALWOOD_COMPONENTS 2\nassets "" "" ""\n',
                     'VALWOOD_COMPONENTS 1\nassets "../outside.physics" "" ""\n',
                     'VALWOOD_COMPONENTS 1\nassets "models/prop.glb" "" ""\n',
                     'VALWOOD_COMPONENTS 1\nassets "" "" ""\nassets "" "" ""\n',
                     'VALWOOD_COMPONENTS 1\nassets "" "" ""\nphysics -1 7 0 0 0 0 0 0 1\n',
                     'VALWOOD_COMPONENTS 1\nassets "" "" ""\nphysics 18446744073709551616 7 0 0 0 0 0 0 1\n',
                     'VALWOOD_COMPONENTS 1\nassets "" "" ""\nphysics 1 7 nan 0 0 0 0 0 1\n',
                     'VALWOOD_COMPONENTS 1\nassets "missing.physics" "" ""\n'):
            with self.subTest(text=text):
                (self.project / "world.components").write_text(text)
                with self.assertRaises((ValidationError, FileNotFoundError)):
                    self.database()

    def test_components_referenced_symlink_is_rejected(self):
        (self.project / "export.physics").write_text("VALWOOD_PHYSICS 1\n")
        (self.project / "linked.physics").symlink_to("export.physics")
        (self.project / "world.components").write_text('VALWOOD_COMPONENTS 1\nassets "linked.physics" "" ""\n')
        with self.assertRaisesRegex(ValidationError, "symlink"):
            self.database()

    def test_live_rig_crate_shader_are_explicit_project_dependencies(self):
        (self.engine / "engine/world_runtime_visuals.inc").write_text("struct WorldLiveVisuals {};\n")
        with self.assertRaisesRegex(ValidationError, "Missing runtime dependency"):
            self.database()
        (self.project / "models/animations").mkdir()
        (self.project / "models/animations/white_knight_brown_armor.glb").write_bytes(glb())
        (self.project / "models/animations/white_knight_brown_armor_demo.glb").write_bytes(glb())
        (self.project / "models/purchased_village_crate_near.glb").write_bytes(glb())
        (self.project / "shaders/world_skeletal.vs.glsl").write_text("void main(){}\n")
        (self.project / "shaders/world_dynamic.vs.glsl").write_text("void main(){}\n")
        data = self.database()
        self.assertEqual(data["live_runtime_dependencies"], [
            "models/animations/white_knight_brown_armor.glb",
            "models/animations/white_knight_brown_armor_demo.glb",
            "models/purchased_village_crate_near.glb", "shaders/world_skeletal.vs.glsl", "shaders/world_dynamic.vs.glsl"])
        record = next(item for item in data["files"] if item["path"] == "project.valwood")
        self.assertEqual(record["dependency_paths"], sorted(data["live_runtime_dependencies"]))

    def test_trusted_seldonne_cooked_model_bank_links_materialized(self):
        bank = self.engine / "projects/seldonne/models"
        bank.mkdir(parents=True)
        (bank / "castle.glb").write_bytes(glb())
        nested = bank / "props"
        nested.mkdir()
        (nested / "wagon.glb").write_bytes(glb())
        (self.project / "models/seldonne_castle.glb").symlink_to(bank / "castle.glb")
        (self.project / "models/seldonne_props").symlink_to(nested, target_is_directory=True)
        inventory = ProjectInventory(self.project, self.engine)
        files = {item["path"]: item for item in inventory.build()["files"]}
        for name, original in (("models/seldonne_castle.glb", bank / "castle.glb"),
                               ("models/seldonne_props/wagon.glb", nested / "wagon.glb")):
            self.assertTrue(files[name]["symlink_materialized"])
            self.assertEqual(files[name]["source_identity"], "engine:" + original.relative_to(self.engine).as_posix())
            self.assertEqual(inventory.sources[name].read_verified(), original.read_bytes())

    def test_sibling_sources_saves_and_private_bank_targets_rejected(self):
        bank = self.engine / "projects/seldonne/models"
        bank.mkdir(parents=True)
        link = self.project / "models/private.glb"
        for destination in (self.engine / "projects/seldonne/saves/private.glb",
                            self.engine / "projects/seldonne/source/private.glb",
                            bank / "credentials/private.glb", bank / "saves/private.glb",
                            self.project / "saves/private.glb"):
            with self.subTest(destination=destination):
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(glb())
                link.symlink_to(destination)
                with self.assertRaises(ValidationError):
                    self.database()
                link.unlink()

    def test_symlinked_sibling_project_cannot_expand_trust_boundary(self):
        outside = self.root / "external-project/models"
        outside.mkdir(parents=True)
        (outside / "private.glb").write_bytes(glb())
        (self.engine / "projects/seldonne").symlink_to(outside.parent, target_is_directory=True)
        (self.project / "models/private.glb").symlink_to(outside / "private.glb")
        with self.assertRaisesRegex(ValidationError, "escapes"):
            self.database()

    def test_repeatable_content_and_source_preservation(self):
        before = {p: p.read_bytes() for p in self.project.rglob("*") if p.is_file()}
        first, second = self.database(), self.database()
        self.assertEqual(canonical_json(first), canonical_json(second))
        self.assertEqual(before, {p: p.read_bytes() for p in self.project.rglob("*") if p.is_file()})
        self.assertEqual(first["scope"], "conservative-full-project-runtime")
        self.assertNotIn(b"/home/private", canonical_json(first))
        files = {item["path"]: item for item in first["files"]}
        self.assertTrue(files["shaders/test.fs.glsl"]["symlink_materialized"])
        self.assertEqual(files["models/source_manifest.json"]["dependencies"],
                         [files["models/prop.glb"]["asset_id"]])
        self.assertEqual(files["graphics.settings"]["sha256"], hashlib.sha256(b"scale=.7\n").hexdigest())

    def test_stable_id_changes_version_when_contents_change(self):
        first = next(item for item in self.database()["assets"] if item["logical_key"] == "path:ground/dirt.png")
        (self.project / "ground/dirt.png").write_bytes(b"higherquality")
        second = next(item for item in self.database()["assets"] if item["logical_key"] == first["logical_key"])
        self.assertEqual(first["id"], second["id"])
        self.assertNotEqual(first["version"], second["version"])

    def test_import_alias_stays_stable_across_revisions(self):
        for revision in ("a" * 24, "b" * 24):
            path = self.project / "models/imports/tree" / revision
            path.mkdir(parents=True)
            (path / "asset.glb").write_bytes(glb())
            (path / "asset_mid.glb").write_bytes(glb())
        document = self.project / "world.studio"
        document.write_text("VALWOOD_STUDIO 3\nasset tree imports/tree/" + "a" * 24 + "/asset 2 1 0 220 60 1\n")
        first = next(item for item in self.database()["assets"] if item["logical_key"] == "import:tree")
        document.write_text(document.read_text().replace("a" * 24, "b" * 24))
        second = next(item for item in self.database()["assets"] if item["logical_key"] == "import:tree")
        self.assertEqual(first["id"], second["id"])
        self.assertEqual(len(first["artifacts"]), 4)
        self.assertEqual(first["import_recipe"]["settings"]["units"], 1)

    def test_previous_database_retains_project_identity_after_rename(self):
        previous = self.database()
        (self.project / "project.valwood").write_text("name=A new title\n")
        changed = ProjectInventory(self.project, self.engine, previous=previous).build()
        self.assertEqual(previous["project_id"], changed["project_id"])

    def test_missing_declared_dependency_rejected(self):
        (self.project / "models/source_manifest.json").write_text('{"runtime_dependencies":["missing.glb"]}')
        with self.assertRaisesRegex(ValidationError, "Missing runtime dependency"):
            self.database()

    def test_external_gltf_dependency_is_local_and_required(self):
        path = self.project / "models/prop.glb"
        path.write_bytes(glb({"asset": {"version": "2.0"}, "images": [{"uri": "paint.png"}]}))
        with self.assertRaisesRegex(ValidationError, "Missing runtime dependency"):
            self.database()
        (self.project / "models/paint.png").write_bytes(b"paint")
        data = self.database()
        record = next(item for item in data["files"] if item["path"] == "models/prop.glb")
        self.assertEqual(record["dependency_paths"], ["models/paint.png"])

    def test_traversal_absolute_and_network_gltf_rejected(self):
        for uri in ("../../private.png", "%2e%2e/secret.png", "/tmp/paint.png", "https://example.invalid/paint.png", "file:///etc/passwd", "C:\\secret.png"):
            with self.subTest(uri=uri):
                (self.project / "models/prop.glb").write_bytes(glb({"asset": {"version": "2.0"}, "images": [{"uri": uri}]}))
                with self.assertRaises(ValidationError):
                    self.database()

    def test_unsafe_symlink_and_private_target_rejected(self):
        path = self.project / "models/unsafe.glb"
        outside = self.root / "secret.glb"
        outside.write_bytes(glb())
        path.symlink_to(outside)
        with self.assertRaisesRegex(ValidationError, "escapes"):
            self.database()
        path.unlink()
        private = self.engine / "assets/Downloads/secret.glb"
        private.parent.mkdir(parents=True)
        private.write_bytes(glb())
        path.symlink_to(private)
        with self.assertRaisesRegex(ValidationError, "private"):
            self.database()

    def test_broken_and_cycle_symlinks_rejected(self):
        path = self.project / "models/missing.glb"
        path.symlink_to("missing-target.glb")
        with self.assertRaises(ValidationError):
            self.database()
        path.unlink()
        (self.project / "models/loop").symlink_to(self.project / "models", target_is_directory=True)
        with self.assertRaisesRegex(ValidationError, "Cyclic"):
            self.database()

    def test_sources_saves_credentials_and_captures_excluded(self):
        (self.project / "models/purchased.zip").write_bytes(b"private")
        (self.project / "models/source.blend").write_bytes(b"private")
        (self.project / "models/.secrets.json").write_text('{"password":"private"}')
        (self.project / "models/credentials.json").write_text('{"password":"should-never-copy"}')
        for folder in ("saves", "captures", ".ssh", "Downloads"):
            (self.project / folder).mkdir()
            (self.project / folder / "private.txt").write_text("private")
        payload = canonical_json(self.database())
        self.assertNotIn(b"password", payload)
        self.assertNotIn(b"should-never-copy", payload)
        self.assertNotIn(b"private.txt", payload)
        self.assertFalse(any(item["path"].endswith((".zip", ".blend")) for item in self.database()["files"]))

    def test_authored_npc_memory_included_player_memory_saves_excluded(self):
        authored = self.project / "ai/memories/keeper.vwmemory"
        authored.parent.mkdir(parents=True)
        authored.write_text('VALWOOD_NPC_MEMORY 1\nPROFILE "keeper" 0 "Keeper" "Host" "Town" "A fictional keeper."\nTRAITS 0\nMEMORIES 0\nEND\n')
        saved = self.project / "saves/keeper.vwmemory"
        saved.parent.mkdir()
        saved.write_text("private player progress")
        paths = {item["path"] for item in self.database()["files"]}
        self.assertIn("ai/memories/keeper.vwmemory", paths)
        self.assertNotIn("saves/keeper.vwmemory", paths)

    def test_studio_missing_environment_and_asset_rejected(self):
        for row in ("environment environments/missing.hdr 0 1 1 1", "asset new imports/missing/asset 1 1 0 200 50 1", "environment ../secret.hdr 0 1 1 1"):
            (self.project / "world.studio").write_text("VALWOOD_STUDIO 3\n" + row + "\n")
            with self.assertRaises(ValidationError):
                self.database()

    def test_studio6_full_transform_metadata_accepted(self):
        (self.project / "world.studio").write_text("VALWOOD_STUDIO 6\nlook .28 1 1 1 17 0 .2 0 1 .6\nprop test_prop 0 0 40 1 0 0 0 23 -17 1.2 .8 1.15\n")
        files = {item["path"]: item for item in self.database()["files"]}
        self.assertIn("world.studio", files)

    def test_studio5_surface_image_closure(self):
        header = 'VALWOOD_STUDIO 5\nsurfaces "VALWOOD_SURFACES 2"\n'
        (self.project / "surfaces").mkdir()
        (self.project / "surfaces/image_0123456789abcdef_0.img").write_bytes(b"immutable encoded image fixture")
        image = 'image "Soil" "surfaces/image_0123456789abcdef_0.img" 0'
        (self.project / "world.studio").write_text(header + "surfaces " + json.dumps(image) + "\n")
        files = {item["path"]: item for item in self.database()["files"]}
        self.assertEqual(files["world.studio"]["dependency_paths"], ["surfaces/image_0123456789abcdef_0.img"])
        (self.project / "world.studio").write_text(header + "surfaces " + json.dumps(image.replace("image_0123456789abcdef_0.img", "missing.img")) + "\n")
        with self.assertRaisesRegex(ValidationError, "Missing runtime dependency"):
            self.database()

    def test_compiled_scenery_alias_missing_mesh_rejected(self):
        (self.project / "models/prop.glb").unlink()
        with self.assertRaisesRegex(ValidationError, "Missing runtime dependency"):
            self.database()

    def test_source_mutation_detected_and_atomic_output_never_overwrites(self):
        inventory = ProjectInventory(self.project, self.engine)
        data = inventory.build()
        source = inventory.sources["ground/dirt.png"]
        (self.project / "ground/dirt.png").write_bytes(b"changed")
        with self.assertRaisesRegex(ValidationError, "changed"):
            source.read_verified()
        path = self.root / "database.json"
        write_new_json(path, data)
        original = path.read_bytes()
        with self.assertRaises(FileExistsError):
            write_new_json(path, {"changed": True})
        self.assertEqual(original, path.read_bytes())
        with self.assertRaises(ValidationError):
            write_new_json(self.project / "new.json", data, forbidden_roots=[self.project])


if __name__ == "__main__":
    unittest.main()

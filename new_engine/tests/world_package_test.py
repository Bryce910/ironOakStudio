#!/usr/bin/env python3
"""Isolated CPU-only Linux packaging and relocatable headless-launch contracts.

The tiny C fixture checks runtime arguments/files without SDL or a GPU. It does
not substitute for the production rpg native smoke test on the target machine.
"""
import contextlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from world_asset_database_test import fixture
from world_asset_database import ValidationError, canonical_json
from build_world_package import (build_package, main, native_dependencies,
                                 validate_inputs, verify_package, validation_project)

SMOKE_SOURCE = r'''
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int main(int argc,char **argv){
 const char *project=0; int headless=0,frames=0;
 for(int i=1;i<argc;i++){
  if(!strcmp(argv[i],"--project")&&i+1<argc) project=argv[++i];
  else if(!strcmp(argv[i],"--headless")) headless=1;
  else if(!strcmp(argv[i],"--frames")&&i+1<argc) frames=!strcmp(argv[++i],"1");
 }
 if(!project||!headless||!frames||chdir(project))return 41;
 const char *files[]={"project.valwood","world.studio","graphics.settings",
   "models/prop.glb","shaders/test.fs.glsl","ground/dirt.png","particles/fire.vfx",
   "audio/wind.wav","fonts/face.ttf","scenery/fixtures_instances.txt"};
 for(unsigned i=0;i<sizeof(files)/sizeof(*files);i++){
  FILE *f=fopen(files[i],"rb");if(!f)return 42;fclose(f);
 }
 puts("PACKAGED_HEADLESS_FIXTURE_OK");return 0;
}
'''


@unittest.skipUnless(sys.platform.startswith("linux") and shutil.which("cc") and
                     shutil.which("readelf") and shutil.which("ldconfig"),
                     "Linux C compiler/readelf/ldconfig required for CPU fixture")
class WorldPackageTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.compiler_temp = tempfile.TemporaryDirectory(prefix="valwood-package-binary-")
        root = Path(cls.compiler_temp.name)
        (root / "smoke.c").write_text(SMOKE_SOURCE)
        cls.binary = root / "smoke"
        subprocess.run(["cc", "-std=c99", "-O2", str(root / "smoke.c"), "-o", str(cls.binary)],
                       check=True, capture_output=True)

    @classmethod
    def tearDownClass(cls):
        cls.compiler_temp.cleanup()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="valwood-package-test-")
        self.root = Path(self.temp.name)
        self.engine, self.project = fixture(self.root)
        shutil.copyfile(self.binary, self.engine / "rpg")
        (self.engine / "rpg").chmod(0o755)
        (self.engine / "docs/licenses").mkdir(parents=True)
        (self.engine / "docs/licenses/fixture.txt").write_text("Fixture license.\n")
        (self.project / "CREDITS_ASSETS.txt").write_text("Purchased source /home/private/Downloads/character.blend\n")

    def tearDown(self):
        self.temp.cleanup()

    def validated(self):
        return validate_inputs(self.project, self.engine)

    def snapshot(self):
        return {str(path.relative_to(self.engine)): (path.read_bytes(), path.stat().st_mode)
                for path in self.engine.rglob("*") if path.is_file()}

    def test_cache_selection_helper_is_delivered_with_packaging_tools(self):
        tools=self.engine/'tools';tools.mkdir()
        helper=tools/'world_package_cache_selection.py';helper.write_text('# cache selector fixture\n')
        validated=self.validated()
        self.assertIn(('tools/world_package_cache_selection.py',helper),[(name,path)for name,path,_ in validated[8]])
        # The validated tuple's extra inputs must also survive the real writer.
        package=self.root/'package';build_package(validated,package)
        self.assertEqual((package/'tools/world_package_cache_selection.py').read_bytes(),helper.read_bytes())

    def test_linked_authoring_validated_in_ordinary_temporary_view(self):
        sibling=self.engine/'projects/shared/ai';sibling.mkdir(parents=True)
        (sibling/'notes.json').write_text('{}')
        (self.project/'ai').symlink_to(sibling,target_is_directory=True)
        tools=self.engine/'tools';tools.mkdir()
        validator=tools/'valwood_project_validate'
        validator.write_text('#!/usr/bin/env python3\nimport sys,json\nfrom pathlib import Path\np=Path(sys.argv[2]);assert (p/"ai").is_dir() and not (p/"ai").is_symlink()\nprint(json.dumps({"ok":True,"components_validated":0,"capabilities":[],"view":str(p)}))\n')
        validator.chmod(0o755)
        before=self.snapshot();result=self.validated()
        view=Path(result[1]['authoring_validation']['view'])
        self.assertFalse(view.exists())
        self.assertTrue((self.project/'ai').is_symlink())
        self.assertEqual(before,self.snapshot())

    def test_validation_view_cleanup_and_changed_source_rejected(self):
        from world_asset_database import ProjectInventory
        inventory=ProjectInventory(self.project,self.engine);inventory.build()
        with self.assertRaisesRegex(RuntimeError,'fixture interruption'):
            with validation_project(inventory)as view:
                target=view
                raise RuntimeError('fixture interruption')
        self.assertFalse(target.exists())
        with self.assertRaisesRegex(ValidationError,'Source changed during native validation'):
            with validation_project(inventory)as view:
                target=view
                (self.project/'models/prop.glb').write_bytes(b'changed while validator running')
        self.assertFalse(target.exists())

    def test_relocated_authoring_wrapper_uses_package_root(self):
        tools = self.engine / "tools"
        tools.mkdir(exist_ok=True)
        (tools / "build_world_package.py").write_text("import json,sys\nprint(json.dumps(sys.argv[1:]))\n")
        (tools / "import_world_asset.py").write_text("# standalone import fixture\n")
        validator = tools / "valwood_project_validate"
        validator.write_text('#!/usr/bin/env python3\nimport json\nprint(json.dumps({"ok":True,"capabilities":[],"components_validated":0}))\n')
        validator.chmod(0o755)

        destination = self.root / "editor wrapper"
        build_package(self.validated(), destination, mode="development")
        moved = self.root / "relocated editor wrapper"
        destination.rename(moved)
        command = [sys.executable, str(moved / "bin/tools/build_world_package.py"),
                   "--project", str(moved / "project"), "--engine-root", str(moved / "bin"),
                   "--validate-only", "--output", str(self.root / "unused")]
        arguments = json.loads(subprocess.check_output(command, cwd="/tmp", text=True))
        self.assertEqual(arguments[arguments.index("--engine-root") + 1], str(moved))
        self.assertEqual(arguments[arguments.index("--executable") + 1], str(moved / "bin/rpg"))
        self.assertTrue((moved / "bin/tools/import_world_asset.py").is_file())
        self.assertTrue((moved / "tools/import_world_asset.py").is_file())
        self.assertTrue(os.access(moved / "tools/valwood_project_validate", os.X_OK))

    def test_full_content_reproducible_and_sources_untouched(self):
        before = self.snapshot()
        first = build_package(self.validated(), self.root / "first")
        second = build_package(self.validated(), self.root / "second")
        self.assertEqual(first["content_id"], second["content_id"])
        self.assertEqual(before, self.snapshot())
        for relative in ("package-manifest.json", "asset-database.json", "asset-provenance.json", "native-dependencies.json", "build-recipe.json", "launch.sh"):
            self.assertEqual((self.root / "first" / relative).read_bytes(),
                             (self.root / "second" / relative).read_bytes())
        self.assertTrue(verify_package(self.root / "first")["verified"])
        for file in (self.root / "first").rglob("*"):
            self.assertFalse(file.is_symlink())
            self.assertEqual(file.stat().st_mtime_ns, 0)
        payload = (self.root / "first/asset-database.json").read_bytes()
        self.assertNotIn(str(self.engine).encode(), payload)
        self.assertNotIn(b"/home/private", (self.root / "first/project/CREDITS_ASSETS.txt").read_bytes())
        self.assertEqual((self.project / "graphics.settings").read_bytes(),
                         (self.root / "first/project/graphics.settings").read_bytes())

    def test_relocated_launch_without_source_tree_headless_fixture(self):
        build_package(self.validated(), self.root / "package")
        relocated = self.root / "moved parent with spaces/demo"
        relocated.parent.mkdir()
        (self.root / "package").rename(relocated)
        self.engine.rename(self.root / "source-not-at-original-location")
        result = subprocess.run([str(relocated / "launch.sh"), "--headless", "--frames", "1"],
                                cwd="/", text=True, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("PACKAGED_HEADLESS_FIXTURE_OK", result.stdout)
        self.assertTrue(verify_package(relocated)["verified"])

    def test_existing_empty_and_populated_output_never_replaced(self):
        validated = self.validated()
        for name in ("empty", "populated"):
            output = self.root / name
            output.mkdir()
            if name == "populated":
                (output / "original.txt").write_text("keep")
            inode = output.stat().st_ino
            with self.assertRaisesRegex(ValidationError, "already exists"):
                build_package(validated, output)
            self.assertEqual(inode, output.stat().st_ino)
        self.assertEqual((self.root / "populated/original.txt").read_text(), "keep")

    def test_output_inside_project_rejected(self):
        with self.assertRaisesRegex(ValidationError, "outside"):
            build_package(self.validated(), self.project / "new-package")

    def test_changed_source_and_failed_build_leave_no_partial_output(self):
        validated = self.validated()
        (self.project / "ground/dirt.png").write_bytes(b"changed after validation")
        with self.assertRaisesRegex(ValidationError, "changed"):
            build_package(validated, self.root / "package")
        self.assertFalse((self.root / "package").exists())
        self.assertEqual(list(self.root.glob(".valwood-package-*")), [])

    def test_symlink_retarget_after_validation_rejected(self):
        validated = self.validated()
        shader = self.project / "shaders/test.fs.glsl"
        shader.unlink()
        outside = self.root / "new.fs.glsl"
        outside.write_text("private")
        shader.symlink_to(outside)
        with self.assertRaisesRegex(ValidationError, "link changed"):
            build_package(validated, self.root / "package")
        self.assertFalse((self.root / "package").exists())

    def test_package_integrity_detects_tampering_and_symlinks(self):
        build_package(self.validated(), self.root / "package")
        path = self.root / "package/project/ground/dirt.png"
        original = path.read_bytes()
        path.write_bytes(b"corrupt")
        with self.assertRaisesRegex(ValidationError, "hash/length mismatch"):
            verify_package(self.root / "package")
        path.unlink()
        outside = self.root / "outside.png"
        outside.write_bytes(original)
        path.symlink_to(outside)
        with self.assertRaisesRegex(ValidationError, "external/symlink"):
            verify_package(self.root / "package")

    def test_native_inventory_leaves_glibc_loader_and_drivers_on_host(self):
        inventory, _ = native_dependencies(self.engine / "rpg")
        self.assertEqual(inventory["format"], "ELF")
        self.assertTrue(any(row["soname"] == "libc.so.6" for row in inventory["libraries"]))
        for row in inventory["libraries"]:
            if row["soname"].startswith(("libc.so", "ld-linux", "libGL", "libX11", "libwayland")):
                self.assertEqual(row["policy"], "host-system-library-or-driver")
        build_package(self.validated(), self.root / "bundled", bundle_libraries=True)
        self.assertEqual(list((self.root / "bundled").glob("lib/libc*")), [])
        self.assertEqual(list((self.root / "bundled").glob("lib/ld-*")), [])

    def test_validate_only_cli_writes_nothing_and_outputs_json(self):
        before = self.snapshot()
        with contextlib.redirect_stdout(io.StringIO()) as captured:
            code = main(["--project", str(self.project), "--engine-root", str(self.engine),
                         "--output", str(self.root / "ignored"), "--database-output", str(self.root / "ignored.json"), "--validate-only"])
        self.assertEqual(code, 0)
        self.assertTrue(json.loads(captured.getvalue())["validate_only"])
        self.assertFalse((self.root / "ignored").exists())
        self.assertFalse((self.root / "ignored.json").exists())
        self.assertEqual(before, self.snapshot())

    def test_missing_reference_cli_fails_before_any_output(self):
        (self.project / "models/source_manifest.json").write_text('{"runtime_dependencies":["missing.glb"]}')
        with contextlib.redirect_stdout(io.StringIO()) as captured:
            code = main(["--project", str(self.project), "--engine-root", str(self.engine),
                         "--output", str(self.root / "package")])
        self.assertEqual(code, 1)
        self.assertFalse(json.loads(captured.getvalue())["ok"])
        self.assertFalse((self.root / "package").exists())

    def test_live_components_require_a_current_native_schema_tool(self):
        (self.project / "world.components").write_text('VALWOOD_COMPONENTS 1\nassets "" "" ""\n')
        with self.assertRaisesRegex(ValidationError, "Build tools/valwood_project_validate"):
            self.validated()
        tools = self.engine / "tools"
        tools.mkdir(exist_ok=True)
        validator = tools / "valwood_project_validate"
        def report(capabilities):
            data = json.dumps({"ok": True, "checked_documents": 2, "components_validated": 1, "capabilities": capabilities})
            validator.write_text("#!/bin/sh\nprintf '%s\\n' '" + data + "'\n")
            validator.chmod(0o755)
        report([])
        with self.assertRaisesRegex(ValidationError, "Rebuild the native authoring validator"):
            self.validated()
        report(["typed-components-v1", "scene-id-closure-v1"])
        data = self.validated()
        self.assertTrue(data[1]["authoring_validation"]["ok"])
        build_package(data, self.root / "component-package")
        self.assertEqual((self.root / "component-package/project/world.components").read_bytes(),
                         (self.project / "world.components").read_bytes())


if __name__ == "__main__":
    unittest.main()

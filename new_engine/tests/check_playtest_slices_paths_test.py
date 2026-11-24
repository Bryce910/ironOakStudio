#!/usr/bin/env python3
"""CPU-only project-root selection; invalid overrides never launch a game."""
import json
from pathlib import Path
import subprocess
import sys
import unittest

sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'));sys.path.insert(0,str(ROOT/'tests'))
import prepare_playtest_slices as preparer
import prepare_playtest_slices_test as fixtures
from check_playtest_slices import project_paths


class PlaytestProjectPathsTest(unittest.TestCase):
    def setUp(self):
        self.fixture=fixtures.PreparePlaytestSlicesTest();self.fixture.setUp()
        self.engine=self.fixture.engine;self.output=self.fixture.output

    def tearDown(self):self.fixture.tearDown()

    def test_default_still_selects_existing_three_copies_with_shared_banks(self):
        self.fixture.build(tuple(preparer.PROJECTS))
        found=project_paths(None,list(preparer.PROJECTS),self.engine)
        self.assertEqual(found,{area:self.output/area/'project' for area in preparer.PROJECTS})
        self.assertTrue((found['grandstone']/'models').is_symlink())

    def test_override_selects_isolated_prepared_copy_and_preserves_installed_state(self):
        self.fixture.build(['grandstone']);before=self.fixture.snapshot(self.output)
        isolated=self.engine/'build/isolated-qa'
        preparer.prepare(self.engine,['grandstone'],isolated)
        found=project_paths(isolated,['grandstone'],self.engine)
        self.assertEqual(found['grandstone'],isolated/'grandstone/project')
        self.assertEqual(self.fixture.snapshot(self.output),before)

    def test_source_project_and_root_or_project_links_reject(self):
        with self.assertRaisesRegex(ValueError,'source projects'):
            project_paths(self.engine/'projects',['grandstone'],self.engine)
        self.fixture.build(['grandstone'])
        linked=self.engine/'linked-qa';linked.symlink_to(self.output,target_is_directory=True)
        with self.assertRaisesRegex(ValueError,'ordinary'):
            project_paths(linked,['grandstone'],self.engine)
        project=self.output/'grandstone/project';moved=project.with_name('preserved-project');project.rename(moved)
        project.symlink_to(moved,target_is_directory=True)
        with self.assertRaisesRegex(ValueError,'ordinary'):
            project_paths(self.output,['grandstone'],self.engine)

    def test_missing_or_wrong_manifest_and_linked_route_reject(self):
        self.fixture.build(['grandstone']);folder=self.output/'grandstone'
        manifest=folder/preparer.MANIFEST;original=manifest.read_bytes();manifest.unlink()
        with self.assertRaisesRegex(ValueError,'manifest'):
            project_paths(self.output,['grandstone'],self.engine)
        manifest.write_text(json.dumps({'schema':'valwood.local-playtest-copy','area':'city'}))
        with self.assertRaisesRegex(ValueError,'manifest'):
            project_paths(self.output,['grandstone'],self.engine)
        manifest.write_bytes(original)
        route=folder/'project/playtest.route';route.unlink()
        route.symlink_to(self.engine/'docs/playtest-slices/2026-11-02/grandstone/runtime/playtest.route')
        with self.assertRaisesRegex(ValueError,'ordinary'):
            project_paths(self.output,['grandstone'],self.engine)

    def test_invalid_cli_override_fails_before_output_or_any_game_launch(self):
        output=self.engine/'results'
        run=subprocess.run([sys.executable,str(ROOT/'tools/check_playtest_slices.py'),
            '--area','grandstone','--mode','combat','--project-root',str(self.engine/'missing'),
            '--output',str(output)],capture_output=True,text=True)
        self.assertEqual(run.returncode,2)
        self.assertIn('ordinary prepared-copy directory',run.stderr)
        self.assertFalse(output.exists())


if __name__=='__main__':unittest.main()

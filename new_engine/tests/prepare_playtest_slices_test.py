"""CPU-only fixtures for local playtest source isolation and narrow refresh."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT=Path(__file__).resolve().parents[1]
sys.dont_write_bytecode=True
sys.path.insert(0,str(ROOT/'tools'))
import prepare_playtest_slices as preparer


class PreparePlaytestSlicesTest(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='valwood-local-slices-')
        self.engine=Path(self.temp.name)/'new_engine';self.engine.mkdir()
        self.output=self.engine/'build/playtest-slices'
        self.sources={}
        for area,name in preparer.PROJECTS.items():
            project=self.engine/'projects'/name;project.mkdir(parents=True);self.sources[area]=project
            for filename,raw in [('project.valwood',b'name=Fixture'),('world.studio',b'VALWOOD_STUDIO 6\n'),
                                 ('graphics.settings',b'private-settings'),('city.scene',b'fixture scene'),
                                 ('world.studio.workspace',b'private workspace'),('world.studio.recovery',b'private recovery'),
                                 ('studio-layout.ini',b'private layout'),('secrets.json',b'private secret'),('.lock',b'private lock')]:
                (project/filename).write_bytes(raw)
            for bank in ('models','ground','scenery','audio'):
                (project/bank).mkdir();(project/bank/'keep.bin').write_bytes(b'immutable-bank')
            for tree in ('ai','shaders'):
                (project/tree).mkdir();(project/tree/'keep.txt').write_text('independent mutable tree')
            for folder in ('saves','qa','captures','backups'):
                (project/folder).mkdir();(project/folder/'keep.txt').write_text('private source state')
            runtime=self.engine/'docs/playtest-slices/2026-11-02'/area/'runtime';runtime.mkdir(parents=True)
            (runtime/'playtest.route').write_text('route-'+area)
            if area=='grandstone':
                for name in ('playtest-hall.route','approach.physics','approach.settings','hall.physics','hall.settings'):
                    (runtime/name).write_text(name)
        self.binary=self.engine/'final-runtime';self.binary.write_bytes(b'guarded-runtime');self.binary.chmod(0o755)

    def tearDown(self): self.temp.cleanup()

    def build(self,areas=('city',),**kwargs):
        return preparer.prepare(self.engine,areas,self.output,**kwargs)

    def snapshot(self,path):
        return {p.relative_to(path).as_posix():p.read_bytes() for p in path.rglob('*') if p.is_file()}

    def test_three_small_copies_have_own_mutable_state_and_resolved_shared_banks(self):
        before={a:self.snapshot(p) for a,p in self.sources.items()}
        rows=self.build(tuple(preparer.PROJECTS),binary=self.binary)
        self.assertEqual(len(rows),3)
        for area,source in self.sources.items():
            project=self.output/area/'project';origin=json.loads((self.output/area/preparer.MANIFEST).read_text())
            self.assertEqual(list((project/'saves').iterdir()),[])
            for name in ('world.studio','graphics.settings','ai/keep.txt','shaders/keep.txt'):
                self.assertFalse((project/name).is_symlink());self.assertEqual((project/name).stat().st_nlink,1)
            for name in ('world.studio.workspace','world.studio.recovery','studio-layout.ini','secrets.json','.lock','qa','captures','backups'):
                self.assertFalse(os.path.lexists(project/name),name)
            self.assertEqual(origin['source_project'],str(source))
            self.assertTrue((project/'models').is_symlink())
            self.assertEqual(os.readlink(project/'models'),str(source/'models'))
            (project/'ai/keep.txt').write_text('local NPC memory')
            (project/'shaders/keep.txt').write_text('local shader copy')
            (project/'graphics.settings').write_text('local quality')
            self.assertEqual(self.snapshot(source),before[area])
        self.assertEqual((self.output/'bin/rpg').read_bytes(),b'guarded-runtime')
        self.assertFalse((self.output/'bin/rpg').is_symlink())
        self.assertEqual((self.output/'bin/rpg').stat().st_nlink,1)
        self.assertTrue((self.output/'grandstone/project/playtest/hall.physics').is_file())

    def test_refresh_changes_only_generated_data_and_preserves_saves_settings_ai(self):
        self.build();project=self.output/'city/project'
        (project/'saves/playtest.session').write_bytes(b'player progress')
        (project/'graphics.settings').write_bytes(b'user preferences')
        (project/'ai/keep.txt').write_bytes(b'local memory')
        before=self.snapshot(project); runtime=self.engine/'docs/playtest-slices/2026-11-02/city/runtime/playtest.route'
        runtime.write_text('new route')
        rows=self.build(refresh=True);after=self.snapshot(project)
        self.assertTrue(rows[0]['refreshed_runtime_only'])
        self.assertEqual({k:v for k,v in before.items() if k!='playtest.route'},
                         {k:v for k,v in after.items() if k!='playtest.route'})
        self.assertEqual(after['playtest.route'],b'new route')

    def test_override_and_refresh_keep_source_defaults_and_private_user_state(self):
        default=self.engine/'docs/playtest-slices/2026-11-02/grandstone/runtime'
        override=self.engine/'staged-grandstone-runtime';shutil.copytree(default,override)
        for path in override.iterdir():path.write_bytes(b'staged-v1-'+path.read_bytes())
        defaults=self.snapshot(default);source=self.snapshot(self.sources['grandstone'])
        self.build(['grandstone'],runtime_directory=override)
        project=self.output/'grandstone/project'
        (project/'saves/playtest.progress').write_bytes(b'user progress')
        (project/'graphics.settings').write_bytes(b'user preferences')
        (project/'ai/keep.txt').write_bytes(b'user AI state')
        before=self.snapshot(project)
        generated=preparer.runtime_inputs(self.engine,'grandstone',override)
        for name,path in generated.items():
            self.assertEqual((project/name).read_bytes(),path.read_bytes())
            path.write_bytes(b'staged-v2-'+path.read_bytes())
        rows=self.build(['grandstone'],runtime_directory=override,refresh=True)
        after=self.snapshot(project)
        self.assertTrue(rows[0]['refreshed_runtime_only'])
        self.assertEqual({k:v for k,v in before.items() if k not in generated},
                         {k:v for k,v in after.items() if k not in generated})
        for name,path in generated.items():self.assertEqual(after[name],path.read_bytes())
        self.assertEqual(self.snapshot(default),defaults)
        self.assertEqual(self.snapshot(self.sources['grandstone']),source)

    def test_override_preflight_rejects_multiple_areas_missing_data_and_symlink(self):
        override=self.engine/'staged-grandstone-runtime'
        shutil.copytree(self.engine/'docs/playtest-slices/2026-11-02/grandstone/runtime',override)
        with self.assertRaisesRegex(ValueError,'single playtest area'):
            self.build(['city','grandstone'],runtime_directory=override,binary=self.binary)
        self.assertFalse(self.output.exists())
        path=override/'hall.physics';path.unlink()
        with self.assertRaisesRegex(ValueError,'not ready'):
            self.build(['grandstone'],runtime_directory=override)
        self.assertFalse(self.output.exists())
        path.symlink_to(self.engine/'docs/playtest-slices/2026-11-02/grandstone/runtime/hall.physics')
        with self.assertRaisesRegex(ValueError,'ordinary file'):
            self.build(['grandstone'],runtime_directory=override)
        self.assertFalse(self.output.exists())

    def test_override_refresh_rejects_independent_runtime_edits_without_user_state_changes(self):
        override=self.engine/'staged-city-runtime';override.mkdir()
        (override/'playtest.route').write_bytes(b'isolated route')
        self.build(runtime_directory=override);project=self.output/'city/project'
        (project/'saves/playtest.progress').write_bytes(b'user progress')
        (project/'playtest.route').write_bytes(b'artist route edit')
        before=self.snapshot(self.output)
        (override/'playtest.route').write_bytes(b'new staged route')
        with self.assertRaisesRegex(ValueError,'independently modified'):
            self.build(runtime_directory=override,refresh=True)
        self.assertEqual(self.snapshot(self.output),before)

    def test_existing_output_and_independently_modified_generated_file_are_rejected(self):
        self.build();project=self.output/'city/project';(project/'saves/keep').write_text('keep')
        before=self.snapshot(self.output)
        with self.assertRaisesRegex(ValueError,'Existing output'):self.build()
        self.assertEqual(self.snapshot(self.output),before)
        (project/'playtest.route').write_text('user edit');before=self.snapshot(self.output)
        with self.assertRaisesRegex(ValueError,'independently modified'):self.build(refresh=True)
        self.assertEqual(self.snapshot(self.output),before)

    def test_source_revision_change_and_output_inside_source_are_rejected(self):
        self.build();(self.sources['city']/'world.studio').write_text('source changed')
        before=self.snapshot(self.output)
        with self.assertRaisesRegex(ValueError,'source revision changed'):self.build(refresh=True)
        self.assertEqual(self.snapshot(self.output),before)
        with self.assertRaisesRegex(ValueError,'source projects'):
            preparer.prepare(self.engine,['city'],self.sources['city']/'bad-output')

    def test_refresh_never_follows_generated_symlink(self):
        self.build(['grandstone']);project=self.output/'grandstone/project';outside=Path(self.temp.name)/'outside'
        outside.write_text('do not touch');(project/'playtest/hall.physics').unlink()
        (project/'playtest/hall.physics').symlink_to(outside)
        with self.assertRaisesRegex(ValueError,'symlink'):self.build(['grandstone'],refresh=True)
        self.assertEqual(outside.read_text(),'do not touch')

    def test_failed_staging_is_removed_and_source_is_preserved(self):
        before=self.snapshot(self.sources['city']);actual=preparer.checked_copy
        def fail(source,target,expected):
            actual(source,target,expected)
            if target.name=='world.studio':raise OSError('fixture disk failure')
        with mock.patch.object(preparer,'checked_copy',side_effect=fail):
            with self.assertRaisesRegex(OSError,'fixture disk'):self.build()
        self.assertFalse((self.output/'city').exists());self.assertEqual(list(self.output.iterdir()),[])
        self.assertEqual(self.snapshot(self.sources['city']),before)

    def test_mutable_source_symlinks_are_materialized_and_cycles_rejected(self):
        source=self.sources['city'];tree=source/'ai';shutil.rmtree(tree)
        tree.symlink_to(self.sources['jungle']/'ai',target_is_directory=True)
        self.build();project=self.output/'city/project'
        self.assertFalse((project/'ai').is_symlink());self.assertFalse((project/'ai/keep.txt').is_symlink())
        cycle=self.sources['jungle']/'ai/cycle';cycle.symlink_to(self.sources['jungle']/'ai',target_is_directory=True)
        with self.assertRaisesRegex(ValueError,'Cyclic'):self.build(['jungle'])

    def test_all_requested_runtime_data_preflight_before_any_output(self):
        (self.engine/'docs/playtest-slices/2026-11-02/grandstone/runtime/hall.settings').unlink()
        with self.assertRaisesRegex(ValueError,'not ready'):self.build(['city','grandstone'])
        self.assertFalse(self.output.exists())

    def test_launcher_selects_private_project_and_environment_without_running_game(self):
        self.build();workspace=self.engine.parent
        for name in ('Launch_Valwood_Playtest.sh','Launch_Valwood_Playtest_City.sh'):
            shutil.copy2(ROOT.parent/name,workspace/name)
        (self.engine/'projects/seldonne').mkdir()
        binary=self.output/'bin/rpg';binary.parent.mkdir(exist_ok=True)
        binary.write_text('#!/usr/bin/env python3\nimport os,sys,json\nprint(json.dumps({"argv":sys.argv[1:],"area":os.getenv("VALWOOD_PLAYTEST_SLICE"),"presentation":os.getenv("VALWOOD_PRESENTATION"),"showcase":os.getenv("VALWOOD_SHOWCASE")}))\n')
        binary.chmod(0o755)
        env=dict(os.environ,VALWOOD_SHOWCASE='jungle')
        run=subprocess.run([str(workspace/'Launch_Valwood_Playtest_City.sh'),'--frames','2'],env=env,capture_output=True,text=True)
        self.assertEqual(run.returncode,0,run.stderr);result=json.loads(run.stdout)
        self.assertEqual(result['area'],'city');self.assertEqual(result['presentation'],'1');self.assertIsNone(result['showcase'])
        self.assertEqual(result['argv'][1],str(self.output/'city/project'));self.assertNotIn('--no-hud',result['argv'])
        blocked=subprocess.run([str(workspace/'Launch_Valwood_Playtest_City.sh'),'--editor'],capture_output=True)
        self.assertEqual(blocked.returncode,2)


if __name__=='__main__':unittest.main()

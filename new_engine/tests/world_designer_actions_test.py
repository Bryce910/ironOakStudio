"""Real tiny bundles/workspaces and guarded process fixtures. No GPU is used."""
import contextlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import unittest
from unittest import mock
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT/'tools'))
sys.path.insert(0, str(ROOT/'tests'))
from windows_artist_bundle_test import WindowsArtistBundleTest
import build_windows_artist_bundle as bundle
import world_designer_actions as actions
from world_artist_handoff import review_return


class WorldDesignerActionsTest(unittest.TestCase):
    def setUp(self):
        self.fixture = WindowsArtistBundleTest()
        self.fixture.setUp()
        self.addCleanup(self.fixture.tearDown)
        # Installer/helper closure is real; renderer bytes are inert tiny PEs.
        name = 'world_package_cache_selection.py'
        (self.fixture.engine/'tools'/name).write_bytes((ROOT/'tools'/name).read_bytes())
        self.root = self.fixture.root
        self.extracted = self.root/'World designer é & spaces'
        guide = self.root/'WORLD_DESIGNER_GUIDE.md'; guide.write_text('World designer guide')
        fake_gui = self.root/'Valwood World Designer.exe'; fake_gui.write_bytes(b'Inert GUI fixture')
        plans = [(label, self.fixture.validated(self.fixture.second if label == 'jungle' else self.fixture.project))
                 for label in actions.PROJECTS]
        archive = self.root/'designer.zip'
        with contextlib.redirect_stderr(io.StringIO()):
            bundle.build_bundle(plans, archive, extra_bootstrap=[
                ('world_designer_actions.py', ROOT/'tools/world_designer_actions.py'),
                ('WORLD_DESIGNER_GUIDE.md', guide), ('Valwood World Designer.exe', fake_gui)])
        with zipfile.ZipFile(archive) as stream: stream.extractall(self.extracted)
        self.original = {p.relative_to(self.extracted): p.read_bytes() for p in self.extracted.rglob('*') if p.is_file()}
        with contextlib.redirect_stdout(io.StringIO()): self.designer = actions.Designer(self.extracted)

    def run_action(self, action='new', label='city'):
        with contextlib.redirect_stdout(io.StringIO()): return self.designer.run(action, label)

    def test_first_open_independent_copies_and_reopen_preserves_edits(self):
        with mock.patch.object(self.designer, 'open_editor', side_effect=lambda path, label: dict(ok=True, workspace=str(path), label=label)):
            first = self.run_action('open')
            work = Path(first['workspace'])
            self.assertEqual(work, self.designer.base/'Workspaces/city')
            seed = self.designer.base/'Seeds/city'
            (work/'project/world.studio').write_text('artist saved edits')
            self.assertNotEqual((seed/'project/world.studio').read_text(), 'artist saved edits')
            again = self.run_action('open')
            self.assertEqual(again['workspace'], first['workspace'])
            self.assertEqual((work/'project/world.studio').read_text(), 'artist saved edits')
            self.assertEqual(self.original, {p.relative_to(self.extracted): p.read_bytes() for p in self.extracted.rglob('*') if p.is_file()})
            for path in work.rglob('*'):
                self.assertFalse(path.is_symlink())
                if path.is_file(): self.assertEqual(path.stat().st_nlink, 1)

    def test_new_copy_keeps_previous_edits_and_changes_selection(self):
        first = Path(self.run_action()['workspace'])
        (first/'project/world.studio').write_text('keep my scene')
        second = Path(self.run_action()['workspace'])
        self.assertNotEqual(first, second)
        self.assertEqual((first/'project/world.studio').read_text(), 'keep my scene')
        self.assertEqual(self.designer.workspace_path('city'), second)
        self.assertNotEqual((second/'project/world.studio').read_text(), 'keep my scene')

    def test_export_saved_delta_and_owner_conflict(self):
        work = Path(self.run_action()['workspace'])
        (work/'project/world.studio').write_text('saved scene change')
        result = self.run_action('export')
        packet = Path(result['output'])
        self.assertEqual(result['changed_files'], 1)
        seed = self.designer.base/'Seeds/city'
        self.assertTrue(review_return(packet, seed)['ok'])
        (seed/'project/world.studio').write_text('independent owner change')
        self.assertEqual(review_return(packet, seed)['conflicts'], ['world.studio'])
        self.assertFalse((self.root/'unrequested-review').exists())

    def test_empty_return_and_changed_baseline_are_preserved(self):
        work = Path(self.run_action()['workspace'])
        with self.assertRaisesRegex(ValueError, 'No saved'): self.run_action('export')
        before = (work/'artist-baseline.json').read_bytes()
        base = json.loads(before); base['files']['world.studio']['sha256'] = '0'*64
        (work/'artist-baseline.json').write_bytes(actions.canonical(base))
        with self.assertRaisesRegex(ValueError, 'baseline changed'): self.run_action('open')
        self.assertEqual(json.loads((work/'artist-baseline.json').read_text()), base)

    def test_wrong_project_marker_and_runtime_are_rejected(self):
        work = Path(self.run_action()['workspace'])
        marker = work/actions.MARKER; original = marker.read_bytes()
        doc = json.loads(original); doc['label'] = 'jungle'; marker.write_bytes(actions.canonical(doc))
        with self.assertRaisesRegex(ValueError, 'another project'): self.run_action('open')
        marker.write_bytes(original)
        (work/'bin/rpg.exe').write_bytes(b'changed exe')
        with self.assertRaisesRegex(ValueError, 'binary changed'): self.run_action('open')

    def test_existing_unknown_default_workcopy_is_never_adopted(self):
        target = self.designer.base/'Workspaces/city'; target.mkdir()
        (target/'keep').write_bytes(b'existing work')
        with self.assertRaises((ValueError, OSError)): self.run_action('open')
        self.assertEqual((target/'keep').read_bytes(), b'existing work')
        self.assertEqual(list(target.iterdir()), [target/'keep'])

    def test_workspace_traversal_symlink_and_shortage_rejected(self):
        state = self.designer.base/'workspace-pointers.json'
        state.write_text('{"city":"../outside"}')
        with self.assertRaises(ValueError): self.run_action('open')
        state.unlink()
        (self.designer.base/'Workspaces/city').symlink_to(self.root, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'Links'): self.run_action('open')
        (self.designer.base/'Workspaces/city').unlink()
        usage = mock.Mock(free=1)
        with mock.patch.object(actions.shutil, 'disk_usage', return_value=usage):
            with self.assertRaisesRegex(ValueError, 'Not enough free'): self.run_action('open')
        self.assertFalse((self.designer.base/'Workspaces/city').exists())

    def test_process_lock_and_live_editor_guard(self):
        with actions.exclusive(self.designer.base/'.designer.lock'):
            with self.assertRaisesRegex(ValueError, 'already copying'): self.run_action('new')
        record = dict(pid=123, created=456, executable='C:\\World\\rpg.exe')
        (self.designer.base/'editor-session.json').write_bytes(actions.canonical(record))
        with mock.patch.object(actions, 'process_identity', return_value=record):
            with self.assertRaisesRegex(ValueError, 'still open'): self.run_action('new')
        with mock.patch.object(actions, 'process_identity', return_value={**record, 'created':999}):
            self.assertTrue(self.run_action('new')['ok'])

    def test_editor_args_and_env_never_use_shell_or_play_flags(self):
        work = Path(self.run_action()['workspace'])
        observed = {}
        class Child:
            pid = 12345
            def wait(self): return 0
        def launch(command, **kwargs): observed.update(command=command, **kwargs); return Child()
        with mock.patch.object(actions.subprocess, 'Popen', side_effect=launch), mock.patch.object(actions, 'process_identity', return_value=None), mock.patch.dict(os.environ, {'VALWOOD_QA_BAD':'1','valwood_OPENING_PROLOGUE':'1','KEEP_ARTIST':'yes'}):
            with contextlib.redirect_stdout(io.StringIO()): self.designer.open_editor(work, 'city')
        self.assertEqual(observed['command'], actions.editor_command(work, 'city'))
        self.assertFalse(observed['shell'])
        self.assertNotIn('--play', observed['command'])
        self.assertTrue(all(not key.upper().startswith('VALWOOD_') for key in observed['env']))
        self.assertEqual(observed['env']['KEEP_ARTIST'], 'yes')
        self.assertIn('é & spaces', observed['command'][0])
        for label, (_, view) in actions.PROJECTS.items(): self.assertEqual(actions.editor_command(work, label)[-5], view)

    def test_inherited_status_path_cannot_write_outside_logs(self):
        with self.assertRaisesRegex(ValueError, 'Status file'): actions.Designer(self.extracted, self.root/'outside.txt')
        self.assertFalse((self.root/'outside.txt').exists())

    def test_wrong_seed_revision_cannot_be_replaced(self):
        self.run_action('new')
        manifest = self.designer.base/'Seeds/city/package-manifest.json'
        old = manifest.read_bytes(); manifest.write_bytes(old+b' ')
        with self.assertRaisesRegex(ValueError, 'another bundle revision'): self.run_action('open')
        self.assertEqual(manifest.read_bytes(), old+b' ')

    def test_editor_failure_keeps_saved_work_and_logs(self):
        work = Path(self.run_action()['workspace'])
        (work/'project/world.studio').write_text('keep after failure')
        child = mock.Mock(pid=234, wait=mock.Mock(return_value=7))
        with mock.patch.object(actions.subprocess, 'Popen', return_value=child), mock.patch.object(actions, 'process_identity', return_value=None):
            with self.assertRaisesRegex(ValueError, 'code 7'):
                with contextlib.redirect_stdout(io.StringIO()): self.designer.open_editor(work, 'city')
        self.assertEqual((work/'project/world.studio').read_text(), 'keep after failure')
        self.assertTrue((work/'logs/editor-latest.log').is_file())


if __name__ == '__main__': unittest.main()

"""CPU checks of the directional controller evidence gate and private copies."""
import copy
import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]/'tools'
sys.path.insert(0, str(TOOLS))
spec = importlib.util.spec_from_file_location('check_sword_stick', TOOLS/'check_sword_stick.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


def valid_world():
    return dict(sword_stick=dict(qa=True, passed=True, checks=43, events=128, captures=15,
                    check_labels=sorted(runner.REQUIRED_CHECKS), capture_names=sorted(runner.CAPTURES),
                    combat_state_mutations=0, player_pose_writes=0, story_state_writes=0),
                playtest_combat=dict(ready=True, starts=7, hero_draw_frames=500, enemy_visible_frames=500),
                combat_motion=dict(actors=2, owned_pose_assets=12, uploads=30),
                gl_error=0, remaining_texture_references=0, asset_streaming=dict(failed=0),
                playtest_slice=dict(area='city'))


def blade_state():
    return dict(available=True, base=[0., 1.4, 0.], tip=[.5, 1.8, .4])


class EvidenceGate(unittest.TestCase):
    def test_complete_observed_workflow_passes(self):
        self.assertEqual(runner.qa_failures(valid_world()), [])

    def test_success_flag_cannot_substitute_for_scenarios(self):
        self.assertTrue(runner.qa_failures({'sword_stick': {'qa': True, 'passed': True}}))

    def test_every_required_behavior_is_enforced(self):
        for label in runner.REQUIRED_CHECKS:
            with self.subTest(label=label):
                world = valid_world()
                world['sword_stick']['check_labels'].remove(label)
                self.assertTrue(runner.qa_failures(world))

    def test_all_rendered_angles_are_required(self):
        for name in runner.CAPTURES:
            with self.subTest(name=name):
                world = valid_world()
                world['sword_stick']['capture_names'].remove(name)
                self.assertTrue(runner.qa_failures(world))

    def test_gameplay_bypasses_are_rejected(self):
        for field in ('combat_state_mutations', 'player_pose_writes', 'story_state_writes'):
            with self.subTest(field=field):
                world = valid_world()
                world['sword_stick'][field] = 1
                self.assertTrue(runner.qa_failures(world))

    def test_missing_rendered_hero_or_opponent_fails(self):
        for field in ('hero_draw_frames', 'enemy_visible_frames'):
            world = valid_world()
            world['playtest_combat'][field] = 0
            self.assertTrue(runner.qa_failures(world))

    def test_guard_image_must_match_actual_rendered_state(self):
        for mode in ('first', 'third'):
            for direction, expected in {'center': 4, 'left': 0, 'right': 1, 'up': 2, 'down': 3}.items():
                name = mode+'-guard-'+direction
                state = dict(name=name, frame=220, selected=expected, attack_direction=2,
                             guard=True, player_phase=0, third_person=mode == 'third',
                             renderer_pose=dict(active=True, slot=75+4*expected, guard_direction=expected, guard_weight=1., blend=0.),
                             world_blade=blade_state(), viewmodel_blade=blade_state())
                self.assertEqual(runner.capture_state_failures(name, state), [])
                for field, value in [('selected', (expected+1) % 5), ('guard', False),
                                     ('third_person', mode != 'third'), ('player_phase', 1)]:
                    with self.subTest(name=name, field=field):
                        changed = copy.deepcopy(state); changed[field] = value
                        self.assertTrue(runner.capture_state_failures(name, changed))
                for field, value in [('slot', 75+4*((expected+1) % 5)), ('guard_direction', (expected+1) % 5),
                                     ('guard_weight', .5), ('active', False), ('blend', float('nan'))]:
                    with self.subTest(name=name, renderer_field=field):
                        changed = copy.deepcopy(state); changed['renderer_pose'][field] = value
                        self.assertTrue(runner.capture_state_failures(name, changed))

    def test_attack_image_must_show_matching_strike_and_camera(self):
        for name in sorted(runner.CAPTURES):
            if '-attack-' not in name: continue
            direction = name.rsplit('-', 1)[-1]
            expected = {'left': 0, 'right': 1, 'up': 3, 'down': 2}[direction]
            state = dict(name=name, frame=380, selected=expected, attack_direction=expected,
                         guard=False, player_phase=2, player_time=.055, third_person=name.startswith('third-'),
                         world_blade=blade_state(), viewmodel_blade=blade_state())
            self.assertEqual(runner.capture_state_failures(name, state), [])
            for field, value in [('selected', (expected+1) % 5), ('attack_direction', (expected+1) % 5),
                                 ('guard', True), ('third_person', not state['third_person']),
                                 ('player_phase', 1), ('player_time', float('nan'))]:
                with self.subTest(name=name, field=field):
                    changed = copy.deepcopy(state); changed[field] = value
                    self.assertTrue(runner.capture_state_failures(name, changed))

    def test_selected_left_with_rendered_overhead_cannot_pass(self):
        name = 'first-guard-left'
        state = dict(name=name, frame=500, selected=0, attack_direction=2, guard=True,
                     player_phase=0, third_person=False, world_blade=blade_state(), viewmodel_blade=blade_state(),
                     renderer_pose=dict(active=True, slot=83, guard_direction=2, guard_weight=1., blend=0.))
        self.assertIn('renderer never reached labeled guard pose: '+name, runner.capture_state_failures(name, state))

    def test_candidate_bank_overlay_preserves_both_sources(self):
        with tempfile.TemporaryDirectory(prefix='sword-stick-bank-cpu-') as directory:
            root = Path(directory); original = root/'source-models'; candidate = root/'candidate'
            project = root/'private'; original.mkdir(); candidate.mkdir(); project.mkdir()
            (original/'another-model.glb').write_bytes(b'original other model')
            (project/'models').symlink_to(original, target_is_directory=True)
            for kind in ('body', 'arms'):
                for index in range(108):
                    (candidate/f'combat_{kind}_{index:03d}.glb').write_bytes(b'candidate immutable model '+bytes([index]))
            before = runner.motion_inventory(candidate)
            private, inputs = runner.clone_motion_bank(project, candidate)
            self.assertEqual(inputs, before)
            self.assertFalse((project/'models').is_symlink())
            self.assertFalse(private.is_symlink())
            self.assertEqual((project/'models/another-model.glb').resolve(), original/'another-model.glb')
            self.assertEqual(runner.motion_inventory(private), before)
            self.assertEqual(runner.motion_inventory(candidate), before)
            self.assertEqual(list(original.iterdir()), [original/'another-model.glb'])

    def test_source_saves_and_graphics_are_not_reused(self):
        with tempfile.TemporaryDirectory(prefix='sword-stick-runner-cpu-') as directory:
            root = Path(directory); source = root/'source'; target = root/'isolated'
            source.mkdir()
            for name in ('project.valwood', 'world.studio', 'graphics.settings'):
                (source/name).write_text('original '+name+'\n')
            (source/'playtest.route').write_text('VALWOOD_PLAYTEST_ROUTE 1\n')
            (source/'models').mkdir(); (source/'models/hero.glb').write_bytes(b'original bank')
            (source/'saves').mkdir(); (source/'saves/playtest.progress').write_text('precious user progress')
            before = runner.mutable_snapshot(source)
            inputs = runner.isolate(source, target)
            self.assertEqual(list((target/'saves').iterdir()), [])
            self.assertEqual((target/'models').resolve(), source/'models')
            self.assertFalse((target/'graphics.settings').is_symlink())
            (target/'graphics.settings').write_text('private QA change')
            self.assertEqual(runner.mutable_snapshot(source), before)
            self.assertIn('playtest.route', inputs)


if __name__ == '__main__':
    unittest.main()

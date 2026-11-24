"""CPU checks for opening QA isolation and evidence validation."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
from PIL import Image
from check_opening_prologue import ARRIVAL_CAMP_CHECKS, film_evidence, GAME_START_CAPTURES, GAME_START_CHECKS, REQUIRED_CHECKS, ROOM_LIGHT_CHECKS, home_cinematic, game_start_story, isolate_project, mutable_snapshot, qa_failures, write_qa_paths


class OpeningQATest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='valwood-opening-qa-fixture-')
        self.root = Path(self.temp.name)
        self.project = self.root/'source'
        self.project.mkdir()
        for name in ('project.valwood', 'graphics.settings', 'world.studio', 'opening.scene', 'opening.config'):
            (self.project/name).write_text('opening fixture\n')
        for name in ('models', 'audio', 'ai', 'shaders', 'saves', 'opening'):
            (self.project/name).mkdir()
            (self.project/name/'fixture.txt').write_text(name+' fixture\n')
        (self.project/'opening/home.physics').write_text('bounded support fixture\n')
        self.target = self.root/'qa/project'

    def tearDown(self):
        self.temp.cleanup()

    def test_isolated_copy_does_not_touch_source_state_or_copy_user_save(self):
        before = mutable_snapshot(self.project)
        inputs = isolate_project(self.project, self.target)
        self.assertIn('opening.config', inputs)
        self.assertIn('opening/home.physics', inputs)
        for name in ('graphics.settings', 'world.studio', 'ai/fixture.txt', 'shaders/fixture.txt', 'opening/home.physics'):
            path = self.target/name
            self.assertFalse(path.is_symlink())
            path.write_text('QA private mutation\n')
        self.assertTrue((self.target/'models').is_symlink())
        self.assertTrue((self.target/'audio').is_symlink())
        self.assertEqual(list((self.target/'saves').iterdir()), [])
        self.assertEqual(before, mutable_snapshot(self.project))

    def test_linked_source_and_support_reject_without_launch(self):
        linked = self.root/'linked'
        linked.symlink_to(self.project, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'ordinary'):
            isolate_project(linked, self.target)
        (self.project/'opening/home.physics').unlink()
        (self.project/'opening/home.physics').symlink_to(self.project/'world.studio')
        with self.assertRaisesRegex(ValueError, 'cannot be links'):
            isolate_project(self.project, self.target)

    def test_game_start_marker_is_explicit_validated_and_privately_copied(self):
        self.assertFalse(game_start_story(self.project))
        marker = self.project/'opening/game-start.settings'
        marker.write_bytes(b'VALWOOD_GAME_START 1\n')
        self.assertTrue(game_start_story(self.project))
        inputs = isolate_project(self.project, self.target)
        self.assertIn('opening/game-start.settings', inputs)
        copied = self.target/'opening/game-start.settings'
        self.assertFalse(copied.is_symlink())
        self.assertEqual(copied.read_bytes(), marker.read_bytes())
        copied.write_bytes(b'private QA change\n')
        self.assertEqual(marker.read_bytes(), b'VALWOOD_GAME_START 1\n')

    def test_invalid_or_linked_game_start_marker_cannot_silently_run_legacy_story(self):
        marker = self.project/'opening/game-start.settings'
        for content in (b'', b'VALWOOD_GAME_START 2\n', b'VALWOOD_GAME_START 1\nextra\n', b'VALWOOD_GAME_START 1\r\n'):
            with self.subTest(content=content):
                marker.write_bytes(content)
                with self.assertRaisesRegex(ValueError, 'Invalid game-start marker'):
                    isolate_project(self.project, self.target)
                self.assertFalse(self.target.exists())
        marker.unlink()
        marker.symlink_to(self.project/'world.studio')
        with self.assertRaisesRegex(ValueError, 'ordinary file'):
            game_start_story(self.project)
        with self.assertRaisesRegex(ValueError, 'ordinary file'):
            isolate_project(self.project, self.target)

    def test_nested_or_existing_target_rejects(self):
        with self.assertRaisesRegex(ValueError, 'outside'):
            isolate_project(self.project, self.project/'qa')
        self.target.mkdir(parents=True)
        with self.assertRaisesRegex(ValueError, 'already exist'):
            isolate_project(self.project, self.target)

    def world(self):
        return {'opening_prologue_qa': {'enabled': True, 'passed': True, 'checks': sorted(REQUIRED_CHECKS),
            'events': 20, 'movement_frames': 60, 'walked_m': 20., 'controller_look_radians': .1,
            'neutral_handoff_playable_frames': 6,
            'qa_player_pose_writes': 0, 'qa_story_state_writes': 0, 'checkpoint_restores': 1,
            'checkpoint_retries': 1, 'capture_failures': 0, 'captures': 5},
            'opening_prologue': {'enabled': True, 'stage': 'complete', 'first_person': True},
            'gl_error': 0, 'remaining_texture_references': 0, 'asset_streaming': {'failed': 0}}

    def test_camp_pass_requires_all_three_people_and_real_input_evidence(self):
        world = self.world()
        self.assertTrue(qa_failures(world, arrival_camp=True))
        world['opening_prologue_qa']['checks'] += sorted(ARRIVAL_CAMP_CHECKS)
        world['opening_prologue_qa']['captures'] = 10
        world['opening_arrival_camp'] = dict(enabled=True, spoken_mask=7, completed=3, dialogue_active=False)
        world['opening_arrival_qa'] = dict(enabled=True, passed=True, phase=10, actors_completed=3,
                                          qa_player_pose_writes=0, qa_story_state_writes=0)
        world['arrival_camp_render'] = dict(active=True, actors=3, pose_assets=12, failures=0, unsafe_roots=0)
        world['opening_arrival_portraits'] = dict(facing_errors_radians=[.01,.02,.03], maximum_allowed_radians=.06, qa_player_pose_writes=0)
        self.assertEqual(qa_failures(world, arrival_camp=True), [])
        for section, key, value in (('opening_arrival_camp', 'spoken_mask', 3),
                                    ('opening_arrival_qa', 'qa_story_state_writes', 1),
                                    ('arrival_camp_render', 'actors', 2)):
            old = world[section][key]
            world[section][key] = value
            self.assertTrue(qa_failures(world, arrival_camp=True), section)
            world[section][key] = old
        world['opening_prologue_qa']['checks'].remove('arrival_checkpoint_restore')
        self.assertIn('missing opening control/story checks', qa_failures(world, arrival_camp=True))
        world['opening_arrival_portraits']['facing_errors_radians'][2] = -1
        self.assertIn('all three speaking travelers did not face the player in their portraits', qa_failures(world, arrival_camp=True))

    def test_arrival_landscape_cannot_pass_with_obsolete_floating_water(self):
        world = self.world()
        self.assertTrue(qa_failures(world, arrival_redesign=True))
        world['castle_creek'] = dict(legacy_surface_enabled=False, source_scene_owns_water=True, draw_frames=0, reflection_frames=0)
        self.assertEqual(qa_failures(world, arrival_redesign=True), [])
        world['castle_creek']['draw_frames'] = 1
        self.assertIn('obsolete castle creek rendered over the authored arrival landscape', qa_failures(world, arrival_redesign=True))

    def test_pass_requires_specific_real_journey_evidence(self):
        world = self.world()
        self.assertEqual(qa_failures(world), [])
        world['opening_prologue_qa']['checks'].remove('transport_control_lock')
        self.assertIn('missing opening control/story checks', qa_failures(world))
        world = self.world()
        world['opening_prologue_qa']['qa_player_pose_writes'] = 1
        self.assertIn('QA bypassed ordinary gameplay', qa_failures(world))
        world = self.world()
        world['opening_prologue_qa']['walked_m'] = 0
        self.assertIn('insufficient walked_m', qa_failures(world))

    def test_empty_report_and_wrong_stage_cannot_pass(self):
        self.assertTrue(qa_failures({}))
        world = self.world()
        world['opening_prologue']['stage'] = 'house_explore'
        self.assertIn('opening story did not reach its waystone objective', qa_failures(world))
        world = self.world()
        world['opening_prologue']['first_person'] = False
        self.assertIn('ordinary father presentation was replaced by prototype hero', qa_failures(world))

    def test_neutral_handoff_requires_six_actually_playable_frames(self):
        world = self.world()
        world['opening_prologue_qa']['neutral_handoff_playable_frames'] = 5
        self.assertIn('insufficient neutral_handoff_playable_frames', qa_failures(world))
        world = self.world()
        world['opening_prologue_qa']['checks'].remove('neutral_handoff')
        self.assertIn('missing opening control/story checks', qa_failures(world))

    def test_both_keyboard_and_controller_menu_confirmation_are_required(self):
        for label in ('menu_keyboard_confirm', 'controller_pause_menu'):
            world = self.world()
            world['opening_prologue_qa']['checks'].remove(label)
            self.assertIn('missing opening control/story checks', qa_failures(world))

    def test_game_start_story_requires_bills_console_and_fresh_menu_actions(self):
        world = self.world()
        self.assertIn('missing opening control/story checks', qa_failures(world, game_start=True))
        self.assertIn('game-start story mode not exercised', qa_failures(world, game_start=True))
        world['opening_prologue_qa'].update(checks=sorted(GAME_START_CHECKS), game_start_story=True,
                                           captures=len(GAME_START_CAPTURES))
        world['opening_game_start'] = dict(enabled=True, menus=2, new_games=1, screen_mode=0)
        self.assertEqual(qa_failures(world, game_start=True), [])
        # Legacy door/notebook checks cannot substitute for this story, and
        # opening a menu alone cannot prove held-input protection or cancel.
        for label in ('bills_interaction', 'console_walk', 'game_menu_open', 'game_menu_held_confirm',
                      'game_menu_control_lock', 'game_menu_cancel', 'game_menu_reopen', 'game_menu_fresh_start'):
            with self.subTest(label=label):
                world['opening_prologue_qa']['checks'].remove(label)
                self.assertIn('missing opening control/story checks', qa_failures(world, game_start=True))
                world['opening_prologue_qa']['checks'].append(label)
        world['opening_prologue_qa']['captures'] -= 1
        self.assertIn('opening capture failure', qa_failures(world, game_start=True))

    def test_game_start_requires_actual_menu_transport_and_tv_runtime_evidence(self):
        world = self.world()
        world['opening_prologue_qa'].update(checks=sorted(GAME_START_CHECKS), game_start_story=True,
                                           captures=len(GAME_START_CAPTURES))
        self.assertIn('console menu runtime events missing', qa_failures(world, game_start=True))
        for field, bad in (('enabled', False), ('menus', 1), ('menus', '2'), ('new_games', 0),
                           ('new_games', 2), ('screen_mode', 1), ('screen_mode', 2)):
            with self.subTest(field=field, bad=bad):
                world['opening_game_start'] = dict(enabled=True, menus=2, new_games=1, screen_mode=0)
                world['opening_game_start'][field] = bad
                self.assertTrue(qa_failures(world, game_start=True))
        world['opening_game_start'] = dict(enabled=True, menus=2, new_games=1, screen_mode=0)
        self.assertEqual(qa_failures(world, game_start=True), [])

    def test_game_start_requires_the_late_transport_capture_beyond_earlier_stage_images(self):
        world = self.world()
        world['opening_prologue_qa'].update(checks=sorted(GAME_START_CHECKS), game_start_story=True, captures=6)
        world['opening_game_start'] = dict(enabled=True, menus=2, new_games=1, screen_mode=0)
        self.assertIn('transport-glow', GAME_START_CAPTURES)
        self.assertIn('opening capture failure', qa_failures(world, game_start=True))
        world['opening_prologue_qa']['captures'] = 7
        self.assertEqual(qa_failures(world, game_start=True), [])

    def test_route_input_uses_authored_points_and_rejects_nonfinite_data(self):
        home = [[index, .18, -index] for index in range(5)]
        meadow = [[100., 2., 100.], [105., 2.5, 106.]]
        scene = {'home': {'walk_path': home}, 'meadow': {'walk_path': meadow}}
        (self.project/'opening.scene').write_text(json.dumps(scene))
        digest = write_qa_paths(self.project)
        self.assertEqual(len(digest), 64)
        path = (self.project/'opening/qa.walk').read_text()
        self.assertIn('home 5\n', path)
        self.assertIn('meadow 2\n100 2 100\n105 2.5 106\nend\n', path)
        scene['meadow']['walk_path'][1][0] = float('nan')
        (self.project/'opening.scene').write_text(json.dumps(scene))
        with self.assertRaisesRegex(ValueError, 'invalid coordinate'):
            write_qa_paths(self.project)

    def test_room_lights_require_interaction_evidence_and_real_practical_powers(self):
        world = self.world()
        self.assertIn('missing room-light SDL interaction/save/pause checks', qa_failures(world, room_lights=True))
        world['opening_prologue_qa']['checks'] += sorted(ROOM_LIGHT_CHECKS)
        world['opening_room_lights'] = dict(enabled=True, mask=15, bindings=6, toggles=[2, 2, 2, 2], powers=[1.35, .55, .9, .9, .65, .85])
        self.assertEqual(qa_failures(world, room_lights=True), [])
        world['opening_room_lights']['powers'][3] = 0
        self.assertIn('room practical powers were not restored', qa_failures(world, room_lights=True))
        world['opening_room_lights']['powers'][3] = .9
        world['opening_room_lights']['toggles'][2] = 3
        self.assertIn('each room must switch off and on exactly once', qa_failures(world, room_lights=True))

    def test_natural_entry_requires_all_shots_real_duration_and_exact_handoff(self):
        cinematic = dict(shot_count=4, duration=16.)
        world = self.world()
        self.assertIn('natural apartment entry did not capture every authored shot', qa_failures(world, cinematic=cinematic))
        world['opening_prologue_qa']['checks'] = sorted((REQUIRED_CHECKS-{'house_skip'}) | {
            'house_cinematic_natural', 'house_cinematic_all_shots', 'house_cinematic_match_handoff'})
        world['opening_prologue_qa']['captures'] = 9
        world['opening_entry_qa'] = dict(enabled=True, shot_count=4, shot_mask=15,
            cinematic_frames=955, observed_seconds=15.983333333, duration_seconds=16.,
            matched_handoff=True, handoff_max_error=.00000004)
        world['opening_cinematic_entry'] = dict(enabled=True, shots=4, duration=16.,
            audio_available=True, cue_mask=3, cue_starts=[1, 1, 1, 2])
        self.assertEqual(qa_failures(world, cinematic=cinematic), [])
        for field, value in [('shot_mask', 7), ('cinematic_frames', 100), ('observed_seconds', 2.),
                             ('duration_seconds', 6.5), ('matched_handoff', False),
                             ('handoff_max_error', .01), ('observed_seconds', float('nan'))]:
            with self.subTest(field=field, value=value):
                before = world['opening_entry_qa'][field]
                world['opening_entry_qa'][field] = value
                self.assertTrue(qa_failures(world, cinematic=cinematic))
                world['opening_entry_qa'][field] = before
        world['opening_prologue_qa']['captures'] = 8
        self.assertIn('opening capture failure', qa_failures(world, cinematic=cinematic))
        world['opening_prologue_qa']['captures'] = 9
        for field, value in [('enabled', False), ('shots', 1), ('duration', 6.5),
                             ('audio_available', False), ('cue_mask', 1),
                             ('cue_starts', [1, 0, 1, 2]), ('cue_starts', [1, 1, 0, 2]),
                             ('cue_starts', [1, 1, 1, 1]), ('cue_starts', [])]:
            with self.subTest(audio_field=field, value=value):
                before = world['opening_cinematic_entry'][field]
                world['opening_cinematic_entry'][field] = value
                self.assertTrue(qa_failures(world, cinematic=cinematic))
                world['opening_cinematic_entry'][field] = before

    def test_cinematic_settings_are_bounded_explicit_and_copied_privately(self):
        marker = self.project/'opening/cinematic.settings'
        self.assertIsNone(home_cinematic(self.project))
        rail = '0 2 0 0 2 1 0 2 -1 0 2 0 .9 .9'
        text = 'VALWOOD_HOME_CINEMATIC 1\nshot 3.2 '+rail+' "Another late shift."\nend\n'
        marker.write_text(text)
        self.assertEqual(home_cinematic(self.project), dict(shot_count=1, duration=3.2))
        inputs = isolate_project(self.project, self.target)
        self.assertIn('opening/cinematic.settings', inputs)
        self.assertFalse((self.target/'opening/cinematic.settings').is_symlink())
        self.assertEqual((self.target/'opening/cinematic.settings').read_text(), text)
        for invalid in [text.replace('shot 3.2', 'shot nan'), text.replace('shot 3.2', 'shot 20'),
                        text.replace('.9 .9', '.1 .9'), text.replace('end\n', 'end\nextra\n'),
                        text.replace('CINEMATIC 1', 'CINEMATIC 2')]:
            marker.write_text(invalid)
            with self.assertRaisesRegex(ValueError, 'Invalid'):
                home_cinematic(self.project)
        marker.unlink()
        marker.symlink_to(self.project/'world.studio')
        with self.assertRaisesRegex(ValueError, 'ordinary file'):
            home_cinematic(self.project)

    def test_silent_native_film_requires_full_ordered_frames_and_clock_coverage(self):
        raw = self.root/'film/raw'
        raw.mkdir(parents=True)
        source = self.root/'one-native-frame.bmp'
        Image.new('RGB', (1600, 900), (12, 16, 20)).save(source)
        records = []
        for index in range(15):
            filename = f'entry-{index:04d}.bmp'
            (raw/filename).hardlink_to(source)
            records.append(dict(file=filename, output_frame=index, rendered_frame=index*4,
                story_seconds=1/60+index/15, wall_seconds=index*.08, shot_index=0,
                shot_progress=1/60+index/15, width=1600, height=900))
        manifest = raw/'manifest.jsonl'
        def write():
            manifest.write_text(''.join(json.dumps(record)+'\n' for record in records))
        write()
        cinematic = dict(shot_count=1, duration=1.)
        entry = dict(film_enabled=True, film_frames=15, film_rendered_frames=60)
        report = film_evidence(raw, cinematic, entry)
        self.assertTrue(report['passed'], report)
        self.assertTrue(report['silent'])
        self.assertEqual(len(report['frame_hashes']), 15)
        self.assertIn('not a gameplay performance benchmark', report['scope'])
        records[3]['rendered_frame'] = 13
        write()
        self.assertFalse(film_evidence(raw, cinematic, entry)['passed'])
        records[3]['rendered_frame'] = 12
        records[-1]['story_seconds'] = .80
        write()
        self.assertFalse(film_evidence(raw, cinematic, entry)['passed'])
        records[-1]['story_seconds'] = 1/60+14/15
        write()
        entry['film_frames'] = 14
        self.assertFalse(film_evidence(raw, cinematic, entry)['passed'])
        entry['film_frames'] = 15
        (raw/'entry-0000.bmp').unlink()
        self.assertFalse(film_evidence(raw, cinematic, entry)['passed'])


if __name__ == '__main__':
    unittest.main()

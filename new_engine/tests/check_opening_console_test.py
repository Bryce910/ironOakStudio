import importlib.util
import math
from pathlib import Path
import sys
import unittest

TOOLS = Path(__file__).resolve().parents[1]/'tools'
sys.path.insert(0, str(TOOLS))
spec = importlib.util.spec_from_file_location('check_opening_console', TOOLS/'check_opening_console.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
CINEMATIC = dict(shot_count=4, duration=16)


def world():
    return dict(opening_prologue_qa=dict(enabled=True, passed=True, game_start_story=True, phase=15,
        checks=sorted(runner.CHECKS), qa_player_pose_writes=0, qa_story_state_writes=0,
        capture_failures=0, movement_frames=100, walked_m=12, neutral_handoff_playable_frames=6,
        checkpoint_restores=1, captures=len(runner.expected_captures(CINEMATIC))),
        opening_prologue=dict(stage='transport', arrivals=0, skips=0),
        opening_game_start=dict(new_games=1, menus=2),
        opening_entry_qa=dict(enabled=True, shot_count=4, shot_mask=15, matched_handoff=True, handoff_max_error=0),
        gl_error=0, remaining_texture_references=0, asset_streaming=dict(failed=0))


class ConsoleAcceptance(unittest.TestCase):
    def test_real_apartment_subset_passes(self):
        self.assertEqual(runner.qa_failures(world(), CINEMATIC), [])

    def test_full_journey_success_cannot_substitute_for_console_only(self):
        data = world()
        data['opening_prologue_qa']['phase'] = 21
        data['opening_prologue']['stage'] = 'complete'
        data['opening_prologue']['arrivals'] = 1
        self.assertTrue(runner.qa_failures(data, CINEMATIC))

    def test_each_real_menu_control_check_required(self):
        for label in runner.CHECKS:
            data = world()
            data['opening_prologue_qa']['checks'].remove(label)
            self.assertTrue(runner.qa_failures(data, CINEMATIC), label)

    def test_duplicate_and_malformed_checks_rejected(self):
        for labels in [None, [{}], sorted(runner.CHECKS)+['console_only_complete']]:
            data = world()
            data['opening_prologue_qa']['checks'] = labels
            self.assertTrue(runner.qa_failures(data, CINEMATIC))

    def test_success_flag_cannot_replace_actual_evidence(self):
        self.assertTrue(runner.qa_failures({'opening_prologue_qa': {'passed': True}}, CINEMATIC))

    def test_pose_story_and_capture_bypasses_rejected(self):
        for key in ['qa_player_pose_writes', 'qa_story_state_writes', 'capture_failures']:
            data = world()
            data['opening_prologue_qa'][key] = 1
            self.assertTrue(runner.qa_failures(data, CINEMATIC), key)

    def test_walk_restore_and_neutral_control_evidence_required(self):
        for key in ['movement_frames', 'walked_m', 'neutral_handoff_playable_frames', 'checkpoint_restores']:
            for invalid in [0, math.nan, True, None]:
                data = world()
                data['opening_prologue_qa'][key] = invalid
                self.assertTrue(runner.qa_failures(data, CINEMATIC), (key, invalid))

    def test_no_skip_or_outdoor_relocation_accepted(self):
        for key, value in [('arrivals', 1), ('skips', 1), ('stage', 'meadow_reveal')]:
            data = world()
            data['opening_prologue'][key] = value
            self.assertTrue(runner.qa_failures(data, CINEMATIC))

    def test_genuine_new_game_and_cancel_reopen_are_required(self):
        for key, value in [('new_games', 0), ('new_games', 2), ('menus', 1)]:
            data = world()
            data['opening_game_start'][key] = value
            self.assertTrue(runner.qa_failures(data, CINEMATIC))

    def test_natural_shots_and_camera_match_required(self):
        for key, value in [('enabled', False), ('shot_mask', 7), ('matched_handoff', False),
                           ('handoff_max_error', math.nan), ('handoff_max_error', .01)]:
            data = world()
            data['opening_entry_qa'][key] = value
            self.assertTrue(runner.qa_failures(data, CINEMATIC))

    def test_capture_contract_contains_tv_objective_and_no_outdoor(self):
        names = runner.expected_captures(CINEMATIC)
        self.assertTrue({'console-objective', 'house-bills', 'game-menu', 'transport', 'transport-glow', 'house-handoff'}.issubset(names))
        self.assertFalse({'meadow-reveal', 'waystone-complete', 'house-establish'} & names)
        data = world()
        data['opening_prologue_qa']['captures'] -= 1
        self.assertTrue(runner.qa_failures(data, CINEMATIC))

    def test_render_failures_rejected(self):
        for key in ['gl_error', 'remaining_texture_references']:
            data = world()
            data[key] = 1
            self.assertTrue(runner.qa_failures(data, CINEMATIC))
        data = world()
        data['asset_streaming']['failed'] = 1
        self.assertTrue(runner.qa_failures(data, CINEMATIC))


if __name__ == '__main__':
    unittest.main()

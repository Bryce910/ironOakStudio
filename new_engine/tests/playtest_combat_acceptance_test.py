#!/usr/bin/env python3
"""Protect combat certification from reporting the former traversal-only state."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from check_playtest_slices import combat_failures


def complete_evidence():
    return {
        'playtest_combat_qa':{'enabled':True,'passed':True,'cases':10,'events':84,'continue_exploring_ui':True,
            'loss_enemy_hits':6,'win_hits':5,'win_parries':2,'win_swings':5,'win_health':82,
            'combat_state_mutations':0},
        'playtest_combat':{'ready':True,'starts':3,'wins':1,'defeats':1,
            'hero_draw_frames':1200,'enemy_visible_frames':1200},
        'combat_motion':{'actors':2,'owned_pose_assets':128,'uploads':1000},
        'combat_vfx':{'hits':5,'parries':2,'incoming':6,'trail_segments':500,'draws':1200,'failures':0}}


class CombatAcceptance(unittest.TestCase):
    def test_complete_evidence_passes(self):
        self.assertEqual(combat_failures(complete_evidence()),[])

    def test_success_flag_cannot_certify_traversal_only_slice(self):
        self.assertTrue(combat_failures({'playtest_combat_qa':{'enabled':True,'passed':True}}))

    def test_missing_hero_or_opponent_fails(self):
        for actor_count in (0,1):
            world=complete_evidence();world['combat_motion']['actors']=actor_count
            self.assertIn('hero/opponent rig presentation',combat_failures(world))

    def test_loaded_model_without_actual_draw_fails(self):
        for field in ('hero_draw_frames','enemy_visible_frames'):
            world=complete_evidence();world['playtest_combat'][field]=0
            self.assertIn('combat '+field,combat_failures(world))

    def test_loss_win_and_retry_need_independent_runtime_counters(self):
        for field in ('starts','wins','defeats'):
            world=complete_evidence();world['playtest_combat'][field]=0
            self.assertIn('combat '+field,combat_failures(world))

    def test_actual_hit_parry_and_player_damage_are_required(self):
        for field in ('loss_enemy_hits','win_hits','win_parries'):
            world=complete_evidence();world['playtest_combat_qa'][field]=0
            self.assertIn('combat QA '+field,combat_failures(world))

    def test_hp_mutation_cannot_certify_the_input_policy(self):
        world=complete_evidence();world['playtest_combat_qa']['combat_state_mutations']=1
        self.assertIn('combat QA state mutation',combat_failures(world))

    def test_result_ui_must_resume_exploration_through_real_input(self):
        world=complete_evidence();world['playtest_combat_qa']['continue_exploring_ui']=False
        self.assertIn('combat result UI Continue exploring',combat_failures(world))

    def test_successful_simulation_without_combat_vfx_fails(self):
        for field in ('hits','parries','incoming','trail_segments','draws'):
            world=deepcopy(complete_evidence());world['combat_vfx'].pop(field)
            self.assertIn('combat VFX '+field,combat_failures(world))

    def test_failed_renderer_or_unavailable_lane_fails(self):
        world=complete_evidence();world['combat_vfx']['failures']=1
        self.assertIn('combat VFX failures',combat_failures(world))
        world=complete_evidence();world['playtest_combat']['ready']=False
        self.assertIn('combat lane unavailable',combat_failures(world))


if __name__=='__main__':unittest.main()
